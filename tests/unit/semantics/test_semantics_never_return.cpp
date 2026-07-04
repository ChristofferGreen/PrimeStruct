#include "third_party/doctest.h"

#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.never_return");

TEST_CASE("accepts never-returning definition") {
  const std::string source = R"(
[return<never>]
myPanic([i32] code) {
  [array<i32>] empty{array<i32>()}
  /at(empty, code)
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("call to never definition terminates control path") {
  const std::string source = R"(
[return<never>]
myPanic([i32] code) {
  [array<i32>] empty{array<i32>()}
  /at(empty, code)
}

[return<i32>]
pick([i32] value) {
  if(greater_than(value, 0i32), then(){ return(value) }, else(){ })
  myPanic(value)
}

[return<int>]
main() {
  return(pick(7i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("rejects bare return inside never definition") {
  const std::string source = R"(
[return<never>]
myPanic([i32] code) {
  return()
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("never-returning definition cannot contain return") != std::string::npos);
}

TEST_CASE("rejects never as binding type") {
  const std::string source = R"(
[return<int>]
main() {
  [never] value{1i32}
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unsupported binding type: never") != std::string::npos);
}

TEST_CASE("missing return still rejected without diverging call") {
  const std::string source = R"(
[return<i32>]
pick([i32] value) {
  if(greater_than(value, 0i32), then(){ return(value) }, else(){ })
}

[return<int>]
main() {
  return(pick(7i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("not all control paths return") != std::string::npos);
}

TEST_SUITE_END();
