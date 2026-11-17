#include "VmThreadedTasks.h"

#include "VmTaskHandle.h"
#include "VmTaskScheduler.h"
#include "VmThreadedSync.h"

#include <pthread.h>

#include <atomic>
#include <condition_variable>
#include <cstring>
#include <exception>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

namespace primec::vm_detail {

namespace {

// The main thread's stack size, as `optexe` gives each task thread (a secondary thread's default is 512 KiB on
// macOS). The step kernel keeps its call frames on the heap, so this is room for host functions.
constexpr size_t TaskStackBytes = size_t{8} << 20;
constexpr size_t MaxLiveTasks = size_t{1} << 20;
// A task looks at the stop flag every this many steps (a power of two).
constexpr uint32_t StopPollSteps = 256;
// The task number of the main task; spawned tasks are numbered from 2. A lock slot holds its holder's number.
constexpr uint64_t MainHolder = 1;

class VmThreadedTasks;

// One spawned task: its thread, its heap and, once its function returned, its result. `done` and `result` are
// written and read with the run's mutex held.
struct ThreadTask {
  VmThreadedTasks *owner = nullptr;
  size_t functionIndex = 0;
  // The task number a lock slot holds while this task holds the lock (the main task is 1).
  uint64_t holder = 0;
  std::vector<uint64_t> arguments;
  void *heap = nullptr;
  uint64_t result = 0;
  bool done = false;
};

// The tasks of one run (docs/spec/vm-design.md, Tasks on threads). Every spawned task is a detached thread running
// the step kernel on a call stack of its own; the calling thread is the main task. The task table (slots with
// generations) and every flag below are behind one mutex, and one condition variable wakes whoever waits for a task
// to end or for the run to stop. The run is over (`ended_`) when the main task returns, a task traps or a task
// faults, whichever comes first; the stop flag then ends every other task at its next poll.
class VmThreadedTasks {
public:
  VmThreadedTasks(const IrModule &module,
                  VmKernelHost &host,
                  std::vector<size_t> localCounts,
                  SyncLogRecorder *recorder)
      : module_(module), host_(host), localCounts_(std::move(localCounts)) {
    sync_.recorder = recorder;
  }

  bool run(uint64_t &result, std::string &error) {
    uint64_t value = 0;
    const TaskEnd end = runGuarded(static_cast<size_t>(module_.entryIndex), MainHolder, {}, value);
    if (end != TaskEnd::Stopped) {
      finishWith(value);
    }
    {
      // Whatever ended the run, every thread stops and is gone before the run returns, so none reads a heap or a
      // host that no longer exists.
      std::unique_lock<std::mutex> lock(sync_.mutex);
      if (!ended_) {
        ended_ = true;
        failed_ = true;
        error_ = "execution stopped";
      }
      sync_.stop.store(true, std::memory_order_relaxed);
      sync_.changed.notify_all();
      sync_.changed.wait(lock, [this] { return sync_.spawned == 0; });
    }
    if (failed_) {
      error = error_;
      return false;
    }
    result = result_;
    return true;
  }

private:
  enum class TaskEnd { Returned, Trapped, Stopped };
  struct Slot {
    std::unique_ptr<ThreadTask> task;
    uint16_t generation = 0;
  };

  // The program ends with `value` (the main task's return or a trap); the first end wins.
  void finishWith(uint64_t value) {
    std::lock_guard<std::mutex> lock(sync_.mutex);
    if (!ended_) {
      ended_ = true;
      result_ = value;
    }
    sync_.stop.store(true, std::memory_order_relaxed);
    sync_.signalChange();
  }

  // The program ends with a fault; only the first one is reported.
  void fail(std::string message) {
    std::lock_guard<std::mutex> lock(sync_.mutex);
    if (!ended_) {
      ended_ = true;
      failed_ = true;
      error_ = std::move(message);
    }
    sync_.stop.store(true, std::memory_order_relaxed);
    sync_.signalChange();
  }

  TaskEnd runGuarded(size_t functionIndex,
                     uint64_t holder,
                     std::vector<uint64_t> arguments,
                     uint64_t &value) {
    try {
      return runTask(functionIndex, holder, std::move(arguments), value);
    } catch (const std::exception &exception) {
      fail(std::string("exception: ") + exception.what());
    } catch (...) {
      fail("unknown exception");
    }
    return TaskEnd::Stopped;
  }

  // The step kernel on one call stack. Faults are reported through fail(); the answer is Stopped for them and for a
  // stop that another task started.
  TaskEnd
  runTask(size_t functionIndex, uint64_t holder, std::vector<uint64_t> arguments, uint64_t &value) {
    // The arguments are the task's operand stack, where the function's first instructions store its parameters.
    std::vector<uint64_t> stack = std::move(arguments);
    std::vector<VmKernelFrame> frames;
    frames.reserve(64);
    VmKernelFrame entry;
    entry.functionIndex = functionIndex;
    entry.function = &module_.functions[functionIndex];
    entry.locals.assign(localCounts_[functionIndex], 0);
    // Each call stack numbers its own locals from 0, so a local address does not name a slot of another task.
    entry.localsBase = 0;
    frames.push_back(std::move(entry));

    VmKernelStepEvent event;
    const std::atomic<bool> *cancel = host_.cancelFlag();
    std::string error;
    uint32_t steps = 0;
    while (true) {
      if ((++steps & (StopPollSteps - 1)) == 0) {
        if (sync_.stop.load(std::memory_order_relaxed)) {
          return TaskEnd::Stopped;
        }
        if (cancel != nullptr && cancel->load(std::memory_order_relaxed)) {
          fail("execution stopped");
          return TaskEnd::Stopped;
        }
      }
      VmKernelFrame &top = frames.back();
      if (top.ip < top.function->instructions.size()) {
        const IrInstruction &inst = top.function->instructions[top.ip];
        if (inst.op == IrOpcode::TaskSpawn) {
          if (!spawn(inst.imm, holder, stack, error)) {
            fail(error);
            return TaskEnd::Stopped;
          }
          top.ip += 1;
          continue;
        }
        if (isVmSyncOpcode(inst.op)) {
          const SyncOutcome synced =
              stepThreadedSyncOpcode(inst, sync_, host_, stack, frames, holder, error);
          if (synced == SyncOutcome::Fault) {
            fail(error);
          }
          if (synced != SyncOutcome::Done) {
            return TaskEnd::Stopped;
          }
          top.ip += 1;
          continue;
        }
        if (inst.op == IrOpcode::TaskWait) {
          if (!wait(holder, stack, error)) {
            if (!error.empty()) {
              fail(error);
            }
            return TaskEnd::Stopped;
          }
          top.ip += 1;
          continue;
        }
      }
      switch (
          stepVmKernelUntracked(module_, host_, stack, frames, localCounts_, value, event, error)) {
      case VmKernelStepOutcome::Continue:
        continue;
      case VmKernelStepOutcome::Fault:
        fail(error);
        return TaskEnd::Stopped;
      case VmKernelStepOutcome::Exit:
        break;
      }
      const VmKernelFrame &exitFrame = frames.back();
      const bool trapped = exitFrame.ip < exitFrame.function->instructions.size() &&
                           exitFrame.function->instructions[exitFrame.ip].op == IrOpcode::Trap;
      return trapped ? TaskEnd::Trapped : TaskEnd::Returned;
    }
  }

  // TaskSpawn: pops the callee's arguments, gives the task a heap and a thread, and pushes its handle. False with
  // `error` set for a fault; false with it empty when the run is already stopping.
  bool
  spawn(uint64_t functionImm, uint64_t spawner, std::vector<uint64_t> &stack, std::string &error) {
    error.clear();
    if (functionImm >= module_.functions.size()) {
      error = "invalid task function in IR";
      return false;
    }
    const size_t functionIndex = static_cast<size_t>(functionImm);
    const size_t parameterCount = module_.functions[functionIndex].parameterCount;
    if (stack.size() < parameterCount) {
      error = "IR stack underflow on task spawn";
      return false;
    }
    if (sync_.stop.load(std::memory_order_relaxed)) {
      return false;
    }
    auto task = std::make_unique<ThreadTask>();
    task->owner = this;
    task->functionIndex = functionIndex;
    task->arguments.assign(stack.end() - static_cast<std::ptrdiff_t>(parameterCount), stack.end());
    stack.resize(stack.size() - parameterCount);
    // The spawner acquires the heap: the first acquire pins the first arena's storage, which is only safe while the
    // main task is the one running thread, as it is at the first spawn.
    task->heap = host_.acquireTaskHeap(error);
    if (task->heap == nullptr) {
      return false;
    }
    ThreadTask *raw = task.get();
    size_t slot = 0;
    uint16_t generation = 0;
    {
      std::lock_guard<std::mutex> lock(sync_.mutex);
      if (slots_.size() - freeSlots_.size() >= MaxLiveTasks) {
        host_.releaseTaskHeap(raw->heap);
        error = "too many live tasks";
        return false;
      }
      if (!freeSlots_.empty()) {
        slot = freeSlots_.back();
        freeSlots_.pop_back();
      } else {
        slot = slots_.size();
        slots_.emplace_back();
      }
      raw->holder = nextHolder_++;
      // Noted with the task numbered, so the log's spawn events come in the order of the task numbers.
      if (sync_.recorder != nullptr) {
        sync_.recorder->note(spawner, SyncEventKind::Spawn);
      }
      slots_[slot].task = std::move(task);
      generation = slots_[slot].generation;
      ++sync_.spawned;
    }
    pthread_attr_t attributes;
    int rc = pthread_attr_init(&attributes);
    if (rc == 0) {
      rc = pthread_attr_setstacksize(&attributes, TaskStackBytes);
      if (rc == 0) {
        rc = pthread_attr_setdetachstate(&attributes, PTHREAD_CREATE_DETACHED);
      }
      if (rc == 0) {
        pthread_t thread;
        rc = pthread_create(&thread, &attributes, &VmThreadedTasks::threadMain, raw);
      }
      pthread_attr_destroy(&attributes);
    }
    if (rc != 0) {
      {
        std::lock_guard<std::mutex> lock(sync_.mutex);
        slots_[slot].task.reset();
        freeSlots_.push_back(slot);
        --sync_.spawned;
        sync_.signalChange();
      }
      host_.releaseTaskHeap(raw->heap);
      error = std::string("cannot start a task thread: ") + std::strerror(rc);
      return false;
    }
    stack.push_back(makeTaskHandle(generation, slot));
    return true;
  }

  // TaskWait: consumes the handle on top of the stack, sleeps until the task's function has returned and replaces
  // the handle by its result. False with `error` set for a fault; false with it empty when the run stopped.
  bool wait(uint64_t waiter, std::vector<uint64_t> &stack, std::string &error) {
    error.clear();
    if (stack.empty()) {
      error = "IR stack underflow on task wait";
      return false;
    }
    const uint64_t handle = stack.back();
    const size_t slot = static_cast<size_t>(handle & 0xFFFFFFFFull);
    const uint16_t generation = static_cast<uint16_t>((handle >> 32) & 0xFFFFull);
    std::unique_lock<std::mutex> lock(sync_.mutex);
    if ((handle & TaskHandleTagMask) != TaskHandleTag || slot >= slots_.size()) {
      error = "invalid task handle in IR";
      return false;
    }
    Slot &entry = slots_[slot];
    if (entry.task == nullptr || entry.generation != generation) {
      error = "task handle already waited";
      return false;
    }
    std::unique_ptr<ThreadTask> task = std::move(entry.task);
    entry.generation = static_cast<uint16_t>(entry.generation + 1);
    freeSlots_.push_back(slot);
    // A wait that no live thread can end (every task waits) is the run's deadlock fault.
    const WaitResult waited = waitForChange(sync_, lock, [&] { return task->done; });
    if (waited == WaitResult::Deadlock) {
      orphans_.push_back(std::move(task));
      error = DeadlockMessage;
      return false;
    }
    if (!task->done || ended_) {
      // The run is over (this task may even have ended it with a trap, so its result is not an answer any more); a
      // thread may still hold the task, so it is freed when the run ends.
      orphans_.push_back(std::move(task));
      return false;
    }
    if (sync_.recorder != nullptr) {
      sync_.recorder->note(waiter, SyncEventKind::Wait);
    }
    stack.back() = task->result;
    return true;
  }

  static void *threadMain(void *raw) {
    ThreadTask *task = static_cast<ThreadTask *>(raw);
    VmThreadedTasks &owner = *task->owner;
    owner.host_.enterTaskHeap(task->heap);
    uint64_t value = 0;
    const TaskEnd end =
        owner.runGuarded(task->functionIndex, task->holder, std::move(task->arguments), value);
    if (end == TaskEnd::Trapped) {
      owner.finishWith(value);
    }
    // The arena's live blocks stay valid: a result the task allocated is freed by whoever holds it.
    owner.host_.releaseTaskHeap(task->heap);
    // The last use of the task and of the run: the waiter or the end of the run may free both once the lock is free.
    std::lock_guard<std::mutex> lock(owner.sync_.mutex);
    task->result = value;
    task->done = true;
    --owner.sync_.spawned;
    owner.sync_.signalChange();
    return nullptr;
  }

  const IrModule &module_;
  VmKernelHost &host_;
  const std::vector<size_t> localCounts_;

  // The mutex, condition variable, stop flag and wait counts every thread of the run shares.
  ThreadedWaitState sync_;
  // Guarded by sync_.mutex.
  std::vector<Slot> slots_;
  std::vector<size_t> freeSlots_;
  std::vector<std::unique_ptr<ThreadTask>> orphans_;
  uint64_t nextHolder_ = MainHolder + 1;
  bool ended_ = false;
  bool failed_ = false;
  uint64_t result_ = 0;
  std::string error_;
};

} // namespace

bool runVmThreadedTasks(const IrModule &module,
                        VmKernelHost &host,
                        uint64_t &result,
                        std::string &error) {
  std::vector<size_t> localCounts(module.functions.size(), 0);
  for (size_t i = 0; i < module.functions.size(); ++i) {
    localCounts[i] = computeVmKernelLocalCount(module.functions[i]);
  }
  const VmSyncLogPaths *logPaths = host.syncLogPaths();
  std::unique_ptr<SyncLogRecorder> recorder;
  if (logPaths != nullptr && !logPaths->record.empty()) {
    uint64_t fingerprint = 0;
    if (!fingerprintIrModule(module, fingerprint, error)) {
      return false;
    }
    recorder = SyncLogRecorder::open(logPaths->record, fingerprint, error);
    if (recorder == nullptr) {
      return false;
    }
  }
  VmThreadedTasks tasks(module, host, std::move(localCounts), recorder.get());
  const bool ran = tasks.run(result, error);
  // Every task thread ended, so the log is complete; a run that faulted keeps the events up to its fault.
  std::string recordError;
  if (recorder != nullptr && !recorder->finish(recordError) && ran) {
    error = recordError;
    return false;
  }
  return ran;
}

} // namespace primec::vm_detail
