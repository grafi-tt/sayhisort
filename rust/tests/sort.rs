//! Integration tests of the public API, comparing results with the standard stable sort.

use std::cmp::Ordering;
use std::panic::{AssertUnwindSafe, catch_unwind};

// Share the PRNG with unit tests without making it a part of the public API.
#[path = "../src/test_util.rs"]
#[allow(dead_code)]
mod test_util;

use test_util::test_rng;

fn cmp_div4(a: &i32, b: &i32) -> Ordering {
    (a >> 2).cmp(&(b >> 2))
}

/// Shuffled `0..len` followed by sentinels, to detect writes beyond the sorted range.
fn shuffled_with_sentinel(len: usize, rng: &mut test_util::Xoshiro256PlusPlus) -> Vec<i32> {
    const ARY_LEN: usize = 1024;
    let mut ary: Vec<i32> = (0..len as i32).collect();
    rng.shuffle(&mut ary);
    ary.resize(ARY_LEN, ARY_LEN as i32);
    ary
}

#[test]
fn sort_matches_std() {
    let mut rng = test_rng();
    for len in 0..1024 {
        let mut ary = shuffled_with_sentinel(len, &mut rng);
        let mut expected = ary.clone();
        sayhisort::sort(&mut ary[..len]);
        expected[..len].sort();
        assert_eq!(ary, expected, "len={len}");
    }
}

/// Many items compare equal, so the comparison with `slice::sort_by` also checks stability.
#[test]
fn sort_by_is_stable() {
    let mut rng = test_rng();
    for len in 0..1024 {
        let mut ary = shuffled_with_sentinel(len, &mut rng);
        let mut expected = ary.clone();
        sayhisort::sort_by(&mut ary[..len], |a, b| (a >> 2).cmp(&(b >> 2)));
        expected[..len].sort_by(|a, b| (a >> 2).cmp(&(b >> 2)));
        assert_eq!(ary, expected, "len={len}");
    }
}

/// Counterpart of the C++ `SortFunctionPointer` test: a function item instead of a closure.
#[test]
fn sort_by_fn_item() {
    let mut rng = test_rng();
    for len in [0, 1, 8, 9, 16, 17, 100, 1023] {
        let mut ary = shuffled_with_sentinel(len, &mut rng);
        let mut expected = ary.clone();
        sayhisort::sort_by(&mut ary[..len], cmp_div4);
        expected[..len].sort_by(cmp_div4);
        assert_eq!(ary, expected, "len={len}");
    }
}

/// Typical input patterns, with lengths across the boundaries of the buffered / unbuffered merge.
#[test]
fn sort_patterns() {
    let mut rng = test_rng();
    for len in [17, 100, 1000, 10000, 65537] {
        let patterns: [(&str, Vec<u32>); 5] = [
            ("sorted", (0..len).collect()),
            ("reversed", (0..len).rev().collect()),
            ("all_equal", vec![7; len as usize]),
            ("few_unique", (0..len).map(|_| rng.below(5) as u32).collect()),
            ("random", (0..len).map(|_| rng.next_u64() as u32).collect()),
        ];
        for (name, data) in patterns {
            // Pair with the original index to check stability.
            let mut ary: Vec<(u32, usize)> = data.into_iter().zip(0..).collect();
            let mut expected = ary.clone();
            sayhisort::sort_by(&mut ary, |a, b| a.0.cmp(&b.0));
            expected.sort_by(|a, b| a.0.cmp(&b.0));
            assert!(ary == expected, "pattern={name} len={len}");
        }
    }
}

/// Non-`Copy` items with destructors, and zero-sized items.
#[test]
fn sort_non_copy_and_zst() {
    let mut rng = test_rng();
    let mut ary: Vec<String> = (0..1000).map(|_| format!("{:03}", rng.below(300))).collect();
    let mut expected = ary.clone();
    sayhisort::sort(&mut ary);
    expected.sort();
    assert_eq!(ary, expected);

    let mut zst = vec![(); 1000];
    sayhisort::sort(&mut zst);
    assert_eq!(zst.len(), 1000);
}

/// An inconsistent comparator must not cause memory unsafety. The result is a permutation of the input.
#[test]
fn sort_inconsistent_comparator() {
    for len in [9, 17, 100, 1000] {
        let mut ary: Vec<i32> = (0..len).collect();
        let mut coin = test_rng();
        sayhisort::sort_by(&mut ary, |_, _| match coin.below(3) {
            0 => Ordering::Less,
            1 => Ordering::Equal,
            _ => Ordering::Greater,
        });
        ary.sort();
        assert!(ary.iter().copied().eq(0..len), "len={len}");
    }
}

/// A panic in the comparator must leave the slice as a permutation of the input, without double drop or leak.
#[test]
fn sort_panic_safety() {
    for len in [9, 17, 100, 1000] {
        for panic_at in [0, 1, len / 2, len * 4] {
            let mut ary: Vec<String> = (0..len).rev().map(|i| i.to_string()).collect();
            let mut calls = 0;
            let result = catch_unwind(AssertUnwindSafe(|| {
                sayhisort::sort_by(&mut ary, |a, b| {
                    calls += 1;
                    if calls > panic_at {
                        panic!("comparator panicked");
                    }
                    a.len().cmp(&b.len()).then_with(|| a.cmp(b))
                })
            }));
            assert!(result.is_err() || panic_at >= len * 4, "len={len} panic_at={panic_at}");
            let mut sorted: Vec<usize> = ary.iter().map(|s| s.parse().unwrap()).collect();
            sorted.sort();
            assert!(sorted.into_iter().eq(0..len), "len={len} panic_at={panic_at}");
        }
    }
}
