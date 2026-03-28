#pragma once

TEST_CASE("uninitialized not allowed in array element types") {
  primec::Program program;
  primec::Expr init = makeCall("array");
  init.templateArgs = {"uninitialized<i32>"};
  primec::Expr binding = makeBinding("values", {makeTransform("array", std::string("uninitialized<i32>"))}, {init});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {binding, makeCall("/return")}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("uninitialized storage is not allowed in array element types") != std::string::npos);
}

TEST_CASE("uninitialized not allowed as user template arg") {
  primec::Program program;
  primec::Expr init = makeCall("Widget");
  init.templateArgs = {"uninitialized<i32>"};
  primec::Expr binding = makeBinding("value", {makeTransform("Widget", std::string("uninitialized<i32>"))}, {init});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {binding, makeCall("/return")}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("uninitialized storage is not allowed as template argument to user-defined types") !=
        std::string::npos);
}

TEST_CASE("uninitialized canonical map template arg keeps map key value diagnostics") {
  primec::Program program;
  primec::Transform canonicalMapTransform;
  canonicalMapTransform.name = "/std/collections/map";
  canonicalMapTransform.templateArgs = {"i32", "uninitialized<i32>"};

  primec::Expr init = makeCall("map");
  init.templateArgs = {"i32", "uninitialized<i32>"};
  primec::Expr binding = makeBinding("value", {canonicalMapTransform}, {init});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {binding, makeCall("/return")}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("uninitialized storage is not allowed in map key/value types") != std::string::npos);
}

TEST_CASE("canonical map template arg without uninitialized validates") {
  primec::Program program;
  primec::Transform canonicalMapTransform;
  canonicalMapTransform.name = "/std/collections/map";
  canonicalMapTransform.templateArgs = {"i32", "i32"};

  primec::Expr init = makeCall("map");
  init.templateArgs = {"i32", "i32"};
  primec::Expr binding = makeBinding("value", {canonicalMapTransform}, {init});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {binding, makeCall("/return")}));
  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

TEST_CASE("uninitialized struct fields are allowed") {
  primec::Program program;
  primec::Expr fieldInit = makeCall("uninitialized");
  fieldInit.templateArgs = {"i32"};
  primec::Expr field = makeBinding("storage", {makeTransform("uninitialized", std::string("i32"))}, {fieldInit});
  program.definitions.push_back(makeDefinition("/Box", {makeTransform("struct")}, {field}));
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {makeCall("/return")}));
  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

TEST_CASE("nested uninitialized remains disallowed in pointer targets") {
  primec::Program program;
  primec::Expr valueBinding = makeBinding("value", {makeTransform("i32")}, {makeLiteral(1)});
  primec::Expr init = makeCall("location", {makeName("value")});
  primec::Expr binding =
      makeBinding("ptr", {makeTransform("Pointer", std::string("uninitialized<array<uninitialized<i32>>>"))}, {init});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {valueBinding, binding, makeCall("/return")}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("uninitialized storage is not allowed in pointer targets") != std::string::npos);
}

TEST_CASE("nested uninitialized remains disallowed in reference targets") {
  primec::Program program;
  primec::Expr valueBinding = makeBinding("value", {makeTransform("i32")}, {makeLiteral(1)});
  primec::Expr init = makeCall("location", {makeName("value")});
  primec::Expr binding =
      makeBinding("ref", {makeTransform("Reference", std::string("uninitialized<array<uninitialized<i32>>>"))}, {init});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {valueBinding, binding, makeCall("/return")}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("uninitialized storage is not allowed in reference targets") != std::string::npos);
}

TEST_CASE("uninitialized return types are rejected") {
  primec::Program program;
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("uninitialized<i32>"))},
                                               {makeCall("/return")}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("uninitialized storage is not allowed as return type") != std::string::npos);
}

TEST_CASE("implicit auto parameters reject templated definitions") {
  primec::Program program;
  primec::Definition wrap =
      makeDefinition("/wrap", {makeTransform("return", std::string("auto"))},
                     {makeCall("/return", {makeName("value")})},
                     {makeParameter("value", "auto")});
  wrap.templateArgs = {"T"};
  program.definitions.push_back(wrap);
  program.definitions.push_back(
      makeDefinition("/main", {makeTransform("return", std::string("i32"))}, {makeCall("/return", {makeLiteral(1)})}));

  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("implicit auto parameters are only supported on non-templated definitions") != std::string::npos);
}

TEST_CASE("struct definition cannot return a value") {
  primec::Program program;
  primec::Definition def = makeDefinition("/main", {makeTransform("struct")}, {});
  def.returnExpr = makeLiteral(1);
  program.definitions.push_back(def);
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("struct definitions cannot return a value") != std::string::npos);
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

TEST_CASE("block arguments accept bindings on statement calls") {
  primec::Program program;
  primec::Expr binding = makeBinding("value", {makeTransform("i32")}, {makeLiteral(1)});
  primec::Expr callWithBlock = makeCall("foo", {}, {}, {binding});
  program.definitions.push_back(
      makeDefinition("/foo", {makeTransform("return", std::string("void"))}, {}));
  program.definitions.push_back(
      makeDefinition("/main", {makeTransform("return", std::string("int"))},
                     {callWithBlock, makeCall("/return", {makeLiteral(1)})}));
  std::string error;
  CHECK(validateProgram(program, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("binding blocks are rejected") {
  primec::Program program;
  primec::Expr binding = makeBinding("value", {makeTransform("i32")}, {makeLiteral(1)});
  binding.hasBodyArguments = true;
  binding.bodyArguments = {makeLiteral(2)};
  program.definitions.push_back(
      makeDefinition("/main", {makeTransform("return", std::string("int"))},
                     {binding, makeCall("/return", {makeLiteral(1)})}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("binding does not accept block arguments") != std::string::npos);
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

TEST_CASE("expression call accepts empty block arguments") {
  primec::Program program;
  program.definitions.push_back(
      makeDefinition("/task", {makeTransform("return", std::string("int"))}, {makeCall("/return", {makeLiteral(3)})}));
  primec::Expr blockCall = makeCall("task");
  blockCall.hasBodyArguments = true;
  program.definitions.push_back(makeDefinition(
      "/main", {makeTransform("return", std::string("int"))}, {makeCall("/return", {blockCall})}));
  std::string error;
  CHECK(validateProgram(program, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("expression call accepts block arguments") {
  primec::Program program;
  primec::Expr returnExpr = makeCall("/return", {makeName("input")});
  program.definitions.push_back(makeDefinition("/task",
                                               {makeTransform("return", std::string("int"))},
                                               {returnExpr},
                                               {makeParameter("input")}));
  primec::Expr binding = makeBinding("temp", {makeTransform("i32")}, {makeLiteral(1)});
  primec::Expr blockCall = makeCall("task", {makeLiteral(2)}, {}, {binding, makeName("temp")});
  blockCall.hasBodyArguments = true;
  program.definitions.push_back(makeDefinition(
      "/main", {makeTransform("return", std::string("int"))}, {makeCall("/return", {blockCall})}));
  std::string error;
  CHECK(validateProgram(program, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("block expression yields last expression") {
  primec::Program program;
  primec::Expr binding = makeBinding("x", {makeTransform("i32")}, {makeLiteral(4)});
  primec::Expr plusCall = makeCall("plus", {makeName("x"), makeLiteral(1)});
  primec::Expr blockCall = makeCall("block", {}, {}, {binding, plusCall});
  blockCall.hasBodyArguments = true;
  program.definitions.push_back(makeDefinition(
      "/main", {makeTransform("return", std::string("int"))}, {makeCall("/return", {blockCall})}));
  std::string error;
  CHECK(validateProgram(program, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("block expression requires a value") {
  primec::Program program;
  primec::Expr blockCall = makeCall("block");
  blockCall.hasBodyArguments = true;
  program.definitions.push_back(makeDefinition(
      "/main", {makeTransform("return", std::string("int"))}, {makeCall("/return", {blockCall})}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("block expression requires a value") != std::string::npos);
}

TEST_CASE("block statement can contain return") {
  primec::Program program;
  primec::Expr innerReturn = makeCall("/return", {makeLiteral(1)});
  primec::Expr blockStmt = makeCall("block", {}, {}, {innerReturn});
  blockStmt.hasBodyArguments = true;
  program.definitions.push_back(makeDefinition(
      "/main", {makeTransform("return", std::string("int"))}, {blockStmt}));
  std::string error;
  CHECK(validateProgram(program, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("return rejects named arguments") {
  primec::Program program;
  primec::Expr returnCall = makeCall("/return", {makeLiteral(1)}, {std::string("value")});
  program.definitions.push_back(
      makeDefinition("/main", {makeTransform("return", std::string("int"))}, {returnCall}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_CASE("if rejects named arguments in manual AST") {
  primec::Program program;
  primec::Expr thenBlock = makeCall("then", {}, {}, {makeCall("/return", {makeLiteral(1)})});
  thenBlock.hasBodyArguments = true;
  primec::Expr elseBlock = makeCall("else", {}, {}, {makeCall("/return", {makeLiteral(2)})});
  elseBlock.hasBodyArguments = true;
  primec::Expr ifCall = makeCall("if", {makeBool(true), thenBlock, elseBlock},
                                 {std::string("cond"), std::string("yes"), std::string("no")});
  program.definitions.push_back(
      makeDefinition("/main", {makeTransform("return", std::string("int"))}, {ifCall}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_CASE("if branches returning satisfy missing return checks") {
  primec::Program program;
  primec::Expr thenBlock = makeCall("then", {}, {}, {makeCall("/return", {makeLiteral(1)})});
  thenBlock.hasBodyArguments = true;
  primec::Expr elseBlock = makeCall("else", {}, {}, {makeCall("/return", {makeLiteral(2)})});
  elseBlock.hasBodyArguments = true;
  primec::Expr ifCall = makeCall("if", {makeBool(true), thenBlock, elseBlock});
  program.definitions.push_back(
      makeDefinition("/main", {makeTransform("return", std::string("int"))}, {ifCall}));
  std::string error;
  CHECK(validateProgram(program, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("if missing else return fails missing return checks") {
  primec::Program program;
  primec::Expr thenBlock = makeCall("then", {}, {}, {makeCall("/return", {makeLiteral(1)})});
  thenBlock.hasBodyArguments = true;
  primec::Expr fallbackBinding = makeBinding("fallback", {makeTransform("i32")}, {makeLiteral(2)});
  primec::Expr elseBlock = makeCall("else", {}, {}, {fallbackBinding});
  elseBlock.hasBodyArguments = true;
  primec::Expr ifCall = makeCall("if", {makeBool(true), thenBlock, elseBlock});
  program.definitions.push_back(
      makeDefinition("/main", {makeTransform("return", std::string("int"))}, {ifCall}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("missing return statement") != std::string::npos);
}

TEST_CASE("for condition binding bool passes") {
  primec::Program program;
  primec::Expr initBinding = makeBinding("i", {makeTransform("i32"), makeTransform("mut")}, {makeLiteral(0)});
  primec::Expr condBinding = makeBinding("keepGoing", {makeTransform("bool")}, {makeBool(true)});
  primec::Expr stepCall = makeCall("increment", {makeName("i")});
  primec::Expr bodyBlock = makeCall("body", {}, {}, {makeCall("/return", {makeLiteral(7)})});
  bodyBlock.hasBodyArguments = true;
  primec::Expr forCall = makeCall("for", {initBinding, condBinding, stepCall, bodyBlock});
  program.definitions.push_back(makeDefinition(
      "/main", {makeTransform("return", std::string("int"))}, {forCall, makeCall("/return", {makeLiteral(0)})}));
  std::string error;
  CHECK(validateProgram(program, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("for condition binding requires bool in manual AST") {
  primec::Program program;
  primec::Expr initBinding = makeBinding("i", {makeTransform("i32"), makeTransform("mut")}, {makeLiteral(0)});
  primec::Expr condBinding = makeBinding("keepGoing", {makeTransform("i32")}, {makeLiteral(1)});
  primec::Expr stepCall = makeCall("increment", {makeName("i")});
  primec::Expr bodyBlock = makeCall("body", {}, {}, {makeCall("/return", {makeLiteral(7)})});
  bodyBlock.hasBodyArguments = true;
  primec::Expr forCall = makeCall("for", {initBinding, condBinding, stepCall, bodyBlock});
  program.definitions.push_back(makeDefinition(
      "/main", {makeTransform("return", std::string("int"))}, {forCall, makeCall("/return", {makeLiteral(0)})}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("for condition requires bool") != std::string::npos);
}

