#include "SemanticsValidator.h"
#include "SemanticsValidatorExprNumericInternal.h"

namespace primec::semantics {

using detail::MatrixQuaternionDivideInfo;
using detail::MatrixQuaternionMultiplyInfo;
using detail::NumericCategory;
using detail::NumericDomain;
using detail::NumericInfo;

bool SemanticsValidator::validateNumericBuiltinExpr(const std::vector<ParameterInfo> &params,
                                                    const std::unordered_map<std::string, BindingInfo> &locals,
                                                    const Expr &expr,
                                                    bool &handled) {
  handled = false;
  auto failNumericDiagnostic = [&](std::string message) -> bool {
    return failExprDiagnostic(expr, std::move(message));
  };

  auto numericInfoForKind = [](ReturnKind kind) -> NumericInfo {
    switch (kind) {
      case ReturnKind::Int:
      case ReturnKind::Int64:
      case ReturnKind::UInt64:
        return {NumericDomain::Fixed, NumericCategory::Integer};
      case ReturnKind::Float32:
      case ReturnKind::Float64:
        return {NumericDomain::Fixed, NumericCategory::Float};
      case ReturnKind::Integer:
        return {NumericDomain::Software, NumericCategory::Integer};
      case ReturnKind::Decimal:
        return {NumericDomain::Software, NumericCategory::Float};
      case ReturnKind::Complex:
        return {NumericDomain::Software, NumericCategory::Complex};
      case ReturnKind::Bool:
        return {NumericDomain::Fixed, NumericCategory::Integer};
      default:
        return {NumericDomain::Unknown, NumericCategory::Unknown};
    }
  };
  auto numericInfoForTypeName = [](const std::string &typeName) -> NumericInfo {
    if (typeName == "int" || typeName == "i32" || typeName == "i64" || typeName == "u64" || typeName == "bool") {
      return {NumericDomain::Fixed, NumericCategory::Integer};
    }
    if (typeName == "float" || typeName == "f32" || typeName == "f64") {
      return {NumericDomain::Fixed, NumericCategory::Float};
    }
    if (typeName == "integer") {
      return {NumericDomain::Software, NumericCategory::Integer};
    }
    if (typeName == "decimal") {
      return {NumericDomain::Software, NumericCategory::Float};
    }
    if (typeName == "complex") {
      return {NumericDomain::Software, NumericCategory::Complex};
    }
    return {NumericDomain::Unknown, NumericCategory::Unknown};
  };
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
  auto isBoolExpr = [&](const Expr &arg) -> bool {
    ReturnKind kind = inferExprReturnKind(arg, params, locals);
    if (kind == ReturnKind::Bool) {
      return true;
    }
    if (kind == ReturnKind::Void || kind == ReturnKind::Array || kind == ReturnKind::Int ||
        kind == ReturnKind::Int64 || kind == ReturnKind::UInt64 || kind == ReturnKind::Float32 ||
        kind == ReturnKind::Float64) {
      return false;
    }
    if (kind == ReturnKind::Unknown) {
      if (arg.kind == Expr::Kind::FloatLiteral || arg.kind == Expr::Kind::StringLiteral) {
        return false;
      }
      if (isPointerExpr(arg, params, locals)) {
        return false;
      }
      if (arg.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, arg.name)) {
          return returnKindForBinding(*paramBinding) == ReturnKind::Bool;
        }
        auto it = locals.find(arg.name);
        if (it != locals.end()) {
          return returnKindForBinding(it->second) == ReturnKind::Bool;
        }
      }
      return true;
    }
    return false;
  };
  auto isStringExpr = [&](const Expr &arg) -> bool {
    return arg.kind == Expr::Kind::StringLiteral || inferExprReturnKind(arg, params, locals) == ReturnKind::String;
  };
  auto isUnsignedExpr = [&](const Expr &arg) -> bool {
    return inferExprReturnKind(arg, params, locals) == ReturnKind::UInt64;
  };
  auto hasMixedSignedness = [&](const std::vector<Expr> &args, bool boolCountsAsSigned) -> bool {
    bool sawUnsigned = false;
    bool sawSigned = false;
    for (const auto &arg : args) {
      ReturnKind kind = inferExprReturnKind(arg, params, locals);
      if (kind == ReturnKind::Integer || kind == ReturnKind::Decimal || kind == ReturnKind::Complex) {
        continue;
      }
      if (kind == ReturnKind::UInt64) {
        sawUnsigned = true;
        continue;
      }
      if (kind == ReturnKind::Int || kind == ReturnKind::Int64 || (boolCountsAsSigned && kind == ReturnKind::Bool)) {
        sawSigned = true;
      }
    }
    return sawUnsigned && sawSigned;
  };
  auto numericCategoryForExpr = [&](const Expr &arg) -> NumericCategory {
    ReturnKind kind = inferExprReturnKind(arg, params, locals);
    NumericInfo info = numericInfoForKind(kind);
    if (info.category != NumericCategory::Unknown) {
      return info.category;
    }
    if (kind == ReturnKind::Unknown) {
      if (arg.kind == Expr::Kind::FloatLiteral) {
        return NumericCategory::Float;
      }
      if (arg.kind == Expr::Kind::StringLiteral) {
        return NumericCategory::Unknown;
      }
      if (isPointerExpr(arg, params, locals)) {
        return NumericCategory::Unknown;
      }
      if (arg.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, arg.name)) {
          NumericInfo typeInfo = numericInfoForTypeName(paramBinding->typeName);
          if (typeInfo.category != NumericCategory::Unknown) {
            return typeInfo.category;
          }
          if (paramBinding->typeName == "Reference") {
            NumericInfo refInfo = numericInfoForTypeName(paramBinding->typeTemplateArg);
            if (refInfo.category != NumericCategory::Unknown) {
              return refInfo.category;
            }
          }
        }
        auto it = locals.find(arg.name);
        if (it != locals.end()) {
          NumericInfo typeInfo = numericInfoForTypeName(it->second.typeName);
          if (typeInfo.category != NumericCategory::Unknown) {
            return typeInfo.category;
          }
          if (it->second.typeName == "Reference") {
            NumericInfo refInfo = numericInfoForTypeName(it->second.typeTemplateArg);
            if (refInfo.category != NumericCategory::Unknown) {
              return refInfo.category;
            }
          }
        }
      }
    }
    return NumericCategory::Unknown;
  };
  auto numericDomainForExpr = [&](const Expr &arg) -> NumericDomain {
    ReturnKind kind = inferExprReturnKind(arg, params, locals);
    NumericInfo info = numericInfoForKind(kind);
    if (info.domain != NumericDomain::Unknown) {
      return info.domain;
    }
    if (kind == ReturnKind::Unknown) {
      if (arg.kind == Expr::Kind::StringLiteral) {
        return NumericDomain::Unknown;
      }
      if (isPointerExpr(arg, params, locals)) {
        return NumericDomain::Unknown;
      }
      if (arg.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, arg.name)) {
          NumericInfo typeInfo = numericInfoForTypeName(paramBinding->typeName);
          if (typeInfo.domain != NumericDomain::Unknown) {
            return typeInfo.domain;
          }
          if (paramBinding->typeName == "Reference") {
            NumericInfo refInfo = numericInfoForTypeName(paramBinding->typeTemplateArg);
            if (refInfo.domain != NumericDomain::Unknown) {
              return refInfo.domain;
            }
          }
        }
        auto it = locals.find(arg.name);
        if (it != locals.end()) {
          NumericInfo typeInfo = numericInfoForTypeName(it->second.typeName);
          if (typeInfo.domain != NumericDomain::Unknown) {
            return typeInfo.domain;
          }
          if (it->second.typeName == "Reference") {
            NumericInfo refInfo = numericInfoForTypeName(it->second.typeTemplateArg);
            if (refInfo.domain != NumericDomain::Unknown) {
              return refInfo.domain;
            }
          }
        }
      }
    }
    return NumericDomain::Unknown;
  };
  auto hasMixedNumericDomain = [&](const std::vector<Expr> &args) -> bool {
    bool sawFixed = false;
    bool sawSoftware = false;
    for (const auto &arg : args) {
      NumericDomain domain = numericDomainForExpr(arg);
      if (domain == NumericDomain::Fixed) {
        sawFixed = true;
      } else if (domain == NumericDomain::Software) {
        sawSoftware = true;
      }
    }
    return sawFixed && sawSoftware;
  };
  auto hasMixedNumericCategory = [&](const std::vector<Expr> &args) -> bool {
    bool sawFloat = false;
    bool sawInteger = false;
    bool sawComplex = false;
    for (const auto &arg : args) {
      NumericCategory category = numericCategoryForExpr(arg);
      if (category == NumericCategory::Float) {
        sawFloat = true;
      } else if (category == NumericCategory::Integer) {
        sawInteger = true;
      } else if (category == NumericCategory::Complex) {
        sawComplex = true;
      }
    }
    if (sawComplex && (sawFloat || sawInteger)) {
      return true;
    }
    return sawFloat && sawInteger;
  };
  auto hasComplexNumeric = [&](const std::vector<Expr> &args) -> bool {
    for (const auto &arg : args) {
      if (numericCategoryForExpr(arg) == NumericCategory::Complex) {
        return true;
      }
    }
    return false;
  };
  auto hasMixedComplexNumeric = [&](const std::vector<Expr> &args) -> bool {
    bool sawComplex = false;
    bool sawOther = false;
    for (const auto &arg : args) {
      NumericCategory category = numericCategoryForExpr(arg);
      if (category == NumericCategory::Complex) {
        sawComplex = true;
      } else if (category == NumericCategory::Integer || category == NumericCategory::Float) {
        sawOther = true;
      }
    }
    return sawComplex && sawOther;
  };
  auto classifyMatrixQuaternionMultiply = [&](const Expr &left, const Expr &right) -> MatrixQuaternionMultiplyInfo {
    const std::string leftMathType = inferMatrixQuaternionTypePath(left, params, locals);
    const std::string rightMathType = inferMatrixQuaternionTypePath(right, params, locals);
    if (leftMathType.empty() && rightMathType.empty()) {
      return {};
    }
    const std::string rightVectorType = inferVectorTypePath(right, params, locals);
    MatrixQuaternionMultiplyInfo result;
    result.handled = true;
    if (!leftMathType.empty()) {
      if (leftMathType == "/std/math/Quat") {
        result.valid = rightMathType == "/std/math/Quat" || rightVectorType == "/std/math/Vec3" ||
                       (rightMathType.empty() && rightVectorType.empty() && isNumericExpr(params, locals, right));
        return result;
      }
      if (!rightMathType.empty()) {
        result.valid = leftMathType == rightMathType;
        return result;
      }
      if (!rightVectorType.empty()) {
        result.valid = detail::matrixDimensionForTypePath(leftMathType) ==
                       detail::vectorDimensionForTypePath(rightVectorType);
        return result;
      }
      result.valid = isNumericExpr(params, locals, right);
      return result;
    }
    if (rightMathType == "/std/math/Quat") {
      result.valid = isNumericExpr(params, locals, left);
      return result;
    }
    result.valid = isNumericExpr(params, locals, left);
    return result;
  };
  auto classifyMatrixQuaternionDivide = [&](const Expr &left, const Expr &right) -> MatrixQuaternionDivideInfo {
    const std::string leftMathType = inferMatrixQuaternionTypePath(left, params, locals);
    const std::string rightMathType = inferMatrixQuaternionTypePath(right, params, locals);
    if (leftMathType.empty() && rightMathType.empty()) {
      return {};
    }
    MatrixQuaternionDivideInfo result;
    result.handled = true;
    result.valid = !leftMathType.empty() && rightMathType.empty() && isNumericExpr(params, locals, right);
    return result;
  };
  auto validateArgs = [&]() -> bool {
    for (const auto &arg : expr.args) {
      if (!validateExpr(params, locals, arg)) {
        return false;
      }
    }
    return true;
  };

  std::string builtinName;
  if (getBuiltinOperatorName(expr, builtinName)) {
    handled = true;
    size_t expectedArgs = builtinName == "negate" ? 1 : 2;
    if (expr.args.size() != expectedArgs) {
      return failNumericDiagnostic("argument count mismatch for builtin " + builtinName);
    }
    const bool isPlusMinus = builtinName == "plus" || builtinName == "minus";
    bool leftPointer = false;
    bool rightPointer = false;
    if (expr.args.size() == 2) {
      leftPointer = isPointerExpr(expr.args[0], params, locals);
      rightPointer = isPointerExpr(expr.args[1], params, locals);
    }
    if (isPlusMinus && expr.args.size() == 2 && (leftPointer || rightPointer)) {
      if (leftPointer && rightPointer) {
        return failNumericDiagnostic("pointer arithmetic does not support pointer + pointer");
      }
      if (rightPointer) {
        return failNumericDiagnostic("pointer arithmetic requires pointer on the left");
      }
      const Expr &offsetExpr = expr.args[1];
      ReturnKind offsetKind = inferExprReturnKind(offsetExpr, params, locals);
      if (offsetKind != ReturnKind::Unknown && offsetKind != ReturnKind::Int && offsetKind != ReturnKind::Int64 &&
          offsetKind != ReturnKind::UInt64) {
        return failNumericDiagnostic("pointer arithmetic requires integer offset");
      }
    } else if (leftPointer || rightPointer) {
      return failNumericDiagnostic("arithmetic operators require numeric operands in " +
                                   currentValidationState_.context.definitionPath);
    }
    if (builtinName == "negate") {
      if (!isNumericExpr(params, locals, expr.args.front())) {
        return failNumericDiagnostic("arithmetic operators require numeric operands in " +
                                     currentValidationState_.context.definitionPath);
      }
      if (isUnsignedExpr(expr.args.front())) {
        return failNumericDiagnostic("negate does not support unsigned operands");
      }
      return validateArgs();
    }
    if (!leftPointer && !rightPointer) {
      bool useMatrixQuaternionShapeRules = false;
      if (builtinName == "plus" || builtinName == "minus") {
        const std::string leftMathType = inferMatrixQuaternionTypePath(expr.args[0], params, locals);
        const std::string rightMathType = inferMatrixQuaternionTypePath(expr.args[1], params, locals);
        if (!leftMathType.empty() || !rightMathType.empty()) {
          if (leftMathType.empty() || rightMathType.empty() || leftMathType != rightMathType) {
            return failNumericDiagnostic(builtinName +
                                         " requires matching matrix/quaternion operand types");
          }
          useMatrixQuaternionShapeRules = true;
        }
      }
      if (useMatrixQuaternionShapeRules) {
        return validateArgs();
      }
      if (builtinName == "multiply") {
        MatrixQuaternionMultiplyInfo multiplyInfo = classifyMatrixQuaternionMultiply(expr.args[0], expr.args[1]);
        if (multiplyInfo.handled) {
          if (!multiplyInfo.valid) {
            return failNumericDiagnostic(
                "multiply requires scalar scaling, matrix-vector, matrix-matrix, quaternion-quaternion, or quaternion-Vec3 operands");
          }
          return validateArgs();
        }
      }
      if (builtinName == "divide") {
        MatrixQuaternionDivideInfo divideInfo = classifyMatrixQuaternionDivide(expr.args[0], expr.args[1]);
        if (divideInfo.handled) {
          if (!divideInfo.valid) {
            return failNumericDiagnostic(
                "divide requires matrix/quaternion composite-by-scalar operands");
          }
          return validateArgs();
        }
      }
      if (!isNumericExpr(params, locals, expr.args[0]) || !isNumericExpr(params, locals, expr.args[1])) {
        return failNumericDiagnostic("arithmetic operators require numeric operands in " +
                                     currentValidationState_.context.definitionPath);
      }
      if (hasMixedNumericDomain(expr.args)) {
        return failNumericDiagnostic(
            "arithmetic operators do not support mixed software/fixed numeric operands");
      }
      if (hasMixedComplexNumeric(expr.args)) {
        return failNumericDiagnostic("arithmetic operators do not support mixed complex/real operands");
      }
      if (hasMixedSignedness(expr.args, false)) {
        return failNumericDiagnostic("arithmetic operators do not support mixed signed/unsigned operands");
      }
      if (hasMixedNumericCategory(expr.args)) {
        return failNumericDiagnostic("arithmetic operators do not support mixed int/float operands");
      }
    } else if (leftPointer && !isNumericExpr(params, locals, expr.args[1])) {
      return failNumericDiagnostic("arithmetic operators require numeric operands in " +
                                   currentValidationState_.context.definitionPath);
    }
    return validateArgs();
  }

  if (getBuiltinComparisonName(expr, builtinName)) {
    handled = true;
    size_t expectedArgs = builtinName == "not" ? 1 : 2;
    if (expr.args.size() != expectedArgs) {
      return failNumericDiagnostic("argument count mismatch for builtin " + builtinName);
    }
    const bool isBooleanOp = builtinName == "and" || builtinName == "or" || builtinName == "not";
    if (isBooleanOp) {
      for (const auto &arg : expr.args) {
        if (!isBoolExpr(arg)) {
          return failNumericDiagnostic("boolean operators require bool operands");
        }
      }
    } else {
      bool sawString = false;
      bool sawNonString = false;
      for (const auto &arg : expr.args) {
        if (isStringExpr(arg)) {
          sawString = true;
        } else {
          sawNonString = true;
        }
      }
      if (sawString) {
        if (sawNonString) {
          return failNumericDiagnostic("comparisons do not support mixed string/numeric operands");
        }
      } else {
        for (const auto &arg : expr.args) {
          if (!isComparisonOperand(params, locals, arg)) {
            return failNumericDiagnostic("comparisons require numeric, bool, or string operands");
          }
        }
        if (hasMixedNumericDomain(expr.args)) {
          return failNumericDiagnostic(
              "comparisons do not support mixed software/fixed numeric operands");
        }
        if (hasMixedComplexNumeric(expr.args)) {
          return failNumericDiagnostic("comparisons do not support mixed complex/real operands");
        }
        if (builtinName != "equal" && builtinName != "not_equal" && hasComplexNumeric(expr.args)) {
          return failNumericDiagnostic("comparisons do not support ordered complex operands");
        }
        if (hasMixedSignedness(expr.args, true)) {
          return failNumericDiagnostic("comparisons do not support mixed signed/unsigned operands");
        }
        if (hasMixedNumericCategory(expr.args)) {
          return failNumericDiagnostic("comparisons do not support mixed int/float operands");
        }
      }
    }
    return validateArgs();
  }

  if (getBuiltinClampName(expr, builtinName, allowMathBareName(expr.name))) {
    handled = true;
    if (expr.args.size() != 3) {
      return failNumericDiagnostic("argument count mismatch for builtin " + builtinName);
    }
    for (const auto &arg : expr.args) {
      if (!isNumericExpr(params, locals, arg)) {
        return failNumericDiagnostic("clamp requires numeric operands");
      }
    }
    if (hasMixedNumericDomain(expr.args)) {
      return failNumericDiagnostic("clamp does not support mixed software/fixed numeric operands");
    }
    if (hasComplexNumeric(expr.args)) {
      return failNumericDiagnostic("clamp does not support complex operands");
    }
    if (hasMixedSignedness(expr.args, false)) {
      return failNumericDiagnostic("clamp does not support mixed signed/unsigned operands");
    }
    if (hasMixedNumericCategory(expr.args)) {
      return failNumericDiagnostic("clamp does not support mixed int/float operands");
    }
    return validateArgs();
  }

  if (getBuiltinMinMaxName(expr, builtinName, allowMathBareName(expr.name))) {
    handled = true;
    if (expr.args.size() != 2) {
      return failNumericDiagnostic("argument count mismatch for builtin " + builtinName);
    }
    for (const auto &arg : expr.args) {
      if (!isNumericExpr(params, locals, arg)) {
        return failNumericDiagnostic(builtinName + " requires numeric operands");
      }
    }
    if (hasMixedNumericDomain(expr.args)) {
      return failNumericDiagnostic(builtinName +
                                   " does not support mixed software/fixed numeric operands");
    }
    if (hasComplexNumeric(expr.args)) {
      return failNumericDiagnostic(builtinName + " does not support complex operands");
    }
    if (hasMixedSignedness(expr.args, false)) {
      return failNumericDiagnostic(builtinName + " does not support mixed signed/unsigned operands");
    }
    if (hasMixedNumericCategory(expr.args)) {
      return failNumericDiagnostic(builtinName + " does not support mixed int/float operands");
    }
    return validateArgs();
  }

  if (getBuiltinAbsSignName(expr, builtinName, allowMathBareName(expr.name))) {
    handled = true;
    if (expr.args.size() != 1) {
      return failNumericDiagnostic("argument count mismatch for builtin " + builtinName);
    }
    if (!isNumericExpr(params, locals, expr.args.front())) {
      return failNumericDiagnostic(builtinName + " requires numeric operand");
    }
    if (builtinName == "sign" && hasComplexNumeric(expr.args)) {
      return failNumericDiagnostic("sign does not support complex operands");
    }
    return validateExpr(params, locals, expr.args.front());
  }

  if (getBuiltinSaturateName(expr, builtinName, allowMathBareName(expr.name))) {
    handled = true;
    if (expr.args.size() != 1) {
      return failNumericDiagnostic("argument count mismatch for builtin " + builtinName);
    }
    if (!isNumericExpr(params, locals, expr.args.front())) {
      return failNumericDiagnostic(builtinName + " requires numeric operand");
    }
    if (hasComplexNumeric(expr.args)) {
      return failNumericDiagnostic(builtinName + " does not support complex operands");
    }
    return validateExpr(params, locals, expr.args.front());
  }

  if (getBuiltinMathName(expr, builtinName, allowMathBareName(expr.name))) {
    handled = true;
    if (!expr.templateArgs.empty()) {
      return failNumericDiagnostic(builtinName + " does not accept template arguments");
    }
    if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
      return failNumericDiagnostic(builtinName + " does not accept block arguments");
    }
    size_t expectedArgs = 1;
    if (builtinName == "lerp" || builtinName == "fma") {
      expectedArgs = 3;
    } else if (builtinName == "pow" || builtinName == "atan2" || builtinName == "hypot" ||
               builtinName == "copysign") {
      expectedArgs = 2;
    }
    if (expr.args.size() != expectedArgs) {
      return failNumericDiagnostic("argument count mismatch for builtin " + builtinName);
    }
    auto validateFloatArgs = [&](size_t count) -> bool {
      for (size_t i = 0; i < count; ++i) {
        if (!isFloatExpr(params, locals, expr.args[i])) {
          return failNumericDiagnostic(builtinName + " requires float operands");
        }
      }
      return true;
    };
    if (builtinName == "lerp" || builtinName == "pow") {
      for (const auto &arg : expr.args) {
        if (!isNumericExpr(params, locals, arg)) {
          return failNumericDiagnostic(builtinName + " requires numeric operands");
        }
      }
      if (hasMixedNumericDomain(expr.args)) {
        return failNumericDiagnostic(builtinName +
                                     " does not support mixed software/fixed numeric operands");
      }
      if (hasMixedComplexNumeric(expr.args)) {
        return failNumericDiagnostic(builtinName + " does not support mixed complex/real operands");
      }
      if (hasMixedSignedness(expr.args, false)) {
        return failNumericDiagnostic(builtinName + " does not support mixed signed/unsigned operands");
      }
      if (hasMixedNumericCategory(expr.args)) {
        return failNumericDiagnostic(builtinName + " does not support mixed int/float operands");
      }
    } else if (builtinName == "is_nan" || builtinName == "is_inf" || builtinName == "is_finite") {
      if (!validateFloatArgs(1)) {
        return false;
      }
    } else if (builtinName == "fma") {
      if (!validateFloatArgs(3)) {
        return false;
      }
    } else if (builtinName == "atan2" || builtinName == "hypot" || builtinName == "copysign") {
      if (!validateFloatArgs(2)) {
        return false;
      }
    } else if (!validateFloatArgs(1)) {
      return false;
    }
    return validateArgs();
  }

  return true;
}

} // namespace primec::semantics
