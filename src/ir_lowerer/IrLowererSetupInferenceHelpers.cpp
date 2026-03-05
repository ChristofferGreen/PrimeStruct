#include "IrLowererSetupInferenceHelpers.h"

#include "IrLowererHelpers.h"
#include "IrLowererSetupTypeHelpers.h"

namespace primec::ir_lowerer {
namespace {

bool isPointerExpression(const Expr &expr,
                         const LocalMap &localsIn,
                         const GetSetupInferenceBuiltinOperatorNameFn &getBuiltinOperatorName) {
  if (expr.kind == Expr::Kind::Name) {
    auto it = localsIn.find(expr.name);
    return it != localsIn.end() && it->second.kind == LocalInfo::Kind::Pointer;
  }
  if (expr.kind == Expr::Kind::Call && isSimpleCallName(expr, "location")) {
    return true;
  }
  if (expr.kind == Expr::Kind::Call) {
    std::string builtinName;
    if (getBuiltinOperatorName(expr, builtinName) &&
        (builtinName == "plus" || builtinName == "minus") &&
        expr.args.size() == 2) {
      return isPointerExpression(expr.args[0], localsIn, getBuiltinOperatorName) &&
             !isPointerExpression(expr.args[1], localsIn, getBuiltinOperatorName);
    }
  }
  return false;
}

} // namespace

LocalInfo::ValueKind inferPointerTargetValueKind(
    const Expr &expr,
    const LocalMap &localsIn,
    const GetSetupInferenceBuiltinOperatorNameFn &getBuiltinOperatorName) {
  if (expr.kind == Expr::Kind::Name) {
    auto it = localsIn.find(expr.name);
    if (it == localsIn.end()) {
      return LocalInfo::ValueKind::Unknown;
    }
    if (it->second.kind == LocalInfo::Kind::Pointer || it->second.kind == LocalInfo::Kind::Reference) {
      if (it->second.kind == LocalInfo::Kind::Reference && it->second.referenceToArray) {
        return LocalInfo::ValueKind::Unknown;
      }
      return it->second.valueKind;
    }
    return LocalInfo::ValueKind::Unknown;
  }
  if (expr.kind == Expr::Kind::Call) {
    if (isSimpleCallName(expr, "location") && expr.args.size() == 1) {
      const Expr &target = expr.args.front();
      if (target.kind == Expr::Kind::Name) {
        auto it = localsIn.find(target.name);
        if (it != localsIn.end()) {
          if (it->second.kind == LocalInfo::Kind::Reference && it->second.referenceToArray) {
            return LocalInfo::ValueKind::Unknown;
          }
          return it->second.valueKind;
        }
      }
      return LocalInfo::ValueKind::Unknown;
    }
    std::string builtinName;
    if (getBuiltinOperatorName(expr, builtinName) &&
        (builtinName == "plus" || builtinName == "minus") &&
        expr.args.size() == 2 &&
        isPointerExpression(expr.args[0], localsIn, getBuiltinOperatorName) &&
        !isPointerExpression(expr.args[1], localsIn, getBuiltinOperatorName)) {
      return inferPointerTargetValueKind(expr.args[0], localsIn, getBuiltinOperatorName);
    }
  }
  return LocalInfo::ValueKind::Unknown;
}

LocalInfo::ValueKind inferBufferElementValueKind(
    const Expr &expr,
    const LocalMap &localsIn,
    const InferSetupInferenceValueKindFn &inferArrayElementKind) {
  if (expr.kind == Expr::Kind::Name) {
    auto it = localsIn.find(expr.name);
    if (it != localsIn.end() && it->second.kind == LocalInfo::Kind::Buffer) {
      return it->second.valueKind;
    }
    return LocalInfo::ValueKind::Unknown;
  }
  if (expr.kind == Expr::Kind::Call) {
    if (isSimpleCallName(expr, "buffer") && expr.templateArgs.size() == 1) {
      return valueKindFromTypeName(expr.templateArgs.front());
    }
    if (isSimpleCallName(expr, "upload") && expr.args.size() == 1) {
      return inferArrayElementKind(expr.args.front(), localsIn);
    }
  }
  return LocalInfo::ValueKind::Unknown;
}

LocalInfo::ValueKind inferArrayElementValueKind(
    const Expr &expr,
    const LocalMap &localsIn,
    const InferSetupInferenceValueKindFn &inferBufferElementKind,
    const ResolveSetupInferenceExprPathFn &resolveExprPath,
    const ResolveSetupInferenceArrayElementKindByPathFn &resolveStructArrayElementKindByPath,
    const ResolveSetupInferenceArrayReturnKindFn &resolveDirectCallArrayReturnKind,
    const ResolveSetupInferenceArrayReturnKindFn &resolveCountMethodArrayReturnKind,
    const ResolveSetupInferenceArrayReturnKindFn &resolveMethodCallArrayReturnKind) {
  if (expr.kind == Expr::Kind::Name) {
    auto it = localsIn.find(expr.name);
    if (it != localsIn.end()) {
      if (it->second.kind == LocalInfo::Kind::Array || it->second.kind == LocalInfo::Kind::Buffer ||
          (it->second.kind == LocalInfo::Kind::Reference && it->second.referenceToArray)) {
        return it->second.valueKind;
      }
    }
    return LocalInfo::ValueKind::Unknown;
  }
  if (expr.kind == Expr::Kind::Call) {
    if (!expr.isMethodCall && isSimpleCallName(expr, "readback") && expr.args.size() == 1) {
      LocalInfo::ValueKind bufferKind = inferBufferElementKind(expr.args.front(), localsIn);
      if (bufferKind != LocalInfo::ValueKind::Unknown && bufferKind != LocalInfo::ValueKind::String) {
        return bufferKind;
      }
    }

    std::string collection;
    if (getBuiltinCollectionName(expr, collection) && collection == "array" && expr.templateArgs.size() == 1) {
      return valueKindFromTypeName(expr.templateArgs.front());
    }

    if (!expr.isMethodCall) {
      LocalInfo::ValueKind structArrayElementKind = LocalInfo::ValueKind::Unknown;
      if (resolveStructArrayElementKindByPath(resolveExprPath(expr), structArrayElementKind)) {
        return structArrayElementKind;
      }
    }

    LocalInfo::ValueKind returnKind = LocalInfo::ValueKind::Unknown;
    if (!expr.isMethodCall) {
      if (resolveDirectCallArrayReturnKind(expr, localsIn, returnKind)) {
        return returnKind;
      }
      if (resolveCountMethodArrayReturnKind(expr, localsIn, returnKind)) {
        return returnKind;
      }
    } else if (resolveMethodCallArrayReturnKind(expr, localsIn, returnKind)) {
      return returnKind;
    }
  }
  return LocalInfo::ValueKind::Unknown;
}

CallExpressionReturnKindResolution resolveCallExpressionReturnKind(
    const Expr &expr,
    const LocalMap &localsIn,
    const ResolveSetupInferenceCallReturnKindFn &resolveDefinitionCallReturnKind,
    const ResolveSetupInferenceCallReturnKindFn &resolveCountMethodCallReturnKind,
    const ResolveSetupInferenceCallReturnKindFn &resolveMethodCallReturnKind,
    LocalInfo::ValueKind &kindOut) {
  kindOut = LocalInfo::ValueKind::Unknown;
  LocalInfo::ValueKind returnKind = LocalInfo::ValueKind::Unknown;
  bool matched = false;

  if (!expr.isMethodCall) {
    if (resolveDefinitionCallReturnKind(expr, localsIn, returnKind, matched)) {
      kindOut = returnKind;
      return CallExpressionReturnKindResolution::Resolved;
    }
    if (matched) {
      return CallExpressionReturnKindResolution::MatchedButUnsupported;
    }

    matched = false;
    if (resolveCountMethodCallReturnKind(expr, localsIn, returnKind, matched)) {
      kindOut = returnKind;
      return CallExpressionReturnKindResolution::Resolved;
    }
    if (matched) {
      return CallExpressionReturnKindResolution::MatchedButUnsupported;
    }

    return CallExpressionReturnKindResolution::NotResolved;
  }

  if (resolveMethodCallReturnKind(expr, localsIn, returnKind, matched)) {
    kindOut = returnKind;
    return CallExpressionReturnKindResolution::Resolved;
  }
  if (matched) {
    return CallExpressionReturnKindResolution::MatchedButUnsupported;
  }
  return CallExpressionReturnKindResolution::NotResolved;
}

ArrayMapAccessElementKindResolution resolveArrayMapAccessElementKind(
    const Expr &expr,
    const LocalMap &localsIn,
    const IsSetupInferenceEntryArgsNameFn &isEntryArgsName,
    LocalInfo::ValueKind &kindOut) {
  kindOut = LocalInfo::ValueKind::Unknown;

  std::string accessName;
  if (!getBuiltinArrayAccessName(expr, accessName)) {
    return ArrayMapAccessElementKindResolution::NotMatched;
  }
  if (expr.args.size() != 2) {
    return ArrayMapAccessElementKindResolution::Resolved;
  }

  const Expr &target = expr.args.front();
  if (target.kind == Expr::Kind::StringLiteral) {
    kindOut = LocalInfo::ValueKind::Int32;
    return ArrayMapAccessElementKindResolution::Resolved;
  }
  if (target.kind == Expr::Kind::Name) {
    auto it = localsIn.find(target.name);
    if (it != localsIn.end() && it->second.valueKind == LocalInfo::ValueKind::String) {
      kindOut = LocalInfo::ValueKind::Int32;
      return ArrayMapAccessElementKindResolution::Resolved;
    }
  }
  if (isEntryArgsName(target, localsIn)) {
    return ArrayMapAccessElementKindResolution::Resolved;
  }

  if (target.kind == Expr::Kind::Name) {
    auto it = localsIn.find(target.name);
    if (it != localsIn.end() && it->second.kind == LocalInfo::Kind::Map &&
        it->second.mapValueKind != LocalInfo::ValueKind::Unknown &&
        it->second.mapValueKind != LocalInfo::ValueKind::String) {
      kindOut = it->second.mapValueKind;
      return ArrayMapAccessElementKindResolution::Resolved;
    }
  } else if (target.kind == Expr::Kind::Call) {
    std::string collection;
    if (getBuiltinCollectionName(target, collection) && collection == "map" && target.templateArgs.size() == 2) {
      const LocalInfo::ValueKind valueKind = valueKindFromTypeName(target.templateArgs[1]);
      if (valueKind != LocalInfo::ValueKind::Unknown && valueKind != LocalInfo::ValueKind::String) {
        kindOut = valueKind;
        return ArrayMapAccessElementKindResolution::Resolved;
      }
    }
  }

  LocalInfo::ValueKind elementKind = LocalInfo::ValueKind::Unknown;
  if (target.kind == Expr::Kind::Name) {
    auto it = localsIn.find(target.name);
    if (it != localsIn.end()) {
      if (it->second.kind == LocalInfo::Kind::Array || it->second.kind == LocalInfo::Kind::Vector) {
        elementKind = it->second.valueKind;
      } else if (it->second.kind == LocalInfo::Kind::Reference && it->second.referenceToArray) {
        elementKind = it->second.valueKind;
      }
    }
  } else if (target.kind == Expr::Kind::Call) {
    std::string collection;
    if (getBuiltinCollectionName(target, collection) && (collection == "array" || collection == "vector") &&
        target.templateArgs.size() == 1) {
      elementKind = valueKindFromTypeName(target.templateArgs.front());
    }
  }
  if (elementKind != LocalInfo::ValueKind::Unknown && elementKind != LocalInfo::ValueKind::String) {
    kindOut = elementKind;
  }
  return ArrayMapAccessElementKindResolution::Resolved;
}

LocalInfo::ValueKind inferBodyValueKindWithLocalsScaffolding(
    const std::vector<Expr> &bodyExpressions,
    const LocalMap &localsBase,
    const InferSetupInferenceValueKindFn &inferExprKind,
    const IsSetupInferenceBindingMutableFn &isBindingMutable,
    const SetupInferenceBindingKindFn &bindingKind,
    const HasSetupInferenceExplicitBindingTypeTransformFn &hasExplicitBindingTypeTransform,
    const SetupInferenceBindingValueKindFn &bindingValueKind,
    const ApplySetupInferenceStructInfoFn &applyStructArrayInfo,
    const ApplySetupInferenceStructInfoFn &applyStructValueInfo,
    const InferSetupInferenceStructExprPathFn &inferStructExprPath) {
  LocalMap bodyLocals = localsBase;
  bool sawValue = false;
  LocalInfo::ValueKind lastKind = LocalInfo::ValueKind::Unknown;
  for (const auto &bodyExpr : bodyExpressions) {
    if (bodyExpr.isBinding) {
      if (bodyExpr.args.size() != 1) {
        return LocalInfo::ValueKind::Unknown;
      }
      LocalInfo info;
      info.index = 0;
      info.isMutable = isBindingMutable(bodyExpr);
      info.kind = bindingKind(bodyExpr);
      LocalInfo::ValueKind valueKind = LocalInfo::ValueKind::Unknown;
      if (hasExplicitBindingTypeTransform(bodyExpr)) {
        valueKind = bindingValueKind(bodyExpr, info.kind);
      } else if (bodyExpr.args.size() == 1 && info.kind == LocalInfo::Kind::Value) {
        valueKind = inferExprKind(bodyExpr.args.front(), bodyLocals);
        if (valueKind == LocalInfo::ValueKind::Unknown) {
          valueKind = LocalInfo::ValueKind::Int32;
        }
      }
      info.valueKind = valueKind;
      applyStructArrayInfo(bodyExpr, info);
      applyStructValueInfo(bodyExpr, info);
      if (info.structTypeName.empty() && info.kind == LocalInfo::Kind::Value &&
          info.valueKind == LocalInfo::ValueKind::Unknown) {
        std::string inferredStruct = inferStructExprPath(bodyExpr.args.front(), bodyLocals);
        if (!inferredStruct.empty()) {
          info.structTypeName = inferredStruct;
        }
      }
      bodyLocals.emplace(bodyExpr.name, info);
      continue;
    }
    if (isReturnCall(bodyExpr)) {
      if (bodyExpr.args.size() != 1) {
        return LocalInfo::ValueKind::Unknown;
      }
      return inferExprKind(bodyExpr.args.front(), bodyLocals);
    }
    sawValue = true;
    lastKind = inferExprKind(bodyExpr, bodyLocals);
  }
  return sawValue ? lastKind : LocalInfo::ValueKind::Unknown;
}

MathBuiltinReturnKindResolution inferMathBuiltinReturnKind(
    const Expr &expr,
    const LocalMap &localsIn,
    bool hasMathImport,
    const InferSetupInferenceValueKindFn &inferExprKind,
    const SetupInferenceCombineNumericKindsFn &combineNumericKinds,
    LocalInfo::ValueKind &kindOut) {
  kindOut = LocalInfo::ValueKind::Unknown;
  std::string builtin;

  if (getBuiltinClampName(expr, hasMathImport)) {
    if (expr.args.size() != 3) {
      return MathBuiltinReturnKindResolution::Resolved;
    }
    auto first = inferExprKind(expr.args[0], localsIn);
    auto second = inferExprKind(expr.args[1], localsIn);
    auto third = inferExprKind(expr.args[2], localsIn);
    kindOut = combineNumericKinds(combineNumericKinds(first, second), third);
    return MathBuiltinReturnKindResolution::Resolved;
  }
  if (getBuiltinMinMaxName(expr, builtin, hasMathImport) ||
      getBuiltinPowName(expr, builtin, hasMathImport) ||
      getBuiltinHypotName(expr, builtin, hasMathImport) ||
      getBuiltinCopysignName(expr, builtin, hasMathImport) ||
      getBuiltinTrig2Name(expr, builtin, hasMathImport)) {
    if (expr.args.size() != 2) {
      return MathBuiltinReturnKindResolution::Resolved;
    }
    auto left = inferExprKind(expr.args[0], localsIn);
    auto right = inferExprKind(expr.args[1], localsIn);
    kindOut = combineNumericKinds(left, right);
    return MathBuiltinReturnKindResolution::Resolved;
  }
  if (getBuiltinLerpName(expr, builtin, hasMathImport) ||
      getBuiltinFmaName(expr, builtin, hasMathImport)) {
    if (expr.args.size() != 3) {
      return MathBuiltinReturnKindResolution::Resolved;
    }
    auto first = inferExprKind(expr.args[0], localsIn);
    auto second = inferExprKind(expr.args[1], localsIn);
    auto third = inferExprKind(expr.args[2], localsIn);
    kindOut = combineNumericKinds(combineNumericKinds(first, second), third);
    return MathBuiltinReturnKindResolution::Resolved;
  }
  if (getBuiltinMathPredicateName(expr, builtin, hasMathImport)) {
    if (expr.args.size() != 1) {
      return MathBuiltinReturnKindResolution::Resolved;
    }
    kindOut = LocalInfo::ValueKind::Bool;
    return MathBuiltinReturnKindResolution::Resolved;
  }
  if (getBuiltinAngleName(expr, builtin, hasMathImport) ||
      getBuiltinTrigName(expr, builtin, hasMathImport) ||
      getBuiltinArcTrigName(expr, builtin, hasMathImport) ||
      getBuiltinHyperbolicName(expr, builtin, hasMathImport) ||
      getBuiltinArcHyperbolicName(expr, builtin, hasMathImport) ||
      getBuiltinExpName(expr, builtin, hasMathImport) ||
      getBuiltinLogName(expr, builtin, hasMathImport) ||
      getBuiltinAbsSignName(expr, builtin, hasMathImport) ||
      getBuiltinSaturateName(expr, builtin, hasMathImport) ||
      getBuiltinRoundingName(expr, builtin, hasMathImport) ||
      getBuiltinRootName(expr, builtin, hasMathImport)) {
    if (expr.args.size() != 1) {
      return MathBuiltinReturnKindResolution::Resolved;
    }
    kindOut = inferExprKind(expr.args.front(), localsIn);
    return MathBuiltinReturnKindResolution::Resolved;
  }

  return MathBuiltinReturnKindResolution::NotMatched;
}

NonMathScalarCallReturnKindResolution inferNonMathScalarCallReturnKind(
    const Expr &expr,
    const LocalMap &localsIn,
    const InferSetupInferenceValueKindFn &inferExprKind,
    const InferSetupInferenceValueKindFn &inferPointerTargetKind,
    LocalInfo::ValueKind &kindOut) {
  kindOut = LocalInfo::ValueKind::Unknown;

  if (getBuiltinConvertName(expr)) {
    if (expr.templateArgs.size() != 1) {
      return NonMathScalarCallReturnKindResolution::Resolved;
    }
    kindOut = valueKindFromTypeName(expr.templateArgs.front());
    return NonMathScalarCallReturnKindResolution::Resolved;
  }

  if (isSimpleCallName(expr, "increment") || isSimpleCallName(expr, "decrement") ||
      isSimpleCallName(expr, "move")) {
    if (expr.args.size() != 1) {
      return NonMathScalarCallReturnKindResolution::Resolved;
    }
    kindOut = inferExprKind(expr.args.front(), localsIn);
    return NonMathScalarCallReturnKindResolution::Resolved;
  }

  if (isSimpleCallName(expr, "assign")) {
    if (expr.args.size() != 2) {
      return NonMathScalarCallReturnKindResolution::Resolved;
    }
    const Expr &target = expr.args.front();
    if (target.kind == Expr::Kind::Name) {
      auto it = localsIn.find(target.name);
      if (it == localsIn.end()) {
        return NonMathScalarCallReturnKindResolution::Resolved;
      }
      if (it->second.kind != LocalInfo::Kind::Value && it->second.kind != LocalInfo::Kind::Reference) {
        return NonMathScalarCallReturnKindResolution::Resolved;
      }
      kindOut = it->second.valueKind;
      return NonMathScalarCallReturnKindResolution::Resolved;
    }
    if (target.kind == Expr::Kind::Call && isSimpleCallName(target, "dereference") && target.args.size() == 1) {
      kindOut = inferPointerTargetKind(target.args.front(), localsIn);
    }
    return NonMathScalarCallReturnKindResolution::Resolved;
  }

  return NonMathScalarCallReturnKindResolution::NotMatched;
}

} // namespace primec::ir_lowerer
