#include "sayhisort_bench_runner.h"

#include "sayhisort.h"

void RunSayhiSort(std::vector<uint64_t>& data) {
    sayhisort::sort(data.begin(), data.end());
}
