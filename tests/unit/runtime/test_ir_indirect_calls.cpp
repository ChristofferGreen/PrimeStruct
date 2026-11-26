#include "ir_link_test_support.h"

#include "primec/ir/Ir.h"
#include "primec/ir/IrCfg.h"
#include "primec/ir/IrIndirectCallExpansion.h"
#include "primec/ir/IrLinker.h"
#include "primec/ir/IrOperandTable.h"
#include "primec/ir/IrSerializer.h"
#include "primec/ir/IrValidation.h"

#include "third_party/doctest.h"

#include <string>
#include <vector>

using namespace primec;
using namespace ir_link_test;

// The function address opcodes (docs/spec/vm-design.md, Function addresses): FunctionAddress pushes `index + 1`, and
// CallIndirect takes that address as the deepest of its parameters' slots. Targets without them run the module
// through expandIndirectCalls, which these tests execute on the VM.
TEST_SUITE_BEGIN("primestruct.runtime.ir_indirect_calls");

namespace {

IrFunction doubler() {
  return functionOf("/double",
                    1,
                    {{IrOpcode::StoreLocal, 0},
                     {IrOpcode::LoadLocal, 0},
                     {IrOpcode::LoadLocal, 0},
                     {IrOpcode::AddI32, 0},
                     {IrOpcode::ReturnI32, 0}});
}

IrFunction addHundred() {
  return functionOf("/add_hundred",
                    1,
                    {{IrOpcode::StoreLocal, 0},
                     {IrOpcode::LoadLocal, 0},
                     {IrOpcode::PushI32, 100},
                     {IrOpcode::AddI32, 0},
                     {IrOpcode::ReturnI32, 0}});
}

// The entry picks one of the two functions by the argument count (1 under `run`), so the address is not a constant
// of the callee's code: argc 1 selects /double and gives 10.
IrModule chosenAtRunTime() {
  return moduleOf({functionOf("/main",
                              0,
                              {{IrOpcode::PushArgc, 0},
                               {IrOpcode::JumpIfZero, 4},
                               {IrOpcode::FunctionAddress, 1},
                               {IrOpcode::Jump, 5},
                               {IrOpcode::FunctionAddress, 2},
                               {IrOpcode::PushI32, 5},
                               {IrOpcode::CallIndirect, 1},
                               {IrOpcode::ReturnI32, 0}}),
                   doubler(),
                   addHundred()});
}

IrModule roundTrip(const IrModule &module) {
  std::vector<uint8_t> bytes;
  std::string error;
  REQUIRE_MESSAGE(serializeIr(module, bytes, error), error);
  IrModule back;
  REQUIRE_MESSAGE(deserializeIr(bytes, back, error), error);
  return back;
}

} // namespace

TEST_CASE("indirect calls: the opcodes have stack effects and operand classes") {
  const IrModule module = chosenAtRunTime();
  IrStackEffect effect;
  REQUIRE(computeIrStackEffect({IrOpcode::FunctionAddress, 1}, module, effect));
  CHECK(effect.pops == 0);
  CHECK(effect.pushes == 1);
  REQUIRE(computeIrStackEffect({IrOpcode::CallIndirect, 3}, module, effect));
  CHECK(effect.pops == 4);
  CHECK(effect.pushes == 1);
  REQUIRE(computeIrStackEffect({IrOpcode::CallIndirectVoid, 0}, module, effect));
  CHECK(effect.pops == 1);
  CHECK(effect.pushes == 0);
  CHECK(irOperandClass(IrOpcode::FunctionAddress) == IrOperandClass::Function);
  CHECK(irOperandClass(IrOpcode::CallIndirect) == IrOperandClass::ParameterCount);
  CHECK(irOperandClass(IrOpcode::CallIndirectVoid) == IrOperandClass::ParameterCount);
}

TEST_CASE("indirect calls: a module with the opcodes round-trips and validates as bytecode") {
  const IrModule module = chosenAtRunTime();
  std::string error;
  REQUIRE_MESSAGE(validateIrModule(module, IrValidationTarget::Serialized, error), error);
  const IrModule back = roundTrip(module);
  CHECK(back.schemaVersion == IrSchemaVersion);
  CHECK(IrSchemaVersion == 42u);
  REQUIRE(back.functions[0].instructions.size() == module.functions[0].instructions.size());
  CHECK(back.functions[0].instructions[2].op == IrOpcode::FunctionAddress);
  CHECK(back.functions[0].instructions[2].imm == 1);
  CHECK(back.functions[0].instructions[6].op == IrOpcode::CallIndirect);
  CHECK(back.functions[0].instructions[6].imm == 1);
  CHECK(irModuleHasIndirectCalls(back));
}

TEST_CASE("indirect calls: a version 41 module cannot hold the opcodes") {
  IrModule module = chosenAtRunTime();
  std::vector<uint8_t> bytes;
  std::string error;
  REQUIRE_MESSAGE(serializeIr(module, bytes, error), error);
  // The schema version is the second u32 of the file; claim an older writer wrote it.
  REQUIRE(bytes.size() > 8);
  bytes[4] = 41;
  IrModule back;
  CHECK_FALSE(deserializeIr(bytes, back, error));
  CHECK(error == "unsupported IR opcode");
}

TEST_CASE("indirect calls: a target that does not run them refuses the module until it is expanded") {
  IrModule module = chosenAtRunTime();
  std::string error;
  CHECK_FALSE(validateIrModule(module, IrValidationTarget::Native, error));
  CHECK(error.find("function address opcodes") != std::string::npos);
  CHECK_FALSE(validateIrModule(module, IrValidationTarget::Cpp, error));
  CHECK_FALSE(validateIrModule(module, IrValidationTarget::Wasm, error));
  // The VM, the optimized C++ emitters and serialized bytecode run them.
  REQUIRE_MESSAGE(validateIrModule(module, IrValidationTarget::Vm, error), error);
  REQUIRE_MESSAGE(validateIrModule(module, IrValidationTarget::OptCpp, error), error);
  REQUIRE_MESSAGE(validateIrModule(module, IrValidationTarget::Serialized, error), error);
  expandIndirectCalls(module);
  CHECK_FALSE(irModuleHasIndirectCalls(module));
  REQUIRE_MESSAGE(validateIrModule(module, IrValidationTarget::Native, error), error);
  REQUIRE_MESSAGE(validateIrModule(module, IrValidationTarget::Cpp, error), error);
}

TEST_CASE("indirect calls: the VM calls the function the address names") {
  const Run result = run(chosenAtRunTime());
  REQUIRE_MESSAGE(result.ok, result.error);
  CHECK(result.value == 10);
}

TEST_CASE("indirect calls: the VM and the expansion agree on the other address and on void calls") {
  IrModule module = chosenAtRunTime();
  module.functions[0].instructions[0] = {IrOpcode::PushI32, 0};
  const Run direct = run(module);
  REQUIRE_MESSAGE(direct.ok, direct.error);
  CHECK(direct.value == 105);
  IrModule voidModule = moduleOf(
      {functionOf("/main",
                  0,
                  {{IrOpcode::FunctionAddress, 1},
                   {IrOpcode::CallIndirectVoid, 0},
                   {IrOpcode::FunctionAddress, 2},
                   {IrOpcode::CallIndirect, 0},
                   {IrOpcode::ReturnI32, 0}}),
       functionOf("/say", 0, {{IrOpcode::PushI32, 7}, {IrOpcode::PrintI32, Newline}, {IrOpcode::ReturnVoid, 0}}),
       functionOf("/nine", 0, {{IrOpcode::PushI32, 9}, {IrOpcode::ReturnI32, 0}})});
  const Run voidDirect = run(voidModule);
  REQUIRE_MESSAGE(voidDirect.ok, voidDirect.error);
  CHECK(voidDirect.out == "7\n");
  CHECK(voidDirect.value == 9);
  expandIndirectCalls(module);
  const Run expanded = run(module);
  REQUIRE_MESSAGE(expanded.ok, expanded.error);
  CHECK(expanded.value == direct.value);
}

TEST_CASE("indirect calls: the VM faults on a null address, an address past the table and the wrong arity") {
  IrModule module = chosenAtRunTime();
  const auto runWith = [&](std::vector<IrInstruction> instructions) {
    module.functions[0].instructions = std::move(instructions);
    return run(module);
  };
  Run result = runWith({{IrOpcode::PushI64, 0}, {IrOpcode::PushI32, 5}, {IrOpcode::CallIndirect, 1}, {IrOpcode::ReturnI32, 0}});
  CHECK_FALSE(result.ok);
  CHECK(result.error == "indirect call through a null function address");
  result = runWith({{IrOpcode::PushI64, 4}, {IrOpcode::PushI32, 5}, {IrOpcode::CallIndirect, 1}, {IrOpcode::ReturnI32, 0}});
  CHECK_FALSE(result.ok);
  CHECK(result.error == "indirect call through an invalid function address");
  result = runWith({{IrOpcode::FunctionAddress, 1},
                    {IrOpcode::PushI32, 5},
                    {IrOpcode::PushI32, 6},
                    {IrOpcode::CallIndirect, 2},
                    {IrOpcode::ReturnI32, 0}});
  CHECK_FALSE(result.ok);
  CHECK(result.error == "indirect call passes 2 parameters to /double, which takes 1");
}

TEST_CASE("indirect calls: a task calls through an address") {
  IrModule module = moduleOf({functionOf("/main",
                                         0,
                                         {{IrOpcode::TaskSpawn, 1}, {IrOpcode::TaskWait, 0}, {IrOpcode::ReturnI32, 0}}),
                              functionOf("/worker",
                                         0,
                                         {{IrOpcode::FunctionAddress, 2},
                                          {IrOpcode::PushI32, 21},
                                          {IrOpcode::CallIndirect, 1},
                                          {IrOpcode::ReturnI32, 0}}),
                              doubler()});
  const Run result = run(module);
  REQUIRE_MESSAGE(result.ok, result.error);
  CHECK(result.value == 42);
}

TEST_CASE("indirect calls: the expansion calls the function the address names") {
  IrModule module = chosenAtRunTime();
  expandIndirectCalls(module);
  const Run result = run(module);
  REQUIRE_MESSAGE(result.ok, result.error);
  CHECK(result.value == 10);
}

TEST_CASE("indirect calls: the other address reaches the other function and jumps keep their targets") {
  IrModule module = chosenAtRunTime();
  // PushArgc becomes 0: the entry takes the second function (5 + 100).
  module.functions[0].instructions[0] = {IrOpcode::PushI32, 0};
  expandIndirectCalls(module);
  const Run result = run(module);
  REQUIRE_MESSAGE(result.ok, result.error);
  CHECK(result.value == 105);
}

TEST_CASE("indirect calls: a void call and a call with no parameters") {
  IrModule module = moduleOf({functionOf("/main",
                                         0,
                                         {{IrOpcode::FunctionAddress, 1},
                                          {IrOpcode::CallIndirectVoid, 0},
                                          {IrOpcode::FunctionAddress, 2},
                                          {IrOpcode::CallIndirect, 0},
                                          {IrOpcode::ReturnI32, 0}}),
                              functionOf("/say", 0, {{IrOpcode::PushI32, 7}, {IrOpcode::PrintI32, Newline}, {IrOpcode::ReturnVoid, 0}}),
                              functionOf("/nine", 0, {{IrOpcode::PushI32, 9}, {IrOpcode::ReturnI32, 0}})});
  expandIndirectCalls(module);
  const Run result = run(module);
  REQUIRE_MESSAGE(result.ok, result.error);
  CHECK(result.out == "7\n");
  CHECK(result.value == 9);
}

TEST_CASE("indirect calls: a null address ends the program as a runtime error does") {
  IrModule module = chosenAtRunTime();
  module.functions[0].instructions = {{IrOpcode::PushI64, 0},
                                      {IrOpcode::PushI32, 5},
                                      {IrOpcode::CallIndirect, 1},
                                      {IrOpcode::ReturnI32, 0}};
  expandIndirectCalls(module);
  const Run result = run(module);
  REQUIRE_MESSAGE(result.ok, result.error);
  CHECK(result.value == 3);
}

TEST_CASE("indirect calls: a function that takes another number of parameters is not a target") {
  IrModule module = chosenAtRunTime();
  // /double and /add_hundred take one parameter; the call passes two, so no function matches.
  module.functions[0].instructions = {{IrOpcode::FunctionAddress, 1},
                                      {IrOpcode::PushI32, 5},
                                      {IrOpcode::PushI32, 6},
                                      {IrOpcode::CallIndirect, 2},
                                      {IrOpcode::ReturnI32, 0}};
  expandIndirectCalls(module);
  const Run result = run(module);
  REQUIRE_MESSAGE(result.ok, result.error);
  CHECK(result.value == 3);
}

TEST_CASE("indirect calls: the link renumbers an address a library takes") {
  // The library gives out the address of its own function; the program calls through it.
  IrModule program = moduleOf({functionOf("/main",
                                          0,
                                          {{IrOpcode::Call, 1},
                                           {IrOpcode::PushI32, 4},
                                           {IrOpcode::CallIndirect, 1},
                                           {IrOpcode::ReturnI32, 0}}),
                               importOf("/lib/increment_address", 0, "lib")});
  const IrModule library =
      moduleOf({functionOf("/lib/increment_address", 0, {{IrOpcode::FunctionAddress, 2}, {IrOpcode::ReturnI64, 0}}),
                functionOf("/lib/unused", 0, {{IrOpcode::PushI32, 0}, {IrOpcode::ReturnI32, 0}}),
                functionOf("/lib/increment",
                           1,
                           {{IrOpcode::StoreLocal, 0},
                            {IrOpcode::LoadLocal, 0},
                            {IrOpcode::PushI32, 1},
                            {IrOpcode::AddI32, 0},
                            {IrOpcode::ReturnI32, 0}})},
               -1);
  std::string error;
  REQUIRE_MESSAGE(linkIrModule(program, roundTrip(library), error), error);
  REQUIRE_MESSAGE(checkIrImportsResolved(program, error), error);
  // The address names /lib/increment, which the library table keeps at index 2 and the merged module at index 3.
  REQUIRE(program.functions.size() == 4);
  CHECK(program.functions[3].name == "/lib/increment");
  CHECK(program.functions[1].instructions[0].op == IrOpcode::FunctionAddress);
  CHECK(program.functions[1].instructions[0].imm == 3);
  // The walk that decides which library functions a link takes follows the address, so a function that only an
  // address names is not dropped.
  const IrModule consumer = moduleOf({functionOf("/main", 0, {{IrOpcode::Call, 1}, {IrOpcode::ReturnI32, 0}}),
                                      importOf("/lib/increment_address", 0, "lib")});
  const IrModule libraryAgain =
      moduleOf({functionOf("/lib/increment_address", 0, {{IrOpcode::FunctionAddress, 2}, {IrOpcode::ReturnI64, 0}}),
                functionOf("/lib/unused", 0, {{IrOpcode::PushI32, 0}, {IrOpcode::ReturnI32, 0}}),
                functionOf("/lib/increment", 1, {{IrOpcode::ReturnI32, 0}})},
               -1);
  const auto reached = reachableLibraryFunctionNames(consumer, {&libraryAgain});
  CHECK(reached.count("/lib/increment") == 1);
  CHECK(reached.count("/lib/unused") == 0);
  REQUIRE_MESSAGE(validateIrModule(program, IrValidationTarget::Serialized, error), error);
  expandIndirectCalls(program);
  const Run result = run(program);
  REQUIRE_MESSAGE(result.ok, result.error);
  CHECK(result.value == 5);
}

TEST_SUITE_END();
