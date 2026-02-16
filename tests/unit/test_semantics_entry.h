TEST_SUITE_BEGIN("primestruct.semantics.entry");

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

TEST_CASE("entry definition accepts array<string> parameter") {
  const std::string source = R"(
[return<int>]
main([array<string>] argv) {
  return(argv.count())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("entry definition rejects default args parameter") {
  const std::string source = R"(
[return<int>]
main([array<string>] args{array<string>("hi"utf8)}) {
  return(args.count())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("entry parameter does not allow a default value") != std::string::npos);
}

TEST_CASE("entry default effects enable io_out") {
  const std::string source = R"(
[return<int>]
main() {
  print_line("hello"utf8)
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgramWithDefaults(source, "/main", {}, {"io_out"}, error));
  CHECK(error.empty());
}

TEST_CASE("non-entry defaults remain pure") {
  const std::string source = R"(
[return<int>]
main() {
  log()
  return(1i32)
}

[return<void>]
log() {
  print_line("hello"utf8)
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgramWithDefaults(source, "/main", {}, {"io_out"}, error));
  CHECK(error.find("print_line requires io_out effect") != std::string::npos);
}


TEST_SUITE_END();

TEST_SUITE_BEGIN("primestruct.semantics.resolution");

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
  [mut] value{plus(1i64, 2i64)}
  return(value.inc())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("namespace blocks may be reopened") {
  const std::string source = R"(
namespace demo {
  [return<int>]
  first() {
    return(2i32)
  }
}

namespace demo {
  [return<int>]
  second() {
    return(3i32)
  }
}

[return<int>]
main() {
  return(plus(/demo/first(), /demo/second()))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_SUITE_END();

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

TEST_SUITE_END();

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

TEST_SUITE_END();

TEST_SUITE_BEGIN("primestruct.semantics.operators");

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
  [i32] value{1i32}
}

[return<int>]
main() {
  [thing] item{1i32}
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

TEST_SUITE_END();

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

TEST_SUITE_BEGIN("primestruct.semantics.tags");

TEST_CASE("pod and handle tags conflict on definitions") {
  const std::string source = R"(
[pod handle]
Thing() {
  [i32] value{1i32}
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("pod definitions cannot be tagged as handle or gpu_lane") != std::string::npos);
}

TEST_CASE("pod definitions reject handle fields") {
  const std::string source = R"(
[pod]
Thing() {
  [handle i32] value{1i32}
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("pod definitions cannot contain handle or gpu_lane fields") != std::string::npos);
}

TEST_CASE("pod and gpu_lane tags conflict on definitions") {
  const std::string source = R"(
[pod gpu_lane]
Thing() {
  [i32] value{1i32}
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("pod definitions cannot be tagged as handle or gpu_lane") != std::string::npos);
}

TEST_CASE("handle and gpu_lane tags conflict on definitions") {
  const std::string source = R"(
[handle gpu_lane]
Thing() {
  [i32] value{1i32}
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("handle definitions cannot be tagged as gpu_lane") != std::string::npos);
}

TEST_CASE("fields reject handle and gpu_lane together") {
  const std::string source = R"(
[struct]
Thing() {
  [handle gpu_lane i32] value{1i32}
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("fields cannot be tagged as handle and gpu_lane") != std::string::npos);
}

TEST_CASE("fields reject pod and handle together") {
  const std::string source = R"(
[struct]
Thing() {
  [pod handle i32] value{1i32}
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("fields cannot be tagged as pod and handle") != std::string::npos);
}

TEST_CASE("fields reject pod and gpu_lane together") {
  const std::string source = R"(
[struct]
Thing() {
  [pod gpu_lane i32] value{1i32}
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("fields cannot be tagged as pod and gpu_lane") != std::string::npos);
}

TEST_CASE("handle tag does not replace binding type") {
  const std::string source = R"(
[return<int>]
main() {
  [handle i32] value{1i32}
  return(value)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("handle tag accepts template arg without changing type") {
  const std::string source = R"(
[return<int>]
main() {
  [handle<PathNode> i32] value{1i32}
  return(value)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("pod tag rejects template args on bindings") {
  const std::string source = R"(
[return<int>]
main() {
  [pod<i32> i32] value{1i32}
  return(value)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("binding transforms do not take template arguments") != std::string::npos);
}

TEST_CASE("duplicate static tags reject on bindings") {
  const std::string source = R"(
[return<int>]
main() {
  [static static i32] value{1i32}
  return(value)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("duplicate static transform on binding") != std::string::npos);
}

TEST_CASE("duplicate visibility tags reject on bindings") {
  const std::string source = R"(
[return<int>]
main() {
  [public private i32] value{1i32}
  return(value)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("binding visibility transforms are mutually exclusive") != std::string::npos);
}

TEST_CASE("visibility tags reject on definitions") {
  const std::string source = R"(
[public return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("binding visibility/static transforms are only valid on bindings") != std::string::npos);
}

TEST_CASE("static tag rejects on definitions") {
  const std::string source = R"(
[static return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("binding visibility/static transforms are only valid on bindings") != std::string::npos);
}

TEST_CASE("copy tag rejects on definitions") {
  const std::string source = R"(
[copy return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("copy transform is only supported on bindings and parameters") != std::string::npos);
}

TEST_CASE("restrict tag rejects on definitions") {
  const std::string source = R"(
[restrict<i32> return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("restrict transform is only supported on bindings and parameters") != std::string::npos);
}

TEST_SUITE_END();

TEST_SUITE_BEGIN("primestruct.semantics.builtins_numeric");

TEST_CASE("infers return type from builtin clamp") {
  const std::string source = R"(
import /math/*
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
import /math/*
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
import /math/*
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
import /math/*
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
import /math/*
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
import /math/*
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
import /math/*
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
import /math/*
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
import /math/*
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
import /math/*
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
import /math/*
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
import /math/*
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
import /math/*
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
  [i32 mut] value{1i32}
  [Pointer<i32>] ptr{location(value)}
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
  [i32 mut] value{1i32}
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

TEST_CASE("boolean operators accept bool operands") {
  const std::string source = R"(
[return<bool>]
main() {
  return(and(false, true))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("boolean operators reject integer operands") {
  const std::string source = R"(
[return<bool>]
main() {
  return(or(0i32, 1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("boolean operators require bool operands") != std::string::npos);
}

TEST_CASE("boolean operators reject unsigned operands") {
  const std::string source = R"(
[return<bool>]
main() {
  return(and(0u64, 1u64))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("boolean operators require bool operands") != std::string::npos);
}

TEST_CASE("not accepts bool operand") {
  const std::string source = R"(
[return<bool>]
main() {
  return(not(false))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("boolean operators reject float operands") {
  const std::string source = R"(
[return<bool>]
main() {
  return(and(1.0f, true))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("boolean operators require bool operands") != std::string::npos);
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

TEST_SUITE_END();

TEST_SUITE_BEGIN("primestruct.semantics.executions");

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

TEST_CASE("execution accepts bracket-labeled arguments") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

[return<int>]
execute_repeat([i32] count) {
  return(count)
}

execute_repeat([count] 2i32)
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("execution rejects unknown named argument") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

[return<int>]
execute_repeat([i32] count) {
  return(count)
}

execute_repeat([missing] 2i32)
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown named argument: missing") != std::string::npos);
}

TEST_CASE("execution rejects duplicate named argument") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

[return<int>]
execute_repeat([i32] count) {
  return(count)
}

execute_repeat([count] 2i32, [count] 3i32)
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("duplicate named argument: count") != std::string::npos);
}

TEST_CASE("execution body accepts literal forms") {
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
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("execution body accepts bindings") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

[return<int>]
execute_repeat([i32] x) {
  return(x)
}

execute_repeat(3i32) { [i32] value{1i32} }
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("execution body accepts nested bindings") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

[return<void>]
execute_repeat([i32] x) {
  return()
}

execute_repeat(3i32) { if(true, then(){ [i32] value{2i32} }, else(){ }) }
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("execution body accepts nested literals") {
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
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
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

TEST_CASE("execution body accepts multiple calls") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

[return<void>]
execute_repeat([i32] x) {
  return()
}

execute_repeat(2i32) { main(), main() }
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("execution body accepts whitespace-separated calls") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

[return<void>]
execute_repeat([i32] x) {
  return()
}

execute_repeat(2i32) { main() main() }
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("execution body accepts empty list") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

[return<void>]
execute_repeat([i32] x) {
  return()
}

execute_repeat(2i32) { }
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("execution rejects copy transform") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

[return<void>]
execute_repeat([i32] x) {
  return()
}

[copy]
execute_repeat(2i32) { main() }
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("copy transform is not allowed on executions") != std::string::npos);
}

TEST_CASE("execution rejects mut transform") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

[return<void>]
execute_repeat([i32] x) {
  return()
}

[mut]
execute_repeat(2i32) { main() }
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("mut transform is not allowed on executions") != std::string::npos);
}

TEST_CASE("execution rejects restrict transform") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

[return<void>]
execute_repeat([i32] x) {
  return()
}

[restrict]
execute_repeat(2i32) { main() }
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("restrict transform is not allowed on executions") != std::string::npos);
}

TEST_CASE("execution rejects placement transforms") {
  const char *placements[] = {"stack", "heap", "buffer"};
  for (const auto *placement : placements) {
    CAPTURE(placement);
    const std::string source =
        std::string("[return<int>]\n") +
        "main() {\n"
        "  return(1i32)\n"
        "}\n"
        "\n"
        "[return<void>]\n"
        "execute_repeat([i32] x) {\n"
        "  return()\n"
        "}\n"
        "\n"
        "[" + placement + "]\n"
        "execute_repeat(2i32) { main() }\n";
    std::string error;
    CHECK_FALSE(validateProgram(source, "/main", error));
    CHECK(error.find("placement transforms are not supported") != std::string::npos);
  }
}

TEST_CASE("execution rejects duplicate effects transforms") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

[return<void>]
execute_repeat([i32] x) {
  return()
}

[effects(io_out) effects(asset_read)]
execute_repeat(2i32) { main() }
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("duplicate effects transform on /execute_repeat") != std::string::npos);
}

TEST_CASE("execution rejects duplicate capabilities transforms") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

[return<void>]
execute_repeat([i32] x) {
  return()
}

[capabilities(io_out) capabilities(asset_read)]
execute_repeat(2i32) { main() }
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("duplicate capabilities transform on /execute_repeat") != std::string::npos);
}

TEST_CASE("execution rejects invalid effects capability") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

[return<void>]
execute_repeat([i32] x) {
  return()
}

[effects("io"utf8)]
execute_repeat(2i32) { main() }
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("invalid effects capability") != std::string::npos);
}

TEST_CASE("execution rejects invalid capability name") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

[return<void>]
execute_repeat([i32] x) {
  return()
}

[capabilities("io"utf8)]
execute_repeat(2i32) { main() }
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("invalid capability") != std::string::npos);
}

TEST_CASE("execution rejects duplicate effects capability") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

[return<void>]
execute_repeat([i32] x) {
  return()
}

[effects(io_out, io_out)]
execute_repeat(2i32) { main() }
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("duplicate effects capability") != std::string::npos);
}

TEST_CASE("execution rejects duplicate capability") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

[return<void>]
execute_repeat([i32] x) {
  return()
}

[capabilities(io_out, io_out)]
execute_repeat(2i32) { main() }
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("duplicate capability") != std::string::npos);
}

TEST_CASE("execution rejects effects template arguments") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

[return<void>]
execute_repeat([i32] x) {
  return()
}

[effects<io_out>]
execute_repeat(2i32) { main() }
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("effects transform does not accept template arguments on /execute_repeat") != std::string::npos);
}

TEST_CASE("execution rejects capabilities template arguments") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

[return<void>]
execute_repeat([i32] x) {
  return()
}

[capabilities<io_out>]
execute_repeat(2i32) { main() }
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("capabilities transform does not accept template arguments on /execute_repeat") != std::string::npos);
}

TEST_CASE("execution capabilities require matching effects") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

[return<void>]
execute_repeat([i32] x) {
  return()
}

[effects(io_out) capabilities(io_err)]
execute_repeat(2i32) { main() }
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("capability requires matching effect on /execute_repeat") != std::string::npos);
}

TEST_CASE("execution accepts effects and capabilities") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

[return<void>]
execute_repeat([i32] x) {
  return()
}

[effects(io_out) capabilities(io_out)]
execute_repeat(2i32) { main() }
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("execution rejects struct transform") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

[return<void>]
execute_repeat([i32] x) {
  return()
}

[struct]
execute_repeat(2i32) { main() }
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("struct transforms are not allowed on executions") != std::string::npos);
}

TEST_CASE("execution rejects visibility transform") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

[return<void>]
execute_repeat([i32] x) {
  return()
}

[public]
execute_repeat(2i32) { main() }
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("binding visibility/static transforms are only valid on bindings") != std::string::npos);
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

TEST_SUITE_END();

TEST_SUITE_BEGIN("primestruct.semantics.transforms");

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

TEST_CASE("software numeric return type fails") {
  const std::string source = R"(
[return<complex>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("software numeric types are not supported yet") != std::string::npos);
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

TEST_CASE("single_type_to_return requires a type transform") {
  const std::string source = R"(
[single_type_to_return]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("single_type_to_return requires a type transform") != std::string::npos);
}

TEST_CASE("single_type_to_return rejects multiple type transforms") {
  const std::string source = R"(
[single_type_to_return i32 i64]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("single_type_to_return requires a single type transform") != std::string::npos);
}

TEST_CASE("single_type_to_return rejects duplicate markers") {
  const std::string source = R"(
[single_type_to_return single_type_to_return i32]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("duplicate single_type_to_return transform") != std::string::npos);
}

TEST_CASE("single_type_to_return rejects return transform combo") {
  const std::string source = R"(
[single_type_to_return return<i32>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("single_type_to_return cannot be combined with return transform") != std::string::npos);
}

TEST_CASE("single_type_to_return rejects template args") {
  const std::string source = R"(
[single_type_to_return<i32> i32]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("single_type_to_return does not accept template arguments") != std::string::npos);
}

TEST_CASE("single_type_to_return rejects arguments") {
  const std::string source = R"(
[single_type_to_return(1i32) i32]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("single_type_to_return does not accept arguments") != std::string::npos);
}

TEST_CASE("single_type_to_return enforces non-void return") {
  const std::string source = R"(
[single_type_to_return i32]
main() {
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("missing return statement") != std::string::npos);
}

TEST_CASE("text group rejects non-text transform") {
  const std::string source = R"(
[text(return<int>)]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("text(...) group requires text transforms") != std::string::npos);
}

TEST_CASE("semantic group rejects text transform") {
  const std::string source = R"(
[semantic(operators) return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("text transform cannot appear in semantic(...) group") != std::string::npos);
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

TEST_CASE("int alias maps to i32") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i64)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("return type mismatch: expected i32") != std::string::npos);
}

TEST_CASE("float alias maps to f32") {
  const std::string source = R"(
[return<float>]
main() {
  return(1.0f64)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("return type mismatch: expected f32") != std::string::npos);
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

TEST_SUITE_END();
