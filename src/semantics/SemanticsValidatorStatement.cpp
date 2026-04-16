#include "SemanticsValidator.h"
#include "SemanticsValidatorStatementLoopCountStep.h"

#include <cctype>
#include <functional>
#include <optional>

namespace primec::semantics {

namespace {

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

} // namespace

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
  observeLocalMapSize(locals.size());
  auto failStatementDiagnostic = [&](std::string message) -> bool {
    return failExprDiagnostic(stmt, std::move(message));
  };
  const std::vector<std::string> *definitionTemplateArgs = nullptr;
  auto currentDefIt = defMap_.find(currentValidationState_.context.definitionPath);
  if (currentDefIt != defMap_.end() && currentDefIt->second != nullptr) {
    definitionTemplateArgs = &currentDefIt->second->templateArgs;
  }
  bool handledBindingStatement = false;
  if (!validateBindingStatement(params, locals, stmt, allowBindings, namespacePrefix, handledBindingStatement)) {
    if (error_.empty()) {
      return failStatementDiagnostic("validateBindingStatement failed");
    }
    return false;
  }
  if (handledBindingStatement) {
    observeLocalMapSize(locals.size());
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
      return failStatementDiagnostic("named arguments not supported for builtin calls");
    }
    if (!stmt.templateArgs.empty()) {
      return failStatementDiagnostic(name + " does not accept template arguments");
    }
    if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
      return failStatementDiagnostic(name + " does not accept block arguments");
    }
    const size_t expectedArgs = (name == "init") ? 2 : 1;
    if (stmt.args.size() != expectedArgs) {
      return failStatementDiagnostic(name + " requires exactly " + std::to_string(expectedArgs) + " argument" +
                                    (expectedArgs == 1 ? "" : "s"));
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
      return failStatementDiagnostic(name + " requires uninitialized<T> storage");
    }
    if (name == "init") {
      if (!validateExpr(params, locals, stmt.args[1])) {
        return false;
      }
      ReturnKind valueKind = inferExprReturnKind(stmt.args[1], params, locals);
      if (valueKind == ReturnKind::Void) {
        return failStatementDiagnostic("init requires a value");
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
        auto extractMapArgsFromAnyType = [&](const std::string &typeText,
                                             std::string &keyTypeOut,
                                             std::string &valueTypeOut) -> bool {
          keyTypeOut.clear();
          valueTypeOut.clear();
          const std::string normalizedType = normalizeBindingTypeName(typeText);
          if (extractMapKeyValueTypesFromTypeText(normalizedType, keyTypeOut, valueTypeOut)) {
            return true;
          }
          std::string structPath = resolveStructTypePath(normalizedType, namespacePrefix, structNames_);
          if (structPath.empty() && normalizedType.rfind("std/collections/experimental_map/Map__", 0) == 0) {
            structPath = "/" + normalizedType;
          } else if (structPath.empty() &&
                     normalizedType.rfind("/std/collections/experimental_map/Map__", 0) == 0) {
            structPath = normalizedType;
          }
          return !structPath.empty() && extractExperimentalMapFieldTypesFromStructPath(structPath,
                                                                                       keyTypeOut,
                                                                                       valueTypeOut);
        };
        std::string expectedMapKeyType;
        std::string expectedMapValueType;
        std::string actualMapKeyType;
        std::string actualMapValueType;
        if (extractMapArgsFromAnyType(expectedTrim, expectedMapKeyType, expectedMapValueType) &&
            extractMapArgsFromAnyType(actualTrim, actualMapKeyType, actualMapValueType)) {
          return typesMatch(expectedMapKeyType, actualMapKeyType) &&
                 typesMatch(expectedMapValueType, actualMapValueType);
        }
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
          if (expectedNorm == "Result") {
            if (expectedArgs.empty()) {
              return true;
            }
            if (expectedArgs.size() == 1) {
              return trimType(expectedArgs.front()) == "_" || trimType(actualArgs.front()) == "_" ||
                     typesMatch(expectedArgs.front(), actualArgs.front());
            }
            return typesMatch(expectedArgs[0], actualArgs[0]) &&
                   (trimType(expectedArgs[1]) == "_" || trimType(actualArgs[1]) == "_" ||
                    typesMatch(expectedArgs[1], actualArgs[1]));
          }
          for (size_t i = 0; i < expectedArgs.size(); ++i) {
            if (!typesMatch(expectedArgs[i], actualArgs[i])) {
              return false;
            }
          }
          return true;
        }
        const std::string expectedStruct =
            resolveStructTypePath(expectedTrim, namespacePrefix, structNames_);
        const std::string actualStruct =
            resolveStructTypePath(actualTrim, namespacePrefix, structNames_);
        if (!expectedStruct.empty() && expectedStruct == actualStruct) {
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
        std::string inferredTypeText;
        if (inferQueryExprTypeText(value, params, locals, inferredTypeText) &&
            !inferredTypeText.empty()) {
          typeOut = normalizeBindingTypeName(inferredTypeText);
          return true;
        }
        std::string structPath = inferStructReturnPath(value, params, locals);
        if (!structPath.empty()) {
          typeOut = structPath;
          return true;
        }
        return false;
      };
      const std::string expectedType = storageBinding.typeTemplateArg;
      std::string actualType;
      if (inferValueTypeString(stmt.args[1], actualType)) {
        if (!typesMatch(expectedType, actualType)) {
          const std::string expectedStruct = resolveStructTypePath(expectedType, namespacePrefix, structNames_);
          const std::string actualStruct = resolveStructTypePath(actualType, namespacePrefix, structNames_);
          return failStatementDiagnostic(initValueTypeMismatchDiagnostic(expectedStruct, actualStruct));
        }
        return true;
      }
      ReturnKind expectedKind = returnKindForTypeName(expectedType);
      if (expectedKind == ReturnKind::Array) {
        if (valueKind != ReturnKind::Unknown && valueKind != ReturnKind::Array) {
          return failStatementDiagnostic("init value type mismatch");
        }
      } else if (expectedKind != ReturnKind::Unknown) {
        if (valueKind != ReturnKind::Unknown && valueKind != expectedKind) {
          return failStatementDiagnostic("init value type mismatch");
        }
      } else {
        std::string expectedStruct = resolveStructTypePath(expectedType, namespacePrefix, structNames_);
        if (!expectedStruct.empty()) {
          if (valueKind != ReturnKind::Unknown && valueKind != ReturnKind::Array) {
            return failStatementDiagnostic("init value type mismatch");
          }
          if (valueKind == ReturnKind::Array) {
            std::string actualStruct = inferStructReturnPath(stmt.args[1], params, locals);
            if (!actualStruct.empty() && actualStruct != expectedStruct) {
              return failStatementDiagnostic(initValueTypeMismatchDiagnostic(expectedStruct, actualStruct));
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
        return failStatementDiagnostic("named arguments not supported for builtin calls");
      }
      if (!stmt.templateArgs.empty()) {
        return failStatementDiagnostic(name + " does not accept template arguments");
      }
      if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
        return failStatementDiagnostic(name + " does not accept block arguments");
      }
      if (stmt.args.size() != 1) {
        return failStatementDiagnostic(name + " requires exactly 1 argument");
      }
      if (!validateExpr(params, locals, stmt.args.front())) {
        return false;
      }
      if (!isUninitializedStorage(stmt.args.front())) {
        return failStatementDiagnostic(name + " requires uninitialized<T> storage");
      }
      return true;
    }
  }
  if (stmt.kind != Expr::Kind::Call) {
    if (!allowBindings) {
      return failStatementDiagnostic("execution body arguments must be calls");
    }
    return validateExpr(params, locals, stmt);
  }
  if (isReturnCall(stmt)) {
    return validateReturnStatement(params, locals, stmt, returnKind, allowReturn, sawReturn, namespacePrefix);
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
    return failStatementDiagnostic("block requires block arguments");
  }
  if (isBuiltinBlockCall(stmt) && stmt.hasBodyArguments) {
    if (hasNamedArguments(stmt.argNames) || !stmt.args.empty() || !stmt.templateArgs.empty()) {
      return failStatementDiagnostic("block does not accept arguments");
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
    LocalBindingScope blockLocalScope(*this, locals);
    OnErrorScope onErrorScope(*this, blockOnError);
    BorrowEndScope borrowScope(*this, currentValidationState_.endedReferenceBorrows);
    for (size_t bodyIndex = 0; bodyIndex < stmt.bodyArguments.size(); ++bodyIndex) {
      const Expr &bodyExpr = stmt.bodyArguments[bodyIndex];
      if (!validateStatement(params,
                             locals,
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
      expireReferenceBorrowsForRemainder(params, locals, stmt.bodyArguments, bodyIndex + 1);
    }
    auto isSoaOwnerBinding = [&](const BindingInfo &binding) -> bool {
      if (binding.typeName == "soa_vector") {
        return true;
      }
      std::string elemType;
      return extractExperimentalSoaVectorElementType(binding, elemType);
    };
    auto referenceRootForBorrowBinding =
        [&](const std::string &bindingName,
            const BindingInfo &binding) -> std::string {
      std::string normalized = normalizeBindingTypeName(binding.typeName);
      if (normalized.empty()) {
        return "";
      }
      std::string base;
      std::string arg;
      if (splitTemplateTypeName(normalized, base, arg)) {
        normalized = normalizeBindingTypeName(base);
      }
      const bool isSoaFieldView = normalized == "SoaFieldView" ||
                                  normalized == "std/collections/experimental_soa_storage/SoaFieldView";
      if (binding.typeName != "Reference" && !isSoaFieldView) {
        return "";
      }
      if (!binding.referenceRoot.empty()) {
        return binding.referenceRoot;
      }
      return bindingName;
    };
    auto hasActiveBorrowForRoot =
        [&](const std::string &borrowRoot) -> bool {
      if (borrowRoot.empty() ||
          currentValidationState_.context.definitionIsUnsafe) {
        return false;
      }
      auto hasBorrowFrom =
          [&](const std::string &bindingName,
              const BindingInfo &binding) -> bool {
        if (currentValidationState_.endedReferenceBorrows.count(bindingName) >
            0) {
          return false;
        }
        const std::string root =
            referenceRootForBorrowBinding(bindingName, binding);
        return !root.empty() && root == borrowRoot;
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
    for (const auto &name : blockLocalScope.insertedNames) {
      auto entryIt = locals.find(name);
      if (entryIt == locals.end()) {
        continue;
      }
      const BindingInfo &binding = entryIt->second;
      if (!isSoaOwnerBinding(binding)) {
        continue;
      }
      if (hasActiveBorrowForRoot(name)) {
        return failStatementDiagnostic(
            "borrowed binding: " + name + " (root: " + name +
            ", sink: " + name + ")");
      }
    }
    return true;
  }
  PrintBuiltin printBuiltin;
  if (getPrintBuiltin(stmt, printBuiltin)) {
    if (hasNamedArguments(stmt.argNames)) {
      return failStatementDiagnostic("named arguments not supported for builtin calls");
    }
    if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
      return failStatementDiagnostic(printBuiltin.name + " does not accept block arguments");
    }
    if (stmt.args.size() != 1) {
      return failStatementDiagnostic(printBuiltin.name + " requires exactly one argument");
    }
    const std::string effectName = (printBuiltin.target == PrintTarget::Err) ? "io_err" : "io_out";
    if (currentValidationState_.context.activeEffects.count(effectName) == 0) {
      return failStatementDiagnostic(printBuiltin.name + " requires " + effectName + " effect");
    }
    {
      EntryArgStringScope entryArgScope(*this, true);
      if (!validateExpr(params, locals, stmt.args.front())) {
        return false;
      }
    }
    if (!isPrintableStatementExpr(stmt.args.front(), params, locals)) {
      return failStatementDiagnostic(printBuiltin.name +
                                    " requires an integer/bool or string literal/binding argument");
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
  bool handledBodyArgumentStatement = false;
  if (!validateStatementBodyArguments(params,
                                      locals,
                                      stmt,
                                      returnKind,
                                      namespacePrefix,
                                      enclosingStatements,
                                      statementIndex,
                                      handledBodyArgumentStatement)) {
    return false;
  }
  if (handledBodyArgumentStatement) {
    return true;
  }
  return validateExpr(params, locals, stmt, enclosingStatements, statementIndex);
}

} // namespace primec::semantics
