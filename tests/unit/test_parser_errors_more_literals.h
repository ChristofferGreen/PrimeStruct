#pragma once

#include "test_parser_errors_more_helpers.h"

TEST_SUITE_BEGIN("primestruct.parser.errors.literals");

TEST_CASE("string literal requires suffix") {
  const std::string source = R"(
[return<void>]
main() {
  print_line("hello")
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("string literal requires utf8/ascii/raw_utf8/raw_ascii suffix") != std::string::npos);
}

TEST_CASE("canonical string literal requires double quotes") {
  const std::string source = R"(
[return<void>]
main() {
  print_line('hello'utf8)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize(), false);
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("canonical string literal requires double quotes") != std::string::npos);
}

TEST_CASE("canonical string literal rejects raw suffix") {
  const std::string source = R"(
[return<void>]
main() {
  print_line("hello"raw_utf8)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize(), false);
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("canonical string literal requires utf8/ascii suffix") != std::string::npos);
}

TEST_CASE("canonical string literal requires normalized escapes") {
  const std::string source = R"(
[return<void>]
main() {
  print_line("he\'llo"utf8)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize(), false);
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("canonical string literal must use normalized escapes") != std::string::npos);
}

TEST_CASE("string literal rejects unknown suffix") {
  const std::string source = R"(
[return<void>]
main() {
  print_line("hello"utf16)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("unknown string literal suffix") != std::string::npos);
}

TEST_CASE("string literal rejects unknown escape") {
  const std::string source = R"(
[return<void>]
main() {
  print_line("hello\q"utf8)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("unknown escape sequence") != std::string::npos);
}

TEST_CASE("non-ASCII identifier rejects") {
  const std::string source = R"(
[return<void>]
maín() {
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("invalid character") != std::string::npos);
}

TEST_CASE("ascii string literal rejects non-ASCII characters") {
  const std::string source = R"(
[return<void>]
main() {
  print_line("héllo"ascii)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("ascii string literal contains non-ASCII characters") != std::string::npos);
}

TEST_CASE("raw ascii string literal rejects non-ASCII characters") {
  const std::string source = R"(
[return<void>]
main() {
  print_line("héllo"raw_ascii)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("ascii string literal contains non-ASCII characters") != std::string::npos);
}

TEST_CASE("raw string literal rejects embedded quotes") {
  const std::string source = R"(
[return<void>]
main() {
  print_line("he\"llo"raw_utf8)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("raw string literal cannot contain quote characters") != std::string::npos);
}

TEST_CASE("named arguments rejected for print builtin") {
  const std::string source = R"(
[return<void> effects(io_out)]
main() {
  print_line([message] "hello"utf8)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_CASE("named arguments require bracket syntax") {
  const std::string source = R"(
[return<int>]
foo([i32] a) {
  return(a)
}

[return<int>]
main() {
  return(foo(a = 1i32))
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("named arguments must use [name] syntax") != std::string::npos);
}

TEST_CASE("named arguments reject comment-separated equals") {
  const std::string source = R"(
[return<int>]
foo([i32] a) {
  return(a)
}

[return<int>]
main() {
  return(foo(a /* label */ = 1i32))
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("named arguments must use [name] syntax") != std::string::npos);
}

TEST_CASE("named arguments rejected for math builtin") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  return(sin([angle] 0.5f))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_CASE("named arguments rejected for slash math builtin") {
  const std::string source = R"(
[return<int>]
main() {
  return(/math/sin([angle] 0.5f))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_CASE("named arguments rejected for math builtin after import") {
  const std::string source = R"(
[return<int>]
main() {
  return(sin([angle] 0.5f))
}
import /math/*
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_CASE("import /math rejected") {
  const std::string source = R"(
import /math
[return<int>]
main() {
  return(0i32)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("import /math is not supported") != std::string::npos);
}

TEST_CASE("named arguments rejected for vector helper") {
  const std::string source = R"(
[return<int> effects(heap_alloc)]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  clear([values] values)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
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

TEST_CASE("block return does not satisfy definition return") {
  const std::string source = R"(
[return<int>]
main() {
  [effects(io_out)] block() { return(1i32) }
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("missing return statement in definition body") != std::string::npos);
}

TEST_CASE("import inside namespace fails") {
  const std::string source = R"(
namespace demo {
  import /util
  [return<int>]
  main() {
    return(1i32)
  }
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("import statements must appear at the top level") != std::string::npos);
}

TEST_CASE("import inside definition body fails") {
  const std::string source = R"(
[return<int>]
main() {
  import /util
  return(1i32)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("import statements must appear at the top level") != std::string::npos);
}

TEST_CASE("import path must be a slash path") {
  const std::string source = R"(
import util
[return<int>]
main() {
  return(1i32)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("import path must be a slash path") != std::string::npos);
}

TEST_CASE("import path rejects invalid slash identifier") {
  const std::string source = R"(
import /util//extra
[return<int>]
main() {
  return(1i32)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("invalid slash path identifier") != std::string::npos);
}

TEST_CASE("import path rejects trailing slash") {
  const std::string source = R"(
import /util/
[return<int>]
main() {
  return(1i32)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("invalid slash path identifier: /util/") != std::string::npos);
}

TEST_CASE("non-ascii identifiers are rejected") {
  const std::string source =
      "[return<int>]\n"
      "main() {\n"
      "  [i32] \xC3\xA5{1i32}\n"
      "  return(\xC3\xA5)\n"
      "}\n";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("invalid character") != std::string::npos);
}

TEST_CASE("namespace name must be a simple identifier") {
  const std::string source = R"(
namespace demo/util {
  [return<int>]
  main() {
    return(1i32)
  }
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("namespace name must be a simple identifier") != std::string::npos);
}

TEST_CASE("invalid slash path identifier fails") {
  const std::string source = R"(
[return<int>]
/demo//widget() {
  return(1i32)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("invalid slash path identifier") != std::string::npos);
}

TEST_CASE("slash path rejects reserved keyword segment") {
  const std::string source = R"(
[return<int>]
/demo/return/widget() {
  return(1i32)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("reserved keyword cannot be used as identifier: return") != std::string::npos);
}

TEST_CASE("slash path rejects control keyword segment") {
  const std::string source = R"(
[return<int>]
/demo/if/widget() {
  return(1i32)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("reserved keyword cannot be used as identifier: if") != std::string::npos);
}

TEST_CASE("slash path requires leading slash") {
  const std::string source = R"(
[return<int>]
demo/widget() {
  return(1i32)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("invalid slash path identifier") != std::string::npos);
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
  CHECK(error.find("integer literal requires i32/i64/u64 suffix") != std::string::npos);
}

TEST_CASE("negative unsigned literal is rejected") {
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

TEST_CASE("invalid float literal is rejected") {
  const std::string source = R"(
[return<f32>]
main() {
  return(1e-f32)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("invalid float literal") != std::string::npos);
}

TEST_CASE("leading dot float literal validates exponent") {
  const std::string source = R"(
[return<f32>]
main() {
  return(.5e)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("invalid float literal") != std::string::npos);
}

TEST_SUITE_END();
