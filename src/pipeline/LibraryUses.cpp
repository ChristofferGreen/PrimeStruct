#include "LibraryUses.h"

#include "DefinitionReferences.h"
#include "primec/pipeline/ModuleInterface.h"
#include "primec/support/SourceLocationMapper.h"

#include <algorithm>
#include <fstream>
#include <map>
#include <set>
#include <tuple>
#include <unordered_map>
#include <unordered_set>

namespace primec {
namespace {

// `/ui/add__ov1` is `/ui/add`: the number of an overload is a compiler detail that differs between the library and
// its consumer.
std::string withoutOverloadSuffix(const std::string &path) {
  const size_t slash = path.rfind('/');
  const size_t name = slash == std::string::npos ? 0 : slash + 1;
  const size_t overload = path.find("__ov", name);
  if (overload == std::string::npos || overload == name) {
    return path;
  }
  for (size_t i = overload + 4; i < path.size(); ++i) {
    if (path[i] < '0' || path[i] > '9') {
      return path;
    }
  }
  return path.substr(0, overload);
}

// The symbols of every loaded library by normalized path.
class LibrarySymbolIndex {
public:
  bool load(const std::vector<LoadedLibrary> &libraries, std::string &error) {
    for (const LoadedLibrary &library : libraries) {
      std::vector<ModuleInterfaceSymbol> symbols;
      if (!parseModuleSymbolTable(library.content.symbolsText, symbols, error)) {
        error = "library " + library.content.manifest.name + ": " + error;
        return false;
      }
      for (ModuleInterfaceSymbol &symbol : symbols) {
        const std::string key = withoutOverloadSuffix(symbol.path);
        byPath_[key].push_back({library.content.manifest.name, std::move(symbol)});
      }
    }
    return true;
  }

  bool contains(const std::string &path) const {
    return byPath_.count(withoutOverloadSuffix(path)) != 0;
  }

  void
  addUses(const std::string &path,
          std::set<std::tuple<std::string, std::string, std::string, std::string>> &uses) const {
    const auto found = byPath_.find(withoutOverloadSuffix(path));
    if (found == byPath_.end()) {
      return;
    }
    for (const Entry &entry : found->second) {
      uses.insert(
          {entry.symbol.path, entry.library, entry.symbol.signatureHash, entry.symbol.contentHash});
    }
  }

private:
  struct Entry {
    std::string library;
    ModuleInterfaceSymbol symbol;
  };
  std::map<std::string, std::vector<Entry>> byPath_;
};

// The path a program definition has in a library's symbol table, or empty when it is not a library's: an instance
// of a library template is named by the template, any other definition must sit in a library interface unit.
std::string librarySymbolPath(const Definition &definition,
                              const LibrarySymbolIndex &index,
                              const SourceLocationMapper &mapper) {
  if (!definition.instanceOfTemplate.empty()) {
    return index.contains(definition.instanceOfTemplate) ? definition.instanceOfTemplate
                                                         : std::string();
  }
  if (!index.contains(definition.fullPath)) {
    return {};
  }
  const std::optional<SourceUnitLocation> location =
      mapper.mapExpandedSourceLocation(definition.sourceLine, definition.sourceColumn);
  if (!location.has_value() || location->unitKind != SourceUnitKind::Library) {
    return {};
  }
  return definition.fullPath;
}

// An execution, as the definition whose references are its own.
Definition executionAsDefinition(const Execution &execution) {
  Definition definition;
  definition.name = execution.name;
  definition.fullPath = execution.fullPath;
  definition.namespacePrefix = execution.namespacePrefix;
  definition.transforms = execution.transforms;
  definition.templateArgs = execution.templateArgs;
  definition.statements = execution.arguments;
  definition.statements.insert(
      definition.statements.end(), execution.bodyArguments.begin(), execution.bodyArguments.end());
  return definition;
}

} // namespace

bool collectLibraryUses(const Program &program,
                        const ExpandedSource &source,
                        const std::vector<LoadedLibrary> &libraries,
                        std::vector<LibraryUse> &uses,
                        std::string &error) {
  uses.clear();
  LibrarySymbolIndex index;
  if (!index.load(libraries, error)) {
    return false;
  }
  const SourceLocationMapper mapper(source);
  std::unordered_set<std::string> definedPaths;
  std::unordered_map<std::string, std::vector<const Definition *>> byPath;
  for (const Definition &definition : program.definitions) {
    definedPaths.insert(definition.fullPath);
    byPath[definition.fullPath].push_back(&definition);
  }

  std::vector<std::string> pending;
  std::set<std::string> visited;
  auto reach = [&](const Definition &definition) {
    for (const std::string &reference : collectDefinitionReferences(definition, definedPaths)) {
      if (visited.insert(reference).second) {
        pending.push_back(reference);
      }
    }
  };
  for (const Definition &definition : program.definitions) {
    if (librarySymbolPath(definition, index, mapper).empty()) {
      reach(definition);
    }
  }
  for (const Execution &execution : program.executions) {
    reach(executionAsDefinition(execution));
  }

  std::set<std::tuple<std::string, std::string, std::string, std::string>> found;
  while (!pending.empty()) {
    const std::string path = pending.back();
    pending.pop_back();
    for (const Definition *definition : byPath[path]) {
      const std::string symbolPath = librarySymbolPath(*definition, index, mapper);
      if (symbolPath.empty()) {
        continue;
      }
      index.addUses(symbolPath, found);
      reach(*definition);
    }
  }
  for (const auto &entry : found) {
    uses.push_back(
        {std::get<0>(entry), std::get<1>(entry), std::get<2>(entry), std::get<3>(entry)});
  }
  std::sort(uses.begin(), uses.end(), [](const LibraryUse &left, const LibraryUse &right) {
    return std::tie(left.path, left.library, left.signatureHash) <
           std::tie(right.path, right.library, right.signatureHash);
  });
  return true;
}

bool writeEmitUsesFile(const std::string &path,
                       const std::vector<LibraryUse> &uses,
                       std::string &error) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out) {
    error = "failed to write --emit-uses file: " + path;
    return false;
  }
  for (const LibraryUse &use : uses) {
    out << use.path << ' ' << use.library << ' ' << use.signatureHash << ' ' << use.contentHash
        << '\n';
  }
  out.flush();
  if (!out) {
    error = "failed to write --emit-uses file: " + path;
    return false;
  }
  return true;
}

bool emitLibraryUses(const Options &options,
                     const Program &program,
                     const ExpandedSource &source,
                     std::string &error) {
  std::vector<LoadedLibrary> libraries;
  std::vector<LibraryUse> uses;
  return loadLibraries(options.libraryPaths, libraries, error) &&
         collectLibraryUses(program, source, libraries, uses, error) &&
         writeEmitUsesFile(options.emitUsesPath, uses, error);
}

} // namespace primec
