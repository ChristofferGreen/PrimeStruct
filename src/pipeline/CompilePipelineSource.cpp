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
  if (dumpStage == "ir-lowered" || dumpStage == "ir_lowered") {
    return DumpStage::IrLowered;
  }
  if (dumpStage == "ir-optimized" || dumpStage == "ir_optimized") {
    return DumpStage::IrOptimized;
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
  std::ostringstream line;
  line << "[benchmark-ast-heap-estimate] "
            << "{\"stage\":\"" << stage
            << "\",\"definitions\":" << stats.definitions
            << ",\"executions\":" << stats.executions
            << ",\"exprs\":" << stats.exprs
            << ",\"transforms\":" << stats.transforms
            << ",\"strings\":" << stats.strings
            << ",\"dynamic_bytes\":" << stats.dynamicBytes
            << "}";
  primec::support::emitBenchmarkLine(line.str());
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

// std::filesystem::exists() matches case-insensitively on case-insensitive
// filesystems (the macOS default), so a stdlib module-root key derived from
// an imported symbol name (e.g. "/std/maybe/Maybe" from "import
// /std/maybe/Maybe") can spuriously "exist" against a differently-cased
// sibling file (stdlib's actual "maybe.prime"), stealing the file from the
// correctly-cased parent module key that should have claimed it. Require an
// exact-case match against the real directory entry before accepting it.
bool existsWithExactCase(const std::filesystem::path &path, std::error_code &ec) {
  if (!std::filesystem::exists(path, ec)) {
    return false;
  }
  const std::filesystem::path parent = path.parent_path();
  const std::string wantName = path.filename().string();
  for (const auto &entry : std::filesystem::directory_iterator(parent, ec)) {
    if (ec) {
      return false;
    }
    if (entry.path().filename().string() == wantName) {
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

} // namespace compile_pipeline_detail
} // namespace primec
