#include "IrLowererLowerImportsStructsSetup.h"

#include <functional>

#include "IrLowererStructLayoutHelpers.h"
#include "IrLowererStructTypeHelpers.h"
#include "primec/FrontendSyntax.h"

namespace primec::ir_lowerer {

bool runLowerImportsStructsSetup(
    const Program &program,
    const SemanticProgram *semanticProgram,
    IrModule &outModule,
    std::unordered_map<std::string, const Definition *> &defMapOut,
    std::unordered_set<std::string> &structNamesOut,
    std::unordered_map<std::string, std::string> &importAliasesOut,
    std::unordered_map<std::string, std::vector<LayoutFieldBinding>> &structFieldInfoByNameOut,
    std::string &errorOut) {
  defMapOut.clear();
  structNamesOut.clear();
  importAliasesOut.clear();
  structFieldInfoByNameOut.clear();

  buildDefinitionMapAndStructNames(program.definitions, defMapOut, structNamesOut, semanticProgram);
  importAliasesOut = primec::buildSyntaxImportAliases(program.imports, program.definitions, defMapOut);

  auto resolveStructTypePath = [&](const std::string &typeName, const std::string &namespacePrefix) -> std::string {
    return resolveStructTypePathCandidateFromScope(typeName, namespacePrefix, structNamesOut, importAliasesOut);
  };

  if (!collectStructLayoutFieldBindingsFromProgramContext(
          program,
          structNamesOut,
          resolveStructTypePath,
          defMapOut,
          importAliasesOut,
          semanticProgram,
          structFieldInfoByNameOut,
          errorOut)) {
    return false;
  }

  std::unordered_map<std::string, IrStructLayout> layoutCache;
  std::unordered_set<std::string> layoutStack;
  std::function<bool(const Definition &, IrStructLayout &)> computeStructLayout;
  computeStructLayout = [&](const Definition &def, IrStructLayout &layoutOut) -> bool {
    auto computeUncachedLayout = [&](IrStructLayout &layout, std::string &layoutError) -> bool {
      return computeStructLayoutFromFieldInfo(
          def,
          structFieldInfoByNameOut,
          resolveStructTypePath,
          defMapOut,
          computeStructLayout,
          semanticProgram,
          layout,
          layoutError);
    };
    return computeStructLayoutWithCache(
        def.fullPath, layoutCache, layoutStack, computeUncachedLayout, layoutOut, errorOut);
  };

  if (!appendProgramStructLayouts(
          program, defMapOut, semanticProgram, computeStructLayout, outModule.structLayouts, errorOut)) {
    return false;
  }

  return true;
}

} // namespace primec::ir_lowerer
