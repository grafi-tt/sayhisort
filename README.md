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

* search (approximately) 2√N unique keys for imitation buffer and data buffer at first, and
* alternately apply left-to-right merge and right-to-left merge to skip needless buffer movement, if data buffer is available.

NOTE: The following documentation isn't complete and polished.

### Searching unique keys

This phase's overhead heavily depends on input data. When the data has unique keys slight less that 2√N, it performs worst.

(TODO: Some unrolling experiment didn't show improvement.)

(TODO: Discuss that skipping equal keys doesn't improve worst case.)

(TODO: Discuss trade-off btw multi-phase collection.)

### Sorting blocks

This phase also takes quite noticeable time.

Each of two sequences to merge, those length is len, are divided at most `sqrt(len) / sqrt(2)` blocks. The blocks are merged in-place by the algorithm based on selection-sort. The merge algorithm is similar to basic one used in merge sort, but it uses selection sort to search the smallest block in the left sequence.

The algorithm is quite basic on block merge sort. The author tried moving blocks' first elements to buffer space to improve data locality, but performance had degraded. At now there is no idea for further speed-up.

### Merge algorithm

This phase is the largest bottleneck.

For the case buffer is available, basically textbook merge algorithm is used. Current implementation applies [cross merge](https://github.com/scandum/quadsort#cross-merge) technique for optimization.

Some adaptive merge logic can significantly improve performance, since the length of two sequences `xs` and `ys` can differ. Depending on data, `ys` can be much longer than `xs`.

For buffer-less case, the algorithm repeats the following procedure to merge sequences `xs` and `ys`:

* find the index `i` such that `xs[i] > ys[0]` by binary searching `xs`
* find the index `j` such in `xs[i] <= ys[j]` by binary searching `ys`, and
* rotate `xs[i:]` and `ys[:j]`.

To the author's best knowledge, there's no room of improvement here.

### Sorting short sequences

This phase takes non-neglible time, but anyway it takes only O(N) time.

When the sequence length is less than or equals to 8, odd-even sort is used. (Don't confuse with Batcher's odd-even merge sort.) It's stable and friendly to superscalar execution.

The loop calling odd-even sort is optimized to reduce overhead of dispatching specialized odd-even sort routines. Further optimization likely bloats-up inlined code size, so the author is reluctant to explore this direction.

### Sorting unique keys

This phase takes negligible time, so it isn't worth of micro-optimization. Let K be the number of unique keys collected, which is at most 2√N.

To de-interleave imitation buffer, bin-sorting is used if data buffer can be used as a auxiliary space. Otherwise novel O(K logK) algorithm is used, that iteratively rotate skewed parts. See comments of `DeinterleaveImitation` for detail.

For sorting data buffer, ShellSort with [Ciura's gap sequence](https://en.wikipedia.org/wiki/Shellsort#Computational_complexity) is used. I's very fast in practice. From theoretical viewpoint, time complexity is O(N) even if ShellSort were O(K^2). Actual worst-case time complexity is likely much better.

### Rotation sub-algorithm

Uses [Helix rotation](https://github.com/scandum/rotate#helix-rotation) for large data, because of it's simple control flow and cache friendliness. Switches to triple reversal when data becomes small, to avoid integer modulo operation in Helix rotation.

Though some improvement might be possible, significant speed-up is seemingly difficult.

### Binary search sub-algorithm

[Monobound binary search](https://github.com/scandum/binary_search). Information theoretically, the number of comparison to identify an item from N choices is at most `ceil(log2(N))`. The algorithm always performs this fixed number of comparisons. Though one comparison is redundant, friendliness to CPU pipeline likely wins.
