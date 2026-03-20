#include "primec/WasmEmitter.h"

#include "WasmEmitterInternal.h"

#include <cctype>
#include <iterator>
#include <string>
#include <vector>

namespace primec {

namespace {

constexpr uint8_t WasmMagic[] = {0x00, 0x61, 0x73, 0x6d};
constexpr uint8_t WasmVersion[] = {0x01, 0x00, 0x00, 0x00};
constexpr uint8_t WasmValueTypeI32 = 0x7f;
constexpr uint8_t WasmValueTypeI64 = 0x7e;
constexpr uint8_t WasmValueTypeF32 = 0x7d;
constexpr uint8_t WasmValueTypeF64 = 0x7c;

std::string wasmExportName(const std::string &path) {
  std::string name;
  name.reserve(path.size());
  for (char c : path) {
    if (c == '/') {
      if (!name.empty()) {
        name.push_back('_');
      }
      continue;
    }
    const unsigned char uc = static_cast<unsigned char>(c);
    if (std::isalnum(uc) != 0 || c == '_' || c == '$' || c == '.') {
      name.push_back(c);
    } else {
      name.push_back('_');
    }
  }
  if (name.empty()) {
    return "main";
  }
  return name;
}

bool inferFunctionType(const IrFunction &function, WasmFunctionType &outType, std::string &error) {
  bool hasReturnI32 = false;
  bool hasReturnI64 = false;
  bool hasReturnF32 = false;
  bool hasReturnF64 = false;
  bool hasReturnVoid = false;
  for (const IrInstruction &inst : function.instructions) {
    if (inst.op == IrOpcode::ReturnI32) {
      hasReturnI32 = true;
    } else if (inst.op == IrOpcode::ReturnI64) {
      hasReturnI64 = true;
    } else if (inst.op == IrOpcode::ReturnF32) {
      hasReturnF32 = true;
    } else if (inst.op == IrOpcode::ReturnF64) {
      hasReturnF64 = true;
    } else if (inst.op == IrOpcode::ReturnVoid) {
      hasReturnVoid = true;
    }
  }
  const uint32_t returnKindCount =
      (hasReturnI32 ? 1u : 0u) + (hasReturnI64 ? 1u : 0u) + (hasReturnF32 ? 1u : 0u) +
      (hasReturnF64 ? 1u : 0u) + (hasReturnVoid ? 1u : 0u);
  if (returnKindCount > 1u) {
    error = "wasm emitter does not support mixed return kinds in function: " + function.name;
    return false;
  }
  outType.params.clear();
  outType.results.clear();
  if (hasReturnI32) {
    outType.results.push_back(WasmValueTypeI32);
  } else if (hasReturnI64) {
    outType.results.push_back(WasmValueTypeI64);
  } else if (hasReturnF32) {
    outType.results.push_back(WasmValueTypeF32);
  } else if (hasReturnF64) {
    outType.results.push_back(WasmValueTypeF64);
  }
  return true;
}

} // namespace

bool WasmEmitter::emitModule(const IrModule &module, std::vector<uint8_t> &out, std::string &error) const {
  error.clear();
  out.clear();

  if (!module.functions.empty() &&
      (module.entryIndex < 0 || static_cast<size_t>(module.entryIndex) >= module.functions.size())) {
    error = "invalid IR entry index";
    return false;
  }
  if (module.functions.empty() && module.entryIndex != -1) {
    error = "invalid IR entry index";
    return false;
  }

  std::vector<WasmFunctionType> functionTypes;
  functionTypes.reserve(module.functions.size());
  for (size_t functionIndex = 0; functionIndex < module.functions.size(); ++functionIndex) {
    WasmFunctionType functionType;
    if (!inferFunctionType(module.functions[functionIndex], functionType, error)) {
      return false;
    }
    functionTypes.push_back(std::move(functionType));
  }

  std::vector<WasmImport> imports;
  std::vector<WasmFunctionType> types;
  WasmRuntimeContext runtime;
  if (!buildWasiRuntimeContext(module, imports, types, runtime, error)) {
    return false;
  }
  if (!runtime.enabled) {
    runtime.functionIndexOffset = 0;
  }

  std::vector<uint32_t> functionTypeIndexes;
  std::vector<WasmCodeBody> codeBodies;
  types.reserve(types.size() + module.functions.size());
  functionTypeIndexes.reserve(module.functions.size());
  codeBodies.reserve(module.functions.size());

  const uint32_t firstFunctionTypeIndex = static_cast<uint32_t>(types.size());
  for (size_t functionIndex = 0; functionIndex < module.functions.size(); ++functionIndex) {
    types.push_back(functionTypes[functionIndex]);
    functionTypeIndexes.push_back(firstFunctionTypeIndex + static_cast<uint32_t>(functionIndex));
  }
  for (size_t functionIndex = 0; functionIndex < module.functions.size(); ++functionIndex) {
    WasmCodeBody codeBody;
    if (!lowerFunctionCode(module.functions[functionIndex], functionTypes, runtime, codeBody, error)) {
      return false;
    }
    codeBodies.push_back(std::move(codeBody));
  }

  std::vector<WasmExport> exports;
  if (module.entryIndex >= 0) {
    WasmExport exportEntry;
    exportEntry.name = wasmExportName(module.functions[static_cast<size_t>(module.entryIndex)].name);
    exportEntry.kind = WasmFunctionKind;
    exportEntry.index = runtime.functionIndexOffset + static_cast<uint32_t>(module.entryIndex);
    exports.push_back(std::move(exportEntry));
  }

  const std::vector<WasmMemoryLimit> &memories = runtime.memories;
  const std::vector<WasmDataSegment> &dataSegments = runtime.dataSegments;

  out.insert(out.end(), std::begin(WasmMagic), std::end(WasmMagic));
  out.insert(out.end(), std::begin(WasmVersion), std::end(WasmVersion));

  std::vector<uint8_t> payload;
  if (!encodeTypeSection(types, payload, error) || !appendSection(WasmSectionType, payload, out, error)) {
    return false;
  }
  if (!encodeImportSection(imports, payload, error) || !appendSection(WasmSectionImport, payload, out, error)) {
    return false;
  }
  if (!encodeFunctionSection(functionTypeIndexes, payload, error) ||
      !appendSection(WasmSectionFunction, payload, out, error)) {
    return false;
  }
  if (!memories.empty() &&
      (!encodeMemorySection(memories, payload, error) || !appendSection(WasmSectionMemory, payload, out, error))) {
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
