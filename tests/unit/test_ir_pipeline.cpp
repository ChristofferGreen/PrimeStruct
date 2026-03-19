#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <unordered_map>
#include <unordered_set>

#include "third_party/doctest.h"

#include "primec/CompilePipeline.h"
#include "primec/IrLowerer.h"
#include "primec/IrInliner.h"
#include "primec/IrBackends.h"
#include "primec/IrPreparation.h"
#include "primec/IrSerializer.h"
#include "primec/IrToCppEmitter.h"
#include "primec/IrToGlslEmitter.h"
#include "primec/IrValidation.h"
#include "primec/IrVirtualRegisterAllocator.h"
#include "primec/IrVirtualRegisterLiveness.h"
#include "primec/IrVirtualRegisterLowering.h"
#include "primec/IrVirtualRegisterScheduler.h"
#include "primec/IrVirtualRegisterSpillInsertion.h"
#include "primec/IrVirtualRegisterVerifier.h"
#include "primec/Lexer.h"
#include "primec/NativeEmitter.h"
#include "primec/Parser.h"
#include "primec/Semantics.h"
#include "primec/Vm.h"
#include "primec/VmDebugAdapter.h"
#include "primec/WasmEmitter.h"
#include "primec/testing/EmitterHelpers.h"
#include "primec/testing/IrLowererHelpers.h"
#include "primec/testing/SemanticsValidationHelpers.h"
#if defined(__unix__) || defined(__APPLE__)
#include <sys/wait.h>
#endif

namespace {
bool usesStdImportPipeline(const std::string &source) {
  size_t pos = 0;
  while ((pos = source.find("import /std", pos)) != std::string::npos) {
    const size_t next = pos + std::string("import /std").size();
    if (next >= source.size()) {
      return true;
    }
    const char c = source[next];
    if (c == '/' || std::isspace(static_cast<unsigned char>(c)) != 0) {
      return true;
    }
    pos = next;
  }
  return false;
}

std::filesystem::path makeTempIrPipelineSourcePath() {
  static std::atomic<unsigned long long> counter{0};
  const auto nonce = std::to_string(
      static_cast<unsigned long long>(std::chrono::steady_clock::now().time_since_epoch().count()));
  return std::filesystem::temp_directory_path() /
         ("primestruct-ir-pipeline-" + nonce + "-" + std::to_string(counter++) + ".prime");
}

bool parseAndValidateThroughCompilePipeline(const std::string &source,
                                            primec::Program &program,
                                            std::string &error,
                                            const std::vector<std::string> &defaultEffects,
                                            const std::vector<std::string> &entryDefaultEffects) {
  const std::filesystem::path tempPath = makeTempIrPipelineSourcePath();
  {
    std::ofstream file(tempPath);
    if (!file) {
      error = "failed to write ir pipeline test source";
      return false;
    }
    file << source;
  }

  primec::Options options;
  options.inputPath = tempPath.string();
  options.entryPath = "/main";
  options.defaultEffects = defaultEffects;
  options.entryDefaultEffects = entryDefaultEffects;
  options.dumpStage = "ast_semantic";
  primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);

  primec::CompilePipelineOutput output;
  primec::CompilePipelineErrorStage errorStage = primec::CompilePipelineErrorStage::None;
  const bool ok = primec::runCompilePipeline(options, output, errorStage, error);

  std::error_code ec;
  std::filesystem::remove(tempPath, ec);
  if (!ok) {
    return false;
  }

  program = std::move(output.program);
  return true;
}

bool parseAndValidate(const std::string &source,
                      primec::Program &program,
                      std::string &error,
                      const std::vector<std::string> &defaultEffects,
                      const std::vector<std::string> &entryDefaultEffects) {
  if (usesStdImportPipeline(source)) {
    return parseAndValidateThroughCompilePipeline(
        source, program, error, defaultEffects, entryDefaultEffects);
  }
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  if (!parser.parse(program, error)) {
    return false;
  }
  primec::Semantics semantics;
  return semantics.validate(program, "/main", error, defaultEffects, entryDefaultEffects);
}

bool parseAndValidate(const std::string &source,
                      primec::Program &program,
                      std::string &error,
                      std::vector<std::string> defaultEffects = {}) {
  return parseAndValidate(source, program, error, defaultEffects, defaultEffects);
}

bool validateProgram(primec::Program &program,
                     const std::string &entry,
                     std::string &error,
                     std::vector<std::string> defaultEffects = {}) {
  primec::Semantics semantics;
  return semantics.validate(program, entry, error, defaultEffects, defaultEffects);
}

std::string snapshotDiagnosticReport(const primec::DiagnosticSinkReport &report) {
  std::string snapshot = "message=" + report.message + "\n";
  snapshot += "primary=" + report.primarySpan.file + ":" + std::to_string(report.primarySpan.line) + ":" +
              std::to_string(report.primarySpan.column) + ":" + std::to_string(report.primarySpan.endLine) + ":" +
              std::to_string(report.primarySpan.endColumn) + ":" + (report.hasPrimarySpan ? "1" : "0") + "\n";
  for (size_t relatedIndex = 0; relatedIndex < report.relatedSpans.size(); ++relatedIndex) {
    const auto &related = report.relatedSpans[relatedIndex];
    snapshot += "related[" + std::to_string(relatedIndex) + "]=" + related.label + "@" + related.span.file + ":" +
                std::to_string(related.span.line) + ":" + std::to_string(related.span.column) + ":" +
                std::to_string(related.span.endLine) + ":" + std::to_string(related.span.endColumn) + "\n";
  }
  for (size_t recordIndex = 0; recordIndex < report.records.size(); ++recordIndex) {
    const auto &record = report.records[recordIndex];
    snapshot += "record[" + std::to_string(recordIndex) + "]=" + record.message + "@" + record.primarySpan.file +
                ":" + std::to_string(record.primarySpan.line) + ":" + std::to_string(record.primarySpan.column) + ":" +
                std::to_string(record.primarySpan.endLine) + ":" + std::to_string(record.primarySpan.endColumn) + ":" +
                (record.hasPrimarySpan ? "1" : "0") + "\n";
    for (size_t relatedIndex = 0; relatedIndex < record.relatedSpans.size(); ++relatedIndex) {
      const auto &related = record.relatedSpans[relatedIndex];
      snapshot += "record_related[" + std::to_string(recordIndex) + "][" + std::to_string(relatedIndex) + "]=" +
                  related.label + "@" + related.span.file + ":" + std::to_string(related.span.line) + ":" +
                  std::to_string(related.span.column) + ":" + std::to_string(related.span.endLine) + ":" +
                  std::to_string(related.span.endColumn) + "\n";
    }
  }
  return snapshot;
}

struct TypeResolverPipelineSnapshot {
  bool ok = false;
  primec::CompilePipelineErrorStage errorStage = primec::CompilePipelineErrorStage::None;
  std::string error;
  std::string diagnosticSnapshot;
  std::vector<uint8_t> serializedIr;
};

TypeResolverPipelineSnapshot runTypeResolverPipelineSnapshot(const std::string &source,
                                                            const std::string &typeResolver,
                                                            const std::string &entry = "/main") {
  TypeResolverPipelineSnapshot snapshot;
  const std::filesystem::path tempPath = makeTempIrPipelineSourcePath();
  {
    std::ofstream file(tempPath);
    CHECK(file.good());
    if (!file.good()) {
      snapshot.error = "failed to write parity source";
      return snapshot;
    }
    file << source;
  }

  primec::Options options;
  options.inputPath = tempPath.string();
  options.entryPath = entry;
  options.emitKind = "vm";
  options.typeResolver = typeResolver;
  options.collectDiagnostics = true;
  options.defaultEffects = {"io_out", "io_err"};
  options.entryDefaultEffects = options.defaultEffects;
  primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);

  primec::CompilePipelineOutput output;
  primec::CompilePipelineDiagnosticInfo diagnosticInfo;
  snapshot.ok = primec::runCompilePipeline(options, output, snapshot.errorStage, snapshot.error, &diagnosticInfo);
  snapshot.diagnosticSnapshot = snapshotDiagnosticReport(diagnosticInfo);
  if (snapshot.ok) {
    primec::IrModule ir;
    primec::IrPreparationFailure irFailure;
    const bool prepared = primec::prepareIrModule(output.program, options, primec::IrValidationTarget::Vm, ir, irFailure);
    if (prepared) {
      std::string serializeError;
      const bool serialized = primec::serializeIr(ir, snapshot.serializedIr, serializeError);
      if (!serialized) {
        snapshot.ok = false;
        snapshot.error = serializeError;
      }
    } else {
      snapshot.ok = false;
      snapshot.error = irFailure.message;
    }
  }

  std::error_code ec;
  std::filesystem::remove(tempPath, ec);
  return snapshot;
}
} // namespace

#include "test_ir_pipeline_serialization.h"
#include "test_ir_pipeline_pointers.h"
#include "test_ir_pipeline_conversions.h"
#include "test_ir_pipeline_entry_args.h"
#include "test_ir_pipeline_validation.h"
#include "test_vm_debug_session.h"
#include "test_ir_pipeline_gpu.h"
#include "test_ir_pipeline_external_tooling.h"
#include "test_ir_pipeline_wasm.h"
#include "test_ir_pipeline_backends.h"
#include "test_ir_pipeline_to_cpp.h"
#include "test_ir_pipeline_to_glsl.h"
#include "test_ir_pipeline_type_resolution_parity.h"
