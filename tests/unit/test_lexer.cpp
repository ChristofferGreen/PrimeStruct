#include "primec/Lexer.h"

#include "third_party/doctest.h"

namespace {
std::vector<primec::Token> lex(const std::string &source) {
  primec::Lexer lexer(source);
  return lexer.tokenize();
}
} // namespace

TEST_SUITE_BEGIN("primestruct.lexer");

TEST_CASE("lexes namespace keyword and identifiers") {
  const auto tokens = lex("namespace demo { }");
  REQUIRE(tokens.size() >= 4);
  CHECK(tokens[0].kind == primec::TokenKind::KeywordNamespace);
  CHECK(tokens[1].kind == primec::TokenKind::Identifier);
  CHECK(tokens[1].text == "demo");
  CHECK(tokens[2].kind == primec::TokenKind::LBrace);
  CHECK(tokens[3].kind == primec::TokenKind::RBrace);
}

TEST_CASE("lexes import keyword") {
  const auto tokens = lex("import std");
  REQUIRE(tokens.size() >= 2);
  CHECK(tokens[0].kind == primec::TokenKind::KeywordImport);
  CHECK(tokens[1].kind == primec::TokenKind::Identifier);
}

TEST_CASE("lexes slash paths as identifiers") {
  const auto tokens = lex("/foo/bar");
  REQUIRE(tokens.size() >= 1);
  CHECK(tokens[0].kind == primec::TokenKind::Identifier);
  CHECK(tokens[0].text == "/foo/bar");
}

TEST_CASE("lexes raw string literal suffixes") {
  const auto tokens = lex("\"hello world\"raw_utf8");
  REQUIRE(tokens.size() >= 1);
  CHECK(tokens[0].kind == primec::TokenKind::String);
  CHECK(tokens[0].text == "\"hello world\"raw_utf8");
}

TEST_CASE("lexes string literal with underscore suffix") {
  const auto tokens = lex("\"hello\"_custom");
  REQUIRE(tokens.size() >= 1);
  CHECK(tokens[0].kind == primec::TokenKind::String);
  CHECK(tokens[0].text == "\"hello\"_custom");
}

TEST_CASE("lexes single-quoted string literals") {
  const auto tokens = lex("'hello world'utf8");
  REQUIRE(tokens.size() >= 1);
  CHECK(tokens[0].kind == primec::TokenKind::String);
  CHECK(tokens[0].text == "'hello world'utf8");
}

TEST_CASE("lexes string literals with escapes") {
  const auto tokens = lex("\"hello\\nworld\"");
  REQUIRE(tokens.size() >= 1);
  CHECK(tokens[0].kind == primec::TokenKind::String);
  CHECK(tokens[0].text == "\"hello\\nworld\"");
}

TEST_CASE("lexes unterminated string literal as invalid token") {
  const auto tokens = lex("\"hello");
  REQUIRE(tokens.size() >= 1);
  CHECK(tokens[0].kind == primec::TokenKind::Invalid);
  CHECK(tokens[0].text.find("unterminated string literal") != std::string::npos);
}

TEST_CASE("lexes numeric literals with suffixes and exponents") {
  const auto tokens = lex("123i32 0x2Ai32 -5i32 9i64 10u64 1.25f 2.5f32 3.0f64 1e-3 1e+3f");
  REQUIRE(tokens.size() >= 10);
  CHECK(tokens[0].text == "123i32");
  CHECK(tokens[1].text == "0x2Ai32");
  CHECK(tokens[2].text == "-5i32");
  CHECK(tokens[3].text == "9i64");
  CHECK(tokens[4].text == "10u64");
  CHECK(tokens[5].text == "1.25f");
  CHECK(tokens[6].text == "2.5f32");
  CHECK(tokens[7].text == "3.0f64");
  CHECK(tokens[8].text == "1e-3");
  CHECK(tokens[9].text == "1e+3f");
}

TEST_CASE("lexes numeric literals with comma separators") {
  const auto tokens = lex("1,000i32 0xAB,CDi32 1,234.5f32");
  REQUIRE(tokens.size() >= 3);
  CHECK(tokens[0].kind == primec::TokenKind::Number);
  CHECK(tokens[0].text == "1,000i32");
  CHECK(tokens[1].kind == primec::TokenKind::Number);
  CHECK(tokens[1].text == "0xAB,CDi32");
  CHECK(tokens[2].kind == primec::TokenKind::Number);
  CHECK(tokens[2].text == "1,234.5f32");
}

TEST_CASE("lexes numbers with trailing dot and empty hex") {
  const auto tokens = lex("1. 0x");
  REQUIRE(tokens.size() >= 2);
  CHECK(tokens[0].kind == primec::TokenKind::Number);
  CHECK(tokens[0].text == "1.");
  CHECK(tokens[1].kind == primec::TokenKind::Number);
  CHECK(tokens[1].text == "0x");
}

TEST_CASE("skips comments and tracks lines") {
  const auto tokens = lex("alpha // comment\nbeta");
  REQUIRE(tokens.size() >= 2);
  CHECK(tokens[0].kind == primec::TokenKind::Identifier);
  CHECK(tokens[0].text == "alpha");
  CHECK(tokens[0].line == 1);
  CHECK(tokens[1].kind == primec::TokenKind::Identifier);
  CHECK(tokens[1].text == "beta");
  CHECK(tokens[1].line == 2);
  CHECK(tokens[1].column == 1);
}

TEST_CASE("skips block comments") {
  const auto tokens = lex("alpha /* comment */ beta");
  REQUIRE(tokens.size() >= 2);
  CHECK(tokens[0].text == "alpha");
  CHECK(tokens[1].text == "beta");
  CHECK(tokens[1].line == 1);
}

TEST_CASE("skips block comments with newline") {
  const auto tokens = lex("alpha /* comment\nline */ beta");
  REQUIRE(tokens.size() >= 2);
  CHECK(tokens[1].text == "beta");
  CHECK(tokens[1].line == 2);
}

TEST_CASE("lexes punctuation tokens") {
  const auto tokens = lex("[](){}<>,.=*;");
  REQUIRE(tokens.size() >= 13);
  CHECK(tokens[0].kind == primec::TokenKind::LBracket);
  CHECK(tokens[1].kind == primec::TokenKind::RBracket);
  CHECK(tokens[2].kind == primec::TokenKind::LParen);
  CHECK(tokens[3].kind == primec::TokenKind::RParen);
  CHECK(tokens[4].kind == primec::TokenKind::LBrace);
  CHECK(tokens[5].kind == primec::TokenKind::RBrace);
  CHECK(tokens[6].kind == primec::TokenKind::LAngle);
  CHECK(tokens[7].kind == primec::TokenKind::RAngle);
  CHECK(tokens[8].kind == primec::TokenKind::Comma);
  CHECK(tokens[9].kind == primec::TokenKind::Dot);
  CHECK(tokens[10].kind == primec::TokenKind::Equal);
  CHECK(tokens[11].kind == primec::TokenKind::Star);
  CHECK(tokens[12].kind == primec::TokenKind::Semicolon);
}

TEST_CASE("lexes unknown punctuation as end token") {
  const auto tokens = lex("@");
  REQUIRE(tokens.size() >= 1);
  CHECK(tokens[0].kind == primec::TokenKind::Invalid);
}

TEST_CASE("lexes minus as invalid punctuation") {
  const auto tokens = lex("-");
  REQUIRE(tokens.size() >= 1);
  CHECK(tokens[0].kind == primec::TokenKind::Invalid);
  CHECK(tokens[0].text == "-");
}

TEST_CASE("lexes unterminated block comment as invalid token") {
  const auto tokens = lex("/* nope");
  REQUIRE(tokens.size() >= 1);
  CHECK(tokens[0].kind == primec::TokenKind::Invalid);
  CHECK(tokens[0].text.find("unterminated block comment") != std::string::npos);
}

TEST_SUITE_END();
