#include "sayhisort_profile_util.h"
#include "sayhisort.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <iostream>
#include <tuple>

#include "sayhisort_bench_data.h"
#include "sayhisort_test_util.h"

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
        BENCHDATA(Append),
    };

    std::vector<uint64_t> data(kSize);
    std::vector<uint64_t> expected(kSize);
    for (auto [name, fn] : kBenchData) {
        const std::mt19937_64 gen = GetRNG(seed, {"SayhiSortBench", "::", name});

        std::mt19937_64 tmpgen = gen;
        fn(data.data(), kSize, tmpgen);
        {
            SAYHISORT_PERF_TRACE("std::stable_sort");
            std::stable_sort(data.begin(), data.end());
        }
        std::copy(data.begin(), data.end(), expected.begin());

        tmpgen = gen;
        fn(data.data(), kSize, tmpgen);
        {
            SAYHISORT_PERF_TRACE("sayhisort::sort");
            sayhisort::sort(data.begin(), data.end());
        }

        if (data != expected) {
            std::cout << "Result check failed!";
            return 1;
        }

        PushReport(name, std::cout);
        Report(std::cout);
        PopReport();
    }

    return 0;
}
