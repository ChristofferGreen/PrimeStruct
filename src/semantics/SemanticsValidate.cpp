#include "primec/Semantics.h"

#include "SemanticsValidator.h"

namespace primec {

bool Semantics::validate(const Program &program,
                         const std::string &entryPath,
                         std::string &error,
                         const std::vector<std::string> &defaultEffects) const {
  semantics::SemanticsValidator validator(program, entryPath, error, defaultEffects);
  return validator.run();
}

} // namespace primec
