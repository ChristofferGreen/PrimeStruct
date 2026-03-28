#pragma once

TEST_CASE("math conformance float64 basics") {
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
near([f64] a, [f64] b, [f64] eps) {
  return(abs(a - b) <= eps)
}

[return<int>]
main() {
  [f64] pi_d{convert<f64>(pi)}
  [f64] tau_d{convert<f64>(tau)}
  [f64] half_pi{pi_d / 2.0f64}
  [f64] quarter_pi{pi_d / 4.0f64}

  emit("sin_0_d"utf8, near(sin(0.0f64), 0.0f64, 0.0001f64))
  emit("cos_0_d"utf8, near(cos(0.0f64), 1.0f64, 0.0001f64))
  emit("sin_pi_d"utf8, abs(sin(pi_d)) < 0.05f64)
  emit("cos_pi_d"utf8, abs(cos(pi_d) + 1.0f64) < 0.05f64)
  emit("tan_qp_d"utf8, abs(tan(quarter_pi) - 1.0f64) < 0.05f64)
  emit("wrap_tau_d"utf8, abs(sin(tau_d + 0.5f64) - sin(0.5f64)) < 0.01f64)

  [f64] exp1{exp(1.0f64)}
  emit("exp_1_d"utf8, exp1 > 2.0f64)
  emit("exp_1_hi_d"utf8, exp1 < 4.0f64)

  [f64] log_e{log(convert<f64>(e))}
  emit("log_e_low_d"utf8, log_e > 0.5f64)
  emit("log_e_hi_d"utf8, log_e < 1.5f64)

  emit("sqrt_9_d"utf8, near(sqrt(9.0f64), 3.0f64, 0.05f64))
  emit("pow_1_d"utf8, abs(pow(1.0f64, 10.0f64) - 1.0f64) < 0.01f64)

  return(0i32)
}
)";
  checkMathConformance(source, "math_conformance_f64");
}

TEST_CASE("math conformance float64 inverse trig and logs") {
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
near([f64] a, [f64] b, [f64] eps) {
  return(abs(a - b) <= eps)
}

[return<int>]
main() {
  [f64] pi_d{convert<f64>(pi)}
  [f64] half_pi{pi_d / 2.0f64}
  [f64] quarter_pi{pi_d / 4.0f64}

  emit("asin_half_d"utf8, abs(asin(0.5f64) - pi_d / 6.0f64) < 0.1f64)
  emit("acos_half_d"utf8, abs(acos(0.5f64) - pi_d / 3.0f64) < 0.1f64)
  emit("atan_1_d"utf8, abs(atan(1.0f64) - quarter_pi) < 0.1f64)
  emit("atan2_q1_d"utf8, abs(atan2(1.0f64, 1.0f64) - quarter_pi) < 0.1f64)
  emit("atan2_axis_d"utf8, abs(atan2(1.0f64, 0.0f64) - half_pi) < 0.1f64)

  [f64] log2_8{log2(8.0f64)}
  emit("log2_8_d_low"utf8, log2_8 > 2.0f64)
  emit("log2_8_d_high"utf8, log2_8 < 4.0f64)
  [f64] log10_100{log10(100.0f64)}
  emit("log10_100_d_low"utf8, log10_100 > 1.0f64)
  emit("log10_100_d_high"utf8, log10_100 < 3.0f64)

  return(0i32)
}
)";
  checkMathConformance(source, "math_conformance_f64_inv");
}

TEST_CASE("math conformance float64 hyperbolic") {
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
  [f64] x{1.1f64}
  [f64] s{sinh(x)}
  [f64] c{cosh(x)}
  [f64] t{tanh(x)}

  emit("sinh_0_d"utf8, abs(sinh(0.0f64)) < 0.001f64)
  emit("cosh_0_d"utf8, abs(cosh(0.0f64) - 1.0f64) < 0.001f64)
  emit("tanh_0_d"utf8, abs(tanh(0.0f64)) < 0.001f64)
  emit("sinh_neg_d"utf8, abs(sinh(-0.7f64) + sinh(0.7f64)) < 0.01f64)
  emit("cosh_neg_d"utf8, abs(cosh(-0.7f64) - cosh(0.7f64)) < 0.01f64)
  emit("tanh_neg_d"utf8, abs(tanh(-0.7f64) + tanh(0.7f64)) < 0.01f64)
  emit("cosh_sinh_id_d"utf8, abs(c * c - s * s - 1.0f64) < 0.1f64)
  emit("tanh_range_d"utf8, abs(t) <= 1.0f64)

  [f64] as{asinh(1.2f64)}
  [f64] ac{acosh(2.0f64)}
  [f64] at{atanh(0.25f64)}
  emit("asinh_round_d"utf8, abs(sinh(as) - 1.2f64) < 0.05f64)
  emit("acosh_round_d"utf8, abs(cosh(ac) - 2.0f64) < 0.1f64)
  emit("atanh_round_d"utf8, abs(tanh(at) - 0.25f64) < 0.05f64)

  return(0i32)
}
)";
  checkMathConformance(source, "math_conformance_f64_hyper");
}

TEST_CASE("math conformance float64 grids") {
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
  [array<f64>] grid{array<f64>(
    -3.0f64, -2.0f64, -1.0f64, -0.5f64, 0.0f64, 0.5f64, 1.0f64, 2.0f64, 3.0f64
  )}
  [int mut] i{0}
  [int mut] ok{1}
  while(i < grid.count()) {
    [f64] x{grid[i]}
    [f64] s{sin(x)}
    [f64] c{cos(x)}
    if(abs(s * s + c * c - 1.0f64) > 0.1f64) {
      ok = 0
    } else {
    }
    i = i + 1
  }
  emit("trig_identity_f64_grid"utf8, ok == 1)

  [array<f64>] pos{array<f64>(0.5f64, 1.0f64, 1.5f64, 2.0f64, 3.0f64)}
  [int mut] j{0}
  [int mut] ok2{1}
  [f64 mut] prev{-999.0f64}
  while(j < pos.count()) {
    [f64] x{pos[j]}
    [f64] l{log(x)}
    if(j > 0) {
      if(l < prev) {
        ok2 = 0
      } else {
      }
    } else {
    }
    prev = l
    j = j + 1
  }
  emit("log_monotonic_f64"utf8, ok2 == 1)

  return(0i32)
}
)";
  checkMathConformance(source, "math_conformance_f64_grid");
}

TEST_CASE("math conformance float64 rounding") {
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
  emit("floor_pos_d"utf8, convert<i64>(floor(1.9f64)) == 1i64)
  emit("ceil_pos_d"utf8, convert<i64>(ceil(1.1f64)) == 2i64)
  emit("trunc_pos_d"utf8, convert<i64>(trunc(1.9f64)) == 1i64)
  emit("round_pos_d"utf8, convert<i64>(round(1.4f64)) == 1i64)
  emit("round_pos_half_d"utf8, convert<i64>(round(1.5f64)) == 2i64)

  emit("floor_neg_d"utf8, convert<i64>(floor(-1.1f64)) == -2i64)
  emit("ceil_neg_d"utf8, convert<i64>(ceil(-1.9f64)) == -1i64)
  emit("trunc_neg_d"utf8, convert<i64>(trunc(-1.9f64)) == -1i64)
  emit("round_neg_d"utf8, convert<i64>(round(-1.4f64)) == -1i64)
  emit("round_neg_half_d"utf8, convert<i64>(round(-1.5f64)) == -2i64)

  emit("fract_pos_d"utf8, abs(fract(1.75f64) - 0.75f64) < 0.01f64)
  emit("fract_neg_d"utf8, abs(fract(-1.25f64) - 0.75f64) < 0.02f64)

  return(0i32)
}
)";
  checkMathConformance(source, "math_conformance_f64_round");
}

TEST_CASE("math conformance roots and pow") {
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
  [f32] root2{sqrt(2.0f32)}
  emit("sqrt_4"utf8, near(sqrt(4.0f32), 2.0f32, 0.01f32))
  emit("sqrt_1"utf8, near(sqrt(1.0f32), 1.0f32, 0.01f32))
  emit("sqrt_0"utf8, near(sqrt(0.0f32), 0.0f32, 0.001f32))
  emit("sqrt_roundtrip"utf8, abs(root2 * root2 - 2.0f32) < 0.02f32)
  emit("sqrt_nan"utf8, is_nan(sqrt(-1.0f32)))
  emit("pow_zero_exp"utf8, pow(2i32, 0i32) == 1i32)
  emit("pow_zero_base"utf8, pow(0i32, 3i32) == 0i32)
  emit("pow_zero_zero"utf8, pow(0i32, 0i32) == 1i32)
  emit("pow_two_three"utf8, pow(2i32, 3i32) == 8i32)
  emit("pow_ten_two"utf8, pow(10i32, 2i32) == 100i32)
  emit("pow_one_any"utf8, pow(1i32, 5i32) == 1i32)
  emit("pow_neg_odd"utf8, pow(-2i32, 3i32) == -8i32)
  emit("pow_neg_even"utf8, pow(-2i32, 2i32) == 4i32)
  emit("pow_int"utf8, pow(2i32, 6i32) == 64i32)
  emit("pow_float"utf8, abs(pow(2.0f32, 0.5f32) - root2) < 0.05f32)
  emit("pow_float_nan"utf8, is_nan(pow(-2.0f32, 0.5f32)))

  return(0i32)
}
)";
  checkMathConformance(source, "math_conformance_roots_pow");
}

TEST_CASE("math conformance rounding") {
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
  emit("floor_pos"utf8, convert<int>(floor(1.9f32)) == 1i32)
  emit("ceil_pos"utf8, convert<int>(ceil(1.1f32)) == 2i32)
  emit("trunc_pos"utf8, convert<int>(trunc(1.9f32)) == 1i32)
  emit("round_pos"utf8, convert<int>(round(1.4f32)) == 1i32)
  emit("round_pos_half"utf8, convert<int>(round(1.5f32)) == 2i32)

  emit("floor_neg"utf8, convert<int>(floor(-1.1f32)) == -2i32)
  emit("ceil_neg"utf8, convert<int>(ceil(-1.9f32)) == -1i32)
  emit("trunc_neg"utf8, convert<int>(trunc(-1.9f32)) == -1i32)
  emit("round_neg"utf8, convert<int>(round(-1.4f32)) == -1i32)
  emit("round_neg_half"utf8, convert<int>(round(-1.5f32)) == -2i32)

  emit("fract_pos"utf8, abs(fract(1.75f32) - 0.75f32) < 0.01f32)
  emit("fract_neg"utf8, abs(fract(-1.25f32) - 0.75f32) < 0.02f32)
  [f32] fx{1.75f32}
  [f32] fxn{-1.25f32}
  emit("fract_consistency_pos"utf8, abs(fract(fx) - (fx - floor(fx))) < 0.01f32)
  emit("fract_consistency_neg"utf8, abs(fract(fxn) - (fxn - floor(fxn))) < 0.01f32)
  emit("floor_edge"utf8, convert<int>(floor(2.0f32 - 0.0001f32)) == 1i32)
  emit("ceil_edge"utf8, convert<int>(ceil(2.0f32 + 0.0001f32)) == 3i32)

  return(0i32)
}
)";
  checkMathConformance(source, "math_conformance_rounding");
}

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

