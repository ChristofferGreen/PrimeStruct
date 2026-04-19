#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "primec/Ir.h"

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
};

bool resolveSourceBreakpoints(const IrModule &module,
                              uint32_t line,
                              std::optional<uint32_t> column,
                              std::vector<VmResolvedSourceBreakpoint> &outBreakpoints,
                              std::string &error);

class Vm {
public:
  bool execute(const IrModule &module, uint64_t &result, std::string &error, uint64_t argCount = 0) const;
  bool execute(const IrModule &module,
               uint64_t &result,
               std::string &error,
               const std::vector<std::string_view> &args) const;
};

class VmDebugSession {
public:
  bool start(const IrModule &module, std::string &error, uint64_t argCount = 0);
  bool start(const IrModule &module, std::string &error, const std::vector<std::string_view> &args);
  bool step(VmDebugStopReason &stopReason, std::string &error);
  bool continueExecution(VmDebugStopReason &stopReason, std::string &error);
  bool pause(std::string &error);
  bool addBreakpoint(size_t functionIndex, size_t instructionPointer, std::string &error);
  bool addSourceBreakpoint(uint32_t line, std::optional<uint32_t> column, size_t &resolvedCount, std::string &error);
  bool removeBreakpoint(size_t functionIndex, size_t instructionPointer, std::string &error);
  void clearBreakpoints();
  void setHooks(const VmDebugHooks &hooks);
  void clearHooks();
  VmDebugSnapshot snapshot() const;
  VmDebugSnapshotPayload snapshotPayload() const;

  struct HeapAllocation {
    size_t baseIndex = 0;
    size_t slotCount = 0;
    bool live = false;
  };

private:
  struct Frame {
    const IrFunction *function = nullptr;
    size_t functionIndex = 0;
    std::vector<uint64_t> locals;
    size_t ip = 0;
    bool returnValueToCaller = false;
  };

  enum class StepOutcome { Continue, Exit, Fault };

  bool initFromModule(const IrModule &module, uint64_t argCount, const std::vector<std::string_view> *args);
  void appendMappedStackTrace(std::string &error) const;
  StepOutcome stepInstruction(std::string &error);

  const IrModule *module_ = nullptr;
  uint64_t argCount_ = 0;
  // Debug sessions deep-copy argv text at startup; `argvViews_` stays null for
  // count-only launches and otherwise points at `ownedArgViews_`.
  const std::vector<std::string_view> *argvViews_ = nullptr;
  std::vector<std::string> ownedArgStorage_;
  std::vector<std::string_view> ownedArgViews_;
  std::vector<size_t> localCounts_;
  std::vector<uint64_t> stack_;
  std::vector<uint64_t> heapSlots_;
  std::vector<HeapAllocation> heapAllocations_;
  std::vector<Frame> frames_;
  VmDebugSessionState state_ = VmDebugSessionState::Idle;
  uint64_t result_ = 0;
  bool pauseRequested_ = false;
  bool lastStopWasBreakpoint_ = false;
  std::set<std::pair<size_t, size_t>> breakpoints_;
  uint64_t nextHookSequence_ = 0;
  VmDebugHooks hooks_;
};

} // namespace primec
