use crate::blocking::{determine_blocking, MergeSortControl, over_approx_sqrt};
use crate::merge::{merge_without_buf};
use crate::seqdiv::{merge_one_level, SequenceDivider};
use crate::smallsort::{heap_sort, sort_0to8, sort_leaves};
use crate::utils::{binary_search, FwdPtr, Pos, rotate, strict_flag};

unsafe fn collect_keys<T, F: FnMut(&T, &T) -> bool>(first: FwdPtr<T>, last: FwdPtr<T>, mut num_desired_keys: usize, f: &mut F) -> usize {
    unsafe {
        let mut keys = first;
        let mut keys_last = first.add(1);
        let mut cur = first.add(1);
        num_desired_keys -= 1;

        loop {
            let mut inspos = binary_search(keys, keys_last, cur, f, strict_flag::<true>());
            if inspos == keys_last || cur.precede(inspos, f, strict_flag::<true>()) {
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
                if num_desired_keys == 0 {
                    break;
                }
                cur = cur.add(1);
                if cur == last {
                    break;
                }
            }
        }

        if keys.offset_from_unsigned(first) != 0 {
            rotate(first, keys, keys_last);
        }
        keys.offset_from_unsigned(keys_last)
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
        sort_leaves(data, ctrl.seq_len, &mut SequenceDivider::new(ctrl.data_len, ctrl.log2_num_seqs), f);

        loop {
            let p = determine_blocking(&ctrl);

            if ctrl.forward {
                let mut seqdiv = SequenceDivider::<true>::new(ctrl.data_len, ctrl.log2_num_seqs);
                if ctrl.buf_len == 0 {
                    merge_one_level::<_, _, true, true>(imit, imit.add(ctrl.imit_len), data, ctrl.seq_len, &mut seqdiv, p, f);
                } else {
                    merge_one_level::<_, _, false, true>(imit, imit.add(ctrl.imit_len), data, ctrl.seq_len, &mut seqdiv, p, f);
                }
            } else {
                let mut seqdiv = SequenceDivider::<false>::new(ctrl.data_len, ctrl.log2_num_seqs);
                merge_one_level::<_, _, true, false>(imit, last, last.sub(ctrl.buf_len), ctrl.seq_len, &mut seqdiv, p, f);
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
