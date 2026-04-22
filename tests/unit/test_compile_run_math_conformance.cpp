#include "test_compile_run_helpers.h"
#include "test_compile_run_math_conformance_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.math_conformance");


TEST_CASE("math conformance reference printer script") {
  if (!hasPython3()) {
    return;
  }
  std::filesystem::path scriptPath = std::filesystem::current_path() / "tools" / "print_math_refs.py";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path().parent_path() / "tools" / "print_math_refs.py";
  }
  CHECK(std::filesystem::exists(scriptPath));
  const std::string outPath = (testScratchPath("") / "primec_math_refs.txt").string();
  const std::string command = "python3 " + quoteShellArg(scriptPath.string()) +
                              " --values sin=0.5 > " + quoteShellArg(outPath);
  CHECK(runCommand(command) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("# Python") != std::string::npos);
  CHECK(output.find("sin:") != std::string::npos);
  CHECK(output.find("0.5") != std::string::npos);
}

TEST_CASE("math conformance PrimeStructc policy docs") {
  std::filesystem::path specPath = std::filesystem::current_path() / "docs" / "PrimeStruct.md";
  if (!std::filesystem::exists(specPath)) {
    specPath = std::filesystem::current_path().parent_path() / "docs" / "PrimeStruct.md";
  }
  CHECK(std::filesystem::exists(specPath));
  const std::string contents = readFile(specPath.string());
  CHECK(contents.find("tools/PrimeStructc") != std::string::npos);
  CHECK(contents.find("PrimeStructc stays a minimal subset") != std::string::npos);
  CHECK(contents.find("template codegen") != std::string::npos);
  CHECK(contents.find("import version selection are explicitly out of scope for v1") != std::string::npos);
}

TEST_CASE("math conformance labeled output allowlist") {
  const std::string baselineText = "ok 1\nskip 0\n";
  const std::string candidateText = "ok 1\nskip 1\n";
  const std::vector<LabeledSample> baseline = parseLabeledOutput(baselineText, "allowlist base");
  const std::vector<LabeledSample> candidate = parseLabeledOutput(candidateText, "allowlist candidate");
  const std::unordered_set<std::string> allowlist = {"skip"};
  compareLabeledOutputs(baseline, candidate, "native", allowlist);
}

TEST_CASE("math conformance sign and range helpers") {
  const ParsedFloat pos = parseFloatToken("1.5", "pos");
  const ParsedFloat neg = parseFloatToken("-2.0", "neg");
  const ParsedFloat negZero = parseFloatToken("-0.0", "neg_zero");
  const ParsedFloat posZero = parseFloatToken("0.0", "pos_zero");
  CHECK(signMatches(pos, parseFloatToken("9.0", "pos2")));
  CHECK_FALSE(signMatches(pos, neg));
  CHECK(signMatches(negZero, parseFloatToken("-0.0", "neg_zero2")));
  CHECK_FALSE(signMatches(negZero, posZero));
  CHECK(inRange(pos, 1.0, 2.0));
  CHECK_FALSE(inRange(pos, 2.0, 3.0));
}

TEST_CASE("math conformance batch emit helper") {
  const std::vector<EmitCase> cases = {
      {"abs_zero", "abs(0.0f32) == 0.0f32"},
      {"sin_small", "abs(sin(0.1f32)) < 0.2f32"},
      {"cos_small", "cos(0.1f32) > 0.9f32"},
  };
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
)" + renderEmitCases(cases) + R"(
  return(0i32)
}
)";
  checkMathConformance(source, "math_conformance_batch_emit");
}

TEST_CASE("math conformance constants") {
  // Golden constants verified with Python 3.12.12 (math.pi/math.tau/math.e).
  // float32 rounding (IEEE-754) from Python 3.12.12:
  // pi -> 3.1415927410125732, tau -> 6.2831854820251465, e -> 2.7182817459106445.
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
near32([f32] a, [f32] b, [f32] eps) {
  return(abs(a - b) <= eps)
}

[bool]
near64([f64] a, [f64] b, [f64] eps) {
  return(abs(a - b) <= eps)
}

[return<int>]
main() {
  [f32] pi_f32_expected{3.1415927f32}
  [f32] tau_f32_expected{6.2831855f32}
  [f32] e_f32_expected{2.7182817f32}
  [f32] pi_f32{convert<f32>(pi)}
  [f32] tau_f32{convert<f32>(tau)}
  [f32] e_f32{convert<f32>(e)}
  emit("pi_f32_value"utf8, near32(pi_f32, pi_f32_expected, 0.00005f32))
  emit("tau_f32_twopi"utf8, near32(tau_f32, 2.0f32 * pi_f32, 0.0005f32))
  emit("e_f32_value"utf8, near32(e_f32, e_f32_expected, 0.00005f32))
  emit("tau_f32_value"utf8, near32(tau_f32, tau_f32_expected, 0.0001f32))

  [f64] pi_f64_expected{3.141592653589793f64}
  [f64] tau_f64_expected{6.283185307179586f64}
  [f64] e_f64_expected{2.718281828459045f64}
  [f64] pi_f64{convert<f64>(pi)}
  [f64] tau_f64{convert<f64>(tau)}
  [f64] e_f64{convert<f64>(e)}
  emit("tau_f64_twopi"utf8, near64(tau_f64, 2.0f64 * pi_f64, 0.0000005f64))
  emit("pi_f64_value"utf8, near64(pi_f64, pi_f64_expected, 0.0000000005f64))
  emit("tau_f64_value"utf8, near64(tau_f64, tau_f64_expected, 0.0000000005f64))
  emit("e_f64_value"utf8, near64(e_f64, e_f64_expected, 0.0000000005f64))

  [f64] pi_round{convert<f64>(pi_f32)}
  [f64] tau_round{convert<f64>(tau_f32)}
  [f64] e_round{convert<f64>(e_f32)}
  emit("pi_round_trip"utf8, near64(pi_round, pi_f64, 0.00001f64))
  emit("tau_round_trip"utf8, near64(tau_round, tau_f64, 0.00001f64))
  emit("e_round_trip"utf8, near64(e_round, e_f64, 0.00001f64))
  return(0i32)
}
)";
  checkMathConformance(source, "math_conformance_constants");
}

TEST_CASE("math conformance exp log basics") {
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
  emit("exp_zero"utf8, near(exp(0.0f32), 1.0f32, 0.0002f32))
  emit("exp2_zero"utf8, near(exp2(0.0f32), 1.0f32, 0.0002f32))
  emit("log_one"utf8, near(log(1.0f32), 0.0f32, 0.0002f32))
  emit("log2_one"utf8, near(log2(1.0f32), 0.0f32, 0.0002f32))
  emit("log10_one"utf8, near(log10(1.0f32), 0.0f32, 0.0002f32))
  emit("log2_eight"utf8, near(log2(8.0f32), 3.0f32, 0.2f32))
  emit("log10_thousand"utf8, near(log10(1000.0f32), 3.0f32, 0.3f32))
  emit("exp_log_roundtrip"utf8, near(exp(log(2.0f32)), 2.0f32, 0.002f32))
  emit("log_exp_roundtrip"utf8, near(log(exp(1.0f32)), 1.0f32, 0.002f32))
  [array<f32>] exp_log_values{array<f32>(0.5f32, 1.0f32, 2.0f32, 10.0f32)}
  [int mut] exp_log_idx{0}
  [int mut] exp_log_ok{1}
  while(exp_log_idx < exp_log_values.count()) {
    [f32] x{exp_log_values[exp_log_idx]}
    [f32] ratio{exp(log(x)) / x}
    if(ratio < 0.5f32 || ratio > 2.0f32) {
      exp_log_ok = 0
    } else {
    }
    exp_log_idx = exp_log_idx + 1
  }
  emit("exp_log_grid"utf8, exp_log_ok == 1)
  [array<f32>] log_exp_values{array<f32>(-2.0f32, -1.0f32, 0.0f32, 1.0f32, 2.0f32)}
  [int mut] log_exp_idx{0}
  [int mut] log_exp_ok{1}
  while(log_exp_idx < log_exp_values.count()) {
    [f32] x{log_exp_values[log_exp_idx]}
    if(abs(log(exp(x)) - x) > 0.05f32) {
      log_exp_ok = 0
    } else {
    }
    log_exp_idx = log_exp_idx + 1
  }
  emit("log_exp_grid"utf8, log_exp_ok == 1)
  return(0i32)
}
)";
  checkMathConformance(source, "math_conformance_exp_log_basics");
}

TEST_CASE("math conformance roots") {
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
  emit("sqrt_4"utf8, near(sqrt(4.0f32), 2.0f32, 0.01f32))
  emit("sqrt_9"utf8, near(sqrt(9.0f32), 3.0f32, 0.01f32))
  emit("sqrt_64"utf8, near(sqrt(64.0f32), 8.0f32, 0.01f32))

  emit("cbrt_27"utf8, near(cbrt(27.0f32), 3.0f32, 0.01f32))
  emit("cbrt_64"utf8, near(cbrt(64.0f32), 4.0f32, 0.2f32))
  emit("cbrt_neg"utf8, near(cbrt(-8.0f32), -2.0f32, 0.01f32))

  [array<f32>] roots{array<f32>(2.0f32, 3.0f32, 5.0f32, 10.0f32)}
  [int mut] root_idx{0}
  [int mut] root_ok{1}
  while(root_idx < roots.count()) {
    [f32] x{roots[root_idx]}
    [f32] r{sqrt(x)}
    if(abs(r * r - x) > 0.03f32) {
      root_ok = 0
    } else {
    }
    root_idx = root_idx + 1
  }
  emit("sqrt_roundtrip"utf8, root_ok == 1)
  return(0i32)
}
)";
  checkMathConformance(source, "math_conformance_roots");
}

TEST_CASE("math conformance trig quadrants axes") {
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
  [f32] three_half_pi{pi_f * 1.5f32}
  [f32] two_pi{pi_f * 2.0f32}

  emit("sin_0"utf8, near(sin(0.0f32), 0.0f32, 0.0002f32))
  emit("cos_0"utf8, near(cos(0.0f32), 1.0f32, 0.0002f32))
  emit("sin_half_pi"utf8, near(sin(half_pi), 1.0f32, 0.02f32))
  emit("cos_half_pi"utf8, abs(cos(half_pi)) < 0.02f32)
  emit("sin_pi"utf8, abs(sin(pi_f)) < 0.02f32)
  emit("cos_pi"utf8, near(cos(pi_f), -1.0f32, 0.02f32))
  emit("sin_three_half_pi"utf8, near(sin(three_half_pi), -1.0f32, 0.02f32))
  emit("cos_three_half_pi"utf8, abs(cos(three_half_pi)) < 0.02f32)
  emit("sin_two_pi"utf8, abs(sin(two_pi)) < 0.02f32)
  emit("cos_two_pi"utf8, near(cos(two_pi), 1.0f32, 0.02f32))

  emit("tan_quarter_pi"utf8, abs(tan(quarter_pi) - 1.0f32) < 0.02f32)
  return(0i32)
}
)";
  checkMathConformance(source, "math_conformance_trig_quadrants_axes");
}

TEST_CASE("math conformance trig quadrants symmetries") {
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
  [f32] three_half_pi{pi_f * 1.5f32}

  [f32] x{0.7f32}
  emit("sin_sym"utf8, abs(sin(-x) + sin(x)) < 0.002f32)
  emit("cos_sym"utf8, abs(cos(-x) - cos(x)) < 0.002f32)
  emit("tan_sym"utf8, abs(tan(-x) + tan(x)) < 0.01f32)

  [f32] ident{sin(x) * sin(x) + cos(x) * cos(x)}
  emit("pyth_identity"utf8, abs(ident - 1.0f32) < 0.01f32)

  [f32] small{0.0001f32}
  emit("sin_small_angle"utf8, abs(sin(small) - small) < 0.0002f32)
  emit("tan_identity"utf8, abs(tan(0.3f32) - sin(0.3f32) / cos(0.3f32)) < 0.02f32)
  [f32] eps{0.001f32}
  emit("cos_cross_half_pi"utf8, cos(half_pi - eps) > 0.0f32 && cos(half_pi + eps) < 0.0f32)
  emit("cos_cross_three_half_pi"utf8, cos(three_half_pi - eps) < 0.0f32 && cos(three_half_pi + eps) > 0.0f32)
  return(0i32)
}
)";
  checkMathConformance(source, "math_conformance_trig_quadrants_symmetry");
}

TEST_CASE("math conformance trig quadrants range reduction") {
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
  [f32] tau_f{pi_f * 2.0f32}
  [f32] eps{0.001f32}

  emit("sin_cross_zero"utf8, sin(eps) > 0.0f32 && sin(-eps) < 0.0f32)
  emit("sin_cross_pi"utf8, sin(pi_f - eps) > 0.0f32 && sin(pi_f + eps) < 0.0f32)

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
  checkMathConformance(source, "math_conformance_trig_quadrants_range_reduce");
}

TEST_CASE("math conformance float helpers parse tokens") {
  const std::string output = "sin 0.5\ncos 0.6\nnan nan\ninf inf\nneg -inf\n";
  const std::vector<FloatSample> samples = parseFloatOutput(output, "float helper test");
  CHECK(samples.size() == 5);
  CHECK(samples[0].label == "sin");
  CHECK(samples[0].value.isNan == false);
  CHECK(samples[0].value.isInf == false);
  CHECK(absDiff(samples[0].value.value, 0.5) < 1e-8);
  CHECK(samples[2].value.isNan);
  CHECK(samples[3].value.isInf);
  CHECK(samples[4].value.isInf);
  CHECK(std::signbit(samples[4].value.value));
}

TEST_CASE("math conformance float helpers compare tolerance") {
  const ParsedFloat base = parseFloatToken("1.0", "base");
  const ParsedFloat near = parseFloatToken("1.00005", "near");
  const ParsedFloat far = parseFloatToken("1.1", "far");
  CHECK(floatsNear(base, near, 1e-4, 1e-4));
  CHECK_FALSE(floatsNear(base, far, 1e-4, 1e-4));
  const ParsedFloat nanValue = parseFloatToken("nan", "nan");
  const ParsedFloat infValue = parseFloatToken("inf", "inf");
  const ParsedFloat negInfValue = parseFloatToken("-inf", "neg_inf");
  CHECK(floatsNear(nanValue, parseFloatToken("nan", "nan2"), 1e-6, 1e-6));
  CHECK_FALSE(floatsNear(nanValue, base, 1e-6, 1e-6));
  CHECK(floatsNear(infValue, parseFloatToken("inf", "inf2"), 1e-6, 1e-6));
  CHECK_FALSE(floatsNear(infValue, negInfValue, 1e-6, 1e-6));
}


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
    const std::string exePath =
        (testScratchPath("") / ("math_conformance_convert_nonfinite_" + name + "_exe")).string();
    const std::string exeErrPath =
        (testScratchPath("") / ("math_conformance_convert_nonfinite_" + name + "_exe.err")).string();
    const std::string vmErrPath =
        (testScratchPath("") / ("math_conformance_convert_nonfinite_" + name + "_vm.err")).string();

    const std::string compileCmd = "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " +
                                   quoteShellArg(exePath) + " --entry /main";
    CHECK(runCommand(compileCmd) == 0);
    CHECK(runCommand(quoteShellArg(exePath) + " 2> " + quoteShellArg(exeErrPath)) == 3);
    CHECK(readFile(exeErrPath) == "float to int conversion requires finite value\n");

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
  const std::string exePath =
      (testScratchPath("") / "math_conformance_pow_negative_exe").string();
  const std::string exeErrPath =
      (testScratchPath("") / "math_conformance_pow_negative_exe.err").string();
  const std::string vmErrPath =
      (testScratchPath("") / "math_conformance_pow_negative_vm.err").string();

  const std::string compileCmd = "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " +
                                 quoteShellArg(exePath) + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(quoteShellArg(exePath) + " 2> " + quoteShellArg(exeErrPath)) == 3);
  CHECK(readFile(exeErrPath) == "pow exponent must be non-negative\n");

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
