#include "primec/Lexer.h"
#include "primec/Parser.h"

#include "third_party/doctest.h"

TEST_SUITE_BEGIN("primestruct.parser.errors.more");

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

TEST_CASE("reserved keyword cannot name parameter") {
  const std::string source = R"(
[return<int>]
main(mut) {
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

TEST_CASE("missing return fails in parser") {
  const std::string source = R"(
[return<int>]
main() {
  helper()
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("missing return statement in definition body") != std::string::npos);
}

TEST_CASE("out of range literal fails") {
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

TEST_CASE("minimum i32 literal succeeds") {
  const std::string source = R"(
[return<int>]
main() {
  return(-2147483648i32)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK(parser.parse(program, error));
  CHECK(error.empty());
}

TEST_CASE("minimum hex i32 literal succeeds") {
  const std::string source = R"(
[return<int>]
main() {
  return(-0x80000000i32)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK(parser.parse(program, error));
  CHECK(error.empty());
}

TEST_CASE("below minimum i32 literal fails") {
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

TEST_CASE("hex literal out of range fails") {
  const std::string source = R"(
[return<int>]
main() {
  return(0x80000000i32)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("integer literal out of range") != std::string::npos);
}

TEST_CASE("below minimum hex literal fails") {
  const std::string source = R"(
[return<int>]
main() {
  return(-0x80000001i32)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("integer literal out of range") != std::string::npos);
}

TEST_CASE("integer literal requires suffix") {
  const std::string source = R"(
[return<int>]
main() {
  return(42)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("integer literal requires i32 suffix") != std::string::npos);
}

TEST_CASE("named args for builtin fail in parser") {
  const std::string source = R"(
[return<int>]
main() {
  return(plus(left = 1i32, right = 2i32))
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_CASE("positional argument after named fails in parser") {
  const std::string source = R"(
[return<int>]
foo(a, b) {
  return(a)
}

[return<int>]
main() {
  return(foo(a = 1i32, 2i32))
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("positional argument cannot follow named arguments") != std::string::npos);
}

TEST_SUITE_END();
