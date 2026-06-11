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
  CHECK(expr.isBraceConstructor);
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
  CHECK(expr.isBraceConstructor);
  REQUIRE(expr.args.size() == 1);
  const auto &block = expr.args[0];
  CHECK(block.kind == primec::Expr::Kind::Call);
  CHECK(block.name == "block");
  CHECK(block.hasBodyArguments);
  REQUIRE(block.bodyArguments.size() == 1);
  CHECK(block.bodyArguments[0].kind == primec::Expr::Kind::Literal);
}

TEST_CASE("expression parser keeps single collection brace item positional") {
  std::vector<primec::Token> tokens = {
      {primec::TokenKind::Identifier, "array", 1, 1},
      {primec::TokenKind::LAngle, "<", 1, 6},
      {primec::TokenKind::Identifier, "i32", 1, 7},
      {primec::TokenKind::RAngle, ">", 1, 10},
      {primec::TokenKind::LBrace, "{", 1, 11},
      {primec::TokenKind::Number, "1i32", 1, 12},
      {primec::TokenKind::RBrace, "}", 1, 16},
      {primec::TokenKind::End, "", 1, 17},
  };
  primec::Parser parser(std::move(tokens));
  primec::Expr expr;
  std::string error;
  CHECK(parser.parseExpression(expr, "", error));
  CHECK(error.empty());
  REQUIRE(expr.kind == primec::Expr::Kind::Call);
  CHECK(expr.name == "array");
  CHECK(expr.isBraceConstructor);
  REQUIRE(expr.args.size() == 1);
  CHECK_FALSE(expr.argNames[0].has_value());
  CHECK(expr.args[0].kind == primec::Expr::Kind::Literal);
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
  CHECK(expr.isBraceConstructor);
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
  CHECK(expr.isBraceConstructor);
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
  CHECK(expr.isBraceConstructor);
  REQUIRE(expr.args.size() == 1);
  const auto &block = expr.args[0];
  CHECK(block.kind == primec::Expr::Kind::Call);
  CHECK(block.name == "block");
  CHECK(block.hasBodyArguments);
  REQUIRE(block.bodyArguments.size() == 1);
  CHECK(block.bodyArguments[0].kind == primec::Expr::Kind::Literal);
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
