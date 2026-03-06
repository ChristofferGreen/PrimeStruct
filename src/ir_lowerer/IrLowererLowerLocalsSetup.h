#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "primec/Ast.h"
#include "primec/Ir.h"

#include "IrLowererSetupLocalsHelpers.h"
#include "IrLowererStructFieldBindingHelpers.h"

namespace primec::ir_lowerer {

bool runLowerLocalsSetup(
    std::vector<std::string> &stringTable,
    IrFunction &function,
    const Program &program,
    const Definition &entryDef,
    const std::string &entryPath,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const std::unordered_map<std::string, std::string> &importAliases,
    const std::unordered_set<std::string> &structNames,
    const std::unordered_map<std::string, std::vector<LayoutFieldBinding>> &structFieldInfoByName,
    SetupLocalsOrchestration &setupLocalsOrchestrationOut,
    std::string &errorOut);

} // namespace primec::ir_lowerer
