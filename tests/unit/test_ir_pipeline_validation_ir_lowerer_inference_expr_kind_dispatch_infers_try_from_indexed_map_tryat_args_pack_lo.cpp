#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir lowerer inference expr-kind dispatch infers try from indexed map tryAt args pack lookups") {
  primec::ir_lowerer::LowerInferenceSetupBootstrapState state;
  state.inferLiteralOrNameExprKind = [](const primec::Expr &expr,
                                        const primec::ir_lowerer::LocalMap &,
                                        primec::ir_lowerer::LocalInfo::ValueKind &kindOut) {
    kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
    if (expr.kind == primec::Expr::Kind::Literal) {
      kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
      return true;
    }
    return false;
  };
  state.inferCallExprBaseKind = [](const primec::Expr &,
                                   const primec::ir_lowerer::LocalMap &,
                                   primec::ir_lowerer::LocalInfo::ValueKind &kindOut) {
    kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
    return false;
  };
  state.inferCallExprDirectReturnKind = [](const primec::Expr &,
                                           const primec::ir_lowerer::LocalMap &,
                                           primec::ir_lowerer::LocalInfo::ValueKind &kindOut) {
    kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
    return primec::ir_lowerer::CallExpressionReturnKindResolution::NotResolved;
  };
  state.inferCallExprCountAccessGpuFallbackKind = [](const primec::Expr &,
                                                     const primec::ir_lowerer::LocalMap &,
                                                     primec::ir_lowerer::LocalInfo::ValueKind &kindOut) {
    kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
    return false;
  };
  state.resolveMethodCallDefinition =
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
    return nullptr;
  };
  state.inferCallExprOperatorFallbackKind = [](const primec::Expr &,
                                               const primec::ir_lowerer::LocalMap &,
                                               primec::ir_lowerer::LocalInfo::ValueKind &kindOut) {
    kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
    return false;
  };
  state.inferCallExprControlFlowFallbackKind = [](const primec::Expr &,
                                                  const primec::ir_lowerer::LocalMap &,
                                                  std::string &,
                                                  primec::ir_lowerer::LocalInfo::ValueKind &kindOut) {
    kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
    return false;
  };
  state.inferCallExprPointerFallbackKind = [](const primec::Expr &,
                                              const primec::ir_lowerer::LocalMap &,
                                              primec::ir_lowerer::LocalInfo::ValueKind &kindOut) {
    kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
    return false;
  };

  std::unordered_map<std::string, const primec::Definition *> defMap;
  std::string inferenceError;
  std::string error;
  CHECK(primec::ir_lowerer::runLowerInferenceExprKindDispatchSetup(
      {
          .defMap = &defMap,
          .resolveExprPath = [](const primec::Expr &expr) { return "/" + expr.name; },
          .error = &inferenceError,
      },
      state,
      error));
  CHECK(error.empty());
  CHECK(static_cast<bool>(state.inferExprKind));

  auto makeArgsPackInfo = [](primec::ir_lowerer::LocalInfo::Kind elementKind,
                             bool referenceToKeyValueCollection,
                             bool pointerToKeyValueCollection) {
    primec::ir_lowerer::LocalInfo info;
    info.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
    info.isArgsPack = true;
    info.argsPackElementKind = elementKind;
    info.referenceToKeyValueCollection = referenceToKeyValueCollection;
    info.pointerToKeyValueCollection = pointerToKeyValueCollection;
    info.keyValueKeyKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
    info.keyValueValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
    info.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
    return info;
  };

  primec::ir_lowerer::LocalMap locals;
  locals.emplace("maps", makeArgsPackInfo(primec::ir_lowerer::LocalInfo::Kind::KeyValueCollection, false, false));
  locals.emplace("mapRefs", makeArgsPackInfo(primec::ir_lowerer::LocalInfo::Kind::Reference, true, false));
  locals.emplace("mapPtrs", makeArgsPackInfo(primec::ir_lowerer::LocalInfo::Kind::Pointer, false, true));

  auto makeNameExpr = [](const std::string &name) {
    primec::Expr expr;
    expr.kind = primec::Expr::Kind::Name;
    expr.name = name;
    return expr;
  };
  auto makeLiteralExpr = [](int64_t value) {
    primec::Expr expr;
    expr.kind = primec::Expr::Kind::Literal;
    expr.intWidth = 32;
    expr.literalValue = value;
    return expr;
  };
  auto makeAtExpr = [&](const std::string &packName) {
    primec::Expr atExpr;
    atExpr.kind = primec::Expr::Kind::Call;
    atExpr.name = "at";
    atExpr.args = {makeNameExpr(packName), makeLiteralExpr(0)};
    return atExpr;
  };
  auto makeTryExpr = [](primec::Expr resultExpr) {
    primec::Expr tryExpr;
    tryExpr.kind = primec::Expr::Kind::Call;
    tryExpr.name = "try";
    tryExpr.args = {std::move(resultExpr)};
    return tryExpr;
  };

  primec::Expr helperTryAtExpr;
  helperTryAtExpr.kind = primec::Expr::Kind::Call;
  helperTryAtExpr.name = "/std/collections/map/tryAt";
  helperTryAtExpr.args = {makeAtExpr("maps"), makeLiteralExpr(3)};
  CHECK(state.inferExprKind(makeTryExpr(helperTryAtExpr), locals) ==
        primec::ir_lowerer::LocalInfo::ValueKind::Int32);

  primec::Expr borrowedMethodTryAtExpr;
  borrowedMethodTryAtExpr.kind = primec::Expr::Kind::Call;
  borrowedMethodTryAtExpr.name = "tryAt";
  borrowedMethodTryAtExpr.isMethodCall = true;
  borrowedMethodTryAtExpr.args = {makeAtExpr("mapRefs"), makeLiteralExpr(3)};
  CHECK(state.inferExprKind(makeTryExpr(borrowedMethodTryAtExpr), locals) ==
        primec::ir_lowerer::LocalInfo::ValueKind::Int32);

  primec::Expr pointerMethodTryAtExpr;
  pointerMethodTryAtExpr.kind = primec::Expr::Kind::Call;
  pointerMethodTryAtExpr.name = "tryAt";
  pointerMethodTryAtExpr.isMethodCall = true;
  pointerMethodTryAtExpr.args = {makeAtExpr("mapPtrs"), makeLiteralExpr(3)};
  CHECK(state.inferExprKind(makeTryExpr(pointerMethodTryAtExpr), locals) ==
        primec::ir_lowerer::LocalInfo::ValueKind::Int32);
}

TEST_CASE("ir lowerer inference expr-kind dispatch prefers graph-backed indexed map value facts") {
  using Kind = primec::ir_lowerer::LocalInfo::ValueKind;

  auto addQueryFact = [](primec::SemanticProgram &semanticProgram,
                         uint64_t semanticNodeId,
                         const std::string &queryTypeText) {
    primec::SemanticProgramQueryFact queryFact;
    queryFact.semanticNodeId = semanticNodeId;
    queryFact.callName = "at";
    queryFact.callNameId =
        primec::semanticProgramInternCallTargetString(semanticProgram, "at");
    queryFact.resolvedPathId = primec::semanticProgramInternCallTargetString(
        semanticProgram, "/std/collections/map/at");
    queryFact.queryTypeText = queryTypeText;
    queryFact.queryTypeTextId =
        primec::semanticProgramInternCallTargetString(semanticProgram, queryTypeText);
    queryFact.bindingTypeText = queryTypeText;
    queryFact.bindingTypeTextId =
        primec::semanticProgramInternCallTargetString(semanticProgram, queryTypeText);
    const size_t index = semanticProgram.queryFacts.size();
    semanticProgram.queryFacts.push_back(std::move(queryFact));
    semanticProgram.publishedRoutingLookups.queryFactIndicesByExpr.insert_or_assign(
        semanticNodeId,
        index);
  };
  auto addBindingFact = [](primec::SemanticProgram &semanticProgram,
                           uint64_t semanticNodeId,
                           const std::string &bindingTypeText) {
    primec::SemanticProgramBindingFact bindingFact;
    bindingFact.semanticNodeId = semanticNodeId;
    bindingFact.scopePath = "/main";
    bindingFact.siteKind = "local";
    bindingFact.name = "values";
    bindingFact.bindingTypeText = bindingTypeText;
    bindingFact.scopePathId =
        primec::semanticProgramInternCallTargetString(semanticProgram, "/main");
    bindingFact.siteKindId =
        primec::semanticProgramInternCallTargetString(semanticProgram, "local");
    bindingFact.nameId =
        primec::semanticProgramInternCallTargetString(semanticProgram, "values");
    bindingFact.bindingTypeTextId =
        primec::semanticProgramInternCallTargetString(semanticProgram, bindingTypeText);
    const size_t index = semanticProgram.bindingFacts.size();
    semanticProgram.bindingFacts.push_back(std::move(bindingFact));
    semanticProgram.publishedRoutingLookups.bindingFactIndicesByExpr.insert_or_assign(
        semanticNodeId,
        index);
  };

  primec::SemanticProgram semanticProgram;
  addQueryFact(semanticProgram, 7301, "string");
  addQueryFact(semanticProgram, 7302, "i32");
  addBindingFact(semanticProgram, 7303, "map<i32, string>");
  addBindingFact(semanticProgram, 7304, "map<i32, bool>");
  addBindingFact(semanticProgram, 7305, "i32");
  const auto semanticIndex =
      primec::ir_lowerer::buildSemanticProductIndex(&semanticProgram);

  primec::ir_lowerer::LowerInferenceSetupBootstrapState state;
  state.semanticProgram = &semanticProgram;
  state.semanticIndex = &semanticIndex;
  state.inferLiteralOrNameExprKind = [](const primec::Expr &,
                                        const primec::ir_lowerer::LocalMap &,
                                        Kind &kindOut) {
    kindOut = Kind::Unknown;
    return false;
  };
  state.inferCallExprDirectReturnKind = [](const primec::Expr &,
                                           const primec::ir_lowerer::LocalMap &,
                                           Kind &kindOut) {
    kindOut = Kind::Unknown;
    return primec::ir_lowerer::CallExpressionReturnKindResolution::NotResolved;
  };
  state.inferCallExprCountAccessGpuFallbackKind = [](const primec::Expr &,
                                                     const primec::ir_lowerer::LocalMap &,
                                                     Kind &kindOut) {
    kindOut = Kind::Unknown;
    return false;
  };
  state.inferCallExprOperatorFallbackKind = [](const primec::Expr &,
                                               const primec::ir_lowerer::LocalMap &,
                                               Kind &kindOut) {
    kindOut = Kind::Unknown;
    return false;
  };
  state.inferCallExprControlFlowFallbackKind = [](const primec::Expr &,
                                                  const primec::ir_lowerer::LocalMap &,
                                                  std::string &,
                                                  Kind &kindOut) {
    kindOut = Kind::Unknown;
    return false;
  };
  state.inferCallExprPointerFallbackKind = [](const primec::Expr &,
                                              const primec::ir_lowerer::LocalMap &,
                                              Kind &kindOut) {
    kindOut = Kind::Unknown;
    return false;
  };
  std::string error;
  CHECK(primec::ir_lowerer::runLowerInferenceExprKindCallBaseSetup(
      {
          .inferStructExprPath =
              [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
                return std::string{};
              },
          .resolveStructFieldSlot =
              [](const std::string &, const std::string &,
                 primec::ir_lowerer::StructSlotFieldInfo &) { return false; },
          .resolveUninitializedStorage =
              [](const primec::Expr &, const primec::ir_lowerer::LocalMap &,
                 primec::ir_lowerer::UninitializedStorageAccessInfo &,
                 bool &resolved) {
                resolved = false;
                return true;
              },
      },
      state,
      error));
  CHECK(error.empty());

  std::unordered_map<std::string, const primec::Definition *> defMap;
  std::string inferenceError;
  CHECK(primec::ir_lowerer::runLowerInferenceExprKindDispatchSetup(
      {
          .defMap = &defMap,
          .resolveExprPath = [](const primec::Expr &expr) {
            return "/" + expr.name;
          },
          .error = &inferenceError,
      },
      state,
      error));
  CHECK(error.empty());
  CHECK(static_cast<bool>(state.inferExprKind));

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo staleMapInfo;
  staleMapInfo.kind = primec::ir_lowerer::LocalInfo::Kind::KeyValueCollection;
  staleMapInfo.keyValueKeyKind = Kind::Int32;
  staleMapInfo.keyValueValueKind = Kind::String;
  locals.emplace("values", staleMapInfo);

  primec::Expr valuesName;
  valuesName.kind = primec::Expr::Kind::Name;
  valuesName.name = "values";

  primec::Expr keyExpr;
  keyExpr.kind = primec::Expr::Kind::Literal;
  keyExpr.intWidth = 32;
  keyExpr.literalValue = 0;

  primec::Expr accessExpr;
  accessExpr.kind = primec::Expr::Kind::Call;
  accessExpr.name = "at";
  accessExpr.semanticNodeId = 7301;
  accessExpr.args = {valuesName, keyExpr};

  primec::Expr countExpr;
  countExpr.kind = primec::Expr::Kind::Call;
  countExpr.name = "count";
  countExpr.args = {accessExpr};

  CHECK(state.inferExprKind(countExpr, locals) == Kind::Int32);

  accessExpr.semanticNodeId = 7302;
  countExpr.args = {accessExpr};
  CHECK(state.inferExprKind(countExpr, locals) == Kind::Unknown);

  accessExpr.semanticNodeId = 0;
  countExpr.args = {accessExpr};
  CHECK(state.inferExprKind(countExpr, locals) == Kind::Int32);

  valuesName.semanticNodeId = 7303;
  accessExpr.args = {valuesName, keyExpr};
  countExpr.args = {accessExpr};
  CHECK(state.inferExprKind(countExpr, locals) == Kind::Int32);

  valuesName.semanticNodeId = 7304;
  accessExpr.args = {valuesName, keyExpr};
  countExpr.args = {accessExpr};
  CHECK(state.inferExprKind(countExpr, locals) == Kind::Unknown);

  valuesName.semanticNodeId = 7305;
  accessExpr.args = {valuesName, keyExpr};
  countExpr.args = {accessExpr};
  CHECK(state.inferExprKind(countExpr, locals) == Kind::Unknown);

  valuesName.semanticNodeId = 0;
  accessExpr.args = {valuesName, keyExpr};
  countExpr.args = {accessExpr};
  CHECK(state.inferExprKind(countExpr, locals) == Kind::Int32);
}

TEST_CASE("ir lowerer inference expr-kind dispatch uses semantic map receiver facts") {
  using Kind = primec::ir_lowerer::LocalInfo::ValueKind;

  auto intern = [](primec::SemanticProgram &semanticProgram, const std::string &text) {
    return primec::semanticProgramInternCallTargetString(semanticProgram, text);
  };

  auto addCollectionFact = [&](primec::SemanticProgram &semanticProgram,
                               uint64_t semanticNodeId,
                               const std::string &family,
                               const std::string &keyType,
                               const std::string &valueType) {
    primec::SemanticProgramCollectionSpecialization fact;
    fact.scopePath = "/main";
    fact.siteKind = "local";
    fact.name = "values";
    fact.collectionFamily = family;
    fact.bindingTypeText = family + "<" + keyType + ", " + valueType + ">";
    fact.keyTypeText = keyType;
    fact.valueTypeText = valueType;
    fact.semanticNodeId = semanticNodeId;
    fact.scopePathId = intern(semanticProgram, "/main");
    fact.siteKindId = intern(semanticProgram, "local");
    fact.nameId = intern(semanticProgram, "values");
    fact.collectionFamilyId = intern(semanticProgram, family);
    fact.bindingTypeTextId = intern(semanticProgram, fact.bindingTypeText);
    fact.keyTypeTextId = intern(semanticProgram, keyType);
    fact.valueTypeTextId = intern(semanticProgram, valueType);
    const size_t index = semanticProgram.collectionSpecializations.size();
    semanticProgram.collectionSpecializations.push_back(std::move(fact));
    semanticProgram.publishedRoutingLookups.collectionSpecializationIndicesByExpr
        .insert_or_assign(semanticNodeId, index);
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

  auto installDispatchSetup =
      [](primec::ir_lowerer::LowerInferenceSetupBootstrapState &state,
         std::unordered_map<std::string, const primec::Definition *> &defMap,
         std::string &inferenceError) {
    state.inferLiteralOrNameExprKind = [](const primec::Expr &,
                                          const primec::ir_lowerer::LocalMap &,
                                          Kind &kindOut) {
      kindOut = Kind::Unknown;
      return false;
    };
    state.inferCallExprBaseKind = [](const primec::Expr &,
                                     const primec::ir_lowerer::LocalMap &,
                                     Kind &kindOut) {
      kindOut = Kind::Unknown;
      return false;
    };
    state.inferCallExprDirectReturnKind = [](const primec::Expr &,
                                             const primec::ir_lowerer::LocalMap &,
                                             Kind &kindOut) {
      kindOut = Kind::Unknown;
      return primec::ir_lowerer::CallExpressionReturnKindResolution::NotResolved;
    };
    state.inferCallExprCountAccessGpuFallbackKind = [](const primec::Expr &,
                                                       const primec::ir_lowerer::LocalMap &,
                                                       Kind &kindOut) {
      kindOut = Kind::Unknown;
      return false;
    };
    state.inferCallExprOperatorFallbackKind = [](const primec::Expr &,
                                                 const primec::ir_lowerer::LocalMap &,
                                                 Kind &kindOut) {
      kindOut = Kind::Unknown;
      return false;
    };
    state.inferCallExprControlFlowFallbackKind = [](const primec::Expr &,
                                                    const primec::ir_lowerer::LocalMap &,
                                                    std::string &,
                                                    Kind &kindOut) {
      kindOut = Kind::Unknown;
      return false;
    };
    state.inferCallExprPointerFallbackKind = [](const primec::Expr &,
                                                const primec::ir_lowerer::LocalMap &,
                                                Kind &kindOut) {
      kindOut = Kind::Unknown;
      return false;
    };
    state.resolveMethodCallDefinition =
        [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
      return nullptr;
    };
    std::string setupError;
    CHECK(primec::ir_lowerer::runLowerInferenceExprKindDispatchSetup(
        {
            .defMap = &defMap,
            .resolveExprPath = [](const primec::Expr &expr) { return "/" + expr.name; },
            .error = &inferenceError,
        },
        state,
        setupError));
    CHECK(setupError.empty());
    REQUIRE(static_cast<bool>(state.inferExprKind));
  };

  auto makeNameExpr = [](const std::string &name, uint64_t semanticNodeId) {
    primec::Expr expr;
    expr.kind = primec::Expr::Kind::Name;
    expr.name = name;
    expr.semanticNodeId = semanticNodeId;
    return expr;
  };

  auto makeLiteralExpr = [] {
    primec::Expr expr;
    expr.kind = primec::Expr::Kind::Literal;
    expr.intWidth = 32;
    expr.literalValue = 7;
    return expr;
  };

  auto makeMapCallExpr = [&](const std::string &name, primec::Expr receiver) {
    primec::Expr expr;
    expr.kind = primec::Expr::Kind::Call;
    expr.name = name;
    expr.args = {std::move(receiver), makeLiteralExpr()};
    return expr;
  };

  auto makeTryExpr = [](primec::Expr operand) {
    primec::Expr expr;
    expr.kind = primec::Expr::Kind::Call;
    expr.name = "try";
    expr.args = {std::move(operand)};
    return expr;
  };

  primec::SemanticProgram semanticProgram;
  addCollectionFact(semanticProgram, 7311, "map", "i32", "string");
  addBindingFact(semanticProgram, 7312, "/std/collections/map<i32, bool>");
  addBindingFact(semanticProgram, 7313, "i32");
  const auto semanticIndex =
      primec::ir_lowerer::buildSemanticProductIndex(&semanticProgram);

  primec::ir_lowerer::LocalInfo staleMapInfo;
  staleMapInfo.kind = primec::ir_lowerer::LocalInfo::Kind::KeyValueCollection;
  staleMapInfo.keyValueKeyKind = Kind::Int32;
  staleMapInfo.keyValueValueKind = Kind::Int32;

  primec::ir_lowerer::LocalMap staleLocals;
  staleLocals.emplace("values", staleMapInfo);

  std::unordered_map<std::string, const primec::Definition *> defMap;
  std::string inferenceError;
  primec::ir_lowerer::LowerInferenceSetupBootstrapState state;
  state.semanticProgram = &semanticProgram;
  state.semanticIndex = &semanticIndex;
  installDispatchSetup(state, defMap, inferenceError);

  CHECK(state.inferExprKind(makeTryExpr(makeMapCallExpr("tryAt",
                                                        makeNameExpr("values", 7311))),
                            staleLocals) == Kind::String);
  CHECK(state.inferExprKind(makeTryExpr(makeMapCallExpr("contains",
                                                        makeNameExpr("values", 7312))),
                            staleLocals) == Kind::Bool);
  CHECK(state.inferExprKind(makeTryExpr(makeMapCallExpr("tryAt",
                                                        makeNameExpr("values", 7313))),
                            staleLocals) == Kind::Unknown);
  CHECK(inferenceError.empty());

  std::unordered_map<std::string, const primec::Definition *> syntaxDefMap;
  std::string syntaxInferenceError;
  primec::ir_lowerer::LowerInferenceSetupBootstrapState syntaxState;
  installDispatchSetup(syntaxState, syntaxDefMap, syntaxInferenceError);
  CHECK(syntaxState.inferExprKind(makeTryExpr(makeMapCallExpr("tryAt",
                                                              makeNameExpr("values", 0))),
                                  staleLocals) == Kind::Int32);
  CHECK(syntaxState.inferExprKind(makeTryExpr(makeMapCallExpr("contains",
                                                              makeNameExpr("values", 0))),
                                  staleLocals) == Kind::Bool);
  CHECK(syntaxInferenceError.empty());
}

TEST_CASE("ir lowerer inference expr-kind dispatch uses semantic try operand Result facts") {
  using Kind = primec::ir_lowerer::LocalInfo::ValueKind;

  auto addBindingFact = [](primec::SemanticProgram &semanticProgram,
                           uint64_t semanticNodeId,
                           const std::string &name,
                           const std::string &bindingTypeText) {
    primec::SemanticProgramBindingFact fact;
    fact.scopePath = "/main";
    fact.siteKind = "local";
    fact.name = name;
    fact.bindingTypeText = bindingTypeText;
    fact.semanticNodeId = semanticNodeId;
    fact.scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main");
    fact.siteKindId = primec::semanticProgramInternCallTargetString(semanticProgram, "local");
    fact.nameId = primec::semanticProgramInternCallTargetString(semanticProgram, name);
    fact.bindingTypeTextId =
        primec::semanticProgramInternCallTargetString(semanticProgram, bindingTypeText);
    const size_t index = semanticProgram.bindingFacts.size();
    semanticProgram.bindingFacts.push_back(std::move(fact));
    semanticProgram.publishedRoutingLookups.bindingFactIndicesByExpr.insert_or_assign(
        semanticNodeId,
        index);
  };

  auto addQueryResultFact = [](primec::SemanticProgram &semanticProgram,
                               uint64_t semanticNodeId,
                               const std::string &callName,
                               const std::string &resultValueType) {
    primec::SemanticProgramQueryFact fact;
    fact.scopePath = "/main";
    fact.callName = callName;
    fact.hasResultType = true;
    fact.resultTypeHasValue = true;
    fact.resultValueType = resultValueType;
    fact.resultErrorType = "FileError";
    fact.semanticNodeId = semanticNodeId;
    fact.scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main");
    fact.callNameId = primec::semanticProgramInternCallTargetString(semanticProgram, callName);
    fact.resolvedPathId =
        primec::semanticProgramInternCallTargetString(semanticProgram, "/main/" + callName);
    fact.resultValueTypeId =
        primec::semanticProgramInternCallTargetString(semanticProgram, resultValueType);
    fact.resultErrorTypeId =
        primec::semanticProgramInternCallTargetString(semanticProgram, "FileError");
    const size_t index = semanticProgram.queryFacts.size();
    semanticProgram.queryFacts.push_back(std::move(fact));
    semanticProgram.publishedRoutingLookups.queryFactIndicesByExpr.insert_or_assign(
        semanticNodeId,
        index);
  };

  auto addQueryTypeFact = [](primec::SemanticProgram &semanticProgram,
                             uint64_t semanticNodeId,
                             const std::string &callName,
                             const std::string &queryTypeText) {
    primec::SemanticProgramQueryFact fact;
    fact.scopePath = "/main";
    fact.callName = callName;
    fact.queryTypeText = queryTypeText;
    fact.bindingTypeText = queryTypeText;
    fact.semanticNodeId = semanticNodeId;
    fact.scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main");
    fact.callNameId = primec::semanticProgramInternCallTargetString(semanticProgram, callName);
    fact.resolvedPathId =
        primec::semanticProgramInternCallTargetString(semanticProgram, "/main/" + callName);
    fact.queryTypeTextId =
        primec::semanticProgramInternCallTargetString(semanticProgram, queryTypeText);
    fact.bindingTypeTextId =
        primec::semanticProgramInternCallTargetString(semanticProgram, queryTypeText);
    const size_t index = semanticProgram.queryFacts.size();
    semanticProgram.queryFacts.push_back(std::move(fact));
    semanticProgram.publishedRoutingLookups.queryFactIndicesByExpr.insert_or_assign(
        semanticNodeId,
        index);
  };

  auto addLocalAutoFact = [](primec::SemanticProgram &semanticProgram,
                             uint64_t semanticNodeId,
                             const std::string &name,
                             const std::string &bindingTypeText) {
    primec::SemanticProgramLocalAutoFact fact;
    fact.scopePath = "/main";
    fact.bindingName = name;
    fact.bindingTypeText = bindingTypeText;
    fact.initializerBindingTypeText = bindingTypeText;
    fact.semanticNodeId = semanticNodeId;
    fact.scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main");
    fact.bindingNameId = primec::semanticProgramInternCallTargetString(semanticProgram, name);
    fact.bindingTypeTextId =
        primec::semanticProgramInternCallTargetString(semanticProgram, bindingTypeText);
    const size_t index = semanticProgram.localAutoFacts.size();
    semanticProgram.localAutoFacts.push_back(std::move(fact));
    semanticProgram.publishedRoutingLookups.localAutoFactIndicesByExpr.insert_or_assign(
        semanticNodeId,
        index);
  };

  auto installDispatchSetup = [](primec::ir_lowerer::LowerInferenceSetupBootstrapState &state,
                                 std::unordered_map<std::string, const primec::Definition *> &defMap,
                                 std::string &inferenceError) {
    state.inferLiteralOrNameExprKind = [](const primec::Expr &,
                                          const primec::ir_lowerer::LocalMap &,
                                          Kind &kindOut) {
      kindOut = Kind::Unknown;
      return false;
    };
    state.inferCallExprBaseKind = [](const primec::Expr &,
                                     const primec::ir_lowerer::LocalMap &,
                                     Kind &kindOut) {
      kindOut = Kind::Unknown;
      return false;
    };
    state.inferCallExprDirectReturnKind = [](const primec::Expr &,
                                             const primec::ir_lowerer::LocalMap &,
                                             Kind &kindOut) {
      kindOut = Kind::Unknown;
      return primec::ir_lowerer::CallExpressionReturnKindResolution::NotResolved;
    };
    state.inferCallExprCountAccessGpuFallbackKind = [](const primec::Expr &,
                                                       const primec::ir_lowerer::LocalMap &,
                                                       Kind &kindOut) {
      kindOut = Kind::Unknown;
      return false;
    };
    state.inferCallExprOperatorFallbackKind = [](const primec::Expr &,
                                                 const primec::ir_lowerer::LocalMap &,
                                                 Kind &kindOut) {
      kindOut = Kind::Unknown;
      return false;
    };
    state.inferCallExprControlFlowFallbackKind = [](const primec::Expr &,
                                                    const primec::ir_lowerer::LocalMap &,
                                                    std::string &,
                                                    Kind &kindOut) {
      kindOut = Kind::Unknown;
      return false;
    };
    state.inferCallExprPointerFallbackKind = [](const primec::Expr &,
                                                const primec::ir_lowerer::LocalMap &,
                                                Kind &kindOut) {
      kindOut = Kind::Unknown;
      return false;
    };
    state.resolveMethodCallDefinition =
        [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
      return nullptr;
    };
    std::string setupError;
    CHECK(primec::ir_lowerer::runLowerInferenceExprKindDispatchSetup(
        {
            .defMap = &defMap,
            .resolveExprPath = [](const primec::Expr &expr) { return "/" + expr.name; },
            .error = &inferenceError,
        },
        state,
        setupError));
    CHECK(setupError.empty());
    REQUIRE(static_cast<bool>(state.inferExprKind));
  };

  auto makeNameExpr = [](const std::string &name, uint64_t semanticNodeId) {
    primec::Expr expr;
    expr.kind = primec::Expr::Kind::Name;
    expr.name = name;
    expr.semanticNodeId = semanticNodeId;
    return expr;
  };

  auto makeLiteralExpr = [] {
    primec::Expr expr;
    expr.kind = primec::Expr::Kind::Literal;
    expr.intWidth = 32;
    expr.literalValue = 0;
    return expr;
  };

  auto makeCallExpr = [](const std::string &name, uint64_t semanticNodeId) {
    primec::Expr expr;
    expr.kind = primec::Expr::Kind::Call;
    expr.name = name;
    expr.semanticNodeId = semanticNodeId;
    return expr;
  };

  auto makeAtExpr = [&](const std::string &name, uint64_t semanticNodeId) {
    primec::Expr expr;
    expr.kind = primec::Expr::Kind::Call;
    expr.name = "at";
    expr.args = {makeNameExpr(name, 0), makeLiteralExpr()};
    expr.semanticNodeId = semanticNodeId;
    return expr;
  };

  auto makeDereferenceExpr = [](primec::Expr target) {
    primec::Expr expr;
    expr.kind = primec::Expr::Kind::Call;
    expr.name = "dereference";
    expr.args = {std::move(target)};
    return expr;
  };

  auto makeTryExpr = [](primec::Expr operand) {
    primec::Expr expr;
    expr.kind = primec::Expr::Kind::Call;
    expr.name = "try";
    expr.args = {std::move(operand)};
    return expr;
  };

  primec::SemanticProgram semanticProgram;
  addBindingFact(semanticProgram, 7401, "result", "Result<string, FileError>");
  addQueryResultFact(semanticProgram, 7402, "lookup", "bool");
  addLocalAutoFact(semanticProgram, 7403, "status", "Result<FileError>");
  addQueryTypeFact(semanticProgram, 7404, "at", "i32");
  addQueryResultFact(semanticProgram, 7405, "at", "bool");
  addQueryTypeFact(semanticProgram, 7406, "at", "i32");
  const auto semanticIndex =
      primec::ir_lowerer::buildSemanticProductIndex(&semanticProgram);

  primec::ir_lowerer::LocalInfo staleNamedResult;
  staleNamedResult.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  staleNamedResult.isResult = true;
  staleNamedResult.resultHasValue = true;
  staleNamedResult.resultValueKind = Kind::Int64;

  primec::ir_lowerer::LocalInfo staleResultPack;
  staleResultPack.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  staleResultPack.isArgsPack = true;
  staleResultPack.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Reference;
  staleResultPack.isResult = true;
  staleResultPack.resultHasValue = true;
  staleResultPack.resultValueKind = Kind::String;

  primec::ir_lowerer::LocalMap staleLocals;
  staleLocals.emplace("result", staleNamedResult);
  staleLocals.emplace("status", staleNamedResult);
  staleLocals.emplace("results", staleResultPack);

  std::unordered_map<std::string, const primec::Definition *> defMap;
  std::string inferenceError;
  primec::ir_lowerer::LowerInferenceSetupBootstrapState state;
  state.semanticProgram = &semanticProgram;
  state.semanticIndex = &semanticIndex;
  installDispatchSetup(state, defMap, inferenceError);

  CHECK(state.inferExprKind(makeTryExpr(makeNameExpr("result", 7401)), staleLocals) ==
        Kind::String);
  CHECK(state.inferExprKind(makeTryExpr(makeCallExpr("lookup", 7402)), staleLocals) ==
        Kind::Bool);
  CHECK(state.inferExprKind(makeTryExpr(makeNameExpr("status", 7403)), staleLocals) ==
        Kind::Int32);
  CHECK(state.inferExprKind(makeTryExpr(makeAtExpr("results", 7404)), staleLocals) ==
        Kind::Unknown);
  CHECK(state.inferExprKind(
            makeTryExpr(makeDereferenceExpr(makeAtExpr("results", 7405))),
            staleLocals) == Kind::Bool);
  CHECK(state.inferExprKind(
            makeTryExpr(makeDereferenceExpr(makeAtExpr("results", 7406))),
            staleLocals) == Kind::Unknown);
  CHECK(inferenceError.empty());

  std::unordered_map<std::string, const primec::Definition *> syntaxDefMap;
  std::string syntaxInferenceError;
  primec::ir_lowerer::LowerInferenceSetupBootstrapState syntaxState;
  installDispatchSetup(syntaxState, syntaxDefMap, syntaxInferenceError);
  CHECK(syntaxState.inferExprKind(makeTryExpr(makeNameExpr("result", 0)), staleLocals) ==
        Kind::Int64);
  CHECK(syntaxState.inferExprKind(
            makeTryExpr(makeDereferenceExpr(makeAtExpr("results", 0))),
            staleLocals) == Kind::String);
  CHECK(syntaxInferenceError.empty());
}

TEST_CASE("ir lowerer inference expr-kind dispatch uses semantic name facts before stale locals") {
  using Kind = primec::ir_lowerer::LocalInfo::ValueKind;

  auto intern = [](primec::SemanticProgram &semanticProgram,
                   const std::string &text) {
    return primec::semanticProgramInternCallTargetString(semanticProgram, text);
  };

  auto addBindingFact = [&](primec::SemanticProgram &semanticProgram,
                            uint64_t semanticNodeId,
                            const std::string &name,
                            const std::string &bindingTypeText) {
    primec::SemanticProgramBindingFact fact;
    fact.scopePath = "/main";
    fact.siteKind = "local";
    fact.name = name;
    fact.bindingTypeText = bindingTypeText;
    fact.semanticNodeId = semanticNodeId;
    fact.scopePathId = intern(semanticProgram, "/main");
    fact.siteKindId = intern(semanticProgram, "local");
    fact.nameId = intern(semanticProgram, name);
    fact.bindingTypeTextId = intern(semanticProgram, bindingTypeText);
    const size_t index = semanticProgram.bindingFacts.size();
    semanticProgram.bindingFacts.push_back(std::move(fact));
    semanticProgram.publishedRoutingLookups.bindingFactIndicesByExpr
        .insert_or_assign(semanticNodeId, index);
  };

  auto addLocalAutoFact = [&](primec::SemanticProgram &semanticProgram,
                              uint64_t semanticNodeId,
                              const std::string &name,
                              const std::string &bindingTypeText) {
    primec::SemanticProgramLocalAutoFact fact;
    fact.scopePath = "/main";
    fact.bindingName = name;
    fact.bindingTypeText = bindingTypeText;
    fact.initializerBindingTypeText = bindingTypeText;
    fact.semanticNodeId = semanticNodeId;
    fact.scopePathId = intern(semanticProgram, "/main");
    fact.bindingNameId = intern(semanticProgram, name);
    fact.bindingTypeTextId = intern(semanticProgram, bindingTypeText);
    const size_t index = semanticProgram.localAutoFacts.size();
    semanticProgram.localAutoFacts.push_back(std::move(fact));
    semanticProgram.publishedRoutingLookups.localAutoFactIndicesByExpr
        .insert_or_assign(semanticNodeId, index);
  };

  auto addQueryFact = [&](primec::SemanticProgram &semanticProgram,
                          uint64_t semanticNodeId,
                          const std::string &callName,
                          const std::string &queryTypeText) {
    primec::SemanticProgramQueryFact fact;
    fact.scopePath = "/main";
    fact.callName = callName;
    fact.queryTypeText = queryTypeText;
    fact.bindingTypeText = queryTypeText;
    fact.semanticNodeId = semanticNodeId;
    fact.scopePathId = intern(semanticProgram, "/main");
    fact.callNameId = intern(semanticProgram, callName);
    fact.resolvedPathId = intern(semanticProgram, "/main/" + callName);
    fact.queryTypeTextId = intern(semanticProgram, queryTypeText);
    fact.bindingTypeTextId = intern(semanticProgram, queryTypeText);
    const size_t index = semanticProgram.queryFacts.size();
    semanticProgram.queryFacts.push_back(std::move(fact));
    semanticProgram.publishedRoutingLookups.queryFactIndicesByExpr
        .insert_or_assign(semanticNodeId, index);
  };

  primec::SemanticProgram semanticProgram;
  addBindingFact(semanticProgram, 7801, "count", "i64");
  addLocalAutoFact(semanticProgram, 7802, "flag", "bool");
  addQueryFact(semanticProgram, 7803, "textSource", "string");
  addBindingFact(semanticProgram, 7804, "refCount", "Reference<i32>");
  addBindingFact(semanticProgram, 7805, "values", "vector<i32>");
  const auto semanticIndex =
      primec::ir_lowerer::buildSemanticProductIndex(&semanticProgram);

  auto makeNameExpr = [](const std::string &name, uint64_t semanticNodeId) {
    primec::Expr expr;
    expr.kind = primec::Expr::Kind::Name;
    expr.name = name;
    expr.semanticNodeId = semanticNodeId;
    return expr;
  };

  auto makeLocal = [](Kind kind) {
    primec::ir_lowerer::LocalInfo info;
    info.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
    info.valueKind = kind;
    return info;
  };

  primec::ir_lowerer::LocalMap staleLocals;
  staleLocals.emplace("count", makeLocal(Kind::String));
  staleLocals.emplace("flag", makeLocal(Kind::Int64));
  staleLocals.emplace("textSource", makeLocal(Kind::Int32));
  staleLocals.emplace("refCount", makeLocal(Kind::String));
  staleLocals.emplace("values", makeLocal(Kind::String));

  auto installDispatchSetup =
      [](primec::ir_lowerer::LowerInferenceSetupBootstrapState &state,
         std::unordered_map<std::string, const primec::Definition *> &defMap,
         std::string &inferenceError) {
    state.inferLiteralOrNameExprKind = [](const primec::Expr &expr,
                                          const primec::ir_lowerer::LocalMap &locals,
                                          Kind &kindOut) {
      kindOut = Kind::Unknown;
      if (expr.kind != primec::Expr::Kind::Name) {
        return false;
      }
      const auto it = locals.find(expr.name);
      if (it == locals.end()) {
        return true;
      }
      kindOut = it->second.valueKind;
      return true;
    };
    state.inferCallExprBaseKind = [](const primec::Expr &,
                                     const primec::ir_lowerer::LocalMap &,
                                     Kind &kindOut) {
      kindOut = Kind::Unknown;
      return false;
    };
    state.inferCallExprDirectReturnKind = [](const primec::Expr &,
                                             const primec::ir_lowerer::LocalMap &,
                                             Kind &kindOut) {
      kindOut = Kind::Unknown;
      return primec::ir_lowerer::CallExpressionReturnKindResolution::NotResolved;
    };
    state.inferCallExprCountAccessGpuFallbackKind = [](const primec::Expr &,
                                                       const primec::ir_lowerer::LocalMap &,
                                                       Kind &kindOut) {
      kindOut = Kind::Unknown;
      return false;
    };
    state.inferCallExprOperatorFallbackKind = [](const primec::Expr &,
                                                 const primec::ir_lowerer::LocalMap &,
                                                 Kind &kindOut) {
      kindOut = Kind::Unknown;
      return false;
    };
    state.inferCallExprControlFlowFallbackKind = [](const primec::Expr &,
                                                    const primec::ir_lowerer::LocalMap &,
                                                    std::string &,
                                                    Kind &kindOut) {
      kindOut = Kind::Unknown;
      return false;
    };
    state.inferCallExprPointerFallbackKind = [](const primec::Expr &,
                                                const primec::ir_lowerer::LocalMap &,
                                                Kind &kindOut) {
      kindOut = Kind::Unknown;
      return false;
    };
    state.resolveMethodCallDefinition =
        [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
      return nullptr;
    };
    std::string setupError;
    CHECK(primec::ir_lowerer::runLowerInferenceExprKindDispatchSetup(
        {
            .defMap = &defMap,
            .resolveExprPath = [](const primec::Expr &expr) { return "/" + expr.name; },
            .error = &inferenceError,
        },
        state,
        setupError));
    CHECK(setupError.empty());
    REQUIRE(static_cast<bool>(state.inferExprKind));
  };

  std::unordered_map<std::string, const primec::Definition *> defMap;
  std::string inferenceError;
  primec::ir_lowerer::LowerInferenceSetupBootstrapState state;
  state.semanticProgram = &semanticProgram;
  state.semanticIndex = &semanticIndex;
  installDispatchSetup(state, defMap, inferenceError);

  CHECK(state.inferExprKind(makeNameExpr("count", 7801), staleLocals) ==
        Kind::Int64);
  CHECK(state.inferExprKind(makeNameExpr("flag", 7802), staleLocals) ==
        Kind::Bool);
  CHECK(state.inferExprKind(makeNameExpr("textSource", 7803), staleLocals) ==
        Kind::String);
  CHECK(state.inferExprKind(makeNameExpr("refCount", 7804), staleLocals) ==
        Kind::Int32);
  CHECK(state.inferExprKind(makeNameExpr("values", 7805), staleLocals) ==
        Kind::Unknown);
  CHECK(inferenceError.empty());

  std::unordered_map<std::string, const primec::Definition *> syntaxDefMap;
  std::string syntaxInferenceError;
  primec::ir_lowerer::LowerInferenceSetupBootstrapState syntaxState;
  installDispatchSetup(syntaxState, syntaxDefMap, syntaxInferenceError);
  CHECK(syntaxState.inferExprKind(makeNameExpr("count", 0), staleLocals) ==
        Kind::String);
  CHECK(syntaxState.inferExprKind(makeNameExpr("values", 0), staleLocals) ==
        Kind::String);
  CHECK(syntaxInferenceError.empty());
}

TEST_CASE("ir lowerer inference expr-kind dispatch uses semantic method receiver facts") {
  using Kind = primec::ir_lowerer::LocalInfo::ValueKind;

  auto intern = [](primec::SemanticProgram &semanticProgram, const std::string &text) {
    return primec::semanticProgramInternCallTargetString(semanticProgram, text);
  };

  auto addBindingFact = [&](primec::SemanticProgram &semanticProgram,
                            uint64_t semanticNodeId,
                            const std::string &name,
                            const std::string &bindingTypeText) {
    primec::SemanticProgramBindingFact fact;
    fact.scopePath = "/main";
    fact.siteKind = "local";
    fact.name = name;
    fact.bindingTypeText = bindingTypeText;
    fact.semanticNodeId = semanticNodeId;
    fact.scopePathId = intern(semanticProgram, "/main");
    fact.siteKindId = intern(semanticProgram, "local");
    fact.nameId = intern(semanticProgram, name);
    fact.bindingTypeTextId = intern(semanticProgram, bindingTypeText);
    const size_t index = semanticProgram.bindingFacts.size();
    semanticProgram.bindingFacts.push_back(std::move(fact));
    semanticProgram.publishedRoutingLookups.bindingFactIndicesByExpr.insert_or_assign(
        semanticNodeId,
        index);
  };

  auto addQueryFact = [&](primec::SemanticProgram &semanticProgram,
                          uint64_t semanticNodeId,
                          const std::string &callName,
                          const std::string &queryTypeText) {
    primec::SemanticProgramQueryFact fact;
    fact.scopePath = "/main";
    fact.callName = callName;
    fact.queryTypeText = queryTypeText;
    fact.bindingTypeText = queryTypeText;
    fact.semanticNodeId = semanticNodeId;
    fact.scopePathId = intern(semanticProgram, "/main");
    fact.callNameId = intern(semanticProgram, callName);
    fact.resolvedPathId = intern(semanticProgram, "/main/" + callName);
    fact.queryTypeTextId = intern(semanticProgram, queryTypeText);
    fact.bindingTypeTextId = intern(semanticProgram, queryTypeText);
    const size_t index = semanticProgram.queryFacts.size();
    semanticProgram.queryFacts.push_back(std::move(fact));
    semanticProgram.publishedRoutingLookups.queryFactIndicesByExpr.insert_or_assign(
        semanticNodeId,
        index);
  };

  auto addLocalAutoFact = [&](primec::SemanticProgram &semanticProgram,
                              uint64_t semanticNodeId,
                              const std::string &name,
                              const std::string &bindingTypeText) {
    primec::SemanticProgramLocalAutoFact fact;
    fact.scopePath = "/main";
    fact.bindingName = name;
    fact.bindingTypeText = bindingTypeText;
    fact.initializerBindingTypeText = bindingTypeText;
    fact.semanticNodeId = semanticNodeId;
    fact.scopePathId = intern(semanticProgram, "/main");
    fact.bindingNameId = intern(semanticProgram, name);
    fact.bindingTypeTextId = intern(semanticProgram, bindingTypeText);
    const size_t index = semanticProgram.localAutoFacts.size();
    semanticProgram.localAutoFacts.push_back(std::move(fact));
    semanticProgram.publishedRoutingLookups.localAutoFactIndicesByExpr.insert_or_assign(
        semanticNodeId,
        index);
  };

  auto installDispatchSetup = [](primec::ir_lowerer::LowerInferenceSetupBootstrapState &state,
                                 std::unordered_map<std::string, const primec::Definition *> &defMap,
                                 std::string &inferenceError) {
    state.inferLiteralOrNameExprKind = [](const primec::Expr &,
                                          const primec::ir_lowerer::LocalMap &,
                                          Kind &kindOut) {
      kindOut = Kind::Unknown;
      return false;
    };
    state.inferCallExprBaseKind = [](const primec::Expr &,
                                     const primec::ir_lowerer::LocalMap &,
                                     Kind &kindOut) {
      kindOut = Kind::Unknown;
      return false;
    };
    state.inferCallExprDirectReturnKind = [](const primec::Expr &,
                                             const primec::ir_lowerer::LocalMap &,
                                             Kind &kindOut) {
      kindOut = Kind::Unknown;
      return primec::ir_lowerer::CallExpressionReturnKindResolution::NotResolved;
    };
    state.inferCallExprCountAccessGpuFallbackKind = [](const primec::Expr &,
                                                       const primec::ir_lowerer::LocalMap &,
                                                       Kind &kindOut) {
      kindOut = Kind::Unknown;
      return false;
    };
    state.inferCallExprOperatorFallbackKind = [](const primec::Expr &,
                                                 const primec::ir_lowerer::LocalMap &,
                                                 Kind &kindOut) {
      kindOut = Kind::Unknown;
      return false;
    };
    state.inferCallExprControlFlowFallbackKind = [](const primec::Expr &,
                                                    const primec::ir_lowerer::LocalMap &,
                                                    std::string &,
                                                    Kind &kindOut) {
      kindOut = Kind::Unknown;
      return false;
    };
    state.inferCallExprPointerFallbackKind = [](const primec::Expr &,
                                                const primec::ir_lowerer::LocalMap &,
                                                Kind &kindOut) {
      kindOut = Kind::Unknown;
      return false;
    };
    state.resolveMethodCallDefinition =
        [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
      return nullptr;
    };
    std::string setupError;
    CHECK(primec::ir_lowerer::runLowerInferenceExprKindDispatchSetup(
        {
            .defMap = &defMap,
            .resolveExprPath = [](const primec::Expr &expr) { return "/" + expr.name; },
            .error = &inferenceError,
        },
        state,
        setupError));
    CHECK(setupError.empty());
    REQUIRE(static_cast<bool>(state.inferExprKind));
  };

  auto makeNameExpr = [](const std::string &name, uint64_t semanticNodeId) {
    primec::Expr expr;
    expr.kind = primec::Expr::Kind::Name;
    expr.name = name;
    expr.semanticNodeId = semanticNodeId;
    return expr;
  };

  auto makeLiteralExpr = [] {
    primec::Expr expr;
    expr.kind = primec::Expr::Kind::Literal;
    expr.intWidth = 32;
    expr.literalValue = 0;
    return expr;
  };

  auto makeAtExpr = [&](const std::string &name, uint64_t semanticNodeId) {
    primec::Expr expr;
    expr.kind = primec::Expr::Kind::Call;
    expr.name = "at";
    expr.args = {makeNameExpr(name, 0), makeLiteralExpr()};
    expr.semanticNodeId = semanticNodeId;
    return expr;
  };

  auto makeDereferenceExpr = [](primec::Expr target) {
    primec::Expr expr;
    expr.kind = primec::Expr::Kind::Call;
    expr.name = "dereference";
    expr.args = {std::move(target)};
    return expr;
  };

  auto makeMethodExpr = [](const std::string &methodName, primec::Expr receiver) {
    primec::Expr expr;
    expr.kind = primec::Expr::Kind::Call;
    expr.name = methodName;
    expr.isMethodCall = true;
    expr.args = {std::move(receiver)};
    return expr;
  };

  auto makeTryExpr = [](primec::Expr operand) {
    primec::Expr expr;
    expr.kind = primec::Expr::Kind::Call;
    expr.name = "try";
    expr.args = {std::move(operand)};
    return expr;
  };

  primec::SemanticProgram semanticProgram;
  addBindingFact(semanticProgram, 7501, "err", "/std/file/FileError");
  addQueryFact(semanticProgram, 7502, "file", "/std/file/File<Write>");
  addBindingFact(semanticProgram, 7503, "FileError", "i32");
  addLocalAutoFact(semanticProgram, 7504, "autoErr", "/std/file/FileError");
  addBindingFact(semanticProgram, 7505, "staleFile", "i32");
  addQueryFact(semanticProgram, 7506, "at", "/std/file/File<Write>");
  addQueryFact(semanticProgram, 7507, "at", "i32");
  addQueryFact(semanticProgram, 7508, "at", "/std/file/FileError");
  addQueryFact(semanticProgram, 7509, "at", "i32");
  const auto semanticIndex =
      primec::ir_lowerer::buildSemanticProductIndex(&semanticProgram);

  primec::ir_lowerer::LocalInfo staleFileHandle;
  staleFileHandle.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  staleFileHandle.isFileHandle = true;

  primec::ir_lowerer::LocalInfo staleBorrowedFileHandlePack;
  staleBorrowedFileHandlePack.isArgsPack = true;
  staleBorrowedFileHandlePack.isFileHandle = true;
  staleBorrowedFileHandlePack.argsPackElementKind =
      primec::ir_lowerer::LocalInfo::Kind::Reference;

  primec::ir_lowerer::LocalInfo staleBorrowedFileErrorPack;
  staleBorrowedFileErrorPack.isArgsPack = true;
  staleBorrowedFileErrorPack.isFileError = true;
  staleBorrowedFileErrorPack.argsPackElementKind =
      primec::ir_lowerer::LocalInfo::Kind::Reference;

  primec::ir_lowerer::LocalMap staleLocals;
  staleLocals.emplace("staleFile", staleFileHandle);
  staleLocals.emplace("files", staleBorrowedFileHandlePack);
  staleLocals.emplace("errors", staleBorrowedFileErrorPack);

  std::unordered_map<std::string, const primec::Definition *> defMap;
  std::string inferenceError;
  primec::ir_lowerer::LowerInferenceSetupBootstrapState state;
  state.semanticProgram = &semanticProgram;
  state.semanticIndex = &semanticIndex;
  installDispatchSetup(state, defMap, inferenceError);

  CHECK(state.inferExprKind(makeMethodExpr("why", makeNameExpr("err", 7501)), staleLocals) ==
        Kind::String);
  CHECK(state.inferExprKind(makeMethodExpr("why", makeNameExpr("autoErr", 7504)), staleLocals) ==
        Kind::String);
  CHECK(state.inferExprKind(makeMethodExpr("why", makeNameExpr("FileError", 7503)), staleLocals) ==
        Kind::Unknown);
  CHECK(state.inferExprKind(makeMethodExpr(
            "why",
            makeDereferenceExpr(makeAtExpr("errors", 7508))),
                            staleLocals) == Kind::String);
  CHECK(state.inferExprKind(makeMethodExpr(
            "why",
            makeDereferenceExpr(makeAtExpr("errors", 7509))),
                            staleLocals) == Kind::Unknown);
  CHECK(state.inferExprKind(makeTryExpr(makeMethodExpr("write", makeNameExpr("file", 7502))),
                            staleLocals) == Kind::Int32);
  CHECK(state.inferExprKind(makeTryExpr(makeMethodExpr("write", makeNameExpr("staleFile", 7505))),
                            staleLocals) == Kind::Unknown);
  CHECK(state.inferExprKind(
            makeTryExpr(makeMethodExpr(
                "flush",
                makeDereferenceExpr(makeAtExpr("files", 7506)))),
            staleLocals) == Kind::Int32);
  CHECK(state.inferExprKind(
            makeTryExpr(makeMethodExpr(
                "flush",
                makeDereferenceExpr(makeAtExpr("files", 7507)))),
            staleLocals) == Kind::Unknown);
  CHECK(state.inferExprKind(
            makeTryExpr(makeMethodExpr(
                "why",
                makeDereferenceExpr(makeAtExpr("errors", 7508)))),
            staleLocals) == Kind::String);
  CHECK(state.inferExprKind(
            makeTryExpr(makeMethodExpr(
                "why",
                makeDereferenceExpr(makeAtExpr("errors", 7509)))),
            staleLocals) == Kind::Unknown);
  CHECK(inferenceError.empty());

  std::unordered_map<std::string, const primec::Definition *> syntaxDefMap;
  std::string syntaxInferenceError;
  primec::ir_lowerer::LowerInferenceSetupBootstrapState syntaxState;
  installDispatchSetup(syntaxState, syntaxDefMap, syntaxInferenceError);
  CHECK(syntaxState.inferExprKind(makeMethodExpr("why", makeNameExpr("FileError", 0)),
                                  staleLocals) == Kind::String);
  CHECK(syntaxState.inferExprKind(makeTryExpr(makeMethodExpr("write", makeNameExpr("staleFile", 0))),
                                  staleLocals) == Kind::Int32);
  CHECK(syntaxState.inferExprKind(
            makeTryExpr(makeMethodExpr(
                "flush",
                makeDereferenceExpr(makeAtExpr("files", 0)))),
            staleLocals) == Kind::Int32);
  CHECK(syntaxInferenceError.empty());
}

TEST_CASE("ir lowerer inference expr-kind dispatch uses semantic File call facts") {
  using Kind = primec::ir_lowerer::LocalInfo::ValueKind;

  auto intern = [](primec::SemanticProgram &semanticProgram, const std::string &text) {
    return primec::semanticProgramInternCallTargetString(semanticProgram, text);
  };

  auto addQueryFact = [&](primec::SemanticProgram &semanticProgram,
                          uint64_t semanticNodeId,
                          const std::string &queryTypeText) {
    primec::SemanticProgramQueryFact fact;
    fact.scopePath = "/main";
    fact.callName = "File";
    fact.queryTypeText = queryTypeText;
    fact.bindingTypeText = queryTypeText;
    fact.semanticNodeId = semanticNodeId;
    fact.scopePathId = intern(semanticProgram, "/main");
    fact.callNameId = intern(semanticProgram, "File");
    fact.resolvedPathId = intern(semanticProgram, "/std/file/File");
    fact.queryTypeTextId = intern(semanticProgram, queryTypeText);
    fact.bindingTypeTextId = intern(semanticProgram, queryTypeText);
    const size_t index = semanticProgram.queryFacts.size();
    semanticProgram.queryFacts.push_back(std::move(fact));
    semanticProgram.publishedRoutingLookups.queryFactIndicesByExpr.insert_or_assign(
        semanticNodeId,
        index);
  };

  auto makeFileCallExpr = [](uint64_t semanticNodeId) {
    primec::Expr pathExpr;
    pathExpr.kind = primec::Expr::Kind::StringLiteral;
    pathExpr.stringValue = "tmp.txt";

    primec::Expr fileExpr;
    fileExpr.kind = primec::Expr::Kind::Call;
    fileExpr.name = "File";
    fileExpr.templateArgs = {"Read"};
    fileExpr.args = {pathExpr};
    fileExpr.semanticNodeId = semanticNodeId;
    return fileExpr;
  };

  auto makeTryExpr = [](primec::Expr operand) {
    primec::Expr expr;
    expr.kind = primec::Expr::Kind::Call;
    expr.name = "try";
    expr.args = {std::move(operand)};
    return expr;
  };

  auto installDispatchSetup =
      [](primec::ir_lowerer::LowerInferenceSetupBootstrapState &state,
         std::unordered_map<std::string, const primec::Definition *> &defMap,
         std::string &inferenceError) {
    state.inferLiteralOrNameExprKind = [](const primec::Expr &,
                                          const primec::ir_lowerer::LocalMap &,
                                          Kind &kindOut) {
      kindOut = Kind::Unknown;
      return false;
    };
    state.inferCallExprBaseKind = [](const primec::Expr &,
                                     const primec::ir_lowerer::LocalMap &,
                                     Kind &kindOut) {
      kindOut = Kind::Unknown;
      return false;
    };
    state.inferCallExprDirectReturnKind = [](const primec::Expr &,
                                             const primec::ir_lowerer::LocalMap &,
                                             Kind &kindOut) {
      kindOut = Kind::Unknown;
      return primec::ir_lowerer::CallExpressionReturnKindResolution::NotResolved;
    };
    state.inferCallExprCountAccessGpuFallbackKind = [](const primec::Expr &,
                                                       const primec::ir_lowerer::LocalMap &,
                                                       Kind &kindOut) {
      kindOut = Kind::Unknown;
      return false;
    };
    state.inferCallExprOperatorFallbackKind = [](const primec::Expr &,
                                                 const primec::ir_lowerer::LocalMap &,
                                                 Kind &kindOut) {
      kindOut = Kind::Unknown;
      return false;
    };
    state.inferCallExprControlFlowFallbackKind = [](const primec::Expr &,
                                                    const primec::ir_lowerer::LocalMap &,
                                                    std::string &,
                                                    Kind &kindOut) {
      kindOut = Kind::Unknown;
      return false;
    };
    state.inferCallExprPointerFallbackKind = [](const primec::Expr &,
                                                const primec::ir_lowerer::LocalMap &,
                                                Kind &kindOut) {
      kindOut = Kind::Unknown;
      return false;
    };
    state.resolveMethodCallDefinition =
        [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
      return nullptr;
    };
    std::string setupError;
    CHECK(primec::ir_lowerer::runLowerInferenceExprKindDispatchSetup(
        {
            .defMap = &defMap,
            .resolveExprPath = [](const primec::Expr &expr) { return "/" + expr.name; },
            .error = &inferenceError,
        },
        state,
        setupError));
    CHECK(setupError.empty());
    REQUIRE(static_cast<bool>(state.inferExprKind));
  };

  primec::SemanticProgram semanticProgram;
  addQueryFact(semanticProgram, 7601, "/std/file/File<Read>");
  addQueryFact(semanticProgram, 7602, "i32");
  const auto semanticIndex =
      primec::ir_lowerer::buildSemanticProductIndex(&semanticProgram);

  std::unordered_map<std::string, const primec::Definition *> defMap;
  std::string inferenceError;
  primec::ir_lowerer::LowerInferenceSetupBootstrapState state;
  state.semanticProgram = &semanticProgram;
  state.semanticIndex = &semanticIndex;
  installDispatchSetup(state, defMap, inferenceError);

  CHECK(state.inferExprKind(makeTryExpr(makeFileCallExpr(7601)),
                            primec::ir_lowerer::LocalMap{}) == Kind::Int64);
  CHECK(state.inferExprKind(makeTryExpr(makeFileCallExpr(7602)),
                            primec::ir_lowerer::LocalMap{}) == Kind::Unknown);
  CHECK(inferenceError.empty());

  std::unordered_map<std::string, const primec::Definition *> syntaxDefMap;
  std::string syntaxInferenceError;
  primec::ir_lowerer::LowerInferenceSetupBootstrapState syntaxState;
  installDispatchSetup(syntaxState, syntaxDefMap, syntaxInferenceError);
  CHECK(syntaxState.inferExprKind(makeTryExpr(makeFileCallExpr(0)),
                                  primec::ir_lowerer::LocalMap{}) == Kind::Int64);
  CHECK(syntaxInferenceError.empty());
}

TEST_CASE("ir lowerer inference expr-kind dispatch uses semantic Result method facts") {
  using Kind = primec::ir_lowerer::LocalInfo::ValueKind;

  auto intern = [](primec::SemanticProgram &semanticProgram, const std::string &text) {
    return primec::semanticProgramInternCallTargetString(semanticProgram, text);
  };

  auto addPayloadBindingFact = [&](primec::SemanticProgram &semanticProgram,
                                   uint64_t semanticNodeId,
                                   const std::string &name,
                                   const std::string &bindingTypeText) {
    primec::SemanticProgramBindingFact fact;
    fact.scopePath = "/main";
    fact.siteKind = "local";
    fact.name = name;
    fact.bindingTypeText = bindingTypeText;
    fact.semanticNodeId = semanticNodeId;
    fact.scopePathId = intern(semanticProgram, "/main");
    fact.siteKindId = intern(semanticProgram, "local");
    fact.nameId = intern(semanticProgram, name);
    fact.bindingTypeTextId = intern(semanticProgram, bindingTypeText);
    const size_t index = semanticProgram.bindingFacts.size();
    semanticProgram.bindingFacts.push_back(std::move(fact));
    semanticProgram.publishedRoutingLookups.bindingFactIndicesByExpr.insert_or_assign(
        semanticNodeId,
        index);
  };

  auto addPayloadQueryFact = [&](primec::SemanticProgram &semanticProgram,
                                 uint64_t semanticNodeId,
                                 const std::string &callName,
                                 const std::string &queryTypeText) {
    primec::SemanticProgramQueryFact fact;
    fact.scopePath = "/main";
    fact.callName = callName;
    fact.queryTypeText = queryTypeText;
    fact.bindingTypeText = queryTypeText;
    fact.semanticNodeId = semanticNodeId;
    fact.scopePathId = intern(semanticProgram, "/main");
    fact.callNameId = intern(semanticProgram, callName);
    fact.resolvedPathId = intern(semanticProgram, "/main/" + callName);
    fact.queryTypeTextId = intern(semanticProgram, queryTypeText);
    fact.bindingTypeTextId = intern(semanticProgram, queryTypeText);
    const size_t index = semanticProgram.queryFacts.size();
    semanticProgram.queryFacts.push_back(std::move(fact));
    semanticProgram.publishedRoutingLookups.queryFactIndicesByExpr.insert_or_assign(
        semanticNodeId,
        index);
  };

  auto installDispatchSetup = [](primec::ir_lowerer::LowerInferenceSetupBootstrapState &state,
                                 std::unordered_map<std::string, const primec::Definition *> &defMap,
                                 std::string &inferenceError) {
    state.inferLiteralOrNameExprKind = [](const primec::Expr &expr,
                                          const primec::ir_lowerer::LocalMap &locals,
                                          Kind &kindOut) {
      kindOut = Kind::Unknown;
      if (expr.kind != primec::Expr::Kind::Name) {
        return false;
      }
      auto it = locals.find(expr.name);
      if (it == locals.end() || it->second.kind != primec::ir_lowerer::LocalInfo::Kind::Value) {
        return false;
      }
      kindOut = it->second.valueKind;
      return true;
    };
    state.inferCallExprBaseKind = [](const primec::Expr &,
                                     const primec::ir_lowerer::LocalMap &,
                                     Kind &kindOut) {
      kindOut = Kind::Unknown;
      return false;
    };
    state.inferCallExprDirectReturnKind = [](const primec::Expr &,
                                             const primec::ir_lowerer::LocalMap &,
                                             Kind &kindOut) {
      kindOut = Kind::Unknown;
      return primec::ir_lowerer::CallExpressionReturnKindResolution::NotResolved;
    };
    state.inferCallExprCountAccessGpuFallbackKind = [](const primec::Expr &,
                                                       const primec::ir_lowerer::LocalMap &,
                                                       Kind &kindOut) {
      kindOut = Kind::Unknown;
      return false;
    };
    state.inferCallExprOperatorFallbackKind = [](const primec::Expr &,
                                                 const primec::ir_lowerer::LocalMap &,
                                                 Kind &kindOut) {
      kindOut = Kind::Unknown;
      return false;
    };
    state.inferCallExprControlFlowFallbackKind = [](const primec::Expr &,
                                                    const primec::ir_lowerer::LocalMap &,
                                                    std::string &,
                                                    Kind &kindOut) {
      kindOut = Kind::Unknown;
      return false;
    };
    state.inferCallExprPointerFallbackKind = [](const primec::Expr &,
                                                const primec::ir_lowerer::LocalMap &,
                                                Kind &kindOut) {
      kindOut = Kind::Unknown;
      return false;
    };
    state.resolveMethodCallDefinition =
        [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
      return nullptr;
    };
    std::string setupError;
    CHECK(primec::ir_lowerer::runLowerInferenceExprKindDispatchSetup(
        {
            .defMap = &defMap,
            .resolveExprPath = [](const primec::Expr &expr) { return "/" + expr.name; },
            .error = &inferenceError,
        },
        state,
        setupError));
    CHECK(setupError.empty());
    REQUIRE(static_cast<bool>(state.inferExprKind));
  };

  auto makeNameExpr = [](const std::string &name, uint64_t semanticNodeId) {
    primec::Expr expr;
    expr.kind = primec::Expr::Kind::Name;
    expr.name = name;
    expr.semanticNodeId = semanticNodeId;
    return expr;
  };

  auto makeCallExpr = [](const std::string &name, uint64_t semanticNodeId) {
    primec::Expr expr;
    expr.kind = primec::Expr::Kind::Call;
    expr.name = name;
    expr.semanticNodeId = semanticNodeId;
    return expr;
  };

  auto makeResultMethodExpr = [&](const std::string &methodName,
                                  std::vector<primec::Expr> args,
                                  uint64_t semanticNodeId) {
    primec::Expr expr;
    expr.kind = primec::Expr::Kind::Call;
    expr.name = methodName;
    expr.isMethodCall = true;
    expr.semanticNodeId = semanticNodeId;
    expr.args = {makeNameExpr("Result", 0)};
    for (auto &arg : args) {
      expr.args.push_back(std::move(arg));
    }
    return expr;
  };

  auto makeTryExpr = [](primec::Expr operand) {
    primec::Expr expr;
    expr.kind = primec::Expr::Kind::Call;
    expr.name = "try";
    expr.args = {std::move(operand)};
    return expr;
  };

  primec::SemanticProgram semanticProgram;
  addPayloadQueryFact(semanticProgram, 7601, "lookup", "bool");
  addPayloadBindingFact(semanticProgram, 7602, "payload", "bool");
  const auto semanticIndex =
      primec::ir_lowerer::buildSemanticProductIndex(&semanticProgram);

  primec::ir_lowerer::LocalInfo stalePayload;
  stalePayload.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  stalePayload.valueKind = Kind::Int64;
  const primec::ir_lowerer::LocalMap staleLocals{{"payload", stalePayload}};

  std::unordered_map<std::string, const primec::Definition *> defMap;
  std::string inferenceError;
  primec::ir_lowerer::LowerInferenceSetupBootstrapState state;
  state.semanticProgram = &semanticProgram;
  state.semanticIndex = &semanticIndex;
  installDispatchSetup(state, defMap, inferenceError);

  CHECK(state.inferExprKind(
            makeResultMethodExpr("ok", {makeCallExpr("lookup", 7601)}, 7603),
            staleLocals) == Kind::Bool);
  CHECK(state.inferExprKind(
            makeResultMethodExpr("ok", {makeNameExpr("payload", 7602)}, 7604),
            staleLocals) == Kind::Bool);
  CHECK(state.inferExprKind(
            makeTryExpr(makeResultMethodExpr("ok", {makeCallExpr("lookup", 7601)}, 7605)),
            staleLocals) == Kind::Bool);
  CHECK(state.inferExprKind(makeResultMethodExpr("error", {}, 7606), staleLocals) ==
        Kind::Unknown);
  CHECK(state.inferExprKind(makeResultMethodExpr("why", {}, 7607), staleLocals) ==
        Kind::Unknown);
  CHECK(inferenceError.empty());

  std::unordered_map<std::string, const primec::Definition *> syntaxDefMap;
  std::string syntaxInferenceError;
  primec::ir_lowerer::LowerInferenceSetupBootstrapState syntaxState;
  installDispatchSetup(syntaxState, syntaxDefMap, syntaxInferenceError);
  CHECK(syntaxState.inferExprKind(
            makeResultMethodExpr("ok", {makeNameExpr("payload", 0)}, 0),
            staleLocals) == Kind::Int64);
  CHECK(syntaxState.inferExprKind(makeResultMethodExpr("error", {}, 0), staleLocals) ==
        Kind::Bool);
  CHECK(syntaxState.inferExprKind(makeResultMethodExpr("why", {}, 0), staleLocals) ==
        Kind::String);
  CHECK(syntaxInferenceError.empty());
}

TEST_CASE("ir lowerer inference expr-kind dispatch setup validates dependencies") {
  primec::ir_lowerer::LowerInferenceSetupBootstrapState state;
  std::string inferenceError;
  std::string error;
  CHECK_FALSE(primec::ir_lowerer::runLowerInferenceExprKindDispatchSetup(
      {
          .error = &inferenceError,
      },
      state,
      error));
  CHECK(error == "native backend missing inference expr-kind dispatch setup dependency: defMap");
}

TEST_CASE("ir lowerer inference expr-kind call-fallback setup wires callback") {
  primec::ir_lowerer::LowerInferenceSetupBootstrapState state;
  state.inferBufferElementKind = [](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
    if (expr.kind == primec::Expr::Kind::Name && expr.name == "buf") {
      return primec::ir_lowerer::LocalInfo::ValueKind::Float32;
    }
    return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  };
  state.inferExprKind = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
    return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  };

  std::string error;
  CHECK(primec::ir_lowerer::runLowerInferenceExprKindCallFallbackSetup(
      {
          .isArrayCountCall = [](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
            return expr.kind == primec::Expr::Kind::Call && expr.name == "count";
          },
          .isStringCountCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
          .isVectorCapacityCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
          .isEntryArgsName = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
          .inferStructExprPath = [](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
            if (expr.kind == primec::Expr::Kind::Name && expr.name == "holder") {
              return std::string{"/pkg/Holder"};
            }
            return std::string{};
          },
          .resolveStructFieldSlot =
              [](const std::string &structPath,
                 const std::string &fieldName,
                 primec::ir_lowerer::StructSlotFieldInfo &fieldInfo) {
                if (structPath == "/pkg/Holder" && fieldName == "values") {
                  fieldInfo.structPath = "/std/collections/experimental_map/Map__ti32_i64";
                  fieldInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
                  return true;
                }
                return false;
              },
      },
      state,
      error));
  CHECK(error.empty());
  CHECK(static_cast<bool>(state.inferCallExprCountAccessGpuFallbackKind));

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo arrayInfo;
  arrayInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  arrayInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  locals.emplace("arr", arrayInfo);

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  primec::Expr countExpr;
  countExpr.kind = primec::Expr::Kind::Call;
  countExpr.name = "count";
  CHECK(state.inferCallExprCountAccessGpuFallbackKind(countExpr, locals, kindOut));
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Int32);

  primec::Expr arrayNameExpr;
  arrayNameExpr.kind = primec::Expr::Kind::Name;
  arrayNameExpr.name = "arr";
  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Literal;
  primec::Expr accessExpr;
  accessExpr.kind = primec::Expr::Kind::Call;
  accessExpr.name = "at";
  accessExpr.args.push_back(arrayNameExpr);
  accessExpr.args.push_back(indexExpr);
  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(state.inferCallExprCountAccessGpuFallbackKind(accessExpr, locals, kindOut));
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Int64);

  primec::Expr bufferNameExpr;
  bufferNameExpr.kind = primec::Expr::Kind::Name;
  bufferNameExpr.name = "buf";
  primec::Expr bufferLoadExpr;
  bufferLoadExpr.kind = primec::Expr::Kind::Call;
  bufferLoadExpr.name = "buffer_load";
  bufferLoadExpr.args.push_back(bufferNameExpr);
  bufferLoadExpr.args.push_back(indexExpr);
  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(state.inferCallExprCountAccessGpuFallbackKind(bufferLoadExpr, locals, kindOut));
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Float32);

  primec::Expr holderExpr;
  holderExpr.kind = primec::Expr::Kind::Name;
  holderExpr.name = "holder";
  primec::Expr valuesFieldExpr;
  valuesFieldExpr.kind = primec::Expr::Kind::Call;
  valuesFieldExpr.name = "values";
  valuesFieldExpr.isFieldAccess = true;
  valuesFieldExpr.args.push_back(holderExpr);
  primec::Expr mapCountExpr;
  mapCountExpr.kind = primec::Expr::Kind::Call;
  mapCountExpr.name = "count";
  mapCountExpr.args.push_back(valuesFieldExpr);
  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(state.inferCallExprCountAccessGpuFallbackKind(mapCountExpr, locals, kindOut));
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Int32);

  primec::Expr unknownExpr;
  unknownExpr.kind = primec::Expr::Kind::Call;
  unknownExpr.name = "noop";
  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  CHECK_FALSE(state.inferCallExprCountAccessGpuFallbackKind(unknownExpr, locals, kindOut));
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
}

TEST_CASE("ir lowerer inference expr-kind call-fallback setup validates dependencies") {
  primec::ir_lowerer::LowerInferenceSetupBootstrapState state;

  std::string error;
  CHECK_FALSE(primec::ir_lowerer::runLowerInferenceExprKindCallFallbackSetup(
      {
          .isArrayCountCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
          .isStringCountCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
          .isVectorCapacityCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
          .isEntryArgsName = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
          .inferStructExprPath =
              [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string{}; },
          .resolveStructFieldSlot =
              [](const std::string &, const std::string &, primec::ir_lowerer::StructSlotFieldInfo &) {
                return false;
              },
      },
      state,
      error));
  CHECK(error == "native backend missing inference expr-kind call-fallback setup state: inferBufferElementKind");
}

TEST_CASE("ir lowerer inference expr-kind call-operator fallback setup wires callback") {
  primec::ir_lowerer::LowerInferenceSetupBootstrapState state;
  state.inferExprKind = [](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
    if (expr.kind == primec::Expr::Kind::FloatLiteral) {
      return primec::ir_lowerer::LocalInfo::ValueKind::Float64;
    }
    return primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  };
  state.inferPointerTargetKind = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
    return primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  };

  std::string error;
  CHECK(primec::ir_lowerer::runLowerInferenceExprKindCallOperatorFallbackSetup(
      {
          .hasMathImport = true,
          .combineNumericKinds =
              [](primec::ir_lowerer::LocalInfo::ValueKind left, primec::ir_lowerer::LocalInfo::ValueKind right) {
                return (left == primec::ir_lowerer::LocalInfo::ValueKind::Float64 ||
                        right == primec::ir_lowerer::LocalInfo::ValueKind::Float64)
                           ? primec::ir_lowerer::LocalInfo::ValueKind::Float64
                           : primec::ir_lowerer::LocalInfo::ValueKind::Int32;
              },
      },
      state,
      error));
  CHECK(error.empty());
  CHECK(static_cast<bool>(state.inferCallExprOperatorFallbackKind));

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;

  primec::Expr comparisonExpr;
  comparisonExpr.kind = primec::Expr::Kind::Call;
  comparisonExpr.name = "less_than";
  CHECK(state.inferCallExprOperatorFallbackKind(comparisonExpr, primec::ir_lowerer::LocalMap{}, kindOut));
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Bool);

  primec::Expr floatArg;
  floatArg.kind = primec::Expr::Kind::FloatLiteral;
  floatArg.floatWidth = 64;
  primec::Expr mathExpr;
  mathExpr.kind = primec::Expr::Kind::Call;
  mathExpr.name = "std/math/abs";
  mathExpr.args.push_back(floatArg);
  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(state.inferCallExprOperatorFallbackKind(mathExpr, primec::ir_lowerer::LocalMap{}, kindOut));
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Float64);

  primec::Expr convertExpr;
  convertExpr.kind = primec::Expr::Kind::Call;
  convertExpr.name = "convert";
  convertExpr.templateArgs.push_back("u64");
  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(state.inferCallExprOperatorFallbackKind(convertExpr, primec::ir_lowerer::LocalMap{}, kindOut));
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::UInt64);

  primec::Expr unknownExpr;
  unknownExpr.kind = primec::Expr::Kind::Call;
  unknownExpr.name = "noop";
  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  CHECK_FALSE(state.inferCallExprOperatorFallbackKind(unknownExpr, primec::ir_lowerer::LocalMap{}, kindOut));
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
}

TEST_CASE("ir lowerer inference expr-kind call-operator fallback setup validates dependencies") {
  primec::ir_lowerer::LowerInferenceSetupBootstrapState state;
  state.inferPointerTargetKind = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
    return primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  };

  std::string error;
  CHECK_FALSE(primec::ir_lowerer::runLowerInferenceExprKindCallOperatorFallbackSetup(
      {
          .hasMathImport = true,
          .combineNumericKinds = {},
      },
      state,
      error));
  CHECK(error == "native backend missing inference expr-kind call-operator fallback setup dependency: combineNumericKinds");
}

TEST_CASE("ir lowerer inference expr-kind call-operator fallback setup validates state dependencies") {
  primec::ir_lowerer::LowerInferenceSetupBootstrapState state;

  std::string error;
  CHECK_FALSE(primec::ir_lowerer::runLowerInferenceExprKindCallOperatorFallbackSetup(
      {
          .hasMathImport = true,
          .combineNumericKinds =
              [](primec::ir_lowerer::LocalInfo::ValueKind, primec::ir_lowerer::LocalInfo::ValueKind) {
                return primec::ir_lowerer::LocalInfo::ValueKind::Int32;
              },
      },
      state,
      error));
  CHECK(error == "native backend missing inference expr-kind call-operator fallback setup state: inferPointerTargetKind");
}

TEST_CASE("ir lowerer inference expr-kind call-control-flow fallback setup wires callback") {
  std::unordered_map<std::string, const primec::Definition *> defMap;
  primec::ir_lowerer::LowerInferenceSetupBootstrapState state;
  state.inferExprKind = [](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
    if (expr.kind == primec::Expr::Kind::BoolLiteral) {
      return primec::ir_lowerer::LocalInfo::ValueKind::Bool;
    }
    if (expr.kind == primec::Expr::Kind::Literal) {
      return primec::ir_lowerer::LocalInfo::ValueKind::Int32;
    }
    return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  };

  std::string error;
  CHECK(primec::ir_lowerer::runLowerInferenceExprKindCallControlFlowFallbackSetup(
      {
          .defMap = &defMap,
          .resolveExprPath = [](const primec::Expr &) { return std::string(); },
          .lowerMatchToIf = [](const primec::Expr &, primec::Expr &, std::string &) { return true; },
          .combineNumericKinds =
              [](primec::ir_lowerer::LocalInfo::ValueKind left, primec::ir_lowerer::LocalInfo::ValueKind right) {
                return (left == primec::ir_lowerer::LocalInfo::ValueKind::Float64 ||
                        right == primec::ir_lowerer::LocalInfo::ValueKind::Float64)
                           ? primec::ir_lowerer::LocalInfo::ValueKind::Float64
                           : primec::ir_lowerer::LocalInfo::ValueKind::Int32;
              },
          .isBindingMutable = [](const primec::Expr &) { return false; },
          .bindingKind = [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::Kind::Value; },
          .hasExplicitBindingTypeTransform = [](const primec::Expr &) { return false; },
          .bindingValueKind = [](const primec::Expr &, primec::ir_lowerer::LocalInfo::Kind) {
            return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
          },
          .applyStructArrayInfo = [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
          .applyStructValueInfo = [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
          .inferStructExprPath = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
            return std::string();
          },
      },
      state,
      error));
  CHECK(error.empty());
  CHECK(static_cast<bool>(state.inferCallExprControlFlowFallbackKind));

  primec::Expr conditionExpr;
  conditionExpr.kind = primec::Expr::Kind::BoolLiteral;
  conditionExpr.boolValue = true;

  primec::Expr thenExpr;
  thenExpr.kind = primec::Expr::Kind::Literal;

  primec::Expr elseExpr;
  elseExpr.kind = primec::Expr::Kind::Literal;

  primec::Expr ifExpr;
  ifExpr.kind = primec::Expr::Kind::Call;
  ifExpr.name = "if";
  ifExpr.args.push_back(conditionExpr);
  ifExpr.args.push_back(thenExpr);
  ifExpr.args.push_back(elseExpr);

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(state.inferCallExprControlFlowFallbackKind(ifExpr, primec::ir_lowerer::LocalMap{}, error, kindOut));
  CHECK(error.empty());
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Int32);

  primec::Expr unknownExpr;
  unknownExpr.kind = primec::Expr::Kind::Call;
  unknownExpr.name = "noop";
  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  CHECK_FALSE(state.inferCallExprControlFlowFallbackKind(unknownExpr, primec::ir_lowerer::LocalMap{}, error, kindOut));
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
}

TEST_CASE("ir lowerer inference expr-kind call-control-flow fallback setup validates dependencies") {
  std::unordered_map<std::string, const primec::Definition *> defMap;
  primec::ir_lowerer::LowerInferenceSetupBootstrapState state;

  std::string error;
  CHECK_FALSE(primec::ir_lowerer::runLowerInferenceExprKindCallControlFlowFallbackSetup(
      {
          .defMap = &defMap,
          .resolveExprPath = {},
          .lowerMatchToIf = [](const primec::Expr &, primec::Expr &, std::string &) { return true; },
          .combineNumericKinds =
              [](primec::ir_lowerer::LocalInfo::ValueKind, primec::ir_lowerer::LocalInfo::ValueKind) {
                return primec::ir_lowerer::LocalInfo::ValueKind::Int32;
              },
          .isBindingMutable = [](const primec::Expr &) { return false; },
          .bindingKind = [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::Kind::Value; },
          .hasExplicitBindingTypeTransform = [](const primec::Expr &) { return false; },
          .bindingValueKind = [](const primec::Expr &, primec::ir_lowerer::LocalInfo::Kind) {
            return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
          },
          .applyStructArrayInfo = [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
          .applyStructValueInfo = [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
          .inferStructExprPath = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
            return std::string();
          },
      },
      state,
      error));
  CHECK(error == "native backend missing inference expr-kind call-control-flow fallback setup dependency: resolveExprPath");
}

TEST_CASE("ir lowerer inference expr-kind call-pointer fallback setup wires callback") {
  primec::ir_lowerer::LowerInferenceSetupBootstrapState state;
  state.inferPointerTargetKind = [](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
    if (expr.kind == primec::Expr::Kind::Name && expr.name == "ptr") {
      return primec::ir_lowerer::LocalInfo::ValueKind::Int64;
    }
    return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  };

  std::string error;
  CHECK(primec::ir_lowerer::runLowerInferenceExprKindCallPointerFallbackSetup(
      {},
      state,
      error));
  CHECK(error.empty());
  CHECK(static_cast<bool>(state.inferCallExprPointerFallbackKind));

  primec::Expr pointerExpr;
  pointerExpr.kind = primec::Expr::Kind::Name;
  pointerExpr.name = "ptr";

  primec::Expr dereferenceExpr;
  dereferenceExpr.kind = primec::Expr::Kind::Call;
  dereferenceExpr.name = "dereference";
  dereferenceExpr.args.push_back(pointerExpr);

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(state.inferCallExprPointerFallbackKind(dereferenceExpr, primec::ir_lowerer::LocalMap{}, kindOut));
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Int64);

  primec::Expr unknownExpr;
  unknownExpr.kind = primec::Expr::Kind::Call;
  unknownExpr.name = "noop";
  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  CHECK_FALSE(state.inferCallExprPointerFallbackKind(unknownExpr, primec::ir_lowerer::LocalMap{}, kindOut));
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
}

TEST_CASE("ir lowerer inference expr-kind call-pointer fallback setup validates state dependencies") {
  primec::ir_lowerer::LowerInferenceSetupBootstrapState state;

  std::string error;
  CHECK_FALSE(primec::ir_lowerer::runLowerInferenceExprKindCallPointerFallbackSetup(
      {},
      state,
      error));
  CHECK(error == "native backend missing inference expr-kind call-pointer fallback setup state: inferPointerTargetKind");
}

TEST_CASE("ir lowerer return/calls setup wires emitFileErrorWhy callback") {
  primec::IrFunction function;
  function.name = "/main";
  primec::ir_lowerer::LowerReturnCallsEmitFileErrorWhyFn emitFileErrorWhy;
  int32_t internedCount = 0;
  std::string error;

  CHECK(primec::ir_lowerer::runLowerReturnCallsSetup(
      {
          .function = &function,
          .internString = [&](const std::string &) {
            return internedCount++;
          },
      },
      emitFileErrorWhy,
      error));
  CHECK(error.empty());
  CHECK(static_cast<bool>(emitFileErrorWhy));

  const size_t before = function.instructions.size();
  CHECK(emitFileErrorWhy(7));
  CHECK(function.instructions.size() > before);
}

TEST_CASE("ir lowerer return/calls setup validates dependencies") {
  primec::IrFunction function;
  primec::ir_lowerer::LowerReturnCallsEmitFileErrorWhyFn emitFileErrorWhy;
  std::string error;

  CHECK_FALSE(primec::ir_lowerer::runLowerReturnCallsSetup(
      {
          .function = nullptr,
          .internString = [&](const std::string &) { return 0; },
      },
      emitFileErrorWhy,
      error));
  CHECK(error == "native backend missing return/calls setup dependency: function");
  CHECK_FALSE(static_cast<bool>(emitFileErrorWhy));

  CHECK_FALSE(primec::ir_lowerer::runLowerReturnCallsSetup(
      {
          .function = &function,
          .internString = {},
      },
      emitFileErrorWhy,
      error));
  CHECK(error == "native backend missing return/calls setup dependency: internString");
  CHECK_FALSE(static_cast<bool>(emitFileErrorWhy));
}

TEST_CASE("ir lowerer statements/calls step emits assign-or-expr fallback") {
  primec::Expr stmt;
  stmt.kind = primec::Expr::Kind::Name;
  stmt.name = "value";

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo valueInfo;
  valueInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  valueInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  valueInfo.index = 7;
  locals.emplace("value", valueInfo);

  int emitExprCalls = 0;
  int allocTempCalls = 0;
  std::vector<primec::IrInstruction> instructions;
  std::string error;
  CHECK(primec::ir_lowerer::runLowerStatementsCallsStep(
      {
          .inferExprKind = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
            return primec::ir_lowerer::LocalInfo::ValueKind::Int32;
          },
          .inferStructExprPath = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
            return std::string();
          },
          .emitExpr =
              [&](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &localsIn) {
                ++emitExprCalls;
                auto it = localsIn.find(expr.name);
                if (it == localsIn.end()) {
                  return false;
                }
                instructions.push_back({primec::IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
                return true;
              },
          .allocTempLocal = [&]() { return allocTempCalls++; },
          .resolveExprPath = [](const primec::Expr &) { return std::string(); },
          .findDefinitionByPath = [](const std::string &) { return static_cast<const primec::Definition *>(nullptr); },
          .resolveDestroyHelperForStruct =
              [](const std::string &) { return static_cast<const primec::Definition *>(nullptr); },
          .resolveMoveHelperForStruct =
              [](const std::string &) { return static_cast<const primec::Definition *>(nullptr); },
          .isArrayCountCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
          .isStringCountCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
          .isVectorCapacityCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
          .resolveMethodCallDefinition =
              [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
                return static_cast<const primec::Definition *>(nullptr);
              },
          .resolveDefinitionCall = [](const primec::Expr &) { return static_cast<const primec::Definition *>(nullptr); },
          .getReturnInfo = [](const std::string &, primec::ir_lowerer::ReturnInfo &) { return false; },
          .emitInlineDefinitionCall =
              [](const primec::Expr &, const primec::Definition &, const primec::ir_lowerer::LocalMap &, bool) {
                return false;
              },
          .emitArrayIndexOutOfBounds = []() {},
          .emitVectorCapacityExceeded = []() {},
          .emitVectorPopOnEmpty = []() {},
          .emitVectorIndexOutOfBounds = []() {},
          .emitVectorReserveNegative = []() {},
          .emitVectorReserveExceeded = []() {},
          .instructions = &instructions,
      },
      stmt,
      locals,
      error));
  CHECK(error.empty());
  CHECK(emitExprCalls == 1);
  CHECK(allocTempCalls == 0);
  REQUIRE(instructions.size() == 2);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[1].op == primec::IrOpcode::Pop);
}


TEST_SUITE_END();
