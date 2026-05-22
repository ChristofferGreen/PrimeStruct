#include "SemanticsValidator.h"

#include <algorithm>
#include <functional>

namespace primec::semantics {

bool SemanticsValidator::resolveStructFieldBinding(const Definition &structDef,
                                                   const Expr &fieldStmt,
                                                   BindingInfo &bindingOut) {
  auto failStructFieldDiagnostic = [&](std::string message) -> bool {
    return failExprDiagnostic(fieldStmt, std::move(message));
  };
  if (isCompileTimeTypeBinding(fieldStmt)) {
    return failStructFieldDiagnostic("type binding is not a struct field: " +
                                     structDef.fullPath + "/" +
                                     fieldStmt.name);
  }
  auto bindingTypeText = [](const BindingInfo &binding) {
    if (binding.typeTemplateArg.empty()) {
      return binding.typeName;
    }
    return binding.typeName + "<" + binding.typeTemplateArg + ">";
  };
  auto isStaticFieldLocal = [](const Expr &stmt) {
    for (const auto &transform : stmt.transforms) {
      if (transform.name == "static") {
        return true;
      }
    }
    return false;
  };
  std::unordered_map<std::string, BindingInfo> priorFieldLocals;
  std::unordered_map<std::string, std::string> priorTypeLocals;
  auto resolveNamedConcreteType = [&](const Expr &namedType,
                                      std::string &resolvedTypeOut) -> bool {
    if (namedType.kind != Expr::Kind::Name || namedType.name == "auto") {
      return false;
    }
    if (auto typeLocalIt = priorTypeLocals.find(namedType.name);
        typeLocalIt != priorTypeLocals.end()) {
      resolvedTypeOut = typeLocalIt->second;
      return true;
    }
    if (isPrimitiveBindingTypeName(namedType.name)) {
      resolvedTypeOut = normalizeBindingTypeName(namedType.name);
      return true;
    }
    resolvedTypeOut = resolveStructTypePath(namedType.name,
                                            structDef.namespacePrefix,
                                            structNames_);
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
  auto resolveTypeofSymbol = [&](const Expr &typeofExpr,
                                 std::string &resolvedTypeOut) -> bool {
    if (typeofExpr.templateArgs.size() != 1 ||
        typeofExpr.templateArgDetails.size() != 1 ||
        typeofExpr.templateArgDetails.front().kind !=
            TemplateArgumentKind::Symbol) {
      return false;
    }
    const std::string &symbol = typeofExpr.templateArgs.front();
    auto fieldIt = priorFieldLocals.find(symbol);
    if (fieldIt != priorFieldLocals.end()) {
      resolvedTypeOut = bindingTypeText(fieldIt->second);
      return true;
    }
    auto typeLocalIt = priorTypeLocals.find(symbol);
    if (typeLocalIt != priorTypeLocals.end()) {
      resolvedTypeOut = typeLocalIt->second;
      return true;
    }
    return false;
  };
  auto resolveTypeBindingInitializer = [&](const Expr &typeStmt,
                                           std::string &resolvedTypeOut) -> bool {
    if (typeStmt.args.size() != 1) {
      return false;
    }
    const Expr *typeExpr = &typeStmt.args.front();
    const Expr &initializer = typeStmt.args.front();
    if (initializer.kind == Expr::Kind::Call &&
        initializer.name == "block" && initializer.hasBodyArguments &&
        initializer.args.empty() && initializer.templateArgs.empty() &&
        !hasNamedArguments(initializer.argNames)) {
      if (initializer.bodyArguments.size() != 1) {
        return false;
      }
      typeExpr = &initializer.bodyArguments.front();
    }
    if (typeExpr->kind == Expr::Kind::Call && typeExpr->name == "typeof") {
      return resolveTypeofSymbol(*typeExpr, resolvedTypeOut);
    }
    return resolveNamedConcreteType(*typeExpr, resolvedTypeOut);
  };
  for (const Expr &stmt : structDef.statements) {
    if (&stmt == &fieldStmt) {
      break;
    }
    if (!stmt.isBinding || isStaticFieldLocal(stmt)) {
      continue;
    }
    if (isCompileTimeTypeBinding(stmt)) {
      std::string resolvedType;
      if (resolveTypeBindingInitializer(stmt, resolvedType)) {
        priorTypeLocals[stmt.name] = std::move(resolvedType);
      }
      continue;
    }
    BindingInfo previousBinding;
    if (resolveStructFieldBinding(structDef, stmt, previousBinding)) {
      priorFieldLocals[stmt.name] = std::move(previousBinding);
    }
  }
  std::optional<std::string> restrictType;
  std::string parseError;
  if (!parseBindingInfo(fieldStmt, structDef.namespacePrefix, structNames_, importAliases_, bindingOut,
                        restrictType, parseError, &sumNames_, &priorTypeLocals)) {
    return failStructFieldDiagnostic(std::move(parseError));
  }
  if (hasExplicitBindingTypeTransform(fieldStmt)) {
    if (!validateBuiltinComparableKeyType(bindingOut, &structDef.templateArgs, error_)) {
      return failExprDiagnostic(fieldStmt, error_);
    }
    return true;
  }
  BindingInfo graphBinding = bindingOut;
  if (lookupGraphLocalAutoBinding(structDef.fullPath, fieldStmt, graphBinding) &&
      graphBindingIsUsable(graphBinding)) {
    bindingOut = std::move(graphBinding);
    if (!validateBuiltinComparableKeyType(bindingOut, &structDef.templateArgs, error_)) {
      return failExprDiagnostic(fieldStmt, error_);
    }
    return true;
  }
  const std::string fieldPath = structDef.fullPath + "/" + fieldStmt.name;
  if (fieldStmt.args.size() != 1) {
    return failStructFieldDiagnostic(
        "omitted struct field envelope requires exactly one initializer: " +
        fieldPath);
  }
  const std::vector<ParameterInfo> noParams;
  const std::unordered_map<std::string, BindingInfo> noLocals;
  BindingInfo inferred = bindingOut;
  std::function<bool(const Expr &, BindingInfo &)> inferIdentityWrappedInitializer;
  inferIdentityWrappedInitializer = [&](const Expr &candidate,
                                        BindingInfo &wrappedBinding) -> bool {
    if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall ||
        candidate.isBinding || candidate.args.size() != 1 ||
        hasNamedArguments(candidate.argNames) || candidate.hasBodyArguments ||
        !candidate.bodyArguments.empty()) {
      return false;
    }
    const std::string resolvedPath = resolveCalleePath(candidate);
    auto defIt = defMap_.find(resolvedPath);
    if (defIt == defMap_.end() || defIt->second == nullptr ||
        defIt->second->parameters.size() != 1) {
      return false;
    }
    const Definition &definition = *defIt->second;
    const auto returnTransformIt =
        std::find_if(definition.transforms.begin(),
                     definition.transforms.end(),
                     [](const Transform &transform) {
                       return transform.name == "return" &&
                              transform.templateArgs.size() == 1;
                     });
    if (returnTransformIt == definition.transforms.end()) {
      return false;
    }
    BindingInfo paramBinding;
    std::optional<std::string> paramRestrictType;
    std::string paramParseError;
    if (!parseBindingInfo(definition.parameters.front(),
                          definition.namespacePrefix,
                          structNames_,
                          importAliases_,
                          paramBinding,
                          paramRestrictType,
                          paramParseError,
                          &sumNames_)) {
      return false;
    }
    const std::string returnType =
        normalizeBindingTypeName(returnTransformIt->templateArgs.front());
    if (returnType.empty() ||
        returnType != normalizeBindingTypeName(paramBinding.typeName)) {
      return false;
    }
    if (inferBindingTypeFromInitializer(candidate.args.front(), noParams, noLocals,
                                        wrappedBinding)) {
      return true;
    }
    if (tryInferBindingTypeFromInitializer(candidate.args.front(), noParams,
                                           noLocals, wrappedBinding,
                                           hasAnyMathImport())) {
      return true;
    }
    return inferIdentityWrappedInitializer(candidate.args.front(), wrappedBinding);
  };
  if (inferBindingTypeFromInitializer(fieldStmt.args.front(), noParams, noLocals, inferred)) {
    if (!(inferred.typeName == "array" && inferred.typeTemplateArg.empty())) {
      bindingOut = std::move(inferred);
      if (!validateBuiltinComparableKeyType(bindingOut, &structDef.templateArgs, error_)) {
        return failExprDiagnostic(fieldStmt, error_);
      }
      return true;
    }
  }
  if (tryInferBindingTypeFromInitializer(
          fieldStmt.args.front(), noParams, noLocals, inferred, hasAnyMathImport())) {
    if (!(inferred.typeName == "array" && inferred.typeTemplateArg.empty())) {
      bindingOut = std::move(inferred);
      if (!validateBuiltinComparableKeyType(bindingOut, &structDef.templateArgs, error_)) {
        return failExprDiagnostic(fieldStmt, error_);
      }
      return true;
    }
  }
  if (inferIdentityWrappedInitializer(fieldStmt.args.front(), inferred)) {
    if (!(inferred.typeName == "array" && inferred.typeTemplateArg.empty())) {
      bindingOut = std::move(inferred);
      if (!validateBuiltinComparableKeyType(bindingOut, &structDef.templateArgs, error_)) {
        return failExprDiagnostic(fieldStmt, error_);
      }
      return true;
    }
  }
  if (inferExprReturnKind(fieldStmt.args.front(), noParams, noLocals) == ReturnKind::Array) {
    std::string structPath = inferStructReturnPath(fieldStmt.args.front(), noParams, noLocals);
    if (!structPath.empty()) {
      bindingOut.typeName = std::move(structPath);
      bindingOut.typeTemplateArg.clear();
      if (!validateBuiltinComparableKeyType(bindingOut, &structDef.templateArgs, error_)) {
        return failExprDiagnostic(fieldStmt, error_);
      }
      return true;
    }
  }
  return failStructFieldDiagnostic(
      "unresolved or ambiguous omitted struct field envelope: " + fieldPath);
}

bool SemanticsValidator::validateSoaVectorElementFieldEnvelopes(const std::string &typeArg,
                                                                const std::string &namespacePrefix) {
  auto resolveStructPath = [&](const std::string &candidate, const std::string &scope) -> std::string {
    const std::string normalized = normalizeBindingTypeName(candidate);
    std::string resolved = resolveStructTypePath(normalized, scope, structNames_);
    if (!resolved.empty()) {
      return resolved;
    }
    auto importIt = importAliases_.find(normalized);
    if (importIt != importAliases_.end() && structNames_.count(importIt->second) > 0) {
      return importIt->second;
    }
    return {};
  };
  std::string elementStructPath = resolveStructPath(typeArg, namespacePrefix);
  if (elementStructPath.empty()) {
    return true;
  }
  auto structIt = defMap_.find(elementStructPath);
  if (structIt == defMap_.end()) {
    return true;
  }
  auto hasStaticTransform = [](const Expr &stmt) -> bool {
    for (const auto &transform : stmt.transforms) {
      if (transform.name == "static") {
        return true;
      }
    }
    return false;
  };
  std::unordered_set<std::string> activeStructs;
  std::function<bool(const Definition &, const std::string &)> validateStructFields;
  validateStructFields = [&](const Definition &structDef, const std::string &pathPrefix) -> bool {
    if (!activeStructs.insert(structDef.fullPath).second) {
      return true;
    }
    for (const auto &fieldStmt : structDef.statements) {
      if (!fieldStmt.isBinding || hasStaticTransform(fieldStmt) ||
          isCompileTimeTypeBinding(fieldStmt)) {
        continue;
      }
      auto failSoaFieldEnvelopeDiagnostic = [&](std::string message) -> bool {
        return failExprDiagnostic(fieldStmt, std::move(message));
      };
      BindingInfo fieldBinding;
      if (!resolveStructFieldBinding(structDef, fieldStmt, fieldBinding)) {
        activeStructs.erase(structDef.fullPath);
        return false;
      }
      const std::string fieldPath = pathPrefix + "/" + fieldStmt.name;
      const std::string normalizedFieldType = normalizeBindingTypeName(fieldBinding.typeName);
      if (normalizedFieldType == "string" || fieldBinding.typeName == "Pointer" ||
          fieldBinding.typeName == "Reference" || !fieldBinding.typeTemplateArg.empty()) {
        std::string fieldType = fieldBinding.typeName;
        if (!fieldBinding.typeTemplateArg.empty()) {
          fieldType += "<" + fieldBinding.typeTemplateArg + ">";
        }
        activeStructs.erase(structDef.fullPath);
        return failSoaFieldEnvelopeDiagnostic(
            "soa" "_vector field envelope is unsupported on " + fieldPath +
            ": " + fieldType);
      }
      if (fieldBinding.typeTemplateArg.empty() && !isPrimitiveBindingTypeName(normalizedFieldType)) {
        const std::string nestedStructPath = resolveStructPath(fieldBinding.typeName, structDef.namespacePrefix);
        if (!nestedStructPath.empty()) {
          auto nestedIt = defMap_.find(nestedStructPath);
          if (nestedIt != defMap_.end()) {
            if (!validateStructFields(*nestedIt->second, fieldPath)) {
              activeStructs.erase(structDef.fullPath);
              return false;
            }
          }
        }
      }
    }
    activeStructs.erase(structDef.fullPath);
    return true;
  };
  return validateStructFields(*structIt->second, structIt->second->fullPath);
}

}  // namespace primec::semantics
