#include "test_ir_pipeline_validation_callback_factories.h"
#include "primec/testing/IrLowererCollectionSurfaceContracts.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir lowerer call-helper contracts replace source delegation locks") {
  using InlineResult = primec::ir_lowerer::InlineCallDispatchResult;
  using ResolvedInlineResult = primec::ir_lowerer::ResolvedInlineCallResult;

  primec::Definition directDef;
  directDef.fullPath = "/main/direct";
  primec::Definition importedDef;
  importedDef.fullPath = "/pkg/imported";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {directDef.fullPath, &directDef},
      {importedDef.fullPath, &importedDef},
  };
  const std::unordered_map<std::string, std::string> importAliases = {
      {"importedAlias", importedDef.fullPath},
  };

  const auto adapters = primec::ir_lowerer::makeCallResolutionAdapters(defMap, importAliases);

  primec::Expr directCall;
  directCall.kind = primec::Expr::Kind::Call;
  directCall.namespacePrefix = "/main";
  directCall.name = "direct";
  CHECK(adapters.resolveExprPath(directCall) == directDef.fullPath);
  CHECK(primec::ir_lowerer::resolveDefinitionCall(directCall,
                                                  defMap,
                                                  adapters.resolveExprPath) == &directDef);

  primec::Expr importedCall;
  importedCall.kind = primec::Expr::Kind::Call;
  importedCall.name = "importedAlias";
  CHECK(adapters.resolveExprPath(importedCall) == importedDef.fullPath);
  CHECK(primec::ir_lowerer::resolveDefinitionCall(importedCall,
                                                  defMap,
                                                  adapters.resolveExprPath) == &importedDef);
  CHECK(adapters.isTailCallCandidate(importedCall));
  CHECK(adapters.definitionExists(importedDef.fullPath));

  primec::Expr missingCall = importedCall;
  missingCall.name = "missing";
  CHECK_FALSE(adapters.isTailCallCandidate(missingCall));
  CHECK_FALSE(adapters.definitionExists("/pkg/missing"));

  primec::Expr methodCall = importedCall;
  methodCall.isMethodCall = true;
  CHECK(primec::ir_lowerer::resolveDefinitionCall(methodCall,
                                                  defMap,
                                                  adapters.resolveExprPath) == nullptr);

  int emittedInlineCalls = 0;
  std::string error;
  CHECK(primec::ir_lowerer::emitResolvedInlineDefinitionCall(
            importedCall,
            &importedDef,
            [&](const primec::Expr &expr, const primec::Definition &definition) {
              CHECK(expr.name == "importedAlias");
              CHECK(&definition == &importedDef);
              ++emittedInlineCalls;
              return true;
            },
            error) == ResolvedInlineResult::Emitted);
  CHECK(error.empty());
  CHECK(emittedInlineCalls == 1);

  primec::Expr blockCall = importedCall;
  blockCall.hasBodyArguments = true;
  error.clear();
  CHECK(primec::ir_lowerer::emitResolvedInlineDefinitionCall(
            blockCall,
            &importedDef,
            [](const primec::Expr &, const primec::Definition &) { return true; },
            error) == ResolvedInlineResult::Error);
  CHECK(error == "native backend does not support block arguments on calls");

  emittedInlineCalls = 0;
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitInlineCallWithCountFallbacks(
            importedCall,
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &) -> const primec::Definition * { return nullptr; },
            [&](const primec::Expr &expr) {
              return primec::ir_lowerer::resolveDefinitionCall(
                  expr, defMap, adapters.resolveExprPath);
            },
            [&](const primec::Expr &expr, const primec::Definition &definition) {
              CHECK(expr.name == "importedAlias");
              CHECK(&definition == &importedDef);
              ++emittedInlineCalls;
              return true;
            },
            error) == InlineResult::Emitted);
  CHECK(error.empty());
  CHECK(emittedInlineCalls == 1);

  primec::Expr tailCall = importedCall;
  primec::Expr returnCall;
  returnCall.kind = primec::Expr::Kind::Call;
  returnCall.name = "return";
  returnCall.args = {tailCall};

  primec::Definition entryDef;
  entryDef.fullPath = "/main";
  entryDef.statements = {returnCall};
  const auto entrySetup = primec::ir_lowerer::buildEntryCallResolutionSetup(
      entryDef, false, defMap, importAliases);
  CHECK(entrySetup.hasTailExecution);
}

TEST_CASE("ir lowerer collection dispatch metadata resolves through public contracts") {
  const auto *vectorMetadata = primec::ir_lowerer::vectorHelperSurfaceMetadata();
  const auto *mapMetadata = primec::ir_lowerer::keyValueHelperSurfaceMetadata();
  const auto *mapConstructorMetadata =
      primec::ir_lowerer::keyValueConstructorSurfaceMetadata();
  REQUIRE(vectorMetadata != nullptr);
  REQUIRE(mapMetadata != nullptr);
  REQUIRE(mapConstructorMetadata != nullptr);

  auto expectSurfaceMember = [](std::string_view path,
                                primec::StdlibSurfaceId surfaceId,
                                std::string_view expectedMember) {
    std::string memberName;
    CHECK(primec::ir_lowerer::resolvePublishedStdlibSurfaceMemberName(
        path, surfaceId, memberName));
    CHECK(memberName == expectedMember);
    CHECK(primec::ir_lowerer::isPublishedStdlibSurfaceLoweringPath(path,
                                                                   surfaceId));
    CHECK(primec::ir_lowerer::isCanonicalPublishedStdlibSurfaceHelperPath(
        path, surfaceId));
  };

  expectSurfaceMember("/std/collections/vector/count", vectorMetadata->id, "count");
  expectSurfaceMember("/std/collections/vector/at_unsafe",
                      vectorMetadata->id,
                      "at_unsafe");
  expectSurfaceMember("/std/collections/map/contains", mapMetadata->id, "contains");
  expectSurfaceMember("/std/collections/map/tryAt", mapMetadata->id, "tryAt");
  expectSurfaceMember("/std/collections/map/map",
                      mapConstructorMetadata->id,
                      "map");

  std::string memberName = "stale";
  CHECK_FALSE(primec::ir_lowerer::resolvePublishedStdlibSurfaceMemberName(
      "/std/collections/experimental_vector/vectorCount__t1",
      vectorMetadata->id,
      memberName));
  CHECK(memberName.empty());

  primec::SemanticProgram semanticProgram;
  const primec::SymbolId vectorAtPathId =
      primec::semanticProgramInternCallTargetString(semanticProgram,
                                                    "/std/collections/vector/at");
  semanticProgram.publishedRoutingLookups.directCallTargetIdsByExpr.insert_or_assign(
      7101, vectorAtPathId);
  semanticProgram.publishedRoutingLookups.directCallStdlibSurfaceIdsByExpr.insert_or_assign(
      7101, vectorMetadata->id);

  const primec::SymbolId mapContainsPathId =
      primec::semanticProgramInternCallTargetString(semanticProgram,
                                                    "/std/collections/map/contains");
  semanticProgram.publishedRoutingLookups.methodCallTargetIdsByExpr.insert_or_assign(
      7102, mapContainsPathId);
  semanticProgram.publishedRoutingLookups.methodCallStdlibSurfaceIdsByExpr.insert_or_assign(
      7102, mapMetadata->id);

  const primec::SymbolId mapTryAtPathId =
      primec::semanticProgramInternCallTargetString(semanticProgram,
                                                    "/std/collections/map/tryAt");
  semanticProgram.publishedRoutingLookups.bridgePathChoiceIdsByExpr.insert_or_assign(
      7103, mapTryAtPathId);
  semanticProgram.publishedRoutingLookups.bridgePathChoiceStdlibSurfaceIdsByExpr.insert_or_assign(
      7103, mapMetadata->id);

  auto expectSemanticSurfaceMember = [&](uint64_t semanticNodeId,
                                         primec::StdlibSurfaceId surfaceId,
                                         std::string_view expectedMember) {
    primec::Expr callExpr;
    callExpr.kind = primec::Expr::Kind::Call;
    callExpr.semanticNodeId = semanticNodeId;
    std::string semanticMember;
    CHECK(primec::ir_lowerer::resolvePublishedSemanticStdlibSurfaceMemberName(
        &semanticProgram, callExpr, surfaceId, semanticMember));
    CHECK(semanticMember == expectedMember);
  };

  expectSemanticSurfaceMember(7101, vectorMetadata->id, "at");
  expectSemanticSurfaceMember(7102, mapMetadata->id, "contains");
  expectSemanticSurfaceMember(7103, mapMetadata->id, "tryAt");

  primec::Expr wrongSurfaceCall;
  wrongSurfaceCall.kind = primec::Expr::Kind::Call;
  wrongSurfaceCall.semanticNodeId = 7102;
  memberName = "stale";
  CHECK_FALSE(primec::ir_lowerer::resolvePublishedSemanticStdlibSurfaceMemberName(
      &semanticProgram, wrongSurfaceCall, vectorMetadata->id, memberName));
  CHECK(memberName.empty());

  primec::Expr nonCallExpr;
  nonCallExpr.kind = primec::Expr::Kind::Name;
  nonCallExpr.semanticNodeId = 7101;
  CHECK_FALSE(primec::ir_lowerer::resolvePublishedSemanticStdlibSurfaceMemberName(
      &semanticProgram, nonCallExpr, vectorMetadata->id, memberName));
}

namespace {

primec::Definition *findLowererDefinitionByPathMutable(primec::Program &program, std::string_view fullPath) {
  const auto it =
      std::find_if(program.definitions.begin(),
                   program.definitions.end(),
                   [fullPath](const primec::Definition &definition) { return definition.fullPath == fullPath; });
  return it == program.definitions.end() ? nullptr : &*it;
}

template <typename Entry, typename Predicate>
const Entry *findLowererSemanticEntry(const std::vector<const Entry *> &entries, const Predicate &predicate) {
  const auto it = std::find_if(entries.begin(),
                               entries.end(),
                               [&](const Entry *entry) { return entry != nullptr && predicate(*entry); });
  return it == entries.end() ? nullptr : *it;
}

template <typename Predicate>
primec::Expr *findLowererExprRecursiveMutable(primec::Expr &expr, const Predicate &predicate) {
  if (predicate(expr)) {
    return &expr;
  }
  for (auto &arg : expr.args) {
    if (primec::Expr *found = findLowererExprRecursiveMutable(arg, predicate)) {
      return found;
    }
  }
  for (auto &bodyExpr : expr.bodyArguments) {
    if (primec::Expr *found = findLowererExprRecursiveMutable(bodyExpr, predicate)) {
      return found;
    }
  }
  return nullptr;
}

template <typename Predicate>
primec::Expr *findLowererExprInDefinitionMutable(primec::Definition &definition, const Predicate &predicate) {
  for (auto &parameter : definition.parameters) {
    if (primec::Expr *found = findLowererExprRecursiveMutable(parameter, predicate)) {
      return found;
    }
  }
  for (auto &statement : definition.statements) {
    if (primec::Expr *found = findLowererExprRecursiveMutable(statement, predicate)) {
      return found;
    }
  }
  if (definition.returnExpr.has_value()) {
    return findLowererExprRecursiveMutable(*definition.returnExpr, predicate);
  }
  return nullptr;
}

} // namespace

TEST_CASE("ir lowerer call helpers consume pilot routing semantic-product facts") {
  const std::string source = R"(
import /std/collections/*

[return<i32>]
id_i32([i32] value) {
  return(value)
}

[effects(heap_alloc), return<i32>]
main() {
  [auto] selected{id_i32(1i32)}
  [auto] values{vector<i32>(1i32)}
  [i32] viaMethod{values.count()}
  [i32] viaBridge{count(values)}
  return(plus(selected, plus(viaMethod, viaBridge)))
}
)";

  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  INFO(error);
  REQUIRE(parseAndValidate(source, program, semanticProgram, error, {"io_out", "io_err"}));
  CHECK(error.empty());

  const auto adapter = primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);
  primec::Definition *mainDef = findLowererDefinitionByPathMutable(program, "/main");
  REQUIRE(mainDef != nullptr);

  primec::Expr *directExpr = findLowererExprInDefinitionMutable(
      *mainDef,
      [](const primec::Expr &expr) {
        return expr.kind == primec::Expr::Kind::Call && !expr.isMethodCall && expr.name == "id_i32";
      });
  primec::Expr *methodExpr = findLowererExprInDefinitionMutable(
      *mainDef,
      [](const primec::Expr &expr) {
        return expr.kind == primec::Expr::Kind::Call && expr.isMethodCall && expr.name == "count";
      });
  primec::Expr *vectorDirectExpr = findLowererExprInDefinitionMutable(
      *mainDef,
      [](const primec::Expr &expr) {
        return expr.kind == primec::Expr::Kind::Call && !expr.isMethodCall &&
               expr.name == "count" && expr.args.size() == 1 &&
               expr.args.front().kind == primec::Expr::Kind::Name &&
               expr.args.front().name == "values";
      });
  REQUIRE(directExpr != nullptr);
  REQUIRE(methodExpr != nullptr);
  REQUIRE(vectorDirectExpr != nullptr);

  CHECK(semanticProgram.publishedRoutingLookups.directCallTargetIdsByExpr.count(directExpr->semanticNodeId) == 1);
  CHECK(semanticProgram.publishedRoutingLookups.methodCallTargetIdsByExpr.count(methodExpr->semanticNodeId) == 1);
  CHECK(semanticProgram.publishedRoutingLookups.directCallTargetIdsByExpr.count(
            vectorDirectExpr->semanticNodeId) == 1);
  CHECK(semanticProgram.publishedRoutingLookups.directCallStdlibSurfaceIdsByExpr.count(
            directExpr->semanticNodeId) == 0);
  CHECK(semanticProgram.publishedRoutingLookups.methodCallStdlibSurfaceIdsByExpr.count(
            methodExpr->semanticNodeId) == 1);
  CHECK(semanticProgram.publishedRoutingLookups.directCallStdlibSurfaceIdsByExpr.count(
            vectorDirectExpr->semanticNodeId) == 1);
  CHECK(semanticProgram.publishedRoutingLookups.bridgePathChoiceIdsByExpr.count(
            vectorDirectExpr->semanticNodeId) == 1);
  CHECK(primec::ir_lowerer::findSemanticProductDirectCallTarget(adapter, *directExpr) == "/id_i32");
  CHECK(primec::ir_lowerer::findSemanticProductMethodCallTarget(adapter, *methodExpr) ==
        "/std/collections/vector/count");
  CHECK(primec::ir_lowerer::findSemanticProductDirectCallTarget(adapter, *vectorDirectExpr) ==
        "/std/collections/vector/count");
  CHECK_FALSE(primec::ir_lowerer::findSemanticProductDirectCallStdlibSurfaceId(adapter, *directExpr)
                  .has_value());
  const auto methodSurfaceId =
      primec::ir_lowerer::findSemanticProductMethodCallStdlibSurfaceId(adapter, *methodExpr);
  REQUIRE(methodSurfaceId.has_value());
  CHECK(*methodSurfaceId == primec::StdlibSurfaceId::CollectionsManifestSurface0);
  const auto vectorDirectSurfaceId =
      primec::ir_lowerer::findSemanticProductDirectCallStdlibSurfaceId(
          adapter, *vectorDirectExpr);
  REQUIRE(vectorDirectSurfaceId.has_value());
  CHECK(*vectorDirectSurfaceId == primec::StdlibSurfaceId::CollectionsManifestSurface0);

  const auto *summary = primec::ir_lowerer::findSemanticProductCallableSummary(adapter, "/main");
  REQUIRE(summary != nullptr);
  CHECK(semanticProgram.publishedRoutingLookups.callableSummaryIndicesByPathId.count(summary->fullPathId) == 1);
  CHECK(summary->returnKind == "i32");
}

TEST_CASE("ir lowerer call helpers publish root soa constructor metadata without bridge choices") {
  const std::string source = R"(
[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [soa<Particle>] values{soa<Particle>(Particle{7i32}, Particle{9i32})}
  return(0i32)
}
)";

  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  INFO(error);
  REQUIRE(parseAndValidate(source, program, semanticProgram, error, {"io_out", "io_err"}));
  CHECK(error.empty());

  const auto *collectionEntry = findLowererSemanticEntry(
      primec::semanticProgramCollectionSpecializationView(semanticProgram),
      [](const primec::SemanticProgramCollectionSpecialization &entry) {
        return entry.scopePath == "/main" && entry.name == "values" &&
               entry.collectionFamily == "soa";
      });
  REQUIRE(collectionEntry != nullptr);
  REQUIRE(collectionEntry->constructorSurfaceId.has_value());
  CHECK(*collectionEntry->constructorSurfaceId ==
        primec::StdlibSurfaceId::CollectionsColumnarConstructors);

  const auto adapter = primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);
  primec::Definition *mainDef = findLowererDefinitionByPathMutable(program, "/main");
  REQUIRE(mainDef != nullptr);
  primec::Expr *constructorExpr = findLowererExprInDefinitionMutable(
      *mainDef,
      [](const primec::Expr &expr) {
        return expr.kind == primec::Expr::Kind::Call && !expr.isMethodCall &&
               expr.name == "soa";
      });
  REQUIRE(constructorExpr != nullptr);
  CHECK(semanticProgram.publishedRoutingLookups.bridgePathChoiceIdsByExpr.count(
            constructorExpr->semanticNodeId) == 0);
  CHECK(primec::ir_lowerer::findSemanticProductBridgePathChoice(adapter, *constructorExpr)
            .empty());
}

TEST_CASE("ir lowerer call helpers keep exact-import vector and map bridge parity") {
  const std::string source = R"(
import /std/collections/vector
import /std/collections/map

[effects(heap_alloc), return<i32>]
main() {
  [auto] values{vector<i32>(1i32, 2i32)}
  [auto] pairs{map<i32, i32>(1i32, 7i32, 2i32, 11i32)}
  [i32] viaVector{values.count()}
  [i32] viaMap{pairs.count()}
  return(plus(viaVector, viaMap))
}
)";

  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error, {"io_out", "io_err"}));
  CHECK(error.empty());

  const auto adapter = primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);
  primec::Definition *mainDef = findLowererDefinitionByPathMutable(program, "/main");
  REQUIRE(mainDef != nullptr);
  primec::Expr *vectorDirectExpr = findLowererExprInDefinitionMutable(
      *mainDef,
      [](const primec::Expr &expr) {
        return expr.kind == primec::Expr::Kind::Call && expr.isMethodCall &&
               expr.name == "count" && expr.args.size() == 1 &&
               expr.args.front().kind == primec::Expr::Kind::Name &&
               expr.args.front().name == "values";
      });
  primec::Expr *mapDirectExpr = findLowererExprInDefinitionMutable(
      *mainDef,
      [](const primec::Expr &expr) {
        return expr.kind == primec::Expr::Kind::Call && expr.isMethodCall &&
               expr.name == "count" && expr.args.size() == 1 &&
               expr.args.front().kind == primec::Expr::Kind::Name &&
               expr.args.front().name == "pairs";
      });
  REQUIRE(vectorDirectExpr != nullptr);
  REQUIRE(mapDirectExpr != nullptr);

  CHECK(semanticProgram.publishedRoutingLookups.methodCallTargetIdsByExpr.count(
            vectorDirectExpr->semanticNodeId) == 1);
  CHECK(semanticProgram.publishedRoutingLookups.methodCallStdlibSurfaceIdsByExpr.count(
            vectorDirectExpr->semanticNodeId) == 1);
  CHECK(primec::ir_lowerer::findSemanticProductMethodCallTarget(adapter, *vectorDirectExpr) ==
        "/std/collections/vector/count");
  CHECK(semanticProgram.publishedRoutingLookups.methodCallTargetIdsByExpr.count(
            mapDirectExpr->semanticNodeId) == 1);
  CHECK(semanticProgram.publishedRoutingLookups.methodCallStdlibSurfaceIdsByExpr.count(
            mapDirectExpr->semanticNodeId) == 1);
  CHECK(primec::ir_lowerer::findSemanticProductMethodCallTarget(adapter, *mapDirectExpr) ==
        "/std/collections/map/count");
  CHECK(primec::ir_lowerer::findSemanticProductMethodCallStdlibSurfaceId(
            adapter, *mapDirectExpr)
            .has_value());
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

TEST_CASE("ir lowerer call helpers prefer explicit experimental vector helpers over structs") {
  primec::Definition vectorHelper;
  vectorHelper.fullPath = "/std/collections/experimental_vector/vector__t25a78a513414c3bf";
  primec::Definition vectorStruct;
  vectorStruct.fullPath = "/std/collections/vector/Vector__t25a78a513414c3bf";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {vectorHelper.fullPath, &vectorHelper},
      {vectorStruct.fullPath, &vectorStruct},
  };
  auto resolver = [](const primec::Expr &) {
    return std::string("/std/collections/vector/Vector__t25a78a513414c3bf");
  };

  primec::Expr directCall;
  directCall.kind = primec::Expr::Kind::Call;
  directCall.name = "/std/collections/experimental_vector/vector";
  directCall.templateArgs = {"i32"};

  CHECK(primec::ir_lowerer::resolveDefinitionCall(directCall, defMap, resolver) ==
        &vectorHelper);
}

TEST_CASE("ir lowerer call helpers reject missing definition path resolver") {
  primec::Definition callee;
  callee.fullPath = "/callee";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {{"/callee", &callee}};

  primec::Expr directCall;
  directCall.kind = primec::Expr::Kind::Call;
  directCall.name = "callee";

  CHECK(primec::ir_lowerer::resolveDefinitionCall(
            directCall, defMap, primec::ExprStringFn{}) == nullptr);
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
      {"map_count", "std/collections/map/count"},
      {"map_at", "map/at"},
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

  primec::Expr slashlessCanonicalMapAlias;
  slashlessCanonicalMapAlias.name = "map_count";
  CHECK(primec::ir_lowerer::resolveCallPathFromScope(
            slashlessCanonicalMapAlias, defMap, importAliases) ==
        "std/collections/map/count");

  primec::Expr slashlessMapAlias;
  slashlessMapAlias.name = "map_at";
  CHECK(primec::ir_lowerer::resolveCallPathFromScope(
            slashlessMapAlias, defMap, importAliases) == "map/at");
}

TEST_CASE("ir lowerer call helpers build scoped call path resolver") {
  primec::Definition scopedDef;
  scopedDef.fullPath = "/pkg/foo";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {{"/pkg/foo", &scopedDef}};
  const std::unordered_map<std::string, std::string> importAliases = {
      {"foo", "/import/foo"},
      {"bar", "/import/bar"},
      {"map_count", "std/collections/map/count"},
      {"map_at", "map/at"},
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

  primec::Expr slashlessCanonicalMapAlias;
  slashlessCanonicalMapAlias.name = "map_count";
  CHECK(resolveExprPath(slashlessCanonicalMapAlias) == "std/collections/map/count");

  primec::Expr slashlessMapAlias;
  slashlessMapAlias.name = "map_at";
  CHECK(resolveExprPath(slashlessMapAlias) == "map/at");
}

TEST_CASE("ir lowerer call helpers keep alias fallback only on raw path") {
  primec::Definition scopedDef;
  scopedDef.fullPath = "/pkg/foo";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {{"/pkg/foo", &scopedDef}};
  const std::unordered_map<std::string, std::string> importAliases = {
      {"bar", "/import/bar"},
      {"map_count", "std/collections/map/count"},
  };

  const auto rawResolveExprPath =
      primec::ir_lowerer::makeResolveCallPathFromScope(defMap, importAliases);

  primec::SemanticProgram semanticProgram;
  const auto semanticResolveExprPath =
      primec::ir_lowerer::makeResolveCallPathFromScope(defMap, importAliases, &semanticProgram);

  primec::Expr namespacedAlias;
  namespacedAlias.name = "bar";
  namespacedAlias.namespacePrefix = "/pkg";
  CHECK(rawResolveExprPath(namespacedAlias) == "/import/bar");
  CHECK(semanticResolveExprPath(namespacedAlias) == "/pkg/bar");

  primec::Expr slashlessCanonicalMapAlias;
  slashlessCanonicalMapAlias.name = "map_count";
  CHECK(rawResolveExprPath(slashlessCanonicalMapAlias) == "std/collections/map/count");
  CHECK(semanticResolveExprPath(slashlessCanonicalMapAlias) == "/map_count");
}

TEST_CASE("ir lowerer call helpers avoid semantic-product scope/root fallback probes") {
  primec::Definition rootDef;
  rootDef.fullPath = "/foo";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/foo", &rootDef},
  };
  const std::unordered_map<std::string, std::string> importAliases = {};

  primec::SemanticProgram semanticProgram;
  const auto semanticResolveExprPath =
      primec::ir_lowerer::makeResolveCallPathFromScope(defMap, importAliases, &semanticProgram);

  primec::Expr namespacedExpr;
  namespacedExpr.kind = primec::Expr::Kind::Name;
  namespacedExpr.name = "foo";
  namespacedExpr.namespacePrefix = "/pkg";
  CHECK(semanticResolveExprPath(namespacedExpr) == "/pkg/foo");
}

TEST_CASE("ir lowerer call helpers fail closed when semantic-product direct-call targets are missing") {
  primec::Definition callee;
  callee.fullPath = "/imported/callee";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/imported/callee", &callee},
  };
  const std::unordered_map<std::string, std::string> importAliases = {
      {"callee", "/imported/callee"},
  };

  primec::SemanticProgram semanticProgram;
  const auto semanticResolveExprPath =
      primec::ir_lowerer::makeResolveCallPathFromScope(defMap, importAliases, &semanticProgram);
  const auto semanticResolveDefinitionCall =
      primec::ir_lowerer::makeResolveDefinitionCall(defMap, semanticResolveExprPath);

  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "callee";
  callExpr.semanticNodeId = 17;
  CHECK(semanticResolveExprPath(callExpr) == "/callee");
  CHECK(semanticResolveDefinitionCall(callExpr) == nullptr);

  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "callee",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 17,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/imported/callee"),
      .stdlibSurfaceId = std::nullopt,
  });
  semanticProgram.publishedRoutingLookups.directCallTargetIdsByExpr.insert_or_assign(
      17, primec::semanticProgramInternCallTargetString(semanticProgram, "/imported/callee"));
  const auto populatedResolveExprPath =
      primec::ir_lowerer::makeResolveCallPathFromScope(defMap, importAliases, &semanticProgram);
  CHECK(populatedResolveExprPath(callExpr) == "/imported/callee");
}

TEST_CASE("ir lowerer call helpers keep unresolved rooted semantic operator targets authoritative") {
  primec::Definition importedMultiply;
  importedMultiply.fullPath = "/std/math/multiply";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/std/math/multiply", &importedMultiply},
  };
  const std::unordered_map<std::string, std::string> importAliases = {
      {"multiply", "/std/math/multiply"},
  };

  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "multiply";
  callExpr.semanticNodeId = 71;

  primec::SemanticProgram semanticProgram;
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "multiply",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 71,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/multiply"),
      .stdlibSurfaceId = std::nullopt,
  });
  semanticProgram.publishedRoutingLookups.directCallTargetIdsByExpr.insert_or_assign(
      71, primec::semanticProgramInternCallTargetString(semanticProgram, "/multiply"));
  const auto resolveExprPath =
      primec::ir_lowerer::makeResolveCallPathFromScope(defMap, importAliases, &semanticProgram);
  const auto resolveDefinitionCall =
      primec::ir_lowerer::makeResolveDefinitionCall(defMap, resolveExprPath);

  CHECK(resolveExprPath(callExpr) == "/multiply");
  CHECK(resolveDefinitionCall(callExpr) == nullptr);
}

TEST_CASE("ir lowerer call helpers keep semantic-product direct-call targets authoritative over rooted rewritten expr names") {
  primec::Definition legacyRootedCall;
  legacyRootedCall.fullPath = "/legacy";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/legacy", &legacyRootedCall},
  };
  const std::unordered_map<std::string, std::string> importAliases = {};

  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "/legacy";
  callExpr.semanticNodeId = 19;

  primec::SemanticProgram semanticProgram;
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "/legacy",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 19,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/semantic/target"),
      .stdlibSurfaceId = std::nullopt,
  });
  semanticProgram.publishedRoutingLookups.directCallTargetIdsByExpr.insert_or_assign(
      19, primec::semanticProgramInternCallTargetString(semanticProgram, "/semantic/target"));
  const auto resolveExprPath =
      primec::ir_lowerer::makeResolveCallPathFromScope(defMap, importAliases, &semanticProgram);

  CHECK(resolveExprPath(callExpr) == "/semantic/target");
}

TEST_CASE("ir lowerer call helpers keep rooted rewritten expr names when semantic-product direct-call targets are missing") {
  const std::unordered_map<std::string, const primec::Definition *> defMap = {};
  const std::unordered_map<std::string, std::string> importAliases = {};

  primec::Expr rewrittenExpr;
  rewrittenExpr.kind = primec::Expr::Kind::Call;
  rewrittenExpr.name = "/operator/add";

  primec::SemanticProgram semanticProgram;
  const auto resolveExprPath =
      primec::ir_lowerer::makeResolveCallPathFromScope(defMap, importAliases, &semanticProgram);

  CHECK(resolveExprPath(rewrittenExpr) == "/operator/add");
}

TEST_CASE("ir lowerer call helpers keep source-shaped method paths when semantic-product targets are missing") {
  const std::unordered_map<std::string, const primec::Definition *> defMap = {};
  const std::unordered_map<std::string, std::string> importAliases = {
      {"contains", "/std/collections/map/contains"},
  };

  primec::Expr methodExpr;
  methodExpr.kind = primec::Expr::Kind::Call;
  methodExpr.isMethodCall = true;
  methodExpr.namespacePrefix = "main";
  methodExpr.name = "contains";
  methodExpr.semanticNodeId = 44;

  primec::SemanticProgram semanticProgram;
  semanticProgram.bridgePathChoices.push_back(primec::SemanticProgramBridgePathChoice{
      .scopePath = "/main",
      .collectionFamily = "map",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 44,
      .scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
      .collectionFamilyId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "map"),
      .helperNameId = primec::semanticProgramInternCallTargetString(semanticProgram, "contains"),
      .chosenPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram,
                                                        "/std/collections/map/contains"),
      .stdlibSurfaceId = primec::findStdlibSurfaceMetadataByBridgeKey("collections.map_helpers")->id,
  });
  auto resolveExprPath =
      primec::ir_lowerer::makeResolveCallPathFromScope(defMap, importAliases, &semanticProgram);
  CHECK(resolveExprPath(methodExpr) == "/main/contains");

  semanticProgram.methodCallTargets.push_back(primec::SemanticProgramMethodCallTarget{
      .scopePath = "/main",
      .methodName = "contains",
      .receiverTypeText = "",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 44,
      .scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
      .methodNameId = primec::semanticProgramInternCallTargetString(semanticProgram, "contains"),
      .resolvedPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram,
                                                        "/std/collections/map/contains"),
      .stdlibSurfaceId = std::nullopt,
  });
  semanticProgram.publishedRoutingLookups.methodCallTargetIdsByExpr.insert_or_assign(
      44,
      primec::semanticProgramInternCallTargetString(
          semanticProgram, "/std/collections/map/contains"));
  resolveExprPath =
      primec::ir_lowerer::makeResolveCallPathFromScope(defMap, importAliases, &semanticProgram);
  CHECK(resolveExprPath(methodExpr) == "/std/collections/map/contains");
}

TEST_CASE("ir lowerer semantic-product adapter reuses method-call path ids") {
  primec::SemanticProgram semanticProgram;
  semanticProgram.methodCallTargets.push_back(primec::SemanticProgramMethodCallTarget{
      .scopePath = "/main",
      .methodName = "contains",
      .receiverTypeText = "",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 44,
      .scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
      .methodNameId = primec::semanticProgramInternCallTargetString(semanticProgram, "contains"),
      .resolvedPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram,
                                                        "/std/collections/map/contains"),
      .stdlibSurfaceId = std::nullopt,
  });
  semanticProgram.methodCallTargets.push_back(primec::SemanticProgramMethodCallTarget{
      .scopePath = "/main",
      .methodName = "contains",
      .receiverTypeText = "",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 45,
      .scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
      .methodNameId = primec::semanticProgramInternCallTargetString(semanticProgram, "contains"),
      .resolvedPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram,
                                                        "/std/collections/map/contains"),
      .stdlibSurfaceId = std::nullopt,
  });
  semanticProgram.publishedRoutingLookups.methodCallTargetIdsByExpr.insert_or_assign(
      44,
      semanticProgram.methodCallTargets[0].resolvedPathId);
  semanticProgram.publishedRoutingLookups.methodCallTargetIdsByExpr.insert_or_assign(
      45,
      semanticProgram.methodCallTargets[1].resolvedPathId);

  const auto adapter = primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);
  REQUIRE(adapter.semanticProgram == &semanticProgram);
  REQUIRE(adapter.publishedRoutingLookups != nullptr);
  REQUIRE(adapter.publishedRoutingLookups->methodCallTargetIdsByExpr.count(44) == 1);
  REQUIRE(adapter.publishedRoutingLookups->methodCallTargetIdsByExpr.count(45) == 1);
  CHECK(adapter.publishedRoutingLookups->methodCallTargetIdsByExpr.at(44) ==
        adapter.publishedRoutingLookups->methodCallTargetIdsByExpr.at(45));
  CHECK(adapter.publishedRoutingLookups->methodCallTargetIdsByExpr.at(44) ==
        semanticProgram.methodCallTargets[0].resolvedPathId);

  primec::Expr firstExpr;
  firstExpr.kind = primec::Expr::Kind::Call;
  firstExpr.isMethodCall = true;
  firstExpr.semanticNodeId = 44;
  CHECK(primec::ir_lowerer::findSemanticProductMethodCallTarget(adapter, firstExpr) ==
        "/std/collections/map/contains");

  primec::Expr secondExpr;
  secondExpr.kind = primec::Expr::Kind::Call;
  secondExpr.isMethodCall = true;
  secondExpr.semanticNodeId = 45;
  CHECK(primec::ir_lowerer::findSemanticProductMethodCallTarget(adapter, secondExpr) ==
        "/std/collections/map/contains");
}

TEST_CASE("ir lowerer semantic-product adapter rejects call-target source lookup") {
  primec::SemanticProgram semanticProgram;
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "status",
      .sourceLine = 7,
      .sourceColumn = 9,
      .semanticNodeId = 155,
      .resolvedPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/std/file/FileError/status"),
      .stdlibSurfaceId = primec::StdlibSurfaceId::FileErrorHelpers,
  });
  semanticProgram.publishedRoutingLookups.directCallTargetIdsByExpr.insert_or_assign(
      155,
      primec::semanticProgramInternCallTargetString(
          semanticProgram, "/std/file/FileError/status"));
  semanticProgram.publishedRoutingLookups.directCallStdlibSurfaceIdsByExpr
      .insert_or_assign(155, primec::StdlibSurfaceId::FileErrorHelpers);
  semanticProgram.methodCallTargets.push_back(primec::SemanticProgramMethodCallTarget{
      .scopePath = "/main",
      .methodName = "at",
      .receiverTypeText = "/std/collections/vector/Vector__t25a78a513414c3bf",
      .sourceLine = 11,
      .sourceColumn = 15,
      .semanticNodeId = 244,
      .scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
      .methodNameId = primec::semanticProgramInternCallTargetString(semanticProgram, "at"),
      .receiverTypeTextId = primec::semanticProgramInternCallTargetString(
          semanticProgram, "/std/collections/vector/Vector__t25a78a513414c3bf"),
      .resolvedPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/std/collections/vector/at"),
      .stdlibSurfaceId = primec::StdlibSurfaceId::CollectionsManifestSurface0,
  });
  semanticProgram.publishedRoutingLookups.methodCallTargetIdsByExpr.insert_or_assign(
      244,
      primec::semanticProgramInternCallTargetString(
          semanticProgram, "/std/collections/vector/at"));
  semanticProgram.publishedRoutingLookups.methodCallStdlibSurfaceIdsByExpr
      .insert_or_assign(244, primec::StdlibSurfaceId::CollectionsManifestSurface0);

  const auto adapter = primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);

  primec::Expr directExpr;
  directExpr.kind = primec::Expr::Kind::Call;
  directExpr.name = "status";
  directExpr.sourceLine = 7;
  directExpr.sourceColumn = 9;
  directExpr.semanticNodeId = 0;

  CHECK(primec::ir_lowerer::findSemanticProductDirectCallTarget(adapter, directExpr) ==
        "");
  const auto directSurfaceId =
      primec::ir_lowerer::findSemanticProductDirectCallStdlibSurfaceId(adapter, directExpr);
  CHECK_FALSE(directSurfaceId.has_value());

  directExpr.semanticNodeId = 155;
  CHECK(primec::ir_lowerer::findSemanticProductDirectCallTarget(adapter, directExpr) ==
        "/std/file/FileError/status");
  const auto directSemanticSurfaceId =
      primec::ir_lowerer::findSemanticProductDirectCallStdlibSurfaceId(adapter, directExpr);
  REQUIRE(directSemanticSurfaceId.has_value());
  CHECK(*directSemanticSurfaceId == primec::StdlibSurfaceId::FileErrorHelpers);

  primec::Expr methodExpr;
  methodExpr.kind = primec::Expr::Kind::Call;
  methodExpr.isMethodCall = true;
  methodExpr.name = "at";
  methodExpr.sourceLine = 11;
  methodExpr.sourceColumn = 15;
  methodExpr.semanticNodeId = 0;

  CHECK(primec::ir_lowerer::findSemanticProductMethodCallTarget(adapter, methodExpr) ==
        "");
  const auto surfaceId =
      primec::ir_lowerer::findSemanticProductMethodCallStdlibSurfaceId(adapter, methodExpr);
  CHECK_FALSE(surfaceId.has_value());

  methodExpr.semanticNodeId = 244;
  CHECK(primec::ir_lowerer::findSemanticProductMethodCallTarget(adapter, methodExpr) ==
        "/std/collections/vector/at");
  const auto semanticSurfaceId =
      primec::ir_lowerer::findSemanticProductMethodCallStdlibSurfaceId(adapter, methodExpr);
  REQUIRE(semanticSurfaceId.has_value());
  CHECK(*semanticSurfaceId == primec::StdlibSurfaceId::CollectionsManifestSurface0);
}

TEST_CASE("ir lowerer semantic-product call-target context separates meaning from provenance") {
  primec::SemanticProgram semanticProgram;
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "callee",
      .sourceLine = 3,
      .sourceColumn = 5,
      .semanticNodeId = 77,
      .scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
      .callNameId = primec::semanticProgramInternCallTargetString(semanticProgram, "callee"),
      .resolvedPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/semantic/callee"),
      .stdlibSurfaceId = std::nullopt,
  });
  semanticProgram.publishedRoutingLookups.directCallTargetIdsByExpr.insert_or_assign(
      77,
      semanticProgram.directCallTargets.back().resolvedPathId);

  const auto adapter = primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);
  const primec::ir_lowerer::SemanticProductMeaningContext meaning{&adapter};

  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "syntax_callee";
  callExpr.sourceLine = 3;
  callExpr.sourceColumn = 5;
  callExpr.semanticNodeId = 77;

  primec::ir_lowerer::SemanticProductCallTarget target;
  std::string error;
  CHECK(primec::ir_lowerer::requireSemanticProductCallTarget(
      {
          .meaning = meaning,
          .syntax = {.scopePath = "/main", .expr = &callExpr},
      },
      primec::ir_lowerer::SemanticProductCallTargetKind::DirectCall,
      target,
      error));
  CHECK(target.resolvedPath == "/semantic/callee");
  CHECK_FALSE(target.stdlibSurfaceId.has_value());
  CHECK(error.empty());
  CHECK(primec::ir_lowerer::describeSyntaxCallSite({"/main", &callExpr}) ==
        "/main -> syntax_callee");
}

TEST_CASE("ir lowerer semantic-product call-target context fails closed on missing meaning") {
  primec::SemanticProgram semanticProgram;
  const auto adapter = primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);
  const primec::ir_lowerer::SemanticProductMeaningContext meaning{&adapter};

  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "callee";
  callExpr.sourceLine = 8;
  callExpr.sourceColumn = 13;
  callExpr.semanticNodeId = 91;

  primec::ir_lowerer::SemanticProductCallTarget target;
  std::string error;
  CHECK_FALSE(primec::ir_lowerer::requireSemanticProductCallTarget(
      {
          .meaning = meaning,
          .syntax = {.scopePath = "/main", .expr = &callExpr},
      },
      primec::ir_lowerer::SemanticProductCallTargetKind::DirectCall,
      target,
      error));
  CHECK(target.resolvedPath.empty());
  CHECK_FALSE(target.stdlibSurfaceId.has_value());
  CHECK(error == "missing semantic-product direct-call target: /main -> callee");
}

TEST_CASE("ir lowerer semantic-product adapter exposes published stdlib surface ids") {
  primec::SemanticProgram semanticProgram;
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "status",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 52,
      .resolvedPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/std/file/FileError/status"),
      .stdlibSurfaceId = primec::StdlibSurfaceId::FileErrorHelpers,
  });
  semanticProgram.methodCallTargets.push_back(primec::SemanticProgramMethodCallTarget{
      .scopePath = "/main",
      .methodName = "status",
      .receiverTypeText = "FileError",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 53,
      .scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
      .methodNameId = primec::semanticProgramInternCallTargetString(semanticProgram, "status"),
      .receiverTypeTextId = primec::semanticProgramInternCallTargetString(semanticProgram, "FileError"),
      .resolvedPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/FileError/status"),
      .stdlibSurfaceId = primec::StdlibSurfaceId::FileErrorHelpers,
  });
  semanticProgram.bridgePathChoices.push_back(primec::SemanticProgramBridgePathChoice{
      .scopePath = "/main",
      .collectionFamily = "map",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 54,
      .scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
      .collectionFamilyId = primec::semanticProgramInternCallTargetString(semanticProgram, "map"),
      .helperNameId = primec::semanticProgramInternCallTargetString(semanticProgram, "contains_ref"),
      .chosenPathId = primec::semanticProgramInternCallTargetString(
          semanticProgram, "/std/collections/experimental_map/mapContainsRef"),
      .stdlibSurfaceId = primec::findStdlibSurfaceMetadataByBridgeKey("collections.map_helpers")->id,
  });
  semanticProgram.publishedRoutingLookups.directCallStdlibSurfaceIdsByExpr.insert_or_assign(
      52, primec::StdlibSurfaceId::FileErrorHelpers);
  semanticProgram.publishedRoutingLookups.directCallTargetIdsByExpr.insert_or_assign(
      52,
      primec::semanticProgramInternCallTargetString(
          semanticProgram, "/std/file/FileError/status"));
  semanticProgram.publishedRoutingLookups.methodCallStdlibSurfaceIdsByExpr.insert_or_assign(
      53, primec::StdlibSurfaceId::FileErrorHelpers);
  semanticProgram.publishedRoutingLookups.methodCallTargetIdsByExpr.insert_or_assign(
      53,
      primec::semanticProgramInternCallTargetString(semanticProgram, "/FileError/status"));
  semanticProgram.publishedRoutingLookups.bridgePathChoiceStdlibSurfaceIdsByExpr.insert_or_assign(
      54, primec::findStdlibSurfaceMetadataByBridgeKey("collections.map_helpers")->id);

  const auto adapter = primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);
  REQUIRE(adapter.publishedRoutingLookups != nullptr);
  CHECK(adapter.publishedRoutingLookups->directCallStdlibSurfaceIdsByExpr.count(52) == 1);
  CHECK(adapter.publishedRoutingLookups->methodCallStdlibSurfaceIdsByExpr.count(53) == 1);
  CHECK(adapter.publishedRoutingLookups->bridgePathChoiceStdlibSurfaceIdsByExpr.count(54) == 1);
  CHECK(adapter.publishedRoutingLookups->directCallTargetIdsByExpr.count(52) == 1);
  CHECK(adapter.publishedRoutingLookups->methodCallTargetIdsByExpr.count(53) == 1);

  primec::Expr directExpr;
  directExpr.kind = primec::Expr::Kind::Call;
  directExpr.semanticNodeId = 52;
  const auto directSurfaceId =
      primec::ir_lowerer::findSemanticProductDirectCallStdlibSurfaceId(adapter, directExpr);
  REQUIRE(directSurfaceId.has_value());
  CHECK(*directSurfaceId == primec::StdlibSurfaceId::FileErrorHelpers);

  primec::Expr methodExpr;
  methodExpr.kind = primec::Expr::Kind::Call;
  methodExpr.isMethodCall = true;
  methodExpr.semanticNodeId = 53;
  const auto methodSurfaceId =
      primec::ir_lowerer::findSemanticProductMethodCallStdlibSurfaceId(adapter, methodExpr);
  REQUIRE(methodSurfaceId.has_value());
  CHECK(*methodSurfaceId == primec::StdlibSurfaceId::FileErrorHelpers);

  primec::Expr bridgeExpr;
  bridgeExpr.kind = primec::Expr::Kind::Call;
  bridgeExpr.semanticNodeId = 54;
  const auto bridgeSurfaceId =
      primec::ir_lowerer::findSemanticProductBridgePathChoiceStdlibSurfaceId(adapter, bridgeExpr);
  REQUIRE(bridgeSurfaceId.has_value());
  CHECK(*bridgeSurfaceId == primec::findStdlibSurfaceMetadataByBridgeKey("collections.map_helpers")->id);
}

TEST_CASE("ir lowerer semantic-product adapter ignores method-call targets missing resolved path ids") {
  primec::SemanticProgram semanticProgram;
  semanticProgram.methodCallTargets.push_back(primec::SemanticProgramMethodCallTarget{
      .scopePath = "/main",
      .methodName = "contains",
      .receiverTypeText = "",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 144,
      .scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
      .methodNameId = primec::semanticProgramInternCallTargetString(semanticProgram, "contains"),
      .resolvedPathId = primec::InvalidSymbolId,
      .stdlibSurfaceId = std::nullopt,
  });
  semanticProgram.methodCallTargets.push_back(primec::SemanticProgramMethodCallTarget{
      .scopePath = "/main",
      .methodName = "contains",
      .receiverTypeText = "",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 145,
      .scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
      .methodNameId = primec::semanticProgramInternCallTargetString(semanticProgram, "contains"),
      .resolvedPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram,
                                                        "/std/collections/map/contains"),
      .stdlibSurfaceId = std::nullopt,
  });
  semanticProgram.publishedRoutingLookups.methodCallTargetIdsByExpr.insert_or_assign(
      145,
      semanticProgram.methodCallTargets[1].resolvedPathId);

  const auto adapter = primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);
  REQUIRE(adapter.publishedRoutingLookups != nullptr);
  CHECK(adapter.publishedRoutingLookups->methodCallTargetIdsByExpr.count(144) == 0);
  CHECK(adapter.publishedRoutingLookups->methodCallTargetIdsByExpr.count(145) == 1);

  primec::Expr missingPathExpr;
  missingPathExpr.kind = primec::Expr::Kind::Call;
  missingPathExpr.isMethodCall = true;
  missingPathExpr.semanticNodeId = 144;
  CHECK(primec::ir_lowerer::findSemanticProductMethodCallTarget(adapter, missingPathExpr).empty());

  primec::Expr validExpr;
  validExpr.kind = primec::Expr::Kind::Call;
  validExpr.isMethodCall = true;
  validExpr.semanticNodeId = 145;
  CHECK(primec::ir_lowerer::findSemanticProductMethodCallTarget(adapter, validExpr) ==
        "/std/collections/map/contains");
}

TEST_CASE("ir lowerer semantic-product adapter indexes callable summaries by full-path id") {
  primec::SemanticProgram semanticProgram;
  const auto mainPathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/main");
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
      .isExecution = false,
      .returnKind = "void",
      .isCompute = false,
      .isUnsafe = false,
      .activeEffects = {},
      .activeCapabilities = {},
      .hasResultType = false,
      .resultTypeHasValue = false,
      .resultValueType = "",
      .resultErrorType = "",
      .hasOnError = false,
      .onErrorHandlerPath = "",
      .onErrorErrorType = "",
      .onErrorBoundArgCount = 0,
      .semanticNodeId = 201,
      .provenanceHandle = 0,
      .fullPathId = mainPathId,
  });
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
      .isExecution = false,
      .returnKind = "void",
      .isCompute = false,
      .isUnsafe = false,
      .activeEffects = {},
      .activeCapabilities = {},
      .hasResultType = false,
      .resultTypeHasValue = false,
      .resultValueType = "",
      .resultErrorType = "",
      .hasOnError = false,
      .onErrorHandlerPath = "",
      .onErrorErrorType = "",
      .onErrorBoundArgCount = 0,
      .semanticNodeId = 202,
      .provenanceHandle = 0,
      .fullPathId = primec::InvalidSymbolId,
  });
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
      .isExecution = false,
      .returnKind = "void",
      .isCompute = false,
      .isUnsafe = false,
      .activeEffects = {},
      .activeCapabilities = {},
      .hasResultType = false,
      .resultTypeHasValue = false,
      .resultValueType = "",
      .resultErrorType = "",
      .hasOnError = false,
      .onErrorHandlerPath = "",
      .onErrorErrorType = "",
      .onErrorBoundArgCount = 0,
      .semanticNodeId = 203,
      .provenanceHandle = 0,
      .fullPathId =
          static_cast<primec::SymbolId>(semanticProgram.callTargetStringTable.size() + 1u),
  });
  semanticProgram.publishedRoutingLookups.callableSummaryIndicesByPathId.insert_or_assign(mainPathId, 0);

  const auto adapter = primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);
  REQUIRE(adapter.publishedRoutingLookups != nullptr);
  CHECK(adapter.publishedRoutingLookups->callableSummaryIndicesByPathId.count(mainPathId) == 1);
  CHECK(adapter.publishedRoutingLookups->callableSummaryIndicesByPathId.count(primec::InvalidSymbolId) == 0);
  CHECK(adapter.publishedRoutingLookups->callableSummaryIndicesByPathId.count(
            static_cast<primec::SymbolId>(semanticProgram.callTargetStringTable.size() + 1u)) == 0);
  const auto *summary = primec::ir_lowerer::findSemanticProductCallableSummary(adapter, "/main");
  REQUIRE(summary != nullptr);
  CHECK(summary->semanticNodeId == 201);
  CHECK(primec::semanticProgramLookupPublishedCallableSummaryByPathId(
            semanticProgram, mainPathId) == summary);
  CHECK(primec::semanticProgramLookupPublishedCallableSummary(
            semanticProgram, "/main") == summary);
  CHECK(primec::ir_lowerer::findSemanticProductCallableSummary(adapter, "/missing") == nullptr);
}

TEST_CASE("ir lowerer callable summary helper ignores raw summaries without published lookup") {
  primec::SemanticProgram semanticProgram;
  const auto mainPathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/main");
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
      .isExecution = false,
      .returnKind = "i32",
      .isCompute = false,
      .isUnsafe = false,
      .activeEffects = {},
      .activeCapabilities = {},
      .hasResultType = false,
      .resultTypeHasValue = false,
      .resultValueType = "",
      .resultErrorType = "",
      .hasOnError = false,
      .onErrorHandlerPath = "",
      .onErrorErrorType = "",
      .onErrorBoundArgCount = 0,
      .semanticNodeId = 219,
      .provenanceHandle = 0,
      .fullPathId = mainPathId,
  });

  const auto adapter =
      primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);
  CHECK(primec::ir_lowerer::findSemanticProductCallableSummary(adapter, "/main") == nullptr);
  CHECK(primec::semanticProgramLookupPublishedCallableSummaryByPathId(
            semanticProgram, mainPathId) == nullptr);
  CHECK(primec::semanticProgramLookupPublishedCallableSummary(
            semanticProgram, "/main") == nullptr);
}

TEST_CASE("ir lowerer call helpers require semantic-product bridge-path choices") {
  const std::unordered_map<std::string, const primec::Definition *> defMap;
  const std::unordered_map<std::string, std::string> importAliases;

  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "count";
  callExpr.semanticNodeId = 18;

  primec::SemanticProgram semanticProgram;
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "count",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 18,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/vector/count"),
      .stdlibSurfaceId = std::nullopt,
  });
  semanticProgram.publishedRoutingLookups.directCallTargetIdsByExpr.insert_or_assign(
      18, primec::semanticProgramInternCallTargetString(semanticProgram, "/vector/count"));
  auto semanticResolveExprPath =
      primec::ir_lowerer::makeResolveCallPathFromScope(defMap, importAliases, &semanticProgram);
  CHECK(semanticResolveExprPath(callExpr) == "/vector/count");

  semanticProgram.bridgePathChoices.push_back(primec::SemanticProgramBridgePathChoice{
      .scopePath = "/main",
      .collectionFamily = "vector",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 18,
      .scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
      .collectionFamilyId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "vector"),
      .helperNameId = primec::semanticProgramInternCallTargetString(semanticProgram, "count"),
      .chosenPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/vector/count"),
      .stdlibSurfaceId = std::nullopt,
  });
  semanticProgram.publishedRoutingLookups.bridgePathChoiceIdsByExpr.insert_or_assign(
      18, primec::semanticProgramInternCallTargetString(semanticProgram, "/vector/count"));
  semanticResolveExprPath =
      primec::ir_lowerer::makeResolveCallPathFromScope(defMap, importAliases, &semanticProgram);
  CHECK(semanticResolveExprPath(callExpr) == "/vector/count");
}

TEST_CASE("ir lowerer semantic-product adapter ignores bridge-path choices with missing or invalid helper name ids") {
  primec::SemanticProgram semanticProgram;
  semanticProgram.bridgePathChoices.push_back(primec::SemanticProgramBridgePathChoice{
      .scopePath = "/main",
      .collectionFamily = "vector",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 118,
      .scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
      .collectionFamilyId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "vector"),
      .helperNameId = primec::InvalidSymbolId,
      .chosenPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/vector/count"),
      .stdlibSurfaceId = std::nullopt,
  });
  semanticProgram.bridgePathChoices.push_back(primec::SemanticProgramBridgePathChoice{
      .scopePath = "/main",
      .collectionFamily = "vector",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 119,
      .scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
      .collectionFamilyId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "vector"),
      .helperNameId = primec::semanticProgramInternCallTargetString(semanticProgram, "count"),
      .chosenPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/vector/count"),
      .stdlibSurfaceId = std::nullopt,
  });
  semanticProgram.publishedRoutingLookups.bridgePathChoiceIdsByExpr.insert_or_assign(
      119,
      semanticProgram.bridgePathChoices[1].chosenPathId);
  semanticProgram.bridgePathChoices.push_back(primec::SemanticProgramBridgePathChoice{
      .scopePath = "/main",
      .collectionFamily = "vector",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 120,
      .scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
      .collectionFamilyId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "vector"),
      .helperNameId =
          static_cast<primec::SymbolId>(semanticProgram.callTargetStringTable.size() + 1u),
      .chosenPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/vector/count"),
      .stdlibSurfaceId = std::nullopt,
  });

  const auto adapter = primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);
  REQUIRE(adapter.publishedRoutingLookups != nullptr);
  CHECK(adapter.publishedRoutingLookups->bridgePathChoiceIdsByExpr.count(118) == 0);
  CHECK(adapter.publishedRoutingLookups->bridgePathChoiceIdsByExpr.count(119) == 1);
  CHECK(adapter.publishedRoutingLookups->bridgePathChoiceIdsByExpr.count(120) == 0);

  primec::Expr missingHelperExpr;
  missingHelperExpr.kind = primec::Expr::Kind::Call;
  missingHelperExpr.semanticNodeId = 118;
  CHECK(primec::ir_lowerer::findSemanticProductBridgePathChoice(adapter, missingHelperExpr).empty());

  primec::Expr validExpr;
  validExpr.kind = primec::Expr::Kind::Call;
  validExpr.semanticNodeId = 119;
  CHECK(primec::ir_lowerer::findSemanticProductBridgePathChoice(adapter, validExpr) == "/vector/count");

  primec::Expr invalidHelperIdExpr;
  invalidHelperIdExpr.kind = primec::Expr::Kind::Call;
  invalidHelperIdExpr.semanticNodeId = 120;
  CHECK(primec::ir_lowerer::findSemanticProductBridgePathChoice(adapter, invalidHelperIdExpr).empty());
}

TEST_CASE("ir lowerer semantic-product adapter joins facts by semantic id without return-path fallback") {
  primec::Definition mainDef;
  mainDef.fullPath = "/renamed_main";
  mainDef.semanticNodeId = 61;

  primec::Expr localAutoExpr;
  localAutoExpr.kind = primec::Expr::Kind::Call;
  localAutoExpr.name = "select";
  localAutoExpr.semanticNodeId = 62;

  primec::Expr queryExpr;
  queryExpr.kind = primec::Expr::Kind::Call;
  queryExpr.name = "lookup";
  queryExpr.semanticNodeId = 63;

  primec::Expr tryExpr;
  tryExpr.kind = primec::Expr::Kind::Call;
  tryExpr.name = "try";
  tryExpr.semanticNodeId = 64;

  primec::SemanticProgram semanticProgram;
  semanticProgram.returnFacts.push_back(primec::SemanticProgramReturnFact{
      .returnKind = "return",
      .structPath = "/i32",
      .bindingTypeText = "i32",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 9,
      .sourceColumn = 3,
      .semanticNodeId = 61,
      .definitionPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/legacy_main"),
  });
  semanticProgram.publishedRoutingLookups.returnFactIndicesByDefinitionId
      .insert_or_assign(61, 0);
  semanticProgram.localAutoFacts.push_back(primec::SemanticProgramLocalAutoFact{
      .scopePath = "/main",
      .bindingName = "selected",
      .bindingTypeText = "i32",
      .initializerBindingTypeText = "i32",
      .initializerReceiverBindingTypeText = "",
      .initializerQueryTypeText = "i32",
      .initializerResultHasValue = false,
      .initializerResultValueType = "",
      .initializerResultErrorType = "",
      .initializerHasTry = false,
      .initializerTryOperandResolvedPath = "",
      .initializerTryOperandBindingTypeText = "",
      .initializerTryOperandReceiverBindingTypeText = "",
      .initializerTryOperandQueryTypeText = "",
      .initializerTryValueType = "",
      .initializerTryErrorType = "",
      .initializerTryContextReturnKind = "return",
      .initializerTryOnErrorHandlerPath = "",
      .initializerTryOnErrorErrorType = "",
      .initializerTryOnErrorBoundArgCount = 0,
      .sourceLine = 10,
      .sourceColumn = 5,
      .semanticNodeId = 62,
      .provenanceHandle = 0,
      .initializerDirectCallResolvedPath = "",
      .initializerDirectCallReturnKind = "",
      .initializerMethodCallResolvedPath = "",
      .initializerMethodCallReturnKind = "",
      .initializerStdlibSurfaceId = std::nullopt,
      .initializerDirectCallStdlibSurfaceId = std::nullopt,
      .initializerMethodCallStdlibSurfaceId = std::nullopt,
      .initializerResolvedPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/id"),
  });
  semanticProgram.publishedRoutingLookups.localAutoFactIndicesByExpr.insert_or_assign(62, 0);
  semanticProgram.queryFacts.push_back(primec::SemanticProgramQueryFact{
      .scopePath = "/main",
      .callName = "lookup",
      .queryTypeText = "Result<i32, FileError>",
      .bindingTypeText = "Result<i32, FileError>",
      .receiverBindingTypeText = "",
      .hasResultType = true,
      .resultTypeHasValue = true,
      .resultValueType = "i32",
      .resultErrorType = "FileError",
      .sourceLine = 11,
      .sourceColumn = 7,
      .semanticNodeId = 63,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/lookup"),
  });
  semanticProgram.publishedRoutingLookups.queryFactIndicesByExpr.insert_or_assign(63, 0);
  semanticProgram.tryFacts.push_back(primec::SemanticProgramTryFact{
      .scopePath = "/main",
      .operandBindingTypeText = "Result<i32, FileError>",
      .operandReceiverBindingTypeText = "",
      .operandQueryTypeText = "Result<i32, FileError>",
      .valueType = "i32",
      .errorType = "FileError",
      .contextReturnKind = "return",
      .onErrorHandlerPath = "/handler",
      .onErrorErrorType = "FileError",
      .onErrorBoundArgCount = 1,
      .sourceLine = 12,
      .sourceColumn = 9,
      .semanticNodeId = 64,
      .operandResolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/lookup"),
  });
  semanticProgram.publishedRoutingLookups.tryFactIndicesByExpr.insert_or_assign(64, 0);

  const auto semanticTargets =
      primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);

  const auto *returnFact = primec::ir_lowerer::findSemanticProductReturnFact(semanticTargets, mainDef);
  REQUIRE(returnFact != nullptr);
  CHECK(primec::semanticProgramReturnFactDefinitionPath(semanticProgram, *returnFact) ==
        "/legacy_main");

  primec::Definition legacyFixtureDef;
  legacyFixtureDef.fullPath = "/legacy_main";
  const auto *legacyReturnFact =
      primec::ir_lowerer::findSemanticProductReturnFact(semanticTargets, legacyFixtureDef);
  CHECK(legacyReturnFact == nullptr);

  const auto *localAutoFact = primec::ir_lowerer::findSemanticProductLocalAutoFact(semanticTargets, localAutoExpr);
  REQUIRE(localAutoFact != nullptr);
  CHECK(localAutoFact->bindingName == "selected");

  const auto *queryFact = primec::ir_lowerer::findSemanticProductQueryFact(semanticTargets, queryExpr);
  REQUIRE(queryFact != nullptr);
  CHECK(primec::semanticProgramQueryFactResolvedPath(semanticProgram, *queryFact) == "/lookup");

  const auto *tryFact = primec::ir_lowerer::findSemanticProductTryFact(semanticTargets, tryExpr);
  REQUIRE(tryFact != nullptr);
  CHECK(tryFact->onErrorHandlerPath == "/handler");
}

TEST_CASE("ir lowerer semantic-product adapter does not expose return path fallback") {
  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 0;

  primec::SemanticProgram semanticProgram;
  semanticProgram.returnFacts.push_back(primec::SemanticProgramReturnFact{
      .returnKind = "return",
      .structPath = "/i32",
      .bindingTypeText = "i32",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 9,
      .sourceColumn = 3,
      .semanticNodeId = 0,
      .definitionPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
  });

  const auto semanticTargets =
      primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);
  CHECK(semanticTargets.semanticIndex.returnFactsByDefinitionId.empty());
  const auto *returnFact = primec::ir_lowerer::findSemanticProductReturnFact(semanticTargets, mainDef);
  CHECK(returnFact == nullptr);
}

TEST_CASE("ir lowerer semantic-product index does not expose return path fallback") {
  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 0;

  primec::SemanticProgram semanticProgram;
  semanticProgram.returnFacts.push_back(primec::SemanticProgramReturnFact{
      .returnKind = "return",
      .structPath = "/i32",
      .bindingTypeText = "i32",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 9,
      .sourceColumn = 3,
      .semanticNodeId = 0,
      .definitionPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
  });

  const auto semanticIndex =
      primec::ir_lowerer::buildSemanticProductIndex(&semanticProgram);
  CHECK(semanticIndex.returnFactsByDefinitionId.empty());
  const auto *returnFact =
      primec::ir_lowerer::findSemanticProductReturnFact(
          &semanticProgram, semanticIndex, mainDef);
  CHECK(returnFact == nullptr);
}

TEST_CASE("ir lowerer semantic-product index requires published return definition-id maps") {
  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 7801;

  primec::SemanticProgram semanticProgram;
  semanticProgram.returnFacts.push_back(primec::SemanticProgramReturnFact{
      .returnKind = "return",
      .structPath = "/i32",
      .bindingTypeText = "i32",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 9,
      .sourceColumn = 3,
      .semanticNodeId = 7801,
      .definitionPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
  });

  const auto rawOnlySemanticIndex =
      primec::ir_lowerer::buildSemanticProductIndex(&semanticProgram);
  CHECK(rawOnlySemanticIndex.returnFactsByDefinitionId.empty());
  const auto *rawOnlyReturnFact =
      primec::ir_lowerer::findSemanticProductReturnFact(
          &semanticProgram, rawOnlySemanticIndex, mainDef);
  CHECK(rawOnlyReturnFact == nullptr);

  semanticProgram.publishedRoutingLookups.returnFactIndicesByDefinitionId
      .insert_or_assign(7801, 0);
  const auto semanticIndex =
      primec::ir_lowerer::buildSemanticProductIndex(&semanticProgram);
  CHECK(semanticIndex.returnFactsByDefinitionId.count(7801) == 1);
  const auto *returnFact =
      primec::ir_lowerer::findSemanticProductReturnFact(
          &semanticProgram, semanticIndex, mainDef);
  REQUIRE(returnFact != nullptr);
  CHECK(returnFact->bindingTypeText == "i32");
}

TEST_CASE("ir lowerer return-by-path helper uses published definition-path facts") {
  primec::SemanticProgram semanticProgram;
  const primec::SymbolId makeChoicePathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/makeChoice");
  semanticProgram.returnFacts.push_back(primec::SemanticProgramReturnFact{
      .returnKind = "return",
      .structPath = "/i32",
      .bindingTypeText = "i32",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 9,
      .sourceColumn = 3,
      .semanticNodeId = 7901,
      .definitionPathId = makeChoicePathId,
  });
  semanticProgram.returnFacts.push_back(primec::SemanticProgramReturnFact{
      .returnKind = "return",
      .structPath = "/Choice",
      .bindingTypeText = "Choice",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 10,
      .sourceColumn = 3,
      .semanticNodeId = 7902,
      .definitionPathId = makeChoicePathId,
  });
  semanticProgram.publishedRoutingLookups.returnFactIndicesByDefinitionPathId
      .insert_or_assign(makeChoicePathId, 1);

  const auto semanticTargets =
      primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);
  const auto *returnFact =
      primec::ir_lowerer::findSemanticProductReturnFactByPath(
          semanticTargets, "/makeChoice");
  REQUIRE(returnFact != nullptr);
  CHECK(returnFact->semanticNodeId == 7902);
  CHECK(returnFact->bindingTypeText == "Choice");
}

TEST_CASE("ir lowerer return-by-path helper ignores raw path facts without published lookup") {
  primec::SemanticProgram semanticProgram;
  semanticProgram.returnFacts.push_back(primec::SemanticProgramReturnFact{
      .returnKind = "return",
      .structPath = "/Choice",
      .bindingTypeText = "Choice",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 10,
      .sourceColumn = 3,
      .semanticNodeId = 7903,
      .definitionPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/makeChoice"),
  });

  const auto semanticTargets =
      primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);
  const auto *returnFact =
      primec::ir_lowerer::findSemanticProductReturnFactByPath(
          semanticTargets, "/makeChoice");
  CHECK(returnFact == nullptr);
}

TEST_CASE("ir lowerer sum metadata helpers use published lookup maps") {
  primec::SemanticProgram semanticProgram;
  const primec::SymbolId choicePathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/Choice");
  const primec::SymbolId rightNameId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "right");

  semanticProgram.sumTypeMetadata.push_back(primec::SemanticProgramSumTypeMetadata{
      .fullPath = "/Choice",
      .isPublic = false,
      .activeTagTypeText = "u32",
      .payloadStorageText = "array",
      .variantCount = 1,
      .semanticNodeId = 8101,
  });
  semanticProgram.sumTypeMetadata.push_back(primec::SemanticProgramSumTypeMetadata{
      .fullPath = "/Choice",
      .isPublic = false,
      .activeTagTypeText = "u32",
      .payloadStorageText = "array",
      .variantCount = 2,
      .semanticNodeId = 8102,
  });
  semanticProgram.publishedRoutingLookups.sumTypeMetadataIndicesByPathId
      .insert_or_assign(choicePathId, 1);

  semanticProgram.sumVariantMetadata.push_back(primec::SemanticProgramSumVariantMetadata{
      .sumPath = "/Choice",
      .variantName = "right",
      .variantIndex = 0,
      .tagValue = 0,
      .hasPayload = false,
      .payloadTypeText = "",
      .semanticNodeId = 8201,
  });
  semanticProgram.sumVariantMetadata.push_back(primec::SemanticProgramSumVariantMetadata{
      .sumPath = "/Choice",
      .variantName = "right",
      .variantIndex = 1,
      .tagValue = 17,
      .hasPayload = true,
      .payloadTypeText = "i32",
      .semanticNodeId = 8202,
  });
  const uint64_t rightKey =
      (static_cast<uint64_t>(choicePathId) << 32) |
      static_cast<uint64_t>(rightNameId);
  semanticProgram.publishedRoutingLookups
      .sumVariantMetadataIndicesBySumPathAndVariantNameId.insert_or_assign(rightKey, 1);

  const auto semanticTargets =
      primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);
  const auto *sumMetadata =
      primec::ir_lowerer::findSemanticProductSumTypeMetadata(
          semanticTargets, "/Choice");
  REQUIRE(sumMetadata != nullptr);
  CHECK(sumMetadata->semanticNodeId == 8102);
  CHECK(sumMetadata->variantCount == 2);

  const auto *variantMetadata =
      primec::ir_lowerer::findSemanticProductSumVariantMetadata(
          semanticTargets, "/Choice", "right");
  REQUIRE(variantMetadata != nullptr);
  CHECK(variantMetadata->semanticNodeId == 8202);
  CHECK(variantMetadata->tagValue == 17);
  CHECK(variantMetadata->payloadTypeText == "i32");
}

TEST_CASE("ir lowerer sum metadata helpers ignore raw metadata without published lookup") {
  primec::SemanticProgram semanticProgram;
  semanticProgram.sumTypeMetadata.push_back(primec::SemanticProgramSumTypeMetadata{
      .fullPath = "/Choice",
      .isPublic = false,
      .activeTagTypeText = "u32",
      .payloadStorageText = "array",
      .variantCount = 2,
      .semanticNodeId = 8301,
  });
  semanticProgram.sumVariantMetadata.push_back(primec::SemanticProgramSumVariantMetadata{
      .sumPath = "/Choice",
      .variantName = "right",
      .variantIndex = 1,
      .tagValue = 1,
      .hasPayload = true,
      .payloadTypeText = "i32",
      .semanticNodeId = 8302,
  });
  primec::semanticProgramInternCallTargetString(semanticProgram, "/Choice");
  primec::semanticProgramInternCallTargetString(semanticProgram, "right");

  const auto semanticTargets =
      primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);
  CHECK(primec::ir_lowerer::findSemanticProductSumTypeMetadata(
            semanticTargets, "/Choice") == nullptr);
  CHECK(primec::ir_lowerer::findSemanticProductSumVariantMetadata(
            semanticTargets, "/Choice", "right") == nullptr);
}

TEST_CASE("ir lowerer semantic-product adapter ignores on_error definition-path fallback") {
  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 0;

  primec::SemanticProgram semanticProgram;
  semanticProgram.onErrorFacts.push_back(primec::SemanticProgramOnErrorFact{
      .definitionPath = "/main",
      .returnKind = "return",
      .errorType = "FileError",
      .boundArgCount = 1,
      .boundArgTexts = {"value"},
      .returnResultHasValue = false,
      .returnResultValueType = "",
      .returnResultErrorType = "",
      .semanticNodeId = 0,
      .definitionPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
      .returnKindId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "return"),
      .handlerPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/handler"),
      .errorTypeId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "FileError"),
      .boundArgTextIds = {
          primec::semanticProgramInternCallTargetString(semanticProgram, "value"),
      },
  });
  const auto mainPathId =
      primec::semanticProgramLookupCallTargetStringId(semanticProgram, "/main");
  REQUIRE(mainPathId.has_value());
  semanticProgram.publishedRoutingLookups.onErrorFactIndicesByDefinitionPathId.insert_or_assign(
      *mainPathId, 0);

  const auto semanticTargets =
      primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);
  CHECK(semanticTargets.semanticIndex.onErrorFactsByDefinitionId.empty());
  const auto *onErrorFact = primec::ir_lowerer::findSemanticProductOnErrorFact(semanticTargets, mainDef);
  CHECK(onErrorFact == nullptr);
}

TEST_SUITE_END();
