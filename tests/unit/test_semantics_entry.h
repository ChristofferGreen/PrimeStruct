
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

TEST_CASE("entry definition rejects non-arg parameter") {
  const std::string source = R"(
[return<int>]
main([i32] value) {
  return(value)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("entry definition must take a single array<string> parameter") != std::string::npos);
}

TEST_CASE("entry definition without args parameter is allowed") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("entry definition rejects default args parameter") {
  const std::string source = R"(
[return<int>]
main([array<string>] args(array<string>("hi"utf8))) {
  return(args.count())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("entry parameter does not allow a default value") != std::string::npos);
}

TEST_CASE("unknown identifier fails") {
  const std::string source = R"(
[return<int>]
main([i32] x) {
  return(y)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown identifier") != std::string::npos);
}

TEST_CASE("binding inference from expression enables method call validation") {
  const std::string source = R"(
namespace i64 {
  [return<i64>]
  inc([i64] self) {
    return(plus(self, 1i64))
  }
}

[return<i64>]
main() {
  [mut] value(plus(1i64, 2i64))
  return(value.inc())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("parameter default literal is allowed") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

[return<int>]
add([i32] left, [i32] right(10i32)) {
  return(plus(left, right))
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
add([i32] left, [i32] right(plus(left, 1i32))) {
  return(plus(left, right))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("parameter default must be a literal or pure expression") != std::string::npos);
}

TEST_CASE("parameter default expression rejects block arguments") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

[return<int>]
add([i32] left, [i32] right(block(){ 1i32 })) {
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
add([i32] left, [i32] right(block(){ [i32] temp(1i32) temp })) {
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
add([i32] left, [i32] right(add_one(value = 1i32))) {
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
add([i32] left, [i32] right(1i32, 2i32)) {
  return(plus(left, right))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("parameter defaults accept at most one argument") != std::string::npos);
}

TEST_CASE("infers parameter type from default initializer") {
  const std::string source = R"(
[return<i64>]
id([mut] value(3i64)) {
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

TEST_CASE("restrict matches inferred binding type during return inference") {
  const std::string source = R"(
main() {
  [restrict<i64>] value(1i64)
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
    [restrict<i64>] value(1i64)
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
  [i32] value(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
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
  [i32] value(1i32)
}

[return<int>]
main() {
  [thing] item(1i32)
  return(plus(item, item))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("arithmetic operators require numeric operands") != std::string::npos);
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

TEST_CASE("infers return type from builtin clamp") {
  const std::string source = R"(
main() {
  return(clamp(2i32, 1i32, 5i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("infers return type from builtin min") {
  const std::string source = R"(
main() {
  return(min(2i32, 1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("infers return type from builtin abs") {
  const std::string source = R"(
main() {
  return(abs(negate(2i32)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("infers return type from builtin saturate") {
  const std::string source = R"(
main() {
  return(saturate(2i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("clamp argument count fails") {
  const std::string source = R"(
[return<int>]
main() {
  return(clamp(2i32, 1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count") != std::string::npos);
}

TEST_CASE("min argument count fails") {
  const std::string source = R"(
[return<int>]
main() {
  return(min(2i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count") != std::string::npos);
}

TEST_CASE("abs argument count fails") {
  const std::string source = R"(
[return<int>]
main() {
  return(abs(2i32, 3i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count") != std::string::npos);
}

TEST_CASE("saturate argument count fails") {
  const std::string source = R"(
[return<int>]
main() {
  return(saturate(2i32, 3i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count") != std::string::npos);
}

TEST_CASE("clamp rejects mixed int/float operands") {
  const std::string source = R"(
[return<int>]
main() {
  return(clamp(1i32, 0.5f, 2i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("mixed int/float") != std::string::npos);
}

TEST_CASE("min rejects mixed int/float operands") {
  const std::string source = R"(
[return<int>]
main() {
  return(min(1i32, 0.5f))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("mixed int/float") != std::string::npos);
}

TEST_CASE("max rejects mixed signed/unsigned operands") {
  const std::string source = R"(
[return<int>]
main() {
  return(max(2i64, 1u64))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("mixed signed/unsigned") != std::string::npos);
}

TEST_CASE("sign rejects non-numeric operand") {
  const std::string source = R"(
[return<int>]
main() {
  return(sign(true))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("numeric") != std::string::npos);
}

TEST_CASE("saturate rejects non-numeric operand") {
  const std::string source = R"(
[return<int>]
main() {
  return(saturate(true))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("numeric") != std::string::npos);
}

TEST_CASE("assign through non-mut pointer fails") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value(1i32)
  [Pointer<i32>] ptr(location(value))
  assign(dereference(ptr), 3i32)
  return(value)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("assign target must be a mutable binding") != std::string::npos);
}

TEST_CASE("assign through non-pointer dereference fails") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value(1i32)
  assign(dereference(value), 3i32)
  return(value)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("assign target must be a mutable pointer binding") != std::string::npos);
}

TEST_CASE("not argument count fails") {
  const std::string source = R"(
[return<int>]
main() {
  return(not(true, false))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  const bool hasCountMismatch = error.find("argument count mismatch") != std::string::npos;
  const bool hasExactArg = error.find("requires exactly one argument") != std::string::npos;
  if (!hasCountMismatch) {
    CHECK(hasExactArg);
  }
}

TEST_CASE("convert requires template argument") {
  const std::string source = R"(
[return<int>]
main() {
  return(convert(1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("template") != std::string::npos);
}

TEST_CASE("infers void return type without transform") {
  const std::string source = R"(
main() {
  return()
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("argument count mismatch fails") {
  const std::string source = R"(
[return<int>]
callee([i32] x) {
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
task([i32] x) {
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
execute_repeat([i32] x) {
  return(x)
}

execute_repeat(3i32) { 1i32 }
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("execution body arguments must be calls") != std::string::npos);
}

TEST_CASE("execution body arguments cannot be bindings") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

[return<int>]
execute_repeat([i32] x) {
  return(x)
}

execute_repeat(3i32) { [i32] value(1i32) }
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("execution body arguments cannot be bindings") != std::string::npos);
}

TEST_CASE("execution body rejects nested bindings") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

[return<void>]
execute_repeat([i32] x) {
  return()
}

execute_repeat(3i32) { if(true, then(){ [i32] value(2i32) }, else(){ }) }
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("binding not allowed in execution body") != std::string::npos);
}

TEST_CASE("execution body rejects nested non-call expressions") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

[return<void>]
execute_repeat([i32] x) {
  return()
}

execute_repeat(3i32) { if(true, then(){ 1i32 }, else(){ main() }) }
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("execution body arguments must be calls") != std::string::npos);
}

TEST_CASE("execution body accepts if statement sugar") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

[return<void>]
execute_repeat([i32] x) {
  return()
}

execute_repeat(3i32) { if(true) { main() } else { main() } }
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("execution rejects alignment transforms") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

[return<void>]
execute_repeat([i32] x) {
  return()
}

[align_bytes(16)]
execute_repeat(3i32) { main() }
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("alignment transforms are not supported on executions") != std::string::npos);
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

TEST_CASE("duplicate return transform fails") {
  const std::string source = R"(
[return<int>, return<i32>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("duplicate return transform") != std::string::npos);
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

TEST_CASE("return type mismatch fails") {
  const std::string source = R"(
[return<int>]
main() {
  return(true)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("return type mismatch") != std::string::npos);
}

TEST_CASE("return type mismatch for bool fails") {
  const std::string source = R"(
[return<bool>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("return type mismatch") != std::string::npos);
}

TEST_CASE("return transform rejects arguments") {
  const std::string source = R"(
[return<int>(foo)]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("return transform does not accept arguments") != std::string::npos);
}

TEST_CASE("effects transform validates identifiers") {
  const std::string source = R"(
[effects(global_write, io_out), return<int>]
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
[effects("io"utf8), return<int>]
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
[effects(io_out, io_out), return<int>]
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
[effects(render_graph, io_out), capabilities(render_graph, io_out), return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("capabilities require matching effects") {
  const std::string source = R"(
[effects(io_out), capabilities(io_err), return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("capability requires matching effect") != std::string::npos);
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
[capabilities(io_out, io_out), return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("duplicate capability") != std::string::npos);
}

TEST_CASE("effects transform rejects duplicates") {
  const std::string source = R"(
[effects(io_out), effects(asset_read), return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("duplicate effects transform") != std::string::npos);
}

TEST_CASE("capabilities transform rejects duplicates") {
  const std::string source = R"(
[capabilities(io_out), capabilities(asset_read), return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("duplicate capabilities transform") != std::string::npos);
}
