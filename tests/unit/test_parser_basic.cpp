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

TEST_SUITE_BEGIN("primestruct.parser.basic");

TEST_CASE("parses minimal main definition") {
  const std::string source = R"(
[return<int>]
main() {
  return(7i32)
}
)";

  const auto program = parseProgram(source);
  CHECK(program.definitions.size() == 1);
  CHECK(program.definitions[0].fullPath == "/main");
}

TEST_CASE("parses definition without return transform") {
  const std::string source = R"(
main() {
  return(7i32)
}
)";

  const auto program = parseProgram(source);
  CHECK(program.definitions.size() == 1);
  CHECK(program.definitions[0].fullPath == "/main");
}

TEST_CASE("parses void return without transform") {
  const std::string source = R"(
main() {
  return()
}
)";

  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  CHECK(program.definitions[0].fullPath == "/main");
  CHECK(program.definitions[0].hasReturnStatement);
  CHECK_FALSE(program.definitions[0].returnExpr.has_value());
}

TEST_CASE("parses slash path definition") {
  const std::string source = R"(
[return<int>]
/demo/widget() {
  return(1i32)
}
)";

  const auto program = parseProgram(source);
  CHECK(program.definitions.size() == 1);
  CHECK(program.definitions[0].fullPath == "/demo/widget");
}

TEST_CASE("parses execution with arguments and body") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

execute_repeat(3i32) { main() }
)";
  const auto program = parseProgram(source);
  CHECK(program.executions.size() == 1);
  CHECK(program.executions[0].fullPath == "/execute_repeat");
  CHECK(program.executions[0].arguments.size() == 1);
  CHECK(program.executions[0].bodyArguments.size() == 1);
}

TEST_CASE("parses local binding statements") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value(7i32)
  return(value)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].statements.size() == 2);
  const auto &stmt = program.definitions[0].statements[0];
  CHECK(stmt.isBinding);
  CHECK(stmt.name == "value");
  REQUIRE(stmt.transforms.size() == 1);
  CHECK(stmt.transforms[0].name == "i32");
}

TEST_CASE("parses literal statement") {
  const std::string source = R"(
[return<int>]
main() {
  1i32
  return(2i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].statements.size() == 2);
  CHECK(program.definitions[0].statements[0].kind == primec::Expr::Kind::Literal);
}

TEST_CASE("parses struct definition without return") {
  const std::string source = R"(
[struct]
data() {
  [i32] value(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  CHECK(program.definitions[0].fullPath == "/data");
  CHECK(program.definitions[0].hasReturnStatement == false);
  CHECK(program.definitions[0].statements.size() == 1);
  CHECK(program.definitions[0].statements[0].isBinding);
}

TEST_CASE("parses pod definition without return") {
  const std::string source = R"(
[pod]
data() {
  [i32] value(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  CHECK(program.definitions[0].fullPath == "/data");
  CHECK(program.definitions[0].hasReturnStatement == false);
  CHECK(program.definitions[0].statements.size() == 1);
  CHECK(program.definitions[0].statements[0].isBinding);
}

TEST_CASE("parses if with block arguments") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value(1i32)
  if(1i32, then{
    [i32] temp(2i32)
    assign(value, temp)
  }, else{ assign(value, 3i32) })
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

TEST_CASE("parses statement call with block arguments") {
  const std::string source = R"(
[return<int>]
main() {
  execute_repeat(3i32) {
    [i32] temp(1i32)
    assign(temp, 2i32)
  }
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].statements.size() == 2);
  const auto &stmt = program.definitions[0].statements[0];
  CHECK(stmt.kind == primec::Expr::Kind::Call);
  CHECK(stmt.name == "execute_repeat");
  CHECK(stmt.bodyArguments.size() == 2);
  CHECK(stmt.bodyArguments[0].isBinding);
}

TEST_CASE("parses boolean literals") {
  const std::string source = R"(
[return<int>]
main() {
  return(true)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].returnExpr.has_value());
  CHECK(program.definitions[0].returnExpr->kind == primec::Expr::Kind::BoolLiteral);
  CHECK(program.definitions[0].returnExpr->boolValue);
}

TEST_CASE("parses hex integer literals") {
  const std::string source = R"(
[return<int>]
main() {
  return(0x2Ai32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].returnExpr.has_value());
  CHECK(program.definitions[0].returnExpr->kind == primec::Expr::Kind::Literal);
  CHECK(program.definitions[0].returnExpr->literalValue == 42);
}

TEST_CASE("parses i64 and u64 integer literals") {
  const std::string source = R"(
[return<int>]
main() {
  return(9i64)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].returnExpr.has_value());
  CHECK(program.definitions[0].returnExpr->kind == primec::Expr::Kind::Literal);
  CHECK(program.definitions[0].returnExpr->intWidth == 64);
  CHECK_FALSE(program.definitions[0].returnExpr->isUnsigned);
  CHECK(program.definitions[0].returnExpr->literalValue == 9);

  const std::string sourceUnsigned = R"(
[return<int>]
main() {
  return(10u64)
}
)";
  const auto programUnsigned = parseProgram(sourceUnsigned);
  REQUIRE(programUnsigned.definitions.size() == 1);
  REQUIRE(programUnsigned.definitions[0].returnExpr.has_value());
  CHECK(programUnsigned.definitions[0].returnExpr->kind == primec::Expr::Kind::Literal);
  CHECK(programUnsigned.definitions[0].returnExpr->intWidth == 64);
  CHECK(programUnsigned.definitions[0].returnExpr->isUnsigned);
  CHECK(programUnsigned.definitions[0].returnExpr->literalValue == 10);
}

TEST_CASE("parses float literals") {
  const std::string source = R"(
[return<float>]
main() {
  return(1.25f)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].returnExpr.has_value());
  CHECK(program.definitions[0].returnExpr->kind == primec::Expr::Kind::FloatLiteral);
  CHECK(program.definitions[0].returnExpr->floatValue == "1.25");
  CHECK(program.definitions[0].returnExpr->floatWidth == 32);
}

TEST_CASE("parses transform arguments") {
  const std::string source = R"(
[effects(global_write, io_out), align_bytes(16), return<int>]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 3);
  CHECK(transforms[0].name == "effects");
  REQUIRE(transforms[0].arguments.size() == 2);
  CHECK(transforms[0].arguments[0] == "global_write");
  CHECK(transforms[0].arguments[1] == "io_out");
  CHECK(transforms[1].name == "align_bytes");
  REQUIRE(transforms[1].arguments.size() == 1);
  CHECK(transforms[1].arguments[0] == "16");
  CHECK(transforms[2].name == "return");
  REQUIRE(transforms[2].templateArg.has_value());
  CHECK(*transforms[2].templateArg == "int");
}

TEST_CASE("parses transform string arguments") {
  const std::string source = R"(
[doc("hello world"utf8), return<int>]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 2);
  CHECK(transforms[0].name == "doc");
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] == "\"hello world\"utf8");
  CHECK(transforms[1].name == "return");
}

TEST_CASE("parses named call arguments") {
  const std::string source = R"(
[return<int>]
main() {
  return(make_color(hue = 1i32, value = 2i32))
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].returnExpr.has_value());
  const auto &call = *program.definitions[0].returnExpr;
  REQUIRE(call.kind == primec::Expr::Kind::Call);
  REQUIRE(call.args.size() == 2);
  REQUIRE(call.argNames.size() == 2);
  CHECK(call.argNames[0].has_value());
  CHECK(call.argNames[1].has_value());
  CHECK(*call.argNames[0] == "hue");
  CHECK(*call.argNames[1] == "value");
}

TEST_CASE("parses float literals without suffix") {
  const std::string source = R"(
[return<float>]
main() {
  return(2.5)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].returnExpr.has_value());
  CHECK(program.definitions[0].returnExpr->kind == primec::Expr::Kind::FloatLiteral);
  CHECK(program.definitions[0].returnExpr->floatValue == "2.5");
  CHECK(program.definitions[0].returnExpr->floatWidth == 32);
}

TEST_CASE("parses float literals with exponent") {
  const std::string source = R"(
[return<float>]
main() {
  return(1e3)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].returnExpr.has_value());
  CHECK(program.definitions[0].returnExpr->kind == primec::Expr::Kind::FloatLiteral);
  CHECK(program.definitions[0].returnExpr->floatValue == "1e3");
  CHECK(program.definitions[0].returnExpr->floatWidth == 32);
}

TEST_CASE("parses float literals with exponent and sign") {
  const std::string source = R"(
[return<float>]
main() {
  return(1e-3f)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].returnExpr.has_value());
  CHECK(program.definitions[0].returnExpr->kind == primec::Expr::Kind::FloatLiteral);
  CHECK(program.definitions[0].returnExpr->floatValue == "1e-3");
  CHECK(program.definitions[0].returnExpr->floatWidth == 32);
}

TEST_CASE("parses double literals") {
  const std::string source = R"(
[return<f64>]
main() {
  return(3.0f64)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].returnExpr.has_value());
  CHECK(program.definitions[0].returnExpr->kind == primec::Expr::Kind::FloatLiteral);
  CHECK(program.definitions[0].returnExpr->floatValue == "3.0");
  CHECK(program.definitions[0].returnExpr->floatWidth == 64);
}

TEST_CASE("parses string literal arguments") {
  const std::string source = R"(
[return<void>]
main() {
  log("hello"utf8)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].statements.size() == 1);
  const auto &stmt = program.definitions[0].statements[0];
  CHECK(stmt.kind == primec::Expr::Kind::Call);
  REQUIRE(stmt.args.size() == 1);
  CHECK(stmt.args[0].kind == primec::Expr::Kind::StringLiteral);
  CHECK(stmt.args[0].stringValue == "\"hello\"utf8");
}

TEST_CASE("parses raw string literal arguments") {
  const std::string source =
      "[return<void>]\n"
      "main() {\n"
      "  log(R\"(hello world)\"utf8)\n"
      "}\n";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].statements.size() == 1);
  const auto &stmt = program.definitions[0].statements[0];
  CHECK(stmt.kind == primec::Expr::Kind::Call);
  REQUIRE(stmt.args.size() == 1);
  CHECK(stmt.args[0].kind == primec::Expr::Kind::StringLiteral);
  CHECK(stmt.args[0].stringValue == "R\"(hello world)\"utf8");
}

TEST_CASE("parses method call sugar") {
  const std::string source = R"(
[return<int>]
main() {
  return(items.count())
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].returnExpr.has_value());
  const auto &expr = *program.definitions[0].returnExpr;
  CHECK(expr.kind == primec::Expr::Kind::Call);
  CHECK(expr.isMethodCall);
  CHECK(expr.name == "count");
  REQUIRE(expr.args.size() == 1);
  CHECK(expr.args[0].kind == primec::Expr::Kind::Name);
  CHECK(expr.args[0].name == "items");
}

TEST_CASE("parses index sugar") {
  const std::string source = R"(
[return<int>]
main() {
  return(items[1i32])
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].returnExpr.has_value());
  const auto &expr = *program.definitions[0].returnExpr;
  CHECK(expr.kind == primec::Expr::Kind::Call);
  CHECK(expr.name == "at");
  REQUIRE(expr.args.size() == 2);
  CHECK(expr.args[0].kind == primec::Expr::Kind::Name);
  CHECK(expr.args[0].name == "items");
  CHECK(expr.args[1].kind == primec::Expr::Kind::Literal);
}

TEST_CASE("parses if statement sugar") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value(1i32)
  if(1i32) {
    assign(value, 2i32)
  } else {
    assign(value, 3i32)
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
  CHECK(stmt.args[1].bodyArguments.size() == 1);
  CHECK(stmt.args[2].bodyArguments.size() == 1);
}

TEST_CASE("parses call with body arguments in expression") {
  const std::string source = R"(
[return<int>]
main() {
  return(task { step() })
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].returnExpr.has_value());
  const auto &expr = *program.definitions[0].returnExpr;
  CHECK(expr.kind == primec::Expr::Kind::Call);
  CHECK(expr.name == "task");
  CHECK(expr.args.empty());
  REQUIRE(expr.bodyArguments.size() == 1);
  CHECK(expr.bodyArguments[0].name == "step");
}

TEST_SUITE_END();
