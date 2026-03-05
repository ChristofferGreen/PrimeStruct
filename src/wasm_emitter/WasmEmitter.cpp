#include "primec/WasmEmitter.h"

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace primec {

namespace {

constexpr uint8_t WasmMagic[] = {0x00, 0x61, 0x73, 0x6d};
constexpr uint8_t WasmVersion[] = {0x01, 0x00, 0x00, 0x00};
constexpr uint8_t WasmFunctionKind = 0x00;
constexpr uint8_t WasmFunctionTypeTag = 0x60;
constexpr uint8_t WasmSectionType = 1;
constexpr uint8_t WasmSectionImport = 2;
constexpr uint8_t WasmSectionFunction = 3;
constexpr uint8_t WasmSectionExport = 7;
constexpr uint8_t WasmSectionCode = 10;
constexpr uint8_t WasmSectionData = 11;

struct WasmFunctionType {
  std::vector<uint8_t> params;
  std::vector<uint8_t> results;
};

struct WasmImport {
  std::string module;
  std::string name;
  uint8_t kind = WasmFunctionKind;
  uint32_t typeIndex = 0;
};

struct WasmExport {
  std::string name;
  uint8_t kind = WasmFunctionKind;
  uint32_t index = 0;
};

struct WasmCodeBody {
  std::vector<uint8_t> localDecls;
  std::vector<uint8_t> instructions;
};

struct WasmDataSegment {
  uint32_t memoryIndex = 0;
  std::vector<uint8_t> offsetExpression;
  std::vector<uint8_t> bytes;
};

void appendU32Leb(uint32_t value, std::vector<uint8_t> &out) {
  do {
    uint8_t byte = static_cast<uint8_t>(value & 0x7f);
    value >>= 7;
    if (value != 0) {
      byte |= 0x80;
    }
    out.push_back(byte);
  } while (value != 0);
}

bool appendLength(size_t value, std::vector<uint8_t> &out, std::string &error) {
  if (value > static_cast<size_t>(std::numeric_limits<uint32_t>::max())) {
    error = "wasm section exceeds 32-bit size limit";
    return false;
  }
  appendU32Leb(static_cast<uint32_t>(value), out);
  return true;
}

bool appendName(const std::string &name, std::vector<uint8_t> &out, std::string &error) {
  if (!appendLength(name.size(), out, error)) {
    return false;
  }
  out.insert(out.end(), name.begin(), name.end());
  return true;
}

bool appendSection(uint8_t sectionId, const std::vector<uint8_t> &payload, std::vector<uint8_t> &out, std::string &error) {
  out.push_back(sectionId);
  if (!appendLength(payload.size(), out, error)) {
    return false;
  }
  out.insert(out.end(), payload.begin(), payload.end());
  return true;
}

bool encodeTypeSection(const std::vector<WasmFunctionType> &types, std::vector<uint8_t> &payload, std::string &error) {
  payload.clear();
  if (!appendLength(types.size(), payload, error)) {
    return false;
  }
  for (const WasmFunctionType &type : types) {
    payload.push_back(WasmFunctionTypeTag);
    if (!appendLength(type.params.size(), payload, error)) {
      return false;
    }
    payload.insert(payload.end(), type.params.begin(), type.params.end());
    if (!appendLength(type.results.size(), payload, error)) {
      return false;
    }
    payload.insert(payload.end(), type.results.begin(), type.results.end());
  }
  return true;
}

bool encodeImportSection(const std::vector<WasmImport> &imports, std::vector<uint8_t> &payload, std::string &error) {
  payload.clear();
  if (!appendLength(imports.size(), payload, error)) {
    return false;
  }
  for (const WasmImport &entry : imports) {
    if (!appendName(entry.module, payload, error)) {
      return false;
    }
    if (!appendName(entry.name, payload, error)) {
      return false;
    }
    payload.push_back(entry.kind);
    appendU32Leb(entry.typeIndex, payload);
  }
  return true;
}

bool encodeFunctionSection(const std::vector<uint32_t> &functionTypeIndexes,
                           std::vector<uint8_t> &payload,
                           std::string &error) {
  payload.clear();
  if (!appendLength(functionTypeIndexes.size(), payload, error)) {
    return false;
  }
  for (uint32_t typeIndex : functionTypeIndexes) {
    appendU32Leb(typeIndex, payload);
  }
  return true;
}

bool encodeExportSection(const std::vector<WasmExport> &exports, std::vector<uint8_t> &payload, std::string &error) {
  payload.clear();
  if (!appendLength(exports.size(), payload, error)) {
    return false;
  }
  for (const WasmExport &entry : exports) {
    if (!appendName(entry.name, payload, error)) {
      return false;
    }
    payload.push_back(entry.kind);
    appendU32Leb(entry.index, payload);
  }
  return true;
}

bool encodeCodeSection(const std::vector<WasmCodeBody> &bodies, std::vector<uint8_t> &payload, std::string &error) {
  payload.clear();
  if (!appendLength(bodies.size(), payload, error)) {
    return false;
  }
  for (const WasmCodeBody &body : bodies) {
    std::vector<uint8_t> bodyPayload;
    bodyPayload = body.localDecls;
    bodyPayload.insert(bodyPayload.end(), body.instructions.begin(), body.instructions.end());
    if (!appendLength(bodyPayload.size(), payload, error)) {
      return false;
    }
    payload.insert(payload.end(), bodyPayload.begin(), bodyPayload.end());
  }
  return true;
}

bool encodeDataSection(const std::vector<WasmDataSegment> &segments, std::vector<uint8_t> &payload, std::string &error) {
  payload.clear();
  if (!appendLength(segments.size(), payload, error)) {
    return false;
  }
  for (const WasmDataSegment &segment : segments) {
    if (segment.memoryIndex == 0) {
      payload.push_back(0x00);
    } else {
      payload.push_back(0x02);
      appendU32Leb(segment.memoryIndex, payload);
    }
    payload.insert(payload.end(), segment.offsetExpression.begin(), segment.offsetExpression.end());
    if (!appendLength(segment.bytes.size(), payload, error)) {
      return false;
    }
    payload.insert(payload.end(), segment.bytes.begin(), segment.bytes.end());
  }
  return true;
}

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

  const std::vector<WasmFunctionType> types;
  const std::vector<WasmImport> imports;
  const std::vector<uint32_t> functions;
  const std::vector<WasmExport> exports;
  const std::vector<WasmCodeBody> codeBodies;
  const std::vector<WasmDataSegment> dataSegments;

  std::vector<uint8_t> payload;
  if (!encodeTypeSection(types, payload, error) || !appendSection(WasmSectionType, payload, out, error)) {
    return false;
  }
  if (!encodeImportSection(imports, payload, error) || !appendSection(WasmSectionImport, payload, out, error)) {
    return false;
  }
  if (!encodeFunctionSection(functions, payload, error) || !appendSection(WasmSectionFunction, payload, out, error)) {
    return false;
  }
  if (!encodeExportSection(exports, payload, error) || !appendSection(WasmSectionExport, payload, out, error)) {
    return false;
  }
  if (!encodeCodeSection(codeBodies, payload, error) || !appendSection(WasmSectionCode, payload, out, error)) {
    return false;
  }
  if (!encodeDataSection(dataSegments, payload, error) || !appendSection(WasmSectionData, payload, out, error)) {
    return false;
  }

  return true;
}

} // namespace primec
