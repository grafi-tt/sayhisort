#include "sayhisort.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <numeric>
#include <set>
#include <tuple>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

#include "sayhisort_test_util.h"

namespace {

using namespace sayhisort::detail;
using namespace sayhisort::test;

using Iterator = std::vector<int>::iterator;
using Compare = std::less<int>;
using SsizeT = Iterator::difference_type;

struct CompareDiv4 {
    bool operator()(int x, int y) const { return (x >> 2) < (y >> 2); }
};

std::mt19937_64 GetPerTestRNG() {
    int seed = testing::UnitTest::GetInstance()->random_seed();
    const auto* test_info = testing::UnitTest::GetInstance()->current_test_info();
    const char* suite_name = test_info->test_suite_name();
    const char* test_name = test_info->name();
    return GetRNG(seed, {suite_name, "::", test_name});
}

struct FinalLess final : public std::less<int> {};

TEST(SayhiSortTest, IterComp) {
    std::vector ary{0, 1, 2};
    auto it = ary.begin();

    IterComp lt_ebo{std::less<int>{}, VoidProj{}};
    static_assert(std::is_empty_v<decltype(lt_ebo)>);
    EXPECT_TRUE(lt_ebo(it, it + 1));
    EXPECT_FALSE(lt_ebo(it + 1, it + 1));
    EXPECT_FALSE(lt_ebo(it + 2, it + 1));

    IterComp lt_noebo{FinalLess{}, VoidProj{}};
    static_assert(!std::is_empty_v<decltype(lt_noebo)>);
    EXPECT_TRUE(lt_noebo(it, it + 1));
    EXPECT_FALSE(lt_noebo(it + 1, it + 1));
    EXPECT_FALSE(lt_noebo(it + 2, it + 1));

    IterComp gt_ebo{std::less<int>{}, [](int x) { return -x; }};
    static_assert(std::is_empty_v<decltype(gt_ebo)>);
    EXPECT_FALSE(gt_ebo(it, it + 1));
    EXPECT_FALSE(gt_ebo(it + 1, it + 1));
    EXPECT_TRUE(gt_ebo(it + 2, it + 1));

    IterComp gt_noebo{FinalLess{}, [](int x) { return -x; }};
    static_assert(!std::is_empty_v<decltype(gt_noebo)>);
    EXPECT_FALSE(gt_noebo(it, it + 1));
    EXPECT_FALSE(gt_noebo(it + 1, it + 1));
    EXPECT_TRUE(gt_noebo(it + 2, it + 1));
}

TEST(SayhiSortTest, OverApproxSqrt) {
    for (int x = 8; x < 9; ++x) {
        SsizeT ar = OverApproxSqrt(x);
        EXPECT_EQ(ar, 3);
    }
    for (int x = 9; x < 17; ++x) {
        SsizeT ar = OverApproxSqrt(x);
        EXPECT_EQ(ar, 4);
    }
    for (int x = 17; x < 1000; ++x) {
        SsizeT ar = OverApproxSqrt(x);
        double r = std::sqrt(static_cast<double>(x));
        EXPECT_GE(ar, r);
        EXPECT_LT(ar, r * 1.25);
    }
    for (int x = 1000; x < 200000; ++x) {
        SsizeT ar = OverApproxSqrt(x);
        double r = std::sqrt(static_cast<double>(x));
        EXPECT_GE(ar, r);
        EXPECT_LT(ar, r * (1.0 + 1.0 / 32));
    }
    for (int x = 200000; x < 1500000; ++x) {
        SsizeT ar = OverApproxSqrt(x);
        double r = std::sqrt(static_cast<double>(x));
        EXPECT_GE(ar, r);
        EXPECT_LT(ar, r * (1.0 + 1.0 / 256));
    }
}

TEST(SayhiSortTest, Rotate) {
    std::vector<int> data(32);
    std::vector<int> expected(32);

    for (int l : {2, 42, 123}) {
        for (int i = 1; i < l; ++i) {
            std::iota(data.begin(), data.begin() + l, 0);
            Rotate(data.begin(), data.begin() + i, data.begin() + l);
            std::iota(expected.begin() + l - i, expected.begin() + l, 0);
            std::iota(expected.begin(), expected.begin() + l - i, i);
            EXPECT_EQ(data, expected);
        }
    }
}

TEST(SayhiSortTest, BinarySearch) {
    std::vector<int> data(17);
    std::iota(data.begin(), data.end() - 1, 0);
    for (int i = 1; i <= 16; ++i) {
        for (int j = -1; j <= i; ++j) {
            data[16] = j;
            auto it =
                BinarySearch<true>(data.begin(), data.begin() + i, data.begin() + 16, IterComp{Compare{}, VoidProj{}});
            SsizeT idx = it - data.begin();
            EXPECT_EQ(idx, std::max(0, j));
            it =
                BinarySearch<false>(data.begin(), data.begin() + i, data.begin() + 16, IterComp{Compare{}, VoidProj{}});
            idx = it - data.begin();
            EXPECT_EQ(idx, std::min(j + 1, i));
        }
    }
}

TEST(SayhiSortTest, MergeWithBuf) {
    SsizeT ary_len = 32;

    std::vector<int> merged_space(ary_len);
    std::vector<int> rest_space(ary_len);
    std::vector<int> expected(ary_len);

    auto naive_impl = [&](Iterator buf, Iterator xs, Iterator ys, Iterator ys_last, Compare comp) {
        SsizeT len = ys_last - buf;
        Iterator xs_last = ys;

        SsizeT num_merged = 0;
        SsizeT num_rest = 0;

        while (xs < xs_last && ys < ys_last) {
            if (!comp(*ys, *xs)) {
                merged_space[num_merged++] = *xs++;
            } else {
                merged_space[num_merged++] = *ys++;
            }
        }
        bool xs_consumed = xs == xs_last;

        while (xs < xs_last) {
            rest_space[num_rest++] = *xs++;
        }
        while (ys < ys_last) {
            rest_space[num_rest++] = *ys++;
        }
        SsizeT rest_offset = len - num_rest;

        std::copy(merged_space.begin(), merged_space.begin() + num_merged, expected.begin());
        std::fill(expected.begin() + num_merged, expected.begin() + rest_offset, 0);
        std::copy(rest_space.begin(), rest_space.begin() + num_rest, expected.begin() + rest_offset);
        std::fill(expected.begin() + len, expected.end(), 42);

        Iterator new_buf = buf + num_merged;
        Iterator rest = buf + rest_offset;
        return std::tuple{new_buf, xs_consumed, rest};
    };

    std::vector<int> ary(ary_len);
    SsizeT buf_len = 8;
    auto rng = GetPerTestRNG();

    for (SsizeT ys_len = 1; ys_len <= buf_len; ++ys_len) {
        for (SsizeT xs_len = 1; xs_len <= ary_len - (buf_len + ys_len); ++xs_len) {
            Iterator buf = ary.begin();
            Iterator xs = buf + buf_len;
            Iterator ys = xs + xs_len;
            Iterator ys_last = ys + ys_len;

            std::fill(buf, xs, 0);
            std::iota(xs, ys_last, 100);
            std::fill(ys_last, ary.end(), 42);
            std::shuffle(xs, ys_last, rng);
            std::sort(xs, ys, Compare{});
            std::sort(ys, ys_last, Compare{});

            auto [buf_expected, xs_consumed_expected, rest_expected] = naive_impl(buf, xs, ys, ys_last, Compare{});
            auto [xs_consumed, rest] = MergeWithBuf<false>(buf, xs, ys, ys_last, IterComp{Compare{}, VoidProj{}});

            EXPECT_EQ(ary, expected) << "xs_len=" << xs_len << " ys_len=" << ys_len;
            EXPECT_EQ(buf, buf_expected);
            EXPECT_EQ(rest, rest_expected);
            EXPECT_EQ(xs_consumed, xs_consumed_expected);
        }
    }
}

TEST(SayhiSortTest, MergeWithoutBuf) {
    SsizeT ary_len = 24;

    std::vector<int> merged_space(ary_len);
    std::vector<int> rest_space(ary_len);
    std::vector<int> expected(ary_len);

    auto naive_impl = [&](Iterator xs, Iterator ys, Iterator ys_last, Compare comp) {
        SsizeT len = ys_last - xs;
        Iterator xs_orig = xs;
        Iterator xs_last = ys;

        SsizeT num_merged = 0;
        SsizeT num_rest = 0;

        while (xs < xs_last && ys < ys_last) {
            if (!comp(*ys, *xs)) {
                merged_space[num_merged++] = *xs++;
            } else {
                merged_space[num_merged++] = *ys++;
            }
        }
        bool xs_consumed = xs == xs_last;

        while (xs < xs_last) {
            rest_space[num_rest++] = *xs++;
        }
        while (ys < ys_last) {
            rest_space[num_rest++] = *ys++;
        }

        std::copy(merged_space.begin(), merged_space.begin() + num_merged, expected.begin());
        std::copy(rest_space.begin(), rest_space.begin() + num_rest, expected.begin() + num_merged);
        std::fill(expected.begin() + len, expected.end(), 42);

        Iterator rest = xs_orig + num_merged;
        return std::tuple{xs_consumed, rest};
    };

    std::vector<int> ary(ary_len);
    auto rng = GetPerTestRNG();

    for (SsizeT ys_len = 1; ys_len <= ary_len; ++ys_len) {
        for (SsizeT xs_len = 1; xs_len <= ary_len - ys_len; ++xs_len) {
            Iterator xs = ary.begin();
            Iterator ys = xs + xs_len;
            Iterator ys_last = ys + ys_len;

            std::iota(xs, ys_last, 100);
            std::fill(ys_last, ary.end(), 42);
            std::shuffle(xs, ys_last, rng);
            std::sort(xs, ys, Compare{});
            std::sort(ys, ys_last, Compare{});

            auto [xs_consumed_expected, rest_expected] = naive_impl(xs, ys, ys_last, Compare{});
            auto [xs_consumed, rest] = MergeWithoutBuf<false>(xs, ys, ys_last, IterComp{Compare{}, VoidProj{}});

            EXPECT_EQ(ary, expected) << "xs_len=" << xs_len << " ys_len=" << ys_len;
            EXPECT_EQ(rest, rest_expected);
            EXPECT_EQ(xs_consumed, xs_consumed_expected);
            EXPECT_EQ(rest, rest_expected);
        }
    }
}

TEST(SayhiSortTest, InterleaveBlocks) {
    SsizeT ary_len = 32;

    std::vector<int> imit_space(ary_len);
    std::vector<int> merged_space(ary_len);

    auto naive_impl = [&](Iterator imit, Iterator blocks, SsizeT num_blocks, SsizeT block_len, Compare comp) {
        Iterator xs = blocks;
        Iterator ys = xs + num_blocks / 2 * block_len;
        Iterator xs_last = ys;
        Iterator ys_last = ys + num_blocks / 2 * block_len;

        Iterator x_key = imit;
        Iterator y_key = imit + num_blocks / 2;

        auto imit_cur = imit_space.begin();
        auto merged_cur = merged_space.begin();
        auto mid_val = *y_key;

        while (xs != xs_last || ys != ys_last) {
            if (ys == ys_last || (xs != xs_last && !comp(*ys, *xs))) {
                *imit_cur++ = *x_key++;
                std::copy(xs, xs + block_len, merged_cur);
                xs += block_len;
                merged_cur += block_len;
            } else {
                *imit_cur++ = *y_key++;
                std::copy(ys, ys + block_len, merged_cur);
                ys += block_len;
                merged_cur += block_len;
            }
        }

        std::copy(imit_space.begin(), imit_cur, imit);
        std::copy(merged_space.begin(), merged_cur, blocks);

        Iterator mid_key = imit;
        while (*mid_key != mid_val) {
            ++mid_key;
        }
        return mid_key;
    };

    std::vector<int> expected(ary_len);
    std::vector<int> ary(ary_len);
    SsizeT block_len = 3;
    auto rng = GetPerTestRNG();

    for (SsizeT num_blocks = 0; num_blocks <= 8; num_blocks += 2) {
        for (SsizeT pad = 0; pad < ary_len - (num_blocks + num_blocks * block_len); ++pad) {
            Iterator imit = ary.begin();
            Iterator blocks = imit + num_blocks + pad;

            std::fill(ary.begin(), ary.end(), 42);
            std::iota(imit, imit + num_blocks, 0);
            int a;
            auto gen = [&]() { return std::uniform_int_distribution<int>{a, a + 40}(rng); };
            a = rng() % 2 ? 70 : 90;
            std::generate(blocks, blocks + num_blocks / 2 * block_len, gen);
            std::sort(blocks, blocks + num_blocks / 2 * block_len, Compare{});
            a = rng() % 2 ? 70 : 90;
            std::generate(blocks + num_blocks / 2 * block_len, blocks + num_blocks * block_len, gen);
            std::sort(blocks + num_blocks / 2 * block_len, blocks + num_blocks * block_len, Compare{});

            std::copy(ary.begin(), ary.end(), expected.begin());
            Iterator mid_key_expected = naive_impl(imit, blocks, num_blocks, block_len, Compare{});
            std::swap_ranges(ary.begin(), ary.end(), expected.begin());
            Iterator mid_key = InterleaveBlocks(imit, blocks, num_blocks, block_len, IterComp{Compare{}, VoidProj{}});

            EXPECT_EQ(ary, expected);
            EXPECT_EQ(mid_key - imit, mid_key_expected - imit);
        }
    }
}

TEST(SayhiSortTest, DeinterleaveImitation) {
    SsizeT ary_len = 48;

    std::vector<int> expected(ary_len);

    std::vector<int> vals(ary_len);
    std::vector<int> ary(ary_len);
    auto rng = GetPerTestRNG();

    for (bool use_buf : {true, false}) {
        for (SsizeT imit_len = 0; imit_len + imit_len / 2 < ary_len; imit_len += 2) {
            SsizeT buf_len = imit_len / 2;
            Iterator imit = ary.begin();
            Iterator imit_last = imit + imit_len;
            Iterator buf = ary.end() - buf_len;

            std::iota(ary.begin(), ary.end(), 0);
            std::iota(vals.begin(), vals.begin() + imit_len, 0);
            std::shuffle(vals.begin(), vals.begin() + imit_len, rng);
            std::sort(vals.begin(), vals.begin() + imit_len / 2);
            std::sort(vals.begin() + imit_len / 2, vals.begin() + imit_len);
            std::sort(imit, imit_last, [&vals](int x, int y) { return vals[x] < vals[y]; });

            Iterator mid_key = imit;
            while (*mid_key != imit_len / 2) {
                ++mid_key;
            }
            std::fill(imit_last, buf, 100);
            if (use_buf) {
                DeinterleaveImitation(imit, imit_len, buf, mid_key, IterComp{Compare{}, VoidProj{}});
            }
            std::fill(buf, ary.end(), 200);
            if (!use_buf) {
                DeinterleaveImitation(imit, imit_len, mid_key, IterComp{Compare{}, VoidProj{}});
            }

            std::iota(expected.begin(), expected.begin() + imit_len, 0);
            std::fill(expected.begin() + imit_len, expected.end() - buf_len, 100);
            std::fill(expected.end() - buf_len, expected.end(), 200);

            EXPECT_EQ(ary, expected);
        }
    }
}

TEST(SayhiSortTest, MergeAdjacentBlocks) {
    BlockingParam<SsizeT> params[] = {
        {6, 6, 5, 4},
        {16, 17, 8, 8},
    };

    auto rng = GetPerTestRNG();

    for (auto p : params) {
        for (bool has_buf : {true, false}) {
            SsizeT imit_len = p.num_blocks - 2;
            SsizeT buf_len = p.block_len;
            SsizeT lseq_len = imit_len / 2 * p.block_len + p.first_block_len;
            SsizeT rseq_len = imit_len / 2 * p.block_len + p.last_block_len;
            SsizeT ary_len = imit_len + buf_len + lseq_len + rseq_len;
            std::vector<int> ary(ary_len);

            Iterator imit = ary.begin();
            Iterator buf = imit + imit_len;
            Iterator lseq = buf + buf_len;
            Iterator rseq = lseq + lseq_len;
            Iterator rseq_last = rseq + rseq_len;

            std::iota(imit, buf, 0);
            std::fill(buf, lseq, 0);
            std::iota(lseq, rseq_last, 100);
            std::shuffle(lseq, rseq_last, rng);
            std::sort(lseq, rseq, Compare{});
            std::sort(rseq, rseq_last, Compare{});

            Iterator mid_key = InterleaveBlocks(imit, lseq + p.first_block_len, p.num_blocks - 2, p.block_len,
                                                IterComp{Compare{}, VoidProj{}});
            if (has_buf) {
                MergeAdjacentBlocks<true>(imit, buf, lseq, p, mid_key, IterComp{Compare{}, VoidProj{}});
                EXPECT_EQ(buf, ary.end() - buf_len);
            } else {
                MergeAdjacentBlocks<false>(imit, buf, lseq, p, mid_key, IterComp{Compare{}, VoidProj{}});
                Rotate(buf, lseq, ary.end());
            }

            std::vector<int> expected(ary_len);
            std::copy(imit, imit + imit_len, expected.begin());
            std::iota(expected.begin() + imit_len, expected.end() - buf_len, 100);
            std::fill(expected.end() - buf_len, expected.end(), 0);

            EXPECT_EQ(ary, expected);
        }
    }
}

TEST(SayhiSortTest, MergeBlocking) {
    BlockingParam<SsizeT> params[] = {
        {6, 6, 5, 4},
        {16, 17, 8, 8},
        {24, 25, 1, 1},
    };

    auto rng = GetPerTestRNG();

    for (auto p : params) {
        for (bool has_buf : {true, false}) {
            SsizeT imit_len = p.num_blocks - 2;
            SsizeT buf_len = p.block_len;

            SsizeT lseq_len = imit_len / 2 * p.block_len + p.first_block_len;
            SsizeT rseq_len = imit_len / 2 * p.block_len + p.last_block_len;
            SsizeT ary_len = imit_len + buf_len + lseq_len + rseq_len;
            std::vector<int> ary(ary_len);

            Iterator imit = ary.begin();
            Iterator buf = imit + imit_len;
            Iterator lseq = buf + buf_len;
            Iterator rseq = lseq + lseq_len;
            Iterator rseq_last = rseq + rseq_len;

            std::iota(imit, buf, 0);
            std::fill(buf, lseq, 0);
            std::iota(lseq, rseq_last, 100);
            std::shuffle(lseq, rseq_last, rng);
            std::sort(lseq, rseq, Compare{});
            std::sort(rseq, rseq_last, Compare{});

            if (has_buf) {
                MergeBlocking<true>(imit, buf, lseq, p, IterComp{Compare{}, VoidProj{}});
                EXPECT_EQ(buf, ary.end() - buf_len);
            } else {
                MergeBlocking<false>(imit, buf, lseq, p, IterComp{Compare{}, VoidProj{}});
                Rotate(buf, lseq, ary.end());
            }

            std::vector<int> expected(ary_len);
            std::iota(expected.begin(), expected.begin() + imit_len, 0);
            std::iota(expected.begin() + imit_len, expected.end() - buf_len, 100);
            std::fill(expected.end() - buf_len, expected.end(), 0);

            EXPECT_EQ(ary, expected);
        }
    }
}

TEST(SayhiSortTest, MergeOneLevel) {
    BlockingParam<SsizeT> p{16, 19, 17, 17};
    SsizeT imit_len = 14;
    SsizeT buf_len = 19;
    SsizeT ary_len = imit_len + buf_len + 599;

    std::vector<int> ary(ary_len);
    std::vector<int> expected(ary_len);

    auto rng = GetPerTestRNG();

    auto test_fwd = [&](auto comp) {
        Iterator data = ary.begin() + imit_len + buf_len;
        for (int i = 0; i < imit_len; ++i) {
            ary[i] = i * 4;
        }
        std::fill(ary.begin() + imit_len, data, 42);
        std::iota(data, data + 599, 100);
        std::shuffle(data, data + 599, rng);
        std::stable_sort(data, data + 149, comp);
        std::stable_sort(data + 149, data + 299, comp);
        std::stable_sort(data + 299, data + 449, comp);
        std::stable_sort(data + 449, data + 599, comp);

        Iterator edata = expected.begin() + imit_len;
        for (int i = 0; i < imit_len; ++i) {
            expected[i] = i * 4;
        }
        std::copy(data, data + 599, edata);
        std::stable_sort(edata, edata + 299, comp);
        std::stable_sort(edata + 299, edata + 599, comp);
        std::fill(expected.end() - buf_len, expected.end(), 42);

        MergeOneLevel<true, true>(ary.begin(), ary.begin() + imit_len, data, 150, {SsizeT{599}, SsizeT{2}}, p,
                                  IterComp{comp, VoidProj{}});
        EXPECT_EQ(ary, expected);
    };

    auto test_bwd = [&](auto comp) {
        Iterator data = ary.begin() + imit_len;
        for (int i = 0; i < imit_len; ++i) {
            ary[i] = i * 4;
        }
        std::iota(data, data + 599, 100);
        std::shuffle(data, data + 599, rng);
        std::stable_sort(data, data + 149, comp);
        std::stable_sort(data + 149, data + 299, comp);
        std::stable_sort(data + 299, data + 449, comp);
        std::stable_sort(data + 449, data + 599, comp);
        std::fill(ary.end() - buf_len, ary.end(), 42);

        Iterator edata = expected.begin() + imit_len + buf_len;
        for (int i = 0; i < imit_len; ++i) {
            expected[i] = i * 4;
        }
        std::fill(expected.begin() + imit_len, edata, 42);
        std::copy(data, data + 599, edata);
        std::stable_sort(edata, edata + 299, comp);
        std::stable_sort(edata + 299, edata + 599, comp);

        MergeOneLevel<true, false>(ary.begin(), ary.end(), ary.end() - buf_len, 150, {SsizeT{599}, SsizeT{2}}, p,
                                   IterComp{comp, VoidProj{}});
        EXPECT_EQ(ary, expected);
    };

    test_fwd(Compare{});
    test_bwd(Compare{});
    test_fwd(CompareDiv4{});
    test_bwd(CompareDiv4{});
}

TEST(SayhiSortTest, Sort0To8) {
    std::vector<int> ary(8);
    std::vector<int> expected(8);
    std::iota(expected.begin(), expected.end(), 0);

    auto rng = GetPerTestRNG();

    for (int len = 0; len <= 8; ++len) {
        std::iota(ary.begin(), ary.end(), 0);
        std::shuffle(ary.begin(), ary.begin() + len, rng);
        Sort0To8(ary.begin(), len, IterComp{Compare{}, VoidProj{}});
        EXPECT_EQ(ary, expected) << len;
    }
}

TEST(SayhiSortTest, HeapSort) {
    auto rng = GetPerTestRNG();

    for (SsizeT sz : {5, 2024}) {
        std::vector<int> data(sz);
        std::iota(data.begin(), data.end(), 0);
        std::shuffle(data.begin(), data.end(), rng);
        HeapSort(data.begin(), sz, IterComp{Compare{}, VoidProj{}});

        std::vector<int> expected(sz);
        std::iota(expected.begin(), expected.end(), 0);
        EXPECT_EQ(data, expected);
    }
}

TEST(SayhiSortTest, CollectKeys) {
    SsizeT ary_len = 1000;

    std::vector<int> expected(ary_len);
    std::vector<int> dups_space(ary_len);

    auto naive_impl = [&](Iterator first, Iterator last, SsizeT num_desired_keys, Compare comp) {
        std::set<int, Compare> keys{comp};
        auto dups = dups_space.begin();

        while (first != last) {
            if (keys.find(*first) == keys.end()) {
                keys.insert(*first++);
                if (static_cast<SsizeT>(keys.size()) == num_desired_keys) {
                    break;
                }
            } else {
                *dups++ = *first++;
            }
        }

        auto copied = std::copy(keys.begin(), keys.end(), expected.begin());
        copied = std::copy(dups_space.begin(), dups, copied);
        std::copy(first, last, copied);
        return keys.size();
    };

    std::vector<int> ary(ary_len);
    SsizeT num_desired_keys = 10;
    auto rng = GetPerTestRNG();

    for (int k : {0, 1, 11}) {
        auto gen = [&]() { return std::uniform_int_distribution<int>{0, k}(rng); };
        std::generate(ary.begin(), ary.end(), gen);
        SsizeT expected_num_keys = naive_impl(ary.begin(), ary.end(), num_desired_keys, Compare{});
        SsizeT num_keys = CollectKeys(ary.begin(), ary.end(), num_desired_keys, IterComp{Compare{}, VoidProj{}});
        EXPECT_EQ(num_keys, expected_num_keys);
        EXPECT_EQ(ary, expected);
    }
}

TEST(SayhiSortTest, MergeSortControl) {
    MergeSortControl<SsizeT> ctrl{8, 16};

    EXPECT_EQ(ctrl.log2_num_seqs, 1);
    EXPECT_EQ(ctrl.imit_len, 2);
    EXPECT_EQ(ctrl.buf_len, 6);
    EXPECT_EQ(ctrl.bufferable_len, 12);
    EXPECT_EQ(ctrl.Next(), 6);
    EXPECT_EQ(ctrl.imit_len, 8);
    EXPECT_EQ(ctrl.buf_len, 0);
    EXPECT_EQ(ctrl.log2_num_seqs, 0);

    ctrl = {21, 123};
    EXPECT_EQ(ctrl.log2_num_seqs, 4);
    EXPECT_EQ(ctrl.imit_len, 8);
    EXPECT_EQ(ctrl.buf_len, 13);
    EXPECT_EQ(ctrl.seq_len, 8);
    EXPECT_TRUE(ctrl.forward);
    EXPECT_EQ(ctrl.Next(), 0);
    EXPECT_EQ(ctrl.log2_num_seqs, 3);
    EXPECT_EQ(ctrl.imit_len, 8);
    EXPECT_EQ(ctrl.buf_len, 13);
    EXPECT_EQ(ctrl.seq_len, 16);
    EXPECT_FALSE(ctrl.forward);

    ctrl = {22, 123};
    EXPECT_EQ(ctrl.imit_len, 10);
    EXPECT_EQ(ctrl.buf_len, 12);

    ctrl = {47, 953};
    EXPECT_EQ(ctrl.log2_num_seqs, 7);
    EXPECT_EQ(ctrl.imit_len, 22);
    EXPECT_EQ(ctrl.buf_len, 25);
    EXPECT_EQ(ctrl.seq_len, 8);
    EXPECT_EQ(ctrl.Next(), 0);
    EXPECT_EQ(ctrl.seq_len, 15);
    EXPECT_EQ(ctrl.Next(), 0);
    EXPECT_EQ(ctrl.seq_len, 30);
    EXPECT_EQ(ctrl.Next(), 0);
    EXPECT_EQ(ctrl.seq_len, 60);
    EXPECT_EQ(ctrl.Next(), 0);
    EXPECT_EQ(ctrl.seq_len, 120);
    EXPECT_EQ(ctrl.Next(), 0);
    EXPECT_EQ(ctrl.seq_len, 239);
    EXPECT_EQ(ctrl.Next(), 25);
    EXPECT_EQ(ctrl.seq_len, 477);
    EXPECT_EQ(ctrl.Next(), 0);
    EXPECT_EQ(ctrl.seq_len, 953);
}

TEST(SayhiSortTest, DetermineBlocking) {
    MergeSortControl<SsizeT> ctrl{47, 953};
    BlockingParam<SsizeT> p = DetermineBlocking(ctrl);
    EXPECT_EQ(p.num_blocks, 2);
    EXPECT_GE(p.first_block_len, 2);
    EXPECT_LE(p.first_block_len, p.block_len);
    ctrl.Next();
    p = DetermineBlocking(ctrl);
    EXPECT_EQ(p.num_blocks, 2);
    EXPECT_GE(p.first_block_len, 2);
    EXPECT_LE(p.first_block_len, p.block_len);
    ctrl.Next();
    p = DetermineBlocking(ctrl);
    EXPECT_EQ(p.num_blocks, 4);
    EXPECT_GE(p.first_block_len, 2);
    EXPECT_LE(p.first_block_len, p.block_len);
    ctrl.Next();
    p = DetermineBlocking(ctrl);
    EXPECT_EQ(p.num_blocks, 6);
    EXPECT_GE(p.first_block_len, 2);
    EXPECT_LE(p.first_block_len, p.block_len);
    ctrl.Next();
    p = DetermineBlocking(ctrl);
    EXPECT_EQ(p.num_blocks, 10);
    EXPECT_GE(p.first_block_len, 2);
    EXPECT_LE(p.first_block_len, p.block_len);
    ctrl.Next();
    p = DetermineBlocking(ctrl);
    EXPECT_EQ(p.num_blocks, 20);
    EXPECT_GE(p.first_block_len, 2);
    EXPECT_LE(p.first_block_len, p.block_len);
    ctrl.Next();
    p = DetermineBlocking(ctrl);
    EXPECT_EQ(p.num_blocks, 30);
    EXPECT_GE(p.first_block_len, 2);
    EXPECT_LE(p.first_block_len, p.block_len);
}

TEST(SayhiSortTest, Sort) {
    SsizeT ary_len = 1024;
    std::vector<int> ary(ary_len);
    std::vector<int> expected(ary_len);

    auto rng = GetPerTestRNG();

    for (SsizeT i = 0; i < ary_len; ++i) {
        std::iota(ary.begin(), ary.begin() + i, 0);
        std::fill(ary.begin() + i, ary.end(), static_cast<int>(ary_len));
        std::shuffle(ary.begin(), ary.begin() + i, rng);
        std::copy(ary.begin(), ary.end(), expected.begin());
        sayhisort::sort(ary.begin(), ary.begin() + i, Compare{});
        std::stable_sort(expected.begin(), expected.begin() + i, Compare{});
        EXPECT_EQ(ary, expected) << i;

        std::shuffle(ary.begin(), ary.begin() + i, rng);
        std::copy(ary.begin(), ary.end(), expected.begin());
        sayhisort::sort(ary.begin(), ary.begin() + i, CompareDiv4{});
        std::stable_sort(expected.begin(), expected.begin() + i, CompareDiv4{});
        EXPECT_EQ(ary, expected) << i;
    }
}

TEST(SayhiSortTest, SortAPI) {
    SsizeT ary_len = 100;
    std::vector<int> ary(ary_len);
    std::vector<int> expected(ary_len);

    auto rng = GetPerTestRNG();

    std::iota(ary.begin(), ary.end(), 0);
    std::shuffle(ary.begin(), ary.end(), rng);
    std::copy(ary.begin(), ary.end(), expected.begin());
    sayhisort::sort(ary.begin(), ary.end());
    std::stable_sort(expected.begin(), expected.end());
    EXPECT_EQ(ary, expected);
}

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
struct CheckedInt {
    CheckedInt() : CheckedInt{0} {}

    CheckedInt(int64_t v) {
        CheckedInt::CheckRange(v);
        val = static_cast<int32_t>(v);
    }

    explicit operator int() const { return static_cast<int>(val); }
    explicit operator ptrdiff_t() const { return static_cast<ptrdiff_t>(val); }
    explicit operator bool() const { return !!val; }

    friend CheckedInt& operator+=(CheckedInt& lhs, int rhs) {
        CheckIntRange(rhs);
        int64_t v = int64_t{lhs.val} + int64_t{rhs};
        CheckedInt::CheckRange(v);
        lhs.val = static_cast<int32_t>(v);
        return lhs;
    }

    friend CheckedInt& operator-=(CheckedInt& lhs, int rhs) {
        CheckIntRange(rhs);
        int64_t v = int64_t{lhs.val} - int64_t{rhs};
        CheckedInt::CheckRange(v);
        lhs.val = static_cast<int32_t>(v);
        return lhs;
    }

    friend CheckedInt& operator*=(CheckedInt& lhs, int rhs) {
        CheckIntRange(rhs);
        int64_t v = int64_t{lhs.val} * int64_t{rhs};
        CheckedInt::CheckRange(v);
        lhs.val = static_cast<int32_t>(v);
        return lhs;
    }

    friend CheckedInt& operator/=(CheckedInt& lhs, int rhs) {
        if (rhs <= 0) {
            CheckedInt::err = true;
        }
        lhs.val /= rhs;
        return lhs;
    }

    friend CheckedInt& operator%=(CheckedInt& lhs, int rhs) {
        if (rhs <= 0) {
            CheckedInt::err = true;
        }
        lhs.val %= rhs;
        return lhs;
    }

    friend CheckedInt& operator&=(CheckedInt& lhs, int rhs) {
        lhs.val &= rhs;
        CheckedInt::CheckRange(lhs.val);
        return lhs;
    }

    friend CheckedInt& operator|=(CheckedInt& lhs, int rhs) {
        lhs.val |= rhs;
        CheckedInt::CheckRange(lhs.val);
        return lhs;
    }

    friend CheckedInt& operator^=(CheckedInt& lhs, int rhs) {
        lhs.val ^= rhs;
        CheckedInt::CheckRange(lhs.val);
        return lhs;
    }

    friend CheckedInt& operator>>=(CheckedInt& lhs, int rhs) {
        if (!(0 <= rhs && rhs < 32)) {
            CheckedInt::err = true;
        }
        lhs.val >>= rhs;
        return lhs;
    }

    friend CheckedInt& operator<<=(CheckedInt& lhs, int rhs) {
        if (!(0 <= rhs && rhs < 32)) {
            CheckedInt::err = true;
        }
        int64_t v = int64_t{lhs.val} << rhs;
        CheckedInt::CheckRange(v);
        lhs.val = static_cast<int32_t>(v);
        return lhs;
    }

    friend CheckedInt operator-(int lhs, CheckedInt rhs) {
        CheckIntRange(lhs);
        int64_t v = int64_t{lhs} - int64_t{rhs.val};
        return CheckedInt{v};
    }

    friend CheckedInt operator/(int lhs, CheckedInt rhs) {
        CheckIntRange(lhs);
        if (rhs.val <= 0) {
            CheckedInt::err = true;
        }
        int v = lhs / rhs.val;
        return CheckedInt{v};
    }

    friend CheckedInt operator%(int lhs, CheckedInt rhs) {
        CheckIntRange(lhs);
        if (rhs.val <= 0) {
            CheckedInt::err = true;
        }
        int v = lhs % rhs.val;
        return CheckedInt{v};
    }

    friend CheckedInt operator>>(int lhs, CheckedInt rhs) {
        CheckIntRange(lhs);
        if (!(0 <= rhs.val && rhs.val < 16)) {
            CheckedInt::err = true;
        }
        int v = lhs >> rhs.val;
        return CheckedInt{v};
    }

    friend CheckedInt operator<<(int lhs, CheckedInt rhs) {
        CheckIntRange(lhs);
        if (!(0 <= rhs.val && rhs.val < 16)) {
            CheckedInt::err = true;
        }
        int64_t v = int64_t{lhs} << rhs.val;
        return CheckedInt{v};
    }

    friend CheckedInt operator+(CheckedInt lhs, int rhs) { return (lhs += rhs); }
    friend CheckedInt operator-(CheckedInt lhs, int rhs) { return (lhs -= rhs); }
    friend CheckedInt operator*(CheckedInt lhs, int rhs) { return (lhs *= rhs); }
    friend CheckedInt operator/(CheckedInt lhs, int rhs) { return (lhs /= rhs); }
    friend CheckedInt operator%(CheckedInt lhs, int rhs) { return (lhs %= rhs); }
    friend CheckedInt operator&(CheckedInt lhs, int rhs) { return (lhs &= rhs); }
    friend CheckedInt operator|(CheckedInt lhs, int rhs) { return (lhs |= rhs); }
    friend CheckedInt operator^(CheckedInt lhs, int rhs) { return (lhs ^= rhs); }
    friend CheckedInt operator>>(CheckedInt lhs, int rhs) { return (lhs >>= rhs); }
    friend CheckedInt operator<<(CheckedInt lhs, int rhs) { return (lhs <<= rhs); }

    friend CheckedInt operator+(int lhs, CheckedInt rhs) { return rhs + lhs; }
    friend CheckedInt operator*(int lhs, CheckedInt rhs) { return rhs * lhs; }
    friend CheckedInt operator&(int lhs, CheckedInt rhs) { return rhs & lhs; }
    friend CheckedInt operator|(int lhs, CheckedInt rhs) { return rhs | lhs; }
    friend CheckedInt operator^(int lhs, CheckedInt rhs) { return rhs ^ lhs; }

    friend CheckedInt& operator+=(CheckedInt& lhs, CheckedInt rhs) { return (lhs += rhs.val); }
    friend CheckedInt& operator-=(CheckedInt& lhs, CheckedInt rhs) { return (lhs -= rhs.val); }
    friend CheckedInt& operator*=(CheckedInt& lhs, CheckedInt rhs) { return (lhs *= rhs.val); }
    friend CheckedInt& operator/=(CheckedInt& lhs, CheckedInt rhs) { return (lhs /= rhs.val); }
    friend CheckedInt& operator%=(CheckedInt& lhs, CheckedInt rhs) { return (lhs %= rhs.val); }
    friend CheckedInt& operator&=(CheckedInt& lhs, CheckedInt rhs) { return (lhs &= rhs.val); }
    friend CheckedInt& operator|=(CheckedInt& lhs, CheckedInt rhs) { return (lhs |= rhs.val); }
    friend CheckedInt& operator^=(CheckedInt& lhs, CheckedInt rhs) { return (lhs ^= rhs.val); }
    friend CheckedInt& operator>>=(CheckedInt& lhs, CheckedInt rhs) { return (lhs >>= rhs.val); }
    friend CheckedInt& operator<<=(CheckedInt& lhs, CheckedInt rhs) { return (lhs <<= rhs.val); }

    friend CheckedInt operator+(CheckedInt lhs, CheckedInt rhs) { return (lhs += rhs); }
    friend CheckedInt operator-(CheckedInt lhs, CheckedInt rhs) { return (lhs -= rhs); }
    friend CheckedInt operator*(CheckedInt lhs, CheckedInt rhs) { return (lhs *= rhs); }
    friend CheckedInt operator/(CheckedInt lhs, CheckedInt rhs) { return (lhs /= rhs); }
    friend CheckedInt operator%(CheckedInt lhs, CheckedInt rhs) { return (lhs %= rhs); }
    friend CheckedInt operator&(CheckedInt lhs, CheckedInt rhs) { return (lhs &= rhs); }
    friend CheckedInt operator|(CheckedInt lhs, CheckedInt rhs) { return (lhs |= rhs); }
    friend CheckedInt operator^(CheckedInt lhs, CheckedInt rhs) { return (lhs ^= rhs); }
    friend CheckedInt operator>>(CheckedInt lhs, CheckedInt rhs) { return (lhs >>= rhs); }
    friend CheckedInt operator<<(CheckedInt lhs, CheckedInt rhs) { return (lhs <<= rhs); }

    friend CheckedInt& operator++(CheckedInt& obj) { return (obj += 1); }
    friend CheckedInt operator++(CheckedInt& obj, int) {
        CheckedInt old = obj;
        ++obj;
        return old;
    }

    friend CheckedInt& operator--(CheckedInt& obj) { return (obj -= 1); }
    friend CheckedInt operator--(CheckedInt& obj, int) {
        CheckedInt old = obj;
        --obj;
        return old;
    }

    friend CheckedInt operator-(CheckedInt obj) {
        obj.val = -obj.val;
        return obj;
    }

    // Allow the form `a & ~b`, where `~b` is out of range
    friend int operator~(CheckedInt obj) { return ~obj.val; }

    friend bool operator==(CheckedInt lhs, CheckedInt rhs) { return lhs.val == rhs.val; }
    friend bool operator!=(CheckedInt lhs, CheckedInt rhs) { return lhs.val != rhs.val; }
    friend bool operator<(CheckedInt lhs, CheckedInt rhs) { return lhs.val < rhs.val; }
    friend bool operator<=(CheckedInt lhs, CheckedInt rhs) { return lhs.val <= rhs.val; }
    friend bool operator>(CheckedInt lhs, CheckedInt rhs) { return lhs.val > rhs.val; }
    friend bool operator>=(CheckedInt lhs, CheckedInt rhs) { return lhs.val >= rhs.val; }

    friend bool operator==(CheckedInt lhs, int rhs) { return lhs.val == rhs; }
    friend bool operator!=(CheckedInt lhs, int rhs) { return lhs.val != rhs; }
    friend bool operator<(CheckedInt lhs, int rhs) { return lhs.val < rhs; }
    friend bool operator<=(CheckedInt lhs, int rhs) { return lhs.val <= rhs; }
    friend bool operator>(CheckedInt lhs, int rhs) { return lhs.val > rhs; }
    friend bool operator>=(CheckedInt lhs, int rhs) { return lhs.val >= rhs; }

    friend bool operator==(int lhs, CheckedInt rhs) { return lhs == rhs.val; }
    friend bool operator!=(int lhs, CheckedInt rhs) { return lhs != rhs.val; }
    friend bool operator<(int lhs, CheckedInt rhs) { return lhs < rhs.val; }
    friend bool operator<=(int lhs, CheckedInt rhs) { return lhs <= rhs.val; }
    friend bool operator>(int lhs, CheckedInt rhs) { return lhs > rhs.val; }
    friend bool operator>=(int lhs, CheckedInt rhs) { return lhs >= rhs.val; }

    static inline thread_local int32_t max = std::numeric_limits<int32_t>::max();
    static inline thread_local bool err = false;

private:
    static void CheckRange(int64_t v) {
        if (!(-CheckedInt::max <= v && v <= CheckedInt::max)) {
            CheckedInt::err = true;
        }
    }

    static void CheckIntRange(int v) {
        if (!(-65536 <= v && v <= 65535)) {
            CheckedInt::err = true;
        }
    }

    int32_t val;
};

template <typename T>
struct CheckedIterator {
    CheckedIterator(T* p) : ptr{p} {}

    using difference_type = CheckedInt;
    using value_type = T;
    using pointer = T*;
    using reference = T&;
    using iterator_category = std::random_access_iterator_tag;

    friend CheckedIterator& operator+=(CheckedIterator& it, CheckedInt d) {
        it.ptr += static_cast<ptrdiff_t>(d);
        return it;
    }

    friend CheckedIterator& operator-=(CheckedIterator& it, CheckedInt d) {
        it.ptr -= static_cast<ptrdiff_t>(d);
        return it;
    }

    friend CheckedIterator operator+(CheckedIterator it, CheckedInt d) { return (it += d); }
    friend CheckedIterator operator+(CheckedInt d, CheckedIterator it) { return (it += d); }
    friend CheckedIterator operator-(CheckedIterator it, CheckedInt d) { return (it -= d); }
    friend CheckedIterator operator-(CheckedInt d, CheckedIterator it) { return (it -= d); }

    friend CheckedIterator& operator++(CheckedIterator& it) { return (it += 1); }
    friend CheckedIterator operator++(CheckedIterator& it, int) {
        CheckedIterator old = it;
        ++it;
        return old;
    }

    friend CheckedIterator& operator--(CheckedIterator& it) { return (it -= 1); }
    friend CheckedIterator operator--(CheckedIterator& it, int) {
        CheckedIterator old = it;
        --it;
        return old;
    }

    friend CheckedInt operator-(CheckedIterator lhs, CheckedIterator rhs) {
        int64_t d = lhs.ptr - rhs.ptr;
        return CheckedInt{d};
    }

    T& operator*() const { return *ptr; }
    T& operator[](CheckedInt i) const { return *(*this + i); }

    friend bool operator==(CheckedIterator lhs, CheckedIterator rhs) { return lhs.ptr == rhs.ptr; }
    friend bool operator!=(CheckedIterator lhs, CheckedIterator rhs) { return lhs.ptr != rhs.ptr; }
    friend bool operator<(CheckedIterator lhs, CheckedIterator rhs) { return lhs.ptr < rhs.ptr; }
    friend bool operator<=(CheckedIterator lhs, CheckedIterator rhs) { return lhs.ptr <= rhs.ptr; }
    friend bool operator>(CheckedIterator lhs, CheckedIterator rhs) { return lhs.ptr > rhs.ptr; }
    friend bool operator>=(CheckedIterator lhs, CheckedIterator rhs) { return lhs.ptr >= rhs.ptr; }

private:
    T* ptr;
};
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

TEST(SayhiSortTest, NoOverflow) {
    int32_t ary_len = 128;
    std::vector<int> ary(ary_len);

    auto rng = GetPerTestRNG();

    for (int32_t i = 0; i < ary_len; ++i) {
        std::iota(ary.begin(), ary.begin() + i, 0);
        std::fill(ary.begin() + i, ary.end(), static_cast<int>(ary_len));
        std::shuffle(ary.begin(), ary.begin() + i, rng);
        CheckedInt::max = std::max<int32_t>(i, 16);
        sayhisort::sort(CheckedIterator{ary.data()}, CheckedIterator{ary.data() + i});
        EXPECT_FALSE(CheckedInt::err) << i;
        CheckedInt::err = false;
    }
}

}  // namespace

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
