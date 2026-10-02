#pragma once

// Shared callback sets for the ir_lowerer helper validation tests (TODO-5388).
// Each factory returns the "do nothing" configuration that many tests repeated
// inline; a test that needs a different callback overrides just that field.
#include "test_ir_pipeline_validation_helpers.h"

namespace primec::validation_test_support {

// Call-base inference setup input whose struct-path, field-slot and
// uninitialized-storage callbacks all report "nothing found" (the storage
// callback succeeds with resolved == false).
inline primec::ir_lowerer::LowerInferenceExprKindCallBaseSetupInput defaultCallBaseSetupInput() {
  return {
      .inferStructExprPath = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string(); },
      .resolveStructFieldSlot =
          [](const std::string &, const std::string &, primec::ir_lowerer::StructSlotFieldInfo &) { return false; },
      .resolveUninitializedStorage =
          [](const primec::Expr &,
             const primec::ir_lowerer::LocalMap &,
             primec::ir_lowerer::UninitializedStorageAccessInfo &,
             bool &resolved) {
            resolved = false;
            return true;
          },
  };
}

// Call-return setup input over `defMap`: resolves paths to the expression name
// and reports no array or string count calls.
inline primec::ir_lowerer::LowerInferenceExprKindCallReturnSetupInput defaultCallReturnSetupInput(
    const std::unordered_map<std::string, const primec::Definition *> &defMap) {
  return {
      .defMap = &defMap,
      .resolveExprPath = [](const primec::Expr &expr) { return expr.name; },
      .isArrayCountCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      .isStringCountCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
  };
}

// Return-info setup input with inert callbacks; `bindingValueKind` is the value
// kind every binding reports.
inline primec::ir_lowerer::LowerInferenceReturnInfoSetupInput defaultReturnInfoSetupInput(
    primec::ir_lowerer::LocalInfo::ValueKind bindingValueKind) {
  return {
      .resolveStructTypeName = [](const std::string &, const std::string &, std::string &) { return false; },
      .resolveStructArrayInfoFromPath =
          [](const std::string &, primec::ir_lowerer::StructArrayTypeInfo &) { return false; },
      .isBindingMutable = [](const primec::Expr &) { return false; },
      .bindingKind = [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::Kind::Value; },
      .hasExplicitBindingTypeTransform = [](const primec::Expr &) { return true; },
      .bindingValueKind =
          [bindingValueKind](const primec::Expr &, primec::ir_lowerer::LocalInfo::Kind) { return bindingValueKind; },
      .inferExprKind =
          [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
            return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
          },
      .isFileErrorBinding = [](const primec::Expr &) { return false; },
      .setReferenceArrayInfo = [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      .applyStructArrayInfo = [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      .applyStructValueInfo = [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      .inferStructExprPath = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string{}; },
      .isStringBinding = [](const primec::Expr &) { return false; },
      .inferArrayElementKind =
          [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
            return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
          },
      .lowerMatchToIf = [](const primec::Expr &, primec::Expr &, std::string &) { return true; },
  };
}

// Dispatch setup input that resolves expression paths to "/<name>" and reports
// inference errors through `error`.
inline primec::ir_lowerer::LowerInferenceExprKindDispatchSetupInput defaultDispatchSetupInput(
    const std::unordered_map<std::string, const primec::Definition *> &defMap, std::string &error) {
  return {
      .defMap = &defMap,
      .resolveExprPath = [](const primec::Expr &expr) { return "/" + expr.name; },
      .error = &error,
  };
}

// Appends a plain void callable summary for `path` (no effects, no Result
// type). A non-empty `handlerPath` turns on the on_error fields with the given
// error type and bound argument count.
inline void addVoidCallableSummary(primec::SemanticProgram &semanticProgram,
                                   const std::string &path,
                                   uint64_t semanticNodeId,
                                   const std::string &handlerPath = "",
                                   const std::string &onErrorType = "",
                                   size_t onErrorBoundArgCount = 0) {
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
      .isExecution = false,
      .returnKind = "void",
      .isCompute = false,
      .isUnsafe = false,
      .activeEffects = {},
      .activeCapabilities = {},
      .hasResultType = false,
      .resultTypeHasValue = false,
      .resultValueType = "",
      .resultErrorType = "",
      .hasOnError = !handlerPath.empty(),
      .onErrorHandlerPath = handlerPath,
      .onErrorErrorType = onErrorType,
      .onErrorBoundArgCount = onErrorBoundArgCount,
      .semanticNodeId = semanticNodeId,
      .provenanceHandle = 0,
      .fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, path),
  });
}

// emitConversionsAndCallsOperatorExpr with the inert tail callbacks most cases
// share: compare-to-zero succeeds, temp locals come from `nextLocal`, the
// out-of-bounds/non-finite emitters do nothing, and every lookup (string table,
// type name, math constant, struct path/type/slot/field/copy) finds nothing.
inline bool emitConversionsOperatorExprWithInertTail(
    const primec::Expr &expr,
    const primec::ir_lowerer::LocalMap &locals,
    int32_t &nextLocal,
    const primec::ir_lowerer::EmitConversionsAndCallsExprWithLocalsFn &emitExpr,
    const primec::ir_lowerer::InferConversionsAndCallsExprKindWithLocalsFn &inferExprKind,
    std::vector<primec::IrInstruction> &instructions,
    bool &handled,
    std::string &error) {
  return primec::ir_lowerer::emitConversionsAndCallsOperatorExpr(
      expr,
      locals,
      nextLocal,
      emitExpr,
      inferExprKind,
      [](primec::ir_lowerer::LocalInfo::ValueKind, bool) { return true; },
      [&]() { return nextLocal++; },
      []() {},
      []() {},
      []() {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return false; },
      [](const std::string &) { return primec::ir_lowerer::LocalInfo::ValueKind::Unknown; },
      [](const std::string &, std::string &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string(); },
      [](const std::string &, const std::string &, std::string &) { return false; },
      [](const std::string &, int32_t &) { return false; },
      [](const std::string &, const std::string &, int32_t &, int32_t &, std::string &) { return false; },
      [](int32_t, int32_t, int32_t) { return false; },
      instructions,
      handled,
      error);
}

// inferCallParameterLocalInfo over an empty local map with the usual inert
// callbacks (bindings are immutable value kinds from their transforms, nothing is
// a struct or string); only `applyStructValueInfo` varies per case.
inline bool inferCallParameterLocalInfoWithStructHook(
    const primec::Expr &param,
    const std::function<void(const primec::Expr &, primec::ir_lowerer::LocalInfo &)> &applyStructValueInfo,
    primec::ir_lowerer::LocalInfo &info,
    std::string &error) {
  return primec::ir_lowerer::inferCallParameterLocalInfo(
      param,
      {},
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &) { return true; },
      [](const primec::Expr &expr) { return primec::ir_lowerer::bindingKindFromTransforms(expr); },
      [](const primec::Expr &expr, primec::ir_lowerer::LocalInfo::Kind kind) {
        return primec::ir_lowerer::bindingValueKindFromTransforms(expr, kind);
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      applyStructValueInfo,
      [](const primec::Expr &) { return false; },
      info,
      error);
}

} // namespace primec::validation_test_support
