#ifndef SAYHISORT_PROFILE_UTIL_H
#define SAYHISORT_PROFILE_UTIL_H

#include <chrono>
#include <cstddef>
#include <cstdint>
namespace sayhisort::test {
namespace detail {

template <size_t N>
struct CompileTimeString {
    constexpr CompileTimeString(const char (&s)[N]) {
        for (size_t i = 0; i < N; ++i) {
            value[i] = s[i];
        }
    }
    char value[N];
};

template <size_t N>
CompileTimeString(const char (&s)[N]) -> CompileTimeString<N>;

class PerfTraceRegistrator {
public:
    PerfTraceRegistrator(void (*report_fn)());
};

void ReportImpl(const char* tag, uint64_t sum_ns);

}  // namespace detail

template <detail::CompileTimeString kTag>
class PerfTrace {
public:
    PerfTrace() : start_{std::chrono::steady_clock::now()} {
        static detail::PerfTraceRegistrator reg{PerfTrace<kTag>::Report};
    }
    ~PerfTrace() {
        sum_ns_ +=
            std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start_).count();
    }

    static void Report() {
        detail::ReportImpl(kTag.value, sum_ns_);
        sum_ns_ = 0;
    }

private:
    std::chrono::time_point<std::chrono::steady_clock> start_;

    static inline uint64_t sum_ns_ = 0;
};

void ReportPerfTrace();

}  // namespace sayhisort::test

#define SAYHISORT_PERF_TRACE(tag) \
    ::sayhisort::test::PerfTrace<::sayhisort::test::detail::CompileTimeString{#tag}> _perf_trace_##tag

#endif  // SAYHISORT_PROFILE_UTIL_H
