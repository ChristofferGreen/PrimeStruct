#include "third_party/doctest.h"

#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.operators");

TEST_CASE("arithmetic rejects bool operands") {
  const std::string source = R"(
[return<int>]
main() {
  return(plus(true, 1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("arithmetic operators require numeric operands") != std::string::npos);
}

TEST_CASE("arithmetic rejects string operands") {
  const std::string source = R"(
[return<int>]
main() {
  return(multiply("nope"utf8, 1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("arithmetic operators require numeric operands") != std::string::npos);
}

TEST_CASE("arithmetic rejects struct operands") {
  const std::string source = R"(
thing() {
  [i32] value{1i32}
}

[return<int>]
main() {
  [thing] item{1i32}
  return(plus(item, item))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("arithmetic operators require numeric operands") != std::string::npos);
}

TEST_CASE("arithmetic plus accepts matching matrix operands") {
  const std::string source = R"(
import /std/math/*
[return<Mat2>]
main() {
  [Mat2] a{Mat2{1.0f32, 2.0f32, 3.0f32, 4.0f32}}
  [Mat2] b{Mat2{5.0f32, 6.0f32, 7.0f32, 8.0f32}}
  return(plus(plus(a, b), a))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("arithmetic plus accepts matching quaternion operands") {
  const std::string source = R"(
import /std/math/*
[return<Quat>]
main() {
  [Quat] a{Quat{0.0f32, 1.0f32, 0.0f32, 0.0f32}}
  [Quat] b{Quat{1.0f32, 0.0f32, 0.0f32, 0.0f32}}
  [Quat] sum{plus(a, b)}
  return(sum)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("arithmetic plus rejects mismatched matrix shapes") {
  const std::string source = R"(
import /std/math/*
[return<int>]
main() {
  [Mat2] lhs{Mat2{1.0f32, 2.0f32, 3.0f32, 4.0f32}}
  [Mat3] rhs{Mat3{
    5.0f32, 6.0f32, 7.0f32,
    8.0f32, 9.0f32, 10.0f32,
    11.0f32, 12.0f32, 13.0f32
  }}
  [Mat2] sum{plus(lhs, rhs)}
  return(convert<int>(sum.m00))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("plus requires matching matrix/quaternion operand types") != std::string::npos);
}

TEST_CASE("arithmetic plus rejects mixed matrix quaternion operands") {
  const std::string source = R"(
import /std/math/*
[return<int>]
main() {
  [Mat2] lhs{Mat2{1.0f32, 2.0f32, 3.0f32, 4.0f32}}
  [Quat] rhs{Quat{0.0f32, 0.0f32, 0.0f32, 1.0f32}}
  [Mat2] sum{plus(lhs, rhs)}
  return(convert<int>(sum.m00))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("plus requires matching matrix/quaternion operand types") != std::string::npos);
}

TEST_CASE("arithmetic minus accepts matching matrix operands") {
  const std::string source = R"(
import /std/math/*
[return<Mat2>]
main() {
  [Mat2] a{Mat2{1.0f32, 2.0f32, 3.0f32, 4.0f32}}
  [Mat2] b{Mat2{5.0f32, 6.0f32, 7.0f32, 8.0f32}}
  return(minus(minus(a, b), a))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("arithmetic minus accepts matching quaternion operands") {
  const std::string source = R"(
import /std/math/*
[return<Quat>]
main() {
  [Quat] a{Quat{0.0f32, 1.0f32, 0.0f32, 0.0f32}}
  [Quat] b{Quat{1.0f32, 0.0f32, 0.0f32, 0.0f32}}
  [Quat] diff{minus(a, b)}
  return(diff)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("arithmetic minus rejects mismatched matrix shapes") {
  const std::string source = R"(
import /std/math/*
[return<int>]
main() {
  [Mat2] lhs{Mat2{1.0f32, 2.0f32, 3.0f32, 4.0f32}}
  [Mat3] rhs{Mat3{
    5.0f32, 6.0f32, 7.0f32,
    8.0f32, 9.0f32, 10.0f32,
    11.0f32, 12.0f32, 13.0f32
  }}
  [Mat2] diff{minus(lhs, rhs)}
  return(convert<int>(diff.m00))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("minus requires matching matrix/quaternion operand types") != std::string::npos);
}

TEST_CASE("arithmetic minus rejects mixed matrix quaternion operands") {
  const std::string source = R"(
import /std/math/*
[return<int>]
main() {
  [Mat2] lhs{Mat2{1.0f32, 2.0f32, 3.0f32, 4.0f32}}
  [Quat] rhs{Quat{0.0f32, 0.0f32, 0.0f32, 1.0f32}}
  [Mat2] diff{minus(lhs, rhs)}
  return(convert<int>(diff.m00))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("minus requires matching matrix/quaternion operand types") != std::string::npos);
}

TEST_CASE("arithmetic multiply accepts matrix scalar scaling") {
  const std::string source = R"(
import /std/math/*
[return<Mat2>]
main() {
  [Mat2] value{Mat2{1.0f32, 2.0f32, 3.0f32, 4.0f32}}
  return(multiply(value, 2i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("arithmetic multiply accepts scalar quaternion scaling") {
  const std::string source = R"(
import /std/math/*
[return<Quat>]
main() {
  [Quat] value{Quat{0.0f32, 1.0f32, 0.0f32, 0.0f32}}
  return(multiply(2.0f32, value))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("arithmetic multiply accepts matrix vector products") {
  const std::string source = R"(
import /std/math/*
[return<Vec3>]
main() {
  [Mat3] basis{Mat3{
    1.0f32, 0.0f32, 0.0f32,
    0.0f32, 1.0f32, 0.0f32,
    0.0f32, 0.0f32, 1.0f32
  }}
  [Vec3] value{Vec3{1.0f32, 2.0f32, 3.0f32}}
  return(multiply(basis, value))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("arithmetic multiply accepts matrix matrix products") {
  const std::string source = R"(
import /std/math/*
[return<Mat4>]
main() {
  [Mat4] left{Mat4{
    1.0f32, 0.0f32, 0.0f32, 0.0f32,
    0.0f32, 1.0f32, 0.0f32, 0.0f32,
    0.0f32, 0.0f32, 1.0f32, 0.0f32,
    0.0f32, 0.0f32, 0.0f32, 1.0f32
  }}
  [Mat4] right{left}
  return(multiply(left, right))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("arithmetic multiply accepts quaternion quaternion products") {
  const std::string source = R"(
import /std/math/*
[return<Quat>]
main() {
  [Quat] left{Quat{0.0f32, 0.0f32, 0.0f32, 1.0f32}}
  [Quat] right{Quat{0.0f32, 1.0f32, 0.0f32, 0.0f32}}
  return(multiply(left, right))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("arithmetic multiply accepts quaternion vector rotation shape") {
  const std::string source = R"(
import /std/math/*
[return<Vec3>]
main() {
  [Quat] rotation{Quat{0.0f32, 0.0f32, 0.0f32, 1.0f32}}
  [Vec3] value{Vec3{1.0f32, 0.0f32, 0.0f32}}
  return(multiply(rotation, value))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("arithmetic multiply rejects mismatched matrix shapes") {
  const std::string source = R"(
import /std/math/*
[return<int>]
main() {
  [Mat2] left{Mat2{1.0f32, 2.0f32, 3.0f32, 4.0f32}}
  [Mat3] right{Mat3{
    1.0f32, 0.0f32, 0.0f32,
    0.0f32, 1.0f32, 0.0f32,
    0.0f32, 0.0f32, 1.0f32
  }}
  [Mat2] value{multiply(left, right)}
  return(convert<int>(value.m00))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("multiply requires scalar scaling, matrix-vector, matrix-matrix, quaternion-quaternion, or quaternion-Vec3 operands") != std::string::npos);
}

TEST_CASE("arithmetic multiply rejects vector matrix ordering") {
  const std::string source = R"(
import /std/math/*
[return<int>]
main() {
  [Vec3] left{Vec3{1.0f32, 2.0f32, 3.0f32}}
  [Mat3] right{Mat3{
    1.0f32, 0.0f32, 0.0f32,
    0.0f32, 1.0f32, 0.0f32,
    0.0f32, 0.0f32, 1.0f32
  }}
  [Vec3] value{multiply(left, right)}
  return(convert<int>(value.x))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("multiply requires scalar scaling, matrix-vector, matrix-matrix, quaternion-quaternion, or quaternion-Vec3 operands") != std::string::npos);
}

TEST_CASE("arithmetic multiply rejects quaternion vec4 operands") {
  const std::string source = R"(
import /std/math/*
[return<int>]
main() {
  [Quat] left{Quat{0.0f32, 0.0f32, 0.0f32, 1.0f32}}
  [Vec4] right{Vec4{1.0f32, 0.0f32, 0.0f32, 1.0f32}}
  [Vec4] value{multiply(left, right)}
  return(convert<int>(value.x))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("multiply requires scalar scaling, matrix-vector, matrix-matrix, quaternion-quaternion, or quaternion-Vec3 operands") != std::string::npos);
}

TEST_CASE("arithmetic divide accepts matrix scalar division") {
  const std::string source = R"(
import /std/math/*
[return<Mat2>]
main() {
  [Mat2] value{Mat2{1.0f32, 2.0f32, 3.0f32, 4.0f32}}
  return(divide(value, 2.0f32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("arithmetic divide accepts quaternion scalar division") {
  const std::string source = R"(
import /std/math/*
[return<Quat>]
main() {
  [Quat] value{Quat{0.0f32, 2.0f32, 0.0f32, 0.0f32}}
  return(divide(value, 2i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("arithmetic divide rejects matrix matrix operands") {
  const std::string source = R"(
import /std/math/*
[return<int>]
main() {
  [Mat2] left{Mat2{1.0f32, 2.0f32, 3.0f32, 4.0f32}}
  [Mat2] right{left}
  [Mat2] value{divide(left, right)}
  return(convert<int>(value.m00))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("divide requires matrix/quaternion composite-by-scalar operands") != std::string::npos);
}

TEST_CASE("arithmetic divide rejects scalar quaternion operands") {
  const std::string source = R"(
import /std/math/*
[return<int>]
main() {
  [Quat] value{Quat{0.0f32, 0.0f32, 0.0f32, 1.0f32}}
  [Quat] scaled{divide(2.0f32, value)}
  return(convert<int>(scaled.w))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("divide requires matrix/quaternion composite-by-scalar operands") != std::string::npos);
}

TEST_CASE("arithmetic negate rejects bool operands") {
  const std::string source = R"(
[return<int>]
main() {
  return(negate(true))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("arithmetic operators require numeric operands") != std::string::npos);
}

TEST_CASE("arithmetic negate rejects unsigned operands") {
  const std::string source = R"(
[return<u64>]
main() {
  return(negate(2u64))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("negate does not support unsigned operands") != std::string::npos);
}

TEST_CASE("arithmetic rejects mixed signed/unsigned operands") {
  const std::string source = R"(
[return<i64>]
main() {
  return(plus(1i64, 2u64))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("mixed signed/unsigned") != std::string::npos);
}

TEST_CASE("arithmetic multiply rejects mixed signed/unsigned operands") {
  const std::string source = R"(
[return<i64>]
main() {
  return(multiply(3i64, 4u64))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("mixed signed/unsigned") != std::string::npos);
}

TEST_CASE("arithmetic rejects mixed int/float operands") {
  const std::string source = R"(
[return<int>]
main() {
  return(plus(1i32, 1.5f))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("mixed int/float") != std::string::npos);
}

TEST_SUITE_END();
