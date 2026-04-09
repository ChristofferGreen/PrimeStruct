#pragma once

#include <atomic>
#include <chrono>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "primec/CompilePipeline.h"
#include "primec/EmitKind.h"
#include "primec/IrBackends.h"
#include "primec/IrLowerer.h"
#include "primec/IrPreparation.h"
#include "primec/Lexer.h"
#include "primec/Parser.h"
#include "primec/Semantics.h"
#include "primec/testing/TestScratch.h"

namespace {

inline bool usesStdImportPipeline(const std::string &source) {
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

inline std::filesystem::path makeTempIrPipelineSourcePath() {
  static std::atomic<unsigned long long> counter{0};
  const auto nonce =
      std::to_string(static_cast<unsigned long long>(std::chrono::steady_clock::now().time_since_epoch().count()));
  return primec::testing::testScratchPath(
      "ir_pipeline/primestruct-ir-pipeline-" + nonce + "-" + std::to_string(counter++) + ".prime");
}

inline bool parseAndValidateThroughCompilePipeline(const std::string &source,
                                                   primec::Program &program,
                                                   primec::SemanticProgram *semanticProgramOut,
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
  options.skipSemanticProductForNonConsumingPath = semanticProgramOut == nullptr;
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
  if (semanticProgramOut != nullptr) {
    if (!output.hasSemanticProgram) {
      error = "compile pipeline did not publish semantic product";
      return false;
    }
    *semanticProgramOut = std::move(output.semanticProgram);
  }
  return true;
}

struct PreparedCompilePipelineIrForTesting {
  primec::Options options;
  primec::CompilePipelineOutput output;
  primec::CompilePipelineErrorStage errorStage = primec::CompilePipelineErrorStage::None;
  primec::IrModule ir;
  std::string backendKind;
};

inline bool prepareIrThroughCompilePipeline(const std::string &source,
                                            const std::string &entryPath,
                                            std::string_view emitKind,
                                            PreparedCompilePipelineIrForTesting &prepared,
                                            std::string &error,
                                            primec::CompilePipelineDiagnosticInfo *diagnosticInfo = nullptr) {
  const std::filesystem::path tempPath = makeTempIrPipelineSourcePath();
  {
    std::ofstream file(tempPath);
    if (!file) {
      error = "failed to write compile-pipeline IR test source";
      return false;
    }
    file << source;
  }

  prepared = {};
  prepared.options.inputPath = tempPath.string();
  prepared.options.entryPath = entryPath;
  prepared.options.emitKind = std::string(emitKind);
  prepared.options.wasmProfile = "wasi";
  prepared.options.defaultEffects = {"io_out", "io_err"};
  prepared.options.entryDefaultEffects = prepared.options.defaultEffects;
  prepared.options.collectDiagnostics = diagnosticInfo != nullptr;
  primec::addDefaultStdlibInclude(prepared.options.inputPath, prepared.options.importPaths);

  const bool ok = primec::runCompilePipeline(
      prepared.options, prepared.output, prepared.errorStage, error, diagnosticInfo);

  std::error_code ec;
  std::filesystem::remove(tempPath, ec);
  if (!ok) {
    return false;
  }

  if (!prepared.output.hasSemanticProgram) {
    error = "compile pipeline did not publish semantic product";
    return false;
  }

  prepared.backendKind = std::string(primec::resolveIrBackendEmitKind(emitKind));
  const primec::IrBackend *backend = primec::findIrBackend(prepared.backendKind);
  if (backend == nullptr) {
    error = "unknown IR backend for compile-pipeline IR test: " + std::string(emitKind);
    return false;
  }

  primec::IrPreparationFailure failure;
  if (!primec::prepareIrModule(prepared.output.program,
                               &prepared.output.semanticProgram,
                               prepared.options,
                               backend->validationTarget(prepared.options),
                               prepared.ir,
                               failure)) {
    error = failure.message;
    return false;
  }

  return true;
}

inline bool parseAndValidate(const std::string &source,
                             primec::Program &program,
                             primec::SemanticProgram *semanticProgramOut,
                             std::string &error,
                             const std::vector<std::string> &defaultEffects,
                             const std::vector<std::string> &entryDefaultEffects) {
  if (usesStdImportPipeline(source)) {
    return parseAndValidateThroughCompilePipeline(
        source, program, semanticProgramOut, error, defaultEffects, entryDefaultEffects);
  }
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  if (!parser.parse(program, error)) {
    return false;
  }
  primec::Semantics semantics;
  return semantics.validate(program,
                            "/main",
                            error,
                            defaultEffects,
                            entryDefaultEffects,
                            {},
                            nullptr,
                            false,
                            semanticProgramOut);
}

inline bool parseAndValidate(const std::string &source,
                             primec::Program &program,
                             primec::SemanticProgram &semanticProgram,
                             std::string &error,
                             const std::vector<std::string> &defaultEffects,
                             const std::vector<std::string> &entryDefaultEffects) {
  return parseAndValidate(
      source, program, &semanticProgram, error, defaultEffects, entryDefaultEffects);
}

inline bool parseAndValidate(const std::string &source,
                             primec::Program &program,
                             primec::SemanticProgram &semanticProgram,
                             std::string &error) {
  return parseAndValidate(source, program, &semanticProgram, error, {}, {});
}

inline bool parseAndValidate(const std::string &source,
                             primec::Program &program,
                             primec::SemanticProgram &semanticProgram,
                             std::string &error,
                             std::vector<std::string> defaultEffects) {
  return parseAndValidate(
      source, program, &semanticProgram, error, defaultEffects, defaultEffects);
}

inline bool parseAndValidate(const std::string &source,
                             primec::Program &program,
                             std::string &error,
                             std::vector<std::string> defaultEffects = {}) {
  return parseAndValidate(source, program, nullptr, error, defaultEffects, defaultEffects);
}

inline bool validateProgram(primec::Program &program,
                            const std::string &entry,
                            std::string &error,
                            std::vector<std::string> defaultEffects = {}) {
  primec::Semantics semantics;
  return semantics.validate(program, entry, error, defaultEffects, defaultEffects);
}

inline bool parseValidateAndLower(const std::string &source,
                                  primec::IrModule &module,
                                  std::string &error,
                                  const std::vector<std::string> &defaultEffects,
                                  const std::vector<std::string> &entryDefaultEffects) {
  if (usesStdImportPipeline(source)) {
    PreparedCompilePipelineIrForTesting prepared;
    if (!prepareIrThroughCompilePipeline(
            source, "/main", "vm", prepared, error)) {
      return false;
    }
    module = std::move(prepared.ir);
    return true;
  }

  primec::Program program;
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  if (!parser.parse(program, error)) {
    return false;
  }

  primec::SemanticProgram semanticProgram;
  primec::Semantics semantics;
  if (!semantics.validate(program,
                          "/main",
                          error,
                          defaultEffects,
                          entryDefaultEffects,
                          {},
                          nullptr,
                          false,
                          &semanticProgram)) {
    return false;
  }

  primec::IrLowerer lowerer;
  return lowerer.lower(program, &semanticProgram, "/main", defaultEffects, entryDefaultEffects, module, error);
}

inline bool parseValidateAndLower(const std::string &source,
                                  primec::IrModule &module,
                                  std::string &error,
                                  std::vector<std::string> defaultEffects = {}) {
  return parseValidateAndLower(source, module, error, defaultEffects, defaultEffects);
}

} // namespace
