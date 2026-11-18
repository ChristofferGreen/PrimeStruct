#pragma once

// The library symbols a compile resolved (`--emit-uses`, docs/ModulesAndLibrariesPlan.md, sections 8 and 11): the
// symbols of the loaded libraries that the program refers to, directly or through a generic it instantiated or a
// private definition that one reaches, each with the hashes the library's symbol table records for it.

#include "primec/ast/Ast.h"
#include "primec/frontend/ExpandedSource.h"
#include "primec/pipeline/LibraryLink.h"

#include <string>
#include <vector>

namespace primec {

struct LibraryUse {
  std::string path;
  std::string library;
  std::string signatureHash;
  std::string contentHash;
};

// The uses of `program`, sorted by path, then library, then signature hash, without duplicates. The roots of the walk
// are the definitions and executions that are not a library's (a definition whose source unit is not a library
// interface and that instantiates no library template); a library definition is a use when a root reaches it through
// names, resolved call targets and type arguments, and what it refers to is followed in turn. Instances of a library
// generic are listed under the generic's path. An overload of a library function is listed with its siblings: the
// compiler numbers overloads on each side separately, so the paths are compared without the `__ov<N>` suffix.
// Definitions of the program that are never reached from `main` count as roots too (they are validated and lowered).
bool collectLibraryUses(const Program &program,
                        const ExpandedSource &source,
                        const std::vector<LoadedLibrary> &libraries,
                        std::vector<LibraryUse> &uses,
                        std::string &error);

// Writes one use per line, `<path> <library> <signature hash> <content hash>`, to `path`. An empty list writes an
// empty file. On failure sets `error` and returns false.
bool writeEmitUsesFile(const std::string &path,
                       const std::vector<LibraryUse> &uses,
                       std::string &error);

// `--emit-uses`: loads `options.libraryPaths`, collects the uses of `program` and writes them to
// `options.emitUsesPath`. Without a library the file is empty.
bool emitLibraryUses(const Options &options,
                     const Program &program,
                     const ExpandedSource &source,
                     std::string &error);

} // namespace primec
