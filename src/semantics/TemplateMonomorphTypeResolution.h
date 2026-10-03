#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "TemplateMonomorphContext.h"
#include "primec/ast/Ast.h"
#include "SemanticsHelpers.h"
#include "primec/support/StdlibSurfaceRegistry.h"
#include "SemanticsValidatorInferCollectionCompatibilityInternal.h"
#include "TemplateMonomorphExperimentalCollectionReturnSetup.h"
#include "StdlibCollectionSurfaceHelpers.h"
#include "TemplateMonomorphUsings.h"

namespace primec {

ResolvedType resolveTypeStringImpl(std::string input,
                                   const SubstMap &mapping,
                                   const std::unordered_set<std::string> &allowedParams,
                                   const std::string &namespacePrefix,
                                   Context &ctx,
                                   std::string &error,
                                   std::unordered_set<std::string> &substitutionStack);

bool splitTypePackExpansionText(const std::string &input, std::string &packNameOut);

bool splitTypePackIndexText(const std::string &input,
                            std::string &packNameOut,
                            std::string &indexTextOut);

bool parseUnsignedTemplateIndex(std::string_view text, uint64_t &valueOut);

bool resolveTemplateIndexText(const std::string &indexText,
                              const SubstMap &mapping,
                              uint64_t &indexOut);

const TemplatePackBinding *findTemplatePackBinding(const Definition &def,
                                                   const std::string &name);

const TemplatePackBinding *findTemplatePackBindingForCurrentDefinition(const Context &ctx,
                                                                       const std::string &name);

bool appendResolvedTemplateArg(const std::string &arg,
                               const SubstMap &mapping,
                               const std::unordered_set<std::string> &allowedParams,
                               const std::string &namespacePrefix,
                               Context &ctx,
                               std::string &error,
                               std::unordered_set<std::string> &substitutionStack,
                               std::vector<std::string> &resolvedArgs,
                               bool &allConcrete);

bool resolveTemplateArgumentList(std::vector<std::string> &args,
                                 const SubstMap &mapping,
                                 const std::unordered_set<std::string> &allowedParams,
                                 const std::string &namespacePrefix,
                                 Context &ctx,
                                 std::string &error,
                                 bool &allConcreteOut);

bool isRequirementSubstitutionIdentChar(char ch);

std::string rewriteRequirementArgumentText(const std::string &text,
                                           const SubstMap &mapping);

ResolvedType resolveTypeStringImpl(std::string input,
                                   const SubstMap &mapping,
                                   const std::unordered_set<std::string> &allowedParams,
                                   const std::string &namespacePrefix,
                                   Context &ctx,
                                   std::string &error,
                                   std::unordered_set<std::string> &substitutionStack);

ResolvedType resolveTypeString(std::string input,
                               const SubstMap &mapping,
                               const std::unordered_set<std::string> &allowedParams,
                               const std::string &namespacePrefix,
                               Context &ctx,
                               std::string &error);

bool rewriteTransforms(std::vector<Transform> &transforms,
                       const SubstMap &mapping,
                       const std::unordered_set<std::string> &allowedParams,
                       const std::string &namespacePrefix,
                       Context &ctx,
                       std::string &error);

std::string resolveCalleePath(const Expr &expr,
                              const std::string &namespacePrefix,
                              const Context &ctx,
                              const LocalTypeMap *locals = nullptr,
                              const std::vector<ParameterInfo> *params = nullptr);

} // namespace primec
