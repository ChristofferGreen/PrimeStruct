#pragma once

TEST_CASE("missing return statement fails") {
  primec::Program program;
  primec::Expr binding = makeBinding("value", {makeTransform("i32")}, {makeLiteral(1)});
  program.definitions.push_back(
      makeDefinition("/main", {makeTransform("return", std::string("int"))}, {binding}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("missing return statement") != std::string::npos);
}

TEST_CASE("method calls require a receiver") {
  primec::Program program;
  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.isMethodCall = true;
  methodCall.name = "count";
  primec::Expr returnCall = makeCall("/return", {methodCall});
  program.definitions.push_back(
      makeDefinition("/main", {makeTransform("return", std::string("int"))}, {returnCall}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("method call missing receiver") != std::string::npos);
}

TEST_CASE("recursive return inference requires annotation") {
  const std::string source = R"(
main() {
  return(main())
}
)";
  std::string error;
  CHECK_FALSE(validateSourceProgram(source, "/main", error));
  CHECK(error.find("/main") != std::string::npos);
}

TEST_CASE("named arguments not allowed on builtin calls in manual AST") {
  primec::Program program;
  primec::Expr plusCall = makeCall("plus", {makeLiteral(1), makeLiteral(2)},
                                  {std::string("left"), std::string("right")});
  program.definitions.push_back(makeDefinition(
      "/main", {makeTransform("return", std::string("int"))}, {makeCall("/return", {plusCall})}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_CASE("execution unknown named argument fails in manual AST") {
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

TEST_CASE("duplicate parameter name fails") {
  primec::Program program;
  program.definitions.push_back(makeDefinition(
      "/main", {makeTransform("return", std::string("int"))}, {makeCall("/return", {makeLiteral(1)})},
      {makeParameter("value"), makeParameter("value")}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("duplicate parameter") != std::string::npos);
}

TEST_CASE("parameters must use binding syntax") {
  primec::Program program;
  primec::Expr param = makeName("value");
  program.definitions.push_back(makeDefinition(
      "/main", {makeTransform("return", std::string("int"))}, {makeCall("/return", {makeLiteral(1)})}, {param}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("parameters must use binding syntax") != std::string::npos);
}

TEST_CASE("parameters reject block arguments") {
  primec::Program program;
  primec::Expr param = makeParameter("value");
  param.hasBodyArguments = true;
  param.bodyArguments = {makeLiteral(1)};
  program.definitions.push_back(makeDefinition(
      "/main", {makeTransform("return", std::string("int"))}, {makeCall("/return", {makeLiteral(1)})}, {param}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("parameter does not accept block arguments") != std::string::npos);
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

TEST_CASE("positional arguments may follow named arguments") {
  primec::Program program;
  program.definitions.push_back(
      makeDefinition("/callee", {makeTransform("return", std::string("int"))}, {makeCall("/return", {makeLiteral(1)})},
                     {makeParameter("a"), makeParameter("b")}));
  primec::Expr call = makeCall("/callee", {makeLiteral(1), makeLiteral(2)});
  call.argNames = {std::string("a"), std::nullopt};
  program.definitions.push_back(makeDefinition(
      "/main", {makeTransform("return", std::string("int"))}, {makeCall("/return", {call})}));
  std::string error;
  CHECK(validateProgram(program, "/main", error));
  CHECK(error.empty());
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
  CHECK(error.find("array<T, N> is unsupported; use array<T> (runtime-count array)") !=
        std::string::npos);
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

TEST_CASE("execution positional argument after named is allowed") {
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
  CHECK(validateProgram(program, "/main", error));
  CHECK(error.empty());
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

