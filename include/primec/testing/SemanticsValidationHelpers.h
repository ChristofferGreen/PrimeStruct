#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "primec/Ast.h"

namespace primec::semantics {

bool isSimpleCallName(const Expr &expr, const char *nameToMatch);
std::string resolveTypePath(const std::string &name, const std::string &namespacePrefix);
bool validateBuiltinMapKeyType(const std::string &typeName,
                               const std::vector<std::string> *templateArgs,
                               std::string &error);
bool splitTopLevelTemplateArgs(const std::string &text, std::vector<std::string> &out);
bool isRootBuiltinName(const std::string &name);
bool getBuiltinCollectionName(const Expr &expr, std::string &out);
bool isExplicitRemovedCollectionMethodAlias(const std::string &receiverPath, std::string rawMethodName);
bool isExplicitRemovedCollectionCallAlias(std::string rawPath);
bool getNamespacedCollectionHelperName(const Expr &expr, std::string &collectionOut, std::string &helperOut);
bool lowerMatchToIf(const Expr &expr, Expr &out, std::string &error);
std::string runSemanticsReturnKindNameStep(const Definition &def,
                                           const std::unordered_set<std::string> &structNames,
                                           const std::unordered_map<std::string, std::string> &importAliases,
                                           std::string &error);

} // namespace primec::semantics
