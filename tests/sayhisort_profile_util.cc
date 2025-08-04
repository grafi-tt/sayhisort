#include "sayhisort_profile_util.h"

#include <iostream>
#include <vector>

namespace sayhisort::test {
namespace detail {
namespace {

std::vector<void (*)()> g_registered_;

}  // namespace

void ReportImpl(const char* tag, uint64_t sum_ns) {
    std::cout << tag << ": " << (sum_ns / 1000000.0) << "ms" << std::endl;
}

PerfTraceRegistrator::PerfTraceRegistrator(void (*report_fn)()) {
    g_registered_.push_back(report_fn);
}

}  // namespace detail

void ReportPerfTrace() {
    for (auto fn : detail::g_registered_) {
        fn();
    }
}

}  // namespace sayhisort::test
