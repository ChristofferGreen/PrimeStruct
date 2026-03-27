#pragma once

#include "primec/Semantics.h"

#include <string>
#include <unordered_map>
#include <unordered_set>

namespace primec::semantics {

struct ExperimentalGfxRewriteContext {
  std::unordered_set<std::string> structNames;
  std::unordered_map<std::string, std::string> importAliases;

  [[nodiscard]] std::string resolveImportedStructPath(const std::string &name,
                                                      const std::string &namespacePrefix) const;
};

ExperimentalGfxRewriteContext buildExperimentalGfxRewriteContext(const Program &program);

bool hasExperimentalGfxImportedDefinitionPath(const Program &program, const std::string &path);

} // namespace primec::semantics
