#include "src/text_filter/TextFilterHelpers.h"

#include "primec/Ast.h"
#include "primec/TextFilterPipeline.h"
#include "primec/TransformRules.h"

#include "third_party/doctest.h"

#include <string>

TEST_SUITE_BEGIN("primestruct.text_filters.helpers");

TEST_CASE("character classification helpers") {
  using namespace primec::text_filter;
  CHECK(isSeparator(' '));
  CHECK(isSeparator('('));
  CHECK_FALSE(isSeparator('a'));
  CHECK(isTokenChar('a'));
  CHECK(isTokenChar('Z'));
  CHECK(isTokenChar('0'));
  CHECK(isTokenChar('_'));
  CHECK_FALSE(isTokenChar('-'));
  CHECK(isStringSuffixStart('_'));
  CHECK(isStringSuffixStart('A'));
  CHECK_FALSE(isStringSuffixStart('9'));
  CHECK(isStringSuffixBody('9'));
  CHECK(isOperatorTokenChar('.'));
  CHECK(isOperatorTokenChar('/'));
  CHECK(isLeftOperandEndChar(')'));
  CHECK(isLeftOperandEndChar('"'));
  CHECK(isDigitChar('8'));
  CHECK_FALSE(isDigitChar('x'));
  CHECK(isHexDigitChar('a'));
  CHECK(isHexDigitChar('F'));
  CHECK_FALSE(isHexDigitChar('g'));
}

TEST_CASE("unary prefix detection") {
  using namespace primec::text_filter;
  CHECK(isUnaryPrefixPosition("-a", 0));
  CHECK_FALSE(isUnaryPrefixPosition("a-b", 1));
  CHECK(isUnaryPrefixPosition("a*-b", 2));
  CHECK(isUnaryPrefixPosition("a -b", 2));
  CHECK_FALSE(isUnaryPrefixPosition("a)-b", 2));
}

TEST_CASE("right operand start detection") {
  using namespace primec::text_filter;
  CHECK(isRightOperandStartChar("-value", 0));
  CHECK(isRightOperandStartChar("(value)", 0));
  CHECK(isRightOperandStartChar("\"hi\"", 0));
  CHECK_FALSE(isRightOperandStartChar("?value", 0));
  CHECK_FALSE(isRightOperandStartChar("a-b", 1));
}

TEST_CASE("exponent sign detection") {
  using namespace primec::text_filter;
  CHECK(isExponentSign("1e-3", 2));
  CHECK(isExponentSign("1E+3", 2));
  CHECK_FALSE(isExponentSign("1-3", 1));
  CHECK_FALSE(isExponentSign("e-3", 1));
}

TEST_CASE("quoted helper functions") {
  using namespace primec::text_filter;
  std::string escaped = "\\\"";
  CHECK(isEscaped(escaped, 1));
  std::string notEscaped = "\\\\\"";
  CHECK_FALSE(isEscaped(notEscaped, 2));

  std::string quoted = "\"a\\\"b\"";
  CHECK(findQuotedStart(quoted, quoted.size() - 1) == 0);
  CHECK(findQuotedStart(quoted, 0) == std::string::npos);

  std::string single = "'a\\'b'";
  CHECK(skipQuotedForward(single, 0) == 4);

  std::string unterminated = "\"oops";
  CHECK(skipQuotedForward(unterminated, 0) == std::string::npos);
}

TEST_CASE("matching parens skip quoted text") {
  using namespace primec::text_filter;
  const std::string text = "(a(\" )\" ) b)";
  CHECK(findMatchingClose(text, 0, '(', ')') == text.size() - 1);
  CHECK(findMatchingOpen(text, text.size() - 1, '(', ')') == 0);

  const std::string nested = "(a(b)c)";
  CHECK(findMatchingClose(nested, 0, '(', ')') == nested.size() - 1);

  const std::string badQuote = "(a(\"b)";
  CHECK(findMatchingClose(badQuote, 0, '(', ')') == std::string::npos);

  const std::string unmatchedQuote = "(\"oops)";
  CHECK(findMatchingOpen(unmatchedQuote, unmatchedQuote.size() - 1, '(', ')') == std::string::npos);

  const std::string noClose = "(";
  CHECK(findMatchingClose(noClose, 0, '(', ')') == std::string::npos);
}

TEST_CASE("token boundary helpers") {
  using namespace primec::text_filter;
  const std::string suffixed = "\"hi\"utf8";
  CHECK(findLeftTokenStart(suffixed, suffixed.size()) == 0);
  CHECK(findRightTokenEnd(suffixed, 0) == suffixed.size());

  const std::string templated = "convert<i32>(value)";
  CHECK(findLeftTokenStart(templated, templated.size()) == 0);
  CHECK(findRightTokenEnd(templated, 0) == templated.size());

  const std::string exponent = "1e-3";
  CHECK(findLeftTokenStart(exponent, exponent.size()) == 0);

  const std::string unary = "-value";
  CHECK(findRightTokenEnd(unary, 0) == unary.size());

  const std::string grouped = "(a(b))";
  CHECK(findRightTokenEnd(grouped, 0) == grouped.size());

  const std::string openParen = "(a";
  CHECK(findRightTokenEnd(openParen, 0) == 0);
}

TEST_CASE("strip and normalize unary operands") {
  using namespace primec::text_filter;
  CHECK(stripOuterParens("(a)") == "a");
  CHECK(stripOuterParens("(a)(b)") == "(a)(b)");

  CHECK(normalizeUnaryOperand("-value") == "negate(value)");
  CHECK(normalizeUnaryOperand("-(value)") == "negate(value)");
  CHECK(normalizeUnaryOperand("-1i32") == "-1i32");
}

TEST_CASE("template list heuristics") {
  using namespace primec::text_filter;
  std::string templated = "convert<i32>";
  size_t openPos = templated.find('<');
  size_t closePos = templated.find('>');
  CHECK(looksLikeTemplateList(templated, openPos));
  CHECK(looksLikeTemplateListClose(templated, closePos));

  std::string invalid = "convert<>";
  openPos = invalid.find('<');
  CHECK_FALSE(looksLikeTemplateList(invalid, openPos));

  std::string notTemplate = "value>";
  closePos = notTemplate.find('>');
  CHECK_FALSE(looksLikeTemplateListClose(notTemplate, closePos));

  std::string nested = "convert<map<i32, array<i32>>>";
  openPos = nested.find('<');
  closePos = nested.rfind('>');
  CHECK(looksLikeTemplateList(nested, openPos));
  CHECK(looksLikeTemplateListClose(nested, closePos));
}

TEST_CASE("literal suffix helpers") {
  using namespace primec::text_filter;
  CHECK(maybeAppendI32("123") == "123i32");
  CHECK(maybeAppendI32("1,234") == "1,234i32");
  CHECK(maybeAppendI32("-7") == "-7i32");
  CHECK(maybeAppendI32("0x2A") == "0x2Ai32");
  CHECK(maybeAppendI32("0xDE,AD") == "0xDE,ADi32");
  CHECK(maybeAppendI32("-0x2A") == "-0x2Ai32");
  CHECK(maybeAppendI32("-0xDE,AD") == "-0xDE,ADi32");
  CHECK(maybeAppendI32("1,,234") == "1,,234");
  CHECK(maybeAppendI32("0xDE,,AD") == "0xDE,,AD");
  CHECK(maybeAppendI32("1foo") == "1foo");
  CHECK(maybeAppendI32("-") == "-");

  CHECK(maybeAppendUtf8("\"hi\"") == "\"hi\"utf8");
  CHECK(maybeAppendUtf8("\"hi\"ascii") == "\"hi\"ascii");
  CHECK(maybeAppendUtf8("value") == "value");
  CHECK(maybeAppendUtf8("") == "");
}

TEST_CASE("rewrite unary helpers") {
  using namespace primec::text_filter;
  std::string output;
  size_t index = 0;
  primec::TextFilterOptions options;

  std::string input = "!flag";
  CHECK(rewriteUnaryNot(input, output, index, options));
  CHECK(output == "not(flag)");
  CHECK(index == input.size() - 1);

  output = "unchanged";
  index = 0;
  CHECK_FALSE(rewriteUnaryNot("value", output, index, options));
  CHECK(output == "unchanged");
  CHECK(index == 0);

  output = "stay";
  index = 0;
  CHECK_FALSE(rewriteUnaryNot("!=", output, index, options));
  CHECK(output == "stay");

  output = "bang";
  index = 0;
  CHECK_FALSE(rewriteUnaryNot("!", output, index, options));
  CHECK(output == "bang");
  CHECK(index == 0);

  output = "keep";
  index = 1;
  CHECK_FALSE(rewriteUnaryNot("a!b", output, index, options));
  CHECK(output == "keep");
  CHECK(index == 1);

  output = "double";
  index = 0;
  CHECK_FALSE(rewriteUnaryNot("!!=flag", output, index, options));
  CHECK(output == "double");
  CHECK(index == 0);

  output.clear();
  index = 0;
  CHECK(rewriteUnaryNot("!!flag", output, index, options));
  CHECK(output == "not(not(flag))");
  CHECK(index == std::string("!!flag").size() - 1);

  output.clear();
  index = 0;
  primec::TextFilterOptions implicitOptions;
  implicitOptions.enabledFilters = {"implicit-i32"};
  CHECK(rewriteUnaryNot("!1", output, index, implicitOptions));
  CHECK(output == "not(1i32)");

  output.clear();
  index = 0;
  CHECK(rewriteUnaryAddressOf("&value", output, index, options));
  CHECK(output == "location(value)");
  CHECK(index == std::string("&value").size() - 1);

  output.clear();
  index = 0;
  CHECK(rewriteUnaryDeref("*ptr", output, index, options));
  CHECK(output == "dereference(ptr)");
  CHECK(index == std::string("*ptr").size() - 1);

  const std::string infixDeref = "a*ptr";
  output = "stay";
  index = 1;
  CHECK_FALSE(rewriteUnaryDeref(infixDeref, output, index, options));
  CHECK(output == "stay");
  CHECK(index == 1);

  output = "star";
  index = 0;
  CHECK_FALSE(rewriteUnaryDeref("*", output, index, options));
  CHECK(output == "star");
  CHECK(index == 0);

  output = "none";
  index = 0;
  CHECK_FALSE(rewriteUnaryDeref("*-ptr", output, index, options));
  CHECK(output == "none");
  CHECK(index == 0);

  output.clear();
  index = 0;
  CHECK(rewriteUnaryMinus("-value", output, index, options));
  CHECK(output == "negate(value)");
  CHECK(index == std::string("-value").size() - 1);

  output = "edge";
  index = 0;
  CHECK_FALSE(rewriteUnaryMinus("-", output, index, options));
  CHECK(output == "edge");
  CHECK(index == 0);

  output.clear();
  index = 0;
  primec::TextFilterOptions implicitMinusOptions;
  implicitMinusOptions.enabledFilters = {"implicit-i32"};
  CHECK(rewriteUnaryMinus("-foo", output, index, implicitMinusOptions));
  CHECK(output == "negate(foo)");
  CHECK(index == std::string("-foo").size() - 1);

  output = "keepminus";
  index = 1;
  CHECK_FALSE(rewriteUnaryMinus("a-value", output, index, options));
  CHECK(output == "keepminus");
  CHECK(index == 1);

  output = "nochange";
  index = 0;
  CHECK_FALSE(rewriteUnaryMinus("-1", output, index, options));
  CHECK(output == "nochange");
}

TEST_CASE("rewrite unary helpers with parens") {
  using namespace primec::text_filter;
  std::string output;
  size_t index = 0;
  primec::TextFilterOptions options;

  CHECK(rewriteUnaryAddressOf("&(value)", output, index, options));
  CHECK(output == "location");
  CHECK(index == 0);

  output.clear();
  index = 0;
  CHECK(rewriteUnaryDeref("*(value)", output, index, options));
  CHECK(output == "dereference");
  CHECK(index == 0);

  output.clear();
  index = 0;
  CHECK(rewriteUnaryMinus("-(value)", output, index, options));
  CHECK(output == "negate");
  CHECK(index == 0);
}

TEST_CASE("rewrite unary address-of rejects logical and") {
  using namespace primec::text_filter;
  std::string output = "unchanged";
  size_t index = 0;
  primec::TextFilterOptions options;

  CHECK_FALSE(rewriteUnaryAddressOf("value", output, index, options));
  CHECK(output == "unchanged");
  CHECK(index == 0);

  CHECK_FALSE(rewriteUnaryAddressOf("&&value", output, index, options));
  CHECK(output == "unchanged");
  CHECK(index == 0);

  output = "amp";
  index = 0;
  CHECK_FALSE(rewriteUnaryAddressOf("&", output, index, options));
  CHECK(output == "amp");
  CHECK(index == 0);

  const std::string chained = "a&&value";
  output = "still";
  index = 2;
  CHECK_FALSE(rewriteUnaryAddressOf(chained, output, index, options));
  CHECK(output == "still");
  CHECK(index == 2);

  output = "token";
  index = 0;
  CHECK_FALSE(rewriteUnaryAddressOf("&-value", output, index, options));
  CHECK(output == "token");
  CHECK(index == 0);

  const std::string infix = "a&value";
  output = "infix";
  index = 1;
  CHECK_FALSE(rewriteUnaryAddressOf(infix, output, index, options));
  CHECK(output == "infix");
  CHECK(index == 1);
}

TEST_CASE("rewrite unary mutation helpers") {
  using namespace primec::text_filter;
  std::string output;
  size_t index = 0;
  primec::TextFilterOptions options;

  std::string prefix = "++value";
  CHECK(rewriteUnaryIncrement(prefix, output, index, options));
  CHECK(output == "increment(value)");
  CHECK(index == prefix.size() - 1);

  output = "badprefix";
  index = 0;
  CHECK_FALSE(rewriteUnaryIncrement("++?a", output, index, options));
  CHECK(output == "badprefix");
  CHECK(index == 0);

  output.clear();
  index = 0;
  CHECK(rewriteUnaryIncrement("++(value)", output, index, options));
  CHECK(output == "increment");
  CHECK(index == 1);

  const std::string postfix = "value--";
  output = "value";
  index = 5;
  CHECK(rewriteUnaryDecrement(postfix, output, index, options));
  CHECK(output == "decrement(value)");
  CHECK(index == postfix.size() - 1);

  output.clear();
  index = 0;
  CHECK(rewriteUnaryDecrement("--value", output, index, options));
  CHECK(output == "decrement(value)");
  CHECK(index == std::string("--value").size() - 1);

  const std::string infix = "value--next";
  output = "value";
  index = 5;
  CHECK_FALSE(rewriteUnaryDecrement(infix, output, index, options));
  CHECK(output == "value");
  CHECK(index == 5);

  output = "stay";
  index = 0;
  CHECK_FALSE(rewriteUnaryIncrement("++", output, index, options));
  CHECK(output == "stay");
  CHECK(index == 0);
}

TEST_CASE("transform rules reject non-absolute root wildcard match") {
  primec::TextTransformRule rule;
  rule.wildcard = true;
  rule.path.clear();
  CHECK_FALSE(primec::ruleMatchesPath(rule, "main"));
}

TEST_CASE("transform rules reject empty path for root wildcard match") {
  primec::TextTransformRule rule;
  rule.wildcard = true;
  rule.path.clear();
  CHECK_FALSE(primec::ruleMatchesPath(rule, ""));
}

TEST_CASE("apply semantic transform rules returns early when empty") {
  primec::Program program;
  primec::Definition def;
  def.fullPath = "/main";
  program.definitions.push_back(def);

  primec::applySemanticTransformRules(program, {});
  REQUIRE(program.definitions.size() == 1);
  CHECK(program.definitions[0].transforms.empty());
}

TEST_CASE("apply semantic transform rules handles executions") {
  primec::Program program;
  primec::Execution exec;
  exec.fullPath = "/main";
  program.executions.push_back(exec);

  primec::TextTransformRule rule;
  rule.path = "/main";
  rule.transforms = {"single_type_to_return"};
  primec::applySemanticTransformRules(program, {rule});

  REQUIRE(program.executions.size() == 1);
  REQUIRE(program.executions[0].transforms.size() == 1);
  CHECK(program.executions[0].transforms[0].name == "single_type_to_return");
  CHECK(program.executions[0].transforms[0].phase == primec::TransformPhase::Semantic);
}

TEST_CASE("apply semantic transform rules skips unmatched execution") {
  primec::Program program;
  primec::Execution exec;
  exec.fullPath = "/main";
  program.executions.push_back(exec);

  primec::TextTransformRule rule;
  rule.path = "/other";
  rule.transforms = {"single_type_to_return"};
  primec::applySemanticTransformRules(program, {rule});

  REQUIRE(program.executions.size() == 1);
  CHECK(program.executions[0].transforms.empty());
}

TEST_SUITE_END();
