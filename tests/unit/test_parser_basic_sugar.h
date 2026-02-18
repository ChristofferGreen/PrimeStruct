#pragma once

#include "test_parser_basic_helpers.h"

TEST_CASE("normalizes string literals with double-quoted escapes") {
  const std::string source = R"(
[return<void>]
main() {
  log("he\"llo"utf8)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].statements.size() == 1);
  const auto &stmt = program.definitions[0].statements[0];
  CHECK(stmt.kind == primec::Expr::Kind::Call);
  REQUIRE(stmt.args.size() == 1);
  CHECK(stmt.args[0].kind == primec::Expr::Kind::StringLiteral);
  CHECK(stmt.args[0].stringValue == std::string("\"he\\\"llo\"utf8"));
}

TEST_CASE("parses ascii string literal arguments") {
  const std::string source = R"(
[return<void>]
main() {
  log("hello"ascii)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].statements.size() == 1);
  const auto &stmt = program.definitions[0].statements[0];
  CHECK(stmt.kind == primec::Expr::Kind::Call);
  REQUIRE(stmt.args.size() == 1);
  CHECK(stmt.args[0].kind == primec::Expr::Kind::StringLiteral);
  CHECK(stmt.args[0].stringValue == "\"hello\"ascii");
}

TEST_CASE("parses raw string literal arguments") {
  const std::string source =
      "[return<void>]\n"
      "main() {\n"
      "  log(\"hello world\"raw_utf8)\n"
      "}\n";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].statements.size() == 1);
  const auto &stmt = program.definitions[0].statements[0];
  CHECK(stmt.kind == primec::Expr::Kind::Call);
  REQUIRE(stmt.args.size() == 1);
  CHECK(stmt.args[0].kind == primec::Expr::Kind::StringLiteral);
  CHECK(stmt.args[0].stringValue == "\"hello world\"utf8");
}

TEST_CASE("parses raw string literal escapes") {
  const std::string source = R"(
[return<void>]
main() {
  log("hello\q"raw_utf8)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].statements.size() == 1);
  const auto &stmt = program.definitions[0].statements[0];
  CHECK(stmt.kind == primec::Expr::Kind::Call);
  REQUIRE(stmt.args.size() == 1);
  CHECK(stmt.args[0].kind == primec::Expr::Kind::StringLiteral);
  CHECK(stmt.args[0].stringValue == "\"hello\\\\q\"utf8");
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
  [i32 mut] value{1i32}
  if(true) {
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

TEST_CASE("parses if sugar in return argument") {
  const std::string source = R"(
[return<int>]
main() {
  return(if(true) { 4i32 } else { 9i32 })
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].returnExpr.has_value());
  const auto &expr = *program.definitions[0].returnExpr;
  CHECK(expr.kind == primec::Expr::Kind::Call);
  CHECK(expr.name == "if");
  REQUIRE(expr.args.size() == 3);
  CHECK(expr.args[1].name == "then");
  CHECK(expr.args[2].name == "else");
  REQUIRE(expr.args[1].bodyArguments.size() == 1);
  REQUIRE(expr.args[2].bodyArguments.size() == 1);
  CHECK(expr.args[1].bodyArguments[0].kind == primec::Expr::Kind::Literal);
  CHECK(expr.args[2].bodyArguments[0].kind == primec::Expr::Kind::Literal);
}

TEST_CASE("parses block expression with parens") {
  const std::string source = R"(
[return<int>]
main() {
  return(block() { 1i32 })
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].returnExpr.has_value());
  const auto &expr = *program.definitions[0].returnExpr;
  CHECK(expr.kind == primec::Expr::Kind::Call);
  CHECK(expr.name == "block");
  CHECK(expr.args.empty());
  REQUIRE(expr.bodyArguments.size() == 1);
  CHECK(expr.bodyArguments[0].kind == primec::Expr::Kind::Literal);
}

TEST_CASE("parses block expression with mixed separators") {
  const std::string source = R"(
[return<int>]
main() {
  return(block() { 1i32, 2i32 3i32 })
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].returnExpr.has_value());
  const auto &expr = *program.definitions[0].returnExpr;
  CHECK(expr.kind == primec::Expr::Kind::Call);
  CHECK(expr.name == "block");
  CHECK(expr.args.empty());
  REQUIRE(expr.bodyArguments.size() == 3);
}

TEST_CASE("parses return inside block expression list") {
  const std::string source = R"(
[return<int>]
main() {
  return(block() { return(1i32) 2i32 })
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].returnExpr.has_value());
  const auto &expr = *program.definitions[0].returnExpr;
  CHECK(expr.kind == primec::Expr::Kind::Call);
  CHECK(expr.name == "block");
  REQUIRE(expr.bodyArguments.size() == 1);
  CHECK(expr.bodyArguments[0].kind == primec::Expr::Kind::Literal);
}

TEST_CASE("parses if sugar with block statements in return argument") {
  const std::string source = R"(
[return<int>]
main() {
  return(if(true) { [i32] x{4i32} plus(x, 1i32) } else { 0i32 })
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].returnExpr.has_value());
  const auto &expr = *program.definitions[0].returnExpr;
  CHECK(expr.kind == primec::Expr::Kind::Call);
  CHECK(expr.name == "if");
  REQUIRE(expr.args.size() == 3);
  REQUIRE(expr.args[1].bodyArguments.size() == 2);
  CHECK(expr.args[1].bodyArguments[0].isBinding);
  CHECK(expr.args[1].bodyArguments[0].name == "x");
  CHECK(expr.args[1].bodyArguments[1].kind == primec::Expr::Kind::Call);
}

TEST_CASE("parses if sugar inside block with mixed separators") {
  const std::string source = R"(
[return<int>]
main() {
  return(block() { if(true) { 1i32 } else { 2i32 }, 3i32 4i32 })
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].returnExpr.has_value());
  const auto &expr = *program.definitions[0].returnExpr;
  CHECK(expr.kind == primec::Expr::Kind::Call);
  CHECK(expr.name == "block");
  REQUIRE(expr.bodyArguments.size() == 3);
  CHECK(expr.bodyArguments[0].kind == primec::Expr::Kind::Call);
  CHECK(expr.bodyArguments[0].name == "if");
 }

TEST_CASE("ignores line comments") {
  const std::string source = R"(
// header
[return<int>]
main() {
  // inside body
  return(1i32) // trailing
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].returnExpr.has_value());
  const auto &expr = *program.definitions[0].returnExpr;
  CHECK(expr.kind == primec::Expr::Kind::Literal);
}

TEST_CASE("ignores block comments") {
  const std::string source = R"(
[return<int>]
main() {
  return(plus(1i32, /* comment */ 2i32))
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].returnExpr.has_value());
  const auto &expr = *program.definitions[0].returnExpr;
  CHECK(expr.kind == primec::Expr::Kind::Call);
  CHECK(expr.name == "plus");
  REQUIRE(expr.args.size() == 2);
}

TEST_CASE("parses call with body arguments in expression") {
  const std::string source = R"(
[return<int>]
main() {
  return(task() { step() })
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
