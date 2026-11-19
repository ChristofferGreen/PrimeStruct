#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "VmFastKernelProgram.h"
#include "VmSyncLog.h"
#include "primec/ir/Ir.h"
#include "primec/runtime/VmExecutionKernel.h"

namespace primec::vm_detail {

// The state every thread of a `--vm-threads` run shares (docs/spec/vm-design.md, Tasks on threads): the one mutex and
// condition variable that task waits, lock waits and condition waits sleep on, the stop flag, and the counts the
// deadlock check uses. Everything but `stop` and `sleepers` is guarded by `mutex`.
struct ThreadedWaitState {
  std::mutex mutex;
  std::condition_variable changed;
  std::atomic<bool> stop{false};
  // Threads inside a lock's slow path; an unlock that finds none skips the mutex.
  std::atomic<size_t> sleepers{0};
  // Spawned tasks whose function has not returned; the main task is one more live thread.
  size_t spawned = 0;
  // Threads that found nothing to do since the last change, and the number of changes.
  size_t waiting = 0;
  uint64_t epoch = 0;
  // The conditions some task waits on, by the address of their slot: the notifies so far and the tasks waiting. The
  // slot itself is not used (a notify on a condition nobody waits on does nothing). Guarded by `mutex`.
  struct Condition {
    uint64_t notifies = 0;
    size_t waiters = 0;
  };
  std::unordered_map<const uint64_t *, Condition> conditions;
  // The run's sync-order record (docs/spec/vm-design.md, Synchronization record and replay), or null.
  SyncLogRecorder *recorder = nullptr;

  // Something a waiter may wait for happened (a lock freed, a notify, a task ended): every waiter looks again and
  // counts itself waiting again only if it still has to wait. Call with `mutex` held.
  void signalChange() {
    waiting = 0;
    ++epoch;
    changed.notify_all();
  }
};

enum class WaitResult { Ready, Stopped, Deadlock };

// Sleeps until `ready()` holds, with `lock` (on state.mutex) held between checks. `Deadlock` means every live thread
// (the spawned tasks and the main task) has looked since the last change and had to wait, so nothing can change any
// more; `Stopped` means the run is ending.
template <typename Ready>
WaitResult
waitForChange(ThreadedWaitState &state, std::unique_lock<std::mutex> &lock, Ready ready) {
  while (!ready()) {
    if (state.stop.load(std::memory_order_relaxed)) {
      return WaitResult::Stopped;
    }
    ++state.waiting;
    if (state.waiting >= state.spawned + 1) {
      return WaitResult::Deadlock;
    }
    const uint64_t seen = state.epoch;
    state.changed.wait(
        lock, [&] { return state.epoch != seen || state.stop.load(std::memory_order_relaxed); });
  }
  return WaitResult::Ready;
}

inline constexpr const char *DeadlockMessage = "deadlock: every task is waiting";

bool isVmSyncOpcode(IrOpcode op);

enum class SyncOutcome { Done, Stopped, Fault };

// The slot an address names on the call stack of the task that runs a sync opcode: a local of one of its frames or
// a heap slot. `atomic` asks for the check atomic operations need.
class SyncAddressResolver {
public:
  virtual ~SyncAddressResolver() = default;
  virtual bool resolve(uint64_t address, bool atomic, uint64_t *&slot, std::string &error) = 0;
};

// A task on the step kernel: its frames hold the locals.
class FrameAddressResolver final : public SyncAddressResolver {
public:
  FrameAddressResolver(VmKernelHost &host, std::vector<VmKernelFrame> &frames)
      : host_(host), frames_(frames) {
  }
  bool resolve(uint64_t address, bool atomic, uint64_t *&slot, std::string &error) override;

private:
  VmKernelHost &host_;
  std::vector<VmKernelFrame> &frames_;
};

// A task on the flat loop: its locals are one arena, numbered across its frames.
class FastTaskAddressResolver final : public SyncAddressResolver {
public:
  FastTaskAddressResolver(VmKernelHost &host, VmFastTask &task) : host_(host), task_(task) {
  }
  bool resolve(uint64_t address, bool atomic, uint64_t *&slot, std::string &error) override;

private:
  VmKernelHost &host_;
  VmFastTask &task_;
};

// Runs one atomic, mutex or condition opcode of the task that owns `stack` and whose addresses `resolver` resolves.
// `holder` is the task's non-zero number, stored in a lock slot it holds. Done: the opcode ran and the caller advances. Stopped: the run is
// ending. Fault: `error` is set (a deadlock is one).
SyncOutcome stepThreadedSyncOpcode(const IrInstruction &inst,
                                   ThreadedWaitState &state,
                                   SyncAddressResolver &resolver,
                                   std::vector<uint64_t> &stack,
                                   uint64_t holder,
                                   std::string &error);

} // namespace primec::vm_detail
