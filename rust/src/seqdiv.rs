use core::marker::PhantomData;

use crate::blocking::MergeSortControl;
use crate::blockmerge::{BlockingParam, merge_blocking};
use crate::utils::{BoolFlag, Flag, Pos};

/// Helper to evenly divide data whose length may not be a power of 2. If `D::FLAG` is false, sequences are yielded
/// from the end.
pub struct SequenceDivider<D: BoolFlag = Flag<true>> {
    log2_num_seqs: usize,
    num_seqs: usize,
    remainder: usize,
    frac_counter: usize,
    forward: PhantomData<D>,
}

impl<D: BoolFlag> SequenceDivider<D> {
    pub fn new(data_len: usize, log2_num_seqs: usize) -> Self {
        let mut obj = SequenceDivider {
            log2_num_seqs,
            num_seqs: 1usize << log2_num_seqs,
            remainder: (data_len - 1) % (1usize << log2_num_seqs) + 1,
            frac_counter: 0,
            forward: PhantomData,
        };
        if !D::FLAG {
            obj.remainder = obj.num_seqs - obj.remainder;
        }
        obj
    }

    /// Advances to the next sequence. Returns whether its length is `seq_len - 1` rather than `seq_len`.
    pub fn next(&mut self) -> bool {
        self.frac_counter += self.remainder;
        let mut no_carry = (self.frac_counter & (1usize << self.log2_num_seqs)) == 0;
        if !D::FLAG {
            no_carry = !no_carry;
        }
        self.frac_counter &= (1usize << self.log2_num_seqs) - 1;
        self.num_seqs -= 1;
        no_carry
    }

    /// Whether all sequences have been yielded.
    pub fn is_end(&self) -> bool {
        self.num_seqs == 0
    }
}

/// Merges each pair of adjacent sequences, doubling the sequence length.
///
/// # Safety
///
/// * The layout is `[imit | buf | data]` in the direction of `P`. If `P` is [`BwdPtr`](crate::utils::BwdPtr), the
///   sequences are divided from the end, and `imit` is the start of the reversed imitation buffer.
/// * `p` is computed from `ctrl` by [`determine_blocking`](crate::blocking::determine_blocking).
pub unsafe fn merge_one_level<T, P: Pos<T>, F: FnMut(&T, &T) -> bool, B: BoolFlag>(
    imit: P,
    mut buf: P,
    mut data: P,
    ctrl: &MergeSortControl,
    mut p: BlockingParam,
    f: &mut F,
    has_buf: B,
) {
    unsafe {
        let mut seq_div = SequenceDivider::<P::Forward>::new(ctrl.data_len, ctrl.log2_num_seqs);
        let residual_len = p.first_block_len;
        loop {
            let lseq_decr = seq_div.next();
            let rseq_decr = seq_div.next();
            let merging_len = (ctrl.seq_len - lseq_decr as usize) + (ctrl.seq_len - rseq_decr as usize);
            p.first_block_len = residual_len - lseq_decr as usize;
            p.last_block_len = residual_len - rseq_decr as usize;

            merge_blocking(imit, &mut buf, data, &p, f, has_buf);
            data = data.add(merging_len);
            if seq_div.is_end() {
                break;
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::test_util::test_rng;
    use crate::utils::FwdPtr;

    #[test]
    fn merge_one_level_test() {
        let p = BlockingParam {
            num_blocks: 16,
            block_len: 19,
            first_block_len: 17,
            last_block_len: 17,
        };
        let imit_len = 14;
        let buf_len = 19;
        let data_len = 599;
        let ary_len = imit_len + buf_len + data_len;
        let mut ctrl = MergeSortControl::new(0, data_len);
        ctrl.log2_num_seqs = 2;
        ctrl.seq_len = 150;
        let mut rng = test_rng();

        let less: fn(&i32, &i32) -> bool = |a, b| a < b;
        let less_div4: fn(&i32, &i32) -> bool = |a, b| a / 4 < b / 4;
        let sort_by = |v: &mut [i32], comp: fn(&i32, &i32) -> bool| {
            v.sort_by(|a, b| {
                if comp(a, b) {
                    std::cmp::Ordering::Less
                } else if comp(b, a) {
                    std::cmp::Ordering::Greater
                } else {
                    std::cmp::Ordering::Equal
                }
            })
        };

        for comp in [less, less_div4] {
            for forward in [true, false] {
                // forward: [ imit | buf | data ] -> [ imit | merged | buf ]
                // backward: [ imit | data | buf ] -> [ imit | buf | merged ]
                let (data, edata) = if forward {
                    (imit_len + buf_len, imit_len)
                } else {
                    (imit_len, imit_len + buf_len)
                };

                let mut ary = vec![42; ary_len];
                let mut expected = vec![42; ary_len];
                for i in 0..imit_len {
                    ary[i] = i as i32 * 4;
                    expected[i] = i as i32 * 4;
                }
                for (v, i) in ary[data..data + data_len].iter_mut().zip(100..) {
                    *v = i;
                }
                rng.shuffle(&mut ary[data..data + data_len]);
                for range in [0..149, 149..299, 299..449, 449..599] {
                    sort_by(&mut ary[data + range.start..data + range.end], comp);
                }
                expected[edata..edata + data_len].copy_from_slice(&ary[data..data + data_len]);
                for range in [0..299, 299..599] {
                    sort_by(&mut expected[edata + range.start..edata + range.end], comp);
                }

                let base = FwdPtr::new(ary.as_mut_ptr());
                let f = &mut |a: &i32, b: &i32| comp(a, b);
                unsafe {
                    if forward {
                        merge_one_level(
                            base,
                            base.add(imit_len),
                            base.add(data),
                            &ctrl,
                            BlockingParam { ..p },
                            f,
                            Flag::<true>,
                        );
                    } else {
                        merge_one_level(
                            base.add(p.num_blocks - 2).bwd(),
                            base.add(ary_len).bwd(),
                            base.add(ary_len - buf_len).bwd(),
                            &ctrl,
                            BlockingParam { ..p },
                            f,
                            Flag::<true>,
                        );
                    }
                }

                assert_eq!(ary, expected, "forward={forward}");
            }
        }
    }
}
