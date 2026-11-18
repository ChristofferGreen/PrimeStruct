#pragma once

// Compiling against library archives (`--library`, docs/spec/integration-and-tooling.md, Compiling against a
// library): loading and checking the archives, and the interface units the import resolver reads.

#include "primec/frontend/ImportResolver.h"
#include "primec/ir/Ir.h"
#include "primec/pipeline/LibraryArchive.h"
#include "primec/support/Options.h"

#include <string>
#include <vector>

namespace primec {

struct ExpandedSource;

struct LoadedLibrary {
  // The path as given on the command line.
  std::string path;
  LibraryArchiveContent content;
};

// The compiler build id of the running executable (the SHA-256 of the file), computed once per process.
bool currentCompilerBuildId(std::string &id, std::string &error);

// Reads every archive (the container, section hashes, compiler build id and IR schema, see readLibraryArchiveFile)
// and checks the set: a library supplied twice, a library `mid` that was compiled against `base` when `base` is not
// supplied (`library mid needs library base`), and one whose interface differs from the one `mid` recorded
// (`library base changed since library mid was compiled; rebuild`). Sorted by name.
bool loadLibraries(const std::vector<std::string> &paths,
                   std::vector<LoadedLibrary> &out,
                   std::string &error);

// What the import resolver needs of each loaded library.
std::vector<LibraryInterfaceUnit>
libraryInterfaceUnits(const std::vector<LoadedLibrary> &libraries);

// The libraries whose interface the program imported: the units of kind SourceUnitKind::Library of `source`, as
// {name, interface hash}, sorted by name. A library records them as the libraries it was compiled against.
std::vector<LibraryManifestDependency> importedLibraries(const ExpandedSource &source);

// The link of docs/ModulesAndLibrariesPlan.md, section 9, for `--library`: merges the code of every library of
// `options.libraryPaths` into the lowered `module` (the program's entry stays), before validation and optimization. It
// loads the archives again and checks, first, that each is the one the program was compiled against (the interface
// hash of its SourceUnitKind::Library unit in `source`; otherwise `library ui changed since this program was compiled;
// rebuild`), and last that no import is left (`undefined symbol /ui/draw (library ui)`). The stdlib routines a library
// holds as real calls (`/std/...`) are shared with the program's own copy, see IrLinkOptions.
bool linkProgramLibraries(IrModule &module,
                          const Options &options,
                          const ExpandedSource *source,
                          std::string &error);

} // namespace primec
