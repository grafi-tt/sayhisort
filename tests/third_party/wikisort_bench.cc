#include "wikisort_bench.h"

#include <WikiSort.cpp>

#include <functional>

void RunWikiSort(std::vector<uint64_t>& data) {
    Wiki::Sort(data.begin(), data.end(), std::less{});
}
