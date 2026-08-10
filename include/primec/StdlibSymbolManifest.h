#pragma once

// Per-module stdlib symbol manifest plumbing (TODO-5227,
// docs/LibrarySymbolManifestLazyImports.md Phase 2a). Loads the `[symbol]`
// manifest files tools/generate_stdlib_manifest.cpp writes and re-derives a
// manifested symbol's exact source slice from its module's current source,
// re-verifying the recorded content hash before ever handing the slice back
// - a mismatch means the module changed since the manifest was generated,
// and must fail loudly rather than silently splicing stale source.
//
// This header intentionally has no dependency on CompilePipeline.cpp and is
// not yet wired into any import-resolution control flow (that is TODO-5228).

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace primec {

struct StdlibSymbolManifestEntry {
  std::string path;
  int startLine = 0;
  int endLine = 0;
  uint64_t contentHash = 0;
};

// FNV-1a 64-bit. Not cryptographic - only needs to detect content drift.
uint64_t fnv1a64(std::string_view text);

// Splits text into 1-indexed lines (index 0 is unused/empty), so manifest
// line numbers map directly onto vector indices.
std::vector<std::string> splitSourceIntoLines(const std::string &text);

// Applies the same TextFilterPipeline pass runCompilePipelineTransformStage
// runs before lexing (default collections/operators/implicit-utf8 filters),
// so Definition::sourceLine/sourceColumn - and therefore manifest line
// numbers derived from them - line up with the returned text.
bool applyDefaultStdlibTextFilter(const std::string &rawSource,
                                  std::string &filteredSource,
                                  std::string &error);

// Wraps an isolated definition slice in `namespace a { namespace b { ... }
// }` nesting matching fullPath's parent segments, so it resolves to the
// same fullPath when reparsed standalone. Harmless no-op for definitions
// whose own name is already an absolute `/a/b/c` path (Parser::makeFullPath
// ignores the wrapping prefix for those), since the parent-segment guess is
// then unused by the reparse.
std::string wrapDefinitionInNamespace(std::string_view fullPath, const std::string &body);

// Reads a `[symbol]`-block manifest file (path/start_line/end_line/
// content_hash keys, `#` comments), as written by
// tools/generate_stdlib_manifest.cpp. Returns false with `error` set on any
// malformed entry; a missing file is reported as an error too (unlike the
// `[module]`-block std/modules.psmeta reader, callers here always know a
// specific manifest is expected to exist).
bool readStdlibSymbolManifest(const std::string &manifestPath,
                              std::vector<StdlibSymbolManifestEntry> &out,
                              std::string &error);

// Re-derives a manifested symbol's exact source slice from its module's
// current source file (raw read -> default text filter -> line slice) and
// re-verifies its content hash (reparse standalone, print canonical AST,
// hash) before returning it. Fails loudly on any mismatch.
bool extractAndVerifyManifestedSymbolSource(const StdlibSymbolManifestEntry &entry,
                                            const std::string &moduleSourcePath,
                                            std::string &outSourceText,
                                            std::string &error);

} // namespace primec
