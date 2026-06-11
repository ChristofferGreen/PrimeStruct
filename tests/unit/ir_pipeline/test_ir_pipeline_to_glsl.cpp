#include <cstdint>
#include <string>

#include "third_party/doctest.h"

#include "primec/Ir.h"
#include "primec/IrToGlslEmitter.h"

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

TEST_CASE("ir to glsl emitter writes print opcode no-op handling") {
  primec::IrToGlslEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  module.stringTable.push_back("hello");
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back(
      {primec::IrOpcode::PrintString, primec::encodePrintStringImm(0, primec::encodePrintFlags(true, false))});
  fn.instructions.push_back({primec::IrOpcode::PushI32, 1});
  fn.instructions.push_back({primec::IrOpcode::PrintI32, 0});
  fn.instructions.push_back({primec::IrOpcode::PushI32, 2});
  fn.instructions.push_back({primec::IrOpcode::PrintI64, 0});
  fn.instructions.push_back({primec::IrOpcode::PushI32, 3});
  fn.instructions.push_back({primec::IrOpcode::PrintU64, 0});
  fn.instructions.push_back({primec::IrOpcode::PushI32, 4});
  fn.instructions.push_back({primec::IrOpcode::PrintStringDynamic, 0});
  fn.instructions.push_back({primec::IrOpcode::PushI32, 5});
  fn.instructions.push_back({primec::IrOpcode::PrintArgv, 0});
  fn.instructions.push_back({primec::IrOpcode::PushI32, 6});
  fn.instructions.push_back({primec::IrOpcode::PrintArgvUnsafe, 0});
  fn.instructions.push_back({primec::IrOpcode::PushI32, 9});
  fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(fn);

  std::string glsl;
  std::string error;
  REQUIRE(emitter.emitSource(module, glsl, error));
  CHECK(error.empty());
  CHECK(glsl.find("// GLSL backend ignores print-string side effects.") != std::string::npos);
  CHECK(glsl.find("--sp; // GLSL backend ignores print side effects.") != std::string::npos);
}

TEST_CASE("ir to glsl emitter writes loadstringbyte opcode") {
  primec::IrToGlslEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  module.stringTable.push_back("Az");
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::PushI32, 0});
  fn.instructions.push_back({primec::IrOpcode::LoadStringByte, 0});
  fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(fn);

  std::string glsl;
  std::string error;
  REQUIRE(emitter.emitSource(module, glsl, error));
  CHECK(error.empty());
  CHECK(glsl.find("switch (stringByteIndex)") != std::string::npos);
  CHECK(glsl.find("case 0u: stringByte = 65; break;") != std::string::npos);
  CHECK(glsl.find("case 1u: stringByte = 122; break;") != std::string::npos);
  CHECK(glsl.find("default: stringByte = 0; break;") != std::string::npos);
}

TEST_CASE("ir to glsl emitter writes pushi64 opcode when value fits i32") {
  primec::IrToGlslEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::PushI64, static_cast<uint64_t>(static_cast<int64_t>(42))});
  fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(fn);

  std::string glsl;
  std::string error;
  REQUIRE(emitter.emitSource(module, glsl, error));
  CHECK(error.empty());
  CHECK(glsl.find("stack[sp++] = 42;") != std::string::npos);
}

TEST_CASE("ir to glsl emitter writes i64 compare and return opcodes") {
  primec::IrToGlslEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::PushI64, static_cast<uint64_t>(static_cast<int64_t>(2))});
  fn.instructions.push_back({primec::IrOpcode::PushI64, static_cast<uint64_t>(static_cast<int64_t>(3))});
  fn.instructions.push_back({primec::IrOpcode::CmpLtI64, 0});
  fn.instructions.push_back({primec::IrOpcode::ReturnI64, 0});
  module.functions.push_back(fn);

  std::string glsl;
  std::string error;
  REQUIRE(emitter.emitSource(module, glsl, error));
  CHECK(error.empty());
  CHECK(glsl.find("stack[sp++] = (left < right) ? 1 : 0;") != std::string::npos);
  CHECK(glsl.find("return stack[--sp];") != std::string::npos);
}

TEST_CASE("ir to glsl emitter writes i64 arithmetic opcodes") {
  primec::IrToGlslEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::PushI64, static_cast<uint64_t>(static_cast<int64_t>(9))});
  fn.instructions.push_back({primec::IrOpcode::PushI64, static_cast<uint64_t>(static_cast<int64_t>(4))});
  fn.instructions.push_back({primec::IrOpcode::SubI64, 0});
  fn.instructions.push_back({primec::IrOpcode::PushI64, static_cast<uint64_t>(static_cast<int64_t>(2))});
  fn.instructions.push_back({primec::IrOpcode::MulI64, 0});
  fn.instructions.push_back({primec::IrOpcode::PushI64, static_cast<uint64_t>(static_cast<int64_t>(5))});
  fn.instructions.push_back({primec::IrOpcode::AddI64, 0});
  fn.instructions.push_back({primec::IrOpcode::PushI64, static_cast<uint64_t>(static_cast<int64_t>(3))});
  fn.instructions.push_back({primec::IrOpcode::DivI64, 0});
  fn.instructions.push_back({primec::IrOpcode::NegI64, 0});
  fn.instructions.push_back({primec::IrOpcode::ReturnI64, 0});
  module.functions.push_back(fn);

  std::string glsl;
  std::string error;
  REQUIRE(emitter.emitSource(module, glsl, error));
  CHECK(error.empty());
  CHECK(glsl.find("stack[sp++] = left - right;") != std::string::npos);
  CHECK(glsl.find("stack[sp++] = left * right;") != std::string::npos);
  CHECK(glsl.find("stack[sp++] = left + right;") != std::string::npos);
  CHECK(glsl.find("stack[sp++] = left / right;") != std::string::npos);
  CHECK(glsl.find("stack[sp - 1] = -stack[sp - 1];") != std::string::npos);
}

TEST_CASE("ir to glsl emitter writes u64 compare opcodes") {
  primec::IrToGlslEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::PushI64, static_cast<uint64_t>(static_cast<int64_t>(7))});
  fn.instructions.push_back({primec::IrOpcode::PushI64, static_cast<uint64_t>(static_cast<int64_t>(3))});
  fn.instructions.push_back({primec::IrOpcode::CmpGtU64, 0});
  fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(fn);

  std::string glsl;
  std::string error;
  REQUIRE(emitter.emitSource(module, glsl, error));
  CHECK(error.empty());
  CHECK(glsl.find("uint right = uint(stack[--sp]);") != std::string::npos);
  CHECK(glsl.find("uint left = uint(stack[--sp]);") != std::string::npos);
  CHECK(glsl.find("stack[sp++] = (left > right) ? 1 : 0;") != std::string::npos);
}

TEST_CASE("ir to glsl emitter writes u64 division opcode") {
  primec::IrToGlslEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::PushI64, static_cast<uint64_t>(static_cast<int64_t>(12))});
  fn.instructions.push_back({primec::IrOpcode::PushI64, static_cast<uint64_t>(static_cast<int64_t>(5))});
  fn.instructions.push_back({primec::IrOpcode::DivU64, 0});
  fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(fn);

  std::string glsl;
  std::string error;
  REQUIRE(emitter.emitSource(module, glsl, error));
  CHECK(error.empty());
  CHECK(glsl.find("uint right = uint(stack[--sp]);") != std::string::npos);
  CHECK(glsl.find("uint left = uint(stack[--sp]);") != std::string::npos);
  CHECK(glsl.find("stack[sp++] = int(left / right);") != std::string::npos);
}

TEST_CASE("ir to glsl emitter writes i64/u64 to f32 conversion opcodes") {
  primec::IrToGlslEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::PushI64, static_cast<uint64_t>(static_cast<int64_t>(6))});
  fn.instructions.push_back({primec::IrOpcode::ConvertI64ToF32, 0});
  fn.instructions.push_back({primec::IrOpcode::Pop, 0});
  fn.instructions.push_back({primec::IrOpcode::PushI64, static_cast<uint64_t>(static_cast<int64_t>(7))});
  fn.instructions.push_back({primec::IrOpcode::ConvertU64ToF32, 0});
  fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(fn);

  std::string glsl;
  std::string error;
  REQUIRE(emitter.emitSource(module, glsl, error));
  CHECK(error.empty());
  CHECK(glsl.find("stack[sp++] = floatBitsToInt(float(stack[--sp]));") != std::string::npos);
  CHECK(glsl.find("stack[sp++] = floatBitsToInt(float(uint(stack[--sp])));") != std::string::npos);
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

TEST_CASE("ir to glsl emitter writes f32 to i64 conversion opcode") {
  primec::IrToGlslEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::PushF32, 0x40400000u});
  fn.instructions.push_back({primec::IrOpcode::ConvertF32ToI64, 0});
  fn.instructions.push_back({primec::IrOpcode::ReturnI64, 0});
  module.functions.push_back(fn);

  std::string glsl;
  std::string error;
  REQUIRE(emitter.emitSource(module, glsl, error));
  CHECK(error.empty());
  CHECK(glsl.find("float value = intBitsToFloat(stack[--sp]);") != std::string::npos);
  CHECK(glsl.find("converted = int(value);") != std::string::npos);
  CHECK(glsl.find("stack[sp++] = converted;") != std::string::npos);
}

TEST_CASE("ir to glsl emitter writes f32 to u64 conversion opcode") {
  primec::IrToGlslEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::PushF32, 0x41200000u});
  fn.instructions.push_back({primec::IrOpcode::ConvertF32ToU64, 0});
  fn.instructions.push_back({primec::IrOpcode::ReturnI64, 0});
  module.functions.push_back(fn);

  std::string glsl;
  std::string error;
  REQUIRE(emitter.emitSource(module, glsl, error));
  CHECK(error.empty());
  CHECK(glsl.find("uint converted = 0u;") != std::string::npos);
  CHECK(glsl.find("converted = uint(int(value));") != std::string::npos);
  CHECK(glsl.find("stack[sp++] = int(converted);") != std::string::npos);
}

TEST_CASE("ir to glsl emitter writes f32/f64 passthrough conversion opcodes") {
  primec::IrToGlslEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::PushF32, 0x3f800000u});
  fn.instructions.push_back({primec::IrOpcode::ConvertF32ToF64, 0});
  fn.instructions.push_back({primec::IrOpcode::ConvertF64ToF32, 0});
  fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(fn);

  std::string glsl;
  std::string error;
  REQUIRE(emitter.emitSource(module, glsl, error));
  CHECK(error.empty());
  CHECK(glsl.find("// Narrowed GLSL path keeps f32/f64 conversion as bit-preserving passthrough.") !=
        std::string::npos);
  CHECK(glsl.find("// Narrowed GLSL path keeps f64/f32 conversion as bit-preserving passthrough.") !=
        std::string::npos);
}

TEST_CASE("ir to glsl emitter writes i32/f64 narrowed conversion opcodes") {
  primec::IrToGlslEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::PushI32, 11});
  fn.instructions.push_back({primec::IrOpcode::ConvertI32ToF64, 0});
  fn.instructions.push_back({primec::IrOpcode::ConvertF64ToI32, 0});
  fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(fn);

  std::string glsl;
  std::string error;
  REQUIRE(emitter.emitSource(module, glsl, error));
  CHECK(error.empty());
  CHECK(glsl.find("// Narrowed GLSL path lowers i32/f64 conversion through f32 payloads.") != std::string::npos);
  CHECK(glsl.find("// Narrowed GLSL path lowers f64/i32 conversion through f32 payloads.") != std::string::npos);
}

TEST_CASE("ir to glsl emitter writes i64/u64 to f64 narrowed conversion opcodes") {
  primec::IrToGlslEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::PushI64, static_cast<uint64_t>(static_cast<int64_t>(8))});
  fn.instructions.push_back({primec::IrOpcode::ConvertI64ToF64, 0});
  fn.instructions.push_back({primec::IrOpcode::Pop, 0});
  fn.instructions.push_back({primec::IrOpcode::PushI64, static_cast<uint64_t>(static_cast<int64_t>(9))});
  fn.instructions.push_back({primec::IrOpcode::ConvertU64ToF64, 0});
  fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(fn);

  std::string glsl;
  std::string error;
  REQUIRE(emitter.emitSource(module, glsl, error));
  CHECK(error.empty());
  CHECK(glsl.find("// Narrowed GLSL path lowers i64/f64 conversion through f32 payloads.") != std::string::npos);
  CHECK(glsl.find("// Narrowed GLSL path lowers u64/f64 conversion through f32 payloads.") != std::string::npos);
}

TEST_CASE("ir to glsl emitter writes f64 to i64/u64 narrowed conversion opcodes") {
  primec::IrToGlslEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::PushF32, 0x40c00000u});
  fn.instructions.push_back({primec::IrOpcode::ConvertF32ToF64, 0});
  fn.instructions.push_back({primec::IrOpcode::ConvertF64ToI64, 0});
  fn.instructions.push_back({primec::IrOpcode::Pop, 0});
  fn.instructions.push_back({primec::IrOpcode::PushF32, 0x41100000u});
  fn.instructions.push_back({primec::IrOpcode::ConvertF32ToF64, 0});
  fn.instructions.push_back({primec::IrOpcode::ConvertF64ToU64, 0});
  fn.instructions.push_back({primec::IrOpcode::ReturnI64, 0});
  module.functions.push_back(fn);

  std::string glsl;
  std::string error;
  REQUIRE(emitter.emitSource(module, glsl, error));
  CHECK(error.empty());
  CHECK(glsl.find("// Narrowed GLSL path lowers f64/i64 conversion through f32 payloads.") != std::string::npos);
  CHECK(glsl.find("// Narrowed GLSL path lowers f64/u64 conversion through f32 payloads.") != std::string::npos);
  CHECK(glsl.find("uint converted = 0u;") != std::string::npos);
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

TEST_SUITE_END();
