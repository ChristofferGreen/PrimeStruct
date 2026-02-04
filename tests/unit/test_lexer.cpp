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

TEST_CASE("lexes slash paths as identifiers") {
  const auto tokens = lex("/foo/bar");
  REQUIRE(tokens.size() >= 1);
  CHECK(tokens[0].kind == primec::TokenKind::Identifier);
  CHECK(tokens[0].text == "/foo/bar");
}

TEST_CASE("lexes raw string literals") {
  const auto tokens = lex("R\"(hello world)\"");
  REQUIRE(tokens.size() >= 1);
  CHECK(tokens[0].kind == primec::TokenKind::String);
  CHECK(tokens[0].text == "R\"(hello world)\"");
}

TEST_CASE("lexes string literals with escapes") {
  const auto tokens = lex("\"hello\\nworld\"");
  REQUIRE(tokens.size() >= 1);
  CHECK(tokens[0].kind == primec::TokenKind::String);
  CHECK(tokens[0].text == "\"hello\\nworld\"");
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

TEST_CASE("lexes unknown punctuation as end token") {
  const auto tokens = lex("@");
  REQUIRE(tokens.size() >= 1);
  CHECK(tokens[0].kind == primec::TokenKind::End);
}

TEST_SUITE_END();
