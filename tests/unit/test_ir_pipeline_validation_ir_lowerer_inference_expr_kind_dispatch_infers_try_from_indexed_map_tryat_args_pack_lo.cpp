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
                             bool referenceToMap,
                             bool pointerToMap) {
    primec::ir_lowerer::LocalInfo info;
    info.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
    info.isArgsPack = true;
    info.argsPackElementKind = elementKind;
    info.referenceToMap = referenceToMap;
    info.pointerToMap = pointerToMap;
    info.mapKeyKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
    info.mapValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
    info.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
    return info;
  };

  primec::ir_lowerer::LocalMap locals;
  locals.emplace("maps", makeArgsPackInfo(primec::ir_lowerer::LocalInfo::Kind::Map, false, false));
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

  std::string error;
  CHECK(primec::ir_lowerer::runLowerInferenceExprKindCallFallbackSetup(
      {
          .isArrayCountCall = [](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
            return expr.kind == primec::Expr::Kind::Call && expr.name == "count";
          },
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
