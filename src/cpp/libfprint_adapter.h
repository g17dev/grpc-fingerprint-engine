#ifndef LIBFPRINT_ADAPTER_H
#define LIBFPRINT_ADAPTER_H

#include <string>
#include <vector>

bool fp_init_device();
std::string capture_to_fmd();
void fp_close_device();

#endif