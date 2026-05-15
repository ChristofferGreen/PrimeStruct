#pragma once

TEST_CASE("monomorphizes template definition bindings") {
  primec::Program program;
  primec::Definition identity =
      makeDefinition("/identity", {makeTransform("return", std::string("T"))},
                     {makeCall("/return", {makeName("value")})},
                     {makeBinding("value", {makeTransform("T")}, {})});
  identity.templateArgs = {"T"};
  program.definitions.push_back(identity);

  primec::Expr call = makeCall("/identity", {makeLiteral(1)});
  call.templateArgs = {"i32"};
  program.definitions.push_back(
      makeDefinition("/main", {makeTransform("return", std::string("int"))}, {makeCall("/return", {call})}));

  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

TEST_CASE("template argument count mismatch fails") {
  primec::Program program;
  primec::Definition wrap = makeDefinition("/wrap",
                                           {makeTransform("return", std::string("int"))},
                                           {makeCall("/return", {makeName("value")})},
                                           {makeParameter("value", "int")});
  wrap.templateArgs = {"T"};
  program.definitions.push_back(wrap);

  primec::Expr call = makeCall("/wrap", {makeLiteral(1)});
  call.templateArgs = {"i32", "i64"};
  program.definitions.push_back(
      makeDefinition("/main", {makeTransform("return", std::string("int"))}, {makeCall("/return", {call})}));

  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("template argument count mismatch") != std::string::npos);
}

TEST_CASE("template arguments required for templated calls") {
  primec::Program program;
  primec::Definition wrap = makeDefinition("/wrap",
                                           {makeTransform("return", std::string("int"))},
                                           {makeCall("/return", {makeName("value")})},
                                           {makeParameter("value", "int")});
  wrap.templateArgs = {"T"};
  program.definitions.push_back(wrap);

  primec::Expr call = makeCall("/wrap", {makeLiteral(1)});
  program.definitions.push_back(
      makeDefinition("/main", {makeTransform("return", std::string("int"))}, {makeCall("/return", {call})}));

  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("template arguments required") != std::string::npos);
}

TEST_CASE("monomorphizes integer template arguments distinctly") {
  primec::Program program;
  primec::Definition stamp =
      makeDefinition("/stamp",
                     {makeTransform("return", std::string("i32"))},
                     {makeCall("/return", {makeLiteral(7)})});
  stamp.templateArgs = {"Index"};
  program.definitions.push_back(stamp);

  primec::Expr call = makeCall("/stamp");
  call.templateArgs = {"0"};
  call.templateArgDetails = {primec::TemplateArgument::integer("0", 0)};
  program.definitions.push_back(
      makeDefinition("/main",
                     {makeTransform("return", std::string("i32"))},
                     {makeCall("/return", {call})}));

  std::string error;
  primec::SemanticProgram semanticProgram;
  primec::Semantics semantics;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  REQUIRE(semantics.validate(program,
                             "/main",
                             error,
                             defaults,
                             defaults,
                             {},
                             nullptr,
                             false,
                             &semanticProgram));
  CHECK(error.empty());

  const auto specialized = std::find_if(program.definitions.begin(),
                                        program.definitions.end(),
                                        [](const primec::Definition &def) {
                                          return def.fullPath.rfind("/stamp__t", 0) == 0;
                                        });
  REQUIRE(specialized != program.definitions.end());
  CHECK(specialized->templateArgs.empty());
}

TEST_CASE("integer template arguments cannot substitute type positions") {
  primec::Program program;
  primec::Definition bad =
      makeDefinition("/bad",
                     {makeTransform("return", std::string("Index"))},
                     {makeCall("/return", {makeLiteral(7)})});
  bad.templateArgs = {"Index"};
  program.definitions.push_back(bad);

  primec::Expr call = makeCall("/bad");
  call.templateArgs = {"0"};
  call.templateArgDetails = {primec::TemplateArgument::integer("0", 0)};
  program.definitions.push_back(
      makeDefinition("/main",
                     {makeTransform("return", std::string("i32"))},
                     {makeCall("/return", {call})}));

  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("integer template argument cannot be used as a type: Index") !=
        std::string::npos);
}

TEST_CASE("type pack specializations bind zero one and many arguments") {
  primec::Program program;
  primec::Definition stamp =
      makeDefinition("/stamp",
                     {makeTransform("return", std::string("i32"))},
                     {makeCall("/return", {makeLiteral(7)})});
  stamp.templateArgs = {"Ts"};
  stamp.templateArgIsPack = {true};
  program.definitions.push_back(stamp);

  primec::Expr zeroCall = makeCall("/stamp");
  primec::Expr oneCall = makeCall("/stamp");
  oneCall.templateArgs = {"i32"};
  primec::Expr manyCall = makeCall("/stamp");
  manyCall.templateArgs = {"i32", "bool", "string"};
  program.definitions.push_back(
      makeDefinition("/main",
                     {makeTransform("return", std::string("i32"))},
                     {zeroCall,
                      oneCall,
                      manyCall,
                      makeCall("/return", {makeLiteral(0)})}));

  std::string error;
  primec::SemanticProgram semanticProgram;
  primec::Semantics semantics;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  REQUIRE(semantics.validate(program,
                             "/main",
                             error,
                             defaults,
                             defaults,
                             {},
                             nullptr,
                             false,
                             &semanticProgram));

  std::vector<std::vector<std::string>> observedPackArgs;
  for (const primec::Definition &def : program.definitions) {
    if (def.fullPath.rfind("/stamp__t", 0) != 0) {
      continue;
    }
    REQUIRE(def.templatePackBindings.size() == 1);
    CHECK(def.templatePackBindings.front().parameterName == "Ts");
    observedPackArgs.push_back(def.templatePackBindings.front().arguments);
  }
  REQUIRE(observedPackArgs.size() == 3);
  CHECK(std::find(observedPackArgs.begin(),
                  observedPackArgs.end(),
                  std::vector<std::string>{}) != observedPackArgs.end());
  CHECK(std::find(observedPackArgs.begin(),
                  observedPackArgs.end(),
                  std::vector<std::string>{"i32"}) != observedPackArgs.end());
  CHECK(std::find(observedPackArgs.begin(),
                  observedPackArgs.end(),
                  std::vector<std::string>{"i32", "bool", "string"}) !=
        observedPackArgs.end());

  const std::string formatted = primec::formatSemanticProgram(semanticProgram);
  CHECK(formatted.find("template_pack_bindings=[Ts=[]]") != std::string::npos);
  CHECK(formatted.find("template_pack_bindings=[Ts=[\"i32\"]]") !=
        std::string::npos);
  CHECK(formatted.find("template_pack_bindings=[Ts=[\"i32\", \"bool\", \"string\"]]") !=
        std::string::npos);
}

TEST_CASE("type pack specialization requires ordinary arguments before pack") {
  primec::Program program;
  primec::Definition stamp =
      makeDefinition("/stamp",
                     {makeTransform("return", std::string("i32"))},
                     {makeCall("/return", {makeLiteral(7)})});
  stamp.templateArgs = {"T", "Ts"};
  stamp.templateArgIsPack = {false, true};
  program.definitions.push_back(stamp);

  primec::Expr call = makeCall("/stamp");
  program.definitions.push_back(
      makeDefinition("/main",
                     {makeTransform("return", std::string("i32"))},
                     {makeCall("/return", {call})}));

  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("template arguments required") != std::string::npos);
}

TEST_CASE("type pack parameters cannot be used as scalar types before expansion") {
  primec::Program program;
  primec::Definition bad =
      makeDefinition("/bad",
                     {makeTransform("return", std::string("i32"))},
                     {makeCall("/return", {makeLiteral(7)})},
                     {makeParameter("value", "Ts")});
  bad.templateArgs = {"Ts"};
  bad.templateArgIsPack = {true};
  program.definitions.push_back(bad);

  primec::Expr call = makeCall("/bad", {makeLiteral(1)});
  call.templateArgs = {"i32"};
  program.definitions.push_back(
      makeDefinition("/main",
                     {makeTransform("return", std::string("i32"))},
                     {makeCall("/return", {call})}));

  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("type-pack parameter requires expansion support from TODO-4275: Ts") !=
        std::string::npos);
}

TEST_CASE("implicit auto template inference for calls") {
  primec::Program program;
  primec::Definition identity =
      makeDefinition("/identity", {makeTransform("return", std::string("auto"))},
                     {makeCall("/return", {makeName("value")})},
                     {makeParameter("value", "auto")});
  program.definitions.push_back(identity);

  primec::Expr call = makeCall("/identity", {makeLiteral(1)});
  program.definitions.push_back(
      makeDefinition("/main", {makeTransform("return", std::string("i32"))}, {makeCall("/return", {call})}));

  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

TEST_CASE("implicit auto template inference uses defaults") {
  primec::Program program;
  primec::Expr param = makeBinding("value", {makeTransform("auto")}, {makeLiteral(3)});
  primec::Definition identity =
      makeDefinition("/identity", {makeTransform("return", std::string("auto"))},
                     {makeCall("/return", {makeName("value")})},
                     {param});
  program.definitions.push_back(identity);

  primec::Expr call = makeCall("/identity");
  program.definitions.push_back(
      makeDefinition("/main", {makeTransform("return", std::string("i32"))}, {makeCall("/return", {call})}));

  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

TEST_CASE("implicit auto template inference honors named arguments") {
  primec::Program program;
  primec::Definition pick =
      makeDefinition("/pick", {makeTransform("return", std::string("auto"))},
                     {makeCall("/return", {makeName("first")})},
                     {makeParameter("first", "auto"), makeParameter("second", "auto")});
  program.definitions.push_back(pick);

  std::vector<primec::Expr> args = {makeLiteral(2), makeLiteral(1)};
  std::vector<std::optional<std::string>> argNames = {std::string("second"), std::string("first")};
  primec::Expr call = makeCall("/pick", args, argNames);
  program.definitions.push_back(
      makeDefinition("/main", {makeTransform("return", std::string("i32"))}, {makeCall("/return", {call})}));

  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

TEST_CASE("implicit auto template inference supports named arguments with defaults") {
  primec::Program program;
  primec::Expr first = makeBinding("first", {makeTransform("auto")}, {makeLiteral(3)});
  primec::Expr second = makeBinding("second", {makeTransform("auto")}, {makeLiteral(4)});
  primec::Definition pick =
      makeDefinition("/pick", {makeTransform("return", std::string("auto"))},
                     {makeCall("/return", {makeName("second")})},
                     {first, second});
  program.definitions.push_back(pick);

  std::vector<primec::Expr> args = {makeLiteral(9)};
  std::vector<std::optional<std::string>> argNames = {std::string("second")};
  primec::Expr call = makeCall("/pick", args, argNames);
  program.definitions.push_back(
      makeDefinition("/main", {makeTransform("return", std::string("i32"))}, {makeCall("/return", {call})}));

  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

TEST_CASE("implicit auto template inference for omitted parameters") {
  primec::Program program;
  primec::Expr param = makeBinding("value", {}, {});
  primec::Definition identity =
      makeDefinition("/identity", {makeTransform("return", std::string("auto"))},
                     {makeCall("/return", {makeName("value")})},
                     {param});
  program.definitions.push_back(identity);

  primec::Expr call = makeCall("/identity", {makeLiteral(1)});
  program.definitions.push_back(
      makeDefinition("/main", {makeTransform("return", std::string("i32"))}, {makeCall("/return", {call})}));

  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

TEST_CASE("implicit auto template inference uses defaults for omitted parameters") {
  primec::Program program;
  primec::Expr param = makeBinding("value", {}, {makeLiteral(3)});
  primec::Definition identity =
      makeDefinition("/identity", {makeTransform("return", std::string("auto"))},
                     {makeCall("/return", {makeName("value")})},
                     {param});
  program.definitions.push_back(identity);

  primec::Expr call = makeCall("/identity");
  program.definitions.push_back(
      makeDefinition("/main", {makeTransform("return", std::string("i32"))}, {makeCall("/return", {call})}));

  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

TEST_CASE("implicit auto parameters reject templated definitions when omitted") {
  primec::Program program;
  primec::Expr param = makeBinding("value", {}, {});
  primec::Definition identity =
      makeDefinition("/identity", {makeTransform("return", std::string("auto"))},
                     {makeCall("/return", {makeName("value")})},
                     {param});
  identity.templateArgs = {"T"};
  program.definitions.push_back(identity);

  primec::Expr call = makeCall("/identity", {makeLiteral(1)});
  program.definitions.push_back(
      makeDefinition("/main", {makeTransform("return", std::string("i32"))}, {makeCall("/return", {call})}));

  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("implicit auto parameters are only supported on non-templated definitions") != std::string::npos);
}

TEST_CASE("match call behaves like if") {
  primec::Program program;
  primec::Expr thenCall = makeCall("then", {}, {}, {makeLiteral(1)});
  thenCall.hasBodyArguments = true;
  primec::Expr elseCall = makeCall("else", {}, {}, {makeLiteral(2)});
  elseCall.hasBodyArguments = true;
  primec::Expr matchCall = makeCall("match", {makeBool(true), thenCall, elseCall});
  program.definitions.push_back(
      makeDefinition("/main", {makeTransform("return", std::string("int"))}, {makeCall("/return", {matchCall})}));

  std::string error;
  CHECK(validateProgram(program, "/main", error));
}

TEST_CASE("match cases validate") {
  primec::Program program;
  primec::Expr valueBinding = makeBinding("value", {makeTransform("i32")}, {makeLiteral(2)});
  primec::Expr caseOne = makeCall("case", {makeLiteral(1)}, {}, {makeLiteral(10)});
  caseOne.hasBodyArguments = true;
  primec::Expr caseTwo = makeCall("case", {makeLiteral(2)}, {}, {makeLiteral(20)});
  caseTwo.hasBodyArguments = true;
  primec::Expr elseCall = makeCall("else", {}, {}, {makeLiteral(30)});
  elseCall.hasBodyArguments = true;
  primec::Expr matchCall = makeCall("match", {makeName("value"), caseOne, caseTwo, elseCall});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("i32"))},
                                               {valueBinding, makeCall("/return", {matchCall})}));

  std::string error;
  CHECK_MESSAGE(validateProgram(program, "/main", error), error);
}

TEST_CASE("match requires else block") {
  primec::Program program;
  primec::Expr valueBinding = makeBinding("value", {makeTransform("i32")}, {makeLiteral(2)});
  primec::Expr caseOne = makeCall("case", {makeLiteral(1)}, {}, {makeLiteral(10)});
  caseOne.hasBodyArguments = true;
  primec::Expr matchCall = makeCall("match", {makeName("value"), caseOne});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("i32"))},
                                               {valueBinding, makeCall("/return", {matchCall})}));

  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  const bool hasMatchError = (error.find("match requires value, cases, else") != std::string::npos) ||
                             (error.find("match requires final else block") != std::string::npos);
  CHECK(hasMatchError);
}

TEST_CASE("match rejects incompatible case patterns") {
  primec::Program program;
  primec::Expr valueBinding = makeBinding("value", {makeTransform("i32")}, {makeLiteral(2)});
  primec::Expr stringLiteral;
  stringLiteral.kind = primec::Expr::Kind::StringLiteral;
  stringLiteral.stringValue = "\"nope\"utf8";
  primec::Expr caseOne = makeCall("case", {stringLiteral}, {}, {makeLiteral(10)});
  caseOne.hasBodyArguments = true;
  primec::Expr elseCall = makeCall("else", {}, {}, {makeLiteral(30)});
  elseCall.hasBodyArguments = true;
  primec::Expr matchCall = makeCall("match", {makeName("value"), caseOne, elseCall});
  program.definitions.push_back(makeDefinition("/main",
                                               {makeTransform("return", std::string("i32"))},
                                               {valueBinding, makeCall("/return", {matchCall})}));

  std::string error;
  CHECK_FALSE(validateProgram(program, "/main", error));
  CHECK(error.find("comparisons do not support mixed string/numeric operands") != std::string::npos);
}
