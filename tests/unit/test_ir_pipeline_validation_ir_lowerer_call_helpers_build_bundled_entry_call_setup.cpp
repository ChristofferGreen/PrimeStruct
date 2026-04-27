#include "test_ir_pipeline_validation_helpers.h"
#include "primec/FrontendSyntax.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

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

TEST_CASE("frontend syntax helpers build setup import aliases") {
  std::string prefix;
  CHECK(primec::isSyntaxWildcardImportPath("/pkg/*", prefix));
  CHECK(prefix == "/pkg");
  CHECK(primec::isSyntaxWildcardImportPath("/single", prefix));
  CHECK(prefix == "/single");
  CHECK_FALSE(primec::isSyntaxWildcardImportPath("/pkg/nested", prefix));
  CHECK(primec::normalizeSyntaxImportAliasTargetPath("map/at") == "/map/at");
  CHECK(primec::normalizeSyntaxImportAliasTargetPath("std/collections/map/count") ==
        "/std/collections/map/count");

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
  const auto aliases = primec::buildSyntaxImportAliases(imports, definitions, defMap);
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
      {"MapCountAlias", "std/collections/map/count"},
      {"MapAtAlias", "map/at"},
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

  primec::Expr slashlessCanonicalMapAliasExpr;
  slashlessCanonicalMapAliasExpr.kind = primec::Expr::Kind::Name;
  slashlessCanonicalMapAliasExpr.name = "MapCountAlias";
  CHECK(primec::ir_lowerer::resolveStructLayoutExprPathFromScope(
            slashlessCanonicalMapAliasExpr, defMap, importAliases) == "/std/collections/map/count");

  primec::Expr slashlessMapAliasExpr;
  slashlessMapAliasExpr.kind = primec::Expr::Kind::Name;
  slashlessMapAliasExpr.name = "MapAtAlias";
  slashlessMapAliasExpr.namespacePrefix = "/pkg";
  CHECK(primec::ir_lowerer::resolveStructLayoutExprPathFromScope(
            slashlessMapAliasExpr, defMap, importAliases) == "/map/at");
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

  primec::Expr canonicalMapExpr;
  primec::Transform canonicalMapTransform;
  canonicalMapTransform.name = "/std/collections/map";
  canonicalMapTransform.templateArgs = {"i32", "bool"};
  canonicalMapExpr.transforms = {publicTransform, canonicalMapTransform};
  CHECK(primec::ir_lowerer::extractExplicitLayoutFieldBinding(canonicalMapExpr, binding));
  CHECK(binding.typeName == "map");
  CHECK(binding.typeTemplateArg == "i32, bool");

  primec::Expr experimentalSoaExpr;
  primec::Transform experimentalSoaTransform;
  experimentalSoaTransform.name = "SoaVector";
  experimentalSoaTransform.templateArgs = {"Particle"};
  experimentalSoaExpr.transforms = {publicTransform, experimentalSoaTransform};
  CHECK(primec::ir_lowerer::extractExplicitLayoutFieldBinding(experimentalSoaExpr, binding));
  CHECK(binding.typeName == "SoaVector");
  CHECK(binding.typeTemplateArg == "Particle");

  primec::Expr specializedSoaExpr;
  primec::Transform specializedSoaTransform;
  specializedSoaTransform.name =
      "/std/collections/experimental_soa_vector/SoaVector__Particle";
  specializedSoaExpr.transforms = {publicTransform, specializedSoaTransform};
  CHECK(primec::ir_lowerer::extractExplicitLayoutFieldBinding(specializedSoaExpr, binding));
  CHECK(binding.typeName ==
        "/std/collections/experimental_soa_vector/SoaVector__Particle");
  CHECK(binding.typeTemplateArg.empty());

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


TEST_SUITE_END();
