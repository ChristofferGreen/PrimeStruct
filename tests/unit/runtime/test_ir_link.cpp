#include "ir_link_test_support.h"

#include "primec/ir/Ir.h"
#include "primec/ir/IrLinker.h"
#include "primec/ir/IrOperandTable.h"
#include "primec/ir/IrSerializer.h"
#include "primec/ir/IrValidation.h"

#include "third_party/doctest.h"

#include <string>
#include <unordered_set>
#include <vector>

using namespace primec;
using namespace ir_link_test;

// The IR link (TODO-5922, docs/ModulesAndLibrariesPlan.md section 9) with hand-built serialized modules.
TEST_SUITE_BEGIN("primestruct.runtime.ir_link");

namespace {

IrModule roundTrip(const IrModule &module) {
  std::vector<uint8_t> bytes;
  std::string error;
  REQUIRE_MESSAGE(serializeIr(module, bytes, error), error);
  IrModule back;
  REQUIRE_MESSAGE(deserializeIr(bytes, back, error), error);
  return back;
}

IrStructLayout layoutOf(uint32_t size) {
  IrStructLayout layout;
  layout.name = "/Point";
  layout.totalSizeBytes = size;
  layout.alignmentBytes = 4;
  IrStructField field;
  field.name = "x";
  field.envelope = "i32";
  field.sizeBytes = 4;
  field.alignmentBytes = 4;
  layout.fields.push_back(field);
  return layout;
}

IrHostImport hostOf(IrHostValueKind parameter) {
  return {"host_log", {parameter}, IrHostValueKind::Void};
}

} // namespace

TEST_CASE("link: an import resolves to the definition in the serialized library and runs") {
  IrModule program = roundTrip(consumerModule());
  const IrModule library = roundTrip(libraryModule());
  CHECK(library.entryIndex == -1);
  CHECK(program.functions[1].isImport);
  CHECK(program.functions[1].importLibrary == "lib");
  std::string error;
  REQUIRE_MESSAGE(linkIrModule(program, library, error), error);
  REQUIRE_MESSAGE(checkIrImportsResolved(program, error), error);
  REQUIRE_MESSAGE(validateIrModule(program, IrValidationTarget::Vm, error), error);
  REQUIRE(program.functions.size() == 3);
  CHECK(program.entryIndex == 0);
  CHECK_FALSE(program.functions[1].isImport);
  CHECK(program.functions[1].name == "/lib/add_one");
  // add_one calls twice, which moved from index 1 of the library to index 2 of the merged module.
  CHECK(program.functions[1].instructions[3].imm == 2);
  const Run result = run(program);
  REQUIRE_MESSAGE(result.ok, result.error);
  CHECK(result.value == 21);
}

TEST_CASE("link: an import in a library resolves in a library linked after it") {
  const IrModule main = consumerModule();
  IrModule first = libraryModule();
  // The first library calls /other/seven, which only the second library defines.
  first.functions.push_back(importOf("/other/seven", 0, "other"));
  first.functions[1].instructions = {{IrOpcode::StoreLocal, 0},
                                     {IrOpcode::LoadLocal, 0},
                                     {IrOpcode::Call, 2},
                                     {IrOpcode::AddI32, 0},
                                     {IrOpcode::ReturnI32, 0}};
  const IrModule second = moduleOf(
      {functionOf("/other/seven", 0, {{IrOpcode::PushI32, 7}, {IrOpcode::ReturnI32, 0}})}, -1);
  std::string error;
  IrModule program = main;
  REQUIRE_MESSAGE(linkIrModule(program, first, error), error);
  CHECK_FALSE(checkIrImportsResolved(program, error));
  CHECK(error == "undefined symbol /other/seven (library other)");
  REQUIRE_MESSAGE(linkIrModule(program, second, error), error);
  REQUIRE_MESSAGE(checkIrImportsResolved(program, error), error);
  REQUIRE_MESSAGE(validateIrModule(program, IrValidationTarget::Vm, error), error);
  const Run result = run(program);
  REQUIRE_MESSAGE(result.ok, result.error);
  // add_one(20) = 20 + twice(0) + 1 and twice(x) = x + seven().
  CHECK(result.value == 28);
}

TEST_CASE("link: a name defined in both modules is an error and the module is unchanged") {
  IrModule program =
      moduleOf({functionOf("/main", 0, {{IrOpcode::PushI32, 1}, {IrOpcode::ReturnI32, 0}}),
                functionOf("/lib/twice", 1, {{IrOpcode::ReturnI32, 0}})});
  const IrModule before = program;
  std::string error;
  CHECK_FALSE(linkIrModule(program, libraryModule(), error));
  CHECK(error == "duplicate definition of /lib/twice");
  CHECK(program.functions.size() == before.functions.size());
  CHECK(program.functions[1].instructions.size() == before.functions[1].instructions.size());
}

TEST_CASE("link: a declaration with another parameter count is an error") {
  IrModule program = consumerModule();
  program.functions[1].parameterCount = 2;
  std::string error;
  CHECK_FALSE(linkIrModule(program, libraryModule(), error));
  CHECK(error.find("/lib/add_one") != std::string::npos);
  CHECK(error.find("parameters") != std::string::npos);
}

TEST_CASE("link: equal struct layouts merge and unequal ones of one name are an error") {
  IrModule program = consumerModule();
  program.structLayouts.push_back(layoutOf(4));
  IrModule library = libraryModule();
  library.structLayouts.push_back(layoutOf(4));
  IrStructLayout other = layoutOf(8);
  other.name = "/Other";
  library.structLayouts.push_back(other);
  std::string error;
  REQUIRE_MESSAGE(linkIrModule(program, library, error), error);
  REQUIRE(program.structLayouts.size() == 2);
  CHECK(program.structLayouts[1].name == "/Other");

  IrModule unequal = consumerModule();
  unequal.structLayouts.push_back(layoutOf(4));
  IrModule wider = libraryModule();
  wider.structLayouts.push_back(layoutOf(8));
  CHECK_FALSE(linkIrModule(unequal, wider, error));
  CHECK(error == "struct layout of /Point differs between the modules");
  CHECK(unequal.functions[1].isImport);

  IrModule fieldDiffers = libraryModule();
  fieldDiffers.structLayouts.push_back(layoutOf(4));
  fieldDiffers.structLayouts[0].fields[0].offsetBytes = 4;
  IrModule same = consumerModule();
  same.structLayouts.push_back(layoutOf(4));
  CHECK_FALSE(linkIrModule(same, fieldDiffers, error));
}

TEST_CASE("link: an import nobody defines is undefined symbol <name> (library <lib>)") {
  IrModule program = consumerModule();
  const IrModule emptyLibrary =
      moduleOf({functionOf("/lib/other", 0, {{IrOpcode::ReturnVoid, 0}})}, -1);
  std::string error;
  REQUIRE_MESSAGE(linkIrModule(program, emptyLibrary, error), error);
  CHECK_FALSE(checkIrImportsResolved(program, error));
  CHECK(error == "undefined symbol /lib/add_one (library lib)");
  // An unlinked module does not validate either, so nothing runs it.
  CHECK_FALSE(validateIrModule(program, IrValidationTarget::Vm, error));
  CHECK(error.find("/lib/add_one") != std::string::npos);
}

TEST_CASE("link: host imports merge by name and a name with two signatures is an error") {
  IrModule program = consumerModule();
  program.hostImports.push_back(hostOf(IrHostValueKind::I32));
  IrModule library = libraryModule();
  library.hostImports.push_back({"host_other", {}, IrHostValueKind::I32});
  library.hostImports.push_back(hostOf(IrHostValueKind::I32));
  library.functions[1].instructions.insert(library.functions[1].instructions.begin(),
                                           IrInstruction{IrOpcode::CallHost, 1});
  std::string error;
  IrModule merged = program;
  REQUIRE_MESSAGE(linkIrModule(merged, library, error), error);
  REQUIRE(merged.hostImports.size() == 2);
  CHECK(merged.hostImports[1].name == "host_other");
  // The library's host_log (its index 1) is the program's index 0; host_other (its index 0) moved to index 1.
  CHECK(merged.functions[2].instructions[0].op == IrOpcode::CallHost);
  CHECK(merged.functions[2].instructions[0].imm == 0);

  IrModule differing = libraryModule();
  differing.hostImports.push_back(hostOf(IrHostValueKind::I64));
  CHECK_FALSE(linkIrModule(program, differing, error));
  CHECK(error == "host import host_log has two signatures");
  CHECK(program.hostImports.size() == 1);

  IrModule returning = libraryModule();
  returning.hostImports.push_back({"host_log", {IrHostValueKind::I32}, IrHostValueKind::I32});
  CHECK_FALSE(linkIrModule(program, returning, error));
}

TEST_CASE("link: only the functions the program reaches come along, with what they read") {
  // /lib/unused and /lib/chain_end are not reached from the program; they read a string and a host import.
  IrModule library = libraryModule();
  library.stringTable = {"kept", "dropped"};
  library.hostImports.push_back({"host_dropped", {}, IrHostValueKind::Void});
  library.functions[1].instructions.insert(library.functions[1].instructions.begin(),
                                           stringPush(IrOpcode::PushI32, 0));
  library.functions[1].instructions.insert(library.functions[1].instructions.begin() + 1,
                                           IrInstruction{IrOpcode::Pop, 0});
  library.functions.push_back(functionOf("/lib/unused",
                                         0,
                                         {stringPush(IrOpcode::PushI32, 1),
                                          IrInstruction{IrOpcode::Pop, 0},
                                          IrInstruction{IrOpcode::CallHost, 0},
                                          IrInstruction{IrOpcode::Call, 3},
                                          IrInstruction{IrOpcode::ReturnI32, 0}}));
  library.functions.push_back(
      functionOf("/lib/chain_end", 0, {{IrOpcode::PushI32, 9}, {IrOpcode::ReturnI32, 0}}));

  IrModule program = consumerModule();
  const std::unordered_set<std::string> reached =
      reachableLibraryFunctionNames(program, {&library});
  CHECK(reached == std::unordered_set<std::string>{"/lib/add_one", "/lib/twice"});

  IrLinkOptions options;
  options.onlyFunctions = &reached;
  std::string error;
  REQUIRE_MESSAGE(linkIrModule(program, library, error, &options), error);
  REQUIRE_MESSAGE(checkIrImportsResolved(program, error), error);
  CHECK(program.functions.size() == 3);
  CHECK(program.stringTable == std::vector<std::string>{"kept"});
  CHECK(program.hostImports.empty());
  REQUIRE_MESSAGE(validateIrModule(program, IrValidationTarget::Vm, error), error);
  const Run result = run(program);
  REQUIRE_MESSAGE(result.ok, result.error);
  CHECK(result.value == 21);

  // The whole link still takes everything.
  IrModule whole = consumerModule();
  REQUIRE_MESSAGE(linkIrModule(whole, library, error), error);
  CHECK(whole.functions.size() == 5);
  CHECK(whole.stringTable.size() == 2);
  CHECK(whole.hostImports.size() == 1);
}

TEST_CASE("link: source maps and debug ids of the library shift past those of the program") {
  IrModule program = consumerModule();
  program.functions[0].instructions[0].debugId = 1;
  program.functions[0].instructions[1].debugId = 2;
  program.instructionSourceMap.push_back(
      {1, 10, 1, IrSourceMapProvenance::CanonicalAst, "main.prime"});
  program.instructionSourceMap.push_back(
      {2, 11, 1, IrSourceMapProvenance::CanonicalAst, "main.prime"});
  IrModule library = libraryModule();
  library.functions[0].instructions[0].debugId = 1;
  library.functions[0].instructions[1].debugId = 0;
  library.functions[1].instructions[0].debugId = 2;
  library.instructionSourceMap.push_back(
      {1, 5, 3, IrSourceMapProvenance::CanonicalAst, "lib.prime"});
  library.instructionSourceMap.push_back(
      {2, 6, 3, IrSourceMapProvenance::SyntheticIr, "lib.prime"});
  std::string error;
  REQUIRE_MESSAGE(linkIrModule(program, library, error), error);
  REQUIRE(program.instructionSourceMap.size() == 4);
  CHECK(program.instructionSourceMap[2].debugId == 3);
  CHECK(program.instructionSourceMap[2].line == 5);
  CHECK(program.instructionSourceMap[2].sourceUnit == "lib.prime");
  CHECK(program.instructionSourceMap[3].debugId == 4);
  CHECK(program.instructionSourceMap[3].provenance == IrSourceMapProvenance::SyntheticIr);
  CHECK(program.functions[1].instructions[0].debugId == 3);
  CHECK(program.functions[1].instructions[1].debugId == 0);
  CHECK(program.functions[2].instructions[0].debugId == 4);
  CHECK(program.functions[0].instructions[0].debugId == 1);
}

TEST_CASE("link: the local debug slots of a library function come along with it") {
  IrModule library = libraryModule();
  library.functions[0].localDebugSlots.push_back({0, "value", "i32"});
  IrModule program = consumerModule();
  std::string error;
  REQUIRE_MESSAGE(linkIrModule(program, library, error), error);
  REQUIRE(program.functions[1].localDebugSlots.size() == 1);
  CHECK(program.functions[1].localDebugSlots[0].name == "value");
  CHECK(program.functions[1].localDebugSlots[0].typeName == "i32");
}

TEST_CASE("operand table: every opcode has a class and the call opcodes name functions") {
  for (const IrOperandInfo &row : IrOperandTable) {
    INFO(irOpcodeInfo(row.op)->name);
    CHECK(row.operandClass != IrOperandClass::Unclassified);
  }
  CHECK(irOperandClass(IrOpcode::Call) == IrOperandClass::Function);
  CHECK(irOperandClass(IrOpcode::CallVoid) == IrOperandClass::Function);
  CHECK(irOperandClass(IrOpcode::TaskSpawn) == IrOperandClass::Function);
  CHECK(irOperandClass(IrOpcode::CallHost) == IrOperandClass::HostImport);
  CHECK(irOperandClass(IrOpcode::PrintString) == IrOperandClass::PrintString);
  CHECK(irOperandClass(IrOpcode::LoadStringByte) == IrOperandClass::String);
  CHECK(irOperandClass(IrOpcode::Jump) == IrOperandClass::Jump);
  CHECK(irOperandClass(IrOpcode::AddI32) == IrOperandClass::None);
}

TEST_SUITE_END();
