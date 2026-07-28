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

primec::Expr makeScalarParam(const std::string &name, const std::string &typeName) {
  primec::Expr param;
  param.kind = primec::Expr::Kind::Name;
  param.name = name;
  param.isBinding = true;
  primec::Transform typeTransform;
  typeTransform.name = typeName;
  param.transforms.push_back(typeTransform);
  return param;
}

primec::Expr makeStructParam(const std::string &name, const std::string &structTypeName) {
  primec::Expr param;
  param.kind = primec::Expr::Kind::Name;
  param.name = name;
  param.isBinding = true;
  primec::Transform typeTransform;
  typeTransform.name = structTypeName;
  typeTransform.templateArgs = {"i32"};
  param.transforms.push_back(typeTransform);
  return param;
}

void addReturnTransform(primec::Definition &def, const std::string &typeName) {
  primec::Transform returnTransform;
  returnTransform.name = "return";
  returnTransform.templateArgs = {typeName};
  def.transforms.push_back(returnTransform);
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

TEST_CASE("reachability finds only definitions transitively called from entry") {
  primec::Program program;
  primec::Definition entry = makeDef("/main/entry");
  entry.statements.push_back(makeCall("/main/helper"));
  primec::Definition helper = makeDef("/main/helper");
  primec::Definition unreachable = makeDef("/main/unreachable");
  program.definitions = {entry, helper, unreachable};

  const auto reachable = primec::ir_lowerer::findReachableDefinitionPaths(program, "/main/entry");
  REQUIRE(reachable.size() == 2u);
  CHECK(reachable.count("/main/entry") == 1u);
  CHECK(reachable.count("/main/helper") == 1u);
  CHECK(reachable.count("/main/unreachable") == 0u);
}

TEST_CASE("reachability follows transitive and cyclic call chains") {
  primec::Program program;
  primec::Definition entry = makeDef("/main/entry");
  entry.statements.push_back(makeCall("/main/a"));
  primec::Definition a = makeDef("/main/a");
  a.statements.push_back(makeCall("/main/b"));
  primec::Definition b = makeDef("/main/b");
  b.statements.push_back(makeCall("/main/a"));
  program.definitions = {entry, a, b};

  const auto reachable = primec::ir_lowerer::findReachableDefinitionPaths(program, "/main/entry");
  REQUIRE(reachable.size() == 3u);
  CHECK(reachable.count("/main/entry") == 1u);
  CHECK(reachable.count("/main/a") == 1u);
  CHECK(reachable.count("/main/b") == 1u);
}

TEST_CASE("reachability returns empty set for an unknown entry path") {
  primec::Program program;
  program.definitions = {makeDef("/main/entry")};

  const auto reachable = primec::ir_lowerer::findReachableDefinitionPaths(program, "/main/missing");
  CHECK(reachable.empty());
}

TEST_CASE("eligibility accepts a scalar-parameter self-recursive definition") {
  primec::Program program;
  primec::Definition entry = makeDef("/main/entry");
  entry.statements.push_back(makeCall("/main/factorial"));
  primec::Definition factorial = makeDef("/main/factorial");
  factorial.parameters.push_back(makeScalarParam("n", "i32"));
  addReturnTransform(factorial, "i32");
  factorial.statements.push_back(makeCall("/main/factorial"));
  program.definitions = {entry, factorial};

  const auto eligible = primec::ir_lowerer::computeRealCallEligibleDefinitionPaths(program, "/main/entry");
  REQUIRE(eligible.size() == 1u);
  CHECK(eligible.count("/main/factorial") == 1u);
}

TEST_CASE("eligibility rejects a struct-parameter recursive definition") {
  primec::Program program;
  primec::Definition entry = makeDef("/main/entry");
  entry.statements.push_back(makeCall("/main/walk"));
  primec::Definition walk = makeDef("/main/walk");
  walk.parameters.push_back(makeStructParam("node", "/main/Node"));
  walk.statements.push_back(makeCall("/main/walk"));
  program.definitions = {entry, walk};

  const auto eligible = primec::ir_lowerer::computeRealCallEligibleDefinitionPaths(program, "/main/entry");
  CHECK(eligible.empty());
}

TEST_CASE("eligibility rejects a recursive definition with an on_error handler") {
  primec::Program program;
  primec::Definition entry = makeDef("/main/entry");
  entry.statements.push_back(makeCall("/main/risky"));
  primec::Definition risky = makeDef("/main/risky");
  risky.parameters.push_back(makeScalarParam("n", "i32"));
  addReturnTransform(risky, "i32");
  primec::Transform onError;
  onError.name = "on_error";
  risky.transforms.push_back(onError);
  risky.statements.push_back(makeCall("/main/risky"));
  program.definitions = {entry, risky};

  const auto eligible = primec::ir_lowerer::computeRealCallEligibleDefinitionPaths(program, "/main/entry");
  CHECK(eligible.empty());
}

TEST_CASE("eligibility rejects a recursive struct definition") {
  primec::Program program;
  primec::Definition entry = makeDef("/main/entry");
  entry.statements.push_back(makeCall("/main/Node"));
  primec::Definition node = makeDef("/main/Node");
  primec::Transform structTransform;
  structTransform.name = "struct";
  node.transforms.push_back(structTransform);
  node.statements.push_back(makeCall("/main/Node"));
  program.definitions = {entry, node};

  const auto eligible = primec::ir_lowerer::computeRealCallEligibleDefinitionPaths(program, "/main/entry");
  CHECK(eligible.empty());
}

TEST_CASE("eligibility excludes an unreachable recursive definition") {
  primec::Program program;
  primec::Definition entry = makeDef("/main/entry");
  primec::Definition factorial = makeDef("/main/factorial");
  factorial.parameters.push_back(makeScalarParam("n", "i32"));
  addReturnTransform(factorial, "i32");
  factorial.statements.push_back(makeCall("/main/factorial"));
  program.definitions = {entry, factorial};

  const auto eligible = primec::ir_lowerer::computeRealCallEligibleDefinitionPaths(program, "/main/entry");
  CHECK(eligible.empty());
}

TEST_CASE("eligibility excludes a non-recursive scalar definition") {
  primec::Program program;
  primec::Definition entry = makeDef("/main/entry");
  entry.statements.push_back(makeCall("/main/add_one"));
  primec::Definition addOne = makeDef("/main/add_one");
  addOne.parameters.push_back(makeScalarParam("n", "i32"));
  addReturnTransform(addOne, "i32");
  program.definitions = {entry, addOne};

  const auto eligible = primec::ir_lowerer::computeRealCallEligibleDefinitionPaths(program, "/main/entry");
  CHECK(eligible.empty());
}

TEST_CASE("eligibility accepts mutual recursion with void returns") {
  primec::Program program;
  primec::Definition entry = makeDef("/main/entry");
  entry.statements.push_back(makeCall("/main/is_even"));
  primec::Definition isEven = makeDef("/main/is_even");
  isEven.parameters.push_back(makeScalarParam("n", "i32"));
  addReturnTransform(isEven, "bool");
  isEven.statements.push_back(makeCall("/main/is_odd"));
  primec::Definition isOdd = makeDef("/main/is_odd");
  isOdd.parameters.push_back(makeScalarParam("n", "i32"));
  addReturnTransform(isOdd, "bool");
  isOdd.statements.push_back(makeCall("/main/is_even"));
  program.definitions = {entry, isEven, isOdd};

  const auto eligible = primec::ir_lowerer::computeRealCallEligibleDefinitionPaths(program, "/main/entry");
  REQUIRE(eligible.size() == 2u);
  CHECK(eligible.count("/main/is_even") == 1u);
  CHECK(eligible.count("/main/is_odd") == 1u);
}

TEST_CASE("eligibility excludes a self-recursive entry definition") {
  // The entry has its own dedicated lowering path (argc/argv binding,
  // whole-program validation) and is never one of the bodies the real-call
  // body-lowering loop iterates. Including it here would lower it twice
  // under the same name - see IrLowererLowerStatementsCallsStage.cpp's
  // "duplicate IR function name" failure this regression test guards
  // against. A self-recursive entry, otherwise eligible by shape (scalar
  // return, no parameters), must still be excluded.
  primec::Program program;
  primec::Definition entry = makeDef("/main/entry");
  addReturnTransform(entry, "i32");
  entry.statements.push_back(makeCall("/main/entry"));
  program.definitions = {entry};

  const auto eligible = primec::ir_lowerer::computeRealCallEligibleDefinitionPaths(program, "/main/entry");
  CHECK(eligible.empty());
}

TEST_SUITE_END();
