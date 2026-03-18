#include "SemanticsValidator.h"
#include "SemanticsValidatorStatementLoopCountStep.h"

#include <cctype>
#include <functional>
#include <optional>

namespace primec::semantics {

namespace {

std::string bindingTypeText(const BindingInfo &binding) {
  if (binding.typeTemplateArg.empty()) {
    return binding.typeName;
  }
  return binding.typeName + "<" + binding.typeTemplateArg + ">";
}

bool templateArgsContainTypeName(const std::vector<std::string> *templateArgs, const std::string &typeName) {
  if (templateArgs == nullptr) {
    return false;
  }
  const std::string normalized = normalizeBindingTypeName(typeName);
  for (const auto &candidate : *templateArgs) {
    if (normalizeBindingTypeName(candidate) == normalized) {
      return true;
    }
  }
  return false;
}

bool isMatrixQuaternionTypePathForDiagnostic(const std::string &typePath) {
  return typePath == "/std/math/Mat2" || typePath == "/std/math/Mat3" || typePath == "/std/math/Mat4" ||
         typePath == "/std/math/Quat";
}

bool isVectorTypePathForDiagnostic(const std::string &typePath) {
  return typePath == "/std/math/Vec2" || typePath == "/std/math/Vec3" || typePath == "/std/math/Vec4";
}

bool isMatrixQuaternionConversionTypePath(const std::string &typePath) {
  return isMatrixQuaternionTypePathForDiagnostic(typePath) || isVectorTypePathForDiagnostic(typePath);
}

bool isImplicitMatrixQuaternionConversion(const std::string &expectedTypePath, const std::string &actualTypePath) {
  return !expectedTypePath.empty() && !actualTypePath.empty() && expectedTypePath != actualTypePath &&
         isMatrixQuaternionConversionTypePath(expectedTypePath) &&
         isMatrixQuaternionConversionTypePath(actualTypePath) &&
         (isMatrixQuaternionTypePathForDiagnostic(expectedTypePath) ||
          isMatrixQuaternionTypePathForDiagnostic(actualTypePath));
}

std::string implicitMatrixQuaternionConversionDiagnostic(const std::string &expectedTypePath,
                                                        const std::string &actualTypePath) {
  return "implicit matrix/quaternion family conversion requires explicit helper: expected " + expectedTypePath +
         " got " + actualTypePath;
}

std::string initValueTypeMismatchDiagnostic(const std::string &expectedTypePath, const std::string &actualTypePath) {
  if (isImplicitMatrixQuaternionConversion(expectedTypePath, actualTypePath)) {
    return "init value type mismatch: " +
           implicitMatrixQuaternionConversionDiagnostic(expectedTypePath, actualTypePath);
  }
  return "init value type mismatch";
}

std::string returnTypeMismatchDiagnostic(const std::string &expectedTypePath,
                                         const std::string &actualTypePath,
                                         const std::string &fallbackExpectedType) {
  if (isImplicitMatrixQuaternionConversion(expectedTypePath, actualTypePath)) {
    return "return type mismatch: " +
           implicitMatrixQuaternionConversionDiagnostic(expectedTypePath, actualTypePath);
  }
  return "return type mismatch: expected " + fallbackExpectedType;
}

} // namespace

bool SemanticsValidator::isDropTrivialContainerElementType(const std::string &typeName,
                                                           const std::string &namespacePrefix,
                                                           const std::vector<std::string> *definitionTemplateArgs,
                                                           std::unordered_set<std::string> &visitingStructs) {
  if (templateArgsContainTypeName(definitionTemplateArgs, typeName)) {
    return true;
  }

  const std::string normalizedType = normalizeBindingTypeName(typeName);
  if (normalizedType == "bool" || normalizedType == "i32" || normalizedType == "i64" || normalizedType == "u64" ||
      normalizedType == "f32" || normalizedType == "f64" || normalizedType == "string") {
    return true;
  }

  std::string base;
  std::string argText;
  if (splitTemplateTypeName(normalizedType, base, argText)) {
    const std::string normalizedBase = normalizeBindingTypeName(base);
    if (templateArgsContainTypeName(definitionTemplateArgs, normalizedBase)) {
      return true;
    }
    if (normalizedBase == "Pointer" || normalizedBase == "Reference") {
      return true;
    }
    if (normalizedBase == "array") {
      std::vector<std::string> args;
      return splitTopLevelTemplateArgs(argText, args) && args.size() == 1 &&
             isDropTrivialContainerElementType(args.front(), namespacePrefix, definitionTemplateArgs, visitingStructs);
    }
    if (normalizedBase == "vector" || normalizedBase == "map" || normalizedBase == "soa_vector" ||
        normalizedBase == "uninitialized" || normalizedBase == "Buffer") {
      return false;
    }
    base = normalizedBase;
  } else {
    base = normalizedType;
  }

  const std::string structPath = resolveStructTypePath(base, namespacePrefix, structNames_);
  if (structPath.empty() || structNames_.count(structPath) == 0) {
    return true;
  }
  if (!visitingStructs.insert(structPath).second) {
    return true;
  }

  struct VisitingScope {
    std::unordered_set<std::string> &set;
    std::string value;
    ~VisitingScope() { set.erase(value); }
  } visitingScope{visitingStructs, structPath};

  if (defMap_.count(structPath + "/Destroy") > 0 || defMap_.count(structPath + "/DestroyStack") > 0 ||
      defMap_.count(structPath + "/DestroyHeap") > 0 || defMap_.count(structPath + "/DestroyBuffer") > 0) {
    return false;
  }

  const Definition *structDef = nullptr;
  auto defIt = defMap_.find(structPath);
  if (defIt != defMap_.end()) {
    structDef = defIt->second;
  }
  if (structDef == nullptr) {
    return true;
  }

  for (const auto &fieldStmt : structDef->statements) {
    if (!fieldStmt.isBinding) {
      continue;
    }
    BindingInfo fieldBinding;
    if (!resolveStructFieldBinding(*structDef, fieldStmt, fieldBinding)) {
      continue;
    }
    if (!isDropTrivialContainerElementType(bindingTypeText(fieldBinding),
                                           structDef->namespacePrefix,
                                           &structDef->templateArgs,
                                           visitingStructs)) {
      return false;
    }
  }

  return true;
}

bool SemanticsValidator::isRelocationTrivialContainerElementType(const std::string &typeName,
                                                                 const std::string &namespacePrefix,
                                                                 const std::vector<std::string> *definitionTemplateArgs,
                                                                 std::unordered_set<std::string> &visitingStructs) {
  if (templateArgsContainTypeName(definitionTemplateArgs, typeName)) {
    return true;
  }

  const std::string normalizedType = normalizeBindingTypeName(typeName);
  if (normalizedType == "bool" || normalizedType == "i32" || normalizedType == "i64" || normalizedType == "u64" ||
      normalizedType == "f32" || normalizedType == "f64" || normalizedType == "string") {
    return true;
  }

  std::string base;
  std::string argText;
  if (splitTemplateTypeName(normalizedType, base, argText)) {
    const std::string normalizedBase = normalizeBindingTypeName(base);
    if (templateArgsContainTypeName(definitionTemplateArgs, normalizedBase)) {
      return true;
    }
    if (normalizedBase == "Pointer" || normalizedBase == "Reference") {
      return true;
    }
    if (normalizedBase == "array") {
      std::vector<std::string> args;
      return splitTopLevelTemplateArgs(argText, args) && args.size() == 1 &&
             isRelocationTrivialContainerElementType(args.front(),
                                                     namespacePrefix,
                                                     definitionTemplateArgs,
                                                     visitingStructs);
    }
    if (normalizedBase == "vector" || normalizedBase == "map" || normalizedBase == "soa_vector" ||
        normalizedBase == "uninitialized" || normalizedBase == "Buffer") {
      return false;
    }
    base = normalizedBase;
  } else {
    base = normalizedType;
  }

  const std::string structPath = resolveStructTypePath(base, namespacePrefix, structNames_);
  if (structPath.empty() || structNames_.count(structPath) == 0) {
    return true;
  }
  if (!visitingStructs.insert(structPath).second) {
    return true;
  }

  struct VisitingScope {
    std::unordered_set<std::string> &set;
    std::string value;
    ~VisitingScope() { set.erase(value); }
  } visitingScope{visitingStructs, structPath};

  if (defMap_.count(structPath + "/Destroy") > 0 || defMap_.count(structPath + "/DestroyStack") > 0 ||
      defMap_.count(structPath + "/DestroyHeap") > 0 || defMap_.count(structPath + "/DestroyBuffer") > 0 ||
      defMap_.count(structPath + "/Copy") > 0 || defMap_.count(structPath + "/Move") > 0) {
    return false;
  }

  const Definition *structDef = nullptr;
  auto defIt = defMap_.find(structPath);
  if (defIt != defMap_.end()) {
    structDef = defIt->second;
  }
  if (structDef == nullptr) {
    return true;
  }

  for (const auto &fieldStmt : structDef->statements) {
    if (!fieldStmt.isBinding) {
      continue;
    }
    BindingInfo fieldBinding;
    if (!resolveStructFieldBinding(*structDef, fieldStmt, fieldBinding)) {
      continue;
    }
    if (!isRelocationTrivialContainerElementType(bindingTypeText(fieldBinding),
                                                 structDef->namespacePrefix,
                                                 &structDef->templateArgs,
                                                 visitingStructs)) {
      return false;
    }
  }

  return true;
}

bool SemanticsValidator::validateVectorDiscardHelperElementType(const BindingInfo &binding,
                                                                const std::string &helperName,
                                                                const std::string &namespacePrefix,
                                                                const std::vector<std::string> *definitionTemplateArgs) {
  if (binding.typeTemplateArg.empty()) {
    return true;
  }

  std::unordered_set<std::string> visitingStructs;
  if (isDropTrivialContainerElementType(binding.typeTemplateArg,
                                        namespacePrefix,
                                        definitionTemplateArgs,
                                        visitingStructs)) {
    return true;
  }

  error_ = helperName + " requires drop-trivial vector element type until container drop semantics are implemented: " +
           binding.typeTemplateArg;
  return false;
}

bool SemanticsValidator::validateVectorIndexedRemovalHelperElementType(
    const BindingInfo &binding,
    const std::string &helperName,
    const std::string &namespacePrefix,
    const std::vector<std::string> *definitionTemplateArgs) {
  if (!validateVectorDiscardHelperElementType(binding, helperName, namespacePrefix, definitionTemplateArgs)) {
    return false;
  }
  return validateVectorRelocationHelperElementType(binding, helperName, namespacePrefix, definitionTemplateArgs);
}

bool SemanticsValidator::validateVectorRelocationHelperElementType(
    const BindingInfo &binding,
    const std::string &helperName,
    const std::string &namespacePrefix,
    const std::vector<std::string> *definitionTemplateArgs) {
  if (binding.typeTemplateArg.empty()) {
    return true;
  }

  std::unordered_set<std::string> visitingStructs;
  if (isRelocationTrivialContainerElementType(binding.typeTemplateArg,
                                              namespacePrefix,
                                              definitionTemplateArgs,
                                              visitingStructs)) {
    return true;
  }

  error_ = helperName +
           " requires relocation-trivial vector element type until container move/reallocation semantics are "
           "implemented: " +
           binding.typeTemplateArg;
  return false;
}

bool SemanticsValidator::validateStatement(const std::vector<ParameterInfo> &params,
                                           std::unordered_map<std::string, BindingInfo> &locals,
                                           const Expr &stmt,
                                           ReturnKind returnKind,
                                           bool allowReturn,
                                           bool allowBindings,
                                           bool *sawReturn,
                                           const std::string &namespacePrefix,
                                           const std::vector<Expr> *enclosingStatements,
                                           size_t statementIndex) {
  ExprContextScope statementScope(*this, stmt);
  const std::vector<std::string> *definitionTemplateArgs = nullptr;
  auto currentDefIt = defMap_.find(currentValidationContext_.definitionPath);
  if (currentDefIt != defMap_.end() && currentDefIt->second != nullptr) {
    definitionTemplateArgs = &currentDefIt->second->templateArgs;
  }
  auto returnKindForBinding = [&](const BindingInfo &binding) -> ReturnKind {
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
  };
  auto isStringExpr = [&](const Expr &arg,
                          const std::vector<ParameterInfo> &paramsIn,
                          const std::unordered_map<std::string, BindingInfo> &localsIn) -> bool {
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
        if (const BindingInfo *paramBinding = findParamBinding(paramsIn, target.name)) {
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
        auto it = localsIn.find(target.name);
        if (it == localsIn.end()) {
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
        if (const BindingInfo *paramBinding = findParamBinding(paramsIn, target.name)) {
          std::string ignoredKeyType;
          return extractMapKeyValueTypes(*paramBinding, ignoredKeyType, valueTypeOut);
        }
        auto it = localsIn.find(target.name);
        if (it == localsIn.end()) {
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
        if (const BindingInfo *paramBinding = findParamBinding(paramsIn, candidate.name)) {
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
                (void)inferBindingTypeFromInitializer(bodyExpr.args.front(), paramsIn, branchLocals, info, &bodyExpr);
              }
              branchLocals.emplace(bodyExpr.name, std::move(info));
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
      if (candidate.kind == Expr::Kind::Call) {
        std::string accessName;
        if (defMap_.find(resolveCalleePath(candidate)) == defMap_.end() &&
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
        if (inferExprReturnKind(candidate, paramsIn, candidateLocals) == ReturnKind::String) {
          return true;
        }
      }
      return false;
    };
    if (arg.kind == Expr::Kind::StringLiteral) {
      return true;
    }
    if (arg.kind == Expr::Kind::Name) {
      if (const BindingInfo *paramBinding = findParamBinding(paramsIn, arg.name)) {
        return paramBinding->typeName == "string";
      }
      auto it = localsIn.find(arg.name);
      return it != localsIn.end() && it->second.typeName == "string";
    }
    return isStringExprRef(arg, localsIn);
  };
  auto isPrintableExpr = [&](const Expr &arg,
                             const std::vector<ParameterInfo> &paramsIn,
                             const std::unordered_map<std::string, BindingInfo> &localsIn) -> bool {
    if (isStringExpr(arg, paramsIn, localsIn)) {
      return true;
    }
    ReturnKind kind = inferExprReturnKind(arg, paramsIn, localsIn);
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
      if (const BindingInfo *paramBinding = findParamBinding(paramsIn, arg.name)) {
        ReturnKind paramKind = returnKindForBinding(*paramBinding);
        return paramKind == ReturnKind::Int || paramKind == ReturnKind::Int64 || paramKind == ReturnKind::UInt64 ||
               paramKind == ReturnKind::Bool;
      }
      auto it = localsIn.find(arg.name);
      if (it != localsIn.end()) {
        ReturnKind localKind = returnKindForBinding(it->second);
        return localKind == ReturnKind::Int || localKind == ReturnKind::Int64 || localKind == ReturnKind::UInt64 ||
               localKind == ReturnKind::Bool;
      }
    }
    if (isPointerLikeExpr(arg, paramsIn, localsIn)) {
      return false;
    }
    if (arg.kind == Expr::Kind::Call) {
      std::string builtinName;
      if (defMap_.find(resolveCalleePath(arg)) == defMap_.end() && getBuiltinCollectionName(arg, builtinName)) {
        return false;
      }
    }
    return false;
  };
  bool handledBindingStatement = false;
  if (!validateBindingStatement(params, locals, stmt, allowBindings, namespacePrefix, handledBindingStatement)) {
    return false;
  }
  if (handledBindingStatement) {
    return true;
  }
  std::optional<EffectScope> effectScope;
  if (stmt.kind == Expr::Kind::Call && !stmt.isBinding && !stmt.transforms.empty() && !isBuiltinBlockCall(stmt)) {
    std::unordered_set<std::string> executionEffects;
    if (!resolveExecutionEffects(stmt, executionEffects)) {
      return false;
    }
    effectScope.emplace(*this, std::move(executionEffects));
  }
  if (stmt.kind == Expr::Kind::Call && !stmt.isBinding && !stmt.isMethodCall &&
      (isSimpleCallName(stmt, "init") || isSimpleCallName(stmt, "drop"))) {
    const std::string name = stmt.name;
    if (hasNamedArguments(stmt.argNames)) {
      error_ = "named arguments not supported for builtin calls";
      return false;
    }
    if (!stmt.templateArgs.empty()) {
      error_ = name + " does not accept template arguments";
      return false;
    }
    if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
      error_ = name + " does not accept block arguments";
      return false;
    }
    const size_t expectedArgs = (name == "init") ? 2 : 1;
    if (stmt.args.size() != expectedArgs) {
      error_ = name + " requires exactly " + std::to_string(expectedArgs) + " argument" +
               (expectedArgs == 1 ? "" : "s");
      return false;
    }
    const Expr &storageArg = stmt.args.front();
    if (!validateExpr(params, locals, storageArg)) {
      return false;
    }
    BindingInfo storageBinding;
    bool resolved = false;
    if (!resolveUninitializedStorageBinding(params, locals, storageArg, storageBinding, resolved)) {
      return false;
    }
    if (!resolved || storageBinding.typeName != "uninitialized" || storageBinding.typeTemplateArg.empty()) {
      error_ = name + " requires uninitialized<T> storage";
      return false;
    }
    if (name == "init") {
      if (!validateExpr(params, locals, stmt.args[1])) {
        return false;
      }
      ReturnKind valueKind = inferExprReturnKind(stmt.args[1], params, locals);
      if (valueKind == ReturnKind::Void) {
        error_ = "init requires a value";
        return false;
      }
      auto trimType = [](const std::string &text) -> std::string {
        size_t start = 0;
        while (start < text.size() && std::isspace(static_cast<unsigned char>(text[start])) != 0) {
          ++start;
        }
        size_t end = text.size();
        while (end > start && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
          --end;
        }
        return text.substr(start, end - start);
      };
      auto isBuiltinTemplateBase = [](const std::string &base) -> bool {
        return base == "array" || base == "vector" || base == "map" || base == "Result" || base == "Pointer" ||
               base == "Reference" || base == "Buffer" || base == "uninitialized";
      };
      std::function<bool(const std::string &, const std::string &)> typesMatch;
      typesMatch = [&](const std::string &expected, const std::string &actual) -> bool {
        std::string expectedTrim = trimType(expected);
        std::string actualTrim = trimType(actual);
        std::string expectedBase;
        std::string expectedArg;
        std::string actualBase;
        std::string actualArg;
        const bool expectedHasTemplate = splitTemplateTypeName(expectedTrim, expectedBase, expectedArg);
        const bool actualHasTemplate = splitTemplateTypeName(actualTrim, actualBase, actualArg);
        if (expectedHasTemplate != actualHasTemplate) {
          return false;
        }
        auto normalizeBase = [&](std::string base) -> std::string {
          base = trimType(base);
          if (!base.empty() && base[0] == '/') {
            base.erase(0, 1);
          }
          return base;
        };
        if (expectedHasTemplate) {
          std::string expectedNorm = normalizeBase(expectedBase);
          std::string actualNorm = normalizeBase(actualBase);
          if (isBuiltinTemplateBase(expectedNorm) || isBuiltinTemplateBase(actualNorm)) {
            if (expectedNorm != actualNorm) {
              return false;
            }
          } else if (!errorTypesMatch(expectedBase, actualBase, namespacePrefix)) {
            return false;
          }
          std::vector<std::string> expectedArgs;
          std::vector<std::string> actualArgs;
          if (!splitTopLevelTemplateArgs(expectedArg, expectedArgs) ||
              !splitTopLevelTemplateArgs(actualArg, actualArgs)) {
            return false;
          }
          if (expectedArgs.size() != actualArgs.size()) {
            return false;
          }
          for (size_t i = 0; i < expectedArgs.size(); ++i) {
            if (!typesMatch(expectedArgs[i], actualArgs[i])) {
              return false;
            }
          }
          return true;
        }
        return errorTypesMatch(expectedTrim, actualTrim, namespacePrefix);
      };
      auto inferValueTypeString = [&](const Expr &value, std::string &typeOut) -> bool {
        BindingInfo inferred;
        if (inferBindingTypeFromInitializer(value, params, locals, inferred)) {
          if (!inferred.typeTemplateArg.empty()) {
            typeOut = inferred.typeName + "<" + inferred.typeTemplateArg + ">";
            return true;
          }
          if (!inferred.typeName.empty() && inferred.typeName != "array") {
            typeOut = inferred.typeName;
            return true;
          }
        }
        if (inferExprReturnKind(value, params, locals) == ReturnKind::Array) {
          std::string structPath = inferStructReturnPath(value, params, locals);
          if (!structPath.empty()) {
            typeOut = structPath;
            return true;
          }
        }
        return false;
      };
      const std::string expectedType = storageBinding.typeTemplateArg;
      std::string actualType;
      if (inferValueTypeString(stmt.args[1], actualType)) {
        if (!typesMatch(expectedType, actualType)) {
          const std::string expectedStruct = resolveStructTypePath(expectedType, namespacePrefix, structNames_);
          const std::string actualStruct = resolveStructTypePath(actualType, namespacePrefix, structNames_);
          error_ = initValueTypeMismatchDiagnostic(expectedStruct, actualStruct);
          return false;
        }
        return true;
      }
      ReturnKind expectedKind = returnKindForTypeName(expectedType);
      if (expectedKind == ReturnKind::Array) {
        if (valueKind != ReturnKind::Unknown && valueKind != ReturnKind::Array) {
          error_ = "init value type mismatch";
          return false;
        }
      } else if (expectedKind != ReturnKind::Unknown) {
        if (valueKind != ReturnKind::Unknown && valueKind != expectedKind) {
          error_ = "init value type mismatch";
          return false;
        }
      } else {
        std::string expectedStruct = resolveStructTypePath(expectedType, namespacePrefix, structNames_);
        if (!expectedStruct.empty()) {
          if (valueKind != ReturnKind::Unknown && valueKind != ReturnKind::Array) {
            error_ = "init value type mismatch";
            return false;
          }
          if (valueKind == ReturnKind::Array) {
            std::string actualStruct = inferStructReturnPath(stmt.args[1], params, locals);
            if (!actualStruct.empty() && actualStruct != expectedStruct) {
              error_ = initValueTypeMismatchDiagnostic(expectedStruct, actualStruct);
              return false;
            }
          }
        }
      }
    }
    return true;
  }
  if (stmt.kind == Expr::Kind::Call && !stmt.isBinding && !stmt.isMethodCall &&
      (isSimpleCallName(stmt, "take") || isSimpleCallName(stmt, "borrow"))) {
    const std::string name = stmt.name;
    auto isUninitializedStorage = [&](const Expr &arg) -> bool {
      if (arg.kind != Expr::Kind::Name) {
        return false;
      }
      const BindingInfo *binding = findBinding(params, locals, arg.name);
      return binding && binding->typeName == "uninitialized" && !binding->typeTemplateArg.empty();
    };
    const bool treatAsUninitializedHelper =
        (name != "take") || (!stmt.args.empty() && isUninitializedStorage(stmt.args.front()));
    if (treatAsUninitializedHelper) {
      if (hasNamedArguments(stmt.argNames)) {
        error_ = "named arguments not supported for builtin calls";
        return false;
      }
      if (!stmt.templateArgs.empty()) {
        error_ = name + " does not accept template arguments";
        return false;
      }
      if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
        error_ = name + " does not accept block arguments";
        return false;
      }
      if (stmt.args.size() != 1) {
        error_ = name + " requires exactly 1 argument";
        return false;
      }
      if (!validateExpr(params, locals, stmt.args.front())) {
        return false;
      }
      if (!isUninitializedStorage(stmt.args.front())) {
        error_ = name + " requires uninitialized<T> storage";
        return false;
      }
      return true;
    }
  }
  if (stmt.kind != Expr::Kind::Call) {
    if (!allowBindings) {
      error_ = "execution body arguments must be calls";
      return false;
    }
    return validateExpr(params, locals, stmt);
  }
  if (isReturnCall(stmt)) {
    if (hasNamedArguments(stmt.argNames)) {
      error_ = "named arguments not supported for builtin calls";
      return false;
    }
    if (!allowReturn) {
      error_ = "return not allowed in execution body";
      return false;
    }
    if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
      error_ = "return does not accept block arguments";
      return false;
    }
    if (returnKind == ReturnKind::Void) {
      if (!stmt.args.empty()) {
        error_ = "return value not allowed for void definition";
        return false;
      }
    } else {
      if (stmt.args.size() != 1) {
        error_ = "return requires exactly one argument";
        return false;
      }
      if (!validateExpr(params, locals, stmt.args.front())) {
        return false;
      }
      auto declaredReferenceReturnTarget = [&]() -> std::optional<std::string> {
        auto defIt = defMap_.find(currentValidationContext_.definitionPath);
        if (defIt == defMap_.end() || defIt->second == nullptr) {
          return std::nullopt;
        }
        for (const auto &transform : defIt->second->transforms) {
          if (transform.name != "return" || transform.templateArgs.size() != 1) {
            continue;
          }
          std::string base;
          std::string arg;
          if (!splitTemplateTypeName(transform.templateArgs.front(), base, arg) || base != "Reference") {
            continue;
          }
          std::vector<std::string> args;
          if (!splitTopLevelTemplateArgs(arg, args) || args.size() != 1) {
            return std::nullopt;
          }
          return args.front();
        }
        return std::nullopt;
      };
      if (std::optional<std::string> expectedReferenceTarget = declaredReferenceReturnTarget();
          expectedReferenceTarget.has_value()) {
        const Expr &returnExpr = stmt.args.front();
        if (returnExpr.kind != Expr::Kind::Name) {
          error_ = "reference return requires direct parameter reference";
          return false;
        }
        const BindingInfo *paramBinding = findParamBinding(params, returnExpr.name);
        if (paramBinding == nullptr || paramBinding->typeName != "Reference") {
          error_ = "reference return requires direct parameter reference";
          return false;
        }
        auto normalizeReferenceTarget = [&](const std::string &target) -> std::string {
          std::string trimmed = target;
          size_t start = 0;
          while (start < trimmed.size() && std::isspace(static_cast<unsigned char>(trimmed[start])) != 0) {
            ++start;
          }
          size_t end = trimmed.size();
          while (end > start && std::isspace(static_cast<unsigned char>(trimmed[end - 1])) != 0) {
            --end;
          }
          trimmed = trimmed.substr(start, end - start);
          return normalizeBindingTypeName(trimmed);
        };
        if (normalizeReferenceTarget(paramBinding->typeTemplateArg) !=
            normalizeReferenceTarget(*expectedReferenceTarget)) {
          error_ = "reference return type mismatch";
          return false;
        }
      }
      if (returnKind != ReturnKind::Unknown) {
        ReturnKind exprKind = inferExprReturnKind(stmt.args.front(), params, locals);
        if (returnKind == ReturnKind::Array) {
          auto structIt = returnStructs_.find(currentValidationContext_.definitionPath);
          if (structIt != returnStructs_.end()) {
            auto normalizeCollectionStructPath = [&](const std::string &typePath) -> std::string {
              std::string normalizedTypePath = normalizeBindingTypeName(typePath);
              if (!normalizedTypePath.empty() && normalizedTypePath.front() == '/') {
                normalizedTypePath.erase(normalizedTypePath.begin());
              }
              if (normalizedTypePath.rfind("std/collections/experimental_map/Map__", 0) == 0) {
                return "/map";
              }
              if (typePath == "/array" || typePath == "array") {
                return "/array";
              }
              if (typePath == "/vector" || typePath == "vector" || typePath == "/std/collections/vector" ||
                  typePath == "std/collections/vector") {
                return "/vector";
              }
              if (typePath == "/soa_vector" || typePath == "soa_vector") {
                return "/soa_vector";
              }
              if (isMapCollectionTypeName(typePath) || typePath == "/map" || typePath == "/std/collections/map") {
                return "/map";
              }
              if (typePath == "/string" || typePath == "string") {
                return "/string";
              }
              return "";
            };
            std::string actualStruct = inferStructReturnPath(stmt.args.front(), params, locals);
            const std::string normalizedExpectedStruct = normalizeCollectionStructPath(structIt->second);
            const std::string normalizedActualStruct = normalizeCollectionStructPath(actualStruct);
            if (actualStruct.empty() ||
                (actualStruct != structIt->second &&
                 (normalizedExpectedStruct.empty() || normalizedExpectedStruct != normalizedActualStruct))) {
              std::string expectedType = structIt->second;
              if (expectedType == "/array" || expectedType == "/vector" || expectedType == "/map" ||
                  expectedType == "/string") {
                expectedType.erase(0, 1);
              }
              error_ = returnTypeMismatchDiagnostic(structIt->second, actualStruct, expectedType);
              return false;
            }
          } else if (exprKind != ReturnKind::Array) {
            error_ = "return type mismatch: expected array";
            return false;
          }
        } else if (exprKind != ReturnKind::Unknown && exprKind != returnKind) {
          error_ = "return type mismatch: expected " + typeNameForReturnKind(returnKind);
          return false;
        }
      }
    }
    if (sawReturn) {
      *sawReturn = true;
    }
    return true;
  }
  bool handledControlFlowStatement = false;
  if (!validateControlFlowStatement(params,
                                    locals,
                                    stmt,
                                    returnKind,
                                    allowReturn,
                                    allowBindings,
                                    sawReturn,
                                    namespacePrefix,
                                    enclosingStatements,
                                    statementIndex,
                                    handledControlFlowStatement)) {
    return false;
  }
  if (handledControlFlowStatement) {
    return true;
  }
  if (isBuiltinBlockCall(stmt) && !stmt.hasBodyArguments) {
    error_ = "block requires block arguments";
    return false;
  }
  if (isBuiltinBlockCall(stmt) && stmt.hasBodyArguments) {
    if (hasNamedArguments(stmt.argNames) || !stmt.args.empty() || !stmt.templateArgs.empty()) {
      error_ = "block does not accept arguments";
      return false;
    }
    std::optional<OnErrorHandler> blockOnError;
    if (!parseOnErrorTransform(stmt.transforms, stmt.namespacePrefix, "block", blockOnError)) {
      return false;
    }
    if (blockOnError.has_value()) {
      OnErrorScope onErrorArgsScope(*this, std::nullopt);
      for (const auto &arg : blockOnError->boundArgs) {
        if (!validateExpr(params, locals, arg)) {
          return false;
        }
      }
    }
    std::unordered_map<std::string, BindingInfo> blockLocals = locals;
    OnErrorScope onErrorScope(*this, blockOnError);
    BorrowEndScope borrowScope(*this, currentValidationContext_.endedReferenceBorrows);
    for (size_t bodyIndex = 0; bodyIndex < stmt.bodyArguments.size(); ++bodyIndex) {
      const Expr &bodyExpr = stmt.bodyArguments[bodyIndex];
      if (!validateStatement(params,
                             blockLocals,
                             bodyExpr,
                             returnKind,
                             allowReturn,
                             allowBindings,
                             sawReturn,
                             namespacePrefix,
                             &stmt.bodyArguments,
                             bodyIndex)) {
        return false;
      }
      expireReferenceBorrowsForRemainder(params, blockLocals, stmt.bodyArguments, bodyIndex + 1);
    }
    return true;
  }
  PrintBuiltin printBuiltin;
  if (getPrintBuiltin(stmt, printBuiltin)) {
    if (hasNamedArguments(stmt.argNames)) {
      error_ = "named arguments not supported for builtin calls";
      return false;
    }
    if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
      error_ = printBuiltin.name + " does not accept block arguments";
      return false;
    }
    if (stmt.args.size() != 1) {
      error_ = printBuiltin.name + " requires exactly one argument";
      return false;
    }
    const std::string effectName = (printBuiltin.target == PrintTarget::Err) ? "io_err" : "io_out";
    if (currentValidationContext_.activeEffects.count(effectName) == 0) {
      error_ = printBuiltin.name + " requires " + effectName + " effect";
      return false;
    }
    {
      EntryArgStringScope entryArgScope(*this, true);
      if (!validateExpr(params, locals, stmt.args.front())) {
        return false;
      }
    }
    if (!isPrintableExpr(stmt.args.front(), params, locals)) {
      error_ = printBuiltin.name + " requires an integer/bool or string literal/binding argument";
      return false;
    }
    return true;
  }
  bool handledPathSpaceComputeBuiltin = false;
  if (!validatePathSpaceComputeBuiltinStatement(params, locals, stmt, handledPathSpaceComputeBuiltin)) {
    return false;
  }
  if (handledPathSpaceComputeBuiltin) {
    return true;
  }
  bool handledVectorStatementHelper = false;
  if (!validateVectorStatementHelper(params,
                                     locals,
                                     stmt,
                                     namespacePrefix,
                                     definitionTemplateArgs,
                                     enclosingStatements,
                                     statementIndex,
                                     handledVectorStatementHelper)) {
    return false;
  }
  if (handledVectorStatementHelper) {
    return true;
  }
  auto isRemovedVectorCompatibilityHelper = [](std::string_view helperName) {
    return helperName == "count" || helperName == "capacity" || helperName == "at" || helperName == "at_unsafe" ||
           helperName == "push" || helperName == "pop" || helperName == "reserve" || helperName == "clear" ||
           helperName == "remove_at" || helperName == "remove_swap";
  };
  auto isRemovedMapCompatibilityHelper = [](std::string_view helperName) {
    return helperName == "count" || helperName == "at" || helperName == "at_unsafe";
  };
  auto shouldPreserveBodyArgumentTarget = [&](const std::string &path) -> bool {
    auto helperSuffix = [](const std::string &candidate, const char *prefix) -> std::string_view {
      const size_t prefixLen = std::char_traits<char>::length(prefix);
      if (candidate.rfind(prefix, 0) != 0 || candidate.size() <= prefixLen) {
        return std::string_view();
      }
      return std::string_view(candidate).substr(prefixLen);
    };
    std::string_view helper = helperSuffix(path, "/vector/");
    if (helper.empty()) {
      helper = helperSuffix(path, "/array/");
    }
    if (helper.empty()) {
      helper = helperSuffix(path, "/std/collections/vector/");
    }
    if (!helper.empty()) {
      return isRemovedVectorCompatibilityHelper(helper);
    }
    helper = helperSuffix(path, "/map/");
    if (helper.empty()) {
      helper = helperSuffix(path, "/std/collections/map/");
    }
    return !helper.empty() && isRemovedMapCompatibilityHelper(helper);
  };
  auto normalizeBodyArgumentTarget = [&](const std::string &path) {
    if (shouldPreserveBodyArgumentTarget(path)) {
      return path;
    }
    return preferVectorStdlibHelperPath(path);
  };
  auto preferredMapBodyArgumentTarget = [&](const std::string &helperName) {
    const std::string canonical = "/std/collections/map/" + helperName;
    const std::string alias = "/map/" + helperName;
    if (helperName == "at" || helperName == "at_unsafe") {
      return canonical;
    }
    if (defMap_.count(canonical) > 0) {
      return canonical;
    }
    if (defMap_.count(alias) > 0) {
      return alias;
    }
    return canonical;
  };
  auto isMapReceiverExpr = [&](const Expr &target) {
    if (target.kind == Expr::Kind::Name) {
      if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
        std::string keyType;
        std::string valueType;
        return extractMapKeyValueTypes(*paramBinding, keyType, valueType);
      }
      auto it = locals.find(target.name);
      if (it == locals.end()) {
        return false;
      }
      std::string keyType;
      std::string valueType;
      return extractMapKeyValueTypes(it->second, keyType, valueType);
    }
    if (target.kind != Expr::Kind::Call) {
      return false;
    }
    auto defIt = defMap_.find(resolveCalleePath(target));
    if (defIt != defMap_.end() && defIt->second) {
      for (const auto &transform : defIt->second->transforms) {
        if (transform.name != "return" || transform.templateArgs.size() != 1) {
          continue;
        }
        return returnsMapCollectionType(transform.templateArgs.front());
      }
      return false;
    }
    std::string collection;
    return getBuiltinCollectionName(target, collection) && collection == "map" && target.templateArgs.size() == 2;
  };
  auto resolveBodyArgumentTarget = [&](const Expr &callExpr, std::string &resolvedOut) {
    auto resolveBareMapCallBodyArgumentTarget = [&](const Expr &candidate) -> bool {
      if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall || candidate.name.empty()) {
        return false;
      }
      std::string normalized = candidate.name;
      if (!normalized.empty() && normalized.front() == '/') {
        normalized.erase(normalized.begin());
      }
      std::string helperName;
      if (normalized == "count") {
        helperName = "count";
      } else if (normalized == "at") {
        helperName = "at";
      } else if (normalized == "at_unsafe") {
        helperName = "at_unsafe";
      } else {
        const std::string resolvedPath = resolveCalleePath(candidate);
        if (resolvedPath == "/count") {
          helperName = "count";
        } else if (resolvedPath == "/at") {
          helperName = "at";
        } else if (resolvedPath == "/at_unsafe") {
          helperName = "at_unsafe";
        } else {
          return false;
        }
      }
      if (defMap_.count("/" + helperName) > 0 || candidate.args.empty()) {
        return false;
      }
      std::vector<size_t> receiverIndices;
      auto appendReceiverIndex = [&](size_t index) {
        if (index >= candidate.args.size()) {
          return;
        }
        for (size_t existing : receiverIndices) {
          if (existing == index) {
            return;
          }
        }
        receiverIndices.push_back(index);
      };
      if (hasNamedArguments(candidate.argNames)) {
        bool foundValues = false;
        for (size_t i = 0; i < candidate.args.size(); ++i) {
          if (i < candidate.argNames.size() && candidate.argNames[i].has_value() &&
              *candidate.argNames[i] == "values") {
            appendReceiverIndex(i);
            foundValues = true;
            break;
          }
        }
        if (!foundValues) {
          for (size_t i = 0; i < candidate.args.size(); ++i) {
            appendReceiverIndex(i);
          }
        }
      } else {
        for (size_t i = 0; i < candidate.args.size(); ++i) {
          appendReceiverIndex(i);
        }
      }
      for (size_t receiverIndex : receiverIndices) {
        if (!isMapReceiverExpr(candidate.args[receiverIndex])) {
          continue;
        }
        resolvedOut = preferredMapBodyArgumentTarget(helperName);
        return true;
      }
      return false;
    };
    if (!callExpr.isMethodCall) {
      if (resolveBareMapCallBodyArgumentTarget(callExpr)) {
        return;
      }
      resolvedOut = normalizeBodyArgumentTarget(resolveCalleePath(callExpr));
      return;
    }
    if (callExpr.args.empty()) {
      resolvedOut = normalizeBodyArgumentTarget(resolveCalleePath(callExpr));
      return;
    }
    const Expr &receiver = callExpr.args.front();
    std::string methodName = callExpr.name;
    const bool isExplicitMethodPath = !methodName.empty() && methodName.front() == '/';
    if (!methodName.empty() && methodName.front() == '/') {
      methodName.erase(methodName.begin());
    }
    std::string namespacedCollection;
    std::string namespacedHelper;
    if (getNamespacedCollectionHelperName(callExpr, namespacedCollection, namespacedHelper) &&
        !namespacedHelper.empty()) {
      methodName = namespacedHelper;
    }
    if (receiver.kind == Expr::Kind::Call && !receiver.isBinding && !receiver.isMethodCall) {
      const std::string resolvedType = resolveCalleePath(receiver);
      if (!resolvedType.empty() && structNames_.count(resolvedType) > 0) {
        resolvedOut = normalizeBodyArgumentTarget(resolvedType + "/" + methodName);
        return;
      }
      if (!isExplicitMethodPath &&
          (methodName == "count" || methodName == "at" || methodName == "at_unsafe") &&
          isMapReceiverExpr(receiver)) {
        resolvedOut = "/std/collections/map/" + methodName;
        return;
      }
    }
    auto inferPointerLikeCallReturnType = [&](const Expr &receiverExpr) -> std::string {
      if (receiverExpr.kind != Expr::Kind::Call || receiverExpr.isBinding || receiverExpr.isMethodCall) {
        return "";
      }
      auto callPathCandidates = [&](const std::string &path) {
        auto allowsArrayVectorCompatibilitySuffix = [](const std::string &suffix) {
          return suffix != "count" && suffix != "capacity" && suffix != "at" && suffix != "at_unsafe" &&
                 suffix != "push" && suffix != "pop" && suffix != "reserve" && suffix != "clear" &&
                 suffix != "remove_at" && suffix != "remove_swap";
        };
        auto allowsVectorStdlibCompatibilitySuffix = [](const std::string &suffix) {
          return suffix != "count" && suffix != "capacity" && suffix != "at" && suffix != "at_unsafe" &&
                 suffix != "push" && suffix != "pop" && suffix != "reserve" && suffix != "clear" &&
                 suffix != "remove_at" && suffix != "remove_swap";
        };
        std::vector<std::string> candidates;
        auto appendUnique = [&](const std::string &candidate) {
          if (candidate.empty()) {
            return;
          }
          for (const auto &existing : candidates) {
            if (existing == candidate) {
              return;
            }
          }
          candidates.push_back(candidate);
        };
        appendUnique(path);
        if (path.rfind("/array/", 0) == 0) {
          const std::string suffix = path.substr(std::string("/array/").size());
          if (allowsArrayVectorCompatibilitySuffix(suffix)) {
            appendUnique("/vector/" + suffix);
            appendUnique("/std/collections/vector/" + suffix);
          }
        } else if (path.rfind("/vector/", 0) == 0) {
          const std::string suffix = path.substr(std::string("/vector/").size());
          if (allowsVectorStdlibCompatibilitySuffix(suffix)) {
            appendUnique("/std/collections/vector/" + suffix);
          }
          if (allowsArrayVectorCompatibilitySuffix(suffix)) {
            appendUnique("/array/" + suffix);
          }
        } else if (path.rfind("/std/collections/vector/", 0) == 0) {
          const std::string suffix = path.substr(std::string("/std/collections/vector/").size());
          if (allowsVectorStdlibCompatibilitySuffix(suffix)) {
            appendUnique("/vector/" + suffix);
          }
          if (allowsArrayVectorCompatibilitySuffix(suffix)) {
            appendUnique("/array/" + suffix);
          }
        } else if (path.rfind("/map/", 0) == 0) {
          const std::string suffix = path.substr(std::string("/map/").size());
          if (suffix != "count" && suffix != "contains" && suffix != "tryAt") {
            appendUnique("/std/collections/map/" + suffix);
          }
        } else if (path.rfind("/std/collections/map/", 0) == 0) {
          const std::string suffix = path.substr(std::string("/std/collections/map/").size());
          if (suffix != "count" && suffix != "contains" && suffix != "tryAt" &&
              suffix != "at" && suffix != "at_unsafe") {
            appendUnique("/map/" + suffix);
          }
        }
        return candidates;
      };
      for (const auto &callPath : callPathCandidates(resolveCalleePath(receiverExpr))) {
        auto defIt = defMap_.find(callPath);
        if (defIt == defMap_.end() || defIt->second == nullptr) {
          continue;
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
          if (base == "Pointer") {
            return "Pointer";
          }
          if (base == "Reference") {
            return "Reference";
          }
        }
      }
      return "";
    };
    std::string typeName;
    if (receiver.kind == Expr::Kind::Name) {
      if (const BindingInfo *paramBinding = findParamBinding(params, receiver.name)) {
        typeName = paramBinding->typeName;
      } else {
        auto it = locals.find(receiver.name);
        if (it != locals.end()) {
          typeName = it->second.typeName;
        }
      }
    }
    if (typeName.empty()) {
      typeName = inferPointerLikeCallReturnType(receiver);
    }
    if (typeName.empty()) {
      ReturnKind inferredKind = inferExprReturnKind(receiver, params, locals);
      std::string inferred;
      if (inferredKind == ReturnKind::Array) {
        inferred = inferStructReturnPath(receiver, params, locals);
        if (inferred.empty()) {
          inferred = typeNameForReturnKind(inferredKind);
        }
      } else {
        inferred = typeNameForReturnKind(inferredKind);
      }
      if (!inferred.empty()) {
        typeName = inferred;
      }
    }
    auto shouldPreferCanonicalMapBodyArgumentTarget = [&](const std::string &candidateType) {
      if (isExplicitMethodPath) {
        return false;
      }
      if (methodName != "count" && methodName != "at" && methodName != "at_unsafe") {
        return false;
      }
      std::string normalized = normalizeBindingTypeName(candidateType);
      if (!normalized.empty() && normalized.front() == '/') {
        normalized.erase(normalized.begin());
      }
      return isMapCollectionTypeName(normalized);
    };
    if (typeName.empty()) {
      if (isPointerExpr(receiver, params, locals)) {
        typeName = "Pointer";
      } else if (isPointerLikeExpr(receiver, params, locals)) {
        typeName = "Reference";
      }
    }
    if (typeName == "Pointer" || typeName == "Reference") {
      if (!isExplicitMethodPath &&
          (methodName == "count" || methodName == "at" || methodName == "at_unsafe") &&
          isMapReceiverExpr(receiver)) {
        resolvedOut = preferredMapBodyArgumentTarget(methodName);
        return;
      }
      resolvedOut = "/" + typeName + "/" + methodName;
      return;
    }
    if (typeName.empty()) {
      resolvedOut = normalizeBodyArgumentTarget(resolveCalleePath(callExpr));
      return;
    }
    if (isPrimitiveBindingTypeName(typeName)) {
      resolvedOut = normalizeBodyArgumentTarget("/" + normalizeBindingTypeName(typeName) + "/" + methodName);
      return;
    }
    std::string resolvedType = resolveTypePath(typeName, receiver.namespacePrefix);
    if (structNames_.count(resolvedType) == 0 && defMap_.count(resolvedType) == 0) {
      auto importIt = importAliases_.find(typeName);
      if (importIt != importAliases_.end()) {
        resolvedType = importIt->second;
      }
    }
    if (shouldPreferCanonicalMapBodyArgumentTarget(resolvedType.empty() ? typeName : resolvedType)) {
      resolvedOut = preferredMapBodyArgumentTarget(methodName);
      return;
    }
    resolvedOut = normalizeBodyArgumentTarget(resolvedType + "/" + methodName);
  };

  if ((stmt.hasBodyArguments || !stmt.bodyArguments.empty()) && !isBuiltinBlockCall(stmt) && !stmt.isLambda) {
    std::string collectionName;
    if (defMap_.find(resolveCalleePath(stmt)) == defMap_.end() && getBuiltinCollectionName(stmt, collectionName)) {
      error_ = collectionName + " literal does not accept block arguments";
      return false;
    }
    std::string resolved;
    resolveBodyArgumentTarget(stmt, resolved);
    if (defMap_.count(resolved) == 0) {
      error_ = "block arguments require a definition target: " + resolved;
      return false;
    }
    Expr call = stmt;
    call.bodyArguments.clear();
    call.hasBodyArguments = false;
    if (!validateExpr(params, locals, call)) {
      return false;
    }
    std::unordered_map<std::string, BindingInfo> blockLocals = locals;
    std::vector<Expr> livenessStatements = stmt.bodyArguments;
    if (enclosingStatements != nullptr && statementIndex < enclosingStatements->size()) {
      for (size_t idx = statementIndex + 1; idx < enclosingStatements->size(); ++idx) {
        livenessStatements.push_back((*enclosingStatements)[idx]);
      }
    }
    BorrowEndScope borrowScope(*this, currentValidationContext_.endedReferenceBorrows);
    for (size_t bodyIndex = 0; bodyIndex < stmt.bodyArguments.size(); ++bodyIndex) {
      const Expr &bodyExpr = stmt.bodyArguments[bodyIndex];
      if (!validateStatement(params,
                             blockLocals,
                             bodyExpr,
                             returnKind,
                             false,
                             true,
                             nullptr,
                             namespacePrefix,
                             &stmt.bodyArguments,
                             bodyIndex)) {
        return false;
      }
      expireReferenceBorrowsForRemainder(params, blockLocals, livenessStatements, bodyIndex + 1);
    }
    return true;
  }
  return validateExpr(params, locals, stmt, enclosingStatements, statementIndex);
}

} // namespace primec::semantics
