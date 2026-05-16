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

TEST_CASE("parses heterogeneous type pack field expansion") {
  const std::string source = R"(
[struct]
Tuple<Ts...> {
  [Ts...] values
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].statements.size() == 1);
  const auto &field = program.definitions[0].statements.front();
  REQUIRE(field.isBinding);
  CHECK(field.name == "values");
  REQUIRE(field.transforms.size() == 1);
  CHECK(field.transforms.front().name == "Ts");
  CHECK(field.transforms.front().isPackExpansion);
}

TEST_CASE("parses heterogeneous type pack expansion in type envelopes") {
  const std::string source = R"(
[struct]
Tuple<Ts...> {
  [Ts...] values
}

[return<Tuple<Ts...>>]
identity<Ts...>([Tuple<Ts...>] value) {
  return(value)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 2);
  const auto &helper = program.definitions[1];
  REQUIRE(helper.transforms.size() == 1);
  REQUIRE(helper.transforms.front().templateArgs.size() == 1);
  CHECK(helper.transforms.front().templateArgs.front() == "Tuple<Ts...>");
  REQUIRE(helper.parameters.size() == 1);
  REQUIRE(helper.parameters.front().transforms.size() == 1);
  REQUIRE(helper.parameters.front().transforms.front().templateArgs.size() == 1);
  CHECK(helper.parameters.front().transforms.front().templateArgs.front() == "Ts...");
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

TEST_CASE("parses integer template argument metadata on call and method call") {
  const std::string source = R"(
[return<i32>]
main() {
  [auto] value{wrap<i32>(1i32)}
  [auto] picked{select<0>(value)}
  return(value.get<1>())
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &mainDef = program.definitions[0];
  REQUIRE(mainDef.statements.size() == 3);
  REQUIRE(mainDef.statements[0].args.size() == 1);
  const auto &wrapCall = mainDef.statements[0].args[0];
  REQUIRE(wrapCall.templateArgs.size() == 1);
  CHECK(wrapCall.templateArgs[0] == "i32");
  REQUIRE(wrapCall.templateArgDetails.size() == 1);
  CHECK(wrapCall.templateArgDetails[0].kind == primec::TemplateArgumentKind::Type);
  REQUIRE(mainDef.statements[1].args.size() == 1);
  const auto &selectCall = mainDef.statements[1].args[0];
  REQUIRE(selectCall.templateArgs.size() == 1);
  CHECK(selectCall.templateArgs[0] == "0");
  REQUIRE(selectCall.templateArgDetails.size() == 1);
  CHECK(selectCall.templateArgDetails[0].kind == primec::TemplateArgumentKind::Integer);
  CHECK(selectCall.templateArgDetails[0].integerValue == 0);

  REQUIRE(mainDef.returnExpr);
  const auto &methodCall = *mainDef.returnExpr;
  CHECK(methodCall.isMethodCall);
  CHECK(methodCall.name == "get");
  REQUIRE(methodCall.templateArgs.size() == 1);
  CHECK(methodCall.templateArgs[0] == "1");
  REQUIRE(methodCall.templateArgDetails.size() == 1);
  CHECK(methodCall.templateArgDetails[0].kind == primec::TemplateArgumentKind::Integer);
  CHECK(methodCall.templateArgDetails[0].integerValue == 1);
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

TEST_CASE("rejects invalid non-type template arguments") {
  CHECK(parseError(R"(
[return<i32>]
main() {
  return(get<1.5>(0i32))
}
)").find("template integer arguments must be unsigned integer literals") !=
        std::string::npos);

  CHECK(parseError(R"(
[return<i32>]
main() {
  return(get<-1>(0i32))
}
)").find("negative integer template arguments are not supported") !=
        std::string::npos);

  CHECK(parseError(R"(
[return<i32>]
main() {
  return(get<"field">(0i32))
}
)").find("template arguments do not accept string literals") !=
        std::string::npos);

  CHECK(parseError(R"(
[return<i32>]
main() {
  return(get<true>(0i32))
}
)").find("template arguments do not accept bool literals") !=
        std::string::npos);
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

TEST_CASE("parses type pack expansion in call template arguments") {
  const std::string source = R"(
[return<i32>]
forward<Ts...>() {
  return(use<Ts...>())
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions.front().returnExpr.has_value());
  const auto &useCall = *program.definitions.front().returnExpr;
  REQUIRE(useCall.kind == primec::Expr::Kind::Call);
  REQUIRE(useCall.templateArgs.size() == 1);
  CHECK(useCall.templateArgs.front() == "Ts...");
}

TEST_SUITE_END();
