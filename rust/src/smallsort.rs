use crate::seqdiv::SequenceDivider;
use crate::utils::{FwdPtr, Pos, strict_flag};

unsafe fn odd_even_sort<T, F: FnMut(&T, &T) -> bool, const LEN: usize>(data: FwdPtr<T>, f: &mut F) {
    unsafe {
        for i in 0..LEN {
            for j in ((i % 2)..(LEN - 1)).step_by(2) {
                if data.add(j + 1).precede(data.add(j), f, strict_flag::<true>()) {
                    data.add(j).swap(data.add(j + 1));
                }
            }
        }
    }
}

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

pub unsafe fn sort_0to8<T, F: FnMut(&T, &T) -> bool>(data: FwdPtr<T>, len: usize, f: &mut F) {
    unsafe {
        if len <= 1 {
            return;
        }
        if len <= 3 {
            if data.add(1).precede(data, f, strict_flag::<true>()) {
                data.swap(data.add(1));
            }
            if len == 2 {
                return;
            }
            if data.add(2).precede(data.add(1), f, strict_flag::<true>()) {
                data.add(1).swap(data.add(2));
            }
            if data.add(1).precede(data, f, strict_flag::<true>()) {
                data.swap(data.add(1));
            }
            return;
        }
        sort_leaves(data, len, &mut SequenceDivider::new(len, 0), f);
    }
}

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
                cur = if data.add(l).precede(data.add(r), f, strict_flag::<true>()) {
                    r
                } else {
                    l
                };
            }
            if cur < len / 2 {
                cur = left(cur);
            }
            while data.add(cur).precede(data.add(start), f, strict_flag::<true>()) {
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
