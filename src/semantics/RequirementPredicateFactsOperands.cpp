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


TypeOperandResolution resolveTypeOperand(
    const RequirementPredicateDefinitionContext &context,
    const RequirementPredicateOperandFact &operand,
    std::string_view predicateName) {
  if (operand.kind != "type_fact") {
    return {TypeOperandStatus::Invalid,
            {},
            "unsupported operand for requirement predicate " +
                std::string(predicateName) + ": " + operand.text};
  }

  std::string text = trimRequirementText(operand.text);
  if (text.rfind("typeof<", 0) == 0 && text.size() > 8 && text.back() == '>') {
    std::string symbol = trimRequirementText(
        std::string_view(text).substr(7, text.size() - 8));
    if (!isRequirementSymbolText(symbol) || symbol.find('/') != std::string::npos ||
        symbol.find('.') != std::string::npos || symbol.find(':') != std::string::npos) {
      return {TypeOperandStatus::Invalid,
              {},
              "typeof operand in requirement predicate " +
                  std::string(predicateName) +
                  " requires a local symbol: " + text};
    }
    if (const BindingInfo *binding = findParameterBinding(context, symbol)) {
      return {TypeOperandStatus::Resolved,
              canonicalizeResolvedType(context, bindingTypeTextForRequirement(*binding)),
              {}};
    }
    if (isTemplateTypeParameter(context, symbol)) {
      return {TypeOperandStatus::Deferred,
              {},
              "requirement predicate " + std::string(predicateName) +
                  " deferred for unresolved type fact: " + text};
    }
    return {TypeOperandStatus::Invalid,
            {},
            "unknown type fact for requirement predicate " +
                std::string(predicateName) + ": " + text};
  }

  if (isTemplateTypeParameter(context, text)) {
    return {TypeOperandStatus::Deferred,
            {},
            "requirement predicate " + std::string(predicateName) +
                " deferred for unresolved type fact: " + text};
  }

  const std::string canonical = canonicalizeResolvedType(context, text);
  if (isPrimitiveOrBuiltinTypeName(canonical) ||
      resolveNominalPath(context, canonical, context.structNames) == canonical ||
      resolveNominalPath(context, canonical, context.sumNames) == canonical ||
      context.structNames.count(canonical) > 0 ||
      context.sumNames.count(canonical) > 0) {
    return {TypeOperandStatus::Resolved, canonical, {}};
  }

  return {TypeOperandStatus::Invalid,
          {},
          "unknown type fact for requirement predicate " +
              std::string(predicateName) + ": " + text};
}

bool parseUnsignedRequirementInteger(std::string_view text,
                                     std::uint64_t &valueOut) {
  while (!text.empty() &&
         std::isspace(static_cast<unsigned char>(text.front()))) {
    text.remove_prefix(1);
  }
  while (!text.empty() &&
         std::isspace(static_cast<unsigned char>(text.back()))) {
    text.remove_suffix(1);
  }
  if (text.empty()) {
    return false;
  }
  auto isSupportedIntegerSuffix = [](std::string_view suffix) {
    return suffix.empty() || suffix == "int" || suffix == "uint" ||
           suffix == "i8" || suffix == "i16" || suffix == "i32" ||
           suffix == "i64" || suffix == "u8" || suffix == "u16" ||
           suffix == "u32" || suffix == "u64";
  };
  std::uint64_t value = 0;
  if (text.size() > 2 && text[0] == '0' &&
      (text[1] == 'x' || text[1] == 'X')) {
    bool sawDigit = false;
    std::size_t i = 2;
    for (; i < text.size(); ++i) {
      const char c = text[i];
      if (c == ',' || c == '_') {
        continue;
      }
      std::uint64_t digit = 0;
      if (c >= '0' && c <= '9') {
        digit = static_cast<std::uint64_t>(c - '0');
      } else if (c >= 'a' && c <= 'f') {
        digit = static_cast<std::uint64_t>(10 + c - 'a');
      } else if (c >= 'A' && c <= 'F') {
        digit = static_cast<std::uint64_t>(10 + c - 'A');
      } else {
        break;
      }
      if (value > (UINT64_MAX - digit) / 16) {
        return false;
      }
      value = value * 16 + digit;
      sawDigit = true;
    }
    if (!sawDigit || !isSupportedIntegerSuffix(text.substr(i))) {
      return false;
    }
    valueOut = value;
    return true;
  }
  bool sawDigit = false;
  std::size_t i = 0;
  for (; i < text.size(); ++i) {
    const char c = text[i];
    if (c == ',' || c == '_') {
      continue;
    }
    if (c < '0' || c > '9') {
      break;
    }
    const std::uint64_t digit = static_cast<std::uint64_t>(c - '0');
    if (value > (UINT64_MAX - digit) / 10) {
      return false;
    }
    value = value * 10 + digit;
    sawDigit = true;
  }
  if (!sawDigit || !isSupportedIntegerSuffix(text.substr(i))) {
    return false;
  }
  valueOut = value;
  return true;
}

ValueOperandResolution resolveValueOperand(
    const RequirementPredicateDefinitionContext &context,
    const RequirementPredicateOperandFact &operand,
    std::string_view predicateName) {
  if (operand.kind == "literal_compile_time_argument") {
    std::uint64_t value = 0;
    if (!parseUnsignedRequirementInteger(operand.text, value)) {
      return {ValueOperandStatus::Invalid,
              0,
              "unsupported integer value operand for requirement predicate " +
                  std::string(predicateName) + ": " + operand.text};
    }
    return {ValueOperandStatus::Resolved, value, {}};
  }
  if (operand.kind == "compile_time_symbol") {
    if (std::find(context.templateArgs.begin(),
                  context.templateArgs.end(),
                  operand.text) != context.templateArgs.end()) {
      return {ValueOperandStatus::Deferred,
              0,
              "requirement predicate " + std::string(predicateName) +
                  " deferred for unresolved value fact: " + operand.text};
    }
    return {ValueOperandStatus::Invalid,
            0,
            "non-constant value operand for requirement predicate " +
                std::string(predicateName) + ": " + operand.text};
  }
  return {ValueOperandStatus::Invalid,
          0,
          "unsupported value operand for requirement predicate " +
              std::string(predicateName) + ": " + operand.text};
}

bool isRuntimeCountableContractParameterType(const BindingInfo &binding) {
  const std::string normalized = normalizeBindingTypeName(binding.typeName);
  return normalized == "vector" || normalized == "array" ||
         normalized == "string";
}

bool isRuntimeIntegerContractParameterType(const BindingInfo &binding) {
  if (!binding.typeTemplateArg.empty()) {
    return false;
  }
  const std::string normalized = normalizeBindingTypeName(binding.typeName);
  return normalized == "int" || normalized == "uint" || normalized == "i8" ||
         normalized == "i16" || normalized == "i32" || normalized == "i64" ||
         normalized == "u8" || normalized == "u16" || normalized == "u32" ||
         normalized == "u64";
}

// Classifies an operand that is not a compile-time fact as a runtime-checkable
// contract operand. Returns the runtime operand kind, or an empty string when
// the operand cannot be checked by a deterministic runtime precondition.
std::string runtimeContractOperandKind(
    const RequirementPredicateDefinitionContext &context,
    const RequirementPredicateOperandFact &operand) {
  const std::string text = trimRequirementText(operand.text);
  if (text.rfind("count(", 0) == 0 && text.size() > 7 && text.back() == ')') {
    const std::string inner = trimRequirementText(
        std::string_view(text).substr(6, text.size() - 7));
    if (!isRequirementSymbolText(inner) ||
        inner.find('/') != std::string::npos ||
        inner.find('.') != std::string::npos ||
        inner.find(':') != std::string::npos) {
      return {};
    }
    if (const BindingInfo *binding = findParameterBinding(context, inner);
        binding != nullptr &&
        isRuntimeCountableContractParameterType(*binding)) {
      return "runtime_count_of_parameter";
    }
    return {};
  }
  if (isRequirementSymbolText(text) && !isTemplateTypeParameter(context, text)) {
    if (const BindingInfo *binding = findParameterBinding(context, text);
        binding != nullptr && isRuntimeIntegerContractParameterType(*binding)) {
      return "runtime_parameter_value";
    }
  }
  return {};
}

std::string formatResolvedTypeFacts(const std::vector<TypeOperandResolution> &resolved) {
  std::ostringstream out;
  out << "type facts: ";
  for (std::size_t i = 0; i < resolved.size(); ++i) {
    if (i != 0) {
      out << ", ";
    }
    out << resolved[i].canonicalType;
  }
  return out.str();
}

std::string callableDisplaySignature(
    std::string_view name,
    const std::vector<TypeOperandResolution> &resolved,
    std::size_t paramCount,
    std::string_view returnType) {
  std::ostringstream out;
  out << name << "(";
  for (std::size_t i = 0; i < paramCount; ++i) {
    if (i != 0) {
      out << ", ";
    }
    out << resolved[i].canonicalType;
  }
  out << ") -> " << returnType;
  return out.str();
}

bool parseCompileTimeNameOperand(const RequirementPredicateOperandFact &operand,
                                 std::string_view predicateName,
                                 std::string_view label,
                                 std::string &nameOut,
                                 std::string &diagnosticOut) {
  if (operand.kind == "compile_time_symbol") {
    nameOut = operand.text;
    return true;
  }
  if (operand.kind == "literal_compile_time_argument") {
    ParsedStringLiteral parsed;
    std::string parseError;
    if (!parseStringLiteralToken(operand.text, parsed, parseError)) {
      diagnosticOut = "requirement predicate " + std::string(predicateName) +
                      " requires utf8/ascii string literal " +
                      std::string(label) + " argument";
      return false;
    }
    nameOut = parsed.decoded;
    if (nameOut.empty()) {
      diagnosticOut = "requirement predicate " + std::string(predicateName) +
                      " requires non-empty " + std::string(label) + " name";
      return false;
    }
    return true;
  }
  diagnosticOut = "requirement predicate " + std::string(predicateName) +
                  " requires constant string or identifier " +
                  std::string(label) + " argument";
  return false;
}

std::string typePathForCanonical(std::string_view canonicalType) {
  if (!canonicalType.empty() && canonicalType.front() == '/') {
    return std::string(canonicalType);
  }
  return "/" + std::string(canonicalType);
}

bool isVisibleFromRequirementDefinition(
    const RequirementPredicateDefinitionContext &context,
    std::string_view ownerPath,
    bool isPrivate) {
  if (!isPrivate) {
    return true;
  }
  if (context.definitionPath == ownerPath) {
    return true;
  }
  const std::string ownerPrefix = std::string(ownerPath) + "/";
  return context.definitionPath.rfind(ownerPrefix, 0) == 0;
}

bool callableFactIsVisibleFromRequirementDefinition(
    const RequirementPredicateDefinitionContext &context,
    const RequirementPredicateDefinitionContext::CallableFact &callable) {
  const std::size_t slash = callable.fullPath.find_last_of('/');
  const std::string owner =
      slash == std::string::npos ? std::string{} : callable.fullPath.substr(0, slash);
  return isVisibleFromRequirementDefinition(context, owner, callable.isPrivate);
}

bool fieldFactIsVisibleFromRequirementDefinition(
    const RequirementPredicateDefinitionContext &context,
    const RequirementPredicateDefinitionContext::StructFieldFact &field) {
  return isVisibleFromRequirementDefinition(context, field.structPath, field.isPrivate);
}

bool hasAnnotatedStructTrait(const RequirementPredicateDefinitionContext &context,
                             std::string_view structPath,
                             std::string_view traitName) {
  if (structPath.empty()) {
    return false;
  }
  for (const auto &trait : context.structTraits) {
    if (trait.structPath == structPath && trait.traitName == traitName &&
        isVisibleFromRequirementDefinition(context, trait.structPath, trait.isPrivate)) {
      return true;
    }
  }
  return false;
}

std::vector<RequirementPredicateDefinitionContext::CallableFact>
visibleCallableMatches(const RequirementPredicateDefinitionContext &context,
                       std::string_view requestedPath) {
  std::vector<RequirementPredicateDefinitionContext::CallableFact> matches;
  const std::string path(requestedPath);
  for (const auto &callable : context.callables) {
    const bool samePath = callable.fullPath == path;
    const bool overloadPath = callable.fullPath.rfind(path + "__ov", 0) == 0 ||
                              callable.fullPath.rfind(path + "__t", 0) == 0;
    if (!samePath && !overloadPath) {
      continue;
    }
    if (!callableFactIsVisibleFromRequirementDefinition(context, callable)) {
      continue;
    }
    matches.push_back(callable);
  }
  std::stable_sort(matches.begin(), matches.end(), [](const auto &left, const auto &right) {
    return left.fullPath < right.fullPath;
  });
  return matches;
}

std::vector<std::string> userPredicatePathCandidates(
    const RequirementPredicateDefinitionContext &context,
    std::string_view predicateName) {
  std::vector<std::string> candidates;
  const std::string name = trimRequirementText(predicateName);
  if (name.empty()) {
    return candidates;
  }
  auto pushUnique = [&](std::string path) {
    if (path.empty()) {
      return;
    }
    if (std::find(candidates.begin(), candidates.end(), path) == candidates.end()) {
      candidates.push_back(std::move(path));
    }
  };

  if (name.front() == '/') {
    pushUnique(name);
    return candidates;
  }
  if (const auto importIt = context.importAliases.find(name);
      importIt != context.importAliases.end()) {
    pushUnique(importIt->second);
  }
  if (name.find('/') != std::string::npos) {
    pushUnique("/" + name);
  }
  if (!context.namespacePrefix.empty()) {
    pushUnique(context.namespacePrefix + "/" + name);
  }
  pushUnique("/" + name);
  return candidates;
}

std::vector<RequirementPredicateDefinitionContext::CallableFact>
visibleUserPredicateMatches(const RequirementPredicateDefinitionContext &context,
                            std::string_view predicateName) {
  std::vector<RequirementPredicateDefinitionContext::CallableFact> matches;
  for (const std::string &candidatePath :
       userPredicatePathCandidates(context, predicateName)) {
    std::vector<RequirementPredicateDefinitionContext::CallableFact> pathMatches =
        visibleCallableMatches(context, candidatePath);
    matches.insert(matches.end(), pathMatches.begin(), pathMatches.end());
  }
  std::stable_sort(matches.begin(),
                   matches.end(),
                   [](const auto &left, const auto &right) {
                     return left.fullPath < right.fullPath;
                   });
  matches.erase(std::unique(matches.begin(),
                            matches.end(),
                            [](const auto &left, const auto &right) {
                              return left.fullPath == right.fullPath;
                            }),
                matches.end());
  return matches;
}

std::string compileTimeOperandDiagnostic(
    const RequirementPredicateOperandFact &operand,
    std::string_view predicateName) {
  if (operand.kind == "unsupported_runtime_only_expression") {
    return "runtime-only operation in compile-time predicate " +
           std::string(predicateName) + ": " + operand.text;
  }
  if (operand.kind != "type_fact" &&
      operand.kind != "compile_time_symbol" &&
      operand.kind != "literal_compile_time_argument") {
    return "unsupported compile-time argument for predicate " +
           std::string(predicateName) + ": " + operand.text;
  }
  return {};
}

void evaluateUserDefinedRequirementPredicate(
    RequirementPredicateFactDraft &fact,
    const RequirementPredicateDefinitionContext &context) {
  const std::vector<RequirementPredicateDefinitionContext::CallableFact> matches =
      visibleUserPredicateMatches(context, fact.predicateName);
  if (matches.empty()) {
    fact.evaluationOutcome = "invalid_evaluation";
    fact.evaluationDiagnostic =
        "unknown requirement predicate: " + fact.predicateName;
    return;
  }
  if (matches.size() > 1) {
    fact.evaluationOutcome = "invalid_evaluation";
    fact.evaluationDiagnostic =
        "ambiguous user requirement predicate: " + fact.predicateName;
    return;
  }

  const RequirementPredicateDefinitionContext::CallableFact &callable =
      matches.front();
  fact.predicateName = callable.fullPath;
  const std::string canonicalReturn =
      canonicalizeResolvedType(context, callable.returnType);
  if (canonicalReturn != "bool") {
    fact.evaluationOutcome = "invalid_evaluation";
    fact.evaluationDiagnostic =
        "user requirement predicate must return bool: " + callable.fullPath;
    return;
  }
  if (!callable.effectNames.empty()) {
    for (const std::string &effectName : callable.effectNames) {
      if (std::find(context.compileTimeEffects.begin(),
                    context.compileTimeEffects.end(),
                    effectName) != context.compileTimeEffects.end()) {
        continue;
      }
      fact.evaluationOutcome = "denied_effect";
      fact.evaluationDiagnostic =
          "denied compile-time effect in user requirement predicate " +
          callable.fullPath + ": " + effectName +
          " (missing effects<compiletime>(" + effectName + "))";
      return;
    }
  }
  if (!callable.parameterTypes.empty()) {
    fact.evaluationOutcome = "invalid_evaluation";
    fact.evaluationDiagnostic =
        "runtime parameters are not supported in compile-time user predicate: " +
        callable.fullPath;
    return;
  }
  if (fact.operands.size() != callable.templateArgs.size()) {
    fact.evaluationOutcome = "invalid_evaluation";
    fact.evaluationDiagnostic =
        "user requirement predicate " + callable.fullPath + " expects " +
        std::to_string(callable.templateArgs.size()) +
        " compile-time arguments, got " +
        std::to_string(fact.operands.size());
    return;
  }
  for (const RequirementPredicateOperandFact &operand : fact.operands) {
    if (const std::string diagnostic =
            compileTimeOperandDiagnostic(operand, callable.fullPath);
        !diagnostic.empty()) {
      fact.evaluationOutcome = "invalid_evaluation";
      fact.evaluationDiagnostic = diagnostic;
      return;
    }
  }
  if (!callable.hasReturnExpr || !callable.returnExprIsBoolLiteral) {
    fact.evaluationOutcome = "invalid_evaluation";
    fact.evaluationDiagnostic =
        "unsupported pure user requirement predicate body: " +
        callable.fullPath;
    return;
  }

  fact.evaluationOutcome = callable.returnBoolValue ? "satisfied" : "unsatisfied";
  fact.evaluationDiagnostic = callable.returnBoolValue
                                  ? "user predicate returned true"
                                  : "user predicate returned false";
}

bool callableSignatureMatches(
    const RequirementPredicateDefinitionContext &context,
    std::string_view requestedPath,
    const std::vector<TypeOperandResolution> &resolved,
    std::size_t paramCount,
    std::string_view expectedReturnType) {
  const std::vector<RequirementPredicateDefinitionContext::CallableFact> matches =
      visibleCallableMatches(context, requestedPath);
  for (const auto &callable : matches) {
    if (callable.parameterTypes.size() != paramCount) {
      continue;
    }
    bool paramsMatch = true;
    for (std::size_t i = 0; i < paramCount; ++i) {
      const std::string canonicalParam =
          canonicalizeResolvedType(context, callable.parameterTypes[i]);
      if (canonicalParam != resolved[i].canonicalType) {
        paramsMatch = false;
        break;
      }
    }
    if (!paramsMatch) {
      continue;
    }
    const std::string canonicalReturn =
        canonicalizeResolvedType(context, callable.returnType);
    if (canonicalReturn == expectedReturnType) {
      return true;
    }
  }
  return false;
}

std::vector<const RequirementPredicateDefinitionContext::StructFieldFact *>
visibleStructFields(const RequirementPredicateDefinitionContext &context,
                    std::string_view structPath) {
  std::vector<const RequirementPredicateDefinitionContext::StructFieldFact *> fields;
  for (const auto &field : context.structFields) {
    if (field.structPath != structPath) {
      continue;
    }
    if (!fieldFactIsVisibleFromRequirementDefinition(context, field)) {
      continue;
    }
    fields.push_back(&field);
  }
  std::stable_sort(fields.begin(), fields.end(), [](const auto *left, const auto *right) {
    if (left->structPath != right->structPath) {
      return left->structPath < right->structPath;
    }
    return left->fieldName < right->fieldName;
  });
  return fields;
}

bool isNumericTraitType(std::string_view canonicalType) {
  if (canonicalType.empty() || canonicalType.find('<') != std::string_view::npos) {
    return false;
  }
  const std::string normalized = normalizeBindingTypeName(std::string(canonicalType));
  return normalized == "i32" || normalized == "i64" ||
         normalized == "u64" || normalized == "f32" ||
         normalized == "f64" || normalized == "integer" ||
         normalized == "decimal" || normalized == "complex";
}

bool isComparableBuiltinTraitType(std::string_view canonicalType) {
  if (canonicalType.empty() || canonicalType.find('<') != std::string_view::npos) {
    return false;
  }
  const std::string normalized = normalizeBindingTypeName(std::string(canonicalType));
  if (normalized == "complex") {
    return false;
  }
  if (isNumericTraitType(canonicalType)) {
    return true;
  }
  return normalized == "bool" || normalized == "string";
}

bool isIndexableBuiltinTraitType(std::string_view canonicalType,
                                 std::string_view elemCanonicalType) {
  const std::string normalized = normalizeBindingTypeName(std::string(canonicalType));
  if (normalized == "string") {
    return elemCanonicalType == "i32";
  }
  std::string base;
  std::string arg;
  if (!splitTemplateTypeName(std::string(canonicalType), base, arg)) {
    return false;
  }
  const std::string normalizedBase = normalizeBindingTypeName(base);
  if (normalizedBase != "array" && normalizedBase != "vector") {
    return false;
  }
  std::vector<std::string> args;
  if (!splitTopLevelTemplateArgs(arg, args) || args.size() != 1) {
    return false;
  }
  return canonicalizeResolvedType(RequirementPredicateDefinitionContext{}, args.front()) ==
         elemCanonicalType;
}

} // namespace requirementFacts
} // namespace primec::semantics
