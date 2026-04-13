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
