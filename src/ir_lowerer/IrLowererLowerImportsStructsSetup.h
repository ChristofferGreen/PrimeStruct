#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "primec/Ast.h"
#include "primec/Ir.h"
#include "primec/SemanticProduct.h"

#include "IrLowererStructFieldBindingHelpers.h"

namespace primec::ir_lowerer {

bool runLowerImportsStructsSetup(
    const Program &program,
    const SemanticProgram *semanticProgram,
    IrModule &outModule,
    std::unordered_map<std::string, const Definition *> &defMapOut,
    std::unordered_set<std::string> &structNamesOut,
    std::unordered_map<std::string, std::string> &importAliasesOut,
    std::unordered_map<std::string, std::vector<LayoutFieldBinding>> &structFieldInfoByNameOut,
    std::string &errorOut);

} // namespace primec::ir_lowerer
