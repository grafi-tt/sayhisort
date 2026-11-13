// libFuzzer entry point. Enabled by `-DSAYHISORT_ENABLE_FUZZER=ON` (Clang only).
//
// Usage: sayhisort_fuzzer [corpus_dir] [-max_total_time=seconds]

#include <cstddef>
#include <cstdint>

#include "sayhisort_fuzz_util.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    sayhisort::test::CheckSortWithFuzzInput(data, size);
    return 0;
}
