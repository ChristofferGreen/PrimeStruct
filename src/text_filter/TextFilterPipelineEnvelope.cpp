#include "TextFilterPipelineInternal.h"
#include "TextFilterPipelineEnvelopeHelpers.h"

#include "TextFilterHelpers.h"
#include "primec/TextFilterPipeline.h"
#include "primec/TransformRules.h"

#include <unordered_set>

namespace primec {
using namespace text_filter;

bool applyPerEnvelope(const std::string &input,
                      std::string &output,
                      std::string &error,
                      const std::vector<std::string> &filters,
                      const std::vector<TextTransformRule> &rules,
                      bool suppressLeadingOverride,
                      const std::string &baseNamespacePrefix,
                      const std::string &baseDefinitionPrefix,
                      const std::vector<std::string> &baseFilters);

namespace {

// Applies each filter in `filterSequence` to `input` in order, threading each
// filter's output into the next filter's input - i.e. exactly the same
// per-filter sequential composition the caller used to get by driving N
// separate top-to-bottom envelope-structure walks (one per filter). Doing
// that composition here, chunk-locally, instead of one envelope walk per
// filter is safe because a leaf chunk's filtered text never depends on any
// other chunk's content - only the order filters are applied *to this one
// chunk* matters, and that order (filterSequence's order) is unchanged.
bool applyFiltersToChunk(const std::string &input,
                         std::string &output,
                         std::string &error,
                         const std::vector<std::string> &filterSequence) {
  if (filterSequence.empty()) {
    output = input;
    return true;
  }
  std::string current = input;
  for (const auto &filter : filterSequence) {
    if (filter.empty()) {
      continue;
    }
    TextFilterOptions passOptions;
    passOptions.enabledFilters = {filter};
    std::string next;
    if (!applyPass(current, next, error, passOptions)) {
      return false;
    }
    current.swap(next);
  }
  output = std::move(current);
  return true;
}

// Walks the envelope/namespace/definition structure of `input` exactly once,
// applying every filter in `filterSequence` (in order) to each leaf chunk of
// plain code found along the way. `activeFilters` is the filter set that
// governs envelope-inheritance decisions (which envelope bodies keep
// `filterSequence` verbatim, i.e. recursion happens via a further
// same-shaped combined pass, vs which envelope bodies declare an
// independent filter set - "explicit"/rule-selected - vs `activeFilters`
// and so get diverted through the full `applyPerEnvelope` driver instead).
// This function used to be driven once *per individual filter*, from
// `applyPerEnvelope`'s loop - each such call independently re-ran the full
// `findNextEnvelopeStart`/`parseNamespaceBlock`/`parseDefinitionBlock`
// structural scan of the same text, even though none of that structural
// scan logic ever looks at which filter is active. Folding the filter loop
// down into the (cheap, per-chunk) `applyFiltersToChunk` call below,
// instead of looping the whole structural walk, keeps the exact same
// per-chunk filter application order while performing the walk itself only
// once per distinct filter *set* instead of once per filter.
bool applyPerEnvelopeFilterPass(const std::string &input,
                                std::string &output,
                                std::string &error,
                                const std::vector<std::string> &filterSequence,
                                const std::vector<std::string> &activeFilters,
                                const std::vector<TextTransformRule> &rules,
                                bool suppressLeadingOverride,
                                const std::string &baseNamespacePrefix,
                                const std::string &baseDefinitionPrefix,
                                bool allowExplicitRecursion,
                                const std::vector<std::string> &baseFilters) {
  output.clear();
  size_t pos = 0;
  size_t scanPos = 0;
  bool suppress = suppressLeadingOverride;
  const int targetBodyDepth = suppressLeadingOverride ? 1 : 0;
  std::vector<envelope_internal::NamespaceBlock> namespaceStack;
  std::vector<envelope_internal::DefinitionBlock> definitionStack;
  while (scanPos < input.size()) {
    size_t listStart = findNextTransformListStart(input, scanPos);
    size_t envelopeStart = envelope_internal::findNextEnvelopeStart(input, scanPos, targetBodyDepth);
    size_t nextStart = std::string::npos;
    bool isTransformList = false;
    if (listStart != std::string::npos &&
        (envelopeStart == std::string::npos || listStart < envelopeStart)) {
      nextStart = listStart;
      isTransformList = true;
    } else if (envelopeStart != std::string::npos) {
      nextStart = envelopeStart;
      isTransformList = false;
    }
    if (nextStart == std::string::npos) {
      break;
    }
    size_t contextPos = scanPos;
    envelope_internal::advanceNamespaceScan(input, contextPos, nextStart, namespaceStack);
    contextPos = scanPos;
    envelope_internal::advanceDefinitionScan(input, contextPos, nextStart, definitionStack, targetBodyDepth);
    scanPos = nextStart;
    std::string namespacePrefix = envelope_internal::buildNamespacePrefix(baseNamespacePrefix, namespaceStack);
    std::string definitionBase = baseDefinitionPrefix.empty() ? namespacePrefix : baseDefinitionPrefix;
    std::string definitionPrefix = envelope_internal::buildDefinitionPrefix(definitionBase, definitionStack);
    size_t envelopeStartPos = nextStart;
    size_t envelopeEnd = std::string::npos;
  std::string envelopeName;
  std::vector<std::string> explicitTransforms;
  bool envelopeIsDefinition = false;
  bool envelopeIsLambda = false;
  std::string definitionFullPath;
  if (isTransformList) {
    TransformListScan listScan;
    if (!scanTransformList(input, listStart, listScan)) {
        break;
      }
      if (suppress && listStart == 0) {
        scanPos = listScan.end + 1;
        suppress = false;
        continue;
      }
      if (!scanEnvelopeAfterList(input, listScan.end + 1, envelopeEnd, envelopeName)) {
        scanPos = listScan.end + 1;
        suppress = false;
        continue;
      }
      envelope_internal::DefinitionBlock definitionBlock;
      size_t afterPos = listScan.end + 1;
      if (envelope_internal::parseDefinitionBlock(input, listScan.end + 1, afterPos, definitionBlock)) {
        envelopeIsDefinition = true;
        definitionFullPath = envelope_internal::makeFullPath(definitionBlock.name, definitionPrefix);
      }
      explicitTransforms = std::move(listScan.textTransforms);
    } else {
      if (suppress && envelopeStart == 0) {
        scanPos = envelopeStart + 1;
        suppress = false;
        continue;
      }
      if (input[envelopeStart] == '[') {
        size_t listEnd = std::string::npos;
        if (!scanLambdaEnvelope(input, envelopeStart, listEnd, envelopeEnd)) {
          scanPos = envelopeStart + 1;
          suppress = false;
          continue;
        }
        envelopeIsLambda = true;
        envelopeName.clear();
      } else {
        size_t probe = envelopeStart;
        if (!scanEnvelopeAfterList(input, probe, envelopeEnd, envelopeName)) {
          scanPos = envelopeStart + 1;
          suppress = false;
          continue;
        }
        envelope_internal::DefinitionBlock definitionBlock;
        size_t afterPos = envelopeStart;
        if (envelope_internal::parseDefinitionBlock(input, envelopeStart, afterPos, definitionBlock)) {
          envelopeIsDefinition = true;
          definitionFullPath = envelope_internal::makeFullPath(definitionBlock.name, definitionPrefix);
        }
      }
    }
    std::string chunk = input.substr(pos, envelopeStartPos - pos);
    std::string filteredChunk;
    if (!applyFiltersToChunk(chunk, filteredChunk, error, filterSequence)) {
      return false;
    }
    output.append(filteredChunk);
    std::string envelope = input.substr(envelopeStartPos, envelopeEnd - envelopeStartPos + 1);
    const std::vector<std::string> *autoFilters = &activeFilters;
    if (envelopeIsDefinition || envelopeIsLambda) {
      autoFilters = &baseFilters;
    }
    if (!rules.empty() && !envelopeIsLambda) {
      std::string fullPath =
          envelope_internal::makeFullPath(envelopeName, envelopeIsDefinition ? definitionPrefix : namespacePrefix);
      if (const auto *ruleFilters = selectRuleTransforms(rules, fullPath)) {
        autoFilters = ruleFilters;
      }
    }
    const std::vector<std::string> &envelopeFilters =
        explicitTransforms.empty() ? *autoFilters : explicitTransforms;
    const bool inheritsFilters = envelope_internal::filtersEqual(envelopeFilters, activeFilters);
    std::string filteredEnvelope;
    const std::string &nextDefinitionPrefix = envelopeIsDefinition ? definitionFullPath : baseDefinitionPrefix;
    if (inheritsFilters) {
      if (!applyPerEnvelopeFilterPass(envelope,
                                      filteredEnvelope,
                                      error,
                                      filterSequence,
                                      envelopeFilters,
                                      rules,
                                      true,
                                      namespacePrefix,
                                      nextDefinitionPrefix,
                                      allowExplicitRecursion,
                                      baseFilters)) {
        return false;
      }
      output.append(filteredEnvelope);
    } else if (allowExplicitRecursion) {
      if (!applyPerEnvelope(envelope,
                            filteredEnvelope,
                            error,
                            envelopeFilters,
                            rules,
                            true,
                            namespacePrefix,
                            nextDefinitionPrefix,
                            baseFilters)) {
        return false;
      }
      output.append(filteredEnvelope);
    } else {
      output.append(envelope);
    }
    pos = envelopeEnd + 1;
    scanPos = pos;
    suppress = false;
  }
  std::string tail = input.substr(pos);
  std::string filteredTail;
  if (!applyFiltersToChunk(tail, filteredTail, error, filterSequence)) {
    return false;
  }
  output.append(filteredTail);
  return true;
}

} // namespace

bool applyPerEnvelope(const std::string &input,
                      std::string &output,
                      std::string &error,
                      const std::vector<std::string> &filters,
                      const std::vector<TextTransformRule> &rules,
                      bool suppressLeadingOverride,
                      const std::string &baseNamespacePrefix,
                      const std::string &baseDefinitionPrefix,
                      const std::vector<std::string> &baseFilters) {
  std::vector<std::string> activeFilters = filters;
  if (activeFilters.empty()) {
    return applyPerEnvelopeFilterPass(input,
                                      output,
                                      error,
                                      activeFilters,
                                      activeFilters,
                                      rules,
                                      suppressLeadingOverride,
                                      baseNamespacePrefix,
                                      baseDefinitionPrefix,
                                      true,
                                      baseFilters);
  }
  // Apply the filter list in rounds instead of one filter at a time. Every
  // filter already known about when a round starts is applied to every leaf
  // chunk, in order, inside a *single* envelope-structure walk (see
  // `applyPerEnvelopeFilterPass`'s own comment) - this preserves the exact
  // same per-chunk filter ordering as the old one-filter-per-walk loop
  // (each chunk still sees filter[0], then filter[1], then filter[2], ...,
  // in the same order, and chunks don't interact), it just stops re-running
  // the (filter-independent) structural scan once per filter. A later round
  // only ever runs if a leading `[filterName]<...>` transform-list
  // annotation at the very start of the (already-filtered) text reveals a
  // filter not yet applied - the same rare, dynamic-discovery case the
  // original loop supported - and even then, only the newly discovered
  // filters run in that next round; already-applied filters are never
  // reapplied.
  std::unordered_set<std::string> applied;
  std::string current = input;
  bool allowExplicitRecursion = true;
  size_t consideredCount = 0;
  while (consideredCount < activeFilters.size()) {
    std::vector<std::string> roundFilters;
    for (size_t i = consideredCount; i < activeFilters.size(); ++i) {
      if (applied.insert(activeFilters[i]).second) {
        roundFilters.push_back(activeFilters[i]);
      }
    }
    consideredCount = activeFilters.size();
    if (roundFilters.empty()) {
      continue;
    }
    std::string next;
    if (!applyPerEnvelopeFilterPass(current,
                                    next,
                                    error,
                                    roundFilters,
                                    activeFilters,
                                    rules,
                                    suppressLeadingOverride,
                                    baseNamespacePrefix,
                                    baseDefinitionPrefix,
                                    allowExplicitRecursion,
                                    baseFilters)) {
      return false;
    }
    current.swap(next);
    allowExplicitRecursion = false;
    std::vector<std::string> listTransforms;
    if (envelope_internal::scanLeadingTransformList(current, listTransforms)) {
      for (const auto &name : listTransforms) {
        if (applied.find(name) != applied.end()) {
          continue;
        }
        if (!envelope_internal::containsFilterName(activeFilters, name)) {
          activeFilters.push_back(name);
        }
      }
    }
  }
  output = current;
  return true;
}

} // namespace primec
