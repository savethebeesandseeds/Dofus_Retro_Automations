/* dproc_fn.hpp
* ──────────────────────────────────────────────────────────────────────────*/
#pragma once
#include "dconfig.hpp"
#include "dlog.hpp"
#include "dutils.hpp"       // du::trim / trim_quotes / simplify
#include "dwin_api.hpp"     // dw::* helpers
#include "dscreen_ocr.hpp"  // so::read_region / locate_text
#include <opencv2/opencv.hpp>
#include <optional>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <vector>
#include <cmath>  // for std::abs
#include <algorithm>
#include <thread>

namespace dp_fn {

/// Fast centre-finder for the orange band.
/// Down-scales the ROI, does the same HSV / morphology logic,
/// then rescales the centroid to original coords.
inline std::optional<cv::Point>
find_orange_box_center(const cv::Mat& region)
{
    CV_Assert(region.type() == CV_8UC3 || region.type() == CV_8UC4);

    /* 0. Build a small thumbnail ------------------------------------------------ */
    constexpr double SCALE = 0.25;                // 25 %  (tweak if needed)
    cv::Mat small;
    cv::resize(region, small, cv::Size(), SCALE, SCALE, cv::INTER_AREA);

    /* 1.  Ensure 3-channel BGR --------------------------------------------------- */
    if (small.channels() == 4)
        cv::cvtColor(small, small, cv::COLOR_BGRA2BGR);

    /* 2.  HSV thresholding (same numbers) --------------------------------------- */
    cv::Mat hsv;
    cv::cvtColor(small, hsv, cv::COLOR_BGR2HSV_FULL);

    const cv::Scalar lo(10, 100,   0);
    const cv::Scalar hi(40, 255, 255);

    cv::Mat mask;
    cv::inRange(hsv, lo, hi, mask);

    /* morphological clean-up (kernels scaled with the thumbnail) */
    auto kOpen  = cv::getStructuringElement(
                      cv::MORPH_RECT, {std::max(3, int(5  * SCALE)),
                                       std::max(2, int(3  * SCALE))});
    auto kClose = cv::getStructuringElement(
                      cv::MORPH_RECT, {std::max(5, int(25 * SCALE)),
                                       std::max(3, int(5  * SCALE))});

    cv::morphologyEx(mask, mask, cv::MORPH_OPEN,  kOpen);
    cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, kClose);

    /* 3.  Largest contour → moments → centre ------------------------------------ */
    std::vector<std::vector<cv::Point>> cc;
    cv::findContours(mask, cc, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    if (cc.empty()) return std::nullopt;

    auto big = std::max_element(cc.begin(), cc.end(),
                [](auto& a, auto& b){ return cv::contourArea(a) < cv::contourArea(b); });

    cv::Moments m = cv::moments(*big);
    if (m.m00 == 0) return std::nullopt;

    cv::Point centre_small{ int(m.m10 / m.m00), int(m.m01 / m.m00) };

    /* 4.  Map back to full resolution ------------------------------------------- */
    return cv::Point{ int(std::round(centre_small.x / SCALE)),
                      int(std::round(centre_small.y / SCALE)) };
}

/* Prototype every intrinsic here */
/*────────────────── click_next_item_in_line ──────────────────*/
/* args:
 * 0 finder_left   1 finder_top   2 finder_width  3 finder_height
 * 4 offset_x      5 offset_y
 */
bool click_next_item_in_line(Context& ctx,
                             const std::vector<std::string>& args)
{
    if (args.size() < 6) {
        LOG_ERROR("click_next_item_in_line: need 6 args, got %zu\n", args.size());
        return false;
    }

    const int finder_left   = std::stoi(args[0]);
    const int finder_top    = std::stoi(args[1]);
    const int finder_width  = std::stoi(args[2]);
    const int finder_height = std::stoi(args[3]);
    const int offset_x      = std::stoi(args[4]);
    const int offset_y      = std::stoi(args[5]);

    RECT finder_rc{ finder_left,
                    finder_top,
                    finder_left + finder_width,
                    finder_top + finder_height };

    LOG_EVENT("[call_fn] click_next_item_in_line finder (%d,%d,%d,%d) "
              "offset (%d,%d)\n",
              finder_left, finder_top, finder_width, finder_height,
              offset_x, offset_y);

    /* grab the sub-image that contains the orange bar */
    cv::Mat full  = so::detail::capture(ctx.hwnd);
    cv::Mat sub   = full(cv::Rect(finder_rc.left,  finder_rc.top,
                                  finder_rc.right - finder_rc.left,
                                  finder_rc.bottom - finder_rc.top));
    auto centre = find_orange_box_center(sub);
    int x_corretion = 0;
    int y_corretion = 0;

    if (!centre) {
        LOG_DEBUG("[call_fn] click_next_item_in_line → orange bar not found. Clicking first element. \n");
        centre = {0, 0};
        x_corretion = static_cast<int>(static_cast<double>(finder_width) / 2);
        y_corretion = -static_cast<int>(static_cast<double>(offset_y) / 2);
    }

    POINT centre_pt{ centre->x + finder_rc.left + x_corretion,
                     centre->y + finder_rc.top  + y_corretion};

    POINT click_pt{ centre_pt.x + offset_x,
                    centre_pt.y + offset_y };

    LOG_DEBUG("[call_fn] click_next_item_in_line centre=(%ld,%ld) "
              "→ click=(%ld,%ld)\n",
              centre_pt.x, centre_pt.y, click_pt.x, click_pt.y);

    dw::click(ctx.hwnd, click_pt.x, click_pt.y);
    return true;
}


/*────────────────── read_from_selected_item ──────────────────*/
/* args:
 * 0 var_name
 * 1 finder_left   2 finder_top    3 finder_width  4 finder_height
 * 5 delta_x       6 delta_y
 * 7 namebox_w     8 namebox_h
 */
bool read_from_selected_item(Context& ctx,
                             const std::vector<std::string>& args)
{
    if (args.size() < 9) {
        LOG_ERROR("read_from_selected_item: need 9 args, got %zu\n", args.size());
        return false;
    }

    const std::string& var_name     = args[0];

    const int finder_left           = std::stoi(args[1]);
    const int finder_top            = std::stoi(args[2]);
    const int finder_width          = std::stoi(args[3]);
    const int finder_height         = std::stoi(args[4]);

    const int delta_x               = std::stoi(args[5]);
    const int delta_y               = std::stoi(args[6]);
    const int namebox_w             = std::stoi(args[7]);
    const int namebox_h             = std::stoi(args[8]);

    RECT finder_rc{ finder_left,
                    finder_top,
                    finder_left + finder_width,
                    finder_top + finder_height };

    LOG_EVENT("[call_fn] read_from_selected_item finder (%d,%d,%d,%d)\n",
              finder_left, finder_top, finder_width, finder_height);

    cv::Mat full  = so::detail::capture(ctx.hwnd);
    cv::Mat sub   = full(cv::Rect(finder_rc.left,  finder_rc.top,
                                  finder_rc.right - finder_rc.left,
                                  finder_rc.bottom - finder_rc.top));

    auto centre = find_orange_box_center(sub);
    if (!centre) {
        LOG_ERROR("[call_fn] read_from_selected_item → orange bar not found\n");
        return false;
    }

    POINT centre_pt{ centre->x + finder_rc.left,
                     centre->y + finder_rc.top };

    RECT namebox_rc{
        centre_pt.x + delta_x,
        centre_pt.y + delta_y,
        centre_pt.x + delta_x + namebox_w,
        centre_pt.y + delta_y + namebox_h
    };

    std::string value = so::read_region(ctx.hwnd, namebox_rc);
    ctx.vars[var_name] = value;

    LOG_EVENT("[call_fn] read_from_selected_item \"%s\" = <%s>\n",
              var_name.c_str(), value.c_str());
    return true;
}




// returns the 4 “extreme” centers: top, bottom, left, right
struct Extremes {
    cv::Point2d top, bottom, left, right;
};

Extremes find_white_square_centers(const cv::Mat& prev,
                                   const cv::Mat& post)
{
    // read params
    int thr       = CFG_INT("white_diff_thresh", 30);
    int max_area  = CFG_INT("max_arrow_area", 5000);
    int min_area  = CFG_INT("min_arrow_area", 500);
    int fuzz      = CFG_INT("change_zone_white_square_fuse_comparison", 350);
    int dilateK   = CFG_INT("diff_dilate_ksize", 3) | 1;               // ensure odd
    int morphSize = CFG_INT("morph_size", 7) | 1;                   // ensure odd

    // 1) diff → gray
    cv::Mat diff, gray;
    cv::absdiff(post, prev, diff);
    cv::cvtColor(diff, gray, cv::COLOR_BGRA2GRAY);
    LOG_DEBUG("[find_white] gray diff %dx%d\n", gray.cols, gray.rows);

    // 2) optional dialte (max filter) to smooth discontinuities
    if(dilateK) {
        // build a square kernel of size dilateK×dilateK
        cv::Mat kernelBlur = cv::getStructuringElement(
            cv::MORPH_RECT, cv::Size(dilateK, dilateK));
    
        // apply max‐filter (grayscale dilation)
        cv::Mat dilated;
        cv::dilate(gray, dilated, kernelBlur);
        gray = dilated;   // overwrite, same as median blur
        LOG_DEBUG("[find_white] dilate(max) k=%d\n", dilateK);
    }
    
    // 3) threshold → binarize
    cv::Mat mask;
    cv::threshold(gray, mask, thr, 255, cv::THRESH_BINARY);
    LOG_DEBUG("[find_white] after thresh(%d): %d pix\n", thr, cv::countNonZero(mask));

    // 3.5) apply frame‐mask: keep only two vertical and two horizontal stripes
    {
        LOG_DEBUG("[find_white] applying frame mask\n");
        cv::Mat frameMask = cv::Mat::zeros(mask.size(), mask.type());
        int H = mask.rows, W = mask.cols;

        // vertical stripes: x∈[280,500), [2060,2276)
        std::vector<std::pair<int,int>> xRanges{{280,500}, {2060,2276}};
        for (auto [x0,x1] : xRanges) {
            cv::Rect r(x0, 0, x1 - x0, H);
            frameMask(r).setTo(255);
        }

        // horizontal stripes: y∈[0,150), [1000,1160)
        std::vector<std::pair<int,int>> yRanges{{0,150}, {1000,1160}};
        for (auto [y0,y1] : yRanges) {
            cv::Rect r(0, y0, W, y1 - y0);
            frameMask(r).setTo(255);
        }

        // mask out everything else
        mask &= frameMask;
        LOG_DEBUG("[find_white] after frame mask: %d pix\n", cv::countNonZero(mask));
    }

    // 4) morphology: first close small holes, then open tiny spots
    cv::Mat kernel = cv::getStructuringElement(
        cv::MORPH_RECT, cv::Size(morphSize, morphSize));
    cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, kernel);
    cv::morphologyEx(mask, mask, cv::MORPH_OPEN,  kernel);
    LOG_DEBUG("[find_white] after morph k=%d: %d pix\n", 
              morphSize, cv::countNonZero(mask));

    // 5) connected components
    cv::Mat labels, stats, centroids;
    int ncomp = cv::connectedComponentsWithStats(
        mask, labels, stats, centroids, 8);
    LOG_DEBUG("[find_white] %d comps\n", ncomp);

    // 6) collect valid squares
    struct C { cv::Point2d ctr; int area; };
    std::vector<C> comps;
    for (int i = 1; i < ncomp; ++i) {
        int area = stats.at<int>(i, cv::CC_STAT_AREA);
        if (area > max_area || area < min_area) {
            LOG_DEBUG("[find_white] skip comp %d area=%d\n", i, area);
            continue;
        }
        double x = centroids.at<double>(i, 0);
        double y = centroids.at<double>(i, 1);
        comps.push_back({{x,y}, area});
        LOG_DEBUG("[find_white] keep comp %d \tarea=%d \t at = (  %.1f,\t%.1f  )\n", 
                  i, area, x, y);
    }
    if (comps.empty()) {
        LOG_ERROR("[find_white] no squares after filter!\n");
        throw std::runtime_error("find_white: 0 centers");
    }

    // 6.5) debug: annotate centers + areas on the post image
    if (CFG_BOOL("debug_img", false)) {
        so::detail::save_debug_image(mask,  "mask_change_map");
        // convert post (BGRA) → BGR for drawing
        cv::Mat dbg;
        cv::cvtColor(post, dbg, cv::COLOR_BGRA2BGR);

        for (auto &c : comps) {
            // round center to int
            cv::Point pt(int(c.ctr.x), int(c.ctr.y));

            // draw a filled red circle
            cv::circle(dbg, pt, 5, cv::Scalar(0, 0, 255), -1);

            // draw the area next to it (slightly offset above-right)
            std::string txt = std::to_string(c.area);
            cv::putText(dbg, txt, pt + cv::Point(6, -6),
                        cv::FONT_HERSHEY_SIMPLEX,
                        0.5,               // font scale
                        cv::Scalar(0,0,255),
                        1,                 // thickness
                        cv::LINE_AA);
        }

        // save out the annotated image
        so::detail::save_debug_image(dbg, "white_centers");
        LOG_DEBUG("[find_white] debug_centers: saved annotated image\n");
    }

    // 7) fuzzy extreme comparators (as before)
    auto topCmp    = [fuzz](auto &a, auto &b){
        double dy = a.ctr.y - b.ctr.y;
        if (std::abs(dy) > fuzz) return a.ctr.y < b.ctr.y;
        return a.area > b.area;
    };
    auto bottomCmp = [fuzz](auto &a, auto &b){
        double dy = a.ctr.y - b.ctr.y;
        if (std::abs(dy) > fuzz) return a.ctr.y > b.ctr.y;
        return a.area > b.area;
    };
    auto leftCmp   = [fuzz](auto &a, auto &b){
        double dx = a.ctr.x - b.ctr.x;
        if (std::abs(dx) > fuzz) return a.ctr.x < b.ctr.x;
        return a.area > b.area;
    };
    auto rightCmp  = [fuzz](auto &a, auto &b){
        double dx = a.ctr.x - b.ctr.x;
        if (std::abs(dx) > fuzz) return a.ctr.x > b.ctr.x;
        return a.area > b.area;
    };

    // 8) pick extremes
    Extremes ex;
    ex.top    = std::min_element(comps.begin(), comps.end(), topCmp)->ctr;
    ex.bottom = std::min_element(comps.begin(), comps.end(), bottomCmp)->ctr;
    ex.left   = std::min_element(comps.begin(), comps.end(), leftCmp)->ctr;
    ex.right  = std::min_element(comps.begin(), comps.end(), rightCmp)->ctr;

    LOG_DEBUG(
      "[find_white] extremes: top(%.1f,%.1f), bottom(%.1f,%.1f), "
      "left(%.1f,%.1f), right(%.1f,%.1f)\n",
      ex.top.x, ex.top.y,
      ex.bottom.x, ex.bottom.y,
      ex.left.x, ex.left.y,
      ex.right.x, ex.right.y
    );
    return ex;
}


/*────────────────── change_map ──────────────────*/
/* args:
 * 0 [up,right,left,down]
 */
bool change_map(dp::Context& ctx,
                const std::vector<std::string>& args)
{
    if (args.empty()) {
        LOG_ERROR("[change_map] missing direction argument\n");
        return false;
    }
    const auto dir = args[0];
    LOG_EVENT("[change_map] direction='%s'\n", dir.c_str());

    // 1. before
    cv::Mat prev = so::detail::capture(ctx.hwnd);
    LOG_DEBUG("[change_map] captured prev frame\n");

    // 2. trigger map move
    dw::send_vk_infocus(ctx.hwnd, "a");
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    LOG_DEBUG("[change_map] sent key 'a' and waited 150ms\n");

    // 3. after
    cv::Mat post = so::detail::capture(ctx.hwnd);
    LOG_DEBUG("[change_map] captured post frame\n");

    // 4. call the diff→center routine (note the '='!)
    LOG_DEBUG("[change_map] about to find white squares...\n");
    Extremes ex;
    try {
        ex = find_white_square_centers(prev, post);
        LOG_DEBUG(
          "[change_map] extremes: top=(%.1f,%.1f) bottom=(%.1f,%.1f) "
          "left=(%.1f,%.1f) right=(%.1f,%.1f)\n",
          ex.top.x,    ex.top.y,
          ex.bottom.x, ex.bottom.y,
          ex.left.x,   ex.left.y,
          ex.right.x,  ex.right.y
        );
    }
    catch (const std::exception& e) {
        LOG_ERROR("[change_map] exception in find_white_square_centers: %s\n", e.what());
        return false;
    }

    // 5. pick your click point
    int cx, cy;
    if      (dir == "up")    { cx = int(ex.top.x);    cy = int(ex.top.y); }
    else if (dir == "down")  { cx = int(ex.bottom.x); cy = int(ex.bottom.y); }
    else if (dir == "left")  { cx = int(ex.left.x);   cy = int(ex.left.y); }
    else if (dir == "right") { cx = int(ex.right.x);  cy = int(ex.right.y); }
    else {
        LOG_ERROR("[change_map] invalid direction '%s'\n", dir.c_str());
        return false;
    }

    LOG_EVENT("[change_map] clicking to chang the map to [%s]=(%d,%d)\n", dir.c_str(), cx, cy);
    dw::click(ctx.hwnd, cx, cy);

    return true;
}

}; // namespace dp_fn