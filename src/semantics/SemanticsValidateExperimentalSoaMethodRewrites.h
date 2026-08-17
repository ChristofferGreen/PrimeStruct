#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "SemanticsHelpers.h"
#include "primec/semantics/Semantics.h"

namespace primec {

bool rewriteExperimentalSoaSamePathHelperMethods(Program &program, std::string &error);
bool rewriteExperimentalSoaToAosMethods(Program &program, std::string &error);
bool rewriteExperimentalSoaInlineBorrowMethods(Program &program, std::string &error);
std::optional<Expr> normalizeExperimentalSoaBorrowedHelperReceiver(
    const Expr &receiver,
    const std::unordered_map<std::string, semantics::BindingInfo> &bindings,
    const std::unordered_map<std::string, semantics::BindingInfo> &soaCollectionReturnDefinitions,
    const std::string &definitionNamespace,
    const std::unordered_set<std::string> &structPaths);

} // namespace primec
