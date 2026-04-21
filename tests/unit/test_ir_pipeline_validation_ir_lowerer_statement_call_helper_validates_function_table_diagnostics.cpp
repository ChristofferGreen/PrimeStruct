#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir lowerer statement call helper validates function table diagnostics") {
  using Result = primec::ir_lowerer::FunctionTableFinalizationResult;

  primec::Program program;
  primec::Definition entry;
  entry.fullPath = "/main/entry";
  primec::Definition target;
  target.fullPath = "/main/target";
  program.definitions = {entry, target};

  primec::IrFunction entryFunction;
  entryFunction.name = "/main/entry";
  std::unordered_set<std::string> loweredCallTargets = {"/main/target"};
  int32_t nextLocal = 4;
  std::vector<primec::IrFunction> outFunctions;
  int32_t entryIndex = -1;
  std::string error;
  CHECK(primec::ir_lowerer::finalizeEntryFunctionTableAndLowerCallables(
            program,
            program.definitions.front(),
            entryFunction,
            loweredCallTargets,
            [](const primec::Definition &) { return false; },
            [](const std::string &, primec::ir_lowerer::ReturnInfo &info) {
              info.returnsVoid = false;
              info.kind = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
              return true;
            },
            {},
            {},
            [](const primec::Expr &) { return false; },
            []() {},
            [](const primec::Definition &, int32_t &, primec::ir_lowerer::LocalMap &, primec::Expr &, std::string &) {
              return true;
            },
            [](const primec::Expr &, const primec::Definition &, const primec::ir_lowerer::LocalMap &, bool) {
              return true;
            },
            nextLocal,
            outFunctions,
            entryIndex,
            error) == Result::Error);
  CHECK(error == "native backend does not support return type on /main/target");
  CHECK(entryIndex == 0);
  REQUIRE(outFunctions.size() == 1);
  CHECK(outFunctions[0].name == "/main/entry");
}

TEST_CASE("ir lowerer arithmetic helper emits integer add opcode") {
  primec::Expr left;
  left.kind = primec::Expr::Kind::Literal;
  left.literalValue = 1;
  primec::Expr right;
  right.kind = primec::Expr::Kind::Literal;
  right.literalValue = 2;
  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.name = "plus";
  expr.args = {left, right};

  std::vector<primec::IrInstruction> instructions;
  std::string error;
  auto result = primec::ir_lowerer::emitArithmeticOperatorExpr(
      expr,
      {},
      [&](const primec::Expr &arg, const primec::ir_lowerer::LocalMap &) {
        instructions.push_back({primec::IrOpcode::PushI32, static_cast<uint64_t>(arg.literalValue)});
        return true;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Int32;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string{}; },
      [](primec::ir_lowerer::LocalInfo::ValueKind leftKind, primec::ir_lowerer::LocalInfo::ValueKind rightKind) {
        return (leftKind == rightKind) ? leftKind : primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      error);

  CHECK(result == primec::ir_lowerer::OperatorArithmeticEmitResult::Handled);
  CHECK(error.empty());
  REQUIRE(instructions.size() == 3);
  CHECK(instructions.back().op == primec::IrOpcode::AddI32);
}

TEST_CASE("ir lowerer arithmetic helper rewrites quaternion multiply from result-type fallback") {
  primec::Expr left;
  left.kind = primec::Expr::Kind::Name;
  left.name = "lhs";
  primec::Expr right;
  right.kind = primec::Expr::Kind::Name;
  right.name = "rhs";
  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.name = "multiply";
  expr.args = {left, right};

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo leftInfo;
  leftInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  leftInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  leftInfo.structTypeName = "/std/math/Quat";
  locals.emplace("lhs", leftInfo);
  primec::ir_lowerer::LocalInfo rightInfo;
  rightInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  rightInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  locals.emplace("rhs", rightInfo);

  std::string forwardedName;
  std::string error;
  auto result = primec::ir_lowerer::emitArithmeticOperatorExpr(
      expr,
      locals,
      [&](const primec::Expr &forwardedExpr, const primec::ir_lowerer::LocalMap &) {
        forwardedName = forwardedExpr.name;
        return true;
      },
      [](const primec::Expr &candidate, const primec::ir_lowerer::LocalMap &) {
        if (candidate.kind == primec::Expr::Kind::Name && candidate.name == "lhs") {
          return primec::ir_lowerer::LocalInfo::ValueKind::Int64;
        }
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &candidate, const primec::ir_lowerer::LocalMap &localsIn) {
        if (candidate.kind == primec::Expr::Kind::Name) {
          auto it = localsIn.find(candidate.name);
          return it == localsIn.end() ? std::string{} : it->second.structTypeName;
        }
        if (candidate.kind == primec::Expr::Kind::Call && candidate.name == "multiply") {
          return std::string("/std/math/Quat");
        }
        return std::string{};
      },
      [](primec::ir_lowerer::LocalInfo::ValueKind leftKind, primec::ir_lowerer::LocalInfo::ValueKind rightKind) {
        return (leftKind == rightKind) ? leftKind : primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](primec::IrOpcode, uint64_t) {},
      error);

  CHECK(result == primec::ir_lowerer::OperatorArithmeticEmitResult::Handled);
  CHECK(error.empty());
  CHECK(forwardedName == "/std/math/quat_multiply_internal");
}

TEST_CASE("ir lowerer arithmetic helper rewrites mat3 vec3 multiply from result-type fallback") {
  primec::Expr left;
  left.kind = primec::Expr::Kind::Name;
  left.name = "lhs";
  primec::Expr right;
  right.kind = primec::Expr::Kind::Name;
  right.name = "rhs";
  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.name = "multiply";
  expr.args = {left, right};

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo leftInfo;
  leftInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  leftInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  leftInfo.structTypeName = "/std/math/Mat3";
  locals.emplace("lhs", leftInfo);
  primec::ir_lowerer::LocalInfo rightInfo;
  rightInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  rightInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  locals.emplace("rhs", rightInfo);

  std::string forwardedName;
  std::string error;
  auto result = primec::ir_lowerer::emitArithmeticOperatorExpr(
      expr,
      locals,
      [&](const primec::Expr &forwardedExpr, const primec::ir_lowerer::LocalMap &) {
        forwardedName = forwardedExpr.name;
        return true;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &candidate, const primec::ir_lowerer::LocalMap &localsIn) {
        if (candidate.kind == primec::Expr::Kind::Name) {
          auto it = localsIn.find(candidate.name);
          return it == localsIn.end() ? std::string{} : it->second.structTypeName;
        }
        if (candidate.kind == primec::Expr::Kind::Call && candidate.name == "multiply") {
          return std::string("/std/math/Vec3");
        }
        return std::string{};
      },
      [](primec::ir_lowerer::LocalInfo::ValueKind leftKind, primec::ir_lowerer::LocalInfo::ValueKind rightKind) {
        return (leftKind == rightKind) ? leftKind : primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](primec::IrOpcode, uint64_t) {},
      error);

  CHECK(result == primec::ir_lowerer::OperatorArithmeticEmitResult::Handled);
  CHECK(error.empty());
  CHECK(forwardedName == "/std/math/mat3_mul_vec3_internal");
}

TEST_CASE("ir lowerer arithmetic helper validates pointer operand side") {
  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo pointerInfo;
  pointerInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Pointer;
  pointerInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  locals.emplace("ptr", pointerInfo);
  primec::ir_lowerer::LocalInfo intInfo;
  intInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  intInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  locals.emplace("value", intInfo);

  primec::Expr left;
  left.kind = primec::Expr::Kind::Name;
  left.name = "value";
  primec::Expr right;
  right.kind = primec::Expr::Kind::Name;
  right.name = "ptr";
  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.name = "plus";
  expr.args = {left, right};

  std::string error;
  auto result = primec::ir_lowerer::emitArithmeticOperatorExpr(
      expr,
      locals,
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
      [](const primec::Expr &arg, const primec::ir_lowerer::LocalMap &localMap) {
        auto it = localMap.find(arg.name);
        return (it == localMap.end()) ? primec::ir_lowerer::LocalInfo::ValueKind::Unknown : it->second.valueKind;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string{}; },
      [](primec::ir_lowerer::LocalInfo::ValueKind leftKind, primec::ir_lowerer::LocalInfo::ValueKind rightKind) {
        return (leftKind == rightKind) ? leftKind : primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](primec::IrOpcode, uint64_t) {},
      error);

  CHECK(result == primec::ir_lowerer::OperatorArithmeticEmitResult::Error);
  CHECK(error == "pointer arithmetic requires pointer on the left");
}

TEST_CASE("ir lowerer arithmetic helper treats reference handles as pointer operands") {
  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo referenceInfo;
  referenceInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Reference;
  referenceInfo.index = 7;
  referenceInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  locals.emplace("ref", referenceInfo);

  primec::Expr left;
  left.kind = primec::Expr::Kind::Name;
  left.name = "ref";
  primec::Expr right;
  right.kind = primec::Expr::Kind::Literal;
  right.literalValue = 0;
  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.name = "plus";
  expr.args = {left, right};

  std::vector<primec::IrInstruction> instructions;
  std::string error;
  auto result = primec::ir_lowerer::emitArithmeticOperatorExpr(
      expr,
      locals,
      [&](const primec::Expr &arg, const primec::ir_lowerer::LocalMap &) {
        if (arg.kind == primec::Expr::Kind::Literal) {
          instructions.push_back({primec::IrOpcode::PushI32, static_cast<uint64_t>(arg.literalValue)});
          return true;
        }
        return false;
      },
      [](const primec::Expr &arg, const primec::ir_lowerer::LocalMap &localMap) {
        if (arg.kind == primec::Expr::Kind::Name) {
          auto it = localMap.find(arg.name);
          return (it == localMap.end()) ? primec::ir_lowerer::LocalInfo::ValueKind::Unknown : it->second.valueKind;
        }
        return primec::ir_lowerer::LocalInfo::ValueKind::Int32;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string{}; },
      [](primec::ir_lowerer::LocalInfo::ValueKind leftKind, primec::ir_lowerer::LocalInfo::ValueKind rightKind) {
        return (leftKind == rightKind) ? leftKind : primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      error);

  CHECK(result == primec::ir_lowerer::OperatorArithmeticEmitResult::Handled);
  CHECK(error.empty());
  REQUIRE(instructions.size() == 3);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 7);
  CHECK(instructions[1].op == primec::IrOpcode::PushI32);
  CHECK(instructions[2].op == primec::IrOpcode::AddI64);
}

TEST_CASE("ir lowerer arithmetic helper allows scalar reference offsets on the right") {
  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo pointerInfo;
  pointerInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Pointer;
  pointerInfo.index = 3;
  pointerInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  locals.emplace("ptr", pointerInfo);
  primec::ir_lowerer::LocalInfo referenceInfo;
  referenceInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Reference;
  referenceInfo.index = 9;
  referenceInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  locals.emplace("offset", referenceInfo);

  primec::Expr left;
  left.kind = primec::Expr::Kind::Name;
  left.name = "ptr";
  primec::Expr right;
  right.kind = primec::Expr::Kind::Name;
  right.name = "offset";
  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.name = "plus";
  expr.args = {left, right};

  std::vector<primec::IrInstruction> instructions;
  std::string error;
  auto result = primec::ir_lowerer::emitArithmeticOperatorExpr(
      expr,
      locals,
      [&](const primec::Expr &arg, const primec::ir_lowerer::LocalMap &) {
        if (arg.kind == primec::Expr::Kind::Name && arg.name == "offset") {
          instructions.push_back({primec::IrOpcode::LoadLocal, 9});
          return true;
        }
        return false;
      },
      [](const primec::Expr &arg, const primec::ir_lowerer::LocalMap &localMap) {
        if (arg.kind == primec::Expr::Kind::Name) {
          auto it = localMap.find(arg.name);
          return (it == localMap.end()) ? primec::ir_lowerer::LocalInfo::ValueKind::Unknown : it->second.valueKind;
        }
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string{}; },
      [](primec::ir_lowerer::LocalInfo::ValueKind leftKind, primec::ir_lowerer::LocalInfo::ValueKind rightKind) {
        return (leftKind == rightKind) ? leftKind : primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      error);

  CHECK(result == primec::ir_lowerer::OperatorArithmeticEmitResult::Handled);
  CHECK(error.empty());
  REQUIRE(instructions.size() == 3);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 3);
  CHECK(instructions[1].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[1].imm == 9);
  CHECK(instructions[2].op == primec::IrOpcode::AddI64);
}

TEST_CASE("ir lowerer arithmetic helper infers scalar reference offsets on the right") {
  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo pointerInfo;
  pointerInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Pointer;
  pointerInfo.index = 3;
  pointerInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  locals.emplace("ptr", pointerInfo);
  primec::ir_lowerer::LocalInfo referenceInfo;
  referenceInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Reference;
  referenceInfo.index = 9;
  referenceInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  locals.emplace("offset", referenceInfo);

  primec::Expr left;
  left.kind = primec::Expr::Kind::Name;
  left.name = "ptr";
  primec::Expr right;
  right.kind = primec::Expr::Kind::Name;
  right.name = "offset";
  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.name = "plus";
  expr.args = {left, right};

  std::vector<primec::IrInstruction> instructions;
  std::string error;
  auto result = primec::ir_lowerer::emitArithmeticOperatorExpr(
      expr,
      locals,
      [&](const primec::Expr &arg, const primec::ir_lowerer::LocalMap &) {
        if (arg.kind == primec::Expr::Kind::Name && arg.name == "offset") {
          instructions.push_back({primec::IrOpcode::LoadLocal, 9});
          return true;
        }
        return false;
      },
      [](const primec::Expr &arg, const primec::ir_lowerer::LocalMap &localMap) {
        if (arg.kind == primec::Expr::Kind::Name) {
          if (arg.name == "offset") {
            return primec::ir_lowerer::LocalInfo::ValueKind::Int32;
          }
          auto it = localMap.find(arg.name);
          return (it == localMap.end()) ? primec::ir_lowerer::LocalInfo::ValueKind::Unknown
                                        : it->second.valueKind;
        }
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string{}; },
      [](primec::ir_lowerer::LocalInfo::ValueKind leftKind, primec::ir_lowerer::LocalInfo::ValueKind rightKind) {
        return (leftKind == rightKind) ? leftKind : primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      error);

  CHECK(result == primec::ir_lowerer::OperatorArithmeticEmitResult::Handled);
  CHECK(error.empty());
  REQUIRE(instructions.size() == 3);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 3);
  CHECK(instructions[1].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[1].imm == 9);
  CHECK(instructions[2].op == primec::IrOpcode::AddI64);
}

TEST_CASE("ir lowerer arithmetic helper keeps mutable scalar locals numeric") {
  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo widthInfo;
  widthInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Reference;
  widthInfo.index = 4;
  widthInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  locals.emplace("width", widthInfo);
  primec::ir_lowerer::LocalInfo heightInfo;
  heightInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Reference;
  heightInfo.index = 6;
  heightInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  locals.emplace("height", heightInfo);

  primec::Expr left;
  left.kind = primec::Expr::Kind::Name;
  left.name = "width";
  primec::Expr right;
  right.kind = primec::Expr::Kind::Name;
  right.name = "height";
  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.name = "plus";
  expr.args = {left, right};

  std::vector<primec::IrInstruction> instructions;
  std::string error;
  auto result = primec::ir_lowerer::emitArithmeticOperatorExpr(
      expr,
      locals,
      [&](const primec::Expr &arg, const primec::ir_lowerer::LocalMap &) {
        if (arg.kind == primec::Expr::Kind::Name && arg.name == "width") {
          instructions.push_back({primec::IrOpcode::LoadLocal, 4});
          return true;
        }
        if (arg.kind == primec::Expr::Kind::Name && arg.name == "height") {
          instructions.push_back({primec::IrOpcode::LoadLocal, 6});
          return true;
        }
        return false;
      },
      [](const primec::Expr &arg, const primec::ir_lowerer::LocalMap &localMap) {
        if (arg.kind == primec::Expr::Kind::Name) {
          auto it = localMap.find(arg.name);
          return (it == localMap.end()) ? primec::ir_lowerer::LocalInfo::ValueKind::Unknown
                                        : it->second.valueKind;
        }
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string{}; },
      [](primec::ir_lowerer::LocalInfo::ValueKind leftKind, primec::ir_lowerer::LocalInfo::ValueKind rightKind) {
        return (leftKind == rightKind) ? leftKind : primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      error);

  CHECK(result == primec::ir_lowerer::OperatorArithmeticEmitResult::Handled);
  CHECK(error.empty());
  REQUIRE(instructions.size() == 3);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 4);
  CHECK(instructions[1].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[1].imm == 6);
  CHECK(instructions[2].op == primec::IrOpcode::AddI32);
}

TEST_CASE("ir lowerer arithmetic helper allows scoped buffer byte offsets on the right") {
  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo pointerInfo;
  pointerInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Pointer;
  pointerInfo.index = 5;
  pointerInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  locals.emplace("baseBytes", pointerInfo);
  primec::ir_lowerer::LocalInfo offsetInfo;
  offsetInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Reference;
  offsetInfo.index = 11;
  offsetInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  locals.emplace("byteOffset", offsetInfo);

  primec::Expr left;
  left.kind = primec::Expr::Kind::Name;
  left.name = "baseBytes";
  primec::Expr right;
  right.kind = primec::Expr::Kind::Name;
  right.name = "byteOffset";
  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.name = "plus";
  expr.namespacePrefix = "/std/collections/internal_buffer_unchecked";
  expr.args = {left, right};

  std::vector<primec::IrInstruction> instructions;
  std::string error;
  auto result = primec::ir_lowerer::emitArithmeticOperatorExpr(
      expr,
      locals,
      [&](const primec::Expr &arg, const primec::ir_lowerer::LocalMap &) {
        if (arg.kind == primec::Expr::Kind::Name && arg.name == "byteOffset") {
          instructions.push_back({primec::IrOpcode::LoadLocal, 11});
          return true;
        }
        return false;
      },
      [](const primec::Expr &arg, const primec::ir_lowerer::LocalMap &localMap) {
        if (arg.kind == primec::Expr::Kind::Name) {
          auto it = localMap.find(arg.name);
          return (it == localMap.end()) ? primec::ir_lowerer::LocalInfo::ValueKind::Unknown : it->second.valueKind;
        }
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string{}; },
      [](primec::ir_lowerer::LocalInfo::ValueKind leftKind, primec::ir_lowerer::LocalInfo::ValueKind rightKind) {
        return (leftKind == rightKind) ? leftKind : primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      error);

  CHECK(result == primec::ir_lowerer::OperatorArithmeticEmitResult::Handled);
  CHECK(error.empty());
  REQUIRE(instructions.size() == 3);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 5);
  CHECK(instructions[1].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[1].imm == 11);
  CHECK(instructions[2].op == primec::IrOpcode::AddI64);
}

TEST_CASE("ir lowerer arithmetic helper ignores non arithmetic calls") {
  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.name = "equal";

  std::string error;
  auto result = primec::ir_lowerer::emitArithmeticOperatorExpr(
      expr,
      {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string{}; },
      [](primec::ir_lowerer::LocalInfo::ValueKind leftKind, primec::ir_lowerer::LocalInfo::ValueKind rightKind) {
        return (leftKind == rightKind) ? leftKind : primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](primec::IrOpcode, uint64_t) {},
      error);

  CHECK(result == primec::ir_lowerer::OperatorArithmeticEmitResult::NotHandled);
  CHECK(error.empty());
}

TEST_CASE("ir lowerer comparison helper emits integer less-than opcode") {
  primec::Expr left;
  left.kind = primec::Expr::Kind::Literal;
  left.literalValue = 1;
  primec::Expr right;
  right.kind = primec::Expr::Kind::Literal;
  right.literalValue = 2;
  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.name = "less_than";
  expr.args = {left, right};

  std::vector<primec::IrInstruction> instructions;
  std::string error;
  auto result = primec::ir_lowerer::emitComparisonOperatorExpr(
      expr,
      {},
      [&](const primec::Expr &arg, const primec::ir_lowerer::LocalMap &) {
        instructions.push_back({primec::IrOpcode::PushI32, static_cast<uint64_t>(arg.literalValue)});
        return true;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Int32;
      },
      [](primec::ir_lowerer::LocalInfo::ValueKind leftKind, primec::ir_lowerer::LocalInfo::ValueKind rightKind) {
        return (leftKind == rightKind) ? leftKind : primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](primec::ir_lowerer::LocalInfo::ValueKind, bool) { return true; },
      instructions,
      error);

  CHECK(result == primec::ir_lowerer::OperatorComparisonEmitResult::Handled);
  CHECK(error.empty());
  REQUIRE(instructions.size() == 3);
  CHECK(instructions.back().op == primec::IrOpcode::CmpLtI32);
}

TEST_CASE("ir lowerer comparison helper lowers logical and short-circuit") {
  primec::Expr left;
  left.kind = primec::Expr::Kind::BoolLiteral;
  left.boolValue = true;
  primec::Expr right;
  right.kind = primec::Expr::Kind::BoolLiteral;
  right.boolValue = false;
  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.name = "and";
  expr.args = {left, right};

  std::vector<primec::IrInstruction> instructions;
  std::string error;
  auto result = primec::ir_lowerer::emitComparisonOperatorExpr(
      expr,
      {},
      [&](const primec::Expr &arg, const primec::ir_lowerer::LocalMap &) {
        instructions.push_back({primec::IrOpcode::PushI32, arg.boolValue ? 1u : 0u});
        return true;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Bool;
      },
      [](primec::ir_lowerer::LocalInfo::ValueKind leftKind, primec::ir_lowerer::LocalInfo::ValueKind rightKind) {
        return (leftKind == rightKind) ? leftKind : primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [&](primec::ir_lowerer::LocalInfo::ValueKind kind, bool equals) {
        if (kind != primec::ir_lowerer::LocalInfo::ValueKind::Bool &&
            kind != primec::ir_lowerer::LocalInfo::ValueKind::Int32) {
          return false;
        }
        instructions.push_back({primec::IrOpcode::PushI32, 0});
        instructions.push_back({equals ? primec::IrOpcode::CmpEqI32 : primec::IrOpcode::CmpNeI32, 0});
        return true;
      },
      instructions,
      error);

  CHECK(result == primec::ir_lowerer::OperatorComparisonEmitResult::Handled);
  CHECK(error.empty());
  REQUIRE(instructions.size() == 9);
  CHECK(instructions[3].op == primec::IrOpcode::JumpIfZero);
  CHECK(instructions[3].imm == 8);
  CHECK(instructions[7].op == primec::IrOpcode::Jump);
  CHECK(instructions[7].imm == 9);
  CHECK(instructions[8].op == primec::IrOpcode::PushI32);
  CHECK(instructions[8].imm == 0);
}

TEST_CASE("ir lowerer comparison helper rejects string operands") {
  primec::Expr left;
  left.kind = primec::Expr::Kind::StringLiteral;
  left.stringValue = "\"a\"";
  primec::Expr right;
  right.kind = primec::Expr::Kind::StringLiteral;
  right.stringValue = "\"b\"";
  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.name = "equal";
  expr.args = {left, right};

  std::vector<primec::IrInstruction> instructions;
  std::string error;
  auto result = primec::ir_lowerer::emitComparisonOperatorExpr(
      expr,
      {},
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        instructions.push_back({primec::IrOpcode::PushI64, 0});
        return true;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::String;
      },
      [](primec::ir_lowerer::LocalInfo::ValueKind leftKind, primec::ir_lowerer::LocalInfo::ValueKind rightKind) {
        return (leftKind == rightKind) ? leftKind : primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](primec::ir_lowerer::LocalInfo::ValueKind, bool) { return true; },
      instructions,
      error);

  CHECK(result == primec::ir_lowerer::OperatorComparisonEmitResult::Error);
  CHECK(error == "native backend does not support string comparisons");
  CHECK(instructions.empty());
}

TEST_CASE("ir lowerer comparison helper ignores non comparison calls") {
  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.name = "plus";

  std::vector<primec::IrInstruction> instructions;
  std::string error;
  auto result = primec::ir_lowerer::emitComparisonOperatorExpr(
      expr,
      {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](primec::ir_lowerer::LocalInfo::ValueKind leftKind, primec::ir_lowerer::LocalInfo::ValueKind rightKind) {
        return (leftKind == rightKind) ? leftKind : primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](primec::ir_lowerer::LocalInfo::ValueKind, bool) { return true; },
      instructions,
      error);

  CHECK(result == primec::ir_lowerer::OperatorComparisonEmitResult::NotHandled);
  CHECK(error.empty());
  CHECK(instructions.empty());
}

TEST_CASE("ir lowerer saturate helper emits is_nan predicate opcode") {
  primec::Expr arg;
  arg.kind = primec::Expr::Kind::FloatLiteral;
  arg.floatValue = "1.0";
  arg.floatWidth = 32;
  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.name = "is_nan";
  expr.args = {arg};

  std::vector<primec::IrInstruction> instructions;
  std::string error;
  int32_t nextLocal = 0;
  auto result = primec::ir_lowerer::emitSaturateRoundingRootsOperatorExpr(
      expr,
      {},
      true,
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        instructions.push_back({primec::IrOpcode::PushF32, 0});
        return true;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Float32;
      },
      [&]() { return nextLocal++; },
      instructions,
      error);

  CHECK(result == primec::ir_lowerer::OperatorSaturateRoundingRootsEmitResult::Handled);
  CHECK(error.empty());
  REQUIRE(instructions.size() == 5);
  CHECK(instructions[1].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[4].op == primec::IrOpcode::CmpNeF32);
}

TEST_CASE("ir lowerer saturate helper rejects bool saturate operand") {
  primec::Expr arg;
  arg.kind = primec::Expr::Kind::BoolLiteral;
  arg.boolValue = true;
  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.name = "saturate";
  expr.args = {arg};

  std::vector<primec::IrInstruction> instructions;
  bool emitted = false;
  std::string error;
  int32_t nextLocal = 0;
  auto result = primec::ir_lowerer::emitSaturateRoundingRootsOperatorExpr(
      expr,
      {},
      true,
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        emitted = true;
        return true;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Bool;
      },
      [&]() { return nextLocal++; },
      instructions,
      error);

  CHECK(result == primec::ir_lowerer::OperatorSaturateRoundingRootsEmitResult::Error);
  CHECK(error == "saturate requires numeric argument");
  CHECK_FALSE(emitted);
  CHECK(instructions.empty());
}

TEST_CASE("ir lowerer saturate helper ignores non saturate builtins") {
  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.name = "plus";

  std::vector<primec::IrInstruction> instructions;
  std::string error;
  int32_t nextLocal = 0;
  auto result = primec::ir_lowerer::emitSaturateRoundingRootsOperatorExpr(
      expr,
      {},
      true,
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [&]() { return nextLocal++; },
      instructions,
      error);

  CHECK(result == primec::ir_lowerer::OperatorSaturateRoundingRootsEmitResult::NotHandled);
  CHECK(error.empty());
  CHECK(instructions.empty());
}

TEST_CASE("ir lowerer clamp helper emits radians multiply opcode") {
  primec::Expr arg;
  arg.kind = primec::Expr::Kind::FloatLiteral;
  arg.floatValue = "1.0";
  arg.floatWidth = 64;
  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.name = "radians";
  expr.args = {arg};

  std::vector<primec::IrInstruction> instructions;
  std::string error;
  int32_t nextLocal = 0;
  auto result = primec::ir_lowerer::emitClampMinMaxTrigOperatorExpr(
      expr,
      {},
      true,
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        instructions.push_back({primec::IrOpcode::PushF64, 0});
        return true;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Float64;
      },
      [](primec::ir_lowerer::LocalInfo::ValueKind leftKind, primec::ir_lowerer::LocalInfo::ValueKind rightKind) {
        return (leftKind == rightKind) ? leftKind : primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [&]() { return nextLocal++; },
      instructions,
      error);

  CHECK(result == primec::ir_lowerer::OperatorClampMinMaxTrigEmitResult::Handled);
  CHECK(error.empty());
  REQUIRE(instructions.size() == 3);
  CHECK(instructions.back().op == primec::IrOpcode::MulF64);
}

TEST_CASE("ir lowerer clamp helper rejects mixed signed unsigned clamp args") {
  primec::Expr value;
  value.kind = primec::Expr::Kind::Literal;
  value.literalValue = 1;
  value.isUnsigned = true;
  primec::Expr minValue;
  minValue.kind = primec::Expr::Kind::Literal;
  minValue.literalValue = 0;
  primec::Expr maxValue;
  maxValue.kind = primec::Expr::Kind::Literal;
  maxValue.literalValue = 2;
  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.name = "clamp";
  expr.args = {value, minValue, maxValue};

  std::vector<primec::IrInstruction> instructions;
  std::string error;
  int32_t nextLocal = 0;
  auto result = primec::ir_lowerer::emitClampMinMaxTrigOperatorExpr(
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
      instructions,
      error);

  CHECK(result == primec::ir_lowerer::OperatorClampMinMaxTrigEmitResult::Error);
  CHECK(error == "clamp requires numeric arguments of the same type");
  CHECK(instructions.empty());
}

TEST_CASE("ir lowerer clamp helper ignores non clamp builtins") {
  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.name = "saturate";

  std::vector<primec::IrInstruction> instructions;
  std::string error;
  int32_t nextLocal = 0;
  auto result = primec::ir_lowerer::emitClampMinMaxTrigOperatorExpr(
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
      instructions,
      error);

  CHECK(result == primec::ir_lowerer::OperatorClampMinMaxTrigEmitResult::NotHandled);
  CHECK(error.empty());
  CHECK(instructions.empty());
}

TEST_CASE("ir lowerer arc helper emits exp2 float multiply opcode") {
  primec::Expr arg;
  arg.kind = primec::Expr::Kind::FloatLiteral;
  arg.floatValue = "1.0";
  arg.floatWidth = 64;
  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.name = "exp2";
  expr.args = {arg};

  std::vector<primec::IrInstruction> instructions;
  std::string error;
  int32_t nextLocal = 0;
  auto result = primec::ir_lowerer::emitArcHyperbolicOperatorExpr(
      expr,
      {},
      true,
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        instructions.push_back({primec::IrOpcode::PushF64, 0});
        return true;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Float64;
      },
      [&]() { return nextLocal++; },
      instructions,
      error);

  CHECK(result == primec::ir_lowerer::OperatorArcHyperbolicEmitResult::Handled);
  CHECK(error.empty());
  CHECK(instructions.size() > 10);
  CHECK(std::any_of(instructions.begin(),
                    instructions.end(),
                    [](const primec::IrInstruction &inst) { return inst.op == primec::IrOpcode::MulF64; }));
}

TEST_CASE("ir lowerer arc helper rejects non-float arc operands") {
  primec::Expr arg;
  arg.kind = primec::Expr::Kind::BoolLiteral;
  arg.boolValue = true;
  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.name = "asin";
  expr.args = {arg};

  std::vector<primec::IrInstruction> instructions;
  std::string error;
  int32_t nextLocal = 0;
  auto result = primec::ir_lowerer::emitArcHyperbolicOperatorExpr(
      expr,
      {},
      true,
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Bool;
      },
      [&]() { return nextLocal++; },
      instructions,
      error);

  CHECK(result == primec::ir_lowerer::OperatorArcHyperbolicEmitResult::Error);
  CHECK(error == "asin requires float argument");
  CHECK(instructions.empty());
}

TEST_CASE("ir lowerer arc helper ignores non arc builtins") {
  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.name = "clamp";

  std::vector<primec::IrInstruction> instructions;
  std::string error;
  int32_t nextLocal = 0;
  auto result = primec::ir_lowerer::emitArcHyperbolicOperatorExpr(
      expr,
      {},
      true,
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [&]() { return nextLocal++; },
      instructions,
      error);

  CHECK(result == primec::ir_lowerer::OperatorArcHyperbolicEmitResult::NotHandled);
  CHECK(error.empty());
  CHECK(instructions.empty());
}

TEST_CASE("ir lowerer pow helper emits integer multiply opcode") {
  primec::Expr base;
  base.kind = primec::Expr::Kind::Literal;
  base.literalValue = 2;
  primec::Expr exponent;
  exponent.kind = primec::Expr::Kind::Literal;
  exponent.literalValue = 3;
  primec::Expr expr;
  expr.kind = primec::Expr::Kind::Call;
  expr.name = "pow";
  expr.args = {base, exponent};

  std::vector<primec::IrInstruction> instructions;
  std::string error;
  int32_t nextLocal = 0;
  auto result = primec::ir_lowerer::emitPowAbsSignOperatorExpr(
      expr,
      {},
      true,
      [&](const primec::Expr &arg, const primec::ir_lowerer::LocalMap &) {
        instructions.push_back({primec::IrOpcode::PushI32, static_cast<uint64_t>(arg.literalValue)});
        return true;
      },
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

  CHECK(result == primec::ir_lowerer::OperatorPowAbsSignEmitResult::Handled);
  CHECK(error.empty());
  CHECK(std::any_of(instructions.begin(),
                    instructions.end(),
                    [](const primec::IrInstruction &inst) { return inst.op == primec::IrOpcode::MulI32; }));
}

TEST_SUITE_END();
