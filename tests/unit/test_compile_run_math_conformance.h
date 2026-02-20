#include <filesystem>
#include <string>

namespace {
std::string runMathConformance(const std::string &source, const std::string &name, const std::string &emitKind) {
  const std::string srcPath = writeTemp(name + ".prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / (name + "_" + emitKind + ".out")).string();

  if (emitKind == "vm") {
    const std::string runCmd =
        "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main > " + quoteShellArg(outPath);
    CHECK(runCommand(runCmd) == 0);
    return readFile(outPath);
  }

  const std::string exePath = (std::filesystem::temp_directory_path() / (name + "_" + emitKind + "_exe")).string();
  const std::string compileCmd = "./primec --emit=" + emitKind + " " + quoteShellArg(srcPath) + " -o " +
                                 quoteShellArg(exePath) + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = quoteShellArg(exePath) + " > " + quoteShellArg(outPath);
  CHECK(runCommand(runCmd) == 0);
  return readFile(outPath);
}

void checkMathConformance(const std::string &source, const std::string &name) {
  const std::string baseline = runMathConformance(source, name, "exe");
  const std::string vmOut = runMathConformance(source, name, "vm");
  CHECK(vmOut == baseline);
#if defined(__APPLE__) && (defined(__arm64__) || defined(__aarch64__))
  const std::string nativeOut = runMathConformance(source, name, "native");
  CHECK(nativeOut == baseline);
#endif
}
} // namespace

TEST_SUITE_BEGIN("primestruct.compile.run.math_conformance");

TEST_CASE("math conformance trig basics") {
  const std::string source = R"(
import /math/*

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
  return(0i32)
}
)";
  checkMathConformance(source, "math_conformance_trig");
}

TEST_CASE("math conformance inverse trig and atan2") {
  const std::string source = R"(
import /math/*

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
  emit("acos_0"utf8, near(acos(0.0f32), half_pi, 0.02f32))
  emit("acos_half"utf8, near(acos(0.5f32), pi_f / 3.0f32, 0.05f32))
  emit("acos_neg_half"utf8, near(acos(-0.5f32), pi_f * 2.0f32 / 3.0f32, 0.05f32))
  emit("atan_1_pos"utf8, atan(1.0f32) > 0.5f32)
  emit("atan_1_upper"utf8, atan(1.0f32) < 1.2f32)
  emit("atan_neg1_neg"utf8, atan(-1.0f32) < -0.5f32)
  emit("atan_neg1_upper"utf8, atan(-1.0f32) > -1.2f32)

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
import /math/*

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

  return(0i32)
}
)";
  checkMathConformance(source, "math_conformance_hyperbolic");
}

TEST_CASE("math conformance inverse hyperbolic") {
  const std::string source = R"(
import /math/*

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
  emit("atanh_0"utf8, abs(atanh(0.0f32)) < 0.001f32)
  emit("acosh_1"utf8, abs(acosh(1.0f32)) < 0.001f32)
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
import /math/*

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
import /math/*

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

TEST_CASE("math conformance float64 basics") {
  const std::string source = R"(
import /math/*

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
import /math/*

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
import /math/*

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
import /math/*

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
import /math/*

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
import /math/*

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
import /math/*

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
  emit("floor_edge"utf8, convert<int>(floor(2.0f32 - 0.0001f32)) == 1i32)
  emit("ceil_edge"utf8, convert<int>(ceil(2.0f32 + 0.0001f32)) == 3i32)

  return(0i32)
}
)";
  checkMathConformance(source, "math_conformance_rounding");
}

TEST_CASE("math conformance misc ops") {
  const std::string source = R"(
import /math/*

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
  emit("fma_f"utf8, near(fma(2.0f32, 3.0f32, 4.0f32), 10.0f32, 0.01f32))
  emit("hypot_f"utf8, near(hypot(3.0f32, 4.0f32), 5.0f32, 0.05f32))
  emit("hypot_swap"utf8, abs(hypot(3.0f32, 4.0f32) - hypot(4.0f32, 3.0f32)) < 0.01f32)
  emit("hypot_scale"utf8, abs(hypot(6.0f32, 8.0f32) - 10.0f32) < 0.1f32)
  emit("copysign_f"utf8, near(copysign(2.5f32, -1.0f32), -2.5f32, 0.001f32))
  emit("copysign_pos"utf8, near(copysign(-2.5f32, 1.0f32), 2.5f32, 0.001f32))

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
  emit("is_nan"utf8, is_nan(nan))
  emit("is_inf"utf8, is_inf(inf))
  emit("is_finite"utf8, is_finite(1.0f32))
  emit("is_finite_inf"utf8, is_finite(inf) == false)

  emit("radians_degrees"utf8, abs(radians(degrees(1.25f32)) - 1.25f32) < 0.0005f32)
  emit("degrees_radians"utf8, abs(degrees(radians(45.0f32)) - 45.0f32) < 0.01f32)
  emit("degrees_pi"utf8, abs(degrees(convert<f32>(pi)) - 180.0f32) < 1.0f32)

  return(0i32)
}
)";
  checkMathConformance(source, "math_conformance_misc");
}

TEST_CASE("math conformance stress grid") {
  const std::string source = R"(
import /math/*

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

TEST_CASE("math conformance dense grids") {
  const std::string source = R"(
import /math/*

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
import /math/*

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
import /math/*

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
import /math/*

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
import /math/*

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

TEST_CASE("math conformance policy behavior") {
  const std::string source = R"(
import /math/*

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
  checkMathConformance(source, "math_conformance_policy");
}

TEST_CASE("math conformance integer edge cases") {
  const std::string source = R"(
import /math/*

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
  emit("abs_i_min"utf8, abs(-2147483648i32) == -2147483648i32)
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
import /math/*

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
