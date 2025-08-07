# SayhiSort

Block merge sort implementation in C++ inspired by [GrailSort](https://github.com/Mrrl/GrailSort). It's in-place, stable and runs in O(N log(N)) wort-case time complexity. The performance is smililar to `std::stable_sort`.

The implementation is purely swap-based. So items nither default-constructible nor move-constructible are allowed, as long as they are swappable. It also care about portability. No floating point number is used, and the code is carefully written and tested to avoid overflow in any integer type.

Its name derives from GrailSort, in honor of its auhor [Andrey Astrelin](https://superliminal.com/andrey/biography.html) rest in peace. Pronunciation of “say hi” sounds like the Japanse word 「聖杯（せいはい）」, which means grail.

## Usage

It's header-only C++ library, so just including `sayhisort.h` is fine. C++17 or later is supported. You can also import CMake external project.

## Benchmark

TODO: benchmark with other libs and show result

## Similar projects

* https://github.com/HolyGrailSortProject/Rewritten-Grailsort
* https://github.com/HolyGrailSortProject/Holy-Grailsort
* https://github.com/ecstatic-morse/MrrlSort/
* https://github.com/BonzaiThePenguin/WikiSort/
* https://github.com/scandum/octosort
