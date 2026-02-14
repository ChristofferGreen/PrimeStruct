#include "primec/IrSerializer.h"

#include <cstring>
#include <limits>

namespace primec {
namespace {
constexpr uint32_t IrMagic = 0x50534952; // "PSIR"
constexpr uint32_t IrVersion = 12;

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
} // namespace

bool serializeIr(const IrModule &module, std::vector<uint8_t> &out, std::string &error) {
  out.clear();
  if (module.entryIndex < 0 || static_cast<size_t>(module.entryIndex) >= module.functions.size()) {
    error = "invalid IR entry index";
    return false;
  }
  appendU32(out, IrMagic);
  appendU32(out, IrVersion);
  appendU32(out, static_cast<uint32_t>(module.functions.size()));
  appendU32(out, static_cast<uint32_t>(module.entryIndex));
  if (module.stringTable.size() > static_cast<size_t>(std::numeric_limits<uint32_t>::max())) {
    error = "too many strings for IR serialization";
    return false;
  }
  appendU32(out, static_cast<uint32_t>(module.stringTable.size()));
  for (const auto &text : module.stringTable) {
    if (text.size() > static_cast<size_t>(std::numeric_limits<uint32_t>::max())) {
      error = "string literal too long for IR serialization";
      return false;
    }
    appendU32(out, static_cast<uint32_t>(text.size()));
    out.insert(out.end(), text.begin(), text.end());
  }
  if (module.structLayouts.size() > static_cast<size_t>(std::numeric_limits<uint32_t>::max())) {
    error = "too many struct layouts for IR serialization";
    return false;
  }
  appendU32(out, static_cast<uint32_t>(module.structLayouts.size()));
  for (const auto &layout : module.structLayouts) {
    if (layout.name.size() > static_cast<size_t>(std::numeric_limits<uint32_t>::max())) {
      error = "struct name too long for IR serialization";
      return false;
    }
    appendU32(out, static_cast<uint32_t>(layout.name.size()));
    out.insert(out.end(), layout.name.begin(), layout.name.end());
    appendU32(out, layout.totalSizeBytes);
    appendU32(out, layout.alignmentBytes);
    if (layout.fields.size() > static_cast<size_t>(std::numeric_limits<uint32_t>::max())) {
      error = "too many struct fields for IR serialization";
      return false;
    }
    appendU32(out, static_cast<uint32_t>(layout.fields.size()));
    for (const auto &field : layout.fields) {
      if (field.name.size() > static_cast<size_t>(std::numeric_limits<uint32_t>::max())) {
        error = "struct field name too long for IR serialization";
        return false;
      }
      appendU32(out, static_cast<uint32_t>(field.name.size()));
      out.insert(out.end(), field.name.begin(), field.name.end());
      if (field.envelope.size() > static_cast<size_t>(std::numeric_limits<uint32_t>::max())) {
        error = "struct field envelope too long for IR serialization";
        return false;
      }
      appendU32(out, static_cast<uint32_t>(field.envelope.size()));
      out.insert(out.end(), field.envelope.begin(), field.envelope.end());
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
    if (fn.name.size() > static_cast<size_t>(std::numeric_limits<uint32_t>::max())) {
      error = "function name too long for IR serialization";
      return false;
    }
    appendU32(out, static_cast<uint32_t>(fn.name.size()));
    out.insert(out.end(), fn.name.begin(), fn.name.end());
    if (fn.instructions.size() > static_cast<size_t>(std::numeric_limits<uint32_t>::max())) {
      error = "too many IR instructions";
      return false;
    }
    appendU32(out, static_cast<uint32_t>(fn.instructions.size()));
    for (const auto &inst : fn.instructions) {
      out.push_back(static_cast<uint8_t>(inst.op));
      appendU64(out, inst.imm);
    }
  }
  return true;
}

bool deserializeIr(const std::vector<uint8_t> &data, IrModule &out, std::string &error) {
  out = IrModule{};
  size_t offset = 0;
  uint32_t magic = 0;
  if (!readU32(data, offset, magic) || magic != IrMagic) {
    error = "invalid IR header";
    return false;
  }
  uint32_t version = 0;
  if (!readU32(data, offset, version) || version != IrVersion) {
    error = "unsupported IR version";
    return false;
  }
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
    uint32_t len = 0;
    if (!readU32(data, offset, len)) {
      error = "truncated IR string length";
      return false;
    }
    if (offset + len > data.size()) {
      error = "truncated IR string";
      return false;
    }
    out.stringTable.emplace_back(reinterpret_cast<const char *>(data.data() + offset), len);
    offset += len;
  }
  uint32_t structCount = 0;
  if (!readU32(data, offset, structCount)) {
    error = "truncated IR struct layout count";
    return false;
  }
  out.structLayouts.reserve(structCount);
  for (uint32_t i = 0; i < structCount; ++i) {
    uint32_t nameLen = 0;
    if (!readU32(data, offset, nameLen)) {
      error = "truncated IR struct name";
      return false;
    }
    if (offset + nameLen > data.size()) {
      error = "truncated IR struct name";
      return false;
    }
    std::string name(reinterpret_cast<const char *>(data.data() + offset), nameLen);
    offset += nameLen;
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
      uint32_t fieldNameLen = 0;
      if (!readU32(data, offset, fieldNameLen)) {
        error = "truncated IR struct field name";
        return false;
      }
      if (offset + fieldNameLen > data.size()) {
        error = "truncated IR struct field name";
        return false;
      }
      std::string fieldName(reinterpret_cast<const char *>(data.data() + offset), fieldNameLen);
      offset += fieldNameLen;
      uint32_t envelopeLen = 0;
      if (!readU32(data, offset, envelopeLen)) {
        error = "truncated IR struct field envelope";
        return false;
      }
      if (offset + envelopeLen > data.size()) {
        error = "truncated IR struct field envelope";
        return false;
      }
      std::string envelope(reinterpret_cast<const char *>(data.data() + offset), envelopeLen);
      offset += envelopeLen;
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
      field.visibility = static_cast<IrStructVisibility>(visibility);
      field.isStatic = (isStatic != 0u);
      layout.fields.push_back(std::move(field));
    }
    out.structLayouts.push_back(std::move(layout));
  }
  out.functions.reserve(funcCount);
  for (uint32_t i = 0; i < funcCount; ++i) {
    uint32_t nameLen = 0;
    if (!readU32(data, offset, nameLen)) {
      error = "truncated IR function header";
      return false;
    }
    if (offset + nameLen > data.size()) {
      error = "truncated IR function name";
      return false;
    }
    std::string name(reinterpret_cast<const char *>(data.data() + offset), nameLen);
    offset += nameLen;
    uint32_t instCount = 0;
    if (!readU32(data, offset, instCount)) {
      error = "truncated IR instruction count";
      return false;
    }
    IrFunction fn;
    fn.name = std::move(name);
    fn.instructions.reserve(instCount);
    for (uint32_t instIndex = 0; instIndex < instCount; ++instIndex) {
      if (offset >= data.size()) {
        error = "truncated IR instruction";
        return false;
      }
      IrInstruction inst;
      inst.op = static_cast<IrOpcode>(data[offset]);
      offset += 1;
      if (!readU64(data, offset, inst.imm)) {
        error = "truncated IR instruction data";
        return false;
      }
      fn.instructions.push_back(inst);
    }
    out.functions.push_back(std::move(fn));
  }
  if (entryIndex >= out.functions.size()) {
    error = "invalid IR entry index";
    return false;
  }
  out.entryIndex = static_cast<int32_t>(entryIndex);
  return true;
}

} // namespace primec
