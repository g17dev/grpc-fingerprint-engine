#include "libfprint_adapter.h"
#include <iostream>
#include <libfprint-2/fprint.h>
#include <chrono>
#include <thread>
#include <vector>
#include <algorithm>

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

std::string capture_to_fmd(int duration_seconds, bool multiple_samples) {
    if (!global_device) return "";
    
    std::cout << "🔵 Coloca tu dedo en el lector (" << duration_seconds << "s)..." << std::endl;
    
    // Vector para guardar todas las muestras con su calidad
    struct Sample {
        std::string fmd;
        int minutiae_count;
        FpImage *img;
    };
    std::vector<Sample> samples;
    
    auto start = std::chrono::steady_clock::now();
    
    while (std::chrono::steady_clock::now() - start < std::chrono::seconds(duration_seconds)) {
        GError *error = nullptr;
        FpImage *img = fp_device_capture_sync(global_device, TRUE, nullptr, &error);
        
        if (img) {
            fp_image_detect_minutiae(img, nullptr, nullptr, nullptr);
            GPtrArray *minutiae = fp_image_get_minutiae(img);
            int count = minutiae ? minutiae->len : 0;
            
            if (count > 0) {
                std::string fmd;
                for (guint i = 0; i < minutiae->len; i++) {
                    FpMinutia *min = (FpMinutia *)g_ptr_array_index(minutiae, i);
                    int x, y;
                    fp_minutia_get_coords(min, &x, &y);
                    if (i > 0) fmd += ";";
                    fmd += std::to_string(x) + "," + std::to_string(y);
                }
                samples.push_back({fmd, count, img});
            } else {
                g_object_unref(img);
            }
        }
        if (error) g_clear_error(&error);
    }
    
    // Ordenar por cantidad de minucias (mejor primero)
    std::sort(samples.begin(), samples.end(), [](const Sample& a, const Sample& b) {
        return a.minutiae_count > b.minutiae_count;
    });
    
    // Liberar imágenes que no usaremos
    int keep = multiple_samples ? std::min(2, (int)samples.size()) : 1;
    for (int i = keep; i < (int)samples.size(); i++) {
        g_object_unref(samples[i].img);
    }
    
    if (samples.empty()) {
        std::cout << "❌ No se capturaron muestras válidas" << std::endl;
        return "";
    }
    
    std::string result;
    if (multiple_samples && samples.size() >= 2) {
        result = samples[0].fmd + "|" + samples[1].fmd;
        std::cout << "✅ Listo | Muestras: " << samples.size() 
                  << " | Mejores: " << samples[0].minutiae_count 
                  << " y " << samples[1].minutiae_count << " minucias" << std::endl;
    } else {
        result = samples[0].fmd;
        std::cout << "✅ Listo | Muestras: " << samples.size() 
                  << " | Mejor: " << samples[0].minutiae_count << " minucias" << std::endl;
    }
    
    // Liberar imágenes que guardamos
    for (int i = 0; i < keep; i++) {
        g_object_unref(samples[i].img);
    }
    
    return result;
}

void fp_close_device() {
    if (global_device) {
        GError *error = nullptr;
        fp_device_close_sync(global_device, nullptr, &error);
        if (error) { std::cerr << "Error: " << error->message << std::endl; g_clear_error(&error); }
        global_device = nullptr;
    }
}