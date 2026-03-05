#include <cstddef>
#include <cstdint>
#include <vector>

#include "primec/WasmEmitter.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

namespace {

bool hasMinimalWasmHeader(const std::vector<uint8_t> &bytes) {
  static constexpr uint8_t Expected[] = {0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00};
  if (bytes.size() != sizeof(Expected)) {
    return false;
  }
  for (size_t i = 0; i < sizeof(Expected); ++i) {
    if (bytes[i] != Expected[i]) {
      return false;
    }
  }
  return true;
}

} // namespace

TEST_CASE("wasm emitter writes deterministic minimal module") {
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

TEST_CASE("wasm emitter rejects function lowering before codegen support") {
  primec::WasmEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;

  primec::IrFunction entry;
  entry.name = "/main";
  entry.instructions.push_back({primec::IrOpcode::ReturnVoid, 0});
  module.functions.push_back(entry);

  std::vector<uint8_t> bytes;
  std::string error;
  CHECK_FALSE(emitter.emitModule(module, bytes, error));
  CHECK(error.find("function lowering is not implemented yet") != std::string::npos);
}

TEST_SUITE_END();
