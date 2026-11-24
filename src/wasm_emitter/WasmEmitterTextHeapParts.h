#pragma once

#include "WasmEmitterAsm.h"
#include "WasmEmitterHeapRegions.h"
#include "WasmEmitterInternal.h"

namespace primec {
namespace wasm_text {

// The pieces of the string heap (WasmEmitterTextHeap.cpp describes it): the layout constants and the small emitters
// both components use, the metadata component (the metadata allocator, size classes, chunk records and the return of
// chunks: WasmEmitterTextMeta.cpp) and the block component (allocation, free, capacity, length, usable and the
// statistics: WasmEmitterTextHeap.cpp).

using namespace wasm_heap;

inline constexpr int64_t OffsetMask47 = (int64_t{1} << 47) - 1;
inline constexpr int32_t ChunkLimit = 256 * 1024;
inline constexpr int32_t FirstChunk = 4096;
inline constexpr int32_t FreeChunkLimit = 4 * ChunkLimit;
inline constexpr int32_t LargeThreshold = 64 * 1024;
inline constexpr int32_t FreeFlag = static_cast<int32_t>(0x80000000u);

inline constexpr uint32_t RecBase = 0;
inline constexpr uint32_t RecBytes = 8;
inline constexpr uint32_t RecBlockBytes = 12;
inline constexpr uint32_t RecBlockCount = 16;
inline constexpr uint32_t RecFreeCount = 20;
inline constexpr uint32_t RecHead = 24;
inline constexpr uint32_t RecState = 28;
inline constexpr uint32_t RecKind = 32;
inline constexpr uint32_t ClassHeld = 0;
inline constexpr uint32_t ClassFree = 4;
inline constexpr uint32_t ClassLow = 8;

// Fault kinds of the slot heap's `fault` helper (WasmEmitterHeap.cpp).
inline constexpr int32_t FaultOverflow = 2;
inline constexpr int32_t FaultTextFree = 4;
inline constexpr int32_t FaultTextTwice = 5;
inline constexpr int32_t FaultTextAddress = 6;

inline uint32_t textFn(const WasmRuntimeContext &runtime, WasmTextHelper helper) {
  return runtime.textFunction[static_cast<size_t>(helper)];
}

inline uint32_t byteFn(const WasmRuntimeContext &runtime, WasmByteHelper helper) {
  return runtime.byteFunction[static_cast<size_t>(helper)];
}

inline void
faultWith(WasmAsm &a, const WasmRuntimeContext &runtime, int32_t kind, uint32_t addressLocal) {
  a.i32Const(kind);
  a.localGet(addressLocal);
  a.call(fn(runtime, WasmHeapHelper::Fault));
}

inline void faultOverflow(WasmAsm &a, const WasmRuntimeContext &runtime) {
  a.i32Const(FaultOverflow);
  a.i64Const(0);
  a.call(fn(runtime, WasmHeapHelper::Fault));
}

// Pushes the memory address of chunk record `indexLocal`.
inline void recordAddress(WasmAsm &a, uint32_t indexLocal) {
  a.localGet(indexLocal);
  a.i32Const(6);
  a.i32Shl();
  a.globalGet(WasmGlobalTextRecords);
  a.i32Add();
}

// Pushes the memory address of the class entry of the block size in `blockBytesLocal`.
inline void classAddress(WasmAsm &a, const WasmRuntimeContext &runtime, uint32_t blockBytesLocal) {
  a.localGet(blockBytesLocal);
  a.call(textFn(runtime, WasmTextHelper::ClassIndex));
  a.i32Const(4);
  a.i32Shl();
  a.i32Const(static_cast<int32_t>(runtime.textClasses));
  a.i32Add();
}

// Pushes the byte offset (i32) of the chunk whose record address is in `recordLocal`.
inline void recordOffset(WasmAsm &a, uint32_t recordLocal) {
  a.localGet(recordLocal);
  a.i64Load(RecBase);
  a.i64Const(OffsetMask47);
  a.i64And();
  a.i32WrapI64();
}

// global = global +/- the i32 local.
inline void globalAddLocal(WasmAsm &a, uint32_t global, uint32_t local, bool subtract) {
  a.globalGet(global);
  a.localGet(local);
  subtract ? a.i32Sub() : a.i32Add();
  a.globalSet(global);
}

// `memory[addressLocal + offset] = memory[...] +/- valueLocal` on an i32 field.
inline void fieldAddLocal(
    WasmAsm &a, uint32_t addressLocal, uint32_t offset, uint32_t valueLocal, bool subtract) {
  a.localGet(addressLocal);
  a.localGet(addressLocal);
  a.i32Load(offset);
  a.localGet(valueLocal);
  subtract ? a.i32Sub() : a.i32Add();
  a.i32Store(offset);
}

// rounded = the smallest of 16, 32, 64, ... that is at least `size`; bucket = its number.
inline void
emitBucket(WasmAsm &a, uint32_t sizeLocal, uint32_t roundedLocal, uint32_t bucketLocal) {
  a.i32Const(16);
  a.localSet(roundedLocal);
  a.i32Const(0);
  a.localSet(bucketLocal);
  a.block();
  a.loop();
  a.localGet(roundedLocal);
  a.localGet(sizeLocal);
  a.i32GeU();
  a.brIf(1);
  a.localGet(roundedLocal);
  a.i32Const(1);
  a.i32Shl();
  a.localSet(roundedLocal);
  a.i32Increment(bucketLocal);
  a.br(0);
  a.end();
  a.end();
}

// Pushes the memory address of the free list head of a bucket.
inline void bucketHead(WasmAsm &a, const WasmRuntimeContext &runtime, uint32_t bucketLocal) {
  a.localGet(bucketLocal);
  a.i32Const(2);
  a.i32Shl();
  a.i32Const(static_cast<int32_t>(runtime.textMetaHeads));
  a.i32Add();
}

// The metadata component.
WasmCodeBody buildMetaAlloc(const WasmRuntimeContext &runtime);
WasmCodeBody buildMetaFree(const WasmRuntimeContext &runtime);
WasmCodeBody buildClassIndex(const WasmRuntimeContext &runtime);
WasmCodeBody buildBlockSizeFor(const WasmRuntimeContext &runtime);
WasmCodeBody buildFind(const WasmRuntimeContext &runtime);
WasmCodeBody buildNewRecord(const WasmRuntimeContext &runtime);
WasmCodeBody buildRescanLow(const WasmRuntimeContext &runtime);
WasmCodeBody buildReturnChunk(const WasmRuntimeContext &runtime);
WasmCodeBody buildReturnFreeChunk(const WasmRuntimeContext &runtime);
WasmCodeBody buildLargestFreeChunk(const WasmRuntimeContext &runtime);

// The block component.
WasmCodeBody buildAlloc(const WasmRuntimeContext &runtime);
WasmCodeBody buildTryAlloc(const WasmRuntimeContext &runtime);
WasmCodeBody buildFree(const WasmRuntimeContext &runtime);
WasmCodeBody buildCapacity(const WasmRuntimeContext &runtime);
WasmCodeBody buildLength(const WasmRuntimeContext &runtime);
WasmCodeBody buildUsable(const WasmRuntimeContext &runtime);
WasmCodeBody buildStat(const WasmRuntimeContext &runtime);

} // namespace wasm_text
} // namespace primec
