# SayhiSort

Block merge sort implementation inspired by [GrailSort](https://github.com/Mrrl/GrailSort). It's in-place, stable and runs in O(NlogN) time complexity.

The implementation is purely swap-based. So items nither default-constructible nor move-constructible are allowed, as long as they are swappable.

Its name derives from GrailSort, in honor of its auhor [Andrey Astrelin](https://superliminal.com/andrey/biography.html) rest in peace. Pronunciation of “say hi” sounds like the Japanse word 「聖杯（せいはい）」, which means grail.

TODO: complete sort routine. Most subroutines are already unittested.

Remaining tasks are

* refactor merge level management to a struct
* test and fix it
* test and fix MergeOneLevel
* test and fix the toplevel Sort routine

TODO: doc

TODO: benchmark

TODO: overflow resistance test by using 16bit index iterator. Its carefully written to avoid any overflow. Should be tested.

## Similar projects

* https://github.com/HolyGrailSortProject/Rewritten-Grailsort
* https://github.com/HolyGrailSortProject/Holy-Grailsort
* https://github.com/ecstatic-morse/MrrlSort/
* https://github.com/BonzaiThePenguin/WikiSort/
* https://github.com/scandum/octosort
