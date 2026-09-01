// collection-surface-audit: exempt
#include "SemanticsValidator.h"
#include "StdlibCollectionSurfaceHelpers.h"
#include "SemanticsValidatorInferCollectionCompatibilityInternal.h"
#include "primec/support/CollectionSpellingClassifier.h"
#include "primec/support/StdlibSurfaceRegistry.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <sstream>
#include <string>
#include <string_view>

namespace primec::semantics {
std::string SemanticsValidator::classifyVectorCompatHelperParamFamily(
    const BindingInfo &binding) const {
  std::string elemType;
  std::string keyType;
  std::string valueType;
  if (extractCollectionVectorElementType(binding, elemType)) {
    return legacyExperimentalVectorCompatibilityFamilyName();
  }
  if (extractKeyValueCollectionTypes(binding, keyType, valueType)) {
    return "map";
  }
  const std::string normalizedType = normalizeBindingTypeName(binding.typeName);
  if (normalizedType == "vector" ||
      isInternalSoaCollectionTypeName(normalizedType) ||
      normalizedType == "array" || normalizedType == "string") {
    return normalizedType;
  }
  return {};
}

bool SemanticsValidator::explicitVectorCompatHelperFamilyHasCompatibleReceiver(
    std::string_view path, std::string_view receiverFamily) const {
  const std::string pathText(path);
  auto paramsMatchReceiver = [&](const std::vector<ParameterInfo> &helperParams) {
    return !helperParams.empty() &&
           classifyVectorCompatHelperParamFamily(helperParams.front().binding) == receiverFamily;
  };
  auto definitionMatchesReceiver = [&](const Definition &def) {
    if (def.parameters.empty()) {
      return false;
    }
    BindingInfo binding;
    std::optional<std::string> restrictType;
    std::string parseError;
    if (!parseBindingInfo(def.parameters.front(), def.namespacePrefix,
                          structNames_, importAliases_, binding,
                          restrictType, parseError, &sumNames_)) {
      return false;
    }
    return classifyVectorCompatHelperParamFamily(binding) == receiverFamily;
  };
  if (auto paramsIt = paramsByDef_.find(pathText);
      paramsIt != paramsByDef_.end() && paramsMatchReceiver(paramsIt->second)) {
    return true;
  }
  const std::string overloadPrefix = pathText + "__ov";
  const std::string specializationPrefix = pathText + "__t";
  for (const auto &def : program_.definitions) {
    if (def.fullPath != pathText &&
        def.fullPath.rfind(overloadPrefix, 0) != 0 &&
        def.fullPath.rfind(specializationPrefix, 0) != 0) {
      continue;
    }
    if (auto paramsIt = paramsByDef_.find(def.fullPath);
        paramsIt != paramsByDef_.end() && paramsMatchReceiver(paramsIt->second)) {
      return true;
    }
    if (definitionMatchesReceiver(def)) {
      return true;
    }
  }
  return false;
}

std::string SemanticsValidator::classifyExplicitVectorHelperReceiver(
    const Expr &receiverExpr, const MethodTargetCollectionResolvers &resolvers) const {
  std::string elemType;
  if (resolvers.resolveCollectionVectorValueTarget(receiverExpr, elemType)) {
    return legacyExperimentalVectorCompatibilityFamilyName();
  }
  if (resolvers.resolveVectorTarget(receiverExpr, elemType)) {
    return "vector";
  }
  if (resolvers.resolveSoaVectorTarget(receiverExpr, elemType)) {
    return internalSoaCollectionTypeName();
  }
  if (resolvers.resolveArrayTarget(receiverExpr, elemType)) {
    return "array";
  }
  if (resolvers.resolveStringTarget(receiverExpr)) {
    return "string";
  }
  if (resolvers.resolveKeyValueTarget(receiverExpr)) {
    return "map";
  }
  return {};
}

bool SemanticsValidator::hasReceiverCompatibleExplicitVectorHelperPath(
    const std::string &path, const Expr &receiverExpr,
    const MethodTargetCollectionResolvers &resolvers) const {
  const std::string receiverFamily = classifyExplicitVectorHelperReceiver(receiverExpr, resolvers);
  if (receiverFamily.empty()) {
    return false;
  }
  return explicitVectorCompatHelperFamilyHasCompatibleReceiver(path, receiverFamily);
}

bool SemanticsValidator::preferExplicitCanonicalVectorHelperForReceiver(
    const Expr &receiverExpr, const std::string &explicitVectorHelperPath,
    const MethodTargetCollectionResolvers &resolvers) const {
  if (explicitVectorHelperPath.empty()) {
    return false;
  }
  std::string elemType;
  if (isCanonicalVectorCompatibilityPath(explicitVectorHelperPath)) {
    return resolvers.resolveCollectionVectorValueTarget(receiverExpr, elemType);
  }
  if (splitSoaSurfaceHelperPath(explicitVectorHelperPath, nullptr, nullptr)) {
    return resolvers.resolveSoaVectorTarget(receiverExpr, elemType);
  }
  return false;
}

std::optional<bool> SemanticsValidator::tryResolveExplicitCanonicalVectorCountMethodTarget(
    const Expr &receiverExpr,
    const std::string &explicitVectorHelperPath,
    const std::string &normalizedMethodName,
    const MethodTargetCollectionResolvers &resolvers,
    std::string &resolvedOut,
    bool &isBuiltinOut) {
  if (explicitVectorHelperPath.empty() ||
      !isCanonicalVectorCompatibilityPath(explicitVectorHelperPath) ||
      normalizedMethodName != "count") {
    return std::nullopt;
  }
  const std::string receiverFamily = classifyExplicitVectorHelperReceiver(receiverExpr, resolvers);
  if (receiverFamily != "string" && receiverFamily != "array" &&
      receiverFamily != "map") {
    return std::nullopt;
  }
  // A map receiver that is a call expression to a function with an
  // *explicit* map-typed return annotation (e.g. [return<map<i32, i32>>])
  // always rejects the explicit vector-namespaced count spelling with the
  // map-family "unknown call target" diagnostic, even if a same-path
  // helper exists for map - unlike array/string wrapper receivers, which
  // do accept a matching same-path helper (see "...keeps wrapper
  // array/string same-path helper" tests), and unlike a map receiver
  // whose type comes from body-inference rather than an explicit
  // annotation (e.g. [effects(heap_alloc)] with no return<> - see
  // "wrapper temporary canonical vector count slash-method rejects map
  // receiver"), which keeps the older "unknown method: <explicit path>"
  // diagnostic instead.
  if (receiverFamily == "map" && receiverExpr.kind == Expr::Kind::Call) {
    const std::string calleePath = resolveCalleePath(receiverExpr);
    auto calleeDefIt = defMap_.find(calleePath);
    bool hasExplicitMapReturnAnnotation = false;
    if (calleeDefIt != defMap_.end() && calleeDefIt->second != nullptr) {
      for (const auto &transform : calleeDefIt->second->transforms) {
        if (transform.name != "return" || transform.templateArgs.size() != 1) {
          continue;
        }
        std::string base;
        std::string arg;
        const std::string normalizedBase =
            splitTemplateTypeName(transform.templateArgs.front(), base, arg)
                ? normalizeBindingTypeName(base)
                : normalizeBindingTypeName(transform.templateArgs.front());
        if (normalizedBase == "map") {
          hasExplicitMapReturnAnnotation = true;
          break;
        }
      }
    }
    if (hasExplicitMapReturnAnnotation) {
      return failExprDiagnostic(receiverExpr,
          "unknown call target: " +
          canonicalKeyValueCompatibilityPrefixOrFallback() + "/" +
          normalizedMethodName);
    }
  }
  if (hasReceiverCompatibleExplicitVectorHelperPath(explicitVectorHelperPath,
                                                    receiverExpr, resolvers)) {
    resolvedOut = explicitVectorHelperPath;
    isBuiltinOut = false;
    return true;
  }
  return failExprDiagnostic(receiverExpr, "unknown method: " +
                                          explicitVectorHelperPath);
}

std::string SemanticsValidator::preferredBorrowedSoaHelperTargetForCollectionMethod(
    std::string helperName) const {
  // TODO-4691: all four branches now resolve via the registry-backed
  // borrowed-variant lookup instead of hardcoded literal reassignment
  // (TODO-4690 migrated only the "count" branch; this migrates the
  // rest of this lambda's chain).
  if (const std::string_view borrowedVariant = findBorrowedVariant(
          StdlibSurfaceId::CollectionsColumnarHelpers, helperName);
      !borrowedVariant.empty()) {
    helperName = std::string(borrowedVariant);
  }
  return preferredSoaHelperTargetForCollectionType(
      helperName, internalSoaCollectionTypePath(true));
}

bool SemanticsValidator::resolveBorrowedVectorReceiver(
    const Expr &candidate, std::string &elemTypeOut,
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals) {
  auto extractBorrowedVectorType = [&](const BindingInfo &binding) {
    const std::string normalizedType = normalizeBindingTypeName(binding.typeName);
    if (normalizedType == "vector" && !binding.typeTemplateArg.empty()) {
      elemTypeOut = binding.typeTemplateArg;
      return true;
    }
    if ((normalizedType != "Reference" && normalizedType != "Pointer") ||
        binding.typeTemplateArg.empty()) {
      return false;
    }
    std::string pointeeBase;
    std::string pointeeArgText;
    const std::string normalizedPointee =
        normalizeBindingTypeName(binding.typeTemplateArg);
    if (!splitTemplateTypeName(normalizedPointee, pointeeBase,
                               pointeeArgText)) {
      return false;
    }
    if (normalizeBindingTypeName(pointeeBase) != "vector" ||
        pointeeArgText.empty()) {
      return false;
    }
    elemTypeOut = pointeeArgText;
    return true;
  };
  if (candidate.kind == Expr::Kind::Name) {
    if (const BindingInfo *paramBinding = findParamBinding(params, candidate.name)) {
      return extractBorrowedVectorType(*paramBinding);
    }
    if (auto localIt = locals.find(candidate.name); localIt != locals.end()) {
      return extractBorrowedVectorType(localIt->second);
    }
  }
  if (candidate.kind == Expr::Kind::Call && !candidate.isBinding &&
      isSimpleCallName(candidate, "location") && candidate.args.size() == 1) {
    return resolveBorrowedVectorReceiver(candidate.args.front(), elemTypeOut, params, locals);
  }
  if (candidate.kind == Expr::Kind::Call && !candidate.isBinding &&
      isSimpleCallName(candidate, "dereference") && candidate.args.size() == 1) {
    return resolveBorrowedVectorReceiver(candidate.args.front(), elemTypeOut, params, locals);
  }
  std::string inferredTypeText;
  if (!inferQueryExprTypeText(candidate, params, locals, inferredTypeText) ||
      inferredTypeText.empty()) {
    return false;
  }
  const std::string normalizedType = normalizeBindingTypeName(inferredTypeText);
  std::string base;
  std::string argText;
  if (!splitTemplateTypeName(normalizedType, base, argText)) {
    return false;
  }
  const std::string normalizedBase = normalizeBindingTypeName(base);
  if (normalizedBase != "Reference" && normalizedBase != "Pointer") {
    return false;
  }
  std::string pointeeBase;
  std::string pointeeArgText;
  const std::string normalizedPointee = normalizeBindingTypeName(argText);
  if (!splitTemplateTypeName(normalizedPointee, pointeeBase, pointeeArgText)) {
    return false;
  }
  if (normalizeBindingTypeName(pointeeBase) != "vector" || pointeeArgText.empty()) {
    return false;
  }
  elemTypeOut = pointeeArgText;
  return true;
}

bool SemanticsValidator::resolveVectorTarget(
    const Expr &target, std::string &elemType,
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const std::function<bool(const Expr &, std::string &)> &resolveArgsPackAccessTarget) {
  if (target.kind == Expr::Kind::Name) {
    if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
      if (paramBinding->typeName == "vector" && !paramBinding->typeTemplateArg.empty()) {
        elemType = paramBinding->typeTemplateArg;
        return true;
      }
      return false;
    }
    auto it = locals.find(target.name);
    if (it != locals.end() && it->second.typeName == "vector" &&
        !it->second.typeTemplateArg.empty()) {
      elemType = it->second.typeTemplateArg;
      return true;
    }
    return false;
  }
  BindingInfo fieldBinding;
  if (resolveFieldBindingTarget(params, locals, target, fieldBinding) &&
      fieldBinding.typeName == "vector" && !fieldBinding.typeTemplateArg.empty()) {
    elemType = fieldBinding.typeTemplateArg;
    return true;
  }
  if (target.kind == Expr::Kind::Call) {
    std::string indexedElemType;
    if ((resolveIndexedArgsPackElementType(target, indexedElemType, resolveArgsPackAccessTarget) ||
         resolveWrappedIndexedArgsPackElementType(target, indexedElemType,
                                                  resolveArgsPackAccessTarget) ||
         resolveDereferencedIndexedArgsPackElementType(target, indexedElemType,
                                                        resolveArgsPackAccessTarget)) &&
        extractCollectionElementType(indexedElemType, "vector", elemType)) {
      return true;
    }
    std::string collectionTypePath;
    if (resolveCallCollectionTypePath(target, params, locals, collectionTypePath) &&
        collectionTypePath == "/vector") {
      std::vector<std::string> args;
      if (resolveCallCollectionTemplateArgs(target, "vector", params, locals, args) &&
          args.size() == 1) {
        elemType = args.front();
        return true;
      }
      std::string collectionName;
      if (getBuiltinCollectionName(target, collectionName) &&
          collectionName == "vector" &&
          target.templateArgs.size() == 1) {
        elemType = target.templateArgs.front();
        return true;
      }
      return false;
    }
    auto defIt = defMap_.find(resolveCalleePath(target));
    if (defIt != defMap_.end() && defIt->second != nullptr) {
      BindingInfo inferredReturn;
      if (inferDefinitionReturnBinding(*defIt->second, inferredReturn) &&
          normalizeBindingTypeName(inferredReturn.typeName) == "vector" &&
          !inferredReturn.typeTemplateArg.empty()) {
        elemType = inferredReturn.typeTemplateArg;
        return true;
      }
    }
    const std::string resolvedTarget = resolveCalleePath(target);
    const bool matchesSoaToAosTarget =
        ((target.isMethodCall && target.name == "to_aos") ||
         (!target.isMethodCall && isSimpleCallName(target, "to_aos"))) ||
        isCanonicalStdlibSoaHelperPath(resolvedTarget, "to_aos");
    const bool matchesBorrowedSoaToAosTarget =
        ((target.isMethodCall && target.name == "to_aos_ref") ||
         (!target.isMethodCall && isSimpleCallName(target, "to_aos_ref"))) ||
        isCanonicalStdlibSoaHelperPath(resolvedTarget, "to_aos_ref");
    if ((matchesSoaToAosTarget || matchesBorrowedSoaToAosTarget) &&
        target.args.size() == 1) {
      std::string sourceElemType;
      const Expr &source = target.args.front();
      if (resolveSoaVectorTarget(source, sourceElemType, params, locals,
                                 resolveArgsPackAccessTarget)) {
        elemType = sourceElemType;
        return true;
      }
      if (source.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, source.name)) {
          if (!isInternalSoaCollectionTypeName(paramBinding->typeName) ||
              paramBinding->typeTemplateArg.empty()) {
            return false;
          }
          sourceElemType = paramBinding->typeTemplateArg;
        } else {
          auto it = locals.find(source.name);
          if (it == locals.end() ||
              !isInternalSoaCollectionTypeName(it->second.typeName) ||
              it->second.typeTemplateArg.empty()) {
            return false;
          }
          sourceElemType = it->second.typeTemplateArg;
        }
      } else if (source.kind == Expr::Kind::Call) {
        std::string sourceCollectionTypePath;
        if (defMap_.find(resolveCalleePath(source)) == defMap_.end()) {
          std::string collection;
          if (getBuiltinCollectionName(source, collection) &&
              isInternalSoaCollectionTypeName(collection) &&
              source.templateArgs.size() == 1) {
            sourceElemType = source.templateArgs.front();
          }
          if (sourceElemType.empty() &&
              (((!source.isMethodCall && isSimpleCallName(source, "to_soa")) ||
                resolveCalleePath(source) == "/to_soa") &&
               source.args.size() == 1)) {
            if (!resolveVectorTarget(source.args.front(), sourceElemType, params, locals,
                                     resolveArgsPackAccessTarget)) {
              return false;
            }
          }
        } else if (!resolveCallCollectionTypePath(source, params, locals, sourceCollectionTypePath) ||
                   !isInternalSoaCollectionTypePath(sourceCollectionTypePath)) {
          return false;
        } else {
          std::vector<std::string> sourceArgs;
          if (resolveCallCollectionTemplateArgs(source,
                                               internalSoaCollectionTypeName(),
                                               params,
                                               locals,
                                               sourceArgs) &&
              sourceArgs.size() == 1) {
            sourceElemType = sourceArgs.front();
          }
        }
      } else {
        return false;
      }
      if (!sourceElemType.empty()) {
        elemType = sourceElemType;
        return true;
      }
    }
  }
  return false;
}

bool SemanticsValidator::resolveSoaVectorTarget(
    const Expr &target, std::string &elemType,
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const std::function<bool(const Expr &, std::string &)> &resolveArgsPackAccessTarget) {
  auto extractValueBinding = [&](const BindingInfo &binding) {
    if (isInternalSoaCollectionTypeName(binding.typeName) &&
        !binding.typeTemplateArg.empty()) {
      elemType = binding.typeTemplateArg;
      return true;
    }
    return extractExperimentalSoaVectorElementType(binding, elemType);
  };
  auto resolveInlineBorrowedValue = [&](const Expr &candidate) -> bool {
    auto resolveValueExpr = [&](const Expr &valueExpr) -> bool {
      if (valueExpr.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, valueExpr.name)) {
          return extractValueBinding(*paramBinding);
        }
        auto it = locals.find(valueExpr.name);
        return it != locals.end() && extractValueBinding(it->second);
      }
      if (valueExpr.kind != Expr::Kind::Call || valueExpr.isBinding) {
        return false;
      }
      BindingInfo inferredBinding;
      std::string inferredTypeText;
      if (inferQueryExprTypeText(valueExpr, params, locals, inferredTypeText)) {
        std::string base;
        std::string argText;
        const std::string normalizedType = normalizeBindingTypeName(inferredTypeText);
        if (splitTemplateTypeName(normalizedType, base, argText)) {
          inferredBinding.typeName = normalizeBindingTypeName(base);
          inferredBinding.typeTemplateArg = argText;
        } else {
          inferredBinding.typeName = normalizedType;
          inferredBinding.typeTemplateArg.clear();
        }
        if (extractValueBinding(inferredBinding)) {
          return true;
        }
      }
      return false;
    };
    if (!candidate.isBinding &&
        isSimpleCallName(candidate, "location") &&
        candidate.args.size() == 1) {
      return resolveValueExpr(candidate.args.front());
    }
    if (!candidate.isBinding &&
        isSimpleCallName(candidate, "dereference") &&
        candidate.args.size() == 1) {
      const Expr &borrowedExpr = candidate.args.front();
      return borrowedExpr.kind == Expr::Kind::Call &&
             !borrowedExpr.isBinding &&
             isSimpleCallName(borrowedExpr, "location") &&
             borrowedExpr.args.size() == 1 &&
             resolveValueExpr(borrowedExpr.args.front());
    }
    return false;
  };
  if (target.kind == Expr::Kind::Name) {
    if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
      return extractValueBinding(*paramBinding);
    }
    auto it = locals.find(target.name);
    if (it != locals.end()) {
      return extractValueBinding(it->second);
    }
    return false;
  }
  BindingInfo fieldBinding;
  if (resolveFieldBindingTarget(params, locals, target, fieldBinding) &&
      extractValueBinding(fieldBinding)) {
    return true;
  }
  if (target.kind == Expr::Kind::Call) {
    if (resolveInlineBorrowedValue(target)) {
      return true;
    }
    std::string indexedElemType;
    if ((resolveIndexedArgsPackElementType(target, indexedElemType, resolveArgsPackAccessTarget) ||
         resolveWrappedIndexedArgsPackElementType(target, indexedElemType,
                                                  resolveArgsPackAccessTarget) ||
         resolveDereferencedIndexedArgsPackElementType(target, indexedElemType,
                                                        resolveArgsPackAccessTarget)) &&
        extractCollectionElementType(indexedElemType,
                                     internalSoaCollectionTypeName(),
                                     elemType)) {
      return true;
    }
    std::string collectionTypePath;
    if (resolveCallCollectionTypePath(target, params, locals, collectionTypePath) &&
        isInternalSoaCollectionTypePath(collectionTypePath)) {
      std::vector<std::string> args;
      if (resolveCallCollectionTemplateArgs(target,
                                           internalSoaCollectionTypeName(),
                                           params,
                                           locals,
                                           args) &&
          args.size() == 1) {
        elemType = args.front();
      }
      return true;
    }
    if (((!target.isMethodCall && isSimpleCallName(target, "to_soa")) ||
         resolveCalleePath(target) == "/to_soa") &&
        target.args.size() == 1) {
      return resolveVectorTarget(target.args.front(), elemType, params, locals,
                                 resolveArgsPackAccessTarget);
    }
    BindingInfo inferredBinding;
    std::string inferredTypeText;
    if (inferQueryExprTypeText(target, params, locals, inferredTypeText)) {
      std::string base;
      std::string argText;
      const std::string normalizedType = normalizeBindingTypeName(inferredTypeText);
      if (splitTemplateTypeName(normalizedType, base, argText)) {
        inferredBinding.typeName = normalizeBindingTypeName(base);
        inferredBinding.typeTemplateArg = argText;
      } else {
        inferredBinding.typeName = normalizedType;
        inferredBinding.typeTemplateArg.clear();
      }
      if (extractValueBinding(inferredBinding)) {
        return true;
      }
    }
  }
  return false;
}

bool SemanticsValidator::resolveArrayTarget(
    const Expr &target, std::string &elemType, const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const std::function<bool(const Expr &, std::string &)> &resolveArgsPackAccessTarget) {
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
      elemType = args.front();
      return true;
    };
    if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
      if (resolveReference(*paramBinding)) {
        return true;
      }
      if ((paramBinding->typeName == "array" || paramBinding->typeName == "vector") &&
          !paramBinding->typeTemplateArg.empty()) {
        elemType = paramBinding->typeTemplateArg;
        return true;
      }
      return false;
    }
    auto it = locals.find(target.name);
    if (it == locals.end()) {
      return false;
    }
    if (resolveReference(it->second)) {
      return true;
    }
    if ((it->second.typeName == "array" || it->second.typeName == "vector") &&
        !it->second.typeTemplateArg.empty()) {
      elemType = it->second.typeTemplateArg;
      return true;
    }
    return false;
  }
  BindingInfo fieldBinding;
  if (resolveFieldBindingTarget(params, locals, target, fieldBinding)) {
    std::string base;
    std::string arg;
    if (fieldBinding.typeName == "Reference" && !fieldBinding.typeTemplateArg.empty() &&
        splitTemplateTypeName(fieldBinding.typeTemplateArg, base, arg) && base == "array") {
      std::vector<std::string> args;
      if (splitTopLevelTemplateArgs(arg, args) && args.size() == 1) {
        elemType = args.front();
        return true;
      }
    }
    if ((fieldBinding.typeName == "array" || fieldBinding.typeName == "vector") &&
        !fieldBinding.typeTemplateArg.empty()) {
      elemType = fieldBinding.typeTemplateArg;
      return true;
    }
  }
  if (target.kind == Expr::Kind::Call) {
    std::string indexedElemType;
    if ((resolveIndexedArgsPackElementType(target, indexedElemType, resolveArgsPackAccessTarget) ||
         resolveWrappedIndexedArgsPackElementType(target, indexedElemType,
                                                  resolveArgsPackAccessTarget) ||
         resolveDereferencedIndexedArgsPackElementType(target, indexedElemType,
                                                        resolveArgsPackAccessTarget)) &&
        (extractCollectionElementType(indexedElemType, "array", elemType) ||
         extractCollectionElementType(indexedElemType, "vector", elemType))) {
      return true;
    }
    std::string collectionTypePath;
    if (resolveCallCollectionTypePath(target, params, locals, collectionTypePath) &&
        (collectionTypePath == "/array" || collectionTypePath == "/vector")) {
      std::vector<std::string> args;
      const std::string expectedBase = collectionTypePath == "/vector" ? "vector" : "array";
      if (resolveCallCollectionTemplateArgs(target, expectedBase, params, locals, args) &&
          args.size() == 1) {
        elemType = args.front();
      }
      return true;
    }
  }
  return false;
}

bool SemanticsValidator::resolveCollectionVectorValueTarget(
    const Expr &target, std::string &elemTypeOut, const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals) {
  elemTypeOut.clear();
  auto extractValueBinding = [&](const BindingInfo &binding) {
    const std::string normalizedType = normalizeBindingTypeName(binding.typeName);
    if (normalizedType == "Reference" || normalizedType == "Pointer") {
      return false;
    }
    return extractCollectionVectorElementType(binding, elemTypeOut);
  };
  auto extractBindingFromTypeText = [&](const std::string &typeText, BindingInfo &bindingOut) {
    const std::string normalizedType = normalizeBindingTypeName(typeText);
    std::string base;
    std::string argText;
    if (splitTemplateTypeName(normalizedType, base, argText)) {
      bindingOut.typeName = normalizeBindingTypeName(base);
      bindingOut.typeTemplateArg = argText;
    } else {
      bindingOut.typeName = normalizedType;
      bindingOut.typeTemplateArg.clear();
    }
  };
  BindingInfo binding;
  if (target.kind == Expr::Kind::Name) {
    if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
      return extractValueBinding(*paramBinding);
    }
    if (auto it = locals.find(target.name); it != locals.end()) {
      return extractValueBinding(it->second);
    }
  }
  if (target.kind == Expr::Kind::Call) {
    auto defIt = defMap_.find(resolveCalleePath(target));
    if (defIt != defMap_.end() && defIt->second != nullptr &&
        inferDefinitionReturnBinding(*defIt->second, binding) &&
        extractValueBinding(binding)) {
      return true;
    }
    std::string receiverTypeText;
    if (inferQueryExprTypeText(target, params, locals, receiverTypeText)) {
      extractBindingFromTypeText(receiverTypeText, binding);
      return extractValueBinding(binding);
    }
    const std::string inferredStructPath = inferStructReturnPath(target, params, locals);
    if (!inferredStructPath.empty()) {
      binding.typeName = inferredStructPath;
      binding.typeTemplateArg.clear();
      return extractValueBinding(binding);
    }
  }
  return false;
}

std::string SemanticsValidator::preferredBorrowedSoaAccessHelperTarget(
    std::string_view helperName) const {
  // TODO-4691: registry-backed borrowed-variant lookup instead of a
  // hardcoded count/get/ref/to_aos literal chain (mirrors the sibling
  // preferredBorrowedSoaHelperTargetForCollectionMethod member).
  if (const std::string_view borrowedVariant = findBorrowedVariant(
          StdlibSurfaceId::CollectionsColumnarHelpers, helperName);
      !borrowedVariant.empty()) {
    helperName = borrowedVariant;
  }
  return preferredSoaHelperTargetForCollectionType(helperName, internalSoaCollectionTypePath(true));
}

bool SemanticsValidator::tryRedirectConcreteExperimentalSoaMethodTarget(
    const std::string &resolvedType, const std::string &canonicalCollectionHelperName,
    const Expr &receiver, const std::string &explicitRemovedMethodPath,
    const std::string &normalizedMethodName, const MethodTargetCollectionResolvers &resolvers,
    std::string &resolvedOut, bool &isBuiltinOut) {
  const bool isConcreteExperimentalSoaReceiver =
      isExperimentalSoaVectorSpecializedTypePath(resolvedType);
  const bool isCanonicalSoaWrapperMethod =
      isSupportedCompatibilitySoaHelperName(canonicalCollectionHelperName);
  if (!isConcreteExperimentalSoaReceiver || !isCanonicalSoaWrapperMethod) {
    return false;
  }
  return resolveExplicitOrCanonicalCollectionMethodTarget(
      preferredSoaHelperTargetForCollectionType(canonicalCollectionHelperName,
                                                internalSoaCollectionTypePath(true)),
      explicitRemovedMethodPath, normalizedMethodName, receiver, resolvers, resolvedOut,
      isBuiltinOut);
}

bool SemanticsValidator::resolveCollectionVectorMetadataMethodTarget(
    const std::string &normalizedMethodName, const Expr &receiver,
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals, std::string &resolvedOut,
    bool &isBuiltinOut) {
  auto isCollectionVectorMetadataMethodName = [](std::string_view name) {
    return name == "field_count" || name == "field_capacity" ||
           name == "set_field_count" || name == "set_field_capacity";
  };
  if (!isCollectionVectorMetadataMethodName(normalizedMethodName)) {
    return false;
  }
  auto receiverBindingTypeText = [](const BindingInfo &binding) {
    if (binding.typeTemplateArg.empty()) {
      return binding.typeName;
    }
    return binding.typeName + "<" + binding.typeTemplateArg + ">";
  };
  auto concreteExperimentalVectorReceiverPath =
      [&](const BindingInfo &binding) -> std::string {
    std::string typeText =
        normalizeBindingTypeName(receiverBindingTypeText(binding));
    while (true) {
      std::string base;
      std::string argText;
      if (!splitTemplateTypeName(typeText, base, argText) || base.empty()) {
        break;
      }
      base = normalizeBindingTypeName(base);
      if (base != "Reference" && base != "Pointer") {
        break;
      }
      std::vector<std::string> args;
      if (!splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
        return {};
      }
      typeText = normalizeBindingTypeName(args.front());
    }
    if (!typeText.empty() && typeText.front() != '/') {
      typeText.insert(typeText.begin(), '/');
    }
    if (isLegacyExperimentalVectorCompatibilityTypePath(typeText)) {
      return typeText;
    }
    std::string elemType;
    if (extractCollectionVectorElementType(binding, elemType)) {
      const std::string specializedVectorPath =
          specializedExperimentalVectorHelperTarget("Vector", elemType);
      if (isLegacyExperimentalVectorCompatibilityTypePath(
              specializedVectorPath)) {
        return specializedVectorPath;
      }
    }
    return {};
  };
  BindingInfo receiverBinding;
  if (receiver.kind == Expr::Kind::Name) {
    if (const BindingInfo *paramBinding = findParamBinding(params, receiver.name)) {
      receiverBinding = *paramBinding;
    } else if (auto localIt = locals.find(receiver.name); localIt != locals.end()) {
      receiverBinding = localIt->second;
    }
  }
  if (receiverBinding.typeName.empty()) {
    std::string receiverTypeText;
    if (inferQueryExprTypeText(receiver, params, locals, receiverTypeText)) {
      std::string base;
      std::string argText;
      const std::string normalizedReceiverType =
          normalizeBindingTypeName(receiverTypeText);
      if (splitTemplateTypeName(normalizedReceiverType, base, argText)) {
        receiverBinding.typeName = normalizeBindingTypeName(base);
        receiverBinding.typeTemplateArg = argText;
      } else {
        receiverBinding.typeName = normalizedReceiverType;
      }
    }
  }
  const std::string receiverPath =
      concreteExperimentalVectorReceiverPath(receiverBinding);
  if (receiverPath.empty()) {
    return false;
  }
  const std::string methodPath = receiverPath + "/" + normalizedMethodName;
  if (!hasDefinitionFamilyPath(methodPath)) {
    return false;
  }
  resolvedOut = methodPath;
  isBuiltinOut = false;
  return true;
}

std::string SemanticsValidator::explicitVectorMethodPath(
    const std::string &rawMethodName, const std::string &callNamespacePrefix) const {
  std::string candidate = rawMethodName;
  if (!candidate.empty() && candidate.front() == '/') {
    candidate.erase(candidate.begin());
  }
  std::string normalizedPrefix = callNamespacePrefix;
  if (!normalizedPrefix.empty() && normalizedPrefix.front() == '/') {
    normalizedPrefix.erase(normalizedPrefix.begin());
  }
  if (normalizedPrefix == "vector" ||
      isCanonicalVectorCompatibilityNamespace(normalizedPrefix) ||
      isCompatibilitySoaSurfaceNamespace(normalizedPrefix) ||
      isPublicSoaSurfaceNamespace(normalizedPrefix)) {
    return "/" + normalizedPrefix + "/" + candidate;
  }
  if (isUnrootedVectorHelperPath(candidate) ||
      isUnrootedCanonicalVectorCompatibilityPath(candidate) ||
      splitSoaSurfaceHelperPath(candidate, nullptr, nullptr)) {
    return "/" + candidate;
  }
  return "";
}

} // namespace primec::semantics
