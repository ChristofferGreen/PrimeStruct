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

TEST_SUITE_BEGIN("primestruct.parser.templates");

TEST_CASE("parses template list on definition") {
  const std::string source = R"(
[return<int>]
identity<T>(x) {
  return(x)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  CHECK(program.definitions[0].templateArgs.size() == 1);
  CHECK(program.definitions[0].templateArgs[0] == "T");
}

TEST_CASE("parses template list on call") {
  const std::string source = R"(
[return<int>]
identity<T>(x) {
  return(x)
}

[return<int>]
main() {
  return(identity<int>(3i32))
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 2);
  const auto &mainDef = program.definitions[1];
  REQUIRE(mainDef.returnExpr);
  CHECK(mainDef.returnExpr->kind == primec::Expr::Kind::Call);
  CHECK(mainDef.returnExpr->templateArgs.size() == 1);
  CHECK(mainDef.returnExpr->templateArgs[0] == "int");
}

TEST_SUITE_END();
