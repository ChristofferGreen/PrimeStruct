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

TEST_SUITE_BEGIN("primestruct.parser.executions");

TEST_CASE("parses execution with empty body") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

execute_task(1i32) { }
)";
  const auto program = parseProgram(source);
  REQUIRE(program.executions.size() == 1);
  CHECK(program.executions[0].arguments.size() == 1);
  CHECK(program.executions[0].bodyArguments.empty());
}

TEST_CASE("parses execution with body arguments") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

execute_repeat(3i32) { main(), main() }
)";
  const auto program = parseProgram(source);
  REQUIRE(program.executions.size() == 1);
  CHECK(program.executions[0].arguments.size() == 1);
  CHECK(program.executions[0].bodyArguments.size() == 2);
}

TEST_CASE("parses execution transforms") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

[effects]
execute_task(2i32)
)";
  const auto program = parseProgram(source);
  REQUIRE(program.executions.size() == 1);
  REQUIRE(program.executions[0].transforms.size() == 1);
  CHECK(program.executions[0].transforms[0].name == "effects");
}

TEST_CASE("parses execution named arguments") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

execute_task(count = 2i32) { }
)";
  const auto program = parseProgram(source);
  REQUIRE(program.executions.size() == 1);
  REQUIRE(program.executions[0].arguments.size() == 1);
  REQUIRE(program.executions[0].argumentNames.size() == 1);
  CHECK(program.executions[0].argumentNames[0].has_value());
  CHECK(*program.executions[0].argumentNames[0] == "count");
}

TEST_SUITE_END();
