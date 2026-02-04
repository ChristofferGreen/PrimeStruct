#include "third_party/doctest.h"

#include "primec/IrLowerer.h"
#include "primec/IrSerializer.h"
#include "primec/Lexer.h"
#include "primec/Parser.h"
#include "primec/Semantics.h"
#include "primec/Vm.h"

namespace {
bool parseAndValidate(const std::string &source, primec::Program &program, std::string &error) {
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  if (!parser.parse(program, error)) {
    return false;
  }
  primec::Semantics semantics;
  return semantics.validate(program, "/main", error);
}
} // namespace

TEST_SUITE_BEGIN("primestruct.ir.pipeline");

TEST_CASE("ir serialize roundtrip and vm execution") {
  const std::string source = R"(
[return<int>]
main() {
  return(plus(1i32, 2i32))
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", module, error));
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
  int32_t result = 0;
  REQUIRE(vm.execute(decoded, result, error));
  CHECK(error.empty());
  CHECK(result == 3);
}

TEST_CASE("ir lowers locals and assign") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value(2i32)
  assign(value, plus(value, 3i32))
  return(value)
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", module, error));
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
  int32_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 5);
}

TEST_CASE("ir lowerer rejects unsupported statements") {
  const std::string source = R"(
[return<int>]
main() {
  plus(1i32, 2i32)
  return(1i32)
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  CHECK_FALSE(lowerer.lower(program, "/main", module, error));
  CHECK(error.find("native backend only supports return or assign statements") != std::string::npos);
}

TEST_SUITE_END();
