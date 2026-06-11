#include "third_party/doctest.h"

#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.math_imports");

TEST_CASE("math builtin requires import") {
  const std::string source = R"(
[return<int>]
main() {
  return(clamp(2i32, 1i32, 5i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("math builtin requires import /std/math/* or /std/math/<name>: clamp") != std::string::npos);
}

TEST_CASE("math builtin resolves with import") {
  const std::string source = R"(
import /std/math/*
[return<int>]
main() {
  return(clamp(2i32, 1i32, 5i32))
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("math matrix stdlib constructor requires import") {
  const std::string source = R"(
[return<int>]
main() {
  [auto] value{Mat2{}}
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: Mat2") != std::string::npos);
}

TEST_CASE("math matrix stdlib constructor resolves with import") {
  const std::string source = R"(
import /std/math/*
[return<int>]
main() {
  [Mat2] m2{Mat2{1.0f32, 2.0f32, 3.0f32, 4.0f32}}
  [Mat3] m3{Mat3{
    5.0f32, 6.0f32, 7.0f32,
    8.0f32, 9.0f32, 10.0f32,
    11.0f32, 12.0f32, 13.0f32
  }}
  [Mat4] m4{Mat4{
    14.0f32, 15.0f32, 16.0f32, 17.0f32,
    18.0f32, 19.0f32, 20.0f32, 21.0f32,
    22.0f32, 23.0f32, 24.0f32, 25.0f32,
    26.0f32, 27.0f32, 28.0f32, 29.0f32
  }}
  return(convert<int>(m2.m00 + m2.m11 + m3.m12 + m4.m33))
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("math matrix stdlib constructor supports trailing defaults") {
  const std::string source = R"(
import /std/math/*
[return<int>]
main() {
  [Mat2] value{Mat2{1.0f32, 2.0f32, 3.0f32}}
  return(convert<int>(value.m10 + value.m11))
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("math quaternion stdlib constructor requires import") {
  const std::string source = R"(
[return<int>]
main() {
  [auto] value{Quat{}}
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: Quat") != std::string::npos);
}

TEST_CASE("math quaternion stdlib constructor resolves with import") {
  const std::string source = R"(
import /std/math/*
[return<int>]
main() {
  [Quat] raw{Quat{0.0f32, 0.0f32, 0.0f32, 2.0f32}}
  [Quat] normalized{raw.toNormalized()}
  [Quat] zero{Quat{0.0f32, 0.0f32, 0.0f32, 0.0f32}.normalize()}
  return(convert<int>(normalized.w + zero.x + zero.y + zero.z + zero.w))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("math quaternion stdlib constructor supports trailing defaults") {
  const std::string source = R"(
import /std/math/*
[return<int>]
main() {
  [Quat] value{Quat{0.0f32, 0.0f32, 1.0f32}}
  return(convert<int>(value.z + value.w))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("math quat_to_mat3 helper requires import") {
  const std::string source = R"(
[return<int>]
main() {
  [auto] value{quat_to_mat3(0i32)}
  return(convert<int>(value.m00))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: quat_to_mat3") != std::string::npos);
}

TEST_CASE("math quat_to_mat3 helper resolves with import") {
  const std::string source = R"(
import /std/math/*
[return<int>]
main() {
  [Quat] raw{Quat{2.0f32, 0.0f32, 0.0f32, 0.0f32}}
  [Mat3] basis{quat_to_mat3(raw)}
  return(convert<int>(basis.m00 - basis.m11 - basis.m22))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("math quat_to_mat3 helper resolves with explicit import") {
  const std::string source = R"(
import /std/math/quat_to_mat3
import /std/math/Quat
[return<int>]
main() {
  [auto] basis{quat_to_mat3(Quat{0.0f32, 0.0f32, 0.0f32, 1.0f32})}
  return(convert<int>(basis.m00 + basis.m11 + basis.m22))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("math quat_to_mat4 helper requires import") {
  const std::string source = R"(
[return<int>]
main() {
  [auto] value{quat_to_mat4(0i32)}
  return(convert<int>(value.m00))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: quat_to_mat4") != std::string::npos);
}

TEST_CASE("math quat_to_mat4 helper resolves with import") {
  const std::string source = R"(
import /std/math/*
[return<int>]
main() {
  [Quat] raw{Quat{2.0f32, 0.0f32, 0.0f32, 0.0f32}}
  [Mat4] basis{quat_to_mat4(raw)}
  return(convert<int>(basis.m00 - basis.m11 - basis.m22 + basis.m33))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("math quat_to_mat4 helper resolves with explicit import") {
  const std::string source = R"(
import /std/math/quat_to_mat4
import /std/math/Quat
[return<int>]
main() {
  [auto] basis{quat_to_mat4(Quat{0.0f32, 0.0f32, 0.0f32, 1.0f32})}
  return(convert<int>(basis.m00 + basis.m11 + basis.m22 + basis.m33))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("math mat3_to_quat helper requires import") {
  const std::string source = R"(
[return<int>]
main() {
  [auto] value{mat3_to_quat(0i32)}
  return(convert<int>(value.w))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: mat3_to_quat") != std::string::npos);
}

TEST_CASE("math mat3_to_quat helper resolves with import") {
  const std::string source = R"(
import /std/math/*
[return<int>]
main() {
  [Mat3] basis{Mat3{
    1.0f32, 0.0f32, 0.0f32,
    0.0f32, 1.0f32, 0.0f32,
    0.0f32, 0.0f32, 1.0f32
  }}
  [Quat] value{mat3_to_quat(basis)}
  return(convert<int>(value.w + value.x + value.y + value.z))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("math mat3_to_quat helper resolves with explicit import") {
  const std::string source = R"(
import /std/math/mat3_to_quat
import /std/math/Mat3
[return<int>]
main() {
  [auto] value{mat3_to_quat(Mat3{
    1.0f32, 0.0f32, 0.0f32,
    0.0f32, 1.0f32, 0.0f32,
    0.0f32, 0.0f32, 1.0f32
  })}
  return(convert<int>(value.w + value.x + value.y + value.z))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("math binding accepts implicit matrix quaternion conversion") {
  const std::string source = R"(
import /std/math/*
[return<int>]
main() {
  [Quat] value{Mat3{
    1.0f32, 0.0f32, 0.0f32,
    0.0f32, 1.0f32, 0.0f32,
    0.0f32, 0.0f32, 1.0f32
  }}
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("math helper rejects implicit matrix quaternion conversion with stable diagnostic") {
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
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find(
            "argument type mismatch for /std/math/quat_to_mat3 parameter value: implicit matrix/quaternion family "
            "conversion requires explicit helper: expected /std/math/Quat got /std/math/Mat3") !=
        std::string::npos);
}

TEST_CASE("math return rejects implicit matrix quaternion conversion with stable diagnostic") {
  const std::string source = R"(
import /std/math/*
[return<Quat>]
main() {
  return(Mat3{
    1.0f32, 0.0f32, 0.0f32,
    0.0f32, 1.0f32, 0.0f32,
    0.0f32, 0.0f32, 1.0f32
  })
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("return type mismatch: implicit matrix/quaternion family conversion requires explicit helper: expected /std/math/Quat got /std/math/Mat3") !=
        std::string::npos);
}

TEST_CASE("math trig builtin requires import") {
  const std::string source = R"(
[return<f32>]
main() {
  return(sin(0.5f32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("math builtin requires import /std/math/* or /std/math/<name>: sin") != std::string::npos);
}

TEST_CASE("math trig builtin resolves with import") {
  const std::string source = R"(
import /std/math/*
[return<f32>]
main() {
  return(sin(0.5f32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("math trig builtin resolves with explicit import") {
  const std::string source = R"(
import /std/math/sin
[return<f32>]
main() {
  return(sin(0.5f32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("math trig explicit import currently exposes sibling builtin names") {
  const std::string source = R"(
import /std/math/sin
[return<f32>]
main() {
  return(cos(0.5f32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("math-qualified builtin works without import") {
  const std::string source = R"(
[return<int>]
main() {
  return(/std/math/clamp(2i32, 1i32, 5i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("math constant requires import") {
  const std::string source = R"(
[return<f64>]
main() {
  return(pi)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("math constant requires import /std/math/* or /std/math/<name>: pi") != std::string::npos);
}

TEST_CASE("math constants resolve with import") {
  const std::string source = R"(
import /std/math/*
[return<f64>]
main() {
  return(plus(plus(pi, tau), e))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("math constant resolves with explicit import") {
  const std::string source = R"(
import /std/math/pi
[return<f64>]
main() {
  return(pi)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("math constant explicit import currently exposes sibling constants") {
  const std::string source = R"(
import /std/math/pi
[return<f64>]
main() {
  return(plus(pi, tau))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("math import rejects root definition conflicts") {
  const std::string source = R"(
import /std/math/*
[return<f32>]
sin([f32] value) {
  return(value)
}
[return<f32>]
main() {
  return(sin(0.5f32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("import creates name conflict: sin") != std::string::npos);
}

TEST_CASE("explicit math import rejects root definition conflicts") {
  const std::string source = R"(
import /std/math/pi
[return<f64>]
pi() {
  return(3.14f64)
}
[return<f64>]
main() {
  return(pi())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("import creates name conflict: pi") != std::string::npos);
}

TEST_CASE("math import rejects root conflicts after definitions") {
  const std::string source = R"(
[return<f32>]
sin([f32] value) {
  return(value)
}
import /std/math/*
[return<f32>]
main() {
  return(sin(0.5f32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("import creates name conflict: sin") != std::string::npos);
}

TEST_CASE("explicit math import rejects root conflicts after definitions") {
  const std::string source = R"(
[return<f64>]
pi() {
  return(3.14f64)
}
import /std/math/pi
[return<f64>]
main() {
  return(pi())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("import creates name conflict: pi") != std::string::npos);
}

TEST_CASE("math-qualified constant works without import") {
  const std::string source = R"(
[return<f64>]
main() {
  return(/std/math/pi)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("math-qualified non-math builtin fails") {
  const std::string source = R"(
[return<int>]
main() {
  return(/std/math/plus(1i32, 2i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/math/plus") != std::string::npos);
}

TEST_CASE("import rejects unknown math builtin") {
  const std::string source = R"(
import /std/math/nope
[return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown import path: /std/math/nope") != std::string::npos);
}

TEST_CASE("import rejects math builtin conflicts") {
  const std::string source = R"(
import /std/math/*
import /util
namespace util {
  [public return<f32>]
  sin([f32] value) {
    return(value)
  }
}
[return<f32>]
main() {
  return(sin(0.5f32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("import creates name conflict: sin") != std::string::npos);
}

TEST_CASE("import aliases namespace definitions") {
  const std::string source = R"(
import /util
namespace util {
  [public return<int>]
  add_one([i32] value) {
    return(plus(value, 1i32))
  }
}
[return<int>]
main() {
  return(add_one(2i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("import aliases namespace types and methods") {
  const std::string source = R"(
import /util
namespace util {
  [public struct]
  Thing() {
    [i32] value{1i32}
  }

  [public return<int>]
  /util/Thing/get([Thing] self) {
    return(7i32)
  }
}
[return<int>]
main() {
  [Thing] item{1i32}
  return(item.get())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_SUITE_END();
