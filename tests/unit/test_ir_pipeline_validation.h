TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir validator accepts lowered canonical module") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}
)";
  primec::Program program;
  std::string error;
  REQUIRE(parseAndValidate(source, program, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, "/main", {}, {}, module, error));
  CHECK(error.empty());

  CHECK(primec::validateIrModule(module, primec::IrValidationTarget::Any, error));
  CHECK(error.empty());
  CHECK(primec::validateIrModule(module, primec::IrValidationTarget::Vm, error));
  CHECK(error.empty());
  CHECK(primec::validateIrModule(module, primec::IrValidationTarget::Native, error));
  CHECK(error.empty());
}

TEST_CASE("ir lowerer effects unit resolves entry and non-entry defaults") {
  const std::vector<primec::Transform> transforms;
  const std::vector<std::string> defaultEffects = {"io_out"};
  const std::vector<std::string> entryDefaultEffects = {"io_err"};

  const auto entryActive =
      primec::ir_lowerer::resolveActiveEffects(transforms, true, defaultEffects, entryDefaultEffects);
  CHECK(entryActive.size() == 1);
  CHECK(entryActive.count("io_err") == 1);
  CHECK(entryActive.count("io_out") == 0);

  const auto nonEntryActive =
      primec::ir_lowerer::resolveActiveEffects(transforms, false, defaultEffects, entryDefaultEffects);
  CHECK(nonEntryActive.size() == 1);
  CHECK(nonEntryActive.count("io_out") == 1);
  CHECK(nonEntryActive.count("io_err") == 0);
}

TEST_CASE("ir lowerer effects unit rejects software numeric envelopes") {
  primec::Program program;
  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  primec::Transform returnTransform;
  returnTransform.name = "return";
  returnTransform.templateArgs.push_back("decimal");
  mainDef.transforms.push_back(returnTransform);
  program.definitions.push_back(std::move(mainDef));

  std::string error;
  CHECK_FALSE(primec::ir_lowerer::validateNoSoftwareNumericTypes(program, error));
  CHECK(error == "native backend does not support software numeric types: decimal");
}

TEST_CASE("ir lowerer call helpers resolve direct definition calls only") {
  primec::Definition callee;
  callee.fullPath = "/callee";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {{"/callee", &callee}};
  auto resolver = [](const primec::Expr &) { return std::string("/callee"); };

  primec::Expr directCall;
  directCall.kind = primec::Expr::Kind::Call;
  directCall.name = "callee";
  CHECK(primec::ir_lowerer::resolveDefinitionCall(directCall, defMap, resolver) == &callee);

  primec::Expr methodCall = directCall;
  methodCall.isMethodCall = true;
  CHECK(primec::ir_lowerer::resolveDefinitionCall(methodCall, defMap, resolver) == nullptr);

  primec::Expr bindingCall = directCall;
  bindingCall.isBinding = true;
  CHECK(primec::ir_lowerer::resolveDefinitionCall(bindingCall, defMap, resolver) == nullptr);
}

TEST_CASE("ir lowerer call helpers order positional named and default args") {
  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "demo";
  primec::Expr argA;
  argA.kind = primec::Expr::Kind::Literal;
  argA.literalValue = 1;
  primec::Expr argC;
  argC.kind = primec::Expr::Kind::Literal;
  argC.literalValue = 3;
  callExpr.args = {argA, argC};
  callExpr.argNames = {std::nullopt, std::make_optional<std::string>("c")};

  primec::Expr paramA;
  paramA.name = "a";
  primec::Expr paramB;
  paramB.name = "b";
  primec::Expr defaultB;
  defaultB.kind = primec::Expr::Kind::Literal;
  defaultB.literalValue = 2;
  paramB.args.push_back(defaultB);
  primec::Expr paramC;
  paramC.name = "c";
  const std::vector<primec::Expr> params = {paramA, paramB, paramC};

  std::vector<const primec::Expr *> ordered;
  std::string error;
  REQUIRE(primec::ir_lowerer::buildOrderedCallArguments(callExpr, params, ordered, error));
  CHECK(error.empty());
  REQUIRE(ordered.size() == 3);
  REQUIRE(ordered[0] != nullptr);
  CHECK(ordered[0]->literalValue == 1);
  REQUIRE(ordered[1] != nullptr);
  CHECK(ordered[1]->literalValue == 2);
  REQUIRE(ordered[2] != nullptr);
  CHECK(ordered[2]->literalValue == 3);
}

TEST_CASE("ir lowerer call helpers reject unknown named arg") {
  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "demo";
  primec::Expr arg;
  arg.kind = primec::Expr::Kind::Literal;
  arg.literalValue = 1;
  callExpr.args = {arg};
  callExpr.argNames = {std::make_optional<std::string>("missing")};

  primec::Expr param;
  param.name = "a";
  const std::vector<primec::Expr> params = {param};

  std::vector<const primec::Expr *> ordered;
  std::string error;
  CHECK_FALSE(primec::ir_lowerer::buildOrderedCallArguments(callExpr, params, ordered, error));
  CHECK(error == "unknown named argument: missing");
}

TEST_CASE("ir lowerer on_error helpers wire definition handlers") {
  primec::Program program;

  primec::Definition handlerDef;
  handlerDef.fullPath = "/handler";
  handlerDef.namespacePrefix = "";
  program.definitions.push_back(handlerDef);

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.namespacePrefix = "";
  primec::Transform onError;
  onError.name = "on_error";
  onError.templateArgs = {"FileError", "handler"};
  onError.arguments = {"1i32"};
  mainDef.transforms.push_back(onError);
  program.definitions.push_back(mainDef);

  auto resolveExprPath = [](const primec::Expr &expr) {
    if (!expr.name.empty() && expr.name[0] == '/') {
      return expr.name;
    }
    return std::string("/") + expr.name;
  };
  auto definitionExists = [](const std::string &path) { return path == "/handler" || path == "/main"; };

  primec::ir_lowerer::OnErrorByDefinition onErrorByDef;
  std::string error;
  REQUIRE(primec::ir_lowerer::buildOnErrorByDefinition(
      program, resolveExprPath, definitionExists, onErrorByDef, error));
  CHECK(error.empty());

  REQUIRE(onErrorByDef.count("/main") == 1);
  REQUIRE(onErrorByDef.at("/main").has_value());
  CHECK(onErrorByDef.at("/main")->handlerPath == "/handler");
  REQUIRE(onErrorByDef.at("/main")->boundArgs.size() == 1);
  CHECK(onErrorByDef.at("/main")->boundArgs.front().kind == primec::Expr::Kind::Literal);
  CHECK(onErrorByDef.at("/main")->boundArgs.front().literalValue == 1);

  REQUIRE(onErrorByDef.count("/handler") == 1);
  CHECK_FALSE(onErrorByDef.at("/handler").has_value());
}

TEST_CASE("ir lowerer on_error helpers reject unknown handler") {
  primec::Transform onError;
  onError.name = "on_error";
  onError.templateArgs = {"FileError", "missing"};
  const std::vector<primec::Transform> transforms = {onError};

  std::optional<primec::ir_lowerer::OnErrorHandler> out;
  std::string error;
  CHECK_FALSE(primec::ir_lowerer::parseOnErrorTransform(
      transforms,
      "",
      "/main",
      [](const primec::Expr &expr) { return std::string("/") + expr.name; },
      [](const std::string &) { return false; },
      out,
      error));
  CHECK(error == "unknown on_error handler: /missing");
}

TEST_CASE("ir lowerer setup type helper maps primitive aliases") {
  CHECK(primec::ir_lowerer::valueKindFromTypeName("int") == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  CHECK(primec::ir_lowerer::valueKindFromTypeName("i32") == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  CHECK(primec::ir_lowerer::valueKindFromTypeName("float") == primec::ir_lowerer::LocalInfo::ValueKind::Float32);
  CHECK(primec::ir_lowerer::valueKindFromTypeName("f64") == primec::ir_lowerer::LocalInfo::ValueKind::Float64);
  CHECK(primec::ir_lowerer::valueKindFromTypeName("bool") == primec::ir_lowerer::LocalInfo::ValueKind::Bool);
  CHECK(primec::ir_lowerer::valueKindFromTypeName("string") == primec::ir_lowerer::LocalInfo::ValueKind::String);
}

TEST_CASE("ir lowerer setup type helper maps file and fileerror types") {
  CHECK(primec::ir_lowerer::valueKindFromTypeName("FileError") == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  CHECK(primec::ir_lowerer::valueKindFromTypeName("File<Read>") == primec::ir_lowerer::LocalInfo::ValueKind::Int64);
  CHECK(primec::ir_lowerer::valueKindFromTypeName("File<Write>") == primec::ir_lowerer::LocalInfo::ValueKind::Int64);
}

TEST_CASE("ir lowerer setup type helper returns unknown for unsupported names") {
  CHECK(primec::ir_lowerer::valueKindFromTypeName("Vec3") == primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
  CHECK(primec::ir_lowerer::valueKindFromTypeName("Result<FileError>") ==
        primec::ir_lowerer::LocalInfo::ValueKind::Unknown);
}

TEST_CASE("ir lowerer setup math helper resolves namespaced builtins") {
  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "/std/math/sin";

  std::string builtinName;
  CHECK(primec::ir_lowerer::getSetupMathBuiltinName(callExpr, false, builtinName));
  CHECK(builtinName == "sin");
}

TEST_CASE("ir lowerer setup math helper requires import for bare names") {
  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "sin";

  std::string builtinName;
  CHECK_FALSE(primec::ir_lowerer::getSetupMathBuiltinName(callExpr, false, builtinName));
  CHECK(primec::ir_lowerer::getSetupMathBuiltinName(callExpr, true, builtinName));
  CHECK(builtinName == "sin");
}

TEST_CASE("ir lowerer setup math helper resolves constants only for supported names") {
  std::string constantName;
  CHECK(primec::ir_lowerer::getSetupMathConstantName("/std/math/pi", false, constantName));
  CHECK(constantName == "pi");
  CHECK_FALSE(primec::ir_lowerer::getSetupMathConstantName("phi", true, constantName));
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

TEST_CASE("ir lowerer statement binding helper selects uninitialized zero opcodes") {
  primec::IrOpcode mapZeroOp = primec::IrOpcode::PushI32;
  uint64_t mapZeroImm = 123;
  std::string error;
  REQUIRE(primec::ir_lowerer::selectUninitializedStorageZeroInstruction(
      primec::ir_lowerer::LocalInfo::Kind::Map,
      primec::ir_lowerer::LocalInfo::ValueKind::Unknown,
      "mapStorage",
      mapZeroOp,
      mapZeroImm,
      error));
  CHECK(mapZeroOp == primec::IrOpcode::PushI64);
  CHECK(mapZeroImm == 0);
  CHECK(error.empty());

  primec::IrOpcode floatZeroOp = primec::IrOpcode::PushI32;
  uint64_t floatZeroImm = 99;
  REQUIRE(primec::ir_lowerer::selectUninitializedStorageZeroInstruction(
      primec::ir_lowerer::LocalInfo::Kind::Value,
      primec::ir_lowerer::LocalInfo::ValueKind::Float32,
      "floatStorage",
      floatZeroOp,
      floatZeroImm,
      error));
  CHECK(floatZeroOp == primec::IrOpcode::PushF32);
  CHECK(floatZeroImm == 0);
}

TEST_CASE("ir lowerer statement binding helper rejects unknown uninitialized value kind") {
  primec::IrOpcode zeroOp = primec::IrOpcode::PushI32;
  uint64_t zeroImm = 0;
  std::string error;
  CHECK_FALSE(primec::ir_lowerer::selectUninitializedStorageZeroInstruction(
      primec::ir_lowerer::LocalInfo::Kind::Value,
      primec::ir_lowerer::LocalInfo::ValueKind::Unknown,
      "storageSlot",
      zeroOp,
      zeroImm,
      error));
  CHECK(error == "native backend requires a concrete uninitialized storage type on storageSlot");
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
      [](primec::ir_lowerer::LocalInfo::ValueKind leftKind, primec::ir_lowerer::LocalInfo::ValueKind rightKind) {
        return (leftKind == rightKind) ? leftKind : primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](primec::IrOpcode, uint64_t) {},
      error);

  CHECK(result == primec::ir_lowerer::OperatorArithmeticEmitResult::Error);
  CHECK(error == "pointer arithmetic requires pointer on the left");
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
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return false; },
      [](const std::string &typeName) {
        if (typeName == "f32") {
          return primec::ir_lowerer::LocalInfo::ValueKind::Float32;
        }
        return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
      },
      [](const std::string &, std::string &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string(); },
      [](const std::string &, int32_t &) { return false; },
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
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return false; },
      [](const std::string &) { return primec::ir_lowerer::LocalInfo::ValueKind::Unknown; },
      [](const std::string &, std::string &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string(); },
      [](const std::string &, int32_t &) { return false; },
      [](int32_t, int32_t, int32_t) { return false; },
      instructions,
      handled,
      error);

  CHECK_FALSE(ok);
  CHECK(handled);
  CHECK(error == "assign target must be mutable: x");
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
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &, int32_t &, size_t &) { return false; },
      [](const std::string &) { return primec::ir_lowerer::LocalInfo::ValueKind::Unknown; },
      [](const std::string &, std::string &) { return false; },
      [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) { return std::string(); },
      [](const std::string &, int32_t &) { return false; },
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

TEST_CASE("ir lowerer result helpers resolve definition result metadata") {
  primec::Definition callee;
  callee.fullPath = "/make";

  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "make";

  auto lookupLocal = [](const std::string &) { return primec::ir_lowerer::LocalResultInfo{}; };
  auto resolveNone = [](const primec::Expr &) -> const primec::Definition * { return nullptr; };
  auto resolveDirect = [&](const primec::Expr &) -> const primec::Definition * { return &callee; };
  auto lookupDefinition = [](const std::string &path, primec::ir_lowerer::ResultExprInfo &out) {
    if (path != "/make") {
      return false;
    }
    out.isResult = true;
    out.hasValue = false;
    out.errorType = "FileError";
    return true;
  };

  primec::ir_lowerer::ResultExprInfo out;
  CHECK(primec::ir_lowerer::resolveResultExprInfo(
      callExpr, lookupLocal, resolveNone, resolveDirect, lookupDefinition, out));
  CHECK(out.isResult);
  CHECK_FALSE(out.hasValue);
  CHECK(out.errorType == "FileError");
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

TEST_CASE("ir lowerer string call helpers emit literal and binding values") {
  std::vector<primec::IrInstruction> instructions;
  auto emitInstruction = [&](primec::IrOpcode op, uint64_t imm) {
    instructions.push_back({op, imm});
  };
  auto internString = [](const std::string &text) -> int32_t {
    if (text == "hello") {
      return 7;
    }
    return 9;
  };

  primec::Expr literalArg;
  literalArg.kind = primec::Expr::Kind::StringLiteral;
  literalArg.stringValue = "\"hello\"utf8";

  primec::ir_lowerer::StringCallSource source = primec::ir_lowerer::StringCallSource::None;
  int32_t stringIndex = -1;
  bool argvChecked = true;
  std::string error;

  CHECK(primec::ir_lowerer::emitLiteralOrBindingStringCallValue(
            literalArg,
            internString,
            emitInstruction,
            [](const std::string &) { return primec::ir_lowerer::StringBindingInfo{}; },
            source,
            stringIndex,
            argvChecked,
            error) == primec::ir_lowerer::StringCallEmitResult::Handled);
  CHECK(source == primec::ir_lowerer::StringCallSource::TableIndex);
  CHECK(stringIndex == 7);
  CHECK(argvChecked);
  REQUIRE(instructions.size() == 1);
  CHECK(instructions[0].op == primec::IrOpcode::PushI64);
  CHECK(instructions[0].imm == 7);

  instructions.clear();
  primec::Expr nameArg;
  nameArg.kind = primec::Expr::Kind::Name;
  nameArg.name = "text";
  CHECK(primec::ir_lowerer::emitLiteralOrBindingStringCallValue(
            nameArg,
            internString,
            emitInstruction,
            [](const std::string &name) {
              primec::ir_lowerer::StringBindingInfo binding;
              if (name == "text") {
                binding.found = true;
                binding.isString = true;
                binding.localIndex = 42;
                binding.source = primec::ir_lowerer::StringCallSource::RuntimeIndex;
                binding.argvChecked = true;
              }
              return binding;
            },
            source,
            stringIndex,
            argvChecked,
            error) == primec::ir_lowerer::StringCallEmitResult::Handled);
  CHECK(source == primec::ir_lowerer::StringCallSource::RuntimeIndex);
  REQUIRE(instructions.size() == 1);
  CHECK(instructions[0].op == primec::IrOpcode::LoadLocal);
  CHECK(instructions[0].imm == 42);
}

TEST_CASE("ir lowerer string call helpers surface errors and not-handled cases") {
  std::vector<primec::IrInstruction> instructions;
  auto emitInstruction = [&](primec::IrOpcode op, uint64_t imm) {
    instructions.push_back({op, imm});
  };
  auto internString = [](const std::string &) -> int32_t { return 1; };

  primec::Expr badLiteral;
  badLiteral.kind = primec::Expr::Kind::StringLiteral;
  badLiteral.stringValue = "\"x\"bogus";
  primec::ir_lowerer::StringCallSource source = primec::ir_lowerer::StringCallSource::None;
  int32_t stringIndex = -1;
  bool argvChecked = true;
  std::string error;
  CHECK(primec::ir_lowerer::emitLiteralOrBindingStringCallValue(
            badLiteral,
            internString,
            emitInstruction,
            [](const std::string &) { return primec::ir_lowerer::StringBindingInfo{}; },
            source,
            stringIndex,
            argvChecked,
            error) == primec::ir_lowerer::StringCallEmitResult::Error);
  CHECK(error.find("unknown string literal suffix") != std::string::npos);

  instructions.clear();
  primec::Expr unknownName;
  unknownName.kind = primec::Expr::Kind::Name;
  unknownName.name = "missing";
  CHECK(primec::ir_lowerer::emitLiteralOrBindingStringCallValue(
            unknownName,
            internString,
            emitInstruction,
            [](const std::string &) { return primec::ir_lowerer::StringBindingInfo{}; },
            source,
            stringIndex,
            argvChecked,
            error) == primec::ir_lowerer::StringCallEmitResult::Error);
  CHECK(error == "native backend does not know identifier: missing");

  instructions.clear();
  primec::Expr callArg;
  callArg.kind = primec::Expr::Kind::Call;
  callArg.name = "at";
  CHECK(primec::ir_lowerer::emitLiteralOrBindingStringCallValue(
            callArg,
            internString,
            emitInstruction,
            [](const std::string &) { return primec::ir_lowerer::StringBindingInfo{}; },
            source,
            stringIndex,
            argvChecked,
            error) == primec::ir_lowerer::StringCallEmitResult::NotHandled);
  CHECK(instructions.empty());
}

TEST_CASE("ir lowerer string call helpers handle call-expression paths") {
  std::vector<primec::IrInstruction> instructions;
  auto emitInstruction = [&](primec::IrOpcode op, uint64_t imm) {
    instructions.push_back({op, imm});
  };
  auto getInstructionCount = [&]() { return instructions.size(); };
  auto patchInstructionImm = [&](size_t index, int32_t imm) { instructions.at(index).imm = imm; };

  primec::Expr argvExpr;
  argvExpr.kind = primec::Expr::Kind::Name;
  argvExpr.name = "argv";
  primec::Expr indexExpr;
  indexExpr.kind = primec::Expr::Kind::Literal;
  indexExpr.literalValue = 2;
  primec::Expr atCall;
  atCall.kind = primec::Expr::Kind::Call;
  atCall.name = "at";
  atCall.args = {argvExpr, indexExpr};

  primec::ir_lowerer::StringCallSource source = primec::ir_lowerer::StringCallSource::None;
  bool argvChecked = false;
  std::string error;
  int emittedIndexExprs = 0;

  CHECK(primec::ir_lowerer::emitCallStringCallValue(
            atCall,
            [](const primec::Expr &callExpr, std::string &nameOut) {
              if (callExpr.name == "at" || callExpr.name == "unchecked_at") {
                nameOut = callExpr.name;
                return true;
              }
              return false;
            },
            [](const primec::Expr &targetExpr) {
              return targetExpr.kind == primec::Expr::Kind::Name && targetExpr.name == "argv";
            },
            [](const primec::Expr &, const std::string &, primec::ir_lowerer::StringIndexOps &ops, std::string &) {
              ops.pushZero = primec::IrOpcode::PushI32;
              ops.cmpLt = primec::IrOpcode::CmpLtI32;
              ops.cmpGe = primec::IrOpcode::CmpGeI32;
              ops.skipNegativeCheck = false;
              return true;
            },
            [&](const primec::Expr &) {
              emittedIndexExprs++;
              emitInstruction(primec::IrOpcode::PushI32, 2);
              return true;
            },
            [](const primec::Expr &) { return false; },
            []() { return 11; },
            emitInstruction,
            getInstructionCount,
            patchInstructionImm,
            [&]() { emitInstruction(primec::IrOpcode::PushI32, 999); },
            source,
            argvChecked,
            error) == primec::ir_lowerer::StringCallEmitResult::Handled);
  CHECK(error.empty());
  CHECK(source == primec::ir_lowerer::StringCallSource::ArgvIndex);
  CHECK(argvChecked);
  CHECK(emittedIndexExprs == 1);
  REQUIRE_FALSE(instructions.empty());
  CHECK(instructions.back().op == primec::IrOpcode::LoadLocal);
  CHECK(instructions.back().imm == 11);

  instructions.clear();
  source = primec::ir_lowerer::StringCallSource::None;
  argvChecked = true;
  bool runtimeExprEmitted = false;
  primec::Expr runtimeCall;
  runtimeCall.kind = primec::Expr::Kind::Call;
  runtimeCall.name = "build_string";
  CHECK(primec::ir_lowerer::emitCallStringCallValue(
            runtimeCall,
            [](const primec::Expr &, std::string &) { return false; },
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &, const std::string &, primec::ir_lowerer::StringIndexOps &, std::string &) {
              return false;
            },
            [&](const primec::Expr &) {
              runtimeExprEmitted = true;
              emitInstruction(primec::IrOpcode::LoadLocal, 4);
              return true;
            },
            [](const primec::Expr &) { return true; },
            []() { return 0; },
            emitInstruction,
            getInstructionCount,
            patchInstructionImm,
            []() {},
            source,
            argvChecked,
            error) == primec::ir_lowerer::StringCallEmitResult::Handled);
  CHECK(source == primec::ir_lowerer::StringCallSource::RuntimeIndex);
  CHECK(runtimeExprEmitted);
}

TEST_CASE("ir lowerer string call helpers report call-expression diagnostics") {
  primec::Expr badCall;
  badCall.kind = primec::Expr::Kind::Call;
  badCall.name = "bad";

  primec::ir_lowerer::StringCallSource source = primec::ir_lowerer::StringCallSource::None;
  bool argvChecked = true;
  std::string error;
  CHECK(primec::ir_lowerer::emitCallStringCallValue(
            badCall,
            [](const primec::Expr &, std::string &) { return false; },
            [](const primec::Expr &) { return false; },
            [](const primec::Expr &, const std::string &, primec::ir_lowerer::StringIndexOps &, std::string &) {
              return false;
            },
            [](const primec::Expr &) { return true; },
            [](const primec::Expr &) { return false; },
            []() { return 0; },
            [](primec::IrOpcode, uint64_t) {},
            []() { return size_t{0}; },
            [](size_t, int32_t) {},
            []() {},
            source,
            argvChecked,
            error) == primec::ir_lowerer::StringCallEmitResult::Error);
  CHECK(error == "native backend requires string arguments to use string literals, bindings, or entry args");
}

TEST_CASE("ir opcode allowlist matches vm/native support matrix") {
  const std::vector<primec::IrOpcode> expected = {
      primec::IrOpcode::PushI32,
      primec::IrOpcode::PushI64,
      primec::IrOpcode::LoadLocal,
      primec::IrOpcode::StoreLocal,
      primec::IrOpcode::AddressOfLocal,
      primec::IrOpcode::LoadIndirect,
      primec::IrOpcode::StoreIndirect,
      primec::IrOpcode::Dup,
      primec::IrOpcode::Pop,
      primec::IrOpcode::AddI32,
      primec::IrOpcode::SubI32,
      primec::IrOpcode::MulI32,
      primec::IrOpcode::DivI32,
      primec::IrOpcode::NegI32,
      primec::IrOpcode::AddI64,
      primec::IrOpcode::SubI64,
      primec::IrOpcode::MulI64,
      primec::IrOpcode::DivI64,
      primec::IrOpcode::DivU64,
      primec::IrOpcode::NegI64,
      primec::IrOpcode::CmpEqI32,
      primec::IrOpcode::CmpNeI32,
      primec::IrOpcode::CmpLtI32,
      primec::IrOpcode::CmpLeI32,
      primec::IrOpcode::CmpGtI32,
      primec::IrOpcode::CmpGeI32,
      primec::IrOpcode::CmpEqI64,
      primec::IrOpcode::CmpNeI64,
      primec::IrOpcode::CmpLtI64,
      primec::IrOpcode::CmpLeI64,
      primec::IrOpcode::CmpGtI64,
      primec::IrOpcode::CmpGeI64,
      primec::IrOpcode::CmpLtU64,
      primec::IrOpcode::CmpLeU64,
      primec::IrOpcode::CmpGtU64,
      primec::IrOpcode::CmpGeU64,
      primec::IrOpcode::JumpIfZero,
      primec::IrOpcode::Jump,
      primec::IrOpcode::ReturnVoid,
      primec::IrOpcode::ReturnI32,
      primec::IrOpcode::ReturnI64,
      primec::IrOpcode::PrintI32,
      primec::IrOpcode::PrintI64,
      primec::IrOpcode::PrintU64,
      primec::IrOpcode::PrintString,
      primec::IrOpcode::PushArgc,
      primec::IrOpcode::PrintArgv,
      primec::IrOpcode::PrintArgvUnsafe,
      primec::IrOpcode::LoadStringByte,
      primec::IrOpcode::FileOpenRead,
      primec::IrOpcode::FileOpenWrite,
      primec::IrOpcode::FileOpenAppend,
      primec::IrOpcode::FileClose,
      primec::IrOpcode::FileFlush,
      primec::IrOpcode::FileWriteI32,
      primec::IrOpcode::FileWriteI64,
      primec::IrOpcode::FileWriteU64,
      primec::IrOpcode::FileWriteString,
      primec::IrOpcode::FileWriteByte,
      primec::IrOpcode::FileWriteNewline,
      primec::IrOpcode::PushF32,
      primec::IrOpcode::PushF64,
      primec::IrOpcode::AddF32,
      primec::IrOpcode::SubF32,
      primec::IrOpcode::MulF32,
      primec::IrOpcode::DivF32,
      primec::IrOpcode::NegF32,
      primec::IrOpcode::AddF64,
      primec::IrOpcode::SubF64,
      primec::IrOpcode::MulF64,
      primec::IrOpcode::DivF64,
      primec::IrOpcode::NegF64,
      primec::IrOpcode::CmpEqF32,
      primec::IrOpcode::CmpNeF32,
      primec::IrOpcode::CmpLtF32,
      primec::IrOpcode::CmpLeF32,
      primec::IrOpcode::CmpGtF32,
      primec::IrOpcode::CmpGeF32,
      primec::IrOpcode::CmpEqF64,
      primec::IrOpcode::CmpNeF64,
      primec::IrOpcode::CmpLtF64,
      primec::IrOpcode::CmpLeF64,
      primec::IrOpcode::CmpGtF64,
      primec::IrOpcode::CmpGeF64,
      primec::IrOpcode::ConvertI32ToF32,
      primec::IrOpcode::ConvertI32ToF64,
      primec::IrOpcode::ConvertI64ToF32,
      primec::IrOpcode::ConvertI64ToF64,
      primec::IrOpcode::ConvertU64ToF32,
      primec::IrOpcode::ConvertU64ToF64,
      primec::IrOpcode::ConvertF32ToI32,
      primec::IrOpcode::ConvertF32ToI64,
      primec::IrOpcode::ConvertF32ToU64,
      primec::IrOpcode::ConvertF64ToI32,
      primec::IrOpcode::ConvertF64ToI64,
      primec::IrOpcode::ConvertF64ToU64,
      primec::IrOpcode::ConvertF32ToF64,
      primec::IrOpcode::ConvertF64ToF32,
      primec::IrOpcode::ReturnF32,
      primec::IrOpcode::ReturnF64,
      primec::IrOpcode::PrintStringDynamic,
      primec::IrOpcode::Call,
      primec::IrOpcode::CallVoid,
  };

  CHECK(expected.size() == static_cast<size_t>(static_cast<uint8_t>(primec::IrOpcode::CallVoid)));
  for (size_t i = 0; i < expected.size(); ++i) {
    const auto expectedValue = static_cast<uint8_t>(i + 1);
    CHECK(static_cast<uint8_t>(expected[i]) == expectedValue);
  }
}

TEST_CASE("ir call semantics matrix accepts recursive call opcodes with tail metadata") {
  primec::IrModule module;
  module.entryIndex = 0;

  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.metadata.instrumentationFlags = primec::InstrumentationTailExecution;
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 4});
  mainFn.instructions.push_back({primec::IrOpcode::Call, 1});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});

  primec::IrFunction factFn;
  factFn.name = "/fact";
  factFn.instructions.push_back({primec::IrOpcode::Dup, 0});
  factFn.instructions.push_back({primec::IrOpcode::PushI32, 0});
  factFn.instructions.push_back({primec::IrOpcode::CmpEqI32, 0});
  factFn.instructions.push_back({primec::IrOpcode::JumpIfZero, 7});
  factFn.instructions.push_back({primec::IrOpcode::Pop, 0});
  factFn.instructions.push_back({primec::IrOpcode::PushI32, 1});
  factFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  factFn.instructions.push_back({primec::IrOpcode::Dup, 0});
  factFn.instructions.push_back({primec::IrOpcode::PushI32, 1});
  factFn.instructions.push_back({primec::IrOpcode::SubI32, 0});
  factFn.instructions.push_back({primec::IrOpcode::Call, 1});
  factFn.instructions.push_back({primec::IrOpcode::MulI32, 0});
  factFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});

  module.functions.push_back(std::move(mainFn));
  module.functions.push_back(std::move(factFn));

  std::string error;
  CHECK(primec::validateIrModule(module, primec::IrValidationTarget::Vm, error));
  CHECK(error.empty());
  CHECK(primec::validateIrModule(module, primec::IrValidationTarget::Native, error));
  CHECK(error.empty());
}

TEST_CASE("ir call semantics matrix rejects non-direct call targets for vm and native") {
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::Call, 9});
  fn.instructions.push_back({primec::IrOpcode::ReturnVoid, 0});
  module.functions.push_back(fn);

  std::string error;
  CHECK_FALSE(primec::validateIrModule(module, primec::IrValidationTarget::Vm, error));
  CHECK(error.find("invalid call target") != std::string::npos);

  CHECK_FALSE(primec::validateIrModule(module, primec::IrValidationTarget::Native, error));
  CHECK(error.find("invalid call target") != std::string::npos);
}

TEST_CASE("ir validator rejects invalid jump targets") {
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::Jump, 3});
  fn.instructions.push_back({primec::IrOpcode::ReturnVoid, 0});
  module.functions.push_back(fn);

  std::string error;
  CHECK_FALSE(primec::validateIrModule(module, primec::IrValidationTarget::Any, error));
  CHECK(error.find("invalid jump target") != std::string::npos);
}

TEST_CASE("ir validator rejects invalid call targets") {
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::Call, 1});
  fn.instructions.push_back({primec::IrOpcode::ReturnVoid, 0});
  module.functions.push_back(fn);

  std::string error;
  CHECK_FALSE(primec::validateIrModule(module, primec::IrValidationTarget::Any, error));
  CHECK(error.find("invalid call target") != std::string::npos);
}

TEST_CASE("ir validator rejects invalid print flags") {
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::PushI32, 1});
  fn.instructions.push_back({primec::IrOpcode::PrintI32, 4});
  fn.instructions.push_back({primec::IrOpcode::ReturnVoid, 0});
  module.functions.push_back(fn);

  std::string error;
  CHECK_FALSE(primec::validateIrModule(module, primec::IrValidationTarget::Any, error));
  CHECK(error.find("invalid print flags") != std::string::npos);
}

TEST_CASE("ir validator rejects invalid string indices") {
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::FileOpenRead, 0});
  fn.instructions.push_back({primec::IrOpcode::ReturnVoid, 0});
  module.functions.push_back(fn);

  std::string error;
  CHECK_FALSE(primec::validateIrModule(module, primec::IrValidationTarget::Any, error));
  CHECK(error.find("invalid string index") != std::string::npos);
}

TEST_CASE("ir validator rejects local indices beyond 32-bit") {
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::LoadLocal,
                             static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()) + 1u});
  fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(fn);

  std::string error;
  CHECK_FALSE(primec::validateIrModule(module, primec::IrValidationTarget::Any, error));
  CHECK(error.find("local index exceeds 32-bit limit") != std::string::npos);
}

TEST_CASE("ir validator rejects unknown opcodes") {
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({static_cast<primec::IrOpcode>(0), 0});
  module.functions.push_back(fn);

  std::string error;
  CHECK_FALSE(primec::validateIrModule(module, primec::IrValidationTarget::Any, error));
  CHECK(error.find("unsupported opcode") != std::string::npos);
}

TEST_CASE("ir validator rejects unknown metadata bits") {
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.metadata.effectMask = (1ull << 63);
  fn.instructions.push_back({primec::IrOpcode::ReturnVoid, 0});
  module.functions.push_back(fn);

  std::string error;
  CHECK_FALSE(primec::validateIrModule(module, primec::IrValidationTarget::Any, error));
  CHECK(error.find("unsupported effect mask bits") != std::string::npos);
}

TEST_SUITE_END();
