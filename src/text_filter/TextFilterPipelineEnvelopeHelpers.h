#pragma once

#include <string>
#include <vector>

namespace primec::envelope_internal {

struct NamespaceBlock {
  std::string name;
  size_t end = std::string::npos;
};

struct DefinitionBlock {
  std::string name;
  size_t end = std::string::npos;
};

bool parseDefinitionBlock(const std::string &input, size_t start, size_t &afterPos, DefinitionBlock &block);

void advanceNamespaceScan(const std::string &input,
                          size_t &scanPos,
                          size_t targetPos,
                          std::vector<NamespaceBlock> &stack);

void advanceDefinitionScan(const std::string &input,
                           size_t &scanPos,
                           size_t targetPos,
                           std::vector<DefinitionBlock> &stack,
                           int targetBodyDepth);

size_t findNextEnvelopeStart(const std::string &input, size_t start, int targetBodyDepth);

std::string buildNamespacePrefix(const std::string &basePrefix, const std::vector<NamespaceBlock> &stack);
std::string buildDefinitionPrefix(const std::string &basePrefix, const std::vector<DefinitionBlock> &stack);
std::string makeFullPath(const std::string &name, const std::string &prefix);
bool containsFilterName(const std::vector<std::string> &filters, const std::string &name);
bool filtersEqual(const std::vector<std::string> &left, const std::vector<std::string> &right);
bool scanLeadingTransformList(const std::string &input, std::vector<std::string> &out);

} // namespace primec::envelope_internal
