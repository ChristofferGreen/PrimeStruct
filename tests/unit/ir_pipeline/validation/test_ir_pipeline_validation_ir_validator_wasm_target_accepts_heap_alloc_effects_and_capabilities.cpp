#include "test_ir_pipeline_validation_callback_factories.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir validator wasm target accepts heap_alloc effects and capabilities") {
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.metadata.effectMask = primec::EffectHeapAlloc | primec::EffectFileWrite;
  fn.metadata.capabilityMask = primec::EffectHeapAlloc | primec::EffectFileWrite;
  fn.instructions.push_back({primec::IrOpcode::ReturnVoid, 0});
  module.functions.push_back(fn);

  std::string error;
  CHECK(primec::validateIrModule(module, primec::IrValidationTarget::Wasm, error));
  CHECK(error.empty());

  fn.metadata.effectMask = primec::EffectPathSpaceNotify;
  fn.metadata.capabilityMask = primec::EffectIoOut;
  module.functions[0] = fn;
  CHECK_FALSE(primec::validateIrModule(module, primec::IrValidationTarget::Wasm, error));
  CHECK_ERROR_CONTAINS(error, "unsupported effect mask bits for wasm target");

  fn.metadata.effectMask = primec::EffectIoOut;
  fn.metadata.capabilityMask = primec::EffectPathSpaceNotify;
  module.functions[0] = fn;
  CHECK_FALSE(primec::validateIrModule(module, primec::IrValidationTarget::Wasm, error));
  CHECK_ERROR_CONTAINS(error, "unsupported capability mask bits for wasm target");
}

TEST_CASE("ir validator wasm-browser target accepts integer control-flow subset") {
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::PushI32, 1});
  fn.instructions.push_back({primec::IrOpcode::StoreLocal, 0});
  fn.instructions.push_back({primec::IrOpcode::LoadLocal, 0});
  fn.instructions.push_back({primec::IrOpcode::PushI32, 4});
  fn.instructions.push_back({primec::IrOpcode::AddI32, 0});
  fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(fn);

  std::string error;
  CHECK(primec::validateIrModule(module, primec::IrValidationTarget::WasmBrowser, error));
  CHECK(error.empty());
}

TEST_CASE("ir validator wasm-browser target rejects wasi-only opcodes") {
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::PushArgc, 0});
  fn.instructions.push_back({primec::IrOpcode::Pop, 0});
  fn.instructions.push_back({primec::IrOpcode::ReturnVoid, 0});
  module.functions.push_back(fn);

  std::string error;
  CHECK_FALSE(primec::validateIrModule(module, primec::IrValidationTarget::WasmBrowser, error));
  CHECK_ERROR_CONTAINS(error, "unsupported opcode for wasm-browser target");
}

TEST_CASE("ir validator wasm-browser target rejects effects and capabilities") {
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.metadata.effectMask = primec::EffectIoOut;
  fn.metadata.capabilityMask = primec::EffectIoOut;
  fn.instructions.push_back({primec::IrOpcode::ReturnVoid, 0});
  module.functions.push_back(fn);

  std::string error;
  CHECK_FALSE(primec::validateIrModule(module, primec::IrValidationTarget::WasmBrowser, error));
  CHECK_ERROR_CONTAINS(error, "unsupported effect mask bits for wasm-browser target");

  fn.metadata.effectMask = 0;
  fn.metadata.capabilityMask = primec::EffectIoOut;
  module.functions[0] = fn;
  CHECK_FALSE(primec::validateIrModule(module, primec::IrValidationTarget::WasmBrowser, error));
  CHECK_ERROR_CONTAINS(error, "unsupported capability mask bits for wasm-browser target");
}


TEST_SUITE_END();
