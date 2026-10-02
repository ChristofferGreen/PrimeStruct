#include "RequirementPredicateFacts.h"

#include "primec/frontend/StringLiteral.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <optional>
#include <sstream>
#include "RequirementPredicateFactsInternal.h"

namespace primec::semantics {
using namespace requirementFacts;


bool isReservedRequirementPredicateNamespace(std::string_view fullPath) {
  return fullPath == "/std/meta" || fullPath.rfind("/std/meta/", 0) == 0;
}

RequirementPredicateFactDraft buildRequirementPredicateFactDraft(
    std::string sourceText,
    int sourceLine,
    int sourceColumn,
    const RequirementPredicateDefinitionContext &context) {
  sourceText = trimRequirementText(sourceText);
  RequirementPredicateFactDraft fact;
  fact.sourceText = sourceText;
  fact.evaluationOutcome = "invalid_evaluation";
  fact.evaluationDiagnostic = "requirement predicate evaluation pending";

  if (const auto relation = findTopLevelRequirementRelation(sourceText);
      relation.has_value()) {
    fact.relationOperator = relation->second;
    if (relation->second == "==" || relation->second == "!=") {
      fact.predicateKind = "predicate_call";
      fact.predicateName = relation->second == "==" ? "/std/meta/type_equals"
                                                    : "/std/meta/type_not_equals";
    } else {
      fact.predicateKind = "predicate_call";
      if (relation->second == "<") {
        fact.predicateName = "/std/meta/value_less";
      } else if (relation->second == "<=") {
        fact.predicateName = "/std/meta/value_less_equal";
      } else if (relation->second == ">") {
        fact.predicateName = "/std/meta/value_greater";
      } else if (relation->second == ">=") {
        fact.predicateName = "/std/meta/value_greater_equal";
      }
    }
    fact.operands.push_back(classifyRequirementOperand(
        sourceText.substr(0, relation->first),
        fact.predicateName,
        0,
        false,
        sourceLine,
        sourceColumn));
    fact.operands.push_back(classifyRequirementOperand(
        sourceText.substr(relation->first + relation->second.size()),
        fact.predicateName,
        1,
        false,
        sourceLine,
        sourceColumn));
    fact.sourceText = canonicalCallSourceText(fact.predicateName, fact.operands);
    evaluateRequirementPredicate(fact, context);
    return fact;
  }

  if (const auto callParen = findTopLevelRequirementCallParen(sourceText);
      callParen.has_value() && sourceText.back() == ')') {
    const std::string calleeText =
        trimRequirementText(std::string_view(sourceText).substr(0, *callParen));
    const auto templateStart = findTopLevelTemplateStart(calleeText);
    fact.predicateKind = "predicate_call";
    fact.predicateName = canonicalPredicateName(
        templateStart.has_value() ? calleeText.substr(0, *templateStart)
                                  : calleeText);
    if (templateStart.has_value() && calleeText.back() == '>') {
      const std::string templateArgs =
          calleeText.substr(*templateStart + 1, calleeText.size() - *templateStart - 2);
      for (const auto &arg : splitRequirementTopLevelList(templateArgs)) {
        fact.operands.push_back(classifyRequirementOperand(arg,
                                                           fact.predicateName,
                                                           fact.operands.size(),
                                                           true,
                                                           sourceLine,
                                                           sourceColumn));
      }
    }
    const std::string callArgs =
        sourceText.substr(*callParen + 1, sourceText.size() - *callParen - 2);
    for (const auto &arg : splitRequirementTopLevelList(callArgs)) {
      fact.operands.push_back(classifyRequirementOperand(arg,
                                                         fact.predicateName,
                                                         fact.operands.size(),
                                                         false,
                                                         sourceLine,
                                                         sourceColumn));
    }
    if (isBuiltinRequirementPredicate(fact.predicateName)) {
      fact.sourceText = canonicalCallSourceText(fact.predicateName, fact.operands);
    }
    evaluateRequirementPredicate(fact, context);
    return fact;
  }

  fact.predicateKind = "unsupported";
  fact.predicateName = {};
  fact.operands.push_back(classifyRequirementOperand(sourceText,
                                                     fact.predicateName,
                                                     0,
                                                     false,
                                                     sourceLine,
                                                     sourceColumn));
  fact.evaluationDiagnostic = "unsupported requirement predicate shape";
  return fact;
}

} // namespace primec::semantics
