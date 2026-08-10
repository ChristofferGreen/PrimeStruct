#include "primec/CompilePipeline.h"

#include "ExpandedSourceBuilder.h"

#include "primec/AstMemory.h"
#include "primec/AstPrinter.h"
#include "primec/ImportResolver.h"
#include "primec/IrBackendProfiles.h"
#include "primec/IrPrinter.h"
#include "primec/Lexer.h"
#include "primec/Parser.h"
#include "primec/Semantics.h"
#include "primec/StdlibCollectionPaths.h"
#include "primec/SemanticsBenchmark.h"
#include "primec/SourceLocationMapper.h"
#include "primec/StdlibSurfaceRegistry.h"
#include "primec/StdlibSymbolManifest.h"
#include "primec/TextFilterPipeline.h"
#include "primec/TransformRules.h"
#include "semantics/TypeResolutionGraph.h"

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

namespace primec {
namespace {

enum class DumpStage {
  None,
  PreAst,
  Ast,
  Ir,
  AstSemantic,
  SemanticProduct,
  TypeGraph,
  Unsupported,
};

DumpStage parseDumpStage(const std::string &dumpStage) {
  if (dumpStage.empty()) {
    return DumpStage::None;
  }
  if (dumpStage == "pre_ast") {
    return DumpStage::PreAst;
  }
  if (dumpStage == "ast") {
    return DumpStage::Ast;
  }
  if (dumpStage == "ir") {
    return DumpStage::Ir;
  }
  if (dumpStage == "ast_semantic" || dumpStage == "ast-semantic") {
    return DumpStage::AstSemantic;
  }
  if (dumpStage == "semantic_product" || dumpStage == "semantic-product") {
    return DumpStage::SemanticProduct;
  }
  if (dumpStage == "type_graph" || dumpStage == "type-graph") {
    return DumpStage::TypeGraph;
  }
  return DumpStage::Unsupported;
}

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

bool isKnownSemanticCollectorFamily(std::string_view name) {
  return std::find(SemanticCollectorFamilies.begin(), SemanticCollectorFamilies.end(), name) !=
         SemanticCollectorFamilies.end();
}

bool compilePipelineBenchmarkConfigRequested(const Options &options) {
  return options.benchmarkForceSemanticProduct.has_value() ||
         options.benchmarkSemanticNoFactEmission ||
         options.benchmarkSemanticFactFamiliesSpecified ||
         options.benchmarkSemanticTwoChunkDefinitionValidation ||
         options.benchmarkSemanticDefinitionValidationWorkerCount.has_value() ||
         options.benchmarkSemanticPhaseCounters ||
         options.benchmarkSemanticAllocationCounters ||
         options.benchmarkSemanticRssCheckpoints ||
         options.benchmarkSemanticDisableMethodTargetMemoization ||
         options.benchmarkSemanticGraphLocalAutoLegacyKeyShadow ||
         options.benchmarkSemanticGraphLocalAutoLegacySideChannelShadow ||
         options.benchmarkSemanticDisableGraphLocalAutoDependencyScratchPmr;
}

CompilePipelineBenchmarkConfig makeCompilePipelineBenchmarkConfigFromOptions(const Options &options) {
  CompilePipelineBenchmarkConfig config;
  config.forceSemanticProduct = options.benchmarkForceSemanticProduct;
  config.semanticNoFactEmission = options.benchmarkSemanticNoFactEmission;
  config.semanticFactFamiliesSpecified = options.benchmarkSemanticFactFamiliesSpecified;
  config.semanticFactFamilies = options.benchmarkSemanticFactFamilies;
  config.semanticTwoChunkDefinitionValidation = options.benchmarkSemanticTwoChunkDefinitionValidation;
  config.semanticDefinitionValidationWorkerCount =
      options.benchmarkSemanticDefinitionValidationWorkerCount;
  config.semanticPhaseCounters = options.benchmarkSemanticPhaseCounters;
  config.semanticAllocationCounters = options.benchmarkSemanticAllocationCounters;
  config.semanticRssCheckpoints = options.benchmarkSemanticRssCheckpoints;
  config.semanticDisableMethodTargetMemoization =
      options.benchmarkSemanticDisableMethodTargetMemoization;
  config.semanticGraphLocalAutoLegacyKeyShadow =
      options.benchmarkSemanticGraphLocalAutoLegacyKeyShadow;
  config.semanticGraphLocalAutoLegacySideChannelShadow =
      options.benchmarkSemanticGraphLocalAutoLegacySideChannelShadow;
  config.semanticDisableGraphLocalAutoDependencyScratchPmr =
      options.benchmarkSemanticDisableGraphLocalAutoDependencyScratchPmr;
  return config;
}

CompilePipelineRunConfig makeCompilePipelineRunConfigFromOptions(
    const Options &options,
    CompilePipelineBenchmarkConfig &benchmarkConfigStorage) {
  CompilePipelineRunConfig runConfig;
  runConfig.skipSemanticProductForNonConsumingPath =
      options.skipSemanticProductForNonConsumingPath;
  if (compilePipelineBenchmarkConfigRequested(options)) {
    benchmarkConfigStorage = makeCompilePipelineBenchmarkConfigFromOptions(options);
    runConfig.benchmark = &benchmarkConfigStorage;
  }
  return runConfig;
}

CompilePipelineSemanticProductDecision decideSemanticProductDecision(
    DumpStage dumpStage,
    const CompilePipelineRunConfig &runConfig) {
  const CompilePipelineBenchmarkConfig *benchmarkConfig = runConfig.benchmark;
  if (benchmarkConfig != nullptr && benchmarkConfig->forceSemanticProduct.has_value()) {
    return *benchmarkConfig->forceSemanticProduct
               ? CompilePipelineSemanticProductDecision::ForcedOnForBenchmark
               : CompilePipelineSemanticProductDecision::ForcedOffForBenchmark;
  }
  if (dumpStage == DumpStage::AstSemantic) {
    return CompilePipelineSemanticProductDecision::SkipForAstSemanticDump;
  }
  if (runConfig.skipSemanticProductForNonConsumingPath) {
    return CompilePipelineSemanticProductDecision::SkipForNonConsumingPath;
  }
  return CompilePipelineSemanticProductDecision::RequireForConsumingPath;
}

uint32_t semanticDefinitionValidationWorkerCount(
    const CompilePipelineBenchmarkConfig *benchmarkConfig) {
  if (benchmarkConfig == nullptr) {
    return 1;
  }
  if (benchmarkConfig->semanticDefinitionValidationWorkerCount.has_value()) {
    return *benchmarkConfig->semanticDefinitionValidationWorkerCount;
  }
  if (benchmarkConfig->semanticTwoChunkDefinitionValidation) {
    return 2;
  }
  return 1;
}

bool semanticBenchmarkCountersRequested(const CompilePipelineBenchmarkConfig *benchmarkConfig) {
  return benchmarkConfig != nullptr &&
         (benchmarkConfig->semanticPhaseCounters ||
          benchmarkConfig->semanticAllocationCounters ||
          benchmarkConfig->semanticRssCheckpoints);
}

bool semanticBenchmarkValidationConfigRequested(
    const CompilePipelineBenchmarkConfig *benchmarkConfig,
    uint32_t definitionValidationWorkerCount) {
  return benchmarkConfig != nullptr &&
         (definitionValidationWorkerCount != 1 ||
          benchmarkConfig->semanticDisableMethodTargetMemoization ||
          benchmarkConfig->semanticGraphLocalAutoLegacyKeyShadow ||
          benchmarkConfig->semanticGraphLocalAutoLegacySideChannelShadow ||
          benchmarkConfig->semanticDisableGraphLocalAutoDependencyScratchPmr);
}

bool semanticProductDecisionRequestsBuild(CompilePipelineSemanticProductDecision decision) {
  switch (decision) {
    case CompilePipelineSemanticProductDecision::RequireForConsumingPath:
    case CompilePipelineSemanticProductDecision::ForcedOnForBenchmark:
      return true;
    case CompilePipelineSemanticProductDecision::SkipForAstSemanticDump:
    case CompilePipelineSemanticProductDecision::SkipForNonConsumingPath:
    case CompilePipelineSemanticProductDecision::ForcedOffForBenchmark:
      return false;
  }
  return true;
}

bool shouldAutoIncludeStdlib(const std::string &source) {
  size_t pos = 0;
  while ((pos = source.find("import /std", pos)) != std::string::npos) {
    size_t next = pos + std::string("import /std").size();
    if (next >= source.size()) {
      return true;
    }
    char c = source[next];
    if (c == '/' || std::isspace(static_cast<unsigned char>(c)) != 0) {
      return true;
    }
    pos = next;
  }
  return false;
}

bool isIgnorableImportToken(TokenKind kind) {
  return kind == TokenKind::Comment || kind == TokenKind::Comma || kind == TokenKind::Semicolon;
}

void emitProgramHeapEstimate(const Program &program,
                             std::string_view stage) {
  const ProgramHeapEstimateStats stats = estimateProgramHeap(program);
  std::cerr << "[benchmark-ast-heap-estimate] "
            << "{\"stage\":\"" << stage
            << "\",\"definitions\":" << stats.definitions
            << ",\"executions\":" << stats.executions
            << ",\"exprs\":" << stats.exprs
            << ",\"transforms\":" << stats.transforms
            << ",\"strings\":" << stats.strings
            << ",\"dynamic_bytes\":" << stats.dynamicBytes
            << "}\n";
}

std::vector<std::string> collectImportPaths(const std::string &source, bool stdOnly) {
  std::vector<std::string> imports;
  Lexer lexer(source);
  const std::vector<Token> tokens = lexer.tokenize();
  auto skipIgnorableTokens = [&](size_t cursor) {
    while (cursor < tokens.size() && isIgnorableImportToken(tokens[cursor].kind)) {
      ++cursor;
    }
    return cursor;
  };
  for (size_t scan = 0; scan < tokens.size(); ++scan) {
    if (tokens[scan].kind != TokenKind::KeywordImport) {
      continue;
    }
    size_t cursor = skipIgnorableTokens(scan + 1);
    while (cursor < tokens.size()) {
      if (tokens[cursor].kind != TokenKind::Identifier || tokens[cursor].text.empty() ||
          tokens[cursor].text[0] != '/') {
        break;
      }
      std::string path = tokens[cursor].text;
      size_t next = skipIgnorableTokens(cursor + 1);
      if (!path.empty() && path.back() == '/' && next < tokens.size() && tokens[next].kind == TokenKind::Star) {
        path.pop_back();
        path += "/*";
        cursor = next + 1;
      } else {
        ++cursor;
      }
      if (!stdOnly || path.rfind("/std/", 0) == 0 || path == "/std") {
        imports.push_back(std::move(path));
      }
      cursor = skipIgnorableTokens(cursor);
    }
  }
  return imports;
}

std::vector<std::string> collectStdImportPaths(const std::string &source) {
  return collectImportPaths(source, true);
}

std::vector<std::string> collectSourceImportPaths(const std::string &source) {
  return collectImportPaths(source, false);
}

std::vector<std::string> collectImplicitStdlibAutoIncludeKeys(const std::string &source) {
  (void)source;
  return {};
}

std::vector<std::string> collectStdlibAutoIncludeKeys(const std::string &importPath) {
  std::vector<std::string> keys;
  if (importPath.rfind("/std/", 0) != 0) {
    return keys;
  }

  std::string key = importPath;
  if (key.size() >= 2 && key.compare(key.size() - 2, 2, "/*") == 0) {
    key.erase(key.size() - 2);
  }
  if (const auto *metadata = findStdlibSurfaceMetadataBySpelling(key);
      metadata != nullptr &&
      (metadata->domain == StdlibSurfaceDomain::File ||
       metadata->domain == StdlibSurfaceDomain::Gfx)) {
    key = std::string(metadata->canonicalImportRoot);
  }

  while (!key.empty()) {
    keys.push_back(key);
    if (key == "/std/gfx/experimental") {
      break;
    }
    const size_t slash = key.find_last_of('/');
    if (slash <= std::string("/std").size()) {
      break;
    }
    key.erase(slash);
  }

  return keys;
}

struct StdlibModuleManifest {
  std::unordered_map<std::string, std::filesystem::path> sourceFilesByRoot;
};

std::string trimAscii(std::string_view value) {
  while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())) != 0) {
    value.remove_prefix(1);
  }
  while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())) != 0) {
    value.remove_suffix(1);
  }
  return std::string(value);
}

bool isValidStdlibModuleRoot(std::string_view root) {
  return root == "/std" || root.rfind("/std/", 0) == 0;
}

bool pathContainsParentTraversal(const std::filesystem::path &path) {
  for (const auto &part : path) {
    if (part == std::filesystem::path("..")) {
      return true;
    }
  }
  return false;
}

bool appendStdlibModuleManifestEntry(StdlibModuleManifest &manifest,
                                     const std::filesystem::path &manifestPath,
                                     const std::string &root,
                                     const std::string &sourceFile,
                                     std::string &error) {
  if (root.empty()) {
    error = "invalid stdlib module manifest " + manifestPath.string() + ": module entry missing root";
    return false;
  }
  if (!isValidStdlibModuleRoot(root)) {
    error = "invalid stdlib module manifest " + manifestPath.string() +
            ": module root must be /std or /std/...: " + root;
    return false;
  }
  if (sourceFile.empty()) {
    error = "invalid stdlib module manifest " + manifestPath.string() +
            ": module entry missing source_file for " + root;
    return false;
  }

  const std::filesystem::path sourcePath(sourceFile);
  if (sourcePath.is_absolute() || pathContainsParentTraversal(sourcePath)) {
    error = "invalid stdlib module manifest " + manifestPath.string() +
            ": source_file must be a relative stdlib path for " + root;
    return false;
  }
  if (sourcePath.extension() != ".prime") {
    error = "invalid stdlib module manifest " + manifestPath.string() +
            ": source_file must name a .prime file for " + root;
    return false;
  }
  if (!manifest.sourceFilesByRoot.emplace(root, sourcePath).second) {
    error = "invalid stdlib module manifest " + manifestPath.string() +
            ": duplicate module root " + root;
    return false;
  }
  return true;
}

bool readStdlibModuleManifest(const std::filesystem::path &stdlibRoot,
                              StdlibModuleManifest &manifest,
                              std::string &error) {
  std::error_code ec;
  const std::filesystem::path manifestPath =
      stdlibRoot / "std" / "modules.psmeta";
  if (!std::filesystem::exists(manifestPath, ec)) {
    return true;
  }
  if (!std::filesystem::is_regular_file(manifestPath, ec)) {
    error = "invalid stdlib module manifest path: " + manifestPath.string();
    return false;
  }

  std::ifstream input(manifestPath);
  if (!input) {
    error = "failed to read stdlib module manifest: " + manifestPath.string();
    return false;
  }

  bool inModule = false;
  std::string currentRoot;
  std::string currentSourceFile;
  auto flushModule = [&]() -> bool {
    if (!inModule) {
      return true;
    }
    if (!appendStdlibModuleManifestEntry(
            manifest, manifestPath, currentRoot, currentSourceFile, error)) {
      return false;
    }
    currentRoot.clear();
    currentSourceFile.clear();
    return true;
  };

  std::string line;
  while (std::getline(input, line)) {
    const std::size_t commentPos = line.find('#');
    if (commentPos != std::string::npos) {
      line.resize(commentPos);
    }
    const std::string trimmed = trimAscii(line);
    if (trimmed.empty()) {
      continue;
    }
    if (trimmed == "[module]") {
      if (!flushModule()) {
        return false;
      }
      inModule = true;
      continue;
    }
    if (!inModule) {
      error = "invalid stdlib module manifest " + manifestPath.string() +
              ": expected [module] before entries";
      return false;
    }
    const std::size_t equalsPos = trimmed.find('=');
    if (equalsPos == std::string::npos) {
      error = "invalid stdlib module manifest " + manifestPath.string() +
              ": expected key = value entry";
      return false;
    }

    const std::string key = trimAscii(std::string_view(trimmed).substr(0, equalsPos));
    const std::string value =
        trimAscii(std::string_view(trimmed).substr(equalsPos + 1));
    if (key == "root") {
      if (!currentRoot.empty()) {
        error = "invalid stdlib module manifest " + manifestPath.string() +
                ": duplicate root in module entry";
        return false;
      }
      currentRoot = value;
    } else if (key == "source_file") {
      if (!currentSourceFile.empty()) {
        error = "invalid stdlib module manifest " + manifestPath.string() +
                ": duplicate source_file in module entry";
        return false;
      }
      currentSourceFile = value;
    } else {
      error = "invalid stdlib module manifest " + manifestPath.string() +
              ": unknown key " + key;
      return false;
    }
  }

  return flushModule();
}

bool isMathBuiltinOrConstantName(std::string_view name) {
  constexpr std::array<std::string_view, 44> MathBuiltinsAndConstants = {
      "abs",
      "sign",
      "min",
      "max",
      "clamp",
      "lerp",
      "saturate",
      "floor",
      "ceil",
      "round",
      "trunc",
      "fract",
      "sqrt",
      "cbrt",
      "pow",
      "exp",
      "exp2",
      "log",
      "log2",
      "log10",
      "sin",
      "cos",
      "tan",
      "asin",
      "acos",
      "atan",
      "atan2",
      "radians",
      "degrees",
      "sinh",
      "cosh",
      "tanh",
      "asinh",
      "acosh",
      "atanh",
      "fma",
      "hypot",
      "copysign",
      "is_nan",
      "is_inf",
      "is_finite",
      "pi",
      "tau",
      "e",
  };
  return std::find(MathBuiltinsAndConstants.begin(), MathBuiltinsAndConstants.end(), name) !=
         MathBuiltinsAndConstants.end();
}

bool isMathStdlibSurfaceName(std::string_view name) {
  constexpr std::array<std::string_view, 16> MathStdlibSurfaceNames = {
      "Vec2",
      "Vec3",
      "Vec4",
      "Mat2",
      "Mat3",
      "Mat4",
      "Quat",
      "ColorRGB",
      "ColorRGBA",
      "ColorSRGB",
      "ColorSRGBA",
      "quat_to_mat3",
      "quat_to_mat4",
      "mat3_to_quat",
      "srgbToLinearChannel",
      "linearToSrgbChannel",
  };
  return std::find(MathStdlibSurfaceNames.begin(), MathStdlibSurfaceNames.end(), name) !=
         MathStdlibSurfaceNames.end();
}

bool sourceReferencesNonBuiltinMathSymbols(const std::string &source) {
  Lexer lexer(source);
  const std::vector<Token> tokens = lexer.tokenize();
  auto skipIgnorableTokens = [&](size_t cursor) {
    while (cursor < tokens.size() && isIgnorableImportToken(tokens[cursor].kind)) {
      ++cursor;
    }
    return cursor;
  };

  for (size_t scan = 0; scan < tokens.size();) {
    if (tokens[scan].kind == TokenKind::KeywordImport) {
      size_t cursor = skipIgnorableTokens(scan + 1);
      while (cursor < tokens.size()) {
        if (tokens[cursor].kind != TokenKind::Identifier || tokens[cursor].text.empty() ||
            tokens[cursor].text[0] != '/') {
          break;
        }
        size_t next = skipIgnorableTokens(cursor + 1);
        if (!tokens[cursor].text.empty() && tokens[cursor].text.back() == '/' &&
            next < tokens.size() && tokens[next].kind == TokenKind::Star) {
          cursor = next + 1;
        } else {
          ++cursor;
        }
        cursor = skipIgnorableTokens(cursor);
      }
      scan = cursor;
      continue;
    }

    if (tokens[scan].kind != TokenKind::Identifier) {
      ++scan;
      continue;
    }

    const std::string &text = tokens[scan].text;
    if (text.rfind("/std/math/", 0) == 0 && text.size() > 10) {
      const std::string_view name(text.data() + 10, text.size() - 10);
      if (!isMathBuiltinOrConstantName(name)) {
        return true;
      }
      ++scan;
      continue;
    }
    if (text.find('/') == std::string::npos && isMathStdlibSurfaceName(text)) {
      return true;
    }
    ++scan;
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

bool appendStdlibModuleSources(const std::vector<std::string> &importPaths,
                               const std::vector<std::string> &sourceImports,
                               const std::vector<std::string> &implicitKeys,
                               std::string &source,
                               std::string &error,
                               ExpandedSource *expandedSource = nullptr,
                               const std::unordered_set<std::string> &excludedKeys = {}) {
  std::error_code ec;
  std::deque<std::string> pendingKeys;
  std::unordered_set<std::string> queuedKeys;
  std::unordered_map<std::string, StdlibModuleManifest> moduleManifestCache;
  const bool skipMathWildcardStdlibModule = shouldSkipMathWildcardStdlibModule(sourceImports, source);
  for (const auto &importPath : sourceImports) {
    for (const auto &key : collectStdlibAutoIncludeKeys(importPath)) {
      if (skipMathWildcardStdlibModule && importPath == "/std/math/*" && key == "/std/math") {
        continue;
      }
      if (excludedKeys.count(key) > 0) {
        continue;
      }
      if (queuedKeys.insert(key).second) {
        pendingKeys.push_back(key);
      }
    }
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
        if (std::filesystem::exists(moduleFile, ec)) {
          moduleRoot = std::move(moduleFile);
        } else {
          continue;
        }
      }

      auto appendFile = [&](const std::filesystem::path &filePath) -> bool {
        std::filesystem::path absolute = std::filesystem::absolute(filePath, ec);
        if (ec) {
          absolute = filePath;
        }
        const std::string absoluteText = absolute.string();
        if (!seenFiles.insert(absoluteText).second) {
          return true;
        }
        std::ifstream file(absoluteText);
        if (!file) {
          error = "failed to read stdlib file: " + absoluteText;
          return false;
        }
        std::ostringstream buffer;
        buffer << file.rdbuf();
        const std::string contents = buffer.str();
        if (sourceBuilder.has_value()) {
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
          for (const auto &nestedKey : collectStdlibAutoIncludeKeys(nestedImport)) {
            if (queuedKeys.insert(nestedKey).second) {
              pendingKeys.push_back(nestedKey);
            }
          }
        }
        return true;
      };

      if (appendSpecificFile || std::filesystem::is_regular_file(moduleRoot, ec)) {
        if (moduleRoot.extension() == ".prime" && !appendFile(moduleRoot)) {
          return false;
        }
        continue;
      }

      if (!std::filesystem::is_directory(moduleRoot, ec)) {
        continue;
      }

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
        }
        if (!appendFile(entry.path())) {
          return false;
        }
      }
    }
  }
  if (!appended) {
    error = "stdlib import requested but matching stdlib modules were not found";
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

std::string manifestEntryLeafName(const std::string &fullPath) {
  const size_t lastSlash = fullPath.find_last_of('/');
  return lastSlash == std::string::npos ? fullPath : fullPath.substr(lastSlash + 1);
}

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
                                          std::string &error) {
  std::deque<std::string> pendingTexts;
  pendingTexts.push_back(seedText);

  while (!pendingTexts.empty()) {
    const std::string text = std::move(pendingTexts.front());
    pendingTexts.pop_front();

    for (LazyStdlibModule &module : modules) {
      for (const StdlibSymbolManifestEntry &entry : module.entries) {
        if (module.includedPaths.count(entry.path) > 0) {
          continue;
        }
        const std::string leaf = manifestEntryLeafName(entry.path);
        bool matched = containsWholeWord(text, leaf);
        if (!matched) {
          const std::string sugarStructLeaf = constructorSugarStructLeafName(leaf);
          matched = !sugarStructLeaf.empty() && containsWholeWord(text, sugarStructLeaf);
        }
        if (!matched) {
          continue;
        }
        std::string extractedText;
        if (!extractAndVerifyManifestedSymbolSource(
                entry, module.sourceFile.string(), extractedText, error)) {
          return false;
        }
        module.includedPaths.insert(entry.path);
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

struct CompilePipelineImportStageState {
  std::string source;
  ExpandedSource expandedSource;
  std::vector<std::string> sourceImports;
  std::vector<std::string> sourceStdImports;
  std::vector<std::string> implicitStdlibKeys;
  // TODO-5228: module-root keys that were treated as lazy-eligible (symbol
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
                                   DiagnosticSink &diagnosticSink) {
  ImportResolver importResolver;
  if (!importResolver.expandImports(options.inputPath,
                                    out.expandedSource,
                                    error,
                                    options.importPaths)) {
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
      const std::optional<std::filesystem::path> sourceFile =
          resolveSingleFileStdlibModuleSource(options.importPaths, key);
      if (!sourceFile.has_value()) {
        continue;
      }
      const std::optional<std::filesystem::path> manifestPath =
          stdlibSymbolManifestPathForSource(*sourceFile);
      if (!manifestPath.has_value()) {
        continue;
      }
      LazyStdlibModule module;
      module.key = key;
      module.sourceFile = *sourceFile;
      if (!readStdlibSymbolManifest(manifestPath->string(), module.entries, error)) {
        diagnosticSink.setSummary(error);
        return false;
      }
      lazyKeys.insert(key);
      lazyModules.push_back(std::move(module));
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
      const bool moduleLikelyNeeded =
          std::any_of(module.entries.begin(), module.entries.end(),
                     [&](const StdlibSymbolManifestEntry &entry) {
                       return containsWholeWord(out.source, manifestEntryLeafName(entry.path));
                     });
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

  if (shouldAutoIncludeStdlib(out.source) || !out.implicitStdlibKeys.empty()) {
    if (!appendStdlibModuleSources(options.importPaths,
                                   out.sourceStdImports,
                                   out.implicitStdlibKeys,
                                   out.source,
                                   error,
                                   &out.expandedSource,
                                   lazyKeys)) {
      diagnosticSink.setSummary(error);
      return false;
    }
  }

  if (!lazyModules.empty()) {
    std::vector<LazyStdlibExtractedSymbol> extracted;
    if (!computeLazyStdlibModuleClosureSource(lazyModules, out.source, extracted, error)) {
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

  out.sourceImports = importStage.sourceImports;
  return true;
}

void sortParserErrorsForStableOrdering(std::vector<Parser::ErrorInfo> &errors);

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

} // namespace

void addDefaultStdlibInclude(const std::string &inputPath, std::vector<std::string> &importPaths) {
  auto addFromBase = [&](const std::filesystem::path &base) -> bool {
    std::error_code ec;
    std::filesystem::path dir = base;
    if (!std::filesystem::is_directory(dir, ec)) {
      dir = dir.parent_path();
    }
    for (std::filesystem::path current = dir; !current.empty(); current = current.parent_path()) {
      std::filesystem::path candidate = current / "stdlib";
      if (std::filesystem::exists(candidate, ec) && std::filesystem::is_directory(candidate, ec)) {
        std::filesystem::path absoluteCandidate = std::filesystem::absolute(candidate, ec);
        std::string candidateText = absoluteCandidate.string();
        for (const auto &path : importPaths) {
          std::filesystem::path existing = std::filesystem::absolute(path, ec);
          if (!ec && std::filesystem::equivalent(existing, absoluteCandidate, ec)) {
            return true;
          }
          if (path == candidateText) {
            return true;
          }
        }
        importPaths.push_back(candidateText);
        return true;
      }
      if (current == current.root_path()) {
        break;
      }
    }
    return false;
  };

  if (!inputPath.empty()) {
    std::error_code ec;
    std::filesystem::path resolved = std::filesystem::absolute(inputPath, ec);
    if (ec) {
      resolved = std::filesystem::path(inputPath);
    }
    if (addFromBase(resolved)) {
      return;
    }
  }

  std::error_code ec;
  std::filesystem::path cwd = std::filesystem::current_path(ec);
  if (!ec) {
    addFromBase(cwd);
  }
}

bool runCompilePipeline(const Options &options,
                        CompilePipelineOutput &output,
                        CompilePipelineErrorStage &errorStage,
                        std::string &error,
                        CompilePipelineDiagnosticInfo *diagnosticInfo) {
  CompilePipelineBenchmarkConfig benchmarkConfig;
  const CompilePipelineRunConfig runConfig =
      makeCompilePipelineRunConfigFromOptions(options, benchmarkConfig);
  return runCompilePipeline(options, runConfig, output, errorStage, error, diagnosticInfo);
}

namespace {

CompilePipelineFailureResult makeCompilePipelineFailureResult(
    CompilePipelineOutput &&output,
    CompilePipelineErrorStage errorStage,
    const std::string &error,
    const CompilePipelineDiagnosticInfo *diagnosticInfo) {
  CompilePipelineFailureResult result;
  result.program = std::move(output.program);
  result.semanticProgram = std::move(output.semanticProgram);
  result.hasSemanticProgram = output.hasSemanticProgram;
  result.semanticProductDecision = output.semanticProductDecision;
  result.semanticProductRequested = output.semanticProductRequested;
  result.semanticProductBuilt = output.semanticProductBuilt;
  result.semanticPhaseCounters = output.semanticPhaseCounters;
  result.hasSemanticPhaseCounters = output.hasSemanticPhaseCounters;
  result.expandedSource = std::move(output.expandedSource);
  result.filteredSource = std::move(output.filteredSource);
  result.dumpOutput = std::move(output.dumpOutput);
  result.hasDumpOutput = output.hasDumpOutput;
  result.failure = std::move(output.failure);
  if (!output.hasFailure) {
    result.failure.stage = errorStage;
    result.failure.message = error;
    if (diagnosticInfo != nullptr) {
      result.failure.diagnosticInfo = *diagnosticInfo;
    }
  }
  if (result.failure.message.empty()) {
    result.failure.message = error;
  }
  return result;
}

} // namespace

CompilePipelineResult runCompilePipelineResult(
    const Options &options,
    CompilePipelineErrorStage &errorStage,
    std::string &error,
    CompilePipelineDiagnosticInfo *diagnosticInfo) {
  CompilePipelineBenchmarkConfig benchmarkConfig;
  const CompilePipelineRunConfig runConfig =
      makeCompilePipelineRunConfigFromOptions(options, benchmarkConfig);
  return runCompilePipelineResult(options, runConfig, errorStage, error, diagnosticInfo);
}

CompilePipelineResult runCompilePipelineResult(
    const Options &options,
    const CompilePipelineRunConfig &runConfig,
    CompilePipelineErrorStage &errorStage,
    std::string &error,
    CompilePipelineDiagnosticInfo *diagnosticInfo) {
  CompilePipelineOutput output;
  if (runCompilePipeline(options, runConfig, output, errorStage, error, diagnosticInfo)) {
    return CompilePipelineSuccessResult{std::move(output)};
  }
  return makeCompilePipelineFailureResult(
      std::move(output), errorStage, error, diagnosticInfo);
}

bool runCompilePipeline(const Options &options,
                        const CompilePipelineRunConfig &runConfig,
                        CompilePipelineOutput &output,
                        CompilePipelineErrorStage &errorStage,
                        std::string &error,
                        CompilePipelineDiagnosticInfo *diagnosticInfo) {
  errorStage = CompilePipelineErrorStage::None;
  output = {};
  error.clear();
  CompilePipelineDiagnosticInfo capturedDiagnosticInfo;
  DiagnosticSink diagnosticSink(&capturedDiagnosticInfo);
  diagnosticSink.reset();
  if (diagnosticInfo != nullptr) {
    *diagnosticInfo = {};
  }
  const bool benchmarkAstHeapEstimate =
      std::getenv("PRIMEC_BENCHMARK_COMPILE_AST_HEAP_ESTIMATE") != nullptr;

  auto failPipeline = [&](CompilePipelineErrorStage stage,
                          const std::string &message,
                          const CompilePipelineDiagnosticInfo &info) -> bool {
    errorStage = stage;
    output.failure.stage = stage;
    output.failure.message = message;
    output.failure.diagnosticInfo = info;
    output.hasFailure = true;
    if (diagnosticInfo != nullptr) {
      *diagnosticInfo = info;
    }
    return false;
  };

  CompilePipelineImportStageState importStage;
  if (!runCompilePipelineImportStage(options,
                                     importStage,
                                     error,
                                     diagnosticSink)) {
    return failPipeline(CompilePipelineErrorStage::Import, error, capturedDiagnosticInfo);
  }
  output.expandedSource = importStage.expandedSource;

  CompilePipelinePreParseStageState preParseStage;
  if (!runCompilePipelineTransformStage(options,
                                        importStage,
                                        preParseStage,
                                        error,
                                        diagnosticSink)) {
    return failPipeline(CompilePipelineErrorStage::Transform, error, capturedDiagnosticInfo);
  }

  output.filteredSource = preParseStage.filteredSource;

  const DumpStage dumpStage = parseDumpStage(options.dumpStage);

  if (dumpStage == DumpStage::PreAst) {
    output.dumpOutput = preParseStage.filteredSource;
    output.hasDumpOutput = true;
    return true;
  }

  CompilePipelineParsedProgramStageState parsedStage;
  if (!runCompilePipelineParseStage(options,
                                    importStage.expandedSource,
                                    preParseStage,
                                    parsedStage,
                                    error,
                                    diagnosticSink)) {
    return failPipeline(CompilePipelineErrorStage::Parse, error, capturedDiagnosticInfo);
  }
  output.program = std::move(parsedStage.program);

  if (benchmarkAstHeapEstimate) {
    emitProgramHeapEstimate(output.program, "post-parse-pre-semantics");
  }

  if (dumpStage != DumpStage::None && dumpStage != DumpStage::AstSemantic &&
      dumpStage != DumpStage::SemanticProduct && dumpStage != DumpStage::TypeGraph) {
    if (dumpStage == DumpStage::Ast) {
      AstPrinter printer;
      output.dumpOutput = printer.print(output.program);
      output.hasDumpOutput = true;
      return true;
    }
    if (dumpStage == DumpStage::Ir) {
      IrPrinter printer;
      output.dumpOutput = printer.print(output.program);
      output.hasDumpOutput = true;
      return true;
    }
    error = options.dumpStage;
    diagnosticSink.setSummary(error);
    return failPipeline(CompilePipelineErrorStage::UnsupportedDumpStage, error, capturedDiagnosticInfo);
  }

  if (!options.semanticTransformRules.empty()) {
    applySemanticTransformRules(output.program, options.semanticTransformRules);
  }

  if (dumpStage == DumpStage::TypeGraph) {
    semantics::TypeResolutionGraph graph;
    if (!semantics::buildTypeResolutionGraphForProgram(
            output.program, options.entryPath, options.semanticTransforms, error, graph)) {
      diagnosticSink.setSummary(error);
      return failPipeline(CompilePipelineErrorStage::Semantic, error, capturedDiagnosticInfo);
    }
    output.dumpOutput = semantics::formatTypeResolutionGraph(graph);
    output.hasDumpOutput = true;
    return true;
  }

  Semantics semantics;
  SemanticDiagnosticInfo semanticDiagnosticInfo;
  SemanticProgram semanticProgram;
  const CompilePipelineSemanticProductDecision semanticProductDecision =
      decideSemanticProductDecision(dumpStage, runConfig);
  const bool needsSemanticProduct = semanticProductDecisionRequestsBuild(semanticProductDecision);
  const CompilePipelineBenchmarkConfig *benchmarkConfig = runConfig.benchmark;
  SemanticProductBuildConfig semanticProductBuildConfig;
  const SemanticProductBuildConfig *semanticProductBuildConfigPtr = nullptr;
  if (benchmarkConfig != nullptr &&
      (benchmarkConfig->semanticNoFactEmission || benchmarkConfig->semanticFactFamiliesSpecified)) {
    semanticProductBuildConfig.disableAllCollectors = benchmarkConfig->semanticNoFactEmission;
    semanticProductBuildConfig.collectorAllowlistSpecified =
        benchmarkConfig->semanticFactFamiliesSpecified;
    semanticProductBuildConfig.collectorAllowlist = benchmarkConfig->semanticFactFamilies;
    for (const auto &collectorFamily : semanticProductBuildConfig.collectorAllowlist) {
      if (!isKnownSemanticCollectorFamily(collectorFamily)) {
        error = "unknown benchmark semantic collector family: " + collectorFamily;
        diagnosticSink.setSummary(error);
        return failPipeline(CompilePipelineErrorStage::Semantic, error, capturedDiagnosticInfo);
      }
    }
    semanticProductBuildConfigPtr = &semanticProductBuildConfig;
  }
  const uint32_t benchmarkSemanticDefinitionValidationWorkerCount =
      semanticDefinitionValidationWorkerCount(benchmarkConfig);
  SemanticPhaseCounters benchmarkSemanticPhaseCounters;
  const bool benchmarkSemanticCountersRequested =
      semanticBenchmarkCountersRequested(benchmarkConfig);
  SemanticPhaseCounters *benchmarkSemanticPhaseCountersPtr =
      benchmarkSemanticCountersRequested ? &benchmarkSemanticPhaseCounters : nullptr;
  const bool benchmarkSemanticConfigRequested =
      semanticBenchmarkValidationConfigRequested(benchmarkConfig,
                                                benchmarkSemanticDefinitionValidationWorkerCount);
  output.semanticProductDecision = semanticProductDecision;
  output.semanticProductRequested = needsSemanticProduct;
  bool semanticValidationOk = false;
  if (benchmarkSemanticConfigRequested || benchmarkSemanticCountersRequested) {
    SemanticValidationBenchmarkConfig benchmarkConfig;
    benchmarkConfig.definitionValidationWorkerCount = benchmarkSemanticDefinitionValidationWorkerCount;
    benchmarkConfig.disableMethodTargetMemoization =
        runConfig.benchmark->semanticDisableMethodTargetMemoization;
    benchmarkConfig.graphLocalAutoLegacyKeyShadow =
        runConfig.benchmark->semanticGraphLocalAutoLegacyKeyShadow;
    benchmarkConfig.graphLocalAutoLegacySideChannelShadow =
        runConfig.benchmark->semanticGraphLocalAutoLegacySideChannelShadow;
    benchmarkConfig.disableGraphLocalAutoDependencyScratchPmr =
        runConfig.benchmark->semanticDisableGraphLocalAutoDependencyScratchPmr;

    SemanticValidationBenchmarkObserver benchmarkObserver;
    benchmarkObserver.phaseCounters = benchmarkSemanticPhaseCountersPtr;
    benchmarkObserver.allocationCountersEnabled = runConfig.benchmark->semanticAllocationCounters;
    benchmarkObserver.rssCheckpointsEnabled = runConfig.benchmark->semanticRssCheckpoints;

    semanticValidationOk = validateSemanticsForBenchmark(output.program,
                                                         options.entryPath,
                                                         error,
                                                         options.defaultEffects,
                                                         options.entryDefaultEffects,
                                                         options.semanticTransforms,
                                                         &semanticDiagnosticInfo,
                                                         options.collectDiagnostics,
                                                         needsSemanticProduct ? &semanticProgram : nullptr,
                                                         semanticProductBuildConfigPtr,
                                                         benchmarkConfig,
                                                         benchmarkObserver);
  } else {
    semanticValidationOk = semantics.validate(output.program,
                                              options.entryPath,
                                              error,
                                              options.defaultEffects,
                                              options.entryDefaultEffects,
                                              options.semanticTransforms,
                                              &semanticDiagnosticInfo,
                                              options.collectDiagnostics,
                                              needsSemanticProduct ? &semanticProgram : nullptr,
                                              semanticProductBuildConfigPtr);
  }
  if (!semanticValidationOk) {
    // TODO-5228: the closure-scan heuristic that decides which manifested
    // symbols to splice in is purely syntactic (a whole-word name scan),
    // so it can miss a symbol the program actually needed. When that
    // happens, nothing from the lazily-imported module ends up in the
    // compiled buffer, and the pre-existing "unknown import path: X"
    // check (which fires whenever a wildcard-imported root has zero
    // definitions) is what actually surfaces - not the more specific
    // "unknown call target"/"unknown identifier" a whole-file splice would
    // have produced. Rewrite it into the clearer, TODO-5228-specific
    // message the plan calls for, rather than letting a confusing
    // downstream error stand.
    for (const std::string &lazyKey : importStage.lazyStdlibModuleKeys) {
      const std::string unknownImportPrefix = "unknown import path: " + lazyKey;
      if (error.compare(0, unknownImportPrefix.size(), unknownImportPrefix) == 0) {
        error = "unknown symbol in imported library " + lazyKey +
                " (--experimental-lazy-stdlib-imports could not find any "
                "manifested symbol matching a name referenced by this "
                "program; disable the flag to see the underlying "
                "diagnostic, or verify the spelling)";
        semanticDiagnosticInfo.message = error;
        break;
      }
    }
    if (semanticDiagnosticInfo.message.empty()) {
      semanticDiagnosticInfo.message = error;
    }
    mapAndSortDiagnosticReportSpansToSourceUnits(importStage.expandedSource,
                                                 semanticDiagnosticInfo);
    return failPipeline(CompilePipelineErrorStage::Semantic, error, semanticDiagnosticInfo);
  }
  if (benchmarkSemanticPhaseCountersPtr != nullptr) {
    output.semanticPhaseCounters = benchmarkSemanticPhaseCounters;
    output.hasSemanticPhaseCounters = true;
  }

  if (benchmarkAstHeapEstimate) {
    emitProgramHeapEstimate(output.program, "post-semantics");
  }

  if (needsSemanticProduct) {
    output.semanticProgram = std::move(semanticProgram);
    output.hasSemanticProgram = true;
    output.semanticProductBuilt = true;
  }

  if (!validateGraphicsBackendSupport(output.program, options, error, &capturedDiagnosticInfo)) {
    return failPipeline(CompilePipelineErrorStage::Semantic, error, capturedDiagnosticInfo);
  }

  if (dumpStage == DumpStage::SemanticProduct) {
    if (!output.hasSemanticProgram) {
      error = "semantic-product dump requested without semantic product";
      diagnosticSink.setSummary(error);
      return failPipeline(CompilePipelineErrorStage::Semantic, error, capturedDiagnosticInfo);
    }
    output.dumpOutput = formatSemanticProgram(output.semanticProgram);
    output.hasDumpOutput = true;
    return true;
  }

  if (dumpStage == DumpStage::AstSemantic) {
    AstPrinter printer;
    output.dumpOutput = printer.print(output.program);
    output.hasDumpOutput = true;
    return true;
  }

  return true;
}

} // namespace primec
