#include "primec/VmDebugAdapter.h"

#include <string>
#include <unordered_map>

namespace primec {
namespace {

std::string jsonEscape(std::string_view text) {
  std::string out;
  out.reserve(text.size() + 8);
  for (const char c : text) {
    switch (c) {
      case '\"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\b':
        out += "\\b";
        break;
      case '\f':
        out += "\\f";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        if (static_cast<unsigned char>(c) < 0x20) {
          out += "\\u00";
          static constexpr char Hex[] = "0123456789abcdef";
          out.push_back(Hex[(static_cast<unsigned char>(c) >> 4) & 0x0f]);
          out.push_back(Hex[static_cast<unsigned char>(c) & 0x0f]);
        } else {
          out.push_back(c);
        }
        break;
    }
  }
  return out;
}

size_t resolveLocalVariableCount(const IrFunction &function, const VmDebugSnapshotPayload &payload, size_t frameOrdinal) {
  size_t slotCount = 0;
  for (const IrLocalDebugSlot &slot : function.localDebugSlots) {
    const size_t nextCount = static_cast<size_t>(slot.slotIndex) + 1;
    if (nextCount > slotCount) {
      slotCount = nextCount;
    }
  }
  if (frameOrdinal < payload.callStack.size()) {
    const size_t payloadIndex = payload.callStack.size() - frameOrdinal - 1;
    if (payloadIndex < payload.frameLocals.size() && payload.frameLocals[payloadIndex].size() > slotCount) {
      slotCount = payload.frameLocals[payloadIndex].size();
    } else if (frameOrdinal == 0 && payload.currentFrameLocals.size() > slotCount) {
      slotCount = payload.currentFrameLocals.size();
    }
  }
  return slotCount;
}

const std::vector<uint64_t> *frameLocalsForOrdinal(const VmDebugSnapshotPayload &payload, size_t frameOrdinal) {
  if (frameOrdinal >= payload.callStack.size()) {
    return nullptr;
  }
  const size_t payloadIndex = payload.callStack.size() - frameOrdinal - 1;
  if (payloadIndex < payload.frameLocals.size()) {
    return &payload.frameLocals[payloadIndex];
  }
  if (frameOrdinal == 0) {
    return &payload.currentFrameLocals;
  }
  return nullptr;
}

} // namespace

bool VmDebugAdapter::launch(const IrModule &module, std::string &error, uint64_t argCount) {
  clearTranscript();
  appendRequest("launch", std::string("\"arg_count\":") + std::to_string(argCount));
  module_ = nullptr;

  error.clear();
  if (!session_.start(module, error, argCount)) {
    appendResponse("launch", false, {}, error);
    return false;
  }

  module_ = &module;
  appendResponse("launch", true);
  transcript_.push_back("{\"type\":\"event\",\"event\":\"initialized\"}");
  transcript_.push_back("{\"type\":\"event\",\"event\":\"stopped\",\"reason\":\"entry\"}");
  return true;
}

bool VmDebugAdapter::launch(const IrModule &module, std::string &error, const std::vector<std::string_view> &args) {
  clearTranscript();
  appendRequest("launch", std::string("\"arg_count\":") + std::to_string(args.size()));
  module_ = nullptr;

  error.clear();
  if (!session_.start(module, error, args)) {
    appendResponse("launch", false, {}, error);
    return false;
  }

  module_ = &module;
  appendResponse("launch", true);
  transcript_.push_back("{\"type\":\"event\",\"event\":\"initialized\"}");
  transcript_.push_back("{\"type\":\"event\",\"event\":\"stopped\",\"reason\":\"entry\"}");
  return true;
}

bool VmDebugAdapter::continueExecution(VmDebugAdapterStopEvent &stopEvent, std::string &error) {
  appendRequest("continue");
  if (!isLaunched(error)) {
    appendResponse("continue", false, {}, error);
    return false;
  }

  VmDebugStopReason reason = VmDebugStopReason::Step;
  std::string vmError;
  const bool ok = session_.continueExecution(reason, vmError);
  if (!ok && reason != VmDebugStopReason::Fault) {
    error = vmError;
    appendResponse("continue", false, {}, error);
    return false;
  }

  stopEvent.reason = reason;
  stopEvent.dapReason = dapReason(reason);
  stopEvent.message = reason == VmDebugStopReason::Fault ? vmError : "";
  appendStoppedEvent(stopEvent);
  appendResponse("continue", true, std::string("\"reason\":\"") + jsonEscape(stopEvent.dapReason) + "\"");
  error.clear();
  return true;
}

bool VmDebugAdapter::step(VmDebugAdapterStopEvent &stopEvent, std::string &error) {
  appendRequest("next");
  if (!isLaunched(error)) {
    appendResponse("next", false, {}, error);
    return false;
  }

  VmDebugStopReason reason = VmDebugStopReason::Step;
  std::string vmError;
  const bool ok = session_.step(reason, vmError);
  if (!ok && reason != VmDebugStopReason::Fault) {
    error = vmError;
    appendResponse("next", false, {}, error);
    return false;
  }

  stopEvent.reason = reason;
  stopEvent.dapReason = dapReason(reason);
  stopEvent.message = reason == VmDebugStopReason::Fault ? vmError : "";
  appendStoppedEvent(stopEvent);
  appendResponse("next", true, std::string("\"reason\":\"") + jsonEscape(stopEvent.dapReason) + "\"");
  error.clear();
  return true;
}

bool VmDebugAdapter::pause(std::string &error) {
  appendRequest("pause");
  if (!isLaunched(error)) {
    appendResponse("pause", false, {}, error);
    return false;
  }
  if (!session_.pause(error)) {
    appendResponse("pause", false, {}, error);
    return false;
  }
  appendResponse("pause", true);
  return true;
}

bool VmDebugAdapter::setInstructionBreakpoints(const std::vector<std::pair<size_t, size_t>> &breakpoints, std::string &error) {
  appendRequest("setInstructionBreakpoints", std::string("\"count\":") + std::to_string(breakpoints.size()));
  if (!isLaunched(error)) {
    appendResponse("setInstructionBreakpoints", false, {}, error);
    return false;
  }

  session_.clearBreakpoints();
  size_t verifiedCount = 0;
  for (const auto &breakpoint : breakpoints) {
    std::string localError;
    if (!session_.addBreakpoint(breakpoint.first, breakpoint.second, localError)) {
      error = localError;
      appendResponse("setInstructionBreakpoints", false, {}, error);
      return false;
    }
    ++verifiedCount;
  }

  appendResponse("setInstructionBreakpoints",
                 true,
                 std::string("\"verified_count\":") + std::to_string(verifiedCount));
  error.clear();
  return true;
}

bool VmDebugAdapter::setSourceBreakpoints(const std::vector<VmDebugAdapterSourceBreakpoint> &breakpoints,
                                          std::vector<VmDebugAdapterBreakpointResult> &results,
                                          std::string &error) {
  appendRequest("setBreakpoints", std::string("\"count\":") + std::to_string(breakpoints.size()));
  if (!isLaunched(error)) {
    appendResponse("setBreakpoints", false, {}, error);
    return false;
  }

  results.clear();
  results.reserve(breakpoints.size());
  session_.clearBreakpoints();

  size_t verifiedCount = 0;
  for (const VmDebugAdapterSourceBreakpoint &breakpoint : breakpoints) {
    VmDebugAdapterBreakpointResult result;
    result.line = breakpoint.line;
    result.column = breakpoint.column.value_or(0);
    std::string localError;
    size_t resolvedCount = 0;
    if (session_.addSourceBreakpoint(breakpoint.line, breakpoint.column, resolvedCount, localError)) {
      result.verified = true;
      result.resolvedCount = resolvedCount;
      ++verifiedCount;
    } else {
      result.verified = false;
      result.message = localError;
    }
    results.push_back(std::move(result));
  }

  appendResponse("setBreakpoints",
                 true,
                 std::string("\"verified_count\":") + std::to_string(verifiedCount));
  error.clear();
  return true;
}

bool VmDebugAdapter::threads(std::vector<VmDebugAdapterThread> &outThreads, std::string &error) {
  appendRequest("threads");
  if (!isLaunched(error)) {
    appendResponse("threads", false, {}, error);
    return false;
  }

  outThreads = {{1, "main"}};
  appendResponse("threads", true, "\"count\":1");
  error.clear();
  return true;
}

bool VmDebugAdapter::stackTrace(int64_t threadId, std::vector<VmDebugAdapterStackFrame> &outFrames, std::string &error) {
  appendRequest("stackTrace", std::string("\"thread_id\":") + std::to_string(threadId));
  if (!isLaunched(error)) {
    appendResponse("stackTrace", false, {}, error);
    return false;
  }
  if (threadId != 1) {
    error = "invalid thread id";
    appendResponse("stackTrace", false, {}, error);
    return false;
  }

  const VmDebugSnapshotPayload payload = session_.snapshotPayload();
  outFrames.clear();
  outFrames.reserve(payload.callStack.size());
  for (size_t reverseIndex = 0; reverseIndex < payload.callStack.size(); ++reverseIndex) {
    const VmDebugStackFrameSnapshot &frameSnapshot = payload.callStack[payload.callStack.size() - reverseIndex - 1];
    if (frameSnapshot.functionIndex >= module_->functions.size()) {
      error = "debug adapter observed invalid function index in stack";
      appendResponse("stackTrace", false, {}, error);
      return false;
    }

    const IrFunction &function = module_->functions[frameSnapshot.functionIndex];
    size_t mappedIp = frameSnapshot.instructionPointer;
    if (reverseIndex > 0 && mappedIp > 0 && mappedIp <= function.instructions.size()) {
      const IrInstruction &callSite = function.instructions[mappedIp - 1];
      if (callSite.op == IrOpcode::Call || callSite.op == IrOpcode::CallVoid) {
        mappedIp -= 1;
      }
    }

    VmDebugAdapterStackFrame frame;
    frame.id = static_cast<int64_t>(reverseIndex + 1);
    frame.functionIndex = frameSnapshot.functionIndex;
    frame.instructionPointer = mappedIp;
    frame.name = function.name;
    if (mappedIp < function.instructions.size()) {
      const IrInstruction &instruction = function.instructions[mappedIp];
      frame.debugId = instruction.debugId;
      if (const IrInstructionSourceMapEntry *entry = lookupSourceMap(instruction.debugId); entry != nullptr) {
        frame.line = entry->line;
        frame.column = entry->column;
        frame.provenance = entry->provenance;
      }
    }
    outFrames.push_back(std::move(frame));
  }

  appendResponse("stackTrace", true, std::string("\"frame_count\":") + std::to_string(outFrames.size()));
  error.clear();
  return true;
}

bool VmDebugAdapter::scopes(int64_t frameId, std::vector<VmDebugAdapterScope> &outScopes, std::string &error) {
  appendRequest("scopes", std::string("\"frame_id\":") + std::to_string(frameId));
  if (!isLaunched(error)) {
    appendResponse("scopes", false, {}, error);
    return false;
  }
  if (frameId <= 0) {
    error = "invalid frame id";
    appendResponse("scopes", false, {}, error);
    return false;
  }

  const VmDebugSnapshotPayload payload = session_.snapshotPayload();
  const size_t frameOrdinal = static_cast<size_t>(frameId - 1);
  if (frameOrdinal >= payload.callStack.size()) {
    error = "invalid frame id";
    appendResponse("scopes", false, {}, error);
    return false;
  }

  const VmDebugStackFrameSnapshot &frameSnapshot = payload.callStack[payload.callStack.size() - frameOrdinal - 1];
  if (frameSnapshot.functionIndex >= module_->functions.size()) {
    error = "debug adapter observed invalid function index in scope query";
    appendResponse("scopes", false, {}, error);
    return false;
  }

  const IrFunction &function = module_->functions[frameSnapshot.functionIndex];
  const size_t localCount = resolveLocalVariableCount(function, payload, frameOrdinal);
  const size_t operandCount = frameOrdinal == 0 ? payload.operandStack.size() : 0;

  outScopes.clear();
  outScopes.push_back(
      {encodeVariablesReference(frameOrdinal, ScopeKind::Locals), "Locals", localCount, false});
  outScopes.push_back(
      {encodeVariablesReference(frameOrdinal, ScopeKind::OperandStack), "Operand Stack", operandCount, false});

  appendResponse("scopes", true, std::string("\"scope_count\":") + std::to_string(outScopes.size()));
  error.clear();
  return true;
}

bool VmDebugAdapter::variables(int64_t variablesReference,
                               std::vector<VmDebugAdapterVariable> &outVariables,
                               std::string &error) {
  appendRequest("variables", std::string("\"reference\":") + std::to_string(variablesReference));
  if (!isLaunched(error)) {
    appendResponse("variables", false, {}, error);
    return false;
  }

  size_t frameOrdinal = 0;
  ScopeKind scopeKind = ScopeKind::Locals;
  if (!decodeVariablesReference(variablesReference, frameOrdinal, scopeKind)) {
    error = "invalid variable reference";
    appendResponse("variables", false, {}, error);
    return false;
  }

  const VmDebugSnapshotPayload payload = session_.snapshotPayload();
  if (frameOrdinal >= payload.callStack.size()) {
    error = "invalid variable reference";
    appendResponse("variables", false, {}, error);
    return false;
  }

  const VmDebugStackFrameSnapshot &frameSnapshot = payload.callStack[payload.callStack.size() - frameOrdinal - 1];
  if (frameSnapshot.functionIndex >= module_->functions.size()) {
    error = "debug adapter observed invalid function index in variable query";
    appendResponse("variables", false, {}, error);
    return false;
  }

  const IrFunction &function = module_->functions[frameSnapshot.functionIndex];
  outVariables.clear();
  if (scopeKind == ScopeKind::Locals) {
    std::unordered_map<size_t, const IrLocalDebugSlot *> bySlotIndex;
    bySlotIndex.reserve(function.localDebugSlots.size());
    for (const IrLocalDebugSlot &slot : function.localDebugSlots) {
      bySlotIndex.emplace(slot.slotIndex, &slot);
    }

    const size_t slotCount = resolveLocalVariableCount(function, payload, frameOrdinal);
    const std::vector<uint64_t> *frameLocals = frameLocalsForOrdinal(payload, frameOrdinal);
    outVariables.reserve(slotCount);
    for (size_t slotIndex = 0; slotIndex < slotCount; ++slotIndex) {
      VmDebugAdapterVariable variable;
      const auto debugIt = bySlotIndex.find(slotIndex);
      if (debugIt != bySlotIndex.end() && !debugIt->second->name.empty()) {
        variable.name = debugIt->second->name;
      } else {
        variable.name = "slot" + std::to_string(slotIndex);
      }
      if (debugIt != bySlotIndex.end() && !debugIt->second->typeName.empty()) {
        variable.typeName = debugIt->second->typeName;
      } else {
        variable.typeName = "u64";
      }

      if (frameLocals != nullptr && slotIndex < frameLocals->size()) {
        variable.value = std::to_string((*frameLocals)[slotIndex]);
      } else {
        variable.value = "<unavailable>";
      }
      outVariables.push_back(std::move(variable));
    }
  } else {
    if (frameOrdinal == 0) {
      outVariables.reserve(payload.operandStack.size());
      for (size_t i = 0; i < payload.operandStack.size(); ++i) {
        const size_t stackIndex = payload.operandStack.size() - i - 1;
        VmDebugAdapterVariable variable;
        variable.name = "stack[" + std::to_string(i) + "]";
        variable.typeName = "u64";
        variable.value = std::to_string(payload.operandStack[stackIndex]);
        outVariables.push_back(std::move(variable));
      }
    }
  }

  appendResponse("variables", true, std::string("\"variable_count\":") + std::to_string(outVariables.size()));
  error.clear();
  return true;
}

VmDebugSnapshot VmDebugAdapter::snapshot() const {
  return session_.snapshot();
}

const std::vector<std::string> &VmDebugAdapter::transcript() const {
  return transcript_;
}

void VmDebugAdapter::clearTranscript() {
  transcript_.clear();
}

bool VmDebugAdapter::isLaunched(std::string &error) const {
  if (module_ == nullptr) {
    error = "debug adapter session is not started";
    return false;
  }
  return true;
}

std::string VmDebugAdapter::dapReason(VmDebugStopReason reason) const {
  switch (reason) {
    case VmDebugStopReason::Breakpoint:
      return "breakpoint";
    case VmDebugStopReason::Step:
      return "step";
    case VmDebugStopReason::Pause:
      return "pause";
    case VmDebugStopReason::Fault:
      return "exception";
    case VmDebugStopReason::Exit:
      return "exited";
  }
  return "unknown";
}

void VmDebugAdapter::appendRequest(std::string_view command, std::string_view payload) {
  std::string line = std::string("{\"type\":\"request\",\"command\":\"") + jsonEscape(command) + "\"";
  if (!payload.empty()) {
    line += "," + std::string(payload);
  }
  line += "}";
  transcript_.push_back(std::move(line));
}

void VmDebugAdapter::appendResponse(std::string_view command,
                                    bool success,
                                    std::string_view payload,
                                    std::string_view message) {
  std::string line = std::string("{\"type\":\"response\",\"command\":\"") + jsonEscape(command) + "\",\"success\":" +
                     (success ? "true" : "false");
  if (!payload.empty()) {
    line += "," + std::string(payload);
  }
  if (!message.empty()) {
    line += ",\"message\":\"" + jsonEscape(message) + "\"";
  }
  line += "}";
  transcript_.push_back(std::move(line));
}

void VmDebugAdapter::appendStoppedEvent(const VmDebugAdapterStopEvent &stopEvent) {
  std::string line = std::string("{\"type\":\"event\",\"event\":\"stopped\",\"reason\":\"") +
                     jsonEscape(stopEvent.dapReason) + "\",\"vm_reason\":\"" +
                     jsonEscape(vmDebugStopReasonName(stopEvent.reason)) + "\"";
  if (!stopEvent.message.empty()) {
    line += ",\"message\":\"" + jsonEscape(stopEvent.message) + "\"";
  }
  line += "}";
  transcript_.push_back(std::move(line));
}

bool VmDebugAdapter::decodeVariablesReference(int64_t variablesReference,
                                              size_t &frameOrdinal,
                                              ScopeKind &scopeKind) const {
  if (variablesReference <= 0) {
    return false;
  }
  const int64_t kindCode = variablesReference & 0xff;
  const int64_t frameCode = variablesReference >> 8;
  if (frameCode <= 0) {
    return false;
  }

  if (kindCode == static_cast<int64_t>(ScopeKind::Locals)) {
    scopeKind = ScopeKind::Locals;
  } else if (kindCode == static_cast<int64_t>(ScopeKind::OperandStack)) {
    scopeKind = ScopeKind::OperandStack;
  } else {
    return false;
  }
  frameOrdinal = static_cast<size_t>(frameCode - 1);
  return true;
}

int64_t VmDebugAdapter::encodeVariablesReference(size_t frameOrdinal, ScopeKind scopeKind) const {
  const uint64_t frameCode = static_cast<uint64_t>(frameOrdinal + 1);
  return static_cast<int64_t>((frameCode << 8u) | static_cast<uint64_t>(scopeKind));
}

const IrInstructionSourceMapEntry *VmDebugAdapter::lookupSourceMap(uint32_t debugId) const {
  if (module_ == nullptr || debugId == 0) {
    return nullptr;
  }
  for (const IrInstructionSourceMapEntry &entry : module_->instructionSourceMap) {
    if (entry.debugId == debugId) {
      return &entry;
    }
  }
  return nullptr;
}

} // namespace primec
