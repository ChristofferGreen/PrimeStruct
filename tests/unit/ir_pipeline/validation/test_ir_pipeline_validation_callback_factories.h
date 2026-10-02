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

} // namespace primec::validation_test_support
