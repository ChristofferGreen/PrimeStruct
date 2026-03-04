#pragma once

#include <string>
#include <unordered_map>

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
std::string formatLayoutFieldEnvelope(const LayoutFieldBinding &binding);

} // namespace primec::ir_lowerer
