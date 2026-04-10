TEST_CASE("ir lowerer setup type helper resolves builtin-like count call methods") {
  primec::Definition methodDef;
  methodDef.fullPath = "/array/count";
  primec::Definition stringMethodDef;
  stringMethodDef.fullPath = "/string/count";

  std::unordered_map<std::string, primec::ir_lowerer::ReturnInfo> infoByPath;
  primec::ir_lowerer::ReturnInfo scalarInfo;
  scalarInfo.returnsVoid = false;
  scalarInfo.returnsArray = false;
  scalarInfo.kind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  infoByPath.emplace("/array/count", scalarInfo);
  infoByPath.emplace("/string/count", scalarInfo);

  auto getReturnInfo = [&infoByPath](const std::string &path, primec::ir_lowerer::ReturnInfo &out) {
    auto it = infoByPath.find(path);
    if (it == infoByPath.end()) {
      return false;
    }
    out = it->second;
    return true;
  };

  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "items";

  primec::Expr countCall;
  countCall.kind = primec::Expr::Kind::Call;
  countCall.name = "count";
  countCall.args.push_back(receiverExpr);

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  bool methodResolved = false;
  CHECK(primec::ir_lowerer::resolveCountMethodCallReturnKind(
      countCall,
      {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [&methodDef](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return &methodDef; },
      getReturnInfo,
      false,
      kindOut,
      &methodResolved));
  CHECK(methodResolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Int64);

  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  methodResolved = false;
  CHECK(primec::ir_lowerer::resolveCountMethodCallReturnKind(
      countCall,
      {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
      [&stringMethodDef](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        return &stringMethodDef;
      },
      getReturnInfo,
      false,
      kindOut,
      &methodResolved));
  CHECK(methodResolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Int64);
}

TEST_CASE("ir lowerer setup type helper skips non-eligible count call method resolution") {
  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "items";

  primec::Expr countCall;
  countCall.kind = primec::Expr::Kind::Call;
  countCall.name = "count";
  countCall.args.push_back(receiverExpr);

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  bool methodResolved = true;
  CHECK_FALSE(primec::ir_lowerer::resolveCountMethodCallReturnKind(
      countCall,
      {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return nullptr; },
      {},
      false,
      kindOut,
      &methodResolved));
  CHECK_FALSE(methodResolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);

  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  methodResolved = true;
  CHECK_FALSE(primec::ir_lowerer::resolveCountMethodCallReturnKind(
      countCall,
      {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return true; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return nullptr; },
      {},
      false,
      kindOut,
      &methodResolved));
  CHECK_FALSE(methodResolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);

  countCall.isMethodCall = true;
  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  methodResolved = true;
  CHECK_FALSE(primec::ir_lowerer::resolveCountMethodCallReturnKind(
      countCall,
      {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return nullptr; },
      {},
      false,
      kindOut,
      &methodResolved));
  CHECK_FALSE(methodResolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
}

TEST_CASE("ir lowerer setup type helper resolves capacity call method return kinds") {
  primec::Definition methodDef;
  methodDef.fullPath = "/vector/capacity";

  std::unordered_map<std::string, primec::ir_lowerer::ReturnInfo> infoByPath;
  primec::ir_lowerer::ReturnInfo scalarInfo;
  scalarInfo.returnsVoid = false;
  scalarInfo.returnsArray = false;
  scalarInfo.kind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  infoByPath.emplace("/vector/capacity", scalarInfo);

  auto getReturnInfo = [&infoByPath](const std::string &path, primec::ir_lowerer::ReturnInfo &out) {
    auto it = infoByPath.find(path);
    if (it == infoByPath.end()) {
      return false;
    }
    out = it->second;
    return true;
  };

  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "items";

  primec::Expr capacityCall;
  capacityCall.kind = primec::Expr::Kind::Call;
  capacityCall.name = "capacity";
  capacityCall.args.push_back(receiverExpr);

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  bool methodResolved = false;
  CHECK(primec::ir_lowerer::resolveCapacityMethodCallReturnKind(
      capacityCall,
      {},
      [&methodDef](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return &methodDef; },
      getReturnInfo,
      false,
      kindOut,
      &methodResolved));
  CHECK(methodResolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Int64);

  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
  methodResolved = false;
  CHECK_FALSE(primec::ir_lowerer::resolveCapacityMethodCallReturnKind(
      capacityCall,
      {},
      [&methodDef](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return &methodDef; },
      getReturnInfo,
      true,
      kindOut,
      &methodResolved));
  CHECK(methodResolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
}

TEST_CASE("ir lowerer setup type helper skips non-eligible capacity call method resolution") {
  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "items";

  primec::Expr capacityCall;
  capacityCall.kind = primec::Expr::Kind::Call;
  capacityCall.name = "capacity";
  capacityCall.args.push_back(receiverExpr);

  primec::ir_lowerer::LocalInfo::ValueKind kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  bool methodResolved = true;
  CHECK_FALSE(primec::ir_lowerer::resolveCapacityMethodCallReturnKind(
      capacityCall,
      {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return nullptr; },
      {},
      false,
      kindOut,
      &methodResolved));
  CHECK_FALSE(methodResolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);

  capacityCall.isMethodCall = true;
  kindOut = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  methodResolved = true;
  CHECK_FALSE(primec::ir_lowerer::resolveCapacityMethodCallReturnKind(
      capacityCall,
      {},
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return nullptr; },
      {},
      false,
      kindOut,
      &methodResolved));
  CHECK_FALSE(methodResolved);
  CHECK(kindOut == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
}

TEST_CASE("ir lowerer return inference helper analyzes entry return transforms") {
  primec::Definition entryDef;
  primec::Transform resultReturn;
  resultReturn.name = "return";
  resultReturn.templateArgs = {"Result<i64, FileError>"};
  entryDef.transforms.push_back(resultReturn);

  primec::ir_lowerer::EntryReturnConfig out;
  std::string error;
  REQUIRE(primec::ir_lowerer::analyzeEntryReturnTransforms(entryDef, "/main", out, error));
  CHECK(error.empty());
  CHECK(out.hasReturnTransform);
  CHECK_FALSE(out.returnsVoid);
  CHECK(out.hasResultInfo);
  CHECK(out.resultInfo.isResult);
  CHECK(out.resultInfo.hasValue);
}

TEST_CASE("ir lowerer return inference helper handles void and diagnostics") {
  primec::Definition noReturnDef;
  primec::ir_lowerer::EntryReturnConfig out;
  std::string error;
  REQUIRE(primec::ir_lowerer::analyzeEntryReturnTransforms(noReturnDef, "/main", out, error));
  CHECK(out.returnsVoid);
  CHECK_FALSE(out.hasReturnTransform);

  primec::Definition invalidReturnDef;
  primec::Transform arrayStringReturn;
  arrayStringReturn.name = "return";
  arrayStringReturn.templateArgs = {"array<string>"};
  invalidReturnDef.transforms.push_back(arrayStringReturn);
  CHECK_FALSE(primec::ir_lowerer::analyzeEntryReturnTransforms(invalidReturnDef, "/main", out, error));
  CHECK(error == "native backend does not support string array return types on /main");
}

TEST_CASE("ir lowerer return inference helper reads semantic product return facts") {
  primec::Definition entryDef;
  entryDef.fullPath = "/main";
  entryDef.semanticNodeId = 41;

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
      .isExecution = false,
      .returnKind = "i32",
      .isCompute = false,
      .isUnsafe = false,
      .activeEffects = {"io_out"},
      .activeCapabilities = {"io_out"},
      .hasResultType = true,
      .resultTypeHasValue = true,
      .resultValueType = "i64",
      .resultErrorType = "FileError",
      .hasOnError = false,
      .onErrorHandlerPath = "",
      .onErrorErrorType = "",
      .onErrorBoundArgCount = 0,
      .semanticNodeId = 41,
      .provenanceHandle = 0,
      .fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
      .returnKindId = primec::semanticProgramInternCallTargetString(semanticProgram, "i32"),
      .activeEffectIds = {
          primec::semanticProgramInternCallTargetString(semanticProgram, "io_out"),
      },
      .activeCapabilityIds = {
          primec::semanticProgramInternCallTargetString(semanticProgram, "io_out"),
      },
      .resultValueTypeId = primec::semanticProgramInternCallTargetString(semanticProgram, "i64"),
      .resultErrorTypeId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "FileError"),
  });
  semanticProgram.returnFacts.push_back(primec::SemanticProgramReturnFact{
      "/semantic/main",
      "i32",
      "",
      "Result<i64, FileError>",
      false,
      false,
      false,
      "",
      4,
      2,
      41,
  });

  primec::ir_lowerer::EntryReturnConfig out;
  std::string error;
  REQUIRE(primec::ir_lowerer::analyzeEntryReturnTransforms(entryDef, &semanticProgram, "/main", out, error));
  CHECK(error.empty());
  CHECK(out.hasReturnTransform);
  CHECK_FALSE(out.returnsVoid);
  CHECK(out.hasResultInfo);
  CHECK(out.resultInfo.isResult);
  CHECK(out.resultInfo.hasValue);
}

TEST_CASE("ir lowerer return inference helper analyzes declared return transforms") {
  primec::Definition def;
  def.fullPath = "/pkg/main";
  def.namespacePrefix = "/pkg";

  primec::Transform returnAuto;
  returnAuto.name = "return";
  returnAuto.templateArgs = {"auto"};
  def.transforms.push_back(returnAuto);

  primec::Transform returnResult;
  returnResult.name = "return";
  returnResult.templateArgs = {"Result<i64, FileError>"};
  def.transforms.push_back(returnResult);

  primec::ir_lowerer::ReturnInfo info;
  bool hasReturnTransform = false;
  bool hasReturnAuto = false;
  primec::ir_lowerer::analyzeDeclaredReturnTransforms(
      def,
      [](const std::string &, const std::string &, std::string &) { return false; },
      [](const std::string &, primec::ir_lowerer::StructArrayTypeInfo &) { return false; },
      info,
      hasReturnTransform,
      hasReturnAuto);

  CHECK(hasReturnTransform);
  CHECK(hasReturnAuto);
  CHECK(info.isResult);
  CHECK(info.resultHasValue);
  CHECK(info.resultErrorType == "FileError");
  CHECK(info.kind == primec::ir_lowerer::LocalInfo::ValueKind::Int64);
  CHECK_FALSE(info.returnsVoid);
}

TEST_CASE("ir lowerer return inference helper resolves declared array and struct returns") {
  primec::Definition defArray;
  primec::Transform returnArray;
  returnArray.name = "return";
  returnArray.templateArgs = {"array<i32>"};
  defArray.transforms.push_back(returnArray);

  primec::ir_lowerer::ReturnInfo info;
  bool hasReturnTransform = false;
  bool hasReturnAuto = false;
  primec::ir_lowerer::analyzeDeclaredReturnTransforms(
      defArray,
      [](const std::string &, const std::string &, std::string &) { return false; },
      [](const std::string &, primec::ir_lowerer::StructArrayTypeInfo &) { return false; },
      info,
      hasReturnTransform,
      hasReturnAuto);

  CHECK(hasReturnTransform);
  CHECK_FALSE(hasReturnAuto);
  CHECK(info.returnsArray);
  CHECK(info.kind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);

  primec::Definition defStruct;
  defStruct.namespacePrefix = "/pkg";
  primec::Transform returnStruct;
  returnStruct.name = "return";
  returnStruct.templateArgs = {"VecLike"};
  defStruct.transforms.push_back(returnStruct);

  info = primec::ir_lowerer::ReturnInfo{};
  hasReturnTransform = false;
  hasReturnAuto = false;
  primec::ir_lowerer::analyzeDeclaredReturnTransforms(
      defStruct,
      [](const std::string &typeName, const std::string &, std::string &pathOut) {
        if (typeName == "VecLike") {
          pathOut = "/pkg/VecLike";
          return true;
        }
        return false;
      },
      [](const std::string &path, primec::ir_lowerer::StructArrayTypeInfo &infoOut) {
        if (path == "/pkg/VecLike") {
          infoOut.elementKind = primec::ir_lowerer::LocalInfo::ValueKind::Float32;
          return true;
        }
        return false;
      },
      info,
      hasReturnTransform,
      hasReturnAuto);

  CHECK(hasReturnTransform);
  CHECK_FALSE(hasReturnAuto);
  CHECK(info.returnsArray);
  CHECK(info.kind == primec::ir_lowerer::LocalInfo::ValueKind::Float32);

  primec::Definition defVectorStruct;
  defVectorStruct.namespacePrefix = "/pkg";
  primec::Transform returnVectorStruct;
  returnVectorStruct.name = "return";
  returnVectorStruct.templateArgs = {"vector<VecLike>"};
  defVectorStruct.transforms.push_back(returnVectorStruct);

  info = primec::ir_lowerer::ReturnInfo{};
  hasReturnTransform = false;
  hasReturnAuto = false;
  primec::ir_lowerer::analyzeDeclaredReturnTransforms(
      defVectorStruct,
      [](const std::string &typeName, const std::string &, std::string &pathOut) {
        if (typeName == "VecLike") {
          pathOut = "/pkg/VecLike";
          return true;
        }
        return false;
      },
      [](const std::string &, primec::ir_lowerer::StructArrayTypeInfo &) { return false; },
      info,
      hasReturnTransform,
      hasReturnAuto);

  CHECK(hasReturnTransform);
  CHECK_FALSE(hasReturnAuto);
  CHECK(info.returnsArray);
  CHECK(info.kind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
}

TEST_CASE("ir lowerer return inference helper unwraps referenced collection returns") {
  primec::Definition def;
  primec::Transform returnTransform;
  returnTransform.name = "return";
  returnTransform.templateArgs = {"Reference</std/collections/map<i32, string>>"};
  def.transforms.push_back(returnTransform);

  primec::ir_lowerer::ReturnInfo info;
  bool hasReturnTransform = false;
  bool hasReturnAuto = false;
  primec::ir_lowerer::analyzeDeclaredReturnTransforms(
      def,
      [](const std::string &, const std::string &, std::string &) { return false; },
      [](const std::string &, primec::ir_lowerer::StructArrayTypeInfo &) { return false; },
      info,
      hasReturnTransform,
      hasReturnAuto);

  CHECK(hasReturnTransform);
  CHECK_FALSE(hasReturnAuto);
  CHECK(info.returnsArray);
  CHECK(info.kind == primec::ir_lowerer::LocalInfo::ValueKind::String);
}

TEST_CASE("ir lowerer setup type infers referenced declared collection receivers") {
  primec::Definition def;
  primec::Transform returnTransform;
  returnTransform.name = "return";
  returnTransform.templateArgs = {"Reference</std/collections/map<i32, string>>"};
  def.transforms.push_back(returnTransform);

  std::string typeName;
  CHECK(primec::ir_lowerer::inferReceiverTypeFromDeclaredReturn(def, typeName));
  CHECK(typeName == "map");
}

TEST_CASE("ir lowerer inline call context helper prepares scoped setup") {
  primec::Definition callee;
  callee.fullPath = "/pkg/do_work";

  primec::ir_lowerer::ReturnInfo returnInfo;
  returnInfo.returnsVoid = false;
  returnInfo.isResult = true;
  returnInfo.resultHasValue = false;
  returnInfo.kind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;

  primec::ir_lowerer::OnErrorHandler handler;
  handler.handlerPath = "/pkg/on_error";

  std::unordered_set<std::string> inlineStack;
  std::unordered_set<std::string> loweredCallTargets;
  primec::ir_lowerer::OnErrorByDefinition onErrorByDef;
  onErrorByDef.emplace(callee.fullPath, handler);

  primec::ir_lowerer::InlineDefinitionCallContextSetup out;
  std::string error;
  REQUIRE(primec::ir_lowerer::prepareInlineDefinitionCallContext(
      callee,
      true,
      [&](const std::string &path, primec::ir_lowerer::ReturnInfo &infoOut) {
        if (path != callee.fullPath) {
          return false;
        }
        infoOut = returnInfo;
        return true;
      },
      [](const primec::Definition &) { return false; },
      inlineStack,
      loweredCallTargets,
      onErrorByDef,
      out,
      error));
  CHECK(error.empty());
  CHECK(out.returnInfo.isResult);
  CHECK_FALSE(out.returnInfo.resultHasValue);
  CHECK(out.returnInfo.kind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  CHECK_FALSE(out.structDefinition);
  REQUIRE(out.scopedOnError.has_value());
  CHECK(out.scopedOnError->handlerPath == "/pkg/on_error");
  REQUIRE(out.scopedResult.has_value());
  CHECK(out.scopedResult->isResult);
  CHECK_FALSE(out.scopedResult->hasValue);
  CHECK(inlineStack.count(callee.fullPath) == 1u);
  CHECK(loweredCallTargets.count(callee.fullPath) == 1u);
}

TEST_CASE("ir lowerer inline call context helper reports setup diagnostics") {
  primec::Definition callee;
  callee.fullPath = "/pkg/do_work";

  primec::ir_lowerer::InlineDefinitionCallContextSetup out;
  std::string error;

  std::unordered_set<std::string> inlineStack;
  std::unordered_set<std::string> loweredCallTargets;
  primec::ir_lowerer::OnErrorByDefinition onErrorByDef;

  CHECK_FALSE(primec::ir_lowerer::prepareInlineDefinitionCallContext(
      callee,
      true,
      [](const std::string &, primec::ir_lowerer::ReturnInfo &infoOut) {
        infoOut = primec::ir_lowerer::ReturnInfo{};
        infoOut.returnsVoid = true;
        return true;
      },
      [](const primec::Definition &) { return false; },
      inlineStack,
      loweredCallTargets,
      onErrorByDef,
      out,
      error));
  CHECK(error == "void call not allowed in expression context: /pkg/do_work");
  CHECK(inlineStack.empty());
  CHECK(loweredCallTargets.empty());

  error.clear();
  inlineStack.insert(callee.fullPath);
  CHECK_FALSE(primec::ir_lowerer::prepareInlineDefinitionCallContext(
      callee,
      false,
      [](const std::string &, primec::ir_lowerer::ReturnInfo &infoOut) {
        infoOut = primec::ir_lowerer::ReturnInfo{};
        infoOut.returnsVoid = false;
        return true;
      },
      [](const primec::Definition &) { return false; },
      inlineStack,
      loweredCallTargets,
      onErrorByDef,
      out,
      error));
  CHECK(error == "native backend does not support recursive calls: /pkg/do_work");
  CHECK(loweredCallTargets.empty());

  error.clear();
  inlineStack.clear();
  CHECK_FALSE(primec::ir_lowerer::prepareInlineDefinitionCallContext(
      callee,
      false,
      [](const std::string &, primec::ir_lowerer::ReturnInfo &) { return false; },
      [](const primec::Definition &) { return false; },
      inlineStack,
      loweredCallTargets,
      onErrorByDef,
      out,
      error));
  CHECK(error.empty());
  CHECK(inlineStack.empty());
  CHECK(loweredCallTargets.empty());
}

TEST_CASE("ir lowerer inline struct arg helper emits struct constructor argument flow") {
  primec::Expr scalarArg;
  scalarArg.kind = primec::Expr::Kind::Name;
  scalarArg.name = "x";
  primec::Expr nestedArg;
  nestedArg.kind = primec::Expr::Kind::Name;
  nestedArg.name = "nested";

  const std::vector<const primec::Expr *> orderedArgs = {&scalarArg, &nestedArg};
  primec::ir_lowerer::LocalMap callerLocals;

  primec::ir_lowerer::StructSlotLayoutInfo layout;
  layout.structPath = "/pkg/Vec";
  layout.totalSlots = 4;
  primec::ir_lowerer::StructSlotFieldInfo scalarField;
  scalarField.name = "x";
  scalarField.slotOffset = 1;
  scalarField.slotCount = 1;
  scalarField.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Int32;
  layout.fields.push_back(scalarField);
  primec::ir_lowerer::StructSlotFieldInfo nestedField;
  nestedField.name = "nested";
  nestedField.slotOffset = 2;
  nestedField.slotCount = 2;
  nestedField.structPath = "/pkg/Nested";
  layout.fields.push_back(nestedField);

  int32_t nextLocal = 10;
  int32_t nextTempLocal = 50;
  std::vector<primec::IrInstruction> instructions;
  int emitExprCalls = 0;
  int copyCalls = 0;
  int32_t copiedDest = -1;
  int32_t copiedSrc = -1;
  int32_t copiedCount = -1;
  std::string error;
  REQUIRE(primec::ir_lowerer::emitInlineStructDefinitionArguments(
      "/pkg/Vec",
      orderedArgs,
      callerLocals,
      true,
      nextLocal,
      [&](const std::string &path, primec::ir_lowerer::StructSlotLayoutInfo &layoutOut) {
        if (path != "/pkg/Vec") {
          return false;
        }
        layoutOut = layout;
        return true;
      },
      [](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
        if (expr.name == "x") {
          return primec::ir_lowerer::LocalInfo::ValueKind::Int32;
        }
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const primec::Expr &expr, const primec::ir_lowerer::LocalMap &) {
        if (expr.name == "nested") {
          return std::string("/pkg/Nested");
        }
        return std::string();
      },
      [&](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
        ++emitExprCalls;
        return true;
      },
      [&](int32_t destBaseLocal, int32_t srcPtrLocal, int32_t slotCount) {
        ++copyCalls;
        copiedDest = destBaseLocal;
        copiedSrc = srcPtrLocal;
        copiedCount = slotCount;
        return true;
      },
      [&]() { return nextTempLocal++; },
      [&](primec::IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
      error));
  CHECK(error.empty());
  CHECK(nextLocal == 15);
  CHECK(emitExprCalls == 2);
  CHECK(copyCalls == 1);
  CHECK(copiedDest == 12);
  CHECK(copiedSrc == 50);
  CHECK(copiedCount == 2);
  REQUIRE(instructions.size() == 7u);
  CHECK(instructions[0].op == primec::IrOpcode::PushI32);
  CHECK(instructions[0].imm == 3u);
  CHECK(instructions[1].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[1].imm == 10u);
  CHECK(instructions[2].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[2].imm == 11u);
  CHECK(instructions[3].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[3].imm == 50u);
  CHECK(instructions[4].op == primec::IrOpcode::AddressOfLocal);
  CHECK(instructions[4].imm == 12u);
  CHECK(instructions[5].op == primec::IrOpcode::StoreLocal);
  CHECK(instructions[5].imm == 14u);
  CHECK(instructions[6].op == primec::IrOpcode::AddressOfLocal);
  CHECK(instructions[6].imm == 10u);
}
