#include "sayhisort_profile_bench_runner.h"

#include "sayhisort_profile_util.h"

#include "sayhisort.h"

namespace {

// A comparator local to this translation unit. `SAYHISORT_PERF_TRACE` changes the bodies of the internal functions in
// this translation unit, so they must not share instantiations with `RunSayhiSort`. Otherwise it violates ODR, and the
// linker may keep only either version.
struct ProfileLess {
    bool operator()(uint64_t a, uint64_t b) const { return a < b; }
};

}  // namespace

void RunSayhiSortProfile(std::vector<uint64_t>& data) {
    sayhisort::sort(data.begin(), data.end(), ProfileLess{});
}
