#include "WasmEmitterInternal.h"

namespace primec {

namespace {

constexpr uint8_t WasmValueTypeI32 = 0x7f;
constexpr uint8_t WasmValueTypeI64 = 0x7e;
constexpr uint8_t WasmOpEnd = 0x0b;
constexpr uint8_t WasmOpI32Const = 0x41;
constexpr uint32_t WasmNumericScratchBytes = 32u;

bool opcodeNeedsWasiRuntime(IrOpcode op) {
  switch (op) {
    case IrOpcode::PrintI32:
    case IrOpcode::PrintI64:
    case IrOpcode::PrintU64:
    case IrOpcode::PushArgc:
    case IrOpcode::PrintString:
    case IrOpcode::PrintStringDynamic:
    case IrOpcode::PrintArgv:
    case IrOpcode::PrintArgvUnsafe:
    case IrOpcode::FileOpenRead:
    case IrOpcode::FileOpenWrite:
    case IrOpcode::FileOpenAppend:
    case IrOpcode::FileOpenReadDynamic:
    case IrOpcode::FileOpenWriteDynamic:
    case IrOpcode::FileOpenAppendDynamic:
    case IrOpcode::FileReadByte:
    case IrOpcode::FileClose:
    case IrOpcode::FileFlush:
    case IrOpcode::FileWriteI32:
    case IrOpcode::FileWriteI64:
    case IrOpcode::FileWriteU64:
    case IrOpcode::FileWriteString:
    case IrOpcode::FileWriteStringDynamic:
    case IrOpcode::FileWriteByte:
    case IrOpcode::FileWriteNewline:
      return true;
    default:
      return false;
  }
}

void appendDataSegmentAt(uint32_t address,
                         const std::vector<uint8_t> &bytes,
                         std::vector<WasmDataSegment> &segments) {
  WasmDataSegment segment;
  segment.memoryIndex = 0;
  segment.offsetExpression.push_back(WasmOpI32Const);
  appendS32Leb(static_cast<int32_t>(address), segment.offsetExpression);
  segment.offsetExpression.push_back(WasmOpEnd);
  segment.bytes = bytes;
  segments.push_back(std::move(segment));
}

} // namespace

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
      if (inst.op == IrOpcode::PrintI32 || inst.op == IrOpcode::PrintI64 || inst.op == IrOpcode::PrintU64 ||
          inst.op == IrOpcode::PrintString || inst.op == IrOpcode::PrintStringDynamic ||
          inst.op == IrOpcode::PrintArgv || inst.op == IrOpcode::PrintArgvUnsafe) {
        runtime.hasOutputOps = true;
      }
      if (inst.op == IrOpcode::FileOpenRead || inst.op == IrOpcode::FileOpenWrite || inst.op == IrOpcode::FileOpenAppend ||
          inst.op == IrOpcode::FileOpenReadDynamic || inst.op == IrOpcode::FileOpenWriteDynamic ||
          inst.op == IrOpcode::FileOpenAppendDynamic ||
          inst.op == IrOpcode::FileReadByte || inst.op == IrOpcode::FileClose || inst.op == IrOpcode::FileFlush ||
          inst.op == IrOpcode::FileWriteI32 ||
          inst.op == IrOpcode::FileWriteI64 || inst.op == IrOpcode::FileWriteU64 ||
          inst.op == IrOpcode::FileWriteString || inst.op == IrOpcode::FileWriteStringDynamic ||
          inst.op == IrOpcode::FileWriteByte || inst.op == IrOpcode::FileWriteNewline) {
        runtime.hasFileOps = true;
      }
      if (inst.op == IrOpcode::FileReadByte) {
        runtime.hasFileReadOps = true;
      }
      if (inst.op == IrOpcode::FileWriteI32 || inst.op == IrOpcode::FileWriteI64 || inst.op == IrOpcode::FileWriteU64 ||
          inst.op == IrOpcode::FileWriteString || inst.op == IrOpcode::FileWriteStringDynamic ||
          inst.op == IrOpcode::FileWriteByte || inst.op == IrOpcode::FileWriteNewline) {
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
    if (runtime.hasFileReadOps) {
      WasmFunctionType fdReadType;
      fdReadType.params = {WasmValueTypeI32, WasmValueTypeI32, WasmValueTypeI32, WasmValueTypeI32};
      fdReadType.results = {WasmValueTypeI32};
      runtime.fdReadImportIndex = static_cast<uint32_t>(imports.size());
      imports.push_back({"wasi_snapshot_preview1", "fd_read", WasmFunctionKind, static_cast<uint32_t>(types.size())});
      types.push_back(std::move(fdReadType));
    }

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

} // namespace primec
