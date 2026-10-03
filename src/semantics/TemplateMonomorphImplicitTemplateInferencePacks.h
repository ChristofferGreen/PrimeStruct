#pragma once

// Free-function forms of lambdas that used to live inside inferImplicitTemplateArgs
#include "TemplateMonomorphImplicitTemplateInference.h"

namespace primec {

bool extractSpecializedSumTemplateArgsFromTypeText(std::string typeText, std::string paramBaseType, const std::vector<std::string> &paramNames, std::vector<std::string> &templateArgsOut, Context &ctx);
bool buildTypePackOrderedArguments(const Definition &def,
                                   const std::vector<semantics::ParameterInfo> &callParams,
                                   const std::vector<Expr> *orderedCallArgs,
                                   const std::vector<std::optional<std::string>> *orderedCallArgNames,
                                   size_t typePackParamIndex,
                                   std::vector<const Expr *> &orderedArgs,
                                   std::vector<const Expr *> &packedArgs,
                                   size_t &packedParamIndex,
                                   std::string &error);

} // namespace primec
