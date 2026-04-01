#include "SemanticsValidator.h"

#include <cctype>
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
    auto declaresAutoReturn = [&]() {
      auto defIt = defMap_.find(currentValidationContext_.definitionPath);
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
        error_ = "unable to infer return type on " + currentValidationContext_.definitionPath;
      }
      return false;
    }
    std::string soaFieldViewName;
    if (isBuiltinSoaFieldViewExpr(stmt.args.front(), params, locals, &soaFieldViewName)) {
      error_ = soaFieldViewPendingDiagnostic(soaFieldViewName);
      return false;
    }
    if (isBuiltinSoaRefExpr(stmt.args.front(), params, locals)) {
      error_ = soaBorrowedViewPendingDiagnostic();
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
    bool validatedPointerLikeReturn = false;
    auto currentDefIt = defMap_.find(currentValidationContext_.definitionPath);
    if (currentDefIt != defMap_.end() && currentDefIt->second != nullptr) {
      BindingInfo declaredReturnBinding;
      if (inferDefinitionReturnBinding(*currentDefIt->second, declaredReturnBinding) &&
          (declaredReturnBinding.typeName == "Pointer" ||
           declaredReturnBinding.typeName == "Reference")) {
        BindingInfo actualReturnBinding;
        if (!inferBindingTypeFromInitializer(stmt.args.front(), params, locals, actualReturnBinding) ||
            actualReturnBinding.typeName != declaredReturnBinding.typeName ||
            !errorTypesMatch(actualReturnBinding.typeTemplateArg,
                             declaredReturnBinding.typeTemplateArg,
                             namespacePrefix)) {
          error_ = "return type mismatch: expected " + declaredReturnBinding.typeName;
          return false;
        }
        validatedPointerLikeReturn = true;
      }
    }
    if (!validatedPointerLikeReturn && returnKind != ReturnKind::Unknown) {
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
