// soa-surface-audit: exempt
#pragma once

#include <string>
#include <string_view>
#include <unordered_map>

#include "primec/ast/Ast.h"

namespace primec {

struct Context;

// Removed/retired name membership delegates to the single authoritative
// sets in primec/support/CollectionSpellingClassifier.h (decision D2 in
// docs/CompatPathResolutionConsolidation.md).
bool isRemovedVectorCompatibilityHelper(const std::string &helperName);

bool isRemovedBorrowedSoaCompatibilityHelper(std::string_view helperName);

bool isRemovedKeyValueCompatibilityHelper(std::string_view helperName);

std::string_view keyValueCompatibilityHelperBase(std::string_view helperName);

bool isTemplateMonomorphMapCollectionRoot(std::string_view value);

bool isExplicitRemovedCollectionMethodAlias(const std::string &receiverTypeName,
                                            std::string rawMethodName);

std::string preferVectorStdlibHelperPath(const std::string &path,
                                         const std::unordered_map<std::string, Definition> &defs);

std::string preferVectorStdlibTemplatePath(const std::string &path, const Context &ctx);

bool definitionAcceptsCallShape(const Definition &def, const Expr &expr);

bool definitionHasArgumentCountMismatch(const Definition &def, const Expr &expr);

bool hasNamedCallArguments(const Expr &expr);

bool isCollectionCompatibilityTemplateFallbackPath(const std::string &path);

bool isExplicitCollectionCompatibilityAliasPath(std::string path);

bool shouldPreserveCompatibilityTemplatePath(const std::string &path, const Context &ctx);

std::string normalizeCollectionReceiverTypeName(std::string value);

bool isCollectionReceiverTypeName(const std::string &value);

std::string unwrapCollectionReceiverEnvelope(std::string typeName, const std::string &typeTemplateArg = {});

}  // namespace primec
