#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "primec/Ast.h"

namespace primec::semantics {

struct TemplatedFallbackQueryStateEnvelopeSnapshotForTesting {
  bool hasResultType = false;
  bool resultTypeHasValue = false;
  std::string resultValueType;
  std::string resultErrorType;
  std::string mismatchDiagnostic;
};

struct ExplicitTemplateArgResolutionFactForTesting {
  std::string scopePath;
  std::string targetPath;
  std::string explicitArgsText;
  std::string resolvedTypeText;
  bool resolvedConcrete = false;
};

struct ImplicitTemplateArgResolutionFactForTesting {
  std::string scopePath;
  std::string callName;
  std::string targetPath;
  std::string inferredArgsText;
};

struct ExplicitTemplateArgFactConsumptionMetricsForTesting {
  uint64_t hitCount = 0;
};

std::vector<std::string> runSemanticsValidatorExprCaptureSplitStep(const std::string &text);
bool isSimpleCallName(const Expr &expr, const char *nameToMatch);
std::optional<uint64_t> runSemanticsValidatorStatementKnownIterationCountStep(const Expr &countExpr,
                                                                              bool allowBoolCount);
bool runSemanticsValidatorStatementCanIterateMoreThanOnceStep(const Expr &countExpr, bool allowBoolCount);
bool runSemanticsValidatorStatementIsNegativeIntegerLiteralStep(const Expr &expr);
std::string resolveTypePath(const std::string &name, const std::string &namespacePrefix);
bool validateBuiltinMapKeyType(const std::string &typeName,
                               const std::vector<std::string> *templateArgs,
                               std::string &error);
bool splitTopLevelTemplateArgs(const std::string &text, std::vector<std::string> &out);
bool isRootBuiltinName(const std::string &name);
bool isExplicitRemovedCollectionCallAlias(std::string rawPath);
bool getNamespacedCollectionHelperName(const Expr &expr, std::string &collectionOut, std::string &helperOut);
bool lowerMatchToIf(const Expr &expr, Expr &out, std::string &error);
std::string runSemanticsReturnKindNameStep(const Definition &def,
                                           const std::unordered_set<std::string> &structNames,
                                           const std::unordered_map<std::string, std::string> &importAliases,
                                           std::string &error);
void classifyTemplatedFallbackQueryTypeTextForTesting(
    const std::string &queryTypeText,
    TemplatedFallbackQueryStateEnvelopeSnapshotForTesting &out);
bool collectExplicitTemplateArgResolutionFactsForTesting(
    Program program,
    const std::string &entryPath,
    std::string &error,
    std::vector<ExplicitTemplateArgResolutionFactForTesting> &out);
bool collectImplicitTemplateArgResolutionFactsForTesting(
    Program program,
    const std::string &entryPath,
    std::string &error,
    std::vector<ImplicitTemplateArgResolutionFactForTesting> &out);
bool collectExplicitTemplateArgFactConsumptionMetricsForTesting(
    Program program,
    const std::string &entryPath,
    std::string &error,
    ExplicitTemplateArgFactConsumptionMetricsForTesting &out);

} // namespace primec::semantics
