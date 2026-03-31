#pragma once

#include "IrLowererUninitializedTypeHelpers.h"

namespace primec::ir_lowerer {

struct SetupLocalsOrchestration {
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

SetupLocalsOrchestration unpackSetupLocalsOrchestration(
    const EntryReturnRuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup &setup);

} // namespace primec::ir_lowerer
