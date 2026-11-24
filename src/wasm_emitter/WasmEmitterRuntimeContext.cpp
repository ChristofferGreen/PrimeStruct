#include "WasmEmitterInternal.h"

#include "primec/ir/IrNativeTextHost.h"

namespace primec {

namespace {

constexpr uint8_t WasmValueTypeI32 = 0x7f;
constexpr uint8_t WasmValueTypeI64 = 0x7e;
constexpr uint8_t WasmOpEnd = 0x0b;
constexpr uint8_t WasmOpI32Const = 0x41;
constexpr uint32_t WasmNumericScratchBytes = 32u;
// The shadow stack of local frames (WasmEmitterMemory.cpp): 16 pages of 8-byte slots, aligned to the slot address stride.
constexpr uint32_t WasmSlotBytes = 8u;
constexpr uint32_t WasmShadowStackAlignment = 16u;
constexpr uint32_t WasmShadowStackSlots = 16u * 65536u / WasmSlotBytes;

uint32_t shadowStackPages(uint32_t shadowBase) {
  return (shadowBase + WasmShadowStackSlots * WasmSlotBytes) / 65536u + 1u;
}

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

// Places the heap regions from `start` on (8-byte aligned) and returns where they end.
uint32_t layoutHeapRegions(uint32_t start, WasmRuntimeContext &runtime) {
  const auto place = [&](uint32_t bytes) {
    const uint32_t begin = (start + 7u) / 8u * 8u;
    start = begin + bytes;
    return begin;
  };
  runtime.heapValues = place(WasmHeapInitialSlots * 8u);
  runtime.heapStates = place(WasmHeapInitialSlots * 2u);
  runtime.heapCounts = place(WasmHeapInitialSlots * 4u);
  runtime.heapRuns = place((WasmHeapInitialSlots / 2u + 1u) * 8u);
  if (runtime.hasBytes) {
    runtime.byteBase = place(WasmByteInitialGranules * 8u);
    runtime.byteOwner = place(WasmByteInitialGranules * 4u);
    runtime.byteCounts = place(WasmByteInitialGranules * 4u);
    runtime.byteGens = place(WasmByteInitialGranules * 2u);
    runtime.byteRuns = place((WasmByteInitialGranules / 2u + 1u) * 8u);
  }
  if (runtime.hasText) {
    runtime.textClasses = place(WasmTextClassCount * 16u);
    runtime.textRecords = place(WasmTextInitialRecords * WasmTextRecordBytes);
    runtime.textMeta = place(WasmTextMetaBytes);
    runtime.textMetaHeads = place(WasmTextMetaBuckets * 4u);
  }
  return start;
}

} // namespace

bool buildWasiRuntimeContext(const IrModule &module,
                             bool wasi,
                             std::vector<WasmImport> &imports,
                             std::vector<WasmFunctionType> &types,
                             WasmRuntimeContext &runtime,
                             std::string &error) {
  runtime = WasmRuntimeContext{};
  runtime.hasLocalMemory = wasmModuleUsesLocalMemory(module);
  for (const IrHostImport &hostImport : module.hostImports) {
    runtime.statHostImports.push_back(nativeTextHost(hostImport.name) == NativeTextHost::Stat);
    runtime.textHostKinds.push_back(nativeTextHost(hostImport.name));
    runtime.textHostReturns.push_back(hostImport.returnKind);
  }
  runtime.hasText = wasmModuleCallsTextHeap(module);
  if (runtime.hasText) {
    runtime.hasHeap = true;
    runtime.hasBytes = true;
  }
  for (const IrFunction &function : module.functions) {
    for (const IrInstruction &inst : function.instructions) {
      if (inst.op == IrOpcode::HeapAlloc || inst.op == IrOpcode::HeapFree ||
          inst.op == IrOpcode::HeapRealloc || inst.op == IrOpcode::HeapAllocBytes) {
        runtime.hasHeap = true;
      }
      if (inst.op == IrOpcode::HeapAllocBytes) {
        runtime.hasBytes = true;
      }
    }
  }
  if (wasi && runtime.hasLocalMemory) {
    // The address check writes the VM's fault message (memory and `fd_write`) and ends the program (`proc_exit`).
    runtime.hasMemoryFault = true;
    runtime.hasTrap = true;
    runtime.enabled = true;
  }
  for (const IrFunction &function : module.functions) {
    for (const IrInstruction &inst : function.instructions) {
      if (inst.op == IrOpcode::Trap) {
        runtime.hasTrap = true;
      }
      if (wasi && inst.op == IrOpcode::LoadStringByte) {
        runtime.hasStringByteFault = true;
        runtime.hasTrap = true;
        runtime.enabled = true;
      }
      if (wasi && (inst.op == IrOpcode::DivI32 || inst.op == IrOpcode::DivI64 ||
                   inst.op == IrOpcode::DivU64)) {
        // The division helper writes the fault message (memory and `fd_write`) and ends the program (`proc_exit`).
        runtime.hasDivisionFault = true;
        runtime.hasTrap = true;
        runtime.enabled = true;
      }
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
  // The function with the result-less `proc_exit(code)` WASI import: the program ends with that exit code.
  const auto addProcExitImport = [&]() {
    if (!runtime.hasTrap || !wasi) {
      return;
    }
    WasmFunctionType procExitType;
    procExitType.params = {WasmValueTypeI32};
    runtime.hasProcExit = true;
    runtime.procExitImportIndex = static_cast<uint32_t>(imports.size());
    imports.push_back({"wasi_snapshot_preview1",
                       "proc_exit",
                       WasmFunctionKind,
                       static_cast<uint32_t>(types.size())});
    types.push_back(std::move(procExitType));
  };
  if (!runtime.enabled) {
    addProcExitImport();
    runtime.functionIndexOffset = static_cast<uint32_t>(imports.size());
    if (runtime.hasLocalMemory) {
      // The browser profile: a memory for the shadow stack alone, no WASI data.
      runtime.shadowBase = WasmShadowStackAlignment;
      if (runtime.hasHeap) {
        runtime.shadowBase =
            (layoutHeapRegions(WasmShadowStackAlignment, runtime) + WasmShadowStackAlignment - 1u) /
            WasmShadowStackAlignment * WasmShadowStackAlignment;
      }
      runtime.shadowSlots = WasmShadowStackSlots;
      runtime.memories.push_back(WasmMemoryLimit{shadowStackPages(runtime.shadowBase), false, 0});
    }
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
  if (runtime.hasDivisionFault) {
    static const std::string DivisionMessage = "VM error: division by zero in IR\n";
    runtime.divisionMessageAddr = stringBase + static_cast<uint32_t>(stringBlob.size());
    runtime.divisionMessageLength = static_cast<uint32_t>(DivisionMessage.size());
    appendDataSegmentAt(runtime.divisionMessageAddr,
                        std::vector<uint8_t>(DivisionMessage.begin(), DivisionMessage.end()),
                        runtime.dataSegments);
    stringBlob.insert(stringBlob.end(), DivisionMessage.begin(), DivisionMessage.end());
  }

  if (runtime.hasStringByteFault) {
    static const std::string StringByteMessage = "VM error: string index out of bounds in IR\n";
    runtime.stringByteMessageAddr = stringBase + static_cast<uint32_t>(stringBlob.size());
    runtime.stringByteMessageLength = static_cast<uint32_t>(StringByteMessage.size());
    appendDataSegmentAt(runtime.stringByteMessageAddr,
                        std::vector<uint8_t>(StringByteMessage.begin(), StringByteMessage.end()),
                        runtime.dataSegments);
    stringBlob.insert(stringBlob.end(), StringByteMessage.begin(), StringByteMessage.end());
  }

  if (runtime.hasMemoryFault) {
    static const std::string UnalignedMessage = "VM error: unaligned indirect address in IR: ";
    static const std::string InvalidMessage = "VM error: invalid indirect address in IR: ";
    static const std::string StackOverflowMessage = "VM error: VM call stack overflow\n";
    const auto appendMessage = [&](const std::string &message, uint32_t &addr, uint32_t &length) {
      addr = stringBase + static_cast<uint32_t>(stringBlob.size());
      length = static_cast<uint32_t>(message.size());
      appendDataSegmentAt(
          addr, std::vector<uint8_t>(message.begin(), message.end()), runtime.dataSegments);
      stringBlob.insert(stringBlob.end(), message.begin(), message.end());
    };
    if (runtime.hasHeap) {
      static const std::string FreeMessage = "VM error: invalid heap free address in IR: ";
      static const std::string ReallocMessage = "VM error: invalid heap realloc address in IR: ";
      static const std::string OverflowMessage = "VM error: VM heap allocation overflow\n";
      static const std::string ByteMessage = "VM error: invalid byte address in IR: ";
      static const std::string SlotByteMessage = "VM error: slot access to a byte region in IR: ";
      if (runtime.hasText) {
        static const std::string TextFreeMessage = "VM error: invalid string heap free address: ";
        static const std::string TextTwiceMessage = "VM error: string heap block freed twice: ";
        static const std::string TextAddressMessage = "VM error: invalid string heap address: ";
        appendMessage(TextFreeMessage, runtime.textFreeMessageAddr, runtime.textFreeMessageLength);
        appendMessage(
            TextTwiceMessage, runtime.textTwiceMessageAddr, runtime.textTwiceMessageLength);
        appendMessage(
            TextAddressMessage, runtime.textAddressMessageAddr, runtime.textAddressMessageLength);
      }
      if (runtime.hasBytes) {
        appendMessage(ByteMessage, runtime.byteMessageAddr, runtime.byteMessageLength);
        appendMessage(SlotByteMessage, runtime.slotByteMessageAddr, runtime.slotByteMessageLength);
      }
      appendMessage(FreeMessage, runtime.heapFreeMessageAddr, runtime.heapFreeMessageLength);
      appendMessage(
          ReallocMessage, runtime.heapReallocMessageAddr, runtime.heapReallocMessageLength);
      appendMessage(
          OverflowMessage, runtime.heapOverflowMessageAddr, runtime.heapOverflowMessageLength);
    }
    appendMessage(UnalignedMessage, runtime.unalignedMessageAddr, runtime.unalignedMessageLength);
    appendMessage(InvalidMessage, runtime.invalidMessageAddr, runtime.invalidMessageLength);
    appendMessage(
        StackOverflowMessage, runtime.stackOverflowMessageAddr, runtime.stackOverflowMessageLength);
  }

  if (runtime.hasFileOps) {
    const std::vector<uint8_t> &errnoTable = wasiErrnoTable();
    runtime.errnoTableAddr = stringBase + static_cast<uint32_t>(stringBlob.size());
    appendDataSegmentAt(runtime.errnoTableAddr, errnoTable, runtime.dataSegments);
    stringBlob.insert(stringBlob.end(), errnoTable.begin(), errnoTable.end());
  }

  if (runtime.hasOutputOps || runtime.hasFileWriteOps || runtime.hasDivisionFault ||
      runtime.hasMemoryFault || runtime.hasStringByteFault) {
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

  addProcExitImport();
  runtime.functionIndexOffset = static_cast<uint32_t>(imports.size());

  uint32_t memoryEnd = stringBase + static_cast<uint32_t>(stringBlob.size());
  if (runtime.hasHeap) {
    memoryEnd = layoutHeapRegions(memoryEnd, runtime);
  }
  if (runtime.hasLocalMemory) {
    runtime.shadowBase = (memoryEnd + WasmShadowStackAlignment - 1u) / WasmShadowStackAlignment *
                         WasmShadowStackAlignment;
    runtime.shadowSlots = WasmShadowStackSlots;
    memoryEnd = runtime.shadowBase + WasmShadowStackSlots * WasmSlotBytes;
  }
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
