#pragma once

// Internal helpers of RequirementPredicateFacts*.cpp (TODO-5384): types and declarations; the
// definitions live in the RequirementPredicateFacts*.cpp units.
#include "RequirementPredicateFacts.h"
#include "primec/frontend/StringLiteral.h"
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <optional>
#include <sstream>

namespace primec::semantics {
namespace requirementFacts {


enum class TypeOperandStatus { Resolved, Deferred, Invalid };
enum class ValueOperandStatus { Resolved, Deferred, Invalid };

struct TypeOperandResolution {
  TypeOperandStatus status = TypeOperandStatus::Invalid;
  std::string canonicalType;
  std::string diagnostic;
};

struct ValueOperandResolution {
  ValueOperandStatus status = ValueOperandStatus::Invalid;
  std::uint64_t unsignedValue = 0;
  std::string diagnostic;
};

std::string trimRequirementText(std::string_view text);

bool isRequirementIdentifierChar(char ch);

bool isRequirementSymbolText(std::string_view text);

bool isRequirementLiteralText(std::string_view text);

std::vector<std::string> splitRequirementTopLevelList(std::string_view text);

std::optional<std::size_t> findTopLevelRequirementCallParen(std::string_view text);

std::optional<std::size_t> findTopLevelTemplateStart(std::string_view text);

std::optional<std::pair<std::size_t, std::string>>
findTopLevelRequirementRelation(std::string_view text);

std::string requirementOperandStableHandle(std::string_view kind, std::string_view text);

std::string normalizeRequirementTypeofText(std::string text);

std::string bindingTypeTextForRequirement(const BindingInfo &binding);

std::string canonicalPredicateName(std::string name);

bool isBuiltinTypeRelationPredicate(std::string_view name);

bool isBuiltinCapabilityPredicate(std::string_view name);

bool isBuiltinValuePredicate(std::string_view name);

bool isBuiltinRequirementPredicate(std::string_view name);

std::string canonicalCallSourceText(std::string_view predicateName,
                                    const std::vector<RequirementPredicateOperandFact> &operands);

RequirementPredicateOperandFact classifyRequirementOperand(
    std::string text,
    std::string_view predicateName,
    std::size_t operandIndex,
    bool isTemplateOperand,
    int sourceLine,
    int sourceColumn);

const BindingInfo *findParameterBinding(
    const RequirementPredicateDefinitionContext &context,
    std::string_view name);

bool isTemplateTypeParameter(const RequirementPredicateDefinitionContext &context,
                             std::string_view name);

bool isPrimitiveOrBuiltinTypeName(std::string_view typeText);

std::string resolveNominalPath(const RequirementPredicateDefinitionContext &context,
                               const std::string &typeText,
                               const std::unordered_set<std::string> &names);

std::string canonicalizeResolvedType(
    const RequirementPredicateDefinitionContext &context,
    const std::string &typeText);

TypeOperandResolution resolveTypeOperand(
    const RequirementPredicateDefinitionContext &context,
    const RequirementPredicateOperandFact &operand,
    std::string_view predicateName);

bool parseUnsignedRequirementInteger(std::string_view text,
                                     std::uint64_t &valueOut);

ValueOperandResolution resolveValueOperand(
    const RequirementPredicateDefinitionContext &context,
    const RequirementPredicateOperandFact &operand,
    std::string_view predicateName);

bool isRuntimeCountableContractParameterType(const BindingInfo &binding);

bool isRuntimeIntegerContractParameterType(const BindingInfo &binding);

// Classifies an operand that is not a compile-time fact as a runtime-checkable
// contract operand. Returns the runtime operand kind, or an empty string when
// the operand cannot be checked by a deterministic runtime precondition.
std::string runtimeContractOperandKind(
    const RequirementPredicateDefinitionContext &context,
    const RequirementPredicateOperandFact &operand);

std::string formatResolvedTypeFacts(const std::vector<TypeOperandResolution> &resolved);

std::string callableDisplaySignature(
    std::string_view name,
    const std::vector<TypeOperandResolution> &resolved,
    std::size_t paramCount,
    std::string_view returnType);

bool parseCompileTimeNameOperand(const RequirementPredicateOperandFact &operand,
                                 std::string_view predicateName,
                                 std::string_view label,
                                 std::string &nameOut,
                                 std::string &diagnosticOut);

std::string typePathForCanonical(std::string_view canonicalType);

bool isVisibleFromRequirementDefinition(
    const RequirementPredicateDefinitionContext &context,
    std::string_view ownerPath,
    bool isPrivate);

bool callableFactIsVisibleFromRequirementDefinition(
    const RequirementPredicateDefinitionContext &context,
    const RequirementPredicateDefinitionContext::CallableFact &callable);

bool fieldFactIsVisibleFromRequirementDefinition(
    const RequirementPredicateDefinitionContext &context,
    const RequirementPredicateDefinitionContext::StructFieldFact &field);

bool hasAnnotatedStructTrait(const RequirementPredicateDefinitionContext &context,
                             std::string_view structPath,
                             std::string_view traitName);

std::vector<RequirementPredicateDefinitionContext::CallableFact>
visibleCallableMatches(const RequirementPredicateDefinitionContext &context,
                       std::string_view requestedPath);

std::vector<std::string> userPredicatePathCandidates(
    const RequirementPredicateDefinitionContext &context,
    std::string_view predicateName);

std::vector<RequirementPredicateDefinitionContext::CallableFact>
visibleUserPredicateMatches(const RequirementPredicateDefinitionContext &context,
                            std::string_view predicateName);

std::string compileTimeOperandDiagnostic(
    const RequirementPredicateOperandFact &operand,
    std::string_view predicateName);

void evaluateUserDefinedRequirementPredicate(
    RequirementPredicateFactDraft &fact,
    const RequirementPredicateDefinitionContext &context);

bool callableSignatureMatches(
    const RequirementPredicateDefinitionContext &context,
    std::string_view requestedPath,
    const std::vector<TypeOperandResolution> &resolved,
    std::size_t paramCount,
    std::string_view expectedReturnType);

std::vector<const RequirementPredicateDefinitionContext::StructFieldFact *>
visibleStructFields(const RequirementPredicateDefinitionContext &context,
                    std::string_view structPath);

bool isNumericTraitType(std::string_view canonicalType);

bool isComparableBuiltinTraitType(std::string_view canonicalType);

bool isIndexableBuiltinTraitType(std::string_view canonicalType,
                                 std::string_view elemCanonicalType);

void evaluateBuiltinTypePredicate(RequirementPredicateFactDraft &fact,
                                  const RequirementPredicateDefinitionContext &context);

void evaluateBuiltinValuePredicate(RequirementPredicateFactDraft &fact,
                                   const RequirementPredicateDefinitionContext &context);

void evaluateHasTraitPredicate(RequirementPredicateFactDraft &fact,
                               const RequirementPredicateDefinitionContext &context);

void evaluateSupportsCallPredicate(RequirementPredicateFactDraft &fact,
                                   const RequirementPredicateDefinitionContext &context);

void evaluateCanConstructPredicate(RequirementPredicateFactDraft &fact,
                                   const RequirementPredicateDefinitionContext &context);

void evaluateLifecyclePredicate(RequirementPredicateFactDraft &fact,
                                const RequirementPredicateDefinitionContext &context,
                                std::string_view helperName);

void evaluateNamedMemberPredicate(RequirementPredicateFactDraft &fact,
                                  const RequirementPredicateDefinitionContext &context,
                                  bool fieldOnly);

void evaluateBuiltinCapabilityPredicate(
    RequirementPredicateFactDraft &fact,
    const RequirementPredicateDefinitionContext &context);

void evaluateRequirementPredicate(RequirementPredicateFactDraft &fact,
                                  const RequirementPredicateDefinitionContext &context);

} // namespace requirementFacts
} // namespace primec::semantics
