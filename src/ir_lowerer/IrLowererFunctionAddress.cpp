#include "IrLowererFunctionAddress.h"


namespace primec::ir_lowerer {

bool lowerFunctionAddressCall(const Definition &getter,
                              LowerSetupStageState &setupStage,
                              std::vector<IrInstruction> &instructions,
                              bool requireValue,
                              std::string &error) {
  const std::string targetPath = functionAddressTarget(getter);
  const auto targetIt = setupStage.defMap.find(targetPath);
  if (targetIt == setupStage.defMap.end() || targetIt->second == nullptr) {
    error = "function_address of " + getter.fullPath + " names no definition: " + targetPath;
    return false;
  }
  const Definition &target = *targetIt->second;
  auto reservation = setupStage.realCallReservationIndex.find(target.fullPath);
  if (reservation == setupStage.realCallReservationIndex.end()) {
    // The address of a function needs the function, so the definition must be one the target lowers as a real call.
    if (&target == setupStage.entryDef) {
      error = "function_address of " + getter.fullPath + " names the entry definition " + target.fullPath +
              ", which is not a function a program can call through an address";
      return false;
    }
    if (!isRealCallShapeEligible(target, callSiteRealCallContext(setupStage, target, false))) {
      error = "function_address of " + getter.fullPath + " names " + target.fullPath +
              ", which cannot be a real call on this target: its parameters or result do not fit the call ABI";
      return false;
    }
    const uint64_t index = static_cast<uint64_t>(setupStage.realCallEligibleOrder.size());
    reservation = setupStage.realCallReservationIndex.emplace(target.fullPath, index).first;
    setupStage.realCallEligibleOrder.push_back(target.fullPath);
  }
  instructions.push_back({IrOpcode::FunctionAddress, (uint64_t(1) << 32) | reservation->second});
  if (!requireValue) {
    instructions.push_back({IrOpcode::Pop, 0});
  }
  return true;
}

RealCallShapeContext callSiteRealCallContext(const LowerSetupStageState &setupStage,
                                             const Definition &callee,
                                             bool lifecycleReentry) {
  RealCallShapeContext context{&setupStage.defMap,
                               &setupStage.structNames,
                               setupStage.realCallAllowStructs,
                               false,
                               setupStage.realCallAllowStrings,
                               false,
                               lifecycleReentry};
  context.allowSlotResults = isIndirectCallDefinition(callee);
  return context;
}

} // namespace primec::ir_lowerer
