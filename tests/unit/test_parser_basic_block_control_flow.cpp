#include "test_parser_basic_helpers.h"

TEST_CASE("parses if with block arguments") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{1i32}
  if(true, then(){
    [i32] temp{2i32}
    assign(value, temp)
  }, else(){ assign(value, 3i32) })
  return(value)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].statements.size() == 3);
  const auto &stmt = program.definitions[0].statements[1];
  CHECK(stmt.kind == primec::Expr::Kind::Call);
  CHECK(stmt.name == "if");
  REQUIRE(stmt.args.size() == 3);
  CHECK(stmt.args[1].name == "then");
  CHECK(stmt.args[2].name == "else");
  CHECK(stmt.args[1].bodyArguments.size() == 2);
  CHECK(stmt.args[2].bodyArguments.size() == 1);
  CHECK(stmt.args[1].bodyArguments[0].isBinding);
}

TEST_CASE("parses single-branch if statement sugar") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{1i32}
  if(true) {
    assign(value, 2i32)
  }
  return(value)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].statements.size() == 3);
  const auto &stmt = program.definitions[0].statements[1];
  CHECK(stmt.kind == primec::Expr::Kind::Call);
  CHECK(stmt.name == "if");
  REQUIRE(stmt.args.size() == 3);
  CHECK(stmt.args[1].name == "then");
  CHECK(stmt.args[2].name == "else");
  CHECK(stmt.args[1].hasBodyArguments);
  CHECK(stmt.args[2].hasBodyArguments);
  REQUIRE(stmt.args[1].bodyArguments.size() == 1);
  CHECK(stmt.args[2].bodyArguments.empty());
}

TEST_CASE("parses if blocks with return statements") {
  const std::string source = R"(
[return<int>]
main() {
  if(true, then(){ return(1i32) }, else(){ return(2i32) })
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].statements.size() == 1);
  const auto &stmt = program.definitions[0].statements[0];
  CHECK(stmt.kind == primec::Expr::Kind::Call);
  CHECK(stmt.name == "if");
  REQUIRE(stmt.args.size() == 3);
  REQUIRE(stmt.args[1].bodyArguments.size() == 1);
  REQUIRE(stmt.args[2].bodyArguments.size() == 1);
  CHECK(stmt.args[1].bodyArguments[0].name == "return");
  CHECK(stmt.args[2].bodyArguments[0].name == "return");
}
