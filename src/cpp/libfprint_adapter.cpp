#include "libfprint_adapter.h"
#include <iostream>
#include <libfprint-2/fprint.h>
#include <chrono>
#include <thread>

static FpDevice *global_device = nullptr;

bool fp_init_device() {
    if (global_device) return true;
    FpContext *ctx = fp_context_new();
    GPtrArray *devs = fp_context_get_devices(ctx);
    if (!devs || devs->len == 0) {
        std::cerr << "No se encontraron dispositivos" << std::endl;
        g_object_unref(ctx);
        return false;
    }
    global_device = FP_DEVICE(g_ptr_array_index(devs, 0));
    std::cout << "Dispositivo: " << fp_device_get_name(global_device) << std::endl;
    GError *error = nullptr;
    if (!fp_device_open_sync(global_device, nullptr, &error)) {
        std::cerr << "Error al abrir: " << error->message << std::endl;
        g_clear_error(&error);
        return false;
    }
    std::cout << "Lector iniciado correctamente" << std::endl;
    return true;
}

std::string capture_to_fmd() {
    if (!global_device) return "";
    
    std::cout << "🔵 Coloca tu dedo en el lector..." << std::endl;
    
    FpImage *best_img = nullptr;
    std::string best_fmd = "";
    int best_minutiae = 0;
    int frames = 0;
    
    auto start = std::chrono::steady_clock::now();
    
    // Capturar durante 4 segundos exactos
    while (std::chrono::steady_clock::now() - start < std::chrono::seconds(4)) {
        GError *error = nullptr;
        FpImage *img = fp_device_capture_sync(global_device, TRUE, nullptr, &error);
        
        if (img) {
            frames++;
            fp_image_detect_minutiae(img, nullptr, nullptr, nullptr);
            GPtrArray *minutiae = fp_image_get_minutiae(img);
            int count = minutiae ? minutiae->len : 0;
            
            if (count > best_minutiae) {
                if (best_img) g_object_unref(best_img);
                best_img = img;
                
                std::string fmd;
                for (guint i = 0; i < minutiae->len; i++) {
                    FpMinutia *min = (FpMinutia *)g_ptr_array_index(minutiae, i);
                    int x, y;
                    fp_minutia_get_coords(min, &x, &y);
                    if (i > 0) fmd += ";";
                    fmd += std::to_string(x) + "," + std::to_string(y);
                }
                best_fmd = fmd;
                best_minutiae = count;
            } else {
                g_object_unref(img);
            }
        }
        if (error) g_clear_error(&error);
    }
    
    std::cout << "✅ Captura completada | Frames: " << frames 
              << " | Mejor muestra: " << best_minutiae << " minucias" << std::endl;
    std::cout << "👆 Puedes retirar tu dedo" << std::endl;
    
    if (best_img) g_object_unref(best_img);
    return best_fmd;
}

void fp_close_device() {
    if (global_device) {
        GError *error = nullptr;
        fp_device_close_sync(global_device, nullptr, &error);
        if (error) { std::cerr << "Error: " << error->message << std::endl; g_clear_error(&error); }
        global_device = nullptr;
    }
}