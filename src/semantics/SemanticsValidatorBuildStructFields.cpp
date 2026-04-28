#include "SemanticsValidator.h"

namespace primec::semantics {

bool SemanticsValidator::resolveStructFieldBinding(const Definition &structDef,
                                                   const Expr &fieldStmt,
                                                   BindingInfo &bindingOut) {
  auto failStructFieldDiagnostic = [&](std::string message) -> bool {
    return failExprDiagnostic(fieldStmt, std::move(message));
  };
  std::optional<std::string> restrictType;
  std::string parseError;
  if (!parseBindingInfo(fieldStmt, structDef.namespacePrefix, structNames_, importAliases_, bindingOut,
                        restrictType, parseError, &sumNames_)) {
    return failStructFieldDiagnostic(std::move(parseError));
  }
  if (hasExplicitBindingTypeTransform(fieldStmt)) {
    if (!validateBuiltinMapKeyType(bindingOut, &structDef.templateArgs, error_)) {
      return failExprDiagnostic(fieldStmt, error_);
    }
    return true;
  }
  BindingInfo graphBinding = bindingOut;
  if (lookupGraphLocalAutoBinding(structDef.fullPath, fieldStmt, graphBinding) &&
      graphBindingIsUsable(graphBinding)) {
    bindingOut = std::move(graphBinding);
    if (!validateBuiltinMapKeyType(bindingOut, &structDef.templateArgs, error_)) {
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
  if (inferBindingTypeFromInitializer(fieldStmt.args.front(), noParams, noLocals, inferred)) {
    if (!(inferred.typeName == "array" && inferred.typeTemplateArg.empty())) {
      bindingOut = std::move(inferred);
      if (!validateBuiltinMapKeyType(bindingOut, &structDef.templateArgs, error_)) {
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
      if (!validateBuiltinMapKeyType(bindingOut, &structDef.templateArgs, error_)) {
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
      if (!fieldStmt.isBinding || hasStaticTransform(fieldStmt)) {
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
            "soa_vector field envelope is unsupported on " + fieldPath +
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
