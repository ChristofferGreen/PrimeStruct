#include "primec/SemanticValidationPlan.h"

#include <string_view>
#include <unordered_set>

namespace primec::semantics {

namespace {

std::vector<std::string> copyImportPaths(const std::vector<std::string> &paths) {
  return paths;
}

} // namespace

SemanticValidationPlan buildSemanticValidationPlan(const Program &program,
                                                   const std::string &entryPath) {
  SemanticValidationPlan plan;
  plan.definitionPrepass = buildDefinitionPrepassSnapshot(program);

  plan.entry.fullPath = entryPath;
  const auto entryIt =
      plan.definitionPrepass.firstDeclarationIndexByPath.find(std::string_view(entryPath));
  if (entryIt != plan.definitionPrepass.firstDeclarationIndexByPath.end()) {
    plan.entry.declared = true;
    plan.entry.stableOrderOffset = entryIt->second;
    plan.entry.stableIndex =
        plan.definitionPrepass.declarationsInStableOrder[entryIt->second].stableIndex;
  }

  plan.imports.hasSourceImports = !program.sourceImports.empty();
  plan.imports.sourceImportPaths = copyImportPaths(program.sourceImports);
  plan.imports.programImportPaths = copyImportPaths(program.imports);
  plan.imports.directImportPaths = plan.imports.hasSourceImports
                                       ? plan.imports.sourceImportPaths
                                       : plan.imports.programImportPaths;
  if (plan.imports.hasSourceImports) {
    std::unordered_set<std::string> directImportSet(
        plan.imports.directImportPaths.begin(),
        plan.imports.directImportPaths.end());
    for (const auto &importPath : plan.imports.programImportPaths) {
      if (directImportSet.count(importPath) == 0) {
        plan.imports.transitiveImportPaths.push_back(importPath);
      }
    }
  }

  plan.graphLocalAutoInputs.entryPath = entryPath;
  plan.graphLocalAutoInputs.definitionCount = program.definitions.size();
  plan.graphLocalAutoInputs.executionCount = program.executions.size();

  plan.executionSlice.executionsInStableOrder.reserve(program.executions.size());
  for (std::size_t stableIndex = 0; stableIndex < program.executions.size();
       ++stableIndex) {
    plan.executionSlice.executionsInStableOrder.push_back(
        SemanticValidationExecutionDeclaration{
            program.executions[stableIndex].fullPath,
            stableIndex,
        });
  }

  return plan;
}

} // namespace primec::semantics
