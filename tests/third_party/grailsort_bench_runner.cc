#include "grailsort_bench_runner.h"

#define SORT_TYPE uint64_t 
#define SORT_CMP sort_comp 

static inline int sort_comp(const uint64_t* a, const uint64_t* b) {
    if (*a < *b) {
        return -1;
    }
    if (*a > *b) {
        return 1;
    }
    return 0;
}

#include <GrailSort.h>

void RunGrailSort(std::vector<uint64_t>& data) {
    GrailSortWithBuffer(data.data(), data.size());
}
