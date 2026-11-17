#include "VmThreadedSync.h"

#include "VmKernelLocalize.h"

#include <atomic>

namespace primec::vm_detail {

namespace {

using Cell = std::atomic_ref<uint64_t>;

bool resolveSlot(VmKernelHost &host,
                 std::vector<VmKernelFrame> &frames,
                 uint64_t address,
                 bool atomic,
                 uint64_t *&slot,
                 std::string &error) {
  std::vector<uint64_t> *addressLocals = nullptr;
  localizeVmAddress(frames, host.slotBytes(), address, addressLocals);
  return atomic ? host.resolveAtomicAddress(address, *addressLocals, slot, error)
                : host.resolveIndirectAddress(address, *addressLocals, slot, error);
}

// Sequentially consistent atomics on the heap slot (docs/spec/vm-design.md, Atomics). A 32-bit value is kept
// sign-extended in its 64-bit slot, like every i32, so the read-modify-write forms are compare-exchange loops.
SyncOutcome stepAtomic(const IrInstruction &inst,
                       VmKernelHost &host,
                       std::vector<uint64_t> &stack,
                       std::vector<VmKernelFrame> &frames,
                       SyncLogRecorder *recorder,
                       uint64_t holder,
                       std::string &error) {
  const size_t operandCount = inst.op == IrOpcode::AtomicLoad              ? 1u
                              : inst.op == IrOpcode::AtomicCompareExchange ? 3u
                                                                           : 2u;
  if (stack.size() < operandCount) {
    error = "IR stack underflow on atomic operation";
    return SyncOutcome::Fault;
  }
  if (inst.imm != 32u && inst.imm != 64u) {
    error = "invalid atomic width in IR";
    return SyncOutcome::Fault;
  }
  const bool narrow = inst.imm == 32u;
  const auto canonical = [narrow](uint64_t value) {
    return narrow ? static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(value)))
                  : value;
  };
  const uint64_t second = operandCount >= 2 ? canonical(stack[stack.size() - 1]) : 0;
  const uint64_t first = operandCount == 3 ? canonical(stack[stack.size() - 2]) : second;
  const uint64_t address = stack[stack.size() - operandCount];
  stack.resize(stack.size() - operandCount);
  uint64_t *slot = nullptr;
  if (!resolveSlot(host, frames, address, true, slot, error)) {
    return SyncOutcome::Fault;
  }
  // A recorded run holds the record's ordering point across the operation, so the log orders atomic operations as
  // they ran (docs/spec/vm-design.md, Synchronization record and replay).
  std::unique_lock<std::mutex> order;
  if (recorder != nullptr) {
    order = std::unique_lock<std::mutex>(recorder->orderMutex());
  }
  Cell cell(*slot);
  switch (inst.op) {
  case IrOpcode::AtomicLoad:
    stack.push_back(canonical(cell.load()));
    break;
  case IrOpcode::AtomicStore:
    cell.store(second);
    break;
  case IrOpcode::AtomicAdd: {
    uint64_t before = cell.load();
    while (!cell.compare_exchange_weak(before, canonical(canonical(before) + second))) {
    }
    stack.push_back(canonical(before));
    break;
  }
  case IrOpcode::AtomicExchange:
    stack.push_back(canonical(cell.exchange(second)));
    break;
  default: {
    // first is the expected value and second the desired one; the slot's canonical form is compared.
    uint64_t before = cell.load();
    bool swapped = false;
    while (canonical(before) == first) {
      if (cell.compare_exchange_weak(before, second)) {
        swapped = true;
        break;
      }
    }
    stack.push_back(swapped ? 1 : 0);
    break;
  }
  }
  if (recorder != nullptr) {
    recorder->noteLocked(holder, syncEventKindOf(inst.op));
  }
  return SyncOutcome::Done;
}

// Decrements the slow-path count however the lock function leaves.
struct SleeperGuard {
  explicit SleeperGuard(std::atomic<size_t> &count) : count_(count) {
    count_.fetch_add(1);
  }
  ~SleeperGuard() {
    count_.fetch_sub(1);
  }
  SleeperGuard(const SleeperGuard &) = delete;
  SleeperGuard &operator=(const SleeperGuard &) = delete;

private:
  std::atomic<size_t> &count_;
};

SyncOutcome deadlockOrStop(WaitResult result, std::string &error) {
  if (result == WaitResult::Deadlock) {
    error = DeadlockMessage;
    return SyncOutcome::Fault;
  }
  return SyncOutcome::Stopped;
}

// The slot holds 0 while free and the holder's task number otherwise. A free lock is taken with one compare-exchange;
// a held one parks the thread on the run's condition variable until an unlock signals.
SyncOutcome
lockMutex(ThreadedWaitState &state, uint64_t *slot, uint64_t holder, std::string &error) {
  Cell cell(*slot);
  // The event is noted once the lock is taken, so the log has it after the unlock it waited for.
  const auto noteLock = [&] {
    if (state.recorder != nullptr) {
      state.recorder->note(holder, SyncEventKind::Lock);
    }
  };
  uint64_t expected = 0;
  if (cell.compare_exchange_strong(expected, holder)) {
    noteLock();
    return SyncOutcome::Done;
  }
  if (expected == holder) {
    error = "mutex locked again by the task that holds it";
    return SyncOutcome::Fault;
  }
  // Announced before the lock is tried again, so an unlock either sees the sleeper or is seen by the retry.
  SleeperGuard sleeper(state.sleepers);
  std::unique_lock<std::mutex> lock(state.mutex);
  const WaitResult result = waitForChange(state, lock, [&] {
    uint64_t free = 0;
    if (!cell.compare_exchange_strong(free, holder)) {
      return false;
    }
    noteLock();
    return true;
  });
  return result == WaitResult::Ready ? SyncOutcome::Done : deadlockOrStop(result, error);
}

SyncOutcome
unlockMutex(ThreadedWaitState &state, uint64_t *slot, uint64_t holder, std::string &error) {
  Cell cell(*slot);
  if (cell.load() != holder) {
    error = "mutex unlocked by a task that does not hold it";
    return SyncOutcome::Fault;
  }
  // The event is noted before the lock is released, so the log has it before the next holder's lock event.
  if (state.recorder != nullptr) {
    state.recorder->note(holder, SyncEventKind::Unlock);
  }
  cell.store(0);
  if (state.sleepers.load() > 0) {
    std::lock_guard<std::mutex> lock(state.mutex);
    state.signalChange();
  }
  return SyncOutcome::Done;
}

// A condition is the address of a slot that is never read or written (the VM does not use its value); the run counts the
// notifies of the conditions some task waits on. A wait registers itself and frees the lock with the mutex held, then
// sleeps until the count changed, so a notify between the two is not lost; the task's next instruction takes the lock
// again.
SyncOutcome waitCondition(ThreadedWaitState &state,
                          uint64_t *lockSlot,
                          uint64_t *conditionSlot,
                          uint64_t holder,
                          std::string &error) {
  Cell lockCell(*lockSlot);
  if (lockCell.load() != holder) {
    error = "condition wait by a task that does not hold the lock";
    return SyncOutcome::Fault;
  }
  std::unique_lock<std::mutex> lock(state.mutex);
  ThreadedWaitState::Condition &condition = state.conditions[conditionSlot];
  ++condition.waiters;
  const uint64_t seen = condition.notifies;
  if (state.recorder != nullptr) {
    state.recorder->note(holder, SyncEventKind::ConditionWait);
  }
  lockCell.store(0);
  state.signalChange();
  const WaitResult result = waitForChange(state, lock, [&] { return condition.notifies != seen; });
  if (--condition.waiters == 0) {
    state.conditions.erase(conditionSlot);
  }
  return result == WaitResult::Ready ? SyncOutcome::Done : deadlockOrStop(result, error);
}

void notifyCondition(ThreadedWaitState &state, uint64_t *conditionSlot, uint64_t holder) {
  std::lock_guard<std::mutex> lock(state.mutex);
  if (state.recorder != nullptr) {
    state.recorder->note(holder, SyncEventKind::ConditionNotify);
  }
  const auto found = state.conditions.find(conditionSlot);
  if (found == state.conditions.end()) {
    return;
  }
  ++found->second.notifies;
  state.signalChange();
}

} // namespace

bool isVmSyncOpcode(IrOpcode op) {
  switch (op) {
  case IrOpcode::AtomicLoad:
  case IrOpcode::AtomicStore:
  case IrOpcode::AtomicAdd:
  case IrOpcode::AtomicExchange:
  case IrOpcode::AtomicCompareExchange:
  case IrOpcode::MutexLock:
  case IrOpcode::MutexUnlock:
  case IrOpcode::ConditionWait:
  case IrOpcode::ConditionNotify:
    return true;
  default:
    return false;
  }
}

SyncOutcome stepThreadedSyncOpcode(const IrInstruction &inst,
                                   ThreadedWaitState &state,
                                   VmKernelHost &host,
                                   std::vector<uint64_t> &stack,
                                   std::vector<VmKernelFrame> &frames,
                                   uint64_t holder,
                                   std::string &error) {
  switch (inst.op) {
  case IrOpcode::MutexLock:
  case IrOpcode::MutexUnlock: {
    if (stack.empty()) {
      error = "IR stack underflow on mutex operation";
      return SyncOutcome::Fault;
    }
    uint64_t *slot = nullptr;
    if (!resolveSlot(host, frames, stack.back(), false, slot, error)) {
      return SyncOutcome::Fault;
    }
    stack.pop_back();
    return inst.op == IrOpcode::MutexLock ? lockMutex(state, slot, holder, error)
                                          : unlockMutex(state, slot, holder, error);
  }
  case IrOpcode::ConditionWait:
  case IrOpcode::ConditionNotify: {
    const bool wait = inst.op == IrOpcode::ConditionWait;
    const size_t operandCount = wait ? 2 : 1;
    if (stack.size() < operandCount) {
      error = "IR stack underflow on condition operation";
      return SyncOutcome::Fault;
    }
    uint64_t *conditionSlot = nullptr;
    if (!resolveSlot(
            host, frames, stack[stack.size() - operandCount], false, conditionSlot, error)) {
      return SyncOutcome::Fault;
    }
    uint64_t *lockSlot = nullptr;
    if (wait && !resolveSlot(host, frames, stack.back(), false, lockSlot, error)) {
      return SyncOutcome::Fault;
    }
    stack.resize(stack.size() - operandCount);
    if (!wait) {
      notifyCondition(state, conditionSlot, holder);
      return SyncOutcome::Done;
    }
    return waitCondition(state, lockSlot, conditionSlot, holder, error);
  }
  default:
    return stepAtomic(inst, host, stack, frames, state.recorder, holder, error);
  }
}

} // namespace primec::vm_detail
