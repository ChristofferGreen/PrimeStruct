#include "IrLowererRequirementContractHelpers.h"

#include <algorithm>
#include <cctype>
#include <optional>
#include <string_view>

namespace primec::ir_lowerer {

namespace {

std::string trimContractText(std::string_view text) {
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

bool isContractSymbolText(std::string_view text) {
  if (text.empty()) {
    return false;
  }
  if (!std::isalpha(static_cast<unsigned char>(text.front())) &&
      text.front() != '_') {
    return false;
  }
  return std::all_of(text.begin(), text.end(), [](char ch) {
    return std::isalnum(static_cast<unsigned char>(ch)) || ch == '_';
  });
}

// Mirrors the semantic requirement-relation scanner: bare < and > need
// surrounding whitespace so template argument lists never match.
std::optional<std::pair<std::size_t, std::string>>
findTopLevelContractRelation(std::string_view text) {
  int angleDepth = 0;
  int parenDepth = 0;
  int braceDepth = 0;
  int bracketDepth = 0;
  for (std::size_t i = 0; i + 1 < text.size(); ++i) {
    const char ch = text[i];
    if (angleDepth == 0 && parenDepth == 0 && braceDepth == 0 &&
        bracketDepth == 0) {
      const std::string_view op = text.substr(i, 2);
      if (op == "==" || op == "!=" || op == ">=" || op == "<=") {
        return std::make_pair(i, std::string(op));
      }
      if ((ch == '>' || ch == '<') && i > 0 &&
          std::isspace(static_cast<unsigned char>(text[i - 1])) &&
          std::isspace(static_cast<unsigned char>(text[i + 1]))) {
        return std::make_pair(i, std::string(1, ch));
      }
    }
    if (ch == '<') {
      ++angleDepth;
    } else if (ch == '>') {
      angleDepth = std::max(0, angleDepth - 1);
    } else if (ch == '(') {
      ++parenDepth;
    } else if (ch == ')') {
      parenDepth = std::max(0, parenDepth - 1);
    } else if (ch == '{') {
      ++braceDepth;
    } else if (ch == '}') {
      braceDepth = std::max(0, braceDepth - 1);
    } else if (ch == '[') {
      ++bracketDepth;
    } else if (ch == ']') {
      bracketDepth = std::max(0, bracketDepth - 1);
    }
  }
  return std::nullopt;
}

std::string comparisonCallNameForRelation(std::string_view relation) {
  if (relation == "<") {
    return "less_than";
  }
  if (relation == "<=") {
    return "less_equal";
  }
  if (relation == ">") {
    return "greater_than";
  }
  if (relation == ">=") {
    return "greater_equal";
  }
  return {};
}

// Maps the canonical value-predicate call spellings to the comparison builtin
// the expression emitter lowers. Bare equal/not_equal are type predicates in
// require(...) and stay compile-time-only.
std::string comparisonCallNameForPredicateCall(std::string_view callee) {
  std::string name = trimContractText(callee);
  if (name.rfind("/std/meta/", 0) == 0) {
    name = name.substr(std::string("/std/meta/").size());
  } else if (name.rfind("std/meta/", 0) == 0) {
    name = name.substr(std::string("std/meta/").size());
  } else if (name.rfind("meta.", 0) == 0) {
    name = name.substr(std::string("meta.").size());
  }
  if (name == "value_less" || name == "less_than") {
    return "less_than";
  }
  if (name == "value_less_equal" || name == "less_equal") {
    return "less_equal";
  }
  if (name == "value_greater" || name == "greater_than") {
    return "greater_than";
  }
  if (name == "value_greater_equal" || name == "greater_equal") {
    return "greater_equal";
  }
  if (name == "value_equals" || name == "value_equal") {
    return "equal";
  }
  if (name == "value_not_equals" || name == "value_not_equal") {
    return "not_equal";
  }
  return {};
}

std::optional<std::size_t> findTopLevelContractCallParen(std::string_view text) {
  int angleDepth = 0;
  for (std::size_t i = 0; i < text.size(); ++i) {
    const char ch = text[i];
    if (ch == '<') {
      ++angleDepth;
    } else if (ch == '>') {
      angleDepth = std::max(0, angleDepth - 1);
    } else if (ch == '(' && angleDepth == 0) {
      return i;
    }
  }
  return std::nullopt;
}

std::vector<std::string> splitTopLevelContractArgs(std::string_view text) {
  std::vector<std::string> out;
  int angleDepth = 0;
  int parenDepth = 0;
  int braceDepth = 0;
  int bracketDepth = 0;
  std::size_t start = 0;
  auto pushSegment = [&](std::size_t end) {
    std::string segment = trimContractText(text.substr(start, end - start));
    if (!segment.empty()) {
      out.push_back(std::move(segment));
    }
  };
  for (std::size_t i = 0; i < text.size(); ++i) {
    const char ch = text[i];
    if (ch == '<') {
      ++angleDepth;
    } else if (ch == '>') {
      angleDepth = std::max(0, angleDepth - 1);
    } else if (ch == '(') {
      ++parenDepth;
    } else if (ch == ')') {
      parenDepth = std::max(0, parenDepth - 1);
    } else if (ch == '{') {
      ++braceDepth;
    } else if (ch == '}') {
      braceDepth = std::max(0, braceDepth - 1);
    } else if (ch == '[') {
      ++bracketDepth;
    } else if (ch == ']') {
      bracketDepth = std::max(0, bracketDepth - 1);
    } else if (ch == ',' && angleDepth == 0 && parenDepth == 0 &&
               braceDepth == 0 && bracketDepth == 0) {
      pushSegment(i);
      start = i + 1;
    }
  }
  pushSegment(text.size());
  return out;
}

// Extracts the comparison call name and the two operand texts from a contract
// predicate in either infix relation form (count(values) > 0i32) or canonical
// call form (greater_than(count(values), 0i32), value_greater<...>()).
bool extractContractComparison(const std::string &sourceText,
                               std::string &comparisonNameOut,
                               std::vector<std::string> &operandTextsOut) {
  if (const auto relation = findTopLevelContractRelation(sourceText);
      relation.has_value()) {
    comparisonNameOut = comparisonCallNameForRelation(relation->second);
    if (comparisonNameOut.empty()) {
      return false;
    }
    operandTextsOut.push_back(
        trimContractText(std::string_view(sourceText).substr(0, relation->first)));
    operandTextsOut.push_back(trimContractText(std::string_view(sourceText).substr(
        relation->first + relation->second.size())));
    return true;
  }
  const auto callParen = findTopLevelContractCallParen(sourceText);
  if (!callParen.has_value() || sourceText.back() != ')') {
    return false;
  }
  std::string calleeText =
      trimContractText(std::string_view(sourceText).substr(0, *callParen));
  std::string templateArgsText;
  if (!calleeText.empty() && calleeText.back() == '>') {
    const std::size_t templateStart = calleeText.find('<');
    if (templateStart != std::string::npos) {
      templateArgsText =
          calleeText.substr(templateStart + 1,
                            calleeText.size() - templateStart - 2);
      calleeText = trimContractText(calleeText.substr(0, templateStart));
    }
  }
  comparisonNameOut = comparisonCallNameForPredicateCall(calleeText);
  if (comparisonNameOut.empty()) {
    return false;
  }
  if (!templateArgsText.empty()) {
    for (std::string &arg : splitTopLevelContractArgs(templateArgsText)) {
      operandTextsOut.push_back(std::move(arg));
    }
  }
  const std::string callArgsText = sourceText.substr(
      *callParen + 1, sourceText.size() - *callParen - 2);
  for (std::string &arg : splitTopLevelContractArgs(callArgsText)) {
    operandTextsOut.push_back(std::move(arg));
  }
  return true;
}

const Expr *findDefinitionParameter(const Definition &definition,
                                    std::string_view name) {
  for (const Expr &param : definition.parameters) {
    if (param.name == name) {
      return &param;
    }
  }
  return nullptr;
}

bool parseContractIntegerLiteral(std::string_view text, Expr &exprOut) {
  std::string_view digits = text;
  int intWidth = 32;
  bool isUnsigned = false;
  auto stripSuffix = [&](std::string_view suffix, int width, bool unsignedFlag) {
    if (digits.size() > suffix.size() &&
        digits.substr(digits.size() - suffix.size()) == suffix) {
      digits.remove_suffix(suffix.size());
      intWidth = width;
      isUnsigned = unsignedFlag;
      return true;
    }
    return false;
  };
  if (!stripSuffix("i64", 64, false) && !stripSuffix("u64", 64, true) &&
      !stripSuffix("i32", 32, false) && !stripSuffix("u32", 32, true) &&
      !stripSuffix("i16", 32, false) && !stripSuffix("u16", 32, true) &&
      !stripSuffix("uint", 64, true) && !stripSuffix("int", 64, false) &&
      !stripSuffix("i8", 32, false) && !stripSuffix("u8", 32, true)) {
    // no suffix: bare digits
  }
  if (digits.empty()) {
    return false;
  }
  std::uint64_t value = 0;
  for (const char ch : digits) {
    if (ch == '_' || ch == ',') {
      continue;
    }
    if (!std::isdigit(static_cast<unsigned char>(ch))) {
      return false;
    }
    const std::uint64_t digit = static_cast<std::uint64_t>(ch - '0');
    if (value > (UINT64_MAX - digit) / 10) {
      return false;
    }
    value = value * 10 + digit;
  }
  exprOut = Expr{};
  exprOut.kind = Expr::Kind::Literal;
  exprOut.literalValue = value;
  exprOut.intWidth = intWidth;
  exprOut.isUnsigned = isUnsigned;
  return true;
}

enum class ContractOperandKind {
  CompileTimeOnly,
  Literal,
  RuntimeValue,
};

// Builds the operand expression for a contract operand. CompileTimeOnly marks
// operands the semantic phase already discharged (types, constants), so the
// whole predicate is skipped at lowering.
bool classifyContractOperand(const Definition &definition,
                             std::string_view operandText,
                             ContractOperandKind &kindOut,
                             Expr &exprOut) {
  const std::string text = trimContractText(operandText);
  if (text.empty()) {
    kindOut = ContractOperandKind::CompileTimeOnly;
    return true;
  }
  if (parseContractIntegerLiteral(text, exprOut)) {
    kindOut = ContractOperandKind::Literal;
    return true;
  }
  if (text.rfind("count(", 0) == 0 && text.size() > 7 && text.back() == ')') {
    const std::string inner = trimContractText(
        std::string_view(text).substr(6, text.size() - 7));
    if (isContractSymbolText(inner) &&
        findDefinitionParameter(definition, inner) != nullptr) {
      Expr argExpr;
      argExpr.kind = Expr::Kind::Name;
      argExpr.name = inner;
      exprOut = Expr{};
      exprOut.kind = Expr::Kind::Call;
      exprOut.name = "count";
      exprOut.args.push_back(std::move(argExpr));
      exprOut.argNames.push_back(std::nullopt);
      kindOut = ContractOperandKind::RuntimeValue;
      return true;
    }
    return false;
  }
  if (isContractSymbolText(text)) {
    if (findDefinitionParameter(definition, text) != nullptr) {
      exprOut = Expr{};
      exprOut.kind = Expr::Kind::Name;
      exprOut.name = text;
      kindOut = ContractOperandKind::RuntimeValue;
      return true;
    }
    // Symbols that are not parameters are compile-time facts that semantic
    // validation already proved (template values, enum constants, types).
    kindOut = ContractOperandKind::CompileTimeOnly;
    return true;
  }
  // typeof<...>, predicate calls, and other compile-time-only shapes.
  kindOut = ContractOperandKind::CompileTimeOnly;
  return true;
}

} // namespace

bool collectRequirementContractChecks(const Definition &definition,
                                      std::vector<RequirementContractCheck> &checksOut,
                                      std::string &error) {
  for (const Transform &transform : definition.transforms) {
    if (transform.name != "require") {
      continue;
    }
    for (const std::string &argument : transform.arguments) {
      const std::string sourceText = trimContractText(argument);
      std::string comparisonName;
      std::vector<std::string> operandTexts;
      if (!extractContractComparison(sourceText, comparisonName, operandTexts) ||
          operandTexts.size() != 2) {
        // Not a value comparison: a compile-time predicate semantic
        // validation already discharged.
        continue;
      }
      Expr lhs;
      Expr rhs;
      ContractOperandKind lhsKind = ContractOperandKind::CompileTimeOnly;
      ContractOperandKind rhsKind = ContractOperandKind::CompileTimeOnly;
      const bool lhsOk =
          classifyContractOperand(definition, operandTexts[0], lhsKind, lhs);
      const bool rhsOk =
          classifyContractOperand(definition, operandTexts[1], rhsKind, rhs);
      if (!lhsOk || !rhsOk) {
        error = "unsupported runtime contract predicate operand on " +
                definition.fullPath + ": " + sourceText;
        return false;
      }
      if (lhsKind == ContractOperandKind::CompileTimeOnly ||
          rhsKind == ContractOperandKind::CompileTimeOnly) {
        continue;
      }
      if (lhsKind == ContractOperandKind::Literal &&
          rhsKind == ContractOperandKind::Literal) {
        // Fully constant relations were proven or rejected at compile time.
        continue;
      }
      RequirementContractCheck check;
      check.predicate.kind = Expr::Kind::Call;
      check.predicate.name = comparisonName;
      check.predicate.sourceLine = transform.sourceLine;
      check.predicate.sourceColumn = transform.sourceColumn;
      check.predicate.args.push_back(std::move(lhs));
      check.predicate.args.push_back(std::move(rhs));
      check.predicate.argNames = {std::nullopt, std::nullopt};
      check.sourceText = sourceText;
      checksOut.push_back(std::move(check));
    }
  }
  return true;
}

bool emitRequirementContractChecks(
    const Definition &definition,
    IrFunction &function,
    const std::function<bool(const Expr &)> &emitPredicateExpr,
    const InternRuntimeErrorStringFn &internString,
    std::string &error) {
  std::vector<RequirementContractCheck> checks;
  if (!collectRequirementContractChecks(definition, checks, error)) {
    return false;
  }
  for (const RequirementContractCheck &check : checks) {
    if (!emitPredicateExpr(check.predicate)) {
      if (error.empty()) {
        error = "failed to lower runtime contract predicate on " +
                definition.fullPath + ": " + check.sourceText;
      }
      return false;
    }
    const std::size_t jumpToFailIndex = function.instructions.size();
    function.instructions.push_back({IrOpcode::JumpIfZero, 0});
    const std::size_t jumpOverFailIndex = function.instructions.size();
    function.instructions.push_back({IrOpcode::Jump, 0});
    function.instructions[jumpToFailIndex].imm =
        static_cast<uint64_t>(function.instructions.size());
    const std::string message = "requirement contract failed on " +
                                definition.fullPath + ": " + check.sourceText;
    const uint64_t flags = encodePrintFlags(true, true);
    const int32_t messageIndex = internString(message);
    function.instructions.push_back(
        {IrOpcode::PrintString,
         encodePrintStringImm(static_cast<uint64_t>(messageIndex), flags)});
    function.instructions.push_back({IrOpcode::PushI32, 3});
    function.instructions.push_back({IrOpcode::ReturnI32, 0});
    function.instructions[jumpOverFailIndex].imm =
        static_cast<uint64_t>(function.instructions.size());
  }
  return true;
}

} // namespace primec::ir_lowerer
