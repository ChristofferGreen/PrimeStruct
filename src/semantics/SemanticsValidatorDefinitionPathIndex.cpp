#include "SemanticsValidator.h"

#include <algorithm>

namespace primec::semantics {

namespace {

// Appends the program indices of every entry whose key starts with `prefix`.
void collectPrefixMatches(const std::vector<std::pair<std::string, size_t>> &sorted,
                          const std::string &prefix,
                          std::vector<size_t> &indices) {
  auto it = std::lower_bound(sorted.begin(),
                             sorted.end(),
                             prefix,
                             [](const std::pair<std::string, size_t> &entry, const std::string &value) {
                               return entry.first < value;
                             });
  for (; it != sorted.end() && it->first.compare(0, prefix.size(), prefix) == 0; ++it) {
    indices.push_back(it->second);
  }
}

} // namespace

std::vector<const Definition *> SemanticsValidator::definitionsMatchingPathOrSpecialization(
    const std::string &path) const {
  if (definitionPathIndexSize_ != program_.definitions.size()) {
    definitionPathIndex_.clear();
    definitionPathIndex_.reserve(program_.definitions.size());
    for (size_t i = 0; i < program_.definitions.size(); ++i) {
      definitionPathIndex_.emplace_back(program_.definitions[i].fullPath, i);
    }
    std::sort(definitionPathIndex_.begin(), definitionPathIndex_.end());
    definitionPathIndexSize_ = program_.definitions.size();
  }
  std::vector<size_t> exactAndSpecializations;
  auto exact = std::lower_bound(definitionPathIndex_.begin(),
                                definitionPathIndex_.end(),
                                path,
                                [](const std::pair<std::string, size_t> &entry, const std::string &value) {
                                  return entry.first < value;
                                });
  for (; exact != definitionPathIndex_.end() && exact->first == path; ++exact) {
    exactAndSpecializations.push_back(exact->second);
  }
  collectPrefixMatches(definitionPathIndex_, path + "__", exactAndSpecializations);
  collectPrefixMatches(definitionPathIndex_, path + "<", exactAndSpecializations);
  std::sort(exactAndSpecializations.begin(), exactAndSpecializations.end());
  std::vector<const Definition *> result;
  result.reserve(exactAndSpecializations.size());
  for (const size_t index : exactAndSpecializations) {
    result.push_back(&program_.definitions[index]);
  }
  return result;
}

} // namespace primec::semantics
