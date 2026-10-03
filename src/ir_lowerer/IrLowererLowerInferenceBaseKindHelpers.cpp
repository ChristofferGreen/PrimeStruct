#include "primec/ir_lowerer/IrLowererLowerInferenceBaseKindHelpers.h"

#include "primec/ir_lowerer/IrLowererHelpers.h"
#include "primec/ir_lowerer/IrLowererResultHelpers.h"
#include "primec/ir_lowerer/IrLowererSemanticProductTargetAdapters.h"
#include "primec/ir_lowerer/IrLowererSetupTypeCollectionHelpers.h"
#include "primec/ir_lowerer/IrLowererSetupTypeHelpers.h"
#include "primec/ir_lowerer/IrLowererTemplateTypeParseHelpers.h"

#include <vector>
#include "IrLowererLowerInferenceBaseKindInternal.h"

namespace primec::ir_lowerer {
using namespace base_kind_internal;

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
  return normalized == canonicalKeyValueHelperPath("tryAt", false);
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
  return normalized == canonicalKeyValueHelperPath("contains", false);
}

bool inferMapTryAtResultValueKind(const Expr &expr,
                                  const LocalMap &localsIn,
                                  LocalInfo::ValueKind &kindOut,
                                  const SemanticProgram *semanticProgram,
                                  const SemanticProductIndex *semanticIndex) {
  kindOut = LocalInfo::ValueKind::Unknown;
  if (expr.kind != Expr::Kind::Call || expr.args.empty() || !isMapTryAtCallName(expr)) {
    return false;
  }
  const auto targetInfo =
      resolveCollectionPairTypeInfo(expr.args.front(), localsIn, {}, semanticProgram, semanticIndex);
  if (!targetInfo.isKeyValueTarget || targetInfo.keyValueValueKind == LocalInfo::ValueKind::Unknown) {
    return false;
  }
  kindOut = targetInfo.keyValueValueKind;
  return true;
}

bool inferMapContainsResultKind(const Expr &expr,
                                const LocalMap &localsIn,
                                LocalInfo::ValueKind &kindOut,
                                const SemanticProgram *semanticProgram,
                                const SemanticProductIndex *semanticIndex) {
  kindOut = LocalInfo::ValueKind::Unknown;
  if (expr.kind != Expr::Kind::Call || expr.args.empty() || !isMapContainsCallName(expr)) {
    return false;
  }
  const auto targetInfo =
      resolveCollectionPairTypeInfo(expr.args.front(), localsIn, {}, semanticProgram, semanticIndex);
  if (!targetInfo.isKeyValueTarget) {
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
        kindOut = (it->second.referenceToArray || it->second.referenceToVector ||
                   !it->second.structTypeName.empty())
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
                               const ExprLocalsValueKindFn *fallbackInferExprKind,
                               LocalInfo::ValueKind &kindOut) {
  kindOut = LocalInfo::ValueKind::Unknown;
  if (expr.kind != Expr::Kind::Call) {
    return false;
  }
  if (expr.isFieldAccess) {
    if (expr.args.size() != 1) {
      return true;
    }
    if (inferBaseSetupSemanticQueryFactValueKind(
            expr, semanticProgram, semanticIndex, kindOut)) {
      return true;
    }
    const Expr &receiver = expr.args.front();
    bool hasSemanticFieldReceiver = false;
    if (inferBaseSetupSemanticFieldAccessKind(receiver,
                                             expr.name,
                                             resolveStructFieldSlot,
                                             semanticProgram,
                                             semanticIndex,
                                             kindOut,
                                             hasSemanticFieldReceiver)) {
      return true;
    }
    if (hasSemanticFieldReceiver) {
      return true;
    }
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
  if (inferBaseSetupSemanticQueryFactValueKind(
          expr, semanticProgram, semanticIndex, kindOut)) {
    return true;
  }
  if (!expr.isMethodCall && (isSimpleCallName(expr, "take") || isSimpleCallName(expr, "borrow")) &&
      expr.args.size() == 1) {
    bool hasSemanticTakeBorrowQuery = false;
    if (inferBaseSetupSemanticQueryFactValueKindWithPresence(
            expr,
            semanticProgram,
            semanticIndex,
            kindOut,
            hasSemanticTakeBorrowQuery)) {
      return true;
    }
    if (hasSemanticTakeBorrowQuery ||
        hasSemanticProductBaseSetupSite(expr, semanticProgram, semanticIndex)) {
      return true;
    }
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
  bool hasSemanticTrySite = false;
  if (inferBaseSetupSemanticTryFactValueKind(
          expr, semanticProgram, semanticIndex, kindOut, hasSemanticTrySite)) {
    return true;
  }
  if (hasSemanticTrySite) {
    return true;
  }
  if (inferBaseSetupResultTypeCallKind(expr,
                                       localsIn,
                                       resolveMethodCall,
                                       resolveDefinitionCall,
                                       lookupReturnInfo,
                                       semanticProgram,
                                       semanticIndex,
                                       fallbackInferExprKind,
                                       kindOut)) {
    return true;
  }
  if (expr.isMethodCall) {
    if (!expr.args.empty() && expr.args.front().kind == Expr::Kind::Name && expr.args.front().name == "Result") {
      ResultExprInfo resultInfo;
      if (hasSemanticProductResultMethodFactContext(expr, semanticProgram, semanticIndex) &&
          expr.name == "ok" &&
          resolveBaseSetupResultExprInfo(
              expr,
              localsIn,
              resolveMethodCall,
              resolveDefinitionCall,
              lookupReturnInfo,
              semanticProgram,
              semanticIndex,
              fallbackInferExprKind,
              resultInfo) &&
          resultInfo.isResult) {
        if (!resultInfo.hasValue) {
          kindOut = LocalInfo::ValueKind::Int32;
          return true;
        }
        if (resultInfo.valueKind != LocalInfo::ValueKind::Unknown) {
          kindOut = resultInfo.valueKind;
        }
        return true;
      }
      if (hasSemanticProductBaseSetupSite(expr, semanticProgram, semanticIndex) &&
          isBaseSetupResultTypeMethodCall(expr)) {
        if (inferBaseSetupSemanticQueryFactValueKind(
                expr, semanticProgram, semanticIndex, kindOut)) {
          return true;
        }
        return true;
      }
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
      bool hasSemanticReceiver = false;
      if (inferBaseSetupSemanticFileErrorWhyKind(receiver,
                                                semanticProgram,
                                                semanticIndex,
                                                kindOut,
                                                hasSemanticReceiver)) {
        return true;
      }
      if (hasSemanticReceiver) {
        return true;
      }
      bool hasSemanticDereferenceTarget = false;
      if (inferBaseSetupSemanticDereferencedFileErrorWhyKind(
              receiver,
              semanticProgram,
              semanticIndex,
              kindOut,
              hasSemanticDereferenceTarget)) {
        return true;
      }
      if (hasSemanticDereferenceTarget) {
        return true;
      }
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
      // TODO-5302 round 7: these two structural-only fallbacks (a bare or
      // dereferenced indexed args-pack `FileError` element) trust the
      // receiver's raw `LocalInfo` shape alone, with no semantic
      // corroboration at all. That is safe for every real compiled program
      // (`IrLowererLower.cpp` hard-errors before lowering when
      // `semanticProgram` is null), but is exactly the "no structural
      // corroboration available" staleness signal this cluster's other
      // fixes gate on - so require a semantic context to exist before
      // trusting it, matching the un-annotated/no-semantics-at-all unit
      // test cases that must defer here instead of silently resolving.
      if (receiver.kind == Expr::Kind::Call && semanticProgram != nullptr) {
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
    if (!expr.args.empty()) {
      const Expr &receiver = expr.args.front();
      bool hasSemanticFileHandleReceiver = false;
      if (inferBaseSetupSemanticFileHandleMethodKind(receiver,
                                                    expr.name,
                                                    semanticProgram,
                                                    semanticIndex,
                                                    kindOut,
                                                    hasSemanticFileHandleReceiver)) {
        return true;
      }
      if (hasSemanticFileHandleReceiver) {
        return true;
      }
      bool hasSemanticFileHandleTarget = false;
      if (inferBaseSetupSemanticDereferencedFileHandleMethodKind(
              receiver,
              expr.name,
              semanticProgram,
              semanticIndex,
              kindOut,
              hasSemanticFileHandleTarget)) {
        return true;
      }
      if (hasSemanticFileHandleTarget) {
        return true;
      }
    }
    if (!expr.args.empty() && expr.args.front().kind == Expr::Kind::Name) {
      const Expr &receiver = expr.args.front();
      auto it = localsIn.find(receiver.name);
      if (it != localsIn.end() && it->second.isFileHandle) {
        if (isBaseSetupFileHandleMethodName(expr.name)) {
          kindOut = LocalInfo::ValueKind::Int32;
          return true;
        }
      }
    }
    // TODO-5302 round 7: same "structural-only, no semantic corroboration"
    // shape as the `FileError`/`why` fallback above - gate on a semantic
    // context existing at all, which every real compiled program has.
    if (semanticProgram != nullptr) {
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
    }
    return false;
  }
  bool hasSemanticFileHandleCall = false;
  if (inferBaseSetupSemanticFileHandleCallKind(
          expr, semanticProgram, semanticIndex, kindOut, hasSemanticFileHandleCall)) {
    return true;
  }
  if (hasSemanticFileHandleCall) {
    return true;
  }
  if (isFileHandleCall(expr)) {
    kindOut = LocalInfo::ValueKind::Int64;
    return true;
  }
  if (isSimpleCallName(expr, "try") && expr.args.size() == 1) {
    const Expr &arg = expr.args.front();
    if (arg.kind == Expr::Kind::Call) {
      bool hasSemanticFileHandleArg = false;
      if (inferBaseSetupSemanticFileHandleCallKind(
              arg, semanticProgram, semanticIndex, kindOut, hasSemanticFileHandleArg)) {
        return true;
      }
      if (hasSemanticFileHandleArg) {
        return true;
      }
    }
    bool hasSemanticResultOperand = false;
    if (inferBaseSetupSemanticTryOperandResultKind(
            arg, semanticProgram, semanticIndex, kindOut, hasSemanticResultOperand)) {
      return true;
    }
    if (hasSemanticResultOperand) {
      return true;
    }
    bool hasSemanticDereferencedResultOperand = false;
    if (inferBaseSetupSemanticDereferencedTryOperandResultKind(
            arg,
            semanticProgram,
            semanticIndex,
            kindOut,
            hasSemanticDereferencedResultOperand)) {
      return true;
    }
    if (hasSemanticDereferencedResultOperand) {
      return true;
    }
    if (arg.kind == Expr::Kind::Call && arg.isMethodCall && !arg.args.empty()) {
      bool hasSemanticFileHandleReceiver = false;
      if (inferBaseSetupSemanticFileHandleMethodKind(arg.args.front(),
                                                    arg.name,
                                                    semanticProgram,
                                                    semanticIndex,
                                                    kindOut,
                                                    hasSemanticFileHandleReceiver)) {
        return true;
      }
      if (hasSemanticFileHandleReceiver) {
        return true;
      }
    }
    if (arg.kind == Expr::Kind::Name) {
      auto it = localsIn.find(arg.name);
      if (it != localsIn.end() && it->second.isResult) {
        kindOut = it->second.resultHasValue ? it->second.resultValueKind : LocalInfo::ValueKind::Int32;
        return true;
      }
    }
    if (arg.kind == Expr::Kind::Call) {
      if (isBaseSetupResultTypeMethodCall(arg) &&
          hasSemanticProductResultMethodFactContext(arg, semanticProgram, semanticIndex)) {
        ResultExprInfo resultInfo;
        if (arg.name == "ok" &&
            resolveBaseSetupResultExprInfo(
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
          if (!resultInfo.hasValue) {
            kindOut = LocalInfo::ValueKind::Int32;
            return true;
          }
          if (resultInfo.valueKind != LocalInfo::ValueKind::Unknown) {
            kindOut = resultInfo.valueKind;
          }
          return true;
        }
        return true;
      }
      if (isMapTryAtCallName(arg) && !arg.args.empty()) {
        bool hasSemanticMapReceiver = false;
        if (inferBaseSetupSemanticMapTryAtReceiverKind(arg.args.front(),
                                                       semanticProgram,
                                                       semanticIndex,
                                                       kindOut,
                                                       hasSemanticMapReceiver)) {
          return true;
        }
        if (hasSemanticMapReceiver) {
          return true;
        }
      }
      if (inferMapTryAtResultValueKind(
              arg, localsIn, kindOut, semanticProgram, semanticIndex)) {
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
      bool hasSemanticFileHandleArg = false;
      if (inferBaseSetupSemanticFileHandleCallKind(
              arg, semanticProgram, semanticIndex, kindOut, hasSemanticFileHandleArg)) {
        return true;
      }
      if (hasSemanticFileHandleArg) {
        return true;
      }
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
