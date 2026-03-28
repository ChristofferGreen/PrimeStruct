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
#include "primec/testing/TestScratch.h"
#include "test_ir_pipeline_helpers.h"
#if defined(__unix__) || defined(__APPLE__)
#include <sys/wait.h>
#endif

namespace {
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
#include "test_ir_pipeline_validation.h"
#include "test_ir_pipeline_gpu.h"
#include "test_ir_pipeline_type_resolution_parity.h"
