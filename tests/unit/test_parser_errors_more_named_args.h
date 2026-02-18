#pragma once

#include "test_parser_errors_more_helpers.h"

TEST_SUITE_BEGIN("primestruct.parser.errors.named_args");

TEST_CASE("named args for builtin fail in parser") {
  const std::string source = R"(
[return<int>]
main() {
  return(plus([left] 1i32, [right] 2i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_CASE("named args for slash builtin fail in parser") {
  const std::string source = R"(
[return<int>]
main() {
  return(/plus([left] 1i32, [right] 2i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_CASE("named args for collection builtin fail in parser") {
  const std::string source = R"(
[return<int>]
main() {
  return(array<i32>([first] 1i32, [second] 2i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_CASE("named args for pointer helpers fail in parser") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{1i32}
  return(dereference([ptr] location(value)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_CASE("named args for count fail in parser") {
  const std::string source = R"(
[return<int>]
main() {
  return(count([values] array<i32>(1i32, 2i32)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_CASE("named args for pathspace builtins fail in parser") {
  const std::string source = R"(
[return<void> effects(pathspace_notify, pathspace_insert, pathspace_take)]
main() {
  notify([path] "/events/test"utf8, [value] 1i32)
  insert([path] "/events/test"utf8, [value] 1i32)
  take([path] "/events/test"utf8)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_CASE("named args reject slash identifiers") {
  const std::string source = R"(
[return<int>]
main() {
  return(add([/foo] 1i32, 2i32))
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("named argument must be a simple identifier") != std::string::npos);
}

TEST_CASE("named args for array access fail in parser") {
  const std::string source = R"(
[return<int>]
main() {
  return(at([items] array<i32>(1i32, 2i32), [index] 1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_CASE("named args for unsafe array access fail in parser") {
  const std::string source = R"(
[return<int>]
main() {
  return(at_unsafe([items] array<i32>(1i32, 2i32), [index] 1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_CASE("execution positional argument after named parses") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

execute_task([a] 1i32, 2i32)
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK(parser.parse(program, error));
  CHECK(error.empty());
}

TEST_CASE("execution named arguments cannot target builtins") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

[return<void>]
execute_task([array<i32>] items) {
  return()
}

execute_task([items] array<i32>([first] 1i32))
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_CASE("execution positional after named with collections parses") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

execute_task([items] array<i32>(1i32, 2i32), map<i32, i32>(1i32, 2i32))
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK(parser.parse(program, error));
  CHECK(error.empty());
}

TEST_CASE("positional argument after named parses") {
  const std::string source = R"(
[return<int>]
foo([i32] a, [i32] b) {
  return(a)
}

[return<int>]
main() {
  return(foo([a] 1i32, 2i32))
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK(parser.parse(program, error));
  CHECK(error.empty());
}

TEST_SUITE_END();
