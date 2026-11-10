#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace primec {

constexpr uint32_t IrSchemaMagic = 0x50534952u; // "PSIR"
constexpr uint32_t IrSchemaVersion = 37u;
// v34 only appends TaskSpawn and TaskWait, v35 only the Atomic* opcodes, v36 only MutexLock and MutexUnlock and v37
// only ConditionWait and ConditionNotify, so a v33 to v36 module loads unchanged (it cannot hold the opcodes added
// after it).
constexpr uint32_t IrSchemaMinimumSupportedVersion = 33u;
constexpr uint32_t IrSchemaMaximumSupportedVersion = IrSchemaVersion;

enum class IrOpcode : uint8_t {
  PushI32 = 1,
  PushI64,
  LoadLocal,
  StoreLocal,
  AddressOfLocal,
  LoadIndirect,
  StoreIndirect,
  Dup,
  Pop,
  AddI32,
  SubI32,
  MulI32,
  DivI32,
  NegI32,
  AddI64,
  SubI64,
  MulI64,
  DivI64,
  DivU64,
  NegI64,
  CmpEqI32,
  CmpNeI32,
  CmpLtI32,
  CmpLeI32,
  CmpGtI32,
  CmpGeI32,
  CmpEqI64,
  CmpNeI64,
  CmpLtI64,
  CmpLeI64,
  CmpGtI64,
  CmpGeI64,
  CmpLtU64,
  CmpLeU64,
  CmpGtU64,
  CmpGeU64,
  JumpIfZero,
  Jump,
  ReturnVoid,
  ReturnI32,
  ReturnI64,
  PrintI32,
  PrintI64,
  PrintU64,
  PrintString,
  PushArgc,
  PrintArgv,
  PrintArgvUnsafe,
  LoadStringByte,
  LoadStringLength,
  FileOpenRead,
  FileOpenWrite,
  FileOpenAppend,
  FileOpenReadDynamic,
  FileOpenWriteDynamic,
  FileOpenAppendDynamic,
  FileReadByte,
  FileClose,
  FileFlush,
  FileWriteI32,
  FileWriteI64,
  FileWriteU64,
  FileWriteString,
  FileWriteByte,
  FileWriteNewline,
  PushF32,
  PushF64,
  AddF32,
  SubF32,
  MulF32,
  DivF32,
  NegF32,
  AddF64,
  SubF64,
  MulF64,
  DivF64,
  NegF64,
  CmpEqF32,
  CmpNeF32,
  CmpLtF32,
  CmpLeF32,
  CmpGtF32,
  CmpGeF32,
  CmpEqF64,
  CmpNeF64,
  CmpLtF64,
  CmpLeF64,
  CmpGtF64,
  CmpGeF64,
  ConvertI32ToF32,
  ConvertI32ToF64,
  ConvertI64ToF32,
  ConvertI64ToF64,
  ConvertU64ToF32,
  ConvertU64ToF64,
  ConvertF32ToI32,
  ConvertF32ToI64,
  ConvertF32ToU64,
  ConvertF64ToI32,
  ConvertF64ToI64,
  ConvertF64ToU64,
  ConvertF32ToF64,
  ConvertF64ToF32,
  ReturnF32,
  ReturnF64,
  PrintStringDynamic,
  Call,
  CallVoid,
  HeapAlloc,
  HeapFree,
  HeapRealloc,
  FileWriteStringDynamic,
  // Calls host import `imm` (an index into IrModule::hostImports). Pops the
  // import's parameters, pushes its result unless it returns void. VM only.
  CallHost,
  // Pops a byte position, then a string index (module table or dynamic VM
  // string), and pushes the byte at that position. VM only.
  LoadStringByteDynamic,
  // Pops one value and pushes its low 32 bits sign-extended to 64. Lowering
  // emits it after user-level i32 add/sub/mul/div/negate so that i32 values wrap
  // at 32 bits on every backend; the I32 arithmetic opcodes themselves compute on
  // the full 64-bit slot because lowering also uses them for address math.
  SextI32,
  // Byte memory (docs/spec/vm-design.md, Byte regions). Pops a byte count and pushes the
  // address of a zeroed byte region of that size (0 for a count of 0); `address + i` names
  // byte i. HeapFree and HeapRealloc accept byte regions (a realloc counts bytes).
  HeapAllocBytes,
  // Pops a byte address and pushes the byte there, zero-extended to 64 bits.
  LoadU8,
  // Pops a value, then a byte address, stores the low 8 bits of the value there and
  // pushes the stored byte, zero-extended (like StoreIndirect, which pushes its value).
  StoreU8,
  // Pops one value and pushes its low 8 bits zero-extended: the wrap step after u8
  // arithmetic, the way SextI32 is for i32.
  ZextU8,
  // Ends the whole program with exit code imm, from any frame (docs/spec/vm-design.md, Real calls): a runtime error
  // (a failed bounds check, `panic`) inside a function the lowering calls instead of inlining, where a return would
  // only leave that function. The VM, serialized bytecode and the optimized C++ emitter run it. Added in PSIR v33.
  Trap,
  // Starts a task running function imm (docs/spec/vm-design.md, Tasks): pops the function's parameters, in order,
  // into the task's own call stack and pushes the task handle. The VM runs tasks under its scheduler and the
  // optimized C++ emitter runs each on a thread; the vm and optcpp targets and serialized bytecode accept it (every
  // other backend runs a spawned call inline). Added in PSIR v34.
  TaskSpawn,
  // Pops a task handle (the value of a TaskSpawn), waits until that task has ended, and pushes its result (0 for a
  // task whose function returns nothing). The handle is consumed: waiting it again faults. Added in PSIR v34.
  TaskWait,
  // Atomic operations on one heap slot (docs/spec/vm-design.md, Atomics). imm is the width of the integer, 32 or 64;
  // a 32-bit operation works on the low 32 bits and keeps the slot sign-extended, like every i32 value. Each one is
  // sequentially consistent. The operands are pushed address first. The VM (a switch point under a schedule seed),
  // serialized bytecode and the optimized C++ emitter run them; every other backend rejects a module that has one.
  // Added in PSIR v35.
  // Pops an address and pushes the value there.
  AtomicLoad,
  // Pops a value, then an address, and stores the value there. Pushes nothing.
  AtomicStore,
  // Pops a delta, then an address, adds the delta to the value there (wrapping) and pushes the value before.
  AtomicAdd,
  // Pops a value, then an address, stores the value there and pushes the value before.
  AtomicExchange,
  // Pops a desired value, an expected value, then an address. Stores the desired value if the slot holds the expected
  // one and pushes 1; pushes 0 and stores nothing otherwise.
  AtomicCompareExchange,
  // The lock of a `/std/sync` Mutex is one heap slot (docs/spec/vm-design.md, Mutexes): 0 while free, the holder
  // otherwise. The VM (a switch point under a schedule seed; a task that finds the lock held waits and counts for
  // deadlock detection), serialized bytecode and the optimized C++ emitter run them; every other backend rejects a
  // module that has one. Added in PSIR v36.
  // Pops an address and takes the lock there, waiting while another task holds it. Faults when the running task
  // already holds it.
  MutexLock,
  // Pops an address and frees the lock there, which the running task must hold.
  MutexUnlock,
  // A condition is one heap slot that tasks wait on while they hold a lock (docs/spec/vm-design.md, Conditions); a
  // `/std/sync` Channel has two. They run where the mutex opcodes do, and every other backend rejects a module that
  // has one. Added in PSIR v37.
  // Pops a lock address, then a condition address. Frees the lock, which the running task must hold, and waits for
  // the next ConditionNotify on the condition; the task takes the lock again with a MutexLock of its own. A waiting
  // task counts for deadlock detection.
  ConditionWait,
  // Pops a condition address and wakes every task that waits on it.
  ConditionNotify,
};

enum class IrStructFieldCategory : uint8_t {
  Default = 0,
  Pod,
  Handle,
  GpuLane,
};

enum class IrStructPaddingKind : uint8_t {
  None = 0,
  Align,
};

enum class IrStructVisibility : uint8_t {
  Private = 0,
  Public = 2,
};

enum class IrSchedulingScope : uint32_t {
  Default = 0,
};

constexpr uint64_t EffectIoOut = 1ull << 0;
constexpr uint64_t EffectIoErr = 1ull << 1;
constexpr uint64_t EffectHeapAlloc = 1ull << 2;
constexpr uint64_t EffectPathSpaceNotify = 1ull << 3;
constexpr uint64_t EffectPathSpaceInsert = 1ull << 4;
constexpr uint64_t EffectPathSpaceTake = 1ull << 5;
constexpr uint64_t EffectFileWrite = 1ull << 6;
constexpr uint64_t EffectGpuDispatch = 1ull << 7;
constexpr uint64_t EffectPathSpaceBind = 1ull << 8;
constexpr uint64_t EffectPathSpaceSchedule = 1ull << 9;
constexpr uint64_t EffectFileRead = 1ull << 10;
constexpr uint64_t EffectTask = 1ull << 11;
constexpr uint64_t EffectNetIo = 1ull << 12;
constexpr uint64_t EffectAtomic = 1ull << 13;
constexpr uint64_t EffectLock = 1ull << 14;
constexpr uint64_t EffectChannel = 1ull << 15;
constexpr uint32_t FileReadEofCode = 65536u;

struct IrStructField {
  std::string name;
  std::string envelope;
  uint32_t offsetBytes = 0;
  uint32_t sizeBytes = 0;
  uint32_t alignmentBytes = 1;
  IrStructPaddingKind paddingKind = IrStructPaddingKind::None;
  IrStructFieldCategory category = IrStructFieldCategory::Default;
  IrStructVisibility visibility = IrStructVisibility::Public;
  bool isStatic = false;
};

struct IrStructLayout {
  std::string name;
  uint32_t totalSizeBytes = 0;
  uint32_t alignmentBytes = 1;
  std::vector<IrStructField> fields;
};

struct IrExecutionMetadata {
  uint64_t effectMask = 0;
  uint64_t capabilityMask = 0;
  IrSchedulingScope schedulingScope = IrSchedulingScope::Default;
  uint32_t instrumentationFlags = 0;
};

constexpr uint32_t InstrumentationTailExecution = 1u << 0;

constexpr uint64_t PrintFlagNewline = 1ull << 0;
constexpr uint64_t PrintFlagStderr = 1ull << 1;
constexpr uint64_t PrintFlagMask = PrintFlagNewline | PrintFlagStderr;
constexpr uint64_t PrintStringIndexShift = 2;
constexpr uint64_t IrSlotBytes = 16;
constexpr int32_t IrSlotBytesI32 = static_cast<int32_t>(IrSlotBytes);

inline uint64_t encodePrintFlags(bool newline, bool stderrOut) {
  return (newline ? PrintFlagNewline : 0) | (stderrOut ? PrintFlagStderr : 0);
}

inline uint64_t encodePrintStringImm(uint64_t stringIndex, uint64_t flags) {
  return (stringIndex << PrintStringIndexShift) | (flags & PrintFlagMask);
}

inline uint64_t decodePrintFlags(uint64_t imm) {
  return imm & PrintFlagMask;
}

inline uint64_t decodePrintStringIndex(uint64_t imm) {
  return imm >> PrintStringIndexShift;
}

struct IrInstruction {
  IrOpcode op = IrOpcode::PushI32;
  uint64_t imm = 0;
  uint32_t debugId = 0;
};

struct IrLocalDebugSlot {
  uint32_t slotIndex = 0;
  std::string name;
  std::string typeName;
};

enum class IrSourceMapProvenance : uint8_t {
  Unknown = 0,
  CanonicalAst = 1,
  SyntheticIr = 2,
};

struct IrInstructionSourceMapEntry {
  uint32_t debugId = 0;
  uint32_t line = 0;
  uint32_t column = 0;
  IrSourceMapProvenance provenance = IrSourceMapProvenance::CanonicalAst;
  std::string sourceUnit{};
};

struct IrFunction {
  std::string name;
  IrExecutionMetadata metadata;
  // Number of arguments the caller leaves on the operand stack for this
  // function: they are the callee's initial stack (the callee stores them into
  // locals itself). Static-analysis passes (the shared CFG, the native emitter's
  // stack-depth checker, the wasm emitter's function-type signature) need this
  // to reason about a callee without executing it. Lowering emits real
  // Call/CallVoid targets for definitions it does not inline.
  uint32_t parameterCount = 0;
  std::vector<IrLocalDebugSlot> localDebugSlots;
  std::vector<IrInstruction> instructions;
};

// Primitive value kinds that can cross the host boundary of a CallHost.
enum class IrHostValueKind : uint8_t {
  Void = 0,
  I32,
  I64,
  U64,
  F32,
  F64,
  Bool,
  // Parameters only: the VM passes the host a pointer to the module's string.
  String,
  // Appended last so serialized kind values stay stable. Slots hold the byte
  // zero-extended.
  U8,
  // Owned text. Parameter: the VM hands the host a pointer to the NUL-terminated bytes of a string-heap
  // block (or of a module-table string, which is accepted in the same position). Result: the host returns
  // text, the VM copies it into a fresh string-heap block and pushes the block address (0 for empty text).
  Text,
  // Parameters only: a `Pointer<u8>` into a byte region, always followed by its `u64` length. Compiled C++ hands
  // the host a `uint8_t *` checked to cover the whole length (null for address 0 with length 0); the VM keeps the
  // address.
  BytePointer,
};

constexpr uint8_t IrHostValueKindMax = static_cast<uint8_t>(IrHostValueKind::BytePointer);

struct IrHostImport {
  std::string name;
  std::vector<IrHostValueKind> parameters;
  IrHostValueKind returnKind = IrHostValueKind::Void;
};

struct IrModule {
  uint32_t schemaVersion = IrSchemaVersion;
  std::vector<IrFunction> functions;
  int32_t entryIndex = -1;
  std::vector<std::string> stringTable;
  std::vector<IrStructLayout> structLayouts;
  std::vector<IrInstructionSourceMapEntry> instructionSourceMap;
  // Host functions a VM embedder must bind before running (see CallHost).
  std::vector<IrHostImport> hostImports;
};

} // namespace primec
