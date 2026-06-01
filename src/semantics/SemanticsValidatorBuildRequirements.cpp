#include "RequirementPredicateFacts.h"
#include "SemanticsValidator.h"

#include <algorithm>
#include <sstream>

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
  auto compileTimeEffectsForDefinition = [](const Definition &definition) {
    std::vector<std::string> effects;
    for (const Transform &transform : definition.transforms) {
      if (transform.name == "effects" && transform.templateArgs.size() == 1 &&
          transform.templateArgs.front() == "compiletime") {
        effects.insert(effects.end(),
                       transform.arguments.begin(),
                       transform.arguments.end());
      }
    }
    std::sort(effects.begin(), effects.end());
    effects.erase(std::unique(effects.begin(), effects.end()), effects.end());
    return effects;
  };
  auto formatRequirementPredicateDiagnostic =
      [](const Definition &definition,
         std::string_view transformName,
         int sourceLine,
         int sourceColumn,
         std::string_view argument,
         const RequirementPredicateFactDraft &fact) {
        const std::string predicateName =
            fact.predicateName.empty() ? std::string(argument)
                                       : fact.predicateName;
        const bool unsatisfied = fact.evaluationOutcome == "unsatisfied";
        std::ostringstream out;
        out << "direct requirement check failed on " << definition.fullPath
            << '\n';
        if (unsatisfied) {
          out << "requirement predicate not satisfied: " << predicateName
              << '\n';
          out << "category: unsatisfied requirement predicate\n";
        } else if (fact.evaluationOutcome == "denied_effect") {
          out << "compile-time effect opt-in rejected: " << predicateName
              << '\n';
          out << "category: denied compile-time effect\n";
        } else {
          out << "invalid requirement predicate " << predicateName << '\n';
          out << "category: invalid requirement predicate evaluation\n";
        }
        out << transformName << " transform: " << definition.fullPath << " at "
            << sourceLine << ':' << sourceColumn << '\n';
        out << "predicate source: "
            << (fact.sourceText.empty() ? std::string(argument)
                                        : fact.sourceText)
            << '\n';
        out << "concrete facts:";
        if (fact.operands.empty()) {
          out << " none\n";
        } else {
          out << '\n';
          for (const RequirementPredicateOperandFact &operand :
               fact.operands) {
            out << "- " << operand.stableHandle << " kind=" << operand.kind
                << " text=" << operand.text << " at "
                << operand.sourceLine << ':' << operand.sourceColumn
                << '\n';
          }
        }
        out << "result: " << fact.evaluationDiagnostic << '\n';
        if (unsatisfied) {
          out << "hint: pass values or types that satisfy the require(...) "
                 "predicate, or relax the constrained definition.";
        } else if (fact.evaluationOutcome == "denied_effect") {
          out << "hint: add effects<compiletime>(...) for the required "
                 "compile-time host effect, or make the predicate pure.";
        } else {
          out << "hint: make the require(...) predicate a supported "
                 "compile-time predicate with available concrete facts.";
        }
        return out.str();
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
          if (transform.name == "effects" && transform.templateArgs.empty()) {
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
    context.compileTimeEffects = compileTimeEffectsForDefinition(definition);
    context.structNames = structNames_;
    context.sumNames = sumNames_;
    context.importAliases = importAliases_;
    if (paramsIt != paramsByDef_.end()) {
      context.params = paramsIt->second;
    }
    populateCapabilityFacts(context);

    for (const auto &transform : definition.transforms) {
      if (transform.name != "require" && transform.name != "restrict") {
        continue;
      }
      const int sourceLine =
          transform.sourceLine > 0 ? transform.sourceLine : definition.sourceLine;
      const int sourceColumn =
          transform.sourceColumn > 0 ? transform.sourceColumn : definition.sourceColumn;
      const auto &predicateArgs =
          transform.name == "restrict" ? transform.templateArgs : transform.arguments;
      for (const auto &argument : predicateArgs) {
        RequirementPredicateFactDraft fact =
            buildRequirementPredicateFactDraft(argument,
                                               sourceLine,
                                               sourceColumn,
                                               context);
        if (fact.evaluationDiagnostic.find("deferred for unresolved") !=
            std::string::npos) {
          continue;
        }
        if (fact.evaluationOutcome == "satisfied") {
          continue;
        }
        return failDefinitionDiagnostic(
            definition,
            formatRequirementPredicateDiagnostic(definition,
                                                 transform.name,
                                                 sourceLine,
                                                 sourceColumn,
                                                 argument,
                                                 fact));
      }
    }
  }

  return true;
}

} // namespace primec::semantics
