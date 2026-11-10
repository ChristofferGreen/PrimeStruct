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

[effects(task, io_out), return<int>]
main() {
  [Task<i64>] doubled{[spawn] twice(21i64)};
  [Task<i32>] bumped{[spawn] bump(4i32)};
  print_line(wait(doubled))
  print_line(wait(bumped))
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
  // twice runs on a thread; `copy mut` keeps bump inline, and the report says so.
  CHECK(contains(cpp, "= ps_task_spawn(rt, ps_t"));
  CHECK(contains(cpp, "= ps_task_wait("));
  CHECK(contains(
      emit.err,
      "lowering: spawn of /bump runs inline: a parameter or the result is not a scalar a real "
      "call takes\n"));
  CHECK_FALSE(contains(emit.err, "/twice"));

  const std::filesystem::path binary = scratch("spawn_scalar.bin");
  const ProgramRun build = runCommand("./primec --emit=optexe '" + sourcePath.string() +
                                          "' --entry /main -o '" + binary.string() + "'",
                                      "spawn_scalar_build");
  REQUIRE_MESSAGE(build.exitCode == 0, build.err);
  const ProgramRun actual = runCommand("'" + binary.string() + "'", "spawn_scalar_run");
  CHECK(actual.exitCode == 0);
  CHECK(actual.out == "42\n5\n");
  CHECK(actual.err.empty());
}

TEST_SUITE_END();
