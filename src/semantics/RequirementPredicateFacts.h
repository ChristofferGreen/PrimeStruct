#pragma once

#include "SemanticsHelpers.h"

#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace primec::semantics {

struct RequirementPredicateDefinitionContext {
  std::string definitionPath;
  std::string namespacePrefix;
  std::vector<ParameterInfo> params;
  std::vector<std::string> templateArgs;
  std::unordered_set<std::string> structNames;
  std::unordered_set<std::string> sumNames;
  std::unordered_map<std::string, std::string> importAliases;
};

struct RequirementPredicateOperandFact {
  std::string kind;
  std::string text;
  std::string stableHandle;
  int sourceLine = 0;
  int sourceColumn = 0;
};

struct RequirementPredicateFactDraft {
  std::string predicateKind;
  std::string predicateName;
  std::string relationOperator;
  std::string sourceText;
  std::vector<RequirementPredicateOperandFact> operands;
  std::string evaluationOutcome;
  std::string evaluationDiagnostic;
};

bool isReservedRequirementPredicateNamespace(std::string_view fullPath);

RequirementPredicateFactDraft buildRequirementPredicateFactDraft(
    std::string sourceText,
    int sourceLine,
    int sourceColumn,
    const RequirementPredicateDefinitionContext &context);

} // namespace primec::semantics
