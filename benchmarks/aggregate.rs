fn main() {
    let n: i64 = 5_000_000;
    let mut sum: i64 = 0;
    let mut sumsq: i64 = 0;
    for i in 0..n {
        sum += i;
        sumsq += i * i;
    }
    println!("{}", sum + sumsq);
}
