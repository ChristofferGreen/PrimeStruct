#pragma once

// The symbol-level rebuild decision of a compile step that links libraries (docs/ModulesAndLibrariesPlan.md, section
// 11). The step's cache entry records the library symbols its last compile used (`--emit-uses`) with their hashes in
// the library's symbol table. The archives themselves are not dependencies of the step: it is up to date when every
// symbol it used still has the hashes it had, whatever else a library changed.
//
// The closure hash of the symbol table is recorded too, because a use list names the public symbols only and a body
// change of a private function that a used function calls moves the closure hash and nothing else. What the decision
// leaves alone is the code of library symbols the program cannot reach, so the output of an up-to-date step can differ
// from a fresh link by dead library code.

#include "primec/pipeline/BuildCache.h"
#include "primec/pipeline/ModuleInterface.h"

#include <map>
#include <string>
#include <vector>

namespace primec {

// The symbols the archives export now, by library name (the name in the archive's manifest).
using BuildLibrarySymbols = std::map<std::string, std::vector<ModuleInterfaceSymbol>>;

// Reads a `--emit-uses` file: `<path> <library> <signature hash> <content hash>` per line. The closure hashes are
// left empty (see completeBuildLibraryUses). False when the file is missing or a line is malformed.
bool readBuildUsesList(const std::string &path, std::vector<BuildLibraryUse> &uses);

// Loads the archives at `archivePaths` (verified for the compiler build `compilerBuildId`) and reads their symbol
// tables. False, with `error` set, when an archive cannot be used; the caller then runs the step, whose compile reports
// the problem.
bool loadBuildLibrarySymbols(const std::vector<std::string> &archivePaths,
                             const std::string &compilerBuildId,
                             BuildLibrarySymbols &symbols,
                             std::string &error);

// Fills the closure hash of every use from `symbols`. False when a use matches no symbol (by library, path without an
// overload suffix, signature and content hash).
bool completeBuildLibraryUses(std::vector<BuildLibraryUse> &uses,
                              const BuildLibrarySymbols &symbols);

// True when every recorded use still has its hashes: for each library and function name the uses mention, the symbols
// of that library under the name (overloads share one name, without their `__ov<N>` suffix) have exactly the recorded
// signature, content and closure hashes. A removed, added or changed symbol under a used name is a change; a symbol
// under a name the step did not use is not looked at.
bool buildLibraryUsesUnchanged(const std::vector<BuildLibraryUse> &recorded,
                               const BuildLibrarySymbols &current);

} // namespace primec
