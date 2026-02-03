#include "primec/Lexer.h"
#include "primec/Parser.h"

#include "third_party/doctest.h"

namespace {
primec::Program parseProgram(const std::string &source) {
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK(parser.parse(program, error));
  CHECK(error.empty());
  return program;
}
} // namespace

TEST_SUITE_BEGIN("primestruct.parser.execution.whitespace");

TEST_CASE("parses empty body with newlines") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

execute_task(1i32)
{
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.executions.size() == 1);
  CHECK(program.executions[0].bodyArguments.empty());
}

TEST_CASE("parses body args across lines") {
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
  const auto program = parseProgram(source);
  REQUIRE(program.executions.size() == 1);
  CHECK(program.executions[0].arguments.size() == 2);
  CHECK(program.executions[0].bodyArguments.size() == 2);
}

TEST_SUITE_END();
