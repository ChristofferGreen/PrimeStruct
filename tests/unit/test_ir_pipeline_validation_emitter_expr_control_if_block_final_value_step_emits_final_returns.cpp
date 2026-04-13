#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("emitter expr control if-block final-value step emits final returns") {
  primec::Expr stmt;
  stmt.kind = primec::Expr::Kind::Call;
  stmt.name = "value";

  const auto nonFinal = primec::emitter::runEmitterExprControlIfBlockFinalValueStep(
      stmt,
      false,
      [&](const primec::Expr &) { return true; },
      [&](const primec::Expr &) { return "unused"; });
  CHECK_FALSE(nonFinal.handled);
  CHECK(nonFinal.emittedStatement.empty());

  primec::Expr bindingStmt = stmt;
  bindingStmt.isBinding = true;
  const auto bindingFinal = primec::emitter::runEmitterExprControlIfBlockFinalValueStep(
      bindingStmt,
      true,
      [&](const primec::Expr &) { return false; },
      [&](const primec::Expr &) { return "unused"; });
  CHECK(bindingFinal.handled);
  CHECK(bindingFinal.emittedStatement == "return 0; ");

  primec::Expr returnStmt;
  returnStmt.kind = primec::Expr::Kind::Call;
  returnStmt.name = "return";
  primec::Expr returnValue;
  returnValue.kind = primec::Expr::Kind::Literal;
  returnValue.literalValue = 3;
  returnStmt.args = {returnValue};

  int emitCalls = 0;
  const auto validReturn = primec::emitter::runEmitterExprControlIfBlockFinalValueStep(
      returnStmt,
      true,
      [&](const primec::Expr &candidate) {
        CHECK(candidate.name == "return");
        return true;
      },
      [&](const primec::Expr &candidate) {
        ++emitCalls;
        CHECK(candidate.kind == primec::Expr::Kind::Literal);
        CHECK(candidate.literalValue == 3);
        return std::string("emit_final");
      });
  CHECK(validReturn.handled);
  CHECK(emitCalls == 1);
  CHECK(validReturn.emittedStatement == "return emit_final; ");

  returnStmt.args.clear();
  const auto invalidArity = primec::emitter::runEmitterExprControlIfBlockFinalValueStep(
      returnStmt,
      true,
      [&](const primec::Expr &) { return true; },
      [&](const primec::Expr &) { return "unused"; });
  CHECK(invalidArity.handled);
  CHECK(invalidArity.emittedStatement == "return 0; ");

  const auto missingEmit = primec::emitter::runEmitterExprControlIfBlockFinalValueStep(
      stmt,
      true,
      [&](const primec::Expr &) { return false; },
      {});
  CHECK(missingEmit.handled);
  CHECK(missingEmit.emittedStatement == "return 0; ");
}

TEST_CASE("emitter expr control if-block binding-prelude step resolves binding metadata") {
  primec::Expr nonBinding;
  nonBinding.kind = primec::Expr::Kind::Call;
  nonBinding.name = "value";

  std::unordered_map<std::string, primec::Emitter::BindingInfo> branchTypes;
  const std::unordered_map<std::string, primec::Emitter::ReturnKind> returnKinds;
  const auto nonBindingResult = primec::emitter::runEmitterExprControlIfBlockBindingPreludeStep(
      nonBinding,
      branchTypes,
      returnKinds,
      false,
      [&](const primec::Expr &) { return primec::Emitter::BindingInfo{}; },
      [&](const primec::Expr &) { return false; },
      {},
      {});
  CHECK_FALSE(nonBindingResult.handled);
  CHECK(branchTypes.empty());

  primec::Expr inferredStmt;
  inferredStmt.kind = primec::Expr::Kind::Call;
  inferredStmt.isBinding = true;
  inferredStmt.name = "item";
  primec::Expr initExpr;
  initExpr.kind = primec::Expr::Kind::Literal;
  initExpr.literalValue = 7;
  inferredStmt.args = {initExpr};

  primec::Emitter::BindingInfo inferredBinding;
  inferredBinding.typeName = "auto";
  inferredBinding.isMutable = false;
  int inferCalls = 0;
  int typeNameCalls = 0;
  auto inferredResult = primec::emitter::runEmitterExprControlIfBlockBindingPreludeStep(
      inferredStmt,
      branchTypes,
      returnKinds,
      true,
      [&](const primec::Expr &candidate) {
        CHECK(candidate.name == "item");
        return inferredBinding;
      },
      [&](const primec::Expr &) { return false; },
      [&](const primec::Expr &candidate,
          const std::unordered_map<std::string, primec::Emitter::BindingInfo> &candidateBranchTypes,
          const std::unordered_map<std::string, primec::Emitter::ReturnKind> &candidateReturnKinds,
          bool candidateAllowMathBare) {
        ++inferCalls;
        CHECK(candidate.kind == primec::Expr::Kind::Literal);
        CHECK(candidate.literalValue == 7);
        CHECK(candidateBranchTypes.empty());
        CHECK(candidateReturnKinds.empty());
        CHECK(candidateAllowMathBare);
        return primec::Emitter::ReturnKind::Int;
      },
      [&](primec::Emitter::ReturnKind kind) {
        ++typeNameCalls;
        CHECK(kind == primec::Emitter::ReturnKind::Int);
        return std::string("i32");
      });
  CHECK(inferredResult.handled);
  CHECK_FALSE(inferredResult.hasExplicitType);
  CHECK_FALSE(inferredResult.lambdaInit);
  CHECK_FALSE(inferredResult.useAuto);
  CHECK(inferredResult.binding.typeName == "i32");
  CHECK(inferCalls == 1);
  CHECK(typeNameCalls == 1);
  REQUIRE(branchTypes.count("item") == 1);
  CHECK(branchTypes.at("item").typeName == "i32");

  primec::Expr lambdaExpr;
  lambdaExpr.kind = primec::Expr::Kind::Call;
  lambdaExpr.isLambda = true;
  primec::Expr lambdaStmt = inferredStmt;
  lambdaStmt.args = {lambdaExpr};
  branchTypes.clear();

  const auto lambdaResult = primec::emitter::runEmitterExprControlIfBlockBindingPreludeStep(
      lambdaStmt,
      branchTypes,
      returnKinds,
      false,
      [&](const primec::Expr &) { return inferredBinding; },
      [&](const primec::Expr &) { return false; },
      [&](const primec::Expr &, const auto &, const auto &, bool) {
        CHECK_FALSE(true);
        return primec::Emitter::ReturnKind::Unknown;
      },
      [&](primec::Emitter::ReturnKind) {
        CHECK_FALSE(true);
        return std::string{};
      });
  CHECK(lambdaResult.handled);
  CHECK_FALSE(lambdaResult.hasExplicitType);
  CHECK(lambdaResult.lambdaInit);
  CHECK(lambdaResult.useAuto);
  REQUIRE(branchTypes.count("item") == 1);
  CHECK(branchTypes.at("item").typeName == "auto");

  const auto missingCallbacks = primec::emitter::runEmitterExprControlIfBlockBindingPreludeStep(
      inferredStmt,
      branchTypes,
      returnKinds,
      false,
      {},
      {},
      {},
      {});
  CHECK_FALSE(missingCallbacks.handled);
}

TEST_CASE("emitter expr control if-block binding-auto step emits auto bindings") {
  primec::Expr stmt;
  stmt.kind = primec::Expr::Kind::Call;
  stmt.isBinding = true;
  stmt.name = "item";

  primec::Emitter::BindingInfo binding;
  binding.isMutable = false;
  binding.typeName = "auto";

  const auto nonAuto = primec::emitter::runEmitterExprControlIfBlockBindingAutoStep(
      stmt,
      binding,
      false,
      [&](const primec::Expr &) { return "unused"; });
  CHECK_FALSE(nonAuto.handled);
  CHECK(nonAuto.emittedStatement.empty());

  primec::Expr initExpr;
  initExpr.kind = primec::Expr::Kind::Literal;
  initExpr.literalValue = 9;
  stmt.args = {initExpr};

  int emitCalls = 0;
  const auto constAuto = primec::emitter::runEmitterExprControlIfBlockBindingAutoStep(
      stmt,
      binding,
      true,
      [&](const primec::Expr &candidate) {
        ++emitCalls;
        CHECK(candidate.kind == primec::Expr::Kind::Literal);
        CHECK(candidate.literalValue == 9);
        return std::string("emit_init");
      });
  CHECK(constAuto.handled);
  CHECK(emitCalls == 1);
  CHECK(constAuto.emittedStatement == "const auto item = emit_init; ");

  binding.isMutable = true;
  emitCalls = 0;
  const auto mutableAuto = primec::emitter::runEmitterExprControlIfBlockBindingAutoStep(
      stmt,
      binding,
      true,
      [&](const primec::Expr &candidate) {
        ++emitCalls;
        CHECK(candidate.kind == primec::Expr::Kind::Literal);
        CHECK(candidate.literalValue == 9);
        return std::string("emit_mutable");
      });
  CHECK(mutableAuto.handled);
  CHECK(emitCalls == 1);
  CHECK(mutableAuto.emittedStatement == "auto item = emit_mutable; ");

  const auto missingEmit = primec::emitter::runEmitterExprControlIfBlockBindingAutoStep(
      stmt,
      binding,
      true,
      {});
  CHECK(missingEmit.handled);
  CHECK(missingEmit.emittedStatement == "auto item; ");
}

TEST_CASE("emitter expr control if-block binding-qualifiers step resolves const/reference flags") {
  primec::Emitter::BindingInfo mutableBinding;
  mutableBinding.isMutable = true;
  mutableBinding.isCopy = false;
  const auto mutableResult = primec::emitter::runEmitterExprControlIfBlockBindingQualifiersStep(
      mutableBinding,
      true,
      [&](const primec::Emitter::BindingInfo &) { return true; });
  CHECK_FALSE(mutableResult.needsConst);
  CHECK_FALSE(mutableResult.useRef);

  primec::Emitter::BindingInfo copyBinding;
  copyBinding.isMutable = false;
  copyBinding.isCopy = true;
  const auto copyResult = primec::emitter::runEmitterExprControlIfBlockBindingQualifiersStep(
      copyBinding,
      true,
      [&](const primec::Emitter::BindingInfo &) { return true; });
  CHECK(copyResult.needsConst);
  CHECK_FALSE(copyResult.useRef);

  primec::Emitter::BindingInfo constBinding;
  constBinding.isMutable = false;
  constBinding.isCopy = false;
  int referenceCandidateCalls = 0;
  const auto constResult = primec::emitter::runEmitterExprControlIfBlockBindingQualifiersStep(
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

  const auto noInitializer = primec::emitter::runEmitterExprControlIfBlockBindingQualifiersStep(
      constBinding,
      false,
      [&](const primec::Emitter::BindingInfo &) { return true; });
  CHECK(noInitializer.needsConst);
  CHECK_FALSE(noInitializer.useRef);

  const auto missingCallback = primec::emitter::runEmitterExprControlIfBlockBindingQualifiersStep(
      constBinding,
      true,
      {});
  CHECK(missingCallback.needsConst);
  CHECK_FALSE(missingCallback.useRef);
}

TEST_CASE("emitter expr control if-block binding-explicit step emits typed bindings") {
  primec::Expr stmt;
  stmt.kind = primec::Expr::Kind::Call;
  stmt.name = "item";

  primec::Emitter::BindingInfo bindingInfo;
  bindingInfo.typeName = "i32";
  bindingInfo.isMutable = false;

  const auto nonExplicit = primec::emitter::runEmitterExprControlIfBlockBindingExplicitStep(
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

  const auto constTyped = primec::emitter::runEmitterExprControlIfBlockBindingExplicitStep(
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
  const auto mutableTyped = primec::emitter::runEmitterExprControlIfBlockBindingExplicitStep(
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

  const auto constRefTyped = primec::emitter::runEmitterExprControlIfBlockBindingExplicitStep(
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

  const auto referenceTyped = primec::emitter::runEmitterExprControlIfBlockBindingExplicitStep(
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

  const auto missingEmit = primec::emitter::runEmitterExprControlIfBlockBindingExplicitStep(
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

TEST_CASE("emitter expr control if-block binding-fallback step emits inferred bindings") {
  primec::Expr stmt;
  stmt.kind = primec::Expr::Kind::Call;
  stmt.name = "item";

  const auto explicitType = primec::emitter::runEmitterExprControlIfBlockBindingFallbackStep(
      stmt,
      true,
      true,
      false,
      [&](const primec::Expr &) { return "unused"; });
  CHECK_FALSE(explicitType.handled);
  CHECK(explicitType.emittedStatement.empty());

  const auto constFallback = primec::emitter::runEmitterExprControlIfBlockBindingFallbackStep(
      stmt,
      false,
      true,
      false,
      [&](const primec::Expr &) { return "unused"; });
  CHECK(constFallback.handled);
  CHECK(constFallback.emittedStatement == "const auto item; ");

  primec::Expr initExpr;
  initExpr.kind = primec::Expr::Kind::Literal;
  initExpr.literalValue = 13;
  stmt.args = {initExpr};

  int emitCalls = 0;
  const auto mutableFallback = primec::emitter::runEmitterExprControlIfBlockBindingFallbackStep(
      stmt,
      false,
      false,
      false,
      [&](const primec::Expr &candidate) {
        ++emitCalls;
        CHECK(candidate.kind == primec::Expr::Kind::Literal);
        CHECK(candidate.literalValue == 13);
        return std::string("emit_init");
      });
  CHECK(mutableFallback.handled);
  CHECK(emitCalls == 1);
  CHECK(mutableFallback.emittedStatement == "auto item = emit_init; ");

  const auto refFallback = primec::emitter::runEmitterExprControlIfBlockBindingFallbackStep(
      stmt,
      false,
      true,
      true,
      [&](const primec::Expr &) { return "emit_ref"; });
  CHECK(refFallback.handled);
  CHECK(refFallback.emittedStatement == "const auto & item = emit_ref; ");

  const auto missingEmit = primec::emitter::runEmitterExprControlIfBlockBindingFallbackStep(
      stmt,
      false,
      false,
      false,
      {});
  CHECK(missingEmit.handled);
  CHECK(missingEmit.emittedStatement == "auto item; ");
}

TEST_CASE("emitter expr control if branch prelude step handles envelope entry cases") {
  primec::Expr stmt;
  stmt.kind = primec::Expr::Kind::Name;
  stmt.name = "value";

  int envelopeCalls = 0;
  int emitCalls = 0;
  const auto nonEnvelope = primec::emitter::runEmitterExprControlIfBranchPreludeStep(
      stmt,
      [&](const primec::Expr &candidate) {
        ++envelopeCalls;
        CHECK(candidate.kind == primec::Expr::Kind::Name);
        return false;
      },
      [&](const primec::Expr &candidate) {
        ++emitCalls;
        CHECK(candidate.name == "value");
        return std::string("emit_value");
      });
  CHECK(nonEnvelope.handled);
  CHECK(envelopeCalls == 1);
  CHECK(emitCalls == 1);
  CHECK(nonEnvelope.emittedExpr == "emit_value");

  primec::Expr emptyEnvelope;
  emptyEnvelope.kind = primec::Expr::Kind::Call;
  emptyEnvelope.hasBodyArguments = true;
  const auto emptyBody = primec::emitter::runEmitterExprControlIfBranchPreludeStep(
      emptyEnvelope,
      [&](const primec::Expr &) { return true; },
      [&](const primec::Expr &) {
        CHECK_FALSE(true);
        return std::string("unused");
      });
  CHECK(emptyBody.handled);
  CHECK(emptyBody.emittedExpr == "0");

  primec::Expr nonEmptyEnvelope;
  nonEmptyEnvelope.kind = primec::Expr::Kind::Call;
  nonEmptyEnvelope.hasBodyArguments = true;
  nonEmptyEnvelope.bodyArguments.push_back(stmt);
  const auto nonEmptyBody = primec::emitter::runEmitterExprControlIfBranchPreludeStep(
      nonEmptyEnvelope,
      [&](const primec::Expr &) { return true; },
      [&](const primec::Expr &) {
        CHECK_FALSE(true);
        return std::string("unused");
      });
  CHECK_FALSE(nonEmptyBody.handled);
  CHECK(nonEmptyBody.emittedExpr.empty());

  const auto missingCallbacks = primec::emitter::runEmitterExprControlIfBranchPreludeStep(
      stmt,
      {},
      {});
  CHECK_FALSE(missingCallbacks.handled);
  CHECK(missingCallbacks.emittedExpr.empty());
}

TEST_CASE("emitter expr control if branch wrapper step wraps branch body") {
  int bodyCalls = 0;
  const auto wrapped = primec::emitter::runEmitterExprControlIfBranchWrapperStep(
      [&]() {
        ++bodyCalls;
        return std::string("return 1; ");
      });
  CHECK(wrapped.handled);
  CHECK(bodyCalls == 1);
  CHECK(wrapped.emittedExpr == "([&]() { return 1; }())");

  const auto missingBody = primec::emitter::runEmitterExprControlIfBranchWrapperStep({});
  CHECK_FALSE(missingBody.handled);
  CHECK(missingBody.emittedExpr.empty());
}

TEST_CASE("emitter expr control if branch body step emits ordered statements") {
  primec::Expr first;
  first.kind = primec::Expr::Kind::Name;
  first.name = "first";

  primec::Expr second;
  second.kind = primec::Expr::Kind::Name;
  second.name = "second";

  primec::Expr third;
  third.kind = primec::Expr::Kind::Name;
  third.name = "third";

  primec::Expr candidate;
  candidate.kind = primec::Expr::Kind::Call;
  candidate.bodyArguments = {first, second, third};

  int emitCalls = 0;
  const auto emitted = primec::emitter::runEmitterExprControlIfBranchBodyStep(
      candidate,
      [&](const primec::Expr &stmt, bool isLast) {
        ++emitCalls;
        if (stmt.name == "first") {
          CHECK_FALSE(isLast);
          return primec::emitter::EmitterExprControlIfBranchBodyEmitResult{
              true, "first; ", false};
        }
        if (stmt.name == "second") {
          CHECK_FALSE(isLast);
          return primec::emitter::EmitterExprControlIfBranchBodyEmitResult{
              true, "second; ", true};
        }
        CHECK_FALSE(true);
        return primec::emitter::EmitterExprControlIfBranchBodyEmitResult{};
      });
  CHECK(emitted.handled);
  CHECK(emitCalls == 2);
  CHECK(emitted.emittedBody == "first; second; ");

  int skipCalls = 0;
  primec::Expr twoStmtCandidate;
  twoStmtCandidate.kind = primec::Expr::Kind::Call;
  twoStmtCandidate.bodyArguments = {first, second};
  const auto skipFirst = primec::emitter::runEmitterExprControlIfBranchBodyStep(
      twoStmtCandidate,
      [&](const primec::Expr &stmt, bool) {
        ++skipCalls;
        if (stmt.name == "first") {
          return primec::emitter::EmitterExprControlIfBranchBodyEmitResult{};
        }
        return primec::emitter::EmitterExprControlIfBranchBodyEmitResult{
            true, "second; ", false};
      });
  CHECK(skipFirst.handled);
  CHECK(skipCalls == 2);
  CHECK(skipFirst.emittedBody == "second; ");

  const auto emptyBody = primec::emitter::runEmitterExprControlIfBranchBodyStep(
      primec::Expr{},
      [&](const primec::Expr &, bool) {
        CHECK_FALSE(true);
        return primec::emitter::EmitterExprControlIfBranchBodyEmitResult{};
      });
  CHECK_FALSE(emptyBody.handled);
  CHECK(emptyBody.emittedBody.empty());

  const auto missingCallback = primec::emitter::runEmitterExprControlIfBranchBodyStep(
      candidate,
      {});
  CHECK_FALSE(missingCallback.handled);
  CHECK(missingCallback.emittedBody.empty());
}


TEST_SUITE_END();
