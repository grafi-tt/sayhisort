#ifndef SAYHISORT_BENCH_DATA_H
#define SAYHISORT_BENCH_DATA_H

#include <cstdint>
#include <random>

namespace sayhisort::test {

void Random(uint64_t* p, uint64_t n, std::mt19937_64& gen);
void RandomSqrtKeys(uint64_t* p, uint64_t n, std::mt19937_64& gen);
void RandomFewKeys(uint64_t* p, uint64_t n, std::mt19937_64& gen);
void Ascending(uint64_t* p, uint64_t n, std::mt19937_64& gen);
void Descending(uint64_t* p, uint64_t n, std::mt19937_64& gen);
void Equal(uint64_t* p, uint64_t n, std::mt19937_64& gen);
void MostlyAscending(uint64_t* p, uint64_t n, std::mt19937_64& gen);
void MostlyDescending(uint64_t* p, uint64_t n, std::mt19937_64& gen);
void MostlyEqual(uint64_t* p, uint64_t n, std::mt19937_64& gen);

}  // namespace sayhisort::test

#endif  // SAYHISORT_BEHCH_DATA_H
