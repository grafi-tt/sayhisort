#include "octosort_bench_runner.h"

#include <octosort.h>

static inline int comp(const void* a, const void* b) {
    if (*(const uint64_t*)a < *(const uint64_t*)b) {
        return -1;
    }
    if (*(const uint64_t*)a > *(const uint64_t*)b) {
        return 1;
    }
    return 0;
}

void RunOctoSort(uint64_t* data, size_t numel) {
    octosort(data, numel, sizeof(*data), comp);
}
