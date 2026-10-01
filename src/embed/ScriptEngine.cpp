#include "primec/embed/ScriptEngine.h"

#include "primec/backend/IrBackendProfiles.h"
#include "primec/ir/Ir.h"
#include "primec/ir/IrPreparation.h"
#include "primec/pipeline/CliDriver.h"
#include "primec/pipeline/CompilePipeline.h"
#include "primec/runtime/Vm.h"
#include "primec/support/Options.h"

#include <sstream>
#include <string_view>
#include <utility>
#include <variant>

namespace primec::embed {

struct Script::Module {
  IrModule ir;
};

namespace {
std::string renderFailure(const Options &options, const CliFailure &failure) {
  std::ostringstream out;
  emitCliFailure(out, options, failure);
  return out.str();
}
} // namespace

ScriptEngine::ScriptEngine() = default;

void ScriptEngine::addImportPath(std::string path) { importPaths_.push_back(std::move(path)); }

void ScriptEngine::setEntryPath(std::string entryPath) { entryPath_ = std::move(entryPath); }

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
  return script;
}

ScriptResult Script::run(const std::vector<std::string> &args) const {
  ScriptResult result;
  if (!valid()) {
    result.diagnostics = diagnostics_.empty() ? "script was not compiled successfully" : diagnostics_;
    return result;
  }
  std::vector<std::string_view> views;
  views.reserve(args.size() + 1);
  views.push_back(name_);
  for (const auto &arg : args) {
    views.push_back(arg);
  }
  Vm vm;
  uint64_t value = 0;
  std::string error;
  if (!vm.execute(module_->ir, value, error, views)) {
    result.diagnostics = "VM error: " + error;
    return result;
  }
  result.ok = true;
  result.exitCode = static_cast<int>(static_cast<int32_t>(value));
  return result;
}

} // namespace primec::embed
