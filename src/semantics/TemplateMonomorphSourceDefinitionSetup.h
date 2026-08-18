#pragma once

#include <string>
#include <unordered_set>

#include "primec/ast/Ast.h"

namespace primec {

struct Context;

bool validateTemplateParameterMetadataForTemplateSetup(const Definition &def,
                                                       std::string &error);

bool initializeTemplateMonomorphSourceDefinitions(Context &ctx,
                                                  const std::string &entryPath,
                                                  std::unordered_set<std::string> &templateRoots,
                                                  std::string &error);

}  // namespace primec
