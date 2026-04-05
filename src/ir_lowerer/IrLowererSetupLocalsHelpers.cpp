#include "IrLowererSetupLocalsHelpers.h"

namespace primec::ir_lowerer {

void populateSetupLocalsOrchestration(
    const EntryReturnRuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup &setup,
    SetupLocalsOrchestration &out) {
  out = {};
  out.entryReturnConfig = setup.entryReturnConfig;

  const auto &runtimeEntrySetup =
      setup.runtimeEntrySetupMathTypeStructAndUninitializedResolutionSetup;
  out.runtimeErrorAndStringLiteralSetup = runtimeEntrySetup.runtimeErrorAndStringLiteralSetup;

  const auto &entrySetup =
      runtimeEntrySetup.entrySetupMathTypeStructAndUninitializedResolutionSetup;
  out.entryCountAccessSetup = entrySetup.entryCountCallOnErrorSetup.countAccessSetup;
  out.entryCallOnErrorSetup = entrySetup.entryCountCallOnErrorSetup.callOnErrorSetup;

  const auto &setupMathTypeStructAndUninitializedResolutionSetup =
      entrySetup.setupMathTypeStructAndUninitializedResolutionSetup;
  out.setupMathResolvers =
      setupMathTypeStructAndUninitializedResolutionSetup.setupMathAndBindingAdapters.setupMathResolvers;
  out.bindingTypeAdapters =
      setupMathTypeStructAndUninitializedResolutionSetup.setupMathAndBindingAdapters.bindingTypeAdapters;
  out.setupTypeAndStructTypeAdapters =
      setupMathTypeStructAndUninitializedResolutionSetup
          .setupTypeStructAndUninitializedResolutionSetup.setupTypeAndStructTypeAdapters;

  const auto &structAndUninitializedResolutionSetup =
      setupMathTypeStructAndUninitializedResolutionSetup
          .setupTypeStructAndUninitializedResolutionSetup.structAndUninitializedResolutionSetup;
  out.structArrayInfoAdapters =
      structAndUninitializedResolutionSetup.structLayoutResolutionAdapters.structArrayInfo;
  out.structSlotResolutionAdapters =
      structAndUninitializedResolutionSetup.structLayoutResolutionAdapters.structSlotResolution;
  out.uninitializedResolutionAdapters =
      structAndUninitializedResolutionSetup.uninitializedResolutionAdapters;
  out.applyStructValueInfo = out.setupTypeAndStructTypeAdapters.structTypeResolutionAdapters.applyStructValueInfo;
}

} // namespace primec::ir_lowerer
