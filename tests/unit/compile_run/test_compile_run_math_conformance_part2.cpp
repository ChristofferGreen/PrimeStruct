#include "test_compile_run_helpers.h"
#include "test_compile_run_math_conformance_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.math_conformance");

TEST_CASE("math conformance misc ops") {
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
  emit("min_i"utf8, min(7i32, 2i32) == 2i32)
  emit("max_i"utf8, max(7i32, 2i32) == 7i32)
  emit("clamp_i"utf8, clamp(9i32, 2i32, 6i32) == 6i32)
  emit("saturate_i"utf8, saturate(-2i32) == 0i32)
  emit("saturate_i_hi"utf8, saturate(2i32) == 1i32)

  emit("lerp_f"utf8, near(lerp(0.0f32, 10.0f32, 0.5f32), 5.0f32, 0.01f32))
  emit("lerp_f_start"utf8, near(lerp(-2.0f32, 6.0f32, 0.0f32), -2.0f32, 0.001f32))
  emit("lerp_f_end"utf8, near(lerp(-2.0f32, 6.0f32, 1.0f32), 6.0f32, 0.001f32))
  emit("lerp_f_neg"utf8, near(lerp(-4.0f32, 6.0f32, 0.5f32), 1.0f32, 0.01f32))
  emit("lerp_oob_low"utf8, near(lerp(0.0f32, 10.0f32, -0.5f32), -5.0f32, 0.02f32))
  emit("lerp_oob_high"utf8, near(lerp(0.0f32, 10.0f32, 1.5f32), 15.0f32, 0.02f32))
  emit("lerp_i_end"utf8, lerp(0i32, 10i32, 1i32) == 10i32)
  emit("lerp_i_extrap"utf8, lerp(0i32, 10i32, 2i32) == 20i32)
  emit("fma_f"utf8, near(fma(2.0f32, 3.0f32, 4.0f32), 10.0f32, 0.01f32))
  [f32] fma_small{fma(0.0001f32, 0.0001f32, 0.0001f32)}
  emit("fma_small"utf8, abs(fma_small - (0.0001f32 * 0.0001f32 + 0.0001f32)) < 0.000001f32)
  [f32] fma_large{fma(10000000000.0f32, 10000000000.0f32, 100000.0f32)}
  emit("fma_large"utf8, is_finite(fma_large) && fma_large > 1.0e19f32)
  emit("hypot_f"utf8, near(hypot(3.0f32, 4.0f32), 5.0f32, 0.05f32))
  emit("hypot_5_12"utf8, near(hypot(5.0f32, 12.0f32), 13.0f32, 0.1f32))
  emit("hypot_swap"utf8, abs(hypot(3.0f32, 4.0f32) - hypot(4.0f32, 3.0f32)) < 0.01f32)
  emit("hypot_scale"utf8, abs(hypot(6.0f32, 8.0f32) - 10.0f32) < 0.1f32)
  emit("copysign_f"utf8, near(copysign(2.5f32, -1.0f32), -2.5f32, 0.001f32))
  emit("copysign_pos"utf8, near(copysign(-2.5f32, 1.0f32), 2.5f32, 0.001f32))
  [f32] neg_zero{copysign(0.0f32, -1.0f32)}
  [f32] neg_zero_inv{1.0f32 / neg_zero}
  emit("copysign_neg_zero"utf8, is_inf(neg_zero_inv) && neg_zero_inv < 0.0f32)

  emit("abs_f"utf8, near(abs(-2.5f32), 2.5f32, 0.001f32))
  emit("abs_i"utf8, abs(-12345i32) == 12345i32)
  emit("sign_i"utf8, sign(-7i32) == -1i32)
  emit("sign_i_zero"utf8, sign(0i32) == 0i32)
  emit("sign_f"utf8, near(sign(-0.5f32), -1.0f32, 0.001f32))
  emit("sign_f_zero"utf8, near(sign(0.0f32), 0.0f32, 0.001f32))
  emit("min_f"utf8, near(min(1.0f32, -2.0f32), -2.0f32, 0.001f32))
  emit("max_f"utf8, near(max(1.0f32, -2.0f32), 1.0f32, 0.001f32))
  emit("clamp_f"utf8, near(clamp(2.5f32, 0.0f32, 1.0f32), 1.0f32, 0.001f32))
  emit("clamp_mid_f"utf8, near(clamp(0.5f32, 0.0f32, 1.0f32), 0.5f32, 0.001f32))
  emit("saturate_f"utf8, near(saturate(1.5f32), 1.0f32, 0.001f32))
  emit("saturate_f_low"utf8, near(saturate(-0.25f32), 0.0f32, 0.001f32))

  [f32] nan{0.0f32 / 0.0f32}
  [f32] inf{1.0f32 / 0.0f32}
  emit("copysign_nan"utf8, is_nan(copysign(nan, -1.0f32)))
  emit("is_nan"utf8, is_nan(nan))
  emit("is_inf"utf8, is_inf(inf))
  emit("is_finite"utf8, is_finite(1.0f32))
  emit("is_finite_inf"utf8, is_finite(inf) == false)
  emit("nan_add"utf8, is_nan(nan + 1.0f32))
  emit("nan_sub"utf8, is_nan(1.0f32 - nan))
  emit("nan_mul"utf8, is_nan(nan * 0.0f32))
  emit("nan_div"utf8, is_nan(nan / 2.0f32))

  emit("radians_degrees"utf8, abs(radians(degrees(1.25f32)) - 1.25f32) < 0.0005f32)
  emit("degrees_radians"utf8, abs(degrees(radians(45.0f32)) - 45.0f32) < 0.01f32)
  emit("degrees_pi"utf8, abs(degrees(convert<f32>(pi)) - 180.0f32) < 1.0f32)
  emit("degrees_large"utf8, abs(degrees(radians(720.0f32)) - 720.0f32) < 0.5f32)

  return(0i32)
}
)";
  checkMathConformance(source, "math_conformance_misc");
}

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

TEST_CASE("math conformance deterministic samples") {
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
  [array<f32>] samples{array<f32>(
    0.123f32, 0.456f32, 0.789f32, 1.234f32, 1.618f32,
    -0.321f32, -0.654f32, -0.987f32
  )}
  [int mut] idx{0}
  [int mut] ok{1}
  while(idx < samples.count()) {
    [f32] x{samples[idx]}
    [int mut] j{idx + 3}
    if(j >= samples.count()) {
      j = j - samples.count()
    } else {
    }
    [f32] y{samples[j]}
    [f32] s{sin(x)}
    [f32] c{cos(x)}
    if(abs(s * s + c * c - 1.0f32) > 0.05f32) {
      ok = 0
    } else {
    }
    if(abs(hypot(x, y) - hypot(y, x)) > 0.02f32) {
      ok = 0
    } else {
    }
    if(abs(tan(x) - (sin(x) / cos(x))) > 0.2f32) {
      ok = 0
    } else {
    }
    idx = idx + 1
  }
  emit("deterministic_samples"utf8, ok == 1)

  return(0i32)
}
)";
  checkMathConformance(source, "math_conformance_samples");
}

TEST_CASE("math conformance deterministic exp/log samples") {
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
  [array<f32>] values{array<f32>(
    0.1f32, 0.2f32, 0.5f32, 1.0f32, 1.5f32, 2.0f32
  )}
  [int mut] i{0}
  [int mut] ok{1}
  while(i < values.count()) {
    [f32] x{values[i]}
    [f32] l{log(x)}
    [f32] e{exp(l)}
    if(abs(e - x) > 0.05f32) {
      ok = 0
    } else {
    }
    [f32] e2{exp(x)}
    [f32] l2{log(e2)}
    if(abs(l2 - x) > 0.05f32) {
      ok = 0
    } else {
    }
    i = i + 1
  }
  emit("exp_log_roundtrip"utf8, ok == 1)

  [array<f32>] rounds{array<f32>(
    1.49f32, 1.5f32, 1.51f32, -1.49f32, -1.5f32, -1.51f32
  )}
  [int mut] j{0}
  [int mut] ok2{1}
  while(j < rounds.count()) {
    [f32] v{rounds[j]}
    [f32] r{round(v)}
    if(abs(r - v) > 1.0f32) {
      ok2 = 0
    } else {
    }
    j = j + 1
  }
  emit("rounding_boundaries"utf8, ok2 == 1)

  return(0i32)
}
)";
  checkMathConformance(source, "math_conformance_exp_log_samples");
}

TEST_CASE("math conformance fixed seed samples") {
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
  [array<f32>] samples{array<f32>(
    0.137f32, 0.482f32, 0.911f32, 1.273f32, 2.047f32,
    -0.143f32, -0.577f32, -1.113f32, -2.333f32
  )}
  [int mut] i{0}
  [int mut] ok{1}
  while(i < samples.count()) {
    [f32] x{samples[i]}
    [f32] s{sin(x)}
    [f32] c{cos(x)}
    if(abs(s * s + c * c - 1.0f32) > 0.05f32) {
      ok = 0
    } else {
    }
    if(abs(exp(log(abs(x) + 1.0f32)) - (abs(x) + 1.0f32)) > 0.05f32) {
      ok = 0
    } else {
    }
    i = i + 1
  }
  emit("fixed_seed_samples"utf8, ok == 1)

  return(0i32)
}
)";
  checkMathConformance(source, "math_conformance_fixed_seed");
}

TEST_CASE("math conformance conversions and comparisons") {
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
  emit("convert_i32_f32"utf8, near(convert<f32>(42i32), 42.0f32, 0.001f32))
  emit("convert_i64_f32"utf8, near(convert<f32>(12345i64), 12345.0f32, 0.01f32))
  emit("convert_f32_i32"utf8, convert<int>(1.9f32) == 1i32)
  emit("convert_f32_i64"utf8, convert<i64>(-2.9f32) == -2i64)
  emit("cmp_nan_eq"utf8, equal(0.0f32 / 0.0f32, 0.0f32) == false)
  emit("cmp_inf_gt"utf8, greater_than(1.0f32 / 0.0f32, 1000.0f32))

  return(0i32)
}
)";
  checkMathConformance(source, "math_conformance_conversions");
}

TEST_CASE("math conformance convert non-finite float to int") {
  auto runCase = [&](const std::string &name, const std::string &valueExpr) {
    const std::string source = std::string(R"(
import /std/math/*

[return<int>]
main() {
  [f32] value{)") + valueExpr + R"(}
  return(convert<i32>(value))
}
)";
    const std::string srcPath = writeTemp("math_conformance_convert_nonfinite_" + name + ".prime", source);
    const std::string vmErrPath =
        (testScratchPath("") / ("math_conformance_convert_nonfinite_" + name + "_vm.err")).string();

    const std::string vmCmd =
        "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main 2> " + quoteShellArg(vmErrPath);
    CHECK(runCommand(vmCmd) == 3);
    CHECK(readFile(vmErrPath) == "float to int conversion requires finite value\n");

#if defined(__APPLE__) && (defined(__arm64__) || defined(__aarch64__))
    const std::string nativePath =
        (testScratchPath("") / ("math_conformance_convert_nonfinite_" + name + "_native")).string();
    const std::string nativeErrPath =
        (testScratchPath("") / ("math_conformance_convert_nonfinite_" + name + "_native.err")).string();
    const std::string nativeCompileCmd = "./primec --emit=native " + quoteShellArg(srcPath) + " -o " +
                                         quoteShellArg(nativePath) + " --entry /main";
    CHECK(runCommand(nativeCompileCmd) == 0);
    CHECK(runCommand(quoteShellArg(nativePath) + " 2> " + quoteShellArg(nativeErrPath)) == 3);
    CHECK(readFile(nativeErrPath) == "float to int conversion requires finite value\n");
#endif
  };

  runCase("nan", "0.0f32 / 0.0f32");
  runCase("pos_inf", "1.0f32 / 0.0f32");
  runCase("neg_inf", "-1.0f32 / 0.0f32");
}

TEST_CASE("math conformance policy behavior") {
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
  [f32] pow_neg_exp{pow(2.0f32, -1.0f32)}
  emit("pow_neg_exp"utf8, is_finite(pow_neg_exp))
  emit("pow_neg_exp_lt1"utf8, pow_neg_exp < 1.0f32)

  [f32] log_neg{log(-2.0f32)}
  emit("log_neg_nan"utf8, is_nan(log_neg))

  [f32] sqrt_neg{sqrt(-4.0f32)}
  emit("sqrt_neg_nan"utf8, is_nan(sqrt_neg))

  [f32] clamp_swapped{clamp(0.5f32, 1.0f32, 0.0f32)}
  emit("clamp_swapped_finite"utf8, is_finite(clamp_swapped))

  return(0i32)
}
)";
  checkMathConformanceVmParity(source, "math_conformance_policy");
}

TEST_CASE("math conformance integer pow negative exponent") {
  const std::string source = R"(
import /std/math/*

[return<int>]
main() {
  return(pow(2i32, -1i32))
}
)";
  const std::string srcPath = writeTemp("math_conformance_pow_negative.prime", source);
  const std::string vmErrPath =
      (testScratchPath("") / "math_conformance_pow_negative_vm.err").string();

  const std::string vmCmd =
      "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main 2> " + quoteShellArg(vmErrPath);
  CHECK(runCommand(vmCmd) == 3);
  CHECK(readFile(vmErrPath) == "pow exponent must be non-negative\n");

#if defined(__APPLE__) && (defined(__arm64__) || defined(__aarch64__))
  const std::string nativePath =
      (testScratchPath("") / "math_conformance_pow_negative_native").string();
  const std::string nativeErrPath =
      (testScratchPath("") / "math_conformance_pow_negative_native.err").string();
  const std::string nativeCompileCmd = "./primec --emit=native " + quoteShellArg(srcPath) + " -o " +
                                       quoteShellArg(nativePath) + " --entry /main";
  CHECK(runCommand(nativeCompileCmd) == 0);
  CHECK(runCommand(quoteShellArg(nativePath) + " 2> " + quoteShellArg(nativeErrPath)) == 3);
  CHECK(readFile(nativeErrPath) == "pow exponent must be non-negative\n");
#endif
}

TEST_CASE("math conformance integer edge cases") {
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
  emit("abs_i_min"utf8, not_equal(abs(-2147483648i32), 0i32))
  emit("sign_i_min"utf8, sign(-2147483648i32) == -1i32)
  emit("clamp_i_minmax"utf8, clamp(-5i32, -3i32, 3i32) == -3i32)
  emit("clamp_i_hi"utf8, clamp(5i32, -3i32, 3i32) == 3i32)
  emit("saturate_i_low"utf8, saturate(-10i32) == 0i32)
  emit("saturate_i_mid"utf8, saturate(0i32) == 0i32)
  emit("saturate_i_hi"utf8, saturate(10i32) == 1i32)

  [i32] pow_big{pow(2i32, 20i32)}
  emit("pow_big"utf8, pow_big == 1048576i32)

  return(0i32)
}
)";
  checkMathConformance(source, "math_conformance_int_edges");
}

TEST_CASE("math conformance atan2 edges") {
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
  [f32] pi_f{convert<f32>(pi)}
  [f32] half_pi{pi_f / 2.0f32}

  [f32] up{atan2(1.0f32, 0.0f32)}
  [f32] down{atan2(-1.0f32, 0.0f32)}
  [f32] left{atan2(0.0f32, -1.0f32)}
  [f32] right{atan2(0.0f32, 1.0f32)}

  emit("atan2_up"utf8, abs(up - half_pi) < 0.05f32)
  emit("atan2_down"utf8, abs(down + half_pi) < 0.05f32)
  emit("atan2_left"utf8, abs(left - pi_f) < 0.05f32)
  emit("atan2_right"utf8, abs(right) < 0.05f32)

  return(0i32)
}
)";
  checkMathConformance(source, "math_conformance_atan2_edges");
}

TEST_SUITE_END();
