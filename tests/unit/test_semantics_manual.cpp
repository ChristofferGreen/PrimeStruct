#include "primec/Ast.h"
#include "primec/Semantics.h"

#include "third_party/doctest.h"

namespace {
primec::Expr makeLiteral(int value) {
  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Literal;
  expr.literalValue = value;
  return expr;
}

primec::Expr makeBool(bool value) {
  primec::Expr expr;
  expr.kind = primec::Expr::Kind::BoolLiteral;
  expr.boolValue = value;
  return expr;
}

primec::Transform makeTransform(const std::string &name,
                                std::optional<std::string> templateArg = std::nullopt) {
  primec::Transform transform;
  transform.name = name;
  transform.templateArg = std::move(templateArg);
  return transform;
}

primec::Expr makeCall(const std::string &name,
                      std::vector<primec::Expr> args = {},
                      std::vector<std::optional<std::string>> argNames = {},
                      std::vector<primec::Expr> bodyArguments = {}) {
  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.name = name;
  expr.args = std::move(args);
  expr.argNames = std::move(argNames);
  expr.bodyArguments = std::move(bodyArguments);
  return expr;
}

primec::Expr makeBinding(const std::string &name,
                         std::vector<primec::Transform> transforms,
                         std::vector<primec::Expr> args) {
  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.isBinding = true;
  expr.name = name;
  expr.transforms = std::move(transforms);
  expr.args = std::move(args);
  return expr;
}

primec::Expr makeParameter(const std::string &name, const std::string &type = "i32") {
  return makeBinding(name, {makeTransform(type)}, {});
}

primec::Definition makeDefinition(const std::string &fullPath,
                                  std::vector<primec::Transform> transforms,
                                  std::vector<primec::Expr> statements,
                                  std::vector<primec::Expr> parameters = {}) {
  primec::Definition def;
  def.fullPath = fullPath;
  def.name = fullPath;
  def.transforms = std::move(transforms);
  def.statements = std::move(statements);
  def.parameters = std::move(parameters);
  return def;
}

bool validateProgram(const primec::Program &program, const std::string &entry, std::string &error) {
  primec::Semantics semantics;
  return semantics.validate(program, entry, error, {});
}
} // namespace

TEST_SUITE_BEGIN("primestruct.semantics.manual");

TEST_CASE("duplicate definitions fail") {
  primec::Program program;
  program.definitions.push_back(
      makeDefinition("/main", {makeTransform("return", std::string("int"))},
                     {makeCall("/return", {makeLiteral(1)})}));
  program.definitions.push_back(
      makeDefinition("/main", {makeTransform("return", std::string("int"))},
                     {makeCall("/return", {makeLiteral(2)})}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("duplicate definition") != std::string::npos);
}

TEST_CASE("return transform requires template argument") {
  primec::Program program;
  program.definitions.push_back(
      makeDefinition("/main", {makeTransform("return")}, {makeCall("/return", {makeLiteral(1)})}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("return transform requires a type") != std::string::npos);
}

TEST_CASE("return transform rejects arguments") {
  primec::Program program;
  primec::Transform transform = makeTransform("return", std::string("int"));
  transform.arguments = {"bad"};
  program.definitions.push_back(makeDefinition("/main", {transform}, {makeCall("/return", {makeLiteral(1)})}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("return transform does not accept arguments") != std::string::npos);
}

TEST_CASE("conflicting return types fail") {
  primec::Program program;
  program.definitions.push_back(makeDefinition(
      "/main",
      {makeTransform("return", std::string("int")), makeTransform("return", std::string("float"))},
      {makeCall("/return", {makeLiteral(1)})}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("conflicting return types") != std::string::npos);
}

TEST_CASE("duplicate return transforms fail") {
  primec::Program program;
  program.definitions.push_back(makeDefinition(
      "/main",
      {makeTransform("return", std::string("int")), makeTransform("return", std::string("i32"))},
      {makeCall("/return", {makeLiteral(1)})}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("duplicate return transform") != std::string::npos);
}

TEST_CASE("binding transform template arguments fail") {
  primec::Program program;
  primec::Expr binding =
      makeBinding("value", {makeTransform("i32", std::string("bad"))}, {makeLiteral(1)});
  program.definitions.push_back(
      makeDefinition("/main", {makeTransform("return", std::string("int"))},
                     {binding, makeCall("/return", {makeLiteral(1)})}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("binding transforms do not take template arguments") != std::string::npos);
}

TEST_CASE("binding requires exactly one type") {
  primec::Program program;
  primec::Expr binding = makeBinding(
      "value", {makeTransform("i32"), makeTransform("f32")}, {makeLiteral(1)});
  program.definitions.push_back(
      makeDefinition("/main", {makeTransform("return", std::string("int"))},
                     {binding, makeCall("/return", {makeLiteral(1)})}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("binding requires exactly one type") != std::string::npos);
}

TEST_CASE("binding defaults to int when omitted") {
  primec::Program program;
  primec::Expr binding = makeBinding("value", {makeTransform("mut")}, {makeLiteral(1)});
  program.definitions.push_back(
      makeDefinition("/main", {makeTransform("return", std::string("int"))},
                     {binding, makeCall("/return", {makeLiteral(1)})}));
  std::string error;
  CHECK(validateProgram(program, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("return blocks are rejected") {
  primec::Program program;
  primec::Expr returnCall = makeCall("/return", {makeLiteral(1)}, {}, {makeLiteral(2)});
  program.definitions.push_back(
      makeDefinition("/main", {makeTransform("return", std::string("int"))}, {returnCall}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("return does not accept block arguments") != std::string::npos);
}

TEST_CASE("if trailing blocks are rejected") {
  primec::Program program;
  primec::Expr thenBlock = makeCall("then", {}, {}, {makeCall("/return", {makeLiteral(1)})});
  primec::Expr elseBlock = makeCall("else", {}, {}, {makeCall("/return", {makeLiteral(2)})});
  primec::Expr ifCall = makeCall("if", {makeBool(true), thenBlock, elseBlock}, {}, {makeLiteral(3)});
  program.definitions.push_back(
      makeDefinition("/main", {makeTransform("return", std::string("int"))}, {ifCall}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("if does not accept trailing block arguments") != std::string::npos);
}

TEST_CASE("if block wrappers do not require special names") {
  primec::Program program;
  primec::Expr thenBlock = makeCall("wrong", {}, {}, {makeCall("/return", {makeLiteral(1)})});
  primec::Expr elseBlock = makeCall("also_wrong", {}, {}, {makeCall("/return", {makeLiteral(2)})});
  primec::Expr ifCall = makeCall("if", {makeBool(true), thenBlock, elseBlock});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("int"))},
                                               {ifCall}));
  std::string error;
  CHECK(validateProgram(program, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("block arguments allowed on statement calls") {
  primec::Program program;
  primec::Expr binding = makeBinding("value", {makeTransform("i32")}, {makeLiteral(1)});
  primec::Expr callWithBlock = makeCall("foo", {}, {}, {binding});
  program.definitions.push_back(
      makeDefinition("/foo", {makeTransform("return", std::string("void"))}, {}));
  program.definitions.push_back(
      makeDefinition("/main", {makeTransform("return", std::string("int"))},
                     {callWithBlock, makeCall("/return", {makeLiteral(1)})}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("binding not allowed in execution body") != std::string::npos);
}

TEST_CASE("literal statements are allowed") {
  primec::Program program;
  primec::Expr literal = makeLiteral(1);
  primec::Expr returnCall = makeCall("/return", {makeLiteral(1)});
  program.definitions.push_back(
      makeDefinition("/main", {makeTransform("return", std::string("int"))}, {literal, returnCall}));
  std::string error;
  CHECK(validateProgram(program, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("assign target must be a mutable binding") {
  primec::Program program;
  primec::Expr assignCall = makeCall("assign", {makeCall("foo"), makeLiteral(1)});
  program.definitions.push_back(
      makeDefinition("/main", {makeTransform("return", std::string("int"))},
                     {assignCall, makeCall("/return", {makeLiteral(1)})}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("assign target must be a mutable binding") != std::string::npos);
}

TEST_CASE("execution argument name count mismatch fails") {
  primec::Program program;
  program.definitions.push_back(
      makeDefinition("/main", {makeTransform("return", std::string("int"))},
                     {makeCall("/return", {makeLiteral(1)})}));
  program.definitions.push_back(makeDefinition(
      "/task", {makeTransform("return", std::string("int"))}, {makeCall("/return", {makeLiteral(1)})},
      {makeParameter("value")}));

  primec::Execution exec;
  exec.fullPath = "/task";
  exec.arguments = {makeLiteral(1)};
  exec.argumentNames = {std::string("value"), std::string("extra")};
  program.executions.push_back(exec);

  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("argument name count mismatch") != std::string::npos);
}

TEST_CASE("execution return transform rejects") {
  primec::Program program;
  program.definitions.push_back(
      makeDefinition("/main", {makeTransform("return", std::string("int"))},
                     {makeCall("/return", {makeLiteral(1)})}));
  program.definitions.push_back(makeDefinition(
      "/task", {makeTransform("return", std::string("int"))}, {makeCall("/return", {makeLiteral(1)})}));

  primec::Execution exec;
  exec.fullPath = "/task";
  exec.arguments = {makeLiteral(1)};
  exec.transforms = {makeTransform("return", std::string("int"))};
  program.executions.push_back(exec);

  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("return transform not allowed on executions") != std::string::npos);
}

TEST_CASE("duplicate binding names fail") {
  primec::Program program;
  primec::Expr bindingA = makeBinding("value", {makeTransform("i32")}, {makeLiteral(1)});
  primec::Expr bindingB = makeBinding("value", {makeTransform("i32")}, {makeLiteral(2)});
  program.definitions.push_back(makeDefinition(
      "/main", {makeTransform("return", std::string("int"))},
      {bindingA, bindingB, makeCall("/return", {makeLiteral(1)})}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("duplicate binding name") != std::string::npos);
}

TEST_CASE("binding cannot shadow parameter") {
  primec::Program program;
  primec::Expr binding = makeBinding("value", {makeTransform("i32")}, {makeLiteral(1)});
  program.definitions.push_back(makeDefinition(
      "/main", {makeTransform("return", std::string("int"))},
      {binding, makeCall("/return", {makeLiteral(1)})}, {makeParameter("value")}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("duplicate binding name") != std::string::npos);
}

TEST_CASE("unknown call target fails") {
  primec::Program program;
  primec::Expr unknownCall = makeCall("mystery", {makeLiteral(1)});
  program.definitions.push_back(makeDefinition(
      "/main", {makeTransform("return", std::string("int"))},
      {unknownCall, makeCall("/return", {makeLiteral(1)})}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("unknown call target") != std::string::npos);
}

TEST_CASE("return not allowed in expression context") {
  primec::Program program;
  primec::Expr innerReturn = makeCall("return", {makeLiteral(1)});
  primec::Expr plusCall = makeCall("plus", {innerReturn, makeLiteral(2)});
  program.definitions.push_back(makeDefinition(
      "/main", {makeTransform("return", std::string("int"))}, {makeCall("/return", {plusCall})}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("return not allowed in expression context") != std::string::npos);
}

TEST_CASE("empty block arguments not allowed in expressions") {
  primec::Program program;
  primec::Expr blockCall = makeCall("task");
  blockCall.hasBodyArguments = true;
  program.definitions.push_back(makeDefinition(
      "/main", {makeTransform("return", std::string("int"))}, {makeCall("/return", {blockCall})}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("block arguments are only supported on statement calls") != std::string::npos);
}

TEST_CASE("block arguments not allowed in expression context") {
  primec::Program program;
  primec::Expr blockCall = makeCall("task", {}, {}, {makeLiteral(1)});
  program.definitions.push_back(makeDefinition(
      "/main", {makeTransform("return", std::string("int"))}, {makeCall("/return", {blockCall})}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("block arguments are only supported on statement calls") != std::string::npos);
}

TEST_CASE("named arguments not allowed on builtin calls") {
  primec::Program program;
  primec::Expr plusCall = makeCall("plus", {makeLiteral(1), makeLiteral(2)},
                                  {std::string("left"), std::string("right")});
  program.definitions.push_back(makeDefinition(
      "/main", {makeTransform("return", std::string("int"))}, {makeCall("/return", {plusCall})}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_CASE("execution unknown named argument fails") {
  primec::Program program;
  program.definitions.push_back(
      makeDefinition("/main", {makeTransform("return", std::string("int"))},
                     {makeCall("/return", {makeLiteral(1)})}));
  program.definitions.push_back(makeDefinition(
      "/task", {makeTransform("return", std::string("int"))}, {makeCall("/return", {makeLiteral(1)})},
      {makeParameter("a"), makeParameter("b")}));

  primec::Execution exec;
  exec.fullPath = "/task";
  exec.arguments = {makeLiteral(1), makeLiteral(2)};
  exec.argumentNames = {std::string("c"), std::string("b")};
  program.executions.push_back(exec);

  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("unknown named argument") != std::string::npos);
}

TEST_CASE("call argument name count mismatch fails") {
  primec::Program program;
  primec::Expr call = makeCall("callee", {makeLiteral(1)});
  call.argNames = {std::string("a"), std::string("b")};
  program.definitions.push_back(makeDefinition(
      "/main", {makeTransform("return", std::string("int"))}, {makeCall("/return", {call})}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("argument name count mismatch") != std::string::npos);
}

TEST_CASE("positional argument after named fails") {
  primec::Program program;
  primec::Expr call = makeCall("callee", {makeLiteral(1), makeLiteral(2)});
  call.argNames = {std::string("a"), std::nullopt};
  program.definitions.push_back(makeDefinition(
      "/main", {makeTransform("return", std::string("int"))}, {makeCall("/return", {call})}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("positional argument cannot follow named arguments") != std::string::npos);
}

TEST_CASE("assign requires exactly two arguments") {
  primec::Program program;
  primec::Expr assignCall = makeCall("assign", {makeLiteral(1)});
  program.definitions.push_back(makeDefinition(
      "/main", {makeTransform("return", std::string("int"))},
      {assignCall, makeCall("/return", {makeLiteral(1)})}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("assign requires exactly two arguments") != std::string::npos);
}

TEST_CASE("convert requires exactly one argument") {
  primec::Program program;
  primec::Expr convertCall = makeCall("convert");
  convertCall.templateArgs = {"int"};
  program.definitions.push_back(makeDefinition(
      "/main", {makeTransform("return", std::string("int"))}, {makeCall("/return", {convertCall})}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("argument count mismatch for builtin convert") != std::string::npos);
}

TEST_CASE("array literal requires exactly one template argument") {
  primec::Program program;
  primec::Expr arrayCall = makeCall("array", {makeLiteral(1)});
  arrayCall.templateArgs = {"i32", "i32"};
  program.definitions.push_back(makeDefinition(
      "/main", {makeTransform("return", std::string("int"))}, {makeCall("/return", {arrayCall})}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("array literal requires exactly one template argument") != std::string::npos);
}

TEST_CASE("if expression blocks require a value expression") {
  primec::Program program;
  primec::Expr thenBlock = makeCall("block");
  thenBlock.hasBodyArguments = true;
  primec::Expr elseBlock = makeCall("block", {}, {}, {makeLiteral(3)});
  primec::Expr ifCall = makeCall("if", {makeBool(true), thenBlock, elseBlock});
  program.definitions.push_back(makeDefinition(
      "/main", {makeTransform("return", std::string("int"))}, {makeCall("/return", {ifCall})}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("then block must produce a value") != std::string::npos);
}

TEST_CASE("if statement allows empty branch blocks") {
  primec::Program program;
  primec::Expr thenBlock = makeCall("then");
  thenBlock.hasBodyArguments = true;
  primec::Expr elseBlock = makeCall("else");
  elseBlock.hasBodyArguments = true;
  primec::Expr ifCall = makeCall("if", {makeBool(true), thenBlock, elseBlock});
  program.definitions.push_back(makeDefinition(
      "/main",
      {makeTransform("return", std::string("int"))},
      {ifCall, makeCall("/return", {makeLiteral(1)})}));
  std::string error;
  CHECK(validateProgram(program, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("execution positional argument after named fails") {
  primec::Program program;
  program.definitions.push_back(
      makeDefinition("/main", {makeTransform("return", std::string("int"))},
                     {makeCall("/return", {makeLiteral(1)})}));
  program.definitions.push_back(makeDefinition(
      "/task", {makeTransform("return", std::string("int"))}, {makeCall("/return", {makeLiteral(1)})},
      {makeParameter("a"), makeParameter("b")}));

  primec::Execution exec;
  exec.fullPath = "/task";
  exec.arguments = {makeLiteral(1), makeLiteral(2)};
  exec.argumentNames = {std::string("a"), std::nullopt};
  program.executions.push_back(exec);

  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("positional argument cannot follow named arguments") != std::string::npos);
}

TEST_CASE("execution named arguments reorder") {
  primec::Program program;
  program.definitions.push_back(
      makeDefinition("/main", {makeTransform("return", std::string("int"))},
                     {makeCall("/return", {makeLiteral(1)})}));
  program.definitions.push_back(makeDefinition(
      "/task", {makeTransform("return", std::string("int"))}, {makeCall("/return", {makeLiteral(1)})},
      {makeParameter("a"), makeParameter("b")}));

  primec::Execution exec;
  exec.fullPath = "/task";
  exec.arguments = {makeLiteral(2), makeLiteral(1)};
  exec.argumentNames = {std::string("b"), std::string("a")};
  program.executions.push_back(exec);

  std::string error;
  CHECK(validateProgram(program, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("execution duplicate named arguments fail") {
  primec::Program program;
  program.definitions.push_back(
      makeDefinition("/main", {makeTransform("return", std::string("int"))},
                     {makeCall("/return", {makeLiteral(1)})}));
  program.definitions.push_back(makeDefinition(
      "/task", {makeTransform("return", std::string("int"))}, {makeCall("/return", {makeLiteral(1)})},
      {makeParameter("a"), makeParameter("b")}));

  primec::Execution exec;
  exec.fullPath = "/task";
  exec.arguments = {makeLiteral(1), makeLiteral(2)};
  exec.argumentNames = {std::string("a"), std::string("a")};
  program.executions.push_back(exec);

  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("duplicate named argument") != std::string::npos);
}

TEST_CASE("execution arguments accept collection literals") {
  primec::Program program;
  program.definitions.push_back(
      makeDefinition("/main", {makeTransform("return", std::string("int"))},
                     {makeCall("/return", {makeLiteral(1)})}));
  program.definitions.push_back(makeDefinition(
      "/task", {makeTransform("return", std::string("int"))}, {makeCall("/return", {makeLiteral(1)})},
      {makeParameter("items"), makeParameter("pairs")}));

  primec::Expr arrayCall = makeCall("array", {makeLiteral(1), makeLiteral(2)});
  arrayCall.templateArgs = {"i32"};
  primec::Expr mapCall = makeCall("map", {makeLiteral(1), makeLiteral(10), makeLiteral(2), makeLiteral(20)});
  mapCall.templateArgs = {"i32", "i32"};

  primec::Execution exec;
  exec.fullPath = "/task";
  exec.arguments = {arrayCall, mapCall};
  exec.argumentNames = {std::string("items"), std::string("pairs")};
  program.executions.push_back(exec);

  std::string error;
  CHECK(validateProgram(program, "/main", error));
  CHECK(error.empty());
}

TEST_SUITE_END();
