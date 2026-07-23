#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

namespace {

primec::Expr makeMethodCallExpr(const std::string &name, size_t argCount) {
  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.name = name;
  expr.isMethodCall = true;
  for (size_t i = 0; i < argCount; ++i) {
    primec::Expr receiver;
    receiver.kind = primec::Expr::Kind::Name;
    receiver.name = "receiver";
    expr.args.push_back(receiver);
  }
  return expr;
}

} // namespace

// TODO-4737: isBuiltinClassifiedMethodCallTarget is the single source of
// truth both IrLowererInlineNativeCallDispatch.cpp call sites now use to
// decide whether a semantic-product method-call target that has no
// materialized definition is a known builtin-classified exemption (and
// therefore safe to skip silently) versus a real gap that should surface
// as "semantic-product method-call target missing lowered definition".
// This test pins the exact classification surface so that widening it
// incorrectly (the TODO-4731 gap (c) bug class) fails here instead of
// silently letting an unbacked target through.
TEST_CASE("ir lowerer helpers classify exactly the known builtin method call target exemptions") {
  CHECK(primec::ir_lowerer::isBuiltinClassifiedMethodCallTarget(
      "/string/count", makeMethodCallExpr("count", 1)));
  CHECK(primec::ir_lowerer::isBuiltinClassifiedMethodCallTarget(
      "/std/collections/vector/count", makeMethodCallExpr("count", 1)));
  CHECK(primec::ir_lowerer::isBuiltinClassifiedMethodCallTarget(
      "/std/collections/vector/capacity", makeMethodCallExpr("capacity", 1)));
  CHECK(primec::ir_lowerer::isBuiltinClassifiedMethodCallTarget(
      "/std/collections/soa/count", makeMethodCallExpr("count", 1)));
  CHECK(primec::ir_lowerer::isBuiltinClassifiedMethodCallTarget(
      "/std/collections/soa/count", makeMethodCallExpr("anything", 3)));
  CHECK(primec::ir_lowerer::isBuiltinClassifiedMethodCallTarget(
      "/std/collections/vector/at", makeMethodCallExpr("at", 2)));
  CHECK(primec::ir_lowerer::isBuiltinClassifiedMethodCallTarget(
      "/std/collections/vector/at_unsafe", makeMethodCallExpr("at_unsafe", 2)));
  CHECK(primec::ir_lowerer::isBuiltinClassifiedMethodCallTarget(
      "/std/collections/soa/to_aos", makeMethodCallExpr("to_aos", 1)));
}

TEST_CASE("ir lowerer helpers reject targets outside the known builtin method call exemptions") {
  CHECK_FALSE(primec::ir_lowerer::isBuiltinClassifiedMethodCallTarget(
      "", makeMethodCallExpr("count", 1)));
  CHECK_FALSE(primec::ir_lowerer::isBuiltinClassifiedMethodCallTarget(
      "/std/collections/vector/push", makeMethodCallExpr("push", 2)));
  CHECK_FALSE(primec::ir_lowerer::isBuiltinClassifiedMethodCallTarget(
      "/std/collections/vector/count", makeMethodCallExpr("count", 2)));
  CHECK_FALSE(primec::ir_lowerer::isBuiltinClassifiedMethodCallTarget(
      "/std/collections/vector/count", makeMethodCallExpr("length", 1)));
  CHECK_FALSE(primec::ir_lowerer::isBuiltinClassifiedMethodCallTarget(
      "/std/collections/vector/at", makeMethodCallExpr("at", 1)));
  CHECK_FALSE(primec::ir_lowerer::isBuiltinClassifiedMethodCallTarget(
      "/std/collections/vector/capacity", makeMethodCallExpr("capacity", 2)));
}
