#include "IrLowererSetupInferenceHelpers.h"

#include "IrLowererHelpers.h"

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

} // namespace primec::ir_lowerer
