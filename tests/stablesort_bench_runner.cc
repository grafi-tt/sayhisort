#include "stablesort_bench_runner.h"

#include <algorithm>

void RunStableSort(std::vector<uint64_t>& data) {
    std::stable_sort(data.begin(), data.end());
}
