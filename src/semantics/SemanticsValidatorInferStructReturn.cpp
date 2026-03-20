#include "SemanticsValidator.h"

#include <cctype>
#include <cstdint>
#include <functional>
#include <sstream>

namespace primec::semantics {

std::string SemanticsValidator::inferStructReturnPath(
    const Expr &expr,
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals) {
  auto bindingTypeText = [](const BindingInfo &binding) {
    if (binding.typeTemplateArg.empty()) {
      return binding.typeName;
    }
    return binding.typeName + "<" + binding.typeTemplateArg + ">";
  };
  auto normalizeCollectionPath = [&](std::string typeName, std::string typeTemplateArg) -> std::string {
    if ((typeName == "Reference" || typeName == "Pointer") && !typeTemplateArg.empty()) {
      typeName = normalizeBindingTypeName(typeTemplateArg);
      typeTemplateArg.clear();
    } else {
      typeName = normalizeBindingTypeName(typeName);
    }
    if (typeName == "string") {
      return "/string";
    }
    if ((typeName == "array" || typeName == "vector" || typeName == "soa_vector") && !typeTemplateArg.empty()) {
      return "/" + typeName;
    }
    if (isMapCollectionTypeName(typeName) && !typeTemplateArg.empty()) {
      return "/map";
    }
    std::string base;
    std::string arg;
    if (splitTemplateTypeName(typeName, base, arg)) {
      std::vector<std::string> args;
      if (splitTopLevelTemplateArgs(arg, args)) {
        if ((base == "array" || base == "vector" || base == "soa_vector") && args.size() == 1) {
          return "/" + base;
        }
        if (isMapCollectionTypeName(base) && args.size() == 2) {
          return "/map";
        }
      }
    }
    return "";
  };
  auto resolveStructTypePath = [&](const std::string &typeName,
                                   const std::string &namespacePrefix) -> std::string {
    if (typeName.empty()) {
      return "";
    }
    if (!typeName.empty() && typeName[0] == '/') {
      return structNames_.count(typeName) > 0 ? typeName : "";
    }
    std::string current = namespacePrefix;
    while (true) {
      if (!current.empty()) {
        std::string direct = current + "/" + typeName;
        if (structNames_.count(direct) > 0) {
          return direct;
        }
        if (current.size() > typeName.size()) {
          const size_t start = current.size() - typeName.size();
          if (start > 0 && current[start - 1] == '/' &&
              current.compare(start, typeName.size(), typeName) == 0 &&
              structNames_.count(current) > 0) {
            return current;
          }
        }
      } else {
        std::string root = "/" + typeName;
        if (structNames_.count(root) > 0) {
          return root;
        }
      }
      if (current.empty()) {
        break;
      }
      const size_t slash = current.find_last_of('/');
      if (slash == std::string::npos || slash == 0) {
        current.clear();
      } else {
        current.erase(slash);
      }
    }
    auto importIt = importAliases_.find(typeName);
    if (importIt != importAliases_.end() && structNames_.count(importIt->second) > 0) {
      return importIt->second;
    }
    return "";
  };
  auto isMatrixQuaternionTypePath = [](const std::string &typePath) {
    return typePath == "/std/math/Mat2" || typePath == "/std/math/Mat3" || typePath == "/std/math/Mat4" ||
           typePath == "/std/math/Quat";
  };
  auto matrixDimensionForTypePath = [](const std::string &typePath) -> size_t {
    if (typePath == "/std/math/Mat2") {
      return 2;
    }
    if (typePath == "/std/math/Mat3") {
      return 3;
    }
    if (typePath == "/std/math/Mat4") {
      return 4;
    }
    return 0;
  };
  auto isVectorTypePath = [](const std::string &typePath) {
    return typePath == "/std/math/Vec2" || typePath == "/std/math/Vec3" || typePath == "/std/math/Vec4";
  };
  auto vectorDimensionForTypePath = [](const std::string &typePath) -> size_t {
    if (typePath == "/std/math/Vec2") {
      return 2;
    }
    if (typePath == "/std/math/Vec3") {
      return 3;
    }
    if (typePath == "/std/math/Vec4") {
      return 4;
    }
    return 0;
  };
  auto isNumericScalarExpr = [&](const Expr &arg) -> bool {
    ReturnKind kind = inferExprReturnKind(arg, params, locals);
    const bool isSoftware =
        kind == ReturnKind::Integer || kind == ReturnKind::Decimal || kind == ReturnKind::Complex;
    if (kind == ReturnKind::Int || kind == ReturnKind::Int64 || kind == ReturnKind::UInt64 ||
        kind == ReturnKind::Float32 || kind == ReturnKind::Float64 || isSoftware) {
      return true;
    }
    if (kind == ReturnKind::Bool || kind == ReturnKind::Void || kind == ReturnKind::Array) {
      return false;
    }
    if (kind == ReturnKind::Unknown) {
      if (arg.kind == Expr::Kind::StringLiteral || arg.kind == Expr::Kind::BoolLiteral) {
        return false;
      }
      if (!inferStructReturnPath(arg, params, locals).empty()) {
        return false;
      }
      return true;
    }
    return false;
  };
  auto normalizeCollectionTypePath = [&](const std::string &typePath) {
    std::string normalizedTypePath = normalizeBindingTypeName(typePath);
    if (!normalizedTypePath.empty() && normalizedTypePath.front() == '/') {
      normalizedTypePath.erase(normalizedTypePath.begin());
    }
    if (normalizedTypePath.rfind("std/collections/experimental_map/Map__", 0) == 0) {
      return std::string("/map");
    }
    if (typePath == "/array" || typePath == "array") {
      return std::string("/array");
    }
    if (typePath == "/vector" || typePath == "vector" || typePath == "/std/collections/vector" ||
        typePath == "std/collections/vector") {
      return std::string("/vector");
    }
    if (typePath == "/soa_vector" || typePath == "soa_vector") {
      return std::string("/soa_vector");
    }
    if (isMapCollectionTypeName(typePath) || typePath == "/map" || typePath == "/std/collections/map") {
      return std::string("/map");
    }
    if (typePath == "/string" || typePath == "string") {
      return std::string("/string");
    }
    return std::string();
  };
  std::function<std::string(const Expr &)> resolvePointerTargetTypeText;
  resolvePointerTargetTypeText = [&](const Expr &pointerExpr) -> std::string {
    auto resolveBindingTargetTypeText = [&](const Expr &target) -> std::string {
      if (target.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
          return bindingTypeText(*paramBinding);
        }
        auto it = locals.find(target.name);
        if (it != locals.end()) {
          return bindingTypeText(it->second);
        }
        return {};
      }
      if (!(target.kind == Expr::Kind::Call && target.isFieldAccess && target.args.size() == 1)) {
        return {};
      }
      std::string receiverStruct = inferStructReturnPath(target.args.front(), params, locals);
      if (receiverStruct.empty() || structNames_.count(receiverStruct) == 0) {
        return {};
      }
      auto defIt = defMap_.find(receiverStruct);
      if (defIt == defMap_.end() || defIt->second == nullptr) {
        return {};
      }
      for (const auto &stmt : defIt->second->statements) {
        if (!stmt.isBinding || stmt.name != target.name) {
          continue;
        }
        BindingInfo fieldBinding;
        if (!resolveStructFieldBinding(*defIt->second, stmt, fieldBinding)) {
          return {};
        }
        return bindingTypeText(fieldBinding);
      }
      return {};
    };
    if (pointerExpr.kind == Expr::Kind::Name) {
      if (const BindingInfo *paramBinding = findParamBinding(params, pointerExpr.name)) {
        if ((paramBinding->typeName == "Reference" || paramBinding->typeName == "Pointer") &&
            !paramBinding->typeTemplateArg.empty()) {
          return paramBinding->typeTemplateArg;
        }
        return {};
      }
      auto it = locals.find(pointerExpr.name);
      if (it != locals.end() &&
          (it->second.typeName == "Reference" || it->second.typeName == "Pointer") &&
          !it->second.typeTemplateArg.empty()) {
        return it->second.typeTemplateArg;
      }
      return {};
    }
    if (pointerExpr.kind != Expr::Kind::Call) {
      return {};
    }
    std::string pointerBuiltin;
    if (getBuiltinPointerName(pointerExpr, pointerBuiltin) && pointerExpr.args.size() == 1 &&
        pointerBuiltin == "location") {
      return resolveBindingTargetTypeText(pointerExpr.args.front());
    }
    auto defIt = defMap_.find(resolveCalleePath(pointerExpr));
    if (defIt == defMap_.end() || defIt->second == nullptr) {
      return {};
    }
    for (const auto &transform : defIt->second->transforms) {
      if (transform.name != "return" || transform.templateArgs.size() != 1) {
        continue;
      }
      std::string base;
      std::string argText;
      const std::string normalizedReturnType = normalizeBindingTypeName(transform.templateArgs.front());
      if (!splitTemplateTypeName(normalizedReturnType, base, argText) || argText.empty()) {
        return {};
      }
      base = normalizeBindingTypeName(base);
      if (base == "Reference" || base == "Pointer") {
        return argText;
      }
      return {};
    }
    return {};
  };
  if (expr.kind == Expr::Kind::Call) {
    std::string pointerBuiltin;
    if (getBuiltinPointerName(expr, pointerBuiltin) && pointerBuiltin == "dereference" && expr.args.size() == 1) {
      const std::string pointeeType = resolvePointerTargetTypeText(expr.args.front());
      if (!pointeeType.empty()) {
        const std::string collectionPath = normalizeCollectionTypePath(pointeeType);
        if (!collectionPath.empty()) {
          return collectionPath;
        }
        std::string unwrappedType = unwrapReferencePointerTypeText(pointeeType);
        std::string structPath = resolveStructTypePath(unwrappedType, expr.namespacePrefix);
        if (!structPath.empty()) {
          return structPath;
        }
      }
    }
  }
  auto resolveCallCollectionTypePath = [&](const Expr &target, std::string &typePathOut) -> bool {
    return SemanticsValidator::resolveCallCollectionTypePath(target, params, locals, typePathOut);
  };
  auto resolveCallCollectionTemplateArgs =
      [&](const Expr &target, const std::string &expectedBase, std::vector<std::string> &argsOut) -> bool {
    return SemanticsValidator::resolveCallCollectionTemplateArgs(target, expectedBase, params, locals, argsOut);
  };
  const BuiltinCollectionDispatchResolverAdapters builtinCollectionDispatchResolverAdapters;
  const BuiltinCollectionDispatchResolvers builtinCollectionDispatchResolvers =
      makeBuiltinCollectionDispatchResolvers(params, locals, builtinCollectionDispatchResolverAdapters);
  const auto &resolveArgsPackAccessTarget = builtinCollectionDispatchResolvers.resolveArgsPackAccessTarget;
  const auto &resolveArrayTarget = builtinCollectionDispatchResolvers.resolveArrayTarget;
  const auto &resolveVectorTarget = builtinCollectionDispatchResolvers.resolveVectorTarget;
  const auto &resolveStringTarget = builtinCollectionDispatchResolvers.resolveStringTarget;
  const auto &resolveMapTarget = builtinCollectionDispatchResolvers.resolveMapTarget;
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
  auto collectionHelperPathCandidates = [&](const std::string &path) {
    std::vector<std::string> candidates;
    auto appendUnique = [&](const std::string &candidate) {
      for (const auto &existing : candidates) {
        if (existing == candidate) {
          return;
        }
      }
      candidates.push_back(candidate);
    };

    std::string normalizedPath = path;
    if (!normalizedPath.empty() && normalizedPath.front() != '/') {
      if (normalizedPath.rfind("array/", 0) == 0 || normalizedPath.rfind("vector/", 0) == 0 ||
          normalizedPath.rfind("std/collections/vector/", 0) == 0 || normalizedPath.rfind("map/", 0) == 0 ||
          normalizedPath.rfind("std/collections/map/", 0) == 0) {
        normalizedPath.insert(normalizedPath.begin(), '/');
      }
    }

    appendUnique(path);
    appendUnique(normalizedPath);
    if (normalizedPath.rfind("/array/", 0) == 0) {
      const std::string suffix = normalizedPath.substr(std::string("/array/").size());
      if (suffix != "count" && suffix != "capacity" && suffix != "at" && suffix != "at_unsafe" &&
          suffix != "push" && suffix != "pop" && suffix != "reserve" && suffix != "clear" &&
          suffix != "remove_at" && suffix != "remove_swap") {
        appendUnique("/vector/" + suffix);
        appendUnique("/std/collections/vector/" + suffix);
      }
    } else if (normalizedPath.rfind("/vector/", 0) == 0) {
      const std::string suffix = normalizedPath.substr(std::string("/vector/").size());
      if (suffix != "count" && suffix != "capacity" && suffix != "at" && suffix != "at_unsafe" &&
          suffix != "push" && suffix != "pop" && suffix != "reserve" && suffix != "clear" &&
          suffix != "remove_at" && suffix != "remove_swap") {
        appendUnique("/std/collections/vector/" + suffix);
      }
      if (suffix != "count" && suffix != "capacity" && suffix != "at" && suffix != "at_unsafe" &&
          suffix != "push" && suffix != "pop" && suffix != "reserve" && suffix != "clear" &&
          suffix != "remove_at" && suffix != "remove_swap") {
        appendUnique("/array/" + suffix);
      }
    } else if (normalizedPath.rfind("/std/collections/vector/", 0) == 0) {
      const std::string suffix = normalizedPath.substr(std::string("/std/collections/vector/").size());
      appendUnique("/vector/" + suffix);
      if (suffix != "count" && suffix != "capacity" && suffix != "at" && suffix != "at_unsafe" &&
          suffix != "push" && suffix != "pop" && suffix != "reserve" && suffix != "clear" &&
          suffix != "remove_at" && suffix != "remove_swap") {
        appendUnique("/array/" + suffix);
      }
    } else if (normalizedPath.rfind("/map/", 0) == 0) {
      const std::string suffix = normalizedPath.substr(std::string("/map/").size());
      if (suffix != "count" && suffix != "contains" && suffix != "tryAt") {
        appendUnique("/std/collections/map/" + suffix);
      }
    } else if (normalizedPath.rfind("/std/collections/map/", 0) == 0) {
      const std::string suffix = normalizedPath.substr(std::string("/std/collections/map/").size());
      if (suffix != "map") {
        appendUnique("/map/" + suffix);
      }
    }
    return candidates;
  };
  auto pruneMapAccessStructReturnCompatibilityCandidates = [&](const std::string &path,
                                                               std::vector<std::string> &candidates) {
    std::string normalizedPath = path;
    if (!normalizedPath.empty() && normalizedPath.front() != '/') {
      if (normalizedPath.rfind("map/", 0) == 0 || normalizedPath.rfind("std/collections/map/", 0) == 0) {
        normalizedPath.insert(normalizedPath.begin(), '/');
      }
    }
    auto eraseCandidate = [&](const std::string &candidate) {
      for (auto it = candidates.begin(); it != candidates.end();) {
        if (*it == candidate) {
          it = candidates.erase(it);
        } else {
          ++it;
        }
      }
    };
    if (normalizedPath.rfind("/map/", 0) == 0) {
      const std::string suffix = normalizedPath.substr(std::string("/map/").size());
      if (suffix == "at" || suffix == "at_unsafe") {
        eraseCandidate("/std/collections/map/" + suffix);
      }
    } else if (normalizedPath.rfind("/std/collections/map/", 0) == 0) {
      const std::string suffix = normalizedPath.substr(std::string("/std/collections/map/").size());
      if (suffix == "at" || suffix == "at_unsafe") {
        eraseCandidate("/map/" + suffix);
      }
    }
  };
  auto pruneBuiltinVectorAccessStructReturnCandidates = [&](const Expr &candidate,
                                                            const std::string &path,
                                                            std::vector<std::string> &candidates) {
    std::string normalizedPath = path;
    if (!normalizedPath.empty() && normalizedPath.front() != '/') {
      if (normalizedPath.rfind("vector/", 0) == 0 || normalizedPath.rfind("std/collections/vector/", 0) == 0) {
        normalizedPath.insert(normalizedPath.begin(), '/');
      }
    }
    if (normalizedPath.rfind("/std/collections/vector/", 0) != 0) {
      return;
    }
    const std::string suffix = normalizedPath.substr(std::string("/std/collections/vector/").size());
    if (suffix != "at" && suffix != "at_unsafe") {
      return;
    }
    if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall || candidate.args.empty()) {
      return;
    }
    size_t receiverIndex = 0;
    if (hasNamedArguments(candidate.argNames)) {
      bool foundValues = false;
      for (size_t i = 0; i < candidate.args.size(); ++i) {
        if (i < candidate.argNames.size() && candidate.argNames[i].has_value() &&
            *candidate.argNames[i] == "values") {
          receiverIndex = i;
          foundValues = true;
          break;
        }
      }
      if (!foundValues) {
        receiverIndex = 0;
      }
    }
    if (receiverIndex >= candidate.args.size()) {
      return;
    }
    std::string elemType;
    if (!resolveVectorTarget(candidate.args[receiverIndex], elemType) &&
        !resolveArrayTarget(candidate.args[receiverIndex], elemType) &&
        !resolveStringTarget(candidate.args[receiverIndex])) {
      return;
    }
    const std::string canonicalCandidate = "/std/collections/vector/" + suffix;
    for (auto it = candidates.begin(); it != candidates.end();) {
      if (*it == canonicalCandidate) {
        it = candidates.erase(it);
      } else {
        ++it;
      }
    }
  };
  auto isExplicitMapAccessStructReturnCompatibilityCall = [&](const Expr &candidate) {
    if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall || candidate.name.empty()) {
      return false;
    }
    std::string normalized = candidate.name;
    if (!normalized.empty() && normalized.front() == '/') {
      normalized.erase(normalized.begin());
    }
    std::string helperName;
    if (normalized == "map/at") {
      helperName = "at";
    } else if (normalized == "map/at_unsafe") {
      helperName = "at_unsafe";
    } else {
      return false;
    }
    if (hasDefinitionPath("/map/" + helperName)) {
      return false;
    }
    if (candidate.args.empty()) {
      return false;
    }
    size_t receiverIndex = 0;
    if (hasNamedArguments(candidate.argNames)) {
      bool foundValues = false;
      for (size_t i = 0; i < candidate.args.size(); ++i) {
        if (i < candidate.argNames.size() && candidate.argNames[i].has_value() &&
            *candidate.argNames[i] == "values") {
          receiverIndex = i;
          foundValues = true;
          break;
        }
      }
      if (!foundValues) {
        receiverIndex = 0;
      }
    }
    if (receiverIndex >= candidate.args.size()) {
      return false;
    }
    std::string keyType;
    std::string valueType;
    return resolveMapTarget(candidate.args[receiverIndex], keyType, valueType);
  };

  if (expr.isLambda) {
    return "";
  }

  if (expr.kind == Expr::Kind::Name) {
    if (const BindingInfo *paramBinding = findParamBinding(params, expr.name)) {
      if (std::string collectionPath =
              normalizeCollectionPath(paramBinding->typeName, paramBinding->typeTemplateArg);
          !collectionPath.empty()) {
        return collectionPath;
      }
      if (paramBinding->typeName != "Reference" && paramBinding->typeName != "Pointer") {
        return resolveStructTypePath(paramBinding->typeName, expr.namespacePrefix);
      }
      return "";
    }
    auto it = locals.find(expr.name);
    if (it != locals.end()) {
      if (std::string collectionPath = normalizeCollectionPath(it->second.typeName, it->second.typeTemplateArg);
          !collectionPath.empty()) {
        return collectionPath;
      }
      if (it->second.typeName != "Reference" && it->second.typeName != "Pointer") {
        return resolveStructTypePath(it->second.typeName, expr.namespacePrefix);
      }
    }
    return "";
  }

  if (expr.kind == Expr::Kind::Call) {
    std::string builtinName;
    if (getBuiltinOperatorName(expr, builtinName) && (builtinName == "plus" || builtinName == "minus") &&
        expr.args.size() == 2) {
      const std::string leftType = inferStructReturnPath(expr.args[0], params, locals);
      const std::string rightType = inferStructReturnPath(expr.args[1], params, locals);
      if (isMatrixQuaternionTypePath(leftType) && leftType == rightType) {
        return leftType;
      }
    }
    if (getBuiltinOperatorName(expr, builtinName) && builtinName == "multiply" && expr.args.size() == 2) {
      const std::string leftType = inferStructReturnPath(expr.args[0], params, locals);
      const std::string rightType = inferStructReturnPath(expr.args[1], params, locals);
      if (isMatrixQuaternionTypePath(leftType)) {
        if (leftType == "/std/math/Quat") {
          if (rightType == "/std/math/Quat") {
            return leftType;
          }
          if (rightType == "/std/math/Vec3") {
            return rightType;
          }
          if (isNumericScalarExpr(expr.args[1])) {
            return leftType;
          }
          return "";
        }
        if (isMatrixQuaternionTypePath(rightType) && leftType == rightType) {
          return leftType;
        }
        if (isVectorTypePath(rightType) &&
            matrixDimensionForTypePath(leftType) == vectorDimensionForTypePath(rightType)) {
          return rightType;
        }
        if (isNumericScalarExpr(expr.args[1])) {
          return leftType;
        }
        return "";
      }
      if (isMatrixQuaternionTypePath(rightType)) {
        if (isNumericScalarExpr(expr.args[0])) {
          return rightType;
        }
        return "";
      }
    }
    if (getBuiltinOperatorName(expr, builtinName) && builtinName == "divide" && expr.args.size() == 2) {
      const std::string leftType = inferStructReturnPath(expr.args[0], params, locals);
      const std::string rightType = inferStructReturnPath(expr.args[1], params, locals);
      if (isMatrixQuaternionTypePath(leftType) && rightType.empty() && isNumericScalarExpr(expr.args[1])) {
        return leftType;
      }
      if (isMatrixQuaternionTypePath(leftType) || isMatrixQuaternionTypePath(rightType)) {
        return "";
      }
    }
    if (expr.isFieldAccess && expr.args.size() == 1) {
      auto isStaticField = [](const Expr &stmt) -> bool {
        for (const auto &transform : stmt.transforms) {
          if (transform.name == "static") {
            return true;
          }
        }
        return false;
      };
      std::string receiverStruct;
      if (expr.args.front().kind == Expr::Kind::Name &&
          findParamBinding(params, expr.args.front().name) == nullptr &&
          locals.find(expr.args.front().name) == locals.end()) {
        receiverStruct = resolveStructTypePath(expr.args.front().name, expr.args.front().namespacePrefix);
      }
      if (receiverStruct.empty()) {
        receiverStruct = inferStructReturnPath(expr.args.front(), params, locals);
      }
      if (receiverStruct.empty()) {
        return "";
      }
      auto defIt = defMap_.find(receiverStruct);
      if (defIt == defMap_.end() || !defIt->second) {
        return "";
      }
      const bool isTypeReceiver =
          expr.args.front().kind == Expr::Kind::Name &&
          findParamBinding(params, expr.args.front().name) == nullptr &&
          locals.find(expr.args.front().name) == locals.end();
      for (const auto &stmt : defIt->second->statements) {
        if (!stmt.isBinding || stmt.name != expr.name) {
          continue;
        }
        if (isTypeReceiver ? !isStaticField(stmt) : isStaticField(stmt)) {
          continue;
        }
        BindingInfo fieldBinding;
        if (!resolveStructFieldBinding(*defIt->second, stmt, fieldBinding)) {
          return "";
        }
        std::string fieldType = fieldBinding.typeName;
        if ((fieldType == "Reference" || fieldType == "Pointer") && !fieldBinding.typeTemplateArg.empty()) {
          fieldType = fieldBinding.typeTemplateArg;
        }
        return resolveStructTypePath(fieldType, expr.namespacePrefix);
      }
      return "";
    }
    if (expr.isMethodCall) {
      if (expr.args.empty()) {
        return "";
      }
      std::string receiverStruct = inferStructReturnPath(expr.args.front(), params, locals);
      if (receiverStruct.empty()) {
        return "";
      }
      std::string methodName = expr.name;
      std::string rawMethodName = expr.name;
      if (!rawMethodName.empty() && rawMethodName.front() == '/') {
        rawMethodName.erase(rawMethodName.begin());
      }
      if (!methodName.empty() && methodName.front() == '/') {
        methodName.erase(methodName.begin());
      }
      std::string namespacedCollection;
      std::string namespacedHelper;
      if (getNamespacedCollectionHelperName(expr, namespacedCollection, namespacedHelper) &&
          !namespacedHelper.empty()) {
        methodName = namespacedHelper;
      }
      const bool blocksBuiltinVectorAccessStructReturnForwarding =
          methodName == "at" || methodName == "at_unsafe";
      const bool isExplicitRemovedMapAliasStructReturnMethod =
          rawMethodName == "map/at" || rawMethodName == "map/at_unsafe" ||
          rawMethodName == "std/collections/map/at" || rawMethodName == "std/collections/map/at_unsafe";
      std::vector<std::string> methodCandidates;
      if (receiverStruct == "/vector") {
        methodCandidates = {"/vector/" + methodName};
        if (!blocksBuiltinVectorAccessStructReturnForwarding) {
          methodCandidates.push_back("/std/collections/vector/" + methodName);
        }
        if (methodName != "count" && !blocksBuiltinVectorAccessStructReturnForwarding) {
          methodCandidates.push_back("/array/" + methodName);
        }
      } else if (receiverStruct == "/array") {
        methodCandidates = {"/array/" + methodName};
        if (methodName != "count") {
          methodCandidates.push_back("/vector/" + methodName);
          if (!blocksBuiltinVectorAccessStructReturnForwarding) {
            methodCandidates.push_back("/std/collections/vector/" + methodName);
          }
        }
      } else if (receiverStruct == "/map") {
        if (!isExplicitRemovedMapAliasStructReturnMethod) {
          methodCandidates = {"/std/collections/map/" + methodName, "/map/" + methodName};
        }
      } else {
        methodCandidates = {receiverStruct + "/" + methodName};
      }
      for (const auto &candidate : methodCandidates) {
        auto structIt = returnStructs_.find(candidate);
        if (structIt != returnStructs_.end()) {
          return structIt->second;
        }
      }
      for (const auto &candidate : methodCandidates) {
        auto defIt = defMap_.find(candidate);
        if (defIt == defMap_.end()) {
          continue;
        }
        if (!ensureDefinitionReturnKindReady(*defIt->second)) {
          return "";
        }
        auto structIt = returnStructs_.find(candidate);
        if (structIt != returnStructs_.end()) {
          return structIt->second;
        }
      }
      return "";
    }

    if (isMatchCall(expr)) {
      Expr expanded;
      std::string error;
      if (!lowerMatchToIf(expr, expanded, error)) {
        return "";
      }
      return inferStructReturnPath(expanded, params, locals);
    }
    if (isIfCall(expr) && expr.args.size() == 3) {
      const Expr &thenArg = expr.args[1];
      const Expr &elseArg = expr.args[2];
      const Expr *thenValue = getEnvelopeValueExpr(thenArg, true);
      const Expr *elseValue = getEnvelopeValueExpr(elseArg, true);
      const Expr &thenExpr = thenValue ? *thenValue : thenArg;
      const Expr &elseExpr = elseValue ? *elseValue : elseArg;
      std::string thenPath = inferStructReturnPath(thenExpr, params, locals);
      if (thenPath.empty()) {
        return "";
      }
      std::string elsePath = inferStructReturnPath(elseExpr, params, locals);
      return (thenPath == elsePath) ? thenPath : "";
    }

    if (const Expr *valueExpr = getEnvelopeValueExpr(expr, false)) {
      if (isReturnCall(*valueExpr) && !valueExpr->args.empty()) {
        return inferStructReturnPath(valueExpr->args.front(), params, locals);
      }
      return inferStructReturnPath(*valueExpr, params, locals);
    }

    if (expr.kind == Expr::Kind::Call) {
      std::string accessHelperName;
      if (getBuiltinArrayAccessName(expr, accessHelperName) && !expr.args.empty()) {
        size_t receiverIndex = expr.isMethodCall ? 0 : 0;
        if (!expr.isMethodCall && hasNamedArguments(expr.argNames)) {
          bool foundValues = false;
          for (size_t i = 0; i < expr.args.size(); ++i) {
            if (i < expr.argNames.size() && expr.argNames[i].has_value() &&
                *expr.argNames[i] == "values") {
              receiverIndex = i;
              foundValues = true;
              break;
            }
          }
          if (!foundValues) {
            receiverIndex = 0;
          }
        }
        if (receiverIndex < expr.args.size()) {
          std::string elemType;
          if (resolveArgsPackAccessTarget(expr.args[receiverIndex], elemType) ||
              resolveVectorTarget(expr.args[receiverIndex], elemType) ||
              resolveArrayTarget(expr.args[receiverIndex], elemType) ||
              resolveStringTarget(expr.args[receiverIndex])) {
            return "";
          }
        }
      }
      std::string collectionTypePath;
      if (resolveCallCollectionTypePath(expr, collectionTypePath) && collectionTypePath == "/map") {
        const std::string resolvedCollectionPath = resolveCalleePath(expr);
        std::string normalizedCollectionPath = normalizeBindingTypeName(resolvedCollectionPath);
        if (!normalizedCollectionPath.empty() && normalizedCollectionPath.front() != '/') {
          normalizedCollectionPath.insert(normalizedCollectionPath.begin(), '/');
        }
        if (normalizedCollectionPath.rfind("/std/collections/experimental_map/Map__", 0) == 0 &&
            structNames_.count(resolvedCollectionPath) > 0) {
          return resolvedCollectionPath;
        }
        std::vector<std::string> collectionTemplateArgs;
        auto specializedExperimentalMapPath = [&](const std::vector<std::string> &templateArgs) {
          auto stripWhitespace = [](const std::string &text) {
            std::string result;
            result.reserve(text.size());
            for (unsigned char ch : text) {
              if (!std::isspace(ch)) {
                result.push_back(static_cast<char>(ch));
              }
            }
            return result;
          };
          auto fnv1a64 = [](const std::string &text) {
            uint64_t hash = 1469598103934665603ULL;
            for (unsigned char ch : text) {
              hash ^= static_cast<uint64_t>(ch);
              hash *= 1099511628211ULL;
            }
            return hash;
          };
          std::ostringstream specializedPath;
          specializedPath << "/std/collections/experimental_map/Map__t"
                          << std::hex
                          << fnv1a64(stripWhitespace(joinTemplateArgs(templateArgs)));
          return specializedPath.str();
        };
        if (resolveCallCollectionTemplateArgs(expr, "map", collectionTemplateArgs) &&
            collectionTemplateArgs.size() == 2) {
          return specializedExperimentalMapPath(collectionTemplateArgs);
        }
        auto defIt = defMap_.find(resolvedCollectionPath);
        if (defIt != defMap_.end() && defIt->second != nullptr) {
          for (const auto &transform : defIt->second->transforms) {
            if (transform.name != "return" || transform.templateArgs.size() != 1) {
              continue;
            }
            std::string keyType;
            std::string valueType;
            if (extractMapKeyValueTypesFromTypeText(transform.templateArgs.front(), keyType, valueType)) {
              return specializedExperimentalMapPath({keyType, valueType});
            }
          }
        }
        return "/map";
      }
    }

    const std::string resolvedCallee = resolveCalleePath(expr);
    const bool isExplicitMapAccessCompatibilityCall = isExplicitMapAccessStructReturnCompatibilityCall(expr);
    const std::string structReturnProbePath = isExplicitMapAccessCompatibilityCall ? expr.name : resolvedCallee;
    auto resolvedCandidates = collectionHelperPathCandidates(structReturnProbePath);
    pruneMapAccessStructReturnCompatibilityCandidates(structReturnProbePath, resolvedCandidates);
    pruneBuiltinVectorAccessStructReturnCandidates(expr, structReturnProbePath, resolvedCandidates);
    for (const auto &candidate : resolvedCandidates) {
      auto structIt = returnStructs_.find(candidate);
      if (structIt != returnStructs_.end()) {
        return structIt->second;
      }
    }
    std::string resolved = resolvedCandidates.empty() ? std::string() : resolvedCandidates.front();
    bool hasDefinitionCandidate = false;
    for (const auto &candidate : resolvedCandidates) {
      auto defIt = defMap_.find(candidate);
      if (defIt == defMap_.end()) {
        continue;
      }
      hasDefinitionCandidate = true;
      if (!ensureDefinitionReturnKindReady(*defIt->second)) {
        return "";
      }
      auto structIt = returnStructs_.find(candidate);
      if (structIt != returnStructs_.end()) {
        return structIt->second;
      }
      if (resolved.empty()) {
        resolved = candidate;
      }
    }
    std::string collection;
    if (!hasDefinitionCandidate && getBuiltinCollectionName(expr, collection)) {
      if ((collection == "array" || collection == "vector") && expr.templateArgs.size() == 1) {
        return "/" + collection;
      }
      if (collection == "map" && expr.templateArgs.size() == 2) {
        return "/map";
      }
    }
    if (!resolved.empty() && structNames_.count(resolved) > 0) {
      return resolved;
    }
  }

  return "";
}

} // namespace primec::semantics
