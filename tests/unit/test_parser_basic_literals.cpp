#include "test_parser_basic_helpers.h"

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
