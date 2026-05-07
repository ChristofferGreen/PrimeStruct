#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir lowerer inference call-return setup resolves namespaced capacity definition directly") {
  primec::Definition receiverCapacityDef;
  receiverCapacityDef.fullPath = "/vector/capacity";
  primec::Definition canonicalCapacityDef;
  canonicalCapacityDef.fullPath = "/std/collections/vector/capacity";
  std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/std/collections/vector/capacity", &canonicalCapacityDef},
  };

  bool resolveReceiverHelper = true;
  int resolveMethodCalls = 0;
  primec::ir_lowerer::LowerInferenceSetupBootstrapState state;
  state.getReturnInfo = [](const std::string &path, primec::ir_lowerer::ReturnInfo &out) {
    out.returnsVoid = false;
    out.returnsArray = false;
    if (path == "/vector/capacity") {
      out.kind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
      return true;
    }
    if (path == "/std/collections/vector/capacity") {
      out.kind = primec::ir_lowerer::LocalInfo::ValueKind::UInt64;
      return true;
    }
    return false;
  };
  state.resolveMethodCallDefinition =
      [&](const primec::Expr &methodExpr, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
    ++resolveMethodCalls;
    if (!resolveReceiverHelper || !methodExpr.isMethodCall || methodExpr.name != "capacity" || methodExpr.args.empty()) {
      return nullptr;
    }
    if (methodExpr.args.front().kind == primec::Expr::Kind::Name && methodExpr.args.front().name == "values") {
      return &receiverCapacityDef;
    }
    return nullptr;
  };

  std::string error;
  CHECK(primec::ir_lowerer::runLowerInferenceExprKindCallReturnSetup(
      {
          .defMap = &defMap,
          .resolveExprPath = [](const primec::Expr &expr) { return expr.name; },
          .isArrayCountCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
          .isStringCountCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      },
      state,
      error));
  CHECK(error.empty());

  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "values";
  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "/std/collections/vector/capacity";
  callExpr.args = {receiverExpr};

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(state.inferCallExprDirectReturnKind(callExpr, {}, kindOut) ==
        primec::ir_lowerer::CallExpressionReturnKindResolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::UInt64);
  CHECK(resolveMethodCalls == 0);

  resolveReceiverHelper = false;
  resolveMethodCalls = 0;
  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(state.inferCallExprDirectReturnKind(callExpr, {}, kindOut) ==
        primec::ir_lowerer::CallExpressionReturnKindResolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::UInt64);
  CHECK(resolveMethodCalls == 0);
}

TEST_CASE("ir lowerer inference call-return setup resolves namespaced access definition directly") {
  primec::Definition receiverAtDef;
  receiverAtDef.fullPath = "/vector/at";
  primec::Definition canonicalAtDef;
  canonicalAtDef.fullPath = "/std/collections/vector/at";
  std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/std/collections/vector/at", &canonicalAtDef},
  };

  bool resolveReceiverHelper = true;
  int resolveMethodCalls = 0;
  primec::ir_lowerer::LowerInferenceSetupBootstrapState state;
  state.getReturnInfo = [](const std::string &path, primec::ir_lowerer::ReturnInfo &out) {
    out.returnsVoid = false;
    out.returnsArray = false;
    if (path == "/vector/at") {
      out.kind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
      return true;
    }
    if (path == "/std/collections/vector/at") {
      out.kind = primec::ir_lowerer::LocalInfo::ValueKind::UInt64;
      return true;
    }
    return false;
  };
  state.resolveMethodCallDefinition =
      [&](const primec::Expr &methodExpr, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
    ++resolveMethodCalls;
    if (!resolveReceiverHelper || !methodExpr.isMethodCall || methodExpr.name != "at" || methodExpr.args.empty()) {
      return nullptr;
    }
    if (methodExpr.args.front().kind == primec::Expr::Kind::Name && methodExpr.args.front().name == "values") {
      return &receiverAtDef;
    }
    return nullptr;
  };

  std::string error;
  CHECK(primec::ir_lowerer::runLowerInferenceExprKindCallReturnSetup(
      {
          .defMap = &defMap,
          .resolveExprPath = [](const primec::Expr &expr) { return expr.name; },
          .isArrayCountCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
          .isStringCountCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      },
      state,
      error));
  CHECK(error.empty());

  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "values";
  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Literal;
  indexExpr.intWidth = 32;
  indexExpr.literalValue = 1;
  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "/std/collections/vector/at";
  callExpr.args = {receiverExpr, indexExpr};

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(state.inferCallExprDirectReturnKind(callExpr, {}, kindOut) ==
        primec::ir_lowerer::CallExpressionReturnKindResolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::UInt64);
  CHECK(resolveMethodCalls == 0);

  resolveReceiverHelper = false;
  resolveMethodCalls = 0;
  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(state.inferCallExprDirectReturnKind(callExpr, {}, kindOut) ==
        primec::ir_lowerer::CallExpressionReturnKindResolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::UInt64);
  CHECK(resolveMethodCalls == 0);
}

TEST_CASE("ir lowerer inference call-return setup uses semantic access receiver facts before stale locals") {
  std::unordered_map<std::string, const primec::Definition *> defMap;

  primec::SemanticProgram semanticProgram;
  const primec::SymbolId vectorTypeId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "vector<f32>");
  primec::SemanticProgramBindingFact bindingFact;
  bindingFact.semanticNodeId = 9101;
  bindingFact.bindingTypeText = "map<i32,i64>";
  bindingFact.bindingTypeTextId = vectorTypeId;
  semanticProgram.bindingFacts.push_back(bindingFact);
  semanticProgram.publishedRoutingLookups.bindingFactIndicesByExpr.insert_or_assign(
      bindingFact.semanticNodeId, 0);
  const primec::ir_lowerer::SemanticProductIndex semanticIndex =
      primec::ir_lowerer::buildSemanticProductIndex(&semanticProgram);

  primec::ir_lowerer::LowerInferenceSetupBootstrapState state;
  state.semanticProgram = &semanticProgram;
  state.semanticIndex = &semanticIndex;
  state.getReturnInfo = [](const std::string &, primec::ir_lowerer::ReturnInfo &) {
    return false;
  };
  state.resolveMethodCallDefinition =
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
    return nullptr;
  };

  std::string error;
  CHECK(primec::ir_lowerer::runLowerInferenceExprKindCallReturnSetup(
      {
          .defMap = &defMap,
          .resolveExprPath = [](const primec::Expr &expr) { return expr.name; },
          .isArrayCountCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
          .isStringCountCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      },
      state,
      error));
  CHECK(error.empty());

  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "values";
  receiverExpr.semanticNodeId = bindingFact.semanticNodeId;
  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Literal;
  indexExpr.intWidth = 32;
  indexExpr.literalValue = 1;
  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "at";
  callExpr.args = {receiverExpr, indexExpr};

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo staleLocal;
  staleLocal.kind = primec::ir_lowerer::LocalInfo::Kind::Map;
  staleLocal.mapKeyKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  staleLocal.mapValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  locals.emplace("values", staleLocal);

  primec::ir_lowerer::LocalInfo::ValueKind kindOut =
      primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(state.inferCallExprDirectReturnKind(callExpr, locals, kindOut) ==
        primec::ir_lowerer::CallExpressionReturnKindResolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Float32);
}

TEST_CASE("ir lowerer inference call-return setup uses semantic access reorder facts before local metadata") {
  std::unordered_map<std::string, const primec::Definition *> defMap;

  primec::SemanticProgram semanticProgram;
  const primec::SymbolId scalarTypeId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "i32");
  primec::SemanticProgramBindingFact scalarFact;
  scalarFact.semanticNodeId = 9601;
  scalarFact.bindingTypeText = "map<i32,i64>";
  scalarFact.bindingTypeTextId = scalarTypeId;
  semanticProgram.bindingFacts.push_back(scalarFact);
  semanticProgram.publishedRoutingLookups.bindingFactIndicesByExpr.insert_or_assign(
      scalarFact.semanticNodeId, 0);

  const primec::SymbolId mapTypeId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "map<i32,f32>");
  primec::SemanticProgramBindingFact mapFact;
  mapFact.semanticNodeId = 9602;
  mapFact.bindingTypeText = "i32";
  mapFact.bindingTypeTextId = mapTypeId;
  semanticProgram.bindingFacts.push_back(mapFact);
  semanticProgram.publishedRoutingLookups.bindingFactIndicesByExpr.insert_or_assign(
      mapFact.semanticNodeId, 1);

  const primec::ir_lowerer::SemanticProductIndex semanticIndex =
      primec::ir_lowerer::buildSemanticProductIndex(&semanticProgram);

  primec::ir_lowerer::LowerInferenceSetupBootstrapState state;
  state.semanticProgram = &semanticProgram;
  state.semanticIndex = &semanticIndex;
  state.getReturnInfo = [](const std::string &, primec::ir_lowerer::ReturnInfo &) {
    return false;
  };
  state.resolveMethodCallDefinition =
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
    return nullptr;
  };

  std::string error;
  CHECK(primec::ir_lowerer::runLowerInferenceExprKindCallReturnSetup(
      {
          .defMap = &defMap,
          .resolveExprPath = [](const primec::Expr &expr) { return expr.name; },
          .isArrayCountCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
          .isStringCountCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      },
      state,
      error));
  CHECK(error.empty());

  primec::Expr candidateExpr;
  candidateExpr.kind = primec::Expr::Kind::Name;
  candidateExpr.name = "candidate";
  candidateExpr.semanticNodeId = scalarFact.semanticNodeId;
  primec::Expr valuesExpr;
  valuesExpr.kind = primec::Expr::Kind::Name;
  valuesExpr.name = "values";
  valuesExpr.semanticNodeId = mapFact.semanticNodeId;
  primec::Expr atExpr;
  atExpr.kind = primec::Expr::Kind::Call;
  atExpr.name = "at";
  atExpr.args = {candidateExpr, valuesExpr};

  primec::ir_lowerer::LocalInfo::ValueKind kindOut =
      primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(state.inferCallExprDirectReturnKind(atExpr, {}, kindOut) ==
        primec::ir_lowerer::CallExpressionReturnKindResolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Float32);

  primec::Expr keyExpr;
  keyExpr.kind = primec::Expr::Kind::Literal;
  keyExpr.intWidth = 32;
  keyExpr.literalValue = 1;
  atExpr.args = {candidateExpr, keyExpr};

  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(state.inferCallExprDirectReturnKind(atExpr, {}, kindOut) ==
        primec::ir_lowerer::CallExpressionReturnKindResolution::NotResolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
}

TEST_CASE("ir lowerer inference call-return setup uses semantic access stale facts before local metadata") {
  primec::Definition staleAtDef;
  staleAtDef.fullPath = "/map/at";
  std::unordered_map<std::string, const primec::Definition *> defMap;

  primec::SemanticProgram semanticProgram;
  const primec::SymbolId scalarTypeId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "i32");
  primec::SemanticProgramBindingFact scalarFact;
  scalarFact.semanticNodeId = 9701;
  scalarFact.bindingTypeText = "map<i32,i64>";
  scalarFact.bindingTypeTextId = scalarTypeId;
  semanticProgram.bindingFacts.push_back(scalarFact);
  semanticProgram.publishedRoutingLookups.bindingFactIndicesByExpr.insert_or_assign(
      scalarFact.semanticNodeId, 0);

  const primec::SymbolId stringTypeId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "string");
  primec::SemanticProgramBindingFact stringFact;
  stringFact.semanticNodeId = 9702;
  stringFact.bindingTypeText = "map<i32,i64>";
  stringFact.bindingTypeTextId = stringTypeId;
  semanticProgram.bindingFacts.push_back(stringFact);
  semanticProgram.publishedRoutingLookups.bindingFactIndicesByExpr.insert_or_assign(
      stringFact.semanticNodeId, 1);

  const primec::ir_lowerer::SemanticProductIndex semanticIndex =
      primec::ir_lowerer::buildSemanticProductIndex(&semanticProgram);

  int staleMapResolutions = 0;
  primec::ir_lowerer::LowerInferenceSetupBootstrapState state;
  state.semanticProgram = &semanticProgram;
  state.semanticIndex = &semanticIndex;
  state.getReturnInfo = [](const std::string &path, primec::ir_lowerer::ReturnInfo &out) {
    if (path != "/map/at") {
      return false;
    }
    out.returnsVoid = false;
    out.returnsArray = false;
    out.kind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
    return true;
  };
  state.resolveMethodCallDefinition =
      [&](const primec::Expr &methodExpr, const primec::ir_lowerer::LocalMap &localsIn) -> const primec::Definition * {
    if (!methodExpr.isMethodCall || methodExpr.name != "at" ||
        methodExpr.args.empty() ||
        methodExpr.args.front().kind != primec::Expr::Kind::Name) {
      return nullptr;
    }
    const auto localIt = localsIn.find(methodExpr.args.front().name);
    if (localIt == localsIn.end() ||
        localIt->second.kind != primec::ir_lowerer::LocalInfo::Kind::Map) {
      return nullptr;
    }
    ++staleMapResolutions;
    return &staleAtDef;
  };

  std::string error;
  CHECK(primec::ir_lowerer::runLowerInferenceExprKindCallReturnSetup(
      {
          .defMap = &defMap,
          .resolveExprPath = [](const primec::Expr &expr) { return expr.name; },
          .isArrayCountCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
          .isStringCountCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      },
      state,
      error));
  CHECK(error.empty());

  primec::Expr keyExpr;
  keyExpr.kind = primec::Expr::Kind::Literal;
  keyExpr.intWidth = 32;
  keyExpr.literalValue = 1;

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo staleMapLocal;
  staleMapLocal.kind = primec::ir_lowerer::LocalInfo::Kind::Map;
  staleMapLocal.mapKeyKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  staleMapLocal.mapValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  locals.emplace("candidate", staleMapLocal);
  locals.emplace("text", staleMapLocal);

  primec::Expr candidateExpr;
  candidateExpr.kind = primec::Expr::Kind::Name;
  candidateExpr.name = "candidate";
  candidateExpr.semanticNodeId = scalarFact.semanticNodeId;
  primec::Expr atExpr;
  atExpr.kind = primec::Expr::Kind::Call;
  atExpr.name = "at";
  atExpr.args = {candidateExpr, keyExpr};

  primec::ir_lowerer::LocalInfo::ValueKind kindOut =
      primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(state.inferCallExprDirectReturnKind(atExpr, locals, kindOut) ==
        primec::ir_lowerer::CallExpressionReturnKindResolution::NotResolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);

  primec::ir_lowerer::LocalInfo staleStringLocal;
  staleStringLocal.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  staleStringLocal.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::String;
  locals.insert_or_assign("candidate", staleStringLocal);

  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(state.inferCallExprDirectReturnKind(atExpr, locals, kindOut) ==
        primec::ir_lowerer::CallExpressionReturnKindResolution::NotResolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);

  primec::Expr textExpr;
  textExpr.kind = primec::Expr::Kind::Name;
  textExpr.name = "text";
  textExpr.semanticNodeId = stringFact.semanticNodeId;
  atExpr.args = {textExpr, keyExpr};

  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(state.inferCallExprDirectReturnKind(atExpr, locals, kindOut) ==
        primec::ir_lowerer::CallExpressionReturnKindResolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  CHECK(staleMapResolutions == 0);
}

TEST_CASE("ir lowerer inference call-return setup uses semantic contains receiver facts before local metadata") {
  primec::Definition staleContainsDef;
  staleContainsDef.fullPath = "/map/contains";
  std::unordered_map<std::string, const primec::Definition *> defMap;

  primec::SemanticProgram semanticProgram;
  const primec::SymbolId scalarTypeId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "i32");
  primec::SemanticProgramBindingFact scalarFact;
  scalarFact.semanticNodeId = 9801;
  scalarFact.bindingTypeText = "map<i32,i64>";
  scalarFact.bindingTypeTextId = scalarTypeId;
  semanticProgram.bindingFacts.push_back(scalarFact);
  semanticProgram.publishedRoutingLookups.bindingFactIndicesByExpr.insert_or_assign(
      scalarFact.semanticNodeId, 0);

  const primec::SymbolId mapTypeId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "map<i32,f32>");
  primec::SemanticProgramBindingFact mapFact;
  mapFact.semanticNodeId = 9802;
  mapFact.bindingTypeText = "i32";
  mapFact.bindingTypeTextId = mapTypeId;
  semanticProgram.bindingFacts.push_back(mapFact);
  semanticProgram.publishedRoutingLookups.bindingFactIndicesByExpr.insert_or_assign(
      mapFact.semanticNodeId, 1);

  const primec::ir_lowerer::SemanticProductIndex semanticIndex =
      primec::ir_lowerer::buildSemanticProductIndex(&semanticProgram);

  int staleMapResolutions = 0;
  primec::ir_lowerer::LowerInferenceSetupBootstrapState state;
  state.semanticProgram = &semanticProgram;
  state.semanticIndex = &semanticIndex;
  state.getReturnInfo = [](const std::string &path, primec::ir_lowerer::ReturnInfo &out) {
    if (path != "/map/contains") {
      return false;
    }
    out.returnsVoid = false;
    out.returnsArray = false;
    out.kind = primec::ir_lowerer::LocalInfo::ValueKind::Bool;
    return true;
  };
  state.resolveMethodCallDefinition =
      [&](const primec::Expr &methodExpr, const primec::ir_lowerer::LocalMap &localsIn) -> const primec::Definition * {
    if (!methodExpr.isMethodCall || methodExpr.name != "contains" ||
        methodExpr.args.empty() ||
        methodExpr.args.front().kind != primec::Expr::Kind::Name) {
      return nullptr;
    }
    const auto localIt = localsIn.find(methodExpr.args.front().name);
    if (localIt == localsIn.end() ||
        localIt->second.kind != primec::ir_lowerer::LocalInfo::Kind::Map) {
      return nullptr;
    }
    ++staleMapResolutions;
    return &staleContainsDef;
  };

  std::string error;
  CHECK(primec::ir_lowerer::runLowerInferenceExprKindCallReturnSetup(
      {
          .defMap = &defMap,
          .resolveExprPath = [](const primec::Expr &expr) { return expr.name; },
          .isArrayCountCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
          .isStringCountCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      },
      state,
      error));
  CHECK(error.empty());

  primec::Expr keyExpr;
  keyExpr.kind = primec::Expr::Kind::Literal;
  keyExpr.intWidth = 32;
  keyExpr.literalValue = 1;

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo staleMapLocal;
  staleMapLocal.kind = primec::ir_lowerer::LocalInfo::Kind::Map;
  staleMapLocal.mapKeyKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  staleMapLocal.mapValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  locals.emplace("candidate", staleMapLocal);
  locals.emplace("values", staleMapLocal);

  primec::Expr candidateExpr;
  candidateExpr.kind = primec::Expr::Kind::Name;
  candidateExpr.name = "candidate";
  candidateExpr.semanticNodeId = scalarFact.semanticNodeId;
  primec::Expr containsExpr;
  containsExpr.kind = primec::Expr::Kind::Call;
  containsExpr.name = "contains";
  containsExpr.args = {candidateExpr, keyExpr};

  primec::ir_lowerer::LocalInfo::ValueKind kindOut =
      primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(state.inferCallExprDirectReturnKind(containsExpr, locals, kindOut) ==
        primec::ir_lowerer::CallExpressionReturnKindResolution::NotResolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);

  primec::Expr valuesExpr;
  valuesExpr.kind = primec::Expr::Kind::Name;
  valuesExpr.name = "values";
  valuesExpr.semanticNodeId = mapFact.semanticNodeId;
  containsExpr.args = {valuesExpr, keyExpr};

  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(state.inferCallExprDirectReturnKind(containsExpr, locals, kindOut) ==
        primec::ir_lowerer::CallExpressionReturnKindResolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Bool);
  CHECK(staleMapResolutions == 0);
}

TEST_CASE("ir lowerer inference call-return setup uses semantic count receiver facts before local metadata") {
  primec::Definition staleCountDef;
  staleCountDef.fullPath = "/map/count";
  std::unordered_map<std::string, const primec::Definition *> defMap;

  primec::SemanticProgram semanticProgram;
  const primec::SymbolId scalarTypeId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "i32");
  primec::SemanticProgramBindingFact scalarFact;
  scalarFact.semanticNodeId = 9901;
  scalarFact.bindingTypeText = "map<i32,i64>";
  scalarFact.bindingTypeTextId = scalarTypeId;
  semanticProgram.bindingFacts.push_back(scalarFact);
  semanticProgram.publishedRoutingLookups.bindingFactIndicesByExpr.insert_or_assign(
      scalarFact.semanticNodeId, 0);

  const primec::SymbolId stringTypeId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "string");
  primec::SemanticProgramBindingFact stringFact;
  stringFact.semanticNodeId = 9902;
  stringFact.bindingTypeText = "map<i32,i64>";
  stringFact.bindingTypeTextId = stringTypeId;
  semanticProgram.bindingFacts.push_back(stringFact);
  semanticProgram.publishedRoutingLookups.bindingFactIndicesByExpr.insert_or_assign(
      stringFact.semanticNodeId, 1);

  const primec::SymbolId mapTypeId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "map<i32,f32>");
  primec::SemanticProgramBindingFact mapFact;
  mapFact.semanticNodeId = 9903;
  mapFact.bindingTypeText = "i32";
  mapFact.bindingTypeTextId = mapTypeId;
  semanticProgram.bindingFacts.push_back(mapFact);
  semanticProgram.publishedRoutingLookups.bindingFactIndicesByExpr.insert_or_assign(
      mapFact.semanticNodeId, 2);

  const primec::ir_lowerer::SemanticProductIndex semanticIndex =
      primec::ir_lowerer::buildSemanticProductIndex(&semanticProgram);

  int staleMapResolutions = 0;
  primec::ir_lowerer::LowerInferenceSetupBootstrapState state;
  state.semanticProgram = &semanticProgram;
  state.semanticIndex = &semanticIndex;
  state.getReturnInfo = [](const std::string &path, primec::ir_lowerer::ReturnInfo &out) {
    if (path != "/map/count") {
      return false;
    }
    out.returnsVoid = false;
    out.returnsArray = false;
    out.kind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
    return true;
  };
  state.resolveMethodCallDefinition =
      [&](const primec::Expr &methodExpr, const primec::ir_lowerer::LocalMap &localsIn) -> const primec::Definition * {
    if (!methodExpr.isMethodCall || methodExpr.name != "count" ||
        methodExpr.args.empty() ||
        methodExpr.args.front().kind != primec::Expr::Kind::Name) {
      return nullptr;
    }
    const auto localIt = localsIn.find(methodExpr.args.front().name);
    if (localIt == localsIn.end() ||
        localIt->second.kind != primec::ir_lowerer::LocalInfo::Kind::Map) {
      return nullptr;
    }
    ++staleMapResolutions;
    return &staleCountDef;
  };

  std::string error;
  CHECK(primec::ir_lowerer::runLowerInferenceExprKindCallReturnSetup(
      {
          .defMap = &defMap,
          .resolveExprPath = [](const primec::Expr &expr) { return expr.name; },
          .isArrayCountCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
          .isStringCountCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      },
      state,
      error));
  CHECK(error.empty());

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo staleMapLocal;
  staleMapLocal.kind = primec::ir_lowerer::LocalInfo::Kind::Map;
  staleMapLocal.mapKeyKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  staleMapLocal.mapValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  locals.emplace("candidate", staleMapLocal);
  locals.emplace("text", staleMapLocal);
  locals.emplace("values", staleMapLocal);

  primec::Expr candidateExpr;
  candidateExpr.kind = primec::Expr::Kind::Name;
  candidateExpr.name = "candidate";
  candidateExpr.semanticNodeId = scalarFact.semanticNodeId;
  primec::Expr countExpr;
  countExpr.kind = primec::Expr::Kind::Call;
  countExpr.name = "count";
  countExpr.args = {candidateExpr};

  primec::ir_lowerer::LocalInfo::ValueKind kindOut =
      primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(state.inferCallExprDirectReturnKind(countExpr, locals, kindOut) ==
        primec::ir_lowerer::CallExpressionReturnKindResolution::NotResolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);

  primec::ir_lowerer::LocalInfo staleStringLocal;
  staleStringLocal.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  staleStringLocal.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::String;
  locals.insert_or_assign("candidate", staleStringLocal);

  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(state.inferCallExprDirectReturnKind(countExpr, locals, kindOut) ==
        primec::ir_lowerer::CallExpressionReturnKindResolution::NotResolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);

  primec::Expr textExpr;
  textExpr.kind = primec::Expr::Kind::Name;
  textExpr.name = "text";
  textExpr.semanticNodeId = stringFact.semanticNodeId;
  countExpr.args = {textExpr};

  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(state.inferCallExprDirectReturnKind(countExpr, locals, kindOut) ==
        primec::ir_lowerer::CallExpressionReturnKindResolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Int32);

  primec::Expr valuesExpr;
  valuesExpr.kind = primec::Expr::Kind::Name;
  valuesExpr.name = "values";
  valuesExpr.semanticNodeId = mapFact.semanticNodeId;
  countExpr.args = {valuesExpr};

  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(state.inferCallExprDirectReturnKind(countExpr, locals, kindOut) ==
        primec::ir_lowerer::CallExpressionReturnKindResolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  CHECK(staleMapResolutions == 0);
}

TEST_CASE("ir lowerer inference call-return setup uses semantic count tail facts before stale strings") {
  std::unordered_map<std::string, const primec::Definition *> defMap;

  primec::SemanticProgram semanticProgram;
  const primec::SymbolId scalarTypeId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "i32");
  primec::SemanticProgramBindingFact scalarFact;
  scalarFact.semanticNodeId = 9911;
  scalarFact.bindingTypeText = "string";
  scalarFact.bindingTypeTextId = scalarTypeId;
  semanticProgram.bindingFacts.push_back(scalarFact);
  semanticProgram.publishedRoutingLookups.bindingFactIndicesByExpr.insert_or_assign(
      scalarFact.semanticNodeId, 0);

  const primec::ir_lowerer::SemanticProductIndex semanticIndex =
      primec::ir_lowerer::buildSemanticProductIndex(&semanticProgram);

  primec::ir_lowerer::LowerInferenceSetupBootstrapState state;
  state.semanticProgram = &semanticProgram;
  state.semanticIndex = &semanticIndex;
  state.getReturnInfo = [](const std::string &, primec::ir_lowerer::ReturnInfo &) {
    return false;
  };
  state.resolveMethodCallDefinition =
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
    return nullptr;
  };

  std::string error;
  CHECK(primec::ir_lowerer::runLowerInferenceExprKindCallReturnSetup(
      {
          .defMap = &defMap,
          .resolveExprPath = [](const primec::Expr &expr) { return expr.name; },
          .isArrayCountCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
          .isStringCountCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      },
      state,
      error));
  CHECK(error.empty());

  primec::Expr candidateExpr;
  candidateExpr.kind = primec::Expr::Kind::Name;
  candidateExpr.name = "candidate";
  candidateExpr.semanticNodeId = scalarFact.semanticNodeId;
  primec::Expr countExpr;
  countExpr.kind = primec::Expr::Kind::Call;
  countExpr.name = "count";
  countExpr.args = {candidateExpr};

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo staleStringLocal;
  staleStringLocal.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  staleStringLocal.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::String;
  locals.emplace("candidate", staleStringLocal);

  primec::ir_lowerer::LocalInfo::ValueKind kindOut =
      primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(state.inferCallExprDirectReturnKind(countExpr, locals, kindOut) ==
        primec::ir_lowerer::CallExpressionReturnKindResolution::NotResolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
}

TEST_CASE("ir lowerer inference call-return setup uses semantic method access receiver facts before stale locals") {
  primec::Definition receiverAtDef;
  receiverAtDef.fullPath = "/std/collections/vector/at";
  std::unordered_map<std::string, const primec::Definition *> defMap = {
      {receiverAtDef.fullPath, &receiverAtDef},
  };

  primec::SemanticProgram semanticProgram;
  const primec::SymbolId vectorTypeId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "vector<f32>");
  primec::SemanticProgramBindingFact bindingFact;
  bindingFact.semanticNodeId = 9201;
  bindingFact.bindingTypeText = "map<i32,i64>";
  bindingFact.bindingTypeTextId = vectorTypeId;
  semanticProgram.bindingFacts.push_back(bindingFact);
  semanticProgram.publishedRoutingLookups.bindingFactIndicesByExpr.insert_or_assign(
      bindingFact.semanticNodeId, 0);
  const primec::ir_lowerer::SemanticProductIndex semanticIndex =
      primec::ir_lowerer::buildSemanticProductIndex(&semanticProgram);

  primec::ir_lowerer::LowerInferenceSetupBootstrapState state;
  state.semanticProgram = &semanticProgram;
  state.semanticIndex = &semanticIndex;
  state.getReturnInfo = [](const std::string &, primec::ir_lowerer::ReturnInfo &out) {
    out.returnsVoid = false;
    out.returnsArray = false;
    out.kind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
    return true;
  };
  state.resolveMethodCallDefinition =
      [&](const primec::Expr &methodExpr, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
    if (!methodExpr.isMethodCall || methodExpr.name != "at" || methodExpr.args.empty()) {
      return nullptr;
    }
    if (methodExpr.args.front().kind == primec::Expr::Kind::Name &&
        methodExpr.args.front().name == "values") {
      return &receiverAtDef;
    }
    return nullptr;
  };

  std::string error;
  CHECK(primec::ir_lowerer::runLowerInferenceExprKindCallReturnSetup(
      {
          .defMap = &defMap,
          .resolveExprPath = [](const primec::Expr &expr) { return expr.name; },
          .isArrayCountCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
          .isStringCountCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      },
      state,
      error));
  CHECK(error.empty());

  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "values";
  receiverExpr.semanticNodeId = bindingFact.semanticNodeId;
  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Literal;
  indexExpr.intWidth = 32;
  indexExpr.literalValue = 1;
  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "at";
  callExpr.isMethodCall = true;
  callExpr.args = {receiverExpr, indexExpr};

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo staleLocal;
  staleLocal.kind = primec::ir_lowerer::LocalInfo::Kind::Vector;
  staleLocal.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  locals.emplace("values", staleLocal);

  primec::ir_lowerer::LocalInfo::ValueKind kindOut =
      primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(state.inferCallExprDirectReturnKind(callExpr, locals, kindOut) ==
        primec::ir_lowerer::CallExpressionReturnKindResolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Float32);
}

TEST_CASE("ir lowerer inference call-return setup uses semantic unresolved builtin receiver facts before stale locals") {
  std::unordered_map<std::string, const primec::Definition *> defMap;

  primec::SemanticProgram semanticProgram;
  const primec::SymbolId mapTypeId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "map<i32,f32>");
  primec::SemanticProgramBindingFact mapFact;
  mapFact.semanticNodeId = 9301;
  mapFact.bindingTypeText = "map<i32,i64>";
  mapFact.bindingTypeTextId = mapTypeId;
  semanticProgram.bindingFacts.push_back(mapFact);
  semanticProgram.publishedRoutingLookups.bindingFactIndicesByExpr.insert_or_assign(
      mapFact.semanticNodeId, 0);

  const primec::SymbolId scalarTypeId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "i32");
  primec::SemanticProgramBindingFact scalarFact;
  scalarFact.semanticNodeId = 9302;
  scalarFact.bindingTypeText = "map<i32,i64>";
  scalarFact.bindingTypeTextId = scalarTypeId;
  semanticProgram.bindingFacts.push_back(scalarFact);
  semanticProgram.publishedRoutingLookups.bindingFactIndicesByExpr.insert_or_assign(
      scalarFact.semanticNodeId, 1);

  const primec::ir_lowerer::SemanticProductIndex semanticIndex =
      primec::ir_lowerer::buildSemanticProductIndex(&semanticProgram);

  primec::ir_lowerer::LowerInferenceSetupBootstrapState state;
  state.semanticProgram = &semanticProgram;
  state.semanticIndex = &semanticIndex;
  state.getReturnInfo = [](const std::string &, primec::ir_lowerer::ReturnInfo &) {
    return false;
  };
  state.resolveMethodCallDefinition =
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
    return nullptr;
  };

  std::string error;
  CHECK(primec::ir_lowerer::runLowerInferenceExprKindCallReturnSetup(
      {
          .defMap = &defMap,
          .resolveExprPath = [](const primec::Expr &expr) { return expr.name; },
          .isArrayCountCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
          .isStringCountCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      },
      state,
      error));
  CHECK(error.empty());

  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "values";
  receiverExpr.semanticNodeId = mapFact.semanticNodeId;
  primec::Expr keyExpr;
  keyExpr.kind = primec::Expr::Kind::Literal;
  keyExpr.intWidth = 32;
  keyExpr.literalValue = 1;
  primec::Expr tryAtExpr;
  tryAtExpr.kind = primec::Expr::Kind::Call;
  tryAtExpr.name = "tryAt";
  tryAtExpr.isMethodCall = true;
  tryAtExpr.args = {receiverExpr, keyExpr};

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo staleMapLocal;
  staleMapLocal.kind = primec::ir_lowerer::LocalInfo::Kind::Map;
  staleMapLocal.mapValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  locals.emplace("values", staleMapLocal);

  primec::ir_lowerer::LocalInfo::ValueKind kindOut =
      primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(state.inferCallExprDirectReturnKind(tryAtExpr, locals, kindOut) ==
        primec::ir_lowerer::CallExpressionReturnKindResolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Float32);

  receiverExpr.semanticNodeId = scalarFact.semanticNodeId;
  primec::Expr containsExpr;
  containsExpr.kind = primec::Expr::Kind::Call;
  containsExpr.name = "contains";
  containsExpr.isMethodCall = true;
  containsExpr.args = {receiverExpr, keyExpr};

  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(state.inferCallExprDirectReturnKind(containsExpr, locals, kindOut) ==
        primec::ir_lowerer::CallExpressionReturnKindResolution::NotResolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
}

TEST_CASE("ir lowerer inference call-return setup uses semantic vector mutator receiver facts before stale locals") {
  std::unordered_map<std::string, const primec::Definition *> defMap;
  primec::Definition stalePushDef;
  stalePushDef.fullPath = "/vector/push";
  primec::Definition canonicalPushDef;
  canonicalPushDef.fullPath = "/std/collections/vector/push";

  primec::SemanticProgram semanticProgram;
  const primec::SymbolId scalarTypeId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "i32");
  primec::SemanticProgramBindingFact scalarFact;
  scalarFact.semanticNodeId = 9401;
  scalarFact.bindingTypeText = "vector<i64>";
  scalarFact.bindingTypeTextId = scalarTypeId;
  semanticProgram.bindingFacts.push_back(scalarFact);
  semanticProgram.publishedRoutingLookups.bindingFactIndicesByExpr.insert_or_assign(
      scalarFact.semanticNodeId, 0);

  const primec::SymbolId vectorTypeId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "vector<f32>");
  primec::SemanticProgramBindingFact vectorFact;
  vectorFact.semanticNodeId = 9402;
  vectorFact.bindingTypeText = "i32";
  vectorFact.bindingTypeTextId = vectorTypeId;
  semanticProgram.bindingFacts.push_back(vectorFact);
  semanticProgram.publishedRoutingLookups.bindingFactIndicesByExpr.insert_or_assign(
      vectorFact.semanticNodeId, 1);

  const primec::ir_lowerer::SemanticProductIndex semanticIndex =
      primec::ir_lowerer::buildSemanticProductIndex(&semanticProgram);

  primec::ir_lowerer::LowerInferenceSetupBootstrapState state;
  state.semanticProgram = &semanticProgram;
  state.semanticIndex = &semanticIndex;
  state.getReturnInfo = [](const std::string &path, primec::ir_lowerer::ReturnInfo &out) {
    out.returnsVoid = false;
    out.returnsArray = false;
    if (path == "/vector/push") {
      out.kind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
      return true;
    }
    if (path == "/std/collections/vector/push") {
      out.kind = primec::ir_lowerer::LocalInfo::ValueKind::UInt64;
      return true;
    }
    return false;
  };
  state.resolveMethodCallDefinition =
      [&](const primec::Expr &methodExpr, const primec::ir_lowerer::LocalMap &localsIn) -> const primec::Definition * {
    if (!methodExpr.isMethodCall || methodExpr.name != "push" || methodExpr.args.empty()) {
      return nullptr;
    }
    const primec::Expr &receiverExpr = methodExpr.args.front();
    if (receiverExpr.semanticNodeId == vectorFact.semanticNodeId) {
      return &canonicalPushDef;
    }
    if (receiverExpr.kind == primec::Expr::Kind::Name) {
      const auto localIt = localsIn.find(receiverExpr.name);
      if (localIt != localsIn.end() &&
          localIt->second.kind == primec::ir_lowerer::LocalInfo::Kind::Vector) {
        return &stalePushDef;
      }
    }
    return nullptr;
  };

  std::string error;
  CHECK(primec::ir_lowerer::runLowerInferenceExprKindCallReturnSetup(
      {
          .defMap = &defMap,
          .resolveExprPath = [](const primec::Expr &expr) { return expr.name; },
          .isArrayCountCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
          .isStringCountCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      },
      state,
      error));
  CHECK(error.empty());

  primec::Expr staleCandidateExpr;
  staleCandidateExpr.kind = primec::Expr::Kind::Name;
  staleCandidateExpr.name = "candidate";
  staleCandidateExpr.semanticNodeId = scalarFact.semanticNodeId;
  primec::Expr valuesExpr;
  valuesExpr.kind = primec::Expr::Kind::Name;
  valuesExpr.name = "values";
  valuesExpr.semanticNodeId = vectorFact.semanticNodeId;
  primec::Expr valueExpr;
  valueExpr.kind = primec::Expr::Kind::Literal;
  valueExpr.intWidth = 32;
  valueExpr.literalValue = 1;

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo staleVectorLocal;
  staleVectorLocal.kind = primec::ir_lowerer::LocalInfo::Kind::Vector;
  staleVectorLocal.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  locals.emplace("candidate", staleVectorLocal);

  primec::Expr reorderedPushExpr;
  reorderedPushExpr.kind = primec::Expr::Kind::Call;
  reorderedPushExpr.name = "push";
  reorderedPushExpr.args = {staleCandidateExpr, valuesExpr};

  primec::ir_lowerer::LocalInfo::ValueKind kindOut =
      primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(state.inferCallExprDirectReturnKind(reorderedPushExpr, locals, kindOut) ==
        primec::ir_lowerer::CallExpressionReturnKindResolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::UInt64);

  primec::ir_lowerer::LocalMap noLocalLocals;
  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(state.inferCallExprDirectReturnKind(reorderedPushExpr, noLocalLocals, kindOut) ==
        primec::ir_lowerer::CallExpressionReturnKindResolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::UInt64);

  primec::Expr staleOnlyPushExpr;
  staleOnlyPushExpr.kind = primec::Expr::Kind::Call;
  staleOnlyPushExpr.name = "push";
  staleOnlyPushExpr.args = {staleCandidateExpr, valueExpr};

  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(state.inferCallExprDirectReturnKind(staleOnlyPushExpr, locals, kindOut) ==
        primec::ir_lowerer::CallExpressionReturnKindResolution::NotResolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
}

TEST_CASE("ir lowerer inference call-return setup uses semantic capacity receiver facts before stale locals") {
  std::unordered_map<std::string, const primec::Definition *> defMap;

  primec::SemanticProgram semanticProgram;
  const primec::SymbolId vectorTypeId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "vector<f32>");
  primec::SemanticProgramBindingFact vectorFact;
  vectorFact.semanticNodeId = 9501;
  vectorFact.bindingTypeText = "i32";
  vectorFact.bindingTypeTextId = vectorTypeId;
  semanticProgram.bindingFacts.push_back(vectorFact);
  semanticProgram.publishedRoutingLookups.bindingFactIndicesByExpr.insert_or_assign(
      vectorFact.semanticNodeId, 0);

  const primec::SymbolId scalarTypeId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "i32");
  primec::SemanticProgramBindingFact scalarFact;
  scalarFact.semanticNodeId = 9502;
  scalarFact.bindingTypeText = "vector<i64>";
  scalarFact.bindingTypeTextId = scalarTypeId;
  semanticProgram.bindingFacts.push_back(scalarFact);
  semanticProgram.publishedRoutingLookups.bindingFactIndicesByExpr.insert_or_assign(
      scalarFact.semanticNodeId, 1);

  const primec::ir_lowerer::SemanticProductIndex semanticIndex =
      primec::ir_lowerer::buildSemanticProductIndex(&semanticProgram);

  primec::ir_lowerer::LowerInferenceSetupBootstrapState state;
  state.semanticProgram = &semanticProgram;
  state.semanticIndex = &semanticIndex;
  state.getReturnInfo = [](const std::string &, primec::ir_lowerer::ReturnInfo &) {
    return false;
  };
  state.resolveMethodCallDefinition =
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
    return nullptr;
  };

  std::string error;
  CHECK(primec::ir_lowerer::runLowerInferenceExprKindCallReturnSetup(
      {
          .defMap = &defMap,
          .resolveExprPath = [](const primec::Expr &expr) { return expr.name; },
          .isArrayCountCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
          .isStringCountCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      },
      state,
      error));
  CHECK(error.empty());

  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "values";
  receiverExpr.semanticNodeId = vectorFact.semanticNodeId;
  primec::Expr capacityExpr;
  capacityExpr.kind = primec::Expr::Kind::Call;
  capacityExpr.name = "capacity";
  capacityExpr.args = {receiverExpr};

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo staleScalarLocal;
  staleScalarLocal.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  staleScalarLocal.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  locals.emplace("values", staleScalarLocal);

  primec::ir_lowerer::LocalInfo::ValueKind kindOut =
      primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(state.inferCallExprDirectReturnKind(capacityExpr, locals, kindOut) ==
        primec::ir_lowerer::CallExpressionReturnKindResolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Int32);

  primec::ir_lowerer::LocalInfo staleVectorLocal;
  staleVectorLocal.kind = primec::ir_lowerer::LocalInfo::Kind::Vector;
  locals.insert_or_assign("values", staleVectorLocal);
  receiverExpr.semanticNodeId = scalarFact.semanticNodeId;
  capacityExpr.args = {receiverExpr};

  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(state.inferCallExprDirectReturnKind(capacityExpr, locals, kindOut) ==
        primec::ir_lowerer::CallExpressionReturnKindResolution::NotResolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
}

TEST_CASE("ir lowerer inference call-return setup rejects vector compatibility array access fallback") {
  primec::Definition receiverAtDef;
  receiverAtDef.fullPath = "/vector/at";
  primec::Definition arrayAtDef;
  arrayAtDef.fullPath = "/array/at";
  std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/array/at", &arrayAtDef},
  };

  bool resolveReceiverHelper = true;
  int resolveMethodCalls = 0;
  primec::ir_lowerer::LowerInferenceSetupBootstrapState state;
  state.getReturnInfo = [](const std::string &path, primec::ir_lowerer::ReturnInfo &out) {
    out.returnsVoid = false;
    out.returnsArray = false;
    if (path == "/vector/at") {
      out.kind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
      return true;
    }
    if (path == "/array/at") {
      out.kind = primec::ir_lowerer::LocalInfo::ValueKind::UInt64;
      return true;
    }
    return false;
  };
  state.resolveMethodCallDefinition =
      [&](const primec::Expr &methodExpr, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
    ++resolveMethodCalls;
    if (!resolveReceiverHelper || !methodExpr.isMethodCall || methodExpr.name != "at" || methodExpr.args.empty()) {
      return nullptr;
    }
    if (methodExpr.args.front().kind == primec::Expr::Kind::Name && methodExpr.args.front().name == "values") {
      return &receiverAtDef;
    }
    return nullptr;
  };

  std::string error;
  CHECK(primec::ir_lowerer::runLowerInferenceExprKindCallReturnSetup(
      {
          .defMap = &defMap,
          .resolveExprPath = [](const primec::Expr &expr) { return expr.name; },
          .isArrayCountCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
          .isStringCountCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      },
      state,
      error));
  CHECK(error.empty());

  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "values";
  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Literal;
  indexExpr.intWidth = 32;
  indexExpr.literalValue = 1;
  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "/array/at";
  callExpr.args = {receiverExpr, indexExpr};

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(state.inferCallExprDirectReturnKind(callExpr, {}, kindOut) ==
        primec::ir_lowerer::CallExpressionReturnKindResolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::UInt64);
  CHECK(resolveMethodCalls == 0);

  resolveReceiverHelper = false;
  resolveMethodCalls = 0;
  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(state.inferCallExprDirectReturnKind(callExpr, {}, kindOut) ==
        primec::ir_lowerer::CallExpressionReturnKindResolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::UInt64);
  CHECK(resolveMethodCalls == 0);
}

TEST_CASE("ir lowerer inference call-return setup resolves namespaced at unsafe definition directly") {
  primec::Definition receiverAtUnsafeDef;
  receiverAtUnsafeDef.fullPath = "/vector/at_unsafe";
  primec::Definition canonicalAtUnsafeDef;
  canonicalAtUnsafeDef.fullPath = "/std/collections/vector/at_unsafe";
  std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/std/collections/vector/at_unsafe", &canonicalAtUnsafeDef},
  };

  bool resolveReceiverHelper = true;
  int resolveMethodCalls = 0;
  primec::ir_lowerer::LowerInferenceSetupBootstrapState state;
  state.getReturnInfo = [](const std::string &path, primec::ir_lowerer::ReturnInfo &out) {
    out.returnsVoid = false;
    out.returnsArray = false;
    if (path == "/vector/at_unsafe") {
      out.kind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
      return true;
    }
    if (path == "/std/collections/vector/at_unsafe") {
      out.kind = primec::ir_lowerer::LocalInfo::ValueKind::UInt64;
      return true;
    }
    return false;
  };
  state.resolveMethodCallDefinition =
      [&](const primec::Expr &methodExpr, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
    ++resolveMethodCalls;
    if (!resolveReceiverHelper || !methodExpr.isMethodCall || methodExpr.name != "at_unsafe" ||
        methodExpr.args.empty()) {
      return nullptr;
    }
    if (methodExpr.args.front().kind == primec::Expr::Kind::Name && methodExpr.args.front().name == "values") {
      return &receiverAtUnsafeDef;
    }
    return nullptr;
  };

  std::string error;
  CHECK(primec::ir_lowerer::runLowerInferenceExprKindCallReturnSetup(
      {
          .defMap = &defMap,
          .resolveExprPath = [](const primec::Expr &expr) { return expr.name; },
          .isArrayCountCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
          .isStringCountCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      },
      state,
      error));
  CHECK(error.empty());

  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "values";
  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Literal;
  indexExpr.intWidth = 32;
  indexExpr.literalValue = 1;
  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "/std/collections/vector/at_unsafe";
  callExpr.args = {receiverExpr, indexExpr};

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(state.inferCallExprDirectReturnKind(callExpr, {}, kindOut) ==
        primec::ir_lowerer::CallExpressionReturnKindResolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::UInt64);
  CHECK(resolveMethodCalls == 0);

  resolveReceiverHelper = false;
  resolveMethodCalls = 0;
  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(state.inferCallExprDirectReturnKind(callExpr, {}, kindOut) ==
        primec::ir_lowerer::CallExpressionReturnKindResolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::UInt64);
  CHECK(resolveMethodCalls == 0);
}

TEST_CASE("ir lowerer inference call-return setup resolves canonical namespaced map count directly") {
  primec::Definition receiverCountDef;
  receiverCountDef.fullPath = "/map/count";
  primec::Definition canonicalCountDef;
  canonicalCountDef.fullPath = "/std/collections/map/count";
  std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/std/collections/map/count", &canonicalCountDef},
  };

  bool resolveReceiverHelper = true;
  int resolveMethodCalls = 0;
  primec::ir_lowerer::LowerInferenceSetupBootstrapState state;
  state.getReturnInfo = [](const std::string &path, primec::ir_lowerer::ReturnInfo &out) {
    out.returnsVoid = false;
    out.returnsArray = false;
    if (path == "/map/count") {
      out.kind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
      return true;
    }
    if (path == "/std/collections/map/count") {
      out.kind = primec::ir_lowerer::LocalInfo::ValueKind::UInt64;
      return true;
    }
    return false;
  };
  state.resolveMethodCallDefinition =
      [&](const primec::Expr &methodExpr, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
    ++resolveMethodCalls;
    if (!resolveReceiverHelper || !methodExpr.isMethodCall || methodExpr.name != "count" || methodExpr.args.empty()) {
      return nullptr;
    }
    if (methodExpr.args.front().kind == primec::Expr::Kind::Name && methodExpr.args.front().name == "values") {
      return &receiverCountDef;
    }
    return nullptr;
  };

  std::string error;
  CHECK(primec::ir_lowerer::runLowerInferenceExprKindCallReturnSetup(
      {
          .defMap = &defMap,
          .resolveExprPath = [](const primec::Expr &expr) { return expr.name; },
          .isArrayCountCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
          .isStringCountCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      },
      state,
      error));
  CHECK(error.empty());

  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "values";
  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "/std/collections/map/count";
  callExpr.args = {receiverExpr};

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(state.inferCallExprDirectReturnKind(callExpr, {}, kindOut) ==
        primec::ir_lowerer::CallExpressionReturnKindResolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::UInt64);
  CHECK(resolveMethodCalls == 0);

  resolveReceiverHelper = false;
  resolveMethodCalls = 0;
  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(state.inferCallExprDirectReturnKind(callExpr, {}, kindOut) ==
        primec::ir_lowerer::CallExpressionReturnKindResolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::UInt64);
  CHECK(resolveMethodCalls == 0);
}

TEST_CASE("ir lowerer inference call-return setup keeps explicit compatibility map count on direct fallback") {
  primec::Definition receiverCountDef;
  receiverCountDef.fullPath = "/map/count";
  primec::Definition canonicalCountDef;
  canonicalCountDef.fullPath = "/std/collections/map/count";
  std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/std/collections/map/count", &canonicalCountDef},
  };

  bool resolveReceiverHelper = true;
  int resolveMethodCalls = 0;
  primec::ir_lowerer::LowerInferenceSetupBootstrapState state;
  state.getReturnInfo = [](const std::string &path, primec::ir_lowerer::ReturnInfo &out) {
    out.returnsVoid = false;
    out.returnsArray = false;
    if (path == "/map/count") {
      out.kind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
      return true;
    }
    if (path == "/std/collections/map/count") {
      out.kind = primec::ir_lowerer::LocalInfo::ValueKind::UInt64;
      return true;
    }
    return false;
  };
  state.resolveMethodCallDefinition =
      [&](const primec::Expr &methodExpr, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
    ++resolveMethodCalls;
    if (!resolveReceiverHelper || !methodExpr.isMethodCall || methodExpr.name != "count" || methodExpr.args.empty()) {
      return nullptr;
    }
    if (methodExpr.args.front().kind == primec::Expr::Kind::Name && methodExpr.args.front().name == "values") {
      return &receiverCountDef;
    }
    return nullptr;
  };

  std::string error;
  CHECK(primec::ir_lowerer::runLowerInferenceExprKindCallReturnSetup(
      {
          .defMap = &defMap,
          .resolveExprPath = [](const primec::Expr &expr) {
            if (expr.name == "/map/count") {
              return std::string("/std/collections/map/count");
            }
            return expr.name;
          },
          .isArrayCountCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
          .isStringCountCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      },
      state,
      error));
  CHECK(error.empty());

  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "values";
  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "/map/count";
  callExpr.args = {receiverExpr};

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(state.inferCallExprDirectReturnKind(callExpr, {}, kindOut) ==
        primec::ir_lowerer::CallExpressionReturnKindResolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::UInt64);
  CHECK(resolveMethodCalls == 0);

  resolveReceiverHelper = false;
  resolveMethodCalls = 0;
  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(state.inferCallExprDirectReturnKind(callExpr, {}, kindOut) ==
        primec::ir_lowerer::CallExpressionReturnKindResolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::UInt64);
  CHECK(resolveMethodCalls == 0);
}

TEST_CASE("ir lowerer inference call-return setup keeps unresolved compatibility map count without definitions") {
  std::unordered_map<std::string, const primec::Definition *> defMap;
  int resolveMethodCalls = 0;
  primec::ir_lowerer::LowerInferenceSetupBootstrapState state;
  state.getReturnInfo = [](const std::string &, primec::ir_lowerer::ReturnInfo &) { return false; };
  state.resolveMethodCallDefinition =
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
    ++resolveMethodCalls;
    return nullptr;
  };

  std::string error;
  CHECK(primec::ir_lowerer::runLowerInferenceExprKindCallReturnSetup(
      {
          .defMap = &defMap,
          .resolveExprPath = [](const primec::Expr &expr) { return expr.name; },
          .isArrayCountCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
          .isStringCountCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      },
      state,
      error));
  CHECK(error.empty());

  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "values";
  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "/map/count";
  callExpr.args = {receiverExpr};

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(state.inferCallExprDirectReturnKind(callExpr, {}, kindOut) ==
        primec::ir_lowerer::CallExpressionReturnKindResolution::NotResolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
  CHECK(resolveMethodCalls == 0);
}

TEST_CASE("ir lowerer inference call-return setup resolves explicit map helper aliases against canonical-only defs") {
  primec::Definition canonicalCountDef;
  canonicalCountDef.fullPath = "/std/collections/map/count";
  primec::Definition canonicalAtDef;
  canonicalAtDef.fullPath = "/std/collections/map/at";
  primec::Definition canonicalAtUnsafeDef;
  canonicalAtUnsafeDef.fullPath = "/std/collections/map/at_unsafe";
  std::unordered_map<std::string, const primec::Definition *> defMap = {
      {canonicalCountDef.fullPath, &canonicalCountDef},
      {canonicalAtDef.fullPath, &canonicalAtDef},
      {canonicalAtUnsafeDef.fullPath, &canonicalAtUnsafeDef},
  };

  primec::ir_lowerer::LowerInferenceSetupBootstrapState state;
  state.getReturnInfo = [](const std::string &path, primec::ir_lowerer::ReturnInfo &out) {
    out.returnsVoid = false;
    out.returnsArray = false;
    if (path == "/std/collections/map/count") {
      out.kind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
      return true;
    }
    if (path == "/std/collections/map/at" || path == "/std/collections/map/at_unsafe") {
      out.kind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
      return true;
    }
    return false;
  };
  state.resolveMethodCallDefinition =
      [&defMap](const primec::Expr &methodExpr, const primec::ir_lowerer::LocalMap &localsIn)
      -> const primec::Definition * {
    std::string error;
    return primec::ir_lowerer::resolveMethodCallDefinitionFromExpr(
        methodExpr,
        localsIn,
        [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
        [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
        [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
        {},
        {},
        [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
          return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
        },
        [](const primec::Expr &expr) { return expr.name; },
        defMap,
        error);
  };

  std::string error;
  CHECK(primec::ir_lowerer::runLowerInferenceExprKindCallReturnSetup(
      {
          .defMap = &defMap,
          .resolveExprPath = [](const primec::Expr &expr) { return expr.name; },
          .isArrayCountCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
          .isStringCountCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      },
      state,
      error));
  CHECK(error.empty());

  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "values";

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo valuesLocal;
  valuesLocal.kind = primec::ir_lowerer::LocalInfo::Kind::Map;
  locals.emplace("values", valuesLocal);

  primec::Expr keyExpr;
  keyExpr.kind = primec::Expr::Kind::Literal;
  keyExpr.intWidth = 32;
  keyExpr.literalValue = 1;

  primec::Expr aliasCountCall;
  aliasCountCall.kind = primec::Expr::Kind::Call;
  aliasCountCall.name = "/map/count";
  aliasCountCall.args = {receiverExpr};

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(state.inferCallExprDirectReturnKind(aliasCountCall, locals, kindOut) ==
        primec::ir_lowerer::CallExpressionReturnKindResolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Int32);

  primec::Expr aliasAtCall;
  aliasAtCall.kind = primec::Expr::Kind::Call;
  aliasAtCall.name = "/map/at";
  aliasAtCall.args = {receiverExpr, keyExpr};

  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(state.inferCallExprDirectReturnKind(aliasAtCall, locals, kindOut) ==
        primec::ir_lowerer::CallExpressionReturnKindResolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Int64);

  primec::Expr aliasAtUnsafeCall;
  aliasAtUnsafeCall.kind = primec::Expr::Kind::Call;
  aliasAtUnsafeCall.name = "/map/at_unsafe";
  aliasAtUnsafeCall.args = {receiverExpr, keyExpr};

  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(state.inferCallExprDirectReturnKind(aliasAtUnsafeCall, locals, kindOut) ==
        primec::ir_lowerer::CallExpressionReturnKindResolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Int64);

  primec::Expr canonicalCountCall;
  canonicalCountCall.kind = primec::Expr::Kind::Call;
  canonicalCountCall.name = "/std/collections/map/count";
  canonicalCountCall.args = {receiverExpr};

  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(state.inferCallExprDirectReturnKind(canonicalCountCall, locals, kindOut) ==
        primec::ir_lowerer::CallExpressionReturnKindResolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Int32);

  primec::Expr canonicalAtCall;
  canonicalAtCall.kind = primec::Expr::Kind::Call;
  canonicalAtCall.name = "/std/collections/map/at";
  canonicalAtCall.args = {receiverExpr, keyExpr};

  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(state.inferCallExprDirectReturnKind(canonicalAtCall, locals, kindOut) ==
        primec::ir_lowerer::CallExpressionReturnKindResolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Int64);

  primec::Expr canonicalAtUnsafeCall;
  canonicalAtUnsafeCall.kind = primec::Expr::Kind::Call;
  canonicalAtUnsafeCall.name = "/std/collections/map/at_unsafe";
  canonicalAtUnsafeCall.args = {receiverExpr, keyExpr};

  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(state.inferCallExprDirectReturnKind(canonicalAtUnsafeCall, locals, kindOut) ==
        primec::ir_lowerer::CallExpressionReturnKindResolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Int64);
}

TEST_SUITE_END();
