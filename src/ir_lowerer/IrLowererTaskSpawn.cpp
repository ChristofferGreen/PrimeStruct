#include "IrLowererTaskSpawn.h"

#include "IrLowererRecursionAnalysis.h"
#include "primec/ir_lowerer/IrLowererCallHelpers.h"

namespace primec::ir_lowerer {

bool taskSpawnRunsOnScheduler(const Definition &callee,
                              const LowerSetupStageState &setupStage,
                              std::string &reasonOut) {
  reasonOut.clear();
  if (&callee == setupStage.entryDef) {
    reasonOut = "it is the entry definition";
    return false;
  }
  if (definitionHasTransform(callee, "host")) {
    reasonOut = "it is a host function";
    return false;
  }
  std::string parentStructPath;
  if (isStructHelperDefinition(callee, setupStage.structNames, parentStructPath) &&
      !definitionHasTransform(callee, "static")) {
    reasonOut = "it is a method";
    return false;
  }
  // The scalar shape: every parameter a plain scalar (no `mut`), a scalar or void result. On the address ABI targets
  // the body is lowered with that ABI's rules too, so it must take that shape as well.
  if (!isRealCallShapeEligible(
          callee, RealCallShapeContext{&setupStage.defMap, &setupStage.structNames, false}) ||
      (setupStage.realCallAllowStructs &&
       !isRealCallShapeEligible(
           callee, RealCallShapeContext{&setupStage.defMap, &setupStage.structNames, true}))) {
    reasonOut = "a parameter or the result is not a scalar a real call takes";
    return false;
  }
  return true;
}

uint64_t reserveTaskSpawnTarget(LowerSetupStageState &setupStage, const Definition &callee) {
  const auto it = setupStage.realCallReservationIndex.find(callee.fullPath);
  if (it != setupStage.realCallReservationIndex.end()) {
    return it->second;
  }
  const uint64_t index = static_cast<uint64_t>(setupStage.realCallEligibleOrder.size());
  setupStage.realCallReservationIndex.emplace(callee.fullPath, index);
  setupStage.realCallEligibleOrder.push_back(callee.fullPath);
  return index;
}

} // namespace primec::ir_lowerer
