#include "SemanticsValidator.h"

#include <array>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace primec::semantics {

bool SemanticsValidator::validateExprMutationBorrowBuiltins(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &expr,
    bool &handledOut) {
  handledOut = false;
  auto failMutationBorrowDiagnostic = [&](std::string message) -> bool {
    return failExprDiagnostic(expr, std::move(message));
  };
  auto failBorrowedBindingDiagnostic =
      [&](const std::string &borrowRoot, const std::string &sinkName) -> bool {
        const std::string sink = sinkName.empty() ? borrowRoot : sinkName;
        return failMutationBorrowDiagnostic("borrowed binding: " + borrowRoot +
                                            " (root: " + borrowRoot +
                                            ", sink: " + sink + ")");
      };
  auto isMutableBinding = [&](const std::string &name) -> bool {
    if (const BindingInfo *paramBinding = findParamBinding(params, name)) {
      return paramBinding->isMutable;
    }
    auto it = locals.find(name);
    return it != locals.end() && it->second.isMutable;
  };
  const BuiltinCollectionDispatchResolverAdapters
      builtinCollectionDispatchResolverAdapters;
  const BuiltinCollectionDispatchResolvers builtinCollectionDispatchResolvers =
      makeBuiltinCollectionDispatchResolvers(
          params, locals, builtinCollectionDispatchResolverAdapters);
  auto isVectorOrArrayIndexedTarget = [&](const Expr &target) -> bool {
    auto bindingTargetsVectorOrArray = [&](const BindingInfo &binding,
                                           auto &&bindingTargetsVectorOrArrayRef)
        -> bool {
      const std::string normalizedTypeName =
          normalizeBindingTypeName(binding.typeName);
      if ((normalizedTypeName == "vector" || normalizedTypeName == "array") &&
          !binding.typeTemplateArg.empty()) {
        return true;
      }
      if ((normalizedTypeName != "Reference" &&
           normalizedTypeName != "Pointer") ||
          binding.typeTemplateArg.empty()) {
        return false;
      }
      BindingInfo pointeeBinding;
      const std::string normalizedPointeeType =
          normalizeBindingTypeName(binding.typeTemplateArg);
      std::string pointeeBase;
      std::string pointeeArgs;
      if (splitTemplateTypeName(normalizedPointeeType, pointeeBase,
                                pointeeArgs)) {
        pointeeBinding.typeName = normalizeBindingTypeName(pointeeBase);
        pointeeBinding.typeTemplateArg = pointeeArgs;
      } else {
        pointeeBinding.typeName = normalizedPointeeType;
        pointeeBinding.typeTemplateArg.clear();
      }
      return bindingTargetsVectorOrArrayRef(
          pointeeBinding, bindingTargetsVectorOrArrayRef);
    };
    std::string elemType;
    if (builtinCollectionDispatchResolvers.resolveVectorTarget(target, elemType) ||
        builtinCollectionDispatchResolvers.resolveArrayTarget(target, elemType)) {
      return true;
    }
    if (target.kind == Expr::Kind::Call && target.isFieldAccess &&
        target.args.size() == 1) {
      BindingInfo fieldBinding;
      if (resolveStructFieldBinding(params, locals, target.args.front(),
                                    target.name, fieldBinding) &&
          bindingTargetsVectorOrArray(fieldBinding,
                                      bindingTargetsVectorOrArray)) {
        return true;
      }
    }
    std::string collectionTypePath;
    return resolveCallCollectionTypePath(target, params, locals,
                                         collectionTypePath) &&
           (collectionTypePath == "/vector" || collectionTypePath == "/array");
  };

  auto hasActiveBorrowForBinding =
      [&](const std::string &name,
          const std::string &ignoreBorrowName = std::string()) -> bool {
    if (currentValidationState_.context.definitionIsUnsafe) {
      return false;
    }
    auto isSoaFieldViewBindingType = [&](const BindingInfo &binding) -> bool {
      std::string normalized = normalizeBindingTypeName(binding.typeName);
      if (normalized.empty()) {
        return false;
      }
      std::string base;
      std::string arg;
      if (splitTemplateTypeName(normalized, base, arg)) {
        normalized = normalizeBindingTypeName(base);
      }
      return normalized == "SoaFieldView" ||
             normalized == "std/collections/experimental_soa_storage/SoaFieldView";
    };
    auto referenceRootForBinding =
        [&](const std::string &bindingName,
            const BindingInfo &binding) -> std::string {
      if (binding.typeName != "Reference" &&
          !isSoaFieldViewBindingType(binding)) {
        return "";
      }
      if (!binding.referenceRoot.empty()) {
        return binding.referenceRoot;
      }
      return bindingName;
    };
    auto hasBorrowFrom =
        [&](const std::string &borrowName, const BindingInfo &binding) -> bool {
      if (borrowName == name) {
        return false;
      }
      if (!ignoreBorrowName.empty() && borrowName == ignoreBorrowName) {
        return false;
      }
      if (currentValidationState_.endedReferenceBorrows.count(borrowName) >
          0) {
        return false;
      }
      const std::string root = referenceRootForBinding(borrowName, binding);
      return !root.empty() && root == name;
    };
    for (const auto &param : params) {
      if (hasBorrowFrom(param.name, param.binding)) {
        return true;
      }
    }
    for (const auto &entry : locals) {
      if (hasBorrowFrom(entry.first, entry.second)) {
        return true;
      }
    }
    return false;
  };
  auto findNamedBinding = [&](const std::string &name) -> const BindingInfo * {
    if (const BindingInfo *paramBinding = findParamBinding(params, name)) {
      return paramBinding;
    }
    auto itBinding = locals.find(name);
    if (itBinding != locals.end()) {
      return &itBinding->second;
    }
    return nullptr;
  };
  std::function<bool(const Expr &, std::string &)>
      resolveLocationRootBindingName;
  resolveLocationRootBindingName =
      [&](const Expr &pointerExpr, std::string &rootNameOut) -> bool {
    if (pointerExpr.kind != Expr::Kind::Call) {
      return false;
    }
    std::string pointerBuiltinName;
    if (getBuiltinPointerName(pointerExpr, pointerBuiltinName) &&
        pointerBuiltinName == "location" && pointerExpr.args.size() == 1) {
      if (pointerExpr.args.front().kind != Expr::Kind::Name) {
        return false;
      }
      rootNameOut = pointerExpr.args.front().name;
      return true;
    }
    std::string opName;
    if (!getBuiltinOperatorName(pointerExpr, opName) ||
        (opName != "plus" && opName != "minus") ||
        pointerExpr.args.size() != 2) {
      const std::string nestedResolved = resolveCalleePath(pointerExpr);
      if (nestedResolved.empty()) {
        return false;
      }
      auto nestedIt = defMap_.find(nestedResolved);
      if (nestedIt == defMap_.end() || nestedIt->second == nullptr) {
        return false;
      }
      const Definition &nestedDef = *nestedIt->second;
      const Expr *returnedValueExpr = nullptr;
      for (const auto &stmt : nestedDef.statements) {
        if (isReturnCall(stmt) && stmt.args.size() == 1) {
          returnedValueExpr = &stmt.args.front();
        }
      }
      if (nestedDef.returnExpr.has_value()) {
        returnedValueExpr = &*nestedDef.returnExpr;
      }
      if (returnedValueExpr == nullptr || returnedValueExpr->kind != Expr::Kind::Name) {
        return false;
      }
      const auto paramsIt = paramsByDef_.find(nestedResolved);
      if (paramsIt == paramsByDef_.end()) {
        return false;
      }
      const auto &nestedParams = paramsIt->second;
      std::string nestedArgError;
      std::vector<const Expr *> nestedOrderedArgs;
      if (!buildOrderedArguments(nestedParams, pointerExpr.args, pointerExpr.argNames,
                                 nestedOrderedArgs, nestedArgError)) {
        return false;
      }
      for (size_t nestedIndex = 0;
           nestedIndex < nestedParams.size() && nestedIndex < nestedOrderedArgs.size();
           ++nestedIndex) {
        const auto &nestedParam = nestedParams[nestedIndex];
        const Expr *nestedArg = nestedOrderedArgs[nestedIndex];
        if (nestedArg == nullptr || nestedParam.name != returnedValueExpr->name) {
          continue;
        }
        const std::string normalizedParamType = normalizeBindingTypeName(nestedParam.binding.typeName);
        if (normalizedParamType != "Reference" && normalizedParamType != "Pointer") {
          return false;
        }
        return resolveLocationRootBindingName(*nestedArg, rootNameOut);
      }
      return false;
    }
    if (isPointerLikeExpr(pointerExpr.args[1], params, locals)) {
      return false;
    }
    return resolveLocationRootBindingName(pointerExpr.args.front(),
                                          rootNameOut);
  };
  std::function<bool(const Expr &, std::string &, std::string &)>
      resolveMutablePointerWriteTarget;
  std::function<bool(const Expr &, std::string &, std::string &)>
      resolveMutableExperimentalSoaReceiverTarget;
  resolveMutablePointerWriteTarget = [&](const Expr &pointerExpr,
                                         std::string &borrowRootOut,
                                         std::string &ignoreBorrowNameOut)
      -> bool {
    if (pointerExpr.kind == Expr::Kind::Name) {
      const BindingInfo *pointerBinding = findNamedBinding(pointerExpr.name);
      if (!pointerBinding || !isPointerLikeExpr(pointerExpr, params, locals)) {
        return false;
      }
      ignoreBorrowNameOut = pointerExpr.name;
      if (!pointerBinding->referenceRoot.empty()) {
        borrowRootOut = pointerBinding->referenceRoot;
        return isMutableBinding(pointerBinding->referenceRoot);
      }
      return isMutableBinding(pointerExpr.name);
    }
    if (pointerExpr.kind != Expr::Kind::Call) {
      return false;
    }
    std::string pointerBuiltinName;
    if (getBuiltinPointerName(pointerExpr, pointerBuiltinName) &&
        pointerBuiltinName == "location" && pointerExpr.args.size() == 1) {
      const Expr &locationTarget = pointerExpr.args.front();
      if (locationTarget.kind != Expr::Kind::Name ||
          !isMutableBinding(locationTarget.name)) {
        return false;
      }
      const BindingInfo *targetBinding = findNamedBinding(locationTarget.name);
      if (targetBinding == nullptr) {
        return false;
      }
      if (targetBinding->typeName == "Reference") {
        ignoreBorrowNameOut = locationTarget.name;
        if (!targetBinding->referenceRoot.empty()) {
          borrowRootOut = targetBinding->referenceRoot;
        } else {
          borrowRootOut = locationTarget.name;
        }
      } else {
        borrowRootOut = locationTarget.name;
      }
      return true;
    }
    const std::string resolvedPointerPath = resolveCalleePath(pointerExpr);
    if (resolvedPointerPath.rfind(
            "/std/collections/experimental_soa_storage/soaFieldViewRef", 0) ==
            0 &&
        pointerExpr.args.size() == 2) {
      const Expr &fieldViewExpr = pointerExpr.args.front();
      if (fieldViewExpr.kind != Expr::Kind::Call || fieldViewExpr.isBinding ||
          fieldViewExpr.args.size() != 2) {
        return false;
      }
      const std::string resolvedFieldViewPath = resolveCalleePath(fieldViewExpr);
      if (resolvedFieldViewPath.rfind(
              "/std/collections/experimental_soa_vector/soaVectorFieldView",
              0) != 0) {
        return false;
      }
      return resolveMutableExperimentalSoaReceiverTarget(
          fieldViewExpr.args.front(), borrowRootOut, ignoreBorrowNameOut);
    }
    std::string opName;
    if (!getBuiltinOperatorName(pointerExpr, opName) ||
        (opName != "plus" && opName != "minus") ||
        pointerExpr.args.size() != 2) {
      const std::string nestedResolved = resolveCalleePath(pointerExpr);
      if (nestedResolved.empty()) {
        return false;
      }
      auto nestedIt = defMap_.find(nestedResolved);
      if (nestedIt == defMap_.end() || nestedIt->second == nullptr) {
        return false;
      }
      const Definition &nestedDef = *nestedIt->second;
      const Expr *returnedValueExpr = nullptr;
      for (const auto &stmt : nestedDef.statements) {
        if (isReturnCall(stmt) && stmt.args.size() == 1) {
          returnedValueExpr = &stmt.args.front();
        }
      }
      if (nestedDef.returnExpr.has_value()) {
        returnedValueExpr = &*nestedDef.returnExpr;
      }
      if (returnedValueExpr == nullptr || returnedValueExpr->kind != Expr::Kind::Name) {
        return false;
      }
      const auto paramsIt = paramsByDef_.find(nestedResolved);
      if (paramsIt == paramsByDef_.end()) {
        return false;
      }
      const auto &nestedParams = paramsIt->second;
      std::string nestedArgError;
      std::vector<const Expr *> nestedOrderedArgs;
      if (!buildOrderedArguments(nestedParams, pointerExpr.args, pointerExpr.argNames,
                                 nestedOrderedArgs, nestedArgError)) {
        return false;
      }
      for (size_t nestedIndex = 0;
           nestedIndex < nestedParams.size() && nestedIndex < nestedOrderedArgs.size();
           ++nestedIndex) {
        const auto &nestedParam = nestedParams[nestedIndex];
        const Expr *nestedArg = nestedOrderedArgs[nestedIndex];
        if (nestedArg == nullptr || nestedParam.name != returnedValueExpr->name) {
          continue;
        }
        const std::string normalizedParamType = normalizeBindingTypeName(nestedParam.binding.typeName);
        if (normalizedParamType != "Reference" && normalizedParamType != "Pointer") {
          return false;
        }
        if (resolveMutablePointerWriteTarget(*nestedArg, borrowRootOut, ignoreBorrowNameOut)) {
          return true;
        }
        std::string locationRootName;
        if (resolveLocationRootBindingName(*nestedArg, locationRootName) &&
            isMutableBinding(locationRootName)) {
          borrowRootOut = std::move(locationRootName);
          ignoreBorrowNameOut.clear();
          return true;
        }
        return false;
      }
      return false;
    }
    if (isPointerLikeExpr(pointerExpr.args[1], params, locals)) {
      return false;
    }
    return resolveMutablePointerWriteTarget(pointerExpr.args.front(),
                                            borrowRootOut,
                                            ignoreBorrowNameOut);
  };
  auto resolveExperimentalSoaRefReceiverTarget =
      [&](const Expr &refExpr, const Expr *&receiverTargetOut) -> bool {
    auto hasRefLikeSuffix = [](const std::string &resolvedPath) {
      return (resolvedPath.size() >= 4 &&
              resolvedPath.compare(resolvedPath.size() - 4, 4, "/ref") == 0) ||
             (resolvedPath.size() >= 8 &&
              resolvedPath.compare(resolvedPath.size() - 8, 8, "/ref_ref") == 0);
    };
    auto isBuiltinSoaRefPath = [&](const std::string &resolvedPath,
                                   bool methodForm) -> bool {
      if (resolvedPath.empty()) {
        return false;
      }
      if (resolvedPath.rfind(
              "/std/collections/experimental_soa_vector/soaVectorRef",
              0) == 0) {
        return true;
      }
      if (resolvedPath.rfind(
              "/std/collections/experimental_soa_vector/soaVectorRefRef",
              0) == 0) {
        return true;
      }
      if (methodForm) {
        return resolvedPath.rfind(
                   "/std/collections/experimental_soa_vector/", 0) == 0 &&
               hasRefLikeSuffix(resolvedPath);
      }
      return resolvedPath.rfind("/std/collections/soa_vector/ref", 0) == 0 ||
             resolvedPath.rfind("/soa_vector/ref", 0) == 0 ||
             resolvedPath.rfind("/std/collections/soa_vector/ref_ref", 0) ==
                 0 ||
             resolvedPath.rfind("/soa_vector/ref_ref", 0) == 0;
    };

    receiverTargetOut = nullptr;
    if (refExpr.kind != Expr::Kind::Call || refExpr.args.empty()) {
      return false;
    }
    if (refExpr.isMethodCall) {
      const std::string resolvedMethodPath = resolveCalleePath(refExpr);
      if (refExpr.name != "ref" && refExpr.name != "ref_ref" &&
          !isBuiltinSoaRefPath(resolvedMethodPath, true)) {
        return false;
      }
      if (refExpr.args.size() != 2) {
        return false;
      }
      receiverTargetOut = &refExpr.args.front();
      return true;
    }

    const bool isBareRefCall =
        isSimpleCallName(refExpr, "ref") ||
        isSimpleCallName(refExpr, "ref_ref");
    const bool isCanonicalRefCall =
        refExpr.name.rfind(
            "/std/collections/experimental_soa_vector/soaVectorRef",
            0) == 0 ||
        refExpr.name.rfind(
            "/std/collections/experimental_soa_vector/soaVectorRefRef",
            0) == 0;
    const std::string resolvedCallPath = resolveCalleePath(refExpr);
    if ((!isBareRefCall && !isCanonicalRefCall &&
         !isBuiltinSoaRefPath(resolvedCallPath, false)) ||
        refExpr.args.size() != 2) {
      return false;
    }
    receiverTargetOut = &refExpr.args.front();
    return true;
  };
  resolveMutableExperimentalSoaReceiverTarget =
      [&](const Expr &receiverTarget,
          std::string &borrowRootOut,
          std::string &ignoreBorrowNameOut) -> bool {
    borrowRootOut.clear();
    ignoreBorrowNameOut.clear();

    if (receiverTarget.kind == Expr::Kind::Name) {
      const BindingInfo *receiverBinding = findNamedBinding(receiverTarget.name);
      if (receiverBinding == nullptr) {
        return false;
      }
      std::string elemType;
      if (!extractExperimentalSoaVectorElementType(*receiverBinding, elemType)) {
        return false;
      }
      const std::string normalizedType =
          normalizeBindingTypeName(receiverBinding->typeName);
      if (normalizedType == "Reference" || normalizedType == "Pointer") {
        return resolveMutablePointerWriteTarget(receiverTarget,
                                               borrowRootOut,
                                               ignoreBorrowNameOut);
      }
      borrowRootOut = receiverTarget.name;
      return isMutableBinding(receiverTarget.name);
    }

    if (receiverTarget.kind != Expr::Kind::Call || receiverTarget.isBinding) {
      return false;
    }

    if (isSimpleCallName(receiverTarget, "dereference") &&
        receiverTarget.args.size() == 1) {
      return resolveMutablePointerWriteTarget(receiverTarget.args.front(),
                                             borrowRootOut,
                                             ignoreBorrowNameOut);
    }

    std::string inferredTypeText;
    if (!inferQueryExprTypeText(receiverTarget, params, locals,
                                inferredTypeText) ||
        inferredTypeText.empty()) {
      return false;
    }
    BindingInfo inferredBinding;
    std::string base;
    std::string argText;
    const std::string normalizedTypeText =
        normalizeBindingTypeName(inferredTypeText);
    if (splitTemplateTypeName(normalizedTypeText, base, argText)) {
      inferredBinding.typeName = normalizeBindingTypeName(base);
      inferredBinding.typeTemplateArg = argText;
    } else {
      inferredBinding.typeName = normalizedTypeText;
    }

    std::string elemType;
    if (!extractExperimentalSoaVectorElementType(inferredBinding, elemType)) {
      return false;
    }

    if (normalizeBindingTypeName(inferredBinding.typeName) == "Reference" ||
        normalizeBindingTypeName(inferredBinding.typeName) == "Pointer") {
      if (resolveMutablePointerWriteTarget(receiverTarget,
                                           borrowRootOut,
                                           ignoreBorrowNameOut)) {
        return true;
      }
      if (isSimpleCallName(receiverTarget, "dereference") &&
          receiverTarget.args.size() == 1) {
        return resolveMutablePointerWriteTarget(receiverTarget.args.front(),
                                               borrowRootOut,
                                               ignoreBorrowNameOut);
      }
    }
    return false;
  };

  if (isSimpleCallName(expr, "move")) {
    handledOut = true;
    if (hasNamedArguments(expr.argNames)) {
      return failMutationBorrowDiagnostic(
          "named arguments not supported for builtin calls");
    }
    if (expr.isMethodCall) {
      return failMutationBorrowDiagnostic(
          "move does not support method-call syntax");
    }
    if (!expr.templateArgs.empty()) {
      return failMutationBorrowDiagnostic(
          "move does not accept template arguments");
    }
    if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
      return failMutationBorrowDiagnostic(
          "move does not accept block arguments");
    }
    if (expr.args.size() != 1) {
      return failMutationBorrowDiagnostic("move requires exactly one argument");
    }
    const Expr &target = expr.args.front();
    if (target.kind != Expr::Kind::Name) {
      return failMutationBorrowDiagnostic("move requires a binding name");
    }
    const BindingInfo *binding = findNamedBinding(target.name);
    if (!binding) {
      return failMutationBorrowDiagnostic(
          "move requires a local binding or parameter: " + target.name);
    }
    if (binding->typeName == "Reference") {
      return failMutationBorrowDiagnostic(
          "move does not support Reference bindings: " + target.name);
    }
    if (hasActiveBorrowForBinding(target.name)) {
      return failBorrowedBindingDiagnostic(target.name, target.name);
    }
    if (currentValidationState_.movedBindings.count(target.name) > 0) {
      return failMutationBorrowDiagnostic("use-after-move: " + target.name);
    }
    currentValidationState_.movedBindings.insert(target.name);
    return true;
  }

  if (isAssignCall(expr)) {
    handledOut = true;
    if (expr.args.size() != 2) {
      return failMutationBorrowDiagnostic("assign requires exactly two arguments");
    }
    const Expr &target = expr.args.front();
    auto pendingFieldViewNameFromRewrittenHelper =
        [&](const Expr &candidate) -> std::optional<std::string> {
          if (candidate.kind != Expr::Kind::Call || candidate.args.size() < 2 ||
              candidate.args[1].kind != Expr::Kind::Literal) {
            return std::nullopt;
          }
          const std::string resolvedPath = resolveCalleePath(candidate);
          const bool isFieldViewHelper =
              resolvedPath.rfind(
                  "/std/collections/experimental_soa_vector/soaVectorFieldView",
                  0) == 0 ||
              resolvedPath.rfind(
                  "/std/collections/experimental_soa_storage/soaColumnFieldViewUnsafe",
                  0) == 0;
          if (!isFieldViewHelper) {
            return std::nullopt;
          }
          auto inferStructTypeText = [&]() -> std::optional<std::string> {
            if (!candidate.templateArgs.empty()) {
              return candidate.templateArgs.front();
            }
            if (candidate.args.empty()) {
              return std::nullopt;
            }
            const Expr &receiverExpr = candidate.args.front();
            auto extractReceiverStructType =
                [&](const BindingInfo &binding) -> std::optional<std::string> {
              std::string elemType;
              if (extractExperimentalSoaVectorElementType(binding, elemType)) {
                return elemType;
              }
              return std::nullopt;
            };
            if (receiverExpr.kind == Expr::Kind::Name) {
              if (const BindingInfo *paramBinding =
                      findParamBinding(params, receiverExpr.name)) {
                return extractReceiverStructType(*paramBinding);
              }
              auto localIt = locals.find(receiverExpr.name);
              if (localIt != locals.end()) {
                return extractReceiverStructType(localIt->second);
              }
            }
            BindingInfo receiverBinding;
            if (inferBindingTypeFromInitializer(receiverExpr, params, locals,
                                                receiverBinding)) {
              if (const auto elemType =
                      extractReceiverStructType(receiverBinding)) {
                return elemType;
              }
            }
            std::string inferredTypeText;
            if (inferQueryExprTypeText(receiverExpr, params, locals,
                                       inferredTypeText)) {
              BindingInfo inferredBinding;
              std::string base;
              std::string argText;
              const std::string normalizedType =
                  normalizeBindingTypeName(inferredTypeText);
              if (splitTemplateTypeName(normalizedType, base, argText)) {
                inferredBinding.typeName = normalizeBindingTypeName(base);
                inferredBinding.typeTemplateArg = argText;
              } else {
                inferredBinding.typeName = normalizedType;
                inferredBinding.typeTemplateArg.clear();
              }
              return extractReceiverStructType(inferredBinding);
            }
            return std::nullopt;
          };
          const auto structTypeText = inferStructTypeText();
          if (!structTypeText.has_value()) {
            return std::nullopt;
          }
          std::string currentNamespace;
          if (!currentValidationState_.context.definitionPath.empty()) {
            const size_t slash =
                currentValidationState_.context.definitionPath.find_last_of('/');
            if (slash != std::string::npos && slash > 0) {
              currentNamespace =
                  currentValidationState_.context.definitionPath.substr(0, slash);
            }
          }
          const std::string lookupNamespace =
              !candidate.namespacePrefix.empty() ? candidate.namespacePrefix
                                                : currentNamespace;
          const std::string structPath = resolveStructTypePath(
              normalizeBindingTypeName(*structTypeText),
              lookupNamespace,
              structNames_);
          auto defIt = defMap_.find(structPath);
          if (structPath.empty() || defIt == defMap_.end() ||
              defIt->second == nullptr) {
            return std::nullopt;
          }
          const size_t fieldIndex =
              static_cast<size_t>(candidate.args[1].literalValue);
          size_t currentFieldIndex = 0;
          for (const auto &fieldStmt : defIt->second->statements) {
            bool isStaticField = false;
            for (const auto &transform : fieldStmt.transforms) {
              if (transform.name == "static") {
                isStaticField = true;
                break;
              }
            }
            if (!fieldStmt.isBinding || isStaticField) {
              continue;
            }
            if (currentFieldIndex == fieldIndex) {
              return fieldStmt.name;
            }
            ++currentFieldIndex;
          }
          return std::nullopt;
        };
    auto resolveSoaFieldViewReceiverExpr =
        [&](const Expr &fieldViewExpr,
            std::string *fieldNameOut,
            const Expr *&receiverExprOut) -> bool {
          receiverExprOut = nullptr;
          std::string fieldName;
          if (isBuiltinSoaFieldViewExpr(fieldViewExpr, params, locals,
                                        &fieldName)) {
            if (fieldNameOut != nullptr) {
              *fieldNameOut = fieldName;
            }
            if (!fieldViewExpr.args.empty()) {
              receiverExprOut = &fieldViewExpr.args.front();
              return true;
            }
            return false;
          }
          if (const auto rewrittenFieldName =
                  pendingFieldViewNameFromRewrittenHelper(fieldViewExpr)) {
            if (fieldNameOut != nullptr) {
              *fieldNameOut = *rewrittenFieldName;
            }
            if (!fieldViewExpr.args.empty()) {
              receiverExprOut = &fieldViewExpr.args.front();
              return true;
            }
            return false;
          }
          return false;
        };
    std::string standaloneSoaFieldViewName;
    const bool targetIsBuiltinSoaFieldView =
        isBuiltinSoaFieldViewExpr(target, params, locals,
                                  &standaloneSoaFieldViewName);
    const Expr *standaloneSoaFieldViewReceiver = nullptr;
    if ((targetIsBuiltinSoaFieldView ||
         resolveSoaFieldViewReceiverExpr(target, &standaloneSoaFieldViewName,
                                         standaloneSoaFieldViewReceiver)) &&
        !hasNamedArguments(expr.argNames)) {
      std::string accessName;
      if (!(getBuiltinArrayAccessName(target, accessName) &&
            target.args.size() == 2)) {
        return failMutationBorrowDiagnostic(soaUnavailableMethodDiagnostic(
            soaFieldViewHelperPath(standaloneSoaFieldViewName)));
      }
    }
    const bool targetIsName = target.kind == Expr::Kind::Name;
    auto isLifecycleHelperPath = [](const std::string &fullPath) -> bool {
      static const std::array<std::string_view, 10> suffixes = {
          "/Create",        "/Destroy",        "/Copy",
          "/Move",          "/CreateStack",    "/DestroyStack",
          "/CreateHeap",    "/DestroyHeap",    "/CreateBuffer",
          "/DestroyBuffer"};
      for (std::string_view suffix : suffixes) {
        if (fullPath.size() < suffix.size()) {
          continue;
        }
        if (fullPath.compare(fullPath.size() - suffix.size(), suffix.size(),
                             suffix.data(), suffix.size()) == 0) {
          return true;
        }
      }
      return false;
    };
    auto isNamedFieldTarget =
        [&](const Expr &candidate, std::string_view rootName) -> bool {
      if (candidate.kind != Expr::Kind::Call || !candidate.isFieldAccess ||
          candidate.args.size() != 1) {
        return false;
      }
      const Expr *receiver = &candidate.args.front();
      while (receiver->kind == Expr::Kind::Call && receiver->isFieldAccess &&
             receiver->args.size() == 1) {
        receiver = &receiver->args.front();
      }
      return receiver->kind == Expr::Kind::Name &&
             receiver->name == rootName;
    };
    auto resolveFieldTargetRootName =
        [&](const Expr &candidate, std::string &rootNameOut) -> bool {
      if (candidate.kind != Expr::Kind::Call || !candidate.isFieldAccess ||
          candidate.args.size() != 1) {
        return false;
      }
      const Expr *receiver = &candidate.args.front();
      while (receiver->kind == Expr::Kind::Call && receiver->isFieldAccess &&
             receiver->args.size() == 1) {
        receiver = &receiver->args.front();
      }
      if (receiver->kind != Expr::Kind::Name) {
        return false;
      }
      rootNameOut = receiver->name;
      return true;
    };
    auto validateMutableFieldAccessTarget = [&](const Expr &fieldTarget) -> bool {
      if (fieldTarget.kind != Expr::Kind::Call || !fieldTarget.isFieldAccess ||
          fieldTarget.args.size() != 1) {
        return failMutationBorrowDiagnostic(
            "assign target must be a mutable binding");
      }
      std::string fieldViewName;
      if (isBuiltinSoaFieldViewExpr(fieldTarget, params, locals,
                                    &fieldViewName)) {
        std::string borrowRoot;
        std::string ignoreBorrowName;
        if (!resolveMutableExperimentalSoaReceiverTarget(
                fieldTarget.args.front(), borrowRoot, ignoreBorrowName)) {
          return failMutationBorrowDiagnostic(
              "assign target must be a mutable binding");
        }
        if (!borrowRoot.empty() &&
            hasActiveBorrowForBinding(borrowRoot, ignoreBorrowName)) {
          const std::string borrowSink =
              !ignoreBorrowName.empty() ? ignoreBorrowName : borrowRoot;
          return failBorrowedBindingDiagnostic(borrowRoot, borrowSink);
        }
        return true;
      }
      if (auto pendingPath =
              builtinSoaDirectPendingHelperPath(fieldTarget, params, locals);
          pendingPath.has_value()) {
        return failMutationBorrowDiagnostic(
            soaUnavailableMethodDiagnostic(*pendingPath));
      }
      if (!validateExpr(params, locals, fieldTarget)) {
        return false;
      }
      const Expr &fieldReceiver = fieldTarget.args.front();
      BindingInfo fieldBinding;
      if (!this->resolveStructFieldBinding(params, locals, fieldReceiver,
                                           fieldTarget.name, fieldBinding)) {
        if (error_.empty()) {
          return failMutationBorrowDiagnostic(
              "assign target must be a mutable binding");
        }
        return false;
      }
      std::string fieldTargetRootName;
      const bool allowMutableReceiverFieldWrite =
          resolveFieldTargetRootName(fieldTarget, fieldTargetRootName) &&
          isMutableBinding(fieldTargetRootName);
      std::string mutableBorrowRootName;
      std::string ignoreBorrowName;
      bool allowMutableBorrowedFieldWrite = false;
      const Expr *soaRefReceiverTarget = nullptr;
      if (resolveExperimentalSoaRefReceiverTarget(fieldReceiver,
                                                  soaRefReceiverTarget) &&
          soaRefReceiverTarget != nullptr) {
        allowMutableBorrowedFieldWrite =
            resolveMutableExperimentalSoaReceiverTarget(*soaRefReceiverTarget,
                                                       mutableBorrowRootName,
                                                       ignoreBorrowName);
      }
      const bool allowLifecycleFieldWrite =
          !fieldBinding.isMutable && isNamedFieldTarget(fieldTarget, "this") &&
          isLifecycleHelperPath(currentValidationState_.context.definitionPath);
      if (!fieldBinding.isMutable && !allowLifecycleFieldWrite &&
          !allowMutableReceiverFieldWrite && !allowMutableBorrowedFieldWrite) {
        return failMutationBorrowDiagnostic(
            "assign target must be a mutable binding");
      }
      if (!mutableBorrowRootName.empty() &&
          hasActiveBorrowForBinding(mutableBorrowRootName, ignoreBorrowName)) {
        const std::string borrowSink =
            !ignoreBorrowName.empty() ? ignoreBorrowName : mutableBorrowRootName;
        return failBorrowedBindingDiagnostic(mutableBorrowRootName, borrowSink);
      }
      if (fieldReceiver.kind == Expr::Kind::Name &&
          hasActiveBorrowForBinding(fieldReceiver.name)) {
        return failBorrowedBindingDiagnostic(fieldReceiver.name,
                                            fieldReceiver.name);
      }
      return true;
    };
    if (target.kind == Expr::Kind::Name) {
      if (!isMutableBinding(target.name)) {
        return failMutationBorrowDiagnostic(
            "assign target must be a mutable binding: " + target.name);
      }
      if (hasActiveBorrowForBinding(target.name)) {
        return failBorrowedBindingDiagnostic(target.name, target.name);
      }
    } else if (target.kind == Expr::Kind::Call && target.isFieldAccess) {
      if (!validateMutableFieldAccessTarget(target)) {
        return false;
      }
    } else if (target.kind == Expr::Kind::Call) {
      std::string accessName;
      if (getBuiltinArrayAccessName(target, accessName) &&
          target.args.size() == 2) {
        const Expr &collectionTarget = target.args.front();
        std::string fieldViewName;
        const Expr *fieldViewReceiver = nullptr;
        if (resolveSoaFieldViewReceiverExpr(collectionTarget, &fieldViewName,
                                            fieldViewReceiver)) {
          std::string borrowRoot;
          std::string ignoreBorrowName;
          if (fieldViewReceiver == nullptr ||
              !resolveMutableExperimentalSoaReceiverTarget(
                  *fieldViewReceiver, borrowRoot, ignoreBorrowName)) {
            return failMutationBorrowDiagnostic(
                "assign target must be a mutable binding");
          }
          if (!borrowRoot.empty() &&
              hasActiveBorrowForBinding(borrowRoot, ignoreBorrowName)) {
            const std::string borrowSink =
                !ignoreBorrowName.empty() ? ignoreBorrowName : borrowRoot;
            return failBorrowedBindingDiagnostic(borrowRoot, borrowSink);
          }
          if (!validateExpr(params, locals, target)) {
            return false;
          }
        } else
        if (!isVectorOrArrayIndexedTarget(collectionTarget)) {
          return failMutationBorrowDiagnostic(
              "assign target must be a mutable binding");
        } else {
          if (collectionTarget.kind == Expr::Kind::Name) {
            if (!isMutableBinding(collectionTarget.name)) {
              return failMutationBorrowDiagnostic(
                  "assign target must be a mutable binding: " +
                  collectionTarget.name);
            }
            if (hasActiveBorrowForBinding(collectionTarget.name)) {
              return failBorrowedBindingDiagnostic(collectionTarget.name,
                                                  collectionTarget.name);
            }
          } else if (collectionTarget.kind == Expr::Kind::Call &&
                     collectionTarget.isFieldAccess) {
            if (!validateMutableFieldAccessTarget(collectionTarget)) {
              return false;
            }
          } else {
            return failMutationBorrowDiagnostic(
                "assign target must be a mutable binding");
          }
          if (!validateExpr(params, locals, target)) {
            return false;
          }
        }
      } else {
        std::string pointerName;
        if (!getBuiltinPointerName(target, pointerName) ||
            pointerName != "dereference" || target.args.size() != 1) {
          return failMutationBorrowDiagnostic(
              "assign target must be a mutable binding");
        }
        const Expr &pointerExpr = target.args.front();
        if (pointerExpr.kind == Expr::Kind::Name &&
            !isMutableBinding(pointerExpr.name)) {
          return failMutationBorrowDiagnostic(
              "assign target must be a mutable binding");
        }
        std::string pointerBorrowRoot;
        std::string ignoreBorrowName;
        if (!resolveMutablePointerWriteTarget(pointerExpr, pointerBorrowRoot,
                                             ignoreBorrowName)) {
          if (pointerExpr.kind == Expr::Kind::Name &&
              !isMutableBinding(pointerExpr.name)) {
            return failMutationBorrowDiagnostic(
                "assign target must be a mutable binding");
          }
          std::string locationRootName;
          if (resolveLocationRootBindingName(pointerExpr, locationRootName) &&
              !isMutableBinding(locationRootName)) {
            return failMutationBorrowDiagnostic(
                "assign target must be a mutable binding: " + locationRootName);
          }
          return failMutationBorrowDiagnostic(
              "assign target must be a mutable pointer binding");
        }
        if (!pointerBorrowRoot.empty() &&
            hasActiveBorrowForBinding(pointerBorrowRoot, ignoreBorrowName)) {
          const std::string borrowSink =
              !ignoreBorrowName.empty() ? ignoreBorrowName : pointerBorrowRoot;
          return failBorrowedBindingDiagnostic(pointerBorrowRoot, borrowSink);
        }
        std::string escapeSink;
        bool hasEscapeSink = false;
        if (pointerExpr.kind == Expr::Kind::Name) {
          hasEscapeSink =
              resolveReferenceEscapeSink(params, locals, pointerExpr.name,
                                         escapeSink);
        }
        if (!hasEscapeSink) {
          std::string locationRootName;
          if (resolveLocationRootBindingName(pointerExpr, locationRootName)) {
            hasEscapeSink =
                resolveReferenceEscapeSink(params, locals, locationRootName,
                                           escapeSink);
          }
        }
        if (!hasEscapeSink && !pointerBorrowRoot.empty()) {
          if (const BindingInfo *rootParam =
                  findParamBinding(params, pointerBorrowRoot);
              rootParam != nullptr && rootParam->typeName == "Reference") {
            hasEscapeSink = true;
            escapeSink = pointerBorrowRoot;
          }
        }
        if (currentValidationState_.context.definitionIsUnsafe) {
          if (isUnsafeReferenceExpr(params, locals, expr.args[1]) &&
              hasEscapeSink &&
              reportReferenceAssignmentEscape(params, locals, escapeSink,
                                              expr.args[1])) {
            return false;
          }
        } else if (hasEscapeSink &&
                   reportReferenceAssignmentEscape(params, locals, escapeSink,
                                                   expr.args[1])) {
          return false;
        }
        if (!validateExpr(params, locals, pointerExpr)) {
          return false;
        }
      }
    } else {
      return failMutationBorrowDiagnostic(
          "assign target must be a mutable binding");
    }
    if (targetIsName) {
      std::string escapeSink;
      if (resolveReferenceEscapeSink(params, locals, target.name, escapeSink)) {
        if (currentValidationState_.context.definitionIsUnsafe) {
          if (isUnsafeReferenceExpr(params, locals, expr.args[1]) &&
              reportReferenceAssignmentEscape(params, locals, escapeSink,
                                              expr.args[1])) {
            return false;
          }
        } else if (reportReferenceAssignmentEscape(params, locals, escapeSink,
                                                   expr.args[1])) {
          return false;
        }
      }
    }
    if (!validateExpr(params, locals, expr.args[1])) {
      return false;
    }
    if (targetIsName) {
      currentValidationState_.movedBindings.erase(target.name);
    }
    return true;
  }

  std::string mutateName;
  if (getBuiltinMutationName(expr, mutateName)) {
    handledOut = true;
    if (expr.args.size() != 1) {
      return failMutationBorrowDiagnostic(mutateName +
                                          " requires exactly one argument");
    }
    const Expr &target = expr.args.front();
    if (target.kind == Expr::Kind::Name) {
      if (!isMutableBinding(target.name)) {
        return failMutationBorrowDiagnostic(
            mutateName + " target must be a mutable binding: " + target.name);
      }
      if (hasActiveBorrowForBinding(target.name)) {
        return failBorrowedBindingDiagnostic(target.name, target.name);
      }
    } else if (target.kind == Expr::Kind::Call) {
      std::string pointerName;
      if (!getBuiltinPointerName(target, pointerName) ||
          pointerName != "dereference" || target.args.size() != 1) {
        return failMutationBorrowDiagnostic(mutateName +
                                            " target must be a mutable binding");
      }
      const Expr &pointerExpr = target.args.front();
      std::string pointerBorrowRoot;
      std::string ignoreBorrowName;
      if (!resolveMutablePointerWriteTarget(pointerExpr, pointerBorrowRoot,
                                           ignoreBorrowName)) {
        if (pointerExpr.kind == Expr::Kind::Name &&
            !isMutableBinding(pointerExpr.name)) {
          return failMutationBorrowDiagnostic(
              mutateName + " target must be a mutable binding");
        }
        std::string locationRootName;
        if (resolveLocationRootBindingName(pointerExpr, locationRootName) &&
            !isMutableBinding(locationRootName)) {
          return failMutationBorrowDiagnostic(
              mutateName + " target must be a mutable binding: " +
              locationRootName);
        }
        return failMutationBorrowDiagnostic(
            mutateName + " target must be a mutable pointer binding");
      }
      if (!pointerBorrowRoot.empty() &&
          hasActiveBorrowForBinding(pointerBorrowRoot, ignoreBorrowName)) {
        const std::string borrowSink =
            !ignoreBorrowName.empty() ? ignoreBorrowName : pointerBorrowRoot;
        return failBorrowedBindingDiagnostic(pointerBorrowRoot, borrowSink);
      }
      if (!validateExpr(params, locals, pointerExpr)) {
        return false;
      }
    } else {
      return failMutationBorrowDiagnostic(mutateName +
                                          " target must be a mutable binding");
    }
    if (!isNumericExpr(params, locals, target)) {
      return failMutationBorrowDiagnostic(mutateName +
                                          " requires numeric operand");
    }
    return true;
  }

  return true;
}

} // namespace primec::semantics
