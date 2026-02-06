const DATA: &str = r#"{"users":[{"id":1,"name":"alice","score":12},{"id":2,"name":"bob","score":7},{"id":3,"name":"carol","score":42}],"active":true,"count":3,"misc":{"neg":-7,"escape":"a\n\"b\"","unicode":"\u0041","zip":90210},"tags":["a","b","c"]}"#;

#[inline]
fn is_digit(ch: u8) -> bool {
    ch >= b'0' && ch <= b'9'
}

fn main() {
    let outer: i64 = 20_000;
    let bytes = DATA.as_bytes();

    let mut token_total: i64 = 0;
    let mut number_count: i64 = 0;
    let mut number_sum: i64 = 0;
    let mut string_bytes: i64 = 0;

    for _ in 0..outer {
        let mut local_tokens: i64 = 0;
        let mut local_number_count: i64 = 0;
        let mut local_number_sum: i64 = 0;
        let mut local_string_bytes: i64 = 0;

        let mut in_string = false;
        let mut escape = false;
        let mut unicode_digits: i32 = 0;
        let mut in_number = false;
        let mut sign: i64 = 1;
        let mut value: i64 = 0;

        for &ch in bytes {
            if in_string {
                if unicode_digits > 0 {
                    unicode_digits -= 1;
                    continue;
                }
                if escape {
                    escape = false;
                    if ch == b'u' {
                        unicode_digits = 4;
                    }
                    local_string_bytes += 1;
                    continue;
                }
                if ch == b'\\' {
                    escape = true;
                    continue;
                }
                if ch == b'"' {
                    in_string = false;
                    continue;
                }
                local_string_bytes += 1;
                continue;
            }

            if in_number {
                if is_digit(ch) {
                    value = value * 10 + (ch - b'0') as i64;
                    continue;
                }
                local_number_sum += sign * value;
                local_number_count += 1;
                in_number = false;
                sign = 1;
                value = 0;
            }

            if ch == b'"' {
                in_string = true;
            } else if ch == b'-' {
                in_number = true;
                sign = -1;
                value = 0;
            } else if is_digit(ch) {
                in_number = true;
                sign = 1;
                value = (ch - b'0') as i64;
            } else if ch == b'{' || ch == b'}' || ch == b'[' || ch == b']' || ch == b':' || ch == b',' {
                local_tokens += 1;
            }
        }

        if in_number {
            local_number_sum += sign * value;
            local_number_count += 1;
        }

        token_total += local_tokens;
        number_count += local_number_count;
        number_sum += local_number_sum;
        string_bytes += local_string_bytes;
    }

    let output: i64 = number_sum + (token_total * 1_000_000_000) + (number_count * 1_000_000) + string_bytes;
    println!("{}", output);
}

