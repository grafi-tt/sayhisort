# SayhiSort

Fast and portable block merge sort implementation written in C++17, inspired by [GrailSort](https://github.com/Mrrl/GrailSort). It's in-place, stable and runs in O(N log(N)) wort-case time complexity.

The implementation is **purely swap-based**. It means **no item is constructed** at runtime. Items neither default-constructible nor move-constructible are allowed, as long as they are swappable. In contrast, other implementations usually come with fixed-size buffer to cache items. Despite the absence of cache, it has **competitive performance**.

Substantial effort is made for portability and security. **No floating point number** is used, and the code is carefully written and tested to avoid overflow in any integer-like type. The code has meticulous comments that clarify mathematical invariants.

Its name derives from GrailSort, in honor of its auhor [Andrey Astrelin](https://superliminal.com/andrey/biography.html) rest in peace. Pronunciation of “say hi” sounds like the Japanese word 「聖杯（せいはい）」, which means grail.

## Usage

It's header-only C++ library, so just including `sayhisort.h` is fine. You can also import CMake external project.

## Benchmark

TODO: benchmark with other libs and show result

## Similar projects

* https://github.com/HolyGrailSortProject/Rewritten-Grailsort
* https://github.com/HolyGrailSortProject/Holy-Grailsort
* https://github.com/ecstatic-morse/MrrlSort/
* https://github.com/BonzaiThePenguin/WikiSort/
* https://github.com/scandum/octosort

## Algorithm detail

Whereas all algorithm code is written by the author, many ideas are given from others' research and implementation.

It's the variant of block merge sort that

* search O(√len) unique keys for imitation buffer and data buffer at first, and
* alternately apply left-to-right merge and right-to-left merge to skip needless buffer movement, if data buffer is available.

NOTE: The following documentation isn't complete and polished.

### Merge algorithm

This phase is the largest bottleneck.

For the case buffer is available, basically textbook merge algorithm is used. Current implementation applies [cross merge](https://github.com/scandum/quadsort#cross-merge) technique for optimization.

Some adaptive merge logic can significantly improve performance, since the length of two sequences `xs` and `ys` can differ. Depending on data, `ys` can be much longer than `xs`.

For buffer-less case, the algorithm repeats the following procedure to merge sequences `xs` and `ys`:

* find the index `i` such that `xs[i] > ys[0]` by binary searching `xs`
* find the index `j` such in `xs[i] <= ys[j]` by binary searching `ys`, and
* rotate `xs[i:]` and `ys[:j]`.

To the author's best knowledge, there's no room of improvement here.

### Sorting blocks

This phase also takes quite noticeable time.

(TODO: write something better. It's selection sort on the permuted blocks of sequence ys. I think the algorithm is quite basic. I feel some empirical improvement is still possible...)

### Searching unique keys

This overhead heavily depends on input data. For RandomFew benchmark MostEqual benchmark, it's large. For random input, it's acceptable. (So the current implementation strategy to search O(√N) unique keys in single step seems to be OK.)

(TODO: Some unrolling experiment didn't show improvement. Try skipping equal keys.)

### Sorting short sequences

This phase takes non-neglible time, but anyway it takes only O(len) time.

When the sequence length is less than or equals to 8, odd-even sort is used. (Don't confuse with Batcher's odd-even merge sort.) It's stable and friendly to superscalar execution.

The loop calling odd-even sort is optimized to reduce overhead of dispatching specialized odd-even sort routines. Further optimization likely broats-up inlined code size, so the author is reluctant to explore this direction.

### Sorting unique keys

This phase takes negligible time, so it isn't worth of micro-optimization.

For de-interleaving imitation buffer, bin-sorting is used if data buffer can be used as auxiliary space. Otherwise novel O(NlogN) algorithm is used, that iteratively rotate skewed parts. See comments of `DeinterleaveImitation` for detail.

For sorting data buffer, ShellSort with [Ciura's gap sequence](https://en.wikipedia.org/wiki/Shellsort#Computational_complexity) is used. In practice it's very fast. From theoretical viewpoint, it's time complexity is O(len) even if ShellSort were O(N^2). Actual worst-case time complexity is likely much better.

### Rotation sub-algorithm

Use [Helix rotation](https://github.com/scandum/rotate#helix-rotation) for large data, because of it's simple control flow and cache friendliness. Switch to triple reversal when data becomes small, to avoid integer modulo operation in Helix rotation.

Though some improvement might be possible, significant speed-up is seemingly difficult.

### Binary search sub-algorithm

[Monobound binary search](https://github.com/scandum/binary_search). Information theoretically, the number of comparison to identify an item from N choices is at most `ceil(log2(N))`. The algorithm always performs this fixed number of comparisons. Though one comparison is redundant, friendliness to CPU pipeline likely wins.
