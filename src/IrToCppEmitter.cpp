#include "primec/IrToCppEmitter.h"

#include "IrToCppEmitterInternal.h"
#include "primec/Ir.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace primec {
namespace {

std::string escapeCppStringLiteral(const std::string &text) {
  std::string escaped;
  escaped.reserve(text.size() + 2);
  for (unsigned char ch : text) {
    switch (ch) {
      case '\\':
        escaped += "\\\\";
        break;
      case '"':
        escaped += "\\\"";
        break;
      case '\n':
        escaped += "\\n";
        break;
      case '\r':
        escaped += "\\r";
        break;
      case '\t':
        escaped += "\\t";
        break;
      default:
        escaped.push_back(static_cast<char>(ch));
        break;
    }
  }
  return escaped;
}

size_t computeLocalCount(const IrFunction &function) {
  size_t localCount = 0;
  for (const IrInstruction &instruction : function.instructions) {
    switch (instruction.op) {
      case IrOpcode::LoadLocal:
      case IrOpcode::StoreLocal:
      case IrOpcode::AddressOfLocal:
      case IrOpcode::FileReadByte:
        localCount = std::max(localCount, static_cast<size_t>(instruction.imm) + 1);
        break;
      default:
        break;
    }
  }
  return localCount;
}

bool usesF32Helpers(IrOpcode opcode) {
  switch (opcode) {
    case IrOpcode::AddF32:
    case IrOpcode::SubF32:
    case IrOpcode::MulF32:
    case IrOpcode::DivF32:
    case IrOpcode::NegF32:
    case IrOpcode::CmpEqF32:
    case IrOpcode::CmpNeF32:
    case IrOpcode::CmpLtF32:
    case IrOpcode::CmpLeF32:
    case IrOpcode::CmpGtF32:
    case IrOpcode::CmpGeF32:
    case IrOpcode::ConvertI32ToF32:
    case IrOpcode::ConvertI64ToF32:
    case IrOpcode::ConvertU64ToF32:
    case IrOpcode::ConvertF32ToI32:
    case IrOpcode::ConvertF32ToI64:
    case IrOpcode::ConvertF32ToU64:
    case IrOpcode::ConvertF32ToF64:
    case IrOpcode::ConvertF64ToF32:
      return true;
    default:
      return false;
  }
}

bool moduleUsesF32Helpers(const IrModule &module) {
  for (const IrFunction &function : module.functions) {
    for (const IrInstruction &instruction : function.instructions) {
      if (usesF32Helpers(instruction.op)) {
        return true;
      }
    }
  }
  return false;
}

bool usesF64Helpers(IrOpcode opcode) {
  switch (opcode) {
    case IrOpcode::AddF64:
    case IrOpcode::SubF64:
    case IrOpcode::MulF64:
    case IrOpcode::DivF64:
    case IrOpcode::NegF64:
    case IrOpcode::CmpEqF64:
    case IrOpcode::CmpNeF64:
    case IrOpcode::CmpLtF64:
    case IrOpcode::CmpLeF64:
    case IrOpcode::CmpGtF64:
    case IrOpcode::CmpGeF64:
    case IrOpcode::ConvertI32ToF64:
    case IrOpcode::ConvertI64ToF64:
    case IrOpcode::ConvertU64ToF64:
    case IrOpcode::ConvertF64ToI32:
    case IrOpcode::ConvertF64ToI64:
    case IrOpcode::ConvertF64ToU64:
    case IrOpcode::ConvertF32ToF64:
    case IrOpcode::ConvertF64ToF32:
      return true;
    default:
      return false;
  }
}

bool moduleUsesF64Helpers(const IrModule &module) {
  for (const IrFunction &function : module.functions) {
    for (const IrInstruction &instruction : function.instructions) {
      if (usesF64Helpers(instruction.op)) {
        return true;
      }
    }
  }
  return false;
}

bool usesClampI32ConvertHelpers(IrOpcode opcode) {
  switch (opcode) {
    case IrOpcode::ConvertF32ToI32:
    case IrOpcode::ConvertF64ToI32:
      return true;
    default:
      return false;
  }
}

bool moduleUsesClampI32ConvertHelpers(const IrModule &module) {
  for (const IrFunction &function : module.functions) {
    for (const IrInstruction &instruction : function.instructions) {
      if (usesClampI32ConvertHelpers(instruction.op)) {
        return true;
      }
    }
  }
  return false;
}

bool usesClampI64ConvertHelpers(IrOpcode opcode) {
  switch (opcode) {
    case IrOpcode::ConvertF32ToI64:
    case IrOpcode::ConvertF64ToI64:
      return true;
    default:
      return false;
  }
}

bool moduleUsesClampI64ConvertHelpers(const IrModule &module) {
  for (const IrFunction &function : module.functions) {
    for (const IrInstruction &instruction : function.instructions) {
      if (usesClampI64ConvertHelpers(instruction.op)) {
        return true;
      }
    }
  }
  return false;
}

bool usesClampU64ConvertHelpers(IrOpcode opcode) {
  switch (opcode) {
    case IrOpcode::ConvertF32ToU64:
    case IrOpcode::ConvertF64ToU64:
      return true;
    default:
      return false;
  }
}

bool moduleUsesClampU64ConvertHelpers(const IrModule &module) {
  for (const IrFunction &function : module.functions) {
    for (const IrInstruction &instruction : function.instructions) {
      if (usesClampU64ConvertHelpers(instruction.op)) {
        return true;
      }
    }
  }
  return false;
}

bool usesFileIoHelpers(IrOpcode opcode) {
  switch (opcode) {
    case IrOpcode::FileOpenRead:
    case IrOpcode::FileOpenWrite:
    case IrOpcode::FileOpenAppend:
    case IrOpcode::FileOpenReadDynamic:
    case IrOpcode::FileOpenWriteDynamic:
    case IrOpcode::FileOpenAppendDynamic:
    case IrOpcode::FileReadByte:
    case IrOpcode::FileClose:
    case IrOpcode::FileFlush:
    case IrOpcode::FileWriteI32:
    case IrOpcode::FileWriteI64:
    case IrOpcode::FileWriteU64:
    case IrOpcode::FileWriteString:
    case IrOpcode::FileWriteByte:
    case IrOpcode::FileWriteNewline:
      return true;
    default:
      return false;
  }
}

bool moduleUsesFileIoHelpers(const IrModule &module) {
  for (const IrFunction &function : module.functions) {
    for (const IrInstruction &instruction : function.instructions) {
      if (usesFileIoHelpers(instruction.op)) {
        return true;
      }
    }
  }
  return false;
}

std::string irFunctionSymbol(size_t functionIndex) {
  return "ps_fn_" + std::to_string(functionIndex);
}

std::string irFunctionChunkSymbol(size_t functionIndex, size_t chunkIndex) {
  return "ps_fn_" + std::to_string(functionIndex) + "_chunk_" + std::to_string(chunkIndex);
}

} // namespace

bool IrToCppEmitter::emitSource(const IrModule &module, std::string &out, std::string &error) const {
  out.clear();
  error.clear();
  if (module.entryIndex < 0 || static_cast<size_t>(module.entryIndex) >= module.functions.size()) {
    error = "IrToCppEmitter invalid IR entry index";
    return false;
  }
  const IrFunction &entry = module.functions[static_cast<size_t>(module.entryIndex)];
  if (entry.instructions.empty()) {
    error = "IrToCppEmitter entry function has no instructions";
    return false;
  }
  for (size_t functionIndex = 0; functionIndex < module.functions.size(); ++functionIndex) {
    if (module.functions[functionIndex].instructions.empty()) {
      error = "IrToCppEmitter function has no instructions at index " + std::to_string(functionIndex);
      return false;
    }
  }

  std::ostringstream body;
  const bool needsF32Helpers = moduleUsesF32Helpers(module);
  const bool needsF64Helpers = moduleUsesF64Helpers(module);
  const bool needsClampI32ConvertHelpers = moduleUsesClampI32ConvertHelpers(module);
  const bool needsClampI64ConvertHelpers = moduleUsesClampI64ConvertHelpers(module);
  const bool needsClampU64ConvertHelpers = moduleUsesClampU64ConvertHelpers(module);
  const bool needsFileIoHelpers = moduleUsesFileIoHelpers(module);
  body << "#include <algorithm>\n";
  body << "#include <cstddef>\n";
  body << "#include <cstdint>\n";
  body << "#include <limits>\n";
  body << "#include <string>\n";
  body << "#include <deque>\n";
  body << "#include <vector>\n";
  if (needsClampI32ConvertHelpers || needsClampI64ConvertHelpers || needsClampU64ConvertHelpers) {
    body << "#include <cmath>\n";
  }
  if (needsF32Helpers || needsF64Helpers) {
    body << "#include <cstring>\n";
  }
  if (needsFileIoHelpers) {
    body << "#include <cerrno>\n";
    body << "#include <fcntl.h>\n";
    body << "#include <unistd.h>\n";
  }
  body << "\n";
  body << "#include <iostream>\n\n";
  if (needsF32Helpers) {
    body << "static float psBitsToF32(uint64_t raw) {\n";
    body << "  uint32_t bits = static_cast<uint32_t>(raw);\n";
    body << "  float value = 0.0f;\n";
    body << "  std::memcpy(&value, &bits, sizeof(value));\n";
    body << "  return value;\n";
    body << "}\n\n";
    body << "static uint64_t psF32ToBits(float value) {\n";
    body << "  uint32_t bits = 0;\n";
    body << "  std::memcpy(&bits, &value, sizeof(bits));\n";
    body << "  return static_cast<uint64_t>(bits);\n";
    body << "}\n\n";
  }
  if (needsF64Helpers) {
    body << "static double psBitsToF64(uint64_t raw) {\n";
    body << "  double value = 0.0;\n";
    body << "  std::memcpy(&value, &raw, sizeof(value));\n";
    body << "  return value;\n";
    body << "}\n\n";
    body << "static uint64_t psF64ToBits(double value) {\n";
    body << "  uint64_t bits = 0;\n";
    body << "  std::memcpy(&bits, &value, sizeof(bits));\n";
    body << "  return bits;\n";
    body << "}\n\n";
  }
  if (needsClampI32ConvertHelpers) {
    body << "static int32_t psConvertF32ToI32(float value) {\n";
    body << "  if (std::isnan(value)) {\n";
    body << "    return 0;\n";
    body << "  }\n";
    body << "  if (value <= static_cast<float>(std::numeric_limits<int32_t>::min())) {\n";
    body << "    return std::numeric_limits<int32_t>::min();\n";
    body << "  }\n";
    body << "  if (value >= static_cast<float>(std::numeric_limits<int32_t>::max())) {\n";
    body << "    return std::numeric_limits<int32_t>::max();\n";
    body << "  }\n";
    body << "  return static_cast<int32_t>(value);\n";
    body << "}\n\n";
    body << "static int32_t psConvertF64ToI32(double value) {\n";
    body << "  if (std::isnan(value)) {\n";
    body << "    return 0;\n";
    body << "  }\n";
    body << "  if (value <= static_cast<double>(std::numeric_limits<int32_t>::min())) {\n";
    body << "    return std::numeric_limits<int32_t>::min();\n";
    body << "  }\n";
    body << "  if (value >= static_cast<double>(std::numeric_limits<int32_t>::max())) {\n";
    body << "    return std::numeric_limits<int32_t>::max();\n";
    body << "  }\n";
    body << "  return static_cast<int32_t>(value);\n";
    body << "}\n\n";
  }
  if (needsClampI64ConvertHelpers) {
    body << "static int64_t psConvertF32ToI64(float value) {\n";
    body << "  if (std::isnan(value)) {\n";
    body << "    return 0;\n";
    body << "  }\n";
    body << "  if (value <= static_cast<float>(std::numeric_limits<int64_t>::min())) {\n";
    body << "    return std::numeric_limits<int64_t>::min();\n";
    body << "  }\n";
    body << "  if (value >= static_cast<float>(std::numeric_limits<int64_t>::max())) {\n";
    body << "    return std::numeric_limits<int64_t>::max();\n";
    body << "  }\n";
    body << "  return static_cast<int64_t>(value);\n";
    body << "}\n\n";
    body << "static int64_t psConvertF64ToI64(double value) {\n";
    body << "  if (std::isnan(value)) {\n";
    body << "    return 0;\n";
    body << "  }\n";
    body << "  if (value <= static_cast<double>(std::numeric_limits<int64_t>::min())) {\n";
    body << "    return std::numeric_limits<int64_t>::min();\n";
    body << "  }\n";
    body << "  if (value >= static_cast<double>(std::numeric_limits<int64_t>::max())) {\n";
    body << "    return std::numeric_limits<int64_t>::max();\n";
    body << "  }\n";
    body << "  return static_cast<int64_t>(value);\n";
    body << "}\n\n";
  }
  if (needsClampU64ConvertHelpers) {
    body << "static uint64_t psConvertF32ToU64(float value) {\n";
    body << "  if (std::isnan(value) || value <= 0.0f) {\n";
    body << "    return 0u;\n";
    body << "  }\n";
    body << "  if (value >= static_cast<float>(std::numeric_limits<uint64_t>::max())) {\n";
    body << "    return std::numeric_limits<uint64_t>::max();\n";
    body << "  }\n";
    body << "  return static_cast<uint64_t>(value);\n";
    body << "}\n\n";
    body << "static uint64_t psConvertF64ToU64(double value) {\n";
    body << "  if (std::isnan(value) || value <= 0.0) {\n";
    body << "    return 0u;\n";
    body << "  }\n";
    body << "  if (value >= static_cast<double>(std::numeric_limits<uint64_t>::max())) {\n";
    body << "    return std::numeric_limits<uint64_t>::max();\n";
    body << "  }\n";
    body << "  return static_cast<uint64_t>(value);\n";
    body << "}\n\n";
  }
  if (needsFileIoHelpers) {
    body << "static uint32_t psWriteAll(int fd, const void *data, std::size_t size) {\n";
    body << "  const char *cursor = static_cast<const char *>(data);\n";
    body << "  std::size_t remaining = size;\n";
    body << "  while (remaining > 0) {\n";
    body << "    ssize_t wrote = ::write(fd, cursor, remaining);\n";
    body << "    if (wrote < 0) {\n";
    body << "      return errno == 0 ? 1u : static_cast<uint32_t>(errno);\n";
    body << "    }\n";
    body << "    if (wrote == 0) {\n";
    body << "      return 5u;\n";
    body << "    }\n";
    body << "    remaining -= static_cast<std::size_t>(wrote);\n";
    body << "    cursor += wrote;\n";
    body << "  }\n";
    body << "  return 0u;\n";
    body << "}\n\n";
  }
  body << "static const char *ps_string_table[] = {\n";
  if (module.stringTable.empty()) {
    body << "  \"\",\n";
  } else {
    for (const std::string &text : module.stringTable) {
      body << "  \"" << escapeCppStringLiteral(text) << "\",\n";
    }
  }
  body << "};\n\n";
  body << "static constexpr std::size_t ps_string_table_count = " << module.stringTable.size() << "u;\n\n";
  body << "static constexpr std::size_t ps_string_table_lengths[] = {\n";
  if (module.stringTable.empty()) {
    body << "  0u,\n";
  } else {
    for (const std::string &text : module.stringTable) {
      body << "  " << text.size() << "u,\n";
    }
  }
  body << "};\n\n";
  body << "static constexpr uint64_t ps_heap_address_tag = 1ull << 63;\n\n";
  body << "struct PsHeapAllocation {\n";
  body << "  std::size_t baseIndex = 0;\n";
  body << "  std::size_t slotCount = 0;\n";
  body << "  bool live = false;\n";
  body << "};\n\n";
  body << "static bool psResolveHeapSlot(uint64_t address,\n";
  body << "                              const std::vector<uint64_t> &heapSlots,\n";
  body << "                              const std::vector<PsHeapAllocation> &heapAllocations,\n";
  body << "                              std::size_t &slotIndexOut) {\n";
  body << "  if ((address & ps_heap_address_tag) == 0ull || (address % " << IrSlotBytes << "ull) != 0ull) {\n";
  body << "    return false;\n";
  body << "  }\n";
  body << "  uint64_t heapAddress = address & ~ps_heap_address_tag;\n";
  body << "  uint64_t heapIndex = heapAddress / " << IrSlotBytes << "ull;\n";
  body << "  if (heapIndex >= heapSlots.size()) {\n";
  body << "    return false;\n";
  body << "  }\n";
  body << "  for (const auto &allocation : heapAllocations) {\n";
  body << "    if (!allocation.live) {\n";
  body << "      continue;\n";
  body << "    }\n";
  body << "    uint64_t baseIndex = static_cast<uint64_t>(allocation.baseIndex);\n";
  body << "    uint64_t endIndex = baseIndex + static_cast<uint64_t>(allocation.slotCount);\n";
  body << "    if (heapIndex >= baseIndex && heapIndex < endIndex) {\n";
  body << "      slotIndexOut = static_cast<std::size_t>(heapIndex);\n";
  body << "      return true;\n";
  body << "    }\n";
  body << "  }\n";
  body << "  return false;\n";
  body << "}\n\n";
  body << "static bool psHeapAlloc(uint64_t slotCount,\n";
  body << "                        std::vector<uint64_t> &heapSlots,\n";
  body << "                        std::vector<PsHeapAllocation> &heapAllocations,\n";
  body << "                        uint64_t &addressOut) {\n";
  body << "  if (slotCount == 0ull) {\n";
  body << "    addressOut = 0ull;\n";
  body << "    return true;\n";
  body << "  }\n";
  body << "  if (slotCount > static_cast<uint64_t>(heapSlots.max_size() - heapSlots.size())) {\n";
  body << "    return false;\n";
  body << "  }\n";
  body << "  std::size_t baseIndex = heapSlots.size();\n";
  body << "  uint64_t maxAddressableIndex = (std::numeric_limits<uint64_t>::max() - ps_heap_address_tag) / "
       << IrSlotBytes << "ull;\n";
  body << "  if (baseIndex > maxAddressableIndex) {\n";
  body << "    return false;\n";
  body << "  }\n";
  body << "  heapSlots.resize(baseIndex + static_cast<std::size_t>(slotCount), 0ull);\n";
  body << "  heapAllocations.push_back({baseIndex, static_cast<std::size_t>(slotCount), true});\n";
  body << "  addressOut = ps_heap_address_tag + static_cast<uint64_t>(baseIndex) * " << IrSlotBytes << "ull;\n";
  body << "  return true;\n";
  body << "}\n\n";
  body << "static bool psHeapFree(uint64_t address,\n";
  body << "                       std::vector<uint64_t> &heapSlots,\n";
  body << "                       std::vector<PsHeapAllocation> &heapAllocations) {\n";
  body << "  if (address == 0ull) {\n";
  body << "    return true;\n";
  body << "  }\n";
  body << "  if ((address & ps_heap_address_tag) == 0ull || (address % " << IrSlotBytes << "ull) != 0ull) {\n";
  body << "    return false;\n";
  body << "  }\n";
  body << "  uint64_t heapAddress = address & ~ps_heap_address_tag;\n";
  body << "  std::size_t baseIndex = static_cast<std::size_t>(heapAddress / " << IrSlotBytes << "ull);\n";
  body << "  for (auto &allocation : heapAllocations) {\n";
  body << "    if (allocation.baseIndex != baseIndex) {\n";
  body << "      continue;\n";
  body << "    }\n";
  body << "    if (!allocation.live || allocation.baseIndex + allocation.slotCount > heapSlots.size()) {\n";
  body << "      return false;\n";
  body << "    }\n";
  body << "    for (std::size_t i = 0; i < allocation.slotCount; ++i) {\n";
  body << "      heapSlots[allocation.baseIndex + i] = 0ull;\n";
  body << "    }\n";
  body << "    allocation.live = false;\n";
  body << "    return true;\n";
  body << "  }\n";
  body << "  return false;\n";
  body << "}\n\n";
  body << "static bool psHeapRealloc(uint64_t address,\n";
  body << "                          uint64_t slotCount,\n";
  body << "                          std::vector<uint64_t> &heapSlots,\n";
  body << "                          std::vector<PsHeapAllocation> &heapAllocations,\n";
  body << "                          uint64_t &addressOut) {\n";
  body << "  if (address == 0ull) {\n";
  body << "    return psHeapAlloc(slotCount, heapSlots, heapAllocations, addressOut);\n";
  body << "  }\n";
  body << "  if (slotCount == 0ull) {\n";
  body << "    if (!psHeapFree(address, heapSlots, heapAllocations)) {\n";
  body << "      return false;\n";
  body << "    }\n";
  body << "    addressOut = 0ull;\n";
  body << "    return true;\n";
  body << "  }\n";
  body << "  if ((address & ps_heap_address_tag) == 0ull || (address % " << IrSlotBytes << "ull) != 0ull) {\n";
  body << "    return false;\n";
  body << "  }\n";
  body << "  uint64_t heapAddress = address & ~ps_heap_address_tag;\n";
  body << "  std::size_t baseIndex = static_cast<std::size_t>(heapAddress / " << IrSlotBytes << "ull);\n";
  body << "  for (auto &allocation : heapAllocations) {\n";
  body << "    if (allocation.baseIndex != baseIndex) {\n";
  body << "      continue;\n";
  body << "    }\n";
  body << "    if (!allocation.live || allocation.baseIndex + allocation.slotCount > heapSlots.size()) {\n";
  body << "      return false;\n";
  body << "    }\n";
  body << "    std::size_t oldBaseIndex = allocation.baseIndex;\n";
  body << "    std::size_t oldSlotCount = allocation.slotCount;\n";
  body << "    uint64_t newAddress = 0ull;\n";
  body << "    if (!psHeapAlloc(slotCount, heapSlots, heapAllocations, newAddress)) {\n";
  body << "      addressOut = 0ull;\n";
  body << "      return true;\n";
  body << "    }\n";
  body << "    std::size_t newBaseIndex = static_cast<std::size_t>((newAddress & ~ps_heap_address_tag) / "
       << IrSlotBytes << "ull);\n";
  body << "    std::size_t copySlots = std::min(oldSlotCount, static_cast<std::size_t>(slotCount));\n";
  body << "    for (std::size_t i = 0; i < copySlots; ++i) {\n";
  body << "      heapSlots[newBaseIndex + i] = heapSlots[oldBaseIndex + i];\n";
  body << "    }\n";
  body << "    for (std::size_t i = 0; i < oldSlotCount; ++i) {\n";
  body << "      heapSlots[oldBaseIndex + i] = 0ull;\n";
  body << "    }\n";
  body << "    for (auto &candidate : heapAllocations) {\n";
  body << "      if (candidate.baseIndex == oldBaseIndex) {\n";
  body << "        candidate.live = false;\n";
  body << "        break;\n";
  body << "      }\n";
  body << "    }\n";
  body << "    addressOut = newAddress;\n";
  body << "    return true;\n";
  body << "  }\n";
  body << "  return false;\n";
  body << "}\n\n";
  body << "struct PsStack {\n";
  body << "  explicit PsStack(std::size_t initialSize) : slots(initialSize, 0ull) {}\n";
  body << "  uint64_t &operator[](std::size_t index) {\n";
  body << "    if (index >= slots.size()) {\n";
  body << "      std::size_t newSize = slots.empty() ? 1024ull : slots.size();\n";
  body << "      while (index >= newSize) {\n";
  body << "        newSize *= 2ull;\n";
  body << "      }\n";
  body << "      slots.resize(newSize, 0ull);\n";
  body << "    }\n";
  body << "    return slots[index];\n";
  body << "  }\n";
  body << "  const uint64_t &operator[](std::size_t index) const {\n";
  body << "    return slots[index];\n";
  body << "  }\n";
  body << "  std::deque<uint64_t> slots;\n";
  body << "};\n\n";

  for (size_t functionIndex = 0; functionIndex < module.functions.size(); ++functionIndex) {
    body << "static int64_t " << irFunctionSymbol(functionIndex)
         << "(PsStack &stack, std::size_t &sp, std::vector<uint64_t> &heapSlots, "
            "std::vector<PsHeapAllocation> &heapAllocations, int argc, char **argv);\n";
  }
  body << "\n";

  std::vector<size_t> stringLengths;
  stringLengths.reserve(module.stringTable.size());
  for (const std::string &text : module.stringTable) {
    stringLengths.push_back(text.size());
  }
  const EmitContext context{
      .stringCount = module.stringTable.size(),
      .functionCount = module.functions.size(),
      .stringLengths = std::move(stringLengths),
  };
  constexpr size_t DispatchChunkSize = 8192ull;
  for (size_t functionIndex = 0; functionIndex < module.functions.size(); ++functionIndex) {
    const IrFunction &function = module.functions[functionIndex];
    const size_t localCount = computeLocalCount(function);
    const size_t instructionCount = function.instructions.size();
    const size_t chunkCount = (instructionCount + DispatchChunkSize - 1ull) / DispatchChunkSize;

    for (size_t chunkIndex = 0; chunkIndex < chunkCount; ++chunkIndex) {
      const size_t chunkBegin = chunkIndex * DispatchChunkSize;
      const size_t chunkEnd = std::min(chunkBegin + DispatchChunkSize, instructionCount);
      body << "static int64_t " << irFunctionChunkSymbol(functionIndex, chunkIndex)
           << "(PsStack &stack, std::size_t &sp, std::vector<uint64_t> &locals, "
              "std::vector<uint64_t> &heapSlots, std::vector<PsHeapAllocation> &heapAllocations, "
              "std::size_t &pc, int argc, char **argv, bool &transfer) {\n";
      body << "  while (true) {\n";
      body << "    switch (pc) {\n";

      for (size_t i = chunkBegin; i < chunkEnd; ++i) {
        const size_t next = i + 1;
        body << "      case " << i << ": {\n";
        if (!emitInstruction(function.instructions[i], i, next, instructionCount, localCount, context, body, error)) {
          return false;
        }
        body << "      }\n";
      }

      body << "      default:\n";
      body << "        transfer = true;\n";
      body << "        return 0;\n";
      body << "    }\n";
      body << "  }\n";
      body << "}\n\n";
    }

    body << "static int64_t " << irFunctionSymbol(functionIndex)
         << "(PsStack &stack, std::size_t &sp, std::vector<uint64_t> &heapSlots, "
            "std::vector<PsHeapAllocation> &heapAllocations, int argc, char **argv) {\n";
    body << "  std::vector<uint64_t> locals(" << localCount << "ull, 0ull);\n";
    body << "  std::size_t pc = 0;\n";
    body << "  while (true) {\n";
    body << "    bool transfer = false;\n";
    for (size_t chunkIndex = 0; chunkIndex < chunkCount; ++chunkIndex) {
      const size_t chunkEnd = std::min((chunkIndex + 1u) * DispatchChunkSize, instructionCount);
      body << "    if (pc < " << chunkEnd << "ull) {\n";
      body << "      int64_t chunkResult = " << irFunctionChunkSymbol(functionIndex, chunkIndex)
           << "(stack, sp, locals, heapSlots, heapAllocations, pc, argc, argv, transfer);\n";
      body << "      if (!transfer) {\n";
      body << "        return chunkResult;\n";
      body << "      }\n";
      body << "      continue;\n";
      body << "    }\n";
    }
    body << "    return 0;\n";
    body << "  }\n";
    body << "}\n\n";
  }

  body << "static int64_t ps_entry_" << static_cast<size_t>(module.entryIndex) << "(int argc, char **argv) {\n";
  body << "  PsStack stack(1024ull);\n";
  body << "  std::size_t sp = 0;\n";
  body << "  std::vector<uint64_t> heapSlots;\n";
  body << "  std::vector<PsHeapAllocation> heapAllocations;\n";
  body << "  return " << irFunctionSymbol(static_cast<size_t>(module.entryIndex))
       << "(stack, sp, heapSlots, heapAllocations, argc, argv);\n";
  body << "}\n\n";
  body << "int main(int argc, char **argv) {\n";
  body << "  return static_cast<int>(ps_entry_" << static_cast<size_t>(module.entryIndex) << "(argc, argv));\n";
  body << "}\n";

  out = body.str();
  return true;
}

} // namespace primec
