#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "primec/ir/Ir.h"
#include "primec/ir/IrNativeTextHost.h"

namespace primec {

inline constexpr uint8_t WasmFunctionKind = 0x00;
inline constexpr uint8_t WasmFunctionTypeTag = 0x60;
inline constexpr uint8_t WasmSectionType = 1;
inline constexpr uint8_t WasmSectionImport = 2;
inline constexpr uint8_t WasmSectionFunction = 3;
inline constexpr uint8_t WasmSectionTable = 4;
inline constexpr uint8_t WasmSectionMemory = 5;
inline constexpr uint8_t WasmSectionGlobal = 6;
inline constexpr uint8_t WasmSectionExport = 7;
inline constexpr uint8_t WasmSectionElement = 9;
inline constexpr uint8_t WasmSectionCode = 10;
inline constexpr uint8_t WasmSectionData = 11;

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

// The wasm value types, named apart from the per-file constants of the emitter units.
inline constexpr uint8_t WasmTypeI32 = 0x7f;
inline constexpr uint8_t WasmTypeI64 = 0x7e;
inline constexpr uint8_t WasmTypeF32 = 0x7d;
inline constexpr uint8_t WasmTypeF64 = 0x7c;
// A value whose type is not known yet (type inference, WasmEmitterTypes.cpp); never in the output.
inline constexpr uint8_t WasmTypeUnknown = 0;
// The 64 raw bits of an IR slot: what an indirect load yields and what a local of an unknown type holds. It is an `i64`
// in wasm; an operand that wants another type converts it (WasmEmitterMemory.cpp). Never in the output as a type byte
// (`wasmValueTypeByte`).
inline constexpr uint8_t WasmTypeSlot = 1;

// A local the emitter allocates while it emits one instruction (a value waiting while a deeper operand is converted,
// the copy of a `Dup`). Slots are reused by later instructions of the same function.
struct WasmScratchSlot {
  uint8_t type = WasmTypeUnknown;
  uint32_t index = 0;
  bool inUse = false;
};

// What the emitter knows about the wasm operand stack while it emits a function. IR is a typed stack machine whose
// locals carry no type, so the emitter follows the type of every stack value and local (WasmEmitterTypes.cpp) and
// converts an integer operand where an instruction wants the other integer width.
struct WasmTypeState {
  // Dry runs (`inferring`) widen `WasmLocalLayout::localTypes` and record what each call passes; the real run emits.
  bool inferring = false;
  bool changed = false;
  std::vector<uint8_t> stack;
  std::vector<WasmScratchSlot> scratch;
  // Per function, the type of the argument each call site passes for every parameter (inference only).
  std::vector<std::vector<uint8_t>> *observedParams = nullptr;
};

// A `block` the emitter opened around an `if`/`else` so that a jump inside it to the end of the construct (an early
// exit of one arm) is a `br`. `depth` is the number of wasm control frames open inside the block's own frame.
struct WasmJumpLabel {
  size_t irTarget = 0;
  uint32_t depth = 0;
};

struct WasmLocalLayout {
  // The wasm control frames (`block`, `loop`, `if`) open around the instruction being emitted, and the labels of the
  // enclosing structured constructs a jump may leave (WasmEmitterControlFlow.cpp).
  uint32_t controlDepth = 0;
  std::vector<WasmJumpLabel> jumpLabels;
  uint32_t irLocalCount = 0;
  uint32_t parameterCount = 0;
  // The function takes the address of a local: all its IR locals live in a frame of the shadow stack
  // (WasmEmitterMemory.cpp). `frameBaseLocal` holds the number of slots below the frame, `framePtrLocal` the memory
  // address of its first slot.
  bool addressed = false;
  uint32_t frameBaseLocal = 0;
  uint32_t framePtrLocal = 0;
  // The wasm type of every local: the parameters, the IR locals, then the emitter's helper locals.
  std::vector<uint8_t> localTypes;
  WasmTypeState typeState;
  bool hasArgvHelpers = false;
  uint32_t argvIndexLocal = 0;
  uint32_t argvCountLocal = 0;
  uint32_t argvPtrLocal = 0;
  uint32_t argvLenLocal = 0;
  uint32_t argvNextPtrLocal = 0;
  uint32_t argvBaseLocal = 0;
  bool hasFileHelpers = false;
  uint32_t fileHandleLocal = 0;
  uint32_t fileValueLocal = 0;
  uint32_t fileErrLocal = 0;
  // The i32 scratch of the errno mapping, and the i64 a file handle is unpacked from (low half the descriptor, high
  // half the errno of a failed open, as the VM packs it).
  uint32_t fileMapLocal = 0;
  uint32_t fileHandle64Local = 0;
  bool hasFileNumericHelpers = false;
  uint32_t fileDigitsPtrLocal = 0;
  uint32_t fileDigitsLenLocal = 0;
  uint32_t fileDigitsNegLocal = 0;
  uint32_t fileDigitsValueLocal = 0;
  uint32_t fileDigitsRemLocal = 0;
  uint32_t totalLocals = 0;
};

// The integer divisions that fault on a zero divisor (`DivI32`, `DivI64`, `DivU64`).
enum class WasmDivisionOp : uint8_t { I32 = 0, I64 = 1, U64 = 2 };
inline constexpr size_t WasmDivisionOpCount = 3;

// The slot heap (WasmEmitterHeap.cpp): four regions of the linear memory (values, states, counts, free runs) whose
// base addresses are mutable globals, and the helper functions appended after the shadow-stack helpers. The regions
// start with `WasmHeapInitialSlots` slots; when the slots handed out outgrow them the helper `ensureCapacity` moves
// all four to the end of the memory (`memory.grow`) with room for twice as many, up to `WasmHeapMaxSlots`. The runs
// table holds at most one free run between two live allocations.
enum class WasmHeapHelper : uint8_t {
  Fault,
  Find,
  NewGeneration,
  FillStates,
  RemoveRun,
  InsertRun,
  TakeFront,
  Release,
  EnsureCapacity,
  Alloc,
  GrowInPlace,
  Realloc,
  Free,
};
inline constexpr size_t WasmHeapHelperCount = 13;
inline constexpr uint32_t WasmHeapInitialSlots = 1u << 14;
inline constexpr uint32_t WasmHeapMaxSlots = 1u << 27;
// The bytes the four regions take for `slots` slots: 8 + 2 + 4 per slot, and a free run table of 8 bytes for each of
// the at most `slots / 2 + 1` runs.
inline constexpr uint64_t wasmHeapRegionBytes(uint64_t slots) {
  return slots * 18u + 8u;
}
// The mutable `i32` globals after the shadow stack's two: the slots handed out so far, the number of free runs and
// the number of live allocations (`__pstext_stat(8)`, the VM's `liveSlotAllocationCount`); then the base addresses
// of the values, states, counts and runs regions, the slots they have room for, the base address of the shadow
// stack (it moves when the memory has to grow past another region) and whether the shadow stack is the last region
// of the memory, which is the only place it grows in place.
inline constexpr uint32_t WasmGlobalHeapSize = 2;
inline constexpr uint32_t WasmGlobalRunCount = 3;
inline constexpr uint32_t WasmGlobalLiveBlocks = 4;
inline constexpr uint32_t WasmGlobalValuesBase = 5;
inline constexpr uint32_t WasmGlobalStatesBase = 6;
inline constexpr uint32_t WasmGlobalCountsBase = 7;
inline constexpr uint32_t WasmGlobalRunsBase = 8;
inline constexpr uint32_t WasmGlobalHeapCapacity = 9;
inline constexpr uint32_t WasmGlobalShadowBase = 10;
inline constexpr uint32_t WasmGlobalStackLast = 11;
inline constexpr uint32_t WasmGlobalCountWithHeap = 12;

// The byte regions (WasmEmitterByteHeap.cpp), present when the module has `HeapAllocBytes`: the VM's byte addresses
// over regions of the linear memory that move as the slot heap's do. The unit is the 8-byte granule: a region occupies
// `(count + 7) / 8` granules, and the free runs, the high-water mark and the capacity are counted in them. Five regions
// follow the granule count: the bytes (8 per granule), the owner of every granule (u32: the first granule of the live
// region that holds it plus one, 0 when none does), the byte count of the live region that starts at a granule (u32)
// and its generation (u16), and the free runs (8 bytes for each of the at most `granules / 2 + 1` runs).
enum class WasmByteHelper : uint8_t {
  RemoveRun,
  InsertRun,
  TakeFront,
  Ensure,
  Release,
  FindStart,
  Access,
  Alloc,
  Free,
  Realloc,
  LoadU8,
  StoreU8,
  Span,
};
inline constexpr size_t WasmByteHelperCount = 13;
inline constexpr uint32_t WasmByteInitialGranules = 1u << 11;
// The byte storage holds at most `WasmByteMaxGranules * 8` bytes (512 MiB); a region past it is the VM's overflow fault.
inline constexpr uint32_t WasmByteMaxGranules = 1u << 26;
inline constexpr uint64_t wasmByteRegionBytes(uint64_t granules) {
  return granules * 22u + 8u;
}
// The mutable `i32` globals after `WasmGlobalCountWithHeap`: the granules handed out so far, the number of free runs,
// the generation of the last region, the granules the regions have room for, and the base addresses of the five
// regions.
inline constexpr uint32_t WasmGlobalByteSize = 12;
inline constexpr uint32_t WasmGlobalByteRunCount = 13;
inline constexpr uint32_t WasmGlobalByteGeneration = 14;
inline constexpr uint32_t WasmGlobalByteCapacity = 15;
inline constexpr uint32_t WasmGlobalByteBase = 16;
inline constexpr uint32_t WasmGlobalByteOwnerBase = 17;
inline constexpr uint32_t WasmGlobalByteCountsBase = 18;
inline constexpr uint32_t WasmGlobalByteGensBase = 19;
inline constexpr uint32_t WasmGlobalByteRunsBase = 20;
inline constexpr uint32_t WasmGlobalCountWithBytes = 21;

// The string heap (WasmEmitterTextHeap.cpp), present when the module calls a `__pstext_*` allocation function: the
// VM's `VmTextHeap` over the byte regions. Its chunk records, free lists and class table live in a metadata area of the
// linear memory of their own (a byte region would take a generation and an offset the VM's blocks do not). The helper
// functions follow the byte helpers.
enum class WasmTextHelper : uint8_t {
  MetaAlloc,
  MetaFree,
  ClassIndex,
  BlockSizeFor,
  Find,
  NewRecord,
  RescanLow,
  ReturnChunk,
  ReturnFreeChunk,
  LargestFreeChunk,
  Alloc,
  TryAlloc,
  Free,
  Capacity,
  Length,
  Usable,
  Stat,
  RoomFor,
  AllocLarge,
  SetLimit,
  AllocInArena,
  ArenaBegin,
  ArenaEnd,
  MoveBytes,
  FillBytes,
  CompareBytes,
  FindByte,
  Literal,
  Argument,
};
inline constexpr size_t WasmTextHelperCount = 29;
// The mutable `i32` globals after `WasmGlobalCountWithBytes`: the next free byte and the end of the metadata area, the
// chunk record table (base address, room and records in use), the bytes of text blocks handed out, the number of
// allocations (low and high word), the bytes of chunk regions held, of those the bytes of chunks whose blocks are all
// free, the chunk regions taken, and 1 while `__pstext_try_alloc` runs (a byte region that cannot be made returns 0
// instead of the VM's overflow fault). After those come the byte limit of the heap (low and high word, 0 for none), the
// large blocks held and their bytes, then the innermost open arena (its record plus one, 0 for none), the number of open
// arenas and the bytes of the small blocks they handed out, and the count (low and high word) of the byte range
// operation that faults.
inline constexpr uint32_t WasmGlobalTextMetaBump = 21;
inline constexpr uint32_t WasmGlobalTextMetaEnd = 22;
inline constexpr uint32_t WasmGlobalTextRecords = 23;
inline constexpr uint32_t WasmGlobalTextRecordCap = 24;
inline constexpr uint32_t WasmGlobalTextRecordHigh = 25;
inline constexpr uint32_t WasmGlobalTextInUse = 26;
inline constexpr uint32_t WasmGlobalTextAllocLow = 27;
inline constexpr uint32_t WasmGlobalTextAllocHigh = 28;
inline constexpr uint32_t WasmGlobalTextChunkBytes = 29;
inline constexpr uint32_t WasmGlobalTextFreeChunkBytes = 30;
inline constexpr uint32_t WasmGlobalTextChunkRegions = 31;
inline constexpr uint32_t WasmGlobalTextTry = 32;
inline constexpr uint32_t WasmGlobalTextLimitLow = 33;
inline constexpr uint32_t WasmGlobalTextLimitHigh = 34;
inline constexpr uint32_t WasmGlobalTextLargeBlocks = 35;
inline constexpr uint32_t WasmGlobalTextLargeBytes = 36;
inline constexpr uint32_t WasmGlobalTextArena = 37;
inline constexpr uint32_t WasmGlobalTextArenaDepth = 38;
inline constexpr uint32_t WasmGlobalTextArenaBytes = 39;
inline constexpr uint32_t WasmGlobalTextRangeLow = 40;
inline constexpr uint32_t WasmGlobalTextRangeHigh = 41;
inline constexpr uint32_t WasmGlobalCountWithText = 42;
// The metadata area the module starts with (the records start with room for 64 of 64 bytes; both are powers of two so
// a table that grew can return its old block to the metadata allocator), the class table (45 size classes of 16 bytes)
// and the free list heads of the metadata allocator (one per power of two from 16 bytes).
inline constexpr uint32_t WasmTextMetaBytes = 32768;
inline constexpr uint32_t WasmTextInitialRecords = 64;
inline constexpr uint32_t WasmTextRecordBytes = 64;
inline constexpr uint32_t WasmTextClassCount = 45;
inline constexpr uint32_t WasmTextMetaBuckets = 28;

struct WasmRuntimeContext {
  bool enabled = false;
  bool hasArgvOps = false;
  bool hasOutputOps = false;
  bool hasFileOps = false;
  bool hasFileReadOps = false;
  bool hasFileWriteOps = false;
  // The module has a `Trap` (a runtime error ends the program). On the WASI profile it calls `proc_exit(code)` first;
  // the browser profile has no host function to call, so the trap is the `unreachable` instruction alone.
  bool hasTrap = false;
  bool hasProcExit = false;
  uint32_t procExitImportIndex = 0;
  uint32_t functionIndexOffset = 0;
  // The helper functions appended after the IR functions that compute a fused multiply-add with one rounding
  // (docs/spec/vm-design.md, Fused multiply-add); present only when the module has the opcode.
  bool hasFma32 = false;
  bool hasFma64 = false;
  uint32_t fma32Function = 0;
  uint32_t fma64Function = 0;
  // WASI profile only: an integer division goes through a helper function of the module that checks the divisor, so a
  // zero divisor is the VM's runtime fault (`VM error: division by zero in IR` on stderr, `proc_exit(3)`) and not a
  // wasm trap. `hasDivisionFault` is set when the module divides; the helpers exist per used operation.
  bool hasDivisionFault = false;
  // WASI profile only: a string heap statistic the VM does not know (a number above 8) is the VM's fault
  // (`VM error: unknown string heap statistic: <n>` on stderr, `proc_exit(3)`), written by a helper function of the
  // module (`statFault`, type (i64) -> ()) that `__pstext_stat` calls; the browser profile traps.
  bool hasStatFault = false;
  uint32_t statFaultFunction = 0;
  uint32_t statMessageAddr = 0;
  uint32_t statMessageLength = 0;
  bool hasDivisionHelper[WasmDivisionOpCount] = {false, false, false};
  uint32_t divisionHelperFunction[WasmDivisionOpCount] = {0, 0, 0};
  uint32_t divisionMessageAddr = 0;
  uint32_t divisionMessageLength = 0;
  // WASI profile only: `LoadStringByte` checks the position against the length of the string and ends the program with
  // the VM's message (`string index out of bounds in IR`) and `proc_exit(3)` when it is not below it.
  bool hasStringByteFault = false;
  // WASI profile only: `PrintArgv` with an index past the arguments ends the program with the VM's
  // `invalid argv index in IR` and `proc_exit(3)` (`PrintArgvUnsafe` keeps its unchecked read).
  bool hasArgvFault = false;
  uint32_t argvMessageAddr = 0;
  uint32_t argvMessageLength = 0;
  // WASI profile only: `LoadStringLength` reads the length of the string at a computed index from `stringLengthTable`
  // (one `u32` per string of the module table); an index past the table is the VM's `invalid string index in IR`.
  bool hasStringLength = false;
  uint32_t stringLengthTableAddr = 0;
  uint32_t stringIndexMessageAddr = 0;
  uint32_t stringIndexMessageLength = 0;
  uint32_t stringByteMessageAddr = 0;
  uint32_t stringByteMessageLength = 0;
  // The module takes a local address or reads or writes through one: a mutable `i32` global (index 0) holds the number
  // of live slots of the shadow stack, which starts with `shadowSlots` 8-byte slots of the memory from `shadowBase`
  // and is the last region of the memory; a second mutable `i32` global (index 1) holds the slots the memory has now,
  // and the helper `growShadowStack` grows the memory when a frame needs more. The helper function `slotAddress`
  // checks an address and maps it to the memory (WasmEmitterMemory.cpp).
  bool hasLocalMemory = false;
  bool hasMemoryFault = false;
  // The module has `HeapAlloc`, `HeapFree` or `HeapRealloc`: the heap regions (values, states, counts, free runs), the
  // two extra globals, the helper function indexes and the VM's fault messages.
  bool hasHeap = false;
  // Per host import of the module: it is `__pstext_stat`, the one host function a Wasm module answers itself.
  std::vector<bool> statHostImports;
  // The initial base addresses of the heap regions (the globals hold the current ones).
  uint32_t heapValues = 0;
  uint32_t heapStates = 0;
  uint32_t heapCounts = 0;
  uint32_t heapRuns = 0;
  uint32_t heapFunction[WasmHeapHelperCount] = {};
  // The module has `HeapAllocBytes` (and so a heap): the byte regions, their helpers and the fault message of a byte
  // address that names no live byte.
  bool hasBytes = false;
  uint32_t byteBase = 0;
  uint32_t byteOwner = 0;
  uint32_t byteCounts = 0;
  uint32_t byteGens = 0;
  uint32_t byteRuns = 0;
  uint32_t byteFunction[WasmByteHelperCount] = {};
  // The module calls a string heap allocation function (`__pstext_alloc` and its relatives): the string heap helpers
  // over the byte regions, their messages and the static tables (the globals hold the dynamic ones).
  bool hasText = false;
  uint32_t textFunction[WasmTextHelperCount] = {};
  uint32_t textClasses = 0;
  uint32_t textRecords = 0;
  uint32_t textMeta = 0;
  uint32_t textMetaHeads = 0;
  uint32_t textFreeMessageAddr = 0;
  uint32_t textFreeMessageLength = 0;
  uint32_t textTwiceMessageAddr = 0;
  uint32_t textTwiceMessageLength = 0;
  uint32_t textAddressMessageAddr = 0;
  uint32_t textAddressMessageLength = 0;
  uint32_t textLimitMessageAddr = 0;
  uint32_t textLimitMessageLength = 0;
  uint32_t textArenaMessageAddr = 0;
  uint32_t textArenaMessageLength = 0;
  uint32_t textIndexMessageAddr = 0;
  uint32_t textIndexMessageLength = 0;
  uint32_t textRangeMessageAddr = 0;
  uint32_t textRangeMessageLength = 0;
  uint32_t textRangeMidMessageAddr = 0;
  uint32_t textRangeMidMessageLength = 0;
  // The address and string count of the table that holds the address of every string literal the module asked
  // `__pstext_literal` for (a `u64` per string index, 0 until asked for), and the table of the (address, length) pair of
  // every literal in the data segment.
  uint32_t textLiterals = 0;
  uint32_t textLiteralInfo = 0;
  uint32_t textLiteralCount = 0;
  // Per host import of the module: the string heap function it names and what it returns.
  std::vector<NativeTextHost> textHostKinds;
  std::vector<IrHostValueKind> textHostReturns;
  uint32_t byteMessageAddr = 0;
  uint32_t byteMessageLength = 0;
  uint32_t slotByteMessageAddr = 0;
  uint32_t slotByteMessageLength = 0;
  uint32_t heapFreeMessageAddr = 0;
  uint32_t heapFreeMessageLength = 0;
  uint32_t heapReallocMessageAddr = 0;
  uint32_t heapReallocMessageLength = 0;
  uint32_t heapOverflowMessageAddr = 0;
  uint32_t heapOverflowMessageLength = 0;
  uint32_t shadowBase = 0;
  uint32_t shadowSlots = 0;
  uint32_t slotAddressFunction = 0;
  // The module has atomic opcodes and byte regions: `atomicCell` (type (i64) -> i32) names the 8 bytes of a cell that is
  // a slot or the aligned bytes of a byte region; without byte regions an atomic cell is a slot (`slotAddress`).
  bool hasAtomics = false;
  uint32_t atomicCellFunction = 0;
  // The module has lock or condition opcodes: `syncFault` (type (i32) -> ()) writes message `which` and ends the program
  // with exit code 3 (WasmEmitterSync.cpp).
  bool hasSync = false;
  uint32_t syncFaultFunction = 0;
  uint32_t syncMessageAddr[4] = {0, 0, 0, 0};
  uint32_t syncMessageLength[4] = {0, 0, 0, 0};
  uint32_t growShadowStackFunction = 0;
  uint32_t stackOverflowMessageAddr = 0;
  uint32_t stackOverflowMessageLength = 0;
  uint32_t unalignedMessageAddr = 0;
  uint32_t unalignedMessageLength = 0;
  uint32_t invalidMessageAddr = 0;
  uint32_t invalidMessageLength = 0;
  uint32_t fdWriteImportIndex = 0;
  uint32_t fdReadImportIndex = 0;
  uint32_t argsSizesGetImportIndex = 0;
  uint32_t argsGetImportIndex = 0;
  uint32_t fdCloseImportIndex = 0;
  uint32_t fdSyncImportIndex = 0;
  uint32_t pathOpenImportIndex = 0;
  uint32_t argcAddr = 0;
  uint32_t argvBufSizeAddr = 0;
  uint32_t nwrittenAddr = 0;
  uint32_t openedFdAddr = 0;
  // The table that maps a WASI errno to the POSIX number the VM reports (wasiErrnoTable).
  uint32_t errnoTableAddr = 0;
  uint32_t iovAddr = 0;
  // Two words: the base and the byte capacity of the pages `PrintArgv` took for the argument table and text (0 until
  // the first call), kept so a program that prints arguments in a loop takes pages once.
  uint32_t argvRegionAddr = 0;
  uint32_t newlineAddr = 0;
  uint32_t byteScratchAddr = 0;
  uint32_t numericScratchAddr = 0;
  uint32_t numericScratchBytes = 0;
  // Calls through function addresses (WasmEmitterIndirect.cpp): per IR function, whether the module takes its address,
  // and per parameter count the index of the type `(i64 x count) -> i64` that `call_indirect` and the thunks of the
  // table share (`UINT32_MAX` for a count no call and no thunk uses).
  std::vector<bool> addressTaken;
  std::vector<uint32_t> indirectTypeIndex;
  std::vector<uint32_t> stringPtrs;
  std::vector<uint32_t> stringLens;
  std::vector<WasmMemoryLimit> memories;
  std::vector<WasmDataSegment> dataSegments;
};

inline void appendU32Leb(uint32_t value, std::vector<uint8_t> &out) {
  do {
    uint8_t byte = static_cast<uint8_t>(value & 0x7f);
    value >>= 7;
    if (value != 0) {
      byte |= 0x80;
    }
    out.push_back(byte);
  } while (value != 0);
}

// The memory argument of an i32 load or store: alignment 2 (4 bytes) and the offset.
inline void appendI32MemArg(uint32_t offset, std::vector<uint8_t> &out) {
  appendU32Leb(2, out);
  appendU32Leb(offset, out);
}

// The memory argument of a byte load or store: alignment 1 (2^0) and the offset.
inline void appendI8MemArg(uint32_t offset, std::vector<uint8_t> &out) {
  appendU32Leb(0, out);
  appendU32Leb(offset, out);
}

inline void appendS32Leb(int32_t value, std::vector<uint8_t> &out) {
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

inline void appendS64Leb(int64_t value, std::vector<uint8_t> &out) {
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

inline void appendFixedU32Le(uint32_t value, std::vector<uint8_t> &out) {
  out.push_back(static_cast<uint8_t>(value & 0xff));
  out.push_back(static_cast<uint8_t>((value >> 8) & 0xff));
  out.push_back(static_cast<uint8_t>((value >> 16) & 0xff));
  out.push_back(static_cast<uint8_t>((value >> 24) & 0xff));
}

inline void appendFixedU64Le(uint64_t value, std::vector<uint8_t> &out) {
  for (size_t byte = 0; byte < 8; ++byte) {
    out.push_back(static_cast<uint8_t>((value >> (byte * 8)) & 0xff));
  }
}

bool appendSection(uint8_t sectionId,
                   const std::vector<uint8_t> &payload,
                   std::vector<uint8_t> &out,
                   std::string &error);

bool encodeTypeSection(const std::vector<WasmFunctionType> &types,
                       std::vector<uint8_t> &payload,
                       std::string &error);

bool encodeImportSection(const std::vector<WasmImport> &imports,
                         std::vector<uint8_t> &payload,
                         std::string &error);

bool encodeFunctionSection(const std::vector<uint32_t> &functionTypeIndexes,
                           std::vector<uint8_t> &payload,
                           std::string &error);

bool encodeMemorySection(const std::vector<WasmMemoryLimit> &memories,
                         std::vector<uint8_t> &payload,
                         std::string &error);

// Mutable `i32` globals: the number of live slots of the shadow stack (initially 0) and the number of slots its
// memory holds (initially `capacitySlots`); with a heap, the globals from `WasmGlobalHeapSize` on, with the initial
// values in `heapGlobalInitials` in the order of their indexes.
bool encodeShadowStackGlobalSection(uint32_t capacitySlots,
                                    const std::vector<uint32_t> &heapGlobalInitials,
                                    std::vector<uint8_t> &payload,
                                    std::string &error);

bool encodeExportSection(const std::vector<WasmExport> &exports,
                         std::vector<uint8_t> &payload,
                         std::string &error);

bool encodeCodeSection(const std::vector<WasmCodeBody> &bodies,
                       std::vector<uint8_t> &payload,
                       std::string &error);

bool encodeDataSection(const std::vector<WasmDataSegment> &segments,
                       std::vector<uint8_t> &payload,
                       std::string &error);

// `wasi` is false for the browser profile, which imports nothing from WASI (a `Trap` becomes `unreachable` alone).
bool buildWasiRuntimeContext(const IrModule &module,
                             bool wasi,
                             std::vector<WasmImport> &imports,
                             std::vector<WasmFunctionType> &types,
                             WasmRuntimeContext &runtime,
                             std::string &error);

// The table of POSIX errno numbers indexed by WASI errno (an unmapped errno maps to itself), and the code that maps the
// i32 errno on the stack through it so a program reads the same status on Wasm as on the VM.
const std::vector<uint8_t> &wasiErrnoTable();
void emitMapWasiErrno(uint32_t scratchLocal,
                      const WasmRuntimeContext &runtime,
                      std::vector<uint8_t> &out);

void emitWasiWritePtrLen(uint32_t fd,
                         uint32_t ptrLocal,
                         uint32_t lenLocal,
                         const WasmRuntimeContext &runtime,
                         std::vector<uint8_t> &out,
                         bool dropResult = true);

void emitWasiWriteLiteral(uint32_t fd,
                          uint32_t ptr,
                          uint32_t len,
                          const WasmRuntimeContext &runtime,
                          std::vector<uint8_t> &out,
                          bool dropResult = true);

void emitWasiWriteLiteralFromFdLocal(uint32_t fdLocal,
                                     uint32_t ptr,
                                     uint32_t len,
                                     const WasmRuntimeContext &runtime,
                                     std::vector<uint8_t> &out);

void emitWasiWritePtrLenFromFdLocal(uint32_t fdLocal,
                                    uint32_t ptrLocal,
                                    uint32_t lenLocal,
                                    const WasmRuntimeContext &runtime,
                                    std::vector<uint8_t> &out);

void emitWasiReadByteFromFdLocal(uint32_t fdLocal,
                                 const WasmRuntimeContext &runtime,
                                 std::vector<uint8_t> &out);

void emitWasiWriteDecimalFromI64Local(uint32_t fdLocal,
                                      uint32_t valueLocal,
                                      uint32_t remLocal,
                                      uint32_t ptrLocal,
                                      uint32_t lenLocal,
                                      uint32_t negLocal,
                                      bool signedValue,
                                      const WasmRuntimeContext &runtime,
                                      std::vector<uint8_t> &out);

void emitWasiWriteDynamicStringFromI64Local(uint32_t fdLocal,
                                            uint32_t stringIndexLocal,
                                            const WasmRuntimeContext &runtime,
                                            std::vector<uint8_t> &out);

void emitWasiPathOpenDynamicFromI64Local(uint32_t stringIndexLocal,
                                         uint32_t errorLocal,
                                         uint32_t oflags,
                                         uint64_t rightsBase,
                                         uint32_t fdflags,
                                         const WasmRuntimeContext &runtime,
                                         std::vector<uint8_t> &out);

void emitWasiPathOpen(uint32_t pathPtr,
                      uint32_t pathLen,
                      uint32_t oflags,
                      uint64_t rightsBase,
                      uint32_t fdflags,
                      const WasmRuntimeContext &runtime,
                      std::vector<uint8_t> &out);

bool emitIoInstruction(const IrInstruction &inst,
                       const WasmLocalLayout &localLayout,
                       const WasmRuntimeContext &runtime,
                       std::vector<uint8_t> &out,
                       std::string &error,
                       const std::string &functionName);

bool emitSimpleInstruction(const IrInstruction &inst,
                           const WasmLocalLayout &localLayout,
                           const std::vector<WasmFunctionType> &functionTypes,
                           const WasmRuntimeContext &runtime,
                           std::vector<uint8_t> &out,
                           std::string &error,
                           const std::string &functionName);

// `outDiverges`, if non-null, is set to whether every path through
// [start, end) provably ends in an explicit Return* - i.e. wasm code
// immediately following this range is unreachable. Conservative: a loop
// is never reported as diverging, and an `if` with no `else` never
// diverges (the implicit empty else always reaches the end normally). See
// TODO-4748 for why callers need this: wasm's own validator does not
// infer that code after a void-typed if/else is unreachable just because
// neither arm falls through normally, so callers that know both arms
// diverge must emit an explicit `unreachable` opcode themselves.
bool emitInstructionRange(const IrFunction &function,
                          size_t start,
                          size_t end,
                          WasmLocalLayout &localLayout,
                          const std::vector<WasmFunctionType> &functionTypes,
                          const WasmRuntimeContext &runtime,
                          std::vector<uint8_t> &out,
                          std::string &error,
                          bool *outDiverges = nullptr);

// One operand of a short-circuit chain: the IR range [start, end) computes the condition, and the `JumpIfZero` at
// `end` leaves the chain for the else-part when it is false.
struct WasmShortCircuitOperand {
  size_t start = 0;
  size_t end = 0;
};

// A condition `first || (a && b && ...)` as the IR lowers it: `JumpIfZero -> second`, `Jump -> thenStart`, then the
// operands of the chain, each ending in a `JumpIfZero -> elseStart`, then the then-part at `thenStart`.
struct WasmShortCircuitCondition {
  size_t thenStart = 0;
  size_t elseStart = 0;
  std::vector<WasmShortCircuitOperand> operands;
};

// Whether the `JumpIfZero` at `conditionIndex` (whose range ends at `end`) is the first operand of a short-circuit
// `||` (WasmEmitterShortCircuit.cpp).
bool findWasmShortCircuitCondition(const IrFunction &function,
                                   size_t conditionIndex,
                                   size_t end,
                                   WasmShortCircuitCondition &condition);

// Emits the condition as one `i32` value (the first operand is on the type stack; the value replaces it).
bool emitWasmShortCircuitCondition(const IrFunction &function,
                                   const WasmShortCircuitCondition &condition,
                                   WasmLocalLayout &localLayout,
                                   const std::vector<WasmFunctionType> &functionTypes,
                                   const WasmRuntimeContext &runtime,
                                   std::vector<uint8_t> &out,
                                   std::string &error);

// The code of the helper function behind `FmaF32` (`isF64` false; type (f32, f32, f32) -> f32) or `FmaF64`.
WasmCodeBody buildWasmFmaHelperBody(bool isF64);

// Calls through function addresses (WasmEmitterIndirect.cpp, docs/spec/vm-design.md, Function addresses). The address
// of function `f` is `f + 1`, which is also the table index plus one: `call_indirect` of the address minus one traps
// on a null address, an address past the table, an element no function fills (a function whose address is never taken)
// and a thunk of another parameter count. Every function whose address is taken has a thunk of type
// `(i64 x parameterCount) -> i64` (a void result is 0) that converts its slots to the function's parameter types.
std::vector<bool> wasmAddressTakenFunctions(const IrModule &module);
// The distinct parameter counts of the module's indirect calls and of the functions whose address it takes, ascending.
std::vector<uint32_t> wasmIndirectParameterCounts(const IrModule &module, const std::vector<bool> &addressTaken);
WasmFunctionType wasmIndirectSignature(uint32_t parameterCount);
WasmCodeBody buildWasmIndirectThunkBody(const WasmFunctionType &callee, uint32_t calleeFunction);
// The table of `entries` function references and its element segments (table index, wasm function index).
bool encodeTableSection(uint32_t entries, std::vector<uint8_t> &payload, std::string &error);
bool encodeElementSection(const std::vector<std::pair<uint32_t, uint32_t>> &elements,
                          std::vector<uint8_t> &payload,
                          std::string &error);

// The code of the helper function behind an integer division on the WASI profile (type (T, T) -> T): a zero divisor
// writes the VM's message to stderr and calls `proc_exit(3)`.
WasmCodeBody buildWasmDivisionHelperBody(WasmDivisionOp op, const WasmRuntimeContext &runtime);

// Writes `messageLength` bytes at `messageAddr` to stderr and ends the program with `proc_exit(3)`: a runtime error of
// the WASI profile. The code never falls through.
void emitWasmRuntimeFault(const WasmRuntimeContext &runtime,
                          uint32_t messageAddr,
                          uint32_t messageLength,
                          std::vector<uint8_t> &out);

// `LoadStringByte`: the position (an `i64` on the stack) becomes the byte of the string at `pointer` of `length` bytes,
// as an `i32`; a position that is not below the length is the VM's fault. `positionLocal` is an `i64` scratch local.
void emitWasmStringByteLoad(uint32_t positionLocal,
                            uint32_t pointer,
                            uint32_t length,
                            const WasmRuntimeContext &runtime,
                            std::vector<uint8_t> &out);

// `LoadStringLength`: the string index (an `i64` on the stack) becomes the length of that string as an `i64`; an index
// past the table of `count` strings is the VM's fault. `indexLocal` is an `i64` scratch local.
void emitWasmStringLengthLoad(uint32_t indexLocal,
                              uint32_t count,
                              const WasmRuntimeContext &runtime,
                              std::vector<uint8_t> &out);

// Whether the leading `StoreLocal`s of the function (its parameter prologue) store the parameters anywhere but to the
// locals 0..parameterCount-1, which are the wasm parameters (a struct passed by value keeps its slots in the locals
// after the ones its address goes to).
bool wasmFunctionNeedsParameterShift(const IrFunction &function);

// Rewrites such a function so that its prologue is the canonical one: the body's locals move up by `parameterCount`
// and the prologue copies each wasm parameter to the local the body reads it from. The order of the body's slots,
// which an address into its frame can walk, is kept.
bool shiftWasmParameterLocals(IrFunction &function, std::string &error);

// `irLocalTypes` are the inferred types of the function's IR locals (inferWasmLocalTypes).
bool lowerFunctionCode(const IrFunction &function,
                       const std::vector<WasmFunctionType> &functionTypes,
                       const WasmRuntimeContext &runtime,
                       const std::vector<uint8_t> &irLocalTypes,
                       WasmCodeBody &outBody,
                       std::string &error);

// Infers the wasm type of every IR local and parameter of the module by dry runs of the emitter to a fixed point,
// and sets `functionTypes[i].params` to the parameter types. `irLocalTypes[i]` holds function i's IR local types.
bool inferWasmLocalTypes(const IrModule &module,
                         std::vector<WasmFunctionType> &functionTypes,
                         const WasmRuntimeContext &runtime,
                         std::vector<std::vector<uint8_t>> &irLocalTypes,
                         std::string &error);

// Typed emission (WasmEmitterTypes.cpp). `emitTypedInstruction` converts the operands of `inst` to the types it wants,
// emits it, and updates the tracked stack.
bool emitTypedInstruction(const IrInstruction &inst,
                          WasmLocalLayout &localLayout,
                          const std::vector<WasmFunctionType> &functionTypes,
                          const WasmRuntimeContext &runtime,
                          std::vector<uint8_t> &out,
                          std::string &error,
                          const std::string &functionName);

// Turns the value on top of the stack into the i32 an `if` or `br_if` takes and pops it from the tracked stack.
bool consumeWasmCondition(WasmLocalLayout &localLayout,
                          std::vector<uint8_t> &out,
                          std::string &error,
                          const std::string &functionName);

// Local addresses and indirect memory access (WasmEmitterMemory.cpp).
bool wasmFunctionTakesLocalAddress(const IrFunction &function);
bool wasmModuleUsesLocalMemory(const IrModule &module);
// True when the module calls a string heap allocation function (`__pstext_alloc`, `_try_alloc`, `_free`, `_capacity`,
// `_length`, `_usable`, `_set_limit`, `_arena_begin`, `_arena_end`, the byte operations, `_literal` or `_argument`): it then has the byte regions and the string heap helpers.
bool wasmModuleCallsTextHeap(const IrModule &module);
// The type byte of the output for a type of the emitter (`WasmTypeSlot` is an `i64`, an unknown type an `i32`).
uint8_t wasmValueTypeByte(uint8_t type);
void emitWasmSlotConversion(uint8_t from, uint8_t to, std::vector<uint8_t> &out);
void emitWasmFramePrologue(const WasmLocalLayout &layout,
                           const WasmRuntimeContext &runtime,
                           std::vector<uint8_t> &out);
void emitWasmFrameEpilogue(const WasmLocalLayout &layout, std::vector<uint8_t> &out);
// Recomputes `framePtrLocal` from the frame base and the current base of the shadow stack. A call can move the shadow
// stack (a callee's frame that does not fit relocates it), so a function that keeps a frame emits this after each
// call; a module without a heap never moves it and needs none.
void emitWasmFramePointerReload(const WasmLocalLayout &layout,
                                const WasmRuntimeContext &runtime,
                                std::vector<uint8_t> &out);
void emitWasmFrameLoad(uint64_t slot, const WasmLocalLayout &layout, std::vector<uint8_t> &out);
void emitWasmFrameStore(uint64_t slot,
                        uint32_t valueLocal,
                        const WasmLocalLayout &layout,
                        std::vector<uint8_t> &out);
void emitWasmAddressOfLocal(uint64_t slot,
                            const WasmLocalLayout &layout,
                            std::vector<uint8_t> &out);
void emitWasmIndirectLoad(const WasmRuntimeContext &runtime, std::vector<uint8_t> &out);

// `__pstext_stat(which)`: takes the i64 on the stack (through the i64 scratch local) and leaves the statistic as an i64.
void emitWasmStringHeapStat(uint32_t whichLocal,
                            const WasmRuntimeContext &runtime,
                            std::vector<uint8_t> &out);
void emitWasmIndirectStore(uint32_t valueLocal,
                           const WasmRuntimeContext &runtime,
                           std::vector<uint8_t> &out);
// The code of the helper `slotAddress` (type (i64) -> i32).
WasmCodeBody buildWasmSlotAddressHelperBody(const WasmRuntimeContext &runtime);
// The code of the helper `syncFault` (type (i32) -> ()).
WasmCodeBody buildWasmSyncFaultHelperBody(const WasmRuntimeContext &runtime);
// The messages of `syncFault`, indexed by its argument.
const std::string &wasmSyncMessage(size_t which);
// The lock and condition opcodes on one thread: `first` is the i32 local that holds the memory address of the lock (a
// wait checks that its caller holds it; the condition cell is only checked to be a valid address).
void emitWasmSyncOperation(IrOpcode op,
                           std::vector<uint8_t> &out,
                           const WasmRuntimeContext &runtime,
                           uint32_t first);
// The code of the helper `atomicCell` (type (i64) -> i32): the memory address of the cell an atomic opcode names.
WasmCodeBody buildWasmAtomicCellHelperBody(const WasmRuntimeContext &runtime);
// The code of the helper `statFault` (type (i64) -> ()): the VM's unknown-statistic message with the number it was
// asked for, then `proc_exit(3)`.
WasmCodeBody buildWasmStatFaultHelperBody(const WasmRuntimeContext &runtime);
// The code of the helper `growShadowStack` (type (i32) -> ()): the argument is the number of live slots a frame needs;
// the memory grows to hold them, or the program ends with the VM's `VM call stack overflow` fault.
WasmCodeBody buildWasmShadowGrowHelperBody(const WasmRuntimeContext &runtime);
// The largest frame, in slots, the emitter accepts (a frame holds at most half of the 4 GiB a wasm memory can have).
inline constexpr uint32_t WasmMaxFrameSlots = 1u << 28;

// A structured block (`if`, `loop`) cannot reach the values below it: its code starts with an empty tracked stack and
// the outer stack comes back at the end.
// Converts the value on top of the tracked stack to `type` (an integer of the other width), emitting into `out`.
bool coerceWasmTopValue(WasmLocalLayout &localLayout,
                        uint8_t type,
                        std::vector<uint8_t> &out,
                        std::string &error,
                        const std::string &functionName);
std::vector<uint8_t> enterWasmBlock(WasmLocalLayout &localLayout);
void leaveWasmBlock(WasmLocalLayout &localLayout, std::vector<uint8_t> &&outerStack);

// The widest of two integer types, or false when they are different kinds (an integer and a float, f32 and f64).
bool mergeWasmTypes(uint8_t current, uint8_t incoming, uint8_t &merged);

} // namespace primec
