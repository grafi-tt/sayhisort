#include "sayhisort_bench_data.h"

#include <algorithm>
#include <cmath>

namespace sayhisort::test {

void Random(uint64_t* p, uint64_t n, std::mt19937_64& gen) {
    for (uint64_t i = 0; i < n; ++i) {
        p[i] = gen();
    }
}

void RandomSqrtKeys(uint64_t* p, uint64_t n, std::mt19937_64& gen) {
    std::uniform_int_distribution<uint64_t> dist{0, static_cast<uint64_t>(std::sqrt(static_cast<double>(n)))};
    for (uint64_t i = 0; i < n; ++i) {
        p[i] = dist(gen);
    }
}

void RandomFewKeys(uint64_t* p, uint64_t n, std::mt19937_64& gen) {
    std::uniform_int_distribution dist{0, 98};
    for (uint64_t i = 0; i < n; ++i) {
        p[i] = dist(gen);
    }
}

void Ascending(uint64_t* p, uint64_t n, std::mt19937_64&) {
    for (uint64_t i = 0; i < n; ++i) {
        p[i] = i;
    }
}

void Descending(uint64_t* p, uint64_t n, std::mt19937_64&) {
    for (uint64_t i = 0; i < n; ++i) {
        p[i] = n - i;
    }
}

void Equal(uint64_t* p, uint64_t n, std::mt19937_64&) {
    for (uint64_t i = 0; i < n; ++i) {
        p[i] = 1000;
    }
}

void MostlyAscending(uint64_t* p, uint64_t n, std::mt19937_64& gen) {
    std::uniform_real_distribution dist{-2.5, 2.5};
    for (uint64_t i = 0; i < n; ++i) {
        p[i] = static_cast<uint64_t>(std::max(i + dist(gen), 0.0));
    }
}

void MostlyDescending(uint64_t* p, uint64_t n, std::mt19937_64& gen) {
    std::uniform_real_distribution dist{-2.5, 2.5};
    for (uint64_t i = 0; i < n; ++i) {
        p[i] = static_cast<uint64_t>(std::max(n - i + dist(gen), 0.0));
    }
}

void MostlyEqual(uint64_t* p, uint64_t n, std::mt19937_64& gen) {
    std::uniform_int_distribution dist{0, 3};
    for (uint64_t i = 0; i < n; ++i) {
        p[i] = 1000 + dist(gen);
    }
}

}  // namespace sayhisort::test
