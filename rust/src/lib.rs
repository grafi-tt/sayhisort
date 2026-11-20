//! A fast, in-place and stable block merge sort.
//!
//! SayhiSort sorts a slice in O(*n* \* log(*n*)) worst-case time, without allocating any memory. It's a port of the
//! C++ implementation in the same repository, and shares the same algorithm.
//!
//! The functions are drop-in replacements for [`slice::sort`] and [`slice::sort_by`]:
//!
//! * The crate is `#![no_std]` and never allocates memory, whereas [`slice::sort`] requires a heap allocator for its
//!   working buffer.
//! * Items need not be [`Copy`], [`Clone`] or [`Default`], and zero-sized types are supported.
//! * Memory safety is guaranteed even when the comparator is inconsistent or panics. The slice is left as a
//!   permutation of the input, without double drop or leak.
//!
//! # Examples
//!
//! ```
//! let mut v = [5, 4, 1, 3, 2];
//! sayhisort::sort(&mut v);
//! assert_eq!(v, [1, 2, 3, 4, 5]);
//! ```
//!
//! [`slice::sort`]: https://doc.rust-lang.org/std/primitive.slice.html#method.sort
//! [`slice::sort_by`]: https://doc.rust-lang.org/std/primitive.slice.html#method.sort_by
#![cfg_attr(not(test), no_std)]
#![warn(missing_docs)]

use core::cmp::Ordering;
use core::mem::size_of;

// The internal functions are ports of the C++ functions with the corresponding CamelCase names in `cpp/sayhisort.h`,
// which is the canonical source of algorithm descriptions and proofs. The Rust doc comments only state what each
// function does and, for an `unsafe fn`, its preconditions in `# Safety`. In addition to them, every position passed
// to an `unsafe fn` must point into the slice being sorted (or one past its end).
mod blocking;
mod blockmerge;
mod merge;
mod seqdiv;
mod smallsort;
#[cfg(test)]
mod test_util;
mod top;
mod utils;

/// Sorts the slice in ascending order, preserving the initial order of equal elements.
///
/// This sort is stable (i.e., does not reorder equal elements), in-place (i.e., does not allocate), and
/// *O*(*n* \* log(*n*)) worst-case.
///
/// If the implementation of [`Ord`] for `T` does not implement a total order, the resulting order is unspecified,
/// but the slice is still a permutation of the input. If it panics, the slice is likewise left as a permutation of
/// the input.
///
/// # Examples
///
/// ```
/// let mut v = [4, -5, 1, -3, 2];
///
/// sayhisort::sort(&mut v);
/// assert_eq!(v, [-5, -3, 1, 2, 4]);
/// ```
#[inline(always)]
pub fn sort<T: Ord>(v: &mut [T]) {
    sort_impl(v, &mut |a, b| a.lt(b))
}

/// Sorts the slice with a comparison function, preserving the initial order of equal elements.
///
/// This sort is stable (i.e., does not reorder equal elements), in-place (i.e., does not allocate), and
/// *O*(*n* \* log(*n*)) worst-case.
///
/// If `compare` does not implement a total order, the resulting order is unspecified, but the slice is still a
/// permutation of the input. If `compare` panics, the slice is likewise left as a permutation of the input.
///
/// # Examples
///
/// ```
/// let mut v = [4, -5, 1, -3, 2];
///
/// // Reverse sorting
/// sayhisort::sort_by(&mut v, |a, b| b.cmp(a));
/// assert_eq!(v, [4, 2, 1, -3, -5]);
///
/// // Equal elements keep their initial order
/// let mut v = [(2, 'a'), (1, 'b'), (2, 'c'), (1, 'd')];
/// sayhisort::sort_by(&mut v, |a, b| a.0.cmp(&b.0));
/// assert_eq!(v, [(1, 'b'), (1, 'd'), (2, 'a'), (2, 'c')]);
/// ```
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
