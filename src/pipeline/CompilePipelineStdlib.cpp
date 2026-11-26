#include "primec/pipeline/CompilePipeline.h"
#include "primec/support/CompileContext.h"

#include "../frontend/ExpandedSourceBuilder.h"

#include "primec/ast/AstMemory.h"
#include "primec/ast/AstPrinter.h"
#include "primec/frontend/ImportResolver.h"
#include "primec/backend/IrBackendProfiles.h"
#include "primec/ir/IrPrinter.h"
#include "primec/frontend/Lexer.h"
#include "primec/frontend/Parser.h"
#include "primec/semantics/Semantics.h"
#include "primec/ir/StdlibCollectionPaths.h"
#include "primec/semantics/SemanticsBenchmark.h"
#include "primec/support/SourceLocationMapper.h"
#include "primec/support/StdlibSurfaceRegistry.h"
#include "primec/frontend/StdlibSymbolManifest.h"
#include "primec/support/TextFilterPipeline.h"
#include "primec/support/TransformRules.h"
#include "../semantics/TypeResolutionGraph.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include "primec/support/BenchmarkSink.h"
#include "CompilePipelineInternal.h"

namespace primec {
namespace compile_pipeline_detail {


// True when `text` contains `needle` (lowercase ASCII) in any letter case, e.g.
// "soa" matches every soa spelling. Conservative on
// purpose: a false positive only keeps the module in the compile.
bool sourceMentionsCaseInsensitive(const std::string &text, std::string_view needle) {
  if (needle.empty() || text.size() < needle.size()) {
    return false;
  }
  for (std::size_t i = 0; i + needle.size() <= text.size(); ++i) {
    std::size_t k = 0;
    while (k < needle.size() &&
           (text[i + k] | 0x20) == needle[k]) {
      ++k;
    }
    if (k == needle.size()) {
      return true;
    }
  }
  return false;
}

bool shouldSkipMathWildcardStdlibModule(const std::vector<std::string> &sourceImports,
                                        const std::string &source) {
  bool hasMathWildcardImport = false;
  for (const auto &importPath : sourceImports) {
    if (importPath == "/std/math/*") {
      hasMathWildcardImport = true;
      break;
    }
  }
  if (!hasMathWildcardImport) {
    return false;
  }
  return !sourceReferencesNonBuiltinMathSymbols(source);
}

// The module of `ownedModules` whose stdlib file is `absoluteFile`, or null.
static const OwnedStdlibModule *
findOwnedStdlibModule(const std::vector<OwnedStdlibModule> *ownedModules,
                      const std::string &absoluteFile) {
  if (ownedModules == nullptr || ownedModules->empty()) {
    return nullptr;
  }
  const std::string relative = stdlibRelativeFile(absoluteFile);
  for (const OwnedStdlibModule &module : *ownedModules) {
    if (module.file == relative) {
      return &module;
    }
  }
  return nullptr;
}

bool appendStdlibModuleSources(const std::vector<std::string> &importPaths,
                               const std::vector<std::string> &sourceImports,
                               const std::vector<std::string> &implicitKeys,
                               std::string &source,
                               std::string &error,
                               ExpandedSource *expandedSource,
                               const std::unordered_set<std::string> &excludedKeys,
                               std::string *unresolvedImport,
                               const std::vector<OwnedStdlibModule> *ownedModules,
                               std::unordered_set<std::string> *lazyKeysImportedByModules) {
  std::error_code ec;
  std::deque<std::string> pendingKeys;
  std::unordered_set<std::string> queuedKeys;
  std::unordered_map<std::string, StdlibModuleManifest> moduleManifestCache;
  // TODO-5241/5242: collectStdlibAutoIncludeKeys returns the most-specific
  // key first (the literal import path, with any trailing wildcard suffix
  // trimmed) followed by progressively shorter ancestor keys down to the
  // module root. Those ancestor keys are needed for the lazy-exclusion check
  // just below (if ANY ancestor belongs to a lazy-excluded module family,
  // the whole import path must not be queued for whole-file inclusion at
  // all), but they must NOT all be queued for *actual splicing*
  // unconditionally - only as a fallback for the case where the
  // most-specific key doesn't resolve to a real file/directory (e.g. a
  // typo'd sub-path, where letting the ancestor directory scan run produces
  // a better "unknown import path" diagnostic downstream than a spurious
  // "stdlib import requested but matching stdlib modules were not found"
  // error). Queuing every ancestor unconditionally meant any single-file
  // collections submodule import (vector, buffer_checked, map, ...) - each
  // of which resolves cleanly on its own to its own one file - ALSO spliced
  // in the entire parent collections directory: 237KB of unrelated
  // soa_storage.prime alone, on every one of those submodule imports. See
  // docs/TestRuntimeOptimization.md's TODO-5241 entry for the measured
  // breakdown. `fallbackKeyOf` records each key's immediate broader ancestor
  // (keys[i] -> keys[i+1]); a key is only actually enqueued when its more
  // specific sibling turns out not to resolve to anything (see the
  // `keyResolved` tracking in the resolve loop below).
  //
  // `applyMathSkip` must only be true for the top-level sourceImports loop:
  // shouldSkipMathWildcardStdlibModule decides based on the *user's own*
  // source text, not any particular stdlib file's text, so it must never
  // suppress a math import discovered while scanning a stdlib file's own
  // nested imports (e.g. gfx.prime's own `/std/math/*` import, needed for
  // gfx.prime's own use of math types regardless of whether the user's
  // program textually mentions math symbols itself).
  std::unordered_map<std::string, std::string> fallbackKeyOf;
  const bool skipMathWildcardStdlibModule = shouldSkipMathWildcardStdlibModule(sourceImports, source);
  // TODO-5378: the base `/std/collections/*` wildcard scans the whole directory,
  // which includes soa.prime and the 237KB generated soa_storage.prime - about
  // 85% of the collections source and ~0.27s of every compile. No other stdlib
  // module depends on them, so when the user's own text never mentions "soa"
  // (any case, which covers every soa spelling) the wildcard does not
  // need to splice them. Decided once from the user's source, before any stdlib
  // text is appended.
  const bool skipSoaInCollectionsWildcard = !sourceMentionsCaseInsensitive(source, "soa");
  // Same for map.prime (nothing else in the stdlib imports it): skipped when the
  // user's text never mentions "map" in any case.
  const bool skipMapInCollectionsWildcard = !sourceMentionsCaseInsensitive(source, "map");
  const bool skipRingBufferInCollectionsWildcard = !sourceMentionsCaseInsensitive(source, "ring");
  auto queueKeyChain = [&](const std::vector<std::string> &keys, const std::string &importPath,
                           bool applyMathSkip) {
    for (std::size_t i = 0; i + 1 < keys.size(); ++i) {
      fallbackKeyOf.emplace(keys[i], keys[i + 1]);
    }
    if (keys.empty()) {
      return;
    }
    const std::string &mostSpecific = keys.front();
    if (applyMathSkip && skipMathWildcardStdlibModule && importPath == "/std/math/*" &&
        mostSpecific == "/std/math") {
      return;
    }
    if (queuedKeys.insert(mostSpecific).second) {
      pendingKeys.push_back(mostSpecific);
    }
  };
  for (const auto &importPath : sourceImports) {
    const std::vector<std::string> keys = collectStdlibAutoIncludeKeys(importPath);
    const bool importPathIsLazyExcluded =
        std::any_of(keys.begin(), keys.end(), [&](const std::string &key) {
          return excludedKeys.count(key) > 0;
        });
    if (importPathIsLazyExcluded) {
      continue;
    }
    queueKeyChain(keys, importPath, /*applyMathSkip=*/true);
  }
  for (const auto &key : implicitKeys) {
    if (excludedKeys.count(key) > 0) {
      continue;
    }
    if (queuedKeys.insert(key).second) {
      pendingKeys.push_back(key);
    }
  }
  if (pendingKeys.empty()) {
    return true;
  }

  std::unordered_set<std::string> seenFiles;
  std::unordered_set<std::string> processedKeys;
  // The root file and the files the import resolver spliced from `import<...>` entries, by canonical path.
  const auto pathKey = [](const std::filesystem::path &path) {
    std::error_code keyEc;
    const std::filesystem::path canonical = std::filesystem::weakly_canonical(path, keyEc);
    return keyEc ? path.string() : canonical.string();
  };
  std::unordered_set<std::string> resolverSplicedFiles;
  if (expandedSource != nullptr) {
    for (const SourceUnit &unit : expandedSource->units) {
      // The root file counts too: a stdlib file named as the root is in the source once, and the module scan of an
      // import it makes of its own directory must not read it a second time.
      if (unit.kind == SourceUnitKind::Import || unit.kind == SourceUnitKind::Primary) {
        resolverSplicedFiles.insert(pathKey(unit.displayPath));
      }
    }
  }
  std::optional<ExpandedSourceBuilder> sourceBuilder;
  if (expandedSource != nullptr) {
    sourceBuilder.emplace(*expandedSource);
  }
  bool appended = false;
  auto manifestForStdlibRoot = [&](const std::filesystem::path &root)
      -> const StdlibModuleManifest * {
    std::error_code absoluteEc;
    std::filesystem::path absolute = std::filesystem::absolute(root, absoluteEc);
    if (absoluteEc) {
      absolute = root;
    }
    const std::string cacheKey = absolute.string();
    auto existing = moduleManifestCache.find(cacheKey);
    if (existing != moduleManifestCache.end()) {
      return &existing->second;
    }
    StdlibModuleManifest manifest;
    if (!readStdlibModuleManifest(root, manifest, error)) {
      return nullptr;
    }
    auto inserted = moduleManifestCache.emplace(cacheKey, std::move(manifest)).first;
    return &inserted->second;
  };

  while (!pendingKeys.empty()) {
    const std::string key = pendingKeys.front();
    pendingKeys.pop_front();
    if (!processedKeys.insert(key).second) {
      continue;
    }
    // A lazy module is read by the closure scan, which also covers the text of the modules spliced here.
    if (excludedKeys.count(key) > 0) {
      if (lazyKeysImportedByModules != nullptr) {
        lazyKeysImportedByModules->insert(key);
      }
      continue;
    }
    bool keyResolved = false;

    for (const auto &pathText : importPaths) {
      std::filesystem::path root(pathText);
      if (root.filename() != "stdlib") {
        continue;
      }
      if (!std::filesystem::exists(root, ec) || !std::filesystem::is_directory(root, ec)) {
        continue;
      }

      const StdlibModuleManifest *moduleManifest = manifestForStdlibRoot(root);
      if (moduleManifest == nullptr && !error.empty()) {
        return false;
      }

      std::filesystem::path moduleRoot;
      bool appendSpecificFile = false;
      if (moduleManifest != nullptr) {
        auto manifestEntry = moduleManifest->sourceFilesByRoot.find(key);
        if (manifestEntry != moduleManifest->sourceFilesByRoot.end()) {
          moduleRoot = root / manifestEntry->second;
          appendSpecificFile = true;
        }
      }
      if (!appendSpecificFile) {
        // The bare "/std" root names no module; the parser reports it.
        if (key.size() <= std::string("/std/").size()) {
          continue;
        }
        const std::string relative = key.substr(std::string("/std/").size());
        moduleRoot = root / "std" / relative;
      }
      if (appendSpecificFile &&
          (!std::filesystem::exists(moduleRoot, ec) ||
           !std::filesystem::is_regular_file(moduleRoot, ec))) {
        error = "stdlib module manifest source not found for " + key + ": " +
                moduleRoot.string();
        return false;
      }
      if (!appendSpecificFile && !std::filesystem::exists(moduleRoot, ec)) {
        std::filesystem::path moduleFile = moduleRoot;
        moduleFile += ".prime";
        if (existsWithExactCase(moduleFile, ec)) {
          moduleRoot = std::move(moduleFile);
        } else {
          continue;
        }
      }

      // `importsOnly`: splice just the file's `import` lines (see
      // skipSoaInCollectionsWildcard above). A merged stdlib file's own imports
      // are visible to the whole program ("known architectural gap", locked by
      // tests such as the bare vector-count import test), so skipping a
      // module's definitions must not change which import paths are in scope.
      // Imports of soa_storage are dropped when soa is skipped, since that
      // module is not loaded either.
      auto appendFile = [&](const std::filesystem::path &filePath, bool importsOnly = false) -> bool {
        std::filesystem::path absolute = std::filesystem::absolute(filePath, ec);
        if (ec) {
          absolute = filePath;
        }
        const std::string absoluteText = absolute.string();
        // A module a linked library stands for: its source is not read. Its interface is spliced
        // (once: `import</root>` may have done it), and the stdlib imports of the module are what the source would
        // have brought.
        if (const OwnedStdlibModule *owned = findOwnedStdlibModule(ownedModules, absoluteText)) {
          if (!seenFiles.insert(absoluteText + "#owned").second) {
            return true;
          }
          if (sourceBuilder.has_value() && owned->interface != nullptr) {
            sourceBuilder->appendLibraryInterfaceOnce(*owned->interface);
          }
          for (const std::string &nestedImport : owned->imports) {
            const std::string line = "\nimport " + nestedImport + "\n";
            if (sourceBuilder.has_value()) {
              sourceBuilder->appendGenerated(line, "<library-stdlib-import>");
            } else {
              source.append(line);
            }
            queueKeyChain(collectStdlibAutoIncludeKeys(nestedImport),
                          nestedImport,
                          /*applyMathSkip=*/false);
          }
          appended = true;
          return true;
        }
        if (!seenFiles.insert(importsOnly ? absoluteText + "#imports" : absoluteText).second) {
          return true;
        }
        std::ifstream file(absoluteText);
        if (!file) {
          error = "failed to read stdlib file: " + absoluteText;
          return false;
        }
        std::ostringstream buffer;
        buffer << file.rdbuf();
        std::string contents = buffer.str();
        if (importsOnly) {
          std::string kept;
          std::istringstream lines(contents);
          std::string line;
          while (std::getline(lines, line)) {
            const std::size_t first = line.find_first_not_of(" \t");
            if (first == std::string::npos || line.compare(first, 7, "import ") != 0) {
              continue;
            }
            if (skipSoaInCollectionsWildcard && line.find("/soa_storage/") != std::string::npos) {
              continue;
            }
            kept += line;
            kept += '\n';
          }
          contents = std::move(kept);
        }
        // A file the import resolver already spliced (`import</std/math>` without a library) is in the source
        // once: appending it again would define everything in it twice. Its nested imports are
        // still queued below, since the resolver expands `import<...>` only.
        if (resolverSplicedFiles.count(pathKey(absolute)) > 0) {
          // Nothing to append.
        } else if (sourceBuilder.has_value()) {
          sourceBuilder->appendGenerated("\n", "<stdlib-separator>");
          const std::size_t unitId =
              sourceBuilder->addUnit(SourceUnitKind::Stdlib, absoluteText, key, 1, 1);
          sourceBuilder->appendSegment(unitId, contents, 1, 1);
        } else {
          source.append("\n");
          source.append(contents);
        }
        appended = true;

        const std::vector<std::string> nestedImports = collectStdImportPaths(contents);
        for (const auto &nestedImport : nestedImports) {
          // Same most-specific-key-only queuing as the top-level sourceImports
          // loop above (see TODO-5241/5242 comment there) - a stdlib file's
          // own nested imports must not unconditionally drag in their
          // ancestor module directory either. applyMathSkip is false here:
          // the math-wildcard skip decision is about the user's own source,
          // never about what a stdlib file the user didn't write imports.
          queueKeyChain(collectStdlibAutoIncludeKeys(nestedImport), nestedImport,
                       /*applyMathSkip=*/false);
        }
        return true;
      };

      if (appendSpecificFile || std::filesystem::is_regular_file(moduleRoot, ec)) {
        keyResolved = true;
        if (moduleRoot.extension() == ".prime" && !appendFile(moduleRoot)) {
          return false;
        }
        continue;
      }

      if (!std::filesystem::is_directory(moduleRoot, ec)) {
        continue;
      }
      keyResolved = true;

      const bool skipExperimentalCollectionsInBaseWildcard =
          (key == "/std/collections");

      std::filesystem::path siblingModuleFile = moduleRoot;
      siblingModuleFile += ".prime";
      if (std::filesystem::exists(siblingModuleFile, ec) &&
          std::filesystem::is_regular_file(siblingModuleFile, ec)) {
        if (!appendFile(siblingModuleFile)) {
          return false;
        }
      }

      for (const auto &entry : std::filesystem::recursive_directory_iterator(moduleRoot, ec)) {
        if (ec) {
          error = "failed to scan stdlib module: " + moduleRoot.string();
          return false;
        }
        if (!entry.is_regular_file(ec) || entry.path().extension() != ".prime") {
          continue;
        }
        if (skipExperimentalCollectionsInBaseWildcard) {
          const std::string stem = entry.path().stem().string();
          if (stem.rfind(collection_paths::kExperimentalFolderPrefix, 0) == 0) {
            continue;
          }
          // soa_storage.prime plus the files of the soa_storage/ directory form one module.
          const bool inSoaStorageDirectory = entry.path().parent_path().filename() == "soa_storage";
          const bool skipThisFile =
              (skipSoaInCollectionsWildcard &&
               (stem == "soa" || stem == "soa_storage" || inSoaStorageDirectory)) ||
              (skipMapInCollectionsWildcard && stem == "map") ||
              (skipRingBufferInCollectionsWildcard && stem == "ring_buffer");
          if (skipThisFile) {
            if (!appendFile(entry.path(), /*importsOnly=*/true)) {
              return false;
            }
            continue;
          }
        }
        if (!appendFile(entry.path())) {
          return false;
        }
      }
    }
    if (!keyResolved) {
      const auto fallback = fallbackKeyOf.find(key);
      if (fallback != fallbackKeyOf.end() && queuedKeys.insert(fallback->second).second) {
        pendingKeys.push_back(fallback->second);
      }
    }
  }
  if (!appended) {
    error = "stdlib import requested but matching stdlib modules were not found";
    // Name the first import that asked for a module, so the caller can point at it.
    for (const auto &importPath : sourceImports) {
      if (!collectStdlibAutoIncludeKeys(importPath).empty()) {
        error += ": import " + importPath;
        if (unresolvedImport != nullptr) {
          *unresolvedImport = importPath;
        }
        break;
      }
    }
    return false;
  }
  if (expandedSource != nullptr) {
    source = expandedSource->text;
  }
  return true;
}

// TODO-5229: PRIMESTRUCT_FORCE_LAZY_STDLIB_IMPORTS forces lazy expansion on
// regardless of Options::experimentalLazyStdlibImports/the CLI flag, so the
// entire existing test corpus (both primec-subprocess compile_run tests and
// in-process helpers like validateProgramThroughCompilePipeline that build
// Options directly) doubles as a differential corpus on demand - the same
// "existing corpus as differential corpus" methodology
// docs/CompatPathResolutionConsolidation.md's Step 1 used for its own
// permanent differential harness, adapted here since lazy import expansion
// is an alternate compile-pipeline path rather than a single classifier
// function with one legacy-vs-new answer to compare per call.
bool lazyStdlibImportsEnabled(const Options &options) {
  return options.experimentalLazyStdlibImports ||
         std::getenv("PRIMESTRUCT_FORCE_LAZY_STDLIB_IMPORTS") != nullptr;
}

// TODO-5228 (docs/LibrarySymbolManifestLazyImports.md): lazy stdlib import
// expansion. Resolves a stdlib module root key to its physical .prime
// source file using the same std/modules.psmeta override and
// directory-scan-default conventions appendStdlibModuleSources uses above,
// but only accepts the single-file case (no recursive multi-file directory
// scan) - sufficient for every module that currently ships a lazy-loadable
// sibling .psmeta symbol manifest, and a safe thing to decline for modules
// that don't fit that shape (they simply aren't lazy-eligible, and fall
// through to the normal whole-file splice unchanged).
std::optional<std::filesystem::path> resolveSingleFileStdlibModuleSource(
    const std::vector<std::string> &importPaths, const std::string &key) {
  std::error_code ec;
  for (const auto &pathText : importPaths) {
    std::filesystem::path root(pathText);
    if (root.filename() != "stdlib") {
      continue;
    }
    if (!std::filesystem::exists(root, ec) || !std::filesystem::is_directory(root, ec)) {
      continue;
    }

    StdlibModuleManifest manifest;
    std::string manifestError;
    if (readStdlibModuleManifest(root, manifest, manifestError)) {
      const auto manifestEntry = manifest.sourceFilesByRoot.find(key);
      if (manifestEntry != manifest.sourceFilesByRoot.end()) {
        std::filesystem::path resolved = root / manifestEntry->second;
        if (std::filesystem::exists(resolved, ec) && std::filesystem::is_regular_file(resolved, ec)) {
          return resolved;
        }
      }
    }

    if (key.size() <= std::string("/std/").size()) {
      continue;
    }
    const std::string relative = key.substr(std::string("/std/").size());
    const std::filesystem::path moduleRoot = root / "std" / relative;

    std::filesystem::path asFile = moduleRoot;
    asFile += ".prime";
    if (std::filesystem::exists(asFile, ec) && std::filesystem::is_regular_file(asFile, ec)) {
      return asFile;
    }

    if (std::filesystem::is_directory(moduleRoot, ec)) {
      std::optional<std::filesystem::path> onlyFile;
      bool multipleFiles = false;
      for (const auto &dirEntry : std::filesystem::recursive_directory_iterator(moduleRoot, ec)) {
        if (ec) {
          break;
        }
        if (!dirEntry.is_regular_file(ec) || dirEntry.path().extension() != ".prime") {
          continue;
        }
        if (onlyFile.has_value()) {
          multipleFiles = true;
          break;
        }
        onlyFile = dirEntry.path();
      }
      if (onlyFile.has_value() && !multipleFiles) {
        return onlyFile;
      }
    }
  }
  return std::nullopt;
}

std::vector<std::filesystem::path>
resolveLazyStdlibModuleSources(const std::vector<std::string> &importPaths,
                               const std::string &key) {
  if (const auto single = resolveSingleFileStdlibModuleSource(importPaths, key);
      single.has_value()) {
    return {*single};
  }
  if (key.size() <= std::string("/std/").size()) {
    return {};
  }
  std::error_code ec;
  for (const auto &pathText : importPaths) {
    std::filesystem::path root(pathText);
    if (root.filename() != "stdlib") {
      continue;
    }
    const std::filesystem::path moduleRoot = root / "std" / key.substr(std::string("/std/").size());
    if (!std::filesystem::is_directory(moduleRoot, ec)) {
      continue;
    }
    std::vector<std::filesystem::path> files;
    bool allManifested = true;
    for (const auto &dirEntry : std::filesystem::recursive_directory_iterator(moduleRoot, ec)) {
      if (ec) {
        return {};
      }
      if (!dirEntry.is_regular_file(ec) || dirEntry.path().extension() != ".prime") {
        continue;
      }
      if (!std::filesystem::exists(
              std::filesystem::path(dirEntry.path()).replace_extension(".psmeta"), ec)) {
        allManifested = false;
        break;
      }
      files.push_back(dirEntry.path());
    }
    if (allManifested && files.size() > 1) {
      std::sort(files.begin(), files.end());
      return files;
    }
  }
  return {};
}

std::optional<std::filesystem::path> stdlibSymbolManifestPathForSource(
    const std::filesystem::path &sourceFile) {
  std::filesystem::path manifestPath = sourceFile;
  manifestPath.replace_extension(".psmeta");
  std::error_code ec;
  if (std::filesystem::exists(manifestPath, ec) && std::filesystem::is_regular_file(manifestPath, ec)) {
    return manifestPath;
  }
  return std::nullopt;
}

bool isIdentifierChar(char c) {
  return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
}

// Whole-word substring search: true when `word` appears in `text` bounded
// by non-identifier characters (or the start/end of text) on both sides.
bool containsWholeWord(const std::string &text, const std::string &word) {
  if (word.empty()) {
    return false;
  }
  size_t pos = 0;
  while ((pos = text.find(word, pos)) != std::string::npos) {
    const bool leftOk = pos == 0 || !isIdentifierChar(text[pos - 1]);
    const size_t end = pos + word.size();
    const bool rightOk = end >= text.size() || !isIdentifierChar(text[end]);
    if (leftOk && rightOk) {
      return true;
    }
    pos += 1;
  }
  return false;
}

// A fallible struct constructor call (`Window(...)?`) desugars to a call to
// a factory function named by lowercasing the struct's first letter and
// appending "Create" (e.g. "Window" -> "windowCreate", confirmed against
// stdlib/std/gfx/experimental.psmeta's windowCreate/deviceCreate entries).
// That factory function's name never appears as text anywhere the closure
// scan looks - the call site only ever spells the struct name - so a purely
// syntactic scan can't discover it without knowing this convention
// specifically. Given a factory leaf like "windowCreate", returns the
// struct name "Window" a call site would actually spell, or empty if
// `factoryLeaf` doesn't end in "Create" (or is too short to strip it).
std::string constructorSugarStructLeafName(const std::string &factoryLeaf) {
  constexpr std::string_view suffix = "Create";
  if (factoryLeaf.size() <= suffix.size() ||
      factoryLeaf.compare(factoryLeaf.size() - suffix.size(), suffix.size(), suffix) != 0) {
    return {};
  }
  std::string structName = factoryLeaf.substr(0, factoryLeaf.size() - suffix.size());
  structName.front() = static_cast<char>(std::toupper(static_cast<unsigned char>(structName.front())));
  return structName;
}

// An operator on a matrix or quaternion (`multiply(m, v)`, `plus(a, b)`) is
// rewritten after the text scan to a call of a helper named
// `<type, lower case>_<operation>_internal` (`mat4_mul_vec4_internal`), whose
// name appears nowhere in the source. Given such a helper leaf, returns the
// type a call site spells (`Mat4`), or empty for any other leaf.
std::string operatorHelperTypeLeafName(const std::string &helperLeaf) {
  constexpr std::string_view suffix = "_internal";
  const size_t typeEnd = helperLeaf.find('_');
  if (typeEnd == std::string::npos || typeEnd == 0 || helperLeaf.size() <= suffix.size() ||
      helperLeaf.compare(helperLeaf.size() - suffix.size(), suffix.size(), suffix) != 0) {
    return {};
  }
  std::string typeName = helperLeaf.substr(0, typeEnd);
  typeName.front() = static_cast<char>(std::toupper(static_cast<unsigned char>(typeName.front())));
  return typeName;
}

std::string manifestEntryLeafName(const std::string &fullPath) {
  const size_t lastSlash = fullPath.find_last_of('/');
  return lastSlash == std::string::npos ? fullPath : fullPath.substr(lastSlash + 1);
}

bool computeLazyStdlibModuleClosureSource(
    std::vector<LazyStdlibModule> &modules,
    const std::string &seedText,
    std::vector<LazyStdlibExtractedSymbol> &extracted,
    std::string &error,
    const std::vector<LazyStdlibExtractedSymbol> &previouslyExtracted) {
  std::deque<std::string> pendingTexts;
  pendingTexts.push_back(seedText);

  while (!pendingTexts.empty()) {
    const std::string text = std::move(pendingTexts.front());
    pendingTexts.pop_front();

    for (LazyStdlibModule &module : modules) {
      if (!module.active) {
        continue;
      }
      for (const StdlibSymbolManifestEntry &entry : module.entries) {
        if (module.includedPaths.count(entry.path) > 0) {
          continue;
        }
        // A symbol whose lines sit inside one an earlier scan spliced is already in the source.
        if (std::any_of(previouslyExtracted.begin(),
                        previouslyExtracted.end(),
                        [&](const LazyStdlibExtractedSymbol &done) {
                          return done.module == &module &&
                                 done.entry->startLine <= entry.startLine &&
                                 done.entry->endLine >= entry.endLine;
                        })) {
          continue;
        }
        const std::string leaf = manifestEntryLeafName(entry.path);
        bool matched = containsWholeWord(text, leaf);
        if (!matched) {
          const std::string sugarStructLeaf = constructorSugarStructLeafName(leaf);
          matched = !sugarStructLeaf.empty() && containsWholeWord(text, sugarStructLeaf);
        }
        if (!matched) {
          const std::string helperTypeLeaf = operatorHelperTypeLeafName(leaf);
          matched = !helperTypeLeaf.empty() && containsWholeWord(text, helperTypeLeaf);
        }
        if (!matched) {
          continue;
        }
        if (module.loadedSource.path.empty() &&
            !loadManifestModuleSource(module.sourceFile.string(), module.loadedSource, error)) {
          return false;
        }
        std::string extractedText;
        bool isNamespace = false;
        if (!extractAndVerifyManifestedSymbolSource(
                entry, module.loadedSource, extractedText, error, &isNamespace)) {
          return false;
        }
        module.includedPaths.insert(entry.path);
        if (isNamespace) {
          // A stale or hand-written manifest entry for a `namespace` block: its members are manifested on their own.
          continue;
        }
        extracted.push_back(LazyStdlibExtractedSymbol{&module, &entry, extractedText});
        pendingTexts.push_back(extractedText);
      }
    }
  }

  // Some modules declare a method directly inside its struct's own body
  // (rather than via a separate reopened `namespace StructName { ... }`
  // block) - both the struct entry and the nested method entry are
  // independently manifested, but the struct's own extracted slice already
  // contains the nested method's text. Splicing both produces a duplicate
  // definition, so drop any entry whose [start_line, end_line] range falls
  // entirely inside another included entry's range from the same module.
  std::vector<LazyStdlibExtractedSymbol> filtered;
  filtered.reserve(extracted.size());
  for (const LazyStdlibExtractedSymbol &candidate : extracted) {
    bool nested = false;
    for (const LazyStdlibExtractedSymbol &other : extracted) {
      if (other.entry == candidate.entry || other.module != candidate.module) {
        continue;
      }
      if (other.entry->startLine <= candidate.entry->startLine &&
          other.entry->endLine >= candidate.entry->endLine) {
        nested = true;
        break;
      }
    }
    if (!nested) {
      filtered.push_back(candidate);
    }
  }
  extracted = std::move(filtered);
  return true;
}

} // namespace compile_pipeline_detail
} // namespace primec
