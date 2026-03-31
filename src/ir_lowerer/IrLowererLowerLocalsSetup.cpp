#include "IrLowererLowerLocalsSetup.h"

#include <memory>

#include "IrLowererUninitializedTypeHelpers.h"

namespace primec::ir_lowerer {

bool runLowerLocalsSetup(
    std::vector<std::string> &stringTable,
    IrFunction &function,
    const Program &program,
    const Definition &entryDef,
    const std::string &entryPath,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const std::unordered_map<std::string, std::string> &importAliases,
    const std::unordered_set<std::string> &structNames,
    const std::unordered_map<std::string, std::vector<LayoutFieldBinding>> &structFieldInfoByName,
    SetupLocalsOrchestration &setupLocalsOrchestrationOut,
    std::string &errorOut) {
  std::destroy_at(&setupLocalsOrchestrationOut);
  std::construct_at(&setupLocalsOrchestrationOut);

  EntryReturnRuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup
      entryReturnRuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup{};
  if (!buildProgramEntryReturnRuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup(
          stringTable,
          function,
          program,
          entryDef,
          entryPath,
          defMap,
          importAliases,
          structNames,
          structFieldInfoByName.size(),
          [&](const AppendStructLayoutFieldFn &appendStructLayoutField) {
            appendStructLayoutFieldsFromFieldBindings(
                structFieldInfoByName, defMap, appendStructLayoutField);
          },
          entryReturnRuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup,
          errorOut)) {
    return false;
  }

  setupLocalsOrchestrationOut = unpackSetupLocalsOrchestration(
      entryReturnRuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup);
  return true;
}

} // namespace primec::ir_lowerer
