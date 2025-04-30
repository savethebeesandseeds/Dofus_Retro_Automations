/* main.cpp – log right-clicks in target window’s client coords
------------------------------------------------------------ */
#include "dlog.hpp"
#include "dconfig.hpp"
#include "dwin_api.hpp"           // dw:: helpers for title lookup & DPI
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <mutex>

namespace {

/*------------------------------ config ------------------------------*/
const std::filesystem::path kFile       =    CFG_STR("capture_dump_file", "./click_log.txt");
const std::string           kTitle      =    CFG_STR("window", "...");
const bool                  kPartial    =    CFG_BOOL("window_partial", true);

std::mutex g_mu;
HHOOK      g_hook   = nullptr;
HWND       g_hwnd   = nullptr;               // target window

/*----------------------- append one line "x,y" ----------------------*/
void append_click(long x, long y)
{
    std::string           kFormat =    CFG_STR("capture_dump_format", "$x,$y");
    // 2) replace all occurrences of "$x" and "$y"
    auto replace_all = [&](const std::string& key, const std::string& val) {
        size_t pos = 0;
        while ((pos = kFormat.find(key, pos)) != std::string::npos) {
            kFormat.replace(pos, key.size(), val);
            pos += val.size();
        }
    };
    replace_all("$x", std::to_string(x));
    replace_all("$y", std::to_string(y));

    // 3) append to file using C I/O
    std::lock_guard<std::mutex> lock(g_mu);
    FILE* f = std::fopen(kFile.string().c_str(), "a");
    if (!f) {
        LOG_ERROR("cannot open click log file: %s\n", kFile.string().c_str());
        return;
    }
    std::fprintf(f, "%s\n", kFormat.c_str());
    LOG_EVENT("\t→\t %s\n", kFormat.c_str());
    std::fclose(f);
}
/*------------------ low-level mouse hook callback -------------------*/
LRESULT CALLBACK mouse_proc(int code, WPARAM wParam, LPARAM lParam)
{
    if (code == HC_ACTION && wParam == WM_RBUTTONDOWN && g_hwnd)
    {
        const auto* s = reinterpret_cast<const MSLLHOOKSTRUCT*>(lParam);

        /* convert screen → client for the target window */
        POINT pt = s->pt;
        if (!::ScreenToClient(g_hwnd, &pt))
            return CallNextHookEx(g_hook, code, wParam, lParam);

        /* ensure point is inside the client area */
        RECT rc{}; ::GetClientRect(g_hwnd, &rc);
        if (pt.x < rc.left || pt.x >= rc.right ||
            pt.y < rc.top  || pt.y >= rc.bottom)
            return CallNextHookEx(g_hook, code, wParam, lParam);

        append_click(pt.x, pt.y);
        LOG_DEBUG("right-click @ client (%ld,%ld)\n", pt.x, pt.y);
    }

    return CallNextHookEx(g_hook, code, wParam, lParam);
}

} // anonym-ns

/*------------------------------ main -------------------------------*/
int main()
{
    SetConsoleOutputCP(CP_UTF8);
    std::setlocale(LC_ALL, ".UTF8");
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    LOG_INFO("Looking for window: \"%s\" (partial=%s)\n",
            kTitle.c_str(), kPartial ? "true" : "false");

    g_hwnd = dw::find_window_utf8(kTitle, kPartial);
    if (!g_hwnd) {
        LOG_ERROR("target window not found – exiting\n");
        return 1;
    }
    LOG_INFO("Hooked window: %s\n", dw::get_window_title(g_hwnd).c_str());
    LOG_INFO("Logging right-clicks to: %s\n", kFile.string().c_str());

    /* install global mouse hook */
    g_hook = ::SetWindowsHookExW(WH_MOUSE_LL, mouse_proc, nullptr, 0);
    if (!g_hook) {
        LOG_ERROR("SetWindowsHookEx failed\n");
        return 1;
    }

    /* message pump */
    MSG msg;
    while (::GetMessageW(&msg, nullptr, 0, 0) > 0) {
        ::TranslateMessage(&msg);
        ::DispatchMessageW(&msg);
    }

    ::UnhookWindowsHookEx(g_hook);
    return 0;
}
