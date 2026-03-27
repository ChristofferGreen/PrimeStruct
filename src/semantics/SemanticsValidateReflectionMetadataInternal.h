#pragma once

#include "primec/Ast.h"

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace primec::semantics {

struct ReflectionFieldMetadata {
  std::string name;
  std::string typeName;
  std::string visibility;
};

struct ReflectionMetadataRewriteContext {
  std::unordered_set<std::string> structNames;
  std::unordered_set<std::string> reflectedStructNames;
  std::unordered_map<std::string, std::vector<ReflectionFieldMetadata>> structFieldMetadata;
  std::unordered_map<std::string, const Definition *> definitionByPath;
  std::unordered_map<std::string, std::string> importAliases;

  bool canonicalizeTypeName(const std::string &typeName,
                            const std::string &namespacePrefix,
                            std::string &canonicalOut,
                            std::string &structPathOut,
                            std::string &error) const;
};

ReflectionMetadataRewriteContext buildReflectionMetadataRewriteContext(const Program &program);

} // namespace primec::semantics
