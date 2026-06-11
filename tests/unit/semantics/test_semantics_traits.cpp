#include "third_party/doctest.h"

#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.traits");

TEST_CASE("builtin numeric traits are satisfied") {
  const std::string source = R"(
[return<i32> Additive<i32> Multiplicative<i32> Comparable<i32>]
main() {
  return(plus(1i32, 2i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("missing trait requirement reports error") {
  const std::string source = R"(
[struct]
Vec2() {
  [i32] x{0i32}
  [i32] y{0i32}
}

[return<Vec2>]
/Vec2/plus([Vec2] left, [Vec2] right) {
  return(Vec2{})
}

[return<Vec2> Multiplicative<Vec2>]
main() {
  return(Vec2{})
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("Multiplicative") != std::string::npos);
  CHECK(error.find("multiply") != std::string::npos);
}

TEST_CASE("trait signature mismatch reports error") {
  const std::string source = R"(
[struct]
Widget() {
  [i32] id{0i32}
}

[return<i32>]
/Widget/equal([Widget] left, [Widget] right) {
  return(0i32)
}

[return<bool>]
/Widget/less_than([Widget] left, [Widget] right) {
  return(false)
}

[return<i32> Comparable<Widget>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("Comparable") != std::string::npos);
  CHECK(error.find("equal") != std::string::npos);
}

TEST_CASE("indexable builtins match element type") {
  const std::string source = R"(
[return<i32> Indexable<array<i32>, i32>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("indexable mismatch reports error") {
  const std::string source = R"(
[return<i32> Indexable<array<i32>, i64>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("Indexable") != std::string::npos);
  CHECK(error.find("count") != std::string::npos);
}

TEST_SUITE_END();
