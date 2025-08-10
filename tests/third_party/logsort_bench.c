#include "logsort_bench.h"

#define VAR uint64_t
#define CMP comp

static inline int comp(const void* a, const void* b) {
    return (*(const uint64_t*)a > *(const uint64_t*)b) - (*(const uint64_t*)a < *(const uint64_t*)b);
}

#include <logsort.h>

void RunLogSort(uint64_t* data, size_t numel) {
    logsort(data, numel, 512);
}
