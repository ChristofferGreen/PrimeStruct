#include <string>

TEST_SUITE_BEGIN("primestruct.ir.pipeline.to_glsl");

namespace {

primec::IrModule makeSimpleI32AddModule() {
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

TEST_CASE("ir to glsl emitter writes numeric stack program") {
  primec::IrToGlslEmitter emitter;
  const primec::IrModule module = makeSimpleI32AddModule();
  std::string glsl;
  std::string error;
  REQUIRE(emitter.emitSource(module, glsl, error));
  CHECK(error.empty());
  CHECK(glsl.find("#version 450") != std::string::npos);
  CHECK(glsl.find("layout(std430, binding = 0) buffer PrimeStructOutput") != std::string::npos);
  CHECK(glsl.find("int ps_entry_0()") != std::string::npos);
  CHECK(glsl.find("switch (pc)") != std::string::npos);
  CHECK(glsl.find("case 0") != std::string::npos);
  CHECK(glsl.find("stack[sp++] = 2;") != std::string::npos);
  CHECK(glsl.find("stack[sp++] = 3;") != std::string::npos);
  CHECK(glsl.find("stack[sp++] = left + right;") != std::string::npos);
  CHECK(glsl.find("return stack[--sp];") != std::string::npos);
}

TEST_CASE("ir to glsl emitter writes jump and conditional jump control flow") {
  primec::IrToGlslEmitter emitter;
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

  std::string glsl;
  std::string error;
  REQUIRE(emitter.emitSource(module, glsl, error));
  CHECK(error.empty());
  CHECK(glsl.find("pc = (cond == 0) ? 4 : 2;") != std::string::npos);
  CHECK(glsl.find("pc = 5;") != std::string::npos);
}

TEST_CASE("ir to glsl emitter writes f32 literal push") {
  primec::IrToGlslEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::PushF32, 0x3fc00000u});
  fn.instructions.push_back({primec::IrOpcode::StoreLocal, 0});
  fn.instructions.push_back({primec::IrOpcode::ReturnVoid, 0});
  module.functions.push_back(fn);

  std::string glsl;
  std::string error;
  REQUIRE(emitter.emitSource(module, glsl, error));
  CHECK(error.empty());
  CHECK(glsl.find("stack[sp++] = int(uint(1069547520u));") != std::string::npos);
}

TEST_CASE("ir to glsl emitter rejects unsupported opcodes") {
  primec::IrToGlslEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::PushF64, 0});
  fn.instructions.push_back({primec::IrOpcode::ReturnF64, 0});
  module.functions.push_back(fn);

  std::string glsl;
  std::string error;
  CHECK_FALSE(emitter.emitSource(module, glsl, error));
  CHECK(error.find("unsupported opcode") != std::string::npos);
}

TEST_CASE("ir to glsl emitter rejects invalid entry index") {
  primec::IrToGlslEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 1;
  module.functions.push_back({});

  std::string glsl;
  std::string error;
  CHECK_FALSE(emitter.emitSource(module, glsl, error));
  CHECK(error == "IrToGlslEmitter invalid IR entry index");
}

TEST_CASE("ir to glsl emitter rejects out-of-range local indices") {
  primec::IrToGlslEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::PushI32, 1});
  fn.instructions.push_back({primec::IrOpcode::StoreLocal, 2048});
  fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(fn);

  std::string glsl;
  std::string error;
  CHECK_FALSE(emitter.emitSource(module, glsl, error));
  CHECK(error.find("local index out of range") != std::string::npos);
}

TEST_SUITE_END();
