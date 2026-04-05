#include "SemanticsValidator.h"

#include <functional>
#include <optional>

namespace primec::semantics {
namespace {

bool isSoaFieldViewBindingType(const BindingInfo &binding) {
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
}

std::string referenceRootForBorrowBinding(const std::string &bindingName, const BindingInfo &binding) {
  if (binding.typeName == "Reference" || isSoaFieldViewBindingType(binding)) {
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
  auto failBindingDiagnostic = [&](std::string message) -> bool {
    return failExprDiagnostic(stmt, std::move(message));
  };
  const std::vector<std::string> *definitionTemplateArgs = nullptr;
  auto currentDefIt = defMap_.find(currentValidationState_.context.definitionPath);
  if (currentDefIt != defMap_.end() && currentDefIt->second != nullptr) {
    definitionTemplateArgs = &currentDefIt->second->templateArgs;
  }

  if (!allowBindings) {
    return failBindingDiagnostic("binding not allowed in execution body");
  }
  if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
    return failBindingDiagnostic("binding does not accept block arguments");
  }
  if (isParam(params, stmt.name) || locals.count(stmt.name) > 0) {
    return failBindingDiagnostic("duplicate binding name: " + stmt.name);
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
    if (structNames_.count(currentValidationState_.context.definitionPath) > 0) {
      if (restrictType.has_value()) {
        const bool hasTemplate = !info.typeTemplateArg.empty();
        if (!restrictMatchesBinding(*restrictType, info.typeName, info.typeTemplateArg, hasTemplate, namespacePrefix)) {
          return failBindingDiagnostic("restrict type does not match binding type");
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
        return failBindingDiagnostic("restrict type does not match binding type");
      }
    }
    if (!validateBuiltinMapKeyType(info, definitionTemplateArgs, error_)) {
      return false;
    }
    locals.emplace(stmt.name, info);
    return true;
  }

  if (stmt.args.size() != 1) {
    return failBindingDiagnostic("binding requires exactly one argument");
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
        return failBindingDiagnostic("restrict type does not match binding type");
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
      return failBindingDiagnostic(soaDirectPendingUnavailableMethodDiagnostic(*pendingPath));
    }
    if (error_.empty()) {
      return failBindingDiagnostic("binding initializer validateExpr failed");
    }
    return false;
  }
  if (const auto pendingPath =
          builtinSoaDirectPendingHelperPath(initializer, params, locals)) {
    return failBindingDiagnostic(soaDirectPendingUnavailableMethodDiagnostic(*pendingPath));
  }
  if (isBuiltinSoaFieldViewExpr(initializer, params, locals, nullptr) &&
      !initializer.args.empty()) {
    const Expr &fieldViewReceiver = initializer.args.front();
    if (fieldViewReceiver.kind != Expr::Kind::Name) {
      return failBindingDiagnostic("field-view escapes via binding");
    }
  }

  ReturnKind initKind = inferExprReturnKind(initializer, params, locals);
  if (initKind == ReturnKind::Void && !isStructConstructorValueExpr(initializer)) {
    return failBindingDiagnostic("binding initializer requires a value");
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
        return failBindingDiagnostic("binding initializer type mismatch");
    }
    ResultTypeInfo resultInfo;
    if (expectedType != "Result" &&
        resolveResultTypeForExpr(initializer, params, locals, resultInfo) &&
        resultInfo.isResult) {
      return failBindingDiagnostic("binding initializer type mismatch");
    }
    if (expectedType == "string") {
      if (!isStringExpr(initializer, params, locals)) {
        return failBindingDiagnostic("binding initializer type mismatch");
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
          return failBindingDiagnostic("binding initializer type mismatch");
        }
      } else if (hasInitializerBindingInfo) {
        const std::string actualRepresentation =
            collectionRepresentation(initializerBindingInfo.typeName, initializerBindingInfo.typeTemplateArg);
        if (!expectedRepresentation.empty() &&
            !actualRepresentation.empty() &&
            expectedRepresentation != actualRepresentation) {
          return failBindingDiagnostic("binding initializer type mismatch");
        }
      }
      const ReturnKind expectedKind = returnKindForTypeName(expectedType);
      if (expectedKind != ReturnKind::Unknown && initKind != ReturnKind::Unknown) {
        if (!isSoftwareNumericBindingCompatible(expectedKind, initKind) &&
            !isFloatBindingCompatible(expectedKind, initKind) &&
            initKind != expectedKind) {
          return failBindingDiagnostic("binding initializer type mismatch");
        }
      }
    }
  }

  if (info.typeName == "uninitialized") {
    if (info.typeTemplateArg.empty()) {
      return failBindingDiagnostic("uninitialized requires exactly one template argument");
    }
    if (initializer.kind != Expr::Kind::Call || initializer.isMethodCall || initializer.isBinding) {
      return failBindingDiagnostic("uninitialized bindings require uninitialized<T>() initializer");
    }
    if (initializer.name != "uninitialized" && initializer.name != "/uninitialized") {
      return failBindingDiagnostic("uninitialized bindings require uninitialized<T>() initializer");
    }
    if (initializer.hasBodyArguments || !initializer.bodyArguments.empty() || !initializer.args.empty()) {
      return failBindingDiagnostic("uninitialized does not accept arguments");
    }
    if (initializer.templateArgs.size() != 1 ||
        !errorTypesMatch(info.typeTemplateArg, initializer.templateArgs.front(), namespacePrefix)) {
      return failBindingDiagnostic("uninitialized initializer type mismatch");
    }
  }

  if (restrictType.has_value()) {
    const bool hasTemplate = !info.typeTemplateArg.empty();
    if (!restrictMatchesBinding(*restrictType, info.typeName, info.typeTemplateArg, hasTemplate, namespacePrefix)) {
      return failBindingDiagnostic("restrict type does not match binding type");
    }
  }

  if (entryArgInit || entryArgStringInit) {
    if (normalizeBindingTypeName(info.typeName) != "string") {
      return failBindingDiagnostic("entry argument strings require string bindings");
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

  std::function<bool(const Expr &, std::string &)> resolveStorageRootExpr;
  std::function<bool(const Expr &, std::string &)> resolvePointerRoot;
  resolveStorageRootExpr = [&](const Expr &expr, std::string &rootOut) -> bool {
    if (expr.kind == Expr::Kind::Name) {
      const BindingInfo *binding = resolveNamedBinding(expr.name);
      if (binding == nullptr) {
        return false;
      }
      std::string aliasRoot = pointerAliasRootForBinding(expr.name, *binding);
      if (!aliasRoot.empty()) {
        rootOut = std::move(aliasRoot);
      } else {
        rootOut = expr.name;
      }
      return true;
    }
    if (expr.kind != Expr::Kind::Call) {
      return false;
    }
    std::string builtinName;
    if (getBuiltinPointerName(expr, builtinName) && builtinName == "dereference" && expr.args.size() == 1) {
      return resolvePointerRoot(expr.args.front(), rootOut);
    }
    if (expr.isFieldAccess && expr.args.size() == 1) {
      std::string receiverRoot;
      if (!resolveStorageRootExpr(expr.args.front(), receiverRoot) || receiverRoot.empty()) {
        return false;
      }
      rootOut = receiverRoot + "." + expr.name;
      return true;
    }
    return false;
  };
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
      if (target.kind == Expr::Kind::Name) {
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
      return resolvePointerRoot(target, rootOut);
    }
    if (expr.isFieldAccess && expr.args.size() == 1) {
      std::string receiverRoot;
      if (!resolvePointerRoot(expr.args.front(), receiverRoot) || receiverRoot.empty()) {
        return false;
      }
      rootOut = receiverRoot + "." + expr.name;
      return true;
    }
    std::string opName;
    if (getBuiltinOperatorName(expr, opName) && (opName == "plus" || opName == "minus") && expr.args.size() == 2) {
      if (isPointerLikeExpr(expr.args[1], params, locals)) {
        return false;
      }
      return resolvePointerRoot(expr.args[0], rootOut);
    }
    const std::string resolvedCallPath = resolveCalleePath(expr);
    const bool isSoaColumnSlotUnsafe =
        resolvedCallPath.rfind("/std/collections/experimental_soa_storage/soaColumnSlotUnsafe", 0) == 0;
    const bool isVectorSlotUnsafe =
        resolvedCallPath.rfind("/std/collections/experimental_vector/vectorSlotUnsafe", 0) == 0;
    if ((isSoaColumnSlotUnsafe || isVectorSlotUnsafe) && !expr.args.empty()) {
      std::string storageRoot;
      if (!resolveStorageRootExpr(expr.args.front(), storageRoot) || storageRoot.empty()) {
        return false;
      }
      rootOut = storageRoot + ".data";
      return true;
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
    auto isStandaloneBorrowStorageExpr = [&](const Expr &candidate) {
      if (candidate.kind == Expr::Kind::Name) {
        return true;
      }
      std::string builtinName;
      if (candidate.kind == Expr::Kind::Call && getBuiltinPointerName(candidate, builtinName) &&
          builtinName == "dereference" && candidate.args.size() == 1) {
        return true;
      }
      return candidate.kind == Expr::Kind::Call && candidate.isFieldAccess && candidate.args.size() == 1;
    };
    auto resolveDirectBorrowStorageTargetType = [&](const Expr &expr, std::string &targetOut) -> bool {
      if (expr.kind != Expr::Kind::Call || expr.isMethodCall || !isSimpleCallName(expr, "borrow") ||
          expr.args.size() != 1) {
        return false;
      }
      const Expr &storage = expr.args.front();
      if (!isStandaloneBorrowStorageExpr(storage)) {
        return false;
      }
      BindingInfo binding;
      bool resolved = false;
      if (!resolveUninitializedStorageBinding(params, locals, storage, binding, resolved)) {
        return false;
      }
      if (!resolved || binding.typeName != "uninitialized" || binding.typeTemplateArg.empty()) {
        return false;
      }
      targetOut = binding.typeTemplateArg;
      return true;
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
        if (target.kind == Expr::Kind::Name) {
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
        BindingInfo inferredBinding;
        if (!inferBindingTypeFromInitializer(target, params, locals, inferredBinding)) {
          return false;
        }
        if (inferredBinding.typeName == "Reference" && !inferredBinding.typeTemplateArg.empty()) {
          targetOut = inferredBinding.typeTemplateArg;
        } else {
          targetOut = formatBindingType(inferredBinding);
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
    auto resolveBorrowRoot = [&](const std::string &targetName, std::string &rootOut) -> bool {
      if (const BindingInfo *paramBinding = findParamBinding(params, targetName)) {
        if (paramBinding->typeName == "Reference" ||
            isSoaFieldViewBindingType(*paramBinding)) {
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
      if (it->second.typeName == "Reference" ||
          isSoaFieldViewBindingType(it->second)) {
        rootOut = referenceRootForBorrowBinding(it->first, it->second);
      } else {
        rootOut = targetName;
      }
      return true;
    };
    using ExprSubstitutions = std::vector<std::pair<std::string, const Expr *>>;
    auto findSubstitutedExpr = [&](const ExprSubstitutions &substitutions,
                                   const std::string &name) -> const Expr * {
      for (auto it = substitutions.rbegin(); it != substitutions.rend(); ++it) {
        if (it->first == name) {
          return it->second;
        }
      }
      return nullptr;
    };
    auto appendCallSubstitutions =
        [&](const Expr &callExpr,
            const ExprSubstitutions &baseSubstitutions,
            ExprSubstitutions &extendedSubstitutions,
            const Expr *&returnedValueExprOut) -> bool {
          returnedValueExprOut = nullptr;
          const std::string resolvedCallPath = resolveCalleePath(callExpr);
          auto defIt = defMap_.find(resolvedCallPath);
          if (defIt == defMap_.end() || defIt->second == nullptr) {
            return false;
          }
          const auto paramsIt = paramsByDef_.find(resolvedCallPath);
          if (paramsIt == paramsByDef_.end()) {
            return false;
          }
          const auto &nestedParams = paramsIt->second;
          std::string nestedArgError;
          std::vector<const Expr *> nestedOrderedArgs;
          if (!buildOrderedArguments(nestedParams, callExpr.args, callExpr.argNames,
                                     nestedOrderedArgs, nestedArgError)) {
            return false;
          }
          const Definition &nestedDef = *defIt->second;
          for (const auto &stmtExpr : nestedDef.statements) {
            if (isReturnCall(stmtExpr) && stmtExpr.args.size() == 1) {
              returnedValueExprOut = &stmtExpr.args.front();
            }
          }
          if (nestedDef.returnExpr.has_value()) {
            returnedValueExprOut = &*nestedDef.returnExpr;
          }
          if (returnedValueExprOut == nullptr) {
            return false;
          }
          extendedSubstitutions = baseSubstitutions;
          for (size_t nestedIndex = 0;
               nestedIndex < nestedParams.size() && nestedIndex < nestedOrderedArgs.size();
               ++nestedIndex) {
            const Expr *nestedArg = nestedOrderedArgs[nestedIndex];
            if (nestedArg == nullptr) {
              continue;
            }
            extendedSubstitutions.emplace_back(nestedParams[nestedIndex].name, nestedArg);
          }
          return true;
        };
    std::function<bool(const Expr &, const ExprSubstitutions &, std::string &)>
        resolveReceiverRootExpr;
    std::function<bool(const Expr &, const ExprSubstitutions &, std::string &)>
        resolveStandaloneRefRootExpr;
    auto isStandaloneRefCall = [&](const Expr &expr) -> bool {
      if (expr.kind != Expr::Kind::Call || expr.args.size() != 2) {
        return false;
      }
      const std::string resolvedPath = resolveCalleePath(expr);
      if (expr.isMethodCall) {
        return expr.name == "ref" ||
               resolvedPath.rfind("/std/collections/soa_vector/ref", 0) == 0 ||
               resolvedPath.rfind("/soa_vector/ref", 0) == 0 ||
               resolvedPath.rfind("/std/collections/experimental_soa_vector/soaVectorRef", 0) == 0;
      }
      return isSimpleCallName(expr, "ref") ||
             resolvedPath.rfind("/std/collections/soa_vector/ref", 0) == 0 ||
             resolvedPath.rfind("/soa_vector/ref", 0) == 0 ||
             resolvedPath.rfind("/std/collections/experimental_soa_vector/soaVectorRef", 0) == 0 ||
             resolvedPath.rfind("/std/collections/experimental_soa_storage/soaColumnRef", 0) == 0;
    };
    auto hasBorrowConflictForRoot =
        [&](const std::string &borrowRoot, bool requestMutable) -> bool {
          if (borrowRoot.empty() ||
              currentValidationState_.context.definitionIsUnsafe) {
            return false;
          }
          bool sawMutableBorrow = false;
          bool sawImmutableBorrow = false;
          auto referenceRootForBorrowBinding =
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
          auto observeBorrow = [&](const std::string &bindingName,
                                   const BindingInfo &binding) {
            if (currentValidationState_.endedReferenceBorrows.count(bindingName) > 0) {
              return;
            }
            const std::string root =
                referenceRootForBorrowBinding(bindingName, binding);
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
          return requestMutable ? (sawMutableBorrow || sawImmutableBorrow)
                                : sawMutableBorrow;
        };
    auto isMutableRootBinding = [&](const std::string &borrowRoot) -> bool {
      if (borrowRoot.empty()) {
        return false;
      }
      if (const BindingInfo *paramBinding = findParamBinding(params, borrowRoot)) {
        return paramBinding->isMutable;
      }
      auto it = locals.find(borrowRoot);
      return it != locals.end() && it->second.isMutable;
    };
    resolveReceiverRootExpr =
        [&](const Expr &expr,
            const ExprSubstitutions &substitutions,
            std::string &rootOut) -> bool {
          if (expr.kind == Expr::Kind::Name) {
            if (const Expr *substitutedExpr =
                    findSubstitutedExpr(substitutions, expr.name)) {
              return resolveReceiverRootExpr(*substitutedExpr,
                                            substitutions,
                                            rootOut);
            }
            return resolveBorrowRoot(expr.name, rootOut);
          }
          if (expr.kind != Expr::Kind::Call) {
            return false;
          }
          std::string builtinName;
          if (getBuiltinPointerName(expr, builtinName) && expr.args.size() == 1) {
            if (builtinName == "location" || builtinName == "dereference") {
              return resolveReceiverRootExpr(expr.args.front(),
                                            substitutions,
                                            rootOut);
            }
          }
          ExprSubstitutions nestedSubstitutions;
          const Expr *returnedValueExpr = nullptr;
          if (!appendCallSubstitutions(expr, substitutions, nestedSubstitutions,
                                       returnedValueExpr)) {
            return false;
          }
          return resolveReceiverRootExpr(*returnedValueExpr,
                                        nestedSubstitutions,
                                        rootOut);
        };
    resolveStandaloneRefRootExpr =
        [&](const Expr &expr,
            const ExprSubstitutions &substitutions,
            std::string &rootOut) -> bool {
          if (expr.kind == Expr::Kind::Name) {
            if (const Expr *substitutedExpr =
                    findSubstitutedExpr(substitutions, expr.name)) {
              return resolveStandaloneRefRootExpr(*substitutedExpr,
                                                 substitutions,
                                                 rootOut);
            }
            return false;
          }
          if (expr.kind == Expr::Kind::Call && expr.args.size() == 2) {
            const std::string resolvedPath = resolveCalleePath(expr);
            const bool isMethodRefCall =
                expr.isMethodCall &&
                (expr.name == "ref" ||
                 resolvedPath.rfind("/std/collections/soa_vector/ref", 0) == 0 ||
                 resolvedPath.rfind("/soa_vector/ref", 0) == 0);
            const bool isHelperRefCall =
                !expr.isMethodCall &&
                (isSimpleCallName(expr, "ref") ||
                 resolvedPath.rfind("/std/collections/soa_vector/ref", 0) == 0 ||
                 resolvedPath.rfind("/soa_vector/ref", 0) == 0 ||
                 resolvedPath.rfind("/std/collections/experimental_soa_vector/soaVectorRef", 0) == 0 ||
                 resolvedPath.rfind("/std/collections/experimental_soa_storage/soaColumnRef", 0) == 0);
            if ((isMethodRefCall || isHelperRefCall) &&
                resolveReceiverRootExpr(expr.args.front(),
                                       substitutions,
                                       rootOut)) {
              return !rootOut.empty();
            }
          }
          if (expr.kind != Expr::Kind::Call) {
            return false;
          }
          ExprSubstitutions nestedSubstitutions;
          const Expr *returnedValueExpr = nullptr;
          if (!appendCallSubstitutions(expr, substitutions, nestedSubstitutions,
                                       returnedValueExpr)) {
            return false;
          }
          return resolveStandaloneRefRootExpr(*returnedValueExpr,
                                             nestedSubstitutions,
                                             rootOut);
        };

    std::string pointerName;
    const bool initIsLocation =
        init.kind == Expr::Kind::Call && getBuiltinPointerName(init, pointerName) && pointerName == "location" &&
        init.args.size() == 1;
    std::string safeTargetType;
    const bool initIsDirectBorrowStorage = resolveDirectBorrowStorageTargetType(init, safeTargetType);
    const bool initIsPointerLike = resolvePointerTargetType(init, safeTargetType);
    if (!initIsLocation && !initIsDirectBorrowStorage && !initIsPointerLike &&
        !currentValidationState_.context.definitionIsUnsafe) {
      return failBindingDiagnostic("Reference bindings require location(...)");
    }
    if (initIsLocation || initIsDirectBorrowStorage ||
        (!currentValidationState_.context.definitionIsUnsafe && initIsPointerLike)) {
      if (!errorTypesMatch(safeTargetType, info.typeTemplateArg, namespacePrefix)) {
        return failBindingDiagnostic("Reference binding type mismatch");
      }
    }
    if (!initIsLocation && !initIsDirectBorrowStorage && !currentValidationState_.context.definitionIsUnsafe) {
      std::string borrowRoot;
      const ExprSubstitutions substitutions;
      const bool resolvedStandaloneRoot =
          resolveStandaloneRefRootExpr(init, substitutions, borrowRoot);
      if (isStandaloneRefCall(init) &&
          (!resolvedStandaloneRoot || borrowRoot.empty())) {
        return failBindingDiagnostic("Reference binding requires borrow root");
      }
      if (resolvedStandaloneRoot && !borrowRoot.empty()) {
        if (hasBorrowConflictForRoot(borrowRoot, info.isMutable)) {
          return failBindingDiagnostic(
              "borrow conflict: " + borrowRoot + " (root: " + borrowRoot +
              ", sink: " + stmt.name + ")");
        }
        if (info.isMutable && !isMutableRootBinding(borrowRoot)) {
          return failBindingDiagnostic("Reference binding requires mutable root: " +
                                       borrowRoot);
        }
        info.referenceRoot = std::move(borrowRoot);
      }
      if (!validateBuiltinMapKeyType(info, definitionTemplateArgs, error_)) {
        return false;
      }
      locals.emplace(stmt.name, info);
      return true;
    }
    if (!initIsLocation && currentValidationState_.context.definitionIsUnsafe) {
      std::string pointerTargetType;
      if (!resolvePointerTargetType(init, pointerTargetType)) {
        return failBindingDiagnostic("unsafe Reference bindings require pointer-like initializer");
      }
      if (!errorTypesMatch(pointerTargetType, info.typeTemplateArg, namespacePrefix)) {
        return failBindingDiagnostic("unsafe Reference binding type mismatch");
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
    std::function<bool(const Expr &, std::string &)> resolveBorrowRootExpr;
    resolveBorrowRootExpr = [&](const Expr &targetExpr, std::string &rootOut) -> bool {
      if (targetExpr.kind == Expr::Kind::Name) {
        return resolveBorrowRoot(targetExpr.name, rootOut);
      }
      std::string builtinName;
      if (targetExpr.kind == Expr::Kind::Call && getBuiltinPointerName(targetExpr, builtinName) &&
          builtinName == "dereference" && targetExpr.args.size() == 1) {
        return resolvePointerRoot(targetExpr.args.front(), rootOut);
      }
      if (targetExpr.kind == Expr::Kind::Call && targetExpr.isFieldAccess && targetExpr.args.size() == 1) {
        std::string receiverRoot;
        if (!resolveBorrowRootExpr(targetExpr.args.front(), receiverRoot) || receiverRoot.empty()) {
          return false;
        }
        rootOut = receiverRoot + "." + targetExpr.name;
        return true;
      }
      return false;
    };

    std::string borrowRoot;
    if (!resolveBorrowRootExpr(target, borrowRoot) || borrowRoot.empty()) {
      return failBindingDiagnostic("Reference bindings require location(...)");
    }
    bool sawMutableBorrow = false;
    bool sawImmutableBorrow = false;
    auto observeBorrow = [&](const std::string &bindingName, const BindingInfo &binding) {
      if (currentValidationState_.endedReferenceBorrows.count(bindingName) > 0) {
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
    if (conflict && !currentValidationState_.context.definitionIsUnsafe) {
      return failBindingDiagnostic("borrow conflict: " + borrowRoot + " (root: " + borrowRoot +
                                   ", sink: " + stmt.name + ")");
    }
    info.referenceRoot = std::move(borrowRoot);
    info.isUnsafeReference = currentValidationState_.context.definitionIsUnsafe;
  }

  BindingInfo fieldViewBinding = info;
  if ((!hasExplicitType || explicitAutoType) && info.typeName != "Reference") {
    BindingInfo inferredBinding;
    if (inferBindingTypeFromInitializer(initializer, params, locals, inferredBinding, &stmt)) {
      fieldViewBinding = std::move(inferredBinding);
    }
  }
  if (isSoaFieldViewBindingType(fieldViewBinding)) {
    auto hasBorrowConflictForRoot =
        [&](const std::string &borrowRoot, bool requestMutable) -> bool {
          if (borrowRoot.empty() ||
              currentValidationState_.context.definitionIsUnsafe) {
            return false;
          }
          bool sawMutableBorrow = false;
          bool sawImmutableBorrow = false;
          auto observeBorrow = [&](const std::string &bindingName,
                                   const BindingInfo &binding) {
            if (currentValidationState_.endedReferenceBorrows.count(bindingName) > 0) {
              return;
            }
            const std::string root =
                referenceRootForBorrowBinding(bindingName, binding);
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
          return requestMutable ? (sawMutableBorrow || sawImmutableBorrow)
                                : sawMutableBorrow;
        };
    auto isMutableRootBinding = [&](const std::string &borrowRoot) -> bool {
      if (borrowRoot.empty()) {
        return false;
      }
      if (const BindingInfo *paramBinding = findParamBinding(params, borrowRoot)) {
        return paramBinding->isMutable;
      }
      auto it = locals.find(borrowRoot);
      return it != locals.end() && it->second.isMutable;
    };
    auto resolveBorrowRootName = [&](const std::string &name,
                                     std::string &rootOut) -> bool {
      if (const BindingInfo *paramBinding = findParamBinding(params, name)) {
        if (paramBinding->typeName == "Reference" ||
            isSoaFieldViewBindingType(*paramBinding)) {
          rootOut = referenceRootForBorrowBinding(name, *paramBinding);
        } else {
          rootOut = name;
        }
        return true;
      }
      auto it = locals.find(name);
      if (it == locals.end()) {
        return false;
      }
      if (it->second.typeName == "Reference" ||
          isSoaFieldViewBindingType(it->second)) {
        rootOut = referenceRootForBorrowBinding(it->first, it->second);
      } else {
        rootOut = name;
      }
      return true;
    };
    using ExprSubstitutions = std::vector<std::pair<std::string, const Expr *>>;
    auto findSubstitutedExpr = [&](const ExprSubstitutions &substitutions,
                                   const std::string &name) -> const Expr * {
      for (auto it = substitutions.rbegin(); it != substitutions.rend(); ++it) {
        if (it->first == name) {
          return it->second;
        }
      }
      return nullptr;
    };
    auto appendCallSubstitutions =
        [&](const Expr &callExpr,
            const ExprSubstitutions &baseSubstitutions,
            ExprSubstitutions &extendedSubstitutions,
            const Expr *&returnedValueExprOut) -> bool {
          returnedValueExprOut = nullptr;
          const std::string resolvedCallPath = resolveCalleePath(callExpr);
          auto defIt = defMap_.find(resolvedCallPath);
          if (defIt == defMap_.end() || defIt->second == nullptr) {
            return false;
          }
          const auto paramsIt = paramsByDef_.find(resolvedCallPath);
          if (paramsIt == paramsByDef_.end()) {
            return false;
          }
          const auto &nestedParams = paramsIt->second;
          std::string nestedArgError;
          std::vector<const Expr *> nestedOrderedArgs;
          if (!buildOrderedArguments(nestedParams, callExpr.args, callExpr.argNames,
                                     nestedOrderedArgs, nestedArgError)) {
            return false;
          }
          const Definition &nestedDef = *defIt->second;
          for (const auto &stmtExpr : nestedDef.statements) {
            if (isReturnCall(stmtExpr) && stmtExpr.args.size() == 1) {
              returnedValueExprOut = &stmtExpr.args.front();
            }
          }
          if (nestedDef.returnExpr.has_value()) {
            returnedValueExprOut = &*nestedDef.returnExpr;
          }
          if (returnedValueExprOut == nullptr) {
            return false;
          }
          extendedSubstitutions = baseSubstitutions;
          for (size_t nestedIndex = 0;
               nestedIndex < nestedParams.size() && nestedIndex < nestedOrderedArgs.size();
               ++nestedIndex) {
            const Expr *nestedArg = nestedOrderedArgs[nestedIndex];
            if (nestedArg == nullptr) {
              continue;
            }
            extendedSubstitutions.emplace_back(nestedParams[nestedIndex].name, nestedArg);
          }
          return true;
        };
    std::function<bool(const Expr &, const ExprSubstitutions &, std::string &)>
        resolveReceiverRootExpr;
    std::function<bool(const Expr &, const ExprSubstitutions &, std::string &)>
        resolveStandaloneFieldViewRootExpr;
    resolveReceiverRootExpr =
        [&](const Expr &expr,
            const ExprSubstitutions &substitutions,
            std::string &rootOut) -> bool {
          if (expr.kind == Expr::Kind::Name) {
            if (const Expr *substitutedExpr =
                    findSubstitutedExpr(substitutions, expr.name)) {
              return resolveReceiverRootExpr(*substitutedExpr,
                                            substitutions,
                                            rootOut);
            }
            return resolveBorrowRootName(expr.name, rootOut);
          }
          if (expr.kind != Expr::Kind::Call) {
            return false;
          }
          std::string builtinName;
          if (getBuiltinPointerName(expr, builtinName) && expr.args.size() == 1) {
            if (builtinName == "location" || builtinName == "dereference") {
              return resolveReceiverRootExpr(expr.args.front(),
                                            substitutions,
                                            rootOut);
            }
          }
          ExprSubstitutions nestedSubstitutions;
          const Expr *returnedValueExpr = nullptr;
          if (!appendCallSubstitutions(expr, substitutions, nestedSubstitutions,
                                       returnedValueExpr)) {
            return false;
          }
          return resolveReceiverRootExpr(*returnedValueExpr,
                                        nestedSubstitutions,
                                        rootOut);
        };
    resolveStandaloneFieldViewRootExpr =
        [&](const Expr &expr,
            const ExprSubstitutions &substitutions,
            std::string &rootOut) -> bool {
          if (expr.kind == Expr::Kind::Name) {
            if (const Expr *substitutedExpr =
                    findSubstitutedExpr(substitutions, expr.name)) {
              return resolveStandaloneFieldViewRootExpr(*substitutedExpr,
                                                       substitutions,
                                                       rootOut);
            }
            const BindingInfo *binding = findParamBinding(params, expr.name);
            if (binding == nullptr) {
              auto localIt = locals.find(expr.name);
              if (localIt != locals.end()) {
                binding = &localIt->second;
              }
            }
            if (binding != nullptr && isSoaFieldViewBindingType(*binding)) {
              rootOut = referenceRootForBorrowBinding(expr.name, *binding);
              return !rootOut.empty();
            }
            return false;
          }
          if (expr.kind == Expr::Kind::Call && expr.args.size() >= 1) {
            if (isBuiltinSoaFieldViewExpr(expr, params, locals, nullptr)) {
              const Expr *receiverExpr = &expr.args.front();
              if (receiverExpr->kind == Expr::Kind::Name) {
                if (const Expr *substitutedExpr =
                        findSubstitutedExpr(substitutions, receiverExpr->name)) {
                  receiverExpr = substitutedExpr;
                }
              }
              if (receiverExpr->kind != Expr::Kind::Name) {
                return false;
              }
              if (resolveReceiverRootExpr(*receiverExpr, substitutions, rootOut)) {
                return !rootOut.empty();
              }
            }
          }
          if (expr.kind != Expr::Kind::Call) {
            return false;
          }
          ExprSubstitutions nestedSubstitutions;
          const Expr *returnedValueExpr = nullptr;
          if (!appendCallSubstitutions(expr, substitutions, nestedSubstitutions,
                                       returnedValueExpr)) {
            return false;
          }
          return resolveStandaloneFieldViewRootExpr(*returnedValueExpr,
                                                    nestedSubstitutions,
                                                    rootOut);
        };

    std::string borrowRoot;
    const ExprSubstitutions substitutions;
    if (!resolveStandaloneFieldViewRootExpr(initializer, substitutions, borrowRoot) ||
        borrowRoot.empty()) {
      return failBindingDiagnostic("field-view binding requires borrow root");
    }
    if (hasBorrowConflictForRoot(borrowRoot, info.isMutable)) {
      return failBindingDiagnostic("borrow conflict: " + borrowRoot + " (root: " +
                                   borrowRoot + ", sink: " + stmt.name + ")");
    }
    if (info.isMutable && !isMutableRootBinding(borrowRoot)) {
      return failBindingDiagnostic("field-view binding requires mutable root: " +
                                   borrowRoot);
    }
    info.referenceRoot = std::move(borrowRoot);
  }

  if (!validateBuiltinMapKeyType(info, definitionTemplateArgs, error_)) {
    return false;
  }
  locals.emplace(stmt.name, info);
  return true;
}

} // namespace primec::semantics
