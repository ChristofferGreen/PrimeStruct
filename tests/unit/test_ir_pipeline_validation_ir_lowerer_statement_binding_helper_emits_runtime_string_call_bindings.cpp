#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir lowerer statement binding helper emits runtime string call bindings") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;

  primec::Expr stmt;
  stmt.name = "label";
  stmt.isBinding = true;

  primec::Expr init;
  init.kind = primec::Expr::Kind::Call;
  init.name = "mapAt";

  primec::ir_lowerer::LocalMap locals;
  int32_t nextLocal = 4;
  std::vector<primec::IrInstruction> instructions;
  int emitExprCalls = 0;
  int boundsCalls = 0;
  std::string error;
  REQUIRE(primec::ir_lowerer::emitStringStatementBindingInitializer(
      stmt,
      init,
      locals,
      nextLocal,
      instructions,
      [](const primec::Expr &) { return false; },
      [](const std::string &) { return 0; },
      [&](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
        ++emitExprCalls;
        CHECK(expr.kind == primec::Expr::Kind::Call);
        instructions.push_back({primec::IrOpcode::PushI64, 77});
        return true;
      },
      [](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
        if (expr.kind == primec::Expr::Kind::Call && expr.name == "mapAt") {
          return ValueKind::String;
        }
        return ValueKind::Unknown;
      },
      []() { return 90; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [&]() { ++boundsCalls; },
      error));
  CHECK(error.empty());
  CHECK(emitExprCalls == 1);
  CHECK(boundsCalls == 0);
  CHECK(nextLocal == 5);
  auto it = locals.find("label");
  REQUIRE(it != locals.end());
  CHECK(it->second.index == 4);
  CHECK(it->second.valueKind == ValueKind::String);
  CHECK(it->second.stringSource == primec::ir_lowerer::LocalInfo::StringSource::RuntimeIndex);
  CHECK(it->second.stringIndex == -1);
  REQUIRE(instructions.size() == 2);
  CHECK(instructions[0].op == primec::IrOpcode::PushI64);
  CHECK(instructions[0].imm == 77);
  CHECK(instructions[1].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[1].imm == 4);
}

TEST_CASE("ir lowerer statement binding helper emits runtime string access bindings") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;

  primec::Expr stmt;
  stmt.name = "label";
  stmt.isBinding = true;

  primec::Expr keysExpr;
  keysExpr.kind = primec::Expr::Kind::Name;
  keysExpr.name = "keys";
  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Literal;
  indexExpr.intWidth = 32;
  indexExpr.literalValue = 1;
  primec::Expr init;
  init.kind = primec::Expr::Kind::Call;
  init.name = "at";
  init.args = {keysExpr, indexExpr};

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo keysInfo;
  keysInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Vector;
  keysInfo.valueKind = ValueKind::String;
  keysInfo.index = 2;
  locals.emplace("keys", keysInfo);

  int32_t nextLocal = 4;
  std::vector<primec::IrInstruction> instructions;
  int emitExprCalls = 0;
  int boundsCalls = 0;
  std::string error;
  REQUIRE(primec::ir_lowerer::emitStringStatementBindingInitializer(
      stmt,
      init,
      locals,
      nextLocal,
      instructions,
      [](const primec::Expr &) { return false; },
      [](const std::string &) { return 0; },
      [&](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
        ++emitExprCalls;
        CHECK(expr.kind == primec::Expr::Kind::Call);
        CHECK(expr.name == "at");
        instructions.push_back({primec::IrOpcode::PushI64, 91});
        return true;
      },
      [](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
        if (expr.kind == primec::Expr::Kind::Call && expr.name == "at") {
          return ValueKind::String;
        }
        if (expr.kind == primec::Expr::Kind::Literal) {
          return ValueKind::Int32;
        }
        return ValueKind::Unknown;
      },
      []() { return 90; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [&]() { ++boundsCalls; },
      error));
  CHECK(error.empty());
  CHECK(emitExprCalls == 1);
  CHECK(boundsCalls == 0);
  CHECK(nextLocal == 5);
  auto it = locals.find("label");
  REQUIRE(it != locals.end());
  CHECK(it->second.index == 4);
  CHECK(it->second.valueKind == ValueKind::String);
  CHECK(it->second.stringSource == primec::ir_lowerer::LocalInfo::StringSource::RuntimeIndex);
  CHECK(it->second.stringIndex == -1);
  REQUIRE(instructions.size() == 2);
  CHECK(instructions[0].op == primec::IrOpcode::PushI64);
  CHECK(instructions[0].imm == 91);
  CHECK(instructions[1].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[1].imm == 4);
}

TEST_CASE("ir lowerer statement binding helper rejects non-string source bindings") {
  primec::Expr stmt;
  stmt.name = "label";
  stmt.isBinding = true;

  primec::Expr init;
  init.kind = primec::Expr::Kind::Name;
  init.name = "src";

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo srcInfo;
  srcInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  srcInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  srcInfo.stringSource = primec::ir_lowerer::LocalInfo::StringSource::None;
  locals.emplace("src", srcInfo);

  int32_t nextLocal = 3;
  std::vector<primec::IrInstruction> instructions;
  std::string error;
  CHECK_FALSE(primec::ir_lowerer::emitStringStatementBindingInitializer(
      stmt,
      init,
      locals,
      nextLocal,
      instructions,
      [](const primec::Expr &) { return false; },
      [](const std::string &) { return 0; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      []() { return 0; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      []() {},
      error));
  CHECK(error == "native backend requires string arguments to use string literals, bindings, or entry args");
}

TEST_CASE("ir lowerer statement binding helper emits uninitialized local init and drop") {
  using EmitResult = primec::ir_lowerer::UninitializedStorageInitDropEmitResult;

  primec::Expr storageExpr;
  storageExpr.kind = primec::Expr::Kind::Name;
  storageExpr.name = "slot";

  primec::Expr valueExpr;
  valueExpr.kind = primec::Expr::Kind::Literal;
  valueExpr.intWidth = 32;
  valueExpr.literalValue = 77;

  primec::Expr initCall;
  initCall.kind = primec::Expr::Kind::Call;
  initCall.name = "init";
  initCall.args = {storageExpr, valueExpr};

  primec::Expr dropCall;
  dropCall.kind = primec::Expr::Kind::Call;
  dropCall.name = "drop";
  dropCall.args = {storageExpr};

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo storageInfo;
  storageInfo.index = 8;
  storageInfo.isUninitializedStorage = true;

  std::vector<primec::IrInstruction> instructions;
  int32_t nextTempLocal = 50;
  int structCopyCalls = 0;
  std::string error;

  auto resolveAccess = [&](const primec::Expr &storage,
                           const primec::ir_lowerer::LocalMap &,
                           primec::ir_lowerer::UninitializedStorageAccessInfo &access,
                           bool &resolved) {
    if (storage.kind == primec::Expr::Kind::Name && storage.name == "slot") {
      access.location = primec::ir_lowerer::UninitializedStorageAccessInfo::Location::Local;
      access.local = &storageInfo;
      resolved = true;
      return true;
    }
    resolved = false;
    return true;
  };

  CHECK(primec::ir_lowerer::tryEmitUninitializedStorageInitDropStatement(
            initCall,
            locals,
            instructions,
            resolveAccess,
            [&](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
              instructions.push_back({primec::IrOpcode::PushI32, expr.literalValue});
              return true;
            },
            [](const std::string &, primec::ir_lowerer::StructSlotLayoutInfo &) { return false; },
            [&]() { return nextTempLocal++; },
            [&](int32_t, int32_t, int32_t) {
              ++structCopyCalls;
              return true;
            },
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(structCopyCalls == 0);
  REQUIRE(instructions.size() == 2);
  CHECK(instructions[0].op == primec::IrOpcode::PushI32);
  CHECK(instructions[0].imm == 77);
  CHECK(instructions[1].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[1].imm == 8);

  instructions.clear();
  CHECK(primec::ir_lowerer::tryEmitUninitializedStorageInitDropStatement(
            dropCall,
            locals,
            instructions,
            resolveAccess,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const std::string &, primec::ir_lowerer::StructSlotLayoutInfo &) { return false; },
            [&]() { return nextTempLocal++; },
            [&](int32_t, int32_t, int32_t) {
              ++structCopyCalls;
              return true;
            },
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(instructions.empty());
}

TEST_CASE("ir lowerer statement binding helper emits struct-storage init via ptr copy") {
  using EmitResult = primec::ir_lowerer::UninitializedStorageInitDropEmitResult;

  primec::Expr storageExpr;
  storageExpr.kind = primec::Expr::Kind::Name;
  storageExpr.name = "slot";

  primec::Expr valueExpr;
  valueExpr.kind = primec::Expr::Kind::Name;
  valueExpr.name = "srcPtr";

  primec::Expr initCall;
  initCall.kind = primec::Expr::Kind::Call;
  initCall.name = "init";
  initCall.args = {storageExpr, valueExpr};

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo storageInfo;
  storageInfo.index = 11;
  storageInfo.structTypeName = "/Pair";

  std::vector<primec::IrInstruction> instructions;
  int32_t nextTempLocal = 30;
  int copiedSlotCount = 0;
  std::string error;

  CHECK(primec::ir_lowerer::tryEmitUninitializedStorageInitDropStatement(
            initCall,
            locals,
            instructions,
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &, primec::ir_lowerer::UninitializedStorageAccessInfo &access,
                bool &resolved) {
              access.location = primec::ir_lowerer::UninitializedStorageAccessInfo::Location::Local;
              access.local = &storageInfo;
              resolved = true;
              return true;
            },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              instructions.push_back({primec::IrOpcode::PushI64, 1234});
              return true;
            },
            [](const std::string &structPath, primec::ir_lowerer::StructSlotLayoutInfo &layout) {
              if (structPath != "/Pair") {
                return false;
              }
              layout.totalSlots = 3;
              return true;
            },
            [&]() { return nextTempLocal++; },
            [&](int32_t destPtrLocal, int32_t srcPtrLocal, int32_t slotCount) {
              copiedSlotCount = slotCount;
              instructions.push_back({primec::IrOpcode::PushI32, static_cast<uint64_t>(destPtrLocal)});
              instructions.push_back({primec::IrOpcode::PushI32, static_cast<uint64_t>(srcPtrLocal)});
              return true;
            },
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(copiedSlotCount == 3);
  CHECK_FALSE(instructions.empty());
}

TEST_CASE("ir lowerer statement binding helper emits indirect uninitialized init") {
  using EmitResult = primec::ir_lowerer::UninitializedStorageInitDropEmitResult;

  primec::Expr pointerExpr;
  pointerExpr.kind = primec::Expr::Kind::Name;
  pointerExpr.name = "ptr";
  primec::Expr storageExpr;
  storageExpr.kind = primec::Expr::Kind::Call;
  storageExpr.name = "dereference";
  storageExpr.args = {pointerExpr};

  primec::Expr valueExpr;
  valueExpr.kind = primec::Expr::Kind::Literal;
  valueExpr.intWidth = 32;
  valueExpr.literalValue = 77;

  primec::Expr initCall;
  initCall.kind = primec::Expr::Kind::Call;
  initCall.name = "init";
  initCall.args = {storageExpr, valueExpr};

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo pointerInfo;
  pointerInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Pointer;
  pointerInfo.index = 9;
  pointerInfo.targetsUninitializedStorage = true;
  locals.emplace("ptr", pointerInfo);

  std::vector<primec::IrInstruction> instructions;
  int32_t nextTempLocal = 50;
  std::string error;
  CHECK(primec::ir_lowerer::tryEmitUninitializedStorageInitDropStatement(
            initCall,
            locals,
            instructions,
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &, primec::ir_lowerer::UninitializedStorageAccessInfo &access,
                bool &resolved) {
              access.location = primec::ir_lowerer::UninitializedStorageAccessInfo::Location::Indirect;
              access.pointer = &locals.find("ptr")->second;
              access.pointerExpr = &storageExpr.args.front();
              access.typeInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
              resolved = true;
              return true;
            },
            [&](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
              instructions.push_back({primec::IrOpcode::PushI32, static_cast<uint64_t>(expr.literalValue)});
              return true;
            },
            [](const std::string &, primec::ir_lowerer::StructSlotLayoutInfo &) { return false; },
            [&]() { return nextTempLocal++; },
            [](int32_t, int32_t, int32_t) { return true; },
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  REQUIRE(instructions.size() == 8);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 9);
  CHECK(instructions[1].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[2].op == primec::IrOpcode::PushI32);
  CHECK(instructions[2].imm == 77);
  CHECK(instructions[4].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[5].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[6].op == primec::IrOpcode::StoreIndirect);
  CHECK(instructions[7].op == primec::IrOpcode::Pop);
}

TEST_CASE("ir lowerer statement binding helper emits indirect struct init via ptr copy") {
  using EmitResult = primec::ir_lowerer::UninitializedStorageInitDropEmitResult;

  primec::Expr refExpr;
  refExpr.kind = primec::Expr::Kind::Name;
  refExpr.name = "ref";
  primec::Expr storageExpr;
  storageExpr.kind = primec::Expr::Kind::Call;
  storageExpr.name = "dereference";
  storageExpr.args = {refExpr};

  primec::Expr valueExpr;
  valueExpr.kind = primec::Expr::Kind::Name;
  valueExpr.name = "srcPtr";

  primec::Expr initCall;
  initCall.kind = primec::Expr::Kind::Call;
  initCall.name = "init";
  initCall.args = {storageExpr, valueExpr};

  primec::ir_lowerer::LocalMap locals;
  primec::ir_lowerer::LocalInfo refInfo;
  refInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Reference;
  refInfo.index = 11;
  refInfo.targetsUninitializedStorage = true;
  refInfo.structTypeName = "/Pair";
  locals.emplace("ref", refInfo);

  std::vector<primec::IrInstruction> instructions;
  int32_t nextTempLocal = 30;
  int copiedSlotCount = 0;
  std::string error;
  CHECK(primec::ir_lowerer::tryEmitUninitializedStorageInitDropStatement(
            initCall,
            locals,
            instructions,
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &, primec::ir_lowerer::UninitializedStorageAccessInfo &access,
                bool &resolved) {
              access.location = primec::ir_lowerer::UninitializedStorageAccessInfo::Location::Indirect;
              access.pointer = &locals.find("ref")->second;
              access.pointerExpr = &storageExpr.args.front();
              access.typeInfo.structPath = "/Pair";
              resolved = true;
              return true;
            },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              instructions.push_back({primec::IrOpcode::PushI64, 1234});
              return true;
            },
            [](const std::string &structPath, primec::ir_lowerer::StructSlotLayoutInfo &layout) {
              if (structPath != "/Pair") {
                return false;
              }
              layout.totalSlots = 3;
              return true;
            },
            [&]() { return nextTempLocal++; },
            [&](int32_t destPtrLocal, int32_t srcPtrLocal, int32_t slotCount) {
              copiedSlotCount = slotCount;
              instructions.push_back({primec::IrOpcode::PushI32, static_cast<uint64_t>(destPtrLocal)});
              instructions.push_back({primec::IrOpcode::PushI32, static_cast<uint64_t>(srcPtrLocal)});
              return true;
            },
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(copiedSlotCount == 3);
  CHECK_FALSE(instructions.empty());
}

TEST_CASE("ir lowerer statement binding helper validates uninitialized init drop diagnostics") {
  using EmitResult = primec::ir_lowerer::UninitializedStorageInitDropEmitResult;

  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "init";
  callExpr.args.push_back(primec::Expr{});

  primec::ir_lowerer::LocalMap locals;
  std::vector<primec::IrInstruction> instructions;
  std::string error;

  CHECK(primec::ir_lowerer::tryEmitUninitializedStorageInitDropStatement(
            callExpr,
            locals,
            instructions,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, primec::ir_lowerer::UninitializedStorageAccessInfo &, bool &) {
              return true;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const std::string &, primec::ir_lowerer::StructSlotLayoutInfo &) { return true; },
            []() { return 0; },
            [](int32_t, int32_t, int32_t) { return true; },
            error) == EmitResult::Error);
  CHECK(error == "init requires exactly 2 arguments");

  primec::Expr dropCall;
  dropCall.kind = primec::Expr::Kind::Call;
  dropCall.name = "drop";
  primec::Expr storageExpr;
  storageExpr.kind = primec::Expr::Kind::Name;
  storageExpr.name = "x";
  dropCall.args = {storageExpr};

  error.clear();
  CHECK(primec::ir_lowerer::tryEmitUninitializedStorageInitDropStatement(
            dropCall,
            locals,
            instructions,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, primec::ir_lowerer::UninitializedStorageAccessInfo &,
               bool &resolved) {
              resolved = false;
              return true;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            [](const std::string &, primec::ir_lowerer::StructSlotLayoutInfo &) { return true; },
            []() { return 0; },
            [](int32_t, int32_t, int32_t) { return true; },
            error) == EmitResult::Error);
  CHECK(error == "drop requires uninitialized storage");
}

TEST_CASE("ir lowerer statement binding helper emits uninitialized take statements") {
  using EmitResult = primec::ir_lowerer::UninitializedStorageTakeEmitResult;

  primec::Expr storageExpr;
  storageExpr.kind = primec::Expr::Kind::Name;
  storageExpr.name = "slot";

  primec::Expr takeCall;
  takeCall.kind = primec::Expr::Kind::Call;
  takeCall.name = "take";
  takeCall.args = {storageExpr};

  primec::ir_lowerer::LocalMap locals;
  std::vector<primec::IrInstruction> instructions;
  std::string error;
  CHECK(primec::ir_lowerer::tryEmitUninitializedStorageTakeStatement(
            takeCall,
            locals,
            instructions,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, primec::ir_lowerer::UninitializedStorageAccessInfo &,
               bool &resolved) {
              resolved = true;
              return true;
            },
            [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
              instructions.push_back({primec::IrOpcode::PushI64, 7});
              return true;
            }) == EmitResult::Emitted);
  CHECK(error.empty());
  REQUIRE(instructions.size() == 2);
  CHECK(instructions[0].op == primec::IrOpcode::PushI64);
  CHECK(instructions[1].op == primec::IrOpcode::Pop);
}

TEST_CASE("ir lowerer statement binding helper skips non-emittable take statements") {
  using EmitResult = primec::ir_lowerer::UninitializedStorageTakeEmitResult;

  primec::Expr storageExpr;
  storageExpr.kind = primec::Expr::Kind::Name;
  storageExpr.name = "slot";

  primec::Expr takeCall = storageExpr;
  takeCall.kind = primec::Expr::Kind::Call;
  takeCall.name = "take";
  takeCall.args = {storageExpr};

  primec::ir_lowerer::LocalMap locals;
  std::vector<primec::IrInstruction> instructions;
  std::string error;
  CHECK(primec::ir_lowerer::tryEmitUninitializedStorageTakeStatement(
            takeCall,
            locals,
            instructions,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, primec::ir_lowerer::UninitializedStorageAccessInfo &,
               bool &resolved) {
              resolved = false;
              return true;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; }) == EmitResult::NotMatched);
  CHECK(error.empty());
  CHECK(instructions.empty());

  primec::Expr wrongArity = takeCall;
  wrongArity.args.push_back(storageExpr);
  CHECK(primec::ir_lowerer::tryEmitUninitializedStorageTakeStatement(
            wrongArity,
            locals,
            instructions,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, primec::ir_lowerer::UninitializedStorageAccessInfo &,
               bool &resolved) {
              resolved = true;
              return true;
            },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; }) == EmitResult::NotMatched);
  CHECK(error.empty());
  CHECK(instructions.empty());
}

TEST_CASE("ir lowerer statement binding helper surfaces take resolution errors") {
  using EmitResult = primec::ir_lowerer::UninitializedStorageTakeEmitResult;

  primec::Expr storageExpr;
  storageExpr.kind = primec::Expr::Kind::Name;
  storageExpr.name = "slot";

  primec::Expr takeCall;
  takeCall.kind = primec::Expr::Kind::Call;
  takeCall.name = "take";
  takeCall.args = {storageExpr};

  primec::ir_lowerer::LocalMap locals;
  std::vector<primec::IrInstruction> instructions;
  std::string error;
  CHECK(primec::ir_lowerer::tryEmitUninitializedStorageTakeStatement(
            takeCall,
            locals,
            instructions,
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, primec::ir_lowerer::UninitializedStorageAccessInfo &,
               bool &) { return false; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; }) == EmitResult::Error);
}

TEST_CASE("ir lowerer statement binding helper emits print statement builtins") {
  using EmitResult = primec::ir_lowerer::StatementPrintPathSpaceEmitResult;

  primec::Expr argExpr;
  argExpr.kind = primec::Expr::Kind::Literal;
  argExpr.intWidth = 32;
  argExpr.literalValue = 3;

  primec::Expr stmt;
  stmt.kind = primec::Expr::Kind::Call;
  stmt.name = "print_line";
  stmt.args = {argExpr};

  primec::ir_lowerer::LocalMap locals;
  std::vector<primec::IrInstruction> instructions;
  int printCalls = 0;
  std::string error;
  CHECK(primec::ir_lowerer::tryEmitPrintPathSpaceStatementBuiltin(
            stmt,
            locals,
            [&](const primec::Expr &printedArg, const primec::ir_lowerer::LocalMap &, const primec::ir_lowerer::PrintBuiltin &builtin) {
              ++printCalls;
              CHECK(printedArg.literalValue == 3);
              CHECK(builtin.name == "print_line");
              CHECK(builtin.newline);
              CHECK(builtin.target == primec::ir_lowerer::PrintTarget::Out);
              return true;
            },
            [](const primec::Expr &) -> const primec::Definition * { return nullptr; },
            [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
            instructions,
            error) == EmitResult::Emitted);
  CHECK(error.empty());
  CHECK(printCalls == 1);
  CHECK(instructions.empty());
}


TEST_SUITE_END();
