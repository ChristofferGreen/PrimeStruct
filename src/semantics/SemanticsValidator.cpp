#include "SemanticsValidator.h"
#include "SemanticsValidatorExprCaptureSplitStep.h"

#include "primec/Lexer.h"
#include "primec/Parser.h"

#include <algorithm>
#include <cctype>
#include <functional>
#include <limits>
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
                                       bool collectDiagnostics)
    : program_(program),
      entryPath_(entryPath),
      error_(error),
      defaultEffects_(defaultEffects),
      entryDefaultEffects_(entryDefaultEffects),
      diagnosticInfo_(diagnosticInfo),
      diagnosticSink_(diagnosticInfo),
      collectDiagnostics_(collectDiagnostics) {
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

bool SemanticsValidator::makeDefinitionValidationContext(const Definition &def, ValidationContext &out) {
  out = {};
  out.definitionPath = def.fullPath;
  for (const auto &transform : def.transforms) {
    if (transform.name == "compute") {
      out.definitionIsCompute = true;
    } else if (transform.name == "unsafe") {
      out.definitionIsUnsafe = true;
    } else if (transform.name == "return" && transform.templateArgs.size() == 1) {
      ResultTypeInfo resultInfo;
      if (resolveResultTypeFromTypeName(transform.templateArgs.front(), resultInfo)) {
        out.resultType = std::move(resultInfo);
      }
    }
  }
  out.activeEffects = resolveEffects(def.transforms, def.fullPath == entryPath_);
  std::optional<OnErrorHandler> onErrorHandler;
  if (!parseOnErrorTransform(def.transforms, def.namespacePrefix, def.fullPath, onErrorHandler)) {
    return false;
  }
  out.onError = std::move(onErrorHandler);
  return true;
}

SemanticsValidator::ValidationContext
SemanticsValidator::makeExecutionValidationContext(const Execution &exec) const {
  ValidationContext context;
  context.definitionPath.clear();
  context.definitionIsCompute = false;
  context.definitionIsUnsafe = false;
  context.activeEffects = resolveEffects(exec.transforms, false);
  return context;
}

const SemanticsValidator::ValidationContext &
SemanticsValidator::buildDefinitionValidationContext(const Definition &def) const {
  auto it = definitionValidationContexts_.find(def.fullPath);
  if (it != definitionValidationContexts_.end()) {
    return it->second;
  }
  static const ValidationContext EmptyContext;
  return EmptyContext;
}

const SemanticsValidator::ValidationContext &
SemanticsValidator::buildExecutionValidationContext(const Execution &exec) const {
  auto it = executionValidationContexts_.find(exec.fullPath);
  if (it != executionValidationContexts_.end()) {
    return it->second;
  }
  static const ValidationContext EmptyContext;
  return EmptyContext;
}

void SemanticsValidator::capturePrimarySpanIfUnset(int line, int column) {
  diagnosticSink_.capturePrimarySpanIfUnset(line, column);
}

void SemanticsValidator::captureRelatedSpan(int line, int column, const std::string &label) {
  diagnosticSink_.addRelatedSpan(line, column, label);
}

void SemanticsValidator::captureExprContext(const Expr &expr) {
  capturePrimarySpanIfUnset(expr.sourceLine, expr.sourceColumn);
  if (currentDefinitionContext_ != nullptr) {
    captureRelatedSpan(currentDefinitionContext_->sourceLine,
                       currentDefinitionContext_->sourceColumn,
                       "definition: " + currentDefinitionContext_->fullPath);
  }
  if (currentExecutionContext_ != nullptr) {
    captureRelatedSpan(currentExecutionContext_->sourceLine,
                       currentExecutionContext_->sourceColumn,
                       "execution: " + currentExecutionContext_->fullPath);
  }
}

void SemanticsValidator::captureDefinitionContext(const Definition &def) {
  capturePrimarySpanIfUnset(def.sourceLine, def.sourceColumn);
  captureRelatedSpan(def.sourceLine, def.sourceColumn, "definition: " + def.fullPath);
}

void SemanticsValidator::captureExecutionContext(const Execution &exec) {
  capturePrimarySpanIfUnset(exec.sourceLine, exec.sourceColumn);
  captureRelatedSpan(exec.sourceLine, exec.sourceColumn, "execution: " + exec.fullPath);
}

bool SemanticsValidator::collectDuplicateDefinitionDiagnostics() {
  std::unordered_map<std::string, std::vector<const Definition *>> definitionsByPath;
  definitionsByPath.reserve(program_.definitions.size());
  for (const auto &def : program_.definitions) {
    definitionsByPath[def.fullPath].push_back(&def);
  }

  struct DuplicateGroup {
    std::string path;
    std::vector<const Definition *> definitions;
  };
  std::vector<DuplicateGroup> duplicateGroups;
  duplicateGroups.reserve(definitionsByPath.size());
  for (auto &entry : definitionsByPath) {
    if (entry.second.size() <= 1) {
      continue;
    }
    auto &defs = entry.second;
    std::stable_sort(defs.begin(), defs.end(), [](const Definition *left, const Definition *right) {
      const int leftLine = left->sourceLine > 0 ? left->sourceLine : std::numeric_limits<int>::max();
      const int rightLine = right->sourceLine > 0 ? right->sourceLine : std::numeric_limits<int>::max();
      if (leftLine != rightLine) {
        return leftLine < rightLine;
      }
      const int leftColumn = left->sourceColumn > 0 ? left->sourceColumn : std::numeric_limits<int>::max();
      const int rightColumn = right->sourceColumn > 0 ? right->sourceColumn : std::numeric_limits<int>::max();
      if (leftColumn != rightColumn) {
        return leftColumn < rightColumn;
      }
      return left->fullPath < right->fullPath;
    });
    duplicateGroups.push_back(DuplicateGroup{entry.first, defs});
  }
  if (duplicateGroups.empty()) {
    return true;
  }

  std::stable_sort(duplicateGroups.begin(), duplicateGroups.end(), [](const DuplicateGroup &left, const DuplicateGroup &right) {
    const Definition *leftDef = left.definitions.front();
    const Definition *rightDef = right.definitions.front();
    const int leftLine = leftDef->sourceLine > 0 ? leftDef->sourceLine : std::numeric_limits<int>::max();
    const int rightLine = rightDef->sourceLine > 0 ? rightDef->sourceLine : std::numeric_limits<int>::max();
    if (leftLine != rightLine) {
      return leftLine < rightLine;
    }
    const int leftColumn = leftDef->sourceColumn > 0 ? leftDef->sourceColumn : std::numeric_limits<int>::max();
    const int rightColumn = rightDef->sourceColumn > 0 ? rightDef->sourceColumn : std::numeric_limits<int>::max();
    if (leftColumn != rightColumn) {
      return leftColumn < rightColumn;
    }
    return left.path < right.path;
  });

  if (diagnosticInfo_ != nullptr) {
    std::vector<DiagnosticSinkRecord> records;
    records.reserve(duplicateGroups.size());
    for (const auto &group : duplicateGroups) {
      DiagnosticSinkRecord record;
      record.message = "duplicate definition: " + group.path;
      const Definition *primary = group.definitions.front();
      if (primary->sourceLine > 0 && primary->sourceColumn > 0) {
        record.primarySpan.line = primary->sourceLine;
        record.primarySpan.column = primary->sourceColumn;
        record.primarySpan.endLine = primary->sourceLine;
        record.primarySpan.endColumn = primary->sourceColumn;
        record.hasPrimarySpan = true;
      }
      for (const Definition *def : group.definitions) {
        if (def->sourceLine <= 0 || def->sourceColumn <= 0) {
          continue;
        }
        const std::string label = "definition: " + def->fullPath;
        bool duplicateSpan = false;
        for (const auto &existing : record.relatedSpans) {
          if (existing.span.line == def->sourceLine && existing.span.column == def->sourceColumn &&
              existing.label == label) {
            duplicateSpan = true;
            break;
          }
        }
        if (duplicateSpan) {
          continue;
        }
        DiagnosticRelatedSpan span;
        span.span.line = def->sourceLine;
        span.span.column = def->sourceColumn;
        span.span.endLine = def->sourceLine;
        span.span.endColumn = def->sourceColumn;
        span.label = label;
        record.relatedSpans.push_back(std::move(span));
      }
      records.push_back(std::move(record));
    }
    diagnosticSink_.setRecords(std::move(records));
  }

  error_ = "duplicate definition: " + duplicateGroups.front().path;
  return false;
}

bool SemanticsValidator::run() {
  auto runStage = [&](const char *stageName, auto &&fn) {
    try {
      const bool ok = fn();
      if (!ok && error_.empty()) {
        error_ = std::string(stageName) + " failed without diagnostic";
      }
      return ok;
    } catch (...) {
      error_ = std::string("semantic validator exception in ") + stageName;
      return false;
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
  if (!currentValidationContext_.definitionPath.empty()) {
    if (currentValidationContext_.definitionPath == "/std/math" ||
        currentValidationContext_.definitionPath.rfind("/std/math/", 0) == 0) {
      return true;
    }
    if (currentValidationContext_.definitionPath.rfind("/std/", 0) == 0) {
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
  if (currentValidationContext_.definitionPath != entryPath_) {
    return false;
  }
  if (entryArgsName_.empty()) {
    return false;
  }
  return name == entryArgsName_;
}

bool SemanticsValidator::isEntryArgsAccess(const Expr &expr) const {
  if (currentValidationContext_.definitionPath != entryPath_ || entryArgsName_.empty()) {
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
  if (currentValidationContext_.definitionPath != entryPath_) {
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
  Lexer lexer(text);
  Parser parser(lexer.tokenize());
  std::string parseError;
  if (!parser.parseExpression(out, namespacePrefix, parseError)) {
    error_ = parseError.empty() ? "invalid transform argument expression" : parseError;
    return false;
  }
  return true;
}

namespace {
std::string trimText(const std::string &text) {
  size_t start = 0;
  while (start < text.size() && std::isspace(static_cast<unsigned char>(text[start]))) {
    ++start;
  }
  size_t end = text.size();
  while (end > start && std::isspace(static_cast<unsigned char>(text[end - 1]))) {
    --end;
  }
  return text.substr(start, end - start);
}
} // namespace

bool SemanticsValidator::resolveResultTypeFromTemplateArg(const std::string &templateArg, ResultTypeInfo &out) const {
  out = ResultTypeInfo{};
  std::vector<std::string> parts;
  if (!splitTopLevelTemplateArgs(templateArg, parts)) {
    return false;
  }
  if (parts.size() == 1) {
    out.isResult = true;
    out.hasValue = false;
    out.errorType = trimText(parts.front());
    return true;
  }
  if (parts.size() == 2) {
    out.isResult = true;
    out.hasValue = true;
    out.valueType = trimText(parts[0]);
    out.errorType = trimText(parts[1]);
    return true;
  }
  return false;
}

bool SemanticsValidator::resolveResultTypeFromTypeName(const std::string &typeName, ResultTypeInfo &out) const {
  out = ResultTypeInfo{};
  std::string base;
  std::string arg;
  if (!splitTemplateTypeName(typeName, base, arg) || base != "Result") {
    return false;
  }
  return resolveResultTypeFromTemplateArg(arg, out);
}

bool SemanticsValidator::resolveResultTypeForExpr(const Expr &expr,
                                                  const std::vector<ParameterInfo> &params,
                                                  const std::unordered_map<std::string, BindingInfo> &locals,
                                                  ResultTypeInfo &out) {
  out = ResultTypeInfo{};
  auto bindingTypeText = [](const BindingInfo &binding) {
    if (binding.typeTemplateArg.empty()) {
      return binding.typeName;
    }
    return binding.typeName + "<" + binding.typeTemplateArg + ">";
  };
  auto resolveDefinitionResultType = [&](const Definition &definition, ResultTypeInfo &resultOut) -> bool {
    for (const auto &transform : definition.transforms) {
      if (transform.name != "return" || transform.templateArgs.size() != 1) {
        continue;
      }
      if (resolveResultTypeFromTypeName(transform.templateArgs.front(), resultOut)) {
        return true;
      }
    }
    BindingInfo inferredReturn;
    return inferDefinitionReturnBinding(definition, inferredReturn) &&
           resolveResultTypeFromTypeName(bindingTypeText(inferredReturn), resultOut);
  };
  auto resolveDirectResultOkType = [&](const Expr &candidate,
                                       const std::vector<ParameterInfo> &currentParams,
                                       const std::unordered_map<std::string, BindingInfo> &currentLocals,
                                       const std::string &preferredErrorType,
                                       ResultTypeInfo &resultOut) -> bool {
    resultOut = ResultTypeInfo{};
    if (candidate.kind != Expr::Kind::Call || !candidate.isMethodCall || candidate.name != "ok" ||
        !candidate.templateArgs.empty() || candidate.hasBodyArguments || !candidate.bodyArguments.empty() ||
        candidate.args.empty()) {
      return false;
    }
    const Expr &receiver = candidate.args.front();
    if (receiver.kind != Expr::Kind::Name || normalizeBindingTypeName(receiver.name) != "Result") {
      return false;
    }
    std::string errorType = preferredErrorType;
    if (errorType.empty() && currentValidationContext_.resultType.has_value() &&
        currentValidationContext_.resultType->isResult &&
        !currentValidationContext_.resultType->errorType.empty()) {
      errorType = currentValidationContext_.resultType->errorType;
    }
    if (errorType.empty() && currentValidationContext_.onError.has_value() &&
        !currentValidationContext_.onError->errorType.empty()) {
      errorType = currentValidationContext_.onError->errorType;
    }
    if (errorType.empty()) {
      errorType = "_";
    }
    resultOut.isResult = true;
    resultOut.errorType = std::move(errorType);
    if (candidate.args.size() == 1) {
      resultOut.hasValue = false;
      return true;
    }
    if (candidate.args.size() != 2) {
      return false;
    }
    BindingInfo payloadBinding;
    if (!inferBindingTypeFromInitializer(candidate.args.back(), currentParams, currentLocals, payloadBinding)) {
      return false;
    }
    resultOut.hasValue = true;
    resultOut.valueType = bindingTypeText(payloadBinding);
    return !resultOut.valueType.empty();
  };
  auto resolveBindingTypeText = [&](const std::string &name, std::string &typeTextOut) -> bool {
    typeTextOut.clear();
    auto describeBindingType = [](const BindingInfo &binding) {
      if (binding.typeTemplateArg.empty()) {
        return binding.typeName;
      }
      return binding.typeName + "<" + binding.typeTemplateArg + ">";
    };
    if (const BindingInfo *paramBinding = findParamBinding(params, name)) {
      typeTextOut = describeBindingType(*paramBinding);
      return true;
    }
    auto it = locals.find(name);
    if (it != locals.end()) {
      typeTextOut = describeBindingType(it->second);
      return true;
    }
    return false;
  };
  auto resolveBuiltinMapResultType = [&](const std::string &typeText) -> bool {
    const std::string normalizedTypeText = normalizeBindingTypeName(unwrapReferencePointerTypeText(typeText));
    std::string base;
    std::string argText;
    if (splitTemplateTypeName(normalizedTypeText, base, argText) &&
        normalizeBindingTypeName(base) == "map") {
      std::vector<std::string> args;
      if (!splitTopLevelTemplateArgs(argText, args) || args.size() != 2) {
        return false;
      }
      out.isResult = true;
      out.hasValue = true;
      out.valueType = args[1];
      out.errorType = "ContainerError";
      return true;
    }
    std::string resolvedPath = normalizedTypeText;
    if (!resolvedPath.empty() && resolvedPath.front() != '/') {
      resolvedPath.insert(resolvedPath.begin(), '/');
    }
    if (resolvedPath.rfind("/std/collections/experimental_map/Map__", 0) != 0) {
      return false;
    }
    auto defIt = defMap_.find(resolvedPath);
    if (defIt == defMap_.end() || defIt->second == nullptr) {
      return false;
    }
    std::string valueType;
    for (const auto &fieldExpr : defIt->second->statements) {
      if (!fieldExpr.isBinding || fieldExpr.name != "payloads") {
        continue;
      }
      BindingInfo fieldBinding;
      std::optional<std::string> restrictType;
      std::string parseError;
      if (!parseBindingInfo(fieldExpr,
                            defIt->second->namespacePrefix,
                            structNames_,
                            importAliases_,
                            fieldBinding,
                            restrictType,
                            parseError)) {
        continue;
      }
      if (normalizeBindingTypeName(fieldBinding.typeName) != "vector" ||
          fieldBinding.typeTemplateArg.empty()) {
        continue;
      }
      std::vector<std::string> args;
      if (!splitTopLevelTemplateArgs(fieldBinding.typeTemplateArg, args) || args.size() != 1) {
        continue;
      }
      valueType = args.front();
      break;
    }
    if (valueType.empty()) {
      return false;
    }
    out.isResult = true;
    out.hasValue = true;
    out.valueType = valueType;
    out.errorType = "ContainerError";
    return true;
  };
  std::function<bool(const Expr &, std::string &)> resolveMapReceiverTypeText;
  resolveMapReceiverTypeText = [&](const Expr &receiverExpr, std::string &typeTextOut) -> bool {
    typeTextOut.clear();
    std::string accessName;
    if (receiverExpr.kind == Expr::Kind::Call && getBuiltinArrayAccessName(receiverExpr, accessName) &&
        receiverExpr.args.size() == 2 && !receiverExpr.args.empty()) {
      const Expr &accessReceiver = receiverExpr.args.front();
      if (accessReceiver.kind != Expr::Kind::Name) {
        return false;
      }
      const BindingInfo *binding = findParamBinding(params, accessReceiver.name);
      if (!binding) {
        auto it = locals.find(accessReceiver.name);
        if (it != locals.end()) {
          binding = &it->second;
        }
      }
      if (binding == nullptr) {
        return false;
      }
      if (getArgsPackElementType(*binding, typeTextOut)) {
        return true;
      }
    }
    return inferQueryExprTypeText(receiverExpr, params, locals, typeTextOut);
  };
  auto seedLambdaLocals = [&](const Expr &lambdaExpr,
                              std::unordered_map<std::string, BindingInfo> &lambdaLocals) -> bool {
    lambdaLocals.clear();
    bool captureAll = false;
    std::vector<std::string> captureNames;
    captureNames.reserve(lambdaExpr.lambdaCaptures.size());
    for (const auto &capture : lambdaExpr.lambdaCaptures) {
      const std::vector<std::string> tokens = runSemanticsValidatorExprCaptureSplitStep(capture);
      if (tokens.empty()) {
        return false;
      }
      if (tokens.size() == 1) {
        if (tokens[0] == "=" || tokens[0] == "&") {
          captureAll = true;
          continue;
        }
        captureNames.push_back(tokens[0]);
        continue;
      }
      captureNames.push_back(tokens.back());
    }
    auto addCapturedBinding = [&](const std::string &name) {
      if (const BindingInfo *paramBinding = findParamBinding(params, name)) {
        lambdaLocals.emplace(name, *paramBinding);
        return true;
      }
      auto localIt = locals.find(name);
      if (localIt != locals.end()) {
        lambdaLocals.emplace(name, localIt->second);
        return true;
      }
      return false;
    };
    if (captureAll) {
      for (const auto &param : params) {
        lambdaLocals.emplace(param.name, param.binding);
      }
      for (const auto &entry : locals) {
        lambdaLocals.emplace(entry.first, entry.second);
      }
    } else {
      for (const auto &name : captureNames) {
        (void)addCapturedBinding(name);
      }
    }
    for (const auto &param : lambdaExpr.args) {
      if (!param.isBinding) {
        return false;
      }
      BindingInfo binding;
      std::optional<std::string> restrictType;
      if (!parseBindingInfo(
              param, lambdaExpr.namespacePrefix, structNames_, importAliases_, binding, restrictType, error_)) {
        return false;
      }
      if (!hasExplicitBindingTypeTransform(param) && param.args.size() == 1) {
        (void)tryInferBindingTypeFromInitializer(param.args.front(), {}, {}, binding, hasAnyMathImport());
      }
      lambdaLocals[param.name] = std::move(binding);
    }
    return true;
  };
  auto inferLambdaBodyTypeText = [&](const Expr &lambdaExpr, std::string &typeTextOut) -> bool {
    typeTextOut.clear();
    if (!lambdaExpr.isLambda || (!lambdaExpr.hasBodyArguments && lambdaExpr.bodyArguments.empty())) {
      return false;
    }
    std::unordered_map<std::string, BindingInfo> lambdaLocals;
    if (!seedLambdaLocals(lambdaExpr, lambdaLocals)) {
      return false;
    }
    const Expr *valueExpr = nullptr;
    for (const auto &bodyExpr : lambdaExpr.bodyArguments) {
      if (bodyExpr.isBinding) {
        BindingInfo binding;
        std::optional<std::string> restrictType;
        if (!parseBindingInfo(
                bodyExpr, bodyExpr.namespacePrefix, structNames_, importAliases_, binding, restrictType, error_)) {
          return false;
        }
        if (!hasExplicitBindingTypeTransform(bodyExpr) && bodyExpr.args.size() == 1) {
          (void)inferBindingTypeFromInitializer(bodyExpr.args.front(), {}, lambdaLocals, binding, &bodyExpr);
        }
        if (restrictType.has_value()) {
          const bool hasTemplate = !binding.typeTemplateArg.empty();
          if (!restrictMatchesBinding(
                  *restrictType, binding.typeName, binding.typeTemplateArg, hasTemplate, bodyExpr.namespacePrefix)) {
            return false;
          }
        }
        lambdaLocals[bodyExpr.name] = std::move(binding);
        continue;
      }
      if (isReturnCall(bodyExpr)) {
        if (bodyExpr.args.size() != 1) {
          return false;
        }
        valueExpr = &bodyExpr.args.front();
        break;
      }
      valueExpr = &bodyExpr;
    }
    if (valueExpr == nullptr) {
      return false;
    }
    BindingInfo inferredBinding;
    if (inferBindingTypeFromInitializer(*valueExpr, {}, lambdaLocals, inferredBinding)) {
      typeTextOut = bindingTypeText(inferredBinding);
      if (!typeTextOut.empty()) {
        return true;
      }
    }
    return inferQueryExprTypeText(*valueExpr, {}, lambdaLocals, typeTextOut) && !typeTextOut.empty();
  };
  auto inferLambdaBodyResultType = [&](const Expr &lambdaExpr,
                                       const std::string &preferredErrorType,
                                       ResultTypeInfo &resultOut) -> bool {
    resultOut = ResultTypeInfo{};
    if (!lambdaExpr.isLambda || (!lambdaExpr.hasBodyArguments && lambdaExpr.bodyArguments.empty())) {
      return false;
    }
    std::unordered_map<std::string, BindingInfo> lambdaLocals;
    if (!seedLambdaLocals(lambdaExpr, lambdaLocals)) {
      return false;
    }
    const Expr *valueExpr = nullptr;
    for (const auto &bodyExpr : lambdaExpr.bodyArguments) {
      if (bodyExpr.isBinding) {
        BindingInfo binding;
        std::optional<std::string> restrictType;
        if (!parseBindingInfo(
                bodyExpr, bodyExpr.namespacePrefix, structNames_, importAliases_, binding, restrictType, error_)) {
          return false;
        }
        if (!hasExplicitBindingTypeTransform(bodyExpr) && bodyExpr.args.size() == 1) {
          (void)inferBindingTypeFromInitializer(bodyExpr.args.front(), {}, lambdaLocals, binding, &bodyExpr);
        }
        if (restrictType.has_value()) {
          const bool hasTemplate = !binding.typeTemplateArg.empty();
          if (!restrictMatchesBinding(
                  *restrictType, binding.typeName, binding.typeTemplateArg, hasTemplate, bodyExpr.namespacePrefix)) {
            return false;
          }
        }
        lambdaLocals[bodyExpr.name] = std::move(binding);
        continue;
      }
      if (isReturnCall(bodyExpr)) {
        if (bodyExpr.args.size() != 1) {
          return false;
        }
        valueExpr = &bodyExpr.args.front();
        break;
      }
      valueExpr = &bodyExpr;
    }
    if (valueExpr == nullptr) {
      return false;
    }
    if (resolveDirectResultOkType(*valueExpr, {}, lambdaLocals, preferredErrorType, resultOut)) {
      return true;
    }
    if (!resolveResultTypeForExpr(*valueExpr, {}, lambdaLocals, resultOut) || !resultOut.isResult) {
      return false;
    }
    if (!preferredErrorType.empty() && resultOut.errorType == "_") {
      resultOut.errorType = preferredErrorType;
    }
    return true;
  };
  auto hasDefinitionFamilyPath = [&](std::string_view path) {
    if (defMap_.count(std::string(path)) > 0) {
      return true;
    }
    const std::string templatedPrefix = std::string(path) + "<";
    const std::string specializedPrefix = std::string(path) + "__t";
    for (const auto &def : program_.definitions) {
      if (def.fullPath == path || def.fullPath.rfind(templatedPrefix, 0) == 0 ||
          def.fullPath.rfind(specializedPrefix, 0) == 0) {
        return true;
      }
    }
    return false;
  };
  auto preferredFileErrorHelperTarget = [&](std::string_view helperName) -> std::string {
    if (helperName == "why") {
      if (hasDefinitionFamilyPath("/std/file/FileError/why")) {
        return "/std/file/FileError/why";
      }
      if (hasDefinitionFamilyPath("/FileError/why")) {
        return "/FileError/why";
      }
      return "/file_error/why";
    }
    if (helperName == "is_eof") {
      if (hasDefinitionFamilyPath("/std/file/FileError/is_eof")) {
        return "/std/file/FileError/is_eof";
      }
      if (hasDefinitionFamilyPath("/FileError/is_eof")) {
        return "/FileError/is_eof";
      }
      if (hasDefinitionFamilyPath("/std/file/fileErrorIsEof")) {
        return "/std/file/fileErrorIsEof";
      }
      return "";
    }
    if (helperName == "eof") {
      if (hasDefinitionFamilyPath("/std/file/FileError/eof")) {
        return "/std/file/FileError/eof";
      }
      if (hasDefinitionFamilyPath("/FileError/eof")) {
        return "/FileError/eof";
      }
      if (hasDefinitionFamilyPath("/std/file/fileReadEof")) {
        return "/std/file/fileReadEof";
      }
      return "";
    }
    if (helperName == "status") {
      if (hasDefinitionFamilyPath("/std/file/FileError/status")) {
        return "/std/file/FileError/status";
      }
      return "";
    }
    if (helperName == "result") {
      if (hasDefinitionFamilyPath("/std/file/FileError/result")) {
        return "/std/file/FileError/result";
      }
      return "";
    }
    return "";
  };
  auto preferredImageErrorHelperTarget = [&](std::string_view helperName) -> std::string {
    if (helperName == "why") {
      if (defMap_.count("/std/image/ImageError/why") > 0) {
        return "/std/image/ImageError/why";
      }
      if (defMap_.count("/ImageError/why") > 0) {
        return "/ImageError/why";
      }
      return "";
    }
    if (helperName == "status") {
      if (defMap_.count("/std/image/ImageError/status") > 0) {
        return "/std/image/ImageError/status";
      }
      if (defMap_.count("/ImageError/status") > 0) {
        return "/ImageError/status";
      }
      if (defMap_.count("/std/image/imageErrorStatus") > 0) {
        return "/std/image/imageErrorStatus";
      }
      return "";
    }
    if (helperName == "result") {
      if (defMap_.count("/std/image/ImageError/result") > 0) {
        return "/std/image/ImageError/result";
      }
      if (defMap_.count("/ImageError/result") > 0) {
        return "/ImageError/result";
      }
      if (defMap_.count("/std/image/imageErrorResult") > 0) {
        return "/std/image/imageErrorResult";
      }
      return "";
    }
    return "";
  };
  auto preferredContainerErrorHelperTarget = [&](std::string_view helperName) -> std::string {
    if (helperName == "why") {
      if (defMap_.count("/std/collections/ContainerError/why") > 0) {
        return "/std/collections/ContainerError/why";
      }
      if (defMap_.count("/ContainerError/why") > 0) {
        return "/ContainerError/why";
      }
      return "";
    }
    if (helperName == "status") {
      if (defMap_.count("/std/collections/ContainerError/status") > 0) {
        return "/std/collections/ContainerError/status";
      }
      if (defMap_.count("/ContainerError/status") > 0) {
        return "/ContainerError/status";
      }
      if (defMap_.count("/std/collections/containerErrorStatus") > 0) {
        return "/std/collections/containerErrorStatus";
      }
      return "";
    }
    if (helperName == "result") {
      if (defMap_.count("/std/collections/ContainerError/result") > 0) {
        return "/std/collections/ContainerError/result";
      }
      if (defMap_.count("/ContainerError/result") > 0) {
        return "/ContainerError/result";
      }
      if (defMap_.count("/std/collections/containerErrorResult") > 0) {
        return "/std/collections/containerErrorResult";
      }
      return "";
    }
    return "";
  };
  auto preferredGfxErrorHelperTarget = [&](std::string_view helperName,
                                           const std::string &preferredNamespacePath) -> std::string {
    const bool experimental = preferredNamespacePath.find("/std/gfx/experimental/") == 0;
    if (helperName == "why") {
      if (experimental) {
        if (defMap_.count("/std/gfx/experimental/GfxError/why") > 0) {
          return "/std/gfx/experimental/GfxError/why";
        }
      } else if (defMap_.count("/std/gfx/GfxError/why") > 0) {
        return "/std/gfx/GfxError/why";
      }
      if (defMap_.count("/GfxError/why") > 0) {
        return "/GfxError/why";
      }
      return "";
    }
    if (helperName == "status") {
      if (experimental) {
        if (defMap_.count("/std/gfx/experimental/GfxError/status") > 0) {
          return "/std/gfx/experimental/GfxError/status";
        }
      } else if (defMap_.count("/std/gfx/GfxError/status") > 0) {
        return "/std/gfx/GfxError/status";
      }
      if (defMap_.count("/GfxError/status") > 0) {
        return "/GfxError/status";
      }
      return "";
    }
    if (helperName == "result") {
      if (experimental) {
        if (defMap_.count("/std/gfx/experimental/GfxError/result") > 0) {
          return "/std/gfx/experimental/GfxError/result";
        }
      } else if (defMap_.count("/std/gfx/GfxError/result") > 0) {
        return "/std/gfx/GfxError/result";
      }
      if (defMap_.count("/GfxError/result") > 0) {
        return "/GfxError/result";
      }
      return "";
    }
    return "";
  };
  auto resolveMethodResultPath = [&]() -> std::string {
    if (!expr.isMethodCall || expr.args.empty()) {
      return "";
    }
    const BuiltinCollectionDispatchResolvers dispatchResolvers =
        makeBuiltinCollectionDispatchResolvers(params, locals);
    std::string resolvedPath;
    if (resolveLeadingNonCollectionAccessReceiverPath(
            params, locals, expr.args.front(), expr.name, dispatchResolvers, resolvedPath)) {
      return resolvedPath;
    }
    const Expr &receiver = expr.args.front();
    const std::string receiverTypeName = [&]() -> std::string {
      if (receiver.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, receiver.name)) {
          return paramBinding->typeName;
        }
        auto localIt = locals.find(receiver.name);
        if (localIt != locals.end()) {
          return localIt->second.typeName;
        }
        if (isPrimitiveBindingTypeName(receiver.name)) {
          return receiver.name;
        }
        const std::string rootReceiverPath = "/" + receiver.name;
        if (defMap_.find(rootReceiverPath) != defMap_.end() || structNames_.count(rootReceiverPath) > 0) {
          return rootReceiverPath;
        }
        auto importIt = importAliases_.find(receiver.name);
        if (importIt != importAliases_.end()) {
          return importIt->second;
        }
        const std::string resolvedType = resolveStructTypePath(receiver.name, receiver.namespacePrefix, structNames_);
        if (!resolvedType.empty()) {
          return resolvedType;
        }
      }
      if (receiver.kind == Expr::Kind::Call && !receiver.isMethodCall) {
        const std::string resolvedReceiverPath = resolveCalleePath(receiver);
        if (!resolvedReceiverPath.empty()) {
          return resolvedReceiverPath;
        }
      }
      return std::string();
    }();
    if (receiver.kind == Expr::Kind::Name && receiver.name == "FileError" &&
        (expr.name == "why" || expr.name == "is_eof" || expr.name == "eof" || expr.name == "status" ||
         expr.name == "result")) {
      return preferredFileErrorHelperTarget(expr.name);
    }
    if (receiverTypeName == "FileError" &&
        (expr.name == "why" || expr.name == "is_eof" || expr.name == "status" || expr.name == "result")) {
      return preferredFileErrorHelperTarget(expr.name);
    }
    if (receiverTypeName == "ImageError" &&
        (expr.name == "why" || expr.name == "status" || expr.name == "result")) {
      return preferredImageErrorHelperTarget(expr.name);
    }
    if (receiverTypeName == "ContainerError" &&
        (expr.name == "why" || expr.name == "status" || expr.name == "result")) {
      return preferredContainerErrorHelperTarget(expr.name);
    }
    if (receiverTypeName == "GfxError" &&
        (expr.name == "why" || expr.name == "status" || expr.name == "result")) {
      return preferredGfxErrorHelperTarget(
          expr.name,
          resolveStructTypePath(receiverTypeName, receiver.namespacePrefix, structNames_));
    }
    if (receiverTypeName.empty() || receiverTypeName == "File") {
      return "";
    }
    if (isPrimitiveBindingTypeName(receiverTypeName)) {
      return "/" + normalizeBindingTypeName(receiverTypeName) + "/" + expr.name;
    }
    std::string resolvedType;
    if (!receiverTypeName.empty() && receiverTypeName.front() == '/') {
      resolvedType = receiverTypeName;
    } else {
      resolvedType = resolveStructTypePath(receiverTypeName, receiver.namespacePrefix, structNames_);
      if (resolvedType.empty()) {
        auto importIt = importAliases_.find(receiverTypeName);
        if (importIt != importAliases_.end()) {
          resolvedType = importIt->second;
        }
      }
      if (resolvedType.empty()) {
        resolvedType = resolveTypePath(receiverTypeName, receiver.namespacePrefix);
      }
    }
    if (resolvedType.empty()) {
      return "";
    }
    return resolvedType + "/" + expr.name;
    return "";
  };
  if (expr.kind == Expr::Kind::Name) {
    if (const BindingInfo *paramBinding = findParamBinding(params, expr.name)) {
      if (paramBinding->typeName == "Result") {
        return resolveResultTypeFromTemplateArg(paramBinding->typeTemplateArg, out);
      }
    }
    auto it = locals.find(expr.name);
    if (it != locals.end() && it->second.typeName == "Result") {
      return resolveResultTypeFromTemplateArg(it->second.typeTemplateArg, out);
    }
    return false;
  }
  if (expr.kind != Expr::Kind::Call) {
    return false;
  }
  if (resolveDirectResultOkType(expr, params, locals, "", out)) {
    return true;
  }
  if (!expr.isMethodCall && isSimpleCallName(expr, "File") && expr.templateArgs.size() == 1) {
    out.isResult = true;
    out.hasValue = true;
    out.valueType = "File<" + expr.templateArgs.front() + ">";
    out.errorType = "FileError";
    return true;
  }
  if (isSimpleCallName(expr, "dereference") && expr.args.size() == 1) {
    std::string pointeeType;
    const Expr &target = expr.args.front();
    if (target.kind == Expr::Kind::Name) {
      if (resolveBindingTypeText(target.name, pointeeType)) {
        return resolveResultTypeFromTypeName(unwrapReferencePointerTypeText(pointeeType), out);
      }
    }
    std::string accessName;
    if (getBuiltinArrayAccessName(target, accessName) && target.args.size() == 2 && !target.args.empty()) {
      const Expr &receiver = target.args.front();
      if (receiver.kind == Expr::Kind::Name && resolveBindingTypeText(receiver.name, pointeeType)) {
        std::string elemType;
        if (resolveResultTypeFromTypeName(unwrapReferencePointerTypeText(pointeeType), out)) {
          return true;
        }
        if (const BindingInfo *binding = findParamBinding(params, receiver.name)) {
          if (getArgsPackElementType(*binding, elemType)) {
            return resolveResultTypeFromTypeName(unwrapReferencePointerTypeText(elemType), out);
          }
        } else if (auto it = locals.find(receiver.name); it != locals.end()) {
          if (getArgsPackElementType(it->second, elemType)) {
            return resolveResultTypeFromTypeName(unwrapReferencePointerTypeText(elemType), out);
          }
        }
      }
    }
  }
  std::string accessName;
  if (getBuiltinArrayAccessName(expr, accessName) && expr.args.size() == 2 && !expr.args.empty()) {
    const Expr &receiver = expr.args.front();
    if (receiver.kind == Expr::Kind::Name) {
      const BindingInfo *binding = findParamBinding(params, receiver.name);
      if (!binding) {
        auto it = locals.find(receiver.name);
        if (it != locals.end()) {
          binding = &it->second;
        }
      }
      std::string elemType;
      if (binding != nullptr && getArgsPackElementType(*binding, elemType)) {
        return resolveResultTypeFromTypeName(unwrapReferencePointerTypeText(elemType), out);
      }
    }
  }
  if (expr.isMethodCall) {
    if (!expr.args.empty() &&
        expr.args.front().kind == Expr::Kind::Name &&
        normalizeBindingTypeName(expr.args.front().name) == "Result") {
      if ((expr.name == "map" || expr.name == "and_then") && expr.args.size() == 3) {
        ResultTypeInfo argResult;
        if (!resolveResultTypeForExpr(expr.args[1], params, locals, argResult) || !argResult.isResult ||
            !argResult.hasValue) {
          return false;
        }
        if (expr.name == "map") {
          std::string mappedType;
          if (!inferLambdaBodyTypeText(expr.args[2], mappedType) || mappedType.empty()) {
            return false;
          }
          out.isResult = true;
          out.hasValue = true;
          out.valueType = std::move(mappedType);
          out.errorType = argResult.errorType;
          return true;
        }
        ResultTypeInfo chainedResult;
        if (!inferLambdaBodyResultType(expr.args[2], argResult.errorType, chainedResult) || !chainedResult.isResult) {
          return false;
        }
        if (!chainedResult.errorType.empty() && !argResult.errorType.empty() &&
            !errorTypesMatch(chainedResult.errorType, argResult.errorType, expr.namespacePrefix)) {
          return false;
        }
        out = std::move(chainedResult);
        return true;
      }
      if (expr.name == "map2" && expr.args.size() == 4) {
        ResultTypeInfo leftResult;
        ResultTypeInfo rightResult;
        if (!resolveResultTypeForExpr(expr.args[1], params, locals, leftResult) || !leftResult.isResult ||
            !leftResult.hasValue ||
            !resolveResultTypeForExpr(expr.args[2], params, locals, rightResult) || !rightResult.isResult ||
            !rightResult.hasValue) {
          return false;
        }
        if (!errorTypesMatch(leftResult.errorType, rightResult.errorType, expr.namespacePrefix)) {
          return false;
        }
        std::string mappedType;
        if (!inferLambdaBodyTypeText(expr.args[3], mappedType) || mappedType.empty()) {
          return false;
        }
        out.isResult = true;
        out.hasValue = true;
        out.valueType = std::move(mappedType);
        out.errorType = leftResult.errorType;
        return true;
      }
    }
    if (expr.name == "write" || expr.name == "write_line" || expr.name == "write_byte" || expr.name == "read_byte" ||
        expr.name == "write_bytes" || expr.name == "flush" || expr.name == "close") {
      out.isResult = true;
      out.hasValue = false;
      out.errorType = "FileError";
      return true;
    }
    const std::string resolvedMethodPath = resolveMethodResultPath();
    if (!resolvedMethodPath.empty()) {
      auto it = defMap_.find(resolvedMethodPath);
      if (it != defMap_.end() && it->second != nullptr) {
        return resolveDefinitionResultType(*it->second, out);
      }
    }
    if (expr.name == "tryAt" && !expr.args.empty()) {
      std::string receiverTypeText;
      if (resolveMapReceiverTypeText(expr.args.front(), receiverTypeText) &&
          resolveBuiltinMapResultType(receiverTypeText)) {
        return true;
      }
    }
  }
  if (!expr.isMethodCall) {
    const std::string resolved = resolveCalleePath(expr);
    auto it = defMap_.find(resolved);
    if (it != defMap_.end() && it->second != nullptr) {
      return resolveDefinitionResultType(*it->second, out);
    }
    if ((resolved == "/std/collections/map/tryAt" || resolved == "/map/tryAt" || isSimpleCallName(expr, "tryAt")) &&
        !expr.args.empty()) {
      const Expr *receiverExpr = &expr.args.front();
      if (hasNamedArguments(expr.argNames)) {
        for (size_t i = 0; i < expr.args.size(); ++i) {
          if (i < expr.argNames.size() && expr.argNames[i].has_value() &&
              *expr.argNames[i] == "values") {
            receiverExpr = &expr.args[i];
            break;
          }
        }
      }
      std::string receiverTypeText;
      if (resolveMapReceiverTypeText(*receiverExpr, receiverTypeText) &&
          resolveBuiltinMapResultType(receiverTypeText)) {
        return true;
      }
    }
  }
  return false;
}

bool SemanticsValidator::errorTypesMatch(const std::string &left,
                                         const std::string &right,
                                         const std::string &namespacePrefix) const {
  const auto stripInnerWhitespace = [](const std::string &text) {
    std::string out;
    out.reserve(text.size());
    for (unsigned char ch : text) {
      if (!std::isspace(ch)) {
        out.push_back(static_cast<char>(ch));
      }
    }
    return out;
  };
  const auto normalizeType = [&](const std::string &name) -> std::string {
    const std::string trimmed = trimText(name);
    const std::string normalized = normalizeBindingTypeName(trimmed);
    std::string base;
    std::string arg;
    if (splitTemplateTypeName(normalized, base, arg) &&
        (isBuiltinTemplateTypeName(base) || base == "array" || base == "vector" || base == "soa_vector" ||
         base == "map" || base == "Result" || base == "File")) {
      return stripInnerWhitespace(normalized);
    }
    if (normalized != trimmed || isPrimitiveBindingTypeName(trimmed)) {
      return stripInnerWhitespace(normalized);
    }
    if (!trimmed.empty() && trimmed[0] == '/') {
      return stripInnerWhitespace(trimmed);
    }
    if (auto aliasIt = importAliases_.find(trimmed); aliasIt != importAliases_.end()) {
      return stripInnerWhitespace(aliasIt->second);
    }
    return stripInnerWhitespace(resolveTypePath(trimmed, namespacePrefix));
  };
  return normalizeType(left) == normalizeType(right);
}

bool SemanticsValidator::parseOnErrorTransform(const std::vector<Transform> &transforms,
                                               const std::string &namespacePrefix,
                                               const std::string &context,
                                               std::optional<OnErrorHandler> &out) {
  out.reset();
  for (const auto &transform : transforms) {
    if (transform.name != "on_error") {
      continue;
    }
    if (out.has_value()) {
      error_ = "duplicate on_error transform on " + context;
      return false;
    }
    if (transform.templateArgs.size() != 2) {
      error_ = "on_error requires exactly two template arguments on " + context;
      return false;
    }
    OnErrorHandler handler;
    handler.errorType = transform.templateArgs.front();
    Expr handlerExpr;
    handlerExpr.kind = Expr::Kind::Name;
    handlerExpr.name = transform.templateArgs[1];
    handlerExpr.namespacePrefix = namespacePrefix;
    handler.handlerPath = resolveCalleePath(handlerExpr);
    if (defMap_.count(handler.handlerPath) == 0) {
      error_ = "unknown on_error handler: " + handler.handlerPath;
      return false;
    }
    auto paramIt = paramsByDef_.find(handler.handlerPath);
    if (paramIt == paramsByDef_.end() || paramIt->second.empty()) {
      error_ = "on_error handler requires an error parameter: " + handler.handlerPath;
      return false;
    }
    const BindingInfo &paramBinding = paramIt->second.front().binding;
    if (!errorTypesMatch(handler.errorType, paramBinding.typeName, namespacePrefix)) {
      error_ = "on_error handler error type mismatch: " + handler.handlerPath;
      return false;
    }
    handler.boundArgs.reserve(transform.arguments.size());
    for (const auto &argText : transform.arguments) {
      Expr argExpr;
      if (!parseTransformArgumentExpr(argText, namespacePrefix, argExpr)) {
        if (error_.empty()) {
          error_ = "invalid on_error argument on " + context;
        }
        return false;
      }
      handler.boundArgs.push_back(std::move(argExpr));
    }
    out = std::move(handler);
  }
  return true;
}

bool SemanticsValidator::exprUsesName(const Expr &expr, const std::string &name) const {
  if (name.empty()) {
    return false;
  }
  if (expr.kind == Expr::Kind::Name && expr.name == name) {
    return true;
  }
  if (expr.isLambda) {
    for (const auto &capture : expr.lambdaCaptures) {
      std::string token;
      std::vector<std::string> tokens;
      for (char c : capture) {
        if (std::isspace(static_cast<unsigned char>(c)) != 0) {
          if (!token.empty()) {
            tokens.push_back(token);
            token.clear();
          }
          continue;
        }
        token.push_back(c);
      }
      if (!token.empty()) {
        tokens.push_back(token);
      }
      if (tokens.size() == 1 && tokens.front() == name) {
        return true;
      }
      if (tokens.size() == 2 && tokens.back() == name) {
        return true;
      }
    }
    for (const auto &param : expr.args) {
      for (const auto &defaultArg : param.args) {
        if (exprUsesName(defaultArg, name)) {
          return true;
        }
      }
    }
    for (const auto &bodyExpr : expr.bodyArguments) {
      if (exprUsesName(bodyExpr, name)) {
        return true;
      }
    }
    return false;
  }
  for (const auto &arg : expr.args) {
    if (exprUsesName(arg, name)) {
      return true;
    }
  }
  for (const auto &bodyExpr : expr.bodyArguments) {
    if (exprUsesName(bodyExpr, name)) {
      return true;
    }
  }
  return false;
}

bool SemanticsValidator::statementsUseNameFrom(const std::vector<Expr> &statements,
                                               size_t startIndex,
                                               const std::string &name) const {
  for (size_t index = startIndex; index < statements.size(); ++index) {
    if (exprUsesName(statements[index], name)) {
      return true;
    }
  }
  return false;
}

void SemanticsValidator::expireReferenceBorrowsForRemainder(const std::vector<ParameterInfo> &params,
                                                            const std::unordered_map<std::string, BindingInfo> &locals,
                                                            const std::vector<Expr> &statements,
                                                            size_t nextIndex) {
  std::function<bool(const Expr &, const std::string &, bool)> exprUsesNameOutsideWriteTarget;
  exprUsesNameOutsideWriteTarget = [&](const Expr &expr, const std::string &name, bool inWriteTarget) -> bool {
    if (name.empty()) {
      return false;
    }
    if (expr.kind == Expr::Kind::Name && expr.name == name) {
      return !inWriteTarget;
    }
    if (expr.isLambda) {
      if (!inWriteTarget) {
        for (const auto &capture : expr.lambdaCaptures) {
          std::string token;
          std::vector<std::string> tokens;
          for (char c : capture) {
            if (std::isspace(static_cast<unsigned char>(c)) != 0) {
              if (!token.empty()) {
                tokens.push_back(token);
                token.clear();
              }
              continue;
            }
            token.push_back(c);
          }
          if (!token.empty()) {
            tokens.push_back(token);
          }
          if ((tokens.size() == 1 && tokens.front() == name) || (tokens.size() == 2 && tokens.back() == name)) {
            return true;
          }
        }
      }
      for (const auto &param : expr.args) {
        for (const auto &defaultArg : param.args) {
          if (exprUsesNameOutsideWriteTarget(defaultArg, name, false)) {
            return true;
          }
        }
      }
      for (const auto &bodyExpr : expr.bodyArguments) {
        if (exprUsesNameOutsideWriteTarget(bodyExpr, name, false)) {
          return true;
        }
      }
      return false;
    }
    if (isAssignCall(expr) && !expr.args.empty()) {
      if (exprUsesNameOutsideWriteTarget(expr.args.front(), name, true)) {
        return true;
      }
      for (size_t i = 1; i < expr.args.size(); ++i) {
        if (exprUsesNameOutsideWriteTarget(expr.args[i], name, false)) {
          return true;
        }
      }
      for (const auto &bodyExpr : expr.bodyArguments) {
        if (exprUsesNameOutsideWriteTarget(bodyExpr, name, false)) {
          return true;
        }
      }
      return false;
    }
    std::string mutateName;
    if (getBuiltinMutationName(expr, mutateName) && !expr.args.empty()) {
      if (exprUsesNameOutsideWriteTarget(expr.args.front(), name, true)) {
        return true;
      }
      for (size_t i = 1; i < expr.args.size(); ++i) {
        if (exprUsesNameOutsideWriteTarget(expr.args[i], name, false)) {
          return true;
        }
      }
      for (const auto &bodyExpr : expr.bodyArguments) {
        if (exprUsesNameOutsideWriteTarget(bodyExpr, name, false)) {
          return true;
        }
      }
      return false;
    }
    for (const auto &arg : expr.args) {
      if (exprUsesNameOutsideWriteTarget(arg, name, inWriteTarget)) {
        return true;
      }
    }
    for (const auto &bodyExpr : expr.bodyArguments) {
      if (exprUsesNameOutsideWriteTarget(bodyExpr, name, inWriteTarget)) {
        return true;
      }
    }
    return false;
  };
  auto statementsUseNameOutsideWriteTargetsFrom =
      [&](const std::vector<Expr> &candidates, size_t startIndex, const std::string &name) -> bool {
    for (size_t index = startIndex; index < candidates.size(); ++index) {
      if (exprUsesNameOutsideWriteTarget(candidates[index], name, false)) {
        return true;
      }
    }
    return false;
  };
  auto referenceRootForBinding = [](const std::string &bindingName, const BindingInfo &binding) -> std::string {
    if (binding.typeName != "Reference") {
      return "";
    }
    if (!binding.referenceRoot.empty()) {
      return binding.referenceRoot;
    }
    return bindingName;
  };
  auto pointerAliasUsesReferenceRoot = [&](const std::string &referenceRoot) -> bool {
    if (referenceRoot.empty()) {
      return false;
    }
    auto pointerUsesRoot = [&](const std::string &bindingName, const BindingInfo &binding) -> bool {
      if (binding.typeName != "Pointer" || binding.referenceRoot != referenceRoot) {
        return false;
      }
      return statementsUseNameOutsideWriteTargetsFrom(statements, nextIndex, bindingName);
    };
    for (const auto &param : params) {
      if (pointerUsesRoot(param.name, param.binding)) {
        return true;
      }
    }
    for (const auto &entry : locals) {
      if (pointerUsesRoot(entry.first, entry.second)) {
        return true;
      }
    }
    return false;
  };
  auto updateName = [&](const std::string &bindingName, const BindingInfo &binding) {
    if (binding.typeName != "Reference") {
      return;
    }
    const std::string referenceRoot = referenceRootForBinding(bindingName, binding);
    if (statementsUseNameFrom(statements, nextIndex, bindingName) ||
        pointerAliasUsesReferenceRoot(referenceRoot)) {
      currentValidationContext_.endedReferenceBorrows.erase(bindingName);
      return;
    }
    currentValidationContext_.endedReferenceBorrows.insert(bindingName);
  };
  for (const auto &param : params) {
    updateName(param.name, param.binding);
  }
  for (const auto &entry : locals) {
    updateName(entry.first, entry.second);
  }
}

} // namespace primec::semantics
