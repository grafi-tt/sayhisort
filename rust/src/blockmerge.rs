use crate::merge::{merge_with_buf, merge_without_buf};
use crate::utils::{Pos, rotate, strict_flag};

unsafe fn interleave_blocks<T, P: Pos<T>, F: FnMut(&T, &T) -> bool>(imit: P, blocks: P, imit_len: usize, block_len: usize, f: &mut F) -> P {
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
            if right_keys == last_right_key || !right_blocks.precede(least_left_block, f, strict_flag::<true>()) {
                left_keys.swap(least_left_key);
                swap_block(left_blocks, least_left_block);

                left_keys = left_keys.add(1);
                left_blocks = left_blocks.add(block_len);
                if left_keys == right_keys {
                    break;
                }

                least_left_key = left_keys;
                least_left_block = left_blocks;
                let mut key = if left_keys < orig_right_key {orig_right_key} else {left_keys.add(1)};
                while key < right_keys {
                    if key.precede(least_left_key, f, strict_flag::<true>()) {
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

unsafe fn deinterleave_imitation<T, P: Pos<T>, F: FnMut(&T, &T) -> bool>(imit: P, imit_len: usize, mut buf: P, mut mid_key: P, f: &mut F) {
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
            if cur.precede(mid_key, f, strict_flag::<true>()) {
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

unsafe fn deinterleave_imitation_nobuf<T, P: Pos<T>, F: FnMut(&T, &T) -> bool>(imit: P, imit_len: usize, mut mid_key: P, f: &mut F) {
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
                if cur == imit.add(imit_len) || !cur.precede(mid_key, f, strict_flag::<true>()) {
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

unsafe fn merge_adjacent_blocks<T, P: Pos<T>, F: FnMut(&T, &T) -> bool, const HAS_BUF: bool>(mut imit: P, buf: &mut P, blocks: P, p: &BlockingParam, mid_key: P, f: &mut F) {
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
            let ys_last = ys.add(if num_remained_blocks - 1 == 0 { p.block_len } else {p.last_block_len});
            let mut ys_origin = BlockOrigin::Left;
            if num_remained_blocks != 0 {
                imit = imit.add(1);
                if !imit.precede(mid_key, f, strict_flag::<true>()) {
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

            let mr =
                if HAS_BUF {
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

pub unsafe fn merge_blocking<T, P: Pos<T>, F: FnMut(&T, &T) -> bool, const HAS_BUF: bool>(imit: P, buf: &mut P, blocks: P, p: &BlockingParam, f: &mut F) {
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
