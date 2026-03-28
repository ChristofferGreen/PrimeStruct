#pragma once

#include <atomic>
#include <chrono>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include "primec/CompilePipeline.h"
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

inline bool parseAndValidate(const std::string &source,
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

inline bool parseAndValidate(const std::string &source,
                             primec::Program &program,
                             std::string &error,
                             std::vector<std::string> defaultEffects = {}) {
  return parseAndValidate(source, program, error, defaultEffects, defaultEffects);
}

inline bool validateProgram(primec::Program &program,
                            const std::string &entry,
                            std::string &error,
                            std::vector<std::string> defaultEffects = {}) {
  primec::Semantics semantics;
  return semantics.validate(program, entry, error, defaultEffects, defaultEffects);
}

} // namespace
