#include "test_parser_errors_more_helpers.h"

TEST_SUITE_BEGIN("primestruct.parser.errors.transforms");

TEST_CASE("transform group cannot be empty") {
  const std::string source = R"(
[text()]
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
  CHECK(error.find("transform text group cannot be empty") != std::string::npos);
}

TEST_CASE("text transform group requires parentheses") {
  const std::string source = R"(
[text]
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
  CHECK(error.find("transform group requires parentheses") != std::string::npos);
}

TEST_CASE("text transform group with comments requires parentheses") {
  const std::string source = R"(
[text /* gap */]
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
  CHECK(error.find("transform group requires parentheses") != std::string::npos);
}

TEST_CASE("semantic transform group requires parentheses") {
  const std::string source = R"(
[semantic<foo>]
main() {
  return(1i32)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("transform group requires parentheses") != std::string::npos);
}

TEST_CASE("semantic transform group with comments requires parentheses") {
  const std::string source = R"(
[semantic /* gap */ <foo>]
main() {
  return(1i32)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("transform group requires parentheses") != std::string::npos);
}

TEST_CASE("transform arguments cannot be empty") {
  const std::string source = R"(
[effects()]
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
  CHECK(error.find("transform argument list cannot be empty") != std::string::npos);
}

TEST_CASE("transform arguments reject punctuation token") {
  const std::string source = R"(
[tag(.)]
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

TEST_CASE("transform arguments reject nested envelopes") {
  const std::string source = R"(
[tag([i32] value)]
main() {
  return(1i32)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("text transform arguments must be identifiers or literals") != std::string::npos);
}

TEST_CASE("text transform arguments reject full forms") {
  const std::string source = R"(
[text(custom(foo(1i32)))]
main() {
  return(1i32)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("text transform arguments must be identifiers or literals") != std::string::npos);
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

TEST_CASE("transform template argument required with comments") {
  const std::string source = R"(
[return</* gap */>]
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

TEST_CASE("binding without initializer parses as binding") {
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
  CHECK(parser.parse(program, error));
  CHECK(error.empty());
  REQUIRE(program.definitions.size() == 1);
  const auto &statements = program.definitions[0].statements;
  REQUIRE(statements.size() == 1);
  const auto &call = statements[0];
  REQUIRE(call.kind == primec::Expr::Kind::Call);
  REQUIRE(call.args.size() == 1);
  const auto &arg = call.args[0];
  REQUIRE(arg.kind == primec::Expr::Kind::Call);
  CHECK(arg.isBinding);
  CHECK(arg.args.empty());
}

TEST_CASE("binding-like transforms allow paren call") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value(1i32)
  return(value)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK(parser.parse(program, error));
  CHECK(error.empty());
  REQUIRE(program.definitions.size() == 1);
  const auto &statements = program.definitions[0].statements;
  REQUIRE(statements.size() == 2);
  const auto &call = statements[0];
  REQUIRE(call.kind == primec::Expr::Kind::Call);
  REQUIRE(call.transforms.size() == 1);
  CHECK(call.transforms[0].name == "i32");
}

TEST_CASE("binding initializer rejects return without value") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{return()}
  return(1i32)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("return requires exactly one argument") != std::string::npos);
}

TEST_CASE("parameter default rejects paren initializer") {
  const std::string source = R"(
[return<int>]
add([i32] left(1i32), [i32] right) {
  return(plus(left, right))
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("parameter defaults must use braces") != std::string::npos);
}

TEST_CASE("parameter default rejects named arguments") {
  const std::string source = R"(
[return<int>]
add([i32] left{[value] 1i32}, [i32] right) {
  return(plus(left, right))
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("parameter defaults do not accept named arguments") != std::string::npos);
}

TEST_CASE("field access allows member without parameter list") {
  const std::string source = R"(
[return<int>]
main() {
  return(items.count)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK(parser.parse(program, error));
  CHECK(error.empty());
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

TEST_CASE("match template arguments require a call") {
  const std::string source = R"(
[return<int>]
main() {
  match<i32>
  return(1i32)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("template arguments require a call") != std::string::npos);
}

TEST_CASE("member template arguments require a call") {
  const std::string source = R"(
[return<int>]
main() {
  return(items.count<i32>)
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

TEST_CASE("match statement cannot have transforms") {
  const std::string source = R"(
[return<int>]
main() {
  [trace] match(true) {
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
  CHECK(error.find("match statement cannot have transforms") != std::string::npos);
}

TEST_CASE("definition without parameter list is allowed") {
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
  CHECK(parser.parse(program, error));
  CHECK(error.empty());
  REQUIRE(program.definitions.size() == 1);
  CHECK(program.definitions[0].fullPath == "/main");
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

TEST_CASE("single-branch if in value position is rejected") {
  const std::string source = R"(
[return<int>]
main() {
  return(if(true) { return(1i32) })
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("single-branch if is only allowed in statement position") != std::string::npos);
}

TEST_CASE("match statement requires else block") {
  const std::string source = R"(
[return<int>]
main() {
  match(true) {
    return(1i32)
  }
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("match statement requires else block") != std::string::npos);
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

TEST_CASE("definition tail transforms require parameter list first") {
  const std::string source = R"(
main[]() {
  return(1i32)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("definition transform tail sugar requires parameter list before transform list") !=
        std::string::npos);
}

TEST_CASE("executions reject tail transform lists") {
  const std::string source = R"(
run(1i32) [effects(io_out)]
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("post-parameter transforms are only valid on definitions") != std::string::npos);
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

TEST_CASE("namespace identifier cannot be control keyword") {
  const std::string source = R"(
namespace else {
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
  return(foo([return] 1i32))
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("reserved keyword cannot be used as identifier") != std::string::npos);
}

TEST_CASE("control keyword cannot name argument") {
  const std::string source = R"(
[return<int>]
main() {
  return(foo([while] 1i32))
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("reserved keyword cannot be used as identifier") != std::string::npos);
}

TEST_CASE("top-level binding-style transform requires parameter list") {
  const std::string source = R"(
[i32]
main {
  return(1i32)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("bindings are only allowed inside definition bodies or parameter lists") != std::string::npos);
}

TEST_CASE("top-level binding-only transform requires parameter list") {
  const std::string source = R"(
[mut]
main {
  return(1i32)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("expected '(' after identifier; bindings are only allowed inside definition bodies or parameter lists") !=
        std::string::npos);
}

TEST_SUITE_END();
