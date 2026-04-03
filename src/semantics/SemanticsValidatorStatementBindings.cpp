#include "SemanticsValidator.h"

#include <functional>
#include <optional>

namespace primec::semantics {
namespace {

std::string referenceRootForBorrowBinding(const std::string &bindingName, const BindingInfo &binding) {
  if (binding.typeName == "Reference") {
    if (!binding.referenceRoot.empty()) {
      return binding.referenceRoot;
    }
    return bindingName;
  }
  return "";
}

} // namespace

bool SemanticsValidator::validateBindingStatement(const std::vector<ParameterInfo> &params,
                                                  std::unordered_map<std::string, BindingInfo> &locals,
                                                  const Expr &stmt,
                                                  bool allowBindings,
                                                  const std::string &namespacePrefix,
                                                  bool &handled) {
  handled = false;
  if (!stmt.isBinding) {
    return true;
  }

  handled = true;
  auto publishBindingDiagnostic = [&]() -> bool {
    captureExprContext(stmt);
    return publishCurrentStructuredDiagnosticNow();
  };
  const std::vector<std::string> *definitionTemplateArgs = nullptr;
  auto currentDefIt = defMap_.find(currentValidationContext_.definitionPath);
  if (currentDefIt != defMap_.end() && currentDefIt->second != nullptr) {
    definitionTemplateArgs = &currentDefIt->second->templateArgs;
  }

  if (!allowBindings) {
    error_ = "binding not allowed in execution body";
    return publishBindingDiagnostic();
  }
  if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
    error_ = "binding does not accept block arguments";
    return publishBindingDiagnostic();
  }
  if (isParam(params, stmt.name) || locals.count(stmt.name) > 0) {
    error_ = "duplicate binding name: " + stmt.name;
    return publishBindingDiagnostic();
  }
  for (const auto &transform : stmt.transforms) {
    if (transform.name != "soa_vector" || transform.templateArgs.size() != 1) {
      continue;
    }
    if (!isSoaVectorStructElementType(transform.templateArgs.front(), namespacePrefix, structNames_, importAliases_)) {
      break;
    }
    if (!validateSoaVectorElementFieldEnvelopes(transform.templateArgs.front(), namespacePrefix)) {
      return false;
    }
    break;
  }

  BindingInfo info;
  std::optional<std::string> restrictType;
  if (!parseBindingInfo(stmt, namespacePrefix, structNames_, importAliases_, info, restrictType, error_)) {
    return false;
  }

  const bool hasExplicitType = hasExplicitBindingTypeTransform(stmt);
  const bool explicitAutoType = hasExplicitType && normalizeBindingTypeName(info.typeName) == "auto";
  if (stmt.args.size() == 1 && stmt.args.front().isLambda && (!hasExplicitType || explicitAutoType)) {
    info.typeName = "lambda";
    info.typeTemplateArg.clear();
  }

  if (stmt.args.empty()) {
    if (structNames_.count(currentValidationContext_.definitionPath) > 0) {
      if (restrictType.has_value()) {
        const bool hasTemplate = !info.typeTemplateArg.empty();
        if (!restrictMatchesBinding(*restrictType, info.typeName, info.typeTemplateArg, hasTemplate, namespacePrefix)) {
          error_ = "restrict type does not match binding type";
          return false;
        }
      }
      if (!validateBuiltinMapKeyType(info, definitionTemplateArgs, error_)) {
        return false;
      }
      locals.emplace(stmt.name, info);
      return true;
    }
    if (!validateOmittedBindingInitializer(stmt, info, namespacePrefix)) {
      return false;
    }
    if (restrictType.has_value()) {
      const bool hasTemplate = !info.typeTemplateArg.empty();
      if (!restrictMatchesBinding(*restrictType, info.typeName, info.typeTemplateArg, hasTemplate, namespacePrefix)) {
        error_ = "restrict type does not match binding type";
        return false;
      }
    }
    if (!validateBuiltinMapKeyType(info, definitionTemplateArgs, error_)) {
      return false;
    }
    locals.emplace(stmt.name, info);
    return true;
  }

  if (stmt.args.size() != 1) {
    error_ = "binding requires exactly one argument";
    return publishBindingDiagnostic();
  }

  const Expr &initializer = stmt.args.front();
  auto isEmptyBuiltinBlockInitializer = [&](const Expr &candidate) -> bool {
    if (!candidate.hasBodyArguments || !candidate.bodyArguments.empty()) {
      return false;
    }
    if (!candidate.args.empty() || !candidate.templateArgs.empty() || hasNamedArguments(candidate.argNames)) {
      return false;
    }
    return isBuiltinBlockCall(candidate);
  };

  const std::string normalizedBindingType = normalizeBindingTypeName(info.typeName);
  if ((normalizedBindingType == "vector" || normalizedBindingType == "soa_vector") &&
      isEmptyBuiltinBlockInitializer(initializer)) {
    if (!validateOmittedBindingInitializer(stmt, info, namespacePrefix)) {
      return false;
    }
    if (restrictType.has_value()) {
      const bool hasTemplate = !info.typeTemplateArg.empty();
      if (!restrictMatchesBinding(*restrictType, info.typeName, info.typeTemplateArg, hasTemplate, namespacePrefix)) {
        error_ = "restrict type does not match binding type";
        return false;
      }
    }
    if (!validateBuiltinMapKeyType(info, definitionTemplateArgs, error_)) {
      return false;
    }
    locals.emplace(stmt.name, info);
    return true;
  }

  const bool entryArgInit = isEntryArgsAccess(initializer);
  const bool entryArgStringInit = isEntryArgStringBinding(locals, initializer);
  std::optional<EntryArgStringScope> entryArgScope;
  if (entryArgInit || entryArgStringInit) {
    entryArgScope.emplace(*this, true);
  }

  Expr initializerForValidation = initializer;
  const Expr *initializerExprForValidation = &initializer;
  if (!hasExplicitType || explicitAutoType) {
    std::string namespacedCollection;
    std::string namespacedHelper;
    const std::string resolvedInitializer = resolveCalleePath(initializer);
    auto hasImportedInitializerDefinitionPath = [&](const std::string &path) {
      std::string canonicalPath = path;
      const size_t suffix = canonicalPath.find("__t");
      if (suffix != std::string::npos) {
        canonicalPath.erase(suffix);
      }
      const auto &importPaths = program_.sourceImports.empty() ? program_.imports : program_.sourceImports;
      for (const auto &importPath : importPaths) {
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
    };
    const bool isUnresolvedStdNamespacedVectorCountCapacityCall =
        initializer.kind == Expr::Kind::Call &&
        !initializer.isMethodCall &&
        getNamespacedCollectionHelperName(initializer, namespacedCollection, namespacedHelper) &&
        namespacedCollection == "vector" &&
        (namespacedHelper == "count" || namespacedHelper == "capacity") &&
        resolvedInitializer == "/std/collections/vector/" + namespacedHelper &&
        defMap_.find(resolvedInitializer) == defMap_.end() &&
        !hasImportedInitializerDefinitionPath(resolvedInitializer);
    if (isUnresolvedStdNamespacedVectorCountCapacityCall) {
      initializerForValidation.name = "/vector/" + namespacedHelper;
      initializerForValidation.namespacePrefix.clear();
      initializerExprForValidation = &initializerForValidation;
    }
  }

  if (!validateExpr(params, locals, *initializerExprForValidation)) {
    if (const auto pendingPath =
            builtinSoaDirectPendingHelperPath(initializer, params, locals)) {
      error_ = soaDirectPendingUnavailableMethodDiagnostic(*pendingPath);
      return false;
    }
    if (error_.empty()) {
      error_ = "binding initializer validateExpr failed";
    }
    return false;
  }
  if (const auto pendingPath =
          builtinSoaDirectPendingHelperPath(initializer, params, locals)) {
    error_ = soaDirectPendingUnavailableMethodDiagnostic(*pendingPath);
    return false;
  }

  ReturnKind initKind = inferExprReturnKind(initializer, params, locals);
  if (initKind == ReturnKind::Void && !isStructConstructorValueExpr(initializer)) {
    error_ = "binding initializer requires a value";
    return publishBindingDiagnostic();
  }

  auto isSoftwareNumericBindingCompatible = [](ReturnKind expectedKind, ReturnKind actualKind) -> bool {
    switch (expectedKind) {
      case ReturnKind::Integer:
        return actualKind == ReturnKind::Int || actualKind == ReturnKind::Int64 || actualKind == ReturnKind::UInt64 ||
               actualKind == ReturnKind::Bool || actualKind == ReturnKind::Integer;
      case ReturnKind::Decimal:
        return actualKind == ReturnKind::Int || actualKind == ReturnKind::Int64 || actualKind == ReturnKind::UInt64 ||
               actualKind == ReturnKind::Bool || actualKind == ReturnKind::Float32 ||
               actualKind == ReturnKind::Float64 || actualKind == ReturnKind::Integer ||
               actualKind == ReturnKind::Decimal;
      case ReturnKind::Complex:
        return actualKind == ReturnKind::Int || actualKind == ReturnKind::Int64 || actualKind == ReturnKind::UInt64 ||
               actualKind == ReturnKind::Bool || actualKind == ReturnKind::Float32 ||
               actualKind == ReturnKind::Float64 || actualKind == ReturnKind::Integer ||
               actualKind == ReturnKind::Decimal || actualKind == ReturnKind::Complex;
      default:
        return false;
    }
  };

  auto isFloatBindingCompatible = [](ReturnKind expectedKind, ReturnKind actualKind) -> bool {
    if (expectedKind != ReturnKind::Float32 && expectedKind != ReturnKind::Float64) {
      return false;
    }
    return actualKind == ReturnKind::Float32 || actualKind == ReturnKind::Float64;
  };
  auto isStringExpr = [&](const Expr &candidate,
                          const std::vector<ParameterInfo> &paramsIn,
                          const std::unordered_map<std::string, BindingInfo> &localsIn) -> bool {
    if (candidate.kind == Expr::Kind::StringLiteral) {
      return true;
    }
    if (candidate.kind == Expr::Kind::Name) {
      if (const BindingInfo *paramBinding = findParamBinding(paramsIn, candidate.name)) {
        return paramBinding->typeName == "string";
      }
      auto it = localsIn.find(candidate.name);
      return it != localsIn.end() && it->second.typeName == "string";
    }
    return inferExprReturnKind(candidate, paramsIn, localsIn) == ReturnKind::String;
  };
  auto collectionRepresentation = [&](const std::string &typeName,
                                      const std::string &typeTemplateArg) -> std::string {
    std::string normalizedType = normalizeBindingTypeName(typeName);
    std::string base = normalizedType;
    std::string argText = typeTemplateArg;
    if (argText.empty()) {
      std::string splitBase;
      std::string splitArgText;
      if (splitTemplateTypeName(normalizedType, splitBase, splitArgText)) {
        base = normalizeBindingTypeName(splitBase);
        argText = splitArgText;
      }
    }
    if (base == "vector") {
      return "builtin_vector";
    }
    if (base == "Vector" || base == "/std/collections/experimental_vector/Vector" ||
        base == "std/collections/experimental_vector/Vector" ||
        base.rfind("/std/collections/experimental_vector/Vector__", 0) == 0 ||
        base.rfind("std/collections/experimental_vector/Vector__", 0) == 0) {
      return "experimental_vector";
    }
    return {};
  };

  if (!hasExplicitType || explicitAutoType) {
    (void)inferBindingTypeFromInitializer(initializer, params, locals, info, &stmt);
  } else {
    const std::string expectedType = normalizeBindingTypeName(info.typeName);
    const std::string expectedRepresentation =
        collectionRepresentation(info.typeName, info.typeTemplateArg);
    auto isExplicitCanonicalVectorConstructor = [&](const Expr &candidate) {
      if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall) {
        return false;
      }
      if (candidate.name == "/std/collections/vector/vector" ||
          candidate.name == "std/collections/vector/vector") {
        return true;
      }
      std::string normalizedPrefix = candidate.namespacePrefix;
      if (!normalizedPrefix.empty() && normalizedPrefix.front() == '/') {
        normalizedPrefix.erase(normalizedPrefix.begin());
      }
      return normalizedPrefix == "std/collections/vector" && candidate.name == "vector";
    };
    if (expectedRepresentation == "builtin_vector" &&
        isExplicitCanonicalVectorConstructor(initializer) &&
        hasImportedDefinitionPath("/std/collections/vector/vector")) {
        error_ = "binding initializer type mismatch";
        return false;
    }
    ResultTypeInfo resultInfo;
    if (expectedType != "Result" &&
        resolveResultTypeForExpr(initializer, params, locals, resultInfo) &&
        resultInfo.isResult) {
      error_ = "binding initializer type mismatch";
      return false;
    }
    if (expectedType == "string") {
      if (!isStringExpr(initializer, params, locals)) {
        error_ = "binding initializer type mismatch";
        return false;
      }
    } else {
      BindingInfo initializerBindingInfo;
      const bool hasInitializerBindingInfo =
          inferBindingTypeFromInitializer(initializer, params, locals, initializerBindingInfo, &stmt);
      std::string initializerTypeText;
      const bool hasInitializerTypeText =
          inferQueryExprTypeText(initializer, params, locals, initializerTypeText);
      if (hasInitializerTypeText) {
        std::string actualRepresentation =
            collectionRepresentation(initializerTypeText, {});
        const std::string initializerBindingRepresentation =
            hasInitializerBindingInfo
                ? collectionRepresentation(initializerBindingInfo.typeName, initializerBindingInfo.typeTemplateArg)
                : std::string{};
        if (actualRepresentation.empty()) {
          actualRepresentation = initializerBindingRepresentation;
        } else if (!expectedRepresentation.empty() &&
                   !initializerBindingRepresentation.empty() &&
                   initializerBindingRepresentation == expectedRepresentation) {
          actualRepresentation = initializerBindingRepresentation;
        }
        if (!expectedRepresentation.empty() &&
            !actualRepresentation.empty() &&
            expectedRepresentation != actualRepresentation) {
          error_ = "binding initializer type mismatch";
          return false;
        }
      } else if (hasInitializerBindingInfo) {
        const std::string actualRepresentation =
            collectionRepresentation(initializerBindingInfo.typeName, initializerBindingInfo.typeTemplateArg);
        if (!expectedRepresentation.empty() &&
            !actualRepresentation.empty() &&
            expectedRepresentation != actualRepresentation) {
          error_ = "binding initializer type mismatch";
          return false;
        }
      }
      const ReturnKind expectedKind = returnKindForTypeName(expectedType);
      if (expectedKind != ReturnKind::Unknown && initKind != ReturnKind::Unknown) {
        if (!isSoftwareNumericBindingCompatible(expectedKind, initKind) &&
            !isFloatBindingCompatible(expectedKind, initKind) &&
            initKind != expectedKind) {
          error_ = "binding initializer type mismatch";
          return false;
        }
      }
    }
  }

  if (info.typeName == "uninitialized") {
    if (info.typeTemplateArg.empty()) {
      error_ = "uninitialized requires exactly one template argument";
      return false;
    }
    if (initializer.kind != Expr::Kind::Call || initializer.isMethodCall || initializer.isBinding) {
      error_ = "uninitialized bindings require uninitialized<T>() initializer";
      return false;
    }
    if (initializer.name != "uninitialized" && initializer.name != "/uninitialized") {
      error_ = "uninitialized bindings require uninitialized<T>() initializer";
      return false;
    }
    if (initializer.hasBodyArguments || !initializer.bodyArguments.empty() || !initializer.args.empty()) {
      error_ = "uninitialized does not accept arguments";
      return false;
    }
    if (initializer.templateArgs.size() != 1 ||
        !errorTypesMatch(info.typeTemplateArg, initializer.templateArgs.front(), namespacePrefix)) {
      error_ = "uninitialized initializer type mismatch";
      return false;
    }
  }

  if (restrictType.has_value()) {
    const bool hasTemplate = !info.typeTemplateArg.empty();
    if (!restrictMatchesBinding(*restrictType, info.typeName, info.typeTemplateArg, hasTemplate, namespacePrefix)) {
      error_ = "restrict type does not match binding type";
      return false;
    }
  }

  if (entryArgInit || entryArgStringInit) {
    if (normalizeBindingTypeName(info.typeName) != "string") {
      error_ = "entry argument strings require string bindings";
      return false;
    }
    info.isEntryArgString = true;
  }

  auto pointerAliasRootForBinding = [&](const std::string &bindingName, const BindingInfo &binding) -> std::string {
    std::string referenceRoot = referenceRootForBorrowBinding(bindingName, binding);
    if (!referenceRoot.empty()) {
      return referenceRoot;
    }
    if (binding.typeName == "Pointer" && !binding.referenceRoot.empty()) {
      return binding.referenceRoot;
    }
    return "";
  };

  auto resolveNamedBinding = [&](const std::string &name) -> const BindingInfo * {
    if (const BindingInfo *paramBinding = findParamBinding(params, name)) {
      return paramBinding;
    }
    auto it = locals.find(name);
    if (it == locals.end()) {
      return nullptr;
    }
    return &it->second;
  };

  std::function<bool(const Expr &, std::string &)> resolvePointerRoot;
  resolvePointerRoot = [&](const Expr &expr, std::string &rootOut) -> bool {
    if (expr.kind == Expr::Kind::Name) {
      const BindingInfo *binding = resolveNamedBinding(expr.name);
      if (binding == nullptr) {
        return false;
      }
      rootOut = pointerAliasRootForBinding(expr.name, *binding);
      return !rootOut.empty();
    }
    if (expr.kind != Expr::Kind::Call) {
      return false;
    }
    std::string builtinName;
    if (getBuiltinPointerName(expr, builtinName) && builtinName == "location" && expr.args.size() == 1) {
      const Expr &target = expr.args.front();
      if (target.kind != Expr::Kind::Name) {
        return false;
      }
      const BindingInfo *binding = resolveNamedBinding(target.name);
      if (binding != nullptr) {
        std::string root = pointerAliasRootForBinding(target.name, *binding);
        if (!root.empty()) {
          rootOut = std::move(root);
        } else {
          rootOut = target.name;
        }
        return true;
      }
      return false;
    }
    std::string opName;
    if (getBuiltinOperatorName(expr, opName) && (opName == "plus" || opName == "minus") && expr.args.size() == 2) {
      if (isPointerLikeExpr(expr.args[1], params, locals)) {
        return false;
      }
      return resolvePointerRoot(expr.args[0], rootOut);
    }
    return false;
  };

  if (info.typeName == "Pointer") {
    std::string pointerRoot;
    if (resolvePointerRoot(initializer, pointerRoot)) {
      info.referenceRoot = std::move(pointerRoot);
    }
    if (!validateBuiltinMapKeyType(info, definitionTemplateArgs, error_)) {
      return false;
    }
    locals.emplace(stmt.name, info);
    return true;
  }

  if (info.typeName == "Reference") {
    const Expr &init = initializer;
    auto formatBindingType = [](const BindingInfo &binding) -> std::string {
      if (binding.typeTemplateArg.empty()) {
        return binding.typeName;
      }
      return binding.typeName + "<" + binding.typeTemplateArg + ">";
    };

    std::function<bool(const Expr &, std::string &)> resolvePointerTargetType;
    resolvePointerTargetType = [&](const Expr &expr, std::string &targetOut) -> bool {
      if (expr.kind == Expr::Kind::Name) {
        const BindingInfo *binding = resolveNamedBinding(expr.name);
        if (binding == nullptr) {
          return false;
        }
        if ((binding->typeName == "Pointer" || binding->typeName == "Reference") &&
            !binding->typeTemplateArg.empty()) {
          targetOut = binding->typeTemplateArg;
          return true;
        }
        return false;
      }
      if (expr.kind != Expr::Kind::Call) {
        return false;
      }
      std::string builtinName;
      if (getBuiltinPointerName(expr, builtinName) && builtinName == "location" && expr.args.size() == 1) {
        const Expr &target = expr.args.front();
        if (target.kind != Expr::Kind::Name) {
          return false;
        }
        const BindingInfo *binding = resolveNamedBinding(target.name);
        if (binding == nullptr) {
          return false;
        }
        if (binding->typeName == "Reference" && !binding->typeTemplateArg.empty()) {
          targetOut = binding->typeTemplateArg;
        } else {
          targetOut = formatBindingType(*binding);
        }
        return true;
      }
      std::string opName;
      if (getBuiltinOperatorName(expr, opName) && (opName == "plus" || opName == "minus") && expr.args.size() == 2) {
        if (isPointerLikeExpr(expr.args[1], params, locals)) {
          return false;
        }
        return resolvePointerTargetType(expr.args[0], targetOut);
      }
      auto defIt = defMap_.find(resolveCalleePath(expr));
      if (defIt == defMap_.end() || defIt->second == nullptr) {
        return false;
      }
      for (const auto &transform : defIt->second->transforms) {
        if (transform.name != "return" || transform.templateArgs.size() != 1) {
          continue;
        }
        std::string base;
        std::string arg;
        if (!splitTemplateTypeName(transform.templateArgs.front(), base, arg)) {
          continue;
        }
        if (base != "Reference" && base != "Pointer") {
          continue;
        }
        std::vector<std::string> args;
        if (!splitTopLevelTemplateArgs(arg, args) || args.size() != 1) {
          return false;
        }
        targetOut = args.front();
        return true;
      }
      return false;
    };

    std::string pointerName;
    const bool initIsLocation =
        init.kind == Expr::Kind::Call && getBuiltinPointerName(init, pointerName) && pointerName == "location" &&
        init.args.size() == 1 && init.args.front().kind == Expr::Kind::Name;
    std::string safeTargetType;
    const bool initIsPointerLike = resolvePointerTargetType(init, safeTargetType);
    if (!initIsLocation && !initIsPointerLike && !currentValidationContext_.definitionIsUnsafe) {
      error_ = "Reference bindings require location(...)";
      return false;
    }
    if (initIsLocation || (!currentValidationContext_.definitionIsUnsafe && initIsPointerLike)) {
      if (!initIsPointerLike || !errorTypesMatch(safeTargetType, info.typeTemplateArg, namespacePrefix)) {
        error_ = "Reference binding type mismatch";
        return false;
      }
    }
    if (!initIsLocation && !currentValidationContext_.definitionIsUnsafe) {
      if (!validateBuiltinMapKeyType(info, definitionTemplateArgs, error_)) {
        return false;
      }
      locals.emplace(stmt.name, info);
      return true;
    }
    if (!initIsLocation && currentValidationContext_.definitionIsUnsafe) {
      std::string pointerTargetType;
      if (!resolvePointerTargetType(init, pointerTargetType)) {
        error_ = "unsafe Reference bindings require pointer-like initializer";
        return false;
      }
      if (!errorTypesMatch(pointerTargetType, info.typeTemplateArg, namespacePrefix)) {
        error_ = "unsafe Reference binding type mismatch";
        return false;
      }
      std::string borrowRoot;
      if (resolvePointerRoot(init, borrowRoot)) {
        info.referenceRoot = std::move(borrowRoot);
      }
      info.isUnsafeReference = true;
      if (!validateBuiltinMapKeyType(info, definitionTemplateArgs, error_)) {
        return false;
      }
      locals.emplace(stmt.name, info);
      return true;
    }

    const Expr &target = init.args.front();
    auto resolveBorrowRoot = [&](const std::string &targetName, std::string &rootOut) -> bool {
      if (const BindingInfo *paramBinding = findParamBinding(params, targetName)) {
        if (paramBinding->typeName == "Reference") {
          rootOut = referenceRootForBorrowBinding(targetName, *paramBinding);
        } else {
          rootOut = targetName;
        }
        return true;
      }
      auto it = locals.find(targetName);
      if (it == locals.end()) {
        return false;
      }
      if (it->second.typeName == "Reference") {
        rootOut = referenceRootForBorrowBinding(it->first, it->second);
      } else {
        rootOut = targetName;
      }
      return true;
    };

    std::string borrowRoot;
    if (!resolveBorrowRoot(target.name, borrowRoot) || borrowRoot.empty()) {
      error_ = "Reference bindings require location(...)";
      return false;
    }
    bool sawMutableBorrow = false;
    bool sawImmutableBorrow = false;
    auto observeBorrow = [&](const std::string &bindingName, const BindingInfo &binding) {
      if (currentValidationContext_.endedReferenceBorrows.count(bindingName) > 0) {
        return;
      }
      const std::string root = referenceRootForBorrowBinding(bindingName, binding);
      if (root.empty() || root != borrowRoot) {
        return;
      }
      if (binding.isMutable) {
        sawMutableBorrow = true;
      } else {
        sawImmutableBorrow = true;
      }
    };
    for (const auto &param : params) {
      observeBorrow(param.name, param.binding);
    }
    for (const auto &entry : locals) {
      observeBorrow(entry.first, entry.second);
    }
    const bool conflict = info.isMutable ? (sawMutableBorrow || sawImmutableBorrow) : sawMutableBorrow;
    if (conflict && !currentValidationContext_.definitionIsUnsafe) {
      error_ = "borrow conflict: " + borrowRoot + " (root: " + borrowRoot + ", sink: " + stmt.name + ")";
      return false;
    }
    info.referenceRoot = std::move(borrowRoot);
    info.isUnsafeReference = currentValidationContext_.definitionIsUnsafe;
  }

  if (!validateBuiltinMapKeyType(info, definitionTemplateArgs, error_)) {
    return false;
  }
  locals.emplace(stmt.name, info);
  return true;
}

} // namespace primec::semantics
