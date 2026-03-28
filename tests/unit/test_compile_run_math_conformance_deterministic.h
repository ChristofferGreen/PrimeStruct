#pragma once

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

