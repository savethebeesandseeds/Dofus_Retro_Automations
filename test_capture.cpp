/* test_capture.cpp */
#include "dlog.hpp"
#include "dwin_api.hpp"
#include "dscreen_ocr.hpp"   // OCR wrapper we built

int main() {
    SetConsoleOutputCP(CP_UTF8);
    std::setlocale(LC_ALL, ".UTF8");
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    
    LOG_INFO("IMPORTANTE: debes modificar las propiedades del .exe de Dofus.\n");
    LOG_INFO("\t - Configura el modo de compatibilidad a 'Windows 7'.\n");
    LOG_INFO("\t - Desactiva la opción de 'Escalar en altas DPI'.\n");
    LOG_INFO("\t - Esto es necesario para que las capturas de pantalla y OCR funcionen correctamente.\n");
    
    
    LOG_INFO("Starting...\n");
    const char *temp_dir = CFG_STR("temp_dir", "./temp").c_str();
    std::string window_label = CFG_STR("window", "......");

    LOG_INFO("Cleaning temporal directory: %s...\n", temp_dir);
    du::DeleteFilesInDirectory(temp_dir);


    HWND hwnd = dw::find_window_utf8(window_label, true);
    if (!hwnd) {
        LOG_ERROR("Window not found\n");
        return 1;
    }

    LOG_INFO("Hooked window: %s\n", dw::get_window_title(hwnd).c_str());

    so::detail::capture(hwnd, true);

    LOG_INFO("Capture image, check temp folder: %s\n", temp_dir);
    
    // if (!dw::capture_window(hwnd, "capture.bmp")) {
    //     LOG_ERROR("Failed to capture window\n");
    //     return 1;
    // }

    // LOG_INFO("Saved window snapshot to capture.bmp\n");

    // RECT rcClient, rcWindow;
    // ::GetClientRect(hwnd, &rcClient);
    // ::GetWindowRect(hwnd, &rcWindow);

    // LOG_INFO("Client Area: %d x %d\n", rcClient.right - rcClient.left, rcClient.bottom - rcClient.top);
    // LOG_INFO("Window Area: %d x %d\n", rcWindow.right - rcWindow.left, rcWindow.bottom - rcWindow.top);

    
    // UINT dpi = GetDpiForWindow(hwnd);
    // LOG_INFO("Window DPI: %u\n", dpi);

    // std::string txt = so::read_window(hwnd, tesseract::PSM_SPARSE_TEXT);
    // LOG_INFO("=== Full OCR ===\n%s\n", txt.c_str());

    // RECT loot{320, 1215, 1230, 1430};
    // std::string loot_txt = so::read_region(hwnd, loot);
    // LOG_INFO("=== Loot OCR ===\n%s\n", loot_txt.c_str());

    // /* claen the enviroment */
    // if(CFG_BOOL("delete_temp", false)) {
    //     LOG_INFO("Cleaning temporal directory: %s...\n", temp_dir);
    //     du::DeleteFilesInDirectory(temp_dir);
    // }


    return 0;
}
