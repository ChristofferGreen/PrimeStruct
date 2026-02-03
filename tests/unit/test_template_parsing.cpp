#include "primec/Lexer.h"
#include "primec/Parser.h"

#include "third_party/doctest.h"

TEST_SUITE_BEGIN("primestruct.parser.templates");

TEST_CASE("rejects template list on definition") {
  const std::string source = R"(
[return<int>]
identity<T>(x) {
  return(x)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program.definitions, program.executions, error));
  CHECK(error.find("templates are not supported in v0.1") != std::string::npos);
}

TEST_CASE("rejects template list on call") {
  const std::string source = R"(
[return<int>]
identity(x) {
  return(x)
}

[return<int>]
main() {
  return(identity<int>(3i32))
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program.definitions, program.executions, error));
  CHECK(error.find("templates are not supported in v0.1") != std::string::npos);
}

TEST_SUITE_END();
