#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "third_party/doctest.h"

#include "primec/testing/IrLowererCountAccessContracts.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

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

  primec::ir_lowerer::LocalInfo soaInfo;
  soaInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  soaInfo.isSoaVector = true;
  locals.emplace("soaValues", soaInfo);
  primec::Expr soaName;
  soaName.kind = primec::Expr::Kind::Name;
  soaName.name = "soaValues";
  countEntry.args = {soaName};
  countEntry.name = "count";
  CHECK(setup.classifiers.isArrayCountCall(countEntry, locals));
  countEntry.name = "/vector/count";
  CHECK_FALSE(setup.classifiers.isArrayCountCall(countEntry, locals));
  countEntry.name = "/std/collections/vector/count";
  CHECK(setup.classifiers.isArrayCountCall(countEntry, locals));
  countEntry.name = "/array/count";
  CHECK_FALSE(setup.classifiers.isArrayCountCall(countEntry, locals));

  primec::Expr extraParam = entryParam;
  extraParam.name = "extra";
  entryDef.parameters = {entryParam, extraParam};
  CHECK_FALSE(primec::ir_lowerer::buildEntryCountAccessSetup(entryDef, setup, error));
  CHECK(error == "native backend only supports a single array<string> entry parameter");
}

TEST_CASE("ir lowerer count access helpers prefer semantic product entry args facts") {
  primec::Definition entryDef;
  entryDef.fullPath = "/main";

  primec::Expr staleParam;
  staleParam.name = "stale";
  staleParam.semanticNodeId = 6201;
  primec::Transform staleTransform;
  staleTransform.name = "array";
  staleTransform.templateArgs = {"i64"};
  staleParam.transforms.push_back(staleTransform);
  entryDef.parameters = {staleParam};

  primec::SemanticProgram semanticProgram;
  semanticProgram.bindingFacts.push_back(primec::SemanticProgramBindingFact{
      .scopePath = "/main",
      .siteKind = "parameter",
      .name = "argv",
      .bindingTypeText = "array<string>",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 2,
      .sourceColumn = 3,
      .semanticNodeId = 6201,
      .resolvedPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/main/argv"),
  });
  semanticProgram.publishedRoutingLookups.bindingFactIndicesByExpr.insert_or_assign(6201, 0);

  primec::ir_lowerer::EntryCountAccessSetup setup;
  std::string error;
  REQUIRE(primec::ir_lowerer::buildEntryCountAccessSetup(entryDef, &semanticProgram, setup, error));
  CHECK(setup.hasEntryArgs);
  CHECK(setup.entryArgsName == "argv");
  CHECK(error.empty());
}

TEST_CASE("ir lowerer count access helpers require published entry args binding maps") {
  primec::Definition entryDef;
  entryDef.fullPath = "/main";

  primec::Expr entryParam;
  entryParam.name = "stale";
  entryParam.semanticNodeId = 6301;
  primec::Transform staleTransform;
  staleTransform.name = "array";
  staleTransform.templateArgs = {"i64"};
  entryParam.transforms.push_back(staleTransform);
  entryDef.parameters = {entryParam};

  primec::SemanticProgram semanticProgram;
  semanticProgram.bindingFacts.push_back(primec::SemanticProgramBindingFact{
      .scopePath = "/main",
      .siteKind = "parameter",
      .name = "argv",
      .bindingTypeText = "array<string>",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 2,
      .sourceColumn = 3,
      .semanticNodeId = 6301,
      .resolvedPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/main/argv"),
  });

  primec::ir_lowerer::EntryCountAccessSetup setup;
  std::string error;
  CHECK_FALSE(primec::ir_lowerer::buildEntryCountAccessSetup(entryDef, &semanticProgram, setup, error));
  CHECK(error == "missing semantic-product entry parameter fact: /main");

  semanticProgram.publishedRoutingLookups.bindingFactIndicesByExpr.insert_or_assign(6301, 0);
  error.clear();
  REQUIRE(primec::ir_lowerer::buildEntryCountAccessSetup(entryDef, &semanticProgram, setup, error));
  CHECK(setup.hasEntryArgs);
  CHECK(setup.entryArgsName == "argv");
  CHECK(error.empty());
}

TEST_CASE("ir lowerer count access classifiers prefer semantic direct-name facts") {
  primec::Definition entryDef;
  entryDef.fullPath = "/main";

  primec::SemanticProgram semanticProgram;
  semanticProgram.bindingFacts.push_back(primec::SemanticProgramBindingFact{
      .scopePath = "/main",
      .siteKind = "local",
      .name = "values",
      .bindingTypeText = "string",
      .semanticNodeId = 7001,
      .bindingTypeTextId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "array<bool>"),
  });
  semanticProgram.publishedRoutingLookups.bindingFactIndicesByExpr.insert_or_assign(7001, 0);
  semanticProgram.localAutoFacts.push_back(primec::SemanticProgramLocalAutoFact{
      .scopePath = "/main",
      .bindingName = "message",
      .bindingTypeText = "i32",
      .semanticNodeId = 7002,
      .bindingTypeTextId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "string"),
  });
  semanticProgram.publishedRoutingLookups.localAutoFactIndicesByExpr.insert_or_assign(7002, 0);
  semanticProgram.queryFacts.push_back(primec::SemanticProgramQueryFact{
      .scopePath = "/main",
      .callName = "makeScalar",
      .queryTypeText = "array<i32>",
      .bindingTypeText = "array<i32>",
      .semanticNodeId = 7003,
      .queryTypeTextId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "i32"),
  });
  semanticProgram.publishedRoutingLookups.queryFactIndicesByExpr.insert_or_assign(7003, 0);
  semanticProgram.collectionSpecializations.push_back(primec::SemanticProgramCollectionSpecialization{
      .scopePath = "/main",
      .siteKind = "local",
      .name = "semanticArray",
      .collectionFamily = "array",
      .bindingTypeText = "array<i32>",
      .elementTypeText = "i32",
      .semanticNodeId = 7004,
      .collectionFamilyId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "array"),
      .bindingTypeTextId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "array<i32>"),
      .elementTypeTextId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "i32"),
  });
  semanticProgram.publishedRoutingLookups.collectionSpecializationIndicesByExpr
      .insert_or_assign(7004, 0);
  const primec::SymbolId vectorCountPathId =
      primec::semanticProgramInternCallTargetString(
          semanticProgram, "/std/collections/vector/count");
  semanticProgram.bridgePathChoices.push_back(primec::SemanticProgramBridgePathChoice{
      .scopePath = "/main",
      .collectionFamily = "vector",
      .semanticNodeId = 7005,
      .scopePathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
      .collectionFamilyId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "vector"),
      .helperNameId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "count"),
      .chosenPathId = vectorCountPathId,
      .stdlibSurfaceId = primec::StdlibSurfaceId::CollectionsManifestSurface0,
  });
  semanticProgram.publishedRoutingLookups.bridgePathChoiceIdsByExpr
      .insert_or_assign(7005, vectorCountPathId);
  semanticProgram.publishedRoutingLookups.bridgePathChoiceStdlibSurfaceIdsByExpr
      .insert_or_assign(7005,
                         primec::StdlibSurfaceId::CollectionsManifestSurface0);

  primec::ir_lowerer::EntryCountAccessSetup setup;
  std::string error;
  REQUIRE(primec::ir_lowerer::buildEntryCountAccessSetup(entryDef, &semanticProgram, setup, error));
  CHECK(error.empty());

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo staleStringInfo;
  staleStringInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  staleStringInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::String;
  staleStringInfo.stringSource = primec::ir_lowerer::LocalInfo::StringSource::TableIndex;
  locals.emplace("values", staleStringInfo);
  primec::ir_lowerer::LocalInfo staleMapInfo;
  staleMapInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  staleMapInfo.keyValueKeyKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  staleMapInfo.keyValueValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  locals.emplace("message", staleMapInfo);
  primec::ir_lowerer::LocalInfo staleArrayInfo;
  staleArrayInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  staleArrayInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  locals.emplace("scalar", staleArrayInfo);
  primec::ir_lowerer::LocalInfo staleScalarInfo;
  staleScalarInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  staleScalarInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  locals.emplace("semanticArray", staleScalarInfo);

  auto makeSemanticName = [](const char *name, uint64_t semanticNodeId) {
    primec::Expr expr;
    expr.kind = primec::Expr::Kind::Name;
    expr.name = name;
    expr.semanticNodeId = semanticNodeId;
    return expr;
  };
  auto makeCountCall = [](const primec::Expr &target) {
    primec::Expr expr;
    expr.kind = primec::Expr::Kind::Call;
    expr.name = "count";
    expr.args = {target};
    return expr;
  };

  primec::Expr valuesCount = makeCountCall(makeSemanticName("values", 7001));
  CHECK(setup.classifiers.isArrayCountCall(valuesCount, locals));
  CHECK_FALSE(setup.classifiers.isStringCountCall(valuesCount, locals));

  primec::Expr messageCount = makeCountCall(makeSemanticName("message", 7002));
  CHECK_FALSE(setup.classifiers.isArrayCountCall(messageCount, locals));
  CHECK(setup.classifiers.isStringCountCall(messageCount, locals));

  primec::Expr scalarCount = makeCountCall(makeSemanticName("scalar", 7003));
  CHECK_FALSE(setup.classifiers.isArrayCountCall(scalarCount, locals));
  CHECK_FALSE(setup.classifiers.isStringCountCall(scalarCount, locals));

  primec::Expr semanticArrayCount =
      makeCountCall(makeSemanticName("semanticArray", 7004));
  CHECK(setup.classifiers.isArrayCountCall(semanticArrayCount, locals));
  CHECK_FALSE(setup.classifiers.isStringCountCall(semanticArrayCount, locals));

  primec::Expr semanticVectorBridgeCount =
      makeCountCall(makeSemanticName("semanticArray", 7004));
  semanticVectorBridgeCount.name = "/std/collections/vector/count";
  semanticVectorBridgeCount.semanticNodeId = 7005;
  CHECK(setup.classifiers.isArrayCountCall(semanticVectorBridgeCount, locals));
  CHECK_FALSE(setup.classifiers.isStringCountCall(semanticVectorBridgeCount, locals));

  primec::Expr missingFactCount = makeCountCall(makeSemanticName("values", 7999));
  CHECK_FALSE(setup.classifiers.isArrayCountCall(missingFactCount, locals));
  CHECK_FALSE(setup.classifiers.isStringCountCall(missingFactCount, locals));
}

TEST_CASE("ir lowerer count access classifiers prefer semantic dereferenced target facts") {
  primec::Definition entryDef;
  entryDef.fullPath = "/main";

  primec::SemanticProgram semanticProgram;
  semanticProgram.bindingFacts.push_back(primec::SemanticProgramBindingFact{
      .scopePath = "/main",
      .siteKind = "local",
      .name = "refVec",
      .bindingTypeText = "i32",
      .semanticNodeId = 7201,
      .bindingTypeTextId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "Reference<vector<i32>>"),
  });
  semanticProgram.publishedRoutingLookups.bindingFactIndicesByExpr.insert_or_assign(7201, 0);
  semanticProgram.bindingFacts.push_back(primec::SemanticProgramBindingFact{
      .scopePath = "/main",
      .siteKind = "local",
      .name = "scalarRef",
      .bindingTypeText = "Reference<vector<i32>>",
      .semanticNodeId = 7202,
      .bindingTypeTextId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "Reference<i32>"),
  });
  semanticProgram.publishedRoutingLookups.bindingFactIndicesByExpr.insert_or_assign(7202, 1);
  semanticProgram.queryFacts.push_back(primec::SemanticProgramQueryFact{
      .scopePath = "/main",
      .callName = "at",
      .queryTypeText = "i32",
      .semanticNodeId = 7203,
      .queryTypeTextId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "Reference<map<i32, string>>"),
  });
  semanticProgram.publishedRoutingLookups.queryFactIndicesByExpr.insert_or_assign(7203, 0);
  semanticProgram.queryFacts.push_back(primec::SemanticProgramQueryFact{
      .scopePath = "/main",
      .callName = "at",
      .queryTypeText = "Reference<vector<i32>>",
      .semanticNodeId = 7204,
      .queryTypeTextId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "i32"),
  });
  semanticProgram.publishedRoutingLookups.queryFactIndicesByExpr.insert_or_assign(7204, 1);

  primec::ir_lowerer::EntryCountAccessSetup setup;
  std::string error;
  REQUIRE(primec::ir_lowerer::buildEntryCountAccessSetup(entryDef, &semanticProgram, setup, error));
  CHECK(error.empty());

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo staleScalar;
  staleScalar.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  staleScalar.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  locals.emplace("refVec", staleScalar);
  primec::ir_lowerer::LocalInfo staleVectorRef;
  staleVectorRef.kind = primec::ir_lowerer::LocalInfo::Kind::Reference;
  staleVectorRef.referenceToVector = true;
  locals.emplace("scalarRef", staleVectorRef);
  locals.emplace("missingRef", staleVectorRef);
  locals.emplace("syntaxRef", staleVectorRef);
  primec::ir_lowerer::LocalInfo stalePrimitiveArgs;
  stalePrimitiveArgs.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  stalePrimitiveArgs.isArgsPack = true;
  stalePrimitiveArgs.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Value;
  stalePrimitiveArgs.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  locals.emplace("mapArgs", stalePrimitiveArgs);
  primec::ir_lowerer::LocalInfo staleCollectionArgs;
  staleCollectionArgs.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  staleCollectionArgs.isArgsPack = true;
  staleCollectionArgs.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Reference;
  staleCollectionArgs.keyValueKeyKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  staleCollectionArgs.keyValueValueKind = primec::ir_lowerer::LocalInfo::ValueKind::String;
  locals.emplace("scalarArgs", staleCollectionArgs);
  locals.emplace("syntaxArgs", staleCollectionArgs);

  auto makeSemanticName = [](const char *name, uint64_t semanticNodeId) {
    primec::Expr expr;
    expr.kind = primec::Expr::Kind::Name;
    expr.name = name;
    expr.semanticNodeId = semanticNodeId;
    return expr;
  };
  auto makeAt = [&](const char *name, uint64_t semanticNodeId) {
    primec::Expr zeroIndex;
    zeroIndex.kind = primec::Expr::Kind::Literal;
    zeroIndex.literalValue = 0;
    primec::Expr expr;
    expr.kind = primec::Expr::Kind::Call;
    expr.name = "at";
    expr.args = {makeSemanticName(name, 0), zeroIndex};
    expr.semanticNodeId = semanticNodeId;
    return expr;
  };
  auto makeDeref = [](const primec::Expr &target) {
    primec::Expr expr;
    expr.kind = primec::Expr::Kind::Call;
    expr.name = "dereference";
    expr.args = {target};
    return expr;
  };
  auto makeCountCall = [](const primec::Expr &target) {
    primec::Expr expr;
    expr.kind = primec::Expr::Kind::Call;
    expr.name = "count";
    expr.args = {target};
    return expr;
  };

  CHECK(setup.classifiers.isArrayCountCall(
      makeCountCall(makeDeref(makeSemanticName("refVec", 7201))), locals));
  CHECK_FALSE(setup.classifiers.isArrayCountCall(
      makeCountCall(makeDeref(makeSemanticName("scalarRef", 7202))), locals));
  CHECK_FALSE(setup.classifiers.isArrayCountCall(
      makeCountCall(makeDeref(makeSemanticName("missingRef", 7299))), locals));
  CHECK(setup.classifiers.isArrayCountCall(
      makeCountCall(makeDeref(makeSemanticName("syntaxRef", 0))), locals));

  CHECK(setup.classifiers.isArrayCountCall(
      makeCountCall(makeDeref(makeAt("mapArgs", 7203))), locals));
  CHECK_FALSE(setup.classifiers.isArrayCountCall(
      makeCountCall(makeDeref(makeAt("scalarArgs", 7204))), locals));
  CHECK(setup.classifiers.isArrayCountCall(
      makeCountCall(makeDeref(makeAt("syntaxArgs", 0))), locals));
}

TEST_CASE("ir lowerer count access classifiers prefer semantic indexed target facts") {
  primec::Definition entryDef;
  entryDef.fullPath = "/main";

  primec::SemanticProgram semanticProgram;
  semanticProgram.queryFacts.push_back(primec::SemanticProgramQueryFact{
      .scopePath = "/main",
      .callName = "at",
      .queryTypeText = "i32",
      .semanticNodeId = 7301,
      .queryTypeTextId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "array<i32>"),
  });
  semanticProgram.publishedRoutingLookups.queryFactIndicesByExpr.insert_or_assign(7301, 0);
  semanticProgram.queryFacts.push_back(primec::SemanticProgramQueryFact{
      .scopePath = "/main",
      .callName = "at",
      .queryTypeText = "array<i32>",
      .semanticNodeId = 7302,
      .queryTypeTextId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "i32"),
  });
  semanticProgram.publishedRoutingLookups.queryFactIndicesByExpr.insert_or_assign(7302, 1);
  semanticProgram.queryFacts.push_back(primec::SemanticProgramQueryFact{
      .scopePath = "/main",
      .callName = "at",
      .queryTypeText = "i32",
      .semanticNodeId = 7303,
      .queryTypeTextId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "map<i32, string>"),
  });
  semanticProgram.publishedRoutingLookups.queryFactIndicesByExpr.insert_or_assign(7303, 2);

  primec::ir_lowerer::EntryCountAccessSetup setup;
  std::string error;
  REQUIRE(primec::ir_lowerer::buildEntryCountAccessSetup(entryDef, &semanticProgram, setup, error));
  CHECK(error.empty());

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo staleScalarArgs;
  staleScalarArgs.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  staleScalarArgs.isArgsPack = true;
  staleScalarArgs.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Value;
  staleScalarArgs.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  locals.emplace("arrayArgs", staleScalarArgs);
  locals.emplace("mapArgs", staleScalarArgs);
  primec::ir_lowerer::LocalInfo staleCollectionArgs;
  staleCollectionArgs.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  staleCollectionArgs.isArgsPack = true;
  staleCollectionArgs.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Vector;
  locals.emplace("scalarArgs", staleCollectionArgs);
  locals.emplace("missingArgs", staleCollectionArgs);
  locals.emplace("syntaxArgs", staleCollectionArgs);

  auto makeName = [](const char *name) {
    primec::Expr expr;
    expr.kind = primec::Expr::Kind::Name;
    expr.name = name;
    return expr;
  };
  auto makeAt = [&](const char *name, uint64_t semanticNodeId) {
    primec::Expr zeroIndex;
    zeroIndex.kind = primec::Expr::Kind::Literal;
    zeroIndex.literalValue = 0;
    primec::Expr expr;
    expr.kind = primec::Expr::Kind::Call;
    expr.name = "at";
    expr.args = {makeName(name), zeroIndex};
    expr.semanticNodeId = semanticNodeId;
    return expr;
  };
  auto makeCountCall = [](const primec::Expr &target) {
    primec::Expr expr;
    expr.kind = primec::Expr::Kind::Call;
    expr.name = "count";
    expr.args = {target};
    return expr;
  };

  CHECK(setup.classifiers.isArrayCountCall(
      makeCountCall(makeAt("arrayArgs", 7301)), locals));
  CHECK_FALSE(setup.classifiers.isArrayCountCall(
      makeCountCall(makeAt("scalarArgs", 7302)), locals));
  CHECK(setup.classifiers.isArrayCountCall(
      makeCountCall(makeAt("mapArgs", 7303)), locals));
  CHECK_FALSE(setup.classifiers.isArrayCountCall(
      makeCountCall(makeAt("missingArgs", 7399)), locals));
  CHECK_FALSE(setup.classifiers.isArrayCountCall(
      makeCountCall(makeAt("syntaxArgs", 0)), locals));
}

TEST_CASE("ir lowerer capacity classifiers prefer semantic target facts") {
  primec::Definition entryDef;
  entryDef.fullPath = "/main";

  primec::SemanticProgram semanticProgram;
  semanticProgram.bindingFacts.push_back(primec::SemanticProgramBindingFact{
      .scopePath = "/main",
      .siteKind = "local",
      .name = "refVec",
      .bindingTypeText = "i32",
      .semanticNodeId = 7101,
      .bindingTypeTextId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "Reference<vector<i32>>"),
  });
  semanticProgram.publishedRoutingLookups.bindingFactIndicesByExpr.insert_or_assign(7101, 0);
  semanticProgram.bindingFacts.push_back(primec::SemanticProgramBindingFact{
      .scopePath = "/main",
      .siteKind = "local",
      .name = "scalarRef",
      .bindingTypeText = "Reference<vector<i32>>",
      .semanticNodeId = 7102,
      .bindingTypeTextId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "Reference<i32>"),
  });
  semanticProgram.publishedRoutingLookups.bindingFactIndicesByExpr.insert_or_assign(7102, 1);
  semanticProgram.queryFacts.push_back(primec::SemanticProgramQueryFact{
      .scopePath = "/main",
      .callName = "at",
      .queryTypeText = "i32",
      .semanticNodeId = 7103,
      .queryTypeTextId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "vector<i32>"),
  });
  semanticProgram.publishedRoutingLookups.queryFactIndicesByExpr.insert_or_assign(7103, 0);
  semanticProgram.queryFacts.push_back(primec::SemanticProgramQueryFact{
      .scopePath = "/main",
      .callName = "at",
      .queryTypeText = "vector<i32>",
      .semanticNodeId = 7104,
      .queryTypeTextId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "map<i32, i32>"),
  });
  semanticProgram.publishedRoutingLookups.queryFactIndicesByExpr.insert_or_assign(7104, 1);

  primec::ir_lowerer::EntryCountAccessSetup setup;
  std::string error;
  REQUIRE(primec::ir_lowerer::buildEntryCountAccessSetup(entryDef, &semanticProgram, setup, error));
  CHECK(error.empty());

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo staleScalar;
  staleScalar.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  staleScalar.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  locals.emplace("refVec", staleScalar);
  primec::ir_lowerer::LocalInfo staleVectorRef;
  staleVectorRef.kind = primec::ir_lowerer::LocalInfo::Kind::Reference;
  staleVectorRef.referenceToVector = true;
  locals.emplace("scalarRef", staleVectorRef);
  locals.emplace("missingRef", staleVectorRef);
  primec::ir_lowerer::LocalInfo stalePrimitiveArgs;
  stalePrimitiveArgs.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  stalePrimitiveArgs.isArgsPack = true;
  stalePrimitiveArgs.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Value;
  locals.emplace("vectorArgs", stalePrimitiveArgs);
  primec::ir_lowerer::LocalInfo staleVectorArgs;
  staleVectorArgs.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  staleVectorArgs.isArgsPack = true;
  staleVectorArgs.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Vector;
  locals.emplace("mapArgs", staleVectorArgs);

  auto makeSemanticName = [](const char *name, uint64_t semanticNodeId) {
    primec::Expr expr;
    expr.kind = primec::Expr::Kind::Name;
    expr.name = name;
    expr.semanticNodeId = semanticNodeId;
    return expr;
  };
  auto makeDeref = [](const primec::Expr &target) {
    primec::Expr expr;
    expr.kind = primec::Expr::Kind::Call;
    expr.name = "dereference";
    expr.args = {target};
    return expr;
  };
  auto makeAt = [](const primec::Expr &target, uint64_t semanticNodeId) {
    primec::Expr zeroIndex;
    zeroIndex.kind = primec::Expr::Kind::Literal;
    zeroIndex.literalValue = 0;
    primec::Expr expr;
    expr.kind = primec::Expr::Kind::Call;
    expr.name = "at";
    expr.args = {target, zeroIndex};
    expr.semanticNodeId = semanticNodeId;
    return expr;
  };
  auto makeCapacityCall = [](const primec::Expr &target) {
    primec::Expr expr;
    expr.kind = primec::Expr::Kind::Call;
    expr.name = "capacity";
    expr.args = {target};
    return expr;
  };

  CHECK(setup.classifiers.isVectorCapacityCall(
      makeCapacityCall(makeDeref(makeSemanticName("refVec", 7101))), locals));
  CHECK_FALSE(setup.classifiers.isVectorCapacityCall(
      makeCapacityCall(makeDeref(makeSemanticName("scalarRef", 7102))), locals));
  CHECK_FALSE(setup.classifiers.isVectorCapacityCall(
      makeCapacityCall(makeDeref(makeSemanticName("missingRef", 7199))), locals));

  CHECK(setup.classifiers.isVectorCapacityCall(
      makeCapacityCall(makeAt(makeSemanticName("vectorArgs", 0), 7103)), locals));
  CHECK_FALSE(setup.classifiers.isVectorCapacityCall(
      makeCapacityCall(makeAt(makeSemanticName("mapArgs", 0), 7104)), locals));
}

TEST_CASE("ir lowerer count access helpers reject removed /array/capacity alias") {
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
  CHECK_FALSE(primec::ir_lowerer::isVectorCapacityCall(capacityCall, locals));

  capacityCall.name = "/std/collections/vector/capacity";
  CHECK_FALSE(primec::ir_lowerer::isVectorCapacityCall(capacityCall, locals));

  capacityCall.name = "/array/capacity";
  CHECK_FALSE(primec::ir_lowerer::isVectorCapacityCall(capacityCall, locals));
}

TEST_CASE("ir lowerer count access helpers classify capacity and string count") {
  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo vecInfo;
  vecInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Vector;
  locals.emplace("values", vecInfo);

  primec::ir_lowerer::LocalInfo refVecInfo;
  refVecInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Reference;
  refVecInfo.referenceToVector = true;
  locals.emplace("refVec", refVecInfo);

  primec::ir_lowerer::LocalInfo ptrVecInfo;
  ptrVecInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Pointer;
  ptrVecInfo.pointerToVector = true;
  locals.emplace("ptrVec", ptrVecInfo);

  primec::ir_lowerer::LocalInfo vectorArgsInfo;
  vectorArgsInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  vectorArgsInfo.isArgsPack = true;
  vectorArgsInfo.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Vector;
  locals.emplace("vectorArgs", vectorArgsInfo);

  primec::ir_lowerer::LocalInfo primitiveArgsInfo;
  primitiveArgsInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  primitiveArgsInfo.isArgsPack = true;
  primitiveArgsInfo.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Value;
  primitiveArgsInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  primitiveArgsInfo.argsPackElementCount = 3;
  locals.emplace("primitiveArgs", primitiveArgsInfo);

  primec::ir_lowerer::LocalInfo refVectorArgsInfo;
  refVectorArgsInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  refVectorArgsInfo.isArgsPack = true;
  refVectorArgsInfo.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Reference;
  refVectorArgsInfo.referenceToVector = true;
  locals.emplace("refVectorArgs", refVectorArgsInfo);

  primec::ir_lowerer::LocalInfo ptrVectorArgsInfo;
  ptrVectorArgsInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  ptrVectorArgsInfo.isArgsPack = true;
  ptrVectorArgsInfo.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Pointer;
  ptrVectorArgsInfo.pointerToVector = true;
  locals.emplace("ptrVectorArgs", ptrVectorArgsInfo);

  primec::ir_lowerer::LocalInfo soaVectorArgsInfo;
  soaVectorArgsInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  soaVectorArgsInfo.isArgsPack = true;
  soaVectorArgsInfo.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Pointer;
  soaVectorArgsInfo.pointerToVector = true;
  soaVectorArgsInfo.isSoaVector = true;
  locals.emplace("soaVectorArgs", soaVectorArgsInfo);

  primec::ir_lowerer::LocalInfo soaInfo;
  soaInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  soaInfo.isSoaVector = true;
  locals.emplace("soaValues", soaInfo);

  primec::Expr valuesName;
  valuesName.kind = primec::Expr::Kind::Name;
  valuesName.name = "values";

  primec::Expr capacityCall;
  capacityCall.kind = primec::Expr::Kind::Call;
  capacityCall.name = "capacity";
  capacityCall.args = {valuesName};
  CHECK_FALSE(primec::ir_lowerer::isVectorCapacityCall(capacityCall, locals));
  capacityCall.name = "/std/collections/vector/capacity";
  CHECK_FALSE(primec::ir_lowerer::isVectorCapacityCall(capacityCall, locals));

  primec::Expr refVecName;
  refVecName.kind = primec::Expr::Kind::Name;
  refVecName.name = "refVec";
  capacityCall.name = "capacity";
  capacityCall.args = {refVecName};
  CHECK_FALSE(primec::ir_lowerer::isVectorCapacityCall(capacityCall, locals));

  primec::Expr ptrVecName;
  ptrVecName.kind = primec::Expr::Kind::Name;
  ptrVecName.name = "ptrVec";
  capacityCall.args = {ptrVecName};
  CHECK_FALSE(primec::ir_lowerer::isVectorCapacityCall(capacityCall, locals));

  primec::Expr vectorArgsName;
  vectorArgsName.kind = primec::Expr::Kind::Name;
  vectorArgsName.name = "vectorArgs";
  primec::Expr primitiveArgsName;
  primitiveArgsName.kind = primec::Expr::Kind::Name;
  primitiveArgsName.name = "primitiveArgs";
  primec::Expr primitiveArgsCount;
  primitiveArgsCount.kind = primec::Expr::Kind::Call;
  primitiveArgsCount.name = "count";
  primitiveArgsCount.args = {primitiveArgsName};
  CHECK(primec::ir_lowerer::isArrayCountCall(primitiveArgsCount, locals, false, "argv"));
  primitiveArgsCount.name = "/std/collections/vector/count";
  CHECK(primec::ir_lowerer::isArrayCountCall(primitiveArgsCount, locals, false, "argv"));
  primitiveArgsCount.name = "/vector/count";
  CHECK_FALSE(primec::ir_lowerer::isArrayCountCall(primitiveArgsCount, locals, false, "argv"));

  primec::Expr zeroIndex;
  zeroIndex.kind = primec::Expr::Kind::Literal;
  zeroIndex.literalValue = 0;
  primec::Expr indexedVector;
  indexedVector.kind = primec::Expr::Kind::Call;
  indexedVector.name = "at";
  indexedVector.args = {vectorArgsName, zeroIndex};
  capacityCall.args = {indexedVector};
  CHECK(primec::ir_lowerer::isVectorCapacityCall(capacityCall, locals));

  primec::Expr refVectorArgsName;
  refVectorArgsName.kind = primec::Expr::Kind::Name;
  refVectorArgsName.name = "refVectorArgs";
  primec::Expr indexedRefVector;
  indexedRefVector.kind = primec::Expr::Kind::Call;
  indexedRefVector.name = "at";
  indexedRefVector.args = {refVectorArgsName, zeroIndex};
  primec::Expr derefRefVector;
  derefRefVector.kind = primec::Expr::Kind::Call;
  derefRefVector.name = "dereference";
  derefRefVector.args = {indexedRefVector};
  capacityCall.args = {derefRefVector};
  CHECK(primec::ir_lowerer::isVectorCapacityCall(capacityCall, locals));

  primec::Expr ptrVectorArgsName;
  ptrVectorArgsName.kind = primec::Expr::Kind::Name;
  ptrVectorArgsName.name = "ptrVectorArgs";
  primec::Expr indexedPtrVector;
  indexedPtrVector.kind = primec::Expr::Kind::Call;
  indexedPtrVector.name = "at";
  indexedPtrVector.args = {ptrVectorArgsName, zeroIndex};
  primec::Expr derefPtrVector;
  derefPtrVector.kind = primec::Expr::Kind::Call;
  derefPtrVector.name = "dereference";
  derefPtrVector.args = {indexedPtrVector};
  capacityCall.args = {derefPtrVector};
  CHECK(primec::ir_lowerer::isVectorCapacityCall(capacityCall, locals));

  primec::Expr soaValuesName;
  soaValuesName.kind = primec::Expr::Kind::Name;
  soaValuesName.name = "soaValues";
  primec::Expr toAosCall;
  toAosCall.kind = primec::Expr::Kind::Call;
  toAosCall.name = "/std/collections/soa_vector/to_aos";
  toAosCall.args = {soaValuesName};
  capacityCall.args = {toAosCall};
  CHECK_FALSE(primec::ir_lowerer::isVectorCapacityCall(capacityCall, locals));

  primec::Expr soaVectorArgsName;
  soaVectorArgsName.kind = primec::Expr::Kind::Name;
  soaVectorArgsName.name = "soaVectorArgs";
  primec::Expr indexedSoaVector;
  indexedSoaVector.kind = primec::Expr::Kind::Call;
  indexedSoaVector.name = "at";
  indexedSoaVector.args = {soaVectorArgsName, zeroIndex};
  primec::Expr derefSoaVector;
  derefSoaVector.kind = primec::Expr::Kind::Call;
  derefSoaVector.name = "dereference";
  derefSoaVector.args = {indexedSoaVector};
  capacityCall.args = {derefSoaVector};
  CHECK_FALSE(primec::ir_lowerer::isVectorCapacityCall(capacityCall, locals));

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
  stringInfo.stringSource = primec::ir_lowerer::LocalInfo::StringSource::TableIndex;
  locals.emplace("text", stringInfo);
  primec::Expr textName;
  textName.kind = primec::Expr::Kind::Name;
  textName.name = "text";
  stringCount.args = {textName};
  CHECK(primec::ir_lowerer::isStringCountCall(stringCount, locals));
  stringCount.name = "/vector/count";
  CHECK_FALSE(primec::ir_lowerer::isStringCountCall(stringCount, locals));

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
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return false; },
            [&](int32_t) {},
            error) == Result::NotHandled);

  countCall.args.clear();
  CHECK(primec::ir_lowerer::tryEmitStringCountCall(
            countCall,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
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
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return false; },
            [&](int32_t) {},
            error) == Result::Error);
  CHECK(error == "native backend only supports count() on string literals or string bindings");

  error.clear();
  bool resolvedStringTableTarget = false;
  CHECK(primec::ir_lowerer::tryEmitStringCountCall(
            countCall,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Int32;
            },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) {
              resolvedStringTableTarget = true;
              return true;
            },
            [&](int32_t) {},
            error) == Result::NotHandled);
  CHECK_FALSE(resolvedStringTableTarget);
  CHECK(error.empty());

  error.clear();
  CHECK(primec::ir_lowerer::tryEmitStringCountCall(
            countCall,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
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
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::String;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &stringIndex, size_t &length) {
              stringIndex = 3;
              length = 42;
              return true;
            },
            [&](int32_t length) { emittedLength = length; },
            error) == Result::Emitted);
  CHECK(emittedLength == 42);
}

TEST_CASE("ir lowerer count access helpers emit dynamic string count calls") {
  using Result = primec::ir_lowerer::CountAccessCallEmitResult;

  primec::Expr targetExpr;
  targetExpr.kind = primec::Expr::Kind::Call;
  targetExpr.name = "at";

  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "count";
  callExpr.args = {targetExpr};

  std::vector<primec::IrInstruction> instructions;
  std::string error;
  int emitExprCalls = 0;

  CHECK(primec::ir_lowerer::tryEmitCountAccessCall(
            callExpr,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::String;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              ++emitExprCalls;
              instructions.push_back({primec::IrOpcode::PushI64, 3});
              return true;
            },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            error) == Result::Emitted);
  CHECK(error.empty());
  CHECK(emitExprCalls == 1);
  REQUIRE(instructions.size() == 2);
  CHECK(instructions[0].op == primec::IrOpcode::PushI64);
  CHECK(instructions[1].op == primec::IrOpcode::LoadStringLength);
}

TEST_CASE("ir lowerer count access helpers defer canonical runtime string count calls") {
  using Result = primec::ir_lowerer::CountAccessCallEmitResult;

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo textInfo;
  textInfo.index = 11;
  textInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  textInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::String;
  textInfo.stringSource = primec::ir_lowerer::LocalInfo::StringSource::RuntimeIndex;
  locals.emplace("text", textInfo);

  primec::Expr targetExpr;
  targetExpr.kind = primec::Expr::Kind::Name;
  targetExpr.name = "text";

  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "/std/collections/vector/count";
  callExpr.args = {targetExpr};

  std::vector<primec::IrInstruction> instructions;
  std::string error;
  int emitExprCalls = 0;

  CHECK_FALSE(primec::ir_lowerer::isStringCountCall(callExpr, locals));
  CHECK(primec::ir_lowerer::tryEmitCountAccessCall(
            callExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &countExpr, const primec::ir_lowerer::LocalMap &countLocals) {
              return primec::ir_lowerer::isStringCountCall(countExpr, countLocals);
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &valueExpr, const primec::ir_lowerer::LocalMap &valueLocals) {
              if (valueExpr.kind == primec::Expr::Kind::Name) {
                auto it = valueLocals.find(valueExpr.name);
                if (it != valueLocals.end()) {
                  return it->second.valueKind;
                }
              }
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return false; },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              ++emitExprCalls;
              instructions.push_back({primec::IrOpcode::LoadLocal, 11});
              return true;
            },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            error) == Result::NotHandled);
  CHECK(error.empty());
  CHECK(emitExprCalls == 0);
  CHECK(instructions.empty());
}

TEST_CASE("ir lowerer count access helpers defer canonical runtime string facts") {
  using Result = primec::ir_lowerer::CountAccessCallEmitResult;

  primec::SemanticProgram semanticProgram;
  semanticProgram.bindingFacts.push_back(primec::SemanticProgramBindingFact{
      .scopePath = "/main",
      .siteKind = "local",
      .name = "scalarText",
      .bindingTypeText = "string",
      .semanticNodeId = 7401,
      .bindingTypeTextId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "i32"),
  });
  semanticProgram.publishedRoutingLookups.bindingFactIndicesByExpr.insert_or_assign(7401, 0);
  semanticProgram.bindingFacts.push_back(primec::SemanticProgramBindingFact{
      .scopePath = "/main",
      .siteKind = "local",
      .name = "text",
      .bindingTypeText = "i32",
      .semanticNodeId = 7402,
      .bindingTypeTextId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "string"),
  });
  semanticProgram.publishedRoutingLookups.bindingFactIndicesByExpr.insert_or_assign(7402, 1);
  const auto semanticIndex = primec::ir_lowerer::buildSemanticProductIndex(&semanticProgram);

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo runtimeStringInfo;
  runtimeStringInfo.index = 11;
  runtimeStringInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  runtimeStringInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::String;
  runtimeStringInfo.stringSource = primec::ir_lowerer::LocalInfo::StringSource::RuntimeIndex;
  locals.emplace("scalarText", runtimeStringInfo);
  locals.emplace("text", runtimeStringInfo);
  locals.emplace("missingText", runtimeStringInfo);
  locals.emplace("syntaxText", runtimeStringInfo);

  auto makeTarget = [](const char *name, uint64_t semanticNodeId) {
    primec::Expr expr;
    expr.kind = primec::Expr::Kind::Name;
    expr.name = name;
    expr.semanticNodeId = semanticNodeId;
    return expr;
  };
  auto makeCountCall = [](const primec::Expr &target) {
    primec::Expr expr;
    expr.kind = primec::Expr::Kind::Call;
    expr.name = "/std/collections/vector/count";
    expr.args = {target};
    return expr;
  };

  auto inferLocalKind = [](const primec::Expr &valueExpr, const primec::ir_lowerer::LocalMap &valueLocals) {
    if (valueExpr.kind == primec::Expr::Kind::Name) {
      auto it = valueLocals.find(valueExpr.name);
      if (it != valueLocals.end()) {
        return it->second.valueKind;
      }
    }
    return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  };
  auto emitCall = [&](const primec::Expr &callExpr, std::vector<primec::IrInstruction> &instructions) {
    std::string error;
    return primec::ir_lowerer::tryEmitCountAccessCall(
        callExpr,
        locals,
        [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
        [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
        [&](const primec::Expr &countExpr, const primec::ir_lowerer::LocalMap &countLocals) {
          return primec::ir_lowerer::isStringCountCall(countExpr, countLocals, &semanticProgram);
        },
        [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
        [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
        [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
        [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
        inferLocalKind,
        [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return false; },
        [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
          instructions.push_back({primec::IrOpcode::LoadLocal, 11});
          return true;
        },
        [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
        error,
        &semanticProgram,
        &semanticIndex);
  };

  std::vector<primec::IrInstruction> instructions;
  CHECK(emitCall(makeCountCall(makeTarget("scalarText", 7401)), instructions) == Result::NotHandled);
  CHECK(instructions.empty());

  CHECK(emitCall(makeCountCall(makeTarget("missingText", 7499)), instructions) == Result::NotHandled);
  CHECK(instructions.empty());

  CHECK(emitCall(makeCountCall(makeTarget("text", 7402)), instructions) == Result::NotHandled);
  CHECK(instructions.empty());

  instructions.clear();
  CHECK(emitCall(makeCountCall(makeTarget("syntaxText", 0)), instructions) == Result::NotHandled);
  CHECK(instructions.empty());
}

TEST_CASE("ir lowerer call helpers lower soa_vector count calls") {
  using Result = primec::ir_lowerer::NativeCallTailDispatchResult;
  using LocalInfo = primec::ir_lowerer::LocalInfo;

  primec::ir_lowerer::LocalMap locals;
  LocalInfo soaInfo;
  soaInfo.kind = LocalInfo::Kind::Value;
  soaInfo.index = 5;
  soaInfo.valueKind = LocalInfo::ValueKind::Unknown;
  soaInfo.isSoaVector = true;
  locals.emplace("soaValues", soaInfo);

  primec::Expr soaName;
  soaName.kind = primec::Expr::Kind::Name;
  soaName.name = "soaValues";

  primec::Expr countCall;
  countCall.kind = primec::Expr::Kind::Call;
  countCall.name = "count";
  countCall.args = {soaName};

  std::vector<primec::IrInstruction> instructions;
  auto emitInstruction = [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); };
  auto instructionCount = [&]() { return instructions.size(); };
  auto patchInstructionImm = [&](size_t index, uint64_t imm) { instructions.at(index).imm = imm; };
  int nextLocal = 32;
  std::string error;

  CHECK(primec::ir_lowerer::tryEmitNativeCallTailDispatch(
            countCall,
            locals,
            [](const primec::Expr &, std::string &) { return false; },
            [](const std::string &) { return true; },
            [](const primec::Expr &callExpr, const primec::ir_lowerer::LocalMap &callLocals) {
              return primec::ir_lowerer::isArrayCountCall(callExpr, callLocals, false, "argv");
            },
            [](const primec::Expr &callExpr, const primec::ir_lowerer::LocalMap &callLocals) {
              return primec::ir_lowerer::isVectorCapacityCall(callExpr, callLocals);
            },
            [](const primec::Expr &callExpr, const primec::ir_lowerer::LocalMap &callLocals) {
              return primec::ir_lowerer::isStringCountCall(callExpr, callLocals);
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return false; },
            [&](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &emitLocals) {
              if (expr.kind != primec::Expr::Kind::Name) {
                return false;
              }
              auto it = emitLocals.find(expr.name);
              if (it == emitLocals.end()) {
                return false;
              }
              emitInstruction(primec::IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index));
              return true;
            },
            [](const primec::Expr &, std::string &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return LocalInfo::ValueKind::Int32; },
            [&]() { return nextLocal++; },
            []() {},
            []() {},
            []() {},
            instructionCount,
            emitInstruction,
            patchInstructionImm,
            error) == Result::Emitted);
  CHECK(error.empty());
  REQUIRE(instructions.size() >= 2);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions.back().op == primec::IrOpcode::LoadIndirect);
}

TEST_SUITE_END();
