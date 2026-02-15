#include "SemanticsValidator.h"

namespace primec::semantics {

SemanticsValidator::SemanticsValidator(const Program &program,
                                       const std::string &entryPath,
                                       std::string &error,
                                       const std::vector<std::string> &defaultEffects,
                                       const std::vector<std::string> &entryDefaultEffects)
    : program_(program),
      entryPath_(entryPath),
      error_(error),
      defaultEffects_(defaultEffects),
      entryDefaultEffects_(entryDefaultEffects) {
  for (const auto &importPath : program_.imports) {
    if (importPath == "/math/*") {
      mathImportAll_ = true;
      continue;
    }
    if (importPath.rfind("/math/", 0) == 0 && importPath.size() > 6) {
      std::string name = importPath.substr(6);
      if (name.find('/') == std::string::npos && name != "*") {
        mathImports_.insert(std::move(name));
      }
    }
  }
}

bool SemanticsValidator::run() {
  if (!buildDefinitionMaps()) {
    return false;
  }
  if (!inferUnknownReturnKinds()) {
    return false;
  }
  if (!validateStructLayouts()) {
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

bool SemanticsValidator::allowMathBareName(const std::string &name) const {
  if (name.empty() || name.find('/') != std::string::npos) {
    return false;
  }
  return mathImportAll_ || mathImports_.count(name) > 0;
}

bool SemanticsValidator::hasAnyMathImport() const {
  return mathImportAll_ || !mathImports_.empty();
}

} // namespace primec::semantics
