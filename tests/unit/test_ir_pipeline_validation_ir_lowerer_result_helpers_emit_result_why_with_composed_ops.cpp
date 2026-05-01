#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir lowerer result helpers emit Result.why with composed ops") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;

  primec::Expr resultWhyExpr;
  resultWhyExpr.kind = primec::Expr::Kind::Call;
  resultWhyExpr.isMethodCall = true;
  resultWhyExpr.name = "why";
  resultWhyExpr.namespacePrefix = "/pkg";

  primec::Definition i32WhyDef;
  i32WhyDef.fullPath = "/i32/why";
  i32WhyDef.parameters.resize(1);
  std::unordered_map<std::string, const primec::Definition *> defMap{
      {"/i32/why", &i32WhyDef},
  };

  primec::ir_lowerer::ResultExprInfo resultInfo;
  resultInfo.isResult = true;
  resultInfo.errorType = "AnyInt";
  primec::ir_lowerer::LocalMap locals;
  std::string error;

  std::vector<primec::IrInstruction> instructions;
  int32_t tempCounter = 0;
  const primec::ir_lowerer::ResultWhyExprOps exprOps = primec::ir_lowerer::makeResultWhyExprOps(
      17,
      "/pkg",
      tempCounter,
      [&](const std::string &) { return 0; },
      [&]() { return 41; },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); });

  bool inlineCalled = false;
  bool fileErrorCalled = false;
  CHECK(primec::ir_lowerer::emitResultWhyCallWithComposedOps(
      resultWhyExpr,
      resultInfo,
      locals,
      17,
      defMap,
      exprOps,
      [](const std::string &, const std::string &, std::string &) { return false; },
      [](const std::string &path, primec::ir_lowerer::ReturnInfo &returnInfoOut) {
        if (path != "/i32/why") {
          return false;
        }
        returnInfoOut = primec::ir_lowerer::ReturnInfo{};
        returnInfoOut.kind = ValueKind::String;
        return true;
      },
      [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::Kind::Value; },
      [](const std::string &, primec::ir_lowerer::StructSlotLayoutInfo &) { return false; },
      [](const std::string &typeName) {
        if (typeName == "AnyInt") {
          return ValueKind::Int32;
        }
        return ValueKind::Unknown;
      },
      [&](const primec::Expr &callExpr, const primec::Definition &callee, const primec::ir_lowerer::LocalMap &) {
        inlineCalled = true;
        CHECK(callee.fullPath == "/i32/why");
        REQUIRE(callExpr.args.size() == 1);
        CHECK(callExpr.args.front().kind == primec::Expr::Kind::Name);
        CHECK(callExpr.args.front().name == "__result_why_err_0");
        return true;
      },
      [&](int32_t) {
        fileErrorCalled = true;
        return true;
      },
      error));
  CHECK(inlineCalled);
  CHECK_FALSE(fileErrorCalled);

  inlineCalled = false;
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::emitResultWhyCallWithComposedOps(
      resultWhyExpr,
      resultInfo,
      locals,
      17,
      defMap,
      exprOps,
      [](const std::string &, const std::string &, std::string &) { return false; },
      [](const std::string &, primec::ir_lowerer::ReturnInfo &returnInfoOut) {
        returnInfoOut = primec::ir_lowerer::ReturnInfo{};
        returnInfoOut.kind = ValueKind::Int32;
        return true;
      },
      [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::Kind::Value; },
      [](const std::string &, primec::ir_lowerer::StructSlotLayoutInfo &) { return false; },
      [](const std::string &) { return ValueKind::Int32; },
      [&](const primec::Expr &, const primec::Definition &, const primec::ir_lowerer::LocalMap &) {
        inlineCalled = true;
        return true;
      },
      [&](int32_t) { return true; },
      error));
  CHECK_FALSE(inlineCalled);
  CHECK(error == "Result.why requires a string-returning why() for AnyInt");
}

TEST_CASE("ir lowerer result helpers emit resolved Result.why calls") {
  using EmitResult = primec::ir_lowerer::ResultWhyCallEmitResult;
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;

  primec::Expr resultWhyExpr;
  resultWhyExpr.kind = primec::Expr::Kind::Call;
  resultWhyExpr.isMethodCall = true;
  resultWhyExpr.name = "why";
  resultWhyExpr.namespacePrefix = "/pkg";

  primec::Definition structWhyDef;
  structWhyDef.fullPath = "/MyError/why";
  structWhyDef.parameters.resize(1);
  primec::Definition i32WhyDef;
  i32WhyDef.fullPath = "/i32/why";
  i32WhyDef.parameters.resize(1);
  std::unordered_map<std::string, const primec::Definition *> defMap{
      {"/MyError/why", &structWhyDef},
      {"/i32/why", &i32WhyDef},
  };

  primec::ir_lowerer::LocalMap locals;
  std::string error;

  primec::ir_lowerer::ResultWhyCallOps ops;
  ops.resolveStructTypeName = [](const std::string &typeName,
                                 const std::string &,
                                 std::string &structPathOut) {
    if (typeName == "MyError") {
      structPathOut = "/MyError";
      return true;
    }
    return false;
  };
  ops.getReturnInfo = [](const std::string &path, primec::ir_lowerer::ReturnInfo &returnInfoOut) {
    if (path != "/MyError/why" && path != "/i32/why") {
      return false;
    }
    returnInfoOut = primec::ir_lowerer::ReturnInfo{};
    returnInfoOut.kind = ValueKind::String;
    return true;
  };
  ops.bindingKind = [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::Kind::Value; };
  ops.extractFirstBindingTypeTransform =
      [](const primec::Expr &, std::string &typeNameOut, std::vector<std::string> &templateArgsOut) {
        typeNameOut = "MyError";
        templateArgsOut.clear();
        return true;
      };
  ops.resolveStructSlotLayout =
      [](const std::string &structPath, primec::ir_lowerer::StructSlotLayoutInfo &layoutOut) {
    if (structPath != "/MyError") {
      return false;
    }
    layoutOut = primec::ir_lowerer::StructSlotLayoutInfo{};
    primec::ir_lowerer::StructSlotFieldInfo first;
    first.slotOffset = 0;
    first.slotCount = 1;
    first.valueKind = ValueKind::Int32;
    layoutOut.fields.push_back(first);
    primec::ir_lowerer::StructSlotFieldInfo second;
    second.slotOffset = 1;
    second.slotCount = 1;
    second.valueKind = ValueKind::Int32;
    layoutOut.fields.push_back(second);
    return true;
  };
  ops.valueKindFromTypeName = [](const std::string &typeName) {
    if (typeName == "AnyInt" || typeName == "MyError") {
      return ValueKind::Int32;
    }
    return ValueKind::Unknown;
  };
  ops.makeErrorValueExpr = [](primec::ir_lowerer::LocalMap &, ValueKind) {
    primec::Expr value;
    value.kind = primec::Expr::Kind::Name;
    value.name = "err";
    return value;
  };
  ops.makeBoolErrorExpr = [](primec::ir_lowerer::LocalMap &) {
    primec::Expr value;
    value.kind = primec::Expr::Kind::Name;
    value.name = "err_bool";
    return value;
  };

  int inlineCalls = 0;
  int fileErrorCalls = 0;
  int emptyStringCalls = 0;
  ops.emitInlineDefinitionCall = [&](const primec::Expr &callExpr,
                                     const primec::Definition &callee,
                                     const primec::ir_lowerer::LocalMap &) {
    ++inlineCalls;
    CHECK(callExpr.kind == primec::Expr::Kind::Call);
    CHECK(callee.fullPath == "/i32/why");
    return true;
  };
  ops.emitFileErrorWhy = [&](int32_t errorLocal) {
    ++fileErrorCalls;
    CHECK(errorLocal == 17);
    return true;
  };
  ops.emitEmptyString = [&]() {
    ++emptyStringCalls;
    return true;
  };

  primec::ir_lowerer::ResultExprInfo structResultInfo;
  structResultInfo.isResult = true;
  structResultInfo.errorType = "MyError";
  CHECK(primec::ir_lowerer::emitResolvedResultWhyCall(
            resultWhyExpr, structResultInfo, locals, 17, defMap, ops, error) ==
        EmitResult::Emitted);
  CHECK(emptyStringCalls == 1);
  CHECK(inlineCalls == 0);

  primec::ir_lowerer::ResultExprInfo i32ResultInfo;
  i32ResultInfo.isResult = true;
  i32ResultInfo.errorType = "AnyInt";
  error.clear();
  CHECK(primec::ir_lowerer::emitResolvedResultWhyCall(
            resultWhyExpr, i32ResultInfo, locals, 17, defMap, ops, error) ==
        EmitResult::Emitted);
  CHECK(inlineCalls == 1);

  ops.getReturnInfo = [](const std::string &, primec::ir_lowerer::ReturnInfo &returnInfoOut) {
    returnInfoOut = primec::ir_lowerer::ReturnInfo{};
    returnInfoOut.kind = ValueKind::Int32;
    return true;
  };
  error.clear();
  CHECK(primec::ir_lowerer::emitResolvedResultWhyCall(
            resultWhyExpr, i32ResultInfo, locals, 17, defMap, ops, error) ==
        EmitResult::Error);
  CHECK(error == "Result.why requires a string-returning why() for AnyInt");

  ops.resolveStructTypeName = [](const std::string &, const std::string &, std::string &) { return false; };
  ops.valueKindFromTypeName = [](const std::string &) { return ValueKind::Unknown; };
  ops.getReturnInfo = [](const std::string &, primec::ir_lowerer::ReturnInfo &) { return false; };
  primec::ir_lowerer::ResultExprInfo fileErrorResultInfo;
  fileErrorResultInfo.isResult = true;
  fileErrorResultInfo.errorType = "FileError";
  error.clear();
  CHECK(primec::ir_lowerer::emitResolvedResultWhyCall(
            resultWhyExpr, fileErrorResultInfo, locals, 17, defMap, ops, error) ==
        EmitResult::Emitted);
  CHECK(fileErrorCalls == 1);

  ops.emitFileErrorWhy = {};
  error.clear();
  CHECK(primec::ir_lowerer::emitResolvedResultWhyCall(
            resultWhyExpr, fileErrorResultInfo, locals, 17, defMap, ops, error) ==
        EmitResult::Error);
  CHECK(error == "FileError.why emitter is unavailable");
}

TEST_CASE("ir lowerer map insert helper writes grown pointers back through wrapper locals") {
  std::vector<primec::IrInstruction> instructions;
  int32_t nextLocal = 30;

  CHECK(primec::ir_lowerer::emitBuiltinCanonicalMapInsertOverwriteOrGrow(
      -1,
      9,
      3,
      4,
      5,
      primec::ir_lowerer::LocalInfo::ValueKind::Int32,
      [&]() { return nextLocal++; },
      [&]() { return instructions.size(); },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      [&](size_t indexToPatch, uint64_t target) { instructions[indexToPatch].imm = target; }));

  bool sawWrapperWriteBack = false;
  for (size_t i = 0; i + 3 < instructions.size(); ++i) {
    if (instructions[i].op != primec::IrOpcode::LoadLocal || instructions[i].imm != 9u) {
      continue;
    }
    if (instructions[i + 1].op != primec::IrOpcode::LoadLocal || instructions[i + 1].imm == 9u) {
      continue;
    }
    if (instructions[i + 2].op != primec::IrOpcode::StoreIndirect) {
      continue;
    }
    if (instructions[i + 3].op != primec::IrOpcode::Pop) {
      continue;
    }
    sawWrapperWriteBack = true;
    break;
  }

  CHECK(sawWrapperWriteBack);
}

TEST_CASE("ir lowerer flow helpers restore scoped state") {
  std::optional<primec::ir_lowerer::OnErrorHandler> onError;
  primec::ir_lowerer::OnErrorHandler initialHandler;
  initialHandler.handlerPath = "/initial";
  onError = initialHandler;
  {
    primec::ir_lowerer::OnErrorHandler nestedHandler;
    nestedHandler.handlerPath = "/nested";
    primec::ir_lowerer::OnErrorScope scope(onError, nestedHandler);
    REQUIRE(onError.has_value());
    CHECK(onError->handlerPath == "/nested");
  }
  REQUIRE(onError.has_value());
  CHECK(onError->handlerPath == "/initial");

  std::optional<primec::ir_lowerer::ResultReturnInfo> resultInfo;
  primec::ir_lowerer::ResultReturnInfo initialResult;
  initialResult.isResult = true;
  initialResult.hasValue = false;
  resultInfo = initialResult;
  {
    primec::ir_lowerer::ResultReturnInfo nestedResult;
    nestedResult.isResult = true;
    nestedResult.hasValue = true;
    primec::ir_lowerer::ResultReturnScope scope(resultInfo, nestedResult);
    REQUIRE(resultInfo.has_value());
    CHECK(resultInfo->hasValue);
  }
  REQUIRE(resultInfo.has_value());
  CHECK_FALSE(resultInfo->hasValue);
}

TEST_CASE("ir lowerer flow helpers emit file scope cleanup sequences") {
  auto checkCloseBlock = [](const std::vector<primec::IrInstruction> &instructions,
                            size_t start,
                            int32_t localIndex,
                            int32_t jumpImm) {
    REQUIRE(instructions.size() >= start + 7);
    CHECK(instructions[start + 0].op == primec::IrOpcode::LoadLocal);
    CHECK(instructions[start + 0].imm == static_cast<uint64_t>(localIndex));
    CHECK(instructions[start + 1].op == primec::IrOpcode::PushI64);
    CHECK(instructions[start + 1].imm == 0);
    CHECK(instructions[start + 2].op == primec::IrOpcode::CmpGeI64);
    CHECK(instructions[start + 3].op == primec::IrOpcode::JumpIfZero);
    CHECK(instructions[start + 3].imm == static_cast<uint64_t>(jumpImm));
    CHECK(instructions[start + 4].op == primec::IrOpcode::LoadLocal);
    CHECK(instructions[start + 4].imm == static_cast<uint64_t>(localIndex));
    CHECK(instructions[start + 5].op == primec::IrOpcode::FileClose);
    CHECK(instructions[start + 6].op == primec::IrOpcode::Pop);
  };

  std::vector<primec::IrInstruction> instructions;
  primec::ir_lowerer::emitFileCloseIfValid(instructions, 5);
  REQUIRE(instructions.size() == 7);
  checkCloseBlock(instructions, 0, 5, 7);

  instructions.clear();
  primec::ir_lowerer::emitFileScopeCleanup(instructions, {4, 9});
  REQUIRE(instructions.size() == 14);
  checkCloseBlock(instructions, 0, 9, 7);
  checkCloseBlock(instructions, 7, 4, 14);

  instructions.clear();
  primec::ir_lowerer::emitAllFileScopeCleanup(instructions, {{1}, {2, 3}});
  REQUIRE(instructions.size() == 21);
  checkCloseBlock(instructions, 0, 3, 7);
  checkCloseBlock(instructions, 7, 2, 14);
  checkCloseBlock(instructions, 14, 1, 21);

  instructions.clear();
  primec::ir_lowerer::emitFileScopeCleanup(instructions, {});
  CHECK(instructions.empty());
}

TEST_CASE("ir lowerer flow helpers emit struct copy sequences") {
  std::vector<primec::IrInstruction> instructions;

  CHECK(primec::ir_lowerer::emitStructCopyFromPtrs(instructions, 2, 3, 2));
  REQUIRE(instructions.size() == 14);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 2);
  CHECK(instructions[1].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[1].imm == 3);
  CHECK(instructions[2].op == primec::IrOpcode::LoadIndirect);
  CHECK(instructions[3].op == primec::IrOpcode::StoreIndirect);
  CHECK(instructions[4].op == primec::IrOpcode::Pop);
  CHECK(instructions[5].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[5].imm == 2);
  CHECK(instructions[6].op == primec::IrOpcode::PushI64);
  CHECK(instructions[6].imm == 16);
  CHECK(instructions[7].op == primec::IrOpcode::AddI64);
  CHECK(instructions[8].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[8].imm == 3);
  CHECK(instructions[9].op == primec::IrOpcode::PushI64);
  CHECK(instructions[9].imm == 16);
  CHECK(instructions[10].op == primec::IrOpcode::AddI64);
  CHECK(instructions[11].op == primec::IrOpcode::LoadIndirect);
  CHECK(instructions[12].op == primec::IrOpcode::StoreIndirect);
  CHECK(instructions[13].op == primec::IrOpcode::Pop);

  instructions.clear();
  int tempAllocs = 0;
  CHECK(primec::ir_lowerer::emitStructCopySlots(
      instructions,
      7,
      9,
      1,
      [&]() {
        tempAllocs++;
        return 11;
      }));
  CHECK(tempAllocs == 1);
  REQUIRE(instructions.size() == 7);
  CHECK(instructions[0].op == primec::IrOpcode::AddressOfLocal);
  CHECK(instructions[0].imm == 7);
  CHECK(instructions[1].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[1].imm == 11);
  CHECK(instructions[2].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[2].imm == 11);
  CHECK(instructions[3].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[3].imm == 9);
  CHECK(instructions[4].op == primec::IrOpcode::LoadIndirect);
  CHECK(instructions[5].op == primec::IrOpcode::StoreIndirect);
  CHECK(instructions[6].op == primec::IrOpcode::Pop);

  instructions.clear();
  CHECK(primec::ir_lowerer::emitStructCopyFromPtrs(instructions, 2, 3, 0));
  CHECK(instructions.empty());
}

TEST_CASE("ir lowerer flow helpers disarm soa storage temporaries after copy") {
  auto checkDisarmAt = [](const std::vector<primec::IrInstruction> &instructions,
                          size_t start,
                          uint64_t offsetBytes) {
    REQUIRE(instructions.size() >= start + 6);
    CHECK(instructions[start + 0].op == primec::IrOpcode::LoadLocal);
    CHECK(instructions[start + 0].imm == 12);
    CHECK(instructions[start + 1].op == primec::IrOpcode::PushI64);
    CHECK(instructions[start + 1].imm == offsetBytes);
    CHECK(instructions[start + 2].op == primec::IrOpcode::AddI64);
    CHECK(instructions[start + 3].op == primec::IrOpcode::PushI32);
    CHECK(instructions[start + 3].imm == 0);
    CHECK(instructions[start + 4].op == primec::IrOpcode::StoreIndirect);
    CHECK(instructions[start + 5].op == primec::IrOpcode::Pop);
  };

  std::vector<primec::IrInstruction> instructions;
  primec::ir_lowerer::emitDisarmTemporaryStructAfterCopy(
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      12,
      "/std/collections/internal_soa_storage/SoaColumn__ti32");
  REQUIRE(instructions.size() == 6);
  checkDisarmAt(instructions, 0, 4u * primec::IrSlotBytes);

  instructions.clear();
  primec::ir_lowerer::emitDisarmTemporaryStructAfterCopy(
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      12,
      "/std/collections/internal_soa_storage/SoaColumns2__ti32_i32");
  REQUIRE(instructions.size() == 12);
  checkDisarmAt(instructions, 0, 5u * primec::IrSlotBytes);
  checkDisarmAt(instructions, 6, 10u * primec::IrSlotBytes);

  instructions.clear();
  primec::ir_lowerer::emitDisarmTemporaryStructAfterCopy(
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      12,
      "/std/collections/experimental_soa_vector/SoaVector__tParticle");
  REQUIRE(instructions.size() == 6);
  checkDisarmAt(instructions, 0, 5u * primec::IrSlotBytes);
}

TEST_CASE("ir lowerer flow helpers classify borrowed struct copy sources") {
  primec::Expr localName;
  localName.kind = primec::Expr::Kind::Name;
  localName.name = "value";
  CHECK_FALSE(primec::ir_lowerer::shouldDisarmStructCopySourceExpr(localName));

  primec::Expr temporaryCall;
  temporaryCall.kind = primec::Expr::Kind::Call;
  temporaryCall.name = "make_value";
  CHECK(primec::ir_lowerer::shouldDisarmStructCopySourceExpr(temporaryCall));

  primec::Expr derefCall;
  derefCall.kind = primec::Expr::Kind::Call;
  derefCall.name = "dereference";
  derefCall.args.push_back(localName);
  CHECK_FALSE(primec::ir_lowerer::shouldDisarmStructCopySourceExpr(derefCall));

  primec::Expr locationCall;
  locationCall.kind = primec::Expr::Kind::Call;
  locationCall.name = "location";
  locationCall.args.push_back(localName);
  CHECK_FALSE(primec::ir_lowerer::shouldDisarmStructCopySourceExpr(locationCall));

  primec::Expr indexLiteral;
  indexLiteral.kind = primec::Expr::Kind::Literal;
  indexLiteral.literalValue = 0;
  primec::Expr accessCall;
  accessCall.kind = primec::Expr::Kind::Call;
  accessCall.name = "at";
  accessCall.args.push_back(localName);
  accessCall.args.push_back(indexLiteral);
  CHECK_FALSE(primec::ir_lowerer::shouldDisarmStructCopySourceExpr(accessCall));
}

TEST_CASE("ir lowerer flow helpers emit destroy helper calls from ptr locals") {
  const primec::Definition destroyHelper = [] {
    primec::Definition def;
    def.fullPath = "/thing/DestroyStack";
    return def;
  }();

  primec::ir_lowerer::LocalMap localsIn;
  primec::Expr capturedExpr;
  primec::ir_lowerer::LocalMap capturedLocals;
  const primec::Definition *capturedCallee = nullptr;
  bool capturedRequireValue = true;
  std::string error;

  CHECK(primec::ir_lowerer::emitDestroyHelperFromPtr(
      9,
      "/thing",
      &destroyHelper,
      localsIn,
      [&](const primec::Expr &expr,
          const primec::Definition &callee,
          const primec::ir_lowerer::LocalMap &callLocals,
          bool requireValue) {
        capturedExpr = expr;
        capturedLocals = callLocals;
        capturedCallee = &callee;
        capturedRequireValue = requireValue;
        return true;
      },
      error));
  CHECK(error.empty());
  REQUIRE(capturedCallee == &destroyHelper);
  CHECK_FALSE(capturedRequireValue);
  CHECK(capturedExpr.kind == primec::Expr::Kind::Call);
  CHECK(capturedExpr.isMethodCall);
  CHECK(capturedExpr.name == "DestroyStack");
  REQUIRE(capturedExpr.args.size() == 1u);
  CHECK(capturedExpr.args.front().kind == primec::Expr::Kind::Name);
  const std::string receiverName = capturedExpr.args.front().name;
  auto receiverIt = capturedLocals.find(receiverName);
  REQUIRE(receiverIt != capturedLocals.end());
  CHECK(receiverIt->second.index == 9);
  CHECK(receiverIt->second.kind == primec::ir_lowerer::LocalInfo::Kind::Reference);
  CHECK(receiverIt->second.isMutable);
  CHECK(receiverIt->second.structTypeName == "/thing");

  capturedExpr = {};
  capturedLocals.clear();
  capturedCallee = nullptr;
  CHECK(primec::ir_lowerer::emitDestroyHelperFromPtr(
      9,
      "/thing",
      nullptr,
      localsIn,
      [&](const primec::Expr &,
          const primec::Definition &,
          const primec::ir_lowerer::LocalMap &,
          bool) {
        return false;
      },
      error));
  CHECK(error.empty());
  CHECK(capturedCallee == nullptr);
}

TEST_CASE("ir lowerer flow helpers emit move helper calls from ptr locals") {
  const primec::Definition moveHelper = [] {
    primec::Definition def;
    def.fullPath = "/thing/Move";
    return def;
  }();

  primec::ir_lowerer::LocalMap localsIn;
  primec::Expr capturedExpr;
  primec::ir_lowerer::LocalMap capturedLocals;
  const primec::Definition *capturedCallee = nullptr;
  bool capturedRequireValue = true;
  std::string error;

  CHECK(primec::ir_lowerer::emitMoveHelperFromPtrs(
      9,
      13,
      "/thing",
      &moveHelper,
      localsIn,
      [&](const primec::Expr &expr,
          const primec::Definition &callee,
          const primec::ir_lowerer::LocalMap &callLocals,
          bool requireValue) {
        capturedExpr = expr;
        capturedLocals = callLocals;
        capturedCallee = &callee;
        capturedRequireValue = requireValue;
        return true;
      },
      error));
  CHECK(error.empty());
  REQUIRE(capturedCallee == &moveHelper);
  CHECK_FALSE(capturedRequireValue);
  CHECK(capturedExpr.kind == primec::Expr::Kind::Call);
  CHECK(capturedExpr.isMethodCall);
  CHECK(capturedExpr.name == "Move");
  REQUIRE(capturedExpr.args.size() == 2u);
  CHECK(capturedExpr.args.front().kind == primec::Expr::Kind::Name);
  CHECK(capturedExpr.args[1].kind == primec::Expr::Kind::Name);

  const std::string receiverName = capturedExpr.args.front().name;
  const std::string sourceName = capturedExpr.args[1].name;
  REQUIRE(capturedLocals.count(receiverName) == 1u);
  REQUIRE(capturedLocals.count(sourceName) == 1u);
  CHECK(capturedLocals.at(receiverName).index == 9);
  CHECK(capturedLocals.at(receiverName).kind == primec::ir_lowerer::LocalInfo::Kind::Reference);
  CHECK(capturedLocals.at(receiverName).structTypeName == "/thing");
  CHECK(capturedLocals.at(sourceName).index == 13);
  CHECK(capturedLocals.at(sourceName).kind == primec::ir_lowerer::LocalInfo::Kind::Reference);
  CHECK(capturedLocals.at(sourceName).structTypeName == "/thing");
}

TEST_CASE("ir lowerer flow helpers emit vector removed-slot destruction sequences") {
  const primec::Definition destroyHelper = [] {
    primec::Definition def;
    def.fullPath = "/thing/DestroyStack";
    return def;
  }();

  std::vector<primec::IrInstruction> instructions;
  primec::Expr capturedExpr;
  primec::ir_lowerer::LocalMap capturedLocals;
  const primec::Definition *capturedCallee = nullptr;
  int tempAllocs = 0;
  std::string error;

  CHECK(primec::ir_lowerer::emitVectorDestroySlot(
      instructions,
      4,
      5,
      primec::ir_lowerer::LocalInfo::ValueKind::Int32,
      "/thing",
      &destroyHelper,
      {},
      [&]() {
        tempAllocs++;
        return 11;
      },
      [&](const primec::Expr &expr,
          const primec::Definition &callee,
          const primec::ir_lowerer::LocalMap &callLocals,
          bool) {
        capturedExpr = expr;
        capturedLocals = callLocals;
        capturedCallee = &callee;
        return true;
      },
      error));
  CHECK(error.empty());
  CHECK(tempAllocs == 1);
  REQUIRE(instructions.size() == 6u);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 4u);
  CHECK(instructions[1].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[1].imm == 5u);
  CHECK(instructions[2].op == primec::IrOpcode::PushI32);
  CHECK(instructions[2].imm == primec::IrSlotBytes);
  CHECK(instructions[3].op == primec::IrOpcode::MulI32);
  CHECK(instructions[4].op == primec::IrOpcode::AddI64);
  CHECK(instructions[5].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[5].imm == 11u);
  REQUIRE(capturedCallee == &destroyHelper);
  REQUIRE(capturedExpr.args.size() == 1u);
  auto receiverIt = capturedLocals.find(capturedExpr.args.front().name);
  REQUIRE(receiverIt != capturedLocals.end());
  CHECK(receiverIt->second.index == 11);
  CHECK(receiverIt->second.structTypeName == "/thing");

  instructions.clear();
  tempAllocs = 0;
  CHECK(primec::ir_lowerer::emitVectorDestroySlot(
      instructions,
      4,
      5,
      primec::ir_lowerer::LocalInfo::ValueKind::Int32,
      "/thing",
      nullptr,
      {},
      [&]() {
        tempAllocs++;
        return 13;
      },
      [&](const primec::Expr &,
          const primec::Definition &,
          const primec::ir_lowerer::LocalMap &,
          bool) {
        return false;
      },
      error));
  CHECK(error.empty());
  CHECK(tempAllocs == 0);
  CHECK(instructions.empty());
}

TEST_CASE("ir lowerer flow helpers wire remove_swap through removed-slot destruction and survivor motion") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;

  primec::Expr stmt;
  stmt.kind = primec::Expr::Kind::Call;
  stmt.name = "remove_swap";

  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "values";
  stmt.args.push_back(receiverExpr);
  stmt.argNames.resize(1);

  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Literal;
  indexExpr.literalValue = 0;
  stmt.args.push_back(indexExpr);
  stmt.argNames.resize(2);

  primec::ir_lowerer::LocalMap localsIn;
  primec::ir_lowerer::LocalInfo valuesInfo;
  valuesInfo.index = 3;
  valuesInfo.isMutable = true;
  valuesInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Vector;
  valuesInfo.valueKind = ValueKind::Int32;
  valuesInfo.structTypeName = "/thing";
  localsIn.emplace("values", valuesInfo);

  primec::Definition destroyHelper;
  destroyHelper.fullPath = "/thing/DestroyStack";
  primec::Definition moveHelper;
  moveHelper.fullPath = "/thing/Move";

  std::vector<primec::IrInstruction> instructions;
  int32_t nextTemp = 10;
  primec::Expr capturedDestroyExpr;
  primec::ir_lowerer::LocalMap capturedDestroyLocals;
  const primec::Definition *capturedDestroyCallee = nullptr;
  primec::Expr capturedMoveExpr;
  primec::ir_lowerer::LocalMap capturedMoveLocals;
  const primec::Definition *capturedMoveCallee = nullptr;
  std::string error;

  const auto result = primec::ir_lowerer::tryEmitVectorStatementHelper(
      stmt,
      localsIn,
      instructions,
      [&]() { return nextTemp++; },
      [&](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
        if (expr.kind == primec::Expr::Kind::Literal) {
          return ValueKind::Int32;
        }
        return ValueKind::Unknown;
      },
      [&](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
        if (expr.kind == primec::Expr::Kind::Name && expr.name == "values") {
          return std::string("/thing");
        }
        return std::string();
      },
      [&](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
        if (expr.kind == primec::Expr::Kind::Name && expr.name == "values") {
          instructions.push_back({primec::IrOpcode::LoadLocal, 3});
          return true;
        }
        if (expr.kind == primec::Expr::Kind::Literal) {
          instructions.push_back({primec::IrOpcode::PushI32, 0});
          return true;
        }
        return false;
      },
      [&](const std::string &structPath) -> const primec::Definition * {
        if (structPath == "/thing") {
          return &destroyHelper;
        }
        return nullptr;
      },
      [&](const std::string &structPath) -> const primec::Definition * {
        if (structPath == "/thing") {
          return &moveHelper;
        }
        return nullptr;
      },
      [&](const primec::Expr &expr,
          const primec::Definition &callee,
          const primec::ir_lowerer::LocalMap &callLocals,
          bool requireValue) {
        if (callee.fullPath == moveHelper.fullPath) {
          capturedMoveExpr = expr;
          capturedMoveLocals = callLocals;
          capturedMoveCallee = &callee;
        } else {
          capturedDestroyExpr = expr;
          capturedDestroyLocals = callLocals;
          capturedDestroyCallee = &callee;
        }
        CHECK_FALSE(requireValue);
        return true;
      },
      [](const primec::Expr &) { return false; },
      []() {},
      []() {},
      []() {},
      []() {},
      []() {},
      error);

  CHECK(result == primec::ir_lowerer::VectorStatementHelperEmitResult::Emitted);
  CHECK(error.empty());
  REQUIRE(capturedDestroyCallee == &destroyHelper);
  REQUIRE(capturedMoveCallee == &moveHelper);
  CHECK(capturedDestroyExpr.kind == primec::Expr::Kind::Call);
  CHECK(capturedDestroyExpr.isMethodCall);
  CHECK(capturedDestroyExpr.name == "DestroyStack");
  REQUIRE(capturedDestroyExpr.args.size() == 1u);
  CHECK(capturedDestroyExpr.args.front().kind == primec::Expr::Kind::Name);
  const std::string receiverName = capturedDestroyExpr.args.front().name;
  REQUIRE(capturedDestroyLocals.count(receiverName) == 1u);
  CHECK(capturedDestroyLocals.at(receiverName).kind == primec::ir_lowerer::LocalInfo::Kind::Reference);
  CHECK(capturedDestroyLocals.at(receiverName).index == 18);
  CHECK(capturedDestroyLocals.at(receiverName).structTypeName == "/thing");
  CHECK(capturedMoveExpr.kind == primec::Expr::Kind::Call);
  CHECK(capturedMoveExpr.isMethodCall);
  CHECK(capturedMoveExpr.name == "Move");
  REQUIRE(capturedMoveExpr.args.size() == 2u);
  REQUIRE(capturedMoveLocals.count(capturedMoveExpr.args.front().name) == 1u);
  REQUIRE(capturedMoveLocals.count(capturedMoveExpr.args[1].name) == 1u);
  CHECK(capturedMoveLocals.at(capturedMoveExpr.args.front().name).index == 16);
  CHECK(capturedMoveLocals.at(capturedMoveExpr.args[1].name).index == 17);

  const auto destroyPtrStoreIt = std::find_if(
      instructions.begin(), instructions.end(), [](const primec::IrInstruction &inst) {
        return inst.op == primec::IrOpcode::StoreLocal && inst.imm == 15u;
      });
  REQUIRE(destroyPtrStoreIt != instructions.end());
  const auto moveDestPtrStoreIt = std::find_if(
      instructions.begin(), instructions.end(), [](const primec::IrInstruction &inst) {
        return inst.op == primec::IrOpcode::StoreLocal && inst.imm == 16u;
      });
  REQUIRE(moveDestPtrStoreIt != instructions.end());
  CHECK(std::find_if(instructions.begin(), instructions.end(), [](const primec::IrInstruction &inst) {
          return inst.op == primec::IrOpcode::JumpIfZero;
        }) != instructions.end());
}

TEST_CASE("ir lowerer flow helpers wire remove_at through removed-slot destruction and survivor motion") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;

  primec::Expr stmt;
  stmt.kind = primec::Expr::Kind::Call;
  stmt.name = "remove_at";

  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "values";
  stmt.args.push_back(receiverExpr);
  stmt.argNames.resize(1);

  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Literal;
  indexExpr.literalValue = 0;
  stmt.args.push_back(indexExpr);
  stmt.argNames.resize(2);

  primec::ir_lowerer::LocalMap localsIn;
  primec::ir_lowerer::LocalInfo valuesInfo;
  valuesInfo.index = 3;
  valuesInfo.isMutable = true;
  valuesInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Vector;
  valuesInfo.valueKind = ValueKind::Int32;
  valuesInfo.structTypeName = "/thing";
  localsIn.emplace("values", valuesInfo);

  primec::Definition destroyHelper;
  destroyHelper.fullPath = "/thing/DestroyStack";
  primec::Definition moveHelper;
  moveHelper.fullPath = "/thing/Move";

  std::vector<primec::IrInstruction> instructions;
  int32_t nextTemp = 10;
  primec::Expr capturedDestroyExpr;
  primec::ir_lowerer::LocalMap capturedDestroyLocals;
  const primec::Definition *capturedDestroyCallee = nullptr;
  primec::Expr capturedMoveExpr;
  primec::ir_lowerer::LocalMap capturedMoveLocals;
  const primec::Definition *capturedMoveCallee = nullptr;
  std::string error;

  const auto result = primec::ir_lowerer::tryEmitVectorStatementHelper(
      stmt,
      localsIn,
      instructions,
      [&]() { return nextTemp++; },
      [&](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
        if (expr.kind == primec::Expr::Kind::Literal) {
          return ValueKind::Int32;
        }
        return ValueKind::Unknown;
      },
      [&](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
        if (expr.kind == primec::Expr::Kind::Name && expr.name == "values") {
          return std::string("/thing");
        }
        return std::string();
      },
      [&](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
        if (expr.kind == primec::Expr::Kind::Name && expr.name == "values") {
          instructions.push_back({primec::IrOpcode::LoadLocal, 3});
          return true;
        }
        if (expr.kind == primec::Expr::Kind::Literal) {
          instructions.push_back({primec::IrOpcode::PushI32, 0});
          return true;
        }
        return false;
      },
      [&](const std::string &structPath) -> const primec::Definition * {
        if (structPath == "/thing") {
          return &destroyHelper;
        }
        return nullptr;
      },
      [&](const std::string &structPath) -> const primec::Definition * {
        if (structPath == "/thing") {
          return &moveHelper;
        }
        return nullptr;
      },
      [&](const primec::Expr &expr,
          const primec::Definition &callee,
          const primec::ir_lowerer::LocalMap &callLocals,
          bool requireValue) {
        if (callee.fullPath == moveHelper.fullPath) {
          capturedMoveExpr = expr;
          capturedMoveLocals = callLocals;
          capturedMoveCallee = &callee;
        } else {
          capturedDestroyExpr = expr;
          capturedDestroyLocals = callLocals;
          capturedDestroyCallee = &callee;
        }
        CHECK_FALSE(requireValue);
        return true;
      },
      [](const primec::Expr &) { return false; },
      []() {},
      []() {},
      []() {},
      []() {},
      []() {},
      error);

  CHECK(result == primec::ir_lowerer::VectorStatementHelperEmitResult::Emitted);
  CHECK(error.empty());
  REQUIRE(capturedDestroyCallee == &destroyHelper);
  REQUIRE(capturedMoveCallee == &moveHelper);
  CHECK(capturedDestroyExpr.kind == primec::Expr::Kind::Call);
  CHECK(capturedDestroyExpr.isMethodCall);
  CHECK(capturedDestroyExpr.name == "DestroyStack");
  REQUIRE(capturedDestroyExpr.args.size() == 1u);
  CHECK(capturedDestroyExpr.args.front().kind == primec::Expr::Kind::Name);
  const std::string receiverName = capturedDestroyExpr.args.front().name;
  REQUIRE(capturedDestroyLocals.count(receiverName) == 1u);
  CHECK(capturedDestroyLocals.at(receiverName).kind == primec::ir_lowerer::LocalInfo::Kind::Reference);
  CHECK(capturedDestroyLocals.at(receiverName).index == 15);
  CHECK(capturedDestroyLocals.at(receiverName).structTypeName == "/thing");
  CHECK(capturedMoveExpr.kind == primec::Expr::Kind::Call);
  CHECK(capturedMoveExpr.isMethodCall);
  CHECK(capturedMoveExpr.name == "Move");
  REQUIRE(capturedMoveExpr.args.size() == 2u);
  REQUIRE(capturedMoveLocals.count(capturedMoveExpr.args.front().name) == 1u);
  REQUIRE(capturedMoveLocals.count(capturedMoveExpr.args[1].name) == 1u);
  CHECK(capturedMoveLocals.at(capturedMoveExpr.args.front().name).index == 17);
  CHECK(capturedMoveLocals.at(capturedMoveExpr.args[1].name).index == 18);

  const auto destroyPtrStoreIt = std::find_if(
      instructions.begin(), instructions.end(), [](const primec::IrInstruction &inst) {
        return inst.op == primec::IrOpcode::StoreLocal && inst.imm == 15u;
      });
  REQUIRE(destroyPtrStoreIt != instructions.end());
  const auto moveDestPtrStoreIt = std::find_if(
      instructions.begin(), instructions.end(), [](const primec::IrInstruction &inst) {
        return inst.op == primec::IrOpcode::StoreLocal && inst.imm == 17u;
      });
  REQUIRE(moveDestPtrStoreIt != instructions.end());
  CHECK(std::find_if(instructions.begin(), instructions.end(), [](const primec::IrInstruction &inst) {
          return inst.op == primec::IrOpcode::JumpIfZero;
        }) != instructions.end());
}

TEST_CASE("ir lowerer flow helpers emit compare-to-zero sequences") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;
  std::vector<primec::IrInstruction> instructions;
  std::string error;

  CHECK(primec::ir_lowerer::emitCompareToZero(instructions, ValueKind::Int64, true, error));
  REQUIRE(instructions.size() == 2u);
  CHECK(instructions[0].op == primec::IrOpcode::PushI64);
  CHECK(instructions[0].imm == 0u);
  CHECK(instructions[1].op == primec::IrOpcode::CmpEqI64);
  CHECK(error.empty());

  instructions.clear();
  CHECK(primec::ir_lowerer::emitCompareToZero(instructions, ValueKind::Float32, false, error));
  REQUIRE(instructions.size() == 2u);
  CHECK(instructions[0].op == primec::IrOpcode::PushF32);
  CHECK(instructions[1].op == primec::IrOpcode::CmpNeF32);
  CHECK(error.empty());

  instructions.clear();
  CHECK(primec::ir_lowerer::emitCompareToZero(instructions, ValueKind::Bool, true, error));
  REQUIRE(instructions.size() == 2u);
  CHECK(instructions[0].op == primec::IrOpcode::PushI32);
  CHECK(instructions[1].op == primec::IrOpcode::CmpEqI32);

  instructions.clear();
  CHECK_FALSE(primec::ir_lowerer::emitCompareToZero(instructions, ValueKind::String, false, error));
  CHECK(error == "boolean conversion requires numeric operand");
  CHECK(instructions.empty());
}

TEST_CASE("ir lowerer comparison helpers treat builtin comparisons as bool conditions") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;
  primec::Expr lhs;
  lhs.kind = primec::Expr::Kind::Literal;
  lhs.literalValue = 1;
  lhs.intWidth = 32;

  primec::Expr rhs = lhs;

  primec::Expr equalExpr;
  equalExpr.kind = primec::Expr::Kind::Call;
  equalExpr.name = "equal";
  equalExpr.args = {lhs, rhs};

  primec::Expr notExpr;
  notExpr.kind = primec::Expr::Kind::Call;
  notExpr.name = "not";
  notExpr.args = {equalExpr};

  std::vector<primec::IrInstruction> instructions;
  std::string error;
  const auto result = primec::ir_lowerer::emitComparisonOperatorExpr(
      notExpr,
      {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return ValueKind::Unknown;
      },
      [](ValueKind left, ValueKind right) {
        return primec::ir_lowerer::comparisonKind(left, right);
      },
      [&](ValueKind kind, bool equals) {
        return primec::ir_lowerer::emitCompareToZero(instructions, kind, equals, error);
      },
      instructions,
      error);

  CHECK(result == primec::ir_lowerer::OperatorComparisonEmitResult::Handled);
  CHECK(error.empty());
  REQUIRE(instructions.size() == 2u);
  CHECK(instructions[0].op == primec::IrOpcode::PushI32);
  CHECK(instructions[0].imm == 0u);
  CHECK(instructions[1].op == primec::IrOpcode::CmpEqI32);
}

TEST_CASE("ir lowerer flow helpers emit float literal sequences") {
  std::vector<primec::IrInstruction> instructions;
  std::string error;

  primec::Expr float64Expr;
  float64Expr.kind = primec::Expr::Kind::FloatLiteral;
  float64Expr.floatValue = "1.5";
  float64Expr.floatWidth = 64;
  CHECK(primec::ir_lowerer::emitFloatLiteral(instructions, float64Expr, error));
  REQUIRE(instructions.size() == 1u);
  CHECK(instructions[0].op == primec::IrOpcode::PushF64);
  CHECK(instructions[0].imm == 0x3ff8000000000000ull);
  CHECK(error.empty());

  instructions.clear();
  primec::Expr float32Expr;
  float32Expr.kind = primec::Expr::Kind::FloatLiteral;
  float32Expr.floatValue = "2.5";
  float32Expr.floatWidth = 32;
  CHECK(primec::ir_lowerer::emitFloatLiteral(instructions, float32Expr, error));
  REQUIRE(instructions.size() == 1u);
  CHECK(instructions[0].op == primec::IrOpcode::PushF32);
  CHECK(instructions[0].imm == 0x40200000ull);
  CHECK(error.empty());

  instructions.clear();
  primec::Expr invalidExpr = float64Expr;
  invalidExpr.floatValue = "not_a_float";
  CHECK_FALSE(primec::ir_lowerer::emitFloatLiteral(instructions, invalidExpr, error));
  CHECK(error == "invalid float literal");
  CHECK(instructions.empty());
}

TEST_CASE("ir lowerer flow helpers emit return for definition") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;
  std::vector<primec::IrInstruction> instructions;
  std::string error;

  primec::ir_lowerer::ReturnInfo info;
  info.returnsVoid = true;
  CHECK(primec::ir_lowerer::emitReturnForDefinition(instructions, "/pkg/voidFn", info, error));
  REQUIRE(instructions.size() == 1u);
  CHECK(instructions.back().op == primec::IrOpcode::ReturnVoid);

  info = primec::ir_lowerer::ReturnInfo{};
  info.kind = ValueKind::Int32;
  CHECK(primec::ir_lowerer::emitReturnForDefinition(instructions, "/pkg/i32Fn", info, error));
  REQUIRE(instructions.size() == 2u);
  CHECK(instructions.back().op == primec::IrOpcode::ReturnI32);

  info = primec::ir_lowerer::ReturnInfo{};
  info.kind = ValueKind::Int64;
  CHECK(primec::ir_lowerer::emitReturnForDefinition(instructions, "/pkg/i64Fn", info, error));
  REQUIRE(instructions.size() == 3u);
  CHECK(instructions.back().op == primec::IrOpcode::ReturnI64);

  info = primec::ir_lowerer::ReturnInfo{};
  info.kind = ValueKind::Float32;
  CHECK(primec::ir_lowerer::emitReturnForDefinition(instructions, "/pkg/f32Fn", info, error));
  REQUIRE(instructions.size() == 4u);
  CHECK(instructions.back().op == primec::IrOpcode::ReturnF32);

  info = primec::ir_lowerer::ReturnInfo{};
  info.kind = ValueKind::Float64;
  CHECK(primec::ir_lowerer::emitReturnForDefinition(instructions, "/pkg/f64Fn", info, error));
  REQUIRE(instructions.size() == 5u);
  CHECK(instructions.back().op == primec::IrOpcode::ReturnF64);

  info = primec::ir_lowerer::ReturnInfo{};
  info.returnsArray = true;
  info.kind = ValueKind::Unknown;
  CHECK(primec::ir_lowerer::emitReturnForDefinition(instructions, "/pkg/arrayFn", info, error));
  REQUIRE(instructions.size() == 6u);
  CHECK(instructions.back().op == primec::IrOpcode::ReturnI64);

  const size_t beforeFailure = instructions.size();
  info = primec::ir_lowerer::ReturnInfo{};
  info.kind = ValueKind::Unknown;
  CHECK_FALSE(primec::ir_lowerer::emitReturnForDefinition(instructions, "/pkg/badFn", info, error));
  CHECK(error == "native backend does not support return type on /pkg/badFn");
  CHECK(instructions.size() == beforeFailure);
}

TEST_CASE("ir lowerer flow helpers resolve and emit gpu builtins") {
  CHECK(primec::ir_lowerer::resolveGpuBuiltinLocalName("global_id_x") != nullptr);
  CHECK(std::string(primec::ir_lowerer::resolveGpuBuiltinLocalName("global_id_x")) == "__gpu_global_id_x");
  CHECK(std::string(primec::ir_lowerer::resolveGpuBuiltinLocalName("global_id_y")) == "__gpu_global_id_y");
  CHECK(std::string(primec::ir_lowerer::resolveGpuBuiltinLocalName("global_id_z")) == "__gpu_global_id_z");
  CHECK(primec::ir_lowerer::resolveGpuBuiltinLocalName("global_id_w") == nullptr);

  std::vector<primec::IrInstruction> instructions;
  auto emitInstruction = [&](primec::IrOpcode op, uint64_t imm) {
    instructions.push_back({op, imm});
  };

  std::string error;
  CHECK(primec::ir_lowerer::emitGpuBuiltinLoad(
      "global_id_y",
      [](const char *localName) -> std::optional<int32_t> {
        if (std::string(localName) == "__gpu_global_id_y") {
          return 42;
        }
        return std::nullopt;
      },
      emitInstruction,
      error));
  CHECK(error.empty());
  REQUIRE(instructions.size() == 1);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 42);

  instructions.clear();
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::emitGpuBuiltinLoad(
      "global_id_w",
      [](const char *) -> std::optional<int32_t> { return 0; },
      emitInstruction,
      error));
  CHECK(error == "native backend does not support gpu builtin: global_id_w");
  CHECK(instructions.empty());

  error.clear();
  CHECK_FALSE(primec::ir_lowerer::emitGpuBuiltinLoad(
      "global_id_x",
      [](const char *) -> std::optional<int32_t> { return std::nullopt; },
      emitInstruction,
      error));
  CHECK(error == "gpu builtin requires dispatch context: global_id_x");
  CHECK(instructions.empty());
}

TEST_CASE("ir lowerer flow helpers match and emit unary passthrough calls") {
  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "upload";

  primec::Expr argExpr;
  argExpr.kind = primec::Expr::Kind::Literal;
  argExpr.intWidth = 32;
  argExpr.literalValue = 7;
  callExpr.args.push_back(argExpr);

  bool emitCalled = false;
  std::string error;
  CHECK(primec::ir_lowerer::tryEmitUnaryPassthroughCall(
            callExpr,
            "upload",
            [&](const primec::Expr &forwardedExpr) {
              emitCalled = true;
              CHECK(forwardedExpr.kind == primec::Expr::Kind::Literal);
              CHECK(forwardedExpr.literalValue == 7u);
              return true;
            },
            error) == primec::ir_lowerer::UnaryPassthroughCallResult::Emitted);
  CHECK(emitCalled);
  CHECK(error.empty());

  primec::Expr notMatchedExpr = callExpr;
  notMatchedExpr.name = "buffer";
  emitCalled = false;
  CHECK(primec::ir_lowerer::tryEmitUnaryPassthroughCall(
            notMatchedExpr,
            "upload",
            [&](const primec::Expr &) {
              emitCalled = true;
              return true;
            },
            error) == primec::ir_lowerer::UnaryPassthroughCallResult::NotMatched);
  CHECK_FALSE(emitCalled);
  CHECK(error.empty());

  primec::Expr wrongArityExpr = callExpr;
  wrongArityExpr.name = "readback";
  wrongArityExpr.args.push_back(argExpr);
  emitCalled = false;
  error.clear();
  CHECK(primec::ir_lowerer::tryEmitUnaryPassthroughCall(
            wrongArityExpr,
            "readback",
            [&](const primec::Expr &) {
              emitCalled = true;
              return true;
            },
            error) == primec::ir_lowerer::UnaryPassthroughCallResult::Error);
  CHECK_FALSE(emitCalled);
  CHECK(error == "readback requires exactly one argument");

  error.clear();
  CHECK(primec::ir_lowerer::tryEmitUnaryPassthroughCall(
            callExpr,
            "upload",
            [&](const primec::Expr &) {
              error = "emit failure";
              return false;
            },
            error) == primec::ir_lowerer::UnaryPassthroughCallResult::Error);
  CHECK(error == "emit failure");
}

TEST_CASE("ir lowerer flow helpers resolve counted loop kinds") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;

  ValueKind countKind = ValueKind::Unknown;
  std::string error;
  CHECK(primec::ir_lowerer::resolveCountedLoopKind(
      ValueKind::Int32,
      false,
      "loop count requires integer",
      countKind,
      error));
  CHECK(countKind == ValueKind::Int32);

  CHECK(primec::ir_lowerer::resolveCountedLoopKind(
      ValueKind::Bool,
      true,
      "repeat count requires integer or bool",
      countKind,
      error));
  CHECK(countKind == ValueKind::Int32);

  error.clear();
  CHECK_FALSE(primec::ir_lowerer::resolveCountedLoopKind(
      ValueKind::Float64,
      false,
      "loop count requires integer",
      countKind,
      error));
  CHECK(error == "loop count requires integer");
}

TEST_SUITE_END();
