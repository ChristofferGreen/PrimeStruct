#pragma once

#include "primec/ast/Ast.h"

#include <functional>
#include <string>
#include <vector>

namespace primec {

// One definition or execution of user code whose effects exceed the ceiling
// that `--allow-effects` sets.
struct EffectCeilingViolation {
  std::string message;
  int line = 0;
  int column = 0;
};

// Checks every definition and execution of user code (paths outside /std/)
// against `allowedEffects`. A declaration is checked by its `[effects(...)]`
// list; one without it by the default set it receives (`entryDefaultEffects`
// for the entry definition, `defaultEffects` otherwise). A declaration for which `isTrustedLocation(line, column)`
// is true (when given) is skipped like a /std path: the stdlib defines types outside /std, such as /File, and
// those are as trusted as /std itself. Type definitions
// (structs, sums, enums) are not callable and are skipped when they only
// receive defaults. Violations are returned in program order.
std::vector<EffectCeilingViolation>
findEffectCeilingViolations(const Program &program,
                            const std::string &entryPath,
                            const std::vector<std::string> &allowedEffects,
                            const std::vector<std::string> &defaultEffects,
                            const std::vector<std::string> &entryDefaultEffects,
                            const std::function<bool(int, int)> &isTrustedLocation = nullptr);

} // namespace primec
