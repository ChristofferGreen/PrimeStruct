#pragma once

#include "test_parser_basic_helpers.h"

TEST_CASE("parses loop form") {
  const std::string source = R"(
[return<int>]
main() {
  loop(3i32) {
    plus(1i32 2i32)
  }
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].statements.size() == 2);
  const auto &loopCall = program.definitions[0].statements[0];
  REQUIRE(loopCall.kind == primec::Expr::Kind::Call);
  CHECK(loopCall.name == "loop");
  REQUIRE(loopCall.args.size() == 2);
  REQUIRE(loopCall.args[1].kind == primec::Expr::Kind::Call);
  CHECK(loopCall.args[1].name == "do");
  CHECK(loopCall.args[1].hasBodyArguments);
  REQUIRE(loopCall.args[1].bodyArguments.size() == 1);
}

TEST_CASE("parses while form") {
  const std::string source = R"(
[return<int>]
main() {
  while(less_than(1i32 2i32)) {
    plus(1i32 2i32)
  }
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].statements.size() == 2);
  const auto &loopCall = program.definitions[0].statements[0];
  REQUIRE(loopCall.kind == primec::Expr::Kind::Call);
  CHECK(loopCall.name == "while");
  REQUIRE(loopCall.args.size() == 2);
  REQUIRE(loopCall.args[1].kind == primec::Expr::Kind::Call);
  CHECK(loopCall.args[1].name == "do");
}

TEST_CASE("parses for form") {
  const std::string source = R"(
[return<int>]
main() {
  for([i32] i{0i32} less_than(i 3i32) increment(i)) {
    plus(i 1i32)
  }
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].statements.size() == 2);
  const auto &loopCall = program.definitions[0].statements[0];
  REQUIRE(loopCall.kind == primec::Expr::Kind::Call);
  CHECK(loopCall.name == "for");
  REQUIRE(loopCall.args.size() == 4);
  CHECK(loopCall.args[0].isBinding);
  REQUIRE(loopCall.args[3].kind == primec::Expr::Kind::Call);
  CHECK(loopCall.args[3].name == "do");
}

TEST_CASE("parses for form with separators") {
  const std::string source = R"(
[return<int>]
main() {
  for([i32] i{0i32}; less_than(i 3i32), increment(i)) {
    plus(i 1i32)
  }
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].statements.size() == 2);
  const auto &loopCall = program.definitions[0].statements[0];
  REQUIRE(loopCall.kind == primec::Expr::Kind::Call);
  CHECK(loopCall.name == "for");
  REQUIRE(loopCall.args.size() == 4);
  CHECK(loopCall.args[0].isBinding);
  REQUIRE(loopCall.args[3].kind == primec::Expr::Kind::Call);
  CHECK(loopCall.args[3].name == "do");
}

TEST_CASE("parses transform-prefixed loop form") {
  const std::string source = R"(
[return<int> effects(io_out)]
main() {
  [effects(io_out)] loop(1i32) {
    print_line("hi"utf8)
  }
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].statements.size() == 2);
  const auto &loopCall = program.definitions[0].statements[0];
  REQUIRE(loopCall.kind == primec::Expr::Kind::Call);
  CHECK(loopCall.name == "loop");
  REQUIRE(loopCall.transforms.size() == 1);
  CHECK(loopCall.transforms[0].name == "effects");
  REQUIRE(loopCall.args.size() == 2);
  REQUIRE(loopCall.args[1].kind == primec::Expr::Kind::Call);
  CHECK(loopCall.args[1].name == "do");
}

TEST_CASE("parses single-quoted strings") {
  const std::string source = R"(
[return<int> effects(io_out)]
main() {
  print_line('hi'utf8)
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].statements.size() == 2);
  const auto &stmt = program.definitions[0].statements[0];
  REQUIRE(stmt.kind == primec::Expr::Kind::Call);
  REQUIRE(stmt.args.size() == 1);
  CHECK(stmt.args[0].kind == primec::Expr::Kind::StringLiteral);
}

TEST_CASE("parses arguments without commas") {
  const std::string source = R"(
[return<int>]
main() {
  return(plus(1i32 2i32))
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].returnExpr.has_value());
  const auto &expr = program.definitions[0].returnExpr.value();
  CHECK(expr.kind == primec::Expr::Kind::Call);
  CHECK(expr.name == "plus");
  CHECK(expr.args.size() == 2);
}

TEST_CASE("parses separators as whitespace") {
  const std::string source = R"(
import /util/*; /math/*,

[return<int>; effects(io_out);]
main([i32] a{1i32;}; [i32] b{2i32,};) {
  [i32] total{plus(a; b,);}
  return(convert<i64; i32;>(total);)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.imports.size() == 2);
  CHECK(program.imports[0] == "/util/*");
  CHECK(program.imports[1] == "/math/*");
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].transforms.size() == 2);
  CHECK(program.definitions[0].parameters.size() == 2);
  REQUIRE(program.definitions[0].returnExpr.has_value());
  CHECK(program.definitions[0].returnExpr->args.size() == 1);
}

TEST_CASE("parses transform arguments with slash paths") {
  const std::string source = R"(
[custom(/util/io)]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].transforms.size() == 1);
  const auto &transform = program.definitions[0].transforms[0];
  REQUIRE(transform.arguments.size() == 1);
  CHECK(transform.arguments[0] == "/util/io");
}

TEST_CASE("parses parameters without commas") {
  const std::string source = R"(
[return<int>]
main([i32] a{1i32} [i32] b{2i32}) {
  return(plus(a b))
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  CHECK(program.definitions[0].parameters.size() == 2);
  REQUIRE(program.definitions[0].returnExpr.has_value());
  CHECK(program.definitions[0].returnExpr->args.size() == 2);
}

TEST_CASE("parses semicolon parameters without return transform") {
  const std::string source = R"(
main([i32] left; [i32] right) {
  return(plus(left, right))
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  CHECK(program.definitions[0].parameters.size() == 2);
  CHECK(program.definitions[0].name == "main");
}

TEST_CASE("parses template arguments without commas") {
  const std::string source = R"(
[return<int>]
main() {
  return(convert<i64 i32>(1i32))
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].returnExpr.has_value());
  CHECK(program.definitions[0].returnExpr->templateArgs.size() == 2);
}

TEST_CASE("parses comments as whitespace") {
  const std::string source = R"(
[return<int> /* transform */]
main(/* params */ [i32] value{1i32} /* end params */) {
  // line comment before binding
  [i32] total{plus(value, /* mid-arg */ 2i32)}
  return(convert<i64 /* tmpl */ i32>(total))
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].returnExpr.has_value());
  CHECK(program.definitions[0].returnExpr->templateArgs.size() == 2);
}

TEST_CASE("parses if call labels with comments") {
  const std::string source = R"(
[return<int>]
main() {
  return(if([/*label*/ cond] true))
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].returnExpr.has_value());
  const auto &expr = *program.definitions[0].returnExpr;
  CHECK(expr.kind == primec::Expr::Kind::Call);
  CHECK(expr.name == "if");
  REQUIRE(expr.argNames.size() == 1);
  REQUIRE(expr.argNames[0].has_value());
  CHECK(*expr.argNames[0] == "cond");
}

TEST_CASE("parses comment between signature and body") {
  const std::string source = R"(
[return<int>]
main() /* body gap */ {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  CHECK(program.definitions.size() == 1);
}

TEST_CASE("parses comment between exec args and terminator") {
  const std::string source = R"(
execute_repeat(2i32) /* exec gap */
)";
  const auto program = parseProgram(source);
  CHECK(program.executions.size() == 1);
  CHECK_FALSE(program.executions[0].hasBodyArguments);
}

TEST_CASE("parses local binding statements") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{7i32}
  return(value)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].statements.size() == 2);
  const auto &stmt = program.definitions[0].statements[0];
  CHECK(stmt.isBinding);
  CHECK(stmt.name == "value");
  REQUIRE(stmt.transforms.size() == 1);
  CHECK(stmt.transforms[0].name == "i32");
}

TEST_CASE("parses bare binding statements") {
  const std::string source = R"(
[return<int>]
main() {
  value{7i32}
  return(value)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].statements.size() == 2);
  const auto &stmt = program.definitions[0].statements[0];
  CHECK(stmt.isBinding);
  CHECK(stmt.name == "value");
  CHECK(stmt.transforms.empty());
}

TEST_CASE("parses binding type transforms with multiple template args") {
  const std::string source = R"(
[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(0i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].statements.size() == 2);
  const auto &stmt = program.definitions[0].statements[0];
  CHECK(stmt.isBinding);
  CHECK(stmt.name == "values");
  REQUIRE(stmt.transforms.size() == 1);
  CHECK(stmt.transforms[0].name == "map");
  REQUIRE(stmt.transforms[0].templateArgs.size() == 2);
  CHECK(stmt.transforms[0].templateArgs[0] == "i32");
  CHECK(stmt.transforms[0].templateArgs[1] == "i32");
}

TEST_CASE("parses transform template lists with multiple args") {
  const std::string source = R"(
[custom<i32, f32>]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].transforms.size() == 1);
  CHECK(program.definitions[0].transforms[0].name == "custom");
  REQUIRE(program.definitions[0].transforms[0].templateArgs.size() == 2);
  CHECK(program.definitions[0].transforms[0].templateArgs[0] == "i32");
  CHECK(program.definitions[0].transforms[0].templateArgs[1] == "f32");
}

TEST_CASE("parses transform string arguments with suffix") {
  const std::string source = R"(
[tag("demo"utf8) return<int>]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].transforms.size() == 2);
  CHECK(program.definitions[0].transforms[0].name == "tag");
  REQUIRE(program.definitions[0].transforms[0].arguments.size() == 1);
  CHECK(program.definitions[0].transforms[0].arguments[0] == "\"demo\"utf8");
}

TEST_CASE("parses transform list with comma separators") {
  const std::string source = R"(
[return<int>, effects(io_out)]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].transforms.size() == 2);
  CHECK(program.definitions[0].transforms[0].name == "return");
  CHECK(program.definitions[0].transforms[1].name == "effects");
  REQUIRE(program.definitions[0].transforms[1].arguments.size() == 1);
  CHECK(program.definitions[0].transforms[1].arguments[0] == "io_out");
}

TEST_CASE("parses template lists without commas") {
  const std::string source = R"(
[custom<i32 f32>]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].transforms.size() == 1);
  CHECK(program.definitions[0].transforms[0].name == "custom");
  REQUIRE(program.definitions[0].transforms[0].templateArgs.size() == 2);
  CHECK(program.definitions[0].transforms[0].templateArgs[0] == "i32");
  CHECK(program.definitions[0].transforms[0].templateArgs[1] == "f32");
}

TEST_CASE("parses nested template lists without commas") {
  const std::string source = R"(
[custom<map<i32 i64>>]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].transforms.size() == 1);
  CHECK(program.definitions[0].transforms[0].name == "custom");
  REQUIRE(program.definitions[0].transforms[0].templateArgs.size() == 1);
  CHECK(program.definitions[0].transforms[0].templateArgs[0] == "map<i32, i64>");
}

TEST_CASE("parses parameter list without commas") {
  const std::string source = R"(
[return<int>]
main([i32] a [i32] b) {
  return(plus(a, b))
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].parameters.size() == 2);
  CHECK(program.definitions[0].parameters[0].name == "a");
  CHECK(program.definitions[0].parameters[1].name == "b");
}

TEST_CASE("parses call arguments without commas") {
  const std::string source = R"(
[return<int>]
main() {
  return(plus(1i32 2i32))
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].returnExpr.has_value());
  const auto &call = *program.definitions[0].returnExpr;
  REQUIRE(call.kind == primec::Expr::Kind::Call);
  REQUIRE(call.args.size() == 2);
  CHECK(call.args[0].kind == primec::Expr::Kind::Literal);
  CHECK(call.args[1].kind == primec::Expr::Kind::Literal);
}

TEST_CASE("single_type_to_return stays in transform list") {
  const std::string source = R"(
[single_type_to_return i32 effects(io_out)]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 3);
  CHECK(transforms[0].name == "single_type_to_return");
  CHECK(transforms[1].name == "i32");
  CHECK(transforms[2].name == "effects");
}

TEST_CASE("single_type_to_return preserves custom type transform") {
  const std::string source = R"(
[single_type_to_return MyType]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 2);
  CHECK(transforms[0].name == "single_type_to_return");
  CHECK(transforms[1].name == "MyType");
}

TEST_CASE("parses method calls with template arguments") {
  const std::string source = R"(
namespace i32 {
  [return<i32>]
  wrap([i32] value) {
    return(value)
  }
}

[return<i32>]
main() {
  [i32] value{1i32}
  return(value.wrap<i32>())
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 2);
  REQUIRE(program.definitions[1].returnExpr.has_value());
  const auto &call = program.definitions[1].returnExpr.value();
  CHECK(call.kind == primec::Expr::Kind::Call);
  CHECK(call.isMethodCall);
  CHECK(call.name == "wrap");
  REQUIRE(call.templateArgs.size() == 1);
  CHECK(call.templateArgs[0] == "i32");
}

TEST_CASE("parses literal statement") {
  const std::string source = R"(
[return<int>]
main() {
  1i32
  return(2i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].statements.size() == 2);
  CHECK(program.definitions[0].statements[0].kind == primec::Expr::Kind::Literal);
}

TEST_CASE("parses struct definition without return") {
  const std::string source = R"(
[struct]
data() {
  [i32] value{1i32}
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  CHECK(program.definitions[0].fullPath == "/data");
  CHECK(program.definitions[0].hasReturnStatement == false);
  CHECK(program.definitions[0].statements.size() == 1);
  CHECK(program.definitions[0].statements[0].isBinding);
}

TEST_CASE("parses pod definition without return") {
  const std::string source = R"(
[pod]
data() {
  [i32] value{1i32}
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  CHECK(program.definitions[0].fullPath == "/data");
  CHECK(program.definitions[0].hasReturnStatement == false);
  CHECK(program.definitions[0].statements.size() == 1);
  CHECK(program.definitions[0].statements[0].isBinding);
}

TEST_CASE("parses if with block arguments") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{1i32}
  if(true, then(){
    [i32] temp{2i32}
    assign(value, temp)
  }, else(){ assign(value, 3i32) })
  return(value)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].statements.size() == 3);
  const auto &stmt = program.definitions[0].statements[1];
  CHECK(stmt.kind == primec::Expr::Kind::Call);
  CHECK(stmt.name == "if");
  REQUIRE(stmt.args.size() == 3);
  CHECK(stmt.args[1].name == "then");
  CHECK(stmt.args[2].name == "else");
  CHECK(stmt.args[1].bodyArguments.size() == 2);
  CHECK(stmt.args[2].bodyArguments.size() == 1);
  CHECK(stmt.args[1].bodyArguments[0].isBinding);
}

TEST_CASE("parses if blocks with return statements") {
  const std::string source = R"(
[return<int>]
main() {
  if(true, then(){ return(1i32) }, else(){ return(2i32) })
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].statements.size() == 1);
  const auto &stmt = program.definitions[0].statements[0];
  CHECK(stmt.kind == primec::Expr::Kind::Call);
  CHECK(stmt.name == "if");
  REQUIRE(stmt.args.size() == 3);
  REQUIRE(stmt.args[1].bodyArguments.size() == 1);
  REQUIRE(stmt.args[2].bodyArguments.size() == 1);
  CHECK(stmt.args[1].bodyArguments[0].name == "return");
  CHECK(stmt.args[2].bodyArguments[0].name == "return");
}
