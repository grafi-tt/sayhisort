//! Deterministic PRNG for unit tests.
//!
//! The seed is read from the `SAYHISORT_TEST_SEED` environment variable (decimal, or hex with `0x` prefix;
//! defaults to 0). Every test gets the same seed.

/// SplitMix64. Used to expand a 64-bit seed into the Xoshiro256++ state.
/// <https://prng.di.unimi.it/splitmix64.c>
struct SplitMix64(u64);

impl SplitMix64 {
    fn next_u64(&mut self) -> u64 {
        self.0 = self.0.wrapping_add(0x9e3779b97f4a7c15);
        let mut z = self.0;
        z = (z ^ (z >> 30)).wrapping_mul(0xbf58476d1ce4e5b9);
        z = (z ^ (z >> 27)).wrapping_mul(0x94d049bb133111eb);
        z ^ (z >> 31)
    }
}

/// Xoshiro256++. <https://prng.di.unimi.it/xoshiro256plusplus.c>
pub struct Xoshiro256PlusPlus {
    s: [u64; 4],
}

impl Xoshiro256PlusPlus {
    pub fn from_seed(seed: u64) -> Self {
        let mut sm = SplitMix64(seed);
        Self {
            s: std::array::from_fn(|_| sm.next_u64()),
        }
    }

    pub fn next_u64(&mut self) -> u64 {
        let s = &mut self.s;
        let result = s[0].wrapping_add(s[3]).rotate_left(23).wrapping_add(s[0]);
        let t = s[1] << 17;
        s[2] ^= s[0];
        s[3] ^= s[1];
        s[1] ^= s[2];
        s[0] ^= s[3];
        s[2] ^= t;
        s[3] = s[3].rotate_left(45);
        result
    }

    /// Value in `0..n`, by multiply-shift (bias is negligible for test use).
    pub fn below(&mut self, n: usize) -> usize {
        ((self.next_u64() as u128 * n as u128) >> 64) as usize
    }

    /// Fisher–Yates shuffle.
    pub fn shuffle<T>(&mut self, v: &mut [T]) {
        for i in (1..v.len()).rev() {
            v.swap(i, self.below(i + 1));
        }
    }
}

/// Creates the PRNG seeded by `SAYHISORT_TEST_SEED`.
pub fn test_rng() -> Xoshiro256PlusPlus {
    let seed = match std::env::var("SAYHISORT_TEST_SEED") {
        Err(std::env::VarError::NotPresent) => 0,
        Err(e) => panic!("invalid SAYHISORT_TEST_SEED: {e}"),
        Ok(s) => {
            let s = s.trim();
            let parsed = match s.strip_prefix("0x").or_else(|| s.strip_prefix("0X")) {
                Some(hex) => u64::from_str_radix(hex, 16),
                None => s.parse(),
            };
            parsed.unwrap_or_else(|e| panic!("invalid SAYHISORT_TEST_SEED {s:?}: {e}"))
        }
    };
    // Shown only when the test fails, since libtest captures stdout.
    println!("SAYHISORT_TEST_SEED={seed}");
    Xoshiro256PlusPlus::from_seed(seed)
}
