#include "SemanticsValidator.h"

namespace primec::semantics {

ReturnKind SemanticsValidator::inferScalarBuiltinReturnKind(
    const Expr &expr,
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    bool &handled) {
  handled = false;
  std::string builtinName;
  std::function<ReturnKind(const Expr &)> pointerTargetKind =
      [&](const Expr &pointerExpr) -> ReturnKind {
    if (pointerExpr.kind == Expr::Kind::Name) {
      if (const BindingInfo *paramBinding =
              findParamBinding(params, pointerExpr.name)) {
        if ((paramBinding->typeName == "Pointer" ||
             paramBinding->typeName == "Reference") &&
            !paramBinding->typeTemplateArg.empty()) {
          return inferReferenceTargetKind(paramBinding->typeTemplateArg,
                                          pointerExpr.namespacePrefix);
        }
        return ReturnKind::Unknown;
      }
      if (auto it = locals.find(pointerExpr.name); it != locals.end()) {
        if ((it->second.typeName == "Pointer" ||
             it->second.typeName == "Reference") &&
            !it->second.typeTemplateArg.empty()) {
          return inferReferenceTargetKind(it->second.typeTemplateArg,
                                          pointerExpr.namespacePrefix);
        }
      }
      return ReturnKind::Unknown;
    }
    if (pointerExpr.kind == Expr::Kind::Call) {
      std::string pointerName;
      if (getBuiltinPointerName(pointerExpr, pointerName) &&
          pointerName == "location" && pointerExpr.args.size() == 1) {
        const Expr &target = pointerExpr.args.front();
        if (target.kind != Expr::Kind::Name) {
          return ReturnKind::Unknown;
        }
        if (const BindingInfo *paramBinding =
                findParamBinding(params, target.name)) {
          if (paramBinding->typeName == "Reference" &&
              !paramBinding->typeTemplateArg.empty()) {
            return inferReferenceTargetKind(paramBinding->typeTemplateArg,
                                            target.namespacePrefix);
          }
          return returnKindForTypeName(paramBinding->typeName);
        }
        if (auto it = locals.find(target.name); it != locals.end()) {
          if (it->second.typeName == "Reference" &&
              !it->second.typeTemplateArg.empty()) {
            return inferReferenceTargetKind(it->second.typeTemplateArg,
                                            target.namespacePrefix);
          }
          return returnKindForTypeName(it->second.typeName);
        }
      }
      if (getBuiltinMemoryName(pointerExpr, pointerName)) {
        if (pointerName == "alloc" && pointerExpr.templateArgs.size() == 1 &&
            pointerExpr.args.size() == 1) {
          return inferReferenceTargetKind(pointerExpr.templateArgs.front(),
                                          pointerExpr.namespacePrefix);
        }
        if (pointerName == "realloc" && pointerExpr.args.size() == 2) {
          return pointerTargetKind(pointerExpr.args.front());
        }
        if (pointerName == "at" && pointerExpr.templateArgs.empty() &&
            pointerExpr.args.size() == 3) {
          return pointerTargetKind(pointerExpr.args.front());
        }
        if (pointerName == "at_unsafe" && pointerExpr.templateArgs.empty() &&
            pointerExpr.args.size() == 2) {
          return pointerTargetKind(pointerExpr.args.front());
        }
        if (pointerName == "reinterpret" && pointerExpr.templateArgs.size() == 1 &&
            pointerExpr.args.size() == 1) {
          return inferReferenceTargetKind(pointerExpr.templateArgs.front(),
                                          pointerExpr.namespacePrefix);
        }
      }
      std::string opName;
      if (getBuiltinOperatorName(pointerExpr, opName) &&
          (opName == "plus" || opName == "minus") &&
          pointerExpr.args.size() == 2) {
        ReturnKind leftKind = pointerTargetKind(pointerExpr.args[0]);
        if (leftKind != ReturnKind::Unknown) {
          return leftKind;
        }
        return pointerTargetKind(pointerExpr.args[1]);
      }
    }
    return ReturnKind::Unknown;
  };

  if (getBuiltinPointerName(expr, builtinName) && expr.args.size() == 1) {
    handled = true;
    if (builtinName == "dereference") {
      const ReturnKind targetKind = pointerTargetKind(expr.args.front());
      if (targetKind != ReturnKind::Unknown) {
        return targetKind;
      }
    }
    return ReturnKind::Unknown;
  }

  if (getBuiltinMemoryName(expr, builtinName)) {
    handled = true;
    if (builtinName == "free") {
      return ReturnKind::Void;
    }
    return ReturnKind::Unknown;
  }

  if (getBuiltinComparisonName(expr, builtinName)) {
    handled = true;
    return ReturnKind::Bool;
  }

  if (getBuiltinMathName(expr, builtinName, allowMathBareName(expr.name))) {
    handled = true;
    if (builtinName == "is_nan" || builtinName == "is_inf" ||
        builtinName == "is_finite") {
      return ReturnKind::Bool;
    }
    if (builtinName == "lerp" && expr.args.size() == 3) {
      ReturnKind result = inferExprReturnKind(expr.args[0], params, locals);
      result = combineInferredNumericKinds(
          result, inferExprReturnKind(expr.args[1], params, locals));
      result = combineInferredNumericKinds(
          result, inferExprReturnKind(expr.args[2], params, locals));
      return result;
    }
    if (builtinName == "pow" && expr.args.size() == 2) {
      ReturnKind result = inferExprReturnKind(expr.args[0], params, locals);
      result = combineInferredNumericKinds(
          result, inferExprReturnKind(expr.args[1], params, locals));
      return result;
    }
    if (expr.args.empty()) {
      return ReturnKind::Unknown;
    }
    const ReturnKind argKind = inferExprReturnKind(expr.args.front(), params, locals);
    if (argKind == ReturnKind::Float64) {
      return ReturnKind::Float64;
    }
    if (argKind == ReturnKind::Float32) {
      return ReturnKind::Float32;
    }
    return ReturnKind::Unknown;
  }

  if (getBuiltinOperatorName(expr, builtinName)) {
    handled = true;
    if (builtinName == "negate") {
      if (expr.args.size() != 1) {
        return ReturnKind::Unknown;
      }
      const ReturnKind argKind =
          inferExprReturnKind(expr.args.front(), params, locals);
      if (argKind == ReturnKind::Bool || argKind == ReturnKind::Void) {
        return ReturnKind::Unknown;
      }
      return argKind;
    }
    if (expr.args.size() != 2) {
      return ReturnKind::Unknown;
    }
    if ((builtinName == "plus" || builtinName == "minus" ||
         builtinName == "multiply" || builtinName == "divide") &&
        !inferStructReturnPath(expr, params, locals).empty()) {
      return ReturnKind::Array;
    }
    const ReturnKind left = inferExprReturnKind(expr.args[0], params, locals);
    const ReturnKind right = inferExprReturnKind(expr.args[1], params, locals);
    return combineInferredNumericKinds(left, right);
  }

  if (getBuiltinClampName(expr, builtinName, allowMathBareName(expr.name))) {
    handled = true;
    if (expr.args.size() != 3) {
      return ReturnKind::Unknown;
    }
    ReturnKind result = inferExprReturnKind(expr.args[0], params, locals);
    result = combineInferredNumericKinds(
        result, inferExprReturnKind(expr.args[1], params, locals));
    result = combineInferredNumericKinds(
        result, inferExprReturnKind(expr.args[2], params, locals));
    return result;
  }

  if (getBuiltinMinMaxName(expr, builtinName, allowMathBareName(expr.name))) {
    handled = true;
    if (expr.args.size() != 2) {
      return ReturnKind::Unknown;
    }
    ReturnKind result = inferExprReturnKind(expr.args[0], params, locals);
    result = combineInferredNumericKinds(
        result, inferExprReturnKind(expr.args[1], params, locals));
    return result;
  }

  if (getBuiltinAbsSignName(expr, builtinName, allowMathBareName(expr.name))) {
    handled = true;
    if (expr.args.size() != 1) {
      return ReturnKind::Unknown;
    }
    const ReturnKind argKind =
        inferExprReturnKind(expr.args.front(), params, locals);
    if (argKind == ReturnKind::Bool || argKind == ReturnKind::Void) {
      return ReturnKind::Unknown;
    }
    return argKind;
  }

  if (getBuiltinSaturateName(expr, builtinName, allowMathBareName(expr.name))) {
    handled = true;
    if (expr.args.size() != 1) {
      return ReturnKind::Unknown;
    }
    const ReturnKind argKind =
        inferExprReturnKind(expr.args.front(), params, locals);
    if (argKind == ReturnKind::Bool || argKind == ReturnKind::Void) {
      return ReturnKind::Unknown;
    }
    return argKind;
  }

  if (getBuiltinConvertName(expr, builtinName)) {
    handled = true;
    if (expr.templateArgs.size() != 1) {
      return ReturnKind::Unknown;
    }
    return returnKindForTypeName(expr.templateArgs.front());
  }

  if (getBuiltinMutationName(expr, builtinName)) {
    handled = true;
    if (expr.args.size() != 1) {
      return ReturnKind::Unknown;
    }
    return inferExprReturnKind(expr.args.front(), params, locals);
  }

  if (isSimpleCallName(expr, "move")) {
    handled = true;
    if (expr.args.size() != 1) {
      return ReturnKind::Unknown;
    }
    return inferExprReturnKind(expr.args.front(), params, locals);
  }

  if (isAssignCall(expr)) {
    handled = true;
    if (expr.args.size() != 2) {
      return ReturnKind::Unknown;
    }
    return inferExprReturnKind(expr.args[1], params, locals);
  }

  return ReturnKind::Unknown;
}

} // namespace primec::semantics
