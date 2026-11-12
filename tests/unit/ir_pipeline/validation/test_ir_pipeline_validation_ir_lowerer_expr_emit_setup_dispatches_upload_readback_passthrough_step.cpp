#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir lowerer expr emit setup dispatches upload/readback passthrough step") {
  primec::ir_lowerer::LowerExprEmitMovePassthroughCallFn emitMovePassthroughCall;
  primec::ir_lowerer::LowerExprEmitUploadPassthroughCallFn emitUploadPassthroughCall;
  primec::ir_lowerer::LowerExprEmitReadbackPassthroughCallFn emitReadbackPassthroughCall;
  std::string error;
  REQUIRE(primec::ir_lowerer::runLowerExprEmitSetup(
      {},
      emitMovePassthroughCall,
      emitUploadPassthroughCall,
      emitReadbackPassthroughCall,
      error));
  REQUIRE(error.empty());

  primec::Expr literalExpr;
  literalExpr.kind = primec::Expr::Kind::Literal;
  literalExpr.literalValue = 9;

  int emitExprCalls = 0;
  auto emitExpr = [&](const primec::Expr &argExpr, const primec::ir_lowerer::LocalMap &) {
    ++emitExprCalls;
    return argExpr.kind == primec::Expr::Kind::Literal;
  };

  primec::Expr uploadExpr;
  uploadExpr.kind = primec::Expr::Kind::Call;
  uploadExpr.name = "upload";
  uploadExpr.args.push_back(literalExpr);
  CHECK(primec::ir_lowerer::runLowerExprEmitUploadReadbackPassthroughStep(
            uploadExpr,
            primec::ir_lowerer::LocalMap{},
            emitUploadPassthroughCall,
            emitReadbackPassthroughCall,
            emitExpr,
            error) == primec::ir_lowerer::UnaryPassthroughCallResult::Emitted);
  CHECK(error.empty());
  CHECK(emitExprCalls == 1);

  primec::Expr readbackExpr = uploadExpr;
  readbackExpr.name = "readback";
  emitExprCalls = 0;
  error.clear();
  CHECK(primec::ir_lowerer::runLowerExprEmitUploadReadbackPassthroughStep(
            readbackExpr,
            primec::ir_lowerer::LocalMap{},
            emitUploadPassthroughCall,
            emitReadbackPassthroughCall,
            emitExpr,
            error) == primec::ir_lowerer::UnaryPassthroughCallResult::Emitted);
  CHECK(error.empty());
  CHECK(emitExprCalls == 1);

  primec::Expr unknownExpr = uploadExpr;
  unknownExpr.name = "unknown";
  emitExprCalls = 0;
  error.clear();
  CHECK(primec::ir_lowerer::runLowerExprEmitUploadReadbackPassthroughStep(
            unknownExpr,
            primec::ir_lowerer::LocalMap{},
            emitUploadPassthroughCall,
            emitReadbackPassthroughCall,
            emitExpr,
            error) == primec::ir_lowerer::UnaryPassthroughCallResult::NotMatched);
  CHECK(error.empty());
  CHECK(emitExprCalls == 0);
}

TEST_CASE("ir lowerer expr emit setup validates upload/readback dispatch dependencies") {
  primec::ir_lowerer::LowerExprEmitMovePassthroughCallFn emitMovePassthroughCall;
  primec::ir_lowerer::LowerExprEmitUploadPassthroughCallFn emitUploadPassthroughCall;
  primec::ir_lowerer::LowerExprEmitReadbackPassthroughCallFn emitReadbackPassthroughCall;
  std::string error;
  REQUIRE(primec::ir_lowerer::runLowerExprEmitSetup(
      {},
      emitMovePassthroughCall,
      emitUploadPassthroughCall,
      emitReadbackPassthroughCall,
      error));
  REQUIRE(error.empty());

  primec::Expr uploadExpr;
  uploadExpr.kind = primec::Expr::Kind::Call;
  uploadExpr.name = "upload";
  primec::Expr literalExpr;
  literalExpr.kind = primec::Expr::Kind::Literal;
  literalExpr.literalValue = 1;
  uploadExpr.args.push_back(literalExpr);

  CHECK(primec::ir_lowerer::runLowerExprEmitUploadReadbackPassthroughStep(
            uploadExpr,
            primec::ir_lowerer::LocalMap{},
            {},
            emitReadbackPassthroughCall,
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            error) == primec::ir_lowerer::UnaryPassthroughCallResult::Error);
  CHECK(error == "native backend missing expr emit setup dependency: emitUploadPassthroughCall");

  CHECK(primec::ir_lowerer::runLowerExprEmitUploadReadbackPassthroughStep(
            uploadExpr,
            primec::ir_lowerer::LocalMap{},
            emitUploadPassthroughCall,
            {},
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            error) == primec::ir_lowerer::UnaryPassthroughCallResult::Error);
  CHECK(error == "native backend missing expr emit setup dependency: emitReadbackPassthroughCall");

  CHECK(primec::ir_lowerer::runLowerExprEmitUploadReadbackPassthroughStep(
            uploadExpr,
            primec::ir_lowerer::LocalMap{},
            emitUploadPassthroughCall,
            emitReadbackPassthroughCall,
            {},
            error) == primec::ir_lowerer::UnaryPassthroughCallResult::Error);
  CHECK(error == "native backend missing expr emit setup dependency: emitExpr");
}

TEST_SUITE_END();
