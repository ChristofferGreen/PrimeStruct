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

TEST_CASE("type pack storage expands into deterministic struct fields") {
  primec::Program program;
  primec::Transform packTransform = makeTransform("Ts");
  packTransform.isPackExpansion = true;
  primec::Definition tuple =
      makeDefinition("/Tuple",
                     {makeTransform("struct")},
                     {makeBinding("values", {packTransform}, {})});
  tuple.templateArgs = {"Ts"};
  tuple.templateArgIsPack = {true};
  program.definitions.push_back(std::move(tuple));

  primec::Expr emptyCtor = makeCall("/Tuple");
  emptyCtor.isBraceConstructor = true;
  primec::Expr oneCtor = makeCall("/Tuple", {makeLiteral(1)});
  oneCtor.isBraceConstructor = true;
  oneCtor.templateArgs = {"i32"};
  primec::Expr manyCtor =
      makeCall("/Tuple", {makeLiteral(1), makeBool(true), makeLiteral(8)});
  manyCtor.isBraceConstructor = true;
  manyCtor.templateArgs = {"i32", "bool", "i32"};
  program.definitions.push_back(
      makeDefinition("/main",
                     {makeTransform("return", std::string("i32"))},
                     {makeBinding("empty", {makeTransform("auto")}, {emptyCtor}),
                      makeBinding("one", {makeTransform("auto")}, {oneCtor}),
                      makeBinding("many", {makeTransform("auto")}, {manyCtor}),
                      makeCall("/return", {makeLiteral(0)})}));

  primec::SemanticProgram semanticProgram;
  primec::Semantics semantics;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  std::string error;
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

  const primec::Definition *zero = nullptr;
  const primec::Definition *one = nullptr;
  const primec::Definition *many = nullptr;
  for (const primec::Definition &def : program.definitions) {
    if (def.fullPath.rfind("/Tuple__t", 0) != 0 ||
        def.templatePackBindings.size() != 1) {
      continue;
    }
    const auto &args = def.templatePackBindings.front().arguments;
    if (args.empty()) {
      zero = &def;
    } else if (args == std::vector<std::string>{"i32"}) {
      one = &def;
    } else if (args == std::vector<std::string>{"i32", "bool", "i32"}) {
      many = &def;
    }
  }
  REQUIRE(zero != nullptr);
  REQUIRE(one != nullptr);
  REQUIRE(many != nullptr);
  CHECK(zero->statements.empty());
  REQUIRE(one->statements.size() == 1);
  CHECK(one->statements[0].name == "__pack_values_0");
  REQUIRE(one->statements[0].transforms.size() == 1);
  CHECK(one->statements[0].transforms[0].name == "i32");
  REQUIRE(many->statements.size() == 3);
  CHECK(many->statements[0].name == "__pack_values_0");
  CHECK(many->statements[1].name == "__pack_values_1");
  CHECK(many->statements[2].name == "__pack_values_2");

  std::vector<std::string> manyFieldTypes;
  for (const auto &entry : semanticProgram.structFieldMetadata) {
    if (entry.structPath != many->fullPath) {
      continue;
    }
    manyFieldTypes.push_back(entry.fieldName + ":" + entry.bindingTypeText);
  }
  CHECK(manyFieldTypes == std::vector<std::string>{
                              "__pack_values_0:i32",
                              "__pack_values_1:bool",
                              "__pack_values_2:i32",
                          });
}

TEST_CASE("type pack storage rejects invalid placements and shapes") {
  auto makePackStorageStruct = [](const std::string &path,
                                  std::vector<primec::Transform> fieldTransforms) {
    primec::Definition def =
        makeDefinition(path, {makeTransform("struct")},
                       {makeBinding("values", std::move(fieldTransforms), {})});
    def.templateArgs = {"Ts"};
    def.templateArgIsPack = {true};
    return def;
  };
  auto makePackTransform = []() {
    primec::Transform transform = makeTransform("Ts");
    transform.isPackExpansion = true;
    return transform;
  };

  std::string error;
  {
    primec::Program program;
    primec::Transform privateTransform = makeTransform("private");
    program.definitions.push_back(
        makePackStorageStruct("/Bad", {privateTransform, makePackTransform()}));
    primec::Expr ctor = makeCall("/Bad", {makeLiteral(1)});
    ctor.isBraceConstructor = true;
    ctor.templateArgs = {"i32"};
    program.definitions.push_back(
        makeDefinition("/main",
                       {makeTransform("return", std::string("i32"))},
                       {makeBinding("bad", {makeTransform("auto")}, {ctor}),
                        makeCall("/return", {makeLiteral(0)})}));
    CHECK_FALSE(validateProgram(program, "/main", error));
    CHECK(error.find("type-pack field expansion does not accept field modifiers") !=
          std::string::npos);
  }

  error.clear();
  {
    primec::Program program;
    primec::Definition bad =
        makeDefinition("/bad",
                       {makeTransform("return", std::string("i32"))},
                       {makeBinding("values", {makePackTransform()}, {}),
                        makeCall("/return", {makeLiteral(0)})});
    bad.templateArgs = {"Ts"};
    bad.templateArgIsPack = {true};
    program.definitions.push_back(std::move(bad));
    primec::Expr call = makeCall("/bad");
    call.templateArgs = {"i32"};
    program.definitions.push_back(
        makeDefinition("/main",
                       {makeTransform("return", std::string("i32"))},
                       {makeCall("/return", {call})}));
    CHECK_FALSE(validateProgram(program, "/main", error));
    CHECK(error.find("type-pack expansion is only supported in struct field declarations") !=
          std::string::npos);
  }

  error.clear();
  {
    primec::Program program;
    program.definitions.push_back(
        makePackStorageStruct("/Tuple", {makePackTransform()}));
    primec::Expr ctor = makeCall("/Tuple", {makeLiteral(1)});
    ctor.isBraceConstructor = true;
    ctor.templateArgs = {"i32", "bool"};
    program.definitions.push_back(
        makeDefinition("/main",
                       {makeTransform("return", std::string("i32"))},
                       {makeBinding("bad", {makeTransform("auto")}, {ctor}),
                        makeCall("/return", {makeLiteral(0)})}));
    CHECK_FALSE(validateProgram(program, "/main", error));
    CHECK(error.find("argument count mismatch for /Tuple__t") != std::string::npos);
  }

  error.clear();
  {
    primec::Program program;
    program.definitions.push_back(
        makePackStorageStruct("/Node", {makePackTransform()}));
    primec::Expr childCtor = makeCall("/Node", {makeLiteral(1)});
    childCtor.isBraceConstructor = true;
    childCtor.templateArgs = {"i32"};
    primec::Expr parentCtor = makeCall("/Node", {makeName("child")});
    parentCtor.isBraceConstructor = true;
    parentCtor.templateArgs = {"Node<i32>"};
    program.definitions.push_back(
        makeDefinition("/main",
                       {makeTransform("return", std::string("i32"))},
                       {makeBinding("child", {makeTransform("auto")}, {childCtor}),
                        makeBinding("parent", {makeTransform("auto")}, {parentCtor}),
                        makeCall("/return", {makeLiteral(0)})}));
    CHECK_FALSE(validateProgram(program, "/main", error));
    CHECK(error.find("recursive pack-expanded storage not supported") !=
          std::string::npos);
  }
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
