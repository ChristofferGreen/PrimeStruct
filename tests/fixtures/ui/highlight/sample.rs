// a Rust sample
#[derive(Debug)]
struct Point<'a> {
    name: &'a str,
}

fn main() {
    let x: u32 = 5;
    println!("x = {}", x);
    let c = 'z';
}
