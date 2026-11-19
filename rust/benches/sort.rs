//! Benchmark ported from `cpp/tests/sayhisort_bench.cc`. Run by `cargo bench`.
//!
//! The result is written to stdout in the same YAML format as the C++ version, so that `cpp/bench_result/plot.py` can
//! read it.

use std::hint::black_box;
use std::time::Instant;

#[path = "../src/test_util.rs"]
#[allow(dead_code)]
mod test_util;

use test_util::Xoshiro256PlusPlus;

const SEED: u64 = 42;
const SIZE: usize = 1_500_000;

/// Uniform value in `[-2.5, 2.5)`.
fn noise(rng: &mut Xoshiro256PlusPlus) -> f64 {
    (rng.next_u64() >> 11) as f64 * (1.0 / (1u64 << 53) as f64) * 5.0 - 2.5
}

fn random(p: &mut [u64], rng: &mut Xoshiro256PlusPlus) {
    p.iter_mut().for_each(|v| *v = rng.next_u64());
}

fn random_sqrt_keys(p: &mut [u64], rng: &mut Xoshiro256PlusPlus) {
    let n = (p.len() as f64).sqrt() as usize;
    p.iter_mut().for_each(|v| *v = rng.below(n + 1) as u64);
}

fn random_few_keys(p: &mut [u64], rng: &mut Xoshiro256PlusPlus) {
    p.iter_mut().for_each(|v| *v = rng.below(99) as u64);
}

fn mostly_ascending(p: &mut [u64], rng: &mut Xoshiro256PlusPlus) {
    for (i, v) in p.iter_mut().enumerate() {
        *v = (i as f64 + noise(rng)).max(0.0) as u64;
    }
}

fn mostly_descending(p: &mut [u64], rng: &mut Xoshiro256PlusPlus) {
    let n = p.len();
    for (i, v) in p.iter_mut().enumerate() {
        *v = ((n - i) as f64 + noise(rng)).max(0.0) as u64;
    }
}

fn mostly_equal(p: &mut [u64], rng: &mut Xoshiro256PlusPlus) {
    p.iter_mut().for_each(|v| *v = 1000 + rng.below(4) as u64);
}

fn ascending(p: &mut [u64], _: &mut Xoshiro256PlusPlus) {
    p.iter_mut().enumerate().for_each(|(i, v)| *v = i as u64);
}

fn descending(p: &mut [u64], _: &mut Xoshiro256PlusPlus) {
    let n = p.len();
    p.iter_mut().enumerate().for_each(|(i, v)| *v = (n - i) as u64);
}

fn equal(p: &mut [u64], _: &mut Xoshiro256PlusPlus) {
    p.fill(1000);
}

type Generator = fn(&mut [u64], &mut Xoshiro256PlusPlus);
type Sorter = fn(&mut [u64]);

fn main() {
    let bench_data: [(&str, Generator); 9] = [
        ("Random", random),
        ("RandomSqrtKeys", random_sqrt_keys),
        ("RandomFewKeys", random_few_keys),
        ("MostlyAscending", mostly_ascending),
        ("MostlyDescending", mostly_descending),
        ("MostlyEqual", mostly_equal),
        ("Ascending", ascending),
        ("Descending", descending),
        ("Equal", equal),
    ];
    let sorters: [(&str, Sorter); 2] = [
        ("sayhisort", |v| sayhisort::sort(v)),
        ("std::slice::sort", |v| v.sort()),
    ];

    let mut data = vec![0u64; SIZE];
    for (name, generate) in bench_data {
        println!("{name}:");
        generate(&mut data, &mut Xoshiro256PlusPlus::from_seed(SEED));
        let mut expected = data.clone();
        expected.sort();

        for (sorter_name, sort) in sorters {
            generate(&mut data, &mut Xoshiro256PlusPlus::from_seed(SEED));
            let start = Instant::now();
            sort(black_box(&mut data));
            let elapsed = start.elapsed();
            println!("  {sorter_name}:");
            println!("    elapsed_time_ms: {:.4}", elapsed.as_secs_f64() * 1000.0);
            if data != expected {
                eprintln!("Result check failed! ({name}, {sorter_name})");
                std::process::exit(1);
            }
        }
    }
}
