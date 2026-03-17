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
  [Mat2] a{Mat2(1.0f32, 2.0f32, 3.0f32, 4.0f32)}
  [Mat2] b{Mat2(5.0f32, 6.0f32, 7.0f32, 8.0f32)}
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
  [Quat] a{Quat(0.0f32, 1.0f32, 0.0f32, 0.0f32)}
  [Quat] b{Quat(1.0f32, 0.0f32, 0.0f32, 0.0f32)}
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
  [Mat2] lhs{Mat2(1.0f32, 2.0f32, 3.0f32, 4.0f32)}
  [Mat3] rhs{Mat3(
    5.0f32, 6.0f32, 7.0f32,
    8.0f32, 9.0f32, 10.0f32,
    11.0f32, 12.0f32, 13.0f32
  )}
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
  [Mat2] lhs{Mat2(1.0f32, 2.0f32, 3.0f32, 4.0f32)}
  [Quat] rhs{Quat(0.0f32, 0.0f32, 0.0f32, 1.0f32)}
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
  [Mat2] a{Mat2(1.0f32, 2.0f32, 3.0f32, 4.0f32)}
  [Mat2] b{Mat2(5.0f32, 6.0f32, 7.0f32, 8.0f32)}
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
  [Quat] a{Quat(0.0f32, 1.0f32, 0.0f32, 0.0f32)}
  [Quat] b{Quat(1.0f32, 0.0f32, 0.0f32, 0.0f32)}
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
  [Mat2] lhs{Mat2(1.0f32, 2.0f32, 3.0f32, 4.0f32)}
  [Mat3] rhs{Mat3(
    5.0f32, 6.0f32, 7.0f32,
    8.0f32, 9.0f32, 10.0f32,
    11.0f32, 12.0f32, 13.0f32
  )}
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
  [Mat2] lhs{Mat2(1.0f32, 2.0f32, 3.0f32, 4.0f32)}
  [Quat] rhs{Quat(0.0f32, 0.0f32, 0.0f32, 1.0f32)}
  [Mat2] diff{minus(lhs, rhs)}
  return(convert<int>(diff.m00))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("minus requires matching matrix/quaternion operand types") != std::string::npos);
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
