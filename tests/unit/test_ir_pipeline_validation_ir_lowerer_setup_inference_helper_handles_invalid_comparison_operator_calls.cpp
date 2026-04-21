#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir lowerer setup inference helper handles invalid comparison/operator calls") {
  using Resolution = primec::ir_lowerer::ComparisonOperatorCallReturnKindResolution;

  primec::Expr nonOperatorCall;
  nonOperatorCall.kind = primec::Expr::Kind::Call;
  nonOperatorCall.name = "clamp";

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  CHECK(primec::ir_lowerer::inferComparisonOperatorCallReturnKind(
            nonOperatorCall,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            [](primec::ir_lowerer::LocalInfo::ValueKind left, primec::ir_lowerer::LocalInfo::ValueKind) {
              return left;
            },
            kindOut) == Resolution::NotMatched);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);

  primec::Expr invalidNegate;
  invalidNegate.kind = primec::Expr::Kind::Call;
  invalidNegate.name = "negate";
  invalidNegate.args = {};
  CHECK(primec::ir_lowerer::inferComparisonOperatorCallReturnKind(
            invalidNegate,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            [](primec::ir_lowerer::LocalInfo::ValueKind left, primec::ir_lowerer::LocalInfo::ValueKind) {
              return left;
            },
            kindOut) == Resolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);

  primec::Expr invalidPlus;
  invalidPlus.kind = primec::Expr::Kind::Call;
  invalidPlus.name = "plus";
  invalidPlus.args = {nonOperatorCall};
  CHECK(primec::ir_lowerer::inferComparisonOperatorCallReturnKind(
            invalidPlus,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            [](primec::ir_lowerer::LocalInfo::ValueKind left, primec::ir_lowerer::LocalInfo::ValueKind) {
              return left;
            },
            kindOut) == Resolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
}

TEST_CASE("ir lowerer setup inference helper infers GPU/buffer call return kinds") {
  using Resolution = primec::ir_lowerer::GpuBufferCallReturnKindResolution;

  primec::Expr gpuExpr;
  gpuExpr.kind = primec::Expr::Kind::Call;
  gpuExpr.name = "global_id_x";

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(primec::ir_lowerer::inferGpuBufferCallReturnKind(
            gpuExpr,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            kindOut) == Resolution::NotMatched);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);

  primec::Expr bufferName;
  bufferName.kind = primec::Expr::Kind::Name;
  bufferName.name = "buf";
  primec::Expr index;
  index.kind = primec::Expr::Kind::Literal;
  index.literalValue = 0;
  primec::Expr bufferLoadExpr;
  bufferLoadExpr.kind = primec::Expr::Kind::Call;
  bufferLoadExpr.name = "buffer_load";
  bufferLoadExpr.args = {bufferName, index};
  CHECK(primec::ir_lowerer::inferGpuBufferCallReturnKind(
            bufferLoadExpr,
            {},
            [](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
              if (expr.kind == primec::Expr::Kind::Name && expr.name == "buf") {
                return primec::ir_lowerer::LocalInfo::ValueKind::Float32;
              }
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            kindOut) == Resolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Float32);
}

TEST_CASE("ir lowerer setup inference helper handles invalid GPU/buffer calls") {
  using Resolution = primec::ir_lowerer::GpuBufferCallReturnKindResolution;

  primec::Expr nonGpuBuffer;
  nonGpuBuffer.kind = primec::Expr::Kind::Call;
  nonGpuBuffer.name = "plus";

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  CHECK(primec::ir_lowerer::inferGpuBufferCallReturnKind(
            nonGpuBuffer,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            kindOut) == Resolution::NotMatched);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);

  primec::Expr bufferLoadExpr;
  bufferLoadExpr.kind = primec::Expr::Kind::Call;
  bufferLoadExpr.name = "buffer_load";
  bufferLoadExpr.args = {};
  CHECK(primec::ir_lowerer::inferGpuBufferCallReturnKind(
            bufferLoadExpr,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
            },
            kindOut) == Resolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);

  primec::Expr bufferName;
  bufferName.kind = primec::Expr::Kind::Name;
  bufferName.name = "buf";
  primec::Expr index;
  index.kind = primec::Expr::Kind::Literal;
  index.literalValue = 0;
  bufferLoadExpr.args = {bufferName, index};
  CHECK(primec::ir_lowerer::inferGpuBufferCallReturnKind(
            bufferLoadExpr,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return primec::ir_lowerer::LocalInfo::ValueKind::String;
            },
            kindOut) == Resolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
}

TEST_CASE("ir lowerer setup inference helper infers count-capacity call return kinds") {
  using Resolution = primec::ir_lowerer::CountCapacityCallReturnKindResolution;

  primec::Expr arrayCountExpr;
  arrayCountExpr.kind = primec::Expr::Kind::Call;
  arrayCountExpr.name = "count";
  arrayCountExpr.isMethodCall = true;

  primec::Expr stringCountExpr;
  stringCountExpr.kind = primec::Expr::Kind::Call;
  stringCountExpr.name = "count";
  stringCountExpr.isMethodCall = true;

  primec::Expr vectorCapacityExpr;
  vectorCapacityExpr.kind = primec::Expr::Kind::Call;
  vectorCapacityExpr.name = "capacity";
  vectorCapacityExpr.isMethodCall = true;

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  CHECK(primec::ir_lowerer::inferCountCapacityCallReturnKind(
            arrayCountExpr,
            {},
            [](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
              return expr.name == "count";
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return false;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return false;
            },
            kindOut) == Resolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Int32);

  CHECK(primec::ir_lowerer::inferCountCapacityCallReturnKind(
            stringCountExpr,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return false;
            },
            [](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
              return expr.name == "count";
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return false;
            },
            kindOut) == Resolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Int32);

  CHECK(primec::ir_lowerer::inferCountCapacityCallReturnKind(
            vectorCapacityExpr,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return false;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return false;
            },
            [](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
              return expr.name == "capacity";
            },
            kindOut) == Resolution::Resolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
}

TEST_CASE("ir lowerer setup inference helper handles non count-capacity calls") {
  using Resolution = primec::ir_lowerer::CountCapacityCallReturnKindResolution;

  primec::Expr nonCountExpr;
  nonCountExpr.kind = primec::Expr::Kind::Call;
  nonCountExpr.name = "plus";

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Float64;
  CHECK(primec::ir_lowerer::inferCountCapacityCallReturnKind(
            nonCountExpr,
            {},
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return false;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return false;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return false;
            },
            kindOut) == Resolution::NotMatched);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
}

TEST_CASE("ir lowerer statement binding helper infers vector kind from initializer call") {
  primec::Expr stmt;
  stmt.name = "values";

  primec::Expr init;
  init.kind = primec::Expr::Kind::Call;
  init.name = "vector";
  init.templateArgs = {"i64"};

  const primec::ir_lowerer::StatementBindingTypeInfo info = primec::ir_lowerer::inferStatementBindingTypeInfo(
      stmt,
      init,
      {},
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::Kind::Value; },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo::Kind) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      });

  CHECK(info.kind == primec::ir_lowerer::LocalInfo::Kind::Vector);
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int64);
}

TEST_CASE("ir lowerer statement binding helper recovers bool kind from builtin comparison initializer") {
  primec::Expr stmt;
  stmt.name = "shapeOk";

  primec::Expr lhs;
  lhs.kind = primec::Expr::Kind::Literal;
  lhs.intWidth = 32;
  lhs.literalValue = 1;

  primec::Expr rhs = lhs;
  rhs.literalValue = 0;

  primec::Expr init;
  init.kind = primec::Expr::Kind::Call;
  init.name = "greater_than";
  init.args = {lhs, rhs};

  const primec::ir_lowerer::StatementBindingTypeInfo info = primec::ir_lowerer::inferStatementBindingTypeInfo(
      stmt,
      init,
      {},
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::Kind::Value; },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo::Kind) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      });

  CHECK(info.kind == primec::ir_lowerer::LocalInfo::Kind::Value);
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Bool);
}

TEST_CASE("ir lowerer statement binding helper prefers semantic temporary facts") {
  primec::SemanticProgram semanticProgram;
  semanticProgram.bindingFacts.push_back(primec::SemanticProgramBindingFact{
      .scopePath = "/main",
      .siteKind = "temporary",
      .name = "makeVec",
      .bindingTypeText = "vector<i64>",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 14,
      .sourceColumn = 5,
      .semanticNodeId = 111,
      .resolvedPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/makeVec"),
  });
  const auto semanticTargets =
      primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);

  primec::Expr stmt;
  stmt.name = "values";

  primec::Expr init;
  init.kind = primec::Expr::Kind::Call;
  init.name = "makeVec";
  init.semanticNodeId = 111;
  primec::Transform staleTransform;
  staleTransform.name = "map";
  staleTransform.templateArgs = {"i32", "i32"};
  init.transforms.push_back(staleTransform);

  const primec::ir_lowerer::StatementBindingTypeInfo info = primec::ir_lowerer::inferStatementBindingTypeInfo(
      stmt,
      init,
      {},
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::Kind::Value; },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo::Kind) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      {},
      &semanticTargets);

  CHECK(info.kind == primec::ir_lowerer::LocalInfo::Kind::Vector);
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int64);
}

TEST_CASE("ir lowerer statement binding helper infers pointer kind from alloc initializer call") {
  primec::Expr stmt;
  stmt.name = "ptr";

  primec::Expr init;
  init.kind = primec::Expr::Kind::Call;
  init.name = "/std/intrinsics/memory/alloc";
  init.templateArgs = {"i32"};
  primec::Expr countExpr;
  countExpr.kind = primec::Expr::Kind::Literal;
  countExpr.literalValue = 4;
  init.args = {countExpr};

  const primec::ir_lowerer::StatementBindingTypeInfo info = primec::ir_lowerer::inferStatementBindingTypeInfo(
      stmt,
      init,
      {},
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::Kind::Value; },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo::Kind) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
        if (expr.kind == primec::Expr::Kind::Call && expr.name == "/std/intrinsics/memory/alloc") {
          return primec::ir_lowerer::LocalInfo::ValueKind::Int32;
        }
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      });

  CHECK(info.kind == primec::ir_lowerer::LocalInfo::Kind::Pointer);
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
}

TEST_CASE("ir lowerer statement binding helper infers pointer kind from realloc initializer call") {
  primec::Expr stmt;
  stmt.name = "grown";

  primec::Expr pointerExpr;
  pointerExpr.kind = primec::Expr::Kind::Name;
  pointerExpr.name = "ptr";

  primec::Expr init;
  init.kind = primec::Expr::Kind::Call;
  init.name = "/std/intrinsics/memory/realloc";
  primec::Expr countExpr;
  countExpr.kind = primec::Expr::Kind::Literal;
  countExpr.literalValue = 8;
  init.args = {pointerExpr, countExpr};

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo sourceInfo;
  sourceInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Pointer;
  sourceInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Float64;
  sourceInfo.structTypeName = "/pkg/Pair";
  locals.emplace("ptr", sourceInfo);

  const primec::ir_lowerer::StatementBindingTypeInfo info = primec::ir_lowerer::inferStatementBindingTypeInfo(
      stmt,
      init,
      locals,
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::Kind::Value; },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo::Kind) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &localsIn) {
        if (expr.kind == primec::Expr::Kind::Name && expr.name == "ptr") {
          auto it = localsIn.find("ptr");
          return it == localsIn.end() ? primec::ir_lowerer::LocalInfo::ValueKind::Unknown
                                      : it->second.valueKind;
        }
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      });

  CHECK(info.kind == primec::ir_lowerer::LocalInfo::Kind::Pointer);
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Float64);
  CHECK(info.structTypeName == "/pkg/Pair");
}

TEST_CASE("ir lowerer statement binding helper infers pointer kind from checked memory at initializer call") {
  primec::Expr stmt;
  stmt.name = "second";

  primec::Expr pointerExpr;
  pointerExpr.kind = primec::Expr::Kind::Name;
  pointerExpr.name = "ptr";

  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Literal;
  indexExpr.literalValue = 1;

  primec::Expr countExpr;
  countExpr.kind = primec::Expr::Kind::Literal;
  countExpr.literalValue = 2;

  primec::Expr init;
  init.kind = primec::Expr::Kind::Call;
  init.name = "/std/intrinsics/memory/at";
  init.args = {pointerExpr, indexExpr, countExpr};

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo sourceInfo;
  sourceInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Pointer;
  sourceInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Float64;
  sourceInfo.structTypeName = "/pkg/Pair";
  locals.emplace("ptr", sourceInfo);

  const primec::ir_lowerer::StatementBindingTypeInfo info = primec::ir_lowerer::inferStatementBindingTypeInfo(
      stmt,
      init,
      locals,
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::Kind::Value; },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo::Kind) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &localsIn) {
        if (expr.kind == primec::Expr::Kind::Name && expr.name == "ptr") {
          auto it = localsIn.find("ptr");
          return it == localsIn.end() ? primec::ir_lowerer::LocalInfo::ValueKind::Unknown
                                      : it->second.valueKind;
        }
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      });

  CHECK(info.kind == primec::ir_lowerer::LocalInfo::Kind::Pointer);
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Float64);
  CHECK(info.structTypeName == "/pkg/Pair");
}

TEST_CASE("ir lowerer statement binding helper infers pointer kind from unchecked memory at initializer call") {
  primec::Expr stmt;
  stmt.name = "second";

  primec::Expr pointerExpr;
  pointerExpr.kind = primec::Expr::Kind::Name;
  pointerExpr.name = "ptr";

  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Literal;
  indexExpr.literalValue = 1;

  primec::Expr init;
  init.kind = primec::Expr::Kind::Call;
  init.name = "/std/intrinsics/memory/at_unsafe";
  init.args = {pointerExpr, indexExpr};

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo sourceInfo;
  sourceInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Pointer;
  sourceInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Float64;
  sourceInfo.structTypeName = "/pkg/Pair";
  locals.emplace("ptr", sourceInfo);

  const primec::ir_lowerer::StatementBindingTypeInfo info = primec::ir_lowerer::inferStatementBindingTypeInfo(
      stmt,
      init,
      locals,
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::Kind::Value; },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo::Kind) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &localsIn) {
        if (expr.kind == primec::Expr::Kind::Name && expr.name == "ptr") {
          auto it = localsIn.find("ptr");
          return it == localsIn.end() ? primec::ir_lowerer::LocalInfo::ValueKind::Unknown
                                      : it->second.valueKind;
        }
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      });

  CHECK(info.kind == primec::ir_lowerer::LocalInfo::Kind::Pointer);
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Float64);
  CHECK(info.structTypeName == "/pkg/Pair");
}

TEST_CASE("ir lowerer statement binding helper infers canonical map constructor metadata") {
  primec::Expr stmt;
  stmt.name = "value";

  primec::Expr init;
  init.kind = primec::Expr::Kind::Call;
  init.name = "/std/collections/map/map";
  init.templateArgs = {"i32", "i32"};

  const primec::ir_lowerer::StatementBindingTypeInfo info = primec::ir_lowerer::inferStatementBindingTypeInfo(
      stmt,
      init,
      {},
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::Kind::Value; },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo::Kind) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
        if (expr.kind == primec::Expr::Kind::Call && expr.name == "/std/collections/map/map") {
          return primec::ir_lowerer::LocalInfo::ValueKind::Int32;
        }
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      });

  CHECK(info.kind == primec::ir_lowerer::LocalInfo::Kind::Map);
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  CHECK(info.mapKeyKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  CHECK(info.mapValueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
}

TEST_CASE("ir lowerer statement binding helper treats scoped Buffer ctor as raw struct init") {
  primec::Expr stmt;
  stmt.name = "value";
  primec::Transform typeTransform;
  typeTransform.name = "Buffer";
  typeTransform.templateArgs = {"i32"};
  stmt.transforms.push_back(typeTransform);

  primec::Expr init;
  init.kind = primec::Expr::Kind::Call;
  init.name = "Buffer";
  init.namespacePrefix = "/std/gfx";
  init.templateArgs = {"i32"};

  const primec::ir_lowerer::StatementBindingTypeInfo info = primec::ir_lowerer::inferStatementBindingTypeInfo(
      stmt,
      init,
      {},
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &expr) { return primec::ir_lowerer::bindingKindFromTransforms(expr); },
      [](const primec::Expr &expr, primec::ir_lowerer::LocalInfo::Kind kind) {
        return primec::ir_lowerer::bindingValueKindFromTransforms(expr, kind);
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      });

  CHECK(info.kind == primec::ir_lowerer::LocalInfo::Kind::Value);
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
}

TEST_CASE("ir lowerer statement binding helper inherits map metadata from named source binding") {
  primec::Expr stmt;
  stmt.name = "dst";

  primec::Expr init;
  init.kind = primec::Expr::Kind::Name;
  init.name = "srcMap";

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo sourceInfo;
  sourceInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Map;
  sourceInfo.mapKeyKind = primec::ir_lowerer::LocalInfo::ValueKind::Bool;
  sourceInfo.mapValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Float64;
  locals.emplace("srcMap", sourceInfo);

  const primec::ir_lowerer::StatementBindingTypeInfo info = primec::ir_lowerer::inferStatementBindingTypeInfo(
      stmt,
      init,
      locals,
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::Kind::Value; },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo::Kind) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      });

  CHECK(info.kind == primec::ir_lowerer::LocalInfo::Kind::Map);
  CHECK(info.mapKeyKind == primec::ir_lowerer::LocalInfo::ValueKind::Bool);
  CHECK(info.mapValueKind == primec::ir_lowerer::LocalInfo::ValueKind::Float64);
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Float64);
}

TEST_CASE("ir lowerer statement binding helper infers call parameter local info") {
  primec::Expr literalInit;
  literalInit.kind = primec::Expr::Kind::Literal;
  literalInit.literalValue = 7;

  primec::Expr param;
  param.name = "value";
  param.args = {literalInit};

  primec::ir_lowerer::LocalInfo info;
  info.index = 12;
  std::string error;
  REQUIRE(primec::ir_lowerer::inferCallParameterLocalInfo(
      param,
      {},
      [](const primec::Expr &) { return true; },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::Kind::Value; },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo::Kind) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
        if (expr.kind == primec::Expr::Kind::Literal) {
          return primec::ir_lowerer::LocalInfo::ValueKind::Int64;
        }
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &) { return false; },
      info,
      error));
  CHECK(error.empty());
  CHECK(info.index == 12);
  CHECK(info.isMutable);
  CHECK(info.kind == primec::ir_lowerer::LocalInfo::Kind::Value);
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int64);
}

TEST_CASE("ir lowerer statement binding helper recovers bool default parameter kind from builtin comparison") {
  primec::Expr lhs;
  lhs.kind = primec::Expr::Kind::Literal;
  lhs.intWidth = 32;
  lhs.literalValue = 1;

  primec::Expr rhs = lhs;
  rhs.literalValue = 0;

  primec::Expr defaultInit;
  defaultInit.kind = primec::Expr::Kind::Call;
  defaultInit.name = "equal";
  defaultInit.args = {lhs, rhs};

  primec::Expr param;
  param.name = "shapeOk";
  param.args = {defaultInit};

  primec::ir_lowerer::LocalInfo info;
  info.index = 21;
  std::string error;
  REQUIRE(primec::ir_lowerer::inferCallParameterLocalInfo(
      param,
      {},
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::Kind::Value; },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo::Kind) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &) { return false; },
      info,
      error));
  CHECK(error.empty());
  CHECK(info.kind == primec::ir_lowerer::LocalInfo::Kind::Value);
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Bool);
}

TEST_CASE("ir lowerer statement binding helper sets string parameter defaults") {
  primec::Expr literalInit;
  literalInit.kind = primec::Expr::Kind::Literal;
  literalInit.literalValue = 0;

  primec::Expr param;
  param.name = "label";
  param.args = {literalInit};

  primec::ir_lowerer::LocalInfo info;
  info.index = 3;
  std::string error;
  REQUIRE(primec::ir_lowerer::inferCallParameterLocalInfo(
      param,
      {},
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::Kind::Value; },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo::Kind) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &) { return true; },
      info,
      error));
  CHECK(error.empty());
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::String);
  CHECK(info.stringSource == primec::ir_lowerer::LocalInfo::StringSource::RuntimeIndex);
  CHECK(info.stringIndex == -1);
  CHECK(info.argvChecked);
}

TEST_CASE("ir lowerer statement binding helper classifies variadic Result parameters") {
  primec::Expr param;
  param.name = "values";
  primec::Transform argsTransform;
  argsTransform.name = "args";
  argsTransform.templateArgs.push_back("Result<i32, ParseError>");
  param.transforms.push_back(argsTransform);

  primec::ir_lowerer::LocalInfo info;
  info.index = 9;
  std::string error;
  REQUIRE(primec::ir_lowerer::inferCallParameterLocalInfo(
      param,
      {},
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &) { return true; },
      [](const primec::Expr &expr) { return primec::ir_lowerer::bindingKindFromTransforms(expr); },
      [](const primec::Expr &expr, primec::ir_lowerer::LocalInfo::Kind kind) {
        return primec::ir_lowerer::bindingValueKindFromTransforms(expr, kind);
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &) { return false; },
      info,
      error));
  CHECK(error.empty());
  CHECK(info.kind == primec::ir_lowerer::LocalInfo::Kind::Array);
  CHECK(info.isArgsPack);
  CHECK(info.isResult);
  CHECK(info.resultHasValue);
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int64);
  CHECK(info.resultValueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  CHECK(info.resultErrorType == "ParseError");
}

TEST_CASE("ir lowerer statement binding helper uses semantic query facts for default Result params") {
  primec::SemanticProgram semanticProgram;
  semanticProgram.queryFacts.push_back(primec::SemanticProgramQueryFact{
      .scopePath = "/main",
      .callName = "loadValue",
      .queryTypeText = "Result<i64, ParseError>",
      .bindingTypeText = "Result<i64, ParseError>",
      .hasResultType = true,
      .resultTypeHasValue = true,
      .resultValueType = "i64",
      .resultErrorType = "ParseError",
      .sourceLine = 27,
      .sourceColumn = 9,
      .semanticNodeId = 313,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/loadValue"),
  });
  const auto semanticTargets =
      primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);

  primec::Expr defaultInit;
  defaultInit.kind = primec::Expr::Kind::Call;
  defaultInit.name = "loadValue";
  defaultInit.semanticNodeId = 313;
  primec::Transform staleTransform;
  staleTransform.name = "vector";
  staleTransform.templateArgs = {"i32"};
  defaultInit.transforms.push_back(staleTransform);

  primec::Expr param;
  param.name = "value";
  param.args = {defaultInit};

  primec::ir_lowerer::LocalInfo info;
  info.index = 4;
  std::string error;
  REQUIRE(primec::ir_lowerer::inferCallParameterLocalInfo(
      param,
      {},
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::Kind::Value; },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo::Kind) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &) { return false; },
      info,
      error,
      {},
      {},
      {},
      &semanticTargets));
  CHECK(error.empty());
  CHECK(info.isResult);
  CHECK(info.resultHasValue);
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int64);
  CHECK(info.resultValueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int64);
  CHECK(info.resultErrorType == "ParseError");
}

TEST_SUITE_END();
