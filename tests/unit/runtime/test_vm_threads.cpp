#include "primec/ir/Ir.h"
#include "primec/runtime/Vm.h"
#include "primec/testing/VmKernelSelection.h"

#include "third_party/doctest.h"

#include <cstdint>
#include <map>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

using namespace primec;

// The VM's threads mode (docs/spec/vm-design.md, Tasks on threads; --vm-threads) with hand-built IR. Nothing here
// sleeps or measures time: every case compares results and output, or ends because the run joined its threads.
TEST_SUITE_BEGIN("primestruct.runtime.vm_threads");

namespace {

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
  module.functions = std::move(functions);
  module.entryIndex = 0;
  return module;
}

struct Run {
  bool ok = false;
  uint64_t raw = 0;
  int32_t value = 0;
  std::string error;
  std::string out;
};

// The sink is not synchronized: the run's one host lock is what keeps concurrent prints from corrupting it.
void appendOutput(int fd, std::string_view chunk, void *userData) {
  if (fd == 1) {
    static_cast<std::string *>(userData)->append(chunk);
  }
}

Run run(const IrModule &module, bool threads) {
  Run result;
  Vm vm;
  vm.setOutputSink({&appendOutput, &result.out});
  vm.setTaskThreads(threads);
  result.ok = vm.execute(module, result.raw, result.error, std::vector<std::string_view>{"prog"});
  result.value = static_cast<int32_t>(result.raw);
  return result;
}

constexpr uint64_t Newline = PrintFlagNewline;
constexpr int Repeats = 20;

// sum_to(n): the sum of 0 .. n-1 as an i64.
IrFunction sumTo() {
  return functionOf("/sum_to",
                    1,
                    {{IrOpcode::StoreLocal, 0}, // n
                     {IrOpcode::PushI64, 0},
                     {IrOpcode::StoreLocal, 1}, // total
                     {IrOpcode::PushI64, 0},
                     {IrOpcode::StoreLocal, 2}, // i
                     // 5: loop head
                     {IrOpcode::LoadLocal, 2},
                     {IrOpcode::LoadLocal, 0},
                     {IrOpcode::CmpLtI64, 0},
                     {IrOpcode::JumpIfZero, 18},
                     {IrOpcode::LoadLocal, 1},
                     {IrOpcode::LoadLocal, 2},
                     {IrOpcode::AddI64, 0},
                     {IrOpcode::StoreLocal, 1},
                     {IrOpcode::LoadLocal, 2},
                     {IrOpcode::PushI64, 1},
                     {IrOpcode::AddI64, 0},
                     {IrOpcode::StoreLocal, 2},
                     {IrOpcode::Jump, 5},
                     // 18
                     {IrOpcode::LoadLocal, 1},
                     {IrOpcode::ReturnI64, 0}});
}

// main: spawns sum_to(1000), sum_to(2000), sum_to(3000) and sum_to(4000), waits them in order, prints each result and
// returns the total modulo 2^31.
IrModule fourSums() {
  std::vector<IrInstruction> main;
  for (int64_t n : {1000, 2000, 3000, 4000}) {
    main.push_back({IrOpcode::PushI64, static_cast<uint64_t>(n)});
    main.push_back({IrOpcode::TaskSpawn, 1});
  }
  // The handles are on the stack, the last spawned on top: store them, then wait in spawn order.
  for (uint64_t local = 4; local-- > 0;) {
    main.push_back({IrOpcode::StoreLocal, local});
  }
  main.push_back({IrOpcode::PushI64, 0});
  main.push_back({IrOpcode::StoreLocal, 4}); // total
  for (uint64_t local = 0; local < 4; ++local) {
    main.push_back({IrOpcode::LoadLocal, local});
    main.push_back({IrOpcode::TaskWait, 0});
    main.push_back({IrOpcode::Dup, 0});
    main.push_back({IrOpcode::PrintI64, Newline});
    main.push_back({IrOpcode::LoadLocal, 4});
    main.push_back({IrOpcode::AddI64, 0});
    main.push_back({IrOpcode::StoreLocal, 4});
  }
  main.push_back({IrOpcode::LoadLocal, 4});
  main.push_back({IrOpcode::ReturnI64, 0});
  return moduleOf({functionOf("/main", 0, std::move(main)), sumTo()});
}

} // namespace

TEST_CASE("four scalar tasks print what the default schedule prints") {
  const IrModule module = fourSums();
  const Run expected = run(module, false);
  REQUIRE_MESSAGE(expected.ok, expected.error);
  // 499500 + 1999000 + 4498500 + 7998000, each result on its line in wait order.
  CHECK(expected.out == "499500\n1999000\n4498500\n7998000\n");
  for (int repeat = 0; repeat < Repeats; ++repeat) {
    CAPTURE(repeat);
    const Run threaded = run(module, true);
    REQUIRE_MESSAGE(threaded.ok, threaded.error);
    CHECK(threaded.raw == expected.raw);
    CHECK(threaded.out == expected.out);
  }
}

TEST_CASE("tasks spawned and waited in a loop reuse their slots and arenas") {
  // The sum of double(i) for i in 0..999, each task spawned and waited in the loop body.
  const IrModule module = moduleOf({functionOf("/main",
                                               0,
                                               {{IrOpcode::PushI32, 0},
                                                {IrOpcode::StoreLocal, 0}, // i
                                                {IrOpcode::PushI64, 0},
                                                {IrOpcode::StoreLocal, 1}, // total
                                                // 4: loop head
                                                {IrOpcode::LoadLocal, 0},
                                                {IrOpcode::PushI32, 1000},
                                                {IrOpcode::CmpLtI32, 0},
                                                {IrOpcode::JumpIfZero, 19},
                                                {IrOpcode::LoadLocal, 0},
                                                {IrOpcode::TaskSpawn, 1},
                                                {IrOpcode::TaskWait, 0},
                                                {IrOpcode::LoadLocal, 1},
                                                {IrOpcode::AddI64, 0},
                                                {IrOpcode::StoreLocal, 1},
                                                {IrOpcode::LoadLocal, 0},
                                                {IrOpcode::PushI32, 1},
                                                {IrOpcode::AddI32, 0},
                                                {IrOpcode::StoreLocal, 0},
                                                {IrOpcode::Jump, 4},
                                                // 19
                                                {IrOpcode::LoadLocal, 1},
                                                {IrOpcode::ReturnI64, 0}}),
                                    functionOf("/double",
                                               1,
                                               {{IrOpcode::StoreLocal, 0},
                                                {IrOpcode::LoadLocal, 0},
                                                {IrOpcode::PushI32, 2},
                                                {IrOpcode::MulI32, 0},
                                                {IrOpcode::ReturnI32, 0}})});
  const Run expected = run(module, false);
  REQUIRE_MESSAGE(expected.ok, expected.error);
  CHECK(expected.raw == 999000); // 2 * (999 * 1000 / 2)
  const Run threaded = run(module, true);
  REQUIRE_MESSAGE(threaded.ok, threaded.error);
  CHECK(threaded.raw == expected.raw);
}

TEST_CASE("a task spawns and waits a task of its own") {
  const IrModule module = moduleOf({functionOf("/main",
                                               0,
                                               {{IrOpcode::PushI64, 3000},
                                                {IrOpcode::TaskSpawn, 1},
                                                {IrOpcode::TaskWait, 0},
                                                {IrOpcode::ReturnI64, 0}}),
                                    functionOf("/outer",
                                               1,
                                               {{IrOpcode::StoreLocal, 0},
                                                {IrOpcode::LoadLocal, 0},
                                                {IrOpcode::TaskSpawn, 2},
                                                {IrOpcode::TaskWait, 0},
                                                {IrOpcode::PushI64, 1},
                                                {IrOpcode::AddI64, 0},
                                                {IrOpcode::ReturnI64, 0}}),
                                    sumTo()});
  for (int repeat = 0; repeat < Repeats; ++repeat) {
    const Run threaded = run(module, true);
    REQUIRE_MESSAGE(threaded.ok, threaded.error);
    CHECK(threaded.raw == 4498501);
  }
}

TEST_CASE("a block a task allocates is in a non-zero arena and the waiter frees it") {
  // /make allocates four slots and returns the address; main waits it, frees it (a block freed by a thread that is
  // not its owner) and returns the address.
  const IrModule module = moduleOf(
      {functionOf("/main",
                  0,
                  {{IrOpcode::TaskSpawn, 1},
                   {IrOpcode::TaskWait, 0},
                   {IrOpcode::Dup, 0},
                   {IrOpcode::HeapFree, 0},
                   {IrOpcode::ReturnI64, 0}}),
       functionOf("/make",
                  0,
                  {{IrOpcode::PushI64, 4}, {IrOpcode::HeapAlloc, 0}, {IrOpcode::ReturnI64, 0}})});
  const Run serial = run(module, false);
  REQUIRE_MESSAGE(serial.ok, serial.error);
  // Address bits 41 to 46 are the arena number: the serial VM has one arena.
  CHECK(((serial.raw >> 41) & 63) == 0);
  for (int repeat = 0; repeat < Repeats; ++repeat) {
    const Run threaded = run(module, true);
    REQUIRE_MESSAGE(threaded.ok, threaded.error);
    CHECK(((threaded.raw >> 41) & 63) != 0);
  }
}

TEST_CASE("a single-task program keeps its addresses in threads mode") {
  // The main task allocates before and after a task ran: arena 0 either way, so the same addresses as the serial VM.
  const IrModule module =
      moduleOf({functionOf("/main",
                           0,
                           {{IrOpcode::PushI64, 3},
                            {IrOpcode::HeapAlloc, 0},
                            {IrOpcode::StoreLocal, 0},
                            {IrOpcode::PushI64, 5},
                            {IrOpcode::HeapAlloc, 0},
                            {IrOpcode::StoreLocal, 1},
                            {IrOpcode::TaskSpawn, 1},
                            {IrOpcode::TaskWait, 0},
                            {IrOpcode::Pop, 0},
                            {IrOpcode::LoadLocal, 0},
                            {IrOpcode::HeapFree, 0},
                            {IrOpcode::PushI64, 3},
                            {IrOpcode::HeapAlloc, 0},
                            {IrOpcode::ReturnI64, 0}}),
                functionOf("/noop", 0, {{IrOpcode::PushI64, 0}, {IrOpcode::ReturnI64, 0}})});
  const Run serial = run(module, false);
  REQUIRE_MESSAGE(serial.ok, serial.error);
  const Run threaded = run(module, true);
  REQUIRE_MESSAGE(threaded.ok, threaded.error);
  CHECK(((threaded.raw >> 41) & 63) == 0);
  CHECK(threaded.raw == serial.raw);

  // A module without tasks never creates a second arena.
  const IrModule single = moduleOf({functionOf(
      "/main", 0, {{IrOpcode::PushI64, 7}, {IrOpcode::HeapAlloc, 0}, {IrOpcode::ReturnI64, 0}})});
  const Run singleSerial = run(single, false);
  const Run singleThreaded = run(single, true);
  REQUIRE_MESSAGE(singleThreaded.ok, singleThreaded.error);
  CHECK(singleThreaded.raw == singleSerial.raw);
}

TEST_CASE("prints of tasks that run together never mix and all arrive") {
  // Four tasks print 200 lines of their own number each; main waits them all.
  const IrFunction printer = functionOf("/printer",
                                        1,
                                        {{IrOpcode::StoreLocal, 0},
                                         {IrOpcode::PushI32, 0},
                                         {IrOpcode::StoreLocal, 1},
                                         // 3: loop head
                                         {IrOpcode::LoadLocal, 1},
                                         {IrOpcode::PushI32, 200},
                                         {IrOpcode::CmpLtI32, 0},
                                         {IrOpcode::JumpIfZero, 14},
                                         {IrOpcode::LoadLocal, 0},
                                         {IrOpcode::PrintI32, Newline},
                                         {IrOpcode::LoadLocal, 1},
                                         {IrOpcode::PushI32, 1},
                                         {IrOpcode::AddI32, 0},
                                         {IrOpcode::StoreLocal, 1},
                                         {IrOpcode::Jump, 3},
                                         // 14
                                         {IrOpcode::ReturnVoid, 0}});
  std::vector<IrInstruction> main;
  for (int32_t id : {1111, 2222, 3333, 4444}) {
    main.push_back({IrOpcode::PushI32, static_cast<uint64_t>(id)});
    main.push_back({IrOpcode::TaskSpawn, 1});
  }
  for (int task = 0; task < 4; ++task) {
    main.push_back({IrOpcode::TaskWait, 0});
    main.push_back({IrOpcode::Pop, 0});
  }
  main.push_back({IrOpcode::PushI32, 0});
  main.push_back({IrOpcode::ReturnI32, 0});
  const IrModule module = moduleOf({functionOf("/main", 0, std::move(main)), printer});
  for (int repeat = 0; repeat < Repeats; ++repeat) {
    CAPTURE(repeat);
    const Run threaded = run(module, true);
    REQUIRE_MESSAGE(threaded.ok, threaded.error);
    std::map<std::string, int> lines;
    std::istringstream stream(threaded.out);
    for (std::string line; std::getline(stream, line);) {
      ++lines[line];
    }
    CHECK(lines.size() == 4);
    for (const char *id : {"1111", "2222", "3333", "4444"}) {
      CHECK(lines[id] == 200);
    }
  }
}

TEST_CASE("a fault in a task ends the run and the waiter does not hang") {
  const IrModule waiting = moduleOf(
      {functionOf("/main",
                  0,
                  {{IrOpcode::TaskSpawn, 1}, {IrOpcode::TaskWait, 0}, {IrOpcode::ReturnI32, 0}}),
       functionOf("/fails",
                  0,
                  {{IrOpcode::PushI32, 1},
                   {IrOpcode::PushI32, 0},
                   {IrOpcode::DivI32, 0},
                   {IrOpcode::ReturnI32, 0}})});
  // main never waits and spins forever: the stop flag is what ends it.
  const IrModule spinning =
      moduleOf({functionOf("/main", 0, {{IrOpcode::TaskSpawn, 1}, {IrOpcode::Jump, 1}}),
                functionOf("/fails",
                           0,
                           {{IrOpcode::PushI32, 1},
                            {IrOpcode::PushI32, 0},
                            {IrOpcode::DivI32, 0},
                            {IrOpcode::ReturnI32, 0}})});
  for (const IrModule *module : {&waiting, &spinning}) {
    for (int repeat = 0; repeat < Repeats; ++repeat) {
      const Run threaded = run(*module, true);
      CHECK_FALSE(threaded.ok);
      CHECK(threaded.error == "division by zero in IR");
    }
  }
}

TEST_CASE("a fault in the main task stops tasks that still run") {
  // The task spins forever; main divides by zero.
  const IrModule module = moduleOf({functionOf("/main",
                                               0,
                                               {{IrOpcode::TaskSpawn, 1},
                                                {IrOpcode::Pop, 0},
                                                {IrOpcode::PushI32, 1},
                                                {IrOpcode::PushI32, 0},
                                                {IrOpcode::DivI32, 0},
                                                {IrOpcode::ReturnI32, 0}}),
                                    functionOf("/spin", 0, {{IrOpcode::Jump, 0}})});
  for (int repeat = 0; repeat < Repeats; ++repeat) {
    const Run threaded = run(module, true);
    CHECK_FALSE(threaded.ok);
    CHECK(threaded.error == "division by zero in IR");
  }
}

TEST_CASE("a task nobody waits for is stopped when the main task returns") {
  const IrModule module = moduleOf({functionOf("/main",
                                               0,
                                               {{IrOpcode::TaskSpawn, 1},
                                                {IrOpcode::Pop, 0},
                                                {IrOpcode::PushI32, 7},
                                                {IrOpcode::ReturnI32, 0}}),
                                    functionOf("/spin", 0, {{IrOpcode::Jump, 0}})});
  for (int repeat = 0; repeat < Repeats; ++repeat) {
    const Run threaded = run(module, true);
    REQUIRE_MESSAGE(threaded.ok, threaded.error);
    CHECK(threaded.value == 7);
  }
}

TEST_CASE("a trap in a task ends the program with its code") {
  const IrModule module = moduleOf(
      {functionOf("/main",
                  0,
                  {{IrOpcode::TaskSpawn, 1}, {IrOpcode::TaskWait, 0}, {IrOpcode::ReturnI32, 0}}),
       functionOf("/fails", 0, {{IrOpcode::Trap, 3}})});
  for (int repeat = 0; repeat < Repeats; ++repeat) {
    const Run threaded = run(module, true);
    REQUIRE_MESSAGE(threaded.ok, threaded.error);
    CHECK(threaded.value == 3);
  }
}

TEST_CASE("handle faults read as in the default schedule") {
  const IrModule twice = moduleOf({functionOf("/main",
                                              0,
                                              {{IrOpcode::PushI64, 5},
                                               {IrOpcode::TaskSpawn, 1},
                                               {IrOpcode::Dup, 0},
                                               {IrOpcode::TaskWait, 0},
                                               {IrOpcode::Pop, 0},
                                               {IrOpcode::TaskWait, 0},
                                               {IrOpcode::ReturnI32, 0}}),
                                   sumTo()});
  const IrModule notHandle = moduleOf({functionOf(
      "/main", 0, {{IrOpcode::PushI32, 7}, {IrOpcode::TaskWait, 0}, {IrOpcode::ReturnI32, 0}})});
  const IrModule badFunction =
      moduleOf({functionOf("/main", 0, {{IrOpcode::TaskSpawn, 9}, {IrOpcode::ReturnI32, 0}})});
  for (const IrModule *module : {&twice, &notHandle, &badFunction}) {
    const Run serial = run(*module, false);
    const Run threaded = run(*module, true);
    CHECK_FALSE(serial.ok);
    CHECK_FALSE(threaded.ok);
    CHECK(threaded.error == serial.error);
  }
}

namespace {

// Turns the flat loop on or off for a scope and restores it.
struct FlatLoopScope {
  explicit FlatLoopScope(bool enabled) {
    testing::setVmFastKernelEnabled(enabled);
  }
  ~FlatLoopScope() {
    testing::setVmFastKernelEnabled(true);
  }
};

// main spawns /worker twice (in order) and adds the results. /worker takes and releases a mutex and adds to an
// atomic, both through the address of its own locals (the flat loop numbers a task's locals in one arena), calls
// /bump twice, and returns the atomic's total.
IrModule localSyncTasks() {
  return moduleOf(
      {functionOf("/main",
                  0,
                  {{IrOpcode::PushI32, 5},
                   {IrOpcode::TaskSpawn, 1},
                   {IrOpcode::PushI32, 6},
                   {IrOpcode::TaskSpawn, 1},
                   {IrOpcode::TaskWait, 0},
                   {IrOpcode::StoreLocal, 0},
                   {IrOpcode::TaskWait, 0},
                   {IrOpcode::LoadLocal, 0},
                   {IrOpcode::AddI32, 0},
                   {IrOpcode::ReturnI32, 0}}),
       functionOf("/worker", 1, {{IrOpcode::StoreLocal, 0}, // n
                                 {IrOpcode::PushI64, 0},
                                 {IrOpcode::StoreLocal, 1}, // lock
                                 {IrOpcode::PushI64, 0},
                                 {IrOpcode::StoreLocal, 2}, // counter
                                 {IrOpcode::AddressOfLocal, 1},
                                 {IrOpcode::MutexLock, 0},
                                 {IrOpcode::AddressOfLocal, 2},
                                 {IrOpcode::LoadLocal, 0},
                                 {IrOpcode::AtomicAdd, 64},
                                 {IrOpcode::Pop, 0},
                                 {IrOpcode::AddressOfLocal, 1},
                                 {IrOpcode::MutexUnlock, 0},
                                 {IrOpcode::AddressOfLocal, 2},
                                 {IrOpcode::Call, 2},
                                 {IrOpcode::Pop, 0},
                                 {IrOpcode::AddressOfLocal, 2},
                                 {IrOpcode::Call, 2},
                                 {IrOpcode::Pop, 0},
                                 {IrOpcode::AddressOfLocal, 2},
                                 {IrOpcode::AtomicLoad, 64},
                                 {IrOpcode::ReturnI32, 0}}),
       // /bump(address): adds 100 to the caller's local through the address, in a counted loop.
       functionOf("/bump",
                  1,
                  {{IrOpcode::StoreLocal, 0}, // address
                   {IrOpcode::PushI32, 0},
                   {IrOpcode::StoreLocal, 1}, // i
                   // 3: loop head
                   {IrOpcode::LoadLocal, 1},
                   {IrOpcode::PushI32, 3},
                   {IrOpcode::CmpLtI32, 0},
                   {IrOpcode::JumpIfZero, 16},
                   {IrOpcode::LoadLocal, 0},
                   {IrOpcode::PushI64, 100},
                   {IrOpcode::AtomicAdd, 64},
                   {IrOpcode::Pop, 0},
                   {IrOpcode::LoadLocal, 1},
                   {IrOpcode::PushI32, 1},
                   {IrOpcode::AddI32, 0},
                   {IrOpcode::StoreLocal, 1},
                   {IrOpcode::Jump, 3},
                   {IrOpcode::PushI32, 0},
                   {IrOpcode::ReturnI32, 0}})});
}

} // namespace

TEST_CASE("threads mode runs its tasks on the flat loop and agrees with the step kernel") {
  const std::vector<IrModule> modules = {fourSums(), localSyncTasks()};
  for (size_t index = 0; index < modules.size(); ++index) {
    CAPTURE(index);
    for (int repeat = 0; repeat < Repeats; ++repeat) {
      Run step;
      {
        const FlatLoopScope off(false);
        const uint64_t before = testing::vmFastTaskRunCount();
        step = run(modules[index], true);
        CHECK(testing::vmFastTaskRunCount() == before);
      }
      const uint64_t before = testing::vmFastTaskRunCount();
      const Run fast = run(modules[index], true);
      CHECK(testing::vmFastTaskRunCount() == before + 1);
      REQUIRE_MESSAGE(step.ok, step.error);
      REQUIRE_MESSAGE(fast.ok, fast.error);
      CHECK(fast.raw == step.raw);
      CHECK(fast.out == step.out);
    }
  }
  // Each worker adds n, then six times 100 in /bump (two calls of three rounds), so 5 + 600 and 6 + 600.
  const Run local = run(localSyncTasks(), true);
  REQUIRE_MESSAGE(local.ok, local.error);
  CHECK(local.value == 1211);
}

TEST_CASE("atomic, mutex and condition opcodes run in threads mode") {
  const IrModule atomic = moduleOf({functionOf("/main",
                                               0,
                                               {{IrOpcode::PushI64, 2},
                                                {IrOpcode::HeapAlloc, 0},
                                                {IrOpcode::PushI32, 1},
                                                {IrOpcode::AtomicAdd, 32},
                                                {IrOpcode::ReturnI32, 0}})});
  const IrModule mutex = moduleOf({functionOf("/main",
                                              0,
                                              {{IrOpcode::PushI64, 2},
                                               {IrOpcode::HeapAlloc, 0},
                                               {IrOpcode::MutexLock, 0},
                                               {IrOpcode::PushI32, 0},
                                               {IrOpcode::ReturnI32, 0}})});
  const IrModule condition = moduleOf({functionOf("/main",
                                                  0,
                                                  {{IrOpcode::PushI64, 2},
                                                   {IrOpcode::HeapAlloc, 0},
                                                   {IrOpcode::ConditionNotify, 0},
                                                   {IrOpcode::PushI32, 0},
                                                   {IrOpcode::ReturnI32, 0}})});
  for (const IrModule *module : {&atomic, &mutex, &condition}) {
    const Run threaded = run(*module, true);
    const Run serial = run(*module, false);
    CHECK(threaded.ok == serial.ok);
    CHECK(threaded.error == serial.error);
    CHECK(threaded.raw == serial.raw);
  }
}

TEST_SUITE_END();
