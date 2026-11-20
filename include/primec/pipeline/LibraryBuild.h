#pragma once

// `primec --emit=lib`: the content of a library archive from a validated module (docs/ModulesAndLibrariesPlan.md
// section 7, docs/spec/integration-and-tooling.md, Library Archive).

#include "primec/pipeline/CompilePipeline.h"
#include "primec/pipeline/LibraryArchive.h"
#include "primec/support/Options.h"

#include <string>

namespace primec {

// The compiler build id of the running executable at `executablePath` (BuildRunner's selfExecutablePath): its linker
// identifier (`uuid-<hex>`), or the SHA-256 of the file (`sha256-<hex>`) when it has none; see ExecutableBuildId.h. False, with `error` set, when the file cannot be read.
bool computeCompilerBuildId(const std::string &executablePath, std::string &id, std::string &error);

// Builds the archive content of the module `output` holds (a program validated with an entry stub, see
// addInterfaceEntryStub): the interface and symbol table of its public definitions, and the unoptimized IR of every
// public non-generic function and what they call, with no entry point. The name is the root file's name without its
// extension, the roots are the first path segments of the public symbols, and the source map names files relative to
// the root file's directory. `output.program` is not usable afterwards (lowering reads it in place).
bool buildLibraryArchiveContent(CompilePipelineOutput &output,
                                const Options &options,
                                const std::string &compilerBuildId,
                                LibraryArchiveContent &content,
                                std::string &error);

} // namespace primec
