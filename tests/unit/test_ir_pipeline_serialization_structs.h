TEST_SUITE_BEGIN("primestruct.ir.pipeline.serialization");


TEST_CASE("ir serialize roundtrip and vm execution") {
  const std::string source = R"(
[return<int>]
main() {
  return(plus(1i32, 2i32))
}
)";
  std::string error;
  primec::IrModule module;
  REQUIRE(parseValidateAndLower(source, module, error));
  CHECK(error.empty());
  REQUIRE(module.functions.size() == 1);
  CHECK(module.functions[0].instructions.size() == 4);
  CHECK(module.functions[0].instructions[0].op == primec::IrOpcode::PushI32);
  CHECK(module.functions[0].instructions[1].op == primec::IrOpcode::PushI32);
  CHECK(module.functions[0].instructions[2].op == primec::IrOpcode::AddI32);
  CHECK(module.functions[0].instructions[3].op == primec::IrOpcode::ReturnI32);

  std::vector<uint8_t> data;
  REQUIRE(primec::serializeIr(module, data, error));
  CHECK(error.empty());

  primec::IrModule decoded;
  REQUIRE(primec::deserializeIr(data, decoded, error));
  CHECK(error.empty());
  REQUIRE(decoded.functions.size() == 1);
  CHECK(decoded.functions[0].instructions.size() == 4);

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(decoded, result, error));
  CHECK(error.empty());
  CHECK(result == 3);
}

TEST_CASE("ir lowers semicolon-separated imports") {
  const std::string source = R"(
import /util; /std/math/*
namespace util {
  [public return<int>]
  inc([i32] value) {
    return(plus(value, 1i32))
  }
}
[return<int>]
main() {
  return(min(inc(2i32), 4i32))
}
)";
  std::string error;
  primec::IrModule module;
  REQUIRE(parseValidateAndLower(source, module, error));
  CHECK(error.empty());
}

TEST_CASE("ir lowers struct brace constructor binding") {
  const std::string source = R"(
[struct]
thing() {
  [i32] value{1i32}
  [i32] count{2i32}
}

[return<int>]
main() {
  [thing] item{thing{[count] 3i32}}
  return(item.count)
}
)";
  std::string error;
  primec::IrModule module;
  INFO(error);
  REQUIRE(parseValidateAndLower(source, module, error));
  CHECK(error.empty());

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(result == 3);
}

TEST_CASE("ir lowers struct return values") {
  const std::string source = R"(
[struct]
Point() {
  [i32] x{1i32}
  [i32] y{2i32}
}

[return<Point>]
make_point([i32] x, [i32] y) {
  return(Point{[x] x, [y] y})
}

[return<int>]
main() {
  [Point] value{make_point(3i32, 4i32)}
  return(1i32)
}
)";
  std::string error;
  primec::IrModule module;
  REQUIRE(parseValidateAndLower(source, module, error));
  CHECK(error.empty());
}

TEST_CASE("ir lowers assign from struct-return function call") {
  const std::string source = R"(
[struct]
Vec3() {
  [i32] x{0i32}
  [i32] y{0i32}
  [i32] z{0i32}
}

[return<Vec3>]
makeVec3([i32] x, [i32] y, [i32] z) {
  return(Vec3{[x] x, [y] y, [z] z})
}

[return<int>]
main() {
  [Vec3 mut] sample{Vec3{}}
  assign(sample, makeVec3(4i32, 5i32, 6i32))
  return(sample.z)
}
)";
  std::string error;
  primec::IrModule module;
  REQUIRE(parseValidateAndLower(source, module, error));

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(result == 6);
}

TEST_CASE("semantics reports assign diagnostic for immutable struct local") {
  const std::string source = R"(
[struct]
Vec3() {
  [i32] x{0i32}
  [i32] y{0i32}
  [i32] z{0i32}
}

[return<Vec3>]
makeVec3([i32] x, [i32] y, [i32] z) {
  return(Vec3{[x] x, [y] y, [z] z})
}

[return<int>]
main() {
  [Vec3] sample{Vec3{}}
  assign(sample, makeVec3(4i32, 5i32, 6i32))
  return(0i32)
}
)";
  primec::Program program;
  std::string error;
  CHECK_FALSE(parseAndValidate(source, program, error));
  CHECK(error == "assign target must be a mutable binding: sample");
}


TEST_CASE("ir serialize roundtrip with implicit void return") {
  const std::string source = R"(
[return<void>]
main() {
  [i32] value{2i32}
}
)";
  std::string error;
  primec::IrModule module;
  REQUIRE(parseValidateAndLower(source, module, error));
  CHECK(error.empty());
  REQUIRE(module.functions.size() == 1);
  REQUIRE(module.functions[0].instructions.size() == 3);
  CHECK(module.functions[0].instructions[2].op == primec::IrOpcode::ReturnVoid);

  std::vector<uint8_t> data;
  REQUIRE(primec::serializeIr(module, data, error));
  CHECK(error.empty());

  primec::IrModule decoded;
  REQUIRE(primec::deserializeIr(data, decoded, error));
  CHECK(error.empty());
  REQUIRE(decoded.functions.size() == 1);
  REQUIRE(decoded.functions[0].instructions.size() == 3);
  CHECK(decoded.functions[0].instructions[2].op == primec::IrOpcode::ReturnVoid);

  primec::Vm vm;
  uint64_t result = 7;
  REQUIRE(vm.execute(decoded, result, error));
  CHECK(error.empty());
  CHECK(result == 0);
}

TEST_CASE("ir lowers literal statement with pop") {
  const std::string source = R"(
[return<int>]
main() {
  1i32
  return(2i32)
}
)";
  std::string error;
  primec::IrModule module;
  REQUIRE(parseValidateAndLower(source, module, error));
  CHECK(error.empty());
  REQUIRE(module.functions.size() == 1);
  const auto &inst = module.functions[0].instructions;
  REQUIRE(inst.size() == 4);
  CHECK(inst[0].op == primec::IrOpcode::PushI32);
  CHECK(inst[1].op == primec::IrOpcode::Pop);
  CHECK(inst[2].op == primec::IrOpcode::PushI32);
  CHECK(inst[3].op == primec::IrOpcode::ReturnI32);
}

TEST_CASE("ir lowers i64 arithmetic") {
  const std::string source = R"(
[return<i64>]
main() {
  return(plus(1i64, 2i64))
}
)";
  std::string error;
  primec::IrModule module;
  REQUIRE(parseValidateAndLower(source, module, error));
  CHECK(error.empty());
  REQUIRE(module.functions.size() == 1);
  const auto &inst = module.functions[0].instructions;
  REQUIRE(inst.size() == 4);
  CHECK(inst[0].op == primec::IrOpcode::PushI64);
  CHECK(inst[1].op == primec::IrOpcode::PushI64);
  CHECK(inst[2].op == primec::IrOpcode::AddI64);
  CHECK(inst[3].op == primec::IrOpcode::ReturnI64);

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 3);
}

TEST_CASE("ir lowers u64 comparison") {
  const std::string source = R"(
[return<bool>]
main() {
  return(greater_than(0xFFFFFFFFFFFFFFFFu64, 1u64))
}
)";
  std::string error;
  primec::IrModule module;
  REQUIRE(parseValidateAndLower(source, module, error));
  CHECK(error.empty());

  bool sawCmp = false;
  for (const auto &inst : module.functions[0].instructions) {
    if (inst.op == primec::IrOpcode::CmpGtU64) {
      sawCmp = true;
      break;
    }
  }
  CHECK(sawCmp);

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 1);
}

TEST_CASE("ir lowers locals and assign") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{2i32}
  assign(value, plus(value, 3i32))
  return(value)
}
)";
  std::string error;
  primec::IrModule module;
  REQUIRE(parseValidateAndLower(source, module, error));
  CHECK(error.empty());
  const auto &inst = module.functions[0].instructions;
  REQUIRE(inst.size() == 10);
  CHECK(inst[0].op == primec::IrOpcode::PushI32);
  CHECK(inst[1].op == primec::IrOpcode::StoreLocal);
  CHECK(inst[2].op == primec::IrOpcode::LoadLocal);
  CHECK(inst[3].op == primec::IrOpcode::PushI32);
  CHECK(inst[4].op == primec::IrOpcode::AddI32);
  CHECK(inst[5].op == primec::IrOpcode::Dup);
  CHECK(inst[6].op == primec::IrOpcode::StoreLocal);
  CHECK(inst[7].op == primec::IrOpcode::Pop);
  CHECK(inst[8].op == primec::IrOpcode::LoadLocal);
  CHECK(inst[9].op == primec::IrOpcode::ReturnI32);

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 5);
}

TEST_CASE("ir lowers assign in return expression") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{1i32}
  return(assign(value, 4i32))
}
)";
  std::string error;
  primec::IrModule module;
  REQUIRE(parseValidateAndLower(source, module, error));
  CHECK(error.empty());
  const auto &inst = module.functions[0].instructions;
  REQUIRE(inst.size() == 6);
  CHECK(inst[0].op == primec::IrOpcode::PushI32);
  CHECK(inst[1].op == primec::IrOpcode::StoreLocal);
  CHECK(inst[2].op == primec::IrOpcode::PushI32);
  CHECK(inst[3].op == primec::IrOpcode::Dup);
  CHECK(inst[4].op == primec::IrOpcode::StoreLocal);
  CHECK(inst[5].op == primec::IrOpcode::ReturnI32);

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 4);
}

TEST_CASE("ir lowers increment in return expression") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{1i32}
  return(increment(value))
}
)";
  std::string error;
  primec::IrModule module;
  REQUIRE(parseValidateAndLower(source, module, error));
  CHECK(error.empty());
  const auto &inst = module.functions[0].instructions;
  REQUIRE(inst.size() == 8);
  CHECK(inst[0].op == primec::IrOpcode::PushI32);
  CHECK(inst[1].op == primec::IrOpcode::StoreLocal);
  CHECK(inst[2].op == primec::IrOpcode::LoadLocal);
  CHECK(inst[3].op == primec::IrOpcode::PushI32);
  CHECK(inst[4].op == primec::IrOpcode::AddI32);
  CHECK(inst[5].op == primec::IrOpcode::Dup);
  CHECK(inst[6].op == primec::IrOpcode::StoreLocal);
  CHECK(inst[7].op == primec::IrOpcode::ReturnI32);

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 2);
}

TEST_CASE("ir lowers decrement in return expression") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{3i32}
  return(decrement(value))
}
)";
  std::string error;
  primec::IrModule module;
  REQUIRE(parseValidateAndLower(source, module, error));
  CHECK(error.empty());
  const auto &inst = module.functions[0].instructions;
  REQUIRE(inst.size() == 8);
  CHECK(inst[0].op == primec::IrOpcode::PushI32);
  CHECK(inst[1].op == primec::IrOpcode::StoreLocal);
  CHECK(inst[2].op == primec::IrOpcode::LoadLocal);
  CHECK(inst[3].op == primec::IrOpcode::PushI32);
  CHECK(inst[4].op == primec::IrOpcode::SubI32);
  CHECK(inst[5].op == primec::IrOpcode::Dup);
  CHECK(inst[6].op == primec::IrOpcode::StoreLocal);
  CHECK(inst[7].op == primec::IrOpcode::ReturnI32);

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 2);
}

#include "test_ir_pipeline_serialization_struct_layouts.h"

TEST_SUITE_END();
