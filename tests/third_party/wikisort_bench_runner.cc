#include "wikisort_bench_runner.h"

#include <WikiSort.cpp>

#include <functional>

void RunWikiSort(std::vector<uint64_t>& data) {
    Wiki::Sort(data.begin(), data.end(), std::less{});
}
