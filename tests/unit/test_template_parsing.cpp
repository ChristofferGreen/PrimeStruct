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

std::string parseError(const std::string &source) {
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  return error;
}
} // namespace

TEST_SUITE_BEGIN("primestruct.parser.templates");

TEST_CASE("parses template list on definition") {
  const std::string source = R"(
[return<int>]
identity<T>([T] x) {
  return(x)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  CHECK(program.definitions[0].templateArgs.size() == 1);
  CHECK(program.definitions[0].templateArgs[0] == "T");
  CHECK(program.definitions[0].templateArgIsPack.size() == 1);
  CHECK_FALSE(program.definitions[0].templateArgIsPack[0]);
}

TEST_CASE("parses heterogeneous type pack parameter on definition") {
  const std::string source = R"(
[return<T>]
pack_head<T, Ts...>([T] head) {
  return(head)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  CHECK(program.definitions[0].templateArgs ==
        std::vector<std::string>{"T", "Ts"});
  CHECK(program.definitions[0].templateArgIsPack ==
        std::vector<bool>{false, true});
}

TEST_CASE("parses heterogeneous type pack parameter on struct") {
  const std::string source = R"(
[struct]
Tuple<T, Ts...> {
  [T] head
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  CHECK(program.definitions[0].templateArgs ==
        std::vector<std::string>{"T", "Ts"});
  CHECK(program.definitions[0].templateArgIsPack ==
        std::vector<bool>{false, true});
}

TEST_CASE("parses template list on call") {
  const std::string source = R"(
[return<int>]
identity<T>([T] x) {
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

TEST_CASE("parses nested template arguments on call") {
  const std::string source = R"(
[return<int>]
main() {
  return(use(map<i32, array<i32>>(1i32, array<i32>(2i32))))
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &mainDef = program.definitions[0];
  REQUIRE(mainDef.returnExpr);
  CHECK(mainDef.returnExpr->kind == primec::Expr::Kind::Call);
  REQUIRE(mainDef.returnExpr->args.size() == 1);
  const auto &mapCall = mainDef.returnExpr->args[0];
  CHECK(mapCall.kind == primec::Expr::Kind::Call);
  CHECK(mapCall.templateArgs.size() == 2);
  CHECK(mapCall.templateArgs[0] == "i32");
  CHECK(mapCall.templateArgs[1] == "array<i32>");
}

TEST_CASE("parses nested template argument on return transform") {
  const std::string source = R"(
[return<array<i32>>]
main() {
  return(array<i32>(1i32))
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &def = program.definitions[0];
  REQUIRE(def.transforms.size() == 1);
  REQUIRE(def.transforms[0].templateArgs.size() == 1);
  CHECK(def.transforms[0].templateArgs[0] == "array<i32>");
}

TEST_CASE("rejects invalid heterogeneous type pack declarations") {
  CHECK(parseError(R"(
[return<i32>]
bad<T, T>() {
  return(0i32)
}
)").find("duplicate template parameter: T") != std::string::npos);

  CHECK(parseError(R"(
[return<i32>]
bad<Ts..., Us...>() {
  return(0i32)
}
)").find("only one type pack parameter is supported") != std::string::npos);

  CHECK(parseError(R"(
[return<i32>]
bad<Ts..., T>() {
  return(0i32)
}
)").find("type pack parameter must be last") != std::string::npos);

  CHECK(parseError(R"(
[return<i32>]
bad<args<T>>() {
  return(0i32)
}
)").find("args<T> is variadic value-pack syntax") != std::string::npos);
}

TEST_CASE("rejects type pack expansion outside template parameter list") {
  CHECK(parseError(R"(
[return<i32>]
main() {
  return(use<Ts...>(0i32))
}
)").find("type-pack expansion is only valid in template parameter lists") !=
        std::string::npos);
}

TEST_SUITE_END();
