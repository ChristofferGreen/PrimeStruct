#include "VmThreadedTasks.h"

#include "VmTaskHandle.h"
#include "VmTaskScheduler.h"

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

class VmThreadedTasks;

// One spawned task: its thread, its heap and, once its function returned, its result. `done` and `result` are
// written and read with the run's mutex held.
struct ThreadTask {
  VmThreadedTasks *owner = nullptr;
  size_t functionIndex = 0;
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
  VmThreadedTasks(const IrModule &module, VmKernelHost &host, std::vector<size_t> localCounts)
      : module_(module), host_(host), localCounts_(std::move(localCounts)) {
  }

  bool run(uint64_t &result, std::string &error) {
    uint64_t value = 0;
    const TaskEnd end = runGuarded(static_cast<size_t>(module_.entryIndex), {}, value);
    if (end != TaskEnd::Stopped) {
      finishWith(value);
    }
    {
      // Whatever ended the run, every thread stops and is gone before the run returns, so none reads a heap or a
      // host that no longer exists.
      std::unique_lock<std::mutex> lock(mutex_);
      if (!ended_) {
        ended_ = true;
        failed_ = true;
        error_ = "execution stopped";
      }
      stop_.store(true, std::memory_order_relaxed);
      changed_.notify_all();
      changed_.wait(lock, [this] { return running_ == 0; });
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
    std::lock_guard<std::mutex> lock(mutex_);
    if (!ended_) {
      ended_ = true;
      result_ = value;
    }
    stop_.store(true, std::memory_order_relaxed);
    changed_.notify_all();
  }

  // The program ends with a fault; only the first one is reported.
  void fail(std::string message) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!ended_) {
      ended_ = true;
      failed_ = true;
      error_ = std::move(message);
    }
    stop_.store(true, std::memory_order_relaxed);
    changed_.notify_all();
  }

  TaskEnd runGuarded(size_t functionIndex, std::vector<uint64_t> arguments, uint64_t &value) {
    try {
      return runTask(functionIndex, std::move(arguments), value);
    } catch (const std::exception &exception) {
      fail(std::string("exception: ") + exception.what());
    } catch (...) {
      fail("unknown exception");
    }
    return TaskEnd::Stopped;
  }

  // The step kernel on one call stack. Faults are reported through fail(); the answer is Stopped for them and for a
  // stop that another task started.
  TaskEnd runTask(size_t functionIndex, std::vector<uint64_t> arguments, uint64_t &value) {
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
        if (stop_.load(std::memory_order_relaxed)) {
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
          if (!spawn(inst.imm, stack, error)) {
            fail(error);
            return TaskEnd::Stopped;
          }
          top.ip += 1;
          continue;
        }
        if (inst.op == IrOpcode::TaskWait) {
          if (!wait(stack, error)) {
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
  bool spawn(uint64_t functionImm, std::vector<uint64_t> &stack, std::string &error) {
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
    if (stop_.load(std::memory_order_relaxed)) {
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
      std::lock_guard<std::mutex> lock(mutex_);
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
      slots_[slot].task = std::move(task);
      generation = slots_[slot].generation;
      ++running_;
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
        std::lock_guard<std::mutex> lock(mutex_);
        slots_[slot].task.reset();
        freeSlots_.push_back(slot);
        --running_;
        changed_.notify_all();
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
  bool wait(std::vector<uint64_t> &stack, std::string &error) {
    error.clear();
    if (stack.empty()) {
      error = "IR stack underflow on task wait";
      return false;
    }
    const uint64_t handle = stack.back();
    const size_t slot = static_cast<size_t>(handle & 0xFFFFFFFFull);
    const uint16_t generation = static_cast<uint16_t>((handle >> 32) & 0xFFFFull);
    std::unique_lock<std::mutex> lock(mutex_);
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
    changed_.wait(lock, [&] { return task->done || stop_.load(std::memory_order_relaxed); });
    if (!task->done || ended_) {
      // The run is over (this task may even have ended it with a trap, so its result is not an answer any more); a
      // thread may still hold the task, so it is freed when the run ends.
      orphans_.push_back(std::move(task));
      return false;
    }
    stack.back() = task->result;
    return true;
  }

  static void *threadMain(void *raw) {
    ThreadTask *task = static_cast<ThreadTask *>(raw);
    VmThreadedTasks &owner = *task->owner;
    owner.host_.enterTaskHeap(task->heap);
    uint64_t value = 0;
    const TaskEnd end = owner.runGuarded(task->functionIndex, std::move(task->arguments), value);
    if (end == TaskEnd::Trapped) {
      owner.finishWith(value);
    }
    // The arena's live blocks stay valid: a result the task allocated is freed by whoever holds it.
    owner.host_.releaseTaskHeap(task->heap);
    // The last use of the task and of the run: the waiter or the end of the run may free both once the lock is free.
    std::lock_guard<std::mutex> lock(owner.mutex_);
    task->result = value;
    task->done = true;
    --owner.running_;
    owner.changed_.notify_all();
    return nullptr;
  }

  const IrModule &module_;
  VmKernelHost &host_;
  const std::vector<size_t> localCounts_;

  std::mutex mutex_;
  std::condition_variable changed_;
  std::atomic<bool> stop_{false};
  // Guarded by mutex_.
  std::vector<Slot> slots_;
  std::vector<size_t> freeSlots_;
  std::vector<std::unique_ptr<ThreadTask>> orphans_;
  size_t running_ = 0;
  bool ended_ = false;
  bool failed_ = false;
  uint64_t result_ = 0;
  std::string error_;
};

} // namespace

bool moduleUsesSyncPrimitives(const IrModule &module) {
  for (const IrFunction &function : module.functions) {
    for (const IrInstruction &inst : function.instructions) {
      switch (inst.op) {
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
        break;
      }
    }
  }
  return false;
}

bool runVmThreadedTasks(const IrModule &module,
                        VmKernelHost &host,
                        uint64_t &result,
                        std::string &error) {
  std::vector<size_t> localCounts(module.functions.size(), 0);
  for (size_t i = 0; i < module.functions.size(); ++i) {
    localCounts[i] = computeVmKernelLocalCount(module.functions[i]);
  }
  VmThreadedTasks tasks(module, host, std::move(localCounts));
  return tasks.run(result, error);
}

} // namespace primec::vm_detail
