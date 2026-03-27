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

bool applyFilterToChunk(const std::string &input,
                        std::string &output,
                        std::string &error,
                        const std::string *filter) {
  if (filter == nullptr || filter->empty()) {
    output = input;
    return true;
  }
  TextFilterOptions passOptions;
  passOptions.enabledFilters = {*filter};
  return applyPass(input, output, error, passOptions);
}

bool applyPerEnvelopeFilterPass(const std::string &input,
                                std::string &output,
                                std::string &error,
                                const std::string &filter,
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
  const bool filterActive = envelope_internal::containsFilterName(activeFilters, filter);
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
    if (!applyFilterToChunk(chunk, filteredChunk, error, filterActive ? &filter : nullptr)) {
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
                                      filter,
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
  if (!applyFilterToChunk(tail, filteredTail, error, filterActive ? &filter : nullptr)) {
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
                                      std::string(),
                                      activeFilters,
                                      rules,
                                      suppressLeadingOverride,
                                      baseNamespacePrefix,
                                      baseDefinitionPrefix,
                                      true,
                                      baseFilters);
  }
  std::unordered_set<std::string> applied;
  std::string current = input;
  bool allowExplicitRecursion = true;
  for (size_t i = 0; i < activeFilters.size(); ++i) {
    const std::string &filter = activeFilters[i];
    if (!applied.insert(filter).second) {
      continue;
    }
    std::string next;
    if (!applyPerEnvelopeFilterPass(current,
                                    next,
                                    error,
                                    filter,
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
