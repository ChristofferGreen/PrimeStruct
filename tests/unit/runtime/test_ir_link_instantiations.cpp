#include "ir_link_test_support.h"

#include "primec/ir/Ir.h"
#include "primec/ir/IrLinker.h"
#include "primec/ir/IrValidation.h"

#include "third_party/doctest.h"

#include <string>

using namespace primec;
using namespace ir_link_test;

// Generic instantiations that the program and a library both produce (docs/ModulesAndLibrariesPlan.md, section 9, step
// 2): the compiler names an instantiation from its template arguments, so one name is one body, and the link keeps one
// copy of identical bodies and refuses different ones.
TEST_SUITE_BEGIN("primestruct.runtime.ir_link");

namespace {

constexpr const char *InstanceName = "/lib/count__t0123456789abcdef";

// `/main` calls the instantiation and the function `/lib/entry` of the library calls it too.
IrModule program(uint64_t addend) {
  return moduleOf(
      {functionOf("/main",
                  0,
                  {{IrOpcode::PushI32, 5},
                   {IrOpcode::Call, 1},
                   {IrOpcode::Call, 2},
                   {IrOpcode::AddI32, 0},
                   {IrOpcode::ReturnI32, 0}}),
       functionOf(InstanceName,
                  1,
                  {{IrOpcode::PushI32, addend}, {IrOpcode::AddI32, 0}, {IrOpcode::ReturnI32, 0}}),
       importOf("/lib/entry", 0, "lib")});
}

IrModule library(uint64_t addend) {
  return moduleOf(
      {functionOf("/lib/entry",
                  0,
                  {{IrOpcode::PushI32, 1}, {IrOpcode::Call, 1}, {IrOpcode::ReturnI32, 0}}),
       functionOf(InstanceName,
                  1,
                  {{IrOpcode::PushI32, addend}, {IrOpcode::AddI32, 0}, {IrOpcode::ReturnI32, 0}})},
      -1);
}

} // namespace

TEST_CASE("link instantiations: identical bodies of one instantiation are one copy") {
  IrModule linked = program(10);
  std::string error;
  REQUIRE_MESSAGE(linkIrModule(linked, library(10), error), error);
  REQUIRE_MESSAGE(checkIrImportsResolved(linked, error), error);
  REQUIRE_MESSAGE(validateIrModule(linked, IrValidationTarget::Vm, error), error);
  size_t copies = 0;
  for (const IrFunction &function : linked.functions) {
    copies += function.name == InstanceName ? 1 : 0;
  }
  CHECK(copies == 1);
  const Run result = run(linked);
  REQUIRE_MESSAGE(result.ok, result.error);
  // (5 + 10) + (1 + 10)
  CHECK(result.value == 26);
}

TEST_CASE("link instantiations: two bodies under one instantiation name are an error") {
  IrModule linked = program(10);
  const IrModule before = linked;
  std::string error;
  CHECK_FALSE(linkIrModule(linked, library(11), error));
  CHECK(error == std::string("instantiation ") + InstanceName + " differs between the modules");
  CHECK(linked.functions.size() == before.functions.size());
}

TEST_CASE("link instantiations: bodies that print the same text from different tables are one") {
  IrModule first = moduleOf(
      {functionOf("/main", 0, {{IrOpcode::Call, 1}, {IrOpcode::Call, 2}, {IrOpcode::ReturnI32, 0}}),
       functionOf("/lib/show__tabcdef0123",
                  0,
                  {stringPush(IrOpcode::PushI32, 1),
                   {IrOpcode::PrintString, encodePrintStringImm(1, Newline)},
                   {IrOpcode::PushI32, 0},
                   {IrOpcode::ReturnI32, 0}}),
       importOf("/lib/entry", 0, "lib")});
  first.stringTable = {"unused", "shared text"};
  first.stringPushesMarked = true;
  IrModule second =
      moduleOf({functionOf("/lib/entry", 0, {{IrOpcode::Call, 1}, {IrOpcode::ReturnI32, 0}}),
                functionOf("/lib/show__tabcdef0123",
                           0,
                           {stringPush(IrOpcode::PushI32, 0),
                            {IrOpcode::PrintString, encodePrintStringImm(0, Newline)},
                            {IrOpcode::PushI32, 0},
                            {IrOpcode::ReturnI32, 0}})},
               -1);
  second.stringTable = {"shared text"};
  second.stringPushesMarked = true;
  std::string error;
  REQUIRE_MESSAGE(linkIrModule(first, second, error), error);
  REQUIRE_MESSAGE(checkIrImportsResolved(first, error), error);
  size_t copies = 0;
  for (const IrFunction &function : first.functions) {
    copies += function.name == "/lib/show__tabcdef0123" ? 1 : 0;
  }
  CHECK(copies == 1);
}

TEST_CASE("link instantiations: a name that only looks like one is still a duplicate") {
  IrModule linked =
      moduleOf({functionOf("/main", 0, {{IrOpcode::PushI32, 1}, {IrOpcode::ReturnI32, 0}}),
                functionOf("/lib/count__tnothex", 0, {{IrOpcode::ReturnI32, 0}})});
  const IrModule other =
      moduleOf({functionOf("/lib/count__tnothex", 0, {{IrOpcode::ReturnI32, 0}})}, -1);
  std::string error;
  CHECK_FALSE(linkIrModule(linked, other, error));
  CHECK(error == "duplicate definition of /lib/count__tnothex");
}

TEST_SUITE_END();
