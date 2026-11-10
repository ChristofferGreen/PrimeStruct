#include "IrLowererTaskSpawn.h"

#include "IrLowererRecursionAnalysis.h"
#include "primec/ir_lowerer/IrLowererCallHelpers.h"

namespace primec::ir_lowerer {

bool taskSpawnRunsOnScheduler(const Definition &callee,
                              const LowerSetupStageState &setupStage,
                              TaskSpawnShape &shapeOut,
                              std::string &reasonOut) {
  shapeOut = {};
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
  if (setupStage.realCallAllowStructs) {
    // The address ABI: scalars by value, structs through a heap block the spawner fills.
    return resolveTaskSpawnShape(
        callee,
        RealCallShapeContext{&setupStage.defMap, &setupStage.structNames, true},
        shapeOut,
        reasonOut);
  }
  // The scalar ABI: every parameter a plain scalar (no `mut`), a scalar or void result.
  if (!isRealCallShapeEligible(
          callee, RealCallShapeContext{&setupStage.defMap, &setupStage.structNames, false})) {
    reasonOut = "a parameter or the result is not a scalar a real call takes";
    return false;
  }
  shapeOut.parameters.resize(callee.parameters.size());
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
