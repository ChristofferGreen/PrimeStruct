#pragma once

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
  std::filesystem::path readmePath = std::filesystem::current_path() / "tools" / "PrimeStructc" / "README.md";
  if (!std::filesystem::exists(readmePath)) {
    readmePath = std::filesystem::current_path().parent_path() / "tools" / "PrimeStructc" / "README.md";
  }
  CHECK(std::filesystem::exists(readmePath));
  const std::string contents = readFile(readmePath.string());
  CHECK(contents.find("intentionally minimal") != std::string::npos);
  CHECK(contents.find("template codegen") != std::string::npos);
  CHECK(contents.find("version") != std::string::npos);
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

