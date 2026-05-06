#ifndef LIBFPRINT_ADAPTER_H
#define LIBFPRINT_ADAPTER_H

#include <string>

bool fp_init_device();
std::string capture_to_fmd(int duration_seconds = 4, bool multiple_samples = false);
void fp_close_device();

#endif