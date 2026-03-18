#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "primec/Semantics.h"

namespace primec::semantics {

struct ReflectionGeneratedHelperContext {
  const Definition &def;
  const std::vector<std::string> &fieldNames;
  const std::unordered_map<std::string, std::string> &fieldTypeNames;
  std::unordered_set<std::string> &definitionPaths;
  std::vector<Definition> &rewrittenDefinitions;
  std::string &error;
};

bool emitReflectionSerializeHelper(ReflectionGeneratedHelperContext &context);
bool emitReflectionDeserializeHelper(ReflectionGeneratedHelperContext &context);

} // namespace primec::semantics
