#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir lowerer pow helper rejects mixed signed unsigned operands") {
  primec::Expr left;
  left.kind = primec::Expr::Kind::Literal;
  left.literalValue = 1;
  left.isUnsigned = true;
  primec::Expr right;
  right.kind = primec::Expr::Kind::Literal;
  right.literalValue = 2;
  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.name = "pow";
  expr.args = {left, right};

  std::vector<primec::IrInstruction> instructions;
  std::string error;
  int32_t nextLocal = 0;
  auto result = primec::ir_lowerer::emitPowAbsSignOperatorExpr(
      expr,
      {},
      true,
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Int32;
      },
      [](primec::ir_lowerer::LocalInfo::ValueKind leftKind, primec::ir_lowerer::LocalInfo::ValueKind rightKind) {
        return (leftKind == rightKind) ? leftKind : primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [&]() { return nextLocal++; },
      []() {},
      instructions,
      error);

  CHECK(result == primec::ir_lowerer::OperatorPowAbsSignEmitResult::Error);
  CHECK(error == "pow requires numeric arguments of the same type");
  CHECK(instructions.empty());
}

TEST_CASE("ir lowerer pow helper ignores non pow/abs/sign builtins") {
  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.name = "clamp";

  std::vector<primec::IrInstruction> instructions;
  std::string error;
  int32_t nextLocal = 0;
  auto result = primec::ir_lowerer::emitPowAbsSignOperatorExpr(
      expr,
      {},
      true,
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](primec::ir_lowerer::LocalInfo::ValueKind leftKind, primec::ir_lowerer::LocalInfo::ValueKind rightKind) {
        return (leftKind == rightKind) ? leftKind : primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [&]() { return nextLocal++; },
      []() {},
      instructions,
      error);

  CHECK(result == primec::ir_lowerer::OperatorPowAbsSignEmitResult::NotHandled);
  CHECK(error.empty());
  CHECK(instructions.empty());
}

TEST_CASE("ir lowerer conversions helper emits float conversion opcode") {
  primec::Expr arg;
  arg.kind = primec::Expr::Kind::Literal;
  arg.literalValue = 7;
  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.name = "convert";
  expr.templateArgs = {"f32"};
  expr.args = {arg};

  std::vector<primec::IrInstruction> instructions;
  std::string error;
  bool handled = false;
  int32_t nextLocal = 0;
  const bool ok = primec::ir_lowerer::emitConversionsAndCallsOperatorExpr(
      expr,
      {},
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
      [](const std::string &typeName) {
        if (typeName == "f32") {
          return primec::ir_lowerer::LocalInfo::ValueKind::Float32;
        }
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
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
  CHECK(handled);
  CHECK(error.empty());
  CHECK(std::any_of(instructions.begin(),
                    instructions.end(),
                    [](const primec::IrInstruction &inst) { return inst.op == primec::IrOpcode::ConvertI32ToF32; }));
}

TEST_CASE("ir lowerer conversions helper lowers alloc intrinsic to heap alloc") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;

  primec::Expr countExpr;
  countExpr.kind = primec::Expr::Kind::Literal;
  countExpr.intWidth = 32;
  countExpr.literalValue = 3;

  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.name = "/std/intrinsics/memory/alloc";
  expr.templateArgs = {"Pair"};
  expr.args = {countExpr};
  expr.namespacePrefix = "/pkg";

  std::vector<primec::IrInstruction> instructions;
  std::string error;
  bool handled = false;
  int32_t nextLocal = 0;
  const bool ok = primec::ir_lowerer::emitConversionsAndCallsOperatorExpr(
      expr,
      {},
      nextLocal,
      [&](const primec::Expr &valueExpr, const primec::ir_lowerer::LocalMap &) {
        instructions.push_back({primec::IrOpcode::PushI32, static_cast<uint64_t>(valueExpr.literalValue)});
        return true;
      },
      [](const primec::Expr &valueExpr, const primec::ir_lowerer::LocalMap &) {
        if (valueExpr.kind == primec::Expr::Kind::Literal) {
          return ValueKind::Int32;
        }
        return ValueKind::Unknown;
      },
      [](ValueKind, bool) { return true; },
      [&]() { return nextLocal++; },
      []() {},
      []() {},
      []() {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return false; },
      [](const std::string &) { return ValueKind::Unknown; },
      [](const std::string &, std::string &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string(); },
      [](const std::string &typeName, const std::string &namespacePrefix, std::string &resolvedOut) {
        if (typeName == "Pair" && namespacePrefix == "/pkg") {
          resolvedOut = "/pkg/Pair";
          return true;
        }
        return false;
      },
      [](const std::string &structTypeName, int32_t &slotCount) {
        if (structTypeName == "/pkg/Pair") {
          slotCount = 2;
          return true;
        }
        return false;
      },
      [](const std::string &, const std::string &, int32_t &, int32_t &, std::string &) { return false; },
      [](int32_t, int32_t, int32_t) { return false; },
      instructions,
      handled,
      error);

  CHECK(ok);
  CHECK(handled);
  CHECK(error.empty());
  REQUIRE(instructions.size() == 4);
  CHECK(instructions[0].op == primec::IrOpcode::PushI32);
  CHECK(instructions[0].imm == 3);
  CHECK(instructions[1].op == primec::IrOpcode::PushI32);
  CHECK(instructions[1].imm == 2);
  CHECK(instructions[2].op == primec::IrOpcode::MulI32);
  CHECK(instructions[3].op == primec::IrOpcode::HeapAlloc);
}

TEST_CASE("ir lowerer conversions helper lowers free intrinsic to heap free") {
  primec::Expr pointerExpr;
  pointerExpr.kind = primec::Expr::Kind::Name;
  pointerExpr.name = "ptr";

  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.name = "/std/intrinsics/memory/free";
  expr.args = {pointerExpr};

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo pointerInfo;
  pointerInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Pointer;
  pointerInfo.index = 4;
  locals.emplace("ptr", pointerInfo);

  std::vector<primec::IrInstruction> instructions;
  std::string error;
  bool handled = false;
  int32_t nextLocal = 0;
  const bool ok = primec::ir_lowerer::emitConversionsAndCallsOperatorExpr(
      expr,
      locals,
      nextLocal,
      [&](const primec::Expr &valueExpr, const primec::ir_lowerer::LocalMap &localsIn) {
        if (valueExpr.kind != primec::Expr::Kind::Name) {
          return false;
        }
        const auto it = localsIn.find(valueExpr.name);
        if (it == localsIn.end()) {
          return false;
        }
        instructions.push_back({primec::IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
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
  CHECK(handled);
  CHECK(error.empty());
  REQUIRE(instructions.size() == 2);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 4);
  CHECK(instructions[1].op == primec::IrOpcode::HeapFree);
}

TEST_CASE("ir lowerer conversions helper lowers realloc intrinsic to heap realloc") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;

  primec::Expr pointerExpr;
  pointerExpr.kind = primec::Expr::Kind::Name;
  pointerExpr.name = "ptr";

  primec::Expr countExpr;
  countExpr.kind = primec::Expr::Kind::Literal;
  countExpr.intWidth = 32;
  countExpr.literalValue = 3;

  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.name = "/std/intrinsics/memory/realloc";
  expr.args = {pointerExpr, countExpr};

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo pointerInfo;
  pointerInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Pointer;
  pointerInfo.index = 4;
  pointerInfo.structTypeName = "/pkg/Pair";
  locals.emplace("ptr", pointerInfo);

  std::vector<primec::IrInstruction> instructions;
  std::string error;
  bool handled = false;
  int32_t nextLocal = 0;
  const bool ok = primec::ir_lowerer::emitConversionsAndCallsOperatorExpr(
      expr,
      locals,
      nextLocal,
      [&](const primec::Expr &valueExpr, const primec::ir_lowerer::LocalMap &localsIn) {
        if (valueExpr.kind == primec::Expr::Kind::Name) {
          auto it = localsIn.find(valueExpr.name);
          if (it == localsIn.end()) {
            return false;
          }
          instructions.push_back({primec::IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
          return true;
        }
        instructions.push_back({primec::IrOpcode::PushI32, static_cast<uint64_t>(valueExpr.literalValue)});
        return true;
      },
      [](const primec::Expr &valueExpr, const primec::ir_lowerer::LocalMap &) {
        if (valueExpr.kind == primec::Expr::Kind::Literal) {
          return ValueKind::Int32;
        }
        return ValueKind::Unknown;
      },
      [](ValueKind, bool) { return true; },
      [&]() { return nextLocal++; },
      []() {},
      []() {},
      []() {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return false; },
      [](const std::string &) { return ValueKind::Unknown; },
      [](const std::string &, std::string &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string(); },
      [](const std::string &typeName, const std::string &namespacePrefix, std::string &resolvedOut) {
        if (typeName == "Pair" && namespacePrefix == "/pkg") {
          resolvedOut = "/pkg/Pair";
          return true;
        }
        return false;
      },
      [](const std::string &structTypeName, int32_t &slotCount) {
        if (structTypeName == "/pkg/Pair") {
          slotCount = 2;
          return true;
        }
        return false;
      },
      [](const std::string &, const std::string &, int32_t &, int32_t &, std::string &) { return false; },
      [](int32_t, int32_t, int32_t) { return false; },
      instructions,
      handled,
      error);

  CHECK(ok);
  CHECK(handled);
  CHECK(error.empty());
  REQUIRE(instructions.size() == 5);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 4);
  CHECK(instructions[1].op == primec::IrOpcode::PushI32);
  CHECK(instructions[1].imm == 3);
  CHECK(instructions[2].op == primec::IrOpcode::PushI32);
  CHECK(instructions[2].imm == 2);
  CHECK(instructions[3].op == primec::IrOpcode::MulI32);
  CHECK(instructions[4].op == primec::IrOpcode::HeapRealloc);
}

TEST_CASE("ir lowerer conversions helper lowers checked memory at intrinsic to bounded pointer arithmetic") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;

  primec::Expr pointerExpr;
  pointerExpr.kind = primec::Expr::Kind::Name;
  pointerExpr.name = "ptr";

  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Literal;
  indexExpr.intWidth = 32;
  indexExpr.literalValue = 1;

  primec::Expr countExpr;
  countExpr.kind = primec::Expr::Kind::Literal;
  countExpr.intWidth = 32;
  countExpr.literalValue = 2;

  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.name = "/std/intrinsics/memory/at";
  expr.args = {pointerExpr, indexExpr, countExpr};

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo pointerInfo;
  pointerInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Pointer;
  pointerInfo.index = 4;
  pointerInfo.structTypeName = "/pkg/Pair";
  locals.emplace("ptr", pointerInfo);

  std::vector<primec::IrInstruction> instructions;
  std::string error;
  bool handled = false;
  int32_t nextLocal = 0;
  const bool ok = primec::ir_lowerer::emitConversionsAndCallsOperatorExpr(
      expr,
      locals,
      nextLocal,
      [&](const primec::Expr &valueExpr, const primec::ir_lowerer::LocalMap &localsIn) {
        if (valueExpr.kind == primec::Expr::Kind::Name) {
          auto it = localsIn.find(valueExpr.name);
          if (it == localsIn.end()) {
            return false;
          }
          instructions.push_back({primec::IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
          return true;
        }
        instructions.push_back({primec::IrOpcode::PushI32, static_cast<uint64_t>(valueExpr.literalValue)});
        return true;
      },
      [](const primec::Expr &valueExpr, const primec::ir_lowerer::LocalMap &) {
        return valueExpr.kind == primec::Expr::Kind::Literal ? ValueKind::Int32 : ValueKind::Unknown;
      },
      [](ValueKind, bool) { return true; },
      [&]() { return nextLocal++; },
      []() {},
      [&]() {
        instructions.push_back(
            {primec::IrOpcode::PrintString, primec::encodePrintStringImm(0, primec::encodePrintFlags(true, true))});
      },
      []() {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return false; },
      [](const std::string &) { return ValueKind::Unknown; },
      [](const std::string &, std::string &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string(); },
      [](const std::string &typeName, const std::string &namespacePrefix, std::string &resolvedOut) {
        if (typeName == "Pair" && namespacePrefix == "/pkg") {
          resolvedOut = "/pkg/Pair";
          return true;
        }
        return false;
      },
      [](const std::string &structTypeName, int32_t &slotCount) {
        if (structTypeName == "/pkg/Pair") {
          slotCount = 2;
          return true;
        }
        return false;
      },
      [](const std::string &, const std::string &, int32_t &, int32_t &, std::string &) { return false; },
      [](int32_t, int32_t, int32_t) { return false; },
      instructions,
      handled,
      error);

  CHECK(ok);
  CHECK(handled);
  CHECK(error.empty());
  CHECK(std::any_of(instructions.begin(),
                    instructions.end(),
                    [](const primec::IrInstruction &inst) { return inst.op == primec::IrOpcode::CmpLtI32; }));
  CHECK(std::any_of(instructions.begin(),
                    instructions.end(),
                    [](const primec::IrInstruction &inst) { return inst.op == primec::IrOpcode::CmpGeI32; }));
  CHECK(std::any_of(instructions.begin(),
                    instructions.end(),
                    [](const primec::IrInstruction &inst) { return inst.op == primec::IrOpcode::JumpIfZero; }));
  CHECK(std::any_of(instructions.begin(), instructions.end(), [](const primec::IrInstruction &inst) {
    return inst.op == primec::IrOpcode::PushI32 && inst.imm == 32;
  }));
  CHECK(std::any_of(instructions.begin(),
                    instructions.end(),
                    [](const primec::IrInstruction &inst) { return inst.op == primec::IrOpcode::MulI32; }));
  CHECK(std::any_of(instructions.begin(),
                    instructions.end(),
                    [](const primec::IrInstruction &inst) { return inst.op == primec::IrOpcode::AddI64; }));
}

TEST_CASE("ir lowerer conversions helper lowers unchecked memory at intrinsic to raw pointer arithmetic") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;

  primec::Expr pointerExpr;
  pointerExpr.kind = primec::Expr::Kind::Name;
  pointerExpr.name = "ptr";

  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Literal;
  indexExpr.intWidth = 32;
  indexExpr.literalValue = 1;

  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.name = "/std/intrinsics/memory/at_unsafe";
  expr.args = {pointerExpr, indexExpr};

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo pointerInfo;
  pointerInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Pointer;
  pointerInfo.index = 4;
  pointerInfo.structTypeName = "/pkg/Pair";
  locals.emplace("ptr", pointerInfo);

  std::vector<primec::IrInstruction> instructions;
  std::string error;
  bool handled = false;
  int32_t nextLocal = 0;
  const bool ok = primec::ir_lowerer::emitConversionsAndCallsOperatorExpr(
      expr,
      locals,
      nextLocal,
      [&](const primec::Expr &valueExpr, const primec::ir_lowerer::LocalMap &localsIn) {
        if (valueExpr.kind == primec::Expr::Kind::Name) {
          auto it = localsIn.find(valueExpr.name);
          if (it == localsIn.end()) {
            return false;
          }
          instructions.push_back({primec::IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
          return true;
        }
        instructions.push_back({primec::IrOpcode::PushI32, static_cast<uint64_t>(valueExpr.literalValue)});
        return true;
      },
      [](const primec::Expr &valueExpr, const primec::ir_lowerer::LocalMap &) {
        return valueExpr.kind == primec::Expr::Kind::Literal ? ValueKind::Int32 : ValueKind::Unknown;
      },
      [](ValueKind, bool) { return true; },
      [&]() { return nextLocal++; },
      []() {},
      []() {},
      []() {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return false; },
      [](const std::string &) { return ValueKind::Unknown; },
      [](const std::string &, std::string &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string(); },
      [](const std::string &typeName, const std::string &namespacePrefix, std::string &resolvedOut) {
        if (typeName == "Pair" && namespacePrefix == "/pkg") {
          resolvedOut = "/pkg/Pair";
          return true;
        }
        return false;
      },
      [](const std::string &structTypeName, int32_t &slotCount) {
        if (structTypeName == "/pkg/Pair") {
          slotCount = 2;
          return true;
        }
        return false;
      },
      [](const std::string &, const std::string &, int32_t &, int32_t &, std::string &) { return false; },
      [](int32_t, int32_t, int32_t) { return false; },
      instructions,
      handled,
      error);

  CHECK(ok);
  CHECK(handled);
  CHECK(error.empty());
  CHECK_FALSE(std::any_of(instructions.begin(), instructions.end(), [](const primec::IrInstruction &inst) {
    return inst.op == primec::IrOpcode::CmpLtI32 || inst.op == primec::IrOpcode::CmpGeI32;
  }));
  CHECK(std::any_of(instructions.begin(), instructions.end(), [](const primec::IrInstruction &inst) {
    return inst.op == primec::IrOpcode::PushI32 && inst.imm == 32;
  }));
  CHECK(std::any_of(instructions.begin(),
                    instructions.end(),
                    [](const primec::IrInstruction &inst) { return inst.op == primec::IrOpcode::MulI32; }));
  CHECK(std::any_of(instructions.begin(),
                    instructions.end(),
                    [](const primec::IrInstruction &inst) { return inst.op == primec::IrOpcode::AddI64; }));
}

TEST_CASE("ir lowerer conversions helper emits vector record header with data pointer") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;

  primec::Expr argA;
  argA.kind = primec::Expr::Kind::Literal;
  argA.intWidth = 32;
  argA.literalValue = 11;

  primec::Expr argB;
  argB.kind = primec::Expr::Kind::Literal;
  argB.intWidth = 32;
  argB.literalValue = 22;

  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.name = "vector";
  expr.templateArgs = {"i32"};
  expr.args = {argA, argB};

  std::vector<primec::IrInstruction> instructions;
  std::string error;
  bool handled = false;
  int32_t nextLocal = 5;
  const bool ok = primec::ir_lowerer::emitConversionsAndCallsOperatorExpr(
      expr,
      {},
      nextLocal,
      [&](const primec::Expr &valueExpr, const primec::ir_lowerer::LocalMap &) {
        instructions.push_back({primec::IrOpcode::PushI32, static_cast<uint64_t>(valueExpr.literalValue)});
        return true;
      },
      [](const primec::Expr &valueExpr, const primec::ir_lowerer::LocalMap &) {
        if (valueExpr.kind == primec::Expr::Kind::Literal) {
          return ValueKind::Int32;
        }
        return ValueKind::Unknown;
      },
      [](ValueKind, bool) { return true; },
      [&]() { return nextLocal++; },
      []() {},
      []() {},
      []() {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return false; },
      [](const std::string &typeName) {
        if (typeName == "i32") {
          return ValueKind::Int32;
        }
        return ValueKind::Unknown;
      },
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
  CHECK(handled);
  CHECK(error.empty());
  CHECK(nextLocal == 8);
  REQUIRE(instructions.size() == 18);
  CHECK(instructions[0].op == primec::IrOpcode::PushI32);
  CHECK(instructions[0].imm == 2);
  CHECK(instructions[1].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[1].imm == 5);
  CHECK(instructions[2].op == primec::IrOpcode::PushI32);
  CHECK(instructions[2].imm == 2);
  CHECK(instructions[3].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[3].imm == 6);
  CHECK(instructions[4].op == primec::IrOpcode::PushI32);
  CHECK(instructions[4].imm == 256);
  CHECK(instructions[5].op == primec::IrOpcode::HeapAlloc);
  CHECK(instructions[6].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[6].imm == 7);
  CHECK(instructions[7].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[7].imm == 7);
  CHECK(instructions[8].op == primec::IrOpcode::PushI32);
  CHECK(instructions[8].imm == 11);
  CHECK(instructions[9].op == primec::IrOpcode::StoreIndirect);
  CHECK(instructions[10].op == primec::IrOpcode::Pop);
  CHECK(instructions[11].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[11].imm == 7);
  CHECK(instructions[12].op == primec::IrOpcode::PushI64);
  CHECK(instructions[12].imm == primec::IrSlotBytes);
  CHECK(instructions[13].op == primec::IrOpcode::AddI64);
  CHECK(instructions[14].op == primec::IrOpcode::PushI32);
  CHECK(instructions[14].imm == 22);
  CHECK(instructions[15].op == primec::IrOpcode::StoreIndirect);
  CHECK(instructions[16].op == primec::IrOpcode::Pop);
  CHECK(instructions[17].op == primec::IrOpcode::AddressOfLocal);
  CHECK(instructions[17].imm == 5);
}


TEST_SUITE_END();
