#include "SemanticsValidator.h"

#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace primec::semantics {
namespace {

ReturnKind returnKindForStatementBinding(const BindingInfo &binding) {
  if (binding.typeName == "Reference") {
    std::string base;
    std::string arg;
    if (splitTemplateTypeName(binding.typeTemplateArg, base, arg) && base == "array") {
      std::vector<std::string> args;
      if (splitTopLevelTemplateArgs(arg, args) && args.size() == 1) {
        return ReturnKind::Array;
      }
    }
    return returnKindForTypeName(binding.typeTemplateArg);
  }
  return returnKindForTypeName(binding.typeName);
}

} // namespace

bool SemanticsValidator::isStringStatementExpr(const Expr &arg,
                                               const std::vector<ParameterInfo> &params,
                                               const std::unordered_map<std::string, BindingInfo> &locals) {
  BuiltinCollectionDispatchResolverAdapters builtinCollectionDispatchResolverAdapters;
  const auto builtinCollectionDispatchResolvers = makeBuiltinCollectionDispatchResolvers(
      params, locals, builtinCollectionDispatchResolverAdapters);
  std::function<bool(const Expr &, const std::unordered_map<std::string, BindingInfo> &)> isStringExprRef;
  auto resolveArrayTarget = [&](const Expr &target, std::string &elemTypeOut) -> bool {
    elemTypeOut.clear();
    if (target.kind == Expr::Kind::Name) {
      auto resolveReference = [&](const BindingInfo &binding) -> bool {
        if (binding.typeName != "Reference" || binding.typeTemplateArg.empty()) {
          return false;
        }
        std::string base;
        std::string arg;
        if (!splitTemplateTypeName(binding.typeTemplateArg, base, arg) || base != "array") {
          return false;
        }
        std::vector<std::string> args;
        if (!splitTopLevelTemplateArgs(arg, args) || args.size() != 1) {
          return false;
        }
        elemTypeOut = args.front();
        return true;
      };
      if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
        if (resolveReference(*paramBinding)) {
          return true;
        }
        if ((paramBinding->typeName != "array" && paramBinding->typeName != "vector") ||
            paramBinding->typeTemplateArg.empty()) {
          return false;
        }
        elemTypeOut = paramBinding->typeTemplateArg;
        return true;
      }
      auto it = locals.find(target.name);
      if (it == locals.end()) {
        return false;
      }
      if (resolveReference(it->second)) {
        return true;
      }
      if ((it->second.typeName != "array" && it->second.typeName != "vector") ||
          it->second.typeTemplateArg.empty()) {
        return false;
      }
      elemTypeOut = it->second.typeTemplateArg;
      return true;
    }
    if (target.kind == Expr::Kind::Call) {
      std::string collectionTypePath;
      if (resolveCallCollectionTypePath(target, params, locals, collectionTypePath) &&
          (collectionTypePath == "/array" || collectionTypePath == "/vector")) {
        std::vector<std::string> templateArgs;
        const std::string expectedBase = collectionTypePath == "/array" ? "array" : "vector";
        if (resolveCallCollectionTemplateArgs(target, expectedBase, params, locals, templateArgs) &&
            templateArgs.size() == 1) {
          elemTypeOut = templateArgs.front();
          return true;
        }
      }
      std::string collection;
      if (defMap_.find(resolveCalleePath(target)) != defMap_.end()) {
        return false;
      }
      if (getBuiltinCollectionName(target, collection) && (collection == "array" || collection == "vector") &&
          target.templateArgs.size() == 1) {
        elemTypeOut = target.templateArgs.front();
        return true;
      }
    }
    return false;
  };
  auto resolveMapValueType = [&](const Expr &target, std::string &valueTypeOut) -> bool {
    valueTypeOut.clear();
    if (target.kind == Expr::Kind::Name) {
      if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
        std::string ignoredKeyType;
        return extractMapKeyValueTypes(*paramBinding, ignoredKeyType, valueTypeOut);
      }
      auto it = locals.find(target.name);
      if (it == locals.end()) {
        return false;
      }
      std::string ignoredKeyType;
      return extractMapKeyValueTypes(it->second, ignoredKeyType, valueTypeOut);
    }
    if (target.kind == Expr::Kind::Call) {
      auto defIt = defMap_.find(resolveCalleePath(target));
      if (defIt != defMap_.end() && defIt->second) {
        auto extractMapValueTypeFromReturn = [&](const std::string &typeName) -> bool {
          std::string normalizedType = normalizeBindingTypeName(typeName);
          while (true) {
            std::string base;
            std::string arg;
            if (!splitTemplateTypeName(normalizedType, base, arg)) {
              return false;
            }
            if (isMapCollectionTypeName(base)) {
              std::vector<std::string> args;
              if (!splitTopLevelTemplateArgs(arg, args) || args.size() != 2) {
                return false;
              }
              valueTypeOut = args[1];
              return true;
            }
            if (base == "Reference" || base == "Pointer") {
              std::vector<std::string> args;
              if (!splitTopLevelTemplateArgs(arg, args) || args.size() != 1) {
                return false;
              }
              normalizedType = normalizeBindingTypeName(args.front());
              continue;
            }
            return false;
          }
        };
        for (const auto &transform : defIt->second->transforms) {
          if (transform.name != "return" || transform.templateArgs.size() != 1) {
            continue;
          }
          return extractMapValueTypeFromReturn(transform.templateArgs.front());
        }
        return false;
      }
      std::string collection;
      if (!getBuiltinCollectionName(target, collection) || collection != "map" || target.templateArgs.size() != 2) {
        return false;
      }
      valueTypeOut = target.templateArgs[1];
      return true;
    }
    return false;
  };
  isStringExprRef = [&](const Expr &candidate,
                        const std::unordered_map<std::string, BindingInfo> &candidateLocals) -> bool {
    if (candidate.kind == Expr::Kind::StringLiteral) {
      return true;
    }
    if (candidate.kind == Expr::Kind::Name) {
      if (const BindingInfo *paramBinding = findParamBinding(params, candidate.name)) {
        return paramBinding->typeName == "string";
      }
      auto it = candidateLocals.find(candidate.name);
      return it != candidateLocals.end() && it->second.typeName == "string";
    }
    if (candidate.kind == Expr::Kind::Call && isIfCall(candidate) && candidate.args.size() == 3) {
      auto resolveEnvelopeValue = [&](const Expr &branchExpr,
                                      std::unordered_map<std::string, BindingInfo> &branchLocals,
                                      const Expr *&valueOut) -> bool {
        valueOut = nullptr;
        branchLocals = candidateLocals;
        if (branchExpr.kind != Expr::Kind::Call || branchExpr.isBinding || branchExpr.isMethodCall) {
          return false;
        }
        if (!branchExpr.args.empty() || !branchExpr.templateArgs.empty() || hasNamedArguments(branchExpr.argNames)) {
          return false;
        }
        if (!branchExpr.hasBodyArguments && branchExpr.bodyArguments.empty()) {
          return false;
        }
        for (const auto &bodyExpr : branchExpr.bodyArguments) {
          if (bodyExpr.isBinding) {
            BindingInfo info;
            std::optional<std::string> restrictType;
            if (!parseBindingInfo(bodyExpr,
                                  bodyExpr.namespacePrefix,
                                  structNames_,
                                  importAliases_,
                                  info,
                                  restrictType,
                                  error_)) {
              return false;
            }
            if (!hasExplicitBindingTypeTransform(bodyExpr) && bodyExpr.args.size() == 1) {
              (void)inferBindingTypeFromInitializer(bodyExpr.args.front(), params, branchLocals, info, &bodyExpr);
            }
            insertLocalBinding(branchLocals, bodyExpr.name, std::move(info));
            continue;
          }
          if (isReturnCall(bodyExpr) && bodyExpr.args.size() == 1) {
            valueOut = &bodyExpr.args.front();
          } else {
            valueOut = &bodyExpr;
          }
        }
        return valueOut != nullptr;
      };
      const Expr *thenValue = nullptr;
      const Expr *elseValue = nullptr;
      std::unordered_map<std::string, BindingInfo> thenLocals;
      std::unordered_map<std::string, BindingInfo> elseLocals;
      return resolveEnvelopeValue(candidate.args[1], thenLocals, thenValue) &&
             resolveEnvelopeValue(candidate.args[2], elseLocals, elseValue) &&
             isStringExprRef(*thenValue, thenLocals) &&
             isStringExprRef(*elseValue, elseLocals);
    }
    if (this->isStringExprForArgumentValidation(candidate, builtinCollectionDispatchResolvers)) {
      return true;
    }
    if (candidate.kind == Expr::Kind::Call) {
      const std::string resolvedPath = resolveCalleePath(candidate);
      const bool treatAsBuiltinAccess =
          defMap_.find(resolvedPath) == defMap_.end() ||
          resolvedPath.rfind("/std/collections/vector/at", 0) == 0 ||
          resolvedPath == "/map/at" ||
          resolvedPath == "/map/at_unsafe" ||
          resolvedPath == "/std/collections/map/at" ||
          resolvedPath == "/std/collections/map/at_unsafe";
      std::string accessName;
      if (treatAsBuiltinAccess &&
          getBuiltinArrayAccessName(candidate, accessName) &&
          candidate.args.size() == 2) {
        std::string elemType;
        if (resolveArrayTarget(candidate.args.front(), elemType) &&
            normalizeBindingTypeName(elemType) == "string") {
          return true;
        }
        std::string mapValueType;
        if (resolveMapValueType(candidate.args.front(), mapValueType) &&
            normalizeBindingTypeName(mapValueType) == "string") {
          return true;
        }
      }
      std::string inferredTypeText;
      if (inferQueryExprTypeText(candidate, params, candidateLocals, inferredTypeText) &&
          normalizeBindingTypeName(inferredTypeText) == "string") {
        return true;
      }
      if (inferExprReturnKind(candidate, params, candidateLocals) == ReturnKind::String) {
        return true;
      }
    }
    return false;
  };
  if (arg.kind == Expr::Kind::StringLiteral) {
    return true;
  }
  if (arg.kind == Expr::Kind::Name) {
    if (const BindingInfo *paramBinding = findParamBinding(params, arg.name)) {
      return paramBinding->typeName == "string";
    }
    auto it = locals.find(arg.name);
    return it != locals.end() && it->second.typeName == "string";
  }
  return isStringExprRef(arg, locals);
}

bool SemanticsValidator::isPrintableStatementExpr(const Expr &arg,
                                                  const std::vector<ParameterInfo> &params,
                                                  const std::unordered_map<std::string, BindingInfo> &locals) {
  if (isStringStatementExpr(arg, params, locals)) {
    return true;
  }
  ReturnKind kind = inferExprReturnKind(arg, params, locals);
  if (kind == ReturnKind::Int || kind == ReturnKind::Int64 || kind == ReturnKind::UInt64 || kind == ReturnKind::Bool) {
    return true;
  }
  if (kind == ReturnKind::String) {
    return true;
  }
  if (kind == ReturnKind::Void || kind == ReturnKind::Array) {
    return false;
  }
  if (arg.kind == Expr::Kind::Name) {
    if (const BindingInfo *paramBinding = findParamBinding(params, arg.name)) {
      ReturnKind paramKind = returnKindForStatementBinding(*paramBinding);
      return paramKind == ReturnKind::Int || paramKind == ReturnKind::Int64 || paramKind == ReturnKind::UInt64 ||
             paramKind == ReturnKind::Bool;
    }
    auto it = locals.find(arg.name);
    if (it != locals.end()) {
      ReturnKind localKind = returnKindForStatementBinding(it->second);
      return localKind == ReturnKind::Int || localKind == ReturnKind::Int64 || localKind == ReturnKind::UInt64 ||
             localKind == ReturnKind::Bool;
    }
  }
  if (isPointerLikeExpr(arg, params, locals)) {
    return false;
  }
  if (arg.kind == Expr::Kind::Call) {
    std::string builtinName;
    if (defMap_.find(resolveCalleePath(arg)) == defMap_.end() && getBuiltinCollectionName(arg, builtinName)) {
      return false;
    }
  }
  return false;
}

} // namespace primec::semantics
