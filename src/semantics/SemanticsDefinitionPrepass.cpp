#include "primec/SemanticsDefinitionPrepass.h"

#include <unordered_set>

namespace primec::semantics {

DefinitionPrepassSnapshot buildDefinitionPrepassSnapshot(const Program &program) {
  DefinitionPrepassSnapshot snapshot;
  snapshot.declarationsInStableOrder.reserve(program.definitions.size());
  snapshot.firstDeclarationIndexByPath.reserve(program.definitions.size());

  std::unordered_set<std::string_view> duplicatePathSet;
  duplicatePathSet.reserve(program.definitions.size());

  for (std::size_t index = 0; index < program.definitions.size(); ++index) {
    const Definition &definition = program.definitions[index];
    const SymbolId pathSymbolId = snapshot.pathSymbols.intern(definition.fullPath);

    snapshot.declarationsInStableOrder.push_back(DefinitionPrepassDeclaration{
        .fullPath = definition.fullPath,
        .pathSymbolId = pathSymbolId,
        .stableIndex = index,
    });

    const auto [it, inserted] =
        snapshot.firstDeclarationIndexByPath.emplace(definition.fullPath, index);
    if (!inserted) {
      (void)it;
      if (duplicatePathSet.insert(definition.fullPath).second) {
        snapshot.duplicateDeclarationPaths.push_back(definition.fullPath);
      }
    }
  }

  return snapshot;
}

} // namespace primec::semantics
