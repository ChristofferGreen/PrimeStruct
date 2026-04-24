#include "primec/CompilePipeline.h"

#include "primec/AstMemory.h"
#include "primec/AstPrinter.h"
#include "primec/ImportResolver.h"
#include "primec/IrPrinter.h"
#include "primec/Lexer.h"
#include "primec/Parser.h"
#include "primec/Semantics.h"
#include "primec/SemanticsBenchmark.h"
#include "primec/StdlibSurfaceRegistry.h"
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
#include <sstream>
#include <string_view>
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

constexpr std::array<std::string_view, 14> SemanticCollectorFamilies = {
    "definitions",
    "executions",
    "direct_call_targets",
    "method_call_targets",
    "bridge_path_choices",
    "callable_summaries",
    "type_metadata",
    "struct_field_metadata",
    "binding_facts",
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

CompilePipelineSemanticProductDecision decideSemanticProductDecision(DumpStage dumpStage, const Options &options) {
  if (options.benchmarkForceSemanticProduct.has_value()) {
    return *options.benchmarkForceSemanticProduct
               ? CompilePipelineSemanticProductDecision::ForcedOnForBenchmark
               : CompilePipelineSemanticProductDecision::ForcedOffForBenchmark;
  }
  if (dumpStage == DumpStage::AstSemantic) {
    return CompilePipelineSemanticProductDecision::SkipForAstSemanticDump;
  }
  if (options.skipSemanticProductForNonConsumingPath) {
    return CompilePipelineSemanticProductDecision::SkipForNonConsumingPath;
  }
  return CompilePipelineSemanticProductDecision::RequireForConsumingPath;
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
  std::vector<std::string> keys;
  Lexer lexer(source);
  const std::vector<Token> tokens = lexer.tokenize();
  bool sawBuiltinSoaVector = false;
  bool sawBuiltinSoaToAos = false;
  for (const Token &token : tokens) {
    if (token.kind != TokenKind::Identifier) {
      continue;
    }
    if (token.text == "soa_vector" || token.text == "/soa_vector") {
      sawBuiltinSoaVector = true;
      continue;
    }
    if (token.text == "to_aos" ||
        token.text == "/to_aos" ||
        token.text == "/std/collections/soa_vector/to_aos") {
      sawBuiltinSoaToAos = true;
      continue;
    }
  }
  if (sawBuiltinSoaVector && sawBuiltinSoaToAos) {
    keys.push_back("/std/collections/soa_vector_conversions");
  }
  return keys;
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
                               std::string &error) {
  std::error_code ec;
  std::deque<std::string> pendingKeys;
  std::unordered_set<std::string> queuedKeys;
  const bool skipMathWildcardStdlibModule = shouldSkipMathWildcardStdlibModule(sourceImports, source);
  for (const auto &importPath : sourceImports) {
    for (const auto &key : collectStdlibAutoIncludeKeys(importPath)) {
      if (skipMathWildcardStdlibModule && importPath == "/std/math/*" && key == "/std/math") {
        continue;
      }
      if (queuedKeys.insert(key).second) {
        pendingKeys.push_back(key);
      }
    }
  }
  for (const auto &key : implicitKeys) {
    if (queuedKeys.insert(key).second) {
      pendingKeys.push_back(key);
    }
  }
  if (pendingKeys.empty()) {
    return true;
  }

  std::unordered_set<std::string> seenFiles;
  std::unordered_set<std::string> processedKeys;
  bool appended = false;
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

      std::filesystem::path moduleRoot;
      bool appendSpecificFile = false;
      if (key == "/std/gfx/experimental") {
        moduleRoot = root / "std" / "gfx" / "experimental.prime";
        appendSpecificFile = true;
      } else if (key == "/std/gfx") {
        moduleRoot = root / "std" / "gfx" / "gfx.prime";
        appendSpecificFile = true;
      } else {
        const std::string relative = key.substr(std::string("/std/").size());
        moduleRoot = root / "std" / relative;
      }
      if (!std::filesystem::exists(moduleRoot, ec)) {
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
        source.append("\n");
        source.append(contents);
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
          if (stem.rfind("experimental_", 0) == 0) {
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
  return true;
}

bool isGraphicsImportPath(const std::string &importPath) {
  if (importPath == "/std/gfx/*" || importPath == "/std/gfx") {
    return true;
  }
  return importPath.rfind("/std/gfx/", 0) == 0;
}

std::string unsupportedGraphicsTargetName(const Options &options) {
  if (options.emitKind == "wasm") {
    return options.wasmProfile == "browser" ? "wasm-browser" : "wasm-wasi";
  }
  if (options.emitKind == "glsl" || options.emitKind == "glsl-ir") {
    return "glsl";
  }
  if (options.emitKind == "spirv" || options.emitKind == "spirv-ir") {
    return "spirv";
  }
  return "";
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
  std::vector<std::string> sourceImports;
  std::vector<std::string> sourceStdImports;
  std::vector<std::string> implicitStdlibKeys;
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
                                    out.source,
                                    error,
                                    options.importPaths)) {
    diagnosticSink.setSummary(error);
    return false;
  }

  out.sourceImports = collectSourceImportPaths(out.source);
  out.sourceStdImports = collectStdImportPaths(out.source);
  out.implicitStdlibKeys = collectImplicitStdlibAutoIncludeKeys(out.source);
  if (shouldAutoIncludeStdlib(out.source) || !out.implicitStdlibKeys.empty()) {
    if (!appendStdlibModuleSources(options.importPaths,
                                   out.sourceStdImports,
                                   out.implicitStdlibKeys,
                                   out.source,
                                   error)) {
      diagnosticSink.setSummary(error);
      return false;
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
      diagnosticSink.setRecords(std::move(records));
    } else {
      diagnosticSink.setSummary(parserErrorInfo.message);
      if (parserErrorInfo.line > 0 && parserErrorInfo.column > 0) {
        diagnosticSink.capturePrimarySpanIfUnset(parserErrorInfo.line,
                                                 parserErrorInfo.column);
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
      decideSemanticProductDecision(dumpStage, options);
  const bool needsSemanticProduct = semanticProductDecisionRequestsBuild(semanticProductDecision);
  SemanticProductBuildConfig semanticProductBuildConfig;
  const SemanticProductBuildConfig *semanticProductBuildConfigPtr = nullptr;
  if (options.benchmarkSemanticNoFactEmission || options.benchmarkSemanticFactFamiliesSpecified) {
    semanticProductBuildConfig.disableAllCollectors = options.benchmarkSemanticNoFactEmission;
    semanticProductBuildConfig.collectorAllowlistSpecified = options.benchmarkSemanticFactFamiliesSpecified;
    semanticProductBuildConfig.collectorAllowlist = options.benchmarkSemanticFactFamilies;
    for (const auto &collectorFamily : semanticProductBuildConfig.collectorAllowlist) {
      if (!isKnownSemanticCollectorFamily(collectorFamily)) {
        error = "unknown benchmark semantic collector family: " + collectorFamily;
        diagnosticSink.setSummary(error);
        return failPipeline(CompilePipelineErrorStage::Semantic, error, capturedDiagnosticInfo);
      }
    }
    semanticProductBuildConfigPtr = &semanticProductBuildConfig;
  }
  uint32_t benchmarkSemanticDefinitionValidationWorkerCount = 1;
  if (options.benchmarkSemanticDefinitionValidationWorkerCount.has_value()) {
    benchmarkSemanticDefinitionValidationWorkerCount =
        *options.benchmarkSemanticDefinitionValidationWorkerCount;
  } else if (options.benchmarkSemanticTwoChunkDefinitionValidation) {
    benchmarkSemanticDefinitionValidationWorkerCount = 2;
  }
  SemanticPhaseCounters benchmarkSemanticPhaseCounters;
  const bool benchmarkSemanticCountersRequested =
      options.benchmarkSemanticPhaseCounters ||
      options.benchmarkSemanticAllocationCounters ||
      options.benchmarkSemanticRssCheckpoints;
  SemanticPhaseCounters *benchmarkSemanticPhaseCountersPtr =
      benchmarkSemanticCountersRequested ? &benchmarkSemanticPhaseCounters : nullptr;
  const bool benchmarkSemanticConfigRequested =
      benchmarkSemanticDefinitionValidationWorkerCount != 1 ||
      options.benchmarkSemanticDisableMethodTargetMemoization ||
      options.benchmarkSemanticGraphLocalAutoLegacyKeyShadow ||
      options.benchmarkSemanticGraphLocalAutoLegacySideChannelShadow ||
      options.benchmarkSemanticDisableGraphLocalAutoDependencyScratchPmr;
  output.semanticProductDecision = semanticProductDecision;
  output.semanticProductRequested = needsSemanticProduct;
  bool semanticValidationOk = false;
  if (benchmarkSemanticConfigRequested || benchmarkSemanticCountersRequested) {
    SemanticValidationBenchmarkConfig benchmarkConfig;
    benchmarkConfig.definitionValidationWorkerCount = benchmarkSemanticDefinitionValidationWorkerCount;
    benchmarkConfig.disableMethodTargetMemoization =
        options.benchmarkSemanticDisableMethodTargetMemoization;
    benchmarkConfig.graphLocalAutoLegacyKeyShadow =
        options.benchmarkSemanticGraphLocalAutoLegacyKeyShadow;
    benchmarkConfig.graphLocalAutoLegacySideChannelShadow =
        options.benchmarkSemanticGraphLocalAutoLegacySideChannelShadow;
    benchmarkConfig.disableGraphLocalAutoDependencyScratchPmr =
        options.benchmarkSemanticDisableGraphLocalAutoDependencyScratchPmr;

    SemanticValidationBenchmarkObserver benchmarkObserver;
    benchmarkObserver.phaseCounters = benchmarkSemanticPhaseCountersPtr;
    benchmarkObserver.allocationCountersEnabled = options.benchmarkSemanticAllocationCounters;
    benchmarkObserver.rssCheckpointsEnabled = options.benchmarkSemanticRssCheckpoints;

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
    if (semanticDiagnosticInfo.message.empty()) {
      semanticDiagnosticInfo.message = error;
    }
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
