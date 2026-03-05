#include "primec/WasmEmitter.h"

namespace primec {

namespace {

constexpr uint8_t WasmMagic[] = {0x00, 0x61, 0x73, 0x6d};
constexpr uint8_t WasmVersion[] = {0x01, 0x00, 0x00, 0x00};

} // namespace

bool WasmEmitter::emitModule(const IrModule &module, std::vector<uint8_t> &out, std::string &error) const {
  error.clear();
  out.clear();

  if (!module.functions.empty()) {
    if (module.entryIndex < 0 || static_cast<size_t>(module.entryIndex) >= module.functions.size()) {
      error = "invalid IR entry index";
      return false;
    }
    error = "wasm emitter function lowering is not implemented yet";
    return false;
  }

  if (module.entryIndex != -1) {
    error = "invalid IR entry index";
    return false;
  }

  for (uint8_t byte : WasmMagic) {
    out.push_back(byte);
  }
  for (uint8_t byte : WasmVersion) {
    out.push_back(byte);
  }
  return true;
}

} // namespace primec
