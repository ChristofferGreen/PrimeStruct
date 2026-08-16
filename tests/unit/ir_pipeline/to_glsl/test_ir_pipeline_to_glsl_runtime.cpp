#include <cstdint>
#include <string>

#include "third_party/doctest.h"

#include "primec/Ir.h"
#include "primec/IrToGlslEmitter.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.to_glsl");

namespace {

constexpr uint64_t encodeSignedI32Imm(int32_t value) {
  return static_cast<uint64_t>(static_cast<int64_t>(value));
}

} // namespace

TEST_CASE("ir to glsl emitter writes narrowed f64 literal and return opcodes") {
  primec::IrToGlslEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::PushF64, 0x3ff8000000000000ull});
  fn.instructions.push_back({primec::IrOpcode::ReturnF64, 0});
  module.functions.push_back(fn);

  std::string glsl;
  std::string error;
  REQUIRE(emitter.emitSource(module, glsl, error));
  CHECK(error.empty());
  CHECK(glsl.find("// Narrowed GLSL path lowers f64 literals through f32 payloads.") != std::string::npos);
  CHECK(glsl.find("stack[sp++] = int(uint(1069547520u));") != std::string::npos);
  CHECK(glsl.find("// Narrowed GLSL path returns f64 values through f32 payloads.") != std::string::npos);
}

TEST_CASE("ir to glsl emitter writes narrowed f64 add opcode") {
  primec::IrToGlslEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::PushF64, 0x3ff8000000000000ull});
  fn.instructions.push_back({primec::IrOpcode::PushF64, 0x4000000000000000ull});
  fn.instructions.push_back({primec::IrOpcode::AddF64, 0});
  fn.instructions.push_back({primec::IrOpcode::ReturnF64, 0});
  module.functions.push_back(fn);

  std::string glsl;
  std::string error;
  REQUIRE(emitter.emitSource(module, glsl, error));
  CHECK(error.empty());
  CHECK(glsl.find("// Narrowed GLSL path lowers f64 add through f32 payloads.") != std::string::npos);
  CHECK(glsl.find("stack[sp++] = floatBitsToInt(left + right);") != std::string::npos);
}

TEST_CASE("ir to glsl emitter writes narrowed f64 sub opcode") {
  primec::IrToGlslEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::PushF64, 0x4004000000000000ull});
  fn.instructions.push_back({primec::IrOpcode::PushF64, 0x3ff8000000000000ull});
  fn.instructions.push_back({primec::IrOpcode::SubF64, 0});
  fn.instructions.push_back({primec::IrOpcode::ReturnF64, 0});
  module.functions.push_back(fn);

  std::string glsl;
  std::string error;
  REQUIRE(emitter.emitSource(module, glsl, error));
  CHECK(error.empty());
  CHECK(glsl.find("// Narrowed GLSL path lowers f64 sub through f32 payloads.") != std::string::npos);
  CHECK(glsl.find("stack[sp++] = floatBitsToInt(left - right);") != std::string::npos);
}

TEST_CASE("ir to glsl emitter writes narrowed f64 mul opcode") {
  primec::IrToGlslEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::PushF64, 0x3ff8000000000000ull});
  fn.instructions.push_back({primec::IrOpcode::PushF64, 0x4010000000000000ull});
  fn.instructions.push_back({primec::IrOpcode::MulF64, 0});
  fn.instructions.push_back({primec::IrOpcode::ReturnF64, 0});
  module.functions.push_back(fn);

  std::string glsl;
  std::string error;
  REQUIRE(emitter.emitSource(module, glsl, error));
  CHECK(error.empty());
  CHECK(glsl.find("// Narrowed GLSL path lowers f64 mul through f32 payloads.") != std::string::npos);
  CHECK(glsl.find("stack[sp++] = floatBitsToInt(left * right);") != std::string::npos);
}

TEST_CASE("ir to glsl emitter writes narrowed f64 div opcode") {
  primec::IrToGlslEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::PushF64, 0x4014000000000000ull});
  fn.instructions.push_back({primec::IrOpcode::PushF64, 0x4000000000000000ull});
  fn.instructions.push_back({primec::IrOpcode::DivF64, 0});
  fn.instructions.push_back({primec::IrOpcode::ReturnF64, 0});
  module.functions.push_back(fn);

  std::string glsl;
  std::string error;
  REQUIRE(emitter.emitSource(module, glsl, error));
  CHECK(error.empty());
  CHECK(glsl.find("// Narrowed GLSL path lowers f64 div through f32 payloads.") != std::string::npos);
  CHECK(glsl.find("stack[sp++] = floatBitsToInt(left / right);") != std::string::npos);
}

TEST_CASE("ir to glsl emitter writes narrowed f64 neg opcode") {
  primec::IrToGlslEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::PushF64, 0x3ff8000000000000ull});
  fn.instructions.push_back({primec::IrOpcode::NegF64, 0});
  fn.instructions.push_back({primec::IrOpcode::ReturnF64, 0});
  module.functions.push_back(fn);

  std::string glsl;
  std::string error;
  REQUIRE(emitter.emitSource(module, glsl, error));
  CHECK(error.empty());
  CHECK(glsl.find("// Narrowed GLSL path lowers f64 neg through f32 payloads.") != std::string::npos);
  CHECK(glsl.find("stack[sp - 1] = floatBitsToInt(-value);") != std::string::npos);
}

TEST_CASE("ir to glsl emitter writes narrowed f64 equality compare opcode") {
  primec::IrToGlslEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::PushF64, 0x3ff8000000000000ull});
  fn.instructions.push_back({primec::IrOpcode::PushF64, 0x3ff8000000000000ull});
  fn.instructions.push_back({primec::IrOpcode::CmpEqF64, 0});
  fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(fn);

  std::string glsl;
  std::string error;
  REQUIRE(emitter.emitSource(module, glsl, error));
  CHECK(error.empty());
  CHECK(glsl.find("// Narrowed GLSL path lowers f64 equality compare through f32 payloads.") != std::string::npos);
  CHECK(glsl.find("stack[sp++] = (left == right) ? 1 : 0;") != std::string::npos);
}

TEST_CASE("ir to glsl emitter writes narrowed f64 inequality compare opcode") {
  primec::IrToGlslEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::PushF64, 0x3ff8000000000000ull});
  fn.instructions.push_back({primec::IrOpcode::PushF64, 0x4000000000000000ull});
  fn.instructions.push_back({primec::IrOpcode::CmpNeF64, 0});
  fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(fn);

  std::string glsl;
  std::string error;
  REQUIRE(emitter.emitSource(module, glsl, error));
  CHECK(error.empty());
  CHECK(glsl.find("// Narrowed GLSL path lowers f64 inequality compare through f32 payloads.") != std::string::npos);
  CHECK(glsl.find("stack[sp++] = (left != right) ? 1 : 0;") != std::string::npos);
}

TEST_CASE("ir to glsl emitter writes narrowed f64 less-than compare opcode") {
  primec::IrToGlslEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::PushF64, 0x3ff8000000000000ull});
  fn.instructions.push_back({primec::IrOpcode::PushF64, 0x4000000000000000ull});
  fn.instructions.push_back({primec::IrOpcode::CmpLtF64, 0});
  fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(fn);

  std::string glsl;
  std::string error;
  REQUIRE(emitter.emitSource(module, glsl, error));
  CHECK(error.empty());
  CHECK(glsl.find("// Narrowed GLSL path lowers f64 less-than compare through f32 payloads.") != std::string::npos);
  CHECK(glsl.find("stack[sp++] = (left < right) ? 1 : 0;") != std::string::npos);
}

TEST_CASE("ir to glsl emitter writes narrowed f64 less-or-equal compare opcode") {
  primec::IrToGlslEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::PushF64, 0x3ff8000000000000ull});
  fn.instructions.push_back({primec::IrOpcode::PushF64, 0x3ff8000000000000ull});
  fn.instructions.push_back({primec::IrOpcode::CmpLeF64, 0});
  fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(fn);

  std::string glsl;
  std::string error;
  REQUIRE(emitter.emitSource(module, glsl, error));
  CHECK(error.empty());
  CHECK(glsl.find("// Narrowed GLSL path lowers f64 less-or-equal compare through f32 payloads.") != std::string::npos);
  CHECK(glsl.find("stack[sp++] = (left <= right) ? 1 : 0;") != std::string::npos);
}

TEST_CASE("ir to glsl emitter writes narrowed f64 greater-than compare opcode") {
  primec::IrToGlslEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::PushF64, 0x4000000000000000ull});
  fn.instructions.push_back({primec::IrOpcode::PushF64, 0x3ff8000000000000ull});
  fn.instructions.push_back({primec::IrOpcode::CmpGtF64, 0});
  fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(fn);

  std::string glsl;
  std::string error;
  REQUIRE(emitter.emitSource(module, glsl, error));
  CHECK(error.empty());
  CHECK(glsl.find("// Narrowed GLSL path lowers f64 greater-than compare through f32 payloads.") != std::string::npos);
  CHECK(glsl.find("stack[sp++] = (left > right) ? 1 : 0;") != std::string::npos);
}

TEST_CASE("ir to glsl emitter writes narrowed f64 greater-or-equal compare opcode") {
  primec::IrToGlslEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::PushF64, 0x4000000000000000ull});
  fn.instructions.push_back({primec::IrOpcode::PushF64, 0x4000000000000000ull});
  fn.instructions.push_back({primec::IrOpcode::CmpGeF64, 0});
  fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(fn);

  std::string glsl;
  std::string error;
  REQUIRE(emitter.emitSource(module, glsl, error));
  CHECK(error.empty());
  CHECK(glsl.find("// Narrowed GLSL path lowers f64 greater-or-equal compare through f32 payloads.") !=
        std::string::npos);
  CHECK(glsl.find("stack[sp++] = (left >= right) ? 1 : 0;") != std::string::npos);
}

TEST_CASE("ir to glsl emitter writes file-open-read stub opcode") {
  primec::IrToGlslEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  module.stringTable.push_back("/dev/null");
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::FileOpenRead, 0});
  fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(fn);

  std::string glsl;
  std::string error;
  REQUIRE(emitter.emitSource(module, glsl, error));
  CHECK(error.empty());
  CHECK(glsl.find("// GLSL backend cannot open files; push deterministic invalid file handle.") != std::string::npos);
  CHECK(glsl.find("stack[sp++] = -1;") != std::string::npos);
}

TEST_CASE("ir to glsl emitter writes file-open-write stub opcode") {
  primec::IrToGlslEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  module.stringTable.push_back("/tmp/out.txt");
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::FileOpenWrite, 0});
  fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(fn);

  std::string glsl;
  std::string error;
  REQUIRE(emitter.emitSource(module, glsl, error));
  CHECK(error.empty());
  CHECK(glsl.find("// GLSL backend cannot open files; push deterministic invalid file handle.") != std::string::npos);
  CHECK(glsl.find("stack[sp++] = -1;") != std::string::npos);
}

TEST_CASE("ir to glsl emitter writes file-open-append stub opcode") {
  primec::IrToGlslEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  module.stringTable.push_back("/tmp/out.txt");
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::FileOpenAppend, 0});
  fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(fn);

  std::string glsl;
  std::string error;
  REQUIRE(emitter.emitSource(module, glsl, error));
  CHECK(error.empty());
  CHECK(glsl.find("// GLSL backend cannot open files; push deterministic invalid file handle.") != std::string::npos);
  CHECK(glsl.find("stack[sp++] = -1;") != std::string::npos);
}

TEST_CASE("ir to glsl emitter writes file-close stub opcode") {
  primec::IrToGlslEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::PushI32, encodeSignedI32Imm(-1)});
  fn.instructions.push_back({primec::IrOpcode::FileClose, 0});
  fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(fn);

  std::string glsl;
  std::string error;
  REQUIRE(emitter.emitSource(module, glsl, error));
  CHECK(error.empty());
  CHECK(glsl.find("// GLSL backend cannot close files; replace handle with deterministic success code.") !=
        std::string::npos);
  CHECK(glsl.find("stack[sp - 1] = 0;") != std::string::npos);
}

TEST_CASE("ir to glsl emitter writes file-flush stub opcode") {
  primec::IrToGlslEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::PushI32, encodeSignedI32Imm(-1)});
  fn.instructions.push_back({primec::IrOpcode::FileFlush, 0});
  fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(fn);

  std::string glsl;
  std::string error;
  REQUIRE(emitter.emitSource(module, glsl, error));
  CHECK(error.empty());
  CHECK(glsl.find("// GLSL backend cannot flush files; replace handle with deterministic success code.") !=
        std::string::npos);
  CHECK(glsl.find("stack[sp - 1] = 0;") != std::string::npos);
}

TEST_CASE("ir to glsl emitter writes file-write-i32 stub opcode") {
  primec::IrToGlslEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::PushI32, encodeSignedI32Imm(-1)});
  fn.instructions.push_back({primec::IrOpcode::PushI32, 7});
  fn.instructions.push_back({primec::IrOpcode::FileWriteI32, 0});
  fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(fn);

  std::string glsl;
  std::string error;
  REQUIRE(emitter.emitSource(module, glsl, error));
  CHECK(error.empty());
  CHECK(glsl.find("// GLSL backend cannot write files; consume value/handle and push deterministic success code.") !=
        std::string::npos);
  CHECK(glsl.find("sp -= 2;") != std::string::npos);
  CHECK(glsl.find("stack[sp++] = 0;") != std::string::npos);
}

TEST_CASE("ir to glsl emitter writes file-write-i64 stub opcode") {
  primec::IrToGlslEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::PushI32, encodeSignedI32Imm(-1)});
  fn.instructions.push_back({primec::IrOpcode::PushI64, 7});
  fn.instructions.push_back({primec::IrOpcode::FileWriteI64, 0});
  fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(fn);

  std::string glsl;
  std::string error;
  REQUIRE(emitter.emitSource(module, glsl, error));
  CHECK(error.empty());
  CHECK(glsl.find("// GLSL backend cannot write files; consume value/handle and push deterministic success code.") !=
        std::string::npos);
  CHECK(glsl.find("sp -= 2;") != std::string::npos);
  CHECK(glsl.find("stack[sp++] = 0;") != std::string::npos);
}

TEST_CASE("ir to glsl emitter writes file-write-u64 stub opcode") {
  primec::IrToGlslEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::PushI32, encodeSignedI32Imm(-1)});
  fn.instructions.push_back({primec::IrOpcode::PushI64, 7});
  fn.instructions.push_back({primec::IrOpcode::FileWriteU64, 0});
  fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(fn);

  std::string glsl;
  std::string error;
  REQUIRE(emitter.emitSource(module, glsl, error));
  CHECK(error.empty());
  CHECK(glsl.find("// GLSL backend cannot write files; consume value/handle and push deterministic success code.") !=
        std::string::npos);
  CHECK(glsl.find("sp -= 2;") != std::string::npos);
  CHECK(glsl.find("stack[sp++] = 0;") != std::string::npos);
}

TEST_CASE("ir to glsl emitter writes file-write-string stub opcode") {
  primec::IrToGlslEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  module.stringTable.push_back("hello");
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::PushI32, encodeSignedI32Imm(-1)});
  fn.instructions.push_back({primec::IrOpcode::FileWriteString, 0});
  fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(fn);

  std::string glsl;
  std::string error;
  REQUIRE(emitter.emitSource(module, glsl, error));
  CHECK(error.empty());
  CHECK(glsl.find("// GLSL backend cannot write files; replace handle with deterministic success code.") !=
        std::string::npos);
  CHECK(glsl.find("stack[sp - 1] = 0;") != std::string::npos);
}

TEST_CASE("ir to glsl emitter writes file-write-byte stub opcode") {
  primec::IrToGlslEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::PushI32, encodeSignedI32Imm(-1)});
  fn.instructions.push_back({primec::IrOpcode::PushI32, 65});
  fn.instructions.push_back({primec::IrOpcode::FileWriteByte, 0});
  fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(fn);

  std::string glsl;
  std::string error;
  REQUIRE(emitter.emitSource(module, glsl, error));
  CHECK(error.empty());
  CHECK(glsl.find("// GLSL backend cannot write files; consume byte/handle and push deterministic success code.") !=
        std::string::npos);
  CHECK(glsl.find("sp -= 2;") != std::string::npos);
  CHECK(glsl.find("stack[sp++] = 0;") != std::string::npos);
}

TEST_CASE("ir to glsl emitter writes file-write-newline stub opcode") {
  primec::IrToGlslEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::PushI32, encodeSignedI32Imm(-1)});
  fn.instructions.push_back({primec::IrOpcode::FileWriteNewline, 0});
  fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(fn);

  std::string glsl;
  std::string error;
  REQUIRE(emitter.emitSource(module, glsl, error));
  CHECK(error.empty());
  CHECK(glsl.find("// GLSL backend cannot write files; replace handle with deterministic success code.") !=
        std::string::npos);
  CHECK(glsl.find("stack[sp - 1] = 0;") != std::string::npos);
}

TEST_CASE("ir to glsl emitter writes address-of-local opcode") {
  primec::IrToGlslEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::AddressOfLocal, 7});
  fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(fn);

  std::string glsl;
  std::string error;
  REQUIRE(emitter.emitSource(module, glsl, error));
  CHECK(error.empty());
  CHECK(glsl.find("// GLSL backend lowers local addresses to deterministic slot-byte offsets.") !=
        std::string::npos);
  CHECK(glsl.find("stack[sp++] = 56;") != std::string::npos);
}

TEST_CASE("ir to glsl emitter writes load-indirect opcode") {
  primec::IrToGlslEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::PushI32, 41});
  fn.instructions.push_back({primec::IrOpcode::StoreLocal, 3});
  fn.instructions.push_back({primec::IrOpcode::AddressOfLocal, 3});
  fn.instructions.push_back({primec::IrOpcode::LoadIndirect, 0});
  fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(fn);

  std::string glsl;
  std::string error;
  REQUIRE(emitter.emitSource(module, glsl, error));
  CHECK(error.empty());
  CHECK(glsl.find("// GLSL backend loads locals through deterministic aligned byte-slot addressing.") !=
        std::string::npos);
  CHECK(glsl.find("uint loadIndirectAddress = uint(stack[--sp]);") != std::string::npos);
  CHECK(glsl.find("if ((loadIndirectAddress & 7u) == 0u) {") != std::string::npos);
  CHECK(glsl.find("loadIndirectValue = locals[loadIndirectIndex];") != std::string::npos);
  CHECK(glsl.find("stack[sp++] = loadIndirectValue;") != std::string::npos);
}

TEST_CASE("ir to glsl emitter writes store-indirect opcode") {
  primec::IrToGlslEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::PushI32, 24});
  fn.instructions.push_back({primec::IrOpcode::PushI32, 41});
  fn.instructions.push_back({primec::IrOpcode::StoreIndirect, 0});
  fn.instructions.push_back({primec::IrOpcode::LoadLocal, 3});
  fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(fn);

  std::string glsl;
  std::string error;
  REQUIRE(emitter.emitSource(module, glsl, error));
  CHECK(error.empty());
  CHECK(glsl.find("// GLSL backend stores locals through deterministic aligned byte-slot addressing.") !=
        std::string::npos);
  CHECK(glsl.find("int storeIndirectValue = stack[--sp];") != std::string::npos);
  CHECK(glsl.find("uint storeIndirectAddress = uint(stack[--sp]);") != std::string::npos);
  CHECK(glsl.find("if ((storeIndirectAddress & 7u) == 0u) {") != std::string::npos);
  CHECK(glsl.find("locals[storeIndirectIndex] = storeIndirectValue;") != std::string::npos);
  CHECK(glsl.find("stack[sp++] = storeIndirectValue;") != std::string::npos);
}

TEST_CASE("ir to glsl emitter rejects file-write-string out-of-range index") {
  primec::IrToGlslEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::PushI32, encodeSignedI32Imm(-1)});
  fn.instructions.push_back({primec::IrOpcode::FileWriteString, 3});
  fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(fn);

  std::string glsl;
  std::string error;
  CHECK_FALSE(emitter.emitSource(module, glsl, error));
  CHECK(error.find("string index out of range") != std::string::npos);
}

TEST_CASE("ir to glsl emitter rejects address-of-local out-of-range index") {
  primec::IrToGlslEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::AddressOfLocal, 2048});
  fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(fn);

  std::string glsl;
  std::string error;
  CHECK_FALSE(emitter.emitSource(module, glsl, error));
  CHECK(error.find("local index out of range") != std::string::npos);
}

TEST_CASE("ir to glsl emitter rejects unsupported opcodes") {
  primec::IrToGlslEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({static_cast<primec::IrOpcode>(255), 0});
  fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
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

TEST_CASE("ir to glsl emitter rejects out-of-range string indices") {
  primec::IrToGlslEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::PushI32, 0});
  fn.instructions.push_back({primec::IrOpcode::LoadStringByte, 1});
  fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(fn);

  std::string glsl;
  std::string error;
  CHECK_FALSE(emitter.emitSource(module, glsl, error));
  CHECK(error.find("string index out of range") != std::string::npos);
}

TEST_CASE("ir to glsl emitter rejects out-of-range print string indices") {
  primec::IrToGlslEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back(
      {primec::IrOpcode::PrintString, primec::encodePrintStringImm(1, primec::encodePrintFlags(true, false))});
  fn.instructions.push_back({primec::IrOpcode::ReturnVoid, 0});
  module.functions.push_back(fn);

  std::string glsl;
  std::string error;
  CHECK_FALSE(emitter.emitSource(module, glsl, error));
  CHECK(error.find("string index out of range") != std::string::npos);
}

TEST_CASE("ir to glsl emitter rejects pushi64 literals outside i32 range") {
  primec::IrToGlslEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::PushI64, 3000000000ull});
  fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(fn);

  std::string glsl;
  std::string error;
  CHECK_FALSE(emitter.emitSource(module, glsl, error));
  CHECK(error.find("i64 literal out of i32 range") != std::string::npos);
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
