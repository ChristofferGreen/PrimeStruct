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

bool buildMachOImportProbe(const std::string &symbol, std::vector<uint8_t> &image, std::string &error) {
#if defined(__APPLE__) && (defined(__aarch64__) || defined(__arm64__))
  using native_emitter::MachOData;
  native_emitter::MachOData data;
  data.imports = {symbol};
  data.stateBytes = 8;

  // Word indices of the page-relative instructions the layout patches below.
  constexpr size_t GotAdrp = 2;
  constexpr size_t GotLoad = 3;
  constexpr size_t StateAdrp = 6;
  constexpr size_t StateAdd = 7;
  std::vector<uint32_t> words = {
      0xD10043FFu, // sub sp, sp, #16
      0xF90003FEu, // str x30, [sp]
      0x90000010u, // adrp x16, <the pointer slot's page>
      0xF9400210u, // ldr x16, [x16, <its offset>]
      0x92800080u, // movn x0, #4: -5
      0xD63F0200u, // blr x16
      0x90000009u, // adrp x9, <the state's page>
      0x91000129u, // add x9, x9, <its offset>
      0xF9000120u, // str x0, [x9]
      0xD2800000u, // mov x0, #0
      0xF9400120u, // ldr x0, [x9]
      0xF94003FEu, // ldr x30, [sp]
      0x910043FFu, // add sp, sp, #16
      0xD65F03C0u, // ret
  };

  const uint32_t codeOffset = native_emitter::computeMachOCodeOffset(&data);
  const uint64_t dataVmAddr = native_emitter::machODataVmAddr(codeOffset, words.size() * 4);
  const auto adrpWord = [&](size_t index, uint64_t target, uint32_t base) {
    const uint64_t pc = native_emitter::TextVmAddr + codeOffset + index * 4;
    const int64_t pages = static_cast<int64_t>((target & ~0xFFFull) - (pc & ~0xFFFull)) >> 12;
    return base | ((static_cast<uint32_t>(pages) & 3u) << 29) | (((static_cast<uint32_t>(pages) >> 2) & 0x7FFFFu) << 5);
  };
  const uint64_t slot = dataVmAddr;
  const uint64_t state = dataVmAddr + 8;
  words[GotAdrp] = adrpWord(GotAdrp, slot, words[GotAdrp]);
  words[GotLoad] |= static_cast<uint32_t>((slot & 0xFFFull) / 8) << 10;
  words[StateAdrp] = adrpWord(StateAdrp, state, words[StateAdrp]);
  words[StateAdd] |= static_cast<uint32_t>(state & 0xFFFull) << 10;

  std::vector<uint8_t> code;
  for (const uint32_t word : words) {
    for (int shift = 0; shift < 32; shift += 8) {
      code.push_back(static_cast<uint8_t>((word >> shift) & 0xFFu));
    }
  }
  return native_emitter::buildMachO(code, image, error, &data);
#else
  (void)symbol;
  (void)image;
  error = "the Mach-O import probe needs macOS/arm64";
  return false;
#endif
}

} // namespace primec::testing
