#include "test_ir_pipeline_wasm_helpers.h"

using namespace ir_pipeline_wasm_test;

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

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
      0x02, 0x40,
      0x03, 0x40,
  };
  const std::vector<uint8_t> breakPattern = {
      0x45,
      0x0d, 0x01,
  };
  const std::vector<uint8_t> continuePattern = {
      0x0c, 0x00,
      0x0b, 0x0b,
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
      0x41, 0x02,
      0xb2,
      0x43, 0x00, 0x00, 0xc0, 0x3f,
      0x92,
      0xbb,
      0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x3f,
      0xa1,
      0xb6,
      0xa8,
  };
  CHECK(containsByteSequence(bytes, opPattern));
}

TEST_CASE("wasm emitter lowers i64 and u64 conversion opcodes deterministically") {
  primec::WasmEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;

  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::PushF32, 0x41200000u});
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
      0x43, 0x00, 0x00, 0x20, 0x41,
      0xae,
      0xb9,
      0xb1,
      0xb5,
      0xa8,
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
      0x10, 0x01,
      0x41, 0x03,
      0x6a,
  };
  const std::vector<uint8_t> callVoidPattern = {
      0x10, 0x00,
      0x10, 0x01,
      0x1a,
      0x10, 0x01,
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
      0x10, 0x00,
      0x0f,
  };
  CHECK(containsByteSequence(bytes, recursiveCallPattern));
}

TEST_SUITE_END();
