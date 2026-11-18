#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "primec/ast/Ast.h"

namespace primec::ir_lowerer {

struct LowerSetupStageState;

// Lookups over the lowerer's definition map that would otherwise scan every definition on a miss. Built once from the
// map after setup (the map does not change while lowering), so a lookup costs a binary search instead of a pass over
// the whole program.
class DefinitionPathIndex {
public:
  explicit DefinitionPathIndex(const std::unordered_map<std::string, const Definition *> &defMap);

  // The definition whose path is `lookupPath` followed by a generated-leaf marker (`__t...`, `__ov...` or `<...`) and
  // no further `/`: a monomorphized or overloaded form of the helper at `lookupPath`. When several match, the one the
  // definition map iterates first, which is what a scan of the map returns. Null when none matches.
  const Definition *findGeneratedLeafDefinition(const std::string &lookupPath) const;

  // The paths of definitions with the `sum` transform, sorted.
  const std::vector<std::string> &sumDefinitionPaths() const { return sumPaths_; }

private:
  struct Entry {
    std::string path;
    size_t mapOrder = 0;
    const Definition *def = nullptr;
  };
  std::vector<Entry> entriesByPath_;
  std::vector<std::string> sumPaths_;
};

// The index of the definition map of `setupStage`, built on first use.
const DefinitionPathIndex &definitionPathIndexOf(LowerSetupStageState &setupStage);

} // namespace primec::ir_lowerer
