#pragma once

// Locks and conditions on arm64 native (MutexLock, MutexUnlock, ConditionWait, ConditionNotify), with the
// semantics, the sleeping rules and the fault messages of `optexe` (OptCppMutexRuntime, OptCppConditionRuntime and
// OptCppWaitRuntime in src/backend/IrToOptCppRuntime.h). A module that has one of the opcodes is `synchronized`: its
// entry function initializes the program's wait state, and the routines below are emitted once after the functions.
//
// The lock is its heap slot: 0 while free, the holder's token otherwise (the thread pointer, TPIDRRO_EL0 without its
// low bits, which every thread has and no two share). Taking a free lock and freeing one nobody sleeps for is one
// atomic operation. A thread that finds the lock held, or waits for a condition or for a task that still runs, sleeps
// on one program-wide pthread condition variable behind one pthread mutex (the gate), both in the data segment (their
// static initializers are written by the entry function). `live` counts the threads that run program code, `blocked`
// the ones that found what they wait for unchanged since the last signal, `round` the signals. A signal (an unlock
// with sleepers, a notify with sleepers, the end of a task, a condition wait freeing its lock) clears `blocked` and
// wakes everyone, who count themselves again if they still have to wait; when the last running thread joins the
// blocked ones nothing can change any more and it faults with the VM's `deadlock: every task is waiting`.
//
// State (SyncState): the gate (a pthread_mutex_t, 64 bytes), the condition variable (a pthread_cond_t, 48 bytes), then
// the sleeper count, `live`, `blocked` and `round`. The routines use x0-x17 and keep x19-x28.

// x9 = the address of the sync state.
inline void Arm64Emitter::emitSyncAddress() {
  emitHeapStateAddress();
  emitAddRegImm(9, 9, static_cast<uint16_t>(HeapStateBytes + 16));
}

// The entry function's first instructions in a synchronized module: the gate and the condition variable as their
// static initializers write them, and the main thread as the one thread that runs.
inline void Arm64Emitter::emitSyncInit() {
  using namespace arm64_sync;
  emitSyncAddress();
  emitMovImm64(10, MutexSignature);
  emit(encodeStrRegBase(10, 9, GateOffset));
  emitMovImm64(10, CondSignature);
  emit(encodeStrRegBase(10, 9, ChangedOffset));
  emitMovImm64(10, 1);
  emit(encodeStrRegBase(10, 9, LiveOffset));
  emit(encodeStrRegBase(10, 9, RoundOffset)); // round 0 means a sleeper has not counted itself yet
}

inline void Arm64Emitter::emitMutexLock() {
  flushValueStackCache();
  emitPopReg(0);
  mutexLockCalls_.push_back(currentWordIndex());
  emit(encodeBl(0));
}

inline void Arm64Emitter::emitMutexUnlock() {
  flushValueStackCache();
  emitPopReg(0);
  mutexUnlockCalls_.push_back(currentWordIndex());
  emit(encodeBl(0));
}

// The lock on top, the condition below it.
inline void Arm64Emitter::emitConditionWait() {
  flushValueStackCache();
  emitPopReg(0);
  emitPopReg(1);
  conditionWaitCalls_.push_back(currentWordIndex());
  emit(encodeBl(0));
}

inline void Arm64Emitter::emitConditionNotify() {
  flushValueStackCache();
  emitPopReg(0);
  conditionNotifyCalls_.push_back(currentWordIndex());
  emit(encodeBl(0));
}

inline void Arm64Emitter::emitSyncRuntime(std::vector<std::pair<size_t, std::string>> &slotLoads) {
  using namespace arm64_sync;
  using namespace arm64_atomics;
  using namespace arm64_tasks;
  if (!sync_) {
    return;
  }
  const auto callImport = [&](const char *symbol) {
    const size_t at = emitHostSlotLoad(16);
    slotLoads.emplace_back(at, symbol);
    emit(0xD63F0200u); // BLR x16
  };
  const auto callRoutine = [&](size_t routine) {
    emit(encodeBl(static_cast<int32_t>(routine) - static_cast<int32_t>(currentWordIndex())));
  };
  const auto here = [&]() { return currentWordIndex(); };
  const auto placeholder = [&]() {
    const size_t at = here();
    emit(encodeBCond(0, static_cast<uint8_t>(CondCode::Eq)));
    return at;
  };
  const auto bind = [&](size_t at, CondCode cond) {
    patchWord(at, encodeBCond(static_cast<int32_t>(here() - at), static_cast<uint8_t>(cond)));
  };
  const auto placeholderCbz = [&]() {
    const size_t at = here();
    emit(encodeCbz(2, 0));
    return at;
  };
  const auto bindCbz = [&](size_t at, uint8_t reg) {
    patchWord(at, encodeCbz(reg, static_cast<int32_t>(here() - at)));
  };
  struct Fault {
    size_t site = 0;
    std::string_view text;
  };
  std::vector<Fault> faults;
  const auto emitFault = [&](std::string_view text) {
    const size_t site = here();
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
  // x1 = the caller's thread token.
  const auto emitToken = [&]() {
    emit(MrsThreadPointer | 1u);
    emit(AndClearLowBits | (1u << 5) | 1u);
  };
  const auto saveLink = [&](uint32_t frame) {
    emit(subSpImm(static_cast<uint16_t>(frame)));
    emit(encodeStrRegBase(30, 31, 0));
  };
  const auto restoreReturn = [&](uint32_t frame) {
    emit(encodeLdrRegBase(30, 31, 0));
    emit(addSpImm(static_cast<uint16_t>(frame)));
    emit(encodeRet());
  };
  // x10 = the address of the sync state field at `offset` (x9 = the state).
  const auto fieldAddress = [&](uint64_t offset) {
    emitAddRegImm(10, 9, static_cast<uint16_t>(offset));
  };

  // ---- gate lock and unlock ----
  syncGateLock_ = here();
  saveLink(16);
  emitSyncAddress();
  emitMovReg(0, 9);
  callImport("pthread_mutex_lock");
  restoreReturn(16);
  syncGateUnlock_ = here();
  saveLink(16);
  emitSyncAddress();
  emitMovReg(0, 9);
  callImport("pthread_mutex_unlock");
  restoreReturn(16);

  // ---- signal: the gate is held; every sleeper looks again ----
  syncSignal_ = here();
  saveLink(16);
  emitSyncAddress();
  emit(encodeLdrRegBase(1, 9, RoundOffset));
  emitAddRegImm(1, 1, 1);
  emit(encodeStrRegBase(1, 9, RoundOffset));
  emit(encodeStrRegBase(31, 9, BlockedOffset));
  emitAddRegImm(0, 9, static_cast<uint16_t>(ChangedOffset));
  callImport("pthread_cond_broadcast");
  restoreReturn(16);

  // ---- sleep: x0 = the address of the caller's `counted` round (0 before its first sleep); the gate is held ----
  syncSleep_ = here();
  saveLink(16);
  emitSyncAddress();
  emit(encodeStrRegBase(0, 31, 8));
  emit(encodeLdrRegBase(1, 0, 0));
  emit(encodeLdrRegBase(2, 9, RoundOffset));
  emitCompareReg(1, 2);
  const size_t sleepCounted = placeholder();
  emit(encodeStrRegBase(2, 0, 0));
  emit(encodeLdrRegBase(3, 9, BlockedOffset));
  emitAddRegImm(3, 3, 1);
  emit(encodeStrRegBase(3, 9, BlockedOffset));
  emit(encodeLdrRegBase(4, 9, LiveOffset));
  emitCompareReg(3, 4);
  const size_t sleepWait = here();
  emit(encodeBCond(0, static_cast<uint8_t>(CondCode::Ne)));
  emitFault("deadlock: every task is waiting\n");
  bind(sleepCounted, CondCode::Eq);
  bind(sleepWait, CondCode::Ne);
  emitAddRegImm(0, 9, static_cast<uint16_t>(ChangedOffset));
  emitMovReg(1, 9);
  callImport("pthread_cond_wait");
  restoreReturn(16);

  // ---- sleepers += x1 (1 or -1), at x9 = the sync state ----
  const auto addSleepers = [&](int delta) {
    emitSyncAddress();
    fieldAddress(SleepersOffset);
    emitMovImm64(1, static_cast<uint64_t>(static_cast<int64_t>(delta)));
    emit(ldaddal(1, 2, 10));
  };

  // ---- lock: x0 = the lock's address ----
  const size_t lockIndex = here();
  emitToken();
  emitMovImm64(2, 0);
  emit(casal(2, 1, 0));
  const size_t lockTaken = placeholderCbz();
  emitCompareReg(2, 1);
  const size_t lockNotHeldByCaller = placeholder();
  emitFault("mutex locked again by the task that holds it\n");
  bind(lockNotHeldByCaller, CondCode::Ne);
  // Contended: [sp] the link, [8] the lock, [16] the holder, [24] the round counted.
  saveLink(32);
  emit(encodeStrRegBase(0, 31, 8));
  emit(encodeStrRegBase(1, 31, 16));
  emit(encodeStrRegBase(31, 31, 24));
  callRoutine(syncGateLock_);
  addSleepers(1);
  const size_t lockRetry = here();
  emit(encodeLdrRegBase(0, 31, 8));
  emit(encodeLdrRegBase(1, 31, 16));
  emitMovImm64(2, 0);
  emit(casal(2, 1, 0));
  const size_t lockGot = placeholderCbz();
  emit(0x910003E0u | (24u << 10) | 0u); // ADD x0, sp, #24
  callRoutine(syncSleep_);
  emit(encodeB(static_cast<int32_t>(lockRetry) - static_cast<int32_t>(here())));
  bindCbz(lockGot, 2);
  addSleepers(-1);
  callRoutine(syncGateUnlock_);
  restoreReturn(32);
  bindCbz(lockTaken, 2);
  emit(encodeRet());

  // ---- unlock: x0 = the lock's address ----
  const size_t unlockIndex = here();
  emitToken();
  emit(ldar(2, 0));
  emitCompareReg(2, 1);
  const size_t unlockHeld = placeholder();
  emitFault("mutex unlocked by a task that does not hold it\n");
  bind(unlockHeld, CondCode::Eq);
  emit(stlr(31, 0));
  emitSyncAddress();
  fieldAddress(SleepersOffset);
  emit(ldar(2, 10));
  const size_t unlockNobody = placeholderCbz();
  saveLink(16);
  callRoutine(syncGateLock_);
  callRoutine(syncSignal_);
  callRoutine(syncGateUnlock_);
  restoreReturn(16);
  bindCbz(unlockNobody, 2);
  emit(encodeRet());

  // ---- condition wait: x0 = the lock's address (held by the caller), x1 = the condition's address ----
  const size_t waitIndex = here();
  emitMovReg(3, 1);
  emitToken();
  emit(ldar(2, 0));
  emitCompareReg(2, 1);
  const size_t waitHeld = placeholder();
  emitFault("condition wait by a task that does not hold the lock\n");
  bind(waitHeld, CondCode::Eq);
  // [sp] the link, [8] the lock, [16] the condition, [24] the notifies seen, [32] the round counted.
  saveLink(48);
  emit(encodeStrRegBase(0, 31, 8));
  emit(encodeStrRegBase(3, 31, 16));
  emit(encodeStrRegBase(31, 31, 32));
  callRoutine(syncGateLock_);
  addSleepers(1);
  emit(encodeLdrRegBase(1, 31, 16));
  emit(ldar(2, 1));
  emit(encodeStrRegBase(2, 31, 24));
  emit(encodeLdrRegBase(0, 31, 8));
  emit(stlr(31, 0));
  callRoutine(syncSignal_);
  const size_t waitLoop = here();
  emit(encodeLdrRegBase(1, 31, 16));
  emit(ldar(2, 1));
  emit(encodeLdrRegBase(3, 31, 24));
  emitCompareReg(2, 3);
  const size_t waitChanged = placeholder();
  emit(0x910003E0u | (32u << 10) | 0u); // ADD x0, sp, #32
  callRoutine(syncSleep_);
  emit(encodeB(static_cast<int32_t>(waitLoop) - static_cast<int32_t>(here())));
  bind(waitChanged, CondCode::Ne);
  addSleepers(-1);
  callRoutine(syncGateUnlock_);
  restoreReturn(48);

  // ---- condition notify: x0 = the condition's address ----
  const size_t notifyIndex = here();
  emitMovImm64(1, 1);
  emit(ldaddal(1, 2, 0));
  emitSyncAddress();
  fieldAddress(SleepersOffset);
  emit(ldar(2, 10));
  const size_t notifyNobody = placeholderCbz();
  saveLink(16);
  callRoutine(syncGateLock_);
  callRoutine(syncSignal_);
  callRoutine(syncGateUnlock_);
  restoreReturn(16);
  bindCbz(notifyNobody, 2);
  emit(encodeRet());

  for (const size_t site : mutexLockCalls_) {
    patchWord(site, encodeBl(static_cast<int32_t>(lockIndex) - static_cast<int32_t>(site)));
  }
  for (const size_t site : mutexUnlockCalls_) {
    patchWord(site, encodeBl(static_cast<int32_t>(unlockIndex) - static_cast<int32_t>(site)));
  }
  for (const size_t site : conditionWaitCalls_) {
    patchWord(site, encodeBl(static_cast<int32_t>(waitIndex) - static_cast<int32_t>(site)));
  }
  for (const size_t site : conditionNotifyCalls_) {
    patchWord(site, encodeBl(static_cast<int32_t>(notifyIndex) - static_cast<int32_t>(site)));
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

inline bool Arm64Emitter::emitConcurrencyOpcode(IrOpcode op,
                                                uint64_t imm,
                                                uint32_t spawnedParameters,
                                                std::string &error) {
  switch (op) {
  case IrOpcode::MutexLock:
    emitMutexLock();
    return true;
  case IrOpcode::MutexUnlock:
    emitMutexUnlock();
    return true;
  case IrOpcode::ConditionWait:
    emitConditionWait();
    return true;
  case IrOpcode::ConditionNotify:
    emitConditionNotify();
    return true;
  case IrOpcode::TaskWait:
    emitTaskWait();
    return true;
  case IrOpcode::TaskSpawn:
    return emitTaskSpawn(spawnedParameters, static_cast<size_t>(imm), error);
  default:
    emitAtomic(op, imm);
    return true;
  }
}
