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
bool inferPrimitiveFieldBinding(const Expr &initializer,
                                const std::unordered_map<std::string, LayoutFieldBinding> &knownFields,
                                LayoutFieldBinding &bindingOut);

} // namespace primec::ir_lowerer
