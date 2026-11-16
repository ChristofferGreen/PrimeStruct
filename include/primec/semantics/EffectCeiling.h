#pragma once

#include "primec/ast/Ast.h"

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
// for the entry definition, `defaultEffects` otherwise). Type definitions
// (structs, sums, enums) are not callable and are skipped when they only
// receive defaults. Violations are returned in program order.
std::vector<EffectCeilingViolation>
findEffectCeilingViolations(const Program &program,
                            const std::string &entryPath,
                            const std::vector<std::string> &allowedEffects,
                            const std::vector<std::string> &defaultEffects,
                            const std::vector<std::string> &entryDefaultEffects);

} // namespace primec
