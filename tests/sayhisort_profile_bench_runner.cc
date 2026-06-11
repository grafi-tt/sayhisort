#include "sayhisort_profile_bench_runner.h"

#include "sayhisort_profile_util.h"
#include "sayhisort.h"

void RunSayhiSortProfile(std::vector<uint64_t>& data) {
    sayhisort::sort(data.begin(), data.end());
}
