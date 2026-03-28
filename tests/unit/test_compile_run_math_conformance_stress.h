#pragma once

TEST_CASE("math conformance stress grid") {
  const std::string source = R"(
import /std/math/*

[int]
pass([bool] ok) {
  if(ok) {
    return(1i32)
  } else {
    return(0i32)
  }
}

[effects(io_out)]
emit([string] label, [bool] ok) {
  print(label)
  print(" "utf8)
  print_line(pass(ok))
}

[bool]
near([f32] a, [f32] b, [f32] eps) {
  return(abs(a - b) <= eps)
}

[return<int>]
main() {
  [array<f32>] grid{array<f32>(-3.0f32, -1.0f32, -0.5f32, 0.0f32, 0.5f32, 1.0f32, 3.0f32)}
  [int mut] count{0}
  [int mut] ok{1}
  while(count < grid.count()) {
    [f32] x{grid[count]}
    [f32] s{sin(x)}
    [f32] c{cos(x)}
    if(abs(s * s + c * c - 1.0f32) > 0.05f32) {
      ok = 0
    } else {
    }
    count = count + 1
  }
  emit("trig_identity_grid"utf8, ok == 1)

  [int mut] count2{0}
  [int mut] ok2{1}
  while(count2 < grid.count()) {
    [f32] x{grid[count2]}
    [f32] t{tanh(x)}
    if(abs(t) > 1.0f32 + 0.01f32) {
      ok2 = 0
    } else {
    }
    count2 = count2 + 1
  }
  emit("tanh_range_grid"utf8, ok2 == 1)

  return(0i32)
}
)";
  checkMathConformance(source, "math_conformance_grid");
}

TEST_CASE("math conformance float baseline trigonometric samples") {
  const std::string source = R"(
import /std/math/*

[effects(io_out)]
emit([string] label, [f32] value) {
  print(label)
  print(" "utf8)
  print_line(scaled(value))
}

[i64]
scaled([f32] value) {
  return(convert<i64>(round(value * 10000.0f32)))
}

[return<int>]
main() {
  emit("sin_0_5"utf8, sin(0.5f32))
  emit("cos_0_5"utf8, cos(0.5f32))
  emit("tan_0_5"utf8, tan(0.5f32))
  emit("asin_0_5"utf8, asin(0.5f32))
  emit("acos_0_5"utf8, acos(0.5f32))
  emit("atan_0_5"utf8, atan(0.5f32))
  return(0i32)
}
)";
  checkMathConformanceFloats(source, "math_conformance_float_samples_trig", 25.0, 5e-4);
}

TEST_CASE("math conformance float baseline transcendental samples") {
  const std::string source = R"(
import /std/math/*

[effects(io_out)]
emit([string] label, [f32] value) {
  print(label)
  print(" "utf8)
  print_line(scaled(value))
}

[i64]
scaled([f32] value) {
  return(convert<i64>(round(value * 10000.0f32)))
}

[return<int>]
main() {
  emit("sinh_1"utf8, sinh(1.0f32))
  emit("cosh_1"utf8, cosh(1.0f32))
  emit("tanh_1"utf8, tanh(1.0f32))
  emit("exp_1"utf8, exp(1.0f32))
  emit("log_2"utf8, log(2.0f32))
  emit("exp2_3"utf8, exp2(3.0f32))
  emit("log2_8"utf8, log2(8.0f32))
  return(0i32)
}
)";
  checkMathConformanceFloats(source, "math_conformance_float_samples_transcendental", 25.0, 5e-4);
}

TEST_CASE("math conformance float baseline composition samples") {
  const std::string source = R"(
import /std/math/*

[effects(io_out)]
emit([string] label, [f32] value) {
  print(label)
  print(" "utf8)
  print_line(scaled(value))
}

[i64]
scaled([f32] value) {
  return(convert<i64>(round(value * 10000.0f32)))
}

[return<int>]
main() {
  emit("sqrt_2"utf8, sqrt(2.0f32))
  emit("cbrt_27"utf8, cbrt(27.0f32))
  emit("hypot_3_4"utf8, hypot(3.0f32, 4.0f32))
  emit("pow_2_3"utf8, pow(2.0f32, 3.0f32))
  emit("fma_basic"utf8, fma(1.25f32, 2.5f32, -0.5f32))
  return(0i32)
}
)";
  checkMathConformanceFloats(source, "math_conformance_float_samples_composition", 25.0, 5e-4);
}

TEST_CASE("math conformance float grid sin") {
  const std::string source = R"(
import /std/math/*

[effects(io_out)]
emit([string] label, [f32] value) {
  print(label)
  print(" "utf8)
  print_line(scaled(value))
}

[i64]
scaled([f32] value) {
  return(convert<i64>(round(value * 10000.0f32)))
}

[return<int>]
main() {
  emit("sin_-3"utf8, sin(-3.0f32))
  emit("sin_-1"utf8, sin(-1.0f32))
  emit("sin_-0_5"utf8, sin(-0.5f32))
  emit("sin_0"utf8, sin(0.0f32))
  emit("sin_0_5"utf8, sin(0.5f32))
  emit("sin_1"utf8, sin(1.0f32))
  emit("sin_3"utf8, sin(3.0f32))
  return(0i32)
}
)";
  checkMathConformanceFloats(source, "math_conformance_float_grid_sin", 25.0, 5e-4);
}

TEST_CASE("math conformance float grid cos") {
  const std::string source = R"(
import /std/math/*

[effects(io_out)]
emit([string] label, [f32] value) {
  print(label)
  print(" "utf8)
  print_line(scaled(value))
}

[i64]
scaled([f32] value) {
  return(convert<i64>(round(value * 10000.0f32)))
}

[return<int>]
main() {
  emit("cos_-3"utf8, cos(-3.0f32))
  emit("cos_-1"utf8, cos(-1.0f32))
  emit("cos_-0_5"utf8, cos(-0.5f32))
  emit("cos_0"utf8, cos(0.0f32))
  emit("cos_0_5"utf8, cos(0.5f32))
  emit("cos_1"utf8, cos(1.0f32))
  emit("cos_3"utf8, cos(3.0f32))
  return(0i32)
}
)";
  checkMathConformanceFloats(source, "math_conformance_float_grid_cos", 25.0, 5e-4);
}

TEST_CASE("math conformance float grid exp_log") {
  const std::string source = R"(
import /std/math/*

[effects(io_out)]
emit([string] label, [f32] value) {
  print(label)
  print(" "utf8)
  print_line(scaled(value))
}

[i64]
scaled([f32] value) {
  return(convert<i64>(round(value * 10000.0f32)))
}

[return<int>]
main() {
  emit("exp_-2"utf8, exp(-2.0f32))
  emit("exp_-1"utf8, exp(-1.0f32))
  emit("exp_0"utf8, exp(0.0f32))
  emit("exp_1"utf8, exp(1.0f32))
  emit("exp_2"utf8, exp(2.0f32))

  emit("log_0_25"utf8, log(0.25f32))
  emit("log_0_5"utf8, log(0.5f32))
  emit("log_1"utf8, log(1.0f32))
  emit("log_2"utf8, log(2.0f32))
  emit("log_4"utf8, log(4.0f32))
  return(0i32)
}
)";
  checkMathConformanceFloats(source, "math_conformance_float_grid_exp_log", 25.0, 5e-4);
}

TEST_CASE("math conformance float grid hypot") {
  const std::string source = R"(
import /std/math/*

[effects(io_out)]
emit([string] label, [f32] value) {
  print(label)
  print(" "utf8)
  print_line(scaled(value))
}

[i64]
scaled([f32] value) {
  return(convert<i64>(round(value * 10000.0f32)))
}

[return<int>]
main() {
  emit("hypot_3_4"utf8, hypot(3.0f32, 4.0f32))
  emit("hypot_5_12"utf8, hypot(5.0f32, 12.0f32))
  emit("hypot_1_2"utf8, hypot(1.0f32, 2.0f32))
  return(0i32)
}
)";
  checkMathConformanceFloats(source, "math_conformance_float_grid_hypot", 25.0, 5e-4);
}

TEST_CASE("math conformance native approximation limits") {
  const std::string source = R"(
import /std/math/*

[int]
pass([bool] ok) {
  if(ok) {
    return(1i32)
  } else {
    return(0i32)
  }
}

[effects(io_out)]
emit([string] label, [bool] ok) {
  print(label)
  print(" "utf8)
  print_line(pass(ok))
}

[return<int>]
main() {
  [f32] sin_1e4{sin(10000.0f32)}
  [f32] cos_1e4{cos(10000.0f32)}
  [f32] sin_1e5{sin(100000.0f32)}
  [f32] cos_1e5{cos(100000.0f32)}
  emit("sin_large_1e4_range"utf8, abs(sin_1e4) <= 1.0f32)
  emit("cos_large_1e4_range"utf8, abs(cos_1e4) <= 1.0f32)
  emit("sin_large_1e5_range"utf8, abs(sin_1e5) <= 1.0f32)
  emit("cos_large_1e5_range"utf8, abs(cos_1e5) <= 1.0f32)
  emit("sin_large_1e4_finite"utf8, is_finite(sin_1e4))
  emit("cos_large_1e4_finite"utf8, is_finite(cos_1e4))
  emit("sin_large_1e5_finite"utf8, is_finite(sin_1e5))
  emit("cos_large_1e5_finite"utf8, is_finite(cos_1e5))
  [f32] norm_1e4{sin_1e4 * sin_1e4 + cos_1e4 * cos_1e4}
  emit("sin_cos_norm_1e4"utf8, abs(norm_1e4 - 1.0f32) < 0.25f32)

  [f32] exp_10{exp(10.0f32)}
  emit("exp_10_finite"utf8, is_finite(exp_10))
  emit("exp_10_positive"utf8, exp_10 > 0.0f32)

  [f32] log_small{log(0.000001f32)}
  emit("log_small_finite"utf8, is_finite(log_small))
  emit("log_small_negative"utf8, log_small < 0.0f32)

  [f32] atan_large{atan(100000.0f32)}
  emit("atan_large_range"utf8, atan_large > 1.5f32 && atan_large < 1.7f32)
  return(0i32)
}
)";
  checkMathConformance(source, "math_conformance_native_limits");
}

TEST_CASE("math conformance heavy trig workload") {
  const std::string source = R"(
import /std/math/*

[int]
pass([bool] ok) {
  if(ok) {
    return(1i32)
  } else {
    return(0i32)
  }
}

[effects(io_out)]
emit([string] label, [bool] ok) {
  print(label)
  print(" "utf8)
  print_line(pass(ok))
}

[return<int>]
main() {
  [int mut] i{0}
  [f32 mut] acc{0.0f32}
  while(i < 20000) {
    [f32] x{convert<f32>(i) * 0.001f32}
    acc = acc + sin(x) * cos(x) + tanh(x)
    i = i + 1
  }
  emit("heavy_trig_finite"utf8, is_finite(acc))
  emit("heavy_trig_range"utf8, abs(acc) < 50000.0f32)
  return(0i32)
}
)";
  checkMathConformance(source, "math_conformance_heavy_trig");
}

TEST_CASE("math conformance heavy exp log workload") {
  const std::string source = R"(
import /std/math/*

[int]
pass([bool] ok) {
  if(ok) {
    return(1i32)
  } else {
    return(0i32)
  }
}

[effects(io_out)]
emit([string] label, [bool] ok) {
  print(label)
  print(" "utf8)
  print_line(pass(ok))
}

[return<int>]
main() {
  [int mut] i{0}
  [f32 mut] acc{0.0f32}
  while(i < 15000) {
    [f32] x{1.0001f32 + convert<f32>(i) * 0.0001f32}
    acc = acc + exp(log(x))
    i = i + 1
  }
  emit("heavy_exp_log_finite"utf8, is_finite(acc))
  emit("heavy_exp_log_range"utf8, acc > 10000.0f32 && acc < 60000.0f32)
  return(0i32)
}
)";
  checkMathConformance(source, "math_conformance_heavy_exp_log");
}

TEST_CASE("math conformance array math usage") {
  const std::string source = R"(
import /std/math/*

[int]
pass([bool] ok) {
  if(ok) {
    return(1i32)
  } else {
    return(0i32)
  }
}

[effects(io_out)]
emit([string] label, [bool] ok) {
  print(label)
  print(" "utf8)
  print_line(pass(ok))
}

[return<int>]
main() {
  [array<f32>] values{array<f32>(-1.5f32, -0.5f32, 0.0f32, 0.5f32, 1.5f32)}
  [int mut] idx{0}
  [int mut] ok{1}
  while(idx < values.count()) {
    [f32] x{values[idx]}
    if(abs(sin(x) * sin(x) + cos(x) * cos(x) - 1.0f32) > 0.05f32) {
      ok = 0
    } else {
    }
    idx = idx + 1
  }
  emit("array_trig_identity"utf8, ok == 1)

  return(0i32)
}
)";
  checkMathConformance(source, "math_conformance_array_math");
}

TEST_CASE("math conformance dense grids") {
  const std::string source = R"(
import /std/math/*

[int]
pass([bool] ok) {
  if(ok) {
    return(1i32)
  } else {
    return(0i32)
  }
}

[effects(io_out)]
emit([string] label, [bool] ok) {
  print(label)
  print(" "utf8)
  print_line(pass(ok))
}

[bool]
near([f32] a, [f32] b, [f32] eps) {
  return(abs(a - b) <= eps)
}

[return<int>]
main() {
  [array<f32>] grid{array<f32>(
    -3.0f32, -2.5f32, -2.0f32, -1.5f32, -1.0f32, -0.5f32,
    0.0f32, 0.5f32, 1.0f32, 1.5f32, 2.0f32, 2.5f32, 3.0f32
  )}
  [int mut] count{0}
  [int mut] ok{1}
  [f32 mut] prev{-999.0f32}
  while(count < grid.count()) {
    [f32] x{grid[count]}
    [f32] t{tanh(x)}
    if(count > 0) {
      if(t < prev) {
        ok = 0
      } else {
      }
    } else {
    }
    prev = t
    count = count + 1
  }
  emit("tanh_monotonic"utf8, ok == 1)

  [array<f32>] pos{array<f32>(0.5f32, 1.0f32, 2.0f32, 3.0f32, 4.0f32)}
  [int mut] count2{0}
  [int mut] ok2{1}
  [f32 mut] prev2{-999.0f32}
  while(count2 < pos.count()) {
    [f32] x{pos[count2]}
    [f32] l{log(x)}
    if(count2 > 0) {
      if(l < prev2) {
        ok2 = 0
      } else {
      }
    } else {
    }
    prev2 = l
    count2 = count2 + 1
  }
  emit("log_monotonic"utf8, ok2 == 1)

  [int mut] count3{0}
  [int mut] ok3{1}
  [f32 mut] prev3{-999.0f32}
  while(count3 < pos.count()) {
    [f32] x{pos[count3]}
    [f32] e{exp(x)}
    if(count3 > 0) {
      if(e < prev3) {
        ok3 = 0
      } else {
      }
    } else {
    }
    prev3 = e
    count3 = count3 + 1
  }
  emit("exp_monotonic"utf8, ok3 == 1)

  [array<f32>] small{array<f32>(-0.1f32, -0.05f32, 0.0f32, 0.05f32, 0.1f32)}
  [int mut] count4{0}
  [int mut] ok4{1}
  while(count4 < small.count()) {
    [f32] x{small[count4]}
    [f32] s{sin(x)}
    if(abs(s - x) > 0.01f32) {
      ok4 = 0
    } else {
    }
    count4 = count4 + 1
  }
  emit("sin_small_angle"utf8, ok4 == 1)

  return(0i32)
}
)";
  checkMathConformance(source, "math_conformance_dense_grid");
}

