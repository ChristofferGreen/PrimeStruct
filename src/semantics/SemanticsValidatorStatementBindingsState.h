#pragma once

// State shared by the validateBinding* phase functions (split out of validateBindingStatement, TODO-5385).
#include "SemanticsValidator.h"

#include <functional>
#include <optional>
#include <string>

namespace primec::semantics {

struct ValidateBindingState {
  bool result{};
  PhaseStatus done(bool value) {
    result = std::move(value);
    return PhaseStatus::Done;
  }
  std::function<bool(std::string message)> failBindingDiagnostic;
  const std::vector<std::string> * definitionTemplateArgs{};
  std::string bindingLookupNamespace{};
  BindingInfo info{};
  std::optional<std::string> restrictType{};
  bool hasExplicitType{};
  bool explicitAutoType{};
  const Expr * initializer{};
  bool entryArgInit{};
  bool entryArgStringInit{};
  std::optional<SemanticsValidator::EntryArgStringScope> entryArgScope{};
  std::function<bool()> isStandaloneSoaFieldViewInitializer;
  std::function<std::optional<bool>()> validateAndRecordTargetTypedSumInitializer;
  std::function<bool()> isTargetTypedSumInitializerSyntax;
  std::function<const BindingInfo *(const std::string &name)> resolveNamedBinding;
  std::function<bool(const Expr &, std::string &)> resolvePointerRoot{};
  bool allowBindings{};
  bool allowCompileTimeTypeBindings{};
};

} // namespace primec::semantics
