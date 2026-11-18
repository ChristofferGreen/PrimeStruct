#include "primec/ir_lowerer/IrLowererDefinitionPathIndex.h"

#include <algorithm>
#include <string_view>

#include "primec/ir_lowerer/IrLowererCallHelpers.h"
#include "primec/ir_lowerer/IrLowererLowerSetupStage.h"

namespace primec::ir_lowerer {

namespace {

bool hasGeneratedLeafMarker(const std::string &candidatePath,
                            const std::string &lookupPath,
                            const char *marker,
                            size_t markerSize) {
  return candidatePath.compare(lookupPath.size(), markerSize, marker) == 0 &&
         candidatePath.find('/', lookupPath.size() + markerSize) == std::string::npos;
}

} // namespace

DefinitionPathIndex::DefinitionPathIndex(const std::unordered_map<std::string, const Definition *> &defMap) {
  entriesByPath_.reserve(defMap.size());
  size_t mapOrder = 0;
  for (const auto &[path, def] : defMap) {
    if (def != nullptr) {
      entriesByPath_.push_back({path, mapOrder, def});
      if (definitionHasTransform(*def, "sum")) {
        sumPaths_.push_back(path);
      }
    }
    ++mapOrder;
  }
  std::sort(entriesByPath_.begin(), entriesByPath_.end(), [](const Entry &left, const Entry &right) {
    return left.path < right.path;
  });
  std::sort(sumPaths_.begin(), sumPaths_.end());
}

const Definition *DefinitionPathIndex::findGeneratedLeafDefinition(const std::string &lookupPath) const {
  const auto first = std::lower_bound(entriesByPath_.begin(),
                                      entriesByPath_.end(),
                                      lookupPath,
                                      [](const Entry &entry, const std::string &path) { return entry.path < path; });
  const Entry *best = nullptr;
  for (auto it = first; it != entriesByPath_.end(); ++it) {
    const std::string &candidatePath = it->path;
    if (candidatePath.compare(0, lookupPath.size(), lookupPath) != 0) {
      break;
    }
    if (best != nullptr && it->mapOrder > best->mapOrder) {
      continue;
    }
    if (hasGeneratedLeafMarker(candidatePath, lookupPath, "__t", 3) ||
        hasGeneratedLeafMarker(candidatePath, lookupPath, "__ov", 4) ||
        hasGeneratedLeafMarker(candidatePath, lookupPath, "<", 1)) {
      best = &*it;
    }
  }
  return best == nullptr ? nullptr : best->def;
}

const DefinitionPathIndex &definitionPathIndexOf(LowerSetupStageState &setupStage) {
  if (setupStage.definitionPathIndex == nullptr) {
    setupStage.definitionPathIndex = std::make_shared<const DefinitionPathIndex>(setupStage.defMap);
  }
  return *setupStage.definitionPathIndex;
}

} // namespace primec::ir_lowerer
