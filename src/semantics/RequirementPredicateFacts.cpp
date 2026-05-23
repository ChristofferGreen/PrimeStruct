#include "RequirementPredicateFacts.h"

#include <algorithm>
#include <cctype>
#include <optional>
#include <sstream>

namespace primec::semantics {
namespace {

enum class TypeOperandStatus { Resolved, Deferred, Invalid };

struct TypeOperandResolution {
  TypeOperandStatus status = TypeOperandStatus::Invalid;
  std::string canonicalType;
  std::string diagnostic;
};

std::string trimRequirementText(std::string_view text) {
  while (!text.empty() &&
         std::isspace(static_cast<unsigned char>(text.front()))) {
    text.remove_prefix(1);
  }
  while (!text.empty() &&
         std::isspace(static_cast<unsigned char>(text.back()))) {
    text.remove_suffix(1);
  }
  return std::string(text);
}

bool isRequirementIdentifierChar(char ch) {
  return std::isalnum(static_cast<unsigned char>(ch)) || ch == '_' ||
         ch == '/' || ch == '.' || ch == ':';
}

bool isRequirementSymbolText(std::string_view text) {
  if (text.empty()) {
    return false;
  }
  if (!std::isalpha(static_cast<unsigned char>(text.front())) &&
      text.front() != '_' && text.front() != '/') {
    return false;
  }
  for (char ch : text) {
    if (!isRequirementIdentifierChar(ch)) {
      return false;
    }
  }
  return true;
}

bool isRequirementLiteralText(std::string_view text) {
  if (text == "true" || text == "false") {
    return true;
  }
  if (text.size() >= 2 &&
      ((text.front() == '"' && text.back() == '"') ||
       (text.front() == '\'' && text.back() == '\''))) {
    return true;
  }
  bool sawDigit = false;
  for (char ch : text) {
    if (std::isdigit(static_cast<unsigned char>(ch))) {
      sawDigit = true;
      continue;
    }
    if (ch == '.' || ch == '-' || ch == '+' || ch == '_' ||
        std::isalpha(static_cast<unsigned char>(ch))) {
      continue;
    }
    return false;
  }
  return sawDigit;
}

std::vector<std::string> splitRequirementTopLevelList(std::string_view text) {
  std::vector<std::string> out;
  int angleDepth = 0;
  int parenDepth = 0;
  int braceDepth = 0;
  int bracketDepth = 0;
  std::size_t start = 0;
  auto pushSegment = [&](std::size_t end) {
    std::string segment = trimRequirementText(text.substr(start, end - start));
    if (!segment.empty()) {
      out.push_back(std::move(segment));
    }
  };
  for (std::size_t i = 0; i < text.size(); ++i) {
    const char ch = text[i];
    if (ch == '<') {
      ++angleDepth;
      continue;
    }
    if (ch == '>') {
      angleDepth = std::max(0, angleDepth - 1);
      continue;
    }
    if (ch == '(') {
      ++parenDepth;
      continue;
    }
    if (ch == ')') {
      parenDepth = std::max(0, parenDepth - 1);
      continue;
    }
    if (ch == '{') {
      ++braceDepth;
      continue;
    }
    if (ch == '}') {
      braceDepth = std::max(0, braceDepth - 1);
      continue;
    }
    if (ch == '[') {
      ++bracketDepth;
      continue;
    }
    if (ch == ']') {
      bracketDepth = std::max(0, bracketDepth - 1);
      continue;
    }
    if (ch == ',' && angleDepth == 0 && parenDepth == 0 &&
        braceDepth == 0 && bracketDepth == 0) {
      pushSegment(i);
      start = i + 1;
    }
  }
  pushSegment(text.size());
  return out;
}

std::optional<std::size_t> findTopLevelRequirementCallParen(std::string_view text) {
  int angleDepth = 0;
  for (std::size_t i = 0; i < text.size(); ++i) {
    const char ch = text[i];
    if (ch == '<') {
      ++angleDepth;
      continue;
    }
    if (ch == '>') {
      angleDepth = std::max(0, angleDepth - 1);
      continue;
    }
    if (ch == '(' && angleDepth == 0) {
      return i;
    }
  }
  return std::nullopt;
}

std::optional<std::size_t> findTopLevelTemplateStart(std::string_view text) {
  for (std::size_t i = 0; i < text.size(); ++i) {
    if (text[i] == '<') {
      return i;
    }
  }
  return std::nullopt;
}

std::optional<std::pair<std::size_t, std::string>>
findTopLevelRequirementRelation(std::string_view text) {
  int angleDepth = 0;
  int parenDepth = 0;
  int braceDepth = 0;
  int bracketDepth = 0;
  for (std::size_t i = 0; i + 1 < text.size(); ++i) {
    const char ch = text[i];
    if (ch == '<') {
      ++angleDepth;
      continue;
    }
    if (ch == '>') {
      angleDepth = std::max(0, angleDepth - 1);
      continue;
    }
    if (ch == '(') {
      ++parenDepth;
      continue;
    }
    if (ch == ')') {
      parenDepth = std::max(0, parenDepth - 1);
      continue;
    }
    if (ch == '{') {
      ++braceDepth;
      continue;
    }
    if (ch == '}') {
      braceDepth = std::max(0, braceDepth - 1);
      continue;
    }
    if (ch == '[') {
      ++bracketDepth;
      continue;
    }
    if (ch == ']') {
      bracketDepth = std::max(0, bracketDepth - 1);
      continue;
    }
    if (angleDepth != 0 || parenDepth != 0 || braceDepth != 0 || bracketDepth != 0) {
      continue;
    }
    const std::string_view op = text.substr(i, 2);
    if (op == "==" || op == "!=" || op == ">=" || op == "<=") {
      return std::make_pair(i, std::string(op));
    }
  }
  return std::nullopt;
}

std::string requirementOperandStableHandle(std::string_view kind, std::string_view text) {
  std::string handle(kind);
  handle.push_back(':');
  handle.append(text);
  return handle;
}

std::string normalizeRequirementTypeofText(std::string text) {
  if (text.rfind("typeof<", 0) == 0 && text.size() > 10 &&
      text.compare(text.size() - 3, 3, ">()") == 0) {
    text.erase(text.size() - 2);
  }
  return text;
}

std::string bindingTypeTextForRequirement(const BindingInfo &binding) {
  if (!binding.typeTemplateArg.empty()) {
    return binding.typeName + "<" + binding.typeTemplateArg + ">";
  }
  return binding.typeName;
}

std::string canonicalPredicateName(std::string name) {
  name = trimRequirementText(name);
  if (name.rfind("std/meta/", 0) == 0) {
    name.insert(name.begin(), '/');
  }
  if (name.rfind("meta.", 0) == 0) {
    name = "/std/meta/" + name.substr(std::string("meta.").size());
  }
  if (name == "type_equals" || name == "equals" || name == "equal" ||
      name == "same_type" || name == "is_same_type") {
    return "/std/meta/type_equals";
  }
  if (name == "type_not_equals" || name == "not_equals" ||
      name == "not_equal" || name == "different_type") {
    return "/std/meta/type_not_equals";
  }
  if (name == "is_type") {
    return "/std/meta/is_type";
  }
  if (name == "is_struct") {
    return "/std/meta/is_struct";
  }
  if (name == "is_sum") {
    return "/std/meta/is_sum";
  }
  return name;
}

bool isBuiltinTypeRelationPredicate(std::string_view name) {
  return name == "/std/meta/type_equals" ||
         name == "/std/meta/type_not_equals" ||
         name == "/std/meta/is_type" ||
         name == "/std/meta/is_struct" ||
         name == "/std/meta/is_sum";
}

std::string canonicalCallSourceText(std::string_view predicateName,
                                    const std::vector<RequirementPredicateOperandFact> &operands) {
  std::ostringstream out;
  out << predicateName << "<";
  for (std::size_t i = 0; i < operands.size(); ++i) {
    if (i != 0) {
      out << ", ";
    }
    out << operands[i].text;
  }
  out << ">()";
  return out.str();
}

RequirementPredicateOperandFact classifyRequirementOperand(
    std::string text,
    std::string_view predicateName,
    std::size_t operandIndex,
    int sourceLine,
    int sourceColumn) {
  text = trimRequirementText(text);
  text = normalizeRequirementTypeofText(std::move(text));
  RequirementPredicateOperandFact operand;
  operand.text = text;
  operand.sourceLine = sourceLine;
  operand.sourceColumn = sourceColumn;

  if (text.rfind("typeof<", 0) == 0 && text.size() > 8 && text.back() == '>') {
    operand.kind = "type_fact";
  } else if ((predicateName.find("type") != std::string_view::npos ||
              predicateName.find("struct") != std::string_view::npos ||
              predicateName.find("sum") != std::string_view::npos ||
              predicateName.find("construct") != std::string_view::npos ||
              predicateName.find("copy") != std::string_view::npos ||
              predicateName.find("move") != std::string_view::npos) &&
             isRequirementSymbolText(text)) {
    operand.kind = "type_fact";
  } else if (isRequirementLiteralText(text)) {
    operand.kind = "literal_compile_time_argument";
  } else if (predicateName.find("trait") != std::string_view::npos &&
             operandIndex > 0 && isRequirementSymbolText(text)) {
    operand.kind = "compile_time_symbol";
  } else if (isRequirementSymbolText(text)) {
    operand.kind = "compile_time_symbol";
  } else {
    operand.kind = "unsupported_runtime_only_expression";
  }
  operand.stableHandle = requirementOperandStableHandle(operand.kind, operand.text);
  return operand;
}

const BindingInfo *findParameterBinding(
    const RequirementPredicateDefinitionContext &context,
    std::string_view name) {
  for (const auto &param : context.params) {
    if (param.name == name) {
      return &param.binding;
    }
  }
  return nullptr;
}

bool isTemplateTypeParameter(const RequirementPredicateDefinitionContext &context,
                             std::string_view name) {
  return std::find(context.templateArgs.begin(), context.templateArgs.end(), name) !=
         context.templateArgs.end();
}

bool isPrimitiveOrBuiltinTypeName(std::string_view typeText) {
  const std::string normalized = normalizeBindingTypeName(std::string(typeText));
  return isPrimitiveBindingTypeName(normalized) ||
         returnKindForTypeName(normalized) != ReturnKind::Unknown ||
         normalized == "Pointer" || normalized == "Reference" ||
         normalized == "array" || normalized == "Buffer" ||
         normalized == "Maybe" || normalized == "Result" ||
         normalized == "soa" "_vector" || normalized == "vector";
}

std::string resolveNominalPath(const RequirementPredicateDefinitionContext &context,
                               const std::string &typeText,
                               const std::unordered_set<std::string> &names) {
  const std::string normalized = normalizeBindingTypeName(typeText);
  if (!normalized.empty() && normalized.front() == '/') {
    return names.count(normalized) > 0 ? normalized : std::string{};
  }
  auto importIt = context.importAliases.find(normalized);
  if (importIt != context.importAliases.end() &&
      names.count(importIt->second) > 0) {
    return importIt->second;
  }
  if (const std::string resolved =
          resolveStructTypePath(normalized, context.namespacePrefix, names);
      !resolved.empty()) {
    return resolved;
  }
  return {};
}

std::string canonicalizeResolvedType(
    const RequirementPredicateDefinitionContext &context,
    const std::string &typeText) {
  std::string normalized = normalizeBindingTypeName(typeText);
  std::string base;
  std::string argText;
  if (splitTemplateTypeName(normalized, base, argText)) {
    std::vector<std::string> args;
    if (!splitTopLevelTemplateArgs(argText, args)) {
      return normalized;
    }
    std::string resolvedBase = canonicalizeResolvedType(context, base);
    if (!resolvedBase.empty() && resolvedBase.front() == '/' &&
        context.sumNames.count(resolvedBase) > 0) {
      const std::string arityPath =
          resolvedBase + "__arity" + std::to_string(args.size());
      if (context.sumNames.count(arityPath) > 0) {
        resolvedBase = arityPath;
      }
    }
    std::ostringstream out;
    out << resolvedBase << "<";
    for (std::size_t i = 0; i < args.size(); ++i) {
      if (i != 0) {
        out << ", ";
      }
      out << canonicalizeResolvedType(context, args[i]);
    }
    out << ">";
    return out.str();
  }
  if (const std::string structPath =
          resolveNominalPath(context, normalized, context.structNames);
      !structPath.empty()) {
    return structPath;
  }
  if (const std::string sumPath =
          resolveNominalPath(context, normalized, context.sumNames);
      !sumPath.empty()) {
    return sumPath;
  }
  return normalized;
}

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

void evaluateRequirementPredicate(RequirementPredicateFactDraft &fact,
                                  const RequirementPredicateDefinitionContext &context) {
  if (!isBuiltinTypeRelationPredicate(fact.predicateName)) {
    fact.evaluationOutcome = "invalid_evaluation";
    fact.evaluationDiagnostic =
        "unknown requirement predicate: " + fact.predicateName;
    return;
  }
  evaluateBuiltinTypePredicate(fact, context);
}

} // namespace

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
      fact.predicateKind = "relation";
      fact.predicateName = "relation";
    }
    fact.operands.push_back(classifyRequirementOperand(
        sourceText.substr(0, relation->first),
        fact.predicateName,
        0,
        sourceLine,
        sourceColumn));
    fact.operands.push_back(classifyRequirementOperand(
        sourceText.substr(relation->first + relation->second.size()),
        fact.predicateName,
        1,
        sourceLine,
        sourceColumn));
    if (fact.predicateKind == "predicate_call") {
      fact.sourceText = canonicalCallSourceText(fact.predicateName, fact.operands);
    } else {
      fact.evaluationDiagnostic =
          "unsupported requirement relation operator: " + relation->second;
    }
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
                                                         sourceLine,
                                                         sourceColumn));
    }
    if (isBuiltinTypeRelationPredicate(fact.predicateName)) {
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
                                                     sourceLine,
                                                     sourceColumn));
  fact.evaluationDiagnostic = "unsupported requirement predicate shape";
  return fact;
}

} // namespace primec::semantics
