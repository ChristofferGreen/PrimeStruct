#include "primec/backend/IrToOptCppEmitter.h"
#include "primec/ir/Ir.h"
#include "primec/support/ExternalTooling.h"
#include "primec/support/ProcessRunner.h"
#include "primec/testing/TestScratch.h"

#include "test_ir_vm_run.h"

#include "third_party/doctest.h"

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#if defined(__unix__) || defined(__APPLE__)
#include <sys/wait.h>
#endif

TEST_SUITE_BEGIN("primestruct.ir.optexe_tasks");

// The optimized C++ emitter runs each spawned task on a thread of its own (docs/spec/vm-design.md, Tasks). Nothing
// here depends on how the threads are scheduled: a program either is free of races, so it has one output (the VM's),
// or it ends in exactly one way whichever thread gets there first, or its output is checked for a property every
// schedule has (whole lines). No test measures time.

namespace {

using primec::IrFunction;
using primec::IrInstruction;
using primec::IrModule;
using primec::IrOpcode;

constexpr uint64_t Newline = primec::PrintFlagNewline;

struct ProgramRun {
  bool built = false;
  int exitCode = -1;
  std::string out;
  std::string err;
  std::string buildError;
};

std::string readText(const std::filesystem::path &path) {
  std::ifstream file(path, std::ios::binary);
  std::stringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}

std::filesystem::path scratch(const std::string &name) {
  const std::filesystem::path path = primec::testing::testScratchPath("optexe_tasks/" + name);
  std::filesystem::create_directories(path.parent_path());
  return path;
}

int exitCodeOf(int status) {
#if defined(__unix__) || defined(__APPLE__)
  return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#else
  return status;
#endif
}

ProgramRun runCommand(const std::string &command, const std::string &name) {
  ProgramRun run;
  run.built = true;
  const std::filesystem::path outPath = scratch(name + ".stdout");
  const std::filesystem::path errPath = scratch(name + ".stderr");
  run.exitCode = exitCodeOf(std::system(
      (command + " > '" + outPath.string() + "' 2> '" + errPath.string() + "'").c_str()));
  run.out = readText(outPath);
  run.err = readText(errPath);
  return run;
}

// Emits, compiles (host -O2) and runs a module.
ProgramRun compileAndRun(const IrModule &module,
                         const std::string &name,
                         const std::vector<std::string> &args = {}) {
  ProgramRun run;
  std::string source;
  if (!primec::IrToOptCppEmitter().emitSource(module, source, run.buildError)) {
    return run;
  }
  const std::filesystem::path cppPath = scratch(name + ".cpp");
  const std::filesystem::path binary = scratch(name + ".bin");
  std::ofstream(cppPath) << source;
  if (!primec::compileCppExecutableOptimized(primec::systemProcessRunner(), cppPath, binary, 2)) {
    run.buildError = "host compiler failed on " + cppPath.string();
    return run;
  }
  std::string command = "'" + binary.string() + "'";
  for (const std::string &arg : args) {
    command += " '" + arg + "'";
  }
  return runCommand(command, name);
}

// The compiled program prints, returns and faults exactly as the VM does on the same module.
void expectMatchesVm(const IrModule &module, const std::string &name) {
  const optimizer_test::Outcome expected = optimizer_test::run(module, {"program"});
  const ProgramRun actual = compileAndRun(module, name);
  INFO(name);
  REQUIRE_MESSAGE(actual.built, actual.buildError);
  CHECK(actual.out == expected.output);
  if (expected.ok) {
    CHECK(actual.exitCode ==
          static_cast<int>(static_cast<uint8_t>(static_cast<int32_t>(expected.result))));
    CHECK(actual.err.empty());
  } else {
    CHECK(actual.exitCode == 3);
    CHECK(actual.err == "VM error: " + expected.error + "\n");
  }
}

IrFunction
functionOf(std::string name, uint32_t parameterCount, std::vector<IrInstruction> instructions) {
  IrFunction function;
  function.name = std::move(name);
  function.parameterCount = parameterCount;
  function.instructions = std::move(instructions);
  return function;
}

IrModule moduleOf(std::vector<IrFunction> functions) {
  IrModule module;
  module.entryIndex = 0;
  module.functions = std::move(functions);
  module.entryIndex = 0;
  return module;
}

// Appends `text` to the module's strings and returns the PrintString operand that prints it as a line.
uint64_t lineOf(IrModule &module, const std::string &text) {
  module.stringTable.push_back(text);
  return primec::encodePrintStringImm(module.stringTable.size() - 1, Newline);
}

// sum_to(n): 0 + 1 + ... + (n - 1) as an i64. Locals: 0 n, 1 total, 2 i.
IrFunction sumTo() {
  return functionOf("/sum_to",
                    1,
                    {/* 0*/ {IrOpcode::StoreLocal, 0},
                     /* 1*/ {IrOpcode::PushI64, 0},
                     /* 2*/ {IrOpcode::StoreLocal, 1},
                     /* 3*/ {IrOpcode::PushI64, 0},
                     /* 4*/ {IrOpcode::StoreLocal, 2},
                     /* 5*/ {IrOpcode::LoadLocal, 2},
                     /* 6*/ {IrOpcode::LoadLocal, 0},
                     /* 7*/ {IrOpcode::CmpLtI64, 0},
                     /* 8*/ {IrOpcode::JumpIfZero, 18},
                     /* 9*/ {IrOpcode::LoadLocal, 1},
                     /*10*/ {IrOpcode::LoadLocal, 2},
                     /*11*/ {IrOpcode::AddI64, 0},
                     /*12*/ {IrOpcode::StoreLocal, 1},
                     /*13*/ {IrOpcode::LoadLocal, 2},
                     /*14*/ {IrOpcode::PushI64, 1},
                     /*15*/ {IrOpcode::AddI64, 0},
                     /*16*/ {IrOpcode::StoreLocal, 2},
                     /*17*/ {IrOpcode::Jump, 5},
                     /*18*/ {IrOpcode::LoadLocal, 1},
                     /*19*/ {IrOpcode::ReturnI64, 0}});
}

// spin(): never returns. Every turn allocates and frees a byte region, so the loop has an effect the host compiler
// keeps, and the task is inside the heap when another thread ends the program.
IrFunction spin() {
  return functionOf("/spin",
                    0,
                    {/*0*/ {IrOpcode::PushI64, 24},
                     /*1*/ {IrOpcode::HeapAllocBytes, 0},
                     /*2*/ {IrOpcode::HeapFree, 0},
                     /*3*/ {IrOpcode::Jump, 0},
                     /*4*/ {IrOpcode::PushI32, 0},
                     /*5*/ {IrOpcode::ReturnI32, 0}});
}

// divide(d): 10 / d, which faults for d == 0.
IrFunction divide() {
  return functionOf("/divide",
                    1,
                    {{IrOpcode::StoreLocal, 0},
                     {IrOpcode::PushI32, 10},
                     {IrOpcode::LoadLocal, 0},
                     {IrOpcode::DivI32, 0},
                     {IrOpcode::ReturnI32, 0}});
}

std::vector<std::string> linesOf(const std::string &text) {
  std::vector<std::string> lines;
  size_t start = 0;
  while (start < text.size()) {
    const size_t end = text.find('\n', start);
    if (end == std::string::npos) {
      lines.push_back(text.substr(start) + "<no newline>");
      break;
    }
    lines.push_back(text.substr(start, end - start));
    start = end + 1;
  }
  return lines;
}

bool contains(const std::string &text, const std::string &needle) {
  return text.find(needle) != std::string::npos;
}

} // namespace

TEST_CASE("optexe runs race-free task programs like the vm") {
  // One program, one host compile: results of two tasks alive together, a void task that prints (waited before the
  // spawner prints again), a task that spawns and waits a task, a task that addresses its own locals and recurses
  // 3000 calls deep on its thread's stack, and 200 tasks spawned and waited one after another (slots are reused).
  IrModule module;
  module.entryIndex = 0;
  // main locals: 0 and 1 handles, 2 total, 3 i.
  module.functions.push_back(functionOf("/main",
                                        0,
                                        {/* 0*/ {IrOpcode::PushI64, 1000},
                                         /* 1*/ {IrOpcode::TaskSpawn, 1},
                                         /* 2*/ {IrOpcode::StoreLocal, 0},
                                         /* 3*/ {IrOpcode::PushI64, 2000},
                                         /* 4*/ {IrOpcode::TaskSpawn, 1},
                                         /* 5*/ {IrOpcode::StoreLocal, 1},
                                         /* 6*/ {IrOpcode::LoadLocal, 1},
                                         /* 7*/ {IrOpcode::TaskWait, 0},
                                         /* 8*/ {IrOpcode::PrintI64, Newline},
                                         /* 9*/ {IrOpcode::LoadLocal, 0},
                                         /*10*/ {IrOpcode::TaskWait, 0},
                                         /*11*/ {IrOpcode::PrintI64, Newline},
                                         /*12*/ {IrOpcode::PushI32, 3},
                                         /*13*/ {IrOpcode::TaskSpawn, 2},
                                         /*14*/ {IrOpcode::TaskWait, 0},
                                         /*15*/ {IrOpcode::PrintI64, Newline},
                                         /*16*/ {IrOpcode::PushI64, 100},
                                         /*17*/ {IrOpcode::TaskSpawn, 3},
                                         /*18*/ {IrOpcode::TaskWait, 0},
                                         /*19*/ {IrOpcode::PrintI64, Newline},
                                         /*20*/ {IrOpcode::PushI64, 7},
                                         /*21*/ {IrOpcode::TaskSpawn, 4},
                                         /*22*/ {IrOpcode::TaskWait, 0},
                                         /*23*/ {IrOpcode::PrintI64, Newline},
                                         /*24*/ {IrOpcode::PushI64, 0},
                                         /*25*/ {IrOpcode::StoreLocal, 2},
                                         /*26*/ {IrOpcode::PushI64, 1},
                                         /*27*/ {IrOpcode::StoreLocal, 3},
                                         /*28*/ {IrOpcode::LoadLocal, 3},
                                         /*29*/ {IrOpcode::PushI64, 200},
                                         /*30*/ {IrOpcode::CmpLeI64, 0},
                                         /*31*/ {IrOpcode::JumpIfZero, 43},
                                         /*32*/ {IrOpcode::LoadLocal, 2},
                                         /*33*/ {IrOpcode::LoadLocal, 3},
                                         /*34*/ {IrOpcode::TaskSpawn, 1},
                                         /*35*/ {IrOpcode::TaskWait, 0},
                                         /*36*/ {IrOpcode::AddI64, 0},
                                         /*37*/ {IrOpcode::StoreLocal, 2},
                                         /*38*/ {IrOpcode::LoadLocal, 3},
                                         /*39*/ {IrOpcode::PushI64, 1},
                                         /*40*/ {IrOpcode::AddI64, 0},
                                         /*41*/ {IrOpcode::StoreLocal, 3},
                                         /*42*/ {IrOpcode::Jump, 28},
                                         /*43*/ {IrOpcode::LoadLocal, 2},
                                         /*44*/ {IrOpcode::PrintI64, Newline},
                                         /*45*/ {IrOpcode::PushI32, 42},
                                         /*46*/ {IrOpcode::ReturnI32, 0}}));
  module.functions.push_back(sumTo());
  // report(count): prints 0 .. count - 1, one a line. Locals: 0 count, 1 i.
  module.functions.push_back(functionOf("/report",
                                        1,
                                        {/* 0*/ {IrOpcode::StoreLocal, 0},
                                         /* 1*/ {IrOpcode::PushI32, 0},
                                         /* 2*/ {IrOpcode::StoreLocal, 1},
                                         /* 3*/ {IrOpcode::LoadLocal, 1},
                                         /* 4*/ {IrOpcode::LoadLocal, 0},
                                         /* 5*/ {IrOpcode::CmpLtI32, 0},
                                         /* 6*/ {IrOpcode::JumpIfZero, 14},
                                         /* 7*/ {IrOpcode::LoadLocal, 1},
                                         /* 8*/ {IrOpcode::PrintI32, Newline},
                                         /* 9*/ {IrOpcode::LoadLocal, 1},
                                         /*10*/ {IrOpcode::PushI32, 1},
                                         /*11*/ {IrOpcode::AddI32, 0},
                                         /*12*/ {IrOpcode::StoreLocal, 1},
                                         /*13*/ {IrOpcode::Jump, 3},
                                         /*14*/ {IrOpcode::ReturnVoid, 0}}));
  // outer(x): spawns sum_to(x), waits it and returns the result plus 1.
  module.functions.push_back(functionOf("/outer",
                                        1,
                                        {{IrOpcode::StoreLocal, 0},
                                         {IrOpcode::LoadLocal, 0},
                                         {IrOpcode::TaskSpawn, 1},
                                         {IrOpcode::TaskWait, 0},
                                         {IrOpcode::PushI64, 1},
                                         {IrOpcode::AddI64, 0},
                                         {IrOpcode::ReturnI64, 0}}));
  // addressed(x): stores x + 5 through the address of local 1, reads it back and adds down(3000).
  module.functions.push_back(functionOf("/addressed",
                                        1,
                                        {{IrOpcode::StoreLocal, 0},
                                         {IrOpcode::AddressOfLocal, 1},
                                         {IrOpcode::LoadLocal, 0},
                                         {IrOpcode::PushI64, 5},
                                         {IrOpcode::AddI64, 0},
                                         {IrOpcode::StoreIndirect, 0},
                                         {IrOpcode::Pop, 0},
                                         {IrOpcode::AddressOfLocal, 1},
                                         {IrOpcode::LoadIndirect, 0},
                                         {IrOpcode::PushI64, 3000},
                                         {IrOpcode::Call, 5},
                                         {IrOpcode::AddI64, 0},
                                         {IrOpcode::ReturnI64, 0}}));
  // down(n): n == 0 ? 0 : 1 + down(n - 1).
  module.functions.push_back(functionOf("/down",
                                        1,
                                        {/* 0*/ {IrOpcode::StoreLocal, 0},
                                         /* 1*/ {IrOpcode::LoadLocal, 0},
                                         /* 2*/ {IrOpcode::JumpIfZero, 10},
                                         /* 3*/ {IrOpcode::PushI64, 1},
                                         /* 4*/ {IrOpcode::LoadLocal, 0},
                                         /* 5*/ {IrOpcode::PushI64, 1},
                                         /* 6*/ {IrOpcode::SubI64, 0},
                                         /* 7*/ {IrOpcode::Call, 5},
                                         /* 8*/ {IrOpcode::AddI64, 0},
                                         /* 9*/ {IrOpcode::ReturnI64, 0},
                                         /*10*/ {IrOpcode::PushI64, 0},
                                         /*11*/ {IrOpcode::ReturnI64, 0}}));
  const optimizer_test::Outcome vm = optimizer_test::run(module, {"program"});
  REQUIRE_MESSAGE(vm.ok, vm.error);
  CHECK(vm.output == "1999000\n499500\n0\n1\n2\n0\n4951\n3012\n1333300\n");
  expectMatchesVm(module, "race_free");
}

TEST_CASE("optexe faults on a handle waited twice and on a value that is no handle") {
  const IrModule twice = moduleOf({functionOf("/main",
                                              0,
                                              {{IrOpcode::PushI64, 10},
                                               {IrOpcode::TaskSpawn, 1},
                                               {IrOpcode::Dup, 0},
                                               {IrOpcode::TaskWait, 0},
                                               {IrOpcode::Pop, 0},
                                               {IrOpcode::TaskWait, 0},
                                               {IrOpcode::ReturnI32, 0}}),
                                   sumTo()});
  CHECK(optimizer_test::run(twice, {"program"}).error == "task handle already waited");
  expectMatchesVm(twice, "wait_twice");

  const IrModule notHandle = moduleOf({functionOf(
      "/main", 0, {{IrOpcode::PushI32, 7}, {IrOpcode::TaskWait, 0}, {IrOpcode::ReturnI32, 0}})});
  CHECK(optimizer_test::run(notHandle, {"program"}).error == "invalid task handle in IR");
  expectMatchesVm(notHandle, "wait_no_handle");
}

TEST_CASE("an optexe task hands heap blocks to its waiter, which frees them") {
  // Three tasks alive together each allocate on their own thread's arena (a byte region, a string heap block and a
  // slot block), write to it and return the address; the spawner reads all three and frees them, 300 times over, so
  // arenas are handed from task to task and every free crosses threads.
  IrModule module;
  module.entryIndex = 0;
  module.hostImports.push_back(
      {"__pstext_alloc", {primec::IrHostValueKind::U64}, primec::IrHostValueKind::U64});
  module.hostImports.push_back(
      {"__pstext_free", {primec::IrHostValueKind::U64}, primec::IrHostValueKind::Void});
  // main locals: 0 total, 1 i, 2 bytes, 3 text, 4 slots.
  module.functions.push_back(functionOf("/main",
                                        0,
                                        {/* 0*/ {IrOpcode::PushI64, 0},
                                         /* 1*/ {IrOpcode::StoreLocal, 0},
                                         /* 2*/ {IrOpcode::PushI64, 0},
                                         /* 3*/ {IrOpcode::StoreLocal, 1},
                                         /* 4*/ {IrOpcode::LoadLocal, 1},
                                         /* 5*/ {IrOpcode::PushI64, 300},
                                         /* 6*/ {IrOpcode::CmpLtI64, 0},
                                         /* 7*/ {IrOpcode::JumpIfZero, 52},
                                         /* 8*/ {IrOpcode::LoadLocal, 1},
                                         /* 9*/ {IrOpcode::TaskSpawn, 1},
                                         /*10*/ {IrOpcode::StoreLocal, 2},
                                         /*11*/ {IrOpcode::LoadLocal, 1},
                                         /*12*/ {IrOpcode::TaskSpawn, 2},
                                         /*13*/ {IrOpcode::StoreLocal, 3},
                                         /*14*/ {IrOpcode::LoadLocal, 1},
                                         /*15*/ {IrOpcode::TaskSpawn, 3},
                                         /*16*/ {IrOpcode::StoreLocal, 4},
                                         /*17*/ {IrOpcode::LoadLocal, 2},
                                         /*18*/ {IrOpcode::TaskWait, 0},
                                         /*19*/ {IrOpcode::StoreLocal, 2},
                                         /*20*/ {IrOpcode::LoadLocal, 3},
                                         /*21*/ {IrOpcode::TaskWait, 0},
                                         /*22*/ {IrOpcode::StoreLocal, 3},
                                         /*23*/ {IrOpcode::LoadLocal, 4},
                                         /*24*/ {IrOpcode::TaskWait, 0},
                                         /*25*/ {IrOpcode::StoreLocal, 4},
                                         /*26*/ {IrOpcode::LoadLocal, 0},
                                         /*27*/ {IrOpcode::LoadLocal, 2},
                                         /*28*/ {IrOpcode::LoadU8, 0},
                                         /*29*/ {IrOpcode::AddI64, 0},
                                         /*30*/ {IrOpcode::LoadLocal, 3},
                                         /*31*/ {IrOpcode::PushI64, 31},
                                         /*32*/ {IrOpcode::AddI64, 0},
                                         /*33*/ {IrOpcode::LoadU8, 0},
                                         /*34*/ {IrOpcode::AddI64, 0},
                                         /*35*/ {IrOpcode::LoadLocal, 4},
                                         /*36*/ {IrOpcode::PushI64, 16},
                                         /*37*/ {IrOpcode::AddI64, 0},
                                         /*38*/ {IrOpcode::LoadIndirect, 0},
                                         /*39*/ {IrOpcode::AddI64, 0},
                                         /*40*/ {IrOpcode::StoreLocal, 0},
                                         /*41*/ {IrOpcode::LoadLocal, 2},
                                         /*42*/ {IrOpcode::HeapFree, 0},
                                         /*43*/ {IrOpcode::LoadLocal, 3},
                                         /*44*/ {IrOpcode::CallHost, 1},
                                         /*45*/ {IrOpcode::LoadLocal, 4},
                                         /*46*/ {IrOpcode::HeapFree, 0},
                                         /*47*/ {IrOpcode::LoadLocal, 1},
                                         /*48*/ {IrOpcode::PushI64, 1},
                                         /*49*/ {IrOpcode::AddI64, 0},
                                         /*50*/ {IrOpcode::StoreLocal, 1},
                                         /*51*/ {IrOpcode::Jump, 4},
                                         /*52*/ {IrOpcode::LoadLocal, 0},
                                         /*53*/ {IrOpcode::PrintI64, Newline},
                                         /*54*/ {IrOpcode::PushI32, 0},
                                         /*55*/ {IrOpcode::ReturnI32, 0}}));
  // make_bytes(n): a 24-byte region whose first byte is n (mod 256). Locals: 0 n, 1 address.
  module.functions.push_back(functionOf("/make_bytes",
                                        1,
                                        {{IrOpcode::StoreLocal, 0},
                                         {IrOpcode::PushI64, 24},
                                         {IrOpcode::HeapAllocBytes, 0},
                                         {IrOpcode::StoreLocal, 1},
                                         {IrOpcode::LoadLocal, 1},
                                         {IrOpcode::LoadLocal, 0},
                                         {IrOpcode::StoreU8, 0},
                                         {IrOpcode::Pop, 0},
                                         {IrOpcode::LoadLocal, 1},
                                         {IrOpcode::ReturnI64, 0}}));
  // make_text(n): a 32-byte string heap block whose last byte is n + 1 (mod 256).
  module.functions.push_back(functionOf("/make_text",
                                        1,
                                        {{IrOpcode::StoreLocal, 0},
                                         {IrOpcode::PushI64, 32},
                                         {IrOpcode::CallHost, 0},
                                         {IrOpcode::StoreLocal, 1},
                                         {IrOpcode::LoadLocal, 1},
                                         {IrOpcode::PushI64, 31},
                                         {IrOpcode::AddI64, 0},
                                         {IrOpcode::LoadLocal, 0},
                                         {IrOpcode::PushI64, 1},
                                         {IrOpcode::AddI64, 0},
                                         {IrOpcode::StoreU8, 0},
                                         {IrOpcode::Pop, 0},
                                         {IrOpcode::LoadLocal, 1},
                                         {IrOpcode::ReturnI64, 0}}));
  // make_slots(n): a block of two slots whose second slot is n * 3.
  module.functions.push_back(functionOf("/make_slots",
                                        1,
                                        {{IrOpcode::StoreLocal, 0},
                                         {IrOpcode::PushI64, 2},
                                         {IrOpcode::HeapAlloc, 0},
                                         {IrOpcode::StoreLocal, 1},
                                         {IrOpcode::LoadLocal, 1},
                                         {IrOpcode::PushI64, 16},
                                         {IrOpcode::AddI64, 0},
                                         {IrOpcode::LoadLocal, 0},
                                         {IrOpcode::PushI64, 3},
                                         {IrOpcode::MulI64, 0},
                                         {IrOpcode::StoreIndirect, 0},
                                         {IrOpcode::Pop, 0},
                                         {IrOpcode::LoadLocal, 1},
                                         {IrOpcode::ReturnI64, 0}}));
  const optimizer_test::Outcome vm = optimizer_test::run(module, {"program"});
  REQUIRE_MESSAGE(vm.ok, vm.error);
  // Bytes n mod 256 and (n + 1) mod 256 for n in 0 .. 299, plus 3 * (0 + ... + 299).
  CHECK(vm.output == "201766\n");
  expectMatchesVm(module, "heap_handoff");
}

TEST_CASE("a fault in an optexe task ends the program while another task still runs") {
  // The spinning task never ends and main waits for it, so the fault in the dividing task is the only way out.
  IrModule faulting = moduleOf({functionOf("/main",
                                           0,
                                           {{IrOpcode::PushI32, 0},
                                            {IrOpcode::TaskSpawn, 1},
                                            {IrOpcode::TaskWait, 0},
                                            {IrOpcode::ReturnI32, 0}}),
                                divide()});
  const optimizer_test::Outcome vm = optimizer_test::run(faulting, {"program"});
  REQUIRE_FALSE(vm.ok);
  CHECK(vm.error == "division by zero in IR");

  IrModule module;
  module.entryIndex = 0;
  const uint64_t started = lineOf(module, "started");
  module.functions.push_back(functionOf("/main",
                                        0,
                                        {{IrOpcode::PrintString, started},
                                         {IrOpcode::TaskSpawn, 2},
                                         {IrOpcode::StoreLocal, 0},
                                         {IrOpcode::PushI32, 0},
                                         {IrOpcode::TaskSpawn, 1},
                                         {IrOpcode::StoreLocal, 1},
                                         {IrOpcode::LoadLocal, 0},
                                         {IrOpcode::TaskWait, 0},
                                         {IrOpcode::LoadLocal, 1},
                                         {IrOpcode::TaskWait, 0},
                                         {IrOpcode::AddI32, 0},
                                         {IrOpcode::ReturnI32, 0}}));
  module.functions.push_back(divide());
  module.functions.push_back(spin());
  const ProgramRun actual = compileAndRun(module, "fault_while_running");
  REQUIRE_MESSAGE(actual.built, actual.buildError);
  CHECK(actual.exitCode == 3);
  CHECK(actual.out == "started\n");
  CHECK(actual.err == "VM error: " + vm.error + "\n");
}

TEST_CASE(
    "a trap in an optexe task and the entry's return end the program while a task still runs") {
  {
    // The trapping task prints, then ends the program with code 7; main waits for the task that never ends.
    IrModule module;
    module.entryIndex = 0;
    const uint64_t before = lineOf(module, "before");
    const uint64_t inTask = lineOf(module, "in task");
    module.functions.push_back(functionOf("/main",
                                          0,
                                          {{IrOpcode::PrintString, before},
                                           {IrOpcode::TaskSpawn, 2},
                                           {IrOpcode::StoreLocal, 0},
                                           {IrOpcode::TaskSpawn, 1},
                                           {IrOpcode::StoreLocal, 1},
                                           {IrOpcode::LoadLocal, 0},
                                           {IrOpcode::TaskWait, 0},
                                           {IrOpcode::LoadLocal, 1},
                                           {IrOpcode::TaskWait, 0},
                                           {IrOpcode::AddI32, 0},
                                           {IrOpcode::ReturnI32, 0}}));
    module.functions.push_back(
        functionOf("/fails", 0, {{IrOpcode::PrintString, inTask}, {IrOpcode::Trap, 7}}));
    module.functions.push_back(spin());
    const ProgramRun actual = compileAndRun(module, "trap_while_running");
    REQUIRE_MESSAGE(actual.built, actual.buildError);
    CHECK(actual.exitCode == 7);
    CHECK(actual.out == "before\nin task\n");
    CHECK(actual.err.empty());
  }
  {
    // The main task's return ends the program; the task it never waits is still running.
    IrModule module;
    module.entryIndex = 0;
    const uint64_t done = lineOf(module, "done");
    module.functions.push_back(functionOf("/main",
                                          0,
                                          {{IrOpcode::TaskSpawn, 1},
                                           {IrOpcode::StoreLocal, 0},
                                           {IrOpcode::PrintString, done},
                                           {IrOpcode::PushI32, 5},
                                           {IrOpcode::ReturnI32, 0}}));
    module.functions.push_back(spin());
    const ProgramRun actual = compileAndRun(module, "return_while_running");
    REQUIRE_MESSAGE(actual.built, actual.buildError);
    CHECK(actual.exitCode == 5);
    CHECK(actual.out == "done\n");
    CHECK(actual.err.empty());
  }
}

TEST_CASE("lines printed by four optexe tasks at once are never mixed") {
  // Each task prints 1000 times: its own 13-digit number, a string literal and the program's first argument, each
  // as a line. Whatever the order, every line of the output is one of those whole, and each appears as often as it
  // was printed.
  constexpr int64_t Digits = 1111111111111;
  constexpr int Turns = 1000;
  const std::string literal =
      "a whole line printed by a task, long enough to be torn if lines could mix";
  const std::string argument = "argument-line-0123456789-abcdefghijklmnopqrstuvwxyz";
  IrModule module;
  module.entryIndex = 0;
  const uint64_t literalLine = lineOf(module, literal);
  // main locals: 0 .. 3 handles.
  std::vector<IrInstruction> mainCode;
  for (uint64_t task = 0; task < 4; ++task) {
    mainCode.push_back({IrOpcode::PushI64, task + 1});
    mainCode.push_back({IrOpcode::TaskSpawn, 1});
    mainCode.push_back({IrOpcode::StoreLocal, task});
  }
  for (uint64_t task = 0; task < 4; ++task) {
    mainCode.push_back({IrOpcode::LoadLocal, task});
    mainCode.push_back({IrOpcode::TaskWait, 0});
    mainCode.push_back({IrOpcode::Pop, 0});
  }
  mainCode.push_back({IrOpcode::PushI32, 0});
  mainCode.push_back({IrOpcode::ReturnI32, 0});
  module.functions.push_back(functionOf("/main", 0, std::move(mainCode)));
  // chatter(n). Locals: 0 n, 1 i.
  module.functions.push_back(functionOf("/chatter",
                                        1,
                                        {/* 0*/ {IrOpcode::StoreLocal, 0},
                                         /* 1*/ {IrOpcode::PushI32, 0},
                                         /* 2*/ {IrOpcode::StoreLocal, 1},
                                         /* 3*/ {IrOpcode::LoadLocal, 1},
                                         /* 4*/ {IrOpcode::PushI32, Turns},
                                         /* 5*/ {IrOpcode::CmpLtI32, 0},
                                         /* 6*/ {IrOpcode::JumpIfZero, 19},
                                         /* 7*/ {IrOpcode::LoadLocal, 0},
                                         /* 8*/ {IrOpcode::PushI64, static_cast<uint64_t>(Digits)},
                                         /* 9*/ {IrOpcode::MulI64, 0},
                                         /*10*/ {IrOpcode::PrintI64, Newline},
                                         /*11*/ {IrOpcode::PrintString, literalLine},
                                         /*12*/ {IrOpcode::PushI32, 1},
                                         /*13*/ {IrOpcode::PrintArgv, Newline},
                                         /*14*/ {IrOpcode::LoadLocal, 1},
                                         /*15*/ {IrOpcode::PushI32, 1},
                                         /*16*/ {IrOpcode::AddI32, 0},
                                         /*17*/ {IrOpcode::StoreLocal, 1},
                                         /*18*/ {IrOpcode::Jump, 3},
                                         /*19*/ {IrOpcode::ReturnVoid, 0}}));
  const ProgramRun actual = compileAndRun(module, "whole_lines", {argument});
  REQUIRE_MESSAGE(actual.built, actual.buildError);
  CHECK(actual.exitCode == 0);
  CHECK(actual.err.empty());
  std::map<std::string, int> expected;
  for (int64_t task = 1; task <= 4; ++task) {
    expected[std::to_string(task * Digits)] = Turns;
  }
  expected[literal] = 4 * Turns;
  expected[argument] = 4 * Turns;
  std::map<std::string, int> seen;
  int torn = 0;
  for (const std::string &line : linesOf(actual.out)) {
    if (expected.count(line) == 0) {
      ++torn;
      if (torn <= 3) {
        INFO("unexpected line: " << line);
        CHECK(false);
      }
    } else {
      ++seen[line];
    }
  }
  CHECK(torn == 0);
  CHECK(seen == expected);
}

TEST_CASE("only an optexe module with task opcodes gets the task runtime") {
  const auto emitted = [](const IrModule &module) {
    std::string source;
    std::string error;
    REQUIRE_MESSAGE(primec::IrToOptCppEmitter().emitSource(module, source, error), error);
    return source;
  };
  const std::string plain = emitted(moduleOf(
      {functionOf(
           "/main", 0, {{IrOpcode::PushI64, 4}, {IrOpcode::Call, 1}, {IrOpcode::ReturnI32, 0}}),
       sumTo()}));
  CHECK_FALSE(contains(plain, "ps_task_spawn"));
  CHECK_FALSE(contains(plain, "pthread_create"));
  CHECK_FALSE(contains(plain, "PsWaits"));
  CHECK_FALSE(contains(plain, "std::_Exit(code)"));

  const std::string tasks = emitted(moduleOf({functionOf("/main",
                                                         0,
                                                         {{IrOpcode::PushI64, 4},
                                                          {IrOpcode::TaskSpawn, 1},
                                                          {IrOpcode::TaskWait, 0},
                                                          {IrOpcode::ReturnI32, 0}}),
                                              sumTo()}));
  CHECK(contains(tasks, "pthread_create"));
  // The task's thread enters sum_to through a function that unpacks the one argument.
  CHECK(
      contains(tasks, "uint64_t ps_t1(Rt &rt, const uint64_t *a) {\n  return ps_f1(rt, a[0]);\n}"));
  CHECK(contains(tasks, "= ps_task_spawn(rt, ps_t1, targs + 1, 1);"));
  CHECK(contains(tasks, "= ps_task_wait("));
  CHECK(contains(tasks, "std::_Exit(code)"));

  // A spawn of a function the module does not have faults when it runs, with the VM's message.
  const std::string missing = emitted(
      moduleOf({functionOf("/main", 0, {{IrOpcode::TaskSpawn, 9}, {IrOpcode::ReturnI32, 0}})}));
  CHECK(contains(missing, "ps_fault(\"invalid task function in IR\");"));
}

TEST_CASE("primec lowers a spawned scalar call to a task for optexe") {
  const std::string source = R"(
[return<i64>]
twice([i64] value) {
  [i64 mut] result{value}
  result = result * 2i64
  return(result)
}

[return<i32>]
bump([i32 copy mut] value) {
  value = value + 1i32
  return(value)
}

[sum]
Item {
  [i32] small
  [i64] large
}

[return<i32>]
pick_item([Item move] item) {
  [i32 mut] out{0i32}
  pick(item) {
    small(s) {
      out = s
    }
    large(l) {
      out = convert<i32>(l)
    }
  }
  return(out)
}

[effects(task, io_out), return<int>]
main() {
  [Task<i64>] doubled{[spawn] twice(21i64)};
  [Task<i32>] bumped{[spawn] bump(4i32)};
  [Task<i32>] picked{[spawn] pick_item(Item{[large] 9i64})};
  print_line(wait(doubled))
  print_line(wait(bumped))
  print_line(wait(picked))
  return(0i32)
}
)";
  const std::filesystem::path sourcePath = scratch("spawn_scalar.prime");
  std::ofstream(sourcePath) << source;
  const std::filesystem::path cppPath = scratch("spawn_scalar.cpp");
  const ProgramRun emit =
      runCommand("./primec --emit=optcpp '" + sourcePath.string() +
                     "' --entry /main --opt-report -o '" + cppPath.string() + "'",
                 "spawn_scalar_emit");
  REQUIRE_MESSAGE(emit.exitCode == 0, emit.err);
  const std::string cpp = readText(cppPath);
  // twice and bump (`copy mut`, the task's own slot) run on threads; an owned sum keeps pick_item inline, and the
  // report says so.
  CHECK(contains(cpp, "= ps_task_spawn(rt, ps_t"));
  CHECK(contains(cpp, "= ps_task_wait("));
  CHECK(
      contains(emit.err,
               "lowering: spawn of /pick_item runs inline: parameter `item` is a `copy` or `move` "
               "sum, which no real call takes yet\n"));
  CHECK_FALSE(contains(emit.err, "/twice"));
  CHECK_FALSE(contains(emit.err, "/bump"));

  const std::filesystem::path binary = scratch("spawn_scalar.bin");
  const ProgramRun build = runCommand("./primec --emit=optexe '" + sourcePath.string() +
                                          "' --entry /main -o '" + binary.string() + "'",
                                      "spawn_scalar_build");
  REQUIRE_MESSAGE(build.exitCode == 0, build.err);
  const ProgramRun actual = runCommand("'" + binary.string() + "'", "spawn_scalar_run");
  CHECK(actual.exitCode == 0);
  CHECK(actual.out == "42\n5\n9\n");
  CHECK(actual.err.empty());
}

TEST_CASE("primec lowers a spawned call with String and struct values to a task") {
  // Positive: a `String` parameter, a `String` result and a struct result with a `String` field run as tasks, with
  // no inline note. Negative: a collection parameter or result and a `string` parameter keep the call inline, and
  // the report names the reason.
  const std::string source = R"(
import /std/text/*
import /std/collections/*

[struct]
Reply() {
  [String] body{String{}}
  [i32] code{0i32}
}

[return<String> effects(heap_alloc)]
greet([String] name) {
  [String mut] out{String{}}
  out.append("hello ")
  out.appendString(name)
  return(out)
}

[return<Reply> effects(heap_alloc)]
make_reply([String] text, [i32] code) {
  [String mut] body{String{}}
  body.appendString(text)
  return(Reply{body, code})
}

[return<i32>]
total([vector<i32>] values) {
  return(values.count())
}

[return<i32>]
label_size([string] label) {
  return(label.count())
}

[return<vector<i32>> effects(heap_alloc)]
numbers([i32] count) {
  [vector<i32> mut] out{vector<i32>()}
  out.push(count)
  return(out)
}

[effects(task, io_out, heap_alloc), return<int>]
main() {
  [String mut] who{String{}}
  who.append("a name long enough to live on the string heap")
  [Task<String>] greeting{[spawn] greet(who)};
  [Task<Reply>] reply{[spawn] make_reply(who, 7i32)};
  [String] text{wait(greeting)}
  [Reply] made{wait(reply)}
  print_line(text.count())
  print_line(made.body.count() + made.code)
  [vector<i32> mut] values{vector<i32>()}
  values.push(4i32)
  [Task<i32>] counted{[spawn] total(values)};
  print_line(wait(counted))
  [Task<i32>] labeled{[spawn] label_size("four")};
  print_line(wait(labeled))
  [Task<vector<i32>>] listed{[spawn] numbers(3i32)};
  [vector<i32>] list{wait(listed)}
  print_line(list.count())
  return(0i32)
}
)";
  const std::filesystem::path sourcePath = scratch("spawn_structs.prime");
  std::ofstream(sourcePath) << source;
  const std::string expectedOut = "51\n52\n1\n4\n1\n";
  const auto checkNotes = [](const std::string &report) {
    CHECK_FALSE(contains(report, "/greet"));
    CHECK_FALSE(contains(report, "/make_reply"));
    CHECK(contains(
        report,
        "lowering: spawn of /total runs inline: parameter `values` is a collection or another "
        "template type\n"));
    CHECK(contains(report,
                   "lowering: spawn of /label_size runs inline: parameter `label` is a `string`, "
                   "which no real "
                   "call takes yet\n"));
    CHECK(contains(report,
                   "lowering: spawn of /numbers runs inline: its result is not a scalar or a "
                   "struct a real call "
                   "returns\n"));
  };

  const std::filesystem::path cppPath = scratch("spawn_structs.cpp");
  const ProgramRun emit =
      runCommand("./primec --emit=optcpp '" + sourcePath.string() +
                     "' --entry /main --opt-report -o '" + cppPath.string() + "'",
                 "spawn_structs_emit");
  REQUIRE_MESSAGE(emit.exitCode == 0, emit.err);
  checkNotes(emit.err);
  CHECK(contains(readText(cppPath), "= ps_task_spawn(rt, ps_t"));

  const ProgramRun vm =
      runCommand("./primec --emit=vm '" + sourcePath.string() + "' --entry /main --opt-report",
                 "spawn_structs_vm");
  CHECK(vm.exitCode == 0);
  CHECK(vm.out == expectedOut);
  checkNotes(vm.err);

  const std::filesystem::path binary = scratch("spawn_structs.bin");
  const ProgramRun build = runCommand("./primec --emit=optexe '" + sourcePath.string() +
                                          "' --entry /main -o '" + binary.string() + "'",
                                      "spawn_structs_build");
  REQUIRE_MESSAGE(build.exitCode == 0, build.err);
  const ProgramRun actual = runCommand("'" + binary.string() + "'", "spawn_structs_run");
  CHECK(actual.exitCode == 0);
  CHECK(actual.out == expectedOut);
  CHECK(actual.err.empty());
}

TEST_CASE("optexe runs the atomic opcodes like the vm") {
  // One thread: every operation at both widths, on a heap slot kept in local 0.
  std::vector<IrInstruction> body = {
      {IrOpcode::PushI32, 1}, {IrOpcode::HeapAlloc, 0}, {IrOpcode::StoreLocal, 0}};
  const auto operation = [&](IrOpcode op, uint64_t width, std::vector<int64_t> operands) {
    body.push_back({IrOpcode::LoadLocal, 0});
    for (const int64_t operand : operands) {
      body.push_back({IrOpcode::PushI64, static_cast<uint64_t>(operand)});
    }
    body.push_back({op, width});
    if (op != IrOpcode::AtomicStore) {
      body.push_back({IrOpcode::PrintI64, Newline});
    }
  };
  operation(IrOpcode::AtomicStore, 64, {int64_t{1} << 40});
  operation(IrOpcode::AtomicAdd, 64, {-1});
  operation(IrOpcode::AtomicExchange, 64, {20});
  operation(IrOpcode::AtomicCompareExchange, 64, {20, 1});
  operation(IrOpcode::AtomicCompareExchange, 64, {20, 2});
  operation(IrOpcode::AtomicLoad, 64, {});
  operation(IrOpcode::AtomicStore, 32, {2147483647});
  operation(IrOpcode::AtomicAdd, 32, {1});
  operation(IrOpcode::AtomicLoad, 32, {});
  operation(IrOpcode::AtomicLoad, 64, {});
  operation(IrOpcode::AtomicExchange, 32, {-5});
  operation(IrOpcode::AtomicCompareExchange, 32, {-5, 9});
  operation(IrOpcode::AtomicStore, 32, {(int64_t{1} << 32) + 3});
  operation(IrOpcode::AtomicLoad, 64, {});
  body.push_back({IrOpcode::PushI32, 0});
  body.push_back({IrOpcode::ReturnI32, 0});
  expectMatchesVm(moduleOf({functionOf("/main", 0, body)}), "atomic_operations");

  // Four threads add 1 to one cell 20000 times each; no addition is lost. bump(cell): local 1 counts the turns.
  const IrFunction bump = functionOf("/bump",
                                     1,
                                     {/* 0*/ {IrOpcode::StoreLocal, 0},
                                      /* 1*/ {IrOpcode::PushI64, 0},
                                      /* 2*/ {IrOpcode::StoreLocal, 1},
                                      /* 3*/ {IrOpcode::LoadLocal, 1},
                                      /* 4*/ {IrOpcode::PushI64, 20000},
                                      /* 5*/ {IrOpcode::CmpLtI64, 0},
                                      /* 6*/ {IrOpcode::JumpIfZero, 16},
                                      /* 7*/ {IrOpcode::LoadLocal, 0},
                                      /* 8*/ {IrOpcode::PushI64, 1},
                                      /* 9*/ {IrOpcode::AtomicAdd, 64},
                                      /*10*/ {IrOpcode::Pop, 0},
                                      /*11*/ {IrOpcode::LoadLocal, 1},
                                      /*12*/ {IrOpcode::PushI64, 1},
                                      /*13*/ {IrOpcode::AddI64, 0},
                                      /*14*/ {IrOpcode::StoreLocal, 1},
                                      /*15*/ {IrOpcode::Jump, 3},
                                      /*16*/ {IrOpcode::ReturnVoid, 0}});
  std::vector<IrInstruction> shared = {
      {IrOpcode::PushI32, 1}, {IrOpcode::HeapAlloc, 0}, {IrOpcode::StoreLocal, 0}};
  for (uint64_t task = 1; task <= 4; ++task) {
    shared.push_back({IrOpcode::LoadLocal, 0});
    shared.push_back({IrOpcode::TaskSpawn, 1});
    shared.push_back({IrOpcode::StoreLocal, task});
  }
  for (uint64_t task = 1; task <= 4; ++task) {
    shared.push_back({IrOpcode::LoadLocal, task});
    shared.push_back({IrOpcode::TaskWait, 0});
    shared.push_back({IrOpcode::Pop, 0});
  }
  shared.push_back({IrOpcode::LoadLocal, 0});
  shared.push_back({IrOpcode::AtomicLoad, 64});
  shared.push_back({IrOpcode::PrintI64, Newline});
  shared.push_back({IrOpcode::PushI32, 0});
  shared.push_back({IrOpcode::ReturnI32, 0});
  const IrModule sharedModule = moduleOf({functionOf("/main", 0, shared), bump});
  CHECK(optimizer_test::run(sharedModule, {"program"}).output == "80000\n");
  expectMatchesVm(sharedModule, "atomic_shared_add");
}

// local 0 is a byte region of `size` bytes; `offset` bytes into it an atomic store of 9, an add of 3 and a load.
static IrModule byteCellProgram(uint64_t size, uint64_t offset) {
  std::vector<IrInstruction> body = {
      {IrOpcode::PushI32, size}, {IrOpcode::HeapAllocBytes, 0}, {IrOpcode::StoreLocal, 0}};
  const auto cell = [&]() {
    body.push_back({IrOpcode::LoadLocal, 0});
    body.push_back({IrOpcode::PushI64, offset});
    body.push_back({IrOpcode::AddI64, 0});
  };
  cell();
  body.push_back({IrOpcode::PushI64, 9});
  body.push_back({IrOpcode::AtomicStore, 64});
  cell();
  body.push_back({IrOpcode::PushI64, 3});
  body.push_back({IrOpcode::AtomicAdd, 64});
  body.push_back({IrOpcode::PrintI64, Newline});
  cell();
  body.push_back({IrOpcode::AtomicLoad, 64});
  body.push_back({IrOpcode::PrintI64, Newline});
  body.push_back({IrOpcode::PushI32, 0});
  body.push_back({IrOpcode::ReturnI32, 0});
  return moduleOf({functionOf("/main", 0, body)});
}

TEST_CASE("optexe runs atomic opcodes on an aligned byte-region cell like the vm") {
  const IrModule whole = byteCellProgram(24, 8);
  CHECK(optimizer_test::run(whole, {"program"}).output == "9\n12\n");
  expectMatchesVm(whole, "atomic_byte_cell");
}

TEST_CASE("optexe faults by name on an atomic opcode at an unaligned or partial byte-region cell") {
  const IrModule unaligned = byteCellProgram(24, 12);
  CHECK(optimizer_test::run(unaligned, {"program"})
            .error.rfind("unaligned atomic address in IR: ", 0) == 0);
  expectMatchesVm(unaligned, "atomic_byte_cell_unaligned");

  const IrModule beyond = byteCellProgram(12, 8);
  CHECK(optimizer_test::run(beyond, {"program"}).error.rfind("invalid atomic address in IR: ", 0) ==
        0);
  expectMatchesVm(beyond, "atomic_byte_cell_beyond");
}

TEST_CASE("only an optexe module with atomic opcodes gets the atomic runtime") {
  const auto emitted = [](const IrModule &module) {
    std::string source;
    std::string error;
    REQUIRE_MESSAGE(primec::IrToOptCppEmitter().emitSource(module, source, error), error);
    return source;
  };
  const std::string plain = emitted(moduleOf(
      {functionOf(
           "/main", 0, {{IrOpcode::PushI64, 4}, {IrOpcode::Call, 1}, {IrOpcode::ReturnI32, 0}}),
       sumTo()}));
  CHECK_FALSE(contains(plain, "PsAtomicSlot"));
  CHECK_FALSE(contains(plain, "ps_atomic_"));

  const std::string atomics = emitted(moduleOf({functionOf("/main",
                                                           0,
                                                           {{IrOpcode::PushI32, 1},
                                                            {IrOpcode::HeapAlloc, 0},
                                                            {IrOpcode::StoreLocal, 0},
                                                            {IrOpcode::LoadLocal, 0},
                                                            {IrOpcode::PushI64, 2},
                                                            {IrOpcode::AtomicAdd, 32},
                                                            {IrOpcode::Pop, 0},
                                                            {IrOpcode::LoadLocal, 0},
                                                            {IrOpcode::AtomicLoad, 64},
                                                            {IrOpcode::ReturnI32, 0}})}));
  CHECK(contains(atomics, "std::atomic_ref<uint64_t>"));
  CHECK(contains(atomics, " = ps_atomic_add(ps_resolve_atomic(rt, "));
  CHECK(contains(atomics, ", true);"));
  CHECK(contains(atomics, " = ps_atomic_load(ps_resolve_atomic(rt, "));
  // No task opcode, so no task runtime.
  CHECK_FALSE(contains(atomics, "pthread_create"));
}

TEST_CASE("primec runs a spawned call with an Atomic argument as a task") {
  const std::string source = R"(
import /std/sync/*

[effects(atomic), return<void>]
add_many([Atomic<i64>] counter, [i32] rounds) {
  [i32 mut] i{0i32}
  while(i < rounds) {
    counter.add(1i64)
    i = i + 1i32
  }
}

[effects(atomic), return<i32>]
take([Atomic<i32>] flag) {
  return(flag.exchange(1i32))
}

[effects(atomic, heap_alloc, io_out, task), return<int>]
main() {
  counter{Atomic<i64>{0i64}}
  [Task<void>] a{[spawn] add_many(counter, 1000i32)}
  [Task<void>] b{[spawn] add_many(counter, 1000i32)}
  wait(a)
  wait(b)
  print_line(counter.load())
  flag{Atomic<i32>{0i32}}
  [Task<i32>] first{[spawn] take(flag)}
  [Task<i32>] second{[spawn] take(flag)}
  print_line(wait(first) + wait(second))
  return(0i32)
}
)";
  const std::filesystem::path sourcePath = scratch("spawn_atomic.prime");
  std::ofstream(sourcePath) << source;
  // Neither backend that runs tasks keeps the calls inline: the report has no note.
  const std::filesystem::path cppPath = scratch("spawn_atomic.cpp");
  const ProgramRun emit =
      runCommand("./primec --emit=optcpp '" + sourcePath.string() +
                     "' --entry /main --opt-report -o '" + cppPath.string() + "'",
                 "spawn_atomic_emit");
  REQUIRE_MESSAGE(emit.exitCode == 0, emit.err);
  CHECK_FALSE(contains(emit.err, "runs inline"));
  const std::string cpp = readText(cppPath);
  CHECK(contains(cpp, "= ps_task_spawn(rt, ps_t"));
  CHECK(contains(cpp, "ps_atomic_add("));
  const ProgramRun vmEmit = runCommand("./primec --emit=vm '" + sourcePath.string() +
                                           "' --entry /main --opt-report -o '" +
                                           scratch("spawn_atomic.psir").string() + "'",
                                       "spawn_atomic_vm_emit");
  REQUIRE_MESSAGE(vmEmit.exitCode == 0, vmEmit.err);
  CHECK_FALSE(contains(vmEmit.err, "runs inline"));
  const ProgramRun vmRun =
      runCommand("./primevm '" + sourcePath.string() + "' --entry /main", "spawn_atomic_vm");
  CHECK(vmRun.exitCode == 0);
  CHECK(vmRun.out == "2000\n1\n");
}

TEST_CASE("only an optexe module with mutex opcodes gets the mutex runtime") {
  const auto emitted = [](const IrModule &module) {
    std::string source;
    std::string error;
    REQUIRE_MESSAGE(primec::IrToOptCppEmitter().emitSource(module, source, error), error);
    return source;
  };
  const std::string atomics = emitted(moduleOf({functionOf("/main",
                                                           0,
                                                           {{IrOpcode::PushI32, 1},
                                                            {IrOpcode::HeapAlloc, 0},
                                                            {IrOpcode::AtomicLoad, 64},
                                                            {IrOpcode::ReturnI32, 0}})}));
  CHECK_FALSE(contains(atomics, "ps_mutex_"));

  const IrModule locking = moduleOf({functionOf("/main",
                                                0,
                                                {{IrOpcode::PushI32, 1},
                                                 {IrOpcode::HeapAlloc, 0},
                                                 {IrOpcode::StoreLocal, 0},
                                                 {IrOpcode::LoadLocal, 0},
                                                 {IrOpcode::MutexLock, 0},
                                                 {IrOpcode::PushI32, 4},
                                                 {IrOpcode::PrintI32, Newline},
                                                 {IrOpcode::LoadLocal, 0},
                                                 {IrOpcode::MutexUnlock, 0},
                                                 {IrOpcode::PushI32, 0},
                                                 {IrOpcode::ReturnI32, 0}})});
  const std::string mutexes = emitted(locking);
  CHECK(contains(mutexes, "ps_mutex_lock(rt, ps_resolve(rt, "));
  CHECK(contains(mutexes, "ps_mutex_unlock(rt, ps_resolve(rt, "));
  // No task opcode, so no task runtime.
  CHECK_FALSE(contains(mutexes, "pthread_create"));
  CHECK(optimizer_test::run(locking, {"program"}).output == "4\n");
  expectMatchesVm(locking, "mutex_lock_unlock");
}

TEST_CASE("primec runs a spawned call with a Mutex argument as a task") {
  const std::string source = R"(
import /std/sync/*

[effects(lock), return<void>]
add_many([Mutex<i64>] counter, [i32] rounds) {
  [i32 mut] i{0i32}
  while(i < rounds) {
    counter.locked([]([Reference<i64> mut] value) { value = value + 1i64 })
    i = i + 1i32
  }
}

[effects(lock, heap_alloc, io_out, task), return<int>]
main() {
  counter{Mutex<i64>{0i64}}
  [Task<void>] a{[spawn] add_many(counter, 1000i32)}
  [Task<void>] b{[spawn] add_many(counter, 1000i32)}
  wait(a)
  wait(b)
  print_line(counter.locked([]([Reference<i64>] value) { return(value + 0i64) }))
  return(0i32)
}
)";
  const std::filesystem::path sourcePath = scratch("spawn_mutex.prime");
  std::ofstream(sourcePath) << source;
  // Neither backend that runs tasks keeps the calls inline: the report has no note.
  const std::filesystem::path cppPath = scratch("spawn_mutex.cpp");
  const ProgramRun emit =
      runCommand("./primec --emit=optcpp '" + sourcePath.string() +
                     "' --entry /main --opt-report -o '" + cppPath.string() + "'",
                 "spawn_mutex_emit");
  REQUIRE_MESSAGE(emit.exitCode == 0, emit.err);
  CHECK_FALSE(contains(emit.err, "runs inline"));
  const std::string cpp = readText(cppPath);
  CHECK(contains(cpp, "= ps_task_spawn(rt, ps_t"));
  CHECK(contains(cpp, "ps_mutex_lock("));
  const ProgramRun vmEmit = runCommand("./primec --emit=vm '" + sourcePath.string() +
                                           "' --entry /main --opt-report -o '" +
                                           scratch("spawn_mutex.psir").string() + "'",
                                       "spawn_mutex_vm_emit");
  REQUIRE_MESSAGE(vmEmit.exitCode == 0, vmEmit.err);
  CHECK_FALSE(contains(vmEmit.err, "runs inline"));
  const ProgramRun vmRun =
      runCommand("./primevm '" + sourcePath.string() + "' --entry /main", "spawn_mutex_vm");
  CHECK(vmRun.exitCode == 0);
  CHECK(vmRun.out == "2000\n");
}

namespace {

// Locals 0 and 1 of a main that starts with these are the addresses of two fresh heap slots: two free locks.
std::vector<IrInstruction> withTwoLocks(std::vector<IrInstruction> body) {
  std::vector<IrInstruction> instructions = {{IrOpcode::PushI32, 1},
                                             {IrOpcode::HeapAlloc, 0},
                                             {IrOpcode::StoreLocal, 0},
                                             {IrOpcode::PushI32, 1},
                                             {IrOpcode::HeapAlloc, 0},
                                             {IrOpcode::StoreLocal, 1}};
  instructions.insert(instructions.end(), body.begin(), body.end());
  return instructions;
}

// section(lock): takes the lock, prints 1 and 2 and frees it.
IrFunction lockedSection() {
  return functionOf("/section",
                    1,
                    {{IrOpcode::StoreLocal, 0},
                     {IrOpcode::LoadLocal, 0},
                     {IrOpcode::MutexLock, 0},
                     {IrOpcode::PushI32, 1},
                     {IrOpcode::PrintI32, Newline},
                     {IrOpcode::PushI32, 2},
                     {IrOpcode::PrintI32, Newline},
                     {IrOpcode::LoadLocal, 0},
                     {IrOpcode::MutexUnlock, 0},
                     {IrOpcode::ReturnVoid, 0}});
}

} // namespace

TEST_CASE("an optexe waiter that holds the lock its task needs faults like the vm") {
  // No schedule matters in these: the task cannot pass the lock before main frees it, and main never frees it in
  // the second module; in the third the task ends with the lock.
  {
    // main: lock, spawn section (it waits), print 5, unlock, wait the task.
    const IrModule module = moduleOf({functionOf("/main",
                                                 0,
                                                 withTwoLocks({{IrOpcode::LoadLocal, 0},
                                                               {IrOpcode::MutexLock, 0},
                                                               {IrOpcode::LoadLocal, 0},
                                                               {IrOpcode::TaskSpawn, 1},
                                                               {IrOpcode::StoreLocal, 2},
                                                               {IrOpcode::PushI32, 5},
                                                               {IrOpcode::PrintI32, Newline},
                                                               {IrOpcode::LoadLocal, 0},
                                                               {IrOpcode::MutexUnlock, 0},
                                                               {IrOpcode::LoadLocal, 2},
                                                               {IrOpcode::TaskWait, 0},
                                                               {IrOpcode::Pop, 0},
                                                               {IrOpcode::PushI32, 0},
                                                               {IrOpcode::ReturnI32, 0}})),
                                      lockedSection()});
    CHECK(optimizer_test::run(module, {"program"}).output == "5\n1\n2\n");
    expectMatchesVm(module, "lock_handed_to_task");
  }
  {
    // main: lock, spawn section, wait the task while still holding the lock.
    const IrModule module = moduleOf({functionOf("/main",
                                                 0,
                                                 withTwoLocks({{IrOpcode::LoadLocal, 0},
                                                               {IrOpcode::MutexLock, 0},
                                                               {IrOpcode::LoadLocal, 0},
                                                               {IrOpcode::TaskSpawn, 1},
                                                               {IrOpcode::TaskWait, 0},
                                                               {IrOpcode::Pop, 0},
                                                               {IrOpcode::PushI32, 0},
                                                               {IrOpcode::ReturnI32, 0}})),
                                      lockedSection()});
    const optimizer_test::Outcome vm = optimizer_test::run(module, {"program"});
    CHECK_FALSE(vm.ok);
    CHECK(vm.error == "deadlock: every task is waiting");
    CHECK(vm.output.empty());
    expectMatchesVm(module, "waiter_holds_lock");
  }
  {
    // keep(lock) takes the lock and ends with it; main waits the task and then needs the lock. The last thread
    // that runs is the one that finds nobody left to free it.
    const IrModule module = moduleOf({functionOf("/main",
                                                 0,
                                                 withTwoLocks({{IrOpcode::LoadLocal, 0},
                                                               {IrOpcode::TaskSpawn, 1},
                                                               {IrOpcode::TaskWait, 0},
                                                               {IrOpcode::Pop, 0},
                                                               {IrOpcode::LoadLocal, 0},
                                                               {IrOpcode::MutexLock, 0},
                                                               {IrOpcode::PushI32, 0},
                                                               {IrOpcode::ReturnI32, 0}})),
                                      functionOf("/keep",
                                                 1,
                                                 {{IrOpcode::StoreLocal, 0},
                                                  {IrOpcode::LoadLocal, 0},
                                                  {IrOpcode::MutexLock, 0},
                                                  {IrOpcode::ReturnVoid, 0}})});
    const optimizer_test::Outcome vm = optimizer_test::run(module, {"program"});
    CHECK_FALSE(vm.ok);
    CHECK(vm.error == "deadlock: every task is waiting");
    expectMatchesVm(module, "lock_kept_by_ended_task");
  }
}

TEST_CASE("two optexe tasks that each hold the lock the other needs fault as a deadlock") {
  // both(first, second, ready): takes first, adds 1 to the atomic slot `ready` and turns until it holds 2, then
  // takes second. Each of the two tasks therefore holds its first lock before either asks for its second, on every
  // schedule; no thread sleeps for a time. The VM's default schedule would turn in the first task for ever, so only
  // the compiled program runs.
  const IrFunction both = functionOf("/both",
                                     3,
                                     {/* 0*/ {IrOpcode::StoreLocal, 2},
                                      /* 1*/ {IrOpcode::StoreLocal, 1},
                                      /* 2*/ {IrOpcode::StoreLocal, 0},
                                      /* 3*/ {IrOpcode::LoadLocal, 0},
                                      /* 4*/ {IrOpcode::MutexLock, 0},
                                      /* 5*/ {IrOpcode::LoadLocal, 2},
                                      /* 6*/ {IrOpcode::PushI64, 1},
                                      /* 7*/ {IrOpcode::AtomicAdd, 64},
                                      /* 8*/ {IrOpcode::Pop, 0},
                                      /* 9*/ {IrOpcode::LoadLocal, 2},
                                      /*10*/ {IrOpcode::AtomicLoad, 64},
                                      /*11*/ {IrOpcode::PushI64, 2},
                                      /*12*/ {IrOpcode::CmpLtI64, 0},
                                      /*13*/ {IrOpcode::JumpIfZero, 15},
                                      /*14*/ {IrOpcode::Jump, 9},
                                      /*15*/ {IrOpcode::LoadLocal, 1},
                                      /*16*/ {IrOpcode::MutexLock, 0},
                                      /*17*/ {IrOpcode::LoadLocal, 1},
                                      /*18*/ {IrOpcode::MutexUnlock, 0},
                                      /*19*/ {IrOpcode::LoadLocal, 0},
                                      /*20*/ {IrOpcode::MutexUnlock, 0},
                                      /*21*/ {IrOpcode::ReturnVoid, 0}});
  // main: local 2 is the `ready` slot, locals 3 and 4 the two handles.
  const IrModule module = moduleOf(
      {functionOf(
           "/main",
           0,
           withTwoLocks(
               {{IrOpcode::PushI32, 1},    {IrOpcode::HeapAlloc, 0},  {IrOpcode::StoreLocal, 2},
                {IrOpcode::LoadLocal, 0},  {IrOpcode::LoadLocal, 1},  {IrOpcode::LoadLocal, 2},
                {IrOpcode::TaskSpawn, 1},  {IrOpcode::StoreLocal, 3}, {IrOpcode::LoadLocal, 1},
                {IrOpcode::LoadLocal, 0},  {IrOpcode::LoadLocal, 2},  {IrOpcode::TaskSpawn, 1},
                {IrOpcode::StoreLocal, 4}, {IrOpcode::LoadLocal, 3},  {IrOpcode::TaskWait, 0},
                {IrOpcode::Pop, 0},        {IrOpcode::LoadLocal, 4},  {IrOpcode::TaskWait, 0},
                {IrOpcode::Pop, 0},        {IrOpcode::PushI32, 0},    {IrOpcode::ReturnI32, 0}})),
       both});
  const ProgramRun actual = compileAndRun(module, "opposite_order_deadlock");
  REQUIRE_MESSAGE(actual.built, actual.buildError);
  CHECK(actual.exitCode == 3);
  CHECK(actual.out.empty());
  CHECK(actual.err == "VM error: deadlock: every task is waiting\n");
}

TEST_CASE("only an optexe module with condition opcodes gets the condition runtime") {
  const auto emitted = [](const IrModule &module) {
    std::string source;
    std::string error;
    REQUIRE_MESSAGE(primec::IrToOptCppEmitter().emitSource(module, source, error), error);
    return source;
  };
  const std::string locking =
      emitted(moduleOf({functionOf("/main",
                                   0,
                                   withTwoLocks({{IrOpcode::LoadLocal, 0},
                                                 {IrOpcode::MutexLock, 0},
                                                 {IrOpcode::LoadLocal, 0},
                                                 {IrOpcode::MutexUnlock, 0},
                                                 {IrOpcode::PushI32, 0},
                                                 {IrOpcode::ReturnI32, 0}}))}));
  CHECK_FALSE(contains(locking, "ps_condition_"));

  // waiter(lock, condition, flag): takes the lock, waits until the flag is set, prints 2 and frees the lock.
  const IrFunction waiter = functionOf("/waiter",
                                       3,
                                       {/*0*/ {IrOpcode::StoreLocal, 2},
                                        /*1*/ {IrOpcode::StoreLocal, 1},
                                        /*2*/ {IrOpcode::StoreLocal, 0},
                                        /*3*/ {IrOpcode::LoadLocal, 0},
                                        /*4*/ {IrOpcode::MutexLock, 0},
                                        /*5*/ {IrOpcode::LoadLocal, 2},
                                        /*6*/ {IrOpcode::AtomicLoad, 64},
                                        /*7*/ {IrOpcode::PushI64, 0},
                                        /*8*/ {IrOpcode::CmpEqI64, 0},
                                        /*9*/ {IrOpcode::JumpIfZero, 16},
                                        /*10*/ {IrOpcode::LoadLocal, 1},
                                        /*11*/ {IrOpcode::LoadLocal, 0},
                                        /*12*/ {IrOpcode::ConditionWait, 0},
                                        /*13*/ {IrOpcode::LoadLocal, 0},
                                        /*14*/ {IrOpcode::MutexLock, 0},
                                        /*15*/ {IrOpcode::Jump, 5},
                                        /*16*/ {IrOpcode::PushI32, 2},
                                        /*17*/ {IrOpcode::PrintI32, Newline},
                                        /*18*/ {IrOpcode::LoadLocal, 0},
                                        /*19*/ {IrOpcode::MutexUnlock, 0},
                                        /*20*/ {IrOpcode::ReturnVoid, 0}});
  // main: local 2 is the flag slot. Spawns the waiter, then sets the flag, prints 5 and notifies with the lock
  // held: the waiter prints 2 only after it has seen the flag, so 5 is first on every schedule.
  const IrModule waiting = moduleOf(
      {functionOf(
           "/main", 0, withTwoLocks({{IrOpcode::PushI32, 1},         {IrOpcode::HeapAlloc, 0},
                                     {IrOpcode::StoreLocal, 2},      {IrOpcode::LoadLocal, 0},
                                     {IrOpcode::LoadLocal, 1},       {IrOpcode::LoadLocal, 2},
                                     {IrOpcode::TaskSpawn, 1},       {IrOpcode::StoreLocal, 3},
                                     {IrOpcode::LoadLocal, 0},       {IrOpcode::MutexLock, 0},
                                     {IrOpcode::LoadLocal, 2},       {IrOpcode::PushI64, 1},
                                     {IrOpcode::AtomicStore, 64},    {IrOpcode::PushI32, 5},
                                     {IrOpcode::PrintI32, Newline},  {IrOpcode::LoadLocal, 1},
                                     {IrOpcode::ConditionNotify, 0}, {IrOpcode::LoadLocal, 0},
                                     {IrOpcode::MutexUnlock, 0},     {IrOpcode::LoadLocal, 3},
                                     {IrOpcode::TaskWait, 0},        {IrOpcode::Pop, 0},
                                     {IrOpcode::PushI32, 0},         {IrOpcode::ReturnI32, 0}})),
       waiter});
  const std::string conditions = emitted(waiting);
  CHECK(contains(conditions, "ps_condition_wait(rt, ps_resolve(rt, "));
  CHECK(contains(conditions, "ps_condition_notify(ps_resolve(rt, "));
  const ProgramRun actual = compileAndRun(waiting, "condition_wait_notify");
  REQUIRE_MESSAGE(actual.built, actual.buildError);
  CHECK(actual.exitCode == 0);
  CHECK(actual.out == "5\n2\n");
  expectMatchesVm(waiting, "condition_wait_notify_vm");
}

TEST_CASE("primec runs a spawned call with a Channel argument as a task") {
  const std::string source = R"(
import /std/sync/*

[effects(channel), return<i64>]
first([Channel<i64>] jobs) {
  [i64 mut] got{0i64}
  pick(jobs.receive()) {
    none {
    }
    some(job) {
      got = job
    }
  }
  return(got)
}

[effects(task, channel, heap_alloc, io_out), return<int>]
main() {
  jobs{Channel<i64>{[capacity] 1i32}}
  [Task<i64>] taker{[spawn] first(jobs)}
  jobs.send(41i64)
  jobs.close()
  print_line(wait(taker) + 1i64)
  return(0i32)
}
)";
  const std::filesystem::path sourcePath = scratch("spawn_channel.prime");
  std::ofstream(sourcePath) << source;
  // Neither backend that runs tasks keeps the call inline: the report has no note.
  const std::filesystem::path cppPath = scratch("spawn_channel.cpp");
  const ProgramRun emit =
      runCommand("./primec --emit=optcpp '" + sourcePath.string() +
                     "' --entry /main --opt-report -o '" + cppPath.string() + "'",
                 "spawn_channel_emit");
  REQUIRE_MESSAGE(emit.exitCode == 0, emit.err);
  CHECK_FALSE(contains(emit.err, "runs inline"));
  const std::string cpp = readText(cppPath);
  CHECK(contains(cpp, "= ps_task_spawn(rt, ps_t"));
  CHECK(contains(cpp, "ps_condition_wait("));
  const ProgramRun vmEmit = runCommand("./primec --emit=vm '" + sourcePath.string() +
                                           "' --entry /main --opt-report -o '" +
                                           scratch("spawn_channel.psir").string() + "'",
                                       "spawn_channel_vm_emit");
  REQUIRE_MESSAGE(vmEmit.exitCode == 0, vmEmit.err);
  CHECK_FALSE(contains(vmEmit.err, "runs inline"));
  const ProgramRun vmRun =
      runCommand("./primevm '" + sourcePath.string() + "' --entry /main", "spawn_channel_vm");
  CHECK(vmRun.exitCode == 0);
  CHECK(vmRun.out == "42\n");
  const std::filesystem::path binary = scratch("spawn_channel.bin");
  const ProgramRun build = runCommand("./primec --emit=optexe '" + sourcePath.string() +
                                          "' --entry /main -o '" + binary.string() + "'",
                                      "spawn_channel_build");
  REQUIRE_MESSAGE(build.exitCode == 0, build.err);
  const ProgramRun actual = runCommand("'" + binary.string() + "'", "spawn_channel_run");
  CHECK(actual.exitCode == 0);
  CHECK(actual.out == "42\n");
}

TEST_SUITE_END();
