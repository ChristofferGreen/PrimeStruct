#pragma once

#include "test_parser_basic_helpers.h"

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

TEST_CASE("parses definition without parameter list") {
  const std::string source = R"(
[return<int>]
main {
  return(7i32)
}
)";

  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  CHECK(program.definitions[0].fullPath == "/main");
  CHECK(program.definitions[0].parameters.empty());
}

TEST_CASE("parses definition without parameter list and return transform") {
  const std::string source = R"(
main {
  return(7i32)
}
)";

  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  CHECK(program.definitions[0].fullPath == "/main");
  CHECK(program.definitions[0].parameters.empty());
}

TEST_CASE("parses struct definition without parameter list") {
  const std::string source = R"(
[struct]
data {
  [i32] value{1i32}
}
)";

  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  CHECK(program.definitions[0].fullPath == "/data");
  CHECK(program.definitions[0].parameters.empty());
  REQUIRE(program.definitions[0].statements.size() == 1);
  CHECK(program.definitions[0].statements[0].isBinding);
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

TEST_CASE("parses lambda expressions after earlier call arguments") {
  const std::string source = R"(
[return<void>]
main() {
  foo(1i32, []([i32] value) { return(value) })
}
)";

  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &mainDef = program.definitions[0];
  REQUIRE(mainDef.statements.size() == 1);
  const auto &fooCall = mainDef.statements[0];
  REQUIRE(fooCall.kind == primec::Expr::Kind::Call);
  REQUIRE(fooCall.name == "foo");
  REQUIRE(fooCall.args.size() == 2);
  CHECK(fooCall.args[0].kind == primec::Expr::Kind::Literal);
  const auto &lambdaExpr = fooCall.args[1];
  CHECK(lambdaExpr.isLambda);
  CHECK(lambdaExpr.lambdaCaptures.empty());
  REQUIRE(lambdaExpr.args.size() == 1);
  CHECK(lambdaExpr.args[0].isBinding);
  CHECK(lambdaExpr.hasBodyArguments);
  REQUIRE(lambdaExpr.bodyArguments.size() == 1);
}

TEST_CASE("parses lambda return inside void definitions") {
  const std::string source = R"(
[return<void>]
main() {
  []([i32] value) { return(value) }
}
)";

  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &mainDef = program.definitions[0];
  REQUIRE(mainDef.statements.size() == 1);
  const auto &lambdaExpr = mainDef.statements[0];
  REQUIRE(lambdaExpr.isLambda);
  REQUIRE(lambdaExpr.bodyArguments.size() == 1);
  const auto &returnCall = lambdaExpr.bodyArguments[0];
  REQUIRE(returnCall.kind == primec::Expr::Kind::Call);
  CHECK(returnCall.name == "return");
  REQUIRE(returnCall.args.size() == 1);
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

TEST_CASE("parses lambda captures with comment-only separators") {
  const std::string source = R"(
[return<int>]
main() {
  return([/* lead */ , /* comma */ ; /* tail */]([i32] value) { value })
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &returnCall = program.definitions[0].statements[0];
  REQUIRE(returnCall.kind == primec::Expr::Kind::Call);
  REQUIRE(returnCall.args.size() == 1);
  const auto &lambdaExpr = returnCall.args[0];
  REQUIRE(lambdaExpr.isLambda);
  CHECK(lambdaExpr.lambdaCaptures.empty());
}

TEST_CASE("parses lambda capture ampersand") {
  const std::string source = R"(
[return<int>]
main() {
  return([&]([i32] value) { value })
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &returnCall = program.definitions[0].statements[0];
  REQUIRE(returnCall.kind == primec::Expr::Kind::Call);
  REQUIRE(returnCall.args.size() == 1);
  const auto &lambdaExpr = returnCall.args[0];
  REQUIRE(lambdaExpr.isLambda);
  REQUIRE(lambdaExpr.lambdaCaptures.size() == 1);
  CHECK(lambdaExpr.lambdaCaptures[0] == "&");
}

TEST_CASE("parses lambda with semicolon-only parameter list") {
  const std::string source = R"(
[return<int>]
main() {
  return([](;) { 1i32 })
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
  REQUIRE(lambdaExpr.isLambda);
  CHECK(lambdaExpr.lambdaCaptures.empty());
  CHECK(lambdaExpr.args.empty());
  CHECK(lambdaExpr.argNames.empty());
  CHECK(lambdaExpr.hasBodyArguments);
  REQUIRE(lambdaExpr.bodyArguments.size() == 1);
  CHECK(lambdaExpr.bodyArguments[0].kind == primec::Expr::Kind::Literal);
}

TEST_CASE("parses lambda template arguments") {
  const std::string source = R"(
[return<int>]
main() {
  return([]<T>([T] value) { value })
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
  REQUIRE(lambdaExpr.isLambda);
  REQUIRE(lambdaExpr.templateArgs.size() == 1);
  CHECK(lambdaExpr.templateArgs[0] == "T");
  REQUIRE(lambdaExpr.args.size() == 1);
  CHECK(lambdaExpr.args[0].isBinding);
  CHECK(lambdaExpr.hasBodyArguments);
  REQUIRE(lambdaExpr.bodyArguments.size() == 1);
}

TEST_CASE("parses lambda captures with comments and operator chain") {
  const std::string source = R"(
[return<int>]
main() {
  return([/* lead */ value /* gap */ = /* eq */ & /* ref */ target ;]([i32] value) { value })
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &returnCall = program.definitions[0].statements[0];
  REQUIRE(returnCall.kind == primec::Expr::Kind::Call);
  REQUIRE(returnCall.args.size() == 1);
  const auto &lambdaExpr = returnCall.args[0];
  REQUIRE(lambdaExpr.isLambda);
  REQUIRE(lambdaExpr.lambdaCaptures.size() == 1);
  CHECK(lambdaExpr.lambdaCaptures[0] == "value = & target");
}

TEST_CASE("parses lambda template arguments with comments and semicolons") {
  const std::string source = R"(
[return<int>]
main() {
  return([]</* open */ T ; /* mid */ U>([T] value [U] extra) { value })
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &returnCall = program.definitions[0].statements[0];
  REQUIRE(returnCall.kind == primec::Expr::Kind::Call);
  REQUIRE(returnCall.args.size() == 1);
  const auto &lambdaExpr = returnCall.args[0];
  REQUIRE(lambdaExpr.isLambda);
  REQUIRE(lambdaExpr.templateArgs.size() == 2);
  CHECK(lambdaExpr.templateArgs[0] == "T");
  CHECK(lambdaExpr.templateArgs[1] == "U");
  REQUIRE(lambdaExpr.args.size() == 2);
  CHECK(lambdaExpr.args[0].isBinding);
  CHECK(lambdaExpr.args[1].isBinding);
  CHECK(lambdaExpr.hasBodyArguments);
  REQUIRE(lambdaExpr.bodyArguments.size() == 1);
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

TEST_CASE("parses primitive brace constructor as convert") {
  const std::string source = R"(
[return<bool>]
main() {
  return(bool{ 35i32 })
}
)";

  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &mainDef = program.definitions[0];
  REQUIRE(mainDef.statements.size() == 1);
  const auto &returnCall = mainDef.statements[0];
  REQUIRE(returnCall.kind == primec::Expr::Kind::Call);
  REQUIRE(returnCall.args.size() == 1);
  const auto &convertCall = returnCall.args[0];
  REQUIRE(convertCall.kind == primec::Expr::Kind::Call);
  CHECK(convertCall.name == "convert");
  REQUIRE(convertCall.templateArgs.size() == 1);
  CHECK(convertCall.templateArgs[0] == "bool");
  REQUIRE(convertCall.args.size() == 1);
  const auto &blockArg = convertCall.args[0];
  REQUIRE(blockArg.kind == primec::Expr::Kind::Call);
  CHECK(blockArg.name == "block");
  CHECK(blockArg.hasBodyArguments);
  REQUIRE(blockArg.bodyArguments.size() == 1);
  CHECK(blockArg.bodyArguments[0].kind == primec::Expr::Kind::Literal);
}

TEST_CASE("parses block call with parens") {
  const std::string source = R"(
[return<int>]
main() {
  return(block() { 3i32 })
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

TEST_CASE("parses execution with arguments") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

execute_repeat(3i32)
)";
  const auto program = parseProgram(source);
  CHECK(program.executions.size() == 1);
  CHECK(program.executions[0].fullPath == "/execute_repeat");
  CHECK(program.executions[0].arguments.size() == 1);
  CHECK(program.executions[0].bodyArguments.empty());
  CHECK_FALSE(program.executions[0].hasBodyArguments);
}

TEST_CASE("parses import paths with comments") {
  const std::string source = R"(
import /util /* inline comment */ , /* gap */ /std/math/ /* star gap */ *
import /std/math/sin // trailing comment
import /std/math/pi /* block comment */

[return<int>]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.imports.size() == 4);
  CHECK(program.imports[0] == "/util/*");
  CHECK(program.imports[1] == "/std/math/*");
  CHECK(program.imports[2] == "/std/math/sin");
  CHECK(program.imports[3] == "/std/math/pi");
}

TEST_CASE("parses semicolon separators") {
  const std::string source = R"(
import /std/math/sin; /std/math/cos

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
  CHECK(program.imports[0] == "/std/math/sin");
  CHECK(program.imports[1] == "/std/math/cos");
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

TEST_CASE("parses trait constraint transforms") {
  const std::string source = R"(
[return<int> Additive<i32> Indexable<array<i32>, i32>]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 3);
  CHECK(transforms[0].name == "return");
  CHECK(transforms[1].name == "Additive");
  REQUIRE(transforms[1].templateArgs.size() == 1);
  CHECK(transforms[1].templateArgs[0] == "i32");
  CHECK(transforms[2].name == "Indexable");
  REQUIRE(transforms[2].templateArgs.size() == 2);
  CHECK(transforms[2].templateArgs[0] == "array<i32>");
  CHECK(transforms[2].templateArgs[1] == "i32");
}

TEST_CASE("parses reflection transforms on struct definitions") {
  const std::string source = R"(
[struct reflect generate(Equal, DebugPrint)]
Widget() {
  [i32] value{1i32}
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 3);
  CHECK(transforms[0].name == "struct");
  CHECK(transforms[1].name == "reflect");
  CHECK(transforms[2].name == "generate");
  REQUIRE(transforms[2].arguments.size() == 2);
  CHECK(transforms[2].arguments[0] == "Equal");
  CHECK(transforms[2].arguments[1] == "DebugPrint");
}

TEST_CASE("parses reflection generate list for all v1 helpers") {
  const std::string source = R"(
[struct reflect generate(Equal, NotEqual, Default, IsDefault, Clone, DebugPrint)]
Widget() {
  [i32] value{1i32}
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 3);
  CHECK(transforms[0].name == "struct");
  CHECK(transforms[1].name == "reflect");
  CHECK(transforms[2].name == "generate");
  REQUIRE(transforms[2].arguments.size() == 6);
  CHECK(transforms[2].arguments[0] == "Equal");
  CHECK(transforms[2].arguments[1] == "NotEqual");
  CHECK(transforms[2].arguments[2] == "Default");
  CHECK(transforms[2].arguments[3] == "IsDefault");
  CHECK(transforms[2].arguments[4] == "Clone");
  CHECK(transforms[2].arguments[5] == "DebugPrint");
}

TEST_CASE("parses reflection generate Compare helper") {
  const std::string source = R"(
[struct reflect generate(Compare)]
Widget() {
  [i32] x{1i32}
  [i32] y{2i32}
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 3);
  CHECK(transforms[0].name == "struct");
  CHECK(transforms[1].name == "reflect");
  CHECK(transforms[2].name == "generate");
  REQUIRE(transforms[2].arguments.size() == 1);
  CHECK(transforms[2].arguments[0] == "Compare");
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

TEST_CASE("parses transform groups with comments and semicolons") {
  const std::string source = R"(
[text(/*first*/ operators; /*second*/ collections;); semantic(return<int>; effects(io_out; /*mid*/ io_err;);)]
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

TEST_CASE("parses semantic transform full form arguments") {
  const std::string source = R"(
[semantic(tag(foo(1i32)))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  CHECK(transforms[0].name == "tag");
  CHECK(transforms[0].phase == primec::TransformPhase::Semantic);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] == "foo(1i32)");
}

TEST_CASE("parses semantic transform arguments without separators") {
  const std::string source = R"(
[semantic(tag(foo(1i32) bar(2i32)))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 2);
  CHECK(transforms[0].arguments[0] == "foo(1i32)");
  CHECK(transforms[0].arguments[1] == "bar(2i32)");
}

TEST_CASE("parses semantic transform full forms across bracket continuation") {
  const std::string source = R"(
[semantic(tag(foo(1i32) [i32] value{1i32}))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 2);
  CHECK(transforms[0].arguments[0] == "at(foo(1i32), i32)");
  CHECK(transforms[0].arguments[1] == "value(block() { 1i32 })");
}

TEST_CASE("parses semantic transform scalar and body full forms") {
  const std::string source = R"(
[semantic(tag(true 1.5f32 "ok"utf8 [i32] value{1i32} block() { 1i32 }))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 5);
  CHECK(transforms[0].arguments[0] == "true");
  CHECK(transforms[0].arguments[1] == "1.5f32");
  CHECK(transforms[0].arguments[2] == "at(\"ok\"utf8, i32)");
  CHECK(transforms[0].arguments[3] == "value(block() { 1i32 })");
  CHECK(transforms[0].arguments[4] == "block() { 1i32 }");
}

TEST_CASE("parses semantic transform method-call full form argument") {
  const std::string source = R"(
[semantic(tag(values.count(2i32)))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] == "values.count(2i32)");
}

TEST_CASE("parses semantic transform method-call without explicit arguments") {
  const std::string source = R"(
[semantic(tag(values.count()))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] == "values.count()");
}

TEST_CASE("parses semantic transform field-access full form argument") {
  const std::string source = R"(
[semantic(tag(values.count))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] == "values.count()");
}

TEST_CASE("parses semantic transform field-access full form on call receiver") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] == "fetch_values(1i32).count()");
}

TEST_CASE("parses semantic transform indexing full form argument") {
  const std::string source = R"(
[semantic(tag(values[1i32]))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] == "at(values, 1i32)");
}

TEST_CASE("parses semantic transform index then field-access full form") {
  const std::string source = R"(
[semantic(tag(values[1i32].count))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] == "at(values, 1i32).count()");
}

TEST_CASE("parses semantic transform index then method-call full form") {
  const std::string source = R"(
[semantic(tag(values[1i32].count(2i32)))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] == "at(values, 1i32).count(2i32)");
}

TEST_CASE("parses semantic transform index then template method-call full form") {
  const std::string source = R"(
[semantic(tag(values[1i32].pick<i32>(2i32)))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] == "at(values, 1i32).pick<i32>(2i32)");
}

TEST_CASE("parses semantic transform index then template method-call named argument") {
  const std::string source = R"(
[semantic(tag(values[1i32].pick<i32>([index] 2i32)))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] == "at(values, 1i32).pick<i32>([index] 2i32)");
}

TEST_CASE("parses semantic transform indexing full form on field access") {
  const std::string source = R"(
[semantic(tag(values.count[1i32]))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] == "at(values.count(), 1i32)");
}

TEST_CASE("parses semantic transform indexing field access on call receiver") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count[2i32]))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] == "at(fetch_values(1i32).count(), 2i32)");
}

TEST_CASE("parses semantic transform indexing full form on method call") {
  const std::string source = R"(
[semantic(tag(values.count()[1i32]))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] == "at(values.count(), 1i32)");
}

TEST_CASE("parses semantic transform indexing method call on call receiver") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32]))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] == "at(fetch_values(1i32).count(), 2i32)");
}

TEST_CASE(
    "parses semantic transform index then template method-call on method-call receiver") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>(3i32)))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] ==
        "at(fetch_values(1i32).count(), 2i32).pick<i32>(3i32)");
}

TEST_CASE(
    "parses semantic transform index then template named method-call on method-call receiver") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>([index] 3i32)))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] ==
        "at(fetch_values(1i32).count(), 2i32).pick<i32>([index] 3i32)");
}

TEST_CASE(
    "parses semantic transform index then template method-call body on method-call receiver") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>(3i32) { foo(4i32) }))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] ==
        "at(fetch_values(1i32).count(), 2i32).pick<i32>(3i32) { foo(4i32) }");
}

TEST_CASE(
    "parses semantic transform index then template named method-call body on method-call receiver") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>([index] 3i32) { foo(4i32) }))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] ==
        "at(fetch_values(1i32).count(), 2i32).pick<i32>([index] 3i32) { foo(4i32) }");
}

TEST_CASE(
    "parses semantic transform index then template method-call no-arg body on method-call receiver") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>() { foo(4i32) }))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] ==
        "at(fetch_values(1i32).count(), 2i32).pick<i32>() { foo(4i32) }");
}

TEST_CASE(
    "parses semantic transform index after template method-call no-arg body on method-call receiver") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>() { foo(4i32) }[5i32]))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] ==
        "at(at(fetch_values(1i32).count(), 2i32).pick<i32>() { foo(4i32) }, 5i32)");
}

TEST_CASE(
    "parses semantic transform index after template named method-call body on method-call receiver") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>([index] 3i32) { foo(4i32) }[5i32]))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] ==
        "at(at(fetch_values(1i32).count(), 2i32).pick<i32>([index] 3i32) { foo(4i32) }, 5i32)");
}

TEST_CASE(
    "parses semantic transform method-call after nested indexed template body chain") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>([index] 3i32) { foo(4i32) }[5i32].count(6i32)))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(
      transforms[0].arguments[0] ==
      "at(at(fetch_values(1i32).count(), 2i32).pick<i32>([index] 3i32) { foo(4i32) }, 5i32).count(6i32)");
}

TEST_CASE(
    "parses semantic transform indexing after nested indexed template body chain method-call") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>([index] 3i32) { foo(4i32) }[5i32].count(6i32)[7i32]))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(
      transforms[0].arguments[0] ==
      "at(at(at(fetch_values(1i32).count(), 2i32).pick<i32>([index] 3i32) { foo(4i32) }, 5i32).count(6i32), 7i32)");
}

TEST_CASE(
    "parses semantic transform method-call after nested indexed template body chain indexed method-call") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>([index] 3i32) { foo(4i32) }[5i32].count(6i32)[7i32].count(8i32)))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(
      transforms[0].arguments[0] ==
      "at(at(at(fetch_values(1i32).count(), 2i32).pick<i32>([index] 3i32) { foo(4i32) }, 5i32).count(6i32), 7i32).count(8i32)");
}

TEST_CASE(
    "parses semantic transform indexing after nested indexed template body chain indexed method-call") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>([index] 3i32) { foo(4i32) }[5i32].count(6i32)[7i32].count(8i32)[9i32]))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(
      transforms[0].arguments[0] ==
      "at(at(at(at(fetch_values(1i32).count(), 2i32).pick<i32>([index] 3i32) { foo(4i32) }, 5i32).count(6i32), 7i32).count(8i32), 9i32)");
}

TEST_CASE(
    "parses semantic transform field-access after nested indexed template body chain indexed method-call") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>([index] 3i32) { foo(4i32) }[5i32].count(6i32)[7i32].count(8i32)[9i32].count))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(
      transforms[0].arguments[0] ==
      "at(at(at(at(fetch_values(1i32).count(), 2i32).pick<i32>([index] 3i32) { foo(4i32) }, 5i32).count(6i32), 7i32).count(8i32), 9i32).count()");
}

TEST_CASE(
    "parses semantic transform indexing after nested indexed template body chain indexed method-call field-access") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>([index] 3i32) { foo(4i32) }[5i32].count(6i32)[7i32].count(8i32)[9i32].count[10i32]))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(
      transforms[0].arguments[0] ==
      "at(at(at(at(at(fetch_values(1i32).count(), 2i32).pick<i32>([index] 3i32) { foo(4i32) }, 5i32).count(6i32), 7i32).count(8i32), 9i32).count(), 10i32)");
}

TEST_CASE(
    "parses semantic transform method-call after nested indexed template body chain indexed method-call field-access") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>([index] 3i32) { foo(4i32) }[5i32].count(6i32)[7i32].count(8i32)[9i32].count[10i32].count(11i32)))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(
      transforms[0].arguments[0] ==
      "at(at(at(at(at(fetch_values(1i32).count(), 2i32).pick<i32>([index] 3i32) { foo(4i32) }, 5i32).count(6i32), 7i32).count(8i32), 9i32).count(), 10i32).count(11i32)");
}

TEST_CASE(
    "parses semantic transform indexing after nested indexed template body chain indexed method-call field-access tail") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>([index] 3i32) { foo(4i32) }[5i32].count(6i32)[7i32].count(8i32)[9i32].count[10i32].count(11i32)[12i32]))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(
      transforms[0].arguments[0] ==
      "at(at(at(at(at(at(fetch_values(1i32).count(), 2i32).pick<i32>([index] 3i32) { foo(4i32) }, 5i32).count(6i32), 7i32).count(8i32), 9i32).count(), 10i32).count(11i32), 12i32)");
}

TEST_CASE(
    "parses semantic transform field-access after nested indexed template body chain indexed method-call field-access tail") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>([index] 3i32) { foo(4i32) }[5i32].count(6i32)[7i32].count(8i32)[9i32].count[10i32].count(11i32)[12i32].count))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(
      transforms[0].arguments[0] ==
      "at(at(at(at(at(at(fetch_values(1i32).count(), 2i32).pick<i32>([index] 3i32) { foo(4i32) }, 5i32).count(6i32), 7i32).count(8i32), 9i32).count(), 10i32).count(11i32), 12i32).count()");
}

TEST_CASE(
    "parses semantic transform indexing after nested indexed template body chain indexed method-call field-access tail field-access") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>([index] 3i32) { foo(4i32) }[5i32].count(6i32)[7i32].count(8i32)[9i32].count[10i32].count(11i32)[12i32].count[13i32]))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(
      transforms[0].arguments[0] ==
      "at(at(at(at(at(at(at(fetch_values(1i32).count(), 2i32).pick<i32>([index] 3i32) { foo(4i32) }, 5i32).count(6i32), 7i32).count(8i32), 9i32).count(), 10i32).count(11i32), 12i32).count(), 13i32)");
}

TEST_CASE(
    "parses semantic transform method-call after nested indexed template body chain indexed method-call field-access tail field-access") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>([index] 3i32) { foo(4i32) }[5i32].count(6i32)[7i32].count(8i32)[9i32].count[10i32].count(11i32)[12i32].count[13i32].count(14i32)))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(
      transforms[0].arguments[0] ==
      "at(at(at(at(at(at(at(fetch_values(1i32).count(), 2i32).pick<i32>([index] 3i32) { foo(4i32) }, 5i32).count(6i32), 7i32).count(8i32), 9i32).count(), 10i32).count(11i32), 12i32).count(), 13i32).count(14i32)");
}

TEST_CASE(
    "parses semantic transform field-access after nested indexed template body chain indexed method-call field-access tail indexed method-call field-access") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>([index] 3i32) { foo(4i32) }[5i32].count(6i32)[7i32].count(8i32)[9i32].count[10i32].count(11i32)[12i32].count[13i32].count(14i32).count))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(
      transforms[0].arguments[0] ==
      "at(at(at(at(at(at(at(fetch_values(1i32).count(), 2i32).pick<i32>([index] 3i32) { foo(4i32) }, 5i32).count(6i32), 7i32).count(8i32), 9i32).count(), 10i32).count(11i32), 12i32).count(), 13i32).count(14i32).count()");
}

TEST_CASE(
    "parses semantic transform indexing after nested indexed template body chain indexed method-call field-access tail indexed method-call field-access") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>([index] 3i32) { foo(4i32) }[5i32].count(6i32)[7i32].count(8i32)[9i32].count[10i32].count(11i32)[12i32].count[13i32].count(14i32).count[15i32]))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(
      transforms[0].arguments[0] ==
      "at(at(at(at(at(at(at(at(fetch_values(1i32).count(), 2i32).pick<i32>([index] 3i32) { foo(4i32) }, 5i32).count(6i32), 7i32).count(8i32), 9i32).count(), 10i32).count(11i32), 12i32).count(), 13i32).count(14i32).count(), 15i32)");
}

TEST_CASE(
    "parses semantic transform method-call after nested indexed template body chain indexed method-call field-access tail indexed method-call field-access index") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>([index] 3i32) { foo(4i32) }[5i32].count(6i32)[7i32].count(8i32)[9i32].count[10i32].count(11i32)[12i32].count[13i32].count(14i32).count[15i32].count(16i32)))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(
      transforms[0].arguments[0] ==
      "at(at(at(at(at(at(at(at(fetch_values(1i32).count(), 2i32).pick<i32>([index] 3i32) { foo(4i32) }, 5i32).count(6i32), 7i32).count(8i32), 9i32).count(), 10i32).count(11i32), 12i32).count(), 13i32).count(14i32).count(), 15i32).count(16i32)");
}

TEST_CASE(
    "parses semantic transform indexing after nested indexed template body chain indexed method-call field-access tail indexed method-call field-access index method-call") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>([index] 3i32) { foo(4i32) }[5i32].count(6i32)[7i32].count(8i32)[9i32].count[10i32].count(11i32)[12i32].count[13i32].count(14i32).count[15i32].count(16i32)[17i32]))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(
      transforms[0].arguments[0] ==
      "at(at(at(at(at(at(at(at(at(fetch_values(1i32).count(), 2i32).pick<i32>([index] 3i32) { foo(4i32) }, 5i32).count(6i32), 7i32).count(8i32), 9i32).count(), 10i32).count(11i32), 12i32).count(), 13i32).count(14i32).count(), 15i32).count(16i32), 17i32)");
}

TEST_CASE(
    "parses semantic transform field-access after nested indexed template body chain indexed method-call field-access tail indexed method-call field-access index method-call") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>([index] 3i32) { foo(4i32) }[5i32].count(6i32)[7i32].count(8i32)[9i32].count[10i32].count(11i32)[12i32].count[13i32].count(14i32).count[15i32].count(16i32)[17i32].count))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(
      transforms[0].arguments[0] ==
      "at(at(at(at(at(at(at(at(at(fetch_values(1i32).count(), 2i32).pick<i32>([index] 3i32) { foo(4i32) }, 5i32).count(6i32), 7i32).count(8i32), 9i32).count(), 10i32).count(11i32), 12i32).count(), 13i32).count(14i32).count(), 15i32).count(16i32), 17i32).count()");
}

TEST_CASE(
    "parses semantic transform indexing after nested indexed template body chain indexed method-call field-access tail indexed method-call field-access index method-call field-access") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>([index] 3i32) { foo(4i32) }[5i32].count(6i32)[7i32].count(8i32)[9i32].count[10i32].count(11i32)[12i32].count[13i32].count(14i32).count[15i32].count(16i32)[17i32].count[18i32]))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(
      transforms[0].arguments[0] ==
      "at(at(at(at(at(at(at(at(at(at(fetch_values(1i32).count(), 2i32).pick<i32>([index] 3i32) { foo(4i32) }, 5i32).count(6i32), 7i32).count(8i32), 9i32).count(), 10i32).count(11i32), 12i32).count(), 13i32).count(14i32).count(), 15i32).count(16i32), 17i32).count(), 18i32)");
}

TEST_CASE(
    "parses semantic transform method-call after nested indexed template body chain indexed method-call field-access tail indexed method-call field-access index method-call field-access index") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>([index] 3i32) { foo(4i32) }[5i32].count(6i32)[7i32].count(8i32)[9i32].count[10i32].count(11i32)[12i32].count[13i32].count(14i32).count[15i32].count(16i32)[17i32].count[18i32].count(19i32)))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(
      transforms[0].arguments[0] ==
      "at(at(at(at(at(at(at(at(at(at(fetch_values(1i32).count(), 2i32).pick<i32>([index] 3i32) { foo(4i32) }, 5i32).count(6i32), 7i32).count(8i32), 9i32).count(), 10i32).count(11i32), 12i32).count(), 13i32).count(14i32).count(), 15i32).count(16i32), 17i32).count(), 18i32).count(19i32)");
}

TEST_CASE(
    "parses semantic transform indexing after nested indexed template body chain indexed method-call field-access tail indexed method-call field-access index method-call field-access index method-call") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>([index] 3i32) { foo(4i32) }[5i32].count(6i32)[7i32].count(8i32)[9i32].count[10i32].count(11i32)[12i32].count[13i32].count(14i32).count[15i32].count(16i32)[17i32].count[18i32].count(19i32)[20i32]))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(
      transforms[0].arguments[0] ==
      "at(at(at(at(at(at(at(at(at(at(at(fetch_values(1i32).count(), 2i32).pick<i32>([index] 3i32) { foo(4i32) }, 5i32).count(6i32), 7i32).count(8i32), 9i32).count(), 10i32).count(11i32), 12i32).count(), 13i32).count(14i32).count(), 15i32).count(16i32), 17i32).count(), 18i32).count(19i32), 20i32)");
}

TEST_CASE(
    "parses semantic transform field-access after nested indexed template body chain indexed method-call field-access tail indexed method-call field-access index method-call field-access index method-call") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>([index] 3i32) { foo(4i32) }[5i32].count(6i32)[7i32].count(8i32)[9i32].count[10i32].count(11i32)[12i32].count[13i32].count(14i32).count[15i32].count(16i32)[17i32].count[18i32].count(19i32)[20i32].count))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(
      transforms[0].arguments[0] ==
      "at(at(at(at(at(at(at(at(at(at(at(fetch_values(1i32).count(), 2i32).pick<i32>([index] 3i32) { foo(4i32) }, 5i32).count(6i32), 7i32).count(8i32), 9i32).count(), 10i32).count(11i32), 12i32).count(), 13i32).count(14i32).count(), 15i32).count(16i32), 17i32).count(), 18i32).count(19i32), 20i32).count()");
}

TEST_CASE(
    "parses semantic transform indexing after nested indexed template body chain indexed method-call field-access tail indexed method-call field-access index method-call field-access index method-call field-access") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>([index] 3i32) { foo(4i32) }[5i32].count(6i32)[7i32].count(8i32)[9i32].count[10i32].count(11i32)[12i32].count[13i32].count(14i32).count[15i32].count(16i32)[17i32].count[18i32].count(19i32)[20i32].count[21i32]))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(
      transforms[0].arguments[0] ==
      "at(at(at(at(at(at(at(at(at(at(at(at(fetch_values(1i32).count(), 2i32).pick<i32>([index] 3i32) { foo(4i32) }, 5i32).count(6i32), 7i32).count(8i32), 9i32).count(), 10i32).count(11i32), 12i32).count(), 13i32).count(14i32).count(), 15i32).count(16i32), 17i32).count(), 18i32).count(19i32), 20i32).count(), 21i32)");
}

TEST_CASE(
    "parses semantic transform method-call after nested indexed template body chain indexed method-call field-access tail indexed method-call field-access index method-call field-access index method-call field-access index") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>([index] 3i32) { foo(4i32) }[5i32].count(6i32)[7i32].count(8i32)[9i32].count[10i32].count(11i32)[12i32].count[13i32].count(14i32).count[15i32].count(16i32)[17i32].count[18i32].count(19i32)[20i32].count[21i32].count(22i32)))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(
      transforms[0].arguments[0] ==
      "at(at(at(at(at(at(at(at(at(at(at(at(fetch_values(1i32).count(), 2i32).pick<i32>([index] 3i32) { foo(4i32) }, 5i32).count(6i32), 7i32).count(8i32), 9i32).count(), 10i32).count(11i32), 12i32).count(), 13i32).count(14i32).count(), 15i32).count(16i32), 17i32).count(), 18i32).count(19i32), 20i32).count(), 21i32).count(22i32)");
}

TEST_CASE(
    "parses semantic transform field-access after nested indexed template body chain indexed method-call field-access tail indexed method-call field-access index method-call field-access index method-call field-access index") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>([index] 3i32) { foo(4i32) }[5i32].count(6i32)[7i32].count(8i32)[9i32].count[10i32].count(11i32)[12i32].count[13i32].count(14i32).count[15i32].count(16i32)[17i32].count[18i32].count(19i32)[20i32].count[21i32].count(22i32).count))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(
      transforms[0].arguments[0] ==
      "at(at(at(at(at(at(at(at(at(at(at(at(fetch_values(1i32).count(), 2i32).pick<i32>([index] 3i32) { foo(4i32) }, 5i32).count(6i32), 7i32).count(8i32), 9i32).count(), 10i32).count(11i32), 12i32).count(), 13i32).count(14i32).count(), 15i32).count(16i32), 17i32).count(), 18i32).count(19i32), 20i32).count(), 21i32).count(22i32).count()");
}

TEST_CASE(
    "parses semantic transform indexing after nested indexed template body chain indexed method-call field-access tail indexed method-call field-access index method-call field-access index method-call field-access index") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>([index] 3i32) { foo(4i32) }[5i32].count(6i32)[7i32].count(8i32)[9i32].count[10i32].count(11i32)[12i32].count[13i32].count(14i32).count[15i32].count(16i32)[17i32].count[18i32].count(19i32)[20i32].count[21i32].count(22i32).count[23i32]))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(
      transforms[0].arguments[0] ==
      "at(at(at(at(at(at(at(at(at(at(at(at(at(fetch_values(1i32).count(), 2i32).pick<i32>([index] 3i32) { foo(4i32) }, 5i32).count(6i32), 7i32).count(8i32), 9i32).count(), 10i32).count(11i32), 12i32).count(), 13i32).count(14i32).count(), 15i32).count(16i32), 17i32).count(), 18i32).count(19i32), 20i32).count(), 21i32).count(22i32).count(), 23i32)");
}

TEST_CASE(
    "parses semantic transform method-call after nested indexed template body chain indexed method-call field-access tail indexed method-call field-access index method-call field-access index method-call field-access index method-call") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>([index] 3i32) { foo(4i32) }[5i32].count(6i32)[7i32].count(8i32)[9i32].count[10i32].count(11i32)[12i32].count[13i32].count(14i32).count[15i32].count(16i32)[17i32].count[18i32].count(19i32)[20i32].count[21i32].count(22i32).count[23i32].count(24i32)))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(
      transforms[0].arguments[0] ==
      "at(at(at(at(at(at(at(at(at(at(at(at(at(fetch_values(1i32).count(), 2i32).pick<i32>([index] 3i32) { foo(4i32) }, 5i32).count(6i32), 7i32).count(8i32), 9i32).count(), 10i32).count(11i32), 12i32).count(), 13i32).count(14i32).count(), 15i32).count(16i32), 17i32).count(), 18i32).count(19i32), 20i32).count(), 21i32).count(22i32).count(), 23i32).count(24i32)");
}

TEST_CASE(
    "parses semantic transform indexing after nested indexed template body chain indexed method-call field-access tail indexed method-call field-access index method-call field-access index method-call field-access index method-call field-access") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>([index] 3i32) { foo(4i32) }[5i32].count(6i32)[7i32].count(8i32)[9i32].count[10i32].count(11i32)[12i32].count[13i32].count(14i32).count[15i32].count(16i32)[17i32].count[18i32].count(19i32)[20i32].count[21i32].count(22i32).count[23i32].count(24i32)[25i32]))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(
      transforms[0].arguments[0] ==
      "at(at(at(at(at(at(at(at(at(at(at(at(at(at(fetch_values(1i32).count(), 2i32).pick<i32>([index] 3i32) { foo(4i32) }, 5i32).count(6i32), 7i32).count(8i32), 9i32).count(), 10i32).count(11i32), 12i32).count(), 13i32).count(14i32).count(), 15i32).count(16i32), 17i32).count(), 18i32).count(19i32), 20i32).count(), 21i32).count(22i32).count(), 23i32).count(24i32), 25i32)");
}

TEST_CASE(
    "parses semantic transform field-access after nested indexed template body chain indexed method-call field-access tail indexed method-call field-access index method-call field-access index method-call field-access index method-call") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>([index] 3i32) { foo(4i32) }[5i32].count(6i32)[7i32].count(8i32)[9i32].count[10i32].count(11i32)[12i32].count[13i32].count(14i32).count[15i32].count(16i32)[17i32].count[18i32].count(19i32)[20i32].count[21i32].count(22i32).count[23i32].count(24i32)[25i32].count))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(
      transforms[0].arguments[0] ==
      "at(at(at(at(at(at(at(at(at(at(at(at(at(at(fetch_values(1i32).count(), 2i32).pick<i32>([index] 3i32) { foo(4i32) }, 5i32).count(6i32), 7i32).count(8i32), 9i32).count(), 10i32).count(11i32), 12i32).count(), 13i32).count(14i32).count(), 15i32).count(16i32), 17i32).count(), 18i32).count(19i32), 20i32).count(), 21i32).count(22i32).count(), 23i32).count(24i32), 25i32).count()");
}

TEST_CASE(
    "parses semantic transform indexing after nested indexed template body chain indexed method-call field-access tail indexed method-call field-access index method-call field-access index method-call field-access index method-call") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>([index] 3i32) { foo(4i32) }[5i32].count(6i32)[7i32].count(8i32)[9i32].count[10i32].count(11i32)[12i32].count[13i32].count(14i32).count[15i32].count(16i32)[17i32].count[18i32].count(19i32)[20i32].count[21i32].count(22i32).count[23i32].count(24i32)[25i32].count[26i32]))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(
      transforms[0].arguments[0] ==
      "at(at(at(at(at(at(at(at(at(at(at(at(at(at(at(fetch_values(1i32).count(), 2i32).pick<i32>([index] 3i32) { foo(4i32) }, 5i32).count(6i32), 7i32).count(8i32), 9i32).count(), 10i32).count(11i32), 12i32).count(), 13i32).count(14i32).count(), 15i32).count(16i32), 17i32).count(), 18i32).count(19i32), 20i32).count(), 21i32).count(22i32).count(), 23i32).count(24i32), 25i32).count(), 26i32)");
}

TEST_CASE(
    "parses semantic transform method-call after nested indexed template body chain indexed method-call field-access tail indexed method-call field-access index method-call field-access index method-call field-access index method-call field-access") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>([index] 3i32) { foo(4i32) }[5i32].count(6i32)[7i32].count(8i32)[9i32].count[10i32].count(11i32)[12i32].count[13i32].count(14i32).count[15i32].count(16i32)[17i32].count[18i32].count(19i32)[20i32].count[21i32].count(22i32).count[23i32].count(24i32)[25i32].count[26i32].count(27i32)))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(
      transforms[0].arguments[0] ==
      "at(at(at(at(at(at(at(at(at(at(at(at(at(at(at(fetch_values(1i32).count(), 2i32).pick<i32>([index] 3i32) { foo(4i32) }, 5i32).count(6i32), 7i32).count(8i32), 9i32).count(), 10i32).count(11i32), 12i32).count(), 13i32).count(14i32).count(), 15i32).count(16i32), 17i32).count(), 18i32).count(19i32), 20i32).count(), 21i32).count(22i32).count(), 23i32).count(24i32), 25i32).count(), 26i32).count(27i32)");
}

TEST_CASE(
    "parses semantic transform indexing after nested indexed template body chain indexed method-call field-access tail indexed method-call field-access index method-call field-access index method-call field-access index method-call field-access method-call field-access") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>([index] 3i32) { foo(4i32) }[5i32].count(6i32)[7i32].count(8i32)[9i32].count[10i32].count(11i32)[12i32].count[13i32].count(14i32).count[15i32].count(16i32)[17i32].count[18i32].count(19i32)[20i32].count[21i32].count(22i32).count[23i32].count(24i32)[25i32].count[26i32].count(27i32)[28i32]))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(
      transforms[0].arguments[0] ==
      "at(at(at(at(at(at(at(at(at(at(at(at(at(at(at(at(fetch_values(1i32).count(), 2i32).pick<i32>([index] 3i32) { foo(4i32) }, 5i32).count(6i32), 7i32).count(8i32), 9i32).count(), 10i32).count(11i32), 12i32).count(), 13i32).count(14i32).count(), 15i32).count(16i32), 17i32).count(), 18i32).count(19i32), 20i32).count(), 21i32).count(22i32).count(), 23i32).count(24i32), 25i32).count(), 26i32).count(27i32), 28i32)");
}

TEST_CASE(
    "parses semantic transform field-access after nested indexed template body chain indexed method-call field-access tail indexed method-call field-access index method-call field-access index method-call field-access index method-call field-access method-call") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>([index] 3i32) { foo(4i32) }[5i32].count(6i32)[7i32].count(8i32)[9i32].count[10i32].count(11i32)[12i32].count[13i32].count(14i32).count[15i32].count(16i32)[17i32].count[18i32].count(19i32)[20i32].count[21i32].count(22i32).count[23i32].count(24i32)[25i32].count[26i32].count(27i32)[28i32].count))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(
      transforms[0].arguments[0] ==
      "at(at(at(at(at(at(at(at(at(at(at(at(at(at(at(at(fetch_values(1i32).count(), 2i32).pick<i32>([index] 3i32) { foo(4i32) }, 5i32).count(6i32), 7i32).count(8i32), 9i32).count(), 10i32).count(11i32), 12i32).count(), 13i32).count(14i32).count(), 15i32).count(16i32), 17i32).count(), 18i32).count(19i32), 20i32).count(), 21i32).count(22i32).count(), 23i32).count(24i32), 25i32).count(), 26i32).count(27i32), 28i32).count()");
}

TEST_CASE(
    "parses semantic transform indexing after nested indexed template body chain indexed method-call field-access tail indexed method-call field-access index method-call field-access index method-call field-access index method-call field-access method-call field-access index") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>([index] 3i32) { foo(4i32) }[5i32].count(6i32)[7i32].count(8i32)[9i32].count[10i32].count(11i32)[12i32].count[13i32].count(14i32).count[15i32].count(16i32)[17i32].count[18i32].count(19i32)[20i32].count[21i32].count(22i32).count[23i32].count(24i32)[25i32].count[26i32].count(27i32)[28i32].count[29i32]))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(
      transforms[0].arguments[0] ==
      "at(at(at(at(at(at(at(at(at(at(at(at(at(at(at(at(at(fetch_values(1i32).count(), 2i32).pick<i32>([index] 3i32) { foo(4i32) }, 5i32).count(6i32), 7i32).count(8i32), 9i32).count(), 10i32).count(11i32), 12i32).count(), 13i32).count(14i32).count(), 15i32).count(16i32), 17i32).count(), 18i32).count(19i32), 20i32).count(), 21i32).count(22i32).count(), 23i32).count(24i32), 25i32).count(), 26i32).count(27i32), 28i32).count(), 29i32)");
}

TEST_CASE(
    "parses semantic transform method-call after nested indexed template body chain indexed method-call field-access tail indexed method-call field-access index method-call field-access index method-call field-access index method-call field-access index") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>([index] 3i32) { foo(4i32) }[5i32].count(6i32)[7i32].count(8i32)[9i32].count[10i32].count(11i32)[12i32].count[13i32].count(14i32).count[15i32].count(16i32)[17i32].count[18i32].count(19i32)[20i32].count[21i32].count(22i32).count[23i32].count(24i32)[25i32].count[26i32].count(27i32)[28i32].count[29i32].count(30i32)))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(
      transforms[0].arguments[0] ==
      "at(at(at(at(at(at(at(at(at(at(at(at(at(at(at(at(at(fetch_values(1i32).count(), 2i32).pick<i32>([index] 3i32) { foo(4i32) }, 5i32).count(6i32), 7i32).count(8i32), 9i32).count(), 10i32).count(11i32), 12i32).count(), 13i32).count(14i32).count(), 15i32).count(16i32), 17i32).count(), 18i32).count(19i32), 20i32).count(), 21i32).count(22i32).count(), 23i32).count(24i32), 25i32).count(), 26i32).count(27i32), 28i32).count(), 29i32).count(30i32)");
}

TEST_CASE(
    "parses semantic transform indexing after nested indexed template body chain indexed method-call field-access tail indexed method-call field-access index method-call field-access index method-call field-access index method-call field-access index method-call") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>([index] 3i32) { foo(4i32) }[5i32].count(6i32)[7i32].count(8i32)[9i32].count[10i32].count(11i32)[12i32].count[13i32].count(14i32).count[15i32].count(16i32)[17i32].count[18i32].count(19i32)[20i32].count[21i32].count(22i32).count[23i32].count(24i32)[25i32].count[26i32].count(27i32)[28i32].count[29i32].count(30i32)[31i32]))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(
      transforms[0].arguments[0] ==
      "at(at(at(at(at(at(at(at(at(at(at(at(at(at(at(at(at(at(fetch_values(1i32).count(), 2i32).pick<i32>([index] 3i32) { foo(4i32) }, 5i32).count(6i32), 7i32).count(8i32), 9i32).count(), 10i32).count(11i32), 12i32).count(), 13i32).count(14i32).count(), 15i32).count(16i32), 17i32).count(), 18i32).count(19i32), 20i32).count(), 21i32).count(22i32).count(), 23i32).count(24i32), 25i32).count(), 26i32).count(27i32), 28i32).count(), 29i32).count(30i32), 31i32)");
}

TEST_CASE(
    "parses semantic transform field-access after nested indexed template body chain method-call") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>([index] 3i32) { foo(4i32) }[5i32].count(6i32)[7i32].count))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(
      transforms[0].arguments[0] ==
      "at(at(at(fetch_values(1i32).count(), 2i32).pick<i32>([index] 3i32) { foo(4i32) }, 5i32).count(6i32), 7i32).count()");
}

TEST_CASE(
    "parses semantic transform field-access after nested indexed template body chain") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>([index] 3i32) { foo(4i32) }[5i32].count))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(
      transforms[0].arguments[0] ==
      "at(at(fetch_values(1i32).count(), 2i32).pick<i32>([index] 3i32) { foo(4i32) }, 5i32).count()");
}

TEST_CASE(
    "parses semantic transform indexing after nested indexed template body chain field-access") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>([index] 3i32) { foo(4i32) }[5i32].count[6i32]))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(
      transforms[0].arguments[0] ==
      "at(at(at(fetch_values(1i32).count(), 2i32).pick<i32>([index] 3i32) { foo(4i32) }, 5i32).count(), 6i32)");
}

TEST_CASE("parses semantic transform indexing full form on call result") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32)[2i32]))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] == "at(fetch_values(1i32), 2i32)");
}

TEST_CASE("parses semantic transform index then method-call on call result") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32)[2i32].count(3i32)))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] ==
        "at(fetch_values(1i32), 2i32).count(3i32)");
}

TEST_CASE(
    "parses semantic transform index then template method-call on call result") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32)[2i32].pick<i32>([index] 3i32)))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] ==
        "at(fetch_values(1i32), 2i32).pick<i32>([index] 3i32)");
}

TEST_CASE(
    "parses semantic transform index then template method-call without arguments") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32)[2i32].pick<i32>()))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] ==
        "at(fetch_values(1i32), 2i32).pick<i32>()");
}

TEST_CASE(
    "parses semantic transform index after indexed template method-call result") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32)[2i32].pick<i32>()[3i32]))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] ==
        "at(at(fetch_values(1i32), 2i32).pick<i32>(), 3i32)");
}

TEST_CASE("parses semantic transform index full form on template method chain") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).pick<i32>([index] 2i32)[3i32]))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] ==
        "at(fetch_values(1i32).pick<i32>([index] 2i32), 3i32)");
}

TEST_CASE("parses semantic transform method-call with body arguments") {
  const std::string source = R"(
[semantic(tag(values.count() { 1i32, foo(2i32) }))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] == "values.count() { 1i32, foo(2i32) }");
}

TEST_CASE("parses semantic transform method-call with named argument") {
  const std::string source = R"(
[semantic(tag(values.mix([ratio] 2i32)))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] == "values.mix([ratio] 2i32)");
}

TEST_CASE("parses semantic transform method-call with multiple arguments") {
  const std::string source = R"(
[semantic(tag(values.mix([ratio] 2i32, 3i32)))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] == "values.mix([ratio] 2i32, 3i32)");
}

TEST_CASE("parses semantic transform method-call with multiple named arguments") {
  const std::string source = R"(
[semantic(tag(values.mix([ratio] 2i32, [bias] 3i32)))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] == "values.mix([ratio] 2i32, [bias] 3i32)");
}

TEST_CASE("parses semantic transform method-call with template arguments") {
  const std::string source = R"(
[semantic(tag(values.pick<i32>([index] 1i32)))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] == "values.pick<i32>([index] 1i32)");
}

TEST_CASE(
    "parses semantic transform method-call with template arguments and no call arguments") {
  const std::string source = R"(
[semantic(tag(values.pick<i32>()))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] == "values.pick<i32>()");
}

TEST_CASE(
    "parses semantic transform method-call with template arguments and body") {
  const std::string source = R"(
[semantic(tag(values.pick<i32>([index] 1i32) { foo(2i32) }))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] ==
        "values.pick<i32>([index] 1i32) { foo(2i32) }");
}

TEST_CASE(
    "parses semantic transform method-call with call receiver and template args") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).pick<i32>([index] 2i32)))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] ==
        "fetch_values(1i32).pick<i32>([index] 2i32)");
}

TEST_CASE(
    "parses semantic transform method-call with multiple template arguments") {
  const std::string source = R"(
[semantic(tag(values.pick<i32, i64>([index] 1i32, 2i32)))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] ==
        "values.pick<i32, i64>([index] 1i32, 2i32)");
}

TEST_CASE(
    "parses semantic transform method-call with template named arguments") {
  const std::string source = R"(
[semantic(tag(values.pick<i32, i64>([index] 1i32, [fallback] 2i32)))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] ==
        "values.pick<i32, i64>([index] 1i32, [fallback] 2i32)");
}

TEST_CASE("parses semantic transform full form template call") {
  const std::string source = R"(
[semantic(tag(convert<i32>(2u64)))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] == "convert<i32>(2u64)");
}

TEST_CASE("parses semantic transform full form multi-template call") {
  const std::string source = R"(
[semantic(tag(make_pair<i32, i64>(1i32, 2i32)))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] == "make_pair<i32, i64>(1i32, 2i32)");
}

TEST_CASE("parses semantic transform full form string literal") {
  const std::string source = R"(
[semantic(tag("hello"utf8))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] == "\"hello\"utf8");
}

TEST_CASE("parses semantic transform full form false literal") {
  const std::string source = R"(
[semantic(tag(false))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] == "false");
}

TEST_CASE("parses semantic transform full form f64 literal") {
  const std::string source = R"(
[semantic(tag(2.5f64))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] == "2.5f64");
}

TEST_CASE("parses semantic transform full form u64 literal") {
  const std::string source = R"(
[semantic(tag(42u64))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] == "42u64");
}

TEST_CASE("parses semantic transform full form i64 literal") {
  const std::string source = R"(
[semantic(tag(7i64))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] == "7i64");
}

TEST_CASE("parses semantic transform full form brace call with multiple values") {
  const std::string source = R"(
[semantic(tag(value{1i32, foo(2i32)}))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] == "value(block() { 1i32, foo(2i32) })");
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
