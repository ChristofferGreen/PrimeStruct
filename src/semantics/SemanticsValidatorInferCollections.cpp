#include "SemanticsValidator.h"

#include <optional>
#include <string_view>
#include <utility>

namespace primec::semantics {
namespace {

std::string bindingTypeText(const BindingInfo &binding) {
  if (binding.typeTemplateArg.empty()) {
    return binding.typeName;
  }
  return binding.typeName + "<" + binding.typeTemplateArg + ">";
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
  auto inferTargetTypeText = [&](const Expr &candidate, std::string &typeTextOut) -> bool {
    typeTextOut.clear();
    if (candidate.kind == Expr::Kind::Name) {
      if (const BindingInfo *paramBinding = findParamBinding(params, candidate.name)) {
        typeTextOut = bindingTypeText(*paramBinding);
        return !typeTextOut.empty();
      }
      auto it = locals.find(candidate.name);
      if (it != locals.end()) {
        typeTextOut = bindingTypeText(it->second);
        return !typeTextOut.empty();
      }
      return false;
    }
    if (candidate.kind == Expr::Kind::Literal) {
      typeTextOut = candidate.isUnsigned ? "u64" : (candidate.intWidth == 64 ? "i64" : "i32");
      return true;
    }
    if (candidate.kind == Expr::Kind::BoolLiteral) {
      typeTextOut = "bool";
      return true;
    }
    if (candidate.kind == Expr::Kind::FloatLiteral) {
      typeTextOut = candidate.floatWidth == 64 ? "f64" : "f32";
      return true;
    }
    if (candidate.kind == Expr::Kind::StringLiteral) {
      typeTextOut = "string";
      return true;
    }
    if (candidate.kind != Expr::Kind::Call) {
      return false;
    }
    const std::string resolvedCandidate = resolveCalleePath(candidate);
    auto defIt = defMap_.find(resolvedCandidate);
    if (defIt == defMap_.end() || defIt->second == nullptr) {
      return false;
    }
    for (const auto &transform : defIt->second->transforms) {
      if (transform.name == "return" && transform.templateArgs.size() == 1) {
        typeTextOut = transform.templateArgs.front();
        return true;
      }
    }
    BindingInfo inferredReturn;
    if (inferDefinitionReturnBinding(*defIt->second, inferredReturn)) {
      typeTextOut = bindingTypeText(inferredReturn);
      return !typeTextOut.empty();
    }
    return false;
  };
  auto inferDirectMapConstructorTemplateArgs = [&](const Expr &candidate,
                                                   std::vector<std::string> &argsOut) -> bool {
    argsOut.clear();
    if (candidate.kind != Expr::Kind::Call || candidate.isBinding || candidate.isMethodCall) {
      return false;
    }
    const std::string resolvedCandidate = resolveCalleePath(candidate);
    auto matchesDirectMapConstructorPath = [&](std::string_view basePath) {
      return resolvedCandidate == basePath || resolvedCandidate.rfind(std::string(basePath) + "__t", 0) == 0;
    };
    const bool isDirectMapConstructor =
        matchesDirectMapConstructorPath("/std/collections/map/map") ||
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
    if (!isDirectMapConstructor) {
      return false;
    }
    if (candidate.templateArgs.size() == 2) {
      argsOut = candidate.templateArgs;
      return true;
    }
    if (candidate.args.empty() || candidate.args.size() % 2 != 0) {
      return false;
    }
    std::string keyType;
    std::string valueType;
    for (size_t i = 0; i < candidate.args.size(); i += 2) {
      std::string currentKeyType;
      std::string currentValueType;
      if (!inferTargetTypeText(candidate.args[i], currentKeyType) ||
          !inferTargetTypeText(candidate.args[i + 1], currentValueType)) {
        return false;
      }
      if (keyType.empty()) {
        keyType = currentKeyType;
      } else if (normalizeBindingTypeName(keyType) != normalizeBindingTypeName(currentKeyType)) {
        return false;
      }
      if (valueType.empty()) {
        valueType = currentValueType;
      } else if (normalizeBindingTypeName(valueType) != normalizeBindingTypeName(currentValueType)) {
        return false;
      }
    }
    if (keyType.empty() || valueType.empty()) {
      return false;
    }
    argsOut = {keyType, valueType};
    return true;
  };

  if (target.kind != Expr::Kind::Call) {
    return false;
  }
  std::vector<std::string> directMapArgs;
  if (inferDirectMapConstructorTemplateArgs(target, directMapArgs)) {
    typePathOut = "/map";
    return true;
  }
  const std::string resolvedTarget = resolveCalleePath(target);
  std::string collection;
  if (defMap_.find(resolvedTarget) == defMap_.end() && getBuiltinCollectionName(target, collection)) {
    if ((collection == "array" || collection == "vector" || collection == "soa_vector") &&
        target.templateArgs.size() == 1) {
      typePathOut = "/" + collection;
      return true;
    }
    if (collection == "map" && target.templateArgs.size() == 2) {
      typePathOut = "/map";
      return true;
    }
  }
  auto defIt = defMap_.find(resolvedTarget);
  if (defIt != defMap_.end() && defIt->second != nullptr) {
    for (const auto &transform : defIt->second->transforms) {
      if (transform.name != "return" || transform.templateArgs.size() != 1) {
        continue;
      }
      const std::string inferred =
          inferCollectionTypePathFromType(transform.templateArgs.front(), inferCollectionTypePathFromType);
      if (!inferred.empty()) {
        typePathOut = inferred;
        return true;
      }
      break;
    }
    BindingInfo inferredReturn;
    if (inferDefinitionReturnBinding(*defIt->second, inferredReturn)) {
      const std::string inferred =
          inferCollectionTypePathFromType(bindingTypeText(inferredReturn), inferCollectionTypePathFromType);
      if (!inferred.empty()) {
        typePathOut = inferred;
        return true;
      }
    }
  }
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
  auto inferTargetTypeText = [&](const Expr &candidate, std::string &typeTextOut) -> bool {
    typeTextOut.clear();
    if (candidate.kind == Expr::Kind::Name) {
      if (const BindingInfo *paramBinding = findParamBinding(params, candidate.name)) {
        typeTextOut = bindingTypeText(*paramBinding);
        return !typeTextOut.empty();
      }
      auto it = locals.find(candidate.name);
      if (it != locals.end()) {
        typeTextOut = bindingTypeText(it->second);
        return !typeTextOut.empty();
      }
      return false;
    }
    if (candidate.kind == Expr::Kind::Literal) {
      typeTextOut = candidate.isUnsigned ? "u64" : (candidate.intWidth == 64 ? "i64" : "i32");
      return true;
    }
    if (candidate.kind == Expr::Kind::BoolLiteral) {
      typeTextOut = "bool";
      return true;
    }
    if (candidate.kind == Expr::Kind::FloatLiteral) {
      typeTextOut = candidate.floatWidth == 64 ? "f64" : "f32";
      return true;
    }
    if (candidate.kind == Expr::Kind::StringLiteral) {
      typeTextOut = "string";
      return true;
    }
    if (candidate.kind != Expr::Kind::Call) {
      return false;
    }
    const std::string resolvedCandidate = resolveCalleePath(candidate);
    auto defIt = defMap_.find(resolvedCandidate);
    if (defIt == defMap_.end() || defIt->second == nullptr) {
      return false;
    }
    for (const auto &transform : defIt->second->transforms) {
      if (transform.name == "return" && transform.templateArgs.size() == 1) {
        typeTextOut = transform.templateArgs.front();
        return true;
      }
    }
    BindingInfo inferredReturn;
    if (inferDefinitionReturnBinding(*defIt->second, inferredReturn)) {
      typeTextOut = bindingTypeText(inferredReturn);
      return !typeTextOut.empty();
    }
    return false;
  };
  auto inferDirectMapConstructorTemplateArgs = [&](const Expr &candidate) -> bool {
    if (expectedBase != "map" || candidate.kind != Expr::Kind::Call || candidate.isBinding || candidate.isMethodCall) {
      return false;
    }
    const std::string resolvedCandidate = resolveCalleePath(candidate);
    auto matchesDirectMapConstructorPath = [&](std::string_view basePath) {
      return resolvedCandidate == basePath || resolvedCandidate.rfind(std::string(basePath) + "__t", 0) == 0;
    };
    const bool isDirectMapConstructor =
        matchesDirectMapConstructorPath("/std/collections/map/map") ||
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
    if (!isDirectMapConstructor) {
      return false;
    }
    if (candidate.templateArgs.size() == 2) {
      argsOut = candidate.templateArgs;
      return true;
    }
    if (candidate.args.empty() || candidate.args.size() % 2 != 0) {
      return false;
    }
    std::string keyType;
    std::string valueType;
    for (size_t i = 0; i < candidate.args.size(); i += 2) {
      std::string currentKeyType;
      std::string currentValueType;
      if (!inferTargetTypeText(candidate.args[i], currentKeyType) ||
          !inferTargetTypeText(candidate.args[i + 1], currentValueType)) {
        return false;
      }
      if (keyType.empty()) {
        keyType = currentKeyType;
      } else if (normalizeBindingTypeName(keyType) != normalizeBindingTypeName(currentKeyType)) {
        return false;
      }
      if (valueType.empty()) {
        valueType = currentValueType;
      } else if (normalizeBindingTypeName(valueType) != normalizeBindingTypeName(currentValueType)) {
        return false;
      }
    }
    if (keyType.empty() || valueType.empty()) {
      return false;
    }
    argsOut = {keyType, valueType};
    return true;
  };

  if (target.kind != Expr::Kind::Call) {
    return false;
  }
  if (inferDirectMapConstructorTemplateArgs(target)) {
    return true;
  }
  const std::string resolvedTarget = resolveCalleePath(target);
  std::string collection;
  if (defMap_.find(resolvedTarget) == defMap_.end() && getBuiltinCollectionName(target, collection) &&
      collection == expectedBase) {
    argsOut = target.templateArgs;
    return true;
  }
  auto defIt = defMap_.find(resolvedTarget);
  if (defIt == defMap_.end() || defIt->second == nullptr) {
    return false;
  }
  for (const auto &transform : defIt->second->transforms) {
    if (transform.name != "return" || transform.templateArgs.size() != 1) {
      continue;
    }
    return extractCollectionArgsFromType(transform.templateArgs.front(), extractCollectionArgsFromType);
  }
  BindingInfo inferredReturn;
  if (inferDefinitionReturnBinding(*defIt->second, inferredReturn)) {
    return extractCollectionArgsFromType(bindingTypeText(inferredReturn), extractCollectionArgsFromType);
  }
  return false;
}

} // namespace primec::semantics
