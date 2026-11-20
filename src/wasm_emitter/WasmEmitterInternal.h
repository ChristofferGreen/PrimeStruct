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

struct WasmLocalLayout {
  uint32_t irLocalCount = 0;
  uint32_t parameterCount = 0;
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

// The code of the helper function behind `FmaF32` (`isF64` false; type (f32, f32, f32) -> f32) or `FmaF64`.
WasmCodeBody buildWasmFmaHelperBody(bool isF64);

// The code of the helper function behind an integer division on the WASI profile (type (T, T) -> T): a zero divisor
// writes the VM's message to stderr and calls `proc_exit(3)`.
WasmCodeBody buildWasmDivisionHelperBody(WasmDivisionOp op, const WasmRuntimeContext &runtime);

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
