#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

#include "primec/semantics/Semantics.h"

#include "SemanticsHelpers.h"

namespace primec {

struct SoaFieldViewFieldInfo {
  size_t index = 0;
  std::string typeText;
};

bool isExperimentalKeyValueTypeText(const std::string &typeText);
std::optional<semantics::BindingInfo> extractBorrowedExperimentalKeyValueBinding(const Expr &expr);
std::string bindingTypeText(const semantics::BindingInfo &binding);
bool isExperimentalKeyValueValueBinding(const semantics::BindingInfo &binding);
std::optional<semantics::BindingInfo> extractExperimentalKeyValueValueBinding(const Expr &expr);
bool isBorrowedExperimentalKeyValueBinding(const semantics::BindingInfo &binding);
std::optional<semantics::BindingInfo> extractBorrowedExperimentalKeyValueReturnBinding(
    const Definition &def);
std::optional<semantics::BindingInfo> extractExperimentalKeyValueValueReturnBinding(const Definition &def);
std::string borrowedExperimentalKeyValueHelperName(std::string_view methodName);
std::string experimentalKeyValueValueHelperName(std::string_view methodName);
bool isBuiltinVectorTypeText(const std::string &typeText);
bool isBuiltinSoaVectorTypeText(const std::string &typeText);
bool isExperimentalSoaVectorBaseName(const std::string &typeName);
bool isExperimentalSoaVectorBinding(const semantics::BindingInfo &binding);
bool isExperimentalSoaVectorOrBorrowedTypeText(std::string typeText);
bool extractExperimentalSoaVectorElementTypeForFieldViewRewrite(const semantics::BindingInfo &binding,
                                                                const std::unordered_map<std::string, std::string>
                                                                    &specializedSoaVectorElementTypes,
                                                                std::string &elemTypeOut);
bool isBuiltinVectorBinding(const semantics::BindingInfo &binding);
bool isBuiltinSoaVectorBinding(const semantics::BindingInfo &binding);
bool isBuiltinSoaVectorOrBorrowedBinding(const semantics::BindingInfo &binding);
std::optional<semantics::BindingInfo> extractBuiltinVectorBinding(const Expr &expr);
std::optional<semantics::BindingInfo> extractBuiltinSoaVectorBinding(const Expr &expr);
std::optional<semantics::BindingInfo> extractExperimentalSoaVectorBinding(const Expr &expr);
std::optional<semantics::BindingInfo> extractBuiltinVectorReturnBinding(const Definition &def);
std::optional<semantics::BindingInfo> extractBuiltinSoaVectorReturnBindingImpl(
    const Definition &def,
    bool allowBorrowed);
std::optional<semantics::BindingInfo> extractBuiltinSoaVectorReturnBinding(const Definition &def);
std::optional<semantics::BindingInfo> extractBuiltinSoaVectorOrBorrowedReturnBinding(
    const Definition &def);
std::optional<semantics::BindingInfo> extractExperimentalSoaVectorReturnBindingImpl(
    const Definition &def,
    bool allowBorrowed);
std::optional<semantics::BindingInfo> extractExperimentalSoaVectorOrBorrowedReturnBinding(
    const Definition &def);
bool extractExperimentalSoaVectorElementTypeForFieldViewRewrite(const semantics::BindingInfo &binding,
                                                                std::string &elemTypeOut);
Expr makeI32LiteralExpr(uint64_t value, int sourceLine, int sourceColumn);
std::optional<size_t> extractNonNegativeI32LiteralIndex(const Expr &expr);
bool extractSoaFieldViewElementTypeText(std::string typeText, std::string &elemTypeOut);
bool extractSoaFieldViewElementTypeFromBinding(const semantics::BindingInfo &binding,
                                               std::string &elemTypeOut);
std::string qualifySoaFieldViewTypeText(const std::string &typeText,
                                        const std::string &namespacePrefix,
                                        const std::unordered_set<std::string> &structPaths);
bool extractExperimentalSoaColumnElementTypeFromSpecializedDefinition(
    const Definition &def,
    std::string &elemTypeOut);
bool extractExperimentalSoaVectorElementTypeFromSpecializedDefinition(
    const Definition &def,
    const std::unordered_map<std::string, const Definition *> &definitionMap,
    std::string &elemTypeOut);
std::unordered_map<std::string, std::string> buildSpecializedExperimentalSoaVectorElementTypes(
    const Program &program);
bool extractExperimentalSoaVectorElementTypeForToAosRewrite(const semantics::BindingInfo &binding,
                                                            std::string &elemTypeOut);
std::optional<semantics::BindingInfo> extractExperimentalSoaVectorFieldViewReceiverBinding(const Expr &expr);

} // namespace primec
