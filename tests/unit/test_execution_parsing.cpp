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

bool parseProgramWithError(const std::string &source, std::string &error) {
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  return parser.parse(program, error);
}

} // namespace

TEST_SUITE_BEGIN("primestruct.parser.executions");

TEST_CASE("parses execution without body") {
  const std::string source = R"(
execute_task(1i32)
)";
  const auto program = parseProgram(source);
  REQUIRE(program.executions.size() == 1);
  CHECK(program.executions[0].arguments.size() == 1);
  CHECK(program.executions[0].bodyArguments.empty());
  CHECK_FALSE(program.executions[0].hasBodyArguments);
}

TEST_CASE("rejects execution body") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

execute_task(1i32) { }
)";
  std::string error;
  CHECK_FALSE(parseProgramWithError(source, error));
  CHECK(error.find("executions do not accept body arguments") != std::string::npos);
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

execute_task([count] 2i32)
)";
  const auto program = parseProgram(source);
  REQUIRE(program.executions.size() == 1);
  REQUIRE(program.executions[0].arguments.size() == 1);
  REQUIRE(program.executions[0].argumentNames.size() == 1);
  CHECK(program.executions[0].argumentNames[0].has_value());
  CHECK(*program.executions[0].argumentNames[0] == "count");
}

TEST_CASE("parses execution bracket-labeled arguments") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

execute_task([count] 2i32)
)";
  const auto program = parseProgram(source);
  REQUIRE(program.executions.size() == 1);
  REQUIRE(program.executions[0].arguments.size() == 1);
  REQUIRE(program.executions[0].argumentNames.size() == 1);
  CHECK(program.executions[0].argumentNames[0].has_value());
  CHECK(*program.executions[0].argumentNames[0] == "count");
}

TEST_CASE("parses execution with collection call arguments") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

execute_task(array<i32>(1i32, 2i32), map<i32, i32>(1i32, 2i32))
)";
  const auto program = parseProgram(source);
  REQUIRE(program.executions.size() == 1);
  CHECK(program.executions[0].arguments.size() == 2);
  CHECK(program.executions[0].bodyArguments.empty());
}

TEST_CASE("parses execution with named args and collections") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

execute_task([items] array<i32>(1i32, 2i32), [pairs] map<i32, i32>(1i32, 2i32))
)";
  const auto program = parseProgram(source);
  REQUIRE(program.executions.size() == 1);
  CHECK(program.executions[0].arguments.size() == 2);
  CHECK(program.executions[0].argumentNames.size() == 2);
  CHECK(program.executions[0].argumentNames[0].has_value());
  CHECK(program.executions[0].argumentNames[1].has_value());
  CHECK(*program.executions[0].argumentNames[0] == "items");
  CHECK(*program.executions[0].argumentNames[1] == "pairs");
}

TEST_SUITE_END();
