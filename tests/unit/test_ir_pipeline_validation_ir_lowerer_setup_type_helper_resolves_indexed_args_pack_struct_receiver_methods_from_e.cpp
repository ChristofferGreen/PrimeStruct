#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir lowerer setup type helper resolves indexed args-pack struct receiver methods from expressions") {
  primec::Definition structMethodDef;
  structMethodDef.fullPath = "/pkg/Pair/length";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {structMethodDef.fullPath, &structMethodDef},
  };

  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "values";

  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Literal;
  indexExpr.intWidth = 32;
  indexExpr.literalValue = 0;

  primec::Expr receiverCall;
  receiverCall.kind = primec::Expr::Kind::Call;
  receiverCall.name = "at";
  receiverCall.isMethodCall = true;
  receiverCall.args = {receiverExpr, indexExpr};

  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.name = "length";
  methodCall.isMethodCall = true;
  methodCall.args.push_back(receiverCall);

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo valuesLocal;
  valuesLocal.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  valuesLocal.isArgsPack = true;
  valuesLocal.structTypeName = "/pkg/Pair";
  valuesLocal.structSlotCount = 2;
  locals.emplace("values", valuesLocal);

  std::string error;
  const primec::Definition *resolved = primec::ir_lowerer::resolveMethodCallDefinitionFromExpr(
      methodCall,
      locals,
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      {},
      {"/pkg/Pair"},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &) { return std::string(); },
      defMap,
      error);
  CHECK(resolved == &structMethodDef);
  CHECK(error.empty());
}

TEST_CASE("ir lowerer setup type helper resolves soa_vector receiver method definitions from expressions") {
  primec::Definition soaPushDef;
  soaPushDef.fullPath = "/std/collections/soa_vector/push";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/std/collections/soa_vector/push", &soaPushDef},
  };

  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "values";

  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.name = "push";
  methodCall.isMethodCall = true;
  methodCall.args.push_back(receiverExpr);

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo valuesLocal;
  valuesLocal.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  valuesLocal.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  valuesLocal.isSoaVector = true;
  locals.emplace("values", valuesLocal);

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
  CHECK(resolved == &soaPushDef);
  CHECK(error.empty());
}

TEST_CASE("ir lowerer setup type helper requires semantic-product method targets") {
  primec::Definition soaPushDef;
  soaPushDef.fullPath = "/std/collections/soa_vector/push";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/std/collections/soa_vector/push", &soaPushDef},
  };

  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "values";

  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.name = "push";
  methodCall.isMethodCall = true;
  methodCall.semanticNodeId = 17;
  methodCall.args.push_back(receiverExpr);

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo valuesLocal;
  valuesLocal.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  valuesLocal.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  valuesLocal.isSoaVector = true;
  locals.emplace("values", valuesLocal);

  primec::SemanticProgram semanticProgram;
  const auto semanticTargets =
      primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);

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
      &semanticTargets,
      defMap,
      error);
  CHECK(resolved == nullptr);
  CHECK(error == "missing semantic-product method-call target: push");

  semanticProgram.methodCallTargets.push_back(primec::SemanticProgramMethodCallTarget{
      .scopePath = "/main",
      .methodName = "push",
      .receiverTypeText = "",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 17,
      .scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
      .methodNameId = primec::semanticProgramInternCallTargetString(semanticProgram, "push"),
      .resolvedPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram,
                                                        "/std/collections/soa_vector/push"),
  });
  const auto populatedTargets =
      primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);

  error.clear();
  resolved = primec::ir_lowerer::resolveMethodCallDefinitionFromExpr(
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
      &populatedTargets,
      defMap,
      error);
  CHECK(resolved == &soaPushDef);
  CHECK(error.empty());
}

TEST_CASE("ir lowerer setup type helper rejects semantic-product method targets without lowered definitions") {
  const std::unordered_map<std::string, const primec::Definition *> defMap = {};

  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "values";

  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.name = "contains";
  methodCall.isMethodCall = true;
  methodCall.semanticNodeId = 91;
  methodCall.args.push_back(receiverExpr);

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo valuesLocal;
  valuesLocal.kind = primec::ir_lowerer::LocalInfo::Kind::Map;
  locals.emplace("values", valuesLocal);

  primec::SemanticProgram semanticProgram;
  semanticProgram.methodCallTargets.push_back(primec::SemanticProgramMethodCallTarget{
      .scopePath = "/main",
      .methodName = "contains",
      .receiverTypeText = "",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 91,
      .scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
      .methodNameId = primec::semanticProgramInternCallTargetString(semanticProgram, "contains"),
      .resolvedPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram,
                                                        "/std/collections/map/contains"),
  });
  const auto semanticTargets =
      primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);

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
      &semanticTargets,
      defMap,
      error);
  CHECK(resolved == nullptr);
  CHECK(error == "semantic-product method-call target missing lowered definition: /std/collections/map/contains");
}

TEST_CASE("ir lowerer setup type helper does not reconstruct method targets from receivers when semantic-product defs are missing") {
  primec::Definition canonicalContainsDef;
  canonicalContainsDef.fullPath = "/std/collections/map/contains";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {canonicalContainsDef.fullPath, &canonicalContainsDef},
  };

  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "values";

  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.name = "contains";
  methodCall.isMethodCall = true;
  methodCall.semanticNodeId = 133;
  methodCall.args.push_back(receiverExpr);

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo valuesLocal;
  valuesLocal.kind = primec::ir_lowerer::LocalInfo::Kind::Map;
  locals.emplace("values", valuesLocal);

  primec::SemanticProgram semanticProgram;
  semanticProgram.methodCallTargets.push_back(primec::SemanticProgramMethodCallTarget{
      .scopePath = "/main",
      .methodName = "contains",
      .receiverTypeText = "",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 133,
      .scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
      .methodNameId = primec::semanticProgramInternCallTargetString(semanticProgram, "contains"),
      .resolvedPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram,
                                                        "/semantic/method/contains"),
  });
  const auto semanticTargets =
      primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);

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
      &semanticTargets,
      defMap,
      error);
  CHECK(resolved == nullptr);
  CHECK(error == "semantic-product method-call target missing lowered definition: /semantic/method/contains");
}

TEST_CASE("ir lowerer setup type helper rejects alias receiver call return fallback to canonical stdlib defs") {
  primec::Definition canonicalAtDef;
  canonicalAtDef.fullPath = "/std/collections/vector/at";
  primec::Transform returnMarker;
  returnMarker.name = "return";
  returnMarker.templateArgs = {"Marker"};
  canonicalAtDef.transforms.push_back(returnMarker);

  primec::Definition markerTagDef;
  markerTagDef.fullPath = "/Marker/tag";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {canonicalAtDef.fullPath, &canonicalAtDef},
      {markerTagDef.fullPath, &markerTagDef},
  };

  primec::Expr valuesExpr;
  valuesExpr.kind = primec::Expr::Kind::Name;
  valuesExpr.name = "values";

  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Literal;
  indexExpr.intWidth = 32;
  indexExpr.literalValue = 1;

  primec::Expr receiverCall;
  receiverCall.kind = primec::Expr::Kind::Call;
  receiverCall.name = "/vector/at";
  receiverCall.args = {valuesExpr, indexExpr};

  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.name = "tag";
  methodCall.isMethodCall = true;
  methodCall.args = {receiverCall};

  std::string error;
  const primec::Definition *resolved = primec::ir_lowerer::resolveMethodCallDefinitionFromExpr(
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
      [](const primec::Expr &expr) { return expr.name; },
      defMap,
      error);
  CHECK(resolved == nullptr);
  CHECK(error == "unknown method target for tag");
}

TEST_CASE("ir lowerer setup type helper keeps reject diagnostics for alias receiver call returns") {
  primec::Definition canonicalAtDef;
  canonicalAtDef.fullPath = "/std/collections/vector/at";
  primec::Transform returnMarker;
  returnMarker.name = "return";
  returnMarker.templateArgs = {"Marker"};
  canonicalAtDef.transforms.push_back(returnMarker);

  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {canonicalAtDef.fullPath, &canonicalAtDef},
  };

  primec::Expr valuesExpr;
  valuesExpr.kind = primec::Expr::Kind::Name;
  valuesExpr.name = "values";

  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Literal;
  indexExpr.intWidth = 32;
  indexExpr.literalValue = 1;

  primec::Expr receiverCall;
  receiverCall.kind = primec::Expr::Kind::Call;
  receiverCall.name = "/vector/at";
  receiverCall.args = {valuesExpr, indexExpr};

  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.name = "tag";
  methodCall.isMethodCall = true;
  methodCall.args = {receiverCall};

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
            [](const primec::Expr &expr) { return expr.name; },
            defMap,
            error) == nullptr);
  CHECK(error == "unknown method target for tag");
}

TEST_CASE("ir lowerer setup type helper rejects alias receiver defs without canonical return fallback") {
  primec::Definition aliasAtDef;
  aliasAtDef.fullPath = "/vector/at";
  primec::Definition canonicalAtDef;
  canonicalAtDef.fullPath = "/std/collections/vector/at";
  primec::Transform returnMarker;
  returnMarker.name = "return";
  returnMarker.templateArgs = {"Marker"};
  canonicalAtDef.transforms.push_back(returnMarker);

  primec::Definition markerTagDef;
  markerTagDef.fullPath = "/Marker/tag";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {aliasAtDef.fullPath, &aliasAtDef},
      {canonicalAtDef.fullPath, &canonicalAtDef},
      {markerTagDef.fullPath, &markerTagDef},
  };

  primec::Expr valuesExpr;
  valuesExpr.kind = primec::Expr::Kind::Name;
  valuesExpr.name = "values";

  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Literal;
  indexExpr.intWidth = 32;
  indexExpr.literalValue = 1;

  primec::Expr receiverCall;
  receiverCall.kind = primec::Expr::Kind::Call;
  receiverCall.name = "/vector/at";
  receiverCall.args = {valuesExpr, indexExpr};

  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.name = "tag";
  methodCall.isMethodCall = true;
  methodCall.args = {receiverCall};

  std::string error;
  const primec::Definition *resolved = primec::ir_lowerer::resolveMethodCallDefinitionFromExpr(
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
      [](const primec::Expr &expr) { return expr.name; },
      defMap,
      error);
  CHECK(resolved == nullptr);
  CHECK(error == "unknown method target for tag");
}

TEST_CASE("ir lowerer setup type helper keeps reject diagnostics when alias receiver defs lack returns") {
  primec::Definition aliasAtDef;
  aliasAtDef.fullPath = "/vector/at";
  primec::Definition canonicalAtDef;
  canonicalAtDef.fullPath = "/std/collections/vector/at";
  primec::Transform returnMarker;
  returnMarker.name = "return";
  returnMarker.templateArgs = {"Marker"};
  canonicalAtDef.transforms.push_back(returnMarker);

  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {aliasAtDef.fullPath, &aliasAtDef},
      {canonicalAtDef.fullPath, &canonicalAtDef},
  };

  primec::Expr valuesExpr;
  valuesExpr.kind = primec::Expr::Kind::Name;
  valuesExpr.name = "values";

  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Literal;
  indexExpr.intWidth = 32;
  indexExpr.literalValue = 1;

  primec::Expr receiverCall;
  receiverCall.kind = primec::Expr::Kind::Call;
  receiverCall.name = "/vector/at";
  receiverCall.args = {valuesExpr, indexExpr};

  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.name = "tag";
  methodCall.isMethodCall = true;
  methodCall.args = {receiverCall};

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
            [](const primec::Expr &expr) { return expr.name; },
            defMap,
            error) == nullptr);
  CHECK(error == "unknown method target for tag");
}

TEST_CASE("ir lowerer setup type helper rejects map alias receiver call return fallback to canonical stdlib defs") {
  primec::Definition canonicalAtDef;
  canonicalAtDef.fullPath = "/std/collections/map/at";
  primec::Transform returnMarker;
  returnMarker.name = "return";
  returnMarker.templateArgs = {"Marker"};
  canonicalAtDef.transforms.push_back(returnMarker);

  primec::Definition markerTagDef;
  markerTagDef.fullPath = "/Marker/tag";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {canonicalAtDef.fullPath, &canonicalAtDef},
      {markerTagDef.fullPath, &markerTagDef},
  };

  primec::Expr valuesExpr;
  valuesExpr.kind = primec::Expr::Kind::Name;
  valuesExpr.name = "values";

  primec::Expr keyExpr;
  keyExpr.kind = primec::Expr::Kind::Literal;
  keyExpr.intWidth = 32;
  keyExpr.literalValue = 1;

  primec::Expr receiverCall;
  receiverCall.kind = primec::Expr::Kind::Call;
  receiverCall.name = "/map/at";
  receiverCall.args = {valuesExpr, keyExpr};

  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.name = "tag";
  methodCall.isMethodCall = true;
  methodCall.args = {receiverCall};

  std::string error;
  const primec::Definition *resolved = primec::ir_lowerer::resolveMethodCallDefinitionFromExpr(
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
      [](const primec::Expr &expr) { return expr.name; },
      defMap,
      error);
  CHECK(resolved == nullptr);
  CHECK(error == "unknown method target for tag");
}

TEST_CASE("ir lowerer setup type helper prefers canonical map method return structs over alias defs") {
  primec::Definition aliasAtDef;
  aliasAtDef.fullPath = "/map/at";
  primec::Transform returnAliasMarker;
  returnAliasMarker.name = "return";
  returnAliasMarker.templateArgs = {"AliasMarker"};
  aliasAtDef.transforms.push_back(returnAliasMarker);

  primec::Definition canonicalAtDef;
  canonicalAtDef.fullPath = "/std/collections/map/at";
  primec::Transform returnCanonicalMarker;
  returnCanonicalMarker.name = "return";
  returnCanonicalMarker.templateArgs = {"CanonicalMarker"};
  canonicalAtDef.transforms.push_back(returnCanonicalMarker);

  primec::Definition canonicalTagDef;
  canonicalTagDef.fullPath = "/CanonicalMarker/tag";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {aliasAtDef.fullPath, &aliasAtDef},
      {canonicalAtDef.fullPath, &canonicalAtDef},
      {canonicalTagDef.fullPath, &canonicalTagDef},
  };

  primec::Expr valuesExpr;
  valuesExpr.kind = primec::Expr::Kind::Name;
  valuesExpr.name = "values";

  primec::Expr keyExpr;
  keyExpr.kind = primec::Expr::Kind::Literal;
  keyExpr.intWidth = 32;
  keyExpr.literalValue = 1;

  primec::Expr receiverCall;
  receiverCall.kind = primec::Expr::Kind::Call;
  receiverCall.name = "at";
  receiverCall.isMethodCall = true;
  receiverCall.args = {valuesExpr, keyExpr};

  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.name = "tag";
  methodCall.isMethodCall = true;
  methodCall.args = {receiverCall};

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo valuesLocal;
  valuesLocal.kind = primec::ir_lowerer::LocalInfo::Kind::Map;
  locals.emplace("values", valuesLocal);

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
      [](const primec::Expr &expr) {
        if (expr.kind == primec::Expr::Kind::Call) {
          return std::string();
        }
        return expr.name;
      },
      defMap,
      error);
  CHECK(resolved == &canonicalTagDef);
  CHECK(error.empty());
}

TEST_CASE("ir lowerer setup type helper keeps canonical map non-struct fallback over alias defs") {
  primec::Definition aliasAtDef;
  aliasAtDef.fullPath = "/map/at";
  primec::Transform returnAliasMarker;
  returnAliasMarker.name = "return";
  returnAliasMarker.templateArgs = {"Marker"};
  aliasAtDef.transforms.push_back(returnAliasMarker);

  primec::Definition canonicalAtDef;
  canonicalAtDef.fullPath = "/std/collections/map/at";
  primec::Transform returnInt;
  returnInt.name = "return";
  returnInt.templateArgs = {"i32"};
  canonicalAtDef.transforms.push_back(returnInt);

  primec::Definition markerTagDef;
  markerTagDef.fullPath = "/Marker/tag";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {aliasAtDef.fullPath, &aliasAtDef},
      {canonicalAtDef.fullPath, &canonicalAtDef},
      {markerTagDef.fullPath, &markerTagDef},
  };

  primec::Expr valuesExpr;
  valuesExpr.kind = primec::Expr::Kind::Name;
  valuesExpr.name = "values";

  primec::Expr keyExpr;
  keyExpr.kind = primec::Expr::Kind::Literal;
  keyExpr.intWidth = 32;
  keyExpr.literalValue = 1;

  primec::Expr receiverCall;
  receiverCall.kind = primec::Expr::Kind::Call;
  receiverCall.name = "at";
  receiverCall.isMethodCall = true;
  receiverCall.args = {valuesExpr, keyExpr};

  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.name = "tag";
  methodCall.isMethodCall = true;
  methodCall.args = {receiverCall};

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo valuesLocal;
  valuesLocal.kind = primec::ir_lowerer::LocalInfo::Kind::Map;
  locals.emplace("values", valuesLocal);

  std::string error;
  CHECK(primec::ir_lowerer::resolveMethodCallDefinitionFromExpr(
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
            [](const primec::Expr &expr) {
              if (expr.kind == primec::Expr::Kind::Call) {
                return std::string();
              }
              return expr.name;
            },
            defMap,
            error) == nullptr);
  CHECK(error == "unknown method: /i32/tag");
}

TEST_CASE("ir lowerer setup type helper keeps reject diagnostics for std-namespaced vector method access") {
  primec::Definition canonicalAtDef;
  canonicalAtDef.fullPath = "/std/collections/vector/at";
  primec::Transform returnMarker;
  returnMarker.name = "return";
  returnMarker.templateArgs = {"Marker"};
  canonicalAtDef.transforms.push_back(returnMarker);

  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {canonicalAtDef.fullPath, &canonicalAtDef},
  };

  primec::Expr valuesExpr;
  valuesExpr.kind = primec::Expr::Kind::Name;
  valuesExpr.name = "values";

  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Literal;
  indexExpr.intWidth = 32;
  indexExpr.literalValue = 1;

  primec::Expr receiverCall;
  receiverCall.kind = primec::Expr::Kind::Call;
  receiverCall.name = "/std/collections/vector/at";
  receiverCall.isMethodCall = true;
  receiverCall.args = {valuesExpr, indexExpr};

  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.name = "tag";
  methodCall.isMethodCall = true;
  methodCall.args = {receiverCall};

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo valuesLocal;
  valuesLocal.kind = primec::ir_lowerer::LocalInfo::Kind::Vector;
  valuesLocal.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  locals.emplace("values", valuesLocal);

  std::string error;
  CHECK(primec::ir_lowerer::resolveMethodCallDefinitionFromExpr(
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
            [](const primec::Expr &expr) {
              if (expr.kind == primec::Expr::Kind::Call) {
                return std::string();
              }
              return expr.name;
            },
            defMap,
            error) == nullptr);
  CHECK(error == "unknown method: /Marker/tag");
}

TEST_CASE("ir lowerer setup type helper keeps reject diagnostics for map alias receiver unsafe call returns") {
  primec::Definition canonicalAtUnsafeDef;
  canonicalAtUnsafeDef.fullPath = "/std/collections/map/at_unsafe";
  primec::Transform returnMarker;
  returnMarker.name = "return";
  returnMarker.templateArgs = {"Marker"};
  canonicalAtUnsafeDef.transforms.push_back(returnMarker);

  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {canonicalAtUnsafeDef.fullPath, &canonicalAtUnsafeDef},
  };

  primec::Expr valuesExpr;
  valuesExpr.kind = primec::Expr::Kind::Name;
  valuesExpr.name = "values";

  primec::Expr keyExpr;
  keyExpr.kind = primec::Expr::Kind::Literal;
  keyExpr.intWidth = 32;
  keyExpr.literalValue = 1;

  primec::Expr receiverCall;
  receiverCall.kind = primec::Expr::Kind::Call;
  receiverCall.name = "/map/at_unsafe";
  receiverCall.args = {valuesExpr, keyExpr};

  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.name = "tag";
  methodCall.isMethodCall = true;
  methodCall.args = {receiverCall};

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
            [](const primec::Expr &expr) { return expr.name; },
            defMap,
            error) == nullptr);
  CHECK(error == "unknown method target for tag");
}

TEST_SUITE_END();
