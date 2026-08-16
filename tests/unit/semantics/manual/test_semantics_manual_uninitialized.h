#pragma once

TEST_CASE("uninitialized binding accepts constructor") {
  primec::Program program;
  primec::Expr init = makeCall("uninitialized");
  init.templateArgs = {"i32"};
  primec::Expr binding = makeBinding("storage", {makeTransform("uninitialized", std::string("i32"))}, {init});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {binding, makeCall("/return")}));
  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

TEST_CASE("uninitialized parameters are rejected") {
  primec::Program program;
  primec::Expr param = makeBinding("value", {makeTransform("uninitialized", std::string("i32"))}, {});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {makeCall("/return")},
                                               {param}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("uninitialized storage is not allowed on parameters") != std::string::npos);
}

TEST_CASE("uninitialized initializer type mismatch fails") {
  primec::Program program;
  primec::Expr init = makeCall("uninitialized");
  init.templateArgs = {"i64"};
  primec::Expr binding = makeBinding("storage", {makeTransform("uninitialized", std::string("i32"))}, {init});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {binding, makeCall("/return")}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("uninitialized initializer type mismatch") != std::string::npos);
}

TEST_CASE("uninitialized helpers are statement-only") {
  primec::Program program;
  primec::Expr initStorage = makeCall("uninitialized");
  initStorage.templateArgs = {"i32"};
  primec::Expr binding = makeBinding("storage", {makeTransform("uninitialized", std::string("i32"))}, {initStorage});
  primec::Expr initCall = makeCall("init", {makeName("storage"), makeLiteral(1)});
  primec::Expr badBinding = makeBinding("value", {makeTransform("i32")}, {initCall});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {binding, badBinding, makeCall("/return")}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("init is only supported as a statement") != std::string::npos);
}

TEST_CASE("uninitialized init/drop allowed as statements") {
  primec::Program program;
  primec::Expr initStorage = makeCall("uninitialized");
  initStorage.templateArgs = {"i32"};
  primec::Expr binding = makeBinding("storage", {makeTransform("uninitialized", std::string("i32"))}, {initStorage});
  primec::Expr initCall = makeCall("init", {makeName("storage"), makeLiteral(1)});
  primec::Expr dropCall = makeCall("drop", {makeName("storage")});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {binding, initCall, dropCall, makeCall("/return")}));
  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

TEST_CASE("uninitialized take infers return kind") {
  primec::Program program;
  primec::Expr initStorage = makeCall("uninitialized");
  initStorage.templateArgs = {"i32"};
  primec::Expr binding = makeBinding("storage", {makeTransform("uninitialized", std::string("i32"))}, {initStorage});
  primec::Expr initCall = makeCall("init", {makeName("storage"), makeLiteral(1)});
  primec::Expr takeCall = makeCall("take", {makeName("storage")});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("i32"))},
                                               {binding, initCall, makeCall("/return", {takeCall})}));
  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

TEST_CASE("uninitialized borrow infers return kind") {
  primec::Program program;
  primec::Expr initStorage = makeCall("uninitialized");
  initStorage.templateArgs = {"i32"};
  primec::Expr binding = makeBinding("storage", {makeTransform("uninitialized", std::string("i32"))}, {initStorage});
  primec::Expr initCall = makeCall("init", {makeName("storage"), makeLiteral(1)});
  primec::Expr borrowCall = makeCall("borrow", {makeName("storage")});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("i32"))},
                                               {binding, initCall, makeCall("/return", {borrowCall})}));
  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

TEST_CASE("uninitialized init rejects non-storage binding") {
  primec::Program program;
  primec::Expr valueBinding = makeBinding("value", {makeTransform("i32")}, {makeLiteral(1)});
  primec::Expr initCall = makeCall("init", {makeName("value"), makeLiteral(2)});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {valueBinding, initCall, makeCall("/return")}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("init requires uninitialized<T> storage") != std::string::npos);
}

TEST_CASE("uninitialized init rejects value type mismatch") {
  primec::Program program;
  primec::Expr initStorage = makeCall("uninitialized");
  initStorage.templateArgs = {"i32"};
  primec::Expr binding = makeBinding("storage", {makeTransform("uninitialized", std::string("i32"))}, {initStorage});
  primec::Expr initCall = makeCall("init", {makeName("storage"), makeBool(true)});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {binding, initCall, makeCall("/return")}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("init value type mismatch") != std::string::npos);
}

TEST_CASE("uninitialized init validates array element types") {
  primec::Program program;
  primec::Expr initStorage = makeCall("uninitialized");
  initStorage.templateArgs = {"array<i32>"};
  primec::Expr binding = makeBinding("storage", {makeTransform("uninitialized", std::string("array<i32>"))}, {initStorage});
  primec::Expr arrayValue = makeCall("array", {makeBool(true)});
  arrayValue.templateArgs = {"bool"};
  primec::Expr initCall = makeCall("init", {makeName("storage"), arrayValue});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {binding, initCall, makeCall("/return")}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("init value type mismatch") != std::string::npos);
}

TEST_CASE("uninitialized init validates vector element types") {
  primec::Program program;
  primec::Expr initStorage = makeCall("uninitialized");
  initStorage.templateArgs = {"vector<i32>"};
  primec::Expr binding = makeBinding("storage", {makeTransform("uninitialized", std::string("vector<i32>"))}, {initStorage});
  primec::Expr vectorValue = makeCall("vector");
  vectorValue.templateArgs = {"bool"};
  primec::Expr initCall = makeCall("init", {makeName("storage"), vectorValue});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {binding, initCall, makeCall("/return")}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("init value type mismatch") != std::string::npos);
}

TEST_CASE("uninitialized init validates map key/value types") {
  primec::Program program;
  primec::Expr initStorage = makeCall("uninitialized");
  initStorage.templateArgs = {"map<i32, string>"};
  primec::Expr binding =
      makeBinding("storage", {makeTransform("uninitialized", std::string("map<i32, string>"))}, {initStorage});
  primec::Expr mapValue = makeCall("map");
  mapValue.templateArgs = {"i32", "bool"};
  primec::Expr initCall = makeCall("init", {makeName("storage"), mapValue});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {binding, initCall, makeCall("/return")}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("init value type mismatch") != std::string::npos);
}

TEST_CASE("uninitialized init requires uninitialized storage") {
  primec::Program program;
  primec::Expr initStorage = makeCall("uninitialized");
  initStorage.templateArgs = {"i32"};
  primec::Expr binding = makeBinding("storage", {makeTransform("uninitialized", std::string("i32"))}, {initStorage});
  primec::Expr initCall = makeCall("init", {makeName("storage"), makeLiteral(1)});
  primec::Expr initAgain = makeCall("init", {makeName("storage"), makeLiteral(2)});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {binding, initCall, initAgain, makeCall("/return")}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("init requires uninitialized storage") != std::string::npos);
}

TEST_CASE("uninitialized drop requires initialized storage") {
  primec::Program program;
  primec::Expr initStorage = makeCall("uninitialized");
  initStorage.templateArgs = {"i32"};
  primec::Expr binding = makeBinding("storage", {makeTransform("uninitialized", std::string("i32"))}, {initStorage});
  primec::Expr dropCall = makeCall("drop", {makeName("storage")});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {binding, dropCall, makeCall("/return")}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("drop requires initialized storage") != std::string::npos);
}

TEST_CASE("uninitialized storage must be dropped before end of scope") {
  primec::Program program;
  primec::Expr initStorage = makeCall("uninitialized");
  initStorage.templateArgs = {"i32"};
  primec::Expr binding = makeBinding("storage", {makeTransform("uninitialized", std::string("i32"))}, {initStorage});
  primec::Expr initCall = makeCall("init", {makeName("storage"), makeLiteral(1)});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {binding, initCall}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("uninitialized storage must be dropped before end of scope") != std::string::npos);
}

TEST_CASE("uninitialized return requires drop") {
  primec::Program program;
  primec::Expr initStorage = makeCall("uninitialized");
  initStorage.templateArgs = {"i32"};
  primec::Expr binding = makeBinding("storage", {makeTransform("uninitialized", std::string("i32"))}, {initStorage});
  primec::Expr initCall = makeCall("init", {makeName("storage"), makeLiteral(1)});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {binding, initCall, makeCall("/return")}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("return requires uninitialized storage to be dropped") != std::string::npos);
}

TEST_CASE("uninitialized non-void return requires drop") {
  primec::Program program;
  primec::Expr initStorage = makeCall("uninitialized");
  initStorage.templateArgs = {"i32"};
  primec::Expr binding = makeBinding("storage", {makeTransform("uninitialized", std::string("i32"))}, {initStorage});
  primec::Expr initCall = makeCall("init", {makeName("storage"), makeLiteral(1)});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("i32"))},
                                               {binding, initCall, makeCall("/return", {makeLiteral(7)})}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("return requires uninitialized storage to be dropped") != std::string::npos);
}

TEST_CASE("uninitialized drop before non-void return passes") {
  primec::Program program;
  primec::Expr initStorage = makeCall("uninitialized");
  initStorage.templateArgs = {"i32"};
  primec::Expr binding = makeBinding("storage", {makeTransform("uninitialized", std::string("i32"))}, {initStorage});
  primec::Expr initCall = makeCall("init", {makeName("storage"), makeLiteral(1)});
  primec::Expr dropCall = makeCall("drop", {makeName("storage")});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("i32"))},
                                               {binding, initCall, dropCall, makeCall("/return", {makeLiteral(7)})}));
  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

TEST_CASE("uninitialized if expression requires drop on all branches") {
  primec::Program program;
  primec::Expr initStorage = makeCall("uninitialized");
  initStorage.templateArgs = {"i32"};
  primec::Expr binding = makeBinding("storage", {makeTransform("uninitialized", std::string("i32"))}, {initStorage});
  primec::Expr initCall = makeCall("init", {makeName("storage"), makeLiteral(1)});
  primec::Expr thenBlock = makeCall("then", {}, {}, {makeCall("take", {makeName("storage")})});
  thenBlock.hasBodyArguments = true;
  primec::Expr elseBlock = makeCall("else", {}, {}, {makeLiteral(7)});
  elseBlock.hasBodyArguments = true;
  primec::Expr ifExpr = makeCall("if", {makeBool(true), thenBlock, elseBlock});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("i32"))},
                                               {binding, initCall, makeCall("/return", {ifExpr})}));
  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("return requires uninitialized storage to be dropped") != std::string::npos);
}

TEST_CASE("uninitialized if expression drops on both branches") {
  primec::Program program;
  primec::Expr initStorage = makeCall("uninitialized");
  initStorage.templateArgs = {"i32"};
  primec::Expr binding = makeBinding("storage", {makeTransform("uninitialized", std::string("i32"))}, {initStorage});
  primec::Expr initCall = makeCall("init", {makeName("storage"), makeLiteral(1)});
  primec::Expr thenBlock = makeCall("then", {}, {}, {makeCall("take", {makeName("storage")})});
  thenBlock.hasBodyArguments = true;
  primec::Expr elseBlock = makeCall("else", {}, {}, {makeCall("take", {makeName("storage")})});
  elseBlock.hasBodyArguments = true;
  primec::Expr ifExpr = makeCall("if", {makeBool(true), thenBlock, elseBlock});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("i32"))},
                                               {binding, initCall, makeCall("/return", {ifExpr})}));
  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

TEST_CASE("uninitialized block expression propagates take") {
  primec::Program program;
  primec::Expr initStorage = makeCall("uninitialized");
  initStorage.templateArgs = {"i32"};
  primec::Expr binding = makeBinding("storage", {makeTransform("uninitialized", std::string("i32"))}, {initStorage});
  primec::Expr initCall = makeCall("init", {makeName("storage"), makeLiteral(1)});
  primec::Expr blockExpr = makeCall("block", {}, {}, {makeCall("take", {makeName("storage")})});
  blockExpr.hasBodyArguments = true;
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("i32"))},
                                               {binding, initCall, makeCall("/return", {blockExpr})}));
  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

TEST_CASE("uninitialized for condition take with reinit step passes") {
  primec::Program program;
  program.definitions.push_back(
      makeDefinition("/noop", {makeTransform("return", std::string("void"))}, {makeCall("/return")}));

  primec::Expr initStorage = makeCall("uninitialized");
  initStorage.templateArgs = {"i32"};
  primec::Expr storageBinding =
      makeBinding("storage", {makeTransform("uninitialized", std::string("i32"))}, {initStorage});
  primec::Expr initCall = makeCall("init", {makeName("storage"), makeLiteral(1)});
  primec::Expr condExpr = makeCall("equal", {makeCall("take", {makeName("storage")}), makeLiteral(1)});
  primec::Expr stepExpr = makeCall("init", {makeName("storage"), makeLiteral(1)});
  primec::Expr bodyBlock = makeCall("do", {}, {}, {makeCall("/noop")});
  bodyBlock.hasBodyArguments = true;
  primec::Expr forCall = makeCall("for", {makeCall("/noop"), condExpr, stepExpr, bodyBlock});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {storageBinding, initCall, forCall, makeCall("/return")}));

  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

TEST_CASE("uninitialized for condition take requires reinit before next check") {
  primec::Program program;
  program.definitions.push_back(
      makeDefinition("/noop", {makeTransform("return", std::string("void"))}, {makeCall("/return")}));

  primec::Expr initStorage = makeCall("uninitialized");
  initStorage.templateArgs = {"i32"};
  primec::Expr storageBinding =
      makeBinding("storage", {makeTransform("uninitialized", std::string("i32"))}, {initStorage});
  primec::Expr initCall = makeCall("init", {makeName("storage"), makeLiteral(1)});
  primec::Expr condExpr = makeCall("equal", {makeCall("take", {makeName("storage")}), makeLiteral(1)});
  primec::Expr bodyBlock = makeCall("do", {}, {}, {makeCall("/noop")});
  bodyBlock.hasBodyArguments = true;
  primec::Expr forCall = makeCall("for", {makeCall("/noop"), condExpr, makeCall("/noop"), bodyBlock});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {storageBinding, initCall, forCall, makeCall("/return")}));

  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("take requires initialized storage") != std::string::npos);
}

TEST_CASE("uninitialized while condition take with reinit body passes") {
  primec::Program program;
  program.definitions.push_back(
      makeDefinition("/noop", {makeTransform("return", std::string("void"))}, {makeCall("/return")}));

  primec::Expr initStorage = makeCall("uninitialized");
  initStorage.templateArgs = {"i32"};
  primec::Expr storageBinding =
      makeBinding("storage", {makeTransform("uninitialized", std::string("i32"))}, {initStorage});
  primec::Expr initCall = makeCall("init", {makeName("storage"), makeLiteral(1)});
  primec::Expr condExpr = makeCall("equal", {makeCall("take", {makeName("storage")}), makeLiteral(1)});
  primec::Expr bodyBlock =
      makeCall("do", {}, {}, {makeCall("init", {makeName("storage"), makeLiteral(1)}), makeCall("/noop")});
  bodyBlock.hasBodyArguments = true;
  primec::Expr whileCall = makeCall("while", {condExpr, bodyBlock});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {storageBinding, initCall, whileCall, makeCall("/return")}));

  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

TEST_CASE("uninitialized while condition take requires reinit before next check") {
  primec::Program program;
  program.definitions.push_back(
      makeDefinition("/noop", {makeTransform("return", std::string("void"))}, {makeCall("/return")}));

  primec::Expr initStorage = makeCall("uninitialized");
  initStorage.templateArgs = {"i32"};
  primec::Expr storageBinding =
      makeBinding("storage", {makeTransform("uninitialized", std::string("i32"))}, {initStorage});
  primec::Expr initCall = makeCall("init", {makeName("storage"), makeLiteral(1)});
  primec::Expr condExpr = makeCall("equal", {makeCall("take", {makeName("storage")}), makeLiteral(1)});
  primec::Expr bodyBlock = makeCall("do", {}, {}, {makeCall("/noop")});
  bodyBlock.hasBodyArguments = true;
  primec::Expr whileCall = makeCall("while", {condExpr, bodyBlock});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {storageBinding, initCall, whileCall, makeCall("/return")}));

  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("take requires initialized storage") != std::string::npos);
}

TEST_CASE("uninitialized loop body take with reinit passes") {
  primec::Program program;
  program.definitions.push_back(
      makeDefinition("/noop", {makeTransform("return", std::string("void"))}, {makeCall("/return")}));

  primec::Expr initStorage = makeCall("uninitialized");
  initStorage.templateArgs = {"i32"};
  primec::Expr storageBinding =
      makeBinding("storage", {makeTransform("uninitialized", std::string("i32"))}, {initStorage});
  primec::Expr initCall = makeCall("init", {makeName("storage"), makeLiteral(1)});
  primec::Expr bodyBlock = makeCall(
      "do", {}, {}, {makeCall("take", {makeName("storage")}), makeCall("init", {makeName("storage"), makeLiteral(1)})});
  bodyBlock.hasBodyArguments = true;
  primec::Expr loopCall = makeCall("loop", {makeLiteral(2), bodyBlock});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {storageBinding, initCall, loopCall, makeCall("drop", {makeName("storage")}),
                                                makeCall("/return")}));

  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

TEST_CASE("uninitialized loop body take requires reinit before next iteration") {
  primec::Program program;
  program.definitions.push_back(
      makeDefinition("/noop", {makeTransform("return", std::string("void"))}, {makeCall("/return")}));

  primec::Expr initStorage = makeCall("uninitialized");
  initStorage.templateArgs = {"i32"};
  primec::Expr storageBinding =
      makeBinding("storage", {makeTransform("uninitialized", std::string("i32"))}, {initStorage});
  primec::Expr initCall = makeCall("init", {makeName("storage"), makeLiteral(1)});
  primec::Expr bodyBlock = makeCall("do", {}, {}, {makeCall("take", {makeName("storage")}), makeCall("/noop")});
  bodyBlock.hasBodyArguments = true;
  primec::Expr loopCall = makeCall("loop", {makeLiteral(2), bodyBlock});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {storageBinding, initCall, loopCall, makeCall("/return")}));

  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("take requires initialized storage") != std::string::npos);
}

TEST_CASE("uninitialized repeat body take with reinit passes") {
  primec::Program program;

  primec::Expr initStorage = makeCall("uninitialized");
  initStorage.templateArgs = {"i32"};
  primec::Expr storageBinding =
      makeBinding("storage", {makeTransform("uninitialized", std::string("i32"))}, {initStorage});
  primec::Expr initCall = makeCall("init", {makeName("storage"), makeLiteral(1)});
  primec::Expr repeatCall = makeCall(
      "repeat", {makeLiteral(2)}, {}, {makeCall("take", {makeName("storage")}),
                                       makeCall("init", {makeName("storage"), makeLiteral(1)})});
  repeatCall.hasBodyArguments = true;
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {storageBinding, initCall, repeatCall, makeCall("drop", {makeName("storage")}),
                                                makeCall("/return")}));

  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

TEST_CASE("uninitialized repeat body take requires reinit before next iteration") {
  primec::Program program;

  primec::Expr initStorage = makeCall("uninitialized");
  initStorage.templateArgs = {"i32"};
  primec::Expr storageBinding =
      makeBinding("storage", {makeTransform("uninitialized", std::string("i32"))}, {initStorage});
  primec::Expr initCall = makeCall("init", {makeName("storage"), makeLiteral(1)});
  primec::Expr repeatCall = makeCall("repeat", {makeLiteral(2)}, {}, {makeCall("take", {makeName("storage")})});
  repeatCall.hasBodyArguments = true;
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {storageBinding, initCall, repeatCall, makeCall("/return")}));

  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("take requires initialized storage") != std::string::npos);
}

TEST_CASE("uninitialized repeat count expression effects are tracked") {
  primec::Program program;
  program.definitions.push_back(
      makeDefinition("/noop", {makeTransform("return", std::string("void"))}, {makeCall("/return")}));

  primec::Expr initStorage = makeCall("uninitialized");
  initStorage.templateArgs = {"i32"};
  primec::Expr storageBinding =
      makeBinding("storage", {makeTransform("uninitialized", std::string("i32"))}, {initStorage});
  primec::Expr initCall = makeCall("init", {makeName("storage"), makeLiteral(1)});
  primec::Expr repeatCall = makeCall("repeat", {makeCall("take", {makeName("storage")})}, {}, {makeCall("/noop")});
  repeatCall.hasBodyArguments = true;
  primec::Expr dropCall = makeCall("drop", {makeName("storage")});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {storageBinding, initCall, repeatCall, dropCall, makeCall("/return")}));

  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("drop requires initialized storage") != std::string::npos);
}

TEST_CASE("uninitialized loop literal one executes body exactly once") {
  primec::Program program;

  primec::Expr initStorage = makeCall("uninitialized");
  initStorage.templateArgs = {"i32"};
  primec::Expr storageBinding =
      makeBinding("storage", {makeTransform("uninitialized", std::string("i32"))}, {initStorage});
  primec::Expr initCall = makeCall("init", {makeName("storage"), makeLiteral(1)});
  primec::Expr bodyBlock = makeCall("do", {}, {}, {makeCall("take", {makeName("storage")})});
  bodyBlock.hasBodyArguments = true;
  primec::Expr loopCall = makeCall("loop", {makeLiteral(1), bodyBlock});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {storageBinding, initCall, loopCall, makeCall("/return")}));

  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

TEST_CASE("uninitialized loop literal zero preserves initialized state") {
  primec::Program program;

  primec::Expr initStorage = makeCall("uninitialized");
  initStorage.templateArgs = {"i32"};
  primec::Expr storageBinding =
      makeBinding("storage", {makeTransform("uninitialized", std::string("i32"))}, {initStorage});
  primec::Expr initCall = makeCall("init", {makeName("storage"), makeLiteral(1)});
  primec::Expr bodyBlock = makeCall("do", {}, {}, {makeCall("take", {makeName("storage")})});
  bodyBlock.hasBodyArguments = true;
  primec::Expr loopCall = makeCall("loop", {makeLiteral(0), bodyBlock});
  primec::Expr dropCall = makeCall("drop", {makeName("storage")});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {storageBinding, initCall, loopCall, dropCall, makeCall("/return")}));

  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

TEST_CASE("uninitialized repeat literal one executes body exactly once") {
  primec::Program program;

  primec::Expr initStorage = makeCall("uninitialized");
  initStorage.templateArgs = {"i32"};
  primec::Expr storageBinding =
      makeBinding("storage", {makeTransform("uninitialized", std::string("i32"))}, {initStorage});
  primec::Expr initCall = makeCall("init", {makeName("storage"), makeLiteral(1)});
  primec::Expr repeatCall = makeCall("repeat", {makeLiteral(1)}, {}, {makeCall("take", {makeName("storage")})});
  repeatCall.hasBodyArguments = true;
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {storageBinding, initCall, repeatCall, makeCall("/return")}));

  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

TEST_CASE("uninitialized repeat false executes zero iterations") {
  primec::Program program;

  primec::Expr initStorage = makeCall("uninitialized");
  initStorage.templateArgs = {"i32"};
  primec::Expr storageBinding =
      makeBinding("storage", {makeTransform("uninitialized", std::string("i32"))}, {initStorage});
  primec::Expr initCall = makeCall("init", {makeName("storage"), makeLiteral(1)});
  primec::Expr repeatCall = makeCall("repeat", {makeBool(false)}, {}, {makeCall("take", {makeName("storage")})});
  repeatCall.hasBodyArguments = true;
  primec::Expr dropCall = makeCall("drop", {makeName("storage")});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {storageBinding, initCall, repeatCall, dropCall, makeCall("/return")}));

  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

TEST_CASE("uninitialized repeat true executes body once") {
  primec::Program program;

  primec::Expr initStorage = makeCall("uninitialized");
  initStorage.templateArgs = {"i32"};
  primec::Expr storageBinding =
      makeBinding("storage", {makeTransform("uninitialized", std::string("i32"))}, {initStorage});
  primec::Expr initCall = makeCall("init", {makeName("storage"), makeLiteral(1)});
  primec::Expr repeatCall = makeCall("repeat", {makeBool(true)}, {}, {makeCall("take", {makeName("storage")})});
  repeatCall.hasBodyArguments = true;
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {storageBinding, initCall, repeatCall, makeCall("/return")}));

  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

TEST_CASE("uninitialized loop literal one leaves storage uninitialized") {
  primec::Program program;

  primec::Expr initStorage = makeCall("uninitialized");
  initStorage.templateArgs = {"i32"};
  primec::Expr storageBinding =
      makeBinding("storage", {makeTransform("uninitialized", std::string("i32"))}, {initStorage});
  primec::Expr initCall = makeCall("init", {makeName("storage"), makeLiteral(1)});
  primec::Expr bodyBlock = makeCall("do", {}, {}, {makeCall("take", {makeName("storage")})});
  bodyBlock.hasBodyArguments = true;
  primec::Expr loopCall = makeCall("loop", {makeLiteral(1), bodyBlock});
  primec::Expr dropCall = makeCall("drop", {makeName("storage")});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("void"))},
                                               {storageBinding, initCall, loopCall, dropCall, makeCall("/return")}));

  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("drop requires initialized storage") != std::string::npos);
}

