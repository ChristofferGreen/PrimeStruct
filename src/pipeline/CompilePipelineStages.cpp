#include "primec/pipeline/CompilePipeline.h"
#include "primec/support/CompileContext.h"

#include "../frontend/ExpandedSourceBuilder.h"

#include "primec/ast/AstMemory.h"
#include "primec/ast/AstPrinter.h"
#include "primec/frontend/ImportResolver.h"
#include "primec/pipeline/LibraryLink.h"
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
#include "primec/support/CompileArena.h"
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


bool isGraphicsImportPath(const std::string &importPath) {
  if (importPath == "/std/gfx/*" || importPath == "/std/gfx") {
    return true;
  }
  return importPath.rfind("/std/gfx/", 0) == 0;
}

std::string unsupportedGraphicsTargetName(const Options &options) {
  const IrBackendCapabilitySupport support =
      queryIrBackendCapabilitySupport(options, IrBackendCapability::GraphicsRuntimeSubstrate);
  return support.supported ? "" : std::string(support.targetName);
}

bool validateGraphicsBackendSupport(const Program &program,
                                    const Options &options,
                                    std::string &error,
                                    CompilePipelineDiagnosticInfo *diagnosticInfo) {
  const std::string targetName = unsupportedGraphicsTargetName(options);
  if (targetName.empty()) {
    return true;
  }

  for (const auto &importPath : program.imports) {
    if (!isGraphicsImportPath(importPath)) {
      continue;
    }
    error = "graphics stdlib runtime substrate unavailable for " + targetName + " target: " + importPath;
    if (diagnosticInfo != nullptr) {
      DiagnosticSink sink(diagnosticInfo);
      DiagnosticSinkRecord record;
      record.message = error;
      sink.setRecords({std::move(record)});
    }
    return false;
  }

  return true;
}

// Names where a missing stdlib module was looked for, and how to add another place.
static std::string describeStdlibSearch(const Options &options) {
  std::string text = "; stdlib roots in use:";
  bool anyRoot = false;
  for (const std::string &path : options.importPaths) {
    if (std::filesystem::path(path).filename() == "stdlib") {
      text += (anyRoot ? ", " : " ") + path;
      anyRoot = true;
    }
  }
  if (!anyRoot) {
    text += " none; no stdlib directory exists at";
    std::string separator = " ";
    for (const std::string &candidate : stdlibSearchCandidates(options.inputPath)) {
      text += separator + candidate;
      separator = ", ";
    }
  }
  text += "; add one with --import-path <dir> (a directory named stdlib)";
  return text;
}

bool runCompilePipelineImportStage(const Options &options,
                                   CompilePipelineImportStageState &out,
                                   std::string &error,
                                   DiagnosticSink &diagnosticSink,
                                   const std::vector<std::string> &forcedLazyPaths) {
  ImportResolver importResolver;
  // `--library`: an `import</root>` of a root a library owns reads the interface of its archive.
  std::vector<LoadedLibrary> libraries;
  std::vector<LibraryInterfaceUnit> libraryInterfaces;
  std::vector<OwnedStdlibModule> ownedModules;
  if (!options.libraryPaths.empty()) {
    if (!loadLibraries(options.libraryPaths, libraries, error)) {
      diagnosticSink.setSummary(error);
      return false;
    }
    libraryInterfaces = libraryInterfaceUnits(libraries);
    out.libraryRoots = libraryRootOwners(libraries);
    ownedModules = ownedStdlibModules(libraries, libraryInterfaces);
    importResolver.setLibraryInterfaces(&libraryInterfaces);
  }
  const bool imported =
      options.inMemorySource.has_value()
          ? importResolver.expandImportsFromSource(options.inputPath,
                                                   *options.inMemorySource,
                                                   out.expandedSource,
                                                   error,
                                                   options.importPaths)
          : importResolver.expandImports(options.inputPath,
                                         out.expandedSource,
                                         error,
                                         options.importPaths);
  if (!imported) {
    diagnosticSink.setSummary(error);
    return false;
  }
  out.source = out.expandedSource.text;

  out.sourceImports = collectSourceImportPaths(out.source);
  out.sourceStdImports = collectStdImportPaths(out.source);
  out.implicitStdlibKeys = collectImplicitStdlibAutoIncludeKeys(out.source);

  // TODO-5228: identify which stdlib module-root imports are lazy-eligible
  // (resolve to a single .prime file with a sibling .psmeta symbol
  // manifest) before the normal whole-file splice runs, so those roots can
  // be excluded from it entirely.
  std::vector<LazyStdlibModule> lazyModules;
  std::unordered_set<std::string> &lazyKeys = out.lazyStdlibModuleKeys;
  if (lazyStdlibImportsEnabled(options)) {
    std::vector<std::string> candidateKeys;
    for (const auto &importPath : out.sourceStdImports) {
      for (const auto &key : collectStdlibAutoIncludeKeys(importPath)) {
        candidateKeys.push_back(key);
      }
    }
    for (const auto &key : out.implicitStdlibKeys) {
      candidateKeys.push_back(key);
    }
    std::unordered_set<std::string> seenCandidateKeys;
    for (const auto &key : candidateKeys) {
      if (!seenCandidateKeys.insert(key).second) {
        continue;
      }
      const std::vector<std::filesystem::path> sourceFiles =
          resolveLazyStdlibModuleSources(options.importPaths, key);
      if (sourceFiles.empty()) {
        continue;
      }
      // Every file of the module is read lazily or none is: a file left out would never be spliced.
      std::vector<LazyStdlibModule> keyModules;
      bool eligible = true;
      for (const std::filesystem::path &sourceFile : sourceFiles) {
        // A module whose file is the root is in the source whole: reading it lazily too would define it twice.
        std::error_code rootEc;
        if (std::filesystem::equivalent(sourceFile, options.inputPath, rootEc)) {
          eligible = false;
          break;
        }
        // A file the import resolver already spliced (`import</std/math>`) is in the source whole.
        if (std::any_of(out.expandedSource.units.begin(),
                        out.expandedSource.units.end(),
                        [&](const SourceUnit &unit) {
                          if (unit.kind != SourceUnitKind::Import) {
                            return false;
                          }
                          std::error_code unitEc;
                          return std::filesystem::equivalent(sourceFile, unit.displayPath, unitEc);
                        })) {
          eligible = false;
          break;
        }
        // A module a linked library stands for is not read, lazily or whole.
        if (std::any_of(
                ownedModules.begin(), ownedModules.end(), [&](const OwnedStdlibModule &owned) {
                  return owned.file ==
                         stdlibRelativeFile(std::filesystem::absolute(sourceFile).string());
                })) {
          eligible = false;
          break;
        }
        const std::optional<std::filesystem::path> manifestPath =
            stdlibSymbolManifestPathForSource(sourceFile);
        if (!manifestPath.has_value()) {
          eligible = false;
          break;
        }
        LazyStdlibModule module;
        module.key = key;
        module.sourceFile = sourceFile;
        if (!readStdlibSymbolManifest(manifestPath->string(), module.entries, error)) {
          diagnosticSink.setSummary(error);
          return false;
        }
        keyModules.push_back(std::move(module));
      }
      if (!eligible) {
        continue;
      }
      lazyKeys.insert(key);
      for (LazyStdlibModule &module : keyModules) {
        lazyModules.push_back(std::move(module));
      }
    }

    // A lazy module's own top-level `import` lines (e.g. image.prime's
    // `import /std/math/*`) normally get pulled in as a side effect of
    // appendStdlibModuleSources appending the whole file and then
    // recursively scanning it for nested imports. Since a lazy module's
    // whole-file text is never appended, those transitively-needed sibling
    // modules must be seeded explicitly here so they still get included
    // (normally, not lazily - they aren't manifested and don't need to be).
    // Only seed them for a module that the closure scan will actually draw
    // from (a cheap pre-check: does the pre-splice seed text reference any
    // of this module's own manifested leaf names?) - otherwise a program
    // that merely imports a lazy module without using it would pay for its
    // sibling modules' full inclusion for no reason, undermining the whole
    // point of lazy expansion.
    for (const LazyStdlibModule &module : lazyModules) {
      bool moduleLikelyNeeded =
          std::any_of(module.entries.begin(), module.entries.end(),
                     [&](const StdlibSymbolManifestEntry &entry) {
                       return containsWholeWord(out.source, manifestEntryLeafName(entry.path));
                     });
      if (!moduleLikelyNeeded) {
        // A program can also reopen a lazy module's own namespace path
        // (`namespace std { namespace image { ... } }`) to declare new,
        // non-manifested code that still needs that module's transitive
        // imports - the leaf-name check above only catches usage of an
        // actual manifested symbol, not this. Treat a textual reopening of
        // the module's own namespace leaf as needing it too.
        moduleLikelyNeeded =
            containsWholeWord(out.source, "namespace " + manifestEntryLeafName(module.key));
      }
      if (!moduleLikelyNeeded) {
        continue;
      }
      std::ifstream moduleFile(module.sourceFile);
      if (!moduleFile) {
        continue;
      }
      std::ostringstream moduleBuffer;
      moduleBuffer << moduleFile.rdbuf();
      for (const std::string &nestedImport : collectStdImportPaths(moduleBuffer.str())) {
        out.sourceStdImports.push_back(nestedImport);
        // Also emit a literal `import X` statement, not just an internal
        // file-inclusion hint: whole-file splicing's own success at
        // resolving bare names/types owned by a transitively-imported
        // module depends on the parser recording that import path into
        // Program::imports (Parser::parseImport pushes every literal
        // `import` statement it sees) - appendStdlibModuleSources's
        // appendFile does this "for free" for a normally-spliced module
        // because the module's own header import lines are verbatim part
        // of the appended text. A lazily-extracted symbol slice never
        // includes its module's header, so without this, nothing ever
        // records the nested import and cross-module bare-name/bare-type
        // resolution silently fails even though the target definition is
        // present in defMap_.
        ExpandedSourceBuilder nestedImportBuilder(out.expandedSource);
        nestedImportBuilder.appendGenerated(
            "\nimport " + nestedImport + "\n", "<lazy-stdlib-nested-import>");
        out.source = out.expandedSource.text;
      }
    }
  }

  // Run the lazy closure scan before appendStdlibModuleSources (rather than
  // after, which is the more obvious order): appendStdlibModuleSources's
  // shouldSkipMathWildcardStdlibModule heuristic decides whether to skip
  // fully including a bare `import /std/math/*` based on whether the
  // *current* source text references any non-builtin math symbol. A lazily
  // extracted symbol can itself reference a non-lazy sibling module's type
  // (e.g. /std/gfx/experimental/Frame/render_pass, pulled in as harmless
  // over-inclusion by the closure scan, takes a [ColorRGBA] parameter from
  // /std/math) - if that extraction happens after the math-wildcard-skip
  // check already ran, the check never sees the ColorRGBA reference and
  // incorrectly skips including /std/math, leaving ColorRGBA undefined.
  // Extracting first ensures every heuristic downstream sees the same
  // source content a whole-file splice would have produced.
  std::vector<LazyStdlibExtractedSymbol> extracted;
  if (!lazyModules.empty()) {
    if (!computeLazyStdlibModuleClosureSource(
            lazyModules, out.source, extracted, error, {}, forcedLazyPaths)) {
      diagnosticSink.setSummary(error);
      return false;
    }
    if (!extracted.empty()) {
      ExpandedSourceBuilder sourceBuilder(out.expandedSource);
      for (const LazyStdlibExtractedSymbol &symbol : extracted) {
        sourceBuilder.appendGenerated("\n", "<lazy-stdlib-separator>");
        const std::size_t unitId = sourceBuilder.addUnit(
            SourceUnitKind::Stdlib, symbol.module->sourceFile.string(), symbol.module->key, 1, 1);
        sourceBuilder.appendSegment(unitId, symbol.text, 1, 1);
      }
      out.source = out.expandedSource.text;
    }
  }

  const std::size_t sourceSizeBeforeModules = out.source.size();
  std::unordered_set<std::string> lazyKeysImportedByModules;
  if (shouldAutoIncludeStdlib(out.source) || !out.implicitStdlibKeys.empty()) {
    std::string unresolvedImport;
    if (!appendStdlibModuleSources(options.importPaths,
                                   out.sourceStdImports,
                                   out.implicitStdlibKeys,
                                   out.source,
                                   error,
                                   &out.expandedSource,
                                   lazyKeys,
                                   &unresolvedImport,
                                   &ownedModules,
                                   &lazyKeysImportedByModules)) {
      if (error.rfind("stdlib import requested but", 0) == 0) {
        error += describeStdlibSearch(options);
      }
      if (const auto span = findImportPathSpan(out.expandedSource, unresolvedImport);
          span.has_value()) {
        DiagnosticSinkRecord record;
        record.message = error;
        record.primarySpan = *span;
        record.hasPrimarySpan = true;
        diagnosticSink.setRecords({std::move(record)});
      }
      diagnosticSink.setSummary(error);
      return false;
    }
    // A wildcard import of a stdlib file that defines its names at the root (`/std/collections/equality` holds
    // `/i32/equal`, nothing under its own path) has no definition under the imported path, and is not a typo
    // either: the file was read. The import check takes such a key like a lazy one.
    for (const auto &importPath : out.sourceStdImports) {
      const std::vector<std::string> keys = collectStdlibAutoIncludeKeys(importPath);
      if (!keys.empty() &&
          resolveSingleFileStdlibModuleSource(options.importPaths, keys.front()).has_value()) {
        lazyKeys.insert(keys.front());
      }
    }
    // A module spliced whole that imports a lazy module uses symbols of it the user's own text never names:
    // scan the spliced text for those modules too.
    if (!lazyKeysImportedByModules.empty() && out.source.size() > sourceSizeBeforeModules) {
      for (LazyStdlibModule &module : lazyModules) {
        module.active = lazyKeysImportedByModules.count(module.key) > 0;
      }
      std::vector<LazyStdlibExtractedSymbol> lateExtracted;
      if (!computeLazyStdlibModuleClosureSource(lazyModules,
                                                out.source.substr(sourceSizeBeforeModules),
                                                lateExtracted,
                                                error,
                                                extracted)) {
        diagnosticSink.setSummary(error);
        return false;
      }
      if (!lateExtracted.empty()) {
        ExpandedSourceBuilder sourceBuilder(out.expandedSource);
        for (const LazyStdlibExtractedSymbol &symbol : lateExtracted) {
          sourceBuilder.appendGenerated("\n", "<lazy-stdlib-separator>");
          const std::size_t unitId = sourceBuilder.addUnit(
              SourceUnitKind::Stdlib, symbol.module->sourceFile.string(), symbol.module->key, 1, 1);
          sourceBuilder.appendSegment(unitId, symbol.text, 1, 1);
        }
        out.source = out.expandedSource.text;
      }
    }
  }

  return true;
}

bool runCompilePipelineTransformStage(
    const Options &options,
    const CompilePipelineImportStageState &importStage,
    CompilePipelinePreParseStageState &out,
    std::string &error,
    DiagnosticSink &diagnosticSink) {
  TextFilterPipeline textPipeline;
  TextFilterOptions textOptions;
  textOptions.enabledFilters = options.textFilters;
  textOptions.rules = options.textTransformRules;
  textOptions.allowEnvelopeTransforms = options.allowEnvelopeTextTransforms;

  if (!textPipeline.apply(importStage.source,
                          out.filteredSource,
                          error,
                          textOptions)) {
    diagnosticSink.setSummary(error);
    return false;
  }

  if (importStage.source.size() == importStage.expandedSource.text.size()) {
    // The options outlive the compile scope: the source location mapper reads them when a diagnostic is made.
    SystemHeapScope systemHeapGuard;
    out.textFilterOptions = std::make_shared<const TextFilterOptions>(textOptions);
  }
  out.sourceImports = importStage.sourceImports;
  return true;
}

bool runCompilePipelineParseStage(const Options &options,
                                  const ExpandedSource &expandedSource,
                                  const CompilePipelinePreParseStageState &preParseStage,
                                  CompilePipelineParsedProgramStageState &out,
                                  std::string &error,
                                  DiagnosticSink &diagnosticSink) {
  Lexer lexer(preParseStage.filteredSource);
  Parser parser(lexer.tokenize(), !options.requireCanonicalSyntax);
  Parser::ErrorInfo parserErrorInfo;
  std::vector<Parser::ErrorInfo> parserErrors;
  if (!parser.parse(out.program,
                    error,
                    &parserErrorInfo,
                    options.collectDiagnostics ? &parserErrors : nullptr)) {
    if (options.collectDiagnostics) {
      if (parserErrors.empty() && !parserErrorInfo.message.empty()) {
        parserErrors.push_back(parserErrorInfo);
      }
      sortParserErrorsForStableOrdering(parserErrors);
      if (!parserErrors.empty()) {
        const Parser::ErrorInfo &first = parserErrors.front();
        if (!first.message.empty()) {
          if (first.line > 0 && first.column > 0) {
            error = first.message + " at " + std::to_string(first.line) +
                    ":" + std::to_string(first.column);
          } else {
            error = first.message;
          }
        }
      }
    }
    if (!parserErrors.empty()) {
      std::vector<DiagnosticSinkRecord> records;
      records.reserve(parserErrors.size());
      for (const auto &item : parserErrors) {
        DiagnosticSinkRecord record;
        record.message = item.message;
        if (item.line > 0 && item.column > 0) {
          record.primarySpan.line = item.line;
          record.primarySpan.column = item.column;
          record.primarySpan.endLine = item.line;
          record.primarySpan.endColumn = item.column;
          record.hasPrimarySpan = true;
        }
        records.push_back(std::move(record));
      }
      mapAndSortDiagnosticRecordsToSourceUnits(expandedSource, records);
      diagnosticSink.setRecords(std::move(records));
    } else {
      diagnosticSink.setSummary(parserErrorInfo.message);
      if (parserErrorInfo.line > 0 && parserErrorInfo.column > 0) {
        const DiagnosticSpan span = mapDiagnosticSpanToSourceUnit(
            expandedSource,
            DiagnosticSpan{.file = {},
                           .line = parserErrorInfo.line,
                           .column = parserErrorInfo.column,
                           .endLine = parserErrorInfo.line,
                           .endColumn = parserErrorInfo.column});
        diagnosticSink.capturePrimarySpanIfUnset(span);
      }
    }
    return false;
  }

  out.program.sourceImports = preParseStage.sourceImports;
  return true;
}

void sortParserErrorsForStableOrdering(std::vector<Parser::ErrorInfo> &errors) {
  auto normalize = [](int value) -> int {
    return value > 0 ? value : std::numeric_limits<int>::max();
  };
  std::stable_sort(errors.begin(), errors.end(), [&](const Parser::ErrorInfo &left, const Parser::ErrorInfo &right) {
    const int leftLine = normalize(left.line);
    const int rightLine = normalize(right.line);
    if (leftLine != rightLine) {
      return leftLine < rightLine;
    }
    const int leftColumn = normalize(left.column);
    const int rightColumn = normalize(right.column);
    if (leftColumn != rightColumn) {
      return leftColumn < rightColumn;
    }
    return left.message < right.message;
  });
}

} // namespace compile_pipeline_detail
} // namespace primec
