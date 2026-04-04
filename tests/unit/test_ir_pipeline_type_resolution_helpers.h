#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "primec/CompilePipeline.h"
#include "primec/IrPreparation.h"
#include "primec/IrSerializer.h"
#include "test_ir_pipeline_helpers.h"

namespace {

inline std::string snapshotDiagnosticReport(const primec::DiagnosticSinkReport &report) {
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

inline TypeResolverPipelineSnapshot runTypeResolverPipelineSnapshot(const std::string &source,
                                                                   const std::string &entry = "/main") {
  TypeResolverPipelineSnapshot snapshot;
  const std::filesystem::path tempPath = makeTempIrPipelineSourcePath();
  {
    std::ofstream file(tempPath);
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
    const primec::SemanticProgram *semanticProgram = output.hasSemanticProgram ? &output.semanticProgram : nullptr;
    const bool prepared =
        primec::prepareIrModule(output.program, semanticProgram, options, primec::IrValidationTarget::Vm, ir, irFailure);
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
