#include "sayhisort_profile_util.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <functional>
#include <iostream>
#include <tuple>

#include "sayhisort_bench_data.h"
#include "sayhisort_test_util.h"

#include "sayhisort_bench_runner.h"
#include "stablesort_bench_runner.h"

#ifdef SAYHISORT_THIRDPARTY_BENCH
#include "third_party/logsort_bench_runner.h"
#include "third_party/octosort_bench_runner.h"
#include "third_party/wikisort_bench_runner.h"
#endif

int main() {
    using namespace sayhisort::test;

    constexpr int seed = 42;
    constexpr uint64_t kSize = 1500000;

#define BENCHDATA(name)                                                        \
    std::tuple<const char*, void (*)(uint64_t*, uint64_t, std::mt19937_64&)> { \
        #name, name                                                            \
    }
    static std::array kBenchData{
        BENCHDATA(Random),          BENCHDATA(RandomFew), BENCHDATA(MostlyDescending),
        BENCHDATA(MostlyAscending), BENCHDATA(Ascending), BENCHDATA(Descending),
        BENCHDATA(Equal),           BENCHDATA(Jittered),  BENCHDATA(MostlyEqual),
        BENCHDATA(Append),          BENCHDATA(SqrtKeys),
    };

    std::vector<uint64_t> data(kSize);
    std::vector<uint64_t> expected(kSize);
    for (auto [name, fn] : kBenchData) {
        Report(std::cout, name, true);
        const std::mt19937_64 gen = GetRNG(seed, {"SayhiSortBench", "::", name});

        std::mt19937_64 tmpgen = gen;
        fn(data.data(), kSize, tmpgen);
        {
            SAYHISORT_PERF_TRACE("std::stable_sort");
            RunStableSort(data);
        }
        std::copy(data.begin(), data.end(), expected.begin());
        Report(std::cout, "std::stable_sort");

        tmpgen = gen;
        fn(data.data(), kSize, tmpgen);
        {
            SAYHISORT_PERF_TRACE("sayhisort");
            RunSayhiSort(data);
        }
        Report(std::cout, "sayhisort", true);
        Report(std::cout);
        PopReportIndent();
        if (data != expected) {
            std::cout << "Result check failed!" << std::endl;
            return 1;
        }

#ifdef SAYHISORT_THIRDPARTY_BENCH
        tmpgen = gen;
        fn(data.data(), kSize, tmpgen);
        {
            SAYHISORT_PERF_TRACE("wikisort");
            RunWikiSort(data);
        }
        Report(std::cout, "wikisort");
        if (data != expected) {
            std::cout << "Result check failed!" << std::endl;
            return 1;
        }

        tmpgen = gen;
        fn(data.data(), kSize, tmpgen);
        {
            SAYHISORT_PERF_TRACE("octosort");
            RunOctoSort(data.data(), kSize);
        }
        Report(std::cout, "octosort");
        if (data != expected) {
            std::cout << "Result check failed!" << std::endl;
            return 1;
        }

        tmpgen = gen;
        fn(data.data(), kSize, tmpgen);
        {
            SAYHISORT_PERF_TRACE("logsort");
            RunLogSort(data.data(), kSize);
        }
        Report(std::cout, "logsort");
        if (data != expected) {
            std::cout << "Result check failed!" << std::endl;
            return 1;
        }
#endif

        PopReportIndent();
    }

    return 0;
}
