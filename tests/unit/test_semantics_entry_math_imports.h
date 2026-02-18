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
  CHECK(error.find("math builtin requires import /math/* or /math/<name>: clamp") != std::string::npos);
}

TEST_CASE("math builtin resolves with import") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  return(clamp(2i32, 1i32, 5i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
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
  CHECK(error.find("math builtin requires import /math/* or /math/<name>: sin") != std::string::npos);
}

TEST_CASE("math trig builtin resolves with import") {
  const std::string source = R"(
import /math/*
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
import /math/sin
[return<f32>]
main() {
  return(sin(0.5f32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("math trig builtin rejects non-imported name") {
  const std::string source = R"(
import /math/sin
[return<f32>]
main() {
  return(cos(0.5f32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("math builtin requires import /math/* or /math/<name>: cos") != std::string::npos);
}

TEST_CASE("math-qualified builtin works without import") {
  const std::string source = R"(
[return<int>]
main() {
  return(/math/clamp(2i32, 1i32, 5i32))
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
  CHECK(error.find("math constant requires import /math/* or /math/<name>: pi") != std::string::npos);
}

TEST_CASE("math constants resolve with import") {
  const std::string source = R"(
import /math/*
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
import /math/pi
[return<f64>]
main() {
  return(pi)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("math constant explicit import does not allow others") {
  const std::string source = R"(
import /math/pi
[return<f64>]
main() {
  return(plus(pi, tau))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("math constant requires import /math/* or /math/<name>: tau") != std::string::npos);
}

TEST_CASE("math import rejects root definition conflicts") {
  const std::string source = R"(
import /math/*
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
import /math/pi
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
import /math/*
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
import /math/pi
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
  return(/math/pi)
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
  return(/math/plus(1i32, 2i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /math/plus") != std::string::npos);
}

TEST_CASE("import rejects unknown math builtin") {
  const std::string source = R"(
import /math/nope
[return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown import path: /math/nope") != std::string::npos);
}

TEST_CASE("import rejects math builtin conflicts") {
  const std::string source = R"(
import /math/*
import /util
namespace util {
  [return<f32>]
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
  [return<int>]
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
  [struct]
  Thing() {
    [i32] value{1i32}
  }

  [return<int>]
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
