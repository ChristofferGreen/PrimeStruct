#include "primec/Lexer.h"
#include "primec/Parser.h"
#include "primec/Semantics.h"

#include "third_party/doctest.h"

namespace {
primec::Program parseProgram(const std::string &source) {
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK(parser.parse(program, error));
  CHECK(error.empty());
  return program;
}

bool validateProgram(const std::string &source, const std::string &entry, std::string &error) {
  auto program = parseProgram(source);
  primec::Semantics semantics;
  return semantics.validate(program, entry, error);
}
} // namespace

TEST_SUITE_BEGIN("primestruct.semantics");

TEST_CASE("missing entry definition fails") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/missing", error));
  CHECK(error.find("missing entry definition") != std::string::npos);
}

TEST_CASE("unknown identifier fails") {
  const std::string source = R"(
[return<int>]
main(x) {
  return(y)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown identifier") != std::string::npos);
}

TEST_CASE("argument count mismatch fails") {
  const std::string source = R"(
[return<int>]
callee(x) {
  return(x)
}

[return<int>]
main() {
  return(callee(1i32, 2i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch") != std::string::npos);
}

TEST_CASE("execution target must exist") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

run()
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown execution target") != std::string::npos);
}

TEST_CASE("execution argument count mismatch fails") {
  const std::string source = R"(
[return<int>]
task(x) {
  return(x)
}

[return<int>]
main() {
  return(1i32)
}

task()
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch") != std::string::npos);
}

TEST_CASE("execution body arguments must be calls") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

[return<int>]
execute_repeat(x) {
  return(x)
}

execute_repeat(3i32) { 1i32 }
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("execution body arguments must be calls") != std::string::npos);
}

TEST_CASE("unsupported return type fails") {
  const std::string source = R"(
[return<u32>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unsupported return type") != std::string::npos);
}

TEST_CASE("bool return type validates") {
  const std::string source = R"(
[return<bool>]
main() {
  return(true)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("float return type validates") {
  const std::string source = R"(
[return<float>]
main() {
  return(1.5f)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("effects transform validates identifiers") {
  const std::string source = R"(
[effects(global_write, io_stdout), return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("effects transform rejects template arguments") {
  const std::string source = R"(
[effects<io>, return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("effects transform does not accept template arguments") != std::string::npos);
}

TEST_CASE("effects transform rejects invalid capability") {
  const std::string source = R"(
[effects("io"), return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("invalid effects capability") != std::string::npos);
}

TEST_CASE("effects transform rejects duplicate capability") {
  const std::string source = R"(
[effects(io_stdout, io_stdout), return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("duplicate effects capability") != std::string::npos);
}

TEST_CASE("capabilities transform validates identifiers") {
  const std::string source = R"(
[capabilities(render_graph, io_stdout), return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("capabilities transform rejects template arguments") {
  const std::string source = R"(
[capabilities<io>, return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("capabilities transform does not accept template arguments") != std::string::npos);
}

TEST_CASE("capabilities transform rejects invalid capability") {
  const std::string source = R"(
[capabilities("io"), return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("invalid capability") != std::string::npos);
}

TEST_CASE("capabilities transform rejects duplicate capability") {
  const std::string source = R"(
[capabilities(io_stdout, io_stdout), return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("duplicate capability") != std::string::npos);
}

TEST_CASE("execution effects transform validates") {
  const std::string source = R"(
[return<int>]
task(x) {
  return(x)
}

[return<int>]
main() {
  return(1i32)
}

[effects(io_stdout)]
task(1i32)
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("execution capabilities transform validates") {
  const std::string source = R"(
[return<int>]
task(x) {
  return(x)
}

[return<int>]
main() {
  return(1i32)
}

[capabilities(io_stdout)]
task(1i32)
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("execution capabilities rejects template arguments") {
  const std::string source = R"(
[return<int>]
task(x) {
  return(x)
}

[return<int>]
main() {
  return(1i32)
}

[capabilities<io>]
task(1i32)
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("capabilities transform does not accept template arguments") != std::string::npos);
}

TEST_CASE("execution capabilities rejects invalid capability") {
  const std::string source = R"(
[return<int>]
task(x) {
  return(x)
}

[return<int>]
main() {
  return(1i32)
}

[capabilities("io")]
task(1i32)
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("invalid capability") != std::string::npos);
}

TEST_CASE("execution capabilities rejects duplicate capability") {
  const std::string source = R"(
[return<int>]
task(x) {
  return(x)
}

[return<int>]
main() {
  return(1i32)
}

[capabilities(io_stdout, io_stdout)]
task(1i32)
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("duplicate capability") != std::string::npos);
}

TEST_CASE("execution effects rejects template arguments") {
  const std::string source = R"(
[return<int>]
task(x) {
  return(x)
}

[return<int>]
main() {
  return(1i32)
}

[effects<io>]
task(1i32)
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("effects transform does not accept template arguments") != std::string::npos);
}

TEST_CASE("execution effects rejects invalid capability") {
  const std::string source = R"(
[return<int>]
task(x) {
  return(x)
}

[return<int>]
main() {
  return(1i32)
}

[effects("io")]
task(1i32)
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("invalid effects capability") != std::string::npos);
}

TEST_CASE("execution effects rejects duplicate capability") {
  const std::string source = R"(
[return<int>]
task(x) {
  return(x)
}

[return<int>]
main() {
  return(1i32)
}

[effects(io, io)]
task(1i32)
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("duplicate effects capability") != std::string::npos);
}

TEST_CASE("capabilities transform validates identifiers") {
  const std::string source = R"(
[capabilities(asset_read, gpu_queue), return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("capabilities transform rejects template arguments") {
  const std::string source = R"(
[capabilities<io>, return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("capabilities transform does not accept template arguments") != std::string::npos);
}

TEST_CASE("capabilities transform rejects invalid capability") {
  const std::string source = R"(
[capabilities("io"), return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("invalid capability") != std::string::npos);
}

TEST_CASE("capabilities transform rejects duplicate capability") {
  const std::string source = R"(
[capabilities(gpu, gpu), return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("duplicate capability") != std::string::npos);
}

TEST_CASE("align_bytes validates integer argument") {
  const std::string source = R"(
[align_bytes(16), return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("align_bytes rejects non-integer argument") {
  const std::string source = R"(
[align_bytes(foo), return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("align_bytes requires a positive integer argument") != std::string::npos);
}

TEST_CASE("align_kbytes rejects template arguments") {
  const std::string source = R"(
[align_kbytes<i32>(4), return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("align_kbytes does not accept template arguments") != std::string::npos);
}

TEST_CASE("struct transform validates without args") {
  const std::string source = R"(
[struct]
main() {
  [i32] value(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("pod transform validates without args") {
  const std::string source = R"(
[pod]
main() {
  [i32] value(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("struct transform rejects template arguments") {
  const std::string source = R"(
[struct<i32>]
main() {
  [i32] value(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("struct transform does not accept template arguments") != std::string::npos);
}

TEST_CASE("struct transform rejects arguments") {
  const std::string source = R"(
[struct(foo)]
main() {
  [i32] value(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("struct transform does not accept arguments") != std::string::npos);
}

TEST_CASE("struct transform rejects return transform") {
  const std::string source = R"(
[struct, return<int>]
main() {
  [i32] value(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("struct definitions cannot declare return types") != std::string::npos);
}

TEST_CASE("struct transform rejects return statements") {
  const std::string source = R"(
[struct]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("struct definitions cannot contain return statements") != std::string::npos);
}

TEST_CASE("stack transform rejects return statements") {
  const std::string source = R"(
[stack]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("struct definitions cannot contain return statements") != std::string::npos);
}

TEST_CASE("struct transform rejects parameters") {
  const std::string source = R"(
[struct]
main(x) {
  [i32] value(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("struct definitions cannot declare parameters") != std::string::npos);
}

TEST_CASE("lifecycle helpers require struct context") {
  const std::string source = R"(
[return<void>]
Create() {
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/Create", error));
  CHECK(error.find("lifecycle helper must be nested inside a struct") != std::string::npos);
}

TEST_CASE("struct transforms are rejected on executions") {
  const std::string source = R"(
[return<int>]
task(x) {
  return(x)
}

[return<int>]
main() {
  return(1i32)
}

[struct]
task(1i32)
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("struct transforms are not allowed on executions") != std::string::npos);
}

TEST_CASE("builtin arithmetic calls validate") {
  const std::string source = R"(
namespace demo {
  [return<int>]
  main() {
    return(plus(1i32, 2i32))
  }
}
)";
  std::string error;
  CHECK(validateProgram(source, "/demo/main", error));
  CHECK(error.empty());
}

TEST_CASE("builtin negate calls validate") {
  const std::string source = R"(
[return<int>]
main() {
  return(negate(2i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("builtin comparison calls validate") {
  const std::string source = R"(
[return<int>]
main() {
  return(greater_than(2i32, 1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("builtin less_than calls validate") {
  const std::string source = R"(
[return<int>]
main() {
  return(less_than(1i32, 2i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("builtin equal calls validate") {
  const std::string source = R"(
[return<int>]
main() {
  return(equal(2i32, 2i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("builtin not_equal calls validate") {
  const std::string source = R"(
[return<int>]
main() {
  return(not_equal(2i32, 3i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("builtin greater_equal calls validate") {
  const std::string source = R"(
[return<int>]
main() {
  return(greater_equal(2i32, 2i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("builtin less_equal calls validate") {
  const std::string source = R"(
[return<int>]
main() {
  return(less_equal(2i32, 3i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("builtin and calls validate") {
  const std::string source = R"(
[return<int>]
main() {
  return(and(1i32, 1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("builtin or calls validate") {
  const std::string source = R"(
[return<int>]
main() {
  return(or(0i32, 1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("builtin not calls validate") {
  const std::string source = R"(
[return<int>]
main() {
  return(not(0i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("builtin clamp calls validate") {
  const std::string source = R"(
[return<int>]
main() {
  return(clamp(2i32, 1i32, 5i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("builtin arithmetic arity mismatch fails") {
  const std::string source = R"(
[return<int>]
main() {
  return(plus(1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for builtin plus") != std::string::npos);
}

TEST_CASE("builtin negate arity mismatch fails") {
  const std::string source = R"(
[return<int>]
main() {
  return(negate(1i32, 2i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for builtin negate") != std::string::npos);
}

TEST_CASE("builtin comparison arity mismatch fails") {
  const std::string source = R"(
[return<int>]
main() {
  return(greater_than(1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for builtin greater_than") != std::string::npos);
}

TEST_CASE("builtin less_than arity mismatch fails") {
  const std::string source = R"(
[return<int>]
main() {
  return(less_than(1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for builtin less_than") != std::string::npos);
}

TEST_CASE("builtin equal arity mismatch fails") {
  const std::string source = R"(
[return<int>]
main() {
  return(equal(1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for builtin equal") != std::string::npos);
}

TEST_CASE("builtin greater_equal arity mismatch fails") {
  const std::string source = R"(
[return<int>]
main() {
  return(greater_equal(1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for builtin greater_equal") != std::string::npos);
}

TEST_CASE("builtin and arity mismatch fails") {
  const std::string source = R"(
[return<int>]
main() {
  return(and(1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for builtin and") != std::string::npos);
}

TEST_CASE("builtin not arity mismatch fails") {
  const std::string source = R"(
[return<int>]
main() {
  return(not(1i32, 2i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for builtin not") != std::string::npos);
}

TEST_CASE("builtin clamp arity mismatch fails") {
  const std::string source = R"(
[return<int>]
main() {
  return(clamp(1i32, 2i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for builtin clamp") != std::string::npos);
}

TEST_CASE("void return can omit return statement") {
  const std::string source = R"(
[return<void>]
main() {
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("local binding names are visible") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value(6i32)
  return(value)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("local binding requires initializer") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value()
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("binding requires exactly one argument") != std::string::npos);
}

TEST_CASE("local binding type must be supported") {
  const std::string source = R"(
[return<int>]
main() {
  [u32] value(1i32)
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unsupported binding type") != std::string::npos);
}

TEST_CASE("float binding validates") {
  const std::string source = R"(
[return<int>]
main() {
  [float] value(1.5f)
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("bool binding validates") {
  const std::string source = R"(
[return<int>]
main() {
  [bool] value(true)
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("binding qualifiers are allowed") {
  const std::string source = R"(
[return<int>]
main() {
  [public float] exposure(1.5f)
  [private align_bytes(16) i32 mut] count(1i32)
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("pointer helpers validate") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value(1i32)
  return(deref(address_of(value)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("assign to mutable binding succeeds") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value(1i32)
  assign(value, 4i32)
  return(value)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("assign to immutable binding fails") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value(1i32)
  assign(value, 2i32)
  return(value)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("assign target must be a mutable binding") != std::string::npos);
}

TEST_CASE("if validates block arguments") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value(1i32)
  if(1i32, then{
    [i32] temp(2i32)
    assign(value, temp)
  }, else{ assign(value, 3i32) })
  return(value)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("if missing else fails") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value(1i32)
  if(1i32, then{ assign(value, 2i32) })
  return(value)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("if requires condition") != std::string::npos);
}

TEST_CASE("binding not allowed in expression") {
  const std::string source = R"(
[return<int>]
main() {
  return([i32] value(1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("binding not allowed in expression") != std::string::npos);
}

TEST_CASE("duplicate named arguments fail") {
  const std::string source = R"(
[return<int>]
foo(a, b) {
  return(a)
}

[return<int>]
main() {
  return(foo(a = 1i32, a = 2i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("duplicate named argument") != std::string::npos);
}

TEST_CASE("array literal validates") {
  const std::string source = R"(
[return<int>]
use(x) {
  return(1i32)
}

[return<int>]
main() {
  return(use(array<i32>(1i32, 2i32)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("named arguments match parameters") {
  const std::string source = R"(
[return<int>]
foo(a, b) {
  return(a)
}

[return<int>]
main() {
  return(foo(a = 1i32, b = 2i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("unknown named argument fails") {
  const std::string source = R"(
[return<int>]
foo(a, b) {
  return(a)
}

[return<int>]
main() {
  return(foo(c = 1i32, b = 2i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown named argument") != std::string::npos);
}

TEST_CASE("named argument duplicates positional parameter fails") {
  const std::string source = R"(
[return<int>]
foo(a, b) {
  return(a)
}

[return<int>]
main() {
  return(foo(1i32, a = 2i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("named argument duplicates parameter") != std::string::npos);
}

TEST_CASE("execution named arguments match parameters") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

[return<int>]
job(a, b) {
  return(a)
}

job(a = 1i32, b = 2i32)
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("named arguments not allowed on builtin calls") {
  const std::string source = R"(
[return<int>]
main() {
  return(plus(left = 1i32, right = 2i32))
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_CASE("convert builtin validates") {
  const std::string source = R"(
[return<int>]
main() {
  return(convert<int>(1.5f))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("convert missing template arg fails") {
  const std::string source = R"(
[return<int>]
main() {
  return(convert(1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("convert requires exactly one template argument") != std::string::npos);
}

TEST_CASE("convert unsupported template arg fails") {
  const std::string source = R"(
[return<int>]
main() {
  return(convert<u32>(1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unsupported convert target type") != std::string::npos);
}

TEST_CASE("map literal validates") {
  const std::string source = R"(
[return<int>]
use(x) {
  return(1i32)
}

[return<int>]
main() {
  return(use(map<i32, i32>(1i32, 2i32)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("array literal missing template arg fails") {
  const std::string source = R"(
[return<int>]
use(x) {
  return(1i32)
}

[return<int>]
main() {
  return(use(array(1i32)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("array literal requires exactly one template argument") != std::string::npos);
}

TEST_CASE("map literal missing template args fails") {
  const std::string source = R"(
[return<int>]
use(x) {
  return(1i32)
}

[return<int>]
main() {
  return(use(map(1i32, 2i32)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("map literal requires exactly two template arguments") != std::string::npos);
}

TEST_CASE("map literal requires even argument count") {
  const std::string source = R"(
[return<int>]
use(x) {
  return(1i32)
}

[return<int>]
main() {
  return(use(map<i32, i32>(1i32, 2i32, 3i32)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("map literal requires an even number of arguments") != std::string::npos);
}

TEST_CASE("boolean literal validates") {
  const std::string source = R"(
[return<int>]
main() {
  return(false)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("if statement sugar validates") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value(1i32)
  if(1i32) {
    assign(value, 2i32)
  } else {
    assign(value, 3i32)
  }
  return(value)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("return inside if block validates") {
  const std::string source = R"(
[return<int>]
main() {
  if(1i32) {
    return(2i32)
  } else {
    return(3i32)
  }
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("return not allowed in execution body") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

[return<int>]
execute_repeat(x) {
  return(x)
}

execute_repeat(1i32) { return(2i32) }
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("return not allowed in execution body") != std::string::npos);
}

TEST_SUITE_END();
