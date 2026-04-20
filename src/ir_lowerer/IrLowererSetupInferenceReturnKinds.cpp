#include "IrLowererSetupInferenceHelpers.h"

#include "IrLowererHelpers.h"
#include "IrLowererSetupTypeHelpers.h"

namespace primec::ir_lowerer {

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

  if (isSimpleCallName(expr, "dereference")) {
    if (expr.args.size() != 1) {
      return NonMathScalarCallReturnKindResolution::Resolved;
    }
    kindOut = inferPointerTargetKind(expr.args.front(), localsIn);
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

ControlFlowCallReturnKindResolution inferControlFlowCallReturnKind(
    const Expr &expr,
    const LocalMap &localsIn,
    const ResolveSetupInferenceExprPathFn &resolveExprPath,
    const LowerSetupInferenceMatchToIfFn &lowerMatchToIf,
    const InferSetupInferenceValueKindFn &inferExprKind,
    const SetupInferenceCombineNumericKindsFn &combineNumericKinds,
    const InferSetupInferenceBodyValueKindFn &inferBodyValueKind,
    const IsSetupInferenceKnownDefinitionPathFn &isKnownDefinitionPath,
    std::string &error,
    LocalInfo::ValueKind &kindOut) {
  kindOut = LocalInfo::ValueKind::Unknown;

  if (isMatchCall(expr)) {
    Expr expanded;
    if (!lowerMatchToIf(expr, expanded, error)) {
      return ControlFlowCallReturnKindResolution::Resolved;
    }
    kindOut = inferExprKind(expanded, localsIn);
    return ControlFlowCallReturnKindResolution::Resolved;
  }

  auto isIfBlockEnvelope = [&](const Expr &candidate) -> bool {
    if (candidate.kind != Expr::Kind::Call || candidate.isBinding || candidate.isMethodCall) {
      return false;
    }
    if (!candidate.args.empty() || !candidate.templateArgs.empty() || hasNamedArguments(candidate.argNames)) {
      return false;
    }
    if (!candidate.hasBodyArguments && candidate.bodyArguments.empty()) {
      return false;
    }
    return true;
  };
  auto inferBranchValueKind = [&](const Expr &candidate, const LocalMap &localsBase) -> LocalInfo::ValueKind {
    if (!isIfBlockEnvelope(candidate)) {
      return inferExprKind(candidate, localsBase);
    }
    return inferBodyValueKind(candidate.bodyArguments, localsBase);
  };

  if (isIfCall(expr)) {
    if (expr.args.size() != 3) {
      return ControlFlowCallReturnKindResolution::Resolved;
    }
    LocalInfo::ValueKind thenKind = inferBranchValueKind(expr.args[1], localsIn);
    LocalInfo::ValueKind elseKind = inferBranchValueKind(expr.args[2], localsIn);
    if (thenKind == elseKind) {
      kindOut = thenKind;
    } else {
      kindOut = combineNumericKinds(thenKind, elseKind);
    }
    return ControlFlowCallReturnKindResolution::Resolved;
  }

  if (isBlockCall(expr)) {
    if (expr.hasBodyArguments) {
      const std::string resolved = resolveExprPath(expr);
      if (!isKnownDefinitionPath(resolved) && expr.args.empty() && expr.templateArgs.empty() &&
          !hasNamedArguments(expr.argNames)) {
        kindOut = inferBodyValueKind(expr.bodyArguments, localsIn);
      }
    }
    return ControlFlowCallReturnKindResolution::Resolved;
  }

  return ControlFlowCallReturnKindResolution::NotMatched;
}

PointerBuiltinCallReturnKindResolution inferPointerBuiltinCallReturnKind(
    const Expr &expr,
    const LocalMap &localsIn,
    const InferSetupInferenceValueKindFn &inferPointerTargetKind,
    LocalInfo::ValueKind &kindOut) {
  kindOut = LocalInfo::ValueKind::Unknown;

  std::string builtin;
  if (!getBuiltinPointerName(expr, builtin)) {
    std::string memoryBuiltin;
    if (!getBuiltinMemoryName(expr, memoryBuiltin) ||
        (memoryBuiltin != "alloc" && memoryBuiltin != "realloc" && memoryBuiltin != "at" &&
         memoryBuiltin != "at_unsafe")) {
      return PointerBuiltinCallReturnKindResolution::NotMatched;
    }
    if (memoryBuiltin == "alloc" && expr.templateArgs.size() == 1) {
      kindOut = valueKindFromTypeName(expr.templateArgs.front());
    }
    if (memoryBuiltin == "realloc" && expr.args.size() == 2) {
      kindOut = inferPointerTargetKind(expr.args.front(), localsIn);
    }
    if (memoryBuiltin == "at" && expr.args.size() == 3) {
      kindOut = inferPointerTargetKind(expr.args.front(), localsIn);
    }
    if (memoryBuiltin == "at_unsafe" && expr.args.size() == 2) {
      kindOut = inferPointerTargetKind(expr.args.front(), localsIn);
    }
    return PointerBuiltinCallReturnKindResolution::Resolved;
  }

  if (builtin == "dereference") {
    if (expr.args.size() != 1) {
      return PointerBuiltinCallReturnKindResolution::Resolved;
    }
    kindOut = inferPointerTargetKind(expr.args.front(), localsIn);
    return PointerBuiltinCallReturnKindResolution::Resolved;
  }

  return PointerBuiltinCallReturnKindResolution::Resolved;
}

ComparisonOperatorCallReturnKindResolution inferComparisonOperatorCallReturnKind(
    const Expr &expr,
    const LocalMap &localsIn,
    const InferSetupInferenceValueKindFn &inferExprKind,
    const SetupInferenceCombineNumericKindsFn &combineNumericKinds,
    LocalInfo::ValueKind &kindOut) {
  kindOut = LocalInfo::ValueKind::Unknown;

  std::string builtin;
  if (getBuiltinComparisonName(expr, builtin)) {
    kindOut = LocalInfo::ValueKind::Bool;
    return ComparisonOperatorCallReturnKindResolution::Resolved;
  }

  if (!getBuiltinOperatorName(expr, builtin)) {
    return ComparisonOperatorCallReturnKindResolution::NotMatched;
  }

  if (builtin == "negate") {
    if (expr.args.size() != 1) {
      return ComparisonOperatorCallReturnKindResolution::Resolved;
    }
    kindOut = inferExprKind(expr.args.front(), localsIn);
    return ComparisonOperatorCallReturnKindResolution::Resolved;
  }

  if (expr.args.size() != 2) {
    return ComparisonOperatorCallReturnKindResolution::Resolved;
  }
  auto left = inferExprKind(expr.args[0], localsIn);
  auto right = inferExprKind(expr.args[1], localsIn);
  kindOut = combineNumericKinds(left, right);
  return ComparisonOperatorCallReturnKindResolution::Resolved;
}

GpuBufferCallReturnKindResolution inferGpuBufferCallReturnKind(
    const Expr &expr,
    const LocalMap &localsIn,
    const InferSetupInferenceValueKindFn &inferBufferElementKind,
    LocalInfo::ValueKind &kindOut) {
  kindOut = LocalInfo::ValueKind::Unknown;

  std::string gpuBuiltin;
  if (getBuiltinGpuName(expr, gpuBuiltin)) {
    kindOut = LocalInfo::ValueKind::Int32;
    return GpuBufferCallReturnKindResolution::Resolved;
  }

  if (!expr.isMethodCall && isSimpleCallName(expr, "buffer_load")) {
    if (expr.args.size() != 2) {
      return GpuBufferCallReturnKindResolution::Resolved;
    }
    LocalInfo::ValueKind elemKind = inferBufferElementKind(expr.args.front(), localsIn);
    if (elemKind != LocalInfo::ValueKind::Unknown && elemKind != LocalInfo::ValueKind::String) {
      kindOut = elemKind;
    }
    return GpuBufferCallReturnKindResolution::Resolved;
  }

  return GpuBufferCallReturnKindResolution::NotMatched;
}

CountCapacityCallReturnKindResolution inferCountCapacityCallReturnKind(
    const Expr &expr,
    const LocalMap &localsIn,
    const IsSetupInferenceMethodCountLikeCallFn &isArrayCountCall,
    const IsSetupInferenceMethodCountLikeCallFn &isStringCountCall,
    const IsSetupInferenceMethodCountLikeCallFn &isVectorCapacityCall,
    LocalInfo::ValueKind &kindOut) {
  kindOut = LocalInfo::ValueKind::Unknown;

  if (isArrayCountCall(expr, localsIn) || isStringCountCall(expr, localsIn) || isVectorCapacityCall(expr, localsIn)) {
    kindOut = LocalInfo::ValueKind::Int32;
    return CountCapacityCallReturnKindResolution::Resolved;
  }

  return CountCapacityCallReturnKindResolution::NotMatched;
}

} // namespace primec::ir_lowerer
