#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "primec/Ast.h"

namespace primec::ir_lowerer {

struct LayoutFieldBinding {
  std::string typeName;
  std::string typeTemplateArg;
};

const Expr *getEnvelopeValueExpr(const Expr &candidate, bool allowAnyName);
bool extractExplicitLayoutFieldBinding(const Expr &expr, LayoutFieldBinding &bindingOut);
bool inferPrimitiveFieldBinding(const Expr &initializer,
                                const std::unordered_map<std::string, LayoutFieldBinding> &knownFields,
                                LayoutFieldBinding &bindingOut);
bool resolveLayoutFieldBinding(
    const Definition &def,
    const Expr &expr,
    const std::unordered_map<std::string, LayoutFieldBinding> &knownFields,
    const std::unordered_set<std::string> &structNames,
    const std::function<std::string(const std::string &, const std::string &)> &resolveStructTypePath,
    const std::function<std::string(const Expr &)> &resolveStructLayoutExprPath,
    const std::unordered_map<std::string, const Definition *> &defMap,
    LayoutFieldBinding &bindingOut,
    std::string &errorOut);
std::string formatLayoutFieldEnvelope(const LayoutFieldBinding &binding);

} // namespace primec::ir_lowerer
