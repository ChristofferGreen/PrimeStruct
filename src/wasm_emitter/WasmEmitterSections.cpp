#include "WasmEmitterInternal.h"

#include <limits>

namespace primec {

namespace {

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

} // namespace

bool appendSection(uint8_t sectionId,
                   const std::vector<uint8_t> &payload,
                   std::vector<uint8_t> &out,
                   std::string &error) {
  out.push_back(sectionId);
  if (!appendLength(payload.size(), out, error)) {
    return false;
  }
  out.insert(out.end(), payload.begin(), payload.end());
  return true;
}

bool encodeTypeSection(const std::vector<WasmFunctionType> &types,
                       std::vector<uint8_t> &payload,
                       std::string &error) {
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

bool encodeImportSection(const std::vector<WasmImport> &imports,
                         std::vector<uint8_t> &payload,
                         std::string &error) {
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

bool encodeMemorySection(const std::vector<WasmMemoryLimit> &memories,
                         std::vector<uint8_t> &payload,
                         std::string &error) {
  payload.clear();
  if (!appendLength(memories.size(), payload, error)) {
    return false;
  }
  for (const WasmMemoryLimit &memory : memories) {
    payload.push_back(memory.hasMax ? 0x01 : 0x00);
    appendU32Leb(memory.minPages, payload);
    if (memory.hasMax) {
      appendU32Leb(memory.maxPages, payload);
    }
  }
  return true;
}

bool encodeExportSection(const std::vector<WasmExport> &exports,
                         std::vector<uint8_t> &payload,
                         std::string &error) {
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

bool encodeCodeSection(const std::vector<WasmCodeBody> &bodies,
                       std::vector<uint8_t> &payload,
                       std::string &error) {
  payload.clear();
  if (!appendLength(bodies.size(), payload, error)) {
    return false;
  }
  for (const WasmCodeBody &body : bodies) {
    std::vector<uint8_t> bodyPayload = body.localDecls;
    bodyPayload.insert(bodyPayload.end(), body.instructions.begin(), body.instructions.end());
    if (!appendLength(bodyPayload.size(), payload, error)) {
      return false;
    }
    payload.insert(payload.end(), bodyPayload.begin(), bodyPayload.end());
  }
  return true;
}

bool encodeDataSection(const std::vector<WasmDataSegment> &segments,
                       std::vector<uint8_t> &payload,
                       std::string &error) {
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

} // namespace primec
