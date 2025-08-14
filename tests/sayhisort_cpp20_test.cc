#include "sayhisort.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <functional>
#include <numeric>
#include <vector>

#include <gtest/gtest.h>

#include "sayhisort_test_util.h"

namespace {

using namespace sayhisort::test;

std::mt19937_64 GetPerTestRNG() {
    int seed = testing::UnitTest::GetInstance()->random_seed();
    const auto* test_info = testing::UnitTest::GetInstance()->current_test_info();
    const char* suite_name = test_info->test_suite_name();
    const char* test_name = test_info->name();
    return GetRNG(seed, {suite_name, "::", test_name});
}

TEST(SayhiSortTest, SortCpp20API) {
    size_t ary_len = 100;
    std::vector<int> ary(ary_len);
    std::vector<int> expected(ary_len);

    auto rng = GetPerTestRNG();

    std::iota(ary.begin(), ary.end(), 0);
    std::shuffle(ary.begin(), ary.end(), rng);
    std::copy(ary.begin(), ary.end(), expected.begin());
    sayhisort::sort(ary);
    std::stable_sort(expected.begin(), expected.end());
    EXPECT_EQ(ary, expected);
}

}  // namespace

int main(int argc, char** argv) {
    constexpr std::array<int, 9> a = ([]() {
        std::array pi{3, 1, 4, 1, 5, 9, 2, 6, 5};
        sayhisort::sort(pi.begin(), pi.end(), std::less<int>{});
        return pi;
    })();
    static_assert(a == std::array{1, 1, 2, 3, 4, 5, 5, 6, 9});

    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
