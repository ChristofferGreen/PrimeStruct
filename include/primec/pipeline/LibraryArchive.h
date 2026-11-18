#pragma once

// The library archive (`.psa`) of `primec --emit=lib` (docs/ModulesAndLibrariesPlan.md sections 6 and 7,
// docs/spec/integration-and-tooling.md, Library Archive): one deterministic file of named sections. This unit writes
// and reads the container; src/pipeline/LibraryBuild.cpp makes its content from a validated module.

#include "primec/ir/Ir.h"

#include <cstdint>
#include <string>
#include <vector>

namespace primec {

// The archive container version. A reader refuses another one.
constexpr uint32_t LibraryArchiveFormatVersion = 1u;

// A library the archive was linked with (a later slice records them; the first version has none).
struct LibraryManifestDependency {
  std::string name;
  std::string interfaceHash;
};

struct LibraryManifest {
  // The root file's name without its extension.
  std::string name;
  // The namespace roots the library owns, sorted (`/ui`).
  std::vector<std::string> roots;
  // SHA-256 of the compiler executable that built the archive.
  std::string compilerBuildId;
  uint32_t irSchemaVersion = IrSchemaVersion;
  // The hash of every section and of the interface: `sha256`.
  std::string hashScheme = "sha256";
  // The interface hash of the symbol table (`primec --emit=symbols`).
  std::string interfaceHash;
  // Sorted by name.
  std::vector<LibraryManifestDependency> libraries;
};

// A `.prime` file of the module, for the debugger and the box view. `path` is relative to the root file's directory.
struct LibraryArchiveSource {
  std::string path;
  std::string text;
};

struct LibraryArchiveContent {
  LibraryManifest manifest;
  // The interface text and the symbol table JSON of `primec --emit=interface` and `--emit=symbols`.
  std::string interfaceText;
  std::string symbolsText;
  // Unoptimized, no entry point (`entryIndex` -1).
  IrModule code;
  // Present only with `--with-sources`; sorted by path.
  bool hasSources = false;
  std::vector<LibraryArchiveSource> sources;
};

// What a reader requires of an archive.
struct LibraryArchiveExpectations {
  std::string compilerBuildId;
  uint32_t irSchemaVersion = IrSchemaVersion;
};

// The bytes of the archive. Sections are written in name order, each with its SHA-256; nothing depends on the time,
// the machine or a path. Fails when a name in the manifest has whitespace (the manifest is lines of words).
bool serializeLibraryArchive(const LibraryArchiveContent &content,
                             std::vector<uint8_t> &out,
                             std::string &error);

// Loads and verifies an archive: the container, the hash of every section, the compiler build id and IR schema
// version against `expected`, the manifest against the symbol table, and the code. Every failure has its own message,
// and every message about a stale archive says to rebuild the library.
bool deserializeLibraryArchive(const std::vector<uint8_t> &bytes,
                               const LibraryArchiveExpectations &expected,
                               LibraryArchiveContent &out,
                               std::string &error);

bool writeLibraryArchiveFile(const std::string &path,
                             const LibraryArchiveContent &content,
                             std::string &error);
bool readLibraryArchiveFile(const std::string &path,
                            const LibraryArchiveExpectations &expected,
                            LibraryArchiveContent &out,
                            std::string &error);

} // namespace primec
