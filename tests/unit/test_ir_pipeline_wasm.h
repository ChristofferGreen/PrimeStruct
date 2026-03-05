#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "primec/WasmEmitter.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

namespace {

bool hasMinimalWasmHeader(const std::vector<uint8_t> &bytes) {
  static constexpr uint8_t Expected[] = {0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00};
  if (bytes.size() < sizeof(Expected)) {
    return false;
  }
  for (size_t i = 0; i < sizeof(Expected); ++i) {
    if (bytes[i] != Expected[i]) {
      return false;
    }
  }
  return true;
}

bool readU32Leb(const std::vector<uint8_t> &bytes, size_t &offset, uint32_t &value) {
  value = 0;
  uint32_t shift = 0;
  for (int i = 0; i < 5; ++i) {
    if (offset >= bytes.size()) {
      return false;
    }
    const uint8_t byte = bytes[offset++];
    value |= static_cast<uint32_t>(byte & 0x7f) << shift;
    if ((byte & 0x80) == 0) {
      return true;
    }
    shift += 7;
  }
  return false;
}

bool parseSections(const std::vector<uint8_t> &bytes,
                   std::vector<uint8_t> &sectionIds,
                   std::vector<uint32_t> &sectionLengths,
                   std::string &error) {
  error.clear();
  sectionIds.clear();
  sectionLengths.clear();
  if (!hasMinimalWasmHeader(bytes)) {
    error = "missing wasm header";
    return false;
  }

  size_t offset = 8;
  while (offset < bytes.size()) {
    const uint8_t sectionId = bytes[offset++];
    uint32_t sectionLength = 0;
    if (!readU32Leb(bytes, offset, sectionLength)) {
      error = "failed to decode section length";
      return false;
    }
    if (offset + sectionLength > bytes.size()) {
      error = "section payload exceeds module bounds";
      return false;
    }
    sectionIds.push_back(sectionId);
    sectionLengths.push_back(sectionLength);
    offset += sectionLength;
  }
  return true;
}

bool sectionPayloadIsEmptyVector(const std::vector<uint8_t> &bytes, uint8_t sectionId) {
  size_t offset = 8;
  while (offset < bytes.size()) {
    const uint8_t currentSectionId = bytes[offset++];
    uint32_t sectionLength = 0;
    if (!readU32Leb(bytes, offset, sectionLength)) {
      return false;
    }
    if (offset + sectionLength > bytes.size()) {
      return false;
    }
    if (currentSectionId == sectionId) {
      return sectionLength == 1 && bytes[offset] == 0;
    }
    offset += sectionLength;
  }
  return false;
}

bool containsByteSequence(const std::vector<uint8_t> &haystack, const std::vector<uint8_t> &needle) {
  if (needle.empty() || needle.size() > haystack.size()) {
    return false;
  }
  for (size_t start = 0; start + needle.size() <= haystack.size(); ++start) {
    bool match = true;
    for (size_t offset = 0; offset < needle.size(); ++offset) {
      if (haystack[start + offset] != needle[offset]) {
        match = false;
        break;
      }
    }
    if (match) {
      return true;
    }
  }
  return false;
}

} // namespace

TEST_CASE("wasm emitter writes deterministic empty-module section snapshots") {
  primec::WasmEmitter emitter;
  primec::IrModule module;
  module.entryIndex = -1;

  std::vector<uint8_t> first;
  std::vector<uint8_t> second;
  std::string error;
  REQUIRE(emitter.emitModule(module, first, error));
  CHECK(error.empty());
  REQUIRE(emitter.emitModule(module, second, error));
  CHECK(error.empty());
  CHECK(first == second);
  CHECK(hasMinimalWasmHeader(first));

  const std::vector<uint8_t> expected = {
      0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
      0x01, 0x01, 0x00,
      0x02, 0x01, 0x00,
      0x03, 0x01, 0x00,
      0x07, 0x01, 0x00,
      0x0a, 0x01, 0x00,
      0x0b, 0x01, 0x00,
  };
  CHECK(first == expected);
}

TEST_CASE("wasm emitter passes structural section validation for empty module") {
  primec::WasmEmitter emitter;
  primec::IrModule module;
  module.entryIndex = -1;

  std::vector<uint8_t> bytes;
  std::string error;
  REQUIRE(emitter.emitModule(module, bytes, error));
  CHECK(error.empty());

  std::vector<uint8_t> sectionIds;
  std::vector<uint32_t> sectionLengths;
  REQUIRE(parseSections(bytes, sectionIds, sectionLengths, error));
  CHECK(error.empty());

  const std::vector<uint8_t> expectedSectionIds = {1, 2, 3, 7, 10, 11};
  CHECK(sectionIds == expectedSectionIds);
  CHECK(sectionLengths.size() == expectedSectionIds.size());
  for (uint32_t sectionLength : sectionLengths) {
    CHECK(sectionLength == 1u);
  }

  CHECK(sectionPayloadIsEmptyVector(bytes, 1));
  CHECK(sectionPayloadIsEmptyVector(bytes, 2));
  CHECK(sectionPayloadIsEmptyVector(bytes, 3));
  CHECK(sectionPayloadIsEmptyVector(bytes, 7));
  CHECK(sectionPayloadIsEmptyVector(bytes, 10));
  CHECK(sectionPayloadIsEmptyVector(bytes, 11));
}

TEST_CASE("wasm emitter rejects invalid empty-module entry index") {
  primec::WasmEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;

  std::vector<uint8_t> bytes;
  std::string error;
  CHECK_FALSE(emitter.emitModule(module, bytes, error));
  CHECK(error == "invalid IR entry index");
}

TEST_CASE("wasm emitter lowers simple integer function to deterministic bytes") {
  primec::WasmEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;

  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 7});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(mainFn);

  std::vector<uint8_t> bytes;
  std::string error;
  REQUIRE(emitter.emitModule(module, bytes, error));
  CHECK(error.empty());

  const std::vector<uint8_t> expected = {
      0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
      0x01, 0x05, 0x01, 0x60, 0x00, 0x01, 0x7f,
      0x02, 0x01, 0x00,
      0x03, 0x02, 0x01, 0x00,
      0x07, 0x08, 0x01, 0x04, 0x6d, 0x61, 0x69, 0x6e, 0x00, 0x00,
      0x0a, 0x07, 0x01, 0x05, 0x00, 0x41, 0x07, 0x0f, 0x0b,
      0x0b, 0x01, 0x00,
  };
  CHECK(bytes == expected);
}

TEST_CASE("wasm emitter rejects unsupported backward jump control flow") {
  primec::WasmEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;

  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 1});
  mainFn.instructions.push_back({primec::IrOpcode::Jump, 0});
  module.functions.push_back(mainFn);

  std::vector<uint8_t> bytes;
  std::string error;
  CHECK_FALSE(emitter.emitModule(module, bytes, error));
  CHECK(error.find("unsupported control-flow pattern") != std::string::npos);
}

TEST_CASE("wasm emitter lowers canonical backward-branch loops") {
  primec::WasmEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;

  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 3});
  mainFn.instructions.push_back({primec::IrOpcode::StoreLocal, 0});
  mainFn.instructions.push_back({primec::IrOpcode::LoadLocal, 0});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::CmpGtI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::JumpIfZero, 11});
  mainFn.instructions.push_back({primec::IrOpcode::LoadLocal, 0});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 1});
  mainFn.instructions.push_back({primec::IrOpcode::SubI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::StoreLocal, 0});
  mainFn.instructions.push_back({primec::IrOpcode::Jump, 2});
  mainFn.instructions.push_back({primec::IrOpcode::LoadLocal, 0});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(mainFn);

  std::vector<uint8_t> bytes;
  std::string error;
  REQUIRE(emitter.emitModule(module, bytes, error));
  CHECK(error.empty());

  const std::vector<uint8_t> loopHeaderPattern = {
      0x02, 0x40, // block void
      0x03, 0x40, // loop void
  };
  const std::vector<uint8_t> breakPattern = {
      0x45,       // i32.eqz
      0x0d, 0x01, // br_if 1
  };
  const std::vector<uint8_t> continuePattern = {
      0x0c, 0x00, // br 0
      0x0b, 0x0b, // end loop + end block
  };
  CHECK(containsByteSequence(bytes, loopHeaderPattern));
  CHECK(containsByteSequence(bytes, breakPattern));
  CHECK(containsByteSequence(bytes, continuePattern));
}

TEST_CASE("wasm emitter rejects malformed jump targets deterministically") {
  primec::WasmEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;

  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 1});
  mainFn.instructions.push_back({primec::IrOpcode::JumpIfZero, 999});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 7});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(mainFn);

  std::vector<uint8_t> bytes;
  std::string error;
  CHECK_FALSE(emitter.emitModule(module, bytes, error));
  CHECK(error == "wasm emitter malformed jump target in function: /main");
}

TEST_CASE("wasm emitter rejects malformed backward jump targets deterministically") {
  primec::WasmEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;

  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 1});
  mainFn.instructions.push_back({primec::IrOpcode::Jump, 999});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(mainFn);

  std::vector<uint8_t> bytes;
  std::string error;
  CHECK_FALSE(emitter.emitModule(module, bytes, error));
  CHECK(error == "wasm emitter malformed jump target in function: /main");
}

TEST_CASE("wasm emitter lowers float ops and conversions to deterministic opcodes") {
  primec::WasmEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;

  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 2});
  mainFn.instructions.push_back({primec::IrOpcode::ConvertI32ToF32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::PushF32, 0x3fc00000u});
  mainFn.instructions.push_back({primec::IrOpcode::AddF32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::ConvertF32ToF64, 0});
  mainFn.instructions.push_back({primec::IrOpcode::PushF64, 0x3ff0000000000000ull});
  mainFn.instructions.push_back({primec::IrOpcode::SubF64, 0});
  mainFn.instructions.push_back({primec::IrOpcode::ConvertF64ToF32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::ConvertF32ToI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(mainFn);

  std::vector<uint8_t> bytes;
  std::string error;
  REQUIRE(emitter.emitModule(module, bytes, error));
  CHECK(error.empty());

  const std::vector<uint8_t> opPattern = {
      0x41, 0x02,                   // i32.const 2
      0xb2,                         // f32.convert_i32_s
      0x43, 0x00, 0x00, 0xc0, 0x3f, // f32.const 1.5
      0x92,                         // f32.add
      0xbb,                         // f64.promote_f32
      0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x3f, // f64.const 1.0
      0xa1,                                                      // f64.sub
      0xb6,                                                      // f32.demote_f64
      0xa8,                                                      // i32.trunc_f32_s
  };
  CHECK(containsByteSequence(bytes, opPattern));
}

TEST_CASE("wasm emitter lowers i64 and u64 conversion opcodes deterministically") {
  primec::WasmEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;

  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::PushF32, 0x41200000u}); // 10.0f32
  mainFn.instructions.push_back({primec::IrOpcode::ConvertF32ToI64, 0});
  mainFn.instructions.push_back({primec::IrOpcode::ConvertI64ToF64, 0});
  mainFn.instructions.push_back({primec::IrOpcode::ConvertF64ToU64, 0});
  mainFn.instructions.push_back({primec::IrOpcode::ConvertU64ToF32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::ConvertF32ToI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(mainFn);

  std::vector<uint8_t> bytes;
  std::string error;
  REQUIRE(emitter.emitModule(module, bytes, error));
  CHECK(error.empty());

  const std::vector<uint8_t> opPattern = {
      0x43, 0x00, 0x00, 0x20, 0x41, // f32.const 10.0
      0xae,                         // i64.trunc_f32_s
      0xb9,                         // f64.convert_i64_s
      0xb1,                         // i64.trunc_f64_u
      0xb5,                         // f32.convert_i64_u
      0xa8,                         // i32.trunc_f32_s
  };
  CHECK(containsByteSequence(bytes, opPattern));
}

TEST_CASE("wasm emitter lowers direct call and callvoid opcodes") {
  primec::WasmEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 2;

  primec::IrFunction voidFn;
  voidFn.name = "/void_fn";
  voidFn.instructions.push_back({primec::IrOpcode::ReturnVoid, 0});

  primec::IrFunction valueFn;
  valueFn.name = "/value_fn";
  valueFn.instructions.push_back({primec::IrOpcode::PushI32, 4});
  valueFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});

  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::CallVoid, 0});
  mainFn.instructions.push_back({primec::IrOpcode::CallVoid, 1});
  mainFn.instructions.push_back({primec::IrOpcode::Call, 1});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 3});
  mainFn.instructions.push_back({primec::IrOpcode::AddI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});

  module.functions.push_back(std::move(voidFn));
  module.functions.push_back(std::move(valueFn));
  module.functions.push_back(std::move(mainFn));

  std::vector<uint8_t> bytes;
  std::string error;
  REQUIRE(emitter.emitModule(module, bytes, error));
  CHECK(error.empty());

  const std::vector<uint8_t> callPattern = {
      0x10, 0x01, // call value_fn
      0x41, 0x03, // i32.const 3
      0x6a,       // i32.add
  };
  const std::vector<uint8_t> callVoidPattern = {
      0x10, 0x00, // call void_fn
      0x10, 0x01, // call value_fn
      0x1a,       // drop callvoid result
      0x10, 0x01, // call value_fn
  };
  CHECK(containsByteSequence(bytes, callPattern));
  CHECK(containsByteSequence(bytes, callVoidPattern));
}

TEST_CASE("wasm emitter lowers recursive call opcode in canonical IR") {
  primec::WasmEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;

  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::Call, 0});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(std::move(mainFn));

  std::vector<uint8_t> bytes;
  std::string error;
  REQUIRE(emitter.emitModule(module, bytes, error));
  CHECK(error.empty());

  const std::vector<uint8_t> recursiveCallPattern = {
      0x10, 0x00, // call self
      0x0f,       // return
  };
  CHECK(containsByteSequence(bytes, recursiveCallPattern));
}

TEST_CASE("wasm emitter adds wasi imports memory and argv output lowering") {
  primec::WasmEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  module.stringTable.push_back("hello");

  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::PushArgc, 0});
  mainFn.instructions.push_back({primec::IrOpcode::Pop, 0});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::PrintArgv, primec::PrintFlagNewline});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::PrintArgvUnsafe, primec::PrintFlagStderr});
  mainFn.instructions.push_back(
      {primec::IrOpcode::PrintString, primec::encodePrintStringImm(0, primec::PrintFlagStderr | primec::PrintFlagNewline)});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnVoid, 0});
  module.functions.push_back(std::move(mainFn));

  std::vector<uint8_t> bytes;
  std::string error;
  REQUIRE(emitter.emitModule(module, bytes, error));
  CHECK(error.empty());

  std::vector<uint8_t> sectionIds;
  std::vector<uint32_t> sectionLengths;
  REQUIRE(parseSections(bytes, sectionIds, sectionLengths, error));
  CHECK(error.empty());
  const std::vector<uint8_t> expectedSectionIds = {1, 2, 3, 5, 7, 10, 11};
  CHECK(sectionIds == expectedSectionIds);
  CHECK_FALSE(sectionPayloadIsEmptyVector(bytes, 5));

  const auto textBytes = [](const std::string &text) {
    return std::vector<uint8_t>(text.begin(), text.end());
  };
  CHECK(containsByteSequence(bytes, textBytes("wasi_snapshot_preview1")));
  CHECK(containsByteSequence(bytes, textBytes("fd_write")));
  CHECK(containsByteSequence(bytes, textBytes("args_sizes_get")));
  CHECK(containsByteSequence(bytes, textBytes("args_get")));

  CHECK(containsByteSequence(bytes, {0x10, 0x00}));
  CHECK(containsByteSequence(bytes, {0x10, 0x01}));
  CHECK(containsByteSequence(bytes, {0x10, 0x02}));
  CHECK(containsByteSequence(bytes, {0x36, 0x02, 0x00}));
  CHECK(containsByteSequence(bytes, {0x28, 0x02, 0x00}));
}

TEST_CASE("wasm emitter rejects unsupported opcodes for this slice") {
  primec::WasmEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;

  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::PushI64, 1});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnVoid, 0});
  module.functions.push_back(mainFn);

  std::vector<uint8_t> bytes;
  std::string error;
  CHECK_FALSE(emitter.emitModule(module, bytes, error));
  CHECK(error.find("unsupported opcode") != std::string::npos);
}

TEST_SUITE_END();
