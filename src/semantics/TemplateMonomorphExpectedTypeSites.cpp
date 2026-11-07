#include "TemplateMonomorphExpectedTypeSites.h"

#include "TemplateMonomorphFallbackTypeInference.h"
#include "TemplateMonomorphImplicitTemplateInference.h"
#include "TemplateMonomorphTypeResolution.h"

#include <algorithm>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace primec {

namespace {

// A declared type spelled as text (`Function<i32, i32>`, a `return<...>` or `args<...>` argument) as a transform.
std::optional<Transform> transformFromTypeText(const std::string &text) {
  Transform type;
  type.name = text;
  std::string base;
  std::string argText;
  if (semantics::splitTemplateTypeName(text, base, argText)) {
    std::vector<std::string> args;
    if (!semantics::splitTopLevelTemplateArgs(argText, args)) {
      return std::nullopt;
    }
    type.name = base;
    type.templateArgs = std::move(args);
  }
  return type;
}

// The conversions that may target a declared type.
std::vector<const ExpectedTypeConversion *> conversionsFor(const std::vector<const ExpectedTypeConversion *> &active,
                                                           const std::string &typeName) {
  std::vector<const ExpectedTypeConversion *> matching;
  for (const ExpectedTypeConversion *conversion : active) {
    if (conversion->mayBeTarget(typeName)) {
      matching.push_back(conversion);
    }
  }
  return matching;
}

bool convertAt(Expr &arg,
               const std::vector<const ExpectedTypeConversion *> &conversions,
               const std::vector<const ExpectedType *> &expected,
               const ExpectedTypeScope &scope,
               std::string &error) {
  for (const ExpectedTypeConversion *conversion : conversions) {
    if (!conversion->convert(arg, expected, scope, error)) {
      return false;
    }
  }
  return true;
}

bool convertAtSingle(Expr &arg,
                     const std::vector<const ExpectedTypeConversion *> &active,
                     const ExpectedType &expected,
                     const ExpectedTypeScope &scope,
                     std::string &error) {
  const std::vector<const ExpectedTypeConversion *> conversions = conversionsFor(active, expected.type.name);
  return conversions.empty() || convertAt(arg, conversions, {&expected}, scope, error);
}

// The type a variadic parameter (`[args<T>] values`) expects of each value; null for any other parameter.
std::optional<Transform> variadicElementType(const Expr &param) {
  const Transform *type = bindingTypeTransform(param);
  if (type == nullptr || type->name != "args" || type->templateArgs.size() != 1) {
    return std::nullopt;
  }
  return transformFromTypeText(type->templateArgs.front());
}

// Whether any binding (parameter or field) is declared with a type a conversion may target: the cheap test before
// matching arguments.
bool declaresTargetBinding(const std::vector<Expr> &bindings,
                           const std::vector<const ExpectedTypeConversion *> &active) {
  for (const Expr &binding : bindings) {
    if (!binding.isBinding) {
      continue;
    }
    const Transform *type = bindingTypeTransform(binding);
    if (type == nullptr) {
      continue;
    }
    const std::string &name =
        type->name == "args" && type->templateArgs.size() == 1 ? type->templateArgs.front() : type->name;
    for (const ExpectedTypeConversion *conversion : active) {
      if (conversion->mayBeTarget(name)) {
        return true;
      }
    }
  }
  return false;
}

// The definitions a call or method call can target, before monomorphization: one, or the same-arity overloads of a
// family in overload order. A generic candidate is null (its parameter types are not known here).
std::vector<const Definition *> conversionCallees(const Expr &expr, const ExpectedTypeScope &scope) {
  Context &ctx = scope.ctx;
  std::string path;
  if (expr.isMethodCall) {
    std::string receiverPath = resolveStructLikeExprPathForTemplatedVectorFallback(
        expr.args.front(), scope.locals, scope.namespacePrefix, ctx, scope.allowMathBare);
    if (receiverPath.empty()) {
      semantics::BindingInfo receiverInfo;
      if (!inferBindingTypeForMonomorph(expr.args.front(), scope.params, scope.locals, scope.allowMathBare, ctx,
                                        receiverInfo)) {
        return {};
      }
      const bool borrowed = receiverInfo.typeName == "Reference" || receiverInfo.typeName == "Pointer";
      receiverPath = resolveStructLikeTypePathForTemplatedVectorFallback(
          borrowed ? receiverInfo.typeTemplateArg : receiverInfo.typeName, scope.namespacePrefix, ctx);
    }
    if (receiverPath.empty() || receiverPath.front() != '/') {
      return {};
    }
    path = receiverPath + "/" + expr.name;
  } else {
    path = resolveCalleePath(expr, scope.namespacePrefix, ctx, &scope.locals, &scope.params);
  }
  auto genericOrNull = [](const Definition &def) { return def.templateArgs.empty() ? &def : nullptr; };
  if (auto defIt = ctx.sourceDefs.find(path); defIt != ctx.sourceDefs.end()) {
    return {genericOrNull(defIt->second)};
  }
  auto overloadIt = ctx.helperOverloads.find(path);
  if (overloadIt == ctx.helperOverloads.end()) {
    return {};
  }
  const size_t argumentCount = expr.args.size() - (expr.isMethodCall ? 1 : 0);
  std::vector<const Definition *> candidates;
  for (const HelperOverloadEntry &entry : overloadIt->second) {
    if (entry.isVariadic || entry.parameterCount != argumentCount) {
      continue;
    }
    auto defIt = ctx.sourceDefs.find(entry.internalPath);
    if (defIt == ctx.sourceDefs.end()) {
      continue;
    }
    candidates.push_back(genericOrNull(defIt->second));
  }
  return candidates;
}

// The parameter type `callee` expects of each argument of `expr`; unset where it is not known.
std::vector<std::optional<ExpectedType>> expectedParameterTypes(const Expr &expr, const Definition &callee) {
  std::vector<std::optional<ExpectedType>> expected(expr.args.size());
  size_t offset = 0;
  if (expr.isMethodCall && (callee.parameters.size() < expr.args.size() || callee.parameters.empty() ||
                            callee.parameters.front().name != "self")) {
    offset = 1;  // the receiver is the implicit `this`
  }
  std::optional<Transform> variadic;
  if (!callee.parameters.empty()) {
    variadic = variadicElementType(callee.parameters.back());
  }
  size_t positional = 0;
  for (size_t i = offset; i < expr.args.size(); ++i) {
    if (expr.args[i].isSpread) {
      break;
    }
    const Expr *param = nullptr;
    if (i < expr.argNames.size() && expr.argNames[i].has_value()) {
      for (const Expr &candidate : callee.parameters) {
        if (candidate.name == *expr.argNames[i]) {
          param = &candidate;
          break;
        }
      }
    } else if (variadic.has_value() && positional + 1 >= callee.parameters.size()) {
      expected[i] = ExpectedType{*variadic, callee.namespacePrefix};
      ++positional;
      continue;
    } else if (positional < callee.parameters.size()) {
      param = &callee.parameters[positional++];
    }
    const Transform *type = param == nullptr ? nullptr : bindingTypeTransform(*param);
    if (type != nullptr) {
      expected[i] = ExpectedType{*type, callee.namespacePrefix};
    }
  }
  return expected;
}

bool rewriteCallSite(Expr &expr,
                     const std::vector<const ExpectedTypeConversion *> &active,
                     const ExpectedTypeScope &scope,
                     std::string &error) {
  // Resolve the callee only when an argument may need a conversion.
  bool anyCandidateArgument = false;
  for (size_t i = expr.isMethodCall ? 1 : 0; i < expr.args.size() && !anyCandidateArgument; ++i) {
    for (const ExpectedTypeConversion *conversion : active) {
      if (conversion->mayConvert(expr.args[i], scope)) {
        anyCandidateArgument = true;
        break;
      }
    }
  }
  if (!anyCandidateArgument) {
    return true;
  }
  const std::vector<const Definition *> callees = conversionCallees(expr, scope);
  bool anyTarget = false;
  for (const Definition *callee : callees) {
    if (callee != nullptr && !isStructDefinition(*callee) && declaresTargetBinding(callee->parameters, active)) {
      anyTarget = true;
    }
  }
  if (!anyTarget) {
    return true;
  }
  std::vector<std::vector<std::optional<ExpectedType>>> perCallee;
  perCallee.reserve(callees.size());
  for (const Definition *callee : callees) {
    if (callee == nullptr || isStructDefinition(*callee)) {
      perCallee.emplace_back(expr.args.size());
    } else {
      perCallee.push_back(expectedParameterTypes(expr, *callee));
    }
  }
  for (size_t i = expr.isMethodCall ? 1 : 0; i < expr.args.size(); ++i) {
    std::vector<const ExpectedType *> expected;
    std::vector<const ExpectedTypeConversion *> conversions;
    for (const auto &types : perCallee) {
      const std::optional<ExpectedType> &type = types[i];
      expected.push_back(type.has_value() ? &*type : nullptr);
      if (!type.has_value()) {
        continue;
      }
      for (const ExpectedTypeConversion *conversion : conversionsFor(active, type->type.name)) {
        if (std::find(conversions.begin(), conversions.end(), conversion) == conversions.end()) {
          conversions.push_back(conversion);
        }
      }
    }
    if (!conversions.empty() && !convertAt(expr.args[i], conversions, expected, scope, error)) {
      return false;
    }
  }
  return true;
}

bool rewriteBraceConstructorSite(Expr &expr,
                                 const std::vector<const ExpectedTypeConversion *> &conversions,
                                 const ExpectedTypeScope &scope,
                                 std::string &error) {
  std::vector<const ExpectedTypeConversion *> active;
  for (const ExpectedTypeConversion *conversion : conversions) {
    if (!conversion->skipsConstructor(expr.name)) {
      active.push_back(conversion);
    }
  }
  if (active.empty()) {
    return true;
  }
  const std::string path = resolveCalleePath(expr, scope.namespacePrefix, scope.ctx, &scope.locals, &scope.params);
  auto defIt = scope.ctx.sourceDefs.find(path);
  if (defIt == scope.ctx.sourceDefs.end() || !defIt->second.templateArgs.empty() ||
      !isStructDefinition(defIt->second) || !declaresTargetBinding(defIt->second.statements, active)) {
    return true;
  }
  const Definition &structDef = defIt->second;
  std::vector<const Expr *> fields;
  for (const Expr &stmt : structDef.statements) {
    bool isStatic = false;
    for (const Transform &transform : stmt.transforms) {
      isStatic = isStatic || transform.name == "static";
    }
    if (stmt.isBinding && !isStatic) {
      fields.push_back(&stmt);
    }
  }
  size_t positional = 0;
  for (size_t i = 0; i < expr.args.size(); ++i) {
    const Expr *field = nullptr;
    if (i < expr.argNames.size() && expr.argNames[i].has_value()) {
      for (const Expr *candidate : fields) {
        if (candidate->name == *expr.argNames[i]) {
          field = candidate;
          break;
        }
      }
    } else if (positional < fields.size()) {
      field = fields[positional++];
    }
    const Transform *type = field == nullptr ? nullptr : bindingTypeTransform(*field);
    if (type != nullptr &&
        !convertAtSingle(expr.args[i], active, ExpectedType{*type, structDef.namespacePrefix}, scope, error)) {
      return false;
    }
  }
  return true;
}

bool rewriteReturnSite(Expr &expr,
                       const std::vector<const ExpectedTypeConversion *> &active,
                       const ExpectedTypeScope &scope,
                       std::string &error) {
  const Definition *current = scope.ctx.currentRewriteDefinition;
  if (current == nullptr) {
    return true;
  }
  for (const Transform &transform : current->transforms) {
    if (transform.name != "return" || transform.templateArgs.size() != 1 ||
        conversionsFor(active, transform.templateArgs.front()).empty()) {
      continue;
    }
    std::optional<Transform> type = transformFromTypeText(transform.templateArgs.front());
    if (!type.has_value()) {
      return true;
    }
    const ExpectedType expected{*type, scope.namespacePrefix, &scope.mapping, &scope.allowedParams};
    if (!convertAtSingle(expr.args.front(), active, expected, scope, error)) {
      return false;
    }
  }
  return true;
}

bool rewriteAssignmentSite(Expr &expr,
                           const std::vector<const ExpectedTypeConversion *> &active,
                           const ExpectedTypeScope &scope,
                           std::string &error) {
  const semantics::BindingInfo *target = nullptr;
  if (auto localIt = scope.locals.find(expr.args.front().name); localIt != scope.locals.end()) {
    target = &localIt->second;
  } else {
    for (const semantics::ParameterInfo &param : scope.params) {
      if (param.name == expr.args.front().name) {
        target = &param.binding;
        break;
      }
    }
  }
  if (target == nullptr) {
    return true;
  }
  ExpectedType expected;
  expected.type.name = target->typeName;
  if (!target->typeTemplateArg.empty()) {
    expected.type.templateArgs.push_back(target->typeTemplateArg);
  }
  expected.typeNamespace = scope.namespacePrefix;
  expected.resolved = true;
  return convertAtSingle(expr.args[1], active, expected, scope, error);
}

} // namespace

bool isFieldQualifierTransformName(const std::string &name) {
  return name == "mut" || name == "public" || name == "private" || name == "static" || name == "copy" ||
         name == "move" || name == "restrict" || name == "unsafe";
}

const Transform *bindingTypeTransform(const Expr &binding) {
  for (const Transform &transform : binding.transforms) {
    if (!isFieldQualifierTransformName(transform.name)) {
      return &transform;
    }
  }
  return nullptr;
}

std::string expectedTypePathLeaf(const std::string &path) {
  const size_t slash = path.rfind('/');
  return slash == std::string::npos ? path : path.substr(slash + 1);
}

bool rewriteExpectedTypeSites(Expr &expr,
                              const std::vector<const ExpectedTypeConversion *> &conversions,
                              const ExpectedTypeScope &scope,
                              std::string &error) {
  if (expr.kind != Expr::Kind::Call || expr.args.empty()) {
    return true;
  }
  std::vector<const ExpectedTypeConversion *> active;
  for (const ExpectedTypeConversion *conversion : conversions) {
    if (conversion->enabled(scope.ctx)) {
      active.push_back(conversion);
    }
  }
  if (active.empty()) {
    return true;
  }
  // `[T mut] name{value}`: the binding's own type, already rewritten.
  if (expr.isBinding) {
    if (expr.args.size() == 1) {
      if (const Transform *type = bindingTypeTransform(expr)) {
        const ExpectedType expected{*type, scope.namespacePrefix, &scope.mapping, &scope.allowedParams};
        return convertAtSingle(expr.args.front(), active, expected, scope, error);
      }
    }
    return true;
  }
  if (expr.isFieldAccess || expr.isLambda) {
    return true;
  }
  if (!expr.isMethodCall && !expr.isBraceConstructor && expr.args.size() == 1 && expr.name == "return") {
    return rewriteReturnSite(expr, active, scope, error);
  }
  if (!expr.isMethodCall && !expr.isBraceConstructor && expr.args.size() == 2 && expr.name == "assign" &&
      expr.args.front().kind == Expr::Kind::Name) {
    return rewriteAssignmentSite(expr, active, scope, error);
  }
  if (expr.isBraceConstructor) {
    return rewriteBraceConstructorSite(expr, active, scope, error);
  }
  if (expr.name.empty()) {
    return true;
  }
  return rewriteCallSite(expr, active, scope, error);
}

} // namespace primec
