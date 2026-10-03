#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "primec/ast/Ast.h"
#include "primec/ir/Ir.h"
#include "primec/frontend/SemanticProduct.h"
#include "primec/support/CanonicalReceiverType.h"
#include "primec/support/CallbackTypes.h"
#include "primec/ir_lowerer/IrLowererSemanticProductTargetAdapters.h"
#include "primec/ir_lowerer/IrLowererGpuEffects.h"
#include "primec/ir_lowerer/IrLowererNativeEffects.h"
#include "primec/ir_lowerer/IrLowererVmEffects.h"
#include "primec/ir_lowerer/IrLowererLowerEntrySetup.h"
#include "primec/ir_lowerer/IrLowererLowerEffects.h"

#include "primec/ir_lowerer/IrLowererSharedTypes.h"
#include "primec/ir_lowerer/IrLowererSetupTypeHelpers.h"
#include "primec/ir_lowerer/IrLowererSetupTypeCollectionHelpers.h"
#include "primec/ir_lowerer/IrLowererCallHelpers.h"
#include "primec/ir_lowerer/IrLowererBindingTransformHelpers.h"
#include "primec/ir_lowerer/IrLowererBindingTypeHelpers.h"
#include "primec/ir_lowerer/IrLowererFileWriteHelpers.h"
#include "primec/ir_lowerer/IrLowererFlowHelpers.h"
#include "primec/ir_lowerer/IrLowererHelpers.h"
#include "primec/ir_lowerer/IrLowererResultHelpers.h"
#include "primec/ir_lowerer/IrLowererStatementBindingHelpers.h"
#include "primec/ir_lowerer/IrLowererCountAccessHelpers.h"
#include "primec/ir_lowerer/IrLowererIndexKindHelpers.h"
#include "primec/ir_lowerer/IrLowererOnErrorHelpers.h"
#include "primec/ir_lowerer/IrLowererInlineCallContextHelpers.h"
#include "primec/ir_lowerer/IrLowererStructFieldBindingHelpers.h"
#include "primec/ir_lowerer/IrLowererStructTypeHelpers.h"
#include "primec/ir_lowerer/IrLowererInlineParamHelpers.h"
#include "primec/ir_lowerer/IrLowererInlineStructArgHelpers.h"
#include "primec/ir_lowerer/IrLowererStringCallHelpers.h"
#include "primec/ir_lowerer/IrLowererOperatorArithmeticHelpers.h"
#include "primec/ir_lowerer/IrLowererStatementCallHelpers.h"
#include "primec/ir_lowerer/IrLowererLowerInlineCallActiveContextStep.h"
#include "primec/ir_lowerer/IrLowererLowerExprEmitSetup.h"
#include "primec/ir_lowerer/IrLowererReturnInferenceHelpers.h"
#include "primec/ir_lowerer/IrLowererSetupMathHelpers.h"
#include "primec/ir_lowerer/IrLowererSetupInferenceHelpers.h"
#include "primec/ir_lowerer/IrLowererLowerInferenceBaseKindHelpers.h"
#include "primec/ir_lowerer/IrLowererLowerInlineCallCleanupStep.h"
#include "primec/ir_lowerer/IrLowererLowerInlineCallContextSetupStep.h"
#include "primec/ir_lowerer/IrLowererLowerInlineCallGpuLocalsStep.h"
#include "primec/ir_lowerer/IrLowererLowerInlineCallReturnValueStep.h"
#include "primec/ir_lowerer/IrLowererLowerImportsStructsSetup.h"
#include "primec/ir_lowerer/IrLowererLowerInlineCallStatementStep.h"
#include "primec/ir_lowerer/IrLowererStringLiteralHelpers.h"
#include "primec/ir_lowerer/IrLowererRuntimeErrorHelpers.h"
#include "primec/ir_lowerer/IrLowererUninitializedTypeHelpers.h"
#include "primec/ir_lowerer/IrLowererSetupLocalsHelpers.h"
#include "primec/ir_lowerer/IrLowererLowerLocalsSetup.h"
#include "primec/ir_lowerer/IrLowererLowerReturnCallsSetup.h"
#include "primec/ir_lowerer/IrLowererOperatorArcHyperbolicHelpers.h"
#include "primec/ir_lowerer/IrLowererOperatorClampMinMaxTrigHelpers.h"
#include "primec/ir_lowerer/IrLowererOperatorComparisonHelpers.h"
#include "primec/ir_lowerer/IrLowererOperatorConversionsAndCallsHelpers.h"
#include "primec/ir_lowerer/IrLowererOperatorPowAbsSignHelpers.h"
#include "primec/ir_lowerer/IrLowererOperatorSaturateRoundingRootsHelpers.h"
#include "primec/ir_lowerer/IrLowererStructLayoutHelpers.h"
#include "primec/ir_lowerer/IrLowererStructReturnPathHelpers.h"
#include "primec/ir_lowerer/IrLowererTemplateTypeParseHelpers.h"

// Convenience overloads that only the lowerer tests use: they forward to the real entry points
// with a null SemanticProgram (and no optional callbacks). They used to live in the hand-kept
// header mirrors.
namespace primec::ir_lowerer {

inline bool runLowerImportsStructsSetup(
    const Program &program,
    IrModule &outModule,
    std::unordered_map<std::string, const Definition *> &defMapOut,
    std::unordered_set<std::string> &structNamesOut,
    std::unordered_map<std::string, std::string> &importAliasesOut,
    std::unordered_map<std::string, std::vector<LayoutFieldBinding>> &structFieldInfoByNameOut,
    std::string &errorOut) {
  return runLowerImportsStructsSetup(program,
                                     nullptr,
                                     outModule,
                                     defMapOut,
                                     structNamesOut,
                                     importAliasesOut,
                                     structFieldInfoByNameOut,
                                     errorOut);
}

inline UninitializedStorageInitDropEmitResult tryEmitUninitializedStorageInitDropStatement(
    const Expr &stmt,
    LocalMap &localsIn,
    std::vector<IrInstruction> &instructions,
    const ResolveUninitializedStorageForStatementFn &resolveUninitializedStorage,
    const ExprLocalsPredicateFn &emitExpr,
    const ResolveStructSlotLayoutForStatementFn &resolveStructSlotLayout,
    const std::function<int32_t()> &allocTempLocal,
    const EmitStructCopyFromPtrsForStatementFn &emitStructCopyFromPtrs,
    std::string &error,
    const EmitUninitializedStorageDropFromPtrForStatementFn &emitDropFromPtr = {}) {
  return tryEmitUninitializedStorageInitDropStatement(stmt,
                                                     localsIn,
                                                     instructions,
                                                     resolveUninitializedStorage,
                                                     emitExpr,
                                                     resolveStructSlotLayout,
                                                     allocTempLocal,
                                                     emitStructCopyFromPtrs,
                                                     ResolveDefinitionCallForStatementFn{},
                                                     error,
                                                     emitDropFromPtr);
}

inline DirectCallStatementEmitResult tryEmitDirectCallStatement(
    const Expr &stmt,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const LocalMap &)> &isArrayCountCall,
    const std::function<bool(const Expr &, const LocalMap &)> &isStringCountCall,
    const std::function<bool(const Expr &, const LocalMap &)> &isVectorCapacityCall,
    const std::function<const Definition *(const Expr &, const LocalMap &)> &resolveMethodCallDefinition,
    const std::function<const Definition *(const Expr &)> &resolveDefinitionCall,
    const std::function<bool(const std::string &, ReturnInfo &)> &getReturnInfo,
    const std::function<bool(const Expr &, const Definition &, const LocalMap &, bool)> &emitInlineDefinitionCall,
    std::vector<IrInstruction> &instructions,
    std::string &error,
    const SemanticProgram *semanticProgram = nullptr,
    const SemanticProductIndex *semanticIndex = nullptr) {
  return tryEmitDirectCallStatement(
      stmt,
      localsIn,
      isArrayCountCall,
      isStringCountCall,
      isVectorCapacityCall,
      [](const Expr &, const LocalMap &) { return true; },
      resolveMethodCallDefinition,
      resolveDefinitionCall,
      getReturnInfo,
      emitInlineDefinitionCall,
      instructions,
      error,
      semanticProgram,
      semanticIndex);
}

inline bool collectStructLayoutFieldBindings(
    const Program &program,
    const std::unordered_set<std::string> &structNames,
    const std::function<std::string(const std::string &, const std::string &)> &resolveStructTypePath,
    const std::function<std::string(const Expr &)> &resolveStructLayoutExprPath,
    const std::unordered_map<std::string, const Definition *> &defMap,
    std::unordered_map<std::string, std::vector<LayoutFieldBinding>> &fieldsByStructOut,
    std::string &errorOut) {
  return collectStructLayoutFieldBindings(program,
                                          structNames,
                                          resolveStructTypePath,
                                          resolveStructLayoutExprPath,
                                          defMap,
                                          nullptr,
                                          fieldsByStructOut,
                                          errorOut);
}

inline bool collectStructLayoutFieldBindingsFromProgramContext(
    const Program &program,
    const std::unordered_set<std::string> &structNames,
    const std::function<std::string(const std::string &, const std::string &)> &resolveStructTypePath,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const std::unordered_map<std::string, std::string> &importAliases,
    std::unordered_map<std::string, std::vector<LayoutFieldBinding>> &fieldsByStructOut,
    std::string &errorOut) {
  return collectStructLayoutFieldBindingsFromProgramContext(program,
                                                            structNames,
                                                            resolveStructTypePath,
                                                            defMap,
                                                            importAliases,
                                                            nullptr,
                                                            fieldsByStructOut,
                                                            errorOut);
}

inline bool computeStructLayoutUncached(
    const ::primec::Definition &def,
    const std::vector<LayoutFieldBinding> &fieldBindings,
    const std::function<bool(const LayoutFieldBinding &, BindingTypeLayout &, std::string &)> &resolveFieldTypeLayout,
    IrStructLayout &layoutOut,
    std::string &errorOut) {
  return computeStructLayoutUncached(def, fieldBindings, resolveFieldTypeLayout, nullptr, layoutOut, errorOut);
}

inline bool computeStructLayoutFromFieldInfo(
    const ::primec::Definition &def,
    const std::unordered_map<std::string, std::vector<LayoutFieldBinding>> &structFieldInfoByName,
    const std::function<std::string(const std::string &, const std::string &)> &resolveStructTypePath,
    const std::unordered_map<std::string, const ::primec::Definition *> &defMap,
    const std::function<bool(const ::primec::Definition &, IrStructLayout &)> &computeStructLayout,
    IrStructLayout &layoutOut,
    std::string &errorOut) {
  return computeStructLayoutFromFieldInfo(
      def, structFieldInfoByName, resolveStructTypePath, defMap, computeStructLayout, nullptr, layoutOut, errorOut);
}

inline bool appendProgramStructLayouts(
    const ::primec::Program &program,
    const std::unordered_map<std::string, const ::primec::Definition *> &defMap,
    const std::function<bool(const ::primec::Definition &, IrStructLayout &)> &computeStructLayout,
    std::vector<IrStructLayout> &layoutsOut,
    std::string &errorOut) {
  return appendProgramStructLayouts(program, defMap, nullptr, computeStructLayout, layoutsOut, errorOut);
}

inline bool appendProgramStructLayouts(
    const ::primec::Program &program,
    const std::function<bool(const ::primec::Definition &, IrStructLayout &)> &computeStructLayout,
    std::vector<IrStructLayout> &layoutsOut,
    std::string &errorOut) {
  std::unordered_map<std::string, const ::primec::Definition *> defMap;
  defMap.reserve(program.definitions.size());
  for (const auto &def : program.definitions) {
    defMap.emplace(def.fullPath, &def);
  }
  return appendProgramStructLayouts(program, defMap, nullptr, computeStructLayout, layoutsOut, errorOut);
}

} // namespace primec::ir_lowerer
