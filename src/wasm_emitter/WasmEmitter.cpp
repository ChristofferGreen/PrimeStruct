#include "WasmEmitterInternal.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace primec {

namespace {

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
constexpr uint8_t WasmOpI32Load8U = 0x2d;
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
constexpr uint8_t WasmOpI64Eq = 0x51;
constexpr uint8_t WasmOpI64Ne = 0x52;
constexpr uint8_t WasmOpI64LtS = 0x53;
constexpr uint8_t WasmOpI64LtU = 0x54;
constexpr uint8_t WasmOpI64GtS = 0x55;
constexpr uint8_t WasmOpI64GtU = 0x56;
constexpr uint8_t WasmOpI64LeS = 0x57;
constexpr uint8_t WasmOpI64LeU = 0x58;
constexpr uint8_t WasmOpI64GeS = 0x59;
constexpr uint8_t WasmOpI64GeU = 0x5a;
constexpr uint8_t WasmOpI32Add = 0x6a;
constexpr uint8_t WasmOpI32Sub = 0x6b;
constexpr uint8_t WasmOpI32Mul = 0x6c;
constexpr uint8_t WasmOpI32DivS = 0x6d;
constexpr uint8_t WasmOpI32And = 0x71;
constexpr uint8_t WasmOpI32Or = 0x72;
constexpr uint8_t WasmOpI64Add = 0x7c;
constexpr uint8_t WasmOpI64Sub = 0x7d;
constexpr uint8_t WasmOpI64Mul = 0x7e;
constexpr uint8_t WasmOpI64DivS = 0x7f;
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

bool emitSimpleInstructionImpl(const IrInstruction &inst,
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
    case IrOpcode::PushI64:
      out.push_back(WasmOpI64Const);
      appendS64Leb(static_cast<int64_t>(inst.imm), out);
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
    case IrOpcode::AddI64:
      out.push_back(WasmOpI64Add);
      return true;
    case IrOpcode::SubI64:
      out.push_back(WasmOpI64Sub);
      return true;
    case IrOpcode::MulI64:
      out.push_back(WasmOpI64Mul);
      return true;
    case IrOpcode::DivI64:
      out.push_back(WasmOpI64DivS);
      return true;
    case IrOpcode::DivU64:
      out.push_back(WasmOpI64DivU);
      return true;
    case IrOpcode::NegI64:
      out.push_back(WasmOpI64Const);
      appendS64Leb(-1, out);
      out.push_back(WasmOpI64Mul);
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
    case IrOpcode::CmpEqI64:
      out.push_back(WasmOpI64Eq);
      return true;
    case IrOpcode::CmpNeI64:
      out.push_back(WasmOpI64Ne);
      return true;
    case IrOpcode::CmpLtI64:
      out.push_back(WasmOpI64LtS);
      return true;
    case IrOpcode::CmpLeI64:
      out.push_back(WasmOpI64LeS);
      return true;
    case IrOpcode::CmpGtI64:
      out.push_back(WasmOpI64GtS);
      return true;
    case IrOpcode::CmpGeI64:
      out.push_back(WasmOpI64GeS);
      return true;
    case IrOpcode::CmpLtU64:
      out.push_back(WasmOpI64LtU);
      return true;
    case IrOpcode::CmpLeU64:
      out.push_back(WasmOpI64LeU);
      return true;
    case IrOpcode::CmpGtU64:
      out.push_back(WasmOpI64GtU);
      return true;
    case IrOpcode::CmpGeU64:
      out.push_back(WasmOpI64GeU);
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
    case IrOpcode::PrintI32:
    case IrOpcode::PrintI64:
    case IrOpcode::PrintU64: {
      if (!runtime.enabled || !runtime.hasOutputOps || !localLayout.hasFileHelpers || !localLayout.hasFileNumericHelpers) {
        error = "wasm emitter missing numeric print runtime support in function: " + functionName;
        return false;
      }
      const uint64_t flags = decodePrintFlags(inst.imm);
      const uint32_t fd = (flags & PrintFlagStderr) ? 2u : 1u;
      out.push_back(WasmOpI32Const);
      appendS32Leb(static_cast<int32_t>(fd), out);
      out.push_back(WasmOpLocalSet);
      appendU32Leb(localLayout.fileHandleLocal, out);
      if (inst.op == IrOpcode::PrintI32) {
        out.push_back(WasmOpI64ExtendI32S);
      }
      out.push_back(WasmOpLocalSet);
      appendU32Leb(localLayout.fileDigitsValueLocal, out);
      emitWasiWriteDecimalFromI64Local(localLayout.fileHandleLocal,
                                       localLayout.fileDigitsValueLocal,
                                       localLayout.fileDigitsRemLocal,
                                       localLayout.fileDigitsPtrLocal,
                                       localLayout.fileDigitsLenLocal,
                                       localLayout.fileDigitsNegLocal,
                                       inst.op != IrOpcode::PrintU64,
                                       runtime,
                                       out);
      out.push_back(WasmOpDrop);
      if ((flags & PrintFlagNewline) != 0) {
        emitWasiWriteLiteralFromFdLocal(localLayout.fileHandleLocal, runtime.newlineAddr, 1, runtime, out);
        out.push_back(WasmOpDrop);
      }
      return true;
    }
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
    case IrOpcode::PrintStringDynamic: {
      if (!runtime.enabled || !runtime.hasOutputOps || !localLayout.hasFileHelpers || !localLayout.hasFileNumericHelpers) {
        error = "wasm emitter missing dynamic string print runtime support in function: " + functionName;
        return false;
      }
      const uint64_t flags = decodePrintFlags(inst.imm);
      const uint32_t fd = (flags & PrintFlagStderr) ? 2u : 1u;
      out.push_back(WasmOpI32Const);
      appendS32Leb(static_cast<int32_t>(fd), out);
      out.push_back(WasmOpLocalSet);
      appendU32Leb(localLayout.fileHandleLocal, out);
      out.push_back(WasmOpLocalSet);
      appendU32Leb(localLayout.fileDigitsValueLocal, out);
      emitWasiWriteDynamicStringFromI64Local(localLayout.fileHandleLocal,
                                             localLayout.fileDigitsValueLocal,
                                             runtime,
                                             out);
      if ((flags & PrintFlagNewline) != 0) {
        emitWasiWriteLiteralFromFdLocal(localLayout.fileHandleLocal, runtime.newlineAddr, 1, runtime, out);
        out.push_back(WasmOpDrop);
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
    case IrOpcode::FileOpenAppend:
    case IrOpcode::FileOpenReadDynamic:
    case IrOpcode::FileOpenWriteDynamic:
    case IrOpcode::FileOpenAppendDynamic: {
      if (!runtime.enabled || !runtime.hasFileOps || !localLayout.hasFileHelpers) {
        error = "wasm emitter missing file runtime support in function: " + functionName;
        return false;
      }

      uint32_t oflags = 0;
      uint64_t rightsBase = WasiRightsFdRead;
      uint32_t fdflags = 0;
      if (inst.op == IrOpcode::FileOpenWrite || inst.op == IrOpcode::FileOpenWriteDynamic) {
        oflags = WasiOflagsCreat | WasiOflagsTrunc;
        rightsBase = WasiRightsFdWrite;
      } else if (inst.op == IrOpcode::FileOpenAppend || inst.op == IrOpcode::FileOpenAppendDynamic) {
        oflags = WasiOflagsCreat;
        rightsBase = WasiRightsFdWrite;
        fdflags = WasiFdflagAppend;
      }

      if (inst.op == IrOpcode::FileOpenReadDynamic || inst.op == IrOpcode::FileOpenWriteDynamic ||
          inst.op == IrOpcode::FileOpenAppendDynamic) {
        out.push_back(WasmOpLocalSet);
        appendU32Leb(localLayout.fileDigitsValueLocal, out);
        emitWasiPathOpenDynamicFromI64Local(
            localLayout.fileDigitsValueLocal, localLayout.fileErrLocal, oflags, rightsBase, fdflags, runtime, out);
      } else {
        if (inst.imm >= runtime.stringPtrs.size()) {
          error = "wasm emitter invalid string index in function: " + functionName;
          return false;
        }
        emitWasiPathOpen(runtime.stringPtrs[static_cast<size_t>(inst.imm)],
                         runtime.stringLens[static_cast<size_t>(inst.imm)],
                         oflags,
                         rightsBase,
                         fdflags,
                         runtime,
                         out);
      }
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
    case IrOpcode::FileReadByte: {
      if (!runtime.enabled || !runtime.hasFileReadOps || !localLayout.hasFileHelpers) {
        error = "wasm emitter missing file read runtime support in function: " + functionName;
        return false;
      }
      if (inst.imm >= localLayout.irLocalCount) {
        error = "wasm emitter local index out of range in function: " + functionName;
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
      emitWasiReadByteFromFdLocal(localLayout.fileHandleLocal, runtime, out);
      out.push_back(WasmOpLocalSet);
      appendU32Leb(localLayout.fileErrLocal, out);
      out.push_back(WasmOpLocalGet);
      appendU32Leb(localLayout.fileErrLocal, out);
      out.push_back(WasmOpI32Eqz);
      out.push_back(WasmOpIf);
      out.push_back(WasmValueTypeI32);
      emitI32LoadAtAddress(runtime.nwrittenAddr, out);
      out.push_back(WasmOpI32Eqz);
      out.push_back(WasmOpIf);
      out.push_back(WasmValueTypeI32);
      out.push_back(WasmOpI32Const);
      appendS32Leb(static_cast<int32_t>(FileReadEofCode), out);
      out.push_back(WasmOpElse);
      out.push_back(WasmOpI32Const);
      appendS32Leb(static_cast<int32_t>(runtime.byteScratchAddr), out);
      out.push_back(WasmOpI32Load8U);
      appendI32MemArg(0, out);
      out.push_back(WasmOpLocalSet);
      appendU32Leb(static_cast<uint32_t>(inst.imm), out);
      out.push_back(WasmOpI32Const);
      appendS32Leb(0, out);
      out.push_back(WasmOpEnd);
      out.push_back(WasmOpElse);
      out.push_back(WasmOpLocalGet);
      appendU32Leb(localLayout.fileErrLocal, out);
      out.push_back(WasmOpEnd);
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
    case IrOpcode::FileWriteStringDynamic: {
      if (!runtime.enabled || !runtime.hasFileWriteOps || !localLayout.hasFileHelpers || !localLayout.hasFileNumericHelpers) {
        error = "wasm emitter missing dynamic file write runtime support in function: " + functionName;
        return false;
      }
      out.push_back(WasmOpLocalSet);
      appendU32Leb(localLayout.fileDigitsValueLocal, out);
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
      emitWasiWriteDynamicStringFromI64Local(localLayout.fileHandleLocal,
                                             localLayout.fileDigitsValueLocal,
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
    case IrOpcode::ReturnI64:
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

} // namespace

bool emitSimpleInstruction(const IrInstruction &inst,
                           const WasmLocalLayout &localLayout,
                           const std::vector<WasmFunctionType> &functionTypes,
                           const WasmRuntimeContext &runtime,
                           std::vector<uint8_t> &out,
                           std::string &error,
                           const std::string &functionName) {
  return emitSimpleInstructionImpl(inst, localLayout, functionTypes, runtime, out, error, functionName);
}

} // namespace primec
