#include "SemanticsValidator.h"

#include "SemanticsValidatorStatementBindingsHelpers.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace primec::semantics {

std::string
SemanticsValidator::activeBorrowSinkOf(const std::vector<ParameterInfo> &params,
                                       const std::unordered_map<std::string, BindingInfo> &locals,
                                       const std::string &name,
                                       const std::string &ignoreBorrowName,
                                       bool includePointers) {
  if (currentValidationState_.context.definitionIsUnsafe) {
    return {};
  }
  std::string sink;
  auto considerBorrow = [&](const std::string &borrowName, const BindingInfo &binding) {
    if (borrowName == name || (!ignoreBorrowName.empty() && borrowName == ignoreBorrowName) ||
        currentValidationState_.endedReferenceBorrows.count(borrowName) > 0) {
      return;
    }
    std::string root = statementBindingsHelpers::referenceRootForBorrowBinding(borrowName, binding);
    if (root.empty() && includePointers && binding.typeName == "Pointer") {
      root = binding.referenceRoot;
    }
    // A view of `dereference(r)` is rooted at the `Reference` local `r`, which aliases `name` when it was made from it
    // (`[Reference<vector<i32>>] r{location(v)}`): the view reads `name`'s storage even after `r`'s last use.
    if (binding.isContainerView && !root.empty() && root != name) {
      if (const auto aliasIt = locals.find(root);
          aliasIt != locals.end() && !aliasIt->second.isContainerView &&
          aliasIt->second.referenceRoot == name &&
          normalizeBindingTypeName(aliasIt->second.typeName) == "Reference") {
        root = name;
      }
    }
    // Several borrows can hold the root: report the smallest name so the diagnostic is stable.
    if (root == name && (sink.empty() || borrowName < sink)) {
      sink = borrowName;
    }
  };
  for (const auto &param : params) {
    considerBorrow(param.name, param.binding);
  }
  for (const auto &entry : locals) {
    considerBorrow(entry.first, entry.second);
  }
  return sink;
}

} // namespace primec::semantics
