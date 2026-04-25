#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir lowerer call helpers dispatch native call tail orchestration") {
  using Result = primec::ir_lowerer::NativeCallTailDispatchResult;
  using LocalInfo = primec::ir_lowerer::LocalInfo;

  primec::ir_lowerer::LocalMap locals;
  LocalInfo arrayInfo;
  arrayInfo.kind = LocalInfo::Kind::Array;
  arrayInfo.index = 9;
  arrayInfo.valueKind = LocalInfo::ValueKind::Int32;
  locals.emplace("arr", arrayInfo);

  LocalInfo indexInfo;
  indexInfo.kind = LocalInfo::Kind::Value;
  indexInfo.index = 3;
  indexInfo.valueKind = LocalInfo::ValueKind::Int32;
  locals.emplace("idx", indexInfo);

  LocalInfo soaInfo;
  soaInfo.kind = LocalInfo::Kind::Value;
  soaInfo.index = 7;
  soaInfo.valueKind = LocalInfo::ValueKind::Unknown;
  soaInfo.isSoaVector = true;
  locals.emplace("soa", soaInfo);

  LocalInfo soaPackInfo;
  soaPackInfo.kind = LocalInfo::Kind::Value;
  soaPackInfo.index = 11;
  soaPackInfo.isArgsPack = true;
  soaPackInfo.argsPackElementKind = LocalInfo::Kind::Vector;
  soaPackInfo.isSoaVector = true;
  locals.emplace("soaPack", soaPackInfo);

  primec::Expr arrName;
  arrName.kind = primec::Expr::Kind::Name;
  arrName.name = "arr";

  primec::Expr idxName;
  idxName.kind = primec::Expr::Kind::Name;
  idxName.name = "idx";

  primec::Expr soaName;
  soaName.kind = primec::Expr::Kind::Name;
  soaName.name = "soa";

  primec::Expr soaPackName;
  soaPackName.kind = primec::Expr::Kind::Name;
  soaPackName.name = "soaPack";

  std::vector<primec::IrInstruction> instructions;
  auto emitInstruction = [&](primec::IrOpcode op, uint64_t imm) {
    instructions.push_back({op, imm});
  };
  auto instructionCount = [&]() { return instructions.size(); };
  auto patchInstructionImm = [&](size_t index, uint64_t imm) { instructions.at(index).imm = imm; };

  int nextLocal = 20;
  std::string error;

  primec::Expr mathCall;
  mathCall.kind = primec::Expr::Kind::Call;
  mathCall.name = "sin";
  CHECK(primec::ir_lowerer::tryEmitNativeCallTailDispatch(
            mathCall,
            locals,
            [](const primec::Expr &, std::string &mathName) {
              mathName = "sin";
              return true;
            },
            [](const std::string &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) {
              return false;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, std::string &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return LocalInfo::ValueKind::Int32;
            },
            [&]() { return nextLocal++; },
            []() {},
            []() {},
            []() {},
            instructionCount,
            emitInstruction,
            patchInstructionImm,
            error) == Result::Error);
  CHECK(error == "native backend does not support math builtin: sin");

  primec::Expr countCall;
  countCall.kind = primec::Expr::Kind::Call;
  countCall.name = "count";
  countCall.args = {arrName};
  instructions.clear();
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitNativeCallTailDispatch(
            countCall,
            locals,
            [](const primec::Expr &, std::string &) { return false; },
            [](const std::string &) { return true; },
            [](const primec::Expr &callExpr, const primec::ir_lowerer::LocalMap &) {
              return callExpr.name == "count";
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) {
              return false;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, std::string &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return LocalInfo::ValueKind::Int32;
            },
            [&]() { return nextLocal++; },
            []() {},
            []() {},
            []() {},
            instructionCount,
            emitInstruction,
            patchInstructionImm,
            error) == Result::Emitted);
  CHECK(error.empty());
  CHECK_FALSE(instructions.empty());

  primec::Expr soaCountCall;
  soaCountCall.kind = primec::Expr::Kind::Call;
  soaCountCall.name = "count";
  soaCountCall.args = {soaName};
  instructions.clear();
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitNativeCallTailDispatch(
            soaCountCall,
            locals,
            [](const primec::Expr &, std::string &) { return false; },
            [](const std::string &) { return true; },
            [](const primec::Expr &callExpr, const primec::ir_lowerer::LocalMap &callLocals) {
              return primec::ir_lowerer::isArrayCountCall(callExpr, callLocals, false, "argv");
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) {
              return false;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, std::string &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return LocalInfo::ValueKind::Int32;
            },
            [&]() { return nextLocal++; },
            []() {},
            []() {},
            []() {},
            instructionCount,
            emitInstruction,
            patchInstructionImm,
            error) == Result::Emitted);
  CHECK(error.empty());
  CHECK_FALSE(instructions.empty());

  primec::Expr soaPackAccess;
  soaPackAccess.kind = primec::Expr::Kind::Call;
  soaPackAccess.name = "at";
  soaPackAccess.args = {soaPackName, idxName};

  primec::Expr soaPackIndexedCountCall;
  soaPackIndexedCountCall.kind = primec::Expr::Kind::Call;
  soaPackIndexedCountCall.name = "count";
  soaPackIndexedCountCall.args = {soaPackAccess};
  instructions.clear();
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitNativeCallTailDispatch(
            soaPackIndexedCountCall,
            locals,
            [](const primec::Expr &, std::string &) { return false; },
            [](const std::string &) { return true; },
            [](const primec::Expr &callExpr, const primec::ir_lowerer::LocalMap &callLocals) {
              return primec::ir_lowerer::isArrayCountCall(callExpr, callLocals, false, "argv");
            },
            [](const primec::Expr &callExpr, const primec::ir_lowerer::LocalMap &callLocals) {
              return primec::ir_lowerer::isVectorCapacityCall(callExpr, callLocals);
            },
            [](const primec::Expr &callExpr, const primec::ir_lowerer::LocalMap &callLocals) {
              return primec::ir_lowerer::isStringCountCall(callExpr, callLocals);
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, std::string &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return LocalInfo::ValueKind::Int32;
            },
            [&]() { return nextLocal++; },
            []() {},
            []() {},
            []() {},
            instructionCount,
            emitInstruction,
            patchInstructionImm,
            error) == Result::Emitted);
  CHECK(error.empty());
  CHECK_FALSE(instructions.empty());

  primec::Expr soaVectorAliasCountCall = soaCountCall;
  soaVectorAliasCountCall.name = "/vector/count";
  instructions.clear();
  error = "stale";
  CHECK(primec::ir_lowerer::tryEmitNativeCallTailDispatch(
            soaVectorAliasCountCall,
            locals,
            [](const primec::Expr &, std::string &) { return false; },
            [](const std::string &) { return true; },
            [](const primec::Expr &callExpr, const primec::ir_lowerer::LocalMap &callLocals) {
              return primec::ir_lowerer::isArrayCountCall(callExpr, callLocals, false, "argv");
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) {
              return false;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, std::string &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return LocalInfo::ValueKind::Int32;
            },
            [&]() { return nextLocal++; },
            []() {},
            []() {},
            []() {},
            instructionCount,
            emitInstruction,
            patchInstructionImm,
            error) == Result::NotHandled);
  CHECK(error == "stale");
  CHECK(instructions.empty());

  primec::Expr soaStdlibAliasCountCall = soaCountCall;
  soaStdlibAliasCountCall.name = "/std/collections/vector/count";
  instructions.clear();
  error = "stale";
  CHECK(primec::ir_lowerer::tryEmitNativeCallTailDispatch(
            soaStdlibAliasCountCall,
            locals,
            [](const primec::Expr &, std::string &) { return false; },
            [](const std::string &) { return true; },
            [](const primec::Expr &callExpr, const primec::ir_lowerer::LocalMap &callLocals) {
              return primec::ir_lowerer::isArrayCountCall(callExpr, callLocals, false, "argv");
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) {
              return false;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, std::string &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return LocalInfo::ValueKind::Int32;
            },
            [&]() { return nextLocal++; },
            []() {},
            []() {},
            []() {},
            instructionCount,
            emitInstruction,
            patchInstructionImm,
            error) == Result::Emitted);
  CHECK(error == "stale");
  CHECK_FALSE(instructions.empty());

  primec::Expr soaGetCall;
  soaGetCall.kind = primec::Expr::Kind::Call;
  soaGetCall.name = "get";
  soaGetCall.args = {soaName, idxName};
  instructions.clear();
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitNativeCallTailDispatch(
            soaGetCall,
            locals,
            [](const primec::Expr &, std::string &) { return false; },
            [](const std::string &) { return true; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) {
              return false;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, std::string &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return LocalInfo::ValueKind::Int32;
            },
            [&]() { return nextLocal++; },
            []() {},
            []() {},
            []() {},
            instructionCount,
            emitInstruction,
            patchInstructionImm,
            error) == Result::NotHandled);
  CHECK(error.empty());

  primec::Expr soaRefCall;
  soaRefCall.kind = primec::Expr::Kind::Call;
  soaRefCall.name = "ref";
  soaRefCall.args = {soaName, idxName};
  instructions.clear();
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitNativeCallTailDispatch(
            soaRefCall,
            locals,
            [](const primec::Expr &, std::string &) { return false; },
            [](const std::string &) { return true; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) {
              return false;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, std::string &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return LocalInfo::ValueKind::Int32;
            },
            [&]() { return nextLocal++; },
            []() {},
            []() {},
            []() {},
            instructionCount,
            emitInstruction,
            patchInstructionImm,
            error) == Result::NotHandled);
  CHECK(error.empty());

  primec::Expr printCall;
  printCall.kind = primec::Expr::Kind::Call;
  printCall.name = "print";
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitNativeCallTailDispatch(
            printCall,
            locals,
            [](const primec::Expr &, std::string &) { return false; },
            [](const std::string &) { return true; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) {
              return false;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, std::string &builtinName) {
              builtinName = "print";
              return true;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return LocalInfo::ValueKind::Int32;
            },
            [&]() { return nextLocal++; },
            []() {},
            []() {},
            []() {},
            instructionCount,
            emitInstruction,
            patchInstructionImm,
            error) == Result::Error);
  CHECK(error == "print is only supported as a statement in the native backend");

  primec::Expr badAccessCall;
  badAccessCall.kind = primec::Expr::Kind::Call;
  badAccessCall.name = "at";
  badAccessCall.args = {arrName};
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitNativeCallTailDispatch(
            badAccessCall,
            locals,
            [](const primec::Expr &, std::string &) { return false; },
            [](const std::string &) { return true; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) {
              return false;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, std::string &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return LocalInfo::ValueKind::Int32;
            },
            [&]() { return nextLocal++; },
            []() {},
            []() {},
            []() {},
            instructionCount,
            emitInstruction,
            patchInstructionImm,
            error) == Result::Error);
  CHECK(error == "at requires exactly two arguments");

  primec::Expr accessCall;
  accessCall.kind = primec::Expr::Kind::Call;
  accessCall.name = "at";
  accessCall.args = {arrName, idxName};
  instructions.clear();
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitNativeCallTailDispatch(
            accessCall,
            locals,
            [](const primec::Expr &, std::string &) { return false; },
            [](const std::string &) { return true; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) {
              return false;
            },
            [&](const primec::Expr &valueExpr, const primec::ir_lowerer::LocalMap &) {
              emitInstruction(primec::IrOpcode::LoadLocal, valueExpr.name == "arr" ? 9 : 3);
              return true;
            },
            [](const primec::Expr &, std::string &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return LocalInfo::ValueKind::Int32;
            },
            [&]() { return nextLocal++; },
            []() {},
            []() {},
            []() {},
            instructionCount,
            emitInstruction,
            patchInstructionImm,
            error) == Result::Emitted);
  CHECK(error.empty());
  CHECK_FALSE(instructions.empty());

  primec::Expr plainCall;
  plainCall.kind = primec::Expr::Kind::Call;
  plainCall.name = "plain";
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitNativeCallTailDispatch(
            plainCall,
            locals,
            [](const primec::Expr &, std::string &) { return false; },
            [](const std::string &) { return true; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) {
              return false;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const primec::Expr &, std::string &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              return LocalInfo::ValueKind::Int32;
            },
            [&]() { return nextLocal++; },
            []() {},
            []() {},
            []() {},
            instructionCount,
            emitInstruction,
            patchInstructionImm,
            error) == Result::NotHandled);
}

TEST_SUITE_END();
