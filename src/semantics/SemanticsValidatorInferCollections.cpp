#include "SemanticsValidator.h"

#include <functional>
#include <optional>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace primec::semantics {
namespace {

std::string bindingTypeText(const BindingInfo &binding) {
  if (binding.typeTemplateArg.empty()) {
    return binding.typeName;
  }
  return binding.typeName + "<" + binding.typeTemplateArg + ">";
}

bool isDirectMapConstructorPath(std::string_view resolvedCandidate) {
  auto matchesDirectMapConstructorPath = [&](std::string_view basePath) {
    return resolvedCandidate == basePath || resolvedCandidate.rfind(std::string(basePath) + "__t", 0) == 0;
  };
  return matchesDirectMapConstructorPath("/std/collections/map/map") ||
         matchesDirectMapConstructorPath("/std/collections/mapNew") ||
         matchesDirectMapConstructorPath("/std/collections/mapSingle") ||
         matchesDirectMapConstructorPath("/std/collections/mapDouble") ||
         matchesDirectMapConstructorPath("/std/collections/mapPair") ||
         matchesDirectMapConstructorPath("/std/collections/mapTriple") ||
         matchesDirectMapConstructorPath("/std/collections/mapQuad") ||
         matchesDirectMapConstructorPath("/std/collections/mapQuint") ||
         matchesDirectMapConstructorPath("/std/collections/mapSext") ||
         matchesDirectMapConstructorPath("/std/collections/mapSept") ||
         matchesDirectMapConstructorPath("/std/collections/mapOct") ||
         matchesDirectMapConstructorPath("/std/collections/experimental_map/mapNew") ||
         matchesDirectMapConstructorPath("/std/collections/experimental_map/mapSingle") ||
         matchesDirectMapConstructorPath("/std/collections/experimental_map/mapDouble") ||
         matchesDirectMapConstructorPath("/std/collections/experimental_map/mapPair") ||
         matchesDirectMapConstructorPath("/std/collections/experimental_map/mapTriple") ||
         matchesDirectMapConstructorPath("/std/collections/experimental_map/mapQuad") ||
         matchesDirectMapConstructorPath("/std/collections/experimental_map/mapQuint") ||
         matchesDirectMapConstructorPath("/std/collections/experimental_map/mapSext") ||
         matchesDirectMapConstructorPath("/std/collections/experimental_map/mapSept") ||
         matchesDirectMapConstructorPath("/std/collections/experimental_map/mapOct");
}

} // namespace

std::string SemanticsValidator::normalizeCollectionTypePath(const std::string &typePath) const {
  if (typePath == "/array" || typePath == "array") {
    return "/array";
  }
  if (typePath == "/vector" || typePath == "vector" || typePath == "/std/collections/vector" ||
      typePath == "std/collections/vector") {
    return "/vector";
  }
  if (typePath == "/soa_vector" || typePath == "soa_vector") {
    return "/soa_vector";
  }
  if (isMapCollectionTypeName(typePath) || typePath == "/map" || typePath == "/std/collections/map") {
    return "/map";
  }
  if (typePath == "/string" || typePath == "string") {
    return "/string";
  }
  return "";
}

bool SemanticsValidator::hasImportedDefinitionPath(const std::string &path) const {
  std::string canonicalPath = path;
  const size_t suffix = canonicalPath.find("__t");
  if (suffix != std::string::npos) {
    canonicalPath.erase(suffix);
  }
  for (const auto &importPath : program_.imports) {
    if (importPath == canonicalPath) {
      return true;
    }
    if (importPath.size() >= 2 && importPath.compare(importPath.size() - 2, 2, "/*") == 0) {
      const std::string prefix = importPath.substr(0, importPath.size() - 2);
      if (canonicalPath == prefix || canonicalPath.rfind(prefix + "/", 0) == 0) {
        return true;
      }
    }
  }
  return false;
}

bool SemanticsValidator::inferDefinitionReturnBinding(const Definition &def, BindingInfo &bindingOut) {
  auto findDefParamBinding = [&](const std::vector<ParameterInfo> &defParams,
                                 const std::string &name) -> const BindingInfo * {
    for (const auto &param : defParams) {
      if (param.name == name) {
        return &param.binding;
      }
    }
    return nullptr;
  };
  auto parseTypeText = [&](const std::string &typeText, BindingInfo &parsedOut) -> bool {
    const std::string normalized = normalizeBindingTypeName(typeText);
    if (normalized.empty()) {
      return false;
    }
    std::string base;
    std::string argText;
    if (splitTemplateTypeName(normalized, base, argText) && !base.empty()) {
      parsedOut.typeName = base;
      parsedOut.typeTemplateArg = argText;
      return true;
    }
    parsedOut.typeName = normalized;
    parsedOut.typeTemplateArg.clear();
    return true;
  };

  for (const auto &transform : def.transforms) {
    if (transform.name != "return" || transform.templateArgs.size() != 1) {
      continue;
    }
    const std::string &returnType = transform.templateArgs.front();
    if (returnType == "auto") {
      break;
    }
    return parseTypeText(returnType, bindingOut);
  }

  ValidationContextScope validationContextScope(*this, buildDefinitionValidationContext(def));

  std::vector<ParameterInfo> defParams;
  defParams.reserve(def.parameters.size());
  for (const auto &paramExpr : def.parameters) {
    ParameterInfo paramInfo;
    paramInfo.name = paramExpr.name;
    std::optional<std::string> restrictType;
    std::string parseError;
    (void)parseBindingInfo(
        paramExpr, def.namespacePrefix, structNames_, importAliases_, paramInfo.binding, restrictType, parseError);
    if (paramExpr.args.size() == 1) {
      paramInfo.defaultExpr = &paramExpr.args.front();
    }
    defParams.push_back(std::move(paramInfo));
  }

  std::unordered_map<std::string, BindingInfo> defLocals;
  const Expr *valueExpr = nullptr;
  bool sawReturn = false;
  for (const auto &stmt : def.statements) {
    if (stmt.isBinding) {
      BindingInfo binding;
      std::optional<std::string> restrictType;
      std::string parseError;
      if (parseBindingInfo(stmt, def.namespacePrefix, structNames_, importAliases_, binding, restrictType, parseError)) {
        defLocals[stmt.name] = binding;
      } else if (stmt.args.size() == 1 &&
                 inferBindingTypeFromInitializer(stmt.args.front(), defParams, defLocals, binding, &stmt)) {
        defLocals[stmt.name] = binding;
      }
      continue;
    }
    if (isReturnCall(stmt)) {
      if (stmt.args.size() != 1) {
        return false;
      }
      valueExpr = &stmt.args.front();
      sawReturn = true;
      continue;
    }
    if (!sawReturn) {
      valueExpr = &stmt;
    }
  }
  if (def.returnExpr.has_value()) {
    valueExpr = &*def.returnExpr;
  }
  if (valueExpr == nullptr) {
    return false;
  }
  if (valueExpr->kind == Expr::Kind::Name) {
    if (const BindingInfo *paramBinding = findDefParamBinding(defParams, valueExpr->name)) {
      bindingOut = *paramBinding;
      return true;
    }
    auto localIt = defLocals.find(valueExpr->name);
    if (localIt != defLocals.end()) {
      bindingOut = localIt->second;
      return true;
    }
  }
  return inferBindingTypeFromInitializer(*valueExpr, defParams, defLocals, bindingOut);
}

bool SemanticsValidator::inferQueryExprTypeText(const Expr &expr,
                                                const std::vector<ParameterInfo> &params,
                                                const std::unordered_map<std::string, BindingInfo> &locals,
                                                std::string &typeTextOut) {
  auto resolveBindingTypeText = [&](const std::string &name, std::string &resolvedTypeTextOut) -> bool {
    resolvedTypeTextOut.clear();
    if (const BindingInfo *paramBinding = findParamBinding(params, name)) {
      resolvedTypeTextOut = bindingTypeText(*paramBinding);
      return !resolvedTypeTextOut.empty();
    }
    auto localIt = locals.find(name);
    if (localIt == locals.end()) {
      return false;
    }
    resolvedTypeTextOut = bindingTypeText(localIt->second);
    return !resolvedTypeTextOut.empty();
  };
  auto isEnvelopeValueExpr = [&](const Expr &candidate, bool allowAnyName) -> bool {
    if (candidate.kind != Expr::Kind::Call || candidate.isBinding || candidate.isMethodCall) {
      return false;
    }
    if (!candidate.args.empty() || !candidate.templateArgs.empty() || hasNamedArguments(candidate.argNames)) {
      return false;
    }
    if (!candidate.hasBodyArguments && candidate.bodyArguments.empty()) {
      return false;
    }
    return allowAnyName || isBuiltinBlockCall(candidate);
  };
  auto getEnvelopeValueExpr = [&](const Expr &candidate, bool allowAnyName) -> const Expr * {
    if (!isEnvelopeValueExpr(candidate, allowAnyName)) {
      return nullptr;
    }
    const Expr *valueExpr = nullptr;
    for (const auto &bodyExpr : candidate.bodyArguments) {
      if (bodyExpr.isBinding) {
        continue;
      }
      valueExpr = &bodyExpr;
    }
    return valueExpr;
  };

  std::unordered_set<std::string> inferredDefinitionTypeStack;
  std::function<bool(const Expr &, std::string &)> inferExprTypeText;
  inferExprTypeText = [&](const Expr &candidate, std::string &currentTypeTextOut) -> bool {
    currentTypeTextOut.clear();
    if (candidate.kind == Expr::Kind::Name) {
      return resolveBindingTypeText(candidate.name, currentTypeTextOut);
    }
    if (candidate.kind == Expr::Kind::Literal) {
      currentTypeTextOut = candidate.isUnsigned ? "u64" : (candidate.intWidth == 64 ? "i64" : "i32");
      return true;
    }
    if (candidate.kind == Expr::Kind::BoolLiteral) {
      currentTypeTextOut = "bool";
      return true;
    }
    if (candidate.kind == Expr::Kind::FloatLiteral) {
      currentTypeTextOut = candidate.floatWidth == 64 ? "f64" : "f32";
      return true;
    }
    if (candidate.kind == Expr::Kind::StringLiteral) {
      currentTypeTextOut = "string";
      return true;
    }
    if (isIfCall(candidate) && candidate.args.size() == 3) {
      const Expr &thenArg = candidate.args[1];
      const Expr &elseArg = candidate.args[2];
      const Expr *thenValue = getEnvelopeValueExpr(thenArg, true);
      const Expr *elseValue = getEnvelopeValueExpr(elseArg, true);
      std::string thenTypeText;
      std::string elseTypeText;
      if (!inferExprTypeText(thenValue ? *thenValue : thenArg, thenTypeText) ||
          !inferExprTypeText(elseValue ? *elseValue : elseArg, elseTypeText)) {
        return false;
      }
      if (normalizeBindingTypeName(thenTypeText) != normalizeBindingTypeName(elseTypeText)) {
        return false;
      }
      currentTypeTextOut = thenTypeText;
      return true;
    }
    if (const Expr *valueExpr = getEnvelopeValueExpr(candidate, false)) {
      if (isReturnCall(*valueExpr) && !valueExpr->args.empty()) {
        return inferExprTypeText(valueExpr->args.front(), currentTypeTextOut);
      }
      return inferExprTypeText(*valueExpr, currentTypeTextOut);
    }
    if (candidate.kind != Expr::Kind::Call) {
      return false;
    }
    if (isSimpleCallName(candidate, "dereference") && candidate.args.size() == 1) {
      std::string wrappedTypeText;
      if (!inferExprTypeText(candidate.args.front(), wrappedTypeText)) {
        return false;
      }
      currentTypeTextOut = unwrapReferencePointerTypeText(wrappedTypeText);
      return !currentTypeTextOut.empty();
    }

    std::string collection;
    if (getBuiltinCollectionName(candidate, collection)) {
      if ((collection == "array" || collection == "vector" || collection == "soa_vector") &&
          candidate.templateArgs.size() == 1) {
        currentTypeTextOut = collection + "<" + candidate.templateArgs.front() + ">";
        return true;
      }
      if (collection == "map" && candidate.templateArgs.size() == 2) {
        currentTypeTextOut = "map<" + candidate.templateArgs[0] + ", " + candidate.templateArgs[1] + ">";
        return true;
      }
    }

    const std::string resolvedCandidate = resolveCalleePath(candidate);
    if (isDirectMapConstructorPath(resolvedCandidate)) {
      if (candidate.templateArgs.size() == 2) {
        currentTypeTextOut = "map<" + candidate.templateArgs[0] + ", " + candidate.templateArgs[1] + ">";
        return true;
      }
      if (candidate.args.empty() || candidate.args.size() % 2 != 0) {
        return false;
      }
      std::string keyTypeText;
      std::string valueTypeText;
      for (size_t i = 0; i < candidate.args.size(); i += 2) {
        std::string currentKeyTypeText;
        std::string currentValueTypeText;
        if (!inferExprTypeText(candidate.args[i], currentKeyTypeText) ||
            !inferExprTypeText(candidate.args[i + 1], currentValueTypeText)) {
          return false;
        }
        if (keyTypeText.empty()) {
          keyTypeText = currentKeyTypeText;
        } else if (normalizeBindingTypeName(keyTypeText) != normalizeBindingTypeName(currentKeyTypeText)) {
          return false;
        }
        if (valueTypeText.empty()) {
          valueTypeText = currentValueTypeText;
        } else if (normalizeBindingTypeName(valueTypeText) != normalizeBindingTypeName(currentValueTypeText)) {
          return false;
        }
      }
      if (keyTypeText.empty() || valueTypeText.empty()) {
        return false;
      }
      currentTypeTextOut = "map<" + keyTypeText + ", " + valueTypeText + ">";
      return true;
    }

    auto defIt = defMap_.find(resolvedCandidate);
    if (defIt == defMap_.end() || defIt->second == nullptr) {
      return false;
    }
    for (const auto &transform : defIt->second->transforms) {
      if (transform.name != "return" || transform.templateArgs.size() != 1) {
        continue;
      }
      if (transform.templateArgs.front() == "auto") {
        break;
      }
      currentTypeTextOut = transform.templateArgs.front();
      return !currentTypeTextOut.empty();
    }
    if (!inferredDefinitionTypeStack.insert(resolvedCandidate).second) {
      return false;
    }
    const auto stackIt = inferredDefinitionTypeStack.find(resolvedCandidate);
    struct ScopeGuard {
      std::unordered_set<std::string> &stack;
      std::unordered_set<std::string>::const_iterator it;
      ~ScopeGuard() {
        stack.erase(it);
      }
    } guard{inferredDefinitionTypeStack, stackIt};
    BindingInfo inferredReturn;
    if (!inferDefinitionReturnBinding(*defIt->second, inferredReturn)) {
      return false;
    }
    currentTypeTextOut = bindingTypeText(inferredReturn);
    return !currentTypeTextOut.empty();
  };

  return inferExprTypeText(expr, typeTextOut);
}

bool SemanticsValidator::resolveCallCollectionTypePath(const Expr &target,
                                                       const std::vector<ParameterInfo> &params,
                                                       const std::unordered_map<std::string, BindingInfo> &locals,
                                                       std::string &typePathOut) {
  typePathOut.clear();
  auto inferCollectionTypePathFromType =
      [&](const std::string &typeName, auto &&inferCollectionTypePathFromTypeRef) -> std::string {
    const std::string normalizedType = normalizeBindingTypeName(typeName);
    std::string normalized = normalizeCollectionTypePath(normalizedType);
    if (!normalized.empty()) {
      return normalized;
    }
    std::string base;
    std::string arg;
    if (!splitTemplateTypeName(normalizedType, base, arg)) {
      return {};
    }
    base = normalizeBindingTypeName(base);
    if (base == "Reference" || base == "Pointer") {
      std::vector<std::string> args;
      if (!splitTopLevelTemplateArgs(arg, args) || args.size() != 1) {
        return {};
      }
      return inferCollectionTypePathFromTypeRef(args.front(), inferCollectionTypePathFromTypeRef);
    }
    std::vector<std::string> args;
    if (!splitTopLevelTemplateArgs(arg, args)) {
      return {};
    }
    if ((base == "array" || base == "vector" || base == "soa_vector") && args.size() == 1) {
      return "/" + base;
    }
    if (isMapCollectionTypeName(base) && args.size() == 2) {
      return "/map";
    }
    return {};
  };

  if (target.kind != Expr::Kind::Call) {
    return false;
  }

  std::string targetTypeText;
  if (inferQueryExprTypeText(target, params, locals, targetTypeText)) {
    const std::string inferred = inferCollectionTypePathFromType(targetTypeText, inferCollectionTypePathFromType);
    if (!inferred.empty()) {
      typePathOut = inferred;
      return true;
    }
  }

  const std::string resolvedTarget = resolveCalleePath(target);
  auto structIt = returnStructs_.find(resolvedTarget);
  if (structIt != returnStructs_.end()) {
    std::string normalized = normalizeCollectionTypePath(structIt->second);
    if (!normalized.empty()) {
      typePathOut = normalized;
      return true;
    }
  }
  auto kindIt = returnKinds_.find(resolvedTarget);
  if (kindIt != returnKinds_.end()) {
    if (kindIt->second == ReturnKind::Array) {
      typePathOut = "/array";
      return true;
    }
    if (kindIt->second == ReturnKind::String) {
      typePathOut = "/string";
      return true;
    }
  }
  return false;
}

bool SemanticsValidator::resolveCallCollectionTemplateArgs(const Expr &target,
                                                           const std::string &expectedBase,
                                                           const std::vector<ParameterInfo> &params,
                                                           const std::unordered_map<std::string, BindingInfo> &locals,
                                                           std::vector<std::string> &argsOut) {
  argsOut.clear();
  auto extractCollectionArgsFromType =
      [&](const std::string &typeName, auto &&extractCollectionArgsFromTypeRef) -> bool {
    std::string base;
    std::string arg;
    const std::string normalizedType = normalizeBindingTypeName(typeName);
    if (!splitTemplateTypeName(normalizedType, base, arg)) {
      return false;
    }
    base = normalizeBindingTypeName(base);
    if (base == expectedBase || (expectedBase == "map" && isMapCollectionTypeName(base))) {
      return splitTopLevelTemplateArgs(arg, argsOut);
    }
    std::vector<std::string> args;
    if ((base == "Reference" || base == "Pointer") && splitTopLevelTemplateArgs(arg, args) && args.size() == 1) {
      return extractCollectionArgsFromTypeRef(args.front(), extractCollectionArgsFromTypeRef);
    }
    return false;
  };

  if (target.kind != Expr::Kind::Call) {
    return false;
  }

  std::string targetTypeText;
  return inferQueryExprTypeText(target, params, locals, targetTypeText) &&
         extractCollectionArgsFromType(targetTypeText, extractCollectionArgsFromType);
}

} // namespace primec::semantics
