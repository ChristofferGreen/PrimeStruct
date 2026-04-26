#include "primec/CliDriver.h"
#include "primec/CompilePipeline.h"
#include "primec/Diagnostics.h"
#include "primec/IrBackendProfiles.h"
#include "primec/IrPreparation.h"
#include "primec/Options.h"
#include "primec/OptionsParser.h"
#include "primec/Vm.h"
#include "primec/VmDebugDap.h"
#include "VmDebugDapProtocol.h"

#include <cctype>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

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

std::string encodeDebugSnapshotJson(const primec::VmDebugSnapshot &snapshot) {
  return std::string("{\"state\":\"") + std::string(primec::vmDebugSessionStateName(snapshot.state)) +
         "\",\"function_index\":" + std::to_string(snapshot.functionIndex) + ",\"instruction_pointer\":" +
         std::to_string(snapshot.instructionPointer) + ",\"call_depth\":" + std::to_string(snapshot.callDepth) +
         ",\"operand_stack_size\":" + std::to_string(snapshot.operandStackSize) + ",\"result\":" +
         std::to_string(snapshot.result) + "}";
}

std::string encodeUint64ArrayJson(const std::vector<uint64_t> &values) {
  std::string out = "[";
  for (size_t i = 0; i < values.size(); ++i) {
    if (i > 0) {
      out += ",";
    }
    out += std::to_string(values[i]);
  }
  out += "]";
  return out;
}

std::string encodeFrameLocalsJson(const std::vector<std::vector<uint64_t>> &frameLocals) {
  std::string out = "[";
  for (size_t i = 0; i < frameLocals.size(); ++i) {
    if (i > 0) {
      out += ",";
    }
    out += encodeUint64ArrayJson(frameLocals[i]);
  }
  out += "]";
  return out;
}

std::string encodeCallStackJson(const std::vector<primec::VmDebugStackFrameSnapshot> &frames) {
  std::string out = "[";
  for (size_t i = 0; i < frames.size(); ++i) {
    if (i > 0) {
      out += ",";
    }
    out += std::string("{\"function_index\":") + std::to_string(frames[i].functionIndex) +
           ",\"instruction_pointer\":" + std::to_string(frames[i].instructionPointer) + "}";
  }
  out += "]";
  return out;
}

std::string encodeDebugSnapshotPayloadJson(const primec::VmDebugSnapshotPayload &payload) {
  return std::string("{\"instruction_pointer\":") + std::to_string(payload.instructionPointer) + ",\"call_stack\":" +
         encodeCallStackJson(payload.callStack) + ",\"frame_locals\":" + encodeFrameLocalsJson(payload.frameLocals) +
         ",\"current_frame_locals\":" +
         encodeUint64ArrayJson(payload.currentFrameLocals) + ",\"operand_stack\":" +
         encodeUint64ArrayJson(payload.operandStack) + "}";
}

bool shouldIncludeDebugSnapshotPayload(primec::DebugJsonSnapshotMode mode, bool stopEvent) {
  switch (mode) {
    case primec::DebugJsonSnapshotMode::None:
      return false;
    case primec::DebugJsonSnapshotMode::Stop:
      return stopEvent;
    case primec::DebugJsonSnapshotMode::All:
      return true;
  }
  return false;
}

struct DebugJsonEmitContext {
  primec::VmDebugSession *session = nullptr;
  primec::DebugJsonSnapshotMode snapshotMode = primec::DebugJsonSnapshotMode::None;
};

void appendDebugSnapshotPayloadField(std::string &line, const DebugJsonEmitContext *context, bool stopEvent) {
  if (!context || !context->session || !shouldIncludeDebugSnapshotPayload(context->snapshotMode, stopEvent)) {
    return;
  }
  line += ",\"snapshot_payload\":" + encodeDebugSnapshotPayloadJson(context->session->snapshotPayload());
}

void emitDebugJsonLine(const std::string &line) {
  std::cout << line << "\n";
}

bool writeTraceLines(const std::string &path, const std::vector<std::string> &lines, std::string &error) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out.good()) {
    error = "failed to open debug trace output: " + path;
    return false;
  }
  for (const std::string &line : lines) {
    out << line << "\n";
    if (!out.good()) {
      error = "failed to write debug trace output: " + path;
      return false;
    }
  }
  return true;
}

struct TraceCheckpoint {
  uint64_t sequence = 0;
  std::string event;
  std::string reason;
  std::string snapshotJson;
  std::string snapshotPayloadJson;
  bool hasSnapshotResult = false;
  uint64_t snapshotResult = 0;
};

bool readTextFile(const std::string &path, std::string &out, std::string &error) {
  std::ifstream in(path, std::ios::binary);
  if (!in.good()) {
    error = "failed to open replay trace: " + path;
    return false;
  }
  std::ostringstream buffer;
  buffer << in.rdbuf();
  if (!in.good() && !in.eof()) {
    error = "failed to read replay trace: " + path;
    return false;
  }
  out = buffer.str();
  return true;
}

std::string encodeParsedJsonValue(const primec::vm_debug_dap_detail::JsonValue &value) {
  using JsonValue = primec::vm_debug_dap_detail::JsonValue;
  switch (value.kind) {
    case JsonValue::Kind::Null:
      return "null";
    case JsonValue::Kind::Bool:
      return value.boolValue ? "true" : "false";
    case JsonValue::Kind::Number:
      return std::to_string(value.numberValue);
    case JsonValue::Kind::String:
      return std::string("\"") + primec::vm_debug_dap_detail::jsonEscape(value.stringValue) + "\"";
    case JsonValue::Kind::Array: {
      std::string out = "[";
      for (size_t i = 0; i < value.arrayValue.size(); ++i) {
        if (i > 0) {
          out += ",";
        }
        out += encodeParsedJsonValue(value.arrayValue[i]);
      }
      out += "]";
      return out;
    }
    case JsonValue::Kind::Object: {
      std::string out = "{";
      for (size_t i = 0; i < value.objectValue.size(); ++i) {
        if (i > 0) {
          out += ",";
        }
        out += std::string("\"") + primec::vm_debug_dap_detail::jsonEscape(value.objectValue[i].first) + "\":";
        out += encodeParsedJsonValue(value.objectValue[i].second);
      }
      out += "}";
      return out;
    }
  }
  return "null";
}

bool extractJsonUnsignedField(const primec::vm_debug_dap_detail::JsonValue &object,
                              std::string_view key,
                              uint64_t &outValue) {
  int64_t parsedValue = 0;
  if (!primec::vm_debug_dap_detail::jsonNumberField(object, key, parsedValue) || parsedValue < 0) {
    return false;
  }
  outValue = static_cast<uint64_t>(parsedValue);
  return true;
}

bool parseTraceCheckpoints(const std::string &traceText,
                          std::vector<TraceCheckpoint> &outCheckpoints,
                          std::string &error) {
  outCheckpoints.clear();
  uint64_t lastSequence = 0;
  bool haveSequence = false;

  std::stringstream stream(traceText);
  std::string line;
  size_t lineNumber = 0;
  while (std::getline(stream, line)) {
    ++lineNumber;
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    if (line.empty()) {
      continue;
    }

    primec::vm_debug_dap_detail::JsonValue parsedLine;
    if (!primec::vm_debug_dap_detail::parseJsonPayload(line, parsedLine, error)) {
      error = "malformed replay trace JSON on line " + std::to_string(lineNumber) + ": " + error;
      return false;
    }
    if (parsedLine.kind != primec::vm_debug_dap_detail::JsonValue::Kind::Object) {
      error = "malformed replay trace JSON on line " + std::to_string(lineNumber) +
              ": root must be a JSON object";
      return false;
    }

    const primec::vm_debug_dap_detail::JsonValue *eventValue =
        primec::vm_debug_dap_detail::jsonObjectField(parsedLine, "event");
    if (eventValue == nullptr) {
      if (primec::vm_debug_dap_detail::jsonObjectField(parsedLine, "sequence") != nullptr ||
          primec::vm_debug_dap_detail::jsonObjectField(parsedLine, "snapshot") != nullptr ||
          primec::vm_debug_dap_detail::jsonObjectField(parsedLine, "snapshot_payload") != nullptr ||
          primec::vm_debug_dap_detail::jsonObjectField(parsedLine, "reason") != nullptr) {
        error = "malformed replay checkpoint on line " + std::to_string(lineNumber) +
                ": missing string event field";
        return false;
      }
      continue;
    }
    if (eventValue->kind != primec::vm_debug_dap_detail::JsonValue::Kind::String) {
      error = "malformed replay checkpoint on line " + std::to_string(lineNumber) +
              ": missing string event field";
      return false;
    }
    const std::string event = eventValue->stringValue;

    const primec::vm_debug_dap_detail::JsonValue *snapshotValue =
        primec::vm_debug_dap_detail::jsonObjectField(parsedLine, "snapshot");
    if (snapshotValue == nullptr ||
        snapshotValue->kind != primec::vm_debug_dap_detail::JsonValue::Kind::Object) {
      error = "malformed replay checkpoint on line " + std::to_string(lineNumber) +
              ": missing object snapshot field";
      return false;
    }
    const primec::vm_debug_dap_detail::JsonValue *snapshotPayloadValue =
        primec::vm_debug_dap_detail::jsonObjectField(parsedLine, "snapshot_payload");
    if (snapshotPayloadValue == nullptr ||
        snapshotPayloadValue->kind != primec::vm_debug_dap_detail::JsonValue::Kind::Object) {
      error = "malformed replay checkpoint on line " + std::to_string(lineNumber) +
              ": missing object snapshot_payload field";
      return false;
    }

    uint64_t sequence = 0;
    const primec::vm_debug_dap_detail::JsonValue *sequenceValue =
        primec::vm_debug_dap_detail::jsonObjectField(parsedLine, "sequence");
    const bool hasSequence = sequenceValue != nullptr;
    if (hasSequence) {
      if (sequenceValue->kind != primec::vm_debug_dap_detail::JsonValue::Kind::Number ||
          sequenceValue->numberValue < 0) {
        error = "malformed replay checkpoint on line " + std::to_string(lineNumber) +
                ": sequence must be a non-negative integer";
        return false;
      }
      sequence = static_cast<uint64_t>(sequenceValue->numberValue);
      lastSequence = sequence;
      haveSequence = true;
    }

    TraceCheckpoint checkpoint;
    checkpoint.sequence = hasSequence ? sequence : (haveSequence ? lastSequence : 0);
    checkpoint.event = event;
    if (event == "stop") {
      const primec::vm_debug_dap_detail::JsonValue *reasonValue =
          primec::vm_debug_dap_detail::jsonObjectField(parsedLine, "reason");
      if (reasonValue == nullptr) {
        checkpoint.reason = "stop";
      } else if (reasonValue->kind != primec::vm_debug_dap_detail::JsonValue::Kind::String) {
        error = "malformed replay checkpoint on line " + std::to_string(lineNumber) +
                ": stop reason must be a string";
        return false;
      } else {
        checkpoint.reason = reasonValue->stringValue;
      }
    } else {
      checkpoint.reason = event;
    }
    checkpoint.snapshotJson = encodeParsedJsonValue(*snapshotValue);
    checkpoint.snapshotPayloadJson = encodeParsedJsonValue(*snapshotPayloadValue);
    checkpoint.hasSnapshotResult = extractJsonUnsignedField(*snapshotValue, "result", checkpoint.snapshotResult);
    outCheckpoints.push_back(std::move(checkpoint));
  }

  if (outCheckpoints.empty()) {
    error = "replay trace has no checkpoint-capable events";
    return false;
  }
  return true;
}

int emitVmRuntimeFailure(const primec::Options &options,
                         const primec::IrBackendDiagnostics &vmDiagnostics,
                         const std::string &message,
                         std::string_view stage = {}) {
  primec::CliFailure runtimeFailure;
  runtimeFailure.code = primec::DiagnosticCode::RuntimeError;
  runtimeFailure.plainPrefix = "VM error: ";
  runtimeFailure.message = message;
  runtimeFailure.exitCode = 3;
  runtimeFailure.notes = primec::makeIrBackendNotes(vmDiagnostics, stage);
  return primec::emitCliFailure(std::cerr, options, runtimeFailure);
}
} // namespace

int main(int argc, char **argv) {
  primec::Options options;
  std::string argError;
  if (!primec::parseOptions(argc, argv, primec::OptionsParserMode::Primevm, options, argError)) {
    if (options.emitDiagnostics) {
      if (argError.empty()) {
        argError = "invalid arguments";
      }
      const primec::DiagnosticRecord diagnostic =
          primec::makeDiagnosticRecord(primec::DiagnosticCode::ArgumentError, argError, options.inputPath);
      std::cerr << primec::encodeDiagnosticsJson({diagnostic}) << "\n";
    } else {
      if (!argError.empty()) {
        std::cerr << "Argument error: " << argError << "\n";
      }
      std::cerr << "Usage: primevm <input.prime> [--entry /path] [--import-path <dir>] [-I <dir>] "
                   "[--text-transforms <list>] [--text-transform-rules <rules>] [--semantic-transform-rules <rules>] "
                   "[--semantic-transforms <list>] [--transform-list <list>] [--no-text-transforms] "
                   "[--no-semantic-transforms] [--no-transforms] [--list-transforms] [--emit-diagnostics] "
                   "[--debug-json] [--debug-json-snapshots [none|stop|all]] [--debug-trace <path>] [--debug-dap] "
                   "[--debug-replay <trace>] [--debug-replay-sequence <n>] "
                   "[--collect-diagnostics] "
                   "[--default-effects <list>] [--ir-inline] "
                   "[--dump-stage pre_ast|ast|ast-semantic|semantic-product|type-graph|ir] "
                   "[-- <program args...>]\n"
                   "Dump-stage note: lowering-facing dumps now include semantic-product between ast-semantic and ir.\n";
    }
    return 2;
  }
  if (options.listTransforms) {
    primec::printTransformList(std::cout);
    return 0;
  }
  const primec::IrBackendDiagnostics &vmDiagnostics = primec::vmIrBackendDiagnostics();
  if (!options.debugReplayPath.empty() && options.dumpStage.empty()) {
    std::ifstream sourceFile(options.inputPath, std::ios::binary);
    if (!sourceFile.good()) {
      primec::CliFailure importFailure;
      importFailure.code = primec::DiagnosticCode::ImportError;
      importFailure.plainPrefix = "Import error: ";
      importFailure.message = "failed to read input: " + options.inputPath;
      return primec::emitCliFailure(std::cerr, options, importFailure);
    }

    std::string error;
    std::string traceText;
    if (!readTextFile(options.debugReplayPath, traceText, error)) {
      return emitVmRuntimeFailure(options, vmDiagnostics, error, "debug-replay");
    }

    std::vector<TraceCheckpoint> checkpoints;
    if (!parseTraceCheckpoints(traceText, checkpoints, error)) {
      return emitVmRuntimeFailure(options, vmDiagnostics, error, "debug-replay");
    }

    const uint64_t targetSequence = options.debugReplaySequence.value_or(checkpoints.back().sequence);
    size_t checkpointIndex = 0;
    for (size_t i = 0; i < checkpoints.size(); ++i) {
      if (checkpoints[i].sequence <= targetSequence) {
        checkpointIndex = i;
      } else {
        break;
      }
    }
    const TraceCheckpoint &checkpoint = checkpoints[checkpointIndex];

    std::string replayLine = std::string("{\"version\":1,\"event\":\"replay_checkpoint\",\"target_sequence\":") +
                             std::to_string(targetSequence) + ",\"checkpoint_sequence\":" +
                             std::to_string(checkpoint.sequence) + ",\"checkpoint_event\":\"" +
                             jsonEscape(checkpoint.event) + "\",\"reason\":\"" + jsonEscape(checkpoint.reason) +
                             "\",\"snapshot\":" + checkpoint.snapshotJson + ",\"snapshot_payload\":" +
                             checkpoint.snapshotPayloadJson + "}";
    std::cout << replayLine << "\n";

    if (!options.debugReplaySequence.has_value() && checkpoint.event == "stop" && checkpoint.reason == "Exit") {
      if (checkpoint.hasSnapshotResult) {
        return static_cast<int>(static_cast<int32_t>(checkpoint.snapshotResult));
      }
    }
    return 0;
  }
  std::string error;
  primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);

  primec::CompilePipelineDiagnosticInfo pipelineDiagnosticInfo;
  primec::CompilePipelineErrorStage pipelineError = primec::CompilePipelineErrorStage::None;
  primec::CompilePipelineResult pipelineResult =
      primec::runCompilePipelineResult(options, pipelineError, error, &pipelineDiagnosticInfo);
  if (const auto *pipelineFailure =
          std::get_if<primec::CompilePipelineFailureResult>(&pipelineResult)) {
    return primec::emitCliFailure(
        std::cerr, options, primec::describeCompilePipelineFailure(*pipelineFailure));
  }
  primec::CompilePipelineOutput pipelineOutput =
      std::move(std::get<primec::CompilePipelineSuccessResult>(pipelineResult).output);
  if (pipelineOutput.hasDumpOutput) {
    std::cout << pipelineOutput.dumpOutput;
    return 0;
  }

  primec::Program &program = pipelineOutput.program;
  const primec::SemanticProgram *semanticProgram =
      pipelineOutput.hasSemanticProgram ? &pipelineOutput.semanticProgram : nullptr;

  primec::IrModule ir;
  primec::IrPreparationFailure irFailure;
  if (!primec::prepareIrModule(program, semanticProgram, options, primec::IrValidationTarget::Vm, ir, irFailure)) {
    return primec::emitCliFailure(
        std::cerr,
        options,
        primec::describeIrPreparationFailure(irFailure, vmDiagnostics, &primec::normalizeVmLoweringError));
  }
  // IR preparation already released body-heavy AST storage. VM execution only
  // needs lowered IR plus the remaining envelope cleanup at this point.
  program = {};
  if (pipelineOutput.hasSemanticProgram) {
    pipelineOutput.semanticProgram = {};
  }

  primec::Vm vm;
  std::vector<std::string_view> args;
  args.reserve(1 + options.programArgs.size());
  args.push_back(options.inputPath);
  for (const auto &arg : options.programArgs) {
    args.push_back(arg);
  }
  uint64_t result = 0;
  if (options.debugDap) {
    primec::VmDebugDapRunResult dapResult;
    if (!primec::runVmDebugDapSession(ir, args, options.inputPath, std::cin, std::cout, dapResult, error)) {
      return emitVmRuntimeFailure(options, vmDiagnostics, error, "debug-dap");
    }
    return 0;
  }
  if (!options.debugTracePath.empty()) {
    primec::VmDebugSession debugSession;
    if (!debugSession.start(ir, error, args)) {
      return emitVmRuntimeFailure(options, vmDiagnostics, error);
    }

    DebugJsonEmitContext emitContext;
    emitContext.session = &debugSession;
    emitContext.snapshotMode = primec::DebugJsonSnapshotMode::All;
    std::vector<std::string> traceLines;

    std::string sessionStartLine =
        std::string("{\"version\":1,\"event\":\"session_start\",\"snapshot\":") + encodeDebugSnapshotJson(debugSession.snapshot());
    appendDebugSnapshotPayloadField(sessionStartLine, &emitContext, false);
    sessionStartLine += "}";
    traceLines.push_back(std::move(sessionStartLine));

    struct DebugTraceEmitContext {
      primec::VmDebugSession *session = nullptr;
      std::vector<std::string> *lines = nullptr;
    };
    DebugTraceEmitContext traceContext;
    traceContext.session = &debugSession;
    traceContext.lines = &traceLines;

    primec::VmDebugHooks hooks;
    hooks.userData = &traceContext;
    hooks.beforeInstruction = [](const primec::VmDebugInstructionHookEvent &event, void *userData) {
      auto *context = static_cast<DebugTraceEmitContext *>(userData);
      std::string line = std::string("{\"version\":1,\"event\":\"before_instruction\",\"sequence\":") +
                         std::to_string(event.sequence) + ",\"snapshot\":" + encodeDebugSnapshotJson(event.snapshot) +
                         ",\"opcode\":" + std::to_string(static_cast<uint32_t>(event.opcode)) + ",\"immediate\":" +
                         std::to_string(event.immediate) + ",\"snapshot_payload\":" +
                         encodeDebugSnapshotPayloadJson(context->session->snapshotPayload());
      line += "}";
      context->lines->push_back(std::move(line));
    };
    hooks.afterInstruction = [](const primec::VmDebugInstructionHookEvent &event, void *userData) {
      auto *context = static_cast<DebugTraceEmitContext *>(userData);
      std::string line = std::string("{\"version\":1,\"event\":\"after_instruction\",\"sequence\":") +
                         std::to_string(event.sequence) + ",\"snapshot\":" + encodeDebugSnapshotJson(event.snapshot) +
                         ",\"opcode\":" + std::to_string(static_cast<uint32_t>(event.opcode)) + ",\"immediate\":" +
                         std::to_string(event.immediate) + ",\"snapshot_payload\":" +
                         encodeDebugSnapshotPayloadJson(context->session->snapshotPayload());
      line += "}";
      context->lines->push_back(std::move(line));
    };
    hooks.callPush = [](const primec::VmDebugCallHookEvent &event, void *userData) {
      auto *context = static_cast<DebugTraceEmitContext *>(userData);
      std::string line = std::string("{\"version\":1,\"event\":\"call_push\",\"sequence\":") +
                         std::to_string(event.sequence) + ",\"snapshot\":" + encodeDebugSnapshotJson(event.snapshot) +
                         ",\"function_index\":" + std::to_string(event.functionIndex) +
                         ",\"returns_value_to_caller\":" + (event.returnsValueToCaller ? "true" : "false") +
                         ",\"snapshot_payload\":" + encodeDebugSnapshotPayloadJson(context->session->snapshotPayload());
      line += "}";
      context->lines->push_back(std::move(line));
    };
    hooks.callPop = [](const primec::VmDebugCallHookEvent &event, void *userData) {
      auto *context = static_cast<DebugTraceEmitContext *>(userData);
      std::string line = std::string("{\"version\":1,\"event\":\"call_pop\",\"sequence\":") +
                         std::to_string(event.sequence) + ",\"snapshot\":" + encodeDebugSnapshotJson(event.snapshot) +
                         ",\"function_index\":" + std::to_string(event.functionIndex) +
                         ",\"returns_value_to_caller\":" + (event.returnsValueToCaller ? "true" : "false") +
                         ",\"snapshot_payload\":" + encodeDebugSnapshotPayloadJson(context->session->snapshotPayload());
      line += "}";
      context->lines->push_back(std::move(line));
    };
    hooks.fault = [](const primec::VmDebugFaultHookEvent &event, void *userData) {
      auto *context = static_cast<DebugTraceEmitContext *>(userData);
      std::string line = std::string("{\"version\":1,\"event\":\"fault\",\"sequence\":") + std::to_string(event.sequence) +
                         ",\"snapshot\":" + encodeDebugSnapshotJson(event.snapshot) + ",\"opcode\":" +
                         std::to_string(static_cast<uint32_t>(event.opcode)) + ",\"immediate\":" +
                         std::to_string(event.immediate) + ",\"message\":\"" + jsonEscape(event.message) +
                         "\",\"snapshot_payload\":" + encodeDebugSnapshotPayloadJson(context->session->snapshotPayload());
      line += "}";
      context->lines->push_back(std::move(line));
    };
    debugSession.setHooks(hooks);

    bool sawFault = false;
    int exitCode = 0;
    while (true) {
      primec::VmDebugStopReason stopReason = primec::VmDebugStopReason::Step;
      error.clear();
      const bool ok = debugSession.continueExecution(stopReason, error);
      const primec::VmDebugSnapshot stopSnapshot = debugSession.snapshot();
      std::string stopLine = std::string("{\"version\":1,\"event\":\"stop\",\"reason\":\"") +
                             std::string(primec::vmDebugStopReasonName(stopReason)) + "\",\"snapshot\":" +
                             encodeDebugSnapshotJson(stopSnapshot) + ",\"snapshot_payload\":" +
                             encodeDebugSnapshotPayloadJson(debugSession.snapshotPayload());
      if (!ok && !error.empty()) {
        stopLine += ",\"message\":\"" + jsonEscape(error) + "\"";
      }
      stopLine += "}";
      traceLines.push_back(std::move(stopLine));

      if (!ok) {
        sawFault = true;
        break;
      }
      if (stopReason == primec::VmDebugStopReason::Exit) {
        exitCode = static_cast<int>(static_cast<int32_t>(stopSnapshot.result));
        break;
      }
    }

    std::string traceError;
    if (!writeTraceLines(options.debugTracePath, traceLines, traceError)) {
      return emitVmRuntimeFailure(options, vmDiagnostics, traceError, "debug-trace");
    }
    if (sawFault) {
      return emitVmRuntimeFailure(options, vmDiagnostics, error, "debug-trace");
    }
    return exitCode;
  }
  if (options.debugJson) {
    primec::VmDebugSession debugSession;
    if (!debugSession.start(ir, error, args)) {
      return emitVmRuntimeFailure(options, vmDiagnostics, error);
    }

    DebugJsonEmitContext emitContext;
    emitContext.session = &debugSession;
    emitContext.snapshotMode = options.debugJsonSnapshotMode;

    std::string sessionStartLine =
        std::string("{\"version\":1,\"event\":\"session_start\",\"snapshot\":") + encodeDebugSnapshotJson(debugSession.snapshot());
    appendDebugSnapshotPayloadField(sessionStartLine, &emitContext, false);
    sessionStartLine += "}";
    emitDebugJsonLine(sessionStartLine);

    primec::VmDebugHooks hooks;
    hooks.userData = &emitContext;
    hooks.beforeInstruction = [](const primec::VmDebugInstructionHookEvent &event, void *userData) {
      const auto *context = static_cast<const DebugJsonEmitContext *>(userData);
      std::string line = std::string("{\"version\":1,\"event\":\"before_instruction\",\"sequence\":") +
                         std::to_string(event.sequence) + ",\"snapshot\":" + encodeDebugSnapshotJson(event.snapshot) +
                         ",\"opcode\":" + std::to_string(static_cast<uint32_t>(event.opcode)) + ",\"immediate\":" +
                         std::to_string(event.immediate);
      appendDebugSnapshotPayloadField(line, context, false);
      line += "}";
      emitDebugJsonLine(line);
    };
    hooks.afterInstruction = [](const primec::VmDebugInstructionHookEvent &event, void *userData) {
      const auto *context = static_cast<const DebugJsonEmitContext *>(userData);
      std::string line = std::string("{\"version\":1,\"event\":\"after_instruction\",\"sequence\":") +
                         std::to_string(event.sequence) + ",\"snapshot\":" + encodeDebugSnapshotJson(event.snapshot) +
                         ",\"opcode\":" + std::to_string(static_cast<uint32_t>(event.opcode)) + ",\"immediate\":" +
                         std::to_string(event.immediate);
      appendDebugSnapshotPayloadField(line, context, false);
      line += "}";
      emitDebugJsonLine(line);
    };
    hooks.callPush = [](const primec::VmDebugCallHookEvent &event, void *userData) {
      const auto *context = static_cast<const DebugJsonEmitContext *>(userData);
      std::string line = std::string("{\"version\":1,\"event\":\"call_push\",\"sequence\":") +
                         std::to_string(event.sequence) + ",\"snapshot\":" + encodeDebugSnapshotJson(event.snapshot) +
                         ",\"function_index\":" + std::to_string(event.functionIndex) +
                         ",\"returns_value_to_caller\":" + (event.returnsValueToCaller ? "true" : "false");
      appendDebugSnapshotPayloadField(line, context, false);
      line += "}";
      emitDebugJsonLine(line);
    };
    hooks.callPop = [](const primec::VmDebugCallHookEvent &event, void *userData) {
      const auto *context = static_cast<const DebugJsonEmitContext *>(userData);
      std::string line = std::string("{\"version\":1,\"event\":\"call_pop\",\"sequence\":") +
                         std::to_string(event.sequence) + ",\"snapshot\":" + encodeDebugSnapshotJson(event.snapshot) +
                         ",\"function_index\":" + std::to_string(event.functionIndex) +
                         ",\"returns_value_to_caller\":" + (event.returnsValueToCaller ? "true" : "false");
      appendDebugSnapshotPayloadField(line, context, false);
      line += "}";
      emitDebugJsonLine(line);
    };
    hooks.fault = [](const primec::VmDebugFaultHookEvent &event, void *userData) {
      const auto *context = static_cast<const DebugJsonEmitContext *>(userData);
      std::string line = std::string("{\"version\":1,\"event\":\"fault\",\"sequence\":") + std::to_string(event.sequence) +
                         ",\"snapshot\":" + encodeDebugSnapshotJson(event.snapshot) + ",\"opcode\":" +
                         std::to_string(static_cast<uint32_t>(event.opcode)) + ",\"immediate\":" +
                         std::to_string(event.immediate) + ",\"message\":\"" + jsonEscape(event.message) + "\"";
      appendDebugSnapshotPayloadField(line, context, false);
      line += "}";
      emitDebugJsonLine(line);
    };
    debugSession.setHooks(hooks);

    while (true) {
      primec::VmDebugStopReason stopReason = primec::VmDebugStopReason::Step;
      error.clear();
      const bool ok = debugSession.continueExecution(stopReason, error);
      const primec::VmDebugSnapshot stopSnapshot = debugSession.snapshot();
      std::string stopLine = std::string("{\"version\":1,\"event\":\"stop\",\"reason\":\"") +
                             std::string(primec::vmDebugStopReasonName(stopReason)) + "\",\"snapshot\":" +
                             encodeDebugSnapshotJson(stopSnapshot);
      appendDebugSnapshotPayloadField(stopLine, &emitContext, true);
      if (!ok && !error.empty()) {
        stopLine += ",\"message\":\"" + jsonEscape(error) + "\"";
      }
      stopLine += "}";
      emitDebugJsonLine(stopLine);

      if (!ok) {
        return emitVmRuntimeFailure(options, vmDiagnostics, error, "debug-json");
      }
      if (stopReason == primec::VmDebugStopReason::Exit) {
        return static_cast<int>(static_cast<int32_t>(stopSnapshot.result));
      }
    }
  }

  if (!vm.execute(ir, result, error, args)) {
    return emitVmRuntimeFailure(options, vmDiagnostics, error);
  }

  return static_cast<int>(static_cast<int32_t>(result));
}
