#include "test_ir_pipeline_validation_helpers.h"

#include <algorithm>

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir lowerer conversions helper rejects immutable assign target") {
  primec::Expr target;
  target.kind = primec::Expr::Kind::Name;
  target.name = "x";
  primec::Expr value;
  value.kind = primec::Expr::Kind::Literal;
  value.literalValue = 3;
  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.name = "assign";
  expr.args = {target, value};

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo info;
  info.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  info.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  info.index = 0;
  info.isMutable = false;
  locals.emplace("x", info);

  std::vector<primec::IrInstruction> instructions;
  std::string error;
  bool handled = false;
  int32_t nextLocal = 0;
  const bool ok = primec::ir_lowerer::emitConversionsAndCallsOperatorExpr(
      expr,
      locals,
      nextLocal,
      [&](const primec::Expr &valueExpr, const primec::ir_lowerer::LocalMap &) {
        instructions.push_back({primec::IrOpcode::PushI32, static_cast<uint64_t>(valueExpr.literalValue)});
        return true;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Int32;
      },
      [](primec::ir_lowerer::LocalInfo::ValueKind, bool) { return true; },
      [&]() { return nextLocal++; },
      []() {},
      []() {},
      []() {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return false; },
      [](const std::string &) { return primec::ir_lowerer::LocalInfo::ValueKind::Unknown; },
      [](const std::string &, std::string &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string(); },
      [](const std::string &, const std::string &, std::string &) { return false; },
      [](const std::string &, int32_t &) { return false; },
      [](const std::string &, const std::string &, int32_t &, int32_t &, std::string &) { return false; },
      [](int32_t, int32_t, int32_t) { return false; },
      instructions,
      handled,
      error);

  CHECK_FALSE(ok);
  CHECK(handled);
  CHECK(error == "assign target must be mutable: x");
}

TEST_CASE("ir lowerer conversions helper uses semantic mutation target facts before stale locals") {
  using Kind = primec::ir_lowerer::LocalInfo::ValueKind;

  primec::SemanticProgram semanticProgram;
  auto intern = [](primec::SemanticProgram &program, const std::string &text) {
    return primec::semanticProgramInternCallTargetString(program, text);
  };
  auto addBindingFact = [&](uint64_t semanticNodeId,
                            const std::string &name,
                            const std::string &typeText) {
    primec::SemanticProgramBindingFact fact;
    fact.scopePath = "/main";
    fact.siteKind = "local";
    fact.name = name;
    fact.bindingTypeText = typeText;
    fact.semanticNodeId = semanticNodeId;
    fact.scopePathId = intern(semanticProgram, "/main");
    fact.siteKindId = intern(semanticProgram, "local");
    fact.nameId = intern(semanticProgram, name);
    fact.bindingTypeTextId = intern(semanticProgram, typeText);
    const size_t index = semanticProgram.bindingFacts.size();
    semanticProgram.bindingFacts.push_back(fact);
    semanticProgram.publishedRoutingLookups.bindingFactIndicesByExpr
        .insert_or_assign(semanticNodeId, index);
  };
  auto addLocalAutoFact = [&](uint64_t semanticNodeId,
                              const std::string &name,
                              const std::string &typeText) {
    primec::SemanticProgramLocalAutoFact fact;
    fact.scopePath = "/main";
    fact.bindingName = name;
    fact.bindingTypeText = typeText;
    fact.initializerBindingTypeText = typeText;
    fact.semanticNodeId = semanticNodeId;
    fact.scopePathId = intern(semanticProgram, "/main");
    fact.bindingNameId = intern(semanticProgram, name);
    fact.bindingTypeTextId = intern(semanticProgram, typeText);
    const size_t index = semanticProgram.localAutoFacts.size();
    semanticProgram.localAutoFacts.push_back(fact);
    semanticProgram.publishedRoutingLookups.localAutoFactIndicesByExpr
        .insert_or_assign(semanticNodeId, index);
  };
  auto addQueryFact = [&](uint64_t semanticNodeId,
                          const std::string &name,
                          const std::string &typeText) {
    primec::SemanticProgramQueryFact fact;
    fact.scopePath = "/main";
    fact.callName = name;
    fact.queryTypeText = typeText;
    fact.bindingTypeText = typeText;
    fact.semanticNodeId = semanticNodeId;
    fact.scopePathId = intern(semanticProgram, "/main");
    fact.callNameId = intern(semanticProgram, name);
    fact.resolvedPathId = intern(semanticProgram, "/main/" + name);
    fact.queryTypeTextId = intern(semanticProgram, typeText);
    fact.bindingTypeTextId = intern(semanticProgram, typeText);
    const size_t index = semanticProgram.queryFacts.size();
    semanticProgram.queryFacts.push_back(fact);
    semanticProgram.publishedRoutingLookups.queryFactIndicesByExpr
        .insert_or_assign(semanticNodeId, index);
  };

  addBindingFact(9201, "staleScalar", "string");
  addLocalAutoFact(9202, "semanticScalar", "f32");
  addQueryFact(9203, "semanticRef", "Reference<f64>");
  addBindingFact(9204, "semanticPtr", "Pointer<f64>");
  const auto semanticTargets =
      primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);

  primec::ir_lowerer::LocalMap locals;
  auto addLocal = [&](const std::string &name,
                      primec::ir_lowerer::LocalInfo::Kind kind,
                      Kind valueKind,
                      int32_t index) {
    primec::ir_lowerer::LocalInfo info;
    info.kind = kind;
    info.valueKind = valueKind;
    info.index = index;
    info.isMutable = true;
    locals.emplace(name, info);
  };
  addLocal("staleScalar", primec::ir_lowerer::LocalInfo::Kind::Value, Kind::Int32, 3);
  addLocal("semanticScalar", primec::ir_lowerer::LocalInfo::Kind::Value, Kind::Int32, 4);
  addLocal("semanticRef", primec::ir_lowerer::LocalInfo::Kind::Reference, Kind::Int32, 5);
  addLocal("semanticPtr", primec::ir_lowerer::LocalInfo::Kind::Pointer, Kind::String, 6);

  auto makeMutation = [](const std::string &callName,
                         const std::string &targetName,
                         uint64_t semanticNodeId) {
    primec::Expr target;
    target.kind = primec::Expr::Kind::Name;
    target.name = targetName;
    target.semanticNodeId = semanticNodeId;
    primec::Expr expr;
    expr.kind = primec::Expr::Kind::Call;
    expr.name = callName;
    expr.args = {target};
    return expr;
  };
  auto makeDereferenceMutation = [](const std::string &callName,
                                    const std::string &targetName,
                                    uint64_t semanticNodeId) {
    primec::Expr target;
    target.kind = primec::Expr::Kind::Name;
    target.name = targetName;
    target.semanticNodeId = semanticNodeId;
    primec::Expr dereference;
    dereference.kind = primec::Expr::Kind::Call;
    dereference.name = "dereference";
    dereference.args = {target};
    primec::Expr expr;
    expr.kind = primec::Expr::Kind::Call;
    expr.name = callName;
    expr.args = {dereference};
    return expr;
  };

  auto emitMutation = [&](const primec::Expr &expr,
                          std::vector<primec::IrInstruction> &instructions,
                          std::string &error) {
    bool handled = false;
    int32_t nextLocal = 20;
    return primec::ir_lowerer::emitConversionsAndCallsOperatorExpr(
        expr,
        locals,
        nextLocal,
        [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
        [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
          return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
        },
        [](primec::ir_lowerer::LocalInfo::ValueKind, bool) { return true; },
        [&]() { return nextLocal++; },
        []() {},
        []() {},
        []() {},
        [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) {
          return false;
        },
        [](const std::string &typeName) {
          return primec::ir_lowerer::valueKindFromTypeName(typeName);
        },
        [](const std::string &, std::string &) { return false; },
        [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string(); },
        [](const std::string &, const std::string &, std::string &) { return false; },
        [](const std::string &, int32_t &) { return false; },
        [](const std::string &, const std::string &, int32_t &, int32_t &, std::string &) {
          return false;
        },
        [](const std::string &, const std::string &, primec::ir_lowerer::LayoutFieldBinding &) {
          return false;
        },
        [](int32_t, int32_t, int32_t) { return false; },
        instructions,
        handled,
        error,
        [](const primec::Expr &) { return static_cast<const primec::Definition *>(nullptr); },
        &semanticTargets);
  };

  std::vector<primec::IrInstruction> instructions;
  std::string error;
  CHECK_FALSE(emitMutation(makeMutation("increment", "staleScalar", 9201), instructions, error));
  CHECK(error == "increment requires numeric operand");
  CHECK(instructions.empty());

  error.clear();
  instructions.clear();
  CHECK(emitMutation(makeMutation("increment", "semanticScalar", 9202), instructions, error));
  CHECK(error.empty());
  REQUIRE(instructions.size() == 5);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 4u);
  CHECK(instructions[1].op == primec::IrOpcode::PushF32);
  CHECK(instructions[2].op == primec::IrOpcode::AddF32);
  CHECK(instructions[4].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[4].imm == 4u);

  error.clear();
  instructions.clear();
  CHECK(emitMutation(makeMutation("decrement", "semanticRef", 9203), instructions, error));
  CHECK(error.empty());
  REQUIRE(instructions.size() == 10);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 5u);
  CHECK(instructions[4].op == primec::IrOpcode::PushF64);
  CHECK(instructions[5].op == primec::IrOpcode::SubF64);
  CHECK(instructions.back().op == primec::IrOpcode::StoreIndirect);

  error.clear();
  instructions.clear();
  CHECK(emitMutation(makeDereferenceMutation("increment", "semanticPtr", 9204), instructions, error));
  CHECK(error.empty());
  REQUIRE(instructions.size() == 10);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 6u);
  CHECK(instructions[4].op == primec::IrOpcode::PushF64);
  CHECK(instructions[5].op == primec::IrOpcode::AddF64);
  CHECK(instructions.back().op == primec::IrOpcode::StoreIndirect);
}

TEST_CASE("ir lowerer indexed assign consumes semantic collection facts before stale locals") {
  primec::SemanticProgram semanticProgram;
  auto intern = [](primec::SemanticProgram &program, const std::string &text) {
    return primec::semanticProgramInternCallTargetString(program, text);
  };
  auto addBindingFact = [&](uint64_t semanticNodeId,
                            const std::string &name,
                            const std::string &typeText) {
    primec::SemanticProgramBindingFact fact;
    fact.scopePath = "/main";
    fact.siteKind = "local";
    fact.name = name;
    fact.bindingTypeText = typeText;
    fact.semanticNodeId = semanticNodeId;
    fact.scopePathId = intern(semanticProgram, "/main");
    fact.siteKindId = intern(semanticProgram, "local");
    fact.nameId = intern(semanticProgram, name);
    fact.bindingTypeTextId = intern(semanticProgram, typeText);
    const size_t index = semanticProgram.bindingFacts.size();
    semanticProgram.bindingFacts.push_back(fact);
    semanticProgram.publishedRoutingLookups.bindingFactIndicesByExpr
        .insert_or_assign(semanticNodeId, index);
  };
  addBindingFact(9301, "values", "vector<i32>");
  addBindingFact(9302, "staleVector", "i32");
  const auto semanticTargets =
      primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);

  auto makeIndexedAssign = [](const std::string &targetName,
                              uint64_t semanticNodeId,
                              const std::string &accessName = "at") {
    primec::Expr target;
    target.kind = primec::Expr::Kind::Name;
    target.name = targetName;
    target.semanticNodeId = semanticNodeId;

    primec::Expr index;
    index.kind = primec::Expr::Kind::Literal;
    index.literalValue = 0;

    primec::Expr access;
    access.kind = primec::Expr::Kind::Call;
    access.name = accessName;
    access.args = {target, index};

    primec::Expr value;
    value.kind = primec::Expr::Kind::Literal;
    value.literalValue = 9;

    primec::Expr expr;
    expr.kind = primec::Expr::Kind::Call;
    expr.name = "assign";
    expr.args = {access, value};
    return expr;
  };

  primec::ir_lowerer::LocalInfo staleScalar;
  staleScalar.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  staleScalar.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::String;
  staleScalar.index = 3;
  staleScalar.isMutable = true;

  primec::ir_lowerer::LocalInfo staleVector;
  staleVector.kind = primec::ir_lowerer::LocalInfo::Kind::Vector;
  staleVector.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::String;
  staleVector.index = 4;
  staleVector.isMutable = true;

  const primec::ir_lowerer::LocalMap locals{
      {"values", staleScalar},
      {"staleVector", staleVector},
  };

  auto emitIndexedAssign = [&](const primec::Expr &expr,
                               std::vector<primec::IrInstruction> &instructions,
                               std::string &error) {
    bool handled = false;
    int32_t nextLocal = 20;
    return primec::ir_lowerer::emitConversionsAndCallsOperatorExpr(
        expr,
        locals,
        nextLocal,
        [&](const primec::Expr &valueExpr, const primec::ir_lowerer::LocalMap &) {
          if (valueExpr.kind == primec::Expr::Kind::Name) {
            instructions.push_back({primec::IrOpcode::LoadLocal,
                                    valueExpr.name == "staleVector" ? 4u : 3u});
            return true;
          }
          if (valueExpr.kind == primec::Expr::Kind::Literal) {
            instructions.push_back(
                {primec::IrOpcode::PushI32, static_cast<uint64_t>(valueExpr.literalValue)});
            return true;
          }
          return false;
        },
        [](const primec::Expr &valueExpr, const primec::ir_lowerer::LocalMap &) {
          return valueExpr.kind == primec::Expr::Kind::Literal
                     ? primec::ir_lowerer::LocalInfo::ValueKind::Int32
                     : primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
        },
        [](primec::ir_lowerer::LocalInfo::ValueKind, bool) { return true; },
        [&]() { return nextLocal++; },
        []() {},
        []() {},
        []() {},
        [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) {
          return false;
        },
        [](const std::string &typeName) {
          return primec::ir_lowerer::valueKindFromTypeName(typeName);
        },
        [](const std::string &, std::string &) { return false; },
        [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string(); },
        [](const std::string &, const std::string &, std::string &) { return false; },
        [](const std::string &, int32_t &) { return false; },
        [](const std::string &, const std::string &, int32_t &, int32_t &, std::string &) {
          return false;
        },
        [](const std::string &, const std::string &, primec::ir_lowerer::LayoutFieldBinding &) {
          return false;
        },
        [](int32_t, int32_t, int32_t) { return false; },
        instructions,
        handled,
        error,
        [](const primec::Expr &) { return static_cast<const primec::Definition *>(nullptr); },
        &semanticTargets);
  };

  std::vector<primec::IrInstruction> instructions;
  std::string error;
  const primec::Expr bareAtAssign = makeIndexedAssign("values", 9301);
  CHECK(emitIndexedAssign(bareAtAssign, instructions, error));
  CHECK(error.empty());
  CHECK(std::any_of(instructions.begin(), instructions.end(), [](const primec::IrInstruction &inst) {
    return inst.op == primec::IrOpcode::StoreIndirect;
  }));

  instructions.clear();
  error.clear();
  CHECK(emitIndexedAssign(
      makeIndexedAssign("values", 9301, "/std/collections/vector/at__ti32"),
      instructions,
      error));
  CHECK(error.empty());
  CHECK(std::any_of(instructions.begin(), instructions.end(), [](const primec::IrInstruction &inst) {
    return inst.op == primec::IrOpcode::StoreIndirect;
  }));

  instructions.clear();
  error.clear();
  CHECK_FALSE(emitIndexedAssign(makeIndexedAssign("staleVector", 9302), instructions, error));
  CHECK(error == "native backend only supports assign to array/vector elements");
  CHECK(instructions.empty());
}

TEST_CASE("ir lowerer conversions helper assigns compatible internal soa storage aliases") {
  primec::Expr target;
  target.kind = primec::Expr::Kind::Name;
  target.name = "storage";
  primec::Expr value;
  value.kind = primec::Expr::Kind::Name;
  value.name = "nextStorage";
  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.name = "assign";
  expr.args = {target, value};

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo storageInfo;
  storageInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  storageInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  storageInfo.structTypeName = "/std/collections/soa_storage/SoaColumn__ti32";
  storageInfo.index = 7;
  storageInfo.isMutable = true;
  locals.emplace("storage", storageInfo);

  std::vector<primec::IrInstruction> instructions;
  std::string error;
  bool handled = false;
  int32_t nextLocal = 20;
  int copyCalls = 0;
  int32_t copiedDest = -1;
  int32_t copiedSrc = -1;
  int32_t copiedCount = -1;
  const bool ok = primec::ir_lowerer::emitConversionsAndCallsOperatorExpr(
      expr,
      locals,
      nextLocal,
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        instructions.push_back({primec::IrOpcode::PushI64, 99});
        return true;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](primec::ir_lowerer::LocalInfo::ValueKind, bool) { return true; },
      [&]() { return nextLocal++; },
      []() {},
      []() {},
      []() {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) {
        return false;
      },
      [](const std::string &) { return primec::ir_lowerer::LocalInfo::ValueKind::Unknown; },
      [](const std::string &, std::string &) { return false; },
      [](const primec::Expr &candidate, const primec::ir_lowerer::LocalMap &) {
        if (candidate.name == "nextStorage") {
          return std::string("SoaColumn<i32>");
        }
        return std::string();
      },
      [](const std::string &, const std::string &, std::string &) { return false; },
      [](const std::string &structPath, int32_t &slotCount) {
        if (structPath == "/std/collections/soa_storage/SoaColumn__ti32") {
          slotCount = 3;
          return true;
        }
        return false;
      },
      [](const std::string &, const std::string &, int32_t &, int32_t &, std::string &) {
        return false;
      },
      [&](int32_t destPtrLocal, int32_t srcPtrLocal, int32_t slotCount) {
        ++copyCalls;
        copiedDest = destPtrLocal;
        copiedSrc = srcPtrLocal;
        copiedCount = slotCount;
        return true;
      },
      instructions,
      handled,
      error);

  CHECK(ok);
  CHECK(handled);
  CHECK(error.empty());
  CHECK(copyCalls == 1);
  CHECK(copiedDest == 20);
  CHECK(copiedSrc == 21);
  CHECK(copiedCount == 3);
  CHECK(nextLocal == 22);
  REQUIRE(instructions.size() == 5u);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 7u);
  CHECK(instructions[1].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[1].imm == 20u);
  CHECK(instructions[2].op == primec::IrOpcode::PushI64);
  CHECK(instructions[2].imm == 99u);
  CHECK(instructions[3].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[3].imm == 21u);
  CHECK(instructions[4].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[4].imm == 7u);
}

TEST_CASE("ir lowerer conversions helper rejects incompatible internal soa storage aliases") {
  primec::Expr target;
  target.kind = primec::Expr::Kind::Name;
  target.name = "storage";
  primec::Expr value;
  value.kind = primec::Expr::Kind::Name;
  value.name = "wrongStorage";
  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.name = "assign";
  expr.args = {target, value};

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo storageInfo;
  storageInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  storageInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  storageInfo.structTypeName = "/std/collections/soa_storage/SoaColumn__ti32";
  storageInfo.index = 7;
  storageInfo.isMutable = true;
  locals.emplace("storage", storageInfo);

  std::vector<primec::IrInstruction> instructions;
  std::string error;
  bool handled = false;
  int32_t nextLocal = 20;
  const bool ok = primec::ir_lowerer::emitConversionsAndCallsOperatorExpr(
      expr,
      locals,
      nextLocal,
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](primec::ir_lowerer::LocalInfo::ValueKind, bool) { return true; },
      [&]() { return nextLocal++; },
      []() {},
      []() {},
      []() {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) {
        return false;
      },
      [](const std::string &) { return primec::ir_lowerer::LocalInfo::ValueKind::Unknown; },
      [](const std::string &, std::string &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return std::string("/std/collections/soa_storage/SoaColumns2__ti32_i32");
      },
      [](const std::string &, const std::string &, std::string &) { return false; },
      [](const std::string &, int32_t &) { return false; },
      [](const std::string &, const std::string &, int32_t &, int32_t &, std::string &) {
        return false;
      },
      [](int32_t, int32_t, int32_t) { return false; },
      instructions,
      handled,
      error);

  CHECK_FALSE(ok);
  CHECK(handled);
  CHECK(error == "assign requires matching struct value");
  CHECK(instructions.empty());
}

TEST_CASE("ir lowerer conversions helper ignores unrelated call names") {
  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.name = "pow";

  std::vector<primec::IrInstruction> instructions;
  std::string error;
  bool handled = true;
  int32_t nextLocal = 0;
  const bool ok = primec::ir_lowerer::emitConversionsAndCallsOperatorExpr(
      expr,
      {},
      nextLocal,
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](primec::ir_lowerer::LocalInfo::ValueKind, bool) { return true; },
      [&]() { return nextLocal++; },
      []() {},
      []() {},
      []() {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return false; },
      [](const std::string &) { return primec::ir_lowerer::LocalInfo::ValueKind::Unknown; },
      [](const std::string &, std::string &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string(); },
      [](const std::string &, const std::string &, std::string &) { return false; },
      [](const std::string &, int32_t &) { return false; },
      [](const std::string &, const std::string &, int32_t &, int32_t &, std::string &) { return false; },
      [](int32_t, int32_t, int32_t) { return false; },
      instructions,
      handled,
      error);

  CHECK(ok);
  CHECK_FALSE(handled);
  CHECK(error.empty());
  CHECK(instructions.empty());
}

TEST_CASE("ir lowerer conversions control-tail helper lowers if blocks") {
  primec::Expr cond;
  cond.kind = primec::Expr::Kind::BoolLiteral;
  cond.boolValue = true;

  primec::Expr thenValue;
  thenValue.kind = primec::Expr::Kind::Literal;
  thenValue.literalValue = 1;
  primec::Expr thenBlock;
  thenBlock.kind = primec::Expr::Kind::Call;
  thenBlock.name = "then_block";
  thenBlock.hasBodyArguments = true;
  thenBlock.bodyArguments = {thenValue};

  primec::Expr elseValue;
  elseValue.kind = primec::Expr::Kind::Literal;
  elseValue.literalValue = 2;
  primec::Expr elseBlock;
  elseBlock.kind = primec::Expr::Kind::Call;
  elseBlock.name = "else_block";
  elseBlock.hasBodyArguments = true;
  elseBlock.bodyArguments = {elseValue};

  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.name = "if";
  expr.args = {cond, thenBlock, elseBlock};

  std::vector<primec::IrInstruction> instructions;
  std::string error;
  bool handled = false;
  int enteredScopes = 0;
  int exitedScopes = 0;
  const bool ok = primec::ir_lowerer::emitConversionsAndCallsControlExprTail(
      expr,
      {},
      [&](const primec::Expr &valueExpr, const primec::ir_lowerer::LocalMap &) {
        if (valueExpr.kind == primec::Expr::Kind::BoolLiteral) {
          instructions.push_back({primec::IrOpcode::PushI32, valueExpr.boolValue ? 1u : 0u});
          return true;
        }
        if (valueExpr.kind == primec::Expr::Kind::Literal) {
          instructions.push_back({primec::IrOpcode::PushI32, static_cast<uint64_t>(valueExpr.literalValue)});
          return true;
        }
        return false;
      },
      [](const primec::Expr &, primec::ir_lowerer::LocalMap &) { return true; },
      [](const primec::Expr &valueExpr, const primec::ir_lowerer::LocalMap &) {
        if (valueExpr.kind == primec::Expr::Kind::BoolLiteral) {
          return primec::ir_lowerer::LocalInfo::ValueKind::Bool;
        }
        if (valueExpr.kind == primec::Expr::Kind::Literal) {
          return primec::ir_lowerer::LocalInfo::ValueKind::Int32;
        }
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](primec::ir_lowerer::LocalInfo::ValueKind leftKind, primec::ir_lowerer::LocalInfo::ValueKind rightKind) {
        return (leftKind == rightKind) ? leftKind : primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const std::vector<std::optional<std::string>> &argNames) {
        return std::any_of(argNames.begin(), argNames.end(), [](const auto &name) { return name.has_value(); });
      },
      [](const primec::Expr &) { return static_cast<const primec::Definition *>(nullptr); },
      [](const primec::Expr &callExpr) { return std::string("/") + callExpr.name; },
      [](const primec::Expr &, primec::Expr &, std::string &) { return false; },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::Kind::Value; },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo::Kind) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string(); },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [&]() { ++enteredScopes; },
      [&]() { ++exitedScopes; },
      [](const primec::Expr &callExpr) { return callExpr.kind == primec::Expr::Kind::Call && callExpr.name == "return"; },
      [](const primec::Expr &callExpr) { return callExpr.kind == primec::Expr::Kind::Call && callExpr.name == "block"; },
      [](const primec::Expr &callExpr) { return callExpr.kind == primec::Expr::Kind::Call && callExpr.name == "match"; },
      [](const primec::Expr &callExpr) { return callExpr.kind == primec::Expr::Kind::Call && callExpr.name == "if"; },
      instructions,
      handled,
      error);

  CHECK(ok);
  CHECK(handled);
  CHECK(error.empty());
  CHECK(enteredScopes == 2);
  CHECK(exitedScopes == 2);
  CHECK(std::any_of(instructions.begin(),
                    instructions.end(),
                    [](const primec::IrInstruction &inst) { return inst.op == primec::IrOpcode::JumpIfZero; }));
}

TEST_CASE("ir lowerer conversions control-tail helper treats builtin comparisons as bool conditions") {
  primec::Expr lhs;
  lhs.kind = primec::Expr::Kind::Literal;
  lhs.literalValue = 2;

  primec::Expr rhs;
  rhs.kind = primec::Expr::Kind::Literal;
  rhs.literalValue = 1;

  primec::Expr cond;
  cond.kind = primec::Expr::Kind::Call;
  cond.name = "greater_than";
  cond.args = {lhs, rhs};

  primec::Expr thenValue;
  thenValue.kind = primec::Expr::Kind::Literal;
  thenValue.literalValue = 7;
  primec::Expr thenBlock;
  thenBlock.kind = primec::Expr::Kind::Call;
  thenBlock.name = "then_block";
  thenBlock.hasBodyArguments = true;
  thenBlock.bodyArguments = {thenValue};

  primec::Expr elseValue;
  elseValue.kind = primec::Expr::Kind::Literal;
  elseValue.literalValue = 3;
  primec::Expr elseBlock;
  elseBlock.kind = primec::Expr::Kind::Call;
  elseBlock.name = "else_block";
  elseBlock.hasBodyArguments = true;
  elseBlock.bodyArguments = {elseValue};

  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.name = "if";
  expr.args = {cond, thenBlock, elseBlock};

  std::vector<primec::IrInstruction> instructions;
  std::string error;
  bool handled = false;
  const bool ok = primec::ir_lowerer::emitConversionsAndCallsControlExprTail(
      expr,
      {},
      [&](const primec::Expr &valueExpr, const primec::ir_lowerer::LocalMap &) {
        if (valueExpr.kind == primec::Expr::Kind::Literal) {
          instructions.push_back({primec::IrOpcode::PushI32, static_cast<uint64_t>(valueExpr.literalValue)});
          return true;
        }
        if (valueExpr.kind == primec::Expr::Kind::Call && valueExpr.name == "greater_than") {
          instructions.push_back({primec::IrOpcode::PushI32, 1u});
          return true;
        }
        return false;
      },
      [](const primec::Expr &, primec::ir_lowerer::LocalMap &) { return true; },
      [](const primec::Expr &valueExpr, const primec::ir_lowerer::LocalMap &) {
        if (valueExpr.kind == primec::Expr::Kind::Literal) {
          return primec::ir_lowerer::LocalInfo::ValueKind::Int32;
        }
        if (valueExpr.kind == primec::Expr::Kind::Call && valueExpr.name == "greater_than") {
          return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
        }
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](primec::ir_lowerer::LocalInfo::ValueKind leftKind, primec::ir_lowerer::LocalInfo::ValueKind rightKind) {
        return (leftKind == rightKind) ? leftKind : primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const std::vector<std::optional<std::string>> &argNames) {
        return std::any_of(argNames.begin(), argNames.end(), [](const auto &name) { return name.has_value(); });
      },
      [](const primec::Expr &) { return static_cast<const primec::Definition *>(nullptr); },
      [](const primec::Expr &callExpr) { return std::string("/") + callExpr.name; },
      [](const primec::Expr &, primec::Expr &, std::string &) { return false; },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::Kind::Value; },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo::Kind) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string(); },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      []() {},
      []() {},
      [](const primec::Expr &callExpr) { return callExpr.kind == primec::Expr::Kind::Call && callExpr.name == "return"; },
      [](const primec::Expr &callExpr) { return callExpr.kind == primec::Expr::Kind::Call && callExpr.name == "block"; },
      [](const primec::Expr &callExpr) { return callExpr.kind == primec::Expr::Kind::Call && callExpr.name == "match"; },
      [](const primec::Expr &callExpr) { return callExpr.kind == primec::Expr::Kind::Call && callExpr.name == "if"; },
      instructions,
      handled,
      error);

  CHECK(ok);
  CHECK(handled);
  CHECK(error.empty());
  CHECK(std::any_of(instructions.begin(),
                    instructions.end(),
                    [](const primec::IrInstruction &inst) { return inst.op == primec::IrOpcode::JumpIfZero; }));
}

TEST_CASE("ir lowerer conversions control-tail helper infers comparison branch values as bool") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;

  primec::Expr cond;
  cond.kind = primec::Expr::Kind::BoolLiteral;
  cond.boolValue = true;

  primec::Expr lhs;
  lhs.kind = primec::Expr::Kind::Literal;
  lhs.literalValue = 4;

  primec::Expr rhs;
  rhs.kind = primec::Expr::Kind::Literal;
  rhs.literalValue = 1;

  primec::Expr thenValue;
  thenValue.kind = primec::Expr::Kind::Call;
  thenValue.name = "greater_than";
  thenValue.args = {lhs, rhs};
  primec::Expr thenBlock;
  thenBlock.kind = primec::Expr::Kind::Call;
  thenBlock.name = "then_block";
  thenBlock.hasBodyArguments = true;
  thenBlock.bodyArguments = {thenValue};

  primec::Expr elseValue;
  elseValue.kind = primec::Expr::Kind::Call;
  elseValue.name = "equal";
  elseValue.args = {lhs, rhs};
  primec::Expr elseBlock;
  elseBlock.kind = primec::Expr::Kind::Call;
  elseBlock.name = "else_block";
  elseBlock.hasBodyArguments = true;
  elseBlock.bodyArguments = {elseValue};

  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.name = "if";
  expr.args = {cond, thenBlock, elseBlock};

  std::vector<primec::IrInstruction> instructions;
  std::string error;
  bool handled = false;
  const bool ok = primec::ir_lowerer::emitConversionsAndCallsControlExprTail(
      expr,
      {},
      [&](const primec::Expr &valueExpr, const primec::ir_lowerer::LocalMap &) {
        if (valueExpr.kind == primec::Expr::Kind::BoolLiteral) {
          instructions.push_back({primec::IrOpcode::PushI32, valueExpr.boolValue ? 1u : 0u});
          return true;
        }
        if (valueExpr.kind == primec::Expr::Kind::Call &&
            (valueExpr.name == "greater_than" || valueExpr.name == "equal")) {
          instructions.push_back({primec::IrOpcode::PushI32, 1u});
          return true;
        }
        return false;
      },
      [](const primec::Expr &, primec::ir_lowerer::LocalMap &) { return true; },
      [](const primec::Expr &valueExpr, const primec::ir_lowerer::LocalMap &) {
        if (valueExpr.kind == primec::Expr::Kind::BoolLiteral) {
          return ValueKind::Bool;
        }
        if (valueExpr.kind == primec::Expr::Kind::Call &&
            (valueExpr.name == "greater_than" || valueExpr.name == "equal")) {
          return ValueKind::Unknown;
        }
        return ValueKind::Unknown;
      },
      [](primec::ir_lowerer::LocalInfo::ValueKind leftKind, primec::ir_lowerer::LocalInfo::ValueKind rightKind) {
        return (leftKind == rightKind) ? leftKind : primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const std::vector<std::optional<std::string>> &argNames) {
        return std::any_of(argNames.begin(), argNames.end(), [](const auto &name) { return name.has_value(); });
      },
      [](const primec::Expr &) { return static_cast<const primec::Definition *>(nullptr); },
      [](const primec::Expr &callExpr) { return std::string("/") + callExpr.name; },
      [](const primec::Expr &, primec::Expr &, std::string &) { return false; },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::Kind::Value; },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo::Kind) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string(); },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      []() {},
      []() {},
      [](const primec::Expr &callExpr) { return callExpr.kind == primec::Expr::Kind::Call && callExpr.name == "return"; },
      [](const primec::Expr &callExpr) { return callExpr.kind == primec::Expr::Kind::Call && callExpr.name == "block"; },
      [](const primec::Expr &callExpr) { return callExpr.kind == primec::Expr::Kind::Call && callExpr.name == "match"; },
      [](const primec::Expr &callExpr) { return callExpr.kind == primec::Expr::Kind::Call && callExpr.name == "if"; },
      instructions,
      handled,
      error);

  CHECK(ok);
  CHECK(handled);
  CHECK(error.empty());
  CHECK(std::any_of(instructions.begin(),
                    instructions.end(),
                    [](const primec::IrInstruction &inst) { return inst.op == primec::IrOpcode::JumpIfZero; }));
}

TEST_CASE("ir lowerer conversions control-tail helper rejects incompatible if branch values") {
  primec::Expr cond;
  cond.kind = primec::Expr::Kind::BoolLiteral;
  cond.boolValue = true;

  primec::Expr thenValue;
  thenValue.kind = primec::Expr::Kind::Literal;
  thenValue.literalValue = 1;
  primec::Expr thenBlock;
  thenBlock.kind = primec::Expr::Kind::Call;
  thenBlock.name = "then_block";
  thenBlock.hasBodyArguments = true;
  thenBlock.bodyArguments = {thenValue};

  primec::Expr elseValue;
  elseValue.kind = primec::Expr::Kind::BoolLiteral;
  elseValue.boolValue = false;
  primec::Expr elseBlock;
  elseBlock.kind = primec::Expr::Kind::Call;
  elseBlock.name = "else_block";
  elseBlock.hasBodyArguments = true;
  elseBlock.bodyArguments = {elseValue};

  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.name = "if";
  expr.args = {cond, thenBlock, elseBlock};

  std::vector<primec::IrInstruction> instructions;
  std::string error;
  bool handled = false;
  const bool ok = primec::ir_lowerer::emitConversionsAndCallsControlExprTail(
      expr,
      {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
      [](const primec::Expr &, primec::ir_lowerer::LocalMap &) { return true; },
      [](const primec::Expr &valueExpr, const primec::ir_lowerer::LocalMap &) {
        if (valueExpr.kind == primec::Expr::Kind::BoolLiteral) {
          return primec::ir_lowerer::LocalInfo::ValueKind::Bool;
        }
        if (valueExpr.kind == primec::Expr::Kind::Literal) {
          return primec::ir_lowerer::LocalInfo::ValueKind::Int32;
        }
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](primec::ir_lowerer::LocalInfo::ValueKind leftKind, primec::ir_lowerer::LocalInfo::ValueKind rightKind) {
        return (leftKind == rightKind) ? leftKind : primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const std::vector<std::optional<std::string>> &) { return false; },
      [](const primec::Expr &) { return static_cast<const primec::Definition *>(nullptr); },
      [](const primec::Expr &) { return std::string("/if"); },
      [](const primec::Expr &, primec::Expr &, std::string &) { return false; },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::Kind::Value; },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo::Kind) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string(); },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      []() {},
      []() {},
      [](const primec::Expr &callExpr) { return callExpr.kind == primec::Expr::Kind::Call && callExpr.name == "return"; },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &callExpr) { return callExpr.kind == primec::Expr::Kind::Call && callExpr.name == "if"; },
      instructions,
      handled,
      error);

  CHECK_FALSE(ok);
  CHECK(handled);
  CHECK(error == "if branches must return compatible types");
}

TEST_CASE("ir lowerer conversions control-tail helper ignores unrelated calls") {
  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.name = "pow";

  std::vector<primec::IrInstruction> instructions;
  std::string error;
  bool handled = true;
  const bool ok = primec::ir_lowerer::emitConversionsAndCallsControlExprTail(
      expr,
      {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
      [](const primec::Expr &, primec::ir_lowerer::LocalMap &) { return true; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](primec::ir_lowerer::LocalInfo::ValueKind leftKind, primec::ir_lowerer::LocalInfo::ValueKind rightKind) {
        return (leftKind == rightKind) ? leftKind : primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const std::vector<std::optional<std::string>> &) { return false; },
      [](const primec::Expr &) { return static_cast<const primec::Definition *>(nullptr); },
      [](const primec::Expr &) { return std::string("/pow"); },
      [](const primec::Expr &, primec::Expr &, std::string &) { return false; },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::Kind::Value; },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo::Kind) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string(); },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      []() {},
      []() {},
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &) { return false; },
      instructions,
      handled,
      error);

  CHECK(ok);
  CHECK_FALSE(handled);
  CHECK(error.empty());
  CHECK(instructions.empty());
}

TEST_CASE("ir lowerer return inference helpers infer parameter locals") {
  primec::Expr bindingExpr;
  bindingExpr.name = "param";

  primec::ir_lowerer::LocalMap locals;
  std::string error;
  REQUIRE(primec::ir_lowerer::inferReturnInferenceBindingIntoLocals(
      bindingExpr,
      true,
      "/pkg/fn",
      locals,
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::Kind::Value; },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo::Kind) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Int64;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string(); },
      [](const primec::Expr &) { return false; },
      error));
  CHECK(error.empty());
  REQUIRE(locals.count("param") == 1u);
  CHECK(locals.at("param").valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int64);
}

TEST_CASE("ir lowerer return inference helpers keep namespaced File bindings scalar") {
  primec::Expr bindingExpr;
  bindingExpr.name = "file";
  primec::Transform fileTransform;
  fileTransform.name = "/std/file/File";
  fileTransform.templateArgs.push_back("Read");
  bindingExpr.transforms.push_back(fileTransform);

  primec::ir_lowerer::LocalMap locals;
  std::string error;
  REQUIRE(primec::ir_lowerer::inferReturnInferenceBindingIntoLocals(
      bindingExpr,
      true,
      "/pkg/fn",
      locals,
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::Kind::Value; },
      [](const primec::Expr &) { return false; },
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
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string(); },
      [](const primec::Expr &) { return false; },
      error));
  CHECK(error.empty());
  REQUIRE(locals.count("file") == 1u);
  CHECK(locals.at("file").isFileHandle);
  CHECK(locals.at("file").valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int64);
  CHECK(locals.at("file").structTypeName.empty());
}

TEST_CASE("ir lowerer return inference helpers keep namespaced File constructor bindings as inferred structs") {
  primec::Expr fileCtor;
  fileCtor.kind = primec::Expr::Kind::Call;
  fileCtor.name = "File";
  fileCtor.namespacePrefix = "/std/file";
  fileCtor.templateArgs.push_back("Read");

  primec::Expr pathArg;
  pathArg.kind = primec::Expr::Kind::StringLiteral;
  pathArg.stringValue = "in.txt";
  fileCtor.args.push_back(pathArg);

  primec::Expr bindingExpr;
  bindingExpr.name = "file";
  bindingExpr.args.push_back(fileCtor);

  primec::ir_lowerer::LocalMap locals;
  std::string error;
  REQUIRE(primec::ir_lowerer::inferReturnInferenceBindingIntoLocals(
      bindingExpr,
      false,
      "/pkg/fn",
      locals,
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::Kind::Value; },
      [](const primec::Expr &) { return false; },
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
      [](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
        if (expr.kind == primec::Expr::Kind::Call && expr.name == "File" &&
            expr.namespacePrefix == "/std/file") {
          return std::string("/std/file/File<Read>");
        }
        return std::string();
      },
      [](const primec::Expr &) { return false; },
      error));
  CHECK(error.empty());
  REQUIRE(locals.count("file") == 1u);
  CHECK_FALSE(locals.at("file").isFileHandle);
  CHECK(locals.at("file").valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int64);
  CHECK(locals.at("file").structTypeName == "/std/file/File<Read>");
}

TEST_CASE("ir lowerer return inference helpers recover bool comparison bindings") {
  primec::Expr lhs;
  lhs.kind = primec::Expr::Kind::Name;
  lhs.name = "lhs";

  primec::Expr rhs;
  rhs.kind = primec::Expr::Kind::Name;
  rhs.name = "rhs";

  primec::Expr comparisonExpr;
  comparisonExpr.kind = primec::Expr::Kind::Call;
  comparisonExpr.name = "greater_than";
  comparisonExpr.args = {lhs, rhs};

  primec::Expr bindingExpr;
  bindingExpr.name = "flag";
  bindingExpr.args = {comparisonExpr};

  primec::ir_lowerer::LocalMap locals;
  std::string error;
  REQUIRE(primec::ir_lowerer::inferReturnInferenceBindingIntoLocals(
      bindingExpr,
      false,
      "/pkg/fn",
      locals,
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::Kind::Value; },
      [](const primec::Expr &) { return false; },
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
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string(); },
      [](const primec::Expr &) { return false; },
      error));
  CHECK(error.empty());
  REQUIRE(locals.count("flag") == 1u);
  CHECK(locals.at("flag").valueKind == primec::ir_lowerer::LocalInfo::ValueKind::Bool);
}

TEST_CASE("ir lowerer return inference helpers report untyped bindings") {
  primec::Expr bindingExpr;
  bindingExpr.name = "tmp";

  primec::ir_lowerer::LocalMap locals;
  std::string error;
  CHECK_FALSE(primec::ir_lowerer::inferReturnInferenceBindingIntoLocals(
      bindingExpr,
      false,
      "/pkg/fn",
      locals,
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::Kind::Value; },
      [](const primec::Expr &) { return false; },
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
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string(); },
      [](const primec::Expr &) { return false; },
      error));
  CHECK(error == "native backend requires typed bindings on /pkg/fn");
  CHECK(locals.empty());
}

TEST_CASE("ir lowerer return inference helpers reject string references") {
  primec::Expr bindingExpr;
  bindingExpr.name = "label";

  primec::ir_lowerer::LocalMap locals;
  std::string error;
  CHECK_FALSE(primec::ir_lowerer::inferReturnInferenceBindingIntoLocals(
      bindingExpr,
      true,
      "/pkg/fn",
      locals,
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::Kind::Reference; },
      [](const primec::Expr &) { return true; },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo::Kind) {
        return primec::ir_lowerer::LocalInfo::ValueKind::String;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string(); },
      [](const primec::Expr &) { return true; },
      error));
  CHECK(error == "native backend does not support string pointers or references");
}

TEST_CASE("ir lowerer return inference helpers reject variadic string references with arg-pack diagnostic") {
  primec::Expr bindingExpr;
  bindingExpr.name = "labels";
  primec::Transform argsTransform;
  argsTransform.name = "args";
  argsTransform.templateArgs.push_back("Reference<string>");
  bindingExpr.transforms.push_back(argsTransform);

  primec::ir_lowerer::LocalMap locals;
  std::string error;
  CHECK_FALSE(primec::ir_lowerer::inferReturnInferenceBindingIntoLocals(
      bindingExpr,
      true,
      "/pkg/fn",
      locals,
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::Kind::Reference; },
      [](const primec::Expr &) { return true; },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo::Kind) {
        return primec::ir_lowerer::LocalInfo::ValueKind::String;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &) {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string(); },
      [](const primec::Expr &) { return true; },
      error));
  CHECK(error == "variadic args<T> does not support string pointers or references");
}

TEST_CASE("ir lowerer return inference helpers infer typed value returns") {
  primec::Definition def;
  def.fullPath = "/main";

  primec::Expr literalOne;
  literalOne.kind = primec::Expr::Kind::Literal;
  literalOne.literalValue = 1;

  primec::Expr bindingStmt;
  bindingStmt.isBinding = true;
  bindingStmt.name = "x";
  bindingStmt.args = {literalOne};

  primec::Expr returnStmt;
  returnStmt.kind = primec::Expr::Kind::Call;
  returnStmt.name = "return";
  primec::Expr nameX;
  nameX.kind = primec::Expr::Kind::Name;
  nameX.name = "x";
  returnStmt.args = {nameX};

  def.statements = {bindingStmt, returnStmt};
  def.hasReturnStatement = true;

  auto inferBinding = [](const primec::Expr &bindingExpr, bool, primec::ir_lowerer::LocalMap &locals, std::string &) {
    primec::ir_lowerer::LocalInfo info;
    info.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
    info.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
    locals[bindingExpr.name] = info;
    return true;
  };
  auto inferExprKind = [](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &locals) {
    if (expr.kind == primec::Expr::Kind::Literal) {
      return primec::ir_lowerer::LocalInfo::ValueKind::Int32;
    }
    if (expr.kind == primec::Expr::Kind::Name) {
      auto it = locals.find(expr.name);
      if (it != locals.end()) {
        return it->second.valueKind;
      }
    }
    return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  };

  primec::ir_lowerer::ReturnInferenceOptions options;
  options.missingReturnBehavior = primec::ir_lowerer::MissingReturnBehavior::Error;
  options.includeDefinitionReturnExpr = false;

  primec::ir_lowerer::ReturnInfo out;
  std::string error;
  REQUIRE(primec::ir_lowerer::inferDefinitionReturnType(
      def,
      {},
      inferBinding,
      inferExprKind,
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &, primec::Expr &, std::string &) { return false; },
      options,
      out,
      error));
  CHECK(error.empty());
  CHECK_FALSE(out.returnsVoid);
  CHECK_FALSE(out.returnsArray);
  CHECK(out.kind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
}

TEST_CASE("ir lowerer return inference helpers recover bool comparison returns") {
  primec::Definition def;
  def.fullPath = "/cmp";
  def.hasReturnStatement = true;

  primec::Expr lhs;
  lhs.kind = primec::Expr::Kind::Literal;
  lhs.literalValue = 1;

  primec::Expr rhs = lhs;
  rhs.literalValue = 0;

  primec::Expr comparisonExpr;
  comparisonExpr.kind = primec::Expr::Kind::Call;
  comparisonExpr.name = "greater_than";
  comparisonExpr.args = {lhs, rhs};

  primec::Expr returnStmt;
  returnStmt.kind = primec::Expr::Kind::Call;
  returnStmt.name = "return";
  returnStmt.args = {comparisonExpr};

  def.statements = {returnStmt};

  primec::ir_lowerer::ReturnInferenceOptions options;
  options.missingReturnBehavior = primec::ir_lowerer::MissingReturnBehavior::Error;
  options.includeDefinitionReturnExpr = false;

  primec::ir_lowerer::ReturnInfo out;
  std::string error;
  REQUIRE(primec::ir_lowerer::inferDefinitionReturnType(
      def,
      {},
      [](const primec::Expr &, bool, primec::ir_lowerer::LocalMap &, std::string &) { return true; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &, primec::Expr &, std::string &) { return false; },
      options,
      out,
      error));
  CHECK(error.empty());
  CHECK_FALSE(out.returnsVoid);
  CHECK_FALSE(out.returnsArray);
  CHECK(out.kind == primec::ir_lowerer::LocalInfo::ValueKind::Bool);
}

TEST_CASE("ir lowerer return inference helpers report missing return in error mode") {
  primec::Definition def;
  def.fullPath = "/missing";
  def.hasReturnStatement = false;

  primec::ir_lowerer::ReturnInferenceOptions options;
  options.missingReturnBehavior = primec::ir_lowerer::MissingReturnBehavior::Error;
  options.includeDefinitionReturnExpr = false;

  primec::ir_lowerer::ReturnInfo out;
  std::string error;
  CHECK_FALSE(primec::ir_lowerer::inferDefinitionReturnType(
      def,
      {},
      [](const primec::Expr &, bool, primec::ir_lowerer::LocalMap &, std::string &) { return true; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &, primec::Expr &, std::string &) { return false; },
      options,
      out,
      error));
  CHECK(error == "unable to infer return type on /missing");
}

TEST_CASE("ir lowerer return inference helpers reject mixed void and value returns") {
  primec::Definition def;
  def.fullPath = "/mixed";
  def.hasReturnStatement = true;

  primec::Expr returnVoid;
  returnVoid.kind = primec::Expr::Kind::Call;
  returnVoid.name = "return";

  primec::Expr literalOne;
  literalOne.kind = primec::Expr::Kind::Literal;
  literalOne.literalValue = 1;
  primec::Expr returnValue;
  returnValue.kind = primec::Expr::Kind::Call;
  returnValue.name = "return";
  returnValue.args = {literalOne};

  def.statements = {returnVoid, returnValue};

  primec::ir_lowerer::ReturnInferenceOptions options;
  options.missingReturnBehavior = primec::ir_lowerer::MissingReturnBehavior::Void;
  options.includeDefinitionReturnExpr = false;

  primec::ir_lowerer::ReturnInfo out;
  std::string error;
  CHECK_FALSE(primec::ir_lowerer::inferDefinitionReturnType(
      def,
      {},
      [](const primec::Expr &, bool, primec::ir_lowerer::LocalMap &, std::string &) { return true; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Int32;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &, primec::Expr &, std::string &) { return false; },
      options,
      out,
      error));
  CHECK(error == "conflicting return types on /mixed");
}

TEST_CASE("ir lowerer result helpers resolve Result.ok method") {
  primec::Expr resultName;
  resultName.kind = primec::Expr::Kind::Name;
  resultName.name = "Result";

  primec::Expr valueExpr;
  valueExpr.kind = primec::Expr::Kind::Literal;
  valueExpr.literalValue = 1;

  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.isMethodCall = true;
  callExpr.name = "ok";
  callExpr.args = {resultName, valueExpr};

  auto lookupLocal = [](const std::string &) { return primec::ir_lowerer::LocalResultInfo{}; };
  auto resolveNone = [](const primec::Expr &) -> const primec::Definition * { return nullptr; };
  auto lookupDefinition = [](const std::string &, primec::ir_lowerer::ResultExprInfo &) { return false; };

  primec::ir_lowerer::ResultExprInfo out;
  CHECK(primec::ir_lowerer::resolveResultExprInfo(
      callExpr, lookupLocal, resolveNone, resolveNone, lookupDefinition, out));
  CHECK(out.isResult);
  CHECK(out.hasValue);
}

TEST_CASE("ir lowerer conversions helper dereferences direct aggregate pointer calls from semantic-product return facts") {
  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "makePairRef";
  callExpr.semanticNodeId = 101;

  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.name = "dereference";
  expr.args = {callExpr};

  primec::Definition callee;
  callee.fullPath = "/makePairRef";
  callee.namespacePrefix = "/";

  primec::SemanticProgram semanticProgram;
  const auto returnPathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/makePairRef");
  semanticProgram.returnFacts.push_back(primec::SemanticProgramReturnFact{
      .returnKind = "Int64",
      .structPath = "/Pair",
      .bindingTypeText = "StaleScalar",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .definitionPathId = returnPathId,
      .bindingTypeTextId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "Reference<Pair>"),
  });
  semanticProgram.publishedRoutingLookups.returnFactIndicesByDefinitionPathId
      .insert_or_assign(returnPathId, 0);
  const auto semanticTargets =
      primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);

  std::vector<primec::IrInstruction> instructions;
  std::string error;
  bool handled = false;
  int32_t nextLocal = 0;
  int emitCalls = 0;
  int resolveCalls = 0;
  const bool ok = primec::ir_lowerer::emitConversionsAndCallsOperatorExpr(
      expr,
      {},
      nextLocal,
      [&](const primec::Expr &valueExpr, const primec::ir_lowerer::LocalMap &) {
        ++emitCalls;
        CHECK(valueExpr.semanticNodeId == 101);
        instructions.push_back({primec::IrOpcode::LoadLocal, 17});
        return true;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](primec::ir_lowerer::LocalInfo::ValueKind, bool) { return true; },
      [&]() { return nextLocal++; },
      []() {},
      []() {},
      []() {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) {
        return false;
      },
      [](const std::string &) { return primec::ir_lowerer::LocalInfo::ValueKind::Unknown; },
      [](const std::string &, std::string &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string(); },
      [](const std::string &typeName, const std::string &, std::string &structPathOut) {
        if (typeName == "Pair") {
          structPathOut = "/Pair";
          return true;
        }
        return false;
      },
      [](const std::string &, int32_t &) { return false; },
      [](const std::string &, const std::string &, int32_t &, int32_t &, std::string &) {
        return false;
      },
      [](const std::string &, const std::string &, primec::ir_lowerer::LayoutFieldBinding &) {
        return false;
      },
      [](int32_t, int32_t, int32_t) { return false; },
      instructions,
      handled,
      error,
      [&](const primec::Expr &valueExpr) -> const primec::Definition * {
        ++resolveCalls;
        CHECK(valueExpr.semanticNodeId == 101);
        return &callee;
      },
      &semanticTargets);

  CHECK(ok);
  CHECK(handled);
  CHECK(error.empty());
  CHECK(resolveCalls == 1);
  CHECK(emitCalls == 1);
  REQUIRE(instructions.size() == 1);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 17);

  primec::SemanticProgram missingSemanticProgram;
  const auto missingSemanticTargets =
      primec::ir_lowerer::buildSemanticProductTargetAdapter(&missingSemanticProgram);
  instructions.clear();
  error.clear();
  handled = false;
  emitCalls = 0;
  resolveCalls = 0;
  const bool missingOk = primec::ir_lowerer::emitConversionsAndCallsOperatorExpr(
      expr,
      {},
      nextLocal,
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        ++emitCalls;
        return true;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](primec::ir_lowerer::LocalInfo::ValueKind, bool) { return true; },
      [&]() { return nextLocal++; },
      []() {},
      []() {},
      []() {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) {
        return false;
      },
      [](const std::string &) { return primec::ir_lowerer::LocalInfo::ValueKind::Unknown; },
      [](const std::string &, std::string &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string(); },
      [](const std::string &, const std::string &, std::string &) { return false; },
      [](const std::string &, int32_t &) { return false; },
      [](const std::string &, const std::string &, int32_t &, int32_t &, std::string &) {
        return false;
      },
      [](const std::string &, const std::string &, primec::ir_lowerer::LayoutFieldBinding &) {
        return false;
      },
      [](int32_t, int32_t, int32_t) { return false; },
      instructions,
      handled,
      error,
      [&](const primec::Expr &) -> const primec::Definition * {
        ++resolveCalls;
        return &callee;
      },
      &missingSemanticTargets);

  CHECK_FALSE(missingOk);
  CHECK(handled);
  CHECK(error == "missing semantic-product aggregate pointer return metadata: /makePairRef");
  CHECK(resolveCalls == 1);
  CHECK(emitCalls == 0);
  CHECK(instructions.empty());
}

TEST_CASE("ir lowerer conversions helper locations direct reference calls from semantic-product return facts") {
  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "makePairRef";
  callExpr.semanticNodeId = 111;

  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.name = "location";
  expr.args = {callExpr};

  primec::Definition callee;
  callee.fullPath = "/makePairRef";
  callee.namespacePrefix = "/";

  primec::SemanticProgram semanticProgram;
  const auto returnPathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/makePairRef");
  semanticProgram.returnFacts.push_back(primec::SemanticProgramReturnFact{
      .returnKind = "Int64",
      .structPath = "/Pair",
      .bindingTypeText = "StaleScalar",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .definitionPathId = returnPathId,
      .bindingTypeTextId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "Reference<Pair>"),
  });
  semanticProgram.publishedRoutingLookups.returnFactIndicesByDefinitionPathId
      .insert_or_assign(returnPathId, 0);
  const auto semanticTargets =
      primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);

  std::vector<primec::IrInstruction> instructions;
  std::string error;
  bool handled = false;
  int32_t nextLocal = 0;
  int emitCalls = 0;
  int resolveCalls = 0;
  const bool ok = primec::ir_lowerer::emitConversionsAndCallsOperatorExpr(
      expr,
      {},
      nextLocal,
      [&](const primec::Expr &valueExpr, const primec::ir_lowerer::LocalMap &) {
        ++emitCalls;
        CHECK(valueExpr.semanticNodeId == 111);
        instructions.push_back({primec::IrOpcode::LoadLocal, 23});
        return true;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](primec::ir_lowerer::LocalInfo::ValueKind, bool) { return true; },
      [&]() { return nextLocal++; },
      []() {},
      []() {},
      []() {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) {
        return false;
      },
      [](const std::string &) { return primec::ir_lowerer::LocalInfo::ValueKind::Unknown; },
      [](const std::string &, std::string &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string(); },
      [](const std::string &, const std::string &, std::string &) { return false; },
      [](const std::string &, int32_t &) { return false; },
      [](const std::string &, const std::string &, int32_t &, int32_t &, std::string &) {
        return false;
      },
      [](const std::string &, const std::string &, primec::ir_lowerer::LayoutFieldBinding &) {
        return false;
      },
      [](int32_t, int32_t, int32_t) { return false; },
      instructions,
      handled,
      error,
      [&](const primec::Expr &valueExpr) -> const primec::Definition * {
        ++resolveCalls;
        CHECK(valueExpr.semanticNodeId == 111);
        return &callee;
      },
      &semanticTargets);

  CHECK(ok);
  CHECK(handled);
  CHECK(error.empty());
  CHECK(resolveCalls == 1);
  CHECK(emitCalls == 1);
  REQUIRE(instructions.size() == 1);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 23);

  primec::SemanticProgram missingSemanticProgram;
  const auto missingSemanticTargets =
      primec::ir_lowerer::buildSemanticProductTargetAdapter(&missingSemanticProgram);
  instructions.clear();
  error.clear();
  handled = false;
  emitCalls = 0;
  resolveCalls = 0;
  const bool missingOk = primec::ir_lowerer::emitConversionsAndCallsOperatorExpr(
      expr,
      {},
      nextLocal,
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        ++emitCalls;
        return true;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](primec::ir_lowerer::LocalInfo::ValueKind, bool) { return true; },
      [&]() { return nextLocal++; },
      []() {},
      []() {},
      []() {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) {
        return false;
      },
      [](const std::string &) { return primec::ir_lowerer::LocalInfo::ValueKind::Unknown; },
      [](const std::string &, std::string &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string(); },
      [](const std::string &, const std::string &, std::string &) { return false; },
      [](const std::string &, int32_t &) { return false; },
      [](const std::string &, const std::string &, int32_t &, int32_t &, std::string &) {
        return false;
      },
      [](const std::string &, const std::string &, primec::ir_lowerer::LayoutFieldBinding &) {
        return false;
      },
      [](int32_t, int32_t, int32_t) { return false; },
      instructions,
      handled,
      error,
      [&](const primec::Expr &) -> const primec::Definition * {
        ++resolveCalls;
        return &callee;
      },
      &missingSemanticTargets);

  CHECK_FALSE(missingOk);
  CHECK(handled);
  CHECK(error == "missing semantic-product location reference return metadata: /makePairRef");
  CHECK(resolveCalls == 1);
  CHECK(emitCalls == 0);
  CHECK(instructions.empty());
}

TEST_SUITE_END();
