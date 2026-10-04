#include "primec/testing/NativeEmitterEncodings.h"

#include "NativeEmitterInternals.h"

namespace primec::testing {

std::vector<uint32_t> arm64TemplateWords(IrOpcode op) {
  native_emitter::Arm64Emitter emitter;
  switch (op) {
  case IrOpcode::SextI32:
    emitter.emitSextI32();
    break;
  case IrOpcode::CmpEqF32:
    emitter.emitCmpEqF32();
    break;
  case IrOpcode::CmpNeF32:
    emitter.emitCmpNeF32();
    break;
  case IrOpcode::CmpLtF32:
    emitter.emitCmpLtF32();
    break;
  case IrOpcode::CmpLeF32:
    emitter.emitCmpLeF32();
    break;
  case IrOpcode::CmpGtF32:
    emitter.emitCmpGtF32();
    break;
  case IrOpcode::CmpGeF32:
    emitter.emitCmpGeF32();
    break;
  case IrOpcode::CmpEqF64:
    emitter.emitCmpEqF64();
    break;
  case IrOpcode::CmpNeF64:
    emitter.emitCmpNeF64();
    break;
  case IrOpcode::CmpLtF64:
    emitter.emitCmpLtF64();
    break;
  case IrOpcode::CmpLeF64:
    emitter.emitCmpLeF64();
    break;
  case IrOpcode::CmpGtF64:
    emitter.emitCmpGtF64();
    break;
  case IrOpcode::CmpGeF64:
    emitter.emitCmpGeF64();
    break;
  default:
    return {};
  }
  const std::vector<uint8_t> bytes = emitter.finalize();
  std::vector<uint32_t> words;
  for (size_t i = 0; i + 4 <= bytes.size(); i += 4) {
    words.push_back(static_cast<uint32_t>(bytes[i]) | (static_cast<uint32_t>(bytes[i + 1]) << 8) |
                    (static_cast<uint32_t>(bytes[i + 2]) << 16) |
                    (static_cast<uint32_t>(bytes[i + 3]) << 24));
  }
  return words;
}

} // namespace primec::testing
