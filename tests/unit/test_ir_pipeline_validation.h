TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir validator accepts lowered canonical module") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {}, {}, module, error));
  CHECK(error.empty());

  CHECK(primec::validateIrModule(module, primec::IrValidationTarget::Any, error));
  CHECK(error.empty());
  CHECK(primec::validateIrModule(module, primec::IrValidationTarget::Vm, error));
  CHECK(error.empty());
  CHECK(primec::validateIrModule(module, primec::IrValidationTarget::Native, error));
  CHECK(error.empty());
}

TEST_CASE("ir lowerer effects unit resolves entry and non-entry defaults") {
  const std::vector<primec::Transform> transforms;
  const std::vector<std::string> defaultEffects = {"io_out"};
  const std::vector<std::string> entryDefaultEffects = {"io_err"};

  const auto entryActive =
      primec::ir_lowerer::resolveActiveEffects(transforms, true, defaultEffects, entryDefaultEffects);
  CHECK(entryActive.size() == 1);
  CHECK(entryActive.count("io_err") == 1);
  CHECK(entryActive.count("io_out") == 0);

  const auto nonEntryActive =
      primec::ir_lowerer::resolveActiveEffects(transforms, false, defaultEffects, entryDefaultEffects);
  CHECK(nonEntryActive.size() == 1);
  CHECK(nonEntryActive.count("io_out") == 1);
  CHECK(nonEntryActive.count("io_err") == 0);
}

TEST_CASE("ir lowerer effects unit rejects software numeric envelopes") {
  primec::Program program;
  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  primec::Transform returnTransform;
  returnTransform.name = "return";
  returnTransform.templateArgs.push_back("decimal");
  mainDef.transforms.push_back(returnTransform);
  program.definitions.push_back(std::move(mainDef));

  std::string error;
  CHECK_FALSE(primec::ir_lowerer::validateNoSoftwareNumericTypes(program, error));
  CHECK(error == "native backend does not support software numeric types: decimal");
}

TEST_CASE("ir lowerer effects unit validates program effect traversal") {
  auto makeEffectsTransform = [](const std::vector<std::string> &effects) {
    primec::Transform transform;
    transform.name = "effects";
    transform.arguments = effects;
    return transform;
  };

  primec::Program program;

  primec::Definition entryDef;
  entryDef.fullPath = "/main";
  entryDef.transforms.push_back(makeEffectsTransform({"io_out"}));

  primec::Expr parameterExpr;
  parameterExpr.transforms.push_back(makeEffectsTransform({"io_err"}));
  primec::Expr nestedParameterArg;
  nestedParameterArg.transforms.push_back(makeEffectsTransform({"heap_alloc"}));
  parameterExpr.args.push_back(nestedParameterArg);
  entryDef.parameters.push_back(parameterExpr);

  primec::Expr statementExpr;
  primec::Expr nestedBodyArg;
  nestedBodyArg.transforms.push_back(makeEffectsTransform({"file_write"}));
  statementExpr.bodyArguments.push_back(nestedBodyArg);
  entryDef.statements.push_back(statementExpr);

  primec::Expr returnExpr;
  returnExpr.transforms.push_back(makeEffectsTransform({"gpu_dispatch"}));
  entryDef.returnExpr = returnExpr;
  program.definitions.push_back(entryDef);

  primec::Execution execution;
  execution.fullPath = "/run";
  execution.transforms.push_back(makeEffectsTransform({"pathspace_notify"}));
  primec::Expr execArg;
  execArg.transforms.push_back(makeEffectsTransform({"pathspace_insert"}));
  execution.arguments.push_back(execArg);
  primec::Expr execBodyArg;
  execBodyArg.transforms.push_back(makeEffectsTransform({"pathspace_take"}));
  execution.bodyArguments.push_back(execBodyArg);
  program.executions.push_back(execution);

  std::string error;
  CHECK(primec::ir_lowerer::validateProgramEffects(program, "/main", {}, {}, error));
  CHECK(error.empty());
}

TEST_CASE("ir lowerer effects unit rejects unsupported nested expression effects") {
  primec::Program program;
  primec::Definition entryDef;
  entryDef.fullPath = "/main";

  primec::Expr statementExpr;
  primec::Expr nestedArg;
  primec::Transform badEffects;
  badEffects.name = "effects";
  badEffects.arguments = {"unsupported_effect"};
  nestedArg.transforms.push_back(badEffects);
  statementExpr.args.push_back(nestedArg);
  entryDef.statements.push_back(statementExpr);
  program.definitions.push_back(entryDef);

  std::string error;
  CHECK_FALSE(primec::ir_lowerer::validateProgramEffects(program, "/main", {}, {}, error));
  CHECK(error == "native backend does not support effect: unsupported_effect on /main");
}

TEST_CASE("ir lowerer effects unit resolves entry metadata masks") {
  primec::Definition entryDef;
  entryDef.fullPath = "/main";

  primec::Transform effects;
  effects.name = "effects";
  effects.arguments = {"io_out", "heap_alloc"};
  entryDef.transforms.push_back(effects);

  uint64_t entryEffectMask = 0;
  uint64_t entryCapabilityMask = 0;
  std::string error;
  CHECK(primec::ir_lowerer::resolveEntryMetadataMasks(
      entryDef, "/main", {"io_err"}, {"io_out"}, entryEffectMask, entryCapabilityMask, error));
  CHECK(error.empty());
  CHECK(entryEffectMask == (primec::EffectIoOut | primec::EffectHeapAlloc));
  CHECK(entryCapabilityMask == (primec::EffectIoOut | primec::EffectHeapAlloc));
}

TEST_CASE("ir lowerer effects unit rejects duplicate entry capabilities transform") {
  primec::Definition entryDef;
  entryDef.fullPath = "/main";

  primec::Transform capabilitiesA;
  capabilitiesA.name = "capabilities";
  capabilitiesA.arguments = {"io_out"};
  entryDef.transforms.push_back(capabilitiesA);

  primec::Transform capabilitiesB;
  capabilitiesB.name = "capabilities";
  capabilitiesB.arguments = {"io_err"};
  entryDef.transforms.push_back(capabilitiesB);

  uint64_t entryEffectMask = 0;
  uint64_t entryCapabilityMask = 0;
  std::string error;
  CHECK_FALSE(primec::ir_lowerer::resolveEntryMetadataMasks(
      entryDef, "/main", {"io_out"}, {"io_out"}, entryEffectMask, entryCapabilityMask, error));
  CHECK(error == "duplicate capabilities transform on /main");
}

TEST_CASE("ir lowerer call helpers resolve direct definition calls only") {
  primec::Definition callee;
  callee.fullPath = "/callee";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {{"/callee", &callee}};
  auto resolver = [](const primec::Expr &) { return std::string("/callee"); };

  primec::Expr directCall;
  directCall.kind = primec::Expr::Kind::Call;
  directCall.name = "callee";
  CHECK(primec::ir_lowerer::resolveDefinitionCall(directCall, defMap, resolver) == &callee);

  primec::Expr methodCall = directCall;
  methodCall.isMethodCall = true;
  CHECK(primec::ir_lowerer::resolveDefinitionCall(methodCall, defMap, resolver) == nullptr);

  primec::Expr bindingCall = directCall;
  bindingCall.isBinding = true;
  CHECK(primec::ir_lowerer::resolveDefinitionCall(bindingCall, defMap, resolver) == nullptr);
}

TEST_CASE("ir lowerer call helpers build definition call resolver") {
  primec::Definition callee;
  callee.fullPath = "/callee";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {{"/callee", &callee}};
  const auto resolveExprPath = [](const primec::Expr &) { return std::string("/callee"); };
  const auto resolveDefinitionCall =
      primec::ir_lowerer::makeResolveDefinitionCall(defMap, resolveExprPath);

  primec::Expr directCall;
  directCall.kind = primec::Expr::Kind::Call;
  directCall.name = "callee";
  CHECK(resolveDefinitionCall(directCall) == &callee);

  primec::Expr methodCall = directCall;
  methodCall.isMethodCall = true;
  CHECK(resolveDefinitionCall(methodCall) == nullptr);

  primec::Expr bindingCall = directCall;
  bindingCall.isBinding = true;
  CHECK(resolveDefinitionCall(bindingCall) == nullptr);
}

TEST_CASE("ir lowerer call helpers resolve definition paths") {
  primec::Definition callee;
  callee.fullPath = "/callee";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/callee", &callee},
      {"/null", nullptr},
  };

  CHECK(primec::ir_lowerer::resolveDefinitionByPath(defMap, "/callee") == &callee);
  CHECK(primec::ir_lowerer::resolveDefinitionByPath(defMap, "/missing") == nullptr);
  CHECK(primec::ir_lowerer::resolveDefinitionByPath(defMap, "/null") == nullptr);
}

TEST_CASE("ir lowerer call helpers resolve scoped call paths") {
  primec::Definition scopedDef;
  scopedDef.fullPath = "/pkg/foo";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {{"/pkg/foo", &scopedDef}};
  const std::unordered_map<std::string, std::string> importAliases = {
      {"foo", "/import/foo"},
      {"bar", "/import/bar"},
  };

  primec::Expr absolute;
  absolute.name = "/absolute";
  CHECK(primec::ir_lowerer::resolveCallPathFromScope(absolute, defMap, importAliases) == "/absolute");

  primec::Expr namespacedScoped;
  namespacedScoped.name = "foo";
  namespacedScoped.namespacePrefix = "/pkg";
  CHECK(primec::ir_lowerer::resolveCallPathFromScope(namespacedScoped, defMap, importAliases) == "/pkg/foo");

  primec::Expr namespacedAlias;
  namespacedAlias.name = "bar";
  namespacedAlias.namespacePrefix = "/pkg";
  CHECK(primec::ir_lowerer::resolveCallPathFromScope(namespacedAlias, defMap, importAliases) == "/import/bar");

  primec::Expr namespacedFallback;
  namespacedFallback.name = "baz";
  namespacedFallback.namespacePrefix = "/pkg";
  CHECK(primec::ir_lowerer::resolveCallPathFromScope(namespacedFallback, defMap, importAliases) == "/pkg/baz");

  primec::Expr rootAlias;
  rootAlias.name = "foo";
  CHECK(primec::ir_lowerer::resolveCallPathFromScope(rootAlias, defMap, importAliases) == "/import/foo");

  primec::Expr rootFallback;
  rootFallback.name = "main";
  CHECK(primec::ir_lowerer::resolveCallPathFromScope(rootFallback, defMap, importAliases) == "/main");
}

TEST_CASE("ir lowerer call helpers build scoped call path resolver") {
  primec::Definition scopedDef;
  scopedDef.fullPath = "/pkg/foo";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {{"/pkg/foo", &scopedDef}};
  const std::unordered_map<std::string, std::string> importAliases = {
      {"foo", "/import/foo"},
      {"bar", "/import/bar"},
  };
  auto resolveExprPath = primec::ir_lowerer::makeResolveCallPathFromScope(defMap, importAliases);

  primec::Expr namespacedScoped;
  namespacedScoped.name = "foo";
  namespacedScoped.namespacePrefix = "/pkg";
  CHECK(resolveExprPath(namespacedScoped) == "/pkg/foo");

  primec::Expr namespacedAlias;
  namespacedAlias.name = "bar";
  namespacedAlias.namespacePrefix = "/pkg";
  CHECK(resolveExprPath(namespacedAlias) == "/import/bar");
}

TEST_CASE("ir lowerer call helpers resolve definition namespace prefixes") {
  primec::Definition namespacedDef;
  namespacedDef.fullPath = "/pkg/foo";
  namespacedDef.namespacePrefix = "/pkg";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/pkg/foo", &namespacedDef},
      {"/pkg/null", nullptr},
  };

  CHECK(primec::ir_lowerer::resolveDefinitionNamespacePrefix(defMap, "/pkg/foo") == "/pkg");
  CHECK(primec::ir_lowerer::resolveDefinitionNamespacePrefix(defMap, "/pkg/missing").empty());
  CHECK(primec::ir_lowerer::resolveDefinitionNamespacePrefix(defMap, "/pkg/null").empty());
}

TEST_CASE("ir lowerer call helpers classify tail call candidates") {
  primec::Definition callee;
  callee.fullPath = "/callee";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {{"/callee", &callee}};
  const std::unordered_map<std::string, std::string> importAliases = {};
  auto resolveExprPath = [&](const primec::Expr &expr) {
    return primec::ir_lowerer::resolveCallPathFromScope(expr, defMap, importAliases);
  };

  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "callee";
  CHECK(primec::ir_lowerer::isTailCallCandidate(callExpr, defMap, resolveExprPath));

  primec::Expr methodCall = callExpr;
  methodCall.isMethodCall = true;
  CHECK_FALSE(primec::ir_lowerer::isTailCallCandidate(methodCall, defMap, resolveExprPath));

  primec::Expr nameExpr;
  nameExpr.kind = primec::Expr::Kind::Name;
  nameExpr.name = "callee";
  CHECK_FALSE(primec::ir_lowerer::isTailCallCandidate(nameExpr, defMap, resolveExprPath));

  primec::Expr unknownCall = callExpr;
  unknownCall.name = "missing";
  CHECK_FALSE(primec::ir_lowerer::isTailCallCandidate(unknownCall, defMap, resolveExprPath));
}

TEST_CASE("ir lowerer call helpers build tail-call and definition-exists adapters") {
  primec::Definition callee;
  callee.fullPath = "/callee";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/callee", &callee},
      {"/null", nullptr},
  };
  const std::unordered_map<std::string, std::string> importAliases = {};

  auto resolveExprPath = primec::ir_lowerer::makeResolveCallPathFromScope(defMap, importAliases);
  auto isTailCallCandidate = primec::ir_lowerer::makeIsTailCallCandidate(defMap, resolveExprPath);
  auto definitionExists = primec::ir_lowerer::makeDefinitionExistsByPath(defMap);

  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "callee";
  CHECK(isTailCallCandidate(callExpr));

  primec::Expr unknownCall = callExpr;
  unknownCall.name = "missing";
  CHECK_FALSE(isTailCallCandidate(unknownCall));

  CHECK(definitionExists("/callee"));
  CHECK_FALSE(definitionExists("/missing"));
  CHECK_FALSE(definitionExists("/null"));
}

TEST_CASE("ir lowerer call helpers build bundled call-resolution adapters") {
  primec::Definition callee;
  callee.fullPath = "/callee";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/callee", &callee},
      {"/null", nullptr},
  };
  const std::unordered_map<std::string, std::string> importAliases = {{"callee", "/callee"}};

  auto adapters = primec::ir_lowerer::makeCallResolutionAdapters(defMap, importAliases);

  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "callee";
  CHECK(adapters.resolveExprPath(callExpr) == "/callee");
  CHECK(adapters.isTailCallCandidate(callExpr));

  primec::Expr unknownCall = callExpr;
  unknownCall.name = "missing";
  CHECK_FALSE(adapters.isTailCallCandidate(unknownCall));

  CHECK(adapters.definitionExists("/callee"));
  CHECK_FALSE(adapters.definitionExists("/missing"));
  CHECK_FALSE(adapters.definitionExists("/null"));
}

TEST_CASE("ir lowerer call helpers detect tail execution candidates from statements") {
  auto isTailCandidate = [](const primec::Expr &expr) {
    return expr.kind == primec::Expr::Kind::Call && expr.name == "callee";
  };

  std::vector<primec::Expr> statements;
  CHECK_FALSE(primec::ir_lowerer::hasTailExecutionCandidate(statements, true, isTailCandidate));

  primec::Expr directTail;
  directTail.kind = primec::Expr::Kind::Call;
  directTail.name = "callee";
  statements = {directTail};
  CHECK(primec::ir_lowerer::hasTailExecutionCandidate(statements, true, isTailCandidate));
  CHECK_FALSE(primec::ir_lowerer::hasTailExecutionCandidate(statements, false, isTailCandidate));

  primec::Expr returnCall;
  returnCall.kind = primec::Expr::Kind::Call;
  returnCall.name = "return";
  returnCall.args = {directTail};
  statements = {returnCall};
  CHECK(primec::ir_lowerer::hasTailExecutionCandidate(statements, false, isTailCandidate));

  primec::Expr nonTail;
  nonTail.kind = primec::Expr::Kind::Name;
  nonTail.name = "value";
  statements = {nonTail};
  CHECK_FALSE(primec::ir_lowerer::hasTailExecutionCandidate(statements, true, isTailCandidate));
}

TEST_CASE("ir lowerer call helpers build bundled entry call setup") {
  primec::Definition callee;
  callee.fullPath = "/callee";

  primec::Definition entryDef;
  entryDef.fullPath = "/entry";
  entryDef.namespacePrefix = "";
  primec::Expr tailCall;
  tailCall.kind = primec::Expr::Kind::Call;
  tailCall.name = "callee";
  entryDef.statements = {tailCall};

  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/callee", &callee},
      {"/entry", &entryDef},
  };
  const std::unordered_map<std::string, std::string> importAliases = {{"callee", "/callee"}};

  const auto voidSetup = primec::ir_lowerer::buildEntryCallResolutionSetup(
      entryDef, true, defMap, importAliases);
  CHECK(voidSetup.hasTailExecution);
  CHECK(voidSetup.adapters.resolveExprPath(tailCall) == "/callee");
  CHECK(voidSetup.adapters.isTailCallCandidate(tailCall));
  CHECK(voidSetup.adapters.definitionExists("/callee"));
  CHECK_FALSE(voidSetup.adapters.definitionExists("/missing"));

  const auto nonVoidSetup = primec::ir_lowerer::buildEntryCallResolutionSetup(
      entryDef, false, defMap, importAliases);
  CHECK_FALSE(nonVoidSetup.hasTailExecution);

  primec::Expr returnCall;
  returnCall.kind = primec::Expr::Kind::Call;
  returnCall.name = "return";
  returnCall.args = {tailCall};
  entryDef.statements = {returnCall};
  const auto returnSetup = primec::ir_lowerer::buildEntryCallResolutionSetup(
      entryDef, false, defMap, importAliases);
  CHECK(returnSetup.hasTailExecution);
}

TEST_CASE("ir lowerer call helpers order positional named and default args") {
  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "demo";
  primec::Expr argA;
  argA.kind = primec::Expr::Kind::Literal;
  argA.literalValue = 1;
  primec::Expr argC;
  argC.kind = primec::Expr::Kind::Literal;
  argC.literalValue = 3;
  callExpr.args = {argA, argC};
  callExpr.argNames = {std::nullopt, std::make_optional<std::string>("c")};

  primec::Expr paramA;
  paramA.name = "a";
  primec::Expr paramB;
  paramB.name = "b";
  primec::Expr defaultB;
  defaultB.kind = primec::Expr::Kind::Literal;
  defaultB.literalValue = 2;
  paramB.args.push_back(defaultB);
  primec::Expr paramC;
  paramC.name = "c";
  const std::vector<primec::Expr> params = {paramA, paramB, paramC};

  std::vector<const primec::Expr *> ordered;
  std::string error;
  REQUIRE(primec::ir_lowerer::buildOrderedCallArguments(callExpr, params, ordered, error));
  CHECK(error.empty());
  REQUIRE(ordered.size() == 3);
  REQUIRE(ordered[0] != nullptr);
  CHECK(ordered[0]->literalValue == 1);
  REQUIRE(ordered[1] != nullptr);
  CHECK(ordered[1]->literalValue == 2);
  REQUIRE(ordered[2] != nullptr);
  CHECK(ordered[2]->literalValue == 3);
}

TEST_CASE("ir lowerer call helpers reject unknown named arg") {
  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "demo";
  primec::Expr arg;
  arg.kind = primec::Expr::Kind::Literal;
  arg.literalValue = 1;
  callExpr.args = {arg};
  callExpr.argNames = {std::make_optional<std::string>("missing")};

  primec::Expr param;
  param.name = "a";
  const std::vector<primec::Expr> params = {param};

  std::vector<const primec::Expr *> ordered;
  std::string error;
  CHECK_FALSE(primec::ir_lowerer::buildOrderedCallArguments(callExpr, params, ordered, error));
  CHECK(error == "unknown named argument: missing");
}

TEST_CASE("ir lowerer call helpers classify struct helpers and transforms") {
  const std::unordered_set<std::string> structNames = {"/pkg/MyStruct"};
  std::string parentStructPath;

  primec::Definition notNested;
  notNested.fullPath = "/pkg/MyStruct/helper";
  CHECK_FALSE(primec::ir_lowerer::isStructHelperDefinition(
      notNested, structNames, parentStructPath));

  primec::Definition structDef;
  structDef.isNested = true;
  structDef.fullPath = "/pkg/MyStruct";
  CHECK_FALSE(primec::ir_lowerer::isStructHelperDefinition(
      structDef, structNames, parentStructPath));

  primec::Definition helperDef;
  helperDef.isNested = true;
  helperDef.fullPath = "/pkg/MyStruct/helper";
  CHECK(primec::ir_lowerer::isStructHelperDefinition(
      helperDef, structNames, parentStructPath));
  CHECK(parentStructPath == "/pkg/MyStruct");

  primec::Transform staticTransform;
  staticTransform.name = "static";
  helperDef.transforms.push_back(staticTransform);
  CHECK(primec::ir_lowerer::definitionHasTransform(helperDef, "static"));
  CHECK_FALSE(primec::ir_lowerer::definitionHasTransform(helperDef, "mut"));
}

TEST_CASE("ir lowerer call helpers classify struct definitions") {
  CHECK(primec::ir_lowerer::isStructTransformName("struct"));
  CHECK(primec::ir_lowerer::isStructTransformName("pod"));
  CHECK(primec::ir_lowerer::isStructTransformName("handle"));
  CHECK(primec::ir_lowerer::isStructTransformName("gpu_lane"));
  CHECK(primec::ir_lowerer::isStructTransformName("no_padding"));
  CHECK(primec::ir_lowerer::isStructTransformName("platform_independent_padding"));
  CHECK_FALSE(primec::ir_lowerer::isStructTransformName("return"));

  primec::Definition transformedStruct;
  transformedStruct.fullPath = "/pkg/StructWithTransform";
  primec::Transform structTransform;
  structTransform.name = "struct";
  transformedStruct.transforms.push_back(structTransform);
  CHECK(primec::ir_lowerer::isStructDefinition(transformedStruct));

  primec::Definition implicitStruct;
  implicitStruct.fullPath = "/pkg/ImplicitStruct";
  primec::Expr field;
  field.kind = primec::Expr::Kind::Name;
  field.isBinding = true;
  field.name = "value";
  implicitStruct.statements.push_back(field);
  CHECK(primec::ir_lowerer::isStructDefinition(implicitStruct));

  primec::Definition returnDef = implicitStruct;
  primec::Transform returnTransform;
  returnTransform.name = "return";
  returnDef.transforms.push_back(returnTransform);
  CHECK_FALSE(primec::ir_lowerer::isStructDefinition(returnDef));

  primec::Definition paramDef = implicitStruct;
  paramDef.parameters.push_back(field);
  CHECK_FALSE(primec::ir_lowerer::isStructDefinition(paramDef));

  primec::Definition mixedDef = implicitStruct;
  primec::Expr callStmt;
  callStmt.kind = primec::Expr::Kind::Call;
  callStmt.name = "doThing";
  callStmt.isBinding = false;
  mixedDef.statements.push_back(callStmt);
  CHECK_FALSE(primec::ir_lowerer::isStructDefinition(mixedDef));
}

TEST_CASE("ir lowerer struct type helpers build setup import aliases") {
  std::string prefix;
  CHECK(primec::ir_lowerer::isWildcardImportPath("/pkg/*", prefix));
  CHECK(prefix == "/pkg");
  CHECK(primec::ir_lowerer::isWildcardImportPath("/single", prefix));
  CHECK(prefix == "/single");
  CHECK_FALSE(primec::ir_lowerer::isWildcardImportPath("/pkg/nested", prefix));

  primec::Definition foo;
  foo.fullPath = "/pkg/foo";
  primec::Definition bar;
  bar.fullPath = "/pkg/bar";
  primec::Definition nested;
  nested.fullPath = "/pkg/nested/baz";
  primec::Definition single;
  single.fullPath = "/single";
  const std::vector<primec::Definition> definitions = {foo, bar, nested, single};
  std::unordered_map<std::string, const primec::Definition *> defMap;
  for (const auto &def : definitions) {
    defMap.emplace(def.fullPath, &def);
  }

  const std::vector<std::string> imports = {
      "/pkg/*",
      "/single",
      "/missing",
      "relative",
      "/pkg/bar",
  };
  const auto aliases =
      primec::ir_lowerer::buildImportAliasesFromProgram(imports, definitions, defMap);
  CHECK(aliases.size() == 3);
  CHECK(aliases.at("foo") == "/pkg/foo");
  CHECK(aliases.at("bar") == "/pkg/bar");
  CHECK(aliases.at("single") == "/single");
  CHECK(aliases.count("baz") == 0);
}

TEST_CASE("ir lowerer struct type helpers build definition map and struct names") {
  primec::Definition transformedStruct;
  transformedStruct.fullPath = "/pkg/StructA";
  primec::Transform structTransform;
  structTransform.name = "struct";
  transformedStruct.transforms.push_back(structTransform);

  primec::Definition implicitStruct;
  implicitStruct.fullPath = "/pkg/StructB";
  primec::Expr field;
  field.kind = primec::Expr::Kind::Name;
  field.isBinding = true;
  field.name = "value";
  implicitStruct.statements.push_back(field);

  primec::Definition nonStruct;
  nonStruct.fullPath = "/pkg/Func";
  nonStruct.parameters.push_back(field);

  const std::vector<primec::Definition> definitions = {
      transformedStruct,
      implicitStruct,
      nonStruct,
  };

  std::unordered_map<std::string, const primec::Definition *> defMap;
  std::unordered_set<std::string> structNames;
  primec::ir_lowerer::buildDefinitionMapAndStructNames(definitions, defMap, structNames);

  CHECK(defMap.size() == 3u);
  REQUIRE(defMap.count("/pkg/StructA") == 1u);
  REQUIRE(defMap.count("/pkg/StructB") == 1u);
  REQUIRE(defMap.count("/pkg/Func") == 1u);
  CHECK(defMap.at("/pkg/StructA") == &definitions[0]);
  CHECK(defMap.at("/pkg/StructB") == &definitions[1]);
  CHECK(defMap.at("/pkg/Func") == &definitions[2]);

  CHECK(structNames.size() == 2u);
  CHECK(structNames.count("/pkg/StructA") == 1u);
  CHECK(structNames.count("/pkg/StructB") == 1u);
  CHECK(structNames.count("/pkg/Func") == 0u);

  primec::ir_lowerer::buildDefinitionMapAndStructNames({}, defMap, structNames);
  CHECK(defMap.empty());
  CHECK(structNames.empty());
}

TEST_CASE("ir lowerer struct type helpers append layout fields from bindings") {
  primec::Definition structDef;
  structDef.fullPath = "/pkg/S";

  primec::Expr firstField;
  firstField.kind = primec::Expr::Kind::Name;
  firstField.isBinding = true;
  firstField.name = "a";

  primec::Expr nonBindingStmt;
  nonBindingStmt.kind = primec::Expr::Kind::Call;
  nonBindingStmt.name = "noop";

  primec::Expr secondField;
  secondField.kind = primec::Expr::Kind::Name;
  secondField.isBinding = true;
  secondField.name = "b";
  primec::Transform staticTransform;
  staticTransform.name = "static";
  secondField.transforms.push_back(staticTransform);

  primec::Expr thirdField;
  thirdField.kind = primec::Expr::Kind::Name;
  thirdField.isBinding = true;
  thirdField.name = "c";

  structDef.statements = {firstField, nonBindingStmt, secondField, thirdField};

  std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/pkg/S", &structDef},
  };
  const std::unordered_map<std::string, std::vector<primec::ir_lowerer::LayoutFieldBinding>> structFieldInfoByName = {
      {"/pkg/S", {{"i32", ""}, {"array", "i32"}}},
      {"/pkg/Missing", {{"i64", ""}}},
  };

  std::vector<std::pair<std::string, primec::ir_lowerer::StructLayoutFieldInfo>> appended;
  primec::ir_lowerer::appendStructLayoutFieldsFromFieldBindings(
      structFieldInfoByName,
      defMap,
      [&](const std::string &structPath, const primec::ir_lowerer::StructLayoutFieldInfo &field) {
        appended.emplace_back(structPath, field);
      });

  REQUIRE(appended.size() == 2u);
  CHECK(appended[0].first == "/pkg/S");
  CHECK(appended[0].second.name == "a");
  CHECK(appended[0].second.typeName == "i32");
  CHECK(appended[0].second.typeTemplateArg.empty());
  CHECK_FALSE(appended[0].second.isStatic);

  CHECK(appended[1].first == "/pkg/S");
  CHECK(appended[1].second.name == "b");
  CHECK(appended[1].second.typeName == "array");
  CHECK(appended[1].second.typeTemplateArg == "i32");
  CHECK(appended[1].second.isStatic);
}

TEST_CASE("ir lowerer struct type helpers resolve setup import paths") {
  const std::unordered_set<std::string> structNames = {
      "/pkg/Foo",
      "/alias/Foo",
  };
  const std::unordered_map<std::string, std::string> importAliases = {
      {"AliasFoo", "/alias/Foo"},
      {"Thing", "/pkg/Thing"},
  };

  CHECK(primec::ir_lowerer::resolveStructTypePathCandidateFromScope(
            "/external/Type", "/pkg", structNames, importAliases) == "/external/Type");
  CHECK(primec::ir_lowerer::resolveStructTypePathCandidateFromScope(
            "Foo", "/pkg", structNames, importAliases) == "/pkg/Foo");
  CHECK(primec::ir_lowerer::resolveStructTypePathCandidateFromScope(
            "AliasFoo", "", structNames, importAliases) == "/alias/Foo");
  CHECK(primec::ir_lowerer::resolveStructTypePathCandidateFromScope(
            "Missing", "/pkg", structNames, importAliases) == "/pkg/Missing");

  primec::Definition thingDef;
  thingDef.fullPath = "/pkg/Thing";
  primec::Definition rootDef;
  rootDef.fullPath = "/root";
  std::unordered_map<std::string, const primec::Definition *> defMap = {
      {thingDef.fullPath, &thingDef},
      {rootDef.fullPath, &rootDef},
  };

  primec::Expr scopedExpr;
  scopedExpr.kind = primec::Expr::Kind::Name;
  scopedExpr.name = "Thing";
  scopedExpr.namespacePrefix = "/pkg/sub";
  CHECK(primec::ir_lowerer::resolveStructLayoutExprPathFromScope(
            scopedExpr, defMap, importAliases) == "/pkg/Thing");

  primec::Expr aliasExpr = scopedExpr;
  aliasExpr.name = "Thing";
  aliasExpr.namespacePrefix = "/other";
  CHECK(primec::ir_lowerer::resolveStructLayoutExprPathFromScope(
            aliasExpr, defMap, importAliases) == "/pkg/Thing");

  primec::Expr rootExpr;
  rootExpr.kind = primec::Expr::Kind::Name;
  rootExpr.name = "root";
  CHECK(primec::ir_lowerer::resolveStructLayoutExprPathFromScope(
            rootExpr, defMap, importAliases) == "/root");

  primec::Expr unresolvedExpr;
  unresolvedExpr.kind = primec::Expr::Kind::Name;
  unresolvedExpr.name = "missing";
  unresolvedExpr.namespacePrefix = "/pkg";
  CHECK(primec::ir_lowerer::resolveStructLayoutExprPathFromScope(
            unresolvedExpr, defMap, importAliases) == "/pkg/missing");

  primec::Expr suffixExpr;
  suffixExpr.kind = primec::Expr::Kind::Name;
  suffixExpr.name = "Thing";
  suffixExpr.namespacePrefix = "/pkg/Thing";
  CHECK(primec::ir_lowerer::resolveStructLayoutExprPathFromScope(
            suffixExpr, defMap, importAliases) == "/pkg/Thing");

  primec::Expr absoluteExpr;
  absoluteExpr.kind = primec::Expr::Kind::Name;
  absoluteExpr.name = "/explicit/Thing";
  CHECK(primec::ir_lowerer::resolveStructLayoutExprPathFromScope(
            absoluteExpr, defMap, importAliases) == "/explicit/Thing");

  primec::Expr slashExpr;
  slashExpr.kind = primec::Expr::Kind::Name;
  slashExpr.name = "pkg/Thing";
  CHECK(primec::ir_lowerer::resolveStructLayoutExprPathFromScope(
            slashExpr, defMap, importAliases) == "/pkg/Thing");
}

TEST_CASE("ir lowerer struct field binding helpers resolve envelope values") {
  primec::Expr bindingExpr;
  bindingExpr.kind = primec::Expr::Kind::Name;
  bindingExpr.isBinding = true;
  bindingExpr.name = "tmp";

  primec::Expr literalExpr;
  literalExpr.kind = primec::Expr::Kind::Literal;
  literalExpr.literalValue = 7;

  primec::Expr blockCall;
  blockCall.kind = primec::Expr::Kind::Call;
  blockCall.name = "block";
  blockCall.hasBodyArguments = true;
  blockCall.bodyArguments = {bindingExpr, literalExpr};

  const primec::Expr *valueExpr =
      primec::ir_lowerer::getEnvelopeValueExpr(blockCall, false);
  REQUIRE(valueExpr != nullptr);
  CHECK(valueExpr->kind == primec::Expr::Kind::Literal);
  CHECK(valueExpr->literalValue == 7);

  primec::Expr notBlock = blockCall;
  notBlock.name = "custom";
  CHECK(primec::ir_lowerer::getEnvelopeValueExpr(notBlock, false) == nullptr);
  CHECK(primec::ir_lowerer::getEnvelopeValueExpr(notBlock, true) != nullptr);

  primec::Expr withArgs = blockCall;
  withArgs.args.push_back(literalExpr);
  CHECK(primec::ir_lowerer::getEnvelopeValueExpr(withArgs, false) == nullptr);
}

TEST_CASE("ir lowerer struct field binding helpers infer primitive bindings") {
  const std::unordered_map<std::string, primec::ir_lowerer::LayoutFieldBinding> knownFields = {
      {"existing", {"i64", ""}},
  };
  primec::ir_lowerer::LayoutFieldBinding inferred;

  primec::Expr literalExpr;
  literalExpr.kind = primec::Expr::Kind::Literal;
  literalExpr.intWidth = 64;
  CHECK(primec::ir_lowerer::inferPrimitiveFieldBinding(
      literalExpr, knownFields, inferred));
  CHECK(inferred.typeName == "i64");

  primec::Expr nameExpr;
  nameExpr.kind = primec::Expr::Kind::Name;
  nameExpr.name = "existing";
  CHECK(primec::ir_lowerer::inferPrimitiveFieldBinding(
      nameExpr, knownFields, inferred));
  CHECK(inferred.typeName == "i64");

  primec::Expr arrayCall;
  arrayCall.kind = primec::Expr::Kind::Call;
  arrayCall.name = "array";
  arrayCall.templateArgs = {"f32"};
  CHECK(primec::ir_lowerer::inferPrimitiveFieldBinding(
      arrayCall, knownFields, inferred));
  CHECK(inferred.typeName == "array");
  CHECK(inferred.typeTemplateArg == "f32");

  primec::Expr mapCall;
  mapCall.kind = primec::Expr::Kind::Call;
  mapCall.name = "map";
  mapCall.templateArgs = {"i32", "bool"};
  CHECK(primec::ir_lowerer::inferPrimitiveFieldBinding(
      mapCall, knownFields, inferred));
  CHECK(inferred.typeName == "map");
  CHECK(inferred.typeTemplateArg == "i32, bool");

  primec::Expr convertCall;
  convertCall.kind = primec::Expr::Kind::Call;
  convertCall.name = "convert";
  convertCall.templateArgs = {"int"};
  CHECK(primec::ir_lowerer::inferPrimitiveFieldBinding(
      convertCall, knownFields, inferred));
  CHECK(inferred.typeName == "i32");

  primec::Expr unsupportedConvert = convertCall;
  unsupportedConvert.templateArgs = {"Widget"};
  CHECK_FALSE(primec::ir_lowerer::inferPrimitiveFieldBinding(
      unsupportedConvert, knownFields, inferred));

  primec::Expr envelopeCall;
  envelopeCall.kind = primec::Expr::Kind::Call;
  envelopeCall.name = "block";
  envelopeCall.hasBodyArguments = true;
  envelopeCall.bodyArguments = {literalExpr};
  CHECK(primec::ir_lowerer::inferPrimitiveFieldBinding(
      envelopeCall, knownFields, inferred));
  CHECK(inferred.typeName == "i64");
}

TEST_CASE("ir lowerer struct field binding helpers extract explicit envelopes") {
  primec::Expr typedExpr;
  primec::Transform effectTransform;
  effectTransform.name = "effects";
  effectTransform.arguments = {"io_out"};
  primec::Transform publicTransform;
  publicTransform.name = "public";
  primec::Transform typeTransform;
  typeTransform.name = "i32";
  typedExpr.transforms = {effectTransform, publicTransform, typeTransform};

  primec::ir_lowerer::LayoutFieldBinding binding;
  CHECK(primec::ir_lowerer::extractExplicitLayoutFieldBinding(typedExpr, binding));
  CHECK(binding.typeName == "i32");
  CHECK(binding.typeTemplateArg.empty());

  primec::Expr templatedExpr;
  primec::Transform arrayTransform;
  arrayTransform.name = "array";
  arrayTransform.templateArgs = {"f32"};
  templatedExpr.transforms = {publicTransform, arrayTransform};
  CHECK(primec::ir_lowerer::extractExplicitLayoutFieldBinding(templatedExpr, binding));
  CHECK(binding.typeName == "array");
  CHECK(binding.typeTemplateArg == "f32");

  primec::Expr qualifierOnlyExpr;
  primec::Transform alignTransform;
  alignTransform.name = "align_bytes";
  alignTransform.arguments = {"16"};
  qualifierOnlyExpr.transforms = {publicTransform, alignTransform};
  CHECK_FALSE(primec::ir_lowerer::extractExplicitLayoutFieldBinding(
      qualifierOnlyExpr, binding));
}

TEST_CASE("ir lowerer struct field binding helpers format envelopes") {
  primec::ir_lowerer::LayoutFieldBinding plain;
  plain.typeName = "i64";
  CHECK(primec::ir_lowerer::formatLayoutFieldEnvelope(plain) == "i64");

  primec::ir_lowerer::LayoutFieldBinding templated;
  templated.typeName = "map";
  templated.typeTemplateArg = "i32, bool";
  CHECK(primec::ir_lowerer::formatLayoutFieldEnvelope(templated) == "map<i32, bool>");
}

TEST_CASE("ir lowerer struct field binding helpers resolve layout bindings") {
  const std::unordered_set<std::string> structNames = {
      "/pkg/A",
  };
  const std::unordered_map<std::string, std::string> importAliases;
  const auto resolveStructTypePath = [&](const std::string &typeName, const std::string &namespacePrefix) {
    return primec::ir_lowerer::resolveStructTypePathCandidateFromScope(
        typeName, namespacePrefix, structNames, importAliases);
  };
  const auto resolveStructLayoutExprPath = [](const primec::Expr &expr) {
    if (!expr.name.empty() && expr.name[0] == '/') {
      return expr.name;
    }
    if (expr.name.find('/') != std::string::npos) {
      return "/" + expr.name;
    }
    if (!expr.namespacePrefix.empty()) {
      return expr.namespacePrefix + "/" + expr.name;
    }
    return std::string("/pkg/") + expr.name;
  };

  primec::Definition holderDef;
  holderDef.fullPath = "/pkg/Holder";
  holderDef.namespacePrefix = "/pkg";

  primec::Definition makeA;
  makeA.fullPath = "/pkg/makeA";
  makeA.namespacePrefix = "/pkg";
  primec::Transform returnA;
  returnA.name = "return";
  returnA.templateArgs = {"A"};
  makeA.transforms.push_back(returnA);

  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {makeA.fullPath, &makeA},
  };
  const std::unordered_map<std::string, primec::ir_lowerer::LayoutFieldBinding> knownFields = {
      {"existing", {"i64", ""}},
  };

  primec::Expr literalValue;
  literalValue.kind = primec::Expr::Kind::Literal;
  literalValue.intWidth = 64;

  primec::Expr explicitField;
  explicitField.kind = primec::Expr::Kind::Call;
  explicitField.isBinding = true;
  explicitField.name = "typed";
  primec::Transform explicitType;
  explicitType.name = "i32";
  explicitField.transforms = {explicitType};
  primec::ir_lowerer::LayoutFieldBinding binding;
  std::string error;
  CHECK(primec::ir_lowerer::resolveLayoutFieldBinding(holderDef,
                                                      explicitField,
                                                      knownFields,
                                                      structNames,
                                                      resolveStructTypePath,
                                                      resolveStructLayoutExprPath,
                                                      defMap,
                                                      binding,
                                                      error));
  CHECK(binding.typeName == "i32");
  CHECK(error.empty());

  primec::Expr primitiveField;
  primitiveField.kind = primec::Expr::Kind::Call;
  primitiveField.isBinding = true;
  primitiveField.name = "count";
  primitiveField.args = {literalValue};
  binding = {};
  error.clear();
  CHECK(primec::ir_lowerer::resolveLayoutFieldBinding(holderDef,
                                                      primitiveField,
                                                      knownFields,
                                                      structNames,
                                                      resolveStructTypePath,
                                                      resolveStructLayoutExprPath,
                                                      defMap,
                                                      binding,
                                                      error));
  CHECK(binding.typeName == "i64");
  CHECK(error.empty());

  primec::Expr makeCall;
  makeCall.kind = primec::Expr::Kind::Call;
  makeCall.name = "makeA";

  primec::Expr structField;
  structField.kind = primec::Expr::Kind::Call;
  structField.isBinding = true;
  structField.name = "child";
  structField.args = {makeCall};
  binding = {};
  error.clear();
  CHECK(primec::ir_lowerer::resolveLayoutFieldBinding(holderDef,
                                                      structField,
                                                      knownFields,
                                                      structNames,
                                                      resolveStructTypePath,
                                                      resolveStructLayoutExprPath,
                                                      defMap,
                                                      binding,
                                                      error));
  CHECK(binding.typeName == "/pkg/A");
  CHECK(error.empty());

  primec::Expr missingArgField;
  missingArgField.kind = primec::Expr::Kind::Call;
  missingArgField.isBinding = true;
  missingArgField.name = "badCount";
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::resolveLayoutFieldBinding(holderDef,
                                                            missingArgField,
                                                            knownFields,
                                                            structNames,
                                                            resolveStructTypePath,
                                                            resolveStructLayoutExprPath,
                                                            defMap,
                                                            binding,
                                                            error));
  CHECK(error == "omitted struct field envelope requires exactly one initializer: /pkg/Holder/badCount");

  primec::Expr unresolvedName;
  unresolvedName.kind = primec::Expr::Kind::Name;
  unresolvedName.name = "missing";

  primec::Expr unresolvedField;
  unresolvedField.kind = primec::Expr::Kind::Call;
  unresolvedField.isBinding = true;
  unresolvedField.name = "badType";
  unresolvedField.args = {unresolvedName};
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::resolveLayoutFieldBinding(holderDef,
                                                            unresolvedField,
                                                            knownFields,
                                                            structNames,
                                                            resolveStructTypePath,
                                                            resolveStructLayoutExprPath,
                                                            defMap,
                                                            binding,
                                                            error));
  CHECK(error == "unresolved or ambiguous omitted struct field envelope: /pkg/Holder/badType");
}

TEST_CASE("ir lowerer struct field binding helpers collect struct layout bindings") {
  primec::Program program;

  primec::Definition structA;
  structA.fullPath = "/pkg/A";
  structA.namespacePrefix = "/pkg";
  primec::Transform structTransform;
  structTransform.name = "struct";
  structA.transforms = {structTransform};

  primec::Definition makeA;
  makeA.fullPath = "/pkg/makeA";
  makeA.namespacePrefix = "/pkg";
  primec::Transform returnA;
  returnA.name = "return";
  returnA.templateArgs = {"A"};
  makeA.transforms.push_back(returnA);

  primec::Definition structS;
  structS.fullPath = "/pkg/S";
  structS.namespacePrefix = "/pkg";
  structS.transforms = {structTransform};

  primec::Expr typedField;
  typedField.kind = primec::Expr::Kind::Call;
  typedField.isBinding = true;
  typedField.name = "typed";
  primec::Transform typeI32;
  typeI32.name = "i32";
  typedField.transforms = {typeI32};

  primec::Expr typedRef;
  typedRef.kind = primec::Expr::Kind::Name;
  typedRef.name = "typed";

  primec::Expr copiedField;
  copiedField.kind = primec::Expr::Kind::Call;
  copiedField.isBinding = true;
  copiedField.name = "copied";
  copiedField.args = {typedRef};

  primec::Expr makeCall;
  makeCall.kind = primec::Expr::Kind::Call;
  makeCall.name = "makeA";

  primec::Expr childField;
  childField.kind = primec::Expr::Kind::Call;
  childField.isBinding = true;
  childField.name = "child";
  childField.args = {makeCall};

  structS.statements = {typedField, copiedField, childField};

  program.definitions = {structA, makeA, structS};

  std::unordered_map<std::string, const primec::Definition *> defMap;
  for (const auto &def : program.definitions) {
    defMap.emplace(def.fullPath, &def);
  }

  const std::unordered_set<std::string> structNames = {
      "/pkg/A",
      "/pkg/S",
  };
  const std::unordered_map<std::string, std::string> importAliases;
  const auto resolveStructTypePath = [&](const std::string &typeName, const std::string &namespacePrefix) {
    return primec::ir_lowerer::resolveStructTypePathCandidateFromScope(
        typeName, namespacePrefix, structNames, importAliases);
  };
  const auto resolveStructLayoutExprPath = [](const primec::Expr &expr) {
    if (!expr.name.empty() && expr.name[0] == '/') {
      return expr.name;
    }
    if (expr.name.find('/') != std::string::npos) {
      return "/" + expr.name;
    }
    if (!expr.namespacePrefix.empty()) {
      return expr.namespacePrefix + "/" + expr.name;
    }
    return std::string("/pkg/") + expr.name;
  };

  std::unordered_map<std::string, std::vector<primec::ir_lowerer::LayoutFieldBinding>> fieldsByStruct;
  std::string error;
  CHECK(primec::ir_lowerer::collectStructLayoutFieldBindings(program,
                                                             structNames,
                                                             resolveStructTypePath,
                                                             resolveStructLayoutExprPath,
                                                             defMap,
                                                             fieldsByStruct,
                                                             error));
  CHECK(error.empty());
  REQUIRE(fieldsByStruct.count("/pkg/S") == 1u);
  REQUIRE(fieldsByStruct.count("/pkg/A") == 1u);
  REQUIRE(fieldsByStruct["/pkg/S"].size() == 3u);
  CHECK(fieldsByStruct["/pkg/S"][0].typeName == "i32");
  CHECK(fieldsByStruct["/pkg/S"][1].typeName == "i32");
  CHECK(fieldsByStruct["/pkg/S"][2].typeName == "/pkg/A");
  CHECK(fieldsByStruct["/pkg/A"].empty());

  primec::Program badProgram;
  primec::Definition badStruct;
  badStruct.fullPath = "/pkg/Bad";
  badStruct.namespacePrefix = "/pkg";
  badStruct.transforms = {structTransform};
  primec::Expr badField;
  badField.kind = primec::Expr::Kind::Call;
  badField.isBinding = true;
  badField.name = "bad";
  badStruct.statements = {badField};
  badProgram.definitions = {badStruct};

  std::unordered_map<std::string, const primec::Definition *> badDefMap = {
      {badStruct.fullPath, &badProgram.definitions[0]},
  };
  const std::unordered_set<std::string> badStructNames = {
      "/pkg/Bad",
  };
  const auto badResolveStructTypePath = [&](const std::string &typeName, const std::string &namespacePrefix) {
    return primec::ir_lowerer::resolveStructTypePathCandidateFromScope(
        typeName, namespacePrefix, badStructNames, importAliases);
  };
  fieldsByStruct.clear();
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::collectStructLayoutFieldBindings(badProgram,
                                                                   badStructNames,
                                                                   badResolveStructTypePath,
                                                                   resolveStructLayoutExprPath,
                                                                   badDefMap,
                                                                   fieldsByStruct,
                                                                   error));
  CHECK(error == "omitted struct field envelope requires exactly one initializer: /pkg/Bad/bad");
}

TEST_CASE("ir lowerer struct field binding helpers collect layout bindings from program context") {
  primec::Program program;

  primec::Definition structA;
  structA.fullPath = "/pkg/A";
  structA.namespacePrefix = "/pkg";
  primec::Transform structTransform;
  structTransform.name = "struct";
  structA.transforms = {structTransform};

  primec::Definition structS;
  structS.fullPath = "/pkg/S";
  structS.namespacePrefix = "/pkg";
  structS.transforms = {structTransform};

  primec::Expr typedField;
  typedField.kind = primec::Expr::Kind::Call;
  typedField.isBinding = true;
  typedField.name = "typed";
  primec::Transform typeI32;
  typeI32.name = "i32";
  typedField.transforms = {typeI32};

  primec::Expr childCtor;
  childCtor.kind = primec::Expr::Kind::Call;
  childCtor.name = "/pkg/A";
  primec::Expr childField;
  childField.kind = primec::Expr::Kind::Call;
  childField.isBinding = true;
  childField.name = "child";
  childField.args = {childCtor};

  structS.statements = {typedField, childField};
  program.definitions = {structA, structS};

  std::unordered_map<std::string, const primec::Definition *> defMap;
  std::unordered_set<std::string> structNames;
  primec::ir_lowerer::buildDefinitionMapAndStructNames(program.definitions, defMap, structNames);
  const std::unordered_map<std::string, std::string> importAliases;
  const auto resolveStructTypePath = [&](const std::string &typeName, const std::string &namespacePrefix) {
    return primec::ir_lowerer::resolveStructTypePathCandidateFromScope(
        typeName, namespacePrefix, structNames, importAliases);
  };

  std::unordered_map<std::string, std::vector<primec::ir_lowerer::LayoutFieldBinding>> fieldsByStruct;
  std::string error;
  CHECK(primec::ir_lowerer::collectStructLayoutFieldBindingsFromProgramContext(
      program, structNames, resolveStructTypePath, defMap, importAliases, fieldsByStruct, error));
  CHECK(error.empty());
  REQUIRE(fieldsByStruct.count("/pkg/S") == 1u);
  REQUIRE(fieldsByStruct["/pkg/S"].size() == 2u);
  CHECK(fieldsByStruct["/pkg/S"][0].typeName == "i32");
  CHECK(fieldsByStruct["/pkg/S"][1].typeName == "/pkg/A");

  primec::Program badProgram;
  primec::Definition badStruct;
  badStruct.fullPath = "/pkg/Bad";
  badStruct.namespacePrefix = "/pkg";
  badStruct.transforms = {structTransform};
  primec::Expr badField;
  badField.kind = primec::Expr::Kind::Call;
  badField.isBinding = true;
  badField.name = "bad";
  badStruct.statements = {badField};
  badProgram.definitions = {badStruct};

  std::unordered_map<std::string, const primec::Definition *> badDefMap;
  std::unordered_set<std::string> badStructNames;
  primec::ir_lowerer::buildDefinitionMapAndStructNames(badProgram.definitions, badDefMap, badStructNames);
  const auto badResolveStructTypePath = [&](const std::string &typeName, const std::string &namespacePrefix) {
    return primec::ir_lowerer::resolveStructTypePathCandidateFromScope(
        typeName, namespacePrefix, badStructNames, importAliases);
  };
  fieldsByStruct.clear();
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::collectStructLayoutFieldBindingsFromProgramContext(
      badProgram, badStructNames, badResolveStructTypePath, badDefMap, importAliases, fieldsByStruct, error));
  CHECK(error == "omitted struct field envelope requires exactly one initializer: /pkg/Bad/bad");
}

TEST_CASE("ir lowerer struct return path helpers infer from definitions") {
  const std::unordered_set<std::string> structNames = {
      "/pkg/A",
      "/pkg/B",
  };
  const std::unordered_map<std::string, std::string> importAliases;
  const auto resolveStructTypePath = [&](const std::string &typeName, const std::string &namespacePrefix) {
    return primec::ir_lowerer::resolveStructTypePathCandidateFromScope(
        typeName, namespacePrefix, structNames, importAliases);
  };
  const auto resolveStructLayoutExprPath = [](const primec::Expr &expr) {
    if (!expr.name.empty() && expr.name[0] == '/') {
      return expr.name;
    }
    if (expr.name.find('/') != std::string::npos) {
      return "/" + expr.name;
    }
    if (!expr.namespacePrefix.empty()) {
      return expr.namespacePrefix + "/" + expr.name;
    }
    return std::string("/pkg/") + expr.name;
  };

  primec::Definition makeA;
  makeA.fullPath = "/pkg/makeA";
  makeA.namespacePrefix = "/pkg";
  primec::Transform returnA;
  returnA.name = "return";
  returnA.templateArgs = {"A"};
  makeA.transforms.push_back(returnA);

  primec::Definition makeB;
  makeB.fullPath = "/pkg/makeB";
  makeB.namespacePrefix = "/pkg";
  primec::Transform returnB;
  returnB.name = "return";
  returnB.templateArgs = {"B"};
  makeB.transforms.push_back(returnB);

  primec::Definition mixed;
  mixed.fullPath = "/pkg/mixed";
  mixed.namespacePrefix = "/pkg";

  primec::Expr callA;
  callA.kind = primec::Expr::Kind::Call;
  callA.name = "makeA";

  primec::Expr callB = callA;
  callB.name = "makeB";

  primec::Expr retA;
  retA.kind = primec::Expr::Kind::Call;
  retA.name = "return";
  retA.args = {callA};

  primec::Expr retB = retA;
  retB.args = {callB};
  mixed.statements = {retA, retB};

  std::unordered_map<std::string, const primec::Definition *> defMap = {
      {makeA.fullPath, &makeA},
      {makeB.fullPath, &makeB},
      {mixed.fullPath, &mixed},
  };

  CHECK(primec::ir_lowerer::inferStructReturnPathFromDefinition(
            "/pkg/makeA", structNames, resolveStructTypePath, resolveStructLayoutExprPath, defMap) == "/pkg/A");
  CHECK(primec::ir_lowerer::inferStructReturnPathFromDefinition(
            "/pkg/mixed", structNames, resolveStructTypePath, resolveStructLayoutExprPath, defMap).empty());
}

TEST_CASE("ir lowerer struct return path helpers infer from expressions") {
  const std::unordered_set<std::string> structNames = {
      "/pkg/A",
      "/pkg/B",
      "/pkg/Receiver",
  };
  const std::unordered_map<std::string, std::string> importAliases;
  const auto resolveStructTypePath = [&](const std::string &typeName, const std::string &namespacePrefix) {
    return primec::ir_lowerer::resolveStructTypePathCandidateFromScope(
        typeName, namespacePrefix, structNames, importAliases);
  };
  const auto resolveStructLayoutExprPath = [](const primec::Expr &expr) {
    if (!expr.name.empty() && expr.name[0] == '/') {
      return expr.name;
    }
    if (expr.name.find('/') != std::string::npos) {
      return "/" + expr.name;
    }
    if (!expr.namespacePrefix.empty()) {
      return expr.namespacePrefix + "/" + expr.name;
    }
    return std::string("/pkg/") + expr.name;
  };

  primec::Definition makeA;
  makeA.fullPath = "/pkg/makeA";
  makeA.namespacePrefix = "/pkg";
  primec::Transform returnA;
  returnA.name = "return";
  returnA.templateArgs = {"A"};
  makeA.transforms.push_back(returnA);

  primec::Definition makeB;
  makeB.fullPath = "/pkg/makeB";
  makeB.namespacePrefix = "/pkg";
  primec::Transform returnB;
  returnB.name = "return";
  returnB.templateArgs = {"B"};
  makeB.transforms.push_back(returnB);

  primec::Definition receiverMethod;
  receiverMethod.fullPath = "/pkg/Receiver/makeA";
  receiverMethod.namespacePrefix = "/pkg/Receiver";
  receiverMethod.transforms.push_back(returnA);

  std::unordered_map<std::string, const primec::Definition *> defMap = {
      {makeA.fullPath, &makeA},
      {makeB.fullPath, &makeB},
      {receiverMethod.fullPath, &receiverMethod},
  };

  const std::unordered_map<std::string, primec::ir_lowerer::LayoutFieldBinding> knownFields = {
      {"recv", {"/pkg/Receiver", ""}},
  };

  primec::Expr receiverName;
  receiverName.kind = primec::Expr::Kind::Name;
  receiverName.name = "recv";

  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.isMethodCall = true;
  methodCall.name = "makeA";
  methodCall.args = {receiverName};
  CHECK(primec::ir_lowerer::inferStructReturnPathFromExpr(methodCall,
                                                          knownFields,
                                                          structNames,
                                                          resolveStructTypePath,
                                                          resolveStructLayoutExprPath,
                                                          defMap) == "/pkg/A");

  primec::Expr callA;
  callA.kind = primec::Expr::Kind::Call;
  callA.name = "makeA";
  primec::Expr callB = callA;
  callB.name = "makeB";

  primec::Expr cond;
  cond.kind = primec::Expr::Kind::BoolLiteral;
  cond.boolValue = true;

  primec::Expr ifExpr;
  ifExpr.kind = primec::Expr::Kind::Call;
  ifExpr.name = "if";
  ifExpr.args = {cond, callA, callA};
  CHECK(primec::ir_lowerer::inferStructReturnPathFromExpr(ifExpr,
                                                          knownFields,
                                                          structNames,
                                                          resolveStructTypePath,
                                                          resolveStructLayoutExprPath,
                                                          defMap) == "/pkg/A");

  ifExpr.args = {cond, callA, callB};
  CHECK(primec::ir_lowerer::inferStructReturnPathFromExpr(ifExpr,
                                                          knownFields,
                                                          structNames,
                                                          resolveStructTypePath,
                                                          resolveStructLayoutExprPath,
                                                          defMap).empty());
}

TEST_CASE("ir lowerer struct layout helpers parse and extract alignment transforms") {
  CHECK(primec::ir_lowerer::alignTo(7u, 4u) == 8u);
  CHECK(primec::ir_lowerer::alignTo(16u, 8u) == 16u);
  CHECK(primec::ir_lowerer::alignTo(13u, 0u) == 13u);

  int parsed = 0;
  CHECK(primec::ir_lowerer::parsePositiveInt("32i32", parsed));
  CHECK(parsed == 32);
  CHECK_FALSE(primec::ir_lowerer::parsePositiveInt("-1", parsed));
  CHECK_FALSE(primec::ir_lowerer::parsePositiveInt("0", parsed));

  std::vector<primec::Transform> transforms;
  primec::Transform alignKbytes;
  alignKbytes.name = "align_kbytes";
  alignKbytes.arguments = {"2i32"};
  transforms.push_back(alignKbytes);

  uint32_t alignment = 0;
  bool hasAlignment = false;
  std::string error;
  CHECK(primec::ir_lowerer::extractAlignment(
      transforms, "struct /pkg/S", alignment, hasAlignment, error));
  CHECK(error.empty());
  CHECK(hasAlignment);
  CHECK(alignment == 2048u);

  std::vector<primec::Transform> duplicateTransforms;
  primec::Transform alignBytes;
  alignBytes.name = "align_bytes";
  alignBytes.arguments = {"4"};
  duplicateTransforms.push_back(alignBytes);
  duplicateTransforms.push_back(alignKbytes);

  error.clear();
  CHECK_FALSE(primec::ir_lowerer::extractAlignment(
      duplicateTransforms, "field /pkg/S/value", alignment, hasAlignment, error));
  CHECK(error == "duplicate align_kbytes transform on field /pkg/S/value");

  std::vector<primec::Transform> invalidTransforms;
  primec::Transform invalidAlign;
  invalidAlign.name = "align_bytes";
  invalidAlign.arguments = {"0"};
  invalidTransforms.push_back(invalidAlign);

  error.clear();
  CHECK_FALSE(primec::ir_lowerer::extractAlignment(
      invalidTransforms, "field /pkg/S/value", alignment, hasAlignment, error));
  CHECK(error == "align_bytes requires a positive integer argument");
}

TEST_CASE("ir lowerer struct layout helpers classify binding type layout") {
  primec::ir_lowerer::BindingTypeLayout layout;
  std::string structTypeName;
  std::string error;

  primec::ir_lowerer::LayoutFieldBinding intAlias;
  intAlias.typeName = "int";
  CHECK(primec::ir_lowerer::classifyBindingTypeLayout(intAlias, layout, structTypeName, error));
  CHECK(layout.sizeBytes == 4u);
  CHECK(layout.alignmentBytes == 4u);
  CHECK(structTypeName.empty());
  CHECK(error.empty());

  primec::ir_lowerer::LayoutFieldBinding pointerType;
  pointerType.typeName = "Pointer";
  pointerType.typeTemplateArg = "i32";
  CHECK(primec::ir_lowerer::classifyBindingTypeLayout(pointerType, layout, structTypeName, error));
  CHECK(layout.sizeBytes == 8u);
  CHECK(layout.alignmentBytes == 8u);
  CHECK(structTypeName.empty());
  CHECK(error.empty());

  primec::ir_lowerer::LayoutFieldBinding uninitPrimitive;
  uninitPrimitive.typeName = "uninitialized";
  uninitPrimitive.typeTemplateArg = "i64";
  CHECK(primec::ir_lowerer::classifyBindingTypeLayout(uninitPrimitive, layout, structTypeName, error));
  CHECK(layout.sizeBytes == 8u);
  CHECK(layout.alignmentBytes == 8u);
  CHECK(structTypeName.empty());
  CHECK(error.empty());

  primec::ir_lowerer::LayoutFieldBinding uninitCollection;
  uninitCollection.typeName = "uninitialized";
  uninitCollection.typeTemplateArg = "array<f32>";
  CHECK(primec::ir_lowerer::classifyBindingTypeLayout(uninitCollection, layout, structTypeName, error));
  CHECK(layout.sizeBytes == 8u);
  CHECK(layout.alignmentBytes == 8u);
  CHECK(structTypeName.empty());
  CHECK(error.empty());

  primec::ir_lowerer::LayoutFieldBinding uninitStruct;
  uninitStruct.typeName = "uninitialized";
  uninitStruct.typeTemplateArg = "Thing";
  CHECK(primec::ir_lowerer::classifyBindingTypeLayout(uninitStruct, layout, structTypeName, error));
  CHECK(structTypeName == "Thing");
  CHECK(error.empty());

  primec::ir_lowerer::LayoutFieldBinding missingTemplate;
  missingTemplate.typeName = "uninitialized";
  CHECK_FALSE(primec::ir_lowerer::classifyBindingTypeLayout(missingTemplate, layout, structTypeName, error));
  CHECK(error == "uninitialized requires a template argument for layout");

  primec::ir_lowerer::LayoutFieldBinding structType;
  structType.typeName = "MyStruct";
  CHECK(primec::ir_lowerer::classifyBindingTypeLayout(structType, layout, structTypeName, error));
  CHECK(structTypeName == "MyStruct");
  CHECK(error.empty());
}

TEST_CASE("ir lowerer struct layout helpers append struct fields") {
  primec::IrStructLayout layout;
  layout.name = "/pkg/S";

  uint32_t offset = 0;
  uint32_t structAlign = 1;
  int resolveCalls = 0;
  std::string error;

  const auto resolveTypeLayout = [&](const primec::ir_lowerer::LayoutFieldBinding &binding,
                                     primec::ir_lowerer::BindingTypeLayout &layoutOut,
                                     std::string &errorOut) {
    (void)errorOut;
    ++resolveCalls;
    if (binding.typeName == "i64") {
      layoutOut = {8u, 8u};
      return true;
    }
    if (binding.typeName == "i32") {
      layoutOut = {4u, 4u};
      return true;
    }
    return false;
  };

  primec::Expr staticField;
  staticField.kind = primec::Expr::Kind::Call;
  staticField.isBinding = true;
  staticField.name = "global";
  primec::Transform staticTransform;
  staticTransform.name = "static";
  primec::Transform privateTransform;
  privateTransform.name = "private";
  staticField.transforms = {privateTransform, staticTransform};

  const primec::ir_lowerer::LayoutFieldBinding staticBinding = {"i64", ""};
  CHECK(primec::ir_lowerer::appendStructLayoutField(
      "/pkg/S", staticField, staticBinding, resolveTypeLayout, offset, structAlign, layout, error));
  CHECK(resolveCalls == 0);
  REQUIRE(layout.fields.size() == 1u);
  CHECK(layout.fields[0].name == "global");
  CHECK(layout.fields[0].isStatic);
  CHECK(layout.fields[0].sizeBytes == 0u);
  CHECK(layout.fields[0].visibility == primec::IrStructVisibility::Private);

  primec::Expr valueField;
  valueField.kind = primec::Expr::Kind::Call;
  valueField.isBinding = true;
  valueField.name = "value";
  primec::Transform alignEight;
  alignEight.name = "align_bytes";
  alignEight.arguments = {"8"};
  valueField.transforms = {alignEight};

  const primec::ir_lowerer::LayoutFieldBinding valueBinding = {"i32", ""};
  CHECK(primec::ir_lowerer::appendStructLayoutField(
      "/pkg/S", valueField, valueBinding, resolveTypeLayout, offset, structAlign, layout, error));
  CHECK(resolveCalls == 1);
  REQUIRE(layout.fields.size() == 2u);
  CHECK(layout.fields[1].name == "value");
  CHECK_FALSE(layout.fields[1].isStatic);
  CHECK(layout.fields[1].offsetBytes == 0u);
  CHECK(layout.fields[1].sizeBytes == 4u);
  CHECK(layout.fields[1].alignmentBytes == 8u);
  CHECK(layout.fields[1].paddingKind == primec::IrStructPaddingKind::None);
  CHECK(offset == 4u);
  CHECK(structAlign == 8u);

  primec::Expr tailField;
  tailField.kind = primec::Expr::Kind::Call;
  tailField.isBinding = true;
  tailField.name = "tail";

  const primec::ir_lowerer::LayoutFieldBinding tailBinding = {"i64", ""};
  CHECK(primec::ir_lowerer::appendStructLayoutField(
      "/pkg/S", tailField, tailBinding, resolveTypeLayout, offset, structAlign, layout, error));
  CHECK(resolveCalls == 2);
  REQUIRE(layout.fields.size() == 3u);
  CHECK(layout.fields[2].offsetBytes == 8u);
  CHECK(layout.fields[2].paddingKind == primec::IrStructPaddingKind::Align);
  CHECK(offset == 16u);
  CHECK(structAlign == 8u);

  primec::Expr badField;
  badField.kind = primec::Expr::Kind::Call;
  badField.isBinding = true;
  badField.name = "bad";
  primec::Transform alignTwo;
  alignTwo.name = "align_bytes";
  alignTwo.arguments = {"2"};
  badField.transforms = {alignTwo};

  error.clear();
  CHECK_FALSE(primec::ir_lowerer::appendStructLayoutField(
      "/pkg/S", badField, tailBinding, resolveTypeLayout, offset, structAlign, layout, error));
  CHECK(error == "alignment requirement on field /pkg/S/bad is smaller than required alignment of 8");
}

TEST_CASE("ir lowerer struct layout helpers resolve binding type layout") {
  primec::Definition nested;
  nested.fullPath = "/pkg/Nested";
  nested.namespacePrefix = "/pkg";

  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {nested.fullPath, &nested},
  };
  const auto resolveStructTypePath = [](const std::string &typeName, const std::string &namespacePrefix) {
    return namespacePrefix + "/" + typeName;
  };

  int computeCalls = 0;
  const auto computeStructLayout = [&](const primec::Definition &def, primec::IrStructLayout &layout, std::string &error) {
    ++computeCalls;
    if (def.fullPath == "/pkg/Fail") {
      error = "layout failure";
      return false;
    }
    layout.name = def.fullPath;
    layout.totalSizeBytes = 24u;
    layout.alignmentBytes = 8u;
    return true;
  };

  primec::ir_lowerer::BindingTypeLayout layout;
  std::string error;

  const primec::ir_lowerer::LayoutFieldBinding primitive = {"i32", ""};
  CHECK(primec::ir_lowerer::resolveBindingTypeLayout(primitive,
                                                     "/pkg",
                                                     resolveStructTypePath,
                                                     defMap,
                                                     computeStructLayout,
                                                     layout,
                                                     error));
  CHECK(layout.sizeBytes == 4u);
  CHECK(layout.alignmentBytes == 4u);
  CHECK(computeCalls == 0);
  CHECK(error.empty());

  const primec::ir_lowerer::LayoutFieldBinding structBinding = {"Nested", ""};
  CHECK(primec::ir_lowerer::resolveBindingTypeLayout(structBinding,
                                                     "/pkg",
                                                     resolveStructTypePath,
                                                     defMap,
                                                     computeStructLayout,
                                                     layout,
                                                     error));
  CHECK(layout.sizeBytes == 24u);
  CHECK(layout.alignmentBytes == 8u);
  CHECK(computeCalls == 1);
  CHECK(error.empty());

  const primec::ir_lowerer::LayoutFieldBinding missingStruct = {"Missing", ""};
  CHECK_FALSE(primec::ir_lowerer::resolveBindingTypeLayout(missingStruct,
                                                           "/pkg",
                                                           resolveStructTypePath,
                                                           defMap,
                                                           computeStructLayout,
                                                           layout,
                                                           error));
  CHECK(error == "unknown struct type for layout: Missing");

  primec::Definition failing;
  failing.fullPath = "/pkg/Fail";
  failing.namespacePrefix = "/pkg";
  const std::unordered_map<std::string, const primec::Definition *> failingMap = {
      {failing.fullPath, &failing},
  };
  const primec::ir_lowerer::LayoutFieldBinding failingStruct = {"Fail", ""};
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::resolveBindingTypeLayout(failingStruct,
                                                           "/pkg",
                                                           resolveStructTypePath,
                                                           failingMap,
                                                           computeStructLayout,
                                                           layout,
                                                           error));
  CHECK(error == "layout failure");
}

TEST_CASE("ir lowerer struct layout helpers compute with cache") {
  std::unordered_map<std::string, primec::IrStructLayout> layoutCache;
  std::unordered_set<std::string> layoutStack;
  std::string error;
  primec::IrStructLayout out;

  int computeCalls = 0;
  const auto computeLayout = [&](primec::IrStructLayout &layout, std::string &computeError) {
    (void)computeError;
    ++computeCalls;
    layout.name = "/pkg/S";
    layout.totalSizeBytes = 16u;
    layout.alignmentBytes = 8u;
    return true;
  };

  CHECK(primec::ir_lowerer::computeStructLayoutWithCache(
      "/pkg/S", layoutCache, layoutStack, computeLayout, out, error));
  CHECK(computeCalls == 1);
  CHECK(layoutCache.count("/pkg/S") == 1u);
  CHECK(layoutStack.empty());
  CHECK(out.totalSizeBytes == 16u);
  CHECK(out.alignmentBytes == 8u);

  int cacheMissCalls = 0;
  const auto shouldNotRun = [&](primec::IrStructLayout &, std::string &) {
    ++cacheMissCalls;
    return false;
  };
  CHECK(primec::ir_lowerer::computeStructLayoutWithCache(
      "/pkg/S", layoutCache, layoutStack, shouldNotRun, out, error));
  CHECK(cacheMissCalls == 0);

  layoutStack.insert("/pkg/Recursive");
  CHECK_FALSE(primec::ir_lowerer::computeStructLayoutWithCache(
      "/pkg/Recursive", layoutCache, layoutStack, computeLayout, out, error));
  CHECK(error == "recursive struct layout not supported: /pkg/Recursive");
  layoutStack.erase("/pkg/Recursive");

  const auto failingCompute = [&](primec::IrStructLayout &, std::string &computeError) {
    computeError = "layout failure";
    return false;
  };
  CHECK_FALSE(primec::ir_lowerer::computeStructLayoutWithCache(
      "/pkg/Fail", layoutCache, layoutStack, failingCompute, out, error));
  CHECK(error == "layout failure");
  CHECK(layoutStack.count("/pkg/Fail") == 0u);

  const auto retryCompute = [&](primec::IrStructLayout &layout, std::string &) {
    layout.name = "/pkg/Fail";
    layout.totalSizeBytes = 4u;
    layout.alignmentBytes = 4u;
    return true;
  };
  CHECK(primec::ir_lowerer::computeStructLayoutWithCache(
      "/pkg/Fail", layoutCache, layoutStack, retryCompute, out, error));
  CHECK(out.totalSizeBytes == 4u);
  CHECK(out.alignmentBytes == 4u);
}

TEST_CASE("ir lowerer struct layout helpers compute uncached layout") {
  primec::Definition def;
  def.fullPath = "/pkg/S";
  def.namespacePrefix = "/pkg";
  primec::Transform structAlign;
  structAlign.name = "align_bytes";
  structAlign.arguments = {"16"};
  def.transforms = {structAlign};

  primec::Expr firstField;
  firstField.kind = primec::Expr::Kind::Name;
  firstField.isBinding = true;
  firstField.name = "value";

  primec::Expr staticField = firstField;
  staticField.name = "cached";
  primec::Transform staticTransform;
  staticTransform.name = "static";
  staticField.transforms = {staticTransform};
  def.statements = {firstField, staticField};

  const std::vector<primec::ir_lowerer::LayoutFieldBinding> fieldBindings = {
      {"i32", ""},
      {"UnknownStaticType", ""},
  };

  int resolveCalls = 0;
  const auto resolveFieldTypeLayout = [&](const primec::ir_lowerer::LayoutFieldBinding &binding,
                                          primec::ir_lowerer::BindingTypeLayout &layout,
                                          std::string &resolveError) {
    ++resolveCalls;
    if (binding.typeName == "i32") {
      layout = {4u, 4u};
      return true;
    }
    resolveError = "unexpected type layout request";
    return false;
  };

  primec::IrStructLayout layout;
  std::string error;
  CHECK(primec::ir_lowerer::computeStructLayoutUncached(
      def, fieldBindings, resolveFieldTypeLayout, layout, error));
  CHECK(error.empty());
  CHECK(resolveCalls == 1);
  CHECK(layout.name == "/pkg/S");
  CHECK(layout.alignmentBytes == 16u);
  CHECK(layout.totalSizeBytes == 16u);
  REQUIRE(layout.fields.size() == 2u);
  CHECK(layout.fields[0].name == "value");
  CHECK(layout.fields[0].offsetBytes == 0u);
  CHECK(layout.fields[0].sizeBytes == 4u);
  CHECK(layout.fields[0].alignmentBytes == 4u);
  CHECK_FALSE(layout.fields[0].isStatic);
  CHECK(layout.fields[1].name == "cached");
  CHECK(layout.fields[1].offsetBytes == 0u);
  CHECK(layout.fields[1].sizeBytes == 0u);
  CHECK(layout.fields[1].alignmentBytes == 1u);
  CHECK(layout.fields[1].isStatic);
}

TEST_CASE("ir lowerer struct layout helpers compute uncached diagnostics") {
  primec::Definition mismatchDef;
  mismatchDef.fullPath = "/pkg/Mismatch";
  primec::Expr mismatchField;
  mismatchField.kind = primec::Expr::Kind::Name;
  mismatchField.isBinding = true;
  mismatchField.name = "field";
  mismatchDef.statements = {mismatchField};

  primec::IrStructLayout layout;
  std::string error;
  const auto resolveFieldTypeLayout = [](const primec::ir_lowerer::LayoutFieldBinding &,
                                         primec::ir_lowerer::BindingTypeLayout &,
                                         std::string &) { return true; };
  CHECK_FALSE(primec::ir_lowerer::computeStructLayoutUncached(
      mismatchDef, {}, resolveFieldTypeLayout, layout, error));
  CHECK(error == "internal error: mismatched struct field info for /pkg/Mismatch");

  primec::Definition alignDef;
  alignDef.fullPath = "/pkg/BadAlign";
  primec::Transform structAlign;
  structAlign.name = "align_bytes";
  structAlign.arguments = {"2"};
  alignDef.transforms = {structAlign};
  primec::Expr wideField = mismatchField;
  wideField.name = "wide";
  alignDef.statements = {wideField};

  const std::vector<primec::ir_lowerer::LayoutFieldBinding> wideBindings = {
      {"Wide", ""},
  };
  const auto resolveWideLayout = [](const primec::ir_lowerer::LayoutFieldBinding &,
                                    primec::ir_lowerer::BindingTypeLayout &resolvedLayout,
                                    std::string &resolveError) {
    (void)resolveError;
    resolvedLayout = {8u, 8u};
    return true;
  };
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::computeStructLayoutUncached(
      alignDef, wideBindings, resolveWideLayout, layout, error));
  CHECK(error == "alignment requirement on struct /pkg/BadAlign is smaller than required alignment of 8");
}

TEST_CASE("ir lowerer struct layout helpers compute from field info") {
  primec::Definition structDef;
  structDef.fullPath = "/pkg/S";
  structDef.namespacePrefix = "/pkg";
  primec::Expr fieldExpr;
  fieldExpr.kind = primec::Expr::Kind::Name;
  fieldExpr.isBinding = true;
  fieldExpr.name = "nested";
  structDef.statements = {fieldExpr};

  primec::Definition nestedDef;
  nestedDef.fullPath = "/pkg/Nested";

  const std::unordered_map<std::string, std::vector<primec::ir_lowerer::LayoutFieldBinding>> structFieldInfoByName = {
      {"/pkg/S", {{"Nested", ""}}},
  };
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/pkg/Nested", &nestedDef},
  };
  const auto resolveStructTypePath = [](const std::string &typeName, const std::string &namespacePrefix) {
    return namespacePrefix + "/" + typeName;
  };

  int nestedCalls = 0;
  const auto computeStructLayout = [&](const primec::Definition &def, primec::IrStructLayout &layout) {
    ++nestedCalls;
    CHECK(def.fullPath == "/pkg/Nested");
    layout.name = def.fullPath;
    layout.totalSizeBytes = 24u;
    layout.alignmentBytes = 8u;
    return true;
  };

  primec::IrStructLayout layout;
  std::string error;
  CHECK(primec::ir_lowerer::computeStructLayoutFromFieldInfo(
      structDef, structFieldInfoByName, resolveStructTypePath, defMap, computeStructLayout, layout, error));
  CHECK(error.empty());
  CHECK(nestedCalls == 1);
  CHECK(layout.name == "/pkg/S");
  CHECK(layout.totalSizeBytes == 24u);
  CHECK(layout.alignmentBytes == 8u);
  REQUIRE(layout.fields.size() == 1u);
  CHECK(layout.fields[0].name == "nested");
  CHECK(layout.fields[0].sizeBytes == 24u);
  CHECK(layout.fields[0].alignmentBytes == 8u);
}

TEST_CASE("ir lowerer struct layout helpers compute from field info diagnostics") {
  primec::Definition structDef;
  structDef.fullPath = "/pkg/S";
  structDef.namespacePrefix = "/pkg";
  primec::Expr fieldExpr;
  fieldExpr.kind = primec::Expr::Kind::Name;
  fieldExpr.isBinding = true;
  fieldExpr.name = "missing";
  structDef.statements = {fieldExpr};

  const auto resolveStructTypePath = [](const std::string &typeName, const std::string &namespacePrefix) {
    return namespacePrefix + "/" + typeName;
  };
  const auto computeStructLayout = [](const primec::Definition &, primec::IrStructLayout &) { return true; };

  primec::IrStructLayout layout;
  std::string error;
  const std::unordered_map<std::string, std::vector<primec::ir_lowerer::LayoutFieldBinding>> noFieldInfo;
  const std::unordered_map<std::string, const primec::Definition *> noDefs;
  CHECK_FALSE(primec::ir_lowerer::computeStructLayoutFromFieldInfo(
      structDef, noFieldInfo, resolveStructTypePath, noDefs, computeStructLayout, layout, error));
  CHECK(error == "internal error: missing struct field info for /pkg/S");

  const std::unordered_map<std::string, std::vector<primec::ir_lowerer::LayoutFieldBinding>> withFieldInfo = {
      {"/pkg/S", {{"Missing", ""}}},
  };
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::computeStructLayoutFromFieldInfo(
      structDef, withFieldInfo, resolveStructTypePath, noDefs, computeStructLayout, layout, error));
  CHECK(error == "unknown struct type for layout: Missing");
}

TEST_CASE("ir lowerer struct layout helpers append program layouts") {
  primec::Program program;

  primec::Definition nonStruct;
  nonStruct.fullPath = "/pkg/fn";
  program.definitions.push_back(nonStruct);

  primec::Definition structA;
  structA.fullPath = "/pkg/A";
  primec::Transform structTransformA;
  structTransformA.name = "struct";
  structA.transforms.push_back(structTransformA);
  program.definitions.push_back(structA);

  primec::Definition structB;
  structB.fullPath = "/pkg/B";
  primec::Transform structTransformB;
  structTransformB.name = "struct";
  structB.transforms.push_back(structTransformB);
  program.definitions.push_back(structB);

  std::vector<std::string> calls;
  const auto computeStructLayout = [&](const primec::Definition &def, primec::IrStructLayout &layout) {
    calls.push_back(def.fullPath);
    layout.name = def.fullPath;
    layout.totalSizeBytes = (def.fullPath == "/pkg/A") ? 4u : 8u;
    layout.alignmentBytes = 4u;
    return true;
  };

  std::vector<primec::IrStructLayout> layouts;
  std::string error;
  CHECK(primec::ir_lowerer::appendProgramStructLayouts(program, computeStructLayout, layouts, error));
  CHECK(error.empty());
  REQUIRE(calls.size() == 2u);
  CHECK(calls[0] == "/pkg/A");
  CHECK(calls[1] == "/pkg/B");
  REQUIRE(layouts.size() == 2u);
  CHECK(layouts[0].name == "/pkg/A");
  CHECK(layouts[1].name == "/pkg/B");
}

TEST_CASE("ir lowerer struct layout helpers append program layout diagnostics") {
  primec::Program program;
  primec::Definition structA;
  structA.fullPath = "/pkg/A";
  primec::Transform structTransformA;
  structTransformA.name = "struct";
  structA.transforms.push_back(structTransformA);
  program.definitions.push_back(structA);

  primec::Definition structB;
  structB.fullPath = "/pkg/B";
  primec::Transform structTransformB;
  structTransformB.name = "struct";
  structB.transforms.push_back(structTransformB);
  program.definitions.push_back(structB);

  std::vector<primec::IrStructLayout> layouts;
  std::string error;
  const auto explicitError = [&](const primec::Definition &def, primec::IrStructLayout &layout) {
    layout.name = def.fullPath;
    if (def.fullPath == "/pkg/B") {
      error = "layout failure: /pkg/B";
      return false;
    }
    return true;
  };
  CHECK_FALSE(primec::ir_lowerer::appendProgramStructLayouts(program, explicitError, layouts, error));
  CHECK(error == "layout failure: /pkg/B");
  REQUIRE(layouts.size() == 1u);
  CHECK(layouts[0].name == "/pkg/A");

  layouts.clear();
  error.clear();
  const auto defaultError = [&](const primec::Definition &, primec::IrStructLayout &) { return false; };
  CHECK_FALSE(primec::ir_lowerer::appendProgramStructLayouts(program, defaultError, layouts, error));
  CHECK(error == "failed to compute struct layout: /pkg/A");
  CHECK(layouts.empty());
}

TEST_CASE("ir lowerer struct layout helpers classify layout transforms") {
  CHECK(primec::ir_lowerer::isLayoutQualifierName("public"));
  CHECK(primec::ir_lowerer::isLayoutQualifierName("align_kbytes"));
  CHECK_FALSE(primec::ir_lowerer::isLayoutQualifierName("i32"));

  primec::Expr podField;
  primec::Transform podTransform;
  podTransform.name = "pod";
  primec::Transform privateTransform;
  privateTransform.name = "private";
  primec::Transform staticTransform;
  staticTransform.name = "static";
  podField.transforms = {podTransform, privateTransform, staticTransform};
  CHECK(primec::ir_lowerer::fieldCategory(podField) == primec::IrStructFieldCategory::Pod);
  CHECK(primec::ir_lowerer::fieldVisibility(podField) == primec::IrStructVisibility::Private);
  CHECK(primec::ir_lowerer::isStaticField(podField));

  primec::Expr handleField;
  primec::Transform handleTransform;
  handleTransform.name = "handle";
  primec::Transform gpuLaneTransform;
  gpuLaneTransform.name = "gpu_lane";
  handleField.transforms = {gpuLaneTransform, handleTransform};
  CHECK(primec::ir_lowerer::fieldCategory(handleField) == primec::IrStructFieldCategory::Handle);

  primec::Expr publicField;
  primec::Transform publicTransform;
  publicTransform.name = "public";
  publicField.transforms = {privateTransform, publicTransform};
  CHECK(primec::ir_lowerer::fieldVisibility(publicField) == primec::IrStructVisibility::Public);
  CHECK_FALSE(primec::ir_lowerer::isStaticField(publicField));
}

TEST_CASE("ir lowerer call helpers build this params and collect struct fields") {
  primec::Expr thisParam = primec::ir_lowerer::makeStructHelperThisParam("/pkg/MyStruct", true);
  CHECK(thisParam.kind == primec::Expr::Kind::Name);
  CHECK(thisParam.isBinding);
  CHECK(thisParam.name == "this");
  REQUIRE(thisParam.transforms.size() == 2);
  CHECK(thisParam.transforms[0].name == "Reference");
  REQUIRE(thisParam.transforms[0].templateArgs.size() == 1);
  CHECK(thisParam.transforms[0].templateArgs[0] == "/pkg/MyStruct");
  CHECK(thisParam.transforms[1].name == "mut");

  primec::Definition structDef;
  structDef.fullPath = "/pkg/MyStruct";

  primec::Expr instanceField;
  instanceField.kind = primec::Expr::Kind::Name;
  instanceField.isBinding = true;
  instanceField.name = "value";

  primec::Expr staticField = instanceField;
  staticField.name = "globalValue";
  primec::Transform staticTransform;
  staticTransform.name = "static";
  staticField.transforms.push_back(staticTransform);

  structDef.statements = {instanceField, staticField};
  std::vector<primec::Expr> collectedFields;
  std::string error;
  REQUIRE(primec::ir_lowerer::collectInstanceStructFieldParams(
      structDef, collectedFields, error));
  CHECK(error.empty());
  REQUIRE(collectedFields.size() == 1);
  CHECK(collectedFields[0].name == "value");
  CHECK_FALSE(primec::ir_lowerer::isStaticFieldBinding(collectedFields[0]));
  CHECK(primec::ir_lowerer::isStaticFieldBinding(staticField));

  primec::Expr invalidStatement;
  invalidStatement.kind = primec::Expr::Kind::Call;
  invalidStatement.name = "oops";
  invalidStatement.isBinding = false;
  structDef.statements.push_back(invalidStatement);
  CHECK_FALSE(primec::ir_lowerer::collectInstanceStructFieldParams(
      structDef, collectedFields, error));
  CHECK(error == "struct definitions may only contain field bindings: /pkg/MyStruct");
}

TEST_CASE("ir lowerer call helpers build inline call parameter lists") {
  const std::unordered_set<std::string> structNames = {"/pkg/MyStruct"};
  std::vector<primec::Expr> paramsOut;
  std::string error;

  primec::Definition freeDef;
  freeDef.fullPath = "/free";
  primec::Expr freeParam;
  freeParam.kind = primec::Expr::Kind::Name;
  freeParam.isBinding = true;
  freeParam.name = "x";
  freeDef.parameters.push_back(freeParam);
  REQUIRE(primec::ir_lowerer::buildInlineCallParameterList(
      freeDef, structNames, paramsOut, error));
  CHECK(error.empty());
  REQUIRE(paramsOut.size() == 1);
  CHECK(paramsOut[0].name == "x");

  primec::Definition helperDef;
  helperDef.isNested = true;
  helperDef.fullPath = "/pkg/MyStruct/doThing";
  helperDef.parameters.push_back(freeParam);
  primec::Transform mutTransform;
  mutTransform.name = "mut";
  helperDef.transforms.push_back(mutTransform);
  REQUIRE(primec::ir_lowerer::buildInlineCallParameterList(
      helperDef, structNames, paramsOut, error));
  REQUIRE(paramsOut.size() == 2);
  CHECK(paramsOut[0].name == "this");
  REQUIRE(paramsOut[0].transforms.size() == 2);
  CHECK(paramsOut[0].transforms[0].name == "Reference");
  CHECK(paramsOut[0].transforms[1].name == "mut");
  CHECK(paramsOut[1].name == "x");

  primec::Transform staticTransform;
  staticTransform.name = "static";
  helperDef.transforms.push_back(staticTransform);
  REQUIRE(primec::ir_lowerer::buildInlineCallParameterList(
      helperDef, structNames, paramsOut, error));
  REQUIRE(paramsOut.size() == 1);
  CHECK(paramsOut[0].name == "x");

  primec::Definition structDef;
  structDef.fullPath = "/pkg/MyStruct";
  primec::Transform structTransform;
  structTransform.name = "struct";
  structDef.transforms.push_back(structTransform);
  primec::Expr instanceField;
  instanceField.kind = primec::Expr::Kind::Name;
  instanceField.isBinding = true;
  instanceField.name = "value";
  primec::Expr staticField = instanceField;
  staticField.name = "cached";
  staticField.transforms.push_back(staticTransform);
  structDef.statements = {instanceField, staticField};
  REQUIRE(primec::ir_lowerer::buildInlineCallParameterList(
      structDef, structNames, paramsOut, error));
  REQUIRE(paramsOut.size() == 1);
  CHECK(paramsOut[0].name == "value");

  primec::Expr invalidStmt;
  invalidStmt.kind = primec::Expr::Kind::Call;
  invalidStmt.name = "oops";
  invalidStmt.isBinding = false;
  structDef.statements.push_back(invalidStmt);
  CHECK_FALSE(primec::ir_lowerer::buildInlineCallParameterList(
      structDef, structNames, paramsOut, error));
  CHECK(error == "struct definitions may only contain field bindings: /pkg/MyStruct");
}

TEST_CASE("ir lowerer call helpers build inline call ordered arguments") {
  const std::unordered_set<std::string> structNames = {"/pkg/MyStruct"};
  std::vector<primec::Expr> paramsOut;
  std::vector<const primec::Expr *> orderedArgs;
  std::string error;

  primec::Definition helperDef;
  helperDef.isNested = true;
  helperDef.fullPath = "/pkg/MyStruct/doThing";
  primec::Expr valueParam;
  valueParam.kind = primec::Expr::Kind::Name;
  valueParam.isBinding = true;
  valueParam.name = "value";
  helperDef.parameters.push_back(valueParam);

  primec::Expr receiverArg;
  receiverArg.kind = primec::Expr::Kind::Name;
  receiverArg.name = "obj";
  primec::Expr valueArg;
  valueArg.kind = primec::Expr::Kind::Literal;
  valueArg.literalValue = 42;
  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "doThing";
  callExpr.args = {receiverArg, valueArg};
  callExpr.argNames = {std::nullopt, std::nullopt};

  REQUIRE(primec::ir_lowerer::buildInlineCallOrderedArguments(
      callExpr, helperDef, structNames, paramsOut, orderedArgs, error));
  CHECK(error.empty());
  REQUIRE(paramsOut.size() == 2);
  CHECK(paramsOut[0].name == "this");
  CHECK(paramsOut[1].name == "value");
  REQUIRE(orderedArgs.size() == 2);
  REQUIRE(orderedArgs[0] != nullptr);
  REQUIRE(orderedArgs[1] != nullptr);
  CHECK(orderedArgs[0]->name == "obj");
  CHECK(orderedArgs[1]->literalValue == 42);

  primec::Expr badCall = callExpr;
  badCall.args = {valueArg};
  badCall.argNames = {std::nullopt};
  CHECK_FALSE(primec::ir_lowerer::buildInlineCallOrderedArguments(
      badCall, helperDef, structNames, paramsOut, orderedArgs, error));
  CHECK(error == "argument count mismatch");
}

TEST_CASE("ir lowerer call helpers emit resolved inline definition call") {
  using Result = primec::ir_lowerer::ResolvedInlineCallResult;

  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "work";

  primec::Definition callee;
  callee.fullPath = "/pkg/work";

  std::string error;
  int emitCalls = 0;
  CHECK(primec::ir_lowerer::emitResolvedInlineDefinitionCall(
            callExpr,
            &callee,
            [&](const primec::Expr &resolvedExpr, const primec::Definition &resolvedCallee) {
              ++emitCalls;
              CHECK(resolvedExpr.name == "work");
              CHECK(resolvedCallee.fullPath == "/pkg/work");
              return true;
            },
            error) == Result::Emitted);
  CHECK(error.empty());
  CHECK(emitCalls == 1);

  CHECK(primec::ir_lowerer::emitResolvedInlineDefinitionCall(
            callExpr,
            nullptr,
            [&](const primec::Expr &, const primec::Definition &) {
              CHECK(false);
              return false;
            },
            error) == Result::NoCallee);

  primec::Expr bodyArgExpr;
  bodyArgExpr.kind = primec::Expr::Kind::Literal;
  bodyArgExpr.intWidth = 32;
  bodyArgExpr.literalValue = 5;
  primec::Expr callWithBodyArgs = callExpr;
  callWithBodyArgs.hasBodyArguments = true;
  callWithBodyArgs.bodyArguments.push_back(bodyArgExpr);
  error.clear();
  CHECK(primec::ir_lowerer::emitResolvedInlineDefinitionCall(
            callWithBodyArgs,
            &callee,
            [&](const primec::Expr &, const primec::Definition &) {
              CHECK(false);
              return false;
            },
            error) == Result::Error);
  CHECK(error == "native backend does not support block arguments on calls");

  error.clear();
  CHECK(primec::ir_lowerer::emitResolvedInlineDefinitionCall(
            callExpr,
            &callee,
            [&](const primec::Expr &, const primec::Definition &) {
              error = "emit failed";
              return false;
            },
            error) == Result::Error);
  CHECK(error == "emit failed");
}

TEST_CASE("ir lowerer call helpers dispatch inline call with count fallbacks") {
  using Result = primec::ir_lowerer::InlineCallDispatchResult;

  primec::Definition callee;
  callee.fullPath = "/pkg/helper";

  primec::Expr countCall;
  countCall.kind = primec::Expr::Kind::Call;
  countCall.name = "count";
  primec::Expr countArg;
  countArg.kind = primec::Expr::Kind::Name;
  countArg.name = "items";
  countCall.args.push_back(countArg);

  std::string error;

  int firstResolveMethodCalls = 0;
  int firstResolveDefinitionCalls = 0;
  int firstEmitCalls = 0;
  CHECK(primec::ir_lowerer::tryEmitInlineCallWithCountFallbacks(
            countCall,
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [&](const primec::Expr &) -> const primec::Definition * {
              ++firstResolveMethodCalls;
              return &callee;
            },
            [&](const primec::Expr &) -> const primec::Definition * {
              ++firstResolveDefinitionCalls;
              return nullptr;
            },
            [&](const primec::Expr &, const primec::Definition &) {
              ++firstEmitCalls;
              return true;
            },
            error) == Result::Emitted);
  CHECK(firstResolveMethodCalls == 1);
  CHECK(firstResolveDefinitionCalls == 0);
  CHECK(firstEmitCalls == 1);

  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.name = "helper";
  methodCall.isMethodCall = true;
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitInlineCallWithCountFallbacks(
            methodCall,
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [&](const primec::Expr &) -> const primec::Definition * { return nullptr; },
            [&](const primec::Expr &) -> const primec::Definition * {
              CHECK(false);
              return nullptr;
            },
            [&](const primec::Expr &, const primec::Definition &) {
              CHECK(false);
              return false;
            },
            error) == Result::Error);

  primec::Expr directCall;
  directCall.kind = primec::Expr::Kind::Call;
  directCall.name = "helper";
  int directEmitCalls = 0;
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitInlineCallWithCountFallbacks(
            directCall,
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [&](const primec::Expr &) -> const primec::Definition * {
              CHECK(false);
              return nullptr;
            },
            [&](const primec::Expr &) -> const primec::Definition * { return &callee; },
            [&](const primec::Expr &, const primec::Definition &) {
              ++directEmitCalls;
              return true;
            },
            error) == Result::Emitted);
  CHECK(directEmitCalls == 1);

  int secondResolveMethodCalls = 0;
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitInlineCallWithCountFallbacks(
            countCall,
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [&](const primec::Expr &) -> const primec::Definition * {
              ++secondResolveMethodCalls;
              return secondResolveMethodCalls == 1 ? nullptr : &callee;
            },
            [&](const primec::Expr &) -> const primec::Definition * { return nullptr; },
            [&](const primec::Expr &, const primec::Definition &) { return true; },
            error) == Result::Emitted);
  CHECK(secondResolveMethodCalls == 2);

  error.clear();
  CHECK(primec::ir_lowerer::tryEmitInlineCallWithCountFallbacks(
            directCall,
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [&](const primec::Expr &) -> const primec::Definition * {
              CHECK(false);
              return nullptr;
            },
            [&](const primec::Expr &) -> const primec::Definition * { return &callee; },
            [&](const primec::Expr &, const primec::Definition &) {
              error = "emit failed";
              return false;
            },
            error) == Result::Error);
  CHECK(error == "emit failed");

  primec::Expr plainCall;
  plainCall.kind = primec::Expr::Kind::Call;
  plainCall.name = "plain";
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitInlineCallWithCountFallbacks(
            plainCall,
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [&](const primec::Expr &) -> const primec::Definition * {
              CHECK(false);
              return nullptr;
            },
            [&](const primec::Expr &) -> const primec::Definition * { return nullptr; },
            [&](const primec::Expr &, const primec::Definition &) {
              CHECK(false);
              return false;
            },
            error) == Result::NotHandled);
}

TEST_CASE("ir lowerer call helpers dispatch inline calls with locals") {
  using Result = primec::ir_lowerer::InlineCallDispatchResult;

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo marker;
  marker.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  marker.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  locals.emplace("ctx", marker);

  primec::Definition callee;
  callee.fullPath = "/pkg/helper";

  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.name = "helper";
  methodCall.isMethodCall = true;

  std::string error;
  int emitCalls = 0;
  CHECK(primec::ir_lowerer::tryEmitInlineCallDispatchWithLocals(
            methodCall,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &localMap) -> const primec::Definition * {
              CHECK(localMap.count("ctx") == 1);
              return &callee;
            },
            [&](const primec::Expr &) -> const primec::Definition * {
              CHECK(false);
              return nullptr;
            },
            [&](const primec::Expr &, const primec::Definition &resolvedCallee, const primec::ir_lowerer::LocalMap &localMap) {
              ++emitCalls;
              CHECK(localMap.count("ctx") == 1);
              CHECK(resolvedCallee.fullPath == "/pkg/helper");
              return true;
            },
            error) == Result::Emitted);
  CHECK(emitCalls == 1);

  error.clear();
  CHECK(primec::ir_lowerer::tryEmitInlineCallDispatchWithLocals(
            methodCall,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &) -> const primec::Definition * {
              CHECK(false);
              return nullptr;
            },
            [&](const primec::Expr &, const primec::Definition &, const primec::ir_lowerer::LocalMap &) {
              CHECK(false);
              return false;
            },
            error) == Result::Error);

  primec::Expr directCall;
  directCall.kind = primec::Expr::Kind::Call;
  directCall.name = "helper";
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitInlineCallDispatchWithLocals(
            directCall,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              CHECK(false);
              return nullptr;
            },
            [&](const primec::Expr &) -> const primec::Definition * { return &callee; },
            [&](const primec::Expr &, const primec::Definition &, const primec::ir_lowerer::LocalMap &) {
              return true;
            },
            error) == Result::Emitted);

  primec::Expr plainCall;
  plainCall.kind = primec::Expr::Kind::Call;
  plainCall.name = "plain";
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitInlineCallDispatchWithLocals(
            plainCall,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
              return nullptr;
            },
            [&](const primec::Expr &) -> const primec::Definition * { return nullptr; },
            [&](const primec::Expr &, const primec::Definition &, const primec::ir_lowerer::LocalMap &) {
              CHECK(false);
              return false;
            },
            error) == Result::NotHandled);
}

TEST_CASE("ir lowerer call helpers detect unsupported vector helper names") {
  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  std::string helperName;

  callExpr.name = "push";
  REQUIRE(primec::ir_lowerer::getUnsupportedVectorHelperName(callExpr, helperName));
  CHECK(helperName == "push");

  callExpr.name = "remove_swap";
  REQUIRE(primec::ir_lowerer::getUnsupportedVectorHelperName(callExpr, helperName));
  CHECK(helperName == "remove_swap");

  callExpr.name = "count";
  helperName.clear();
  CHECK_FALSE(primec::ir_lowerer::getUnsupportedVectorHelperName(callExpr, helperName));
  CHECK(helperName.empty());

  callExpr.name = "push";
  callExpr.isMethodCall = true;
  helperName.clear();
  CHECK_FALSE(primec::ir_lowerer::getUnsupportedVectorHelperName(callExpr, helperName));
  CHECK(helperName.empty());
}

TEST_CASE("ir lowerer call helpers emit unsupported native call diagnostics") {
  using Result = primec::ir_lowerer::UnsupportedNativeCallResult;

  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  std::string error;

  callExpr.name = "count";
  CHECK(primec::ir_lowerer::emitUnsupportedNativeCallDiagnostic(
            callExpr,
            [](const primec::Expr &, std::string &) { return false; },
            error) == Result::Error);
  CHECK(error == "count requires array, vector, map, or string target");

  callExpr.name = "capacity";
  error.clear();
  CHECK(primec::ir_lowerer::emitUnsupportedNativeCallDiagnostic(
            callExpr,
            [](const primec::Expr &, std::string &) { return false; },
            error) == Result::Error);
  CHECK(error == "capacity requires vector target");

  callExpr.name = "remove_at";
  error.clear();
  CHECK(primec::ir_lowerer::emitUnsupportedNativeCallDiagnostic(
            callExpr,
            [](const primec::Expr &, std::string &) { return false; },
            error) == Result::Error);
  CHECK(error == "native backend does not support vector helper: remove_at");

  callExpr.name = "print";
  error.clear();
  CHECK(primec::ir_lowerer::emitUnsupportedNativeCallDiagnostic(
            callExpr,
            [](const primec::Expr &, std::string &builtinName) {
              builtinName = "print";
              return true;
            },
            error) == Result::Error);
  CHECK(error == "print is only supported as a statement in the native backend");

  callExpr.name = "plain";
  error.clear();
  CHECK(primec::ir_lowerer::emitUnsupportedNativeCallDiagnostic(
            callExpr,
            [](const primec::Expr &, std::string &) { return false; },
            error) == Result::NotHandled);
}

TEST_CASE("ir lowerer call helpers dispatch native call tail orchestration") {
  using Result = primec::ir_lowerer::NativeCallTailDispatchResult;
  using LocalInfo = primec::ir_lowerer::LocalInfo;

  primec::ir_lowerer::LocalMap locals;
  LocalInfo arrayInfo;
  arrayInfo.kind = LocalInfo::Kind::Array;
  arrayInfo.index = 9;
  arrayInfo.valueKind = LocalInfo::ValueKind::Int32;
  locals.emplace("arr", arrayInfo);

  LocalInfo indexInfo;
  indexInfo.kind = LocalInfo::Kind::Value;
  indexInfo.index = 3;
  indexInfo.valueKind = LocalInfo::ValueKind::Int32;
  locals.emplace("idx", indexInfo);

  primec::Expr arrName;
  arrName.kind = primec::Expr::Kind::Name;
  arrName.name = "arr";

  primec::Expr idxName;
  idxName.kind = primec::Expr::Kind::Name;
  idxName.name = "idx";

  std::vector<primec::IrInstruction> instructions;
  auto emitInstruction = [&](primec::IrOpcode op, uint64_t imm) {
    instructions.push_back({op, imm});
  };
  auto instructionCount = [&]() { return instructions.size(); };
  auto patchInstructionImm = [&](size_t index, uint64_t imm) { instructions.at(index).imm = imm; };

  int nextLocal = 20;
  std::string error;

  primec::Expr mathCall;
  mathCall.kind = primec::Expr::Kind::Call;
  mathCall.name = "sin";
  CHECK(primec::ir_lowerer::tryEmitNativeCallTailDispatch(
            mathCall,
            locals,
            [](const primec::Expr &, std::string &mathName) {
              mathName = "sin";
              return true;
            },
            [](const std::string &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) {
              return false;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, std::string &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return LocalInfo::ValueKind::Int32;
            },
            [&]() { return nextLocal++; },
            []() {},
            []() {},
            []() {},
            instructionCount,
            emitInstruction,
            patchInstructionImm,
            error) == Result::Error);
  CHECK(error == "native backend does not support math builtin: sin");

  primec::Expr countCall;
  countCall.kind = primec::Expr::Kind::Call;
  countCall.name = "count";
  countCall.args = {arrName};
  instructions.clear();
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitNativeCallTailDispatch(
            countCall,
            locals,
            [](const primec::Expr &, std::string &) { return false; },
            [](const std::string &) { return true; },
            [](const primec::Expr &callExpr, const primec::ir_lowerer::LocalMap &) {
              return callExpr.name == "count";
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) {
              return false;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, std::string &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return LocalInfo::ValueKind::Int32;
            },
            [&]() { return nextLocal++; },
            []() {},
            []() {},
            []() {},
            instructionCount,
            emitInstruction,
            patchInstructionImm,
            error) == Result::Emitted);
  CHECK(error.empty());
  CHECK_FALSE(instructions.empty());

  primec::Expr printCall;
  printCall.kind = primec::Expr::Kind::Call;
  printCall.name = "print";
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitNativeCallTailDispatch(
            printCall,
            locals,
            [](const primec::Expr &, std::string &) { return false; },
            [](const std::string &) { return true; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) {
              return false;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, std::string &builtinName) {
              builtinName = "print";
              return true;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return LocalInfo::ValueKind::Int32;
            },
            [&]() { return nextLocal++; },
            []() {},
            []() {},
            []() {},
            instructionCount,
            emitInstruction,
            patchInstructionImm,
            error) == Result::Error);
  CHECK(error == "print is only supported as a statement in the native backend");

  primec::Expr badAccessCall;
  badAccessCall.kind = primec::Expr::Kind::Call;
  badAccessCall.name = "at";
  badAccessCall.args = {arrName};
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitNativeCallTailDispatch(
            badAccessCall,
            locals,
            [](const primec::Expr &, std::string &) { return false; },
            [](const std::string &) { return true; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) {
              return false;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, std::string &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return LocalInfo::ValueKind::Int32;
            },
            [&]() { return nextLocal++; },
            []() {},
            []() {},
            []() {},
            instructionCount,
            emitInstruction,
            patchInstructionImm,
            error) == Result::Error);
  CHECK(error == "at requires exactly two arguments");

  primec::Expr accessCall;
  accessCall.kind = primec::Expr::Kind::Call;
  accessCall.name = "at";
  accessCall.args = {arrName, idxName};
  instructions.clear();
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitNativeCallTailDispatch(
            accessCall,
            locals,
            [](const primec::Expr &, std::string &) { return false; },
            [](const std::string &) { return true; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) {
              return false;
            },
            [&](const primec::Expr &valueExpr, const primec::ir_lowerer::LocalMap &) {
              emitInstruction(primec::IrOpcode::LoadLocal, valueExpr.name == "arr" ? 9 : 3);
              return true;
            },
            [](const primec::Expr &, std::string &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return LocalInfo::ValueKind::Int32;
            },
            [&]() { return nextLocal++; },
            []() {},
            []() {},
            []() {},
            instructionCount,
            emitInstruction,
            patchInstructionImm,
            error) == Result::Emitted);
  CHECK(error.empty());
  CHECK_FALSE(instructions.empty());

  primec::Expr plainCall;
  plainCall.kind = primec::Expr::Kind::Call;
  plainCall.name = "plain";
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitNativeCallTailDispatch(
            plainCall,
            locals,
            [](const primec::Expr &, std::string &) { return false; },
            [](const std::string &) { return true; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) {
              return false;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, std::string &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return LocalInfo::ValueKind::Int32;
            },
            [&]() { return nextLocal++; },
            []() {},
            []() {},
            []() {},
            instructionCount,
            emitInstruction,
            patchInstructionImm,
            error) == Result::NotHandled);
}

TEST_CASE("ir lowerer call helpers resolve and validate map access targets") {
  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo mapInfo;
  mapInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Map;
  mapInfo.mapKeyKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  mapInfo.mapValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Float64;
  locals.emplace("items", mapInfo);

  primec::Expr mapName;
  mapName.kind = primec::Expr::Kind::Name;
  mapName.name = "items";
  auto resolved = primec::ir_lowerer::resolveMapAccessTargetInfo(mapName, locals);
  CHECK(resolved.isMapTarget);
  CHECK(resolved.mapKeyKind == primec::ir_lowerer::LocalInfo::ValueKind::Int64);
  CHECK(resolved.mapValueKind == primec::ir_lowerer::LocalInfo::ValueKind::Float64);

  std::string error;
  CHECK(primec::ir_lowerer::validateMapAccessTargetInfo(resolved, "at", error));
  CHECK(error.empty());

  primec::Expr mapCtor;
  mapCtor.kind = primec::Expr::Kind::Call;
  mapCtor.name = "map";
  mapCtor.templateArgs = {"bool", "i32"};
  resolved = primec::ir_lowerer::resolveMapAccessTargetInfo(mapCtor, locals);
  CHECK(resolved.isMapTarget);
  CHECK(resolved.mapKeyKind == primec::ir_lowerer::LocalInfo::ValueKind::Bool);
  CHECK(resolved.mapValueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);

  primec::Expr plain;
  plain.kind = primec::Expr::Kind::Name;
  plain.name = "other";
  resolved = primec::ir_lowerer::resolveMapAccessTargetInfo(plain, locals);
  CHECK_FALSE(resolved.isMapTarget);
  error = "stale";
  CHECK(primec::ir_lowerer::validateMapAccessTargetInfo(resolved, "get", error));
  CHECK(error == "stale");

  primec::ir_lowerer::MapAccessTargetInfo untyped;
  untyped.isMapTarget = true;
  untyped.mapKeyKind = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  untyped.mapValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::validateMapAccessTargetInfo(untyped, "at", error));
  CHECK(error == "native backend requires typed map bindings for at");

  primec::ir_lowerer::MapAccessTargetInfo stringValue;
  stringValue.isMapTarget = true;
  stringValue.mapKeyKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  stringValue.mapValueKind = primec::ir_lowerer::LocalInfo::ValueKind::String;
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::validateMapAccessTargetInfo(stringValue, "at", error));
  CHECK(error == "native backend only supports numeric/bool map values");
}

TEST_CASE("ir lowerer call helpers resolve and validate array vector access targets") {
  using Kind = primec::ir_lowerer::LocalInfo::ValueKind;
  using LocalInfo = primec::ir_lowerer::LocalInfo;

  primec::ir_lowerer::LocalMap locals;
  LocalInfo arrayInfo;
  arrayInfo.kind = LocalInfo::Kind::Array;
  arrayInfo.valueKind = Kind::Int32;
  locals.emplace("arr", arrayInfo);

  LocalInfo vectorInfo;
  vectorInfo.kind = LocalInfo::Kind::Vector;
  vectorInfo.valueKind = Kind::Float64;
  locals.emplace("vec", vectorInfo);

  LocalInfo refArrayInfo;
  refArrayInfo.kind = LocalInfo::Kind::Reference;
  refArrayInfo.referenceToArray = true;
  refArrayInfo.valueKind = Kind::Bool;
  locals.emplace("refArr", refArrayInfo);

  primec::Expr arrName;
  arrName.kind = primec::Expr::Kind::Name;
  arrName.name = "arr";
  auto resolved = primec::ir_lowerer::resolveArrayVectorAccessTargetInfo(arrName, locals);
  CHECK(resolved.isArrayOrVectorTarget);
  CHECK(resolved.elemKind == Kind::Int32);
  CHECK_FALSE(resolved.isVectorTarget);

  primec::Expr vecName;
  vecName.kind = primec::Expr::Kind::Name;
  vecName.name = "vec";
  resolved = primec::ir_lowerer::resolveArrayVectorAccessTargetInfo(vecName, locals);
  CHECK(resolved.isArrayOrVectorTarget);
  CHECK(resolved.elemKind == Kind::Float64);
  CHECK(resolved.isVectorTarget);

  primec::Expr refArrName;
  refArrName.kind = primec::Expr::Kind::Name;
  refArrName.name = "refArr";
  resolved = primec::ir_lowerer::resolveArrayVectorAccessTargetInfo(refArrName, locals);
  CHECK(resolved.isArrayOrVectorTarget);
  CHECK(resolved.elemKind == Kind::Bool);
  CHECK_FALSE(resolved.isVectorTarget);

  primec::Expr vectorCtor;
  vectorCtor.kind = primec::Expr::Kind::Call;
  vectorCtor.name = "vector";
  vectorCtor.templateArgs = {"u64"};
  resolved = primec::ir_lowerer::resolveArrayVectorAccessTargetInfo(vectorCtor, locals);
  CHECK(resolved.isArrayOrVectorTarget);
  CHECK(resolved.elemKind == Kind::UInt64);
  CHECK(resolved.isVectorTarget);

  std::string error;
  CHECK(primec::ir_lowerer::validateArrayVectorAccessTargetInfo(resolved, error));
  CHECK(error.empty());

  primec::Expr plain;
  plain.kind = primec::Expr::Kind::Name;
  plain.name = "other";
  resolved = primec::ir_lowerer::resolveArrayVectorAccessTargetInfo(plain, locals);
  CHECK_FALSE(resolved.isArrayOrVectorTarget);
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::validateArrayVectorAccessTargetInfo(resolved, error));
  CHECK(error == "native backend only supports at() on numeric/bool arrays or vectors");

  primec::ir_lowerer::ArrayVectorAccessTargetInfo stringElem;
  stringElem.isArrayOrVectorTarget = true;
  stringElem.elemKind = Kind::String;
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::validateArrayVectorAccessTargetInfo(stringElem, error));
  CHECK(error == "native backend only supports at() on numeric/bool arrays or vectors");
}

TEST_CASE("ir lowerer call helpers try emit map access lookup") {
  using Kind = primec::ir_lowerer::LocalInfo::ValueKind;
  using Result = primec::ir_lowerer::MapAccessLookupEmitResult;

  primec::Expr targetExpr;
  targetExpr.kind = primec::Expr::Kind::Name;
  targetExpr.name = "items";
  primec::Expr keyExpr;
  keyExpr.kind = primec::Expr::Kind::Name;
  keyExpr.name = "mapKey";

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo mapInfo;
  mapInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Map;
  mapInfo.mapKeyKind = Kind::Int32;
  mapInfo.mapValueKind = Kind::Float64;
  locals.emplace(targetExpr.name, mapInfo);

  std::vector<primec::Instruction> instructions;
  int nextLocal = 20;
  int emitExprCalls = 0;
  int inferCalls = 0;
  int keyNotFoundCalls = 0;
  std::string error;
  CHECK(primec::ir_lowerer::tryEmitMapAccessLookup(
            "at",
            targetExpr,
            keyExpr,
            locals,
            [&]() { return nextLocal++; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              ++emitExprCalls;
              return true;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              ++inferCalls;
              return Kind::Int32;
            },
            [&]() { ++keyNotFoundCalls; },
            [&]() { return instructions.size(); },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            [&](size_t instructionIndex, uint64_t imm) { instructions[instructionIndex].imm = imm; },
            error) == Result::Emitted);
  CHECK(error.empty());
  CHECK(emitExprCalls == 2);
  CHECK(inferCalls == 1);
  CHECK(keyNotFoundCalls == 1);
  CHECK_FALSE(instructions.empty());
  CHECK(instructions.front().op == primec::IrOpcode::StoreLocal);

  instructions.clear();
  emitExprCalls = 0;
  inferCalls = 0;
  keyNotFoundCalls = 0;
  error = "stale";
  primec::Expr nonMapTarget = targetExpr;
  nonMapTarget.name = "plain";
  CHECK(primec::ir_lowerer::tryEmitMapAccessLookup(
            "at",
            nonMapTarget,
            keyExpr,
            locals,
            [&]() { return nextLocal++; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              ++emitExprCalls;
              return true;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              ++inferCalls;
              return Kind::Int32;
            },
            [&]() { ++keyNotFoundCalls; },
            [&]() { return instructions.size(); },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            [&](size_t instructionIndex, uint64_t imm) { instructions[instructionIndex].imm = imm; },
            error) == Result::NotHandled);
  CHECK(error == "stale");
  CHECK(emitExprCalls == 0);
  CHECK(inferCalls == 0);
  CHECK(keyNotFoundCalls == 0);
  CHECK(instructions.empty());

  instructions.clear();
  emitExprCalls = 0;
  inferCalls = 0;
  keyNotFoundCalls = 0;
  error.clear();
  primec::ir_lowerer::LocalMap untypedLocals;
  primec::ir_lowerer::LocalInfo untypedMapInfo;
  untypedMapInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Map;
  untypedMapInfo.mapKeyKind = Kind::Unknown;
  untypedMapInfo.mapValueKind = Kind::Int32;
  untypedLocals.emplace(targetExpr.name, untypedMapInfo);
  CHECK(primec::ir_lowerer::tryEmitMapAccessLookup(
            "at",
            targetExpr,
            keyExpr,
            untypedLocals,
            [&]() { return nextLocal++; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              ++emitExprCalls;
              return true;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              ++inferCalls;
              return Kind::Int32;
            },
            [&]() { ++keyNotFoundCalls; },
            [&]() { return instructions.size(); },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            [&](size_t instructionIndex, uint64_t imm) { instructions[instructionIndex].imm = imm; },
            error) == Result::Error);
  CHECK(error == "native backend requires typed map bindings for at");
  CHECK(emitExprCalls == 0);
  CHECK(inferCalls == 0);
  CHECK(keyNotFoundCalls == 0);
  CHECK(instructions.empty());
}

TEST_CASE("ir lowerer call helpers resolve validated access index kind") {
  using Kind = primec::ir_lowerer::LocalInfo::ValueKind;

  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Name;
  indexExpr.name = "idx";
  primec::ir_lowerer::LocalMap locals;
  std::string error;
  Kind indexKind = Kind::Unknown;

  int inferCalls = 0;
  CHECK(primec::ir_lowerer::resolveValidatedAccessIndexKind(
      indexExpr,
      locals,
      "at",
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        ++inferCalls;
        return Kind::Bool;
      },
      indexKind,
      error));
  CHECK(inferCalls == 1);
  CHECK(indexKind == Kind::Int32);
  CHECK(error.empty());

  inferCalls = 0;
  error.clear();
  CHECK(primec::ir_lowerer::resolveValidatedAccessIndexKind(
      indexExpr,
      locals,
      "index",
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        ++inferCalls;
        return Kind::UInt64;
      },
      indexKind,
      error));
  CHECK(inferCalls == 1);
  CHECK(indexKind == Kind::UInt64);
  CHECK(error.empty());

  inferCalls = 0;
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::resolveValidatedAccessIndexKind(
      indexExpr,
      locals,
      "at",
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        ++inferCalls;
        return Kind::Float32;
      },
      indexKind,
      error));
  CHECK(inferCalls == 1);
  CHECK(error == "native backend requires integer indices for at");
}

TEST_CASE("ir lowerer call helpers emit string table access load") {
  using Kind = primec::ir_lowerer::LocalInfo::ValueKind;
  using Result = primec::ir_lowerer::StringTableAccessEmitResult;

  primec::Expr targetExpr;
  targetExpr.kind = primec::Expr::Kind::Name;
  targetExpr.name = "text";

  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Name;
  indexExpr.name = "idx";

  primec::ir_lowerer::LocalMap locals;
  std::vector<primec::Instruction> instructions;
  std::string error = "stale";

  int allocCalls = 0;
  int inferCalls = 0;
  int emitExprCalls = 0;
  int stringIndexOutOfBoundsCalls = 0;

  CHECK(primec::ir_lowerer::tryEmitStringTableAccessLoad(
            "at",
            targetExpr,
            indexExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              ++inferCalls;
              return Kind::Int32;
            },
            [&]() {
              ++allocCalls;
              return 9;
            },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              ++emitExprCalls;
              return true;
            },
            [&]() { ++stringIndexOutOfBoundsCalls; },
            [&]() { return instructions.size(); },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            [&](size_t instructionIndex, uint64_t imm) { instructions[instructionIndex].imm = imm; },
            error) == Result::NotHandled);
  CHECK(error == "stale");
  CHECK(allocCalls == 0);
  CHECK(inferCalls == 0);
  CHECK(emitExprCalls == 0);
  CHECK(stringIndexOutOfBoundsCalls == 0);
  CHECK(instructions.empty());

  error.clear();
  CHECK(primec::ir_lowerer::tryEmitStringTableAccessLoad(
            "at",
            targetExpr,
            indexExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &stringIndex, size_t &length) {
              stringIndex = 7;
              length = 5;
              return true;
            },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              ++inferCalls;
              return Kind::Float32;
            },
            [&]() {
              ++allocCalls;
              return 10;
            },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              ++emitExprCalls;
              return true;
            },
            [&]() { ++stringIndexOutOfBoundsCalls; },
            [&]() { return instructions.size(); },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            [&](size_t instructionIndex, uint64_t imm) { instructions[instructionIndex].imm = imm; },
            error) == Result::Error);
  CHECK(error == "native backend requires integer indices for at");
  CHECK(allocCalls == 0);
  CHECK(inferCalls == 1);
  CHECK(emitExprCalls == 0);
  CHECK(stringIndexOutOfBoundsCalls == 0);
  CHECK(instructions.empty());

  error.clear();
  CHECK(primec::ir_lowerer::tryEmitStringTableAccessLoad(
            "at",
            targetExpr,
            indexExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &stringIndex, size_t &length) {
              stringIndex = 8;
              length = static_cast<size_t>(std::numeric_limits<int32_t>::max()) + 1;
              return true;
            },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              ++inferCalls;
              return Kind::Int32;
            },
            [&]() {
              ++allocCalls;
              return 11;
            },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              ++emitExprCalls;
              return true;
            },
            [&]() { ++stringIndexOutOfBoundsCalls; },
            [&]() { return instructions.size(); },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            [&](size_t instructionIndex, uint64_t imm) { instructions[instructionIndex].imm = imm; },
            error) == Result::Error);
  CHECK(error == "native backend string too large for indexing");
  CHECK(allocCalls == 0);
  CHECK(inferCalls == 2);
  CHECK(emitExprCalls == 0);
  CHECK(stringIndexOutOfBoundsCalls == 0);
  CHECK(instructions.empty());

  error.clear();
  CHECK(primec::ir_lowerer::tryEmitStringTableAccessLoad(
            "at",
            targetExpr,
            indexExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &stringIndex, size_t &length) {
              stringIndex = 9;
              length = 4;
              return true;
            },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              ++inferCalls;
              return Kind::Int32;
            },
            [&]() {
              ++allocCalls;
              return 12;
            },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              ++emitExprCalls;
              return false;
            },
            [&]() { ++stringIndexOutOfBoundsCalls; },
            [&]() { return instructions.size(); },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            [&](size_t instructionIndex, uint64_t imm) { instructions[instructionIndex].imm = imm; },
            error) == Result::Error);
  CHECK(error.empty());
  CHECK(allocCalls == 1);
  CHECK(inferCalls == 3);
  CHECK(emitExprCalls == 1);
  CHECK(stringIndexOutOfBoundsCalls == 0);
  CHECK(instructions.empty());

  instructions.clear();
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitStringTableAccessLoad(
            "at",
            targetExpr,
            indexExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &stringIndex, size_t &length) {
              stringIndex = 12;
              length = 5;
              return true;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return Kind::Int32; },
            []() { return 21; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              instructions.push_back({primec::IrOpcode::PushI32, 3});
              return true;
            },
            [&]() { ++stringIndexOutOfBoundsCalls; },
            [&]() { return instructions.size(); },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            [&](size_t instructionIndex, uint64_t imm) { instructions[instructionIndex].imm = imm; },
            error) == Result::Emitted);
  CHECK(stringIndexOutOfBoundsCalls == 2);
  CHECK(error.empty());
  REQUIRE(instructions.size() == 12);
  CHECK(instructions[0].op == primec::IrOpcode::PushI32);
  CHECK(instructions[0].imm == 3);
  CHECK(instructions[1].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[1].imm == 21);
  CHECK(instructions[2].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[2].imm == 21);
  CHECK(instructions[11].op == primec::IrOpcode::LoadStringByte);
  CHECK(instructions[11].imm == 12);
}

TEST_CASE("ir lowerer call helpers emit array vector indexed access") {
  using Kind = primec::ir_lowerer::LocalInfo::ValueKind;

  primec::Expr targetExpr;
  targetExpr.kind = primec::Expr::Kind::Name;
  targetExpr.name = "vec";

  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Name;
  indexExpr.name = "idx";

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo vectorLocalInfo;
  vectorLocalInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Vector;
  vectorLocalInfo.valueKind = Kind::Int32;
  locals.emplace("vec", vectorLocalInfo);

  std::vector<primec::Instruction> instructions;
  std::string error;

  int inferCalls = 0;
  int allocCalls = 0;
  int emitExprCalls = 0;
  int arrayIndexOutOfBoundsCalls = 0;

  error = "stale";
  CHECK_FALSE(primec::ir_lowerer::emitArrayVectorIndexedAccess(
      "at",
      targetExpr,
      indexExpr,
      primec::ir_lowerer::LocalMap{},
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        ++inferCalls;
        return Kind::Int32;
      },
      [&]() {
        ++allocCalls;
        return 0;
      },
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        ++emitExprCalls;
        return true;
      },
      [&]() { ++arrayIndexOutOfBoundsCalls; },
      [&]() { return instructions.size(); },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      [&](size_t instructionIndex, uint64_t imm) { instructions[instructionIndex].imm = imm; },
      error));
  CHECK(error == "native backend only supports at() on numeric/bool arrays or vectors");
  CHECK(inferCalls == 0);
  CHECK(allocCalls == 0);
  CHECK(emitExprCalls == 0);
  CHECK(arrayIndexOutOfBoundsCalls == 0);
  CHECK(instructions.empty());

  instructions.clear();
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::emitArrayVectorIndexedAccess(
      "at",
      targetExpr,
      indexExpr,
      locals,
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        ++inferCalls;
        return Kind::Float32;
      },
      [&]() {
        ++allocCalls;
        return 1;
      },
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        ++emitExprCalls;
        return true;
      },
      [&]() { ++arrayIndexOutOfBoundsCalls; },
      [&]() { return instructions.size(); },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      [&](size_t instructionIndex, uint64_t imm) { instructions[instructionIndex].imm = imm; },
      error));
  CHECK(error == "native backend requires integer indices for at");
  CHECK(inferCalls == 1);
  CHECK(allocCalls == 0);
  CHECK(emitExprCalls == 0);
  CHECK(arrayIndexOutOfBoundsCalls == 0);
  CHECK(instructions.empty());

  instructions.clear();
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::emitArrayVectorIndexedAccess(
      "at",
      targetExpr,
      indexExpr,
      locals,
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        ++inferCalls;
        return Kind::Int32;
      },
      [&]() { return 40 + allocCalls++; },
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        ++emitExprCalls;
        return false;
      },
      [&]() { ++arrayIndexOutOfBoundsCalls; },
      [&]() { return instructions.size(); },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      [&](size_t instructionIndex, uint64_t imm) { instructions[instructionIndex].imm = imm; },
      error));
  CHECK(error.empty());
  CHECK(inferCalls == 2);
  CHECK(allocCalls == 1);
  CHECK(emitExprCalls == 1);
  CHECK(arrayIndexOutOfBoundsCalls == 0);
  CHECK(instructions.empty());

  instructions.clear();
  error.clear();
  inferCalls = 0;
  allocCalls = 0;
  emitExprCalls = 0;
  arrayIndexOutOfBoundsCalls = 0;
  CHECK(primec::ir_lowerer::emitArrayVectorIndexedAccess(
      "at",
      targetExpr,
      indexExpr,
      locals,
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        ++inferCalls;
        return Kind::Int32;
      },
      [&]() { return 50 + allocCalls++; },
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        ++emitExprCalls;
        instructions.push_back({primec::IrOpcode::PushI32, emitExprCalls == 1 ? 101u : 3u});
        return true;
      },
      [&]() { ++arrayIndexOutOfBoundsCalls; },
      [&]() { return instructions.size(); },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      [&](size_t instructionIndex, uint64_t imm) { instructions[instructionIndex].imm = imm; },
      error));
  CHECK(error.empty());
  CHECK(inferCalls == 1);
  CHECK(allocCalls == 3);
  CHECK(emitExprCalls == 2);
  CHECK(arrayIndexOutOfBoundsCalls == 2);
  REQUIRE(instructions.size() == 23);
  CHECK(instructions[0].op == primec::IrOpcode::PushI32);
  CHECK(instructions[0].imm == 101);
  CHECK(instructions[1].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[1].imm == 50);
  CHECK(instructions[2].op == primec::IrOpcode::PushI32);
  CHECK(instructions[2].imm == 3);
  CHECK(instructions[3].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[3].imm == 51);
  CHECK(instructions[17].op == primec::ir_lowerer::pushOneForIndex(Kind::Int32));
  CHECK(instructions[17].imm == 2);
  CHECK(instructions.back().op == primec::IrOpcode::LoadIndirect);
}

TEST_CASE("ir lowerer call helpers emit builtin array access") {
  using Kind = primec::ir_lowerer::LocalInfo::ValueKind;

  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Name;
  indexExpr.name = "idx";

  primec::ir_lowerer::LocalMap locals;
  std::vector<primec::Instruction> instructions;
  std::string error = "stale";

  primec::Expr stringLiteralTarget;
  stringLiteralTarget.kind = primec::Expr::Kind::StringLiteral;
  stringLiteralTarget.stringValue = "abc";

  CHECK_FALSE(primec::ir_lowerer::emitBuiltinArrayAccess(
      "at",
      stringLiteralTarget,
      indexExpr,
      locals,
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return Kind::Int32; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      []() { return 1; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
      []() {},
      []() {},
      []() {},
      [&]() { return instructions.size(); },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      [&](size_t instructionIndex, uint64_t imm) { instructions[instructionIndex].imm = imm; },
      error));
  CHECK(error == "stale");
  CHECK(instructions.empty());

  instructions.clear();
  error.clear();
  int emitExprCalls = 0;
  CHECK(primec::ir_lowerer::emitBuiltinArrayAccess(
      "at",
      stringLiteralTarget,
      indexExpr,
      locals,
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &stringIndex, size_t &length) {
        stringIndex = 4;
        length = 3;
        return true;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return Kind::Int32; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      []() { return 7; },
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        ++emitExprCalls;
        instructions.push_back({primec::IrOpcode::PushI32, 1});
        return true;
      },
      []() {},
      []() {},
      []() {},
      [&]() { return instructions.size(); },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      [&](size_t instructionIndex, uint64_t imm) { instructions[instructionIndex].imm = imm; },
      error));
  CHECK(emitExprCalls == 1);
  CHECK(error.empty());
  REQUIRE(instructions.size() == 12);
  CHECK(instructions[0].op == primec::IrOpcode::PushI32);
  CHECK(instructions[1].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions.back().op == primec::IrOpcode::LoadStringByte);
  CHECK(instructions.back().imm == 4);

  instructions.clear();
  error.clear();
  primec::Expr mapTarget;
  mapTarget.kind = primec::Expr::Kind::Name;
  mapTarget.name = "m";
  primec::ir_lowerer::LocalInfo mapInfo;
  mapInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Map;
  mapInfo.mapKeyKind = Kind::Unknown;
  mapInfo.mapValueKind = Kind::Int32;
  locals.clear();
  locals.emplace("m", mapInfo);
  CHECK_FALSE(primec::ir_lowerer::emitBuiltinArrayAccess(
      "at",
      mapTarget,
      indexExpr,
      locals,
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return Kind::Int32; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      []() { return 10; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
      []() {},
      []() {},
      []() {},
      [&]() { return instructions.size(); },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      [&](size_t instructionIndex, uint64_t imm) { instructions[instructionIndex].imm = imm; },
      error));
  CHECK(error == "native backend requires typed map bindings for at");
  CHECK(instructions.empty());

  instructions.clear();
  error.clear();
  primec::Expr vectorTarget;
  vectorTarget.kind = primec::Expr::Kind::Name;
  vectorTarget.name = "v";
  primec::ir_lowerer::LocalInfo vectorInfo;
  vectorInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Vector;
  vectorInfo.valueKind = Kind::Int32;
  locals.clear();
  locals.emplace("v", vectorInfo);
  int arrayIndexOutOfBoundsCalls = 0;
  int nextLocal = 20;
  CHECK(primec::ir_lowerer::emitBuiltinArrayAccess(
      "at",
      vectorTarget,
      indexExpr,
      locals,
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return Kind::Int32; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [&]() { return nextLocal++; },
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        instructions.push_back({primec::IrOpcode::PushI32, 2});
        return true;
      },
      []() {},
      []() {},
      [&]() { ++arrayIndexOutOfBoundsCalls; },
      [&]() { return instructions.size(); },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      [&](size_t instructionIndex, uint64_t imm) { instructions[instructionIndex].imm = imm; },
      error));
  CHECK(error.empty());
  CHECK(arrayIndexOutOfBoundsCalls == 2);
  REQUIRE(instructions.size() >= 6);
  CHECK(instructions.back().op == primec::IrOpcode::LoadIndirect);
}

TEST_CASE("ir lowerer call helpers validate non literal string access target") {
  using Result = primec::ir_lowerer::NonLiteralStringAccessTargetResult;
  using Kind = primec::ir_lowerer::LocalInfo::ValueKind;

  primec::ir_lowerer::LocalMap locals;
  std::string error = "stale";

  primec::Expr stringLiteralTarget;
  stringLiteralTarget.kind = primec::Expr::Kind::StringLiteral;
  stringLiteralTarget.stringValue = "abc";
  CHECK(primec::ir_lowerer::validateNonLiteralStringAccessTarget(
            stringLiteralTarget,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            error) == Result::Stop);
  CHECK(error == "stale");

  primec::ir_lowerer::LocalInfo stringLocalInfo;
  stringLocalInfo.valueKind = Kind::String;
  locals.emplace("s", stringLocalInfo);
  primec::Expr stringNameTarget;
  stringNameTarget.kind = primec::Expr::Kind::Name;
  stringNameTarget.name = "s";
  error.clear();
  CHECK(primec::ir_lowerer::validateNonLiteralStringAccessTarget(
            stringNameTarget,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            error) == Result::Error);
  CHECK(error == "native backend only supports indexing into string literals or string bindings");

  primec::Expr entryArgsTarget;
  entryArgsTarget.kind = primec::Expr::Kind::Name;
  entryArgsTarget.name = "argv";
  error.clear();
  CHECK(primec::ir_lowerer::validateNonLiteralStringAccessTarget(
            entryArgsTarget,
            locals,
            [](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) { return expr.name == "argv"; },
            error) == Result::Error);
  CHECK(error == "native backend only supports entry argument indexing in print calls or string bindings");

  primec::Expr otherTarget;
  otherTarget.kind = primec::Expr::Kind::Name;
  otherTarget.name = "arr";
  error = "unchanged";
  CHECK(primec::ir_lowerer::validateNonLiteralStringAccessTarget(
            otherTarget,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            error) == Result::Continue);
  CHECK(error == "unchanged");
}

TEST_CASE("ir lowerer call helpers map key compare opcode selection") {
  using Kind = primec::ir_lowerer::LocalInfo::ValueKind;
  using primec::IrOpcode;

  CHECK(primec::ir_lowerer::mapKeyCompareOpcode(Kind::Int32) == IrOpcode::CmpEqI32);
  CHECK(primec::ir_lowerer::mapKeyCompareOpcode(Kind::Bool) == IrOpcode::CmpEqI32);
  CHECK(primec::ir_lowerer::mapKeyCompareOpcode(Kind::Int64) == IrOpcode::CmpEqI64);
  CHECK(primec::ir_lowerer::mapKeyCompareOpcode(Kind::UInt64) == IrOpcode::CmpEqI64);
  CHECK(primec::ir_lowerer::mapKeyCompareOpcode(Kind::Float32) == IrOpcode::CmpEqF32);
  CHECK(primec::ir_lowerer::mapKeyCompareOpcode(Kind::Float64) == IrOpcode::CmpEqF64);
}

TEST_CASE("ir lowerer call helpers resolve map lookup string keys") {
  using Kind = primec::ir_lowerer::LocalInfo::ValueKind;
  using Result = primec::ir_lowerer::MapLookupStringKeyResult;

  primec::Expr keyExpr;
  keyExpr.kind = primec::Expr::Kind::Name;
  keyExpr.name = "k";
  primec::ir_lowerer::LocalMap locals;
  std::string error;
  int32_t stringIndex = -1;

  CHECK(primec::ir_lowerer::tryResolveMapLookupStringKey(
            Kind::Int32,
            keyExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return true; },
            stringIndex,
            error) == Result::NotHandled);

  CHECK(primec::ir_lowerer::tryResolveMapLookupStringKey(
            Kind::String,
            keyExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return false; },
            stringIndex,
            error) == Result::Error);
  CHECK(error == "native backend requires map lookup key to be string literal or binding backed by literals");

  error.clear();
  CHECK(primec::ir_lowerer::tryResolveMapLookupStringKey(
            Kind::String,
            keyExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &indexOut, size_t &lengthOut) {
              indexOut = 17;
              lengthOut = 3;
              return true;
            },
            stringIndex,
            error) == Result::Resolved);
  CHECK(stringIndex == 17);
  CHECK(error.empty());
}

TEST_CASE("ir lowerer call helpers emit map lookup string key locals") {
  using Kind = primec::ir_lowerer::LocalInfo::ValueKind;
  using Result = primec::ir_lowerer::MapLookupKeyLocalEmitResult;

  primec::Expr keyExpr;
  keyExpr.kind = primec::Expr::Kind::Name;
  keyExpr.name = "k";
  primec::ir_lowerer::LocalMap locals;
  std::string error;
  int32_t pushed = -1;
  int32_t stored = -1;

  CHECK(primec::ir_lowerer::tryEmitMapLookupStringKeyLocal(
            Kind::Int64,
            keyExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return true; },
            [&](int32_t imm) { pushed = imm; },
            [&](int32_t local) { stored = local; },
            4,
            error) == Result::NotHandled);

  CHECK(primec::ir_lowerer::tryEmitMapLookupStringKeyLocal(
            Kind::String,
            keyExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return false; },
            [&](int32_t imm) { pushed = imm; },
            [&](int32_t local) { stored = local; },
            4,
            error) == Result::Error);
  CHECK(error == "native backend requires map lookup key to be string literal or binding backed by literals");

  error.clear();
  CHECK(primec::ir_lowerer::tryEmitMapLookupStringKeyLocal(
            Kind::String,
            keyExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &indexOut, size_t &lengthOut) {
              indexOut = 23;
              lengthOut = 8;
              return true;
            },
            [&](int32_t imm) { pushed = imm; },
            [&](int32_t local) { stored = local; },
            9,
            error) == Result::Emitted);
  CHECK(pushed == 23);
  CHECK(stored == 9);
}

TEST_CASE("ir lowerer call helpers emit map lookup non-string key locals") {
  using Kind = primec::ir_lowerer::LocalInfo::ValueKind;

  primec::Expr keyExpr;
  keyExpr.kind = primec::Expr::Kind::Name;
  keyExpr.name = "k";
  primec::ir_lowerer::LocalMap locals;
  std::string error;
  int32_t stored = -1;

  CHECK_FALSE(primec::ir_lowerer::emitMapLookupNonStringKeyLocal(
      Kind::String,
      keyExpr,
      locals,
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return Kind::Unknown; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
      [&](int32_t local) { stored = local; },
      3,
      error));
  CHECK(error == "native backend requires map lookup key to be string literal or binding backed by literals");

  error.clear();
  CHECK_FALSE(primec::ir_lowerer::emitMapLookupNonStringKeyLocal(
      Kind::Int32,
      keyExpr,
      locals,
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return Kind::Unknown; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
      [&](int32_t local) { stored = local; },
      4,
      error));
  CHECK(error == "native backend requires map lookup key to be numeric/bool");

  error.clear();
  CHECK_FALSE(primec::ir_lowerer::emitMapLookupNonStringKeyLocal(
      Kind::Int64,
      keyExpr,
      locals,
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return Kind::Float64; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
      [&](int32_t local) { stored = local; },
      5,
      error));
  CHECK(error == "native backend requires map lookup key type to match map key type");

  error.clear();
  CHECK_FALSE(primec::ir_lowerer::emitMapLookupNonStringKeyLocal(
      Kind::Int32,
      keyExpr,
      locals,
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return Kind::Int32; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [&](int32_t local) { stored = local; },
      6,
      error));

  error.clear();
  CHECK(primec::ir_lowerer::emitMapLookupNonStringKeyLocal(
      Kind::Int32,
      keyExpr,
      locals,
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return Kind::Int32; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
      [&](int32_t local) { stored = local; },
      7,
      error));
  CHECK(stored == 7);
  CHECK(error.empty());
}

TEST_CASE("ir lowerer call helpers emit map lookup target pointer local") {
  primec::Expr targetExpr;
  targetExpr.kind = primec::Expr::Kind::Name;
  targetExpr.name = "values";

  primec::ir_lowerer::LocalMap locals;
  locals[targetExpr.name] = primec::ir_lowerer::LocalInfo{};

  int nextLocal = 40;
  int stored = -1;
  primec::Expr capturedExpr;
  bool emitCalled = false;
  int32_t ptrLocal = -1;
  CHECK(primec::ir_lowerer::emitMapLookupTargetPointerLocal(
      targetExpr,
      locals,
      [&]() { return nextLocal++; },
      [&](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &localMap) {
        emitCalled = true;
        capturedExpr = expr;
        return localMap.count("values") == 1;
      },
      [&](int32_t localIndex) { stored = localIndex; },
      ptrLocal));
  CHECK(emitCalled);
  CHECK(capturedExpr.kind == primec::Expr::Kind::Name);
  CHECK(capturedExpr.name == "values");
  CHECK(ptrLocal == 40);
  CHECK(stored == 40);

  stored = -1;
  emitCalled = false;
  CHECK_FALSE(primec::ir_lowerer::emitMapLookupTargetPointerLocal(
      targetExpr,
      locals,
      [&]() { return nextLocal++; },
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        emitCalled = true;
        return false;
      },
      [&](int32_t localIndex) { stored = localIndex; },
      ptrLocal));
  CHECK(emitCalled);
  CHECK(ptrLocal == 41);
  CHECK(stored == -1);
}

TEST_CASE("ir lowerer call helpers emit map lookup key local") {
  using Kind = primec::ir_lowerer::LocalInfo::ValueKind;

  primec::Expr keyExpr;
  keyExpr.kind = primec::Expr::Kind::Name;
  keyExpr.name = "k";
  primec::ir_lowerer::LocalMap locals;
  locals["k"] = primec::ir_lowerer::LocalInfo{};

  int nextLocal = 50;
  int pushed = -1;
  int stored = -1;
  bool inferCalled = false;
  bool emitCalled = false;
  int32_t keyLocal = -1;
  std::string error;

  CHECK(primec::ir_lowerer::emitMapLookupKeyLocal(
      Kind::String,
      keyExpr,
      locals,
      [&]() { return nextLocal++; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &stringIndexOut, size_t &lengthOut) {
        stringIndexOut = 13;
        lengthOut = 2;
        return true;
      },
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        inferCalled = true;
        return Kind::Int32;
      },
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        emitCalled = true;
        return true;
      },
      [&](int32_t imm) { pushed = imm; },
      [&](int32_t local) { stored = local; },
      keyLocal,
      error));
  CHECK(keyLocal == 50);
  CHECK(pushed == 13);
  CHECK(stored == 50);
  CHECK_FALSE(inferCalled);
  CHECK_FALSE(emitCalled);
  CHECK(error.empty());

  pushed = -1;
  stored = -1;
  inferCalled = false;
  emitCalled = false;
  error.clear();
  CHECK(primec::ir_lowerer::emitMapLookupKeyLocal(
      Kind::Int32,
      keyExpr,
      locals,
      [&]() { return nextLocal++; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return false; },
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        inferCalled = true;
        return Kind::Int32;
      },
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        emitCalled = true;
        return true;
      },
      [&](int32_t imm) { pushed = imm; },
      [&](int32_t local) { stored = local; },
      keyLocal,
      error));
  CHECK(keyLocal == 51);
  CHECK(pushed == -1);
  CHECK(stored == 51);
  CHECK(inferCalled);
  CHECK(emitCalled);
  CHECK(error.empty());

  pushed = -1;
  stored = -1;
  inferCalled = false;
  emitCalled = false;
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::emitMapLookupKeyLocal(
      Kind::String,
      keyExpr,
      locals,
      [&]() { return nextLocal++; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return false; },
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        inferCalled = true;
        return Kind::Int32;
      },
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        emitCalled = true;
        return true;
      },
      [&](int32_t imm) { pushed = imm; },
      [&](int32_t local) { stored = local; },
      keyLocal,
      error));
  CHECK(keyLocal == 52);
  CHECK(pushed == -1);
  CHECK(stored == -1);
  CHECK_FALSE(inferCalled);
  CHECK_FALSE(emitCalled);
  CHECK(error == "native backend requires map lookup key to be string literal or binding backed by literals");
}

TEST_CASE("ir lowerer call helpers emit map lookup loop search scaffold") {
  using Kind = primec::ir_lowerer::LocalInfo::ValueKind;

  std::vector<primec::Instruction> instructions;
  int nextLocal = 40;
  const auto loopLocals = primec::ir_lowerer::emitMapLookupLoopSearchScaffold(
      3,
      7,
      Kind::Int32,
      [&]() { return nextLocal++; },
      [&]() { return instructions.size(); },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      [&](size_t instructionIndex, uint64_t imm) { instructions[instructionIndex].imm = imm; });

  CHECK(loopLocals.countLocal == 40);
  CHECK(loopLocals.indexLocal == 41);
  REQUIRE(instructions.size() == 28);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 3);
  CHECK(instructions[1].op == primec::IrOpcode::LoadIndirect);
  CHECK(instructions[2].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[2].imm == 40);
  CHECK(instructions[3].op == primec::IrOpcode::PushI32);
  CHECK(instructions[3].imm == 0);
  CHECK(instructions[4].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[4].imm == 41);
  CHECK(instructions[5].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[5].imm == 41);
  CHECK(instructions[6].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[6].imm == 40);
  CHECK(instructions[7].op == primec::IrOpcode::CmpLtI32);
  CHECK(instructions[8].op == primec::IrOpcode::JumpIfZero);
  CHECK(instructions[8].imm == 28);
  CHECK(instructions[9].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[9].imm == 3);
  CHECK(instructions[10].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[10].imm == 41);
  CHECK(instructions[11].op == primec::IrOpcode::PushI32);
  CHECK(instructions[11].imm == 2);
  CHECK(instructions[12].op == primec::IrOpcode::MulI32);
  CHECK(instructions[13].op == primec::IrOpcode::PushI32);
  CHECK(instructions[13].imm == 1);
  CHECK(instructions[14].op == primec::IrOpcode::AddI32);
  CHECK(instructions[15].op == primec::IrOpcode::PushI32);
  CHECK(instructions[15].imm == primec::IrSlotBytesI32);
  CHECK(instructions[16].op == primec::IrOpcode::MulI32);
  CHECK(instructions[17].op == primec::IrOpcode::AddI64);
  CHECK(instructions[18].op == primec::IrOpcode::LoadIndirect);
  CHECK(instructions[19].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[19].imm == 7);
  CHECK(instructions[20].op == primec::IrOpcode::CmpEqI32);
  CHECK(instructions[21].op == primec::IrOpcode::JumpIfZero);
  CHECK(instructions[21].imm == 23);
  CHECK(instructions[22].op == primec::IrOpcode::Jump);
  CHECK(instructions[22].imm == 28);
  CHECK(instructions[23].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[23].imm == 41);
  CHECK(instructions[24].op == primec::IrOpcode::PushI32);
  CHECK(instructions[24].imm == 1);
  CHECK(instructions[25].op == primec::IrOpcode::AddI32);
  CHECK(instructions[26].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[26].imm == 41);
  CHECK(instructions[27].op == primec::IrOpcode::Jump);
  CHECK(instructions[27].imm == 5);
}

TEST_CASE("ir lowerer call helpers emit map lookup access epilogue") {
  std::vector<primec::Instruction> instructions;
  int keyNotFoundCalls = 0;
  primec::ir_lowerer::emitMapLookupAccessEpilogue(
      "at",
      5,
      6,
      7,
      [&]() { ++keyNotFoundCalls; },
      [&]() { return instructions.size(); },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      [&](size_t instructionIndex, uint64_t imm) { instructions[instructionIndex].imm = imm; });
  CHECK(keyNotFoundCalls == 1);
  REQUIRE(instructions.size() == 14);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 6);
  CHECK(instructions[1].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[1].imm == 7);
  CHECK(instructions[2].op == primec::IrOpcode::CmpEqI32);
  CHECK(instructions[3].op == primec::IrOpcode::JumpIfZero);
  CHECK(instructions[3].imm == 4);
  CHECK(instructions[4].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[4].imm == 5);
  CHECK(instructions[5].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[5].imm == 6);
  CHECK(instructions[6].op == primec::IrOpcode::PushI32);
  CHECK(instructions[6].imm == 2);
  CHECK(instructions[7].op == primec::IrOpcode::MulI32);
  CHECK(instructions[8].op == primec::IrOpcode::PushI32);
  CHECK(instructions[8].imm == 2);
  CHECK(instructions[9].op == primec::IrOpcode::AddI32);
  CHECK(instructions[10].op == primec::IrOpcode::PushI32);
  CHECK(instructions[10].imm == primec::IrSlotBytesI32);
  CHECK(instructions[11].op == primec::IrOpcode::MulI32);
  CHECK(instructions[12].op == primec::IrOpcode::AddI64);
  CHECK(instructions[13].op == primec::IrOpcode::LoadIndirect);

  instructions.clear();
  keyNotFoundCalls = 0;
  primec::ir_lowerer::emitMapLookupAccessEpilogue(
      "find",
      5,
      6,
      7,
      [&]() { ++keyNotFoundCalls; },
      [&]() { return instructions.size(); },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      [&](size_t instructionIndex, uint64_t imm) { instructions[instructionIndex].imm = imm; });
  CHECK(keyNotFoundCalls == 0);
  REQUIRE(instructions.size() == 10);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 5);
  CHECK(instructions[1].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[1].imm == 6);
  CHECK(instructions[2].op == primec::IrOpcode::PushI32);
  CHECK(instructions[2].imm == 2);
  CHECK(instructions[3].op == primec::IrOpcode::MulI32);
  CHECK(instructions[4].op == primec::IrOpcode::PushI32);
  CHECK(instructions[4].imm == 2);
  CHECK(instructions[5].op == primec::IrOpcode::AddI32);
  CHECK(instructions[6].op == primec::IrOpcode::PushI32);
  CHECK(instructions[6].imm == primec::IrSlotBytesI32);
  CHECK(instructions[7].op == primec::IrOpcode::MulI32);
  CHECK(instructions[8].op == primec::IrOpcode::AddI64);
  CHECK(instructions[9].op == primec::IrOpcode::LoadIndirect);
}

TEST_CASE("ir lowerer call helpers emit map lookup access") {
  using Kind = primec::ir_lowerer::LocalInfo::ValueKind;

  primec::Expr targetExpr;
  targetExpr.kind = primec::Expr::Kind::Name;
  targetExpr.name = "mapTarget";
  primec::Expr keyExpr;
  keyExpr.kind = primec::Expr::Kind::Name;
  keyExpr.name = "mapKey";
  primec::ir_lowerer::LocalMap locals;
  locals[targetExpr.name] = primec::ir_lowerer::LocalInfo{};
  locals[keyExpr.name] = primec::ir_lowerer::LocalInfo{};

  std::vector<primec::Instruction> instructions;
  int nextLocal = 20;
  int emitExprCalls = 0;
  int inferCalls = 0;
  int keyNotFoundCalls = 0;
  std::string error;
  CHECK(primec::ir_lowerer::emitMapLookupAccess(
      "at",
      Kind::Int32,
      targetExpr,
      keyExpr,
      locals,
      [&]() { return nextLocal++; },
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        ++emitExprCalls;
        return true;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return false; },
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        ++inferCalls;
        return Kind::Int32;
      },
      [&]() { ++keyNotFoundCalls; },
      [&]() { return instructions.size(); },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      [&](size_t instructionIndex, uint64_t imm) { instructions[instructionIndex].imm = imm; },
      error));
  CHECK(emitExprCalls == 2);
  CHECK(inferCalls == 1);
  CHECK(keyNotFoundCalls == 1);
  CHECK(error.empty());
  REQUIRE(instructions.size() == 44);
  CHECK(instructions[0].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[0].imm == 20);
  CHECK(instructions[1].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[1].imm == 21);
  CHECK(instructions[10].op == primec::IrOpcode::JumpIfZero);
  CHECK(instructions[10].imm == 30);
  CHECK(instructions[23].op == primec::IrOpcode::JumpIfZero);
  CHECK(instructions[23].imm == 25);
  CHECK(instructions[24].op == primec::IrOpcode::Jump);
  CHECK(instructions[24].imm == 30);
  CHECK(instructions[33].op == primec::IrOpcode::JumpIfZero);
  CHECK(instructions[33].imm == 34);
  CHECK(instructions[34].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[34].imm == 20);
  CHECK(instructions[43].op == primec::IrOpcode::LoadIndirect);

  instructions.clear();
  nextLocal = 40;
  emitExprCalls = 0;
  inferCalls = 0;
  keyNotFoundCalls = 0;
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::emitMapLookupAccess(
      "at",
      Kind::Int32,
      targetExpr,
      keyExpr,
      locals,
      [&]() { return nextLocal++; },
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        ++emitExprCalls;
        return false;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return false; },
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        ++inferCalls;
        return Kind::Int32;
      },
      [&]() { ++keyNotFoundCalls; },
      [&]() { return instructions.size(); },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      [&](size_t instructionIndex, uint64_t imm) { instructions[instructionIndex].imm = imm; },
      error));
  CHECK(emitExprCalls == 1);
  CHECK(inferCalls == 0);
  CHECK(keyNotFoundCalls == 0);
  CHECK(instructions.empty());
  CHECK(error.empty());

  instructions.clear();
  nextLocal = 50;
  emitExprCalls = 0;
  inferCalls = 0;
  keyNotFoundCalls = 0;
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::emitMapLookupAccess(
      "at",
      Kind::String,
      targetExpr,
      keyExpr,
      locals,
      [&]() { return nextLocal++; },
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        ++emitExprCalls;
        return true;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return false; },
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        ++inferCalls;
        return Kind::Int32;
      },
      [&]() { ++keyNotFoundCalls; },
      [&]() { return instructions.size(); },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      [&](size_t instructionIndex, uint64_t imm) { instructions[instructionIndex].imm = imm; },
      error));
  CHECK(emitExprCalls == 1);
  CHECK(inferCalls == 0);
  CHECK(keyNotFoundCalls == 0);
  REQUIRE(instructions.size() == 1);
  CHECK(instructions[0].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[0].imm == 50);
  CHECK(error == "native backend requires map lookup key to be string literal or binding backed by literals");
}

TEST_CASE("ir lowerer call helpers emit string access load") {
  using Kind = primec::ir_lowerer::LocalInfo::ValueKind;

  std::vector<primec::Instruction> instructions;
  int stringIndexOutOfBoundsCalls = 0;
  primec::ir_lowerer::emitStringAccessLoad(
      "at",
      9,
      Kind::Int32,
      5,
      12,
      [&]() { ++stringIndexOutOfBoundsCalls; },
      [&]() { return instructions.size(); },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      [&](size_t instructionIndex, uint64_t imm) { instructions[instructionIndex].imm = imm; });

  CHECK(stringIndexOutOfBoundsCalls == 2);
  REQUIRE(instructions.size() == 10);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 9);
  CHECK(instructions[1].op == primec::ir_lowerer::pushZeroForIndex(Kind::Int32));
  CHECK(instructions[2].op == primec::ir_lowerer::cmpLtForIndex(Kind::Int32));
  CHECK(instructions[3].op == primec::IrOpcode::JumpIfZero);
  CHECK(instructions[3].imm == 4);
  CHECK(instructions[4].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[4].imm == 9);
  CHECK(instructions[5].op == primec::IrOpcode::PushI32);
  CHECK(instructions[5].imm == 5);
  CHECK(instructions[6].op == primec::ir_lowerer::cmpGeForIndex(Kind::Int32));
  CHECK(instructions[7].op == primec::IrOpcode::JumpIfZero);
  CHECK(instructions[7].imm == 8);
  CHECK(instructions[8].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[8].imm == 9);
  CHECK(instructions[9].op == primec::IrOpcode::LoadStringByte);
  CHECK(instructions[9].imm == 12);

  instructions.clear();
  stringIndexOutOfBoundsCalls = 0;
  primec::ir_lowerer::emitStringAccessLoad(
      "index",
      11,
      Kind::UInt64,
      999,
      4,
      [&]() { ++stringIndexOutOfBoundsCalls; },
      [&]() { return instructions.size(); },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      [&](size_t instructionIndex, uint64_t imm) { instructions[instructionIndex].imm = imm; });

  CHECK(stringIndexOutOfBoundsCalls == 0);
  REQUIRE(instructions.size() == 2);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 11);
  CHECK(instructions[1].op == primec::IrOpcode::LoadStringByte);
  CHECK(instructions[1].imm == 4);
}

TEST_CASE("ir lowerer call helpers emit array vector access load") {
  using Kind = primec::ir_lowerer::LocalInfo::ValueKind;

  std::vector<primec::Instruction> instructions;
  int nextLocal = 30;
  int arrayIndexOutOfBoundsCalls = 0;
  primec::ir_lowerer::emitArrayVectorAccessLoad(
      "at",
      8,
      9,
      Kind::Int32,
      2,
      [&]() { return nextLocal++; },
      [&]() { ++arrayIndexOutOfBoundsCalls; },
      [&]() { return instructions.size(); },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      [&](size_t instructionIndex, uint64_t imm) { instructions[instructionIndex].imm = imm; });
  CHECK(nextLocal == 31);
  CHECK(arrayIndexOutOfBoundsCalls == 2);
  REQUIRE(instructions.size() == 19);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 8);
  CHECK(instructions[1].op == primec::IrOpcode::LoadIndirect);
  CHECK(instructions[2].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[2].imm == 30);
  CHECK(instructions[3].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[3].imm == 9);
  CHECK(instructions[4].op == primec::ir_lowerer::pushZeroForIndex(Kind::Int32));
  CHECK(instructions[5].op == primec::ir_lowerer::cmpLtForIndex(Kind::Int32));
  CHECK(instructions[6].op == primec::IrOpcode::JumpIfZero);
  CHECK(instructions[6].imm == 7);
  CHECK(instructions[7].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[7].imm == 9);
  CHECK(instructions[8].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[8].imm == 30);
  CHECK(instructions[9].op == primec::ir_lowerer::cmpGeForIndex(Kind::Int32));
  CHECK(instructions[10].op == primec::IrOpcode::JumpIfZero);
  CHECK(instructions[10].imm == 11);
  CHECK(instructions[11].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[11].imm == 8);
  CHECK(instructions[12].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[12].imm == 9);
  CHECK(instructions[13].op == primec::ir_lowerer::pushOneForIndex(Kind::Int32));
  CHECK(instructions[13].imm == 2);
  CHECK(instructions[14].op == primec::ir_lowerer::addForIndex(Kind::Int32));
  CHECK(instructions[15].op == primec::ir_lowerer::pushOneForIndex(Kind::Int32));
  CHECK(instructions[15].imm == primec::IrSlotBytesI32);
  CHECK(instructions[16].op == primec::ir_lowerer::mulForIndex(Kind::Int32));
  CHECK(instructions[17].op == primec::IrOpcode::AddI64);
  CHECK(instructions[18].op == primec::IrOpcode::LoadIndirect);

  instructions.clear();
  nextLocal = 40;
  arrayIndexOutOfBoundsCalls = 0;
  primec::ir_lowerer::emitArrayVectorAccessLoad(
      "index",
      8,
      9,
      Kind::UInt64,
      1,
      [&]() { return nextLocal++; },
      [&]() { ++arrayIndexOutOfBoundsCalls; },
      [&]() { return instructions.size(); },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      [&](size_t instructionIndex, uint64_t imm) { instructions[instructionIndex].imm = imm; });
  CHECK(nextLocal == 40);
  CHECK(arrayIndexOutOfBoundsCalls == 0);
  REQUIRE(instructions.size() == 8);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 8);
  CHECK(instructions[1].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[1].imm == 9);
  CHECK(instructions[2].op == primec::ir_lowerer::pushOneForIndex(Kind::UInt64));
  CHECK(instructions[2].imm == 1);
  CHECK(instructions[3].op == primec::ir_lowerer::addForIndex(Kind::UInt64));
  CHECK(instructions[4].op == primec::ir_lowerer::pushOneForIndex(Kind::UInt64));
  CHECK(instructions[4].imm == primec::IrSlotBytesI32);
  CHECK(instructions[5].op == primec::ir_lowerer::mulForIndex(Kind::UInt64));
  CHECK(instructions[6].op == primec::IrOpcode::AddI64);
  CHECK(instructions[7].op == primec::IrOpcode::LoadIndirect);
}

TEST_CASE("ir lowerer call helpers emit map lookup loop locals") {
  std::vector<primec::Instruction> instructions;
  int nextLocal = 30;
  auto locals = primec::ir_lowerer::emitMapLookupLoopLocals(
      12,
      [&]() { return nextLocal++; },
      [&](primec::IrOpcode op, uint64_t imm) {
        instructions.push_back({op, imm});
      });

  CHECK(locals.countLocal == 30);
  CHECK(locals.indexLocal == 31);
  REQUIRE(instructions.size() == 5);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 12);
  CHECK(instructions[1].op == primec::IrOpcode::LoadIndirect);
  CHECK(instructions[2].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[2].imm == 30);
  CHECK(instructions[3].op == primec::IrOpcode::PushI32);
  CHECK(instructions[3].imm == 0);
  CHECK(instructions[4].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[4].imm == 31);
}

TEST_CASE("ir lowerer call helpers emit map lookup loop condition") {
  std::vector<primec::Instruction> instructions;
  auto anchors = primec::ir_lowerer::emitMapLookupLoopCondition(
      7,
      9,
      [&]() { return instructions.size(); },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); });

  CHECK(anchors.loopStart == 0);
  CHECK(anchors.jumpLoopEnd == 3);
  REQUIRE(instructions.size() == 4);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 7);
  CHECK(instructions[1].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[1].imm == 9);
  CHECK(instructions[2].op == primec::IrOpcode::CmpLtI32);
  CHECK(instructions[3].op == primec::IrOpcode::JumpIfZero);
  CHECK(instructions[3].imm == 0);
}

TEST_CASE("ir lowerer call helpers emit map lookup loop match check") {
  std::vector<primec::Instruction> instructions;
  auto anchors = primec::ir_lowerer::emitMapLookupLoopMatchCheck(
      4,
      5,
      6,
      primec::ir_lowerer::LocalInfo::ValueKind::Int64,
      [&]() { return instructions.size(); },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); });

  CHECK(anchors.jumpNotMatch == 12);
  CHECK(anchors.jumpFound == 13);
  REQUIRE(instructions.size() == 14);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 4);
  CHECK(instructions[1].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[1].imm == 5);
  CHECK(instructions[6].op == primec::IrOpcode::PushI32);
  CHECK(instructions[6].imm == primec::IrSlotBytesI32);
  CHECK(instructions[10].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[10].imm == 6);
  CHECK(instructions[11].op == primec::IrOpcode::CmpEqI64);
  CHECK(instructions[12].op == primec::IrOpcode::JumpIfZero);
  CHECK(instructions[12].imm == 0);
  CHECK(instructions[13].op == primec::IrOpcode::Jump);
  CHECK(instructions[13].imm == 0);
}

TEST_CASE("ir lowerer call helpers emit map lookup loop advance patching") {
  std::vector<primec::Instruction> instructions = {
      {primec::IrOpcode::PushI32, 10},
      {primec::IrOpcode::JumpIfZero, 0},
      {primec::IrOpcode::PushI32, 12},
      {primec::IrOpcode::JumpIfZero, 0},
      {primec::IrOpcode::Jump, 0},
      {primec::IrOpcode::PushI32, 13},
  };

  primec::ir_lowerer::emitMapLookupLoopAdvanceAndPatch(
      1,
      3,
      4,
      2,
      9,
      [&]() { return instructions.size(); },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      [&](size_t instructionIndex, uint64_t imm) { instructions[instructionIndex].imm = imm; });

  REQUIRE(instructions.size() == 11);
  CHECK(instructions[1].imm == 6);
  CHECK(instructions[3].imm == 11);
  CHECK(instructions[4].imm == 11);
  CHECK(instructions[6].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[6].imm == 9);
  CHECK(instructions[7].op == primec::IrOpcode::PushI32);
  CHECK(instructions[7].imm == 1);
  CHECK(instructions[8].op == primec::IrOpcode::AddI32);
  CHECK(instructions[9].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[9].imm == 9);
  CHECK(instructions[10].op == primec::IrOpcode::Jump);
  CHECK(instructions[10].imm == 2);
}

TEST_CASE("ir lowerer call helpers emit map lookup at key-not-found guard") {
  std::vector<primec::Instruction> instructions = {
      {primec::IrOpcode::PushI32, 7},
  };
  int notFoundCalls = 0;

  primec::ir_lowerer::emitMapLookupAtKeyNotFoundGuard(
      11,
      12,
      [&]() {
        ++notFoundCalls;
        instructions.push_back({primec::IrOpcode::PushI32, 99});
      },
      [&]() { return instructions.size(); },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      [&](size_t instructionIndex, uint64_t imm) { instructions[instructionIndex].imm = imm; });

  CHECK(notFoundCalls == 1);
  REQUIRE(instructions.size() == 6);
  CHECK(instructions[1].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[1].imm == 11);
  CHECK(instructions[2].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[2].imm == 12);
  CHECK(instructions[3].op == primec::IrOpcode::CmpEqI32);
  CHECK(instructions[4].op == primec::IrOpcode::JumpIfZero);
  CHECK(instructions[4].imm == 6);
  CHECK(instructions[5].op == primec::IrOpcode::PushI32);
  CHECK(instructions[5].imm == 99);
}

TEST_CASE("ir lowerer call helpers emit map lookup value load") {
  std::vector<primec::Instruction> instructions;
  primec::ir_lowerer::emitMapLookupValueLoad(
      21,
      22,
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); });

  REQUIRE(instructions.size() == 10);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 21);
  CHECK(instructions[1].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[1].imm == 22);
  CHECK(instructions[2].op == primec::IrOpcode::PushI32);
  CHECK(instructions[2].imm == 2);
  CHECK(instructions[3].op == primec::IrOpcode::MulI32);
  CHECK(instructions[4].op == primec::IrOpcode::PushI32);
  CHECK(instructions[4].imm == 2);
  CHECK(instructions[5].op == primec::IrOpcode::AddI32);
  CHECK(instructions[6].op == primec::IrOpcode::PushI32);
  CHECK(instructions[6].imm == primec::IrSlotBytesI32);
  CHECK(instructions[7].op == primec::IrOpcode::MulI32);
  CHECK(instructions[8].op == primec::IrOpcode::AddI64);
  CHECK(instructions[9].op == primec::IrOpcode::LoadIndirect);
}

TEST_CASE("ir lowerer call helpers validate map lookup key kinds") {
  using Kind = primec::ir_lowerer::LocalInfo::ValueKind;

  std::string error;
  CHECK(primec::ir_lowerer::validateMapLookupKeyKind(Kind::Int32, Kind::Int32, error));
  CHECK(error.empty());

  CHECK_FALSE(primec::ir_lowerer::validateMapLookupKeyKind(Kind::Int32, Kind::Unknown, error));
  CHECK(error == "native backend requires map lookup key to be numeric/bool");

  error.clear();
  CHECK_FALSE(primec::ir_lowerer::validateMapLookupKeyKind(Kind::Int32, Kind::String, error));
  CHECK(error == "native backend requires map lookup key to be numeric/bool");

  error.clear();
  CHECK_FALSE(primec::ir_lowerer::validateMapLookupKeyKind(Kind::Int64, Kind::Float64, error));
  CHECK(error == "native backend requires map lookup key type to match map key type");
}

TEST_CASE("ir lowerer call helpers handle non-method count fallback") {
  using Result = primec::ir_lowerer::CountMethodFallbackResult;

  primec::Expr countCall;
  countCall.kind = primec::Expr::Kind::Call;
  countCall.name = "count";
  primec::Expr targetArg;
  targetArg.kind = primec::Expr::Kind::Name;
  targetArg.name = "items";
  countCall.args.push_back(targetArg);

  primec::Definition callee;
  callee.fullPath = "/pkg/items/count";

  std::string error;
  int resolveCalls = 0;
  int emitCalls = 0;
  CHECK(primec::ir_lowerer::tryEmitNonMethodCountFallback(
            countCall,
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [&](const primec::Expr &methodExpr) -> const primec::Definition * {
              ++resolveCalls;
              CHECK(methodExpr.isMethodCall);
              CHECK(methodExpr.name == "count");
              return &callee;
            },
            [&](const primec::Expr &methodExpr, const primec::Definition &resolvedCallee) {
              ++emitCalls;
              CHECK(methodExpr.isMethodCall);
              CHECK(resolvedCallee.fullPath == "/pkg/items/count");
              return true;
            },
            error) == Result::Emitted);
  CHECK(error.empty());
  CHECK(resolveCalls == 1);
  CHECK(emitCalls == 1);

  CHECK(primec::ir_lowerer::tryEmitNonMethodCountFallback(
            countCall,
            [](const primec::Expr &) { return true; },
            [](const primec::Expr &) { return false; },
            [&](const primec::Expr &) -> const primec::Definition * {
              CHECK(false);
              return nullptr;
            },
            [&](const primec::Expr &, const primec::Definition &) {
              CHECK(false);
              return false;
            },
            error) == Result::NotHandled);

  CHECK(primec::ir_lowerer::tryEmitNonMethodCountFallback(
            countCall,
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [&](const primec::Expr &) -> const primec::Definition * { return nullptr; },
            [&](const primec::Expr &, const primec::Definition &) {
              CHECK(false);
              return false;
            },
            error) == Result::NoCallee);

  primec::Expr bodyArgExpr;
  bodyArgExpr.kind = primec::Expr::Kind::Literal;
  bodyArgExpr.intWidth = 32;
  bodyArgExpr.literalValue = 1;
  primec::Expr countWithBodyArgs = countCall;
  countWithBodyArgs.hasBodyArguments = true;
  countWithBodyArgs.bodyArguments.push_back(bodyArgExpr);
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitNonMethodCountFallback(
            countWithBodyArgs,
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [&](const primec::Expr &) -> const primec::Definition * { return &callee; },
            [&](const primec::Expr &, const primec::Definition &) {
              CHECK(false);
              return false;
            },
            error) == Result::Error);
  CHECK(error == "native backend does not support block arguments on calls");

  error.clear();
  CHECK(primec::ir_lowerer::tryEmitNonMethodCountFallback(
            countCall,
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [&](const primec::Expr &) -> const primec::Definition * { return &callee; },
            [&](const primec::Expr &, const primec::Definition &) {
              error = "emit failed";
              return false;
            },
            error) == Result::Error);
  CHECK(error == "emit failed");
}

TEST_CASE("ir lowerer on_error helpers wire definition handlers") {
  primec::Program program;

  primec::Definition handlerDef;
  handlerDef.fullPath = "/handler";
  handlerDef.namespacePrefix = "";
  program.definitions.push_back(handlerDef);

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.namespacePrefix = "";
  primec::Transform onError;
  onError.name = "on_error";
  onError.templateArgs = {"FileError", "handler"};
  onError.arguments = {"1i32"};
  mainDef.transforms.push_back(onError);
  program.definitions.push_back(mainDef);

  auto resolveExprPath = [](const primec::Expr &expr) {
    if (!expr.name.empty() && expr.name[0] == '/') {
      return expr.name;
    }
    return std::string("/") + expr.name;
  };
  auto definitionExists = [](const std::string &path) { return path == "/handler" || path == "/main"; };

  primec::ir_lowerer::OnErrorByDefinition onErrorByDef;
  std::string error;
  REQUIRE(primec::ir_lowerer::buildOnErrorByDefinition(
      program, resolveExprPath, definitionExists, onErrorByDef, error));
  CHECK(error.empty());

  REQUIRE(onErrorByDef.count("/main") == 1);
  REQUIRE(onErrorByDef.at("/main").has_value());
  CHECK(onErrorByDef.at("/main")->handlerPath == "/handler");
  REQUIRE(onErrorByDef.at("/main")->boundArgs.size() == 1);
  CHECK(onErrorByDef.at("/main")->boundArgs.front().kind == primec::Expr::Kind::Literal);
  CHECK(onErrorByDef.at("/main")->boundArgs.front().literalValue == 1);

  REQUIRE(onErrorByDef.count("/handler") == 1);
  CHECK_FALSE(onErrorByDef.at("/handler").has_value());
}

TEST_CASE("ir lowerer on_error helpers wire definition handlers from call adapters") {
  primec::Program program;

  primec::Definition handlerDef;
  handlerDef.fullPath = "/handler";
  handlerDef.namespacePrefix = "";
  program.definitions.push_back(handlerDef);

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.namespacePrefix = "";
  primec::Transform onError;
  onError.name = "on_error";
  onError.templateArgs = {"FileError", "handler"};
  onError.arguments = {"1i32"};
  mainDef.transforms.push_back(onError);
  program.definitions.push_back(mainDef);

  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/handler", &program.definitions[0]},
      {"/main", &program.definitions[1]},
  };
  const std::unordered_map<std::string, std::string> importAliases = {};
  const auto callResolutionAdapters = primec::ir_lowerer::makeCallResolutionAdapters(defMap, importAliases);

  primec::ir_lowerer::OnErrorByDefinition onErrorByDef;
  std::string error;
  REQUIRE(primec::ir_lowerer::buildOnErrorByDefinitionFromCallResolutionAdapters(
      program, callResolutionAdapters, onErrorByDef, error));
  CHECK(error.empty());

  REQUIRE(onErrorByDef.count("/main") == 1);
  REQUIRE(onErrorByDef.at("/main").has_value());
  CHECK(onErrorByDef.at("/main")->handlerPath == "/handler");
  REQUIRE(onErrorByDef.at("/main")->boundArgs.size() == 1);
  CHECK(onErrorByDef.at("/main")->boundArgs.front().kind == primec::Expr::Kind::Literal);
  CHECK(onErrorByDef.at("/main")->boundArgs.front().literalValue == 1);

  REQUIRE(onErrorByDef.count("/handler") == 1);
  CHECK_FALSE(onErrorByDef.at("/handler").has_value());
}

TEST_CASE("ir lowerer on_error helpers build bundled entry call and on_error setup") {
  primec::Program program;

  primec::Definition handlerDef;
  handlerDef.fullPath = "/handler";
  handlerDef.namespacePrefix = "";
  program.definitions.push_back(handlerDef);

  primec::Definition calleeDef;
  calleeDef.fullPath = "/callee";
  calleeDef.namespacePrefix = "";
  program.definitions.push_back(calleeDef);

  primec::Definition entryDef;
  entryDef.fullPath = "/main";
  entryDef.namespacePrefix = "";
  primec::Transform onError;
  onError.name = "on_error";
  onError.templateArgs = {"FileError", "handler"};
  onError.arguments = {"1i32"};
  entryDef.transforms.push_back(onError);
  primec::Expr tailCall;
  tailCall.kind = primec::Expr::Kind::Call;
  tailCall.name = "callee";
  entryDef.statements = {tailCall};
  program.definitions.push_back(entryDef);

  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/handler", &program.definitions[0]},
      {"/callee", &program.definitions[1]},
      {"/main", &program.definitions[2]},
  };
  const std::unordered_map<std::string, std::string> importAliases = {};

  primec::ir_lowerer::EntryCallOnErrorSetup setup;
  std::string error;
  REQUIRE(primec::ir_lowerer::buildEntryCallOnErrorSetup(
      program, program.definitions[2], true, defMap, importAliases, setup, error));
  CHECK(error.empty());
  CHECK(setup.hasTailExecution);
  CHECK(setup.callResolutionAdapters.resolveExprPath(tailCall) == "/callee");
  CHECK(setup.callResolutionAdapters.definitionExists("/callee"));

  REQUIRE(setup.onErrorByDefinition.count("/main") == 1);
  REQUIRE(setup.onErrorByDefinition.at("/main").has_value());
  CHECK(setup.onErrorByDefinition.at("/main")->handlerPath == "/handler");
  REQUIRE(setup.onErrorByDefinition.at("/main")->boundArgs.size() == 1);
  CHECK(setup.onErrorByDefinition.at("/main")->boundArgs.front().literalValue == 1);

  primec::Program badProgram = program;
  badProgram.definitions[2].transforms.front().templateArgs[1] = "missing";
  const std::unordered_map<std::string, const primec::Definition *> badDefMap = {
      {"/handler", &badProgram.definitions[0]},
      {"/callee", &badProgram.definitions[1]},
      {"/main", &badProgram.definitions[2]},
  };
  CHECK_FALSE(primec::ir_lowerer::buildEntryCallOnErrorSetup(
      badProgram, badProgram.definitions[2], true, badDefMap, importAliases, setup, error));
  CHECK(error == "unknown on_error handler: /missing");
}

TEST_CASE("ir lowerer on_error helpers build bundled entry count and call/on_error setup") {
  primec::Program program;

  primec::Definition handlerDef;
  handlerDef.fullPath = "/handler";
  handlerDef.namespacePrefix = "";
  program.definitions.push_back(handlerDef);

  primec::Definition calleeDef;
  calleeDef.fullPath = "/callee";
  calleeDef.namespacePrefix = "";
  program.definitions.push_back(calleeDef);

  primec::Definition entryDef;
  entryDef.fullPath = "/main";
  entryDef.namespacePrefix = "";
  primec::Expr entryParam;
  entryParam.name = "argv";
  primec::Transform arrayTransform;
  arrayTransform.name = "array";
  arrayTransform.templateArgs = {"string"};
  entryParam.transforms.push_back(arrayTransform);
  entryDef.parameters.push_back(entryParam);
  primec::Transform onError;
  onError.name = "on_error";
  onError.templateArgs = {"FileError", "handler"};
  onError.arguments = {"1i32"};
  entryDef.transforms.push_back(onError);
  primec::Expr tailCall;
  tailCall.kind = primec::Expr::Kind::Call;
  tailCall.name = "callee";
  entryDef.statements = {tailCall};
  program.definitions.push_back(entryDef);

  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/handler", &program.definitions[0]},
      {"/callee", &program.definitions[1]},
      {"/main", &program.definitions[2]},
  };
  const std::unordered_map<std::string, std::string> importAliases = {};

  primec::ir_lowerer::EntryCountCallOnErrorSetup setup;
  std::string error;
  REQUIRE(primec::ir_lowerer::buildEntryCountCallOnErrorSetup(
      program, program.definitions[2], true, defMap, importAliases, setup, error));
  CHECK(setup.countAccessSetup.hasEntryArgs);
  CHECK(setup.countAccessSetup.entryArgsName == "argv");
  CHECK(setup.callOnErrorSetup.hasTailExecution);
  CHECK(setup.callOnErrorSetup.callResolutionAdapters.resolveExprPath(tailCall) == "/callee");
  REQUIRE(setup.callOnErrorSetup.onErrorByDefinition.count("/main") == 1);
  REQUIRE(setup.callOnErrorSetup.onErrorByDefinition.at("/main").has_value());
  CHECK(setup.callOnErrorSetup.onErrorByDefinition.at("/main")->handlerPath == "/handler");

  primec::Program badEntryProgram = program;
  primec::Expr extraEntryParam = entryParam;
  extraEntryParam.name = "extra";
  badEntryProgram.definitions[2].parameters.push_back(extraEntryParam);
  const std::unordered_map<std::string, const primec::Definition *> badEntryDefMap = {
      {"/handler", &badEntryProgram.definitions[0]},
      {"/callee", &badEntryProgram.definitions[1]},
      {"/main", &badEntryProgram.definitions[2]},
  };
  CHECK_FALSE(primec::ir_lowerer::buildEntryCountCallOnErrorSetup(
      badEntryProgram, badEntryProgram.definitions[2], true, badEntryDefMap, importAliases, setup, error));
  CHECK(error == "native backend only supports a single array<string> entry parameter");
}

TEST_CASE(
    "ir lowerer uninitialized type helpers build bundled entry and setup resolution") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;
  primec::Program program;

  primec::Definition handlerDef;
  handlerDef.fullPath = "/handler";
  handlerDef.namespacePrefix = "";
  program.definitions.push_back(handlerDef);

  primec::Definition calleeDef;
  calleeDef.fullPath = "/callee";
  calleeDef.namespacePrefix = "";
  program.definitions.push_back(calleeDef);

  primec::Definition containerDef;
  containerDef.fullPath = "/pkg/Container";
  containerDef.namespacePrefix = "/pkg";
  program.definitions.push_back(containerDef);

  primec::Definition myStructDef;
  myStructDef.fullPath = "/pkg/MyStruct";
  myStructDef.namespacePrefix = "/pkg";
  program.definitions.push_back(myStructDef);

  primec::Definition entryDef;
  entryDef.fullPath = "/main";
  entryDef.namespacePrefix = "";
  primec::Expr entryParam;
  entryParam.name = "argv";
  primec::Transform arrayTransform;
  arrayTransform.name = "array";
  arrayTransform.templateArgs = {"string"};
  entryParam.transforms.push_back(arrayTransform);
  entryDef.parameters.push_back(entryParam);
  primec::Transform onError;
  onError.name = "on_error";
  onError.templateArgs = {"FileError", "handler"};
  onError.arguments = {"1i32"};
  entryDef.transforms.push_back(onError);
  primec::Expr tailCall;
  tailCall.kind = primec::Expr::Kind::Call;
  tailCall.name = "callee";
  entryDef.statements = {tailCall};
  program.definitions.push_back(entryDef);

  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/handler", &program.definitions[0]},
      {"/callee", &program.definitions[1]},
      {"/pkg/Container", &program.definitions[2]},
      {"/pkg/MyStruct", &program.definitions[3]},
      {"/main", &program.definitions[4]},
  };
  const std::unordered_map<std::string, std::string> importAliases = {{"MyStruct", "/pkg/MyStruct"}};
  const std::unordered_set<std::string> structNames = {"/pkg/Container", "/pkg/MyStruct"};

  std::string error;
  primec::ir_lowerer::EntrySetupMathTypeStructAndUninitializedResolutionSetup setup;
  REQUIRE(primec::ir_lowerer::buildEntrySetupMathTypeStructAndUninitializedResolutionSetup(
      program,
      program.definitions[4],
      true,
      defMap,
      importAliases,
      true,
      structNames,
      2,
      [](const primec::ir_lowerer::AppendStructLayoutFieldFn &appendStructLayoutField) {
        appendStructLayoutField("/pkg/MyStruct", {"value", "i32", "", false});
        appendStructLayoutField("/pkg/Container", {"slot", "uninitialized", "MyStruct", false});
      },
      setup,
      error));
  CHECK(error.empty());
  CHECK(setup.entryCountCallOnErrorSetup.countAccessSetup.hasEntryArgs);
  CHECK(setup.entryCountCallOnErrorSetup.countAccessSetup.entryArgsName == "argv");
  CHECK(setup.entryCountCallOnErrorSetup.callOnErrorSetup.hasTailExecution);
  REQUIRE(setup.entryCountCallOnErrorSetup.callOnErrorSetup.onErrorByDefinition.count("/main") == 1);
  REQUIRE(setup.entryCountCallOnErrorSetup.callOnErrorSetup.onErrorByDefinition.at("/main").has_value());
  CHECK(setup.entryCountCallOnErrorSetup.callOnErrorSetup.onErrorByDefinition.at("/main")->handlerPath ==
        "/handler");

  primec::Expr mathCall;
  mathCall.kind = primec::Expr::Kind::Call;
  mathCall.name = "sin";
  std::string builtinName;
  CHECK(setup.setupMathTypeStructAndUninitializedResolutionSetup.setupMathAndBindingAdapters
            .setupMathResolvers.getMathBuiltinName(mathCall, builtinName));
  CHECK(builtinName == "sin");
  CHECK(setup.setupMathTypeStructAndUninitializedResolutionSetup.setupTypeStructAndUninitializedResolutionSetup
            .setupTypeAndStructTypeAdapters.valueKindFromTypeName("i32") == ValueKind::Int32);

  primec::ir_lowerer::UninitializedTypeInfo typeInfo;
  REQUIRE(setup.setupMathTypeStructAndUninitializedResolutionSetup
              .setupTypeStructAndUninitializedResolutionSetup
              .structAndUninitializedResolutionSetup
              .uninitializedResolutionAdapters.resolveUninitializedTypeInfo(
                  "MyStruct", "/pkg", typeInfo));
  CHECK(typeInfo.structPath == "/pkg/MyStruct");

  primec::Program badEntryProgram = program;
  primec::Expr extraEntryParam = entryParam;
  extraEntryParam.name = "extra";
  badEntryProgram.definitions[4].parameters.push_back(extraEntryParam);
  const std::unordered_map<std::string, const primec::Definition *> badEntryDefMap = {
      {"/handler", &badEntryProgram.definitions[0]},
      {"/callee", &badEntryProgram.definitions[1]},
      {"/pkg/Container", &badEntryProgram.definitions[2]},
      {"/pkg/MyStruct", &badEntryProgram.definitions[3]},
      {"/main", &badEntryProgram.definitions[4]},
  };
  CHECK_FALSE(primec::ir_lowerer::buildEntrySetupMathTypeStructAndUninitializedResolutionSetup(
      badEntryProgram,
      badEntryProgram.definitions[4],
      true,
      badEntryDefMap,
      importAliases,
      true,
      structNames,
      2,
      [](const primec::ir_lowerer::AppendStructLayoutFieldFn &appendStructLayoutField) {
        appendStructLayoutField("/pkg/MyStruct", {"value", "i32", "", false});
        appendStructLayoutField("/pkg/Container", {"slot", "uninitialized", "MyStruct", false});
      },
      setup,
      error));
  CHECK(error == "native backend only supports a single array<string> entry parameter");
}

TEST_CASE(
    "ir lowerer uninitialized type helpers build bundled runtime entry and setup resolution") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;
  primec::Program program;

  primec::Definition handlerDef;
  handlerDef.fullPath = "/handler";
  handlerDef.namespacePrefix = "";
  program.definitions.push_back(handlerDef);

  primec::Definition calleeDef;
  calleeDef.fullPath = "/callee";
  calleeDef.namespacePrefix = "";
  program.definitions.push_back(calleeDef);

  primec::Definition containerDef;
  containerDef.fullPath = "/pkg/Container";
  containerDef.namespacePrefix = "/pkg";
  program.definitions.push_back(containerDef);

  primec::Definition myStructDef;
  myStructDef.fullPath = "/pkg/MyStruct";
  myStructDef.namespacePrefix = "/pkg";
  program.definitions.push_back(myStructDef);

  primec::Definition entryDef;
  entryDef.fullPath = "/main";
  entryDef.namespacePrefix = "";
  primec::Expr entryParam;
  entryParam.name = "argv";
  primec::Transform arrayTransform;
  arrayTransform.name = "array";
  arrayTransform.templateArgs = {"string"};
  entryParam.transforms.push_back(arrayTransform);
  entryDef.parameters.push_back(entryParam);
  primec::Transform onError;
  onError.name = "on_error";
  onError.templateArgs = {"FileError", "handler"};
  onError.arguments = {"1i32"};
  entryDef.transforms.push_back(onError);
  primec::Expr tailCall;
  tailCall.kind = primec::Expr::Kind::Call;
  tailCall.name = "callee";
  entryDef.statements = {tailCall};
  program.definitions.push_back(entryDef);

  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/handler", &program.definitions[0]},
      {"/callee", &program.definitions[1]},
      {"/pkg/Container", &program.definitions[2]},
      {"/pkg/MyStruct", &program.definitions[3]},
      {"/main", &program.definitions[4]},
  };
  const std::unordered_map<std::string, std::string> importAliases = {{"MyStruct", "/pkg/MyStruct"}};
  const std::unordered_set<std::string> structNames = {"/pkg/Container", "/pkg/MyStruct"};

  primec::IrFunction function;
  std::vector<std::string> stringTable;
  std::string error;
  primec::ir_lowerer::RuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup setup;
  REQUIRE(primec::ir_lowerer::buildRuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup(
      stringTable,
      function,
      program,
      program.definitions[4],
      true,
      defMap,
      importAliases,
      true,
      structNames,
      2,
      [](const primec::ir_lowerer::AppendStructLayoutFieldFn &appendStructLayoutField) {
        appendStructLayoutField("/pkg/MyStruct", {"value", "i32", "", false});
        appendStructLayoutField("/pkg/Container", {"slot", "uninitialized", "MyStruct", false});
      },
      setup,
      error));
  CHECK(error.empty());

  CHECK(setup.entrySetupMathTypeStructAndUninitializedResolutionSetup.entryCountCallOnErrorSetup
            .countAccessSetup.hasEntryArgs);
  CHECK(setup.entrySetupMathTypeStructAndUninitializedResolutionSetup.entryCountCallOnErrorSetup
            .callOnErrorSetup.hasTailExecution);
  CHECK(setup.entrySetupMathTypeStructAndUninitializedResolutionSetup
            .setupMathTypeStructAndUninitializedResolutionSetup
            .setupTypeStructAndUninitializedResolutionSetup
            .setupTypeAndStructTypeAdapters.valueKindFromTypeName("i32") == ValueKind::Int32);

  CHECK(setup.runtimeErrorAndStringLiteralSetup.stringLiteralHelpers.internString("hello") == 0);
  setup.runtimeErrorAndStringLiteralSetup.runtimeErrorEmitters.emitArrayIndexOutOfBounds();
  REQUIRE(function.instructions.size() == 3);
  CHECK(function.instructions[0].op == primec::IrOpcode::PrintString);
  CHECK(primec::decodePrintStringIndex(function.instructions[0].imm) == 1);
  REQUIRE(stringTable.size() == 2);
  CHECK(stringTable[0] == "hello");
  CHECK(stringTable[1] == "array index out of bounds");

  primec::Program badEntryProgram = program;
  primec::Expr extraEntryParam = entryParam;
  extraEntryParam.name = "extra";
  badEntryProgram.definitions[4].parameters.push_back(extraEntryParam);
  const std::unordered_map<std::string, const primec::Definition *> badEntryDefMap = {
      {"/handler", &badEntryProgram.definitions[0]},
      {"/callee", &badEntryProgram.definitions[1]},
      {"/pkg/Container", &badEntryProgram.definitions[2]},
      {"/pkg/MyStruct", &badEntryProgram.definitions[3]},
      {"/main", &badEntryProgram.definitions[4]},
  };
  CHECK_FALSE(primec::ir_lowerer::buildRuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup(
      stringTable,
      function,
      badEntryProgram,
      badEntryProgram.definitions[4],
      true,
      badEntryDefMap,
      importAliases,
      true,
      structNames,
      2,
      [](const primec::ir_lowerer::AppendStructLayoutFieldFn &appendStructLayoutField) {
        appendStructLayoutField("/pkg/MyStruct", {"value", "i32", "", false});
        appendStructLayoutField("/pkg/Container", {"slot", "uninitialized", "MyStruct", false});
      },
      setup,
      error));
  CHECK(error == "native backend only supports a single array<string> entry parameter");
}

TEST_CASE(
    "ir lowerer uninitialized type helpers build bundled entry return, runtime entry, and setup resolution") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;
  primec::Program program;

  primec::Definition handlerDef;
  handlerDef.fullPath = "/handler";
  handlerDef.namespacePrefix = "";
  program.definitions.push_back(handlerDef);

  primec::Definition calleeDef;
  calleeDef.fullPath = "/callee";
  calleeDef.namespacePrefix = "";
  program.definitions.push_back(calleeDef);

  primec::Definition containerDef;
  containerDef.fullPath = "/pkg/Container";
  containerDef.namespacePrefix = "/pkg";
  program.definitions.push_back(containerDef);

  primec::Definition myStructDef;
  myStructDef.fullPath = "/pkg/MyStruct";
  myStructDef.namespacePrefix = "/pkg";
  program.definitions.push_back(myStructDef);

  primec::Definition entryDef;
  entryDef.fullPath = "/main";
  entryDef.namespacePrefix = "";
  primec::Expr entryParam;
  entryParam.name = "argv";
  primec::Transform arrayTransform;
  arrayTransform.name = "array";
  arrayTransform.templateArgs = {"string"};
  entryParam.transforms.push_back(arrayTransform);
  entryDef.parameters.push_back(entryParam);

  primec::Transform returnTransform;
  returnTransform.name = "return";
  returnTransform.templateArgs = {"Result<i64, FileError>"};
  entryDef.transforms.push_back(returnTransform);

  primec::Transform onError;
  onError.name = "on_error";
  onError.templateArgs = {"FileError", "handler"};
  onError.arguments = {"1i32"};
  entryDef.transforms.push_back(onError);

  primec::Expr tailCall;
  tailCall.kind = primec::Expr::Kind::Call;
  tailCall.name = "callee";
  entryDef.statements = {tailCall};
  program.definitions.push_back(entryDef);

  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/handler", &program.definitions[0]},
      {"/callee", &program.definitions[1]},
      {"/pkg/Container", &program.definitions[2]},
      {"/pkg/MyStruct", &program.definitions[3]},
      {"/main", &program.definitions[4]},
  };
  const std::unordered_map<std::string, std::string> importAliases = {{"MyStruct", "/pkg/MyStruct"}};
  const std::unordered_set<std::string> structNames = {"/pkg/Container", "/pkg/MyStruct"};

  primec::IrFunction function;
  std::vector<std::string> stringTable;
  std::string error;
  primec::ir_lowerer::EntryReturnRuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup setup;
  REQUIRE(primec::ir_lowerer::buildEntryReturnRuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup(
      stringTable,
      function,
      program,
      program.definitions[4],
      "/main",
      defMap,
      importAliases,
      true,
      structNames,
      2,
      [](const primec::ir_lowerer::AppendStructLayoutFieldFn &appendStructLayoutField) {
        appendStructLayoutField("/pkg/MyStruct", {"value", "i32", "", false});
        appendStructLayoutField("/pkg/Container", {"slot", "uninitialized", "MyStruct", false});
      },
      setup,
      error));
  CHECK(error.empty());
  CHECK(setup.entryReturnConfig.hasReturnTransform);
  CHECK_FALSE(setup.entryReturnConfig.returnsVoid);
  CHECK(setup.entryReturnConfig.hasResultInfo);
  CHECK(setup.entryReturnConfig.resultInfo.isResult);

  CHECK(setup.runtimeEntrySetupMathTypeStructAndUninitializedResolutionSetup
            .entrySetupMathTypeStructAndUninitializedResolutionSetup.entryCountCallOnErrorSetup
            .countAccessSetup.hasEntryArgs);
  CHECK(setup.runtimeEntrySetupMathTypeStructAndUninitializedResolutionSetup
            .entrySetupMathTypeStructAndUninitializedResolutionSetup
            .setupMathTypeStructAndUninitializedResolutionSetup
            .setupTypeStructAndUninitializedResolutionSetup
            .setupTypeAndStructTypeAdapters.valueKindFromTypeName("i32") == ValueKind::Int32);

  CHECK(setup.runtimeEntrySetupMathTypeStructAndUninitializedResolutionSetup
            .runtimeErrorAndStringLiteralSetup.stringLiteralHelpers.internString("hello") == 0);
  setup.runtimeEntrySetupMathTypeStructAndUninitializedResolutionSetup
      .runtimeErrorAndStringLiteralSetup.runtimeErrorEmitters.emitArrayIndexOutOfBounds();
  REQUIRE(function.instructions.size() == 3);
  CHECK(primec::decodePrintStringIndex(function.instructions[0].imm) == 1);

  primec::Program badProgram = program;
  badProgram.definitions[4].transforms.clear();
  primec::Transform badReturn;
  badReturn.name = "return";
  badReturn.templateArgs = {"array<string>"};
  badProgram.definitions[4].transforms.push_back(badReturn);
  const std::unordered_map<std::string, const primec::Definition *> badDefMap = {
      {"/handler", &badProgram.definitions[0]},
      {"/callee", &badProgram.definitions[1]},
      {"/pkg/Container", &badProgram.definitions[2]},
      {"/pkg/MyStruct", &badProgram.definitions[3]},
      {"/main", &badProgram.definitions[4]},
  };
  CHECK_FALSE(primec::ir_lowerer::buildEntryReturnRuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup(
      stringTable,
      function,
      badProgram,
      badProgram.definitions[4],
      "/main",
      badDefMap,
      importAliases,
      true,
      structNames,
      2,
      [](const primec::ir_lowerer::AppendStructLayoutFieldFn &appendStructLayoutField) {
        appendStructLayoutField("/pkg/MyStruct", {"value", "i32", "", false});
        appendStructLayoutField("/pkg/Container", {"slot", "uninitialized", "MyStruct", false});
      },
      setup,
      error));
  CHECK(error == "native backend does not support string array return types on /main");
}

TEST_CASE(
    "ir lowerer uninitialized type helpers build bundled program entry return runtime and setup resolution") {
  primec::Program program;
  program.imports.push_back("/std/math/*");

  primec::Definition handlerDef;
  handlerDef.fullPath = "/handler";
  handlerDef.namespacePrefix = "";
  program.definitions.push_back(handlerDef);

  primec::Definition calleeDef;
  calleeDef.fullPath = "/callee";
  calleeDef.namespacePrefix = "";
  program.definitions.push_back(calleeDef);

  primec::Definition containerDef;
  containerDef.fullPath = "/pkg/Container";
  containerDef.namespacePrefix = "/pkg";
  program.definitions.push_back(containerDef);

  primec::Definition myStructDef;
  myStructDef.fullPath = "/pkg/MyStruct";
  myStructDef.namespacePrefix = "/pkg";
  program.definitions.push_back(myStructDef);

  primec::Definition entryDef;
  entryDef.fullPath = "/main";
  entryDef.namespacePrefix = "";
  primec::Expr entryParam;
  entryParam.name = "argv";
  primec::Transform arrayTransform;
  arrayTransform.name = "array";
  arrayTransform.templateArgs = {"string"};
  entryParam.transforms.push_back(arrayTransform);
  entryDef.parameters.push_back(entryParam);
  program.definitions.push_back(entryDef);

  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/handler", &program.definitions[0]},
      {"/callee", &program.definitions[1]},
      {"/pkg/Container", &program.definitions[2]},
      {"/pkg/MyStruct", &program.definitions[3]},
      {"/main", &program.definitions[4]},
  };
  const std::unordered_map<std::string, std::string> importAliases = {{"MyStruct", "/pkg/MyStruct"}};
  const std::unordered_set<std::string> structNames = {"/pkg/Container", "/pkg/MyStruct"};

  primec::IrFunction function;
  std::vector<std::string> stringTable;
  std::string error;
  primec::ir_lowerer::EntryReturnRuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup setupWithMath;
  REQUIRE(primec::ir_lowerer::buildProgramEntryReturnRuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup(
      stringTable,
      function,
      program,
      program.definitions[4],
      "/main",
      defMap,
      importAliases,
      structNames,
      2,
      [](const primec::ir_lowerer::AppendStructLayoutFieldFn &appendStructLayoutField) {
        appendStructLayoutField("/pkg/MyStruct", {"value", "i32", "", false});
        appendStructLayoutField("/pkg/Container", {"slot", "uninitialized", "MyStruct", false});
      },
      setupWithMath,
      error));
  CHECK(error.empty());
  CHECK(setupWithMath.entryReturnConfig.returnsVoid);

  primec::Expr mathCall;
  mathCall.kind = primec::Expr::Kind::Call;
  mathCall.name = "sin";
  std::string builtinName;
  CHECK(setupWithMath.runtimeEntrySetupMathTypeStructAndUninitializedResolutionSetup
            .entrySetupMathTypeStructAndUninitializedResolutionSetup
            .setupMathTypeStructAndUninitializedResolutionSetup
            .setupMathAndBindingAdapters.setupMathResolvers.getMathBuiltinName(mathCall, builtinName));
  CHECK(builtinName == "sin");

  primec::Program noMathProgram = program;
  noMathProgram.imports.clear();
  const std::unordered_map<std::string, const primec::Definition *> noMathDefMap = {
      {"/handler", &noMathProgram.definitions[0]},
      {"/callee", &noMathProgram.definitions[1]},
      {"/pkg/Container", &noMathProgram.definitions[2]},
      {"/pkg/MyStruct", &noMathProgram.definitions[3]},
      {"/main", &noMathProgram.definitions[4]},
  };
  primec::ir_lowerer::EntryReturnRuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup setupNoMath;
  REQUIRE(primec::ir_lowerer::buildProgramEntryReturnRuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup(
      stringTable,
      function,
      noMathProgram,
      noMathProgram.definitions[4],
      "/main",
      noMathDefMap,
      importAliases,
      structNames,
      2,
      [](const primec::ir_lowerer::AppendStructLayoutFieldFn &appendStructLayoutField) {
        appendStructLayoutField("/pkg/MyStruct", {"value", "i32", "", false});
        appendStructLayoutField("/pkg/Container", {"slot", "uninitialized", "MyStruct", false});
      },
      setupNoMath,
      error));
  CHECK_FALSE(setupNoMath.runtimeEntrySetupMathTypeStructAndUninitializedResolutionSetup
                  .entrySetupMathTypeStructAndUninitializedResolutionSetup
                  .setupMathTypeStructAndUninitializedResolutionSetup
                  .setupMathAndBindingAdapters.setupMathResolvers.getMathBuiltinName(mathCall, builtinName));

  primec::Program badProgram = program;
  primec::Transform badReturn;
  badReturn.name = "return";
  badReturn.templateArgs = {"array<string>"};
  badProgram.definitions[4].transforms.push_back(badReturn);
  const std::unordered_map<std::string, const primec::Definition *> badDefMap = {
      {"/handler", &badProgram.definitions[0]},
      {"/callee", &badProgram.definitions[1]},
      {"/pkg/Container", &badProgram.definitions[2]},
      {"/pkg/MyStruct", &badProgram.definitions[3]},
      {"/main", &badProgram.definitions[4]},
  };
  CHECK_FALSE(primec::ir_lowerer::buildProgramEntryReturnRuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup(
      stringTable,
      function,
      badProgram,
      badProgram.definitions[4],
      "/main",
      badDefMap,
      importAliases,
      structNames,
      2,
      [](const primec::ir_lowerer::AppendStructLayoutFieldFn &appendStructLayoutField) {
        appendStructLayoutField("/pkg/MyStruct", {"value", "i32", "", false});
        appendStructLayoutField("/pkg/Container", {"slot", "uninitialized", "MyStruct", false});
      },
      setupWithMath,
      error));
  CHECK(error == "native backend does not support string array return types on /main");
}

TEST_CASE("ir lowerer on_error helpers reject unknown handler") {
  primec::Transform onError;
  onError.name = "on_error";
  onError.templateArgs = {"FileError", "missing"};
  const std::vector<primec::Transform> transforms = {onError};

  std::optional<primec::ir_lowerer::OnErrorHandler> out;
  std::string error;
  CHECK_FALSE(primec::ir_lowerer::parseOnErrorTransform(
      transforms,
      "",
      "/main",
      [](const primec::Expr &expr) { return std::string("/") + expr.name; },
      [](const std::string &) { return false; },
      out,
      error));
  CHECK(error == "unknown on_error handler: /missing");
}

TEST_CASE("ir lowerer setup type helper maps primitive aliases") {
  CHECK(primec::ir_lowerer::valueKindFromTypeName("int") == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  CHECK(primec::ir_lowerer::valueKindFromTypeName("i32") == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  CHECK(primec::ir_lowerer::valueKindFromTypeName("float") == primec::ir_lowerer::LocalInfo::ValueKind::Float32);
  CHECK(primec::ir_lowerer::valueKindFromTypeName("f64") == primec::ir_lowerer::LocalInfo::ValueKind::Float64);
  CHECK(primec::ir_lowerer::valueKindFromTypeName("bool") == primec::ir_lowerer::LocalInfo::ValueKind::Bool);
  CHECK(primec::ir_lowerer::valueKindFromTypeName("string") == primec::ir_lowerer::LocalInfo::ValueKind::String);
}

TEST_CASE("ir lowerer setup type helper maps file and fileerror types") {
  CHECK(primec::ir_lowerer::valueKindFromTypeName("FileError") == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  CHECK(primec::ir_lowerer::valueKindFromTypeName("File<Read>") == primec::ir_lowerer::LocalInfo::ValueKind::Int64);
  CHECK(primec::ir_lowerer::valueKindFromTypeName("File<Write>") == primec::ir_lowerer::LocalInfo::ValueKind::Int64);
}

TEST_CASE("ir lowerer setup type helper returns unknown for unsupported names") {
  CHECK(primec::ir_lowerer::valueKindFromTypeName("Vec3") == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
  CHECK(primec::ir_lowerer::valueKindFromTypeName("Result<FileError>") ==
        primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
}

TEST_CASE("ir lowerer setup type helper maps value kinds to type names") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;
  CHECK(primec::ir_lowerer::typeNameForValueKind(ValueKind::Int32) == "i32");
  CHECK(primec::ir_lowerer::typeNameForValueKind(ValueKind::Int64) == "i64");
  CHECK(primec::ir_lowerer::typeNameForValueKind(ValueKind::UInt64) == "u64");
  CHECK(primec::ir_lowerer::typeNameForValueKind(ValueKind::Float32) == "f32");
  CHECK(primec::ir_lowerer::typeNameForValueKind(ValueKind::Float64) == "f64");
  CHECK(primec::ir_lowerer::typeNameForValueKind(ValueKind::Bool) == "bool");
  CHECK(primec::ir_lowerer::typeNameForValueKind(ValueKind::String) == "string");
}

TEST_CASE("ir lowerer setup type helper returns empty name for unknown kind") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;
  CHECK(primec::ir_lowerer::typeNameForValueKind(ValueKind::Unknown).empty());
}

TEST_CASE("ir lowerer setup type helper resolves method receiver local targets") {
  using LocalInfo = primec::ir_lowerer::LocalInfo;

  std::string typeName;
  std::string resolvedTypePath;

  LocalInfo arrayLocal;
  arrayLocal.kind = LocalInfo::Kind::Array;
  CHECK(primec::ir_lowerer::resolveMethodReceiverTypeFromLocalInfo(arrayLocal, typeName, resolvedTypePath));
  CHECK(typeName == "array");
  CHECK(resolvedTypePath.empty());

  LocalInfo structArrayLocal;
  structArrayLocal.kind = LocalInfo::Kind::Array;
  structArrayLocal.structTypeName = "/pkg/Vec3";
  CHECK(primec::ir_lowerer::resolveMethodReceiverTypeFromLocalInfo(structArrayLocal, typeName, resolvedTypePath));
  CHECK(typeName.empty());
  CHECK(resolvedTypePath == "/pkg/Vec3");

  LocalInfo vectorLocal;
  vectorLocal.kind = LocalInfo::Kind::Vector;
  CHECK(primec::ir_lowerer::resolveMethodReceiverTypeFromLocalInfo(vectorLocal, typeName, resolvedTypePath));
  CHECK(typeName == "vector");
  CHECK(resolvedTypePath.empty());

  LocalInfo mapLocal;
  mapLocal.kind = LocalInfo::Kind::Map;
  CHECK(primec::ir_lowerer::resolveMethodReceiverTypeFromLocalInfo(mapLocal, typeName, resolvedTypePath));
  CHECK(typeName == "map");
  CHECK(resolvedTypePath.empty());

  LocalInfo referenceArrayLocal;
  referenceArrayLocal.kind = LocalInfo::Kind::Reference;
  referenceArrayLocal.referenceToArray = true;
  CHECK(primec::ir_lowerer::resolveMethodReceiverTypeFromLocalInfo(referenceArrayLocal, typeName, resolvedTypePath));
  CHECK(typeName == "array");
  CHECK(resolvedTypePath.empty());

  LocalInfo valueLocal;
  valueLocal.kind = LocalInfo::Kind::Value;
  valueLocal.valueKind = LocalInfo::ValueKind::Int64;
  CHECK(primec::ir_lowerer::resolveMethodReceiverTypeFromLocalInfo(valueLocal, typeName, resolvedTypePath));
  CHECK(typeName == "i64");
  CHECK(resolvedTypePath.empty());

  LocalInfo unknownValueLocal;
  unknownValueLocal.kind = LocalInfo::Kind::Value;
  unknownValueLocal.valueKind = LocalInfo::ValueKind::Unknown;
  CHECK(primec::ir_lowerer::resolveMethodReceiverTypeFromLocalInfo(
      unknownValueLocal, typeName, resolvedTypePath));
  CHECK(typeName.empty());
  CHECK(resolvedTypePath.empty());
}

TEST_CASE("ir lowerer setup type helper rejects pointer and non-array reference receivers") {
  using LocalInfo = primec::ir_lowerer::LocalInfo;

  std::string typeName = "stale";
  std::string resolvedTypePath = "stale";

  LocalInfo pointerLocal;
  pointerLocal.kind = LocalInfo::Kind::Pointer;
  CHECK_FALSE(primec::ir_lowerer::resolveMethodReceiverTypeFromLocalInfo(
      pointerLocal, typeName, resolvedTypePath));
  CHECK(typeName.empty());
  CHECK(resolvedTypePath.empty());

  typeName = "stale";
  resolvedTypePath = "stale";
  LocalInfo referenceLocal;
  referenceLocal.kind = LocalInfo::Kind::Reference;
  CHECK_FALSE(primec::ir_lowerer::resolveMethodReceiverTypeFromLocalInfo(
      referenceLocal, typeName, resolvedTypePath));
  CHECK(typeName.empty());
  CHECK(resolvedTypePath.empty());
}

TEST_CASE("ir lowerer setup type helper resolves method receiver call targets") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;

  primec::Expr arrayCall;
  arrayCall.kind = primec::Expr::Kind::Call;
  arrayCall.name = "array";
  arrayCall.templateArgs = {"i32"};
  CHECK(primec::ir_lowerer::resolveMethodReceiverTypeNameFromCallExpr(arrayCall, ValueKind::Unknown) == "array");

  primec::Expr vectorCall;
  vectorCall.kind = primec::Expr::Kind::Call;
  vectorCall.name = "vector";
  vectorCall.templateArgs = {"i32"};
  CHECK(primec::ir_lowerer::resolveMethodReceiverTypeNameFromCallExpr(vectorCall, ValueKind::Unknown) ==
        "vector");

  primec::Expr mapCall;
  mapCall.kind = primec::Expr::Kind::Call;
  mapCall.name = "map";
  mapCall.templateArgs = {"i32", "i64"};
  CHECK(primec::ir_lowerer::resolveMethodReceiverTypeNameFromCallExpr(mapCall, ValueKind::Unknown) == "map");
}

TEST_CASE("ir lowerer setup type helper falls back for method receiver call targets") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;

  primec::Expr invalidArrayCall;
  invalidArrayCall.kind = primec::Expr::Kind::Call;
  invalidArrayCall.name = "array";
  invalidArrayCall.templateArgs = {};
  CHECK(primec::ir_lowerer::resolveMethodReceiverTypeNameFromCallExpr(invalidArrayCall, ValueKind::Int64) ==
        "i64");

  primec::Expr userCall;
  userCall.kind = primec::Expr::Kind::Call;
  userCall.name = "/pkg/make";
  CHECK(primec::ir_lowerer::resolveMethodReceiverTypeNameFromCallExpr(userCall, ValueKind::Float32) == "f32");
  CHECK(primec::ir_lowerer::resolveMethodReceiverTypeNameFromCallExpr(userCall, ValueKind::Unknown).empty());
}

TEST_CASE("ir lowerer setup type helper resolves method receiver struct paths from call expressions") {
  primec::Expr receiverCall;
  receiverCall.kind = primec::Expr::Kind::Call;
  receiverCall.name = "Ctor";

  const std::unordered_set<std::string> structNames = {"/pkg/Ctor", "/imports/Ctor"};
  const std::unordered_map<std::string, std::string> importAliases = {{"Ctor", "/imports/Ctor"}};

  CHECK(primec::ir_lowerer::resolveMethodReceiverStructTypePathFromCallExpr(
            receiverCall, "/pkg/Ctor", importAliases, structNames) == "/pkg/Ctor");

  CHECK(primec::ir_lowerer::resolveMethodReceiverStructTypePathFromCallExpr(
            receiverCall, "/not-struct/Ctor", importAliases, structNames) == "/imports/Ctor");
}

TEST_CASE("ir lowerer setup type helper rejects non-struct method receiver call paths") {
  primec::Expr receiverCall;
  receiverCall.kind = primec::Expr::Kind::Call;
  receiverCall.name = "Ctor";

  const std::unordered_set<std::string> structNames = {"/pkg/Ctor"};
  const std::unordered_map<std::string, std::string> importAliases = {{"Ctor", "/imports/Ctor"}};

  CHECK(primec::ir_lowerer::resolveMethodReceiverStructTypePathFromCallExpr(
            receiverCall, "/not-struct/Ctor", importAliases, structNames)
        .empty());

  receiverCall.isBinding = true;
  CHECK(primec::ir_lowerer::resolveMethodReceiverStructTypePathFromCallExpr(
            receiverCall, "/pkg/Ctor", importAliases, structNames)
        .empty());

  receiverCall.isBinding = false;
  receiverCall.isMethodCall = true;
  CHECK(primec::ir_lowerer::resolveMethodReceiverStructTypePathFromCallExpr(
            receiverCall, "/pkg/Ctor", importAliases, structNames)
        .empty());
}

TEST_CASE("ir lowerer setup type helper resolves method definitions from receiver targets") {
  primec::Definition arrayCountDef;
  arrayCountDef.fullPath = "/array/count";
  primec::Definition structMethodDef;
  structMethodDef.fullPath = "/pkg/Ctor/length";

  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/array/count", &arrayCountDef},
      {"/pkg/Ctor/length", &structMethodDef},
  };

  std::string error;
  CHECK(primec::ir_lowerer::resolveMethodDefinitionFromReceiverTarget("count", "array", "", defMap, error) ==
        &arrayCountDef);
  CHECK(error.empty());

  CHECK(primec::ir_lowerer::resolveMethodDefinitionFromReceiverTarget("length", "", "/pkg/Ctor", defMap, error) ==
        &structMethodDef);
  CHECK(error.empty());
}

TEST_CASE("ir lowerer setup type helper reports method target lookup diagnostics") {
  const std::unordered_map<std::string, const primec::Definition *> defMap;
  std::string error;

  CHECK(primec::ir_lowerer::resolveMethodDefinitionFromReceiverTarget("count", "", "", defMap, error) == nullptr);
  CHECK(error == "unknown method target for count");

  error.clear();
  CHECK(primec::ir_lowerer::resolveMethodDefinitionFromReceiverTarget("missing", "array", "", defMap, error) ==
        nullptr);
  CHECK(error == "unknown method: /array/missing");

  error.clear();
  CHECK(primec::ir_lowerer::resolveMethodDefinitionFromReceiverTarget(
            "missing", "", "/pkg/Ctor", defMap, error) == nullptr);
  CHECK(error == "unknown method: /pkg/Ctor/missing");
}

TEST_CASE("ir lowerer setup type helper resolves name receiver targets") {
  primec::Expr receiverName;
  receiverName.kind = primec::Expr::Kind::Name;
  receiverName.name = "items";

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo itemsLocal;
  itemsLocal.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  locals.emplace("items", itemsLocal);

  std::string typeName;
  std::string resolvedTypePath;
  std::string error;
  CHECK(primec::ir_lowerer::resolveMethodReceiverTypeFromNameExpr(
      receiverName, locals, "count", typeName, resolvedTypePath, error));
  CHECK(typeName == "array");
  CHECK(resolvedTypePath.empty());
  CHECK(error.empty());
}

TEST_CASE("ir lowerer setup type helper reports name receiver diagnostics") {
  primec::Expr receiverName;
  receiverName.kind = primec::Expr::Kind::Name;
  receiverName.name = "ptr";

  std::string typeName;
  std::string resolvedTypePath;
  std::string error;
  const primec::ir_lowerer::LocalMap noLocals;
  CHECK_FALSE(primec::ir_lowerer::resolveMethodReceiverTypeFromNameExpr(
      receiverName, noLocals, "count", typeName, resolvedTypePath, error));
  CHECK(error == "native backend does not know identifier: ptr");

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo pointerLocal;
  pointerLocal.kind = primec::ir_lowerer::LocalInfo::Kind::Pointer;
  locals.emplace("ptr", pointerLocal);
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::resolveMethodReceiverTypeFromNameExpr(
      receiverName, locals, "count", typeName, resolvedTypePath, error));
  CHECK(error == "unknown method target for count");
}

TEST_CASE("ir lowerer setup type helper dispatches receiver target resolution") {
  using LocalInfo = primec::ir_lowerer::LocalInfo;
  using ValueKind = LocalInfo::ValueKind;

  primec::ir_lowerer::LocalMap locals;
  LocalInfo nameLocal;
  nameLocal.kind = LocalInfo::Kind::Array;
  locals.emplace("items", nameLocal);

  primec::Expr nameReceiver;
  nameReceiver.kind = primec::Expr::Kind::Name;
  nameReceiver.name = "items";

  std::string typeName;
  std::string resolvedTypePath;
  std::string error;
  CHECK(primec::ir_lowerer::resolveMethodReceiverTarget(nameReceiver,
                                                        locals,
                                                        "count",
                                                        {},
                                                        {},
                                                        [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
                                                          return ValueKind::Unknown;
                                                        },
                                                        [](const primec::Expr &) { return std::string(); },
                                                        typeName,
                                                        resolvedTypePath,
                                                        error));
  CHECK(typeName == "array");
  CHECK(resolvedTypePath.empty());
  CHECK(error.empty());

  primec::Expr callReceiver;
  callReceiver.kind = primec::Expr::Kind::Call;
  callReceiver.name = "/pkg/make";
  CHECK(primec::ir_lowerer::resolveMethodReceiverTarget(callReceiver,
                                                        locals,
                                                        "length",
                                                        {},
                                                        {"/pkg/Ctor"},
                                                        [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
                                                          return ValueKind::Unknown;
                                                        },
                                                        [](const primec::Expr &) { return std::string("/pkg/Ctor"); },
                                                        typeName,
                                                        resolvedTypePath,
                                                        error));
  CHECK(typeName.empty());
  CHECK(resolvedTypePath == "/pkg/Ctor");
}

TEST_CASE("ir lowerer setup type helper dispatch reports missing name receiver diagnostics") {
  primec::Expr nameReceiver;
  nameReceiver.kind = primec::Expr::Kind::Name;
  nameReceiver.name = "missing";

  std::string typeName;
  std::string resolvedTypePath;
  std::string error;
  CHECK_FALSE(primec::ir_lowerer::resolveMethodReceiverTarget(nameReceiver,
                                                               {},
                                                               "count",
                                                               {},
                                                               {},
                                                               [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
                                                                 return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
                                                               },
                                                               [](const primec::Expr &) { return std::string(); },
                                                               typeName,
                                                               resolvedTypePath,
                                                               error));
  CHECK(error == "native backend does not know identifier: missing");
}

TEST_CASE("ir lowerer setup type helper selects method call receiver expression") {
  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "items";

  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.name = "count";
  methodCall.isMethodCall = true;
  methodCall.args.push_back(receiverExpr);

  const primec::Expr *receiverOut = nullptr;
  std::string error;
  CHECK(primec::ir_lowerer::resolveMethodCallReceiverExpr(
      methodCall,
      {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      receiverOut,
      error));
  REQUIRE(receiverOut != nullptr);
  CHECK(receiverOut->kind == primec::Expr::Kind::Name);
  CHECK(receiverOut->name == "items");
  CHECK(error.empty());
}

TEST_CASE("ir lowerer setup type helper skips non-method call receiver selection") {
  primec::Expr nonMethodCall;
  nonMethodCall.kind = primec::Expr::Kind::Call;
  nonMethodCall.name = "count";
  nonMethodCall.isMethodCall = false;

  const primec::Expr *receiverOut = &nonMethodCall;
  std::string error = "stale";
  CHECK_FALSE(primec::ir_lowerer::resolveMethodCallReceiverExpr(
      nonMethodCall,
      {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      receiverOut,
      error));
  CHECK(receiverOut == nullptr);
  CHECK(error == "stale");
}

TEST_CASE("ir lowerer setup type helper reports method receiver selection diagnostics") {
  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.name = "count";
  methodCall.isMethodCall = true;

  const primec::Expr *receiverOut = &methodCall;
  std::string error;
  CHECK_FALSE(primec::ir_lowerer::resolveMethodCallReceiverExpr(
      methodCall,
      {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      receiverOut,
      error));
  CHECK(receiverOut == nullptr);
  CHECK(error == "method call missing receiver");

  primec::Expr entryArgsReceiver;
  entryArgsReceiver.kind = primec::Expr::Kind::Name;
  entryArgsReceiver.name = "argv";
  methodCall.args.push_back(entryArgsReceiver);
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::resolveMethodCallReceiverExpr(
      methodCall,
      {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
        return expr.kind == primec::Expr::Kind::Name && expr.name == "argv";
      },
      receiverOut,
      error));
  CHECK(receiverOut == nullptr);
  CHECK(error == "unknown method target for count");
}

TEST_CASE("ir lowerer setup type helper bypasses array count and vector capacity method calls") {
  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "items";

  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.name = "count";
  methodCall.isMethodCall = true;
  methodCall.args.push_back(receiverExpr);

  const primec::Expr *receiverOut = &methodCall;
  std::string error = "stale";
  CHECK_FALSE(primec::ir_lowerer::resolveMethodCallReceiverExpr(
      methodCall,
      {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      receiverOut,
      error));
  CHECK(receiverOut == nullptr);
  CHECK(error == "stale");

  receiverOut = &methodCall;
  error = "stale";
  CHECK_FALSE(primec::ir_lowerer::resolveMethodCallReceiverExpr(
      methodCall,
      {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      receiverOut,
      error));
  CHECK(receiverOut == nullptr);
  CHECK(error == "stale");
}

TEST_CASE("ir lowerer setup type helper resolves method call definitions from expressions") {
  primec::Definition arrayCountDef;
  arrayCountDef.fullPath = "/array/count";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/array/count", &arrayCountDef},
  };

  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "items";

  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.name = "count";
  methodCall.isMethodCall = true;
  methodCall.args.push_back(receiverExpr);

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo itemsLocal;
  itemsLocal.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  locals.emplace("items", itemsLocal);

  std::string error;
  const primec::Definition *resolved = primec::ir_lowerer::resolveMethodCallDefinitionFromExpr(
      methodCall,
      locals,
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      {},
      {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &) { return std::string(); },
      defMap,
      error);
  CHECK(resolved == &arrayCountDef);
  CHECK(error.empty());
}

TEST_CASE("ir lowerer setup type helper resolves struct receiver method definitions from expressions") {
  primec::Definition structMethodDef;
  structMethodDef.fullPath = "/pkg/Ctor/length";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/pkg/Ctor/length", &structMethodDef},
  };

  primec::Expr receiverCall;
  receiverCall.kind = primec::Expr::Kind::Call;
  receiverCall.name = "Ctor";

  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.name = "length";
  methodCall.isMethodCall = true;
  methodCall.args.push_back(receiverCall);

  std::string error;
  const primec::Definition *resolved = primec::ir_lowerer::resolveMethodCallDefinitionFromExpr(
      methodCall,
      {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      {},
      {"/pkg/Ctor"},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &) { return std::string("/pkg/Ctor"); },
      defMap,
      error);
  CHECK(resolved == &structMethodDef);
  CHECK(error.empty());
}

TEST_CASE("ir lowerer setup type helper reports method call definition diagnostics from expressions") {
  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.name = "count";
  methodCall.isMethodCall = true;

  std::string error;
  CHECK(primec::ir_lowerer::resolveMethodCallDefinitionFromExpr(
            methodCall,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            {},
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            [](const primec::Expr &) { return std::string(); },
            {},
            error) == nullptr);
  CHECK(error == "method call missing receiver");

  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "items";
  methodCall.args.push_back(receiverExpr);
  error.clear();
  CHECK(primec::ir_lowerer::resolveMethodCallDefinitionFromExpr(
            methodCall,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            {},
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            [](const primec::Expr &) { return std::string(); },
            {},
            error) == nullptr);
  CHECK(error == "native backend does not know identifier: items");
}

TEST_CASE("ir lowerer setup type helper resolves return info kinds by path") {
  std::unordered_map<std::string, primec::ir_lowerer::ReturnInfo> infoByPath;
  primec::ir_lowerer::ReturnInfo arrayInfo;
  arrayInfo.returnsVoid = false;
  arrayInfo.returnsArray = true;
  arrayInfo.kind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  infoByPath.emplace("/defs/array", arrayInfo);

  primec::ir_lowerer::ReturnInfo scalarInfo;
  scalarInfo.returnsVoid = false;
  scalarInfo.returnsArray = false;
  scalarInfo.kind = primec::ir_lowerer::LocalInfo::ValueKind::Float64;
  infoByPath.emplace("/defs/scalar", scalarInfo);

  auto getReturnInfo = [&infoByPath](const std::string &path, primec::ir_lowerer::ReturnInfo &out) {
    auto it = infoByPath.find(path);
    if (it == infoByPath.end()) {
      return false;
    }
    out = it->second;
    return true;
  };

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(primec::ir_lowerer::resolveReturnInfoKindForPath("/defs/array", getReturnInfo, true, kindOut));
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Int64);

  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(primec::ir_lowerer::resolveReturnInfoKindForPath("/defs/scalar", getReturnInfo, false, kindOut));
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Float64);

  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK_FALSE(primec::ir_lowerer::resolveReturnInfoKindForPath("/defs/scalar", getReturnInfo, true, kindOut));
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);

  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK_FALSE(primec::ir_lowerer::resolveReturnInfoKindForPath("/defs/missing", getReturnInfo, false, kindOut));
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
}

TEST_CASE("ir lowerer setup type helper resolves method call return kinds") {
  primec::Definition methodDef;
  methodDef.fullPath = "/array/count";

  std::unordered_map<std::string, primec::ir_lowerer::ReturnInfo> infoByPath;
  primec::ir_lowerer::ReturnInfo scalarInfo;
  scalarInfo.returnsVoid = false;
  scalarInfo.returnsArray = false;
  scalarInfo.kind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  infoByPath.emplace("/array/count", scalarInfo);

  auto getReturnInfo = [&infoByPath](const std::string &path, primec::ir_lowerer::ReturnInfo &out) {
    auto it = infoByPath.find(path);
    if (it == infoByPath.end()) {
      return false;
    }
    out = it->second;
    return true;
  };

  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.name = "count";
  methodCall.isMethodCall = true;

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  bool methodResolved = false;
  CHECK(primec::ir_lowerer::resolveMethodCallReturnKind(
      methodCall,
      {},
      [&methodDef](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return &methodDef; },
      getReturnInfo,
      false,
      kindOut,
      &methodResolved));
  CHECK(methodResolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Int32);

  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  methodResolved = false;
  CHECK_FALSE(primec::ir_lowerer::resolveMethodCallReturnKind(
      methodCall,
      {},
      [&methodDef](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return &methodDef; },
      getReturnInfo,
      true,
      kindOut,
      &methodResolved));
  CHECK(methodResolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);

  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  methodResolved = true;
  CHECK_FALSE(primec::ir_lowerer::resolveMethodCallReturnKind(
      methodCall,
      {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return nullptr; },
      getReturnInfo,
      false,
      kindOut,
      &methodResolved));
  CHECK_FALSE(methodResolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
}

TEST_CASE("ir lowerer setup type helper resolves direct definition call return kinds") {
  std::unordered_map<std::string, const primec::Definition *> defMap;
  primec::Definition directDef;
  directDef.fullPath = "/pkg/value";
  defMap.emplace("/pkg/value", &directDef);

  std::unordered_map<std::string, primec::ir_lowerer::ReturnInfo> infoByPath;
  primec::ir_lowerer::ReturnInfo scalarInfo;
  scalarInfo.returnsVoid = false;
  scalarInfo.returnsArray = false;
  scalarInfo.kind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  infoByPath.emplace("/pkg/value", scalarInfo);

  auto getReturnInfo = [&infoByPath](const std::string &path, primec::ir_lowerer::ReturnInfo &out) {
    auto it = infoByPath.find(path);
    if (it == infoByPath.end()) {
      return false;
    }
    out = it->second;
    return true;
  };

  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "value";

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  bool definitionMatched = false;
  CHECK(primec::ir_lowerer::resolveDefinitionCallReturnKind(
      callExpr,
      defMap,
      [](const primec::Expr &) { return std::string("/pkg/value"); },
      getReturnInfo,
      false,
      kindOut,
      &definitionMatched));
  CHECK(definitionMatched);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Int64);

  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  definitionMatched = false;
  CHECK_FALSE(primec::ir_lowerer::resolveDefinitionCallReturnKind(
      callExpr,
      defMap,
      [](const primec::Expr &) { return std::string("/pkg/value"); },
      getReturnInfo,
      true,
      kindOut,
      &definitionMatched));
  CHECK(definitionMatched);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
}

TEST_CASE("ir lowerer setup type helper skips unmatched direct definition call return kinds") {
  std::unordered_map<std::string, const primec::Definition *> defMap;
  primec::Definition directDef;
  directDef.fullPath = "/pkg/value";
  defMap.emplace("/pkg/value", &directDef);

  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "value";

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  bool definitionMatched = true;
  CHECK_FALSE(primec::ir_lowerer::resolveDefinitionCallReturnKind(
      callExpr,
      defMap,
      [](const primec::Expr &) { return std::string("/pkg/missing"); },
      {},
      false,
      kindOut,
      &definitionMatched));
  CHECK_FALSE(definitionMatched);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);

  callExpr.isMethodCall = true;
  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  definitionMatched = true;
  CHECK_FALSE(primec::ir_lowerer::resolveDefinitionCallReturnKind(
      callExpr,
      defMap,
      [](const primec::Expr &) { return std::string("/pkg/value"); },
      {},
      false,
      kindOut,
      &definitionMatched));
  CHECK_FALSE(definitionMatched);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
}

TEST_CASE("ir lowerer setup type helper resolves count call method return kinds") {
  primec::Definition methodDef;
  methodDef.fullPath = "/array/count";

  std::unordered_map<std::string, primec::ir_lowerer::ReturnInfo> infoByPath;
  primec::ir_lowerer::ReturnInfo scalarInfo;
  scalarInfo.returnsVoid = false;
  scalarInfo.returnsArray = false;
  scalarInfo.kind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  infoByPath.emplace("/array/count", scalarInfo);

  auto getReturnInfo = [&infoByPath](const std::string &path, primec::ir_lowerer::ReturnInfo &out) {
    auto it = infoByPath.find(path);
    if (it == infoByPath.end()) {
      return false;
    }
    out = it->second;
    return true;
  };

  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "items";

  primec::Expr countCall;
  countCall.kind = primec::Expr::Kind::Call;
  countCall.name = "count";
  countCall.args.push_back(receiverExpr);

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  bool methodResolved = false;
  CHECK(primec::ir_lowerer::resolveCountMethodCallReturnKind(
      countCall,
      {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [&methodDef](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return &methodDef; },
      getReturnInfo,
      false,
      kindOut,
      &methodResolved));
  CHECK(methodResolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Int32);

  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  methodResolved = false;
  CHECK_FALSE(primec::ir_lowerer::resolveCountMethodCallReturnKind(
      countCall,
      {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [&methodDef](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return &methodDef; },
      getReturnInfo,
      true,
      kindOut,
      &methodResolved));
  CHECK(methodResolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
}

TEST_CASE("ir lowerer setup type helper skips non-eligible count call method resolution") {
  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "items";

  primec::Expr countCall;
  countCall.kind = primec::Expr::Kind::Call;
  countCall.name = "count";
  countCall.args.push_back(receiverExpr);

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  bool methodResolved = true;
  CHECK_FALSE(primec::ir_lowerer::resolveCountMethodCallReturnKind(
      countCall,
      {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return nullptr; },
      {},
      false,
      kindOut,
      &methodResolved));
  CHECK_FALSE(methodResolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);

  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  methodResolved = true;
  CHECK_FALSE(primec::ir_lowerer::resolveCountMethodCallReturnKind(
      countCall,
      {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return nullptr; },
      {},
      false,
      kindOut,
      &methodResolved));
  CHECK_FALSE(methodResolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);

  countCall.isMethodCall = true;
  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  methodResolved = true;
  CHECK_FALSE(primec::ir_lowerer::resolveCountMethodCallReturnKind(
      countCall,
      {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return nullptr; },
      {},
      false,
      kindOut,
      &methodResolved));
  CHECK_FALSE(methodResolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
}

TEST_CASE("ir lowerer return inference helper analyzes entry return transforms") {
  primec::Definition entryDef;
  primec::Transform resultReturn;
  resultReturn.name = "return";
  resultReturn.templateArgs = {"Result<i64, FileError>"};
  entryDef.transforms.push_back(resultReturn);

  primec::ir_lowerer::EntryReturnConfig out;
  std::string error;
  REQUIRE(primec::ir_lowerer::analyzeEntryReturnTransforms(entryDef, "/main", out, error));
  CHECK(error.empty());
  CHECK(out.hasReturnTransform);
  CHECK_FALSE(out.returnsVoid);
  CHECK(out.hasResultInfo);
  CHECK(out.resultInfo.isResult);
  CHECK(out.resultInfo.hasValue);
}

TEST_CASE("ir lowerer return inference helper handles void and diagnostics") {
  primec::Definition noReturnDef;
  primec::ir_lowerer::EntryReturnConfig out;
  std::string error;
  REQUIRE(primec::ir_lowerer::analyzeEntryReturnTransforms(noReturnDef, "/main", out, error));
  CHECK(out.returnsVoid);
  CHECK_FALSE(out.hasReturnTransform);

  primec::Definition invalidReturnDef;
  primec::Transform arrayStringReturn;
  arrayStringReturn.name = "return";
  arrayStringReturn.templateArgs = {"array<string>"};
  invalidReturnDef.transforms.push_back(arrayStringReturn);
  CHECK_FALSE(primec::ir_lowerer::analyzeEntryReturnTransforms(invalidReturnDef, "/main", out, error));
  CHECK(error == "native backend does not support string array return types on /main");
}

TEST_CASE("ir lowerer setup type helper combines numeric kinds") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;
  CHECK(primec::ir_lowerer::combineNumericKinds(ValueKind::Int32, ValueKind::Int32) == ValueKind::Int32);
  CHECK(primec::ir_lowerer::combineNumericKinds(ValueKind::Int32, ValueKind::Int64) == ValueKind::Int64);
  CHECK(primec::ir_lowerer::combineNumericKinds(ValueKind::UInt64, ValueKind::UInt64) == ValueKind::UInt64);
  CHECK(primec::ir_lowerer::combineNumericKinds(ValueKind::Float32, ValueKind::Float32) == ValueKind::Float32);
  CHECK(primec::ir_lowerer::combineNumericKinds(ValueKind::Float64, ValueKind::Float64) == ValueKind::Float64);
  CHECK(primec::ir_lowerer::combineNumericKinds(ValueKind::Unknown, ValueKind::Int32) == ValueKind::Unknown);
  CHECK(primec::ir_lowerer::combineNumericKinds(ValueKind::Float32, ValueKind::Float64) == ValueKind::Unknown);
  CHECK(primec::ir_lowerer::combineNumericKinds(ValueKind::String, ValueKind::Int32) == ValueKind::Unknown);
  CHECK(primec::ir_lowerer::combineNumericKinds(ValueKind::Bool, ValueKind::Int32) == ValueKind::Unknown);
}

TEST_CASE("ir lowerer setup type helper normalizes bool comparison kinds") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;
  CHECK(primec::ir_lowerer::comparisonKind(ValueKind::Bool, ValueKind::Bool) == ValueKind::Int32);
  CHECK(primec::ir_lowerer::comparisonKind(ValueKind::Bool, ValueKind::Int64) == ValueKind::Int64);
  CHECK(primec::ir_lowerer::comparisonKind(ValueKind::Int32, ValueKind::Bool) == ValueKind::Int32);
}

TEST_CASE("ir lowerer setup type helper rejects mixed bool float comparison kinds") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;
  CHECK(primec::ir_lowerer::comparisonKind(ValueKind::Bool, ValueKind::Float32) == ValueKind::Unknown);
  CHECK(primec::ir_lowerer::comparisonKind(ValueKind::Float64, ValueKind::Bool) == ValueKind::Unknown);
}

TEST_CASE("ir lowerer setup type helper builds value-kind adapters") {
  auto valueKindFromTypeName = primec::ir_lowerer::makeValueKindFromTypeName();
  auto combineNumericKinds = primec::ir_lowerer::makeCombineNumericKinds();
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;

  CHECK(valueKindFromTypeName("i64") == ValueKind::Int64);
  CHECK(valueKindFromTypeName("Vec3") == ValueKind::Unknown);
  CHECK(combineNumericKinds(ValueKind::Int32, ValueKind::Int64) == ValueKind::Int64);
  CHECK(combineNumericKinds(ValueKind::Float32, ValueKind::Float64) == ValueKind::Unknown);
}

TEST_CASE("ir lowerer setup type helper builds bundled adapters") {
  auto adapters = primec::ir_lowerer::makeSetupTypeAdapters();
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;

  CHECK(adapters.valueKindFromTypeName("i64") == ValueKind::Int64);
  CHECK(adapters.valueKindFromTypeName("Vec3") == ValueKind::Unknown);
  CHECK(adapters.combineNumericKinds(ValueKind::Int32, ValueKind::Int64) == ValueKind::Int64);
  CHECK(adapters.combineNumericKinds(ValueKind::Float32, ValueKind::Float64) == ValueKind::Unknown);
}

TEST_CASE("ir lowerer setup math helper detects math imports") {
  CHECK(primec::ir_lowerer::isMathImportPath("/std/math/*"));
  CHECK(primec::ir_lowerer::isMathImportPath("/std/math/sin"));
  CHECK_FALSE(primec::ir_lowerer::isMathImportPath("/std"));
  CHECK_FALSE(primec::ir_lowerer::isMathImportPath("/std/math"));

  const std::vector<std::string> importsA = {"/std/io/*", "/std/math/*"};
  CHECK(primec::ir_lowerer::hasProgramMathImport(importsA));

  const std::vector<std::string> importsB = {"/std/io/*", "/std/math/sin"};
  CHECK(primec::ir_lowerer::hasProgramMathImport(importsB));

  const std::vector<std::string> importsC = {"/std/io/*", "/pkg/demo"};
  CHECK_FALSE(primec::ir_lowerer::hasProgramMathImport(importsC));
}

TEST_CASE("ir lowerer struct type helpers join template args text") {
  const std::vector<std::string> emptyArgs;
  CHECK(primec::ir_lowerer::joinTemplateArgsText(emptyArgs).empty());

  const std::vector<std::string> oneArg = {"i64"};
  CHECK(primec::ir_lowerer::joinTemplateArgsText(oneArg) == "i64");

  const std::vector<std::string> manyArgs = {"i64", "vector<f32>", "map<i32, bool>"};
  CHECK(primec::ir_lowerer::joinTemplateArgsText(manyArgs) == "i64, vector<f32>, map<i32, bool>");
}

TEST_CASE("ir lowerer struct type helpers resolve scoped struct paths") {
  const std::unordered_set<std::string> structNames = {"/pkg/Foo", "/imports/Bar", "/Baz"};
  const std::unordered_map<std::string, std::string> importAliases = {{"Bar", "/imports/Bar"}};
  std::string resolved;

  REQUIRE(primec::ir_lowerer::resolveStructTypePathFromScope(
      "/pkg/Foo", "", structNames, importAliases, resolved));
  CHECK(resolved == "/pkg/Foo");

  REQUIRE(primec::ir_lowerer::resolveStructTypePathFromScope(
      "Foo", "/pkg", structNames, importAliases, resolved));
  CHECK(resolved == "/pkg/Foo");

  REQUIRE(primec::ir_lowerer::resolveStructTypePathFromScope(
      "Bar", "/pkg", structNames, importAliases, resolved));
  CHECK(resolved == "/imports/Bar");

  REQUIRE(primec::ir_lowerer::resolveStructTypePathFromScope(
      "Baz", "", structNames, importAliases, resolved));
  CHECK(resolved == "/Baz");

  CHECK_FALSE(primec::ir_lowerer::resolveStructTypePathFromScope(
      "Missing", "/pkg", structNames, importAliases, resolved));
}

TEST_CASE("ir lowerer struct type helpers build scoped struct path resolver") {
  const std::unordered_set<std::string> structNames = {"/pkg/Foo", "/imports/Bar", "/Baz"};
  const std::unordered_map<std::string, std::string> importAliases = {{"Bar", "/imports/Bar"}};
  auto resolveStructTypePath =
      primec::ir_lowerer::makeResolveStructTypePathFromScope(structNames, importAliases);

  std::string resolved;
  REQUIRE(resolveStructTypePath("Foo", "/pkg", resolved));
  CHECK(resolved == "/pkg/Foo");
  REQUIRE(resolveStructTypePath("Bar", "/pkg", resolved));
  CHECK(resolved == "/imports/Bar");
  REQUIRE(resolveStructTypePath("Baz", "", resolved));
  CHECK(resolved == "/Baz");
  CHECK_FALSE(resolveStructTypePath("Missing", "/pkg", resolved));
}

TEST_CASE("ir lowerer struct type helpers resolve definition namespace prefixes from map") {
  primec::Definition namespacedDef;
  namespacedDef.fullPath = "/pkg/Foo";
  namespacedDef.namespacePrefix = "/pkg";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/pkg/Foo", &namespacedDef},
      {"/pkg/Null", nullptr},
  };

  std::string namespacePrefixOut;
  REQUIRE(primec::ir_lowerer::resolveDefinitionNamespacePrefixFromMap(
      defMap, "/pkg/Foo", namespacePrefixOut));
  CHECK(namespacePrefixOut == "/pkg");

  CHECK_FALSE(primec::ir_lowerer::resolveDefinitionNamespacePrefixFromMap(
      defMap, "/pkg/Missing", namespacePrefixOut));
  CHECK_FALSE(primec::ir_lowerer::resolveDefinitionNamespacePrefixFromMap(
      defMap, "/pkg/Null", namespacePrefixOut));
}

TEST_CASE("ir lowerer struct type helpers infer struct return paths") {
  auto resolveStruct = [](const std::string &typeName,
                          const std::string &namespacePrefix,
                          std::string &resolvedOut) {
    if (namespacePrefix != "/pkg") {
      return false;
    }
    if (typeName == "Foo") {
      resolvedOut = "/pkg/Foo";
      return true;
    }
    if (typeName == "Bar") {
      resolvedOut = "/pkg/Bar";
      return true;
    }
    return false;
  };
  auto inferExpr = [](const primec::Expr &expr) {
    if (expr.kind == primec::Expr::Kind::Call && expr.name == "factory") {
      return std::string("/pkg/FromExpr");
    }
    if (expr.kind == primec::Expr::Kind::Call && expr.name == "mk") {
      return std::string("/pkg/FromStmt");
    }
    return std::string();
  };

  primec::Definition returnTransformDef;
  returnTransformDef.namespacePrefix = "/pkg";
  primec::Transform returnTransform;
  returnTransform.name = "return";
  returnTransform.templateArgs = {"Foo"};
  returnTransformDef.transforms.push_back(returnTransform);
  CHECK(primec::ir_lowerer::inferStructReturnPathFromDefinition(returnTransformDef, resolveStruct, inferExpr) ==
        "/pkg/Foo");

  primec::Definition namedTransformDef;
  namedTransformDef.namespacePrefix = "/pkg";
  primec::Transform qualifier;
  qualifier.name = "public";
  namedTransformDef.transforms.push_back(qualifier);
  primec::Transform namedType;
  namedType.name = "Bar";
  namedTransformDef.transforms.push_back(namedType);
  CHECK(primec::ir_lowerer::inferStructReturnPathFromDefinition(namedTransformDef, resolveStruct, inferExpr) ==
        "/pkg/Bar");

  primec::Definition returnExprDef;
  primec::Expr returnExpr;
  returnExpr.kind = primec::Expr::Kind::Call;
  returnExpr.name = "factory";
  returnExprDef.returnExpr = returnExpr;
  CHECK(primec::ir_lowerer::inferStructReturnPathFromDefinition(returnExprDef, resolveStruct, inferExpr) ==
        "/pkg/FromExpr");

  primec::Definition returnStmtDef;
  primec::Expr nestedCall;
  nestedCall.kind = primec::Expr::Kind::Call;
  nestedCall.name = "mk";
  primec::Expr returnStmt;
  returnStmt.kind = primec::Expr::Kind::Call;
  returnStmt.name = "return";
  returnStmt.args = {nestedCall};
  returnStmtDef.statements.push_back(returnStmt);
  CHECK(primec::ir_lowerer::inferStructReturnPathFromDefinition(returnStmtDef, resolveStruct, inferExpr) ==
        "/pkg/FromStmt");

  primec::Definition unresolvedDef;
  unresolvedDef.namespacePrefix = "/pkg";
  primec::Transform unresolvedReturn;
  unresolvedReturn.name = "return";
  unresolvedReturn.templateArgs = {"Missing"};
  unresolvedDef.transforms.push_back(unresolvedReturn);
  CHECK(primec::ir_lowerer::inferStructReturnPathFromDefinition(unresolvedDef, resolveStruct, inferExpr).empty());
}

TEST_CASE("ir lowerer struct type helpers infer call target struct paths") {
  auto resolveExprPath = [](const primec::Expr &expr) {
    if (expr.name == "Ctor") {
      return std::string("/pkg/Ctor");
    }
    if (expr.name == "factory") {
      return std::string("/pkg/factory");
    }
    return std::string("/pkg/unknown");
  };
  auto isKnownStructPath = [](const std::string &path) { return path == "/pkg/Ctor"; };
  auto inferDefinitionStructReturnPath = [](const std::string &path) {
    if (path == "/pkg/factory") {
      return std::string("/pkg/FromDef");
    }
    return std::string();
  };

  primec::Expr ctorCall;
  ctorCall.kind = primec::Expr::Kind::Call;
  ctorCall.name = "Ctor";
  CHECK(primec::ir_lowerer::inferStructPathFromCallTarget(
            ctorCall, resolveExprPath, isKnownStructPath, inferDefinitionStructReturnPath) == "/pkg/Ctor");

  primec::Expr factoryCall;
  factoryCall.kind = primec::Expr::Kind::Call;
  factoryCall.name = "factory";
  CHECK(primec::ir_lowerer::inferStructPathFromCallTarget(
            factoryCall, resolveExprPath, isKnownStructPath, inferDefinitionStructReturnPath) == "/pkg/FromDef");

  primec::Expr unknownCall;
  unknownCall.kind = primec::Expr::Kind::Call;
  unknownCall.name = "unknown";
  CHECK(primec::ir_lowerer::inferStructPathFromCallTarget(
            unknownCall, resolveExprPath, isKnownStructPath, inferDefinitionStructReturnPath).empty());

  primec::Expr methodCall = ctorCall;
  methodCall.isMethodCall = true;
  CHECK(primec::ir_lowerer::inferStructPathFromCallTarget(
            methodCall, resolveExprPath, isKnownStructPath, inferDefinitionStructReturnPath).empty());

  primec::Expr fieldAccessCall = ctorCall;
  fieldAccessCall.isFieldAccess = true;
  CHECK(primec::ir_lowerer::inferStructPathFromCallTarget(
            fieldAccessCall, resolveExprPath, isKnownStructPath, inferDefinitionStructReturnPath).empty());

  primec::Expr nameExpr;
  nameExpr.kind = primec::Expr::Kind::Name;
  nameExpr.name = "Ctor";
  CHECK(primec::ir_lowerer::inferStructPathFromCallTarget(
            nameExpr, resolveExprPath, isKnownStructPath, inferDefinitionStructReturnPath).empty());
}

TEST_CASE("ir lowerer struct type helpers infer name-expression struct paths") {
  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo structInfo;
  structInfo.structTypeName = "/pkg/Point";
  locals.emplace("point", structInfo);

  primec::Expr nameExpr;
  nameExpr.kind = primec::Expr::Kind::Name;
  nameExpr.name = "point";
  CHECK(primec::ir_lowerer::inferStructPathFromNameExpr(nameExpr, locals) == "/pkg/Point");

  nameExpr.name = "missing";
  CHECK(primec::ir_lowerer::inferStructPathFromNameExpr(nameExpr, locals).empty());

  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "point";
  CHECK(primec::ir_lowerer::inferStructPathFromNameExpr(callExpr, locals).empty());
}

TEST_CASE("ir lowerer struct type helpers infer field-access struct paths") {
  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo receiverInfo;
  receiverInfo.structTypeName = "/pkg/Container";
  locals.emplace("self", receiverInfo);

  auto inferStructExprPath = [](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &localsIn) {
    if (expr.kind != primec::Expr::Kind::Name) {
      return std::string();
    }
    auto it = localsIn.find(expr.name);
    if (it == localsIn.end()) {
      return std::string();
    }
    return it->second.structTypeName;
  };
  auto resolveStructFieldSlot =
      [](const std::string &structPath, const std::string &fieldName, primec::ir_lowerer::StructSlotFieldInfo &out) {
        if (structPath == "/pkg/Container" && fieldName == "nested") {
          out.structPath = "/pkg/Nested";
          return true;
        }
        return false;
      };

  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "self";

  primec::Expr fieldAccess;
  fieldAccess.kind = primec::Expr::Kind::Call;
  fieldAccess.isFieldAccess = true;
  fieldAccess.name = "nested";
  fieldAccess.args = {receiverExpr};
  CHECK(primec::ir_lowerer::inferStructPathFromFieldAccessCall(
            fieldAccess, locals, inferStructExprPath, resolveStructFieldSlot) == "/pkg/Nested");

  primec::Expr missingReceiver = fieldAccess;
  missingReceiver.args.front().name = "missing";
  CHECK(primec::ir_lowerer::inferStructPathFromFieldAccessCall(
            missingReceiver, locals, inferStructExprPath, resolveStructFieldSlot).empty());

  primec::Expr missingField = fieldAccess;
  missingField.name = "missing";
  CHECK(primec::ir_lowerer::inferStructPathFromFieldAccessCall(
            missingField, locals, inferStructExprPath, resolveStructFieldSlot).empty());

  primec::Expr notFieldAccess = fieldAccess;
  notFieldAccess.isFieldAccess = false;
  CHECK(primec::ir_lowerer::inferStructPathFromFieldAccessCall(
            notFieldAccess, locals, inferStructExprPath, resolveStructFieldSlot).empty());
}

TEST_CASE("ir lowerer struct type helpers build layout field index and collect fields") {
  const primec::ir_lowerer::StructLayoutFieldIndex fieldIndex =
      primec::ir_lowerer::buildStructLayoutFieldIndex(
          2,
          [](const primec::ir_lowerer::AppendStructLayoutFieldFn &appendStructLayoutField) {
            appendStructLayoutField("/pkg/Foo", {"a", "i32", "", false});
            appendStructLayoutField("/pkg/Foo", {"b", "array<i32>", "", true});
            appendStructLayoutField("/pkg/Bar", {"x", "f64", "", false});
          });
  REQUIRE(fieldIndex.size() == 2);

  std::vector<primec::ir_lowerer::StructLayoutFieldInfo> layoutFields;
  REQUIRE(primec::ir_lowerer::collectStructLayoutFieldsFromIndex(fieldIndex, "/pkg/Foo", layoutFields));
  REQUIRE(layoutFields.size() == 2);
  CHECK(layoutFields[0].name == "a");
  CHECK(layoutFields[1].name == "b");
  CHECK(layoutFields[1].isStatic);

  std::vector<primec::ir_lowerer::StructArrayFieldInfo> arrayFields;
  REQUIRE(primec::ir_lowerer::collectStructArrayFieldsFromLayoutIndex(fieldIndex, "/pkg/Foo", arrayFields));
  REQUIRE(arrayFields.size() == 2);
  CHECK(arrayFields[0].typeName == "i32");
  CHECK(arrayFields[1].typeName == "array<i32>");
  CHECK(arrayFields[1].isStatic);

  CHECK_FALSE(primec::ir_lowerer::collectStructLayoutFieldsFromIndex(fieldIndex, "/pkg/Missing", layoutFields));
  CHECK_FALSE(primec::ir_lowerer::collectStructArrayFieldsFromLayoutIndex(fieldIndex, "/pkg/Missing", arrayFields));

  const primec::ir_lowerer::StructLayoutFieldIndex emptyIndex =
      primec::ir_lowerer::buildStructLayoutFieldIndex(1, {});
  CHECK(emptyIndex.empty());
}

TEST_CASE("ir lowerer struct type helpers resolve struct array info from path") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;
  auto valueKindFromTypeName = [](const std::string &typeName) {
    if (typeName == "i32") {
      return ValueKind::Int32;
    }
    if (typeName == "i64") {
      return ValueKind::Int64;
    }
    if (typeName == "string") {
      return ValueKind::String;
    }
    return ValueKind::Unknown;
  };

  auto collectFields = [](const std::string &structPath,
                          std::vector<primec::ir_lowerer::StructArrayFieldInfo> &out) {
    out.clear();
    if (structPath == "/pkg/Foo") {
      out.push_back({"i32", "", false});
      out.push_back({"i32", "", false});
      out.push_back({"i32", "", true});
      return true;
    }
    if (structPath == "/pkg/Mixed") {
      out.push_back({"i32", "", false});
      out.push_back({"i64", "", false});
      return true;
    }
    if (structPath == "/pkg/Templated") {
      out.push_back({"array", "i32", false});
      return true;
    }
    if (structPath == "/pkg/Strings") {
      out.push_back({"string", "", false});
      return true;
    }
    return false;
  };

  primec::ir_lowerer::StructArrayTypeInfo out;
  REQUIRE(primec::ir_lowerer::resolveStructArrayTypeInfoFromPath(
      "/pkg/Foo", collectFields, valueKindFromTypeName, out));
  CHECK(out.structPath == "/pkg/Foo");
  CHECK(out.elementKind == ValueKind::Int32);
  CHECK(out.fieldCount == 2);

  CHECK_FALSE(primec::ir_lowerer::resolveStructArrayTypeInfoFromPath(
      "/pkg/Mixed", collectFields, valueKindFromTypeName, out));
  CHECK_FALSE(primec::ir_lowerer::resolveStructArrayTypeInfoFromPath(
      "/pkg/Templated", collectFields, valueKindFromTypeName, out));
  CHECK_FALSE(primec::ir_lowerer::resolveStructArrayTypeInfoFromPath(
      "/pkg/Strings", collectFields, valueKindFromTypeName, out));
  CHECK_FALSE(primec::ir_lowerer::resolveStructArrayTypeInfoFromPath(
      "/pkg/Missing", collectFields, valueKindFromTypeName, out));
}

TEST_CASE("ir lowerer struct type helpers resolve and apply struct array info from layout field index") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;
  auto valueKindFromTypeName = [](const std::string &typeName) {
    if (typeName == "i32") {
      return ValueKind::Int32;
    }
    if (typeName == "i64") {
      return ValueKind::Int64;
    }
    if (typeName == "string") {
      return ValueKind::String;
    }
    return ValueKind::Unknown;
  };

  const primec::ir_lowerer::StructLayoutFieldIndex fieldIndex =
      primec::ir_lowerer::buildStructLayoutFieldIndex(
          2,
          [](const primec::ir_lowerer::AppendStructLayoutFieldFn &appendStructLayoutField) {
            appendStructLayoutField("/pkg/Foo", {"a", "i32", "", false});
            appendStructLayoutField("/pkg/Foo", {"b", "i32", "", false});
            appendStructLayoutField("/pkg/Templated", {"c", "array", "i32", false});
          });

  primec::ir_lowerer::StructArrayTypeInfo out;
  REQUIRE(primec::ir_lowerer::resolveStructArrayTypeInfoFromLayoutFieldIndex(
      "/pkg/Foo", fieldIndex, valueKindFromTypeName, out));
  CHECK(out.structPath == "/pkg/Foo");
  CHECK(out.elementKind == ValueKind::Int32);
  CHECK(out.fieldCount == 2);
  CHECK_FALSE(primec::ir_lowerer::resolveStructArrayTypeInfoFromLayoutFieldIndex(
      "/pkg/Templated", fieldIndex, valueKindFromTypeName, out));
  CHECK_FALSE(primec::ir_lowerer::resolveStructArrayTypeInfoFromLayoutFieldIndex(
      "/pkg/Missing", fieldIndex, valueKindFromTypeName, out));

  auto resolveStructTypeName = [](const std::string &typeName,
                                  const std::string &namespacePrefix,
                                  std::string &resolvedOut) {
    if (typeName == "Foo" && namespacePrefix == "/pkg") {
      resolvedOut = "/pkg/Foo";
      return true;
    }
    return false;
  };

  primec::Expr expr;
  expr.namespacePrefix = "/pkg";
  primec::Transform typed;
  typed.name = "Foo";
  expr.transforms.push_back(typed);

  REQUIRE(primec::ir_lowerer::resolveStructArrayTypeInfoFromBindingWithLayoutFieldIndex(
      expr, resolveStructTypeName, fieldIndex, valueKindFromTypeName, out));
  CHECK(out.structPath == "/pkg/Foo");

  primec::ir_lowerer::LocalInfo info;
  info.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  info.valueKind = ValueKind::Unknown;
  primec::ir_lowerer::applyStructArrayInfoFromBindingWithLayoutFieldIndex(
      expr, resolveStructTypeName, fieldIndex, valueKindFromTypeName, info);
  CHECK(info.kind == primec::ir_lowerer::LocalInfo::Kind::Array);
  CHECK(info.valueKind == ValueKind::Int32);
  CHECK(info.structTypeName == "/pkg/Foo");
  CHECK(info.structFieldCount == 2);
}

TEST_CASE("ir lowerer struct type helpers build struct array resolvers from layout field index") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;
  auto valueKindFromTypeName = [](const std::string &typeName) {
    if (typeName == "i32") {
      return ValueKind::Int32;
    }
    if (typeName == "i64") {
      return ValueKind::Int64;
    }
    return ValueKind::Unknown;
  };

  const primec::ir_lowerer::StructLayoutFieldIndex fieldIndex =
      primec::ir_lowerer::buildStructLayoutFieldIndex(
          2,
          [](const primec::ir_lowerer::AppendStructLayoutFieldFn &appendStructLayoutField) {
            appendStructLayoutField("/pkg/Foo", {"a", "i32", "", false});
            appendStructLayoutField("/pkg/Foo", {"b", "i32", "", false});
          });
  auto resolveStructTypeName = [](const std::string &typeName,
                                  const std::string &namespacePrefix,
                                  std::string &resolvedOut) {
    if (typeName == "Foo" && namespacePrefix == "/pkg") {
      resolvedOut = "/pkg/Foo";
      return true;
    }
    return false;
  };

  auto resolveStructArrayInfoFromPath =
      primec::ir_lowerer::makeResolveStructArrayTypeInfoFromLayoutFieldIndex(
          fieldIndex, valueKindFromTypeName);
  auto applyStructArrayInfo =
      primec::ir_lowerer::makeApplyStructArrayInfoFromBindingWithLayoutFieldIndex(
          resolveStructTypeName, fieldIndex, valueKindFromTypeName);

  primec::ir_lowerer::StructArrayTypeInfo out;
  REQUIRE(resolveStructArrayInfoFromPath("/pkg/Foo", out));
  CHECK(out.structPath == "/pkg/Foo");
  CHECK(out.elementKind == ValueKind::Int32);
  CHECK(out.fieldCount == 2);
  CHECK_FALSE(resolveStructArrayInfoFromPath("/pkg/Missing", out));

  primec::Expr expr;
  expr.namespacePrefix = "/pkg";
  primec::Transform typed;
  typed.name = "Foo";
  expr.transforms.push_back(typed);
  primec::ir_lowerer::LocalInfo info;
  info.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  info.valueKind = ValueKind::Unknown;
  applyStructArrayInfo(expr, info);
  CHECK(info.kind == primec::ir_lowerer::LocalInfo::Kind::Array);
  CHECK(info.valueKind == ValueKind::Int32);
  CHECK(info.structTypeName == "/pkg/Foo");
  CHECK(info.structFieldCount == 2);
}

TEST_CASE("ir lowerer struct type helpers build bundled struct array adapters") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;
  auto valueKindFromTypeName = [](const std::string &typeName) {
    if (typeName == "i32") {
      return ValueKind::Int32;
    }
    if (typeName == "i64") {
      return ValueKind::Int64;
    }
    return ValueKind::Unknown;
  };

  const primec::ir_lowerer::StructLayoutFieldIndex fieldIndex =
      primec::ir_lowerer::buildStructLayoutFieldIndex(
          2,
          [](const primec::ir_lowerer::AppendStructLayoutFieldFn &appendStructLayoutField) {
            appendStructLayoutField("/pkg/Foo", {"a", "i32", "", false});
            appendStructLayoutField("/pkg/Foo", {"b", "i32", "", false});
          });
  auto resolveStructTypeName = [](const std::string &typeName,
                                  const std::string &namespacePrefix,
                                  std::string &resolvedOut) {
    if (typeName == "Foo" && namespacePrefix == "/pkg") {
      resolvedOut = "/pkg/Foo";
      return true;
    }
    return false;
  };

  auto adapters = primec::ir_lowerer::makeStructArrayInfoAdapters(
      fieldIndex, resolveStructTypeName, valueKindFromTypeName);

  primec::ir_lowerer::StructArrayTypeInfo out;
  REQUIRE(adapters.resolveStructArrayTypeInfoFromPath("/pkg/Foo", out));
  CHECK(out.structPath == "/pkg/Foo");
  CHECK(out.elementKind == ValueKind::Int32);
  CHECK(out.fieldCount == 2);
  CHECK_FALSE(adapters.resolveStructArrayTypeInfoFromPath("/pkg/Missing", out));

  primec::Expr expr;
  expr.namespacePrefix = "/pkg";
  primec::Transform typed;
  typed.name = "Foo";
  expr.transforms.push_back(typed);
  primec::ir_lowerer::LocalInfo info;
  info.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  info.valueKind = ValueKind::Unknown;
  adapters.applyStructArrayInfo(expr, info);
  CHECK(info.kind == primec::ir_lowerer::LocalInfo::Kind::Array);
  CHECK(info.valueKind == ValueKind::Int32);
  CHECK(info.structTypeName == "/pkg/Foo");
  CHECK(info.structFieldCount == 2);
}

TEST_CASE("ir lowerer struct type helpers apply struct array info") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;
  auto valueKindFromTypeName = [](const std::string &typeName) {
    if (typeName == "i32") {
      return ValueKind::Int32;
    }
    return ValueKind::Unknown;
  };
  auto resolveStructTypeName = [](const std::string &typeName,
                                  const std::string &namespacePrefix,
                                  std::string &resolvedOut) {
    if (typeName == "Foo" && namespacePrefix == "/pkg") {
      resolvedOut = "/pkg/Foo";
      return true;
    }
    return false;
  };
  auto collectFields = [](const std::string &structPath,
                          std::vector<primec::ir_lowerer::StructArrayFieldInfo> &out) {
    out.clear();
    if (structPath == "/pkg/Foo") {
      out.push_back({"i32", "", false});
      out.push_back({"i32", "", false});
      return true;
    }
    return false;
  };

  primec::Expr expr;
  expr.namespacePrefix = "/pkg";
  primec::Transform typed;
  typed.name = "Foo";
  expr.transforms.push_back(typed);

  primec::ir_lowerer::LocalInfo info;
  info.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  info.valueKind = ValueKind::Unknown;
  primec::ir_lowerer::applyStructArrayInfoFromBinding(
      expr, resolveStructTypeName, collectFields, valueKindFromTypeName, info);
  CHECK(info.kind == primec::ir_lowerer::LocalInfo::Kind::Array);
  CHECK(info.valueKind == ValueKind::Int32);
  CHECK(info.structTypeName == "/pkg/Foo");
  CHECK(info.structFieldCount == 2);

  primec::Expr pointerExpr;
  pointerExpr.namespacePrefix = "/pkg";
  primec::Transform pointerType;
  pointerType.name = "Pointer";
  pointerExpr.transforms.push_back(pointerType);
  primec::ir_lowerer::LocalInfo pointerInfo;
  pointerInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  pointerInfo.valueKind = ValueKind::Unknown;
  primec::ir_lowerer::applyStructArrayInfoFromBinding(
      pointerExpr, resolveStructTypeName, collectFields, valueKindFromTypeName, pointerInfo);
  CHECK(pointerInfo.kind == primec::ir_lowerer::LocalInfo::Kind::Value);
  CHECK(pointerInfo.structTypeName.empty());
}

TEST_CASE("ir lowerer struct type helpers resolve struct slot field by name") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;
  const std::vector<primec::ir_lowerer::StructSlotFieldInfo> fields = {
      {"x", 1, 1, ValueKind::Int32, ""},
      {"nested", 2, 3, ValueKind::Unknown, "/pkg/Nested"},
  };

  primec::ir_lowerer::StructSlotFieldInfo out;
  REQUIRE(primec::ir_lowerer::resolveStructSlotFieldByName(fields, "nested", out));
  CHECK(out.name == "nested");
  CHECK(out.slotOffset == 2);
  CHECK(out.slotCount == 3);
  CHECK(out.structPath == "/pkg/Nested");

  CHECK_FALSE(primec::ir_lowerer::resolveStructSlotFieldByName(fields, "missing", out));
}

TEST_CASE("ir lowerer struct type helpers resolve field slot from layout") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;
  auto resolveStructSlotFields = [](const std::string &structPath,
                                    std::vector<primec::ir_lowerer::StructSlotFieldInfo> &out) {
    out.clear();
    if (structPath == "/pkg/Container") {
      out.push_back({"value", 1, 1, ValueKind::Int32, ""});
      out.push_back({"nested", 2, 3, ValueKind::Unknown, "/pkg/Nested"});
      return true;
    }
    return false;
  };

  primec::ir_lowerer::StructSlotFieldInfo out;
  REQUIRE(primec::ir_lowerer::resolveStructFieldSlotFromLayout(
      "/pkg/Container", "nested", resolveStructSlotFields, out));
  CHECK(out.name == "nested");
  CHECK(out.slotOffset == 2);
  CHECK(out.structPath == "/pkg/Nested");

  CHECK_FALSE(primec::ir_lowerer::resolveStructFieldSlotFromLayout(
      "/pkg/Container", "missing", resolveStructSlotFields, out));
  CHECK_FALSE(primec::ir_lowerer::resolveStructFieldSlotFromLayout(
      "/pkg/Missing", "nested", resolveStructSlotFields, out));
}

TEST_CASE("ir lowerer struct type helpers resolve struct slot layouts from definition fields") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;
  auto collectStructLayoutFields = [](const std::string &structPath,
                                      std::vector<primec::ir_lowerer::StructLayoutFieldInfo> &out) {
    out.clear();
    if (structPath == "/pkg/Inner") {
      out.push_back({"x", "i32", "", false});
      return true;
    }
    if (structPath == "/pkg/Outer") {
      out.push_back({"id", "i64", "", false});
      out.push_back({"payload", "uninitialized", "Inner", false});
      out.push_back({"skipStatic", "i32", "", true});
      return true;
    }
    return false;
  };
  auto resolveDefinitionNamespacePrefix = [](const std::string &structPath, std::string &namespacePrefixOut) {
    if (structPath == "/pkg/Inner" || structPath == "/pkg/Outer") {
      namespacePrefixOut = "/pkg";
      return true;
    }
    return false;
  };
  auto resolveStructTypeName = [](const std::string &typeName,
                                  const std::string &namespacePrefix,
                                  std::string &resolvedOut) {
    if (namespacePrefix == "/pkg" && typeName == "Inner") {
      resolvedOut = "/pkg/Inner";
      return true;
    }
    return false;
  };
  auto valueKindFromTypeName = [](const std::string &typeName) {
    if (typeName == "i32") {
      return ValueKind::Int32;
    }
    if (typeName == "i64") {
      return ValueKind::Int64;
    }
    return ValueKind::Unknown;
  };

  primec::ir_lowerer::StructSlotLayoutCache layoutCache;
  std::unordered_set<std::string> layoutStack;
  primec::ir_lowerer::StructSlotLayoutInfo layout;
  std::string error;
  REQUIRE(primec::ir_lowerer::resolveStructSlotLayoutFromDefinitionFields("/pkg/Outer",
                                                                           collectStructLayoutFields,
                                                                           resolveDefinitionNamespacePrefix,
                                                                           resolveStructTypeName,
                                                                           valueKindFromTypeName,
                                                                           layoutCache,
                                                                           layoutStack,
                                                                           layout,
                                                                           error));
  CHECK(error.empty());
  CHECK(layout.structPath == "/pkg/Outer");
  CHECK(layout.totalSlots == 4);
  REQUIRE(layout.fields.size() == 2);
  CHECK(layout.fields[0].name == "id");
  CHECK(layout.fields[0].slotOffset == 1);
  CHECK(layout.fields[0].slotCount == 1);
  CHECK(layout.fields[0].valueKind == ValueKind::Int64);
  CHECK(layout.fields[1].name == "payload");
  CHECK(layout.fields[1].slotOffset == 2);
  CHECK(layout.fields[1].slotCount == 2);
  CHECK(layout.fields[1].structPath == "/pkg/Inner");
  CHECK(layoutCache.count("/pkg/Outer") == 1);
  CHECK(layoutCache.count("/pkg/Inner") == 1);
  CHECK(layoutStack.empty());

  primec::ir_lowerer::StructSlotFieldInfo slot;
  REQUIRE(primec::ir_lowerer::resolveStructFieldSlotFromDefinitionFields("/pkg/Outer",
                                                                          "payload",
                                                                          collectStructLayoutFields,
                                                                          resolveDefinitionNamespacePrefix,
                                                                          resolveStructTypeName,
                                                                          valueKindFromTypeName,
                                                                          layoutCache,
                                                                          layoutStack,
                                                                          slot,
                                                                          error));
  CHECK(slot.name == "payload");
  CHECK(slot.slotOffset == 2);
  CHECK(slot.slotCount == 2);
  CHECK(slot.structPath == "/pkg/Inner");
}

TEST_CASE("ir lowerer struct type helpers resolve struct slots from definition field index") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;
  const primec::ir_lowerer::StructLayoutFieldIndex fieldIndex =
      primec::ir_lowerer::buildStructLayoutFieldIndex(
          2,
          [](const primec::ir_lowerer::AppendStructLayoutFieldFn &appendStructLayoutField) {
            appendStructLayoutField("/pkg/Inner", {"x", "i32", "", false});
            appendStructLayoutField("/pkg/Outer", {"id", "i64", "", false});
            appendStructLayoutField("/pkg/Outer", {"payload", "uninitialized", "Inner", false});
          });
  primec::Definition innerDef;
  innerDef.fullPath = "/pkg/Inner";
  innerDef.namespacePrefix = "/pkg";
  primec::Definition outerDef;
  outerDef.fullPath = "/pkg/Outer";
  outerDef.namespacePrefix = "/pkg";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/pkg/Inner", &innerDef},
      {"/pkg/Outer", &outerDef},
      {"/pkg/Null", nullptr},
  };
  auto resolveStructTypeName = [](const std::string &typeName,
                                  const std::string &namespacePrefix,
                                  std::string &resolvedOut) {
    if (namespacePrefix == "/pkg" && typeName == "Inner") {
      resolvedOut = "/pkg/Inner";
      return true;
    }
    return false;
  };
  auto valueKindFromTypeName = [](const std::string &typeName) {
    if (typeName == "i32") {
      return ValueKind::Int32;
    }
    if (typeName == "i64") {
      return ValueKind::Int64;
    }
    return ValueKind::Unknown;
  };

  primec::ir_lowerer::StructSlotLayoutCache layoutCache;
  std::unordered_set<std::string> layoutStack;
  primec::ir_lowerer::StructSlotLayoutInfo layout;
  std::string error;
  REQUIRE(primec::ir_lowerer::resolveStructSlotLayoutFromDefinitionFieldIndex("/pkg/Outer",
                                                                               fieldIndex,
                                                                               defMap,
                                                                               resolveStructTypeName,
                                                                               valueKindFromTypeName,
                                                                               layoutCache,
                                                                               layoutStack,
                                                                               layout,
                                                                               error));
  CHECK(layout.structPath == "/pkg/Outer");
  CHECK(layout.totalSlots == 4);
  REQUIRE(layout.fields.size() == 2);
  CHECK(layout.fields[1].name == "payload");
  CHECK(layout.fields[1].structPath == "/pkg/Inner");

  primec::ir_lowerer::StructSlotFieldInfo slot;
  REQUIRE(primec::ir_lowerer::resolveStructFieldSlotFromDefinitionFieldIndex("/pkg/Outer",
                                                                              "payload",
                                                                              fieldIndex,
                                                                              defMap,
                                                                              resolveStructTypeName,
                                                                              valueKindFromTypeName,
                                                                              layoutCache,
                                                                              layoutStack,
                                                                              slot,
                                                                              error));
  CHECK(slot.slotOffset == 2);
  CHECK(slot.slotCount == 2);
  CHECK(slot.structPath == "/pkg/Inner");

  CHECK_FALSE(primec::ir_lowerer::resolveStructSlotLayoutFromDefinitionFieldIndex("/pkg/Null",
                                                                                   fieldIndex,
                                                                                   defMap,
                                                                                   resolveStructTypeName,
                                                                                   valueKindFromTypeName,
                                                                                   layoutCache,
                                                                                   layoutStack,
                                                                                   layout,
                                                                                   error));
  CHECK(error == "native backend cannot resolve struct layout: /pkg/Null");
}

TEST_CASE("ir lowerer struct type helpers build struct-slot resolvers from definition field index") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;
  const primec::ir_lowerer::StructLayoutFieldIndex fieldIndex =
      primec::ir_lowerer::buildStructLayoutFieldIndex(
          2,
          [](const primec::ir_lowerer::AppendStructLayoutFieldFn &appendStructLayoutField) {
            appendStructLayoutField("/pkg/Inner", {"x", "i32", "", false});
            appendStructLayoutField("/pkg/Outer", {"id", "i64", "", false});
            appendStructLayoutField("/pkg/Outer", {"payload", "uninitialized", "Inner", false});
          });
  primec::Definition innerDef;
  innerDef.fullPath = "/pkg/Inner";
  innerDef.namespacePrefix = "/pkg";
  primec::Definition outerDef;
  outerDef.fullPath = "/pkg/Outer";
  outerDef.namespacePrefix = "/pkg";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/pkg/Inner", &innerDef},
      {"/pkg/Outer", &outerDef},
  };
  auto resolveStructTypeName = [](const std::string &typeName,
                                  const std::string &namespacePrefix,
                                  std::string &resolvedOut) {
    if (namespacePrefix == "/pkg" && typeName == "Inner") {
      resolvedOut = "/pkg/Inner";
      return true;
    }
    return false;
  };
  auto valueKindFromTypeName = [](const std::string &typeName) {
    if (typeName == "i32") {
      return ValueKind::Int32;
    }
    if (typeName == "i64") {
      return ValueKind::Int64;
    }
    return ValueKind::Unknown;
  };

  primec::ir_lowerer::StructSlotLayoutCache layoutCache;
  std::unordered_set<std::string> layoutStack;
  std::string error;

  auto resolveStructSlotLayout = primec::ir_lowerer::makeResolveStructSlotLayoutFromDefinitionFieldIndex(
      fieldIndex, defMap, resolveStructTypeName, valueKindFromTypeName, layoutCache, layoutStack, error);
  auto resolveStructFieldSlot = primec::ir_lowerer::makeResolveStructFieldSlotFromDefinitionFieldIndex(
      fieldIndex, defMap, resolveStructTypeName, valueKindFromTypeName, layoutCache, layoutStack, error);

  primec::ir_lowerer::StructSlotLayoutInfo layout;
  REQUIRE(resolveStructSlotLayout("/pkg/Outer", layout));
  CHECK(layout.structPath == "/pkg/Outer");
  CHECK(layout.totalSlots == 4);
  REQUIRE(layout.fields.size() == 2);
  CHECK(layout.fields[1].name == "payload");
  CHECK(layout.fields[1].structPath == "/pkg/Inner");

  primec::ir_lowerer::StructSlotFieldInfo slot;
  REQUIRE(resolveStructFieldSlot("/pkg/Outer", "payload", slot));
  CHECK(slot.slotOffset == 2);
  CHECK(slot.slotCount == 2);
  CHECK(slot.structPath == "/pkg/Inner");

  CHECK_FALSE(resolveStructSlotLayout("/pkg/Missing", layout));
  CHECK(error == "native backend cannot resolve struct layout: /pkg/Missing");
}

TEST_CASE("ir lowerer struct type helpers build bundled struct-slot resolution adapters") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;
  const primec::ir_lowerer::StructLayoutFieldIndex fieldIndex =
      primec::ir_lowerer::buildStructLayoutFieldIndex(
          2,
          [](const primec::ir_lowerer::AppendStructLayoutFieldFn &appendStructLayoutField) {
            appendStructLayoutField("/pkg/Inner", {"x", "i32", "", false});
            appendStructLayoutField("/pkg/Outer", {"id", "i64", "", false});
            appendStructLayoutField("/pkg/Outer", {"payload", "uninitialized", "Inner", false});
          });
  primec::Definition innerDef;
  innerDef.fullPath = "/pkg/Inner";
  innerDef.namespacePrefix = "/pkg";
  primec::Definition outerDef;
  outerDef.fullPath = "/pkg/Outer";
  outerDef.namespacePrefix = "/pkg";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/pkg/Inner", &innerDef},
      {"/pkg/Outer", &outerDef},
  };
  auto resolveStructTypeName = [](const std::string &typeName,
                                  const std::string &namespacePrefix,
                                  std::string &resolvedOut) {
    if (namespacePrefix == "/pkg" && typeName == "Inner") {
      resolvedOut = "/pkg/Inner";
      return true;
    }
    return false;
  };
  auto valueKindFromTypeName = [](const std::string &typeName) {
    if (typeName == "i32") {
      return ValueKind::Int32;
    }
    if (typeName == "i64") {
      return ValueKind::Int64;
    }
    return ValueKind::Unknown;
  };

  primec::ir_lowerer::StructSlotLayoutCache layoutCache;
  std::unordered_set<std::string> layoutStack;
  std::string error;
  auto adapters = primec::ir_lowerer::makeStructSlotResolutionAdapters(
      fieldIndex, defMap, resolveStructTypeName, valueKindFromTypeName, layoutCache, layoutStack, error);

  primec::ir_lowerer::StructSlotLayoutInfo layout;
  REQUIRE(adapters.resolveStructSlotLayout("/pkg/Outer", layout));
  CHECK(layout.structPath == "/pkg/Outer");
  CHECK(layout.totalSlots == 4);
  REQUIRE(layout.fields.size() == 2);
  CHECK(layout.fields[1].name == "payload");
  CHECK(layout.fields[1].structPath == "/pkg/Inner");

  primec::ir_lowerer::StructSlotFieldInfo slot;
  REQUIRE(adapters.resolveStructFieldSlot("/pkg/Outer", "payload", slot));
  CHECK(slot.slotOffset == 2);
  CHECK(slot.slotCount == 2);
  CHECK(slot.structPath == "/pkg/Inner");

  CHECK_FALSE(adapters.resolveStructSlotLayout("/pkg/Missing", layout));
  CHECK(error == "native backend cannot resolve struct layout: /pkg/Missing");
}

TEST_CASE("ir lowerer struct type helpers build bundled struct-slot adapters with owned state") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;
  const primec::ir_lowerer::StructLayoutFieldIndex fieldIndex =
      primec::ir_lowerer::buildStructLayoutFieldIndex(
          2,
          [](const primec::ir_lowerer::AppendStructLayoutFieldFn &appendStructLayoutField) {
            appendStructLayoutField("/pkg/Inner", {"x", "i32", "", false});
            appendStructLayoutField("/pkg/Outer", {"id", "i64", "", false});
            appendStructLayoutField("/pkg/Outer", {"payload", "uninitialized", "Inner", false});
          });
  primec::Definition innerDef;
  innerDef.fullPath = "/pkg/Inner";
  innerDef.namespacePrefix = "/pkg";
  primec::Definition outerDef;
  outerDef.fullPath = "/pkg/Outer";
  outerDef.namespacePrefix = "/pkg";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/pkg/Inner", &innerDef},
      {"/pkg/Outer", &outerDef},
  };
  auto resolveStructTypeName = [](const std::string &typeName,
                                  const std::string &namespacePrefix,
                                  std::string &resolvedOut) {
    if (namespacePrefix == "/pkg" && typeName == "Inner") {
      resolvedOut = "/pkg/Inner";
      return true;
    }
    return false;
  };
  auto valueKindFromTypeName = [](const std::string &typeName) {
    if (typeName == "i32") {
      return ValueKind::Int32;
    }
    if (typeName == "i64") {
      return ValueKind::Int64;
    }
    return ValueKind::Unknown;
  };

  std::string error;
  auto adapters = primec::ir_lowerer::makeStructSlotResolutionAdaptersWithOwnedState(
      fieldIndex, defMap, resolveStructTypeName, valueKindFromTypeName, error);

  primec::ir_lowerer::StructSlotLayoutInfo layout;
  REQUIRE(adapters.resolveStructSlotLayout("/pkg/Outer", layout));
  CHECK(layout.structPath == "/pkg/Outer");
  CHECK(layout.totalSlots == 4);
  REQUIRE(layout.fields.size() == 2);
  CHECK(layout.fields[1].name == "payload");
  CHECK(layout.fields[1].structPath == "/pkg/Inner");

  primec::ir_lowerer::StructSlotFieldInfo slot;
  REQUIRE(adapters.resolveStructFieldSlot("/pkg/Outer", "payload", slot));
  CHECK(slot.slotOffset == 2);
  CHECK(slot.slotCount == 2);
  CHECK(slot.structPath == "/pkg/Inner");

  CHECK_FALSE(adapters.resolveStructSlotLayout("/pkg/Missing", layout));
  CHECK(error == "native backend cannot resolve struct layout: /pkg/Missing");
}

TEST_CASE("ir lowerer struct type helpers build bundled struct layout adapters") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;
  const primec::ir_lowerer::StructLayoutFieldIndex fieldIndex =
      primec::ir_lowerer::buildStructLayoutFieldIndex(
          2,
          [](const primec::ir_lowerer::AppendStructLayoutFieldFn &appendStructLayoutField) {
            appendStructLayoutField("/pkg/Inner", {"x", "i32", "", false});
            appendStructLayoutField("/pkg/Outer", {"id", "i64", "", false});
            appendStructLayoutField("/pkg/Outer", {"payload", "uninitialized", "Inner", false});
          });
  primec::Definition innerDef;
  innerDef.fullPath = "/pkg/Inner";
  innerDef.namespacePrefix = "/pkg";
  primec::Definition outerDef;
  outerDef.fullPath = "/pkg/Outer";
  outerDef.namespacePrefix = "/pkg";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/pkg/Inner", &innerDef},
      {"/pkg/Outer", &outerDef},
  };
  auto resolveStructTypeName = [](const std::string &typeName,
                                  const std::string &namespacePrefix,
                                  std::string &resolvedOut) {
    if (namespacePrefix == "/pkg" && typeName == "Inner") {
      resolvedOut = "/pkg/Inner";
      return true;
    }
    if (namespacePrefix == "/pkg" && typeName == "Outer") {
      resolvedOut = "/pkg/Outer";
      return true;
    }
    return false;
  };
  auto valueKindFromTypeName = [](const std::string &typeName) {
    if (typeName == "i32") {
      return ValueKind::Int32;
    }
    if (typeName == "i64") {
      return ValueKind::Int64;
    }
    return ValueKind::Unknown;
  };

  std::string error;
  const auto adapters = primec::ir_lowerer::makeStructLayoutResolutionAdaptersWithOwnedSlotState(
      fieldIndex, defMap, resolveStructTypeName, valueKindFromTypeName, error);

  primec::ir_lowerer::StructArrayTypeInfo arrayInfo;
  REQUIRE(adapters.structArrayInfo.resolveStructArrayTypeInfoFromPath("/pkg/Inner", arrayInfo));
  CHECK(arrayInfo.structPath == "/pkg/Inner");
  CHECK(arrayInfo.elementKind == ValueKind::Int32);
  CHECK(arrayInfo.fieldCount == 1);

  primec::ir_lowerer::StructSlotLayoutInfo layout;
  REQUIRE(adapters.structSlotResolution.resolveStructSlotLayout("/pkg/Outer", layout));
  CHECK(layout.totalSlots == 4);
  REQUIRE(layout.fields.size() == 2);
  CHECK(layout.fields[1].name == "payload");
  CHECK(layout.fields[1].structPath == "/pkg/Inner");

  CHECK_FALSE(adapters.structSlotResolution.resolveStructSlotLayout("/pkg/Missing", layout));
  CHECK(error == "native backend cannot resolve struct layout: /pkg/Missing");
}

TEST_CASE("ir lowerer struct type helpers report definition slot layout diagnostics") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;
  auto resolveDefinitionNamespacePrefix = [](const std::string &structPath, std::string &namespacePrefixOut) {
    if (structPath.rfind("/pkg/", 0) == 0) {
      namespacePrefixOut = "/pkg";
      return true;
    }
    return false;
  };
  auto resolveStructTypeName = [](const std::string &typeName,
                                  const std::string &namespacePrefix,
                                  std::string &resolvedOut) {
    if (namespacePrefix == "/pkg" && typeName == "Cycle") {
      resolvedOut = "/pkg/Cycle";
      return true;
    }
    return false;
  };
  auto valueKindFromTypeName = [](const std::string &typeName) {
    if (typeName == "i32") {
      return ValueKind::Int32;
    }
    return ValueKind::Unknown;
  };

  {
    auto collectStructLayoutFields = [](const std::string &structPath,
                                        std::vector<primec::ir_lowerer::StructLayoutFieldInfo> &out) {
      out.clear();
      if (structPath == "/pkg/BadUninit") {
        out.push_back({"slot", "uninitialized", "", false});
        return true;
      }
      return false;
    };
    primec::ir_lowerer::StructSlotLayoutCache layoutCache;
    std::unordered_set<std::string> layoutStack;
    primec::ir_lowerer::StructSlotLayoutInfo layout;
    std::string error;
    CHECK_FALSE(primec::ir_lowerer::resolveStructSlotLayoutFromDefinitionFields("/pkg/BadUninit",
                                                                                 collectStructLayoutFields,
                                                                                 resolveDefinitionNamespacePrefix,
                                                                                 resolveStructTypeName,
                                                                                 valueKindFromTypeName,
                                                                                 layoutCache,
                                                                                 layoutStack,
                                                                                 layout,
                                                                                 error));
    CHECK(error == "uninitialized requires a template argument on /pkg/BadUninit");
  }

  {
    auto collectStructLayoutFields = [](const std::string &structPath,
                                        std::vector<primec::ir_lowerer::StructLayoutFieldInfo> &out) {
      out.clear();
      if (structPath == "/pkg/BadTemplate") {
        out.push_back({"slot", "array", "i32", false});
        return true;
      }
      return false;
    };
    primec::ir_lowerer::StructSlotLayoutCache layoutCache;
    std::unordered_set<std::string> layoutStack;
    primec::ir_lowerer::StructSlotLayoutInfo layout;
    std::string error;
    CHECK_FALSE(primec::ir_lowerer::resolveStructSlotLayoutFromDefinitionFields("/pkg/BadTemplate",
                                                                                 collectStructLayoutFields,
                                                                                 resolveDefinitionNamespacePrefix,
                                                                                 resolveStructTypeName,
                                                                                 valueKindFromTypeName,
                                                                                 layoutCache,
                                                                                 layoutStack,
                                                                                 layout,
                                                                                 error));
    CHECK(error == "native backend does not support templated struct fields on /pkg/BadTemplate");
  }

  {
    auto collectStructLayoutFields = [](const std::string &structPath,
                                        std::vector<primec::ir_lowerer::StructLayoutFieldInfo> &out) {
      out.clear();
      if (structPath == "/pkg/Cycle") {
        out.push_back({"next", "Cycle", "", false});
        return true;
      }
      return false;
    };
    primec::ir_lowerer::StructSlotLayoutCache layoutCache;
    std::unordered_set<std::string> layoutStack;
    primec::ir_lowerer::StructSlotLayoutInfo layout;
    std::string error;
    CHECK_FALSE(primec::ir_lowerer::resolveStructSlotLayoutFromDefinitionFields("/pkg/Cycle",
                                                                                 collectStructLayoutFields,
                                                                                 resolveDefinitionNamespacePrefix,
                                                                                 resolveStructTypeName,
                                                                                 valueKindFromTypeName,
                                                                                 layoutCache,
                                                                                 layoutStack,
                                                                                 layout,
                                                                                 error));
    CHECK(error == "recursive struct layout not supported: /pkg/Cycle");
  }
}

TEST_CASE("ir lowerer struct type helpers apply struct value info") {
  auto resolveStruct = [](const std::string &typeName,
                          const std::string &namespacePrefix,
                          std::string &resolvedOut) {
    if (typeName == "Foo" && namespacePrefix == "/pkg") {
      resolvedOut = "/pkg/Foo";
      return true;
    }
    return false;
  };

  primec::Expr typedBinding;
  typedBinding.namespacePrefix = "/pkg";
  primec::Transform qualifier;
  qualifier.name = "mut";
  typedBinding.transforms.push_back(qualifier);
  primec::Transform typed;
  typed.name = "Foo";
  typedBinding.transforms.push_back(typed);

  primec::ir_lowerer::LocalInfo info;
  info.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  primec::ir_lowerer::applyStructValueInfoFromBinding(typedBinding, resolveStruct, info);
  CHECK(info.structTypeName == "/pkg/Foo");

  primec::Expr pointerBinding;
  pointerBinding.namespacePrefix = "/pkg";
  primec::Transform pointerType;
  pointerType.name = "Pointer";
  pointerType.templateArgs = {"Foo"};
  pointerBinding.transforms.push_back(pointerType);

  primec::ir_lowerer::LocalInfo pointerInfo;
  pointerInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  primec::ir_lowerer::applyStructValueInfoFromBinding(pointerBinding, resolveStruct, pointerInfo);
  CHECK(pointerInfo.structTypeName == "/pkg/Foo");

  primec::ir_lowerer::LocalInfo alreadyTyped;
  alreadyTyped.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  alreadyTyped.structTypeName = "/already/Set";
  primec::ir_lowerer::applyStructValueInfoFromBinding(typedBinding, resolveStruct, alreadyTyped);
  CHECK(alreadyTyped.structTypeName == "/already/Set");
}

TEST_CASE("ir lowerer struct type helpers build struct value info applier") {
  auto resolveStruct = [](const std::string &typeName,
                          const std::string &namespacePrefix,
                          std::string &resolvedOut) {
    if (typeName == "Foo" && namespacePrefix == "/pkg") {
      resolvedOut = "/pkg/Foo";
      return true;
    }
    return false;
  };
  auto applyStructValueInfo = primec::ir_lowerer::makeApplyStructValueInfoFromBinding(resolveStruct);

  primec::Expr typedBinding;
  typedBinding.namespacePrefix = "/pkg";
  primec::Transform typed;
  typed.name = "Foo";
  typedBinding.transforms.push_back(typed);

  primec::ir_lowerer::LocalInfo info;
  info.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  applyStructValueInfo(typedBinding, info);
  CHECK(info.structTypeName == "/pkg/Foo");
}

TEST_CASE("ir lowerer struct type helpers build bundled struct-type resolution adapters") {
  const std::unordered_set<std::string> structNames = {"/pkg/Foo"};
  const std::unordered_map<std::string, std::string> importAliases = {{"Foo", "/pkg/Foo"}};
  auto adapters = primec::ir_lowerer::makeStructTypeResolutionAdapters(structNames, importAliases);

  std::string resolved;
  REQUIRE(adapters.resolveStructTypeName("Foo", "/pkg", resolved));
  CHECK(resolved == "/pkg/Foo");

  primec::Expr typedBinding;
  typedBinding.namespacePrefix = "/pkg";
  primec::Transform typed;
  typed.name = "Foo";
  typedBinding.transforms.push_back(typed);

  primec::ir_lowerer::LocalInfo info;
  info.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  adapters.applyStructValueInfo(typedBinding, info);
  CHECK(info.structTypeName == "/pkg/Foo");
}

TEST_CASE("ir lowerer struct type helpers build bundled setup-type and struct-type adapters") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;
  const std::unordered_set<std::string> structNames = {"/pkg/Foo"};
  const std::unordered_map<std::string, std::string> importAliases = {{"Foo", "/pkg/Foo"}};
  const auto adapters = primec::ir_lowerer::makeSetupTypeAndStructTypeAdapters(structNames, importAliases);

  CHECK(adapters.valueKindFromTypeName("i64") == ValueKind::Int64);
  CHECK(adapters.combineNumericKinds(ValueKind::Int32, ValueKind::Int64) == ValueKind::Int64);

  std::string resolved;
  REQUIRE(adapters.structTypeResolutionAdapters.resolveStructTypeName("Foo", "/pkg", resolved));
  CHECK(resolved == "/pkg/Foo");
  CHECK_FALSE(adapters.structTypeResolutionAdapters.resolveStructTypeName("Missing", "/pkg", resolved));

  primec::Expr typedBinding;
  typedBinding.namespacePrefix = "/pkg";
  primec::Transform typed;
  typed.name = "Foo";
  typedBinding.transforms.push_back(typed);

  primec::ir_lowerer::LocalInfo info;
  info.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  adapters.structTypeResolutionAdapters.applyStructValueInfo(typedBinding, info);
  CHECK(info.structTypeName == "/pkg/Foo");
}

TEST_CASE("ir lowerer struct type helpers skip unsupported struct value paths") {
  auto resolveStruct = [](const std::string &, const std::string &, std::string &) { return false; };

  primec::Expr typedBinding;
  typedBinding.namespacePrefix = "/pkg";
  primec::Transform typed;
  typed.name = "Foo";
  typedBinding.transforms.push_back(typed);

  primec::ir_lowerer::LocalInfo unresolvedInfo;
  unresolvedInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  primec::ir_lowerer::applyStructValueInfoFromBinding(typedBinding, resolveStruct, unresolvedInfo);
  CHECK(unresolvedInfo.structTypeName.empty());

  primec::ir_lowerer::LocalInfo nonValueInfo;
  nonValueInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  primec::ir_lowerer::applyStructValueInfoFromBinding(typedBinding, resolveStruct, nonValueInfo);
  CHECK(nonValueInfo.structTypeName.empty());

  primec::Expr pointerWithoutTemplate;
  pointerWithoutTemplate.namespacePrefix = "/pkg";
  primec::Transform pointerType;
  pointerType.name = "Pointer";
  pointerWithoutTemplate.transforms.push_back(pointerType);

  primec::ir_lowerer::LocalInfo pointerInfo;
  pointerInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  primec::ir_lowerer::applyStructValueInfoFromBinding(pointerWithoutTemplate, resolveStruct, pointerInfo);
  CHECK(pointerInfo.structTypeName.empty());
}

TEST_CASE("ir lowerer uninitialized type helpers classify supported types") {
  auto resolveStruct = [](const std::string &typeName,
                          const std::string &,
                          std::string &resolvedOut) {
    if (typeName == "MyStruct") {
      resolvedOut = "/pkg/MyStruct";
      return true;
    }
    return false;
  };

  primec::ir_lowerer::UninitializedTypeInfo info;
  std::string error;

  REQUIRE(primec::ir_lowerer::resolveUninitializedTypeInfo("array<i64>", "/pkg", resolveStruct, info, error));
  CHECK(info.kind == primec::ir_lowerer::LocalInfo::Kind::Array);
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int64);

  REQUIRE(primec::ir_lowerer::resolveUninitializedTypeInfo("vector<bool>", "/pkg", resolveStruct, info, error));
  CHECK(info.kind == primec::ir_lowerer::LocalInfo::Kind::Vector);
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Bool);

  REQUIRE(primec::ir_lowerer::resolveUninitializedTypeInfo("map<i32, f64>", "/pkg", resolveStruct, info, error));
  CHECK(info.kind == primec::ir_lowerer::LocalInfo::Kind::Map);
  CHECK(info.mapKeyKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  CHECK(info.mapValueKind == primec::ir_lowerer::LocalInfo::ValueKind::Float64);
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Float64);

  REQUIRE(primec::ir_lowerer::resolveUninitializedTypeInfo("Pointer<i32>", "/pkg", resolveStruct, info, error));
  CHECK(info.kind == primec::ir_lowerer::LocalInfo::Kind::Value);
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int64);

  REQUIRE(primec::ir_lowerer::resolveUninitializedTypeInfo("MyStruct", "/pkg", resolveStruct, info, error));
  CHECK(info.kind == primec::ir_lowerer::LocalInfo::Kind::Value);
  CHECK(info.structPath == "/pkg/MyStruct");

  REQUIRE(primec::ir_lowerer::resolveUninitializedTypeInfo("string", "/pkg", resolveStruct, info, error));
  CHECK(info.kind == primec::ir_lowerer::LocalInfo::Kind::Value);
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::String);
}

TEST_CASE("ir lowerer uninitialized type helpers build type resolver") {
  auto resolveStruct = [](const std::string &typeName,
                          const std::string &,
                          std::string &resolvedOut) {
    if (typeName == "MyStruct") {
      resolvedOut = "/pkg/MyStruct";
      return true;
    }
    return false;
  };

  std::string error;
  auto resolveTypeInfo = primec::ir_lowerer::makeResolveUninitializedTypeInfo(resolveStruct, error);

  primec::ir_lowerer::UninitializedTypeInfo info;
  REQUIRE(resolveTypeInfo("MyStruct", "/pkg", info));
  CHECK(info.structPath == "/pkg/MyStruct");

  CHECK_FALSE(resolveTypeInfo("Thing<i32>", "/pkg", info));
  CHECK(error == "native backend does not support uninitialized storage for type: Thing<i32>");
}

TEST_CASE("ir lowerer uninitialized type helpers report diagnostics") {
  auto resolveStruct = [](const std::string &, const std::string &, std::string &) { return false; };

  primec::ir_lowerer::UninitializedTypeInfo info;
  std::string error;
  CHECK_FALSE(primec::ir_lowerer::resolveUninitializedTypeInfo(
      "array<i32, i64>", "/pkg", resolveStruct, info, error));
  CHECK(error == "native backend requires array to have exactly one template argument");

  error.clear();
  CHECK_FALSE(primec::ir_lowerer::resolveUninitializedTypeInfo(
      "map<i32, string>", "/pkg", resolveStruct, info, error));
  CHECK(error == "native backend only supports numeric/bool map values for uninitialized storage");

  error.clear();
  CHECK_FALSE(primec::ir_lowerer::resolveUninitializedTypeInfo(
      "Thing<i32>", "/pkg", resolveStruct, info, error));
  CHECK(error == "native backend does not support uninitialized storage for type: Thing<i32>");
}

TEST_CASE("ir lowerer uninitialized type helpers resolve local storage metadata") {
  primec::ir_lowerer::LocalInfo local;
  local.isUninitializedStorage = true;
  local.kind = primec::ir_lowerer::LocalInfo::Kind::Map;
  local.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  local.mapKeyKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  local.mapValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Float64;
  local.structTypeName = "/pkg/MyStruct";

  primec::ir_lowerer::UninitializedTypeInfo out;
  REQUIRE(primec::ir_lowerer::resolveUninitializedTypeInfoFromLocalStorage(local, out));
  CHECK(out.kind == primec::ir_lowerer::LocalInfo::Kind::Map);
  CHECK(out.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int64);
  CHECK(out.mapKeyKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  CHECK(out.mapValueKind == primec::ir_lowerer::LocalInfo::ValueKind::Float64);
  CHECK(out.structPath == "/pkg/MyStruct");

  local.isUninitializedStorage = false;
  CHECK_FALSE(primec::ir_lowerer::resolveUninitializedTypeInfoFromLocalStorage(local, out));
}

TEST_CASE("ir lowerer uninitialized type helpers resolve local storage candidates") {
  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo localInfo;
  localInfo.isUninitializedStorage = true;
  localInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  localInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  locals.emplace("slot", localInfo);
  auto localIt = locals.find("slot");
  REQUIRE(localIt != locals.end());

  primec::Expr storageExpr;
  storageExpr.kind = primec::Expr::Kind::Name;
  storageExpr.name = "slot";

  const primec::ir_lowerer::LocalInfo *localOut = nullptr;
  primec::ir_lowerer::UninitializedTypeInfo typeInfoOut;
  bool resolved = false;
  REQUIRE(primec::ir_lowerer::resolveUninitializedLocalStorageCandidate(
      storageExpr, locals, localOut, typeInfoOut, resolved));
  CHECK(resolved);
  CHECK(localOut == &localIt->second);
  CHECK(typeInfoOut.kind == primec::ir_lowerer::LocalInfo::Kind::Array);
  CHECK(typeInfoOut.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int64);

  storageExpr.name = "missing";
  REQUIRE(primec::ir_lowerer::resolveUninitializedLocalStorageCandidate(
      storageExpr, locals, localOut, typeInfoOut, resolved));
  CHECK_FALSE(resolved);
  CHECK(localOut == nullptr);

  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  CHECK_FALSE(primec::ir_lowerer::resolveUninitializedLocalStorageCandidate(
      callExpr, locals, localOut, typeInfoOut, resolved));
}

TEST_CASE("ir lowerer uninitialized type helpers resolve local storage access") {
  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo localInfo;
  localInfo.isUninitializedStorage = true;
  localInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  localInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  locals.emplace("slot", localInfo);
  const auto localIt = locals.find("slot");
  REQUIRE(localIt != locals.end());

  primec::Expr storageExpr;
  storageExpr.kind = primec::Expr::Kind::Name;
  storageExpr.name = "slot";

  primec::ir_lowerer::UninitializedLocalStorageAccessInfo out;
  bool resolved = false;
  REQUIRE(primec::ir_lowerer::resolveUninitializedLocalStorageAccess(storageExpr, locals, out, resolved));
  CHECK(resolved);
  CHECK(out.local == &localIt->second);
  CHECK(out.typeInfo.kind == primec::ir_lowerer::LocalInfo::Kind::Array);
  CHECK(out.typeInfo.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int64);

  storageExpr.name = "missing";
  REQUIRE(primec::ir_lowerer::resolveUninitializedLocalStorageAccess(storageExpr, locals, out, resolved));
  CHECK_FALSE(resolved);

  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  CHECK_FALSE(primec::ir_lowerer::resolveUninitializedLocalStorageAccess(callExpr, locals, out, resolved));
}

TEST_CASE("ir lowerer uninitialized type helpers resolve field template args by struct path") {
  auto collectFields = [](const std::string &structPath,
                          std::vector<primec::ir_lowerer::UninitializedFieldBindingInfo> &fieldsOut) {
    if (structPath != "/pkg/Container") {
      return false;
    }
    fieldsOut.clear();
    fieldsOut.push_back({"skip_static", "uninitialized", "i64", true});
    fieldsOut.push_back({"wrong_type", "i64", "", false});
    fieldsOut.push_back({"slot", "uninitialized", "map<i32, f64>", false});
    return true;
  };

  std::string typeTemplateArg;
  REQUIRE(primec::ir_lowerer::resolveUninitializedFieldTemplateArg(
      "/pkg/Container", "slot", collectFields, typeTemplateArg));
  CHECK(typeTemplateArg == "map<i32, f64>");

  CHECK_FALSE(primec::ir_lowerer::resolveUninitializedFieldTemplateArg(
      "/pkg/Missing", "slot", collectFields, typeTemplateArg));
  CHECK_FALSE(primec::ir_lowerer::resolveUninitializedFieldTemplateArg(
      "/pkg/Container", "missing", collectFields, typeTemplateArg));
  CHECK_FALSE(primec::ir_lowerer::resolveUninitializedFieldTemplateArg(
      "/pkg/Container", "wrong_type", collectFields, typeTemplateArg));
}

TEST_CASE("ir lowerer uninitialized type helpers collect field bindings from index") {
  primec::ir_lowerer::UninitializedFieldBindingIndex fieldIndex;
  fieldIndex["/pkg/Container"] = {
      {"slot", "uninitialized", "i64", false},
      {"ignored", "i64", "", false},
  };

  std::vector<primec::ir_lowerer::UninitializedFieldBindingInfo> fieldsOut;
  REQUIRE(primec::ir_lowerer::collectUninitializedFieldBindingsFromIndex(
      fieldIndex, "/pkg/Container", fieldsOut));
  REQUIRE(fieldsOut.size() == 2);
  CHECK(fieldsOut[0].name == "slot");
  CHECK(fieldsOut[0].typeTemplateArg == "i64");

  CHECK_FALSE(primec::ir_lowerer::collectUninitializedFieldBindingsFromIndex(
      fieldIndex, "/pkg/Missing", fieldsOut));
}

TEST_CASE("ir lowerer uninitialized type helpers build field binding index") {
  const primec::ir_lowerer::UninitializedFieldBindingIndex fieldIndex =
      primec::ir_lowerer::buildUninitializedFieldBindingIndex(
          2,
          [](const primec::ir_lowerer::AppendUninitializedFieldBindingFn &appendFieldBinding) {
            appendFieldBinding("/pkg/Container", {"slot", "uninitialized", "i64", false});
            appendFieldBinding("/pkg/Container", {"static_slot", "uninitialized", "i32", true});
            appendFieldBinding("/pkg/Other", {"value", "i64", "", false});
          });

  REQUIRE(fieldIndex.size() == 2);

  std::vector<primec::ir_lowerer::UninitializedFieldBindingInfo> fieldsOut;
  REQUIRE(primec::ir_lowerer::collectUninitializedFieldBindingsFromIndex(
      fieldIndex, "/pkg/Container", fieldsOut));
  REQUIRE(fieldsOut.size() == 2);
  CHECK(fieldsOut[0].name == "slot");
  CHECK(fieldsOut[0].typeTemplateArg == "i64");
  CHECK(fieldsOut[1].name == "static_slot");
  CHECK(fieldsOut[1].isStatic);

  REQUIRE(primec::ir_lowerer::collectUninitializedFieldBindingsFromIndex(
      fieldIndex, "/pkg/Other", fieldsOut));
  REQUIRE(fieldsOut.size() == 1);
  CHECK(fieldsOut[0].name == "value");

  const primec::ir_lowerer::UninitializedFieldBindingIndex emptyIndex =
      primec::ir_lowerer::buildUninitializedFieldBindingIndex(4, {});
  CHECK(emptyIndex.empty());
}

TEST_CASE("ir lowerer uninitialized type helpers build field binding index from struct layout field index") {
  const primec::ir_lowerer::StructLayoutFieldIndex layoutFieldIndex =
      primec::ir_lowerer::buildStructLayoutFieldIndex(
          2,
          [](const primec::ir_lowerer::AppendStructLayoutFieldFn &appendStructLayoutField) {
            appendStructLayoutField("/pkg/Container", {"slot", "uninitialized", "i64", false});
            appendStructLayoutField("/pkg/Container", {"static_slot", "uninitialized", "i32", true});
            appendStructLayoutField("/pkg/Other", {"value", "i64", "", false});
          });
  const primec::ir_lowerer::UninitializedFieldBindingIndex fieldIndex =
      primec::ir_lowerer::buildUninitializedFieldBindingIndexFromStructLayoutFieldIndex(layoutFieldIndex);

  REQUIRE(fieldIndex.size() == 2);

  std::vector<primec::ir_lowerer::UninitializedFieldBindingInfo> fieldsOut;
  REQUIRE(primec::ir_lowerer::collectUninitializedFieldBindingsFromIndex(
      fieldIndex, "/pkg/Container", fieldsOut));
  REQUIRE(fieldsOut.size() == 2);
  CHECK(fieldsOut[0].name == "slot");
  CHECK(fieldsOut[0].typeTemplateArg == "i64");
  CHECK(fieldsOut[1].name == "static_slot");
  CHECK(fieldsOut[1].isStatic);

  REQUIRE(primec::ir_lowerer::collectUninitializedFieldBindingsFromIndex(
      fieldIndex, "/pkg/Other", fieldsOut));
  REQUIRE(fieldsOut.size() == 1);
  CHECK(fieldsOut[0].name == "value");

  const primec::ir_lowerer::UninitializedFieldBindingIndex emptyIndex =
      primec::ir_lowerer::buildUninitializedFieldBindingIndexFromStructLayoutFieldIndex({});
  CHECK(emptyIndex.empty());
}

TEST_CASE("ir lowerer uninitialized type helpers build bundled struct and uninitialized field indexes") {
  const primec::ir_lowerer::StructAndUninitializedFieldIndexes indexes =
      primec::ir_lowerer::buildStructAndUninitializedFieldIndexes(
          2,
          [](const primec::ir_lowerer::AppendStructLayoutFieldFn &appendStructLayoutField) {
            appendStructLayoutField("/pkg/Container", {"slot", "uninitialized", "i64", false});
            appendStructLayoutField("/pkg/Container", {"value", "i32", "", false});
            appendStructLayoutField("/pkg/Other", {"field", "uninitialized", "f64", false});
          });

  REQUIRE(indexes.structLayoutFieldIndex.size() == 2);
  std::vector<primec::ir_lowerer::StructLayoutFieldInfo> layoutFields;
  REQUIRE(primec::ir_lowerer::collectStructLayoutFieldsFromIndex(
      indexes.structLayoutFieldIndex, "/pkg/Container", layoutFields));
  REQUIRE(layoutFields.size() == 2);
  CHECK(layoutFields[0].name == "slot");
  CHECK(layoutFields[1].name == "value");

  std::vector<primec::ir_lowerer::UninitializedFieldBindingInfo> fieldsOut;
  REQUIRE(primec::ir_lowerer::collectUninitializedFieldBindingsFromIndex(
      indexes.uninitializedFieldBindingIndex, "/pkg/Container", fieldsOut));
  REQUIRE(fieldsOut.size() == 2);
  CHECK(fieldsOut[0].name == "slot");
  CHECK(fieldsOut[0].typeTemplateArg == "i64");
  CHECK(fieldsOut[1].name == "value");

  REQUIRE(primec::ir_lowerer::collectUninitializedFieldBindingsFromIndex(
      indexes.uninitializedFieldBindingIndex, "/pkg/Other", fieldsOut));
  REQUIRE(fieldsOut.size() == 1);
  CHECK(fieldsOut[0].name == "field");
  CHECK(fieldsOut[0].typeTemplateArg == "f64");

  const primec::ir_lowerer::StructAndUninitializedFieldIndexes emptyIndexes =
      primec::ir_lowerer::buildStructAndUninitializedFieldIndexes(1, {});
  CHECK(emptyIndexes.structLayoutFieldIndex.empty());
  CHECK(emptyIndexes.uninitializedFieldBindingIndex.empty());
}

TEST_CASE("ir lowerer uninitialized type helpers build bundled struct and uninitialized resolution setup") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;
  primec::Definition containerDef;
  containerDef.fullPath = "/pkg/Container";
  containerDef.namespacePrefix = "/pkg";
  primec::Definition myStructDef;
  myStructDef.fullPath = "/pkg/MyStruct";
  myStructDef.namespacePrefix = "/pkg";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/pkg/Container", &containerDef},
      {"/pkg/MyStruct", &myStructDef},
  };

  auto resolveStructTypeName = [](const std::string &typeName,
                                  const std::string &namespacePrefix,
                                  std::string &resolvedOut) {
    if (namespacePrefix == "/pkg" && typeName == "MyStruct") {
      resolvedOut = "/pkg/MyStruct";
      return true;
    }
    if (namespacePrefix == "/pkg" && typeName == "Container") {
      resolvedOut = "/pkg/Container";
      return true;
    }
    return false;
  };
  auto valueKindFromTypeName = [](const std::string &typeName) {
    if (typeName == "i32") {
      return ValueKind::Int32;
    }
    return ValueKind::Unknown;
  };
  auto resolveExprPath = [](const primec::Expr &) { return std::string(); };

  std::string error;
  primec::ir_lowerer::StructAndUninitializedResolutionSetup setup;
  REQUIRE(primec::ir_lowerer::buildStructAndUninitializedResolutionSetup(
      2,
      [](const primec::ir_lowerer::AppendStructLayoutFieldFn &appendStructLayoutField) {
        appendStructLayoutField("/pkg/MyStruct", {"value", "i32", "", false});
        appendStructLayoutField("/pkg/Container", {"slot", "uninitialized", "MyStruct", false});
      },
      defMap,
      resolveStructTypeName,
      valueKindFromTypeName,
      resolveExprPath,
      setup,
      error));

  std::vector<primec::ir_lowerer::StructLayoutFieldInfo> layoutFields;
  REQUIRE(primec::ir_lowerer::collectStructLayoutFieldsFromIndex(
      setup.fieldIndexes.structLayoutFieldIndex, "/pkg/Container", layoutFields));
  REQUIRE(layoutFields.size() == 1);
  CHECK(layoutFields[0].name == "slot");

  std::vector<primec::ir_lowerer::UninitializedFieldBindingInfo> fieldBindings;
  REQUIRE(primec::ir_lowerer::collectUninitializedFieldBindingsFromIndex(
      setup.fieldIndexes.uninitializedFieldBindingIndex, "/pkg/Container", fieldBindings));
  REQUIRE(fieldBindings.size() == 1);
  CHECK(fieldBindings[0].name == "slot");
  CHECK(fieldBindings[0].typeTemplateArg == "MyStruct");

  primec::ir_lowerer::StructSlotFieldInfo fieldSlot;
  REQUIRE(setup.structLayoutResolutionAdapters.structSlotResolution.resolveStructFieldSlot(
      "/pkg/Container", "slot", fieldSlot));
  CHECK(fieldSlot.name == "slot");
  CHECK(fieldSlot.slotOffset == 1);
  CHECK(fieldSlot.slotCount == 2);
  CHECK(fieldSlot.structPath == "/pkg/MyStruct");

  primec::ir_lowerer::UninitializedTypeInfo typeInfo;
  REQUIRE(setup.uninitializedResolutionAdapters.resolveUninitializedTypeInfo(
      "MyStruct", "/pkg", typeInfo));
  CHECK(typeInfo.structPath == "/pkg/MyStruct");

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo receiver;
  receiver.structTypeName = "/pkg/Container";
  locals.emplace("self", receiver);
  const auto receiverIt = locals.find("self");
  REQUIRE(receiverIt != locals.end());

  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "self";
  primec::Expr fieldExpr;
  fieldExpr.kind = primec::Expr::Kind::Call;
  fieldExpr.isFieldAccess = true;
  fieldExpr.name = "slot";
  fieldExpr.args.push_back(receiverExpr);

  primec::ir_lowerer::UninitializedStorageAccessInfo access;
  bool resolved = false;
  REQUIRE(setup.uninitializedResolutionAdapters.resolveUninitializedStorage(
      fieldExpr, locals, access, resolved));
  CHECK(resolved);
  CHECK(access.location == primec::ir_lowerer::UninitializedStorageAccessInfo::Location::Field);
  CHECK(access.receiver == &receiverIt->second);
  CHECK(access.fieldSlot.slotOffset == 1);
  CHECK(access.typeInfo.structPath == "/pkg/MyStruct");
}

TEST_CASE(
    "ir lowerer uninitialized type helpers build bundled setup-type, struct-type, and uninitialized setup") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;
  const std::unordered_set<std::string> structNames = {"/pkg/Container", "/pkg/MyStruct"};
  const std::unordered_map<std::string, std::string> importAliases = {{"MyStruct", "/pkg/MyStruct"}};

  primec::Definition containerDef;
  containerDef.fullPath = "/pkg/Container";
  containerDef.namespacePrefix = "/pkg";
  primec::Definition myStructDef;
  myStructDef.fullPath = "/pkg/MyStruct";
  myStructDef.namespacePrefix = "/pkg";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/pkg/Container", &containerDef},
      {"/pkg/MyStruct", &myStructDef},
  };

  std::string error;
  primec::ir_lowerer::SetupTypeStructAndUninitializedResolutionSetup setup;
  REQUIRE(primec::ir_lowerer::buildSetupTypeStructAndUninitializedResolutionSetup(
      structNames,
      importAliases,
      2,
      [](const primec::ir_lowerer::AppendStructLayoutFieldFn &appendStructLayoutField) {
        appendStructLayoutField("/pkg/MyStruct", {"value", "i32", "", false});
        appendStructLayoutField("/pkg/Container", {"slot", "uninitialized", "MyStruct", false});
      },
      defMap,
      [](const primec::Expr &) { return std::string(); },
      setup,
      error));
  CHECK(error.empty());

  CHECK(setup.setupTypeAndStructTypeAdapters.valueKindFromTypeName("i32") == ValueKind::Int32);
  CHECK(setup.setupTypeAndStructTypeAdapters.combineNumericKinds(ValueKind::Int32, ValueKind::Int64) ==
        ValueKind::Int64);

  std::string resolvedStructPath;
  REQUIRE(setup.setupTypeAndStructTypeAdapters.structTypeResolutionAdapters.resolveStructTypeName(
      "MyStruct", "/pkg", resolvedStructPath));
  CHECK(resolvedStructPath == "/pkg/MyStruct");

  primec::ir_lowerer::StructSlotFieldInfo fieldSlot;
  REQUIRE(setup.structAndUninitializedResolutionSetup.structLayoutResolutionAdapters.structSlotResolution
              .resolveStructFieldSlot("/pkg/Container", "slot", fieldSlot));
  CHECK(fieldSlot.structPath == "/pkg/MyStruct");

  primec::ir_lowerer::UninitializedTypeInfo typeInfo;
  REQUIRE(setup.structAndUninitializedResolutionSetup.uninitializedResolutionAdapters.resolveUninitializedTypeInfo(
      "MyStruct", "/pkg", typeInfo));
  CHECK(typeInfo.structPath == "/pkg/MyStruct");
}

TEST_CASE(
    "ir lowerer uninitialized type helpers build bundled setup-math, setup-type, and uninitialized setup") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;
  const std::unordered_set<std::string> structNames = {"/pkg/Container", "/pkg/MyStruct"};
  const std::unordered_map<std::string, std::string> importAliases = {{"MyStruct", "/pkg/MyStruct"}};

  primec::Definition containerDef;
  containerDef.fullPath = "/pkg/Container";
  containerDef.namespacePrefix = "/pkg";
  primec::Definition myStructDef;
  myStructDef.fullPath = "/pkg/MyStruct";
  myStructDef.namespacePrefix = "/pkg";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/pkg/Container", &containerDef},
      {"/pkg/MyStruct", &myStructDef},
  };

  std::string error;
  primec::ir_lowerer::SetupMathTypeStructAndUninitializedResolutionSetup setup;
  REQUIRE(primec::ir_lowerer::buildSetupMathTypeStructAndUninitializedResolutionSetup(
      true,
      structNames,
      importAliases,
      2,
      [](const primec::ir_lowerer::AppendStructLayoutFieldFn &appendStructLayoutField) {
        appendStructLayoutField("/pkg/MyStruct", {"value", "i32", "", false});
        appendStructLayoutField("/pkg/Container", {"slot", "uninitialized", "MyStruct", false});
      },
      defMap,
      [](const primec::Expr &) { return std::string(); },
      setup,
      error));
  CHECK(error.empty());

  primec::Expr mathCall;
  mathCall.kind = primec::Expr::Kind::Call;
  mathCall.name = "sin";
  std::string builtinName;
  CHECK(setup.setupMathAndBindingAdapters.setupMathResolvers.getMathBuiltinName(mathCall, builtinName));
  CHECK(builtinName == "sin");

  primec::Expr stringBinding;
  primec::Transform stringType;
  stringType.name = "string";
  stringBinding.transforms.push_back(stringType);
  CHECK(setup.setupMathAndBindingAdapters.bindingTypeAdapters.isStringBinding(stringBinding));
  CHECK_FALSE(setup.setupMathAndBindingAdapters.bindingTypeAdapters.isFileErrorBinding(stringBinding));

  CHECK(setup.setupTypeStructAndUninitializedResolutionSetup.setupTypeAndStructTypeAdapters.valueKindFromTypeName(
            "i32") == ValueKind::Int32);
  std::string resolvedStructPath;
  REQUIRE(setup.setupTypeStructAndUninitializedResolutionSetup.setupTypeAndStructTypeAdapters
              .structTypeResolutionAdapters.resolveStructTypeName("MyStruct", "/pkg", resolvedStructPath));
  CHECK(resolvedStructPath == "/pkg/MyStruct");
  CHECK_FALSE(setup.setupMathAndBindingAdapters.setupMathResolvers.getMathConstantName("phi", builtinName));
}

TEST_CASE("ir lowerer uninitialized type helpers check field index struct path") {
  const primec::ir_lowerer::UninitializedFieldBindingIndex fieldIndex =
      primec::ir_lowerer::buildUninitializedFieldBindingIndex(
          2,
          [](const primec::ir_lowerer::AppendUninitializedFieldBindingFn &appendFieldBinding) {
            appendFieldBinding("/pkg/Container", {"slot", "uninitialized", "i64", false});
          });
  CHECK(primec::ir_lowerer::hasUninitializedFieldBindingsForStructPath(
      fieldIndex, "/pkg/Container"));
  CHECK_FALSE(primec::ir_lowerer::hasUninitializedFieldBindingsForStructPath(
      fieldIndex, "/pkg/Missing"));
}

TEST_CASE("ir lowerer uninitialized type helpers infer call target struct paths from field index") {
  const primec::ir_lowerer::UninitializedFieldBindingIndex fieldIndex =
      primec::ir_lowerer::buildUninitializedFieldBindingIndex(
          1,
          [](const primec::ir_lowerer::AppendUninitializedFieldBindingFn &appendFieldBinding) {
            appendFieldBinding("/pkg/Ctor", {"slot", "uninitialized", "i64", false});
          });
  auto resolveExprPath = [](const primec::Expr &expr) {
    if (expr.name == "Ctor") {
      return std::string("/pkg/Ctor");
    }
    if (expr.name == "factory") {
      return std::string("/pkg/factory");
    }
    return std::string("/pkg/unknown");
  };
  auto inferDefinitionStructReturnPath = [](const std::string &path) {
    if (path == "/pkg/factory") {
      return std::string("/pkg/FromDef");
    }
    return std::string();
  };

  primec::Expr ctorCall;
  ctorCall.kind = primec::Expr::Kind::Call;
  ctorCall.name = "Ctor";
  CHECK(primec::ir_lowerer::inferStructPathFromCallTargetWithFieldBindingIndex(
            ctorCall, resolveExprPath, fieldIndex, inferDefinitionStructReturnPath) == "/pkg/Ctor");

  primec::Expr factoryCall;
  factoryCall.kind = primec::Expr::Kind::Call;
  factoryCall.name = "factory";
  CHECK(primec::ir_lowerer::inferStructPathFromCallTargetWithFieldBindingIndex(
            factoryCall, resolveExprPath, fieldIndex, inferDefinitionStructReturnPath) == "/pkg/FromDef");

  primec::Expr unknownCall;
  unknownCall.kind = primec::Expr::Kind::Call;
  unknownCall.name = "unknown";
  CHECK(primec::ir_lowerer::inferStructPathFromCallTargetWithFieldBindingIndex(
            unknownCall, resolveExprPath, fieldIndex, inferDefinitionStructReturnPath).empty());
}

TEST_CASE("ir lowerer uninitialized type helpers infer call target struct paths with visited field index callbacks") {
  const primec::ir_lowerer::UninitializedFieldBindingIndex fieldIndex =
      primec::ir_lowerer::buildUninitializedFieldBindingIndex(
          1,
          [](const primec::ir_lowerer::AppendUninitializedFieldBindingFn &appendFieldBinding) {
            appendFieldBinding("/pkg/Ctor", {"slot", "uninitialized", "i64", false});
          });
  auto resolveExprPath = [](const primec::Expr &expr) {
    if (expr.name == "Ctor") {
      return std::string("/pkg/Ctor");
    }
    if (expr.name == "factory") {
      return std::string("/pkg/factory");
    }
    return std::string("/pkg/unknown");
  };
  auto inferDefinitionStructReturnPath = [](const std::string &path, std::unordered_set<std::string> &visitedDefs) {
    if (path == "/pkg/factory" && visitedDefs.insert(path).second) {
      return std::string("/pkg/FromDef");
    }
    return std::string();
  };

  std::unordered_set<std::string> visitedDefs;
  primec::Expr factoryCall;
  factoryCall.kind = primec::Expr::Kind::Call;
  factoryCall.name = "factory";
  CHECK(primec::ir_lowerer::inferStructPathFromCallTargetWithFieldBindingIndexAndVisited(
            factoryCall, resolveExprPath, fieldIndex, inferDefinitionStructReturnPath, visitedDefs) == "/pkg/FromDef");
  CHECK(visitedDefs.count("/pkg/factory") == 1);
  CHECK(primec::ir_lowerer::inferStructPathFromCallTargetWithFieldBindingIndexAndVisited(
            factoryCall, resolveExprPath, fieldIndex, inferDefinitionStructReturnPath, visitedDefs)
            .empty());

  primec::Expr ctorCall;
  ctorCall.kind = primec::Expr::Kind::Call;
  ctorCall.name = "Ctor";
  CHECK(primec::ir_lowerer::inferStructPathFromCallTargetWithFieldBindingIndexAndVisited(
            ctorCall, resolveExprPath, fieldIndex, inferDefinitionStructReturnPath, visitedDefs) == "/pkg/Ctor");
}

TEST_CASE("ir lowerer uninitialized type helpers infer definition return paths with visited map lookups") {
  primec::Definition factoryDef;
  factoryDef.fullPath = "/pkg/factory";
  factoryDef.namespacePrefix = "/pkg";
  primec::Expr returnExpr;
  returnExpr.kind = primec::Expr::Kind::Call;
  returnExpr.name = "inner";
  factoryDef.returnExpr = returnExpr;
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/pkg/factory", &factoryDef},
  };

  auto resolveStructTypeName = [](const std::string &, const std::string &, std::string &) { return false; };
  auto inferStructReturnExprPath = [](const primec::Expr &expr, std::unordered_set<std::string> &visitedDefs) {
    if (expr.name == "inner" && visitedDefs.count("/pkg/factory") > 0) {
      return std::string("/pkg/FromExpr");
    }
    return std::string();
  };

  std::unordered_set<std::string> visitedDefs;
  CHECK(primec::ir_lowerer::inferStructReturnPathFromDefinitionMapWithVisited(
            "/pkg/factory", defMap, resolveStructTypeName, inferStructReturnExprPath, visitedDefs) ==
        "/pkg/FromExpr");
  CHECK(visitedDefs.count("/pkg/factory") == 1);

  CHECK(primec::ir_lowerer::inferStructReturnPathFromDefinitionMapWithVisited(
            "/pkg/missing", defMap, resolveStructTypeName, inferStructReturnExprPath, visitedDefs)
            .empty());
  CHECK(primec::ir_lowerer::inferStructReturnPathFromDefinitionMapWithVisited(
            "/pkg/factory", defMap, resolveStructTypeName, inferStructReturnExprPath, visitedDefs)
            .empty());
}

TEST_CASE("ir lowerer uninitialized type helpers infer definition return paths from definition map") {
  primec::Definition factoryDef;
  factoryDef.fullPath = "/pkg/factory";
  factoryDef.namespacePrefix = "/pkg";
  primec::Expr returnExpr;
  returnExpr.kind = primec::Expr::Kind::Call;
  returnExpr.name = "inner";
  factoryDef.returnExpr = returnExpr;
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/pkg/factory", &factoryDef},
  };

  auto resolveStructTypeName = [](const std::string &, const std::string &, std::string &) { return false; };
  auto inferStructReturnExprPath = [](const primec::Expr &expr, std::unordered_set<std::string> &visitedDefs) {
    if (expr.name == "inner" && visitedDefs.count("/pkg/factory") > 0) {
      return std::string("/pkg/FromExpr");
    }
    return std::string();
  };

  CHECK(primec::ir_lowerer::inferStructReturnPathFromDefinitionMap(
            "/pkg/factory", defMap, resolveStructTypeName, inferStructReturnExprPath) == "/pkg/FromExpr");
  CHECK(primec::ir_lowerer::inferStructReturnPathFromDefinitionMap(
            "/pkg/missing", defMap, resolveStructTypeName, inferStructReturnExprPath)
            .empty());
}

TEST_CASE("ir lowerer uninitialized type helpers infer definition return paths by call target field index") {
  primec::Definition factoryDef;
  factoryDef.fullPath = "/pkg/factory";
  factoryDef.namespacePrefix = "/pkg";
  primec::Expr stepExpr;
  stepExpr.kind = primec::Expr::Kind::Call;
  stepExpr.name = "step";
  factoryDef.returnExpr = stepExpr;

  primec::Definition stepDef;
  stepDef.fullPath = "/pkg/step";
  stepDef.namespacePrefix = "/pkg";
  primec::Expr ctorExpr;
  ctorExpr.kind = primec::Expr::Kind::Call;
  ctorExpr.name = "Ctor";
  stepDef.returnExpr = ctorExpr;

  primec::Definition cycleADef;
  cycleADef.fullPath = "/pkg/cycleA";
  cycleADef.namespacePrefix = "/pkg";
  primec::Expr cycleBExpr;
  cycleBExpr.kind = primec::Expr::Kind::Call;
  cycleBExpr.name = "cycleB";
  cycleADef.returnExpr = cycleBExpr;

  primec::Definition cycleBDef;
  cycleBDef.fullPath = "/pkg/cycleB";
  cycleBDef.namespacePrefix = "/pkg";
  primec::Expr cycleAExpr;
  cycleAExpr.kind = primec::Expr::Kind::Call;
  cycleAExpr.name = "cycleA";
  cycleBDef.returnExpr = cycleAExpr;

  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/pkg/factory", &factoryDef},
      {"/pkg/step", &stepDef},
      {"/pkg/cycleA", &cycleADef},
      {"/pkg/cycleB", &cycleBDef},
  };
  const primec::ir_lowerer::UninitializedFieldBindingIndex fieldIndex =
      primec::ir_lowerer::buildUninitializedFieldBindingIndex(
          1,
          [](const primec::ir_lowerer::AppendUninitializedFieldBindingFn &appendFieldBinding) {
            appendFieldBinding("/pkg/Ctor", {"slot", "uninitialized", "i64", false});
          });

  auto resolveStructTypeName = [](const std::string &, const std::string &, std::string &) { return false; };
  auto resolveExprPath = [](const primec::Expr &expr) {
    if (expr.kind != primec::Expr::Kind::Call) {
      return std::string();
    }
    return std::string("/pkg/") + expr.name;
  };

  CHECK(primec::ir_lowerer::inferStructReturnPathFromDefinitionMapByCallTargetWithFieldIndex(
            "/pkg/factory", defMap, resolveStructTypeName, resolveExprPath, fieldIndex) == "/pkg/Ctor");
  CHECK(primec::ir_lowerer::inferStructReturnPathFromDefinitionMapByCallTargetWithFieldIndex(
            "/pkg/cycleA", defMap, resolveStructTypeName, resolveExprPath, fieldIndex)
            .empty());

  std::unordered_set<std::string> visitedDefs = {"/pkg/factory"};
  CHECK(primec::ir_lowerer::inferStructReturnPathFromDefinitionMapByCallTargetWithFieldIndexWithVisited(
            "/pkg/factory", defMap, resolveStructTypeName, resolveExprPath, fieldIndex, visitedDefs)
            .empty());
}

TEST_CASE("ir lowerer uninitialized type helpers infer expression struct paths by definition call target field index") {
  primec::Definition factoryDef;
  factoryDef.fullPath = "/pkg/factory";
  factoryDef.namespacePrefix = "/pkg";
  primec::Expr stepExpr;
  stepExpr.kind = primec::Expr::Kind::Call;
  stepExpr.name = "step";
  factoryDef.returnExpr = stepExpr;

  primec::Definition stepDef;
  stepDef.fullPath = "/pkg/step";
  stepDef.namespacePrefix = "/pkg";
  primec::Expr ctorExpr;
  ctorExpr.kind = primec::Expr::Kind::Call;
  ctorExpr.name = "Ctor";
  stepDef.returnExpr = ctorExpr;

  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/pkg/factory", &factoryDef},
      {"/pkg/step", &stepDef},
  };
  const primec::ir_lowerer::UninitializedFieldBindingIndex fieldIndex =
      primec::ir_lowerer::buildUninitializedFieldBindingIndex(
          1,
          [](const primec::ir_lowerer::AppendUninitializedFieldBindingFn &appendFieldBinding) {
            appendFieldBinding("/pkg/Ctor", {"slot", "uninitialized", "i64", false});
          });
  auto resolveStructTypeName = [](const std::string &, const std::string &, std::string &) { return false; };
  auto resolveExprPath = [](const primec::Expr &expr) {
    if (expr.kind != primec::Expr::Kind::Call) {
      return std::string();
    }
    return std::string("/pkg/") + expr.name;
  };
  auto resolveStructFieldSlot = [](const std::string &structPath,
                                   const std::string &fieldName,
                                   primec::ir_lowerer::StructSlotFieldInfo &out) {
    if (structPath == "/pkg/Local" && fieldName == "slot") {
      out = {"slot", 1, 1, primec::ir_lowerer::LocalInfo::ValueKind::Unknown, "/pkg/Nested"};
      return true;
    }
    return false;
  };

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo localInfo;
  localInfo.structTypeName = "/pkg/Local";
  locals.emplace("self", localInfo);

  primec::Expr nameExpr;
  nameExpr.kind = primec::Expr::Kind::Name;
  nameExpr.name = "self";
  CHECK(primec::ir_lowerer::inferStructExprPathFromDefinitionMapByCallTargetWithFieldIndex(
            nameExpr, locals, defMap, resolveStructTypeName, resolveExprPath, fieldIndex, resolveStructFieldSlot) ==
        "/pkg/Local");

  primec::Expr fieldReceiverExpr;
  fieldReceiverExpr.kind = primec::Expr::Kind::Name;
  fieldReceiverExpr.name = "self";
  primec::Expr fieldExpr;
  fieldExpr.kind = primec::Expr::Kind::Call;
  fieldExpr.isFieldAccess = true;
  fieldExpr.name = "slot";
  fieldExpr.args.push_back(fieldReceiverExpr);
  CHECK(primec::ir_lowerer::inferStructExprPathFromDefinitionMapByCallTargetWithFieldIndex(
            fieldExpr, locals, defMap, resolveStructTypeName, resolveExprPath, fieldIndex, resolveStructFieldSlot) ==
        "/pkg/Nested");

  primec::Expr unresolvedFieldExpr;
  unresolvedFieldExpr.kind = primec::Expr::Kind::Call;
  unresolvedFieldExpr.isFieldAccess = true;
  unresolvedFieldExpr.name = "factory";
  unresolvedFieldExpr.args.push_back(fieldReceiverExpr);
  CHECK(primec::ir_lowerer::inferStructExprPathFromDefinitionMapByCallTargetWithFieldIndex(unresolvedFieldExpr,
                                                                                             locals,
                                                                                             defMap,
                                                                                             resolveStructTypeName,
                                                                                             resolveExprPath,
                                                                                             fieldIndex,
                                                                                             resolveStructFieldSlot)
            .empty());

  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "factory";
  CHECK(primec::ir_lowerer::inferStructExprPathFromDefinitionMapByCallTargetWithFieldIndex(
            callExpr, locals, defMap, resolveStructTypeName, resolveExprPath, fieldIndex, resolveStructFieldSlot) ==
        "/pkg/Ctor");
}

TEST_CASE("ir lowerer uninitialized type helpers build expression struct path inferer from definition call target field index") {
  primec::Definition factoryDef;
  factoryDef.fullPath = "/pkg/factory";
  factoryDef.namespacePrefix = "/pkg";
  primec::Expr ctorExpr;
  ctorExpr.kind = primec::Expr::Kind::Call;
  ctorExpr.name = "Ctor";
  factoryDef.returnExpr = ctorExpr;

  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/pkg/factory", &factoryDef},
  };
  const primec::ir_lowerer::UninitializedFieldBindingIndex fieldIndex =
      primec::ir_lowerer::buildUninitializedFieldBindingIndex(
          1,
          [](const primec::ir_lowerer::AppendUninitializedFieldBindingFn &appendFieldBinding) {
            appendFieldBinding("/pkg/Ctor", {"slot", "uninitialized", "i64", false});
          });
  auto resolveStructTypeName = [](const std::string &, const std::string &, std::string &) { return false; };
  auto resolveExprPath = [](const primec::Expr &expr) {
    if (expr.kind != primec::Expr::Kind::Call) {
      return std::string();
    }
    return std::string("/pkg/") + expr.name;
  };
  auto resolveStructFieldSlot = [](const std::string &, const std::string &, primec::ir_lowerer::StructSlotFieldInfo &) {
    return false;
  };

  auto inferStructExprPath =
      primec::ir_lowerer::makeInferStructExprPathFromDefinitionMapByCallTargetWithFieldIndex(
          defMap, resolveStructTypeName, resolveExprPath, fieldIndex, resolveStructFieldSlot);

  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "factory";
  primec::ir_lowerer::LocalMap locals;
  CHECK(inferStructExprPath(callExpr, locals) == "/pkg/Ctor");
}

TEST_CASE("ir lowerer uninitialized type helpers find field template args") {
  std::vector<primec::ir_lowerer::UninitializedFieldBindingInfo> fields;
  fields.push_back({"skip_static", "uninitialized", "i64", true});
  fields.push_back({"wrong_type", "i64", "", false});
  fields.push_back({"slot", "uninitialized", "map<i32, f64>", false});

  std::string typeTemplateArg;
  REQUIRE(primec::ir_lowerer::findUninitializedFieldTemplateArg(fields, "slot", typeTemplateArg));
  CHECK(typeTemplateArg == "map<i32, f64>");

  CHECK_FALSE(primec::ir_lowerer::findUninitializedFieldTemplateArg(fields, "missing", typeTemplateArg));
  CHECK_FALSE(primec::ir_lowerer::findUninitializedFieldTemplateArg(fields, "wrong_type", typeTemplateArg));
  CHECK_FALSE(primec::ir_lowerer::findUninitializedFieldTemplateArg(fields, "skip_static", typeTemplateArg));
}

TEST_CASE("ir lowerer uninitialized type helpers resolve field storage candidates") {
  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo receiver;
  receiver.structTypeName = "/pkg/Container";
  locals.emplace("self", receiver);
  const auto receiverIt = locals.find("self");
  REQUIRE(receiverIt != locals.end());

  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "self";

  primec::Expr storageExpr;
  storageExpr.kind = primec::Expr::Kind::Call;
  storageExpr.isFieldAccess = true;
  storageExpr.name = "slot";
  storageExpr.args.push_back(receiverExpr);

  auto findField = [](const std::string &structPath,
                      const std::string &fieldName,
                      std::string &typeTemplateArgOut) {
    if (structPath == "/pkg/Container" && fieldName == "slot") {
      typeTemplateArgOut = "i64";
      return true;
    }
    return false;
  };

  const primec::ir_lowerer::LocalInfo *receiverOut = nullptr;
  std::string structPathOut;
  std::string typeTemplateArgOut;
  REQUIRE(primec::ir_lowerer::resolveUninitializedFieldStorageCandidate(
      storageExpr, locals, findField, receiverOut, structPathOut, typeTemplateArgOut));
  CHECK(receiverOut == &receiverIt->second);
  CHECK(structPathOut == "/pkg/Container");
  CHECK(typeTemplateArgOut == "i64");

  storageExpr.name = "missing";
  CHECK_FALSE(primec::ir_lowerer::resolveUninitializedFieldStorageCandidate(
      storageExpr, locals, findField, receiverOut, structPathOut, typeTemplateArgOut));

  storageExpr.name = "slot";
  storageExpr.isFieldAccess = false;
  CHECK_FALSE(primec::ir_lowerer::resolveUninitializedFieldStorageCandidate(
      storageExpr, locals, findField, receiverOut, structPathOut, typeTemplateArgOut));
}

TEST_CASE("ir lowerer uninitialized type helpers resolve field storage type info") {
  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo receiver;
  receiver.structTypeName = "/pkg/Container";
  locals.emplace("self", receiver);
  const auto receiverIt = locals.find("self");
  REQUIRE(receiverIt != locals.end());

  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "self";

  primec::Expr storageExpr;
  storageExpr.kind = primec::Expr::Kind::Call;
  storageExpr.isFieldAccess = true;
  storageExpr.name = "slot";
  storageExpr.args.push_back(receiverExpr);

  auto findField = [](const std::string &structPath,
                      const std::string &fieldName,
                      std::string &typeTemplateArgOut) {
    if (structPath == "/pkg/Container" && fieldName == "slot") {
      typeTemplateArgOut = "i64";
      return true;
    }
    return false;
  };
  auto resolveNamespacePrefix = [](const std::string &structPath) {
    if (structPath == "/pkg/Container") {
      return std::string("/pkg");
    }
    return std::string();
  };
  auto resolveTypeInfo = [](const std::string &typeText,
                            const std::string &namespacePrefix,
                            primec::ir_lowerer::UninitializedTypeInfo &out) {
    const auto resolveStruct = [](const std::string &, const std::string &, std::string &) { return false; };
    std::string error;
    return primec::ir_lowerer::resolveUninitializedTypeInfo(typeText, namespacePrefix, resolveStruct, out, error);
  };

  primec::ir_lowerer::UninitializedFieldStorageTypeInfo out;
  bool resolved = false;
  std::string error;
  REQUIRE(primec::ir_lowerer::resolveUninitializedFieldStorageTypeInfo(
      storageExpr, locals, findField, resolveNamespacePrefix, resolveTypeInfo, out, resolved, error));
  CHECK(resolved);
  CHECK(error.empty());
  CHECK(out.receiver == &receiverIt->second);
  CHECK(out.structPath == "/pkg/Container");
  CHECK(out.typeInfo.kind == primec::ir_lowerer::LocalInfo::Kind::Value);
  CHECK(out.typeInfo.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int64);

  storageExpr.name = "missing";
  REQUIRE(primec::ir_lowerer::resolveUninitializedFieldStorageTypeInfo(
      storageExpr, locals, findField, resolveNamespacePrefix, resolveTypeInfo, out, resolved, error));
  CHECK_FALSE(resolved);

  storageExpr.name = "slot";
  auto findUnsupportedField = [](const std::string &structPath,
                                 const std::string &fieldName,
                                 std::string &typeTemplateArgOut) {
    if (structPath == "/pkg/Container" && fieldName == "slot") {
      typeTemplateArgOut = "Thing<i32>";
      return true;
    }
    return false;
  };
  auto resolveTypeInfoFail = [](const std::string &, const std::string &, primec::ir_lowerer::UninitializedTypeInfo &) {
    return false;
  };
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::resolveUninitializedFieldStorageTypeInfo(
      storageExpr, locals, findUnsupportedField, resolveNamespacePrefix, resolveTypeInfoFail, out, resolved, error));
  CHECK(error == "native backend does not support uninitialized storage for type: Thing<i32>");
}

TEST_CASE("ir lowerer uninitialized type helpers resolve field storage access") {
  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo receiver;
  receiver.structTypeName = "/pkg/Container";
  locals.emplace("self", receiver);
  const auto receiverIt = locals.find("self");
  REQUIRE(receiverIt != locals.end());

  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "self";

  primec::Expr storageExpr;
  storageExpr.kind = primec::Expr::Kind::Call;
  storageExpr.isFieldAccess = true;
  storageExpr.name = "slot";
  storageExpr.args.push_back(receiverExpr);

  auto findField = [](const std::string &structPath,
                      const std::string &fieldName,
                      std::string &typeTemplateArgOut) {
    if (structPath == "/pkg/Container" && fieldName == "slot") {
      typeTemplateArgOut = "i64";
      return true;
    }
    return false;
  };
  auto resolveNamespacePrefix = [](const std::string &structPath) {
    if (structPath == "/pkg/Container") {
      return std::string("/pkg");
    }
    return std::string();
  };
  auto resolveTypeInfo = [](const std::string &typeText,
                            const std::string &namespacePrefix,
                            primec::ir_lowerer::UninitializedTypeInfo &out) {
    const auto resolveStruct = [](const std::string &, const std::string &, std::string &) { return false; };
    std::string error;
    return primec::ir_lowerer::resolveUninitializedTypeInfo(typeText, namespacePrefix, resolveStruct, out, error);
  };
  auto resolveSlot = [](const std::string &structPath,
                        const std::string &fieldName,
                        primec::ir_lowerer::StructSlotFieldInfo &out) {
    if (structPath == "/pkg/Container" && fieldName == "slot") {
      out.name = "slot";
      out.slotOffset = 4;
      out.slotCount = 1;
      out.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
      return true;
    }
    return false;
  };

  primec::ir_lowerer::UninitializedFieldStorageAccessInfo out;
  bool resolved = false;
  std::string error;
  REQUIRE(primec::ir_lowerer::resolveUninitializedFieldStorageAccess(
      storageExpr,
      locals,
      findField,
      resolveNamespacePrefix,
      resolveTypeInfo,
      resolveSlot,
      out,
      resolved,
      error));
  CHECK(resolved);
  CHECK(error.empty());
  CHECK(out.receiver == &receiverIt->second);
  CHECK(out.fieldSlot.slotOffset == 4);
  CHECK(out.typeInfo.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int64);

  storageExpr.name = "missing";
  REQUIRE(primec::ir_lowerer::resolveUninitializedFieldStorageAccess(
      storageExpr,
      locals,
      findField,
      resolveNamespacePrefix,
      resolveTypeInfo,
      resolveSlot,
      out,
      resolved,
      error));
  CHECK_FALSE(resolved);

  storageExpr.name = "slot";
  auto resolveMissingSlot = [](const std::string &, const std::string &, primec::ir_lowerer::StructSlotFieldInfo &) {
    return false;
  };
  CHECK_FALSE(primec::ir_lowerer::resolveUninitializedFieldStorageAccess(
      storageExpr,
      locals,
      findField,
      resolveNamespacePrefix,
      resolveTypeInfo,
      resolveMissingSlot,
      out,
      resolved,
      error));
}

TEST_CASE("ir lowerer uninitialized type helpers resolve unified storage access") {
  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo localStorage;
  localStorage.isUninitializedStorage = true;
  localStorage.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  localStorage.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  locals.emplace("slot", localStorage);
  const auto localIt = locals.find("slot");
  REQUIRE(localIt != locals.end());

  primec::ir_lowerer::LocalInfo receiver;
  receiver.structTypeName = "/pkg/Container";
  locals.emplace("self", receiver);
  const auto receiverIt = locals.find("self");
  REQUIRE(receiverIt != locals.end());

  auto findField = [](const std::string &structPath,
                      const std::string &fieldName,
                      std::string &typeTemplateArgOut) {
    if (structPath == "/pkg/Container" && fieldName == "slot") {
      typeTemplateArgOut = "i64";
      return true;
    }
    return false;
  };
  auto resolveNamespacePrefix = [](const std::string &structPath) {
    if (structPath == "/pkg/Container") {
      return std::string("/pkg");
    }
    return std::string();
  };
  auto resolveTypeInfo = [](const std::string &typeText,
                            const std::string &namespacePrefix,
                            primec::ir_lowerer::UninitializedTypeInfo &out) {
    const auto resolveStruct = [](const std::string &, const std::string &, std::string &) { return false; };
    std::string error;
    return primec::ir_lowerer::resolveUninitializedTypeInfo(typeText, namespacePrefix, resolveStruct, out, error);
  };
  auto resolveSlot = [](const std::string &structPath,
                        const std::string &fieldName,
                        primec::ir_lowerer::StructSlotFieldInfo &out) {
    if (structPath == "/pkg/Container" && fieldName == "slot") {
      out.name = "slot";
      out.slotOffset = 4;
      out.slotCount = 1;
      out.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
      return true;
    }
    return false;
  };

  primec::ir_lowerer::UninitializedStorageAccessInfo out;
  bool resolved = false;
  std::string error;

  primec::Expr localExpr;
  localExpr.kind = primec::Expr::Kind::Name;
  localExpr.name = "slot";
  REQUIRE(primec::ir_lowerer::resolveUninitializedStorageAccess(
      localExpr, locals, findField, resolveNamespacePrefix, resolveTypeInfo, resolveSlot, out, resolved, error));
  CHECK(resolved);
  CHECK(out.location == primec::ir_lowerer::UninitializedStorageAccessInfo::Location::Local);
  CHECK(out.local == &localIt->second);
  CHECK(out.typeInfo.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int64);

  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "self";
  primec::Expr fieldExpr;
  fieldExpr.kind = primec::Expr::Kind::Call;
  fieldExpr.isFieldAccess = true;
  fieldExpr.name = "slot";
  fieldExpr.args.push_back(receiverExpr);
  REQUIRE(primec::ir_lowerer::resolveUninitializedStorageAccess(
      fieldExpr, locals, findField, resolveNamespacePrefix, resolveTypeInfo, resolveSlot, out, resolved, error));
  CHECK(resolved);
  CHECK(out.location == primec::ir_lowerer::UninitializedStorageAccessInfo::Location::Field);
  CHECK(out.receiver == &receiverIt->second);
  CHECK(out.fieldSlot.slotOffset == 4);

  localExpr.name = "missing";
  REQUIRE(primec::ir_lowerer::resolveUninitializedStorageAccess(
      localExpr, locals, findField, resolveNamespacePrefix, resolveTypeInfo, resolveSlot, out, resolved, error));
  CHECK_FALSE(resolved);

  auto resolveMissingSlot = [](const std::string &, const std::string &, primec::ir_lowerer::StructSlotFieldInfo &) {
    return false;
  };
  CHECK_FALSE(primec::ir_lowerer::resolveUninitializedStorageAccess(fieldExpr,
                                                                    locals,
                                                                    findField,
                                                                    resolveNamespacePrefix,
                                                                    resolveTypeInfo,
                                                                    resolveMissingSlot,
                                                                    out,
                                                                    resolved,
                                                                    error));
}

TEST_CASE("ir lowerer uninitialized type helpers resolve unified storage with field bindings") {
  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo localStorage;
  localStorage.isUninitializedStorage = true;
  localStorage.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  localStorage.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  locals.emplace("slot", localStorage);
  const auto localIt = locals.find("slot");
  REQUIRE(localIt != locals.end());

  primec::ir_lowerer::LocalInfo receiver;
  receiver.structTypeName = "/pkg/Container";
  locals.emplace("self", receiver);
  const auto receiverIt = locals.find("self");
  REQUIRE(receiverIt != locals.end());

  auto collectFields = [](const std::string &structPath,
                          std::vector<primec::ir_lowerer::UninitializedFieldBindingInfo> &fieldsOut) {
    if (structPath != "/pkg/Container") {
      return false;
    }
    fieldsOut.clear();
    fieldsOut.push_back({"slot", "uninitialized", "i64", false});
    return true;
  };
  auto resolveNamespacePrefix = [](const std::string &structPath) {
    if (structPath == "/pkg/Container") {
      return std::string("/pkg");
    }
    return std::string();
  };
  auto resolveTypeInfo = [](const std::string &typeText,
                            const std::string &namespacePrefix,
                            primec::ir_lowerer::UninitializedTypeInfo &out) {
    const auto resolveStruct = [](const std::string &, const std::string &, std::string &) { return false; };
    std::string error;
    return primec::ir_lowerer::resolveUninitializedTypeInfo(typeText, namespacePrefix, resolveStruct, out, error);
  };
  auto resolveSlot = [](const std::string &structPath,
                        const std::string &fieldName,
                        primec::ir_lowerer::StructSlotFieldInfo &out) {
    if (structPath == "/pkg/Container" && fieldName == "slot") {
      out.name = "slot";
      out.slotOffset = 4;
      out.slotCount = 1;
      out.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
      return true;
    }
    return false;
  };

  primec::ir_lowerer::UninitializedStorageAccessInfo out;
  bool resolved = false;
  std::string error;

  primec::Expr localExpr;
  localExpr.kind = primec::Expr::Kind::Name;
  localExpr.name = "slot";
  REQUIRE(primec::ir_lowerer::resolveUninitializedStorageAccessWithFieldBindings(localExpr,
                                                                                  locals,
                                                                                  collectFields,
                                                                                  resolveNamespacePrefix,
                                                                                  resolveTypeInfo,
                                                                                  resolveSlot,
                                                                                  out,
                                                                                  resolved,
                                                                                  error));
  CHECK(resolved);
  CHECK(out.location == primec::ir_lowerer::UninitializedStorageAccessInfo::Location::Local);
  CHECK(out.local == &localIt->second);

  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "self";
  primec::Expr fieldExpr;
  fieldExpr.kind = primec::Expr::Kind::Call;
  fieldExpr.isFieldAccess = true;
  fieldExpr.name = "slot";
  fieldExpr.args.push_back(receiverExpr);
  REQUIRE(primec::ir_lowerer::resolveUninitializedStorageAccessWithFieldBindings(fieldExpr,
                                                                                  locals,
                                                                                  collectFields,
                                                                                  resolveNamespacePrefix,
                                                                                  resolveTypeInfo,
                                                                                  resolveSlot,
                                                                                  out,
                                                                                  resolved,
                                                                                  error));
  CHECK(resolved);
  CHECK(out.location == primec::ir_lowerer::UninitializedStorageAccessInfo::Location::Field);
  CHECK(out.receiver == &receiverIt->second);
  CHECK(out.fieldSlot.slotOffset == 4);

  localExpr.name = "missing";
  REQUIRE(primec::ir_lowerer::resolveUninitializedStorageAccessWithFieldBindings(localExpr,
                                                                                  locals,
                                                                                  collectFields,
                                                                                  resolveNamespacePrefix,
                                                                                  resolveTypeInfo,
                                                                                  resolveSlot,
                                                                                  out,
                                                                                  resolved,
                                                                                  error));
  CHECK_FALSE(resolved);
}

TEST_CASE("ir lowerer uninitialized type helpers resolve unified storage from definitions") {
  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo localStorage;
  localStorage.isUninitializedStorage = true;
  localStorage.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  localStorage.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  locals.emplace("slot", localStorage);
  const auto localIt = locals.find("slot");
  REQUIRE(localIt != locals.end());

  primec::ir_lowerer::LocalInfo receiver;
  receiver.structTypeName = "/pkg/Container";
  locals.emplace("self", receiver);
  const auto receiverIt = locals.find("self");
  REQUIRE(receiverIt != locals.end());

  primec::Definition containerDef;
  containerDef.fullPath = "/pkg/Container";
  containerDef.namespacePrefix = "/pkg";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/pkg/Container", &containerDef},
  };

  auto collectFields = [](const std::string &structPath,
                          std::vector<primec::ir_lowerer::UninitializedFieldBindingInfo> &fieldsOut) {
    if (structPath != "/pkg/Container") {
      return false;
    }
    fieldsOut.clear();
    fieldsOut.push_back({"slot", "uninitialized", "MyStruct", false});
    return true;
  };
  auto resolveTypeInfo = [](const std::string &typeText,
                            const std::string &namespacePrefix,
                            primec::ir_lowerer::UninitializedTypeInfo &out) {
    out = primec::ir_lowerer::UninitializedTypeInfo{};
    if (typeText == "MyStruct" && namespacePrefix == "/pkg") {
      out.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
      out.structPath = "/pkg/MyStruct";
      return true;
    }
    return false;
  };
  auto resolveSlot = [](const std::string &structPath,
                        const std::string &fieldName,
                        primec::ir_lowerer::StructSlotFieldInfo &out) {
    if (structPath == "/pkg/Container" && fieldName == "slot") {
      out.name = "slot";
      out.slotOffset = 4;
      out.slotCount = 1;
      return true;
    }
    return false;
  };

  primec::ir_lowerer::UninitializedStorageAccessInfo out;
  bool resolved = false;
  std::string error;

  primec::Expr localExpr;
  localExpr.kind = primec::Expr::Kind::Name;
  localExpr.name = "slot";
  REQUIRE(primec::ir_lowerer::resolveUninitializedStorageAccessFromDefinitions(localExpr,
                                                                                locals,
                                                                                collectFields,
                                                                                defMap,
                                                                                resolveTypeInfo,
                                                                                resolveSlot,
                                                                                out,
                                                                                resolved,
                                                                                error));
  CHECK(resolved);
  CHECK(out.location == primec::ir_lowerer::UninitializedStorageAccessInfo::Location::Local);
  CHECK(out.local == &localIt->second);

  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "self";
  primec::Expr fieldExpr;
  fieldExpr.kind = primec::Expr::Kind::Call;
  fieldExpr.isFieldAccess = true;
  fieldExpr.name = "slot";
  fieldExpr.args.push_back(receiverExpr);
  REQUIRE(primec::ir_lowerer::resolveUninitializedStorageAccessFromDefinitions(fieldExpr,
                                                                                locals,
                                                                                collectFields,
                                                                                defMap,
                                                                                resolveTypeInfo,
                                                                                resolveSlot,
                                                                                out,
                                                                                resolved,
                                                                                error));
  CHECK(resolved);
  CHECK(out.location == primec::ir_lowerer::UninitializedStorageAccessInfo::Location::Field);
  CHECK(out.receiver == &receiverIt->second);
  CHECK(out.typeInfo.structPath == "/pkg/MyStruct");
}

TEST_CASE("ir lowerer uninitialized type helpers resolve unified storage from definition field index") {
  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo localStorage;
  localStorage.isUninitializedStorage = true;
  localStorage.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  localStorage.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  locals.emplace("slot", localStorage);

  primec::ir_lowerer::LocalInfo receiver;
  receiver.structTypeName = "/pkg/Container";
  locals.emplace("self", receiver);
  const auto receiverIt = locals.find("self");
  REQUIRE(receiverIt != locals.end());

  primec::Definition containerDef;
  containerDef.fullPath = "/pkg/Container";
  containerDef.namespacePrefix = "/pkg";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/pkg/Container", &containerDef},
  };
  primec::ir_lowerer::UninitializedFieldBindingIndex fieldIndex;
  fieldIndex["/pkg/Container"] = {
      {"slot", "uninitialized", "MyStruct", false},
  };

  auto resolveTypeInfo = [](const std::string &typeText,
                            const std::string &namespacePrefix,
                            primec::ir_lowerer::UninitializedTypeInfo &out) {
    out = primec::ir_lowerer::UninitializedTypeInfo{};
    if (typeText == "MyStruct" && namespacePrefix == "/pkg") {
      out.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
      out.structPath = "/pkg/MyStruct";
      return true;
    }
    return false;
  };
  auto resolveSlot = [](const std::string &structPath,
                        const std::string &fieldName,
                        primec::ir_lowerer::StructSlotFieldInfo &out) {
    if (structPath == "/pkg/Container" && fieldName == "slot") {
      out.name = "slot";
      out.slotOffset = 3;
      out.slotCount = 1;
      return true;
    }
    return false;
  };

  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "self";
  primec::Expr fieldExpr;
  fieldExpr.kind = primec::Expr::Kind::Call;
  fieldExpr.isFieldAccess = true;
  fieldExpr.name = "slot";
  fieldExpr.args.push_back(receiverExpr);

  primec::ir_lowerer::UninitializedStorageAccessInfo out;
  bool resolved = false;
  std::string error;
  REQUIRE(primec::ir_lowerer::resolveUninitializedStorageAccessFromDefinitionFieldIndex(fieldExpr,
                                                                                         locals,
                                                                                         fieldIndex,
                                                                                         defMap,
                                                                                         resolveTypeInfo,
                                                                                         resolveSlot,
                                                                                         out,
                                                                                         resolved,
                                                                                         error));
  CHECK(resolved);
  CHECK(out.location == primec::ir_lowerer::UninitializedStorageAccessInfo::Location::Field);
  CHECK(out.receiver == &receiverIt->second);
  CHECK(out.fieldSlot.slotOffset == 3);
  CHECK(out.typeInfo.structPath == "/pkg/MyStruct");
}

TEST_CASE("ir lowerer uninitialized type helpers build storage resolver from definition field index") {
  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo receiver;
  receiver.structTypeName = "/pkg/Container";
  locals.emplace("self", receiver);
  const auto receiverIt = locals.find("self");
  REQUIRE(receiverIt != locals.end());

  primec::Definition containerDef;
  containerDef.fullPath = "/pkg/Container";
  containerDef.namespacePrefix = "/pkg";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/pkg/Container", &containerDef},
  };
  primec::ir_lowerer::UninitializedFieldBindingIndex fieldIndex;
  fieldIndex["/pkg/Container"] = {
      {"slot", "uninitialized", "MyStruct", false},
  };
  auto resolveTypeInfo = [](const std::string &typeText,
                            const std::string &namespacePrefix,
                            primec::ir_lowerer::UninitializedTypeInfo &out) {
    out = primec::ir_lowerer::UninitializedTypeInfo{};
    if (typeText == "MyStruct" && namespacePrefix == "/pkg") {
      out.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
      out.structPath = "/pkg/MyStruct";
      return true;
    }
    return false;
  };
  auto resolveSlot = [](const std::string &structPath,
                        const std::string &fieldName,
                        primec::ir_lowerer::StructSlotFieldInfo &out) {
    if (structPath == "/pkg/Container" && fieldName == "slot") {
      out.name = "slot";
      out.slotOffset = 3;
      out.slotCount = 1;
      return true;
    }
    return false;
  };

  std::string error;
  auto resolveUninitializedStorage =
      primec::ir_lowerer::makeResolveUninitializedStorageAccessFromDefinitionFieldIndex(
          fieldIndex, defMap, resolveTypeInfo, resolveSlot, error);

  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "self";
  primec::Expr fieldExpr;
  fieldExpr.kind = primec::Expr::Kind::Call;
  fieldExpr.isFieldAccess = true;
  fieldExpr.name = "slot";
  fieldExpr.args.push_back(receiverExpr);

  primec::ir_lowerer::UninitializedStorageAccessInfo out;
  bool resolved = false;
  REQUIRE(resolveUninitializedStorage(fieldExpr, locals, out, resolved));
  CHECK(resolved);
  CHECK(out.location == primec::ir_lowerer::UninitializedStorageAccessInfo::Location::Field);
  CHECK(out.receiver == &receiverIt->second);
  CHECK(out.fieldSlot.slotOffset == 3);
  CHECK(out.typeInfo.structPath == "/pkg/MyStruct");
}

TEST_CASE("ir lowerer uninitialized type helpers build bundled uninitialized resolution adapters") {
  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo receiver;
  receiver.structTypeName = "/pkg/Container";
  locals.emplace("self", receiver);
  const auto receiverIt = locals.find("self");
  REQUIRE(receiverIt != locals.end());

  primec::Definition containerDef;
  containerDef.fullPath = "/pkg/Container";
  containerDef.namespacePrefix = "/pkg";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/pkg/Container", &containerDef},
  };
  primec::ir_lowerer::UninitializedFieldBindingIndex fieldIndex;
  fieldIndex["/pkg/Container"] = {
      {"slot", "uninitialized", "MyStruct", false},
  };
  auto resolveStruct = [](const std::string &typeName,
                          const std::string &namespacePrefix,
                          std::string &resolvedOut) {
    if (typeName == "MyStruct" && namespacePrefix == "/pkg") {
      resolvedOut = "/pkg/MyStruct";
      return true;
    }
    return false;
  };
  auto resolveSlot = [](const std::string &structPath,
                        const std::string &fieldName,
                        primec::ir_lowerer::StructSlotFieldInfo &out) {
    if (structPath == "/pkg/Container" && fieldName == "slot") {
      out.name = "slot";
      out.slotOffset = 3;
      out.slotCount = 1;
      return true;
    }
    return false;
  };

  std::string error;
  auto resolveExprPath = [](const primec::Expr &) { return std::string(); };
  auto adapters = primec::ir_lowerer::makeUninitializedResolutionAdapters(
      resolveStruct, resolveExprPath, fieldIndex, defMap, resolveSlot, error);

  primec::ir_lowerer::UninitializedTypeInfo typeInfo;
  REQUIRE(adapters.resolveUninitializedTypeInfo("MyStruct", "/pkg", typeInfo));
  CHECK(typeInfo.structPath == "/pkg/MyStruct");

  CHECK_FALSE(adapters.resolveUninitializedTypeInfo("Thing<i32>", "/pkg", typeInfo));
  CHECK(error == "native backend does not support uninitialized storage for type: Thing<i32>");

  error.clear();
  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "self";
  primec::Expr fieldExpr;
  fieldExpr.kind = primec::Expr::Kind::Call;
  fieldExpr.isFieldAccess = true;
  fieldExpr.name = "slot";
  fieldExpr.args.push_back(receiverExpr);

  primec::ir_lowerer::UninitializedStorageAccessInfo out;
  bool resolved = false;
  REQUIRE(adapters.resolveUninitializedStorage(fieldExpr, locals, out, resolved));
  CHECK(resolved);
  CHECK(out.location == primec::ir_lowerer::UninitializedStorageAccessInfo::Location::Field);
  CHECK(out.receiver == &receiverIt->second);
  CHECK(out.fieldSlot.slotOffset == 3);
  CHECK(out.typeInfo.structPath == "/pkg/MyStruct");

  primec::Expr selfExpr;
  selfExpr.kind = primec::Expr::Kind::Name;
  selfExpr.name = "self";
  CHECK(adapters.inferStructExprPath(selfExpr, locals) == "/pkg/Container");

  primec::Expr missingExpr;
  missingExpr.kind = primec::Expr::Kind::Name;
  missingExpr.name = "missing";
  CHECK(adapters.inferStructExprPath(missingExpr, locals).empty());
}

TEST_CASE("ir lowerer binding transform helpers classify qualifiers and mutability") {
  CHECK(primec::ir_lowerer::isBindingQualifierName("public"));
  CHECK(primec::ir_lowerer::isBindingQualifierName("mut"));
  CHECK_FALSE(primec::ir_lowerer::isBindingQualifierName("array"));

  primec::Expr bindingExpr;
  primec::Transform mutTransform;
  mutTransform.name = "mut";
  bindingExpr.transforms.push_back(mutTransform);
  CHECK(primec::ir_lowerer::isBindingMutable(bindingExpr));

  bindingExpr.transforms.clear();
  CHECK_FALSE(primec::ir_lowerer::isBindingMutable(bindingExpr));
}

TEST_CASE("ir lowerer binding transform helpers detect explicit binding types") {
  primec::Expr expr;

  primec::Transform effects;
  effects.name = "effects";
  effects.arguments = {"io_out"};
  expr.transforms.push_back(effects);

  primec::Transform qualifier;
  qualifier.name = "public";
  expr.transforms.push_back(qualifier);

  CHECK_FALSE(primec::ir_lowerer::hasExplicitBindingTypeTransform(expr));

  primec::Transform typed;
  typed.name = "array";
  typed.templateArgs = {"i64"};
  expr.transforms.push_back(typed);
  CHECK(primec::ir_lowerer::hasExplicitBindingTypeTransform(expr));
}

TEST_CASE("ir lowerer binding transform helpers extract first binding type transform") {
  primec::Expr expr;

  primec::Transform effects;
  effects.name = "effects";
  effects.arguments = {"io_out"};
  expr.transforms.push_back(effects);

  primec::Transform qualifier;
  qualifier.name = "mut";
  expr.transforms.push_back(qualifier);

  primec::Transform withArgs;
  withArgs.name = "align_bytes";
  withArgs.arguments = {"16"};
  expr.transforms.push_back(withArgs);

  primec::Transform typed;
  typed.name = "Result";
  typed.templateArgs = {"i32", "FileError"};
  expr.transforms.push_back(typed);

  std::string typeName;
  std::vector<std::string> templateArgs;
  REQUIRE(primec::ir_lowerer::extractFirstBindingTypeTransform(expr, typeName, templateArgs));
  CHECK(typeName == "Result");
  REQUIRE(templateArgs.size() == 2);
  CHECK(templateArgs[0] == "i32");
  CHECK(templateArgs[1] == "FileError");

  expr.transforms.clear();
  expr.transforms.push_back(effects);
  expr.transforms.push_back(qualifier);
  CHECK_FALSE(primec::ir_lowerer::extractFirstBindingTypeTransform(expr, typeName, templateArgs));
  CHECK(typeName.empty());
  CHECK(templateArgs.empty());
}

TEST_CASE("ir lowerer binding transform helpers extract uninitialized template args") {
  primec::Expr expr;
  primec::Transform uninitialized;
  uninitialized.name = "uninitialized";
  uninitialized.templateArgs = {"i64"};
  expr.transforms.push_back(uninitialized);

  std::string typeText;
  CHECK(primec::ir_lowerer::extractUninitializedTemplateArg(expr, typeText));
  CHECK(typeText == "i64");

  expr.transforms.clear();
  uninitialized.templateArgs = {"i64", "i32"};
  expr.transforms.push_back(uninitialized);
  CHECK_FALSE(primec::ir_lowerer::extractUninitializedTemplateArg(expr, typeText));
}

TEST_CASE("ir lowerer binding transform helpers detect entry args params") {
  primec::Expr param;
  primec::Transform mutTransform;
  mutTransform.name = "mut";
  param.transforms.push_back(mutTransform);

  primec::Transform arrayTransform;
  arrayTransform.name = "array";
  arrayTransform.templateArgs = {"string"};
  param.transforms.push_back(arrayTransform);
  CHECK(primec::ir_lowerer::isEntryArgsParam(param));

  arrayTransform.templateArgs = {"i64"};
  param.transforms.back() = arrayTransform;
  CHECK_FALSE(primec::ir_lowerer::isEntryArgsParam(param));
}

TEST_CASE("ir lowerer binding type helpers classify binding kind and string/fileerror types") {
  primec::Expr vectorExpr;
  primec::Transform vectorTransform;
  vectorTransform.name = "vector";
  vectorTransform.templateArgs = {"i64"};
  vectorExpr.transforms.push_back(vectorTransform);
  CHECK(primec::ir_lowerer::bindingKindFromTransforms(vectorExpr) == primec::ir_lowerer::LocalInfo::Kind::Vector);

  primec::Expr defaultExpr;
  CHECK(primec::ir_lowerer::bindingKindFromTransforms(defaultExpr) == primec::ir_lowerer::LocalInfo::Kind::Value);

  primec::Expr stringExpr;
  primec::Transform qualifier;
  qualifier.name = "mut";
  stringExpr.transforms.push_back(qualifier);
  primec::Transform stringTransform;
  stringTransform.name = "string";
  stringExpr.transforms.push_back(stringTransform);
  CHECK(primec::ir_lowerer::isStringBindingType(stringExpr));

  primec::Expr fileErrorExpr;
  fileErrorExpr.transforms.push_back(qualifier);
  primec::Transform fileErrorTransform;
  fileErrorTransform.name = "FileError";
  fileErrorExpr.transforms.push_back(fileErrorTransform);
  CHECK(primec::ir_lowerer::isFileErrorBindingType(fileErrorExpr));
  CHECK_FALSE(primec::ir_lowerer::isFileErrorBindingType(stringExpr));
  CHECK(primec::ir_lowerer::isStringTypeName("string"));
  CHECK_FALSE(primec::ir_lowerer::isStringTypeName("i64"));
}

TEST_CASE("ir lowerer binding type helpers resolve value kinds from transforms") {
  primec::Expr pointerExpr;
  primec::Transform pointerTransform;
  pointerTransform.name = "Pointer";
  pointerTransform.templateArgs = {"i64"};
  pointerExpr.transforms.push_back(pointerTransform);
  CHECK(primec::ir_lowerer::bindingValueKindFromTransforms(pointerExpr, primec::ir_lowerer::LocalInfo::Kind::Pointer) ==
        primec::ir_lowerer::LocalInfo::ValueKind::Int64);

  primec::Expr mapExpr;
  primec::Transform mapTransform;
  mapTransform.name = "map";
  mapTransform.templateArgs = {"bool", "f64"};
  mapExpr.transforms.push_back(mapTransform);
  CHECK(primec::ir_lowerer::bindingValueKindFromTransforms(mapExpr, primec::ir_lowerer::LocalInfo::Kind::Map) ==
        primec::ir_lowerer::LocalInfo::ValueKind::Float64);

  primec::Expr resultExpr;
  primec::Transform resultTransform;
  resultTransform.name = "Result";
  resultTransform.templateArgs = {"i64", "FileError"};
  resultExpr.transforms.push_back(resultTransform);
  CHECK(primec::ir_lowerer::bindingValueKindFromTransforms(resultExpr, primec::ir_lowerer::LocalInfo::Kind::Value) ==
        primec::ir_lowerer::LocalInfo::ValueKind::Int64);

  primec::Expr defaultExpr;
  CHECK(primec::ir_lowerer::bindingValueKindFromTransforms(defaultExpr, primec::ir_lowerer::LocalInfo::Kind::Value) ==
        primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  CHECK(primec::ir_lowerer::bindingValueKindFromTransforms(defaultExpr, primec::ir_lowerer::LocalInfo::Kind::Array) ==
        primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
}

TEST_CASE("ir lowerer binding type helpers mark reference-to-array metadata") {
  primec::Expr referenceArrayExpr;
  primec::Transform referenceArrayTransform;
  referenceArrayTransform.name = "Reference";
  referenceArrayTransform.templateArgs = {"array<i64>"};
  referenceArrayExpr.transforms.push_back(referenceArrayTransform);

  primec::ir_lowerer::LocalInfo info;
  info.kind = primec::ir_lowerer::LocalInfo::Kind::Reference;
  info.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  primec::ir_lowerer::setReferenceArrayInfoFromTransforms(referenceArrayExpr, info);
  CHECK(info.referenceToArray);
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int64);

  primec::Expr referenceScalarExpr;
  primec::Transform referenceScalarTransform;
  referenceScalarTransform.name = "Reference";
  referenceScalarTransform.templateArgs = {"i64"};
  referenceScalarExpr.transforms.push_back(referenceScalarTransform);

  primec::ir_lowerer::LocalInfo scalarInfo;
  scalarInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Reference;
  scalarInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  primec::ir_lowerer::setReferenceArrayInfoFromTransforms(referenceScalarExpr, scalarInfo);
  CHECK_FALSE(scalarInfo.referenceToArray);
  CHECK(scalarInfo.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);

  primec::ir_lowerer::LocalInfo presetInfo;
  presetInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Reference;
  presetInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  primec::ir_lowerer::setReferenceArrayInfoFromTransforms(referenceArrayExpr, presetInfo);
  CHECK(presetInfo.referenceToArray);
  CHECK(presetInfo.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
}

TEST_CASE("ir lowerer binding type helpers build setup adapter factories") {
  auto bindingKind = primec::ir_lowerer::makeBindingKindFromTransforms();
  auto isStringBinding = primec::ir_lowerer::makeIsStringBindingType();
  auto isFileErrorBinding = primec::ir_lowerer::makeIsFileErrorBindingType();
  auto bindingValueKind = primec::ir_lowerer::makeBindingValueKindFromTransforms();
  auto setReferenceArrayInfo = primec::ir_lowerer::makeSetReferenceArrayInfoFromTransforms();

  primec::Expr vectorExpr;
  primec::Transform vectorTransform;
  vectorTransform.name = "vector";
  vectorTransform.templateArgs = {"i64"};
  vectorExpr.transforms.push_back(vectorTransform);
  CHECK(bindingKind(vectorExpr) == primec::ir_lowerer::LocalInfo::Kind::Vector);

  primec::Expr stringExpr;
  primec::Transform stringTransform;
  stringTransform.name = "string";
  stringExpr.transforms.push_back(stringTransform);
  CHECK(isStringBinding(stringExpr));
  CHECK_FALSE(isFileErrorBinding(stringExpr));

  primec::Expr mapExpr;
  primec::Transform mapTransform;
  mapTransform.name = "map";
  mapTransform.templateArgs = {"bool", "f64"};
  mapExpr.transforms.push_back(mapTransform);
  CHECK(bindingValueKind(mapExpr, primec::ir_lowerer::LocalInfo::Kind::Map) ==
        primec::ir_lowerer::LocalInfo::ValueKind::Float64);

  primec::Expr referenceArrayExpr;
  primec::Transform referenceArrayTransform;
  referenceArrayTransform.name = "Reference";
  referenceArrayTransform.templateArgs = {"array<i64>"};
  referenceArrayExpr.transforms.push_back(referenceArrayTransform);

  primec::ir_lowerer::LocalInfo info;
  info.kind = primec::ir_lowerer::LocalInfo::Kind::Reference;
  info.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  setReferenceArrayInfo(referenceArrayExpr, info);
  CHECK(info.referenceToArray);
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int64);
}

TEST_CASE("ir lowerer binding type helpers build bundled setup adapters") {
  auto adapters = primec::ir_lowerer::makeBindingTypeAdapters();

  primec::Expr vectorExpr;
  primec::Transform vectorTransform;
  vectorTransform.name = "vector";
  vectorTransform.templateArgs = {"i64"};
  vectorExpr.transforms.push_back(vectorTransform);
  CHECK(adapters.bindingKind(vectorExpr) == primec::ir_lowerer::LocalInfo::Kind::Vector);

  primec::Expr stringExpr;
  primec::Transform stringTransform;
  stringTransform.name = "string";
  stringExpr.transforms.push_back(stringTransform);
  CHECK(adapters.isStringBinding(stringExpr));
  CHECK_FALSE(adapters.isFileErrorBinding(stringExpr));

  primec::Expr mapExpr;
  primec::Transform mapTransform;
  mapTransform.name = "map";
  mapTransform.templateArgs = {"bool", "f64"};
  mapExpr.transforms.push_back(mapTransform);
  CHECK(adapters.bindingValueKind(mapExpr, primec::ir_lowerer::LocalInfo::Kind::Map) ==
        primec::ir_lowerer::LocalInfo::ValueKind::Float64);

  primec::ir_lowerer::LocalInfo info;
  info.kind = primec::ir_lowerer::LocalInfo::Kind::Reference;
  info.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  primec::Expr referenceArrayExpr;
  primec::Transform referenceArrayTransform;
  referenceArrayTransform.name = "Reference";
  referenceArrayTransform.templateArgs = {"array<i64>"};
  referenceArrayExpr.transforms.push_back(referenceArrayTransform);
  adapters.setReferenceArrayInfo(referenceArrayExpr, info);
  CHECK(info.referenceToArray);
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int64);
}

TEST_CASE("ir lowerer count access helpers classify entry args and count calls") {
  primec::Definition entryDef;
  bool hasEntryArgs = true;
  std::string entryArgsName = "stale";
  std::string error;
  REQUIRE(primec::ir_lowerer::resolveEntryArgsParameter(entryDef, hasEntryArgs, entryArgsName, error));
  CHECK_FALSE(hasEntryArgs);
  CHECK(entryArgsName.empty());
  CHECK(error.empty());

  primec::Expr entryParam;
  entryParam.name = "argv";
  primec::Transform arrayTransform;
  arrayTransform.name = "array";
  arrayTransform.templateArgs = {"string"};
  entryParam.transforms.push_back(arrayTransform);
  entryDef.parameters = {entryParam};
  REQUIRE(primec::ir_lowerer::resolveEntryArgsParameter(entryDef, hasEntryArgs, entryArgsName, error));
  CHECK(hasEntryArgs);
  CHECK(entryArgsName == "argv");
  CHECK(error.empty());

  primec::Expr extraParam = entryParam;
  extraParam.name = "extra";
  entryDef.parameters = {entryParam, extraParam};
  CHECK_FALSE(primec::ir_lowerer::resolveEntryArgsParameter(entryDef, hasEntryArgs, entryArgsName, error));
  CHECK(error == "native backend only supports a single array<string> entry parameter");

  error.clear();
  primec::Expr badTypeParam;
  badTypeParam.name = "argv";
  primec::Transform badTypeTransform;
  badTypeTransform.name = "array";
  badTypeTransform.templateArgs = {"i64"};
  badTypeParam.transforms.push_back(badTypeTransform);
  entryDef.parameters = {badTypeParam};
  CHECK_FALSE(primec::ir_lowerer::resolveEntryArgsParameter(entryDef, hasEntryArgs, entryArgsName, error));
  CHECK(error == "native backend entry parameter must be array<string>");

  error.clear();
  entryParam.args.push_back(primec::Expr{});
  entryDef.parameters = {entryParam};
  CHECK_FALSE(primec::ir_lowerer::resolveEntryArgsParameter(entryDef, hasEntryArgs, entryArgsName, error));
  CHECK(error == "native backend does not allow entry parameter defaults");

  primec::ir_lowerer::LocalMap locals;
  primec::Expr entryName;
  entryName.kind = primec::Expr::Kind::Name;
  entryName.name = "argv";
  CHECK(primec::ir_lowerer::isEntryArgsName(entryName, locals, true, "argv"));

  primec::ir_lowerer::LocalInfo shadowed;
  shadowed.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  locals.emplace("argv", shadowed);
  CHECK_FALSE(primec::ir_lowerer::isEntryArgsName(entryName, locals, true, "argv"));
  locals.erase("argv");

  primec::Expr countEntry;
  countEntry.kind = primec::Expr::Kind::Call;
  countEntry.name = "count";
  countEntry.args = {entryName};
  CHECK(primec::ir_lowerer::isArrayCountCall(countEntry, locals, true, "argv"));

  primec::ir_lowerer::LocalInfo arrayInfo;
  arrayInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  locals.emplace("arr", arrayInfo);
  primec::Expr arrayName;
  arrayName.kind = primec::Expr::Kind::Name;
  arrayName.name = "arr";
  countEntry.args = {arrayName};
  CHECK(primec::ir_lowerer::isArrayCountCall(countEntry, locals, false, "argv"));

  primec::ir_lowerer::LocalInfo refInfo;
  refInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Reference;
  refInfo.referenceToArray = true;
  locals.emplace("arrRef", refInfo);
  primec::Expr refName;
  refName.kind = primec::Expr::Kind::Name;
  refName.name = "arrRef";
  countEntry.args = {refName};
  CHECK(primec::ir_lowerer::isArrayCountCall(countEntry, locals, false, "argv"));

  primec::Expr vectorCall;
  vectorCall.kind = primec::Expr::Kind::Call;
  vectorCall.name = "vector";
  vectorCall.templateArgs = {"i64"};
  countEntry.args = {vectorCall};
  CHECK(primec::ir_lowerer::isArrayCountCall(countEntry, locals, false, "argv"));
}

TEST_CASE("ir lowerer count access helpers build bundled entry count setup") {
  primec::Definition entryDef;
  primec::ir_lowerer::EntryCountAccessSetup setup;
  std::string error;
  REQUIRE(primec::ir_lowerer::buildEntryCountAccessSetup(entryDef, setup, error));
  CHECK_FALSE(setup.hasEntryArgs);
  CHECK(setup.entryArgsName.empty());

  primec::Expr entryName;
  entryName.kind = primec::Expr::Kind::Name;
  entryName.name = "argv";
  primec::ir_lowerer::LocalMap locals;
  CHECK_FALSE(setup.classifiers.isEntryArgsName(entryName, locals));

  primec::Expr entryParam;
  entryParam.name = "argv";
  primec::Transform arrayTransform;
  arrayTransform.name = "array";
  arrayTransform.templateArgs = {"string"};
  entryParam.transforms.push_back(arrayTransform);
  entryDef.parameters = {entryParam};

  error.clear();
  REQUIRE(primec::ir_lowerer::buildEntryCountAccessSetup(entryDef, setup, error));
  CHECK(setup.hasEntryArgs);
  CHECK(setup.entryArgsName == "argv");
  CHECK(setup.classifiers.isEntryArgsName(entryName, locals));

  primec::Expr countEntry;
  countEntry.kind = primec::Expr::Kind::Call;
  countEntry.name = "count";
  countEntry.args = {entryName};
  CHECK(setup.classifiers.isArrayCountCall(countEntry, locals));

  primec::Expr extraParam = entryParam;
  extraParam.name = "extra";
  entryDef.parameters = {entryParam, extraParam};
  CHECK_FALSE(primec::ir_lowerer::buildEntryCountAccessSetup(entryDef, setup, error));
  CHECK(error == "native backend only supports a single array<string> entry parameter");
}

TEST_CASE("ir lowerer count access helpers classify capacity and string count") {
  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo vecInfo;
  vecInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Vector;
  locals.emplace("values", vecInfo);

  primec::Expr valuesName;
  valuesName.kind = primec::Expr::Kind::Name;
  valuesName.name = "values";

  primec::Expr capacityCall;
  capacityCall.kind = primec::Expr::Kind::Call;
  capacityCall.name = "capacity";
  capacityCall.args = {valuesName};
  CHECK(primec::ir_lowerer::isVectorCapacityCall(capacityCall, locals));

  primec::Expr stringCount;
  stringCount.kind = primec::Expr::Kind::Call;
  stringCount.name = "count";
  primec::Expr literal;
  literal.kind = primec::Expr::Kind::StringLiteral;
  literal.stringValue = "\"ok\"utf8";
  stringCount.args = {literal};
  CHECK(primec::ir_lowerer::isStringCountCall(stringCount, locals));

  primec::ir_lowerer::LocalInfo stringInfo;
  stringInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::String;
  locals.emplace("text", stringInfo);
  primec::Expr textName;
  textName.kind = primec::Expr::Kind::Name;
  textName.name = "text";
  stringCount.args = {textName};
  CHECK(primec::ir_lowerer::isStringCountCall(stringCount, locals));

  capacityCall.name = "count";
  CHECK_FALSE(primec::ir_lowerer::isVectorCapacityCall(capacityCall, locals));
}

TEST_CASE("ir lowerer count access helpers emit string count calls") {
  using Result = primec::ir_lowerer::StringCountCallEmitResult;

  primec::Expr countCall;
  countCall.kind = primec::Expr::Kind::Call;
  countCall.name = "count";
  primec::Expr target;
  target.kind = primec::Expr::Kind::Name;
  target.name = "text";
  countCall.args = {target};

  primec::ir_lowerer::LocalMap locals;
  std::string error;
  int32_t emittedLength = -1;

  CHECK(primec::ir_lowerer::tryEmitStringCountCall(
            countCall,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return false; },
            [&](int32_t) {},
            error) == Result::NotHandled);

  countCall.args.clear();
  CHECK(primec::ir_lowerer::tryEmitStringCountCall(
            countCall,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return true; },
            [&](int32_t) {},
            error) == Result::Error);
  CHECK(error == "count requires exactly one argument");

  countCall.args = {target};
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitStringCountCall(
            countCall,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return false; },
            [&](int32_t) {},
            error) == Result::Error);
  CHECK(error == "native backend only supports count() on string literals or string bindings");

  error.clear();
  CHECK(primec::ir_lowerer::tryEmitStringCountCall(
            countCall,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &stringIndex, size_t &length) {
              stringIndex = 7;
              length = static_cast<size_t>(std::numeric_limits<int32_t>::max()) + 1;
              return true;
            },
            [&](int32_t) {},
            error) == Result::Error);
  CHECK(error == "native backend string too large for count()");

  error.clear();
  CHECK(primec::ir_lowerer::tryEmitStringCountCall(
            countCall,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &stringIndex, size_t &length) {
              stringIndex = 3;
              length = 42;
              return true;
            },
            [&](int32_t length) { emittedLength = length; },
            error) == Result::Emitted);
  CHECK(emittedLength == 42);
}

TEST_CASE("ir lowerer count access helpers emit count access calls") {
  using Result = primec::ir_lowerer::CountAccessCallEmitResult;

  primec::Expr targetName;
  targetName.kind = primec::Expr::Kind::Name;
  targetName.name = "values";

  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "noop";
  callExpr.args = {targetName};

  primec::ir_lowerer::LocalMap locals;
  std::vector<primec::Instruction> instructions;
  std::string error = "stale";

  CHECK(primec::ir_lowerer::tryEmitCountAccessCall(
            callExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            error) == Result::NotHandled);
  CHECK(error == "stale");
  CHECK(instructions.empty());

  instructions.clear();
  error.clear();
  callExpr.name = "count";
  callExpr.args = {targetName};
  int arrayEmitExprCalls = 0;
  CHECK(primec::ir_lowerer::tryEmitCountAccessCall(
            callExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              ++arrayEmitExprCalls;
              return true;
            },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            error) == Result::Emitted);
  CHECK(arrayEmitExprCalls == 0);
  REQUIRE(instructions.size() == 1);
  CHECK(instructions[0].op == primec::IrOpcode::PushArgc);

  instructions.clear();
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitCountAccessCall(
            callExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              ++arrayEmitExprCalls;
              instructions.push_back({primec::IrOpcode::PushI32, 9});
              return true;
            },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            error) == Result::Emitted);
  CHECK(arrayEmitExprCalls == 1);
  REQUIRE(instructions.size() == 2);
  CHECK(instructions[0].op == primec::IrOpcode::PushI32);
  CHECK(instructions[1].op == primec::IrOpcode::LoadIndirect);

  instructions.clear();
  error.clear();
  callExpr.name = "capacity";
  int capacityEmitExprCalls = 0;
  CHECK(primec::ir_lowerer::tryEmitCountAccessCall(
            callExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              ++capacityEmitExprCalls;
              instructions.push_back({primec::IrOpcode::AddressOfLocal, 2});
              return true;
            },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            error) == Result::Emitted);
  CHECK(capacityEmitExprCalls == 1);
  REQUIRE(instructions.size() == 4);
  CHECK(instructions[1].op == primec::IrOpcode::PushI64);
  CHECK(instructions[1].imm == primec::IrSlotBytes);
  CHECK(instructions[2].op == primec::IrOpcode::AddI64);
  CHECK(instructions[3].op == primec::IrOpcode::LoadIndirect);

  instructions.clear();
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitCountAccessCall(
            callExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            error) == Result::Error);
  CHECK(instructions.empty());

  instructions.clear();
  error.clear();
  callExpr.name = "count";
  CHECK(primec::ir_lowerer::tryEmitCountAccessCall(
            callExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &stringIndex, size_t &length) {
              stringIndex = 2;
              length = 11;
              return true;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            error) == Result::Emitted);
  REQUIRE(instructions.size() == 1);
  CHECK(instructions[0].op == primec::IrOpcode::PushI32);
  CHECK(instructions[0].imm == 11);

  instructions.clear();
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitCountAccessCall(
            callExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            error) == Result::Error);
  CHECK(error == "native backend only supports count() on string literals or string bindings");
}

TEST_CASE("ir lowerer count access helpers build count classifier adapters") {
  primec::ir_lowerer::LocalMap locals;
  auto isEntryArgsName = primec::ir_lowerer::makeIsEntryArgsName(true, "argv");
  auto isArrayCountCall = primec::ir_lowerer::makeIsArrayCountCall(true, "argv");
  auto isVectorCapacityCall = primec::ir_lowerer::makeIsVectorCapacityCall();
  auto isStringCountCall = primec::ir_lowerer::makeIsStringCountCall();

  primec::Expr entryName;
  entryName.kind = primec::Expr::Kind::Name;
  entryName.name = "argv";
  CHECK(isEntryArgsName(entryName, locals));

  primec::Expr countEntry;
  countEntry.kind = primec::Expr::Kind::Call;
  countEntry.name = "count";
  countEntry.args = {entryName};
  CHECK(isArrayCountCall(countEntry, locals));

  primec::ir_lowerer::LocalInfo vecInfo;
  vecInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Vector;
  locals.emplace("values", vecInfo);
  primec::Expr valuesName;
  valuesName.kind = primec::Expr::Kind::Name;
  valuesName.name = "values";
  primec::Expr capacityCall;
  capacityCall.kind = primec::Expr::Kind::Call;
  capacityCall.name = "capacity";
  capacityCall.args = {valuesName};
  CHECK(isVectorCapacityCall(capacityCall, locals));

  primec::Expr stringCount;
  stringCount.kind = primec::Expr::Kind::Call;
  stringCount.name = "count";
  primec::Expr literal;
  literal.kind = primec::Expr::Kind::StringLiteral;
  literal.stringValue = "\"ok\"utf8";
  stringCount.args = {literal};
  CHECK(isStringCountCall(stringCount, locals));
}

TEST_CASE("ir lowerer count access helpers build bundled classifiers") {
  primec::ir_lowerer::LocalMap locals;
  auto classifiers = primec::ir_lowerer::makeCountAccessClassifiers(true, "argv");

  primec::Expr entryName;
  entryName.kind = primec::Expr::Kind::Name;
  entryName.name = "argv";
  CHECK(classifiers.isEntryArgsName(entryName, locals));

  primec::Expr countEntry;
  countEntry.kind = primec::Expr::Kind::Call;
  countEntry.name = "count";
  countEntry.args = {entryName};
  CHECK(classifiers.isArrayCountCall(countEntry, locals));

  primec::ir_lowerer::LocalInfo vecInfo;
  vecInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Vector;
  locals.emplace("values", vecInfo);
  primec::Expr valuesName;
  valuesName.kind = primec::Expr::Kind::Name;
  valuesName.name = "values";
  primec::Expr capacityCall;
  capacityCall.kind = primec::Expr::Kind::Call;
  capacityCall.name = "capacity";
  capacityCall.args = {valuesName};
  CHECK(classifiers.isVectorCapacityCall(capacityCall, locals));
  CHECK_FALSE(classifiers.isStringCountCall(capacityCall, locals));
}

TEST_CASE("ir lowerer string literal helper interns string table values") {
  std::vector<std::string> stringTable;
  CHECK(primec::ir_lowerer::internLowererString("hello", stringTable) == 0);
  CHECK(primec::ir_lowerer::internLowererString("world", stringTable) == 1);
  CHECK(primec::ir_lowerer::internLowererString("hello", stringTable) == 0);
  REQUIRE(stringTable.size() == 2);
  CHECK(stringTable[0] == "hello");
  CHECK(stringTable[1] == "world");
}

TEST_CASE("ir lowerer string literal helper builds string interner") {
  std::vector<std::string> stringTable;
  auto internString = primec::ir_lowerer::makeInternLowererString(stringTable);

  CHECK(internString("hello") == 0);
  CHECK(internString("world") == 1);
  CHECK(internString("hello") == 0);
  REQUIRE(stringTable.size() == 2);
  CHECK(stringTable[0] == "hello");
  CHECK(stringTable[1] == "world");
}

TEST_CASE("ir lowerer string literal helper parses and validates encoding") {
  std::string decoded;
  std::string error;
  REQUIRE(primec::ir_lowerer::parseLowererStringLiteral("\"line\\n\"utf8", decoded, error));
  CHECK(decoded == "line\n");
  CHECK(error.empty());

  std::string asciiToken = "\"";
  asciiToken.push_back(static_cast<char>(0xC3));
  asciiToken.push_back(static_cast<char>(0xA5));
  asciiToken += "\"ascii";
  CHECK_FALSE(primec::ir_lowerer::parseLowererStringLiteral(asciiToken, decoded, error));
  CHECK(error == "ascii string literal contains non-ASCII characters");

  CHECK_FALSE(primec::ir_lowerer::parseLowererStringLiteral("\"missing_suffix\"", decoded, error));
  CHECK(error == "string literal requires utf8/ascii/raw_utf8/raw_ascii suffix");
}

TEST_CASE("ir lowerer string literal helper resolves string table targets") {
  std::vector<std::string> stringTable;
  auto internString = [&](const std::string &text) {
    for (size_t i = 0; i < stringTable.size(); ++i) {
      if (stringTable[i] == text) {
        return static_cast<int32_t>(i);
      }
    }
    stringTable.push_back(text);
    return static_cast<int32_t>(stringTable.size() - 1);
  };

  primec::Expr literalExpr;
  literalExpr.kind = primec::Expr::Kind::StringLiteral;
  literalExpr.stringValue = "\"hello\"utf8";

  int32_t stringIndex = -1;
  size_t length = 0;
  std::string error;
  REQUIRE(primec::ir_lowerer::resolveStringTableTarget(
      literalExpr, primec::ir_lowerer::LocalMap{}, stringTable, internString, stringIndex, length, error));
  CHECK(stringIndex == 0);
  CHECK(length == 5);
  CHECK(stringTable.size() == 1);

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo local;
  local.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::String;
  local.stringSource = primec::ir_lowerer::LocalInfo::StringSource::TableIndex;
  local.stringIndex = 0;
  locals.emplace("name", local);

  primec::Expr nameExpr;
  nameExpr.kind = primec::Expr::Kind::Name;
  nameExpr.name = "name";
  REQUIRE(primec::ir_lowerer::resolveStringTableTarget(
      nameExpr, locals, stringTable, internString, stringIndex, length, error));
  CHECK(stringIndex == 0);
  CHECK(length == 5);
}

TEST_CASE("ir lowerer string literal helper builds string table target resolver") {
  std::vector<std::string> stringTable;
  auto internString = [&](const std::string &text) {
    for (size_t i = 0; i < stringTable.size(); ++i) {
      if (stringTable[i] == text) {
        return static_cast<int32_t>(i);
      }
    }
    stringTable.push_back(text);
    return static_cast<int32_t>(stringTable.size() - 1);
  };
  std::string error;
  auto resolveStringTableTarget =
      primec::ir_lowerer::makeResolveStringTableTarget(stringTable, internString, error);

  primec::Expr literalExpr;
  literalExpr.kind = primec::Expr::Kind::StringLiteral;
  literalExpr.stringValue = "\"hello\"utf8";
  int32_t stringIndex = -1;
  size_t length = 0;
  REQUIRE(resolveStringTableTarget(literalExpr, primec::ir_lowerer::LocalMap{}, stringIndex, length));
  CHECK(stringIndex == 0);
  CHECK(length == 5);

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo local;
  local.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::String;
  local.stringSource = primec::ir_lowerer::LocalInfo::StringSource::TableIndex;
  local.stringIndex = 0;
  locals.emplace("name", local);
  primec::Expr nameExpr;
  nameExpr.kind = primec::Expr::Kind::Name;
  nameExpr.name = "name";
  REQUIRE(resolveStringTableTarget(nameExpr, locals, stringIndex, length));
  CHECK(stringIndex == 0);
  CHECK(length == 5);
}

TEST_CASE("ir lowerer string literal helper builds bundled context") {
  std::vector<std::string> stringTable;
  std::string error;
  auto helpers = primec::ir_lowerer::makeStringLiteralHelperContext(stringTable, error);

  CHECK(helpers.internString("hello") == 0);
  CHECK(helpers.internString("hello") == 0);

  primec::Expr literalExpr;
  literalExpr.kind = primec::Expr::Kind::StringLiteral;
  literalExpr.stringValue = "\"hello\"utf8";
  int32_t stringIndex = -1;
  size_t length = 0;
  REQUIRE(helpers.resolveStringTableTarget(literalExpr, primec::ir_lowerer::LocalMap{}, stringIndex, length));
  CHECK(stringIndex == 0);
  CHECK(length == 5);
  REQUIRE(stringTable.size() == 1);
  CHECK(stringTable[0] == "hello");

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo local;
  local.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::String;
  local.stringSource = primec::ir_lowerer::LocalInfo::StringSource::TableIndex;
  local.stringIndex = -1;
  locals.emplace("bad", local);

  primec::Expr nameExpr;
  nameExpr.kind = primec::Expr::Kind::Name;
  nameExpr.name = "bad";
  CHECK_FALSE(helpers.resolveStringTableTarget(nameExpr, locals, stringIndex, length));
  CHECK(error == "native backend missing string table data for: bad");
}

TEST_CASE("ir lowerer string literal helper reports table-target diagnostics") {
  std::vector<std::string> stringTable;
  auto internString = [&](const std::string &text) {
    stringTable.push_back(text);
    return static_cast<int32_t>(stringTable.size() - 1);
  };

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo local;
  local.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::String;
  local.stringSource = primec::ir_lowerer::LocalInfo::StringSource::TableIndex;
  local.stringIndex = -1;
  locals.emplace("bad", local);

  primec::Expr nameExpr;
  nameExpr.kind = primec::Expr::Kind::Name;
  nameExpr.name = "bad";
  int32_t stringIndex = -1;
  size_t length = 0;
  std::string error;
  CHECK_FALSE(primec::ir_lowerer::resolveStringTableTarget(
      nameExpr, locals, stringTable, internString, stringIndex, length, error));
  CHECK(error == "native backend missing string table data for: bad");

  error.clear();
  locals["bad"].stringIndex = 42;
  CHECK_FALSE(primec::ir_lowerer::resolveStringTableTarget(
      nameExpr, locals, stringTable, internString, stringIndex, length, error));
  CHECK(error == "native backend encountered invalid string table index");
}

TEST_CASE("ir lowerer template type parse helper splits nested template args") {
  std::vector<std::string> args;
  REQUIRE(primec::ir_lowerer::splitTemplateArgs(" i32 , map<string, array<i64>> , Result<bool, FileError> ", args));
  REQUIRE(args.size() == 3);
  CHECK(args[0] == "i32");
  CHECK(args[1] == "map<string, array<i64>>");
  CHECK(args[2] == "Result<bool, FileError>");

  CHECK_FALSE(primec::ir_lowerer::splitTemplateArgs("i32, map<string, i64", args));
  CHECK_FALSE(primec::ir_lowerer::splitTemplateArgs("i32>", args));
}

TEST_CASE("ir lowerer template type parse helper parses Result return type names") {
  bool hasValue = false;
  primec::ir_lowerer::LocalInfo::ValueKind valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  std::string errorType;

  REQUIRE(primec::ir_lowerer::parseResultTypeName("Result<FileError>", hasValue, valueKind, errorType));
  CHECK_FALSE(hasValue);
  CHECK(valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
  CHECK(errorType == "FileError");

  REQUIRE(primec::ir_lowerer::parseResultTypeName("Result< i64 , FileError >", hasValue, valueKind, errorType));
  CHECK(hasValue);
  CHECK(valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int64);
  CHECK(errorType == "FileError");

  CHECK_FALSE(primec::ir_lowerer::parseResultTypeName("array<i64>", hasValue, valueKind, errorType));
  CHECK_FALSE(primec::ir_lowerer::parseResultTypeName("Result<i64, FileError, Extra>", hasValue, valueKind, errorType));
}

TEST_CASE("ir lowerer runtime error helpers emit print-and-return sequence") {
  primec::IrFunction function;
  std::vector<std::string> stringTable;
  auto internString = [&](const std::string &text) -> int32_t {
    for (size_t i = 0; i < stringTable.size(); ++i) {
      if (stringTable[i] == text) {
        return static_cast<int32_t>(i);
      }
    }
    stringTable.push_back(text);
    return static_cast<int32_t>(stringTable.size() - 1);
  };

  primec::ir_lowerer::emitArrayIndexOutOfBounds(function, internString);
  REQUIRE(function.instructions.size() == 3);
  CHECK(function.instructions[0].op == primec::IrOpcode::PrintString);
  CHECK(primec::decodePrintFlags(function.instructions[0].imm) == primec::encodePrintFlags(true, true));
  CHECK(primec::decodePrintStringIndex(function.instructions[0].imm) == 0);
  CHECK(function.instructions[1].op == primec::IrOpcode::PushI32);
  CHECK(function.instructions[1].imm == 3);
  CHECK(function.instructions[2].op == primec::IrOpcode::ReturnI32);
  CHECK(function.instructions[2].imm == 0);
  REQUIRE(stringTable.size() == 1);
  CHECK(stringTable[0] == "array index out of bounds");

  primec::ir_lowerer::emitArrayIndexOutOfBounds(function, internString);
  REQUIRE(function.instructions.size() == 6);
  CHECK(primec::decodePrintStringIndex(function.instructions[3].imm) == 0);
  REQUIRE(stringTable.size() == 1);
}

TEST_CASE("ir lowerer runtime error helpers map each helper to expected message") {
  primec::IrFunction function;
  std::vector<std::string> stringTable;
  auto internString = [&](const std::string &text) -> int32_t {
    for (size_t i = 0; i < stringTable.size(); ++i) {
      if (stringTable[i] == text) {
        return static_cast<int32_t>(i);
      }
    }
    stringTable.push_back(text);
    return static_cast<int32_t>(stringTable.size() - 1);
  };

  primec::ir_lowerer::emitStringIndexOutOfBounds(function, internString);
  primec::ir_lowerer::emitMapKeyNotFound(function, internString);
  primec::ir_lowerer::emitVectorIndexOutOfBounds(function, internString);
  primec::ir_lowerer::emitVectorPopOnEmpty(function, internString);
  primec::ir_lowerer::emitVectorCapacityExceeded(function, internString);
  primec::ir_lowerer::emitVectorReserveNegative(function, internString);
  primec::ir_lowerer::emitVectorReserveExceeded(function, internString);
  primec::ir_lowerer::emitLoopCountNegative(function, internString);
  primec::ir_lowerer::emitPowNegativeExponent(function, internString);
  primec::ir_lowerer::emitFloatToIntNonFinite(function, internString);

  const std::vector<std::string> expectedMessages = {"string index out of bounds",
                                                     "map key not found",
                                                     "vector index out of bounds",
                                                     "vector pop on empty",
                                                     "vector capacity exceeded",
                                                     "vector reserve expects non-negative capacity",
                                                     "vector reserve exceeds capacity",
                                                     "loop count must be non-negative",
                                                     "pow exponent must be non-negative",
                                                     "float to int conversion requires finite value"};
  CHECK(stringTable == expectedMessages);

  REQUIRE(function.instructions.size() == expectedMessages.size() * 3);
  for (size_t i = 0; i < expectedMessages.size(); ++i) {
    const size_t base = i * 3;
    CHECK(function.instructions[base].op == primec::IrOpcode::PrintString);
    CHECK(primec::decodePrintStringIndex(function.instructions[base].imm) == i);
    CHECK(function.instructions[base + 1].op == primec::IrOpcode::PushI32);
    CHECK(function.instructions[base + 1].imm == 3);
    CHECK(function.instructions[base + 2].op == primec::IrOpcode::ReturnI32);
    CHECK(function.instructions[base + 2].imm == 0);
  }
}

TEST_CASE("ir lowerer runtime error helpers emit file-error why dispatch sequence") {
  primec::IrFunction function;
  std::vector<std::string> stringTable;
  auto internString = [&](const std::string &text) -> int32_t {
    for (size_t i = 0; i < stringTable.size(); ++i) {
      if (stringTable[i] == text) {
        return static_cast<int32_t>(i);
      }
    }
    stringTable.push_back(text);
    return static_cast<int32_t>(stringTable.size() - 1);
  };

  primec::ir_lowerer::emitFileErrorWhy(function, 7, internString);

  auto emptyIt = std::find(stringTable.begin(), stringTable.end(), "");
  REQUIRE(emptyIt != stringTable.end());
  auto unknownIt = std::find(stringTable.begin(), stringTable.end(), "Unknown file error");
  REQUIRE(unknownIt != stringTable.end());
  const uint64_t unknownIndex = static_cast<uint64_t>(std::distance(stringTable.begin(), unknownIt));

  REQUIRE_FALSE(function.instructions.empty());
  CHECK(function.instructions.back().op == primec::IrOpcode::PushI64);
  CHECK(function.instructions.back().imm == unknownIndex);

  CHECK(std::any_of(function.instructions.begin(),
                    function.instructions.end(),
                    [](const primec::IrInstruction &inst) { return inst.op == primec::IrOpcode::JumpIfZero; }));
  CHECK(std::any_of(function.instructions.begin(),
                    function.instructions.end(),
                    [](const primec::IrInstruction &inst) { return inst.op == primec::IrOpcode::Jump; }));
}

TEST_CASE("ir lowerer runtime error helpers emit FileError.why call paths") {
  using Result = primec::ir_lowerer::FileErrorWhyCallEmitResult;

  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.isMethodCall = true;
  expr.name = "noop";
  primec::ir_lowerer::LocalMap locals;
  std::vector<primec::IrInstruction> instructions;
  std::string error = "stale";
  int32_t emittedWhyLocal = -1;

  CHECK(primec::ir_lowerer::tryEmitFileErrorWhyCall(
            expr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            []() { return 0; },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            [&](int32_t local) { emittedWhyLocal = local; },
            error) == Result::NotHandled);
  CHECK(error == "stale");
  CHECK(instructions.empty());
  CHECK(emittedWhyLocal == -1);

  instructions.clear();
  error.clear();
  emittedWhyLocal = -1;
  primec::Expr fileErrorType;
  fileErrorType.kind = primec::Expr::Kind::Name;
  fileErrorType.name = "FileError";
  primec::Expr errValue;
  errValue.kind = primec::Expr::Kind::Name;
  errValue.name = "err";
  expr.name = "why";
  expr.args = {fileErrorType, errValue};
  int emitExprCalls = 0;
  CHECK(primec::ir_lowerer::tryEmitFileErrorWhyCall(
            expr,
            locals,
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              ++emitExprCalls;
              instructions.push_back({primec::IrOpcode::PushI64, 77});
              return true;
            },
            []() { return 44; },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            [&](int32_t local) { emittedWhyLocal = local; },
            error) == Result::Emitted);
  CHECK(error.empty());
  CHECK(emitExprCalls == 1);
  CHECK(emittedWhyLocal == 44);
  REQUIRE(instructions.size() == 2);
  CHECK(instructions[0].op == primec::IrOpcode::PushI64);
  CHECK(instructions[1].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[1].imm == 44);

  instructions.clear();
  error.clear();
  emittedWhyLocal = -1;
  expr.args = {fileErrorType};
  CHECK(primec::ir_lowerer::tryEmitFileErrorWhyCall(
            expr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            []() { return 3; },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            [&](int32_t local) { emittedWhyLocal = local; },
            error) == Result::Error);
  CHECK(error == "FileError.why requires exactly one argument");
  CHECK(instructions.empty());
  CHECK(emittedWhyLocal == -1);

  instructions.clear();
  error.clear();
  emittedWhyLocal = -1;
  expr.args = {fileErrorType, errValue};
  CHECK(primec::ir_lowerer::tryEmitFileErrorWhyCall(
            expr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            []() { return 5; },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            [&](int32_t local) { emittedWhyLocal = local; },
            error) == Result::Error);
  CHECK(instructions.empty());
  CHECK(emittedWhyLocal == -1);

  instructions.clear();
  error.clear();
  emittedWhyLocal = -1;
  expr.args = {errValue};
  primec::ir_lowerer::LocalInfo fileErrorInfo;
  fileErrorInfo.index = 12;
  fileErrorInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  fileErrorInfo.isFileError = true;
  locals.emplace("err", fileErrorInfo);
  CHECK(primec::ir_lowerer::tryEmitFileErrorWhyCall(
            expr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            []() { return 9; },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            [&](int32_t local) { emittedWhyLocal = local; },
            error) == Result::Emitted);
  CHECK(instructions.empty());
  CHECK(emittedWhyLocal == 12);

  instructions.clear();
  error.clear();
  emittedWhyLocal = -1;
  fileErrorInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Reference;
  fileErrorInfo.index = 33;
  locals["err"] = fileErrorInfo;
  CHECK(primec::ir_lowerer::tryEmitFileErrorWhyCall(
            expr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            []() { return 91; },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            [&](int32_t local) { emittedWhyLocal = local; },
            error) == Result::Emitted);
  REQUIRE(instructions.size() == 3);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 33);
  CHECK(instructions[1].op == primec::IrOpcode::LoadIndirect);
  CHECK(instructions[2].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[2].imm == 91);
  CHECK(emittedWhyLocal == 91);
}

TEST_CASE("ir lowerer runtime error helpers build scoped emitters") {
  primec::IrFunction function;
  std::vector<std::string> stringTable;
  auto internString = [&](const std::string &text) -> int32_t {
    for (size_t i = 0; i < stringTable.size(); ++i) {
      if (stringTable[i] == text) {
        return static_cast<int32_t>(i);
      }
    }
    stringTable.push_back(text);
    return static_cast<int32_t>(stringTable.size() - 1);
  };

  auto emitArrayIndexOutOfBounds = primec::ir_lowerer::makeEmitArrayIndexOutOfBounds(function, internString);
  auto emitMapKeyNotFound = primec::ir_lowerer::makeEmitMapKeyNotFound(function, internString);
  auto emitFloatToIntNonFinite = primec::ir_lowerer::makeEmitFloatToIntNonFinite(function, internString);

  emitArrayIndexOutOfBounds();
  emitMapKeyNotFound();
  emitFloatToIntNonFinite();
  emitArrayIndexOutOfBounds();

  const std::vector<std::string> expectedMessages = {
      "array index out of bounds", "map key not found", "float to int conversion requires finite value"};
  CHECK(stringTable == expectedMessages);
  REQUIRE(function.instructions.size() == 12);
  CHECK(primec::decodePrintStringIndex(function.instructions[0].imm) == 0);
  CHECK(primec::decodePrintStringIndex(function.instructions[3].imm) == 1);
  CHECK(primec::decodePrintStringIndex(function.instructions[6].imm) == 2);
  CHECK(primec::decodePrintStringIndex(function.instructions[9].imm) == 0);
}

TEST_CASE("ir lowerer runtime error helpers build bundled emitters") {
  primec::IrFunction function;
  std::vector<std::string> stringTable;
  auto internString = [&](const std::string &text) -> int32_t {
    for (size_t i = 0; i < stringTable.size(); ++i) {
      if (stringTable[i] == text) {
        return static_cast<int32_t>(i);
      }
    }
    stringTable.push_back(text);
    return static_cast<int32_t>(stringTable.size() - 1);
  };

  auto emitters = primec::ir_lowerer::makeRuntimeErrorEmitters(function, internString);
  emitters.emitStringIndexOutOfBounds();
  emitters.emitVectorPopOnEmpty();
  emitters.emitLoopCountNegative();
  emitters.emitStringIndexOutOfBounds();

  const std::vector<std::string> expectedMessages = {"string index out of bounds",
                                                     "vector pop on empty",
                                                     "loop count must be non-negative"};
  CHECK(stringTable == expectedMessages);
  REQUIRE(function.instructions.size() == 12);
  CHECK(primec::decodePrintStringIndex(function.instructions[0].imm) == 0);
  CHECK(primec::decodePrintStringIndex(function.instructions[3].imm) == 1);
  CHECK(primec::decodePrintStringIndex(function.instructions[6].imm) == 2);
  CHECK(primec::decodePrintStringIndex(function.instructions[9].imm) == 0);
}

TEST_CASE("ir lowerer runtime error helpers build bundled string-literal and emitters setup") {
  primec::IrFunction function;
  std::vector<std::string> stringTable;
  std::string error;

  const auto setup =
      primec::ir_lowerer::makeRuntimeErrorAndStringLiteralSetup(stringTable, function, error);
  CHECK(setup.stringLiteralHelpers.internString("hello") == 0);

  setup.runtimeErrorEmitters.emitArrayIndexOutOfBounds();
  REQUIRE(function.instructions.size() == 3);
  CHECK(function.instructions[0].op == primec::IrOpcode::PrintString);
  CHECK(primec::decodePrintStringIndex(function.instructions[0].imm) == 1);

  primec::Expr literalExpr;
  literalExpr.kind = primec::Expr::Kind::StringLiteral;
  literalExpr.stringValue = "\"hello\"utf8";
  int32_t stringIndex = -1;
  size_t length = 0;
  REQUIRE(setup.stringLiteralHelpers.resolveStringTableTarget(
      literalExpr, primec::ir_lowerer::LocalMap{}, stringIndex, length));
  CHECK(stringIndex == 0);
  CHECK(length == 5);

  REQUIRE(stringTable.size() == 2);
  CHECK(stringTable[0] == "hello");
  CHECK(stringTable[1] == "array index out of bounds");
}

TEST_CASE("ir lowerer index kind helpers normalize and validate supported kinds") {
  CHECK(primec::ir_lowerer::normalizeIndexKind(primec::ir_lowerer::LocalInfo::ValueKind::Bool) ==
        primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  CHECK(primec::ir_lowerer::normalizeIndexKind(primec::ir_lowerer::LocalInfo::ValueKind::Int64) ==
        primec::ir_lowerer::LocalInfo::ValueKind::Int64);

  CHECK(primec::ir_lowerer::isSupportedIndexKind(primec::ir_lowerer::LocalInfo::ValueKind::Int32));
  CHECK(primec::ir_lowerer::isSupportedIndexKind(primec::ir_lowerer::LocalInfo::ValueKind::Int64));
  CHECK(primec::ir_lowerer::isSupportedIndexKind(primec::ir_lowerer::LocalInfo::ValueKind::UInt64));
  CHECK_FALSE(primec::ir_lowerer::isSupportedIndexKind(primec::ir_lowerer::LocalInfo::ValueKind::Bool));
  CHECK_FALSE(primec::ir_lowerer::isSupportedIndexKind(primec::ir_lowerer::LocalInfo::ValueKind::Unknown));
}

TEST_CASE("ir lowerer index kind helpers map index opcodes by kind") {
  CHECK(primec::ir_lowerer::pushZeroForIndex(primec::ir_lowerer::LocalInfo::ValueKind::Int32) ==
        primec::IrOpcode::PushI32);
  CHECK(primec::ir_lowerer::pushZeroForIndex(primec::ir_lowerer::LocalInfo::ValueKind::Int64) ==
        primec::IrOpcode::PushI64);
  CHECK(primec::ir_lowerer::pushZeroForIndex(primec::ir_lowerer::LocalInfo::ValueKind::UInt64) ==
        primec::IrOpcode::PushI64);

  CHECK(primec::ir_lowerer::cmpLtForIndex(primec::ir_lowerer::LocalInfo::ValueKind::Int32) ==
        primec::IrOpcode::CmpLtI32);
  CHECK(primec::ir_lowerer::cmpLtForIndex(primec::ir_lowerer::LocalInfo::ValueKind::Int64) ==
        primec::IrOpcode::CmpLtI64);
  CHECK(primec::ir_lowerer::cmpLtForIndex(primec::ir_lowerer::LocalInfo::ValueKind::UInt64) ==
        primec::IrOpcode::CmpLtI64);

  CHECK(primec::ir_lowerer::cmpGeForIndex(primec::ir_lowerer::LocalInfo::ValueKind::Int32) ==
        primec::IrOpcode::CmpGeI32);
  CHECK(primec::ir_lowerer::cmpGeForIndex(primec::ir_lowerer::LocalInfo::ValueKind::Int64) ==
        primec::IrOpcode::CmpGeI64);
  CHECK(primec::ir_lowerer::cmpGeForIndex(primec::ir_lowerer::LocalInfo::ValueKind::UInt64) ==
        primec::IrOpcode::CmpGeU64);

  CHECK(primec::ir_lowerer::pushOneForIndex(primec::ir_lowerer::LocalInfo::ValueKind::Int32) ==
        primec::IrOpcode::PushI32);
  CHECK(primec::ir_lowerer::pushOneForIndex(primec::ir_lowerer::LocalInfo::ValueKind::Int64) ==
        primec::IrOpcode::PushI64);
  CHECK(primec::ir_lowerer::pushOneForIndex(primec::ir_lowerer::LocalInfo::ValueKind::UInt64) ==
        primec::IrOpcode::PushI64);

  CHECK(primec::ir_lowerer::addForIndex(primec::ir_lowerer::LocalInfo::ValueKind::Int32) ==
        primec::IrOpcode::AddI32);
  CHECK(primec::ir_lowerer::addForIndex(primec::ir_lowerer::LocalInfo::ValueKind::Int64) ==
        primec::IrOpcode::AddI64);
  CHECK(primec::ir_lowerer::addForIndex(primec::ir_lowerer::LocalInfo::ValueKind::UInt64) ==
        primec::IrOpcode::AddI64);

  CHECK(primec::ir_lowerer::mulForIndex(primec::ir_lowerer::LocalInfo::ValueKind::Int32) ==
        primec::IrOpcode::MulI32);
  CHECK(primec::ir_lowerer::mulForIndex(primec::ir_lowerer::LocalInfo::ValueKind::Int64) ==
        primec::IrOpcode::MulI64);
  CHECK(primec::ir_lowerer::mulForIndex(primec::ir_lowerer::LocalInfo::ValueKind::UInt64) ==
        primec::IrOpcode::MulI64);
}

TEST_CASE("ir lowerer setup math helper resolves namespaced builtins") {
  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "/std/math/sin";

  std::string builtinName;
  CHECK(primec::ir_lowerer::getSetupMathBuiltinName(callExpr, false, builtinName));
  CHECK(builtinName == "sin");
}

TEST_CASE("ir lowerer setup math helper validates builtin support names") {
  CHECK(primec::ir_lowerer::isSupportedMathBuiltinName("sin"));
  CHECK(primec::ir_lowerer::isSupportedMathBuiltinName("is_finite"));
  CHECK(primec::ir_lowerer::isSupportedMathBuiltinName("copysign"));
  CHECK_FALSE(primec::ir_lowerer::isSupportedMathBuiltinName("unknown_math"));
}

TEST_CASE("ir lowerer setup math helper requires import for bare names") {
  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "sin";

  std::string builtinName;
  CHECK_FALSE(primec::ir_lowerer::getSetupMathBuiltinName(callExpr, false, builtinName));
  CHECK(primec::ir_lowerer::getSetupMathBuiltinName(callExpr, true, builtinName));
  CHECK(builtinName == "sin");
}

TEST_CASE("ir lowerer setup math helper resolves constants only for supported names") {
  std::string constantName;
  CHECK(primec::ir_lowerer::getSetupMathConstantName("/std/math/pi", false, constantName));
  CHECK(constantName == "pi");
  CHECK_FALSE(primec::ir_lowerer::getSetupMathConstantName("phi", true, constantName));
}

TEST_CASE("ir lowerer setup math helper builds scoped name resolvers") {
  auto resolveBuiltinWithImport = primec::ir_lowerer::makeGetSetupMathBuiltinName(true);
  auto resolveBuiltinWithoutImport = primec::ir_lowerer::makeGetSetupMathBuiltinName(false);
  auto resolveConstantWithImport = primec::ir_lowerer::makeGetSetupMathConstantName(true);
  auto resolveConstantWithoutImport = primec::ir_lowerer::makeGetSetupMathConstantName(false);

  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "sin";
  std::string builtinName;
  CHECK(resolveBuiltinWithImport(callExpr, builtinName));
  CHECK(builtinName == "sin");
  CHECK_FALSE(resolveBuiltinWithoutImport(callExpr, builtinName));

  std::string constantName;
  CHECK(resolveConstantWithImport("pi", constantName));
  CHECK(constantName == "pi");
  CHECK_FALSE(resolveConstantWithoutImport("pi", constantName));
}

TEST_CASE("ir lowerer setup math helper builds bundled name resolvers") {
  auto resolversWithImport = primec::ir_lowerer::makeSetupMathResolvers(true);
  auto resolversWithoutImport = primec::ir_lowerer::makeSetupMathResolvers(false);

  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "sin";

  std::string builtinName;
  CHECK(resolversWithImport.getMathBuiltinName(callExpr, builtinName));
  CHECK(builtinName == "sin");
  CHECK_FALSE(resolversWithoutImport.getMathBuiltinName(callExpr, builtinName));

  std::string constantName;
  CHECK(resolversWithImport.getMathConstantName("pi", constantName));
  CHECK(constantName == "pi");
  CHECK_FALSE(resolversWithoutImport.getMathConstantName("pi", constantName));
}

TEST_CASE("ir lowerer setup math helper builds bundled setup math and binding adapters") {
  const auto adapters = primec::ir_lowerer::makeSetupMathAndBindingAdapters(true);

  primec::Expr mathCall;
  mathCall.kind = primec::Expr::Kind::Call;
  mathCall.name = "sin";
  std::string builtinName;
  CHECK(adapters.setupMathResolvers.getMathBuiltinName(mathCall, builtinName));
  CHECK(builtinName == "sin");

  primec::Expr stringExpr;
  primec::Transform stringTransform;
  stringTransform.name = "string";
  stringExpr.transforms.push_back(stringTransform);
  CHECK(adapters.bindingTypeAdapters.isStringBinding(stringExpr));
  CHECK_FALSE(adapters.bindingTypeAdapters.isFileErrorBinding(stringExpr));

  primec::Expr mapExpr;
  primec::Transform mapTransform;
  mapTransform.name = "map";
  mapTransform.templateArgs = {"bool", "f64"};
  mapExpr.transforms.push_back(mapTransform);
  CHECK(adapters.bindingTypeAdapters.bindingValueKind(mapExpr, primec::ir_lowerer::LocalInfo::Kind::Map) ==
        primec::ir_lowerer::LocalInfo::ValueKind::Float64);
}

TEST_CASE("ir lowerer statement binding helper infers vector kind from initializer call") {
  primec::Expr stmt;
  stmt.name = "values";

  primec::Expr init;
  init.kind = primec::Expr::Kind::Call;
  init.name = "vector";
  init.templateArgs = {"i64"};

  const primec::ir_lowerer::StatementBindingTypeInfo info = primec::ir_lowerer::inferStatementBindingTypeInfo(
      stmt,
      init,
      {},
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::Kind::Value; },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo::Kind) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      });

  CHECK(info.kind == primec::ir_lowerer::LocalInfo::Kind::Vector);
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int64);
}

TEST_CASE("ir lowerer statement binding helper inherits map metadata from named source binding") {
  primec::Expr stmt;
  stmt.name = "dst";

  primec::Expr init;
  init.kind = primec::Expr::Kind::Name;
  init.name = "srcMap";

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo sourceInfo;
  sourceInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Map;
  sourceInfo.mapKeyKind = primec::ir_lowerer::LocalInfo::ValueKind::Bool;
  sourceInfo.mapValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Float64;
  locals.emplace("srcMap", sourceInfo);

  const primec::ir_lowerer::StatementBindingTypeInfo info = primec::ir_lowerer::inferStatementBindingTypeInfo(
      stmt,
      init,
      locals,
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::Kind::Value; },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo::Kind) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      });

  CHECK(info.kind == primec::ir_lowerer::LocalInfo::Kind::Map);
  CHECK(info.mapKeyKind == primec::ir_lowerer::LocalInfo::ValueKind::Bool);
  CHECK(info.mapValueKind == primec::ir_lowerer::LocalInfo::ValueKind::Float64);
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Float64);
}

TEST_CASE("ir lowerer statement binding helper infers call parameter local info") {
  primec::Expr literalInit;
  literalInit.kind = primec::Expr::Kind::Literal;
  literalInit.literalValue = 7;

  primec::Expr param;
  param.name = "value";
  param.args = {literalInit};

  primec::ir_lowerer::LocalInfo info;
  info.index = 12;
  std::string error;
  REQUIRE(primec::ir_lowerer::inferCallParameterLocalInfo(
      param,
      {},
      [](const primec::Expr &) { return true; },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::Kind::Value; },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo::Kind) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
        if (expr.kind == primec::Expr::Kind::Literal) {
          return primec::ir_lowerer::LocalInfo::ValueKind::Int64;
        }
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &) { return false; },
      info,
      error));
  CHECK(error.empty());
  CHECK(info.index == 12);
  CHECK(info.isMutable);
  CHECK(info.kind == primec::ir_lowerer::LocalInfo::Kind::Value);
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int64);
}

TEST_CASE("ir lowerer statement binding helper sets string parameter defaults") {
  primec::Expr literalInit;
  literalInit.kind = primec::Expr::Kind::Literal;
  literalInit.literalValue = 0;

  primec::Expr param;
  param.name = "label";
  param.args = {literalInit};

  primec::ir_lowerer::LocalInfo info;
  info.index = 3;
  std::string error;
  REQUIRE(primec::ir_lowerer::inferCallParameterLocalInfo(
      param,
      {},
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::Kind::Value; },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo::Kind) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &) { return true; },
      info,
      error));
  CHECK(error.empty());
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::String);
  CHECK(info.stringSource == primec::ir_lowerer::LocalInfo::StringSource::RuntimeIndex);
  CHECK(info.stringIndex == -1);
  CHECK(info.argvChecked);
}

TEST_CASE("ir lowerer statement binding helper rejects string reference parameters") {
  primec::Expr param;
  param.name = "label";

  primec::ir_lowerer::LocalInfo info;
  info.index = 4;
  std::string error;
  CHECK_FALSE(primec::ir_lowerer::inferCallParameterLocalInfo(
      param,
      {},
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &) { return true; },
      [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::Kind::Reference; },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo::Kind) {
        return primec::ir_lowerer::LocalInfo::ValueKind::String;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &) { return true; },
      info,
      error));
  CHECK(error == "native backend does not support string pointers or references");
}

TEST_CASE("ir lowerer statement binding helper selects uninitialized zero opcodes") {
  primec::IrOpcode mapZeroOp = primec::IrOpcode::PushI32;
  uint64_t mapZeroImm = 123;
  std::string error;
  REQUIRE(primec::ir_lowerer::selectUninitializedStorageZeroInstruction(
      primec::ir_lowerer::LocalInfo::Kind::Map,
      primec::ir_lowerer::LocalInfo::ValueKind::Unknown,
      "mapStorage",
      mapZeroOp,
      mapZeroImm,
      error));
  CHECK(mapZeroOp == primec::IrOpcode::PushI64);
  CHECK(mapZeroImm == 0);
  CHECK(error.empty());

  primec::IrOpcode floatZeroOp = primec::IrOpcode::PushI32;
  uint64_t floatZeroImm = 99;
  REQUIRE(primec::ir_lowerer::selectUninitializedStorageZeroInstruction(
      primec::ir_lowerer::LocalInfo::Kind::Value,
      primec::ir_lowerer::LocalInfo::ValueKind::Float32,
      "floatStorage",
      floatZeroOp,
      floatZeroImm,
      error));
  CHECK(floatZeroOp == primec::IrOpcode::PushF32);
  CHECK(floatZeroImm == 0);
}

TEST_CASE("ir lowerer statement binding helper rejects unknown uninitialized value kind") {
  primec::IrOpcode zeroOp = primec::IrOpcode::PushI32;
  uint64_t zeroImm = 0;
  std::string error;
  CHECK_FALSE(primec::ir_lowerer::selectUninitializedStorageZeroInstruction(
      primec::ir_lowerer::LocalInfo::Kind::Value,
      primec::ir_lowerer::LocalInfo::ValueKind::Unknown,
      "storageSlot",
      zeroOp,
      zeroImm,
      error));
  CHECK(error == "native backend requires a concrete uninitialized storage type on storageSlot");
}

TEST_CASE("ir lowerer arithmetic helper emits integer add opcode") {
  primec::Expr left;
  left.kind = primec::Expr::Kind::Literal;
  left.literalValue = 1;
  primec::Expr right;
  right.kind = primec::Expr::Kind::Literal;
  right.literalValue = 2;
  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.name = "plus";
  expr.args = {left, right};

  std::vector<primec::IrInstruction> instructions;
  std::string error;
  auto result = primec::ir_lowerer::emitArithmeticOperatorExpr(
      expr,
      {},
      [&](const primec::Expr &arg, const primec::ir_lowerer::LocalMap &) {
        instructions.push_back({primec::IrOpcode::PushI32, static_cast<uint64_t>(arg.literalValue)});
        return true;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Int32;
      },
      [](primec::ir_lowerer::LocalInfo::ValueKind leftKind, primec::ir_lowerer::LocalInfo::ValueKind rightKind) {
        return (leftKind == rightKind) ? leftKind : primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      error);

  CHECK(result == primec::ir_lowerer::OperatorArithmeticEmitResult::Handled);
  CHECK(error.empty());
  REQUIRE(instructions.size() == 3);
  CHECK(instructions.back().op == primec::IrOpcode::AddI32);
}

TEST_CASE("ir lowerer arithmetic helper validates pointer operand side") {
  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo pointerInfo;
  pointerInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Pointer;
  pointerInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  locals.emplace("ptr", pointerInfo);
  primec::ir_lowerer::LocalInfo intInfo;
  intInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  intInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  locals.emplace("value", intInfo);

  primec::Expr left;
  left.kind = primec::Expr::Kind::Name;
  left.name = "value";
  primec::Expr right;
  right.kind = primec::Expr::Kind::Name;
  right.name = "ptr";
  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.name = "plus";
  expr.args = {left, right};

  std::string error;
  auto result = primec::ir_lowerer::emitArithmeticOperatorExpr(
      expr,
      locals,
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
      [](const primec::Expr &arg, const primec::ir_lowerer::LocalMap &localMap) {
        auto it = localMap.find(arg.name);
        return (it == localMap.end()) ? primec::ir_lowerer::LocalInfo::ValueKind::Unknown : it->second.valueKind;
      },
      [](primec::ir_lowerer::LocalInfo::ValueKind leftKind, primec::ir_lowerer::LocalInfo::ValueKind rightKind) {
        return (leftKind == rightKind) ? leftKind : primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](primec::IrOpcode, uint64_t) {},
      error);

  CHECK(result == primec::ir_lowerer::OperatorArithmeticEmitResult::Error);
  CHECK(error == "pointer arithmetic requires pointer on the left");
}

TEST_CASE("ir lowerer arithmetic helper ignores non arithmetic calls") {
  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.name = "equal";

  std::string error;
  auto result = primec::ir_lowerer::emitArithmeticOperatorExpr(
      expr,
      {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](primec::ir_lowerer::LocalInfo::ValueKind leftKind, primec::ir_lowerer::LocalInfo::ValueKind rightKind) {
        return (leftKind == rightKind) ? leftKind : primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](primec::IrOpcode, uint64_t) {},
      error);

  CHECK(result == primec::ir_lowerer::OperatorArithmeticEmitResult::NotHandled);
  CHECK(error.empty());
}

TEST_CASE("ir lowerer comparison helper emits integer less-than opcode") {
  primec::Expr left;
  left.kind = primec::Expr::Kind::Literal;
  left.literalValue = 1;
  primec::Expr right;
  right.kind = primec::Expr::Kind::Literal;
  right.literalValue = 2;
  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.name = "less_than";
  expr.args = {left, right};

  std::vector<primec::IrInstruction> instructions;
  std::string error;
  auto result = primec::ir_lowerer::emitComparisonOperatorExpr(
      expr,
      {},
      [&](const primec::Expr &arg, const primec::ir_lowerer::LocalMap &) {
        instructions.push_back({primec::IrOpcode::PushI32, static_cast<uint64_t>(arg.literalValue)});
        return true;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Int32;
      },
      [](primec::ir_lowerer::LocalInfo::ValueKind leftKind, primec::ir_lowerer::LocalInfo::ValueKind rightKind) {
        return (leftKind == rightKind) ? leftKind : primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](primec::ir_lowerer::LocalInfo::ValueKind, bool) { return true; },
      instructions,
      error);

  CHECK(result == primec::ir_lowerer::OperatorComparisonEmitResult::Handled);
  CHECK(error.empty());
  REQUIRE(instructions.size() == 3);
  CHECK(instructions.back().op == primec::IrOpcode::CmpLtI32);
}

TEST_CASE("ir lowerer comparison helper lowers logical and short-circuit") {
  primec::Expr left;
  left.kind = primec::Expr::Kind::BoolLiteral;
  left.boolValue = true;
  primec::Expr right;
  right.kind = primec::Expr::Kind::BoolLiteral;
  right.boolValue = false;
  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.name = "and";
  expr.args = {left, right};

  std::vector<primec::IrInstruction> instructions;
  std::string error;
  auto result = primec::ir_lowerer::emitComparisonOperatorExpr(
      expr,
      {},
      [&](const primec::Expr &arg, const primec::ir_lowerer::LocalMap &) {
        instructions.push_back({primec::IrOpcode::PushI32, arg.boolValue ? 1u : 0u});
        return true;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Bool;
      },
      [](primec::ir_lowerer::LocalInfo::ValueKind leftKind, primec::ir_lowerer::LocalInfo::ValueKind rightKind) {
        return (leftKind == rightKind) ? leftKind : primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [&](primec::ir_lowerer::LocalInfo::ValueKind kind, bool equals) {
        if (kind != primec::ir_lowerer::LocalInfo::ValueKind::Bool &&
            kind != primec::ir_lowerer::LocalInfo::ValueKind::Int32) {
          return false;
        }
        instructions.push_back({primec::IrOpcode::PushI32, 0});
        instructions.push_back({equals ? primec::IrOpcode::CmpEqI32 : primec::IrOpcode::CmpNeI32, 0});
        return true;
      },
      instructions,
      error);

  CHECK(result == primec::ir_lowerer::OperatorComparisonEmitResult::Handled);
  CHECK(error.empty());
  REQUIRE(instructions.size() == 9);
  CHECK(instructions[3].op == primec::IrOpcode::JumpIfZero);
  CHECK(instructions[3].imm == 8);
  CHECK(instructions[7].op == primec::IrOpcode::Jump);
  CHECK(instructions[7].imm == 9);
  CHECK(instructions[8].op == primec::IrOpcode::PushI32);
  CHECK(instructions[8].imm == 0);
}

TEST_CASE("ir lowerer comparison helper rejects string operands") {
  primec::Expr left;
  left.kind = primec::Expr::Kind::StringLiteral;
  left.stringValue = "\"a\"";
  primec::Expr right;
  right.kind = primec::Expr::Kind::StringLiteral;
  right.stringValue = "\"b\"";
  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.name = "equal";
  expr.args = {left, right};

  std::vector<primec::IrInstruction> instructions;
  std::string error;
  auto result = primec::ir_lowerer::emitComparisonOperatorExpr(
      expr,
      {},
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        instructions.push_back({primec::IrOpcode::PushI64, 0});
        return true;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::String;
      },
      [](primec::ir_lowerer::LocalInfo::ValueKind leftKind, primec::ir_lowerer::LocalInfo::ValueKind rightKind) {
        return (leftKind == rightKind) ? leftKind : primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](primec::ir_lowerer::LocalInfo::ValueKind, bool) { return true; },
      instructions,
      error);

  CHECK(result == primec::ir_lowerer::OperatorComparisonEmitResult::Error);
  CHECK(error == "native backend does not support string comparisons");
  CHECK(instructions.empty());
}

TEST_CASE("ir lowerer comparison helper ignores non comparison calls") {
  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.name = "plus";

  std::vector<primec::IrInstruction> instructions;
  std::string error;
  auto result = primec::ir_lowerer::emitComparisonOperatorExpr(
      expr,
      {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](primec::ir_lowerer::LocalInfo::ValueKind leftKind, primec::ir_lowerer::LocalInfo::ValueKind rightKind) {
        return (leftKind == rightKind) ? leftKind : primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](primec::ir_lowerer::LocalInfo::ValueKind, bool) { return true; },
      instructions,
      error);

  CHECK(result == primec::ir_lowerer::OperatorComparisonEmitResult::NotHandled);
  CHECK(error.empty());
  CHECK(instructions.empty());
}

TEST_CASE("ir lowerer saturate helper emits is_nan predicate opcode") {
  primec::Expr arg;
  arg.kind = primec::Expr::Kind::FloatLiteral;
  arg.floatValue = "1.0";
  arg.floatWidth = 32;
  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.name = "is_nan";
  expr.args = {arg};

  std::vector<primec::IrInstruction> instructions;
  std::string error;
  int32_t nextLocal = 0;
  auto result = primec::ir_lowerer::emitSaturateRoundingRootsOperatorExpr(
      expr,
      {},
      true,
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        instructions.push_back({primec::IrOpcode::PushF32, 0});
        return true;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Float32;
      },
      [&]() { return nextLocal++; },
      instructions,
      error);

  CHECK(result == primec::ir_lowerer::OperatorSaturateRoundingRootsEmitResult::Handled);
  CHECK(error.empty());
  REQUIRE(instructions.size() == 5);
  CHECK(instructions[1].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[4].op == primec::IrOpcode::CmpNeF32);
}

TEST_CASE("ir lowerer saturate helper rejects bool saturate operand") {
  primec::Expr arg;
  arg.kind = primec::Expr::Kind::BoolLiteral;
  arg.boolValue = true;
  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.name = "saturate";
  expr.args = {arg};

  std::vector<primec::IrInstruction> instructions;
  bool emitted = false;
  std::string error;
  int32_t nextLocal = 0;
  auto result = primec::ir_lowerer::emitSaturateRoundingRootsOperatorExpr(
      expr,
      {},
      true,
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        emitted = true;
        return true;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Bool;
      },
      [&]() { return nextLocal++; },
      instructions,
      error);

  CHECK(result == primec::ir_lowerer::OperatorSaturateRoundingRootsEmitResult::Error);
  CHECK(error == "saturate requires numeric argument");
  CHECK_FALSE(emitted);
  CHECK(instructions.empty());
}

TEST_CASE("ir lowerer saturate helper ignores non saturate builtins") {
  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.name = "plus";

  std::vector<primec::IrInstruction> instructions;
  std::string error;
  int32_t nextLocal = 0;
  auto result = primec::ir_lowerer::emitSaturateRoundingRootsOperatorExpr(
      expr,
      {},
      true,
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [&]() { return nextLocal++; },
      instructions,
      error);

  CHECK(result == primec::ir_lowerer::OperatorSaturateRoundingRootsEmitResult::NotHandled);
  CHECK(error.empty());
  CHECK(instructions.empty());
}

TEST_CASE("ir lowerer clamp helper emits radians multiply opcode") {
  primec::Expr arg;
  arg.kind = primec::Expr::Kind::FloatLiteral;
  arg.floatValue = "1.0";
  arg.floatWidth = 64;
  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.name = "radians";
  expr.args = {arg};

  std::vector<primec::IrInstruction> instructions;
  std::string error;
  int32_t nextLocal = 0;
  auto result = primec::ir_lowerer::emitClampMinMaxTrigOperatorExpr(
      expr,
      {},
      true,
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        instructions.push_back({primec::IrOpcode::PushF64, 0});
        return true;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Float64;
      },
      [](primec::ir_lowerer::LocalInfo::ValueKind leftKind, primec::ir_lowerer::LocalInfo::ValueKind rightKind) {
        return (leftKind == rightKind) ? leftKind : primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [&]() { return nextLocal++; },
      instructions,
      error);

  CHECK(result == primec::ir_lowerer::OperatorClampMinMaxTrigEmitResult::Handled);
  CHECK(error.empty());
  REQUIRE(instructions.size() == 3);
  CHECK(instructions.back().op == primec::IrOpcode::MulF64);
}

TEST_CASE("ir lowerer clamp helper rejects mixed signed unsigned clamp args") {
  primec::Expr value;
  value.kind = primec::Expr::Kind::Literal;
  value.literalValue = 1;
  value.isUnsigned = true;
  primec::Expr minValue;
  minValue.kind = primec::Expr::Kind::Literal;
  minValue.literalValue = 0;
  primec::Expr maxValue;
  maxValue.kind = primec::Expr::Kind::Literal;
  maxValue.literalValue = 2;
  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.name = "clamp";
  expr.args = {value, minValue, maxValue};

  std::vector<primec::IrInstruction> instructions;
  std::string error;
  int32_t nextLocal = 0;
  auto result = primec::ir_lowerer::emitClampMinMaxTrigOperatorExpr(
      expr,
      {},
      true,
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Int32;
      },
      [](primec::ir_lowerer::LocalInfo::ValueKind leftKind, primec::ir_lowerer::LocalInfo::ValueKind rightKind) {
        return (leftKind == rightKind) ? leftKind : primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [&]() { return nextLocal++; },
      instructions,
      error);

  CHECK(result == primec::ir_lowerer::OperatorClampMinMaxTrigEmitResult::Error);
  CHECK(error == "clamp requires numeric arguments of the same type");
  CHECK(instructions.empty());
}

TEST_CASE("ir lowerer clamp helper ignores non clamp builtins") {
  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.name = "saturate";

  std::vector<primec::IrInstruction> instructions;
  std::string error;
  int32_t nextLocal = 0;
  auto result = primec::ir_lowerer::emitClampMinMaxTrigOperatorExpr(
      expr,
      {},
      true,
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](primec::ir_lowerer::LocalInfo::ValueKind leftKind, primec::ir_lowerer::LocalInfo::ValueKind rightKind) {
        return (leftKind == rightKind) ? leftKind : primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [&]() { return nextLocal++; },
      instructions,
      error);

  CHECK(result == primec::ir_lowerer::OperatorClampMinMaxTrigEmitResult::NotHandled);
  CHECK(error.empty());
  CHECK(instructions.empty());
}

TEST_CASE("ir lowerer arc helper emits exp2 float multiply opcode") {
  primec::Expr arg;
  arg.kind = primec::Expr::Kind::FloatLiteral;
  arg.floatValue = "1.0";
  arg.floatWidth = 64;
  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.name = "exp2";
  expr.args = {arg};

  std::vector<primec::IrInstruction> instructions;
  std::string error;
  int32_t nextLocal = 0;
  auto result = primec::ir_lowerer::emitArcHyperbolicOperatorExpr(
      expr,
      {},
      true,
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        instructions.push_back({primec::IrOpcode::PushF64, 0});
        return true;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Float64;
      },
      [&]() { return nextLocal++; },
      instructions,
      error);

  CHECK(result == primec::ir_lowerer::OperatorArcHyperbolicEmitResult::Handled);
  CHECK(error.empty());
  CHECK(instructions.size() > 10);
  CHECK(std::any_of(instructions.begin(),
                    instructions.end(),
                    [](const primec::IrInstruction &inst) { return inst.op == primec::IrOpcode::MulF64; }));
}

TEST_CASE("ir lowerer arc helper rejects non-float arc operands") {
  primec::Expr arg;
  arg.kind = primec::Expr::Kind::BoolLiteral;
  arg.boolValue = true;
  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.name = "asin";
  expr.args = {arg};

  std::vector<primec::IrInstruction> instructions;
  std::string error;
  int32_t nextLocal = 0;
  auto result = primec::ir_lowerer::emitArcHyperbolicOperatorExpr(
      expr,
      {},
      true,
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Bool;
      },
      [&]() { return nextLocal++; },
      instructions,
      error);

  CHECK(result == primec::ir_lowerer::OperatorArcHyperbolicEmitResult::Error);
  CHECK(error == "asin requires float argument");
  CHECK(instructions.empty());
}

TEST_CASE("ir lowerer arc helper ignores non arc builtins") {
  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.name = "clamp";

  std::vector<primec::IrInstruction> instructions;
  std::string error;
  int32_t nextLocal = 0;
  auto result = primec::ir_lowerer::emitArcHyperbolicOperatorExpr(
      expr,
      {},
      true,
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [&]() { return nextLocal++; },
      instructions,
      error);

  CHECK(result == primec::ir_lowerer::OperatorArcHyperbolicEmitResult::NotHandled);
  CHECK(error.empty());
  CHECK(instructions.empty());
}

TEST_CASE("ir lowerer pow helper emits integer multiply opcode") {
  primec::Expr base;
  base.kind = primec::Expr::Kind::Literal;
  base.literalValue = 2;
  primec::Expr exponent;
  exponent.kind = primec::Expr::Kind::Literal;
  exponent.literalValue = 3;
  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.name = "pow";
  expr.args = {base, exponent};

  std::vector<primec::IrInstruction> instructions;
  std::string error;
  int32_t nextLocal = 0;
  auto result = primec::ir_lowerer::emitPowAbsSignOperatorExpr(
      expr,
      {},
      true,
      [&](const primec::Expr &arg, const primec::ir_lowerer::LocalMap &) {
        instructions.push_back({primec::IrOpcode::PushI32, static_cast<uint64_t>(arg.literalValue)});
        return true;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Int32;
      },
      [](primec::ir_lowerer::LocalInfo::ValueKind leftKind, primec::ir_lowerer::LocalInfo::ValueKind rightKind) {
        return (leftKind == rightKind) ? leftKind : primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [&]() { return nextLocal++; },
      []() {},
      instructions,
      error);

  CHECK(result == primec::ir_lowerer::OperatorPowAbsSignEmitResult::Handled);
  CHECK(error.empty());
  CHECK(std::any_of(instructions.begin(),
                    instructions.end(),
                    [](const primec::IrInstruction &inst) { return inst.op == primec::IrOpcode::MulI32; }));
}

TEST_CASE("ir lowerer pow helper rejects mixed signed unsigned operands") {
  primec::Expr left;
  left.kind = primec::Expr::Kind::Literal;
  left.literalValue = 1;
  left.isUnsigned = true;
  primec::Expr right;
  right.kind = primec::Expr::Kind::Literal;
  right.literalValue = 2;
  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.name = "pow";
  expr.args = {left, right};

  std::vector<primec::IrInstruction> instructions;
  std::string error;
  int32_t nextLocal = 0;
  auto result = primec::ir_lowerer::emitPowAbsSignOperatorExpr(
      expr,
      {},
      true,
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Int32;
      },
      [](primec::ir_lowerer::LocalInfo::ValueKind leftKind, primec::ir_lowerer::LocalInfo::ValueKind rightKind) {
        return (leftKind == rightKind) ? leftKind : primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [&]() { return nextLocal++; },
      []() {},
      instructions,
      error);

  CHECK(result == primec::ir_lowerer::OperatorPowAbsSignEmitResult::Error);
  CHECK(error == "pow requires numeric arguments of the same type");
  CHECK(instructions.empty());
}

TEST_CASE("ir lowerer pow helper ignores non pow/abs/sign builtins") {
  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.name = "clamp";

  std::vector<primec::IrInstruction> instructions;
  std::string error;
  int32_t nextLocal = 0;
  auto result = primec::ir_lowerer::emitPowAbsSignOperatorExpr(
      expr,
      {},
      true,
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](primec::ir_lowerer::LocalInfo::ValueKind leftKind, primec::ir_lowerer::LocalInfo::ValueKind rightKind) {
        return (leftKind == rightKind) ? leftKind : primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [&]() { return nextLocal++; },
      []() {},
      instructions,
      error);

  CHECK(result == primec::ir_lowerer::OperatorPowAbsSignEmitResult::NotHandled);
  CHECK(error.empty());
  CHECK(instructions.empty());
}

TEST_CASE("ir lowerer conversions helper emits float conversion opcode") {
  primec::Expr arg;
  arg.kind = primec::Expr::Kind::Literal;
  arg.literalValue = 7;
  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.name = "convert";
  expr.templateArgs = {"f32"};
  expr.args = {arg};

  std::vector<primec::IrInstruction> instructions;
  std::string error;
  bool handled = false;
  int32_t nextLocal = 0;
  const bool ok = primec::ir_lowerer::emitConversionsAndCallsOperatorExpr(
      expr,
      {},
      nextLocal,
      [&](const primec::Expr &valueExpr, const primec::ir_lowerer::LocalMap &) {
        instructions.push_back({primec::IrOpcode::PushI32, static_cast<uint64_t>(valueExpr.literalValue)});
        return true;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Int32;
      },
      [](primec::ir_lowerer::LocalInfo::ValueKind, bool) { return true; },
      [&]() { return nextLocal++; },
      []() {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return false; },
      [](const std::string &typeName) {
        if (typeName == "f32") {
          return primec::ir_lowerer::LocalInfo::ValueKind::Float32;
        }
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const std::string &, std::string &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string(); },
      [](const std::string &, int32_t &) { return false; },
      [](int32_t, int32_t, int32_t) { return false; },
      instructions,
      handled,
      error);

  CHECK(ok);
  CHECK(handled);
  CHECK(error.empty());
  CHECK(std::any_of(instructions.begin(),
                    instructions.end(),
                    [](const primec::IrInstruction &inst) { return inst.op == primec::IrOpcode::ConvertI32ToF32; }));
}

TEST_CASE("ir lowerer conversions helper rejects immutable assign target") {
  primec::Expr target;
  target.kind = primec::Expr::Kind::Name;
  target.name = "x";
  primec::Expr value;
  value.kind = primec::Expr::Kind::Literal;
  value.literalValue = 3;
  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.name = "assign";
  expr.args = {target, value};

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo info;
  info.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  info.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  info.index = 0;
  info.isMutable = false;
  locals.emplace("x", info);

  std::vector<primec::IrInstruction> instructions;
  std::string error;
  bool handled = false;
  int32_t nextLocal = 0;
  const bool ok = primec::ir_lowerer::emitConversionsAndCallsOperatorExpr(
      expr,
      locals,
      nextLocal,
      [&](const primec::Expr &valueExpr, const primec::ir_lowerer::LocalMap &) {
        instructions.push_back({primec::IrOpcode::PushI32, static_cast<uint64_t>(valueExpr.literalValue)});
        return true;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Int32;
      },
      [](primec::ir_lowerer::LocalInfo::ValueKind, bool) { return true; },
      [&]() { return nextLocal++; },
      []() {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return false; },
      [](const std::string &) { return primec::ir_lowerer::LocalInfo::ValueKind::Unknown; },
      [](const std::string &, std::string &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string(); },
      [](const std::string &, int32_t &) { return false; },
      [](int32_t, int32_t, int32_t) { return false; },
      instructions,
      handled,
      error);

  CHECK_FALSE(ok);
  CHECK(handled);
  CHECK(error == "assign target must be mutable: x");
}

TEST_CASE("ir lowerer conversions helper ignores unrelated call names") {
  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.name = "pow";

  std::vector<primec::IrInstruction> instructions;
  std::string error;
  bool handled = true;
  int32_t nextLocal = 0;
  const bool ok = primec::ir_lowerer::emitConversionsAndCallsOperatorExpr(
      expr,
      {},
      nextLocal,
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](primec::ir_lowerer::LocalInfo::ValueKind, bool) { return true; },
      [&]() { return nextLocal++; },
      []() {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return false; },
      [](const std::string &) { return primec::ir_lowerer::LocalInfo::ValueKind::Unknown; },
      [](const std::string &, std::string &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string(); },
      [](const std::string &, int32_t &) { return false; },
      [](int32_t, int32_t, int32_t) { return false; },
      instructions,
      handled,
      error);

  CHECK(ok);
  CHECK_FALSE(handled);
  CHECK(error.empty());
  CHECK(instructions.empty());
}

TEST_CASE("ir lowerer conversions control-tail helper lowers if blocks") {
  primec::Expr cond;
  cond.kind = primec::Expr::Kind::BoolLiteral;
  cond.boolValue = true;

  primec::Expr thenValue;
  thenValue.kind = primec::Expr::Kind::Literal;
  thenValue.literalValue = 1;
  primec::Expr thenBlock;
  thenBlock.kind = primec::Expr::Kind::Call;
  thenBlock.name = "then_block";
  thenBlock.hasBodyArguments = true;
  thenBlock.bodyArguments = {thenValue};

  primec::Expr elseValue;
  elseValue.kind = primec::Expr::Kind::Literal;
  elseValue.literalValue = 2;
  primec::Expr elseBlock;
  elseBlock.kind = primec::Expr::Kind::Call;
  elseBlock.name = "else_block";
  elseBlock.hasBodyArguments = true;
  elseBlock.bodyArguments = {elseValue};

  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.name = "if";
  expr.args = {cond, thenBlock, elseBlock};

  std::vector<primec::IrInstruction> instructions;
  std::string error;
  bool handled = false;
  int enteredScopes = 0;
  int exitedScopes = 0;
  const bool ok = primec::ir_lowerer::emitConversionsAndCallsControlExprTail(
      expr,
      {},
      [&](const primec::Expr &valueExpr, const primec::ir_lowerer::LocalMap &) {
        if (valueExpr.kind == primec::Expr::Kind::BoolLiteral) {
          instructions.push_back({primec::IrOpcode::PushI32, valueExpr.boolValue ? 1u : 0u});
          return true;
        }
        if (valueExpr.kind == primec::Expr::Kind::Literal) {
          instructions.push_back({primec::IrOpcode::PushI32, static_cast<uint64_t>(valueExpr.literalValue)});
          return true;
        }
        return false;
      },
      [](const primec::Expr &, primec::ir_lowerer::LocalMap &) { return true; },
      [](const primec::Expr &valueExpr, const primec::ir_lowerer::LocalMap &) {
        if (valueExpr.kind == primec::Expr::Kind::BoolLiteral) {
          return primec::ir_lowerer::LocalInfo::ValueKind::Bool;
        }
        if (valueExpr.kind == primec::Expr::Kind::Literal) {
          return primec::ir_lowerer::LocalInfo::ValueKind::Int32;
        }
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](primec::ir_lowerer::LocalInfo::ValueKind leftKind, primec::ir_lowerer::LocalInfo::ValueKind rightKind) {
        return (leftKind == rightKind) ? leftKind : primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const std::vector<std::optional<std::string>> &argNames) {
        return std::any_of(argNames.begin(), argNames.end(), [](const auto &name) { return name.has_value(); });
      },
      [](const primec::Expr &) { return static_cast<const primec::Definition *>(nullptr); },
      [](const primec::Expr &callExpr) { return std::string("/") + callExpr.name; },
      [](const primec::Expr &, primec::Expr &, std::string &) { return false; },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::Kind::Value; },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo::Kind) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string(); },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [&]() { ++enteredScopes; },
      [&]() { ++exitedScopes; },
      [](const primec::Expr &callExpr) { return callExpr.kind == primec::Expr::Kind::Call && callExpr.name == "return"; },
      [](const primec::Expr &callExpr) { return callExpr.kind == primec::Expr::Kind::Call && callExpr.name == "block"; },
      [](const primec::Expr &callExpr) { return callExpr.kind == primec::Expr::Kind::Call && callExpr.name == "match"; },
      [](const primec::Expr &callExpr) { return callExpr.kind == primec::Expr::Kind::Call && callExpr.name == "if"; },
      instructions,
      handled,
      error);

  CHECK(ok);
  CHECK(handled);
  CHECK(error.empty());
  CHECK(enteredScopes == 2);
  CHECK(exitedScopes == 2);
  CHECK(std::any_of(instructions.begin(),
                    instructions.end(),
                    [](const primec::IrInstruction &inst) { return inst.op == primec::IrOpcode::JumpIfZero; }));
}

TEST_CASE("ir lowerer conversions control-tail helper rejects incompatible if branch values") {
  primec::Expr cond;
  cond.kind = primec::Expr::Kind::BoolLiteral;
  cond.boolValue = true;

  primec::Expr thenValue;
  thenValue.kind = primec::Expr::Kind::Literal;
  thenValue.literalValue = 1;
  primec::Expr thenBlock;
  thenBlock.kind = primec::Expr::Kind::Call;
  thenBlock.name = "then_block";
  thenBlock.hasBodyArguments = true;
  thenBlock.bodyArguments = {thenValue};

  primec::Expr elseValue;
  elseValue.kind = primec::Expr::Kind::BoolLiteral;
  elseValue.boolValue = false;
  primec::Expr elseBlock;
  elseBlock.kind = primec::Expr::Kind::Call;
  elseBlock.name = "else_block";
  elseBlock.hasBodyArguments = true;
  elseBlock.bodyArguments = {elseValue};

  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.name = "if";
  expr.args = {cond, thenBlock, elseBlock};

  std::vector<primec::IrInstruction> instructions;
  std::string error;
  bool handled = false;
  const bool ok = primec::ir_lowerer::emitConversionsAndCallsControlExprTail(
      expr,
      {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
      [](const primec::Expr &, primec::ir_lowerer::LocalMap &) { return true; },
      [](const primec::Expr &valueExpr, const primec::ir_lowerer::LocalMap &) {
        if (valueExpr.kind == primec::Expr::Kind::BoolLiteral) {
          return primec::ir_lowerer::LocalInfo::ValueKind::Bool;
        }
        if (valueExpr.kind == primec::Expr::Kind::Literal) {
          return primec::ir_lowerer::LocalInfo::ValueKind::Int32;
        }
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](primec::ir_lowerer::LocalInfo::ValueKind leftKind, primec::ir_lowerer::LocalInfo::ValueKind rightKind) {
        return (leftKind == rightKind) ? leftKind : primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const std::vector<std::optional<std::string>> &) { return false; },
      [](const primec::Expr &) { return static_cast<const primec::Definition *>(nullptr); },
      [](const primec::Expr &) { return std::string("/if"); },
      [](const primec::Expr &, primec::Expr &, std::string &) { return false; },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::Kind::Value; },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo::Kind) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string(); },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      []() {},
      []() {},
      [](const primec::Expr &callExpr) { return callExpr.kind == primec::Expr::Kind::Call && callExpr.name == "return"; },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &callExpr) { return callExpr.kind == primec::Expr::Kind::Call && callExpr.name == "if"; },
      instructions,
      handled,
      error);

  CHECK_FALSE(ok);
  CHECK(handled);
  CHECK(error == "if branches must return compatible types");
}

TEST_CASE("ir lowerer conversions control-tail helper ignores unrelated calls") {
  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.name = "pow";

  std::vector<primec::IrInstruction> instructions;
  std::string error;
  bool handled = true;
  const bool ok = primec::ir_lowerer::emitConversionsAndCallsControlExprTail(
      expr,
      {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
      [](const primec::Expr &, primec::ir_lowerer::LocalMap &) { return true; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](primec::ir_lowerer::LocalInfo::ValueKind leftKind, primec::ir_lowerer::LocalInfo::ValueKind rightKind) {
        return (leftKind == rightKind) ? leftKind : primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const std::vector<std::optional<std::string>> &) { return false; },
      [](const primec::Expr &) { return static_cast<const primec::Definition *>(nullptr); },
      [](const primec::Expr &) { return std::string("/pow"); },
      [](const primec::Expr &, primec::Expr &, std::string &) { return false; },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::Kind::Value; },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo::Kind) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string(); },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      []() {},
      []() {},
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &) { return false; },
      instructions,
      handled,
      error);

  CHECK(ok);
  CHECK_FALSE(handled);
  CHECK(error.empty());
  CHECK(instructions.empty());
}

TEST_CASE("ir lowerer return inference helpers infer parameter locals") {
  primec::Expr bindingExpr;
  bindingExpr.name = "param";

  primec::ir_lowerer::LocalMap locals;
  std::string error;
  REQUIRE(primec::ir_lowerer::inferReturnInferenceBindingIntoLocals(
      bindingExpr,
      true,
      "/pkg/fn",
      locals,
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::Kind::Value; },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo::Kind) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Int64;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string(); },
      [](const primec::Expr &) { return false; },
      error));
  CHECK(error.empty());
  REQUIRE(locals.count("param") == 1u);
  CHECK(locals.at("param").valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int64);
}

TEST_CASE("ir lowerer return inference helpers report untyped bindings") {
  primec::Expr bindingExpr;
  bindingExpr.name = "tmp";

  primec::ir_lowerer::LocalMap locals;
  std::string error;
  CHECK_FALSE(primec::ir_lowerer::inferReturnInferenceBindingIntoLocals(
      bindingExpr,
      false,
      "/pkg/fn",
      locals,
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::Kind::Value; },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo::Kind) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string(); },
      [](const primec::Expr &) { return false; },
      error));
  CHECK(error == "native backend requires typed bindings on /pkg/fn");
  CHECK(locals.empty());
}

TEST_CASE("ir lowerer return inference helpers reject string references") {
  primec::Expr bindingExpr;
  bindingExpr.name = "label";

  primec::ir_lowerer::LocalMap locals;
  std::string error;
  CHECK_FALSE(primec::ir_lowerer::inferReturnInferenceBindingIntoLocals(
      bindingExpr,
      true,
      "/pkg/fn",
      locals,
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::Kind::Reference; },
      [](const primec::Expr &) { return true; },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo::Kind) {
        return primec::ir_lowerer::LocalInfo::ValueKind::String;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string(); },
      [](const primec::Expr &) { return true; },
      error));
  CHECK(error == "native backend does not support string pointers or references");
}

TEST_CASE("ir lowerer return inference helpers infer typed value returns") {
  primec::Definition def;
  def.fullPath = "/main";

  primec::Expr literalOne;
  literalOne.kind = primec::Expr::Kind::Literal;
  literalOne.literalValue = 1;

  primec::Expr bindingStmt;
  bindingStmt.isBinding = true;
  bindingStmt.name = "x";
  bindingStmt.args = {literalOne};

  primec::Expr returnStmt;
  returnStmt.kind = primec::Expr::Kind::Call;
  returnStmt.name = "return";
  primec::Expr nameX;
  nameX.kind = primec::Expr::Kind::Name;
  nameX.name = "x";
  returnStmt.args = {nameX};

  def.statements = {bindingStmt, returnStmt};
  def.hasReturnStatement = true;

  auto inferBinding = [](const primec::Expr &bindingExpr, bool, primec::ir_lowerer::LocalMap &locals, std::string &) {
    primec::ir_lowerer::LocalInfo info;
    info.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
    info.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
    locals[bindingExpr.name] = info;
    return true;
  };
  auto inferExprKind = [](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &locals) {
    if (expr.kind == primec::Expr::Kind::Literal) {
      return primec::ir_lowerer::LocalInfo::ValueKind::Int32;
    }
    if (expr.kind == primec::Expr::Kind::Name) {
      auto it = locals.find(expr.name);
      if (it != locals.end()) {
        return it->second.valueKind;
      }
    }
    return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  };

  primec::ir_lowerer::ReturnInferenceOptions options;
  options.missingReturnBehavior = primec::ir_lowerer::MissingReturnBehavior::Error;
  options.includeDefinitionReturnExpr = false;

  primec::ir_lowerer::ReturnInfo out;
  std::string error;
  REQUIRE(primec::ir_lowerer::inferDefinitionReturnType(
      def,
      {},
      inferBinding,
      inferExprKind,
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &, primec::Expr &, std::string &) { return false; },
      options,
      out,
      error));
  CHECK(error.empty());
  CHECK_FALSE(out.returnsVoid);
  CHECK_FALSE(out.returnsArray);
  CHECK(out.kind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
}

TEST_CASE("ir lowerer return inference helpers report missing return in error mode") {
  primec::Definition def;
  def.fullPath = "/missing";
  def.hasReturnStatement = false;

  primec::ir_lowerer::ReturnInferenceOptions options;
  options.missingReturnBehavior = primec::ir_lowerer::MissingReturnBehavior::Error;
  options.includeDefinitionReturnExpr = false;

  primec::ir_lowerer::ReturnInfo out;
  std::string error;
  CHECK_FALSE(primec::ir_lowerer::inferDefinitionReturnType(
      def,
      {},
      [](const primec::Expr &, bool, primec::ir_lowerer::LocalMap &, std::string &) { return true; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &, primec::Expr &, std::string &) { return false; },
      options,
      out,
      error));
  CHECK(error == "unable to infer return type on /missing");
}

TEST_CASE("ir lowerer return inference helpers reject mixed void and value returns") {
  primec::Definition def;
  def.fullPath = "/mixed";
  def.hasReturnStatement = true;

  primec::Expr returnVoid;
  returnVoid.kind = primec::Expr::Kind::Call;
  returnVoid.name = "return";

  primec::Expr literalOne;
  literalOne.kind = primec::Expr::Kind::Literal;
  literalOne.literalValue = 1;
  primec::Expr returnValue;
  returnValue.kind = primec::Expr::Kind::Call;
  returnValue.name = "return";
  returnValue.args = {literalOne};

  def.statements = {returnVoid, returnValue};

  primec::ir_lowerer::ReturnInferenceOptions options;
  options.missingReturnBehavior = primec::ir_lowerer::MissingReturnBehavior::Void;
  options.includeDefinitionReturnExpr = false;

  primec::ir_lowerer::ReturnInfo out;
  std::string error;
  CHECK_FALSE(primec::ir_lowerer::inferDefinitionReturnType(
      def,
      {},
      [](const primec::Expr &, bool, primec::ir_lowerer::LocalMap &, std::string &) { return true; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Int32;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &, primec::Expr &, std::string &) { return false; },
      options,
      out,
      error));
  CHECK(error == "conflicting return types on /mixed");
}

TEST_CASE("ir lowerer result helpers resolve Result.ok method") {
  primec::Expr resultName;
  resultName.kind = primec::Expr::Kind::Name;
  resultName.name = "Result";

  primec::Expr valueExpr;
  valueExpr.kind = primec::Expr::Kind::Literal;
  valueExpr.literalValue = 1;

  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.isMethodCall = true;
  callExpr.name = "ok";
  callExpr.args = {resultName, valueExpr};

  auto lookupLocal = [](const std::string &) { return primec::ir_lowerer::LocalResultInfo{}; };
  auto resolveNone = [](const primec::Expr &) -> const primec::Definition * { return nullptr; };
  auto lookupDefinition = [](const std::string &, primec::ir_lowerer::ResultExprInfo &) { return false; };

  primec::ir_lowerer::ResultExprInfo out;
  CHECK(primec::ir_lowerer::resolveResultExprInfo(
      callExpr, lookupLocal, resolveNone, resolveNone, lookupDefinition, out));
  CHECK(out.isResult);
  CHECK(out.hasValue);
}

TEST_CASE("ir lowerer result helpers resolve definition result metadata") {
  primec::Definition callee;
  callee.fullPath = "/make";

  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "make";

  auto lookupLocal = [](const std::string &) { return primec::ir_lowerer::LocalResultInfo{}; };
  auto resolveNone = [](const primec::Expr &) -> const primec::Definition * { return nullptr; };
  auto resolveDirect = [&](const primec::Expr &) -> const primec::Definition * { return &callee; };
  auto lookupDefinition = [](const std::string &path, primec::ir_lowerer::ResultExprInfo &out) {
    if (path != "/make") {
      return false;
    }
    out.isResult = true;
    out.hasValue = false;
    out.errorType = "FileError";
    return true;
  };

  primec::ir_lowerer::ResultExprInfo out;
  CHECK(primec::ir_lowerer::resolveResultExprInfo(
      callExpr, lookupLocal, resolveNone, resolveDirect, lookupDefinition, out));
  CHECK(out.isResult);
  CHECK_FALSE(out.hasValue);
  CHECK(out.errorType == "FileError");
}

TEST_CASE("ir lowerer result helpers resolve from locals and return-info lookups") {
  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo localResult;
  localResult.isResult = true;
  localResult.resultHasValue = true;
  localResult.resultErrorType = "FileError";
  locals.emplace("value", localResult);

  primec::Expr localNameExpr;
  localNameExpr.kind = primec::Expr::Kind::Name;
  localNameExpr.name = "value";

  primec::Definition methodDef;
  methodDef.fullPath = "/method";
  primec::Expr methodReceiver;
  methodReceiver.kind = primec::Expr::Kind::Name;
  methodReceiver.name = "obj";
  primec::Expr methodCallExpr;
  methodCallExpr.kind = primec::Expr::Kind::Call;
  methodCallExpr.isMethodCall = true;
  methodCallExpr.name = "do_it";
  methodCallExpr.args = {methodReceiver};

  auto resolveMethodCall = [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
    return &methodDef;
  };
  auto resolveDefinitionCall = [](const primec::Expr &) -> const primec::Definition * {
    return nullptr;
  };
  auto lookupReturnInfo = [](const std::string &path, primec::ir_lowerer::ReturnInfo &info) {
    if (path != "/method") {
      return false;
    }
    info = primec::ir_lowerer::ReturnInfo{};
    info.isResult = true;
    info.resultHasValue = false;
    info.resultErrorType = "MethodError";
    return true;
  };

  primec::ir_lowerer::ResultExprInfo out;
  CHECK(primec::ir_lowerer::resolveResultExprInfoFromLocals(
      localNameExpr, locals, resolveMethodCall, resolveDefinitionCall, lookupReturnInfo, out));
  CHECK(out.isResult);
  CHECK(out.hasValue);
  CHECK(out.errorType == "FileError");

  CHECK(primec::ir_lowerer::resolveResultExprInfoFromLocals(
      methodCallExpr, locals, resolveMethodCall, resolveDefinitionCall, lookupReturnInfo, out));
  CHECK(out.isResult);
  CHECK_FALSE(out.hasValue);
  CHECK(out.errorType == "MethodError");

  primec::Expr unknownName;
  unknownName.kind = primec::Expr::Kind::Name;
  unknownName.name = "missing";
  CHECK_FALSE(primec::ir_lowerer::resolveResultExprInfoFromLocals(
      unknownName, locals, resolveMethodCall, resolveDefinitionCall, lookupReturnInfo, out));
}

TEST_CASE("ir lowerer result helpers build locals-aware resolver adapters") {
  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo localResult;
  localResult.isResult = true;
  localResult.resultHasValue = false;
  localResult.resultErrorType = "LocalError";
  locals.emplace("resultLocal", localResult);

  primec::Expr localNameExpr;
  localNameExpr.kind = primec::Expr::Kind::Name;
  localNameExpr.name = "resultLocal";

  auto resolveMethodCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
    return nullptr;
  };
  auto resolveDefinitionCall = [](const primec::Expr &) -> const primec::Definition * {
    return nullptr;
  };
  auto lookupReturnInfo = [](const std::string &, primec::ir_lowerer::ReturnInfo &) {
    return false;
  };

  const auto resolveResultExprInfo = primec::ir_lowerer::makeResolveResultExprInfoFromLocals(
      resolveMethodCall, resolveDefinitionCall, lookupReturnInfo);

  primec::ir_lowerer::ResultExprInfo out;
  CHECK(resolveResultExprInfo(localNameExpr, locals, out));
  CHECK(out.isResult);
  CHECK_FALSE(out.hasValue);
  CHECK(out.errorType == "LocalError");

  primec::Expr unknownName;
  unknownName.kind = primec::Expr::Kind::Name;
  unknownName.name = "missing";
  CHECK_FALSE(resolveResultExprInfo(unknownName, locals, out));
}

TEST_CASE("ir lowerer result helpers classify Result.why error kinds") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;
  CHECK(primec::ir_lowerer::isSupportedResultWhyErrorKind(ValueKind::Int32));
  CHECK(primec::ir_lowerer::isSupportedResultWhyErrorKind(ValueKind::Int64));
  CHECK(primec::ir_lowerer::isSupportedResultWhyErrorKind(ValueKind::UInt64));
  CHECK(primec::ir_lowerer::isSupportedResultWhyErrorKind(ValueKind::Bool));
  CHECK_FALSE(primec::ir_lowerer::isSupportedResultWhyErrorKind(ValueKind::Float32));
  CHECK_FALSE(primec::ir_lowerer::isSupportedResultWhyErrorKind(ValueKind::String));
  CHECK_FALSE(primec::ir_lowerer::isSupportedResultWhyErrorKind(ValueKind::Unknown));
}

TEST_CASE("ir lowerer result helpers normalize Result.why type names") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;
  CHECK(primec::ir_lowerer::normalizeResultWhyErrorName("FileError", ValueKind::Int32) == "FileError");
  CHECK(primec::ir_lowerer::normalizeResultWhyErrorName("Other", ValueKind::Int32) == "i32");
  CHECK(primec::ir_lowerer::normalizeResultWhyErrorName("Other", ValueKind::Int64) == "i64");
  CHECK(primec::ir_lowerer::normalizeResultWhyErrorName("Other", ValueKind::UInt64) == "u64");
  CHECK(primec::ir_lowerer::normalizeResultWhyErrorName("Other", ValueKind::Bool) == "bool");
  CHECK(primec::ir_lowerer::normalizeResultWhyErrorName("CustomError", ValueKind::Float64) == "CustomError");
}

TEST_CASE("ir lowerer result helpers emit resolved Result.why calls") {
  using EmitResult = primec::ir_lowerer::ResultWhyCallEmitResult;
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;

  primec::Expr resultWhyExpr;
  resultWhyExpr.kind = primec::Expr::Kind::Call;
  resultWhyExpr.isMethodCall = true;
  resultWhyExpr.name = "why";
  resultWhyExpr.namespacePrefix = "/pkg";

  primec::Definition structWhyDef;
  structWhyDef.fullPath = "/MyError/why";
  structWhyDef.parameters.resize(1);
  primec::Definition i32WhyDef;
  i32WhyDef.fullPath = "/i32/why";
  i32WhyDef.parameters.resize(1);
  std::unordered_map<std::string, const primec::Definition *> defMap{
      {"/MyError/why", &structWhyDef},
      {"/i32/why", &i32WhyDef},
  };

  primec::ir_lowerer::LocalMap locals;
  std::string error;

  primec::ir_lowerer::ResultWhyCallOps ops;
  ops.resolveStructTypeName = [](const std::string &typeName,
                                 const std::string &,
                                 std::string &structPathOut) {
    if (typeName == "MyError") {
      structPathOut = "/MyError";
      return true;
    }
    return false;
  };
  ops.getReturnInfo = [](const std::string &path, primec::ir_lowerer::ReturnInfo &returnInfoOut) {
    if (path != "/MyError/why" && path != "/i32/why") {
      return false;
    }
    returnInfoOut = primec::ir_lowerer::ReturnInfo{};
    returnInfoOut.kind = ValueKind::String;
    return true;
  };
  ops.bindingKind = [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::Kind::Value; };
  ops.extractFirstBindingTypeTransform =
      [](const primec::Expr &, std::string &typeNameOut, std::vector<std::string> &templateArgsOut) {
        typeNameOut = "MyError";
        templateArgsOut.clear();
        return true;
      };
  ops.resolveStructSlotLayout =
      [](const std::string &structPath, primec::ir_lowerer::StructSlotLayoutInfo &layoutOut) {
    if (structPath != "/MyError") {
      return false;
    }
    layoutOut = primec::ir_lowerer::StructSlotLayoutInfo{};
    layoutOut.fields.push_back({0, ValueKind::Int32, ""});
    layoutOut.fields.push_back({1, ValueKind::Int32, ""});
    return true;
  };
  ops.valueKindFromTypeName = [](const std::string &typeName) {
    if (typeName == "AnyInt" || typeName == "MyError") {
      return ValueKind::Int32;
    }
    return ValueKind::Unknown;
  };
  ops.makeErrorValueExpr = [](primec::ir_lowerer::LocalMap &, ValueKind) {
    primec::Expr value;
    value.kind = primec::Expr::Kind::Name;
    value.name = "err";
    return value;
  };
  ops.makeBoolErrorExpr = [](primec::ir_lowerer::LocalMap &) {
    primec::Expr value;
    value.kind = primec::Expr::Kind::Name;
    value.name = "err_bool";
    return value;
  };

  int inlineCalls = 0;
  int fileErrorCalls = 0;
  int emptyStringCalls = 0;
  ops.emitInlineDefinitionCall = [&](const primec::Expr &callExpr,
                                     const primec::Definition &callee,
                                     const primec::ir_lowerer::LocalMap &) {
    ++inlineCalls;
    CHECK(callExpr.kind == primec::Expr::Kind::Call);
    CHECK(callee.fullPath == "/i32/why");
    return true;
  };
  ops.emitFileErrorWhy = [&](int32_t errorLocal) {
    ++fileErrorCalls;
    CHECK(errorLocal == 17);
    return true;
  };
  ops.emitEmptyString = [&]() {
    ++emptyStringCalls;
    return true;
  };

  primec::ir_lowerer::ResultExprInfo structResultInfo;
  structResultInfo.isResult = true;
  structResultInfo.errorType = "MyError";
  CHECK(primec::ir_lowerer::emitResolvedResultWhyCall(
            resultWhyExpr, structResultInfo, locals, 17, defMap, ops, error) ==
        EmitResult::Emitted);
  CHECK(emptyStringCalls == 1);
  CHECK(inlineCalls == 0);

  primec::ir_lowerer::ResultExprInfo i32ResultInfo;
  i32ResultInfo.isResult = true;
  i32ResultInfo.errorType = "AnyInt";
  error.clear();
  CHECK(primec::ir_lowerer::emitResolvedResultWhyCall(
            resultWhyExpr, i32ResultInfo, locals, 17, defMap, ops, error) ==
        EmitResult::Emitted);
  CHECK(inlineCalls == 1);

  ops.getReturnInfo = [](const std::string &, primec::ir_lowerer::ReturnInfo &returnInfoOut) {
    returnInfoOut = primec::ir_lowerer::ReturnInfo{};
    returnInfoOut.kind = ValueKind::Int32;
    return true;
  };
  error.clear();
  CHECK(primec::ir_lowerer::emitResolvedResultWhyCall(
            resultWhyExpr, i32ResultInfo, locals, 17, defMap, ops, error) ==
        EmitResult::Error);
  CHECK(error == "Result.why requires a string-returning why() for AnyInt");

  ops.resolveStructTypeName = [](const std::string &, const std::string &, std::string &) { return false; };
  ops.valueKindFromTypeName = [](const std::string &) { return ValueKind::Unknown; };
  ops.getReturnInfo = [](const std::string &, primec::ir_lowerer::ReturnInfo &) { return false; };
  primec::ir_lowerer::ResultExprInfo fileErrorResultInfo;
  fileErrorResultInfo.isResult = true;
  fileErrorResultInfo.errorType = "FileError";
  error.clear();
  CHECK(primec::ir_lowerer::emitResolvedResultWhyCall(
            resultWhyExpr, fileErrorResultInfo, locals, 17, defMap, ops, error) ==
        EmitResult::Emitted);
  CHECK(fileErrorCalls == 1);
}

TEST_CASE("ir lowerer flow helpers restore scoped state") {
  std::optional<primec::ir_lowerer::OnErrorHandler> onError;
  primec::ir_lowerer::OnErrorHandler initialHandler;
  initialHandler.handlerPath = "/initial";
  onError = initialHandler;
  {
    primec::ir_lowerer::OnErrorHandler nestedHandler;
    nestedHandler.handlerPath = "/nested";
    primec::ir_lowerer::OnErrorScope scope(onError, nestedHandler);
    REQUIRE(onError.has_value());
    CHECK(onError->handlerPath == "/nested");
  }
  REQUIRE(onError.has_value());
  CHECK(onError->handlerPath == "/initial");

  std::optional<primec::ir_lowerer::ResultReturnInfo> resultInfo;
  primec::ir_lowerer::ResultReturnInfo initialResult;
  initialResult.isResult = true;
  initialResult.hasValue = false;
  resultInfo = initialResult;
  {
    primec::ir_lowerer::ResultReturnInfo nestedResult;
    nestedResult.isResult = true;
    nestedResult.hasValue = true;
    primec::ir_lowerer::ResultReturnScope scope(resultInfo, nestedResult);
    REQUIRE(resultInfo.has_value());
    CHECK(resultInfo->hasValue);
  }
  REQUIRE(resultInfo.has_value());
  CHECK_FALSE(resultInfo->hasValue);
}

TEST_CASE("ir lowerer flow helpers emit file scope cleanup sequences") {
  auto checkCloseBlock = [](const std::vector<primec::IrInstruction> &instructions,
                            size_t start,
                            int32_t localIndex,
                            int32_t jumpImm) {
    REQUIRE(instructions.size() >= start + 7);
    CHECK(instructions[start + 0].op == primec::IrOpcode::LoadLocal);
    CHECK(instructions[start + 0].imm == static_cast<uint64_t>(localIndex));
    CHECK(instructions[start + 1].op == primec::IrOpcode::PushI64);
    CHECK(instructions[start + 1].imm == 0);
    CHECK(instructions[start + 2].op == primec::IrOpcode::CmpGeI64);
    CHECK(instructions[start + 3].op == primec::IrOpcode::JumpIfZero);
    CHECK(instructions[start + 3].imm == static_cast<uint64_t>(jumpImm));
    CHECK(instructions[start + 4].op == primec::IrOpcode::LoadLocal);
    CHECK(instructions[start + 4].imm == static_cast<uint64_t>(localIndex));
    CHECK(instructions[start + 5].op == primec::IrOpcode::FileClose);
    CHECK(instructions[start + 6].op == primec::IrOpcode::Pop);
  };

  std::vector<primec::IrInstruction> instructions;
  primec::ir_lowerer::emitFileCloseIfValid(instructions, 5);
  REQUIRE(instructions.size() == 7);
  checkCloseBlock(instructions, 0, 5, 7);

  instructions.clear();
  primec::ir_lowerer::emitFileScopeCleanup(instructions, {4, 9});
  REQUIRE(instructions.size() == 14);
  checkCloseBlock(instructions, 0, 9, 7);
  checkCloseBlock(instructions, 7, 4, 14);

  instructions.clear();
  primec::ir_lowerer::emitAllFileScopeCleanup(instructions, {{1}, {2, 3}});
  REQUIRE(instructions.size() == 21);
  checkCloseBlock(instructions, 0, 3, 7);
  checkCloseBlock(instructions, 7, 2, 14);
  checkCloseBlock(instructions, 14, 1, 21);

  instructions.clear();
  primec::ir_lowerer::emitFileScopeCleanup(instructions, {});
  CHECK(instructions.empty());
}

TEST_CASE("ir lowerer flow helpers emit struct copy sequences") {
  std::vector<primec::IrInstruction> instructions;

  CHECK(primec::ir_lowerer::emitStructCopyFromPtrs(instructions, 2, 3, 2));
  REQUIRE(instructions.size() == 14);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 2);
  CHECK(instructions[1].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[1].imm == 3);
  CHECK(instructions[2].op == primec::IrOpcode::LoadIndirect);
  CHECK(instructions[3].op == primec::IrOpcode::StoreIndirect);
  CHECK(instructions[4].op == primec::IrOpcode::Pop);
  CHECK(instructions[5].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[5].imm == 2);
  CHECK(instructions[6].op == primec::IrOpcode::PushI64);
  CHECK(instructions[6].imm == 16);
  CHECK(instructions[7].op == primec::IrOpcode::AddI64);
  CHECK(instructions[8].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[8].imm == 3);
  CHECK(instructions[9].op == primec::IrOpcode::PushI64);
  CHECK(instructions[9].imm == 16);
  CHECK(instructions[10].op == primec::IrOpcode::AddI64);
  CHECK(instructions[11].op == primec::IrOpcode::LoadIndirect);
  CHECK(instructions[12].op == primec::IrOpcode::StoreIndirect);
  CHECK(instructions[13].op == primec::IrOpcode::Pop);

  instructions.clear();
  int tempAllocs = 0;
  CHECK(primec::ir_lowerer::emitStructCopySlots(
      instructions,
      7,
      9,
      1,
      [&]() {
        tempAllocs++;
        return 11;
      }));
  CHECK(tempAllocs == 1);
  REQUIRE(instructions.size() == 7);
  CHECK(instructions[0].op == primec::IrOpcode::AddressOfLocal);
  CHECK(instructions[0].imm == 7);
  CHECK(instructions[1].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[1].imm == 11);
  CHECK(instructions[2].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[2].imm == 11);
  CHECK(instructions[3].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[3].imm == 9);
  CHECK(instructions[4].op == primec::IrOpcode::LoadIndirect);
  CHECK(instructions[5].op == primec::IrOpcode::StoreIndirect);
  CHECK(instructions[6].op == primec::IrOpcode::Pop);

  instructions.clear();
  CHECK(primec::ir_lowerer::emitStructCopyFromPtrs(instructions, 2, 3, 0));
  CHECK(instructions.empty());
}

TEST_CASE("ir lowerer flow helpers emit compare-to-zero sequences") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;
  std::vector<primec::IrInstruction> instructions;
  std::string error;

  CHECK(primec::ir_lowerer::emitCompareToZero(instructions, ValueKind::Int64, true, error));
  REQUIRE(instructions.size() == 2u);
  CHECK(instructions[0].op == primec::IrOpcode::PushI64);
  CHECK(instructions[0].imm == 0u);
  CHECK(instructions[1].op == primec::IrOpcode::CmpEqI64);
  CHECK(error.empty());

  instructions.clear();
  CHECK(primec::ir_lowerer::emitCompareToZero(instructions, ValueKind::Float32, false, error));
  REQUIRE(instructions.size() == 2u);
  CHECK(instructions[0].op == primec::IrOpcode::PushF32);
  CHECK(instructions[1].op == primec::IrOpcode::CmpNeF32);
  CHECK(error.empty());

  instructions.clear();
  CHECK(primec::ir_lowerer::emitCompareToZero(instructions, ValueKind::Bool, true, error));
  REQUIRE(instructions.size() == 2u);
  CHECK(instructions[0].op == primec::IrOpcode::PushI32);
  CHECK(instructions[1].op == primec::IrOpcode::CmpEqI32);

  instructions.clear();
  CHECK_FALSE(primec::ir_lowerer::emitCompareToZero(instructions, ValueKind::String, false, error));
  CHECK(error == "boolean conversion requires numeric operand");
  CHECK(instructions.empty());
}

TEST_CASE("ir lowerer flow helpers emit float literal sequences") {
  std::vector<primec::IrInstruction> instructions;
  std::string error;

  primec::Expr float64Expr;
  float64Expr.kind = primec::Expr::Kind::FloatLiteral;
  float64Expr.floatValue = "1.5";
  float64Expr.floatWidth = 64;
  CHECK(primec::ir_lowerer::emitFloatLiteral(instructions, float64Expr, error));
  REQUIRE(instructions.size() == 1u);
  CHECK(instructions[0].op == primec::IrOpcode::PushF64);
  CHECK(instructions[0].imm == 0x3ff8000000000000ull);
  CHECK(error.empty());

  instructions.clear();
  primec::Expr float32Expr;
  float32Expr.kind = primec::Expr::Kind::FloatLiteral;
  float32Expr.floatValue = "2.5";
  float32Expr.floatWidth = 32;
  CHECK(primec::ir_lowerer::emitFloatLiteral(instructions, float32Expr, error));
  REQUIRE(instructions.size() == 1u);
  CHECK(instructions[0].op == primec::IrOpcode::PushF32);
  CHECK(instructions[0].imm == 0x40200000ull);
  CHECK(error.empty());

  instructions.clear();
  primec::Expr invalidExpr = float64Expr;
  invalidExpr.floatValue = "not_a_float";
  CHECK_FALSE(primec::ir_lowerer::emitFloatLiteral(instructions, invalidExpr, error));
  CHECK(error == "invalid float literal");
  CHECK(instructions.empty());
}

TEST_CASE("ir lowerer flow helpers emit return for definition") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;
  std::vector<primec::IrInstruction> instructions;
  std::string error;

  primec::ir_lowerer::ReturnInfo info;
  info.returnsVoid = true;
  CHECK(primec::ir_lowerer::emitReturnForDefinition(instructions, "/pkg/voidFn", info, error));
  REQUIRE(instructions.size() == 1u);
  CHECK(instructions.back().op == primec::IrOpcode::ReturnVoid);

  info = primec::ir_lowerer::ReturnInfo{};
  info.kind = ValueKind::Int32;
  CHECK(primec::ir_lowerer::emitReturnForDefinition(instructions, "/pkg/i32Fn", info, error));
  REQUIRE(instructions.size() == 2u);
  CHECK(instructions.back().op == primec::IrOpcode::ReturnI32);

  info = primec::ir_lowerer::ReturnInfo{};
  info.kind = ValueKind::Int64;
  CHECK(primec::ir_lowerer::emitReturnForDefinition(instructions, "/pkg/i64Fn", info, error));
  REQUIRE(instructions.size() == 3u);
  CHECK(instructions.back().op == primec::IrOpcode::ReturnI64);

  info = primec::ir_lowerer::ReturnInfo{};
  info.kind = ValueKind::Float32;
  CHECK(primec::ir_lowerer::emitReturnForDefinition(instructions, "/pkg/f32Fn", info, error));
  REQUIRE(instructions.size() == 4u);
  CHECK(instructions.back().op == primec::IrOpcode::ReturnF32);

  info = primec::ir_lowerer::ReturnInfo{};
  info.kind = ValueKind::Float64;
  CHECK(primec::ir_lowerer::emitReturnForDefinition(instructions, "/pkg/f64Fn", info, error));
  REQUIRE(instructions.size() == 5u);
  CHECK(instructions.back().op == primec::IrOpcode::ReturnF64);

  info = primec::ir_lowerer::ReturnInfo{};
  info.returnsArray = true;
  info.kind = ValueKind::Unknown;
  CHECK(primec::ir_lowerer::emitReturnForDefinition(instructions, "/pkg/arrayFn", info, error));
  REQUIRE(instructions.size() == 6u);
  CHECK(instructions.back().op == primec::IrOpcode::ReturnI64);

  const size_t beforeFailure = instructions.size();
  info = primec::ir_lowerer::ReturnInfo{};
  info.kind = ValueKind::Unknown;
  CHECK_FALSE(primec::ir_lowerer::emitReturnForDefinition(instructions, "/pkg/badFn", info, error));
  CHECK(error == "native backend does not support return type on /pkg/badFn");
  CHECK(instructions.size() == beforeFailure);
}

TEST_CASE("ir lowerer flow helpers resolve and emit gpu builtins") {
  CHECK(primec::ir_lowerer::resolveGpuBuiltinLocalName("global_id_x") != nullptr);
  CHECK(std::string(primec::ir_lowerer::resolveGpuBuiltinLocalName("global_id_x")) == "__gpu_global_id_x");
  CHECK(std::string(primec::ir_lowerer::resolveGpuBuiltinLocalName("global_id_y")) == "__gpu_global_id_y");
  CHECK(std::string(primec::ir_lowerer::resolveGpuBuiltinLocalName("global_id_z")) == "__gpu_global_id_z");
  CHECK(primec::ir_lowerer::resolveGpuBuiltinLocalName("global_id_w") == nullptr);

  std::vector<primec::IrInstruction> instructions;
  auto emitInstruction = [&](primec::IrOpcode op, uint64_t imm) {
    instructions.push_back({op, imm});
  };

  std::string error;
  CHECK(primec::ir_lowerer::emitGpuBuiltinLoad(
      "global_id_y",
      [](const char *localName) -> std::optional<int32_t> {
        if (std::string(localName) == "__gpu_global_id_y") {
          return 42;
        }
        return std::nullopt;
      },
      emitInstruction,
      error));
  CHECK(error.empty());
  REQUIRE(instructions.size() == 1);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 42);

  instructions.clear();
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::emitGpuBuiltinLoad(
      "global_id_w",
      [](const char *) -> std::optional<int32_t> { return 0; },
      emitInstruction,
      error));
  CHECK(error == "native backend does not support gpu builtin: global_id_w");
  CHECK(instructions.empty());

  error.clear();
  CHECK_FALSE(primec::ir_lowerer::emitGpuBuiltinLoad(
      "global_id_x",
      [](const char *) -> std::optional<int32_t> { return std::nullopt; },
      emitInstruction,
      error));
  CHECK(error == "gpu builtin requires dispatch context: global_id_x");
  CHECK(instructions.empty());
}

TEST_CASE("ir lowerer flow helpers match and emit unary passthrough calls") {
  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "upload";

  primec::Expr argExpr;
  argExpr.kind = primec::Expr::Kind::Literal;
  argExpr.intWidth = 32;
  argExpr.literalValue = 7;
  callExpr.args.push_back(argExpr);

  bool emitCalled = false;
  std::string error;
  CHECK(primec::ir_lowerer::tryEmitUnaryPassthroughCall(
            callExpr,
            "upload",
            [&](const primec::Expr &forwardedExpr) {
              emitCalled = true;
              CHECK(forwardedExpr.kind == primec::Expr::Kind::Literal);
              CHECK(forwardedExpr.literalValue == 7u);
              return true;
            },
            error) == primec::ir_lowerer::UnaryPassthroughCallResult::Emitted);
  CHECK(emitCalled);
  CHECK(error.empty());

  primec::Expr notMatchedExpr = callExpr;
  notMatchedExpr.name = "buffer";
  emitCalled = false;
  CHECK(primec::ir_lowerer::tryEmitUnaryPassthroughCall(
            notMatchedExpr,
            "upload",
            [&](const primec::Expr &) {
              emitCalled = true;
              return true;
            },
            error) == primec::ir_lowerer::UnaryPassthroughCallResult::NotMatched);
  CHECK_FALSE(emitCalled);
  CHECK(error.empty());

  primec::Expr wrongArityExpr = callExpr;
  wrongArityExpr.name = "readback";
  wrongArityExpr.args.push_back(argExpr);
  emitCalled = false;
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitUnaryPassthroughCall(
            wrongArityExpr,
            "readback",
            [&](const primec::Expr &) {
              emitCalled = true;
              return true;
            },
            error) == primec::ir_lowerer::UnaryPassthroughCallResult::Error);
  CHECK_FALSE(emitCalled);
  CHECK(error == "readback requires exactly one argument");

  error.clear();
  CHECK(primec::ir_lowerer::tryEmitUnaryPassthroughCall(
            callExpr,
            "upload",
            [&](const primec::Expr &) {
              error = "emit failure";
              return false;
            },
            error) == primec::ir_lowerer::UnaryPassthroughCallResult::Error);
  CHECK(error == "emit failure");
}

TEST_CASE("ir lowerer flow helpers resolve buffer init info") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;

  primec::Expr bufferExpr;
  bufferExpr.kind = primec::Expr::Kind::Call;
  bufferExpr.name = "buffer";
  bufferExpr.templateArgs = {"i64"};

  primec::Expr countExpr;
  countExpr.kind = primec::Expr::Kind::Literal;
  countExpr.intWidth = 32;
  countExpr.literalValue = 3;
  bufferExpr.args.push_back(countExpr);

  primec::ir_lowerer::BufferInitInfo info;
  std::string error;
  CHECK(primec::ir_lowerer::resolveBufferInitInfo(
      bufferExpr,
      [](const std::string &typeName) {
        if (typeName == "i64") {
          return ValueKind::Int64;
        }
        if (typeName == "f32") {
          return ValueKind::Float32;
        }
        if (typeName == "string") {
          return ValueKind::String;
        }
        return ValueKind::Unknown;
      },
      info,
      error));
  CHECK(error.empty());
  CHECK(info.count == 3);
  CHECK(info.elemKind == ValueKind::Int64);
  CHECK(info.zeroOpcode == primec::IrOpcode::PushI64);

  primec::Expr wrongTemplateExpr = bufferExpr;
  wrongTemplateExpr.templateArgs.clear();
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::resolveBufferInitInfo(
      wrongTemplateExpr,
      [](const std::string &) { return ValueKind::Int32; },
      info,
      error));
  CHECK(error == "buffer requires exactly one template argument");

  primec::Expr wrongCountExpr = bufferExpr;
  wrongCountExpr.args.front().literalValue = 2147483648ull;
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::resolveBufferInitInfo(
      wrongCountExpr,
      [](const std::string &) { return ValueKind::Int32; },
      info,
      error));
  CHECK(error == "buffer size out of range");

  primec::Expr unsupportedElemExpr = bufferExpr;
  unsupportedElemExpr.templateArgs = {"string"};
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::resolveBufferInitInfo(
      unsupportedElemExpr,
      [](const std::string &typeName) {
        if (typeName == "string") {
          return ValueKind::String;
        }
        return ValueKind::Unknown;
      },
      info,
      error));
  CHECK(error == "buffer requires numeric/bool element type");
}

TEST_CASE("ir lowerer flow helpers resolve buffer load info") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;

  primec::Expr bufferLoadExpr;
  bufferLoadExpr.kind = primec::Expr::Kind::Call;
  bufferLoadExpr.name = "buffer_load";

  primec::Expr bufferNameExpr;
  bufferNameExpr.kind = primec::Expr::Kind::Name;
  bufferNameExpr.name = "buf";
  bufferLoadExpr.args.push_back(bufferNameExpr);

  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Literal;
  indexExpr.intWidth = 32;
  indexExpr.literalValue = 1;
  bufferLoadExpr.args.push_back(indexExpr);

  primec::ir_lowerer::BufferLoadInfo info;
  std::string error;
  CHECK(primec::ir_lowerer::resolveBufferLoadInfo(
      bufferLoadExpr,
      [](const std::string &name) -> std::optional<ValueKind> {
        if (name == "buf") {
          return ValueKind::Int64;
        }
        return std::nullopt;
      },
      [](const std::string &) { return ValueKind::Unknown; },
      [](const primec::Expr &) { return ValueKind::Int32; },
      info,
      error));
  CHECK(error.empty());
  CHECK(info.elemKind == ValueKind::Int64);
  CHECK(info.indexKind == ValueKind::Int32);

  primec::Expr inlineBufferExpr = bufferLoadExpr;
  inlineBufferExpr.args.front().kind = primec::Expr::Kind::Call;
  inlineBufferExpr.args.front().name = "buffer";
  inlineBufferExpr.args.front().templateArgs = {"f32"};
  CHECK(primec::ir_lowerer::resolveBufferLoadInfo(
      inlineBufferExpr,
      [](const std::string &) -> std::optional<ValueKind> { return std::nullopt; },
      [](const std::string &typeName) {
        if (typeName == "f32") {
          return ValueKind::Float32;
        }
        return ValueKind::Unknown;
      },
      [](const primec::Expr &) { return ValueKind::UInt64; },
      info,
      error));
  CHECK(error.empty());
  CHECK(info.elemKind == ValueKind::Float32);
  CHECK(info.indexKind == ValueKind::UInt64);

  primec::Expr wrongArityExpr = bufferLoadExpr;
  wrongArityExpr.args.pop_back();
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::resolveBufferLoadInfo(
      wrongArityExpr,
      [](const std::string &) -> std::optional<ValueKind> { return std::nullopt; },
      [](const std::string &) { return ValueKind::Unknown; },
      [](const primec::Expr &) { return ValueKind::Int32; },
      info,
      error));
  CHECK(error == "buffer_load requires buffer and index");

  primec::Expr unknownBufferExpr = bufferLoadExpr;
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::resolveBufferLoadInfo(
      unknownBufferExpr,
      [](const std::string &) -> std::optional<ValueKind> { return std::nullopt; },
      [](const std::string &) { return ValueKind::Unknown; },
      [](const primec::Expr &) { return ValueKind::Int32; },
      info,
      error));
  CHECK(error == "buffer_load requires numeric/bool buffer");

  error.clear();
  CHECK_FALSE(primec::ir_lowerer::resolveBufferLoadInfo(
      bufferLoadExpr,
      [](const std::string &) -> std::optional<ValueKind> { return ValueKind::Int32; },
      [](const std::string &) { return ValueKind::Unknown; },
      [](const primec::Expr &) { return ValueKind::Float32; },
      info,
      error));
  CHECK(error == "buffer_load requires integer index");
}

TEST_CASE("ir lowerer flow helpers emit buffer load calls") {
  primec::Expr bufferLoadExpr;
  bufferLoadExpr.kind = primec::Expr::Kind::Call;
  bufferLoadExpr.name = "buffer_load";

  primec::Expr targetExpr;
  targetExpr.kind = primec::Expr::Kind::Name;
  targetExpr.name = "buf";
  bufferLoadExpr.args.push_back(targetExpr);

  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Literal;
  indexExpr.intWidth = 32;
  indexExpr.literalValue = 2;
  bufferLoadExpr.args.push_back(indexExpr);

  std::vector<primec::IrInstruction> instructions;
  int emitStep = 0;
  int nextLocal = 40;
  CHECK(primec::ir_lowerer::emitBufferLoadCall(
      bufferLoadExpr,
      primec::ir_lowerer::LocalInfo::ValueKind::Int32,
      [&](const primec::Expr &) {
        if (emitStep == 0) {
          instructions.push_back({primec::IrOpcode::PushI64, 123});
        } else {
          instructions.push_back({primec::IrOpcode::PushI32, 7});
        }
        ++emitStep;
        return true;
      },
      [&]() { return nextLocal++; },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); }));
  REQUIRE(instructions.size() == 12);
  CHECK(instructions[0].op == primec::IrOpcode::PushI64);
  CHECK(instructions[1].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[1].imm == 40);
  CHECK(instructions[2].op == primec::IrOpcode::PushI32);
  CHECK(instructions[3].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[3].imm == 41);
  CHECK(instructions[4].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[5].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[6].op == primec::IrOpcode::PushI32);
  CHECK(instructions[7].op == primec::IrOpcode::AddI32);
  CHECK(instructions[8].op == primec::IrOpcode::PushI32);
  CHECK(instructions[9].op == primec::IrOpcode::MulI32);
  CHECK(instructions[10].op == primec::IrOpcode::AddI64);
  CHECK(instructions[11].op == primec::IrOpcode::LoadIndirect);

  instructions.clear();
  emitStep = 0;
  nextLocal = 10;
  CHECK_FALSE(primec::ir_lowerer::emitBufferLoadCall(
      bufferLoadExpr,
      primec::ir_lowerer::LocalInfo::ValueKind::Int32,
      [&](const primec::Expr &) {
        ++emitStep;
        return false;
      },
      [&]() { return nextLocal++; },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); }));
  CHECK(emitStep == 1);
  CHECK(instructions.empty());
}

TEST_CASE("ir lowerer flow helpers emit buffer builtin calls") {
  using Result = primec::ir_lowerer::BufferBuiltinCallEmitResult;
  using Kind = primec::ir_lowerer::LocalInfo::ValueKind;

  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.name = "upload";

  std::vector<primec::IrInstruction> instructions;
  primec::ir_lowerer::LocalMap locals;
  std::string error = "stale";

  CHECK(primec::ir_lowerer::tryEmitBufferBuiltinCall(
            expr,
            locals,
            [](const std::string &) { return Kind::Unknown; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return Kind::Int32; },
            [](int32_t) { return 0; },
            []() { return 0; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            error) == Result::NotMatched);
  CHECK(error == "stale");
  CHECK(instructions.empty());

  instructions.clear();
  error.clear();
  expr.name = "buffer";
  expr.templateArgs = {"i32"};
  primec::Expr countExpr;
  countExpr.kind = primec::Expr::Kind::Literal;
  countExpr.intWidth = 32;
  countExpr.literalValue = 2;
  expr.args = {countExpr};
  int32_t nextLocal = 20;
  CHECK(primec::ir_lowerer::tryEmitBufferBuiltinCall(
            expr,
            locals,
            [](const std::string &typeName) {
              if (typeName == "i32") {
                return Kind::Int32;
              }
              return Kind::Unknown;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return Kind::Int32; },
            [&](int32_t localCount) {
              const int32_t baseLocal = nextLocal;
              nextLocal += localCount;
              return baseLocal;
            },
            []() { return 0; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            error) == Result::Emitted);
  CHECK(error.empty());
  CHECK(nextLocal == 23);
  REQUIRE(instructions.size() == 7);
  CHECK(instructions[0].op == primec::IrOpcode::PushI32);
  CHECK(instructions[1].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[1].imm == 20);
  CHECK(instructions[3].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[3].imm == 21);
  CHECK(instructions[5].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[5].imm == 22);
  CHECK(instructions[6].op == primec::IrOpcode::AddressOfLocal);
  CHECK(instructions[6].imm == 20);

  instructions.clear();
  error.clear();
  expr.name = "buffer_load";
  expr.templateArgs.clear();
  primec::Expr bufferName;
  bufferName.kind = primec::Expr::Kind::Name;
  bufferName.name = "buf";
  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Literal;
  indexExpr.intWidth = 32;
  indexExpr.literalValue = 1;
  expr.args = {bufferName, indexExpr};
  primec::ir_lowerer::LocalInfo bufferInfo;
  bufferInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Buffer;
  bufferInfo.valueKind = Kind::Int64;
  locals.emplace("buf", bufferInfo);
  int emitStep = 0;
  bool allocRangeCalled = false;
  int32_t nextTemp = 40;
  CHECK(primec::ir_lowerer::tryEmitBufferBuiltinCall(
            expr,
            locals,
            [](const std::string &typeName) {
              if (typeName == "i64") {
                return Kind::Int64;
              }
              return Kind::Unknown;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return Kind::Int32; },
            [&](int32_t) {
              allocRangeCalled = true;
              return 0;
            },
            [&]() { return nextTemp++; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              if (emitStep == 0) {
                instructions.push_back({primec::IrOpcode::PushI64, 555});
              } else {
                instructions.push_back({primec::IrOpcode::PushI32, 3});
              }
              ++emitStep;
              return true;
            },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            error) == Result::Emitted);
  CHECK(error.empty());
  CHECK_FALSE(allocRangeCalled);
  CHECK(emitStep == 2);
  REQUIRE(instructions.size() == 12);
  CHECK(instructions[1].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[1].imm == 40);
  CHECK(instructions[3].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[3].imm == 41);
  CHECK(instructions.back().op == primec::IrOpcode::LoadIndirect);

  instructions.clear();
  error.clear();
  locals.clear();
  CHECK(primec::ir_lowerer::tryEmitBufferBuiltinCall(
            expr,
            locals,
            [](const std::string &) { return Kind::Unknown; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return Kind::Int32; },
            [](int32_t) { return 0; },
            []() { return 0; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            error) == Result::Error);
  CHECK(error == "buffer_load requires numeric/bool buffer");
  CHECK(instructions.empty());

  instructions.clear();
  error.clear();
  locals.emplace("buf", bufferInfo);
  CHECK(primec::ir_lowerer::tryEmitBufferBuiltinCall(
            expr,
            locals,
            [](const std::string &) { return Kind::Unknown; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return Kind::Int32; },
            [](int32_t) { return 0; },
            []() { return 70; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            error) == Result::Error);
  CHECK(instructions.empty());
}

TEST_CASE("ir lowerer string call helpers emit literal and binding values") {
  std::vector<primec::IrInstruction> instructions;
  auto emitInstruction = [&](primec::IrOpcode op, uint64_t imm) {
    instructions.push_back({op, imm});
  };
  auto internString = [](const std::string &text) -> int32_t {
    if (text == "hello") {
      return 7;
    }
    return 9;
  };

  primec::Expr literalArg;
  literalArg.kind = primec::Expr::Kind::StringLiteral;
  literalArg.stringValue = "\"hello\"utf8";

  primec::ir_lowerer::StringCallSource source = primec::ir_lowerer::StringCallSource::None;
  int32_t stringIndex = -1;
  bool argvChecked = true;
  std::string error;

  CHECK(primec::ir_lowerer::emitLiteralOrBindingStringCallValue(
            literalArg,
            internString,
            emitInstruction,
            [](const std::string &) { return primec::ir_lowerer::StringBindingInfo{}; },
            source,
            stringIndex,
            argvChecked,
            error) == primec::ir_lowerer::StringCallEmitResult::Handled);
  CHECK(source == primec::ir_lowerer::StringCallSource::TableIndex);
  CHECK(stringIndex == 7);
  CHECK(argvChecked);
  REQUIRE(instructions.size() == 1);
  CHECK(instructions[0].op == primec::IrOpcode::PushI64);
  CHECK(instructions[0].imm == 7);

  instructions.clear();
  primec::Expr nameArg;
  nameArg.kind = primec::Expr::Kind::Name;
  nameArg.name = "text";
  CHECK(primec::ir_lowerer::emitLiteralOrBindingStringCallValue(
            nameArg,
            internString,
            emitInstruction,
            [](const std::string &name) {
              primec::ir_lowerer::StringBindingInfo binding;
              if (name == "text") {
                binding.found = true;
                binding.isString = true;
                binding.localIndex = 42;
                binding.source = primec::ir_lowerer::StringCallSource::RuntimeIndex;
                binding.argvChecked = true;
              }
              return binding;
            },
            source,
            stringIndex,
            argvChecked,
            error) == primec::ir_lowerer::StringCallEmitResult::Handled);
  CHECK(source == primec::ir_lowerer::StringCallSource::RuntimeIndex);
  REQUIRE(instructions.size() == 1);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 42);
}

TEST_CASE("ir lowerer file write helpers resolve write opcodes") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;

  primec::IrOpcode opcode = primec::IrOpcode::PushI32;
  CHECK(primec::ir_lowerer::resolveFileWriteValueOpcode(ValueKind::Int32, opcode));
  CHECK(opcode == primec::IrOpcode::FileWriteI32);

  CHECK(primec::ir_lowerer::resolveFileWriteValueOpcode(ValueKind::Bool, opcode));
  CHECK(opcode == primec::IrOpcode::FileWriteI32);

  CHECK(primec::ir_lowerer::resolveFileWriteValueOpcode(ValueKind::Int64, opcode));
  CHECK(opcode == primec::IrOpcode::FileWriteI64);

  CHECK(primec::ir_lowerer::resolveFileWriteValueOpcode(ValueKind::UInt64, opcode));
  CHECK(opcode == primec::IrOpcode::FileWriteU64);

  CHECK_FALSE(primec::ir_lowerer::resolveFileWriteValueOpcode(ValueKind::Float64, opcode));
}

TEST_CASE("ir lowerer file write helpers resolve and emit file open modes") {
  primec::IrOpcode opcode = primec::IrOpcode::PushI32;
  CHECK(primec::ir_lowerer::resolveFileOpenModeOpcode("Read", opcode));
  CHECK(opcode == primec::IrOpcode::FileOpenRead);
  CHECK(primec::ir_lowerer::resolveFileOpenModeOpcode("Write", opcode));
  CHECK(opcode == primec::IrOpcode::FileOpenWrite);
  CHECK(primec::ir_lowerer::resolveFileOpenModeOpcode("Append", opcode));
  CHECK(opcode == primec::IrOpcode::FileOpenAppend);
  CHECK_FALSE(primec::ir_lowerer::resolveFileOpenModeOpcode("Invalid", opcode));

  std::vector<primec::IrInstruction> instructions;
  auto emitInstruction = [&](primec::IrOpcode op, uint64_t imm) {
    instructions.push_back({op, imm});
  };

  std::string error;
  CHECK(primec::ir_lowerer::emitFileOpenCall("Read", 7, emitInstruction, error));
  CHECK(primec::ir_lowerer::emitFileOpenCall("Write", 8, emitInstruction, error));
  CHECK(primec::ir_lowerer::emitFileOpenCall("Append", 9, emitInstruction, error));
  REQUIRE(instructions.size() == 3);
  CHECK(instructions[0].op == primec::IrOpcode::FileOpenRead);
  CHECK(instructions[0].imm == 7);
  CHECK(instructions[1].op == primec::IrOpcode::FileOpenWrite);
  CHECK(instructions[1].imm == 8);
  CHECK(instructions[2].op == primec::IrOpcode::FileOpenAppend);
  CHECK(instructions[2].imm == 9);
  CHECK(error.empty());

  CHECK_FALSE(primec::ir_lowerer::emitFileOpenCall("Invalid", 10, emitInstruction, error));
  CHECK(error == "File requires Read, Write, or Append mode");
  CHECK(instructions.size() == 3);
}

TEST_CASE("ir lowerer file write helpers emit write steps") {
  std::vector<primec::IrInstruction> instructions;
  auto emitInstruction = [&](primec::IrOpcode op, uint64_t imm) {
    instructions.push_back({op, imm});
  };
  std::string error;

  primec::Expr arg;
  arg.kind = primec::Expr::Kind::Name;
  arg.name = "value";

  bool emitExprCalled = false;
  CHECK(primec::ir_lowerer::emitFileWriteStep(
      arg,
      9,
      17,
      [](const primec::Expr &, int32_t &stringIndex, size_t &length) {
        stringIndex = 23;
        length = 5;
        return true;
      },
      [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::ValueKind::Unknown; },
      [&](const primec::Expr &) {
        emitExprCalled = true;
        return true;
      },
      emitInstruction,
      error));
  CHECK_FALSE(emitExprCalled);
  REQUIRE(instructions.size() == 3);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 9);
  CHECK(instructions[1].op == primec::IrOpcode::FileWriteString);
  CHECK(instructions[1].imm == 23);
  CHECK(instructions[2].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[2].imm == 17);
  CHECK(error.empty());

  instructions.clear();
  emitExprCalled = false;
  CHECK(primec::ir_lowerer::emitFileWriteStep(
      arg,
      4,
      8,
      [](const primec::Expr &, int32_t &, size_t &) { return false; },
      [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::ValueKind::Int64; },
      [&](const primec::Expr &) {
        emitExprCalled = true;
        emitInstruction(primec::IrOpcode::PushI64, 77);
        return true;
      },
      emitInstruction,
      error));
  CHECK(emitExprCalled);
  REQUIRE(instructions.size() == 4);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 4);
  CHECK(instructions[1].op == primec::IrOpcode::PushI64);
  CHECK(instructions[1].imm == 77);
  CHECK(instructions[2].op == primec::IrOpcode::FileWriteI64);
  CHECK(instructions[3].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[3].imm == 8);

  instructions.clear();
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::emitFileWriteStep(
      arg,
      4,
      8,
      [](const primec::Expr &, int32_t &, size_t &) { return false; },
      [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::ValueKind::Float32; },
      [](const primec::Expr &) { return true; },
      emitInstruction,
      error));
  CHECK(error == "file write requires integer/bool or string arguments");

  instructions.clear();
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::emitFileWriteStep(
      arg,
      4,
      8,
      [](const primec::Expr &, int32_t &, size_t &) { return false; },
      [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::ValueKind::Int32; },
      [](const primec::Expr &) { return false; },
      emitInstruction,
      error));
  CHECK(instructions.size() == 1);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 4);
}

TEST_CASE("ir lowerer file write helpers emit write and write_line calls") {
  std::vector<primec::IrInstruction> instructions;
  auto emitInstruction = [&](primec::IrOpcode op, uint64_t imm) {
    instructions.push_back({op, imm});
  };
  auto getInstructionCount = [&]() { return instructions.size(); };
  auto patchInstructionImm = [&](size_t index, int32_t imm) { instructions.at(index).imm = imm; };

  primec::Expr receiver;
  receiver.kind = primec::Expr::Kind::Name;
  receiver.name = "file";
  primec::Expr argA = receiver;
  argA.name = "a";
  primec::Expr argB = receiver;
  argB.name = "b";

  primec::Expr writeExpr;
  writeExpr.kind = primec::Expr::Kind::Call;
  writeExpr.name = "write";
  writeExpr.args = {receiver, argA, argB};

  int nextLocal = 20;
  int writeStepCalls = 0;
  CHECK(primec::ir_lowerer::emitFileWriteCall(
      writeExpr,
      5,
      [&](const primec::Expr &, int32_t errorLocal) {
        ++writeStepCalls;
        CHECK(errorLocal == 20);
        emitInstruction(primec::IrOpcode::PushI32, static_cast<uint64_t>(100 + writeStepCalls));
        emitInstruction(primec::IrOpcode::StoreLocal, static_cast<uint64_t>(errorLocal));
        return true;
      },
      [&]() { return nextLocal++; },
      emitInstruction,
      getInstructionCount,
      patchInstructionImm));
  CHECK(writeStepCalls == 2);
  REQUIRE(instructions.size() == 15);
  CHECK(instructions[0].op == primec::IrOpcode::PushI32);
  CHECK(instructions[1].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[1].imm == 20);
  CHECK(instructions[5].op == primec::IrOpcode::JumpIfZero);
  CHECK(instructions[5].imm == 8);
  CHECK(instructions[11].op == primec::IrOpcode::JumpIfZero);
  CHECK(instructions[11].imm == 14);
  CHECK(instructions[14].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[14].imm == 20);

  instructions.clear();
  primec::Expr writeLineExpr;
  writeLineExpr.kind = primec::Expr::Kind::Call;
  writeLineExpr.name = "write_line";
  writeLineExpr.args = {receiver};
  nextLocal = 30;
  writeStepCalls = 0;
  CHECK(primec::ir_lowerer::emitFileWriteCall(
      writeLineExpr,
      7,
      [&](const primec::Expr &, int32_t) {
        ++writeStepCalls;
        return true;
      },
      [&]() { return nextLocal++; },
      emitInstruction,
      getInstructionCount,
      patchInstructionImm));
  CHECK(writeStepCalls == 0);
  REQUIRE(instructions.size() == 10);
  CHECK(instructions[1].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[1].imm == 30);
  CHECK(instructions[5].op == primec::IrOpcode::JumpIfZero);
  CHECK(instructions[5].imm == 9);
  CHECK(instructions[6].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[6].imm == 7);
  CHECK(instructions[7].op == primec::IrOpcode::FileWriteNewline);
  CHECK(instructions[8].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[8].imm == 30);
  CHECK(instructions[9].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[9].imm == 30);

  instructions.clear();
  primec::Expr failingWriteExpr;
  failingWriteExpr.kind = primec::Expr::Kind::Call;
  failingWriteExpr.name = "write";
  failingWriteExpr.args = {receiver, argA};
  nextLocal = 40;
  CHECK_FALSE(primec::ir_lowerer::emitFileWriteCall(
      failingWriteExpr,
      9,
      [](const primec::Expr &, int32_t) { return false; },
      [&]() { return nextLocal++; },
      emitInstruction,
      getInstructionCount,
      patchInstructionImm));
  REQUIRE(instructions.size() == 6);
  CHECK(instructions[1].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[1].imm == 40);
  CHECK(instructions[5].op == primec::IrOpcode::JumpIfZero);
  CHECK(instructions[5].imm == 0);
}

TEST_CASE("ir lowerer file write helpers emit write_byte calls") {
  std::vector<primec::IrInstruction> instructions;
  auto emitInstruction = [&](primec::IrOpcode op, uint64_t imm) {
    instructions.push_back({op, imm});
  };

  primec::Expr receiver;
  receiver.kind = primec::Expr::Kind::Name;
  receiver.name = "file";
  primec::Expr byteArg;
  byteArg.kind = primec::Expr::Kind::Name;
  byteArg.name = "value";

  primec::Expr writeByteExpr;
  writeByteExpr.kind = primec::Expr::Kind::Call;
  writeByteExpr.name = "write_byte";
  writeByteExpr.args = {receiver, byteArg};

  int emitExprCalls = 0;
  std::string error;
  CHECK(primec::ir_lowerer::emitFileWriteByteCall(
      writeByteExpr,
      14,
      [&](const primec::Expr &) {
        ++emitExprCalls;
        emitInstruction(primec::IrOpcode::PushI32, 255);
        return true;
      },
      emitInstruction,
      error));
  CHECK(emitExprCalls == 1);
  CHECK(error.empty());
  REQUIRE(instructions.size() == 3);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 14);
  CHECK(instructions[1].op == primec::IrOpcode::PushI32);
  CHECK(instructions[1].imm == 255);
  CHECK(instructions[2].op == primec::IrOpcode::FileWriteByte);

  instructions.clear();
  error.clear();
  primec::Expr badArityExpr = writeByteExpr;
  badArityExpr.args = {receiver};
  CHECK_FALSE(primec::ir_lowerer::emitFileWriteByteCall(
      badArityExpr, 14, [](const primec::Expr &) { return true; }, emitInstruction, error));
  CHECK(error == "write_byte requires exactly one argument");
  CHECK(instructions.empty());

  instructions.clear();
  error.clear();
  emitExprCalls = 0;
  CHECK_FALSE(primec::ir_lowerer::emitFileWriteByteCall(
      writeByteExpr,
      6,
      [&](const primec::Expr &) {
        ++emitExprCalls;
        return false;
      },
      emitInstruction,
      error));
  CHECK(emitExprCalls == 1);
  REQUIRE(instructions.size() == 1);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 6);
}

TEST_CASE("ir lowerer file write helpers emit write_bytes calls") {
  std::vector<primec::IrInstruction> instructions;
  auto emitInstruction = [&](primec::IrOpcode op, uint64_t imm) {
    instructions.push_back({op, imm});
  };
  auto getInstructionCount = [&]() { return instructions.size(); };
  auto patchInstructionImm = [&](size_t index, int32_t imm) { instructions.at(index).imm = imm; };

  primec::Expr receiver;
  receiver.kind = primec::Expr::Kind::Name;
  receiver.name = "file";
  primec::Expr bytesArg;
  bytesArg.kind = primec::Expr::Kind::Name;
  bytesArg.name = "bytes";

  primec::Expr writeBytesExpr;
  writeBytesExpr.kind = primec::Expr::Kind::Call;
  writeBytesExpr.name = "write_bytes";
  writeBytesExpr.args = {receiver, bytesArg};

  int nextLocal = 50;
  int emitExprCalls = 0;
  std::string error;
  CHECK(primec::ir_lowerer::emitFileWriteBytesCall(
      writeBytesExpr,
      3,
      [&](const primec::Expr &) {
        ++emitExprCalls;
        emitInstruction(primec::IrOpcode::PushI64, 55);
        return true;
      },
      [&]() { return nextLocal++; },
      emitInstruction,
      getInstructionCount,
      patchInstructionImm,
      error));
  CHECK(error.empty());
  CHECK(emitExprCalls == 1);
  REQUIRE(instructions.size() == 34);
  CHECK(instructions[0].op == primec::IrOpcode::PushI64);
  CHECK(instructions[1].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[1].imm == 50);
  CHECK(instructions[33].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[33].imm == 53);

  instructions.clear();
  error.clear();
  primec::Expr badArityExpr = writeBytesExpr;
  badArityExpr.args = {receiver};
  CHECK_FALSE(primec::ir_lowerer::emitFileWriteBytesCall(
      badArityExpr,
      3,
      [](const primec::Expr &) { return true; },
      [&]() { return 0; },
      emitInstruction,
      getInstructionCount,
      patchInstructionImm,
      error));
  CHECK(error == "write_bytes requires exactly one argument");
  CHECK(instructions.empty());

  instructions.clear();
  error.clear();
  emitExprCalls = 0;
  nextLocal = 60;
  CHECK_FALSE(primec::ir_lowerer::emitFileWriteBytesCall(
      writeBytesExpr,
      3,
      [&](const primec::Expr &) {
        ++emitExprCalls;
        return false;
      },
      [&]() { return nextLocal++; },
      emitInstruction,
      getInstructionCount,
      patchInstructionImm,
      error));
  CHECK(emitExprCalls == 1);
  CHECK(error.empty());
  CHECK(instructions.empty());
}

TEST_CASE("ir lowerer file write helpers emit write-bytes loops") {
  std::vector<primec::IrInstruction> instructions;
  auto emitInstruction = [&](primec::IrOpcode op, uint64_t imm) {
    instructions.push_back({op, imm});
  };
  auto getInstructionCount = [&]() { return instructions.size(); };
  auto patchInstructionImm = [&](size_t index, int32_t imm) { instructions.at(index).imm = imm; };

  primec::Expr bytesExpr;
  bytesExpr.kind = primec::Expr::Kind::Name;
  bytesExpr.name = "bytes";

  int nextLocal = 10;
  int emitExprCalls = 0;
  CHECK(primec::ir_lowerer::emitFileWriteBytesLoop(
      bytesExpr,
      3,
      [&](const primec::Expr &) {
        emitExprCalls++;
        emitInstruction(primec::IrOpcode::PushI64, 55);
        return true;
      },
      [&]() { return nextLocal++; },
      emitInstruction,
      getInstructionCount,
      patchInstructionImm));
  CHECK(emitExprCalls == 1);
  REQUIRE(instructions.size() == 34);
  CHECK(instructions[0].op == primec::IrOpcode::PushI64);
  CHECK(instructions[1].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[1].imm == 10);
  CHECK(instructions[12].op == primec::IrOpcode::JumpIfZero);
  CHECK(instructions[12].imm == 33);
  CHECK(instructions[16].op == primec::IrOpcode::JumpIfZero);
  CHECK(instructions[16].imm == 33);
  CHECK(instructions[17].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[17].imm == 3);
  CHECK(instructions[32].op == primec::IrOpcode::Jump);
  CHECK(instructions[32].imm == 9);
  CHECK(instructions[33].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[33].imm == 13);

  instructions.clear();
  nextLocal = 40;
  CHECK_FALSE(primec::ir_lowerer::emitFileWriteBytesLoop(
      bytesExpr,
      7,
      [](const primec::Expr &) { return false; },
      [&]() { return nextLocal++; },
      emitInstruction,
      getInstructionCount,
      patchInstructionImm));
  CHECK(instructions.empty());
}

TEST_CASE("ir lowerer file write helpers dispatch file-handle methods") {
  using Result = primec::ir_lowerer::FileHandleMethodCallEmitResult;

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo fileInfo;
  fileInfo.index = 7;
  fileInfo.isFileHandle = true;
  locals.emplace("file", fileInfo);

  primec::ir_lowerer::LocalInfo valueInfo;
  valueInfo.index = 11;
  locals.emplace("value", valueInfo);

  std::vector<primec::IrInstruction> instructions;
  auto emitInstruction = [&](primec::IrOpcode op, uint64_t imm) {
    instructions.push_back({op, imm});
  };
  auto getInstructionCount = [&]() { return instructions.size(); };
  auto patchInstructionImm = [&](size_t index, int32_t imm) { instructions.at(index).imm = imm; };

  primec::Expr receiver;
  receiver.kind = primec::Expr::Kind::Name;
  receiver.name = "file";
  primec::Expr valueArg;
  valueArg.kind = primec::Expr::Kind::Name;
  valueArg.name = "value";

  primec::Expr writeExpr;
  writeExpr.kind = primec::Expr::Kind::Call;
  writeExpr.name = "write";
  writeExpr.isMethodCall = true;
  writeExpr.args = {receiver, valueArg};

  int emitExprCalls = 0;
  int nextLocal = 20;
  std::string error;
  CHECK(primec::ir_lowerer::tryEmitFileHandleMethodCall(
            writeExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) {
              return false;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Int32;
            },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &localMap) {
              ++emitExprCalls;
              CHECK(localMap.count("file") == 1);
              emitInstruction(primec::IrOpcode::PushI32, 99);
              return true;
            },
            [&]() { return nextLocal++; },
            emitInstruction,
            getInstructionCount,
            patchInstructionImm,
            error) == Result::Emitted);
  CHECK(emitExprCalls == 1);
  CHECK(error.empty());
  CHECK_FALSE(instructions.empty());

  primec::Expr badWriteByteExpr;
  badWriteByteExpr.kind = primec::Expr::Kind::Call;
  badWriteByteExpr.name = "write_byte";
  badWriteByteExpr.isMethodCall = true;
  badWriteByteExpr.args = {receiver};
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitFileHandleMethodCall(
            badWriteByteExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) {
              return false;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Int32;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            []() { return 0; },
            [](primec::IrOpcode, uint64_t) {},
            []() { return static_cast<size_t>(0); },
            [](size_t, int32_t) {},
            error) == Result::Error);
  CHECK(error == "write_byte requires exactly one argument");

  primec::Expr unknownMethodExpr;
  unknownMethodExpr.kind = primec::Expr::Kind::Call;
  unknownMethodExpr.name = "noop";
  unknownMethodExpr.isMethodCall = true;
  unknownMethodExpr.args = {receiver, valueArg};
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitFileHandleMethodCall(
            unknownMethodExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) {
              return false;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Int32;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            []() { return 0; },
            [](primec::IrOpcode, uint64_t) {},
            []() { return static_cast<size_t>(0); },
            [](size_t, int32_t) {},
            error) == Result::NotMatched);

  primec::Expr nonFileReceiver;
  nonFileReceiver.kind = primec::Expr::Kind::Name;
  nonFileReceiver.name = "value";
  primec::Expr nonFileExpr;
  nonFileExpr.kind = primec::Expr::Kind::Call;
  nonFileExpr.name = "write";
  nonFileExpr.isMethodCall = true;
  nonFileExpr.args = {nonFileReceiver, valueArg};
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitFileHandleMethodCall(
            nonFileExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) {
              return false;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Int32;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            []() { return 0; },
            [](primec::IrOpcode, uint64_t) {},
            []() { return static_cast<size_t>(0); },
            [](size_t, int32_t) {},
            error) == Result::NotMatched);
}

TEST_CASE("ir lowerer file write helpers emit flush and close calls") {
  std::vector<primec::IrInstruction> instructions;
  auto emitInstruction = [&](primec::IrOpcode op, uint64_t imm) {
    instructions.push_back({op, imm});
  };

  primec::ir_lowerer::emitFileFlushCall(12, emitInstruction);
  REQUIRE(instructions.size() == 2);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 12);
  CHECK(instructions[1].op == primec::IrOpcode::FileFlush);

  int nextLocal = 30;
  primec::ir_lowerer::emitFileCloseCall(12, [&]() { return nextLocal++; }, emitInstruction);
  REQUIRE(instructions.size() == 8);
  CHECK(instructions[2].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[2].imm == 12);
  CHECK(instructions[3].op == primec::IrOpcode::FileClose);
  CHECK(instructions[4].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[4].imm == 30);
  CHECK(instructions[5].op == primec::IrOpcode::PushI64);
  CHECK(instructions[5].imm == static_cast<uint64_t>(static_cast<int64_t>(-1)));
  CHECK(instructions[6].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[6].imm == 12);
  CHECK(instructions[7].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[7].imm == 30);
}

TEST_CASE("ir lowerer string call helpers emit values from locals") {
  std::vector<primec::IrInstruction> instructions;
  auto emitInstruction = [&](primec::IrOpcode op, uint64_t imm) {
    instructions.push_back({op, imm});
  };
  auto internString = [](const std::string &text) -> int32_t {
    return text == "hello" ? 5 : 8;
  };
  auto resolveArrayAccessName = [](const primec::Expr &, std::string &) { return false; };
  auto isEntryArgsName = [](const primec::Expr &) { return false; };
  auto resolveStringIndexOps =
      [](const primec::Expr &, const std::string &, primec::ir_lowerer::StringIndexOps &, std::string &) {
        return false;
      };
  auto emitExpr = [](const primec::Expr &) { return true; };
  auto inferCallReturnsString = [](const primec::Expr &) { return false; };
  auto allocTempLocal = []() { return 0; };
  auto getInstructionCount = [&]() { return instructions.size(); };
  auto patchInstructionImm = [&](size_t index, int32_t imm) { instructions.at(index).imm = imm; };
  auto emitArrayIndexOutOfBounds = []() {};

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo localString;
  localString.index = 42;
  localString.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::String;
  localString.stringSource = primec::ir_lowerer::LocalInfo::StringSource::RuntimeIndex;
  localString.stringIndex = 11;
  localString.argvChecked = true;
  locals.emplace("text", localString);

  primec::Expr nameArg;
  nameArg.kind = primec::Expr::Kind::Name;
  nameArg.name = "text";
  primec::ir_lowerer::LocalInfo::StringSource sourceOut = primec::ir_lowerer::LocalInfo::StringSource::None;
  int32_t stringIndexOut = -1;
  bool argvCheckedOut = false;
  std::string error;
  CHECK(primec::ir_lowerer::emitStringValueForCallFromLocals(nameArg,
                                                              locals,
                                                              internString,
                                                              emitInstruction,
                                                              resolveArrayAccessName,
                                                              isEntryArgsName,
                                                              resolveStringIndexOps,
                                                              emitExpr,
                                                              inferCallReturnsString,
                                                              allocTempLocal,
                                                              getInstructionCount,
                                                              patchInstructionImm,
                                                              emitArrayIndexOutOfBounds,
                                                              sourceOut,
                                                              stringIndexOut,
                                                              argvCheckedOut,
                                                              error));
  CHECK(error.empty());
  CHECK(sourceOut == primec::ir_lowerer::LocalInfo::StringSource::RuntimeIndex);
  CHECK(stringIndexOut == 11);
  CHECK(argvCheckedOut);
  REQUIRE(instructions.size() == 1);
  CHECK(instructions.front().op == primec::IrOpcode::LoadLocal);
  CHECK(instructions.front().imm == 42);

  instructions.clear();
  primec::Expr badName = nameArg;
  badName.name = "missing";
  CHECK_FALSE(primec::ir_lowerer::emitStringValueForCallFromLocals(badName,
                                                                    locals,
                                                                    internString,
                                                                    emitInstruction,
                                                                    resolveArrayAccessName,
                                                                    isEntryArgsName,
                                                                    resolveStringIndexOps,
                                                                    emitExpr,
                                                                    inferCallReturnsString,
                                                                    allocTempLocal,
                                                                    getInstructionCount,
                                                                    patchInstructionImm,
                                                                    emitArrayIndexOutOfBounds,
                                                                    sourceOut,
                                                                    stringIndexOut,
                                                                    argvCheckedOut,
                                                                    error));
  CHECK(error == "native backend does not know identifier: missing");
}

TEST_CASE("ir lowerer string call helpers emit entry-args call values from locals") {
  std::vector<primec::IrInstruction> instructions;
  auto emitInstruction = [&](primec::IrOpcode op, uint64_t imm) {
    instructions.push_back({op, imm});
  };
  auto getInstructionCount = [&]() { return instructions.size(); };
  auto patchInstructionImm = [&](size_t index, int32_t imm) { instructions.at(index).imm = imm; };

  primec::Expr argvExpr;
  argvExpr.kind = primec::Expr::Kind::Name;
  argvExpr.name = "argv";
  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Literal;
  indexExpr.literalValue = 2;
  primec::Expr atCall;
  atCall.kind = primec::Expr::Kind::Call;
  atCall.name = "at";
  atCall.args = {argvExpr, indexExpr};

  primec::ir_lowerer::LocalInfo::StringSource sourceOut = primec::ir_lowerer::LocalInfo::StringSource::None;
  int32_t stringIndexOut = -1;
  bool argvCheckedOut = false;
  std::string error;
  int emittedIndexExprs = 0;

  CHECK(primec::ir_lowerer::emitStringValueForCallFromLocals(
      atCall,
      {},
      [](const std::string &) { return 0; },
      emitInstruction,
      [](const primec::Expr &callExpr, std::string &nameOut) {
        if (callExpr.name != "at" && callExpr.name != "unchecked_at") {
          return false;
        }
        nameOut = callExpr.name;
        return true;
      },
      [](const primec::Expr &targetExpr) {
        return targetExpr.kind == primec::Expr::Kind::Name && targetExpr.name == "argv";
      },
      [](const primec::Expr &, const std::string &, primec::ir_lowerer::StringIndexOps &ops, std::string &) {
        ops.pushZero = primec::IrOpcode::PushI32;
        ops.cmpLt = primec::IrOpcode::CmpLtI32;
        ops.cmpGe = primec::IrOpcode::CmpGeI32;
        ops.skipNegativeCheck = false;
        return true;
      },
      [&](const primec::Expr &) {
        emittedIndexExprs++;
        emitInstruction(primec::IrOpcode::PushI32, 2);
        return true;
      },
      [](const primec::Expr &) { return false; },
      []() { return 11; },
      getInstructionCount,
      patchInstructionImm,
      [&]() { emitInstruction(primec::IrOpcode::PushI32, 999); },
      sourceOut,
      stringIndexOut,
      argvCheckedOut,
      error));
  CHECK(error.empty());
  CHECK(sourceOut == primec::ir_lowerer::LocalInfo::StringSource::ArgvIndex);
  CHECK(argvCheckedOut);
  CHECK(emittedIndexExprs == 1);
  REQUIRE_FALSE(instructions.empty());
  CHECK(instructions.back().op == primec::IrOpcode::LoadLocal);
}

TEST_CASE("ir lowerer string call helpers surface errors and not-handled cases") {
  std::vector<primec::IrInstruction> instructions;
  auto emitInstruction = [&](primec::IrOpcode op, uint64_t imm) {
    instructions.push_back({op, imm});
  };
  auto internString = [](const std::string &) -> int32_t { return 1; };

  primec::Expr badLiteral;
  badLiteral.kind = primec::Expr::Kind::StringLiteral;
  badLiteral.stringValue = "\"x\"bogus";
  primec::ir_lowerer::StringCallSource source = primec::ir_lowerer::StringCallSource::None;
  int32_t stringIndex = -1;
  bool argvChecked = true;
  std::string error;
  CHECK(primec::ir_lowerer::emitLiteralOrBindingStringCallValue(
            badLiteral,
            internString,
            emitInstruction,
            [](const std::string &) { return primec::ir_lowerer::StringBindingInfo{}; },
            source,
            stringIndex,
            argvChecked,
            error) == primec::ir_lowerer::StringCallEmitResult::Error);
  CHECK(error.find("unknown string literal suffix") != std::string::npos);

  instructions.clear();
  primec::Expr unknownName;
  unknownName.kind = primec::Expr::Kind::Name;
  unknownName.name = "missing";
  CHECK(primec::ir_lowerer::emitLiteralOrBindingStringCallValue(
            unknownName,
            internString,
            emitInstruction,
            [](const std::string &) { return primec::ir_lowerer::StringBindingInfo{}; },
            source,
            stringIndex,
            argvChecked,
            error) == primec::ir_lowerer::StringCallEmitResult::Error);
  CHECK(error == "native backend does not know identifier: missing");

  instructions.clear();
  primec::Expr callArg;
  callArg.kind = primec::Expr::Kind::Call;
  callArg.name = "at";
  CHECK(primec::ir_lowerer::emitLiteralOrBindingStringCallValue(
            callArg,
            internString,
            emitInstruction,
            [](const std::string &) { return primec::ir_lowerer::StringBindingInfo{}; },
            source,
            stringIndex,
            argvChecked,
            error) == primec::ir_lowerer::StringCallEmitResult::NotHandled);
  CHECK(instructions.empty());
}

TEST_CASE("ir lowerer string call helpers handle call-expression paths") {
  std::vector<primec::IrInstruction> instructions;
  auto emitInstruction = [&](primec::IrOpcode op, uint64_t imm) {
    instructions.push_back({op, imm});
  };
  auto getInstructionCount = [&]() { return instructions.size(); };
  auto patchInstructionImm = [&](size_t index, int32_t imm) { instructions.at(index).imm = imm; };

  primec::Expr argvExpr;
  argvExpr.kind = primec::Expr::Kind::Name;
  argvExpr.name = "argv";
  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Literal;
  indexExpr.literalValue = 2;
  primec::Expr atCall;
  atCall.kind = primec::Expr::Kind::Call;
  atCall.name = "at";
  atCall.args = {argvExpr, indexExpr};

  primec::ir_lowerer::StringCallSource source = primec::ir_lowerer::StringCallSource::None;
  bool argvChecked = false;
  std::string error;
  int emittedIndexExprs = 0;

  CHECK(primec::ir_lowerer::emitCallStringCallValue(
            atCall,
            [](const primec::Expr &callExpr, std::string &nameOut) {
              if (callExpr.name == "at" || callExpr.name == "unchecked_at") {
                nameOut = callExpr.name;
                return true;
              }
              return false;
            },
            [](const primec::Expr &targetExpr) {
              return targetExpr.kind == primec::Expr::Kind::Name && targetExpr.name == "argv";
            },
            [](const primec::Expr &, const std::string &, primec::ir_lowerer::StringIndexOps &ops, std::string &) {
              ops.pushZero = primec::IrOpcode::PushI32;
              ops.cmpLt = primec::IrOpcode::CmpLtI32;
              ops.cmpGe = primec::IrOpcode::CmpGeI32;
              ops.skipNegativeCheck = false;
              return true;
            },
            [&](const primec::Expr &) {
              emittedIndexExprs++;
              emitInstruction(primec::IrOpcode::PushI32, 2);
              return true;
            },
            [](const primec::Expr &) { return false; },
            []() { return 11; },
            emitInstruction,
            getInstructionCount,
            patchInstructionImm,
            [&]() { emitInstruction(primec::IrOpcode::PushI32, 999); },
            source,
            argvChecked,
            error) == primec::ir_lowerer::StringCallEmitResult::Handled);
  CHECK(error.empty());
  CHECK(source == primec::ir_lowerer::StringCallSource::ArgvIndex);
  CHECK(argvChecked);
  CHECK(emittedIndexExprs == 1);
  REQUIRE_FALSE(instructions.empty());
  CHECK(instructions.back().op == primec::IrOpcode::LoadLocal);
  CHECK(instructions.back().imm == 11);

  instructions.clear();
  source = primec::ir_lowerer::StringCallSource::None;
  argvChecked = true;
  bool runtimeExprEmitted = false;
  primec::Expr runtimeCall;
  runtimeCall.kind = primec::Expr::Kind::Call;
  runtimeCall.name = "build_string";
  CHECK(primec::ir_lowerer::emitCallStringCallValue(
            runtimeCall,
            [](const primec::Expr &, std::string &) { return false; },
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &, const std::string &, primec::ir_lowerer::StringIndexOps &, std::string &) {
              return false;
            },
            [&](const primec::Expr &) {
              runtimeExprEmitted = true;
              emitInstruction(primec::IrOpcode::LoadLocal, 4);
              return true;
            },
            [](const primec::Expr &) { return true; },
            []() { return 0; },
            emitInstruction,
            getInstructionCount,
            patchInstructionImm,
            []() {},
            source,
            argvChecked,
            error) == primec::ir_lowerer::StringCallEmitResult::Handled);
  CHECK(source == primec::ir_lowerer::StringCallSource::RuntimeIndex);
  CHECK(runtimeExprEmitted);
}

TEST_CASE("ir lowerer string call helpers report call-expression diagnostics") {
  primec::Expr badCall;
  badCall.kind = primec::Expr::Kind::Call;
  badCall.name = "bad";

  primec::ir_lowerer::StringCallSource source = primec::ir_lowerer::StringCallSource::None;
  bool argvChecked = true;
  std::string error;
  CHECK(primec::ir_lowerer::emitCallStringCallValue(
            badCall,
            [](const primec::Expr &, std::string &) { return false; },
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &, const std::string &, primec::ir_lowerer::StringIndexOps &, std::string &) {
              return false;
            },
            [](const primec::Expr &) { return true; },
            [](const primec::Expr &) { return false; },
            []() { return 0; },
            [](primec::IrOpcode, uint64_t) {},
            []() { return size_t{0}; },
            [](size_t, int32_t) {},
            []() {},
            source,
            argvChecked,
            error) == primec::ir_lowerer::StringCallEmitResult::Error);
  CHECK(error == "native backend requires string arguments to use string literals, bindings, or entry args");
}

TEST_CASE("ir opcode allowlist matches vm/native support matrix") {
  const std::vector<primec::IrOpcode> expected = {
      primec::IrOpcode::PushI32,
      primec::IrOpcode::PushI64,
      primec::IrOpcode::LoadLocal,
      primec::IrOpcode::StoreLocal,
      primec::IrOpcode::AddressOfLocal,
      primec::IrOpcode::LoadIndirect,
      primec::IrOpcode::StoreIndirect,
      primec::IrOpcode::Dup,
      primec::IrOpcode::Pop,
      primec::IrOpcode::AddI32,
      primec::IrOpcode::SubI32,
      primec::IrOpcode::MulI32,
      primec::IrOpcode::DivI32,
      primec::IrOpcode::NegI32,
      primec::IrOpcode::AddI64,
      primec::IrOpcode::SubI64,
      primec::IrOpcode::MulI64,
      primec::IrOpcode::DivI64,
      primec::IrOpcode::DivU64,
      primec::IrOpcode::NegI64,
      primec::IrOpcode::CmpEqI32,
      primec::IrOpcode::CmpNeI32,
      primec::IrOpcode::CmpLtI32,
      primec::IrOpcode::CmpLeI32,
      primec::IrOpcode::CmpGtI32,
      primec::IrOpcode::CmpGeI32,
      primec::IrOpcode::CmpEqI64,
      primec::IrOpcode::CmpNeI64,
      primec::IrOpcode::CmpLtI64,
      primec::IrOpcode::CmpLeI64,
      primec::IrOpcode::CmpGtI64,
      primec::IrOpcode::CmpGeI64,
      primec::IrOpcode::CmpLtU64,
      primec::IrOpcode::CmpLeU64,
      primec::IrOpcode::CmpGtU64,
      primec::IrOpcode::CmpGeU64,
      primec::IrOpcode::JumpIfZero,
      primec::IrOpcode::Jump,
      primec::IrOpcode::ReturnVoid,
      primec::IrOpcode::ReturnI32,
      primec::IrOpcode::ReturnI64,
      primec::IrOpcode::PrintI32,
      primec::IrOpcode::PrintI64,
      primec::IrOpcode::PrintU64,
      primec::IrOpcode::PrintString,
      primec::IrOpcode::PushArgc,
      primec::IrOpcode::PrintArgv,
      primec::IrOpcode::PrintArgvUnsafe,
      primec::IrOpcode::LoadStringByte,
      primec::IrOpcode::FileOpenRead,
      primec::IrOpcode::FileOpenWrite,
      primec::IrOpcode::FileOpenAppend,
      primec::IrOpcode::FileClose,
      primec::IrOpcode::FileFlush,
      primec::IrOpcode::FileWriteI32,
      primec::IrOpcode::FileWriteI64,
      primec::IrOpcode::FileWriteU64,
      primec::IrOpcode::FileWriteString,
      primec::IrOpcode::FileWriteByte,
      primec::IrOpcode::FileWriteNewline,
      primec::IrOpcode::PushF32,
      primec::IrOpcode::PushF64,
      primec::IrOpcode::AddF32,
      primec::IrOpcode::SubF32,
      primec::IrOpcode::MulF32,
      primec::IrOpcode::DivF32,
      primec::IrOpcode::NegF32,
      primec::IrOpcode::AddF64,
      primec::IrOpcode::SubF64,
      primec::IrOpcode::MulF64,
      primec::IrOpcode::DivF64,
      primec::IrOpcode::NegF64,
      primec::IrOpcode::CmpEqF32,
      primec::IrOpcode::CmpNeF32,
      primec::IrOpcode::CmpLtF32,
      primec::IrOpcode::CmpLeF32,
      primec::IrOpcode::CmpGtF32,
      primec::IrOpcode::CmpGeF32,
      primec::IrOpcode::CmpEqF64,
      primec::IrOpcode::CmpNeF64,
      primec::IrOpcode::CmpLtF64,
      primec::IrOpcode::CmpLeF64,
      primec::IrOpcode::CmpGtF64,
      primec::IrOpcode::CmpGeF64,
      primec::IrOpcode::ConvertI32ToF32,
      primec::IrOpcode::ConvertI32ToF64,
      primec::IrOpcode::ConvertI64ToF32,
      primec::IrOpcode::ConvertI64ToF64,
      primec::IrOpcode::ConvertU64ToF32,
      primec::IrOpcode::ConvertU64ToF64,
      primec::IrOpcode::ConvertF32ToI32,
      primec::IrOpcode::ConvertF32ToI64,
      primec::IrOpcode::ConvertF32ToU64,
      primec::IrOpcode::ConvertF64ToI32,
      primec::IrOpcode::ConvertF64ToI64,
      primec::IrOpcode::ConvertF64ToU64,
      primec::IrOpcode::ConvertF32ToF64,
      primec::IrOpcode::ConvertF64ToF32,
      primec::IrOpcode::ReturnF32,
      primec::IrOpcode::ReturnF64,
      primec::IrOpcode::PrintStringDynamic,
      primec::IrOpcode::Call,
      primec::IrOpcode::CallVoid,
  };

  CHECK(expected.size() == static_cast<size_t>(static_cast<uint8_t>(primec::IrOpcode::CallVoid)));
  for (size_t i = 0; i < expected.size(); ++i) {
    const auto expectedValue = static_cast<uint8_t>(i + 1);
    CHECK(static_cast<uint8_t>(expected[i]) == expectedValue);
  }
}

TEST_CASE("ir call semantics matrix accepts recursive call opcodes with tail metadata") {
  primec::IrModule module;
  module.entryIndex = 0;

  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.metadata.instrumentationFlags = primec::InstrumentationTailExecution;
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 4});
  mainFn.instructions.push_back({primec::IrOpcode::Call, 1});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});

  primec::IrFunction factFn;
  factFn.name = "/fact";
  factFn.instructions.push_back({primec::IrOpcode::Dup, 0});
  factFn.instructions.push_back({primec::IrOpcode::PushI32, 0});
  factFn.instructions.push_back({primec::IrOpcode::CmpEqI32, 0});
  factFn.instructions.push_back({primec::IrOpcode::JumpIfZero, 7});
  factFn.instructions.push_back({primec::IrOpcode::Pop, 0});
  factFn.instructions.push_back({primec::IrOpcode::PushI32, 1});
  factFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  factFn.instructions.push_back({primec::IrOpcode::Dup, 0});
  factFn.instructions.push_back({primec::IrOpcode::PushI32, 1});
  factFn.instructions.push_back({primec::IrOpcode::SubI32, 0});
  factFn.instructions.push_back({primec::IrOpcode::Call, 1});
  factFn.instructions.push_back({primec::IrOpcode::MulI32, 0});
  factFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});

  module.functions.push_back(std::move(mainFn));
  module.functions.push_back(std::move(factFn));

  std::string error;
  CHECK(primec::validateIrModule(module, primec::IrValidationTarget::Vm, error));
  CHECK(error.empty());
  CHECK(primec::validateIrModule(module, primec::IrValidationTarget::Native, error));
  CHECK(error.empty());
}

TEST_CASE("ir call semantics matrix rejects non-direct call targets for vm and native") {
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::Call, 9});
  fn.instructions.push_back({primec::IrOpcode::ReturnVoid, 0});
  module.functions.push_back(fn);

  std::string error;
  CHECK_FALSE(primec::validateIrModule(module, primec::IrValidationTarget::Vm, error));
  CHECK(error.find("invalid call target") != std::string::npos);

  CHECK_FALSE(primec::validateIrModule(module, primec::IrValidationTarget::Native, error));
  CHECK(error.find("invalid call target") != std::string::npos);
}

TEST_CASE("ir validator rejects invalid jump targets") {
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::Jump, 3});
  fn.instructions.push_back({primec::IrOpcode::ReturnVoid, 0});
  module.functions.push_back(fn);

  std::string error;
  CHECK_FALSE(primec::validateIrModule(module, primec::IrValidationTarget::Any, error));
  CHECK(error.find("invalid jump target") != std::string::npos);
}

TEST_CASE("ir validator rejects invalid call targets") {
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::Call, 1});
  fn.instructions.push_back({primec::IrOpcode::ReturnVoid, 0});
  module.functions.push_back(fn);

  std::string error;
  CHECK_FALSE(primec::validateIrModule(module, primec::IrValidationTarget::Any, error));
  CHECK(error.find("invalid call target") != std::string::npos);
}

TEST_CASE("ir validator rejects invalid print flags") {
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::PushI32, 1});
  fn.instructions.push_back({primec::IrOpcode::PrintI32, 4});
  fn.instructions.push_back({primec::IrOpcode::ReturnVoid, 0});
  module.functions.push_back(fn);

  std::string error;
  CHECK_FALSE(primec::validateIrModule(module, primec::IrValidationTarget::Any, error));
  CHECK(error.find("invalid print flags") != std::string::npos);
}

TEST_CASE("ir validator rejects invalid string indices") {
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::FileOpenRead, 0});
  fn.instructions.push_back({primec::IrOpcode::ReturnVoid, 0});
  module.functions.push_back(fn);

  std::string error;
  CHECK_FALSE(primec::validateIrModule(module, primec::IrValidationTarget::Any, error));
  CHECK(error.find("invalid string index") != std::string::npos);
}

TEST_CASE("ir validator rejects local indices beyond 32-bit") {
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::LoadLocal,
                             static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()) + 1u});
  fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(fn);

  std::string error;
  CHECK_FALSE(primec::validateIrModule(module, primec::IrValidationTarget::Any, error));
  CHECK(error.find("local index exceeds 32-bit limit") != std::string::npos);
}

TEST_CASE("ir validator rejects unknown opcodes") {
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({static_cast<primec::IrOpcode>(0), 0});
  module.functions.push_back(fn);

  std::string error;
  CHECK_FALSE(primec::validateIrModule(module, primec::IrValidationTarget::Any, error));
  CHECK(error.find("unsupported opcode") != std::string::npos);
}

TEST_CASE("ir validator rejects unknown metadata bits") {
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.metadata.effectMask = (1ull << 63);
  fn.instructions.push_back({primec::IrOpcode::ReturnVoid, 0});
  module.functions.push_back(fn);

  std::string error;
  CHECK_FALSE(primec::validateIrModule(module, primec::IrValidationTarget::Any, error));
  CHECK(error.find("unsupported effect mask bits") != std::string::npos);
}

TEST_SUITE_END();
