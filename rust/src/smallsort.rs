use crate::seqdiv::SequenceDivider;
use crate::utils::{FwdPtr, Pos};

/// Sorts `LEN` items by stable odd-even sort.
///
/// # Safety
///
/// `[data, data + LEN)` is valid.
unsafe fn odd_even_sort<T, F: FnMut(&T, &T) -> bool, const LEN: usize>(data: FwdPtr<T>, f: &mut F) {
    unsafe {
        for i in 0..LEN {
            for j in ((i % 2)..(LEN - 1)).step_by(2) {
                if data.add(j + 1).precede_strict(data.add(j), f) {
                    data.add(j).swap(data.add(j + 1));
                }
            }
        }
    }
}

/// Sorts leaf sequences of the bottom-up merge sort.
///
/// # Safety
///
/// `5 <= seq_len <= 8`, or `seq_len == 4` and `seq_div.next()` never returns true.
pub unsafe fn sort_leaves<T, F: FnMut(&T, &T) -> bool>(
    mut data: FwdPtr<T>,
    seq_len: usize,
    seq_div: &mut SequenceDivider,
    f: &mut F,
) {
    enum Dispatcher {
        Len4,
        Len5,
        Len6,
        Len7,
        Len8,
    }
    let mut len = seq_len - seq_div.next() as usize;
    let mut dispatcher = match len {
        4 => Dispatcher::Len4,
        5 => Dispatcher::Len5,
        6 => Dispatcher::Len6,
        7 => Dispatcher::Len7,
        _ => Dispatcher::Len8,
    };
    unsafe {
        loop {
            match dispatcher {
                Dispatcher::Len4 => {
                    odd_even_sort::<_, _, 4>(data, f);
                    data = data.add(4);
                    if seq_div.is_end() {
                        return;
                    }
                    len = seq_len - seq_div.next() as usize;
                    dispatcher = if len == 4 { Dispatcher::Len4 } else { Dispatcher::Len5 };
                }
                Dispatcher::Len5 => {
                    odd_even_sort::<_, _, 5>(data, f);
                    data = data.add(5);
                    if seq_div.is_end() {
                        return;
                    }
                    len = seq_len - seq_div.next() as usize;
                    dispatcher = if len == 4 {
                        Dispatcher::Len4
                    } else if len == 5 {
                        Dispatcher::Len5
                    } else {
                        Dispatcher::Len6
                    };
                }
                Dispatcher::Len6 => {
                    odd_even_sort::<_, _, 6>(data, f);
                    data = data.add(6);
                    if seq_div.is_end() {
                        return;
                    }
                    len = seq_len - seq_div.next() as usize;
                    dispatcher = if len == 5 {
                        Dispatcher::Len5
                    } else if len == 6 {
                        Dispatcher::Len6
                    } else {
                        Dispatcher::Len7
                    };
                }
                Dispatcher::Len7 => {
                    odd_even_sort::<_, _, 7>(data, f);
                    data = data.add(7);
                    if seq_div.is_end() {
                        return;
                    }
                    len = seq_len - seq_div.next() as usize;
                    dispatcher = if len == 6 {
                        Dispatcher::Len6
                    } else if len == 7 {
                        Dispatcher::Len7
                    } else {
                        Dispatcher::Len8
                    };
                }
                Dispatcher::Len8 => {
                    odd_even_sort::<_, _, 8>(data, f);
                    data = data.add(8);
                    if seq_div.is_end() {
                        return;
                    }
                    len = seq_len - seq_div.next() as usize;
                    dispatcher = if len == 7 { Dispatcher::Len7 } else { Dispatcher::Len8 };
                }
            }
        }
    }
}

/// Sorts `len` items by stable sort.
///
/// # Safety
///
/// `len <= 8`.
pub unsafe fn sort_0to8<T, F: FnMut(&T, &T) -> bool>(data: FwdPtr<T>, len: usize, f: &mut F) {
    unsafe {
        if len <= 1 {
            return;
        }
        if len <= 3 {
            if data.add(1).precede_strict(data, f) {
                data.swap(data.add(1));
            }
            if len == 2 {
                return;
            }
            if data.add(2).precede_strict(data.add(1), f) {
                data.add(1).swap(data.add(2));
            }
            if data.add(1).precede_strict(data, f) {
                data.swap(data.add(1));
            }
            return;
        }
        sort_leaves(data, len, &mut SequenceDivider::new(len, 0), f);
    }
}

/// Sorts `len` items by heapsort, which isn't stable.
///
/// # Safety
///
/// `len >= 2`.
#[inline(never)]
pub unsafe fn heap_sort<T, F: FnMut(&T, &T) -> bool>(data: FwdPtr<T>, mut len: usize, f: &mut F) {
    unsafe {
        let left = |i: usize| i * 2 + 1;
        let right = |i: usize| i * 2 + 2;
        let parent = |i: usize| (i - 1) / 2;

        let mut start = parent(len - 1);
        loop {
            let mut cur = start;
            while cur < parent(len) {
                let l = left(cur);
                let r = right(cur);
                cur = if data.add(l).precede_strict(data.add(r), f) {
                    r
                } else {
                    l
                };
            }
            if cur < len / 2 {
                cur = left(cur);
            }
            while data.add(cur).precede_strict(data.add(start), f) {
                if cur <= start {
                    return;
                }
                cur = parent(cur);
            }
            while cur > start {
                data.add(cur).swap(data.add(start));
                cur = parent(cur);
            }
            if start != 0 {
                start -= 1;
                continue;
            }
            len -= 1;
            if len == 0 {
                break;
            }
            data.swap(data.add(len));
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::test_util::test_rng;

    #[test]
    fn sort_0to8_test() {
        let expected: Vec<i32> = (0..8).collect();
        let mut rng = test_rng();

        for len in 0..=8 {
            let mut ary: Vec<i32> = (0..8).collect();
            rng.shuffle(&mut ary[..len]);
            unsafe { sort_0to8(FwdPtr::new(ary.as_mut_ptr()), len, &mut |a: &i32, b: &i32| a < b) };
            assert_eq!(ary, expected, "len={len}");
        }
    }

    #[test]
    fn heap_sort_test() {
        let mut rng = test_rng();

        for sz in [5, 2024] {
            let mut data: Vec<i32> = (0..sz).collect();
            rng.shuffle(&mut data);
            unsafe {
                heap_sort(FwdPtr::new(data.as_mut_ptr()), sz as usize, &mut |a: &i32, b: &i32| {
                    a < b
                })
            };
            let expected: Vec<i32> = (0..sz).collect();
            assert_eq!(data, expected, "sz={sz}");
        }
    }
}
