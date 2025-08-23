#include "sayhisort_bench_runner.h"

#include "sayhisort_profile_util.h"
#include "sayhisort.h"

#ifdef SAYHISORT_DISABLE_PROFILE
void RunSayhiSort(std::vector<uint64_t>& data) {
    sayhisort::sort(data.begin(), data.end());
}
#else
void RunSayhiSortProfile(std::vector<uint64_t>& data) {
    sayhisort::sort(data.begin(), data.end());
}
#endif
