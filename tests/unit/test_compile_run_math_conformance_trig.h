#pragma once

TEST_CASE("math conformance trig basics") {
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
  [f32] pi_f{convert<f32>(pi)}
  [f32] tau_f{convert<f32>(tau)}
  [f32] half_pi{pi_f / 2.0f32}
  [f32] quarter_pi{pi_f / 4.0f32}

  emit("sin_0"utf8, near(sin(0.0f32), 0.0f32, 0.0002f32))
  emit("cos_0"utf8, near(cos(0.0f32), 1.0f32, 0.0002f32))
  emit("sin_half_pi"utf8, near(sin(half_pi), 1.0f32, 0.02f32))
  emit("cos_half_pi"utf8, abs(cos(half_pi)) < 0.02f32)
  emit("sin_pi"utf8, abs(sin(pi_f)) < 0.02f32)
  emit("cos_pi"utf8, near(cos(pi_f), -1.0f32, 0.02f32))
  emit("sin_neg"utf8, abs(sin(-0.75f32) + sin(0.75f32)) < 0.002f32)
  emit("cos_neg"utf8, abs(cos(-0.75f32) - cos(0.75f32)) < 0.002f32)
  emit("tan_zero"utf8, abs(tan(0.0f32)) < 0.0002f32)
  emit("tan_quarter_pi"utf8, abs(tan(quarter_pi) - 1.0f32) < 0.02f32)
  emit("wrap_tau_sin"utf8, abs(sin(tau_f + 0.5f32) - sin(0.5f32)) < 0.003f32)
  emit("wrap_tau_cos"utf8, abs(cos(tau_f + 1.0f32) - cos(1.0f32)) < 0.003f32)
  emit("sin_large_sign"utf8, sin(20.0f32) > 0.0f32)
  emit("cos_large_sign"utf8, cos(20.0f32) > 0.0f32)
  [array<f32>] large_angles{array<f32>(10.0f32, 20.0f32, 100.0f32, 1000.0f32, 10000.0f32)}
  [int mut] idx{0}
  [int mut] ok_sin{1}
  [int mut] ok_cos{1}
  while(idx < large_angles.count()) {
    [f32] x{large_angles[idx]}
    [f32] cycles{floor(x / tau_f)}
    [f32] reduced{x - cycles * tau_f}
    if(abs(sin(x) - sin(reduced)) > 0.1f32) {
      ok_sin = 0
    } else {
    }
    if(abs(cos(x) - cos(reduced)) > 0.1f32) {
      ok_cos = 0
    } else {
    }
    idx = idx + 1
  }
  emit("sin_range_reduce"utf8, ok_sin == 1)
  emit("cos_range_reduce"utf8, ok_cos == 1)
  return(0i32)
}
)";
  checkMathConformance(source, "math_conformance_trig");
}

TEST_CASE("math conformance inverse trig and atan2") {
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
  [f32] pi_f{convert<f32>(pi)}
  [f32] half_pi{pi_f / 2.0f32}
  [f32] quarter_pi{pi_f / 4.0f32}

  emit("asin_0"utf8, near(asin(0.0f32), 0.0f32, 0.001f32))
  emit("asin_half"utf8, near(asin(0.5f32), pi_f / 6.0f32, 0.02f32))
  emit("asin_edge_pos"utf8, asin(1.0f32) > asin(0.5f32))
  emit("asin_edge_neg"utf8, asin(-1.0f32) < asin(-0.5f32))
  emit("asin_near_pos"utf8, asin(0.999f32) > asin(0.9f32))
  emit("asin_near_neg"utf8, asin(-0.999f32) < asin(-0.9f32))
  emit("acos_0"utf8, near(acos(0.0f32), half_pi, 0.02f32))
  emit("acos_half"utf8, near(acos(0.5f32), pi_f / 3.0f32, 0.05f32))
  emit("acos_neg_half"utf8, near(acos(-0.5f32), pi_f * 2.0f32 / 3.0f32, 0.05f32))
  emit("acos_edge_pos"utf8, acos(1.0f32) < acos(0.5f32))
  emit("acos_edge_neg"utf8, acos(-1.0f32) > acos(-0.5f32))
  emit("acos_near_pos"utf8, acos(0.999f32) < acos(0.9f32))
  emit("acos_near_neg"utf8, acos(-0.999f32) > acos(-0.9f32))
  emit("atan_1_pos"utf8, atan(1.0f32) > 0.5f32)
  emit("atan_1_upper"utf8, atan(1.0f32) < 1.2f32)
  emit("atan_neg1_neg"utf8, atan(-1.0f32) < -0.5f32)
  emit("atan_neg1_upper"utf8, atan(-1.0f32) > -1.2f32)
  emit("atan_1_near"utf8, abs(atan(1.0f32) - quarter_pi) < 0.2f32)
  emit("atan_neg1_near"utf8, abs(atan(-1.0f32) + quarter_pi) < 0.2f32)
  emit("atan_mono_zero"utf8, atan(-1.0f32) < atan(0.0f32) && atan(0.0f32) < atan(1.0f32))
  [array<f32>] atan_values{array<f32>(-10.0f32, -5.0f32, -1.0f32, 0.0f32, 1.0f32, 5.0f32, 10.0f32)}
  [int mut] atan_idx{1}
  [int mut] atan_ok{1}
  [f32 mut] atan_prev{atan(atan_values[0])}
  while(atan_idx < atan_values.count()) {
    [f32] atan_curr{atan(atan_values[atan_idx])}
    if(atan_curr + 0.0005f32 < atan_prev) {
      atan_ok = 0
    } else {
    }
    atan_prev = atan_curr
    atan_idx = atan_idx + 1
  }
  emit("atan_monotonic"utf8, atan_ok == 1)
  emit("asin_roundtrip_neg"utf8, abs(sin(asin(-0.7f32)) + 0.7f32) < 0.05f32)
  emit("asin_roundtrip_pos"utf8, abs(sin(asin(0.7f32)) - 0.7f32) < 0.05f32)
  emit("acos_roundtrip_neg"utf8, abs(cos(acos(-0.7f32)) + 0.7f32) < 0.05f32)
  emit("acos_roundtrip_pos"utf8, abs(cos(acos(0.7f32)) - 0.7f32) < 0.05f32)

  [f32] atan_q1{atan2(1.0f32, 1.0f32)}
  [f32] atan_q2{atan2(1.0f32, -1.0f32)}
  [f32] atan_q3{atan2(-1.0f32, -1.0f32)}
  [f32] atan_q4{atan2(-1.0f32, 1.0f32)}
  emit("atan2_q1_pos"utf8, atan_q1 > 0.0f32)
  emit("atan2_q1_upper"utf8, atan_q1 < half_pi)
  emit("atan2_q2_lower"utf8, atan_q2 > half_pi)
  emit("atan2_q2_upper"utf8, atan_q2 < pi_f)
  emit("atan2_q3_lower"utf8, atan_q3 < -half_pi)
  emit("atan2_q3_upper"utf8, atan_q3 > -pi_f)
  emit("atan2_q4_neg"utf8, atan_q4 < 0.0f32)
  emit("atan2_q4_lower"utf8, atan_q4 > -half_pi)
  emit("atan2_q1_known"utf8, abs(atan_q1 - quarter_pi) < 0.2f32)
  emit("atan2_sym"utf8, abs(abs(atan_q3 - atan_q1) - pi_f) < 0.1f32)
  emit("atan2_axis_up"utf8, near(atan2(1.0f32, 0.0f32), half_pi, 0.02f32))
  emit("atan2_axis_left"utf8, near(atan2(0.0f32, -1.0f32), pi_f, 0.02f32))
  emit("atan2_axis_down"utf8, near(atan2(-1.0f32, 0.0f32), -half_pi, 0.02f32))

  return(0i32)
}
)";
  checkMathConformance(source, "math_conformance_inv_trig");
}

TEST_CASE("math conformance hyperbolic") {
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
  [f32] x{1.2f32}
  [f32] s{sinh(x)}
  [f32] c{cosh(x)}

  emit("sinh_0"utf8, abs(sinh(0.0f32)) < 0.0002f32)
  emit("cosh_0"utf8, near(cosh(0.0f32), 1.0f32, 0.0002f32))
  emit("tanh_0"utf8, abs(tanh(0.0f32)) < 0.0002f32)
  emit("sinh_neg"utf8, abs(sinh(-0.7f32) + sinh(0.7f32)) < 0.002f32)
  emit("cosh_neg"utf8, abs(cosh(-0.7f32) - cosh(0.7f32)) < 0.002f32)
  emit("tanh_neg"utf8, abs(tanh(-0.7f32) + tanh(0.7f32)) < 0.002f32)
  emit("cosh_sinh_identity"utf8, abs(c * c - s * s - 1.0f32) < 0.02f32)
  emit("sinh_range_1"utf8, sinh(1.0f32) > 0.0f32)
  emit("sinh_range_2"utf8, sinh(2.0f32) > sinh(1.0f32))
  emit("sinh_range_5"utf8, sinh(5.0f32) > sinh(2.0f32))
  emit("cosh_range_1"utf8, cosh(1.0f32) > 1.0f32)
  emit("cosh_range_2"utf8, cosh(2.0f32) > cosh(1.0f32))
  emit("cosh_range_5"utf8, cosh(5.0f32) > cosh(2.0f32))
  emit("tanh_range_1"utf8, tanh(1.0f32) > 0.0f32 && tanh(1.0f32) < 1.0f32)
  emit("tanh_range_2"utf8, tanh(2.0f32) > 0.0f32 && tanh(2.0f32) < 1.0f32)
  emit("tanh_range_5"utf8, tanh(5.0f32) > 0.0f32 && tanh(5.0f32) < 1.0f32)

  return(0i32)
}
)";
  checkMathConformance(source, "math_conformance_hyperbolic");
}

TEST_CASE("math conformance inverse hyperbolic") {
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
  emit("asinh_0"utf8, abs(asinh(0.0f32)) < 0.001f32)
  emit("asinh_pos"utf8, asinh(2.0f32) > 0.0f32)
  emit("asinh_neg"utf8, asinh(-2.0f32) < 0.0f32)
  emit("atanh_0"utf8, abs(atanh(0.0f32)) < 0.001f32)
  emit("atanh_edge_pos"utf8, atanh(0.99f32) > 0.0f32 && is_finite(atanh(0.99f32)))
  emit("atanh_edge_neg"utf8, atanh(-0.99f32) < 0.0f32 && is_finite(atanh(-0.99f32)))
  emit("acosh_1"utf8, abs(acosh(1.0f32)) < 0.001f32)
  emit("acosh_mid"utf8, acosh(1.5f32) > 0.0f32 && is_finite(acosh(1.5f32)))
  emit("tanh_atanh"utf8, abs(tanh(atanh(0.3f32)) - 0.3f32) < 0.01f32)
  emit("sinh_asinh"utf8, abs(sinh(asinh(1.2f32)) - 1.2f32) < 0.01f32)
  emit("cosh_acosh"utf8, abs(cosh(acosh(2.0f32)) - 2.0f32) < 0.02f32)

  return(0i32)
}
)";
  checkMathConformance(source, "math_conformance_inv_hyperbolic");
}

TEST_CASE("math conformance exp and log") {
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
  emit("exp_0"utf8, near(exp(0.0f32), 1.0f32, 0.001f32))
  emit("exp2_0"utf8, near(exp2(0.0f32), 1.0f32, 0.001f32))
  emit("log_1"utf8, abs(log(1.0f32)) < 0.001f32)
  emit("log2_8"utf8, near(log2(8.0f32), 3.0f32, 0.2f32))
  [f32] log10_val{log10(1000.0f32)}
  emit("log10_1000_low"utf8, log10_val > 0.0f32)
  emit("log10_1000_high"utf8, log10_val < 4.0f32)
  emit("exp_log"utf8, abs(exp(log(1.5f32)) - 1.5f32) < 0.02f32)
  emit("log_exp"utf8, abs(log(exp(1.0f32)) - 1.0f32) < 0.02f32)

  return(0i32)
}
)";
  checkMathConformance(source, "math_conformance_exp_log");
}

TEST_CASE("math conformance exp/log domains") {
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
  [f32] log_zero{log(0.0f32)}
  emit("log_zero_inf"utf8, is_inf(log_zero))
  emit("log_zero_neg"utf8, near(sign(log_zero), -1.0f32, 0.001f32))

  emit("log_neg_nan"utf8, is_nan(log(-1.0f32)))
  emit("exp_neg_range"utf8, exp(-1.0f32) > 0.0f32)
  emit("exp_neg_lt1"utf8, exp(-1.0f32) < 1.0f32)

  emit("exp2_3_low"utf8, exp2(3.0f32) > 7.0f32)
  emit("exp2_3_high"utf8, exp2(3.0f32) < 9.0f32)
  emit("log2_half_low"utf8, log2(0.5f32) < -0.5f32)
  emit("log2_half_high"utf8, log2(0.5f32) > -1.5f32)

  emit("log10_10_low"utf8, log10(10.0f32) > 0.5f32)
  emit("log10_10_high"utf8, log10(10.0f32) < 1.5f32)

  return(0i32)
}
)";
  checkMathConformance(source, "math_conformance_exp_log_domains");
}

