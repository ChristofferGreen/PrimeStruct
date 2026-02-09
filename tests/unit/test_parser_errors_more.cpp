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

TEST_CASE("non-ascii type identifier rejected") {
  const std::string source = std::string(R"(
[return<int>]
main() {
  [array<)") + "\xC3\xA9" + R"(>]
  values(array<i32>(1i32))
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

TEST_CASE("reserved keyword rejected in type identifier") {
  const std::string source = R"(
[return<int>]
main() {
  [array<return>] values(array<i32>(1i32))
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

TEST_CASE("semicolon rejected at top level") {
  const std::string source = R"(
;
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
  CHECK(error.find("semicolon") != std::string::npos);
}

TEST_CASE("semicolon rejected in namespace") {
  const std::string source = R"(
namespace demo {
  ;
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
  CHECK(error.find("semicolon") != std::string::npos);
}

TEST_CASE("trailing comma in transform list is rejected") {
  const std::string source = R"(
[return<int>,]
main() {
  return(1i32)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("trailing comma not allowed in transform list") != std::string::npos);
}

TEST_CASE("non-ascii transform identifier rejected") {
  const std::string source = std::string(R"(
[tr)") + "\xC3\xA9" + R"(]
main() {
  return(1i32)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("invalid character") != std::string::npos);
}

TEST_CASE("invalid punctuation character rejected") {
  const std::string source = R"(
[return<int>]
main() {
  @
  return(1i32)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("invalid character") != std::string::npos);
}

TEST_CASE("unterminated string literal rejected") {
  const std::string source = R"(
[return<int>]
main() {
  print("hello)
  return(1i32)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("unterminated string literal") != std::string::npos);
}

TEST_CASE("unterminated block comment rejected") {
  const std::string source = R"(
[return<int>]
main() {
  /* missing terminator
  return(1i32)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("unterminated block comment") != std::string::npos);
}

TEST_CASE("slash path transform identifier rejected") {
  const std::string source = R"(
[/demo]
main() {
  return(1i32)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("transform identifiers cannot be slash paths") != std::string::npos);
}

TEST_CASE("non-ascii transform argument rejected") {
  const std::string source = std::string(R"(
[effects(i)") + "\xC3\xA9" + R"(o)]
main() {
  return(1i32)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("invalid character") != std::string::npos);
}

TEST_CASE("trailing comma in template argument list is rejected") {
  const std::string source = R"(
[return<int>]
main() {
  return(convert<i64,>(1i32))
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("trailing comma not allowed in template argument list") != std::string::npos);
}

TEST_CASE("trailing comma in nested type template list is rejected") {
  const std::string source = R"(
[return<int>]
main() {
  [array<map<i32,>>] values(array<i32>(1i32))
  return(0i32)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("trailing comma not allowed in template argument list") != std::string::npos);
}

TEST_CASE("trailing comma in parameter list is rejected") {
  const std::string source = R"(
[return<int>]
main([i32] value(1i32),) {
  return(0i32)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("trailing comma not allowed in parameter list") != std::string::npos);
}

TEST_CASE("parameter identifiers reject template arguments") {
  const std::string source = R"(
[return<int>]
main([i32] value<i32>) {
  return(0i32)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("parameter identifiers do not accept template arguments") != std::string::npos);
}

TEST_CASE("parameter defaults reject named arguments") {
  const std::string source = R"(
[return<int>]
main([i32] value(x = 1i32)) {
  return(value)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("parameter defaults do not accept named arguments") != std::string::npos);
}

TEST_CASE("trailing comma in argument list is rejected") {
  const std::string source = R"(
[return<int>]
main() {
  return(plus(1i32,))
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("trailing comma not allowed in argument list") != std::string::npos);
}

TEST_CASE("trailing comma in brace list is rejected") {
  const std::string source = R"(
execute_repeat(2i32) { main(), }
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("trailing comma not allowed in brace list") != std::string::npos);
}

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

TEST_CASE("named arguments rejected for print builtin") {
  const std::string source = R"(
[return<void> effects(io_out)]
main() {
  print_line(message = "hello"utf8)
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

TEST_CASE("named args for slash builtin fail in parser") {
  const std::string source = R"(
[return<int>]
main() {
  return(/plus(left = 1i32, right = 2i32))
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

TEST_CASE("named args for count fail in parser") {
  const std::string source = R"(
[return<int>]
main() {
  return(count(values = array<i32>(1i32, 2i32)))
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_CASE("named args for pathspace builtins fail in parser") {
  const std::string source = R"(
[return<void> effects(pathspace_notify, pathspace_insert, pathspace_take)]
main() {
  notify(path = "/events/test"utf8, value = 1i32)
  insert(path = "/events/test"utf8, value = 1i32)
  take(path = "/events/test"utf8)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_CASE("named args reject slash identifiers") {
  const std::string source = R"(
[return<int>]
main() {
  return(add(/foo = 1i32, 2i32))
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
  return(at(items = array<i32>(1i32, 2i32), index = 1i32))
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_CASE("named args for unsafe array access fail in parser") {
  const std::string source = R"(
[return<int>]
main() {
  return(at_unsafe(items = array<i32>(1i32, 2i32), index = 1i32))
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
foo([i32] a, [i32] b) {
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

TEST_CASE("transform string arguments require suffix") {
  const std::string source = R"(
[tag("oops")]
main() {
  return(1i32)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("string literal requires utf8/ascii/raw_utf8/raw_ascii suffix") != std::string::npos);
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
  [trace] if(true) {
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
  if(true) {
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
