#include "primec/ir/IrSerializer.h"
#include "primec/ir/IrOpcodeTable.h"

#include <cstring>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_map>

namespace primec {
namespace {
// The entry index of a module with no entry point (v38), as written.
constexpr uint32_t NoEntryIndex = 0xFFFFFFFFu;
static_assert(IrOpcodeMax < IrStringPushI32Code,
              "the opcode bytes of marked string pushes must stay free");

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

// An unsigned LEB128 number: seven bits per byte, low bits first, the high bit set on every byte but the last.
void appendVarint(std::vector<uint8_t> &out, uint64_t value) {
  while (value >= 0x80u) {
    out.push_back(static_cast<uint8_t>(value | 0x80u));
    value >>= 7;
  }
  out.push_back(static_cast<uint8_t>(value));
}

// A signed number as an unsigned one with small magnitudes small: 0, -1, 1, -2 become 0, 1, 2, 3.
uint64_t zigzagEncode(int64_t value) {
  return (static_cast<uint64_t>(value) << 1) ^ static_cast<uint64_t>(value >> 63);
}

int64_t zigzagDecode(uint64_t value) {
  return static_cast<int64_t>(value >> 1) ^ -static_cast<int64_t>(value & 1u);
}

// Every serialized element occupies at least one byte, so a count larger than
// the bytes left is corrupt. Checking before `reserve` keeps hostile counts from
// forcing huge allocations (std::bad_alloc) while loading untrusted bytecode.
bool countFitsRemaining(const std::vector<uint8_t> &data, size_t offset, uint64_t count) {
  return offset <= data.size() && count <= data.size() - offset;
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

bool readVarint(const std::vector<uint8_t> &data, size_t &offset, uint64_t &outValue) {
  outValue = 0;
  for (uint32_t shift = 0; shift < 64; shift += 7) {
    if (offset >= data.size()) {
      return false;
    }
    const uint8_t byte = data[offset];
    ++offset;
    outValue |= static_cast<uint64_t>(byte & 0x7Fu) << shift;
    if ((byte & 0x80u) == 0) {
      return true;
    }
  }
  return false;
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
// The instruction source map (v41): the entry count, and when it is not zero the distinct source units once, then each entry as the debug id (the difference
// to the entry before), the line, the column, the provenance byte and the index of its unit. Before v41 every entry
// carried its whole unit string, which made the map most of a large module.
bool appendSourceMap(std::vector<uint8_t> &out,
                     const std::vector<IrInstructionSourceMapEntry> &entries,
                     std::string &error) {
  if (entries.size() > static_cast<size_t>(std::numeric_limits<uint32_t>::max())) {
    error = "too many IR instruction source map entries";
    return false;
  }
  std::vector<const std::string *> units;
  std::unordered_map<std::string_view, uint32_t> unitIndexes;
  std::vector<uint32_t> entryUnits;
  entryUnits.reserve(entries.size());
  for (const IrInstructionSourceMapEntry &entry : entries) {
    const auto inserted = unitIndexes.emplace(std::string_view(entry.sourceUnit),
                                              static_cast<uint32_t>(units.size()));
    if (inserted.second) {
      units.push_back(&entry.sourceUnit);
    }
    entryUnits.push_back(inserted.first->second);
  }
  appendU32(out, static_cast<uint32_t>(entries.size()));
  if (entries.empty()) {
    return true;
  }
  appendU32(out, static_cast<uint32_t>(units.size()));
  for (const std::string *unit : units) {
    if (!appendString(out, *unit, "instruction source map source unit", error)) {
      return false;
    }
  }
  int64_t previousDebugId = 0;
  for (size_t index = 0; index < entries.size(); ++index) {
    const IrInstructionSourceMapEntry &entry = entries[index];
    appendVarint(out, zigzagEncode(static_cast<int64_t>(entry.debugId) - previousDebugId));
    previousDebugId = static_cast<int64_t>(entry.debugId);
    appendVarint(out, entry.line);
    appendVarint(out, entry.column);
    out.push_back(static_cast<uint8_t>(entry.provenance));
    appendVarint(out, entryUnits[index]);
  }
  return true;
}

bool checkSourceMapProvenance(uint8_t provenance, std::string &error) {
  if (provenance > static_cast<uint8_t>(IrSourceMapProvenance::SyntheticIr)) {
    error = "unsupported IR instruction source map provenance";
    return false;
  }
  return true;
}

// The map of a module before v41: every entry holds its unit string.
bool readSourceMapV40(const std::vector<uint8_t> &data,
                      size_t &offset,
                      std::vector<IrInstructionSourceMapEntry> &entries,
                      std::string &error) {
  uint32_t sourceMapCount = 0;
  if (!readU32(data, offset, sourceMapCount) || !countFitsRemaining(data, offset, sourceMapCount)) {
    error = "truncated IR instruction source map count";
    return false;
  }
  entries.reserve(sourceMapCount);
  for (uint32_t sourceMapIndex = 0; sourceMapIndex < sourceMapCount; ++sourceMapIndex) {
    IrInstructionSourceMapEntry entry;
    uint8_t provenance = 0;
    if (!readU32(data, offset, entry.debugId) || !readU32(data, offset, entry.line) ||
        !readU32(data, offset, entry.column) || !readU8(data, offset, provenance)) {
      error = "truncated IR instruction source map entry";
      return false;
    }
    if (!checkSourceMapProvenance(provenance, error)) {
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
    entries.push_back(std::move(entry));
  }
  return true;
}

bool readSourceMapV41(const std::vector<uint8_t> &data,
                      size_t &offset,
                      std::vector<IrInstructionSourceMapEntry> &entries,
                      std::string &error) {
  uint32_t sourceMapCount = 0;
  if (!readU32(data, offset, sourceMapCount) || !countFitsRemaining(data, offset, sourceMapCount)) {
    error = "truncated IR instruction source map count";
    return false;
  }
  if (sourceMapCount == 0) {
    return true;
  }
  uint32_t unitCount = 0;
  if (!readU32(data, offset, unitCount) || !countFitsRemaining(data, offset, unitCount)) {
    error = "truncated IR instruction source map unit count";
    return false;
  }
  std::vector<std::string> units(unitCount);
  for (std::string &unit : units) {
    if (!readString(data,
                    offset,
                    unit,
                    "truncated IR instruction source map source unit",
                    "truncated IR instruction source map source unit",
                    error)) {
      return false;
    }
  }
  entries.reserve(sourceMapCount);
  int64_t debugId = 0;
  for (uint32_t sourceMapIndex = 0; sourceMapIndex < sourceMapCount; ++sourceMapIndex) {
    IrInstructionSourceMapEntry entry;
    uint64_t delta = 0;
    uint64_t line = 0;
    uint64_t column = 0;
    uint64_t unitIndex = 0;
    uint8_t provenance = 0;
    if (!readVarint(data, offset, delta) || !readVarint(data, offset, line) ||
        !readVarint(data, offset, column) || !readU8(data, offset, provenance) ||
        !readVarint(data, offset, unitIndex)) {
      error = "truncated IR instruction source map entry";
      return false;
    }
    debugId += zigzagDecode(delta);
    if (debugId < 0 || debugId > static_cast<int64_t>(std::numeric_limits<uint32_t>::max()) ||
        line > std::numeric_limits<uint32_t>::max() ||
        column > std::numeric_limits<uint32_t>::max()) {
      error = "invalid IR instruction source map entry";
      return false;
    }
    if (!checkSourceMapProvenance(provenance, error)) {
      return false;
    }
    if (unitIndex >= units.size()) {
      error = "invalid IR instruction source map source unit index";
      return false;
    }
    entry.debugId = static_cast<uint32_t>(debugId);
    entry.line = static_cast<uint32_t>(line);
    entry.column = static_cast<uint32_t>(column);
    entry.provenance = static_cast<IrSourceMapProvenance>(provenance);
    entry.sourceUnit = units[static_cast<size_t>(unitIndex)];
    entries.push_back(std::move(entry));
  }
  return true;
}

bool readSourceMap(const std::vector<uint8_t> &data,
                   size_t &offset,
                   uint32_t version,
                   std::vector<IrInstructionSourceMapEntry> &entries,
                   std::string &error) {
  return version >= 41u ? readSourceMapV41(data, offset, entries, error)
                        : readSourceMapV40(data, offset, entries, error);
}

} // namespace

bool serializeIr(const IrModule &module, std::vector<uint8_t> &out, std::string &error) {
  out.clear();
  // A module loaded from an older supported version is written as the current one, which can hold everything it has.
  if (module.schemaVersion < IrSchemaMinimumSupportedVersion ||
      module.schemaVersion > IrSchemaVersion) {
    error = "unsupported IR schema version " + std::to_string(module.schemaVersion) +
            " for serialization (supported " + std::to_string(IrSchemaMinimumSupportedVersion) +
            ".." + std::to_string(IrSchemaVersion) + ")";
    return false;
  }
  // v38: a library has no entry point, which is written as NoEntryIndex.
  if (module.entryIndex < -1 || (module.entryIndex >= 0 && static_cast<size_t>(module.entryIndex) >=
                                                               module.functions.size())) {
    error = "invalid IR entry index";
    return false;
  }
  for (const auto &fn : module.functions) {
    if (fn.isImport && !fn.instructions.empty()) {
      error = "IR import " + fn.name + " has instructions";
      return false;
    }
  }
  appendU32(out, IrSchemaMagic);
  appendU32(out, IrSchemaVersion);
  appendU32(out, static_cast<uint32_t>(module.functions.size()));
  appendU32(out, module.entryIndex < 0 ? NoEntryIndex : static_cast<uint32_t>(module.entryIndex));
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
    appendU32(out, fn.parameterCount);
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
      if (inst.isStringLiteral && inst.op != IrOpcode::PushI32 && inst.op != IrOpcode::PushI64) {
        error = "IR string literal mark on an instruction that is not a push in " + fn.name;
        return false;
      }
      out.push_back(!inst.isStringLiteral          ? static_cast<uint8_t>(inst.op)
                    : inst.op == IrOpcode::PushI32 ? IrStringPushI32Code
                                                   : IrStringPushI64Code);
      appendU64(out, inst.imm);
      appendU32(out, inst.debugId);
    }
  }
  if (module.instructionSourceMap.size() > static_cast<size_t>(std::numeric_limits<uint32_t>::max())) {
    error = "too many IR instruction source map entries";
    return false;
  }
  if (!appendSourceMap(out, module.instructionSourceMap, error)) {
    return false;
  }
  if (module.hostImports.size() > static_cast<size_t>(std::numeric_limits<uint32_t>::max())) {
    error = "too many IR host imports";
    return false;
  }
  appendU32(out, static_cast<uint32_t>(module.hostImports.size()));
  for (const auto &import : module.hostImports) {
    if (!appendString(out, import.name, "host import name", error)) {
      return false;
    }
    if (import.parameters.size() > static_cast<size_t>(std::numeric_limits<uint32_t>::max())) {
      error = "too many IR host import parameters";
      return false;
    }
    appendU32(out, static_cast<uint32_t>(import.parameters.size()));
    for (const IrHostValueKind kind : import.parameters) {
      out.push_back(static_cast<uint8_t>(kind));
    }
    out.push_back(static_cast<uint8_t>(import.returnKind));
  }
  // v38: the functions that are imports of a library, by index, in index order.
  uint32_t importedCount = 0;
  for (const auto &fn : module.functions) {
    importedCount += fn.isImport ? 1u : 0u;
  }
  appendU32(out, importedCount);
  for (size_t index = 0; index < module.functions.size(); ++index) {
    if (module.functions[index].isImport) {
      appendU32(out, static_cast<uint32_t>(index));
      if (!appendString(out, module.functions[index].importLibrary, "import library name", error)) {
        return false;
      }
    }
  }
  // v39: whether the module marks every string literal push.
  out.push_back(module.stringPushesMarked ? 1u : 0u);
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
  if (!countFitsRemaining(data, offset, stringCount)) {
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
  if (!countFitsRemaining(data, offset, structCount)) {
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
    if (!countFitsRemaining(data, offset, fieldCount)) {
      error = "truncated IR struct field count";
      return false;
    }
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
  if (!countFitsRemaining(data, offset, funcCount)) {
    error = "truncated IR function count";
    return false;
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
    uint32_t parameterCount = 0;
    if (!readU32(data, offset, parameterCount)) {
      error = "truncated IR function parameter count";
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
    fn.parameterCount = parameterCount;
    if (!countFitsRemaining(data, offset, localDebugCount)) {
      error = "truncated IR local debug metadata count";
      return false;
    }
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
    if (!countFitsRemaining(data, offset, instCount)) {
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
      uint8_t opcodeValue = data[offset];
      // v39: the opcode bytes 0xFE and 0xFF are a PushI32 and a PushI64 that hold a string index.
      if (version >= 39u &&
          (opcodeValue == IrStringPushI32Code || opcodeValue == IrStringPushI64Code)) {
        inst.isStringLiteral = true;
        opcodeValue = opcodeValue == IrStringPushI32Code ? static_cast<uint8_t>(IrOpcode::PushI32)
                                                         : static_cast<uint8_t>(IrOpcode::PushI64);
      }
      const uint8_t minOpcode = IrOpcodeMin;
      // A module of an older version can only hold the opcodes that version had: v34 appended the task opcodes,
      // v35 the atomic ones, v36 the mutex ones, v37 the condition ones, v40 the fused multiply-adds and v42 the function address opcodes.
      const uint8_t maxOpcode = version < 34u   ? static_cast<uint8_t>(IrOpcode::Trap)
                                : version < 35u ? static_cast<uint8_t>(IrOpcode::TaskWait)
                                : version < 36u
                                    ? static_cast<uint8_t>(IrOpcode::AtomicCompareExchange)
                                : version < 37u ? static_cast<uint8_t>(IrOpcode::MutexUnlock)
                                : version < 40u ? static_cast<uint8_t>(IrOpcode::ConditionNotify)
                                : version < 42u ? static_cast<uint8_t>(IrOpcode::FmaF64)
                                                : IrOpcodeMax;
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
  if (!readSourceMap(data, offset, version, out.instructionSourceMap, error)) {
    return false;
  }
  uint32_t hostImportCount = 0;
  if (!readU32(data, offset, hostImportCount)) {
    error = "truncated IR host import count";
    return false;
  }
  if (!countFitsRemaining(data, offset, hostImportCount)) {
    error = "truncated IR host import count";
    return false;
  }
  out.hostImports.reserve(hostImportCount);
  for (uint32_t importIndex = 0; importIndex < hostImportCount; ++importIndex) {
    IrHostImport import;
    if (!readString(data,
                    offset,
                    import.name,
                    "truncated IR host import name",
                    "truncated IR host import name",
                    error)) {
      return false;
    }
    uint32_t parameterCount = 0;
    if (!readU32(data, offset, parameterCount) || !countFitsRemaining(data, offset, parameterCount)) {
      error = "truncated IR host import parameters";
      return false;
    }
    import.parameters.reserve(parameterCount);
    for (uint32_t parameterIndex = 0; parameterIndex < parameterCount; ++parameterIndex) {
      uint8_t kind = 0;
      if (!readU8(data, offset, kind) || kind > IrHostValueKindMax) {
        error = "invalid IR host import parameter kind";
        return false;
      }
      import.parameters.push_back(static_cast<IrHostValueKind>(kind));
    }
    uint8_t returnKind = 0;
    if (!readU8(data, offset, returnKind) || returnKind > IrHostValueKindMax) {
      error = "invalid IR host import return kind";
      return false;
    }
    import.returnKind = static_cast<IrHostValueKind>(returnKind);
    out.hostImports.push_back(std::move(import));
  }
  if (version >= 38u) {
    uint32_t importedCount = 0;
    if (!readU32(data, offset, importedCount) || !countFitsRemaining(data, offset, importedCount)) {
      error = "truncated IR import section";
      return false;
    }
    for (uint32_t i = 0; i < importedCount; ++i) {
      uint32_t functionIndex = 0;
      if (!readU32(data, offset, functionIndex)) {
        error = "truncated IR import section";
        return false;
      }
      if (functionIndex >= out.functions.size() || out.functions[functionIndex].isImport ||
          !out.functions[functionIndex].instructions.empty()) {
        error = "invalid IR import function index";
        return false;
      }
      IrFunction &imported = out.functions[functionIndex];
      if (!readString(data,
                      offset,
                      imported.importLibrary,
                      "truncated IR import library name",
                      "truncated IR import library name",
                      error)) {
        return false;
      }
      imported.isImport = true;
    }
  }
  // v39: whether the module marks every string literal push. An older module has no marks, so it is unmarked when it
  // has strings.
  out.stringPushesMarked = out.stringTable.empty();
  if (version >= 39u) {
    uint8_t marked = 0;
    if (!readU8(data, offset, marked) || marked > 1) {
      error = "truncated IR string mark flag";
      return false;
    }
    out.stringPushesMarked = marked != 0;
  }
  if (version >= 38u && entryIndex == NoEntryIndex) {
    out.entryIndex = -1;
    return true;
  }
  if (entryIndex >= out.functions.size()) {
    error = "invalid IR entry index";
    return false;
  }
  out.entryIndex = static_cast<int32_t>(entryIndex);
  return true;
}

} // namespace primec
