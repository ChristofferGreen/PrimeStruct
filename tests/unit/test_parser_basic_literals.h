#pragma once

#include "test_parser_basic_helpers.h"

TEST_CASE("parses statement call with block arguments") {
  const std::string source = R"(
[return<int>]
main() {
  execute_repeat(3i32) {
    [i32] temp{1i32}
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

TEST_CASE("parses false boolean literal") {
  const std::string source = R"(
[return<bool>]
main() {
  return(false)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].returnExpr.has_value());
  CHECK(program.definitions[0].returnExpr->kind == primec::Expr::Kind::BoolLiteral);
  CHECK_FALSE(program.definitions[0].returnExpr->boolValue);
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

TEST_CASE("parses unsigned hex integer literals") {
  const std::string source = R"(
[return<int>]
main() {
  return(0xFFu64)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].returnExpr.has_value());
  CHECK(program.definitions[0].returnExpr->kind == primec::Expr::Kind::Literal);
  CHECK(program.definitions[0].returnExpr->intWidth == 64);
  CHECK(program.definitions[0].returnExpr->isUnsigned);
  CHECK(program.definitions[0].returnExpr->literalValue == 255);
}

TEST_CASE("parses uppercase hex integer literals") {
  const std::string source = R"(
[return<int>]
main() {
  return(0X2Ai32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].returnExpr.has_value());
  CHECK(program.definitions[0].returnExpr->kind == primec::Expr::Kind::Literal);
  CHECK(program.definitions[0].returnExpr->intWidth == 32);
  CHECK_FALSE(program.definitions[0].returnExpr->isUnsigned);
  CHECK(program.definitions[0].returnExpr->literalValue == 42);
}

TEST_CASE("parses integer literals with comma separators") {
  const std::string source = R"(
[return<int>]
main() {
  return(1,000i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].returnExpr.has_value());
  CHECK(program.definitions[0].returnExpr->kind == primec::Expr::Kind::Literal);
  CHECK(program.definitions[0].returnExpr->literalValue == 1000);
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

TEST_CASE("rejects u32 integer literals") {
  const std::string source = R"(
[return<int>]
main() {
  return(7u32)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("integer literal requires i32/i64/u64 suffix") != std::string::npos);
}

TEST_CASE("rejects integer literals without suffix") {
  const std::string source = R"(
[return<int>]
main() {
  return(123)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("integer literal requires i32/i64/u64 suffix") != std::string::npos);
}

TEST_CASE("rejects integer literals with unsupported suffix") {
  const std::string source = R"(
[return<int>]
main() {
  return(11i16)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("integer literal requires i32/i64/u64 suffix") != std::string::npos);
}

TEST_CASE("rejects negative unsigned integer literals") {
  const std::string source = R"(
[return<int>]
main() {
  return(-1u64)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("invalid integer literal") != std::string::npos);
}

TEST_CASE("rejects i32 integer literals out of range") {
  const std::string source = R"(
[return<int>]
main() {
  return(2147483648i32)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("integer literal out of range") != std::string::npos);
}

TEST_CASE("rejects i32 integer literals below minimum") {
  const std::string source = R"(
[return<int>]
main() {
  return(-2147483649i32)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("integer literal out of range") != std::string::npos);
}

TEST_CASE("rejects i64 integer literals below minimum") {
  const std::string source = R"(
[return<int>]
main() {
  return(-9223372036854775809i64)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("integer literal out of range") != std::string::npos);
}

TEST_CASE("rejects i64 integer literals above maximum") {
  const std::string source = R"(
[return<int>]
main() {
  return(9223372036854775808i64)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("invalid integer literal") != std::string::npos);
}

TEST_CASE("rejects malformed numeric tokens in expression parser") {
  auto expectInvalidIntegerLiteral = [](const std::string &numberText) {
    std::vector<primec::Token> tokens = {
        {primec::TokenKind::Number, numberText, 1, 1},
        {primec::TokenKind::End, "", 1, 1},
    };
    primec::Parser parser(std::move(tokens));
    primec::Expr expr;
    std::string error;
    CHECK_FALSE(parser.parseExpression(expr, "", error));
    CHECK(error.find("invalid integer literal") != std::string::npos);
  };

  expectInvalidIntegerLiteral("i32");
  expectInvalidIntegerLiteral("-i32");
  expectInvalidIntegerLiteral("0xZi32");
  expectInvalidIntegerLiteral("12a3i32");
}

TEST_CASE("expression parser rejects trailing tokens") {
  std::vector<primec::Token> tokens = {
      {primec::TokenKind::Number, "1i32", 1, 1},
      {primec::TokenKind::Identifier, "trailing", 1, 6},
      {primec::TokenKind::End, "", 1, 14},
  };
  primec::Parser parser(std::move(tokens));
  primec::Expr expr;
  std::string error;
  CHECK_FALSE(parser.parseExpression(expr, "", error));
  CHECK(error.find("unexpected token after expression") != std::string::npos);
}

TEST_CASE("expression parser rejects malformed string token in canonical mode") {
  std::vector<primec::Token> tokens = {
      {primec::TokenKind::String, "not_a_string_literal", 1, 1},
      {primec::TokenKind::End, "", 1, 21},
  };
  primec::Parser parser(std::move(tokens), false);
  primec::Expr expr;
  std::string error;
  CHECK_FALSE(parser.parseExpression(expr, "", error));
  CHECK(error.find("invalid string literal") != std::string::npos);
}

TEST_CASE("expression parser rejects indexing without closing bracket") {
  std::vector<primec::Token> tokens = {
      {primec::TokenKind::Number, "1i32", 1, 1},
      {primec::TokenKind::LBracket, "[", 1, 5},
      {primec::TokenKind::Number, "0i32", 1, 6},
      {primec::TokenKind::End, "", 1, 10},
  };
  primec::Parser parser(std::move(tokens));
  primec::Expr expr;
  std::string error;
  CHECK_FALSE(parser.parseExpression(expr, "", error));
  CHECK(error.find("expected ']' after index expression") != std::string::npos);
}

TEST_CASE("expression parser rejects if keyword without call syntax") {
  std::vector<primec::Token> tokens = {
      {primec::TokenKind::Identifier, "if", 1, 1},
      {primec::TokenKind::Identifier, "cond", 1, 4},
      {primec::TokenKind::End, "", 1, 8},
  };
  primec::Parser parser(std::move(tokens));
  primec::Expr expr;
  std::string error;
  CHECK_FALSE(parser.parseExpression(expr, "", error));
  CHECK(error.find("reserved keyword cannot be used as identifier") != std::string::npos);
}

TEST_CASE("expression parser rejects match keyword without call syntax") {
  std::vector<primec::Token> tokens = {
      {primec::TokenKind::Identifier, "match", 1, 1},
      {primec::TokenKind::Identifier, "cond", 1, 7},
      {primec::TokenKind::End, "", 1, 11},
  };
  primec::Parser parser(std::move(tokens));
  primec::Expr expr;
  std::string error;
  CHECK_FALSE(parser.parseExpression(expr, "", error));
  CHECK(error.find("reserved keyword cannot be used as identifier") != std::string::npos);
}

TEST_CASE("expression parser reports missing close paren after if fallback") {
  std::vector<primec::Token> tokens = {
      {primec::TokenKind::Identifier, "if", 1, 1},
      {primec::TokenKind::LParen, "(", 1, 3},
      {primec::TokenKind::Identifier, "true", 1, 4},
      {primec::TokenKind::End, "", 1, 8},
  };
  primec::Parser parser(std::move(tokens));
  primec::Expr expr;
  std::string error;
  CHECK_FALSE(parser.parseExpression(expr, "", error));
  CHECK(error.find("unexpected token in expression") != std::string::npos);
}

TEST_CASE("expression parser reports missing close paren after match fallback") {
  std::vector<primec::Token> tokens = {
      {primec::TokenKind::Identifier, "match", 1, 1},
      {primec::TokenKind::LParen, "(", 1, 6},
      {primec::TokenKind::Identifier, "true", 1, 7},
      {primec::TokenKind::End, "", 1, 11},
  };
  primec::Parser parser(std::move(tokens));
  primec::Expr expr;
  std::string error;
  CHECK_FALSE(parser.parseExpression(expr, "", error));
  CHECK(error.find("unexpected token in expression") != std::string::npos);
}

TEST_CASE("expression parser treats match call without body as regular call") {
  std::vector<primec::Token> tokens = {
      {primec::TokenKind::Identifier, "match", 1, 1},
      {primec::TokenKind::LParen, "(", 1, 6},
      {primec::TokenKind::Identifier, "true", 1, 7},
      {primec::TokenKind::RParen, ")", 1, 11},
      {primec::TokenKind::End, "", 1, 12},
  };
  primec::Parser parser(std::move(tokens));
  primec::Expr expr;
  std::string error;
  CHECK(parser.parseExpression(expr, "", error));
  CHECK(error.empty());
  CHECK(expr.kind == primec::Expr::Kind::Call);
  CHECK(expr.name == "match");
  REQUIRE(expr.args.size() == 1);
  CHECK(expr.args[0].kind == primec::Expr::Kind::BoolLiteral);
  CHECK(expr.args[0].boolValue);
}

TEST_CASE("expression parser treats if call without body as regular call") {
  std::vector<primec::Token> tokens = {
      {primec::TokenKind::Identifier, "if", 1, 1},
      {primec::TokenKind::LParen, "(", 1, 3},
      {primec::TokenKind::Identifier, "true", 1, 4},
      {primec::TokenKind::RParen, ")", 1, 8},
      {primec::TokenKind::End, "", 1, 9},
  };
  primec::Parser parser(std::move(tokens));
  primec::Expr expr;
  std::string error;
  CHECK(parser.parseExpression(expr, "", error));
  CHECK(error.empty());
  CHECK(expr.kind == primec::Expr::Kind::Call);
  CHECK(expr.name == "if");
  REQUIRE(expr.args.size() == 1);
  CHECK(expr.args[0].kind == primec::Expr::Kind::BoolLiteral);
  CHECK(expr.args[0].boolValue);
}

TEST_CASE("expression parser rejects match template arguments without call") {
  std::vector<primec::Token> tokens = {
      {primec::TokenKind::Identifier, "match", 1, 1},
      {primec::TokenKind::LAngle, "<", 1, 6},
      {primec::TokenKind::Identifier, "i32", 1, 7},
      {primec::TokenKind::RAngle, ">", 1, 10},
      {primec::TokenKind::End, "", 1, 11},
  };
  primec::Parser parser(std::move(tokens));
  primec::Expr expr;
  std::string error;
  CHECK_FALSE(parser.parseExpression(expr, "", error));
  CHECK(error.find("template arguments require a call") != std::string::npos);
}

TEST_CASE("expression parser rejects if template arguments without call") {
  std::vector<primec::Token> tokens = {
      {primec::TokenKind::Identifier, "if", 1, 1},
      {primec::TokenKind::LAngle, "<", 1, 3},
      {primec::TokenKind::Identifier, "i32", 1, 4},
      {primec::TokenKind::RAngle, ">", 1, 7},
      {primec::TokenKind::End, "", 1, 8},
  };
  primec::Parser parser(std::move(tokens));
  primec::Expr expr;
  std::string error;
  CHECK_FALSE(parser.parseExpression(expr, "", error));
  CHECK(error.find("template arguments require a call") != std::string::npos);
}

TEST_CASE("expression parser rejects member call without closing paren") {
  std::vector<primec::Token> tokens = {
      {primec::TokenKind::Identifier, "obj", 1, 1},
      {primec::TokenKind::Dot, ".", 1, 4},
      {primec::TokenKind::Identifier, "method", 1, 5},
      {primec::TokenKind::LParen, "(", 1, 11},
      {primec::TokenKind::Number, "1i32", 1, 12},
      {primec::TokenKind::End, "", 1, 16},
  };
  primec::Parser parser(std::move(tokens));
  primec::Expr expr;
  std::string error;
  CHECK_FALSE(parser.parseExpression(expr, "", error));
  CHECK(error.find("unexpected token in expression") != std::string::npos);
}

TEST_CASE("expression parser treats bare call body as brace constructor") {
  std::vector<primec::Token> tokens = {
      {primec::TokenKind::Identifier, "foo", 1, 1},
      {primec::TokenKind::LBrace, "{", 1, 4},
      {primec::TokenKind::RBrace, "}", 1, 5},
      {primec::TokenKind::End, "", 1, 6},
  };
  primec::Parser parser(std::move(tokens));
  primec::Expr expr;
  std::string error;
  CHECK(parser.parseExpression(expr, "", error));
  CHECK(error.empty());
  REQUIRE(expr.kind == primec::Expr::Kind::Call);
  CHECK(expr.name == "foo");
  REQUIRE(expr.args.size() == 1);
  REQUIRE(expr.argNames.size() == 1);
  CHECK_FALSE(expr.argNames[0].has_value());
  const auto &block = expr.args[0];
  CHECK(block.kind == primec::Expr::Kind::Call);
  CHECK(block.name == "block");
  CHECK(block.hasBodyArguments);
  CHECK(block.bodyArguments.empty());
}

TEST_CASE("expression parser preserves brace constructor body expressions") {
  std::vector<primec::Token> tokens = {
      {primec::TokenKind::Identifier, "foo", 1, 1},
      {primec::TokenKind::LBrace, "{", 1, 4},
      {primec::TokenKind::Number, "1i32", 1, 5},
      {primec::TokenKind::RBrace, "}", 1, 9},
      {primec::TokenKind::End, "", 1, 10},
  };
  primec::Parser parser(std::move(tokens));
  primec::Expr expr;
  std::string error;
  CHECK(parser.parseExpression(expr, "", error));
  CHECK(error.empty());
  REQUIRE(expr.kind == primec::Expr::Kind::Call);
  CHECK(expr.name == "foo");
  REQUIRE(expr.args.size() == 1);
  const auto &block = expr.args[0];
  CHECK(block.kind == primec::Expr::Kind::Call);
  CHECK(block.name == "block");
  CHECK(block.hasBodyArguments);
  REQUIRE(block.bodyArguments.size() == 1);
  CHECK(block.bodyArguments[0].kind == primec::Expr::Kind::Literal);
}

TEST_CASE("expression parser rewrites primitive brace constructor to convert") {
  std::vector<primec::Token> tokens = {
      {primec::TokenKind::Identifier, "i32", 1, 1},
      {primec::TokenKind::LBrace, "{", 1, 4},
      {primec::TokenKind::Number, "1i32", 1, 5},
      {primec::TokenKind::RBrace, "}", 1, 9},
      {primec::TokenKind::End, "", 1, 10},
  };
  primec::Parser parser(std::move(tokens));
  primec::Expr expr;
  std::string error;
  CHECK(parser.parseExpression(expr, "", error));
  CHECK(error.empty());
  REQUIRE(expr.kind == primec::Expr::Kind::Call);
  CHECK(expr.name == "convert");
  REQUIRE(expr.templateArgs.size() == 1);
  CHECK(expr.templateArgs[0] == "i32");
  REQUIRE(expr.args.size() == 1);
  const auto &block = expr.args[0];
  CHECK(block.kind == primec::Expr::Kind::Call);
  CHECK(block.name == "block");
  CHECK(block.hasBodyArguments);
  REQUIRE(block.bodyArguments.size() == 1);
  CHECK(block.bodyArguments[0].kind == primec::Expr::Kind::Literal);
}

TEST_CASE("expression parser rewrites empty primitive brace constructor to convert") {
  std::vector<primec::Token> tokens = {
      {primec::TokenKind::Identifier, "i32", 1, 1},
      {primec::TokenKind::LBrace, "{", 1, 4},
      {primec::TokenKind::RBrace, "}", 1, 5},
      {primec::TokenKind::End, "", 1, 6},
  };
  primec::Parser parser(std::move(tokens));
  primec::Expr expr;
  std::string error;
  CHECK(parser.parseExpression(expr, "", error));
  CHECK(error.empty());
  REQUIRE(expr.kind == primec::Expr::Kind::Call);
  CHECK(expr.name == "convert");
  REQUIRE(expr.templateArgs.size() == 1);
  CHECK(expr.templateArgs[0] == "i32");
  REQUIRE(expr.args.size() == 1);
  const auto &block = expr.args[0];
  CHECK(block.kind == primec::Expr::Kind::Call);
  CHECK(block.name == "block");
  CHECK(block.hasBodyArguments);
  CHECK(block.bodyArguments.empty());
}

TEST_CASE("expression parser normalizes return inside brace constructor body") {
  std::vector<primec::Token> tokens = {
      {primec::TokenKind::Identifier, "foo", 1, 1},
      {primec::TokenKind::LBrace, "{", 1, 4},
      {primec::TokenKind::Identifier, "return", 1, 5},
      {primec::TokenKind::LParen, "(", 1, 11},
      {primec::TokenKind::Number, "1i32", 1, 12},
      {primec::TokenKind::RParen, ")", 1, 16},
      {primec::TokenKind::Number, "2i32", 1, 18},
      {primec::TokenKind::RBrace, "}", 1, 22},
      {primec::TokenKind::End, "", 1, 23},
  };
  primec::Parser parser(std::move(tokens));
  primec::Expr expr;
  std::string error;
  CHECK(parser.parseExpression(expr, "", error));
  CHECK(error.empty());
  REQUIRE(expr.kind == primec::Expr::Kind::Call);
  CHECK(expr.name == "foo");
  REQUIRE(expr.args.size() == 1);
  const auto &block = expr.args[0];
  CHECK(block.kind == primec::Expr::Kind::Call);
  CHECK(block.name == "block");
  CHECK(block.hasBodyArguments);
  REQUIRE(block.bodyArguments.size() == 1);
  CHECK(block.bodyArguments[0].kind == primec::Expr::Kind::Literal);
}

TEST_CASE("rejects hex integer literals without digits") {
  const std::string source = R"(
[return<int>]
main() {
  return(0xi32)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("invalid integer literal") != std::string::npos);
}

TEST_CASE("rejects hex integer literals with invalid digit") {
  const std::string source = R"(
[return<int>]
main() {
  return(0xG1i32)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("rejects decimal integer literals with invalid digit") {
  const std::string source = R"(
[return<int>]
main() {
  return(12a3i32)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("rejects u64 integer literals above maximum") {
  const std::string source = R"(
[return<int>]
main() {
  return(18446744073709551616u64)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("invalid integer literal") != std::string::npos);
}

TEST_CASE("parses i32 integer literals") {
  const std::string source = R"(
[return<int>]
main() {
  return(42i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].returnExpr.has_value());
  CHECK(program.definitions[0].returnExpr->kind == primec::Expr::Kind::Literal);
  CHECK(program.definitions[0].returnExpr->intWidth == 32);
  CHECK_FALSE(program.definitions[0].returnExpr->isUnsigned);
  CHECK(program.definitions[0].returnExpr->literalValue == 42);
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

TEST_CASE("parses float literals with comma separators") {
  const std::string source = R"(
[return<float>]
main() {
  return(1,000.5f32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].returnExpr.has_value());
  CHECK(program.definitions[0].returnExpr->kind == primec::Expr::Kind::FloatLiteral);
  CHECK(program.definitions[0].returnExpr->floatValue == "1000.5");
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
  REQUIRE(transforms[2].templateArgs.size() == 1);
  CHECK(transforms[2].templateArgs[0] == "int");
}

TEST_CASE("parses boolean transform arguments") {
  const std::string source = R"(
[tag(true false)]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  CHECK(transforms[0].name == "tag");
  REQUIRE(transforms[0].arguments.size() == 2);
  CHECK(transforms[0].arguments[0] == "true");
  CHECK(transforms[0].arguments[1] == "false");
}

TEST_CASE("parses transform arguments without commas") {
  const std::string source = R"(
[effects(global_write io_out) return<int>]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 2);
  CHECK(transforms[0].name == "effects");
  REQUIRE(transforms[0].arguments.size() == 2);
  CHECK(transforms[0].arguments[0] == "global_write");
  CHECK(transforms[0].arguments[1] == "io_out");
  CHECK(transforms[1].name == "return");
  REQUIRE(transforms[1].templateArgs.size() == 1);
  CHECK(transforms[1].templateArgs[0] == "int");
}

TEST_CASE("parses transform list without commas") {
  const std::string source = R"(
[effects(global_write, io_out) align_bytes(16) return<int>]
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
  REQUIRE(transforms[2].templateArgs.size() == 1);
  CHECK(transforms[2].templateArgs[0] == "int");
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
  return(make_color([hue] 1i32, [value] 2i32))
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

TEST_CASE("parses named arguments with comments") {
  const std::string source = R"(
[return<int>]
main() {
  return(make_color([hue /* note */] /* gap */ 1i32 [/*label*/ value] 2i32))
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].returnExpr.has_value());
  const auto &call = *program.definitions[0].returnExpr;
  REQUIRE(call.args.size() == 2);
  REQUIRE(call.argNames.size() == 2);
  CHECK(call.argNames[0].has_value());
  CHECK(call.argNames[1].has_value());
  CHECK(*call.argNames[0] == "hue");
  CHECK(*call.argNames[1] == "value");
}

TEST_CASE("parses argument label after expression with comments") {
  const std::string source = R"(
[return<int>]
main() {
  return(mix(base [value /* trailing */] 1i32))
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].returnExpr.has_value());
  const auto &call = *program.definitions[0].returnExpr;
  REQUIRE(call.kind == primec::Expr::Kind::Call);
  CHECK(call.name == "mix");
  REQUIRE(call.args.size() == 2);
  REQUIRE(call.argNames.size() == 2);
  CHECK_FALSE(call.argNames[0].has_value());
  REQUIRE(call.argNames[1].has_value());
  CHECK(*call.argNames[1] == "value");
  CHECK(call.args[0].kind == primec::Expr::Kind::Name);
}

TEST_CASE("parses argument label with comment before value") {
  const std::string source = R"(
[return<int>]
main() {
  return(mix(base [value] /* gap */ 1i32))
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].returnExpr.has_value());
  const auto &call = *program.definitions[0].returnExpr;
  REQUIRE(call.kind == primec::Expr::Kind::Call);
  CHECK(call.name == "mix");
  REQUIRE(call.args.size() == 2);
  REQUIRE(call.argNames.size() == 2);
  CHECK_FALSE(call.argNames[0].has_value());
  REQUIRE(call.argNames[1].has_value());
  CHECK(*call.argNames[1] == "value");
}

TEST_CASE("parses call arguments separated by semicolons") {
  const std::string source = R"(
[return<int>]
main() {
  return(mix(1i32; 2i32;))
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].returnExpr.has_value());
  const auto &call = *program.definitions[0].returnExpr;
  REQUIRE(call.kind == primec::Expr::Kind::Call);
  CHECK(call.name == "mix");
  REQUIRE(call.args.size() == 2);
  REQUIRE(call.argNames.size() == 2);
  CHECK_FALSE(call.argNames[0].has_value());
  CHECK_FALSE(call.argNames[1].has_value());
  CHECK(call.args[0].kind == primec::Expr::Kind::Literal);
  CHECK(call.args[1].kind == primec::Expr::Kind::Literal);
}

TEST_CASE("parses call arguments with leading semicolons") {
  const std::string source = R"(
[return<int>]
main() {
  return(mix(;1i32;;2i32))
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].returnExpr.has_value());
  const auto &call = *program.definitions[0].returnExpr;
  REQUIRE(call.kind == primec::Expr::Kind::Call);
  CHECK(call.name == "mix");
  REQUIRE(call.args.size() == 2);
  REQUIRE(call.argNames.size() == 2);
  CHECK_FALSE(call.argNames[0].has_value());
  CHECK_FALSE(call.argNames[1].has_value());
  CHECK(call.args[0].kind == primec::Expr::Kind::Literal);
  CHECK(call.args[1].kind == primec::Expr::Kind::Literal);
}

TEST_CASE("parses call arguments with semicolon-only body") {
  const std::string source = R"(
[return<int>]
main() {
  return(mix(;))
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].returnExpr.has_value());
  const auto &call = *program.definitions[0].returnExpr;
  REQUIRE(call.kind == primec::Expr::Kind::Call);
  CHECK(call.name == "mix");
  CHECK(call.args.empty());
  CHECK(call.argNames.empty());
}

TEST_CASE("parses call arguments with repeated mixed separators") {
  const std::string source = R"(
[return<int>]
main() {
  return(mix(1i32,,;2i32;,))
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].returnExpr.has_value());
  const auto &call = *program.definitions[0].returnExpr;
  REQUIRE(call.kind == primec::Expr::Kind::Call);
  CHECK(call.name == "mix");
  REQUIRE(call.args.size() == 2);
  REQUIRE(call.argNames.size() == 2);
  CHECK_FALSE(call.argNames[0].has_value());
  CHECK_FALSE(call.argNames[1].has_value());
  CHECK(call.args[0].kind == primec::Expr::Kind::Literal);
  CHECK(call.args[1].kind == primec::Expr::Kind::Literal);
}

TEST_CASE("parses named call arguments separated by semicolons") {
  const std::string source = R"(
[return<int>]
main() {
  return(make_color([hue] 1i32; [value] 2i32;))
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].returnExpr.has_value());
  const auto &call = *program.definitions[0].returnExpr;
  REQUIRE(call.kind == primec::Expr::Kind::Call);
  CHECK(call.name == "make_color");
  REQUIRE(call.args.size() == 2);
  REQUIRE(call.argNames.size() == 2);
  REQUIRE(call.argNames[0].has_value());
  REQUIRE(call.argNames[1].has_value());
  CHECK(*call.argNames[0] == "hue");
  CHECK(*call.argNames[1] == "value");
}

TEST_CASE("parses named call arguments with leading semicolons") {
  const std::string source = R"(
[return<int>]
main() {
  return(make_color(;[hue] 1i32;; [value] 2i32))
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].returnExpr.has_value());
  const auto &call = *program.definitions[0].returnExpr;
  REQUIRE(call.kind == primec::Expr::Kind::Call);
  CHECK(call.name == "make_color");
  REQUIRE(call.args.size() == 2);
  REQUIRE(call.argNames.size() == 2);
  REQUIRE(call.argNames[0].has_value());
  REQUIRE(call.argNames[1].has_value());
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

TEST_CASE("parses float literals with trailing dot") {
  const std::string source = R"(
[return<float>]
main() {
  return(1.)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].returnExpr.has_value());
  CHECK(program.definitions[0].returnExpr->kind == primec::Expr::Kind::FloatLiteral);
  CHECK(program.definitions[0].returnExpr->floatValue == "1.");
  CHECK(program.definitions[0].returnExpr->floatWidth == 32);
}

TEST_CASE("parses float literals with leading dot") {
  const std::string source = R"(
[return<float>]
main() {
  return(.5)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].returnExpr.has_value());
  CHECK(program.definitions[0].returnExpr->kind == primec::Expr::Kind::FloatLiteral);
  CHECK(program.definitions[0].returnExpr->floatValue == ".5");
  CHECK(program.definitions[0].returnExpr->floatWidth == 32);
}

TEST_CASE("parses negative leading dot float literals") {
  const std::string source = R"(
[return<float>]
main() {
  return(-.5f32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].returnExpr.has_value());
  CHECK(program.definitions[0].returnExpr->kind == primec::Expr::Kind::FloatLiteral);
  CHECK(program.definitions[0].returnExpr->floatValue == "-.5");
  CHECK(program.definitions[0].returnExpr->floatWidth == 32);
}

TEST_CASE("parses float literals with suffix after dot") {
  const std::string source = R"(
[return<float>]
main() {
  return(1.f32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].returnExpr.has_value());
  CHECK(program.definitions[0].returnExpr->kind == primec::Expr::Kind::FloatLiteral);
  CHECK(program.definitions[0].returnExpr->floatValue == "1.");
  CHECK(program.definitions[0].returnExpr->floatWidth == 32);
}

TEST_CASE("parses float literals with f suffix") {
  const std::string source = R"(
[return<float>]
main() {
  return(1f)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].returnExpr.has_value());
  CHECK(program.definitions[0].returnExpr->kind == primec::Expr::Kind::FloatLiteral);
  CHECK(program.definitions[0].returnExpr->floatValue == "1");
  CHECK(program.definitions[0].returnExpr->floatWidth == 32);
}

TEST_CASE("parses float literals with exponent after dot") {
  const std::string source = R"(
[return<float>]
main() {
  return(1.e2)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].returnExpr.has_value());
  CHECK(program.definitions[0].returnExpr->kind == primec::Expr::Kind::FloatLiteral);
  CHECK(program.definitions[0].returnExpr->floatValue == "1.e2");
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

TEST_CASE("parses float literals with uppercase exponent") {
  const std::string source = R"(
[return<f64>]
main() {
  return(1E+3f64)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].returnExpr.has_value());
  CHECK(program.definitions[0].returnExpr->kind == primec::Expr::Kind::FloatLiteral);
  CHECK(program.definitions[0].returnExpr->floatValue == "1E+3");
  CHECK(program.definitions[0].returnExpr->floatWidth == 64);
}

TEST_CASE("rejects float literals with missing exponent digits") {
  const std::string source = R"(
[return<float>]
main() {
  return(1e+f32)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("invalid float literal") != std::string::npos);
}

TEST_CASE("rejects float literals without width suffix in canonical mode") {
  const std::string source = R"(
[return<float>]
main() {
  return(1.5f)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize(), false);
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("float literal requires f32/f64 suffix in canonical mode") != std::string::npos);
}

TEST_CASE("parses float literals with width suffix in canonical mode") {
  const std::string source = R"(
[return<float>]
main() {
  return(1.5f32)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize(), false);
  primec::Program program;
  std::string error;
  CHECK(parser.parse(program, error));
  CHECK(error.empty());
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].returnExpr.has_value());
  CHECK(program.definitions[0].returnExpr->kind == primec::Expr::Kind::FloatLiteral);
  CHECK(program.definitions[0].returnExpr->floatValue == "1.5");
  CHECK(program.definitions[0].returnExpr->floatWidth == 32);
}

TEST_CASE("parses f64 float literals in canonical mode") {
  const std::string source = R"(
[return<f64>]
main() {
  return(2.25f64)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize(), false);
  primec::Program program;
  std::string error;
  CHECK(parser.parse(program, error));
  CHECK(error.empty());
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].returnExpr.has_value());
  CHECK(program.definitions[0].returnExpr->kind == primec::Expr::Kind::FloatLiteral);
  CHECK(program.definitions[0].returnExpr->floatValue == "2.25");
  CHECK(program.definitions[0].returnExpr->floatWidth == 64);
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

TEST_CASE("parses signed i64 min literal") {
  const std::string source = R"(
[return<int>]
main() {
  return(-9223372036854775808i64)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].returnExpr.has_value());
  const auto &expr = *program.definitions[0].returnExpr;
  CHECK(expr.kind == primec::Expr::Kind::Literal);
  CHECK_FALSE(expr.isUnsigned);
  CHECK(expr.intWidth == 64);
  CHECK(expr.literalValue == 0x8000000000000000ULL);
}

TEST_CASE("parses signed i32 min literal") {
  const std::string source = R"(
[return<int>]
main() {
  return(-2147483648i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].returnExpr.has_value());
  const auto &expr = *program.definitions[0].returnExpr;
  CHECK(expr.kind == primec::Expr::Kind::Literal);
  CHECK_FALSE(expr.isUnsigned);
  CHECK(expr.intWidth == 32);
  CHECK(expr.literalValue == 0xFFFFFFFF80000000ULL);
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

TEST_CASE("parses string literal escape sequences") {
  const std::string source = R"(
[return<void>]
main() {
  log("hello\nworld"utf8)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].statements.size() == 1);
  const auto &stmt = program.definitions[0].statements[0];
  CHECK(stmt.kind == primec::Expr::Kind::Call);
  REQUIRE(stmt.args.size() == 1);
  CHECK(stmt.args[0].kind == primec::Expr::Kind::StringLiteral);
  CHECK(stmt.args[0].stringValue == std::string("\"hello\\nworld\"utf8"));
}

TEST_CASE("parses single-quoted string literal arguments") {
  const std::string source = R"(
[return<void>]
main() {
  log('hello'utf8)
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

TEST_CASE("parses single-quoted string literal raw content") {
  const std::string source = R"(
[return<void>]
main() {
  log('hello\nworld'utf8)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].statements.size() == 1);
  const auto &stmt = program.definitions[0].statements[0];
  CHECK(stmt.kind == primec::Expr::Kind::Call);
  REQUIRE(stmt.args.size() == 1);
  CHECK(stmt.args[0].kind == primec::Expr::Kind::StringLiteral);
  CHECK(stmt.args[0].stringValue == std::string("\"hello\\\\nworld\"utf8"));
}

TEST_CASE("parses single-quoted string literal unknown escape") {
  const std::string source = R"(
[return<void>]
main() {
  log('hello\q'utf8)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].statements.size() == 1);
  const auto &stmt = program.definitions[0].statements[0];
  CHECK(stmt.kind == primec::Expr::Kind::Call);
  REQUIRE(stmt.args.size() == 1);
  CHECK(stmt.args[0].kind == primec::Expr::Kind::StringLiteral);
  CHECK(stmt.args[0].stringValue == std::string("\"hello\\\\q\"utf8"));
}
