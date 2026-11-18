#include "primec/pipeline/LibraryBuild.h"

#include "primec/ir/IrLowerer.h"
#include "primec/ir/IrValidation.h"
#include "InterfaceGenerics.h"
#include "primec/pipeline/LibraryLink.h"
#include "primec/pipeline/ModuleInterface.h"
#include "primec/support/Sha256.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <unordered_map>

namespace primec {
namespace {

bool hasTransformNamed(const Definition &definition, const char *name) {
  return std::any_of(definition.transforms.begin(),
                     definition.transforms.end(),
                     [&](const Transform &transform) { return transform.name == name; });
}

// `/ui/button/draw` is in the root `/ui`.
std::string rootOf(const std::string &path) {
  const size_t slash = path.find('/', 1);
  return slash == std::string::npos ? path : path.substr(0, slash);
}

std::string relativeToRoot(const std::filesystem::path &file,
                           const std::filesystem::path &rootDir) {
  if (!file.is_absolute()) {
    return file.lexically_normal().generic_string();
  }
  return file.lexically_normal().lexically_relative(rootDir).generic_string();
}

// The text a source unit of the stdlib is named by: the part of its path from `stdlib/`, so no checkout path remains.
std::string stdlibName(const std::string &path) {
  const size_t at = path.rfind("/stdlib/");
  return at == std::string::npos
             ? std::string("stdlib/") + std::filesystem::path(path).filename().generic_string()
             : path.substr(at + 1);
}

// The name of each source unit as the archive records it: relative to the library's directory for the module's own
// files, from `stdlib/` for the stdlib, so two checkouts of the same sources name them alike.
std::unordered_map<std::string, std::string> sourceNames(const ExpandedSource &source,
                                                         const std::filesystem::path &rootDir) {
  std::unordered_map<std::string, std::string> names;
  for (const SourceUnit &unit : source.units) {
    if (unit.displayPath.empty()) {
      continue;
    }
    if (unit.kind == SourceUnitKind::Primary || unit.kind == SourceUnitKind::Import) {
      names.emplace(unit.displayPath, relativeToRoot(unit.displayPath, rootDir));
    } else {
      names.emplace(unit.displayPath, stdlibName(unit.displayPath));
    }
  }
  return names;
}

bool readTextFile(const std::filesystem::path &path, std::string &text) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return false;
  }
  text.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
  return !in.bad();
}

} // namespace

bool computeCompilerBuildId(const std::string &executablePath,
                            std::string &id,
                            std::string &error) {
  if (!sha256FileHex(executablePath, id)) {
    error = "cannot read the compiler executable to identify its build: " + executablePath;
    return false;
  }
  return true;
}

bool buildLibraryArchiveContent(CompilePipelineOutput &output,
                                const Options &options,
                                const std::string &compilerBuildId,
                                LibraryArchiveContent &content,
                                std::string &error) {
  if (!output.hasSemanticProgram) {
    error = "library emit requested without semantic product";
    return false;
  }
  if (options.inputPath.empty() || options.inMemorySource.has_value()) {
    error = "--emit=lib needs a root file";
    return false;
  }
  ModuleInterface moduleInterface;
  if (!buildModuleInterface(output.program,
                            output.semanticProgram,
                            output.expandedSource,
                            options.entryPath,
                            libraryNameOfRootFile(options.inputPath),
                            moduleInterface,
                            error,
                            &output.parsedDefinitions)) {
    return false;
  }

  std::unordered_map<std::string, const Definition *> definitions;
  for (const Definition &definition : output.program.definitions) {
    definitions.emplace(definition.fullPath, &definition);
  }
  std::vector<std::string> roots;
  std::set<std::string> rootNames;
  for (const ModuleInterfaceSymbol &symbol : moduleInterface.symbols) {
    rootNames.insert(rootOf(symbol.path));
    if (symbol.kind != "function") {
      continue;
    }
    const auto definition = definitions.find(symbol.path);
    // A host function has no body (it is a host import) and a compute function runs on a GPU: neither is IR code.
    if (definition == definitions.end() || hasTransformNamed(*definition->second, "host") ||
        hasTransformNamed(*definition->second, "compute") ||
        isLifecycleHelper(*definition->second)) {
      continue;
    }
    roots.push_back(symbol.path);
  }
  std::sort(roots.begin(), roots.end());
  roots.erase(std::unique(roots.begin(), roots.end()), roots.end());

  // The VM target: the address ABI, which a function of a library needs for struct and `mut` parameters.
  IrLowerer lowerer;
  IrModule code;
  if (!lowerer.lower(output.program,
                     &output.semanticProgram,
                     options.entryPath,
                     options.defaultEffects,
                     options.entryDefaultEffects,
                     code,
                     error,
                     nullptr,
                     IrValidationTarget::Vm,
                     &output.expandedSource,
                     nullptr,
                     &roots)) {
    return false;
  }
  if (!code.functions.empty()) {
    // The validator wants an entry point; the library has none, so it checks a copy that names its first function.
    IrModule checked = code;
    checked.entryIndex = 0;
    if (!validateIrModule(checked, IrValidationTarget::Vm, error, true)) {
      error = "library code does not validate: " + error;
      return false;
    }
  }

  const std::filesystem::path rootFile =
      std::filesystem::absolute(options.inputPath).lexically_normal();
  const std::filesystem::path rootDir = rootFile.parent_path();
  const std::unordered_map<std::string, std::string> names =
      sourceNames(output.expandedSource, rootDir);
  for (IrInstructionSourceMapEntry &entry : code.instructionSourceMap) {
    if (entry.sourceUnit.empty()) {
      continue;
    }
    const auto name = names.find(entry.sourceUnit);
    entry.sourceUnit =
        name != names.end() ? name->second : relativeToRoot(entry.sourceUnit, rootDir);
  }

  content = {};
  content.manifest.name = libraryNameOfRootFile(options.inputPath);
  content.manifest.roots.assign(rootNames.begin(), rootNames.end());
  content.manifest.libraries = importedLibraries(output.expandedSource);
  content.manifest.compilerBuildId = compilerBuildId;
  content.manifest.interfaceHash = moduleInterface.interfaceHash;
  content.interfaceText = std::move(moduleInterface.text);
  content.symbolsText = formatModuleSymbolTable(moduleInterface);
  content.code = std::move(code);

  if (options.libraryWithSources) {
    content.hasSources = true;
    std::map<std::string, std::string> files;
    for (const SourceUnit &unit : output.expandedSource.units) {
      if ((unit.kind != SourceUnitKind::Primary && unit.kind != SourceUnitKind::Import) ||
          unit.displayPath.empty()) {
        continue;
      }
      std::string text;
      if (!readTextFile(unit.displayPath, text)) {
        error =
            "cannot read source file for the archive: " + relativeToRoot(unit.displayPath, rootDir);
        return false;
      }
      files.emplace(relativeToRoot(unit.displayPath, rootDir), std::move(text));
    }
    for (auto &file : files) {
      content.sources.push_back({file.first, std::move(file.second)});
    }
  }
  return true;
}

} // namespace primec
