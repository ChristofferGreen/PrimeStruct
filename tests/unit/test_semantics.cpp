#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>

#include "primec/CompilePipeline.h"
#include "primec/Lexer.h"
#include "primec/Parser.h"
#include "primec/Semantics.h"
#include "primec/testing/SemanticsValidationHelpers.h"

#include "third_party/doctest.h"

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

std::filesystem::path makeTempSemanticSourcePath() {
  static std::atomic<unsigned long long> counter{0};
  const auto nonce = std::to_string(
      static_cast<unsigned long long>(std::chrono::steady_clock::now().time_since_epoch().count()));
  return std::filesystem::temp_directory_path() /
         ("primestruct-semantics-" + nonce + "-" + std::to_string(counter++) + ".prime");
}

bool validateProgramThroughCompilePipeline(const std::string &source,
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
  options.collectDiagnostics = diagnosticInfo != nullptr;
  primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);

  primec::CompilePipelineOutput output;
  primec::CompilePipelineErrorStage errorStage = primec::CompilePipelineErrorStage::None;
  const bool ok = primec::runCompilePipeline(options, output, errorStage, error, diagnosticInfo);

  std::error_code ec;
  std::filesystem::remove(tempPath, ec);
  return ok;
}

bool validateProgramThroughCompilePipeline(const std::string &source,
                                           const std::string &entry,
                                           const std::vector<std::string> &defaultEffects,
                                           const std::vector<std::string> &entryDefaultEffects,
                                           std::string &error,
                                           primec::CompilePipelineDiagnosticInfo *diagnosticInfo = nullptr) {
  return validateProgramThroughCompilePipeline(
      source, entry, defaultEffects, entryDefaultEffects, "native", "wasi", error, diagnosticInfo);
}

primec::Program parseProgram(const std::string &source) {
  std::string error;
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  CHECK(parser.parse(program, error));
  CHECK(error.empty());
  return program;
}

bool parseProgramWithError(const std::string &source, std::string &error) {
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  return parser.parse(program, error);
}

bool validateProgram(const std::string &source, const std::string &entry, std::string &error) {
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  if (usesStdImportPipeline(source)) {
    return validateProgramThroughCompilePipeline(source, entry, defaults, defaults, error);
  }
  auto program = parseProgram(source);
  primec::Semantics semantics;
  return semantics.validate(program, entry, error, defaults, defaults);
}

bool validateProgramWithDefaults(const std::string &source,
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

bool validateProgramForCompileTarget(const std::string &source,
                                     const std::string &entry,
                                     const std::string &emitKind,
                                     const std::string &wasmProfile,
                                     std::string &error) {
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  return validateProgramThroughCompilePipeline(
      source, entry, defaults, defaults, emitKind, wasmProfile, error);
}

bool validateProgramWithDefaults(const std::string &source,
                                 const std::string &entry,
                                 const std::vector<std::string> &defaultEffects,
                                 std::string &error) {
  return validateProgramWithDefaults(source, entry, defaultEffects, defaultEffects, error);
}

bool validateProgramCollectingDiagnostics(const std::string &source,
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

#include "test_semantics_entry_entry.h"
#include "test_semantics_entry_resolution.h"
#include "test_semantics_entry_parameters.h"
#include "test_semantics_entry_return_inference.h"
#include "test_semantics_entry_operators.h"
#include "test_semantics_entry_math_imports.h"
#include "test_semantics_entry_tags.h"
#include "test_semantics_entry_builtins_numeric.h"
#include "test_semantics_numeric_software.h"
#include "test_semantics_entry_executions.h"
#include "test_semantics_entry_transforms.h"
#include "test_semantics_enum.h"
#include "test_semantics_capabilities.h"
#include "test_semantics_bindings_core.h"
#include "test_semantics_bindings_struct_defaults.h"
#include "test_semantics_bindings_pointers.h"
#include "test_semantics_bindings_assignments.h"
#include "test_semantics_bindings_control_flow.h"
#include "test_semantics_struct_helpers.h"
#include "test_semantics_result_helpers.h"
#include "test_semantics_imports.h"
#include "test_semantics_calls_and_flow_control_core.h"
#include "test_semantics_calls_and_flow_control_misc.h"
#include "test_semantics_calls_and_flow_collections.h"
#include "test_semantics_calls_and_flow_access.h"
#include "test_semantics_calls_and_flow_named_args.h"
#include "test_semantics_calls_and_flow_numeric_builtins.h"
#include "test_semantics_calls_and_flow_comparisons_literals.h"
#include "test_semantics_calls_and_flow_effects.h"
#include "test_semantics_traits.h"
#include "test_semantics_lambdas.h"
#include "test_semantics_move.h"
#include "test_semantics_maybe.h"
#include "test_semantics_type_resolution_graph.h"
#include "test_semantics_uninitialized_fields.h"
