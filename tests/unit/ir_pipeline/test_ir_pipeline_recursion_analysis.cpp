#include "test_ir_pipeline_validation_helpers.h"

#include "src/ir_lowerer/IrLowererRecursionAnalysis.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

namespace {
primec::Definition makeDef(const std::string &fullPath) {
  primec::Definition def;
  def.fullPath = fullPath;
  return def;
}

primec::Expr makeCall(const std::string &resolvedCallPath) {
  primec::Expr call;
  call.kind = primec::Expr::Kind::Call;
  call.resolvedCallPath = resolvedCallPath;
  return call;
}
} // namespace

TEST_CASE("recursion analysis finds no cycles in a plain call chain") {
  primec::Program program;
  primec::Definition a = makeDef("/main/a");
  a.statements.push_back(makeCall("/main/b"));
  primec::Definition b = makeDef("/main/b");
  b.statements.push_back(makeCall("/main/c"));
  primec::Definition c = makeDef("/main/c");
  program.definitions = {a, b, c};

  const auto recursive = primec::ir_lowerer::findRecursiveDefinitionPaths(program);
  CHECK(recursive.empty());
}

TEST_CASE("recursion analysis finds direct self recursion") {
  primec::Program program;
  primec::Definition factorial = makeDef("/main/factorial");
  factorial.statements.push_back(makeCall("/main/factorial"));
  program.definitions = {factorial};

  const auto recursive = primec::ir_lowerer::findRecursiveDefinitionPaths(program);
  REQUIRE(recursive.size() == 1u);
  CHECK(recursive.count("/main/factorial") == 1u);
}

TEST_CASE("recursion analysis finds mutual recursion across a cycle") {
  primec::Program program;
  primec::Definition isEven = makeDef("/main/is_even");
  isEven.statements.push_back(makeCall("/main/is_odd"));
  primec::Definition isOdd = makeDef("/main/is_odd");
  isOdd.statements.push_back(makeCall("/main/is_even"));
  program.definitions = {isEven, isOdd};

  const auto recursive = primec::ir_lowerer::findRecursiveDefinitionPaths(program);
  REQUIRE(recursive.size() == 2u);
  CHECK(recursive.count("/main/is_even") == 1u);
  CHECK(recursive.count("/main/is_odd") == 1u);
}

TEST_CASE("recursion analysis only flags definitions actually on the cycle") {
  primec::Program program;
  primec::Definition entry = makeDef("/main/entry");
  entry.statements.push_back(makeCall("/main/helper"));
  entry.statements.push_back(makeCall("/main/cycleA"));
  primec::Definition helper = makeDef("/main/helper");
  primec::Definition cycleA = makeDef("/main/cycleA");
  cycleA.statements.push_back(makeCall("/main/cycleB"));
  primec::Definition cycleB = makeDef("/main/cycleB");
  cycleB.statements.push_back(makeCall("/main/cycleA"));
  program.definitions = {entry, helper, cycleA, cycleB};

  const auto recursive = primec::ir_lowerer::findRecursiveDefinitionPaths(program);
  REQUIRE(recursive.size() == 2u);
  CHECK(recursive.count("/main/cycleA") == 1u);
  CHECK(recursive.count("/main/cycleB") == 1u);
  CHECK(recursive.count("/main/entry") == 0u);
  CHECK(recursive.count("/main/helper") == 0u);
}

TEST_CASE("recursion analysis walks nested call expressions and body arguments") {
  primec::Program program;
  primec::Definition loopy = makeDef("/main/loopy");
  primec::Expr whileStmt;
  whileStmt.kind = primec::Expr::Kind::Call;
  whileStmt.name = "while";
  primec::Expr nestedCall = makeCall("/main/loopy");
  primec::Expr wrapper;
  wrapper.kind = primec::Expr::Kind::Call;
  wrapper.name = "identity";
  wrapper.args.push_back(nestedCall);
  whileStmt.bodyArguments.push_back(wrapper);
  loopy.statements.push_back(whileStmt);
  program.definitions = {loopy};

  const auto recursive = primec::ir_lowerer::findRecursiveDefinitionPaths(program);
  REQUIRE(recursive.size() == 1u);
  CHECK(recursive.count("/main/loopy") == 1u);
}

TEST_CASE("recursion analysis checks the return expression too") {
  primec::Program program;
  primec::Definition fib = makeDef("/main/fib");
  fib.returnExpr = makeCall("/main/fib");
  program.definitions = {fib};

  const auto recursive = primec::ir_lowerer::findRecursiveDefinitionPaths(program);
  REQUIRE(recursive.size() == 1u);
  CHECK(recursive.count("/main/fib") == 1u);
}

TEST_CASE("recursion analysis ignores unresolved and external call paths") {
  primec::Program program;
  primec::Definition entry = makeDef("/main/entry");
  primec::Expr unresolvedCall;
  unresolvedCall.kind = primec::Expr::Kind::Call;
  entry.statements.push_back(unresolvedCall);
  entry.statements.push_back(makeCall("/std/io/print_line"));
  program.definitions = {entry};

  const auto recursive = primec::ir_lowerer::findRecursiveDefinitionPaths(program);
  CHECK(recursive.empty());
}

TEST_SUITE_END();
