#include "SemanticsValidator.h"

#include <array>
#include <cctype>
#include <functional>
#include <memory>
#include <optional>
#include <sstream>
#include <string_view>
#include <unordered_set>
#include <utility>

#include "SemanticsValidatorInferCollectionCompatibilityInternal.h"

namespace primec::semantics {

namespace {

bool extractBuiltinSoaVectorElementTypeFromTypeTextForQueryInference(
    const std::string &typeText,
    std::string &elemTypeOut) {
  elemTypeOut.clear();
  const std::string normalizedType = normalizeBindingTypeName(typeText);
  std::string base;
  std::string argText;
  if (!splitTemplateTypeName(normalizedType, base, argText)) {
    return false;
  }
  base = normalizeBindingTypeName(base);
  if ((base == "soa_vector" || isExperimentalSoaVectorTypePath(base)) &&
      !argText.empty()) {
    std::vector<std::string> args;
    if (!splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
      return false;
    }
    elemTypeOut = args.front();
    return true;
  }
  if ((base != "Reference" && base != "Pointer") || argText.empty()) {
    return false;
  }
  std::string wrappedBase;
  std::string wrappedArgText;
  if (!splitTemplateTypeName(argText, wrappedBase, wrappedArgText)) {
    return false;
  }
  wrappedBase = normalizeBindingTypeName(wrappedBase);
  if ((wrappedBase != "soa_vector" && !isExperimentalSoaVectorTypePath(wrappedBase)) ||
      wrappedArgText.empty()) {
    return false;
  }
  std::vector<std::string> wrappedArgs;
  if (!splitTopLevelTemplateArgs(wrappedArgText, wrappedArgs) ||
      wrappedArgs.size() != 1) {
    return false;
  }
  elemTypeOut = wrappedArgs.front();
  return true;
}

} // namespace

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
  std::function<bool(const std::string &)> isConcreteReturnTypeText;
  std::function<bool(const std::string &)> returnTypeReferencesTemplateParam;
  isConcreteReturnTypeText = [&](const std::string &typeText) -> bool {
    const std::string normalized = normalizeBindingTypeName(typeText);
    if (normalized.empty()) {
      return false;
    }
    if (returnKindForTypeName(normalized) != ReturnKind::Unknown) {
      return true;
    }
    if (!normalizeCollectionTypePath(normalized).empty()) {
      return true;
    }
    if (!resolveStructTypePath(normalized, def.namespacePrefix, structNames_).empty()) {
      return true;
    }
    std::string base;
    std::string argText;
    if (!splitTemplateTypeName(normalized, base, argText)) {
      return false;
    }
    base = normalizeBindingTypeName(base);
    if (base.empty()) {
      return false;
    }
    if (base == "Pointer" || base == "Reference" || base == "Result" ||
        base == "Buffer" || base == "uninitialized" || base == "array" ||
        base == "vector" || base == "soa_vector" || isMapCollectionTypeName(base) ||
        base == "Vector" || base == "std/collections/experimental_vector/Vector" ||
        base == "Map" || base == "std/collections/experimental_map/Map" ||
        !resolveStructTypePath(base, def.namespacePrefix, structNames_).empty()) {
      std::vector<std::string> args;
      if (!splitTopLevelTemplateArgs(argText, args) || args.empty()) {
        return false;
      }
      for (const auto &arg : args) {
        if (!isConcreteReturnTypeText(arg)) {
          return false;
        }
      }
      return true;
    }
    return false;
  };
  returnTypeReferencesTemplateParam = [&](const std::string &typeText) -> bool {
    const std::string normalized = normalizeBindingTypeName(typeText);
    if (normalized.empty()) {
      return false;
    }
    for (const auto &templateArg : def.templateArgs) {
      if (normalizeBindingTypeName(templateArg) == normalized) {
        return true;
      }
    }
    std::string base;
    std::string argText;
    if (!splitTemplateTypeName(normalized, base, argText)) {
      return false;
    }
    if (returnTypeReferencesTemplateParam(base)) {
      return true;
    }
    std::vector<std::string> args;
    if (!splitTopLevelTemplateArgs(argText, args)) {
      return false;
    }
    for (const auto &arg : args) {
      if (returnTypeReferencesTemplateParam(arg)) {
        return true;
      }
    }
    return false;
  };

  const bool isSpecializedDefinition = def.fullPath.find("__t") != std::string::npos;
  bool sawReturnTransform = false;
  bool sawUnusableReturnTransform = false;
  for (const auto &transform : def.transforms) {
    if (transform.name != "return" || transform.templateArgs.size() != 1) {
      continue;
    }
    sawReturnTransform = true;
    const std::string &returnType = transform.templateArgs.front();
    if (returnType == "auto") {
      sawUnusableReturnTransform = true;
      break;
    }
    if (isSpecializedDefinition) {
      const bool referencesTemplateParam = returnTypeReferencesTemplateParam(returnType);
      const bool isConcreteReturnType = isConcreteReturnTypeText(returnType);
      if (referencesTemplateParam || !isConcreteReturnType) {
        std::string base;
        std::string argText;
        if (splitTemplateTypeName(normalizeBindingTypeName(returnType), base, argText) &&
            !base.empty()) {
          return parseTypeText(returnType, bindingOut);
        }
        sawUnusableReturnTransform = true;
        break;
      }
    }
    return parseTypeText(returnType, bindingOut);
  }
  if (isSpecializedDefinition && (!sawReturnTransform || sawUnusableReturnTransform)) {
    const std::string &path = def.fullPath;
    const size_t suffix = path.find("__t");
    if (suffix != std::string::npos) {
      std::string basePath = path.substr(0, suffix);
      const size_t nextSlash = path.find('/', suffix);
      if (nextSlash != std::string::npos) {
        basePath += path.substr(nextSlash);
      }
      auto tryBase = [&](const std::string &candidate) -> bool {
        auto baseIt = defMap_.find(candidate);
        if (baseIt == defMap_.end() || baseIt->second == nullptr) {
          return false;
        }
        for (const auto &transform : baseIt->second->transforms) {
          if (transform.name != "return" || transform.templateArgs.size() != 1) {
            continue;
          }
          const std::string &returnType = transform.templateArgs.front();
          if (returnType == "auto") {
            break;
          }
          if (isConcreteReturnTypeText(returnType)) {
            return parseTypeText(returnType, bindingOut);
          }
          break;
        }
        return false;
      };
      if (tryBase(basePath)) {
        return true;
      }
      if (!basePath.empty() && basePath.front() == '/') {
        if (tryBase(basePath.substr(1))) {
          return true;
        }
      } else {
        if (tryBase("/" + basePath)) {
          return true;
        }
      }
    }
  }

  auto cachedBindingIt = returnBindings_.find(def.fullPath);
  if (cachedBindingIt != returnBindings_.end() && !cachedBindingIt->second.typeName.empty()) {
    bindingOut = cachedBindingIt->second;
    return true;
  }

  if (!returnBindingInferenceStack_.insert(def.fullPath).second) {
    return false;
  }
  struct ReturnBindingInferenceScopeGuard {
    std::unordered_set<std::string> &stack;
    const std::string &path;
    ~ReturnBindingInferenceScopeGuard() {
      stack.erase(path);
    }
  } returnBindingInferenceGuard{returnBindingInferenceStack_, def.fullPath};

  ValidationStateScope validationContextScope(*this, buildDefinitionValidationState(def));

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
      if (parseBindingInfo(stmt, def.namespacePrefix, structNames_, importAliases_, binding, restrictType, parseError,
                           &sumNames_)) {
        const bool hasExplicitType = hasExplicitBindingTypeTransform(stmt);
        const bool explicitAutoType = hasExplicitType && normalizeBindingTypeName(binding.typeName) == "auto";
        if (stmt.args.size() == 1 && (!hasExplicitType || explicitAutoType)) {
          (void)inferBindingTypeFromInitializer(stmt.args.front(), defParams, defLocals, binding, &stmt);
        }
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
  error_.clear();
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
  auto inferOldSurfaceSoaToAosTypeTextWithoutDispatchResolvers =
      [&](const Expr &candidate) -> bool {
    if (candidate.kind != Expr::Kind::Call || candidate.args.size() != 1) {
      return false;
    }
    const std::string resolvedCandidate = resolveCalleePath(candidate);
    const bool isBareToAosCall =
        isSimpleCallName(candidate, "to_aos") ||
        isSimpleCallName(candidate, "to_aos_ref");
    const bool isRootToAosDirectCall =
        resolvedCandidate == "/to_aos" ||
        resolvedCandidate == "/to_aos_ref";
    const bool isRootToAosMethodCall =
        candidate.isMethodCall &&
        (candidate.name == "to_aos" || candidate.name == "to_aos_ref" ||
         candidate.name == "/to_aos" || candidate.name == "/to_aos_ref");
    if (!isBareToAosCall && !isRootToAosDirectCall && !isRootToAosMethodCall) {
      return false;
    }
    const std::string samePathHelper =
        (resolvedCandidate == "/to_aos_ref" ||
         isSimpleCallName(candidate, "to_aos_ref") ||
         candidate.name == "to_aos_ref" ||
         candidate.name == "/to_aos_ref")
            ? "/to_aos_ref"
            : "/to_aos";
    if (hasVisibleDefinitionPathForCurrentImports(samePathHelper)) {
      return false;
    }
    std::function<bool(const Expr &, std::string &)> resolveReceiverTypeText =
        [&](const Expr &receiver, std::string &receiverTypeTextOut) -> bool {
      receiverTypeTextOut.clear();
      if (receiver.kind == Expr::Kind::Name) {
        return resolveBindingTypeText(receiver.name, receiverTypeTextOut);
      }
      if (isSimpleCallName(receiver, "location") && receiver.args.size() == 1 &&
          receiver.args.front().kind == Expr::Kind::Name) {
        std::string pointeeTypeText;
        if (!resolveBindingTypeText(receiver.args.front().name, pointeeTypeText) ||
            pointeeTypeText.empty()) {
          return false;
        }
        receiverTypeTextOut = "Reference<" + pointeeTypeText + ">";
        return true;
      }
      if (isSimpleCallName(receiver, "dereference") && receiver.args.size() == 1) {
        std::string wrappedTypeText;
        if (!resolveReceiverTypeText(receiver.args.front(), wrappedTypeText) ||
            wrappedTypeText.empty()) {
          return false;
        }
        receiverTypeTextOut = unwrapReferencePointerTypeText(wrappedTypeText);
        return !receiverTypeTextOut.empty();
      }
      return false;
    };
    std::string receiverTypeText;
    if (!resolveReceiverTypeText(candidate.args.front(), receiverTypeText)) {
      return false;
    }
    std::string elemType;
    if (!extractBuiltinSoaVectorElementTypeFromTypeTextForQueryInference(receiverTypeText, elemType) ||
        elemType.empty()) {
      return false;
    }
    typeTextOut = "/std/collections/experimental_vector/Vector<" + elemType + ">";
    return true;
  };
  if (inferOldSurfaceSoaToAosTypeTextWithoutDispatchResolvers(expr)) {
    return true;
  }
  BuiltinCollectionDispatchResolverAdapters builtinCollectionDispatchResolverAdapters;
  const BuiltinCollectionDispatchResolvers builtinCollectionDispatchResolvers =
      makeBuiltinCollectionDispatchResolvers(params, locals, builtinCollectionDispatchResolverAdapters);
  std::function<bool(const Expr &, std::string &)> inferExprTypeText;
  inferExprTypeText = [&](const Expr &candidate, std::string &currentTypeTextOut) -> bool {
    currentTypeTextOut.clear();
    if (!queryTypeInferenceExprStack_.insert(&candidate).second) {
      return false;
    }
    struct ExprTypeScopeGuard {
      std::unordered_set<const Expr *> &stack;
      const Expr *expr = nullptr;
      ~ExprTypeScopeGuard() {
        if (expr != nullptr) {
          stack.erase(expr);
        }
      }
    } exprGuard{queryTypeInferenceExprStack_, &candidate};
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
      std::unordered_map<std::string, BindingInfo> ifBranchLocals = locals;
      auto inferIfBranchTypeText = [&](const Expr &branchExpr, std::string &branchTypeTextOut) -> bool {
        branchTypeTextOut.clear();
        if (!this->isEnvelopeValueExpr(branchExpr, true)) {
          return inferExprTypeText(branchExpr, branchTypeTextOut);
        }
        LocalBindingScope branchScope(*this, ifBranchLocals);
        const Expr *valueExpr = nullptr;
        bool sawReturn = false;
        for (const auto &bodyExpr : branchExpr.bodyArguments) {
          if (isSyntheticBlockValueBinding(bodyExpr)) {
            if (!sawReturn) {
              valueExpr = &bodyExpr.args.front();
            }
            continue;
          }
          if (bodyExpr.isBinding) {
            BindingInfo binding;
            std::optional<std::string> restrictType;
            if (!parseBindingInfo(bodyExpr,
                                  branchExpr.namespacePrefix,
                                  structNames_,
                                  importAliases_,
                                  binding,
                                  restrictType,
                                  error_)) {
              return false;
            }
            const bool hasExplicitType = hasExplicitBindingTypeTransform(bodyExpr);
            const bool explicitAutoType =
                hasExplicitType && normalizeBindingTypeName(binding.typeName) == "auto";
            if (bodyExpr.args.size() == 1 && (!hasExplicitType || explicitAutoType)) {
              (void)inferBindingTypeFromInitializer(
                  bodyExpr.args.front(), params, ifBranchLocals, binding, &bodyExpr);
            }
            if (restrictType.has_value()) {
              const bool hasTemplate = !binding.typeTemplateArg.empty();
              if (!restrictMatchesBinding(*restrictType,
                                          binding.typeName,
                                          binding.typeTemplateArg,
                                          hasTemplate,
                                          branchExpr.namespacePrefix)) {
                return false;
              }
            }
            insertLocalBinding(ifBranchLocals, bodyExpr.name, std::move(binding));
            continue;
          }
          if (isReturnCall(bodyExpr) && bodyExpr.args.size() == 1) {
            valueExpr = &bodyExpr.args.front();
            sawReturn = true;
            continue;
          }
          if (!sawReturn) {
            valueExpr = &bodyExpr;
          }
        }
        if (valueExpr == nullptr) {
          return false;
        }
        return inferQueryExprTypeText(*valueExpr, params, ifBranchLocals, branchTypeTextOut);
      };
      std::string thenTypeText;
      std::string elseTypeText;
      if (!inferIfBranchTypeText(thenArg, thenTypeText) ||
          !inferIfBranchTypeText(elseArg, elseTypeText)) {
        return false;
      }
      if (normalizeBindingTypeName(thenTypeText) != normalizeBindingTypeName(elseTypeText)) {
        const ReturnKind thenKind = returnKindForTypeName(normalizeBindingTypeName(thenTypeText));
        const ReturnKind elseKind = returnKindForTypeName(normalizeBindingTypeName(elseTypeText));
        const ReturnKind widenedKind = combineInferredNumericKinds(thenKind, elseKind);
        if (widenedKind == ReturnKind::Unknown || widenedKind == ReturnKind::Array || widenedKind == ReturnKind::Void) {
          return false;
        }
        currentTypeTextOut = typeNameForReturnKind(widenedKind);
        return !currentTypeTextOut.empty();
      }
      currentTypeTextOut = thenTypeText;
      return true;
    }
    if (const Expr *valueExpr = this->getEnvelopeValueExpr(candidate, false)) {
      return inferExprTypeText(*valueExpr, currentTypeTextOut);
    }
    if (candidate.kind != Expr::Kind::Call) {
      return false;
    }
    BindingInfo sumConstructorBinding;
    if (inferExplicitSumConstructorBinding(candidate, sumConstructorBinding)) {
      currentTypeTextOut = bindingTypeText(sumConstructorBinding);
      return !currentTypeTextOut.empty();
    }
    if (candidate.isFieldAccess && candidate.args.size() == 1) {
      BindingInfo fieldBinding;
      if (!resolveStructFieldBinding(params, locals, candidate.args.front(), candidate.name, fieldBinding)) {
        return false;
      }
      currentTypeTextOut = bindingTypeText(fieldBinding);
      error_.clear();
      return !currentTypeTextOut.empty();
    }
    if (isSimpleCallName(candidate, "move") && candidate.args.size() == 1) {
      return inferExprTypeText(candidate.args.front(), currentTypeTextOut);
    }
    if (isAssignCall(candidate) && candidate.args.size() == 2) {
      return inferExprTypeText(candidate.args[1], currentTypeTextOut);
    }
    if (isSimpleCallName(candidate, "dereference") && candidate.args.size() == 1) {
      std::string wrappedTypeText;
      if (!inferExprTypeText(candidate.args.front(), wrappedTypeText)) {
        return false;
      }
      currentTypeTextOut = unwrapReferencePointerTypeText(wrappedTypeText);
      return !currentTypeTextOut.empty();
    }
    const std::string resolvedCandidate = resolveCalleePath(candidate);
    auto inferOldSurfaceSoaToAosTypeText = [&]() -> bool {
      if (candidate.args.size() != 1) {
        return false;
      }
      const bool isBareToAosCall =
          isSimpleCallName(candidate, "to_aos") ||
          isSimpleCallName(candidate, "to_aos_ref");
      const bool isRootToAosDirectCall =
          resolvedCandidate == "/to_aos" ||
          resolvedCandidate == "/to_aos_ref";
      const bool isRootToAosMethodCall =
          candidate.isMethodCall &&
          (candidate.name == "to_aos" || candidate.name == "to_aos_ref" ||
           candidate.name == "/to_aos" || candidate.name == "/to_aos_ref");
      if (!isBareToAosCall && !isRootToAosDirectCall && !isRootToAosMethodCall) {
        return false;
      }
      const std::string samePathHelper =
          (resolvedCandidate == "/to_aos_ref" ||
           isSimpleCallName(candidate, "to_aos_ref") ||
           candidate.name == "to_aos_ref" ||
           candidate.name == "/to_aos_ref")
              ? "/to_aos_ref"
              : "/to_aos";
      if (hasVisibleDefinitionPathForCurrentImports(samePathHelper)) {
        return false;
      }
      std::string receiverTypeText;
      if (!inferExprTypeText(candidate.args.front(), receiverTypeText)) {
        return false;
      }
      std::string elemType;
      if (!extractBuiltinSoaVectorElementTypeFromTypeTextForQueryInference(receiverTypeText, elemType) ||
          elemType.empty()) {
        return false;
      }
      currentTypeTextOut = "/std/collections/experimental_vector/Vector<" + elemType + ">";
      return true;
    };
    if (inferOldSurfaceSoaToAosTypeText()) {
      return true;
    }
    if (candidate.args.size() == 1) {
      ReturnKind builtinMethodKind = ReturnKind::Unknown;
      if (resolveBuiltinCollectionMethodReturnKind(
              resolvedCandidate,
              candidate.args.front(),
              builtinCollectionDispatchResolvers,
              builtinMethodKind) &&
          builtinMethodKind != ReturnKind::Unknown &&
          builtinMethodKind != ReturnKind::Void &&
          builtinMethodKind != ReturnKind::Array) {
        currentTypeTextOut = typeNameForReturnKind(builtinMethodKind);
        return !currentTypeTextOut.empty();
      }
    }
    std::string resolvedSoaCanonical =
        canonicalizeLegacySoaGetHelperPath(resolvedCandidate);
    const auto soaAccessHelper =
        candidate.args.size() == 2
            ? builtinSoaAccessHelperName(candidate, params, locals)
            : std::nullopt;
    if (soaAccessHelper.has_value()) {
      const bool oldSurfaceCallShape =
          (*soaAccessHelper == "get" &&
           (isSimpleCallName(candidate, "get") ||
            (candidate.isMethodCall && candidate.name == "get") ||
            isLegacyOrCanonicalSoaHelperPath(resolvedSoaCanonical, "get"))) ||
          (*soaAccessHelper == "get_ref" &&
           (isSimpleCallName(candidate, "get_ref") ||
            (candidate.isMethodCall && candidate.name == "get_ref") ||
            isLegacyOrCanonicalSoaHelperPath(resolvedSoaCanonical,
                                             "get_ref"))) ||
          ((*soaAccessHelper == "ref" || *soaAccessHelper == "ref_ref") &&
           (((*soaAccessHelper == "ref" &&
              isSimpleCallName(candidate, "ref")) ||
             (*soaAccessHelper == "ref_ref" &&
              isSimpleCallName(candidate, "ref_ref"))) ||
            (candidate.isMethodCall && candidate.name == *soaAccessHelper) ||
            isLegacyOrCanonicalSoaHelperPath(resolvedSoaCanonical,
                                             *soaAccessHelper)));
      if (!(hasVisibleDefinitionPathForCurrentImports("/soa_vector/" +
                                                      *soaAccessHelper) &&
            oldSurfaceCallShape)) {
        std::string elemType;
        if (builtinCollectionDispatchResolvers.resolveSoaVectorTarget(candidate.args.front(), elemType)) {
          currentTypeTextOut = normalizeBindingTypeName(elemType);
          return !currentTypeTextOut.empty();
        }
      }
    }
    std::string builtinAccessName;
    if (getBuiltinArrayAccessName(candidate, builtinAccessName) && candidate.args.size() == 2) {
      std::string normalizedAccessName = candidate.name;
      if (!normalizedAccessName.empty() && normalizedAccessName.front() == '/') {
        normalizedAccessName.erase(normalizedAccessName.begin());
      }
      const size_t accessTemplateSuffix = normalizedAccessName.find("__t");
      if (accessTemplateSuffix != std::string::npos) {
        normalizedAccessName.erase(accessTemplateSuffix);
      }
      const bool isExplicitAccessAlias =
          normalizedAccessName.find('/') != std::string::npos;
      const Expr &receiver =
          candidate.isMethodCall ? candidate.args.front()
                                 : (candidate.args.empty() ? candidate : candidate.args.front());
      std::string elemType;
      std::string keyType;
      std::string valueType;
      if (builtinCollectionDispatchResolvers.resolveVectorTarget(receiver, elemType) ||
          builtinCollectionDispatchResolvers.resolveArgsPackAccessTarget(receiver, elemType) ||
          builtinCollectionDispatchResolvers.resolveArrayTarget(receiver, elemType) ||
          builtinCollectionDispatchResolvers.resolveSoaVectorTarget(receiver, elemType)) {
        currentTypeTextOut = normalizeBindingTypeName(elemType);
        return !currentTypeTextOut.empty();
      }
      if (builtinCollectionDispatchResolvers.resolveStringTarget(receiver)) {
        currentTypeTextOut = "i32";
        return true;
      }
      if (candidate.isMethodCall && !isExplicitAccessAlias) {
        std::string resolvedMethodTarget;
        bool isBuiltinMethod = false;
        if (resolveMethodTarget(params, locals, candidate.namespacePrefix, receiver, candidate.name,
                                resolvedMethodTarget, isBuiltinMethod)) {
          auto resolvedMethodIt = defMap_.find(resolvedMethodTarget);
          if (resolvedMethodIt != defMap_.end() && resolvedMethodIt->second != nullptr) {
            for (const auto &transform : resolvedMethodIt->second->transforms) {
              if (transform.name != "return" || transform.templateArgs.size() != 1 ||
                  transform.templateArgs.front() == "auto") {
                continue;
              }
              currentTypeTextOut = transform.templateArgs.front();
              return !currentTypeTextOut.empty();
            }
          }
        }
      }
      if (!isExplicitAccessAlias &&
          (builtinCollectionDispatchResolvers.resolveMapTarget(receiver, keyType, valueType) ||
           builtinCollectionDispatchResolvers.resolveExperimentalMapTarget(receiver, keyType, valueType))) {
        currentTypeTextOut = normalizeBindingTypeName(valueType);
        return !currentTypeTextOut.empty();
      }
    }

    auto canonicalizeResolvedPath = [](std::string path) {
      const size_t suffix = path.find("__t");
      if (suffix != std::string::npos) {
        path.erase(suffix);
      }
      return path;
    };
    auto hasDirectExperimentalVectorImport = [&]() {
      const auto &importPaths = program_.sourceImports.empty() ? program_.imports : program_.sourceImports;
      for (const auto &importPath : importPaths) {
        if (importPath == "/std/collections/experimental_vector/*" ||
            importPath == "/std/collections/experimental_vector/vector" ||
            importPath == "/std/collections/experimental_vector") {
          return true;
        }
      }
      return false;
    };
    auto isImportedExperimentalVectorConstructorPath =
        [&](std::string resolvedPath) {
          resolvedPath = canonicalizeResolvedPath(std::move(resolvedPath));
          return resolvedPath == "/std/collections/experimental_vector/vector" ||
                 (resolvedPath == "/vector" &&
                  hasDirectExperimentalVectorImport());
        };
    const bool prefersImportedExperimentalVectorConstructor =
        !candidate.isMethodCall &&
        candidate.templateArgs.size() == 1 &&
        [&]() {
          if (candidate.name == "vector" && candidate.namespacePrefix.empty()) {
            auto aliasIt = importAliases_.find(candidate.name);
            if (aliasIt != importAliases_.end() &&
                canonicalizeResolvedPath(aliasIt->second) == "/std/collections/experimental_vector/vector") {
              return true;
            }
            if (hasDirectExperimentalVectorImport()) {
              return true;
            }
          }
          return isImportedExperimentalVectorConstructorPath(
              resolvedCandidate);
        }();
    if (prefersImportedExperimentalVectorConstructor) {
      currentTypeTextOut = "Vector<" + candidate.templateArgs.front() + ">";
      return true;
    }
    const std::string canonicalResolvedCandidate = canonicalizeResolvedPath(resolvedCandidate);
    if (canonicalResolvedCandidate == "/vector" &&
        !hasDirectExperimentalVectorImport() &&
        candidate.templateArgs.size() == 1) {
      currentTypeTextOut = "vector<" + candidate.templateArgs.front() + ">";
      return true;
    }
    std::string collection;
    if (getBuiltinCollectionName(candidate, collection)) {
      const bool preferResolvedCollectionDefinition =
          !canonicalResolvedCandidate.empty() &&
          canonicalResolvedCandidate != "/" + collection &&
          defMap_.count(canonicalResolvedCandidate) != 0;
      if (preferResolvedCollectionDefinition) {
        // Imported stdlib collection constructors should infer from their declared return type
        // instead of collapsing to the legacy builtin collection surface.
      } else if ((collection == "array" || collection == "vector" || collection == "soa_vector") &&
                 candidate.templateArgs.size() == 1) {
        currentTypeTextOut = collection + "<" + candidate.templateArgs.front() + ">";
        return true;
      } else if (collection == "map" && candidate.templateArgs.size() == 2) {
        currentTypeTextOut = "map<" + candidate.templateArgs[0] + ", " + candidate.templateArgs[1] + ">";
        return true;
      }
    }
    auto preferredResolvedCandidate = [&]() -> std::string {
      if (const std::string preferredCollectionHelper =
              preferredCollectionHelperResolvedPath(candidate);
          !preferredCollectionHelper.empty()) {
        const std::string concretePreferredCollectionHelper =
            resolveExprConcreteCallPath(
                params, locals, candidate, preferredCollectionHelper);
        if (!concretePreferredCollectionHelper.empty()) {
          return concretePreferredCollectionHelper;
        }
        return preferredCollectionHelper;
      }
      std::string normalizedName = candidate.name;
      if (!normalizedName.empty() && normalizedName.front() == '/') {
        normalizedName.erase(normalizedName.begin());
      }
      std::string normalizedPrefix = candidate.namespacePrefix;
      if (!normalizedPrefix.empty() && normalizedPrefix.front() == '/') {
        normalizedPrefix.erase(normalizedPrefix.begin());
      }
      auto explicitLegacyOrCanonicalSoaHelperName = [&]() -> std::string {
        auto isSupportedSoaHelper = [](std::string_view helperName) {
          return helperName == "count" || helperName == "count_ref" ||
                 helperName == "get" || helperName == "get_ref" ||
                 helperName == "ref" || helperName == "ref_ref" ||
                 helperName == "to_aos" || helperName == "to_aos_ref" ||
                 helperName == "push" || helperName == "reserve";
        };
        if ((normalizedPrefix == "soa_vector" ||
             normalizedPrefix == "std/collections/soa_vector") &&
            isSupportedSoaHelper(normalizedName)) {
          return normalizedName;
        }
        if (normalizedName.rfind("soa_vector/", 0) == 0) {
          const std::string helperName =
              normalizedName.substr(std::string("soa_vector/").size());
          if (isSupportedSoaHelper(helperName)) {
            return helperName;
          }
        }
        if (normalizedName.rfind("std/collections/soa_vector/", 0) == 0) {
          const std::string helperName =
              normalizedName.substr(
                  std::string("std/collections/soa_vector/").size());
          if (isSupportedSoaHelper(helperName)) {
            return helperName;
          }
        }
        return {};
      };
      if (const std::string helperName = explicitLegacyOrCanonicalSoaHelperName();
          !helperName.empty()) {
        return preferredSoaHelperTargetForCollectionType(helperName,
                                                         "/soa_vector");
      }
      return {};
    };
    auto methodResolvedCandidate = [&]() -> std::string {
      if (!candidate.isMethodCall || candidate.args.empty() || candidate.name.empty()) {
        return {};
      }
      std::string methodName = candidate.name;
      if (!methodName.empty() && methodName.front() == '/') {
        methodName.erase(methodName.begin());
      }
      if (methodName.empty()) {
        return {};
      }
      auto resolveMethodOwnerPath = [&](const std::string &typeText, const std::string &typeNamespace) {
        std::string normalizedType = normalizeBindingTypeName(unwrapReferencePointerTypeText(typeText));
        if (normalizedType.empty()) {
          return std::string{};
        }
        std::string base;
        std::string argText;
        if (splitTemplateTypeName(normalizedType, base, argText)) {
          const std::string normalizedBase = normalizeBindingTypeName(base);
          if (!normalizedBase.empty()) {
            if (!normalizeCollectionTypePath(normalizedBase).empty()) {
              return std::string{};
            }
            normalizedType = normalizedBase;
          }
        }
        if (isPrimitiveBindingTypeName(normalizedType)) {
          return "/" + normalizedType;
        }
        if (!normalizedType.empty() && normalizedType.front() == '/') {
          if (structNames_.count(normalizedType) > 0 || defMap_.count(normalizedType) > 0) {
            return normalizedType;
          }
        }
        if (!normalizedType.empty() && normalizedType.front() != '/') {
          const std::string rootPath = "/" + normalizedType;
          if (structNames_.count(rootPath) > 0 || defMap_.count(rootPath) > 0) {
            return rootPath;
          }
          auto importIt = importAliases_.find(normalizedType);
          if (importIt != importAliases_.end()) {
            return importIt->second;
          }
        }
        std::string resolvedType = resolveStructTypePath(normalizedType, typeNamespace, structNames_);
        if (resolvedType.empty()) {
          resolvedType = resolveTypePath(normalizedType, typeNamespace);
        }
        return resolvedType;
      };

      const Expr &receiver = candidate.args.front();
      std::string receiverTypeText;
      if (!inferExprTypeText(receiver, receiverTypeText) || receiverTypeText.empty()) {
        return std::string{};
      }
      const std::string ownerPath = resolveMethodOwnerPath(receiverTypeText, receiver.namespacePrefix);
      if (ownerPath.empty()) {
        return std::string{};
      }
      return ownerPath + "/" + methodName;
    };
    auto resolvedMethodTargetCandidate = [&]() -> std::string {
      if (!candidate.isMethodCall || candidate.args.empty() || candidate.name.empty()) {
        return {};
      }
      std::string resolvedMethodTarget;
      bool isBuiltinMethod = false;
      if (!resolveMethodTarget(
              params,
              locals,
              candidate.namespacePrefix,
              candidate.args.front(),
              candidate.name,
              resolvedMethodTarget,
              isBuiltinMethod)) {
        return {};
      }
      return resolvedMethodTarget;
    };
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
    std::string collectionMethodFallbackTypeText;
    std::string inferredMethodReturnTypeText;
    if (candidate.isMethodCall) {
      const ReturnKind inferredKind = inferExprReturnKind(candidate, params, locals);
      if (inferredKind == ReturnKind::Array) {
        collectionMethodFallbackTypeText = inferStructReturnPath(candidate, params, locals);
        const std::string normalizedCollectionType = normalizeCollectionTypePath(collectionMethodFallbackTypeText);
        if (!normalizedCollectionType.empty()) {
          const bool keepExperimentalCollectionPath =
              collectionMethodFallbackTypeText == "Vector" ||
              collectionMethodFallbackTypeText == "/std/collections/experimental_vector/Vector" ||
              collectionMethodFallbackTypeText == "std/collections/experimental_vector/Vector" ||
              collectionMethodFallbackTypeText.rfind("/std/collections/experimental_vector/Vector__", 0) == 0 ||
              collectionMethodFallbackTypeText.rfind("std/collections/experimental_vector/Vector__", 0) == 0 ||
              collectionMethodFallbackTypeText == "Map" ||
              collectionMethodFallbackTypeText.rfind("/std/collections/experimental_map/Map__", 0) == 0 ||
              collectionMethodFallbackTypeText.rfind("std/collections/experimental_map/Map__", 0) == 0;
          if (!keepExperimentalCollectionPath) {
            collectionMethodFallbackTypeText = normalizedCollectionType.substr(1);
          }
        }
      }
      if (inferredKind != ReturnKind::Unknown && inferredKind != ReturnKind::Void) {
        inferredMethodReturnTypeText = typeNameForReturnKind(inferredKind);
      }
    }

    std::vector<std::string> resolvedCandidates;
    auto appendResolvedCandidate = [&](const std::string &candidatePath) {
      if (candidatePath.empty()) {
        return;
      }
      for (const auto &existing : resolvedCandidates) {
        if (existing == candidatePath) {
          return;
        }
      }
      resolvedCandidates.push_back(candidatePath);
    };
    appendResolvedCandidate(resolvedCandidate);
    appendResolvedCandidate(canonicalResolvedCandidate);
    appendResolvedCandidate(preferredResolvedCandidate());
    appendResolvedCandidate(resolvedMethodTargetCandidate());
    appendResolvedCandidate(methodResolvedCandidate());
    const Definition *resolvedDefinition = nullptr;
    std::string resolvedDefinitionPath;
    for (const auto &candidatePath : resolvedCandidates) {
      auto defIt = defMap_.find(candidatePath);
      if (defIt == defMap_.end() || defIt->second == nullptr) {
        continue;
      }
      resolvedDefinition = defIt->second;
      resolvedDefinitionPath = candidatePath;
      break;
    }
    if (resolvedDefinition == nullptr) {
      if (!inferredMethodReturnTypeText.empty()) {
        currentTypeTextOut = inferredMethodReturnTypeText;
        return true;
      }
      if (collectionMethodFallbackTypeText.empty()) {
        return false;
      }
      currentTypeTextOut = collectionMethodFallbackTypeText;
      return true;
    }
    for (const auto &transform : resolvedDefinition->transforms) {
      if (transform.name != "return" || transform.templateArgs.size() != 1) {
        continue;
      }
      if (transform.templateArgs.front() == "auto") {
        break;
      }
      currentTypeTextOut = transform.templateArgs.front();
      return !currentTypeTextOut.empty();
    }
    if (returnBindingInferenceStack_.contains(resolvedDefinitionPath) ||
        inferenceStack_.contains(resolvedDefinitionPath)) {
      if (!inferredMethodReturnTypeText.empty()) {
        currentTypeTextOut = inferredMethodReturnTypeText;
        return true;
      }
      if (collectionMethodFallbackTypeText.empty()) {
        return false;
      }
      currentTypeTextOut = collectionMethodFallbackTypeText;
      return true;
    }
    if (!queryTypeInferenceDefinitionStack_.insert(resolvedDefinitionPath).second) {
      return false;
    }
    const auto stackIt = queryTypeInferenceDefinitionStack_.find(resolvedDefinitionPath);
    struct ScopeGuard {
      std::unordered_set<std::string> &stack;
      std::unordered_set<std::string>::const_iterator it;
      ~ScopeGuard() {
        stack.erase(it);
      }
    } guard{queryTypeInferenceDefinitionStack_, stackIt};
    BindingInfo inferredReturn;
    if (!inferDefinitionReturnBinding(*resolvedDefinition, inferredReturn)) {
      if (!inferredMethodReturnTypeText.empty()) {
        currentTypeTextOut = inferredMethodReturnTypeText;
        return true;
      }
      if (collectionMethodFallbackTypeText.empty()) {
        return false;
      }
      currentTypeTextOut = collectionMethodFallbackTypeText;
      return true;
    }
    auto substituteCallTemplateArgs = [&](const std::string &typeText) {
      std::function<std::string(const std::string &)> substitute = [&](const std::string &currentTypeText) {
        const std::string normalized = normalizeBindingTypeName(currentTypeText);
        if (normalized.empty()) {
          return currentTypeText;
        }
        for (size_t i = 0; i < resolvedDefinition->templateArgs.size() && i < candidate.templateArgs.size(); ++i) {
          if (normalizeBindingTypeName(resolvedDefinition->templateArgs[i]) == normalized) {
            return candidate.templateArgs[i];
          }
        }
        std::string base;
        std::string argText;
        if (!splitTemplateTypeName(normalized, base, argText)) {
          return normalized;
        }
        std::vector<std::string> args;
        if (!splitTopLevelTemplateArgs(argText, args) || args.empty()) {
          return normalized;
        }
        std::string substitutedBase = substitute(base);
        for (auto &arg : args) {
          arg = substitute(arg);
        }
        return substitutedBase + "<" + joinTemplateArgs(args) + ">";
      };
      return substitute(typeText);
    };
    inferredReturn.typeName = substituteCallTemplateArgs(inferredReturn.typeName);
    inferredReturn.typeTemplateArg = substituteCallTemplateArgs(inferredReturn.typeTemplateArg);
    currentTypeTextOut = bindingTypeText(inferredReturn);
    return !currentTypeTextOut.empty();
  };

  return inferExprTypeText(expr, typeTextOut);
}

} // namespace primec::semantics
