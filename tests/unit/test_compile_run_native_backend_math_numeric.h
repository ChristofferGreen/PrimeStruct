#if defined(__APPLE__) && (defined(__arm64__) || defined(__aarch64__))
TEST_SUITE_BEGIN("primestruct.compile.run.native_backend.math_numeric");

TEST_CASE("compiles and runs native clamp") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  return(clamp(9i32, 2i32, 6i32))
}
)";
  const std::string srcPath = writeTemp("compile_native_clamp.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_clamp_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 6);
}

TEST_CASE("compiles and runs native clamp i64") {
  const std::string source = R"(
import /math/*
[return<bool>]
main() {
  return(equal(clamp(9i64, 2i64, 6i64), 6i64))
}
)";
  const std::string srcPath = writeTemp("compile_native_clamp_i64.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_clamp_i64_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs native math abs/sign/min/max") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  [i32] a{abs(-5i32)}
  [i32] b{sign(-5i32)}
  [i32] c{min(7i32, 2i32)}
  [i32] d{max(7i32, 2i32)}
  return(plus(plus(a, b), plus(c, d)))
}
)";
  const std::string srcPath = writeTemp("compile_native_math_basic.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_math_basic_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 13);
}

TEST_CASE("compiles and runs native qualified math names") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] a{/math/abs(-5i32)}
  [i32] b{/math/sign(-5i32)}
  [i32] c{/math/min(7i32, 2i32)}
  [i32] d{/math/max(7i32, 2i32)}
  [i32] e{convert<int>(/math/pi)}
  return(plus(plus(plus(a, b), plus(c, d)), e))
}
)";
  const std::string srcPath = writeTemp("compile_native_math_qualified.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_math_qualified_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 16);
}

TEST_CASE("compiles and runs native math saturate/lerp") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  [i32] a{saturate(-2i32)}
  [i32] b{saturate(2i32)}
  [i32] c{lerp(0i32, 10i32, 1i32)}
  return(plus(plus(a, b), c))
}
)";
  const std::string srcPath = writeTemp("compile_native_math_saturate_lerp.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_math_saturate_lerp_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 11);
}

TEST_CASE("compiles and runs native math clamp") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  [f32] a{clamp(2.5f32, 0.0f32, 1.0f32)}
  [u64] b{clamp(9u64, 2u64, 6u64)}
  return(plus(convert<int>(a), convert<int>(b)))
}
)";
  const std::string srcPath = writeTemp("compile_native_math_clamp.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_math_clamp_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs native math pow") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  return(pow(2i32, 6i32))
}
)";
  const std::string srcPath = writeTemp("compile_native_math_pow.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_math_pow_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 64);
}

TEST_CASE("compiles and runs native math pow rejects negative exponent") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  return(pow(2i32, -1i32))
}
)";
  const std::string srcPath = writeTemp("compile_native_math_pow_negative.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_math_pow_negative_exe").string();
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_native_math_pow_negative_err.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " 2> " + errPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(errPath) == "pow exponent must be non-negative\n");
}

TEST_CASE("compiles and runs native math constant conversions") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  return(plus(convert<int>(pi), plus(convert<int>(tau), convert<int>(e))))
}
)";
  const std::string srcPath = writeTemp("compile_native_math_constants_convert.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_math_constants_convert_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 11);
}

TEST_CASE("compiles and runs native math constants") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  [f64] sum{plus(pi, plus(tau, e))}
  return(convert<int>(sum))
}
)";
  const std::string srcPath = writeTemp("compile_native_math_constants_direct.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_math_constants_direct_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 12);
}

TEST_CASE("compiles and runs native math predicates") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  [f32] nan{divide(0.0f32, 0.0f32)}
  [f32] inf{divide(1.0f32, 0.0f32)}
  return(plus(
    plus(convert<int>(is_nan(nan)), convert<int>(is_inf(inf))),
    convert<int>(is_finite(1.0f32))
  ))
}
)";
  const std::string srcPath = writeTemp("compile_native_math_predicates.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_math_predicates_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("compiles and runs native math rounding") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  [f32] a{floor(1.9f32)}
  [f32] b{ceil(1.1f32)}
  [f32] c{round(2.5f32)}
  [f32] d{trunc(-1.7f32)}
  [f32] e{fract(2.25f32)}
  return(plus(
    plus(convert<int>(a), convert<int>(b)),
    plus(convert<int>(c), plus(convert<int>(d), convert<int>(multiply(e, 4.0f32))))
  ))
}
)";
  const std::string srcPath = writeTemp("compile_native_math_rounding.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_math_rounding_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 6);
}

TEST_CASE("compiles and runs native math roots") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  [f32] a{sqrt(9.0f32)}
  [f32] b{cbrt(27.0f32)}
  return(plus(convert<int>(a), convert<int>(b)))
}
)";
  const std::string srcPath = writeTemp("compile_native_math_roots.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_math_roots_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 6);
}

TEST_CASE("compiles and runs native math fma/hypot") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  [f32] a{fma(2.0f32, 3.0f32, 4.0f32)}
  [f32] b{hypot(1.0f32, 1.0f32)}
  return(plus(convert<int>(a), convert<int>(b)))
}
)";
  const std::string srcPath = writeTemp("compile_native_math_fma_hypot.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_math_fma_hypot_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 11);
}

TEST_CASE("compiles and runs native math copysign") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  [f32] a{copysign(3.0f32, -1.0f32)}
  [f32] b{copysign(4.0f32, 1.0f32)}
  return(convert<int>(plus(a, b)))
}
)";
  const std::string srcPath = writeTemp("compile_native_math_copysign.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_math_copysign_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs native math angle helpers") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  [f32] a{radians(90.0f32)}
  [f32] b{degrees(1.0f32)}
  return(plus(convert<int>(a), convert<int>(b)))
}
)";
  const std::string srcPath = writeTemp("compile_native_math_angles.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_math_angles_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 58);
}

TEST_CASE("compiles and runs native math trig helpers") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  [f32] a{sin(0.0f32)}
  [f32] b{cos(0.0f32)}
  [f32] c{tan(0.0f32)}
  [f32] d{atan2(1.0f32, 0.0f32)}
  [f32] sum{plus(plus(a, b), c)}
  return(plus(convert<int>(sum), convert<int>(d)))
}
)";
  const std::string srcPath = writeTemp("compile_native_math_trig.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_math_trig_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("compiles and runs native math arc trig helpers") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  [f32] a{asin(0.0f32)}
  [f32] b{acos(0.0f32)}
  [f32] c{atan(1.0f32)}
  return(plus(plus(convert<int>(a), convert<int>(b)), convert<int>(c)))
}
)";
  const std::string srcPath = writeTemp("compile_native_math_arc_trig.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_math_arc_trig_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs native math exp/log") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  [f32] a{exp(0.0f32)}
  [f32] b{exp2(1.0f32)}
  [f32] c{log(1.0f32)}
  [f32] d{log2(2.0f32)}
  [f32] e{log10(1.0f32)}
  [f32] sum{plus(plus(a, b), plus(c, plus(d, e)))}
  return(convert<int>(plus(sum, 0.5f32)))
}
)";
  const std::string srcPath = writeTemp("compile_native_math_exp_log.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_math_exp_log_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 4);
}

TEST_CASE("compiles and runs native math hyperbolic") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  [f32] a{sinh(0.0f32)}
  [f32] b{cosh(0.0f32)}
  [f32] c{tanh(0.0f32)}
  [f32] d{asinh(0.0f32)}
  [f32] e{acosh(1.0f32)}
  [f32] f{atanh(0.0f32)}
  [f32] sum{plus(plus(a, b), plus(c, plus(d, plus(e, f))))}
  return(convert<int>(sum))
}
)";
  const std::string srcPath = writeTemp("compile_native_math_hyperbolic.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_math_hyperbolic_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs native explicit math imports") {
  const std::string source = R"(
import /math/min /math/pi
[return<int>]
main() {
  return(plus(convert<int>(pi), min(7i32, 3i32)))
}
)";
  const std::string srcPath = writeTemp("compile_native_math_explicit_imports.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_math_explicit_imports_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 6);
}

TEST_CASE("compiles and runs native float pow") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  return(convert<int>(plus(pow(2.0f32, 3.0f32), 0.5f32)))
}
)";
  const std::string srcPath = writeTemp("compile_native_math_pow_float.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_math_pow_float_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 8);
}

TEST_CASE("compiles and runs native i64 arithmetic") {
  const std::string source = R"(
[return<bool>]
main() {
  return(equal(plus(4000000000i64, 2i64), 4000000002i64))
}
)";
  const std::string srcPath = writeTemp("compile_native_i64.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_i64_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs native u64 division") {
  const std::string source = R"(
[return<bool>]
main() {
  return(equal(divide(10u64, 2u64), 5u64))
}
)";
  const std::string srcPath = writeTemp("compile_native_u64.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_u64_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs native u64 comparison") {
  const std::string source = R"(
[return<bool>]
main() {
  return(greater_than(0xFFFFFFFFFFFFFFFFu64, 1u64))
}
)";
  const std::string srcPath = writeTemp("compile_native_u64_compare.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_u64_compare_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs native bool return") {
  const std::string source = R"(
[return<bool>]
main() {
  return(true)
}
)";
  const std::string srcPath = writeTemp("compile_native_bool.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_bool_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs native bool comparison with i32") {
  const std::string source = R"(
[return<bool>]
main() {
  return(equal(true, 1i32))
}
)";
  const std::string srcPath = writeTemp("compile_native_bool_compare.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_bool_compare_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs native implicit void main") {
  const std::string source = R"(
main() {
  [i32] value{1i32}
}
)";
  const std::string srcPath = writeTemp("compile_native_void_implicit.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_void_implicit_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("compiles and runs native boolean ops") {
  const std::string source = R"(
[return<bool>]
main() {
  return(or(and(true, false), not(false)))
}
)";
  const std::string srcPath = writeTemp("compile_native_bool_ops.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_bool_ops_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs native numeric boolean ops") {
  const std::string source = R"(
[return<bool>]
main() {
  return(or(and(convert<bool>(0i32), convert<bool>(5i32)), not(convert<bool>(0i32))))
}
)";
  const std::string srcPath = writeTemp("compile_native_bool_numeric.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_bool_numeric_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs native short-circuit and") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{1i32}
  [bool mut] witness{false}
  and(equal(value, 0i32), assign(witness, true))
  return(convert<int>(witness))
}
)";
  const std::string srcPath = writeTemp("compile_native_and_short.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_and_short_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("compiles and runs native short-circuit or") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{1i32}
  [bool mut] witness{false}
  or(equal(value, 1i32), assign(witness, true))
  return(convert<int>(witness))
}
)";
  const std::string srcPath = writeTemp("compile_native_or_short.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_or_short_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("compiles and runs native convert") {
  const std::string source = R"(
[return<int>]
main() {
  return(convert<int>(1i32))
}
)";
  const std::string srcPath = writeTemp("compile_native_convert.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_convert_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs native convert bool") {
  const std::string source = R"(
[return<bool>]
main() {
  return(convert<bool>(0i32))
}
)";
  const std::string srcPath = writeTemp("compile_native_convert_bool.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_convert_bool_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("compiles and runs native convert bool from integers") {
  const std::string source = R"(
[return<int>]
main() {
  return(plus(
    convert<int>(convert<bool>(0i32)),
    plus(convert<int>(convert<bool>(-1i32)), convert<int>(convert<bool>(5u64)))
  ))
}
)";
  const std::string srcPath = writeTemp("compile_native_convert_bool_ints.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_convert_bool_ints_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("compiles and runs native convert i64") {
  const std::string source = R"(
[return<bool>]
main() {
  return(equal(convert<i64>(9i64), 9i64))
}
)";
  const std::string srcPath = writeTemp("compile_native_convert_i64.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_convert_i64_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs native convert u64") {
  const std::string source = R"(
[return<bool>]
main() {
  return(equal(convert<u64>(10u64), 10u64))
}
)";
  const std::string srcPath = writeTemp("compile_native_convert_u64.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_convert_u64_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs native integer width convert") {
  const std::string source = R"(
[return<bool>]
main() {
  return(equal(convert<u64>(-1i32), 18446744073709551615u64))
}
)";
  const std::string srcPath = writeTemp("compile_native_convert_widths.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_convert_widths_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs native float literals") {
  const std::string source = R"(
[return<int>]
main() {
  return(convert<int>(1.5f))
}
)";
  const std::string srcPath = writeTemp("compile_native_float_literal.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_float_literal_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs native float bindings") {
  const std::string source = R"(
[return<int>]
main() {
  [float] value{1.5f}
  [float] other{2.0f32}
  return(convert<int>(plus(value, other)))
}
)";
  const std::string srcPath = writeTemp("compile_native_float_binding.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_float_binding_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("rejects native non-literal string bindings") {
  const std::string source = R"(
[return<int>]
main() {
  [string] message{1i32}
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_string_binding.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_string_binding_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_SUITE_END();
#endif
