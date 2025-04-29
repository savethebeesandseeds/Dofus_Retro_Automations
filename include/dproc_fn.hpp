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

}; // namespace dp_fn