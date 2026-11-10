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
  // A definition inlined at every call site gets no standalone body: no Call could reach it.
  REQUIRE(module.functions.size() == 1);
  CHECK(module.functions[0].name == "/main");
  CHECK(module.functions[0].instructions.size() > 2);
  bool sawAdd = false;
  for (const auto &inst : module.functions[0].instructions) {
    if (inst.op == primec::IrOpcode::AddI32) {
      sawAdd = true;
    }
    CHECK(inst.op != primec::IrOpcode::Call);
  }
  CHECK(sawAdd);

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
  REQUIRE(module.functions.size() == 1);
  CHECK(module.functions[0].name == "/main");
  CHECK(module.functions[0].instructions.size() > 2);
  int addCount = 0;
  for (const auto &inst : module.functions[0].instructions) {
    if (inst.op == primec::IrOpcode::AddI32) {
      addCount++;
    }
  }
  CHECK(addCount >= 2);

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
  REQUIRE(module.functions.size() == 1);
  CHECK(module.functions[0].name == "/main");

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
  // Neither the struct nor the inlined helper gets a function.
  REQUIRE(module.functions.size() == 1);
  CHECK(module.functions[0].name == "/main");
  CHECK(module.functions[0].instructions.size() > 2);
  bool sawPush = false;
  for (const auto &inst : module.functions[0].instructions) {
    if (inst.op == primec::IrOpcode::PushF64) {
      sawPush = true;
    }
  }
  CHECK(sawPush);
}

TEST_CASE("ir keeps the body of a called definition and none for inlined ones") {
  // `square` stays inline at its one call site, in the body of `sum_squares`; `sum_squares` is a real call. Only the
  // called definition gets a function, and each Call names it.
  const std::string source = R"(
[return<int>]
square([i32] x) {
  return(multiply(x, x))
}

[return<int>]
sum_squares([i32] n) {
  [i32 mut] total{0i32}
  [i32 mut] i{1i32}
  while(less_equal(i, n)) {
    assign(total, plus(total, square(i)))
    assign(i, plus(i, 1i32))
  }
  return(total)
}

[return<int>]
main() {
  return(plus(sum_squares(3i32), sum_squares(4i32)))
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program,
                        &semanticProgram,
                        "/main",
                        {},
                        {},
                        module,
                        error,
                        nullptr,
                        primec::IrValidationTarget::Vm));
  CHECK(error.empty());
  REQUIRE(module.functions.size() == 2);
  CHECK(module.functions[0].name == "/main");
  CHECK(module.functions[1].name == "/sum_squares");
  CHECK(module.functions[1].parameterCount == 1u);
  int calls = 0;
  for (const auto &function : module.functions) {
    for (const auto &inst : function.instructions) {
      if (inst.op == primec::IrOpcode::Call || inst.op == primec::IrOpcode::CallVoid) {
        CHECK(inst.imm == 1u);
        ++calls;
      }
    }
  }
  CHECK(calls == 2);

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 44); // 14 + 30
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

TEST_CASE("native backend keeps the writeback of a recursive mut out-parameter") {
  // Regression test: [T mut] is PrimeStruct's out-parameter mechanism (the
  // inline path passes the caller's local by address, so mutations are
  // visible to the caller after the call returns - see e.g.
  // stdlib/std/image/image.prime's ppmNextByte). computeRealCallEligibleDefinitionPaths
  // originally stripped the mut token when extracting a parameter's type
  // name, so this recursive definition was treated as an ordinary eligible
  // scalar and got a real Call that copies by value into a separate locals
  // array, silently discarding the writeback. Now excluded, keeping the
  // same fallback rejection as before real-call support existed. Where the
  // native backend takes the address ABI (arm64, TODO-5740) the parameter
  // passes as the address of the caller's local, so the call is real and
  // the writeback stays.
  const std::string source = R"(
[return<i32>]
accumulate([i32] n, [i32 mut] total) {
  assign(total, plus(total, n))
  if(less_than(n, 1i32)) {
    return(total)
  }
  return(accumulate(minus(n, 1i32), total))
}

[return<int>]
main() {
  [i32 mut] result{0i32}
  return(accumulate(3i32, result))
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
#if defined(__APPLE__) && (defined(__aarch64__) || defined(__arm64__))
  REQUIRE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(result == 6); // 3 + 2 + 1 + 0, accumulated in the caller's `result`
#else
  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error));
  CHECK(error.find("recursive") != std::string::npos);
#endif
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

TEST_CASE("native backend emits one real call for a non-recursive definition called from multiple sites") {
  // TODO-4747 Part B: real Call/CallVoid emission is no longer limited to
  // recursive definitions - a definition called from 2+ distinct sites now
  // also gets one shared IrFunction instead of an inlined copy at each call
  // site (the actual code-bloat problem this epic set out to fix - see
  // docs/todo.md's TODO-4747 entry). A single-call-site definition is left
  // inlined exactly as before (see "ir lowers definition call by inlining"
  // above, which still checks for an inlined AddI32 in main's own body).
  const std::string source = R"(
[return<i32>]
square([i32] x) {
  return(multiply(x, x))
}

[return<int>]
main() {
  return(plus(plus(square(2i32), square(3i32)), square(4i32)))
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
  CHECK(module.functions[1].name == "/square");

  int callCount = 0;
  for (const auto &inst : module.functions[0].instructions) {
    if (inst.op == primec::IrOpcode::Call) {
      CHECK(inst.imm == 1);
      ++callCount;
    }
  }
  CHECK(callCount == 3);

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 29);
}

TEST_CASE("native backend lowers a multi-call-site definition's own return correctly when the entry is void") {
  // Regression test: the real-call body-lowering loop (TODO-4747 Part B)
  // reused the entry's own `emitStatement` closures to lower each eligible
  // definition's body, but those closures read the entry's returnsVoid
  // through a reference bound once during entry setup rather than a value
  // threaded per body - so a non-void definition's own `return(...)` got
  // validated against a *void* entry's returnsVoid whenever the entry itself
  // returned void, failing with "return value not allowed for void
  // definition" as soon as the definition crossed the 2-call-site real-call
  // threshold. A single-call-site (still-inlined) copy of this same shape
  // compiled fine, and the "multiple sites" test above didn't catch this
  // because its own entry happens to be non-void too.
  const std::string source = R"(
[return<i32>]
helper() {
  return(7i32)
}

[return<void>]
main() {
  [i32] value{helper()}
  [i32] value2{helper()}
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
  REQUIRE(module.functions.size() == 2);
  CHECK(module.functions[1].name == "/helper");
  REQUIRE(!module.functions[1].instructions.empty());
  CHECK(module.functions[1].instructions.back().op == primec::IrOpcode::ReturnI32);

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
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
  REQUIRE(inst.size() == 7);
  CHECK(inst[0].op == primec::IrOpcode::PushI32);
  CHECK(inst[1].op == primec::IrOpcode::PushI32);
  CHECK(inst[2].op == primec::IrOpcode::AddI32);
  CHECK(inst[3].op == primec::IrOpcode::SextI32);
  CHECK(inst[4].op == primec::IrOpcode::Pop);
  CHECK(inst[5].op == primec::IrOpcode::PushI32);
  CHECK(inst[6].op == primec::IrOpcode::ReturnI32);

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

// An inlined call lowers its callee's `return(value)` once. It used to lower the value again after the
// body (an unreachable copy), and inlined into an inlined callee that doubled the code at each level.
TEST_CASE("ir inlines nested struct returns without an unreachable copy of each return") {
  const std::string source = R"(
[struct]
Box() {
  [i32] a{0i32}
  [i32] b{0i32}
}

[return<Box>]
make_box([i32] x) {
  return(Box{x, plus(x, 1i32)})
}

[return<Box>]
make_box2([i32] x) {
  return(make_box(x))
}

[return<Box>]
make_box3([i32] x) {
  return(make_box2(x))
}

[return<Box>]
make_box4([i32] x) {
  return(make_box3(x))
}

[return<int>]
main() {
  [Box] b{make_box4(3i32)}
  return(plus(b.a, b.b))
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program,
                        &semanticProgram,
                        "/main",
                        {},
                        {},
                        module,
                        error,
                        nullptr,
                        primec::IrValidationTarget::Cpp));
  CHECK(error.empty());
  REQUIRE(module.functions.size() == 1);
  // Each level adds one struct copy (about 40 instructions); doubling made it more than 1000 at this depth.
  CHECK(module.functions[0].instructions.size() < 260u);

  primec::Vm vm;
  uint64_t result = 0;
  REQUIRE(vm.execute(module, result, error));
  CHECK(error.empty());
  CHECK(result == 7);
}

// A body that ends in `return` runs its scope's destructors on the return path only; the cleanup after the
// last statement was an unreachable second copy of every destructor.
TEST_CASE("ir emits the scope cleanup of a body ending in return once") {
  const std::string source = R"(
[struct]
Res() {
  [i32 mut] a{0i32}

  [mut effects(io_out)]
  Destroy() {
    print_line("destroy"utf8)
  }
}

[return<int> effects(io_out)]
main() {
  [Res mut] r{Res{}}
  return(4i32)
}
)";
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error));
  CHECK(error.empty());

  primec::IrLowerer lowerer;
  primec::IrModule module;
  REQUIRE(lowerer.lower(program,
                        &semanticProgram,
                        "/main",
                        {},
                        {},
                        module,
                        error,
                        nullptr,
                        primec::IrValidationTarget::Vm));
  CHECK(error.empty());
  REQUIRE(!module.functions.empty());
  size_t prints = 0;
  for (const auto &inst : module.functions[0].instructions) {
    if (inst.op == primec::IrOpcode::PrintString) {
      ++prints;
    }
  }
  CHECK(prints == 1u);
  REQUIRE(!module.functions[0].instructions.empty());
  CHECK(module.functions[0].instructions.back().op == primec::IrOpcode::ReturnI32);
}

TEST_SUITE_END();
