#include "IrLowererLowerInferenceSetup.h"

#include "../semantics/CondensationDag.h"
#include "../semantics/TypeResolutionGraph.h"

#include "IrLowererHelpers.h"
#include "IrLowererResultHelpers.h"
#include "IrLowererSetupTypeHelpers.h"
#include "IrLowererUninitializedTypeHelpers.h"

#include <algorithm>
#include <limits>
#include <vector>

namespace primec::ir_lowerer {

namespace {

bool isIndexedArgsPackFileHandleReceiver(const Expr &receiverExpr, const LocalMap &localsIn) {
  std::string accessName;
  if (receiverExpr.kind != Expr::Kind::Call || !getBuiltinArrayAccessName(receiverExpr, accessName) ||
      receiverExpr.args.size() != 2 || receiverExpr.args.front().kind != Expr::Kind::Name) {
    return false;
  }
  auto it = localsIn.find(receiverExpr.args.front().name);
  return it != localsIn.end() && it->second.isArgsPack && it->second.isFileHandle &&
         it->second.argsPackElementKind == LocalInfo::Kind::Value;
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

bool isMapTryAtCallName(const Expr &expr) {
  if (isSimpleCallName(expr, "tryAt")) {
    return true;
  }
  if (expr.name.empty()) {
    return false;
  }
  std::string normalized = expr.name;
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
  std::string normalized = expr.name;
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
                                                  const InferExprKindWithLocalsFn *fallbackInferExprKind);

bool resolveBaseSetupResultExprInfo(const Expr &expr,
                                    const LocalMap &localsIn,
                                    const ResolveMethodCallWithLocalsFn *resolveMethodCall,
                                    const ResolveCallDefinitionFn *resolveDefinitionCall,
                                    const LookupReturnInfoFn *lookupReturnInfo,
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
      [resolveMethodCall, resolveDefinitionCall, lookupReturnInfo, fallbackInferExprKind](
          const Expr &candidate, const LocalMap &candidateLocals) {
        return inferBaseSetupSimpleExprKind(
            candidate, candidateLocals, resolveMethodCall, resolveDefinitionCall, lookupReturnInfo, fallbackInferExprKind);
      },
      out);
}

LocalInfo::ValueKind inferBaseSetupSimpleExprKind(const Expr &expr,
                                                  const LocalMap &localsIn,
                                                  const ResolveMethodCallWithLocalsFn *resolveMethodCall,
                                                  const ResolveCallDefinitionFn *resolveDefinitionCall,
                                                  const LookupReturnInfoFn *lookupReturnInfo,
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
      ResultExprInfo resultInfo;
      if (resolveBaseSetupResultExprInfo(
              expr, localsIn, resolveMethodCall, resolveDefinitionCall, lookupReturnInfo, fallbackInferExprKind, resultInfo) &&
          resultInfo.isResult &&
          resultInfo.hasValue) {
        return resultInfo.valueKind;
      }
      if (fallbackInferExprKind != nullptr && *fallbackInferExprKind && !isBaseSetupResultOrTryCall(expr)) {
        return (*fallbackInferExprKind)(expr, localsIn);
      }
      return LocalInfo::ValueKind::Unknown;
    }
    default:
      return LocalInfo::ValueKind::Unknown;
  }
}

bool returnInfoEquals(const ReturnInfo &left, const ReturnInfo &right) {
  return left.returnsVoid == right.returnsVoid && left.returnsArray == right.returnsArray &&
         left.kind == right.kind && left.isResult == right.isResult &&
         left.resultHasValue == right.resultHasValue &&
         left.resultValueKind == right.resultValueKind &&
         left.resultValueCollectionKind == right.resultValueCollectionKind &&
         left.resultErrorType == right.resultErrorType;
}

void sortDefinitionsForDeterministicReturnSolve(std::vector<const Definition *> &definitions) {
  std::stable_sort(definitions.begin(), definitions.end(), [](const Definition *left, const Definition *right) {
    const int leftLine = left->sourceLine > 0 ? left->sourceLine : std::numeric_limits<int>::max();
    const int rightLine = right->sourceLine > 0 ? right->sourceLine : std::numeric_limits<int>::max();
    if (leftLine != rightLine) {
      return leftLine < rightLine;
    }
    const int leftColumn = left->sourceColumn > 0 ? left->sourceColumn : std::numeric_limits<int>::max();
    const int rightColumn = right->sourceColumn > 0 ? right->sourceColumn : std::numeric_limits<int>::max();
    if (leftColumn != rightColumn) {
      return leftColumn < rightColumn;
    }
    return left->fullPath < right->fullPath;
  });
}

bool componentHasReturnInferenceCycle(const semantics::CondensationDagNode &componentNode) {
  return componentNode.memberNodeIds.size() > 1;
}

std::vector<const Definition *> collectReturnInfoComponentDefinitions(
    const semantics::TypeResolutionGraph &graph,
    const semantics::CondensationDagNode &componentNode,
    const std::unordered_map<std::string, const Definition *> &defMap) {
  std::vector<const Definition *> definitions;
  definitions.reserve(componentNode.memberNodeIds.size());
  for (uint32_t nodeId : componentNode.memberNodeIds) {
    if (nodeId >= graph.nodes.size()) {
      continue;
    }
    const semantics::TypeResolutionGraphNode &node = graph.nodes[nodeId];
    if (node.kind != semantics::TypeResolutionNodeKind::DefinitionReturn) {
      continue;
    }
    const auto defIt = defMap.find(node.resolvedPath);
    if (defIt == defMap.end() || defIt->second == nullptr) {
      continue;
    }
    definitions.push_back(defIt->second);
  }
  sortDefinitionsForDeterministicReturnSolve(definitions);
  return definitions;
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
        if (receiver.name == "FileError") {
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
              it->second.argsPackElementKind == LocalInfo::Kind::Value) {
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
  if (isSimpleCallName(expr, "File")) {
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
              it->second.argsPackElementKind == LocalInfo::Kind::Pointer) {
            kindOut = it->second.resultHasValue ? it->second.resultValueKind : LocalInfo::ValueKind::Int32;
            return true;
          }
        }
      }
      ResultExprInfo resultInfo;
      if (resolveBaseSetupResultExprInfo(
              arg, localsIn, resolveMethodCall, resolveDefinitionCall, lookupReturnInfo, fallbackInferExprKind, resultInfo) &&
          resultInfo.isResult) {
        kindOut = resultInfo.hasValue ? resultInfo.valueKind : LocalInfo::ValueKind::Int32;
        return true;
      }
    }
    if (arg.kind == Expr::Kind::Call) {
      if (!arg.isMethodCall && isSimpleCallName(arg, "File")) {
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

} // namespace
bool runLowerInferenceSetupBootstrap(const LowerInferenceSetupBootstrapInput &input,
                                     LowerInferenceSetupBootstrapState &stateOut,
                                     std::string &errorOut) {
  stateOut = LowerInferenceSetupBootstrapState{};

  if (input.defMap == nullptr) {
    errorOut = "native backend missing inference setup bootstrap dependency: defMap";
    return false;
  }
  if (input.importAliases == nullptr) {
    errorOut = "native backend missing inference setup bootstrap dependency: importAliases";
    return false;
  }
  if (input.structNames == nullptr) {
    errorOut = "native backend missing inference setup bootstrap dependency: structNames";
    return false;
  }
  if (!input.isArrayCountCall) {
    errorOut = "native backend missing inference setup bootstrap dependency: isArrayCountCall";
    return false;
  }
  if (!input.isVectorCapacityCall) {
    errorOut = "native backend missing inference setup bootstrap dependency: isVectorCapacityCall";
    return false;
  }
  if (!input.isEntryArgsName) {
    errorOut = "native backend missing inference setup bootstrap dependency: isEntryArgsName";
    return false;
  }
  if (!input.resolveExprPath) {
    errorOut = "native backend missing inference setup bootstrap dependency: resolveExprPath";
    return false;
  }
  if (!input.getBuiltinOperatorName) {
    errorOut = "native backend missing inference setup bootstrap dependency: getBuiltinOperatorName";
    return false;
  }

  const auto *defMap = input.defMap;
  const auto *importAliases = input.importAliases;
  const auto *structNames = input.structNames;
  const auto isArrayCountCall = input.isArrayCountCall;
  const auto isVectorCapacityCall = input.isVectorCapacityCall;
  const auto isEntryArgsName = input.isEntryArgsName;
  const auto resolveExprPath = input.resolveExprPath;
  const auto getBuiltinOperatorName = input.getBuiltinOperatorName;

  stateOut.resolveMethodCallDefinition = [defMap,
                                          importAliases,
                                          structNames,
                                          isArrayCountCall,
                                          isVectorCapacityCall,
                                          isEntryArgsName,
                                          resolveExprPath,
                                          &stateOut,
                                          &errorOut](const Expr &callExpr,
                                                     const LocalMap &localsIn) -> const Definition * {
    return resolveMethodCallDefinitionFromExpr(callExpr,
                                               localsIn,
                                               isArrayCountCall,
                                               isVectorCapacityCall,
                                               isEntryArgsName,
                                               *importAliases,
                                               *structNames,
                                               stateOut.inferExprKind,
                                               resolveExprPath,
                                               stateOut.getReturnInfo,
                                               *defMap,
                                               errorOut);
  };
  stateOut.resolveDefinitionCall = [defMap, resolveExprPath](const Expr &callExpr) -> const Definition * {
    return primec::ir_lowerer::resolveDefinitionCall(callExpr, *defMap, resolveExprPath);
  };

  stateOut.inferPointerTargetKind = [getBuiltinOperatorName](const Expr &expr,
                                                             const LocalMap &localsIn) -> LocalInfo::ValueKind {
    return inferPointerTargetValueKind(
        expr,
        localsIn,
        [&](const Expr &candidate, std::string &builtinName) {
          return getBuiltinOperatorName(candidate, builtinName);
        });
  };

  return true;
}

bool runLowerInferenceSetup(const LowerInferenceSetupInput &input,
                            LowerInferenceSetupBootstrapState &stateOut,
                            std::string &errorOut) {
  if (!runLowerInferenceSetupBootstrap(
          {
              .defMap = input.defMap,
              .importAliases = input.importAliases,
              .structNames = input.structNames,
              .isArrayCountCall = input.isArrayCountCall,
              .isVectorCapacityCall = input.isVectorCapacityCall,
              .isEntryArgsName = input.isEntryArgsName,
              .resolveExprPath = input.resolveExprPath,
              .getBuiltinOperatorName = input.getBuiltinOperatorName,
          },
          stateOut,
          errorOut)) {
    return false;
  }

  if (!runLowerInferenceArrayKindSetup(
          {
              .defMap = input.defMap,
              .resolveExprPath = input.resolveExprPath,
              .resolveStructArrayInfoFromPath = input.resolveStructArrayInfoFromPath,
              .isArrayCountCall = input.isArrayCountCall,
              .isStringCountCall = input.isStringCountCall,
          },
          stateOut,
          errorOut)) {
    return false;
  }
  if (!runLowerInferenceExprKindCallBaseSetup(
          {
              .inferStructExprPath = input.inferStructExprPath,
              .resolveStructFieldSlot = input.resolveStructFieldSlot,
              .resolveUninitializedStorage = input.resolveUninitializedStorage,
          },
          stateOut,
          errorOut)) {
    return false;
  }
  if (!runLowerInferenceExprKindCallReturnSetup(
          {
              .defMap = input.defMap,
              .resolveExprPath = input.resolveExprPath,
              .isArrayCountCall = input.isArrayCountCall,
              .isStringCountCall = input.isStringCountCall,
          },
          stateOut,
          errorOut)) {
    return false;
  }
  if (!runLowerInferenceExprKindCallFallbackSetup(
          {
              .defMap = input.defMap,
              .resolveExprPath = input.resolveExprPath,
              .isArrayCountCall = input.isArrayCountCall,
              .isStringCountCall = input.isStringCountCall,
              .isVectorCapacityCall = input.isVectorCapacityCall,
              .isEntryArgsName = input.isEntryArgsName,
              .inferStructExprPath = input.inferStructExprPath,
              .resolveStructFieldSlot = input.resolveStructFieldSlot,
          },
          stateOut,
          errorOut)) {
    return false;
  }
  if (!runLowerInferenceExprKindCallOperatorFallbackSetup(
          {
              .hasMathImport = input.hasMathImport,
              .combineNumericKinds = input.combineNumericKinds,
          },
          stateOut,
          errorOut)) {
    return false;
  }
  if (!runLowerInferenceExprKindCallControlFlowFallbackSetup(
          {
              .defMap = input.defMap,
              .resolveExprPath = input.resolveExprPath,
              .lowerMatchToIf = input.lowerMatchToIf,
              .combineNumericKinds = input.combineNumericKinds,
              .isBindingMutable = input.isBindingMutable,
              .bindingKind = input.bindingKind,
              .hasExplicitBindingTypeTransform = input.hasExplicitBindingTypeTransform,
              .bindingValueKind = input.bindingValueKind,
              .applyStructArrayInfo = input.applyStructArrayInfo,
              .applyStructValueInfo = input.applyStructValueInfo,
              .inferStructExprPath = input.inferStructExprPath,
          },
          stateOut,
          errorOut)) {
    return false;
  }
  if (!runLowerInferenceExprKindCallPointerFallbackSetup({}, stateOut, errorOut)) {
    return false;
  }
  if (!runLowerInferenceExprKindBaseSetup(
          {
              .getMathConstantName = input.getMathConstantName,
          },
          stateOut,
          errorOut)) {
    return false;
  }
  if (!runLowerInferenceExprKindDispatchSetup(
          {
              .defMap = input.defMap,
              .resolveExprPath = input.resolveExprPath,
              .error = &errorOut,
          },
          stateOut,
          errorOut)) {
    return false;
  }
  if (input.program == nullptr) {
    errorOut = "native backend missing inference setup dependency: program";
    return false;
  }

  auto &returnInfoCache = stateOut.returnInfoCache;
  auto &returnInferenceStack = stateOut.returnInferenceStack;
  auto &inferExprKind = stateOut.inferExprKind;
  auto &inferArrayElementKind = stateOut.inferArrayElementKind;
  return runLowerInferenceGetReturnInfoSetup(
      {
          .program = input.program,
          .defMap = input.defMap,
          .returnInfoCache = &returnInfoCache,
          .returnInferenceStack = &returnInferenceStack,
          .resolveStructTypeName = input.resolveStructTypeName,
          .resolveStructArrayInfoFromPath = input.resolveStructArrayInfoFromPath,
          .isBindingMutable = input.isBindingMutable,
          .bindingKind = input.bindingKind,
          .hasExplicitBindingTypeTransform = input.hasExplicitBindingTypeTransform,
          .bindingValueKind = input.bindingValueKind,
          .inferExprKind = inferExprKind,
          .isFileErrorBinding = input.isFileErrorBinding,
          .setReferenceArrayInfo = input.setReferenceArrayInfo,
          .applyStructArrayInfo = input.applyStructArrayInfo,
          .applyStructValueInfo = input.applyStructValueInfo,
          .inferStructExprPath = input.inferStructExprPath,
          .isStringBinding = input.isStringBinding,
          .inferArrayElementKind = inferArrayElementKind,
          .lowerMatchToIf = input.lowerMatchToIf,
          .error = &errorOut,
      },
      stateOut.getReturnInfo,
      errorOut);
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
  stateInOut.inferCallExprBaseKind =
      [inferStructExprPath, resolveStructFieldSlot, resolveUninitializedStorage, &stateInOut](
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
            &stateInOut.inferExprKind,
            kindOut);
      };
  return true;
}

} // namespace primec::ir_lowerer
