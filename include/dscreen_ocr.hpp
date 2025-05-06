#pragma once
/*─────────────────────────────────────────────────────────────────────────────
 *  screen_ocr.hpp – capture window → OpenCV → Tesseract OCR
 *────────────────────────────────────────────────────────────────────────────*/
#include "dconfig.hpp"
#include <windows.h>
#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>

#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <string>
#include <vector>
#include <mutex>
#include <algorithm>
#include "dlog.hpp"   // LOG_*
#include "dutils.hpp"
#include "dwin_api.hpp" // dw::*


#define DEBUG_IMG

namespace so {          /*  'so' = screen-ocr  */

/*──────────────────────────────  internal helpers  ─────────────────────────*/
namespace detail {

inline void save_debug_image(const cv::Mat& img, const std::string& tag)
{
    SYSTEMTIME st;
    GetSystemTime(&st);

    char buf[256];
    snprintf(buf, sizeof(buf), "%s/debug_%s_%04d%02d%02d_%02d%02d%02d_%03d.png",
                CFG_STR("temp_dir", "./temp").c_str(), 
                tag.c_str(),
                st.wYear, st.wMonth, st.wDay,
                st.wHour, st.wMinute, st.wSecond,
                st.wMilliseconds);  // <- added milliseconds

    cv::imwrite(buf, img);
}

/* capture HWND → cv::Mat (BGRA, 8-bit) */
inline cv::Mat capture(HWND hwnd, bool overwrite_dbug=false)
{
    RECT rc {};  ::GetClientRect(hwnd, &rc);
    int w = rc.right, h = rc.bottom;

    HDC hdcWin = ::GetDC(hwnd);
    HDC hdcMem = ::CreateCompatibleDC(hdcWin);
    HBITMAP hbm = ::CreateCompatibleBitmap(hdcWin, w, h);
    ::SelectObject(hdcMem, hbm);

    BOOL ok = ::PrintWindow(hwnd, hdcMem, PW_CLIENTONLY);
    if (!ok)     // PrintWindow can fail (UAC, OpenGL, etc.) – fallback:
        ::BitBlt(hdcMem, 0, 0, w, h, hdcWin, 0, 0, SRCCOPY);

    BITMAPINFOHEADER bi{ sizeof(bi), w, -h, 1, 32, BI_RGB };
    cv::Mat img(h, w, CV_8UC4);
    ::GetDIBits(hdcWin, hbm, 0, h, img.data,
                reinterpret_cast<BITMAPINFO*>(&bi), DIB_RGB_COLORS);

    ::DeleteObject(hbm); ::DeleteDC(hdcMem); ::ReleaseDC(hwnd, hdcWin);

if(CFG_BOOL("debug_img",false) && !overwrite_dbug) {
    detail::save_debug_image(img,  "capture");
}

    return img;
}


/* binarise using the same K-means trick you already had              */
inline cv::Mat binarise(const cv::Mat& src)
{
    cv::Mat img; cv::cvtColor(src, img, cv::COLOR_BGRA2BGR);

    cv::Mat f; img.convertTo(f, CV_32F); f = f.reshape(1, img.total());
    cv::Mat labels, centres;
    cv::kmeans(f, 2, labels,
               cv::TermCriteria(cv::TermCriteria::EPS|cv::TermCriteria::MAX_ITER,10,1.0),
               3, cv::KMEANS_PP_CENTERS, centres);

    auto thr = CFG_DBL("binary_image_threshold", 8.0);
    auto c0  = centres.at<cv::Vec3f>(0), c1 = centres.at<cv::Vec3f>(1);
    auto dist = cv::norm(c0 - c1);
    if (dist < thr) { return img; }        // clusters too close → skip

    /* darkest → black, brightest → white */
    int rows = img.rows, cols = img.cols;
    cv::Mat bw(rows, cols, CV_8UC1);
    for (int i = 0; i < rows*cols; ++i)
        bw.data[i] = labels.at<int>(i) ? 255 : 0;
    return bw;
}

inline cv::Mat binarise_adapt(const cv::Mat& src)
{
    cv::Mat gray, bw;
    cv::cvtColor(src, gray, cv::COLOR_BGRA2GRAY);
    cv::adaptiveThreshold(
        gray, bw, 255,
        cv::ADAPTIVE_THRESH_GAUSSIAN_C,
        cv::THRESH_BINARY,  CFG_INT("binarization_blockSize", 11), CFG_INT("binarization_c", 2));          // blkSize, C
    return bw;
}
inline cv::Mat binarise_wrap(const cv::Mat& src) {
    if(CFG_BOOL("adaptative_binarization", false)) {
        return binarise_adapt(src);
    } else {
        return binarise(src);
    }
}

/* convert cv::Mat (BGR/BW) to tesseract image  */
inline void set_image(tesseract::TessBaseAPI& api, const cv::Mat& m)
{
    cv::Mat img;
    int chans = m.channels();
    if (chans == 4) cv::cvtColor(m, img, cv::COLOR_BGRA2RGB);
    else if (chans == 3) cv::cvtColor(m, img, cv::COLOR_BGR2RGB);
    else img = m.clone();                       // already mono

    api.SetImage(img.data, img.cols, img.rows,
                 img.channels(), (int)img.step);
}

} // namespace detail

/*──────────────────────────────  OCR engine  ───────────────────────────────*/
class Engine {
public:
    static Engine& get()
    {
        static Engine eng; return eng;
    }

    std::string read(HWND hwnd);
    std::string read(HWND hwnd, tesseract::PageSegMode psm);
    std::string read(HWND hwnd, const RECT& r);
    std::string read(HWND hwnd, const cv::Mat& prev, const RECT& r);
    std::string read(HWND hwnd, const cv::Mat& prev, tesseract::PageSegMode psm);

    /* return bounding rects (window-relative) whose recognised text contains
       the query substring (case-insensitive).                               */
    std::vector<RECT> find(HWND hwnd,
                            std::string_view query,
                            double conf_threshold = 60);
    std::vector<RECT> find(HWND hwnd,
                            const RECT& roi,
                            std::string_view query,
                            double conf_thr = 60);

private:
    Engine()  { init(); }
    ~Engine() { api_.End(); }
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    void init() {
        std::string path = CFG_STR("languages_path", "./tessdata");
        std::string lang = CFG_STR("language"      , "eng");
        LOG_INFO("USING LANGUAGE: %s\n", lang.c_str());
        
        if (api_.Init(path.c_str(), lang.c_str(), tesseract::OEM_LSTM_ONLY))
            throw std::runtime_error("Tesseract init failed");
            
        api_.SetVariable("debug_file", "nul");                 // silence Leptonica
        api_.SetVariable("load_system_dawg", "0");   // OFF = faster, no
        api_.SetVariable("load_freq_dawg"  , "0");   // unwanted “corrections”
        // api_.SetVariable("load_unambig_dawg", "0");
        // api_.SetVariable("load_number_dawg" , "0");
        api_.SetVariable("classify_min_confidence", CFG_STR("classify_min_confidence", "30").c_str());
        
        api_.SetVariable("preserve_interword_spaces", "1");

        if (CFG_INT("user_dpi", -1) > 0)
            api_.SetVariable("user_defined_dpi", std::to_string(CFG_INT("user_dpi", -1)).c_str());

        api_.SetVariable("tessedit_char_whitelist",
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
            "0123456789"
            "áéíóú"
            "ñÑ"
            ":;./,-+[]()!¡¿? ");
            
        /* default to sparse text – override per-call if you like */
        // api_.SetPageSegMode(tesseract::PSM_SPARSE_TEXT);
        api_.SetPageSegMode(tesseract::PSM_SINGLE_BLOCK);
        // api_.SetPageSegMode(tesseract::PSM_SINGLE_LINE);
    }
public:
    std::string read_(cv::Mat img, tesseract::PageSegMode psm)
    {
        cv::Mat bw;

        if(CFG_BOOL("binarize_for_ocr", "false")) {
            bw  = detail::binarise_wrap(img);
        } else {
            bw  = img.clone();
        }

        if(CFG_BOOL("debug_img",false)) {
            detail::save_debug_image(bw,  "ocr");
        }
        auto old = api_.GetPageSegMode();
        api_.SetPageSegMode(psm);
        
        // cv::Mat upscale;
        // cv::resize(bw, upscale, {}, 2.0, 2.0, cv::INTER_LINEAR);
        // detail::set_image(api_, upscale);
        
        detail::set_image(api_, bw);
        std::unique_ptr<char[]> txt(api_.GetUTF8Text());
        
        api_.SetPageSegMode(old);

        if (!txt) return "";

        std::string s(txt.get());
        if (!s.empty() && (s.back() == '\n' || s.back() == '\r'))
            s.pop_back();

        return s;
    }
private:
    tesseract::TessBaseAPI api_;
    std::mutex             mu_;
};

/*─────────────────────────────  implementation  ───────────────────────────*/
inline std::string Engine::read(HWND hwnd)
{ 
    return read(hwnd, {});
}
inline std::string Engine::read(HWND hwnd, tesseract::PageSegMode psm)
{
    std::lock_guard<std::mutex> lock(mu_);
    
    cv::Mat win = detail::capture(hwnd);
    
    return read_(win, psm);
}
inline std::string Engine::read(HWND hwnd, const RECT& roi)
{
    std::lock_guard<std::mutex> lock(mu_);

    cv::Mat win = detail::capture(hwnd);
    cv::Mat region = roi.right ? win(cv::Rect(roi.left, roi.top,
                                              roi.right - roi.left,
                                              roi.bottom - roi.top)) : win;
    
    return read_(region, 
        (roi.bottom - roi.top) < 60 ? tesseract::PSM_SINGLE_LINE : tesseract::PSM_SINGLE_BLOCK
    );
}
/* diff-OCR that stays simple and never hits the channel-mismatch crash */
inline std::string Engine::read(HWND hwnd,
    const cv::Mat& prev,
    const RECT& roi)
{
    std::lock_guard<std::mutex> lock(mu_);

    /* 1. capture new frame */
    cv::Mat cur = detail::capture(hwnd);

    /* 2. if we don’t have a valid previous frame, fall back to normal read */
    if (prev.empty() || prev.size() != cur.size() || prev.type() != cur.type())
        return read(hwnd, roi);                   // reuse your existing overload

    /* 3. absolute difference (both are CV_8UC4) */
    cv::Mat diff;
    cv::absdiff(cur, prev, diff);                // diff is 4-channel BGRA

    /* 4. make alpha channel fully opaque so PNG preview isn’t transparent */
    {
        std::vector<cv::Mat> ch; cv::split(diff, ch);   // BGRA → {B,G,R,A}
        ch[3].setTo(255);                               // alpha = 255 everywhere
        cv::merge(ch, diff);
    }

    if(CFG_BOOL("debug_img",false)) {
        detail::save_debug_image(cur,  "curr");
        detail::save_debug_image(prev, "prev");
        detail::save_debug_image(diff, "diff");
    }

    /* 5. crop to ROI if given */
    cv::Mat region = roi.right
        ? diff(cv::Rect(roi.left, roi.top,
            roi.right  - roi.left,
            roi.bottom - roi.top))
        : diff;

    /* 6. choose PSM by height and OCR */
    tesseract::PageSegMode psm =
        (roi.bottom - roi.top) < 60
        ? tesseract::PSM_SINGLE_LINE
        : tesseract::PSM_SINGLE_BLOCK;

    return read_(region, psm);      // ← your existing helper
}

/*──────────────────────────── helper that does the actual scan ────────────*/
/*────────────────────────── smarter OCR phrase locator ────────────────────*/
inline std::vector<RECT> scan(cv::Mat img,
    const RECT& roi_shift,        // (0,0,0,0) for full-window
    std::string_view   query,
    double             conf_thr,
    tesseract::TessBaseAPI& api)
{
    LOG_INFO("[scan] start: roi_shift=(%d,%d,%d,%d), query='%.*s', conf_thr=%.2f\n",
             roi_shift.left, roi_shift.top, roi_shift.right, roi_shift.bottom,
             static_cast<int>(query.size()), query.data(), conf_thr);

    std::vector<RECT> hits;                   // final result
    const std::string expected = du::simplify(query);
    LOG_INFO("[scan] simplified query: '%s'\n", expected.c_str());

    if(CFG_BOOL("debug_img",false)) {
        detail::save_debug_image(img, "scan_input");
    }

    /* run Tesseract on the provided image ------------------------------- */
    LOG_INFO("[scan] running initial recognition...\n");
    detail::set_image(api, img);
    api.Recognize(nullptr);

    using Word = struct { std::string txt; std::string txt_s; RECT box; };
    std::vector<Word> words;

    tesseract::ResultIterator* it = api.GetIterator();
    const tesseract::PageIteratorLevel lvl = tesseract::RIL_WORD;

    if (!it) {
        LOG_WARN("[scan] no result iterator after initial recognition.\n");
    } else {
        int word_count = 0;
        for (; !it->Empty(lvl); it->Next(lvl))
        {
            float confidence = it->Confidence(lvl);
            LOG_DEBUG("[scan] iterating word, confidence=%.2f\n", confidence);

            if (confidence < conf_thr) {
                LOG_DEBUG("[scan] word skipped due to low confidence: %.2f\n", confidence);
                continue;
            }

            const char* w = it->GetUTF8Text(lvl);
            if (!w) {
                LOG_WARN("[scan] nullptr returned from GetUTF8Text\n");
                continue;
            }
            LOG_EVENT("[scan] word='%s' (confidence=%.2f)\n", w, confidence);

            int l, t, r, b;
            it->BoundingBox(lvl, &l, &t, &r, &b);
            words.push_back({
                std::string(w),
                du::simplify(w),
                RECT{ l + roi_shift.left, t + roi_shift.top,
                      r + roi_shift.left, b + roi_shift.top }
            });
            delete[] w;
            ++word_count;
        }
        LOG_INFO("[scan] extracted %d words from initial pass.\n", word_count);
    }

    if(CFG_BOOL("debug_img",false)) {
        detail::save_debug_image(img, "scan_input_fixed");
    }

    /* second pass ─ OCR with sparse mode */
    LOG_INFO("[scan] starting second recognition pass (PSM_SPARSE_TEXT)...\n");
    auto old_psm = api.GetPageSegMode();          // keep caller’s mode
    api.SetPageSegMode(tesseract::PSM_SPARSE_TEXT);

    detail::set_image(api, img);
    api.Recognize(nullptr);

    it = api.GetIterator();

    if (!it) {
        LOG_WARN("[scan] no result iterator after second recognition.\n");
    } else {
        int match_count = 0;
        for (; !it->Empty(lvl); it->Next(lvl))
        {
            float confidence = it->Confidence(lvl);
            if (confidence < conf_thr) {
                LOG_DEBUG("[scan] (pass 2) word skipped due to low confidence: %.2f\n", confidence);
                continue;
            }

            const char* w = it->GetUTF8Text(lvl);
            if (!w) {
                LOG_WARN("[scan] (pass 2) nullptr returned from GetUTF8Text\n");
                continue;
            }

            std::string word_s = du::simplify(w);
            delete[] w;

            if (word_s.find(expected) != std::string::npos)
            {
                int l, t, r, b;
                it->BoundingBox(lvl, &l, &t, &r, &b);
                hits.push_back(RECT{ l + roi_shift.left, t + roi_shift.top,
                                     r + roi_shift.left, b + roi_shift.top });

                LOG_INFO("[scan] match found: word='%s' at (%d,%d,%d,%d)\n",
                         word_s.c_str(), l, t, r, b);
                ++match_count;
            } else {
                LOG_DEBUG("[scan] (pass 2) word '%s' does not match query '%s'\n",
                          word_s.c_str(), expected.c_str());
            }
        }
        LOG_INFO("[scan] total matches found: %d\n", match_count);
    }

    api.SetPageSegMode(old_psm);                  // restore
    LOG_INFO("[scan] end, returning %zu hits.\n", hits.size());

    return hits;
}


/*─────────────────────── find over entire window (unchanged API) ───────────*/
inline std::vector<RECT> Engine::find(HWND hwnd,
            std::string_view query,
            double conf_thr)
{
    std::lock_guard<std::mutex> lock(mu_);

    cv::Mat win = detail::capture(hwnd);
    cv::Mat bw  = detail::binarise_wrap(win);
    return scan(bw, RECT{0,0,0,0}, query, conf_thr, api_);
}

/*─────────────────────── find inside a rectangle (NEW) ─────────────────────*/
inline std::vector<RECT> Engine::find(HWND hwnd,
            const RECT& roi,
            std::string_view query,
            double conf_thr)
{
    std::lock_guard<std::mutex> lock(mu_);

    cv::Mat win = detail::capture(hwnd);
    cv::Rect cvroi{roi.left, roi.top, roi.right - roi.left, roi.bottom - roi.top};
    cv::Mat sub = win(cvroi).clone();               // crop
    cv::Mat bw  = detail::binarise_wrap(sub);

    return scan(bw, roi, query, conf_thr, api_);
}


/*─────────────────────────────  public facade  ─────────────────────────────
 *  (thin wrappers around the singleton – for convenience)                   */
inline std::string read_window(HWND hwnd)
{
    return Engine::get().read(hwnd);
}
inline std::string read_window(HWND hwnd, tesseract::PageSegMode psm)
{
    return Engine::get().read(hwnd, psm);
}
inline std::string read_region(HWND hwnd, const RECT& r)
{
    return Engine::get().read(hwnd, r);
}
inline std::string read_window(HWND hwnd, const cv::Mat& prev, tesseract::PageSegMode psm)
{
    return Engine::get().read(hwnd, prev, psm);
}
inline std::string read_region(HWND hwnd, const cv::Mat& prev, const RECT& r)
{
    return Engine::get().read(hwnd, prev, r);
}
/* full window */
inline std::vector<RECT> locate_text(HWND hwnd,
    std::string_view q,
    double conf = 60)
{
    return Engine::get().find(hwnd, q, conf);
}
/* rectangle-limited search */
inline std::vector<RECT> locate_text(HWND hwnd,
    const RECT& roi,
    std::string_view q,
    double conf = 60)
{
    return Engine::get().find(hwnd, roi, q, conf);
}

/* helper: always return 8-bit 1-channel grayscale */
static inline cv::Mat to_gray(const cv::Mat& src)
{
    if (src.channels() == 1) return src;
    cv::Mat g;
    if (src.channels() == 3)      cv::cvtColor(src, g, cv::COLOR_BGR2GRAY);
    else /* 4 channels */         cv::cvtColor(src, g, cv::COLOR_BGRA2GRAY);
    return g;
}

/*──────────────── compare full window ───────────────*/
inline double compare_imag(HWND hwnd, const cv::Mat& prev)
{
    cv::Mat cur = detail::capture(hwnd);
    if (prev.empty() || prev.size() != cur.size()) return 0.0;   // no basis

    cv::Mat a = to_gray(prev);
    cv::Mat b = to_gray(cur);

    cv::Mat diff;  cv::absdiff(a, b, diff);              // |a-b|
    double sumDiff = cv::sum(diff)[0];                   // total absolute error

    double maxDiff = 255.0 * diff.total();               // worst-case error
    double similarity = 1.0 - (sumDiff / maxDiff);       // 1 identical … 0 worst
    return std::clamp(similarity, 0.0, 1.0);
}

/*────────────── compare only a rectangle ─────────────*/
inline double compare_imag(HWND hwnd,
                           const cv::Mat& prev,
                           const RECT& r)
{
    cv::Mat cur = detail::capture(hwnd);
    if (prev.empty() || prev.size() != cur.size()) return 0.0;

    /* guard against bad RECT */
    if (r.right <= r.left || r.bottom <= r.top) return 0.0;

    cv::Rect roi{r.left, r.top, r.right - r.left, r.bottom - r.top};

    cv::Mat a = to_gray(prev)(roi);
    cv::Mat b = to_gray(cur )(roi);

    cv::Mat diff;  cv::absdiff(a, b, diff);
    double sumDiff = cv::sum(diff)[0];
    double maxDiff = 255.0 * diff.total();
    double similarity = 1.0 - (sumDiff / maxDiff);
    return std::clamp(similarity, 0.0, 1.0);
}
} // namespace so
