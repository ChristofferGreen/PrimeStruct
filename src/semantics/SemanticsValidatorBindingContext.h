#pragma once

// What the rules that validate a binding statement read and fill in (TODO-5982): the statement and its scope, then
// the facts the rules settle in order (the parsed binding, its initializer, entry-argument flags). Data only; the
// lookups the rules share are members of SemanticsValidator (SemanticsValidatorBindingHelpers.cpp).
#include "SemanticsValidator.h"

#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace primec::semantics {

struct BindingContext {
  const std::vector<ParameterInfo> &params;
  std::unordered_map<std::string, BindingInfo> &locals;
  const Expr &stmt;
  const std::string &namespacePrefix;
  bool &handled;
  bool allowBindings{};
  bool allowCompileTimeTypeBindings{};
  const std::vector<std::string> *definitionTemplateArgs{};
  std::string bindingLookupNamespace{};
  BindingInfo info{};
  std::optional<std::string> restrictType{};
  bool hasExplicitType{};
  bool explicitAutoType{};
  const Expr *initializer{};
  bool entryArgInit{};
  bool entryArgStringInit{};
  std::optional<SemanticsValidator::EntryArgStringScope> entryArgScope{};
  // The outcome of the statement once a rule has returned Done.
  bool result{};
  PhaseStatus done(bool value) {
    result = value;
    return PhaseStatus::Done;
  }
};

} // namespace primec::semantics
