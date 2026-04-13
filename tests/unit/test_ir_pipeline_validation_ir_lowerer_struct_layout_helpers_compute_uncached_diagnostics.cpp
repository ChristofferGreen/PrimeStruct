#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

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
  std::vector<const primec::Expr *> packedArgs;
  size_t packedParamIndex = 0;
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
  const primec::ir_lowerer::LocalMap callerLocals;

  REQUIRE(primec::ir_lowerer::buildInlineCallOrderedArguments(
      callExpr,
      helperDef,
      structNames,
      callerLocals,
      paramsOut,
      orderedArgs,
      packedArgs,
      packedParamIndex,
      error));
  CHECK(error.empty());
  REQUIRE(paramsOut.size() == 2);
  CHECK(paramsOut[0].name == "this");
  CHECK(paramsOut[1].name == "value");
  CHECK(packedArgs.empty());
  CHECK(packedParamIndex == paramsOut.size());
  REQUIRE(orderedArgs.size() == 2);
  REQUIRE(orderedArgs[0] != nullptr);
  REQUIRE(orderedArgs[1] != nullptr);
  CHECK(orderedArgs[0]->name == "obj");
  CHECK(orderedArgs[1]->literalValue == 42);

  primec::Expr badCall = callExpr;
  badCall.args = {valueArg};
  badCall.argNames = {std::nullopt};
  CHECK_FALSE(primec::ir_lowerer::buildInlineCallOrderedArguments(
      badCall,
      helperDef,
      structNames,
      callerLocals,
      paramsOut,
      orderedArgs,
      packedArgs,
      packedParamIndex,
      error));
  CHECK(error.find("argument count mismatch") != std::string::npos);
}

TEST_CASE("ir lowerer call helpers collect packed variadic inline call arguments") {
  std::unordered_set<std::string> structNames;
  std::vector<primec::Expr> paramsOut;
  std::vector<const primec::Expr *> orderedArgs;
  std::vector<const primec::Expr *> packedArgs;
  size_t packedParamIndex = 0;
  std::string error;

  primec::Definition helperDef;
  helperDef.fullPath = "/collect";

  primec::Expr headParam;
  headParam.kind = primec::Expr::Kind::Name;
  headParam.isBinding = true;
  headParam.name = "head";
  helperDef.parameters.push_back(headParam);

  primec::Expr valuesParam;
  valuesParam.kind = primec::Expr::Kind::Name;
  valuesParam.isBinding = true;
  valuesParam.name = "values";
  primec::Transform argsTransform;
  argsTransform.name = "args";
  argsTransform.templateArgs.push_back("i32");
  valuesParam.transforms.push_back(std::move(argsTransform));
  helperDef.parameters.push_back(valuesParam);

  primec::Expr headArg;
  headArg.kind = primec::Expr::Kind::Literal;
  headArg.literalValue = 10;
  primec::Expr firstPackedArg;
  firstPackedArg.kind = primec::Expr::Kind::Literal;
  firstPackedArg.literalValue = 20;
  primec::Expr spreadPackedArg;
  spreadPackedArg.kind = primec::Expr::Kind::Name;
  spreadPackedArg.name = "rest";
  spreadPackedArg.isSpread = true;

  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "collect";
  callExpr.args = {headArg, firstPackedArg, spreadPackedArg};
  callExpr.argNames = {std::nullopt, std::nullopt, std::nullopt};
  const primec::ir_lowerer::LocalMap callerLocals;

  REQUIRE(primec::ir_lowerer::buildInlineCallOrderedArguments(
      callExpr, helperDef, structNames, callerLocals, paramsOut, orderedArgs, packedArgs, packedParamIndex, error));
  CHECK(error.empty());
  REQUIRE(orderedArgs.size() == 2u);
  REQUIRE(orderedArgs[0] != nullptr);
  CHECK(orderedArgs[0]->literalValue == 10u);
  CHECK(orderedArgs[1] == nullptr);
  CHECK(packedParamIndex == 1u);
  REQUIRE(packedArgs.size() == 2u);
  CHECK(packedArgs[0] != nullptr);
  CHECK(packedArgs[0]->literalValue == 20u);
  CHECK(packedArgs[1] != nullptr);
  CHECK(packedArgs[1]->isSpread);
  CHECK(packedArgs[1]->name == "rest");
}

TEST_CASE("ir lowerer call helpers build inline arguments for inferred experimental map receiver methods") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<auto> effects(heap_alloc)]
buildValues([bool] useCanonical) {
  if(useCanonical,
     then() { /std/collections/map/map("left"raw_utf8, 4i32, "right"raw_utf8, 7i32) },
     else() { /std/collections/mapPair("left"raw_utf8, 4i32, "other"raw_utf8, 2i32) })
}

[return<int> effects(heap_alloc)]
main() {
  return(buildValues(true).count())
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  std::unordered_map<std::string, const primec::Definition *> defMap;
  std::unordered_set<std::string> structNames;
  primec::ir_lowerer::buildDefinitionMapAndStructNames(program.definitions, defMap, structNames);

  const primec::Definition *mainDef = nullptr;
  for (const auto &def : program.definitions) {
    if (def.fullPath == "/main") {
      mainDef = &def;
      break;
    }
  }
  REQUIRE(mainDef != nullptr);
  REQUIRE(mainDef->statements.size() == 1u);
  REQUIRE(mainDef->statements.front().args.size() == 1u);
  const primec::Expr &countCall = mainDef->statements.front().args.front();
  REQUIRE(countCall.kind == primec::Expr::Kind::Call);
  CHECK(countCall.isMethodCall);
  CHECK(countCall.name == "count");

  auto resolveExprPath = [](const primec::Expr &expr) {
    if (!expr.name.empty() && expr.name.front() == '/') {
      return expr.name;
    }
    if (!expr.namespacePrefix.empty()) {
      return expr.namespacePrefix + "/" + expr.name;
    }
    return expr.name.empty() ? std::string() : std::string("/") + expr.name;
  };

  const primec::Definition *resolved = primec::ir_lowerer::resolveMethodCallDefinitionFromExpr(
      countCall,
      {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      {},
      structNames,
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      resolveExprPath,
      defMap,
      error);
  REQUIRE(resolved != nullptr);
  CHECK(resolved->fullPath == "/std/collections/experimental_map/Map__td48f7c0fb764e3c0/count");
  CHECK(error.empty());

  std::vector<primec::Expr> paramsOut;
  std::vector<const primec::Expr *> orderedArgs;
  std::vector<const primec::Expr *> packedArgs;
  size_t packedParamIndex = 0;
  REQUIRE(primec::ir_lowerer::buildInlineCallOrderedArguments(
      countCall, *resolved, structNames, {}, paramsOut, orderedArgs, packedArgs, packedParamIndex, error));
  CHECK(error.empty());
  REQUIRE(paramsOut.size() == 1u);
  CHECK(paramsOut.front().name == "this");
  REQUIRE(orderedArgs.size() == 1u);
  REQUIRE(orderedArgs.front() != nullptr);
  CHECK(orderedArgs.front()->kind == primec::Expr::Kind::Call);
  CHECK(orderedArgs.front()->name == "buildValues");
  CHECK(packedArgs.empty());
  CHECK(packedParamIndex == paramsOut.size());
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


TEST_SUITE_END();
