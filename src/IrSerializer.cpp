#include "primec/IrSerializer.h"

#include <cstring>
#include <limits>
#include <string>
#include <string_view>

namespace primec {
namespace {
void appendU32(std::vector<uint8_t> &out, uint32_t value) {
  out.push_back(static_cast<uint8_t>(value & 0xFF));
  out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
  out.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
  out.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
}

void appendU64(std::vector<uint8_t> &out, uint64_t value) {
  out.push_back(static_cast<uint8_t>(value & 0xFF));
  out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
  out.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
  out.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
  out.push_back(static_cast<uint8_t>((value >> 32) & 0xFF));
  out.push_back(static_cast<uint8_t>((value >> 40) & 0xFF));
  out.push_back(static_cast<uint8_t>((value >> 48) & 0xFF));
  out.push_back(static_cast<uint8_t>((value >> 56) & 0xFF));
}

bool appendString(std::vector<uint8_t> &out,
                  const std::string &text,
                  std::string_view errorContext,
                  std::string &error) {
  if (text.size() > static_cast<size_t>(std::numeric_limits<uint32_t>::max())) {
    error = std::string(errorContext) + " too long for IR serialization";
    return false;
  }
  appendU32(out, static_cast<uint32_t>(text.size()));
  out.insert(out.end(), text.begin(), text.end());
  return true;
}

bool readU32(const std::vector<uint8_t> &data, size_t &offset, uint32_t &outValue) {
  if (offset + 4 > data.size()) {
    return false;
  }
  outValue = static_cast<uint32_t>(data[offset]) |
             (static_cast<uint32_t>(data[offset + 1]) << 8) |
             (static_cast<uint32_t>(data[offset + 2]) << 16) |
             (static_cast<uint32_t>(data[offset + 3]) << 24);
  offset += 4;
  return true;
}


bool readU64(const std::vector<uint8_t> &data, size_t &offset, uint64_t &outValue) {
  if (offset + 8 > data.size()) {
    return false;
  }
  outValue = static_cast<uint64_t>(data[offset]) |
             (static_cast<uint64_t>(data[offset + 1]) << 8) |
             (static_cast<uint64_t>(data[offset + 2]) << 16) |
             (static_cast<uint64_t>(data[offset + 3]) << 24) |
             (static_cast<uint64_t>(data[offset + 4]) << 32) |
             (static_cast<uint64_t>(data[offset + 5]) << 40) |
             (static_cast<uint64_t>(data[offset + 6]) << 48) |
             (static_cast<uint64_t>(data[offset + 7]) << 56);
  offset += 8;
  return true;
}

bool readU8(const std::vector<uint8_t> &data, size_t &offset, uint8_t &outValue) {
  if (offset >= data.size()) {
    return false;
  }
  outValue = data[offset];
  ++offset;
  return true;
}

bool readString(const std::vector<uint8_t> &data,
                size_t &offset,
                std::string &outValue,
                std::string_view truncatedLengthError,
                std::string_view truncatedTextError,
                std::string &error) {
  uint32_t len = 0;
  if (!readU32(data, offset, len)) {
    error = truncatedLengthError;
    return false;
  }
  if (offset + len > data.size()) {
    error = truncatedTextError;
    return false;
  }
  outValue.assign(reinterpret_cast<const char *>(data.data() + offset), len);
  offset += len;
  return true;
}
} // namespace

bool serializeIr(const IrModule &module, std::vector<uint8_t> &out, std::string &error) {
  out.clear();
  if (module.schemaVersion != IrSchemaVersion) {
    error = "unsupported IR schema version " + std::to_string(module.schemaVersion) +
            " for serialization (supported " + std::to_string(IrSchemaVersion) + ")";
    return false;
  }
  if (module.entryIndex < 0 || static_cast<size_t>(module.entryIndex) >= module.functions.size()) {
    error = "invalid IR entry index";
    return false;
  }
  appendU32(out, IrSchemaMagic);
  appendU32(out, IrSchemaVersion);
  appendU32(out, static_cast<uint32_t>(module.functions.size()));
  appendU32(out, static_cast<uint32_t>(module.entryIndex));
  if (module.stringTable.size() > static_cast<size_t>(std::numeric_limits<uint32_t>::max())) {
    error = "too many strings for IR serialization";
    return false;
  }
  appendU32(out, static_cast<uint32_t>(module.stringTable.size()));
  for (const auto &text : module.stringTable) {
    if (!appendString(out, text, "string literal", error)) {
      return false;
    }
  }
  if (module.structLayouts.size() > static_cast<size_t>(std::numeric_limits<uint32_t>::max())) {
    error = "too many struct layouts for IR serialization";
    return false;
  }
  appendU32(out, static_cast<uint32_t>(module.structLayouts.size()));
  for (const auto &layout : module.structLayouts) {
    if (!appendString(out, layout.name, "struct name", error)) {
      return false;
    }
    appendU32(out, layout.totalSizeBytes);
    appendU32(out, layout.alignmentBytes);
    if (layout.fields.size() > static_cast<size_t>(std::numeric_limits<uint32_t>::max())) {
      error = "too many struct fields for IR serialization";
      return false;
    }
    appendU32(out, static_cast<uint32_t>(layout.fields.size()));
    for (const auto &field : layout.fields) {
      if (!appendString(out, field.name, "struct field name", error)) {
        return false;
      }
      if (!appendString(out, field.envelope, "struct field envelope", error)) {
        return false;
      }
      appendU32(out, field.offsetBytes);
      appendU32(out, field.sizeBytes);
      appendU32(out, field.alignmentBytes);
      appendU32(out, static_cast<uint32_t>(field.paddingKind));
      appendU32(out, static_cast<uint32_t>(field.category));
      appendU32(out, static_cast<uint32_t>(field.visibility));
      appendU32(out, static_cast<uint32_t>(field.isStatic ? 1u : 0u));
    }
  }
  for (const auto &fn : module.functions) {
    if (!appendString(out, fn.name, "function name", error)) {
      return false;
    }
    appendU64(out, fn.metadata.effectMask);
    appendU64(out, fn.metadata.capabilityMask);
    appendU32(out, static_cast<uint32_t>(fn.metadata.schedulingScope));
    appendU32(out, fn.metadata.instrumentationFlags);
    if (fn.localDebugSlots.size() > static_cast<size_t>(std::numeric_limits<uint32_t>::max())) {
      error = "too many local debug slots for IR serialization";
      return false;
    }
    appendU32(out, static_cast<uint32_t>(fn.localDebugSlots.size()));
    for (const auto &slot : fn.localDebugSlots) {
      appendU32(out, slot.slotIndex);
      if (!appendString(out, slot.name, "local debug slot name", error)) {
        return false;
      }
      if (!appendString(out, slot.typeName, "local debug slot type", error)) {
        return false;
      }
    }
    if (fn.instructions.size() > static_cast<size_t>(std::numeric_limits<uint32_t>::max())) {
      error = "too many IR instructions";
      return false;
    }
    appendU32(out, static_cast<uint32_t>(fn.instructions.size()));
    for (const auto &inst : fn.instructions) {
      out.push_back(static_cast<uint8_t>(inst.op));
      appendU64(out, inst.imm);
      appendU32(out, inst.debugId);
    }
  }
  if (module.instructionSourceMap.size() > static_cast<size_t>(std::numeric_limits<uint32_t>::max())) {
    error = "too many IR instruction source map entries";
    return false;
  }
  appendU32(out, static_cast<uint32_t>(module.instructionSourceMap.size()));
  for (const auto &entry : module.instructionSourceMap) {
    appendU32(out, entry.debugId);
    appendU32(out, entry.line);
    appendU32(out, entry.column);
    out.push_back(static_cast<uint8_t>(entry.provenance));
    if (!appendString(out, entry.sourceUnit, "instruction source map source unit", error)) {
      return false;
    }
  }
  return true;
}

bool deserializeIr(const std::vector<uint8_t> &data, IrModule &out, std::string &error) {
  out = IrModule{};
  size_t offset = 0;
  uint32_t magic = 0;
  if (!readU32(data, offset, magic) || magic != IrSchemaMagic) {
    error = "invalid IR header";
    return false;
  }
  uint32_t version = 0;
  if (!readU32(data, offset, version)) {
    error = "truncated IR schema version";
    return false;
  }
  if (version < IrSchemaMinimumSupportedVersion || version > IrSchemaMaximumSupportedVersion) {
    error = "unsupported IR schema version " + std::to_string(version) + " (supported " +
            std::to_string(IrSchemaMinimumSupportedVersion);
    if (IrSchemaMinimumSupportedVersion != IrSchemaMaximumSupportedVersion) {
      error += ".." + std::to_string(IrSchemaMaximumSupportedVersion);
    }
    error += ")";
    return false;
  }
  out.schemaVersion = version;
  uint32_t funcCount = 0;
  uint32_t entryIndex = 0;
  if (!readU32(data, offset, funcCount) || !readU32(data, offset, entryIndex)) {
    error = "truncated IR header";
    return false;
  }
  uint32_t stringCount = 0;
  if (!readU32(data, offset, stringCount)) {
    error = "truncated IR string table";
    return false;
  }
  out.stringTable.reserve(stringCount);
  for (uint32_t i = 0; i < stringCount; ++i) {
    std::string text;
    if (!readString(data, offset, text, "truncated IR string length", "truncated IR string", error)) {
      return false;
    }
    out.stringTable.push_back(std::move(text));
  }
  uint32_t structCount = 0;
  if (!readU32(data, offset, structCount)) {
    error = "truncated IR struct layout count";
    return false;
  }
  out.structLayouts.reserve(structCount);
  for (uint32_t i = 0; i < structCount; ++i) {
    std::string name;
    if (!readString(data, offset, name, "truncated IR struct name", "truncated IR struct name", error)) {
      return false;
    }
    uint32_t totalSize = 0;
    uint32_t alignment = 0;
    uint32_t fieldCount = 0;
    if (!readU32(data, offset, totalSize) || !readU32(data, offset, alignment) ||
        !readU32(data, offset, fieldCount)) {
      error = "truncated IR struct layout header";
      return false;
    }
    IrStructLayout layout;
    layout.name = std::move(name);
    layout.totalSizeBytes = totalSize;
    layout.alignmentBytes = alignment;
    layout.fields.reserve(fieldCount);
    for (uint32_t fieldIndex = 0; fieldIndex < fieldCount; ++fieldIndex) {
      std::string fieldName;
      if (!readString(data,
                      offset,
                      fieldName,
                      "truncated IR struct field name",
                      "truncated IR struct field name",
                      error)) {
        return false;
      }
      std::string envelope;
      if (!readString(data,
                      offset,
                      envelope,
                      "truncated IR struct field envelope",
                      "truncated IR struct field envelope",
                      error)) {
        return false;
      }
      uint32_t offsetBytes = 0;
      uint32_t sizeBytes = 0;
      uint32_t alignmentBytes = 0;
      uint32_t paddingKind = 0;
      uint32_t category = 0;
      uint32_t visibility = 0;
      uint32_t isStatic = 0;
      if (!readU32(data, offset, offsetBytes) || !readU32(data, offset, sizeBytes) ||
          !readU32(data, offset, alignmentBytes) || !readU32(data, offset, paddingKind) ||
          !readU32(data, offset, category) || !readU32(data, offset, visibility) ||
          !readU32(data, offset, isStatic)) {
        error = "truncated IR struct field data";
        return false;
      }
      IrStructField field;
      field.name = std::move(fieldName);
      field.envelope = std::move(envelope);
      field.offsetBytes = offsetBytes;
      field.sizeBytes = sizeBytes;
      field.alignmentBytes = alignmentBytes;
      field.paddingKind = static_cast<IrStructPaddingKind>(paddingKind);
      field.category = static_cast<IrStructFieldCategory>(category);
      if (visibility == 1u) {
        visibility = static_cast<uint32_t>(IrStructVisibility::Public);
      }
      field.visibility = static_cast<IrStructVisibility>(visibility);
      field.isStatic = (isStatic != 0u);
      layout.fields.push_back(std::move(field));
    }
    out.structLayouts.push_back(std::move(layout));
  }
  out.functions.reserve(funcCount);
  for (uint32_t i = 0; i < funcCount; ++i) {
    std::string name;
    if (!readString(data, offset, name, "truncated IR function header", "truncated IR function name", error)) {
      return false;
    }
    uint64_t effectMask = 0;
    uint64_t capabilityMask = 0;
    uint32_t schedulingScope = 0;
    uint32_t instrumentationFlags = 0;
    if (!readU64(data, offset, effectMask) || !readU64(data, offset, capabilityMask) ||
        !readU32(data, offset, schedulingScope) || !readU32(data, offset, instrumentationFlags)) {
      error = "truncated IR function metadata";
      return false;
    }
    uint32_t localDebugCount = 0;
    if (!readU32(data, offset, localDebugCount)) {
      error = "truncated IR local debug metadata count";
      return false;
    }
    IrFunction fn;
    fn.name = std::move(name);
    fn.metadata.effectMask = effectMask;
    fn.metadata.capabilityMask = capabilityMask;
    fn.metadata.schedulingScope = static_cast<IrSchedulingScope>(schedulingScope);
    fn.metadata.instrumentationFlags = instrumentationFlags;
    fn.localDebugSlots.reserve(localDebugCount);
    for (uint32_t localIndex = 0; localIndex < localDebugCount; ++localIndex) {
      uint32_t slotIndex = 0;
      if (!readU32(data, offset, slotIndex)) {
        error = "truncated IR local debug metadata entry";
        return false;
      }
      std::string localName;
      if (!readString(data,
                      offset,
                      localName,
                      "truncated IR local debug metadata entry",
                      "truncated IR local debug metadata name",
                      error)) {
        return false;
      }
      std::string typeName;
      if (!readString(data,
                      offset,
                      typeName,
                      "truncated IR local debug metadata type length",
                      "truncated IR local debug metadata type",
                      error)) {
        return false;
      }

      IrLocalDebugSlot slot;
      slot.slotIndex = slotIndex;
      slot.name = std::move(localName);
      slot.typeName = std::move(typeName);
      fn.localDebugSlots.push_back(std::move(slot));
    }
    uint32_t instCount = 0;
    if (!readU32(data, offset, instCount)) {
      error = "truncated IR instruction count";
      return false;
    }
    fn.instructions.reserve(instCount);
    for (uint32_t instIndex = 0; instIndex < instCount; ++instIndex) {
      if (offset >= data.size()) {
        error = "truncated IR instruction";
        return false;
      }
      IrInstruction inst;
      const uint8_t opcodeValue = data[offset];
      const uint8_t minOpcode = static_cast<uint8_t>(IrOpcode::PushI32);
      const uint8_t maxOpcode = static_cast<uint8_t>(IrOpcode::HeapRealloc);
      if (opcodeValue < minOpcode || opcodeValue > maxOpcode) {
        error = "unsupported IR opcode";
        return false;
      }
      inst.op = static_cast<IrOpcode>(opcodeValue);
      offset += 1;
      if (!readU64(data, offset, inst.imm)) {
        error = "truncated IR instruction data";
        return false;
      }
      if (!readU32(data, offset, inst.debugId)) {
        error = "truncated IR instruction debug id";
        return false;
      }
      fn.instructions.push_back(inst);
    }
    out.functions.push_back(std::move(fn));
  }
  uint32_t sourceMapCount = 0;
  if (!readU32(data, offset, sourceMapCount)) {
    error = "truncated IR instruction source map count";
    return false;
  }
  out.instructionSourceMap.reserve(sourceMapCount);
  for (uint32_t sourceMapIndex = 0; sourceMapIndex < sourceMapCount; ++sourceMapIndex) {
    IrInstructionSourceMapEntry entry;
    uint8_t provenance = 0;
    if (!readU32(data, offset, entry.debugId) || !readU32(data, offset, entry.line) ||
        !readU32(data, offset, entry.column) || !readU8(data, offset, provenance)) {
      error = "truncated IR instruction source map entry";
      return false;
    }
    if (provenance > static_cast<uint8_t>(IrSourceMapProvenance::SyntheticIr)) {
      error = "unsupported IR instruction source map provenance";
      return false;
    }
    entry.provenance = static_cast<IrSourceMapProvenance>(provenance);
    if (!readString(data,
                    offset,
                    entry.sourceUnit,
                    "truncated IR instruction source map source unit",
                    "truncated IR instruction source map source unit",
                    error)) {
      return false;
    }
    out.instructionSourceMap.push_back(entry);
  }
  if (entryIndex >= out.functions.size()) {
    error = "invalid IR entry index";
    return false;
  }
  out.entryIndex = static_cast<int32_t>(entryIndex);
  return true;
}

} // namespace primec
