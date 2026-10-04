fn main() {
    let n: i32 = 20_000_000;
    let mut sum: f64 = 0.0;
    let mut sign: f64 = 1.0;
    let mut denominator: f64 = 1.0;
    for _ in 0..n {
        sum += sign / denominator;
        sign = -sign;
        denominator += 2.0;
    }
    println!("{}", (sum * 4_000_000_000.0) as i64);
}
