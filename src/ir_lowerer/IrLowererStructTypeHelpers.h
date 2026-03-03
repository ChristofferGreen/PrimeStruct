#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace primec::ir_lowerer {

std::string joinTemplateArgsText(const std::vector<std::string> &args);

bool resolveStructTypePathFromScope(
    const std::string &typeName,
    const std::string &namespacePrefix,
    const std::unordered_set<std::string> &structNames,
    const std::unordered_map<std::string, std::string> &importAliases,
    std::string &resolvedOut);

} // namespace primec::ir_lowerer
