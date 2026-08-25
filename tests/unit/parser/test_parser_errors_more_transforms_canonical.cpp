#include "test_parser_errors_more_helpers.h"

TEST_SUITE_BEGIN("primestruct.parser.errors.transforms");

TEST_CASE("statement transform requires callable syntax") {
  const std::string source = R"(
[return<int>]
main() {
  [effects(io_out)] action
  return(1i32)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("transform-prefixed execution requires arguments") != std::string::npos);
}

TEST_CASE("expression transform requires callable syntax") {
  const std::string source = R"(
[return<int>]
main() {
  return(foo([effects(io_out)] action))
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("transform-prefixed execution requires arguments") != std::string::npos);
}

TEST_CASE("canonical mode rejects transform-prefixed loop body sugar") {
  const std::string source = R"(
[return<int>]
main() {
  [effects(io_out)] loop(1i32) { return(1i32) }
  return(1i32)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize(), false);
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("control-flow body sugar requires canonical call form") != std::string::npos);
}

TEST_CASE("canonical mode rejects transform-prefixed loop body sugar in expression") {
  const std::string source = R"(
[return<int>]
main() {
  return(foo([effects(io_out)] loop(1i32) { 1i32 }))
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize(), false);
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("control-flow body sugar requires canonical call form") != std::string::npos);
}

TEST_CASE("transform-prefixed loop parses without body") {
  const std::string source = R"(
[return<int>]
main() {
  [effects(io_out)] loop(1i32)
  return(1i32)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK(parser.parse(program, error));
  CHECK(error.empty());
}

TEST_SUITE_END();
