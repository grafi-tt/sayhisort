use crate::blocking::{MergeSortControl, determine_blocking, over_approx_sqrt};
use crate::merge::merge_without_buf;
use crate::seqdiv::{SequenceDivider, merge_one_level};
use crate::smallsort::{heap_sort, sort_0to8, sort_leaves};
use crate::utils::{FwdPtr, Pos, binary_search, rotate, strict_flag};

unsafe fn collect_keys<T, F: FnMut(&T, &T) -> bool>(
    first: FwdPtr<T>,
    last: FwdPtr<T>,
    mut num_desired_keys: usize,
    f: &mut F,
) -> usize {
    unsafe {
        let mut keys = first;
        let mut keys_last = first.add(1);
        let mut cur = first.add(1);
        num_desired_keys -= 1;

        loop {
            let mut inspos = binary_search(keys, keys_last, cur, f, strict_flag::<true>());
            if inspos == keys_last || cur.precede_strict(inspos, f) {
                let distance = cur.offset_from_unsigned(keys_last);
                if distance != 0 {
                    rotate(keys, keys_last, cur);
                    keys = keys.add(distance);
                    inspos = inspos.add(distance);
                }
                let mut tmp = cur;
                while tmp > inspos {
                    tmp.swap(tmp.sub(1));
                    tmp = tmp.sub(1);
                }
                keys_last = cur.add(1);
                num_desired_keys -= 1;
            }
            if num_desired_keys == 0 {
                break;
            }
            cur = cur.add(1);
            if cur == last {
                break;
            }
        }

        if keys.offset_from_unsigned(first) != 0 {
            rotate(first, keys, keys_last);
        }
        keys_last.offset_from_unsigned(keys)
    }
}

#[inline(never)]
pub unsafe fn sort<T, F: FnMut(&T, &T) -> bool>(first: FwdPtr<T>, last: FwdPtr<T>, f: &mut F) {
    unsafe {
        let mut len = last.offset_from_unsigned(first);
        if len <= 8 {
            return sort_0to8(first, len, f);
        }

        let mut imit = first;
        let mut num_keys = 0;
        if len > 16 {
            let num_desired_keys = 2 * over_approx_sqrt(len) - 2;
            num_keys = collect_keys(first, last, num_desired_keys, f);
            if num_keys < 8 {
                imit = imit.add(num_keys);
                len -= num_keys;
                num_keys = 0;
            }
        }

        let data_len = len - num_keys;
        let mut ctrl = MergeSortControl::new(num_keys, data_len);

        let data = imit.add(num_keys);
        sort_leaves(
            data,
            ctrl.seq_len,
            &mut SequenceDivider::new(ctrl.data_len, ctrl.log2_num_seqs),
            f,
        );

        loop {
            let p = determine_blocking(&ctrl);

            if ctrl.forward {
                let mut seqdiv = SequenceDivider::<true>::new(ctrl.data_len, ctrl.log2_num_seqs);
                if ctrl.buf_len == 0 {
                    merge_one_level::<_, _, true, true>(
                        imit,
                        imit.add(ctrl.imit_len),
                        data,
                        ctrl.seq_len,
                        &mut seqdiv,
                        p,
                        f,
                    );
                } else {
                    merge_one_level::<_, _, false, true>(
                        imit,
                        imit.add(ctrl.imit_len),
                        data,
                        ctrl.seq_len,
                        &mut seqdiv,
                        p,
                        f,
                    );
                }
            } else {
                let mut seqdiv = SequenceDivider::<false>::new(ctrl.data_len, ctrl.log2_num_seqs);
                merge_one_level::<_, _, true, false>(
                    imit,
                    last,
                    last.sub(ctrl.buf_len),
                    ctrl.seq_len,
                    &mut seqdiv,
                    p,
                    f,
                );
            }

            let old_buf_len = ctrl.next();
            if old_buf_len != 0 {
                let buf = data.sub(old_buf_len);
                if !ctrl.forward {
                    let mut back_buf = last;
                    let mut back_data = last.sub(old_buf_len);
                    loop {
                        back_data = back_data.sub(1);
                        back_buf = back_buf.sub(1);
                        back_data.swap(back_buf);
                        if back_data == buf {
                            break;
                        }
                    }
                    ctrl.forward = true;
                }
                heap_sort(buf, old_buf_len, f);
            }
            if ctrl.log2_num_seqs == 0 {
                break;
            }
        }

        if first != data {
            merge_without_buf(first, data, last, f, strict_flag::<false>());
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::test_util::test_rng;
    use std::collections::BTreeSet;

    /// Reference implementation. Returns the expected array and the number of keys.
    fn naive_collect_keys(ary: &[i32], num_desired_keys: usize) -> (Vec<i32>, usize) {
        let mut keys = BTreeSet::new();
        let mut dups = Vec::new();
        let mut first = 0;
        while first != ary.len() {
            let v = ary[first];
            first += 1;
            if keys.insert(v) {
                if keys.len() == num_desired_keys {
                    break;
                }
            } else {
                dups.push(v);
            }
        }

        let mut expected: Vec<i32> = keys.iter().copied().collect();
        expected.extend_from_slice(&dups);
        expected.extend_from_slice(&ary[first..]);
        (expected, keys.len())
    }

    #[test]
    fn collect_keys_test() {
        let ary_len = 1000;
        let num_desired_keys = 10;
        let mut rng = test_rng();

        for k in [0, 1, 11] {
            let mut ary: Vec<i32> = (0..ary_len).map(|_| rng.below(k + 1) as i32).collect();
            let (expected, expected_num_keys) = naive_collect_keys(&ary, num_desired_keys);
            let range = ary.as_mut_ptr_range();
            let num_keys = unsafe {
                collect_keys(
                    FwdPtr::new(range.start),
                    FwdPtr::new(range.end),
                    num_desired_keys,
                    &mut |a: &i32, b: &i32| a < b,
                )
            };
            assert_eq!(num_keys, expected_num_keys, "k={k}");
            assert_eq!(ary, expected, "k={k}");
        }
    }
}
