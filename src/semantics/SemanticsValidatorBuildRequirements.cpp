#include "RequirementPredicateFacts.h"
#include "SemanticsValidator.h"

namespace primec::semantics {

bool SemanticsValidator::validateRequirementPredicates() {
  for (const auto &definition : program_.definitions) {
    DefinitionContextScope definitionScope(*this, definition);
    if (isReservedRequirementPredicateNamespace(definition.fullPath)) {
      return failDefinitionDiagnostic(
          definition,
          "/std/meta is reserved for builtin requirement predicates: " +
              definition.fullPath);
    }
  }

  for (const auto &definition : program_.definitions) {
    const auto paramsIt = paramsByDef_.find(definition.fullPath);
    RequirementPredicateDefinitionContext context;
    context.definitionPath = definition.fullPath;
    context.namespacePrefix = definition.namespacePrefix;
    context.templateArgs = definition.templateArgs;
    context.structNames = structNames_;
    context.sumNames = sumNames_;
    context.importAliases = importAliases_;
    if (paramsIt != paramsByDef_.end()) {
      context.params = paramsIt->second;
    }

    for (const auto &transform : definition.transforms) {
      if (transform.name != "require") {
        continue;
      }
      const int sourceLine =
          transform.sourceLine > 0 ? transform.sourceLine : definition.sourceLine;
      const int sourceColumn =
          transform.sourceColumn > 0 ? transform.sourceColumn : definition.sourceColumn;
      for (const auto &argument : transform.arguments) {
        RequirementPredicateFactDraft fact =
            buildRequirementPredicateFactDraft(argument,
                                               sourceLine,
                                               sourceColumn,
                                               context);
        if (fact.evaluationDiagnostic.find("deferred for unresolved type facts") !=
            std::string::npos) {
          continue;
        }
        if (fact.evaluationOutcome == "satisfied") {
          continue;
        }
        if (fact.evaluationOutcome == "unsatisfied") {
          return failDefinitionDiagnostic(
              definition,
              "requirement predicate not satisfied: " + fact.predicateName +
                  " on " + definition.fullPath + ": " +
                  fact.evaluationDiagnostic);
        }
        return failDefinitionDiagnostic(
            definition,
            "invalid requirement predicate " +
                (fact.predicateName.empty() ? argument : fact.predicateName) +
                " on " + definition.fullPath + ": " +
                fact.evaluationDiagnostic);
      }
    }
  }

  return true;
}

} // namespace primec::semantics
