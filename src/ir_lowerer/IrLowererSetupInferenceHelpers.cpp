#include "IrLowererSetupInferenceHelpers.h"

#include <algorithm>

#include "IrLowererHelpers.h"
#include "IrLowererSetupTypeHelpers.h"
#include "IrLowererStatementBindingHelpers.h"

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
    std::string memoryBuiltinName;
    if (getBuiltinMemoryName(expr, memoryBuiltinName)) {
      if (memoryBuiltinName == "alloc" && expr.templateArgs.size() == 1) {
        return true;
      }
      if (memoryBuiltinName == "realloc" && expr.args.size() == 2) {
        return isPointerExpression(expr.args.front(), localsIn, getBuiltinOperatorName);
      }
      if (memoryBuiltinName == "at" && expr.args.size() == 3) {
        return isPointerExpression(expr.args.front(), localsIn, getBuiltinOperatorName);
      }
      if (memoryBuiltinName == "at_unsafe" && expr.args.size() == 2) {
        return isPointerExpression(expr.args.front(), localsIn, getBuiltinOperatorName);
      }
    }
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
      if (it->second.referenceToMap || it->second.pointerToMap) {
        return it->second.mapValueKind;
      }
      if (it->second.referenceToArray || it->second.referenceToVector ||
          it->second.pointerToArray || it->second.pointerToVector) {
        return it->second.valueKind;
      }
      return it->second.valueKind;
    }
    return LocalInfo::ValueKind::Unknown;
  }
  if (expr.kind == Expr::Kind::Call) {
    std::string accessName;
    if (getBuiltinArrayAccessName(expr, accessName) && !accessName.empty() && expr.args.size() == 2 &&
        expr.args.front().kind == Expr::Kind::Name) {
      auto it = localsIn.find(expr.args.front().name);
      if (it != localsIn.end() && it->second.isArgsPack &&
          (it->second.argsPackElementKind == LocalInfo::Kind::Pointer ||
           it->second.argsPackElementKind == LocalInfo::Kind::Reference)) {
        if (it->second.referenceToMap || it->second.pointerToMap) {
          return it->second.mapValueKind;
        }
        return it->second.valueKind;
      }
    }
    if (isSimpleCallName(expr, "location") && expr.args.size() == 1) {
      const Expr &target = expr.args.front();
      if (target.kind == Expr::Kind::Name) {
        auto it = localsIn.find(target.name);
        if (it != localsIn.end()) {
          if (it->second.referenceToMap || it->second.pointerToMap) {
            return it->second.mapValueKind;
          }
          if (it->second.referenceToArray || it->second.referenceToVector ||
              it->second.pointerToArray || it->second.pointerToVector) {
            return it->second.valueKind;
          }
          return it->second.valueKind;
        }
      }
      return LocalInfo::ValueKind::Unknown;
    }
    std::string memoryBuiltinName;
    if (getBuiltinMemoryName(expr, memoryBuiltinName)) {
      if (memoryBuiltinName == "alloc" && expr.templateArgs.size() == 1) {
        return valueKindFromTypeName(expr.templateArgs.front());
      }
      if (memoryBuiltinName == "realloc" && expr.args.size() == 2) {
        return inferPointerTargetValueKind(expr.args.front(), localsIn, getBuiltinOperatorName);
      }
      if (memoryBuiltinName == "at" && expr.args.size() == 3) {
        return inferPointerTargetValueKind(expr.args.front(), localsIn, getBuiltinOperatorName);
      }
      if (memoryBuiltinName == "at_unsafe" && expr.args.size() == 2) {
        return inferPointerTargetValueKind(expr.args.front(), localsIn, getBuiltinOperatorName);
      }
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
    std::string accessName;
    if (getBuiltinArrayAccessName(expr, accessName) && expr.args.size() == 2 &&
        expr.args.front().kind == Expr::Kind::Name) {
      auto it = localsIn.find(expr.args.front().name);
      if (it != localsIn.end() && it->second.isArgsPack &&
          it->second.argsPackElementKind == LocalInfo::Kind::Buffer) {
        return it->second.valueKind;
      }
    }
    if (isSimpleCallName(expr, "dereference") && expr.args.size() == 1) {
      const Expr &targetExpr = expr.args.front();
      if (targetExpr.kind == Expr::Kind::Name) {
        auto it = localsIn.find(targetExpr.name);
        if (it != localsIn.end() &&
            ((it->second.kind == LocalInfo::Kind::Reference && it->second.referenceToBuffer) ||
             (it->second.kind == LocalInfo::Kind::Pointer && it->second.pointerToBuffer))) {
          return it->second.valueKind;
        }
      }
      if (targetExpr.kind == Expr::Kind::Call && getBuiltinArrayAccessName(targetExpr, accessName) &&
          targetExpr.args.size() == 2 && targetExpr.args.front().kind == Expr::Kind::Name) {
        auto it = localsIn.find(targetExpr.args.front().name);
        if (it != localsIn.end() && it->second.isArgsPack &&
            ((it->second.argsPackElementKind == LocalInfo::Kind::Reference && it->second.referenceToBuffer) ||
             (it->second.argsPackElementKind == LocalInfo::Kind::Pointer && it->second.pointerToBuffer))) {
          return it->second.valueKind;
        }
      }
    }
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
      if (it->second.kind == LocalInfo::Kind::Array || it->second.kind == LocalInfo::Kind::Vector ||
          it->second.kind == LocalInfo::Kind::Map || it->second.kind == LocalInfo::Kind::Buffer ||
          (it->second.kind == LocalInfo::Kind::Reference &&
           (it->second.referenceToArray || it->second.referenceToVector))) {
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

    std::string collection;
    if (getBuiltinCollectionName(expr, collection)) {
      if ((collection == "array" || collection == "vector" || collection == "soa_vector") &&
          expr.templateArgs.size() == 1) {
        return valueKindFromTypeName(expr.templateArgs.front());
      }
      if (collection == "map" && expr.templateArgs.size() == 2) {
        return valueKindFromTypeName(expr.templateArgs.back());
      }
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
    LocalInfo::ValueKind &kindOut,
    const ResolveSetupInferenceCallCollectionAccessValueKindFn &resolveCallCollectionAccessValueKind) {
  kindOut = LocalInfo::ValueKind::Unknown;

  std::string accessName;
  if (!getBuiltinArrayAccessName(expr, accessName)) {
    return ArrayMapAccessElementKindResolution::NotMatched;
  }
  if (expr.args.size() != 2) {
    return ArrayMapAccessElementKindResolution::Resolved;
  }

  auto hasNamedArgs = [&]() {
    for (const auto &argName : expr.argNames) {
      if (argName.has_value()) {
        return true;
      }
    }
    return false;
  };
  auto isKnownCollectionAccessReceiverExpr = [&](const Expr &candidate) -> bool {
    if (candidate.kind != Expr::Kind::Name) {
      return false;
    }
    auto it = localsIn.find(candidate.name);
    if (it == localsIn.end()) {
      return false;
    }
    const LocalInfo &info = it->second;
    return info.kind == LocalInfo::Kind::Array || info.kind == LocalInfo::Kind::Vector || info.kind == LocalInfo::Kind::Map ||
           (info.kind == LocalInfo::Kind::Reference &&
            (info.referenceToArray || info.referenceToVector || info.referenceToMap)) ||
           (info.kind == LocalInfo::Kind::Pointer && info.pointerToArray) ||
           (info.kind == LocalInfo::Kind::Pointer && info.pointerToVector) ||
           (info.kind == LocalInfo::Kind::Pointer && info.pointerToMap) ||
           info.isSoaVector ||
           (info.kind == LocalInfo::Kind::Value && info.valueKind == LocalInfo::ValueKind::String);
  };

  std::vector<size_t> receiverIndices;
  auto appendReceiverIndex = [&](size_t index) {
    if (index >= expr.args.size()) {
      return;
    }
    for (size_t existing : receiverIndices) {
      if (existing == index) {
        return;
      }
    }
    receiverIndices.push_back(index);
  };
  const bool hasNamedArgsValue = hasNamedArgs();
  if (hasNamedArgsValue) {
    bool hasValuesNamedReceiver = false;
    for (size_t i = 0; i < expr.args.size(); ++i) {
      if (i < expr.argNames.size() && expr.argNames[i].has_value() && *expr.argNames[i] == "values") {
        appendReceiverIndex(i);
        hasValuesNamedReceiver = true;
      }
    }
    if (!hasValuesNamedReceiver) {
      appendReceiverIndex(0);
      for (size_t i = 1; i < expr.args.size(); ++i) {
        appendReceiverIndex(i);
      }
    }
  } else {
    appendReceiverIndex(0);
  }
  const bool probePositionalReorderedReceiver =
      !hasNamedArgsValue && expr.args.size() > 1 &&
      (expr.args.front().kind == Expr::Kind::Literal || expr.args.front().kind == Expr::Kind::BoolLiteral ||
       expr.args.front().kind == Expr::Kind::FloatLiteral || expr.args.front().kind == Expr::Kind::StringLiteral ||
       (expr.args.front().kind == Expr::Kind::Name &&
        !isKnownCollectionAccessReceiverExpr(expr.args.front())));
  if (probePositionalReorderedReceiver) {
    for (size_t i = 1; i < expr.args.size(); ++i) {
      appendReceiverIndex(i);
    }
  }
  const bool hasAlternativeCollectionReceiver =
      probePositionalReorderedReceiver &&
      std::any_of(receiverIndices.begin(), receiverIndices.end(), [&](size_t index) {
        return index > 0 && index < expr.args.size() && isKnownCollectionAccessReceiverExpr(expr.args[index]);
      });

  for (size_t receiverIndex : receiverIndices) {
    if (receiverIndex >= expr.args.size()) {
      continue;
    }
    if (hasAlternativeCollectionReceiver && receiverIndex == 0) {
      continue;
    }
    const Expr &target = expr.args[receiverIndex];
    if (target.kind == Expr::Kind::StringLiteral) {
      kindOut = LocalInfo::ValueKind::Int32;
      return ArrayMapAccessElementKindResolution::Resolved;
    }
    if (target.kind == Expr::Kind::Name) {
      auto it = localsIn.find(target.name);
      if (it != localsIn.end() && it->second.kind == LocalInfo::Kind::Value &&
          it->second.valueKind == LocalInfo::ValueKind::String) {
        kindOut = LocalInfo::ValueKind::Int32;
        return ArrayMapAccessElementKindResolution::Resolved;
      }
    }
    if (isEntryArgsName(target, localsIn)) {
      return ArrayMapAccessElementKindResolution::Resolved;
    }

    if (target.kind == Expr::Kind::Name) {
      auto it = localsIn.find(target.name);
      if (it != localsIn.end() &&
          ((it->second.kind == LocalInfo::Kind::Map) ||
           (it->second.kind == LocalInfo::Kind::Reference && it->second.referenceToMap) ||
           (it->second.kind == LocalInfo::Kind::Pointer && it->second.pointerToMap)) &&
          it->second.mapValueKind != LocalInfo::ValueKind::Unknown) {
        kindOut = it->second.mapValueKind;
        return ArrayMapAccessElementKindResolution::Resolved;
      }
    } else if (target.kind == Expr::Kind::Call) {
      LocalInfo::ValueKind callValueKind = LocalInfo::ValueKind::Unknown;
      if (resolveCallCollectionAccessValueKind &&
          resolveCallCollectionAccessValueKind(target, localsIn, callValueKind)) {
        kindOut = callValueKind;
        return ArrayMapAccessElementKindResolution::Resolved;
      }
      std::string collection;
      if (getBuiltinCollectionName(target, collection) && collection == "map" && target.templateArgs.size() == 2) {
        const LocalInfo::ValueKind valueKind = valueKindFromTypeName(target.templateArgs[1]);
        if (valueKind != LocalInfo::ValueKind::Unknown) {
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
          } else if (it->second.kind == LocalInfo::Kind::Reference &&
                     (it->second.referenceToArray || it->second.referenceToVector)) {
            elementKind = it->second.valueKind;
          } else if (it->second.kind == LocalInfo::Kind::Pointer && it->second.pointerToArray) {
            elementKind = it->second.valueKind;
          } else if (it->second.kind == LocalInfo::Kind::Pointer && it->second.pointerToVector) {
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
    if (elementKind != LocalInfo::ValueKind::Unknown) {
      kindOut = elementKind;
      return ArrayMapAccessElementKindResolution::Resolved;
    }
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
    const InferSetupInferenceStructExprPathFn &inferStructExprPath,
    const ResolveSetupInferenceDefinitionCallFn &resolveDefinitionCall,
    const SemanticProductTargetAdapter *semanticProductTargets) {
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
      const StatementBindingTypeInfo typeInfo =
          inferStatementBindingTypeInfo(bodyExpr,
                                        bodyExpr.args.front(),
                                        bodyLocals,
                                        hasExplicitBindingTypeTransform,
                                        bindingKind,
                                        bindingValueKind,
                                        inferExprKind,
                                        resolveDefinitionCall,
                                        semanticProductTargets);
      info.kind = typeInfo.kind;
      info.valueKind = typeInfo.valueKind;
      info.mapKeyKind = typeInfo.mapKeyKind;
      info.mapValueKind = typeInfo.mapValueKind;
      info.structTypeName = typeInfo.structTypeName;
      if (info.valueKind == LocalInfo::ValueKind::Unknown && info.kind == LocalInfo::Kind::Value) {
        info.valueKind = LocalInfo::ValueKind::Int32;
      }
      applyStructArrayInfo(bodyExpr, info);
      applyStructValueInfo(bodyExpr, info);
      if (info.structTypeName.empty() && info.kind == LocalInfo::Kind::Value) {
        std::string inferredStruct = inferStructExprPath(bodyExpr.args.front(), bodyLocals);
        if (!inferredStruct.empty()) {
          info.structTypeName = inferredStruct;
        }
      }
      if (info.kind == LocalInfo::Kind::Value && !info.structTypeName.empty()) {
        info.valueKind = LocalInfo::ValueKind::Int64;
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

} // namespace primec::ir_lowerer
