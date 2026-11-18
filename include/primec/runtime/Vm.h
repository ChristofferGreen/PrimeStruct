#pragma once

#include <array>
#include <atomic>
#include <memory>
#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "primec/ir/Ir.h"
#include "primec/runtime/VmExecutionKernel.h"
#include "primec/runtime/VmHeapCore.h"
#include "primec/runtime/VmTextHeap.h"
#include "primec/runtime/VmHost.h"

namespace primec {

enum class VmDebugSessionState { Idle, Running, Paused, Faulted, Exited };

enum class VmDebugStopReason { Breakpoint, Step, Pause, Fault, Exit };

enum class VmDebugSessionCommand { Start, Continue, Reset };

struct VmDebugTransitionResult {
  VmDebugSessionState state = VmDebugSessionState::Idle;
  bool valid = false;
};

inline constexpr std::array<VmDebugStopReason, 5> vmDebugStopReasons() {
  return {
      VmDebugStopReason::Breakpoint,
      VmDebugStopReason::Step,
      VmDebugStopReason::Pause,
      VmDebugStopReason::Fault,
      VmDebugStopReason::Exit,
  };
}

inline constexpr std::string_view vmDebugStopReasonName(VmDebugStopReason reason) {
  switch (reason) {
    case VmDebugStopReason::Breakpoint:
      return "Breakpoint";
    case VmDebugStopReason::Step:
      return "Step";
    case VmDebugStopReason::Pause:
      return "Pause";
    case VmDebugStopReason::Fault:
      return "Fault";
    case VmDebugStopReason::Exit:
      return "Exit";
  }
  return "<invalid>";
}

inline constexpr std::string_view vmDebugSessionStateName(VmDebugSessionState state) {
  switch (state) {
    case VmDebugSessionState::Idle:
      return "Idle";
    case VmDebugSessionState::Running:
      return "Running";
    case VmDebugSessionState::Paused:
      return "Paused";
    case VmDebugSessionState::Faulted:
      return "Faulted";
    case VmDebugSessionState::Exited:
      return "Exited";
  }
  return "<invalid>";
}

inline constexpr VmDebugTransitionResult vmDebugApplyCommand(VmDebugSessionState state,
                                                             VmDebugSessionCommand command) {
  switch (command) {
    case VmDebugSessionCommand::Start:
      if (state == VmDebugSessionState::Idle) {
        return {VmDebugSessionState::Running, true};
      }
      break;
    case VmDebugSessionCommand::Continue:
      if (state == VmDebugSessionState::Paused) {
        return {VmDebugSessionState::Running, true};
      }
      break;
    case VmDebugSessionCommand::Reset:
      if (state == VmDebugSessionState::Paused || state == VmDebugSessionState::Faulted ||
          state == VmDebugSessionState::Exited) {
        return {VmDebugSessionState::Idle, true};
      }
      break;
  }
  return {state, false};
}

inline constexpr VmDebugTransitionResult vmDebugApplyStopReason(VmDebugSessionState state, VmDebugStopReason reason) {
  if (state != VmDebugSessionState::Running) {
    return {state, false};
  }
  switch (reason) {
    case VmDebugStopReason::Breakpoint:
    case VmDebugStopReason::Step:
    case VmDebugStopReason::Pause:
      return {VmDebugSessionState::Paused, true};
    case VmDebugStopReason::Fault:
      return {VmDebugSessionState::Faulted, true};
    case VmDebugStopReason::Exit:
      return {VmDebugSessionState::Exited, true};
  }
  return {state, false};
}

struct VmDebugSnapshot {
  VmDebugSessionState state = VmDebugSessionState::Idle;
  size_t functionIndex = 0;
  size_t instructionPointer = 0;
  size_t callDepth = 0;
  size_t operandStackSize = 0;
  uint64_t result = 0;
};

struct VmDebugStackFrameSnapshot {
  size_t functionIndex = 0;
  size_t instructionPointer = 0;
};

struct VmDebugSnapshotPayload {
  size_t instructionPointer = 0;
  std::vector<VmDebugStackFrameSnapshot> callStack;
  std::vector<std::vector<uint64_t>> frameLocals;
  std::vector<uint64_t> currentFrameLocals;
  std::vector<uint64_t> operandStack;
};

struct VmDebugInstructionHookEvent {
  uint64_t sequence = 0;
  VmDebugSnapshot snapshot;
  IrOpcode opcode = IrOpcode::PushI32;
  uint64_t immediate = 0;
};

struct VmDebugCallHookEvent {
  uint64_t sequence = 0;
  VmDebugSnapshot snapshot;
  size_t functionIndex = 0;
  bool returnsValueToCaller = false;
};

struct VmDebugFaultHookEvent {
  uint64_t sequence = 0;
  VmDebugSnapshot snapshot;
  IrOpcode opcode = IrOpcode::PushI32;
  uint64_t immediate = 0;
  std::string_view message;
};

using VmDebugInstructionHook = void (*)(const VmDebugInstructionHookEvent &, void *);
using VmDebugCallHook = void (*)(const VmDebugCallHookEvent &, void *);
using VmDebugFaultHook = void (*)(const VmDebugFaultHookEvent &, void *);

struct VmDebugHooks {
  VmDebugInstructionHook beforeInstruction = nullptr;
  VmDebugInstructionHook afterInstruction = nullptr;
  VmDebugCallHook callPush = nullptr;
  VmDebugCallHook callPop = nullptr;
  VmDebugFaultHook fault = nullptr;
  void *userData = nullptr;
};

struct VmResolvedSourceBreakpoint {
  size_t functionIndex = 0;
  size_t instructionPointer = 0;
  uint32_t debugId = 0;
  uint32_t line = 0;
  uint32_t column = 0;
  IrSourceMapProvenance provenance = IrSourceMapProvenance::Unknown;
  std::string sourceUnit{};
};

bool resolveSourceBreakpoints(const IrModule &module,
                              uint32_t line,
                              std::optional<uint32_t> column,
                              std::vector<VmResolvedSourceBreakpoint> &outBreakpoints,
                              std::string &error,
                              std::optional<std::string_view> sourceUnit = std::nullopt);

// Receives the program's stdout and stderr (print opcodes, and file writes to handle 1 or 2) in
// program order instead of the process writing them to fd 1 and 2. `fd` is 1 or 2; `chunk` is valid
// only during the call. With no sink set, output goes to the process streams exactly as before.
struct VmOutputSink {
  using Write = void (*)(int fd, std::string_view chunk, void *userData);
  Write write = nullptr;
  void *userData = nullptr;
};

class Vm {
public:
  // Applies to the execute() overloads of this object (debug sessions are not covered).
  void setOutputSink(const VmOutputSink &sink) {
    outputSink_ = sink;
  }
  void clearOutputSink() {
    outputSink_ = {};
  }
  // A run polls the flag and stops with the error "execution stopped" once it is true, so a host can cancel a
  // script that does not end. The flag must outlive the run. A run with a flag takes the plain interpreter, not
  // the fast kernel (which does not poll).
  void setCancelFlag(const std::atomic<bool> *flag) {
    cancelFlag_ = flag;
  }
  // The schedule of a module that spawns tasks (docs/spec/vm-design.md, Tasks): none (the default) runs a new task
  // eagerly until it ends or blocks; a seed picks the next task at every switch point with a generator seeded by it,
  // so the same seed gives the same run (--vm-schedule-seed, PRIMESTRUCT_VM_SCHEDULE_SEED).
  void setScheduleSeed(std::optional<uint64_t> seed) {
    scheduleSeed_ = seed;
  }

  // Runs each task of a module that spawns tasks on an OS thread of its own, with a heap per thread and one lock for
  // host calls (docs/spec/vm-design.md, Tasks on threads; --vm-threads). Off by default. The run then ignores the
  // schedule seed, takes the step kernel for a module with tasks, and refuses a module with atomic, mutex or
  // condition opcodes. A fault or trap of any task ends the run; it returns once every task thread ended.
  void setTaskThreads(bool enabled) {
    taskThreads_ = enabled;
  }

  // Synchronization record and replay (docs/spec/vm-design.md; `--vm-record-sync`, `--vm-replay-sync`). A run with
  // task threads and a record path writes the order of its synchronization events there; a run with a replay path
  // follows that order on the deterministic scheduler and fails with a diagnostic when the file belongs to another
  // program or the run does not follow it. The two are exclusive, and a record needs task threads (an empty path
  // turns either off).
  void setSyncRecordPath(std::string path) {
    syncLog_.record = std::move(path);
  }
  void setSyncReplayPath(std::string path) {
    syncLog_.replay = std::move(path);
  }

  bool execute(const IrModule &module, uint64_t &result, std::string &error, uint64_t argCount = 0) const;
  bool execute(const IrModule &module,
               uint64_t &result,
               std::string &error,
               const std::vector<std::string_view> &args) const;
  // Runs with host functions bound for IrOpcode::CallHost. Every host import of
  // `module` must be bound with a matching signature or execution fails before
  // the first instruction.
  bool execute(const IrModule &module,
               uint64_t &result,
               std::string &error,
               const std::vector<std::string_view> &args,
               const VmHostFunctions &hostFunctions) const;

private:
  const vm_detail::VmSyncLogPaths *syncLogOrNull() const;

  VmOutputSink outputSink_{};
  const std::atomic<bool> *cancelFlag_ = nullptr;
  std::optional<uint64_t> scheduleSeed_;
  bool taskThreads_ = false;
  vm_detail::VmSyncLogPaths syncLog_;
};

// A thread of a debug session is a task of the program (docs/spec/vm-design.md, Tasks): the main task is thread 1 and
// the task numbered n is thread n + 1.
struct VmDebugThread {
  uint64_t id = 1;
  std::string name;
};

struct VmDebugTasks;

class VmDebugSession {
public:
  bool start(const IrModule &module, std::string &error, uint64_t argCount = 0);
  bool start(const IrModule &module, std::string &error, const std::vector<std::string_view> &args);
  // Like start(), with host functions bound for IrOpcode::CallHost (copied into
  // the session). Every host import of `module` must be bound, as for
  // Vm::execute.
  bool start(const IrModule &module,
             std::string &error,
             const std::vector<std::string_view> &args,
             const VmHostFunctions &hostFunctions);
  bool step(VmDebugStopReason &stopReason, std::string &error);
  bool continueExecution(VmDebugStopReason &stopReason, std::string &error);
  bool pause(std::string &error);
  bool addBreakpoint(size_t functionIndex, size_t instructionPointer, std::string &error);
  bool addSourceBreakpoint(uint32_t line,
                           std::optional<uint32_t> column,
                           size_t &resolvedCount,
                           std::string &error,
                           std::optional<std::string_view> sourceUnit = std::nullopt);
  bool removeBreakpoint(size_t functionIndex, size_t instructionPointer, std::string &error);
  void clearBreakpoints();
  void setHooks(const VmDebugHooks &hooks);
  void clearHooks();
  VmDebugSnapshot snapshot() const;
  VmDebugSnapshotPayload snapshotPayload() const;
  // The tasks of the session that have not ended: just the main task unless the program spawns tasks. A debug
  // session runs them on the deterministic scheduler, eager by default.
  std::vector<VmDebugThread> threads() const;
  // The thread that runs next, and so the one a stop is in.
  uint64_t currentThreadId() const;
  // The state of one thread; false when no such thread is alive.
  bool threadSnapshotPayload(uint64_t threadId, VmDebugSnapshotPayload &out) const;
  // The schedule seed of a program with tasks (see primevm --vm-schedule-seed); set before the first step.
  void setScheduleSeed(std::optional<uint64_t> seed) {
    scheduleSeed_ = seed;
  }

private:
  // The session steps the shared VM kernel (VmExecutionKernel.h), so its frames
  // are the kernel's.
  using Frame = vm_detail::VmKernelFrame;

  enum class StepOutcome { Continue, Exit, Fault };

  bool initFromModule(const IrModule &module, uint64_t argCount, const std::vector<std::string_view> *args);
  void appendMappedStackTrace(std::string &error) const;
  StepOutcome stepInstruction(std::string &error);
  // The frames and operand stack of the task that runs next (the only ones without tasks).
  const std::vector<Frame> &currentFrames() const;
  const std::vector<uint64_t> &currentStack() const;

  const IrModule *module_ = nullptr;
  uint64_t argCount_ = 0;
  // Debug sessions deep-copy argv text at startup; `argvViews_` stays null for
  // count-only launches and otherwise points at `ownedArgViews_`.
  const std::vector<std::string_view> *argvViews_ = nullptr;
  std::vector<std::string> ownedArgStorage_;
  std::vector<std::string_view> ownedArgViews_;
  std::vector<size_t> localCounts_;
  std::vector<uint64_t> stack_;
  VmHeapCore heap_;
  std::vector<Frame> frames_;
  std::optional<VmHostFunctions> hostFunctions_;
  std::shared_ptr<VmTextHeap> textHeap_;
  VmDebugSessionState state_ = VmDebugSessionState::Idle;
  uint64_t result_ = 0;
  bool pauseRequested_ = false;
  bool lastStopWasBreakpoint_ = false;
  std::set<std::pair<size_t, size_t>> breakpoints_;
  uint64_t nextHookSequence_ = 0;
  VmDebugHooks hooks_;
  // A program that spawns tasks steps through the task scheduler, made at the first step.
  bool usesTasks_ = false;
  std::optional<uint64_t> scheduleSeed_;
  std::shared_ptr<VmDebugTasks> tasks_;
};

} // namespace primec
