#include "primec/CompilePipeline.h"
#include "primec/Diagnostics.h"
#include "primec/IrInliner.h"
#include "primec/IrLowerer.h"
#include "primec/IrValidation.h"
#include "primec/Options.h"
#include "primec/OptionsParser.h"
#include "primec/TransformRegistry.h"
#include "primec/Vm.h"

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

int emitFailure(const primec::Options &options,
                primec::DiagnosticCode code,
                const std::string &plainPrefix,
                const std::string &message,
                int exitCode,
                const std::vector<std::string> &notes = {},
                const primec::CompilePipelineDiagnosticInfo *diagnosticInfo = nullptr) {
  if (options.emitDiagnostics) {
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
    std::cerr << primec::encodeDiagnosticsJson(diagnostics) << "\n";
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
                   "[--collect-diagnostics] "
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
                           &pipelineDiagnosticInfo);
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
                           &pipelineDiagnosticInfo);
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
  if (!vm.execute(ir, result, error, args)) {
    return emitFailure(options, primec::DiagnosticCode::RuntimeError, "VM error: ", error, 3, {"backend: vm"});
  }

  return static_cast<int>(static_cast<int32_t>(result));
}
