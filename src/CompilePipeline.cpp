#include "primec/CompilePipeline.h"

#include "primec/AstPrinter.h"
#include "primec/IncludeResolver.h"
#include "primec/IrPrinter.h"
#include "primec/Lexer.h"
#include "primec/Parser.h"
#include "primec/Semantics.h"
#include "primec/TextFilterPipeline.h"
#include "primec/TransformRules.h"

#include <cctype>
#include <filesystem>
#include <fstream>
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

bool appendStdlibSources(const std::vector<std::string> &includePaths,
                         std::string &source,
                         std::string &error) {
  std::error_code ec;
  std::unordered_set<std::string> seen;
  bool appended = false;
  for (const auto &pathText : includePaths) {
    std::filesystem::path root(pathText);
    if (root.filename() != "stdlib") {
      continue;
    }
    if (!std::filesystem::exists(root, ec) || !std::filesystem::is_directory(root, ec)) {
      continue;
    }
    for (const auto &entry : std::filesystem::recursive_directory_iterator(root, ec)) {
      if (ec) {
        error = "failed to scan stdlib: " + root.string();
        return false;
      }
      if (!entry.is_regular_file(ec)) {
        continue;
      }
      if (entry.path().extension() != ".prime") {
        continue;
      }
      std::filesystem::path absolute = std::filesystem::absolute(entry.path(), ec);
      if (ec) {
        absolute = entry.path();
      }
      std::string absoluteText = absolute.string();
      if (!seen.insert(absoluteText).second) {
        continue;
      }
      std::ifstream file(absoluteText);
      if (!file) {
        error = "failed to read stdlib file: " + absoluteText;
        return false;
      }
      std::ostringstream buffer;
      buffer << file.rdbuf();
      source.append("\n");
      source.append(buffer.str());
      appended = true;
    }
  }
  if (!appended) {
    error = "stdlib import requested but no stdlib files were found";
    return false;
  }
  return true;
}

} // namespace

void addDefaultStdlibInclude(const std::string &inputPath, std::vector<std::string> &includePaths) {
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
        for (const auto &path : includePaths) {
          std::filesystem::path existing = std::filesystem::absolute(path, ec);
          if (!ec && std::filesystem::equivalent(existing, absoluteCandidate, ec)) {
            return true;
          }
          if (path == candidateText) {
            return true;
          }
        }
        includePaths.push_back(candidateText);
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
  if (diagnosticInfo != nullptr) {
    *diagnosticInfo = {};
  }

  std::string source;
  IncludeResolver includeResolver;
  if (!includeResolver.expandIncludes(options.inputPath, source, error, options.includePaths)) {
    errorStage = CompilePipelineErrorStage::Include;
    return false;
  }

  if (shouldAutoIncludeStdlib(source)) {
    if (!appendStdlibSources(options.includePaths, source, error)) {
      errorStage = CompilePipelineErrorStage::Include;
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
  if (!parser.parse(output.program, error, &parserErrorInfo)) {
    errorStage = CompilePipelineErrorStage::Parse;
    if (diagnosticInfo != nullptr) {
      diagnosticInfo->normalizedMessage = parserErrorInfo.message;
      if (parserErrorInfo.line > 0 && parserErrorInfo.column > 0) {
        diagnosticInfo->primarySpan.file = options.inputPath;
        diagnosticInfo->primarySpan.line = parserErrorInfo.line;
        diagnosticInfo->primarySpan.column = parserErrorInfo.column;
        diagnosticInfo->primarySpan.endLine = parserErrorInfo.line;
        diagnosticInfo->primarySpan.endColumn = parserErrorInfo.column;
        diagnosticInfo->hasPrimarySpan = true;
      }
    }
    return false;
  }

  if (dumpStage != DumpStage::None && dumpStage != DumpStage::AstSemantic) {
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
    return false;
  }

  if (!options.semanticTransformRules.empty()) {
    applySemanticTransformRules(output.program, options.semanticTransformRules);
  }

  Semantics semantics;
  SemanticDiagnosticInfo semanticDiagnosticInfo;
  if (!semantics.validate(output.program,
                          options.entryPath,
                          error,
                          options.defaultEffects,
                          options.entryDefaultEffects,
                          options.semanticTransforms,
                          &semanticDiagnosticInfo)) {
    errorStage = CompilePipelineErrorStage::Semantic;
    if (diagnosticInfo != nullptr) {
      diagnosticInfo->normalizedMessage = error;
      if (semanticDiagnosticInfo.line > 0 && semanticDiagnosticInfo.column > 0) {
        diagnosticInfo->primarySpan.file = options.inputPath;
        diagnosticInfo->primarySpan.line = semanticDiagnosticInfo.line;
        diagnosticInfo->primarySpan.column = semanticDiagnosticInfo.column;
        diagnosticInfo->primarySpan.endLine = semanticDiagnosticInfo.line;
        diagnosticInfo->primarySpan.endColumn = semanticDiagnosticInfo.column;
        diagnosticInfo->hasPrimarySpan = true;
      }
      diagnosticInfo->relatedSpans.clear();
      diagnosticInfo->relatedSpans.reserve(semanticDiagnosticInfo.relatedSpans.size());
      for (const auto &related : semanticDiagnosticInfo.relatedSpans) {
        if (related.line <= 0 || related.column <= 0) {
          continue;
        }
        DiagnosticRelatedSpan span;
        span.span.file = options.inputPath;
        span.span.line = related.line;
        span.span.column = related.column;
        span.span.endLine = related.line;
        span.span.endColumn = related.column;
        span.label = related.label;
        diagnosticInfo->relatedSpans.push_back(std::move(span));
      }
    }
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
