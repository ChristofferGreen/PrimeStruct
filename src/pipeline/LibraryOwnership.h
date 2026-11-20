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

// The namespace root of `path` for a library manifest: the first segment (`/ui/draw` is in `/ui`), and below the
// stdlib the first three (`/std/math/ColorRGB/add` is in `/std/math/ColorRGB`), because several libraries own parts
// of one stdlib module family (`/std/math` holds color and vector) and the roots of two libraries must not overlap.
std::string libraryRootOfPath(const std::string &path);

// The path of a stdlib source file from the stdlib directory (`/a/stdlib/std/math/color.prime` gives
// `std/math/color.prime`), or empty when `file` does not lie in a `stdlib` directory.
std::string stdlibRelativeFile(const std::string &file);

// A stdlib module a linked library stands for: the stdlib pass does not read `file`; it splices `imports` and the
// interface of the library (once, so an `import</std/math>` that did it first is enough).
struct OwnedStdlibModule {
  std::string file;
  std::string library;
  std::vector<std::string> imports;
  // The interface of `library`; owned by the caller's vector of interface units, which outlives the stdlib pass.
  const LibraryInterfaceUnit *interface = nullptr;
};

// The stdlib modules of the manifests of `libraries`, sorted by file; `interfaces` holds one unit per library
// (`libraryInterfaceUnits`).
std::vector<OwnedStdlibModule>
ownedStdlibModules(const std::vector<LoadedLibrary> &libraries,
                   const std::vector<LibraryInterfaceUnit> &interfaces);

// Fails for a definition of the program (a source unit that is not a library interface or the stdlib) at or below
// a root a library owns: `/ui/draw is owned by library ui`.
bool checkProgramDefinesNoOwnedPath(const Program &program,
                                    const ExpandedSource &source,
                                    const std::vector<LibraryRootOwner> &owners,
                                    std::string &error);

} // namespace primec
