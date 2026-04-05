#include "SemanticsValidator.h"

#include <cctype>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace primec::semantics {
namespace {

bool isMatrixQuaternionTypePathForReturnDiagnostic(const std::string &typePath) {
  return typePath == "/std/math/Mat2" || typePath == "/std/math/Mat3" || typePath == "/std/math/Mat4" ||
         typePath == "/std/math/Quat";
}

bool isVectorTypePathForReturnDiagnostic(const std::string &typePath) {
  return typePath == "/std/math/Vec2" || typePath == "/std/math/Vec3" || typePath == "/std/math/Vec4";
}

bool isMatrixQuaternionConversionTypePathForReturn(const std::string &typePath) {
  return isMatrixQuaternionTypePathForReturnDiagnostic(typePath) || isVectorTypePathForReturnDiagnostic(typePath);
}

bool isImplicitMatrixQuaternionReturnConversion(const std::string &expectedTypePath,
                                                const std::string &actualTypePath) {
  return !expectedTypePath.empty() && !actualTypePath.empty() && expectedTypePath != actualTypePath &&
         isMatrixQuaternionConversionTypePathForReturn(expectedTypePath) &&
         isMatrixQuaternionConversionTypePathForReturn(actualTypePath) &&
         (isMatrixQuaternionTypePathForReturnDiagnostic(expectedTypePath) ||
          isMatrixQuaternionTypePathForReturnDiagnostic(actualTypePath));
}

std::string implicitMatrixQuaternionReturnDiagnostic(const std::string &expectedTypePath,
                                                     const std::string &actualTypePath) {
  return "implicit matrix/quaternion family conversion requires explicit helper: expected " + expectedTypePath +
         " got " + actualTypePath;
}

std::string returnTypeMismatchDiagnostic(const std::string &expectedTypePath,
                                         const std::string &actualTypePath,
                                         const std::string &fallbackExpectedType) {
  if (isImplicitMatrixQuaternionReturnConversion(expectedTypePath, actualTypePath)) {
    return "return type mismatch: " +
           implicitMatrixQuaternionReturnDiagnostic(expectedTypePath, actualTypePath);
  }
  return "return type mismatch: expected " + fallbackExpectedType;
}

} // namespace

bool SemanticsValidator::validateReturnStatement(const std::vector<ParameterInfo> &params,
                                                 std::unordered_map<std::string, BindingInfo> &locals,
                                                 const Expr &stmt,
                                                 ReturnKind returnKind,
                                                 bool allowReturn,
                                                 bool *sawReturn,
                                                 const std::string &namespacePrefix) {
  auto failReturnDiagnostic = [&](std::string message) -> bool {
    return failExprDiagnostic(stmt, std::move(message));
  };
  auto rewriteAutoReturnDiagnostic = [&](std::string message) -> bool {
    return failExprDiagnostic(stmt, std::move(message));
  };
  if (hasNamedArguments(stmt.argNames)) {
    return failReturnDiagnostic("named arguments not supported for builtin calls");
  }
  if (!allowReturn) {
    return failReturnDiagnostic("return not allowed in execution body");
  }
  if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
    return failReturnDiagnostic("return does not accept block arguments");
  }
  if (returnKind == ReturnKind::Void) {
    if (!stmt.args.empty()) {
      return failReturnDiagnostic("return value not allowed for void definition");
    }
  } else {
    if (stmt.args.size() != 1) {
      return failReturnDiagnostic("return requires exactly one argument");
    }
    auto declaresAutoReturn = [&]() {
      auto defIt = defMap_.find(currentValidationState_.context.definitionPath);
      if (defIt == defMap_.end() || defIt->second == nullptr) {
        return false;
      }
      for (const auto &transform : defIt->second->transforms) {
        if (transform.name == "return" && transform.templateArgs.size() == 1 &&
            transform.templateArgs.front() == "auto") {
          return true;
        }
      }
      return false;
    };
    if (!validateExpr(params, locals, stmt.args.front())) {
      if (declaresAutoReturn() &&
          (error_ == "unknown method: /map/at" || error_ == "unknown method: /map/at_unsafe")) {
        return rewriteAutoReturnDiagnostic("unable to infer return type on " +
                                           currentValidationState_.context.definitionPath);
      }
      return false;
    }
    auto isStandaloneSoaRefCall = [&](const Expr &expr,
                                      const Expr *&receiverOut) -> bool {
      receiverOut = nullptr;
      if (expr.kind != Expr::Kind::Call || expr.args.size() != 2) {
        return false;
      }
      const std::string resolvedPath = resolveCalleePath(expr);
      if (expr.isMethodCall) {
        if (expr.name != "ref" &&
            resolvedPath.rfind("/std/collections/soa_vector/ref", 0) != 0 &&
            resolvedPath.rfind("/soa_vector/ref", 0) != 0 &&
            resolvedPath.rfind("/std/collections/experimental_soa_vector/soaVectorRef", 0) != 0) {
          return false;
        }
        receiverOut = &expr.args.front();
        return true;
      }
      if (!isSimpleCallName(expr, "ref") &&
          resolvedPath.rfind("/std/collections/soa_vector/ref", 0) != 0 &&
          resolvedPath.rfind("/soa_vector/ref", 0) != 0 &&
          resolvedPath.rfind("/std/collections/experimental_soa_vector/soaVectorRef", 0) != 0 &&
          resolvedPath.rfind("/std/collections/experimental_soa_storage/soaColumnRef", 0) != 0) {
        return false;
      }
      receiverOut = &expr.args.front();
      return true;
    };
    auto failReturnEscapeDiagnostic = [&](std::string message) -> bool {
      return failReturnDiagnostic(std::move(message));
    };
    const Expr &returnExpr = stmt.args.front();
    const Expr *escapeReceiver = nullptr;
    if (isStandaloneSoaRefCall(returnExpr, escapeReceiver) &&
        escapeReceiver != nullptr &&
        escapeReceiver->kind != Expr::Kind::Name) {
      return failReturnEscapeDiagnostic("reference escapes via return");
    }
    if (isBuiltinSoaFieldViewExpr(returnExpr, params, locals, nullptr) &&
        !returnExpr.args.empty()) {
      const Expr &fieldViewReceiver = returnExpr.args.front();
      if (fieldViewReceiver.kind != Expr::Kind::Name) {
        return failReturnEscapeDiagnostic("field-view escapes via return");
      }
    }
    auto declaredReferenceReturnTarget = [&]() -> std::optional<std::string> {
      auto defIt = defMap_.find(currentValidationState_.context.definitionPath);
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
    auto resolveDirectBorrowReturnTargetType = [&](const Expr &expr, std::string &targetOut) -> bool {
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
    auto referenceRootForReturnBinding = [&](const std::string &bindingName,
                                             const BindingInfo &binding) -> std::string {
      if (binding.typeName != "Reference" && binding.typeName != "Pointer") {
        return "";
      }
      if (!binding.referenceRoot.empty()) {
        return binding.referenceRoot;
      }
      return bindingName;
    };
    std::function<bool(const Expr &, const ExprSubstitutions &, std::string &, bool &)>
        resolveParameterOwnedBorrowReturnTarget;
    resolveParameterOwnedBorrowReturnTarget = [&](const Expr &returnExpr,
                                                  const ExprSubstitutions &substitutions,
                                                  std::string &targetOut,
                                                  bool &recognizedOut) -> bool {
      recognizedOut = false;
      targetOut.clear();
      if (returnExpr.kind == Expr::Kind::Name) {
        if (const Expr *substitutedExpr = findSubstitutedExpr(substitutions, returnExpr.name)) {
          return resolveParameterOwnedBorrowReturnTarget(*substitutedExpr, substitutions, targetOut, recognizedOut);
        }
        return false;
      }

      auto parameterOwnedRoot = [&](const std::string &candidateRoot) {
        if (candidateRoot.empty()) {
          return false;
        }
        for (const auto &param : params) {
          std::string paramRoot;
          if (param.binding.typeName == "Reference" || param.binding.typeName == "Pointer") {
            paramRoot = referenceRootForReturnBinding(param.name, param.binding);
          } else {
            paramRoot = param.name;
          }
          if (!paramRoot.empty() &&
              (candidateRoot == paramRoot || candidateRoot.rfind(paramRoot + ".", 0) == 0)) {
            return true;
          }
        }
        return false;
      };
      auto resolvePointerLikeRoot = [&](const std::string &bindingName,
                                        const BindingInfo &binding,
                                        bool isParamBinding,
                                        std::string &rootOut) -> bool {
        if (binding.typeName != "Reference" && binding.typeName != "Pointer") {
          return false;
        }
        const std::string root = referenceRootForReturnBinding(bindingName, binding);
        if (root.empty()) {
          return false;
        }
        if (!isParamBinding && !parameterOwnedRoot(root)) {
          return false;
        }
        rootOut = root;
        return true;
      };
      std::function<bool(const Expr &, std::string &)> resolveParameterOwnedStorageRoot;
      resolveParameterOwnedStorageRoot = [&](const Expr &storageExpr, std::string &rootOut) -> bool {
        if (storageExpr.kind == Expr::Kind::Name) {
          if (const Expr *substitutedExpr = findSubstitutedExpr(substitutions, storageExpr.name)) {
            return resolveParameterOwnedStorageRoot(*substitutedExpr, rootOut);
          }
          if (const BindingInfo *paramBinding = findParamBinding(params, storageExpr.name)) {
            if (resolvePointerLikeRoot(storageExpr.name, *paramBinding, true, rootOut)) {
              return true;
            }
            rootOut = storageExpr.name;
            return true;
          }
          auto localIt = locals.find(storageExpr.name);
          if (localIt == locals.end()) {
            return false;
          }
          return resolvePointerLikeRoot(localIt->first, localIt->second, false, rootOut);
        }

        std::string builtinName;
        if (storageExpr.kind == Expr::Kind::Call && getBuiltinPointerName(storageExpr, builtinName) &&
            builtinName == "dereference" && storageExpr.args.size() == 1) {
          return resolveParameterOwnedStorageRoot(storageExpr.args.front(), rootOut);
        }

        if (storageExpr.kind == Expr::Kind::Call && storageExpr.isFieldAccess && storageExpr.args.size() == 1) {
          std::string receiverRoot;
          if (!resolveParameterOwnedStorageRoot(storageExpr.args.front(), receiverRoot) || receiverRoot.empty()) {
            return false;
          }
          rootOut = receiverRoot + "." + storageExpr.name;
          return true;
        }

        return false;
      };
      auto resolveParameterOwnedReferenceCallTarget = [&](const Expr &callExpr, std::string &targetTypeOut) -> bool {
        if (callExpr.kind != Expr::Kind::Call) {
          return false;
        }
        auto defIt = defMap_.find(resolveCalleePath(callExpr));
        if (defIt == defMap_.end() || defIt->second == nullptr) {
          return false;
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
            return false;
          }
          for (const Expr &argExpr : callExpr.args) {
            std::string root;
            if (resolveParameterOwnedStorageRoot(argExpr, root) && !root.empty()) {
              targetTypeOut = args.front();
              return true;
            }
          }
          return false;
        }
        return false;
      };

      if (returnExpr.kind != Expr::Kind::Call || returnExpr.isMethodCall ||
          !isSimpleCallName(returnExpr, "borrow") || returnExpr.args.size() != 1) {
        if (resolveParameterOwnedReferenceCallTarget(returnExpr, targetOut)) {
          recognizedOut = true;
          return true;
        }
        ExprSubstitutions nestedSubstitutions;
        const Expr *returnedValueExpr = nullptr;
        if (returnExpr.kind == Expr::Kind::Call &&
            appendCallSubstitutions(returnExpr, substitutions, nestedSubstitutions, returnedValueExpr) &&
            returnedValueExpr != nullptr) {
          return resolveParameterOwnedBorrowReturnTarget(*returnedValueExpr, nestedSubstitutions, targetOut,
                                                        recognizedOut);
        }
        return false;
      }
      recognizedOut = true;

      const Expr &storageExpr = returnExpr.args.front();
      BindingInfo storageBinding;
      bool resolvedStorage = false;
      if (!resolveUninitializedStorageBinding(params, locals, storageExpr, storageBinding, resolvedStorage) ||
          !resolvedStorage || storageBinding.typeName != "uninitialized" || storageBinding.typeTemplateArg.empty()) {
        return false;
      }

      std::string borrowRoot;
      if (!resolveParameterOwnedStorageRoot(storageExpr, borrowRoot) || borrowRoot.empty()) {
        return false;
      }

      targetOut = storageBinding.typeTemplateArg;
      return true;
    };
    if (std::optional<std::string> expectedReferenceTarget = declaredReferenceReturnTarget();
        expectedReferenceTarget.has_value()) {
        const Expr &returnExpr = stmt.args.front();
      if (returnExpr.kind != Expr::Kind::Name) {
        std::string borrowedTargetType;
        bool recognizedBorrowReturn = false;
        const ExprSubstitutions substitutions;
        if (!resolveParameterOwnedBorrowReturnTarget(returnExpr, substitutions, borrowedTargetType,
                                                    recognizedBorrowReturn)) {
          return failReturnDiagnostic("reference return requires direct parameter reference or parameter-rooted borrow");
        }
        if (normalizeReferenceTarget(borrowedTargetType) != normalizeReferenceTarget(*expectedReferenceTarget)) {
          return failReturnDiagnostic("reference return type mismatch");
        }
      } else {
        const BindingInfo *paramBinding = findParamBinding(params, returnExpr.name);
        if (paramBinding == nullptr || paramBinding->typeName != "Reference") {
          return failReturnDiagnostic("reference return requires direct parameter reference or parameter-rooted borrow");
        }
        if (normalizeReferenceTarget(paramBinding->typeTemplateArg) !=
            normalizeReferenceTarget(*expectedReferenceTarget)) {
          return failReturnDiagnostic("reference return type mismatch");
        }
      }
    }
    bool validatedPointerLikeReturn = false;
    auto currentDefIt = defMap_.find(currentValidationState_.context.definitionPath);
    if (currentDefIt != defMap_.end() && currentDefIt->second != nullptr) {
      BindingInfo declaredReturnBinding;
      if (inferDefinitionReturnBinding(*currentDefIt->second, declaredReturnBinding) &&
          (declaredReturnBinding.typeName == "Pointer" ||
           declaredReturnBinding.typeName == "Reference")) {
        BindingInfo actualReturnBinding;
        std::string directBorrowTargetType;
        const bool directBorrowReturn =
            declaredReturnBinding.typeName == "Reference" &&
            resolveDirectBorrowReturnTargetType(stmt.args.front(), directBorrowTargetType);
        if (directBorrowReturn) {
          if (!errorTypesMatch(directBorrowTargetType, declaredReturnBinding.typeTemplateArg, namespacePrefix)) {
            return failReturnDiagnostic("return type mismatch: expected " + declaredReturnBinding.typeName);
          }
        } else if (!inferBindingTypeFromInitializer(stmt.args.front(), params, locals, actualReturnBinding) ||
                   actualReturnBinding.typeName != declaredReturnBinding.typeName ||
                   !errorTypesMatch(actualReturnBinding.typeTemplateArg,
                                    declaredReturnBinding.typeTemplateArg,
                                    namespacePrefix)) {
          return failReturnDiagnostic("return type mismatch: expected " + declaredReturnBinding.typeName);
        }
        validatedPointerLikeReturn = true;
      }
    }
    if (!validatedPointerLikeReturn && returnKind != ReturnKind::Unknown) {
      ReturnKind exprKind = inferExprReturnKind(stmt.args.front(), params, locals);
      if (returnKind == ReturnKind::Array) {
        auto structIt = returnStructs_.find(currentValidationState_.context.definitionPath);
        if (structIt != returnStructs_.end()) {
          auto normalizeCollectionStructPath = [&](const std::string &typePath) -> std::string {
            std::string normalizedTypePath = normalizeBindingTypeName(typePath);
            if (!normalizedTypePath.empty() && normalizedTypePath.front() == '/') {
              normalizedTypePath.erase(normalizedTypePath.begin());
            }
            if (normalizedTypePath.rfind("std/collections/experimental_map/Map__", 0) == 0) {
              return "/map";
            }
            if (normalizedTypePath == "std/collections/experimental_vector/Vector" ||
                normalizedTypePath.rfind("std/collections/experimental_vector/Vector__", 0) == 0) {
              return "/vector";
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
            if (actualStruct.empty()) {
              BindingInfo actualReturnBinding;
              if (inferBindingTypeFromInitializer(stmt.args.front(), params, locals, actualReturnBinding)) {
                if (!actualReturnBinding.typeTemplateArg.empty()) {
                  actualStruct = resolveStructTypePath(
                      actualReturnBinding.typeName + "<" + actualReturnBinding.typeTemplateArg + ">",
                      namespacePrefix,
                      structNames_);
                } else if (!actualReturnBinding.typeName.empty()) {
                  actualStruct = resolveStructTypePath(
                      actualReturnBinding.typeName,
                      namespacePrefix,
                      structNames_);
                  if (actualStruct.empty() && actualReturnBinding.typeName.front() == '/') {
                    actualStruct = actualReturnBinding.typeName;
                  }
                }
              }
            }
            if (actualStruct.empty()) {
              std::string inferredTypeText;
              if (inferQueryExprTypeText(stmt.args.front(), params, locals, inferredTypeText) &&
                  !inferredTypeText.empty()) {
                actualStruct = resolveStructTypePath(
                    inferredTypeText,
                    namespacePrefix,
                    structNames_);
              }
            }
            const std::string normalizedExpectedStruct = normalizeCollectionStructPath(structIt->second);
            const std::string normalizedActualStruct = normalizeCollectionStructPath(actualStruct);
            const std::string resolvedExpectedStruct =
                resolveStructTypePath(structIt->second, namespacePrefix, structNames_);
            const std::string resolvedActualStruct =
                resolveStructTypePath(actualStruct, namespacePrefix, structNames_);
            if (actualStruct.empty() ||
                ((actualStruct != structIt->second &&
                  (resolvedExpectedStruct.empty() || resolvedExpectedStruct != resolvedActualStruct)) &&
                 (normalizedExpectedStruct.empty() || normalizedExpectedStruct != normalizedActualStruct))) {
              std::string expectedType = structIt->second;
              if (expectedType == "/array" || expectedType == "/vector" || expectedType == "/map" ||
                expectedType == "/string") {
              expectedType.erase(0, 1);
            }
            return failReturnDiagnostic(returnTypeMismatchDiagnostic(structIt->second, actualStruct, expectedType));
          }
        } else if (exprKind != ReturnKind::Array) {
          return failReturnDiagnostic("return type mismatch: expected array");
        }
      } else if (exprKind != ReturnKind::Unknown && exprKind != returnKind) {
        return failReturnDiagnostic("return type mismatch: expected " + typeNameForReturnKind(returnKind));
      }
    }
  }
  if (sawReturn) {
    *sawReturn = true;
  }
  return true;
}

bool SemanticsValidator::statementAlwaysReturns(const Expr &stmt) {
  auto branchAlwaysReturns = [&](const Expr &branch) -> bool {
    if (isEnvelopeValueExpr(branch, true)) {
      return blockAlwaysReturns(branch.bodyArguments) || getEnvelopeValueExpr(branch, true) != nullptr;
    }
    return statementAlwaysReturns(branch);
  };

  if (isReturnCall(stmt)) {
    return true;
  }
  if (isMatchCall(stmt)) {
    Expr expanded;
    std::string error;
    if (!lowerMatchToIf(stmt, expanded, error)) {
      return false;
    }
    return statementAlwaysReturns(expanded);
  }
  if (isIfCall(stmt) && stmt.args.size() == 3) {
    return branchAlwaysReturns(stmt.args[1]) && branchAlwaysReturns(stmt.args[2]);
  }
  if (getEnvelopeValueExpr(stmt, false) != nullptr) {
    return true;
  }
  if (isBuiltinBlockCall(stmt) && stmt.hasBodyArguments) {
    return blockAlwaysReturns(stmt.bodyArguments);
  }
  return false;
}

bool SemanticsValidator::blockAlwaysReturns(const std::vector<Expr> &statements) {
  for (const auto &stmt : statements) {
    if (statementAlwaysReturns(stmt)) {
      return true;
    }
  }
  return false;
}

} // namespace primec::semantics
