// Randomized test using the same checker as the fuzzer. It works with any compiler, and is registered to CTest.
//
// Usage: sayhisort_random_test [iterations] [seed]

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <vector>

#include "sayhisort_fuzz_util.h"

int main(int argc, char** argv) {
    long iterations = argc > 1 ? std::atol(argv[1]) : 20000;
    uint64_t seed = argc > 2 ? std::strtoull(argv[2], nullptr, 10) : 1;

    std::mt19937_64 rng{seed};
    std::vector<uint8_t> input;
    for (long i = 0; i < iterations; ++i) {
        // 14 bytes of parameters followed by raw keys
        input.resize(14 + rng() % 4096);
        for (auto& byte : input) {
            byte = static_cast<uint8_t>(rng());
        }
        sayhisort::test::CheckSortWithFuzzInput(input.data(), input.size());
    }
    std::printf("%ld iterations passed\n", iterations);
    return 0;
}
