#ifndef SAYHISORT_H
#define SAYHISORT_H

// just for  __cpp_lib_constexpr_algorithms macro
#include <algorithm>

#if __cpp_lib_constexpr_algorithms >= 201806L
#define SAYHISORT_CONSTEXPR_SWAP constexpr
#else
#define SAYHISORT_CONSTEXPR_SWAP
#endif

#include <functional>
#include <iterator>
#include <type_traits>
#include <utility>

#ifndef SAYHISORT_PERF_TRACE
#define SAYHISORT_PERF_TRACE(...)
#define SAYHISORT_H_PERF_TRACE_STUB
#endif

namespace sayhisort {

namespace detail {
namespace {

template <typename Iterator>
using diff_t = typename std::iterator_traits<Iterator>::difference_type;

using std::swap;

//
// Utilities
//

/**
 * @brief Compute an over-approximation of sqrt(x)
 *
 * @param x
 *   @pre x >= 8
 * @return r
 *   @post sqrt(x) <= r < x / 2
 *   @post r = 3, if x = 8
 *   @post r = 4, if 9 <= x <= 16 (exhaustively checked)
 *   @post r < sqrt(x) * 1.25, if x > 16 (exhaustively checked for x < 28, and mathematically shown for x >= 28)
 */
template <typename SsizeT>
constexpr SsizeT OverApproxSqrt(SsizeT x) {
    // https://en.wikipedia.org/wiki/Methods_of_computing_square_roots#Binary_estimates
    // Find a number `n`, so that `x` can be represented as `x = a * 2^{2n}` where `a` in [0.5, 2.0).
    SsizeT n = 1;
    for (SsizeT p = x; p >= 8; p /= 4) {
        ++n;
    }

    // `r0 = ceil((0.5 + 0.5 * a) * 2^n) = 2^{n-1} + ceil(x * 2^{-(n+1)})`, which is an over-approx of `sqrt(x)` .
    //
    // We can bound `r0` using the well-known formula:
    //
    //   sqrt(x) <= (0.5 + 0.5 * a) * 2^n <= (1.5/sqrt(2)) * sqrt(x) .
    //
    // The bound of `r0` is
    //
    //   r0 <= (0.5 + 0.5 * a) * 2^n + 1
    //      <= (1.5 / sqrt(2)) * sqrt(x) + 1
    //      = (1.5 / sqrt(2) + 1 / sqrt(x)) * sqrt(x) .
    //
    // For `x >= 28`, it's easy to check that `r0 < 1.25 * sqrt(x)`.
    SsizeT r0 = (SsizeT{1} << (n - 1)) + ((x - 1) >> (n + 1)) + 1;

    // Apply Newton's method (also known as Heron's method) and take ceil.
    // https://en.wikipedia.org/wiki/Methods_of_computing_square_roots#Heron's_method
    // As `r0` is an over-aprrox, the method returns refiend over-approx value `r`,
    // which satisifies `sqrt(x) <= r <= r0`.
    return (r0 + (x - 1) / r0) / 2 + 1;
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
SAYHISORT_CONSTEXPR_SWAP void Rotate(Iterator first, Iterator middle, Iterator last) {
    diff_t<Iterator> l_len = middle - first;
    diff_t<Iterator> r_len = last - middle;
    diff_t<Iterator> len = l_len + r_len;

    if (!l_len || !r_len) {
        return;
    }

    // Helix Rotation
    // description available: https://github.com/scandum/rotate#helix-rotation
    while (len > 64) {
        if (l_len <= r_len) {
            diff_t<Iterator> rem = r_len % l_len;
            do {
                swap(*first++, *middle++);
            } while (middle != last);
            if (!rem) {
                return;
            }
            middle = last - rem;
            len = l_len;
            l_len -= rem;
            r_len = rem;
        } else {
            diff_t<Iterator> rem = l_len % r_len;
            do {
                swap(*--last, *--middle);
            } while (middle != first);
            if (!rem) {
                return;
            }
            middle = first + rem;
            len = r_len;
            r_len -= rem;
            l_len = rem;
        }
    }

    // Triple reversal for small data to avoid integer division
    Iterator f = first;
    Iterator m = middle;
    Iterator l = last;
    while (f < --m) {
        swap(*f++, *m);
    }
    while (middle < --l) {
        swap(*middle++, *l);
    }
    while (first < --last) {
        swap(*first++, *last);
    }
}

/**
 * @brief Search key from sorted sequence.
 *
 * @param first
 *   @pre first < last
 * @param last
 * @return pos
 *   @post If strict=true:  for any x in [first, last),  comp(*x, *key) iff x < pos
 *   @post If strict=false: for any x in [first, last), !comp(*key, *x) iff x < pos
 */
template <bool strict, typename Iterator, typename Compare>
constexpr Iterator BinarySearch(Iterator first, Iterator last, Iterator key, Compare comp) {
    SAYHISORT_PERF_TRACE("BinarySearch");
    // So-called monobound binary search
    // The algorithm statically determines how many times the loop body runs, so that CPU pipeline becomes happier
    // See https://github.com/scandum/binary_search for idea
    auto pred = [&comp, &key](Iterator p) {
        if constexpr (strict) {
            return comp(*p, *key);
        } else {
            return !comp(*key, *p);
        }
    };

    Iterator base = first;
    diff_t<Iterator> len = last - first + 1;
    diff_t<Iterator> mid{};

    while ((mid = len / 2)) {
        Iterator pivot = base + mid;
        if (pred(pivot - 1)) {
            base = pivot;
        }
        len -= mid;
    }
    return base;
}

//
// Basic merge routines
//

template <typename Iterator>
struct MergeResult {
    bool xs_consumed;
    Iterator rest;
};

/**
 * @brief Merge adjacent sequences xs and ys into the buffer before xs. The buffer moves while merging.
 *
 * When xs or ys becomes empty, the function returns which of xs or ys is fully consumed, as well as
 * the iterator to the rest data.
 *
 * The sequence ys cannot be longer the the length of the buffer (because buffer overrun isn't checked.)
 *
 * @param buf
 *   @pre buf < xs. We let buf_len = xs - buf.
 *   @post rest - buf = buf_len.
 * @param xs
 *   @pre xs < ys
 * @param ys
 *   @pre ys < ys_last
 *   @pre ys_last - ys <= buf_len
 * @param ys_last
 * @param comp
 * @return xs_consumed: Whether `rest` contains elements from `ys`.
 * @return rest: Elements not merged.
 *   @post xs < rest < ys_last
 *
 * @note The implementation actually works if xs or ys is empty, but the behaviour shall not be depended on.
 */
template <bool is_xs_from_right, typename Iterator, typename Compare>
SAYHISORT_CONSTEXPR_SWAP MergeResult<Iterator> MergeWithBuf(Iterator& buf, Iterator xs, Iterator ys, Iterator ys_last,
                                                            Compare comp) {
    SAYHISORT_PERF_TRACE("MergeWithBuf");
    auto is_x_selected = [&comp](decltype(xs[0]) x, decltype(ys[0]) y) {
        if constexpr (is_xs_from_right) {
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
    // So-called cross merge optimization is applied
    // See https://github.com/scandum/quadsort#cross-merge for idea
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
        return {true, ys};
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
    return {false, ys};
}

/**
 * @brief Merge sequences xs and ys in-place.
 *
 * When xs or ys becomes empty, the function returns which of xs or ys is fully consumed, as well as
 * the iterator to the rest data.
 *
 * @param xs
 *   @pre xs < ys
 * @param ys
 *   @pre ys < ys_last
 * @param ys_last
 * @param comp
 * @note For better performance, `xs` shouldn't be longer than `ys`.
 *       The time complexity is `O((m + log(n)) * min(m, n, j, k) + n)`, where
 *       `m` and `n` are the lengthes of `xs` and `ys`, whereas `j` and `k` are
 *       the numbers of unique keys in `xs` and `ys`.
 */
template <bool is_xs_from_right, typename Iterator, typename Compare>
SAYHISORT_CONSTEXPR_SWAP MergeResult<Iterator> MergeWithoutBuf(Iterator xs, Iterator ys, Iterator ys_last,
                                                               Compare comp) {
    SAYHISORT_PERF_TRACE("MergeWithoutBuf");
    while (true) {
        // Seek xs so that xs[0] > ys[0]
        xs = BinarySearch<is_xs_from_right>(xs, ys, ys, comp);
        if (xs == ys) return {true, ys};
        // Insert xs to ys
        Iterator ys_upper = ys + 1;
        if (ys_upper != ys_last) {
            ys_upper = BinarySearch<!is_xs_from_right>(ys_upper, ys_last, xs, comp);
        }
        Rotate(xs, ys, ys_upper);
        xs += ys_upper - ys;
        ys = ys_upper;
        if (ys_upper == ys_last) {
            return {false, xs};
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
template <typename Iterator, typename Compare>
SAYHISORT_CONSTEXPR_SWAP Iterator InterleaveBlocks(Iterator imit, Iterator blocks, diff_t<Iterator> num_blocks,
                                                   diff_t<Iterator> block_len, Compare comp) {
    SAYHISORT_PERF_TRACE("Interleave");
    // Algorithm similar to wikisort's block movement
    // https://github.com/BonzaiThePenguin/WikiSort/blob/master/Chapter%203.%20In-Place.md
    //
    // While interleaving, the state of blocks is like:
    //   [interleaved | left_permuted | right]
    // We pick the least block `least_left` from `left_permuted` by linear search.
    // Then we compare `least_left` with `right[0]`, and swap the selected block for
    // `left_permuted[0]`.
    if (num_blocks == 0) {
        return imit;
    }

    auto swapBlock = [block_len](Iterator a, Iterator b) {
        if (a == b) {
            return;
        }
        diff_t<Iterator> i = block_len;
        do {
            swap(*a++, *b++);
        } while (--i);
    };

    Iterator left_keys = imit;
    Iterator right_keys = imit + num_blocks / 2;
    Iterator left_blocks = blocks;
    Iterator right_blocks = left_blocks + num_blocks / 2 * block_len;

    Iterator least_left_key = left_keys;
    Iterator least_left_block = left_blocks;

    Iterator least_right_key = right_keys;
    Iterator last_right_key = right_keys + num_blocks / 2;

    while (true) {
        if (right_keys == last_right_key || !comp(*right_blocks, *least_left_block)) {
            swap(*left_keys, *least_left_key);
            swapBlock(left_blocks, least_left_block);

            ++left_keys;
            left_blocks += block_len;
            if (left_keys == right_keys) {
                break;
            }

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
            swapBlock(left_blocks, right_blocks);

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
 *   @pre imit_len is a positive multiple of 2
 * @param buf
 *   @pre [imit, imit + imit_len) and [buf, buf + imit_len / 2) are non-overlapping
 * @param mid_key
 * @param comp
 */
template <typename Iterator, typename Compare>
SAYHISORT_CONSTEXPR_SWAP void DeinterleaveImitation(Iterator imit, diff_t<Iterator> imit_len, Iterator buf,
                                                    Iterator mid_key, Compare comp) {
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
            swap(*left_cur++, *cur++);
        } else {
            swap(*right_cur++, *cur++);
        }
    }

    // Append right keys in `buf` to `left_cur`
    do {
        swap(*left_cur++, *buf++);
    } while (buf != right_cur);
}

/**
 * @brief Sort interleaved keys in imitation buffer, in-place.
 *
 * @param imit
 * @param imit_len
 *   @pre imit_len is a positive multiple of 2
 * @param mid_key
 * @param comp
 */
template <typename Iterator, typename Compare>
SAYHISORT_CONSTEXPR_SWAP void DeinterleaveImitation(Iterator imit, diff_t<Iterator> imit_len, Iterator mid_key,
                                                    Compare comp) {
    // We colour each key by whether they are from left or right.
    // Then we can see the imitation buffer as a sequence of runs with alternating colour.
    //
    // In one iteration, the algorithm rotate pairs of (right, left)-runs alternately.
    // When no such a pair is found, all keys are properly sorted.
    // Each iteration halves the number of pairs, So the alogirithm works in O(N log N), where
    // N is size of `imit`.
    //
    // The idea to rotate pairs of runs of is borrowed from HolyGrailsort's algorithm.
    // https://github.com/HolyGrailSortProject/Holy-Grailsort/blob/ccfcc4315c6ccafbca5f6a51886710898a06c8a1/Holy%20Grail%20Sort/Java/Summer%20Dragonfly%20et%20al.'s%20Rough%20Draft/src/holygrail/HolyGrailSort.java#L1373-L1376
    diff_t<Iterator> l_runlength{};
    diff_t<Iterator> r_runlength{};
    diff_t<Iterator> num_rl_pairs{};

    auto rotate_runs = [&](Iterator cur) {
        if (!r_runlength) {
            l_runlength = 0;
            return;
        }
        if (++num_rl_pairs % 2 == 0) {
            l_runlength = 0;
            r_runlength = 0;
            return;
        }
        Iterator l_run = cur - l_runlength;
        Iterator r_run = l_run - r_runlength;
        Rotate(r_run, l_run, cur);
        if (num_rl_pairs == 1) {
            mid_key = cur - r_runlength;
        }
        l_runlength = 0;
        r_runlength = 0;
    };

    do {
        l_runlength = 0;
        r_runlength = 0;
        num_rl_pairs = 0;

        bool was_left = false;
        Iterator cur = imit;
        do {
            bool is_left = comp(*cur, *mid_key);
            if (was_left && !is_left) {
                rotate_runs(cur);
            }
            l_runlength += is_left;
            r_runlength += !is_left;
            was_left = is_left;
        } while (++cur != imit + imit_len);

        if (was_left) {
            rotate_runs(cur);
        }
    } while (num_rl_pairs > 1);
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

template <bool has_buf, typename Iterator, typename Compare>
SAYHISORT_CONSTEXPR_SWAP void MergeAdjacentBlocks(Iterator imit, Iterator& buf, Iterator blocks,
                                                  BlockingParam<diff_t<Iterator>> p, Iterator mid_key, Compare comp) {
    diff_t<Iterator> num_remained_blocks = p.num_blocks;

    enum BlockOrigin {
        kLeft,
        kRight,
    };

    Iterator xs = blocks;
    Iterator last_block_before_ys = xs;
    BlockOrigin xs_origin = kLeft;
    --num_remained_blocks;

    Iterator ys = xs + p.first_block_len;

    do {
        Iterator ys_last = ys + (--num_remained_blocks ? p.block_len : p.last_block_len);
        BlockOrigin ys_origin = (num_remained_blocks && comp(*imit++, *mid_key)) ? kLeft : kRight;

        if (ys_origin == xs_origin) {
            last_block_before_ys = ys;
            ys = ys_last;
            continue;
        }

        if (xs != last_block_before_ys) {
            if constexpr (has_buf) {
                if (num_remained_blocks) {
                    // Safely skip continuing blocks those have the same origin.
                    // Blocks are sorted by the first elements, so we can safely seek to
                    // the position `last_block_before_ys + 1`.
                    // The sequence `xs` won't be empty, since `block_len >= 2`.
                    do {
                        swap(*buf++, *xs++);
                    } while (xs != last_block_before_ys + 1);
                }
            } else {
                if (num_remained_blocks) {
                    // Safely skip continuing blocks as done in `has_buf` case.
                    xs = last_block_before_ys + 1;
                } else if (ys - xs > p.last_block_len) {
                    // Ensure that of `xs` is not longer than `ys`. This is crucial to ensure time complexity due to
                    // implementation of `MergeWithoutBuf`.
                    Rotate(xs, ys, ys_last);
                    ys = xs + p.last_block_len;
                    xs_origin = kRight;
                    ys_origin = kLeft;
                }
            }
        }

        MergeResult mr = ([&]() {
            if constexpr (has_buf) {
                if (xs_origin == kLeft) {
                    return MergeWithBuf<false>(buf, xs, ys, ys_last, comp);
                } else {
                    return MergeWithBuf<true>(buf, xs, ys, ys_last, comp);
                }
            } else {
                if (xs_origin == kLeft) {
                    return MergeWithoutBuf<false>(xs, ys, ys_last, comp);
                } else {
                    return MergeWithoutBuf<true>(xs, ys, ys_last, comp);
                }
            }
        })();

        xs = mr.rest;
        last_block_before_ys = xs;
        xs_origin = static_cast<BlockOrigin>(static_cast<bool>(xs_origin) ^ mr.xs_consumed);

        ys = ys_last;
    } while (num_remained_blocks);

    if constexpr (has_buf) {
        while (xs != ys) {
            swap(*buf++, *xs++);
        }
    }
}

template <bool has_buf, typename Iterator, typename Compare>
SAYHISORT_CONSTEXPR_SWAP void MergeBlocking(Iterator imit, Iterator& buf, Iterator blocks,
                                            BlockingParam<diff_t<Iterator>> p, Compare comp) {
    // Skip interleaving the first block and the last one, those may have shorter length.
    diff_t<Iterator> imit_len = p.num_blocks - 2;
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
    constexpr ReverseCompare(Compare comp) : Compare(comp) {}

    template <typename T>
    constexpr bool operator()(T&& lhs, T&& rhs) {
        return Compare::operator()(std::forward<T>(rhs), std::forward<T>(lhs));
    }
};

template <typename Compare>
struct ReverseCompare<Compare, false> {
    constexpr ReverseCompare(Compare comp) : comp_{comp} {}

    template <typename T>
    constexpr bool operator()(T&& lhs, T&& rhs) {
        return comp_(std::forward<T>(rhs), std::forward<T>(lhs));
    }

private:
    Compare comp_;
};

/**
 * @brief Helper to evenly divide array those length may not be power of 2
 *
 * The algorithm simulates real-number division. That is, when an array with its length `L` is
 * divided to `n` sequences, `i`-th sequence is the slice of the following range:
 *   [ floor(i * (L/n)) , floor((i+1) * (L/N)) ) .
 * Since `N` is power of 2, we can exactly compute the range by tracking the fractional part by an integer.
 */
template <typename SsizeT, bool forward = true>
struct SequenceDivider {
    constexpr SequenceDivider(SsizeT data_len, SsizeT log2_num_seqs)
        : log2_num_seqs{log2_num_seqs},
          num_seqs{SsizeT{1} << log2_num_seqs},
          remainder{(data_len - 1) % num_seqs + 1},
          frac_counter{0} {
        if constexpr (!forward) {
            remainder = num_seqs - remainder;
        }
    }

    constexpr bool Next() {
        frac_counter += remainder;
        bool no_carry = !(frac_counter & (SsizeT{1} << log2_num_seqs));
        if constexpr (!forward) {
            no_carry = !no_carry;
        }
        frac_counter &= ~(SsizeT{1} << log2_num_seqs);
        --num_seqs;
        return no_carry;
    }

    constexpr bool IsEnd() const { return !num_seqs; }

    SsizeT log2_num_seqs;
    SsizeT num_seqs;
    SsizeT remainder;
    SsizeT frac_counter;
};

template <bool has_buf, bool forward, typename Iterator, typename Compare>
SAYHISORT_CONSTEXPR_SWAP void MergeOneLevel(Iterator imit, Iterator buf, Iterator data, diff_t<Iterator> seq_len,
                                            SequenceDivider<diff_t<Iterator>, forward> seq_div,
                                            BlockingParam<diff_t<Iterator>> p, Compare comp) {
    diff_t<Iterator> residual_len = p.first_block_len;
    do {
        bool lseq_decr = seq_div.Next();
        bool rseq_decr = seq_div.Next();
        diff_t<Iterator> merging_len = (seq_len - lseq_decr) + (seq_len - rseq_decr);
        p.first_block_len = residual_len - lseq_decr;
        p.last_block_len = residual_len - rseq_decr;

        if constexpr (forward) {
            MergeBlocking<has_buf>(imit, buf, data, p, comp);
            data += merging_len;
        } else {
            auto rev_imit = std::make_reverse_iterator(imit + p.num_blocks - 2);
            auto rev_buf = std::make_reverse_iterator(buf);
            auto rev_data = std::make_reverse_iterator(data);
            MergeBlocking<has_buf>(rev_imit, rev_buf, rev_data, p, ReverseCompare{comp});
            buf = rev_buf.base();
            data -= merging_len;
        }
    } while (!seq_div.IsEnd());
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
template <int len, typename Iterator, typename Compare, int i = 0>
SAYHISORT_CONSTEXPR_SWAP void OddEvenSort(Iterator data, Compare comp) {
    if constexpr (i < len) {
        for (diff_t<Iterator> j = i % 2; j < len - 1; j += 2) {
            if (comp(data[j + 1], data[j])) {
                swap(data[j], data[j + 1]);
            }
        }
        OddEvenSort<len, Iterator, Compare, i + 1>(data, comp);
    }
}

/**
 * @brief Sort leaf sequences divided by bottom-up merge sorting.
 *
 * @param data
 * @param seq_len
 *   @pre 5 <= seq_len <= 8; or seq_len == 4 and seq_div.Next() never returns true
 * @param seq_div
 * @param comp
 */
template <typename Iterator, typename Compare>
SAYHISORT_CONSTEXPR_SWAP void SortLeaves(Iterator data, diff_t<Iterator> seq_len,
                                         SequenceDivider<diff_t<Iterator>> seq_div, Compare comp) {
    SAYHISORT_PERF_TRACE("SortLeaves");
#if 0
    do {
        bool decr = seq_div.Next();
        diff_t<Iterator> len = seq_len - decr;
        if (len == 4) {
            OddEvenSort<4>(data, comp);
        } else if (len == 5) {
            OddEvenSort<5>(data, comp);
        } else if (len == 6) {
            OddEvenSort<6>(data, comp);
        } else if (len == 7) {
            OddEvenSort<7>(data, comp);
        } else if (len == 8) {
            OddEvenSort<8>(data, comp);
        }
        data += len;
    } while (!seq_div.IsEnd());
#else
    // Apply optimization similar to thread-code technique https://en.wikipedia.org/wiki/Threaded_code
    // Though branch miss-prediction isn't a problem, it reduces the number of branch instructions without
    // duplicating template instantiation of OddEvenSort<n>.
    enum Dispatcher {
        Len4,
        Len5,
        Len6,
        Len7,
        Len8,
    };
    diff_t<Iterator> len = seq_len - seq_div.Next();
    Dispatcher dispatcher = static_cast<Dispatcher>(static_cast<int>(len - 4));
    while (true) {
        switch (dispatcher) {
            case Len4:
                OddEvenSort<4>(data, comp);
                data += 4;
                if (seq_div.IsEnd()) {
                    return;
                }
                len = seq_len - seq_div.Next();
                if (len == 4) {
                    dispatcher = Len4;
                    break;
                }
                [[fallthrough]];
            case Len5:
                OddEvenSort<5>(data, comp);
                data += 5;
                if (seq_div.IsEnd()) {
                    return;
                }
                len = seq_len - seq_div.Next();
                if (len == 4) {
                    dispatcher = Len4;
                    break;
                }
                if (len == 5) {
                    dispatcher = Len5;
                    break;
                }
                [[fallthrough]];
            case Len6:
                OddEvenSort<6>(data, comp);
                data += 6;
                if (seq_div.IsEnd()) {
                    return;
                }
                len = seq_len - seq_div.Next();
                if (len == 5) {
                    dispatcher = Len5;
                    break;
                }
                if (len == 6) {
                    dispatcher = Len6;
                    break;
                }
                [[fallthrough]];
            case Len7:
                OddEvenSort<7>(data, comp);
                data += 7;
                if (seq_div.IsEnd()) {
                    return;
                }
                len = seq_len - seq_div.Next();
                if (len == 6) {
                    dispatcher = Len6;
                    break;
                }
                if (len == 7) {
                    dispatcher = Len7;
                    break;
                }
                [[fallthrough]];
            case Len8:
                OddEvenSort<8>(data, comp);
                data += 8;
                if (seq_div.IsEnd()) {
                    return;
                }
                len = seq_len - seq_div.Next();
                if (len == 7) {
                    dispatcher = Len7;
                    break;
                } else {
                    dispatcher = Len8;
                    break;
                }
        }
    }
#endif
}

/**
 * @brief Sort data with its length 0 to 8. Sorting is stable.
 *
 * @param data
 * @param len
 *   @pre 0 <= len <= 8
 * @param comp
 */
template <typename Iterator, typename Compare>
SAYHISORT_CONSTEXPR_SWAP void Sort0To8(Iterator data, diff_t<Iterator> len, Compare comp) {
    if (len <= 1) {
        return;
    }
    if (len <= 3) {
        if (comp(data[1], data[0])) {
            swap(data[0], data[1]);
        }
        if (len == 2) {
            return;
        }
        if (comp(data[2], data[1])) {
            swap(data[1], data[2]);
        }
        if (comp(data[1], data[0])) {
            swap(data[0], data[1]);
        }
        return;
    }
    return SortLeaves(data, len, {len, diff_t<Iterator>{0}}, comp);
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
    SsizeT n = 0;
    for (int j = 1; j < 8; ++j) {
        n += kCiuraGaps[j] < len;
    }
    SsizeT gap = kCiuraGaps[static_cast<int>(n)];
    if (n == 7) {
        // The loop condition is equivalent to `floor(2.25 * gap) < len`, which can be written as
        // `gap * 2 < len - floor(0.25 * gap)`. We can check the equivalence for each case `len - floor(0.25 * gap)` is
        // even or odd.
        while (gap < (len - gap / 4 + 1) / 2) {
            gap = gap * 2 + gap / 4;
            ++n;
        }
    }
    return {gap, n};
}

/**
 * @brief Find the n'th gap from Ciura's gap sequence.
 *
 * @param n
 *   @pre n >= 0
 * @return gap
 *   @post gap >= 1
 */
template <typename SsizeT>
constexpr SsizeT NthShellSortGap(SsizeT n) {
    if (n < 8) {
        return kCiuraGaps[static_cast<int>(n)];
    }
    SsizeT i = 7;
    SsizeT gap = kCiuraGaps[7];
    do {
        gap = gap * 2 + gap / 4;
    } while (++i < n);
    return gap;
}

/**
 * @brief Sort data by Shell sorting with Ciura's gap sequence. Sorting is unstable.
 *
 * The sequence is extended by repeatedly applying `x \mapsto floor(2.25 * x)` to the last pre-computed element 701.
 * https://en.wikipedia.org/wiki/Shellsort#Computational_complexity
 *
 * Though we also considered Tokuda's sequence, this is abandoned since computing the sequence without floating-number
 * is prone to overflow.
 *
 * @param data
 * @param len
 *   @pre len >= 2
 * @param comp
 */
template <typename Iterator, typename Compare>
SAYHISORT_CONSTEXPR_SWAP void ShellSort(Iterator data, diff_t<Iterator> len, Compare comp) {
    auto [gap, n] = FirstShellSortGap(len);

    while (true) {
        diff_t<Iterator> i = gap;
        do {
            for (diff_t<Iterator> j = i; j >= gap; j -= gap) {
                if (!comp(data[j], data[j - gap])) {
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

/**
 * @brief Collect unique keys.
 *
 * @param first
 * @param last
 *   @pre last - first >= 2
 * @param num_desired_keys
 *   @pre num_desired_keys >= 2
 */
template <typename Iterator, typename Compare>
SAYHISORT_CONSTEXPR_SWAP diff_t<Iterator> CollectKeys(Iterator first, Iterator last, diff_t<Iterator> num_desired_keys,
                                                      Compare comp) {
    Iterator keys = first;
    Iterator keys_last = first + 1;
    Iterator cur = first + 1;
    --num_desired_keys;

    do {
        Iterator inspos = BinarySearch<true>(keys, keys_last, cur, comp);
        if (inspos == keys_last || comp(*cur, *inspos)) {
            // Rotate keys forward so that insertion works in O(num_keys)
            Rotate(keys, keys_last, cur);
            keys += cur - keys_last;
            inspos += cur - keys_last;
            // Insert the new key
            Rotate(inspos, cur, cur + 1);
            keys_last = cur + 1;
            --num_desired_keys;
        }
    } while (num_desired_keys && ++cur < last);

    Rotate(first, keys, keys_last);
    return keys_last - keys;
}

template <typename SsizeT>
struct MergeSortControl {
    /**
     * @param num_keys
     *   @pre num_keys == 0 or num_keys >= 8
     * @param data_len
     *   @pre data_len > 8
     * @post 5 <= this->seq_len <= 8
     *
     * The space of unique keys are divided by imitation buffer (recording block permutation) and
     * merge buffer (holding merged block elements). So `num_keys = imit_len + buf_len` will holds.
     *
     * We don't record the leftmost and rightmost block into the imitation buffer (due to remainder handling).
     * Therefore the number of blocks is at most `imit_len + 2`.
     *
     * The value `bufferable_len` is the maximum length of a sequence, that can be merged with buffering.
     * Imitation buffer holds permutation of blocks in the two sequences which will be merged, and buffer must be
     * large enough to store a block. So it's defined by
     *
     *   bufferable_len = (imit_len + 2) / 2 * buf_len .
     *
     * When `num_keys = num_desired_key`, `imit_len + 2 >= sqrt(len)` and `buf_len >= sqrt(len)` holds.
     * Therefore buffering is enabled until the last merge level, ensuring O(len) time complexity for each merge level.
     */
    constexpr MergeSortControl(SsizeT num_keys, SsizeT data_len) : data_len{data_len} {
        if (num_keys) {
            // Prove that
            //
            //   (1) imit_len >= 2 ,
            //   (2) buf_len >= imit_len + 2 ,
            //   (3) bufferable_len >= 8 .
            //
            // (1) is straightforward, because we require `num_keys >= 8`.
            //
            // To prove (2), we use `imit_len + 2 <= (num_keys + 2) / 2`, which is immediate from definition. Now we
            // have
            //
            //   buf_len = num_keys - imit_len
            //           = (num_keys + 2) - (imit_len + 2)
            //           >= (num_keys + 2) / 2
            //           >= imit_len + 2 .
            //
            // Using (1) and (2), we can show (3) as folows:
            //
            //     bufferable_len = (imit_len + 2) / 2 * buf_len
            //                    >= (imit_len + 2) * (imit_len + 2) / 2
            //                    >= 8 .
            imit_len = (num_keys + 2) / 4 * 2 - 2;
            buf_len = num_keys - imit_len;
            bufferable_len = (imit_len + 2) / 2 * buf_len;
        }

        while ((data_len - 1) >> (log2_num_seqs + 3)) {
            ++log2_num_seqs;
        }
        // Here `5 <= seq_len <= 8` holds.
        // If `num_keys != 0`, we can safely adopt buffered merge logic at the first level.
        // Since `bufferable_len >= 8` is assured, `seq_len <= bufferable_len` is always satisfied here.
        seq_len = ((data_len - 1) >> log2_num_seqs) + 1;
    }

    constexpr SsizeT Next() {
        --log2_num_seqs;
        seq_len = ((data_len - 1) >> log2_num_seqs) + 1;

        if (!buf_len) {
            return 0;
        }
        forward = !forward;

        if (!(log2_num_seqs && seq_len <= bufferable_len)) {
            // No more buffered merge will be done. Need to clean up the buffer here.
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
     *   - seq_len <= bufferable_len
     *   - imit_len + 2 <= buf_len
     */
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
    SsizeT seq_len = ctrl.seq_len;

    SsizeT max_num_blocks = ctrl.imit_len + 2;
    SsizeT num_blocks{};
    if (ctrl.buf_len) {
        // We don't need to check `num_blocks < max_numblocks` since it's ensured by `seq_len <= buffearble_len`.
        num_blocks = ((seq_len - 1) / ctrl.buf_len + 1) * 2;
    } else {
        // Limit the number of blocks by `sqrt(2 * seq_len)`. Though this specific limit isn't carefully optimized
        // at now, it must be O(`sqrt(seq_len)`) to assure `InterleaveBlocks` runs in O(`seq_len`) time complexity.
        // Note that changing constant factor affects to the following validity proofs.
        SsizeT limit_num_blocks = seq_len / OverApproxSqrt(seq_len * 2) * 2;
        num_blocks = max_num_blocks < limit_num_blocks ? max_num_blocks : limit_num_blocks;
    }

    // Proof that `block_len >= 3`.
    // (NOTE: probably tighter bound is possible. For the sake of algorithm correctness `block_len >= 2` is enough.)
    //
    // If `buf_len = 0`, it's easy to see `block_len` is over-approx of `seq_len / sqrt(2)`, since `limit_num_blocks`
    // must be a multple of 2 and under-approx of `sqrt(2 * seq_len)`. As `seq_len >= 5`, we have `block_len >= 3`.
    //
    // Otherwise, we first note that
    //
    //    ceil(seq_len / buf_len) <= seq_len / buf_len + 1
    //                            = seq_len * (1 / buf_len + 1 / seq_len) .
    //
    // Using `buf_len >= imit_len + 2 >= 4` and `seq_len >= 5`, we also have
    //
    //    1 / buf_len + 1 / seq_len <= 1/4 + 1/5
    //                              = 0.45 .
    //
    // Therefore
    //
    //    ceil(seq_len / buf_len) <= seq_len * 0.45
    //
    // holds.
    //
    // From the definition of `block_len`, we have
    //
    //   block_len = ceil(seq_len / ceil(seq_len / buf_len))
    //             >= seq_len / ceil(seq_len / buf_len)
    //             >= 1 / 0.45 .
    //
    // Thus `block_len >= 3` follows, as `block_len` is an integer.
    SsizeT block_len = (seq_len - 1) / (num_blocks / 2) + 1;

    // We need to proof `residual_len >= 2` to assure that all blocks have positive length.
    // (Note that `residual_len` may be decremented once in `MergeOneLevel`.)
    //
    // Proof:
    // We first show the following lemma. Let `N` and `m` be positive integers, such that
    // `N >= m ** 2`, `N >= 2` and `m >= 1`.
    //
    //   (lemma):  N - ceil(N / m) * (m - 1) >= 2 .
    //
    // There are three cases.
    //
    //   (1):  m = 1 .
    //   (2):  m >= 2, and N is a multiple of m .
    //   (3):  m >= 2, and N isn't a multiple of m .
    //
    // If case (1) and (2), it's trivial to check that (lemma) holds.
    // For case (3), we can use the following property:
    //
    //   ceil(N / m) <= (N / m) + ((m - 1) / m) .
    //
    // By multiplying `m` to left and right hand sides, we have
    //
    //   ceil(N / m) * m <= N + (m - 1) .
    //
    // So
    //
    //   ceil(N / m) * (m - 1) <= N + (m - 1) - ceil(N / m) .
    //
    // holds.
    //
    // Since `N >= (m ** 2) + 1` from (3), `ceil(N / m) >= m + 1` holds. Thus we have
    //
    //   ceil(N / m) * (m - 1) <= N + (m - 1) - (m + 1)
    //                         =  N - 2 ,
    //
    // which is equivalent to (lemma). Thus the lemma is proven.
    //
    // To proof `residual_len >= 2`, let `N = seq_len` and `m = num_blocks / 2`. Now it's enough to show that
    //
    //   (proposition):  seq_len >= (num_blocks / 2) ** 2 ,
    //   (*):            seq_len >= 2 , and
    //   (**):           num_blocks / 2 >= 1 .
    //
    // Note that (*) and (**) are immediate from the function's requirement.
    //
    // When `buf_len = 0`, the definition of `limit_num_blocks` ensures that (proposition) is satisfied.
    // For the case `buf_len > 0`, we use the following constraints.
    //
    //   (a):  imit_len + 2 <= buf_len .                   (by MergeSortControl)
    //   (b):  seq_len <= (imit_len + 2) / 2 * buf_len .   (by MergeSortControl)
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
    // Thanks to (e), it's easy to see that the following (subprop) is enough to prove (proposition).
    //
    //   (subprop):  sqrt(seq_len) >= (sqrt(seq_len) / sqrt(2)) + 1
    //
    // As the function requires `seq_len >= 5`, (subprop) is always satisfied. Thus (proposition) is true.
    // Therefore We have proven `residual_len >= 2`.

    SsizeT residual_len = seq_len - block_len * (num_blocks / 2 - 1);

    return {num_blocks, block_len, residual_len, residual_len};
}

template <typename Iterator, typename Compare>
SAYHISORT_CONSTEXPR_SWAP void Sort(Iterator first, Iterator last, Compare comp) {
    diff_t<Iterator> len = last - first;
    if (len <= 8) {
        return Sort0To8(first, len, comp);
    }

    Iterator imit = first;
    diff_t<Iterator> num_keys = 0;
    if (len > 16) {
        // When `len > 16`, `OverApproxSqrt(len) < sqrt(len) * 1.25` is assured. So we have
        //
        //   len - num_desired_keys = len - 2 * OverApproxSqrt(len) + 2
        //                          > len - 2 * sqrt(len) * 1.25 + 2
        //                          = (sqrt(len) - 2.5) * sqrt(len) + 2 .
        //
        // As `sqrt(len) > 4`, therefore, `len - num_desired_keys > 8` holds
        diff_t<Iterator> num_desired_keys = 2 * OverApproxSqrt(len) - 2;
        num_keys = CollectKeys(first, last, num_desired_keys, comp);
        if (num_keys < 8) {
            imit += num_keys;
            len -= num_keys;
            num_keys = 0;
        }
    }

    // `data_len > 8`, because
    //   * if `8 < len <= 16`: `num_keys = 0`, and
    //   * if `len > 16`:      `len - num_desired_keys > 8` whereas `num_keys <= num_desired_keys`.
    diff_t<Iterator> data_len = len - num_keys;
    MergeSortControl ctrl{num_keys, data_len};

    Iterator data = imit + num_keys;
    SortLeaves(data, ctrl.seq_len, {ctrl.data_len, ctrl.log2_num_seqs}, comp);

    do {
        BlockingParam p = DetermineBlocking(ctrl);

        if (!ctrl.buf_len) {
            MergeOneLevel<false, true>(imit, imit + ctrl.imit_len, data, ctrl.seq_len,
                                       {ctrl.data_len, ctrl.log2_num_seqs}, p, comp);
        } else if (ctrl.forward) {
            MergeOneLevel<true, true>(imit, imit + ctrl.imit_len, data, ctrl.seq_len,
                                      {ctrl.data_len, ctrl.log2_num_seqs}, p, comp);
        } else {
            MergeOneLevel<true, false>(imit, last, last - ctrl.buf_len, ctrl.seq_len,
                                       {ctrl.data_len, ctrl.log2_num_seqs}, p, comp);
        }

        if (diff_t<Iterator> old_buf_len = ctrl.Next()) {
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
            MergeWithoutBuf<false>(imit, buf, data, comp);
        }
    } while (ctrl.log2_num_seqs);

    if (first != data) {
        MergeWithoutBuf<false>(first, data, last, comp);
    }
}

}  // namespace
}  // namespace detail

template <typename RandomAccessIterator>
SAYHISORT_CONSTEXPR_SWAP void sort(RandomAccessIterator first, RandomAccessIterator last) {
    return detail::Sort(first, last, std::less<>{});
}

template <typename RandomAccessIterator, typename Compare>
SAYHISORT_CONSTEXPR_SWAP void sort(RandomAccessIterator first, RandomAccessIterator last, Compare comp) {
    return detail::Sort(first, last, comp);
}

}  // namespace sayhisort

#ifdef SAYHISORT_H_PERF_TRACE_STUB
#undef SAYHISORT_PERF_TRACE
#endif

#endif  // SAYHISORT_H
