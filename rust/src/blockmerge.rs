use crate::merge::{merge_with_buf, merge_without_buf};
use crate::utils::{Pos, rotate, strict_flag};

unsafe fn interleave_blocks<T, P: Pos<T>, F: FnMut(&T, &T) -> bool>(
    imit: P,
    blocks: P,
    imit_len: usize,
    block_len: usize,
    f: &mut F,
) -> P {
    unsafe {
        if imit_len == 0 {
            return imit;
        }

        let swap_block = |mut a: P, mut b: P| {
            if a == b {
                return;
            }
            let mut i = block_len;
            loop {
                a.swap(b);
                a = a.add(1);
                b = b.add(1);
                i -= 1;
                if i == 0 {
                    break;
                }
            }
        };

        let mut left_keys = imit;
        let mut right_keys = imit.add(imit_len / 2);
        let mut left_blocks = blocks;
        let mut right_blocks = left_blocks.add(imit_len / 2 * block_len);

        let mut least_left_key = left_keys;
        let mut least_left_block = left_blocks;
        let mut least_right_key = right_keys;
        let orig_right_key = right_keys;
        let last_right_key = right_keys.add(imit_len / 2);

        loop {
            if right_keys == last_right_key || !right_blocks.precede_strict(least_left_block, f) {
                left_keys.swap(least_left_key);
                swap_block(left_blocks, least_left_block);

                left_keys = left_keys.add(1);
                left_blocks = left_blocks.add(block_len);
                if left_keys == right_keys {
                    break;
                }

                least_left_key = left_keys;
                least_left_block = left_blocks;
                let mut key = if left_keys < orig_right_key {
                    orig_right_key
                } else {
                    left_keys.add(1)
                };
                while key < right_keys {
                    if key.precede_strict(least_left_key, f) {
                        least_left_key = key;
                    }
                    key = key.add(1);
                }
                least_left_block = least_left_block.add((least_left_key.offset_from_unsigned(left_keys)) * block_len);
            } else {
                left_keys.swap(right_keys);
                swap_block(left_blocks, right_blocks);

                if left_keys == least_left_key {
                    least_left_key = right_keys;
                    least_left_block = right_blocks;
                }
                if right_keys == least_right_key {
                    least_right_key = left_keys;
                }

                left_keys = left_keys.add(1);
                right_keys = right_keys.add(1);
                left_blocks = left_blocks.add(block_len);
                right_blocks = right_blocks.add(block_len);
            }
        }

        least_right_key
    }
}

unsafe fn deinterleave_imitation<T, P: Pos<T>, F: FnMut(&T, &T) -> bool>(
    imit: P,
    imit_len: usize,
    mut buf: P,
    mut mid_key: P,
    f: &mut F,
) {
    unsafe {
        if imit_len == 0 {
            return;
        }

        mid_key.swap(buf);
        let mut left_cur = mid_key;
        let mut right_cur = buf.add(1);
        let mut cur = mid_key.add(1);
        mid_key = buf;

        while cur != imit.add(imit_len) {
            if cur.precede_strict(mid_key, f) {
                left_cur.swap(cur);
                left_cur = left_cur.add(1);
                cur = cur.add(1);
            } else {
                right_cur.swap(cur);
                right_cur = right_cur.add(1);
                cur = cur.add(1);
            }
        }

        loop {
            left_cur.swap(buf);
            left_cur = left_cur.add(1);
            buf = buf.add(1);
            if buf == right_cur {
                break;
            }
        }
    }
}

unsafe fn deinterleave_imitation_nobuf<T, P: Pos<T>, F: FnMut(&T, &T) -> bool>(
    imit: P,
    imit_len: usize,
    mut mid_key: P,
    f: &mut F,
) {
    unsafe {
        if imit_len == 0 {
            return;
        }

        let mut l_runlength = 0usize;
        let mut num_rl_pairs = imit_len;

        loop {
            let mut r_runlength = 0;
            let old_num_rl_pairs = num_rl_pairs;
            num_rl_pairs = 0;

            let mut cur = imit;
            loop {
                if cur == imit.add(imit_len) || !cur.precede_strict(mid_key, f) {
                    if l_runlength != 0 {
                        num_rl_pairs += 1;
                        if num_rl_pairs % 2 != 0 {
                            let l_run = cur.sub(l_runlength);
                            let r_run = l_run.sub(r_runlength);
                            rotate(r_run, l_run, cur);
                            if num_rl_pairs == 1 {
                                mid_key = cur.sub(r_runlength);
                            }
                        }
                        l_runlength = 0;
                        r_runlength = 0;
                    }
                    if cur == imit.add(imit_len) {
                        break;
                    }
                    r_runlength += 1;
                } else {
                    l_runlength += (r_runlength != 0) as usize;
                }
                cur = cur.add(1);
            }
            if num_rl_pairs <= 1 || num_rl_pairs >= old_num_rl_pairs {
                break;
            }
        }
    }
}

pub struct BlockingParam {
    pub num_blocks: usize,
    pub block_len: usize,
    pub first_block_len: usize,
    pub last_block_len: usize,
}

unsafe fn merge_adjacent_blocks<T, P: Pos<T>, F: FnMut(&T, &T) -> bool, const HAS_BUF: bool>(
    mut imit: P,
    buf: &mut P,
    blocks: P,
    p: &BlockingParam,
    mid_key: P,
    f: &mut F,
) {
    unsafe {
        let mut num_remained_blocks = p.num_blocks;

        #[derive(PartialEq, Eq)]
        enum BlockOrigin {
            Left,
            Right,
        }

        let mut xs = blocks;
        let mut last_block_before_ys = xs;
        let mut xs_origin = BlockOrigin::Left;
        num_remained_blocks -= 1;

        let mut ys = xs.add(p.first_block_len);

        loop {
            let ys_last = ys.add(if num_remained_blocks - 1 == 0 {
                p.block_len
            } else {
                p.last_block_len
            });
            let mut ys_origin = BlockOrigin::Left;
            if num_remained_blocks != 0 {
                imit = imit.add(1);
                if !imit.precede_strict(mid_key, f) {
                    ys_origin = BlockOrigin::Right;
                }
            }
            if ys_origin == xs_origin {
                last_block_before_ys = ys;
                ys = ys_last;
                continue;
            }

            if xs != last_block_before_ys {
                if HAS_BUF {
                    if num_remained_blocks != 0 {
                        loop {
                            buf.swap(xs);
                            *buf = buf.add(1);
                            xs = xs.add(1);
                            if xs == last_block_before_ys.add(1) {
                                break;
                            }
                        }
                    }
                } else {
                    if num_remained_blocks != 0 {
                        xs = last_block_before_ys.add(1)
                    } else if ys.offset_from_unsigned(xs) > p.last_block_len {
                        rotate(xs, ys, ys_last);
                        ys = xs.add(p.last_block_len);
                        xs_origin = BlockOrigin::Right;
                    }
                }
            }

            let mr = if HAS_BUF {
                if xs_origin == BlockOrigin::Left {
                    merge_with_buf(buf, xs, ys, ys_last, f, strict_flag::<false>())
                } else {
                    merge_with_buf(buf, xs, ys, ys_last, f, strict_flag::<true>())
                }
            } else {
                if xs_origin == BlockOrigin::Left {
                    merge_without_buf(xs, ys, ys_last, f, strict_flag::<false>())
                } else {
                    merge_without_buf(xs, ys, ys_last, f, strict_flag::<true>())
                }
            };

            xs = mr.rest;
            last_block_before_ys = xs;
            xs_origin = match (xs_origin, mr.xs_consumed) {
                (BlockOrigin::Right, false) => BlockOrigin::Right,
                (BlockOrigin::Left, false) => BlockOrigin::Left,
                (BlockOrigin::Right, true) => BlockOrigin::Left,
                (BlockOrigin::Left, true) => BlockOrigin::Right,
            };

            ys = ys_last;
            if num_remained_blocks == 0 {
                break;
            }
        }

        if HAS_BUF {
            while xs != ys {
                buf.swap(xs);
                *buf = buf.add(1);
                xs = xs.add(1);
            }
        }
    }
}

pub unsafe fn merge_blocking<T, P: Pos<T>, F: FnMut(&T, &T) -> bool, const HAS_BUF: bool>(
    imit: P,
    buf: &mut P,
    blocks: P,
    p: &BlockingParam,
    f: &mut F,
) {
    unsafe {
        let imit_len = p.num_blocks - 2;
        let mid_key = interleave_blocks(imit, blocks.add(p.first_block_len), imit_len, p.block_len, f);

        merge_adjacent_blocks::<_, _, _, HAS_BUF>(imit, buf, blocks, p, mid_key, f);

        if HAS_BUF {
            deinterleave_imitation(imit, imit_len, *buf, mid_key, f);
        } else {
            deinterleave_imitation_nobuf(imit, imit_len, mid_key, f);
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::test_util::test_rng;
    use crate::utils::FwdPtr;

    /// Reference implementation with indices. Rewrites `ary` and returns the index of the mid key.
    fn naive_interleave_blocks(
        ary: &mut [i32],
        imit: usize,
        blocks: usize,
        num_blocks: usize,
        block_len: usize,
    ) -> usize {
        let half_len = num_blocks / 2 * block_len;
        let (mut xs, xs_last) = (blocks, blocks + half_len);
        let (mut ys, ys_last) = (xs_last, xs_last + half_len);
        let mut x_key = imit;
        let mut y_key = imit + num_blocks / 2;
        let mid_val = ary[y_key];

        let mut imit_space = Vec::new();
        let mut merged_space = Vec::new();
        while xs != xs_last || ys != ys_last {
            if ys == ys_last || (xs != xs_last && !(ary[ys] < ary[xs])) {
                imit_space.push(ary[x_key]);
                x_key += 1;
                merged_space.extend_from_slice(&ary[xs..xs + block_len]);
                xs += block_len;
            } else {
                imit_space.push(ary[y_key]);
                y_key += 1;
                merged_space.extend_from_slice(&ary[ys..ys + block_len]);
                ys += block_len;
            }
        }

        ary[imit..imit + imit_space.len()].copy_from_slice(&imit_space);
        ary[blocks..blocks + merged_space.len()].copy_from_slice(&merged_space);

        let mut mid_key = imit;
        while ary[mid_key] != mid_val {
            mid_key += 1;
        }
        mid_key
    }

    #[test]
    fn interleave_blocks_test() {
        let ary_len = 32;
        let block_len = 3;
        let mut rng = test_rng();

        for num_blocks in (0..=8).step_by(2) {
            for pad in 0..ary_len - (num_blocks + num_blocks * block_len) {
                let imit = 0;
                let blocks = imit + num_blocks + pad;
                let half_len = num_blocks / 2 * block_len;

                let mut ary = vec![42; ary_len];
                for (v, i) in ary[imit..imit + num_blocks].iter_mut().zip(0..) {
                    *v = i;
                }
                for range in [blocks..blocks + half_len, blocks + half_len..blocks + 2 * half_len] {
                    // Values in [a, a + 40]
                    let a = if rng.next_u64() % 2 != 0 { 70 } else { 90 };
                    for v in &mut ary[range.clone()] {
                        *v = a + rng.below(41) as i32;
                    }
                    ary[range].sort();
                }

                let mut expected = ary.clone();
                let mid_key_expected = naive_interleave_blocks(&mut expected, imit, blocks, num_blocks, block_len);

                let base = FwdPtr::new(ary.as_mut_ptr());
                let mid_key = unsafe {
                    let mid_key = interleave_blocks(
                        base.add(imit),
                        base.add(blocks),
                        num_blocks,
                        block_len,
                        &mut |a: &i32, b: &i32| a < b,
                    );
                    mid_key.offset_from_unsigned(base)
                };

                assert_eq!(ary, expected, "num_blocks={num_blocks} pad={pad}");
                assert_eq!(mid_key, mid_key_expected, "num_blocks={num_blocks} pad={pad}");
            }
        }
    }

    #[test]
    fn deinterleave_imitation_test() {
        let ary_len = 48;
        let mut rng = test_rng();

        for use_buf in [true, false] {
            let mut imit_len = 0;
            while imit_len + imit_len / 2 < ary_len {
                let buf_len = imit_len / 2;
                let imit = 0;
                let imit_last = imit + imit_len;
                let buf = ary_len - buf_len;

                let mut ary: Vec<i32> = (0..ary_len as i32).collect();
                let mut vals: Vec<i32> = (0..imit_len as i32).collect();
                rng.shuffle(&mut vals);
                vals[..imit_len / 2].sort();
                vals[imit_len / 2..].sort();
                ary[imit..imit_last].sort_by_key(|&x| vals[x as usize]);

                let mid_key = imit + ary.iter().position(|&x| x == (imit_len / 2) as i32).unwrap();
                ary[imit_last..buf].fill(100);
                let less = &mut |a: &i32, b: &i32| a < b;
                if use_buf {
                    let base = FwdPtr::new(ary.as_mut_ptr());
                    unsafe { deinterleave_imitation(base.add(imit), imit_len, base.add(buf), base.add(mid_key), less) };
                }
                ary[buf..].fill(200);
                if !use_buf {
                    let base = FwdPtr::new(ary.as_mut_ptr());
                    unsafe { deinterleave_imitation_nobuf(base.add(imit), imit_len, base.add(mid_key), less) };
                }

                let mut expected: Vec<i32> = (0..imit_len as i32).collect();
                expected.resize(ary_len - buf_len, 100);
                expected.resize(ary_len, 200);

                assert_eq!(ary, expected, "use_buf={use_buf} imit_len={imit_len}");
                imit_len += 2;
            }
        }
    }

    #[test]
    fn deinterleave_imitation_inconsistent_comparator_test() {
        let imit_len = 256;
        let mut rng = test_rng();
        let random_less = &mut |_: &i32, _: &i32| rng.next_u64() % 2 != 0;

        for _ in 0..100 {
            let mut ary: Vec<i32> = (0..imit_len as i32).collect();
            let base = FwdPtr::new(ary.as_mut_ptr());
            // Terminates, permuting the items
            unsafe { deinterleave_imitation_nobuf(base, imit_len, base.add(imit_len / 2), random_less) };
            ary.sort();
            assert!(ary.iter().copied().eq(0..imit_len as i32));
        }
    }
}
