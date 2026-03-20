#include "WasmEmitterInternal.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace primec {

namespace {

constexpr uint8_t WasmValueTypeI32 = 0x7f;
constexpr uint8_t WasmBlockTypeVoid = 0x40;

constexpr uint8_t WasmOpIf = 0x04;
constexpr uint8_t WasmOpElse = 0x05;
constexpr uint8_t WasmOpEnd = 0x0b;
constexpr uint8_t WasmOpUnreachable = 0x00;
constexpr uint8_t WasmOpCall = 0x10;
constexpr uint8_t WasmOpDrop = 0x1a;
constexpr uint8_t WasmOpLocalGet = 0x20;
constexpr uint8_t WasmOpLocalSet = 0x21;
constexpr uint8_t WasmOpLocalTee = 0x22;
constexpr uint8_t WasmOpI32Load = 0x28;
constexpr uint8_t WasmOpI32Load8U = 0x2d;
constexpr uint8_t WasmOpI32Store8 = 0x3a;
constexpr uint8_t WasmOpI32Const = 0x41;
constexpr uint8_t WasmOpI32Eqz = 0x45;
constexpr uint8_t WasmOpI32Eq = 0x46;
constexpr uint8_t WasmOpI32GeU = 0x4f;
constexpr uint8_t WasmOpI32Add = 0x6a;
constexpr uint8_t WasmOpI32Sub = 0x6b;
constexpr uint8_t WasmOpI32Mul = 0x6c;
constexpr uint8_t WasmOpI32And = 0x71;
constexpr uint8_t WasmOpI32Or = 0x72;
constexpr uint8_t WasmOpI64ExtendI32S = 0xac;

constexpr uint32_t WasiFdflagAppend = 1u;
constexpr uint32_t WasiOflagsCreat = 1u;
constexpr uint32_t WasiOflagsTrunc = 8u;
constexpr uint64_t WasiRightsFdRead = 2ull;
constexpr uint64_t WasiRightsFdWrite = 64ull;
constexpr uint32_t WasmFileHandleErrorBit = 0x80000000u;
constexpr uint32_t WasmFileHandleErrorMask = 0x7fffffffu;

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

void emitMaskedFileHandleError(uint32_t fileHandleLocal, std::vector<uint8_t> &out) {
  out.push_back(WasmOpLocalGet);
  appendU32Leb(fileHandleLocal, out);
  out.push_back(WasmOpI32Const);
  appendS32Leb(static_cast<int32_t>(WasmFileHandleErrorMask), out);
  out.push_back(WasmOpI32And);
}

} // namespace

bool emitIoInstruction(const IrInstruction &inst,
                       const WasmLocalLayout &localLayout,
                       const WasmRuntimeContext &runtime,
                       std::vector<uint8_t> &out,
                       std::string &error,
                       const std::string &functionName) {
  switch (inst.op) {
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
      emitMaskedFileHandleError(localLayout.fileHandleLocal, out);
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
      emitMaskedFileHandleError(localLayout.fileHandleLocal, out);
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
      emitMaskedFileHandleError(localLayout.fileHandleLocal, out);
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
      emitMaskedFileHandleError(localLayout.fileHandleLocal, out);
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
      emitMaskedFileHandleError(localLayout.fileHandleLocal, out);
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
      emitMaskedFileHandleError(localLayout.fileHandleLocal, out);
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
      emitMaskedFileHandleError(localLayout.fileHandleLocal, out);
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
      emitMaskedFileHandleError(localLayout.fileHandleLocal, out);
      out.push_back(WasmOpEnd);
      return true;
    }
    default:
      error = "wasm emitter internal error: non-io opcode routed through io lowering";
      return false;
  }
}

} // namespace primec
