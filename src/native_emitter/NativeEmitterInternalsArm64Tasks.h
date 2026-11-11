#pragma once

// Tasks on pthreads (TaskSpawn, TaskWait; docs/spec/vm-design.md, Tasks on threads, docs/MultithreadingPrototype.md,
// Threads For Real). A module with TaskSpawn is `threaded`: its heap routines take one spin lock (the data segment's
// allocator state has a lock word after the free-list heads), and the routines below are emitted once after the
// functions. A module without TaskSpawn emits none of this, byte for byte as before.
//
// A task block is a heap block of 8-byte words: the result, the pthread_t, the spawner's argc and argv (x19 and x20,
// which every function reads its entry arguments from), then the callee's parameters in order. The task handle is the
// block's address. TaskSpawn allocates the block, pops the parameters into it and calls the spawn routine of its
// target, which starts a pthread (8 MiB of stack) at the target's thread entry; TaskWait calls the wait routine,
// which joins the thread, reads the result and frees the block. The thread entry saves x19-x30 as the C ABI wants,
// reserves a 1 MiB value stack below its saved registers (x28 starts at its top, as in the entry frame), pushes the
// parameters on it as a caller would, calls the function with BL and stores x0 (0 for a function that returns
// nothing, which ReturnVoid sets) in the block. The callee takes its parameters from the value stack (a spawn target
// never takes register arguments), so the thread entry needs no knowledge of the allocator's register plan.
//
// The routines use x0-x17 only and keep x19-x28, like the heap routines. A fault leaves through the exit system call,
// which ends every thread.

namespace arm64_tasks {

constexpr uint64_t BlockResult = 0;
constexpr uint64_t BlockThread = 8;
constexpr uint64_t BlockArgc = 16;
constexpr uint64_t BlockArgv = 24;
constexpr uint64_t BlockArguments = 32;
constexpr uint64_t ThreadStackBytes = 8ull * 1024ull * 1024ull;
constexpr uint32_t SubSpValueStack =
    0xD14403FFu; // SUB sp, sp, #256, lsl #12 (the 1 MiB value stack)
constexpr uint32_t AddSpValueStack = 0x914403FFu; // ADD sp, sp, #256, lsl #12
constexpr uint64_t SavedRegisterBytes = 112; // x19-x30, then the block pointer, 16-byte aligned

inline uint32_t stpOffset(uint8_t rt, uint8_t rt2, uint8_t rn, uint32_t offsetBytes) {
  return 0xA9000000u | (((offsetBytes / 8) & 0x7Fu) << 15) | (static_cast<uint32_t>(rt2) << 10) |
         (static_cast<uint32_t>(rn) << 5) | rt;
}
inline uint32_t ldpOffset(uint8_t rt, uint8_t rt2, uint8_t rn, uint32_t offsetBytes) {
  return 0xA9400000u | (((offsetBytes / 8) & 0x7Fu) << 15) | (static_cast<uint32_t>(rt2) << 10) |
         (static_cast<uint32_t>(rn) << 5) | rt;
}
inline uint32_t
stpPreIndexSp(uint8_t rt, uint8_t rt2, uint32_t bytes) { // STP rt, rt2, [sp, #-bytes]!
  return 0xA9800000u | ((static_cast<uint32_t>(-static_cast<int32_t>(bytes) / 8) & 0x7Fu) << 15) |
         (static_cast<uint32_t>(rt2) << 10) | (31u << 5) | rt;
}
inline uint32_t
ldpPostIndexSp(uint8_t rt, uint8_t rt2, uint32_t bytes) { // LDP rt, rt2, [sp], #bytes
  return 0xA8C00000u | (((bytes / 8) & 0x7Fu) << 15) | (static_cast<uint32_t>(rt2) << 10) |
         (31u << 5) | rt;
}
inline uint32_t casalW(uint8_t compare, uint8_t desired, uint8_t address) {
  return 0x88E0FC00u | (static_cast<uint32_t>(compare) << 16) |
         (static_cast<uint32_t>(address) << 5) | desired;
}
inline uint32_t stlrXzr(uint8_t address) { // STLR xzr, [Xn]
  return 0xC89FFC1Fu | (static_cast<uint32_t>(address) << 5);
}
inline uint32_t movSpToReg(uint8_t rd) { // ADD Xd, sp, #0
  return 0x910003E0u | rd;
}
inline uint32_t addSpImm(uint16_t imm) { // ADD sp, sp, #imm
  return 0x910003FFu | (static_cast<uint32_t>(imm & 0xFFFu) << 10);
}
inline uint32_t subSpImm(uint16_t imm) { // SUB sp, sp, #imm
  return 0xD10003FFu | (static_cast<uint32_t>(imm & 0xFFFu) << 10);
}
inline uint32_t cbnzW(uint8_t rt, int32_t imm19) { // CBNZ Wt
  return 0x35000000u | ((static_cast<uint32_t>(imm19) & 0x7FFFFu) << 5) | rt;
}

} // namespace arm64_tasks

// One spin lock around the heap routines of a threaded module: the entry takes the lock word (CASAL 0 -> 1 until it
// succeeds), calls the routine and releases it with a store-release. The routine's arguments and result are in
// x0-x2, which the lock code leaves alone; x9-x12 are free. Returns the word index of the entry.
inline size_t Arm64Emitter::emitLockedHeapEntry(size_t inner) {
  using namespace arm64_tasks;
  const size_t entry = currentWordIndex();
  emit(0xF81F0FFEu); // str x30, [sp, #-16]!
  emitHeapLockAddress();
  const size_t spin = currentWordIndex();
  emit(encodeMovz(11, 0, 0));
  emit(encodeMovz(12, 1, 0));
  emit(casalW(11, 12, 9));
  emit(cbnzW(11, static_cast<int32_t>(spin) - static_cast<int32_t>(currentWordIndex())));
  emit(encodeBl(static_cast<int32_t>(inner) - static_cast<int32_t>(currentWordIndex())));
  emitHeapLockAddress();
  emit(stlrXzr(9));
  emit(0xF84107FEu); // ldr x30, [sp], #16
  emit(encodeRet());
  return entry;
}

// x9 = the address of the heap lock word, just after the allocator state's free-list heads.
inline void Arm64Emitter::emitHeapLockAddress() {
  emitHeapStateAddress();
  emitAddRegImm(9, 9, static_cast<uint16_t>(HeapStateBytes));
}

// A TaskSpawn: the callee's `parameterCount` parameters are on the value stack (the last on top). The task block is
// allocated, filled and handed to the spawn routine of `targetFunction`; the handle (the block address) is pushed.
inline bool
Arm64Emitter::emitTaskSpawn(uint32_t parameterCount, size_t targetFunction, std::string &error) {
  using namespace arm64_tasks;
  const uint64_t words = BlockArguments / 8 + parameterCount;
  if (words * 8 > MaxLdrStrOffsetBytes) {
    error = "native backend: a spawned function takes too many parameters";
    return false;
  }
  flushValueStackCache();
  emitMovImm64(1, (words + 1) / 2);
  emitMovImm64(2, HeapHeaderMagic);
  heapAllocCalls_.push_back(currentWordIndex());
  emit(encodeBl(0));
  for (uint32_t i = parameterCount; i-- > 0;) {
    emitPopReg(1);
    emit(encodeStrRegBase(1, 0, static_cast<uint16_t>(BlockArguments + 8ull * i)));
  }
  emit(encodeStrRegBase(19, 0, static_cast<uint16_t>(BlockArgc)));
  emit(encodeStrRegBase(20, 0, static_cast<uint16_t>(BlockArgv)));
  taskSpawnCalls_.push_back({currentWordIndex(), targetFunction, parameterCount});
  emit(encodeBl(0));
  emitPushReg(0);
  return true;
}

// A TaskWait: the handle is on the value stack; the result is pushed.
inline void Arm64Emitter::emitTaskWait() {
  flushValueStackCache();
  emitPopReg(0);
  taskWaitCalls_.push_back(currentWordIndex());
  emit(encodeBl(0));
  emitPushReg(0);
}

// The spawn and wait routines and one thread entry per spawned function, after the functions and before the heap
// routines (they add BL sites to the heap lists). `callFixups` get the thread entries' calls of their functions,
// `slotLoads` the libSystem functions the routines call (word index of the ADRP, symbol name).
inline void Arm64Emitter::emitTaskRuntime(std::vector<std::pair<size_t, size_t>> &callFixups,
                                          std::vector<std::pair<size_t, std::string>> &slotLoads) {
  using namespace arm64_tasks;
  if (taskSpawnCalls_.empty() && taskWaitCalls_.empty()) {
    return;
  }
  const auto callImport = [&](const char *symbol) {
    const size_t at = emitHostSlotLoad(16);
    slotLoads.emplace_back(at, symbol);
    emit(0xD63F0200u); // BLR x16
  };
  struct Fault {
    size_t site = 0;
    std::string_view text;
  };
  std::vector<Fault> faults;
  // Writes the message to stderr and exits with 3, like the heap faults.
  const auto emitFault = [&](std::string_view text) {
    const size_t site = currentWordIndex();
    emit(encodeAdr(1, 0));
    emit(encodeMovz(2, static_cast<uint16_t>(text.size()), 0));
    emit(encodeMovz(0, 2, 0));
    emitMovImm64(16, SysWrite);
    emit(encodeSvc());
    emit(encodeMovz(0, 3, 0));
    emitMovImm64(16, SysExit);
    emit(encodeSvc());
    faults.push_back({site, text});
  };

  // ---- spawn: x0 = the filled block, x2 = the thread entry -> x0 = the block ----
  // The frame: a pthread_attr_t (64 bytes), then the link register, the block and the entry.
  size_t spawnFailed = 0;
  const size_t spawnCommon = currentWordIndex();
  emit(subSpImm(96));
  emit(encodeStrRegBase(30, 31, 64));
  emit(encodeStrRegBase(0, 31, 72));
  emit(encodeStrRegBase(2, 31, 80));
  emit(movSpToReg(0));
  callImport("pthread_attr_init");
  emit(movSpToReg(0));
  emitMovImm64(1, ThreadStackBytes);
  callImport("pthread_attr_setstacksize");
  emit(encodeLdrRegBase(3, 31, 72));
  emitAddRegImm(0, 3, static_cast<uint16_t>(BlockThread));
  emit(movSpToReg(1));
  emit(encodeLdrRegBase(2, 31, 80));
  callImport("pthread_create");
  spawnFailed = currentWordIndex();
  emit(cbnzW(0, 0));
  emit(movSpToReg(0));
  callImport("pthread_attr_destroy");
  emit(encodeLdrRegBase(0, 31, 72));
  emit(encodeLdrRegBase(30, 31, 64));
  emit(addSpImm(96));
  emit(encodeRet());
  patchWord(spawnFailed, cbnzW(0, static_cast<int32_t>(currentWordIndex() - spawnFailed)));
  emitFault("task spawn failed: no thread could be created\n");

  // ---- one thread entry per spawned function ----
  std::vector<size_t> targets;
  std::vector<uint32_t> targetParameters;
  std::vector<size_t> spawnEntry;
  for (const TaskSpawnCall &call : taskSpawnCalls_) {
    size_t known = 0;
    while (known < targets.size() && targets[known] != call.function) {
      ++known;
    }
    if (known < targets.size()) {
      continue;
    }
    targets.push_back(call.function);
    targetParameters.push_back(call.parameterCount);
    // spawn_t: x2 = the thread entry that follows, then the common routine.
    spawnEntry.push_back(currentWordIndex());
    emit(encodeAdr(2, 8));
    emit(encodeB(static_cast<int32_t>(spawnCommon) - static_cast<int32_t>(currentWordIndex())));
    // The thread entry (x0 = the block): save the C ABI's registers, give the task its own value stack.
    emit(stpPreIndexSp(19, 20, SavedRegisterBytes));
    emit(stpOffset(21, 22, 31, 16));
    emit(stpOffset(23, 24, 31, 32));
    emit(stpOffset(25, 26, 31, 48));
    emit(stpOffset(27, 28, 31, 64));
    emit(stpOffset(29, 30, 31, 80));
    emit(encodeStrRegBase(0, 31, 96));
    emit(movSpToReg(28));
    emit(SubSpValueStack);
    emit(movSpToReg(27));
    emit(encodeLdrRegBase(19, 0, static_cast<uint16_t>(BlockArgc)));
    emit(encodeLdrRegBase(20, 0, static_cast<uint16_t>(BlockArgv)));
    for (uint32_t i = 0; i < targetParameters.back(); ++i) {
      emit(encodeLdrRegBase(1, 0, static_cast<uint16_t>(BlockArguments + 8ull * i)));
      emit(0xF81F0F81u); // str x1, [x28, #-16]!
    }
    callFixups.emplace_back(currentWordIndex(), call.function);
    emit(encodeBl(0));
    emit(AddSpValueStack);
    emit(encodeLdrRegBase(1, 31, 96));
    emit(encodeStrRegBase(0, 1, static_cast<uint16_t>(BlockResult)));
    emit(ldpOffset(21, 22, 31, 16));
    emit(ldpOffset(23, 24, 31, 32));
    emit(ldpOffset(25, 26, 31, 48));
    emit(ldpOffset(27, 28, 31, 64));
    emit(ldpOffset(29, 30, 31, 80));
    emit(ldpPostIndexSp(19, 20, SavedRegisterBytes));
    emit(encodeMovz(0, 0, 0));
    emit(encodeRet());
  }
  for (const TaskSpawnCall &call : taskSpawnCalls_) {
    size_t known = 0;
    while (targets[known] != call.function) {
      ++known;
    }
    patchWord(call.site,
              encodeBl(static_cast<int32_t>(spawnEntry[known]) - static_cast<int32_t>(call.site)));
  }

  // ---- wait: x0 = the handle -> x0 = the result ----
  if (!taskWaitCalls_.empty()) {
    const size_t waitIndex = currentWordIndex();
    emit(subSpImm(32));
    emit(encodeStrRegBase(30, 31, 0));
    emit(encodeStrRegBase(0, 31, 8));
    // The handle is the address of a task block: a mapped or pooled slot block (a freed one was waited before).
    emitSubRegImm(1, 0, 16);
    emit(encodeLdrRegBase(1, 1, 0));
    std::vector<size_t> valid;
    for (const uint64_t magic : {HeapHeaderMagic, HeapHeaderMagicPooled}) {
      emitMovImm64(2, magic);
      emitCompareReg(1, 2);
      valid.push_back(currentWordIndex());
      emit(encodeBCond(0, static_cast<uint8_t>(CondCode::Eq)));
    }
    emitMovImm64(2, HeapHeaderMagicFree);
    emitCompareReg(1, 2);
    const size_t notFree = currentWordIndex();
    emit(encodeBCond(0, static_cast<uint8_t>(CondCode::Ne)));
    emitFault("task handle already waited\n");
    patchWord(notFree,
              encodeBCond(static_cast<int32_t>(currentWordIndex() - notFree),
                          static_cast<uint8_t>(CondCode::Ne)));
    emitFault("invalid task handle in IR\n");
    for (const size_t at : valid) {
      patchWord(at,
                encodeBCond(static_cast<int32_t>(currentWordIndex() - at),
                            static_cast<uint8_t>(CondCode::Eq)));
    }
    emit(encodeLdrRegBase(0, 0, static_cast<uint16_t>(BlockThread)));
    emit(encodeMovz(1, 0, 0));
    callImport("pthread_join");
    emit(encodeLdrRegBase(1, 31, 8));
    emit(encodeLdrRegBase(2, 1, static_cast<uint16_t>(BlockResult)));
    emit(encodeStrRegBase(2, 31, 16));
    emitMovReg(0, 1);
    heapFreeCalls_.push_back(currentWordIndex());
    emit(encodeBl(0));
    emit(encodeLdrRegBase(0, 31, 16));
    emit(encodeLdrRegBase(30, 31, 0));
    emit(addSpImm(32));
    emit(encodeRet());
    for (const size_t site : taskWaitCalls_) {
      patchWord(site, encodeBl(static_cast<int32_t>(waitIndex) - static_cast<int32_t>(site)));
    }
  }

  for (const Fault &fault : faults) {
    patchWord(fault.site,
              encodeAdr(1, static_cast<int32_t>((currentWordIndex() - fault.site) * 4)));
    for (size_t i = 0; i < fault.text.size(); i += 4) {
      uint32_t word = 0;
      for (size_t b = 0; b < 4 && i + b < fault.text.size(); ++b) {
        word |= static_cast<uint32_t>(static_cast<uint8_t>(fault.text[i + b])) << (8 * b);
      }
      emit(word);
    }
  }
}
