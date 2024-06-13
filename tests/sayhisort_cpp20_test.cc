#include "sayhisort.h"

#include <array>
#include <functional>

int main() {
    constexpr std::array<int, 9> a = ([]() {
        std::array pi{3, 1, 4, 1, 5, 9, 2, 6, 5};
        sayhisort::sort(pi.begin(), pi.end(), std::less<int>{});
        return pi;
    })();
    static_assert(a == std::array{1, 1, 2, 3, 4, 5, 5, 6, 9});
    return 0;
}
