#pragma once

// Namespace ownership of libraries (docs/ModulesAndLibrariesPlan.md, section 6; `--library`): a library owns the
// namespace roots its manifest lists, and nothing else may define a path below one.

#include "primec/ast/Ast.h"
#include "primec/frontend/ExpandedSource.h"
#include "primec/pipeline/LibraryLink.h"

#include <string>
#include <vector>

namespace primec {

// A namespace root and the library that owns it.
struct LibraryRootOwner {
  std::string root;
  std::string library;
};

// Every root of every library, sorted by root then library.
std::vector<LibraryRootOwner> libraryRootOwners(const std::vector<LoadedLibrary> &libraries);

// Fails when two libraries own overlapping roots (the same root, or one inside the other):
// `library a and library b both own /ui`.
bool checkLibraryRootsDisjoint(const std::vector<LibraryRootOwner> &owners, std::string &error);

// True when `path` is `root` or lies below it (`/ui/draw` is in `/ui`, `/ui2` is not).
bool pathIsInRoot(const std::string &path, const std::string &root);

// Fails for a definition of the program (a source unit that is not a library interface or the stdlib) at or below
// a root a library owns: `/ui/draw is owned by library ui`.
bool checkProgramDefinesNoOwnedPath(const Program &program,
                                    const ExpandedSource &source,
                                    const std::vector<LibraryRootOwner> &owners,
                                    std::string &error);

} // namespace primec
