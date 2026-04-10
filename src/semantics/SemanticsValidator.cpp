#include "SemanticsValidator.h"
#include "SemanticsValidatorExprCaptureSplitStep.h"

#include "primec/Lexer.h"
#include "primec/Parser.h"

#include <algorithm>
#include <cctype>
#include <functional>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace primec::semantics {

namespace {

bool isSlashlessMapHelperName(std::string_view name) {
  if (!name.empty() && name.front() == '/') {
    name.remove_prefix(1);
  }
  return name.rfind("map/", 0) == 0 || name.rfind("std/collections/map/", 0) == 0;
}

} // namespace

SemanticsValidator::SemanticsValidator(const Program &program,
                                       const std::string &entryPath,
                                       std::string &error,
                                       const std::vector<std::string> &defaultEffects,
                                       const std::vector<std::string> &entryDefaultEffects,
                                       SemanticDiagnosticInfo *diagnosticInfo,
                                       bool collectDiagnostics,
                                       uint32_t benchmarkSemanticDefinitionValidationWorkerCount,
                                       bool benchmarkSemanticPhaseCountersEnabled,
                                       bool benchmarkSemanticDisableMethodTargetMemoization)
    : program_(program),
      entryPath_(entryPath),
      error_(error),
      defaultEffects_(defaultEffects),
      entryDefaultEffects_(entryDefaultEffects),
      diagnosticInfo_(diagnosticInfo),
      diagnosticSink_(diagnosticInfo),
      collectDiagnostics_(collectDiagnostics),
      benchmarkSemanticDefinitionValidationWorkerCount_(
          benchmarkSemanticDefinitionValidationWorkerCount),
      benchmarkSemanticPhaseCountersEnabled_(benchmarkSemanticPhaseCountersEnabled),
      methodTargetMemoizationEnabled_(!benchmarkSemanticDisableMethodTargetMemoization) {
  diagnosticSink_.reset();
  auto registerMathImport = [&](const std::string &importPath) {
    if (importPath == "/std/math/*") {
      mathImportAll_ = true;
      return;
    }
    if (importPath.rfind("/std/math/", 0) == 0 && importPath.size() > 10) {
      std::string name = importPath.substr(10);
      if (name.find('/') == std::string::npos && name != "*") {
        mathImports_.insert(std::move(name));
      }
    }
  };
  for (const auto &importPath : program_.sourceImports) {
    registerMathImport(importPath);
  }
  for (const auto &importPath : program_.imports) {
    registerMathImport(importPath);
  }
}

void SemanticsValidator::observeCallVisited() {
  if (!benchmarkSemanticPhaseCountersEnabled_) {
    return;
  }
  ++validationCounters_.callsVisited;
}

void SemanticsValidator::observeLocalMapSize(std::size_t size) {
  if (!benchmarkSemanticPhaseCountersEnabled_) {
    return;
  }
  validationCounters_.peakLocalMapSize =
      std::max<uint64_t>(validationCounters_.peakLocalMapSize,
                         static_cast<uint64_t>(size));
}

std::string SemanticsValidator::formatUnknownCallTarget(const Expr &expr) const {
  if (!isSlashlessMapHelperName(expr.name)) {
    return expr.name;
  }
  const std::string resolved = resolveCalleePath(expr);
  return resolved.empty() ? expr.name : resolved;
}

std::string SemanticsValidator::diagnosticCallTargetPath(const std::string &path) const {
  if (path.empty()) {
    return path;
  }
  if (path.rfind("/std/collections/map/count__t", 0) == 0) {
    return "/std/collections/map/count";
  }
  if (path.rfind("/map/count__t", 0) == 0) {
    return "/map/count";
  }
  const size_t lastSlash = path.find_last_of('/');
  const size_t nameStart = lastSlash == std::string::npos ? 0 : lastSlash + 1;
  const size_t suffix = path.find("__t", nameStart);
  if (suffix == std::string::npos) {
    return path;
  }
  const std::string basePath = path.substr(0, suffix);
  auto it = defMap_.find(basePath);
  if (it == defMap_.end() || it->second == nullptr || it->second->templateArgs.empty()) {
    return path;
  }
  return basePath;
}

bool SemanticsValidator::run() {
  auto runStage = [&](const char *stageName, auto &&fn) {
    try {
      const bool ok = fn();
      if (!ok && error_.empty()) {
        return failUncontextualizedDiagnostic(std::string(stageName) +
                                             " failed without diagnostic");
      }
      return ok;
    } catch (...) {
      return failUncontextualizedDiagnostic(
          std::string("semantic validator exception in ") + stageName);
    }
  };
  if (collectDiagnostics_ &&
      !runStage("collectDuplicateDefinitionDiagnostics",
                [&] { return collectDuplicateDefinitionDiagnostics(); })) {
    return false;
  }
  if (!runStage("buildDefinitionMaps", [&] { return buildDefinitionMaps(); })) {
    return false;
  }
  if (!runStage("inferUnknownReturnKinds",
                [&] { return inferUnknownReturnKinds(); })) {
    return false;
  }
  if (!runStage("validateTraitConstraints",
                [&] { return validateTraitConstraints(); })) {
    return false;
  }
  if (!runStage("validateStructLayouts",
                [&] { return validateStructLayouts(); })) {
    return false;
  }
  if (!runStage("validateDefinitions",
                [&] { return validateDefinitions(); })) {
    return false;
  }
  if (!runStage("validateExecutions",
                [&] { return validateExecutions(); })) {
    return false;
  }
  return runStage("validateEntry", [&] { return validateEntry(); });
}

bool SemanticsValidator::allowMathBareName(const std::string &name) const {
  (void)name;
  if (name.empty() || name.find('/') != std::string::npos) {
    return false;
  }
  if (!currentValidationState_.context.definitionPath.empty()) {
    if (currentValidationState_.context.definitionPath == "/std/math" ||
        currentValidationState_.context.definitionPath.rfind("/std/math/", 0) == 0) {
      return true;
    }
    if (currentValidationState_.context.definitionPath.rfind("/std/", 0) == 0) {
      for (const auto &importPath : program_.imports) {
        if (importPath == "/std/math/*") {
          return true;
        }
        if (importPath.rfind("/std/math/", 0) == 0 && importPath.size() > 10) {
          const std::string importedName = importPath.substr(10);
          if (!importedName.empty() && importedName.find('/') == std::string::npos) {
            return true;
          }
        }
      }
    }
  }
  return hasAnyMathImport();
}

bool SemanticsValidator::hasAnyMathImport() const {
  return mathImportAll_ || !mathImports_.empty();
}

bool SemanticsValidator::isEntryArgsName(const std::string &name) const {
  if (currentValidationState_.context.definitionPath != entryPath_) {
    return false;
  }
  if (entryArgsName_.empty()) {
    return false;
  }
  return name == entryArgsName_;
}

bool SemanticsValidator::isEntryArgsAccess(const Expr &expr) const {
  if (currentValidationState_.context.definitionPath != entryPath_ || entryArgsName_.empty()) {
    return false;
  }
  if (expr.kind != Expr::Kind::Call) {
    return false;
  }
  std::string accessName;
  if (!getBuiltinArrayAccessName(expr, accessName) || expr.args.size() != 2) {
    return false;
  }
  if (expr.args.front().kind != Expr::Kind::Name) {
    return false;
  }
  return expr.args.front().name == entryArgsName_;
}

bool SemanticsValidator::isEntryArgStringBinding(const std::unordered_map<std::string, BindingInfo> &locals,
                                                 const Expr &expr) const {
  if (currentValidationState_.context.definitionPath != entryPath_) {
    return false;
  }
  if (expr.kind != Expr::Kind::Name) {
    return false;
  }
  auto it = locals.find(expr.name);
  return it != locals.end() && it->second.isEntryArgString;
}

bool SemanticsValidator::parseTransformArgumentExpr(const std::string &text,
                                                    const std::string &namespacePrefix,
                                                    Expr &out) {
  auto failParseTransformArgumentExpr = [&](std::string message) -> bool {
    return failUncontextualizedDiagnostic(std::move(message));
  };
  Lexer lexer(text);
  Parser parser(lexer.tokenize());
  std::string parseError;
  if (!parser.parseExpression(out, namespacePrefix, parseError)) {
    return failParseTransformArgumentExpr(
        parseError.empty() ? "invalid transform argument expression" : parseError);
  }
  return true;
}

} // namespace primec::semantics
