#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "TemplateMonomorphContext.h"
#include "primec/ast/Ast.h"

namespace primec {

uint64_t fnv1a64(const std::string &text);

bool isUnsignedIntegerTemplateArgText(std::string_view text);

TemplateArgument normalizedTemplateArgumentAt(const std::vector<std::string> &args,
                                              const std::vector<TemplateArgument> *details,
                                              size_t index);

const std::vector<TemplateArgument> *matchingTemplateArgumentDetails(
    const std::vector<std::string> &args,
    const std::vector<TemplateArgument> &details);

std::string joinMangledTemplateArgs(const std::vector<std::string> &args,
                                    const std::vector<TemplateArgument> *details = nullptr);

std::string stripMangledTemplateArgKindPrefix(std::string value);

std::string mangleTemplateArgs(const std::vector<std::string> &args,
                               const std::vector<TemplateArgument> *details = nullptr);

bool isPathPrefix(const std::string &prefix, const std::string &path);

bool isGeneratedTemplateSpecializationPath(std::string_view path);

bool isEnclosingTemplateParamName(const std::string &name,
                                  const std::string &namespacePrefix,
                                  const Context &ctx);

bool isEnclosingTypePackParamName(const std::string &name,
                                  const std::string &namespacePrefix,
                                  const Context &ctx);

std::string replacePathPrefix(const std::string &path, const std::string &prefix, const std::string &replacement);

bool hasMathImport(const Context &ctx);

bool extractExplicitBindingType(const Expr &expr, semantics::BindingInfo &infoOut);

std::string bindingTypeToString(const semantics::BindingInfo &info);

std::string generateTemplateParamName(const Definition &def, size_t index);

bool replaceBindingTypeTransform(Expr &binding, const std::string &typeName, std::string &error);

bool isTemplatedAutoCompatDefinitionPath(std::string_view fullPath);

bool applyImplicitAutoTemplates(Program &program, Context &ctx, std::string &error);

} // namespace primec
