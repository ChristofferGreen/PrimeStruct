TEST_CASE("ir lowerer inline param helper aliases pure map variadic forwarding") {
  primec::Expr valuesParam;
  valuesParam.kind = primec::Expr::Kind::Name;
  valuesParam.isBinding = true;
  valuesParam.name = "values";

  primec::Expr spreadArg;
  spreadArg.kind = primec::Expr::Kind::Name;
  spreadArg.name = "source";
  spreadArg.isSpread = true;

  primec::ir_lowerer::LocalMap callerLocals;
  primec::ir_lowerer::LocalInfo sourceInfo;
  sourceInfo.index = 18;
  sourceInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  sourceInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  sourceInfo.mapKeyKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  sourceInfo.mapValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  sourceInfo.isArgsPack = true;
  sourceInfo.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Map;
  sourceInfo.argsPackElementCount = 2;
  callerLocals.emplace("source", sourceInfo);

  int32_t nextLocal = 2;
  primec::ir_lowerer::LocalMap calleeLocals;
  std::vector<primec::IrInstruction> instructions;
  std::string error;

  REQUIRE(primec::ir_lowerer::emitInlineDefinitionCallParameters(
      {valuesParam},
      {nullptr},
      {&spreadArg},
      0,
      callerLocals,
      nextLocal,
      calleeLocals,
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &infoOut, std::string &) {
        infoOut.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
        infoOut.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
        infoOut.mapKeyKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
        infoOut.mapValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
        infoOut.isArgsPack = true;
        infoOut.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Map;
        return true;
      },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &,
         const primec::ir_lowerer::LocalMap &,
         primec::ir_lowerer::LocalInfo::StringSource &,
         int32_t &,
         bool &) { return true; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string(); },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const std::string &, primec::ir_lowerer::StructSlotLayoutInfo &) { return true; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
      [](int32_t, int32_t, int32_t) { return true; },
      []() { return 0; },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      [](int32_t) {},
      error));

  CHECK(error.empty());
  CHECK(nextLocal == 3);
  REQUIRE(calleeLocals.count("values") == 1u);
  CHECK(calleeLocals.at("values").argsPackElementKind == primec::ir_lowerer::LocalInfo::Kind::Map);
  CHECK(calleeLocals.at("values").mapKeyKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  CHECK(calleeLocals.at("values").mapValueKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  CHECK(calleeLocals.at("values").argsPackElementCount == 2);
  REQUIRE(instructions.size() == 2u);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 18u);
  CHECK(instructions[1].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[1].imm == 2u);
}

TEST_CASE("ir lowerer inline param helper rejects map variadic alias type mismatch") {
  primec::Expr valuesParam;
  valuesParam.kind = primec::Expr::Kind::Name;
  valuesParam.isBinding = true;
  valuesParam.name = "values";

  primec::Expr spreadArg;
  spreadArg.kind = primec::Expr::Kind::Name;
  spreadArg.name = "source";
  spreadArg.isSpread = true;

  primec::ir_lowerer::LocalMap callerLocals;
  primec::ir_lowerer::LocalInfo sourceInfo;
  sourceInfo.index = 18;
  sourceInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  sourceInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  sourceInfo.mapKeyKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  sourceInfo.mapValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  sourceInfo.isArgsPack = true;
  sourceInfo.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Map;
  sourceInfo.argsPackElementCount = 2;
  callerLocals.emplace("source", sourceInfo);

  int32_t nextLocal = 2;
  primec::ir_lowerer::LocalMap calleeLocals;
  std::vector<primec::IrInstruction> instructions;
  std::string error;

  CHECK_FALSE(primec::ir_lowerer::emitInlineDefinitionCallParameters(
      {valuesParam},
      {nullptr},
      {&spreadArg},
      0,
      callerLocals,
      nextLocal,
      calleeLocals,
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &infoOut, std::string &) {
        infoOut.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
        infoOut.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
        infoOut.mapKeyKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
        infoOut.mapValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
        infoOut.isArgsPack = true;
        infoOut.argsPackElementKind = primec::ir_lowerer::LocalInfo::Kind::Map;
        return true;
      },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &,
         const primec::ir_lowerer::LocalMap &,
         primec::ir_lowerer::LocalInfo::StringSource &,
         int32_t &,
         bool &) { return true; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string(); },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const std::string &, primec::ir_lowerer::StructSlotLayoutInfo &) { return true; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
      [](int32_t, int32_t, int32_t) { return true; },
      []() { return 0; },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      [](int32_t) {},
      error));

  CHECK(error == "variadic parameter type mismatch");
  CHECK(calleeLocals.empty());
  CHECK(instructions.empty());
}

TEST_CASE("ir lowerer inline param helper aliases pure struct variadic forwarding") {
  primec::Expr valuesParam;
  valuesParam.kind = primec::Expr::Kind::Name;
  valuesParam.isBinding = true;
  valuesParam.name = "values";

  primec::Expr spreadArg;
  spreadArg.kind = primec::Expr::Kind::Name;
  spreadArg.name = "source";
  spreadArg.isSpread = true;

  primec::ir_lowerer::LocalMap callerLocals;
  primec::ir_lowerer::LocalInfo sourceInfo;
  sourceInfo.index = 12;
  sourceInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  sourceInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  sourceInfo.structTypeName = "/pkg/Pair";
  sourceInfo.structSlotCount = 2;
  sourceInfo.isArgsPack = true;
  sourceInfo.argsPackElementCount = 2;
  callerLocals.emplace("source", sourceInfo);

  int32_t nextLocal = 1;
  primec::ir_lowerer::LocalMap calleeLocals;
  std::vector<primec::IrInstruction> instructions;
  std::string error;

  REQUIRE(primec::ir_lowerer::emitInlineDefinitionCallParameters(
      {valuesParam},
      {nullptr},
      {&spreadArg},
      0,
      callerLocals,
      nextLocal,
      calleeLocals,
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &infoOut, std::string &) {
        infoOut.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
        infoOut.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
        infoOut.structTypeName = "/pkg/Pair";
        infoOut.isArgsPack = true;
        return true;
      },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &,
         const primec::ir_lowerer::LocalMap &,
         primec::ir_lowerer::LocalInfo::StringSource &,
         int32_t &,
         bool &) { return true; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string(); },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const std::string &, primec::ir_lowerer::StructSlotLayoutInfo &) { return true; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
      [](int32_t, int32_t, int32_t) { return true; },
      []() { return 0; },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      [](int32_t) {},
      error));

  CHECK(error.empty());
  CHECK(nextLocal == 2);
  REQUIRE(calleeLocals.count("values") == 1u);
  CHECK(calleeLocals.at("values").structTypeName == "/pkg/Pair");
  CHECK(calleeLocals.at("values").structSlotCount == 2);
  CHECK(calleeLocals.at("values").argsPackElementCount == 2);
  REQUIRE(instructions.size() == 2u);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 12u);
  CHECK(instructions[1].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[1].imm == 1u);
}

TEST_CASE("ir lowerer inline param helper materializes direct struct variadic args packs") {
  primec::Expr valuesParam;
  valuesParam.kind = primec::Expr::Kind::Name;
  valuesParam.isBinding = true;
  valuesParam.name = "values";

  primec::Expr firstArg;
  firstArg.kind = primec::Expr::Kind::Name;
  firstArg.name = "first";
  primec::Expr secondArg;
  secondArg.kind = primec::Expr::Kind::Name;
  secondArg.name = "second";

  int32_t nextLocal = 2;
  primec::ir_lowerer::LocalMap calleeLocals;
  std::vector<primec::IrInstruction> instructions;
  std::vector<int32_t> copyDests;
  std::vector<int32_t> copySrcs;
  std::vector<int32_t> copySlots;
  std::string error;

  REQUIRE(primec::ir_lowerer::emitInlineDefinitionCallParameters(
      {valuesParam},
      {nullptr},
      {&firstArg, &secondArg},
      0,
      {},
      nextLocal,
      calleeLocals,
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &infoOut, std::string &) {
        infoOut.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
        infoOut.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
        infoOut.structTypeName = "/pkg/Pair";
        infoOut.isArgsPack = true;
        return true;
      },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &,
         const primec::ir_lowerer::LocalMap &,
         primec::ir_lowerer::LocalInfo::StringSource &,
         int32_t &,
         bool &) { return true; },
      [&](const primec::Expr &arg, const primec::ir_lowerer::LocalMap &) {
        if (arg.name == "first" || arg.name == "second") {
          return std::string("/pkg/Pair");
        }
        return std::string();
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const std::string &structPath, primec::ir_lowerer::StructSlotLayoutInfo &layout) {
        if (structPath != "/pkg/Pair") {
          return false;
        }
        layout.structPath = structPath;
        layout.totalSlots = 2;
        return true;
      },
      [&](const primec::Expr &arg, const primec::ir_lowerer::LocalMap &) {
        const uint64_t sourceLocal = (arg.name == "first") ? 21u : 22u;
        instructions.push_back({primec::IrOpcode::LoadLocal, sourceLocal});
        return true;
      },
      [&](int32_t destBaseLocal, int32_t srcPtrLocal, int32_t slotCount) {
        copyDests.push_back(destBaseLocal);
        copySrcs.push_back(srcPtrLocal);
        copySlots.push_back(slotCount);
        return true;
      },
      [&]() { return nextLocal++; },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      [](int32_t) {},
      error));

  CHECK(error.empty());
  CHECK(nextLocal == 10);
  REQUIRE(calleeLocals.count("values") == 1u);
  CHECK(calleeLocals.at("values").structTypeName == "/pkg/Pair");
  CHECK(calleeLocals.at("values").structSlotCount == 2);
  CHECK(calleeLocals.at("values").argsPackElementCount == 2);
  REQUIRE(instructions.size() == 8u);
  CHECK(instructions[0].op == primec::IrOpcode::PushI32);
  CHECK(instructions[0].imm == 2u);
  CHECK(instructions[1].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[1].imm == 3u);
  CHECK(instructions[2].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[2].imm == 21u);
  CHECK(instructions[3].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[3].imm == 8u);
  CHECK(instructions[4].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[4].imm == 22u);
  CHECK(instructions[5].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[5].imm == 9u);
  CHECK(instructions[6].op == primec::IrOpcode::AddressOfLocal);
  CHECK(instructions[6].imm == 3u);
  CHECK(instructions[7].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[7].imm == 2u);
  REQUIRE(copyDests.size() == 2u);
  CHECK(copyDests[0] == 4);
  CHECK(copyDests[1] == 6);
  CHECK(copySrcs[0] == 8);
  CHECK(copySrcs[1] == 9);
  CHECK(copySlots[0] == 2);
  CHECK(copySlots[1] == 2);
}

TEST_CASE("ir lowerer inline param helper materializes mixed struct variadic forwarding") {
  primec::Expr valuesParam;
  valuesParam.kind = primec::Expr::Kind::Name;
  valuesParam.isBinding = true;
  valuesParam.name = "values";

  primec::Expr firstArg;
  firstArg.kind = primec::Expr::Kind::Name;
  firstArg.name = "head";
  primec::Expr spreadArg;
  spreadArg.kind = primec::Expr::Kind::Name;
  spreadArg.name = "source";
  spreadArg.isSpread = true;

  primec::ir_lowerer::LocalMap callerLocals;
  primec::ir_lowerer::LocalInfo sourceInfo;
  sourceInfo.index = 33;
  sourceInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
  sourceInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  sourceInfo.structTypeName = "/pkg/Pair";
  sourceInfo.structSlotCount = 2;
  sourceInfo.isArgsPack = true;
  sourceInfo.argsPackElementCount = 2;
  callerLocals.emplace("source", sourceInfo);

  int32_t nextLocal = 4;
  primec::ir_lowerer::LocalMap calleeLocals;
  std::vector<primec::IrInstruction> instructions;
  std::vector<int32_t> copyDests;
  std::vector<int32_t> copySrcs;
  std::vector<int32_t> copySlots;
  std::string error;

  REQUIRE(primec::ir_lowerer::emitInlineDefinitionCallParameters(
      {valuesParam},
      {nullptr},
      {&firstArg, &spreadArg},
      0,
      callerLocals,
      nextLocal,
      calleeLocals,
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &infoOut, std::string &) {
        infoOut.kind = primec::ir_lowerer::LocalInfo::Kind::Array;
        infoOut.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
        infoOut.structTypeName = "/pkg/Pair";
        infoOut.isArgsPack = true;
        return true;
      },
      [](const primec::Expr &) { return false; },
      [](const primec::Expr &,
         const primec::ir_lowerer::LocalMap &,
         primec::ir_lowerer::LocalInfo::StringSource &,
         int32_t &,
         bool &) { return true; },
      [&](const primec::Expr &arg, const primec::ir_lowerer::LocalMap &) {
        if (arg.name == "head") {
          return std::string("/pkg/Pair");
        }
        return std::string();
      },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const std::string &structPath, primec::ir_lowerer::StructSlotLayoutInfo &layout) {
        if (structPath != "/pkg/Pair") {
          return false;
        }
        layout.structPath = structPath;
        layout.totalSlots = 2;
        return true;
      },
      [&](const primec::Expr &arg, const primec::ir_lowerer::LocalMap &) {
        if (arg.name != "head") {
          return false;
        }
        instructions.push_back({primec::IrOpcode::LoadLocal, 21u});
        return true;
      },
      [&](int32_t destBaseLocal, int32_t srcPtrLocal, int32_t slotCount) {
        copyDests.push_back(destBaseLocal);
        copySrcs.push_back(srcPtrLocal);
        copySlots.push_back(slotCount);
        return true;
      },
      [&]() { return nextLocal++; },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      [](int32_t) {},
      error));

  CHECK(error.empty());
  CHECK(nextLocal == 16);
  REQUIRE(calleeLocals.count("values") == 1u);
  CHECK(calleeLocals.at("values").structTypeName == "/pkg/Pair");
  CHECK(calleeLocals.at("values").structSlotCount == 2);
  CHECK(calleeLocals.at("values").argsPackElementCount == 3);
  REQUIRE(instructions.size() == 16u);
  CHECK(instructions[0].op == primec::IrOpcode::PushI32);
  CHECK(instructions[0].imm == 3u);
  CHECK(instructions[1].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[1].imm == 5u);
  CHECK(instructions[2].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[2].imm == 21u);
  CHECK(instructions[3].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[3].imm == 12u);
  CHECK(instructions[4].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[4].imm == 33u);
  CHECK(instructions[5].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[5].imm == 13u);
  CHECK(instructions[6].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[6].imm == 13u);
  CHECK(instructions[7].op == primec::IrOpcode::PushI64);
  CHECK(instructions[7].imm == primec::IrSlotBytes);
  CHECK(instructions[8].op == primec::IrOpcode::AddI64);
  CHECK(instructions[9].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[9].imm == 14u);
  CHECK(instructions[10].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[10].imm == 13u);
  CHECK(instructions[11].op == primec::IrOpcode::PushI64);
  CHECK(instructions[11].imm == 3u * primec::IrSlotBytes);
  CHECK(instructions[12].op == primec::IrOpcode::AddI64);
  CHECK(instructions[13].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[13].imm == 15u);
  CHECK(instructions[14].op == primec::IrOpcode::AddressOfLocal);
  CHECK(instructions[14].imm == 5u);
  CHECK(instructions[15].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[15].imm == 4u);
  REQUIRE(copyDests.size() == 3u);
  CHECK(copyDests[0] == 6);
  CHECK(copyDests[1] == 8);
  CHECK(copyDests[2] == 10);
  CHECK(copySrcs[0] == 12);
  CHECK(copySrcs[1] == 14);
  CHECK(copySrcs[2] == 15);
  CHECK(copySlots[0] == 2);
  CHECK(copySlots[1] == 2);
  CHECK(copySlots[2] == 2);
}

TEST_CASE("ir lowerer inline param helper requires canonical callee path for builtin soa to_aos bridge") {
  primec::Expr valuesParam;
  valuesParam.kind = primec::Expr::Kind::Name;
  valuesParam.isBinding = true;
  valuesParam.name = "values";

  primec::Expr sourceArg;
  sourceArg.kind = primec::Expr::Kind::Name;
  sourceArg.name = "source";

  auto inferStructExprPath = [&](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &locals) {
    if (expr.kind != primec::Expr::Kind::Name) {
      return std::string();
    }
    auto it = locals.find(expr.name);
    if (it == locals.end()) {
      return std::string();
    }
    return it->second.structTypeName;
  };

  auto resolveStructSlotLayout =
      [](const std::string &structPath, primec::ir_lowerer::StructSlotLayoutInfo &layout) {
        if (structPath != "/std/collections/experimental_soa_vector/SoaVector__Particle") {
          return false;
        }
        layout.structPath = structPath;
        layout.totalSlots = 4;
        layout.fields.clear();
        primec::ir_lowerer::StructSlotFieldInfo storageField;
        storageField.name = "storage";
        storageField.slotOffset = 0;
        storageField.slotCount = 4;
        layout.fields.push_back(storageField);
        return true;
      };

  auto inferCallParameterLocalInfo =
      [](const primec::Expr &, primec::ir_lowerer::LocalInfo &infoOut, std::string &) {
        infoOut.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
        infoOut.structTypeName = "/std/collections/experimental_soa_vector/SoaVector__Particle";
        return true;
      };

  primec::ir_lowerer::LocalMap callerLocals;
  primec::ir_lowerer::LocalInfo sourceInfo;
  sourceInfo.index = 17;
  sourceInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Value;
  sourceInfo.structTypeName = "/soa_vector";
  callerLocals.emplace("source", sourceInfo);

  {
    int32_t nextLocal = 3;
    primec::ir_lowerer::LocalMap calleeLocals;
    std::vector<primec::IrInstruction> instructions;
    std::string error;
    CHECK_FALSE(primec::ir_lowerer::emitInlineDefinitionCallParameters(
        {valuesParam},
        {&sourceArg},
        {},
        1,
        callerLocals,
        nextLocal,
        calleeLocals,
        inferCallParameterLocalInfo,
        [](const primec::Expr &) { return false; },
        [](const primec::Expr &,
           const primec::ir_lowerer::LocalMap &,
           primec::ir_lowerer::LocalInfo::StringSource &,
           int32_t &,
           bool &) { return true; },
        inferStructExprPath,
        [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
          return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
        },
        resolveStructSlotLayout,
        [&](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
          if (expr.kind != primec::Expr::Kind::Name || expr.name != "source") {
            return false;
          }
          instructions.push_back({primec::IrOpcode::LoadLocal, 17u});
          return true;
        },
        [](int32_t, int32_t, int32_t) { return true; },
        [&]() { return nextLocal++; },
        [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
        [](int32_t) {},
        error,
        {}));
    CHECK(error.find("struct parameter type mismatch") != std::string::npos);
    CHECK(instructions.empty());
    CHECK(calleeLocals.empty());
  }

  {
    int32_t nextLocal = 3;
    primec::ir_lowerer::LocalMap calleeLocals;
    std::vector<primec::IrInstruction> instructions;
    std::string error;
    REQUIRE(primec::ir_lowerer::emitInlineDefinitionCallParameters(
        {valuesParam},
        {&sourceArg},
        {},
        1,
        "/std/collections/soa_vector/to_aos_ref",
        callerLocals,
        nextLocal,
        calleeLocals,
        inferCallParameterLocalInfo,
        [](const primec::Expr &) { return false; },
        [](const primec::Expr &,
           const primec::ir_lowerer::LocalMap &,
           primec::ir_lowerer::LocalInfo::StringSource &,
           int32_t &,
           bool &) { return true; },
        inferStructExprPath,
        [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
          return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
        },
        resolveStructSlotLayout,
        [&](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
          if (expr.kind != primec::Expr::Kind::Name || expr.name != "source") {
            return false;
          }
          instructions.push_back({primec::IrOpcode::LoadLocal, 17u});
          return true;
        },
        [](int32_t, int32_t, int32_t) { return true; },
        [&]() { return nextLocal++; },
        [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
        [](int32_t) {},
        error,
        {}));
    CHECK(error.empty());
    REQUIRE(calleeLocals.count("values") == 1u);
    CHECK_FALSE(instructions.empty());
  }
}

TEST_CASE("ir lowerer setup type helper combines numeric kinds") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;
  CHECK(primec::ir_lowerer::combineNumericKinds(ValueKind::Int32, ValueKind::Int32) == ValueKind::Int32);
  CHECK(primec::ir_lowerer::combineNumericKinds(ValueKind::Int32, ValueKind::Int64) == ValueKind::Int64);
  CHECK(primec::ir_lowerer::combineNumericKinds(ValueKind::UInt64, ValueKind::UInt64) == ValueKind::UInt64);
  CHECK(primec::ir_lowerer::combineNumericKinds(ValueKind::Float32, ValueKind::Float32) == ValueKind::Float32);
  CHECK(primec::ir_lowerer::combineNumericKinds(ValueKind::Float64, ValueKind::Float64) == ValueKind::Float64);
  CHECK(primec::ir_lowerer::combineNumericKinds(ValueKind::Unknown, ValueKind::Int32) == ValueKind::Unknown);
  CHECK(primec::ir_lowerer::combineNumericKinds(ValueKind::Float32, ValueKind::Float64) == ValueKind::Unknown);
  CHECK(primec::ir_lowerer::combineNumericKinds(ValueKind::String, ValueKind::Int32) == ValueKind::Unknown);
  CHECK(primec::ir_lowerer::combineNumericKinds(ValueKind::Bool, ValueKind::Int32) == ValueKind::Unknown);
}

TEST_CASE("ir lowerer setup type helper normalizes bool comparison kinds") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;
  CHECK(primec::ir_lowerer::comparisonKind(ValueKind::Bool, ValueKind::Bool) == ValueKind::Int32);
  CHECK(primec::ir_lowerer::comparisonKind(ValueKind::Bool, ValueKind::Int64) == ValueKind::Int64);
  CHECK(primec::ir_lowerer::comparisonKind(ValueKind::Int32, ValueKind::Bool) == ValueKind::Int32);
}

TEST_CASE("ir lowerer setup type helper rejects mixed bool float comparison kinds") {
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;
  CHECK(primec::ir_lowerer::comparisonKind(ValueKind::Bool, ValueKind::Float32) == ValueKind::Unknown);
  CHECK(primec::ir_lowerer::comparisonKind(ValueKind::Float64, ValueKind::Bool) == ValueKind::Unknown);
}

TEST_CASE("ir lowerer setup type helper builds value-kind adapters") {
  auto valueKindFromTypeName = primec::ir_lowerer::makeValueKindFromTypeName();
  auto combineNumericKinds = primec::ir_lowerer::makeCombineNumericKinds();
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;

  CHECK(valueKindFromTypeName("i64") == ValueKind::Int64);
  CHECK(valueKindFromTypeName("Vec3") == ValueKind::Unknown);
  CHECK(combineNumericKinds(ValueKind::Int32, ValueKind::Int64) == ValueKind::Int64);
  CHECK(combineNumericKinds(ValueKind::Float32, ValueKind::Float64) == ValueKind::Unknown);
}

TEST_CASE("ir lowerer setup type helper builds bundled adapters") {
  auto adapters = primec::ir_lowerer::makeSetupTypeAdapters();
  using ValueKind = primec::ir_lowerer::LocalInfo::ValueKind;

  CHECK(adapters.valueKindFromTypeName("i64") == ValueKind::Int64);
  CHECK(adapters.valueKindFromTypeName("Vec3") == ValueKind::Unknown);
  CHECK(adapters.combineNumericKinds(ValueKind::Int32, ValueKind::Int64) == ValueKind::Int64);
  CHECK(adapters.combineNumericKinds(ValueKind::Float32, ValueKind::Float64) == ValueKind::Unknown);
}

TEST_CASE("ir lowerer setup math helper detects math imports") {
  CHECK(primec::ir_lowerer::isMathImportPath("/std/math/*"));
  CHECK(primec::ir_lowerer::isMathImportPath("/std/math/sin"));
  CHECK_FALSE(primec::ir_lowerer::isMathImportPath("/std"));
  CHECK_FALSE(primec::ir_lowerer::isMathImportPath("/std/math"));

  const std::vector<std::string> importsA = {"/std/io/*", "/std/math/*"};
  CHECK(primec::ir_lowerer::hasProgramMathImport(importsA));

  const std::vector<std::string> importsB = {"/std/io/*", "/std/math/sin"};
  CHECK(primec::ir_lowerer::hasProgramMathImport(importsB));

  const std::vector<std::string> importsC = {"/std/io/*", "/pkg/demo"};
  CHECK_FALSE(primec::ir_lowerer::hasProgramMathImport(importsC));
}

TEST_CASE("ir lowerer struct type helpers join template args text") {
  const std::vector<std::string> emptyArgs;
  CHECK(primec::ir_lowerer::joinTemplateArgsText(emptyArgs).empty());

  const std::vector<std::string> oneArg = {"i64"};
  CHECK(primec::ir_lowerer::joinTemplateArgsText(oneArg) == "i64");

  const std::vector<std::string> manyArgs = {"i64", "vector<f32>", "map<i32, bool>"};
  CHECK(primec::ir_lowerer::joinTemplateArgsText(manyArgs) == "i64, vector<f32>, map<i32, bool>");
}

TEST_CASE("ir lowerer struct type helpers resolve scoped struct paths") {
  const std::unordered_set<std::string> structNames = {"/pkg/Foo", "/imports/Bar", "/Baz"};
  const std::unordered_map<std::string, std::string> importAliases = {{"Bar", "/imports/Bar"}};
  std::string resolved;

  REQUIRE(primec::ir_lowerer::resolveStructTypePathFromScope(
      "/pkg/Foo", "", structNames, importAliases, resolved));
  CHECK(resolved == "/pkg/Foo");

  REQUIRE(primec::ir_lowerer::resolveStructTypePathFromScope(
      "Foo", "/pkg", structNames, importAliases, resolved));
  CHECK(resolved == "/pkg/Foo");

  REQUIRE(primec::ir_lowerer::resolveStructTypePathFromScope(
      "Bar", "/pkg", structNames, importAliases, resolved));
  CHECK(resolved == "/imports/Bar");

  REQUIRE(primec::ir_lowerer::resolveStructTypePathFromScope(
      "Baz", "", structNames, importAliases, resolved));
  CHECK(resolved == "/Baz");

  CHECK_FALSE(primec::ir_lowerer::resolveStructTypePathFromScope(
      "Missing", "/pkg", structNames, importAliases, resolved));
}

TEST_CASE("ir lowerer struct type helpers build scoped struct path resolver") {
  const std::unordered_set<std::string> structNames = {"/pkg/Foo", "/imports/Bar", "/Baz"};
  const std::unordered_map<std::string, std::string> importAliases = {{"Bar", "/imports/Bar"}};
  auto resolveStructTypePath =
      primec::ir_lowerer::makeResolveStructTypePathFromScope(structNames, importAliases);

  std::string resolved;
  REQUIRE(resolveStructTypePath("Foo", "/pkg", resolved));
  CHECK(resolved == "/pkg/Foo");
  REQUIRE(resolveStructTypePath("Bar", "/pkg", resolved));
  CHECK(resolved == "/imports/Bar");
  REQUIRE(resolveStructTypePath("Baz", "", resolved));
  CHECK(resolved == "/Baz");
  CHECK_FALSE(resolveStructTypePath("Missing", "/pkg", resolved));
}

TEST_CASE("ir lowerer struct type helpers resolve definition namespace prefixes from map") {
  primec::Definition namespacedDef;
  namespacedDef.fullPath = "/pkg/Foo";
  namespacedDef.namespacePrefix = "/pkg";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/pkg/Foo", &namespacedDef},
      {"/pkg/Null", nullptr},
  };

  std::string namespacePrefixOut;
  REQUIRE(primec::ir_lowerer::resolveDefinitionNamespacePrefixFromMap(
      defMap, "/pkg/Foo", namespacePrefixOut));
  CHECK(namespacePrefixOut == "/pkg");

  CHECK_FALSE(primec::ir_lowerer::resolveDefinitionNamespacePrefixFromMap(
      defMap, "/pkg/Missing", namespacePrefixOut));
  CHECK_FALSE(primec::ir_lowerer::resolveDefinitionNamespacePrefixFromMap(
      defMap, "/pkg/Null", namespacePrefixOut));
}
