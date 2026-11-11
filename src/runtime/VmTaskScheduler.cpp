#include "VmTaskScheduler.h"

#include "VmFastKernel.h"
#include "VmKernelLocalize.h"

#include <algorithm>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace primec::vm_detail {

namespace {

bool isAtomicOpcode(IrOpcode op) {
  return op == IrOpcode::AtomicLoad || op == IrOpcode::AtomicStore || op == IrOpcode::AtomicAdd ||
         op == IrOpcode::AtomicExchange || op == IrOpcode::AtomicCompareExchange;
}

// A task handle: a tag in bits 48-62 (bit 63 clear, so it is never a heap address), the slot's generation in bits
// 32-47 and the task slot in bits 0-31. A slot is reused once its task was waited, with the next generation.
constexpr uint64_t TaskHandleTag = uint64_t{0x7A5C} << 48;
constexpr uint64_t TaskHandleTagMask = uint64_t{0xFFFF} << 48;

// The VM's tasks (docs/spec/vm-design.md, Tasks): each is a call stack of its own; all run on this one thread and
// share the heap. A task switch happens only at a spawn, a wait and a task's end. The default schedule is eager: a
// spawn runs the new task until it ends or blocks, then its spawner continues, so a program prints what it prints
// with every spawned call run inline; a blocked task yields to the lowest-numbered runnable task. A seed picks the
// next task at every switch point with a splitmix64 generator, so the same seed gives the same run.
class VmTaskScheduler {
public:
  // With `fastFunctions` every task runs on the fast loop between switch points (docs/spec/vm-design.md, Tasks);
  // without, on the step kernel.
  VmTaskScheduler(const IrModule &module,
                  VmKernelHost &host,
                  const std::vector<size_t> &localCounts,
                  std::optional<uint64_t> seed,
                  const std::vector<FastFunction> *fastFunctions)
      : module_(module), host_(host), localCounts_(localCounts), seeded_(seed.has_value()),
        rng_(seed.value_or(0)), fast_(fastFunctions) {
  }

  bool run(uint64_t &result, std::string &error) {
    const size_t mainSlot = createTask(static_cast<size_t>(module_.entryIndex), {}, NoTask);
    current_ = mainSlot;
    if (fast_ != nullptr) {
      return runFast(mainSlot, result, error);
    }
    VmKernelStepEvent event;
    const std::atomic<bool> *cancel = host_.cancelFlag();
    uint32_t steps = 0;
    while (true) {
      if (cancel != nullptr && (++steps & 0x3FF) == 0 && cancel->load(std::memory_order_relaxed)) {
        error = "execution stopped";
        return false;
      }
      Task &task = tasks_[current_];
      const VmKernelFrame &top = task.frames.back();
      if (top.ip < top.function->instructions.size()) {
        const IrOpcode op = top.function->instructions[top.ip].op;
        if (op == IrOpcode::TaskSpawn) {
          if (!spawn(top.function->instructions[top.ip].imm, error)) {
            return false;
          }
          continue;
        }
        if (op == IrOpcode::TaskWait) {
          if (!wait(error)) {
            return false;
          }
          continue;
        }
        if (op == IrOpcode::MutexLock || op == IrOpcode::MutexUnlock) {
          if (!lockStep(op == IrOpcode::MutexLock, error)) {
            return false;
          }
          continue;
        }
        if (op == IrOpcode::ConditionWait || op == IrOpcode::ConditionNotify) {
          if (!conditionStep(op == IrOpcode::ConditionWait, error)) {
            return false;
          }
          continue;
        }
      }
      // An atomic operation is a switch point of a seeded schedule: any runnable task may run after it.
      const bool atomicStep = seeded_ && top.ip < top.function->instructions.size() &&
                              isAtomicOpcode(top.function->instructions[top.ip].op);
      uint64_t value = 0;
      switch (stepVmKernelUntracked(
          module_, host_, task.stack, task.frames, localCounts_, value, event, error)) {
      case VmKernelStepOutcome::Continue:
        if (atomicStep) {
          pickSeeded();
        }
        continue;
      case VmKernelStepOutcome::Fault:
        return false;
      case VmKernelStepOutcome::Exit:
        break;
      }
      // The main task's return, or a trap in any task, ends the program; another task's return ends that task.
      const VmKernelFrame &exitFrame = tasks_[current_].frames.back();
      const bool trapped = exitFrame.ip < exitFrame.function->instructions.size() &&
                           exitFrame.function->instructions[exitFrame.ip].op == IrOpcode::Trap;
      if (current_ == mainSlot || trapped) {
        result = value;
        return true;
      }
      if (!finishCurrent(value, error)) {
        return false;
      }
    }
  }

private:
  static constexpr size_t NoTask = static_cast<size_t>(-1);
  static constexpr size_t MaxLiveTasks = size_t{1} << 20;
  enum class TaskState : uint8_t { Runnable, Waiting, Done, Free };
  struct Task {
    std::vector<uint64_t> stack;
    std::vector<VmKernelFrame> frames;
    // The call stack of a task that runs on the fast loop (then `stack` and `frames` are unused).
    VmFastTask fast;
    TaskState state = TaskState::Free;
    // Tasks are numbered in spawn order (the main task is 0); a slot gets a new number when it is reused.
    uint64_t number = 0;
    uint16_t generation = 0;
    size_t spawner = NoTask;
    size_t waiter = NoTask;
    // The lock slot this task waits for (it is Waiting then), or 0.
    uint64_t lockAddress = 0;
    // The condition slot this task waits on (it is Waiting then), or 0.
    uint64_t conditionAddress = 0;
    uint64_t result = 0;
  };

  uint64_t nextRandom() {
    uint64_t z = (rng_ += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
  }

  size_t createTask(size_t functionIndex, std::vector<uint64_t> arguments, size_t spawner) {
    size_t slot = 0;
    if (!freeSlots_.empty()) {
      slot = freeSlots_.back();
      freeSlots_.pop_back();
    } else {
      slot = tasks_.size();
      tasks_.emplace_back();
    }
    Task &task = tasks_[slot];
    if (fast_ != nullptr) {
      initVmFastTask(*fast_, functionIndex, std::move(arguments), task.fast);
    } else {
      // The arguments are the task's operand stack, where the function's first instructions store its parameters.
      task.stack = std::move(arguments);
      task.frames.clear();
      VmKernelFrame frame;
      frame.functionIndex = functionIndex;
      frame.function = &module_.functions[functionIndex];
      frame.locals.assign(localCounts_[functionIndex], 0);
      // Each call stack numbers its own locals from 0, so a local address does not name a slot of another task.
      frame.localsBase = 0;
      task.frames.push_back(std::move(frame));
    }
    task.state = TaskState::Runnable;
    task.number = nextNumber_++;
    task.spawner = spawner;
    task.waiter = NoTask;
    task.lockAddress = 0;
    task.conditionAddress = 0;
    task.result = 0;
    addRunnable(slot);
    return slot;
  }

  // runnable_ holds the tasks that may run (not waiting, not ended), ordered by number.
  void addRunnable(size_t slot) {
    const std::pair<uint64_t, size_t> key{tasks_[slot].number, slot};
    runnable_.insert(std::lower_bound(runnable_.begin(), runnable_.end(), key), key);
  }

  void removeRunnable(size_t slot) {
    const std::pair<uint64_t, size_t> key{tasks_[slot].number, slot};
    const auto it = std::lower_bound(runnable_.begin(), runnable_.end(), key);
    if (it != runnable_.end() && *it == key) {
      runnable_.erase(it);
    }
  }

  // A switch point of a seeded schedule: any runnable task may run next.
  void pickSeeded() {
    current_ = runnable_[static_cast<size_t>(nextRandom() % runnable_.size())].second;
  }

  // The current task cannot go on. The default schedule runs `preferred` (the spawner of a task that ended) when it
  // can, else the lowest-numbered runnable task.
  bool pickAfterStop(size_t preferred, std::string &error) {
    if (runnable_.empty()) {
      error = "deadlock: every task is waiting";
      return false;
    }
    if (seeded_) {
      pickSeeded();
    } else if (preferred != NoTask && tasks_[preferred].state == TaskState::Runnable) {
      current_ = preferred;
    } else {
      current_ = runnable_.front().second;
    }
    return true;
  }

  bool spawn(uint64_t functionImm, std::string &error) {
    if (functionImm >= module_.functions.size()) {
      error = "invalid task function in IR";
      return false;
    }
    const size_t functionIndex = static_cast<size_t>(functionImm);
    const size_t parameterCount = module_.functions[functionIndex].parameterCount;
    const size_t spawner = current_;
    std::vector<uint64_t> &spawnerStack = operands(tasks_[spawner]);
    if (spawnerStack.size() < parameterCount) {
      error = "IR stack underflow on task spawn";
      return false;
    }
    if (tasks_.size() - freeSlots_.size() >= MaxLiveTasks) {
      error = "too many live tasks";
      return false;
    }
    std::vector<uint64_t> arguments(
        spawnerStack.end() - static_cast<std::ptrdiff_t>(parameterCount), spawnerStack.end());
    spawnerStack.resize(spawnerStack.size() - parameterCount);
    // createTask may grow tasks_, so the spawner is found again by its slot afterwards.
    const size_t child = createTask(functionIndex, std::move(arguments), spawner);
    Task &task = tasks_[spawner];
    operands(task).push_back(TaskHandleTag |
                             (static_cast<uint64_t>(tasks_[child].generation) << 32) |
                             static_cast<uint64_t>(child));
    advance(task);
    if (seeded_) {
      pickSeeded();
    } else {
      current_ = child;
    }
    return true;
  }

  bool wait(std::string &error) {
    Task &task = tasks_[current_];
    std::vector<uint64_t> &stack = operands(task);
    if (stack.empty()) {
      error = "IR stack underflow on task wait";
      return false;
    }
    const uint64_t handle = stack.back();
    const size_t slot = static_cast<size_t>(handle & 0xFFFFFFFFull);
    const uint16_t generation = static_cast<uint16_t>((handle >> 32) & 0xFFFFull);
    if ((handle & TaskHandleTagMask) != TaskHandleTag || slot >= tasks_.size() ||
        slot == current_) {
      error = "invalid task handle in IR";
      return false;
    }
    Task &target = tasks_[slot];
    if (target.generation != generation || target.state == TaskState::Free ||
        (target.waiter != NoTask && target.waiter != current_)) {
      error = "task handle already waited";
      return false;
    }
    if (target.state == TaskState::Done) {
      stack.back() = target.result;
      advance(task);
      target.state = TaskState::Free;
      target.generation = static_cast<uint16_t>(target.generation + 1);
      freeSlots_.push_back(slot);
      if (seeded_) {
        pickSeeded();
      }
      return true;
    }
    // Block until the task ends; this wait then runs again and finds it done.
    target.waiter = current_;
    task.state = TaskState::Waiting;
    removeRunnable(current_);
    return pickAfterStop(NoTask, error);
  }

  // MutexLock and MutexUnlock (docs/spec/vm-design.md, Mutexes). The lock slot holds 0 while free and the holder's
  // task number plus one otherwise. A task that finds the lock held waits, which counts for deadlock detection, and
  // runs its MutexLock again once the holder unlocked. Both are switch points of a seeded schedule.
  bool lockStep(bool lock, std::string &error) {
    Task &task = tasks_[current_];
    std::vector<uint64_t> &stack = operands(task);
    if (stack.empty()) {
      error = "IR stack underflow on mutex operation";
      return false;
    }
    const uint64_t lockAddress = stack.back();
    uint64_t *slot = nullptr;
    if (!resolveSlot(task, lockAddress, false, slot, error)) {
      return false;
    }
    const uint64_t holder = task.number + 1;
    if (lock) {
      if (*slot == holder) {
        error = "mutex locked again by the task that holds it";
        return false;
      }
      if (*slot != 0) {
        task.lockAddress = lockAddress;
        task.state = TaskState::Waiting;
        removeRunnable(current_);
        return pickAfterStop(NoTask, error);
      }
      *slot = holder;
    } else {
      if (*slot != holder) {
        error = "mutex unlocked by a task that does not hold it";
        return false;
      }
      *slot = 0;
      for (size_t waiting = 0; waiting < tasks_.size(); ++waiting) {
        Task &other = tasks_[waiting];
        if (other.state == TaskState::Waiting && other.lockAddress == lockAddress) {
          other.lockAddress = 0;
          other.state = TaskState::Runnable;
          addRunnable(waiting);
        }
      }
    }
    stack.pop_back();
    advance(task);
    if (seeded_) {
      pickSeeded();
    }
    return true;
  }

  // ConditionWait and ConditionNotify (docs/spec/vm-design.md, Conditions). A wait frees the lock the task holds and
  // leaves the task waiting, which counts for deadlock detection, until a notify on that condition slot; the task's
  // next instruction takes the lock again. A notify is a switch point of a seeded schedule.
  bool conditionStep(bool wait, std::string &error) {
    Task &task = tasks_[current_];
    const size_t operandCount = wait ? 2 : 1;
    std::vector<uint64_t> &stack = operands(task);
    if (stack.size() < operandCount) {
      error = "IR stack underflow on condition operation";
      return false;
    }
    auto resolve = [&](uint64_t address, uint64_t *&slot) {
      return resolveSlot(task, address, false, slot, error);
    };
    const uint64_t conditionAddress = stack[stack.size() - operandCount];
    uint64_t *conditionSlot = nullptr;
    if (!resolve(conditionAddress, conditionSlot)) {
      return false;
    }
    if (!wait) {
      for (size_t waiting = 0; waiting < tasks_.size(); ++waiting) {
        Task &other = tasks_[waiting];
        if (other.state == TaskState::Waiting && other.conditionAddress == conditionAddress) {
          other.conditionAddress = 0;
          other.state = TaskState::Runnable;
          addRunnable(waiting);
        }
      }
      stack.pop_back();
      advance(task);
      if (seeded_) {
        pickSeeded();
      }
      return true;
    }
    const uint64_t lockAddress = stack.back();
    uint64_t *lockSlot = nullptr;
    if (!resolve(lockAddress, lockSlot)) {
      return false;
    }
    if (*lockSlot != task.number + 1) {
      error = "condition wait by a task that does not hold the lock";
      return false;
    }
    *lockSlot = 0;
    for (size_t waiting = 0; waiting < tasks_.size(); ++waiting) {
      Task &other = tasks_[waiting];
      if (other.state == TaskState::Waiting && other.lockAddress == lockAddress) {
        other.lockAddress = 0;
        other.state = TaskState::Runnable;
        addRunnable(waiting);
      }
    }
    stack.resize(stack.size() - operandCount);
    advance(task);
    task.conditionAddress = conditionAddress;
    task.state = TaskState::Waiting;
    removeRunnable(current_);
    return pickAfterStop(NoTask, error);
  }

  // The operand stack of a task and its next instruction, whichever loop runs it.
  std::vector<uint64_t> &operands(Task &task) {
    return fast_ != nullptr ? task.fast.stack : task.stack;
  }

  void advance(Task &task) {
    if (fast_ != nullptr) {
      ++task.fast.ip;
    } else {
      task.frames.back().ip += 1;
    }
  }

  // The slot an address of `task`'s call stack names (a local of one of its frames, or heap).
  bool resolveSlot(Task &task, uint64_t address, bool atomic, uint64_t *&slot, std::string &error) {
    if (fast_ == nullptr) {
      std::vector<uint64_t> *addressLocals = nullptr;
      localizeVmAddress(task.frames, host_.slotBytes(), address, addressLocals);
      return atomic ? host_.resolveAtomicAddress(address, *addressLocals, slot, error)
                    : host_.resolveIndirectAddress(address, *addressLocals, slot, error);
    }
    if ((address & (uint64_t{1} << 63)) != 0) {
      std::vector<uint64_t> noLocals;
      return atomic ? host_.resolveAtomicAddress(address, noLocals, slot, error)
                    : host_.resolveIndirectAddress(address, noLocals, slot, error);
    }
    const uint64_t slotBytes = host_.slotBytes();
    if (address % slotBytes != 0) {
      error = "unaligned indirect address in IR: " + std::to_string(address);
      return false;
    }
    const uint64_t index = address / slotBytes;
    if (index >= task.fast.localsBase + task.fast.current->localCount) {
      error = "invalid indirect address in IR: " + std::to_string(address);
      return false;
    }
    slot = task.fast.localsArena.data() + index;
    return true;
  }

  // An atomic operation of a task on the fast loop (the step kernel runs its own in stepImpl). Under a schedule
  // seed it is a switch point.
  bool atomicStep(IrOpcode op, uint64_t width, std::string &error) {
    Task &task = tasks_[current_];
    std::vector<uint64_t> &stack = operands(task);
    const size_t operandCount = op == IrOpcode::AtomicLoad              ? 1u
                                : op == IrOpcode::AtomicCompareExchange ? 3u
                                                                        : 2u;
    if (stack.size() < operandCount) {
      error = "IR stack underflow on atomic operation";
      return false;
    }
    if (width != 32u && width != 64u) {
      error = "invalid atomic width in IR";
      return false;
    }
    const bool narrow = width == 32u;
    const auto canonical = [narrow](uint64_t value) {
      return narrow ? static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(value)))
                    : value;
    };
    const uint64_t second = operandCount >= 2 ? canonical(stack[stack.size() - 1]) : 0;
    const uint64_t first = operandCount == 3 ? canonical(stack[stack.size() - 2]) : second;
    const uint64_t address = stack[stack.size() - operandCount];
    stack.resize(stack.size() - operandCount);
    uint64_t *slot = nullptr;
    if (!resolveSlot(task, address, true, slot, error)) {
      return false;
    }
    const uint64_t before = canonical(*slot);
    switch (op) {
    case IrOpcode::AtomicLoad:
      stack.push_back(before);
      break;
    case IrOpcode::AtomicStore:
      *slot = second;
      break;
    case IrOpcode::AtomicAdd:
      *slot = canonical(before + second);
      stack.push_back(before);
      break;
    case IrOpcode::AtomicExchange:
      *slot = second;
      stack.push_back(before);
      break;
    default:
      if (before == first) {
        *slot = second;
        stack.push_back(1);
      } else {
        stack.push_back(0);
      }
      break;
    }
    advance(task);
    if (seeded_) {
      pickSeeded();
    }
    return true;
  }

  // The scheduler loop of a run on the fast loop: each task runs until it ends or reaches an opcode of the
  // scheduler, which runs here exactly as the step kernel's scheduler runs it.
  bool runFast(size_t mainSlot, uint64_t &result, std::string &error) {
    while (true) {
      uint64_t value = 0;
      VmFastRunOutcome outcome = VmFastRunOutcome::Finished;
      if (!runVmFastTask(
              module_, host_, *fast_, tasks_[current_].fast, true, value, outcome, error)) {
        return false;
      }
      if (outcome == VmFastRunOutcome::Trapped) {
        result = value;
        return true;
      }
      if (outcome == VmFastRunOutcome::Finished) {
        if (current_ == mainSlot) {
          result = value;
          return true;
        }
        if (!finishCurrent(value, error)) {
          return false;
        }
        continue;
      }
      const IrInstruction &inst = *tasks_[current_].fast.ip->source;
      bool ok = false;
      switch (inst.op) {
      case IrOpcode::TaskSpawn:
        ok = spawn(inst.imm, error);
        break;
      case IrOpcode::TaskWait:
        ok = wait(error);
        break;
      case IrOpcode::MutexLock:
      case IrOpcode::MutexUnlock:
        ok = lockStep(inst.op == IrOpcode::MutexLock, error);
        break;
      case IrOpcode::ConditionWait:
      case IrOpcode::ConditionNotify:
        ok = conditionStep(inst.op == IrOpcode::ConditionWait, error);
        break;
      default:
        ok = atomicStep(inst.op, inst.imm, error);
        break;
      }
      if (!ok) {
        return false;
      }
    }
  }

  bool finishCurrent(uint64_t value, std::string &error) {
    Task &task = tasks_[current_];
    task.result = value;
    removeRunnable(current_);
    task.state = TaskState::Done;
    task.frames = {};
    task.stack = {};
    task.fast = VmFastTask{};
    if (task.waiter != NoTask) {
      tasks_[task.waiter].state = TaskState::Runnable;
      addRunnable(task.waiter);
    }
    return pickAfterStop(task.spawner, error);
  }

  const IrModule &module_;
  VmKernelHost &host_;
  const std::vector<size_t> &localCounts_;
  bool seeded_ = false;
  uint64_t rng_ = 0;
  std::vector<Task> tasks_;
  std::vector<size_t> freeSlots_;
  std::vector<std::pair<uint64_t, size_t>> runnable_;
  uint64_t nextNumber_ = 0;
  size_t current_ = 0;
  const std::vector<FastFunction> *fast_ = nullptr;
};

} // namespace

bool moduleUsesTasks(const IrModule &module) {
  for (const IrFunction &function : module.functions) {
    for (const IrInstruction &inst : function.instructions) {
      if (inst.op == IrOpcode::TaskSpawn || inst.op == IrOpcode::TaskWait) {
        return true;
      }
    }
  }
  return false;
}

bool runVmTaskScheduler(const IrModule &module,
                        VmKernelHost &host,
                        uint64_t &result,
                        std::string &error) {
  std::vector<size_t> localCounts(module.functions.size(), 0);
  for (size_t i = 0; i < module.functions.size(); ++i) {
    localCounts[i] = computeVmKernelLocalCount(module.functions[i]);
  }
  // Every task runs on the fast loop between switch points when the loop takes the whole module.
  std::vector<FastFunction> fastFunctions;
  const bool fast = vmFastKernelEnabled() && host.cancelFlag() == nullptr &&
                    host.slotBytes() == IrSlotBytes && prepareModule(module, fastFunctions, true);
  if (fast) {
    noteVmFastTaskRun();
  }
  VmTaskScheduler scheduler(
      module, host, localCounts, host.scheduleSeed(), fast ? &fastFunctions : nullptr);
  return scheduler.run(result, error);
}

} // namespace primec::vm_detail
