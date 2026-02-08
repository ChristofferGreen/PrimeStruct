#include "SemanticsValidator.h"

namespace primec::semantics {

SemanticsValidator::SemanticsValidator(const Program &program,
                                       const std::string &entryPath,
                                       std::string &error,
                                       const std::vector<std::string> &defaultEffects)
    : program_(program), entryPath_(entryPath), error_(error), defaultEffects_(defaultEffects) {}

bool SemanticsValidator::run() {
  if (!buildDefinitionMaps()) {
    return false;
  }
  if (!inferUnknownReturnKinds()) {
    return false;
  }
  if (!validateDefinitions()) {
    return false;
  }
  if (!validateExecutions()) {
    return false;
  }
  return validateEntry();
}

} // namespace primec::semantics
