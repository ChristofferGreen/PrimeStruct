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

} // namespace primec::validation_test_support
