#pragma once

#include "test_parser_errors_more_helpers.h"

TEST_SUITE_BEGIN("primestruct.parser.errors.punctuation");

TEST_CASE("top-level bindings are rejected") {
  const std::string source = R"(
[i32] value{1i32}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("bindings are only allowed inside definition bodies or parameter lists") != std::string::npos);
}

TEST_CASE("empty transform list is rejected") {
  const std::string source = R"(
[]
main() {
  return(1i32)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("transform list cannot be empty") != std::string::npos);
}

TEST_CASE("non-ascii transform identifier rejected") {
  const std::string source = std::string(R"(
[tr)") + "\xC3\xA9" + R"(]
main() {
  return(1i32)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("invalid character") != std::string::npos);
}

TEST_CASE("invalid punctuation character rejected") {
  const std::string source = R"(
[return<int>]
main() {
  @
  return(1i32)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("invalid character") != std::string::npos);
}

TEST_CASE("unterminated string literal rejected") {
  const std::string source = R"(
[return<int>]
main() {
  print("hello)
  return(1i32)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("unterminated string literal") != std::string::npos);
}

TEST_CASE("unterminated block comment rejected") {
  const std::string source = R"(
[return<int>]
main() {
  /* missing terminator
  return(1i32)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("unterminated block comment") != std::string::npos);
}

TEST_CASE("slash path transform identifier accepted") {
  const std::string source = R"(
[/demo]
main() {
  return(1i32)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK(parser.parse(program, error));
  CHECK(error.empty());
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].transforms.size() == 1);
  CHECK(program.definitions[0].transforms[0].name == "/demo");
}

TEST_CASE("non-ascii transform argument rejected") {
  const std::string source = std::string(R"(
[effects(i)") + "\xC3\xA9" + R"(o)]
main() {
  return(1i32)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("invalid character") != std::string::npos);
}

TEST_CASE("parameter identifiers reject template arguments") {
  const std::string source = R"(
[return<int>]
main([i32] value<i32>) {
  return(0i32)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("parameter identifiers do not accept template arguments") != std::string::npos);
}

TEST_CASE("binding initializer rejects named arguments with equals") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{a /* label */ = 1i32}
  return(value)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("unexpected token in expression") != std::string::npos);
}

TEST_SUITE_END();
