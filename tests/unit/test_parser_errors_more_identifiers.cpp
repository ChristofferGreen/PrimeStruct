#include "test_parser_errors_more_helpers.h"

TEST_SUITE_BEGIN("primestruct.parser.errors.identifiers");

TEST_CASE("multiple return statements parse") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
  return(2i32)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK(parser.parse(program, error));
  CHECK(error.empty());
}

TEST_CASE("lambda requires body") {
  const std::string source = R"(
[return<int>]
main() {
  return([]([i32] value))
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("lambda requires a body") != std::string::npos);
}

TEST_CASE("lambda captures reject unexpected separator token") {
  const std::string source = R"(
[return<int>]
main() {
  return([value x .]([i32] value) { value })
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK_FALSE(error.empty());
  CHECK(error.find("expected") != std::string::npos);
}

TEST_CASE("lambda captures reject invalid first token") {
  const std::string source = R"(
[return<int>]
main() {
  return([.]([i32] value) { value })
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("expected lambda capture") != std::string::npos);
}

TEST_CASE("lambda captures reject numeric token after capture entry") {
  const std::string source = R"(
[return<int>]
main() {
  return([value 1]([i32] arg) { arg })
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("expected ',' or ']' after lambda capture") != std::string::npos);
}

TEST_CASE("lambda captures reject missing close bracket after comments") {
  const std::string source = R"(
[return<int>]
main() {
  return([value /* capture gap */ ([i32] arg) { arg })
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
}

TEST_CASE("lambda captures accept separator-only entry") {
  const std::string source = R"(
[return<int>]
main() {
  return([,]([i32] value) { value })
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK(parser.parse(program, error));
  CHECK(error.empty());
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].returnExpr.has_value());
  const auto &lambdaExpr = *program.definitions[0].returnExpr;
  REQUIRE(lambdaExpr.kind == primec::Expr::Kind::Call);
  REQUIRE(lambdaExpr.isLambda);
  CHECK(lambdaExpr.lambdaCaptures.empty());
}

TEST_CASE("lambda captures accept semicolon-only entry") {
  const std::string source = R"(
[return<int>]
main() {
  return([;]([i32] value) { value })
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK(parser.parse(program, error));
  CHECK(error.empty());
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].returnExpr.has_value());
  const auto &lambdaExpr = *program.definitions[0].returnExpr;
  REQUIRE(lambdaExpr.kind == primec::Expr::Kind::Call);
  REQUIRE(lambdaExpr.isLambda);
  CHECK(lambdaExpr.lambdaCaptures.empty());
}

TEST_CASE("lambda template requires parameter list") {
  const std::string source = R"(
[return<int>]
main() {
  return([]<T>{ value })
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("lambda capture list still requires parameter list after comments") {
  const std::string source = R"(
[return<int>]
main() {
  return([value] /* gap */ { value })
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("lambda template still requires parameter list after comments") {
  const std::string source = R"(
[return<int>]
main() {
  return([]</*template*/ T> /* gap */ { value })
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("lambda template requires closing angle") {
  const std::string source = R"(
[return<int>]
main() {
  return([]<T([i32] value) { value })
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("expected '>'") != std::string::npos);
}

TEST_CASE("labeled struct-literal local binding requires constructor initializer") {
  const std::string source = R"(
[struct]
Pair {
  [int] left{0}
  [int] right{0}
}

[int]
sum_pair() {
  [Pair] pair{[left] 4, [right] 8}
  return(pair.left + pair.right)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("expected identifier") != std::string::npos);
}

TEST_CASE("lambda template with comments still requires closing angle") {
  const std::string source = R"(
[return<int>]
main() {
  return([]</*open*/ T /*gap*/ ([i32] value) { value })
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("expected '>'") != std::string::npos);
}

TEST_CASE("lambda parameter default still requires lambda body") {
  const std::string source = R"(
[return<int>]
main() {
  return([]([i32] value { value })
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("lambda requires a body") != std::string::npos);
}

TEST_CASE("block shorthand rejected in expression context") {
  const std::string source = R"(
[return<int>]
main() {
  return(block{ 1i32 })
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("block requires block arguments") != std::string::npos);
}

TEST_CASE("reserved keyword cannot name definition") {
  const std::string source = R"(
[return<int>]
return() {
  return(1i32)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("reserved keyword") != std::string::npos);
}

TEST_CASE("class keyword cannot name definition") {
  const std::string source = R"(
[return<int>]
class() {
  return(1i32)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("reserved keyword") != std::string::npos);
}

TEST_CASE("control keyword cannot name definition") {
  const std::string source = R"(
[return<int>]
if() {
  return(1i32)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("reserved keyword") != std::string::npos);
}

TEST_CASE("match keyword cannot name definition") {
  const std::string source = R"(
[return<int>]
match() {
  return(1i32)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("reserved keyword") != std::string::npos);
}

TEST_CASE("reserved keyword cannot name parameter") {
  const std::string source = R"(
[return<int>]
main([i32] mut) {
  return(1i32)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("reserved keyword") != std::string::npos);
}

TEST_CASE("non-ascii identifier rejected") {
  const std::string source =
      std::string("[return<int>]\nma") + "\xC3\xA9" + "n() {\n  return(1i32)\n}\n";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("invalid character") != std::string::npos);
}

TEST_CASE("non-ascii slash path rejected") {
  const std::string source = std::string("import /m") + "\xC3\xB6" + "th\n"
                             "[return<int>]\n"
                             "main() {\n"
                             "  return(0i32)\n"
                             "}\n";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("invalid character") != std::string::npos);
}

TEST_CASE("non-ascii type identifier rejected") {
  const std::string source = std::string(R"(
[return<int>]
main() {
  [array<)") + "\xC3\xA9" + R"(>]
  values{array<i32>(1i32)}
  return(0i32)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("invalid character") != std::string::npos);
}

TEST_CASE("non-ascii whitespace rejected") {
  const std::string source =
      std::string("[return<int>]\nmain()") + "\xC2\xA0" + "{\n  return(1i32)\n}\n";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("invalid character") != std::string::npos);
}

TEST_CASE("reserved keyword rejected in type identifier") {
  const std::string source = R"(
[return<int>]
main() {
  [array<return>] values{array<i32>(1i32)}
  return(0i32)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("reserved keyword") != std::string::npos);
}

TEST_CASE("control keyword rejected in type identifier") {
  const std::string source = R"(
[return<int>]
main() {
  [array<for>] values{array<i32>(1i32)}
  return(0i32)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("reserved keyword") != std::string::npos);
}

TEST_CASE("return without argument fails") {
  const std::string source = R"(
[return<int>]
main() {
  return()
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("return requires exactly one argument") != std::string::npos);
}

TEST_CASE("return with too many arguments fails") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32, 2i32)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("return requires exactly one argument") != std::string::npos);
}

TEST_CASE("return value not allowed for void definitions") {
  const std::string source = R"(
[return<void>]
main() {
  return(1i32)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("return value not allowed for void definition") != std::string::npos);
}

TEST_SUITE_END();
