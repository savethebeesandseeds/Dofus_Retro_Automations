#include "dconfig.h"
#include "dutils.h"
#include "dread_images.h"

int main() {
    log_info("[Commerce Pilot] Application Started...\n");
    /* read the configuration */
    char *temp_dir = config_temp_dir();
    if (temp_dir == NULL) { 
        log_error("Bad config\n"); 
        return 1;
    }
    /* initialize tesseract api */
    initialize_tesseract();

    // /* claen the enviroment */
    // log_info("Cleaning temporal directory: %s...\n", temp_dir);
    // DeleteFilesInDirectory(temp_dir);
    
    // /* dispatch the thraed that captures the images and the thread that process the images */
    // #pragma omp parallel sections
    // {
    //     #pragma omp section
    //     {
    //         commerce_pilot_process_loop();
    //     }
    // }

    // /* claen the enviroment */
    // if(config_delete_temp()) {
    //     log_info("Cleaning temporal directory: %s...\n", temp_dir);
    //     DeleteFilesInDirectory(temp_dir);
    // }
    
    /* finalize */
    finalize_tesseract();
    free(temp_dir);

    return 0;
}