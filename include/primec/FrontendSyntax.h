#pragma once

#include "primec/Ast.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace primec {

bool isSyntaxWildcardImportPath(const std::string &path, std::string &prefixOut);

std::string normalizeSyntaxImportAliasTargetPath(const std::string &path);

std::unordered_map<std::string, std::string> buildSyntaxImportAliases(
    const std::vector<std::string> &imports,
    const std::vector<Definition> &definitions,
    const std::unordered_map<std::string, const Definition *> &definitionByPath);

} // namespace primec
