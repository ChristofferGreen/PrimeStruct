#include "primec/ir/IrCfg.h"
#include "primec/ir/IrOpcodeTable.h"
#include "primec/runtime/Vm.h"
#include "primec/runtime/VmHost.h"
#include "primec/testing/TestScratch.h"

#include "third_party/doctest.h"

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#if defined(__unix__) || defined(__APPLE__)
#include <fcntl.h>
#include <unistd.h>
#endif

TEST_SUITE_BEGIN("primestruct.ir.cfg.stack_effects");

// computeIrStackEffect is the one table every IR analysis (virtual-register
// lowering, native stack-depth checking, future optimizer passes) trusts, so
// each row is checked against what the VM really does: run a one-instruction
// micro-module in a debug session and compare the operand stack before and
// after. The pop and push counts are inferred from the stack contents, not
// from sizes alone: the number of untouched bottom slots tells how many
// operands the instruction consumed.

namespace {

using primec::IrInstruction;
using primec::IrOpcode;

#if defined(__unix__) || defined(__APPLE__)
// Print and file opcodes write to fd 1/2; keep the test log clean.
class SilenceStdio {
public:
  SilenceStdio() {
    std::fflush(stdout);
    std::fflush(stderr);
    savedOut_ = ::dup(1);
    savedErr_ = ::dup(2);
    const int devNull = ::open("/dev/null", O_WRONLY);
    if (devNull >= 0) {
      ::dup2(devNull, 1);
      ::dup2(devNull, 2);
      ::close(devNull);
    }
  }
  ~SilenceStdio() {
    std::fflush(stdout);
    std::fflush(stderr);
    if (savedOut_ >= 0) {
      ::dup2(savedOut_, 1);
      ::close(savedOut_);
    }
    if (savedErr_ >= 0) {
      ::dup2(savedErr_, 2);
      ::close(savedErr_);
    }
  }
  SilenceStdio(const SilenceStdio &) = delete;
  SilenceStdio &operator=(const SilenceStdio &) = delete;

private:
  int savedOut_ = -1;
  int savedErr_ = -1;
};
#else
class SilenceStdio {};
#endif

struct EffectCase {
  std::string name;
  IrOpcode op;
  uint64_t imm = 0;
  // Instructions that leave the operands the opcode consumes on the stack.
  std::vector<IrInstruction> setup;
};

IrInstruction pushI32(int32_t value) {
  return {IrOpcode::PushI32, static_cast<uint64_t>(static_cast<int64_t>(value))};
}
IrInstruction pushF32(uint32_t bits) {
  return {IrOpcode::PushF32, bits};
}
IrInstruction pushF64(uint64_t bits) {
  return {IrOpcode::PushF64, bits};
}

constexpr uint32_t F32TwoPointFive = 0x40200000u;
constexpr uint32_t F32OnePointFive = 0x3FC00000u;
constexpr uint64_t F64TwoPointFive = 0x4004000000000000ull;
constexpr uint64_t F64OnePointFive = 0x3FF8000000000000ull;

// String table indices used by the cases below.
constexpr uint64_t TextString = 0;
constexpr uint64_t ScratchPathString = 1;

std::vector<EffectCase> buildCases() {
  std::vector<EffectCase> cases;
  const auto add = [&](IrOpcode op, uint64_t imm, std::vector<IrInstruction> setup, const char *label = "") {
    const primec::IrOpcodeInfo *info = primec::irOpcodeInfo(op);
    std::string name = info != nullptr ? info->name : "?";
    if (label[0] != '\0') {
      name += std::string(" ") + label;
    }
    cases.push_back({std::move(name), op, imm, std::move(setup)});
  };

  const std::vector<IrInstruction> intOperands = {pushI32(7), pushI32(3)};
  const std::vector<IrInstruction> f32Operands = {pushF32(F32TwoPointFive), pushF32(F32OnePointFive)};
  const std::vector<IrInstruction> f64Operands = {pushF64(F64TwoPointFive), pushF64(F64OnePointFive)};

  // Pushes, locals, stack shuffling.
  add(IrOpcode::PushI32, 5, {});
  add(IrOpcode::PushI64, 5, {});
  add(IrOpcode::PushF32, F32TwoPointFive, {});
  add(IrOpcode::PushF64, F64TwoPointFive, {});
  add(IrOpcode::PushArgc, 0, {});
  add(IrOpcode::LoadLocal, 0, {});
  add(IrOpcode::StoreLocal, 0, {pushI32(5)});
  add(IrOpcode::AddressOfLocal, 0, {});
  add(IrOpcode::Dup, 0, {pushI32(5)});
  add(IrOpcode::Pop, 0, {pushI32(5)});
  // A non-zero address: local 0 holds 0, so address 0 would look unchanged.
  add(IrOpcode::LoadIndirect, 0, {{IrOpcode::AddressOfLocal, 1}});
  add(IrOpcode::StoreIndirect, 0, {{IrOpcode::AddressOfLocal, 0}, pushI32(5)});

  // Integer arithmetic and comparisons.
  for (const IrOpcode op : {IrOpcode::AddI32, IrOpcode::SubI32, IrOpcode::MulI32, IrOpcode::DivI32,
                            IrOpcode::AddI64, IrOpcode::SubI64, IrOpcode::MulI64, IrOpcode::DivI64,
                            IrOpcode::DivU64, IrOpcode::CmpEqI32, IrOpcode::CmpNeI32, IrOpcode::CmpLtI32,
                            IrOpcode::CmpLeI32, IrOpcode::CmpGtI32, IrOpcode::CmpGeI32, IrOpcode::CmpEqI64,
                            IrOpcode::CmpNeI64, IrOpcode::CmpLtI64, IrOpcode::CmpLeI64, IrOpcode::CmpGtI64,
                            IrOpcode::CmpGeI64, IrOpcode::CmpLtU64, IrOpcode::CmpLeU64, IrOpcode::CmpGtU64,
                            IrOpcode::CmpGeU64}) {
    add(op, 0, intOperands);
  }
  add(IrOpcode::NegI32, 0, {pushI32(5)});
  add(IrOpcode::NegI64, 0, {pushI32(5)});

  // Float arithmetic and comparisons.
  for (const IrOpcode op : {IrOpcode::AddF32, IrOpcode::SubF32, IrOpcode::MulF32, IrOpcode::DivF32,
                            IrOpcode::CmpEqF32, IrOpcode::CmpNeF32, IrOpcode::CmpLtF32, IrOpcode::CmpLeF32,
                            IrOpcode::CmpGtF32, IrOpcode::CmpGeF32}) {
    add(op, 0, f32Operands);
  }
  for (const IrOpcode op : {IrOpcode::AddF64, IrOpcode::SubF64, IrOpcode::MulF64, IrOpcode::DivF64,
                            IrOpcode::CmpEqF64, IrOpcode::CmpNeF64, IrOpcode::CmpLtF64, IrOpcode::CmpLeF64,
                            IrOpcode::CmpGtF64, IrOpcode::CmpGeF64}) {
    add(op, 0, f64Operands);
  }
  add(IrOpcode::NegF32, 0, {pushF32(F32TwoPointFive)});
  add(IrOpcode::NegF64, 0, {pushF64(F64TwoPointFive)});

  // Conversions.
  for (const IrOpcode op : {IrOpcode::ConvertI32ToF32, IrOpcode::ConvertI32ToF64, IrOpcode::ConvertI64ToF32,
                            IrOpcode::ConvertI64ToF64, IrOpcode::ConvertU64ToF32, IrOpcode::ConvertU64ToF64}) {
    add(op, 0, {pushI32(5)});
  }
  for (const IrOpcode op : {IrOpcode::ConvertF32ToI32, IrOpcode::ConvertF32ToI64, IrOpcode::ConvertF32ToU64,
                            IrOpcode::ConvertF32ToF64}) {
    add(op, 0, {pushF32(F32TwoPointFive)});
  }
  for (const IrOpcode op : {IrOpcode::ConvertF64ToI32, IrOpcode::ConvertF64ToI64, IrOpcode::ConvertF64ToU64,
                            IrOpcode::ConvertF64ToF32}) {
    add(op, 0, {pushF64(F64TwoPointFive)});
  }

  // Control flow. Jumps target the instruction right after themselves so both
  // outcomes continue to the same place; Call targets are set up in makeModule.
  add(IrOpcode::Jump, 0, {});
  add(IrOpcode::JumpIfZero, 0, {pushI32(1)});
  add(IrOpcode::ReturnVoid, 0, {});
  add(IrOpcode::ReturnI32, 0, {pushI32(5)});
  add(IrOpcode::ReturnI64, 0, {pushI32(5)});
  add(IrOpcode::ReturnF32, 0, {pushF32(F32TwoPointFive)});
  add(IrOpcode::ReturnF64, 0, {pushF64(F64TwoPointFive)});
  add(IrOpcode::Call, 1, {pushI32(7), pushI32(3)}, "(two parameters)");
  add(IrOpcode::CallVoid, 2, {pushI32(7)}, "(one parameter)");
  add(IrOpcode::CallHost, 0, {pushI32(4), pushI32(5)}, "(i32,i32 -> i32)");
  add(IrOpcode::CallHost, 1, {pushI32(4)}, "(i32 -> void)");

  // Heap.
  add(IrOpcode::HeapAlloc, 0, {pushI32(2)});
  add(IrOpcode::HeapFree, 0, {pushI32(2), {IrOpcode::HeapAlloc, 0}});
  add(IrOpcode::HeapRealloc, 0, {pushI32(2), {IrOpcode::HeapAlloc, 0}, pushI32(3)});

  // Printing. imm 0 means stdout without a newline.
  add(IrOpcode::PrintI32, 0, {pushI32(5)});
  add(IrOpcode::PrintI64, 0, {pushI32(5)});
  add(IrOpcode::PrintU64, 0, {pushI32(5)});
  add(IrOpcode::PrintString, primec::encodePrintStringImm(TextString, 0), {});
  add(IrOpcode::PrintStringDynamic, 0, {pushI32(static_cast<int32_t>(TextString))});
  add(IrOpcode::PrintArgv, 0, {pushI32(0)});
  add(IrOpcode::PrintArgvUnsafe, 0, {pushI32(0)});

  // Strings.
  add(IrOpcode::LoadStringByte, TextString, {pushI32(1)});
  add(IrOpcode::LoadStringLength, 0, {pushI32(static_cast<int32_t>(TextString))});
  add(IrOpcode::LoadStringByteDynamic, 0, {pushI32(static_cast<int32_t>(TextString)), pushI32(1)});

  // Files. The scratch file exists before any case runs. FileReadByte's
  // immediate names a local, so the setup touches local 0 first.
  const IrInstruction openWrite = {IrOpcode::FileOpenWrite, ScratchPathString};
  const IrInstruction openRead = {IrOpcode::FileOpenRead, ScratchPathString};
  const std::vector<IrInstruction> touchLocal = {{IrOpcode::AddressOfLocal, 0}, {IrOpcode::Pop, 0}};
  add(IrOpcode::FileOpenRead, ScratchPathString, {});
  add(IrOpcode::FileOpenWrite, ScratchPathString, {});
  add(IrOpcode::FileOpenAppend, ScratchPathString, {});
  const auto dynamicPath = std::vector<IrInstruction>{pushI32(static_cast<int32_t>(ScratchPathString))};
  add(IrOpcode::FileOpenReadDynamic, 0, dynamicPath);
  add(IrOpcode::FileOpenWriteDynamic, 0, dynamicPath);
  add(IrOpcode::FileOpenAppendDynamic, 0, dynamicPath);
  {
    std::vector<IrInstruction> setup = touchLocal;
    setup.push_back(openRead);
    add(IrOpcode::FileReadByte, 0, setup);
  }
  add(IrOpcode::FileClose, 0, {openWrite});
  add(IrOpcode::FileFlush, 0, {openWrite});
  add(IrOpcode::FileWriteString, TextString, {openWrite});
  add(IrOpcode::FileWriteNewline, 0, {openWrite});
  add(IrOpcode::FileWriteI32, 0, {openWrite, pushI32(5)});
  add(IrOpcode::FileWriteI64, 0, {openWrite, pushI32(5)});
  add(IrOpcode::FileWriteU64, 0, {openWrite, pushI32(5)});
  add(IrOpcode::FileWriteStringDynamic, 0, {openWrite, pushI32(static_cast<int32_t>(TextString))});
  add(IrOpcode::FileWriteByte, 0, {openWrite, pushI32(65)});

  return cases;
}

primec::IrModule makeModule(const EffectCase &testCase, const std::string &scratchPath) {
  primec::IrModule module;
  module.entryIndex = 0;
  module.stringTable = {"abc", scratchPath};

  primec::IrFunction mainFunction;
  mainFunction.name = "/main";
  // Distinct bottom-of-stack values so untouched slots are recognizable.
  for (uint64_t i = 0; i < 3; ++i) {
    mainFunction.instructions.push_back({IrOpcode::PushI64, 0x5E570000ull + i});
  }
  for (const IrInstruction &instruction : testCase.setup) {
    mainFunction.instructions.push_back(instruction);
  }
  IrInstruction underTest = {testCase.op, testCase.imm};
  if (testCase.op == IrOpcode::Jump || testCase.op == IrOpcode::JumpIfZero) {
    underTest.imm = mainFunction.instructions.size() + 1;
  }
  mainFunction.instructions.push_back(underTest);
  mainFunction.instructions.push_back({IrOpcode::ReturnVoid, 0});
  module.functions.push_back(std::move(mainFunction));

  primec::IrFunction twoParameters;
  twoParameters.name = "/two";
  twoParameters.parameterCount = 2;
  twoParameters.instructions = {{IrOpcode::StoreLocal, 1},
                                {IrOpcode::StoreLocal, 0},
                                pushI32(9),
                                {IrOpcode::ReturnI32, 0}};
  module.functions.push_back(std::move(twoParameters));

  primec::IrFunction oneParameterVoid;
  oneParameterVoid.name = "/one_void";
  oneParameterVoid.parameterCount = 1;
  oneParameterVoid.instructions = {{IrOpcode::StoreLocal, 0}, {IrOpcode::ReturnVoid, 0}};
  module.functions.push_back(std::move(oneParameterVoid));

  primec::IrHostImport add;
  add.name = "test.add";
  add.parameters = {primec::IrHostValueKind::I32, primec::IrHostValueKind::I32};
  add.returnKind = primec::IrHostValueKind::I32;
  module.hostImports.push_back(add);
  primec::IrHostImport sink;
  sink.name = "test.sink";
  sink.parameters = {primec::IrHostValueKind::I32};
  sink.returnKind = primec::IrHostValueKind::Void;
  module.hostImports.push_back(sink);
  return module;
}

primec::VmHostFunctions makeHostFunctions() {
  primec::VmHostFunctions functions;
  primec::VmHostBinding add;
  add.parameters = {primec::IrHostValueKind::I32, primec::IrHostValueKind::I32};
  add.returnKind = primec::IrHostValueKind::I32;
  add.invoke = [](const uint64_t *args, uint64_t &result, std::string &) {
    result = args[0] + args[1];
    return true;
  };
  functions.bind("test.add", add);
  primec::VmHostBinding sink;
  sink.parameters = {primec::IrHostValueKind::I32};
  sink.returnKind = primec::IrHostValueKind::Void;
  sink.invoke = [](const uint64_t *, uint64_t &result, std::string &) {
    result = 0;
    return true;
  };
  functions.bind("test.sink", sink);
  return functions;
}

struct MeasuredEffect {
  size_t pops = 0;
  size_t pushes = 0;
};

// Number of leading slots two operand stacks share.
size_t commonPrefix(const std::vector<uint64_t> &a, const std::vector<uint64_t> &b) {
  size_t i = 0;
  while (i < a.size() && i < b.size() && a[i] == b[i]) {
    ++i;
  }
  return i;
}

bool measure(const EffectCase &testCase,
             const std::string &scratchPath,
             MeasuredEffect &out,
             std::string &failure) {
  const primec::IrModule module = makeModule(testCase, scratchPath);
  const primec::VmHostFunctions hosts = makeHostFunctions();
  const std::vector<std::string_view> args = {"program", "extra"};

  primec::VmDebugSession session;
  std::string error;
  if (!session.start(module, error, args, hosts)) {
    failure = "start failed: " + error;
    return false;
  }
  primec::VmDebugStopReason reason = primec::VmDebugStopReason::Step;
  const size_t setupSteps = 3 + testCase.setup.size();
  for (size_t i = 0; i < setupSteps; ++i) {
    if (!session.step(reason, error)) {
      failure = "setup step " + std::to_string(i) + " failed: " + error;
      return false;
    }
  }
  const std::vector<uint64_t> before = session.snapshotPayload().operandStack;
  if (!session.step(reason, error)) {
    failure = "instruction under test failed: " + error;
    return false;
  }
  // A call runs its callee before the effect on the caller's stack is visible.
  size_t guard = 0;
  while (session.snapshotPayload().callStack.size() > 1 && guard++ < 64) {
    if (!session.step(reason, error)) {
      failure = "callee step failed: " + error;
      return false;
    }
  }
  const std::vector<uint64_t> after = session.snapshotPayload().operandStack;
  const size_t shared = commonPrefix(before, after);
  out.pops = before.size() - shared;
  out.pushes = after.size() - shared;
  return true;
}

} // namespace

TEST_CASE("stack effect table matches the VM for every opcode") {
  const std::filesystem::path scratch = primec::testing::testScratchPath("ir_cfg/stack_effect_file.txt");
  std::filesystem::create_directories(scratch.parent_path());
  {
    std::ofstream file(scratch);
    file << "abc";
  }

  struct Outcome {
    EffectCase testCase;
    bool ok = false;
    std::string failure;
    MeasuredEffect measured;
  };
  // Run every micro-module with stdout/stderr silenced, but report only after
  // the descriptors are restored: assertions made while silenced would print
  // into /dev/null.
  std::vector<Outcome> outcomes;
  {
    SilenceStdio silence;
    for (const EffectCase &testCase : buildCases()) {
      Outcome outcome;
      outcome.testCase = testCase;
      outcome.ok = measure(testCase, scratch.string(), outcome.measured, outcome.failure);
      outcomes.push_back(std::move(outcome));
    }
  }

  for (const Outcome &outcome : outcomes) {
    CAPTURE(outcome.testCase.name);
    CHECK_MESSAGE(outcome.ok, outcome.failure);
    if (!outcome.ok) {
      continue;
    }
    const primec::IrModule module = makeModule(outcome.testCase, scratch.string());
    primec::IrStackEffect effect;
    REQUIRE(primec::computeIrStackEffect({outcome.testCase.op, outcome.testCase.imm}, module, effect));
    CHECK_MESSAGE(effect.pops == outcome.measured.pops,
                  "table pops ", effect.pops, " but the VM popped ", outcome.measured.pops);
    CHECK_MESSAGE(effect.pushes == outcome.measured.pushes,
                  "table pushes ", effect.pushes, " but the VM pushed ", outcome.measured.pushes);
  }
}

TEST_CASE("every opcode has a stack effect case and a table entry") {
  std::set<uint8_t> covered;
  for (const EffectCase &testCase : buildCases()) {
    covered.insert(static_cast<uint8_t>(testCase.op));
  }
  const primec::IrModule module;
  for (const primec::IrOpcodeInfo &info : primec::IrOpcodeTable) {
    CAPTURE(info.name);
    CHECK_MESSAGE(covered.count(static_cast<uint8_t>(info.op)) == 1,
                  "add a VM cross-check case for ", info.name);
    primec::IrStackEffect effect;
    CHECK_MESSAGE(primec::computeIrStackEffect({info.op, 0}, module, effect),
                  "computeIrStackEffect has no entry for ", info.name);
  }
}

TEST_CASE("dup reads its operand without consuming it") {
  const primec::IrModule module;
  primec::IrStackEffect effect;
  REQUIRE(primec::computeIrStackEffect({IrOpcode::Dup, 0}, module, effect));
  CHECK(effect.readsWithoutPop == 1);
  CHECK(effect.pops == 0);
  CHECK(effect.pushes == 1);
}
