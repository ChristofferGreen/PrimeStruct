#pragma once

// Free-function forms of lambdas that used to live inside inferImplicitTemplateArgs
// (TODO-5385).
#include "TemplateMonomorphImplicitTemplateInference.h"

namespace primec {

bool extractBuiltinSoaElementTypeText(std::string typeText, std::string &elemTypeOut);
bool extractBuiltinVectorElementTypeText(std::string typeText, std::string &elemTypeOut);
bool assignBindingFromTypeText(const std::string &typeText, BindingInfo &bindingOut);
std::string unsupportedBuiltinSoaPendingDiagnostic(const Expr &candidate, const LocalTypeMap &locals, const std::vector<ParameterInfo> &params, const std::string &namespacePrefix, Context &ctx, bool allowMathBare);

} // namespace primec
