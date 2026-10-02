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
    if (angleDepth == 0 && parenDepth == 0 && braceDepth == 0 && bracketDepth == 0) {
      const std::string_view op = text.substr(i, 2);
      if (op == "==" || op == "!=" || op == ">=" || op == "<=") {
        return std::make_pair(i, std::string(op));
      }
      if ((ch == '>' || ch == '<') && i > 0 && i + 1 < text.size() &&
          std::isspace(static_cast<unsigned char>(text[i - 1])) &&
          std::isspace(static_cast<unsigned char>(text[i + 1]))) {
        return std::make_pair(i, std::string(1, ch));
      }
    }
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
  if (name == "value_equals" || name == "value_equal") {
    return "/std/meta/value_equals";
  }
  if (name == "value_not_equals" || name == "value_not_equal") {
    return "/std/meta/value_not_equals";
  }
  if (name == "value_less" || name == "less_than") {
    return "/std/meta/value_less";
  }
  if (name == "value_less_equal" || name == "less_equal") {
    return "/std/meta/value_less_equal";
  }
  if (name == "value_greater" || name == "greater_than") {
    return "/std/meta/value_greater";
  }
  if (name == "value_greater_equal" || name == "greater_equal") {
    return "/std/meta/value_greater_equal";
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

bool isBuiltinValuePredicate(std::string_view name) {
  return name == "/std/meta/value_equals" ||
         name == "/std/meta/value_not_equals" ||
         name == "/std/meta/value_less" ||
         name == "/std/meta/value_less_equal" ||
         name == "/std/meta/value_greater" ||
         name == "/std/meta/value_greater_equal";
}

bool isBuiltinRequirementPredicate(std::string_view name) {
  return isBuiltinTypeRelationPredicate(name) ||
         isBuiltinCapabilityPredicate(name) ||
         isBuiltinValuePredicate(name);
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
  } else if (isBuiltinValuePredicate(predicateName) &&
             isRequirementLiteralText(text)) {
    operand.kind = "literal_compile_time_argument";
  } else if (isBuiltinValuePredicate(predicateName) &&
             isRequirementSymbolText(text)) {
    operand.kind = "compile_time_symbol";
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
         normalized == "soa" || normalized == "vector";
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

} // namespace requirementFacts
} // namespace primec::semantics
