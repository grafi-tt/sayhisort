#include "sayhisort_test_util.h"

#include <charconv>
#include <cstdint>
#include <iterator>

namespace sayhisort::test {

std::mt19937_64 GetRNG(int seed, std::initializer_list<const char*> name) {
    uint64_t h = 0xcbf29ce484222325;
    auto fnv1a = [&h](const char* m) {
        while (*m) {
            h ^= *m++;
            h *= 0x00000100000001b3;
        }
    };

    char seed_hex[sizeof(int) * 2 + 2];
    if (char* p = std::to_chars(std::begin(seed_hex), std::end(seed_hex), seed, 16).ptr; p <= std::end(seed_hex) - 2) {
        p[0] = '/';
        p[1] = '\0';
    } else {
        // should be unreachable, but just nul-terminate for safety
        seed_hex[0] = '\0';
    }
    fnv1a(std::begin(seed_hex));

    for (const char* s : name) {
        fnv1a(s);
    }
    return std::mt19937_64{h};
}

}  // namespace sayhisort::test
