#include "libfprint_adapter.h"
#include <iostream>
#include <libfprint-2/fprint.h>

static FpDevice *global_device = nullptr;

bool fp_init_device() {
    if (global_device) return true;

    FpContext *ctx = fp_context_new();
    
    GPtrArray *devs = fp_context_get_devices(ctx);
    if (!devs || devs->len == 0) {
        std::cerr << "No se encontraron dispositivos de huella" << std::endl;
        g_object_unref(ctx);
        return false;
    }
    
    global_device = FP_DEVICE(g_ptr_array_index(devs, 0));
    std::cout << "Dispositivo encontrado: " << fp_device_get_name(global_device) << std::endl;
    
    GError *error = nullptr;
    if (!fp_device_open_sync(global_device, nullptr, &error)) {
        std::cerr << "Error al abrir el lector: " << error->message << std::endl;
        g_clear_error(&error);
        g_object_unref(ctx);
        return false;
    }
    
    std::cout << "Lector iniciado correctamente" << std::endl;
    return true;
}

std::string capture_to_fmd() {
    if (!global_device) {
        std::cerr << "El dispositivo no está inicializado" << std::endl;
        return "";
    }
    
    GError *error = nullptr;
    FpImage *img = nullptr;
    
    std::cout << "Coloca tu dedo en el lector..." << std::endl;
    
    img = fp_device_capture_sync(global_device, TRUE, nullptr, &error);
    if (!img) {
        std::cerr << "Error en captura: " << error->message << std::endl;
        g_clear_error(&error);
        return "";
    }
    
    gsize len;
    const guchar *raw_data = fp_image_get_data(img, &len);
    std::string fmd_base64 = g_base64_encode(raw_data, len);
    
    std::cout << "Huella capturada: " << len << " bytes" << std::endl;
    
    g_object_unref(img);
    return fmd_base64;
}

void fp_close_device() {
    if (global_device) {
        GError *error = nullptr;
        fp_device_close_sync(global_device, nullptr, &error);
        if (error) {
            std::cerr << "Error al cerrar: " << error->message << std::endl;
            g_clear_error(&error);
        }
        global_device = nullptr;
        std::cout << "Lector cerrado correctamente" << std::endl;
    }
}