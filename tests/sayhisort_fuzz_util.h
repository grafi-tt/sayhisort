#ifndef SAYHISORT_FUZZ_UTIL_H
#define SAYHISORT_FUZZ_UTIL_H

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <random>
#include <utility>
#include <vector>

#include "sayhisort.h"

namespace sayhisort::test {

/**
 * @brief Random access iterator that aborts on out-of-bounds dereference.
 */
template <typename T>
class CheckedIterator {
public:
    using iterator_category = std::random_access_iterator_tag;
    using value_type = T;
    using difference_type = std::ptrdiff_t;
    using pointer = T*;
    using reference = T&;

    CheckedIterator() = default;
    CheckedIterator(T* ptr, T* first, T* last) : ptr_{ptr}, first_{first}, last_{last} {}

    reference operator*() const {
        if (ptr_ < first_ || ptr_ >= last_) {
            std::fprintf(stderr, "out-of-bounds access at offset %td\n", ptr_ - first_);
            std::abort();
        }
        return *ptr_;
    }
    pointer operator->() const { return &**this; }
    reference operator[](difference_type d) const { return *(*this + d); }

    CheckedIterator& operator++() {
        ++ptr_;
        return *this;
    }
    CheckedIterator& operator--() {
        --ptr_;
        return *this;
    }
    CheckedIterator operator++(int) {
        CheckedIterator t = *this;
        ++ptr_;
        return t;
    }
    CheckedIterator operator--(int) {
        CheckedIterator t = *this;
        --ptr_;
        return t;
    }
    CheckedIterator& operator+=(difference_type d) {
        ptr_ += d;
        return *this;
    }
    CheckedIterator& operator-=(difference_type d) {
        ptr_ -= d;
        return *this;
    }
    friend CheckedIterator operator+(CheckedIterator a, difference_type d) { return a += d; }
    friend CheckedIterator operator+(difference_type d, CheckedIterator a) { return a += d; }
    friend CheckedIterator operator-(CheckedIterator a, difference_type d) { return a -= d; }
    friend difference_type operator-(CheckedIterator a, CheckedIterator b) { return a.ptr_ - b.ptr_; }
    friend bool operator==(CheckedIterator a, CheckedIterator b) { return a.ptr_ == b.ptr_; }
    friend bool operator!=(CheckedIterator a, CheckedIterator b) { return a.ptr_ != b.ptr_; }
    friend bool operator<(CheckedIterator a, CheckedIterator b) { return a.ptr_ < b.ptr_; }
    friend bool operator>(CheckedIterator a, CheckedIterator b) { return a.ptr_ > b.ptr_; }
    friend bool operator<=(CheckedIterator a, CheckedIterator b) { return a.ptr_ <= b.ptr_; }
    friend bool operator>=(CheckedIterator a, CheckedIterator b) { return a.ptr_ >= b.ptr_; }

private:
    T* ptr_ = nullptr;
    T* first_ = nullptr;
    T* last_ = nullptr;
};

/**
 * @brief Reads parameters from fuzzer input. Missing bytes are read as zero.
 */
class FuzzInput {
public:
    FuzzInput(const uint8_t* data, size_t size) : data_{data}, size_{size} {}

    uint64_t Read(int num_bytes) {
        uint64_t v = 0;
        for (int i = 0; i < num_bytes; ++i) {
            v = (v << 8) | (pos_ < size_ ? data_[pos_++] : 0);
        }
        return v;
    }
    const uint8_t* Rest() const { return data_ + pos_; }
    size_t RestSize() const { return size_ - pos_; }

private:
    const uint8_t* data_;
    size_t size_;
    size_t pos_ = 0;
};

/**
 * @brief Sort data generated from fuzzer input, and abort if the result is wrong.
 *
 * Input layout: length (2 bytes), number of unique keys (1 + 2 bytes), arrangement (1 byte), seed (8 bytes), and
 * optional raw keys. Keys are generated from the seed unless the arrangement selects raw keys. Each item is a pair of
 * a key and its original index, and the result is compared with `std::stable_sort` to check stability as well.
 *
 * If the arrangement selects an inconsistent comparator, only memory safety, termination and permutation are checked.
 */
inline void CheckSortWithFuzzInput(const uint8_t* data, size_t size) {
    constexpr int kMaxLen = 4096;
    FuzzInput in{data, size};

    int len = static_cast<int>(in.Read(2) % (kMaxLen + 1));
    int uniq_mode = static_cast<int>(in.Read(1) % 4);
    int uniq_param = static_cast<int>(in.Read(2));
    int arrangement = static_cast<int>(in.Read(1) % 7);
    std::mt19937_64 rng{in.Read(8)};

    // Cover the paths depending on the number of unique keys: few keys (no buffer), around the number of desired keys,
    // arbitrary, and all unique.
    int uniq = 1;
    int desired = 2 * static_cast<int>(std::sqrt(static_cast<double>(len)));
    switch (uniq_mode) {
        case 0:
            uniq = 1 + uniq_param % 10;
            break;
        case 1:
            uniq = std::max(1, desired - 4 + uniq_param % 9);
            break;
        case 2:
            uniq = 1 + uniq_param % (len + 1);
            break;
        default:
            uniq = len + 1;
            break;
    }

    using Item = std::pair<int, int>;
    std::vector<Item> items(len);
    if (arrangement == 5) {
        // raw keys from input
        len = static_cast<int>(std::min<size_t>(len, in.RestSize()));
        items.resize(len);
        for (int i = 0; i < len; ++i) {
            items[i].first = in.Rest()[i] % uniq;
        }
    } else {
        for (auto& item : items) {
            item.first = static_cast<int>(rng() % static_cast<uint64_t>(uniq));
        }
    }
    auto key_less = [](const Item& x, const Item& y) { return x.first < y.first; };
    switch (arrangement) {
        case 1:  // ascending
            std::sort(items.begin(), items.end(), key_less);
            break;
        case 2:  // descending
            std::sort(items.rbegin(), items.rend(), key_less);
            break;
        case 3:  // rotated ascending
            std::sort(items.begin(), items.end(), key_less);
            if (len) {
                std::rotate(items.begin(), items.begin() + rng() % static_cast<uint64_t>(len), items.end());
            }
            break;
        case 4:  // mostly ascending
            std::sort(items.begin(), items.end(), key_less);
            for (int i = 0; len && i < len / 16 + 1; ++i) {
                std::swap(items[rng() % static_cast<uint64_t>(len)], items[rng() % static_cast<uint64_t>(len)]);
            }
            break;
        default:
            break;
    }
    for (int i = 0; i < len; ++i) {
        items[i].second = i;
    }

    std::vector<Item> work = items;
    CheckedIterator<Item> first{work.data(), work.data(), work.data() + len};
    if (arrangement == 6) {
        // Inconsistent comparator: the result is unspecified, but the items must be permuted safely.
        auto random_less = [&rng](const Item&, const Item&) { return (rng() & 1) != 0; };
        sayhisort::sort(first, first + len, random_less);
        std::sort(work.begin(), work.end());
        std::sort(items.begin(), items.end());
        if (work != items) {
            std::fprintf(stderr, "items are not permuted: len=%d uniq=%d\n", len, uniq);
            std::abort();
        }
        return;
    }

    sayhisort::sort(first, first + len, key_less);
    std::stable_sort(items.begin(), items.end(), key_less);
    if (work != items) {
        std::fprintf(stderr, "wrong result: len=%d uniq=%d arrangement=%d\n", len, uniq, arrangement);
        std::abort();
    }
}

}  // namespace sayhisort::test

#endif  // SAYHISORT_FUZZ_UTIL_H
