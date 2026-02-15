#include "src/text_filter/TextFilterHelpers.h"

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
  CHECK(maybeAppendI32("-7") == "-7i32");
  CHECK(maybeAppendI32("0x2A") == "0x2Ai32");
  CHECK(maybeAppendI32("-0x2A") == "-0x2Ai32");
  CHECK(maybeAppendI32("1foo") == "1foo");
  CHECK(maybeAppendI32("-") == "-");

  CHECK(maybeAppendUtf8("\"hi\"") == "\"hi\"utf8");
  CHECK(maybeAppendUtf8("\"hi\"ascii") == "\"hi\"ascii");
  CHECK(maybeAppendUtf8("value") == "value");
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

  output = "stay";
  index = 0;
  CHECK_FALSE(rewriteUnaryNot("!=", output, index, options));
  CHECK(output == "stay");

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

  output.clear();
  index = 0;
  CHECK(rewriteUnaryMinus("-value", output, index, options));
  CHECK(output == "negate(value)");
  CHECK(index == std::string("-value").size() - 1);

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

TEST_SUITE_END();
