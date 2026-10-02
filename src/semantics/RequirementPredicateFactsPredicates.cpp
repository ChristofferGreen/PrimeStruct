#include "RequirementPredicateFacts.h"

#include "primec/frontend/StringLiteral.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <optional>
#include <sstream>
#include "RequirementPredicateFactsInternal.h"

namespace primec::semantics {
namespace requirementFacts {


void evaluateBuiltinTypePredicate(RequirementPredicateFactDraft &fact,
                                  const RequirementPredicateDefinitionContext &context) {
  const std::string &predicate = fact.predicateName;
  const std::size_t expectedOperandCount =
      (predicate == "/std/meta/type_equals" ||
       predicate == "/std/meta/type_not_equals")
          ? 2u
          : 1u;
  if (fact.operands.size() != expectedOperandCount) {
    fact.evaluationOutcome = "invalid_evaluation";
    fact.evaluationDiagnostic =
        "requirement predicate " + predicate + " expects " +
        std::to_string(expectedOperandCount) + " type operand" +
        (expectedOperandCount == 1 ? "" : "s");
    return;
  }

  std::vector<TypeOperandResolution> resolved;
  resolved.reserve(fact.operands.size());
  bool deferred = false;
  for (const auto &operand : fact.operands) {
    TypeOperandResolution resolution =
        resolveTypeOperand(context, operand, predicate);
    if (resolution.status == TypeOperandStatus::Invalid) {
      fact.evaluationOutcome = "invalid_evaluation";
      fact.evaluationDiagnostic = std::move(resolution.diagnostic);
      return;
    }
    if (resolution.status == TypeOperandStatus::Deferred) {
      deferred = true;
    }
    resolved.push_back(std::move(resolution));
  }
  if (deferred) {
    fact.evaluationOutcome = "invalid_evaluation";
    fact.evaluationDiagnostic =
        "requirement predicate evaluation deferred for unresolved type facts";
    return;
  }

  bool satisfied = false;
  if (predicate == "/std/meta/type_equals") {
    satisfied = resolved[0].canonicalType == resolved[1].canonicalType;
    fact.evaluationDiagnostic =
        satisfied ? "type equality satisfied"
                  : "type equality failed: " + resolved[0].canonicalType +
                        " != " + resolved[1].canonicalType;
  } else if (predicate == "/std/meta/type_not_equals") {
    satisfied = resolved[0].canonicalType != resolved[1].canonicalType;
    fact.evaluationDiagnostic =
        satisfied ? "type inequality satisfied"
                  : "type inequality failed: both operands are " +
                        resolved[0].canonicalType;
  } else if (predicate == "/std/meta/is_type") {
    satisfied = true;
    fact.evaluationDiagnostic =
        "type fact resolved: " + resolved[0].canonicalType;
  } else if (predicate == "/std/meta/is_struct") {
    satisfied = context.structNames.count(resolved[0].canonicalType) > 0;
    fact.evaluationDiagnostic =
        satisfied ? "struct type predicate satisfied"
                  : "struct type predicate failed: " +
                        resolved[0].canonicalType + " is not a struct";
  } else if (predicate == "/std/meta/is_sum") {
    satisfied = context.sumNames.count(resolved[0].canonicalType) > 0;
    fact.evaluationDiagnostic =
        satisfied ? "sum type predicate satisfied"
                  : "sum type predicate failed: " + resolved[0].canonicalType +
                        " is not a sum";
  }
  fact.evaluationOutcome = satisfied ? "satisfied" : "unsatisfied";
}

void evaluateBuiltinValuePredicate(RequirementPredicateFactDraft &fact,
                                   const RequirementPredicateDefinitionContext &context) {
  if (fact.operands.size() != 2) {
    fact.evaluationOutcome = "invalid_evaluation";
    fact.evaluationDiagnostic =
        "requirement predicate " + fact.predicateName +
        " expects two compile-time integer value operands";
    return;
  }

  std::vector<ValueOperandResolution> resolved;
  resolved.reserve(fact.operands.size());
  bool deferred = false;
  std::vector<std::string> runtimeOperandKinds(fact.operands.size());
  bool runtimeCheckable = false;
  for (std::size_t operandIndex = 0; operandIndex < fact.operands.size();
       ++operandIndex) {
    const auto &operand = fact.operands[operandIndex];
    ValueOperandResolution resolution =
        resolveValueOperand(context, operand, fact.predicateName);
    if (resolution.status == ValueOperandStatus::Invalid) {
      std::string runtimeKind = runtimeContractOperandKind(context, operand);
      if (!runtimeKind.empty()) {
        runtimeOperandKinds[operandIndex] = std::move(runtimeKind);
        runtimeCheckable = true;
        resolved.push_back({ValueOperandStatus::Resolved, 0, {}});
        continue;
      }
      fact.evaluationOutcome = "invalid_evaluation";
      fact.evaluationDiagnostic = std::move(resolution.diagnostic);
      return;
    }
    if (resolution.status == ValueOperandStatus::Deferred) {
      deferred = true;
    }
    resolved.push_back(std::move(resolution));
  }
  if (deferred) {
    fact.evaluationOutcome = "invalid_evaluation";
    fact.evaluationDiagnostic =
        "requirement predicate evaluation deferred for unresolved value facts";
    return;
  }
  if (runtimeCheckable) {
    for (std::size_t operandIndex = 0; operandIndex < fact.operands.size();
         ++operandIndex) {
      if (runtimeOperandKinds[operandIndex].empty()) {
        continue;
      }
      fact.operands[operandIndex].kind = runtimeOperandKinds[operandIndex];
      fact.operands[operandIndex].stableHandle = requirementOperandStableHandle(
          fact.operands[operandIndex].kind, fact.operands[operandIndex].text);
    }
    fact.evaluationOutcome = "runtime_contract";
    fact.evaluationDiagnostic =
        "contract deferred to runtime precondition check: " + fact.sourceText;
    return;
  }

  const std::uint64_t left = resolved[0].unsignedValue;
  const std::uint64_t right = resolved[1].unsignedValue;
  bool satisfied = false;
  std::string comparison;
  if (fact.predicateName == "/std/meta/value_equals") {
    satisfied = left == right;
    comparison = "==";
  } else if (fact.predicateName == "/std/meta/value_not_equals") {
    satisfied = left != right;
    comparison = "!=";
  } else if (fact.predicateName == "/std/meta/value_less") {
    satisfied = left < right;
    comparison = "<";
  } else if (fact.predicateName == "/std/meta/value_less_equal") {
    satisfied = left <= right;
    comparison = "<=";
  } else if (fact.predicateName == "/std/meta/value_greater") {
    satisfied = left > right;
    comparison = ">";
  } else if (fact.predicateName == "/std/meta/value_greater_equal") {
    satisfied = left >= right;
    comparison = ">=";
  }

  fact.evaluationOutcome = satisfied ? "satisfied" : "unsatisfied";
  fact.evaluationDiagnostic =
      std::string("value predicate ") + (satisfied ? "satisfied: " : "failed: ") +
      std::to_string(left) + " " + comparison + " " + std::to_string(right);
}

void evaluateHasTraitPredicate(RequirementPredicateFactDraft &fact,
                               const RequirementPredicateDefinitionContext &context) {
  if (fact.operands.size() != 2 && fact.operands.size() != 3) {
    fact.evaluationOutcome = "invalid_evaluation";
    fact.evaluationDiagnostic =
        "requirement predicate /std/meta/has_trait expects one type operand, "
        "one trait-name argument, and optional element type";
    return;
  }

  std::vector<TypeOperandResolution> resolved;
  resolved.reserve(fact.operands.size());
  TypeOperandResolution target = resolveTypeOperand(context, fact.operands[0], fact.predicateName);
  if (target.status == TypeOperandStatus::Invalid) {
    fact.evaluationOutcome = "invalid_evaluation";
    fact.evaluationDiagnostic = std::move(target.diagnostic);
    return;
  }
  if (target.status == TypeOperandStatus::Deferred) {
    fact.evaluationOutcome = "invalid_evaluation";
    fact.evaluationDiagnostic =
        "requirement predicate evaluation deferred for unresolved type facts";
    return;
  }
  resolved.push_back(target);

  std::optional<TypeOperandResolution> elem;
  if (fact.operands.size() == 3) {
    elem = resolveTypeOperand(context, fact.operands[1], fact.predicateName);
    if (elem->status == TypeOperandStatus::Invalid) {
      fact.evaluationOutcome = "invalid_evaluation";
      fact.evaluationDiagnostic = std::move(elem->diagnostic);
      return;
    }
    if (elem->status == TypeOperandStatus::Deferred) {
      fact.evaluationOutcome = "invalid_evaluation";
      fact.evaluationDiagnostic =
          "requirement predicate evaluation deferred for unresolved type facts";
      return;
    }
    resolved.push_back(*elem);
  }

  const RequirementPredicateOperandFact &traitOperand =
      fact.operands.size() == 3 ? fact.operands[2] : fact.operands[1];
  std::string traitName;
  std::string diagnostic;
  if (!parseCompileTimeNameOperand(traitOperand,
                                   fact.predicateName,
                                   "trait",
                                   traitName,
                                   diagnostic)) {
    fact.evaluationOutcome = "invalid_evaluation";
    fact.evaluationDiagnostic = std::move(diagnostic);
    return;
  }

  const bool isAdditive = traitName == "Additive";
  const bool isMultiplicative = traitName == "Multiplicative";
  const bool isComparable = traitName == "Comparable";
  const bool isIndexable = traitName == "Indexable";
  const bool isCollection = traitName == "Collection";
  const bool isKeyValue = traitName == "KeyValue";
  if (!isAdditive && !isMultiplicative && !isComparable && !isIndexable &&
      !isCollection && !isKeyValue) {
    fact.evaluationOutcome = "invalid_evaluation";
    fact.evaluationDiagnostic =
        "requirement predicate /std/meta/has_trait does not support trait: " +
        traitName;
    return;
  }
  if (isIndexable && !elem.has_value()) {
    fact.evaluationOutcome = "invalid_evaluation";
    fact.evaluationDiagnostic =
        "requirement predicate /std/meta/has_trait Indexable requires type "
        "and element type operands";
    return;
  }
  if (!isIndexable && elem.has_value()) {
    fact.evaluationOutcome = "invalid_evaluation";
    fact.evaluationDiagnostic =
        "requirement predicate /std/meta/has_trait " + traitName +
        " requires exactly one type operand";
    return;
  }

  bool satisfied = false;
  std::string requirementText;
  const std::string typePath = typePathForCanonical(target.canonicalType);
  if (isCollection || isKeyValue) {
    requirementText = std::string("[") +
                      (isCollection ? "collection_type" : "key_value_type") +
                      "] struct annotation";
    satisfied = hasAnnotatedStructTrait(context, target.canonicalType, traitName);
  } else if (isAdditive) {
    std::vector<TypeOperandResolution> callTypes = {target, target};
    requirementText = callableDisplaySignature("plus", callTypes, 2, target.canonicalType);
    if (isNumericTraitType(target.canonicalType)) {
      satisfied = true;
    } else {
      satisfied = callableSignatureMatches(context,
                                           typePath + "/plus",
                                           callTypes,
                                           2,
                                           target.canonicalType);
    }
  } else if (isMultiplicative) {
    std::vector<TypeOperandResolution> callTypes = {target, target};
    requirementText = callableDisplaySignature("multiply", callTypes, 2, target.canonicalType);
    if (isNumericTraitType(target.canonicalType)) {
      satisfied = true;
    } else {
      satisfied = callableSignatureMatches(context,
                                           typePath + "/multiply",
                                           callTypes,
                                           2,
                                           target.canonicalType);
    }
  } else if (isComparable) {
    requirementText = "equal(" + target.canonicalType + ", " +
                      target.canonicalType + ") -> bool and less_than(" +
                      target.canonicalType + ", " + target.canonicalType +
                      ") -> bool";
    if (isComparableBuiltinTraitType(target.canonicalType)) {
      satisfied = true;
    } else {
      std::vector<TypeOperandResolution> callTypes = {target, target};
      satisfied = callableSignatureMatches(context,
                                           typePath + "/equal",
                                           callTypes,
                                           2,
                                           "bool") &&
                  callableSignatureMatches(context,
                                           typePath + "/less_than",
                                           callTypes,
                                           2,
                                           "bool");
    }
  } else if (isIndexable) {
    requirementText = "count(" + target.canonicalType +
                      ") -> i32 and at(" + target.canonicalType +
                      ", i32) -> " + elem->canonicalType;
    if (isIndexableBuiltinTraitType(target.canonicalType, elem->canonicalType)) {
      satisfied = true;
    } else {
      std::vector<TypeOperandResolution> countTypes = {target};
      TypeOperandResolution intType;
      intType.status = TypeOperandStatus::Resolved;
      intType.canonicalType = "i32";
      std::vector<TypeOperandResolution> atTypes = {target, intType};
      satisfied = callableSignatureMatches(context,
                                           typePath + "/count",
                                           countTypes,
                                           1,
                                           "i32") &&
                  callableSignatureMatches(context,
                                           typePath + "/at",
                                           atTypes,
                                           2,
                                           elem->canonicalType);
    }
  }

  fact.evaluationOutcome = satisfied ? "satisfied" : "unsatisfied";
  fact.evaluationDiagnostic =
      satisfied ? "trait predicate satisfied: " + traitName + " with " +
                      formatResolvedTypeFacts(resolved)
                : "trait predicate failed: " + traitName + " requires " +
                      requirementText + " with " + formatResolvedTypeFacts(resolved);
}

void evaluateSupportsCallPredicate(RequirementPredicateFactDraft &fact,
                                   const RequirementPredicateDefinitionContext &context) {
  if (fact.operands.size() < 2) {
    fact.evaluationOutcome = "invalid_evaluation";
    fact.evaluationDiagnostic =
        "requirement predicate /std/meta/supports_call expects at least one "
        "type operand and one call-name argument";
    return;
  }

  const std::size_t typeOperandCount = fact.operands.size() - 1;
  std::vector<TypeOperandResolution> resolved;
  resolved.reserve(typeOperandCount);
  for (std::size_t i = 0; i < typeOperandCount; ++i) {
    TypeOperandResolution resolution =
        resolveTypeOperand(context, fact.operands[i], fact.predicateName);
    if (resolution.status == TypeOperandStatus::Invalid) {
      fact.evaluationOutcome = "invalid_evaluation";
      fact.evaluationDiagnostic = std::move(resolution.diagnostic);
      return;
    }
    if (resolution.status == TypeOperandStatus::Deferred) {
      fact.evaluationOutcome = "invalid_evaluation";
      fact.evaluationDiagnostic =
          "requirement predicate evaluation deferred for unresolved type facts";
      return;
    }
    resolved.push_back(std::move(resolution));
  }

  std::string callName;
  std::string diagnostic;
  if (!parseCompileTimeNameOperand(fact.operands.back(),
                                   fact.predicateName,
                                   "call",
                                   callName,
                                   diagnostic)) {
    fact.evaluationOutcome = "invalid_evaluation";
    fact.evaluationDiagnostic = std::move(diagnostic);
    return;
  }
  if (callName.empty()) {
    fact.evaluationOutcome = "invalid_evaluation";
    fact.evaluationDiagnostic =
        "requirement predicate /std/meta/supports_call requires non-empty call name";
    return;
  }
  const std::string requestedPath =
      callName.front() == '/' ? callName : "/" + callName;
  const std::size_t paramCount = resolved.size() - 1;
  const std::string expectedReturn = resolved.back().canonicalType;
  const bool satisfied = callableSignatureMatches(context,
                                                  requestedPath,
                                                  resolved,
                                                  paramCount,
                                                  expectedReturn);
  fact.evaluationOutcome = satisfied ? "satisfied" : "unsatisfied";
  fact.evaluationDiagnostic =
      satisfied ? "call support predicate satisfied: " +
                      callableDisplaySignature(requestedPath,
                                               resolved,
                                               paramCount,
                                               expectedReturn)
                : "call support predicate failed: no visible callable matches " +
                      callableDisplaySignature(requestedPath,
                                               resolved,
                                               paramCount,
                                               expectedReturn) +
                      " with " + formatResolvedTypeFacts(resolved);
}

void evaluateCanConstructPredicate(RequirementPredicateFactDraft &fact,
                                   const RequirementPredicateDefinitionContext &context) {
  if (fact.operands.empty()) {
    fact.evaluationOutcome = "invalid_evaluation";
    fact.evaluationDiagnostic =
        "requirement predicate /std/meta/can_construct expects a target type";
    return;
  }

  std::vector<TypeOperandResolution> resolved;
  resolved.reserve(fact.operands.size());
  for (const auto &operand : fact.operands) {
    TypeOperandResolution resolution =
        resolveTypeOperand(context, operand, fact.predicateName);
    if (resolution.status == TypeOperandStatus::Invalid) {
      fact.evaluationOutcome = "invalid_evaluation";
      fact.evaluationDiagnostic = std::move(resolution.diagnostic);
      return;
    }
    if (resolution.status == TypeOperandStatus::Deferred) {
      fact.evaluationOutcome = "invalid_evaluation";
      fact.evaluationDiagnostic =
          "requirement predicate evaluation deferred for unresolved type facts";
      return;
    }
    resolved.push_back(std::move(resolution));
  }

  const std::string &targetType = resolved.front().canonicalType;
  const bool isStruct = context.structNames.count(targetType) > 0;
  bool satisfied = false;
  std::string expectedShape;
  if (isStruct) {
    const auto fields = visibleStructFields(context, targetType);
    expectedShape = targetType + "{";
    for (std::size_t i = 0; i < fields.size(); ++i) {
      if (i != 0) {
        expectedShape += ", ";
      }
      expectedShape += canonicalizeResolvedType(context, fields[i]->typeText);
    }
    expectedShape += "}";
    if (fields.size() == resolved.size() - 1) {
      satisfied = true;
      for (std::size_t i = 0; i < fields.size(); ++i) {
        const std::string fieldType =
            canonicalizeResolvedType(context, fields[i]->typeText);
        if (fieldType != resolved[i + 1].canonicalType) {
          satisfied = false;
          break;
        }
      }
    }
  } else {
    expectedShape = targetType + "{...}";
  }

  fact.evaluationOutcome = satisfied ? "satisfied" : "unsatisfied";
  fact.evaluationDiagnostic =
      satisfied ? "construct predicate satisfied: " + expectedShape
                : "construct predicate failed: " + expectedShape +
                      " does not match requested constructor with " +
                      formatResolvedTypeFacts(resolved);
}

void evaluateLifecyclePredicate(RequirementPredicateFactDraft &fact,
                                const RequirementPredicateDefinitionContext &context,
                                std::string_view helperName) {
  if (fact.operands.size() != 1) {
    fact.evaluationOutcome = "invalid_evaluation";
    fact.evaluationDiagnostic = "requirement predicate " + fact.predicateName +
                                " expects exactly one type operand";
    return;
  }
  TypeOperandResolution target =
      resolveTypeOperand(context, fact.operands.front(), fact.predicateName);
  if (target.status == TypeOperandStatus::Invalid) {
    fact.evaluationOutcome = "invalid_evaluation";
    fact.evaluationDiagnostic = std::move(target.diagnostic);
    return;
  }
  if (target.status == TypeOperandStatus::Deferred) {
    fact.evaluationOutcome = "invalid_evaluation";
    fact.evaluationDiagnostic =
        "requirement predicate evaluation deferred for unresolved type facts";
    return;
  }

  bool satisfied = false;
  const bool builtinValue = isPrimitiveOrBuiltinTypeName(target.canonicalType) &&
                            context.structNames.count(target.canonicalType) == 0;
  if (builtinValue) {
    satisfied = true;
  } else {
    const std::string helperPath =
        typePathForCanonical(target.canonicalType) + "/" + std::string(helperName);
    const auto matches = visibleCallableMatches(context, helperPath);
    satisfied = !matches.empty();
  }

  fact.evaluationOutcome = satisfied ? "satisfied" : "unsatisfied";
  fact.evaluationDiagnostic =
      satisfied ? "lifecycle predicate satisfied: " + std::string(helperName) +
                      " for " + target.canonicalType
                : "lifecycle predicate failed: no visible " +
                      std::string(helperName) + " helper for " +
                      target.canonicalType + " with type facts: " +
                      target.canonicalType;
}

void evaluateNamedMemberPredicate(RequirementPredicateFactDraft &fact,
                                  const RequirementPredicateDefinitionContext &context,
                                  bool fieldOnly) {
  if (fact.operands.size() != 2) {
    fact.evaluationOutcome = "invalid_evaluation";
    fact.evaluationDiagnostic = "requirement predicate " + fact.predicateName +
                                " expects one type operand and one name argument";
    return;
  }
  TypeOperandResolution target =
      resolveTypeOperand(context, fact.operands.front(), fact.predicateName);
  if (target.status == TypeOperandStatus::Invalid) {
    fact.evaluationOutcome = "invalid_evaluation";
    fact.evaluationDiagnostic = std::move(target.diagnostic);
    return;
  }
  if (target.status == TypeOperandStatus::Deferred) {
    fact.evaluationOutcome = "invalid_evaluation";
    fact.evaluationDiagnostic =
        "requirement predicate evaluation deferred for unresolved type facts";
    return;
  }
  std::string memberName;
  std::string diagnostic;
  if (!parseCompileTimeNameOperand(fact.operands.back(),
                                   fact.predicateName,
                                   fieldOnly ? "field" : "member",
                                   memberName,
                                   diagnostic)) {
    fact.evaluationOutcome = "invalid_evaluation";
    fact.evaluationDiagnostic = std::move(diagnostic);
    return;
  }

  bool hasVisibleField = false;
  for (const auto *field : visibleStructFields(context, target.canonicalType)) {
    if (field->fieldName == memberName) {
      hasVisibleField = true;
      break;
    }
  }

  bool hasVisibleHelper = false;
  if (!fieldOnly) {
    const std::string helperPath =
        typePathForCanonical(target.canonicalType) + "/" + memberName;
    hasVisibleHelper = !visibleCallableMatches(context, helperPath).empty();
  }
  const bool satisfied = fieldOnly ? hasVisibleField
                                   : (hasVisibleField || hasVisibleHelper);
  fact.evaluationOutcome = satisfied ? "satisfied" : "unsatisfied";
  fact.evaluationDiagnostic =
      satisfied ? "member predicate satisfied: " + target.canonicalType +
                      "." + memberName
                : "member predicate failed: no visible " +
                      std::string(fieldOnly ? "field" : "field or helper") +
                      " named " + memberName + " on " + target.canonicalType +
                      " with type facts: " + target.canonicalType;
}

void evaluateBuiltinCapabilityPredicate(
    RequirementPredicateFactDraft &fact,
    const RequirementPredicateDefinitionContext &context) {
  if (fact.predicateName == "/std/meta/has_trait") {
    evaluateHasTraitPredicate(fact, context);
    return;
  }
  if (fact.predicateName == "/std/meta/supports_call") {
    evaluateSupportsCallPredicate(fact, context);
    return;
  }
  if (fact.predicateName == "/std/meta/can_construct") {
    evaluateCanConstructPredicate(fact, context);
    return;
  }
  if (fact.predicateName == "/std/meta/can_copy") {
    evaluateLifecyclePredicate(fact, context, "Copy");
    return;
  }
  if (fact.predicateName == "/std/meta/can_move") {
    evaluateLifecyclePredicate(fact, context, "Move");
    return;
  }
  if (fact.predicateName == "/std/meta/has_field") {
    evaluateNamedMemberPredicate(fact, context, true);
    return;
  }
  if (fact.predicateName == "/std/meta/has_member") {
    evaluateNamedMemberPredicate(fact, context, false);
    return;
  }
}

void evaluateRequirementPredicate(RequirementPredicateFactDraft &fact,
                                  const RequirementPredicateDefinitionContext &context) {
  if (!isBuiltinRequirementPredicate(fact.predicateName)) {
    evaluateUserDefinedRequirementPredicate(fact, context);
    return;
  }
  if (isBuiltinTypeRelationPredicate(fact.predicateName)) {
    evaluateBuiltinTypePredicate(fact, context);
    return;
  }
  if (isBuiltinValuePredicate(fact.predicateName)) {
    evaluateBuiltinValuePredicate(fact, context);
    return;
  }
  evaluateBuiltinCapabilityPredicate(fact, context);
}

} // namespace requirementFacts
} // namespace primec::semantics
