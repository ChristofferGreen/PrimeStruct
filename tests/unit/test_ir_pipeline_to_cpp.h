#include <string>

TEST_SUITE_BEGIN("primestruct.ir.pipeline.to_cpp");

namespace {

primec::IrModule makeSimpleI32AddModuleForCpp() {
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::PushI32, 2});
  fn.instructions.push_back({primec::IrOpcode::PushI32, 3});
  fn.instructions.push_back({primec::IrOpcode::AddI32, 0});
  fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(fn);
  return module;
}

} // namespace

TEST_CASE("ir to cpp emitter writes numeric stack program") {
  primec::IrToCppEmitter emitter;
  const primec::IrModule module = makeSimpleI32AddModuleForCpp();
  std::string cpp;
  std::string error;
  REQUIRE(emitter.emitSource(module, cpp, error));
  CHECK(error.empty());
  CHECK(cpp.find("static int32_t ps_entry_0()") != std::string::npos);
  CHECK(cpp.find("switch (pc)") != std::string::npos);
  CHECK(cpp.find("case 0") != std::string::npos);
  CHECK(cpp.find("stack[sp++] = 2;") != std::string::npos);
  CHECK(cpp.find("stack[sp++] = 3;") != std::string::npos);
  CHECK(cpp.find("stack[sp++] = left + right;") != std::string::npos);
  CHECK(cpp.find("return stack[--sp];") != std::string::npos);
  CHECK(cpp.find("int main()") != std::string::npos);
}

TEST_CASE("ir to cpp emitter writes jump and conditional jump control flow") {
  primec::IrToCppEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::PushI32, 0});
  fn.instructions.push_back({primec::IrOpcode::JumpIfZero, 4});
  fn.instructions.push_back({primec::IrOpcode::PushI32, 99});
  fn.instructions.push_back({primec::IrOpcode::Jump, 5});
  fn.instructions.push_back({primec::IrOpcode::PushI32, 7});
  fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(fn);

  std::string cpp;
  std::string error;
  REQUIRE(emitter.emitSource(module, cpp, error));
  CHECK(error.empty());
  CHECK(cpp.find("pc = (cond == 0) ? 4 : 2;") != std::string::npos);
  CHECK(cpp.find("pc = 5;") != std::string::npos);
}

TEST_CASE("ir to cpp emitter rejects unsupported opcodes") {
  primec::IrToCppEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::PushF32, 0});
  fn.instructions.push_back({primec::IrOpcode::ReturnF32, 0});
  module.functions.push_back(fn);

  std::string cpp;
  std::string error;
  CHECK_FALSE(emitter.emitSource(module, cpp, error));
  CHECK(error.find("unsupported opcode") != std::string::npos);
}

TEST_CASE("ir to cpp emitter rejects invalid entry index") {
  primec::IrToCppEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 1;
  module.functions.push_back({});

  std::string cpp;
  std::string error;
  CHECK_FALSE(emitter.emitSource(module, cpp, error));
  CHECK(error == "IrToCppEmitter invalid IR entry index");
}

TEST_CASE("ir to cpp emitter rejects out-of-range local indices") {
  primec::IrToCppEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::PushI32, 1});
  fn.instructions.push_back({primec::IrOpcode::StoreLocal, 2048});
  fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(fn);

  std::string cpp;
  std::string error;
  CHECK_FALSE(emitter.emitSource(module, cpp, error));
  CHECK(error.find("local index out of range") != std::string::npos);
}

TEST_CASE("ir to cpp emitter rejects out-of-range jump targets") {
  primec::IrToCppEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::Jump, 99});
  fn.instructions.push_back({primec::IrOpcode::ReturnVoid, 0});
  module.functions.push_back(fn);

  std::string cpp;
  std::string error;
  CHECK_FALSE(emitter.emitSource(module, cpp, error));
  CHECK(error.find("jump target out of range") != std::string::npos);
}

TEST_SUITE_END();
