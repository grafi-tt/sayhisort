#pragma once

#ifdef __cplusplus
#include <cstddef>
#include <cstdint>
extern "C" {
#else
#include <stddef.h>
#include <stdint.h>
#endif

void RunOctoSort(uint64_t* data, size_t numel);

#ifdef __cplusplus
}
#endif
