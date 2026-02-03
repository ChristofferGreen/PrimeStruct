#include "primec/Lexer.h"
#include "primec/Parser.h"

#include "third_party/doctest.h"

TEST_SUITE_BEGIN("primestruct.parser.execution.whitespace");

TEST_CASE("rejects empty body with newlines") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

execute_task(1i32)
{
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("executions are not supported in v0.1") != std::string::npos);
}

TEST_CASE("rejects body args across lines") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

execute_task(
  1i32,
  2i32
) {
  main(),
  main()
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("executions are not supported in v0.1") != std::string::npos);
}

TEST_SUITE_END();
