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
