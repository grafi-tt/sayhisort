use crate::utils::{binary_search, Pos, rotate};

pub struct MergeResult<T, P: Pos<T>> {
    pub xs_consumed: bool,
    pub rest: P,
}

pub unsafe fn merge_with_buf<T, P: Pos<T>, F: FnMut(&T, &T) -> bool, const is_xs_from_right: bool>(buf: &mut P, mut xs: P, mut ys: P, ys_last: P, f: &mut F) -> MergeResult<T, P> {
    unsafe {
        let mut xs_last = ys;

        while xs < xs_last.sub(1) && ys < ys_last.sub(1) {
            if xs.add(1).precede::<is_xs_from_right>(ys, f) {
                buf.swap(xs);
                *buf = buf.add(1);
                xs = xs.add(1);
                buf.swap(xs);
                *buf = buf.add(1);
                xs = xs.add(1);
            } else if !xs.precede::<is_xs_from_right>(ys.add(1), f) {
                buf.swap(ys);
                *buf = buf.add(1);
                ys = ys.add(1);
                buf.swap(ys);
                *buf = buf.add(1);
                ys = ys.add(1);
            } else {
                let y_pos = xs.precede::<is_xs_from_right>(ys, f);
                (buf + !y_pos as usize).swap(xs);
                (buf + y_pos as usize).swap(ys);
                *buf = buf.add(2);
                xs = ys.add(1);
                ys = ys.add(1);
            }
        }

        let mut xs_consumed = xs == xs_last;

        if xs == xs_last.sub(1) {
            xs_consumed = false;
            loop {
                if xs.precede::<is_xs_from_right>(ys, f) {
                    buf.swap(xs);
                    buf = buf.add(1);
                    xs = xs.add(1);
                    xs_consumed = true;
                    break;
                }
                buf.swap(ys);
                buf = buf.add(1);
                ys = ys.add(1);
                if ys == ys_last {
                    break;
                }
            }

        } else if ys == ys_last.sub(1) {
            xs_consumed = true;
            loop {
                if !xs.precede::<is_xs_from_right>(ys, f) {
                    buf.swap(ys);
                    buf = buf.add(1);
                    ys = ys.add(1);
                    xs_consumed = false;
                    break;
                }
                buf.swap(xs);
                buf = buf.add(1);
                xs = xs.add(1);
                if xs == xs_last {
                    break;
                }
            }
        }

        if xs_consumed {
            return MergeResult{xs_consumed: true, rest: ys};
        }

        loop {
            ys = ys.sub(1);
            xs_last = xs_last.sub(1);
            ys.swap(xs_last);
            if xs_last == xs {
                break;
            }
        }
        MergeResult{xs_consumed: false, rest: ys}
    }
}

pub unsafe fn merge_without_buf<T, P: Pos<T>, F: FnMut(&T, &T) -> bool, const is_xs_from_right: bool>(mut xs: P, mut ys: P, ys_last: P, f: &mut F) -> MergeResult<T, P> {
    loop {
        let xs = binary_search::<is_xs_from_right>(xs, ys, ys, f);
        if xs == ys {
            return MergeResult{xs_consumed: true, rest: ys};
        }
        let mut ys_upper = ys.add(1);
        if ys_upper != ys_last {
            ys_upper = binary_search::<!is_xs_from_right>(ys_upper, ys_last, xs, f);
        }
        rotate(xs, ys, ys_upper);
        xs = xs.add(ys_upper.offset_from_unsigned(ys));
        ys = ys_upper;
        if ys_upper == ys_last {
            return MergeResult{xs_consumed: false, rest: xs};
        }
    }
}
