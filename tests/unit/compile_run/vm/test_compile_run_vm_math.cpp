#include "../test_compile_run_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.vm.math");

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

TEST_CASE("rejects vm support-matrix math nominal helpers") {
  const std::string source = R"(
import /std/math/*

[return<int>]
main() {
  [Mat2] m2{Mat2{1.0f32, 2.0f32, 3.0f32, 4.0f32}}
  [Quat] raw{Quat{0.0f32, 0.0f32, 0.0f32, 2.0f32}}
  [Quat] normalized{raw.toNormalized()}
  [Mat3] basis3{quat_to_mat3(raw)}
  [Mat4] basis4{quat_to_mat4(raw)}
  [Quat] restored{mat3_to_quat(Mat3{
    1.0f32, 0.0f32, 0.0f32,
    0.0f32, -1.0f32, 0.0f32,
    0.0f32, 0.0f32, -1.0f32
  })}
  [f32] total{m2.m00 + m2.m11 + normalized.w + basis3.m00 + basis4.m33 + restored.x + restored.w}
  return(convert<int>(total))
}
)";
  const std::string srcPath = writeTemp("vm_math_support_matrix_nominal_helpers.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 9);
}

TEST_CASE("runs vm quaternion multiply and rotation helpers") {
  const std::string source = R"(
import /std/math/*

[return<int>]
main() {
  [Quat] turnX{Quat{1.0f32, 0.0f32, 0.0f32, 0.0f32}}
  [Quat] turnY{Quat{0.0f32, 1.0f32, 0.0f32, 0.0f32}}
  [Quat] product{multiply(turnX, turnY)}
  [Vec3] input{Vec3{1.0f32, 2.0f32, 3.0f32}}
  [Vec3] rotated{multiply(product, input)}
  [f32] total{product.z - product.x - product.y - product.w + rotated.z - rotated.x - rotated.y}
  return(convert<int>(total))
}
)";
  const std::string srcPath = writeTemp("vm_math_quaternion_reference_multiply_rotation.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 7);
}

TEST_CASE("runs vm matrix composition order helpers") {
  const std::string source = R"(
import /std/math/*

[return<int>]
main() {
  [Mat3] rotate{Mat3{
    0.0f32, -1.0f32, 0.0f32,
    1.0f32, 0.0f32, 0.0f32,
    0.0f32, 0.0f32, 1.0f32
  }}
  [Mat3] scale{Mat3{
    2.0f32, 0.0f32, 0.0f32,
    0.0f32, 3.0f32, 0.0f32,
    0.0f32, 0.0f32, 1.0f32
  }}
  [Vec3] input{Vec3{1.0f32, 2.0f32, 4.0f32}}
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

TEST_CASE("runs vm matrix arithmetic helpers with tolerance") {
  // Single-focus support-matrix coverage is intentionally broad and currently
  // takes longer than 5s in release mode because it exercises many matrix ops.
  const std::string source = R"(
import /std/math/*

[return<int>]
main() {
  [Mat2] base2{Mat2{
    1.0f32, 2.0f32,
    3.0f32, 4.0f32
  }}
  [Mat2] delta2{Mat2{
    0.5f32, -1.0f32,
    1.5f32, 2.0f32
  }}
  [Mat2] sum2{plus(base2, delta2)}
  [Mat2] div2{divide(sum2, 2.0f32)}
  [Mat3] base3{Mat3{
    1.0f32, 2.0f32, 3.0f32,
    4.0f32, 5.0f32, 6.0f32,
    7.0f32, 8.0f32, 9.0f32
  }}
  [Mat3] delta3{Mat3{
    0.5f32, 1.0f32, 1.5f32,
    2.0f32, 2.5f32, 3.0f32,
    3.5f32, 4.0f32, 4.5f32
  }}
  [Mat3] diff3{minus(base3, delta3)}
  [Mat3] scaledLeft3{multiply(2i32, base3)}
  [Mat4] base4{Mat4{
    1.0f32, 2.0f32, 3.0f32, 4.0f32,
    5.0f32, 6.0f32, 7.0f32, 8.0f32,
    9.0f32, 10.0f32, 11.0f32, 12.0f32,
    13.0f32, 14.0f32, 15.0f32, 16.0f32
  }}
  [Mat4] scaledRight4{multiply(base4, 0.5f32)}
  [Mat4] doubled4{multiply(base4, 2.0f32)}
  [Mat4] restored4{divide(doubled4, 2i32)}
  [f32] tolerance{0.0001f32}
  [f32] sum2Error{abs(sum2.m00 - 1.5f32) + abs(sum2.m01 - 1.0f32) + abs(sum2.m10 - 4.5f32) + abs(sum2.m11 - 6.0f32)}
  [f32] div2Error{abs(div2.m00 - 0.75f32) + abs(div2.m11 - 3.0f32)}
  [f32] diff3Error{abs(diff3.m00 - 0.5f32) + abs(diff3.m12 - 3.0f32) + abs(diff3.m22 - 4.5f32)}
  [f32] scaledLeft3Error{abs(scaledLeft3.m00 - 2.0f32) + abs(scaledLeft3.m12 - 12.0f32) + abs(scaledLeft3.m22 - 18.0f32)}
  [f32] scaledRight4Error{abs(scaledRight4.m00 - 0.5f32) + abs(scaledRight4.m13 - 4.0f32) + abs(scaledRight4.m31 - 7.0f32)}
  [f32] restored4Error{abs(restored4.m00 - 1.0f32) + abs(restored4.m12 - 7.0f32) + abs(restored4.m30 - 13.0f32) + abs(restored4.m33 - 16.0f32)}
  [f32] totalError{sum2Error + div2Error + diff3Error + scaledLeft3Error + scaledRight4Error + restored4Error}
  return(convert<int>(totalError <= tolerance))
}
)";
  const std::string srcPath = writeTemp("vm_math_matrix_arithmetic_helpers.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 1);
}

TEST_CASE("runs vm quaternion arithmetic helpers with tolerance") {
  // Single-focus support-matrix coverage is intentionally broad and currently
  // takes longer than 5s in release mode because it exercises many quat ops.
  const std::string source = R"(
import /std/math/*

[return<int>]
main() {
  [Quat] base{Quat{1.0f32, 2.0f32, 3.0f32, 4.0f32}}
  [Quat] delta{Quat{0.5f32, -1.0f32, 1.5f32, 2.0f32}}
  [Quat] sum{plus(base, delta)}
  [Quat] diff{minus(base, delta)}
  [Quat] scaledLeft{multiply(2i32, base)}
  [Quat] scaledRight{multiply(base, 0.5f32)}
  [Quat] divided{divide(sum, 2i32)}
  [f32] tolerance{0.0001f32}
  [f32] sumError{abs(sum.x - 1.5f32) + abs(sum.y - 1.0f32) + abs(sum.z - 4.5f32) + abs(sum.w - 6.0f32)}
  [f32] diffError{abs(diff.x - 0.5f32) + abs(diff.y - 3.0f32) + abs(diff.z - 1.5f32) + abs(diff.w - 2.0f32)}
  [f32] scaledLeftError{abs(scaledLeft.x - 2.0f32) + abs(scaledLeft.y - 4.0f32) + abs(scaledLeft.z - 6.0f32) + abs(scaledLeft.w - 8.0f32)}
  [f32] scaledRightError{abs(scaledRight.x - 0.5f32) + abs(scaledRight.y - 1.0f32) + abs(scaledRight.z - 1.5f32) + abs(scaledRight.w - 2.0f32)}
  [f32] dividedError{abs(divided.x - 0.75f32) + abs(divided.y - 0.5f32) + abs(divided.z - 2.25f32) + abs(divided.w - 3.0f32)}
  [f32] totalError{sumError + diffError + scaledLeftError + scaledRightError + dividedError}
  return(convert<int>(totalError <= tolerance))
}
)";
  const std::string srcPath = writeTemp("vm_math_quaternion_arithmetic_helpers.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 1);
}

TEST_CASE("rejects vm support-matrix plus mismatch diagnostic") {
  const std::string source = R"(
import /std/math/*

[return<int>]
main() {
  [Mat2] lhs{Mat2{1.0f32, 2.0f32, 3.0f32, 4.0f32}}
  [Mat3] rhs{Mat3{
    1.0f32, 0.0f32, 0.0f32,
    0.0f32, 1.0f32, 0.0f32,
    0.0f32, 0.0f32, 1.0f32
  }}
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
  [Mat3] basis{Mat3{
    1.0f32, 0.0f32, 0.0f32,
    0.0f32, 1.0f32, 0.0f32,
    0.0f32, 0.0f32, 1.0f32
  }}
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

TEST_CASE("runs vm Vec2 plus minus scale and divide exactly") {
  const std::string source = R"(
import /std/math/*

[return<int>]
main() {
  [Vec2] base{Vec2{1.0f32, 2.0f32}}
  [Vec2] other{Vec2{8.0f32, 6.0f32}}
  [Vec2] sum{plus(base, other)}
  [Vec2] diff{minus(base, other)}
  [Vec2] left{multiply(2i32, base)}
  [Vec2] right{multiply(other, 0.5f32)}
  [Vec2] div{divide(other, 4.0f32)}
  [f32] totalError{
    abs(sum.x - 9.0f32) +
    abs(sum.y - 8.0f32) +
    abs(diff.x - -7.0f32) +
    abs(diff.y - -4.0f32) +
    abs(left.x - 2.0f32) +
    abs(left.y - 4.0f32) +
    abs(right.x - 4.0f32) +
    abs(right.y - 3.0f32) +
    abs(div.x - 2.0f32) +
    abs(div.y - 1.5f32)
  }
  return(convert<int>(totalError == 0.0f32))
}
)";
  const std::string srcPath = writeTemp("vm_math_vec2_arithmetic.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 1);
}

TEST_CASE("runs vm Vec3 plus minus scale and divide exactly") {
  const std::string source = R"(
import /std/math/*

[return<int>]
main() {
  [Vec3] base{Vec3{1.0f32, 2.0f32, 3.0f32}}
  [Vec3] other{Vec3{8.0f32, 6.0f32, 4.0f32}}
  [Vec3] sum{plus(base, other)}
  [Vec3] diff{minus(base, other)}
  [Vec3] left{multiply(2i32, base)}
  [Vec3] right{multiply(other, 0.5f32)}
  [Vec3] div{divide(other, 4.0f32)}
  [f32] totalError{
    abs(sum.x - 9.0f32) +
    abs(sum.y - 8.0f32) +
    abs(sum.z - 7.0f32) +
    abs(diff.x - -7.0f32) +
    abs(diff.y - -4.0f32) +
    abs(diff.z - -1.0f32) +
    abs(left.x - 2.0f32) +
    abs(left.y - 4.0f32) +
    abs(left.z - 6.0f32) +
    abs(right.x - 4.0f32) +
    abs(right.y - 3.0f32) +
    abs(right.z - 2.0f32) +
    abs(div.x - 2.0f32) +
    abs(div.y - 1.5f32) +
    abs(div.z - 1.0f32)
  }
  return(convert<int>(totalError == 0.0f32))
}
)";
  const std::string srcPath = writeTemp("vm_math_vec3_arithmetic.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 1);
}

TEST_CASE("runs vm Vec4 plus minus scale and divide exactly") {
  const std::string source = R"(
import /std/math/*

[return<int>]
main() {
  [Vec4] base{Vec4{1.0f32, 2.0f32, 3.0f32, 4.0f32}}
  [Vec4] other{Vec4{8.0f32, 6.0f32, 4.0f32, 2.0f32}}
  [Vec4] sum{plus(base, other)}
  [Vec4] diff{minus(base, other)}
  [Vec4] left{multiply(2i32, base)}
  [Vec4] right{multiply(other, 0.5f32)}
  [Vec4] div{divide(other, 4.0f32)}
  [f32] totalError{
    abs(sum.x - 9.0f32) +
    abs(sum.y - 8.0f32) +
    abs(sum.z - 7.0f32) +
    abs(sum.w - 6.0f32) +
    abs(diff.x - -7.0f32) +
    abs(diff.y - -4.0f32) +
    abs(diff.z - -1.0f32) +
    abs(diff.w - 2.0f32) +
    abs(left.x - 2.0f32) +
    abs(left.y - 4.0f32) +
    abs(left.z - 6.0f32) +
    abs(left.w - 8.0f32) +
    abs(right.x - 4.0f32) +
    abs(right.y - 3.0f32) +
    abs(right.z - 2.0f32) +
    abs(right.w - 1.0f32) +
    abs(div.x - 2.0f32) +
    abs(div.y - 1.5f32) +
    abs(div.z - 1.0f32) +
    abs(div.w - 0.5f32)
  }
  return(convert<int>(totalError == 0.0f32))
}
)";
  const std::string srcPath = writeTemp("vm_math_vec4_arithmetic.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 1);
}

TEST_CASE("rejects vm vector plus of different vector types") {
  const std::string source = R"(
import /std/math/*

[return<int>]
main() {
  [Vec2] lhs{Vec2{1.0f32, 2.0f32}}
  [Vec3] rhs{Vec3{1.0f32, 2.0f32, 3.0f32}}
  [Vec2] value{plus(lhs, rhs)}
  return(convert<int>(value.x))
}
)";
  const std::string srcPath = writeTemp("vm_math_vector_plus_mismatch.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_vm_math_vector_plus_mismatch.err").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("plus requires matching vector operand types") != std::string::npos);
}

TEST_CASE("rejects vm vector multiply of two vectors") {
  const std::string source = R"(
import /std/math/*

[return<int>]
main() {
  [Vec2] lhs{Vec2{1.0f32, 2.0f32}}
  [Vec2] value{multiply(lhs, lhs)}
  return(convert<int>(value.x))
}
)";
  const std::string srcPath = writeTemp("vm_math_vector_multiply_vectors.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_vm_math_vector_multiply_vectors.err").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("multiply requires a vector and a scalar operand") !=
        std::string::npos);
}

TEST_CASE("rejects a program call of a private vector helper") {
  const std::string source = R"(
import /std/math/*

[return<int>]
main() {
  [Vec2] lhs{Vec2{1.0f32, 2.0f32}}
  [Vec2] value{/std/math/vec2_add_internal(lhs, lhs)}
  return(convert<int>(value.x))
}
)";
  const std::string srcPath = writeTemp("vm_math_vector_private_helper.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_vm_math_vector_private_helper.err").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("/std/math/vec2_add_internal is private") != std::string::npos);
}

TEST_SUITE_END();
