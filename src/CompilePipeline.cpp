#include "primec/CompilePipeline.h"

#include "primec/AstPrinter.h"
#include "primec/ImportResolver.h"
#include "primec/IrPrinter.h"
#include "primec/Lexer.h"
#include "primec/Parser.h"
#include "primec/Semantics.h"
#include "primec/TextFilterPipeline.h"
#include "primec/TransformRules.h"
#include "semantics/TypeResolutionGraph.h"

#include <algorithm>
#include <cctype>
#include <deque>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <unordered_set>

namespace primec {
namespace {

enum class DumpStage {
  None,
  PreAst,
  Ast,
  Ir,
  AstSemantic,
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
  if (dumpStage == "type_graph" || dumpStage == "type-graph") {
    return DumpStage::TypeGraph;
  }
  return DumpStage::Unsupported;
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

std::vector<std::string> collectStdlibAutoIncludeKeys(const std::string &importPath) {
  std::vector<std::string> keys;
  if (importPath.rfind("/std/", 0) != 0) {
    return keys;
  }

  std::string key = importPath;
  if (key.size() >= 2 && key.compare(key.size() - 2, 2, "/*") == 0) {
    key.erase(key.size() - 2);
  }

  while (!key.empty()) {
    keys.push_back(key);
    const size_t slash = key.find_last_of('/');
    if (slash <= std::string("/std").size()) {
      break;
    }
    key.erase(slash);
  }

  return keys;
}

bool appendStdlibModuleSources(const std::vector<std::string> &importPaths,
                               const std::vector<std::string> &sourceImports,
                               std::string &source,
                               std::string &error) {
  std::error_code ec;
  std::deque<std::string> pendingKeys;
  std::unordered_set<std::string> queuedKeys;
  for (const auto &importPath : sourceImports) {
    for (const auto &key : collectStdlibAutoIncludeKeys(importPath)) {
      if (queuedKeys.insert(key).second) {
        pendingKeys.push_back(key);
      }
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
  DiagnosticSink diagnosticSink(diagnosticInfo);
  diagnosticSink.reset();

  std::string source;
  ImportResolver importResolver;
  if (!importResolver.expandImports(options.inputPath, source, error, options.importPaths)) {
    errorStage = CompilePipelineErrorStage::Import;
    diagnosticSink.setSummary(error);
    return false;
  }

  const std::vector<std::string> sourceImports = collectSourceImportPaths(source);
  const std::vector<std::string> sourceStdImports = collectStdImportPaths(source);
  output.program.sourceImports = sourceImports;

  if (shouldAutoIncludeStdlib(source)) {
    if (!appendStdlibModuleSources(options.importPaths, sourceStdImports, source, error)) {
      errorStage = CompilePipelineErrorStage::Import;
      diagnosticSink.setSummary(error);
      return false;
    }
  }

  TextFilterPipeline textPipeline;
  TextFilterOptions textOptions;
  textOptions.enabledFilters = options.textFilters;
  textOptions.rules = options.textTransformRules;
  textOptions.allowEnvelopeTransforms = options.allowEnvelopeTextTransforms;

  if (!textPipeline.apply(source, output.filteredSource, error, textOptions)) {
    errorStage = CompilePipelineErrorStage::Transform;
    diagnosticSink.setSummary(error);
    return false;
  }

  const DumpStage dumpStage = parseDumpStage(options.dumpStage);

  if (dumpStage == DumpStage::PreAst) {
    output.dumpOutput = output.filteredSource;
    output.hasDumpOutput = true;
    return true;
  }

  Lexer lexer(output.filteredSource);
  Parser parser(lexer.tokenize(), !options.requireCanonicalSyntax);
  Parser::ErrorInfo parserErrorInfo;
  std::vector<Parser::ErrorInfo> parserErrors;
  if (!parser.parse(
          output.program, error, &parserErrorInfo, options.collectDiagnostics ? &parserErrors : nullptr)) {
    errorStage = CompilePipelineErrorStage::Parse;
    if (options.collectDiagnostics) {
      if (parserErrors.empty() && !parserErrorInfo.message.empty()) {
        parserErrors.push_back(parserErrorInfo);
      }
      sortParserErrorsForStableOrdering(parserErrors);
      if (!parserErrors.empty()) {
        const Parser::ErrorInfo &first = parserErrors.front();
        if (!first.message.empty()) {
          if (first.line > 0 && first.column > 0) {
            error = first.message + " at " + std::to_string(first.line) + ":" + std::to_string(first.column);
          } else {
            error = first.message;
          }
        }
      }
    }
    if (diagnosticInfo != nullptr) {
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
          diagnosticSink.capturePrimarySpanIfUnset(parserErrorInfo.line, parserErrorInfo.column);
        }
      }
    }
    return false;
  }
  output.program.sourceImports = sourceImports;

  if (dumpStage != DumpStage::None && dumpStage != DumpStage::AstSemantic && dumpStage != DumpStage::TypeGraph) {
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
    errorStage = CompilePipelineErrorStage::UnsupportedDumpStage;
    error = options.dumpStage;
    diagnosticSink.setSummary(error);
    return false;
  }

  if (!options.semanticTransformRules.empty()) {
    applySemanticTransformRules(output.program, options.semanticTransformRules);
  }

  if (dumpStage == DumpStage::TypeGraph) {
    semantics::TypeResolutionGraph graph;
    if (!semantics::buildTypeResolutionGraphForProgram(
            output.program, options.entryPath, options.semanticTransforms, error, graph)) {
      errorStage = CompilePipelineErrorStage::Semantic;
      diagnosticSink.setSummary(error);
      return false;
    }
    output.dumpOutput = semantics::formatTypeResolutionGraph(graph);
    output.hasDumpOutput = true;
    return true;
  }

  Semantics semantics;
  SemanticDiagnosticInfo semanticDiagnosticInfo;
  if (!semantics.validate(output.program,
                          options.entryPath,
                          error,
                          options.defaultEffects,
                          options.entryDefaultEffects,
                          options.semanticTransforms,
                          &semanticDiagnosticInfo,
                          options.collectDiagnostics)) {
    errorStage = CompilePipelineErrorStage::Semantic;
    if (diagnosticInfo != nullptr) {
      *diagnosticInfo = semanticDiagnosticInfo;
      if (diagnosticInfo->message.empty()) {
        diagnosticSink.setSummary(error);
      }
    }
    return false;
  }

  if (!validateGraphicsBackendSupport(output.program, options, error, diagnosticInfo)) {
    errorStage = CompilePipelineErrorStage::Semantic;
    return false;
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
