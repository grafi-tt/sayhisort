use std::cmp::Ordering;

#[derive(Clone, Copy)]
#[repr(transparent)]
pub struct StrictHolder<const flag: bool>;
pub trait StrictFlag : Copy {
    const FLAG: bool;
}
impl StrictFlag for StrictHolder<false> {
    const FLAG: bool = false;
}
impl StrictFlag for StrictHolder<true> {
    const FLAG: bool = true;
}
pub fn strict_flag<const flag: bool>() -> StrictHolder<flag> { StrictHolder::<flag>{} }

pub trait Pos<T>: Copy + Ord {
    unsafe fn add(self, d: usize) -> Self;
    unsafe fn sub(self, d: usize) -> Self;
    unsafe fn offset_from_unsigned(self, origin: Self) -> usize;
    unsafe fn swap(self, other: Self);
    unsafe fn precede<F: FnMut(&T, &T) -> bool, S: StrictFlag>(self, other: Self, f: &mut F, strict: S) -> bool;
}

#[repr(transparent)]
pub struct FwdPtr<T> {
    ptr: *mut T,
}

impl<T> Clone for FwdPtr<T> {
    fn clone(&self) -> Self { *self }
}
impl<T> Copy for FwdPtr<T> {}

impl<T> PartialEq for FwdPtr<T> {
    fn eq(&self, other: &Self) -> bool { self.ptr == other.ptr }
}
impl<T> Eq for FwdPtr<T> {}

impl<T> Ord for FwdPtr<T> {
    fn cmp(&self, other: &Self) -> Ordering {
        self.ptr.cmp(other.ptr)
    }
}
impl<T> PartialOrd for FwdPtr<T> {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}

impl<T> Pos<T> for FwdPtr<T> {
    unsafe fn add(self, d: usize) -> Self { FwdPtr{ptr: unsafe{self.ptr.add(d)}} }
    unsafe fn sub(self, d: usize) -> Self { FwdPtr{ptr: unsafe{self.ptr.sub(d)}} }
    unsafe fn offset_from_unsigned(self, origin: Self) -> usize { unsafe{self.ptr.offset_from_unsigned(origin.ptr)} }
    unsafe fn swap(self, other: Self) { unsafe{std::ptr::swap(self.ptr, other.ptr)} }
    unsafe fn precede<F: FnMut(&T, &T) -> bool, S: StrictFlag>(self, other: Self, f: &mut F, strict: S) -> bool {
        unsafe {
            if S::FLAG {
                f(&*self.ptr, &*other.ptr)
            } else {
                !f(&*other.ptr, &*self.ptr)
            }
        }
    }
}

#[repr(transparent)]
pub struct BwdPtr<T> {
    ptr: *mut T,
}

impl<T> Clone for BwdPtr<T> {
    fn clone(&self) -> Self { *self }
}
impl<T> Copy for BwdPtr<T> {}

impl<T> PartialEq for BwdPtr<T> {
    fn eq(&self, other: &Self) -> bool { self.ptr == other.ptr }
}
impl<T> Eq for BwdPtr<T> {}

impl<T> Ord for BwdPtr<T> {
    fn cmp(&self, other: &Self) -> Ordering {
        other.ptr.cmp(self.ptr)
    }
}
impl<T> PartialOrd for BwdPtr<T> {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}

impl<T> Pos<T> for BwdPtr<T> {
    unsafe fn add(self, d: usize) -> Self { BwdPtr{ptr: unsafe{self.ptr.sub(d)}} }
    unsafe fn sub(self, d: usize) -> Self { BwdPtr{ptr: unsafe{self.ptr.add(d)}} }
    unsafe fn offset_from_unsigned(self, origin: Self) -> usize { unsafe{origin.ptr.offset_from_unsigned(self.ptr)} }
    unsafe fn swap(self, other: Self) { unsafe{std::ptr::swap(self.ptr.sub(1), other.ptr.sub(1))} }
    unsafe fn precede<F: FnMut(&T, &T) -> bool, S: StrictFlag>(self, other: Self, f: &mut F, strict: S) -> bool {
        unsafe {
            if S::FLAG {
                f(&*other.ptr.sub(1), &*self.ptr.sub(1))
            } else {
                !f(&*self.ptr.sub(1), &*other.ptr.sub(1))
            }
        }
    }
}

pub unsafe fn rotate<T, P: Pos<T>>(mut first: P, mut middle: P, mut last: P) {
    unsafe {
        let mut l_len = middle.offset_from_unsigned(first);
        let mut r_len = last.offset_from_unsigned(middle);
        let mut len = l_len + r_len;

        while len > 64 {
            if l_len <= r_len {
                let rem = r_len % l_len;
                loop {
                    first.swap(middle);
                    first = first.add(1);
                    middle = middle.add(1);
                    if middle == last {
                        break;
                    }
                }
                if rem != 0 {
                    return;
                }
                middle = last.sub(rem);
                len = l_len;
                l_len = l_len - rem;
                r_len = rem;
            } else {
                let rem = l_len % r_len;
                loop {
                    last = last.sub(1);
                    middle = middle.sub(1);
                    last.swap(middle);
                    if middle == first {
                        break;
                    }
                }
                if rem != 0 {
                    return;
                }
                middle = first.add(rem);
                len = r_len;
                r_len = r_len - rem;
                l_len = rem;
            }
        }

        let mut f = first;
        let mut m = middle;
        let mut l = last;
        loop {
            m = m.sub(1);
            if f >= m {
                break;
            }
            f.swap(m);
            f = f.add(1);
        }
        loop {
            l = l.sub(1);
            if middle >= l {
                break;
            }
            middle.swap(l);
            middle = middle.add(1);
        }
        loop {
            last = last.sub(1);
            if first >= last {
                break;
            }
            first.swap(last);
            first = first.add(1);
        }
    }
}

pub unsafe fn binary_search<T, P: Pos<T>, F: FnMut(&T, &T) -> bool, S: StrictFlag>(first: P, last: P, key: P, f: &mut F, strict: S) -> P {
    unsafe {
        let mut base = first;
        let mut len = last.offset_from_unsigned(first) + 1;

        loop {
            let mid = len / 2;
            if mid == 0 {
                break;
            }
            let pivot = base.add(mid);
            if pivot.sub(1).precede(key, f, strict) {
                base = pivot;
            }
            len -= mid;
        }
        base
    }
}
