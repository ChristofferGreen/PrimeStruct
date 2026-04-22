#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.parameters");

TEST_CASE("parameter default literal is allowed") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

[return<int>]
add([i32] left, [i32] right{10i32}) {
  return(plus(left, right))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("parameter default pure expression is allowed") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

[return<int>]
add([i32] left, [i32] right{plus(1i32, 2i32)}) {
  return(plus(left, right))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("parameter qualifiers are allowed") {
  const std::string source = R"(
[return<int>]
add([i32 mut] left, [copy i32] right) {
  return(plus(left, right))
}

[return<int>]
main() {
  return(add(1i32, 2i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("omitted parameter envelopes are inferred through parser shorthand") {
  const std::string source = R"(
[return<auto>]
identity(value) {
  return(value)
}

[return<int>]
main() {
  return(identity(7i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("omitted parameter envelopes reject templated definitions through parser shorthand") {
  const std::string source = R"(
[return<auto>]
identity<T>(value) {
  return(value)
}

[return<int>]
main() {
  return(identity(7i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("implicit auto parameters are only supported on non-templated definitions") !=
        std::string::npos);
}

TEST_CASE("parameter restrict matches binding type") {
  const std::string source = R"(
[return<int>]
add([restrict<i32> i32] value) {
  return(value)
}

[return<int>]
main() {
  return(add(2i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("parameter restrict rejects mismatched type") {
  const std::string source = R"(
[return<int>]
add([restrict<i32> i64] value) {
  return(1i32)
}

[return<int>]
main() {
  return(add(2i64))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("restrict type does not match binding type") != std::string::npos);
}

TEST_CASE("parameter accepts software numeric type") {
  const std::string source = R"(
[return<int>]
add([complex] value) {
  return(1i32)
}

[return<int>]
main() {
  return(add(1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("software numeric parameter rejects non-numeric argument") {
  const std::string source = R"(
[return<int>]
add([complex] value) {
  return(1i32)
}

[return<int>]
main() {
  return(add("bad"raw_utf8))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch for /add parameter value: expected complex") != std::string::npos);
}

TEST_CASE("parameter default vector literal requires heap_alloc effect") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

[return<int>]
add([i32] left, [vector<i32>] right{vector<i32>(1i32)}) {
  return(left)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("vector literal requires heap_alloc effect") != std::string::npos);
}

TEST_CASE("parameter default vector literal allows heap_alloc effect") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

[effects(heap_alloc), return<int>]
add([i32] left, [vector<i32>] right{vector<i32>(1i32)}) {
  return(left)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("parameter default empty vector literal is allowed") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

[return<int>]
add([i32] left, [vector<i32>] right{vector<i32>()}) {
  return(left)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("if statement sugar allows empty else block in statement position") {
  const std::string source = R"(
[return<int>]
main() {
  if(true) { 1i32 } else { }
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("parameter default expression rejects name references") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

[return<int>]
helper() {
  return(1i32)
}

[return<int>]
add([i32] left, [i32] right{plus(left, 1i32)}) {
  return(plus(left, right))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("parameter default must be a literal or pure expression") != std::string::npos);
}

TEST_CASE("parameter default expression rejects user-defined calls") {
  const std::string source = R"(
[return<int>]
helper() {
  return(1i32)
}

[return<int>]
main() {
  return(1i32)
}

[return<int>]
add([i32] left, [i32] right{helper()}) {
  return(plus(left, right))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("parameter default must be a literal or pure expression") != std::string::npos);
}

TEST_CASE("parameter default expression rejects user-defined array call") {
  const std::string source = R"(
[return<i32>]
array([i32] value) {
  return(value)
}

[return<int>]
main() {
  return(1i32)
}

[return<int>]
add([i32] left, [i32] right{array(1i32)}) {
  return(plus(left, right))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("parameter default must be a literal or pure expression") != std::string::npos);
}

TEST_CASE("parameter default expression allows builtin array literal") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

[return<int>]
take_first([array<i32>] values{array<i32>(4i32, 5i32)}) {
  return(at(values, 0i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("parameter default expression rejects block arguments") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

[return<int>]
add([i32] left, [i32] right{block(){ 1i32 }}) {
  return(plus(left, right))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("parameter default must be a literal or pure expression") != std::string::npos);
}

TEST_CASE("parameter default expression rejects bindings") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

[return<int>]
add([i32] left, [i32] right{block(){ [i32] temp{1i32} temp }}) {
  return(plus(left, right))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("parameter default must be a literal or pure expression") != std::string::npos);
}

TEST_CASE("parameter default expression rejects named arguments") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

[return<int>]
add_one([i32] value) {
  return(plus(value, 1i32))
}

[return<int>]
add([i32] left, [i32] right{add_one([value] 1i32)}) {
  return(plus(left, right))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("parameter default does not accept named arguments") != std::string::npos);
}

TEST_CASE("parameter default rejects multiple arguments") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

[return<int>]
add([i32] left, [i32] right{1i32 2i32}) {
  return(plus(left, right))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("parameter default must be a literal or pure expression") != std::string::npos);
}

TEST_CASE("infers parameter type from default initializer") {
  const std::string source = R"(
[return<i64>]
id([mut] value{3i64}) {
  return(value)
}

[return<int>]
main() {
  return(convert<i32>(id()))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("typed variadic parameter accepts trailing positional arguments") {
  const std::string source = R"(
[return<i32>]
keep_head([i32] head, [i32] values...) {
  return(head)
}

[return<i32>]
main() {
  return(keep_head(1i32, 2i32, 3i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("typed variadic parameter allows empty trailing pack") {
  const std::string source = R"(
[return<i32>]
keep_head([i32] head, [i32] values...) {
  return(head)
}

[return<i32>]
main() {
  return(keep_head(1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("typed variadic parameter rejects mismatched trailing argument") {
  const std::string source = R"(
[return<i32>]
keep_head([i32] head, [i32] values...) {
  return(head)
}

[return<i32>]
main() {
  return(keep_head(1i32, true))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch for /keep_head parameter values: expected i32 got bool") !=
        std::string::npos);
}

TEST_CASE("implicit variadic pack infers homogeneous element type") {
  const std::string source = R"(
[return<i32>]
keep_head([i32] head, values...) {
  return(head)
}

[return<i32>]
main() {
  return(keep_head(1i32, 2i32, 3i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("implicit variadic pack rejects heterogeneous element types") {
  const std::string source = R"(
[return<i32>]
keep_head([i32] head, values...) {
  return(head)
}

[return<i32>]
main() {
  return(keep_head(1i32, 2i32, true))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("implicit template arguments conflict on /keep_head") != std::string::npos);
}

TEST_CASE("typed variadic parameter accepts spread args pack forwarding") {
  const std::string source = R"(
[return<i32>]
keep_head([i32] head, [i32] values...) {
  return(head)
}

[return<i32>]
forward([i32] head, [i32] values...) {
  return(keep_head(head, [spread] values))
}

[return<i32>]
main() {
  return(forward(1i32, 2i32, 3i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("implicit variadic pack infers from spread args pack") {
  const std::string source = R"(
[return<i32>]
count_values(values...) {
  return(count(values))
}

[return<i32>]
forward([i32] values...) {
  return(count_values([spread] values))
}

[return<i32>]
main() {
  return(forward(1i32, 2i32, 3i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("spread argument requires variadic parameter") {
  const std::string source = R"(
[return<i32>]
take([i32] value) {
  return(value)
}

[return<i32>]
forward([i32] values...) {
  return(take([spread] values))
}

[return<i32>]
main() {
  return(forward(1i32, 2i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("spread argument requires variadic parameter") != std::string::npos);
}

TEST_CASE("spread argument requires args pack value") {
  const std::string source = R"(
[return<i32>]
count_values([i32] values...) {
  return(count(values))
}

[return<i32>]
main() {
  return(count_values([spread] 1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("spread argument requires args<T> value") != std::string::npos);
}

TEST_CASE("execution spread argument requires args pack value") {
  const std::string source = R"(
[return<i32>]
main() {
  return(1i32)
}

[return<i32>]
count_values([i32] values...) {
  return(count(values))
}

count_values([spread] 1i32)
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("spread argument requires args<T> value") != std::string::npos);
}

TEST_CASE("variadic pointer string pack declaration is rejected") {
  const std::string source = R"(
[return<i32>]
score([args<Pointer<string>>] values) {
  return(count(values))
}

[return<i32>]
main() {
  [string] first{"first"utf8}
  [Pointer<string>] p0{location(first)}
  score(p0)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("variadic args<T> does not support string pointers or references") != std::string::npos);
}

TEST_CASE("variadic reference string pack declaration is rejected") {
  const std::string source = R"(
[return<i32>]
score([args<Reference<string>>] values) {
  return(count(values))
}

[return<i32>]
main() {
  [string] first{"first"utf8}
  [Reference<string>] r0{location(first)}
  score(r0)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("variadic args<T> does not support string pointers or references") != std::string::npos);
}

TEST_SUITE_END();
