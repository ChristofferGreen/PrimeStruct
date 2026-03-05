#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "primec/Ir.h"
#include "primec/Vm.h"

namespace primec {

struct VmDebugAdapterThread {
  int64_t id = 1;
  std::string name = "main";
};

struct VmDebugAdapterStackFrame {
  int64_t id = 0;
  size_t functionIndex = 0;
  size_t instructionPointer = 0;
  uint32_t debugId = 0;
  std::string name;
  uint32_t line = 0;
  uint32_t column = 0;
  IrSourceMapProvenance provenance = IrSourceMapProvenance::Unknown;
};

struct VmDebugAdapterScope {
  int64_t variablesReference = 0;
  std::string name;
  size_t namedVariables = 0;
  bool expensive = false;
};

struct VmDebugAdapterVariable {
  std::string name;
  std::string value;
  std::string typeName;
  int64_t variablesReference = 0;
};

struct VmDebugAdapterSourceBreakpoint {
  uint32_t line = 0;
  std::optional<uint32_t> column;
};

struct VmDebugAdapterBreakpointResult {
  bool verified = false;
  uint32_t line = 0;
  uint32_t column = 0;
  size_t resolvedCount = 0;
  std::string message;
};

struct VmDebugAdapterStopEvent {
  VmDebugStopReason reason = VmDebugStopReason::Step;
  std::string dapReason;
  std::string message;
};

class VmDebugAdapter {
public:
  bool launch(const IrModule &module, std::string &error, uint64_t argCount = 0);
  bool launch(const IrModule &module, std::string &error, const std::vector<std::string_view> &args);

  bool continueExecution(VmDebugAdapterStopEvent &stopEvent, std::string &error);
  bool step(VmDebugAdapterStopEvent &stopEvent, std::string &error);
  bool pause(std::string &error);

  bool setInstructionBreakpoints(const std::vector<std::pair<size_t, size_t>> &breakpoints, std::string &error);
  bool setSourceBreakpoints(const std::vector<VmDebugAdapterSourceBreakpoint> &breakpoints,
                            std::vector<VmDebugAdapterBreakpointResult> &results,
                            std::string &error);

  bool threads(std::vector<VmDebugAdapterThread> &outThreads, std::string &error);
  bool stackTrace(int64_t threadId, std::vector<VmDebugAdapterStackFrame> &outFrames, std::string &error);
  bool scopes(int64_t frameId, std::vector<VmDebugAdapterScope> &outScopes, std::string &error);
  bool variables(int64_t variablesReference, std::vector<VmDebugAdapterVariable> &outVariables, std::string &error);

  VmDebugSnapshot snapshot() const;
  const std::vector<std::string> &transcript() const;
  void clearTranscript();

private:
  enum class ScopeKind : uint8_t {
    Locals = 1,
    OperandStack = 2,
  };

  bool isLaunched(std::string &error) const;
  std::string dapReason(VmDebugStopReason reason) const;
  void appendRequest(std::string_view command, std::string_view payload = {});
  void appendResponse(std::string_view command,
                      bool success,
                      std::string_view payload = {},
                      std::string_view message = {});
  void appendStoppedEvent(const VmDebugAdapterStopEvent &stopEvent);

  bool decodeVariablesReference(int64_t variablesReference, size_t &frameOrdinal, ScopeKind &scopeKind) const;
  int64_t encodeVariablesReference(size_t frameOrdinal, ScopeKind scopeKind) const;
  const IrInstructionSourceMapEntry *lookupSourceMap(uint32_t debugId) const;

  const IrModule *module_ = nullptr;
  VmDebugSession session_;
  std::vector<std::string> transcript_;
};

} // namespace primec
