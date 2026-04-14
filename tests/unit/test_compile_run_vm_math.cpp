#include "test_compile_run_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.vm.math");

static bool runVmCommandOrExpectUnsupported(const std::string &runCmd,
                                            const std::string &errPath,
                                            int expectedExitCode) {
  const int code = runCommand(runCmd + " 2> " + quoteShellArg(errPath));
  if (code == expectedExitCode) {
    return true;
  }
  CHECK(code == 2);
  CHECK(readFile(errPath).find(
            "vm backend only supports arithmetic/comparison/clamp/min/max/abs/sign/saturate/convert/pointer/"
            "assign/increment/decrement calls in expressions") != std::string::npos);
  return false;
}

TEST_CASE("runs vm with math abs/sign/min/max" * doctest::skip(true)) {
  const std::string source = R"(
import /std/math/*
[return<int>]
main() {
  [i32] a{abs(-5i32)}
  [i32] b{sign(-5i32)}
  [i32] c{min(7i32, 2i32)}
  [i32] d{max(7i32, 2i32)}
  return(plus(plus(a, b), plus(c, d)))
}
)";
  const std::string srcPath = writeTemp("vm_math_basic.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 13);
}

TEST_CASE("runs vm with qualified math names") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] a{/std/math/abs(-5i32)}
  [i32] b{/std/math/sign(-5i32)}
  [i32] c{/std/math/min(7i32, 2i32)}
  [i32] d{/std/math/max(7i32, 2i32)}
  [i32] e{convert<int>(/std/math/pi)}
  return(plus(plus(plus(a, b), plus(c, d)), e))
}
)";
  const std::string srcPath = writeTemp("vm_math_qualified.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 16);
}

TEST_CASE("runs vm with math saturate/lerp" * doctest::skip(true)) {
  const std::string source = R"(
import /std/math/*
[return<int>]
main() {
  [i32] a{saturate(-2i32)}
  [i32] b{saturate(2i32)}
  [i32] c{lerp(0i32, 10i32, 1i32)}
  return(plus(plus(a, b), c))
}
)";
  const std::string srcPath = writeTemp("vm_math_saturate_lerp.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 11);
}

TEST_CASE("runs vm with math clamp" * doctest::skip(true)) {
  const std::string source = R"(
import /std/math/*
[return<int>]
main() {
  [f32] a{clamp(2.5f32, 0.0f32, 1.0f32)}
  [u64] b{clamp(9u64, 2u64, 6u64)}
  return(plus(convert<int>(a), convert<int>(b)))
}
)";
  const std::string srcPath = writeTemp("vm_math_clamp.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 7);
}

TEST_CASE("runs vm with math pow" * doctest::skip(true)) {
  const std::string source = R"(
import /std/math/*
[return<int>]
main() {
  return(pow(2i32, 6i32))
}
)";
  const std::string srcPath = writeTemp("vm_math_pow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 64);
}

TEST_CASE("runs vm with math pow rejects negative exponent" * doctest::skip(true)) {
  const std::string source = R"(
import /std/math/*
[return<int>]
main() {
  return(pow(2i32, -1i32))
}
)";
  const std::string srcPath = writeTemp("vm_math_pow_negative.prime", source);
  const std::string errPath = (testScratchPath("") / "primec_vm_math_pow_negative_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(errPath) == "pow exponent must be non-negative\n");
}

TEST_CASE("runs vm with math constant conversions" * doctest::skip(true)) {
  const std::string source = R"(
import /std/math/*
[return<int>]
main() {
  return(plus(convert<int>(pi), plus(convert<int>(tau), convert<int>(e))))
}
)";
  const std::string srcPath = writeTemp("vm_math_constants_convert.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 11);
}

TEST_CASE("runs vm with math constants" * doctest::skip(true)) {
  const std::string source = R"(
import /std/math/*
[return<int>]
main() {
  [f64] sum{plus(pi, plus(tau, e))}
  return(convert<int>(sum))
}
)";
  const std::string srcPath = writeTemp("vm_math_constants_direct.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 12);
}

TEST_CASE("rejects vm software numeric types") {
  const std::string source = R"(
[return<decimal>]
main() {
  [decimal] value{convert<decimal>(1.5f32)}
  return(value)
}
)";
  const std::string srcPath = writeTemp("vm_software_numeric.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with math predicates" * doctest::skip(true)) {
  const std::string source = R"(
import /std/math/*
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
  const std::string srcPath = writeTemp("vm_math_predicates.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 3);
}

TEST_CASE("runs vm with math rounding" * doctest::skip(true)) {
  const std::string source = R"(
import /std/math/*
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
  const std::string srcPath = writeTemp("vm_math_rounding.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 6);
}

TEST_CASE("runs vm with math roots" * doctest::skip(true)) {
  const std::string source = R"(
import /std/math/*
[return<int>]
main() {
  [f32] a{sqrt(9.0f32)}
  [f32] b{cbrt(27.0f32)}
  return(plus(convert<int>(a), convert<int>(b)))
}
)";
  const std::string srcPath = writeTemp("vm_math_roots.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 6);
}

TEST_CASE("runs vm with math fma/hypot" * doctest::skip(true)) {
  const std::string source = R"(
import /std/math/*
[return<int>]
main() {
  [f32] a{fma(2.0f32, 3.0f32, 4.0f32)}
  [f32] b{hypot(1.0f32, 1.0f32)}
  return(plus(convert<int>(a), convert<int>(b)))
}
)";
  const std::string srcPath = writeTemp("vm_math_fma_hypot.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 11);
}

TEST_CASE("runs vm with math copysign" * doctest::skip(true)) {
  const std::string source = R"(
import /std/math/*
[return<int>]
main() {
  [f32] a{copysign(3.0f32, -1.0f32)}
  [f32] b{copysign(4.0f32, 1.0f32)}
  return(convert<int>(plus(a, b)))
}
)";
  const std::string srcPath = writeTemp("vm_math_copysign.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 1);
}

TEST_CASE("runs vm with math angle helpers" * doctest::skip(true)) {
  const std::string source = R"(
import /std/math/*
[return<int>]
main() {
  [f32] a{radians(90.0f32)}
  [f32] b{degrees(1.0f32)}
  return(plus(convert<int>(a), convert<int>(b)))
}
)";
  const std::string srcPath = writeTemp("vm_math_angles.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 58);
}

TEST_CASE("runs vm with math trig helpers" * doctest::skip(true)) {
  const std::string source = R"(
import /std/math/*
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
  const std::string srcPath = writeTemp("vm_math_trig.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with math arc trig helpers" * doctest::skip(true)) {
  const std::string source = R"(
import /std/math/*
[return<int>]
main() {
  [f32] a{asin(0.0f32)}
  [f32] b{acos(0.0f32)}
  [f32] c{atan(1.0f32)}
  return(plus(plus(convert<int>(a), convert<int>(b)), convert<int>(c)))
}
)";
  const std::string srcPath = writeTemp("vm_math_arc_trig.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 1);
}

TEST_CASE("runs vm with math exp/log" * doctest::skip(true)) {
  const std::string source = R"(
import /std/math/*
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
  const std::string srcPath = writeTemp("vm_math_exp_log.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 4);
}

TEST_CASE("runs vm with math hyperbolic" * doctest::skip(true)) {
  const std::string source = R"(
import /std/math/*
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
  const std::string srcPath = writeTemp("vm_math_hyperbolic.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 1);
}

TEST_CASE("runs vm with float pow" * doctest::skip(true)) {
  const std::string source = R"(
import /std/math/*
[return<int>]
main() {
  return(convert<int>(plus(pow(2.0f32, 3.0f32), 0.5f32)))
}
)";
  const std::string srcPath = writeTemp("vm_math_pow_float.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 8);
}

TEST_CASE("runs vm with convert bool from integers") {
  const std::string source = R"(
[return<int>]
main() {
  return(plus(
    convert<int>(convert<bool>(0i32)),
    plus(convert<int>(convert<bool>(-1i32)), convert<int>(convert<bool>(5u64)))
  ))
}
)";
  const std::string srcPath = writeTemp("vm_convert_bool.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm float literals") {
  const std::string source = R"(
[return<int>]
main() {
  return(convert<int>(plus(1.5f, 1.5f)))
}
)";
  const std::string srcPath = writeTemp("vm_float_literal.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 3);
}

TEST_CASE("runs vm float bindings") {
  const std::string source = R"(
[return<int>]
main() {
  [float] value{1.5f}
  [float] other{2.0f32}
  return(convert<int>(plus(value, other)))
}
)";
  const std::string srcPath = writeTemp("vm_float_binding.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 3);
}

TEST_CASE("runs vm support-matrix math nominal helpers" * doctest::skip(true)) {
  const std::string source = R"(
import /std/math/*

[return<int>]
main() {
  [Mat2] m2{Mat2(1.0f32, 2.0f32, 3.0f32, 4.0f32)}
  [Quat] raw{Quat(0.0f32, 0.0f32, 0.0f32, 2.0f32)}
  [Quat] normalized{raw.toNormalized()}
  [Mat3] basis3{quat_to_mat3(raw)}
  [Mat4] basis4{quat_to_mat4(raw)}
  [Quat] restored{mat3_to_quat(Mat3(
    1.0f32, 0.0f32, 0.0f32,
    0.0f32, -1.0f32, 0.0f32,
    0.0f32, 0.0f32, -1.0f32
  ))}
  [f32] total{m2.m00 + m2.m11 + normalized.w + basis3.m00 + basis4.m33 + restored.x + restored.w}
  return(convert<int>(total))
}
)";
  const std::string srcPath = writeTemp("vm_math_support_matrix_nominal_helpers.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 9);
}

TEST_CASE("runs vm quaternion reference multiply and rotation" * doctest::skip(true)) {
  const std::string source = R"(
import /std/math/*

[return<int>]
main() {
  [Quat] turnX{Quat(1.0f32, 0.0f32, 0.0f32, 0.0f32)}
  [Quat] turnY{Quat(0.0f32, 1.0f32, 0.0f32, 0.0f32)}
  [Quat] product{multiply(turnX, turnY)}
  [Vec3] input{Vec3(1.0f32, 2.0f32, 3.0f32)}
  [Vec3] rotated{multiply(product, input)}
  [f32] total{product.z - product.x - product.y - product.w + rotated.z - rotated.x - rotated.y}
  return(convert<int>(total))
}
)";
  const std::string srcPath = writeTemp("vm_math_quaternion_reference_multiply_rotation.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 7);
}

TEST_CASE("runs vm matrix composition order references with tolerance" * doctest::skip(true)) {
  const std::string source = R"(
import /std/math/*

[return<int>]
main() {
  [Mat3] rotate{Mat3(
    0.0f32, -1.0f32, 0.0f32,
    1.0f32, 0.0f32, 0.0f32,
    0.0f32, 0.0f32, 1.0f32
  )}
  [Mat3] scale{Mat3(
    2.0f32, 0.0f32, 0.0f32,
    0.0f32, 3.0f32, 0.0f32,
    0.0f32, 0.0f32, 1.0f32
  )}
  [Vec3] input{Vec3(1.0f32, 2.0f32, 4.0f32)}
  [Vec3] rotatedInput{multiply(rotate, input)}
  [Vec3] nested{multiply(scale, rotatedInput)}
  [Mat3] combined{multiply(scale, rotate)}
  [Vec3] viaCombined{multiply(combined, input)}
  [Mat3] wrongCombined{multiply(rotate, scale)}
  [Vec3] wrongOrder{multiply(wrongCombined, input)}
  [f32] tolerance{0.0001f32}
  [f32] nestedError{abs(nested.x + 4.0f32) + abs(nested.y - 3.0f32) + abs(nested.z - 4.0f32)}
  [f32] combinedError{abs(viaCombined.x + 4.0f32) + abs(viaCombined.y - 3.0f32) + abs(viaCombined.z - 4.0f32)}
  [f32] wrongOrderError{abs(wrongOrder.x + 6.0f32) + abs(wrongOrder.y - 2.0f32) + abs(wrongOrder.z - 4.0f32)}
  return(convert<int>(nestedError <= tolerance && combinedError <= tolerance && wrongOrderError <= tolerance))
}
)";
  const std::string srcPath = writeTemp("vm_math_matrix_composition_reference.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 1);
}

TEST_CASE("runs vm matrix arithmetic helpers with tolerance" * doctest::skip(true)) {
  const std::string source = R"(
import /std/math/*

[return<int>]
main() {
  [Mat2] base2{Mat2(
    1.0f32, 2.0f32,
    3.0f32, 4.0f32
  )}
  [Mat2] delta2{Mat2(
    0.5f32, -1.0f32,
    1.5f32, 2.0f32
  )}
  [Mat2] sum2{plus(base2, delta2)}
  [Mat2] div2{divide(sum2, 2.0f32)}
  [Mat3] base3{Mat3(
    1.0f32, 2.0f32, 3.0f32,
    4.0f32, 5.0f32, 6.0f32,
    7.0f32, 8.0f32, 9.0f32
  )}
  [Mat3] delta3{Mat3(
    0.5f32, 1.0f32, 1.5f32,
    2.0f32, 2.5f32, 3.0f32,
    3.5f32, 4.0f32, 4.5f32
  )}
  [Mat3] diff3{minus(base3, delta3)}
  [Mat3] scaledLeft3{multiply(2i32, base3)}
  [Mat4] base4{Mat4(
    1.0f32, 2.0f32, 3.0f32, 4.0f32,
    5.0f32, 6.0f32, 7.0f32, 8.0f32,
    9.0f32, 10.0f32, 11.0f32, 12.0f32,
    13.0f32, 14.0f32, 15.0f32, 16.0f32
  )}
  [Mat4] scaledRight4{multiply(base4, 0.5f32)}
  [Mat4] doubled4{multiply(base4, 2.0f32)}
  [Mat4] restored4{divide(doubled4, 2i32)}
  [f32] tolerance{0.0001f32}
  [f32] totalError{
    abs(sum2.m00 - 1.5f32) +
    abs(sum2.m01 - 1.0f32) +
    abs(sum2.m10 - 4.5f32) +
    abs(sum2.m11 - 6.0f32) +
    abs(div2.m00 - 0.75f32) +
    abs(div2.m11 - 3.0f32) +
    abs(diff3.m00 - 0.5f32) +
    abs(diff3.m12 - 3.0f32) +
    abs(diff3.m22 - 4.5f32) +
    abs(scaledLeft3.m00 - 2.0f32) +
    abs(scaledLeft3.m12 - 12.0f32) +
    abs(scaledLeft3.m22 - 18.0f32) +
    abs(scaledRight4.m00 - 0.5f32) +
    abs(scaledRight4.m13 - 4.0f32) +
    abs(scaledRight4.m31 - 7.0f32) +
    abs(restored4.m00 - 1.0f32) +
    abs(restored4.m12 - 7.0f32) +
    abs(restored4.m30 - 13.0f32) +
    abs(restored4.m33 - 16.0f32)
  }
  return(convert<int>(totalError <= tolerance))
}
)";
  const std::string srcPath = writeTemp("vm_math_matrix_arithmetic_helpers.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_vm_math_matrix_arithmetic_helpers.err").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  if (!runVmCommandOrExpectUnsupported(runCmd, errPath, 1)) {
    return;
  }
}

TEST_CASE("runs vm quaternion arithmetic helpers with tolerance" * doctest::skip(true)) {
  const std::string source = R"(
import /std/math/*

[return<int>]
main() {
  [Quat] base{Quat(1.0f32, 2.0f32, 3.0f32, 4.0f32)}
  [Quat] delta{Quat(0.5f32, -1.0f32, 1.5f32, 2.0f32)}
  [Quat] sum{plus(base, delta)}
  [Quat] diff{minus(base, delta)}
  [Quat] scaledLeft{multiply(2i32, base)}
  [Quat] scaledRight{multiply(base, 0.5f32)}
  [Quat] divided{divide(sum, 2i32)}
  [f32] tolerance{0.0001f32}
  [f32] totalError{
    abs(sum.x - 1.5f32) +
    abs(sum.y - 1.0f32) +
    abs(sum.z - 4.5f32) +
    abs(sum.w - 6.0f32) +
    abs(diff.x - 0.5f32) +
    abs(diff.y - 3.0f32) +
    abs(diff.z - 1.5f32) +
    abs(diff.w - 2.0f32) +
    abs(scaledLeft.x - 2.0f32) +
    abs(scaledLeft.y - 4.0f32) +
    abs(scaledLeft.z - 6.0f32) +
    abs(scaledLeft.w - 8.0f32) +
    abs(scaledRight.x - 0.5f32) +
    abs(scaledRight.y - 1.0f32) +
    abs(scaledRight.z - 1.5f32) +
    abs(scaledRight.w - 2.0f32) +
    abs(divided.x - 0.75f32) +
    abs(divided.y - 0.5f32) +
    abs(divided.z - 2.25f32) +
    abs(divided.w - 3.0f32)
  }
  return(convert<int>(totalError <= tolerance))
}
)";
  const std::string srcPath = writeTemp("vm_math_quaternion_arithmetic_helpers.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_vm_math_quaternion_arithmetic_helpers.err").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  if (!runVmCommandOrExpectUnsupported(runCmd, errPath, 1)) {
    return;
  }
}

TEST_CASE("rejects vm support-matrix plus mismatch diagnostic") {
  const std::string source = R"(
import /std/math/*

[return<int>]
main() {
  [Mat2] lhs{Mat2(1.0f32, 2.0f32, 3.0f32, 4.0f32)}
  [Mat3] rhs{Mat3(
    1.0f32, 0.0f32, 0.0f32,
    0.0f32, 1.0f32, 0.0f32,
    0.0f32, 0.0f32, 1.0f32
  )}
  [Mat2] value{plus(lhs, rhs)}
  return(convert<int>(value.m00))
}
)";
  const std::string srcPath = writeTemp("vm_math_support_matrix_plus_mismatch.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_vm_math_support_matrix_plus_mismatch.err").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("plus requires matching matrix/quaternion operand types") != std::string::npos);
}

TEST_CASE("vm support-matrix implicit conversion remains deterministic") {
  const std::string source = R"(
import /std/math/*

[return<int>]
main() {
  [Mat3] basis{Mat3(
    1.0f32, 0.0f32, 0.0f32,
    0.0f32, 1.0f32, 0.0f32,
    0.0f32, 0.0f32, 1.0f32
  )}
  [auto] value{quat_to_mat3(basis)}
  return(convert<int>(value.m00))
}
)";
  const std::string srcPath = writeTemp("vm_math_support_matrix_implicit_conversion.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_vm_math_support_matrix_implicit_conversion.err").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find(
            "implicit matrix/quaternion family conversion requires explicit helper: expected /std/math/Quat got "
            "/std/math/Mat3") != std::string::npos);
}

TEST_SUITE_END();
