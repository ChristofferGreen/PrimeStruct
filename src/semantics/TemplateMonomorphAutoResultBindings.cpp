#include "TemplateMonomorphAutoResultBindings.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "TemplateMonomorphImplicitTemplateInference.h"
#include "TemplateMonomorphMethodTargets.h"
#include "TemplateMonomorphSetupUtilities.h"

namespace primec {

namespace {

// What the initializer says of a `Result`: the payload type (empty for a status-only Result) and the error type.
struct ResultParts {
  bool hasValue = true;
  std::string valueType;
  std::string errorType;
};

struct InferenceScope {
  const std::vector<semantics::ParameterInfo> &params;
  const std::string &namespacePrefix;
  bool allowMathBare;
  Context &ctx;
};

bool sumIsVisible(const InferenceScope &scope) {
  const std::string path =
      resolveNameToPath("Result",
                        scope.namespacePrefix,
                        scopedImportAliasesForNamespace(scope.namespacePrefix, scope.ctx),
                        scope.ctx.sourceDefs);
  if (path.rfind("/std/result/Result", 0) != 0) {
    return false;
  }
  return scope.ctx.templateDefs.count(path) > 0 || scope.ctx.genericTypeOverloads.count(path) > 0;
}

bool isResultCall(const Expr &expr, const char *name, size_t argCount) {
  if (expr.kind != Expr::Kind::Call || !expr.isMethodCall || expr.name != name ||
      expr.args.size() != argCount || !expr.templateArgs.empty() || expr.hasBodyArguments ||
      !expr.bodyArguments.empty()) {
    return false;
  }
  const Expr &receiver = expr.args.front();
  return receiver.kind == Expr::Kind::Name &&
         semantics::normalizeBindingTypeName(receiver.name) == "Result";
}

// `Result<T, E>` or `Result<E>` as written in a `return<...>` transform.
std::optional<ResultParts> partsFromResultTypeText(const std::string &typeText) {
  std::string base;
  std::string argText;
  if (!semantics::splitTemplateTypeName(
          semantics::normalizeBindingTypeName(typeText), base, argText) ||
      semantics::normalizeBindingTypeName(base) != "Result") {
    return std::nullopt;
  }
  std::vector<std::string> args;
  if (!semantics::splitTopLevelTemplateArgs(argText, args) || args.empty() || args.size() > 2) {
    return std::nullopt;
  }
  ResultParts parts;
  parts.hasValue = args.size() == 2;
  parts.valueType = parts.hasValue ? args.front() : std::string{};
  parts.errorType = args.back();
  return parts;
}

// The error type a bare `Result.ok(x)` takes: the one `try` reports through, else the definition's `Result` return.
std::string contextErrorType(const Context &ctx) {
  const Definition *def = ctx.currentRewriteDefinition;
  if (def == nullptr) {
    return {};
  }
  for (const Transform &transform : def->transforms) {
    if (transform.name == "on_error" && !transform.templateArgs.empty()) {
      return transform.templateArgs.front();
    }
  }
  for (const Transform &transform : def->transforms) {
    if (transform.name == "return" && transform.templateArgs.size() == 1) {
      if (const std::optional<ResultParts> parts =
              partsFromResultTypeText(transform.templateArgs.front())) {
        return parts->errorType;
      }
    }
  }
  return {};
}

std::optional<ResultParts> partsFromBindingInfo(const semantics::BindingInfo &info,
                                                const Context &ctx) {
  if (info.typeName == "Result" && !info.typeTemplateArg.empty()) {
    return partsFromResultTypeText("Result<" + info.typeTemplateArg + ">");
  }
  // A declared sum was instantiated before this local was recorded: its template arguments are on the instance.
  const auto defIt = ctx.sourceDefs.find(info.typeName);
  if (defIt == ctx.sourceDefs.end() ||
      defIt->second.instanceOfTemplate.rfind("/std/result/Result", 0) != 0) {
    return std::nullopt;
  }
  const std::vector<std::string> &args = defIt->second.instanceTemplateArgs;
  if (args.empty() || args.size() > 2) {
    return std::nullopt;
  }
  ResultParts parts;
  parts.hasValue = args.size() == 2;
  parts.valueType = parts.hasValue ? args.front() : std::string{};
  parts.errorType = args.back();
  return parts;
}

std::optional<ResultParts>
resultPartsOf(const Expr &expr, const LocalTypeMap &locals, const InferenceScope &scope);

// The value type of the expression a lambda body returns, with the lambda's parameters and bindings in scope.
void collectReturnedExprs(const std::vector<Expr> &statements,
                          LocalTypeMap locals,
                          const InferenceScope &scope,
                          std::vector<std::pair<const Expr *, LocalTypeMap>> &returned) {
  for (const Expr &stmt : statements) {
    if (stmt.kind != Expr::Kind::Call) {
      continue;
    }
    if (semantics::isReturnCall(stmt)) {
      if (stmt.args.size() == 1) {
        returned.emplace_back(&stmt.args.front(), locals);
      }
      continue;
    }
    if (stmt.isBinding) {
      semantics::BindingInfo info;
      if (extractExplicitBindingType(stmt, info) && info.typeName != "auto") {
        locals[stmt.name] = info;
      } else if (stmt.args.size() == 1 && inferBindingTypeForMonomorph(stmt.args.front(),
                                                                       scope.params,
                                                                       locals,
                                                                       scope.allowMathBare,
                                                                       scope.ctx,
                                                                       info)) {
        locals[stmt.name] = info;
      }
      continue;
    }
    // `if(...) { return(...) }`: the branches are bodies of their own; their bindings stay inside.
    for (const Expr &arg : stmt.args) {
      if (arg.kind == Expr::Kind::Call && !arg.bodyArguments.empty()) {
        collectReturnedExprs(arg.bodyArguments, locals, scope, returned);
      }
    }
    collectReturnedExprs(stmt.bodyArguments, locals, scope, returned);
  }
}

std::optional<LocalTypeMap>
lambdaLocals(const Expr &lambda, const LocalTypeMap &locals, const InferenceScope &scope) {
  LocalTypeMap lambdaScope = locals;
  for (const Expr &param : lambda.args) {
    semantics::BindingInfo info;
    if (!extractExplicitBindingType(param, info) || info.typeName == "auto") {
      return std::nullopt;
    }
    lambdaScope[param.name] = info;
  }
  (void)scope;
  return lambdaScope;
}

// The one type every `return` of the lambda body has, as the parts of a `Result` when `wantResult` is set.
std::optional<ResultParts> lambdaReturnParts(const Expr &lambda,
                                             const LocalTypeMap &locals,
                                             const InferenceScope &scope,
                                             bool wantResult) {
  if (!lambda.isLambda || lambda.hasBodyArguments == false) {
    return std::nullopt;
  }
  const std::optional<LocalTypeMap> bodyLocals = lambdaLocals(lambda, locals, scope);
  if (!bodyLocals.has_value()) {
    return std::nullopt;
  }
  std::vector<std::pair<const Expr *, LocalTypeMap>> returned;
  collectReturnedExprs(lambda.bodyArguments, *bodyLocals, scope, returned);
  if (returned.empty()) {
    return std::nullopt;
  }
  std::optional<ResultParts> agreed;
  for (const auto &entry : returned) {
    ResultParts parts;
    if (wantResult) {
      const std::optional<ResultParts> resultParts =
          resultPartsOf(*entry.first, entry.second, scope);
      if (!resultParts.has_value()) {
        return std::nullopt;
      }
      parts = *resultParts;
    } else {
      semantics::BindingInfo info;
      if (!inferBindingTypeForMonomorph(
              *entry.first, scope.params, entry.second, scope.allowMathBare, scope.ctx, info)) {
        return std::nullopt;
      }
      parts.valueType = bindingTypeToString(info);
      if (parts.valueType.empty()) {
        return std::nullopt;
      }
    }
    if (agreed.has_value() &&
        (agreed->valueType != parts.valueType || agreed->hasValue != parts.hasValue)) {
      return std::nullopt;
    }
    agreed = parts;
  }
  return agreed;
}

std::optional<ResultParts>
resultPartsOf(const Expr &expr, const LocalTypeMap &locals, const InferenceScope &scope) {
  if (expr.kind == Expr::Kind::Name) {
    const auto localIt = locals.find(expr.name);
    if (localIt == locals.end()) {
      return std::nullopt;
    }
    return partsFromBindingInfo(localIt->second, scope.ctx);
  }
  if (isResultCall(expr, "ok", 1) || isResultCall(expr, "ok", 2)) {
    const std::string errorType = contextErrorType(scope.ctx);
    if (errorType.empty()) {
      return std::nullopt;
    }
    ResultParts parts;
    parts.errorType = errorType;
    if (expr.args.size() == 1) {
      parts.hasValue = false;
      return parts;
    }
    semantics::BindingInfo payload;
    if (!inferBindingTypeForMonomorph(
            expr.args.back(), scope.params, locals, scope.allowMathBare, scope.ctx, payload)) {
      return std::nullopt;
    }
    parts.valueType = bindingTypeToString(payload);
    if (parts.valueType.empty()) {
      return std::nullopt;
    }
    return parts;
  }
  const bool isMap = isResultCall(expr, "map", 3);
  if (!isMap && !isResultCall(expr, "and_then", 3)) {
    return std::nullopt;
  }
  const std::optional<ResultParts> source = resultPartsOf(expr.args[1], locals, scope);
  if (!source.has_value() || !source->hasValue) {
    return std::nullopt;
  }
  const std::optional<ResultParts> mapped = lambdaReturnParts(expr.args[2], locals, scope, !isMap);
  if (!mapped.has_value() || !mapped->hasValue) {
    return std::nullopt;
  }
  ResultParts parts;
  parts.valueType = mapped->valueType;
  parts.errorType = source->errorType;
  return parts;
}

} // namespace

void materializeAutoResultBindingType(Expr &binding,
                                      const std::vector<semantics::ParameterInfo> &params,
                                      const LocalTypeMap &locals,
                                      const std::string &namespacePrefix,
                                      bool allowMathBare,
                                      Context &ctx) {
  if (!binding.isBinding || binding.args.size() != 1) {
    return;
  }
  Transform *autoTransform = nullptr;
  for (Transform &transform : binding.transforms) {
    if (transform.name == "auto" && transform.templateArgs.empty() && transform.arguments.empty()) {
      autoTransform = &transform;
    }
  }
  if (autoTransform == nullptr) {
    return;
  }
  const Expr &initializer = binding.args.front();
  if (!isResultCall(initializer, "ok", 1) && !isResultCall(initializer, "ok", 2) &&
      !isResultCall(initializer, "map", 3) && !isResultCall(initializer, "and_then", 3)) {
    return;
  }
  const InferenceScope scope{params, namespacePrefix, allowMathBare, ctx};
  if (!sumIsVisible(scope)) {
    return;
  }
  const std::optional<ResultParts> parts = resultPartsOf(initializer, locals, scope);
  if (!parts.has_value() || parts->errorType.empty() ||
      (parts->hasValue && parts->valueType.empty())) {
    return;
  }
  // The source of a combinator is a stdlib sum too: a packed `Result.ok(x)` there is written as the sum helper.
  if ((isResultCall(initializer, "map", 3) || isResultCall(initializer, "and_then", 3)) &&
      isResultCall(initializer.args[1], "ok", 2)) {
    const std::optional<ResultParts> sourceParts =
        resultPartsOf(initializer.args[1], locals, scope);
    if (!sourceParts.has_value() || !sourceParts->hasValue) {
      return;
    }
    Expr &source = binding.args.front().args[1];
    Expr sumOk;
    sumOk.kind = Expr::Kind::Call;
    sumOk.name = "/std/result/ok";
    sumOk.sourceName = source.sourceName;
    sumOk.sourceLine = source.sourceLine;
    sumOk.sourceColumn = source.sourceColumn;
    sumOk.namespacePrefix = source.namespacePrefix;
    sumOk.templateArgs = {sourceParts->valueType, sourceParts->errorType};
    sumOk.args.push_back(source.args.back());
    sumOk.argNames.push_back(std::nullopt);
    source = std::move(sumOk);
  }
  autoTransform->name = "Result";
  if (parts->hasValue) {
    autoTransform->templateArgs = {parts->valueType, parts->errorType};
  } else {
    autoTransform->templateArgs = {parts->errorType};
  }
}

} // namespace primec
