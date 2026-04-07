#include "third_party/doctest.h"

#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.software_numeric");

TEST_CASE("rejects integer binding and return") {
  const std::string source = R"(
[return<integer>]
main() {
  [integer] value{convert<integer>(1i32)}
  return(value)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unsupported convert target type: integer") != std::string::npos);
}

TEST_CASE("rejects decimal binding and return") {
  const std::string source = R"(
[return<decimal>]
main() {
  [decimal] value{convert<decimal>(1.5f32)}
  return(value)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unsupported convert target type: decimal") != std::string::npos);
}

TEST_CASE("rejects complex binding and return") {
  const std::string source = R"(
[return<complex>]
main() {
  [complex] value{convert<complex>(convert<decimal>(2.0f32))}
  return(value)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("binding initializer validateExpr failed") != std::string::npos);
}

TEST_CASE("rejects mixed software and fixed arithmetic") {
  const std::string source = R"(
[return<int>]
main() {
  [integer] value{convert<integer>(1i32)}
  return(plus(value, 2i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unsupported convert target type: integer") != std::string::npos);
}

TEST_CASE("rejects mixed software numeric categories") {
  const std::string source = R"(
[return<int>]
main() {
  [integer] a{convert<integer>(1i32)}
  [decimal] b{convert<decimal>(1.0f32)}
  return(convert<int>(plus(a, b)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unsupported convert target type: integer") != std::string::npos);
}

TEST_CASE("rejects ordered comparisons on complex") {
  const std::string source = R"(
[return<bool>]
main() {
  [complex] a{convert<complex>(convert<decimal>(1.0f32))}
  [complex] b{convert<complex>(convert<decimal>(2.0f32))}
  return(less_than(a, b))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("binding initializer validateExpr failed") != std::string::npos);
}

TEST_CASE("rejects mixed complex and real comparisons") {
  const std::string source = R"(
[return<bool>]
main() {
  [complex] a{convert<complex>(convert<decimal>(1.0f32))}
  [decimal] b{convert<decimal>(2.0f32)}
  return(equal(a, b))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("binding initializer validateExpr failed") != std::string::npos);
}

TEST_SUITE_END();
