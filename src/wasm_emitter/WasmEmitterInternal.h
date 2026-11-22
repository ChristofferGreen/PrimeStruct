#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "primec/ir/Ir.h"

namespace primec {

inline constexpr uint8_t WasmFunctionKind = 0x00;
inline constexpr uint8_t WasmFunctionTypeTag = 0x60;
inline constexpr uint8_t WasmSectionType = 1;
inline constexpr uint8_t WasmSectionImport = 2;
inline constexpr uint8_t WasmSectionFunction = 3;
inline constexpr uint8_t WasmSectionMemory = 5;
inline constexpr uint8_t WasmSectionGlobal = 6;
inline constexpr uint8_t WasmSectionExport = 7;
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
  uint32_t totalLocals = 0;
};

// The integer divisions that fault on a zero divisor (`DivI32`, `DivI64`, `DivU64`).
enum class WasmDivisionOp : uint8_t { I32 = 0, I64 = 1, U64 = 2 };
inline constexpr size_t WasmDivisionOpCount = 3;

// The slot heap (WasmEmitterHeap.cpp): a fixed region of the linear memory below the shadow stack, and the helper
// functions appended after the shadow-stack helpers. `WasmHeapSlots` is the capacity in VM slots; the runs table
// holds at most one free run between two live allocations.
enum class WasmHeapHelper : uint8_t {
  Fault,
  Find,
  NewGeneration,
  FillStates,
  RemoveRun,
  InsertRun,
  TakeFront,
  Release,
  Alloc,
  GrowInPlace,
  Realloc,
  Free,
};
inline constexpr size_t WasmHeapHelperCount = 12;
inline constexpr uint32_t WasmHeapSlots = 1u << 21;
inline constexpr uint32_t WasmHeapRunCapacity = WasmHeapSlots / 2u + 1u;
// The mutable `i32` globals after the shadow stack's two: the slots handed out so far and the number of free runs.
inline constexpr uint32_t WasmGlobalHeapSize = 2;
inline constexpr uint32_t WasmGlobalRunCount = 3;

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
  bool hasDivisionHelper[WasmDivisionOpCount] = {false, false, false};
  uint32_t divisionHelperFunction[WasmDivisionOpCount] = {0, 0, 0};
  uint32_t divisionMessageAddr = 0;
  uint32_t divisionMessageLength = 0;
  // WASI profile only: `LoadStringByte` checks the position against the length of the string and ends the program with
  // the VM's message (`string index out of bounds in IR`) and `proc_exit(3)` when it is not below it.
  bool hasStringByteFault = false;
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
  uint32_t heapValues = 0;
  uint32_t heapStates = 0;
  uint32_t heapCounts = 0;
  uint32_t heapRuns = 0;
  uint32_t heapFunction[WasmHeapHelperCount] = {};
  uint32_t heapFreeMessageAddr = 0;
  uint32_t heapFreeMessageLength = 0;
  uint32_t heapReallocMessageAddr = 0;
  uint32_t heapReallocMessageLength = 0;
  uint32_t heapOverflowMessageAddr = 0;
  uint32_t heapOverflowMessageLength = 0;
  uint32_t shadowBase = 0;
  uint32_t shadowSlots = 0;
  uint32_t slotAddressFunction = 0;
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

// Two mutable `i32` globals: the number of live slots of the shadow stack (initially 0) and the number of slots its
// memory holds (initially `capacitySlots`).
bool encodeShadowStackGlobalSection(uint32_t capacitySlots,
                                    bool hasHeap,
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
// The type byte of the output for a type of the emitter (`WasmTypeSlot` is an `i64`, an unknown type an `i32`).
uint8_t wasmValueTypeByte(uint8_t type);
void emitWasmSlotConversion(uint8_t from, uint8_t to, std::vector<uint8_t> &out);
void emitWasmFramePrologue(const WasmLocalLayout &layout,
                           const WasmRuntimeContext &runtime,
                           std::vector<uint8_t> &out);
void emitWasmFrameEpilogue(const WasmLocalLayout &layout, std::vector<uint8_t> &out);
void emitWasmFrameLoad(uint64_t slot, const WasmLocalLayout &layout, std::vector<uint8_t> &out);
void emitWasmFrameStore(uint64_t slot,
                        uint32_t valueLocal,
                        const WasmLocalLayout &layout,
                        std::vector<uint8_t> &out);
void emitWasmAddressOfLocal(uint64_t slot,
                            const WasmLocalLayout &layout,
                            std::vector<uint8_t> &out);
void emitWasmIndirectLoad(const WasmRuntimeContext &runtime, std::vector<uint8_t> &out);
void emitWasmIndirectStore(uint32_t valueLocal,
                           const WasmRuntimeContext &runtime,
                           std::vector<uint8_t> &out);
// The code of the helper `slotAddress` (type (i64) -> i32).
WasmCodeBody buildWasmSlotAddressHelperBody(const WasmRuntimeContext &runtime);
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
