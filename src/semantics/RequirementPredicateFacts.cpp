#include "RequirementPredicateFacts.h"

#include "primec/StringLiteral.h"

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
  if (name == "has_trait") {
    return "/std/meta/has_trait";
  }
  if (name == "supports_call" || name == "callable") {
    return "/std/meta/supports_call";
  }
  if (name == "can_construct" || name == "constructible") {
    return "/std/meta/can_construct";
  }
  if (name == "can_copy" || name == "copyable") {
    return "/std/meta/can_copy";
  }
  if (name == "can_move" || name == "movable") {
    return "/std/meta/can_move";
  }
  if (name == "has_field") {
    return "/std/meta/has_field";
  }
  if (name == "has_member") {
    return "/std/meta/has_member";
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

bool isBuiltinCapabilityPredicate(std::string_view name) {
  return name == "/std/meta/has_trait" ||
         name == "/std/meta/supports_call" ||
         name == "/std/meta/can_construct" ||
         name == "/std/meta/can_copy" ||
         name == "/std/meta/can_move" ||
         name == "/std/meta/has_field" ||
         name == "/std/meta/has_member";
}

bool isBuiltinRequirementPredicate(std::string_view name) {
  return isBuiltinTypeRelationPredicate(name) ||
         isBuiltinCapabilityPredicate(name);
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
    bool isTemplateOperand,
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
  } else if (isTemplateOperand && isRequirementSymbolText(text)) {
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
    fact.evaluationOutcome = "denied_effect";
    fact.evaluationDiagnostic =
        "denied compile-time effect in user requirement predicate " +
        callable.fullPath + ": " + callable.effectNames.front();
    return;
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
  if (!isAdditive && !isMultiplicative && !isComparable && !isIndexable) {
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
  if (isAdditive) {
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
  evaluateBuiltinCapabilityPredicate(fact, context);
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
