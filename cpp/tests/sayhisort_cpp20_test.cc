#include "sayhisort.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <functional>
#include <numeric>
#include <ranges>
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

struct Item {
    int key;
    int id;
    friend bool operator==(const Item& lhs, const Item& rhs) { return lhs.key == rhs.key && lhs.id == rhs.id; }
};

bool LessInt(int x, int y) {
    return x < y;
}
int KeyOf(const Item& item) {
    return item.key;
}

TEST(SayhiSortTest, SortCpp20Invocables) {
    size_t ary_len = 1000;
    std::vector<Item> ary(ary_len);

    auto rng = GetPerTestRNG();

    for (size_t i = 0; i < ary_len; ++i) {
        ary[i] = {static_cast<int>(rng() % 100), static_cast<int>(i)};
    }
    std::vector<Item> expected = ary;
    std::stable_sort(expected.begin(), expected.end(), [](const Item& x, const Item& y) { return x.key < y.key; });

    // member pointer projection
    std::vector<Item> work = ary;
    sayhisort::sort(work, {}, &Item::key);
    EXPECT_EQ(work, expected);

    // function pointer comparator and function pointer projection
    work = ary;
    sayhisort::sort(work.begin(), work.end(), &LessInt, &KeyOf);
    EXPECT_EQ(work, expected);
}

TEST(SayhiSortTest, SortCpp20NonSizedSentinel) {
    size_t sorted_len = 1000;
    size_t ary_len = 1100;
    std::vector<Item> ary(ary_len);

    auto rng = GetPerTestRNG();

    // Items before the first negative key are sorted.
    for (size_t i = 0; i < ary_len; ++i) {
        ary[i] = {static_cast<int>(rng() % 100), static_cast<int>(i)};
    }
    ary[sorted_len].key = -1;
    std::vector<Item> expected = ary;
    std::stable_sort(expected.begin(), expected.begin() + sorted_len,
                     [](const Item& x, const Item& y) { return x.key < y.key; });

    std::vector<Item> work = ary;
    auto prefix = work | std::views::take_while([](const Item& item) { return item.key >= 0; });
    static_assert(std::ranges::random_access_range<decltype(prefix)>);
    static_assert(
        !std::sized_sentinel_for<std::ranges::sentinel_t<decltype(prefix)>, std::ranges::iterator_t<decltype(prefix)>>);
    auto it = sayhisort::sort(prefix, {}, &Item::key);
    EXPECT_EQ(it, work.begin() + sorted_len);
    EXPECT_EQ(work, expected);

    // iterator and non-sized sentinel
    work = ary;
    it = sayhisort::sort(prefix.begin(), prefix.end(), {}, &Item::key);
    EXPECT_EQ(it, work.begin() + sorted_len);
    EXPECT_EQ(work, expected);
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
