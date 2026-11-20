use core::cmp::Ordering;

/// Type-level boolean, passed as an argument to select a code path at compile time, e.g. strict (`<`) or non-strict
/// (`<=`) comparison, or whether the merge buffer is used.
#[derive(Clone, Copy)]
pub struct Flag<const B: bool>;

/// Implemented by [`Flag`] to read its value and to negate it.
pub trait BoolFlag: Copy {
    const FLAG: bool;
    type Not: BoolFlag;
    fn not(self) -> Self::Not;
}
impl BoolFlag for Flag<true> {
    const FLAG: bool = true;
    type Not = Flag<false>;
    fn not(self) -> Self::Not {
        Flag
    }
}
impl BoolFlag for Flag<false> {
    const FLAG: bool = false;
    type Not = Flag<true>;
    fn not(self) -> Self::Not {
        Flag
    }
}

/// Position in a slice, moving forward ([`FwdPtr`]) or backward ([`BwdPtr`]).
///
/// Generic code written for `Pos` works in both directions, like iterators and `ReversedIterator` in C++. Comparison
/// of positions and of items follows the direction, so a backward scan sees the slice reversed.
pub trait Pos<T>: Copy + Ord {
    unsafe fn add(self, d: usize) -> Self;
    unsafe fn sub(self, d: usize) -> Self;
    unsafe fn offset_from_unsigned(self, origin: Self) -> usize;
    unsafe fn swap(self, other: Self);
    /// Swaps the items at `self` and `other`, then advances both.
    unsafe fn swap_incr(&mut self, other: &mut Self) {
        unsafe {
            self.swap(*other);
            *self = self.add(1);
            *other = other.add(1);
        }
    }
    /// Whether the item at `self` comes before the one at `other`, by `<` if `S` is strict and by `<=` otherwise.
    unsafe fn precede<F: FnMut(&T, &T) -> bool, S: BoolFlag>(self, other: Self, f: &mut F, strict: S) -> bool;
    /// Same as [`Pos::precede`] with strict comparison.
    unsafe fn precede_strict<F: FnMut(&T, &T) -> bool>(self, other: Self, f: &mut F) -> bool {
        unsafe { self.precede(other, f, Flag::<true>) }
    }
}

/// Position moving forward. It points to its item.
#[repr(transparent)]
pub struct FwdPtr<T> {
    ptr: *mut T,
}

impl<T> FwdPtr<T> {
    pub fn new(ptr: *mut T) -> Self {
        FwdPtr { ptr }
    }
    pub fn bwd(self) -> BwdPtr<T> {
        BwdPtr { ptr: self.ptr }
    }
}

impl<T> Clone for FwdPtr<T> {
    fn clone(&self) -> Self {
        *self
    }
}
impl<T> Copy for FwdPtr<T> {}

impl<T> PartialEq for FwdPtr<T> {
    fn eq(&self, other: &Self) -> bool {
        self.ptr == other.ptr
    }
}
impl<T> Eq for FwdPtr<T> {}

impl<T> Ord for FwdPtr<T> {
    fn cmp(&self, other: &Self) -> Ordering {
        self.ptr.cmp(&other.ptr)
    }
}
impl<T> PartialOrd for FwdPtr<T> {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}

impl<T> Pos<T> for FwdPtr<T> {
    unsafe fn add(self, d: usize) -> Self {
        FwdPtr {
            ptr: unsafe { self.ptr.add(d) },
        }
    }
    unsafe fn sub(self, d: usize) -> Self {
        FwdPtr {
            ptr: unsafe { self.ptr.sub(d) },
        }
    }
    unsafe fn swap(self, other: Self) {
        unsafe { core::ptr::swap(self.ptr, other.ptr) }
    }
    unsafe fn offset_from_unsigned(self, origin: Self) -> usize {
        unsafe { self.ptr.offset_from_unsigned(origin.ptr) }
    }
    unsafe fn precede<F: FnMut(&T, &T) -> bool, S: BoolFlag>(self, other: Self, f: &mut F, _strict: S) -> bool {
        unsafe {
            if S::FLAG {
                f(&*self.ptr, &*other.ptr)
            } else {
                !f(&*other.ptr, &*self.ptr)
            }
        }
    }
}

/// Position moving backward. It points one past its item, so the end of a slice is the first position.
#[repr(transparent)]
pub struct BwdPtr<T> {
    ptr: *mut T,
}

impl<T> BwdPtr<T> {
    pub fn fwd(self) -> FwdPtr<T> {
        FwdPtr { ptr: self.ptr }
    }
}

impl<T> Clone for BwdPtr<T> {
    fn clone(&self) -> Self {
        *self
    }
}
impl<T> Copy for BwdPtr<T> {}

impl<T> PartialEq for BwdPtr<T> {
    fn eq(&self, other: &Self) -> bool {
        self.ptr == other.ptr
    }
}
impl<T> Eq for BwdPtr<T> {}

impl<T> Ord for BwdPtr<T> {
    fn cmp(&self, other: &Self) -> Ordering {
        other.ptr.cmp(&self.ptr)
    }
}
impl<T> PartialOrd for BwdPtr<T> {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}

impl<T> Pos<T> for BwdPtr<T> {
    unsafe fn add(self, d: usize) -> Self {
        BwdPtr {
            ptr: unsafe { self.ptr.sub(d) },
        }
    }
    unsafe fn sub(self, d: usize) -> Self {
        BwdPtr {
            ptr: unsafe { self.ptr.add(d) },
        }
    }
    unsafe fn offset_from_unsigned(self, origin: Self) -> usize {
        unsafe { origin.ptr.offset_from_unsigned(self.ptr) }
    }
    unsafe fn swap(self, other: Self) {
        unsafe { core::ptr::swap(self.ptr.sub(1), other.ptr.sub(1)) }
    }
    unsafe fn precede<F: FnMut(&T, &T) -> bool, S: BoolFlag>(self, other: Self, f: &mut F, _strict: S) -> bool {
        unsafe {
            if S::FLAG {
                f(&*other.ptr.sub(1), &*self.ptr.sub(1))
            } else {
                !f(&*self.ptr.sub(1), &*other.ptr.sub(1))
            }
        }
    }
}

/// Rotates two chunks split at `middle`.
///
/// # Safety
///
/// `first < middle < last`.
pub unsafe fn rotate<T, P: Pos<T>>(mut first: P, mut middle: P, mut last: P) {
    unsafe {
        let mut l_len = middle.offset_from_unsigned(first);
        let mut r_len = last.offset_from_unsigned(middle);
        let mut len = l_len + r_len;

        while len > 64 {
            if l_len <= r_len {
                core::hint::assert_unchecked(l_len != 0);
                let rem = r_len % l_len;
                loop {
                    first.swap_incr(&mut middle);
                    if middle == last {
                        break;
                    }
                }
                if rem == 0 {
                    return;
                }
                middle = last.sub(rem);
                len = l_len;
                l_len -= rem;
                r_len = rem;
            } else {
                core::hint::assert_unchecked(r_len != 0);
                let rem = l_len % r_len;
                loop {
                    last = last.sub(1);
                    middle = middle.sub(1);
                    last.swap(middle);
                    if middle == first {
                        break;
                    }
                }
                if rem == 0 {
                    return;
                }
                middle = first.add(rem);
                len = r_len;
                r_len -= rem;
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

/// Searches for `key` in the sorted sequence `[first, last)`.
///
/// Returns `pos` such that, for any `x` in `[first, last)`, `x < pos` iff `x` precedes `key` (by `<` if `S` is strict
/// and by `<=` otherwise).
///
/// # Safety
///
/// `first < last`.
pub unsafe fn binary_search<T, P: Pos<T>, F: FnMut(&T, &T) -> bool, S: BoolFlag>(
    first: P,
    last: P,
    key: P,
    f: &mut F,
    strict: S,
) -> P {
    unsafe {
        let mut base = first;
        let mut len = last.offset_from_unsigned(first) + 1;

        loop {
            let mid = len / 2;
            if mid == 0 {
                break;
            }
            let pivot = base.add(mid);
            // Force a conditional move. The branch is unpredictable, and LLVM may emit it as a jump otherwise.
            base = core::hint::select_unpredictable(pivot.sub(1).precede(key, f, strict), pivot, base);
            len -= mid;
        }
        base
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::ops::Range;

    #[test]
    fn rotate_test() {
        for l in [2, 42, 123] {
            for i in 1..l {
                let mut data: Vec<i32> = (0..l).collect();
                let Range { start, end } = data.as_mut_ptr_range();
                unsafe {
                    rotate(FwdPtr::new(start), FwdPtr::new(start).add(i as usize), FwdPtr::new(end));
                }
                let mut expected: Vec<i32> = (i..l).collect();
                expected.extend(0..i);
                assert_eq!(data, expected);
            }
        }
    }

    #[test]
    fn binary_search_test() {
        let mut data: Vec<i32> = (0..17).collect();
        for i in 1..=16 {
            for j in -1..i {
                data[16] = j;
                let start = data.as_mut_ptr();
                unsafe {
                    let first = FwdPtr::new(start);
                    let last = first.add(16);
                    let middle = first.add(i as usize);
                    let it = binary_search(first, middle, last, &mut |a, b| a.lt(b), Flag::<true>);
                    let idx = it.offset_from_unsigned(first);
                    assert_eq!(idx, std::cmp::max(0, j) as usize);
                    let it2 = binary_search(first, middle, last, &mut |a, b| a.lt(b), Flag::<false>);
                    let idx2 = it2.offset_from_unsigned(first);
                    assert_eq!(idx2, std::cmp::min(j + 1, i) as usize);
                }
            }
        }
    }
}
