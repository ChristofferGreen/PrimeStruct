#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir lowerer setup type helper keeps reject diagnostics for canonical map helper receiver calls") {
  primec::Definition i32TagDef;
  i32TagDef.fullPath = "/i32/tag";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {i32TagDef.fullPath, &i32TagDef},
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
  receiverCall.name = "/std/collections/map/at";
  receiverCall.args = {valuesExpr, keyExpr};

  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.name = "tag";
  methodCall.isMethodCall = true;
  methodCall.args = {receiverCall};

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo valuesLocal;
  valuesLocal.kind = primec::ir_lowerer::LocalInfo::Kind::Map;
  valuesLocal.mapValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
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
            [](const primec::Expr &expr) { return expr.name; },
            defMap,
            error) == nullptr);
  CHECK(error == "unknown method target for tag");
}

TEST_CASE("ir lowerer setup type helper rejects bare map access primitive receiver fallback") {
  primec::Definition i32TagDef;
  i32TagDef.fullPath = "/i32/tag";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {i32TagDef.fullPath, &i32TagDef},
  };

  primec::Expr valuesExpr;
  valuesExpr.kind = primec::Expr::Kind::Name;
  valuesExpr.name = "values";

  primec::Expr keyExpr;
  keyExpr.kind = primec::Expr::Kind::Literal;
  keyExpr.intWidth = 32;
  keyExpr.literalValue = 1;

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo valuesLocal;
  valuesLocal.kind = primec::ir_lowerer::LocalInfo::Kind::Map;
  valuesLocal.mapValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  locals.emplace("values", valuesLocal);

  auto inferExprKind = [](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &localsIn) {
    primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
    if (primec::ir_lowerer::resolveMethodCallReturnKind(
            expr,
            localsIn,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return nullptr; },
            {},
            {},
            false,
            kindOut,
            nullptr)) {
      return kindOut;
    }
    return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  };

  auto expectReject = [&](const char *receiverName, bool isMethodCall) {
    primec::Expr receiverCall;
    receiverCall.kind = primec::Expr::Kind::Call;
    receiverCall.name = receiverName;
    receiverCall.isMethodCall = isMethodCall;
    receiverCall.args = {valuesExpr, keyExpr};

    primec::Expr methodCall;
    methodCall.kind = primec::Expr::Kind::Call;
    methodCall.name = "tag";
    methodCall.isMethodCall = true;
    methodCall.args = {receiverCall};

    std::string error;
    const primec::Definition *resolved = primec::ir_lowerer::resolveMethodCallDefinitionFromExpr(
        methodCall,
        locals,
        [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
        [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
        [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
        {},
        {},
        inferExprKind,
        [](const primec::Expr &expr) { return expr.name; },
        {},
        defMap,
        error);
    CHECK(resolved == nullptr);
    CHECK(error == "unknown method target for tag");
  };

  expectReject("at", false);
  expectReject("at", true);
  expectReject("at_unsafe", false);
  expectReject("at_unsafe", true);
}

TEST_CASE("ir lowerer setup type helper rejects bare map tryAt receiver fallback") {
  primec::Definition i32TagDef;
  i32TagDef.fullPath = "/i32/tag";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {i32TagDef.fullPath, &i32TagDef},
  };

  primec::Expr valuesExpr;
  valuesExpr.kind = primec::Expr::Kind::Name;
  valuesExpr.name = "values";

  primec::Expr keyExpr;
  keyExpr.kind = primec::Expr::Kind::Literal;
  keyExpr.intWidth = 32;
  keyExpr.literalValue = 1;

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo valuesLocal;
  valuesLocal.kind = primec::ir_lowerer::LocalInfo::Kind::Map;
  valuesLocal.mapValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  locals.emplace("values", valuesLocal);

  auto inferExprKind = [](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &localsIn) {
    primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
    if (primec::ir_lowerer::resolveMethodCallReturnKind(
            expr,
            localsIn,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return nullptr; },
            {},
            {},
            false,
            kindOut,
            nullptr)) {
      return kindOut;
    }
    return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  };

  auto expectReject = [&](bool isMethodCall) {
    primec::Expr receiverCall;
    receiverCall.kind = primec::Expr::Kind::Call;
    receiverCall.name = "tryAt";
    receiverCall.isMethodCall = isMethodCall;
    receiverCall.args = {valuesExpr, keyExpr};

    primec::Expr methodCall;
    methodCall.kind = primec::Expr::Kind::Call;
    methodCall.name = "tag";
    methodCall.isMethodCall = true;
    methodCall.args = {receiverCall};

    std::string error;
    const primec::Definition *resolved = primec::ir_lowerer::resolveMethodCallDefinitionFromExpr(
        methodCall,
        locals,
        [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
        [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
        [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
        {},
        {},
        inferExprKind,
        [](const primec::Expr &expr) { return expr.name; },
        {},
        defMap,
        error);
    CHECK(resolved == nullptr);
    CHECK(error == "unknown method target for tag");
  };

  expectReject(false);
  expectReject(true);
}

TEST_CASE("ir lowerer setup type helper rejects alias receiver fallback when expr path is unavailable") {
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
  receiverCall.name = "at";
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
  CHECK(resolved == nullptr);
  CHECK(error == "unknown method target for tag");
}

TEST_CASE("ir lowerer setup type helper keeps reject diagnostics when expr path is unavailable") {
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
  receiverCall.name = "at";
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
  CHECK(error == "unknown method target for tag");
}

TEST_CASE("ir lowerer setup type helper keeps auto-wrapper primitive diagnostics for vector alias receiver calls") {
  primec::Definition projectDef;
  projectDef.fullPath = "/project";
  primec::Transform returnAuto;
  returnAuto.name = "return";
  returnAuto.templateArgs = {"auto"};
  projectDef.transforms.push_back(returnAuto);

  primec::Definition i32TagDef;
  i32TagDef.fullPath = "/i32/tag";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {projectDef.fullPath, &projectDef},
      {i32TagDef.fullPath, &i32TagDef},
  };

  primec::Expr valuesExpr;
  valuesExpr.kind = primec::Expr::Kind::Name;
  valuesExpr.name = "values";

  primec::Expr receiverCall;
  receiverCall.kind = primec::Expr::Kind::Call;
  receiverCall.name = "project";
  receiverCall.args = {valuesExpr};

  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.name = "tag";
  methodCall.isMethodCall = true;
  methodCall.args = {receiverCall};

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo valuesLocal;
  valuesLocal.kind = primec::ir_lowerer::LocalInfo::Kind::Vector;
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
        if (expr.kind == primec::Expr::Kind::Call && expr.name == "project") {
          return std::string("/project");
        }
        return expr.name;
      },
      [&](const std::string &path, primec::ir_lowerer::ReturnInfo &infoOut) {
        if (path != "/project") {
          return false;
        }
        infoOut.kind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
        return true;
      },
      defMap,
      error);
  CHECK(resolved == &i32TagDef);
  CHECK(error.empty());
}

TEST_CASE("ir lowerer setup type helper rejects direct alias primitive fallback from inferred receiver kinds") {
  primec::Definition i32TagDef;
  i32TagDef.fullPath = "/i32/tag";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {i32TagDef.fullPath, &i32TagDef},
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
      [](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
        if (expr.kind == primec::Expr::Kind::Call && expr.name == "/vector/at") {
          return primec::ir_lowerer::LocalInfo::ValueKind::Int32;
        }
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &expr) { return expr.name; },
      {},
      defMap,
      error);
  CHECK(resolved == &i32TagDef);
  CHECK(error.empty());
}

TEST_CASE("ir lowerer setup type helper rejects slash-method alias primitive fallback from inferred receiver kinds") {
  primec::Definition i32TagDef;
  i32TagDef.fullPath = "/i32/tag";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {i32TagDef.fullPath, &i32TagDef},
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

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo valuesLocal;
  valuesLocal.kind = primec::ir_lowerer::LocalInfo::Kind::Vector;
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
      [](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
        if (expr.kind == primec::Expr::Kind::Call && expr.isMethodCall && expr.name == "/vector/at") {
          return primec::ir_lowerer::LocalInfo::ValueKind::Int32;
        }
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &expr) { return expr.name; },
      {},
      defMap,
      error);
  CHECK(resolved == nullptr);
  CHECK(error == "unknown method target for tag");
}

TEST_CASE("ir lowerer setup type helper keeps explicit vector access receiver same-path precedence") {
  primec::Definition aliasAtDef;
  aliasAtDef.fullPath = "/vector/at";
  primec::Transform returnAliasMarker;
  returnAliasMarker.name = "return";
  returnAliasMarker.templateArgs = {"/pkg/AliasMarker"};
  aliasAtDef.transforms.push_back(returnAliasMarker);

  primec::Definition aliasAtUnsafeDef;
  aliasAtUnsafeDef.fullPath = "/vector/at_unsafe";
  aliasAtUnsafeDef.transforms.push_back(returnAliasMarker);

  primec::Definition canonicalAtDef;
  canonicalAtDef.fullPath = "/std/collections/vector/at";
  primec::Transform returnCanonicalMarker;
  returnCanonicalMarker.name = "return";
  returnCanonicalMarker.templateArgs = {"/pkg/CanonicalMarker"};
  canonicalAtDef.transforms.push_back(returnCanonicalMarker);

  primec::Definition canonicalAtUnsafeDef;
  canonicalAtUnsafeDef.fullPath = "/std/collections/vector/at_unsafe";
  canonicalAtUnsafeDef.transforms.push_back(returnCanonicalMarker);

  primec::Definition aliasTagDef;
  aliasTagDef.fullPath = "/pkg/AliasMarker/tag";
  primec::Definition canonicalTagDef;
  canonicalTagDef.fullPath = "/pkg/CanonicalMarker/tag";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {aliasAtDef.fullPath, &aliasAtDef},
      {aliasAtUnsafeDef.fullPath, &aliasAtUnsafeDef},
      {canonicalAtDef.fullPath, &canonicalAtDef},
      {canonicalAtUnsafeDef.fullPath, &canonicalAtUnsafeDef},
      {aliasTagDef.fullPath, &aliasTagDef},
      {canonicalTagDef.fullPath, &canonicalTagDef},
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
  receiverCall.isMethodCall = false;
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
      {},
      defMap,
      error);
  CHECK(resolved == &aliasTagDef);
  CHECK(error.empty());

  receiverCall.name = "/std/collections/vector/at";
  methodCall.args = {receiverCall};
  error.clear();
  resolved = primec::ir_lowerer::resolveMethodCallDefinitionFromExpr(
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
      {},
      defMap,
      error);
  CHECK(resolved == &canonicalTagDef);
  CHECK(error.empty());

  receiverCall.name = "/vector/at_unsafe";
  receiverCall.isMethodCall = true;
  methodCall.args = {receiverCall};
  error.clear();
  resolved = primec::ir_lowerer::resolveMethodCallDefinitionFromExpr(
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
      {},
      defMap,
      error);
  CHECK(resolved == &aliasTagDef);
  CHECK(error.empty());

  receiverCall.name = "/std/collections/vector/at_unsafe";
  receiverCall.isMethodCall = true;
  methodCall.args = {receiverCall};
  error.clear();
  resolved = primec::ir_lowerer::resolveMethodCallDefinitionFromExpr(
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
      {},
      defMap,
      error);
  CHECK(resolved == &canonicalTagDef);
  CHECK(error.empty());
}

TEST_CASE("ir lowerer setup type helper rejects slash-method vector alias primitive receiver fallback") {
  primec::Definition i32TagDef;
  i32TagDef.fullPath = "/i32/tag";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {i32TagDef.fullPath, &i32TagDef},
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

  auto inferExprKind = [](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &localsIn) {
    primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
    if (primec::ir_lowerer::resolveMethodCallReturnKind(
            expr,
            localsIn,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return nullptr; },
            {},
            {},
            false,
            kindOut,
            nullptr)) {
      return kindOut;
    }
    return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  };

  std::string error;
  const primec::Definition *resolved = primec::ir_lowerer::resolveMethodCallDefinitionFromExpr(
      methodCall,
      locals,
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      {},
      {},
      inferExprKind,
      [](const primec::Expr &expr) { return expr.name; },
      {},
      defMap,
      error);
  CHECK(resolved == nullptr);
  CHECK_FALSE(error.empty());
}

TEST_CASE("ir lowerer setup type helper rejects wrapper string access primitive receiver fallback") {
  primec::Definition i32TagDef;
  i32TagDef.fullPath = "/i32/tag";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {i32TagDef.fullPath, &i32TagDef},
  };

  primec::Expr wrapTextCall;
  wrapTextCall.kind = primec::Expr::Kind::Call;
  wrapTextCall.name = "wrapText";

  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Literal;
  indexExpr.intWidth = 32;
  indexExpr.literalValue = 1;

  primec::Expr receiverCall;
  receiverCall.kind = primec::Expr::Kind::Call;
  receiverCall.name = "/std/collections/vector/at";
  receiverCall.args = {wrapTextCall, indexExpr};

  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.name = "tag";
  methodCall.isMethodCall = true;
  methodCall.args = {receiverCall};

  auto getReturnInfo = [&](const std::string &path, primec::ir_lowerer::ReturnInfo &infoOut) {
    if (path != "/wrapText") {
      return false;
    }
    infoOut.kind = primec::ir_lowerer::LocalInfo::ValueKind::String;
    infoOut.returnsVoid = false;
    infoOut.returnsArray = false;
    return true;
  };

  auto inferExprKind = [&](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &localsIn) {
    if (expr.kind == primec::Expr::Kind::Call && expr.name == "wrapText") {
      return primec::ir_lowerer::LocalInfo::ValueKind::String;
    }
    primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
    if (primec::ir_lowerer::resolveMethodCallReturnKind(
            expr,
            localsIn,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return nullptr; },
            {},
            getReturnInfo,
            false,
            kindOut,
            nullptr)) {
      return kindOut;
    }
    return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  };

  std::string error;
  const primec::Definition *resolved = primec::ir_lowerer::resolveMethodCallDefinitionFromExpr(
      methodCall,
      {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      {},
      {},
      inferExprKind,
      [](const primec::Expr &expr) {
        if (expr.kind == primec::Expr::Kind::Call && expr.name == "wrapText") {
          return std::string("/wrapText");
        }
        return expr.name;
      },
      getReturnInfo,
      defMap,
      error);
  CHECK(resolved == nullptr);
  CHECK(error == "unknown method target for tag");
}

TEST_SUITE_END();
