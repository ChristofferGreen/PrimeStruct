#include "primec/ir/IrLocalEscape.h"

#include <algorithm>
#include <limits>

namespace primec {
namespace {

// IR validation caps local indices at 32 bits; clamp so a malformed module
// cannot overflow the slot count.
uint32_t clampSlot(uint64_t index) {
  return static_cast<uint32_t>(std::min<uint64_t>(index, std::numeric_limits<uint32_t>::max() - 1));
}

} // namespace

bool IrLocalEscapeInfo::isPinned(uint32_t slot) const {
  return std::binary_search(pinnedSlots.begin(), pinnedSlots.end(), slot);
}

IrLocalEscapeInfo analyzeIrLocalEscape(const IrFunction &function) {
  IrLocalEscapeInfo info;
  std::vector<uint32_t> fileReadTargets;

  const auto noteSlot = [&info](uint32_t slot) {
    info.localCount = std::max<uint32_t>(info.localCount, slot + 1);
  };

  for (const IrInstruction &instruction : function.instructions) {
    switch (instruction.op) {
    case IrOpcode::LoadLocal:
    case IrOpcode::StoreLocal:
      noteSlot(clampSlot(instruction.imm));
      break;
    case IrOpcode::AddressOfLocal: {
      const uint32_t slot = clampSlot(instruction.imm);
      noteSlot(slot);
      info.lowestAddressedSlot =
          info.addressTaken ? std::min(info.lowestAddressedSlot, slot) : slot;
      info.addressTaken = true;
      break;
    }
    case IrOpcode::FileReadByte: {
      const uint32_t slot = clampSlot(instruction.imm);
      noteSlot(slot);
      fileReadTargets.push_back(slot);
      break;
    }
    default:
      break;
    }
  }

  if (info.addressTaken) {
    info.pinnedSlots.reserve(info.localCount);
    for (uint32_t slot = 0; slot < info.localCount; ++slot) {
      info.pinnedSlots.push_back(slot);
    }
    return info;
  }
  info.pinnedSlots = std::move(fileReadTargets);
  std::sort(info.pinnedSlots.begin(), info.pinnedSlots.end());
  info.pinnedSlots.erase(std::unique(info.pinnedSlots.begin(), info.pinnedSlots.end()),
                         info.pinnedSlots.end());
  return info;
}

} // namespace primec
