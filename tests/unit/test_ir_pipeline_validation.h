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
