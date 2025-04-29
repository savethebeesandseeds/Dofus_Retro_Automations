/* test_proc.cpp */
#include "dlog.hpp"
#include "dwin_api.hpp"
#include "dscreen_ocr.hpp"   // OCR wrapper we built
#include "dproc.hpp"

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

    dp::Context ctx{.hwnd = hwnd};

    dp::run_proc(ctx, CFG_STR("procedure_name", "......"));   // ← one-liner launch


    /* claen the enviroment */
    if(CFG_BOOL("delete_temp", false)) {
        LOG_INFO("Cleaning temporal directory: %s...\n", temp_dir);
        du::DeleteFilesInDirectory(temp_dir);
    }
    
    return 0;
}
