use crate::blockmerge::{BlockingParam, merge_blocking};
use crate::utils::{FwdPtr, Pos};

pub struct SequenceDivider<const FORWARD: bool = true> {
    log2_num_seqs: usize,
    num_seqs: usize,
    remainder: usize,
    frac_counter: usize,
}

impl<const FORWARD: bool> SequenceDivider<FORWARD> {
    pub fn new(data_len: usize, log2_num_seqs: usize) -> Self {
        let mut obj = SequenceDivider {
            log2_num_seqs,
            num_seqs: 1usize << log2_num_seqs,
            remainder: (data_len - 1) % (1usize << log2_num_seqs) + 1,
            frac_counter: 0,
        };
        if !FORWARD {
            obj.remainder = obj.num_seqs - obj.remainder;
        }
        obj
    }

    pub fn next(&mut self) -> bool {
        self.frac_counter += self.remainder;
        let mut no_carry = (self.frac_counter & (1usize << self.log2_num_seqs)) == 0;
        if !FORWARD {
            no_carry = !no_carry;
        }
        self.frac_counter &= (1usize << self.log2_num_seqs) - 1;
        self.num_seqs -= 1;
        no_carry
    }

    pub fn is_end(&self) -> bool {
        self.num_seqs == 0
    }
}

pub unsafe fn merge_one_level<T, F: FnMut(&T, &T) -> bool, const HAS_BUF: bool, const FORWARD: bool>(
    imit: FwdPtr<T>,
    mut buf: FwdPtr<T>,
    mut data: FwdPtr<T>,
    seq_len: usize,
    seq_div: &mut SequenceDivider<FORWARD>,
    mut p: BlockingParam,
    f: &mut F,
) {
    unsafe {
        let residual_len = p.first_block_len;
        loop {
            let lseq_decr = seq_div.next();
            let rseq_decr = seq_div.next();
            let merging_len = (seq_len - lseq_decr as usize) + (seq_len - rseq_decr as usize);
            p.first_block_len = residual_len - lseq_decr as usize;
            p.last_block_len = residual_len - rseq_decr as usize;

            if FORWARD {
                merge_blocking::<_, _, _, HAS_BUF>(imit, &mut buf, data, &p, f);
                data = data.add(merging_len);
            } else {
                let mut bwd_buf = buf.bwd();
                merge_blocking::<_, _, _, HAS_BUF>(imit.add(p.num_blocks - 2).bwd(), &mut bwd_buf, data.bwd(), &p, f);
                buf = bwd_buf.fwd();
                data = data.sub(merging_len);
            }
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
                        merge_one_level::<_, _, true, true>(
                            base,
                            base.add(imit_len),
                            base.add(data),
                            150,
                            &mut SequenceDivider::new(data_len, 2),
                            BlockingParam { ..p },
                            f,
                        );
                    } else {
                        merge_one_level::<_, _, true, false>(
                            base,
                            base.add(ary_len),
                            base.add(ary_len - buf_len),
                            150,
                            &mut SequenceDivider::new(data_len, 2),
                            BlockingParam { ..p },
                            f,
                        );
                    }
                }

                assert_eq!(ary, expected, "forward={forward}");
            }
        }
    }
}
