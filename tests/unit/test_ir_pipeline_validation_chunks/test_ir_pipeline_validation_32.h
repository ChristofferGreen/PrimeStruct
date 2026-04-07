TEST_CASE("ir lowerer call helpers emit map lookup loop search scaffold") {
  using Kind = primec::ir_lowerer::LocalInfo::ValueKind;

  std::vector<primec::IrInstruction> instructions;
  int nextLocal = 40;
  const auto loopLocals = primec::ir_lowerer::emitMapLookupLoopSearchScaffold(
      3,
      7,
      Kind::Int32,
      [&]() { return nextLocal++; },
      [&]() { return instructions.size(); },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      [&](size_t instructionIndex, uint64_t imm) { instructions[instructionIndex].imm = imm; });

  CHECK(loopLocals.countLocal == 40);
  CHECK(loopLocals.indexLocal == 41);
  REQUIRE(instructions.size() == 28);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 3);
  CHECK(instructions[1].op == primec::IrOpcode::LoadIndirect);
  CHECK(instructions[2].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[2].imm == 40);
  CHECK(instructions[3].op == primec::IrOpcode::PushI32);
  CHECK(instructions[3].imm == 0);
  CHECK(instructions[4].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[4].imm == 41);
  CHECK(instructions[5].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[5].imm == 41);
  CHECK(instructions[6].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[6].imm == 40);
  CHECK(instructions[7].op == primec::IrOpcode::CmpLtI32);
  CHECK(instructions[8].op == primec::IrOpcode::JumpIfZero);
  CHECK(instructions[8].imm == 28);
  CHECK(instructions[9].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[9].imm == 3);
  CHECK(instructions[10].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[10].imm == 41);
  CHECK(instructions[11].op == primec::IrOpcode::PushI32);
  CHECK(instructions[11].imm == 2);
  CHECK(instructions[12].op == primec::IrOpcode::MulI32);
  CHECK(instructions[13].op == primec::IrOpcode::PushI32);
  CHECK(instructions[13].imm == 1);
  CHECK(instructions[14].op == primec::IrOpcode::AddI32);
  CHECK(instructions[15].op == primec::IrOpcode::PushI32);
  CHECK(instructions[15].imm == primec::IrSlotBytesI32);
  CHECK(instructions[16].op == primec::IrOpcode::MulI32);
  CHECK(instructions[17].op == primec::IrOpcode::AddI64);
  CHECK(instructions[18].op == primec::IrOpcode::LoadIndirect);
  CHECK(instructions[19].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[19].imm == 7);
  CHECK(instructions[20].op == primec::IrOpcode::CmpEqI32);
  CHECK(instructions[21].op == primec::IrOpcode::JumpIfZero);
  CHECK(instructions[21].imm == 23);
  CHECK(instructions[22].op == primec::IrOpcode::Jump);
  CHECK(instructions[22].imm == 28);
  CHECK(instructions[23].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[23].imm == 41);
  CHECK(instructions[24].op == primec::IrOpcode::PushI32);
  CHECK(instructions[24].imm == 1);
  CHECK(instructions[25].op == primec::IrOpcode::AddI32);
  CHECK(instructions[26].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[26].imm == 41);
  CHECK(instructions[27].op == primec::IrOpcode::Jump);
  CHECK(instructions[27].imm == 5);
}

TEST_CASE("ir lowerer call helpers emit map lookup access epilogue") {
  std::vector<primec::IrInstruction> instructions;
  int keyNotFoundCalls = 0;
  primec::ir_lowerer::emitMapLookupAccessEpilogue(
      "at",
      5,
      6,
      7,
      [&]() { ++keyNotFoundCalls; },
      [&]() { return instructions.size(); },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      [&](size_t instructionIndex, uint64_t imm) { instructions[instructionIndex].imm = imm; });
  CHECK(keyNotFoundCalls == 1);
  REQUIRE(instructions.size() == 14);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 6);
  CHECK(instructions[1].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[1].imm == 7);
  CHECK(instructions[2].op == primec::IrOpcode::CmpEqI32);
  CHECK(instructions[3].op == primec::IrOpcode::JumpIfZero);
  CHECK(instructions[3].imm == 4);
  CHECK(instructions[4].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[4].imm == 5);
  CHECK(instructions[5].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[5].imm == 6);
  CHECK(instructions[6].op == primec::IrOpcode::PushI32);
  CHECK(instructions[6].imm == 2);
  CHECK(instructions[7].op == primec::IrOpcode::MulI32);
  CHECK(instructions[8].op == primec::IrOpcode::PushI32);
  CHECK(instructions[8].imm == 2);
  CHECK(instructions[9].op == primec::IrOpcode::AddI32);
  CHECK(instructions[10].op == primec::IrOpcode::PushI32);
  CHECK(instructions[10].imm == primec::IrSlotBytesI32);
  CHECK(instructions[11].op == primec::IrOpcode::MulI32);
  CHECK(instructions[12].op == primec::IrOpcode::AddI64);
  CHECK(instructions[13].op == primec::IrOpcode::LoadIndirect);

  instructions.clear();
  keyNotFoundCalls = 0;
  primec::ir_lowerer::emitMapLookupAccessEpilogue(
      "find",
      5,
      6,
      7,
      [&]() { ++keyNotFoundCalls; },
      [&]() { return instructions.size(); },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      [&](size_t instructionIndex, uint64_t imm) { instructions[instructionIndex].imm = imm; });
  CHECK(keyNotFoundCalls == 0);
  REQUIRE(instructions.size() == 10);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 5);
  CHECK(instructions[1].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[1].imm == 6);
  CHECK(instructions[2].op == primec::IrOpcode::PushI32);
  CHECK(instructions[2].imm == 2);
  CHECK(instructions[3].op == primec::IrOpcode::MulI32);
  CHECK(instructions[4].op == primec::IrOpcode::PushI32);
  CHECK(instructions[4].imm == 2);
  CHECK(instructions[5].op == primec::IrOpcode::AddI32);
  CHECK(instructions[6].op == primec::IrOpcode::PushI32);
  CHECK(instructions[6].imm == primec::IrSlotBytesI32);
  CHECK(instructions[7].op == primec::IrOpcode::MulI32);
  CHECK(instructions[8].op == primec::IrOpcode::AddI64);
  CHECK(instructions[9].op == primec::IrOpcode::LoadIndirect);
}

TEST_CASE("ir lowerer call helpers emit map lookup access") {
  using Kind = primec::ir_lowerer::LocalInfo::ValueKind;

  primec::Expr targetExpr;
  targetExpr.kind = primec::Expr::Kind::Name;
  targetExpr.name = "mapTarget";
  primec::Expr keyExpr;
  keyExpr.kind = primec::Expr::Kind::Name;
  keyExpr.name = "mapKey";
  primec::ir_lowerer::LocalMap locals;
  locals[targetExpr.name] = primec::ir_lowerer::LocalInfo{};
  locals[keyExpr.name] = primec::ir_lowerer::LocalInfo{};

  std::vector<primec::IrInstruction> instructions;
  int nextLocal = 20;
  int emitExprCalls = 0;
  int inferCalls = 0;
  int keyNotFoundCalls = 0;
  std::string error;
  CHECK(primec::ir_lowerer::emitMapLookupAccess(
      "at",
      Kind::Int32,
      "",
      targetExpr,
      keyExpr,
      locals,
      [&]() { return nextLocal++; },
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        ++emitExprCalls;
        return true;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return false; },
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        ++inferCalls;
        return Kind::Int32;
      },
      [&]() { ++keyNotFoundCalls; },
      [&]() { return instructions.size(); },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      [&](size_t instructionIndex, uint64_t imm) { instructions[instructionIndex].imm = imm; },
      error));
  CHECK(emitExprCalls == 2);
  CHECK(inferCalls == 1);
  CHECK(keyNotFoundCalls == 1);
  CHECK(error.empty());
  REQUIRE(instructions.size() == 44);
  CHECK(instructions[0].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[0].imm == 20);
  CHECK(instructions[1].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[1].imm == 21);
  CHECK(instructions[10].op == primec::IrOpcode::JumpIfZero);
  CHECK(instructions[10].imm == 30);
  CHECK(instructions[23].op == primec::IrOpcode::JumpIfZero);
  CHECK(instructions[23].imm == 25);
  CHECK(instructions[24].op == primec::IrOpcode::Jump);
  CHECK(instructions[24].imm == 30);
  CHECK(instructions[33].op == primec::IrOpcode::JumpIfZero);
  CHECK(instructions[33].imm == 34);
  CHECK(instructions[34].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[34].imm == 20);
  CHECK(instructions[43].op == primec::IrOpcode::LoadIndirect);

  instructions.clear();
  nextLocal = 40;
  emitExprCalls = 0;
  inferCalls = 0;
  keyNotFoundCalls = 0;
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::emitMapLookupAccess(
      "at",
      Kind::Int32,
      "",
      targetExpr,
      keyExpr,
      locals,
      [&]() { return nextLocal++; },
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        ++emitExprCalls;
        return false;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return false; },
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        ++inferCalls;
        return Kind::Int32;
      },
      [&]() { ++keyNotFoundCalls; },
      [&]() { return instructions.size(); },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      [&](size_t instructionIndex, uint64_t imm) { instructions[instructionIndex].imm = imm; },
      error));
  CHECK(emitExprCalls == 1);
  CHECK(inferCalls == 0);
  CHECK(keyNotFoundCalls == 0);
  CHECK(instructions.empty());
  CHECK(error.empty());

  instructions.clear();
  nextLocal = 50;
  emitExprCalls = 0;
  inferCalls = 0;
  keyNotFoundCalls = 0;
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::emitMapLookupAccess(
      "at",
      Kind::String,
      "",
      targetExpr,
      keyExpr,
      locals,
      [&]() { return nextLocal++; },
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        ++emitExprCalls;
        return true;
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return false; },
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        ++inferCalls;
        return Kind::Int32;
      },
      [&]() { ++keyNotFoundCalls; },
      [&]() { return instructions.size(); },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      [&](size_t instructionIndex, uint64_t imm) { instructions[instructionIndex].imm = imm; },
      error));
  CHECK(emitExprCalls == 1);
  CHECK(inferCalls == 1);
  CHECK(keyNotFoundCalls == 0);
  REQUIRE(instructions.size() == 1);
  CHECK(instructions[0].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[0].imm == 50);
  CHECK(error == "native backend requires map lookup key type to match map key type");
}

TEST_CASE("ir lowerer call helpers emit string access load") {
  using Kind = primec::ir_lowerer::LocalInfo::ValueKind;

  std::vector<primec::IrInstruction> instructions;
  int stringIndexOutOfBoundsCalls = 0;
  primec::ir_lowerer::emitStringAccessLoad(
      "at",
      9,
      Kind::Int32,
      5,
      12,
      [&]() { ++stringIndexOutOfBoundsCalls; },
      [&]() { return instructions.size(); },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      [&](size_t instructionIndex, uint64_t imm) { instructions[instructionIndex].imm = imm; });

  CHECK(stringIndexOutOfBoundsCalls == 2);
  REQUIRE(instructions.size() == 10);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 9);
  CHECK(instructions[1].op == primec::ir_lowerer::pushZeroForIndex(Kind::Int32));
  CHECK(instructions[2].op == primec::ir_lowerer::cmpLtForIndex(Kind::Int32));
  CHECK(instructions[3].op == primec::IrOpcode::JumpIfZero);
  CHECK(instructions[3].imm == 4);
  CHECK(instructions[4].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[4].imm == 9);
  CHECK(instructions[5].op == primec::IrOpcode::PushI32);
  CHECK(instructions[5].imm == 5);
  CHECK(instructions[6].op == primec::ir_lowerer::cmpGeForIndex(Kind::Int32));
  CHECK(instructions[7].op == primec::IrOpcode::JumpIfZero);
  CHECK(instructions[7].imm == 8);
  CHECK(instructions[8].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[8].imm == 9);
  CHECK(instructions[9].op == primec::IrOpcode::LoadStringByte);
  CHECK(instructions[9].imm == 12);

  instructions.clear();
  stringIndexOutOfBoundsCalls = 0;
  primec::ir_lowerer::emitStringAccessLoad(
      "index",
      11,
      Kind::UInt64,
      999,
      4,
      [&]() { ++stringIndexOutOfBoundsCalls; },
      [&]() { return instructions.size(); },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      [&](size_t instructionIndex, uint64_t imm) { instructions[instructionIndex].imm = imm; });

  CHECK(stringIndexOutOfBoundsCalls == 0);
  REQUIRE(instructions.size() == 2);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 11);
  CHECK(instructions[1].op == primec::IrOpcode::LoadStringByte);
  CHECK(instructions[1].imm == 4);
}

TEST_CASE("ir lowerer call helpers emit array vector access load") {
  using Kind = primec::ir_lowerer::LocalInfo::ValueKind;

  std::vector<primec::IrInstruction> instructions;
  int nextLocal = 30;
  int arrayIndexOutOfBoundsCalls = 0;
  primec::ir_lowerer::emitArrayVectorAccessLoad(
      "at",
      8,
      9,
      Kind::Int32,
      true,
      1,
      1,
      true,
      [&]() { return nextLocal++; },
      [&]() { ++arrayIndexOutOfBoundsCalls; },
      [&]() { return instructions.size(); },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      [&](size_t instructionIndex, uint64_t imm) { instructions[instructionIndex].imm = imm; });
  CHECK(nextLocal == 32);
  CHECK(arrayIndexOutOfBoundsCalls == 2);
  REQUIRE(instructions.size() == 22);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 8);
  CHECK(instructions[1].op == primec::IrOpcode::LoadIndirect);
  CHECK(instructions[2].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[2].imm == 30);
  CHECK(instructions[3].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[3].imm == 9);
  CHECK(instructions[4].op == primec::ir_lowerer::pushZeroForIndex(Kind::Int32));
  CHECK(instructions[5].op == primec::ir_lowerer::cmpLtForIndex(Kind::Int32));
  CHECK(instructions[6].op == primec::IrOpcode::JumpIfZero);
  CHECK(instructions[6].imm == 7);
  CHECK(instructions[7].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[7].imm == 9);
  CHECK(instructions[8].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[8].imm == 30);
  CHECK(instructions[9].op == primec::ir_lowerer::cmpGeForIndex(Kind::Int32));
  CHECK(instructions[10].op == primec::IrOpcode::JumpIfZero);
  CHECK(instructions[10].imm == 11);
  CHECK(instructions[11].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[11].imm == 8);
  CHECK(instructions[12].op == primec::IrOpcode::PushI64);
  CHECK(instructions[12].imm == 32);
  CHECK(instructions[13].op == primec::IrOpcode::AddI64);
  CHECK(instructions[14].op == primec::IrOpcode::LoadIndirect);
  CHECK(instructions[15].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[15].imm == 31);
  CHECK(instructions[16].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[16].imm == 31);
  CHECK(instructions[17].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[17].imm == 9);
  CHECK(instructions[18].op == primec::ir_lowerer::pushOneForIndex(Kind::Int32));
  CHECK(instructions[18].imm == primec::IrSlotBytesI32);
  CHECK(instructions[19].op == primec::ir_lowerer::mulForIndex(Kind::Int32));
  CHECK(instructions[20].op == primec::IrOpcode::AddI64);
  CHECK(instructions[21].op == primec::IrOpcode::LoadIndirect);

  instructions.clear();
  nextLocal = 40;
  arrayIndexOutOfBoundsCalls = 0;
  primec::ir_lowerer::emitArrayVectorAccessLoad(
      "index",
      8,
      9,
      Kind::UInt64,
      false,
      1,
      1,
      true,
      [&]() { return nextLocal++; },
      [&]() { ++arrayIndexOutOfBoundsCalls; },
      [&]() { return instructions.size(); },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      [&](size_t instructionIndex, uint64_t imm) { instructions[instructionIndex].imm = imm; });
  CHECK(nextLocal == 40);
  CHECK(arrayIndexOutOfBoundsCalls == 0);
  REQUIRE(instructions.size() == 8);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 8);
  CHECK(instructions[1].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[1].imm == 9);
  CHECK(instructions[2].op == primec::ir_lowerer::pushOneForIndex(Kind::UInt64));
  CHECK(instructions[2].imm == 1);
  CHECK(instructions[3].op == primec::ir_lowerer::addForIndex(Kind::UInt64));
  CHECK(instructions[4].op == primec::ir_lowerer::pushOneForIndex(Kind::UInt64));
  CHECK(instructions[4].imm == primec::IrSlotBytesI32);
  CHECK(instructions[5].op == primec::ir_lowerer::mulForIndex(Kind::UInt64));
  CHECK(instructions[6].op == primec::IrOpcode::AddI64);
  CHECK(instructions[7].op == primec::IrOpcode::LoadIndirect);

  instructions.clear();
  nextLocal = 50;
  arrayIndexOutOfBoundsCalls = 0;
  primec::ir_lowerer::emitArrayVectorAccessLoad(
      "at",
      8,
      9,
      Kind::Int32,
      false,
      1,
      2,
      false,
      [&]() { return nextLocal++; },
      [&]() { ++arrayIndexOutOfBoundsCalls; },
      [&]() { return instructions.size(); },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      [&](size_t instructionIndex, uint64_t imm) { instructions[instructionIndex].imm = imm; });
  CHECK(nextLocal == 51);
  CHECK(arrayIndexOutOfBoundsCalls == 2);
  REQUIRE(instructions.size() == 20u);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 8);
  CHECK(instructions[1].op == primec::IrOpcode::LoadIndirect);
  CHECK(instructions[2].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[2].imm == 50);
  CHECK(instructions[11].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[11].imm == 8);
  CHECK(instructions[12].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[12].imm == 9);
  CHECK(instructions[13].op == primec::ir_lowerer::pushOneForIndex(Kind::Int32));
  CHECK(instructions[13].imm == 2u);
  CHECK(instructions[14].op == primec::ir_lowerer::mulForIndex(Kind::Int32));
  CHECK(instructions[15].op == primec::ir_lowerer::pushOneForIndex(Kind::Int32));
  CHECK(instructions[15].imm == 1u);
  CHECK(instructions[16].op == primec::ir_lowerer::addForIndex(Kind::Int32));
  CHECK(instructions[17].op == primec::ir_lowerer::pushOneForIndex(Kind::Int32));
  CHECK(instructions[17].imm == primec::IrSlotBytesI32);
  CHECK(instructions[18].op == primec::ir_lowerer::mulForIndex(Kind::Int32));
  CHECK(instructions[19].op == primec::IrOpcode::AddI64);
}

TEST_CASE("ir lowerer call helpers emit map lookup loop locals") {
  std::vector<primec::IrInstruction> instructions;
  int nextLocal = 30;
  auto locals = primec::ir_lowerer::emitMapLookupLoopLocals(
      12,
      [&]() { return nextLocal++; },
      [&](primec::IrOpcode op, uint64_t imm) {
        instructions.push_back({op, imm});
      });

  CHECK(locals.countLocal == 30);
  CHECK(locals.indexLocal == 31);
  REQUIRE(instructions.size() == 5);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 12);
  CHECK(instructions[1].op == primec::IrOpcode::LoadIndirect);
  CHECK(instructions[2].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[2].imm == 30);
  CHECK(instructions[3].op == primec::IrOpcode::PushI32);
  CHECK(instructions[3].imm == 0);
  CHECK(instructions[4].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[4].imm == 31);
}

TEST_CASE("ir lowerer call helpers emit map lookup loop condition") {
  std::vector<primec::IrInstruction> instructions;
  auto anchors = primec::ir_lowerer::emitMapLookupLoopCondition(
      7,
      9,
      [&]() { return instructions.size(); },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); });

  CHECK(anchors.loopStart == 0);
  CHECK(anchors.jumpLoopEnd == 3);
  REQUIRE(instructions.size() == 4);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 7);
  CHECK(instructions[1].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[1].imm == 9);
  CHECK(instructions[2].op == primec::IrOpcode::CmpLtI32);
  CHECK(instructions[3].op == primec::IrOpcode::JumpIfZero);
  CHECK(instructions[3].imm == 0);
}

TEST_CASE("ir lowerer call helpers emit map lookup loop match check") {
  std::vector<primec::IrInstruction> instructions;
  auto anchors = primec::ir_lowerer::emitMapLookupLoopMatchCheck(
      4,
      5,
      6,
      primec::ir_lowerer::LocalInfo::ValueKind::Int64,
      [&]() { return instructions.size(); },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); });

  CHECK(anchors.jumpNotMatch == 12);
  CHECK(anchors.jumpFound == 13);
  REQUIRE(instructions.size() == 14);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 4);
  CHECK(instructions[1].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[1].imm == 5);
  CHECK(instructions[6].op == primec::IrOpcode::PushI32);
  CHECK(instructions[6].imm == primec::IrSlotBytesI32);
  CHECK(instructions[10].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[10].imm == 6);
  CHECK(instructions[11].op == primec::IrOpcode::CmpEqI64);
  CHECK(instructions[12].op == primec::IrOpcode::JumpIfZero);
  CHECK(instructions[12].imm == 0);
  CHECK(instructions[13].op == primec::IrOpcode::Jump);
  CHECK(instructions[13].imm == 0);
}

TEST_CASE("ir lowerer call helpers emit map lookup loop advance patching") {
  std::vector<primec::IrInstruction> instructions = {
      {primec::IrOpcode::PushI32, 10},
      {primec::IrOpcode::JumpIfZero, 0},
      {primec::IrOpcode::PushI32, 12},
      {primec::IrOpcode::JumpIfZero, 0},
      {primec::IrOpcode::Jump, 0},
      {primec::IrOpcode::PushI32, 13},
  };

  primec::ir_lowerer::emitMapLookupLoopAdvanceAndPatch(
      1,
      3,
      4,
      2,
      9,
      [&]() { return instructions.size(); },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      [&](size_t instructionIndex, uint64_t imm) { instructions[instructionIndex].imm = imm; });

  REQUIRE(instructions.size() == 11);
  CHECK(instructions[1].imm == 6);
  CHECK(instructions[3].imm == 11);
  CHECK(instructions[4].imm == 11);
  CHECK(instructions[6].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[6].imm == 9);
  CHECK(instructions[7].op == primec::IrOpcode::PushI32);
  CHECK(instructions[7].imm == 1);
  CHECK(instructions[8].op == primec::IrOpcode::AddI32);
  CHECK(instructions[9].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[9].imm == 9);
  CHECK(instructions[10].op == primec::IrOpcode::Jump);
  CHECK(instructions[10].imm == 2);
}

TEST_CASE("ir lowerer call helpers emit map lookup at key-not-found guard") {
  std::vector<primec::IrInstruction> instructions = {
      {primec::IrOpcode::PushI32, 7},
  };
  int notFoundCalls = 0;

  primec::ir_lowerer::emitMapLookupAtKeyNotFoundGuard(
      11,
      12,
      [&]() {
        ++notFoundCalls;
        instructions.push_back({primec::IrOpcode::PushI32, 99});
      },
      [&]() { return instructions.size(); },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      [&](size_t instructionIndex, uint64_t imm) { instructions[instructionIndex].imm = imm; });

  CHECK(notFoundCalls == 1);
  REQUIRE(instructions.size() == 6);
  CHECK(instructions[1].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[1].imm == 11);
  CHECK(instructions[2].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[2].imm == 12);
  CHECK(instructions[3].op == primec::IrOpcode::CmpEqI32);
  CHECK(instructions[4].op == primec::IrOpcode::JumpIfZero);
  CHECK(instructions[4].imm == 6);
  CHECK(instructions[5].op == primec::IrOpcode::PushI32);
  CHECK(instructions[5].imm == 99);
}

TEST_CASE("ir lowerer call helpers emit map lookup value load") {
  std::vector<primec::IrInstruction> instructions;
  primec::ir_lowerer::emitMapLookupValueLoad(
      21,
      22,
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); });

  REQUIRE(instructions.size() == 10);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 21);
  CHECK(instructions[1].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[1].imm == 22);
  CHECK(instructions[2].op == primec::IrOpcode::PushI32);
  CHECK(instructions[2].imm == 2);
  CHECK(instructions[3].op == primec::IrOpcode::MulI32);
  CHECK(instructions[4].op == primec::IrOpcode::PushI32);
  CHECK(instructions[4].imm == 2);
  CHECK(instructions[5].op == primec::IrOpcode::AddI32);
  CHECK(instructions[6].op == primec::IrOpcode::PushI32);
  CHECK(instructions[6].imm == primec::IrSlotBytesI32);
  CHECK(instructions[7].op == primec::IrOpcode::MulI32);
  CHECK(instructions[8].op == primec::IrOpcode::AddI64);
  CHECK(instructions[9].op == primec::IrOpcode::LoadIndirect);
}

TEST_CASE("ir lowerer call helpers validate map lookup key kinds") {
  using Kind = primec::ir_lowerer::LocalInfo::ValueKind;

  std::string error;
  CHECK(primec::ir_lowerer::validateMapLookupKeyKind(Kind::Int32, Kind::Int32, error));
  CHECK(error.empty());

  CHECK_FALSE(primec::ir_lowerer::validateMapLookupKeyKind(Kind::Int32, Kind::Unknown, error));
  CHECK(error == "native backend requires map lookup key type to match map key type");

  error.clear();
  CHECK_FALSE(primec::ir_lowerer::validateMapLookupKeyKind(Kind::Int32, Kind::String, error));
  CHECK(error == "native backend requires map lookup key to be numeric/bool");

  error.clear();
  CHECK_FALSE(primec::ir_lowerer::validateMapLookupKeyKind(Kind::Int64, Kind::Float64, error));
  CHECK(error == "native backend requires map lookup key type to match map key type");
}

TEST_CASE("ir lowerer call helpers handle non-method count fallback") {
#include "../test_ir_pipeline_validation_fragments/test_ir_pipeline_validation_count_fallback_01.h"
#include "../test_ir_pipeline_validation_fragments/test_ir_pipeline_validation_count_fallback_02.h"
}
