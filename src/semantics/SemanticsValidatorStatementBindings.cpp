#include "SemanticsValidator.h"

#include "SemanticsValidatorInferCollectionCompatibilityInternal.h"

#include <algorithm>
#include <functional>
#include <optional>
#include <unordered_set>
#include "primec/ir/StdlibCollectionPaths.h"
#include "primec/support/CollectionHelperNames.h"
#include "SemanticsValidatorStatementBindingsHelpers.h"
#include "SemanticsValidatorStatementBindingsState.h"

namespace primec::semantics {
using namespace statementBindingsHelpers;

bool SemanticsValidator::validateBindingStatement(const std::vector<ParameterInfo> &params,
                                                  std::unordered_map<std::string, BindingInfo> &locals,
                                                  const Expr &stmt,
                                                  bool allowBindings,
                                                  const std::string &namespacePrefix,
                                                  bool allowCompileTimeTypeBindings,
                                                  bool &handled) {
    ValidateBindingState st;
    st.allowBindings = allowBindings;
    st.allowCompileTimeTypeBindings = allowCompileTimeTypeBindings;
    if (validateBindingPhase1(params, locals, stmt, namespacePrefix, handled, st) == PhaseStatus::Done) {
      return st.result;
    }
    if (validateBindingPhase2(params, locals, stmt, namespacePrefix, handled, st) == PhaseStatus::Done) {
      return st.result;
    }
    if (validateBindingPhase3(params, locals, stmt, namespacePrefix, handled, st) == PhaseStatus::Done) {
      return st.result;
    }
    if (validateBindingPhase4(params, locals, stmt, namespacePrefix, handled, st) == PhaseStatus::Done) {
      return st.result;
    }
    if (validateBindingPhase5(params, locals, stmt, namespacePrefix, handled, st) == PhaseStatus::Done) {
      return st.result;
    }
    if (validateBindingPhase6(params, locals, stmt, namespacePrefix, handled, st) == PhaseStatus::Done) {
      return st.result;
    }
    if (validateBindingPhase7(params, locals, stmt, namespacePrefix, handled, st) == PhaseStatus::Done) {
      return st.result;
    }
    return st.result;
}

PhaseStatus SemanticsValidator::validateBindingPhase1([[maybe_unused]] const std::vector<ParameterInfo> &params, [[maybe_unused]] std::unordered_map<std::string, BindingInfo> &locals, [[maybe_unused]] const Expr &stmt, [[maybe_unused]] const std::string &namespacePrefix, [[maybe_unused]] bool &handled, ValidateBindingState &st) {
  [[maybe_unused]] auto &allowBindings = st.allowBindings;
  [[maybe_unused]] auto &allowCompileTimeTypeBindings = st.allowCompileTimeTypeBindings;
  handled = false;
  if (!stmt.isBinding) {
    return st.done(true);
  }
  handled = true;
  st.failBindingDiagnostic = [&](std::string message) -> bool {
    return failExprDiagnostic(stmt, std::move(message));
  };
  [[maybe_unused]] auto &failBindingDiagnostic = st.failBindingDiagnostic;
  st.definitionTemplateArgs = nullptr;
  [[maybe_unused]] auto &definitionTemplateArgs = st.definitionTemplateArgs;
  auto currentDefIt = defMap_.find(currentValidationState_.context.definitionPath);
  if (currentDefIt != defMap_.end() && currentDefIt->second != nullptr) {
    definitionTemplateArgs = &currentDefIt->second->templateArgs;
  }
  st.bindingLookupNamespace = !currentValidationState_.context.definitionPath.empty()
          ? currentValidationState_.context.definitionPath
          : namespacePrefix;
  [[maybe_unused]] auto &bindingLookupNamespace = st.bindingLookupNamespace;
  if (!allowBindings) {
    return st.done(failBindingDiagnostic("binding not allowed in execution body"));
  }
  if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
    return st.done(failBindingDiagnostic("binding does not accept block arguments"));
  }
  if (isCompileTimeTypeBinding(stmt)) {
    if (!allowCompileTimeTypeBindings) {
      return st.done(failBindingDiagnostic("type bindings are only supported in definition bodies"));
    }
    if (isParam(params, stmt.name) || locals.count(stmt.name) > 0 ||
        currentValidationState_.compileTimeTypeLocals.count(stmt.name) > 0) {
      return st.done(failBindingDiagnostic("duplicate binding name: " + stmt.name));
    }
    bool sawTypeTransform = false;
    for (const auto &transform : stmt.transforms) {
      if (transform.name == "type") {
        if (sawTypeTransform) {
          return st.done(failBindingDiagnostic("duplicate type transform on binding"));
        }
        sawTypeTransform = true;
        if (!transform.templateArgs.empty() || !transform.arguments.empty()) {
          return st.done(failBindingDiagnostic("type binding transform does not take arguments"));
        }
        continue;
      }
      if (isBindingAuxTransformName(transform.name)) {
        return st.done(failBindingDiagnostic("type binding does not accept binding qualifier: " + transform.name));
      }
      return st.done(failBindingDiagnostic("type binding requires only the type transform"));
    }
    if (!sawTypeTransform) {
      return st.done(failBindingDiagnostic("type binding requires type transform"));
    }
    if (stmt.args.size() != 1) {
      return st.done(failBindingDiagnostic("type binding requires exactly one initializer"));
    }
    const Expr *typeExpr = &stmt.args.front();
    const Expr &initializer = stmt.args.front();
    if (initializer.kind == Expr::Kind::Call && initializer.name == "block" &&
        initializer.hasBodyArguments && initializer.args.empty() &&
        initializer.templateArgs.empty() && !hasNamedArguments(initializer.argNames)) {
      if (initializer.bodyArguments.size() != 1) {
        return st.done(failBindingDiagnostic("type binding initializer must be a single type expression"));
      }
      typeExpr = &initializer.bodyArguments.front();
    } else if (initializer.hasBodyArguments || !initializer.bodyArguments.empty() ||
               hasNamedArguments(stmt.argNames)) {
      return st.done(failBindingDiagnostic("type binding initializer must be a single type expression"));
    }
    auto resolveNamedConcreteType = [&](const Expr &namedType,
                                        std::string &resolvedTypeOut) -> bool {
      if (namedType.kind != Expr::Kind::Name || namedType.name == "auto") {
        return false;
      }
      if (isPrimitiveBindingTypeName(namedType.name)) {
        resolvedTypeOut = normalizeBindingTypeName(namedType.name);
        return true;
      }
      resolvedTypeOut =
          resolveStructTypePath(namedType.name, bindingLookupNamespace, structNames_);
      if (resolvedTypeOut.empty()) {
        auto importIt = importAliases_.find(namedType.name);
        if (importIt != importAliases_.end() &&
            (structNames_.count(importIt->second) > 0 ||
             sumNames_.count(importIt->second) > 0)) {
          resolvedTypeOut = importIt->second;
        }
      }
      if (resolvedTypeOut.empty() && sumNames_.count(namedType.name) > 0) {
        resolvedTypeOut = namedType.name;
      }
      if (resolvedTypeOut.empty() && !namedType.name.empty() &&
          namedType.name.front() == '/' &&
          (structNames_.count(namedType.name) > 0 ||
           sumNames_.count(namedType.name) > 0)) {
        resolvedTypeOut = namedType.name;
      }
      return !resolvedTypeOut.empty();
    };
    auto appendZeroArgCallablePath = [&](const std::string &path,
                                         std::vector<std::string> &paths) {
      if (path.empty() || std::find(paths.begin(), paths.end(), path) != paths.end()) {
        return;
      }
      auto defIt = defMap_.find(path);
      if (defIt == defMap_.end() || defIt->second == nullptr ||
          !defIt->second->parameters.empty() ||
          !defIt->second->templateArgs.empty() ||
          structNames_.count(defIt->second->fullPath) > 0 ||
          isSumDefinition(*defIt->second)) {
        return;
      }
      paths.push_back(path);
    };
    auto visibleZeroArgCallablePaths = [&](const std::string &symbol) {
      std::vector<std::string> paths;
      if (symbol.empty() || symbol.find('/') != std::string::npos) {
        return paths;
      }
      Expr lookup;
      lookup.kind = Expr::Kind::Call;
      lookup.name = symbol;
      lookup.namespacePrefix = namespacePrefix;
      appendZeroArgCallablePath(resolveCalleePath(lookup), paths);
      if (auto importIt = directImportAliases_.find(symbol);
          importIt != directImportAliases_.end()) {
        appendZeroArgCallablePath(importIt->second, paths);
      }
      if (auto importIt = transitiveImportAliases_.find(symbol);
          importIt != transitiveImportAliases_.end()) {
        appendZeroArgCallablePath(importIt->second, paths);
      }
      if (auto importIt = importAliases_.find(symbol);
          importIt != importAliases_.end()) {
        appendZeroArgCallablePath(importIt->second, paths);
      }
      std::sort(paths.begin(), paths.end());
      paths.erase(std::unique(paths.begin(), paths.end()), paths.end());
      return paths;
    };
    auto resolveTypeofSymbol = [&](const Expr &typeofExpr,
                                   std::string &resolvedTypeOut) -> bool {
      if (typeofExpr.isMethodCall || typeofExpr.isFieldAccess ||
          typeofExpr.isBinding || !typeofExpr.args.empty() ||
          typeofExpr.hasBodyArguments || !typeofExpr.bodyArguments.empty() ||
          hasNamedArguments(typeofExpr.argNames)) {
        return failBindingDiagnostic(
            "typeof requires compile-time symbol syntax: typeof<symbol>");
      }
      if (typeofExpr.templateArgs.size() != 1 ||
          typeofExpr.templateArgDetails.size() != 1 ||
          typeofExpr.templateArgDetails.front().text !=
              typeofExpr.templateArgs.front()) {
        return failBindingDiagnostic(
            "typeof requires exactly one compile-time symbol argument");
      }
      if (typeofExpr.templateArgDetails.front().kind !=
          TemplateArgumentKind::Symbol) {
        return failBindingDiagnostic("typeof requires a symbol argument");
      }
      const std::string &symbol = typeofExpr.templateArgs.front();
      const BindingInfo *paramBinding = findParamBinding(params, symbol);
      const auto localIt = locals.find(symbol);
      const auto typeLocalIt =
          currentValidationState_.compileTimeTypeLocals.find(symbol);
      const std::vector<std::string> zeroArgPaths =
          visibleZeroArgCallablePaths(symbol);
      const bool hasValueOrTypeFact =
          paramBinding != nullptr || localIt != locals.end() ||
          typeLocalIt != currentValidationState_.compileTimeTypeLocals.end();
      if (hasValueOrTypeFact && !zeroArgPaths.empty()) {
        return failBindingDiagnostic(
            "ambiguous typeof symbol: " + symbol +
            " could refer to a local value or " +
            formatPathListForTypeof(zeroArgPaths));
      }
      if (zeroArgPaths.size() > 1) {
        return failBindingDiagnostic(
            "ambiguous typeof symbol: " + symbol + " could refer to " +
            formatPathListForTypeof(zeroArgPaths));
      }
      if (paramBinding != nullptr) {
        resolvedTypeOut = bindingTypeTextForTypeof(*paramBinding);
      } else if (localIt != locals.end()) {
        resolvedTypeOut = bindingTypeTextForTypeof(localIt->second);
      } else if (typeLocalIt !=
                 currentValidationState_.compileTimeTypeLocals.end()) {
        resolvedTypeOut = typeLocalIt->second;
      } else if (!zeroArgPaths.empty()) {
        return failBindingDiagnostic("typeof symbol is not a value: " + symbol);
      } else {
        return failBindingDiagnostic("unknown typeof symbol: " + symbol);
      }
      if (resolvedTypeOut.empty() ||
          normalizeBindingTypeName(resolvedTypeOut) == "auto") {
        return failBindingDiagnostic("typeof requires a concrete value type: " +
                                     symbol);
      }
      return true;
    };
    std::string resolvedType;
    if (typeExpr->kind == Expr::Kind::Call && typeExpr->name == "typeof") {
      if (!resolveTypeofSymbol(*typeExpr, resolvedType)) {
        return st.done(false);
      }
    } else if (!resolveNamedConcreteType(*typeExpr, resolvedType)) {
      return st.done(failBindingDiagnostic(
          "type binding initializer requires a concrete type"));
    }
    currentValidationState_.compileTimeTypeLocals.emplace(stmt.name, std::move(resolvedType));
    return st.done(true);
  }
  if (stmt.transforms.empty() && !stmt.args.empty()) {
    const std::string lookupNamespace =
        !stmt.namespacePrefix.empty() ? stmt.namespacePrefix : namespacePrefix;
    const std::string structPath =
        resolveStructTypePath(stmt.name, lookupNamespace, structNames_);
    if (!structPath.empty()) {
      Expr constructorExpr = stmt;
      constructorExpr.isBinding = false;
      constructorExpr.isBraceConstructor = true;
      constructorExpr.name = structPath;
      constructorExpr.namespacePrefix.clear();
      if (!validateExpr(params, locals, constructorExpr)) {
        return st.done(false);
      }
      return st.done(true);
    }
  }
  if (isParam(params, stmt.name) || locals.count(stmt.name) > 0 ||
      currentValidationState_.compileTimeTypeLocals.count(stmt.name) > 0) {
    return st.done(failBindingDiagnostic("duplicate binding name: " + stmt.name));
  }
  for (const auto &transform : stmt.transforms) {
    if (!isInternalSoaCollectionTypeName(transform.name) ||
        transform.templateArgs.size() != 1) {
      continue;
    }
    if (!isSoaVectorStructElementType(transform.templateArgs.front(), namespacePrefix, structNames_, importAliases_)) {
      break;
    }
    if (!validateSoaVectorElementFieldEnvelopes(transform.templateArgs.front(), namespacePrefix)) {
      return st.done(false);
    }
    break;
  }
  [[maybe_unused]] auto &info = st.info;
  [[maybe_unused]] auto &restrictType = st.restrictType;
  if (currentDefIt != defMap_.end() && currentDefIt->second != nullptr &&
      structNames_.count(currentValidationState_.context.definitionPath) > 0) {
    if (!resolveStructFieldBinding(*currentDefIt->second, stmt, info)) {
      return st.done(false);
    }
    if (stmt.args.size() == 1 && stmt.args.front().kind == Expr::Kind::Call &&
        isIfCall(stmt.args.front()) && !validateIfExpr(params, locals, stmt.args.front())) {
      return st.done(false);
    }
    if (!validateBuiltinComparableKeyType(info, definitionTemplateArgs, error_)) {
      return st.done(false);
    }
    insertLocalBinding(locals, stmt.name, std::move(info));
    return st.done(true);
  }
  return PhaseStatus::Continue;
}

PhaseStatus SemanticsValidator::validateBindingPhase2([[maybe_unused]] const std::vector<ParameterInfo> &params, [[maybe_unused]] std::unordered_map<std::string, BindingInfo> &locals, [[maybe_unused]] const Expr &stmt, [[maybe_unused]] const std::string &namespacePrefix, [[maybe_unused]] bool &handled, ValidateBindingState &st) {
  [[maybe_unused]] auto &allowBindings = st.allowBindings;
  [[maybe_unused]] auto &allowCompileTimeTypeBindings = st.allowCompileTimeTypeBindings;
  [[maybe_unused]] auto &failBindingDiagnostic = st.failBindingDiagnostic;
  [[maybe_unused]] auto &definitionTemplateArgs = st.definitionTemplateArgs;
  [[maybe_unused]] auto &bindingLookupNamespace = st.bindingLookupNamespace;
  [[maybe_unused]] auto &info = st.info;
  [[maybe_unused]] auto &restrictType = st.restrictType;
  if (!parseBindingInfo(stmt,
                        bindingLookupNamespace,
                        structNames_,
                        importAliases_,
                        info,
                        restrictType,
                        error_,
                        &sumNames_,
                        &currentValidationState_.compileTimeTypeLocals,
                        // TODO-5251: local bindings are the first
                        // non-parameter context being extended real
                        // Reference/Pointer/Slice capability support.
                        /*allowCapabilityArg=*/true)) {
    return st.done(false);
  }
  if (info.isMove) {
    return st.done(
        failBindingDiagnostic("move transform is only supported on parameters: " + stmt.name));
  }
  std::string parsedSoaElementType;
  if (extractExperimentalSoaVectorElementType(info, parsedSoaElementType) &&
      isSoaVectorStructElementType(parsedSoaElementType,
                                   namespacePrefix,
                                   structNames_,
                                   importAliases_) &&
      !validateSoaVectorElementFieldEnvelopes(parsedSoaElementType,
                                              namespacePrefix)) {
    return st.done(false);
  }
  st.hasExplicitType = hasExplicitBindingTypeTransform(stmt);
  [[maybe_unused]] auto &hasExplicitType = st.hasExplicitType;
  st.explicitAutoType = hasExplicitType && normalizeBindingTypeName(info.typeName) == "auto";
  [[maybe_unused]] auto &explicitAutoType = st.explicitAutoType;
  if (stmt.args.size() == 1 && stmt.args.front().isLambda && (!hasExplicitType || explicitAutoType)) {
    info.typeName = "lambda";
    info.typeTemplateArg.clear();
  }
  if (stmt.args.empty()) {
    if (structNames_.count(currentValidationState_.context.definitionPath) > 0) {
      if (restrictType.has_value()) {
        const bool hasTemplate = !info.typeTemplateArg.empty();
        if (!restrictMatchesBinding(*restrictType, info.typeName, info.typeTemplateArg, hasTemplate, namespacePrefix)) {
          return st.done(failBindingDiagnostic("restrict type does not match binding type"));
        }
      }
      if (!validateBuiltinComparableKeyType(info, definitionTemplateArgs, error_)) {
        return st.done(false);
      }
      insertLocalBinding(locals, stmt.name, std::move(info));
      return st.done(true);
    }
    if (!validateOmittedBindingInitializer(stmt, info, namespacePrefix)) {
      return st.done(false);
    }
    if (restrictType.has_value()) {
      const bool hasTemplate = !info.typeTemplateArg.empty();
      if (!restrictMatchesBinding(*restrictType, info.typeName, info.typeTemplateArg, hasTemplate, namespacePrefix)) {
        return st.done(failBindingDiagnostic("restrict type does not match binding type"));
      }
    }
    if (!validateBuiltinComparableKeyType(info, definitionTemplateArgs, error_)) {
      return st.done(false);
    }
    insertLocalBinding(locals, stmt.name, std::move(info));
    return st.done(true);
  }
  if (stmt.args.size() != 1) {
    return st.done(failBindingDiagnostic("binding requires exactly one argument"));
  }
  st.initializer = &(stmt.args.front());
  [[maybe_unused]] const Expr &initializer = *st.initializer;
  if (initializer.kind == Expr::Kind::Call && !initializer.isMethodCall) {
    const bool explicitOldGetRef =
        initializer.name == samePathSoaHelperTargetPath(collection_helpers::kGetRef) ||
        initializer.name == internalSoaCollectionTypeName() + "/get_ref" ||
        (isCompatibilitySoaSurfaceNamespace(initializer.namespacePrefix) &&
         initializer.name == collection_helpers::kGetRef);
    if (explicitOldGetRef &&
        !hasVisibleDefinitionPathForCurrentImports(
            samePathSoaHelperTargetPath(collection_helpers::kGetRef))) {
      return st.done(failBindingDiagnostic("get_ref is only supported as a statement"));
    }
  }
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
  if (explicitAutoType && initializer.kind == Expr::Kind::Call &&
      initializer.isBraceConstructor && hasNamedArguments(initializer.argNames) &&
      normalizeBindingTypeName(initializer.name) == "auto") {
    return st.done(failBindingDiagnostic("sum construction requires target sum type"));
  }
  if ((normalizedBindingType == "vector" ||
       isInternalSoaCollectionTypeName(normalizedBindingType)) &&
      isEmptyBuiltinBlockInitializer(initializer)) {
    if (!validateOmittedBindingInitializer(stmt, info, namespacePrefix)) {
      return st.done(false);
    }
    if (restrictType.has_value()) {
      const bool hasTemplate = !info.typeTemplateArg.empty();
      if (!restrictMatchesBinding(*restrictType, info.typeName, info.typeTemplateArg, hasTemplate, namespacePrefix)) {
        return st.done(failBindingDiagnostic("restrict type does not match binding type"));
      }
    }
    if (!validateBuiltinComparableKeyType(info, definitionTemplateArgs, error_)) {
      return st.done(false);
    }
    insertLocalBinding(locals, stmt.name, std::move(info));
    return st.done(true);
  }
  auto isUnsupportedRootSoaToAosBindingInitializer = [&]() {
    if (initializer.kind != Expr::Kind::Call || initializer.args.size() != 1) {
      return false;
    }
    std::string normalizedCallName = initializer.name;
    if (!normalizedCallName.empty() && normalizedCallName.front() == '/') {
      normalizedCallName.erase(normalizedCallName.begin());
    }
    std::string normalizedCallPrefix = initializer.namespacePrefix;
    if (!normalizedCallPrefix.empty() && normalizedCallPrefix.front() == '/') {
      normalizedCallPrefix.erase(normalizedCallPrefix.begin());
    }
    std::string resolvedCallPath =
        preferredCollectionHelperResolvedPath(initializer);
    if (resolvedCallPath.empty()) {
      resolvedCallPath = resolveCalleePath(initializer);
    }
    resolvedCallPath = canonicalizeLegacySoaToAosHelperPath(
        resolveExprConcreteCallPath(params, locals, initializer, resolvedCallPath));
    const bool isRootToAosHelper =
        isSoaConversionSurfaceSpelling(normalizedCallPrefix,
                                       normalizedCallName) ||
        isLegacyOrCanonicalSoaHelperPath(resolvedCallPath, "to_aos") ||
        isLegacyOrCanonicalSoaHelperPath(resolvedCallPath, collection_helpers::kToAosRef);
    if (!isRootToAosHelper) {
      return false;
    }
    const bool isBorrowedToAosHelper =
        isLegacyOrCanonicalSoaHelperPath(resolvedCallPath, collection_helpers::kToAosRef);
    const std::string helperPath =
        isBorrowedToAosHelper ? "/to_aos_ref" : "/to_aos";
    const std::string canonicalHelperPath =
        isBorrowedToAosHelper
            ? compatibilitySoaHelperTargetPath(collection_helpers::kToAosRef)
            : compatibilitySoaHelperTargetPath("to_aos");
    const std::string publicHelperPath = publicSoaHelperTargetPath("to_aos");
    auto hasExplicitSourceImportPath = [&](const std::string &path) {
      for (const auto &importPath : program_.sourceImports) {
        if (importPath == path) {
          return true;
        }
        if (importPath.size() >= 2 &&
            importPath.compare(importPath.size() - 2, 2, "/*") == 0) {
          const std::string prefix = importPath.substr(0, importPath.size() - 2);
          if (path == prefix || path.rfind(prefix + "/", 0) == 0) {
            return true;
          }
        }
      }
      return false;
    };
    if (hasDeclaredDefinitionPath(helperPath) ||
        hasExplicitSourceImportPath(helperPath) ||
        (!isBorrowedToAosHelper && hasExplicitSourceImportPath(publicHelperPath)) ||
        hasExplicitSourceImportPath(canonicalHelperPath)) {
      return false;
    }
    const Expr &receiver = initializer.args.front();
    const BindingInfo *receiverBinding = nullptr;
    if (receiver.kind == Expr::Kind::Name) {
      receiverBinding = findParamBinding(params, receiver.name);
      if (receiverBinding == nullptr) {
        auto localIt = locals.find(receiver.name);
        if (localIt != locals.end()) {
          receiverBinding = &localIt->second;
        }
      }
    }
    return receiverBinding != nullptr &&
           isInternalSoaCollectionTypeName(
               normalizeBindingTypeName(receiverBinding->typeName)) &&
           !receiverBinding->typeTemplateArg.empty();
  };
  if (normalizedBindingType == "vector" &&
      isUnsupportedRootSoaToAosBindingInitializer()) {
    return st.done(failBindingDiagnostic("binding initializer type mismatch"));
  }
  st.entryArgInit = isEntryArgsAccess(initializer);
  [[maybe_unused]] auto &entryArgInit = st.entryArgInit;
  st.entryArgStringInit = isEntryArgStringBinding(locals, initializer);
  [[maybe_unused]] auto &entryArgStringInit = st.entryArgStringInit;
  [[maybe_unused]] auto &entryArgScope = st.entryArgScope;
  if (entryArgInit || entryArgStringInit) {
    entryArgScope.emplace(*this, true);
  }
  st.isStandaloneSoaFieldViewInitializer = [&]() {
    if (const auto pendingPath =
            builtinSoaDirectPendingHelperPath(initializer, params, locals)) {
      std::string pendingFieldName;
      if (splitSoaFieldViewHelperPath(*pendingPath, &pendingFieldName)) {
        return true;
      }
    }
    if (initializer.kind == Expr::Kind::Call) {
      std::string resolvedPath = preferredCollectionHelperResolvedPath(initializer);
      if (resolvedPath.empty()) {
        resolvedPath = resolveCalleePath(initializer);
      }
      if (isExperimentalSoaFieldViewHelperPath(resolvedPath)) {
        return true;
      }
    }
    return false;
  };
  [[maybe_unused]] auto &isStandaloneSoaFieldViewInitializer = st.isStandaloneSoaFieldViewInitializer;
  st.validateAndRecordTargetTypedSumInitializer = [&]() -> std::optional<bool> {
    if (!hasExplicitType || explicitAutoType) {
      return std::nullopt;
    }
    bool handledSumInitializer = false;
    if (!validateTargetTypedSumInitializer(expectedBindingTypeText(info),
                                           initializer,
                                           params,
                                           locals,
                                           namespacePrefix,
                                           handledSumInitializer)) {
      return false;
    }
    if (!handledSumInitializer) {
      return std::nullopt;
    }
    if (restrictType.has_value()) {
      const bool hasTemplate = !info.typeTemplateArg.empty();
      if (!restrictMatchesBinding(*restrictType, info.typeName,
                                  info.typeTemplateArg, hasTemplate,
                                  namespacePrefix)) {
        return failBindingDiagnostic("restrict type does not match binding type");
      }
    }
    if (!validateBuiltinComparableKeyType(info, definitionTemplateArgs, error_)) {
      return false;
    }
    insertLocalBinding(locals, stmt.name, std::move(info));
    return true;
  };
  [[maybe_unused]] auto &validateAndRecordTargetTypedSumInitializer = st.validateAndRecordTargetTypedSumInitializer;
  st.isTargetTypedSumInitializerSyntax = [&]() {
    if (initializer.kind == Expr::Kind::Call &&
        isSimpleCallName(initializer, "move")) {
      return false;
    }
    if (initializer.kind == Expr::Kind::Name) {
      return hasExplicitType && !explicitAutoType &&
             resolveSumDefinitionForTypeText(expectedBindingTypeText(info),
                                             namespacePrefix) != nullptr;
    }
    if (initializer.kind != Expr::Kind::Call) {
      return false;
    }
    if (initializer.isBraceConstructor ||
        isEmptyBuiltinBlockInitializer(initializer)) {
      return true;
    }
    return hasExplicitType && !explicitAutoType &&
           !initializer.isMethodCall && !initializer.isFieldAccess &&
           initializer.name == "block" &&
           hasNamedArguments(initializer.argNames) &&
           resolveSumDefinitionForTypeText(expectedBindingTypeText(info),
                                           namespacePrefix) != nullptr;
  };
  [[maybe_unused]] auto &isTargetTypedSumInitializerSyntax = st.isTargetTypedSumInitializerSyntax;
  return PhaseStatus::Continue;
}

} // namespace primec::semantics
