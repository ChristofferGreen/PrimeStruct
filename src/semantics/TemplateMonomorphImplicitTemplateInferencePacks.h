#pragma once

// Free-function forms of lambdas that used to live inside inferImplicitTemplateArgs
#include "TemplateMonomorphImplicitTemplateInference.h"

namespace primec {

bool extractSpecializedSumTemplateArgsFromTypeText(std::string typeText, std::string paramBaseType, const std::vector<std::string> &paramNames, std::vector<std::string> &templateArgsOut, Context &ctx);
// Whether `typeText` is the path of a specialization of the generic struct that `paramBaseType` names (`/Holder__t1a2b`
// for `Holder`; the structs may be written with or without their namespace), and when it is, the template arguments of
// that specialization when there are `paramCount` of them.
bool isSpecializedStructOfParamType(std::string typeText, std::string paramBaseType, const Context &ctx);
bool extractSpecializedStructTemplateArgsFromTypeText(std::string typeText, std::string paramBaseType, size_t paramCount, std::vector<std::string> &templateArgsOut, const Context &ctx);
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
