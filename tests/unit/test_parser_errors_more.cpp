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

TEST_CASE("semicolon is rejected") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32);
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("semicolon") != std::string::npos);
}

TEST_CASE("named arguments rejected for print builtin") {
  const std::string source = R"(
[return<void> effects(io_out)]
main() {
  print_line(message = "hello")
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
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

TEST_CASE("named args for collection builtin fail in parser") {
  const std::string source = R"(
[return<int>]
main() {
  return(array<i32>(first = 1i32, second = 2i32))
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_CASE("named args for pointer helpers fail in parser") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value(1i32)
  return(dereference(ptr = location(value)))
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_CASE("execution positional argument after named fails in parser") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

execute_task(a = 1i32, 2i32) { }
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("positional argument cannot follow named arguments") != std::string::npos);
}

TEST_CASE("execution named arguments cannot target builtins") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

execute_task(items = array<i32>(first = 1i32)) { }
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_CASE("execution positional after named with collections fails in parser") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

execute_task(items = array<i32>(1i32, 2i32), map<i32, i32>(1i32, 2i32)) { }
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("positional argument cannot follow named arguments") != std::string::npos);
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

TEST_CASE("transform argument list cannot be empty") {
  const std::string source = R"(
[effects(), return<int>]
main() {
  return(1i32)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("transform argument list cannot be empty") != std::string::npos);
}

TEST_CASE("transform argument requires value") {
  const std::string source = R"(
[effects(,)]
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
  CHECK(error.find("expected transform argument") != std::string::npos);
}

TEST_CASE("transform template argument required") {
  const std::string source = R"(
[return<>]
main() {
  return(1i32)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("expected template identifier") != std::string::npos);
}

TEST_CASE("binding requires argument list in expression") {
  const std::string source = R"(
[return<int>]
main() {
  return([i32] value)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("binding requires argument list") != std::string::npos);
}

TEST_CASE("template arguments require a call") {
  const std::string source = R"(
[return<int>]
main() {
  return(value<i32>)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("template arguments require a call") != std::string::npos);
}

TEST_CASE("return statement cannot have transforms") {
  const std::string source = R"(
[return<int>]
main() {
  [mut] return(1i32)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("return statement cannot have transforms") != std::string::npos);
}

TEST_CASE("if statement cannot have transforms") {
  const std::string source = R"(
[return<int>]
main() {
  [trace] if(1i32) {
    return(1i32)
  } else {
    return(2i32)
  }
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("if statement cannot have transforms") != std::string::npos);
}

TEST_CASE("missing '(' after identifier fails") {
  const std::string source = R"(
[return<int>]
main {
  return(1i32)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("expected '(' after identifier") != std::string::npos);
}

TEST_CASE("missing ')' after parameters fails") {
  const std::string source = R"(
[return<int>]
main(a {
  return(1i32)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("expected ')' after parameters") != std::string::npos);
}

TEST_CASE("return missing parentheses fails") {
  const std::string source = R"(
[return<int>]
main() {
  return 1i32
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("expected '(' after return") != std::string::npos);
}

TEST_CASE("missing '>' in template list fails") {
  const std::string source = R"(
[return<int>]
main() {
  return(foo<i32(1i32))
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("expected '>'") != std::string::npos);
}

TEST_CASE("unexpected end of file in definition body") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("unexpected end of file inside definition body") != std::string::npos);
}

TEST_CASE("if statement requires else block") {
  const std::string source = R"(
[return<int>]
main() {
  if(1i32) {
    return(1i32)
  }
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("if statement requires else block") != std::string::npos);
}

TEST_CASE("definitions must have body") {
  const std::string source = R"(
[return<int>]
main()
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("definitions must have a body") != std::string::npos);
}

TEST_CASE("namespace identifier cannot be reserved keyword") {
  const std::string source = R"(
namespace return {
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
  CHECK(error.find("reserved keyword cannot be used as identifier") != std::string::npos);
}

TEST_CASE("unexpected end of file inside namespace block") {
  const std::string source = R"(
namespace demo {
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
  CHECK(error.find("unexpected end of file inside namespace block") != std::string::npos);
}

TEST_CASE("reserved keyword cannot name argument") {
  const std::string source = R"(
[return<int>]
main() {
  return(foo(return = 1i32))
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("reserved keyword cannot be used as identifier") != std::string::npos);
}

TEST_SUITE_END();
