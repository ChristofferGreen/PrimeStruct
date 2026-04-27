#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.return_inference");

TEST_CASE("infers return type without transform") {
  const std::string source = R"(
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("return auto infers value type") {
  const std::string source = R"(
[return<auto>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("return auto conflicts on mixed types") {
  const std::string source = R"(
[return<auto>]
main() {
  if(true,
    then(){ return(1i32) },
    else(){ return(1.5f) })
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("conflicting return types on /main") != std::string::npos);
}

TEST_CASE("return auto conflicts on mixed call sites") {
  const std::string source = R"(
[return<auto>]
value_int() {
  return(1i32)
}

[return<auto>]
value_float() {
  return(1.5f)
}

[return<auto>]
main() {
  if(true,
    then(){ return(value_int()) },
    else(){ return(value_float()) })
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("conflicting return types on /main") != std::string::npos);
}

TEST_CASE("return auto conflicts on named/default call path") {
  const std::string source = R"(
[return<auto>]
pick([auto] value{1i32}, [bool] as_float{false}) {
  if(as_float,
    then(){ return(1.5f) },
    else(){ return(value) })
}

[return<i32>]
main() {
  return(pick([as_float] true))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("conflicting return types on /pick") != std::string::npos);
}

TEST_CASE("infers array return type without transform") {
  const std::string source = R"(
main() {
  return(array<i32>{1i32, 2i32})
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("restrict matches inferred binding type during return inference") {
  const std::string source = R"(
main() {
  [restrict<i64>] value{1i64}
  return(value)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("restrict matches inferred binding type inside block return inference") {
  const std::string source = R"(
main() {
  return(block(){
    [restrict<i64>] value{1i64}
    value
  })
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("implicit void definition without return validates") {
  const std::string source = R"(
main() {
  [i32] value{1i32}
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("return auto without return is diagnostic") {
  const std::string source = R"(
[return<auto>]
main() {
  [i32] value{1i32}
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unable to infer return type on /main") != std::string::npos);
}

TEST_CASE("return inference requires inferable binding types") {
  const std::string source = R"(
main() {
  value{if(true, then(){}, else(){})}
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("then block must produce a value") != std::string::npos);
}

TEST_CASE("infers float return type without transform") {
  const std::string source = R"(
main() {
  return(1.5f)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("infers return type from builtin plus") {
  const std::string source = R"(
main() {
  return(plus(1i32, 2i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("infers return type from builtin negate") {
  const std::string source = R"(
main() {
  return(negate(2i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("implicit auto inference supports nested helper calls") {
  const std::string source = R"(
[return<auto>]
identity([auto] value) {
  return(value)
}

[return<auto>]
wrap([auto] value) {
  return(identity(value))
}

[return<i32>]
main() {
  return(wrap(9i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("infers struct return type without transform") {
  const std::string source = R"(
[struct]
Point() {
  [i32] x{1i32}
  [i32] y{2i32}
}

make_point([i32] x, [i32] y) {
  return(Point{[x] x, [y] y})
}

[return<int>]
main() {
  [Point] value{make_point(1i32, 2i32)}
  return(plus(value.x, value.y))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("return auto infers struct constructor return type") {
  const std::string source = R"(
[struct]
Point() {
  [i32] x{1i32}
  [i32] y{2i32}
}

[return<auto>]
make_point([i32] x, [i32] y) {
  return(Point{[x] x, [y] y})
}

[return<int>]
main() {
  [Point] value{make_point(1i32, 2i32)}
  return(plus(value.x, value.y))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("return struct mismatch is diagnostic") {
  const std::string source = R"(
[struct]
Point() {
  [i32] x{1i32}
  [i32] y{2i32}
}

[struct]
Other() {
  [i32] z{3i32}
  [i32] w{4i32}
}

[return<Point>]
make_point() {
  return(Other{})
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("return type mismatch") != std::string::npos);
}

TEST_SUITE_END();
