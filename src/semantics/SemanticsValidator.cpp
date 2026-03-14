#include "SemanticsValidator.h"

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
      collectDiagnostics_(collectDiagnostics) {
  if (diagnosticInfo_ != nullptr) {
    *diagnosticInfo_ = {};
  }
  for (const auto &importPath : program_.imports) {
    if (importPath == "/std/math/*") {
      mathImportAll_ = true;
      continue;
    }
    if (importPath.rfind("/std/math/", 0) == 0 && importPath.size() > 10) {
      std::string name = importPath.substr(10);
      if (name.find('/') == std::string::npos && name != "*") {
        mathImports_.insert(std::move(name));
      }
    }
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

SemanticsValidator::ValidationContext
SemanticsValidator::buildDefinitionValidationContext(const Definition &def) const {
  ValidationContext context;
  context.definitionPath = def.fullPath;
  for (const auto &transform : def.transforms) {
    if (transform.name == "compute") {
      context.definitionIsCompute = true;
    } else if (transform.name == "unsafe") {
      context.definitionIsUnsafe = true;
    }
  }
  context.activeEffects = resolveEffects(def.transforms, def.fullPath == entryPath_);
  return context;
}

SemanticsValidator::ValidationContext
SemanticsValidator::buildExecutionValidationContext(const Execution &exec) const {
  ValidationContext context;
  context.definitionPath.clear();
  context.definitionIsCompute = false;
  context.definitionIsUnsafe = false;
  context.activeEffects = resolveEffects(exec.transforms, false);
  return context;
}

SemanticsValidator::ValidationContext SemanticsValidator::snapshotValidationContext() const {
  ValidationContext snapshot;
  snapshot.activeEffects = activeEffects_;
  snapshot.movedBindings = movedBindings_;
  snapshot.endedReferenceBorrows = endedReferenceBorrows_;
  snapshot.definitionPath = currentDefinitionPath_;
  snapshot.definitionIsCompute = currentDefinitionIsCompute_;
  snapshot.definitionIsUnsafe = currentDefinitionIsUnsafe_;
  snapshot.resultType = currentResultType_;
  snapshot.onError = currentOnError_;
  return snapshot;
}

void SemanticsValidator::restoreValidationContext(ValidationContext context) {
  activeEffects_ = std::move(context.activeEffects);
  movedBindings_ = std::move(context.movedBindings);
  endedReferenceBorrows_ = std::move(context.endedReferenceBorrows);
  currentDefinitionPath_ = std::move(context.definitionPath);
  currentDefinitionIsCompute_ = context.definitionIsCompute;
  currentDefinitionIsUnsafe_ = context.definitionIsUnsafe;
  currentResultType_ = std::move(context.resultType);
  currentOnError_ = std::move(context.onError);
}

void SemanticsValidator::capturePrimarySpanIfUnset(int line, int column) {
  if (diagnosticInfo_ == nullptr) {
    return;
  }
  if (line <= 0 || column <= 0) {
    return;
  }
  if (diagnosticInfo_->line > 0 && diagnosticInfo_->column > 0) {
    return;
  }
  diagnosticInfo_->line = line;
  diagnosticInfo_->column = column;
}

void SemanticsValidator::captureRelatedSpan(int line, int column, const std::string &label) {
  if (diagnosticInfo_ == nullptr) {
    return;
  }
  if (line <= 0 || column <= 0 || label.empty()) {
    return;
  }
  for (const auto &related : diagnosticInfo_->relatedSpans) {
    if (related.line == line && related.column == column && related.label == label) {
      return;
    }
  }
  SemanticDiagnosticRelatedSpan related;
  related.line = line;
  related.column = column;
  related.label = label;
  diagnosticInfo_->relatedSpans.push_back(std::move(related));
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
    diagnosticInfo_->records.clear();
    diagnosticInfo_->records.reserve(duplicateGroups.size());
    for (const auto &group : duplicateGroups) {
      SemanticDiagnosticRecord record;
      record.message = "duplicate definition: " + group.path;
      const Definition *primary = group.definitions.front();
      if (primary->sourceLine > 0 && primary->sourceColumn > 0) {
        record.line = primary->sourceLine;
        record.column = primary->sourceColumn;
      }
      for (const Definition *def : group.definitions) {
        if (def->sourceLine <= 0 || def->sourceColumn <= 0) {
          continue;
        }
        const std::string label = "definition: " + def->fullPath;
        bool duplicateSpan = false;
        for (const auto &existing : record.relatedSpans) {
          if (existing.line == def->sourceLine && existing.column == def->sourceColumn && existing.label == label) {
            duplicateSpan = true;
            break;
          }
        }
        if (duplicateSpan) {
          continue;
        }
        SemanticDiagnosticRelatedSpan span;
        span.line = def->sourceLine;
        span.column = def->sourceColumn;
        span.label = label;
        record.relatedSpans.push_back(std::move(span));
      }
      diagnosticInfo_->records.push_back(std::move(record));
    }
    if (!diagnosticInfo_->records.empty()) {
      diagnosticInfo_->line = diagnosticInfo_->records.front().line;
      diagnosticInfo_->column = diagnosticInfo_->records.front().column;
      diagnosticInfo_->relatedSpans = diagnosticInfo_->records.front().relatedSpans;
    }
  }

  error_ = "duplicate definition: " + duplicateGroups.front().path;
  return false;
}

bool SemanticsValidator::run() {
  if (collectDiagnostics_ && !collectDuplicateDefinitionDiagnostics()) {
    return false;
  }
  if (!buildDefinitionMaps()) {
    return false;
  }
  if (!inferUnknownReturnKinds()) {
    return false;
  }
  if (!validateTraitConstraints()) {
    return false;
  }
  if (!validateStructLayouts()) {
    return false;
  }
  if (!validateDefinitions()) {
    return false;
  }
  if (!validateExecutions()) {
    return false;
  }
  return validateEntry();
}

bool SemanticsValidator::allowMathBareName(const std::string &name) const {
  if (name.empty() || name.find('/') != std::string::npos) {
    return false;
  }
  if (!currentDefinitionPath_.empty()) {
    if (currentDefinitionPath_ == "/std/math" || currentDefinitionPath_.rfind("/std/math/", 0) == 0) {
      return true;
    }
  }
  return mathImportAll_ || mathImports_.count(name) > 0;
}

bool SemanticsValidator::hasAnyMathImport() const {
  return mathImportAll_ || !mathImports_.empty();
}

bool SemanticsValidator::isEntryArgsName(const std::string &name) const {
  if (currentDefinitionPath_ != entryPath_) {
    return false;
  }
  if (entryArgsName_.empty()) {
    return false;
  }
  return name == entryArgsName_;
}

bool SemanticsValidator::isEntryArgsAccess(const Expr &expr) const {
  if (currentDefinitionPath_ != entryPath_ || entryArgsName_.empty()) {
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
  if (currentDefinitionPath_ != entryPath_) {
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
                                                  ResultTypeInfo &out) const {
  out = ResultTypeInfo{};
  auto resolveMethodResultPath = [&]() -> std::string {
    if (!expr.isMethodCall || expr.args.empty()) {
      return "";
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
      }
      if (receiver.kind == Expr::Kind::Call && !receiver.isMethodCall) {
        const std::string resolvedReceiverPath = resolveCalleePath(receiver);
        if (!resolvedReceiverPath.empty()) {
          return resolvedReceiverPath;
        }
      }
      return std::string();
    }();
    if (receiverTypeName.empty()) {
      return "";
    }
    if (receiverTypeName == "File") {
      return "";
    }
    if (isPrimitiveBindingTypeName(receiverTypeName)) {
      return "/" + normalizeBindingTypeName(receiverTypeName) + "/" + expr.name;
    }
    const std::string resolvedType =
        (!receiverTypeName.empty() && receiverTypeName.front() == '/') ? receiverTypeName
                                                                       : resolveTypePath(receiverTypeName, receiver.namespacePrefix);
    if (resolvedType.empty()) {
      return "";
    }
    return resolvedType + "/" + expr.name;
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
  if (!expr.isMethodCall && isSimpleCallName(expr, "File") && expr.templateArgs.size() == 1) {
    out.isResult = true;
    out.hasValue = true;
    out.valueType = "File<" + expr.templateArgs.front() + ">";
    out.errorType = "FileError";
    return true;
  }
  if (expr.isMethodCall) {
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
      if (it != defMap_.end()) {
        for (const auto &transform : it->second->transforms) {
          if (transform.name != "return" || transform.templateArgs.size() != 1) {
            continue;
          }
          return resolveResultTypeFromTypeName(transform.templateArgs.front(), out);
        }
      }
    }
  }
  if (!expr.isMethodCall) {
    const std::string resolved = resolveCalleePath(expr);
    auto it = defMap_.find(resolved);
    if (it != defMap_.end()) {
      for (const auto &transform : it->second->transforms) {
        if (transform.name != "return" || transform.templateArgs.size() != 1) {
          continue;
        }
        return resolveResultTypeFromTypeName(transform.templateArgs.front(), out);
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
      endedReferenceBorrows_.erase(bindingName);
      return;
    }
    endedReferenceBorrows_.insert(bindingName);
  };
  for (const auto &param : params) {
    updateName(param.name, param.binding);
  }
  for (const auto &entry : locals) {
    updateName(entry.first, entry.second);
  }
}

} // namespace primec::semantics
