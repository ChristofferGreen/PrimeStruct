#include "primec/embed/ScriptEngine.h"

#include "ScriptModule.h"
#include "primec/backend/IrBackendProfiles.h"
#include "primec/ir/Ir.h"
#include "primec/ir/IrPreparation.h"
#include "primec/pipeline/CliDriver.h"
#include "primec/pipeline/CompilePipeline.h"
#include "primec/support/Options.h"

#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <utility>
#include <variant>

namespace primec::embed {

namespace {
#ifndef PRIMESTRUCT_EMBED_INSTALLED_STDLIB_DIR
#define PRIMESTRUCT_EMBED_INSTALLED_STDLIB_DIR ""
#endif
#ifndef PRIMESTRUCT_EMBED_SOURCE_STDLIB_DIR
#define PRIMESTRUCT_EMBED_SOURCE_STDLIB_DIR ""
#endif

// First existing stdlib directory among: explicit, $PRIMESTRUCT_STDLIB, the
// installed copy, the source tree. Empty when none exists.
std::string resolveStdlibDir(const std::string &explicitPath) {
  std::vector<std::string> candidates;
  candidates.push_back(explicitPath);
  if (const char *env = std::getenv("PRIMESTRUCT_STDLIB")) {
    candidates.emplace_back(env);
  }
  candidates.emplace_back(PRIMESTRUCT_EMBED_INSTALLED_STDLIB_DIR);
  candidates.emplace_back(PRIMESTRUCT_EMBED_SOURCE_STDLIB_DIR);
  for (const auto &candidate : candidates) {
    std::error_code ec;
    if (!candidate.empty() && std::filesystem::is_directory(candidate, ec)) {
      return candidate;
    }
  }
  return {};
}

std::string renderFailure(const Options &options, const CliFailure &failure) {
  std::ostringstream out;
  emitCliFailure(out, options, failure);
  return out.str();
}
} // namespace

ScriptEngine::ScriptEngine() = default;

void ScriptEngine::addImportPath(std::string path) { importPaths_.push_back(std::move(path)); }

void ScriptEngine::setEntryPath(std::string entryPath) { entryPath_ = std::move(entryPath); }

void ScriptEngine::setStdlibPath(std::string path) { stdlibPath_ = std::move(path); }

Script ScriptEngine::compileFile(const std::string &path) const { return compile(path, nullptr); }

Script ScriptEngine::compileSource(const std::string &name, const std::string &text) const {
  return compile(name, &text);
}

Script ScriptEngine::compile(const std::string &path, const std::string *text) const {
  Script script;
  script.name_ = path;

  Options options;
  options.emitKind = "vm";
  options.inputPath = path;
  options.entryPath = entryPath_;
  options.importPaths = importPaths_;
  if (text != nullptr) {
    options.inMemorySource = *text;
  }
  if (const std::string stdlibDir = resolveStdlibDir(stdlibPath_); !stdlibDir.empty()) {
    options.importPaths.push_back(stdlibDir);
  }
  addDefaultStdlibInclude(options.inputPath, options.importPaths);

  std::string error;
  CompilePipelineErrorStage stage = CompilePipelineErrorStage::None;
  CompilePipelineDiagnosticInfo diagnosticInfo;
  CompilePipelineResult result = runCompilePipelineResult(options, stage, error, &diagnosticInfo);
  if (const auto *failure = std::get_if<CompilePipelineFailureResult>(&result)) {
    script.diagnostics_ = renderFailure(options, describeCompilePipelineFailure(*failure));
    return script;
  }
  CompilePipelineOutput output = std::move(std::get<CompilePipelineSuccessResult>(result).output);

  auto module = std::make_shared<Script::Module>();
  IrPreparationFailure irFailure;
  const SemanticProgram *semanticProgram = output.hasSemanticProgram ? &output.semanticProgram : nullptr;
  if (!prepareIrModule(output.program,
                       semanticProgram,
                       options,
                       IrValidationTarget::Vm,
                       module->ir,
                       irFailure,
                       &output.expandedSource)) {
    script.diagnostics_ = renderFailure(
        options, describeIrPreparationFailure(irFailure, vmIrBackendDiagnostics(), &normalizeVmLoweringError));
    return script;
  }
  script.module_ = std::move(module);
  script.hostBindings_ = hostBindings_;
  return script;
}

} // namespace primec::embed
