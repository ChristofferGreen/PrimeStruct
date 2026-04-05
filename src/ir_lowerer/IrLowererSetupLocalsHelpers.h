#pragma once

#include <memory>

#include "IrLowererUninitializedTypeHelpers.h"

namespace primec::ir_lowerer {

struct SetupLocalsOrchestration {
  std::shared_ptr<EntryReturnRuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup> ownedSetup{};
  EntryReturnConfig entryReturnConfig{};
  RuntimeErrorAndStringLiteralSetup runtimeErrorAndStringLiteralSetup{};
  EntryCountAccessSetup entryCountAccessSetup{};
  EntryCallOnErrorSetup entryCallOnErrorSetup{};
  SetupMathResolvers setupMathResolvers{};
  BindingTypeAdapters bindingTypeAdapters{};
  SetupTypeAndStructTypeAdapters setupTypeAndStructTypeAdapters{};
  StructArrayInfoAdapters structArrayInfoAdapters{};
  StructSlotResolutionAdapters structSlotResolutionAdapters{};
  UninitializedResolutionAdapters uninitializedResolutionAdapters{};
  ApplyStructValueInfoFn applyStructValueInfo{};
};

void populateSetupLocalsOrchestration(
    const EntryReturnRuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup &setup,
    SetupLocalsOrchestration &out);

} // namespace primec::ir_lowerer
