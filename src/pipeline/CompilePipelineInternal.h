#pragma once

// Internal helpers of CompilePipeline*.cpp: types and declarations; the
// definitions live in the CompilePipeline*.cpp units.
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

namespace primec {
namespace compile_pipeline_detail {

enum class DumpStage {
  None,
  PreAst,
  Ast,
  Ir,
  IrLowered,
  IrOptimized,
  AstSemantic,
  SemanticProduct,
  TypeGraph,
  Unsupported,
};

DumpStage parseDumpStage(const std::string &dumpStage);

constexpr std::array<std::string_view, 17> SemanticCollectorFamilies = {
    "definitions",
    "executions",
    "direct_call_targets",
    "method_call_targets",
    "bridge_path_choices",
    "callable_summaries",
    "type_metadata",
    "struct_field_metadata",
    "sum_type_metadata",
    "sum_variant_metadata",
    "binding_facts",
    "array_extent_facts",
    "return_facts",
    "local_auto_facts",
    "query_facts",
    "try_facts",
    "on_error_facts",
};

bool isKnownSemanticCollectorFamily(std::string_view name);

bool compilePipelineBenchmarkConfigRequested(const Options &options);

CompilePipelineBenchmarkConfig makeCompilePipelineBenchmarkConfigFromOptions(const Options &options);

CompilePipelineRunConfig makeCompilePipelineRunConfigFromOptions(
    const Options &options,
    CompilePipelineBenchmarkConfig &benchmarkConfigStorage);

CompilePipelineSemanticProductDecision decideSemanticProductDecision(
    DumpStage dumpStage,
    const CompilePipelineRunConfig &runConfig);

uint32_t semanticDefinitionValidationWorkerCount(
    const CompilePipelineBenchmarkConfig *benchmarkConfig);

bool semanticBenchmarkCountersRequested(const CompilePipelineBenchmarkConfig *benchmarkConfig);

bool semanticBenchmarkValidationConfigRequested(
    const CompilePipelineBenchmarkConfig *benchmarkConfig,
    uint32_t definitionValidationWorkerCount);

bool semanticProductDecisionRequestsBuild(CompilePipelineSemanticProductDecision decision);

bool shouldAutoIncludeStdlib(const std::string &source);

bool isIgnorableImportToken(TokenKind kind);

void emitProgramHeapEstimate(const Program &program,
                             std::string_view stage);

std::vector<std::string> collectImportPaths(const std::string &source, bool stdOnly);

std::vector<std::string> collectStdImportPaths(const std::string &source);

std::vector<std::string> collectSourceImportPaths(const std::string &source);

std::vector<std::string> collectImplicitStdlibAutoIncludeKeys(const std::string &source);

std::vector<std::string> collectStdlibAutoIncludeKeys(const std::string &importPath);

struct StdlibModuleManifest {
  std::unordered_map<std::string, std::filesystem::path> sourceFilesByRoot;
};

std::string trimAscii(std::string_view value);

bool isValidStdlibModuleRoot(std::string_view root);

bool pathContainsParentTraversal(const std::filesystem::path &path);

// std::filesystem::exists() matches case-insensitively on case-insensitive
// filesystems (the macOS default), so a stdlib module-root key derived from
// an imported symbol name (e.g. "/std/maybe/Maybe" from "import
// /std/maybe/Maybe") can spuriously "exist" against a differently-cased
// sibling file (stdlib's actual "maybe.prime"), stealing the file from the
// correctly-cased parent module key that should have claimed it. Require an
// exact-case match against the real directory entry before accepting it.
bool existsWithExactCase(const std::filesystem::path &path, std::error_code &ec);

bool appendStdlibModuleManifestEntry(StdlibModuleManifest &manifest,
                                     const std::filesystem::path &manifestPath,
                                     const std::string &root,
                                     const std::string &sourceFile,
                                     std::string &error);

bool readStdlibModuleManifest(const std::filesystem::path &stdlibRoot,
                              StdlibModuleManifest &manifest,
                              std::string &error);

bool isMathBuiltinOrConstantName(std::string_view name);

bool isMathStdlibSurfaceName(std::string_view name);

bool sourceReferencesNonBuiltinMathSymbols(const std::string &source);

// True when `text` contains `needle` (lowercase ASCII) in any letter case, e.g.
// "soa" matches every soa spelling. Conservative on
// purpose: a false positive only keeps the module in the compile.
bool sourceMentionsCaseInsensitive(const std::string &text, std::string_view needle);

bool shouldSkipMathWildcardStdlibModule(const std::vector<std::string> &sourceImports,
                                        const std::string &source);

bool appendStdlibModuleSources(const std::vector<std::string> &importPaths,
                               const std::vector<std::string> &sourceImports,
                               const std::vector<std::string> &implicitKeys,
                               std::string &source,
                               std::string &error,
                               ExpandedSource *expandedSource = nullptr,
                               const std::unordered_set<std::string> &excludedKeys = {});

// PRIMESTRUCT_FORCE_LAZY_STDLIB_IMPORTS forces lazy expansion on
// regardless of Options::experimentalLazyStdlibImports/the CLI flag, so the
// entire existing test corpus (both primec-subprocess compile_run tests and
// in-process helpers like validateProgramThroughCompilePipeline that build
// Options directly) doubles as a differential corpus on demand - the same
// "existing corpus as differential corpus" methodology
// docs/CompatPathResolutionConsolidation.md's Step 1 used for its own
// permanent differential harness, adapted here since lazy import expansion
// is an alternate compile-pipeline path rather than a single classifier
// function with one legacy-vs-new answer to compare per call.
bool lazyStdlibImportsEnabled(const Options &options);

// (docs/LibrarySymbolManifestLazyImports.md): lazy stdlib import
// expansion. Resolves a stdlib module root key to its physical .prime
// source file using the same std/modules.psmeta override and
// directory-scan-default conventions appendStdlibModuleSources uses above,
// but only accepts the single-file case (no recursive multi-file directory
// scan) - sufficient for every module that currently ships a lazy-loadable
// sibling .psmeta symbol manifest, and a safe thing to decline for modules
// that don't fit that shape (they simply aren't lazy-eligible, and fall
// through to the normal whole-file splice unchanged).
std::optional<std::filesystem::path> resolveSingleFileStdlibModuleSource(
    const std::vector<std::string> &importPaths, const std::string &key);

std::optional<std::filesystem::path> stdlibSymbolManifestPathForSource(
    const std::filesystem::path &sourceFile);

bool isIdentifierChar(char c);

// Whole-word substring search: true when `word` appears in `text` bounded
// by non-identifier characters (or the start/end of text) on both sides.
bool containsWholeWord(const std::string &text, const std::string &word);

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
std::string constructorSugarStructLeafName(const std::string &factoryLeaf);

std::string manifestEntryLeafName(const std::string &fullPath);

struct LazyStdlibModule {
  std::string key;
  std::filesystem::path sourceFile;
  std::vector<StdlibSymbolManifestEntry> entries;
  std::unordered_set<std::string> includedPaths;
};

// Builds the symbol closure for every lazy-eligible module referenced by
// `seedText` (typically the user's own expanded source, before any stdlib
// splicing): a purely syntactic, whole-word scan for each manifest entry's
// leaf name, recursively re-scanning each newly-included symbol's own
// extracted body for further references. This never attempts real name
// resolution, so it can only over-include (splice a symbol that turns out
// unused) - always safe - never under-include in a way that would produce
// wrong output; at worst it fails to find a needed symbol and the normal
// "unknown call target"/"unknown identifier" diagnostic surfaces unchanged,
// same as it would without this feature.
struct LazyStdlibExtractedSymbol {
  const LazyStdlibModule *module = nullptr;
  const StdlibSymbolManifestEntry *entry = nullptr;
  std::string text;
};

bool computeLazyStdlibModuleClosureSource(std::vector<LazyStdlibModule> &modules,
                                          const std::string &seedText,
                                          std::vector<LazyStdlibExtractedSymbol> &extracted,
                                          std::string &error);

bool isGraphicsImportPath(const std::string &importPath);

std::string unsupportedGraphicsTargetName(const Options &options);

bool validateGraphicsBackendSupport(const Program &program,
                                    const Options &options,
                                    std::string &error,
                                    CompilePipelineDiagnosticInfo *diagnosticInfo);

struct CompilePipelineImportStageState {
  std::string source;
  ExpandedSource expandedSource;
  std::vector<std::string> sourceImports;
  std::vector<std::string> sourceStdImports;
  std::vector<std::string> implicitStdlibKeys;
  // module-root keys that were treated as lazy-eligible (symbol
  // manifest present, whole-file splice skipped). Used to give a clearer
  // diagnostic if the closure scan's syntactic heuristic missed a symbol
  // the program actually needed, instead of the generic downstream
  // "unknown import path" error that fires when literally nothing from a
  // wildcard-imported module ended up in the compiled buffer.
  std::unordered_set<std::string> lazyStdlibModuleKeys;
};

struct CompilePipelinePreParseStageState {
  std::string filteredSource;
  std::vector<std::string> sourceImports;
};

struct CompilePipelineParsedProgramStageState {
  Program program;
};

bool runCompilePipelineImportStage(const Options &options,
                                   CompilePipelineImportStageState &out,
                                   std::string &error,
                                   DiagnosticSink &diagnosticSink);

bool runCompilePipelineTransformStage(
    const Options &options,
    const CompilePipelineImportStageState &importStage,
    CompilePipelinePreParseStageState &out,
    std::string &error,
    DiagnosticSink &diagnosticSink);

void sortParserErrorsForStableOrdering(std::vector<Parser::ErrorInfo> &errors);

bool runCompilePipelineParseStage(const Options &options,
                                  const ExpandedSource &expandedSource,
                                  const CompilePipelinePreParseStageState &preParseStage,
                                  CompilePipelineParsedProgramStageState &out,
                                  std::string &error,
                                  DiagnosticSink &diagnosticSink);

void sortParserErrorsForStableOrdering(std::vector<Parser::ErrorInfo> &errors);

} // namespace compile_pipeline_detail
} // namespace primec
