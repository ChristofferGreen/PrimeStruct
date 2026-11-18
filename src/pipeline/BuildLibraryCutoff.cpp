#include "primec/pipeline/BuildLibraryCutoff.h"

#include "primec/pipeline/LibraryArchive.h"

#include <fstream>
#include <set>
#include <sstream>
#include <tuple>

namespace primec {

namespace {

// `/ui/draw__ov2` is the second overload of `/ui/draw`; the compiler numbers overloads on each side separately, so
// the name without the suffix is the one that is compared.
std::string withoutOverloadSuffix(const std::string &path) {
  const size_t marker = path.rfind("__ov");
  if (marker == std::string::npos || marker + 4 >= path.size()) {
    return path;
  }
  for (size_t i = marker + 4; i < path.size(); ++i) {
    if (path[i] < '0' || path[i] > '9') {
      return path;
    }
  }
  return path.substr(0, marker);
}

using HashTriple = std::tuple<std::string, std::string, std::string>;

} // namespace

bool readBuildUsesList(const std::string &path, std::vector<BuildLibraryUse> &uses) {
  uses.clear();
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return false;
  }
  std::string line;
  while (std::getline(file, line)) {
    if (line.empty()) {
      continue;
    }
    std::istringstream words(line);
    BuildLibraryUse use;
    std::string extra;
    if (!(words >> use.path >> use.library >> use.signatureHash >> use.contentHash) ||
        (words >> extra)) {
      uses.clear();
      return false;
    }
    uses.push_back(std::move(use));
  }
  return true;
}

bool loadBuildLibrarySymbols(const std::vector<std::string> &archivePaths,
                             const std::string &compilerBuildId,
                             BuildLibrarySymbols &symbols,
                             std::string &error) {
  symbols.clear();
  LibraryArchiveExpectations expected;
  expected.compilerBuildId = compilerBuildId;
  for (const std::string &path : archivePaths) {
    LibraryArchiveContent content;
    if (!readLibraryArchiveFile(path, expected, content, error)) {
      error = "cannot use library " + path + ": " + error;
      return false;
    }
    std::vector<ModuleInterfaceSymbol> table;
    if (!parseModuleSymbolTable(content.symbolsText, table, error)) {
      error = "cannot use library " + path + ": " + error;
      return false;
    }
    if (!symbols.emplace(content.manifest.name, std::move(table)).second) {
      error = "library " + content.manifest.name + " is supplied twice";
      return false;
    }
  }
  return true;
}

bool completeBuildLibraryUses(std::vector<BuildLibraryUse> &uses,
                              const BuildLibrarySymbols &symbols) {
  for (BuildLibraryUse &use : uses) {
    const auto library = symbols.find(use.library);
    if (library == symbols.end()) {
      return false;
    }
    const std::string name = withoutOverloadSuffix(use.path);
    bool found = false;
    for (const ModuleInterfaceSymbol &symbol : library->second) {
      if (withoutOverloadSuffix(symbol.path) == name && symbol.signatureHash == use.signatureHash &&
          symbol.contentHash == use.contentHash) {
        use.closureHash = symbol.closureHash;
        found = true;
        break;
      }
    }
    if (!found) {
      return false;
    }
  }
  return true;
}

bool buildLibraryUsesUnchanged(const std::vector<BuildLibraryUse> &recorded,
                               const BuildLibrarySymbols &current) {
  // The hashes recorded under each (library, name), and the ones the library has now under the same pair.
  std::map<std::pair<std::string, std::string>, std::multiset<HashTriple>> before;
  for (const BuildLibraryUse &use : recorded) {
    before[{use.library, withoutOverloadSuffix(use.path)}].insert(
        HashTriple(use.signatureHash, use.contentHash, use.closureHash));
  }
  for (const auto &entry : before) {
    const auto library = current.find(entry.first.first);
    if (library == current.end()) {
      return false;
    }
    std::multiset<HashTriple> now;
    for (const ModuleInterfaceSymbol &symbol : library->second) {
      if (withoutOverloadSuffix(symbol.path) == entry.first.second) {
        now.insert(HashTriple(symbol.signatureHash, symbol.contentHash, symbol.closureHash));
      }
    }
    if (now != entry.second) {
      return false;
    }
  }
  return true;
}

} // namespace primec
