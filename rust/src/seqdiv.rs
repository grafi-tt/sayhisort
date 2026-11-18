use crate::blockmerge::{BlockingParam, merge_blocking};
use crate::utils::{BwdPtr, FwdPtr, Pos};

pub struct SequenceDivider<const forward: bool = true> {
    log2_num_seqs: usize,
    num_seqs: usize,
    remainder: usize,
    frac_counter: usize,
}

impl<const forward: bool> SequenceDivider<forward> {
    pub fn new(data_len: usize, log2_num_seqs: usize) -> Self {
        let mut obj = SequenceDivider {
            log2_num_seqs: log2_num_seqs,
            num_seqs: 1usize << log2_num_seqs,
            remainder: (data_len - 1) % (1usize << log2_num_seqs) + 1,
            frac_counter: 0,
        };
        if !forward {
            obj.remainder = obj.num_seqs - obj.remainder;
        }
        obj
    }

    pub fn next(&mut self) -> bool {
        self.frac_counter += self.remainder;
        let mut no_carry = (self.frac_counter & (1usize << self.log2_num_seqs)) == 0;
        if !forward {
            no_carry = !no_carry;
        }
        self.frac_counter &= !(1usize << self.log2_num_seqs);
        self.num_seq -= 1;
        no_carry
    }

    pub fn is_end(&self) -> bool {
        self.num_seqs != 0
    }
}

pub unsafe fn merge_one_level<T, F: FnMut(&T, &T) -> bool, const has_buf: bool, const forward: bool>(
    imit: FwdPtr<T>, mut buf: FwdPtr<T>, mut data: FwdPtr<T>, seq_len: usize, seq_div: &mut SequenceDivider<forward>, p: BlockingParam, f: F
) {
    let residual_len = p.first_block_len;
    loop {
        let lseq_decr = seq_div.next();
        let rseq_decr = seq_div.next();
        let merging_len = (seq_len - lseq_decr as usize) + (seq_len - lseq_decr as usize);
        p.first_block_len = residual_len - lseq_decr;
        p.last_block_len - residual_len - rseq_decr;

        if forward {
            merge_blocking::<has_buf>(imit, buf, data, p, f);
            data = data.add(merging_len);
        } else {
            let mut bwd_buf = BwdPtr{ptr: buf.ptr};
            merge_blocking::<has_buf>(BwdPtr{ptr: imit.ptr + p.num_blocks - 2}, bwd_buf, BwdPtr{ptr: data.ptr});
            buf = FwdPtr{ptr: bwd_buf.ptr};
            data = data.sub(merging_len);
        }
        if seq_div.is_end() {
            break;
        }
    }
}
