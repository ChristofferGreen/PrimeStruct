#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir lowerer setup inference helper handles non-math and invalid math calls") {
  using Resolution = primec::ir_lowerer::MathBuiltinReturnKindResolution;

  primec::Expr nonMath;
  nonMath.kind = primec::Expr::Kind::Call;
  nonMath.name = "plus";

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  CHECK(primec::ir_lowerer::inferMathBuiltinReturnKind(
            nonMath,
            {},
            true,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            [](primec::ir_lowerer::LocalInfo::ValueKind left, primec::ir_lowerer::LocalInfo::ValueKind) {
              return left;
            },
            kindOut) == Resolution::NotMatched);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);

  primec::Expr invalidClamp;
  invalidClamp.kind = primec::Expr::Kind::Call;
  invalidClamp.name = "clamp";
  invalidClamp.args = {};
  CHECK(primec::ir_lowerer::inferMathBuiltinReturnKind(
            invalidClamp,
            {},
            true,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            [](primec::ir_lowerer::LocalInfo::ValueKind left, primec::ir_lowerer::LocalInfo::ValueKind) {
              return left;
            },
            kindOut) == Resolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
}

TEST_CASE("ir lowerer setup inference helper infers non-math scalar call return kinds") {
  using Resolution = primec::ir_lowerer::NonMathScalarCallReturnKindResolution;

  primec::Expr x;
  x.kind = primec::Expr::Kind::Name;
  x.name = "x";

  primec::Expr convertExpr;
  convertExpr.kind = primec::Expr::Kind::Call;
  convertExpr.name = "convert";
  convertExpr.templateArgs = {"f64"};
  convertExpr.args = {x};

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(primec::ir_lowerer::inferNonMathScalarCallReturnKind(
            convertExpr,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Int32;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            kindOut) == Resolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Float64);

  primec::Expr incrementExpr;
  incrementExpr.kind = primec::Expr::Kind::Call;
  incrementExpr.name = "increment";
  incrementExpr.args = {x};
  CHECK(primec::ir_lowerer::inferNonMathScalarCallReturnKind(
            incrementExpr,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Int64;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            kindOut) == Resolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Int64);

  primec::Expr target;
  target.kind = primec::Expr::Kind::Name;
  target.name = "dst";
  primec::Expr assignExpr;
  assignExpr.kind = primec::Expr::Kind::Call;
  assignExpr.name = "assign";
  assignExpr.args = {target, x};

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo dstInfo;
  dstInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  dstInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Bool;
  locals.emplace("dst", dstInfo);
  primec::ir_lowerer::LocalInfo ptrInfo;
  ptrInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Pointer;
  ptrInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Float32;
  locals.emplace("ptr", ptrInfo);
  CHECK(primec::ir_lowerer::inferNonMathScalarCallReturnKind(
            assignExpr,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            kindOut) == Resolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Bool);

  primec::Expr ptr;
  ptr.kind = primec::Expr::Kind::Name;
  ptr.name = "ptr";
  primec::Expr offset;
  offset.kind = primec::Expr::Kind::Literal;
  offset.literalValue = 16;
  offset.intWidth = 32;
  primec::Expr pointerOffsetExpr;
  pointerOffsetExpr.kind = primec::Expr::Kind::Call;
  pointerOffsetExpr.name = "plus";
  pointerOffsetExpr.args = {ptr, offset};
  primec::Expr dereferenceTarget;
  dereferenceTarget.kind = primec::Expr::Kind::Call;
  dereferenceTarget.name = "dereference";
  dereferenceTarget.args = {ptr};
  assignExpr.args = {dereferenceTarget, x};
  CHECK(primec::ir_lowerer::inferNonMathScalarCallReturnKind(
            assignExpr,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            [](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
              if (expr.kind == primec::Expr::Kind::Name && expr.name == "ptr") {
                return primec::ir_lowerer::LocalInfo::ValueKind::Float32;
              }
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            kindOut) == Resolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Float32);

  dereferenceTarget.args = {pointerOffsetExpr};
  CHECK(primec::ir_lowerer::inferNonMathScalarCallReturnKind(
            dereferenceTarget,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            kindOut) == Resolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Float32);
}

TEST_CASE("ir lowerer setup inference helper handles invalid non-math scalar calls") {
  using Resolution = primec::ir_lowerer::NonMathScalarCallReturnKindResolution;

  primec::Expr nonScalar;
  nonScalar.kind = primec::Expr::Kind::Call;
  nonScalar.name = "plus";

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  CHECK(primec::ir_lowerer::inferNonMathScalarCallReturnKind(
            nonScalar,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            kindOut) == Resolution::NotMatched);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);

  primec::Expr invalidConvert;
  invalidConvert.kind = primec::Expr::Kind::Call;
  invalidConvert.name = "convert";
  CHECK(primec::ir_lowerer::inferNonMathScalarCallReturnKind(
            invalidConvert,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            kindOut) == Resolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);

  primec::Expr invalidAssign;
  invalidAssign.kind = primec::Expr::Kind::Call;
  invalidAssign.name = "assign";
  invalidAssign.args = {nonScalar};
  CHECK(primec::ir_lowerer::inferNonMathScalarCallReturnKind(
            invalidAssign,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            kindOut) == Resolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);

  primec::Expr mapTarget;
  mapTarget.kind = primec::Expr::Kind::Name;
  mapTarget.name = "m";
  invalidAssign.args = {mapTarget, nonScalar};
  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo mapInfo;
  mapInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Map;
  mapInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  locals.emplace("m", mapInfo);
  CHECK(primec::ir_lowerer::inferNonMathScalarCallReturnKind(
            invalidAssign,
            locals,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            kindOut) == Resolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
}

TEST_CASE("ir lowerer setup inference helper infers control-flow call return kinds") {
  using Resolution = primec::ir_lowerer::ControlFlowCallReturnKindResolution;

  primec::Expr condition;
  condition.kind = primec::Expr::Kind::Name;
  condition.name = "cond";
  primec::Expr thenBodyValue;
  thenBodyValue.kind = primec::Expr::Kind::Name;
  thenBodyValue.name = "thenValue";
  primec::Expr elseValue;
  elseValue.kind = primec::Expr::Kind::Name;
  elseValue.name = "elseValue";

  primec::Expr thenEnvelope;
  thenEnvelope.kind = primec::Expr::Kind::Call;
  thenEnvelope.name = "then";
  thenEnvelope.hasBodyArguments = true;
  thenEnvelope.bodyArguments = {thenBodyValue};

  primec::Expr ifExpr;
  ifExpr.kind = primec::Expr::Kind::Call;
  ifExpr.name = "if";
  ifExpr.args = {condition, thenEnvelope, elseValue};

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  std::string error;
  CHECK(primec::ir_lowerer::inferControlFlowCallReturnKind(
            ifExpr,
            {},
            [](const primec::Expr &) { return std::string(); },
            [](const primec::Expr &, primec::Expr &, std::string &) { return false; },
            [](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
              if (expr.kind == primec::Expr::Kind::Name && expr.name == "elseValue") {
                return primec::ir_lowerer::LocalInfo::ValueKind::Int32;
              }
              if (expr.kind == primec::Expr::Kind::Name && expr.name == "cond") {
                return primec::ir_lowerer::LocalInfo::ValueKind::Bool;
              }
              if (expr.kind == primec::Expr::Kind::Call && expr.name == "expanded_match") {
                return primec::ir_lowerer::LocalInfo::ValueKind::Float32;
              }
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            [](primec::ir_lowerer::LocalInfo::ValueKind left, primec::ir_lowerer::LocalInfo::ValueKind right) {
              if (left == primec::ir_lowerer::LocalInfo::ValueKind::Int64 ||
                  right == primec::ir_lowerer::LocalInfo::ValueKind::Int64) {
                return primec::ir_lowerer::LocalInfo::ValueKind::Int64;
              }
              return left;
            },
            [](const std::vector<primec::Expr> &body, const primec::ir_lowerer::LocalMap &) {
              if (!body.empty() && body.front().kind == primec::Expr::Kind::Name &&
                  body.front().name == "thenValue") {
                return primec::ir_lowerer::LocalInfo::ValueKind::Int64;
              }
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            [](const std::string &) { return false; },
            error,
            kindOut) == Resolution::Resolved);
  CHECK(error.empty());
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Int64);

  primec::Expr matchExpr;
  matchExpr.kind = primec::Expr::Kind::Call;
  matchExpr.name = "match";
  CHECK(primec::ir_lowerer::inferControlFlowCallReturnKind(
            matchExpr,
            {},
            [](const primec::Expr &) { return std::string(); },
            [](const primec::Expr &, primec::Expr &expanded, std::string &) {
              expanded.kind = primec::Expr::Kind::Call;
              expanded.name = "expanded_match";
              return true;
            },
            [](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
              if (expr.kind == primec::Expr::Kind::Call && expr.name == "expanded_match") {
                return primec::ir_lowerer::LocalInfo::ValueKind::Float32;
              }
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            [](primec::ir_lowerer::LocalInfo::ValueKind left, primec::ir_lowerer::LocalInfo::ValueKind) {
              return left;
            },
            [](const std::vector<primec::Expr> &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            [](const std::string &) { return false; },
            error,
            kindOut) == Resolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Float32);

  primec::Expr blockExpr;
  blockExpr.kind = primec::Expr::Kind::Call;
  blockExpr.name = "block";
  blockExpr.hasBodyArguments = true;
  blockExpr.bodyArguments = {thenBodyValue};
  CHECK(primec::ir_lowerer::inferControlFlowCallReturnKind(
            blockExpr,
            {},
            [](const primec::Expr &) { return "/pkg/block"; },
            [](const primec::Expr &, primec::Expr &, std::string &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            [](primec::ir_lowerer::LocalInfo::ValueKind left, primec::ir_lowerer::LocalInfo::ValueKind) {
              return left;
            },
            [](const std::vector<primec::Expr> &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Bool;
            },
            [](const std::string &) { return false; },
            error,
            kindOut) == Resolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Bool);
}

TEST_CASE("ir lowerer setup inference helper handles invalid control-flow calls") {
  using Resolution = primec::ir_lowerer::ControlFlowCallReturnKindResolution;

  primec::Expr nonControl;
  nonControl.kind = primec::Expr::Kind::Call;
  nonControl.name = "plus";

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  std::string error = "unchanged";
  CHECK(primec::ir_lowerer::inferControlFlowCallReturnKind(
            nonControl,
            {},
            [](const primec::Expr &) { return std::string(); },
            [](const primec::Expr &, primec::Expr &, std::string &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            [](primec::ir_lowerer::LocalInfo::ValueKind left, primec::ir_lowerer::LocalInfo::ValueKind) {
              return left;
            },
            [](const std::vector<primec::Expr> &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            [](const std::string &) { return false; },
            error,
            kindOut) == Resolution::NotMatched);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
  CHECK(error == "unchanged");

  primec::Expr matchExpr;
  matchExpr.kind = primec::Expr::Kind::Call;
  matchExpr.name = "match";
  error.clear();
  CHECK(primec::ir_lowerer::inferControlFlowCallReturnKind(
            matchExpr,
            {},
            [](const primec::Expr &) { return std::string(); },
            [](const primec::Expr &, primec::Expr &, std::string &errorOut) {
              errorOut = "match lowering failed";
              return false;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            [](primec::ir_lowerer::LocalInfo::ValueKind left, primec::ir_lowerer::LocalInfo::ValueKind) {
              return left;
            },
            [](const std::vector<primec::Expr> &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            [](const std::string &) { return false; },
            error,
            kindOut) == Resolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
  CHECK(error == "match lowering failed");

  primec::Expr ifExpr;
  ifExpr.kind = primec::Expr::Kind::Call;
  ifExpr.name = "if";
  ifExpr.args = {nonControl};
  CHECK(primec::ir_lowerer::inferControlFlowCallReturnKind(
            ifExpr,
            {},
            [](const primec::Expr &) { return std::string(); },
            [](const primec::Expr &, primec::Expr &, std::string &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            [](primec::ir_lowerer::LocalInfo::ValueKind left, primec::ir_lowerer::LocalInfo::ValueKind) {
              return left;
            },
            [](const std::vector<primec::Expr> &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            [](const std::string &) { return false; },
            error,
            kindOut) == Resolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);

  primec::Expr body;
  body.kind = primec::Expr::Kind::Name;
  body.name = "body";
  primec::Expr blockExpr;
  blockExpr.kind = primec::Expr::Kind::Call;
  blockExpr.name = "block";
  blockExpr.hasBodyArguments = true;
  blockExpr.bodyArguments = {body};
  CHECK(primec::ir_lowerer::inferControlFlowCallReturnKind(
            blockExpr,
            {},
            [](const primec::Expr &) { return "/pkg/block"; },
            [](const primec::Expr &, primec::Expr &, std::string &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            [](primec::ir_lowerer::LocalInfo::ValueKind left, primec::ir_lowerer::LocalInfo::ValueKind) {
              return left;
            },
            [](const std::vector<primec::Expr> &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Int32;
            },
            [](const std::string &path) { return path == "/pkg/block"; },
            error,
            kindOut) == Resolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
}

TEST_CASE("ir lowerer setup inference helper infers pointer builtin call return kinds") {
  using Resolution = primec::ir_lowerer::PointerBuiltinCallReturnKindResolution;

  primec::Expr pointerName;
  pointerName.kind = primec::Expr::Kind::Name;
  pointerName.name = "ptr";

  primec::Expr dereferenceExpr;
  dereferenceExpr.kind = primec::Expr::Kind::Call;
  dereferenceExpr.name = "dereference";
  dereferenceExpr.args = {pointerName};

  primec::Expr allocExpr;
  allocExpr.kind = primec::Expr::Kind::Call;
  allocExpr.name = "/std/intrinsics/memory/alloc";
  allocExpr.templateArgs = {"i32"};
  primec::Expr allocCount;
  allocCount.kind = primec::Expr::Kind::Literal;
  allocCount.literalValue = 2;
  allocExpr.args = {allocCount};

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(primec::ir_lowerer::inferPointerBuiltinCallReturnKind(
            dereferenceExpr,
            {},
            [](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
              if (expr.kind == primec::Expr::Kind::Name && expr.name == "ptr") {
                return primec::ir_lowerer::LocalInfo::ValueKind::Float64;
              }
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            kindOut) == Resolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Float64);

  CHECK(primec::ir_lowerer::inferPointerBuiltinCallReturnKind(
            allocExpr,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            kindOut) == Resolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Int32);

  primec::Expr reallocExpr;
  reallocExpr.kind = primec::Expr::Kind::Call;
  reallocExpr.name = "/std/intrinsics/memory/realloc";
  primec::Expr reallocCount;
  reallocCount.kind = primec::Expr::Kind::Literal;
  reallocCount.literalValue = 4;
  reallocExpr.args = {pointerName, reallocCount};

  CHECK(primec::ir_lowerer::inferPointerBuiltinCallReturnKind(
            reallocExpr,
            {},
            [](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
              if (expr.kind == primec::Expr::Kind::Name && expr.name == "ptr") {
                return primec::ir_lowerer::LocalInfo::ValueKind::Float64;
              }
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            kindOut) == Resolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Float64);

  primec::Expr atExpr;
  atExpr.kind = primec::Expr::Kind::Call;
  atExpr.name = "/std/intrinsics/memory/at";
  primec::Expr atIndex;
  atIndex.kind = primec::Expr::Kind::Literal;
  atIndex.literalValue = 1;
  primec::Expr atCount;
  atCount.kind = primec::Expr::Kind::Literal;
  atCount.literalValue = 4;
  atExpr.args = {pointerName, atIndex, atCount};

  CHECK(primec::ir_lowerer::inferPointerBuiltinCallReturnKind(
            atExpr,
            {},
            [](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
              if (expr.kind == primec::Expr::Kind::Name && expr.name == "ptr") {
                return primec::ir_lowerer::LocalInfo::ValueKind::Float64;
              }
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            kindOut) == Resolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Float64);

  primec::Expr atUnsafeExpr;
  atUnsafeExpr.kind = primec::Expr::Kind::Call;
  atUnsafeExpr.name = "/std/intrinsics/memory/at_unsafe";
  atUnsafeExpr.args = {pointerName, atIndex};

  CHECK(primec::ir_lowerer::inferPointerBuiltinCallReturnKind(
            atUnsafeExpr,
            {},
            [](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
              if (expr.kind == primec::Expr::Kind::Name && expr.name == "ptr") {
                return primec::ir_lowerer::LocalInfo::ValueKind::Float64;
              }
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            kindOut) == Resolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Float64);
}

TEST_CASE("ir lowerer setup inference helper handles invalid pointer builtin calls") {
  using Resolution = primec::ir_lowerer::PointerBuiltinCallReturnKindResolution;

  primec::Expr nonPointerCall;
  nonPointerCall.kind = primec::Expr::Kind::Call;
  nonPointerCall.name = "plus";

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  CHECK(primec::ir_lowerer::inferPointerBuiltinCallReturnKind(
            nonPointerCall,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            kindOut) == Resolution::NotMatched);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);

  primec::Expr invalidDereference;
  invalidDereference.kind = primec::Expr::Kind::Call;
  invalidDereference.name = "dereference";
  invalidDereference.args = {};
  CHECK(primec::ir_lowerer::inferPointerBuiltinCallReturnKind(
            invalidDereference,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            kindOut) == Resolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
}

TEST_CASE("ir lowerer setup inference helper infers comparison/operator return kinds") {
  using Resolution = primec::ir_lowerer::ComparisonOperatorCallReturnKindResolution;

  primec::Expr x;
  x.kind = primec::Expr::Kind::Name;
  x.name = "x";
  primec::Expr y;
  y.kind = primec::Expr::Kind::Name;
  y.name = "y";

  primec::Expr comparisonExpr;
  comparisonExpr.kind = primec::Expr::Kind::Call;
  comparisonExpr.name = "equal";
  comparisonExpr.args = {x, y};

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(primec::ir_lowerer::inferComparisonOperatorCallReturnKind(
            comparisonExpr,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Int32;
            },
            [](primec::ir_lowerer::LocalInfo::ValueKind left, primec::ir_lowerer::LocalInfo::ValueKind) {
              return left;
            },
            kindOut) == Resolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Bool);

  primec::Expr plusExpr;
  plusExpr.kind = primec::Expr::Kind::Call;
  plusExpr.name = "plus";
  plusExpr.args = {x, y};
  CHECK(primec::ir_lowerer::inferComparisonOperatorCallReturnKind(
            plusExpr,
            {},
            [](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
              if (expr.kind == primec::Expr::Kind::Name && expr.name == "y") {
                return primec::ir_lowerer::LocalInfo::ValueKind::Int64;
              }
              return primec::ir_lowerer::LocalInfo::ValueKind::Int32;
            },
            [](primec::ir_lowerer::LocalInfo::ValueKind left, primec::ir_lowerer::LocalInfo::ValueKind right) {
              return (left == primec::ir_lowerer::LocalInfo::ValueKind::Int64 ||
                      right == primec::ir_lowerer::LocalInfo::ValueKind::Int64)
                         ? primec::ir_lowerer::LocalInfo::ValueKind::Int64
                         : primec::ir_lowerer::LocalInfo::ValueKind::Int32;
            },
            kindOut) == Resolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Int64);

  primec::Expr negateExpr;
  negateExpr.kind = primec::Expr::Kind::Call;
  negateExpr.name = "negate";
  negateExpr.args = {x};
  CHECK(primec::ir_lowerer::inferComparisonOperatorCallReturnKind(
            negateExpr,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Float32;
            },
            [](primec::ir_lowerer::LocalInfo::ValueKind left, primec::ir_lowerer::LocalInfo::ValueKind) {
              return left;
            },
            kindOut) == Resolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Float32);
}


TEST_SUITE_END();
