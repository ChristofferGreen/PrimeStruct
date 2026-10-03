#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir lowerer count access helpers classify canonical counts and defer vector reads") {
  primec::ir_lowerer::LocalMap vectorLocals;
  primec::ir_lowerer::LocalInfo vectorInfo;
  vectorInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Vector;
  vectorLocals.emplace("values", vectorInfo);

  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.args.resize(1);
  callExpr.args.front().kind = primec::Expr::Kind::Name;
  callExpr.args.front().name = "argv";

  callExpr.name = "count";
  CHECK(primec::ir_lowerer::isArrayCountCall(callExpr, vectorLocals, true, "argv"));

  callExpr.name = "/std/collections/vector/count";
  CHECK_FALSE(primec::ir_lowerer::isArrayCountCall(callExpr, vectorLocals, true, "argv"));
  callExpr.name = "/std/collections/vector/count__ti32";
  CHECK_FALSE(primec::ir_lowerer::isArrayCountCall(callExpr, vectorLocals, true, "argv"));

  // TODO-5247: a bare `count(argv)` call's namespacePrefix reflects the
  // enclosing definition's namespace context (populated for every call
  // parsed inside any `namespace` block), not call-site qualification -
  // it still resolves via the entry-args target the same as the unprefixed
  // case above. Only field-access/method receivers keep the stricter,
  // namespace-gated classification (see isUnqualifiedCollectionBuiltinName
  // in IrLowererCountAccessClassifiers.cpp).
  callExpr.name = "count";
  callExpr.namespacePrefix = "/std/collections/vector";
  CHECK(primec::ir_lowerer::isArrayCountCall(callExpr, vectorLocals, true, "argv"));
  callExpr.isMethodCall = true;
  CHECK_FALSE(primec::ir_lowerer::isArrayCountCall(callExpr, vectorLocals, true, "argv"));
  callExpr.isMethodCall = false;

  primec::Expr vectorTemporary;
  vectorTemporary.kind = primec::Expr::Kind::Call;
  vectorTemporary.name = "/std/collections/vector/vector";
  vectorTemporary.templateArgs = {"i32"};
  callExpr.args = {vectorTemporary};
  callExpr.name = "/std/collections/vector/capacity";
  callExpr.namespacePrefix.clear();
  CHECK_FALSE(primec::ir_lowerer::isVectorCapacityCall(callExpr, vectorLocals));
  callExpr.name = "/std/collections/vector/capacity__ti32";
  CHECK_FALSE(primec::ir_lowerer::isVectorCapacityCall(callExpr, vectorLocals));

  std::string accessName;
  callExpr.args.resize(2);
  callExpr.args.back().kind = primec::Expr::Kind::Literal;
  callExpr.args.back().literalValue = 0;

  callExpr.name = "/std/collections/vector/at";
  CHECK_FALSE(primec::ir_lowerer::getBuiltinArrayAccessName(callExpr, accessName));
  callExpr.name = "/std/collections/vector/at__ti32";
  CHECK_FALSE(primec::ir_lowerer::getBuiltinArrayAccessName(callExpr, accessName));

  callExpr.name = "at_unsafe";
  callExpr.namespacePrefix = "/std/collections/vector";
  CHECK_FALSE(primec::ir_lowerer::getBuiltinArrayAccessName(callExpr, accessName));
  callExpr.isMethodCall = true;
  CHECK_FALSE(primec::ir_lowerer::getBuiltinArrayAccessName(callExpr, accessName));
  callExpr.isMethodCall = false;
  callExpr.name = "/std/collections/vector/at_unsafe__ti32";
  callExpr.namespacePrefix.clear();
  CHECK_FALSE(primec::ir_lowerer::getBuiltinArrayAccessName(callExpr, accessName));

  callExpr.namespacePrefix.clear();
  callExpr.name = "/std/collections/vector/at";
  CHECK_FALSE(primec::ir_lowerer::getBuiltinArrayAccessName(callExpr, accessName));

  callExpr.name = "/std/collections/experimental_vector/vectorAtUnsafe";
  CHECK(primec::ir_lowerer::getBuiltinArrayAccessName(callExpr, accessName));
  CHECK(accessName == "at_unsafe");
}

TEST_CASE("ir lowerer count access helpers normalize parser-shaped canonical map access receivers") {
  using Kind = primec::ir_lowerer::LocalInfo::ValueKind;
  using Result = primec::ir_lowerer::CountAccessCallEmitResult;

  primec::Expr valuesName;
  valuesName.kind = primec::Expr::Kind::Name;
  valuesName.name = "values";

  primec::Expr keyExpr;
  keyExpr.kind = primec::Expr::Kind::Literal;
  keyExpr.literalValue = 1;

  primec::Expr targetExpr;
  targetExpr.kind = primec::Expr::Kind::Call;
  targetExpr.name = "at";
  targetExpr.namespacePrefix = "/std/collections/map";
  targetExpr.args = {valuesName, keyExpr};

  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "/std/collections/map/count";
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
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return Kind::Int32; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) {
              return false;
            },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              ++emitExprCalls;
              instructions.push_back({primec::IrOpcode::PushI64, 9});
              return true;
            },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            error) == Result::Emitted);
  CHECK(error.empty());
  CHECK(emitExprCalls == 1);
  REQUIRE(instructions.size() == 2);
  CHECK(instructions[0].op == primec::IrOpcode::PushI64);
  CHECK(instructions[1].op == primec::IrOpcode::LoadIndirect);

  instructions.clear();
  error.clear();
  emitExprCalls = 0;
  CHECK(primec::ir_lowerer::tryEmitCountAccessCall(
            callExpr,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return Kind::Int32; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) {
              return false;
            },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              ++emitExprCalls;
              instructions.push_back({primec::IrOpcode::PushI64, 11});
              return true;
            },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            error) == Result::NotHandled);
  CHECK(error.empty());
  CHECK(emitExprCalls == 0);
  CHECK(instructions.empty());

  primec::ir_lowerer::LocalMap staleStringMapLocals;
  primec::ir_lowerer::LocalInfo staleStringMapInfo;
  staleStringMapInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  staleStringMapInfo.keyValueKeyKind = Kind::Int32;
  staleStringMapInfo.keyValueValueKind = Kind::String;
  staleStringMapLocals.emplace("values", staleStringMapInfo);

  instructions.clear();
  error.clear();
  emitExprCalls = 0;
  CHECK(primec::ir_lowerer::tryEmitCountAccessCall(
            callExpr,
            staleStringMapLocals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return Kind::Int32; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) {
              return false;
            },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              ++emitExprCalls;
              instructions.push_back({primec::IrOpcode::PushI64, 13});
              return true;
            },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            error) == Result::NotHandled);
  CHECK(error.empty());
  CHECK(emitExprCalls == 0);
  CHECK(instructions.empty());

  instructions.clear();
  error.clear();
  emitExprCalls = 0;
  CHECK(primec::ir_lowerer::tryEmitCountAccessCall(
            callExpr,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return Kind::String; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) {
              return false;
            },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              ++emitExprCalls;
              instructions.push_back({primec::IrOpcode::PushI64, 17});
              return true;
            },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            error) == Result::Emitted);
  CHECK(error.empty());
  CHECK(emitExprCalls == 1);
  REQUIRE(instructions.size() == 2);
  CHECK(instructions[0].op == primec::IrOpcode::PushI64);
  CHECK(instructions[0].imm == 17);
  CHECK(instructions[1].op == primec::IrOpcode::LoadStringLength);
}

TEST_CASE("ir lowerer count access helpers prefer graph facts for runtime string names") {
  using Kind = primec::ir_lowerer::LocalInfo::ValueKind;
  using Result = primec::ir_lowerer::CountAccessCallEmitResult;

  primec::Expr valueName;
  valueName.kind = primec::Expr::Kind::Name;
  valueName.name = "value";

  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "/std/collections/map/count";
  callExpr.args = {valueName};

  primec::ir_lowerer::LocalMap staleStringLocals;
  primec::ir_lowerer::LocalInfo staleStringInfo;
  staleStringInfo.valueKind = Kind::String;
  staleStringLocals.emplace("value", staleStringInfo);

  std::vector<primec::IrInstruction> instructions;
  std::string error;
  int emitExprCalls = 0;

  CHECK(primec::ir_lowerer::tryEmitCountAccessCall(
            callExpr,
            staleStringLocals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return Kind::Int32; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) {
              return false;
            },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              ++emitExprCalls;
              instructions.push_back({primec::IrOpcode::PushI64, 19});
              return true;
            },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            error) == Result::NotHandled);
  CHECK(error.empty());
  CHECK(emitExprCalls == 0);
  CHECK(instructions.empty());

  instructions.clear();
  error.clear();
  emitExprCalls = 0;
  CHECK(primec::ir_lowerer::tryEmitCountAccessCall(
            callExpr,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return Kind::String; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) {
              return false;
            },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              ++emitExprCalls;
              instructions.push_back({primec::IrOpcode::PushI64, 23});
              return true;
            },
            [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
            error) == Result::Emitted);
  CHECK(error.empty());
  CHECK(emitExprCalls == 1);
  REQUIRE(instructions.size() == 2);
  CHECK(instructions[0].op == primec::IrOpcode::PushI64);
  CHECK(instructions[0].imm == 23);
  CHECK(instructions[1].op == primec::IrOpcode::LoadStringLength);
}

TEST_CASE("ir lowerer count access helpers resolve string map access emission by semantic fact kind") {
  using Kind = primec::ir_lowerer::LocalInfo::ValueKind;
  using Result = primec::ir_lowerer::CountAccessCallEmitResult;

  auto intern = [](primec::SemanticProgram &semanticProgram,
                   const std::string &text) {
    return primec::semanticProgramInternCallTargetString(semanticProgram, text);
  };
  auto addBindingFact = [&](primec::SemanticProgram &semanticProgram,
                            uint64_t semanticNodeId,
                            const std::string &bindingTypeText) {
    primec::SemanticProgramBindingFact fact;
    fact.scopePath = "/main";
    fact.siteKind = "local";
    fact.name = "values";
    fact.bindingTypeText = bindingTypeText;
    fact.semanticNodeId = semanticNodeId;
    fact.scopePathId = intern(semanticProgram, "/main");
    fact.siteKindId = intern(semanticProgram, "local");
    fact.nameId = intern(semanticProgram, "values");
    fact.bindingTypeTextId = intern(semanticProgram, bindingTypeText);
    const size_t index = semanticProgram.bindingFacts.size();
    semanticProgram.bindingFacts.push_back(std::move(fact));
    semanticProgram.publishedRoutingLookups.bindingFactIndicesByExpr
        .insert_or_assign(semanticNodeId, index);
  };
  auto addQueryFact = [&](primec::SemanticProgram &semanticProgram,
                          uint64_t semanticNodeId,
                          const std::string &queryTypeText) {
    primec::SemanticProgramQueryFact fact;
    fact.scopePath = "/main";
    fact.callName = "at";
    fact.queryTypeText = queryTypeText;
    fact.bindingTypeText = queryTypeText;
    fact.semanticNodeId = semanticNodeId;
    fact.scopePathId = intern(semanticProgram, "/main");
    fact.callNameId = intern(semanticProgram, "at");
    fact.resolvedPathId = intern(semanticProgram, "/std/collections/map/at");
    fact.queryTypeTextId = intern(semanticProgram, queryTypeText);
    fact.bindingTypeTextId = intern(semanticProgram, queryTypeText);
    const size_t index = semanticProgram.queryFacts.size();
    semanticProgram.queryFacts.push_back(std::move(fact));
    semanticProgram.publishedRoutingLookups.queryFactIndicesByExpr
        .insert_or_assign(semanticNodeId, index);
  };

  primec::SemanticProgram semanticProgram;
  addBindingFact(semanticProgram, 9201, "map<i32, string>");
  addBindingFact(semanticProgram, 9202, "map<i32, bool>");
  addBindingFact(semanticProgram, 9203, "i32");
  addQueryFact(semanticProgram, 9204, "string");
  addQueryFact(semanticProgram, 9205, "i32");
  const auto semanticIndex =
      primec::ir_lowerer::buildSemanticProductIndex(&semanticProgram);

  primec::ir_lowerer::LocalMap staleStringMapLocals;
  primec::ir_lowerer::LocalInfo staleStringMapInfo;
  staleStringMapInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  staleStringMapInfo.keyValueKeyKind = Kind::Int32;
  staleStringMapInfo.keyValueValueKind = Kind::String;
  staleStringMapLocals.emplace("values", staleStringMapInfo);

  auto makeCountAtExpr = [](uint64_t accessNodeId, uint64_t targetNodeId) {
    primec::Expr valuesName;
    valuesName.kind = primec::Expr::Kind::Name;
    valuesName.name = "values";
    valuesName.semanticNodeId = targetNodeId;

    primec::Expr keyExpr;
    keyExpr.kind = primec::Expr::Kind::Literal;
    keyExpr.intWidth = 32;
    keyExpr.literalValue = 1;

    primec::Expr accessExpr;
    accessExpr.kind = primec::Expr::Kind::Call;
    accessExpr.name = "at";
    accessExpr.semanticNodeId = accessNodeId;
    accessExpr.args = {valuesName, keyExpr};

    primec::Expr countExpr;
    countExpr.kind = primec::Expr::Kind::Call;
    countExpr.name = "count";
    countExpr.args = {accessExpr};
    return countExpr;
  };

  std::vector<primec::IrInstruction> instructions;
  std::string error;
  int emitExprCalls = 0;
  auto run = [&](const primec::Expr &expr) {
    instructions.clear();
    error.clear();
    emitExprCalls = 0;
    return primec::ir_lowerer::tryEmitCountAccessCall(
        expr,
        staleStringMapLocals,
        [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
          return false;
        },
        [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
          return false;
        },
        [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
          return false;
        },
        [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
          return false;
        },
        [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
          return false;
        },
        [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
          return false;
        },
        [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
          return false;
        },
        [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
          return Kind::Unknown;
        },
        [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) {
          return false;
        },
        [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
          ++emitExprCalls;
          instructions.push_back({primec::IrOpcode::PushI64, 29});
          return true;
        },
        [&](primec::IrOpcode op, uint64_t imm) {
          instructions.push_back({op, imm});
        },
        error,
        &semanticProgram,
        &semanticIndex);
  };

  // A receiver whose binding fact resolves to a String-valued map (9201)
  // already reaches tryEmitCountAccessCall's own string-key-value-access
  // handling (classifySemanticStringKeyValueAccess) and emits the correct
  // LoadStringLength sequence directly - this must succeed (Emitted), not
  // defer, matching the real "compiles native string-valued map
  // constructors on stdlib path" compile_run case
  // (test_compile_run_native_backend_collections_map_literals_and_string_keys.cpp),
  // whose `count(at(values, 1i32))` on a `map<i32, string>` local depends
  // on exactly this shape resolving to a string-length count.
  CHECK(run(makeCountAtExpr(0, 9201)) == Result::Emitted);
  CHECK(error.empty());
  CHECK(emitExprCalls == 1);
  REQUIRE(instructions.size() == 2);
  CHECK(instructions[0].op == primec::IrOpcode::PushI64);
  CHECK(instructions[0].imm == 29);
  CHECK(instructions[1].op == primec::IrOpcode::LoadStringLength);

  // A map<K, bool> receiver fact (9202) correctly defers: the accessed
  // value is neither string nor array/vector-collection shaped, so this
  // classifier is not the one that should decide how to emit it.
  CHECK(run(makeCountAtExpr(0, 9202)) == Result::NotHandled);
  CHECK(error.empty());
  CHECK(emitExprCalls == 0);
  CHECK(instructions.empty());

  // A plain scalar (i32) receiver fact (9203) defers the same way.
  CHECK(run(makeCountAtExpr(0, 9203)) == Result::NotHandled);
  CHECK(error.empty());
  CHECK(emitExprCalls == 0);
  CHECK(instructions.empty());

  // A query fact directly on the access call itself resolving to "string"
  // (9204) is handled the same way as the binding-fact case above.
  CHECK(run(makeCountAtExpr(9204, 0)) == Result::Emitted);
  CHECK(error.empty());
  CHECK(emitExprCalls == 1);
  REQUIRE(instructions.size() == 2);
  CHECK(instructions[0].op == primec::IrOpcode::PushI64);
  CHECK(instructions[0].imm == 29);
  CHECK(instructions[1].op == primec::IrOpcode::LoadStringLength);

  // A query fact resolving to a plain scalar ("i32", 9205) is neither a
  // string nor a collection access, so count() over it is rejected outright
  // by the TODO-5256 guard (a raw scalar must never be treated as a string
  // handle at runtime) rather than deferred.
  CHECK(run(makeCountAtExpr(9205, 0)) == Result::Error);
  CHECK(error == "count() argument resolves to a non-string value");
  CHECK(emitExprCalls == 0);
  CHECK(instructions.empty());

  // With no semantic facts at all (both ids 0), the classifier still falls
  // back to the raw LocalInfo on `values` (map<i32, string>, set up above
  // as staleStringMapInfo) and correctly emits the string-length count.
  CHECK(run(makeCountAtExpr(0, 0)) == Result::Emitted);
  CHECK(error.empty());
  CHECK(emitExprCalls == 1);
  REQUIRE(instructions.size() == 2);
  CHECK(instructions[0].op == primec::IrOpcode::PushI64);
  CHECK(instructions[0].imm == 29);
  CHECK(instructions[1].op == primec::IrOpcode::LoadStringLength);
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

  nameExpr.semanticNodeId = 7111;
  primec::SemanticProgram semanticProgram;
  semanticProgram.bindingFacts.push_back(primec::SemanticProgramBindingFact{
      .scopePath = "/main",
      .siteKind = "local",
      .name = "name",
      .bindingTypeText = "i32",
      .semanticNodeId = nameExpr.semanticNodeId,
      .bindingTypeTextId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "i32"),
  });
  semanticProgram.publishedRoutingLookups.bindingFactIndicesByExpr
      .insert_or_assign(nameExpr.semanticNodeId, 0);
  const auto semanticIndex = primec::ir_lowerer::buildSemanticProductIndex(&semanticProgram);

  error = "stale";
  CHECK_FALSE(primec::ir_lowerer::resolveStringTableTarget(
      nameExpr,
      locals,
      stringTable,
      internString,
      stringIndex,
      length,
      error,
      &semanticProgram,
      &semanticIndex));
  CHECK(error == "stale");

  auto semanticResolver = primec::ir_lowerer::makeResolveStringTableTarget(
      stringTable, internString, error, &semanticProgram);
  CHECK_FALSE(semanticResolver(nameExpr, locals, stringIndex, length));

  auto semanticHelpers =
      primec::ir_lowerer::makeStringLiteralHelperContext(stringTable, error, &semanticProgram);
  CHECK_FALSE(semanticHelpers.resolveStringTableTarget(nameExpr, locals, stringIndex, length));
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

TEST_CASE("ir lowerer template type parse helper splits template type names") {
  std::string base;
  std::string arg;

  REQUIRE(primec::ir_lowerer::splitTemplateTypeName("Result< map<string, i64> , FileError >", base, arg));
  CHECK(base == "Result");
  CHECK(arg == " map<string, i64> , FileError ");

  CHECK_FALSE(primec::ir_lowerer::splitTemplateTypeName("Result<i64", base, arg));
  CHECK(base.empty());
  CHECK(arg.empty());

  CHECK_FALSE(primec::ir_lowerer::splitTemplateTypeName("i64", base, arg));
  CHECK(base.empty());
  CHECK(arg.empty());

  CHECK_FALSE(primec::ir_lowerer::splitTemplateTypeName("", base, arg));
  CHECK(base.empty());
  CHECK(arg.empty());
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

  REQUIRE(primec::ir_lowerer::parseResultTypeName(
      "/std/result/Result< i32 , MyError >",
      hasValue,
      valueKind,
      errorType));
  CHECK(hasValue);
  CHECK(valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  CHECK(errorType == "MyError");

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
  primec::ir_lowerer::emitPointerIndexOutOfBounds(function, internString);
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
                                                     "pointer index out of bounds",
                                                     "map key not found",
                                                     "container index out of bounds",
                                                     "container empty",
                                                     "vector push allocation failed (out of memory)",
                                                     "vector reserve expects non-negative capacity",
                                                     "vector reserve allocation failed (out of memory)",
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

TEST_SUITE_END();
