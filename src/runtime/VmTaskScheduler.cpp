#include "VmTaskScheduler.h"

#include "VmFastKernel.h"
#include "VmKernelLocalize.h"
#include "VmSyncLog.h"
#include "VmTaskHandle.h"

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

} // namespace

// The VM's tasks (docs/spec/vm-design.md, Tasks): each is a call stack of its own; all run on this one thread and
// share the heap. A task switch happens only at a spawn, a wait and a task's end. The default schedule is eager: a
// spawn runs the new task until it ends or blocks, then its spawner continues, so a program prints what it prints
// with every spawned call run inline; a blocked task yields to the lowest-numbered runnable task. A seed picks the
// next task at every switch point with a splitmix64 generator, so the same seed gives the same run. A replay
// (docs/spec/vm-design.md, Synchronization record and replay) lets a task run its next scheduler opcode only when the
// record says it is that task's turn; a task that reaches one out of turn is parked until the record gets to it.
class VmTaskScheduler {
public:
  // With `fastFunctions` every task runs on the fast loop between switch points (docs/spec/vm-design.md, Tasks);
  // without, on the step kernel.
  VmTaskScheduler(const IrModule &module,
                  VmKernelHost &host,
                  const std::vector<size_t> &localCounts,
                  std::optional<uint64_t> seed,
                  const std::vector<FastFunction> *fastFunctions,
                  const std::vector<SyncEvent> *replay)
      : module_(module), host_(host), localCounts_(localCounts), seeded_(seed.has_value()),
        rng_(seed.value_or(0)), fast_(fastFunctions), replay_(replay) {
  }

  bool run(uint64_t &result, std::string &error) {
    start();
    if (fast_ != nullptr) {
      return runFast(mainSlot_, result, error);
    }
    VmKernelStepEvent event;
    const std::atomic<bool> *cancel = host_.cancelFlag();
    uint32_t steps = 0;
    while (true) {
      if (cancel != nullptr && (++steps & 0x3FF) == 0 && cancel->load(std::memory_order_relaxed)) {
        error = "execution stopped";
        return false;
      }
      switch (stepOne(false, event, result, error)) {
      case VmKernelStepOutcome::Continue:
        continue;
      case VmKernelStepOutcome::Fault:
        return false;
      case VmKernelStepOutcome::Exit:
        return true;
      }
    }
  }

  // Creates the main task; a stepped run (debug sessions) calls this once, then stepOne().
  void start() {
    mainSlot_ = createTask(static_cast<size_t>(module_.entryIndex), {}, NoTask);
    current_ = mainSlot_;
  }

  // One instruction of the current task on the step kernel, or one scheduler opcode (spawn, wait, lock, condition),
  // which may switch the current task. Exit: the main task returned or a task trapped, `result` is the program's.
  // `event` describes the call-stack change of the step when `trackEvents` is set (a task's own end is an Exit
  // event with the outcome Continue).
  VmKernelStepOutcome
  stepOne(bool trackEvents, VmKernelStepEvent &event, uint64_t &result, std::string &error) {
    Task &task = tasks_[current_];
    const VmKernelFrame &top = task.frames.back();
    if (top.ip < top.function->instructions.size()) {
      const IrOpcode op = top.function->instructions[top.ip].op;
      if (replay_ != nullptr && syncEventKindOf(op) != SyncEventKind::End) {
        bool turn = false;
        if (!replayGate(op, turn, error)) {
          return VmKernelStepOutcome::Fault;
        }
        if (!turn) {
          return VmKernelStepOutcome::Continue;
        }
      }
      bool scheduled = true;
      bool ok = true;
      if (op == IrOpcode::TaskSpawn) {
        ok = spawn(top.function->instructions[top.ip].imm, error);
      } else if (op == IrOpcode::TaskWait) {
        ok = wait(error);
      } else if (op == IrOpcode::MutexLock || op == IrOpcode::MutexUnlock) {
        ok = lockStep(op == IrOpcode::MutexLock, error);
      } else if (op == IrOpcode::ConditionWait || op == IrOpcode::ConditionNotify) {
        ok = conditionStep(op == IrOpcode::ConditionWait, error);
      } else {
        scheduled = false;
      }
      if (scheduled) {
        if (trackEvents) {
          event = {};
        }
        return ok ? VmKernelStepOutcome::Continue : VmKernelStepOutcome::Fault;
      }
    }
    // An atomic operation is a switch point of a seeded schedule: any runnable task may run after it.
    const bool atomicStep = seeded_ && top.ip < top.function->instructions.size() &&
                            isAtomicOpcode(top.function->instructions[top.ip].op);
    // Under a replay the atomic operation the step kernel runs next is an event of the record.
    const bool replayAtomic = replay_ != nullptr && top.ip < top.function->instructions.size() &&
                              isAtomicOpcode(top.function->instructions[top.ip].op);
    uint64_t value = 0;
    const VmKernelStepOutcome outcome =
        trackEvents
            ? stepVmKernel(
                  module_, host_, task.stack, task.frames, localCounts_, value, event, error)
            : stepVmKernelUntracked(
                  module_, host_, task.stack, task.frames, localCounts_, value, event, error);
    switch (outcome) {
    case VmKernelStepOutcome::Continue:
      if (atomicStep) {
        pickSeeded();
      }
      if (replayAtomic) {
        completeReplayEvent();
      }
      return VmKernelStepOutcome::Continue;
    case VmKernelStepOutcome::Fault:
      return VmKernelStepOutcome::Fault;
    case VmKernelStepOutcome::Exit:
      break;
    }
    // The main task's return, or a trap in any task, ends the program; another task's return ends that task.
    const VmKernelFrame &exitFrame = tasks_[current_].frames.back();
    const bool trapped = exitFrame.ip < exitFrame.function->instructions.size() &&
                         exitFrame.function->instructions[exitFrame.ip].op == IrOpcode::Trap;
    if (current_ == mainSlot_ || trapped) {
      result = value;
      return VmKernelStepOutcome::Exit;
    }
    return finishCurrent(value, error) ? VmKernelStepOutcome::Continue : VmKernelStepOutcome::Fault;
  }

  VmSteppedTaskView currentView() const {
    const Task &task = tasks_[current_];
    return {task.number, &task.frames, &task.stack};
  }

  // The tasks that have not ended (runnable, waiting or parked), in number order.
  void listLive(std::vector<VmSteppedTaskView> &out) const {
    out.clear();
    for (const Task &task : tasks_) {
      if (task.state == TaskState::Runnable || task.state == TaskState::Waiting ||
          task.state == TaskState::Parked) {
        out.push_back({task.number, &task.frames, &task.stack});
      }
    }
    std::sort(out.begin(), out.end(), [](const VmSteppedTaskView &a, const VmSteppedTaskView &b) {
      return a.number < b.number;
    });
  }

  // The program ended: the current task's frames go, as a plain debug session's do.
  void endProgram() {
    tasks_[current_].frames.clear();
  }

private:
  static constexpr size_t NoTask = static_cast<size_t>(-1);
  static constexpr size_t MaxLiveTasks = size_t{1} << 20;
  // Parked: a replay holds the task at a scheduler opcode until the record reaches its turn.
  enum class TaskState : uint8_t { Runnable, Waiting, Parked, Done, Free };
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

  // The diagnostic of a replay that cannot follow its record: the event it is at, and what is in the way.
  std::string divergence(const std::string &what) const {
    std::string text = "sync replay diverged at event " + std::to_string(cursor_ + 1) + " of " +
                       std::to_string(replay_->size());
    if (cursor_ < replay_->size()) {
      text += " (task " + std::to_string((*replay_)[cursor_].task) + " " +
              syncEventKindName((*replay_)[cursor_].kind) + ")";
    }
    return text + ": " + what;
  }

  // Replay (docs/spec/vm-design.md, Synchronization record and replay): `turn` is set when the current task's next
  // scheduler opcode is the record's next event. Otherwise the task is parked and another one picked. False with
  // `error` set when the task does what the record does not have.
  bool replayGate(IrOpcode op, bool &turn, std::string &error) {
    turn = false;
    const uint64_t holder = tasks_[current_].number + 1;
    if (cursor_ < replay_->size() && (*replay_)[cursor_].task == holder) {
      const SyncEventKind kind = syncEventKindOf(op);
      if ((*replay_)[cursor_].kind != kind) {
        error = divergence(std::string("task ") + std::to_string(holder) + " does " +
                           syncEventKindName(kind));
        return false;
      }
      turn = true;
      return true;
    }
    tasks_[current_].state = TaskState::Parked;
    removeRunnable(current_);
    return pickAfterStop(NoTask, error);
  }

  // A scheduler opcode completed (it moved its task on): it was the record's next event, so the record advances and
  // the task whose turn it is now is released if it was parked. This runs before the task that completed it blocks
  // or yields, so the next task is runnable when one is picked.
  void completeReplayEvent() {
    ++cursor_;
    if (cursor_ >= replay_->size()) {
      return;
    }
    const uint64_t next = (*replay_)[cursor_].task;
    for (size_t slot = 0; slot < tasks_.size(); ++slot) {
      if (tasks_[slot].state == TaskState::Parked && tasks_[slot].number + 1 == next) {
        tasks_[slot].state = TaskState::Runnable;
        addRunnable(slot);
        return;
      }
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
      error = replay_ != nullptr && cursor_ < replay_->size()
                  ? divergence("no task can run the next event")
                  : "deadlock: every task is waiting";
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
    operands(task).push_back(makeTaskHandle(tasks_[child].generation, child));
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
    if (replay_ != nullptr) {
      completeReplayEvent();
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
      if (replay_ != nullptr) {
        bool turn = false;
        if (!replayGate(inst.op, turn, error)) {
          return false;
        }
        if (!turn) {
          continue;
        }
      }
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
  size_t mainSlot_ = 0;
  const std::vector<FastFunction> *fast_ = nullptr;
  // The record a replay follows (null: not a replay) and the next event in it.
  const std::vector<SyncEvent> *replay_ = nullptr;
  size_t cursor_ = 0;
};

VmSteppedTasks::VmSteppedTasks(const IrModule &module,
                               VmKernelHost &host,
                               const std::vector<size_t> &localCounts,
                               std::optional<uint64_t> seed)
    : impl_(std::make_unique<VmTaskScheduler>(module, host, localCounts, seed, nullptr, nullptr)) {
  impl_->start();
}

VmSteppedTasks::~VmSteppedTasks() = default;

VmKernelStepOutcome
VmSteppedTasks::step(VmKernelStepEvent &event, uint64_t &result, std::string &error) {
  const VmKernelStepOutcome outcome = impl_->stepOne(true, event, result, error);
  if (outcome == VmKernelStepOutcome::Exit) {
    impl_->endProgram();
  }
  return outcome;
}

VmSteppedTaskView VmSteppedTasks::currentView() const {
  return impl_->currentView();
}

void VmSteppedTasks::listLive(std::vector<VmSteppedTaskView> &out) const {
  impl_->listLive(out);
}

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
  // A replay reads its record first: a record of another program is refused before anything runs.
  std::vector<SyncEvent> replay;
  bool replaying = false;
  if (const VmSyncLogPaths *paths = host.syncLogPaths(); paths != nullptr) {
    if (!paths->record.empty()) {
      error = "a sync record needs task threads (--vm-threads)";
      return false;
    }
    if (!paths->replay.empty()) {
      uint64_t fingerprint = 0;
      if (!fingerprintIrModule(module, fingerprint, error) ||
          !readSyncLog(paths->replay, fingerprint, replay, error)) {
        return false;
      }
      replaying = true;
    }
  }
  // Every task runs on the fast loop between switch points when the loop takes the whole module.
  std::vector<FastFunction> fastFunctions;
  const bool fast = vmFastKernelEnabled() && host.cancelFlag() == nullptr &&
                    host.slotBytes() == IrSlotBytes && prepareModule(module, fastFunctions, true);
  if (fast) {
    noteVmFastTaskRun();
  }
  // A replay follows its record, not a seed.
  VmTaskScheduler scheduler(module,
                            host,
                            localCounts,
                            replaying ? std::nullopt : host.scheduleSeed(),
                            fast ? &fastFunctions : nullptr,
                            replaying ? &replay : nullptr);
  return scheduler.run(result, error);
}

} // namespace primec::vm_detail
