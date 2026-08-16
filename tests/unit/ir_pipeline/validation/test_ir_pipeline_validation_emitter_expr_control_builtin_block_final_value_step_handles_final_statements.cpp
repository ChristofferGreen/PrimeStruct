#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("emitter expr control builtin-block final-value step handles final statements") {
  primec::Expr stmt;
  stmt.kind = primec::Expr::Kind::Call;
  stmt.name = "value";

  const auto nonFinal = primec::emitter::runEmitterExprControlBuiltinBlockFinalValueStep(
      stmt,
      false,
      [&](const primec::Expr &) { return false; },
      [&](const primec::Expr &) { return "unused"; });
  CHECK_FALSE(nonFinal.handled);
  CHECK(nonFinal.emittedStatement.empty());

  primec::Expr bindingStmt = stmt;
  bindingStmt.isBinding = true;
  const auto bindingFinal = primec::emitter::runEmitterExprControlBuiltinBlockFinalValueStep(
      bindingStmt,
      true,
      [&](const primec::Expr &) { return false; },
      [&](const primec::Expr &) { return "unused"; });
  CHECK(bindingFinal.handled);
  CHECK(bindingFinal.emittedStatement == "return 0; ");

  primec::Expr returnStmt = stmt;
  returnStmt.name = "return";
  returnStmt.args.push_back(primec::Expr{});
  const auto returnFinal = primec::emitter::runEmitterExprControlBuiltinBlockFinalValueStep(
      returnStmt,
      true,
      [&](const primec::Expr &) { return true; },
      [&](const primec::Expr &) { return "emit_value"; });
  CHECK(returnFinal.handled);
  CHECK(returnFinal.emittedStatement == "return emit_value; ");

  returnStmt.args.push_back(primec::Expr{});
  const auto invalidReturnArity = primec::emitter::runEmitterExprControlBuiltinBlockFinalValueStep(
      returnStmt,
      true,
      [&](const primec::Expr &) { return true; },
      [&](const primec::Expr &) { return "emit_value"; });
  CHECK(invalidReturnArity.handled);
  CHECK(invalidReturnArity.emittedStatement == "return 0; ");

  const auto noEmit = primec::emitter::runEmitterExprControlBuiltinBlockFinalValueStep(
      stmt,
      true,
      [&](const primec::Expr &) { return false; },
      {});
  CHECK(noEmit.handled);
  CHECK(noEmit.emittedStatement == "return 0; ");
}

TEST_CASE("emitter expr control builtin-block binding-prelude step prepares binding state") {
  primec::Expr nonBinding;
  nonBinding.kind = primec::Expr::Kind::Call;
  nonBinding.name = "value";
  std::unordered_map<std::string, primec::Emitter::BindingInfo> blockTypes;
  const auto nonBindingResult = primec::emitter::runEmitterExprControlBuiltinBlockBindingPreludeStep(
      nonBinding,
      blockTypes,
      {},
      false,
      [&](const primec::Expr &) { return primec::Emitter::BindingInfo{}; },
      [&](const primec::Expr &) { return false; },
      [&](const primec::Expr &,
          const std::unordered_map<std::string, primec::Emitter::BindingInfo> &,
          const std::unordered_map<std::string, primec::Emitter::ReturnKind> &,
          bool) { return primec::Emitter::ReturnKind::Unknown; },
      [&](primec::Emitter::ReturnKind) { return std::string("unused"); });
  CHECK_FALSE(nonBindingResult.handled);
  CHECK(blockTypes.empty());

  primec::Expr bindingExpr = nonBinding;
  bindingExpr.isBinding = true;
  bindingExpr.name = "item";
  primec::Expr argExpr;
  argExpr.kind = primec::Expr::Kind::Literal;
  bindingExpr.args = {argExpr};

  int inferCalls = 0;
  const auto inferredResult = primec::emitter::runEmitterExprControlBuiltinBlockBindingPreludeStep(
      bindingExpr,
      blockTypes,
      {},
      true,
      [&](const primec::Expr &) {
        primec::Emitter::BindingInfo info;
        info.typeName = "Original";
        info.typeTemplateArg = "T";
        return info;
      },
      [&](const primec::Expr &) { return false; },
      [&](const primec::Expr &,
          const std::unordered_map<std::string, primec::Emitter::BindingInfo> &,
          const std::unordered_map<std::string, primec::Emitter::ReturnKind> &,
          bool) {
        ++inferCalls;
        return primec::Emitter::ReturnKind::Int64;
      },
      [&](primec::Emitter::ReturnKind kind) {
        CHECK(kind == primec::Emitter::ReturnKind::Int64);
        return std::string("InferredType");
      });
  CHECK(inferredResult.handled);
  CHECK_FALSE(inferredResult.hasExplicitType);
  CHECK_FALSE(inferredResult.lambdaInit);
  CHECK_FALSE(inferredResult.useAuto);
  CHECK(inferCalls == 1);
  CHECK(inferredResult.binding.typeName == "InferredType");
  CHECK(inferredResult.binding.typeTemplateArg.empty());
  REQUIRE(blockTypes.count("item") == 1);
  CHECK(blockTypes.at("item").typeName == "InferredType");

  blockTypes.clear();
  bindingExpr.args.front().isLambda = true;
  inferCalls = 0;
  const auto lambdaResult = primec::emitter::runEmitterExprControlBuiltinBlockBindingPreludeStep(
      bindingExpr,
      blockTypes,
      {},
      true,
      [&](const primec::Expr &) {
        primec::Emitter::BindingInfo info;
        info.typeName = "LambdaType";
        return info;
      },
      [&](const primec::Expr &) { return false; },
      [&](const primec::Expr &,
          const std::unordered_map<std::string, primec::Emitter::BindingInfo> &,
          const std::unordered_map<std::string, primec::Emitter::ReturnKind> &,
          bool) {
        ++inferCalls;
        return primec::Emitter::ReturnKind::Unknown;
      },
      [&](primec::Emitter::ReturnKind) { return std::string("unused"); });
  CHECK(lambdaResult.handled);
  CHECK_FALSE(lambdaResult.hasExplicitType);
  CHECK(lambdaResult.lambdaInit);
  CHECK(lambdaResult.useAuto);
  CHECK(inferCalls == 0);
  REQUIRE(blockTypes.count("item") == 1);
  CHECK(blockTypes.at("item").typeName == "LambdaType");

  const auto missingCallbacks = primec::emitter::runEmitterExprControlBuiltinBlockBindingPreludeStep(
      bindingExpr,
      blockTypes,
      {},
      true,
      {},
      {},
      {},
      {});
  CHECK_FALSE(missingCallbacks.handled);
}

TEST_CASE("emitter expr control builtin-block binding-auto step emits auto bindings") {
  primec::Expr stmt;
  stmt.kind = primec::Expr::Kind::Call;
  stmt.name = "item";

  primec::Emitter::BindingInfo bindingInfo;
  bindingInfo.isMutable = false;

  const auto nonAuto = primec::emitter::runEmitterExprControlBuiltinBlockBindingAutoStep(
      stmt,
      bindingInfo,
      false,
      [&](const primec::Expr &) { return "unused"; });
  CHECK_FALSE(nonAuto.handled);
  CHECK(nonAuto.emittedStatement.empty());

  const auto constAuto = primec::emitter::runEmitterExprControlBuiltinBlockBindingAutoStep(
      stmt,
      bindingInfo,
      true,
      [&](const primec::Expr &) { return "unused"; });
  CHECK(constAuto.handled);
  CHECK(constAuto.emittedStatement == "const auto item; ");

  primec::Expr initExpr;
  initExpr.kind = primec::Expr::Kind::Literal;
  initExpr.literalValue = 7;
  stmt.args.push_back(initExpr);
  bindingInfo.isMutable = true;
  int emitCalls = 0;
  const auto mutableAuto = primec::emitter::runEmitterExprControlBuiltinBlockBindingAutoStep(
      stmt,
      bindingInfo,
      true,
      [&](const primec::Expr &candidate) {
        ++emitCalls;
        CHECK(candidate.kind == primec::Expr::Kind::Literal);
        CHECK(candidate.literalValue == 7);
        return std::string("emit_init");
      });
  CHECK(mutableAuto.handled);
  CHECK(emitCalls == 1);
  CHECK(mutableAuto.emittedStatement == "auto item = emit_init; ");

  const auto missingEmit = primec::emitter::runEmitterExprControlBuiltinBlockBindingAutoStep(
      stmt,
      bindingInfo,
      true,
      {});
  CHECK(missingEmit.handled);
  CHECK(missingEmit.emittedStatement == "auto item; ");
}

TEST_CASE("emitter expr control builtin-block binding-explicit step emits typed bindings") {
  primec::Expr stmt;
  stmt.kind = primec::Expr::Kind::Call;
  stmt.name = "item";

  primec::Emitter::BindingInfo bindingInfo;
  bindingInfo.typeName = "i32";
  bindingInfo.isMutable = false;

  const auto nonExplicit = primec::emitter::runEmitterExprControlBuiltinBlockBindingExplicitStep(
      stmt,
      bindingInfo,
      false,
      true,
      false,
      "",
      {},
      {},
      [&](const primec::Expr &) { return "unused"; });
  CHECK_FALSE(nonExplicit.handled);
  CHECK(nonExplicit.emittedStatement.empty());

  const auto constTyped = primec::emitter::runEmitterExprControlBuiltinBlockBindingExplicitStep(
      stmt,
      bindingInfo,
      true,
      true,
      false,
      "",
      {},
      {},
      [&](const primec::Expr &) { return "unused"; });
  CHECK(constTyped.handled);
  CHECK(constTyped.emittedStatement == "const int item; ");

  primec::Expr initExpr;
  initExpr.kind = primec::Expr::Kind::Literal;
  initExpr.literalValue = 7;
  stmt.args.push_back(initExpr);

  int emitCalls = 0;
  const auto mutableTyped = primec::emitter::runEmitterExprControlBuiltinBlockBindingExplicitStep(
      stmt,
      bindingInfo,
      true,
      false,
      false,
      "",
      {},
      {},
      [&](const primec::Expr &candidate) {
        ++emitCalls;
        CHECK(candidate.kind == primec::Expr::Kind::Literal);
        CHECK(candidate.literalValue == 7);
        return std::string("emit_init");
      });
  CHECK(mutableTyped.handled);
  CHECK(emitCalls == 1);
  CHECK(mutableTyped.emittedStatement == "int item = emit_init; ");

  const auto constRefTyped = primec::emitter::runEmitterExprControlBuiltinBlockBindingExplicitStep(
      stmt,
      bindingInfo,
      true,
      true,
      true,
      "",
      {},
      {},
      [&](const primec::Expr &) { return "emit_ref"; });
  CHECK(constRefTyped.handled);
  CHECK(constRefTyped.emittedStatement == "const int & item = emit_ref; ");

  primec::Emitter::BindingInfo referenceBinding;
  referenceBinding.typeName = "Reference";
  referenceBinding.typeTemplateArg = "i32";

  const auto referenceTyped = primec::emitter::runEmitterExprControlBuiltinBlockBindingExplicitStep(
      stmt,
      referenceBinding,
      true,
      false,
      false,
      "",
      {},
      {},
      [&](const primec::Expr &) { return "emit_ref_target"; });
  CHECK(referenceTyped.handled);
  CHECK(referenceTyped.emittedStatement == "int & item = *(emit_ref_target); ");

  const auto missingEmit = primec::emitter::runEmitterExprControlBuiltinBlockBindingExplicitStep(
      stmt,
      referenceBinding,
      true,
      false,
      false,
      "",
      {},
      {},
      {});
  CHECK(missingEmit.handled);
  CHECK(missingEmit.emittedStatement == "int & item; ");
}

TEST_CASE("emitter expr control builtin-block binding-fallback step emits inferred bindings") {
  primec::Expr stmt;
  stmt.kind = primec::Expr::Kind::Call;
  stmt.name = "item";

  const auto explicitType = primec::emitter::runEmitterExprControlBuiltinBlockBindingFallbackStep(
      stmt,
      true,
      true,
      false,
      [&](const primec::Expr &) { return "unused"; });
  CHECK_FALSE(explicitType.handled);
  CHECK(explicitType.emittedStatement.empty());

  const auto constAuto = primec::emitter::runEmitterExprControlBuiltinBlockBindingFallbackStep(
      stmt,
      false,
      true,
      false,
      [&](const primec::Expr &) { return "unused"; });
  CHECK(constAuto.handled);
  CHECK(constAuto.emittedStatement == "const auto item; ");

  primec::Expr initExpr;
  initExpr.kind = primec::Expr::Kind::Literal;
  initExpr.literalValue = 7;
  stmt.args.push_back(initExpr);

  int emitCalls = 0;
  const auto mutableAuto = primec::emitter::runEmitterExprControlBuiltinBlockBindingFallbackStep(
      stmt,
      false,
      false,
      false,
      [&](const primec::Expr &candidate) {
        ++emitCalls;
        CHECK(candidate.kind == primec::Expr::Kind::Literal);
        CHECK(candidate.literalValue == 7);
        return std::string("emit_init");
      });
  CHECK(mutableAuto.handled);
  CHECK(emitCalls == 1);
  CHECK(mutableAuto.emittedStatement == "auto item = emit_init; ");

  const auto refAuto = primec::emitter::runEmitterExprControlBuiltinBlockBindingFallbackStep(
      stmt,
      false,
      true,
      true,
      [&](const primec::Expr &) { return "emit_ref"; });
  CHECK(refAuto.handled);
  CHECK(refAuto.emittedStatement == "const auto & item = emit_ref; ");

  const auto missingEmit = primec::emitter::runEmitterExprControlBuiltinBlockBindingFallbackStep(
      stmt,
      false,
      false,
      false,
      {});
  CHECK(missingEmit.handled);
  CHECK(missingEmit.emittedStatement == "auto item; ");
}

TEST_CASE("emitter expr control builtin-block binding-qualifiers step resolves const/reference flags") {
  primec::Emitter::BindingInfo mutableBinding;
  mutableBinding.isMutable = true;
  mutableBinding.isCopy = false;
  const auto mutableResult = primec::emitter::runEmitterExprControlBuiltinBlockBindingQualifiersStep(
      mutableBinding,
      true,
      [&](const primec::Emitter::BindingInfo &) { return true; });
  CHECK_FALSE(mutableResult.needsConst);
  CHECK_FALSE(mutableResult.useRef);

  primec::Emitter::BindingInfo copyBinding;
  copyBinding.isMutable = false;
  copyBinding.isCopy = true;
  const auto copyResult = primec::emitter::runEmitterExprControlBuiltinBlockBindingQualifiersStep(
      copyBinding,
      true,
      [&](const primec::Emitter::BindingInfo &) { return true; });
  CHECK(copyResult.needsConst);
  CHECK_FALSE(copyResult.useRef);

  primec::Emitter::BindingInfo constBinding;
  constBinding.isMutable = false;
  constBinding.isCopy = false;
  int referenceCandidateCalls = 0;
  const auto constResult = primec::emitter::runEmitterExprControlBuiltinBlockBindingQualifiersStep(
      constBinding,
      true,
      [&](const primec::Emitter::BindingInfo &candidate) {
        ++referenceCandidateCalls;
        CHECK_FALSE(candidate.isMutable);
        CHECK_FALSE(candidate.isCopy);
        return true;
      });
  CHECK(constResult.needsConst);
  CHECK(constResult.useRef);
  CHECK(referenceCandidateCalls == 1);

  const auto noInitializer = primec::emitter::runEmitterExprControlBuiltinBlockBindingQualifiersStep(
      constBinding,
      false,
      [&](const primec::Emitter::BindingInfo &) { return true; });
  CHECK(noInitializer.needsConst);
  CHECK_FALSE(noInitializer.useRef);

  const auto missingCallback = primec::emitter::runEmitterExprControlBuiltinBlockBindingQualifiersStep(
      constBinding,
      true,
      {});
  CHECK(missingCallback.needsConst);
  CHECK_FALSE(missingCallback.useRef);
}

TEST_CASE("emitter expr control builtin-block statement step emits void expression statements") {
  primec::Expr stmt;
  stmt.kind = primec::Expr::Kind::Call;
  stmt.name = "act";

  const auto missingEmit = primec::emitter::runEmitterExprControlBuiltinBlockStatementStep(
      stmt,
      {});
  CHECK_FALSE(missingEmit.handled);
  CHECK(missingEmit.emittedStatement.empty());

  int emitCalls = 0;
  const auto emitted = primec::emitter::runEmitterExprControlBuiltinBlockStatementStep(
      stmt,
      [&](const primec::Expr &candidate) {
        ++emitCalls;
        CHECK(candidate.kind == primec::Expr::Kind::Call);
        CHECK(candidate.name == "act");
        return std::string("emit_stmt");
      });
  CHECK(emitted.handled);
  CHECK(emitCalls == 1);
  CHECK(emitted.emittedStatement == "(void)emit_stmt; ");
}

TEST_CASE("emitter expr control body-wrapper step rewrites body-argument calls") {
  primec::Expr noBodyExpr;
  noBodyExpr.kind = primec::Expr::Kind::Call;
  noBodyExpr.name = "compute";
  CHECK_FALSE(primec::emitter::runEmitterExprControlBodyWrapperStep(
      noBodyExpr,
      {},
      [&](const primec::Expr &, const std::unordered_map<std::string, std::string> &) { return false; },
      [&](const primec::Expr &) { return "unused"; }).has_value());

  primec::Expr bodyExpr = noBodyExpr;
  bodyExpr.hasBodyArguments = true;
  bodyExpr.bodyArguments.push_back(primec::Expr{});

  bool emitCalled = false;
  CHECK_FALSE(primec::emitter::runEmitterExprControlBodyWrapperStep(
      bodyExpr,
      {},
      [&](const primec::Expr &, const std::unordered_map<std::string, std::string> &) { return true; },
      [&](const primec::Expr &) {
        emitCalled = true;
        return "unused";
      }).has_value());
  CHECK_FALSE(emitCalled);

  CHECK_FALSE(primec::emitter::runEmitterExprControlBodyWrapperStep(
      bodyExpr,
      {},
      {},
      [&](const primec::Expr &) { return "unused"; }).has_value());
  CHECK_FALSE(primec::emitter::runEmitterExprControlBodyWrapperStep(
      bodyExpr,
      {},
      [&](const primec::Expr &, const std::unordered_map<std::string, std::string> &) { return false; },
      {}).has_value());

  int emitCalls = 0;
  auto wrapped = primec::emitter::runEmitterExprControlBodyWrapperStep(
      bodyExpr,
      {},
      [&](const primec::Expr &, const std::unordered_map<std::string, std::string> &) { return false; },
      [&](const primec::Expr &candidate) {
        ++emitCalls;
        if (candidate.hasBodyArguments) {
          CHECK(candidate.name == "block");
          CHECK(candidate.args.empty());
          CHECK(candidate.templateArgs.empty());
          CHECK(candidate.argNames.empty());
          CHECK_FALSE(candidate.isMethodCall);
          CHECK_FALSE(candidate.isBinding);
          CHECK_FALSE(candidate.isLambda);
          CHECK(candidate.bodyArguments.size() == 1);
          return std::string("emit_block");
        }
        CHECK(candidate.name == "compute");
        CHECK(candidate.bodyArguments.empty());
        return std::string("emit_call");
      });
  REQUIRE(wrapped.has_value());
  CHECK(emitCalls == 2);
  CHECK(*wrapped == "([&]() { auto ps_call_value = emit_call; (void)emit_block; return ps_call_value; }())");
}

TEST_CASE("emitter expr control if-envelope step recognizes block envelopes") {
  primec::Expr notCall;
  notCall.kind = primec::Expr::Kind::Literal;
  CHECK_FALSE(primec::emitter::runEmitterExprControlIfBlockEnvelopeStep(notCall));

  primec::Expr bindingCall;
  bindingCall.kind = primec::Expr::Kind::Call;
  bindingCall.isBinding = true;
  bindingCall.hasBodyArguments = true;
  CHECK_FALSE(primec::emitter::runEmitterExprControlIfBlockEnvelopeStep(bindingCall));

  primec::Expr methodCall = bindingCall;
  methodCall.isBinding = false;
  methodCall.isMethodCall = true;
  CHECK_FALSE(primec::emitter::runEmitterExprControlIfBlockEnvelopeStep(methodCall));

  primec::Expr argsCall = bindingCall;
  argsCall.isBinding = false;
  argsCall.isMethodCall = false;
  argsCall.args.push_back(notCall);
  CHECK_FALSE(primec::emitter::runEmitterExprControlIfBlockEnvelopeStep(argsCall));

  primec::Expr namedArgsCall = bindingCall;
  namedArgsCall.isBinding = false;
  namedArgsCall.isMethodCall = false;
  namedArgsCall.argNames.push_back("value");
  CHECK_FALSE(primec::emitter::runEmitterExprControlIfBlockEnvelopeStep(namedArgsCall));

  primec::Expr noBodyCall = bindingCall;
  noBodyCall.isBinding = false;
  noBodyCall.isMethodCall = false;
  noBodyCall.hasBodyArguments = false;
  noBodyCall.bodyArguments.clear();
  CHECK_FALSE(primec::emitter::runEmitterExprControlIfBlockEnvelopeStep(noBodyCall));

  primec::Expr blockEnvelope = bindingCall;
  blockEnvelope.isBinding = false;
  blockEnvelope.isMethodCall = false;
  blockEnvelope.hasBodyArguments = true;
  CHECK(primec::emitter::runEmitterExprControlIfBlockEnvelopeStep(blockEnvelope));
}

TEST_CASE("emitter expr control if-block early-return step emits non-final returns") {
  primec::Expr stmt;
  stmt.kind = primec::Expr::Kind::Call;
  stmt.name = "return";

  const auto nonReturn = primec::emitter::runEmitterExprControlIfBlockEarlyReturnStep(
      stmt,
      false,
      [&](const primec::Expr &) { return false; },
      [&](const primec::Expr &) { return "unused"; });
  CHECK_FALSE(nonReturn.handled);
  CHECK(nonReturn.emittedStatement.empty());

  const auto lastReturn = primec::emitter::runEmitterExprControlIfBlockEarlyReturnStep(
      stmt,
      true,
      [&](const primec::Expr &) { return true; },
      [&](const primec::Expr &) { return "unused"; });
  CHECK_FALSE(lastReturn.handled);
  CHECK(lastReturn.emittedStatement.empty());

  const auto missingPredicate = primec::emitter::runEmitterExprControlIfBlockEarlyReturnStep(
      stmt,
      false,
      {},
      [&](const primec::Expr &) { return "unused"; });
  CHECK_FALSE(missingPredicate.handled);
  CHECK(missingPredicate.emittedStatement.empty());

  primec::Expr valueExpr;
  valueExpr.kind = primec::Expr::Kind::Literal;
  valueExpr.literalValue = 12;
  stmt.args = {valueExpr};

  int emitCalls = 0;
  const auto validReturn = primec::emitter::runEmitterExprControlIfBlockEarlyReturnStep(
      stmt,
      false,
      [&](const primec::Expr &candidate) {
        CHECK(candidate.name == "return");
        return true;
      },
      [&](const primec::Expr &candidate) {
        ++emitCalls;
        CHECK(candidate.kind == primec::Expr::Kind::Literal);
        CHECK(candidate.literalValue == 12);
        return std::string("emit_value");
      });
  CHECK(validReturn.handled);
  CHECK(emitCalls == 1);
  CHECK(validReturn.emittedStatement == "return emit_value; ");

  stmt.args.clear();
  const auto invalidArity = primec::emitter::runEmitterExprControlIfBlockEarlyReturnStep(
      stmt,
      false,
      [&](const primec::Expr &) { return true; },
      [&](const primec::Expr &) { return "unused"; });
  CHECK(invalidArity.handled);
  CHECK(invalidArity.emittedStatement == "return 0; ");

  stmt.args = {valueExpr};
  const auto missingEmit = primec::emitter::runEmitterExprControlIfBlockEarlyReturnStep(
      stmt,
      false,
      [&](const primec::Expr &) { return true; },
      {});
  CHECK(missingEmit.handled);
  CHECK(missingEmit.emittedStatement == "return 0; ");
}


TEST_SUITE_END();
