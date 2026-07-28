TEST_SUITE_BEGIN("primestruct.ir.pipeline.serialization");

namespace {

bool reportsMissingSemanticProductFact(const std::string &error) {
  return error.find("missing semantic-product") != std::string::npos;
}

} // namespace

TEST_CASE("ir lowers definition call by inlining") {
  const std::string source = R"(
[return<int>]
addOne([i32] x) {
  return(plus(x, 1i32))
}

[return<int>]
main() {
  return(addOne(2i32))
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());
  REQUIRE(module.functions.size() == 2);
  CHECK(module.functions[0].name == "/main");
  CHECK(module.functions[1].name == "/addOne");
  CHECK(module.functions[0].instructions.size() > 2);
  bool sawAdd = false;
  bool sawCalleeAdd = false;
  bool sawReturn = false;
  for (const auto &inst : module.functions[0].instructions) {
    if (inst.op == primec::IrOpcode::AddI32) {
      sawAdd = true;
    }
  }
  for (const auto &inst : module.functions[1].instructions) {
    if (inst.op == primec::IrOpcode::AddI32) {
      sawCalleeAdd = true;
    }
    if (inst.op == primec::IrOpcode::ReturnI32) {
      sawReturn = true;
    }
  }
  CHECK(sawAdd);
  CHECK(sawCalleeAdd);
  CHECK(sawReturn);

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 3);
}

TEST_CASE("ir lowers definition call with defaults and named args") {
  const std::string source = R"(
[return<int>]
sum3([i32] a, [i32] b{2i32}, [i32] c{3i32}) {
  return(plus(plus(a, b), c))
}

[return<int>]
main() {
  return(sum3(1i32, [c] 10i32))
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());
  REQUIRE(module.functions.size() == 2);
  CHECK(module.functions[0].name == "/main");
  CHECK(module.functions[1].name == "/sum3");
  CHECK(module.functions[0].instructions.size() > 2);
  int addCount = 0;
  bool sawReturn = false;
  for (const auto &inst : module.functions[0].instructions) {
    if (inst.op == primec::IrOpcode::AddI32) {
      addCount++;
    }
  }
  for (const auto &inst : module.functions[1].instructions) {
    if (inst.op == primec::IrOpcode::ReturnI32) {
      sawReturn = true;
    }
  }
  CHECK(addCount >= 2);
  CHECK(sawReturn);

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 13);
}

TEST_CASE("ir lowers void definition call as statement") {
  const std::string source = R"(
touch() {
  return()
}

[return<int>]
main() {
  touch()
  return(7i32)
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());
  REQUIRE(module.functions.size() == 2);
  CHECK(module.functions[0].name == "/main");
  CHECK(module.functions[1].name == "/touch");
  REQUIRE(!module.functions[1].instructions.empty());
  CHECK(module.functions[1].instructions.back().op == primec::IrOpcode::ReturnVoid);

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 7);
}

TEST_CASE("ir emits callable function table entries and skips structs") {
  const std::string source = R"(
[struct]
Pair() {
  [i32] left
  [i32] right
}

[return<f64>]
helper() {
  return(1.5f64)
}

[return<int>]
main() {
  return(convert<int>(helper()))
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());
  REQUIRE(module.functions.size() == 2);
  CHECK(module.functions[0].name == "/main");
  CHECK(module.functions[1].name == "/helper");
  CHECK(module.functions[0].instructions.size() > 2);
  bool sawPush = false;
  bool sawReturn = false;
  for (const auto &inst : module.functions[0].instructions) {
    if (inst.op == primec::IrOpcode::PushF64) {
      sawPush = true;
    }
  }
  for (const auto &inst : module.functions[1].instructions) {
    if (inst.op == primec::IrOpcode::ReturnF64) {
      sawReturn = true;
    }
  }
  CHECK(sawPush);
  CHECK(sawReturn);
}

TEST_CASE("native backend rejects recursive definition calls with a non-scalar parameter") {
  // TODO-4747 Phase 1: self-/mutually-recursive definitions with only
  // scalar parameters and a scalar-or-void return are now emitted as real
  // Call/CallVoid instead of being rejected - see
  // computeRealCallEligibleDefinitionPaths. This still exercises the
  // fallback rejection path for a recursive definition the static
  // eligibility scan does not accept (an args-pack parameter here).
  const std::string source = R"(
[return<int>]
recur([args<i32>] values) {
  return(recur([spread] values))
}

[return<int>]
main() {
  return(recur(1i32, 2i32))
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.find("recursive") != std::string::npos);
}

TEST_CASE("native backend emits a real call for self-recursive scalar definitions") {
  const std::string source = R"(
[return<i32>]
countdown([i32] n) {
  if(less_than(n, 1i32)) {
    return(0i32)
  }
  return(plus(1i32, countdown(minus(n, 1i32))))
}

[return<int>]
main() {
  return(countdown(5i32))
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());
  REQUIRE(module.functions.size() == 2);
  CHECK(module.functions[1].name == "/countdown");
  CHECK(module.functions[1].parameterCount == 1);

  bool sawSelfCall = false;
  for (const auto &inst : module.functions[1].instructions) {
    if (inst.op == primec::IrOpcode::Call) {
      CHECK(inst.imm == 1);
      sawSelfCall = true;
    }
  }
  CHECK(sawSelfCall);

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 5);
}

TEST_CASE("native backend real-call redirect resolves omitted defaults and named arguments") {
  // Regression test: the real-call redirect in IrLowererLowerInlineCalls.h
  // originally evaluated callExpr.args left-to-right, ignoring named
  // arguments and callee-side defaults entirely - a call that omitted a
  // defaulted trailing parameter would push too few values onto the
  // operand stack for the callee's StoreLocal prologue to consume. Fixed
  // to reuse buildInlineCallOrderedArguments, same as the inline path.
  const std::string source = R"(
[return<i32>]
countUp([i32] n, [i32] limit{3i32}) {
  if(equal(n, limit)) {
    return(n)
  }
  return(countUp(plus(n, 1i32), [limit] limit))
}

[return<int>]
main() {
  return(countUp(0i32))
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 3);
}

TEST_CASE("native backend resolves mutually recursive forward call references") {
  const std::string source = R"(
[return<bool>]
isEven([i32] n) {
  if(equal(n, 0i32)) {
    return(true)
  }
  return(isOdd(minus(n, 1i32)))
}

[return<bool>]
isOdd([i32] n) {
  if(equal(n, 0i32)) {
    return(false)
  }
  return(isEven(minus(n, 1i32)))
}

[return<int>]
main() {
  if(isEven(6i32)) {
    return(1i32)
  }
  return(0i32)
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());
  REQUIRE(module.functions.size() == 3);

  // Every Call must have been fixed up to a real, in-range function index -
  // none should still carry the placeholder's (1<<32) high bit.
  for (const auto &fn : module.functions) {
    for (const auto &inst : fn.instructions) {
      if (inst.op == primec::IrOpcode::Call || inst.op == primec::IrOpcode::CallVoid) {
        CHECK(inst.imm < module.functions.size());
      }
    }
  }

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 1);
}

TEST_CASE("ir lowers implicit void return") {
  const std::string source = R"(
[return<void>]
main() {
  [i32] value{4i32}
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());
  const auto &inst = module.functions[0].instructions;
  REQUIRE(inst.size() == 3);
  CHECK(inst[0].op == primec::IrOpcode::PushI32);
  CHECK(inst[1].op == primec::IrOpcode::StoreLocal);
  CHECK(inst[2].op == primec::IrOpcode::ReturnVoid);

  primec::Vm vm;
  uint64_t result = 1;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 0);
}

TEST_CASE("ir lowers implicit void return without transform") {
  const std::string source = R"(
main() {
  [i32] value{4i32}
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());
  const auto &inst = module.functions[0].instructions;
  REQUIRE(inst.size() == 3);
  CHECK(inst[0].op == primec::IrOpcode::PushI32);
  CHECK(inst[1].op == primec::IrOpcode::StoreLocal);
  CHECK(inst[2].op == primec::IrOpcode::ReturnVoid);

  primec::Vm vm;
  uint64_t result = 1;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 0);
}

TEST_CASE("ir lowers explicit void return") {
  const std::string source = R"(
[return<void>]
main() {
  return()
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());
  const auto &inst = module.functions[0].instructions;
  REQUIRE(inst.size() == 1);
  CHECK(inst[0].op == primec::IrOpcode::ReturnVoid);

  primec::Vm vm;
  uint64_t result = 1;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 0);
}

TEST_CASE("ir allows expression statements") {
  const std::string source = R"(
[return<int>]
main() {
  plus(1i32, 2i32)
  return(7i32)
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());
  const auto &inst = module.functions[0].instructions;
  REQUIRE(inst.size() == 6);
  CHECK(inst[0].op == primec::IrOpcode::PushI32);
  CHECK(inst[1].op == primec::IrOpcode::PushI32);
  CHECK(inst[2].op == primec::IrOpcode::AddI32);
  CHECK(inst[3].op == primec::IrOpcode::Pop);
  CHECK(inst[4].op == primec::IrOpcode::PushI32);
  CHECK(inst[5].op == primec::IrOpcode::ReturnI32);

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 7);
}

TEST_CASE("ir lowers comparisons and boolean ops") {
  const std::string source = R"(
[return<bool>]
main() {
  return(and(greater_than(5i32, 2i32), not_equal(3i32, 0i32)))
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());

  bool sawCompare = false;
  for (const auto &inst : module.functions[0].instructions) {
    if (inst.op == primec::IrOpcode::CmpGtI32 || inst.op == primec::IrOpcode::CmpNeI32) {
      sawCompare = true;
      break;
    }
  }
  CHECK(sawCompare);

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 1);
}

TEST_CASE("ir lowers boolean not/or") {
  const std::string source = R"(
[return<bool>]
main() {
  return(or(not(false), false))
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 1);
}

TEST_CASE("ir lowers numeric boolean ops") {
  const std::string source = R"(
[return<bool>]
main() {
  return(and(true, not(false)))
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 1);
}

TEST_CASE("ir lowers short-circuit and") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{1i32}
  [bool mut] witness{false}
  and(equal(value, 0i32), assign(witness, true))
  return(convert<int>(witness))
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());

  bool sawJumpIfZero = false;
  bool sawJump = false;
  for (const auto &inst : module.functions[0].instructions) {
    if (inst.op == primec::IrOpcode::JumpIfZero) {
      sawJumpIfZero = true;
    } else if (inst.op == primec::IrOpcode::Jump) {
      sawJump = true;
    }
  }
  CHECK(sawJumpIfZero);
  CHECK(sawJump);

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 0);
}

TEST_CASE("ir lowers short-circuit or") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{1i32}
  [bool mut] witness{false}
  or(equal(value, 1i32), assign(witness, true))
  return(convert<int>(witness))
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.empty());

  bool sawJumpIfZero = false;
  bool sawJump = false;
  for (const auto &inst : module.functions[0].instructions) {
    if (inst.op == primec::IrOpcode::JumpIfZero) {
      sawJumpIfZero = true;
    } else if (inst.op == primec::IrOpcode::Jump) {
      sawJump = true;
    }
  }
  CHECK(sawJumpIfZero);
  CHECK(sawJump);

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 0);
}

TEST_CASE("semantics rejects conflicting auto returns before lowerer entry summary lookup") {
  auto makeProgram = []() {
    auto makeLiteral = [](uint64_t value) {
      primec::Expr expr;
      expr.kind = primec::Expr::Kind::Literal;
      expr.literalValue = value;
      expr.intWidth = 32;
      return expr;
    };
    auto makeBool = [](bool value) {
      primec::Expr expr;
      expr.kind = primec::Expr::Kind::BoolLiteral;
      expr.boolValue = value;
      return expr;
    };
    auto makeCall = [](const std::string &name, std::vector<primec::Expr> args = {}) {
      primec::Expr expr;
      expr.kind = primec::Expr::Kind::Call;
      expr.name = name;
      expr.args = std::move(args);
      return expr;
    };

    primec::Program program;
    primec::Definition pick;
    pick.name = "/pick";
    pick.fullPath = "/pick";
    primec::Transform pickReturn;
    pickReturn.name = "return";
    pickReturn.templateArgs = {"auto"};
    pick.transforms.push_back(std::move(pickReturn));
    pick.hasReturnStatement = true;
    pick.statements.push_back(makeCall("/return", {makeLiteral(1)}));
    pick.statements.push_back(makeCall("/return", {makeBool(true)}));
    program.definitions.push_back(std::move(pick));

    primec::Definition mainDef;
    mainDef.name = "/main";
    mainDef.fullPath = "/main";
    primec::Transform mainReturn;
    mainReturn.name = "return";
    mainReturn.templateArgs = {"i32"};
    mainDef.transforms.push_back(std::move(mainReturn));
    mainDef.hasReturnStatement = true;
    mainDef.statements.push_back(makeCall("/return", {makeCall("/pick")}));
    program.definitions.push_back(std::move(mainDef));
    return program;
  };

  primec::Program semanticsProgram = makeProgram();
  std::string semanticsError;
  CHECK_FALSE(validateProgram(semanticsProgram, "/main", semanticsError));
  CHECK(semanticsError.find("conflicting return types on /pick") != std::string::npos);

  primec::Program lowerProgram = makeProgram();
  primec::SemanticProgram lowerSemanticProgram;
  primec::IrLowerer lowerer;
  primec::IrModule module;
  std::string lowerError;
  CHECK_FALSE(lowerer.lower(lowerProgram, &lowerSemanticProgram, "/main", {}, {}, module, lowerError));
  CHECK(reportsMissingSemanticProductFact(lowerError));
}

TEST_CASE("semantics rejects unresolved auto return before lowerer entry summary lookup") {
  auto makeProgram = []() {
    auto makeLiteral = [](uint64_t value) {
      primec::Expr expr;
      expr.kind = primec::Expr::Kind::Literal;
      expr.literalValue = value;
      expr.intWidth = 32;
      return expr;
    };
    auto makeBool = [](bool value) {
      primec::Expr expr;
      expr.kind = primec::Expr::Kind::BoolLiteral;
      expr.boolValue = value;
      return expr;
    };
    auto makeEnvelope = [](const std::string &name, primec::Expr valueExpr) {
      primec::Expr expr;
      expr.kind = primec::Expr::Kind::Call;
      expr.name = name;
      expr.hasBodyArguments = true;
      expr.bodyArguments.push_back(std::move(valueExpr));
      return expr;
    };
    auto makeCall = [](const std::string &name, std::vector<primec::Expr> args = {}) {
      primec::Expr expr;
      expr.kind = primec::Expr::Kind::Call;
      expr.name = name;
      expr.args = std::move(args);
      return expr;
    };

    primec::Expr unresolvedIf = makeCall("/if", {makeBool(true),
                                                  makeEnvelope("then", makeLiteral(1)),
                                                  makeEnvelope("else", makeBool(true))});

    primec::Program program;
    primec::Definition pick;
    pick.name = "/pick";
    pick.fullPath = "/pick";
    primec::Transform pickReturn;
    pickReturn.name = "return";
    pickReturn.templateArgs = {"auto"};
    pick.transforms.push_back(std::move(pickReturn));
    pick.hasReturnStatement = true;
    pick.statements.push_back(makeCall("/return", {std::move(unresolvedIf)}));
    program.definitions.push_back(std::move(pick));

    primec::Definition mainDef;
    mainDef.name = "/main";
    mainDef.fullPath = "/main";
    primec::Transform mainReturn;
    mainReturn.name = "return";
    mainReturn.templateArgs = {"i32"};
    mainDef.transforms.push_back(std::move(mainReturn));
    mainDef.hasReturnStatement = true;
    mainDef.statements.push_back(makeCall("/return", {makeCall("/pick")}));
    program.definitions.push_back(std::move(mainDef));
    return program;
  };

  primec::Program semanticsProgram = makeProgram();
  std::string semanticsError;
  CHECK_FALSE(validateProgram(semanticsProgram, "/main", semanticsError));
  CHECK(semanticsError.find("unable to infer return type on /pick") != std::string::npos);

  primec::Program lowerProgram = makeProgram();
  primec::SemanticProgram lowerSemanticProgram;
  primec::IrLowerer lowerer;
  primec::IrModule module;
  std::string lowerError;
  CHECK_FALSE(lowerer.lower(lowerProgram, &lowerSemanticProgram, "/main", {}, {}, module, lowerError));
  CHECK(reportsMissingSemanticProductFact(lowerError));
}

TEST_CASE("semantics rejects unresolved auto return_expr before lowerer entry summary lookup") {
  auto makeProgram = []() {
    auto makeLiteral = [](uint64_t value) {
      primec::Expr expr;
      expr.kind = primec::Expr::Kind::Literal;
      expr.literalValue = value;
      expr.intWidth = 32;
      return expr;
    };
    auto makeBool = [](bool value) {
      primec::Expr expr;
      expr.kind = primec::Expr::Kind::BoolLiteral;
      expr.boolValue = value;
      return expr;
    };
    auto makeEnvelope = [](const std::string &name, primec::Expr valueExpr) {
      primec::Expr expr;
      expr.kind = primec::Expr::Kind::Call;
      expr.name = name;
      expr.hasBodyArguments = true;
      expr.bodyArguments.push_back(std::move(valueExpr));
      return expr;
    };
    auto makeCall = [](const std::string &name, std::vector<primec::Expr> args = {}) {
      primec::Expr expr;
      expr.kind = primec::Expr::Kind::Call;
      expr.name = name;
      expr.args = std::move(args);
      return expr;
    };

    primec::Expr unresolvedIf = makeCall("/if", {makeBool(true),
                                                  makeEnvelope("then", makeLiteral(1)),
                                                  makeEnvelope("else", makeBool(true))});

    primec::Program program;
    primec::Definition pick;
    pick.name = "/pick";
    pick.fullPath = "/pick";
    primec::Transform pickReturn;
    pickReturn.name = "return";
    pickReturn.templateArgs = {"auto"};
    pick.transforms.push_back(std::move(pickReturn));
    pick.returnExpr = std::move(unresolvedIf);
    pick.hasReturnStatement = false;
    program.definitions.push_back(std::move(pick));

    primec::Definition mainDef;
    mainDef.name = "/main";
    mainDef.fullPath = "/main";
    primec::Transform mainReturn;
    mainReturn.name = "return";
    mainReturn.templateArgs = {"i32"};
    mainDef.transforms.push_back(std::move(mainReturn));
    mainDef.hasReturnStatement = true;
    mainDef.statements.push_back(makeCall("/return", {makeCall("/pick")}));
    program.definitions.push_back(std::move(mainDef));
    return program;
  };

  primec::Program semanticsProgram = makeProgram();
  std::string semanticsError;
  CHECK_FALSE(validateProgram(semanticsProgram, "/main", semanticsError));
  CHECK(semanticsError.find("unable to infer return type on /pick") != std::string::npos);

  primec::Program lowerProgram = makeProgram();
  primec::SemanticProgram lowerSemanticProgram;
  primec::IrLowerer lowerer;
  primec::IrModule module;
  std::string lowerError;
  CHECK_FALSE(lowerer.lower(lowerProgram, &lowerSemanticProgram, "/main", {}, {}, module, lowerError));
  CHECK(reportsMissingSemanticProductFact(lowerError));
}

TEST_SUITE_END();
