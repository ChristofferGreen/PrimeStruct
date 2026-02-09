TEST_CASE("execution effects transform validates") {
  const std::string source = R"(
[return<int>]
task([i32] x) {
  return(x)
}

[return<int>]
main() {
  return(1i32)
}

[effects(io_out)]
task(1i32)
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("execution capabilities transform validates") {
  const std::string source = R"(
[return<int>]
task([i32] x) {
  return(x)
}

[return<int>]
main() {
  return(1i32)
}

[effects(io_out), capabilities(io_out)]
task(1i32)
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("execution capabilities rejects template arguments") {
  const std::string source = R"(
[return<int>]
task([i32] x) {
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
task([i32] x) {
  return(x)
}

[return<int>]
main() {
  return(1i32)
}

[capabilities("io"utf8)]
task(1i32)
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("invalid capability") != std::string::npos);
}

TEST_CASE("execution capabilities rejects duplicate capability") {
  const std::string source = R"(
[return<int>]
task([i32] x) {
  return(x)
}

[return<int>]
main() {
  return(1i32)
}

[capabilities(io_out, io_out)]
task(1i32)
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("duplicate capability") != std::string::npos);
}

TEST_CASE("execution effects rejects template arguments") {
  const std::string source = R"(
[return<int>]
task([i32] x) {
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
task([i32] x) {
  return(x)
}

[return<int>]
main() {
  return(1i32)
}

[effects("io"utf8)]
task(1i32)
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("invalid effects capability") != std::string::npos);
}

TEST_CASE("execution effects rejects duplicate capability") {
  const std::string source = R"(
[return<int>]
task([i32] x) {
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

TEST_CASE("execution effects rejects duplicates") {
  const std::string source = R"(
[return<int>]
task([i32] x) {
  return(x)
}

[return<int>]
main() {
  return(1i32)
}

[effects(io_out), effects(asset_read)]
task(1i32)
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("duplicate effects transform") != std::string::npos);
}

TEST_CASE("execution capabilities rejects duplicates") {
  const std::string source = R"(
[return<int>]
task([i32] x) {
  return(x)
}

[return<int>]
main() {
  return(1i32)
}

[capabilities(io_out), capabilities(asset_read)]
task(1i32)
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("duplicate capabilities transform") != std::string::npos);
}

TEST_CASE("capabilities transform validates identifiers") {
  const std::string source = R"(
[effects(asset_read, gpu_queue), capabilities(asset_read, gpu_queue), return<int>]
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
[capabilities("io"utf8), return<int>]
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

TEST_CASE("pod transform rejects handle tag") {
  const std::string source = R"(
[pod, handle]
main() {
  [i32] value(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("pod definitions cannot be tagged as handle or gpu_lane") != std::string::npos);
}

TEST_CASE("pod transform rejects handle fields") {
  const std::string source = R"(
[pod]
main() {
  [handle<PathNode>] target(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("pod definitions cannot contain handle or gpu_lane fields") != std::string::npos);
}

TEST_CASE("handle and gpu_lane tags conflict on definition") {
  const std::string source = R"(
[handle, gpu_lane]
main() {
  [i32] value(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("handle definitions cannot be tagged as gpu_lane") != std::string::npos);
}

TEST_CASE("handle and gpu_lane tags conflict on field") {
  const std::string source = R"(
[struct]
main() {
  [handle<PathNode> gpu_lane] target(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("fields cannot be tagged as handle and gpu_lane") != std::string::npos);
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

TEST_CASE("placement transforms are rejected") {
  const std::string source = R"(
[stack]
main() {
  [i32] value(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("placement transforms are not supported") != std::string::npos);
}

TEST_CASE("struct definitions require field initializers") {
  const std::string source = R"(
[struct]
main() {
  [i32] value()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("struct definitions require field initializers") != std::string::npos);
}

TEST_CASE("struct definitions allow initialized fields") {
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

TEST_CASE("struct definitions reject non-binding statements") {
  const std::string source = R"(
[struct]
main() {
  [i32] value(1i32)
  plus(1i32, 2i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("struct definitions may only contain field bindings") != std::string::npos);
}

TEST_CASE("handle transform marks struct definitions") {
  const std::string source = R"(
[handle]
main() {
  [i32] value(1i32)
}

[return<int>]
use() {
  [main] item(1i32)
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/use", error));
  CHECK(error.empty());
}

TEST_CASE("gpu_lane transform marks struct definitions") {
  const std::string source = R"(
[gpu_lane]
main() {
  [i32] value(1i32)
}

[return<int>]
use() {
  [main] item(1i32)
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/use", error));
  CHECK(error.empty());
}

TEST_CASE("struct transform rejects parameters") {
  const std::string source = R"(
[struct]
main([i32] x) {
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

TEST_CASE("lifecycle helpers allow struct parents") {
  const std::string source = R"(
[struct]
thing() {
  [i32] value(1i32)
}

[return<void>]
/thing/Create() {
}

[return<void>]
/thing/Destroy() {
}

[return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("lifecycle helpers require struct tag") {
  const std::string source = R"(
thing() {
  [i32] value(1i32)
}

[return<void>]
/thing/Create() {
}

[return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("lifecycle helper must be nested inside a struct") != std::string::npos);
}

TEST_CASE("lifecycle helpers provide this") {
  const std::string source = R"(
[struct]
thing() {
  [i32] value(1i32)
}

[mut return<void>]
/thing/Create() {
  assign(this, this)
}

[return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("lifecycle helpers require mut for assignment") {
  const std::string source = R"(
[struct]
thing() {
  [i32] value(1i32)
}

[return<void>]
/thing/Create() {
  assign(this, this)
}

[return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("assign target must be a mutable binding") != std::string::npos);
}

TEST_CASE("lifecycle helpers must return void") {
  const std::string source = R"(
[struct]
thing() {
  [i32] value(1i32)
}

[return<int>]
/thing/Create() {
  return(1i32)
}

[return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("lifecycle helpers must return void") != std::string::npos);
}

TEST_CASE("lifecycle helpers reject parameters") {
  const std::string source = R"(
[struct]
thing() {
  [i32] value(1i32)
}

[return<void>]
/thing/Create([i32] x) {
}

[return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("lifecycle helpers do not accept parameters") != std::string::npos);
}

TEST_CASE("mut transform is rejected on non-helpers") {
  const std::string source = R"(
[mut return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("mut transform is only supported on lifecycle helpers") != std::string::npos);
}

TEST_CASE("lifecycle helpers reject duplicate mut") {
  const std::string source = R"(
[struct]
thing() {
  [i32] value(1i32)
}

[mut mut return<void>]
/thing/Create() {
}

[return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("duplicate mut transform on /thing/Create") != std::string::npos);
}

TEST_CASE("lifecycle helpers reject mut template arguments") {
  const std::string source = R"(
[struct]
thing() {
  [i32] value(1i32)
}

[mut<i32> return<void>]
/thing/Create() {
}

[return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("mut transform does not accept template arguments on /thing/Create") != std::string::npos);
}

TEST_CASE("lifecycle helpers reject mut arguments") {
  const std::string source = R"(
[struct]
thing() {
  [i32] value(1i32)
}

[mut(1) return<void>]
/thing/Create() {
}

[return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("mut transform does not accept arguments on /thing/Create") != std::string::npos);
}

TEST_CASE("lifecycle helpers reject non-struct parents") {
  const std::string source = R"(
[return<int>]
thing() {
  return(1i32)
}

[return<void>]
/thing/Create() {
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/thing", error));
  CHECK(error.find("lifecycle helper must be nested inside a struct") != std::string::npos);
}

TEST_CASE("this is not available outside lifecycle helpers") {
  const std::string source = R"(
[return<int>]
main() {
  return(this)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown identifier") != std::string::npos);
}

TEST_CASE("lifecycle helpers accept placement variants") {
  const std::string source = R"(
[struct]
thing() {
  [i32] value(1i32)
}

[return<void>]
/thing/CreateStack() {
}

[return<void>]
/thing/DestroyStack() {
}

[return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("struct transforms are rejected on executions") {
  const std::string source = R"(
[return<int>]
task([i32] x) {
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

TEST_CASE("mut transforms are rejected on executions") {
  const std::string source = R"(
[return<int>]
task([i32] x) {
  return(x)
}

[return<int>]
main() {
  return(1i32)
}

[mut]
task(1i32)
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("mut transform is not allowed on executions") != std::string::npos);
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
[return<bool>]
main() {
  return(greater_than(2i32, 1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("builtin comparison accepts bool operands") {
  const std::string source = R"(
[return<bool>]
main() {
  return(greater_than(true, false))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("builtin comparison accepts bool and signed int") {
  const std::string source = R"(
[return<bool>]
main() {
  return(equal(true, 1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("builtin comparison rejects bool with u64") {
  const std::string source = R"(
[return<bool>]
main() {
  return(greater_than(true, 1u64))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("mixed signed/unsigned") != std::string::npos);
}

TEST_CASE("builtin less_than calls validate") {
  const std::string source = R"(
[return<bool>]
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
[return<bool>]
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
[return<bool>]
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
[return<bool>]
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
[return<bool>]
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
[return<bool>]
main() {
  return(and(1i32, 1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("builtin and rejects float operands") {
  const std::string source = R"(
[return<bool>]
main() {
  return(and(1.5f, 1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("boolean operators require integer or bool operands") != std::string::npos);
}

TEST_CASE("builtin and rejects struct operands") {
  const std::string source = R"(
thing() {
  [i32] value(1i32)
}

[return<bool>]
main() {
  [thing] item(1i32)
  return(and(item, 1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("boolean operators require integer or bool operands") != std::string::npos);
}

TEST_CASE("builtin or calls validate") {
  const std::string source = R"(
[return<bool>]
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
[return<bool>]
main() {
  return(not(0i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("builtin comparison accepts float operands") {
  const std::string source = R"(
[return<bool>]
main() {
  return(less_than(1.5f, 2.5f))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("builtin comparison accepts string operands") {
  const std::string source = R"(
[return<bool>]
main() {
  return(equal("a"utf8, "b"utf8))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("builtin comparison rejects pointer operands") {
  const std::string source = R"(
[return<bool>]
main() {
  [i32] value(1i32)
  return(greater_than(location(value), 1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("comparisons require numeric, bool, or string operands") != std::string::npos);
}

TEST_CASE("builtin comparison rejects struct operands") {
  const std::string source = R"(
thing() {
  [i32] value(1i32)
}

[return<bool>]
main() {
  [thing] item(1i32)
  return(equal(item, item))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("comparisons require numeric, bool, or string operands") != std::string::npos);
}

TEST_CASE("builtin comparison rejects mixed signed/unsigned operands") {
  const std::string source = R"(
[return<bool>]
main() {
  return(greater_than(1i64, 2u64))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("mixed signed/unsigned") != std::string::npos);
}

TEST_CASE("builtin comparison rejects mixed int/float operands") {
  const std::string source = R"(
[return<bool>]
main() {
  return(greater_than(1i32, 2.5f))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("mixed int/float") != std::string::npos);
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

TEST_CASE("builtin clamp rejects mixed signed/unsigned operands") {
  const std::string source = R"(
[return<int>]
main() {
  return(clamp(2i64, 1u64, 5u64))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("mixed signed/unsigned") != std::string::npos);
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
[return<bool>]
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
[return<bool>]
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
[return<bool>]
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
[return<bool>]
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
[return<bool>]
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
[return<bool>]
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
