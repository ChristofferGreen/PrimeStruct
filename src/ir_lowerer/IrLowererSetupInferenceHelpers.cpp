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

} // namespace primec::ir_lowerer
