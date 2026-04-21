#include "IrLowererLowerInferenceSetup.h"

#include "IrLowererHelpers.h"
#include "IrLowererLowerInferenceBaseKindHelpers.h"
#include "IrLowererResultHelpers.h"
#include "IrLowererSemanticProductTargetAdapters.h"
#include "IrLowererSetupTypeCollectionHelpers.h"
#include "IrLowererSetupTypeHelpers.h"

namespace primec::ir_lowerer {

bool runLowerInferenceExprKindDispatchSetup(const LowerInferenceExprKindDispatchSetupInput &input,
                                            LowerInferenceSetupBootstrapState &stateInOut,
                                            std::string &errorOut) {
  if (input.defMap == nullptr) {
    errorOut = "native backend missing inference expr-kind dispatch setup dependency: defMap";
    return false;
  }
  if (!input.resolveExprPath) {
    errorOut = "native backend missing inference expr-kind dispatch setup dependency: resolveExprPath";
    return false;
  }
  if (input.error == nullptr) {
    errorOut = "native backend missing inference expr-kind dispatch setup dependency: error";
    return false;
  }
  if (!stateInOut.inferLiteralOrNameExprKind) {
    errorOut = "native backend missing inference expr-kind dispatch setup state: inferLiteralOrNameExprKind";
    return false;
  }
  if (!stateInOut.inferCallExprBaseKind) {
    errorOut = "native backend missing inference expr-kind dispatch setup state: inferCallExprBaseKind";
    return false;
  }
  if (!stateInOut.inferCallExprDirectReturnKind) {
    errorOut = "native backend missing inference expr-kind dispatch setup state: inferCallExprDirectReturnKind";
    return false;
  }
  if (!stateInOut.inferCallExprCountAccessGpuFallbackKind) {
    errorOut = "native backend missing inference expr-kind dispatch setup state: inferCallExprCountAccessGpuFallbackKind";
    return false;
  }
  if (!stateInOut.inferCallExprOperatorFallbackKind) {
    errorOut = "native backend missing inference expr-kind dispatch setup state: inferCallExprOperatorFallbackKind";
    return false;
  }
  if (!stateInOut.inferCallExprControlFlowFallbackKind) {
    errorOut = "native backend missing inference expr-kind dispatch setup state: inferCallExprControlFlowFallbackKind";
    return false;
  }
  if (!stateInOut.inferCallExprPointerFallbackKind) {
    errorOut = "native backend missing inference expr-kind dispatch setup state: inferCallExprPointerFallbackKind";
    return false;
  }

  const auto *defMap = input.defMap;
  const auto resolveExprPath = input.resolveExprPath;
  std::string *const inferenceError = input.error;
  const auto *semanticProgram = stateInOut.semanticProgram;
  const auto *semanticIndex = stateInOut.semanticIndex;
  stateInOut.inferExprKind = [defMap, resolveExprPath, inferenceError, semanticProgram, semanticIndex, &stateInOut](
                                 const Expr &expr, const LocalMap &localsIn) -> LocalInfo::ValueKind {
    if (!expr.isMethodCall && expr.kind == Expr::Kind::Call && !expr.args.empty()) {
      std::string canonicalMapHelperName;
      std::string borrowedMapHelperName;
      const bool hasBorrowedMapHelperAlias =
          resolveBorrowedMapHelperAliasName(expr, borrowedMapHelperName);
      if (resolveMapHelperAliasName(expr, canonicalMapHelperName) &&
          (!stateInOut.resolveDefinitionCall || stateInOut.resolveDefinitionCall(expr) == nullptr) &&
          !hasBorrowedMapHelperAlias &&
          (canonicalMapHelperName == "count" || canonicalMapHelperName == "contains" ||
           canonicalMapHelperName == "tryAt" || canonicalMapHelperName == "at" ||
           canonicalMapHelperName == "at_unsafe" ||
           canonicalMapHelperName == "insert") &&
          !(isExplicitMapHelperFallbackPath(expr) &&
            (canonicalMapHelperName == "at" || canonicalMapHelperName == "at_unsafe" ||
             canonicalMapHelperName == "tryAt")) &&
          ((expr.name.find('/') != std::string::npos) || !expr.namespacePrefix.empty() ||
           !expr.templateArgs.empty())) {
        Expr rewrittenExpr = expr;
        rewrittenExpr.name = canonicalMapHelperName;
        rewrittenExpr.namespacePrefix.clear();
        rewrittenExpr.semanticNodeId = 0;
        rewrittenExpr.templateArgs.clear();
        return stateInOut.inferExprKind(rewrittenExpr, localsIn);
      }
    }
    auto resolveTryValueKind = [&](const Expr &tryExpr, LocalInfo::ValueKind &kindOut) -> bool {
      kindOut = LocalInfo::ValueKind::Unknown;
      std::string semanticTryFactError;
      if (semanticProgram != nullptr && semanticIndex != nullptr && tryExpr.semanticNodeId != 0) {
        const auto *tryFact =
            findSemanticProductTryFactBySemanticId(*semanticIndex, tryExpr);
        if (tryFact != nullptr) {
          kindOut = valueKindFromTypeName(tryFact->valueType);
          if (kindOut == LocalInfo::ValueKind::Unknown && !tryFact->valueType.empty()) {
            kindOut = LocalInfo::ValueKind::Int64;
          }
          if (kindOut != LocalInfo::ValueKind::Unknown) {
            return true;
          }
          semanticTryFactError = "incomplete semantic-product try fact: try";
        } else {
          semanticTryFactError = "missing semantic-product try fact: try";
        }
      }
      if (tryExpr.kind != Expr::Kind::Call || tryExpr.args.size() != 1) {
        if (!semanticTryFactError.empty()) {
          *inferenceError = semanticTryFactError;
        }
        return false;
      }
      const Expr &resultExpr = tryExpr.args.front();
      auto resolveCallReceiverCollectionValueKind = [&](const Expr &receiverExpr,
                                                        LocalInfo::ValueKind &receiverKindOut) -> bool {
        receiverKindOut = LocalInfo::ValueKind::Unknown;
        if (receiverExpr.kind != Expr::Kind::Call) {
          return false;
        }
        std::string collectionName;
        if (getBuiltinCollectionName(receiverExpr, collectionName)) {
          if ((collectionName == "array" || collectionName == "vector" || collectionName == "soa_vector") &&
              receiverExpr.templateArgs.size() == 1) {
            receiverKindOut = valueKindFromTypeName(receiverExpr.templateArgs.front());
            return receiverKindOut != LocalInfo::ValueKind::Unknown;
          }
          if (collectionName == "map" && receiverExpr.templateArgs.size() == 2) {
            receiverKindOut = valueKindFromTypeName(receiverExpr.templateArgs.back());
            return receiverKindOut != LocalInfo::ValueKind::Unknown;
          }
        }
        if (defMap == nullptr || !resolveExprPath) {
          return false;
        }
        std::string receiverPath = resolveExprPath(receiverExpr);
        if (receiverPath.empty() && !receiverExpr.name.empty()) {
          receiverPath = receiverExpr.name;
          if (!receiverPath.empty() && receiverPath.front() != '/') {
            receiverPath.insert(receiverPath.begin(), '/');
          }
        }
        if (receiverPath.empty()) {
          return false;
        }
        auto defIt = defMap->find(receiverPath);
        if (defIt == defMap->end() || defIt->second == nullptr) {
          return false;
        }
        std::vector<std::string> collectionArgs;
        if (!inferDeclaredReturnCollection(*defIt->second, collectionName, collectionArgs)) {
          return false;
        }
        if ((collectionName == "array" || collectionName == "vector" || collectionName == "soa_vector") &&
            collectionArgs.size() == 1) {
          receiverKindOut = valueKindFromTypeName(collectionArgs.front());
          return receiverKindOut != LocalInfo::ValueKind::Unknown;
        }
        if (collectionName == "map" && collectionArgs.size() == 2) {
          receiverKindOut = valueKindFromTypeName(collectionArgs.back());
          return receiverKindOut != LocalInfo::ValueKind::Unknown;
        }
        return false;
      };
      if (resultExpr.kind == Expr::Kind::Name) {
        auto it = localsIn.find(resultExpr.name);
        if (it != localsIn.end() && it->second.isResult) {
          kindOut = it->second.resultHasValue ? it->second.resultValueKind : LocalInfo::ValueKind::Int32;
          return true;
        }
        if (!semanticTryFactError.empty()) {
          *inferenceError = semanticTryFactError;
        }
        return false;
      }
      if (resultExpr.kind == Expr::Kind::Call) {
        std::string accessName;
        if (getBuiltinArrayAccessName(resultExpr, accessName) && resultExpr.args.size() == 2 &&
            resultExpr.args.front().kind == Expr::Kind::Name) {
          auto it = localsIn.find(resultExpr.args.front().name);
          if (it != localsIn.end() && it->second.isArgsPack && it->second.isResult) {
            kindOut = it->second.resultHasValue ? it->second.resultValueKind : LocalInfo::ValueKind::Int32;
            return true;
          }
        }
        if (isSimpleCallName(resultExpr, "dereference") && resultExpr.args.size() == 1) {
          const Expr &targetExpr = resultExpr.args.front();
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
        if (stateInOut.getReturnInfo && stateInOut.inferExprKind) {
          auto resolveMethodCallDefinitionNoProbeError =
              [&](const Expr &candidate, const LocalMap &candidateLocals) -> const Definition * {
            if (!stateInOut.resolveMethodCallDefinition) {
              return nullptr;
            }
            return stateInOut.resolveMethodCallDefinition(candidate, candidateLocals);
          };
          auto resolveDefinitionCall = [&](const Expr &candidate) -> const Definition * {
            if (candidate.isMethodCall || defMap == nullptr || !resolveExprPath) {
              return nullptr;
            }
            std::string path = resolveExprPath(candidate);
            if (path.empty() && !candidate.name.empty()) {
              path = candidate.name;
              if (!path.empty() && path.front() != '/') {
                path.insert(path.begin(), '/');
              }
            }
            if (path.empty()) {
              return nullptr;
            }
            auto defIt = defMap->find(path);
            return defIt != defMap->end() ? defIt->second : nullptr;
          };

          ResultExprInfo resultInfo;
          if (resolveResultExprInfoFromLocals(resultExpr,
                                             localsIn,
                                             resolveMethodCallDefinitionNoProbeError,
                                             resolveDefinitionCall,
                                             stateInOut.getReturnInfo,
                                             stateInOut.inferExprKind,
                                             resultInfo) &&
              resultInfo.isResult) {
            kindOut = resultInfo.hasValue ? resultInfo.valueKind : LocalInfo::ValueKind::Int32;
            return true;
          }
        }
      }
      if (resultExpr.kind != Expr::Kind::Call) {
        if (!semanticTryFactError.empty()) {
          *inferenceError = semanticTryFactError;
        }
        return false;
      }
      if (isMapContainsCallName(resultExpr) && !resultExpr.args.empty()) {
        LocalInfo::ValueKind receiverCollectionKind = LocalInfo::ValueKind::Unknown;
        if (resolveCallReceiverCollectionValueKind(resultExpr.args.front(), receiverCollectionKind) &&
            receiverCollectionKind != LocalInfo::ValueKind::Unknown) {
          kindOut = LocalInfo::ValueKind::Bool;
          return true;
        }
      }
      if (isMapTryAtCallName(resultExpr) && !resultExpr.args.empty()) {
        LocalInfo::ValueKind receiverCollectionKind = LocalInfo::ValueKind::Unknown;
        if (resolveCallReceiverCollectionValueKind(resultExpr.args.front(), receiverCollectionKind) &&
            receiverCollectionKind != LocalInfo::ValueKind::Unknown) {
          kindOut = receiverCollectionKind;
          return true;
        }
      }
      if (inferMapContainsResultKind(resultExpr, localsIn, kindOut)) {
        return true;
      }
      if (inferMapTryAtResultValueKind(resultExpr, localsIn, kindOut)) {
        return true;
      }
      if (!resultExpr.isMethodCall && isSimpleCallName(resultExpr, "File")) {
        kindOut = LocalInfo::ValueKind::Int64;
        return true;
      }
      if (resultExpr.isMethodCall && !resultExpr.args.empty() && resultExpr.args.front().kind == Expr::Kind::Name) {
        auto it = localsIn.find(resultExpr.args.front().name);
        if (it != localsIn.end() && it->second.isFileHandle) {
          if (resultExpr.name == "write" || resultExpr.name == "write_line" || resultExpr.name == "write_byte" ||
              resultExpr.name == "write_bytes" || resultExpr.name == "flush" || resultExpr.name == "close") {
            kindOut = LocalInfo::ValueKind::Int32;
            return true;
          }
        }
        if (resultExpr.args.front().name == "Result") {
          if (resultExpr.name == "ok") {
            kindOut = resultExpr.args.size() > 1 ? stateInOut.inferExprKind(resultExpr.args[1], localsIn)
                                                 : LocalInfo::ValueKind::Int32;
            return true;
          }
          if (resultExpr.name == "error") {
            kindOut = LocalInfo::ValueKind::Bool;
            return true;
          }
          if (resultExpr.name == "why") {
            kindOut = LocalInfo::ValueKind::String;
            return true;
          }
        }
      }
      if (resultExpr.isMethodCall && !resultExpr.args.empty() &&
          isIndexedArgsPackFileHandleReceiver(resultExpr.args.front(), localsIn)) {
        if (resultExpr.name == "write" || resultExpr.name == "write_line" || resultExpr.name == "write_byte" ||
            resultExpr.name == "write_bytes" || resultExpr.name == "flush" || resultExpr.name == "close") {
          kindOut = LocalInfo::ValueKind::Int32;
          return true;
        }
      }
      if (resultExpr.isMethodCall && !resultExpr.args.empty() &&
          isIndexedBorrowedArgsPackFileHandleReceiver(resultExpr.args.front(), localsIn)) {
        if (resultExpr.name == "write" || resultExpr.name == "write_line" || resultExpr.name == "write_byte" ||
            resultExpr.name == "write_bytes" || resultExpr.name == "flush" || resultExpr.name == "close") {
          kindOut = LocalInfo::ValueKind::Int32;
          return true;
        }
      }
      if (resultExpr.isMethodCall && !resultExpr.args.empty() &&
          isIndexedPointerArgsPackFileHandleReceiver(resultExpr.args.front(), localsIn)) {
        if (resultExpr.name == "write" || resultExpr.name == "write_line" || resultExpr.name == "write_byte" ||
            resultExpr.name == "write_bytes" || resultExpr.name == "flush" || resultExpr.name == "close") {
          kindOut = LocalInfo::ValueKind::Int32;
          return true;
        }
      }

      const Definition *callee = nullptr;
      if (resultExpr.isMethodCall) {
        callee = stateInOut.resolveMethodCallDefinition(resultExpr, localsIn);
      } else {
        const std::string path = resolveExprPath(resultExpr);
        auto defIt = defMap->find(path);
        if (defIt != defMap->end()) {
          callee = defIt->second;
        }
      }
      if (callee == nullptr) {
        if (!semanticTryFactError.empty()) {
          *inferenceError = semanticTryFactError;
        }
        return false;
      }

      ReturnInfo returnInfo;
      if (!stateInOut.getReturnInfo(callee->fullPath, returnInfo) || !returnInfo.isResult) {
        if (!semanticTryFactError.empty()) {
          *inferenceError = semanticTryFactError;
        }
        return false;
      }
      kindOut = returnInfo.resultHasValue ? returnInfo.resultValueKind : LocalInfo::ValueKind::Int32;
      return true;
    };

    LocalInfo::ValueKind literalOrNameKind = LocalInfo::ValueKind::Unknown;
    if (stateInOut.inferLiteralOrNameExprKind(expr, localsIn, literalOrNameKind)) {
      return literalOrNameKind;
    }
    switch (expr.kind) {
      case Expr::Kind::Call: {
        auto isBuiltinCountLikeCall = [](const Expr &candidate) {
          if (candidate.kind != Expr::Kind::Call) {
            return false;
          }
          if (isSimpleCallName(candidate, "count")) {
            return true;
          }
          std::string collectionName;
          std::string helperName;
          return getNamespacedCollectionHelperName(candidate, collectionName, helperName) && helperName == "count";
        };

        if (isSimpleCallName(expr, "try") && expr.args.size() == 1) {
          LocalInfo::ValueKind tryValueKind = LocalInfo::ValueKind::Unknown;
          if (resolveTryValueKind(expr, tryValueKind) && tryValueKind != LocalInfo::ValueKind::Unknown) {
            return tryValueKind;
          }
          if (!inferenceError->empty()) {
            return LocalInfo::ValueKind::Unknown;
          }
        }

        LocalInfo::ValueKind callBaseKind = LocalInfo::ValueKind::Unknown;
        if (stateInOut.inferCallExprBaseKind(expr, localsIn, callBaseKind)) {
          return callBaseKind;
        }
        std::string builtinOperatorName;
        if ((getBuiltinComparisonName(expr, builtinOperatorName) ||
             getBuiltinOperatorName(expr, builtinOperatorName))) {
          LocalInfo::ValueKind operatorFallbackKind = LocalInfo::ValueKind::Unknown;
          if (stateInOut.inferCallExprOperatorFallbackKind(expr, localsIn, operatorFallbackKind)) {
            return operatorFallbackKind;
          }
        }
        LocalInfo::ValueKind callReturnKind = LocalInfo::ValueKind::Unknown;
        const auto callReturnResolution = stateInOut.inferCallExprDirectReturnKind(expr, localsIn, callReturnKind);
        if (callReturnResolution == CallExpressionReturnKindResolution::Resolved) {
          return callReturnKind;
        }
        LocalInfo::ValueKind callFallbackKind = LocalInfo::ValueKind::Unknown;
        if (stateInOut.inferCallExprCountAccessGpuFallbackKind(expr, localsIn, callFallbackKind)) {
          return callFallbackKind;
        }
        if (isBuiltinCountLikeCall(expr) && expr.args.size() == 1 && expr.args.front().kind == Expr::Kind::Call) {
          std::string accessName;
          const Expr &accessExpr = expr.args.front();
          if (getBuiltinArrayAccessName(accessExpr, accessName) && accessExpr.args.size() == 2) {
            const Expr &accessTarget = accessExpr.args.front();
            if (accessTarget.kind == Expr::Kind::Name) {
              auto it = localsIn.find(accessTarget.name);
              if (it != localsIn.end() &&
                  ((it->second.kind == LocalInfo::Kind::Map) ||
                   (it->second.kind == LocalInfo::Kind::Reference && it->second.referenceToMap)) &&
                  it->second.mapValueKind == LocalInfo::ValueKind::String) {
                return LocalInfo::ValueKind::Int32;
              }
            } else if (accessTarget.kind == Expr::Kind::Call) {
              std::string collectionName;
              if (getBuiltinCollectionName(accessTarget, collectionName) && collectionName == "map" &&
                  accessTarget.templateArgs.size() == 2) {
                const std::string &valueType = accessTarget.templateArgs[1];
                if (valueType == "string" || valueType == "/string") {
                  return LocalInfo::ValueKind::Int32;
                }
              }
            }
          }
        }
        if (callReturnResolution == CallExpressionReturnKindResolution::MatchedButUnsupported) {
          return LocalInfo::ValueKind::Unknown;
        }
        LocalInfo::ValueKind operatorFallbackKind = LocalInfo::ValueKind::Unknown;
        if (stateInOut.inferCallExprOperatorFallbackKind(expr, localsIn, operatorFallbackKind)) {
          return operatorFallbackKind;
        }
        LocalInfo::ValueKind controlFlowKind = LocalInfo::ValueKind::Unknown;
        if (stateInOut.inferCallExprControlFlowFallbackKind(expr, localsIn, *inferenceError, controlFlowKind)) {
          return controlFlowKind;
        }
        LocalInfo::ValueKind pointerFallbackKind = LocalInfo::ValueKind::Unknown;
        if (stateInOut.inferCallExprPointerFallbackKind(expr, localsIn, pointerFallbackKind)) {
          return pointerFallbackKind;
        }
        return LocalInfo::ValueKind::Unknown;
      }
      default:
        return LocalInfo::ValueKind::Unknown;
    }
  };
  return true;
}

} // namespace primec::ir_lowerer
