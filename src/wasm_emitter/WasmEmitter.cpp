#include "primec/WasmEmitter.h"

#include <algorithm>
#include <cctype>
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
constexpr uint8_t WasmSectionMemory = 5;
constexpr uint8_t WasmSectionExport = 7;
constexpr uint8_t WasmSectionCode = 10;
constexpr uint8_t WasmSectionData = 11;

constexpr uint8_t WasmValueTypeI32 = 0x7f;
constexpr uint8_t WasmValueTypeI64 = 0x7e;
constexpr uint8_t WasmValueTypeF32 = 0x7d;
constexpr uint8_t WasmValueTypeF64 = 0x7c;
constexpr uint8_t WasmBlockTypeVoid = 0x40;

constexpr uint8_t WasmOpIf = 0x04;
constexpr uint8_t WasmOpElse = 0x05;
constexpr uint8_t WasmOpEnd = 0x0b;
constexpr uint8_t WasmOpUnreachable = 0x00;
constexpr uint8_t WasmOpCall = 0x10;
constexpr uint8_t WasmOpReturn = 0x0f;
constexpr uint8_t WasmOpDrop = 0x1a;
constexpr uint8_t WasmOpBlock = 0x02;
constexpr uint8_t WasmOpLoop = 0x03;
constexpr uint8_t WasmOpBr = 0x0c;
constexpr uint8_t WasmOpBrIf = 0x0d;
constexpr uint8_t WasmOpLocalGet = 0x20;
constexpr uint8_t WasmOpLocalSet = 0x21;
constexpr uint8_t WasmOpLocalTee = 0x22;
constexpr uint8_t WasmOpI32Load = 0x28;
constexpr uint8_t WasmOpI32Store = 0x36;
constexpr uint8_t WasmOpI32Store8 = 0x3a;
constexpr uint8_t WasmOpI32Const = 0x41;
constexpr uint8_t WasmOpI64Const = 0x42;
constexpr uint8_t WasmOpI32Eqz = 0x45;
constexpr uint8_t WasmOpI32Eq = 0x46;
constexpr uint8_t WasmOpI32Ne = 0x47;
constexpr uint8_t WasmOpI32LtS = 0x48;
constexpr uint8_t WasmOpI32GtS = 0x4a;
constexpr uint8_t WasmOpI32LeS = 0x4c;
constexpr uint8_t WasmOpI32GeS = 0x4e;
constexpr uint8_t WasmOpI32GeU = 0x4f;
constexpr uint8_t WasmOpI64Eqz = 0x50;
constexpr uint8_t WasmOpI64LtS = 0x53;
constexpr uint8_t WasmOpI32Add = 0x6a;
constexpr uint8_t WasmOpI32Sub = 0x6b;
constexpr uint8_t WasmOpI32Mul = 0x6c;
constexpr uint8_t WasmOpI32DivS = 0x6d;
constexpr uint8_t WasmOpI32And = 0x71;
constexpr uint8_t WasmOpI32Or = 0x72;
constexpr uint8_t WasmOpI64Sub = 0x7d;
constexpr uint8_t WasmOpI64DivU = 0x80;
constexpr uint8_t WasmOpI64RemU = 0x82;
constexpr uint8_t WasmOpI32WrapI64 = 0xa7;
constexpr uint8_t WasmOpF32Const = 0x43;
constexpr uint8_t WasmOpF64Const = 0x44;
constexpr uint8_t WasmOpF32Eq = 0x5b;
constexpr uint8_t WasmOpF32Ne = 0x5c;
constexpr uint8_t WasmOpF32Lt = 0x5d;
constexpr uint8_t WasmOpF32Gt = 0x5e;
constexpr uint8_t WasmOpF32Le = 0x5f;
constexpr uint8_t WasmOpF32Ge = 0x60;
constexpr uint8_t WasmOpF64Eq = 0x61;
constexpr uint8_t WasmOpF64Ne = 0x62;
constexpr uint8_t WasmOpF64Lt = 0x63;
constexpr uint8_t WasmOpF64Gt = 0x64;
constexpr uint8_t WasmOpF64Le = 0x65;
constexpr uint8_t WasmOpF64Ge = 0x66;
constexpr uint8_t WasmOpF32Add = 0x92;
constexpr uint8_t WasmOpF32Sub = 0x93;
constexpr uint8_t WasmOpF32Mul = 0x94;
constexpr uint8_t WasmOpF32Div = 0x95;
constexpr uint8_t WasmOpF32Neg = 0x8c;
constexpr uint8_t WasmOpF64Add = 0xa0;
constexpr uint8_t WasmOpF64Sub = 0xa1;
constexpr uint8_t WasmOpF64Mul = 0xa2;
constexpr uint8_t WasmOpF64Div = 0xa3;
constexpr uint8_t WasmOpF64Neg = 0x9a;
constexpr uint8_t WasmOpI32TruncF32S = 0xa8;
constexpr uint8_t WasmOpI32TruncF64S = 0xaa;
constexpr uint8_t WasmOpI64TruncF32S = 0xae;
constexpr uint8_t WasmOpI64TruncF32U = 0xaf;
constexpr uint8_t WasmOpI64TruncF64S = 0xb0;
constexpr uint8_t WasmOpI64TruncF64U = 0xb1;
constexpr uint8_t WasmOpF32ConvertI32S = 0xb2;
constexpr uint8_t WasmOpF32ConvertI64S = 0xb4;
constexpr uint8_t WasmOpF32ConvertI64U = 0xb5;
constexpr uint8_t WasmOpF32DemoteF64 = 0xb6;
constexpr uint8_t WasmOpF64ConvertI32S = 0xb7;
constexpr uint8_t WasmOpF64ConvertI64S = 0xb9;
constexpr uint8_t WasmOpF64ConvertI64U = 0xba;
constexpr uint8_t WasmOpF64PromoteF32 = 0xbb;
constexpr uint8_t WasmOpI64ExtendI32S = 0xac;

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

struct WasmMemoryLimit {
  uint32_t minPages = 0;
  bool hasMax = false;
  uint32_t maxPages = 0;
};

struct WasmLocalLayout {
  uint32_t irLocalCount = 0;
  bool needsDupTempLocal = false;
  uint32_t dupTempIndex = 0;
  bool hasArgvHelpers = false;
  uint32_t argvIndexLocal = 0;
  uint32_t argvCountLocal = 0;
  uint32_t argvPtrLocal = 0;
  uint32_t argvLenLocal = 0;
  uint32_t argvNextPtrLocal = 0;
  bool hasFileHelpers = false;
  uint32_t fileHandleLocal = 0;
  uint32_t fileValueLocal = 0;
  uint32_t fileErrLocal = 0;
  bool hasFileNumericHelpers = false;
  uint32_t fileDigitsPtrLocal = 0;
  uint32_t fileDigitsLenLocal = 0;
  uint32_t fileDigitsNegLocal = 0;
  uint32_t fileDigitsValueLocal = 0;
  uint32_t fileDigitsRemLocal = 0;
  uint32_t i32LocalCount = 0;
  uint32_t i64LocalCount = 0;
  uint32_t totalLocals = 0;
};

struct WasmRuntimeContext {
  bool enabled = false;
  bool hasArgvOps = false;
  bool hasOutputOps = false;
  bool hasFileOps = false;
  bool hasFileWriteOps = false;
  uint32_t functionIndexOffset = 0;
  uint32_t fdWriteImportIndex = 0;
  uint32_t argsSizesGetImportIndex = 0;
  uint32_t argsGetImportIndex = 0;
  uint32_t fdCloseImportIndex = 0;
  uint32_t fdSyncImportIndex = 0;
  uint32_t pathOpenImportIndex = 0;
  uint32_t argcAddr = 0;
  uint32_t argvBufSizeAddr = 0;
  uint32_t nwrittenAddr = 0;
  uint32_t openedFdAddr = 0;
  uint32_t iovAddr = 0;
  uint32_t argvPtrsBase = 0;
  uint32_t argvBufferBase = 0;
  uint32_t argvPtrsCapacityBytes = 0;
  uint32_t argvBufferCapacityBytes = 0;
  uint32_t newlineAddr = 0;
  uint32_t byteScratchAddr = 0;
  uint32_t numericScratchAddr = 0;
  uint32_t numericScratchBytes = 0;
  std::vector<uint32_t> stringPtrs;
  std::vector<uint32_t> stringLens;
  std::vector<WasmMemoryLimit> memories;
  std::vector<WasmDataSegment> dataSegments;
};

constexpr uint32_t WasiPreopenDirFd = 3u;
constexpr uint32_t WasiFdflagAppend = 1u;
constexpr uint32_t WasiOflagsCreat = 1u;
constexpr uint32_t WasiOflagsTrunc = 8u;
constexpr uint64_t WasiRightsFdRead = 2ull;
constexpr uint64_t WasiRightsFdWrite = 64ull;
constexpr uint32_t WasmFileHandleErrorBit = 0x80000000u;
constexpr uint32_t WasmFileHandleErrorMask = 0x7fffffffu;
constexpr uint32_t WasmNumericScratchBytes = 32u;
constexpr uint32_t WasmDecimalBase = 10u;
constexpr uint32_t WasmAsciiZero = 48u;
constexpr uint32_t WasmAsciiMinus = 45u;

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

void appendS32Leb(int32_t value, std::vector<uint8_t> &out) {
  bool more = true;
  while (more) {
    uint8_t byte = static_cast<uint8_t>(value & 0x7f);
    value >>= 7;
    const bool signBit = (byte & 0x40) != 0;
    if ((value == 0 && !signBit) || (value == -1 && signBit)) {
      more = false;
    } else {
      byte |= 0x80;
    }
    out.push_back(byte);
  }
}

void appendS64Leb(int64_t value, std::vector<uint8_t> &out) {
  bool more = true;
  while (more) {
    uint8_t byte = static_cast<uint8_t>(value & 0x7f);
    value >>= 7;
    const bool signBit = (byte & 0x40) != 0;
    if ((value == 0 && !signBit) || (value == -1 && signBit)) {
      more = false;
    } else {
      byte |= 0x80;
    }
    out.push_back(byte);
  }
}

void appendFixedU32Le(uint32_t value, std::vector<uint8_t> &out) {
  out.push_back(static_cast<uint8_t>(value & 0xff));
  out.push_back(static_cast<uint8_t>((value >> 8) & 0xff));
  out.push_back(static_cast<uint8_t>((value >> 16) & 0xff));
  out.push_back(static_cast<uint8_t>((value >> 24) & 0xff));
}

void appendFixedU64Le(uint64_t value, std::vector<uint8_t> &out) {
  for (size_t byte = 0; byte < 8; ++byte) {
    out.push_back(static_cast<uint8_t>((value >> (byte * 8)) & 0xff));
  }
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

bool encodeMemorySection(const std::vector<WasmMemoryLimit> &memories, std::vector<uint8_t> &payload, std::string &error) {
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
  bool hasReturnF32 = false;
  bool hasReturnF64 = false;
  bool hasReturnVoid = false;
  for (const IrInstruction &inst : function.instructions) {
    if (inst.op == IrOpcode::ReturnI32) {
      hasReturnI32 = true;
    } else if (inst.op == IrOpcode::ReturnF32) {
      hasReturnF32 = true;
    } else if (inst.op == IrOpcode::ReturnF64) {
      hasReturnF64 = true;
    } else if (inst.op == IrOpcode::ReturnVoid) {
      hasReturnVoid = true;
    }
  }
  const uint32_t returnKindCount =
      (hasReturnI32 ? 1u : 0u) + (hasReturnF32 ? 1u : 0u) + (hasReturnF64 ? 1u : 0u) + (hasReturnVoid ? 1u : 0u);
  if (returnKindCount > 1u) {
    error = "wasm emitter does not support mixed return kinds in function: " + function.name;
    return false;
  }
  outType.params.clear();
  outType.results.clear();
  if (hasReturnI32) {
    outType.results.push_back(WasmValueTypeI32);
  } else if (hasReturnF32) {
    outType.results.push_back(WasmValueTypeF32);
  } else if (hasReturnF64) {
    outType.results.push_back(WasmValueTypeF64);
  }
  return true;
}

bool opcodeNeedsWasiRuntime(IrOpcode op) {
  switch (op) {
    case IrOpcode::PushArgc:
    case IrOpcode::PrintString:
    case IrOpcode::PrintArgv:
    case IrOpcode::PrintArgvUnsafe:
    case IrOpcode::FileOpenRead:
    case IrOpcode::FileOpenWrite:
    case IrOpcode::FileOpenAppend:
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

WasmLocalLayout computeLocalLayout(const IrFunction &function, std::string &error) {
  WasmLocalLayout layout;
  uint64_t maxLocalIndex = 0;
  bool hasLocal = false;
  for (const IrInstruction &inst : function.instructions) {
    if (inst.op == IrOpcode::LoadLocal || inst.op == IrOpcode::StoreLocal) {
      maxLocalIndex = std::max(maxLocalIndex, inst.imm);
      hasLocal = true;
    }
    if (inst.op == IrOpcode::Dup) {
      layout.needsDupTempLocal = true;
    }
    if (inst.op == IrOpcode::PrintArgv || inst.op == IrOpcode::PrintArgvUnsafe) {
      layout.hasArgvHelpers = true;
    }
    if (inst.op == IrOpcode::FileOpenRead || inst.op == IrOpcode::FileOpenWrite || inst.op == IrOpcode::FileOpenAppend ||
        inst.op == IrOpcode::FileClose || inst.op == IrOpcode::FileFlush || inst.op == IrOpcode::FileWriteI32 ||
        inst.op == IrOpcode::FileWriteI64 || inst.op == IrOpcode::FileWriteU64 || inst.op == IrOpcode::FileWriteString ||
        inst.op == IrOpcode::FileWriteByte || inst.op == IrOpcode::FileWriteNewline) {
      layout.hasFileHelpers = true;
    }
    if (inst.op == IrOpcode::FileWriteI32 || inst.op == IrOpcode::FileWriteI64 || inst.op == IrOpcode::FileWriteU64) {
      layout.hasFileNumericHelpers = true;
    }
  }
  const uint64_t baseCount = hasLocal ? (maxLocalIndex + 1) : 0;
  layout.irLocalCount = static_cast<uint32_t>(baseCount);
  uint64_t totalCount = baseCount;
  if (layout.needsDupTempLocal) {
    layout.dupTempIndex = static_cast<uint32_t>(totalCount);
    totalCount += 1;
  }
  if (layout.hasArgvHelpers) {
    layout.argvIndexLocal = static_cast<uint32_t>(totalCount);
    layout.argvCountLocal = static_cast<uint32_t>(totalCount + 1);
    layout.argvPtrLocal = static_cast<uint32_t>(totalCount + 2);
    layout.argvLenLocal = static_cast<uint32_t>(totalCount + 3);
    layout.argvNextPtrLocal = static_cast<uint32_t>(totalCount + 4);
    totalCount += 5;
  }
  if (layout.hasFileHelpers) {
    layout.fileHandleLocal = static_cast<uint32_t>(totalCount);
    layout.fileValueLocal = static_cast<uint32_t>(totalCount + 1);
    layout.fileErrLocal = static_cast<uint32_t>(totalCount + 2);
    totalCount += 3;
  }
  if (layout.hasFileNumericHelpers) {
    layout.fileDigitsPtrLocal = static_cast<uint32_t>(totalCount);
    layout.fileDigitsLenLocal = static_cast<uint32_t>(totalCount + 1);
    layout.fileDigitsNegLocal = static_cast<uint32_t>(totalCount + 2);
    totalCount += 3;
  }
  layout.i32LocalCount = static_cast<uint32_t>(totalCount);
  if (layout.hasFileNumericHelpers) {
    layout.fileDigitsValueLocal = static_cast<uint32_t>(totalCount);
    layout.fileDigitsRemLocal = static_cast<uint32_t>(totalCount + 1);
    totalCount += 2;
    layout.i64LocalCount = 2;
  }
  if (totalCount > static_cast<uint64_t>(std::numeric_limits<uint32_t>::max())) {
    error = "wasm emitter local count exceeds 32-bit limit in function: " + function.name;
    return WasmLocalLayout{};
  }
  layout.totalLocals = static_cast<uint32_t>(totalCount);
  return layout;
}

void appendLocalDecls(const WasmLocalLayout &layout, std::vector<uint8_t> &out) {
  if (layout.totalLocals == 0) {
    appendU32Leb(0, out);
    return;
  }
  uint32_t groupCount = 0;
  if (layout.i32LocalCount > 0) {
    groupCount += 1;
  }
  if (layout.i64LocalCount > 0) {
    groupCount += 1;
  }
  appendU32Leb(groupCount, out);
  if (layout.i32LocalCount > 0) {
    appendU32Leb(layout.i32LocalCount, out);
    out.push_back(WasmValueTypeI32);
  }
  if (layout.i64LocalCount > 0) {
    appendU32Leb(layout.i64LocalCount, out);
    out.push_back(WasmValueTypeI64);
  }
}

void appendI32MemArg(uint32_t offset, std::vector<uint8_t> &out) {
  appendU32Leb(2, out);
  appendU32Leb(offset, out);
}

void emitI32LoadAtAddress(uint32_t address, std::vector<uint8_t> &out) {
  out.push_back(WasmOpI32Const);
  appendS32Leb(static_cast<int32_t>(address), out);
  out.push_back(WasmOpI32Load);
  appendI32MemArg(0, out);
}

void emitWasiWritePtrLen(uint32_t fd,
                         uint32_t ptrLocal,
                         uint32_t lenLocal,
                         const WasmRuntimeContext &runtime,
                         std::vector<uint8_t> &out,
                         bool dropResult = true) {
  out.push_back(WasmOpI32Const);
  appendS32Leb(static_cast<int32_t>(runtime.iovAddr), out);
  out.push_back(WasmOpLocalGet);
  appendU32Leb(ptrLocal, out);
  out.push_back(WasmOpI32Store);
  appendI32MemArg(0, out);

  out.push_back(WasmOpI32Const);
  appendS32Leb(static_cast<int32_t>(runtime.iovAddr + 4), out);
  out.push_back(WasmOpLocalGet);
  appendU32Leb(lenLocal, out);
  out.push_back(WasmOpI32Store);
  appendI32MemArg(0, out);

  out.push_back(WasmOpI32Const);
  appendS32Leb(static_cast<int32_t>(fd), out);
  out.push_back(WasmOpI32Const);
  appendS32Leb(static_cast<int32_t>(runtime.iovAddr), out);
  out.push_back(WasmOpI32Const);
  appendS32Leb(1, out);
  out.push_back(WasmOpI32Const);
  appendS32Leb(static_cast<int32_t>(runtime.nwrittenAddr), out);
  out.push_back(WasmOpCall);
  appendU32Leb(runtime.fdWriteImportIndex, out);
  if (dropResult) {
    out.push_back(WasmOpDrop);
  }
}

void emitWasiWriteLiteral(
    uint32_t fd, uint32_t ptr, uint32_t len, const WasmRuntimeContext &runtime, std::vector<uint8_t> &out, bool dropResult = true) {
  out.push_back(WasmOpI32Const);
  appendS32Leb(static_cast<int32_t>(runtime.iovAddr), out);
  out.push_back(WasmOpI32Const);
  appendS32Leb(static_cast<int32_t>(ptr), out);
  out.push_back(WasmOpI32Store);
  appendI32MemArg(0, out);

  out.push_back(WasmOpI32Const);
  appendS32Leb(static_cast<int32_t>(runtime.iovAddr + 4), out);
  out.push_back(WasmOpI32Const);
  appendS32Leb(static_cast<int32_t>(len), out);
  out.push_back(WasmOpI32Store);
  appendI32MemArg(0, out);

  out.push_back(WasmOpI32Const);
  appendS32Leb(static_cast<int32_t>(fd), out);
  out.push_back(WasmOpI32Const);
  appendS32Leb(static_cast<int32_t>(runtime.iovAddr), out);
  out.push_back(WasmOpI32Const);
  appendS32Leb(1, out);
  out.push_back(WasmOpI32Const);
  appendS32Leb(static_cast<int32_t>(runtime.nwrittenAddr), out);
  out.push_back(WasmOpCall);
  appendU32Leb(runtime.fdWriteImportIndex, out);
  if (dropResult) {
    out.push_back(WasmOpDrop);
  }
}

void emitWasiWriteLiteralFromFdLocal(
    uint32_t fdLocal, uint32_t ptr, uint32_t len, const WasmRuntimeContext &runtime, std::vector<uint8_t> &out) {
  out.push_back(WasmOpI32Const);
  appendS32Leb(static_cast<int32_t>(runtime.iovAddr), out);
  out.push_back(WasmOpI32Const);
  appendS32Leb(static_cast<int32_t>(ptr), out);
  out.push_back(WasmOpI32Store);
  appendI32MemArg(0, out);

  out.push_back(WasmOpI32Const);
  appendS32Leb(static_cast<int32_t>(runtime.iovAddr + 4), out);
  out.push_back(WasmOpI32Const);
  appendS32Leb(static_cast<int32_t>(len), out);
  out.push_back(WasmOpI32Store);
  appendI32MemArg(0, out);

  out.push_back(WasmOpLocalGet);
  appendU32Leb(fdLocal, out);
  out.push_back(WasmOpI32Const);
  appendS32Leb(static_cast<int32_t>(runtime.iovAddr), out);
  out.push_back(WasmOpI32Const);
  appendS32Leb(1, out);
  out.push_back(WasmOpI32Const);
  appendS32Leb(static_cast<int32_t>(runtime.nwrittenAddr), out);
  out.push_back(WasmOpCall);
  appendU32Leb(runtime.fdWriteImportIndex, out);
}

void emitWasiWritePtrLenFromFdLocal(
    uint32_t fdLocal, uint32_t ptrLocal, uint32_t lenLocal, const WasmRuntimeContext &runtime, std::vector<uint8_t> &out) {
  out.push_back(WasmOpI32Const);
  appendS32Leb(static_cast<int32_t>(runtime.iovAddr), out);
  out.push_back(WasmOpLocalGet);
  appendU32Leb(ptrLocal, out);
  out.push_back(WasmOpI32Store);
  appendI32MemArg(0, out);

  out.push_back(WasmOpI32Const);
  appendS32Leb(static_cast<int32_t>(runtime.iovAddr + 4), out);
  out.push_back(WasmOpLocalGet);
  appendU32Leb(lenLocal, out);
  out.push_back(WasmOpI32Store);
  appendI32MemArg(0, out);

  out.push_back(WasmOpLocalGet);
  appendU32Leb(fdLocal, out);
  out.push_back(WasmOpI32Const);
  appendS32Leb(static_cast<int32_t>(runtime.iovAddr), out);
  out.push_back(WasmOpI32Const);
  appendS32Leb(1, out);
  out.push_back(WasmOpI32Const);
  appendS32Leb(static_cast<int32_t>(runtime.nwrittenAddr), out);
  out.push_back(WasmOpCall);
  appendU32Leb(runtime.fdWriteImportIndex, out);
}

void emitWasiWriteDecimalFromI64Local(uint32_t fdLocal,
                                      uint32_t valueLocal,
                                      uint32_t remLocal,
                                      uint32_t ptrLocal,
                                      uint32_t lenLocal,
                                      uint32_t negLocal,
                                      bool signedValue,
                                      const WasmRuntimeContext &runtime,
                                      std::vector<uint8_t> &out) {
  out.push_back(WasmOpI32Const);
  appendS32Leb(static_cast<int32_t>(runtime.numericScratchAddr + runtime.numericScratchBytes), out);
  out.push_back(WasmOpLocalSet);
  appendU32Leb(ptrLocal, out);

  out.push_back(WasmOpI32Const);
  appendS32Leb(0, out);
  out.push_back(WasmOpLocalSet);
  appendU32Leb(lenLocal, out);

  out.push_back(WasmOpI32Const);
  appendS32Leb(0, out);
  out.push_back(WasmOpLocalSet);
  appendU32Leb(negLocal, out);

  if (signedValue) {
    out.push_back(WasmOpLocalGet);
    appendU32Leb(valueLocal, out);
    out.push_back(WasmOpI64Const);
    appendS64Leb(0, out);
    out.push_back(WasmOpI64LtS);
    out.push_back(WasmOpIf);
    out.push_back(WasmBlockTypeVoid);
    out.push_back(WasmOpI32Const);
    appendS32Leb(1, out);
    out.push_back(WasmOpLocalSet);
    appendU32Leb(negLocal, out);
    out.push_back(WasmOpI64Const);
    appendS64Leb(0, out);
    out.push_back(WasmOpLocalGet);
    appendU32Leb(valueLocal, out);
    out.push_back(WasmOpI64Sub);
    out.push_back(WasmOpLocalSet);
    appendU32Leb(valueLocal, out);
    out.push_back(WasmOpEnd);
  }

  out.push_back(WasmOpBlock);
  out.push_back(WasmBlockTypeVoid);
  out.push_back(WasmOpLoop);
  out.push_back(WasmBlockTypeVoid);

  out.push_back(WasmOpLocalGet);
  appendU32Leb(valueLocal, out);
  out.push_back(WasmOpI64Const);
  appendS64Leb(static_cast<int64_t>(WasmDecimalBase), out);
  out.push_back(WasmOpI64RemU);
  out.push_back(WasmOpLocalSet);
  appendU32Leb(remLocal, out);

  out.push_back(WasmOpLocalGet);
  appendU32Leb(ptrLocal, out);
  out.push_back(WasmOpI32Const);
  appendS32Leb(1, out);
  out.push_back(WasmOpI32Sub);
  out.push_back(WasmOpLocalTee);
  appendU32Leb(ptrLocal, out);
  out.push_back(WasmOpLocalGet);
  appendU32Leb(remLocal, out);
  out.push_back(WasmOpI32WrapI64);
  out.push_back(WasmOpI32Const);
  appendS32Leb(static_cast<int32_t>(WasmAsciiZero), out);
  out.push_back(WasmOpI32Add);
  out.push_back(WasmOpI32Store8);
  appendI32MemArg(0, out);

  out.push_back(WasmOpLocalGet);
  appendU32Leb(lenLocal, out);
  out.push_back(WasmOpI32Const);
  appendS32Leb(1, out);
  out.push_back(WasmOpI32Add);
  out.push_back(WasmOpLocalSet);
  appendU32Leb(lenLocal, out);

  out.push_back(WasmOpLocalGet);
  appendU32Leb(valueLocal, out);
  out.push_back(WasmOpI64Const);
  appendS64Leb(static_cast<int64_t>(WasmDecimalBase), out);
  out.push_back(WasmOpI64DivU);
  out.push_back(WasmOpLocalTee);
  appendU32Leb(valueLocal, out);
  out.push_back(WasmOpI64Eqz);
  out.push_back(WasmOpI32Eqz);
  out.push_back(WasmOpBrIf);
  appendU32Leb(0, out);

  out.push_back(WasmOpEnd);
  out.push_back(WasmOpEnd);

  if (signedValue) {
    out.push_back(WasmOpLocalGet);
    appendU32Leb(negLocal, out);
    out.push_back(WasmOpIf);
    out.push_back(WasmBlockTypeVoid);

    out.push_back(WasmOpLocalGet);
    appendU32Leb(ptrLocal, out);
    out.push_back(WasmOpI32Const);
    appendS32Leb(1, out);
    out.push_back(WasmOpI32Sub);
    out.push_back(WasmOpLocalTee);
    appendU32Leb(ptrLocal, out);
    out.push_back(WasmOpI32Const);
    appendS32Leb(static_cast<int32_t>(WasmAsciiMinus), out);
    out.push_back(WasmOpI32Store8);
    appendI32MemArg(0, out);

    out.push_back(WasmOpLocalGet);
    appendU32Leb(lenLocal, out);
    out.push_back(WasmOpI32Const);
    appendS32Leb(1, out);
    out.push_back(WasmOpI32Add);
    out.push_back(WasmOpLocalSet);
    appendU32Leb(lenLocal, out);
    out.push_back(WasmOpEnd);
  }

  emitWasiWritePtrLenFromFdLocal(fdLocal, ptrLocal, lenLocal, runtime, out);
}

void emitWasiPathOpen(uint32_t pathPtr,
                      uint32_t pathLen,
                      uint32_t oflags,
                      uint64_t rightsBase,
                      uint32_t fdflags,
                      const WasmRuntimeContext &runtime,
                      std::vector<uint8_t> &out) {
  out.push_back(WasmOpI32Const);
  appendS32Leb(static_cast<int32_t>(WasiPreopenDirFd), out);
  out.push_back(WasmOpI32Const);
  appendS32Leb(0, out);
  out.push_back(WasmOpI32Const);
  appendS32Leb(static_cast<int32_t>(pathPtr), out);
  out.push_back(WasmOpI32Const);
  appendS32Leb(static_cast<int32_t>(pathLen), out);
  out.push_back(WasmOpI32Const);
  appendS32Leb(static_cast<int32_t>(oflags), out);
  out.push_back(WasmOpI64Const);
  appendS64Leb(static_cast<int64_t>(rightsBase), out);
  out.push_back(WasmOpI64Const);
  appendS64Leb(0, out);
  out.push_back(WasmOpI32Const);
  appendS32Leb(static_cast<int32_t>(fdflags), out);
  out.push_back(WasmOpI32Const);
  appendS32Leb(static_cast<int32_t>(runtime.openedFdAddr), out);
  out.push_back(WasmOpCall);
  appendU32Leb(runtime.pathOpenImportIndex, out);
}

void appendDataSegmentAt(uint32_t address, const std::vector<uint8_t> &bytes, std::vector<WasmDataSegment> &segments) {
  WasmDataSegment segment;
  segment.memoryIndex = 0;
  segment.offsetExpression.push_back(WasmOpI32Const);
  appendS32Leb(static_cast<int32_t>(address), segment.offsetExpression);
  segment.offsetExpression.push_back(WasmOpEnd);
  segment.bytes = bytes;
  segments.push_back(std::move(segment));
}

bool buildWasiRuntimeContext(const IrModule &module,
                             std::vector<WasmImport> &imports,
                             std::vector<WasmFunctionType> &types,
                             WasmRuntimeContext &runtime,
                             std::string &error) {
  runtime = WasmRuntimeContext{};
  for (const IrFunction &function : module.functions) {
    for (const IrInstruction &inst : function.instructions) {
      if (!opcodeNeedsWasiRuntime(inst.op)) {
        continue;
      }
      runtime.enabled = true;
      if (inst.op == IrOpcode::PushArgc || inst.op == IrOpcode::PrintArgv || inst.op == IrOpcode::PrintArgvUnsafe) {
        runtime.hasArgvOps = true;
      }
      if (inst.op == IrOpcode::PrintString || inst.op == IrOpcode::PrintArgv || inst.op == IrOpcode::PrintArgvUnsafe) {
        runtime.hasOutputOps = true;
      }
      if (inst.op == IrOpcode::FileOpenRead || inst.op == IrOpcode::FileOpenWrite || inst.op == IrOpcode::FileOpenAppend ||
          inst.op == IrOpcode::FileClose || inst.op == IrOpcode::FileFlush || inst.op == IrOpcode::FileWriteI32 ||
          inst.op == IrOpcode::FileWriteI64 || inst.op == IrOpcode::FileWriteU64 || inst.op == IrOpcode::FileWriteString ||
          inst.op == IrOpcode::FileWriteByte || inst.op == IrOpcode::FileWriteNewline) {
        runtime.hasFileOps = true;
      }
      if (inst.op == IrOpcode::FileWriteI32 || inst.op == IrOpcode::FileWriteI64 || inst.op == IrOpcode::FileWriteU64 ||
          inst.op == IrOpcode::FileWriteString || inst.op == IrOpcode::FileWriteByte || inst.op == IrOpcode::FileWriteNewline) {
        runtime.hasFileWriteOps = true;
      }
    }
  }
  if (!runtime.enabled) {
    return true;
  }

  runtime.argcAddr = 0;
  runtime.argvBufSizeAddr = 4;
  runtime.nwrittenAddr = 8;
  runtime.openedFdAddr = 12;
  runtime.iovAddr = 16;
  runtime.argvPtrsBase = 64;
  runtime.argvPtrsCapacityBytes = 4096;
  runtime.argvBufferBase = runtime.argvPtrsBase + runtime.argvPtrsCapacityBytes;
  runtime.argvBufferCapacityBytes = 16384;
  runtime.newlineAddr = runtime.argvBufferBase + runtime.argvBufferCapacityBytes;
  runtime.byteScratchAddr = runtime.newlineAddr + 1;
  runtime.numericScratchAddr = runtime.byteScratchAddr + 1;
  runtime.numericScratchBytes = WasmNumericScratchBytes;
  uint32_t stringBase = runtime.numericScratchAddr + runtime.numericScratchBytes;

  runtime.stringPtrs.assign(module.stringTable.size(), 0);
  runtime.stringLens.assign(module.stringTable.size(), 0);
  std::vector<uint8_t> stringBlob;
  for (size_t i = 0; i < module.stringTable.size(); ++i) {
    runtime.stringPtrs[i] = stringBase + static_cast<uint32_t>(stringBlob.size());
    runtime.stringLens[i] = static_cast<uint32_t>(module.stringTable[i].size());
    stringBlob.insert(stringBlob.end(), module.stringTable[i].begin(), module.stringTable[i].end());
  }

  runtime.memories.push_back(WasmMemoryLimit{1, false, 0});
  if (!stringBlob.empty()) {
    appendDataSegmentAt(stringBase, stringBlob, runtime.dataSegments);
  }
  appendDataSegmentAt(runtime.newlineAddr, std::vector<uint8_t>{'\n'}, runtime.dataSegments);

  if (runtime.hasOutputOps || runtime.hasFileWriteOps) {
    WasmFunctionType fdWriteType;
    fdWriteType.params = {WasmValueTypeI32, WasmValueTypeI32, WasmValueTypeI32, WasmValueTypeI32};
    fdWriteType.results = {WasmValueTypeI32};
    runtime.fdWriteImportIndex = static_cast<uint32_t>(imports.size());
    imports.push_back({"wasi_snapshot_preview1", "fd_write", WasmFunctionKind, static_cast<uint32_t>(types.size())});
    types.push_back(std::move(fdWriteType));
  }

  if (runtime.hasArgvOps) {
    WasmFunctionType argsSizesType;
    argsSizesType.params = {WasmValueTypeI32, WasmValueTypeI32};
    argsSizesType.results = {WasmValueTypeI32};
    runtime.argsSizesGetImportIndex = static_cast<uint32_t>(imports.size());
    imports.push_back(
        {"wasi_snapshot_preview1", "args_sizes_get", WasmFunctionKind, static_cast<uint32_t>(types.size())});
    types.push_back(std::move(argsSizesType));

    WasmFunctionType argsGetType;
    argsGetType.params = {WasmValueTypeI32, WasmValueTypeI32};
    argsGetType.results = {WasmValueTypeI32};
    runtime.argsGetImportIndex = static_cast<uint32_t>(imports.size());
    imports.push_back({"wasi_snapshot_preview1", "args_get", WasmFunctionKind, static_cast<uint32_t>(types.size())});
    types.push_back(std::move(argsGetType));
  }

  if (runtime.hasFileOps) {
    WasmFunctionType pathOpenType;
    pathOpenType.params = {WasmValueTypeI32,
                           WasmValueTypeI32,
                           WasmValueTypeI32,
                           WasmValueTypeI32,
                           WasmValueTypeI32,
                           WasmValueTypeI64,
                           WasmValueTypeI64,
                           WasmValueTypeI32,
                           WasmValueTypeI32};
    pathOpenType.results = {WasmValueTypeI32};
    runtime.pathOpenImportIndex = static_cast<uint32_t>(imports.size());
    imports.push_back({"wasi_snapshot_preview1", "path_open", WasmFunctionKind, static_cast<uint32_t>(types.size())});
    types.push_back(std::move(pathOpenType));

    WasmFunctionType fdCloseType;
    fdCloseType.params = {WasmValueTypeI32};
    fdCloseType.results = {WasmValueTypeI32};
    runtime.fdCloseImportIndex = static_cast<uint32_t>(imports.size());
    imports.push_back({"wasi_snapshot_preview1", "fd_close", WasmFunctionKind, static_cast<uint32_t>(types.size())});
    types.push_back(std::move(fdCloseType));

    WasmFunctionType fdSyncType;
    fdSyncType.params = {WasmValueTypeI32};
    fdSyncType.results = {WasmValueTypeI32};
    runtime.fdSyncImportIndex = static_cast<uint32_t>(imports.size());
    imports.push_back({"wasi_snapshot_preview1", "fd_sync", WasmFunctionKind, static_cast<uint32_t>(types.size())});
    types.push_back(std::move(fdSyncType));
  }

  runtime.functionIndexOffset = static_cast<uint32_t>(imports.size());

  const uint32_t memoryEnd = stringBase + static_cast<uint32_t>(stringBlob.size());
  const uint32_t requiredPages = (memoryEnd / 65536u) + 1u;
  if (requiredPages > runtime.memories[0].minPages) {
    runtime.memories[0].minPages = requiredPages;
  }
  if (runtime.memories[0].minPages == 0) {
    error = "wasm emitter invalid memory layout";
    return false;
  }
  return true;
}

bool emitSimpleInstruction(const IrInstruction &inst,
                           const WasmLocalLayout &localLayout,
                           const std::vector<WasmFunctionType> &functionTypes,
                           const WasmRuntimeContext &runtime,
                           std::vector<uint8_t> &out,
                           std::string &error,
                           const std::string &functionName) {
  const uint32_t dupTempIndex = localLayout.dupTempIndex;
  switch (inst.op) {
    case IrOpcode::PushI32:
      out.push_back(WasmOpI32Const);
      appendS32Leb(static_cast<int32_t>(inst.imm), out);
      return true;
    case IrOpcode::PushF32:
      out.push_back(WasmOpF32Const);
      appendFixedU32Le(static_cast<uint32_t>(inst.imm & 0xffffffffu), out);
      return true;
    case IrOpcode::PushF64:
      out.push_back(WasmOpF64Const);
      appendFixedU64Le(inst.imm, out);
      return true;
    case IrOpcode::LoadLocal:
      if (inst.imm >= localLayout.irLocalCount) {
        error = "wasm emitter local index out of range in function: " + functionName;
        return false;
      }
      out.push_back(WasmOpLocalGet);
      appendU32Leb(static_cast<uint32_t>(inst.imm), out);
      return true;
    case IrOpcode::StoreLocal:
      if (inst.imm >= localLayout.irLocalCount) {
        error = "wasm emitter local index out of range in function: " + functionName;
        return false;
      }
      out.push_back(WasmOpLocalSet);
      appendU32Leb(static_cast<uint32_t>(inst.imm), out);
      return true;
    case IrOpcode::Dup:
      if (!localLayout.needsDupTempLocal) {
        error = "wasm emitter internal error: missing dup temp local";
        return false;
      }
      out.push_back(WasmOpLocalTee);
      appendU32Leb(dupTempIndex, out);
      out.push_back(WasmOpLocalGet);
      appendU32Leb(dupTempIndex, out);
      return true;
    case IrOpcode::Pop:
      out.push_back(WasmOpDrop);
      return true;
    case IrOpcode::AddI32:
      out.push_back(WasmOpI32Add);
      return true;
    case IrOpcode::SubI32:
      out.push_back(WasmOpI32Sub);
      return true;
    case IrOpcode::MulI32:
      out.push_back(WasmOpI32Mul);
      return true;
    case IrOpcode::DivI32:
      out.push_back(WasmOpI32DivS);
      return true;
    case IrOpcode::NegI32:
      out.push_back(WasmOpI32Const);
      appendS32Leb(-1, out);
      out.push_back(WasmOpI32Mul);
      return true;
    case IrOpcode::CmpEqI32:
      out.push_back(WasmOpI32Eq);
      return true;
    case IrOpcode::CmpNeI32:
      out.push_back(WasmOpI32Ne);
      return true;
    case IrOpcode::CmpLtI32:
      out.push_back(WasmOpI32LtS);
      return true;
    case IrOpcode::CmpLeI32:
      out.push_back(WasmOpI32LeS);
      return true;
    case IrOpcode::CmpGtI32:
      out.push_back(WasmOpI32GtS);
      return true;
    case IrOpcode::CmpGeI32:
      out.push_back(WasmOpI32GeS);
      return true;
    case IrOpcode::AddF32:
      out.push_back(WasmOpF32Add);
      return true;
    case IrOpcode::SubF32:
      out.push_back(WasmOpF32Sub);
      return true;
    case IrOpcode::MulF32:
      out.push_back(WasmOpF32Mul);
      return true;
    case IrOpcode::DivF32:
      out.push_back(WasmOpF32Div);
      return true;
    case IrOpcode::NegF32:
      out.push_back(WasmOpF32Neg);
      return true;
    case IrOpcode::AddF64:
      out.push_back(WasmOpF64Add);
      return true;
    case IrOpcode::SubF64:
      out.push_back(WasmOpF64Sub);
      return true;
    case IrOpcode::MulF64:
      out.push_back(WasmOpF64Mul);
      return true;
    case IrOpcode::DivF64:
      out.push_back(WasmOpF64Div);
      return true;
    case IrOpcode::NegF64:
      out.push_back(WasmOpF64Neg);
      return true;
    case IrOpcode::CmpEqF32:
      out.push_back(WasmOpF32Eq);
      return true;
    case IrOpcode::CmpNeF32:
      out.push_back(WasmOpF32Ne);
      return true;
    case IrOpcode::CmpLtF32:
      out.push_back(WasmOpF32Lt);
      return true;
    case IrOpcode::CmpLeF32:
      out.push_back(WasmOpF32Le);
      return true;
    case IrOpcode::CmpGtF32:
      out.push_back(WasmOpF32Gt);
      return true;
    case IrOpcode::CmpGeF32:
      out.push_back(WasmOpF32Ge);
      return true;
    case IrOpcode::CmpEqF64:
      out.push_back(WasmOpF64Eq);
      return true;
    case IrOpcode::CmpNeF64:
      out.push_back(WasmOpF64Ne);
      return true;
    case IrOpcode::CmpLtF64:
      out.push_back(WasmOpF64Lt);
      return true;
    case IrOpcode::CmpLeF64:
      out.push_back(WasmOpF64Le);
      return true;
    case IrOpcode::CmpGtF64:
      out.push_back(WasmOpF64Gt);
      return true;
    case IrOpcode::CmpGeF64:
      out.push_back(WasmOpF64Ge);
      return true;
    case IrOpcode::ConvertI32ToF32:
      out.push_back(WasmOpF32ConvertI32S);
      return true;
    case IrOpcode::ConvertI32ToF64:
      out.push_back(WasmOpF64ConvertI32S);
      return true;
    case IrOpcode::ConvertI64ToF32:
      out.push_back(WasmOpF32ConvertI64S);
      return true;
    case IrOpcode::ConvertI64ToF64:
      out.push_back(WasmOpF64ConvertI64S);
      return true;
    case IrOpcode::ConvertU64ToF32:
      out.push_back(WasmOpF32ConvertI64U);
      return true;
    case IrOpcode::ConvertU64ToF64:
      out.push_back(WasmOpF64ConvertI64U);
      return true;
    case IrOpcode::ConvertF32ToI32:
      out.push_back(WasmOpI32TruncF32S);
      return true;
    case IrOpcode::ConvertF64ToI32:
      out.push_back(WasmOpI32TruncF64S);
      return true;
    case IrOpcode::ConvertF32ToI64:
      out.push_back(WasmOpI64TruncF32S);
      return true;
    case IrOpcode::ConvertF32ToU64:
      out.push_back(WasmOpI64TruncF32U);
      return true;
    case IrOpcode::ConvertF64ToI64:
      out.push_back(WasmOpI64TruncF64S);
      return true;
    case IrOpcode::ConvertF64ToU64:
      out.push_back(WasmOpI64TruncF64U);
      return true;
    case IrOpcode::ConvertF32ToF64:
      out.push_back(WasmOpF64PromoteF32);
      return true;
    case IrOpcode::ConvertF64ToF32:
      out.push_back(WasmOpF32DemoteF64);
      return true;
    case IrOpcode::PushArgc:
      if (!runtime.enabled || !runtime.hasArgvOps) {
        error = "wasm emitter missing argv runtime support in function: " + functionName;
        return false;
      }
      out.push_back(WasmOpI32Const);
      appendS32Leb(static_cast<int32_t>(runtime.argcAddr), out);
      out.push_back(WasmOpI32Const);
      appendS32Leb(static_cast<int32_t>(runtime.argvBufSizeAddr), out);
      out.push_back(WasmOpCall);
      appendU32Leb(runtime.argsSizesGetImportIndex, out);
      out.push_back(WasmOpI32Eqz);
      out.push_back(WasmOpIf);
      out.push_back(WasmValueTypeI32);
      emitI32LoadAtAddress(runtime.argcAddr, out);
      out.push_back(WasmOpElse);
      out.push_back(WasmOpI32Const);
      appendS32Leb(0, out);
      out.push_back(WasmOpEnd);
      return true;
    case IrOpcode::PrintString: {
      if (!runtime.enabled || !runtime.hasOutputOps) {
        error = "wasm emitter missing output runtime support in function: " + functionName;
        return false;
      }
      const uint64_t stringIndex = decodePrintStringIndex(inst.imm);
      if (stringIndex >= runtime.stringPtrs.size()) {
        error = "wasm emitter invalid string index in function: " + functionName;
        return false;
      }
      const uint64_t flags = decodePrintFlags(inst.imm);
      const uint32_t fd = (flags & PrintFlagStderr) ? 2u : 1u;
      emitWasiWriteLiteral(
          fd, runtime.stringPtrs[static_cast<size_t>(stringIndex)], runtime.stringLens[static_cast<size_t>(stringIndex)], runtime, out);
      if ((flags & PrintFlagNewline) != 0) {
        emitWasiWriteLiteral(fd, runtime.newlineAddr, 1, runtime, out);
      }
      return true;
    }
    case IrOpcode::PrintArgv:
    case IrOpcode::PrintArgvUnsafe: {
      if (!runtime.enabled || !runtime.hasArgvOps || !localLayout.hasArgvHelpers) {
        error = "wasm emitter missing argv runtime helpers in function: " + functionName;
        return false;
      }
      const bool safePrint = (inst.op == IrOpcode::PrintArgv);
      const uint64_t flags = decodePrintFlags(inst.imm);
      const uint32_t fd = (flags & PrintFlagStderr) ? 2u : 1u;

      out.push_back(WasmOpLocalSet);
      appendU32Leb(localLayout.argvIndexLocal, out);

      out.push_back(WasmOpI32Const);
      appendS32Leb(static_cast<int32_t>(runtime.argcAddr), out);
      out.push_back(WasmOpI32Const);
      appendS32Leb(static_cast<int32_t>(runtime.argvBufSizeAddr), out);
      out.push_back(WasmOpCall);
      appendU32Leb(runtime.argsSizesGetImportIndex, out);
      out.push_back(WasmOpI32Eqz);
      out.push_back(WasmOpIf);
      out.push_back(WasmBlockTypeVoid);
      out.push_back(WasmOpI32Const);
      appendS32Leb(static_cast<int32_t>(runtime.argvPtrsBase), out);
      out.push_back(WasmOpI32Const);
      appendS32Leb(static_cast<int32_t>(runtime.argvBufferBase), out);
      out.push_back(WasmOpCall);
      appendU32Leb(runtime.argsGetImportIndex, out);
      out.push_back(WasmOpI32Eqz);
      out.push_back(WasmOpIf);
      out.push_back(WasmBlockTypeVoid);

      emitI32LoadAtAddress(runtime.argcAddr, out);
      out.push_back(WasmOpLocalSet);
      appendU32Leb(localLayout.argvCountLocal, out);

      out.push_back(WasmOpLocalGet);
      appendU32Leb(localLayout.argvIndexLocal, out);
      out.push_back(WasmOpLocalGet);
      appendU32Leb(localLayout.argvCountLocal, out);
      out.push_back(WasmOpI32GeU);
      out.push_back(WasmOpIf);
      out.push_back(WasmBlockTypeVoid);
      if (safePrint) {
        out.push_back(WasmOpUnreachable);
      }
      out.push_back(WasmOpElse);
      out.push_back(WasmOpI32Const);
      appendS32Leb(static_cast<int32_t>(runtime.argvPtrsBase), out);
      out.push_back(WasmOpLocalGet);
      appendU32Leb(localLayout.argvIndexLocal, out);
      out.push_back(WasmOpI32Const);
      appendS32Leb(4, out);
      out.push_back(WasmOpI32Mul);
      out.push_back(WasmOpI32Add);
      out.push_back(WasmOpI32Load);
      appendI32MemArg(0, out);
      out.push_back(WasmOpLocalSet);
      appendU32Leb(localLayout.argvPtrLocal, out);

      out.push_back(WasmOpLocalGet);
      appendU32Leb(localLayout.argvIndexLocal, out);
      out.push_back(WasmOpLocalGet);
      appendU32Leb(localLayout.argvCountLocal, out);
      out.push_back(WasmOpI32Const);
      appendS32Leb(1, out);
      out.push_back(WasmOpI32Sub);
      out.push_back(WasmOpI32Eq);
      out.push_back(WasmOpIf);
      out.push_back(WasmBlockTypeVoid);
      out.push_back(WasmOpI32Const);
      appendS32Leb(static_cast<int32_t>(runtime.argvBufferBase), out);
      emitI32LoadAtAddress(runtime.argvBufSizeAddr, out);
      out.push_back(WasmOpI32Add);
      out.push_back(WasmOpLocalGet);
      appendU32Leb(localLayout.argvPtrLocal, out);
      out.push_back(WasmOpI32Sub);
      out.push_back(WasmOpLocalSet);
      appendU32Leb(localLayout.argvLenLocal, out);
      out.push_back(WasmOpElse);
      out.push_back(WasmOpI32Const);
      appendS32Leb(static_cast<int32_t>(runtime.argvPtrsBase), out);
      out.push_back(WasmOpLocalGet);
      appendU32Leb(localLayout.argvIndexLocal, out);
      out.push_back(WasmOpI32Const);
      appendS32Leb(1, out);
      out.push_back(WasmOpI32Add);
      out.push_back(WasmOpI32Const);
      appendS32Leb(4, out);
      out.push_back(WasmOpI32Mul);
      out.push_back(WasmOpI32Add);
      out.push_back(WasmOpI32Load);
      appendI32MemArg(0, out);
      out.push_back(WasmOpLocalSet);
      appendU32Leb(localLayout.argvNextPtrLocal, out);
      out.push_back(WasmOpLocalGet);
      appendU32Leb(localLayout.argvNextPtrLocal, out);
      out.push_back(WasmOpLocalGet);
      appendU32Leb(localLayout.argvPtrLocal, out);
      out.push_back(WasmOpI32Sub);
      out.push_back(WasmOpLocalSet);
      appendU32Leb(localLayout.argvLenLocal, out);
      out.push_back(WasmOpEnd);

      emitWasiWritePtrLen(fd, localLayout.argvPtrLocal, localLayout.argvLenLocal, runtime, out);
      if ((flags & PrintFlagNewline) != 0) {
        emitWasiWriteLiteral(fd, runtime.newlineAddr, 1, runtime, out);
      }
      out.push_back(WasmOpEnd);
      out.push_back(WasmOpEnd);
      out.push_back(WasmOpElse);
      if (safePrint) {
        out.push_back(WasmOpUnreachable);
      }
      out.push_back(WasmOpEnd);
      return true;
    }
    case IrOpcode::FileOpenRead:
    case IrOpcode::FileOpenWrite:
    case IrOpcode::FileOpenAppend: {
      if (!runtime.enabled || !runtime.hasFileOps || !localLayout.hasFileHelpers) {
        error = "wasm emitter missing file runtime support in function: " + functionName;
        return false;
      }
      if (inst.imm >= runtime.stringPtrs.size()) {
        error = "wasm emitter invalid string index in function: " + functionName;
        return false;
      }

      uint32_t oflags = 0;
      uint64_t rightsBase = WasiRightsFdRead;
      uint32_t fdflags = 0;
      if (inst.op == IrOpcode::FileOpenWrite) {
        oflags = WasiOflagsCreat | WasiOflagsTrunc;
        rightsBase = WasiRightsFdWrite;
      } else if (inst.op == IrOpcode::FileOpenAppend) {
        oflags = WasiOflagsCreat;
        rightsBase = WasiRightsFdWrite;
        fdflags = WasiFdflagAppend;
      }

      emitWasiPathOpen(runtime.stringPtrs[static_cast<size_t>(inst.imm)],
                       runtime.stringLens[static_cast<size_t>(inst.imm)],
                       oflags,
                       rightsBase,
                       fdflags,
                       runtime,
                       out);
      out.push_back(WasmOpLocalTee);
      appendU32Leb(localLayout.fileErrLocal, out);
      out.push_back(WasmOpI32Eqz);
      out.push_back(WasmOpIf);
      out.push_back(WasmValueTypeI32);
      emitI32LoadAtAddress(runtime.openedFdAddr, out);
      out.push_back(WasmOpElse);
      out.push_back(WasmOpLocalGet);
      appendU32Leb(localLayout.fileErrLocal, out);
      out.push_back(WasmOpI32Const);
      appendS32Leb(static_cast<int32_t>(WasmFileHandleErrorBit), out);
      out.push_back(WasmOpI32Or);
      out.push_back(WasmOpEnd);
      return true;
    }
    case IrOpcode::FileClose: {
      if (!runtime.enabled || !runtime.hasFileOps || !localLayout.hasFileHelpers) {
        error = "wasm emitter missing file runtime support in function: " + functionName;
        return false;
      }
      out.push_back(WasmOpLocalSet);
      appendU32Leb(localLayout.fileHandleLocal, out);
      out.push_back(WasmOpLocalGet);
      appendU32Leb(localLayout.fileHandleLocal, out);
      out.push_back(WasmOpI32Const);
      appendS32Leb(static_cast<int32_t>(WasmFileHandleErrorBit), out);
      out.push_back(WasmOpI32And);
      out.push_back(WasmOpI32Eqz);
      out.push_back(WasmOpIf);
      out.push_back(WasmValueTypeI32);
      out.push_back(WasmOpLocalGet);
      appendU32Leb(localLayout.fileHandleLocal, out);
      out.push_back(WasmOpCall);
      appendU32Leb(runtime.fdCloseImportIndex, out);
      out.push_back(WasmOpElse);
      out.push_back(WasmOpLocalGet);
      appendU32Leb(localLayout.fileHandleLocal, out);
      out.push_back(WasmOpI32Const);
      appendS32Leb(static_cast<int32_t>(WasmFileHandleErrorMask), out);
      out.push_back(WasmOpI32And);
      out.push_back(WasmOpEnd);
      return true;
    }
    case IrOpcode::FileFlush: {
      if (!runtime.enabled || !runtime.hasFileOps || !localLayout.hasFileHelpers) {
        error = "wasm emitter missing file runtime support in function: " + functionName;
        return false;
      }
      out.push_back(WasmOpLocalSet);
      appendU32Leb(localLayout.fileHandleLocal, out);
      out.push_back(WasmOpLocalGet);
      appendU32Leb(localLayout.fileHandleLocal, out);
      out.push_back(WasmOpI32Const);
      appendS32Leb(static_cast<int32_t>(WasmFileHandleErrorBit), out);
      out.push_back(WasmOpI32And);
      out.push_back(WasmOpI32Eqz);
      out.push_back(WasmOpIf);
      out.push_back(WasmValueTypeI32);
      out.push_back(WasmOpLocalGet);
      appendU32Leb(localLayout.fileHandleLocal, out);
      out.push_back(WasmOpCall);
      appendU32Leb(runtime.fdSyncImportIndex, out);
      out.push_back(WasmOpElse);
      out.push_back(WasmOpLocalGet);
      appendU32Leb(localLayout.fileHandleLocal, out);
      out.push_back(WasmOpI32Const);
      appendS32Leb(static_cast<int32_t>(WasmFileHandleErrorMask), out);
      out.push_back(WasmOpI32And);
      out.push_back(WasmOpEnd);
      return true;
    }
    case IrOpcode::FileWriteString: {
      if (!runtime.enabled || !runtime.hasFileWriteOps || !localLayout.hasFileHelpers) {
        error = "wasm emitter missing file write runtime support in function: " + functionName;
        return false;
      }
      if (inst.imm >= runtime.stringPtrs.size()) {
        error = "wasm emitter invalid string index in function: " + functionName;
        return false;
      }
      out.push_back(WasmOpLocalSet);
      appendU32Leb(localLayout.fileHandleLocal, out);
      out.push_back(WasmOpLocalGet);
      appendU32Leb(localLayout.fileHandleLocal, out);
      out.push_back(WasmOpI32Const);
      appendS32Leb(static_cast<int32_t>(WasmFileHandleErrorBit), out);
      out.push_back(WasmOpI32And);
      out.push_back(WasmOpI32Eqz);
      out.push_back(WasmOpIf);
      out.push_back(WasmValueTypeI32);
      emitWasiWriteLiteralFromFdLocal(localLayout.fileHandleLocal,
                                      runtime.stringPtrs[static_cast<size_t>(inst.imm)],
                                      runtime.stringLens[static_cast<size_t>(inst.imm)],
                                      runtime,
                                      out);
      out.push_back(WasmOpElse);
      out.push_back(WasmOpLocalGet);
      appendU32Leb(localLayout.fileHandleLocal, out);
      out.push_back(WasmOpI32Const);
      appendS32Leb(static_cast<int32_t>(WasmFileHandleErrorMask), out);
      out.push_back(WasmOpI32And);
      out.push_back(WasmOpEnd);
      return true;
    }
    case IrOpcode::FileWriteByte: {
      if (!runtime.enabled || !runtime.hasFileWriteOps || !localLayout.hasFileHelpers) {
        error = "wasm emitter missing file write runtime support in function: " + functionName;
        return false;
      }
      out.push_back(WasmOpLocalSet);
      appendU32Leb(localLayout.fileValueLocal, out);
      out.push_back(WasmOpLocalSet);
      appendU32Leb(localLayout.fileHandleLocal, out);
      out.push_back(WasmOpLocalGet);
      appendU32Leb(localLayout.fileHandleLocal, out);
      out.push_back(WasmOpI32Const);
      appendS32Leb(static_cast<int32_t>(WasmFileHandleErrorBit), out);
      out.push_back(WasmOpI32And);
      out.push_back(WasmOpI32Eqz);
      out.push_back(WasmOpIf);
      out.push_back(WasmValueTypeI32);
      out.push_back(WasmOpI32Const);
      appendS32Leb(static_cast<int32_t>(runtime.byteScratchAddr), out);
      out.push_back(WasmOpLocalGet);
      appendU32Leb(localLayout.fileValueLocal, out);
      out.push_back(WasmOpI32Store8);
      appendI32MemArg(0, out);
      emitWasiWriteLiteralFromFdLocal(localLayout.fileHandleLocal, runtime.byteScratchAddr, 1, runtime, out);
      out.push_back(WasmOpElse);
      out.push_back(WasmOpLocalGet);
      appendU32Leb(localLayout.fileHandleLocal, out);
      out.push_back(WasmOpI32Const);
      appendS32Leb(static_cast<int32_t>(WasmFileHandleErrorMask), out);
      out.push_back(WasmOpI32And);
      out.push_back(WasmOpEnd);
      return true;
    }
    case IrOpcode::FileWriteNewline: {
      if (!runtime.enabled || !runtime.hasFileWriteOps || !localLayout.hasFileHelpers) {
        error = "wasm emitter missing file write runtime support in function: " + functionName;
        return false;
      }
      out.push_back(WasmOpLocalSet);
      appendU32Leb(localLayout.fileHandleLocal, out);
      out.push_back(WasmOpLocalGet);
      appendU32Leb(localLayout.fileHandleLocal, out);
      out.push_back(WasmOpI32Const);
      appendS32Leb(static_cast<int32_t>(WasmFileHandleErrorBit), out);
      out.push_back(WasmOpI32And);
      out.push_back(WasmOpI32Eqz);
      out.push_back(WasmOpIf);
      out.push_back(WasmValueTypeI32);
      emitWasiWriteLiteralFromFdLocal(localLayout.fileHandleLocal, runtime.newlineAddr, 1, runtime, out);
      out.push_back(WasmOpElse);
      out.push_back(WasmOpLocalGet);
      appendU32Leb(localLayout.fileHandleLocal, out);
      out.push_back(WasmOpI32Const);
      appendS32Leb(static_cast<int32_t>(WasmFileHandleErrorMask), out);
      out.push_back(WasmOpI32And);
      out.push_back(WasmOpEnd);
      return true;
    }
    case IrOpcode::FileWriteI32:
    case IrOpcode::FileWriteI64:
    case IrOpcode::FileWriteU64: {
      if (!runtime.enabled || !runtime.hasFileWriteOps || !localLayout.hasFileHelpers || !localLayout.hasFileNumericHelpers) {
        error = "wasm emitter missing file numeric write runtime support in function: " + functionName;
        return false;
      }
      if (inst.op == IrOpcode::FileWriteI32) {
        out.push_back(WasmOpLocalSet);
        appendU32Leb(localLayout.fileValueLocal, out);
        out.push_back(WasmOpLocalSet);
        appendU32Leb(localLayout.fileHandleLocal, out);
        out.push_back(WasmOpLocalGet);
        appendU32Leb(localLayout.fileValueLocal, out);
        out.push_back(WasmOpI64ExtendI32S);
        out.push_back(WasmOpLocalSet);
        appendU32Leb(localLayout.fileDigitsValueLocal, out);
      } else {
        out.push_back(WasmOpLocalSet);
        appendU32Leb(localLayout.fileDigitsValueLocal, out);
        out.push_back(WasmOpLocalSet);
        appendU32Leb(localLayout.fileHandleLocal, out);
      }

      out.push_back(WasmOpLocalGet);
      appendU32Leb(localLayout.fileHandleLocal, out);
      out.push_back(WasmOpI32Const);
      appendS32Leb(static_cast<int32_t>(WasmFileHandleErrorBit), out);
      out.push_back(WasmOpI32And);
      out.push_back(WasmOpI32Eqz);
      out.push_back(WasmOpIf);
      out.push_back(WasmValueTypeI32);

      emitWasiWriteDecimalFromI64Local(localLayout.fileHandleLocal,
                                       localLayout.fileDigitsValueLocal,
                                       localLayout.fileDigitsRemLocal,
                                       localLayout.fileDigitsPtrLocal,
                                       localLayout.fileDigitsLenLocal,
                                       localLayout.fileDigitsNegLocal,
                                       inst.op != IrOpcode::FileWriteU64,
                                       runtime,
                                       out);

      out.push_back(WasmOpElse);
      out.push_back(WasmOpLocalGet);
      appendU32Leb(localLayout.fileHandleLocal, out);
      out.push_back(WasmOpI32Const);
      appendS32Leb(static_cast<int32_t>(WasmFileHandleErrorMask), out);
      out.push_back(WasmOpI32And);
      out.push_back(WasmOpEnd);
      return true;
    }
    case IrOpcode::Call:
    case IrOpcode::CallVoid: {
      if (inst.imm >= functionTypes.size()) {
        error = "wasm emitter invalid call target in function: " + functionName;
        return false;
      }
      out.push_back(WasmOpCall);
      appendU32Leb(runtime.functionIndexOffset + static_cast<uint32_t>(inst.imm), out);
      if (inst.op == IrOpcode::CallVoid && !functionTypes[static_cast<size_t>(inst.imm)].results.empty()) {
        out.push_back(WasmOpDrop);
      }
      return true;
    }
    case IrOpcode::ReturnI32:
    case IrOpcode::ReturnF32:
    case IrOpcode::ReturnF64:
    case IrOpcode::ReturnVoid:
      out.push_back(WasmOpReturn);
      return true;
    case IrOpcode::Jump:
    case IrOpcode::JumpIfZero:
      error = "wasm emitter internal error: control flow opcode escaped structured lowering";
      return false;
    default:
      error = "wasm emitter unsupported opcode in function " + functionName + ": " + std::to_string(static_cast<uint32_t>(inst.op));
      return false;
  }
}

bool emitInstructionRange(const IrFunction &function,
                          size_t start,
                          size_t end,
                          const WasmLocalLayout &localLayout,
                          const std::vector<WasmFunctionType> &functionTypes,
                          const WasmRuntimeContext &runtime,
                          std::vector<uint8_t> &out,
                          std::string &error);

struct LoopRegion {
  size_t guardIndex = 0;
  size_t tailJumpIndex = 0;
  size_t endIndex = 0;
};

bool decodeJumpTarget(const IrFunction &function, const IrInstruction &inst, size_t &target) {
  const uint64_t instructionCount = static_cast<uint64_t>(function.instructions.size());
  if (inst.imm > instructionCount) {
    return false;
  }
  target = static_cast<size_t>(inst.imm);
  return true;
}

bool detectCanonicalLoopRegion(
    const IrFunction &function, size_t loopHead, size_t end, LoopRegion &outRegion, std::string &error) {
  bool found = false;
  LoopRegion candidate;

  for (size_t i = loopHead + 1; i < end; ++i) {
    const IrInstruction &inst = function.instructions[i];
    if (inst.op != IrOpcode::Jump) {
      continue;
    }

    size_t jumpTarget = 0;
    if (!decodeJumpTarget(function, inst, jumpTarget)) {
      error = "wasm emitter malformed jump target in function: " + function.name;
      return false;
    }
    if (jumpTarget != loopHead) {
      continue;
    }
    if (i + 1 > end) {
      continue;
    }

    size_t guardIndex = static_cast<size_t>(-1);
    const size_t loopEnd = i + 1;
    for (size_t j = loopHead; j < i; ++j) {
      const IrInstruction &guard = function.instructions[j];
      if (guard.op != IrOpcode::JumpIfZero) {
        continue;
      }
      size_t guardTarget = 0;
      if (!decodeJumpTarget(function, guard, guardTarget)) {
        error = "wasm emitter malformed jump target in function: " + function.name;
        return false;
      }
      if (guardTarget == loopEnd) {
        guardIndex = j;
        break;
      }
    }
    if (guardIndex == static_cast<size_t>(-1)) {
      continue;
    }

    candidate.guardIndex = guardIndex;
    candidate.tailJumpIndex = i;
    candidate.endIndex = loopEnd;
    found = true;
  }

  if (!found) {
    return false;
  }
  outRegion = candidate;
  return true;
}

bool emitIfRegion(const IrFunction &function,
                  size_t conditionIndex,
                  size_t trueStart,
                  size_t trueEnd,
                  size_t falseStart,
                  size_t falseEnd,
                  const WasmLocalLayout &localLayout,
                  const std::vector<WasmFunctionType> &functionTypes,
                  const WasmRuntimeContext &runtime,
                  std::vector<uint8_t> &out,
                  std::string &error) {
  (void)conditionIndex;
  out.push_back(WasmOpIf);
  out.push_back(WasmBlockTypeVoid);
  if (!emitInstructionRange(function, trueStart, trueEnd, localLayout, functionTypes, runtime, out, error)) {
    return false;
  }
  if (falseStart < falseEnd) {
    out.push_back(WasmOpElse);
    if (!emitInstructionRange(
            function, falseStart, falseEnd, localLayout, functionTypes, runtime, out, error)) {
      return false;
    }
  }
  out.push_back(WasmOpEnd);
  return true;
}

bool emitInstructionRange(const IrFunction &function,
                          size_t start,
                          size_t end,
                          const WasmLocalLayout &localLayout,
                          const std::vector<WasmFunctionType> &functionTypes,
                          const WasmRuntimeContext &runtime,
                          std::vector<uint8_t> &out,
                          std::string &error) {
  size_t index = start;
  while (index < end) {
    LoopRegion loopRegion;
    if (detectCanonicalLoopRegion(function, index, end, loopRegion, error)) {
      out.push_back(WasmOpBlock);
      out.push_back(WasmBlockTypeVoid);
      out.push_back(WasmOpLoop);
      out.push_back(WasmBlockTypeVoid);

      if (!emitInstructionRange(
              function,
              index,
              loopRegion.guardIndex,
              localLayout,
              functionTypes,
              runtime,
              out,
              error)) {
        return false;
      }
      out.push_back(WasmOpI32Eqz);
      out.push_back(WasmOpBrIf);
      appendU32Leb(1, out);

      if (!emitInstructionRange(
              function,
              loopRegion.guardIndex + 1,
              loopRegion.tailJumpIndex,
              localLayout,
              functionTypes,
              runtime,
              out,
              error)) {
        return false;
      }
      out.push_back(WasmOpBr);
      appendU32Leb(0, out);
      out.push_back(WasmOpEnd);
      out.push_back(WasmOpEnd);

      index = loopRegion.endIndex;
      continue;
    }

    const IrInstruction &inst = function.instructions[index];

    if (inst.op == IrOpcode::JumpIfZero) {
      size_t target = 0;
      if (!decodeJumpTarget(function, inst, target)) {
        error = "wasm emitter malformed jump target in function: " + function.name;
        return false;
      }
      if (target <= index || target > end) {
        error = "wasm emitter unsupported control-flow pattern in function: " + function.name;
        return false;
      }

      if (target > 0 && target - 1 > index) {
        const IrInstruction &tail = function.instructions[target - 1];
        if (tail.op == IrOpcode::Jump) {
          const size_t jumpTarget = static_cast<size_t>(tail.imm);
          if (jumpTarget <= target - 1) {
            error = "wasm emitter unsupported control-flow pattern in function: " + function.name;
            return false;
          }
          if (jumpTarget > end) {
            error = "wasm emitter unsupported control-flow pattern in function: " + function.name;
            return false;
          }

          out.push_back(WasmOpI32Eqz);
          if (!emitIfRegion(function,
                            index,
                            index + 1,
                            target - 1,
                            target,
                            jumpTarget,
                            localLayout,
                            functionTypes,
                            runtime,
                            out,
                            error)) {
            return false;
          }
          index = jumpTarget;
          continue;
        }
      }

      out.push_back(WasmOpI32Eqz);
      if (!emitIfRegion(function,
                        index,
                        index + 1,
                        target,
                        target,
                        target,
                        localLayout,
                        functionTypes,
                        runtime,
                        out,
                        error)) {
        return false;
      }
      index = target;
      continue;
    }

    if (inst.op == IrOpcode::Jump) {
      size_t target = 0;
      if (!decodeJumpTarget(function, inst, target)) {
        error = "wasm emitter malformed jump target in function: " + function.name;
        return false;
      }
      (void)target;
      error = "wasm emitter unsupported control-flow pattern in function: " + function.name;
      return false;
    }

    if (!emitSimpleInstruction(inst, localLayout, functionTypes, runtime, out, error, function.name)) {
      return false;
    }
    ++index;
  }

  return true;
}

bool lowerFunctionCode(const IrFunction &function,
                       const std::vector<WasmFunctionType> &functionTypes,
                       const WasmRuntimeContext &runtime,
                       WasmCodeBody &outBody,
                       std::string &error) {
  const WasmLocalLayout localLayout = computeLocalLayout(function, error);
  if (!error.empty()) {
    return false;
  }

  outBody.localDecls.clear();
  outBody.instructions.clear();
  appendLocalDecls(localLayout, outBody.localDecls);

  if (!emitInstructionRange(
          function,
          0,
          function.instructions.size(),
          localLayout,
          functionTypes,
          runtime,
          outBody.instructions,
          error)) {
    return false;
  }
  outBody.instructions.push_back(WasmOpEnd);
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
