#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir lowerer inference call-return setup resolves canonical namespaced map access directly") {
  primec::Definition receiverAtDef;
  receiverAtDef.fullPath = "/map/at";
  primec::Definition canonicalAtDef;
  canonicalAtDef.fullPath = "/std/collections/map/at";
  std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/std/collections/map/at", &canonicalAtDef},
  };

  bool resolveReceiverHelper = true;
  int resolveMethodCalls = 0;
  primec::ir_lowerer::LowerInferenceSetupBootstrapState state;
  state.getReturnInfo = [](const std::string &path, primec::ir_lowerer::ReturnInfo &out) {
    out.returnsVoid = false;
    out.returnsArray = false;
    if (path == "/map/at") {
      out.kind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
      return true;
    }
    if (path == "/std/collections/map/at") {
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
  callExpr.name = "/std/collections/map/at";
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

TEST_CASE("ir lowerer inference call-return setup resolves canonical namespaced map at unsafe directly") {
  primec::Definition receiverAtUnsafeDef;
  receiverAtUnsafeDef.fullPath = "/map/at_unsafe";
  primec::Definition canonicalAtUnsafeDef;
  canonicalAtUnsafeDef.fullPath = "/std/collections/map/at_unsafe";
  std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/std/collections/map/at_unsafe", &canonicalAtUnsafeDef},
  };

  bool resolveReceiverHelper = true;
  int resolveMethodCalls = 0;
  primec::ir_lowerer::LowerInferenceSetupBootstrapState state;
  state.getReturnInfo = [](const std::string &path, primec::ir_lowerer::ReturnInfo &out) {
    out.returnsVoid = false;
    out.returnsArray = false;
    if (path == "/map/at_unsafe") {
      out.kind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
      return true;
    }
    if (path == "/std/collections/map/at_unsafe") {
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
  callExpr.name = "/std/collections/map/at_unsafe";
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

TEST_CASE("ir lowerer inference call-return setup resolves namespaced push definition directly") {
  primec::Definition receiverPushDef;
  receiverPushDef.fullPath = "/vector/push";
  primec::Definition canonicalPushDef;
  canonicalPushDef.fullPath = "/std/collections/vector/push";
  std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/std/collections/vector/push", &canonicalPushDef},
  };

  bool resolveReceiverHelper = true;
  int resolveMethodCalls = 0;
  primec::ir_lowerer::LowerInferenceSetupBootstrapState state;
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
      [&](const primec::Expr &methodExpr, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
    ++resolveMethodCalls;
    if (!resolveReceiverHelper || !methodExpr.isMethodCall || methodExpr.name != "push" || methodExpr.args.size() != 2) {
      return nullptr;
    }
    if (methodExpr.args.front().kind == primec::Expr::Kind::Name && methodExpr.args.front().name == "values") {
      return &receiverPushDef;
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
  primec::Expr valueExpr;
  valueExpr.kind = primec::Expr::Kind::Literal;
  valueExpr.intWidth = 32;
  valueExpr.literalValue = 7;
  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "/std/collections/vector/push";
  callExpr.args = {receiverExpr, valueExpr};

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

TEST_CASE("ir lowerer inference call-return setup resolves namespaced pop definition directly") {
  primec::Definition receiverPopDef;
  receiverPopDef.fullPath = "/vector/pop";
  primec::Definition canonicalPopDef;
  canonicalPopDef.fullPath = "/std/collections/vector/pop";
  std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/std/collections/vector/pop", &canonicalPopDef},
  };

  bool resolveReceiverHelper = true;
  int resolveMethodCalls = 0;
  primec::ir_lowerer::LowerInferenceSetupBootstrapState state;
  state.getReturnInfo = [](const std::string &path, primec::ir_lowerer::ReturnInfo &out) {
    out.returnsVoid = false;
    out.returnsArray = false;
    if (path == "/vector/pop") {
      out.kind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
      return true;
    }
    if (path == "/std/collections/vector/pop") {
      out.kind = primec::ir_lowerer::LocalInfo::ValueKind::UInt64;
      return true;
    }
    return false;
  };
  state.resolveMethodCallDefinition =
      [&](const primec::Expr &methodExpr, const primec::ir_lowerer::LocalMap &) -> const primec::Definition * {
    ++resolveMethodCalls;
    if (!resolveReceiverHelper || !methodExpr.isMethodCall || methodExpr.name != "pop" || methodExpr.args.size() != 1) {
      return nullptr;
    }
    if (methodExpr.args.front().kind == primec::Expr::Kind::Name && methodExpr.args.front().name == "values") {
      return &receiverPopDef;
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
  callExpr.name = "/std/collections/vector/pop";
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

TEST_CASE("ir lowerer inference expr-kind call-return setup validates dependencies") {
  std::unordered_map<std::string, const primec::Definition *> defMap;
  primec::ir_lowerer::LowerInferenceSetupBootstrapState state;
  state.resolveMethodCallDefinition = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
    return static_cast<const primec::Definition *>(nullptr);
  };

  std::string error;
  CHECK_FALSE(primec::ir_lowerer::runLowerInferenceExprKindCallReturnSetup(
      {
          .defMap = &defMap,
          .resolveExprPath = {},
          .isArrayCountCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
          .isStringCountCall = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      },
      state,
      error));
  CHECK(error == "native backend missing inference expr-kind call-return setup dependency: resolveExprPath");
}

TEST_CASE("ir lowerer inference return-info setup handles declared return transforms") {
  primec::Definition definition;
  definition.fullPath = "/callee";
  primec::Transform returnTransform;
  returnTransform.name = "return";
  returnTransform.templateArgs = {"i64"};
  definition.transforms.push_back(returnTransform);

  primec::ir_lowerer::ReturnInfo info;
  std::string error;
  CHECK(primec::ir_lowerer::runLowerInferenceReturnInfoSetup(
      {
          .resolveStructTypeName = [](const std::string &, const std::string &, std::string &) { return false; },
          .resolveStructArrayInfoFromPath =
              [](const std::string &, primec::ir_lowerer::StructArrayTypeInfo &) { return false; },
          .isBindingMutable = [](const primec::Expr &) { return false; },
          .bindingKind = [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::Kind::Value; },
          .hasExplicitBindingTypeTransform = [](const primec::Expr &) { return true; },
          .bindingValueKind = [](const primec::Expr &, primec::ir_lowerer::LocalInfo::Kind) {
            return primec::ir_lowerer::LocalInfo::ValueKind::Int32;
          },
          .inferExprKind = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
            return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
          },
          .isFileErrorBinding = [](const primec::Expr &) { return false; },
          .setReferenceArrayInfo = [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
          .applyStructArrayInfo = [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
          .applyStructValueInfo = [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
          .inferStructExprPath = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
            return std::string{};
          },
          .isStringBinding = [](const primec::Expr &) { return false; },
          .inferArrayElementKind = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
            return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
          },
          .lowerMatchToIf = [](const primec::Expr &, primec::Expr &, std::string &) { return true; },
      },
      definition,
      info,
      error));
  CHECK(error.empty());
  CHECK_FALSE(info.returnsVoid);
  CHECK_FALSE(info.returnsArray);
  CHECK(info.kind == primec::ir_lowerer::LocalInfo::ValueKind::Int64);
}

TEST_CASE("ir lowerer inference return-info setup treats pointerish returns as address-like i64") {
  auto checkReturnType = [](const std::string &returnType) {
    primec::Definition definition;
    definition.fullPath = "/pointerish";
    primec::Transform returnTransform;
    returnTransform.name = "return";
    returnTransform.templateArgs = {returnType};
    definition.transforms.push_back(returnTransform);

    primec::ir_lowerer::ReturnInfo info;
    std::string error;
    CHECK(primec::ir_lowerer::runLowerInferenceReturnInfoSetup(
        {
            .resolveStructTypeName = [](const std::string &, const std::string &, std::string &) { return false; },
            .resolveStructArrayInfoFromPath =
                [](const std::string &, primec::ir_lowerer::StructArrayTypeInfo &) { return false; },
            .isBindingMutable = [](const primec::Expr &) { return false; },
            .bindingKind = [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::Kind::Value; },
            .hasExplicitBindingTypeTransform = [](const primec::Expr &) { return true; },
            .bindingValueKind = [](const primec::Expr &, primec::ir_lowerer::LocalInfo::Kind) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Int32;
            },
            .inferExprKind = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            .isFileErrorBinding = [](const primec::Expr &) { return false; },
            .setReferenceArrayInfo = [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
            .applyStructArrayInfo = [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
            .applyStructValueInfo = [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
            .inferStructExprPath = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return std::string{};
            },
            .isStringBinding = [](const primec::Expr &) { return false; },
            .inferArrayElementKind = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            .lowerMatchToIf = [](const primec::Expr &, primec::Expr &, std::string &) { return true; },
        },
        definition,
        info,
        error));
    CHECK(error.empty());
    CHECK_FALSE(info.returnsVoid);
    CHECK_FALSE(info.returnsArray);
    CHECK(info.kind == primec::ir_lowerer::LocalInfo::ValueKind::Int64);
  };

  checkReturnType("Pointer<i32>");
  checkReturnType("Reference<i32>");
}

TEST_CASE("ir lowerer inference return-info setup infers auto returns") {
  primec::Definition definition;
  definition.fullPath = "/auto";
  primec::Transform returnTransform;
  returnTransform.name = "return";
  returnTransform.templateArgs = {"auto"};
  definition.transforms.push_back(returnTransform);

  primec::Expr returnExpr;
  returnExpr.kind = primec::Expr::Kind::Literal;
  returnExpr.intWidth = 64;
  definition.returnExpr = returnExpr;

  primec::ir_lowerer::ReturnInfo info;
  std::string error;
  CHECK(primec::ir_lowerer::runLowerInferenceReturnInfoSetup(
      {
          .resolveStructTypeName = [](const std::string &, const std::string &, std::string &) { return false; },
          .resolveStructArrayInfoFromPath =
              [](const std::string &, primec::ir_lowerer::StructArrayTypeInfo &) { return false; },
          .isBindingMutable = [](const primec::Expr &) { return false; },
          .bindingKind = [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::Kind::Value; },
          .hasExplicitBindingTypeTransform = [](const primec::Expr &) { return true; },
          .bindingValueKind = [](const primec::Expr &, primec::ir_lowerer::LocalInfo::Kind) {
            return primec::ir_lowerer::LocalInfo::ValueKind::Int32;
          },
          .inferExprKind = [](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
            if (expr.kind == primec::Expr::Kind::Literal && expr.intWidth == 64) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Int64;
            }
            return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
          },
          .isFileErrorBinding = [](const primec::Expr &) { return false; },
          .setReferenceArrayInfo = [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
          .applyStructArrayInfo = [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
          .applyStructValueInfo = [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
          .inferStructExprPath = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
            return std::string{};
          },
          .isStringBinding = [](const primec::Expr &) { return false; },
          .inferArrayElementKind = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
            return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
          },
          .lowerMatchToIf = [](const primec::Expr &, primec::Expr &, std::string &) { return true; },
      },
      definition,
      info,
      error));
  CHECK(error.empty());
  CHECK_FALSE(info.returnsVoid);
  CHECK_FALSE(info.returnsArray);
  CHECK(info.kind == primec::ir_lowerer::LocalInfo::ValueKind::Int64);
}

TEST_CASE("ir lowerer inference return-info setup infers implicit auto map returns") {
  primec::Definition definition;
  definition.fullPath = "/auto_map";
  primec::Transform returnTransform;
  returnTransform.name = "return";
  returnTransform.templateArgs = {"auto"};
  definition.transforms.push_back(returnTransform);

  primec::Expr leftKeyExpr;
  leftKeyExpr.kind = primec::Expr::Kind::StringLiteral;
  leftKeyExpr.stringValue = "left";

  primec::Expr leftValueExpr;
  leftValueExpr.kind = primec::Expr::Kind::Literal;
  leftValueExpr.intWidth = 32;
  leftValueExpr.literalValue = 4;

  primec::Expr rightKeyExpr;
  rightKeyExpr.kind = primec::Expr::Kind::StringLiteral;
  rightKeyExpr.stringValue = "other";

  primec::Expr rightValueExpr;
  rightValueExpr.kind = primec::Expr::Kind::Literal;
  rightValueExpr.intWidth = 32;
  rightValueExpr.literalValue = 2;

  primec::Expr mapPairCall;
  mapPairCall.kind = primec::Expr::Kind::Call;
  mapPairCall.name = "/std/collections/mapPair";
  mapPairCall.args = {leftKeyExpr, leftValueExpr, rightKeyExpr, rightValueExpr};
  mapPairCall.argNames = {std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  definition.statements.push_back(mapPairCall);

  primec::ir_lowerer::ReturnInfo info;
  std::string error;
  CHECK(primec::ir_lowerer::runLowerInferenceReturnInfoSetup(
      {
          .resolveStructTypeName = [](const std::string &, const std::string &, std::string &) { return false; },
          .resolveStructArrayInfoFromPath =
              [](const std::string &, primec::ir_lowerer::StructArrayTypeInfo &) { return false; },
          .isBindingMutable = [](const primec::Expr &) { return false; },
          .bindingKind = [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::Kind::Value; },
          .hasExplicitBindingTypeTransform = [](const primec::Expr &) { return true; },
          .bindingValueKind = [](const primec::Expr &, primec::ir_lowerer::LocalInfo::Kind) {
            return primec::ir_lowerer::LocalInfo::ValueKind::Int32;
          },
          .inferExprKind = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
            return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
          },
          .isFileErrorBinding = [](const primec::Expr &) { return false; },
          .setReferenceArrayInfo = [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
          .applyStructArrayInfo = [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
          .applyStructValueInfo = [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
          .inferStructExprPath = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
            return std::string{};
          },
          .isStringBinding = [](const primec::Expr &) { return false; },
          .inferArrayElementKind = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
            return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
          },
          .lowerMatchToIf = [](const primec::Expr &, primec::Expr &, std::string &) { return true; },
      },
      definition,
      info,
      error));
  CHECK(error.empty());
  CHECK_FALSE(info.returnsVoid);
  CHECK(info.returnsArray);
  CHECK(info.kind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
}

TEST_CASE("ir lowerer inference return-info setup validates dependencies") {
  primec::Definition definition;
  definition.fullPath = "/missing";
  primec::ir_lowerer::ReturnInfo info;
  std::string error;
  CHECK_FALSE(primec::ir_lowerer::runLowerInferenceReturnInfoSetup(
      {
          .resolveStructTypeName = {},
      },
      definition,
      info,
      error));
  CHECK(error == "native backend missing inference return-info setup dependency: resolveStructTypeName");
}

TEST_CASE("ir lowerer inference get-return-info step resolves and caches returns") {
  primec::Definition definition;
  definition.fullPath = "/callee";
  primec::Transform returnTransform;
  returnTransform.name = "return";
  returnTransform.templateArgs = {"i64"};
  definition.transforms.push_back(returnTransform);

  std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/callee", &definition},
  };
  std::unordered_map<std::string, primec::ir_lowerer::ReturnInfo> returnInfoCache;
  std::unordered_set<std::string> returnInferenceStack;
  const primec::ir_lowerer::LowerInferenceReturnInfoSetupInput returnInfoSetupInput = {
      .resolveStructTypeName = [](const std::string &, const std::string &, std::string &) { return false; },
      .resolveStructArrayInfoFromPath = [](const std::string &, primec::ir_lowerer::StructArrayTypeInfo &) { return false; },
      .isBindingMutable = [](const primec::Expr &) { return false; },
      .bindingKind = [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::Kind::Value; },
      .hasExplicitBindingTypeTransform = [](const primec::Expr &) { return true; },
      .bindingValueKind = [](const primec::Expr &, primec::ir_lowerer::LocalInfo::Kind) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Int32;
      },
      .inferExprKind = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      .isFileErrorBinding = [](const primec::Expr &) { return false; },
      .setReferenceArrayInfo = [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      .applyStructArrayInfo = [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      .applyStructValueInfo = [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      .inferStructExprPath = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string{}; },
      .isStringBinding = [](const primec::Expr &) { return false; },
      .inferArrayElementKind = [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      .lowerMatchToIf = [](const primec::Expr &, primec::Expr &, std::string &) { return true; },
  };

  const primec::ir_lowerer::LowerInferenceGetReturnInfoStepInput input = {
      .defMap = &defMap,
      .returnInfoCache = &returnInfoCache,
      .returnInferenceStack = &returnInferenceStack,
      .returnInfoSetupInput = &returnInfoSetupInput,
  };

  primec::ir_lowerer::ReturnInfo outInfo;
  std::string error;
  CHECK(primec::ir_lowerer::runLowerInferenceGetReturnInfoStep(input, "/callee", outInfo, error));
  CHECK(error.empty());
  CHECK(outInfo.kind == primec::ir_lowerer::LocalInfo::ValueKind::Int64);
  CHECK(returnInfoCache.count("/callee") == 1);
  CHECK(returnInferenceStack.empty());

  defMap.clear();
  outInfo.kind = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(primec::ir_lowerer::runLowerInferenceGetReturnInfoStep(input, "/callee", outInfo, error));
  CHECK(error.empty());
  CHECK(outInfo.kind == primec::ir_lowerer::LocalInfo::ValueKind::Int64);
}


TEST_SUITE_END();
