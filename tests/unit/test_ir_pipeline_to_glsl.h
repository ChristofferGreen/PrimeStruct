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
  CHECK(glsl.find("int ps_entry_0(inout int stack[1024], inout int sp)") != std::string::npos);
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

TEST_CASE("ir to glsl emitter writes call and callvoid opcodes") {
  primec::IrToGlslEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;

  primec::IrFunction entry;
  entry.name = "/main";
  entry.instructions.push_back({primec::IrOpcode::Call, 1});
  entry.instructions.push_back({primec::IrOpcode::CallVoid, 2});
  entry.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(entry);

  primec::IrFunction callee;
  callee.name = "/helper";
  callee.instructions.push_back({primec::IrOpcode::PushI32, 7});
  callee.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(callee);

  primec::IrFunction voidCallee;
  voidCallee.name = "/side_effect";
  voidCallee.instructions.push_back({primec::IrOpcode::ReturnVoid, 0});
  module.functions.push_back(voidCallee);

  std::string glsl;
  std::string error;
  REQUIRE(emitter.emitSource(module, glsl, error));
  CHECK(error.empty());
  CHECK(glsl.find("int ps_entry_1(inout int stack[1024], inout int sp);") != std::string::npos);
  CHECK(glsl.find("int ps_entry_2(inout int stack[1024], inout int sp);") != std::string::npos);
  CHECK(glsl.find("stack[sp++] = ps_entry_1(stack, sp);") != std::string::npos);
  CHECK(glsl.find("ps_entry_2(stack, sp);") != std::string::npos);
}

TEST_CASE("ir to glsl emitter writes pushargc opcode") {
  primec::IrToGlslEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::PushArgc, 0});
  fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(fn);

  std::string glsl;
  std::string error;
  REQUIRE(emitter.emitSource(module, glsl, error));
  CHECK(error.empty());
  CHECK(glsl.find("stack[sp++] = 0; // GLSL backend has no argv") != std::string::npos);
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

TEST_CASE("ir to glsl emitter writes f32 arithmetic and compare opcodes") {
  primec::IrToGlslEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::PushF32, 0x40000000u});
  fn.instructions.push_back({primec::IrOpcode::PushF32, 0x3f800000u});
  fn.instructions.push_back({primec::IrOpcode::SubF32, 0});
  fn.instructions.push_back({primec::IrOpcode::NegF32, 0});
  fn.instructions.push_back({primec::IrOpcode::PushF32, 0u});
  fn.instructions.push_back({primec::IrOpcode::CmpLtF32, 0});
  fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(fn);

  std::string glsl;
  std::string error;
  REQUIRE(emitter.emitSource(module, glsl, error));
  CHECK(error.empty());
  CHECK(glsl.find("float right = intBitsToFloat(stack[--sp]);") != std::string::npos);
  CHECK(glsl.find("stack[sp++] = floatBitsToInt(left - right);") != std::string::npos);
  CHECK(glsl.find("stack[sp - 1] = floatBitsToInt(-value);") != std::string::npos);
  CHECK(glsl.find("stack[sp++] = (left < right) ? 1 : 0;") != std::string::npos);
}

TEST_CASE("ir to glsl emitter writes f32 to i32 conversion opcode") {
  primec::IrToGlslEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::PushF32, 0x3fc00000u});
  fn.instructions.push_back({primec::IrOpcode::ConvertF32ToI32, 0});
  fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(fn);

  std::string glsl;
  std::string error;
  REQUIRE(emitter.emitSource(module, glsl, error));
  CHECK(error.empty());
  CHECK(glsl.find("float value = intBitsToFloat(stack[--sp]);") != std::string::npos);
  CHECK(glsl.find("if (isnan(value))") != std::string::npos);
  CHECK(glsl.find("converted = 2147483647;") != std::string::npos);
  CHECK(glsl.find("converted = -2147483647 - 1;") != std::string::npos);
  CHECK(glsl.find("stack[sp++] = converted;") != std::string::npos);
}

TEST_CASE("ir to glsl emitter writes i32 to f32 conversion opcode") {
  primec::IrToGlslEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::PushI32, 5});
  fn.instructions.push_back({primec::IrOpcode::ConvertI32ToF32, 0});
  fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(fn);

  std::string glsl;
  std::string error;
  REQUIRE(emitter.emitSource(module, glsl, error));
  CHECK(error.empty());
  CHECK(glsl.find("stack[sp++] = floatBitsToInt(float(stack[--sp]));") != std::string::npos);
}

TEST_CASE("ir to glsl emitter writes f32 return opcode") {
  primec::IrToGlslEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::PushF32, 0x3fc00000u});
  fn.instructions.push_back({primec::IrOpcode::ReturnF32, 0});
  module.functions.push_back(fn);

  std::string glsl;
  std::string error;
  REQUIRE(emitter.emitSource(module, glsl, error));
  CHECK(error.empty());
  CHECK(glsl.find("return stack[--sp];") != std::string::npos);
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

TEST_CASE("ir to glsl emitter rejects out-of-range call targets") {
  primec::IrToGlslEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::Call, 9});
  fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(fn);

  std::string glsl;
  std::string error;
  CHECK_FALSE(emitter.emitSource(module, glsl, error));
  CHECK(error.find("call target out of range") != std::string::npos);
}

TEST_SUITE_END();
