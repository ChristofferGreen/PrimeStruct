#include "ir_link_test_support.h"

#include "primec/ir/Ir.h"
#include "primec/ir/IrLinker.h"
#include "primec/ir/IrSerializer.h"
#include "primec/ir/IrValidation.h"

#include "third_party/doctest.h"

#include <string>
#include <vector>

using namespace primec;
using namespace ir_link_test;

// String operands after the IR link (TODO-5922): the string table of the library is appended and every string
// operand of its functions is shifted by the size the program's table had.
TEST_SUITE_BEGIN("primestruct.runtime.ir_link_strings");

namespace {

uint64_t newlineString(uint64_t index) {
  return encodePrintStringImm(index, PrintFlagNewline);
}

// main prints "alpha", calls /lib/greet, prints "beta".
IrModule programWithStrings() {
  IrModule module = moduleOf({functionOf("/main",
                                         0,
                                         {{IrOpcode::PrintString, newlineString(0)},
                                          {IrOpcode::CallVoid, 1},
                                          {IrOpcode::PrintString, newlineString(1)},
                                          {IrOpcode::PushI32, 0},
                                          {IrOpcode::ReturnI32, 0}}),
                              importOf("/lib/greet", 0, "lib")});
  module.stringTable = {"alpha", "beta"};
  return module;
}

// greet uses every string opcode the VM runs without a file path: the static print, the dynamic print, the length,
// the static and dynamic byte reads, and the static and dynamic file writes (to descriptor 1).
IrModule libraryWithStrings() {
  IrModule module = moduleOf({functionOf("/lib/greet",
                                         0,
                                         {{IrOpcode::PrintString, newlineString(0)},
                                          stringPush(IrOpcode::PushI32, 1),
                                          {IrOpcode::PrintStringDynamic, Newline},
                                          stringPush(IrOpcode::PushI32, 2),
                                          {IrOpcode::LoadStringLength, 0},
                                          {IrOpcode::PrintI32, Newline},
                                          {IrOpcode::PushI32, 1},
                                          {IrOpcode::LoadStringByte, 3},
                                          {IrOpcode::PrintI32, Newline},
                                          stringPush(IrOpcode::PushI32, 3),
                                          {IrOpcode::PushI32, 0},
                                          {IrOpcode::LoadStringByteDynamic, 0},
                                          {IrOpcode::PrintI32, Newline},
                                          {IrOpcode::PushI32, 1},
                                          {IrOpcode::FileWriteString, 2},
                                          {IrOpcode::Pop, 0},
                                          {IrOpcode::PushI32, 1},
                                          stringPush(IrOpcode::PushI32, 1),
                                          {IrOpcode::FileWriteStringDynamic, 0},
                                          {IrOpcode::Pop, 0},
                                          {IrOpcode::PrintString, newlineString(0)},
                                          {IrOpcode::ReturnVoid, 0}})},
                             -1);
  module.stringTable = {"gamma", "delta", "epsilon", "zeta"};
  return module;
}

} // namespace

TEST_CASE("link strings: text from both modules prints right after the remap") {
  std::vector<uint8_t> bytes;
  std::string error;
  IrModule program;
  IrModule library;
  REQUIRE_MESSAGE(serializeIr(programWithStrings(), bytes, error), error);
  REQUIRE_MESSAGE(deserializeIr(bytes, program, error), error);
  REQUIRE_MESSAGE(serializeIr(libraryWithStrings(), bytes, error), error);
  REQUIRE_MESSAGE(deserializeIr(bytes, library, error), error);

  REQUIRE_MESSAGE(linkIrModule(program, library, error), error);
  REQUIRE_MESSAGE(checkIrImportsResolved(program, error), error);
  REQUIRE_MESSAGE(validateIrModule(program, IrValidationTarget::Vm, error), error);
  CHECK(program.stringTable ==
        std::vector<std::string>{"alpha", "beta", "gamma", "delta", "epsilon", "zeta"});
  const Run result = run(program);
  REQUIRE_MESSAGE(result.ok, result.error);
  CHECK(result.value == 0);
  CHECK(result.out == "alpha\ngamma\ndelta\n7\n101\n122\nepsilondeltagamma\nbeta\n");
}

TEST_CASE("link strings: a program without strings takes the library's table unshifted") {
  IrModule program = programWithStrings();
  program.stringTable.clear();
  program.functions[0].instructions = {
      {IrOpcode::CallVoid, 1}, {IrOpcode::PushI32, 0}, {IrOpcode::ReturnI32, 0}};
  std::string error;
  REQUIRE_MESSAGE(linkIrModule(program, libraryWithStrings(), error), error);
  REQUIRE_MESSAGE(validateIrModule(program, IrValidationTarget::Vm, error), error);
  const Run result = run(program);
  REQUIRE_MESSAGE(result.ok, result.error);
  CHECK(result.out == "gamma\ndelta\n7\n101\n122\nepsilondeltagamma\n");
}

TEST_CASE(
    "link strings: every string-carrying opcode and a host call's String argument are shifted") {
  IrModule library =
      moduleOf({functionOf("/lib/all",
                           0,
                           {{IrOpcode::FileOpenRead, 1},
                            {IrOpcode::FileOpenWrite, 0},
                            {IrOpcode::FileOpenAppend, 1},
                            {IrOpcode::FileWriteString, 0},
                            {IrOpcode::LoadStringByte, 1},
                            {IrOpcode::PrintString,
                             encodePrintStringImm(1, PrintFlagNewline | PrintFlagStderr)},
                            {IrOpcode::PushI32, 7},
                            stringPush(IrOpcode::PushI32, 1),
                            {IrOpcode::PushI64, 0},
                            {IrOpcode::CallHost, 0},
                            stringPush(IrOpcode::PushI64, 1),
                            {IrOpcode::PushI32, 0},
                            {IrOpcode::LoadStringByteDynamic, 0},
                            {IrOpcode::ReturnVoid, 0}})},
               -1);
  library.stringTable = {"one", "two"};
  library.hostImports.push_back(
      {"host_note",
       {IrHostValueKind::I32, IrHostValueKind::String, IrHostValueKind::I32},
       IrHostValueKind::Void});
  IrModule program = programWithStrings();
  program.stringTable = {"a", "b", "c"};
  std::string error;
  REQUIRE_MESSAGE(linkIrModule(program, library, error), error);
  REQUIRE(program.functions.size() == 3);
  const std::vector<IrInstruction> &code = program.functions[2].instructions;
  CHECK(program.functions[2].name == "/lib/all");
  CHECK(code[0].imm == 4);
  CHECK(code[1].imm == 3);
  CHECK(code[2].imm == 4);
  CHECK(code[3].imm == 3);
  CHECK(code[4].imm == 4);
  CHECK(decodePrintStringIndex(code[5].imm) == 4);
  CHECK(decodePrintFlags(code[5].imm) == (PrintFlagNewline | PrintFlagStderr));
  // The pushes the String argument of host_note and the dynamic byte read consume are the string indices; the
  // other pushes are plain integers and stay as they were.
  CHECK(code[6].imm == 7);
  CHECK(code[7].imm == 4);
  CHECK(code[8].imm == 0);
  CHECK(code[10].imm == 4);
  CHECK(code[11].imm == 0);
}

TEST_CASE(
    "link strings: a library function prints a string it received and another returns a literal") {
  // /lib/say prints the string index it gets as a parameter, through a local; /lib/name returns a literal of the
  // library's table; main passes the returned one and then its own literal.
  IrModule library = moduleOf(
      {functionOf("/lib/say",
                  1,
                  {{IrOpcode::StoreLocal, 0},
                   {IrOpcode::LoadLocal, 0},
                   {IrOpcode::PrintStringDynamic, Newline},
                   {IrOpcode::ReturnVoid, 0}}),
       functionOf("/lib/name", 0, {stringPush(IrOpcode::PushI64, 1), {IrOpcode::ReturnI64, 0}})},
      -1);
  library.stringTable = {"unused", "gamma"};
  IrModule program = moduleOf({functionOf("/main",
                                          0,
                                          {{IrOpcode::Call, 2},
                                           {IrOpcode::CallVoid, 1},
                                           stringPush(IrOpcode::PushI64, 0),
                                           {IrOpcode::CallVoid, 1},
                                           {IrOpcode::PrintString, newlineString(1)},
                                           {IrOpcode::PushI32, 0},
                                           {IrOpcode::ReturnI32, 0}}),
                               importOf("/lib/say", 1, "lib"),
                               importOf("/lib/name", 0, "lib")});
  program.stringTable = {"alpha", "beta"};
  std::string error;
  REQUIRE_MESSAGE(linkIrModule(program, library, error), error);
  REQUIRE_MESSAGE(checkIrImportsResolved(program, error), error);
  REQUIRE_MESSAGE(validateIrModule(program, IrValidationTarget::Vm, error), error);
  const Run result = run(program);
  REQUIRE_MESSAGE(result.ok, result.error);
  CHECK(result.out == "gamma\nalpha\nbeta\n");
}

TEST_CASE("link strings: a marked push no consumer follows still moves, an unmarked one stays") {
  // The literal is stored and never printed in the library: only the mark says it is a string index.
  IrModule library = moduleOf({functionOf("/lib/keep",
                                          0,
                                          {stringPush(IrOpcode::PushI32, 0),
                                           {IrOpcode::StoreLocal, 0},
                                           {IrOpcode::PushI32, 0},
                                           {IrOpcode::StoreLocal, 1},
                                           {IrOpcode::ReturnVoid, 0}})},
                              -1);
  library.stringTable = {"kept"};
  IrModule program = programWithStrings();
  std::string error;
  REQUIRE_MESSAGE(linkIrModule(program, library, error), error);
  REQUIRE(program.functions.size() == 3);
  const std::vector<IrInstruction> &code = program.functions[2].instructions;
  REQUIRE(code.size() == 5);
  CHECK(code[0].imm == 2);
  CHECK(code[0].isStringLiteral);
  CHECK(code[2].imm == 0);
  CHECK_FALSE(code[2].isStringLiteral);
}

TEST_CASE("link strings: a library that does not mark its string pushes is refused") {
  IrModule library = moduleOf({functionOf("/lib/greet",
                                          0,
                                          {{IrOpcode::PushI32, 0},
                                           {IrOpcode::PrintStringDynamic, Newline},
                                           {IrOpcode::ReturnVoid, 0}})},
                              -1);
  library.stringTable = {"gamma"};
  library.stringPushesMarked = false;
  IrModule program = programWithStrings();
  const IrModule before = program;
  std::string error;
  CHECK_FALSE(linkIrModule(program, library, error));
  CHECK(error == "the library was written before IR schema v39 and does not mark its string "
                 "literals; rebuild it");
  CHECK(program.stringTable == before.stringTable);
  CHECK(program.functions[1].isImport);

  // The program side may be unmarked: its strings do not move.
  IrModule marked = library;
  marked.stringPushesMarked = true;
  marked.functions[0].instructions[0].isStringLiteral = true;
  IrModule unmarkedProgram = programWithStrings();
  unmarkedProgram.stringPushesMarked = false;
  REQUIRE_MESSAGE(linkIrModule(unmarkedProgram, marked, error), error);
  CHECK_FALSE(unmarkedProgram.stringPushesMarked);
}

TEST_SUITE_END();
