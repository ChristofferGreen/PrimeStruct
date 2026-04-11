#pragma once

#include <atomic>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "primec/CompilePipeline.h"
#include "primec/Lexer.h"
#include "primec/Parser.h"
#include "primec/Semantics.h"
#include "primec/testing/TestScratch.h"
#include "third_party/doctest.h"

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

inline std::filesystem::path makeTempSemanticSourcePath() {
  static std::atomic<unsigned long long> counter{0};
  const auto nonce =
      std::to_string(static_cast<unsigned long long>(std::chrono::steady_clock::now().time_since_epoch().count()));
  return primec::testing::testScratchPath(
      "semantics/primestruct-semantics-" + nonce + "-" + std::to_string(counter++) + ".prime");
}

enum class SemanticsCompilePipelineSemanticProductIntentForTesting {
  Require,
  SkipForNonConsumingPath,
};

inline void applySemanticsCompilePipelineSemanticProductIntentForTesting(
    primec::Options &options, SemanticsCompilePipelineSemanticProductIntentForTesting intent) {
  options.skipSemanticProductForNonConsumingPath =
      (intent == SemanticsCompilePipelineSemanticProductIntentForTesting::SkipForNonConsumingPath);
}

inline bool validateProgramThroughCompilePipeline(const std::string &source,
                                                  const std::string &entry,
                                                  const std::vector<std::string> &defaultEffects,
                                                  const std::vector<std::string> &entryDefaultEffects,
                                                  const std::string &emitKind,
                                                  const std::string &wasmProfile,
                                                  std::string &error,
                                                  primec::CompilePipelineDiagnosticInfo *diagnosticInfo = nullptr) {
  const std::filesystem::path tempPath = makeTempSemanticSourcePath();
  {
    std::ofstream file(tempPath);
    if (!file) {
      error = "failed to write semantic test source";
      return false;
    }
    file << source;
  }

  primec::Options options;
  options.inputPath = tempPath.string();
  options.entryPath = entry;
  options.emitKind = emitKind;
  options.wasmProfile = wasmProfile;
  options.defaultEffects = defaultEffects;
  options.entryDefaultEffects = entryDefaultEffects;
  options.dumpStage = "ast_semantic";
  applySemanticsCompilePipelineSemanticProductIntentForTesting(
      options, SemanticsCompilePipelineSemanticProductIntentForTesting::SkipForNonConsumingPath);
  options.collectDiagnostics = diagnosticInfo != nullptr;
  primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);

  primec::CompilePipelineOutput output;
  primec::CompilePipelineErrorStage errorStage = primec::CompilePipelineErrorStage::None;
  const bool ok = primec::runCompilePipeline(options, output, errorStage, error, diagnosticInfo);

  std::error_code ec;
  std::filesystem::remove(tempPath, ec);
  return ok;
}

inline bool validateProgramThroughCompilePipeline(const std::string &source,
                                                  const std::string &entry,
                                                  const std::vector<std::string> &defaultEffects,
                                                  const std::vector<std::string> &entryDefaultEffects,
                                                  std::string &error,
                                                  primec::CompilePipelineDiagnosticInfo *diagnosticInfo = nullptr) {
  return validateProgramThroughCompilePipeline(
      source, entry, defaultEffects, entryDefaultEffects, "native", "wasi", error, diagnosticInfo);
}

inline primec::Program parseProgram(const std::string &source) {
  std::string error;
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  CHECK(parser.parse(program, error));
  CHECK(error.empty());
  return program;
}

inline bool parseProgramWithError(const std::string &source, std::string &error) {
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  return parser.parse(program, error);
}

inline bool validateProgram(const std::string &source, const std::string &entry, std::string &error) {
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  if (usesStdImportPipeline(source)) {
    return validateProgramThroughCompilePipeline(source, entry, defaults, defaults, error);
  }
  auto program = parseProgram(source);
  primec::Semantics semantics;
  return semantics.validate(program, entry, error, defaults, defaults);
}

inline bool validateProgramCapturingProgram(const std::string &source,
                                            const std::string &entry,
                                            std::string &error,
                                            primec::Program &programOut) {
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  if (usesStdImportPipeline(source)) {
    const std::filesystem::path tempPath = makeTempSemanticSourcePath();
    {
      std::ofstream file(tempPath);
      if (!file) {
        error = "failed to write semantic test source";
        return false;
      }
      file << source;
    }

    primec::Options options;
    options.inputPath = tempPath.string();
    options.entryPath = entry;
    options.emitKind = "native";
    options.wasmProfile = "wasi";
    options.defaultEffects = defaults;
    options.entryDefaultEffects = defaults;
    options.dumpStage = "ast_semantic";
    applySemanticsCompilePipelineSemanticProductIntentForTesting(
        options, SemanticsCompilePipelineSemanticProductIntentForTesting::SkipForNonConsumingPath);
    primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);

    primec::CompilePipelineOutput output;
    primec::CompilePipelineErrorStage errorStage = primec::CompilePipelineErrorStage::None;
    const bool ok = primec::runCompilePipeline(options, output, errorStage, error, nullptr);
    if (ok) {
      programOut = std::move(output.program);
    }

    std::error_code ec;
    std::filesystem::remove(tempPath, ec);
    return ok;
  }

  programOut = parseProgram(source);
  primec::Semantics semantics;
  return semantics.validate(programOut, entry, error, defaults, defaults);
}

inline bool validateProgramWithDefaults(const std::string &source,
                                        const std::string &entry,
                                        const std::vector<std::string> &defaultEffects,
                                        const std::vector<std::string> &entryDefaultEffects,
                                        std::string &error) {
  if (usesStdImportPipeline(source)) {
    return validateProgramThroughCompilePipeline(
        source, entry, defaultEffects, entryDefaultEffects, error);
  }
  auto program = parseProgram(source);
  primec::Semantics semantics;
  return semantics.validate(program, entry, error, defaultEffects, entryDefaultEffects);
}

inline bool validateProgramForCompileTarget(const std::string &source,
                                            const std::string &entry,
                                            const std::string &emitKind,
                                            const std::string &wasmProfile,
                                            std::string &error) {
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  return validateProgramThroughCompilePipeline(
      source, entry, defaults, defaults, emitKind, wasmProfile, error);
}

inline bool validateProgramWithDefaults(const std::string &source,
                                        const std::string &entry,
                                        const std::vector<std::string> &defaultEffects,
                                        std::string &error) {
  return validateProgramWithDefaults(source, entry, defaultEffects, defaultEffects, error);
}

inline bool validateProgramCollectingDiagnostics(const std::string &source,
                                                 const std::string &entry,
                                                 std::string &error,
                                                 primec::SemanticDiagnosticInfo &diagnosticInfo) {
  if (usesStdImportPipeline(source)) {
    primec::CompilePipelineDiagnosticInfo pipelineDiagnostics;
    const std::vector<std::string> defaults = {"io_out", "io_err"};
    const bool ok = validateProgramThroughCompilePipeline(
        source, entry, defaults, defaults, error, &pipelineDiagnostics);
    diagnosticInfo = pipelineDiagnostics;
    return ok;
  }
  auto program = parseProgram(source);
  primec::Semantics semantics;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  diagnosticInfo = {};
  return semantics.validate(program, entry, error, defaults, defaults, {}, &diagnosticInfo, true);
}
} // namespace
