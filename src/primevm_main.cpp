#include "primec/CompilePipeline.h"
#include "primec/Diagnostics.h"
#include "primec/IrInliner.h"
#include "primec/IrLowerer.h"
#include "primec/IrValidation.h"
#include "primec/Options.h"
#include "primec/OptionsParser.h"
#include "primec/TransformRegistry.h"
#include "primec/Vm.h"

#include <algorithm>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {
std::string transformAvailability(const primec::TransformInfo &info) {
  std::string availability;
  if (info.availableInPrimec) {
    availability = "primec";
  }
  if (info.availableInPrimevm) {
    if (!availability.empty()) {
      availability += ",";
    }
    availability += "primevm";
  }
  if (availability.empty()) {
    return "none";
  }
  return availability;
}

void printTransformList(std::ostream &out) {
  out << "name\tphase\taliases\tavailability\n";
  for (const auto &transform : primec::listTransforms()) {
    out << transform.name << '\t' << primec::transformPhaseName(transform.phase) << '\t' << transform.aliases << '\t'
        << transformAvailability(transform) << '\n';
  }
}

void replaceAll(std::string &text, std::string_view from, std::string_view to) {
  if (from.empty()) {
    return;
  }
  size_t pos = 0;
  while ((pos = text.find(from, pos)) != std::string::npos) {
    text.replace(pos, from.size(), to);
    pos += to.size();
  }
}

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
         encodeCallStackJson(payload.callStack) + ",\"current_frame_locals\":" +
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

int emitFailure(const primec::Options &options,
                primec::DiagnosticCode code,
                const std::string &plainPrefix,
                const std::string &message,
                int exitCode,
                const std::vector<std::string> &notes = {},
                const primec::CompilePipelineDiagnosticInfo *diagnosticInfo = nullptr,
                const std::string *sourceText = nullptr) {
  std::vector<primec::DiagnosticRecord> diagnostics;
  if (diagnosticInfo != nullptr && !diagnosticInfo->records.empty()) {
    diagnostics.reserve(diagnosticInfo->records.size());
    for (const auto &recordInfo : diagnosticInfo->records) {
      std::string diagnosticMessage = recordInfo.normalizedMessage.empty() ? message : recordInfo.normalizedMessage;
      const primec::DiagnosticSpan *primarySpan = nullptr;
      if (recordInfo.hasPrimarySpan) {
        primarySpan = &recordInfo.primarySpan;
      }
      diagnostics.push_back(primec::makeDiagnosticRecord(
          code, diagnosticMessage, options.inputPath, notes, primarySpan, recordInfo.relatedSpans));
    }
  } else {
    std::string diagnosticMessage = message;
    primec::DiagnosticSpan primarySpanStorage;
    const primec::DiagnosticSpan *primarySpan = nullptr;
    std::vector<primec::DiagnosticRelatedSpan> relatedSpans;
    if (diagnosticInfo != nullptr) {
      if (!diagnosticInfo->normalizedMessage.empty()) {
        diagnosticMessage = diagnosticInfo->normalizedMessage;
      }
      if (diagnosticInfo->hasPrimarySpan) {
        primarySpanStorage = diagnosticInfo->primarySpan;
        primarySpan = &primarySpanStorage;
      }
      relatedSpans = diagnosticInfo->relatedSpans;
    }
    diagnostics.push_back(
        primec::makeDiagnosticRecord(code, diagnosticMessage, options.inputPath, notes, primarySpan, relatedSpans));
  }

  if (options.emitDiagnostics) {
    std::cerr << primec::encodeDiagnosticsJson(diagnostics) << "\n";
    return exitCode;
  }

  const bool hasLocation = std::any_of(diagnostics.begin(), diagnostics.end(), [](const auto &record) {
    return record.primarySpan.line > 0 && record.primarySpan.column > 0;
  });
  if (hasLocation) {
    std::cerr << primec::formatDiagnosticsText(diagnostics, plainPrefix, sourceText);
    return exitCode;
  }

  std::cerr << plainPrefix << message << "\n";
  return exitCode;
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
                   "[--debug-json] [--debug-json-snapshots [none|stop|all]] [--collect-diagnostics] "
                   "[--default-effects <list>] [--ir-inline] [--dump-stage pre_ast|ast|ast-semantic|ir] "
                   "[-- <program args...>]\n";
    }
    return 2;
  }
  if (options.listTransforms) {
    printTransformList(std::cout);
    return 0;
  }
  std::string error;
  primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);

  primec::CompilePipelineOutput pipelineOutput;
  primec::CompilePipelineDiagnosticInfo pipelineDiagnosticInfo;
  primec::CompilePipelineErrorStage pipelineError = primec::CompilePipelineErrorStage::None;
  if (!primec::runCompilePipeline(options, pipelineOutput, pipelineError, error, &pipelineDiagnosticInfo)) {
    switch (pipelineError) {
      case primec::CompilePipelineErrorStage::Import:
        return emitFailure(options,
                           primec::DiagnosticCode::ImportError,
                           "Import error: ",
                           error,
                           2,
                           {"stage: import"});
      case primec::CompilePipelineErrorStage::Transform:
        return emitFailure(options,
                           primec::DiagnosticCode::TransformError,
                           "Transform error: ",
                           error,
                           2,
                           {"stage: transform"});
      case primec::CompilePipelineErrorStage::Parse:
        return emitFailure(options,
                           primec::DiagnosticCode::ParseError,
                           "Parse error: ",
                           error,
                           2,
                           {"stage: parse"},
                           &pipelineDiagnosticInfo,
                           &pipelineOutput.filteredSource);
      case primec::CompilePipelineErrorStage::UnsupportedDumpStage:
        return emitFailure(options,
                           primec::DiagnosticCode::UnsupportedDumpStage,
                           "Unsupported dump stage: ",
                           error,
                           2,
                           {"stage: dump-stage"});
      case primec::CompilePipelineErrorStage::Semantic:
        return emitFailure(options,
                           primec::DiagnosticCode::SemanticError,
                           "Semantic error: ",
                           error,
                           2,
                           {"stage: semantic"},
                           &pipelineDiagnosticInfo,
                           &pipelineOutput.filteredSource);
      default:
        return emitFailure(options,
                           primec::DiagnosticCode::EmitError,
                           "Compile pipeline error: ",
                           error,
                           2,
                           {"stage: compile-pipeline"});
    }
  }
  if (pipelineOutput.hasDumpOutput) {
    std::cout << pipelineOutput.dumpOutput;
    return 0;
  }

  primec::Program &program = pipelineOutput.program;

  primec::IrLowerer lowerer;
  primec::IrModule ir;
  if (!lowerer.lower(program,
                     options.entryPath,
                     options.defaultEffects,
                     options.entryDefaultEffects,
                     ir,
                     error)) {
    std::string vmError = error;
    replaceAll(vmError, "native backend", "vm backend");
    return emitFailure(options,
                       primec::DiagnosticCode::LoweringError,
                       "VM lowering error: ",
                       vmError,
                       2,
                       {"backend: vm"});
  }
  if (!primec::validateIrModule(ir, primec::IrValidationTarget::Vm, error)) {
    return emitFailure(options,
                       primec::DiagnosticCode::LoweringError,
                       "VM IR validation error: ",
                       error,
                       2,
                       {"backend: vm", "stage: ir-validate"});
  }
  if (options.inlineIrCalls) {
    if (!primec::inlineIrModuleCalls(ir, error)) {
      return emitFailure(options,
                         primec::DiagnosticCode::LoweringError,
                         "VM IR inlining error: ",
                         error,
                         2,
                         {"backend: vm", "stage: ir-inline"});
    }
    if (!primec::validateIrModule(ir, primec::IrValidationTarget::Vm, error)) {
      return emitFailure(options,
                         primec::DiagnosticCode::LoweringError,
                         "VM IR validation error: ",
                         error,
                         2,
                         {"backend: vm", "stage: ir-validate"});
    }
  }

  primec::Vm vm;
  std::vector<std::string_view> args;
  args.reserve(1 + options.programArgs.size());
  args.push_back(options.inputPath);
  for (const auto &arg : options.programArgs) {
    args.push_back(arg);
  }
  uint64_t result = 0;
  if (options.debugJson) {
    primec::VmDebugSession debugSession;
    if (!debugSession.start(ir, error, args)) {
      return emitFailure(options, primec::DiagnosticCode::RuntimeError, "VM error: ", error, 3, {"backend: vm"});
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
        return emitFailure(options,
                           primec::DiagnosticCode::RuntimeError,
                           "VM error: ",
                           error,
                           3,
                           {"backend: vm", "stage: debug-json"});
      }
      if (stopReason == primec::VmDebugStopReason::Exit) {
        return static_cast<int>(static_cast<int32_t>(stopSnapshot.result));
      }
    }
  }

  if (!vm.execute(ir, result, error, args)) {
    return emitFailure(options, primec::DiagnosticCode::RuntimeError, "VM error: ", error, 3, {"backend: vm"});
  }

  return static_cast<int>(static_cast<int32_t>(result));
}
