#ifndef SAYHISORT_TEST_UTIL_H
#define SAYHISORT_TEST_UTIL_H

#include <initializer_list>
#include <random>

namespace sayhisort::test {

std::mt19937_64 GetRNG(int seed, std::initializer_list<const char*> name);

}  // namespace sayhisort::test

#endif  // SAYHISORT_TEST_UTIL_H
