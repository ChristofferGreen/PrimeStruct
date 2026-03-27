#include "primec/VmDebugDap.h"

#include "VmDebugDapProtocol.h"
#include "primec/VmDebugAdapter.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace primec {
namespace {
using vm_debug_dap_detail::JsonValue;
using vm_debug_dap_detail::buildEvent;
using vm_debug_dap_detail::buildResponse;
using vm_debug_dap_detail::fileNameFromPath;
using vm_debug_dap_detail::jsonEscape;
using vm_debug_dap_detail::jsonNumberField;
using vm_debug_dap_detail::jsonObjectField;
using vm_debug_dap_detail::jsonStringField;
using vm_debug_dap_detail::parseDapFrame;
using vm_debug_dap_detail::parseInstructionReference;
using vm_debug_dap_detail::writeDapFrame;

struct VmDebugDapRuntime {
  VmDebugAdapter adapter;
  bool launched = false;
  bool disconnected = false;
  bool observedProgramExit = false;
  int programExitCode = 0;
  uint64_t outgoingSeq = 1;
};

void appendResponse(std::vector<std::string> &messages,
                    VmDebugDapRuntime &runtime,
                    int64_t requestSeq,
                    std::string_view command,
                    bool success,
                    std::string_view body = {},
                    std::string_view message = {}) {
  messages.push_back(
      buildResponse(runtime.outgoingSeq++, requestSeq, command, success, body, message));
}

void appendEvent(std::vector<std::string> &messages,
                 VmDebugDapRuntime &runtime,
                 std::string_view event,
                 std::string_view body = {}) {
  messages.push_back(buildEvent(runtime.outgoingSeq++, event, body));
}

bool ensureLaunched(const VmDebugDapRuntime &runtime, std::string &error) {
  if (runtime.launched) {
    return true;
  }
  error = "debug adapter session is not launched";
  return false;
}

std::string stoppedBody(std::string_view reason, std::string_view text = {}) {
  std::string body = std::string("{\"reason\":\"") + jsonEscape(reason) + "\",\"threadId\":1,\"allThreadsStopped\":true";
  if (!text.empty()) {
    body += ",\"text\":\"" + jsonEscape(text) + "\"";
  }
  body += "}";
  return body;
}

bool parseSourceBreakpoints(const JsonValue *arguments,
                            std::vector<VmDebugAdapterSourceBreakpoint> &breakpoints,
                            std::string &error) {
  breakpoints.clear();
  if (arguments == nullptr) {
    return true;
  }
  const JsonValue *list = jsonObjectField(arguments, "breakpoints");
  if (list == nullptr) {
    return true;
  }
  if (list->kind != JsonValue::Kind::Array) {
    error = "setBreakpoints arguments.breakpoints must be an array";
    return false;
  }
  for (const JsonValue &entry : list->arrayValue) {
    if (entry.kind != JsonValue::Kind::Object) {
      error = "setBreakpoints entries must be objects";
      return false;
    }
    int64_t line = 0;
    if (!jsonNumberField(entry, "line", line) || line <= 0) {
      error = "setBreakpoints entries require positive line";
      return false;
    }
    VmDebugAdapterSourceBreakpoint breakpoint;
    breakpoint.line = static_cast<uint32_t>(line);
    int64_t column = 0;
    if (jsonNumberField(entry, "column", column) && column > 0) {
      breakpoint.column = static_cast<uint32_t>(column);
    }
    breakpoints.push_back(breakpoint);
  }
  return true;
}

bool parseInstructionBreakpoints(const JsonValue *arguments,
                                 std::vector<std::pair<size_t, size_t>> &breakpoints,
                                 std::string &error) {
  breakpoints.clear();
  if (arguments == nullptr) {
    return true;
  }
  const JsonValue *list = jsonObjectField(arguments, "breakpoints");
  if (list == nullptr) {
    list = jsonObjectField(arguments, "instructionBreakpoints");
  }
  if (list == nullptr) {
    return true;
  }
  if (list->kind != JsonValue::Kind::Array) {
    error = "setInstructionBreakpoints breakpoints must be an array";
    return false;
  }

  for (const JsonValue &entry : list->arrayValue) {
    if (entry.kind != JsonValue::Kind::Object) {
      error = "setInstructionBreakpoints entries must be objects";
      return false;
    }
    int64_t functionIndex = -1;
    int64_t instructionPointer = -1;
    if (jsonNumberField(entry, "functionIndex", functionIndex) &&
        jsonNumberField(entry, "instructionPointer", instructionPointer)) {
      if (functionIndex < 0 || instructionPointer < 0) {
        error = "setInstructionBreakpoints functionIndex/instructionPointer must be non-negative";
        return false;
      }
      breakpoints.push_back({static_cast<size_t>(functionIndex),
                             static_cast<size_t>(instructionPointer)});
      continue;
    }

    std::string instructionReference;
    if (!jsonStringField(entry, "instructionReference", instructionReference)) {
      error = "setInstructionBreakpoints entry requires functionIndex/instructionPointer or instructionReference";
      return false;
    }
    std::optional<std::pair<size_t, size_t>> parsed = parseInstructionReference(instructionReference);
    if (!parsed.has_value()) {
      error = "invalid instructionReference format (expected f<function>:ip<instruction>)";
      return false;
    }
    int64_t offset = 0;
    if (jsonNumberField(entry, "offset", offset)) {
      if (offset < 0) {
        error = "instruction breakpoint offset must be non-negative";
        return false;
      }
      parsed->second += static_cast<size_t>(offset);
    }
    breakpoints.push_back(*parsed);
  }
  return true;
}

std::string encodeSetBreakpointsBody(const std::vector<VmDebugAdapterBreakpointResult> &results) {
  std::string body = "{\"breakpoints\":[";
  for (size_t i = 0; i < results.size(); ++i) {
    if (i > 0) {
      body += ",";
    }
    const auto &result = results[i];
    body += std::string("{\"verified\":") + (result.verified ? "true" : "false") + ",\"line\":" +
            std::to_string(result.line) + ",\"column\":" + std::to_string(result.column);
    if (!result.message.empty()) {
      body += ",\"message\":\"" + jsonEscape(result.message) + "\"";
    }
    body += "}";
  }
  body += "]}";
  return body;
}

std::string encodeInstructionBreakpointsBody(const std::vector<std::pair<size_t, size_t>> &breakpoints) {
  std::string body = "{\"breakpoints\":[";
  for (size_t i = 0; i < breakpoints.size(); ++i) {
    if (i > 0) {
      body += ",";
    }
    const auto &bp = breakpoints[i];
    body += std::string("{\"verified\":true,\"instructionReference\":\"f") + std::to_string(bp.first) + ":ip" +
            std::to_string(bp.second) + "\"}";
  }
  body += "]}";
  return body;
}

std::string encodeThreadsBody(const std::vector<VmDebugAdapterThread> &threads) {
  std::string body = "{\"threads\":[";
  for (size_t i = 0; i < threads.size(); ++i) {
    if (i > 0) {
      body += ",";
    }
    body += std::string("{\"id\":") + std::to_string(threads[i].id) + ",\"name\":\"" +
            jsonEscape(threads[i].name) + "\"}";
  }
  body += "]}";
  return body;
}

std::string encodeStackTraceBody(const std::vector<VmDebugAdapterStackFrame> &frames, std::string_view sourcePath) {
  const std::string sourceName = fileNameFromPath(sourcePath);
  std::string body = "{\"stackFrames\":[";
  for (size_t i = 0; i < frames.size(); ++i) {
    if (i > 0) {
      body += ",";
    }
    const auto &frame = frames[i];
    body += std::string("{\"id\":") + std::to_string(frame.id) + ",\"name\":\"" + jsonEscape(frame.name) +
            "\",\"line\":" + std::to_string(frame.line) + ",\"column\":" + std::to_string(frame.column) +
            ",\"instructionPointer\":" + std::to_string(frame.instructionPointer) + ",\"source\":{\"name\":\"" +
            jsonEscape(sourceName) + "\",\"path\":\"" + jsonEscape(sourcePath) + "\"}}";
  }
  body += "],\"totalFrames\":" + std::to_string(frames.size()) + "}";
  return body;
}

std::string encodeScopesBody(const std::vector<VmDebugAdapterScope> &scopes) {
  std::string body = "{\"scopes\":[";
  for (size_t i = 0; i < scopes.size(); ++i) {
    if (i > 0) {
      body += ",";
    }
    const auto &scope = scopes[i];
    body += std::string("{\"name\":\"") + jsonEscape(scope.name) + "\",\"variablesReference\":" +
            std::to_string(scope.variablesReference) + ",\"namedVariables\":" +
            std::to_string(scope.namedVariables) + ",\"expensive\":" +
            (scope.expensive ? "true" : "false") + "}";
  }
  body += "]}";
  return body;
}

std::string encodeVariablesBody(const std::vector<VmDebugAdapterVariable> &variables) {
  std::string body = "{\"variables\":[";
  for (size_t i = 0; i < variables.size(); ++i) {
    if (i > 0) {
      body += ",";
    }
    const auto &variable = variables[i];
    body += std::string("{\"name\":\"") + jsonEscape(variable.name) + "\",\"value\":\"" + jsonEscape(variable.value) +
            "\",\"type\":\"" + jsonEscape(variable.typeName) + "\",\"variablesReference\":" +
            std::to_string(variable.variablesReference) + "}";
  }
  body += "]}";
  return body;
}

void appendStopEvents(std::vector<std::string> &messages,
                      VmDebugDapRuntime &runtime,
                      const VmDebugAdapterStopEvent &stopEvent) {
  if (stopEvent.reason == VmDebugStopReason::Exit) {
    runtime.observedProgramExit = true;
    runtime.programExitCode = static_cast<int>(static_cast<int32_t>(runtime.adapter.snapshot().result));
    appendEvent(messages,
                runtime,
                "exited",
                std::string("{\"exitCode\":") + std::to_string(runtime.programExitCode) + "}");
    appendEvent(messages, runtime, "terminated", "{}");
    return;
  }
  appendEvent(messages, runtime, "stopped", stoppedBody(stopEvent.dapReason, stopEvent.message));
}

void appendFailure(std::vector<std::string> &messages,
                   VmDebugDapRuntime &runtime,
                   int64_t requestSeq,
                   std::string_view command,
                   std::string_view message) {
  appendResponse(messages, runtime, requestSeq, command, false, "{}", message);
}

bool handleRequest(const JsonValue &request,
                   const IrModule &module,
                   const std::vector<std::string_view> &args,
                   std::string_view sourcePath,
                   VmDebugDapRuntime &runtime,
                   std::vector<std::string> &messages,
                   std::string &error) {
  messages.clear();
  if (request.kind != JsonValue::Kind::Object) {
    error = "DAP request payload must be a JSON object";
    return false;
  }

  int64_t requestSeq = 0;
  jsonNumberField(request, "seq", requestSeq);
  std::string command;
  if (!jsonStringField(request, "command", command)) {
    appendFailure(messages, runtime, requestSeq, "<unknown>", "missing request command");
    return true;
  }
  const JsonValue *arguments = jsonObjectField(request, "arguments");

  if (command == "initialize") {
    appendResponse(messages,
                   runtime,
                   requestSeq,
                   command,
                   true,
                   "{\"supportsConfigurationDoneRequest\":true,\"supportsInstructionBreakpoints\":true,"
                   "\"supportsSteppingGranularity\":false}",
                   {});
    return true;
  }

  if (command == "launch") {
    if (runtime.launched) {
      appendFailure(messages, runtime, requestSeq, command, "debug adapter session is already launched");
      return true;
    }
    std::string launchError;
    if (!runtime.adapter.launch(module, launchError, args)) {
      appendFailure(messages, runtime, requestSeq, command, launchError);
      return true;
    }
    runtime.launched = true;
    appendResponse(messages, runtime, requestSeq, command, true, "{}", {});
    appendEvent(messages, runtime, "initialized", "{}");
    appendEvent(messages, runtime, "stopped", stoppedBody("entry"));
    return true;
  }

  if (command == "configurationDone") {
    appendResponse(messages, runtime, requestSeq, command, true, "{}", {});
    return true;
  }

  if (command == "setExceptionBreakpoints") {
    appendResponse(messages, runtime, requestSeq, command, true, "{\"breakpoints\":[]}", {});
    return true;
  }

  if (command == "disconnect") {
    appendResponse(messages, runtime, requestSeq, command, true, "{}", {});
    runtime.disconnected = true;
    return true;
  }

  if (command == "setBreakpoints") {
    if (!ensureLaunched(runtime, error)) {
      appendFailure(messages, runtime, requestSeq, command, error);
      return true;
    }
    std::vector<VmDebugAdapterSourceBreakpoint> breakpoints;
    if (!parseSourceBreakpoints(arguments, breakpoints, error)) {
      appendFailure(messages, runtime, requestSeq, command, error);
      return true;
    }
    std::vector<VmDebugAdapterBreakpointResult> results;
    if (!runtime.adapter.setSourceBreakpoints(breakpoints, results, error)) {
      appendFailure(messages, runtime, requestSeq, command, error);
      return true;
    }
    appendResponse(messages, runtime, requestSeq, command, true, encodeSetBreakpointsBody(results), {});
    return true;
  }

  if (command == "setInstructionBreakpoints") {
    if (!ensureLaunched(runtime, error)) {
      appendFailure(messages, runtime, requestSeq, command, error);
      return true;
    }
    std::vector<std::pair<size_t, size_t>> breakpoints;
    if (!parseInstructionBreakpoints(arguments, breakpoints, error)) {
      appendFailure(messages, runtime, requestSeq, command, error);
      return true;
    }
    if (!runtime.adapter.setInstructionBreakpoints(breakpoints, error)) {
      appendFailure(messages, runtime, requestSeq, command, error);
      return true;
    }
    appendResponse(messages,
                   runtime,
                   requestSeq,
                   command,
                   true,
                   encodeInstructionBreakpointsBody(breakpoints),
                   {});
    return true;
  }

  if (command == "threads") {
    if (!ensureLaunched(runtime, error)) {
      appendFailure(messages, runtime, requestSeq, command, error);
      return true;
    }
    std::vector<VmDebugAdapterThread> threads;
    if (!runtime.adapter.threads(threads, error)) {
      appendFailure(messages, runtime, requestSeq, command, error);
      return true;
    }
    appendResponse(messages, runtime, requestSeq, command, true, encodeThreadsBody(threads), {});
    return true;
  }

  if (command == "stackTrace") {
    if (!ensureLaunched(runtime, error)) {
      appendFailure(messages, runtime, requestSeq, command, error);
      return true;
    }
    int64_t threadId = 1;
    jsonNumberField(arguments, "threadId", threadId);
    std::vector<VmDebugAdapterStackFrame> frames;
    if (!runtime.adapter.stackTrace(threadId, frames, error)) {
      appendFailure(messages, runtime, requestSeq, command, error);
      return true;
    }
    appendResponse(messages, runtime, requestSeq, command, true, encodeStackTraceBody(frames, sourcePath), {});
    return true;
  }

  if (command == "scopes") {
    if (!ensureLaunched(runtime, error)) {
      appendFailure(messages, runtime, requestSeq, command, error);
      return true;
    }
    int64_t frameId = 0;
    if (!jsonNumberField(arguments, "frameId", frameId)) {
      appendFailure(messages, runtime, requestSeq, command, "scopes requires arguments.frameId");
      return true;
    }
    std::vector<VmDebugAdapterScope> scopes;
    if (!runtime.adapter.scopes(frameId, scopes, error)) {
      appendFailure(messages, runtime, requestSeq, command, error);
      return true;
    }
    appendResponse(messages, runtime, requestSeq, command, true, encodeScopesBody(scopes), {});
    return true;
  }

  if (command == "variables") {
    if (!ensureLaunched(runtime, error)) {
      appendFailure(messages, runtime, requestSeq, command, error);
      return true;
    }
    int64_t variablesReference = 0;
    if (!jsonNumberField(arguments, "variablesReference", variablesReference)) {
      appendFailure(messages, runtime, requestSeq, command, "variables requires arguments.variablesReference");
      return true;
    }
    std::vector<VmDebugAdapterVariable> variables;
    if (!runtime.adapter.variables(variablesReference, variables, error)) {
      appendFailure(messages, runtime, requestSeq, command, error);
      return true;
    }
    appendResponse(messages, runtime, requestSeq, command, true, encodeVariablesBody(variables), {});
    return true;
  }

  if (command == "continue") {
    if (!ensureLaunched(runtime, error)) {
      appendFailure(messages, runtime, requestSeq, command, error);
      return true;
    }
    VmDebugAdapterStopEvent stopEvent;
    if (!runtime.adapter.continueExecution(stopEvent, error)) {
      appendFailure(messages, runtime, requestSeq, command, error);
      return true;
    }
    appendResponse(messages, runtime, requestSeq, command, true, "{\"allThreadsContinued\":true}", {});
    appendStopEvents(messages, runtime, stopEvent);
    return true;
  }

  if (command == "next") {
    if (!ensureLaunched(runtime, error)) {
      appendFailure(messages, runtime, requestSeq, command, error);
      return true;
    }
    VmDebugAdapterStopEvent stopEvent;
    if (!runtime.adapter.step(stopEvent, error)) {
      appendFailure(messages, runtime, requestSeq, command, error);
      return true;
    }
    appendResponse(messages, runtime, requestSeq, command, true, "{}", {});
    appendStopEvents(messages, runtime, stopEvent);
    return true;
  }

  if (command == "pause") {
    if (!ensureLaunched(runtime, error)) {
      appendFailure(messages, runtime, requestSeq, command, error);
      return true;
    }
    if (!runtime.adapter.pause(error)) {
      appendFailure(messages, runtime, requestSeq, command, error);
      return true;
    }
    appendResponse(messages, runtime, requestSeq, command, true, "{}", {});
    return true;
  }

  appendFailure(messages, runtime, requestSeq, command, "unsupported request command: " + command);
  return true;
}

} // namespace

bool runVmDebugDapSession(const IrModule &module,
                          const std::vector<std::string_view> &args,
                          std::string_view sourcePath,
                          std::istream &input,
                          std::ostream &output,
                          VmDebugDapRunResult &result,
                          std::string &error) {
  result = {};
  VmDebugDapRuntime runtime;

  while (!runtime.disconnected) {
    std::string payload;
    bool eof = false;
    if (!parseDapFrame(input, payload, eof, error)) {
      return false;
    }
    if (eof) {
      break;
    }

    JsonValue request;
    if (!vm_debug_dap_detail::parseJsonPayload(payload, request, error)) {
      return false;
    }

    std::vector<std::string> responses;
    if (!handleRequest(request, module, args, sourcePath, runtime, responses, error)) {
      return false;
    }
    for (const std::string &response : responses) {
      writeDapFrame(output, response);
    }
  }

  result.disconnected = runtime.disconnected;
  result.observedProgramExit = runtime.observedProgramExit;
  result.programExitCode = runtime.programExitCode;
  return true;
}

} // namespace primec
