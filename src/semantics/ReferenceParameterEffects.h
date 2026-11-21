#pragma once

#include <functional>
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
// itself changes). Assigning a field of the parameter (`bag.items = ...`) is one too when the field's type owns
// a container, because the assignment frees the storage a view of the old field reads; assigning a scalar field
// is not. The analysis reads definition bodies only and never reports a read-only parameter.
//
// Answers are cached per (definition, parameter). A cycle assumes "no change" for the call that closes it, so a
// recursive definition is flagged only when some other path in the cycle changes the parameter; an answer that
// relied on such an assumption is not cached.
class ReferenceParameterEffects {
public:
  using DefinitionMap = std::unordered_map<std::string, const Definition *>;
  using ParameterMap = std::unordered_map<std::string, std::vector<ParameterInfo>>;

  // Whether the field reached from a struct of type `structTypeText` (declared in `namespacePrefix`) through
  // the `fields` chain has a type that owns a container.
  using FieldOwnsContainer = std::function<bool(const std::string &structTypeText,
                                                const std::string &namespacePrefix,
                                                const std::vector<std::string> &fields)>;

  ReferenceParameterEffects(const DefinitionMap &definitions,
                            const ParameterMap &parameters,
                            FieldOwnsContainer fieldOwnsContainer)
      : definitions_(definitions), parameters_(parameters),
        fieldOwnsContainer_(std::move(fieldOwnsContainer)) {
  }

  bool changesStructurally(const std::string &definitionPath, size_t parameterIndex);

private:
  bool analyze(const std::string &definitionPath, size_t parameterIndex);
  // The parameter under analysis: the names that stand for its place (the parameter and its aliases), and the
  // type its fields are looked up in.
  struct Subject {
    std::unordered_set<std::string> tracked;
    std::string parameterName;
    std::string structTypeText;
    std::string namespacePrefix;
  };
  bool exprChanges(const Expr &expr, Subject &subject);
  bool callChanges(const Expr &call, const Subject &subject);
  bool assignReplacesContainerField(const Expr &call, const Subject &subject) const;

  const DefinitionMap &definitions_;
  const ParameterMap &parameters_;
  FieldOwnsContainer fieldOwnsContainer_;
  std::unordered_map<std::string, bool> cache_;
  std::unordered_set<std::string> active_;
  bool assumedCycle_ = false;
};

} // namespace primec::semantics
