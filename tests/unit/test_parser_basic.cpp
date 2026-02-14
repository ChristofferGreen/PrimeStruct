#include "primec/Lexer.h"
#include "primec/Parser.h"

#include "third_party/doctest.h"

namespace {
primec::Program parseProgram(const std::string &source) {
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK(parser.parse(program, error));
  CHECK(error.empty());
  return program;
}
} // namespace

TEST_SUITE_BEGIN("primestruct.parser.basic");

TEST_CASE("parses minimal main definition") {
  const std::string source = R"(
[return<int>]
main() {
  return(7i32)
}
)";

  const auto program = parseProgram(source);
  CHECK(program.definitions.size() == 1);
  CHECK(program.definitions[0].fullPath == "/main");
}

TEST_CASE("parses definition without return transform") {
  const std::string source = R"(
main() {
  return(7i32)
}
)";

  const auto program = parseProgram(source);
  CHECK(program.definitions.size() == 1);
  CHECK(program.definitions[0].fullPath == "/main");
}

TEST_CASE("parses void return without transform") {
  const std::string source = R"(
main() {
  return()
}
)";

  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  CHECK(program.definitions[0].fullPath == "/main");
  CHECK(program.definitions[0].hasReturnStatement);
  CHECK_FALSE(program.definitions[0].returnExpr.has_value());
}

TEST_CASE("parses slash path definition") {
  const std::string source = R"(
[return<int>]
/demo/widget() {
  return(1i32)
}
)";

  const auto program = parseProgram(source);
  CHECK(program.definitions.size() == 1);
  CHECK(program.definitions[0].fullPath == "/demo/widget");
}

TEST_CASE("parses nested definitions inside bodies") {
  const std::string source = R"(
[return<int>]
main() {
  helper() {
    return(1i32)
  }
  return(2i32)
}
)";

  const auto program = parseProgram(source);
  const auto findDef = [&](const std::string &path) -> const primec::Definition * {
    for (const auto &def : program.definitions) {
      if (def.fullPath == path) {
        return &def;
      }
    }
    return nullptr;
  };
  const auto *mainDef = findDef("/main");
  const auto *helperDef = findDef("/main/helper");
  REQUIRE(mainDef != nullptr);
  REQUIRE(helperDef != nullptr);
  CHECK(helperDef->namespacePrefix == "/main");
  REQUIRE(mainDef->statements.size() == 1);
  CHECK(mainDef->statements[0].kind == primec::Expr::Kind::Call);
  CHECK(mainDef->statements[0].name == "return");
}

TEST_CASE("parses lambda expressions in bodies") {
  const std::string source = R"(
[return<int>]
main() {
  return([]([i32] value) { value })
}
)";

  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &mainDef = program.definitions[0];
  REQUIRE(mainDef.statements.size() == 1);
  const auto &returnCall = mainDef.statements[0];
  REQUIRE(returnCall.kind == primec::Expr::Kind::Call);
  REQUIRE(returnCall.name == "return");
  REQUIRE(returnCall.args.size() == 1);
  const auto &lambdaExpr = returnCall.args[0];
  CHECK(lambdaExpr.isLambda);
  CHECK(lambdaExpr.lambdaCaptures.empty());
  REQUIRE(lambdaExpr.args.size() == 1);
  CHECK(lambdaExpr.args[0].isBinding);
  CHECK(lambdaExpr.hasBodyArguments);
  REQUIRE(lambdaExpr.bodyArguments.size() == 1);
}

TEST_CASE("parses lambda captures with separators") {
  const std::string source = R"(
[return<int>]
main() {
  return([value x, ref y; =;]([i32] value) { value })
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &returnCall = program.definitions[0].statements[0];
  REQUIRE(returnCall.kind == primec::Expr::Kind::Call);
  REQUIRE(returnCall.args.size() == 1);
  const auto &lambdaExpr = returnCall.args[0];
  REQUIRE(lambdaExpr.isLambda);
  REQUIRE(lambdaExpr.lambdaCaptures.size() == 3);
  CHECK(lambdaExpr.lambdaCaptures[0] == "value x");
  CHECK(lambdaExpr.lambdaCaptures[1] == "ref y");
  CHECK(lambdaExpr.lambdaCaptures[2] == "=");
}

TEST_CASE("parses brace constructor values") {
  const std::string source = R"(
[return<int>]
pick([i32] value) {
  return(value)
}

[return<int>]
main() {
  return(pick{ 3i32 })
}
)";

  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 2);
  const auto &mainDef = program.definitions[1];
  REQUIRE(mainDef.statements.size() == 1);
  const auto &returnCall = mainDef.statements[0];
  REQUIRE(returnCall.kind == primec::Expr::Kind::Call);
  REQUIRE(returnCall.args.size() == 1);
  const auto &pickCall = returnCall.args[0];
  REQUIRE(pickCall.kind == primec::Expr::Kind::Call);
  REQUIRE(pickCall.args.size() == 1);
  const auto &blockArg = pickCall.args[0];
  REQUIRE(blockArg.kind == primec::Expr::Kind::Call);
  CHECK(blockArg.name == "block");
  CHECK(blockArg.hasBodyArguments);
  REQUIRE(blockArg.bodyArguments.size() == 1);
  CHECK(blockArg.bodyArguments[0].kind == primec::Expr::Kind::Literal);
}

TEST_CASE("parses block call without parens") {
  const std::string source = R"(
[return<int>]
main() {
  return(block{ 3i32 })
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &mainDef = program.definitions[0];
  REQUIRE(mainDef.statements.size() == 1);
  const auto &returnCall = mainDef.statements[0];
  REQUIRE(returnCall.kind == primec::Expr::Kind::Call);
  REQUIRE(returnCall.args.size() == 1);
  const auto &blockCall = returnCall.args[0];
  REQUIRE(blockCall.kind == primec::Expr::Kind::Call);
  CHECK(blockCall.name == "block");
  CHECK(blockCall.args.empty());
  CHECK(blockCall.hasBodyArguments);
  REQUIRE(blockCall.bodyArguments.size() == 1);
}

TEST_CASE("parses call with trailing body arguments") {
  const std::string source = R"(
[return<int>]
main() {
  use(1i32) { log(2i32) }
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &mainDef = program.definitions[0];
  REQUIRE(mainDef.statements.size() == 2);
  const auto &useCall = mainDef.statements[0];
  REQUIRE(useCall.kind == primec::Expr::Kind::Call);
  CHECK(useCall.name == "use");
  CHECK(useCall.hasBodyArguments);
  REQUIRE(useCall.bodyArguments.size() == 1);
  CHECK(useCall.bodyArguments[0].kind == primec::Expr::Kind::Call);
}

TEST_CASE("parses execution with arguments and body") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

execute_repeat(3i32) { main() }
)";
  const auto program = parseProgram(source);
  CHECK(program.executions.size() == 1);
  CHECK(program.executions[0].fullPath == "/execute_repeat");
  CHECK(program.executions[0].arguments.size() == 1);
  CHECK(program.executions[0].bodyArguments.size() == 1);
}

TEST_CASE("parses import paths with comments") {
  const std::string source = R"(
import /util /* inline comment */ , /* gap */ /math/ /* star gap */ *
import /math/sin // trailing comment
import /math/pi /* block comment */

[return<int>]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.imports.size() == 4);
  CHECK(program.imports[0] == "/util/*");
  CHECK(program.imports[1] == "/math/*");
  CHECK(program.imports[2] == "/math/sin");
  CHECK(program.imports[3] == "/math/pi");
}

TEST_CASE("parses semicolon separators") {
  const std::string source = R"(
import /math/sin; /math/cos

[return<int>; effects(io_out)]
main([i32] left; [i32] right) {
  [i32] sum{plus(left; right)}
  print_line("hi"utf8);
  return(sum);
};

[return<int>]
pair<i32; i32>() {
  return(1i32);
};

execute_repeat(1i32; 2i32);
)";
  const auto program = parseProgram(source);
  REQUIRE(program.imports.size() == 2);
  CHECK(program.imports[0] == "/math/sin");
  CHECK(program.imports[1] == "/math/cos");
  REQUIRE(program.definitions.size() == 2);
  CHECK(program.definitions[0].parameters.size() == 2);
  REQUIRE(program.definitions[0].transforms.size() == 2);
  CHECK(program.definitions[1].templateArgs.size() == 2);
  CHECK(program.executions.size() == 1);
  CHECK(program.executions[0].arguments.size() == 2);
}

TEST_CASE("parses comma-separated statements and bindings") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{
    first(),
    second()
  }
  third(),
  fourth()
  return(value)
}

[return<int>]
first() { return(1i32) }

[return<int>]
second() { return(2i32) }

[return<int>]
third() { return(0i32) }

[return<int>]
fourth() { return(0i32) }
)";
  const auto program = parseProgram(source);
  const auto findDef = [&](const std::string &path) -> const primec::Definition * {
    for (const auto &def : program.definitions) {
      if (def.fullPath == path) {
        return &def;
      }
    }
    return nullptr;
  };
  const auto *mainDef = findDef("/main");
  REQUIRE(mainDef != nullptr);
  REQUIRE(mainDef->statements.size() >= 3);
  const auto &binding = mainDef->statements[0];
  REQUIRE(binding.isBinding);
  REQUIRE(binding.args.size() == 1);
  const auto &block = binding.args[0];
  REQUIRE(block.kind == primec::Expr::Kind::Call);
  CHECK(block.name == "block");
  CHECK(block.hasBodyArguments);
  REQUIRE(block.bodyArguments.size() == 2);
}

TEST_CASE("parses trailing separators in lists") {
  const std::string source = R"(
[return<int>; effects(io_out);]
pair<i32, i32,>() {
  return(1i32;)
}

[return<int>,]
main([i32;] left; [i32;] right;) {
  [array<i32,>] values{array<i32,>(1i32; 2i32;);}
  print_line("hi"utf8;)
  return(plus(left; right;);)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 2);
  CHECK(program.definitions[0].templateArgs.size() == 2);
  CHECK(program.definitions[1].parameters.size() == 2);
}

TEST_CASE("parses leading separators in template lists") {
  const std::string source = R"(
[return<int>]
pair<, i32; i32>() {
  return(1i32)
}

[return<int>]
main() {
  [array<; i32>] values{array<i32>(1i32)}
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 2);
  CHECK(program.definitions[0].templateArgs.size() == 2);
  const auto &binding = program.definitions[1].statements[0];
  REQUIRE(binding.isBinding);
  REQUIRE(binding.transforms.size() >= 1);
  CHECK(binding.transforms[0].name == "array");
  REQUIRE(binding.transforms[0].templateArgs.size() == 1);
  CHECK(binding.transforms[0].templateArgs[0] == "i32");
}

TEST_CASE("parses comma-separated transform lists") {
  const std::string source = R"(
[return<int>, effects(io_out)]
main() {
  print_line("hi"utf8)
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].transforms.size() == 2);
  CHECK(program.definitions[0].transforms[0].name == "return");
  CHECK(program.definitions[0].transforms[1].name == "effects");
}

TEST_CASE("parses transform groups") {
  const std::string source = R"(
[text(operators, collections) semantic(return<int>, effects(io_out))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].transforms.size() == 4);
  CHECK(program.definitions[0].transforms[0].name == "operators");
  CHECK(program.definitions[0].transforms[0].phase == primec::TransformPhase::Text);
  CHECK(program.definitions[0].transforms[1].name == "collections");
  CHECK(program.definitions[0].transforms[1].phase == primec::TransformPhase::Text);
  CHECK(program.definitions[0].transforms[2].name == "return");
  CHECK(program.definitions[0].transforms[2].phase == primec::TransformPhase::Semantic);
  CHECK(program.definitions[0].transforms[3].name == "effects");
  CHECK(program.definitions[0].transforms[3].phase == primec::TransformPhase::Semantic);
}

TEST_CASE("parses transform groups with semicolons") {
  const std::string source = R"(
[text(operators; collections;); semantic(return<int>; effects(io_out; io_err;););]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 4);
  CHECK(transforms[0].name == "operators");
  CHECK(transforms[0].phase == primec::TransformPhase::Text);
  CHECK(transforms[1].name == "collections");
  CHECK(transforms[1].phase == primec::TransformPhase::Text);
  CHECK(transforms[2].name == "return");
  CHECK(transforms[2].phase == primec::TransformPhase::Semantic);
  CHECK(transforms[3].name == "effects");
  CHECK(transforms[3].phase == primec::TransformPhase::Semantic);
  REQUIRE(transforms[3].arguments.size() == 2);
  CHECK(transforms[3].arguments[0] == "io_out");
  CHECK(transforms[3].arguments[1] == "io_err");
}

TEST_CASE("parses transform-prefixed execution") {
  const std::string source = R"(
[return<int> effects(io_out)]
main() {
  [effects(io_out)] print_line("hi"utf8)
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].statements.size() == 2);
  const auto &stmt = program.definitions[0].statements[0];
  REQUIRE(stmt.kind == primec::Expr::Kind::Call);
  CHECK(stmt.name == "print_line");
  REQUIRE(stmt.transforms.size() == 1);
  CHECK(stmt.transforms[0].name == "effects");
}

TEST_CASE("parses binding-like transforms on calls") {
  const std::string source = R"(
[return<int>]
main() {
  [operators, collections] log(1i32)
  return(sum([operators, collections] add(1i32) 2i32))
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &statements = program.definitions[0].statements;
  REQUIRE(statements.size() == 2);
  const auto &logCall = statements[0];
  REQUIRE(logCall.kind == primec::Expr::Kind::Call);
  REQUIRE(logCall.transforms.size() == 2);
  CHECK(logCall.transforms[0].name == "operators");
  const auto &returnCall = statements[1];
  REQUIRE(returnCall.kind == primec::Expr::Kind::Call);
  REQUIRE(returnCall.args.size() == 1);
  const auto &sumCall = returnCall.args[0];
  REQUIRE(sumCall.kind == primec::Expr::Kind::Call);
  REQUIRE(sumCall.args.size() == 2);
  const auto &nestedCall = sumCall.args[0];
  REQUIRE(nestedCall.kind == primec::Expr::Kind::Call);
  REQUIRE(nestedCall.transforms.size() == 2);
  CHECK(nestedCall.transforms[0].name == "operators");
}

TEST_CASE("parses execution transforms in bodies and arguments") {
  const std::string source = R"(
[return<int>]
main() {
  [effects(io_out)] log(1i32)
  return(add([effects(io_out)] log(2i32) 3i32))
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &statements = program.definitions[0].statements;
  REQUIRE(statements.size() == 2);
  const auto &logCall = statements[0];
  REQUIRE(logCall.kind == primec::Expr::Kind::Call);
  REQUIRE(logCall.transforms.size() == 1);
  CHECK(logCall.transforms[0].name == "effects");
  REQUIRE(logCall.transforms[0].arguments.size() == 1);
  CHECK(logCall.transforms[0].arguments[0] == "io_out");
  const auto &returnCall = statements[1];
  REQUIRE(returnCall.kind == primec::Expr::Kind::Call);
  REQUIRE(returnCall.args.size() == 1);
  const auto &addCall = returnCall.args[0];
  REQUIRE(addCall.kind == primec::Expr::Kind::Call);
  REQUIRE(addCall.args.size() == 2);
  const auto &nestedLog = addCall.args[0];
  REQUIRE(nestedLog.kind == primec::Expr::Kind::Call);
  REQUIRE(nestedLog.transforms.size() == 1);
  CHECK(nestedLog.transforms[0].name == "effects");
}

TEST_CASE("parses semicolon-separated transforms and lists") {
  const std::string source = R"(
[effects(io_out; io_err); return<int>]
sum([i32] a; [i32] b) {
  return(plus(a b))
}

[return<int>]
main() {
  return(sum(1i32; 2i32))
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 2);
  const auto &sum = program.definitions[0];
  REQUIRE(sum.transforms.size() == 2);
  CHECK(sum.transforms[0].name == "effects");
  REQUIRE(sum.transforms[0].arguments.size() == 2);
  CHECK(sum.transforms[0].arguments[0] == "io_out");
  CHECK(sum.transforms[0].arguments[1] == "io_err");
  CHECK(sum.parameters.size() == 2);
  const auto &returnCall = program.definitions[1].statements[0];
  REQUIRE(returnCall.kind == primec::Expr::Kind::Call);
  CHECK(returnCall.name == "return");
  REQUIRE(returnCall.args.size() == 1);
  const auto &call = returnCall.args[0];
  REQUIRE(call.kind == primec::Expr::Kind::Call);
  REQUIRE(call.args.size() == 2);
}

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

TEST_CASE("parses comment between exec args and body") {
  const std::string source = R"(
execute_repeat(2i32) /* exec gap */ { main() }
)";
  const auto program = parseProgram(source);
  CHECK(program.executions.size() == 1);
  CHECK(program.executions[0].hasBodyArguments);
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
  CHECK(program.definitions[0].transforms[0].arguments[0] == "\"demo\"raw_utf8");
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
  CHECK(transforms[0].arguments[0] == "\"hello world\"raw_utf8");
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
  CHECK(stmt.args[0].stringValue == "\"hello\"raw_utf8");
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
  CHECK(stmt.args[0].stringValue == std::string("\"hello\nworld\"raw_utf8"));
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
  CHECK(stmt.args[0].stringValue == "\"hello\"raw_utf8");
}

TEST_CASE("parses single-quoted string literal escapes") {
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
  CHECK(stmt.args[0].stringValue == std::string("\"hello\nworld\"raw_utf8"));
}

TEST_CASE("normalizes string literals with double quotes to single-quoted raw") {
  const std::string source = R"(
[return<void>]
main() {
  log("he\"llo"utf8)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].statements.size() == 1);
  const auto &stmt = program.definitions[0].statements[0];
  CHECK(stmt.kind == primec::Expr::Kind::Call);
  REQUIRE(stmt.args.size() == 1);
  CHECK(stmt.args[0].kind == primec::Expr::Kind::StringLiteral);
  CHECK(stmt.args[0].stringValue == std::string("'he\"llo'raw_utf8"));
}

TEST_CASE("parses ascii string literal arguments") {
  const std::string source = R"(
[return<void>]
main() {
  log("hello"ascii)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].statements.size() == 1);
  const auto &stmt = program.definitions[0].statements[0];
  CHECK(stmt.kind == primec::Expr::Kind::Call);
  REQUIRE(stmt.args.size() == 1);
  CHECK(stmt.args[0].kind == primec::Expr::Kind::StringLiteral);
  CHECK(stmt.args[0].stringValue == "\"hello\"raw_ascii");
}

TEST_CASE("parses raw string literal arguments") {
  const std::string source =
      "[return<void>]\n"
      "main() {\n"
      "  log(\"hello world\"raw_utf8)\n"
      "}\n";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].statements.size() == 1);
  const auto &stmt = program.definitions[0].statements[0];
  CHECK(stmt.kind == primec::Expr::Kind::Call);
  REQUIRE(stmt.args.size() == 1);
  CHECK(stmt.args[0].kind == primec::Expr::Kind::StringLiteral);
  CHECK(stmt.args[0].stringValue == "\"hello world\"raw_utf8");
}

TEST_CASE("parses raw string literal escapes") {
  const std::string source = R"(
[return<void>]
main() {
  log("hello\q"raw_utf8)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].statements.size() == 1);
  const auto &stmt = program.definitions[0].statements[0];
  CHECK(stmt.kind == primec::Expr::Kind::Call);
  REQUIRE(stmt.args.size() == 1);
  CHECK(stmt.args[0].kind == primec::Expr::Kind::StringLiteral);
  CHECK(stmt.args[0].stringValue == "\"hello\\q\"raw_utf8");
}

TEST_CASE("parses method call sugar") {
  const std::string source = R"(
[return<int>]
main() {
  return(items.count())
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].returnExpr.has_value());
  const auto &expr = *program.definitions[0].returnExpr;
  CHECK(expr.kind == primec::Expr::Kind::Call);
  CHECK(expr.isMethodCall);
  CHECK(expr.name == "count");
  REQUIRE(expr.args.size() == 1);
  CHECK(expr.args[0].kind == primec::Expr::Kind::Name);
  CHECK(expr.args[0].name == "items");
}

TEST_CASE("parses index sugar") {
  const std::string source = R"(
[return<int>]
main() {
  return(items[1i32])
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].returnExpr.has_value());
  const auto &expr = *program.definitions[0].returnExpr;
  CHECK(expr.kind == primec::Expr::Kind::Call);
  CHECK(expr.name == "at");
  REQUIRE(expr.args.size() == 2);
  CHECK(expr.args[0].kind == primec::Expr::Kind::Name);
  CHECK(expr.args[0].name == "items");
  CHECK(expr.args[1].kind == primec::Expr::Kind::Literal);
}

TEST_CASE("parses if statement sugar") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{1i32}
  if(true) {
    assign(value, 2i32)
  } else {
    assign(value, 3i32)
  }
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
  CHECK(stmt.args[1].bodyArguments.size() == 1);
  CHECK(stmt.args[2].bodyArguments.size() == 1);
}

TEST_CASE("parses if sugar in return argument") {
  const std::string source = R"(
[return<int>]
main() {
  return(if(true) { 4i32 } else { 9i32 })
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].returnExpr.has_value());
  const auto &expr = *program.definitions[0].returnExpr;
  CHECK(expr.kind == primec::Expr::Kind::Call);
  CHECK(expr.name == "if");
  REQUIRE(expr.args.size() == 3);
  CHECK(expr.args[1].name == "then");
  CHECK(expr.args[2].name == "else");
  REQUIRE(expr.args[1].bodyArguments.size() == 1);
  REQUIRE(expr.args[2].bodyArguments.size() == 1);
  CHECK(expr.args[1].bodyArguments[0].kind == primec::Expr::Kind::Literal);
  CHECK(expr.args[2].bodyArguments[0].kind == primec::Expr::Kind::Literal);
}

TEST_CASE("parses block expression without parens") {
  const std::string source = R"(
[return<int>]
main() {
  return(block{ 1i32 })
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].returnExpr.has_value());
  const auto &expr = *program.definitions[0].returnExpr;
  CHECK(expr.kind == primec::Expr::Kind::Call);
  CHECK(expr.name == "block");
  CHECK(expr.args.empty());
  REQUIRE(expr.bodyArguments.size() == 1);
  CHECK(expr.bodyArguments[0].kind == primec::Expr::Kind::Literal);
}

TEST_CASE("parses block expression with mixed separators") {
  const std::string source = R"(
[return<int>]
main() {
  return(block{ 1i32, 2i32 3i32 })
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].returnExpr.has_value());
  const auto &expr = *program.definitions[0].returnExpr;
  CHECK(expr.kind == primec::Expr::Kind::Call);
  CHECK(expr.name == "block");
  CHECK(expr.args.empty());
  REQUIRE(expr.bodyArguments.size() == 3);
}

TEST_CASE("parses return inside block expression list") {
  const std::string source = R"(
[return<int>]
main() {
  return(block{ return(1i32) 2i32 })
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].returnExpr.has_value());
  const auto &expr = *program.definitions[0].returnExpr;
  CHECK(expr.kind == primec::Expr::Kind::Call);
  CHECK(expr.name == "block");
  REQUIRE(expr.bodyArguments.size() == 2);
  CHECK(expr.bodyArguments[0].kind == primec::Expr::Kind::Call);
  CHECK(expr.bodyArguments[0].name == "return");
  CHECK(expr.bodyArguments[1].kind == primec::Expr::Kind::Literal);
}

TEST_CASE("parses if sugar with block statements in return argument") {
  const std::string source = R"(
[return<int>]
main() {
  return(if(true) { [i32] x{4i32} plus(x, 1i32) } else { 0i32 })
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].returnExpr.has_value());
  const auto &expr = *program.definitions[0].returnExpr;
  CHECK(expr.kind == primec::Expr::Kind::Call);
  CHECK(expr.name == "if");
  REQUIRE(expr.args.size() == 3);
  REQUIRE(expr.args[1].bodyArguments.size() == 2);
  CHECK(expr.args[1].bodyArguments[0].isBinding);
  CHECK(expr.args[1].bodyArguments[0].name == "x");
  CHECK(expr.args[1].bodyArguments[1].kind == primec::Expr::Kind::Call);
}

TEST_CASE("parses if sugar inside block with mixed separators") {
  const std::string source = R"(
[return<int>]
main() {
  return(block{ if(true) { 1i32 } else { 2i32 }, 3i32 4i32 })
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].returnExpr.has_value());
  const auto &expr = *program.definitions[0].returnExpr;
  CHECK(expr.kind == primec::Expr::Kind::Call);
  CHECK(expr.name == "block");
  REQUIRE(expr.bodyArguments.size() == 3);
  CHECK(expr.bodyArguments[0].kind == primec::Expr::Kind::Call);
  CHECK(expr.bodyArguments[0].name == "if");
 }

TEST_CASE("ignores line comments") {
  const std::string source = R"(
// header
[return<int>]
main() {
  // inside body
  return(1i32) // trailing
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].returnExpr.has_value());
  const auto &expr = *program.definitions[0].returnExpr;
  CHECK(expr.kind == primec::Expr::Kind::Literal);
}

TEST_CASE("ignores block comments") {
  const std::string source = R"(
[return<int>]
main() {
  return(plus(1i32, /* comment */ 2i32))
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].returnExpr.has_value());
  const auto &expr = *program.definitions[0].returnExpr;
  CHECK(expr.kind == primec::Expr::Kind::Call);
  CHECK(expr.name == "plus");
  REQUIRE(expr.args.size() == 2);
}

TEST_CASE("parses call with body arguments in expression") {
  const std::string source = R"(
[return<int>]
main() {
  return(task() { step() })
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].returnExpr.has_value());
  const auto &expr = *program.definitions[0].returnExpr;
  CHECK(expr.kind == primec::Expr::Kind::Call);
  CHECK(expr.name == "task");
  CHECK(expr.args.empty());
  REQUIRE(expr.bodyArguments.size() == 1);
  CHECK(expr.bodyArguments[0].name == "step");
}

TEST_SUITE_END();
