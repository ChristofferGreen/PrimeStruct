#include "LibraryOwnership.h"

#include "primec/support/SourceLocationMapper.h"

#include <algorithm>
#include <optional>

namespace primec {

bool pathIsInRoot(const std::string &path, const std::string &root) {
  return path == root || (path.size() > root.size() && path.compare(0, root.size(), root) == 0 &&
                          path[root.size()] == '/');
}

std::vector<LibraryRootOwner> libraryRootOwners(const std::vector<LoadedLibrary> &libraries) {
  std::vector<LibraryRootOwner> owners;
  for (const LoadedLibrary &library : libraries) {
    for (const std::string &root : library.content.manifest.roots) {
      owners.push_back({root, library.content.manifest.name});
    }
  }
  std::sort(owners.begin(),
            owners.end(),
            [](const LibraryRootOwner &left, const LibraryRootOwner &right) {
              return left.root != right.root ? left.root < right.root
                                             : left.library < right.library;
            });
  return owners;
}

bool checkLibraryRootsDisjoint(const std::vector<LibraryRootOwner> &owners, std::string &error) {
  for (size_t i = 0; i < owners.size(); ++i) {
    for (size_t j = i + 1; j < owners.size(); ++j) {
      if (owners[i].library == owners[j].library) {
        continue;
      }
      if (pathIsInRoot(owners[i].root, owners[j].root) ||
          pathIsInRoot(owners[j].root, owners[i].root)) {
        const std::string &shorter =
            owners[i].root.size() <= owners[j].root.size() ? owners[i].root : owners[j].root;
        error = "library " + owners[i].library + " and library " + owners[j].library +
                " both own " + shorter;
        return false;
      }
    }
  }
  return true;
}

std::string libraryRootOfPath(const std::string &path) {
  size_t segments = path.compare(0, 5, "/std/") == 0 ? 3 : 1;
  size_t end = 0;
  while (segments > 0) {
    end = path.find('/', end + 1);
    if (end == std::string::npos) {
      return path;
    }
    --segments;
  }
  return path.substr(0, end);
}

std::string stdlibRelativeFile(const std::string &file) {
  const std::string marker = "/stdlib/";
  const size_t at = file.rfind(marker);
  return at == std::string::npos ? std::string() : file.substr(at + marker.size());
}

std::vector<OwnedStdlibModule>
ownedStdlibModules(const std::vector<LoadedLibrary> &libraries,
                   const std::vector<LibraryInterfaceUnit> &interfaces) {
  std::vector<OwnedStdlibModule> modules;
  for (const LoadedLibrary &library : libraries) {
    const std::string &name = library.content.manifest.name;
    const auto unit =
        std::find_if(interfaces.begin(),
                     interfaces.end(),
                     [&](const LibraryInterfaceUnit &candidate) { return candidate.name == name; });
    for (const LibraryManifestStdlibModule &module : library.content.manifest.stdlibModules) {
      modules.push_back(
          {module.file, name, module.imports, unit == interfaces.end() ? nullptr : &*unit});
    }
  }
  std::sort(modules.begin(),
            modules.end(),
            [](const OwnedStdlibModule &left, const OwnedStdlibModule &right) {
              return left.file != right.file ? left.file < right.file
                                             : left.library < right.library;
            });
  return modules;
}

bool checkProgramDefinesNoOwnedPath(const Program &program,
                                    const ExpandedSource &source,
                                    const std::vector<LibraryRootOwner> &owners,
                                    std::string &error) {
  if (owners.empty()) {
    return true;
  }
  for (const Definition &definition : program.definitions) {
    const LibraryRootOwner *owner = nullptr;
    for (const LibraryRootOwner &candidate : owners) {
      if (pathIsInRoot(definition.fullPath, candidate.root)) {
        owner = &candidate;
        break;
      }
    }
    if (owner == nullptr) {
      continue;
    }
    const std::optional<SourceUnitLocation> location =
        mapExpandedSourceLocation(source, definition.sourceLine, definition.sourceColumn);
    if (!location.has_value() || (location->unitKind != SourceUnitKind::Primary &&
                                  location->unitKind != SourceUnitKind::Import)) {
      continue;
    }
    error = definition.fullPath + " is owned by library " + owner->library;
    return false;
  }
  return true;
}

} // namespace primec
