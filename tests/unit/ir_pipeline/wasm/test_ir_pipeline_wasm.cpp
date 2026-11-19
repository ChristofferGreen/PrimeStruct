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
      0x41, 0x02, 0xb2, 0x43, 0x00, 0x00, 0xc0, 0x3f, 0x92, 0xbb, 0x44, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x3f, 0xa1, 0xb6, 0xfc, 0x00,
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
      0x43,
      0x00,
      0x00,
      0x20,
      0x41,
      0xfc,
      0x04,
      0xb9,
      0xfc,
      0x07,
      0xb5,
      0xfc,
      0x00,
  };
  CHECK(containsByteSequence(bytes, opPattern));
}

TEST_CASE("wasm emitter lowers float to integer conversions to saturating truncations") {
  // A float converts by truncating, saturating at the target's limits, NaN as 0 (docs/spec/type-system.md, Numeric
  // Conversions): the 0xfc-prefixed trunc_sat opcodes, not the trapping trunc.
  primec::WasmEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;

  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::PushF64, 0x3ff0000000000000ull});
  mainFn.instructions.push_back({primec::IrOpcode::ConvertF64ToI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::Pop, 0});
  mainFn.instructions.push_back({primec::IrOpcode::PushF64, 0x3ff0000000000000ull});
  mainFn.instructions.push_back({primec::IrOpcode::ConvertF64ToI64, 0});
  mainFn.instructions.push_back({primec::IrOpcode::Pop, 0});
  mainFn.instructions.push_back({primec::IrOpcode::PushF32, 0x3f800000u});
  mainFn.instructions.push_back({primec::IrOpcode::ConvertF32ToU64, 0});
  mainFn.instructions.push_back({primec::IrOpcode::Pop, 0});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(mainFn);

  std::vector<uint8_t> bytes;
  std::string error;
  REQUIRE(emitter.emitModule(module, bytes, error));
  CHECK(error.empty());

  const std::vector<uint8_t> opPattern = {
      0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x3f, 0xfc, 0x02,
      0x1a, 0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x3f, 0xfc,
      0x06, 0x1a, 0x43, 0x00, 0x00, 0x80, 0x3f, 0xfc, 0x05, 0x1a,
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

// WebAssembly has no fused multiply-add: fma is (a * b) + c with the addend parked in an i64 local, which rounds twice
// (docs/spec/vm-design.md, Fused multiply-add).
TEST_CASE("wasm emitter lowers fused multiply-add as a multiply and an add") {
  for (const bool isF64 : {true, false}) {
    primec::WasmEmitter emitter;
    primec::IrModule module;
    module.entryIndex = 0;

    primec::IrFunction mainFn;
    mainFn.name = "/main";
    if (isF64) {
      mainFn.instructions.push_back({primec::IrOpcode::PushF64, 0x3ff0000000000000ull});
      mainFn.instructions.push_back({primec::IrOpcode::PushF64, 0x4000000000000000ull});
      mainFn.instructions.push_back({primec::IrOpcode::PushF64, 0x4008000000000000ull});
      mainFn.instructions.push_back({primec::IrOpcode::FmaF64, 0});
      mainFn.instructions.push_back({primec::IrOpcode::ReturnF64, 0});
    } else {
      mainFn.instructions.push_back({primec::IrOpcode::PushF32, 0x3f800000ull});
      mainFn.instructions.push_back({primec::IrOpcode::PushF32, 0x40000000ull});
      mainFn.instructions.push_back({primec::IrOpcode::PushF32, 0x40400000ull});
      mainFn.instructions.push_back({primec::IrOpcode::FmaF32, 0});
      mainFn.instructions.push_back({primec::IrOpcode::ReturnF32, 0});
    }
    module.functions.push_back(std::move(mainFn));

    std::vector<uint8_t> bytes;
    std::string error;
    REQUIRE(emitter.emitModule(module, bytes, error));
    CHECK(error.empty());

    // The only local is the i64 temp (index 0): set it from the reinterpreted addend, multiply, get it back, add.
    const std::vector<uint8_t> f64Pattern = {0xbd, 0x21, 0x00, 0xa2, 0x20, 0x00, 0xbf, 0xa0};
    const std::vector<uint8_t> f32Pattern = {
        0xbc, 0xad, 0x21, 0x00, 0x94, 0x20, 0x00, 0xa7, 0xbe, 0x92};
    CHECK(containsByteSequence(bytes, isF64 ? f64Pattern : f32Pattern));
  }
}

// IR locals carry no type: the emitter infers the wasm type of every local and parameter from the values stored and
// passed, and converts an integer operand to the width an instruction wants (WasmEmitterTypes.cpp). SextI32 of an i64
// narrows it modulo 2^32 (`i32.wrap_i64`) and an i32 used as an i64 sign-extends (`i64.extend_i32_s`).
namespace {
std::vector<uint8_t> emitSingleFunctionModule(primec::IrFunction mainFn) {
  primec::WasmEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  mainFn.name = "/main";
  module.functions.push_back(std::move(mainFn));
  std::vector<uint8_t> bytes;
  std::string error;
  REQUIRE(emitter.emitModule(module, bytes, error));
  CHECK(error.empty());
  return bytes;
}
} // namespace

TEST_CASE("wasm emitter wraps a narrowed i64 and extends a widened i32") {
  primec::IrFunction mainFn;
  mainFn.instructions.push_back({primec::IrOpcode::PushI64, 4294967297ull});
  mainFn.instructions.push_back({primec::IrOpcode::StoreLocal, 0});
  mainFn.instructions.push_back({primec::IrOpcode::LoadLocal, 0});
  mainFn.instructions.push_back({primec::IrOpcode::SextI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::StoreLocal, 1});
  mainFn.instructions.push_back({primec::IrOpcode::LoadLocal, 1});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnI64, 0});
  const std::vector<uint8_t> bytes = emitSingleFunctionModule(std::move(mainFn));

  // local 0 is an i64 and local 1 an i32.
  CHECK(containsByteSequence(bytes, {0x02, 0x01, 0x7e, 0x01, 0x7f, 0x42}));
  // store, load, i32.wrap_i64, store, load, i64.extend_i32_s, return
  CHECK(containsByteSequence(bytes,
                             {0x21, 0x00, 0x20, 0x00, 0xa7, 0x21, 0x01, 0x20, 0x01, 0xac, 0x0f}));
}

TEST_CASE("wasm emitter parks the operands above a mismatched integer width while it converts") {
  primec::IrFunction mainFn;
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 3});
  mainFn.instructions.push_back({primec::IrOpcode::PushI64, 4});
  mainFn.instructions.push_back({primec::IrOpcode::AddI64, 0});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnI64, 0});
  const std::vector<uint8_t> bytes = emitSingleFunctionModule(std::move(mainFn));

  // the two scratch locals (an i64 and an i32), then: i32.const 3, i64.const 4, set the i64, set the i32, get the i32
  // and extend it, get the i64, i64.add
  CHECK(containsByteSequence(bytes, {0x02, 0x01, 0x7e, 0x01, 0x7f, 0x41, 0x03, 0x42, 0x04, 0x21,
                                     0x00, 0x21, 0x01, 0x20, 0x01, 0xac, 0x20, 0x00, 0x7c, 0x0f}));
}

TEST_CASE("wasm emitter types the parameters of a function by what its calls pass") {
  primec::WasmEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;

  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::PushI64, 9});
  mainFn.instructions.push_back({primec::IrOpcode::Call, 1});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnI64, 0});
  module.functions.push_back(std::move(mainFn));

  primec::IrFunction calleeFn;
  calleeFn.name = "/callee";
  calleeFn.parameterCount = 1;
  calleeFn.instructions.push_back({primec::IrOpcode::StoreLocal, 0});
  calleeFn.instructions.push_back({primec::IrOpcode::LoadLocal, 0});
  calleeFn.instructions.push_back({primec::IrOpcode::ReturnI64, 0});
  module.functions.push_back(std::move(calleeFn));

  std::vector<uint8_t> bytes;
  std::string error;
  REQUIRE(emitter.emitModule(module, bytes, error));
  CHECK(error.empty());

  // (func (param i64) (result i64)) is the callee's type.
  CHECK(containsByteSequence(bytes, {0x60, 0x01, 0x7e, 0x01, 0x7e}));
}

TEST_CASE("wasm emitter duplicates a float through a scratch local of its own type") {
  primec::IrFunction mainFn;
  mainFn.instructions.push_back({primec::IrOpcode::PushF64, 0x3ff0000000000000ull});
  mainFn.instructions.push_back({primec::IrOpcode::Dup, 0});
  mainFn.instructions.push_back({primec::IrOpcode::AddF64, 0});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnF64, 0});
  const std::vector<uint8_t> bytes = emitSingleFunctionModule(std::move(mainFn));

  // one f64 local; the dup is local.tee 0, local.get 0
  CHECK(containsByteSequence(bytes,
                             {0x01,
                              0x01,
                              0x7c,
                              0x44,
                              0x00,
                              0x00,
                              0x00,
                              0x00,
                              0x00,
                              0x00,
                              0xf0,
                              0x3f,
                              0x22,
                              0x00,
                              0x20,
                              0x00,
                              0xa0,
                              0x0f}));
}

TEST_CASE("wasm emitter rejects a local that holds an integer and a float") {
  primec::WasmEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 1});
  mainFn.instructions.push_back({primec::IrOpcode::StoreLocal, 0});
  mainFn.instructions.push_back({primec::IrOpcode::PushF32, 0x3f800000u});
  mainFn.instructions.push_back({primec::IrOpcode::StoreLocal, 0});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(std::move(mainFn));
  std::vector<uint8_t> bytes;
  std::string error;
  CHECK_FALSE(emitter.emitModule(module, bytes, error));
  CHECK(error.find("different kinds") != std::string::npos);
}

TEST_SUITE_END();
