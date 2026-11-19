use crate::blockmerge::BlockingParam;

pub fn over_approx_sqrt(x: usize) -> usize {
    let mut n = 1u32;
    let mut p = x;
    loop {
        n += 1;
        p /= 4;
        if p < 8 {
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
    let num_blocks = if ctrl.buf_len != 0 {
        ((seq_len - 1) / ctrl.buf_len + 1) * 2
    } else if ctrl.imit_len == 0 {
        // No keys are available, so `num_blocks = max_num_blocks = 2`. (`limit_num_blocks >= 2` always holds.)
        // Skipping `limit_num_blocks` also avoids computing `seq_len * 2`, which may exceed `len` when `len <= 16`.
        2
    } else {
        let limit_num_blocks = seq_len / over_approx_sqrt(seq_len * 2) * 2;
        core::cmp::min(max_num_blocks, limit_num_blocks)
    };

    let block_len = (seq_len - 1) / (num_blocks / 2) + 1;

    let residual_len = seq_len - block_len * (num_blocks / 2 - 1);

    BlockingParam {
        num_blocks: num_blocks,
        block_len: block_len,
        first_block_len: residual_len,
        last_block_len: residual_len,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn over_approx_sqrt_test() {
        assert_eq!(over_approx_sqrt(8), 3);
        for x in 9..17 {
            assert_eq!(over_approx_sqrt(x), 4, "x={x}");
        }
        for (range, ratio) in [
            (17..1000, 1.25),
            (1000..200000, 1.0 + 1.0 / 32.0),
            (200000..1500000, 1.0 + 1.0 / 256.0),
        ] {
            for x in range {
                let ar = over_approx_sqrt(x) as f64;
                let r = (x as f64).sqrt();
                assert!(r <= ar && ar < r * ratio, "x={x} ar={ar} r={r}");
            }
        }
    }

    #[test]
    fn merge_sort_control_test() {
        let mut ctrl = MergeSortControl::new(8, 16);
        assert_eq!(ctrl.log2_num_seqs, 1);
        assert_eq!(ctrl.imit_len, 2);
        assert_eq!(ctrl.buf_len, 6);
        assert_eq!(ctrl.bufferable_len, 12);
        assert_eq!(ctrl.next(), 6);
        assert_eq!(ctrl.imit_len, 8);
        assert_eq!(ctrl.buf_len, 0);
        assert_eq!(ctrl.log2_num_seqs, 0);

        let mut ctrl = MergeSortControl::new(21, 123);
        assert_eq!(ctrl.log2_num_seqs, 4);
        assert_eq!(ctrl.imit_len, 8);
        assert_eq!(ctrl.buf_len, 13);
        assert_eq!(ctrl.seq_len, 8);
        assert!(ctrl.forward);
        assert_eq!(ctrl.next(), 0);
        assert_eq!(ctrl.log2_num_seqs, 3);
        assert_eq!(ctrl.imit_len, 8);
        assert_eq!(ctrl.buf_len, 13);
        assert_eq!(ctrl.seq_len, 16);
        assert!(!ctrl.forward);

        let ctrl = MergeSortControl::new(22, 123);
        assert_eq!(ctrl.imit_len, 10);
        assert_eq!(ctrl.buf_len, 12);

        let mut ctrl = MergeSortControl::new(47, 953);
        assert_eq!(ctrl.log2_num_seqs, 7);
        assert_eq!(ctrl.imit_len, 22);
        assert_eq!(ctrl.buf_len, 25);
        assert_eq!(ctrl.seq_len, 8);
        for (old_buf_len, seq_len) in [(0, 15), (0, 30), (0, 60), (0, 120), (0, 239), (25, 477), (0, 953)] {
            assert_eq!(ctrl.next(), old_buf_len);
            assert_eq!(ctrl.seq_len, seq_len);
        }
    }

    #[test]
    fn determine_blocking_test() {
        let mut ctrl = MergeSortControl::new(47, 953);
        for (i, num_blocks) in [2, 2, 4, 6, 10, 20, 30].into_iter().enumerate() {
            if i != 0 {
                ctrl.next();
            }
            let p = determine_blocking(&ctrl);
            assert_eq!(p.num_blocks, num_blocks, "level={i}");
            assert!(2 <= p.first_block_len && p.first_block_len <= p.block_len, "level={i}");
        }
    }
}
