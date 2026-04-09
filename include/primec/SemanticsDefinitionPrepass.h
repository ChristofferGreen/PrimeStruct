#pragma once

#include "primec/Ast.h"
#include "primec/SymbolInterner.h"

#include <cstddef>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace primec::semantics {

// Read-only declaration record. fullPath points into Program-owned storage.
// The Program passed to buildDefinitionPrepassSnapshot must outlive this view.
struct DefinitionPrepassDeclaration {
  std::string_view fullPath;
  SymbolId pathSymbolId = InvalidSymbolId;
  std::size_t stableIndex = 0;
};

// Read-only prepass snapshot for deterministic definition indexing.
// firstDeclarationIndexByPath resolves declarations by full path.
struct DefinitionPrepassSnapshot {
  SymbolInterner pathSymbols;
  std::vector<DefinitionPrepassDeclaration> declarationsInStableOrder;
  std::unordered_map<std::string_view, std::size_t> firstDeclarationIndexByPath;
  std::vector<std::string_view> duplicateDeclarationPaths;
};

DefinitionPrepassSnapshot buildDefinitionPrepassSnapshot(const Program &program);

} // namespace primec::semantics
