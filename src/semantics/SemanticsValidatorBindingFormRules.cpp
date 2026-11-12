#include "SemanticsValidator.h"

#include "SemanticsValidatorInferCollectionCompatibilityInternal.h"

#include <algorithm>
#include <functional>
#include <optional>
#include <unordered_set>
#include "primec/ir/StdlibCollectionPaths.h"
#include "primec/support/CollectionHelperNames.h"
#include "SemanticsValidatorStatementBindingsHelpers.h"
#include "SemanticsValidatorBindingContext.h"

namespace primec::semantics {
using namespace statementBindingsHelpers;

// The first rules of a binding statement (TODO-5982): is it a legal binding here, and what are its type and
// initializer? Each returns Done when it has decided the statement.

PhaseStatus SemanticsValidator::checkBindingForm(BindingContext &ctx) {
  bool &handled = ctx.handled;
  const std::vector<ParameterInfo> &params = ctx.params;
  std::unordered_map<std::string, BindingInfo> &locals = ctx.locals;
  const Expr &stmt = ctx.stmt;
  const std::string &namespacePrefix = ctx.namespacePrefix;
  const bool allowBindings = ctx.allowBindings;
  const bool allowCompileTimeTypeBindings = ctx.allowCompileTimeTypeBindings;
  auto &definitionTemplateArgs = ctx.definitionTemplateArgs;
  auto &bindingLookupNamespace = ctx.bindingLookupNamespace;
  auto &info = ctx.info;
  handled = false;
  if (!stmt.isBinding) {
    return ctx.done(true);
  }
  handled = true;
  ctx.definitionTemplateArgs = nullptr;
  auto currentDefIt = defMap_.find(currentValidationState_.context.definitionPath);
  if (currentDefIt != defMap_.end() && currentDefIt->second != nullptr) {
    definitionTemplateArgs = &currentDefIt->second->templateArgs;
  }
  ctx.bindingLookupNamespace = !currentValidationState_.context.definitionPath.empty()
                                   ? currentValidationState_.context.definitionPath
                                   : namespacePrefix;
  if (!allowBindings) {
    return ctx.done(failBindingDiagnostic(ctx, "binding not allowed in execution body"));
  }
  if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
    return ctx.done(failBindingDiagnostic(ctx, "binding does not accept block arguments"));
  }
  if (isCompileTimeTypeBinding(stmt)) {
    if (!allowCompileTimeTypeBindings) {
      return ctx.done(
          failBindingDiagnostic(ctx, "type bindings are only supported in definition bodies"));
    }
    if (isParam(params, stmt.name) || locals.count(stmt.name) > 0 ||
        currentValidationState_.compileTimeTypeLocals.count(stmt.name) > 0) {
      return ctx.done(failBindingDiagnostic(ctx, "duplicate binding name: " + stmt.name));
    }
    bool sawTypeTransform = false;
    for (const auto &transform : stmt.transforms) {
      if (transform.name == "type") {
        if (sawTypeTransform) {
          return ctx.done(failBindingDiagnostic(ctx, "duplicate type transform on binding"));
        }
        sawTypeTransform = true;
        if (!transform.templateArgs.empty() || !transform.arguments.empty()) {
          return ctx.done(
              failBindingDiagnostic(ctx, "type binding transform does not take arguments"));
        }
        continue;
      }
      if (isBindingAuxTransformName(transform.name)) {
        return ctx.done(failBindingDiagnostic(
            ctx, "type binding does not accept binding qualifier: " + transform.name));
      }
      return ctx.done(failBindingDiagnostic(ctx, "type binding requires only the type transform"));
    }
    if (!sawTypeTransform) {
      return ctx.done(failBindingDiagnostic(ctx, "type binding requires type transform"));
    }
    if (stmt.args.size() != 1) {
      return ctx.done(failBindingDiagnostic(ctx, "type binding requires exactly one initializer"));
    }
    const Expr *typeExpr = &stmt.args.front();
    const Expr &initializer = stmt.args.front();
    if (initializer.kind == Expr::Kind::Call && initializer.name == "block" &&
        initializer.hasBodyArguments && initializer.args.empty() &&
        initializer.templateArgs.empty() && !hasNamedArguments(initializer.argNames)) {
      if (initializer.bodyArguments.size() != 1) {
        return ctx.done(failBindingDiagnostic(
            ctx, "type binding initializer must be a single type expression"));
      }
      typeExpr = &initializer.bodyArguments.front();
    } else if (initializer.hasBodyArguments || !initializer.bodyArguments.empty() ||
               hasNamedArguments(stmt.argNames)) {
      return ctx.done(
          failBindingDiagnostic(ctx, "type binding initializer must be a single type expression"));
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
      resolvedTypeOut = resolveStructTypePath(namedType.name, bindingLookupNamespace, structNames_);
      if (resolvedTypeOut.empty()) {
        auto importIt = importAliases_.find(namedType.name);
        if (importIt != importAliases_.end() &&
            (structNames_.count(importIt->second) > 0 || sumNames_.count(importIt->second) > 0)) {
          resolvedTypeOut = importIt->second;
        }
      }
      if (resolvedTypeOut.empty() && sumNames_.count(namedType.name) > 0) {
        resolvedTypeOut = namedType.name;
      }
      if (resolvedTypeOut.empty() && !namedType.name.empty() && namedType.name.front() == '/' &&
          (structNames_.count(namedType.name) > 0 || sumNames_.count(namedType.name) > 0)) {
        resolvedTypeOut = namedType.name;
      }
      return !resolvedTypeOut.empty();
    };
    auto appendZeroArgCallablePath = [&](const std::string &path, std::vector<std::string> &paths) {
      if (path.empty() || std::find(paths.begin(), paths.end(), path) != paths.end()) {
        return;
      }
      auto defIt = defMap_.find(path);
      if (defIt == defMap_.end() || defIt->second == nullptr ||
          !defIt->second->parameters.empty() || !defIt->second->templateArgs.empty() ||
          structNames_.count(defIt->second->fullPath) > 0 || isSumDefinition(*defIt->second)) {
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
      if (auto importIt = importAliases_.find(symbol); importIt != importAliases_.end()) {
        appendZeroArgCallablePath(importIt->second, paths);
      }
      std::sort(paths.begin(), paths.end());
      paths.erase(std::unique(paths.begin(), paths.end()), paths.end());
      return paths;
    };
    auto resolveTypeofSymbol = [&](const Expr &typeofExpr, std::string &resolvedTypeOut) -> bool {
      if (typeofExpr.isMethodCall || typeofExpr.isFieldAccess || typeofExpr.isBinding ||
          !typeofExpr.args.empty() || typeofExpr.hasBodyArguments ||
          !typeofExpr.bodyArguments.empty() || hasNamedArguments(typeofExpr.argNames)) {
        return failBindingDiagnostic(ctx,
                                     "typeof requires compile-time symbol syntax: typeof<symbol>");
      }
      if (typeofExpr.templateArgs.size() != 1 || typeofExpr.templateArgDetails.size() != 1 ||
          typeofExpr.templateArgDetails.front().text != typeofExpr.templateArgs.front()) {
        return failBindingDiagnostic(ctx,
                                     "typeof requires exactly one compile-time symbol argument");
      }
      if (typeofExpr.templateArgDetails.front().kind != TemplateArgumentKind::Symbol) {
        return failBindingDiagnostic(ctx, "typeof requires a symbol argument");
      }
      const std::string &symbol = typeofExpr.templateArgs.front();
      const BindingInfo *paramBinding = findParamBinding(params, symbol);
      const auto localIt = locals.find(symbol);
      const auto typeLocalIt = currentValidationState_.compileTimeTypeLocals.find(symbol);
      const std::vector<std::string> zeroArgPaths = visibleZeroArgCallablePaths(symbol);
      const bool hasValueOrTypeFact =
          paramBinding != nullptr || localIt != locals.end() ||
          typeLocalIt != currentValidationState_.compileTimeTypeLocals.end();
      if (hasValueOrTypeFact && !zeroArgPaths.empty()) {
        return failBindingDiagnostic(ctx,
                                     "ambiguous typeof symbol: " + symbol +
                                         " could refer to a local value or " +
                                         formatPathListForTypeof(zeroArgPaths));
      }
      if (zeroArgPaths.size() > 1) {
        return failBindingDiagnostic(ctx,
                                     "ambiguous typeof symbol: " + symbol + " could refer to " +
                                         formatPathListForTypeof(zeroArgPaths));
      }
      if (paramBinding != nullptr) {
        resolvedTypeOut = bindingTypeTextForTypeof(*paramBinding);
      } else if (localIt != locals.end()) {
        resolvedTypeOut = bindingTypeTextForTypeof(localIt->second);
      } else if (typeLocalIt != currentValidationState_.compileTimeTypeLocals.end()) {
        resolvedTypeOut = typeLocalIt->second;
      } else if (!zeroArgPaths.empty()) {
        return failBindingDiagnostic(ctx, "typeof symbol is not a value: " + symbol);
      } else {
        return failBindingDiagnostic(ctx, "unknown typeof symbol: " + symbol);
      }
      if (resolvedTypeOut.empty() || normalizeBindingTypeName(resolvedTypeOut) == "auto") {
        return failBindingDiagnostic(ctx, "typeof requires a concrete value type: " + symbol);
      }
      return true;
    };
    std::string resolvedType;
    if (typeExpr->kind == Expr::Kind::Call && typeExpr->name == "typeof") {
      if (!resolveTypeofSymbol(*typeExpr, resolvedType)) {
        return ctx.done(false);
      }
    } else if (!resolveNamedConcreteType(*typeExpr, resolvedType)) {
      return ctx.done(
          failBindingDiagnostic(ctx, "type binding initializer requires a concrete type"));
    }
    currentValidationState_.compileTimeTypeLocals.emplace(stmt.name, std::move(resolvedType));
    return ctx.done(true);
  }
  if (stmt.transforms.empty() && !stmt.args.empty()) {
    const std::string lookupNamespace =
        !stmt.namespacePrefix.empty() ? stmt.namespacePrefix : namespacePrefix;
    const std::string structPath = resolveStructTypePath(stmt.name, lookupNamespace, structNames_);
    if (!structPath.empty()) {
      Expr constructorExpr = stmt;
      constructorExpr.isBinding = false;
      constructorExpr.isBraceConstructor = true;
      constructorExpr.name = structPath;
      constructorExpr.namespacePrefix.clear();
      if (!validateExpr(params, locals, constructorExpr)) {
        return ctx.done(false);
      }
      return ctx.done(true);
    }
  }
  if (isParam(params, stmt.name) || locals.count(stmt.name) > 0 ||
      currentValidationState_.compileTimeTypeLocals.count(stmt.name) > 0) {
    return ctx.done(failBindingDiagnostic(ctx, "duplicate binding name: " + stmt.name));
  }
  for (const auto &transform : stmt.transforms) {
    if (!isInternalSoaCollectionTypeName(transform.name) || transform.templateArgs.size() != 1) {
      continue;
    }
    if (!isSoaVectorStructElementType(
            transform.templateArgs.front(), namespacePrefix, structNames_, importAliases_)) {
      break;
    }
    if (!validateSoaVectorElementFieldEnvelopes(transform.templateArgs.front(), namespacePrefix)) {
      return ctx.done(false);
    }
    break;
  }
  if (currentDefIt != defMap_.end() && currentDefIt->second != nullptr &&
      structNames_.count(currentValidationState_.context.definitionPath) > 0) {
    if (!resolveStructFieldBinding(*currentDefIt->second, stmt, info)) {
      return ctx.done(false);
    }
    if (stmt.args.size() == 1 && stmt.args.front().kind == Expr::Kind::Call &&
        isIfCall(stmt.args.front()) && !validateIfExpr(params, locals, stmt.args.front())) {
      return ctx.done(false);
    }
    if (!validateBuiltinComparableKeyType(info, definitionTemplateArgs, error_)) {
      return ctx.done(false);
    }
    insertLocalBinding(locals, stmt.name, std::move(info));
    return ctx.done(true);
  }
  return PhaseStatus::Continue;
}

PhaseStatus SemanticsValidator::parseBindingTypeAndInitializer(BindingContext &ctx) {
  const std::vector<ParameterInfo> &params = ctx.params;
  std::unordered_map<std::string, BindingInfo> &locals = ctx.locals;
  const Expr &stmt = ctx.stmt;
  const std::string &namespacePrefix = ctx.namespacePrefix;
  auto &definitionTemplateArgs = ctx.definitionTemplateArgs;
  auto &bindingLookupNamespace = ctx.bindingLookupNamespace;
  auto &info = ctx.info;
  auto &restrictType = ctx.restrictType;
  auto &hasExplicitType = ctx.hasExplicitType;
  auto &explicitAutoType = ctx.explicitAutoType;
  auto &entryArgInit = ctx.entryArgInit;
  auto &entryArgStringInit = ctx.entryArgStringInit;
  auto &entryArgScope = ctx.entryArgScope;
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
    return ctx.done(false);
  }
  if (info.isMove) {
    return ctx.done(
        failBindingDiagnostic(ctx, "move transform is only supported on parameters: " + stmt.name));
  }
  std::string parsedSoaElementType;
  if (extractExperimentalSoaVectorElementType(info, parsedSoaElementType) &&
      isSoaVectorStructElementType(
          parsedSoaElementType, namespacePrefix, structNames_, importAliases_) &&
      !validateSoaVectorElementFieldEnvelopes(parsedSoaElementType, namespacePrefix)) {
    return ctx.done(false);
  }
  ctx.hasExplicitType = hasExplicitBindingTypeTransform(stmt);
  ctx.explicitAutoType = hasExplicitType && normalizeBindingTypeName(info.typeName) == "auto";
  if (stmt.args.size() == 1 && stmt.args.front().isLambda &&
      (!hasExplicitType || explicitAutoType)) {
    info.typeName = "lambda";
    info.typeTemplateArg.clear();
  }
  if (stmt.args.empty()) {
    if (structNames_.count(currentValidationState_.context.definitionPath) > 0) {
      if (restrictType.has_value()) {
        const bool hasTemplate = !info.typeTemplateArg.empty();
        if (!restrictMatchesBinding(
                *restrictType, info.typeName, info.typeTemplateArg, hasTemplate, namespacePrefix)) {
          return ctx.done(failBindingDiagnostic(ctx, "restrict type does not match binding type"));
        }
      }
      if (!validateBuiltinComparableKeyType(info, definitionTemplateArgs, error_)) {
        return ctx.done(false);
      }
      insertLocalBinding(locals, stmt.name, std::move(info));
      return ctx.done(true);
    }
    if (!validateOmittedBindingInitializer(stmt, info, namespacePrefix)) {
      return ctx.done(false);
    }
    if (restrictType.has_value()) {
      const bool hasTemplate = !info.typeTemplateArg.empty();
      if (!restrictMatchesBinding(
              *restrictType, info.typeName, info.typeTemplateArg, hasTemplate, namespacePrefix)) {
        return ctx.done(failBindingDiagnostic(ctx, "restrict type does not match binding type"));
      }
    }
    if (!validateBuiltinComparableKeyType(info, definitionTemplateArgs, error_)) {
      return ctx.done(false);
    }
    insertLocalBinding(locals, stmt.name, std::move(info));
    return ctx.done(true);
  }
  if (stmt.args.size() != 1) {
    return ctx.done(failBindingDiagnostic(ctx, "binding requires exactly one argument"));
  }
  ctx.initializer = &(stmt.args.front());
  const Expr &initializer = *ctx.initializer;
  if (initializer.kind == Expr::Kind::Call && !initializer.isMethodCall) {
    const bool explicitOldGetRef =
        initializer.name == samePathSoaHelperTargetPath(collection_helpers::kGetRef) ||
        initializer.name == internalSoaCollectionTypeName() + "/get_ref" ||
        (isCompatibilitySoaSurfaceNamespace(initializer.namespacePrefix) &&
         initializer.name == collection_helpers::kGetRef);
    if (explicitOldGetRef && !hasVisibleDefinitionPathForCurrentImports(
                                 samePathSoaHelperTargetPath(collection_helpers::kGetRef))) {
      return ctx.done(failBindingDiagnostic(ctx, "get_ref is only supported as a statement"));
    }
  }
  const std::string normalizedBindingType = normalizeBindingTypeName(info.typeName);
  if (explicitAutoType && initializer.kind == Expr::Kind::Call && initializer.isBraceConstructor &&
      hasNamedArguments(initializer.argNames) &&
      normalizeBindingTypeName(initializer.name) == "auto") {
    return ctx.done(failBindingDiagnostic(ctx, "sum construction requires target sum type"));
  }
  if ((normalizedBindingType == "vector" ||
       isInternalSoaCollectionTypeName(normalizedBindingType)) &&
      isEmptyBuiltinBlockInitializer(initializer)) {
    if (!validateOmittedBindingInitializer(stmt, info, namespacePrefix)) {
      return ctx.done(false);
    }
    if (restrictType.has_value()) {
      const bool hasTemplate = !info.typeTemplateArg.empty();
      if (!restrictMatchesBinding(
              *restrictType, info.typeName, info.typeTemplateArg, hasTemplate, namespacePrefix)) {
        return ctx.done(failBindingDiagnostic(ctx, "restrict type does not match binding type"));
      }
    }
    if (!validateBuiltinComparableKeyType(info, definitionTemplateArgs, error_)) {
      return ctx.done(false);
    }
    insertLocalBinding(locals, stmt.name, std::move(info));
    return ctx.done(true);
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
    std::string resolvedCallPath = preferredCollectionHelperResolvedPath(initializer);
    if (resolvedCallPath.empty()) {
      resolvedCallPath = resolveCalleePath(initializer);
    }
    resolvedCallPath = canonicalizeLegacySoaToAosHelperPath(
        resolveExprConcreteCallPath(params, locals, initializer, resolvedCallPath));
    const bool isRootToAosHelper =
        isSoaConversionSurfaceSpelling(normalizedCallPrefix, normalizedCallName) ||
        isLegacyOrCanonicalSoaHelperPath(resolvedCallPath, "to_aos") ||
        isLegacyOrCanonicalSoaHelperPath(resolvedCallPath, collection_helpers::kToAosRef);
    if (!isRootToAosHelper) {
      return false;
    }
    const bool isBorrowedToAosHelper =
        isLegacyOrCanonicalSoaHelperPath(resolvedCallPath, collection_helpers::kToAosRef);
    const std::string helperPath = isBorrowedToAosHelper ? "/to_aos_ref" : "/to_aos";
    const std::string canonicalHelperPath =
        isBorrowedToAosHelper ? compatibilitySoaHelperTargetPath(collection_helpers::kToAosRef)
                              : compatibilitySoaHelperTargetPath("to_aos");
    const std::string publicHelperPath = publicSoaHelperTargetPath("to_aos");
    auto hasExplicitSourceImportPath = [&](const std::string &path) {
      for (const auto &importPath : program_.sourceImports) {
        if (importPath == path) {
          return true;
        }
        if (importPath.size() >= 2 && importPath.compare(importPath.size() - 2, 2, "/*") == 0) {
          const std::string prefix = importPath.substr(0, importPath.size() - 2);
          if (path == prefix || path.rfind(prefix + "/", 0) == 0) {
            return true;
          }
        }
      }
      return false;
    };
    if (hasDeclaredDefinitionPath(helperPath) || hasExplicitSourceImportPath(helperPath) ||
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
           isInternalSoaCollectionTypeName(normalizeBindingTypeName(receiverBinding->typeName)) &&
           !receiverBinding->typeTemplateArg.empty();
  };
  if (normalizedBindingType == "vector" && isUnsupportedRootSoaToAosBindingInitializer()) {
    return ctx.done(failBindingDiagnostic(ctx, "binding initializer type mismatch"));
  }
  if (failIfMoveOnlyCopy(stmt, initializer, params, locals)) {
    return ctx.done(false);
  }
  ctx.entryArgInit = isEntryArgsAccess(initializer);
  ctx.entryArgStringInit = isEntryArgStringBinding(locals, initializer);
  if (entryArgInit || entryArgStringInit) {
    entryArgScope.emplace(*this, true);
  }
  return PhaseStatus::Continue;
}

} // namespace primec::semantics
