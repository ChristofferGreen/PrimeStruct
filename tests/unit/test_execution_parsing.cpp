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
  CHECK(program.executions[0].hasBodyArguments);
}

TEST_CASE("parses execution without body as empty") {
  const std::string source = R"(
execute_task(1i32)
)";
  const auto program = parseProgram(source);
  REQUIRE(program.executions.size() == 1);
  CHECK(program.executions[0].arguments.size() == 1);
  CHECK(program.executions[0].bodyArguments.empty());
  CHECK(program.executions[0].hasBodyArguments);
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

TEST_CASE("parses execution body with whitespace-separated calls") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

execute_repeat(3i32) { main() main() }
)";
  const auto program = parseProgram(source);
  REQUIRE(program.executions.size() == 1);
  CHECK(program.executions[0].arguments.size() == 1);
  CHECK(program.executions[0].bodyArguments.size() == 2);
}

TEST_CASE("parses execution body with non-call form") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

execute_repeat(1i32) { 1i32 }
)";
  const auto program = parseProgram(source);
  REQUIRE(program.executions.size() == 1);
  REQUIRE(program.executions[0].bodyArguments.size() == 1);
  const auto &arg = program.executions[0].bodyArguments[0];
  CHECK(arg.kind == primec::Expr::Kind::Literal);
  CHECK(arg.literalValue == 1);
}

TEST_CASE("parses execution body with mixed separators") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

execute_repeat(3i32) { main(), main() main() }
)";
  const auto program = parseProgram(source);
  REQUIRE(program.executions.size() == 1);
  CHECK(program.executions[0].arguments.size() == 1);
  CHECK(program.executions[0].bodyArguments.size() == 3);
}

TEST_CASE("parses execution body with transform-prefixed call") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

execute_repeat(1i32) { [effects(io_out)] print_line("hi"utf8) }
)";
  const auto program = parseProgram(source);
  REQUIRE(program.executions.size() == 1);
  const auto &bodyArgs = program.executions[0].bodyArguments;
  REQUIRE(bodyArgs.size() == 1);
  CHECK(bodyArgs[0].name == "print_line");
  REQUIRE(bodyArgs[0].transforms.size() == 1);
  CHECK(bodyArgs[0].transforms[0].name == "effects");
}

TEST_CASE("parses execution with identifier argument and body") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

execute_task(worker) { main() }
)";
  const auto program = parseProgram(source);
  REQUIRE(program.executions.size() == 1);
  REQUIRE(program.executions[0].arguments.size() == 1);
  CHECK(program.executions[0].arguments[0].kind == primec::Expr::Kind::Name);
  CHECK(program.executions[0].arguments[0].name == "worker");
  REQUIRE(program.executions[0].bodyArguments.size() == 1);
  CHECK(program.executions[0].bodyArguments[0].name == "main");
}

TEST_CASE("parses execution with if statement sugar") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

execute_repeat(1i32) {
  if(true) { main() } else { main() }
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.executions.size() == 1);
  REQUIRE(program.executions[0].bodyArguments.size() == 1);
  const auto &stmt = program.executions[0].bodyArguments[0];
  CHECK(stmt.kind == primec::Expr::Kind::Call);
  CHECK(stmt.name == "if");
  REQUIRE(stmt.args.size() == 3);
  CHECK(stmt.args[1].name == "then");
  CHECK(stmt.args[2].name == "else");
  REQUIRE(stmt.args[1].bodyArguments.size() == 1);
  REQUIRE(stmt.args[2].bodyArguments.size() == 1);
  CHECK(stmt.args[1].bodyArguments[0].name == "main");
  CHECK(stmt.args[2].bodyArguments[0].name == "main");
}

TEST_CASE("parses execution body with bindings") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

execute_repeat(1i32) { [i32] value{1i32} main() }
)";
  const auto program = parseProgram(source);
  REQUIRE(program.executions.size() == 1);
  const auto &bodyArgs = program.executions[0].bodyArguments;
  REQUIRE(bodyArgs.size() == 2);
  CHECK(bodyArgs[0].isBinding);
  CHECK(bodyArgs[0].name == "value");
  CHECK(bodyArgs[1].name == "main");
}

TEST_CASE("rejects execution body with return") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

execute_repeat(1i32) { return(1i32) }
)";
  std::string error;
  CHECK_FALSE(parseProgramWithError(source, error));
  CHECK(error.find("return not allowed in execution body") != std::string::npos);
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

execute_task([count] 2i32) { }
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

execute_task([count] 2i32) { }
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

execute_task(array<i32>(1i32, 2i32), map<i32, i32>(1i32, 2i32)) { }
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

execute_task([items] array<i32>(1i32, 2i32), [pairs] map<i32, i32>(1i32, 2i32)) { }
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
