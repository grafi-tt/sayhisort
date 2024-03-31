#pragma once

#include <iterator>
#include <type_traits>
#include <utility>

namespace sayhisort {

namespace detail {
namespace {

using std::swap;

//
// Utilities
//

/**
 * @brief Compute an over-approximation of sqrt(x)
 *
 * @param x
 *   @pre x >= 8
 * @returns r
 *   @post sqrt(x) <= r
 *   @post r < max(sqrt(x) + 2, sqrt(x) * (1.0 + 1.0/256))  (numerically tested)
 */
template <typename SsizeT>
constexpr SsizeT OverApproxSqrt(SsizeT x) {
    // https://en.wikipedia.org/wiki/Methods_of_computing_square_roots#Binary_estimates
    // Find a number `n`, so that `x` can be represented as `x = a * 2^{2n}` where `a` in [0.5, 2.0).
    SsizeT n = 1;
    for (SsizeT p = x; p >= 8; p /= 4) {
        ++n;
    }
    // `r = ceil((0.5 + 0.5 * a) * 2^n)`, which is an over-approx of `sqrt(x)`
    SsizeT r = ((x - 1) >> (n + 1)) + (1 << (n - 1)) + 1;

    // Apply Newton's method (also known as Heron's method) and take ceil.
    // https://en.wikipedia.org/wiki/Methods_of_computing_square_roots#Heron's_method
    // If `r` is an over-aprrox, the method computes a refined over-approx.
    return (r + (x - 1) / r) / 2 + 1;
}

/**
 * @brief Rotate two chunks split at `middle`
 *
 * @param first
 * @param middle
 *   @pre first <= middle
 * @param last
 *   @pre middle <= last
 */
template <typename Iterator>
constexpr void Rotate(Iterator first, Iterator middle, Iterator last) {
    // Helix Rotation
    // description available: https://github.com/scandum/rotate#helix-rotation
    using SsizeT = typename Iterator::difference_type;
    SsizeT l_len = middle - first;
    SsizeT r_len = last - middle;

    if (!l_len || !r_len) {
        return;
    }

    while (true) {
        if (l_len <= r_len) {
#if 0
            SsizeT rem = r_len % l_len;
#else
            SsizeT rem = r_len & (l_len - 1);
            if (l_len & (l_len - 1)) {
                rem = r_len % l_len;
            }
#endif
            do {
                swap(*first++, *middle++);
            } while (--r_len);
            if (!rem) {
                return;
            }
            first = last - l_len;
            middle = last - rem;
            l_len -= rem;
            r_len = rem;
        } else {
            SsizeT rem = l_len & (r_len - 1);
            if (r_len & (r_len - 1)) {
                rem = l_len % r_len;
            }
            do {
                swap(*--middle, *--last);
            } while (--l_len);
            if (!rem) {
                return;
            }
            last = first + r_len;
            middle = first + rem;
            r_len -= rem;
            l_len = rem;
        }
    }
}

/**
 * @brief Swap two chunks with the same length.
 *
 * If the two chunks are same, it does nothing. Two chunks cannot partially overlap.
 *
 * @param xs
 * @param ys
 *   @pre [xs, xs + len) and [ys, ys + len) don't overlap, expect the case xs = ys
 * @param len
 *   @pre len is positive
 */
template <typename Iterator, typename SsizeT = typename Iterator::difference_type>
constexpr void SwapChunk(Iterator xs, Iterator ys, SsizeT len) {
    if (xs == ys) {
        return;
    }
    while (len--) {
        swap(*xs++, *ys++);
    }
}

/**
 * @brief Search key from sorted sequence.
 *
 * @param first
 *   @pre first < last
 * @param last
 * @return pos
 *   @post If nonstrict=false: for any x in [first, last), comp(*x, *key) iff x < pos
 *   @post If nonstrict=true:  for any x in [first, last), !comp(*key, *x) iff x < pos
 */
template <bool nonstrict, typename Iterator, typename Compare>
constexpr Iterator BinarySearch(Iterator first, Iterator last, Iterator key, Compare comp) {
    // So-called monobound binary search
    // The algorithm statically determines how many times the loop body runs, so that CPU pipeline becomes happier
    // See https://github.com/scandum/binary_search for idea
    auto pred = [&comp, &key](Iterator p) {
        if constexpr(nonstrict) {
            return !comp(*key, *p);
        } else {
            return comp(*p, *key);
        }
    };

    using SsizeT = typename Iterator::difference_type;
    Iterator base = first;
    SsizeT len = last - first;
    SsizeT mid;

    while ((mid = len / 2)) {
        Iterator pivot = base + mid;
        if (pred(pivot)) {
            base = pivot;
        }
        len -= mid;
    } while (mid);

    base += pred(base);
    return base;
}

//
// Basic merge routines
//

/**
 * @brief Merge sequences xs and ys into the buffer before xs. The buffer moves while merging.
 *
 * The sequence ys cannot be longer the the length of the buffer.
 *
 * Illustration:
 *
 *   [ buffer     | left            | right    ]
 *  buf           xs                ys       ys_last
 *   ->
 *   [ merged                | buffer   | rest ]
 *                           buf        ys   ys_last
 *
 * The area `rest` contains unmerged subsequence from either `xs` or `ys`.
 *
 * @param buf
 *   @pre buf < xs
 *   @post pre_buf < buf, and [pre_buf, buf) contains merged eleemnts, where pre_buf is buf before merging.
 *   @post ys - buf = buf_len, whwre buf_len is defined before mergin by xs - buf.
 * @param xs
 *   @pre xs < ys
 * @param ys
 *   @pre ys < ys_last
 *   @pre ys_last - ys <= xs - buf
 *   @post ys < ys_last
 * @param ys_last
 * @param comp
 * @note The implementation actually works if xs or ys is empty, but the behaviour shall not be depended on.
 */
template <bool flipped, typename Iterator, typename Compare>
constexpr bool MergeWithBuf(Iterator& buf, Iterator xs, Iterator& ys, Iterator ys_last, Compare comp) {
    // So-called cross merge optimization is applied
    // See https://github.com/scandum/quadsort#cross-merge for idea
    auto is_x_selected = [&comp](decltype(xs[0]) x, decltype(ys[0]) y) {
        if constexpr (flipped) {
            return comp(x, y);
        } else {
            return !comp(y, x);
        }
    };

    Iterator xs_last = ys;

#if 0
    while (xs < xs_last && ys < ys_last) {
        if (is_x_selected(xs[0], ys[0])) {
            swap(*buf++, *xs++);
        } else {
            swap(*buf++, *ys++);
        }
    }
    bool xs_consumed = xs == xs_last;
#else
    while (xs < xs_last - 1 && ys < ys_last - 1) {
        if (is_x_selected(xs[1], ys[0])) {
            swap(*buf++, *xs++);
            swap(*buf++, *xs++);
        } else if (!is_x_selected(xs[0], ys[1])) {
            swap(*buf++, *ys++);
            swap(*buf++, *ys++);
        } else {
            bool y_pos = is_x_selected(xs[0], ys[0]);
            swap(buf[!y_pos], *xs++);
            swap(buf[y_pos], *ys++);
            buf += 2;
        }
    }

    bool xs_consumed = xs == xs_last;

    if (xs == xs_last - 1) {
        xs_consumed = false;
        do {
            if (is_x_selected(xs[0], ys[0])) {
                swap(*buf++, *xs++);
                xs_consumed = true;
                break;
            }
            swap(*buf++, *ys++);
        } while (ys < ys_last);

    } else if (ys == ys_last - 1) {
        xs_consumed = true;
        do {
            if (!is_x_selected(xs[0], ys[0])) {
                swap(*buf++, *ys++);
                xs_consumed = false;
                break;
            }
            swap(*buf++, *xs++);
        } while (xs < xs_last);
    }
#endif

    // Case: xs == xs_last
    //    [ merged | buffer | buffer | right ]
    //            buf       xs       ys    ys_last
    if (xs_consumed) {
        return true;
    }

    // Case: ys == ys_last
    //    [ merged | buffer | left  | buffer ]
    //            buf       xs   xs_last     ys
    // -> After repeatedly applying swaps:
    //    [ merged | buffer | buffer | left  ]
    //            buf       xs       ys    ys_last
    do {
        swap(*--ys, *--xs_last);
    } while (xs_last != xs);
    return false;
}

/**
 * @brief Merge sequences xs and ys in-place.
 *
 * @param xs
 *   @pre xs < ys
 * @param ys
 *   @pre ys < ys_last
 *   @post ys < ys_last
 * @param ys_last
 * @param comp
 * @note For better performance, `xs` should be not much longer than `ys`.
 *       The time complexity is `O((m + log(n)) * min(m, n, j, k) + m + n)`, where
 *       `m` and `n` are the lengthes of `xs` and `ys`, whereas `j` and `k` are
 *       the numbers of unique keys in `xs` and `ys`.
 */
template <bool flipped, typename Iterator, typename Compare>
constexpr bool MergeWithoutBuf(Iterator xs, Iterator& ys, Iterator ys_last, Compare comp) {
    while (true) {
        // Seek xs so that xs[0] > ys[0]
        xs = BinarySearch<!flipped>(xs, ys, ys, comp);
        if (xs == ys) return true;
        // Insert xs to ys
        Iterator ys_upper = ys + 1;
        if (ys_upper != ys_last) {
            ys_upper = BinarySearch<flipped>(ys_upper, ys_last, xs, comp);
        }
        Rotate(xs, ys, ys_upper);
        xs += ys_upper - ys;
        ys = ys_upper;
        if (ys_upper == ys_last) {
            ys = xs;
            return false;
        }
    }
}

//
// Block merge subroutines
//

/**
 * @brief Interleave blocks from two sorted sequences, so that blocks are sorted by their first elements.
 *
 * @param imit
 * @param blocks
 *   @pre [imit, imit + num_blocks) and [blocks, bloks + num_blocks * block_len) are non-overlapping
 * @param num_blocks
 *   @pre num_blocks is non-negative
 * @param block_len
 *   @pre block_len is positive
 * @param comp
 */
template <typename Iterator, typename Compare, typename SsizeT = typename Iterator::difference_type>
constexpr Iterator InterleaveBlocks(Iterator imit, Iterator blocks, SsizeT num_blocks, SsizeT block_len, Compare comp) {
    // Algorithm similar to wikisort's block movement
    // https://github.com/BonzaiThePenguin/WikiSort/blob/master/Chapter%203.%20In-Place.md
    //
    // While interleaving, the state of blocks is like:
    //   [interleaved | left_permuted | right]
    // We pick the least block `least_left` from `left_permuted` by linear search.
    // Then we compare `least_left` with `right[0]`, and swap the selected block for
    // `left_permuted[0]`.
    Iterator left_keys = imit;
    Iterator right_keys = imit + num_blocks / 2;
    Iterator left_blocks = blocks;
    Iterator right_blocks = left_blocks + num_blocks / 2 * block_len;

    Iterator least_left_key = left_keys;
    Iterator least_left_block = left_blocks;

    Iterator least_right_key = right_keys;
    Iterator last_right_key = right_keys + num_blocks / 2;

    while (left_keys < right_keys) {
        if (right_keys == last_right_key || !comp(*right_blocks, *least_left_block)) {
            swap(*left_keys, *least_left_key);
            SwapChunk(left_blocks, least_left_block, block_len);

            ++left_keys;
            left_blocks += block_len;

            least_left_key = left_keys;
            least_left_block = left_blocks;
            if (right_keys != least_right_key) {  // skip searching if left keys aren't permuted
                for (Iterator key = left_keys + 1; key < right_keys; ++key) {
                    if (comp(*key, *least_left_key)) {
                        least_left_key = key;
                    }
                }
                least_left_block = left_blocks + (least_left_key - left_keys) * block_len;
            }

        } else {
            swap(*left_keys, *right_keys);
            SwapChunk(left_blocks, right_blocks, block_len);

            if (left_keys == least_left_key) {
                least_left_key = right_keys;
                least_left_block = right_blocks;
            }
            if (right_keys == least_right_key) {
                least_right_key = left_keys;
            }
            ++left_keys;
            ++right_keys;
            left_blocks += block_len;
            right_blocks += block_len;
        }
    }

    return least_right_key;
}

/**
 * @brief Sort interleaved keys in imitation buffer, using another auxiliary buffer.
 *
 * @param imit
 * @param imit_len
 *   @pre imit_len is a multiple of 2
 * @param buf
 *   @pre [imit, imit + imit_len) and [buf, buf + imit_len / 2) are non-overlapping
 * @param mid_key
 * @param comp
 */
template <typename Iterator, typename Compare, typename SsizeT = typename Iterator::difference_type>
constexpr void DeinterleaveImitation(Iterator imit, SsizeT imit_len, Iterator buf, Iterator mid_key, Compare comp) {
    // Bin-sort like algorithm based on partitioning.
    // Same algorithm founds in HolyGrailsort.
    // https://github.com/HolyGrailSortProject/Holy-Grailsort/blob/ccfcc4315c6ccafbca5f6a51886710898a06c8a1/Holy%20Grail%20Sort/Java/Summer%20Dragonfly%20et%20al.'s%20Rough%20Draft/src/holygrail/HolyGrailSort.java#L1328-L1330
    swap(*mid_key, *buf);
    Iterator left_cur = mid_key;
    Iterator right_cur = buf + 1;
    Iterator cur = mid_key + 1;
    mid_key = buf;

    while (cur != imit + imit_len) {
        if (comp(*cur, *mid_key)) {
            swap(*cur++, *left_cur++);
        } else {
            swap(*cur++, *right_cur++);
        }
    }

    // now `left_cur` points to the start of right keys
    SwapChunk(buf, left_cur, imit_len / 2);
}

/**
 * @brief Sort interleaved keys in imitation buffer, in-place.
 *
 * @param imit
 * @param imit_len
 *   @pre imit_len is a multiple of 2
 * @param mid_key
 * @param comp
 */
template <typename Iterator, typename Compare, typename SsizeT = typename Iterator::difference_type>
constexpr void DeinterleaveImitation(Iterator imit, SsizeT imit_len, Iterator mid_key, Compare comp) {
    // We colour each key by whether they are from left or right.
    // Then we can see the imitation buffer as a sequence of runs with alternating colour.
    //
    // In one iteration, the algorithm rotate every pair of (right, left)-runs.
    // When no such a pair is found, all keys are properly sorted.
    // Each iteration halves the number of pairs, So the alogirithm works in O(N log N), where
    // N is size of `imit`.
    //
    // The idea to rotate pairs of runs of is borrowed from HolyGrailsort's algorithm.
    // https://github.com/HolyGrailSortProject/Holy-Grailsort/blob/ccfcc4315c6ccafbca5f6a51886710898a06c8a1/Holy%20Grail%20Sort/Java/Summer%20Dragonfly%20et%20al.'s%20Rough%20Draft/src/holygrail/HolyGrailSort.java#L1373-L1376
    SsizeT runlengths[2] = {};
    SsizeT& l_runlength = runlengths[0];
    SsizeT& r_runlength = runlengths[1];

    bool rotated{};
    auto rotate_runs = [&](Iterator cur) {
        if (!r_runlength) {
            l_runlength = 0;
            return;
        }
        Iterator l_run = cur - l_runlength;
        Iterator r_run = l_run - r_runlength;
        Rotate(r_run, l_run, cur);
        if (!rotated) {
            mid_key = cur - r_runlength;
        }
        l_runlength = 0;
        r_runlength = 0;
        rotated = true;
    };

    do {
        l_runlength = 0;
        r_runlength = 0;
        rotated = false;

        bool was_left = false;
        Iterator cur = imit;
        do {
            bool is_right = !comp(*cur, *mid_key);
            if (was_left && is_right) {
                rotate_runs(cur);
            }
            ++runlengths[is_right];
            was_left = !is_right;
        } while (++cur != imit + imit_len);

        if (was_left) {
            rotate_runs(cur);
        }
    } while (rotated);
}

//
// Full block merge
//

template <typename SsizeT>
struct BlockingParam {
    SsizeT num_blocks;
    SsizeT block_len;
    SsizeT first_block_len;
    SsizeT last_block_len;
};

template <bool has_buf, typename Iterator, typename Compare, typename SsizeT = typename Iterator::difference_type>
constexpr void MergeAdjacentBlocks(Iterator imit, Iterator& buf, Iterator blocks, BlockingParam<SsizeT> p,
                                   Iterator mid_key, Compare comp) {
    SsizeT num_remained_blocks = p.num_blocks;

    enum BlockOrigin {
        kLeft,
        kRight,
    };

    Iterator xs = blocks;
    Iterator xs_latest_block = xs;
    BlockOrigin xs_origin = kLeft;
    --num_remained_blocks;

    Iterator cur = xs + p.first_block_len;

    do {
        Iterator cur_last = cur + (--num_remained_blocks ? p.block_len : p.last_block_len);
        BlockOrigin cur_origin = (num_remained_blocks && comp(*imit++, *mid_key)) ? kLeft : kRight;

        if (cur_origin == xs_origin) {
            xs_latest_block = cur;
            cur = cur_last;
            continue;
        }

        // Optimization to safely skip continuing blocks those have the same origin.
        // In particular, it's crucial when `has_buf = false`. In this case, time complexity isn't assured
        // without the optimization, due to implementation detail of `MergeWithoutBuf`.
        if (xs != xs_latest_block) {
            if constexpr (has_buf) {
                if (num_remained_blocks) {
                    do {
                        swap(*buf++, *xs++);
                    } while (xs != xs_latest_block);
                }
            } else {
                if (num_remained_blocks) {
                    xs = xs_latest_block;
                } else if (cur - xs > p.last_block_len) {
                    // Ensure that length of xs is at most block_len
                    Rotate(xs, cur, cur_last);
                    cur = xs + p.last_block_len;
                    xs_origin = kRight;
                }
            }
        }

        bool xs_consumed{};
        if constexpr (has_buf) {
            if (xs_origin == kLeft) {
                xs_consumed = MergeWithBuf<false>(buf, xs, cur, cur_last, comp);
            } else {
                xs_consumed = MergeWithBuf<true>(buf, xs, cur, cur_last, comp);
            }
        } else {
            if (xs_origin == kLeft) {
                xs_consumed = MergeWithoutBuf<false>(xs, cur, cur_last, comp);
            } else {
                xs_consumed = MergeWithoutBuf<true>(xs, cur, cur_last, comp);
            }
        }

        xs = cur;
        xs_latest_block = xs;
        xs_origin = static_cast<BlockOrigin>(xs_origin ^ xs_consumed);

        cur = cur_last;
    } while (num_remained_blocks);

    if constexpr (has_buf) {
        Rotate(buf, xs, cur);
        buf += cur - xs;
    }
}

template <bool has_buf, typename Iterator, typename Compare, typename SsizeT = typename Iterator::difference_type>
constexpr void MergeBlocking(Iterator imit, Iterator& buf, Iterator blocks, BlockingParam<SsizeT> p, Compare comp) {
    // Skip interleaving the first block and the last one, those may have shorter length.
    SsizeT imit_len = p.num_blocks - 2;
    Iterator mid_key = InterleaveBlocks(imit, blocks + p.first_block_len, imit_len, p.block_len, comp);

    MergeAdjacentBlocks<has_buf>(imit, buf, blocks, p, mid_key, comp);
    if (!imit_len) {
        return;
    }

    if constexpr (has_buf) {
        DeinterleaveImitation(imit, imit_len, buf, mid_key, comp);
    } else {
        DeinterleaveImitation(imit, imit_len, mid_key, comp);
    }
}

//
// Bottom-up merge sort logics
//

template <typename Compare, bool = std::is_empty_v<Compare> && !std::is_final_v<Compare>>
struct ReverseCompare : Compare {
    constexpr ReverseCompare(Compare) {}

    template <typename T>
    constexpr bool operator()(T&& lhs, T&& rhs) {
        return Compare{}(std::forward<T>(rhs), std::forward<T>(lhs));
    }
};

template <typename Compare>
struct ReverseCompare<Compare, false> {
    constexpr ReverseCompare(Compare comp) : comp_{comp} {}

    template <typename T>
    constexpr bool operator()(T&& lhs, T&& rhs) {
        return comp_(std::forward(rhs), std::forward(lhs));
    }

private:
    Compare comp_;
};

template <bool forward, typename SsizeT>
struct SequenceIterator;

template <typename SsizeT>
struct SequenceIterator<true, SsizeT> {
    constexpr SequenceIterator(SsizeT data_len, SsizeT log2_num_seqs) :
        log2_num_seqs{log2_num_seqs},
        num_rest{1 << log2_num_seqs},
        seq_len{((data_len - 1) / num_rest) + 1},
        remainder{(data_len - 1) % num_rest + 1},
        frac_counter{0} {}

    constexpr std::pair<SsizeT, bool> Next() {
        frac_counter += remainder;
        bool no_carry = !(frac_counter & (1 << log2_num_seqs));
        frac_counter &= ~(1 << log2_num_seqs);
        --num_rest;
        return {seq_len - no_carry, no_carry};
    }

    constexpr SsizeT IsEnd() const { return !num_rest; }

    SsizeT log2_num_seqs;
    SsizeT num_rest;
    SsizeT seq_len;
    SsizeT remainder;
    SsizeT frac_counter;
};

template <typename SsizeT>
struct SequenceIterator<false, SsizeT> {
    constexpr SequenceIterator(SsizeT data_len, SsizeT log2_num_seqs) :
        log2_num_seqs{log2_num_seqs},
        num_rest{1 << log2_num_seqs},
        seq_len{((data_len - 1) / num_rest) + 1},
        remainder{num_rest - ((data_len - 1) % num_rest + 1)},
        frac_counter{0} {}

    constexpr std::pair<SsizeT, bool> Next() {
        frac_counter += remainder;
        bool no_borrow = !!(frac_counter & (1 << log2_num_seqs));
        frac_counter &= ~(1 << log2_num_seqs);
        --num_rest;
        return {seq_len - no_borrow, no_borrow};
    }

    constexpr SsizeT IsEnd() const { return !num_rest; }

    SsizeT log2_num_seqs;
    SsizeT num_rest;
    SsizeT seq_len;
    SsizeT remainder;
    SsizeT frac_counter;
};

template <bool has_buf, bool forward, typename Iterator, typename Compare,
          typename SsizeT = typename Iterator::difference_type>
constexpr void MergeOneLevel(Iterator imit, Iterator buf, Iterator data, SsizeT data_len, SsizeT log2_num_seqs,
                             BlockingParam<SsizeT> p, Compare comp) {
    SsizeT residual_len = p.first_block_len;
    SequenceIterator<forward, SsizeT> seq_iter{data_len, log2_num_seqs};
    do {
        auto [lseq_len, lseq_decr] = seq_iter.Next();
        auto [rseq_len, rseq_decr] = seq_iter.Next();
        SsizeT merging_len = lseq_len + rseq_len;
        p.first_block_len = residual_len - lseq_decr;
        p.last_block_len = residual_len - rseq_decr;

        if constexpr (!forward) {
            auto rev_imit = std::make_reverse_iterator(imit + p.num_blocks - 2);
            auto rev_buf = std::make_reverse_iterator(buf);
            auto rev_data = std::make_reverse_iterator(data);
            MergeBlocking<has_buf>(rev_imit, rev_buf, rev_data, p, ReverseCompare{comp});
            buf = rev_buf.base();
            data -= merging_len;
        } else {
            MergeBlocking<has_buf>(imit, buf, data, p, comp);
            data += merging_len;
        }
    } while (!seq_iter.IsEnd());
}

//
// Small array sorting
//

/**
 * @brief Sort data by odd-even sort algorithm. Sorting is stable.
 *
 * It's suitable for small data. Data length is passed as a template argument for generating specialized code.
 *
 * @tparam len
 *   @pre len >= 0
 * @param data
 * @param comp
 */
template <int len, typename Iterator, typename Compare>
constexpr void OddEvenSort(Iterator data, Compare comp) {
    using SsizeT = typename Iterator::difference_type;

    for (SsizeT i = 0; i < len; i += 2) {
        for (SsizeT j = 0; j < len - 1; j += 2) {
            if (comp(data[j + 1], data[j])) {
                swap(data[j + 1], data[j]);
            }
        }
        if (i + 1 == len) {
            break;
        }

        for (SsizeT j = 1; j < len - 1; j += 2) {
            if (comp(data[j + 1], data[j])) {
                swap(data[j + 1], data[j]);
            }
        }
    }
}

/**
 * @brief Sort data with its length 4 to 8. Sorting is stable.
 *
 * @param data
 * @param len
 *   @pre 4 <= len <= 8
 * @param comp
 */
template <typename Iterator, typename Compare, typename SsizeT = typename Iterator::difference_type>
constexpr void Sort4To8(Iterator data, SsizeT len, Compare comp) {
    switch (len) {
    case 4:
        return OddEvenSort<4>(data, comp);
    case 5:
        return OddEvenSort<5>(data, comp);
    case 6:
        return OddEvenSort<6>(data, comp);
    case 7:
        return OddEvenSort<7>(data, comp);
    case 8:
        return OddEvenSort<8>(data, comp);
    default:
#ifdef __GNUC__
        __builtin_unreachable();
#endif
        return;
    }
}

template <typename Iterator, typename Compare, typename SsizeT = typename Iterator::difference_type>
constexpr void SortLeaves(Iterator data, SsizeT data_len, SsizeT log2_num_seqs, Compare comp) {
    SequenceIterator<true, SsizeT> seq_iter{data_len, log2_num_seqs};
    do {
        SsizeT seq_len = seq_iter.Next().first;
        Sort4To8(data, seq_len, comp);
        data += seq_len;
    } while (!seq_iter.IsEnd());
}

/**
 * @brief Sort data with its length 0 to 16. Sorting is stable.
 *
 * @param data
 * @param len
 *   @pre 0 <= len <= 8
 * @param comp
 */
template <typename Iterator, typename Compare, typename SsizeT = typename Iterator::difference_type>
constexpr void Sort0To8(Iterator data, SsizeT len, Compare comp) {
    if (len <= 1) {
        return;
    }
    if (len <= 3) {
        if (comp(data[1], data[0])) {
            swap(data[1], data[0]);
        }
        if (len == 2) {
            return;
        }
        if (comp(data[2], data[1])) {
            swap(data[2], data[1]);
        }
        if (comp(data[1], data[0])) {
            swap(data[1], data[0]);
        }
        return;
    }
    return SortLeaves(data, len, SsizeT{0}, comp);
}

//
// Full sorting subroutines
//

template <typename Iterator, typename Compare, typename SsizeT = typename Iterator::difference_type>
constexpr SsizeT CollectKeys(Iterator first, Iterator last, SsizeT num_desired_keys, Compare comp) {
    if (first == last) {
        return 0;
    }
    if (num_desired_keys <= 1) {
        return num_desired_keys;
    }

    Iterator keys = first;
    Iterator keys_last = first + 1;

    for (Iterator cur = keys_last; cur < last; ++cur) {
        Iterator inspos = BinarySearch<false>(keys, keys_last, cur, comp);
        if (inspos == keys_last || comp(*cur, *inspos)) {
            // Rotate keys forward so that insertion works in O(num_keys)
            Rotate(keys, keys_last, cur);
            keys += cur - keys_last;
            inspos += cur - keys_last;
            keys_last = cur;
            // Insert the new key
            Rotate(inspos, keys_last, cur + 1);
            if (++keys_last - keys == num_desired_keys) {
                break;
            }
        }
    }

    Rotate(first, keys, keys_last);
    return keys_last - keys;
}

constexpr int kCiuraGaps[8] = {1, 4, 10, 23, 57, 132, 301, 701};

/**
 * @brief Find the larget gap within `len` from Ciura's gap sequence.
 *
 * @param len
 *   @pre len >= 2
 * @return gap
 *   @post 1 <= gap < len
 * @return n
 *   @post n >= 0
 *   @post NthShellSortGap(n) == gap
 */
template <typename SsizeT>
constexpr std::pair<SsizeT, SsizeT> FirstShellSortGap(SsizeT len) {
    SsizeT n = 4 * (kCiuraGaps[4] < len);
    n += 2 * (kCiuraGaps[n + 2] < len);
    n += kCiuraGaps[n + 1] < len;
    SsizeT gap = kCiuraGaps[n];
    if (n == 7) {
        // floor(2.25 * gap) < len
        while (gap < (len - gap / 4 + 1) / 2) {
            gap = gap * 2 + gap / 4;
            ++n;
        }
    }
    return {gap, n};
}

/**
 * @brief Find the n'th gap within `len` from Ciura's gap sequence.
 *
 * @param n
 *   @pre n >= 0
 * @returns gap
 *   @post gap >= 1
 */
template <typename SsizeT>
constexpr SsizeT NthShellSortGap(SsizeT n) {
    if (n < 8) {
        return kCiuraGaps[n];
    }
    SsizeT i = 7;
    SsizeT gap = kCiuraGaps[i];
    do {
        gap = gap * 2 + gap / 4;
    } while (++i < n);
    return gap;
}

/**
 * @brief Sort data by Shell sorting with Ciura's gap sequence. Sorting is unstable.
 *
 * The sequence is extended by repeatedly applying `x \mapsto ceil(2.25 * x)` to the last pre-computed element 701.
 *
 * @param data
 * @param len
 *   @pre len >= 2
 * @param comp
 */
template <typename Iterator, typename Compare, typename SsizeT = typename Iterator::difference_type>
constexpr void ShellSort(Iterator data, SsizeT len, Compare comp) {
    auto [gap, n] = FirstShellSortGap(len);

    while (true) {
        SsizeT i = gap;
        do {
            for (SsizeT j = i; j >= gap; j -= gap) {
                if (comp(data[j - gap], data[j])) {
                    break;
                }
                swap(data[j - gap], data[j]);
            }
        } while (++i < len);
        if (!n) {
            break;
        }
        gap = NthShellSortGap(--n);
    }
}

//
// Full sorting
//

template <typename SsizeT>
struct MergeSortControl {
    /**
     * @param num_keys
     *   @pre num_keys == 0 or num_keys >= 8
     * @param data_len
     *   @pre data_len > 8
     * @post 5 < seq_spec.seq_len <= 8
     */
    constexpr MergeSortControl(SsizeT num_keys, SsizeT data_len) : data_len{data_len} {
        if (num_keys) {
            // imit_len >= 2
            imit_len = (num_keys + 2) / 4 * 2 - 2;
            // buf_len >= 6
            buf_len = num_keys - imit_len;
            // bufferable_len >= 12
            bufferable_len = (imit_len + 2) / 2 * buf_len;
        }

        while ((data_len - 1) >> (log2_num_seqs + 3)) {
            ++log2_num_seqs;
        }
        // seq_len <= 8, so seq_len < bufferable_len holds if num_keys != 0
        seq_len = ((data_len - 1) >> log2_num_seqs) + 1;
    }

    constexpr SsizeT Next() {
        --log2_num_seqs;
        seq_len = ((data_len - 1) >> log2_num_seqs) + 1;

        if (!buf_len) {
            return 0;
        }
        forward = !forward;

        if (!log2_num_seqs || seq_len > bufferable_len) {
            SsizeT old_buf_len = buf_len;
            imit_len += buf_len;
            buf_len = 0;
            return old_buf_len;
        }
        return 0;
    }

    /**
     * imit_len is positive and multiple of 2.
     * imit_len + buf_len = num_keys.
     */
    SsizeT imit_len = 0;
    /**
     * buf_len is non-negative.
     * If buf_len > 0
     *   - seq_spec.seq_len <= bufferable_len
     *   - imit_len + 2 <= buf_len
     */
    //! buf_len is non-negative, and  if buf_len > 0
    SsizeT buf_len = 0;
    //! bufferable_len = ((imit_len + 2) / 2) * buf_len
    SsizeT bufferable_len = 0;
    //! data_len > 8
    SsizeT data_len;

    SsizeT log2_num_seqs = 1;
    SsizeT seq_len{};
    bool forward = true;
};

template <typename SsizeT>
constexpr BlockingParam<SsizeT> DetermineBlocking(const MergeSortControl<SsizeT>& ctrl) {
    SsizeT num_blocks = ctrl.imit_len + 2;
    SsizeT seq_len = ctrl.seq_len;

    if (ctrl.buf_len) {
        // We don't need to perform runtime-checking so that computed `num_blocks` fits in imitation buffer.
        // Since
        //   seq_len <= (imit_len + 2) / 2 * buf_len
        // is assured,
        //   ceil(seq_len / buf_len) * 2 <= imit_len + 2
        // always holds.
        num_blocks = ((seq_len - 1) / ctrl.buf_len + 1) * 2;
    } else {
        // max_num_blocks must be a multple of 2 and under-approx of sqrt(2 * seq_len)
        SsizeT max_num_blocks = seq_len < 12 ? 2 : seq_len / OverApproxSqrt(seq_len * 2) * 2;
        if (num_blocks > max_num_blocks) {
            num_blocks = max_num_blocks;
        }
    }

    // We need to proof `residual_len >= 2` to assure that all blocks have positive length.
    // (Note that `residual_len` may be decremented once in `MergeOneLevel`.)
    //
    // We first show the following lemma. Let `N` and `m` be positive integers, such that
    // `N >= m ** 2`, `N >= 2` and `m >= 1`.
    //
    //   (lemma):  N - ceil(N / m) * (m - 1) >= 2 .
    //
    // There are three cases.
    //
    //   (1):  m = 1 .
    //   (2):  N = m ** 2, and m >= 2 .
    //   (3):  N >= (m ** 2) + 1, and m >= 2 .
    //
    // If case (1) and (2), it's trivial to check that (lemma) holds.
    // For case (3), we can use the following property:
    //
    //   ceil(N / m) <= (N / m) + ((m - 1) / m) .
    //
    // By multiplying `m - 1` to left and right hand sides, we have
    //
    //   ceil(N / m) * (m - 1) <= N + (m - 1) - ceil(N / m) .
    //
    // Since `N >= (m ** 2) + 1` from (3), `ceil(N / m) >= m + 1` holds. Therefore
    //
    //   ceil(N / m) * (m - 1) <= N + (m - 1) - (m + 1)
    //                         =  N - 2 ,
    //
    // which is equivalent to (lemma). Thus the lemma is proven.
    //
    // Now it's enough to show that
    //
    //   (proposition):  seq_len >= (num_blocks / 2) ** 2 ,
    //   (*):            seq_len >= 2 , and
    //   (**):           num_blocks / 2 >= 1 .
    //
    // Note that (*) and (**) are immediate from the function's requirement.
    //
    // When `buf_len = 0`, the definition of `max_num_blocks` ensures that (proposition) is satisfied.
    // For the case `buf_len > 0`, we use the following constraints.
    //
    //   (a):  imit_len + 2 <= buf_len .                   (by MergeSortControll)
    //   (b):  seq_len <= (imit_len + 2) / 2 * buf_len .   (by MergeSortControll)
    //   (c):  num_blocks / 2 = ceil(seq_len / buf_len) .  (by definition)
    //
    // From (a) and (b), we have `seq_len <= (buf_len ** 2) / 2`. Therefore
    //
    //   (d):  buf_len >= sqrt(2) * sqrt(seq_len)
    //
    // holds. Using (c) and (d), we have
    //
    //   (e):  num_blocks / 2 <= ceil(sqrt(seq_len) / sqrt(2)) .
    //                        <= (sqrt(seq_len) / sqrt(2)) + 1
    //
    // Thanks to (e), it's easy to show that the following (subprop) is enough to prove (proposition).
    //
    //   (subprop):  sqrt(seq_len) >= (sqrt(seq_len) / sqrt(2)) + 1
    //
    // As the function requires `seq_len >= 8`, (subprop) is always satisfied. Thus (proposition) is true.
    // Therefore We have proven `residual_len >= 2`.

    SsizeT block_len = (seq_len - 1) / (num_blocks / 2) + 1;
    SsizeT residual_len = seq_len - block_len * (num_blocks / 2 - 1);

    return {num_blocks, block_len, residual_len, residual_len};
}

template <typename Iterator, typename Compare>
static constexpr void Sort(Iterator first, Iterator last, Compare comp) {
    using SsizeT = typename Iterator::difference_type;

    SsizeT len = last - first;
    if (len <= 8) {
        return Sort0To8(first, len, comp);
    }

    SsizeT num_keys = 0;
    if (len > 16) {
        SsizeT num_desired_keys = 2 * OverApproxSqrt(len) - 2;
        num_keys = CollectKeys(first, last, num_desired_keys, comp);
        if (num_keys < 8) {
            first += num_keys;
            len -= num_keys;
            num_keys = 0;
        }
    }

    // If len = 17, num_keys is at most 8; so data_len > 8
    SsizeT data_len = len - num_keys;
    MergeSortControl ctrl{num_keys, data_len};

    Iterator data = first + num_keys;
    SortLeaves(data, ctrl.data_len, ctrl.log2_num_seqs, comp);

    do {
        BlockingParam p = DetermineBlocking(ctrl);

        if (!ctrl.buf_len) {
            MergeOneLevel<false, true>(first, first + ctrl.imit_len, data, ctrl.data_len, ctrl.log2_num_seqs, p, comp);
        } else if (ctrl.forward) {
            MergeOneLevel<true, true>(first, first + ctrl.imit_len, data, ctrl.data_len, ctrl.log2_num_seqs, p, comp);
        } else {
            MergeOneLevel<true, false>(first, last, last - ctrl.buf_len, ctrl.data_len, ctrl.log2_num_seqs, p, comp);
        }

        if (SsizeT old_buf_len = ctrl.Next()) {
            Iterator buf = data - old_buf_len;
            if (!ctrl.forward) {
                Iterator back_buf = last;
                Iterator back_data = last - old_buf_len;
                do {
                    swap(*--back_data, *--back_buf);
                } while (back_data != buf);
                ctrl.forward = true;
            }
            ShellSort(buf, old_buf_len, comp);
            MergeWithoutBuf<false>(first, buf, data, comp);
        }
    } while (ctrl.log2_num_seqs);

    if (first != data) {
        MergeWithoutBuf<false>(first, data, last, comp);
    }
}

}  // namespace
}  // namespace detail

template <typename RandomAccessIterator, typename Compare>
constexpr void sort(RandomAccessIterator first, RandomAccessIterator last, Compare comp) {
    return detail::Sort(first, last, comp);
}

}  // namespace sayhisort
