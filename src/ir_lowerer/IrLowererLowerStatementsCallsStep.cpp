#include "IrLowererLowerStatementsCallsStep.h"

#include "IrLowererBindingTransformHelpers.h"
#include "IrLowererBindingTypeHelpers.h"
#include "IrLowererFlowHelpers.h"
#include "IrLowererSemanticProductTargetAdapters.h"
#include "IrLowererTemplateTypeParseHelpers.h"

namespace primec::ir_lowerer {

namespace {

enum class CallsStepVectorHelperReceiverFact {
  Unknown,
  Vector,
  NonVector,
};

bool isCallsStepVectorMutatorMethod(const Expr &candidate) {
  return candidate.isMethodCall && candidate.namespacePrefix.empty() &&
         !candidate.args.empty() &&
         (candidate.name == "push" || candidate.name == "pop" ||
          candidate.name == "reserve" || candidate.name == "clear" ||
          candidate.name == "remove_at" || candidate.name == "remove_swap") &&
         candidate.args.front().kind == Expr::Kind::Name;
}

bool isCallsStepBuiltinVectorMutatorCall(const Expr &candidate) {
  if (candidate.isMethodCall || candidate.name.empty()) {
    return false;
  }
  std::string path = candidate.name;
  if (path.find('/') == std::string::npos && !candidate.namespacePrefix.empty()) {
    path = candidate.namespacePrefix == "/" ? "/" + path
                                            : candidate.namespacePrefix + "/" + path;
  }
  if (!path.empty() && path.front() == '/') {
    path.erase(path.begin());
  }
  const std::string experimentalPrefix = "std/collections/experimental_vector/";
  const std::string internalPrefix = "std/collections/internal_vector/";
  std::string leaf;
  if (path.rfind(experimentalPrefix, 0) == 0) {
    leaf = path.substr(experimentalPrefix.size());
  } else if (path.rfind(internalPrefix, 0) == 0) {
    leaf = path.substr(internalPrefix.size());
  } else {
    return false;
  }
  const size_t generatedSuffix = leaf.find("__");
  if (generatedSuffix != std::string::npos) {
    leaf.erase(generatedSuffix);
  }
  return leaf == "vectorPush" || leaf == "vectorPop" ||
         leaf == "vectorReserve" || leaf == "vectorClear" ||
         leaf == "vectorRemoveAt" || leaf == "vectorRemoveSwap" ||
         leaf == "push" || leaf == "pop" || leaf == "reserve" ||
         leaf == "clear" || leaf == "remove_at" || leaf == "remove_swap";
}

bool isCallsStepExperimentalVectorTypeName(const std::string &typeName) {
  const std::string normalized = trimTemplateTypeText(typeName);
  return normalized == "Vector" ||
         normalized == "std/collections/experimental_vector/Vector" ||
         normalized == "/std/collections/experimental_vector/Vector" ||
         normalized.rfind("std/collections/experimental_vector/Vector__", 0) == 0 ||
         normalized.rfind("/std/collections/experimental_vector/Vector__", 0) == 0;
}

bool isCallsStepVectorTypeText(const std::string &typeText) {
  std::string base;
  std::string argText;
  const std::string normalizedTypeText =
      unwrapTopLevelUninitializedTypeText(trimTemplateTypeText(typeText));
  if (splitTemplateTypeName(normalizedTypeText, base, argText)) {
    const std::string normalizedBase =
        normalizeCollectionBindingTypeName(trimTemplateTypeText(base));
    if (normalizedBase == "Reference" || normalizedBase == "/Reference" ||
        normalizedBase == "Pointer" || normalizedBase == "/Pointer") {
      return isCallsStepVectorTypeText(argText);
    }
    return normalizedBase == "vector" ||
           isCallsStepExperimentalVectorTypeName(trimTemplateTypeText(base));
  }
  return normalizeCollectionBindingTypeName(normalizedTypeText) == "vector" ||
         isCallsStepExperimentalVectorTypeName(normalizedTypeText);
}

CallsStepVectorHelperReceiverFact classifyCallsStepVectorHelperReceiverTypeText(
    const SemanticProgram *semanticProgram,
    SymbolId typeTextId,
    const std::string &typeText) {
  const std::string resolvedTypeText =
      resolveSemanticProductTypeText(semanticProgram, typeText, typeTextId);
  return isCallsStepVectorTypeText(resolvedTypeText)
             ? CallsStepVectorHelperReceiverFact::Vector
             : CallsStepVectorHelperReceiverFact::NonVector;
}

CallsStepVectorHelperReceiverFact combineCallsStepVectorHelperReceiverFacts(
    CallsStepVectorHelperReceiverFact lhs,
    CallsStepVectorHelperReceiverFact rhs) {
  if (lhs == CallsStepVectorHelperReceiverFact::Vector ||
      rhs == CallsStepVectorHelperReceiverFact::Vector) {
    return CallsStepVectorHelperReceiverFact::Vector;
  }
  if (lhs == CallsStepVectorHelperReceiverFact::NonVector ||
      rhs == CallsStepVectorHelperReceiverFact::NonVector) {
    return CallsStepVectorHelperReceiverFact::NonVector;
  }
  return CallsStepVectorHelperReceiverFact::Unknown;
}

CallsStepVectorHelperReceiverFact classifyCallsStepVectorHelperReceiverFromSemanticFacts(
    const Expr &receiverExpr,
    const SemanticProgram *semanticProgram,
    const SemanticProductIndex *semanticIndex) {
  if (semanticProgram == nullptr || semanticIndex == nullptr ||
      receiverExpr.semanticNodeId == 0) {
    return CallsStepVectorHelperReceiverFact::Unknown;
  }

  if (const auto *collectionFact =
          findSemanticProductCollectionSpecialization(*semanticIndex, receiverExpr);
      collectionFact != nullptr) {
    const std::string collectionFamily =
        resolveSemanticProductTypeText(semanticProgram,
                                       collectionFact->collectionFamily,
                                       collectionFact->collectionFamilyId);
    return normalizeCollectionBindingTypeName(collectionFamily) == "vector"
               ? CallsStepVectorHelperReceiverFact::Vector
               : CallsStepVectorHelperReceiverFact::NonVector;
  }

  if (const auto *queryFact =
          findSemanticProductQueryFact(semanticProgram, *semanticIndex, receiverExpr);
      queryFact != nullptr) {
    CallsStepVectorHelperReceiverFact fact =
        classifyCallsStepVectorHelperReceiverTypeText(semanticProgram,
                                                      queryFact->bindingTypeTextId,
                                                      queryFact->bindingTypeText);
    fact = combineCallsStepVectorHelperReceiverFacts(
        fact,
        classifyCallsStepVectorHelperReceiverTypeText(semanticProgram,
                                                      queryFact->queryTypeTextId,
                                                      queryFact->queryTypeText));
    return combineCallsStepVectorHelperReceiverFacts(
        fact,
        classifyCallsStepVectorHelperReceiverTypeText(
            semanticProgram,
            queryFact->receiverBindingTypeTextId,
            queryFact->receiverBindingTypeText));
  }

  if (const auto *bindingFact =
          findSemanticProductBindingFact(*semanticIndex, receiverExpr);
      bindingFact != nullptr) {
    return classifyCallsStepVectorHelperReceiverTypeText(
        semanticProgram,
        bindingFact->bindingTypeTextId,
        bindingFact->bindingTypeText);
  }

  if (const auto *localAutoFact =
          findSemanticProductLocalAutoFact(semanticProgram, *semanticIndex, receiverExpr);
      localAutoFact != nullptr) {
    return classifyCallsStepVectorHelperReceiverTypeText(
        semanticProgram,
        localAutoFact->bindingTypeTextId,
        localAutoFact->bindingTypeText);
  }

  return CallsStepVectorHelperReceiverFact::Unknown;
}

} // namespace

bool runLowerStatementsCallsStep(const LowerStatementsCallsStepInput &input,
                                 const Expr &stmt,
                                 const LocalMap &localsIn,
                                 std::string &errorOut) {
  if (!input.inferExprKind) {
    errorOut = "native backend missing statements/calls step dependency: inferExprKind";
    return false;
  }
  if (!input.inferStructExprPath) {
    errorOut = "native backend missing statements/calls step dependency: inferStructExprPath";
    return false;
  }
  if (!input.emitExpr) {
    errorOut = "native backend missing statements/calls step dependency: emitExpr";
    return false;
  }
  if (!input.allocTempLocal) {
    errorOut = "native backend missing statements/calls step dependency: allocTempLocal";
    return false;
  }
  if (!input.resolveExprPath) {
    errorOut = "native backend missing statements/calls step dependency: resolveExprPath";
    return false;
  }
  if (!input.findDefinitionByPath) {
    errorOut = "native backend missing statements/calls step dependency: findDefinitionByPath";
    return false;
  }
  if (!input.resolveDestroyHelperForStruct) {
    errorOut = "native backend missing statements/calls step dependency: resolveDestroyHelperForStruct";
    return false;
  }
  if (!input.resolveMoveHelperForStruct) {
    errorOut = "native backend missing statements/calls step dependency: resolveMoveHelperForStruct";
    return false;
  }
  if (!input.isArrayCountCall) {
    errorOut = "native backend missing statements/calls step dependency: isArrayCountCall";
    return false;
  }
  if (!input.isStringCountCall) {
    errorOut = "native backend missing statements/calls step dependency: isStringCountCall";
    return false;
  }
  if (!input.isVectorCapacityCall) {
    errorOut = "native backend missing statements/calls step dependency: isVectorCapacityCall";
    return false;
  }
  if (!input.resolveMethodCallDefinition) {
    errorOut = "native backend missing statements/calls step dependency: resolveMethodCallDefinition";
    return false;
  }
  if (!input.resolveDefinitionCall) {
    errorOut = "native backend missing statements/calls step dependency: resolveDefinitionCall";
    return false;
  }
  if (!input.getReturnInfo) {
    errorOut = "native backend missing statements/calls step dependency: getReturnInfo";
    return false;
  }
  if (!input.emitInlineDefinitionCall) {
    errorOut = "native backend missing statements/calls step dependency: emitInlineDefinitionCall";
    return false;
  }
  if (!input.emitArrayIndexOutOfBounds) {
    errorOut = "native backend missing statements/calls step dependency: emitArrayIndexOutOfBounds";
    return false;
  }
  if (!input.emitVectorCapacityExceeded) {
    errorOut = "native backend missing statements/calls step dependency: emitVectorCapacityExceeded";
    return false;
  }
  if (!input.emitVectorPopOnEmpty) {
    errorOut = "native backend missing statements/calls step dependency: emitVectorPopOnEmpty";
    return false;
  }
  if (!input.emitVectorIndexOutOfBounds) {
    errorOut = "native backend missing statements/calls step dependency: emitVectorIndexOutOfBounds";
    return false;
  }
  if (!input.emitVectorReserveNegative) {
    errorOut = "native backend missing statements/calls step dependency: emitVectorReserveNegative";
    return false;
  }
  if (!input.emitVectorReserveExceeded) {
    errorOut = "native backend missing statements/calls step dependency: emitVectorReserveExceeded";
    return false;
  }
  if (input.instructions == nullptr) {
    errorOut = "native backend missing statements/calls step dependency: instructions";
    return false;
  }

  auto &instructions = *input.instructions;
  const auto bufferStoreResult = tryEmitBufferStoreStatement(
      stmt,
      localsIn,
      [&](const Expr &valueExpr, const LocalMap &valueLocals) { return input.inferExprKind(valueExpr, valueLocals); },
      [&](const Expr &valueExpr, const LocalMap &valueLocals) { return input.emitExpr(valueExpr, valueLocals); },
      [&]() { return input.allocTempLocal(); },
      instructions,
      errorOut,
      input.semanticProgram,
      input.semanticIndex);
  if (bufferStoreResult == BufferStoreStatementEmitResult::Error) {
    return false;
  }
  if (bufferStoreResult == BufferStoreStatementEmitResult::Emitted) {
    return true;
  }

  const auto dispatchResult = tryEmitDispatchStatement(
      stmt,
      localsIn,
      [&](const Expr &valueExpr) { return input.resolveExprPath(valueExpr); },
      [&](const std::string &path) { return input.findDefinitionByPath(path); },
      [&](const Expr &valueExpr, const LocalMap &valueLocals) { return input.inferExprKind(valueExpr, valueLocals); },
      [&](const Expr &valueExpr, const LocalMap &valueLocals) { return input.emitExpr(valueExpr, valueLocals); },
      [&]() { return input.allocTempLocal(); },
      [&](const Expr &callExpr, const Definition &callee, const LocalMap &callLocals, bool expectValue) {
        return input.emitInlineDefinitionCall(callExpr, callee, callLocals, expectValue);
      },
      instructions,
      errorOut,
      input.semanticProgram,
      input.semanticIndex);
  if (dispatchResult == DispatchStatementEmitResult::Error) {
    return false;
  }
  if (dispatchResult == DispatchStatementEmitResult::Emitted) {
    return true;
  }

  const auto vectorHelperResult = tryEmitVectorStatementHelper(
      stmt,
      localsIn,
      instructions,
      [&]() { return input.allocTempLocal(); },
      [&](const Expr &valueExpr, const LocalMap &valueLocals) {
        return input.inferExprKind(valueExpr, valueLocals);
      },
      [&](const Expr &valueExpr, const LocalMap &valueLocals) {
        return input.inferStructExprPath(valueExpr, valueLocals);
      },
      [&](const Expr &valueExpr, const LocalMap &valueLocals) {
        return input.emitExpr(valueExpr, valueLocals);
      },
      [&](const std::string &structPath) {
        return input.resolveDestroyHelperForStruct(structPath);
      },
      [&](const std::string &structPath) {
        return input.resolveMoveHelperForStruct(structPath);
      },
      [&](const Expr &callExpr, const Definition &callee, const LocalMap &callLocals, bool requireValue) {
        return input.emitInlineDefinitionCall(callExpr, callee, callLocals, requireValue);
      },
      [&](const Expr &candidate) {
        if (isCallsStepVectorMutatorMethod(candidate)) {
          const CallsStepVectorHelperReceiverFact receiverFact =
              classifyCallsStepVectorHelperReceiverFromSemanticFacts(
                  candidate.args.front(),
                  input.semanticProgram,
                  input.semanticIndex);
          if (receiverFact == CallsStepVectorHelperReceiverFact::Vector) {
            return false;
          }
          if (receiverFact == CallsStepVectorHelperReceiverFact::NonVector) {
            return input.resolveMethodCallDefinition(candidate, localsIn) != nullptr;
          }

          auto localIt = localsIn.find(candidate.args.front().name);
          if (localIt != localsIn.end() && !localIt->second.isSoaVector &&
              (localIt->second.kind == LocalInfo::Kind::Vector ||
               localIt->second.structTypeName == "/std/collections/experimental_vector/Vector" ||
               localIt->second.structTypeName.rfind(
                   "/std/collections/experimental_vector/Vector__", 0) == 0)) {
            return false;
          }
        }
        if (isCallsStepBuiltinVectorMutatorCall(candidate)) {
          return false;
        }
        if (candidate.isMethodCall && !input.isArrayCountCall(candidate, localsIn) &&
            !input.isStringCountCall(candidate, localsIn) &&
            !input.isVectorCapacityCall(candidate, localsIn)) {
          return input.resolveMethodCallDefinition(candidate, localsIn) != nullptr;
        }
        return input.resolveDefinitionCall(candidate) != nullptr;
      },
      [&]() { input.emitVectorCapacityExceeded(); },
      [&]() { input.emitVectorPopOnEmpty(); },
      [&]() { input.emitVectorIndexOutOfBounds(); },
      [&]() { input.emitArrayIndexOutOfBounds(); },
      [&]() { input.emitVectorReserveNegative(); },
      [&]() { input.emitVectorReserveExceeded(); },
      errorOut);
  if (vectorHelperResult == VectorStatementHelperEmitResult::Error) {
    return false;
  }
  if (vectorHelperResult == VectorStatementHelperEmitResult::Emitted) {
    return true;
  }

  const auto directCallResult = tryEmitDirectCallStatement(
      stmt,
      localsIn,
      [&](const Expr &callExpr, const LocalMap &callLocals) { return input.isArrayCountCall(callExpr, callLocals); },
      [&](const Expr &callExpr, const LocalMap &callLocals) { return input.isStringCountCall(callExpr, callLocals); },
      [&](const Expr &callExpr, const LocalMap &callLocals) { return input.isVectorCapacityCall(callExpr, callLocals); },
      [&](const Expr &callExpr, const LocalMap &callLocals) { return input.emitExpr(callExpr, callLocals); },
      [&](const Expr &callExpr, const LocalMap &callLocals) {
        return input.resolveMethodCallDefinition(callExpr, callLocals);
      },
      [&](const Expr &callExpr) { return input.resolveDefinitionCall(callExpr); },
      [&](const std::string &definitionPath, ReturnInfo &returnInfo) { return input.getReturnInfo(definitionPath, returnInfo); },
      [&](const Expr &callExpr, const Definition &callee, const LocalMap &callLocals, bool expectValue) {
        return input.emitInlineDefinitionCall(callExpr, callee, callLocals, expectValue);
      },
      [&]() { input.emitArrayIndexOutOfBounds(); },
      instructions,
      errorOut,
      input.semanticProgram,
      input.semanticIndex);
  if (directCallResult == DirectCallStatementEmitResult::Error) {
    return false;
  }
  if (directCallResult == DirectCallStatementEmitResult::Emitted) {
    return true;
  }

  const auto assignOrExprResult = emitAssignOrExprStatementWithPop(
      stmt,
      localsIn,
      [&](const Expr &valueExpr, const LocalMap &valueLocals) { return input.emitExpr(valueExpr, valueLocals); },
      instructions);
  if (assignOrExprResult == AssignOrExprStatementEmitResult::Error) {
    return false;
  }
  return true;
}

} // namespace primec::ir_lowerer
