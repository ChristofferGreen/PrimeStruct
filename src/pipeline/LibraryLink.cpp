#include "primec/pipeline/LibraryLink.h"

#include "primec/frontend/ExpandedSource.h"
#include "primec/ir/IrLinker.h"
#include "primec/pipeline/BuildRunner.h"
#include "primec/pipeline/LibraryBuild.h"

#include <algorithm>
#include <map>
#include <mutex>

namespace primec {

bool currentCompilerBuildId(std::string &id, std::string &error) {
  static std::mutex mutex;
  static std::string cached;
  const std::lock_guard<std::mutex> lock(mutex);
  if (cached.empty() && !computeCompilerBuildId(selfExecutablePath(""), cached, error)) {
    cached.clear();
    return false;
  }
  id = cached;
  return true;
}

bool loadLibraries(const std::vector<std::string> &paths,
                   std::vector<LoadedLibrary> &out,
                   std::string &error) {
  out.clear();
  if (paths.empty()) {
    return true;
  }
  LibraryArchiveExpectations expected;
  if (!currentCompilerBuildId(expected.compilerBuildId, error)) {
    return false;
  }
  for (const std::string &path : paths) {
    LoadedLibrary library;
    library.path = path;
    std::string readError;
    if (!readLibraryArchiveFile(path, expected, library.content, readError)) {
      error = "cannot use library " + path + ": " + readError;
      return false;
    }
    out.push_back(std::move(library));
  }
  std::sort(out.begin(), out.end(), [](const LoadedLibrary &left, const LoadedLibrary &right) {
    return left.content.manifest.name < right.content.manifest.name;
  });
  std::map<std::string, const LoadedLibrary *> byName;
  for (const LoadedLibrary &library : out) {
    if (!byName.emplace(library.content.manifest.name, &library).second) {
      error = "library " + library.content.manifest.name + " is supplied twice";
      return false;
    }
  }
  for (const LoadedLibrary &library : out) {
    for (const LibraryManifestDependency &dependency : library.content.manifest.libraries) {
      const auto found = byName.find(dependency.name);
      if (found == byName.end()) {
        error = "library " + library.content.manifest.name + " needs library " + dependency.name;
        return false;
      }
      if (found->second->content.manifest.interfaceHash != dependency.interfaceHash) {
        error = "library " + dependency.name + " changed since library " +
                library.content.manifest.name + " was compiled; rebuild";
        return false;
      }
    }
  }
  return true;
}

std::vector<LibraryInterfaceUnit>
libraryInterfaceUnits(const std::vector<LoadedLibrary> &libraries) {
  std::vector<LibraryInterfaceUnit> units;
  units.reserve(libraries.size());
  for (const LoadedLibrary &library : libraries) {
    LibraryInterfaceUnit unit;
    unit.name = library.content.manifest.name;
    unit.displayPath = library.path;
    unit.interfaceHash = library.content.manifest.interfaceHash;
    unit.roots = library.content.manifest.roots;
    unit.text = library.content.interfaceText;
    units.push_back(std::move(unit));
  }
  return units;
}

std::vector<LibraryManifestDependency> importedLibraries(const ExpandedSource &source) {
  std::vector<LibraryManifestDependency> libraries;
  for (const SourceUnit &unit : source.units) {
    if (unit.kind != SourceUnitKind::Library) {
      continue;
    }
    const size_t colon = unit.moduleKey.rfind(':');
    if (colon == std::string::npos) {
      continue;
    }
    libraries.push_back({unit.moduleKey.substr(0, colon), unit.moduleKey.substr(colon + 1)});
  }
  std::sort(libraries.begin(),
            libraries.end(),
            [](const LibraryManifestDependency &left, const LibraryManifestDependency &right) {
              return left.name < right.name;
            });
  return libraries;
}

bool linkProgramLibraries(IrModule &module,
                          const Options &options,
                          const ExpandedSource *source,
                          std::string &error) {
  if (options.libraryPaths.empty()) {
    return true;
  }
  std::vector<LoadedLibrary> libraries;
  if (!loadLibraries(options.libraryPaths, libraries, error)) {
    return false;
  }
  if (source != nullptr) {
    for (const LibraryManifestDependency &compiledAgainst : importedLibraries(*source)) {
      const auto library =
          std::find_if(libraries.begin(), libraries.end(), [&](const LoadedLibrary &candidate) {
            return candidate.content.manifest.name == compiledAgainst.name;
          });
      if (library == libraries.end() ||
          library->content.manifest.interfaceHash != compiledAgainst.interfaceHash) {
        error =
            "library " + compiledAgainst.name + " changed since this program was compiled; rebuild";
        return false;
      }
    }
  }
  for (const LoadedLibrary &library : libraries) {
    IrLinkOptions linkOptions;
    const std::vector<std::string> &roots = library.content.manifest.roots;
    if (std::find(roots.begin(), roots.end(), "/std") == roots.end()) {
      linkOptions.sharedRoots.push_back("/std");
    }
    std::string linkError;
    if (!linkIrModule(module, library.content.code, linkError, &linkOptions)) {
      error = "cannot link library " + library.content.manifest.name + ": " + linkError;
      return false;
    }
  }
  return checkIrImportsResolved(module, error);
}

} // namespace primec
