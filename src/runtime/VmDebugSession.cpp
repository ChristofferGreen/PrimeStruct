#include "primec/runtime/Vm.h"

#include "VmDebugTasks.h"

namespace primec {

bool VmDebugSession::step(VmDebugStopReason &stopReason, std::string &error) {
  if (state_ == VmDebugSessionState::Idle) {
    error = "debug session is not started";
    return false;
  }
  if (state_ != VmDebugSessionState::Paused) {
    error = "debug session is not paused";
    return false;
  }
  const VmDebugTransitionResult continueTransition = vmDebugApplyCommand(state_, VmDebugSessionCommand::Continue);
  if (!continueTransition.valid) {
    error = "debug session cannot step from current state";
    return false;
  }
  state_ = continueTransition.state;
  const StepOutcome outcome = stepInstruction(error);
  if (outcome == StepOutcome::Fault) {
    stopReason = VmDebugStopReason::Fault;
    const VmDebugTransitionResult stopTransition = vmDebugApplyStopReason(state_, stopReason);
    if (stopTransition.valid) {
      state_ = stopTransition.state;
    }
    lastStopWasBreakpoint_ = false;
    return false;
  }
  if (outcome == StepOutcome::Exit) {
    stopReason = VmDebugStopReason::Exit;
    const VmDebugTransitionResult stopTransition = vmDebugApplyStopReason(state_, stopReason);
    if (stopTransition.valid) {
      state_ = stopTransition.state;
    }
    lastStopWasBreakpoint_ = false;
    return true;
  }
  stopReason = VmDebugStopReason::Step;
  const VmDebugTransitionResult stopTransition = vmDebugApplyStopReason(state_, stopReason);
  if (!stopTransition.valid) {
    error = "debug session failed to enter paused state after step";
    return false;
  }
  state_ = stopTransition.state;
  lastStopWasBreakpoint_ = false;
  return true;
}

bool VmDebugSession::continueExecution(VmDebugStopReason &stopReason, std::string &error) {
  if (state_ == VmDebugSessionState::Idle) {
    error = "debug session is not started";
    return false;
  }
  if (state_ != VmDebugSessionState::Paused) {
    error = "debug session is not paused";
    return false;
  }
  const VmDebugTransitionResult continueTransition = vmDebugApplyCommand(state_, VmDebugSessionCommand::Continue);
  if (!continueTransition.valid) {
    error = "debug session cannot continue from current state";
    return false;
  }
  state_ = continueTransition.state;
  if (pauseRequested_) {
    pauseRequested_ = false;
    stopReason = VmDebugStopReason::Pause;
    const VmDebugTransitionResult stopTransition = vmDebugApplyStopReason(state_, stopReason);
    if (!stopTransition.valid) {
      error = "debug session failed to pause";
      return false;
    }
    state_ = stopTransition.state;
    lastStopWasBreakpoint_ = false;
    return true;
  }
  bool skipCurrentBreakpoint = lastStopWasBreakpoint_;
  lastStopWasBreakpoint_ = false;
  while (true) {
    if (!currentFrames().empty()) {
      const Frame &frame = currentFrames().back();
      const std::pair<size_t, size_t> key{frame.functionIndex, frame.ip};
      if (!skipCurrentBreakpoint && breakpoints_.contains(key)) {
        stopReason = VmDebugStopReason::Breakpoint;
        const VmDebugTransitionResult stopTransition = vmDebugApplyStopReason(state_, stopReason);
        if (!stopTransition.valid) {
          error = "debug session failed to pause at breakpoint";
          return false;
        }
        state_ = stopTransition.state;
        lastStopWasBreakpoint_ = true;
        return true;
      }
    }
    skipCurrentBreakpoint = false;
    const StepOutcome outcome = stepInstruction(error);
    if (outcome == StepOutcome::Fault) {
      stopReason = VmDebugStopReason::Fault;
      const VmDebugTransitionResult stopTransition = vmDebugApplyStopReason(state_, stopReason);
      if (stopTransition.valid) {
        state_ = stopTransition.state;
      }
      lastStopWasBreakpoint_ = false;
      return false;
    }
    if (outcome == StepOutcome::Exit) {
      stopReason = VmDebugStopReason::Exit;
      const VmDebugTransitionResult stopTransition = vmDebugApplyStopReason(state_, stopReason);
      if (!stopTransition.valid) {
        error = "debug session failed to exit";
        return false;
      }
      state_ = stopTransition.state;
      lastStopWasBreakpoint_ = false;
      return true;
    }
    if (pauseRequested_) {
      pauseRequested_ = false;
      stopReason = VmDebugStopReason::Pause;
      const VmDebugTransitionResult stopTransition = vmDebugApplyStopReason(state_, stopReason);
      if (!stopTransition.valid) {
        error = "debug session failed to pause";
        return false;
      }
      state_ = stopTransition.state;
      lastStopWasBreakpoint_ = false;
      return true;
    }
  }
}

bool VmDebugSession::addBreakpoint(size_t functionIndex, size_t instructionPointer, std::string &error) {
  if (!module_) {
    error = "debug session is not started";
    return false;
  }
  if (functionIndex >= module_->functions.size()) {
    error = "invalid breakpoint function index";
    return false;
  }
  const IrFunction &function = module_->functions[functionIndex];
  if (instructionPointer >= function.instructions.size()) {
    error = "invalid breakpoint instruction pointer";
    return false;
  }
  breakpoints_.insert({functionIndex, instructionPointer});
  return true;
}

bool VmDebugSession::addSourceBreakpoint(uint32_t line,
                                         std::optional<uint32_t> column,
                                         size_t &resolvedCount,
                                         std::string &error,
                                         std::optional<std::string_view> sourceUnit) {
  if (!module_) {
    error = "debug session is not started";
    return false;
  }
  std::vector<VmResolvedSourceBreakpoint> resolved;
  if (!resolveSourceBreakpoints(*module_, line, column, resolved, error, sourceUnit)) {
    return false;
  }
  for (const auto &breakpoint : resolved) {
    breakpoints_.insert({breakpoint.functionIndex, breakpoint.instructionPointer});
  }
  resolvedCount = resolved.size();
  return true;
}

bool VmDebugSession::removeBreakpoint(size_t functionIndex, size_t instructionPointer, std::string &error) {
  if (!module_) {
    error = "debug session is not started";
    return false;
  }
  const std::pair<size_t, size_t> key{functionIndex, instructionPointer};
  if (breakpoints_.erase(key) == 0) {
    error = "breakpoint not found";
    return false;
  }
  return true;
}

void VmDebugSession::clearBreakpoints() {
  breakpoints_.clear();
  lastStopWasBreakpoint_ = false;
}

void VmDebugSession::setHooks(const VmDebugHooks &hooks) {
  hooks_ = hooks;
}

void VmDebugSession::clearHooks() {
  hooks_ = {};
}

bool VmDebugSession::pause(std::string &error) {
  if (state_ == VmDebugSessionState::Idle) {
    error = "debug session is not started";
    return false;
  }
  if (state_ == VmDebugSessionState::Faulted || state_ == VmDebugSessionState::Exited) {
    error = "debug session cannot pause after termination";
    return false;
  }
  pauseRequested_ = true;
  return true;
}

VmDebugSnapshot VmDebugSession::snapshot() const {
  VmDebugSnapshot out;
  out.state = state_;
  const std::vector<Frame> &frames = currentFrames();
  out.callDepth = frames.size();
  out.operandStackSize = currentStack().size();
  out.result = result_;
  if (!frames.empty()) {
    out.functionIndex = frames.back().functionIndex;
    out.instructionPointer = frames.back().ip;
  }
  return out;
}

namespace {

VmDebugSnapshotPayload payloadOf(const std::vector<vm_detail::VmKernelFrame> &frames,
                                 const std::vector<uint64_t> &stack) {
  VmDebugSnapshotPayload out;
  out.operandStack = stack;
  out.callStack.reserve(frames.size());
  out.frameLocals.reserve(frames.size());
  for (const vm_detail::VmKernelFrame &frame : frames) {
    out.callStack.push_back({frame.functionIndex, frame.ip});
    out.frameLocals.push_back(frame.locals);
  }
  if (!frames.empty()) {
    const vm_detail::VmKernelFrame &frame = frames.back();
    out.instructionPointer = frame.ip;
    out.currentFrameLocals = frame.locals;
  }
  return out;
}

} // namespace

VmDebugSnapshotPayload VmDebugSession::snapshotPayload() const {
  return payloadOf(currentFrames(), currentStack());
}

uint64_t VmDebugSession::currentThreadId() const {
  return tasks_ ? tasks_->scheduler.currentView().number + 1 : 1;
}

std::vector<VmDebugThread> VmDebugSession::threads() const {
  std::vector<VmDebugThread> out;
  if (!tasks_) {
    out.push_back({1, "main"});
    return out;
  }
  std::vector<vm_detail::VmSteppedTaskView> live;
  tasks_->scheduler.listLive(live);
  for (const vm_detail::VmSteppedTaskView &task : live) {
    VmDebugThread thread;
    thread.id = task.number + 1;
    if (task.number == 0) {
      thread.name = "main";
    } else {
      thread.name = "task " + std::to_string(task.number);
      if (!task.frames->empty() && task.frames->front().function != nullptr) {
        thread.name += ": " + task.frames->front().function->name;
      }
    }
    out.push_back(std::move(thread));
  }
  return out;
}

bool VmDebugSession::threadSnapshotPayload(uint64_t threadId, VmDebugSnapshotPayload &out) const {
  if (!tasks_) {
    if (threadId != 1) {
      return false;
    }
    out = snapshotPayload();
    return true;
  }
  std::vector<vm_detail::VmSteppedTaskView> live;
  tasks_->scheduler.listLive(live);
  for (const vm_detail::VmSteppedTaskView &task : live) {
    if (task.number + 1 == threadId) {
      out = payloadOf(*task.frames, *task.stack);
      return true;
    }
  }
  return false;
}

} // namespace primec
