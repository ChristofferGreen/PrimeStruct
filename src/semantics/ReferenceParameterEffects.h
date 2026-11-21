#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "SemanticsHelpers.h"
#include "primec/ast/Ast.h"

namespace primec::semantics {

// Answers "does the body of this definition change the container behind this parameter structurally?" for a
// parameter that a caller passes by `Reference` without `mut`. A structural change is a call of `push`, `pop`,
// `clear`, `reserve`, `remove_at`, `remove_swap` or `insert` on a place rooted at the parameter, or passing the
// parameter on to a callee that changes it (a `mut` parameter, or a plain `Reference` parameter the callee
// itself changes). The analysis reads definition bodies only and never reports a read-only parameter.
//
// Answers are cached per (definition, parameter). A cycle assumes "no change" for the call that closes it, so a
// recursive definition is flagged only when some other path in the cycle changes the parameter; an answer that
// relied on such an assumption is not cached.
class ReferenceParameterEffects {
public:
  using DefinitionMap = std::unordered_map<std::string, const Definition *>;
  using ParameterMap = std::unordered_map<std::string, std::vector<ParameterInfo>>;

  ReferenceParameterEffects(const DefinitionMap &definitions, const ParameterMap &parameters)
      : definitions_(definitions), parameters_(parameters) {}

  bool changesStructurally(const std::string &definitionPath, size_t parameterIndex);

private:
  bool analyze(const std::string &definitionPath, size_t parameterIndex);
  bool exprChanges(const Expr &expr, std::unordered_set<std::string> &tracked);
  bool callChanges(const Expr &call, const std::unordered_set<std::string> &tracked);

  const DefinitionMap &definitions_;
  const ParameterMap &parameters_;
  std::unordered_map<std::string, bool> cache_;
  std::unordered_set<std::string> active_;
  bool assumedCycle_ = false;
};

} // namespace primec::semantics
