#![cfg_attr(not(test), no_std)]

use core::cmp::Ordering;
use core::mem::size_of;

mod blocking;
mod blockmerge;
mod merge;
mod seqdiv;
mod smallsort;
#[cfg(test)]
mod test_util;
mod top;
mod utils;

#[inline(always)]
pub fn sort<T: Ord>(v: &mut [T]) {
    sort_impl(v, &mut |a, b| a.lt(b))
}

#[inline(always)]
pub fn sort_by<T, F: FnMut(&T, &T) -> Ordering>(v: &mut [T], mut compare: F) {
    sort_impl(v, &mut |a, b| compare(a, b) == Ordering::Less);
}

#[inline(always)]
fn sort_impl<T, F: FnMut(&T, &T) -> bool>(v: &mut [T], f: &mut F) {
    if size_of::<T>() == 0 {
        return;
    }
    let range = v.as_mut_ptr_range();
    let first = crate::utils::FwdPtr::new(range.start);
    let last = crate::utils::FwdPtr::new(range.end);
    unsafe {
        crate::top::sort(first, last, f);
    }
}
