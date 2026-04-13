#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("emitter expr control if branch body return step dispatches returns") {
  primec::Expr returnArg;
  returnArg.kind = primec::Expr::Kind::Literal;
  returnArg.literalValue = 11;

  primec::Expr returnStmt;
  returnStmt.kind = primec::Expr::Kind::Call;
  returnStmt.name = "return";
  returnStmt.args = {returnArg};

  int nonLastEmitCalls = 0;
  const auto nonLast = primec::emitter::runEmitterExprControlIfBranchBodyReturnStep(
      returnStmt,
      false,
      [&](const primec::Expr &candidate) {
        CHECK(candidate.kind == primec::Expr::Kind::Call);
        CHECK(candidate.name == "return");
        return true;
      },
      [&](const primec::Expr &candidate) {
        ++nonLastEmitCalls;
        CHECK(candidate.kind == primec::Expr::Kind::Literal);
        CHECK(candidate.literalValue == 11);
        return std::string("emit_return");
      });
  CHECK(nonLast.handled);
  CHECK(nonLast.emitted.handled);
  CHECK(nonLastEmitCalls == 1);
  CHECK(nonLast.emitted.emittedStatement == "return emit_return; ");
  CHECK(nonLast.emitted.shouldBreak);

  primec::Expr tailValue;
  tailValue.kind = primec::Expr::Kind::Name;
  tailValue.name = "tail";

  int tailEmitCalls = 0;
  const auto lastValue = primec::emitter::runEmitterExprControlIfBranchBodyReturnStep(
      tailValue,
      true,
      [&](const primec::Expr &candidate) {
        CHECK(candidate.kind == primec::Expr::Kind::Name);
        CHECK(candidate.name == "tail");
        return false;
      },
      [&](const primec::Expr &candidate) {
        ++tailEmitCalls;
        CHECK(candidate.kind == primec::Expr::Kind::Name);
        CHECK(candidate.name == "tail");
        return std::string("emit_tail");
      });
  CHECK(lastValue.handled);
  CHECK(lastValue.emitted.handled);
  CHECK(tailEmitCalls == 1);
  CHECK(lastValue.emitted.emittedStatement == "return emit_tail; ");
  CHECK(lastValue.emitted.shouldBreak);

  primec::Expr bindingTail = tailValue;
  bindingTail.isBinding = true;
  const auto bindingResult = primec::emitter::runEmitterExprControlIfBranchBodyReturnStep(
      bindingTail,
      true,
      [&](const primec::Expr &) { return false; },
      [&](const primec::Expr &) {
        CHECK_FALSE(true);
        return std::string{};
      });
  CHECK(bindingResult.handled);
  CHECK(bindingResult.emitted.handled);
  CHECK(bindingResult.emitted.emittedStatement == "return 0; ");
  CHECK(bindingResult.emitted.shouldBreak);

  const auto missingCallbacks = primec::emitter::runEmitterExprControlIfBranchBodyReturnStep(
      returnStmt,
      false,
      {},
      {});
  CHECK_FALSE(missingCallbacks.handled);
  CHECK_FALSE(missingCallbacks.emitted.handled);
  CHECK(missingCallbacks.emitted.emittedStatement.empty());
  CHECK_FALSE(missingCallbacks.emitted.shouldBreak);
}

TEST_CASE("emitter expr control if branch body binding step dispatches bindings") {
  primec::Expr nonBinding;
  nonBinding.kind = primec::Expr::Kind::Name;
  nonBinding.name = "value";

  std::unordered_map<std::string, primec::Emitter::BindingInfo> branchTypes;
  const std::unordered_map<std::string, primec::Emitter::ReturnKind> returnKinds;
  const std::unordered_map<std::string, std::string> importAliases;
  const std::unordered_map<std::string, std::string> structTypeMap;

  const auto nonBindingResult = primec::emitter::runEmitterExprControlIfBranchBodyBindingStep(
      nonBinding,
      branchTypes,
      returnKinds,
      false,
      importAliases,
      structTypeMap,
      [&](const primec::Expr &) { return primec::Emitter::BindingInfo{}; },
      [&](const primec::Expr &) { return false; },
      {},
      {},
      [&](const primec::Emitter::BindingInfo &) { return false; },
      [&](const primec::Expr &) { return std::string("unused"); });
  CHECK_FALSE(nonBindingResult.handled);
  CHECK_FALSE(nonBindingResult.emitted.handled);
  CHECK(branchTypes.empty());

  primec::Expr lambdaExpr;
  lambdaExpr.kind = primec::Expr::Kind::Call;
  lambdaExpr.isLambda = true;

  primec::Expr lambdaBinding;
  lambdaBinding.kind = primec::Expr::Kind::Call;
  lambdaBinding.isBinding = true;
  lambdaBinding.name = "item";
  lambdaBinding.args = {lambdaExpr};

  int autoEmitCalls = 0;
  primec::Emitter::BindingInfo autoBindingInfo;
  autoBindingInfo.typeName = "auto";
  autoBindingInfo.isMutable = false;
  branchTypes.clear();
  const auto autoBindingResult = primec::emitter::runEmitterExprControlIfBranchBodyBindingStep(
      lambdaBinding,
      branchTypes,
      returnKinds,
      true,
      importAliases,
      structTypeMap,
      [&](const primec::Expr &candidate) {
        CHECK(candidate.name == "item");
        return autoBindingInfo;
      },
      [&](const primec::Expr &) { return false; },
      [&](const primec::Expr &, const auto &, const auto &, bool) {
        CHECK_FALSE(true);
        return primec::Emitter::ReturnKind::Unknown;
      },
      [&](primec::Emitter::ReturnKind) {
        CHECK_FALSE(true);
        return std::string{};
      },
      [&](const primec::Emitter::BindingInfo &) {
        CHECK_FALSE(true);
        return false;
      },
      [&](const primec::Expr &candidate) {
        ++autoEmitCalls;
        CHECK(candidate.isLambda);
        return std::string("emit_lambda");
      });
  CHECK(autoBindingResult.handled);
  CHECK(autoBindingResult.emitted.handled);
  CHECK(autoBindingResult.emitted.emittedStatement == "const auto item = emit_lambda; ");
  CHECK_FALSE(autoBindingResult.emitted.shouldBreak);
  CHECK(autoEmitCalls == 1);
  REQUIRE(branchTypes.count("item") == 1);

  primec::Expr initExpr;
  initExpr.kind = primec::Expr::Kind::Literal;
  initExpr.literalValue = 9;

  primec::Expr fallbackBinding = lambdaBinding;
  fallbackBinding.args = {initExpr};
  branchTypes.clear();

  int inferCalls = 0;
  int fallbackEmitCalls = 0;
  const auto fallbackResult = primec::emitter::runEmitterExprControlIfBranchBodyBindingStep(
      fallbackBinding,
      branchTypes,
      returnKinds,
      true,
      importAliases,
      structTypeMap,
      [&](const primec::Expr &) { return autoBindingInfo; },
      [&](const primec::Expr &) { return false; },
      [&](const primec::Expr &candidate, const auto &, const auto &, bool candidateAllowMathBare) {
        ++inferCalls;
        CHECK(candidate.kind == primec::Expr::Kind::Literal);
        CHECK(candidate.literalValue == 9);
        CHECK(candidateAllowMathBare);
        return primec::Emitter::ReturnKind::Unknown;
      },
      [&](primec::Emitter::ReturnKind kind) {
        CHECK(kind == primec::Emitter::ReturnKind::Unknown);
        return std::string{};
      },
      [&](const primec::Emitter::BindingInfo &) { return false; },
      [&](const primec::Expr &candidate) {
        ++fallbackEmitCalls;
        CHECK(candidate.kind == primec::Expr::Kind::Literal);
        CHECK(candidate.literalValue == 9);
        return std::string("emit_value");
      });
  CHECK(fallbackResult.handled);
  CHECK(fallbackResult.emitted.handled);
  CHECK(fallbackResult.emitted.emittedStatement == "const auto item = emit_value; ");
  CHECK_FALSE(fallbackResult.emitted.shouldBreak);
  CHECK(inferCalls == 1);
  CHECK(fallbackEmitCalls == 1);
  REQUIRE(branchTypes.count("item") == 1);
}

TEST_CASE("emitter expr control if branch body dispatch step routes handlers") {
  primec::Expr stmt;
  stmt.kind = primec::Expr::Kind::Name;
  stmt.name = "value";

  int returnCalls = 0;
  int bindingCalls = 0;
  int statementCalls = 0;
  const auto returnFirst = primec::emitter::runEmitterExprControlIfBranchBodyDispatchStep(
      stmt,
      false,
      [&](const primec::Expr &candidate, bool isLast) {
        ++returnCalls;
        CHECK(candidate.name == "value");
        CHECK_FALSE(isLast);
        return primec::emitter::EmitterExprControlIfBranchBodyEmitResult{
            true, "return_stmt; ", true};
      },
      [&](const primec::Expr &) {
        ++bindingCalls;
        return primec::emitter::EmitterExprControlIfBranchBodyEmitResult{};
      },
      [&](const primec::Expr &) {
        ++statementCalls;
        return primec::emitter::EmitterExprControlIfBranchBodyEmitResult{};
      });
  CHECK(returnFirst.handled);
  CHECK(returnFirst.emitted.handled);
  CHECK(returnFirst.emitted.emittedStatement == "return_stmt; ");
  CHECK(returnFirst.emitted.shouldBreak);
  CHECK(returnCalls == 1);
  CHECK(bindingCalls == 0);
  CHECK(statementCalls == 0);

  returnCalls = 0;
  bindingCalls = 0;
  statementCalls = 0;
  const auto bindingFallback = primec::emitter::runEmitterExprControlIfBranchBodyDispatchStep(
      stmt,
      true,
      [&](const primec::Expr &, bool isLast) {
        ++returnCalls;
        CHECK(isLast);
        return primec::emitter::EmitterExprControlIfBranchBodyEmitResult{};
      },
      [&](const primec::Expr &candidate) {
        ++bindingCalls;
        CHECK(candidate.name == "value");
        return primec::emitter::EmitterExprControlIfBranchBodyEmitResult{
            true, "binding_stmt; ", false};
      },
      [&](const primec::Expr &) {
        ++statementCalls;
        return primec::emitter::EmitterExprControlIfBranchBodyEmitResult{};
      });
  CHECK(bindingFallback.handled);
  CHECK(bindingFallback.emitted.handled);
  CHECK(bindingFallback.emitted.emittedStatement == "binding_stmt; ");
  CHECK_FALSE(bindingFallback.emitted.shouldBreak);
  CHECK(returnCalls == 1);
  CHECK(bindingCalls == 1);
  CHECK(statementCalls == 0);

  returnCalls = 0;
  bindingCalls = 0;
  statementCalls = 0;
  const auto statementFallback = primec::emitter::runEmitterExprControlIfBranchBodyDispatchStep(
      stmt,
      false,
      [&](const primec::Expr &, bool) {
        ++returnCalls;
        return primec::emitter::EmitterExprControlIfBranchBodyEmitResult{};
      },
      [&](const primec::Expr &) {
        ++bindingCalls;
        return primec::emitter::EmitterExprControlIfBranchBodyEmitResult{};
      },
      [&](const primec::Expr &candidate) {
        ++statementCalls;
        CHECK(candidate.name == "value");
        return primec::emitter::EmitterExprControlIfBranchBodyEmitResult{
            true, "statement_stmt; ", false};
      });
  CHECK(statementFallback.handled);
  CHECK(statementFallback.emitted.handled);
  CHECK(statementFallback.emitted.emittedStatement == "statement_stmt; ");
  CHECK_FALSE(statementFallback.emitted.shouldBreak);
  CHECK(returnCalls == 1);
  CHECK(bindingCalls == 1);
  CHECK(statementCalls == 1);

  const auto noneHandled = primec::emitter::runEmitterExprControlIfBranchBodyDispatchStep(
      stmt,
      false,
      [&](const primec::Expr &, bool) {
        return primec::emitter::EmitterExprControlIfBranchBodyEmitResult{};
      },
      [&](const primec::Expr &) {
        return primec::emitter::EmitterExprControlIfBranchBodyEmitResult{};
      },
      [&](const primec::Expr &) {
        return primec::emitter::EmitterExprControlIfBranchBodyEmitResult{};
      });
  CHECK_FALSE(noneHandled.handled);
  CHECK_FALSE(noneHandled.emitted.handled);
  CHECK(noneHandled.emitted.emittedStatement.empty());
  CHECK_FALSE(noneHandled.emitted.shouldBreak);

  const auto missingCallbacks = primec::emitter::runEmitterExprControlIfBranchBodyDispatchStep(
      stmt,
      false,
      {},
      {},
      {});
  CHECK_FALSE(missingCallbacks.handled);
  CHECK_FALSE(missingCallbacks.emitted.handled);
  CHECK(missingCallbacks.emitted.emittedStatement.empty());
  CHECK_FALSE(missingCallbacks.emitted.shouldBreak);
}

TEST_CASE("emitter expr control if branch body handlers step composes handlers") {
  std::unordered_map<std::string, primec::Emitter::BindingInfo> branchTypes;
  const std::unordered_map<std::string, primec::Emitter::ReturnKind> returnKinds;
  const std::unordered_map<std::string, std::string> importAliases;
  const std::unordered_map<std::string, std::string> structTypeMap;

  primec::Expr returnArg;
  returnArg.kind = primec::Expr::Kind::Literal;
  returnArg.literalValue = 12;
  primec::Expr returnStmt;
  returnStmt.kind = primec::Expr::Kind::Call;
  returnStmt.name = "return";
  returnStmt.args = {returnArg};

  int returnEmitCalls = 0;
  const auto returnResult = primec::emitter::runEmitterExprControlIfBranchBodyHandlersStep(
      returnStmt,
      false,
      branchTypes,
      returnKinds,
      true,
      importAliases,
      structTypeMap,
      [&](const primec::Expr &candidate) { return candidate.name == "return"; },
      [&](const primec::Expr &) {
        CHECK_FALSE(true);
        return primec::Emitter::BindingInfo{};
      },
      [&](const primec::Expr &) {
        CHECK_FALSE(true);
        return false;
      },
      [&](const primec::Expr &, const auto &, const auto &, bool) {
        CHECK_FALSE(true);
        return primec::Emitter::ReturnKind::Unknown;
      },
      [&](primec::Emitter::ReturnKind) {
        CHECK_FALSE(true);
        return std::string{};
      },
      [&](const primec::Emitter::BindingInfo &) {
        CHECK_FALSE(true);
        return false;
      },
      [&](const primec::Expr &candidate) {
        ++returnEmitCalls;
        CHECK(candidate.kind == primec::Expr::Kind::Literal);
        CHECK(candidate.literalValue == 12);
        return std::string("emit_return");
      });
  CHECK(returnResult.handled);
  CHECK(returnResult.emitted.handled);
  CHECK(returnResult.emitted.emittedStatement == "return emit_return; ");
  CHECK(returnResult.emitted.shouldBreak);
  CHECK(returnEmitCalls == 1);

  primec::Expr lambdaExpr;
  lambdaExpr.kind = primec::Expr::Kind::Call;
  lambdaExpr.isLambda = true;
  primec::Expr bindingStmt;
  bindingStmt.kind = primec::Expr::Kind::Call;
  bindingStmt.isBinding = true;
  bindingStmt.name = "item";
  bindingStmt.args = {lambdaExpr};
  branchTypes.clear();

  int bindingEmitCalls = 0;
  const auto bindingResult = primec::emitter::runEmitterExprControlIfBranchBodyHandlersStep(
      bindingStmt,
      false,
      branchTypes,
      returnKinds,
      false,
      importAliases,
      structTypeMap,
      [&](const primec::Expr &) { return false; },
      [&](const primec::Expr &) {
        primec::Emitter::BindingInfo info;
        info.typeName = "auto";
        info.isMutable = false;
        return info;
      },
      [&](const primec::Expr &) { return false; },
      [&](const primec::Expr &, const auto &, const auto &, bool) {
        CHECK_FALSE(true);
        return primec::Emitter::ReturnKind::Unknown;
      },
      [&](primec::Emitter::ReturnKind) {
        CHECK_FALSE(true);
        return std::string{};
      },
      [&](const primec::Emitter::BindingInfo &) {
        CHECK_FALSE(true);
        return false;
      },
      [&](const primec::Expr &candidate) {
        ++bindingEmitCalls;
        CHECK(candidate.isLambda);
        return std::string("emit_lambda");
      });
  CHECK(bindingResult.handled);
  CHECK(bindingResult.emitted.handled);
  CHECK(bindingResult.emitted.emittedStatement == "const auto item = emit_lambda; ");
  CHECK_FALSE(bindingResult.emitted.shouldBreak);
  CHECK(bindingEmitCalls == 1);
  REQUIRE(branchTypes.count("item") == 1);

  primec::Expr statementStmt;
  statementStmt.kind = primec::Expr::Kind::Name;
  statementStmt.name = "value";
  int statementEmitCalls = 0;
  const auto statementResult = primec::emitter::runEmitterExprControlIfBranchBodyHandlersStep(
      statementStmt,
      true,
      branchTypes,
      returnKinds,
      false,
      importAliases,
      structTypeMap,
      [&](const primec::Expr &) { return false; },
      {},
      {},
      {},
      {},
      {},
      [&](const primec::Expr &candidate) {
        ++statementEmitCalls;
        CHECK(candidate.name == "value");
        return std::string("emit_stmt");
      });
  CHECK(statementResult.handled);
  CHECK(statementResult.emitted.handled);
  CHECK(statementResult.emitted.emittedStatement == "return emit_stmt; ");
  CHECK(statementResult.emitted.shouldBreak);
  CHECK(statementEmitCalls == 1);

  const auto missingCallbacks = primec::emitter::runEmitterExprControlIfBranchBodyHandlersStep(
      statementStmt,
      false,
      branchTypes,
      returnKinds,
      false,
      importAliases,
      structTypeMap,
      {},
      {},
      {},
      {},
      {},
      {},
      {});
  CHECK_FALSE(missingCallbacks.handled);
  CHECK_FALSE(missingCallbacks.emitted.handled);
  CHECK(missingCallbacks.emitted.emittedStatement.empty());
  CHECK_FALSE(missingCallbacks.emitted.shouldBreak);
}

TEST_CASE("emitter expr control if branch value step composes prelude body and wrapper") {
  primec::Expr nonEnvelope;
  nonEnvelope.kind = primec::Expr::Kind::Name;
  nonEnvelope.name = "direct";

  int nonEnvelopeEmitCalls = 0;
  const auto directResult = primec::emitter::runEmitterExprControlIfBranchValueStep(
      nonEnvelope,
      [&](const primec::Expr &) { return false; },
      [&](const primec::Expr &candidate) {
        ++nonEnvelopeEmitCalls;
        CHECK(candidate.kind == primec::Expr::Kind::Name);
        CHECK(candidate.name == "direct");
        return std::string("emit_direct");
      },
      [&](const primec::Expr &, bool) {
        CHECK_FALSE(true);
        return primec::emitter::EmitterExprControlIfBranchBodyEmitResult{};
      });
  CHECK(directResult.handled);
  CHECK(nonEnvelopeEmitCalls == 1);
  CHECK(directResult.emittedExpr == "emit_direct");

  primec::Expr stmt;
  stmt.kind = primec::Expr::Kind::Name;
  stmt.name = "value";

  primec::Expr envelope;
  envelope.kind = primec::Expr::Kind::Call;
  envelope.bodyArguments = {stmt};

  int statementCalls = 0;
  const auto wrapped = primec::emitter::runEmitterExprControlIfBranchValueStep(
      envelope,
      [&](const primec::Expr &) { return true; },
      [&](const primec::Expr &) {
        CHECK_FALSE(true);
        return std::string{};
      },
      [&](const primec::Expr &candidate, bool isLast) {
        ++statementCalls;
        CHECK(candidate.kind == primec::Expr::Kind::Name);
        CHECK(candidate.name == "value");
        CHECK(isLast);
        return primec::emitter::EmitterExprControlIfBranchBodyEmitResult{
            true, "return 7; ", true};
      });
  CHECK(wrapped.handled);
  CHECK(statementCalls == 1);
  CHECK(wrapped.emittedExpr == "([&]() { return 7; }())");

  primec::Expr emptyEnvelope;
  emptyEnvelope.kind = primec::Expr::Kind::Call;
  const auto emptyResult = primec::emitter::runEmitterExprControlIfBranchValueStep(
      emptyEnvelope,
      [&](const primec::Expr &) { return true; },
      [&](const primec::Expr &) {
        CHECK_FALSE(true);
        return std::string{};
      },
      [&](const primec::Expr &, bool) {
        CHECK_FALSE(true);
        return primec::emitter::EmitterExprControlIfBranchBodyEmitResult{};
      });
  CHECK(emptyResult.handled);
  CHECK(emptyResult.emittedExpr == "0");

  const auto missingCallbacks = primec::emitter::runEmitterExprControlIfBranchValueStep(
      nonEnvelope,
      {},
      {},
      {});
  CHECK_FALSE(missingCallbacks.handled);
  CHECK(missingCallbacks.emittedExpr.empty());
}


TEST_SUITE_END();
