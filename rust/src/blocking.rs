use crate::blockmerge::BlockingParam;

pub fn over_approx_sqrt(x: usize) -> usize {
    let mut n = 1u32;
    let mut p = x;
    loop {
        n += 1;
        p /= 4;
        if p >= 8 {
            break;
        }
    }
    let r0 = (1usize << (n - 1)) + ((x - 1) >> (n + 1)) + 1;
    return (r0 + (x - 1) / r0) / 2 + 1;
}

pub struct MergeSortControl {
    pub imit_len: usize,
    pub buf_len: usize,
    bufferable_len: usize,
    pub data_len: usize,
    pub log2_num_seqs: usize,
    pub seq_len: usize,
    pub forward: bool,
}

impl MergeSortControl {
    pub fn new(num_keys: usize, data_len: usize) -> Self {
        let mut ctrl = MergeSortControl {
            imit_len: 0,
            buf_len: 0,
            bufferable_len: 0,
            data_len: data_len,
            log2_num_seqs: 1,
            seq_len: 0,
            forward: true,
        };
        if num_keys != 0 {
            ctrl.imit_len = (num_keys + 2) / 4 * 2 - 2;
            ctrl.buf_len = num_keys - ctrl.imit_len;
            ctrl.bufferable_len = (ctrl.imit_len + 2) / 2 * ctrl.buf_len;
        }

        while (data_len - 1) >> (ctrl.log2_num_seqs + 3) != 0 {
            ctrl.log2_num_seqs += 1;
        }
        ctrl.seq_len = ((data_len - 1) >> ctrl.log2_num_seqs) + 1;
        ctrl
    }

    pub fn next(&mut self) -> usize {
        self.log2_num_seqs -= 1;
        self.seq_len = ((self.data_len - 1) >> self.log2_num_seqs) + 1;

        if self.buf_len == 0 {
            return 0;
        }
        self.forward = !self.forward;

        if self.log2_num_seqs == 0 || self.seq_len > self.bufferable_len {
            let old_buf_len = self.buf_len;
            self.imit_len += self.buf_len / 2 * 2;
            self.buf_len = 0;
            self.bufferable_len = 0;
            return old_buf_len;
        }
        return 0;
    }
}

pub fn determine_blocking(ctrl: &MergeSortControl) -> BlockingParam {
    let seq_len = ctrl.seq_len;

    let max_num_blocks = ctrl.imit_len + 2;
    let num_blocks =
        if ctrl.buf_len != 0 {
            ((seq_len - 1) / ctrl.buf_len + 1) * 2
        } else {
            let limit_num_blocks = seq_len / over_approx_sqrt(seq_len * 2) * 2;
            std::cmp::min(max_num_blocks, limit_num_blocks)
        };

    let block_len = (seq_len - 1) / (num_blocks / 2) + 1;

    let residual_len = seq_len - block_len * (num_blocks / 2 - 1);

    BlockingParam{
        num_blocks: num_blocks,
        block_len: block_len,
        first_block_len: residual_len,
        last_block_len: residual_len,
    }
}
