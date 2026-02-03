#include "primec/Lexer.h"
#include "primec/Parser.h"

#include "third_party/doctest.h"

TEST_SUITE_BEGIN("primestruct.parser.errors.more");

TEST_CASE("multiple return statements fail in parser") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
  return(2i32)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program.definitions, program.executions, error));
  CHECK(error.find("expected '}' after return statement") != std::string::npos);
}

TEST_CASE("missing return turns into execution") {
  const std::string source = R"(
[return<int>]
main() {
  helper()
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK(parser.parse(program.definitions, program.executions, error));
  CHECK(error.empty());
  CHECK(program.definitions.empty());
  CHECK(program.executions.size() == 1);
  CHECK(program.executions[0].fullPath == "/main");
}

TEST_SUITE_END();
