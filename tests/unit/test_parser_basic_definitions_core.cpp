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

TEST_CASE("parses definition with omitted parameter envelopes") {
  const std::string source = R"(
run_countdown(start) {
  return(start)
}
)";

  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  CHECK(program.definitions[0].fullPath == "/run_countdown");
  REQUIRE(program.definitions[0].parameters.size() == 1);
  const auto &parameter = program.definitions[0].parameters[0];
  CHECK(parameter.isBinding);
  CHECK(parameter.name == "start");
  CHECK(parameter.transforms.empty());
  CHECK(parameter.args.empty());
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

TEST_CASE("parses sum declaration variants in source order") {
  const std::string source = R"(
[sum]
Shape {
  none
  [Circle] circle
  [Rectangle] rectangle
  [Label] labeled
}
)";

  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &shape = program.definitions[0];
  CHECK(shape.fullPath == "/Shape");
  REQUIRE(shape.transforms.size() == 1);
  CHECK(shape.transforms[0].name == "sum");
  REQUIRE(shape.sumVariants.size() == 4);
  CHECK(shape.sumVariants[0].name == "none");
  CHECK_FALSE(shape.sumVariants[0].hasPayload);
  CHECK(shape.sumVariants[0].payloadTypeText.empty());
  CHECK(shape.sumVariants[0].variantIndex == 0);
  CHECK(shape.sumVariants[1].name == "circle");
  CHECK(shape.sumVariants[1].hasPayload);
  CHECK(shape.sumVariants[1].payloadTypeText == "Circle");
  CHECK(shape.sumVariants[1].variantIndex == 1);
  CHECK(shape.sumVariants[2].name == "rectangle");
  CHECK(shape.sumVariants[2].payloadTypeText == "Rectangle");
  CHECK(shape.sumVariants[2].variantIndex == 2);
  CHECK(shape.sumVariants[3].name == "labeled");
  CHECK(shape.sumVariants[3].payloadTypeText == "Label");
  CHECK(shape.sumVariants[3].variantIndex == 3);
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
  CHECK(pickCall.isBraceConstructor);
  REQUIRE(pickCall.args.size() == 1);
  const auto &blockArg = pickCall.args[0];
  REQUIRE(blockArg.kind == primec::Expr::Kind::Call);
  CHECK(blockArg.name == "block");
  CHECK(blockArg.hasBodyArguments);
  REQUIRE(blockArg.bodyArguments.size() == 1);
  CHECK(blockArg.bodyArguments[0].kind == primec::Expr::Kind::Literal);
}

TEST_CASE("parses brace constructor argument lists") {
  const std::string source = R"(
[return<int>]
main() {
  return(Pair{[right] 2i32, [left] 1i32})
}
)";

  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &mainDef = program.definitions[0];
  REQUIRE(mainDef.statements.size() == 1);
  const auto &returnCall = mainDef.statements[0];
  REQUIRE(returnCall.kind == primec::Expr::Kind::Call);
  REQUIRE(returnCall.args.size() == 1);
  const auto &pairCtor = returnCall.args[0];
  REQUIRE(pairCtor.kind == primec::Expr::Kind::Call);
  CHECK(pairCtor.name == "Pair");
  CHECK(pairCtor.isBraceConstructor);
  REQUIRE(pairCtor.args.size() == 2);
  REQUIRE(pairCtor.argNames.size() == 2);
  REQUIRE(pairCtor.argNames[0].has_value());
  REQUIRE(pairCtor.argNames[1].has_value());
  CHECK(*pairCtor.argNames[0] == "right");
  CHECK(*pairCtor.argNames[1] == "left");
  CHECK(pairCtor.args[0].kind == primec::Expr::Kind::Literal);
  CHECK(pairCtor.args[1].kind == primec::Expr::Kind::Literal);
}

TEST_CASE("parses positional brace constructor argument lists") {
  const std::string source = R"(
[return<int>]
main() {
  return(Pair{1i32, 2i32})
}
)";

  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &mainDef = program.definitions[0];
  REQUIRE(mainDef.statements.size() == 1);
  const auto &returnCall = mainDef.statements[0];
  REQUIRE(returnCall.kind == primec::Expr::Kind::Call);
  REQUIRE(returnCall.args.size() == 1);
  const auto &pairCtor = returnCall.args[0];
  REQUIRE(pairCtor.kind == primec::Expr::Kind::Call);
  CHECK(pairCtor.name == "Pair");
  CHECK(pairCtor.isBraceConstructor);
  REQUIRE(pairCtor.args.size() == 2);
  REQUIRE(pairCtor.argNames.size() == 2);
  CHECK_FALSE(pairCtor.argNames[0].has_value());
  CHECK_FALSE(pairCtor.argNames[1].has_value());
  CHECK(pairCtor.args[0].kind == primec::Expr::Kind::Literal);
  CHECK(pairCtor.args[1].kind == primec::Expr::Kind::Literal);
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

TEST_CASE("parses labeled struct-literal local binding as constructor initializer") {
  const std::string source = R"(
[struct]
Pair {
  [int] left{0i32}
  [int] right{0i32}
}

[int]
sum_pair() {
  [Pair] pair{[left] 4i32, [right] 8i32}
  return(pair.left)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 2);
  const auto &sumPairDef = program.definitions[1];
  REQUIRE(sumPairDef.statements.size() == 2);
  const auto &binding = sumPairDef.statements[0];
  REQUIRE(binding.isBinding);
  REQUIRE(binding.args.size() == 1);
  REQUIRE(binding.argNames.size() == 1);
  CHECK_FALSE(binding.argNames[0].has_value());

  const auto &initializer = binding.args[0];
  REQUIRE(initializer.kind == primec::Expr::Kind::Call);
  CHECK(initializer.name == "Pair");
  REQUIRE(initializer.args.size() == 2);
  REQUIRE(initializer.argNames.size() == 2);
  REQUIRE(initializer.argNames[0].has_value());
  REQUIRE(initializer.argNames[1].has_value());
  CHECK(*initializer.argNames[0] == "left");
  CHECK(*initializer.argNames[1] == "right");
  CHECK(initializer.args[0].kind == primec::Expr::Kind::Literal);
  CHECK(initializer.args[1].kind == primec::Expr::Kind::Literal);
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

TEST_SUITE_END();
