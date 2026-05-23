#include "RequirementPredicateFacts.h"
#include "SemanticsValidator.h"

namespace primec::semantics {

bool SemanticsValidator::validateRequirementPredicates() {
  auto bindingTypeText = [](const BindingInfo &binding) {
    if (!binding.typeTemplateArg.empty()) {
      return binding.typeName + "<" + binding.typeTemplateArg + ">";
    }
    return binding.typeName;
  };
  auto hasTransform = [](const auto &transforms, std::string_view name) {
    for (const auto &transform : transforms) {
      if (transform.name == name) {
        return true;
      }
    }
    return false;
  };
  auto isStaticField = [&](const Expr &stmt) {
    return hasTransform(stmt.transforms, "static");
  };
  auto returnTypeTextForDefinition = [&](const Definition &definition) {
    if (const auto bindingIt = returnBindings_.find(definition.fullPath);
        bindingIt != returnBindings_.end() &&
        !bindingIt->second.typeName.empty()) {
      return bindingTypeText(bindingIt->second);
    }
    if (const auto structIt = returnStructs_.find(definition.fullPath);
        structIt != returnStructs_.end() && !structIt->second.empty()) {
      return structIt->second;
    }
    if (const auto kindIt = returnKinds_.find(definition.fullPath);
        kindIt != returnKinds_.end()) {
      if (kindIt->second == ReturnKind::Void) {
        return std::string("void");
      }
      return typeNameForReturnKind(kindIt->second);
    }
    for (const auto &transform : definition.transforms) {
      if (transform.name == "return" && transform.templateArgs.size() == 1) {
        return transform.templateArgs.front();
      }
    }
    return std::string{};
  };
  auto populateCapabilityFacts = [&](RequirementPredicateDefinitionContext &context) {
    context.callables.clear();
    context.structFields.clear();
    context.callables.reserve(program_.definitions.size());
    for (const auto &candidate : program_.definitions) {
      auto paramsIt = paramsByDef_.find(candidate.fullPath);
      const std::string returnType = returnTypeTextForDefinition(candidate);
      if (!returnType.empty()) {
        RequirementPredicateDefinitionContext::CallableFact callable;
        callable.fullPath = candidate.fullPath;
        callable.namespacePrefix = candidate.namespacePrefix;
        callable.templateArgs = candidate.templateArgs;
        callable.isPrivate = hasTransform(candidate.transforms, "private");
        callable.returnType = returnType;
        for (const auto &transform : candidate.transforms) {
          if (transform.name == "effects") {
            callable.effectNames.insert(callable.effectNames.end(),
                                        transform.arguments.begin(),
                                        transform.arguments.end());
          }
        }
        callable.hasReturnExpr = candidate.returnExpr.has_value();
        if (candidate.returnExpr.has_value() &&
            candidate.returnExpr->kind == Expr::Kind::BoolLiteral) {
          callable.returnExprIsBoolLiteral = true;
          callable.returnBoolValue = candidate.returnExpr->boolValue;
        }
        if (paramsIt != paramsByDef_.end()) {
          callable.parameterTypes.reserve(paramsIt->second.size());
          for (const auto &param : paramsIt->second) {
            callable.parameterTypes.push_back(bindingTypeText(param.binding));
          }
        }
        context.callables.push_back(std::move(callable));
      }

      if (structNames_.count(candidate.fullPath) == 0) {
        continue;
      }
      for (const auto &stmt : candidate.statements) {
        if (!stmt.isBinding || isStaticField(stmt) ||
            isCompileTimeTypeBinding(stmt)) {
          continue;
        }
        BindingInfo binding;
        if (!resolveStructFieldBinding(candidate, stmt, binding)) {
          continue;
        }
        RequirementPredicateDefinitionContext::StructFieldFact field;
        field.structPath = candidate.fullPath;
        field.fieldName = stmt.name;
        field.typeText = bindingTypeText(binding);
        field.isPrivate = hasTransform(stmt.transforms, "private");
        context.structFields.push_back(std::move(field));
      }
    }
  };

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
    populateCapabilityFacts(context);

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
