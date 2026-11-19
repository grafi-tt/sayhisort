use crate::utils::{Pos, StrictFlag, binary_search, rotate};

pub struct MergeResult<P> {
    pub xs_consumed: bool,
    pub rest: P,
}

pub unsafe fn merge_with_buf<T, P: Pos<T>, F: FnMut(&T, &T) -> bool, S: StrictFlag>(
    buf: &mut P,
    mut xs: P,
    mut ys: P,
    ys_last: P,
    f: &mut F,
    is_xs_from_right: S,
) -> MergeResult<P> {
    unsafe {
        let mut xs_last = ys;

        while xs < xs_last.sub(1) && ys < ys_last.sub(1) {
            if xs.add(1).precede(ys, f, is_xs_from_right) {
                buf.swap(xs);
                *buf = buf.add(1);
                xs = xs.add(1);
                buf.swap(xs);
                *buf = buf.add(1);
                xs = xs.add(1);
            } else if !xs.precede(ys.add(1), f, is_xs_from_right) {
                buf.swap(ys);
                *buf = buf.add(1);
                ys = ys.add(1);
                buf.swap(ys);
                *buf = buf.add(1);
                ys = ys.add(1);
            } else {
                let y_pos = xs.precede(ys, f, is_xs_from_right);
                buf.add(!y_pos as usize).swap(xs);
                buf.add(y_pos as usize).swap(ys);
                *buf = buf.add(2);
                xs = xs.add(1);
                ys = ys.add(1);
            }
        }

        let mut xs_consumed = xs == xs_last;

        if xs == xs_last.sub(1) {
            xs_consumed = false;
            loop {
                if xs.precede(ys, f, is_xs_from_right) {
                    buf.swap(xs);
                    *buf = buf.add(1);
                    xs = xs.add(1);
                    xs_consumed = true;
                    break;
                }
                buf.swap(ys);
                *buf = buf.add(1);
                ys = ys.add(1);
                if ys == ys_last {
                    break;
                }
            }
        } else if ys == ys_last.sub(1) {
            xs_consumed = true;
            loop {
                if !xs.precede(ys, f, is_xs_from_right) {
                    buf.swap(ys);
                    *buf = buf.add(1);
                    ys = ys.add(1);
                    xs_consumed = false;
                    break;
                }
                buf.swap(xs);
                *buf = buf.add(1);
                xs = xs.add(1);
                if xs == xs_last {
                    break;
                }
            }
        }

        if xs_consumed {
            return MergeResult {
                xs_consumed: true,
                rest: ys,
            };
        }

        loop {
            ys = ys.sub(1);
            xs_last = xs_last.sub(1);
            ys.swap(xs_last);
            if xs_last == xs {
                break;
            }
        }
        MergeResult {
            xs_consumed: false,
            rest: ys,
        }
    }
}

pub unsafe fn merge_without_buf<T, P: Pos<T>, F: FnMut(&T, &T) -> bool, S: StrictFlag>(
    mut xs: P,
    mut ys: P,
    ys_last: P,
    f: &mut F,
    is_xs_from_right: S,
) -> MergeResult<P> {
    unsafe {
        loop {
            xs = binary_search(xs, ys, ys, f, is_xs_from_right);
            if xs == ys {
                return MergeResult {
                    xs_consumed: true,
                    rest: ys,
                };
            }
            let mut ys_upper = ys.add(1);
            if ys_upper != ys_last {
                ys_upper = binary_search(ys_upper, ys_last, xs, f, is_xs_from_right.not());
            }
            rotate(xs, ys, ys_upper);
            xs = xs.add(ys_upper.offset_from_unsigned(ys));
            ys = ys_upper;
            if ys_upper == ys_last {
                return MergeResult {
                    xs_consumed: false,
                    rest: xs,
                };
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::test_util::test_rng;
    use crate::utils::{FwdPtr, strict_flag};

    struct NaiveResult {
        buf: usize,
        xs_consumed: bool,
        rest: usize,
        expected: Vec<i32>,
    }

    /// Reference implementation with indices. Returns the expected array as well.
    fn naive_merge_with_buf(ary: &[i32], buf: usize, mut xs: usize, mut ys: usize, ys_last: usize) -> NaiveResult {
        let len = ys_last - buf;
        let xs_last = ys;

        let mut merged = Vec::new();
        while xs < xs_last && ys < ys_last {
            if !(ary[ys] < ary[xs]) {
                merged.push(ary[xs]);
                xs += 1;
            } else {
                merged.push(ary[ys]);
                ys += 1;
            }
        }
        let xs_consumed = xs == xs_last;

        let mut rest: Vec<i32> = ary[xs..xs_last].to_vec();
        rest.extend_from_slice(&ary[ys..ys_last]);
        let rest_offset = len - rest.len();

        let mut expected = merged.clone();
        expected.resize(rest_offset, 0);
        expected.extend_from_slice(&rest);
        expected.resize(ary.len(), 42);

        NaiveResult {
            buf: buf + merged.len(),
            xs_consumed,
            rest: buf + rest_offset,
            expected,
        }
    }

    #[test]
    fn merge_with_buf_test() {
        let ary_len = 32;
        let buf_len = 8;
        let mut rng = test_rng();

        for ys_len in 1..=buf_len {
            for xs_len in 1..=ary_len - (buf_len + ys_len) {
                let xs = buf_len;
                let ys = xs + xs_len;
                let ys_last = ys + ys_len;

                let mut ary = vec![0; ary_len];
                for (v, i) in ary[xs..ys_last].iter_mut().zip(100..) {
                    *v = i;
                }
                ary[ys_last..].fill(42);
                rng.shuffle(&mut ary[xs..ys_last]);
                ary[xs..ys].sort();
                ary[ys..ys_last].sort();

                let naive = naive_merge_with_buf(&ary, 0, xs, ys, ys_last);

                let base = FwdPtr::new(ary.as_mut_ptr());
                let (buf, xs_consumed, rest) = unsafe {
                    let mut buf = base;
                    let result = merge_with_buf(
                        &mut buf,
                        base.add(xs),
                        base.add(ys),
                        base.add(ys_last),
                        &mut |a: &i32, b: &i32| a < b,
                        strict_flag::<false>(),
                    );
                    (
                        buf.offset_from_unsigned(base),
                        result.xs_consumed,
                        result.rest.offset_from_unsigned(base),
                    )
                };

                assert_eq!(ary, naive.expected, "xs_len={xs_len} ys_len={ys_len}");
                assert_eq!(buf, naive.buf, "xs_len={xs_len} ys_len={ys_len}");
                assert_eq!(rest, naive.rest, "xs_len={xs_len} ys_len={ys_len}");
                assert_eq!(xs_consumed, naive.xs_consumed, "xs_len={xs_len} ys_len={ys_len}");
            }
        }
    }

    struct NaiveResultWithoutBuf {
        xs_consumed: bool,
        rest: usize,
        expected: Vec<i32>,
    }

    /// Reference implementation with indices. Returns the expected array as well.
    fn naive_merge_without_buf(ary: &[i32], mut xs: usize, mut ys: usize, ys_last: usize) -> NaiveResultWithoutBuf {
        let xs_orig = xs;
        let xs_last = ys;

        let mut merged = Vec::new();
        while xs < xs_last && ys < ys_last {
            if !(ary[ys] < ary[xs]) {
                merged.push(ary[xs]);
                xs += 1;
            } else {
                merged.push(ary[ys]);
                ys += 1;
            }
        }
        let xs_consumed = xs == xs_last;

        let mut expected = ary[..xs_orig].to_vec();
        expected.extend_from_slice(&merged);
        expected.extend_from_slice(&ary[xs..xs_last]);
        expected.extend_from_slice(&ary[ys..ys_last]);
        expected.resize(ary.len(), 42);

        NaiveResultWithoutBuf {
            xs_consumed,
            rest: xs_orig + merged.len(),
            expected,
        }
    }

    #[test]
    fn merge_without_buf_test() {
        let ary_len = 24;
        let mut rng = test_rng();

        for ys_len in 1..=ary_len {
            for xs_len in 1..=ary_len - ys_len {
                let xs = 0;
                let ys = xs + xs_len;
                let ys_last = ys + ys_len;

                let mut ary = vec![0; ary_len];
                for (v, i) in ary[xs..ys_last].iter_mut().zip(100..) {
                    *v = i;
                }
                ary[ys_last..].fill(42);
                rng.shuffle(&mut ary[xs..ys_last]);
                ary[xs..ys].sort();
                ary[ys..ys_last].sort();

                let naive = naive_merge_without_buf(&ary, xs, ys, ys_last);

                let base = FwdPtr::new(ary.as_mut_ptr());
                let (xs_consumed, rest) = unsafe {
                    let result = merge_without_buf(
                        base.add(xs),
                        base.add(ys),
                        base.add(ys_last),
                        &mut |a: &i32, b: &i32| a < b,
                        strict_flag::<false>(),
                    );
                    (result.xs_consumed, result.rest.offset_from_unsigned(base))
                };

                assert_eq!(ary, naive.expected, "xs_len={xs_len} ys_len={ys_len}");
                assert_eq!(rest, naive.rest, "xs_len={xs_len} ys_len={ys_len}");
                assert_eq!(xs_consumed, naive.xs_consumed, "xs_len={xs_len} ys_len={ys_len}");
            }
        }
    }
}
