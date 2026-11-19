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
                xs = ys.add(1);
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
