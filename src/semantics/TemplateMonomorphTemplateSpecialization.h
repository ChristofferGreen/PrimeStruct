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

namespace primec {

std::vector<Definition> collectTemplateSpecializationFamily(const std::string &basePath, const Context &ctx);

bool canReplaceGeneratedTemplateShell(const Definition &existingDef, const Definition &clone);

std::string parentPathForDefinition(const std::string &path);

std::vector<TemplateRootInfo> collectNestedTemplateRoots(const std::string &basePath,
                                                         const std::vector<Definition> &family);

std::optional<size_t> finalTypePackParameterIndex(const Definition &def);

bool definitionAllowsEmptyTypePackSpecialization(const Definition &def);

bool bindTemplateArguments(const Definition &baseDef,
                           const std::vector<std::string> &resolvedArgs,
                           const std::vector<TemplateArgument> *resolvedArgDetails,
                           const std::string &displayPath,
                           TemplateArgumentBinding &bindingOut,
                           std::string &error);

std::unordered_set<std::string> collectShadowedTemplateParams(const std::string &definitionPath,
                                                              const std::vector<TemplateRootInfo> &nestedTemplates);

bool isUnderNestedTemplateRoot(const std::string &basePath,
                               const std::string &definitionPath,
                               const std::vector<TemplateRootInfo> &nestedTemplates);

bool specializeTemplateDefinitionFamily(const std::string &basePath,
                                        const TemplateArgumentBinding &templateBinding,
                                        const std::string &specializedBasePath,
                                        const std::string &specializedName,
                                        const std::string &cacheKey,
                                        Context &ctx,
                                        std::string &error);

} // namespace primec
