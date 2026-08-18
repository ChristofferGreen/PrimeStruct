#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "RequirementPredicateFacts.h"
#include "TemplateMonomorphContext.h"
#include "primec/ast/Ast.h"

namespace primec {

bool isNonTypeTransformName(const std::string &name);

bool isBuiltinTemplateContainer(const std::string &name);

std::string templateMonomorphSoaReceiverTypeName();

bool isTemplateMonomorphSoaReceiverType(std::string_view receiverTypeName);

std::string templateMonomorphSamePathSoaHelperPrefix(bool leadingSlash = true);

std::string templateMonomorphCompatibilitySoaHelperPrefix(bool leadingSlash = true);

std::string templateMonomorphPublicSoaHelperPrefix(bool leadingSlash = true);

std::string templateMonomorphExperimentalSoaHelperPrefix();

std::string templateMonomorphSoaToAosHelperName(bool borrowed = false);

std::string templateMonomorphSoaToSoaHelperName();

bool stripTemplateMonomorphSoaHelperPrefix(std::string_view path,
                                           std::string &helperNameOut,
                                           bool leadingSlash = true);

bool isTemplateMonomorphMapImportAlias(std::string_view name);

bool isTemplateMonomorphMapConstructorCallPath(std::string_view path);

bool isTemplateMonomorphMapEntryConstructorPath(std::string path);

std::string normalizeBuiltinCollectionTemplateBase(const std::string &name);

bool isBuiltinCollectionTemplateBase(const std::string &name, size_t argumentCount);

bool importPathCoversTarget(const std::string &importPath, const std::string &targetPath);

std::string helperOverloadInternalPath(const std::string &publicPath, size_t parameterCount);

std::string helperOverloadDefinitionKey(const Definition &def);

bool definitionHasRequireTransform(const Definition &def);

std::string genericTypeOverloadInternalPath(const std::string &publicPath,
                                            size_t templateParameterCount);

std::string helperOverloadDisplayPath(const std::string &path, const Context &ctx);

bool selectGenericTypeOverloadPath(const std::string &resolvedPath,
                                   size_t templateArgumentCount,
                                   const Context &ctx,
                                   std::string &pathOut,
                                   std::string &error);

std::string bindingTypeTextForRequirementOverloadSelection(const semantics::BindingInfo &binding);

std::optional<std::string>
requirementOverloadArgumentTypeText(const Expr &arg,
                                    const LocalTypeMap *locals,
                                    const std::vector<semantics::ParameterInfo> *params);

std::string requirementOverloadSourceLocation(int line, int column);

std::string requirementOverloadExprDisplayText(const Expr &expr);

std::string formatRequirementOverloadArgumentFacts(
    const Expr &expr,
    const LocalTypeMap *locals,
    const std::vector<semantics::ParameterInfo> *params);

std::string formatRequirementOverloadCallSite(const Expr &expr,
                                              const std::string &resolvedPath);

std::string formatRequirementOverloadPredicateSummary(
    const semantics::RequirementPredicateFactDraft &fact,
    std::string_view originalArgument,
    int sourceLine,
    int sourceColumn);

struct RequirementOverloadViability {
  bool viable = true;
  bool decisive = false;
  std::vector<std::string> diagnostics;
  std::vector<std::string> satisfiedDiagnostics;
};

RequirementOverloadViability evaluateRequirementOverloadViability(
    const Definition &def,
    const Expr &expr,
    const Context &ctx,
    const LocalTypeMap *locals,
    const std::vector<semantics::ParameterInfo> *params,
    bool exactConcreteTypeMatch = false);

std::string formatRequirementOverloadCandidateSummary(
    const HelperOverloadEntry &entry,
    const Definition *def,
    const Context &ctx,
    const RequirementOverloadViability *viability);

std::string selectRequirementAwareHelperOverloadPath(
    const Expr &expr,
    const std::string &resolvedPath,
    const Context &ctx,
    const std::vector<const HelperOverloadEntry *> &candidates,
    const LocalTypeMap *locals,
    const std::vector<semantics::ParameterInfo> *params);

std::string selectHelperOverloadPath(const Expr &expr,
                                     const std::string &resolvedPath,
                                     const Context &ctx,
                                     const LocalTypeMap *locals = nullptr,
                                     const std::vector<semantics::ParameterInfo> *params = nullptr);

bool resolveHelperOverloadDefinitionIdentity(const Definition &def,
                                             const Context &ctx,
                                             std::string &internalPathOut,
                                             std::string &nameOut);

bool resolveGenericTypeOverloadDefinitionIdentity(const Definition &def,
                                                  const Context &ctx,
                                                  std::string &internalPathOut,
                                                  std::string &nameOut);

std::string trimWhitespace(const std::string &text);

std::string stripWhitespace(const std::string &text);

} // namespace primec
