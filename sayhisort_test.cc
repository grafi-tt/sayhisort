#include "sayhisort.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <numeric>
#include <random>
#include <set>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

namespace {

using namespace sayhisort::detail;

using Iterator = std::vector<int>::iterator;
using Compare = std::less<int>;
using SsizeT = Iterator::difference_type;

std::mt19937_64 GetPerTestRNG() {
    uint64_t h = 0xcbf29ce484222325;
    auto fnv1a = [&h](const char* m) {
        while (*m) {
            h ^= *m++;
            h *= 0x00000100000001b3;
        }
    };

    int seed = testing::UnitTest::GetInstance()->random_seed();

    std::array<char, sizeof(int) * 2 + 2> seed_hex;
    if (char* p = std::to_chars(seed_hex.begin(), seed_hex.end(), seed, 16).ptr; p > seed_hex.end() - 2) {
        // should be unreachable, but just nul-terminate for safety
        seed_hex[0] = '\0';
    } else {
        p[0] = '/';
        p[1] = '\0';
    }
    fnv1a(seed_hex.begin());

    const auto* test_info = testing::UnitTest::GetInstance()->current_test_info();
    const char* suite_name = test_info->test_suite_name();
    const char* test_name = test_info->name();
    fnv1a(suite_name);
    fnv1a("::");
    fnv1a(test_name);

    return std::mt19937_64{h};
}

TEST(SayhiSortTest, OverApproxSqrt) {
    EXPECT_EQ(OverApproxSqrt(16), 4);

    for (int x = 17; x < 1000; ++x) {
        SsizeT r = OverApproxSqrt(x);
        EXPECT_GT(r, 0);
        EXPECT_LE(r, x);
        EXPECT_GE(r * r, x);
    }

    for (int x = 2048; x <= 8192; ++x) {
        int r = OverApproxSqrt(x);
        int q = 0.97 * r;
        EXPECT_LT(q * q, x);
    }
}

TEST(SayhiSortTest, Rotate) {
    std::vector<int> data(32);
    std::vector<int> expected(32);

    for (int l : {30, 31, 32}) {
        for (int i = 0; i <= l; ++i) {
            std::iota(data.begin(), data.begin() + l, 0);
            Rotate(data.begin(), data.begin() + i, data.begin() + l);
            std::iota(expected.begin() + l - i, expected.begin() + l, 0);
            std::iota(expected.begin(), expected.begin() + l - i, i);
            EXPECT_EQ(data, expected);
        }
    }
}

TEST(SayhiSortTest, SwapChunk) {
    std::vector<int> data = {0, 1, 2, 3, 4, 5, 6, 7};
    std::vector<int> expected = {4, 5, 6, 3, 0, 1, 2, 7};

    SwapChunk(data.begin(), data.begin() + 4, 3);
    EXPECT_EQ(data, expected);
}

TEST(SayhiSortTest, BinarySearch) {
    std::vector<int> data(17);
    std::iota(data.begin(), data.end() - 1, 0);
    for (int i = 1; i <= 16; ++i) {
        for (int j = -1; j <= i; ++j) {
            data[16] = j;
            auto it = BinarySearch<false>(data.begin(), data.begin() + i, data.begin() + 16, Compare{});
            SsizeT idx = it - data.begin();
            EXPECT_EQ(idx, std::max(0, j));
            it = BinarySearch<true>(data.begin(), data.begin() + i, data.begin() + 16, Compare{});
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
        SsizeT ys_offset = len - num_rest;

        std::copy(merged_space.begin(), merged_space.begin() + num_merged, expected.begin());
        std::fill(expected.begin() + num_merged, expected.begin() + ys_offset, 0);
        std::copy(rest_space.begin(), rest_space.begin() + num_rest, expected.begin() + ys_offset);
        std::fill(expected.begin() + len, expected.end(), 42);

        Iterator new_buf = buf + num_merged;
        Iterator new_ys = buf + ys_offset;
        return std::tuple{new_buf, new_ys, xs_consumed};
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

            auto [buf_expected, ys_expected, xs_consumed_expected] = naive_impl(buf, xs, ys, ys_last, Compare{});
            bool xs_consumed = MergeWithBuf<false>(buf, xs, ys, ys_last, Compare{});

            EXPECT_EQ(ary, expected) << "xs_len=" << xs_len << " ys_len=" << ys_len;
            EXPECT_EQ(buf, buf_expected);
            EXPECT_EQ(ys, ys_expected);
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

        Iterator new_ys = xs_orig + num_merged;
        return std::tuple{new_ys, xs_consumed};
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

            auto [ys_expected, xs_consumed_expected] = naive_impl(xs, ys, ys_last, Compare{});
            bool xs_consumed = MergeWithoutBuf<false>(xs, ys, ys_last, Compare{});

            EXPECT_EQ(ary, expected) << "xs_len=" << xs_len << " ys_len=" << ys_len;
            EXPECT_EQ(ys - ary.begin(), ys_expected - ary.begin());
            EXPECT_EQ(xs_consumed, xs_consumed_expected);
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
            Iterator mid_key = InterleaveBlocks(imit, blocks, num_blocks, block_len, Compare{});

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
        for (SsizeT imit_len = 2; imit_len + imit_len / 2 < ary_len; imit_len += 2) {
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
                DeinterleaveImitation(imit, imit_len, buf, mid_key, Compare{});
            }
            std::fill(buf, ary.end(), 200);
            if (!use_buf) {
                DeinterleaveImitation(imit, imit_len, mid_key, Compare{});
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

            Iterator mid_key = InterleaveBlocks(imit, lseq + p.first_block_len, p.num_blocks - 2, p.block_len, Compare{});
            if (has_buf) {
                MergeAdjacentBlocks<true>(imit, buf, lseq, p, mid_key, Compare{});
                EXPECT_EQ(buf, ary.end() - buf_len);
            } else {
                MergeAdjacentBlocks<false>(imit, buf, lseq, p, mid_key, Compare{});
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
                MergeBlocking<true>(imit, buf, lseq, p, Compare{});
                EXPECT_EQ(buf, ary.end() - buf_len);
            } else {
                MergeBlocking<false>(imit, buf, lseq, p, Compare{});
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
        SsizeT num_keys = CollectKeys(ary.begin(), ary.end(), num_desired_keys, Compare{});
        EXPECT_EQ(num_keys, expected_num_keys);
        EXPECT_EQ(ary, expected);
    }
}

TEST(SayhiSortTest, ReverseCompare) {
    ReverseCompare gt_ebo{std::less<int>{}};
    static_assert(std::is_empty_v<decltype(gt_ebo)>);
    EXPECT_FALSE(gt_ebo(1, 2));
    EXPECT_FALSE(gt_ebo(2, 2));
    EXPECT_TRUE(gt_ebo(3, 2));

    std::function lessobj = [](int x, int y) { return x < y; };
    ReverseCompare gt_noebo{lessobj};
    static_assert(!std::is_empty_v<decltype(gt_noebo)>);
    EXPECT_FALSE(gt_ebo(1, 2));
    EXPECT_FALSE(gt_ebo(2, 2));
    EXPECT_TRUE(gt_ebo(3, 2));
}

TEST(SayhiSortTest, Sort4To8) {
    std::vector<int> ary(8);
    std::vector<int> expected(8);
    std::iota(expected.begin(), expected.end(), 0);

    auto rng = GetPerTestRNG();

    for (int len = 4; len <= 8; ++len) {
        std::iota(ary.begin(), ary.end(), 0);
        std::shuffle(ary.begin(), ary.begin() + len, rng);
        Sort4To8(ary.begin(), len, Compare{});
        EXPECT_EQ(ary, expected);
    }
}

TEST(SayhiSortTest, Sort0To16) {
    std::vector<int> ary(16);
    std::vector<int> expected(16);
    std::iota(expected.begin(), expected.end(), 0);

    auto rng = GetPerTestRNG();

    for (int len = 0; len <= 16; ++len) {
        std::iota(ary.begin(), ary.end(), 0);
        std::shuffle(ary.begin(), ary.begin() + len, rng);
        Sort0To16(ary.begin(), len, Compare{});
        EXPECT_EQ(ary, expected) << len;
    }
}

TEST(SayhiSortTest, FirstShellSortGap) {
    SsizeT n;
    SsizeT gap;

    for (SsizeT len = 2; len < 1600; ++len) {
        SsizeT n_ans;
        if (len <= 4) {
            n_ans = 0;
        } else if (len <= 10) {
            n_ans = 1;
        } else if (len <= 23) {
            n_ans = 2;
        } else if (len <= 57) {
            n_ans = 3;
        } else if (len <= 132) {
            n_ans = 4;
        } else if (len <= 301) {
            n_ans = 5;
        } else if (len <= 701) {
            n_ans = 6;
        } else if (len <= 1577) {
            n_ans = 7;
        } else {
            n_ans = 8;
        }
        gap = FirstShellSortGap(len, n);
        EXPECT_EQ(n, n_ans);
        EXPECT_EQ(gap, n_ans == 8 ? 1577 : kCiuraGaps[n]);
    }

    gap = FirstShellSortGap(SsizeT{3548}, n);
    EXPECT_EQ(n, 8);
    EXPECT_EQ(gap, 1577);

    gap = FirstShellSortGap(SsizeT{3549}, n);
    EXPECT_EQ(n, 9);
    EXPECT_EQ(gap, 3548);

    gap = FirstShellSortGap(SsizeT{7983}, n);
    EXPECT_EQ(n, 9);
    EXPECT_EQ(gap, 3548);

    gap = FirstShellSortGap(SsizeT{7984}, n);
    EXPECT_EQ(n, 10);
    EXPECT_EQ(gap, 7983);
}

TEST(SayhiSortTest, NthShellSortGap) {
    EXPECT_EQ(NthShellSortGap(0), 1);
    EXPECT_EQ(NthShellSortGap(1), 4);
    EXPECT_EQ(NthShellSortGap(2), 10);
    EXPECT_EQ(NthShellSortGap(3), 23);
    EXPECT_EQ(NthShellSortGap(4), 57);
    EXPECT_EQ(NthShellSortGap(5), 132);
    EXPECT_EQ(NthShellSortGap(6), 301);
    EXPECT_EQ(NthShellSortGap(7), 701);
    EXPECT_EQ(NthShellSortGap(8), 1577);
    EXPECT_EQ(NthShellSortGap(9), 3548);
    EXPECT_EQ(NthShellSortGap(10), 7983);
}

TEST(SayhiSortTest, ShellSort) {
    auto rng = GetPerTestRNG();

    for (SsizeT sz : {5, 2024}) {
        std::vector<int> data(sz);
        std::iota(data.begin(), data.end(), 0);
        std::shuffle(data.begin(), data.end(), rng);
        ShellSort(data.begin(), sz, Compare{});

        std::vector<int> expected(sz);
        std::iota(expected.begin(), expected.end(), 0);
        EXPECT_EQ(data, expected);
    }
}

}  // namespace

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}