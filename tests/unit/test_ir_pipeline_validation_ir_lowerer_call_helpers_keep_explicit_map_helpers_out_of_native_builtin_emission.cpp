#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir lowerer call helpers keep explicit map helpers out of native builtin emission") {
  using Result = primec::ir_lowerer::NativeCallTailDispatchResult;
  using LocalInfo = primec::ir_lowerer::LocalInfo;
  using MapAccessTargetInfo = primec::ir_lowerer::MapAccessTargetInfo;
  using ArrayVectorAccessTargetInfo = primec::ir_lowerer::ArrayVectorAccessTargetInfo;

  primec::ir_lowerer::LocalMap locals;
  LocalInfo mapInfo;
  mapInfo.kind = LocalInfo::Kind::Map;
  mapInfo.index = 7;
  mapInfo.mapKeyKind = LocalInfo::ValueKind::Int32;
  mapInfo.mapValueKind = LocalInfo::ValueKind::Int64;
  locals.emplace("items", mapInfo);

  LocalInfo keyInfo;
  keyInfo.kind = LocalInfo::Kind::Value;
  keyInfo.index = 9;
  keyInfo.valueKind = LocalInfo::ValueKind::Int32;
  locals.emplace("key", keyInfo);

  primec::Expr mapName;
  mapName.kind = primec::Expr::Kind::Name;
  mapName.name = "items";

  primec::Expr keyName;
  keyName.kind = primec::Expr::Kind::Name;
  keyName.name = "key";

  std::vector<primec::IrInstruction> instructions;
  auto emitInstruction = [&](primec::IrOpcode op, uint64_t imm) {
    instructions.push_back({op, imm});
  };
  auto instructionCount = [&]() { return instructions.size(); };
  auto patchInstructionImm = [&](size_t index, uint64_t imm) { instructions.at(index).imm = imm; };
  auto emitExpr = [&](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &emitLocals) {
    if (expr.kind != primec::Expr::Kind::Name) {
      return false;
    }
    auto it = emitLocals.find(expr.name);
    if (it == emitLocals.end()) {
      return false;
    }
    emitInstruction(primec::IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index));
    return true;
  };
  auto resolveMapAccessTargetInfo =
      [&](const primec::Expr &targetExpr, MapAccessTargetInfo &out) {
        if (targetExpr.kind != primec::Expr::Kind::Name || targetExpr.name != "items") {
          return false;
        }
        out.isMapTarget = true;
        out.mapKeyKind = LocalInfo::ValueKind::Int32;
        out.mapValueKind = LocalInfo::ValueKind::Int64;
        return true;
      };
  auto resolveArrayVectorAccessTargetInfo =
      [](const primec::Expr &, ArrayVectorAccessTargetInfo &) { return false; };

  auto expectDispatch = [&](const char *helperName,
                            const std::vector<primec::Expr> &args,
                            Result expectedResult,
                            const std::string &expectedError) {
    primec::Expr callExpr;
    callExpr.kind = primec::Expr::Kind::Call;
    callExpr.name = helperName;
    callExpr.args = args;

    instructions.clear();
    std::string error = "stale";
    CHECK(primec::ir_lowerer::tryEmitNativeCallTailDispatch(
              callExpr,
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
              emitExpr,
              resolveMapAccessTargetInfo,
              resolveArrayVectorAccessTargetInfo,
              [](const primec::Expr &, std::string &) { return false; },
              [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
                return LocalInfo::ValueKind::Unknown;
              },
              []() { return 0; },
              []() {},
              []() {},
              []() {},
              instructionCount,
              emitInstruction,
              patchInstructionImm,
              error) == expectedResult);
    CHECK(error == expectedError);
    if (expectedResult == Result::NotHandled) {
      CHECK(instructions.empty());
    } else {
      CHECK_FALSE(instructions.empty());
    }
  };
  auto expectNamespacedDispatch = [&](const char *namespacePrefix,
                                      const char *helperName,
                                      const std::vector<primec::Expr> &args,
                                      Result expectedResult,
                                      const std::string &expectedError) {
    primec::Expr callExpr;
    callExpr.kind = primec::Expr::Kind::Call;
    callExpr.namespacePrefix = namespacePrefix;
    callExpr.name = helperName;
    callExpr.args = args;

    instructions.clear();
    std::string error = "stale";
    CHECK(primec::ir_lowerer::tryEmitNativeCallTailDispatch(
              callExpr,
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
              emitExpr,
              resolveMapAccessTargetInfo,
              resolveArrayVectorAccessTargetInfo,
              [](const primec::Expr &, std::string &) { return false; },
              [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
                return LocalInfo::ValueKind::Unknown;
              },
              []() { return 0; },
              []() {},
              []() {},
              []() {},
              instructionCount,
              emitInstruction,
              patchInstructionImm,
              error) == expectedResult);
    CHECK(error == expectedError);
    if (expectedResult == Result::NotHandled) {
      CHECK(instructions.empty());
    } else {
      CHECK_FALSE(instructions.empty());
    }
  };

  expectDispatch("/map/count", {mapName}, Result::Emitted, "stale");
  expectDispatch("/std/collections/map/count", {mapName}, Result::Emitted, "stale");
  expectDispatch("/map/contains", {mapName, keyName}, Result::NotHandled, "stale");
  expectDispatch("/std/collections/map/contains",
                 {mapName, keyName},
                 Result::NotHandled,
                 "stale");
  expectDispatch("/map/tryAt", {mapName, keyName}, Result::NotHandled, "stale");
  expectDispatch("/std/collections/map/tryAt",
                 {mapName, keyName},
                 Result::NotHandled,
                 "stale");
  expectDispatch("/map/at", {mapName, keyName}, Result::NotHandled, "stale");
  expectDispatch("/std/collections/map/at",
                 {mapName, keyName},
                 Result::NotHandled,
                 "stale");
  expectDispatch("/map/at_unsafe",
                 {mapName, keyName},
                 Result::NotHandled,
                 "stale");
  expectDispatch("/std/collections/map/at_unsafe",
                 {mapName, keyName},
                 Result::NotHandled,
                 "stale");
  expectNamespacedDispatch("/std/collections/map",
                           "count",
                           {mapName},
                           Result::Emitted,
                           "stale");
  expectNamespacedDispatch("/std/collections/map",
                           "contains",
                           {mapName, keyName},
                           Result::NotHandled,
                           "stale");
  expectNamespacedDispatch("/std/collections/map",
                           "tryAt",
                           {mapName, keyName},
                           Result::NotHandled,
                           "stale");

  primec::SemanticProgram semanticProgram;
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "contains",
      .sourceLine = 12,
      .sourceColumn = 3,
      .semanticNodeId = 91,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(
          semanticProgram, "/std/collections/mapContains__ov1"),
      .stdlibSurfaceId = primec::StdlibSurfaceId::CollectionsMapHelpers,
  });
  semanticProgram.publishedRoutingLookups.directCallTargetIdsByExpr.insert_or_assign(
      91, semanticProgram.directCallTargets.front().resolvedPathId);
  semanticProgram.publishedRoutingLookups.directCallStdlibSurfaceIdsByExpr.insert_or_assign(
      91, primec::StdlibSurfaceId::CollectionsMapHelpers);

  primec::Expr semanticContainsCall;
  semanticContainsCall.kind = primec::Expr::Kind::Call;
  semanticContainsCall.name = "/std/collections/mapContains__ov1";
  semanticContainsCall.semanticNodeId = 91;
  semanticContainsCall.args = {mapName, keyName};

  instructions.clear();
  std::string semanticError = "stale";
  CHECK(primec::ir_lowerer::tryEmitNativeCallTailDispatch(
            semanticContainsCall,
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
            emitExpr,
            resolveMapAccessTargetInfo,
            resolveArrayVectorAccessTargetInfo,
            [](const primec::Expr &, std::string &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return LocalInfo::ValueKind::Unknown;
            },
            []() { return 0; },
            []() {},
            []() {},
            []() {},
            instructionCount,
            emitInstruction,
            patchInstructionImm,
            semanticError,
            &semanticProgram) == Result::NotHandled);
  CHECK(semanticError == "stale");
  CHECK(instructions.empty());
}

TEST_CASE("ir lowerer call helpers emit local vector count capacity calls through native tail dispatch") {
  using Result = primec::ir_lowerer::NativeCallTailDispatchResult;
  using LocalInfo = primec::ir_lowerer::LocalInfo;
  using MapAccessTargetInfo = primec::ir_lowerer::MapAccessTargetInfo;
  using ArrayVectorAccessTargetInfo = primec::ir_lowerer::ArrayVectorAccessTargetInfo;

  primec::ir_lowerer::LocalMap locals;
  LocalInfo vectorInfo;
  vectorInfo.kind = LocalInfo::Kind::Vector;
  vectorInfo.index = 7;
  vectorInfo.valueKind = LocalInfo::ValueKind::Int32;
  locals.emplace("values", vectorInfo);

  LocalInfo indexInfo;
  indexInfo.kind = LocalInfo::Kind::Value;
  indexInfo.index = 9;
  indexInfo.valueKind = LocalInfo::ValueKind::Int32;
  locals.emplace("index", indexInfo);

  primec::Expr valuesName;
  valuesName.kind = primec::Expr::Kind::Name;
  valuesName.name = "values";

  primec::Expr indexName;
  indexName.kind = primec::Expr::Kind::Name;
  indexName.name = "index";

  std::vector<primec::IrInstruction> instructions;
  auto emitInstruction = [&](primec::IrOpcode op, uint64_t imm) {
    instructions.push_back({op, imm});
  };
  auto instructionCount = [&]() { return instructions.size(); };
  auto patchInstructionImm = [&](size_t index, uint64_t imm) { instructions.at(index).imm = imm; };
  auto emitExpr = [&](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &emitLocals) {
    if (expr.kind != primec::Expr::Kind::Name) {
      return false;
    }
    auto it = emitLocals.find(expr.name);
    if (it == emitLocals.end()) {
      return false;
    }
    emitInstruction(primec::IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index));
    return true;
  };
  auto resolveMapAccessTargetInfo = [](const primec::Expr &, MapAccessTargetInfo &) { return false; };
  auto resolveArrayVectorAccessTargetInfo =
      [](const primec::Expr &targetExpr, ArrayVectorAccessTargetInfo &out) {
        if (targetExpr.kind != primec::Expr::Kind::Name || targetExpr.name != "values") {
          return false;
        }
        out.isArrayOrVectorTarget = true;
        out.isVectorTarget = true;
        out.elemKind = LocalInfo::ValueKind::Int32;
        return true;
      };

  auto expectDispatch = [&](const char *helperName,
                            const std::vector<primec::Expr> &args,
                            Result expectedResult,
                            const std::string &expectedError,
                            bool expectInstructions) {
    primec::Expr callExpr;
    callExpr.kind = primec::Expr::Kind::Call;
    callExpr.name = helperName;
    callExpr.args = args;

    instructions.clear();
    std::string error = "stale";
    CHECK(primec::ir_lowerer::tryEmitNativeCallTailDispatch(
              callExpr,
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
              emitExpr,
              resolveMapAccessTargetInfo,
              resolveArrayVectorAccessTargetInfo,
              [](const primec::Expr &, std::string &) { return false; },
              [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
                return LocalInfo::ValueKind::Unknown;
              },
              []() { return 0; },
              []() {},
              []() {},
              []() {},
              instructionCount,
              emitInstruction,
              patchInstructionImm,
              error) == expectedResult);
    CHECK(error == expectedError);
    CHECK(instructions.empty() != expectInstructions);
  };

  expectDispatch("/vector/count",
                 {valuesName},
                 Result::NotHandled,
                 "stale",
                 false);
  expectDispatch("/std/collections/vector/count",
                 {valuesName},
                 Result::Emitted,
                 "stale",
                 true);
  expectDispatch("/vector/capacity", {valuesName}, Result::NotHandled, "stale", false);
  expectDispatch("/std/collections/vector/capacity",
                 {valuesName},
                 Result::Emitted,
                 "stale",
                 true);
  expectDispatch("/vector/at",
                 {valuesName, indexName},
                 Result::NotHandled,
                 "stale",
                 false);
  expectDispatch("/std/collections/vector/at",
                 {valuesName, indexName},
                 Result::NotHandled,
                 "stale",
                 false);
  expectDispatch("/vector/at_unsafe",
                 {valuesName, indexName},
                 Result::NotHandled,
                 "stale",
                 false);
  expectDispatch("/std/collections/vector/at_unsafe",
                 {valuesName, indexName},
                 Result::NotHandled,
                 "stale",
                 false);

  primec::Expr namedSecondValue;
  namedSecondValue.kind = primec::Expr::Kind::Name;
  namedSecondValue.name = "secondValue";

  primec::Expr namedFirstValue;
  namedFirstValue.kind = primec::Expr::Kind::Name;
  namedFirstValue.name = "firstValue";

  primec::Expr namedVectorTemporary;
  namedVectorTemporary.kind = primec::Expr::Kind::Call;
  namedVectorTemporary.name = "vector";
  namedVectorTemporary.namespacePrefix = "/std/collections/vector";
  namedVectorTemporary.templateArgs = {"i32"};
  namedVectorTemporary.args = {namedSecondValue, namedFirstValue};
  namedVectorTemporary.argNames = {std::optional<std::string>{"second"},
                                   std::optional<std::string>{"first"}};

  expectDispatch("/std/collections/vector/count",
                 {namedVectorTemporary},
                 Result::Error,
                 "count requires array, vector, map, or string target",
                 false);

  primec::Expr bareCountCall;
  bareCountCall.kind = primec::Expr::Kind::Call;
  bareCountCall.name = "count";
  bareCountCall.args = {valuesName};
  instructions.clear();
  std::string bareCountError = "stale";
  CHECK(primec::ir_lowerer::tryEmitNativeCallTailDispatch(
            bareCountCall,
            locals,
            [](const primec::Expr &, std::string &) { return false; },
            [](const std::string &) { return true; },
            [](const primec::Expr &candidate, const primec::ir_lowerer::LocalMap &candidateLocals) {
              return primec::ir_lowerer::isArrayCountCall(candidate, candidateLocals, false, "argv");
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) {
              return false;
            },
            emitExpr,
            resolveMapAccessTargetInfo,
            resolveArrayVectorAccessTargetInfo,
            [](const primec::Expr &, std::string &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return LocalInfo::ValueKind::Unknown;
            },
            []() { return 0; },
            []() {},
            []() {},
            []() {},
            instructionCount,
            emitInstruction,
            patchInstructionImm,
            bareCountError) == Result::Emitted);
  CHECK(bareCountError == "stale");
  CHECK_FALSE(instructions.empty());

  primec::Expr bareCapacityCall;
  bareCapacityCall.kind = primec::Expr::Kind::Call;
  bareCapacityCall.name = "capacity";
  bareCapacityCall.args = {valuesName};
  instructions.clear();
  std::string bareCapacityError = "stale";
  CHECK(primec::ir_lowerer::tryEmitNativeCallTailDispatch(
            bareCapacityCall,
            locals,
            [](const primec::Expr &, std::string &) { return false; },
            [](const std::string &) { return true; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &candidate, const primec::ir_lowerer::LocalMap &candidateLocals) {
              return primec::ir_lowerer::isVectorCapacityCall(candidate, candidateLocals);
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) {
              return false;
            },
            emitExpr,
            resolveMapAccessTargetInfo,
            resolveArrayVectorAccessTargetInfo,
            [](const primec::Expr &, std::string &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return LocalInfo::ValueKind::Unknown;
            },
            []() { return 0; },
            []() {},
            []() {},
            []() {},
            instructionCount,
            emitInstruction,
            patchInstructionImm,
            bareCapacityError) == Result::NotHandled);
  CHECK(bareCapacityError == "stale");
  CHECK(instructions.empty());
}

TEST_SUITE_END();
