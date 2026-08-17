#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "SemanticsHelpers.h"
#include "primec/semantics/Semantics.h"

namespace primec {

struct BuiltinSoaReturnInfo {
  semantics::BindingInfo binding;
  std::string namespacePrefix;
  std::unordered_set<std::string> templateParams;
};

struct BuiltinSoaReceiverBindingInfo {
  semantics::BindingInfo binding;
  bool borrowed = false;
};

bool localImportPathCoversTarget(const std::string &importPath, const std::string &targetPath);
bool hasVisiblePublicSoaHelperDefinition(const Program &program, std::string_view helperName);
bool hasVisibleRootSoaHelper(const Program &program, std::string_view helperName);
bool hasVisibleRootSoaHelperForReceiverType(const Program &program,
                                            std::string_view helperName,
                                            std::string_view receiverTypeName);
bool hasVisibleExperimentalSoaSamePathHelper(const Program &program,
                                             std::string_view helperName);
bool hasVisibleRootExperimentalSoaHelper(const Program &program,
                                         std::string_view helperName);
std::vector<std::string> candidateDefinitionPaths(const Expr &expr, const std::string &definitionNamespace);
bool isFieldOnlyStructDefinition(const Definition &def);
bool isReflectEnabledStructDefinition(const Definition &def);
bool validateBuiltinSoaHelperReturnMetadataRequirements(Program &program,
                                                        std::string &error);
std::optional<semantics::BindingInfo> extractParsedBindingInfo(
    const Expr &expr,
    const std::unordered_set<std::string> *structTypes = nullptr);
std::optional<semantics::BindingInfo> extractParsedOrExperimentalSoaBindingInfo(
    const Expr &expr,
    const std::unordered_set<std::string> *structTypes = nullptr);
std::string resolveStructReceiverPathFromBinding(
    const semantics::BindingInfo &binding,
    const std::string &namespacePrefix,
    const std::unordered_set<std::string> &structPaths);
std::optional<semantics::BindingInfo> extractBuiltinCollectionBindingFromTypeText(
    const std::string &typeText,
    std::string_view expectedBase);
std::optional<semantics::BindingInfo> extractBuiltinCollectionBindingFromWrappedTypeText(
    const std::string &typeText,
    std::string_view expectedBase);
std::optional<BuiltinSoaReceiverBindingInfo> extractBuiltinSoaReceiverBinding(
    const semantics::BindingInfo &binding);
std::string stripSoaSurfaceHelperPrefix(std::string_view rawName);
std::string builtinSoaConversionMethodName(std::string_view methodName);
std::string builtinSoaAccessHelperName(std::string_view rawName);
std::string borrowedBuiltinSoaAccessHelperName(std::string_view helperName);
std::string builtinSoaCountHelperName(std::string_view rawName);
std::string borrowedBuiltinSoaCountHelperName(std::string_view helperName);
bool isOldExplicitSoaCountHelperName(std::string_view rawName);
std::string builtinSoaMutatorHelperName(std::string_view rawName);
std::string oldExplicitSoaMutatorHelperName(std::string_view rawName);

} // namespace primec
