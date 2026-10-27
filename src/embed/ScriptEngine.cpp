#include "primec/embed/ScriptEngine.h"

#include "ScriptModule.h"
#include "primec/backend/IrBackendProfiles.h"
#include "primec/ir/Ir.h"
#include "primec/ir/IrPreparation.h"
#include "primec/pipeline/CliDriver.h"
#include "primec/pipeline/CompilePipeline.h"
#include "primec/support/Options.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
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

void ScriptEngine::exportFunction(std::string name, std::vector<HostType> parameters, HostType returnType) {
  for (ExportDecl &decl : exports_) {
    if (decl.name == name) {
      decl.signature = ExportSignature{std::move(parameters), returnType};
      return;
    }
  }
  exports_.push_back(ExportDecl{std::move(name), ExportSignature{std::move(parameters), returnType}});
}

Script ScriptEngine::compileFile(const std::string &path) const {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    Script script;
    script.name_ = path;
    script.diagnostics_ = "failed to read input: " + std::filesystem::absolute(path).string() + "\n";
    return script;
  }
  const std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  return compile(path, &text);
}

Script ScriptEngine::compileSource(const std::string &name, const std::string &text) const {
  return compile(name, &text);
}

namespace {
// Generated entry that calls export `name`, fetching arguments through the
// reserved __psarg_<type> host functions and reporting through __psret_<type>.
// Text is the stdlib `String` (the exported function declares `[String]` parameters
// and a `String` result); every other type is its own spelling.
std::string scriptTypeOf(HostType type) {
  return type == HostType::String ? "/std/text/String" : detail::hostTypeSpelling(type);
}

std::string exportWrapperSource(const std::string &name, const ExportSignature &signature, size_t index) {
  std::string source = "\n";
  std::vector<HostType> seen;
  bool usesText = signature.returnType == HostType::String;
  for (const HostType type : signature.parameters) {
    usesText = usesText || type == HostType::String;
    if (std::find(seen.begin(), seen.end(), type) != seen.end()) {
      continue;
    }
    seen.push_back(type);
    source += "[host return<" + scriptTypeOf(type) + ">]\n" + detail::ExportArgPrefix + detail::exportSpelling(type) +
              "([i32] index) {\n}\n\n";
  }
  const std::string returnType = scriptTypeOf(signature.returnType);
  if (signature.returnType != HostType::Void) {
    source += "[host return<void>]\n" + std::string(detail::ExportResultPrefix) +
              detail::exportSpelling(signature.returnType) + "([" + returnType + "] value) {\n}\n\n";
  }
  // Arguments and a text result go through local bindings, so the String owns its block.
  std::string locals;
  std::string call = name + "(";
  for (size_t i = 0; i < signature.parameters.size(); ++i) {
    const HostType type = signature.parameters[i];
    locals += "  [" + scriptTypeOf(type) + "] __psa" + std::to_string(i) + "{" + detail::ExportArgPrefix +
              detail::exportSpelling(type) + "(" + std::to_string(i) + "i32)}\n";
    call += (i == 0 ? "" : ", ");
    call += "__psa" + std::to_string(i);
  }
  call += ")";
  source += std::string(usesText ? "[return<int> effects(heap_alloc)]" : "[return<int>]") + "\n__ps_call_" +
            std::to_string(index) + "() {\n" + locals;
  if (signature.returnType == HostType::Void) {
    source += "  " + call + "\n";
  } else if (signature.returnType == HostType::String) {
    source += "  [" + returnType + "] __psr{" + call + "}\n  " + std::string(detail::ExportResultPrefix) +
              detail::exportSpelling(signature.returnType) + "(__psr)\n";
  } else {
    source += "  " + std::string(detail::ExportResultPrefix) + detail::exportSpelling(signature.returnType) + "(" +
              call + ")\n";
  }
  source += "  return(0i32)\n}\n";
  return source;
}
} // namespace

bool ScriptEngine::compileEntry(const std::string &path,
                                const std::string &text,
                                const std::string &entryPath,
                                std::shared_ptr<Script::Module> &module,
                                std::string &diagnostics) const {
  Options options;
  options.emitKind = "vm";
  options.inputPath = path;
  options.entryPath = entryPath;
  options.importPaths = importPaths_;
  options.inMemorySource = text;
  // An explicit stdlib wins; the checkout the script's path happens to sit in would add the same modules a second time.
  if (const std::string stdlibDir = resolveStdlibDir(stdlibPath_); !stdlibDir.empty()) {
    options.importPaths.push_back(stdlibDir);
  } else {
    addDefaultStdlibInclude(options.inputPath, options.importPaths);
  }

  std::string error;
  CompilePipelineErrorStage stage = CompilePipelineErrorStage::None;
  CompilePipelineDiagnosticInfo diagnosticInfo;
  CompilePipelineResult result = runCompilePipelineResult(options, stage, error, &diagnosticInfo);
  if (const auto *failure = std::get_if<CompilePipelineFailureResult>(&result)) {
    diagnostics = renderFailure(options, describeCompilePipelineFailure(*failure));
    return false;
  }
  CompilePipelineOutput output = std::move(std::get<CompilePipelineSuccessResult>(result).output);

  module = std::make_shared<Script::Module>();
  IrPreparationFailure irFailure;
  const SemanticProgram *semanticProgram = output.hasSemanticProgram ? &output.semanticProgram : nullptr;
  if (!prepareIrModule(output.program,
                       semanticProgram,
                       options,
                       IrValidationTarget::Vm,
                       module->ir,
                       irFailure,
                       &output.expandedSource)) {
    diagnostics = renderFailure(
        options, describeIrPreparationFailure(irFailure, vmIrBackendDiagnostics(), &normalizeVmLoweringError));
    module.reset();
    return false;
  }
  return true;
}

Script ScriptEngine::compile(const std::string &path, const std::string *text) const {
  Script script;
  script.name_ = path;
  const std::string source = text != nullptr ? *text : std::string();

  std::shared_ptr<Script::Module> mainModule;
  std::string diagnostics;
  const bool mainOk = compileEntry(path, source, entryPath_, mainModule, diagnostics);
  const bool missingMain = !mainOk && diagnostics.find("missing entry definition " + entryPath_) != std::string::npos;
  if (!mainOk && !(missingMain && !exports_.empty())) {
    script.diagnostics_ = diagnostics;
    return script;
  }

  if (!exports_.empty()) {
    auto table = std::make_shared<Script::ExportTable>();
    for (size_t i = 0; i < exports_.size(); ++i) {
      Script::ExportTable::Entry entry;
      entry.name = exports_[i].name;
      entry.signature = exports_[i].signature;
      std::shared_ptr<Script::Module> module;
      std::string exportDiagnostics;
      const std::string entryName = "/__ps_call_" + std::to_string(i);
      if (!compileEntry(path, source + exportWrapperSource(entry.name, entry.signature, i), entryName, module,
                        exportDiagnostics)) {
        script.diagnostics_ = "export '" + entry.name + "': " + exportDiagnostics;
        return script;
      }
      entry.module = std::move(module);
      table->entries.push_back(std::move(entry));
    }
    script.exports_ = std::move(table);
  }
  if (mainOk) {
    script.module_ = std::move(mainModule);
  }
  script.hostBindings_ = hostBindings_;
  return script;
}

} // namespace primec::embed
