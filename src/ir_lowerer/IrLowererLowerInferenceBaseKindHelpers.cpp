#include "IrLowererLowerInferenceBaseKindHelpers.h"

#include "IrLowererHelpers.h"
#include "IrLowererResultHelpers.h"
#include "IrLowererSemanticProductTargetAdapters.h"
#include "IrLowererSetupTypeHelpers.h"
#include "IrLowererTemplateTypeParseHelpers.h"

namespace primec::ir_lowerer {

namespace {

std::string resolveScopedExprPath(const Expr &expr) {
  if (expr.name.empty()) {
    return {};
  }
  if (!expr.namespacePrefix.empty()) {
    return expr.namespacePrefix + "/" + expr.name;
  }
  return expr.name;
}

bool isBaseSetupResultOrTryCall(const Expr &expr) {
  if (expr.kind != Expr::Kind::Call) {
    return false;
  }
  if (isSimpleCallName(expr, "try")) {
    return true;
  }
  return expr.isMethodCall && !expr.args.empty() && expr.args.front().kind == Expr::Kind::Name &&
         expr.args.front().name == "Result";
}

LocalInfo::ValueKind inferBaseSetupSimpleExprKind(const Expr &expr,
                                                  const LocalMap &localsIn,
                                                  const ResolveMethodCallWithLocalsFn *resolveMethodCall,
                                                  const ResolveCallDefinitionFn *resolveDefinitionCall,
                                                  const LookupReturnInfoFn *lookupReturnInfo,
                                                  const SemanticProgram *semanticProgram,
                                                  const SemanticProductIndex *semanticIndex,
                                                  const InferExprKindWithLocalsFn *fallbackInferExprKind);

bool inferBaseSetupSemanticQueryFactValueKind(const Expr &expr,
                                              const SemanticProgram *semanticProgram,
                                              const SemanticProductIndex *semanticIndex,
                                              LocalInfo::ValueKind &kindOut) {
  kindOut = LocalInfo::ValueKind::Unknown;
  if (semanticProgram == nullptr || semanticIndex == nullptr || expr.semanticNodeId == 0) {
    return false;
  }
  const auto *queryFact = findSemanticProductQueryFactBySemanticId(*semanticIndex, expr);
  if (queryFact == nullptr) {
    return false;
  }
  const LocalInfo::ValueKind queryKind = valueKindFromTypeName(trimTemplateTypeText(queryFact->queryTypeText));
  if (queryKind != LocalInfo::ValueKind::Unknown) {
    kindOut = queryKind;
    return true;
  }
  const LocalInfo::ValueKind bindingKind = valueKindFromTypeName(trimTemplateTypeText(queryFact->bindingTypeText));
  if (bindingKind != LocalInfo::ValueKind::Unknown) {
    kindOut = bindingKind;
    return true;
  }
  return false;
}

bool resolveBaseSetupResultExprInfo(const Expr &expr,
                                    const LocalMap &localsIn,
                                    const ResolveMethodCallWithLocalsFn *resolveMethodCall,
                                    const ResolveCallDefinitionFn *resolveDefinitionCall,
                                    const LookupReturnInfoFn *lookupReturnInfo,
                                    const SemanticProgram *semanticProgram,
                                    const SemanticProductIndex *semanticIndex,
                                    const InferExprKindWithLocalsFn *fallbackInferExprKind,
                                    ResultExprInfo &out) {
  const ResolveMethodCallWithLocalsFn noopResolveMethodCall =
      [](const Expr &, const LocalMap &) -> const Definition * { return nullptr; };
  const ResolveCallDefinitionFn noopResolveDefinitionCall = [](const Expr &) -> const Definition * { return nullptr; };
  const LookupReturnInfoFn noopLookupReturnInfo = [](const std::string &, ReturnInfo &) { return false; };
  const ResolveMethodCallWithLocalsFn &resolveMethodCallFn =
      (resolveMethodCall != nullptr && *resolveMethodCall) ? *resolveMethodCall : noopResolveMethodCall;
  const ResolveCallDefinitionFn &resolveDefinitionCallFn =
      (resolveDefinitionCall != nullptr && *resolveDefinitionCall) ? *resolveDefinitionCall : noopResolveDefinitionCall;
  const LookupReturnInfoFn &lookupReturnInfoFn =
      (lookupReturnInfo != nullptr && *lookupReturnInfo) ? *lookupReturnInfo : noopLookupReturnInfo;
  return resolveResultExprInfoFromLocals(
      expr,
      localsIn,
      resolveMethodCallFn,
      resolveDefinitionCallFn,
      lookupReturnInfoFn,
      [resolveMethodCall, resolveDefinitionCall, lookupReturnInfo, semanticProgram, semanticIndex, fallbackInferExprKind](
          const Expr &candidate, const LocalMap &candidateLocals) {
        return inferBaseSetupSimpleExprKind(
            candidate,
            candidateLocals,
            resolveMethodCall,
            resolveDefinitionCall,
            lookupReturnInfo,
            semanticProgram,
            semanticIndex,
            fallbackInferExprKind);
      },
      out);
}

LocalInfo::ValueKind inferBaseSetupSimpleExprKind(const Expr &expr,
                                                  const LocalMap &localsIn,
                                                  const ResolveMethodCallWithLocalsFn *resolveMethodCall,
                                                  const ResolveCallDefinitionFn *resolveDefinitionCall,
                                                  const LookupReturnInfoFn *lookupReturnInfo,
                                                  const SemanticProgram *semanticProgram,
                                                  const SemanticProductIndex *semanticIndex,
                                                  const InferExprKindWithLocalsFn *fallbackInferExprKind) {
  switch (expr.kind) {
    case Expr::Kind::Literal:
      if (expr.isUnsigned) {
        return LocalInfo::ValueKind::UInt64;
      }
      return (expr.intWidth == 64) ? LocalInfo::ValueKind::Int64 : LocalInfo::ValueKind::Int32;
    case Expr::Kind::FloatLiteral:
      return (expr.floatWidth == 64) ? LocalInfo::ValueKind::Float64 : LocalInfo::ValueKind::Float32;
    case Expr::Kind::BoolLiteral:
      return LocalInfo::ValueKind::Bool;
    case Expr::Kind::StringLiteral:
      return LocalInfo::ValueKind::String;
    case Expr::Kind::Name: {
      auto it = localsIn.find(expr.name);
      if (it == localsIn.end()) {
        return LocalInfo::ValueKind::Unknown;
      }
      if (it->second.kind == LocalInfo::Kind::Value) {
        return it->second.valueKind;
      }
      if (it->second.kind == LocalInfo::Kind::Reference && !it->second.referenceToArray &&
          !it->second.referenceToVector && !it->second.referenceToMap) {
        return it->second.valueKind;
      }
      return LocalInfo::ValueKind::Unknown;
    }
    case Expr::Kind::Call: {
      if (!expr.isMethodCall && expr.args.empty() && expr.templateArgs.empty() &&
          !expr.hasBodyArguments && expr.bodyArguments.empty()) {
        const std::string resolvedPrimitivePath = resolveScopedExprPath(expr);
        const size_t slash = resolvedPrimitivePath.find_last_of('/');
        const std::string leaf = slash == std::string::npos
                                     ? resolvedPrimitivePath
                                     : resolvedPrimitivePath.substr(slash + 1);
        if (leaf == "int" || leaf == "i32" || leaf == "i64" ||
            leaf == "u64" || leaf == "float" || leaf == "f32" ||
            leaf == "f64" || leaf == "bool") {
          return valueKindFromTypeName(leaf);
        }
      }
      LocalInfo::ValueKind kindOut = LocalInfo::ValueKind::Unknown;
      if (inferBaseSetupSemanticQueryFactValueKind(expr, semanticProgram, semanticIndex, kindOut)) {
        return kindOut;
      }
      ResultExprInfo resultInfo;
      if (resolveBaseSetupResultExprInfo(
              expr,
              localsIn,
              resolveMethodCall,
              resolveDefinitionCall,
              lookupReturnInfo,
              semanticProgram,
              semanticIndex,
              fallbackInferExprKind,
              resultInfo) &&
          resultInfo.isResult &&
          resultInfo.hasValue) {
        return resultInfo.valueKind;
      }
      const bool hasSemanticProductQuerySite =
          semanticProgram != nullptr && semanticIndex != nullptr && expr.semanticNodeId != 0;
      if (!hasSemanticProductQuerySite && fallbackInferExprKind != nullptr && *fallbackInferExprKind &&
          !isBaseSetupResultOrTryCall(expr)) {
        kindOut = (*fallbackInferExprKind)(expr, localsIn);
        if (kindOut != LocalInfo::ValueKind::Unknown) {
          return kindOut;
        }
      }
      std::string builtinComparison;
      if (getBuiltinComparisonName(expr, builtinComparison)) {
        return LocalInfo::ValueKind::Bool;
      }
      return LocalInfo::ValueKind::Unknown;
    }
    default:
      return LocalInfo::ValueKind::Unknown;
  }
}

} // namespace

bool isIndexedArgsPackFileHandleReceiver(const Expr &receiverExpr, const LocalMap &localsIn) {
  std::string accessName;
  if (receiverExpr.kind != Expr::Kind::Call || !getBuiltinArrayAccessName(receiverExpr, accessName) ||
      receiverExpr.args.size() != 2 || receiverExpr.args.front().kind != Expr::Kind::Name) {
    return false;
  }
  auto it = localsIn.find(receiverExpr.args.front().name);
  return it != localsIn.end() && it->second.isArgsPack && it->second.isFileHandle &&
         (it->second.argsPackElementKind == LocalInfo::Kind::Value ||
          it->second.argsPackElementKind == LocalInfo::Kind::Reference ||
          it->second.argsPackElementKind == LocalInfo::Kind::Pointer);
}

bool isIndexedBorrowedArgsPackFileHandleReceiver(const Expr &receiverExpr, const LocalMap &localsIn) {
  if (!(receiverExpr.kind == Expr::Kind::Call && isSimpleCallName(receiverExpr, "dereference") &&
        receiverExpr.args.size() == 1)) {
    return false;
  }
  std::string accessName;
  const Expr &targetExpr = receiverExpr.args.front();
  if (targetExpr.kind != Expr::Kind::Call || !getBuiltinArrayAccessName(targetExpr, accessName) ||
      targetExpr.args.size() != 2 || targetExpr.args.front().kind != Expr::Kind::Name) {
    return false;
  }
  auto it = localsIn.find(targetExpr.args.front().name);
  return it != localsIn.end() && it->second.isArgsPack && it->second.isFileHandle &&
         it->second.argsPackElementKind == LocalInfo::Kind::Reference;
}

bool isIndexedPointerArgsPackFileHandleReceiver(const Expr &receiverExpr, const LocalMap &localsIn) {
  if (!(receiverExpr.kind == Expr::Kind::Call && isSimpleCallName(receiverExpr, "dereference") &&
        receiverExpr.args.size() == 1)) {
    return false;
  }
  std::string accessName;
  const Expr &targetExpr = receiverExpr.args.front();
  if (targetExpr.kind != Expr::Kind::Call || !getBuiltinArrayAccessName(targetExpr, accessName) ||
      targetExpr.args.size() != 2 || targetExpr.args.front().kind != Expr::Kind::Name) {
    return false;
  }
  auto it = localsIn.find(targetExpr.args.front().name);
  return it != localsIn.end() && it->second.isArgsPack && it->second.isFileHandle &&
         it->second.argsPackElementKind == LocalInfo::Kind::Pointer;
}

std::string resolveScopedCallName(const Expr &expr) {
  if (expr.name.find('/') != std::string::npos || expr.namespacePrefix.empty()) {
    return expr.name;
  }
  if (expr.namespacePrefix == "/") {
    return "/" + expr.name;
  }
  return expr.namespacePrefix + "/" + expr.name;
}

bool isMapTryAtCallName(const Expr &expr) {
  if (isSimpleCallName(expr, "tryAt")) {
    return true;
  }
  if (expr.name.empty()) {
    return false;
  }
  std::string normalized = resolveScopedCallName(expr);
  if (!normalized.empty() && normalized.front() == '/') {
    normalized.erase(normalized.begin());
  }
  return normalized == "map/tryAt" || normalized == "std/collections/map/tryAt";
}

bool isMapContainsCallName(const Expr &expr) {
  if (isSimpleCallName(expr, "contains")) {
    return true;
  }
  if (expr.name.empty()) {
    return false;
  }
  std::string normalized = resolveScopedCallName(expr);
  if (!normalized.empty() && normalized.front() == '/') {
    normalized.erase(normalized.begin());
  }
  return normalized == "map/contains" || normalized == "std/collections/map/contains";
}

bool inferMapTryAtResultValueKind(const Expr &expr,
                                  const LocalMap &localsIn,
                                  LocalInfo::ValueKind &kindOut) {
  kindOut = LocalInfo::ValueKind::Unknown;
  if (expr.kind != Expr::Kind::Call || expr.args.empty() || !isMapTryAtCallName(expr)) {
    return false;
  }
  const auto targetInfo = resolveMapAccessTargetInfo(expr.args.front(), localsIn);
  if (!targetInfo.isMapTarget || targetInfo.mapValueKind == LocalInfo::ValueKind::Unknown) {
    return false;
  }
  kindOut = targetInfo.mapValueKind;
  return true;
}

bool inferMapContainsResultKind(const Expr &expr,
                                const LocalMap &localsIn,
                                LocalInfo::ValueKind &kindOut) {
  kindOut = LocalInfo::ValueKind::Unknown;
  if (expr.kind != Expr::Kind::Call || expr.args.empty() || !isMapContainsCallName(expr)) {
    return false;
  }
  const auto targetInfo = resolveMapAccessTargetInfo(expr.args.front(), localsIn);
  if (!targetInfo.isMapTarget) {
    return false;
  }
  kindOut = LocalInfo::ValueKind::Bool;
  return true;
}

bool runLowerInferenceExprKindBaseSetup(const LowerInferenceExprKindBaseSetupInput &input,
                                        LowerInferenceSetupBootstrapState &stateInOut,
                                        std::string &errorOut) {
  if (!input.getMathConstantName) {
    errorOut = "native backend missing inference expr-kind base setup dependency: getMathConstantName";
    return false;
  }

  const auto getMathConstantName = input.getMathConstantName;
  stateInOut.inferLiteralOrNameExprKind =
      [getMathConstantName](const Expr &expr, const LocalMap &localsIn, LocalInfo::ValueKind &kindOut) {
        return inferLiteralOrNameExprKindImpl(expr, localsIn, getMathConstantName, kindOut);
      };
  return true;
}

bool runLowerInferenceExprKindCallBaseSetup(const LowerInferenceExprKindCallBaseSetupInput &input,
                                            LowerInferenceSetupBootstrapState &stateInOut,
                                            std::string &errorOut) {
  if (!input.inferStructExprPath) {
    errorOut = "native backend missing inference expr-kind call-base setup dependency: inferStructExprPath";
    return false;
  }
  if (!input.resolveStructFieldSlot) {
    errorOut = "native backend missing inference expr-kind call-base setup dependency: resolveStructFieldSlot";
    return false;
  }
  if (!input.resolveUninitializedStorage) {
    errorOut = "native backend missing inference expr-kind call-base setup dependency: resolveUninitializedStorage";
    return false;
  }

  const auto inferStructExprPath = input.inferStructExprPath;
  const auto resolveStructFieldSlot = input.resolveStructFieldSlot;
  const auto resolveUninitializedStorage = input.resolveUninitializedStorage;
  const auto *semanticProgram = stateInOut.semanticProgram;
  const auto *semanticIndex = stateInOut.semanticIndex;
  stateInOut.inferCallExprBaseKind =
      [inferStructExprPath, resolveStructFieldSlot, resolveUninitializedStorage, semanticProgram, semanticIndex, &stateInOut](
          const Expr &expr, const LocalMap &localsIn, LocalInfo::ValueKind &kindOut) {
        const ResolveMethodCallWithLocalsFn resolveMethodCall =
            [&stateInOut](const Expr &candidate, const LocalMap &candidateLocals) -> const Definition * {
          return stateInOut.resolveMethodCallDefinition != nullptr
                     ? stateInOut.resolveMethodCallDefinition(candidate, candidateLocals)
                     : nullptr;
        };
        const ResolveCallDefinitionFn resolveDefinitionCall = [&stateInOut](const Expr &candidate) -> const Definition * {
          return stateInOut.resolveDefinitionCall != nullptr ? stateInOut.resolveDefinitionCall(candidate) : nullptr;
        };
        const LookupReturnInfoFn lookupReturnInfo = [&stateInOut](const std::string &path, ReturnInfo &returnInfoOut) {
          return stateInOut.getReturnInfo != nullptr ? stateInOut.getReturnInfo(path, returnInfoOut) : false;
        };
        return inferCallExprBaseKindImpl(
            expr,
            localsIn,
            inferStructExprPath,
            resolveStructFieldSlot,
            resolveUninitializedStorage,
            &resolveMethodCall,
            &resolveDefinitionCall,
            &lookupReturnInfo,
            semanticProgram,
            semanticIndex,
            &stateInOut.inferExprKind,
            kindOut);
      };
  return true;
}

bool inferLiteralOrNameExprKindImpl(const Expr &expr,
                                    const LocalMap &localsIn,
                                    const GetSetupMathConstantNameFn &getMathConstantName,
                                    LocalInfo::ValueKind &kindOut) {
  kindOut = LocalInfo::ValueKind::Unknown;
  switch (expr.kind) {
    case Expr::Kind::Literal:
      if (expr.isUnsigned) {
        kindOut = LocalInfo::ValueKind::UInt64;
      } else if (expr.intWidth == 64) {
        kindOut = LocalInfo::ValueKind::Int64;
      } else {
        kindOut = LocalInfo::ValueKind::Int32;
      }
      return true;
    case Expr::Kind::FloatLiteral:
      kindOut = (expr.floatWidth == 64) ? LocalInfo::ValueKind::Float64 : LocalInfo::ValueKind::Float32;
      return true;
    case Expr::Kind::BoolLiteral:
      kindOut = LocalInfo::ValueKind::Bool;
      return true;
    case Expr::Kind::StringLiteral:
      kindOut = LocalInfo::ValueKind::String;
      return true;
    case Expr::Kind::Name: {
      auto it = localsIn.find(expr.name);
      if (it == localsIn.end()) {
        std::string mathConstant;
        if (getMathConstantName(expr.name, mathConstant)) {
          kindOut = LocalInfo::ValueKind::Float64;
        }
        return true;
      }
      if (it->second.kind == LocalInfo::Kind::Value) {
        kindOut = it->second.valueKind;
        return true;
      }
      if (it->second.kind == LocalInfo::Kind::Reference) {
        kindOut = (it->second.referenceToArray || it->second.referenceToVector || it->second.referenceToMap)
                      ? LocalInfo::ValueKind::Unknown
                      : it->second.valueKind;
        return true;
      }
      kindOut = LocalInfo::ValueKind::Unknown;
      return true;
    }
    default:
      return false;
  }
}

bool inferCallExprBaseKindImpl(const Expr &expr,
                               const LocalMap &localsIn,
                               const InferStructExprWithLocalsFn &inferStructExprPath,
                               const ResolveStructFieldSlotFn &resolveStructFieldSlot,
                               const std::function<bool(const Expr &,
                                                        const LocalMap &,
                                                        UninitializedStorageAccessInfo &,
                                                        bool &)> &resolveUninitializedStorage,
                               const ResolveMethodCallWithLocalsFn *resolveMethodCall,
                               const ResolveCallDefinitionFn *resolveDefinitionCall,
                               const LookupReturnInfoFn *lookupReturnInfo,
                               const SemanticProgram *semanticProgram,
                               const SemanticProductIndex *semanticIndex,
                               const InferExprKindWithLocalsFn *fallbackInferExprKind,
                               LocalInfo::ValueKind &kindOut) {
  kindOut = LocalInfo::ValueKind::Unknown;
  if (expr.kind != Expr::Kind::Call) {
    return false;
  }
  if (expr.isFieldAccess) {
    if (expr.args.size() != 1) {
      return true;
    }
    const Expr &receiver = expr.args.front();
    std::string structPath = inferStructExprPath(receiver, localsIn);
    if (structPath.empty()) {
      return true;
    }
    StructSlotFieldInfo fieldInfo;
    if (!resolveStructFieldSlot(structPath, expr.name, fieldInfo)) {
      return true;
    }
    kindOut = fieldInfo.structPath.empty() ? fieldInfo.valueKind : LocalInfo::ValueKind::Unknown;
    return true;
  }
  if (!expr.isMethodCall && (isSimpleCallName(expr, "take") || isSimpleCallName(expr, "borrow")) &&
      expr.args.size() == 1) {
    UninitializedStorageAccessInfo access;
    bool resolved = false;
    if (!resolveUninitializedStorage(expr.args.front(), localsIn, access, resolved)) {
      return true;
    }
    if (!resolved) {
      return false;
    }
    if (access.typeInfo.kind == LocalInfo::Kind::Value && access.typeInfo.structPath.empty() &&
        access.typeInfo.valueKind != LocalInfo::ValueKind::Unknown) {
      kindOut = access.typeInfo.valueKind;
    }
    return true;
  }
  if (inferBaseSetupSemanticQueryFactValueKind(expr, semanticProgram, semanticIndex, kindOut)) {
    return true;
  }
  if (expr.isMethodCall) {
    if (!expr.args.empty() && expr.args.front().kind == Expr::Kind::Name && expr.args.front().name == "Result") {
      if (expr.name == "ok") {
        kindOut = expr.args.size() > 1 ? LocalInfo::ValueKind::Int64 : LocalInfo::ValueKind::Int32;
        return true;
      }
      if (expr.name == "error") {
        kindOut = LocalInfo::ValueKind::Bool;
        return true;
      }
      if (expr.name == "why") {
        kindOut = LocalInfo::ValueKind::String;
        return true;
      }
    }
    if (!expr.args.empty() && expr.name == "why") {
      const Expr &receiver = expr.args.front();
      if (receiver.kind == Expr::Kind::Name) {
        const std::string receiverPath = resolveScopedExprPath(receiver);
        if (receiverPath == "FileError" || receiverPath == "/std/file/FileError") {
          kindOut = LocalInfo::ValueKind::String;
          return true;
        }
        auto it = localsIn.find(receiver.name);
        if (it != localsIn.end() && it->second.isFileError) {
          kindOut = LocalInfo::ValueKind::String;
          return true;
        }
      }
      if (receiver.kind == Expr::Kind::Call) {
        std::string accessName;
        if (getBuiltinArrayAccessName(receiver, accessName) && receiver.args.size() == 2 &&
            receiver.args.front().kind == Expr::Kind::Name) {
          auto it = localsIn.find(receiver.args.front().name);
          if (it != localsIn.end() && it->second.isArgsPack && it->second.isFileError &&
              (it->second.argsPackElementKind == LocalInfo::Kind::Value ||
               it->second.argsPackElementKind == LocalInfo::Kind::Reference ||
               it->second.argsPackElementKind == LocalInfo::Kind::Pointer)) {
            kindOut = LocalInfo::ValueKind::String;
            return true;
          }
        }
        if (isSimpleCallName(receiver, "dereference") && receiver.args.size() == 1) {
          const Expr &target = receiver.args.front();
          if (target.kind == Expr::Kind::Call && getBuiltinArrayAccessName(target, accessName) &&
              target.args.size() == 2 && target.args.front().kind == Expr::Kind::Name) {
            auto it = localsIn.find(target.args.front().name);
            if (it != localsIn.end() && it->second.isArgsPack && it->second.isFileError &&
                (it->second.argsPackElementKind == LocalInfo::Kind::Reference ||
                 it->second.argsPackElementKind == LocalInfo::Kind::Pointer)) {
              kindOut = LocalInfo::ValueKind::String;
              return true;
            }
          }
        }
      }
    }
    if (!expr.args.empty() && expr.args.front().kind == Expr::Kind::Name) {
      auto it = localsIn.find(expr.args.front().name);
      if (it != localsIn.end() && it->second.isFileHandle) {
        if (expr.name == "write" || expr.name == "write_line" || expr.name == "write_byte" || expr.name == "read_byte" ||
            expr.name == "write_bytes" || expr.name == "flush" || expr.name == "close") {
          kindOut = LocalInfo::ValueKind::Int32;
          return true;
        }
      }
    }
    if (!expr.args.empty() && isIndexedArgsPackFileHandleReceiver(expr.args.front(), localsIn)) {
      if (expr.name == "write" || expr.name == "write_line" || expr.name == "write_byte" || expr.name == "read_byte" ||
          expr.name == "write_bytes" || expr.name == "flush" || expr.name == "close") {
        kindOut = LocalInfo::ValueKind::Int32;
        return true;
      }
    }
    if (!expr.args.empty() && isIndexedBorrowedArgsPackFileHandleReceiver(expr.args.front(), localsIn)) {
      if (expr.name == "write" || expr.name == "write_line" || expr.name == "write_byte" || expr.name == "read_byte" ||
          expr.name == "write_bytes" || expr.name == "flush" || expr.name == "close") {
        kindOut = LocalInfo::ValueKind::Int32;
        return true;
      }
    }
    if (!expr.args.empty() && isIndexedPointerArgsPackFileHandleReceiver(expr.args.front(), localsIn)) {
      if (expr.name == "write" || expr.name == "write_line" || expr.name == "write_byte" || expr.name == "read_byte" ||
          expr.name == "write_bytes" || expr.name == "flush" || expr.name == "close") {
        kindOut = LocalInfo::ValueKind::Int32;
        return true;
      }
    }
    return false;
  }
  if (isFileHandleCall(expr)) {
    kindOut = LocalInfo::ValueKind::Int64;
    return true;
  }
  if (isSimpleCallName(expr, "try") && expr.args.size() == 1) {
    const Expr &arg = expr.args.front();
    if (arg.kind == Expr::Kind::Name) {
      auto it = localsIn.find(arg.name);
      if (it != localsIn.end() && it->second.isResult) {
        kindOut = it->second.resultHasValue ? it->second.resultValueKind : LocalInfo::ValueKind::Int32;
        return true;
      }
    }
    if (arg.kind == Expr::Kind::Call) {
      if (inferMapTryAtResultValueKind(arg, localsIn, kindOut)) {
        return true;
      }
      std::string accessName;
      if (getBuiltinArrayAccessName(arg, accessName) && arg.args.size() == 2 && arg.args.front().kind == Expr::Kind::Name) {
        auto it = localsIn.find(arg.args.front().name);
        if (it != localsIn.end() && it->second.isArgsPack && it->second.isResult) {
          kindOut = it->second.resultHasValue ? it->second.resultValueKind : LocalInfo::ValueKind::Int32;
          return true;
        }
      }
      if (isSimpleCallName(arg, "dereference") && arg.args.size() == 1) {
        const Expr &targetExpr = arg.args.front();
        if (targetExpr.kind == Expr::Kind::Call && getBuiltinArrayAccessName(targetExpr, accessName) &&
            targetExpr.args.size() == 2 && targetExpr.args.front().kind == Expr::Kind::Name) {
          auto it = localsIn.find(targetExpr.args.front().name);
          if (it != localsIn.end() && it->second.isArgsPack && it->second.isResult &&
              (it->second.argsPackElementKind == LocalInfo::Kind::Reference ||
               it->second.argsPackElementKind == LocalInfo::Kind::Pointer)) {
            kindOut = it->second.resultHasValue ? it->second.resultValueKind : LocalInfo::ValueKind::Int32;
            return true;
          }
        }
      }
      ResultExprInfo resultInfo;
      if (resolveBaseSetupResultExprInfo(
              arg,
              localsIn,
              resolveMethodCall,
              resolveDefinitionCall,
              lookupReturnInfo,
              semanticProgram,
              semanticIndex,
              fallbackInferExprKind,
              resultInfo) &&
          resultInfo.isResult) {
        kindOut = resultInfo.hasValue ? resultInfo.valueKind : LocalInfo::ValueKind::Int32;
        return true;
      }
    }
    if (arg.kind == Expr::Kind::Call) {
      if (isFileHandleCall(arg)) {
        kindOut = LocalInfo::ValueKind::Int64;
        return true;
      }
      if (arg.isMethodCall && !arg.args.empty() && arg.args.front().kind == Expr::Kind::Name) {
        auto it = localsIn.find(arg.args.front().name);
        if (it != localsIn.end() && it->second.isFileHandle) {
          if (arg.name == "write" || arg.name == "write_line" || arg.name == "write_byte" || arg.name == "read_byte" ||
              arg.name == "write_bytes" || arg.name == "flush" || arg.name == "close") {
            kindOut = LocalInfo::ValueKind::Int32;
            return true;
          }
        }
        if (arg.args.front().name == "Result") {
          if (arg.name == "ok") {
            kindOut = arg.args.size() > 1 ? LocalInfo::ValueKind::Int64 : LocalInfo::ValueKind::Int32;
            return true;
          }
          if (arg.name == "error") {
            kindOut = LocalInfo::ValueKind::Bool;
            return true;
          }
          if (arg.name == "why") {
            kindOut = LocalInfo::ValueKind::String;
            return true;
          }
        }
      }
    }
  }
  return false;
}

} // namespace primec::ir_lowerer
