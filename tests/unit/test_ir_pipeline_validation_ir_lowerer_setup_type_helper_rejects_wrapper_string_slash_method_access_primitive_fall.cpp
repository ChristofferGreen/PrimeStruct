#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir lowerer setup type helper rejects wrapper string slash-method access primitive fallback") {
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
  receiverCall.isMethodCall = true;
  receiverCall.name = "/std/collections/vector/at";
  receiverCall.args = {wrapTextCall, indexExpr};

  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.name = "tag";
  methodCall.isMethodCall = true;
  methodCall.args = {receiverCall};

  auto inferExprKind = [](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
    if (expr.kind == primec::Expr::Kind::Call && expr.name == "wrapText") {
      return primec::ir_lowerer::LocalInfo::ValueKind::String;
    }
    return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  };

  auto resolveExprPath = [](const primec::Expr &expr) {
    if (expr.kind == primec::Expr::Kind::Call && expr.name == "wrapText") {
      return std::string("/wrapText");
    }
    return expr.name;
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
      resolveExprPath,
      {},
      defMap,
      error);
  CHECK(resolved == nullptr);
  CHECK(error == "unknown method: /string/at");

  receiverCall.name = "/vector/at_unsafe";
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
      inferExprKind,
      resolveExprPath,
      {},
      defMap,
      error);
  CHECK(resolved == nullptr);
  CHECK_FALSE(error.empty());
}

TEST_CASE("ir lowerer setup type helper keeps parser-shaped canonical vector receiver routed diagnostics") {
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

  auto resolveExprPath = [](const primec::Expr &expr) {
    if (!expr.namespacePrefix.empty()) {
      return expr.namespacePrefix + "/" + expr.name;
    }
    return expr.name;
  };

  primec::Expr receiverCall;
  receiverCall.kind = primec::Expr::Kind::Call;
  receiverCall.name = "at";
  receiverCall.namespacePrefix = "/std/collections/vector";
  receiverCall.isMethodCall = true;
  receiverCall.args = {valuesExpr, indexExpr};

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
      resolveExprPath,
      {},
      defMap,
      error);
  CHECK(resolved == nullptr);
  CHECK(error == "unknown method: /std/collections/vector/at");

  receiverCall.name = "capacity";
  receiverCall.args = {valuesExpr};
  methodCall.args = {receiverCall};
  error.clear();
  resolved = primec::ir_lowerer::resolveMethodCallDefinitionFromExpr(
      methodCall,
      locals,
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      {},
      {},
      inferExprKind,
      resolveExprPath,
      {},
      defMap,
      error);
  CHECK(resolved == nullptr);
  CHECK(error == "unknown method: /std/collections/vector/capacity");
}

TEST_CASE("ir lowerer setup type helper keeps reject diagnostics for explicit slash-method map access receivers") {
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
}

TEST_CASE("ir lowerer setup type helper rejects array compatibility slash-method vector access primitive fallback") {
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
  receiverCall.name = "/array/at";
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
  CHECK(error == "unknown method target for tag");

  receiverCall.name = "/array/at_unsafe";
  methodCall.args = {receiverCall};
  error.clear();
  resolved = primec::ir_lowerer::resolveMethodCallDefinitionFromExpr(
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
}

TEST_CASE("ir lowerer setup type helper rejects array compatibility slash-method vector count primitive fallback") {
  primec::Definition i32TagDef;
  i32TagDef.fullPath = "/i32/tag";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {i32TagDef.fullPath, &i32TagDef},
  };

  primec::Expr valuesExpr;
  valuesExpr.kind = primec::Expr::Kind::Name;
  valuesExpr.name = "values";

  primec::Expr markerExpr;
  markerExpr.kind = primec::Expr::Kind::BoolLiteral;
  markerExpr.boolValue = true;

  primec::Expr receiverCall;
  receiverCall.kind = primec::Expr::Kind::Call;
  receiverCall.name = "/array/count";
  receiverCall.isMethodCall = true;
  receiverCall.args = {valuesExpr, markerExpr};

  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.name = "tag";
  methodCall.isMethodCall = true;
  methodCall.args = {receiverCall};

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo valuesLocal;
  valuesLocal.kind = primec::ir_lowerer::LocalInfo::Kind::Vector;
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
  CHECK(error == "unknown method target for tag");
}

TEST_CASE("ir lowerer setup type helper rejects array compatibility slash-method vector capacity primitive fallback") {
  primec::Definition i32TagDef;
  i32TagDef.fullPath = "/i32/tag";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {i32TagDef.fullPath, &i32TagDef},
  };

  primec::Expr valuesExpr;
  valuesExpr.kind = primec::Expr::Kind::Name;
  valuesExpr.name = "values";

  primec::Expr receiverCall;
  receiverCall.kind = primec::Expr::Kind::Call;
  receiverCall.name = "/array/capacity";
  receiverCall.isMethodCall = true;
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
  CHECK(error == "unknown method target for tag");
}

TEST_CASE("ir lowerer setup type helper preserves wrapper vector slash-method count diagnostics") {
  primec::Definition wrapVectorDef;
  wrapVectorDef.fullPath = "/wrapVector";
  primec::Transform returnVector;
  returnVector.name = "return";
  returnVector.templateArgs = {"vector<i32>"};
  wrapVectorDef.transforms.push_back(returnVector);

  primec::Definition i32TagDef;
  i32TagDef.fullPath = "/i32/tag";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {wrapVectorDef.fullPath, &wrapVectorDef},
      {i32TagDef.fullPath, &i32TagDef},
  };

  primec::Expr wrapVectorCall;
  wrapVectorCall.kind = primec::Expr::Kind::Call;
  wrapVectorCall.name = "wrapVector";

  primec::Expr receiverCall;
  receiverCall.kind = primec::Expr::Kind::Call;
  receiverCall.name = "/vector/count";
  receiverCall.isMethodCall = true;
  receiverCall.args = {wrapVectorCall};

  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.name = "tag";
  methodCall.isMethodCall = true;
  methodCall.args = {receiverCall};

  auto resolveExprPath = [](const primec::Expr &expr) {
    if (expr.kind == primec::Expr::Kind::Call && expr.name == "wrapVector") {
      return std::string("/wrapVector");
    }
    return expr.name;
  };

  auto inferExprKind = [&](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &localsIn) {
    primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
    if (primec::ir_lowerer::resolveMethodCallReturnKind(
            expr,
            localsIn,
            [&](const primec::Expr &candidate, const primec::ir_lowerer::LocalMap &candidateLocals) {
              std::string nestedError;
              return primec::ir_lowerer::resolveMethodCallDefinitionFromExpr(
                  candidate,
                  candidateLocals,
                  [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
                  [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
                  [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
                  {},
                  {},
                  {},
                  resolveExprPath,
                  {},
                  defMap,
                  nestedError);
            },
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
      {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      {},
      {},
      inferExprKind,
      resolveExprPath,
      {},
      defMap,
      error);
  CHECK(resolved == nullptr);
  CHECK(error == "unknown method: /vector/count");
}

TEST_CASE("ir lowerer setup type helper preserves wrapper vector slash-method capacity diagnostics") {
  primec::Definition wrapVectorDef;
  wrapVectorDef.fullPath = "/wrapVector";
  primec::Transform returnVector;
  returnVector.name = "return";
  returnVector.templateArgs = {"vector<i32>"};
  wrapVectorDef.transforms.push_back(returnVector);

  primec::Definition i32TagDef;
  i32TagDef.fullPath = "/i32/tag";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {wrapVectorDef.fullPath, &wrapVectorDef},
      {i32TagDef.fullPath, &i32TagDef},
  };

  primec::Expr wrapVectorCall;
  wrapVectorCall.kind = primec::Expr::Kind::Call;
  wrapVectorCall.name = "wrapVector";

  primec::Expr receiverCall;
  receiverCall.kind = primec::Expr::Kind::Call;
  receiverCall.name = "/vector/capacity";
  receiverCall.isMethodCall = true;
  receiverCall.args = {wrapVectorCall};

  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.name = "tag";
  methodCall.isMethodCall = true;
  methodCall.args = {receiverCall};

  auto resolveExprPath = [](const primec::Expr &expr) {
    if (expr.kind == primec::Expr::Kind::Call && expr.name == "wrapVector") {
      return std::string("/wrapVector");
    }
    return expr.name;
  };

  auto inferExprKind = [&](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &localsIn) {
    primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
    if (primec::ir_lowerer::resolveMethodCallReturnKind(
            expr,
            localsIn,
            [&](const primec::Expr &candidate, const primec::ir_lowerer::LocalMap &candidateLocals) {
              std::string nestedError;
              return primec::ir_lowerer::resolveMethodCallDefinitionFromExpr(
                  candidate,
                  candidateLocals,
                  [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
                  [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
                  [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
                  {},
                  {},
                  {},
                  resolveExprPath,
                  {},
                  defMap,
                  nestedError);
            },
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
      {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      {},
      {},
      inferExprKind,
      resolveExprPath,
      {},
      defMap,
      error);
  CHECK(resolved == nullptr);
  CHECK(error == "unknown method: /vector/capacity");
}

TEST_CASE("ir lowerer setup type helper keeps reject diagnostics for wrapper-returned explicit slash-method map access") {
  primec::Definition wrapMapDef;
  wrapMapDef.fullPath = "/wrapMap";
  primec::Transform returnMap;
  returnMap.name = "return";
  returnMap.templateArgs = {"map<i32, i32>"};
  wrapMapDef.transforms.push_back(returnMap);

  primec::Definition i32TagDef;
  i32TagDef.fullPath = "/i32/tag";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {wrapMapDef.fullPath, &wrapMapDef},
      {i32TagDef.fullPath, &i32TagDef},
  };

  primec::Expr wrapMapCall;
  wrapMapCall.kind = primec::Expr::Kind::Call;
  wrapMapCall.name = "wrapMap";

  primec::Expr keyExpr;
  keyExpr.kind = primec::Expr::Kind::Literal;
  keyExpr.intWidth = 32;
  keyExpr.literalValue = 1;

  primec::Expr receiverCall;
  receiverCall.kind = primec::Expr::Kind::Call;
  receiverCall.name = "/std/collections/map/at";
  receiverCall.isMethodCall = true;
  receiverCall.args = {wrapMapCall, keyExpr};

  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.name = "tag";
  methodCall.isMethodCall = true;
  methodCall.args = {receiverCall};

  auto resolveExprPath = [](const primec::Expr &expr) {
    if (expr.kind == primec::Expr::Kind::Call && expr.name == "wrapMap") {
      return std::string("/wrapMap");
    }
    return expr.name;
  };

  auto inferExprKind = [&](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &localsIn) {
    primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
    if (primec::ir_lowerer::resolveMethodCallReturnKind(
            expr,
            localsIn,
            [&](const primec::Expr &candidate, const primec::ir_lowerer::LocalMap &candidateLocals) {
              std::string nestedError;
              return primec::ir_lowerer::resolveMethodCallDefinitionFromExpr(
                  candidate,
                  candidateLocals,
                  [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
                  [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
                  [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
                  {},
                  {},
                  {},
                  resolveExprPath,
                  {},
                  defMap,
                  nestedError);
            },
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
      {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      {},
      {},
      inferExprKind,
      resolveExprPath,
      {},
      defMap,
      error);
  CHECK(resolved == nullptr);
  CHECK(error == "unknown method target for tag");
}

TEST_CASE("ir lowerer setup type helper keeps reject diagnostics for explicit map tryAt receivers") {
  primec::Definition canonicalTryAtDef;
  canonicalTryAtDef.fullPath = "/std/collections/map/tryAt";
  canonicalTryAtDef.namespacePrefix = "/std/collections/map";
  primec::Transform returnResult;
  returnResult.name = "return";
  returnResult.templateArgs = {"/pkg/CanonicalResult"};
  canonicalTryAtDef.transforms.push_back(returnResult);

  primec::Definition resultTagDef;
  resultTagDef.fullPath = "/pkg/CanonicalResult/tag";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {canonicalTryAtDef.fullPath, &canonicalTryAtDef},
      {resultTagDef.fullPath, &resultTagDef},
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
  receiverCall.name = "/map/tryAt";
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
      [](const primec::Expr &expr) { return expr.name; },
      {},
      defMap,
      error);
  CHECK(resolved == nullptr);
  CHECK(error == "unknown method target for tag");
}

TEST_CASE("ir lowerer setup type helper keeps reject diagnostics for explicit map count and contains receivers") {
  primec::Definition intTagDef;
  intTagDef.fullPath = "/i32/tag";
  primec::Definition boolTagDef;
  boolTagDef.fullPath = "/bool/tag";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {intTagDef.fullPath, &intTagDef},
      {boolTagDef.fullPath, &boolTagDef},
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

  auto expectReject = [&](const char *receiverName, primec::ir_lowerer::LocalInfo::ValueKind inferredKind) {
    const std::string receiverNameStr = receiverName;
    primec::Expr receiverCall;
    receiverCall.kind = primec::Expr::Kind::Call;
    receiverCall.name = receiverName;
    receiverCall.args = {valuesExpr};
    if (receiverNameStr.find("contains") != std::string::npos) {
      receiverCall.args.push_back(keyExpr);
    }

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
        [&](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
          return expr.kind == primec::Expr::Kind::Call && expr.name == receiverName
                     ? inferredKind
                     : primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
        },
        [](const primec::Expr &expr) { return expr.name; },
        {},
        defMap,
        error);
    CHECK(resolved == nullptr);
    CHECK(error == "unknown method target for tag");
  };

  expectReject("/map/count", primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  expectReject("/std/collections/map/count", primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  expectReject("/map/contains", primec::ir_lowerer::LocalInfo::ValueKind::Bool);
  expectReject("/std/collections/map/contains", primec::ir_lowerer::LocalInfo::ValueKind::Bool);
}

TEST_CASE("ir lowerer setup type helper keeps explicit map count and contains receiver same-path precedence") {
  primec::Definition aliasCountDef;
  aliasCountDef.fullPath = "/map/count";
  aliasCountDef.namespacePrefix = "/map";
  primec::Transform returnAliasCount;
  returnAliasCount.name = "return";
  returnAliasCount.templateArgs = {"/pkg/AliasCountResult"};
  aliasCountDef.transforms.push_back(returnAliasCount);

  primec::Definition canonicalCountDef;
  canonicalCountDef.fullPath = "/std/collections/map/count";
  canonicalCountDef.namespacePrefix = "/std/collections/map";
  primec::Transform returnCanonicalCount;
  returnCanonicalCount.name = "return";
  returnCanonicalCount.templateArgs = {"/pkg/CanonicalCountResult"};
  canonicalCountDef.transforms.push_back(returnCanonicalCount);

  primec::Definition aliasContainsDef;
  aliasContainsDef.fullPath = "/map/contains";
  aliasContainsDef.namespacePrefix = "/map";
  primec::Transform returnAliasContains;
  returnAliasContains.name = "return";
  returnAliasContains.templateArgs = {"/pkg/AliasContainsResult"};
  aliasContainsDef.transforms.push_back(returnAliasContains);

  primec::Definition canonicalContainsDef;
  canonicalContainsDef.fullPath = "/std/collections/map/contains";
  canonicalContainsDef.namespacePrefix = "/std/collections/map";
  primec::Transform returnCanonicalContains;
  returnCanonicalContains.name = "return";
  returnCanonicalContains.templateArgs = {"/pkg/CanonicalContainsResult"};
  canonicalContainsDef.transforms.push_back(returnCanonicalContains);

  primec::Definition aliasCountTagDef;
  aliasCountTagDef.fullPath = "/pkg/AliasCountResult/tag";
  primec::Definition canonicalCountTagDef;
  canonicalCountTagDef.fullPath = "/pkg/CanonicalCountResult/tag";
  primec::Definition aliasContainsTagDef;
  aliasContainsTagDef.fullPath = "/pkg/AliasContainsResult/tag";
  primec::Definition canonicalContainsTagDef;
  canonicalContainsTagDef.fullPath = "/pkg/CanonicalContainsResult/tag";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {aliasCountDef.fullPath, &aliasCountDef},
      {canonicalCountDef.fullPath, &canonicalCountDef},
      {aliasContainsDef.fullPath, &aliasContainsDef},
      {canonicalContainsDef.fullPath, &canonicalContainsDef},
      {aliasCountTagDef.fullPath, &aliasCountTagDef},
      {canonicalCountTagDef.fullPath, &canonicalCountTagDef},
      {aliasContainsTagDef.fullPath, &aliasContainsTagDef},
      {canonicalContainsTagDef.fullPath, &canonicalContainsTagDef},
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
  locals.emplace("values", valuesLocal);

  auto expectResolvedTag = [&](const char *receiverName, const primec::Definition *expectedTagDef) {
    const std::string receiverNameStr = receiverName;
    primec::Expr receiverCall;
    receiverCall.kind = primec::Expr::Kind::Call;
    receiverCall.name = receiverName;
    receiverCall.args = {valuesExpr};
    if (receiverNameStr.find("contains") != std::string::npos) {
      receiverCall.args.push_back(keyExpr);
    }

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
        [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
          return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
        },
        [](const primec::Expr &expr) { return expr.name; },
        {},
        defMap,
        error);
    CHECK(resolved == expectedTagDef);
    CHECK(error.empty());
  };

  expectResolvedTag("/map/count", &aliasCountTagDef);
  expectResolvedTag("/std/collections/map/count", &canonicalCountTagDef);
  expectResolvedTag("/map/contains", &aliasContainsTagDef);
  expectResolvedTag("/std/collections/map/contains", &canonicalContainsTagDef);
}

TEST_CASE("ir lowerer setup type helper keeps bare map method receiver canonical precedence") {
  auto expectResolvedTag = [](const std::string &methodName) {
    primec::Definition aliasDef;
    aliasDef.fullPath = "/map/" + methodName;
    aliasDef.namespacePrefix = "/map";
    primec::Transform returnAliasResult;
    returnAliasResult.name = "return";
    returnAliasResult.templateArgs = {"/pkg/AliasResult"};
    aliasDef.transforms.push_back(returnAliasResult);

    primec::Definition canonicalDef;
    canonicalDef.fullPath = "/std/collections/map/" + methodName;
    canonicalDef.namespacePrefix = "/std/collections/map";
    primec::Transform returnCanonicalResult;
    returnCanonicalResult.name = "return";
    returnCanonicalResult.templateArgs = {"/pkg/CanonicalResult"};
    canonicalDef.transforms.push_back(returnCanonicalResult);

    primec::Definition aliasTagDef;
    aliasTagDef.fullPath = "/pkg/AliasResult/tag";
    primec::Definition canonicalTagDef;
    canonicalTagDef.fullPath = "/pkg/CanonicalResult/tag";
    const std::unordered_map<std::string, const primec::Definition *> defMap = {
        {aliasDef.fullPath, &aliasDef},
        {canonicalDef.fullPath, &canonicalDef},
        {aliasTagDef.fullPath, &aliasTagDef},
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
    receiverCall.name = methodName;
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
        [](const primec::Expr &expr) { return expr.name; },
        {},
        defMap,
        error);
    CHECK(resolved == &canonicalTagDef);
    CHECK(error.empty());
  };

  expectResolvedTag("contains");
  expectResolvedTag("tryAt");
}

TEST_CASE("ir lowerer setup type helper rejects bare map method receiver alias fallback") {
  auto expectReject = [](const std::string &methodName) {
    primec::Definition aliasDef;
    aliasDef.fullPath = "/map/" + methodName;
    aliasDef.namespacePrefix = "/map";
    primec::Transform returnAliasResult;
    returnAliasResult.name = "return";
    returnAliasResult.templateArgs = {"/pkg/AliasResult"};
    aliasDef.transforms.push_back(returnAliasResult);

    primec::Definition aliasTagDef;
    aliasTagDef.fullPath = "/pkg/AliasResult/tag";
    const std::unordered_map<std::string, const primec::Definition *> defMap = {
        {aliasDef.fullPath, &aliasDef},
        {aliasTagDef.fullPath, &aliasTagDef},
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
    receiverCall.name = methodName;
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
        [](const primec::Expr &expr) { return expr.name; },
        {},
        defMap,
        error);
    CHECK(resolved == nullptr);
    CHECK(error == "unknown method target for tag");
  };

  expectReject("contains");
  expectReject("tryAt");
}

TEST_CASE("ir lowerer setup type helper keeps explicit map tryAt receiver alias precedence") {
  primec::Definition aliasTryAtDef;
  aliasTryAtDef.fullPath = "/map/tryAt";
  aliasTryAtDef.namespacePrefix = "/map";
  primec::Transform returnAliasResult;
  returnAliasResult.name = "return";
  returnAliasResult.templateArgs = {"/pkg/AliasResult"};
  aliasTryAtDef.transforms.push_back(returnAliasResult);

  primec::Definition canonicalTryAtDef;
  canonicalTryAtDef.fullPath = "/std/collections/map/tryAt";
  canonicalTryAtDef.namespacePrefix = "/std/collections/map";
  primec::Transform returnCanonicalResult;
  returnCanonicalResult.name = "return";
  returnCanonicalResult.templateArgs = {"/pkg/CanonicalResult"};
  canonicalTryAtDef.transforms.push_back(returnCanonicalResult);

  primec::Definition aliasTagDef;
  aliasTagDef.fullPath = "/pkg/AliasResult/tag";
  primec::Definition canonicalTagDef;
  canonicalTagDef.fullPath = "/pkg/CanonicalResult/tag";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {aliasTryAtDef.fullPath, &aliasTryAtDef},
      {canonicalTryAtDef.fullPath, &canonicalTryAtDef},
      {aliasTagDef.fullPath, &aliasTagDef},
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
  receiverCall.name = "/map/tryAt";
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
      [](const primec::Expr &expr) { return expr.name; },
      {},
      defMap,
      error);
  CHECK(resolved == &aliasTagDef);
  CHECK(error.empty());
}

TEST_CASE("ir lowerer setup type helper resolves dereferenced indexed variadic map receivers") {
  primec::Definition containsDef;
  containsDef.fullPath = "/std/collections/map/contains";
  primec::Definition atUnsafeDef;
  atUnsafeDef.fullPath = "/std/collections/map/at_unsafe";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {containsDef.fullPath, &containsDef},
      {atUnsafeDef.fullPath, &atUnsafeDef},
  };

  auto makeMethodCall = [](const std::string &packName,
                           const std::string &methodName,
                           int64_t keyValue) {
    primec::Expr packNameExpr;
    packNameExpr.kind = primec::Expr::Kind::Name;
    packNameExpr.name = packName;

    primec::Expr indexExpr;
    indexExpr.kind = primec::Expr::Kind::Literal;
    indexExpr.intWidth = 32;
    indexExpr.literalValue = 0;

    primec::Expr accessExpr;
    accessExpr.kind = primec::Expr::Kind::Call;
    accessExpr.name = "at";
    accessExpr.args = {packNameExpr, indexExpr};

    primec::Expr derefExpr;
    derefExpr.kind = primec::Expr::Kind::Call;
    derefExpr.name = "dereference";
    derefExpr.args = {accessExpr};

    primec::Expr keyExpr;
    keyExpr.kind = primec::Expr::Kind::Literal;
    keyExpr.intWidth = 32;
    keyExpr.literalValue = keyValue;

    primec::Expr methodCall;
    methodCall.kind = primec::Expr::Kind::Call;
    methodCall.name = methodName;
    methodCall.isMethodCall = true;
    methodCall.args = {derefExpr, keyExpr};
    return methodCall;
  };

  primec::ir_lowerer::LocalMap locals;

  primec::ir_lowerer::LocalInfo borrowedPackInfo;
  borrowedPackInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  borrowedPackInfo.isArgsPack = true;
  borrowedPackInfo.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Reference;
  borrowedPackInfo.referenceToMap = true;
  borrowedPackInfo.mapKeyKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  borrowedPackInfo.mapValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  locals.emplace("mapRefs", borrowedPackInfo);

  primec::ir_lowerer::LocalInfo pointerPackInfo;
  pointerPackInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  pointerPackInfo.isArgsPack = true;
  pointerPackInfo.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Pointer;
  pointerPackInfo.pointerToMap = true;
  pointerPackInfo.mapKeyKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  pointerPackInfo.mapValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  locals.emplace("mapPtrs", pointerPackInfo);

  auto resolveExprPath = [](const primec::Expr &expr) { return expr.name; };
  auto isNever = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; };

  std::string error;
  const primec::Definition *resolved = primec::ir_lowerer::resolveMethodCallDefinitionFromExpr(
      makeMethodCall("mapRefs", "contains", 11),
      locals,
      isNever,
      isNever,
      isNever,
      {},
      {},
      {},
      resolveExprPath,
      {},
      defMap,
      error);
  CHECK(resolved == &containsDef);
  CHECK(error.empty());

  error.clear();
  resolved = primec::ir_lowerer::resolveMethodCallDefinitionFromExpr(
      makeMethodCall("mapPtrs", "at_unsafe", 11),
      locals,
      isNever,
      isNever,
      isNever,
      {},
      {},
      {},
      resolveExprPath,
      {},
      defMap,
      error);
  CHECK(resolved == &atUnsafeDef);
  CHECK(error.empty());
}

TEST_CASE("ir lowerer setup inference helper resolves wrapper-returned slash-method map access kinds") {
  using Resolution = primec::ir_lowerer::ArrayMapAccessElementKindResolution;

  primec::Expr wrapMapCall;
  wrapMapCall.kind = primec::Expr::Kind::Call;
  wrapMapCall.name = "wrapMap";

  primec::Expr keyExpr;
  keyExpr.kind = primec::Expr::Kind::Literal;
  keyExpr.intWidth = 32;
  keyExpr.literalValue = 1;

  primec::Expr accessExpr;
  accessExpr.kind = primec::Expr::Kind::Call;
  accessExpr.isMethodCall = true;
  accessExpr.name = "/std/collections/map/at";
  accessExpr.args = {wrapMapCall, keyExpr};

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  auto resolveWrapMapKind = [](const primec::Expr &candidate,
                               const primec::ir_lowerer::LocalMap &,
                               primec::ir_lowerer::LocalInfo::ValueKind &candidateKindOut) {
    if (candidate.kind != primec::Expr::Kind::Call || candidate.name != "wrapMap") {
      return false;
    }
    candidateKindOut = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
    return true;
  };

  CHECK(primec::ir_lowerer::resolveArrayMapAccessElementKind(
            accessExpr,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            kindOut,
            resolveWrapMapKind) == Resolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Int32);

  accessExpr.name = "/std/collections/map/at_unsafe";
  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(primec::ir_lowerer::resolveArrayMapAccessElementKind(
            accessExpr,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            kindOut,
            resolveWrapMapKind) == Resolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
}

TEST_CASE("ir lowerer setup inference helper resolves wrapper-returned canonical map access int32 kinds") {
  using Resolution = primec::ir_lowerer::ArrayMapAccessElementKindResolution;

  primec::Expr wrapMapCall;
  wrapMapCall.kind = primec::Expr::Kind::Call;
  wrapMapCall.name = "wrapMap";

  primec::Expr keyExpr;
  keyExpr.kind = primec::Expr::Kind::Literal;
  keyExpr.intWidth = 32;
  keyExpr.literalValue = 1;

  primec::Expr accessExpr;
  accessExpr.kind = primec::Expr::Kind::Call;
  accessExpr.name = "/std/collections/map/at";
  accessExpr.args = {wrapMapCall, keyExpr};

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  auto resolveWrapMapKind = [](const primec::Expr &candidate,
                               const primec::ir_lowerer::LocalMap &,
                               primec::ir_lowerer::LocalInfo::ValueKind &candidateKindOut) {
    if (candidate.kind != primec::Expr::Kind::Call || candidate.name != "wrapMap") {
      return false;
    }
    candidateKindOut = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
    return true;
  };

  CHECK(primec::ir_lowerer::resolveArrayMapAccessElementKind(
            accessExpr,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            kindOut,
            resolveWrapMapKind) == Resolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Int32);

  accessExpr.name = "/std/collections/map/at_unsafe";
  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(primec::ir_lowerer::resolveArrayMapAccessElementKind(
            accessExpr,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            kindOut,
            resolveWrapMapKind) == Resolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
}

TEST_SUITE_END();
