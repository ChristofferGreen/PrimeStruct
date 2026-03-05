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
constexpr uint8_t WasmSectionExport = 7;
constexpr uint8_t WasmSectionCode = 10;
constexpr uint8_t WasmSectionData = 11;

constexpr uint8_t WasmValueTypeI32 = 0x7f;
constexpr uint8_t WasmValueTypeF32 = 0x7d;
constexpr uint8_t WasmValueTypeF64 = 0x7c;
constexpr uint8_t WasmBlockTypeVoid = 0x40;

constexpr uint8_t WasmOpIf = 0x04;
constexpr uint8_t WasmOpElse = 0x05;
constexpr uint8_t WasmOpEnd = 0x0b;
constexpr uint8_t WasmOpReturn = 0x0f;
constexpr uint8_t WasmOpDrop = 0x1a;
constexpr uint8_t WasmOpBlock = 0x02;
constexpr uint8_t WasmOpLoop = 0x03;
constexpr uint8_t WasmOpBr = 0x0c;
constexpr uint8_t WasmOpBrIf = 0x0d;
constexpr uint8_t WasmOpLocalGet = 0x20;
constexpr uint8_t WasmOpLocalSet = 0x21;
constexpr uint8_t WasmOpLocalTee = 0x22;
constexpr uint8_t WasmOpI32Const = 0x41;
constexpr uint8_t WasmOpI32Eqz = 0x45;
constexpr uint8_t WasmOpI32Eq = 0x46;
constexpr uint8_t WasmOpI32Ne = 0x47;
constexpr uint8_t WasmOpI32LtS = 0x48;
constexpr uint8_t WasmOpI32GtS = 0x4a;
constexpr uint8_t WasmOpI32LeS = 0x4c;
constexpr uint8_t WasmOpI32GeS = 0x4e;
constexpr uint8_t WasmOpI32Add = 0x6a;
constexpr uint8_t WasmOpI32Sub = 0x6b;
constexpr uint8_t WasmOpI32Mul = 0x6c;
constexpr uint8_t WasmOpI32DivS = 0x6d;
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

uint32_t computeLocalCount(const IrFunction &function, bool &needsDupTempLocal, std::string &error) {
  uint64_t maxLocalIndex = 0;
  bool hasLocal = false;
  needsDupTempLocal = false;
  for (const IrInstruction &inst : function.instructions) {
    if (inst.op == IrOpcode::LoadLocal || inst.op == IrOpcode::StoreLocal) {
      maxLocalIndex = std::max(maxLocalIndex, inst.imm);
      hasLocal = true;
    }
    if (inst.op == IrOpcode::Dup) {
      needsDupTempLocal = true;
    }
  }
  const uint64_t baseCount = hasLocal ? (maxLocalIndex + 1) : 0;
  const uint64_t totalCount = baseCount + (needsDupTempLocal ? 1 : 0);
  if (totalCount > static_cast<uint64_t>(std::numeric_limits<uint32_t>::max())) {
    error = "wasm emitter local count exceeds 32-bit limit in function: " + function.name;
    return 0;
  }
  return static_cast<uint32_t>(baseCount);
}

void appendLocalDecls(uint32_t localCount, bool needsDupTempLocal, std::vector<uint8_t> &out) {
  const uint32_t totalLocals = localCount + (needsDupTempLocal ? 1u : 0u);
  if (totalLocals == 0) {
    appendU32Leb(0, out);
    return;
  }
  appendU32Leb(1, out);
  appendU32Leb(totalLocals, out);
  out.push_back(WasmValueTypeI32);
}

bool emitSimpleInstruction(const IrInstruction &inst,
                           uint32_t localCount,
                           bool needsDupTempLocal,
                           std::vector<uint8_t> &out,
                           std::string &error,
                           const std::string &functionName) {
  const uint32_t dupTempIndex = localCount;
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
      if (inst.imm >= localCount) {
        error = "wasm emitter local index out of range in function: " + functionName;
        return false;
      }
      out.push_back(WasmOpLocalGet);
      appendU32Leb(static_cast<uint32_t>(inst.imm), out);
      return true;
    case IrOpcode::StoreLocal:
      if (inst.imm >= localCount) {
        error = "wasm emitter local index out of range in function: " + functionName;
        return false;
      }
      out.push_back(WasmOpLocalSet);
      appendU32Leb(static_cast<uint32_t>(inst.imm), out);
      return true;
    case IrOpcode::Dup:
      if (!needsDupTempLocal) {
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
                          uint32_t localCount,
                          bool needsDupTempLocal,
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
                  uint32_t localCount,
                  bool needsDupTempLocal,
                  std::vector<uint8_t> &out,
                  std::string &error) {
  (void)conditionIndex;
  out.push_back(WasmOpIf);
  out.push_back(WasmBlockTypeVoid);
  if (!emitInstructionRange(function, trueStart, trueEnd, localCount, needsDupTempLocal, out, error)) {
    return false;
  }
  if (falseStart < falseEnd) {
    out.push_back(WasmOpElse);
    if (!emitInstructionRange(function, falseStart, falseEnd, localCount, needsDupTempLocal, out, error)) {
      return false;
    }
  }
  out.push_back(WasmOpEnd);
  return true;
}

bool emitInstructionRange(const IrFunction &function,
                          size_t start,
                          size_t end,
                          uint32_t localCount,
                          bool needsDupTempLocal,
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
              function, index, loopRegion.guardIndex, localCount, needsDupTempLocal, out, error)) {
        return false;
      }
      out.push_back(WasmOpI32Eqz);
      out.push_back(WasmOpBrIf);
      appendU32Leb(1, out);

      if (!emitInstructionRange(
              function, loopRegion.guardIndex + 1, loopRegion.tailJumpIndex, localCount, needsDupTempLocal, out, error)) {
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
                            localCount,
                            needsDupTempLocal,
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
                        localCount,
                        needsDupTempLocal,
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

    if (!emitSimpleInstruction(inst, localCount, needsDupTempLocal, out, error, function.name)) {
      return false;
    }
    ++index;
  }

  return true;
}

bool lowerFunctionCode(const IrFunction &function, WasmCodeBody &outBody, std::string &error) {
  bool needsDupTempLocal = false;
  const uint32_t localCount = computeLocalCount(function, needsDupTempLocal, error);
  if (!error.empty()) {
    return false;
  }

  outBody.localDecls.clear();
  outBody.instructions.clear();
  appendLocalDecls(localCount, needsDupTempLocal, outBody.localDecls);

  if (!emitInstructionRange(
          function, 0, function.instructions.size(), localCount, needsDupTempLocal, outBody.instructions, error)) {
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

  std::vector<WasmFunctionType> types;
  std::vector<uint32_t> functionTypeIndexes;
  std::vector<WasmCodeBody> codeBodies;
  types.reserve(module.functions.size());
  functionTypeIndexes.reserve(module.functions.size());
  codeBodies.reserve(module.functions.size());

  for (size_t functionIndex = 0; functionIndex < module.functions.size(); ++functionIndex) {
    WasmFunctionType functionType;
    if (!inferFunctionType(module.functions[functionIndex], functionType, error)) {
      return false;
    }
    types.push_back(std::move(functionType));
    functionTypeIndexes.push_back(static_cast<uint32_t>(functionIndex));

    WasmCodeBody codeBody;
    if (!lowerFunctionCode(module.functions[functionIndex], codeBody, error)) {
      return false;
    }
    codeBodies.push_back(std::move(codeBody));
  }

  std::vector<WasmExport> exports;
  if (module.entryIndex >= 0) {
    WasmExport exportEntry;
    exportEntry.name = wasmExportName(module.functions[static_cast<size_t>(module.entryIndex)].name);
    exportEntry.kind = WasmFunctionKind;
    exportEntry.index = static_cast<uint32_t>(module.entryIndex);
    exports.push_back(std::move(exportEntry));
  }

  const std::vector<WasmImport> imports;
  const std::vector<WasmDataSegment> dataSegments;

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
