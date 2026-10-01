#include "embed_test_support.h"
#include "primec/embed/Script.h"
#include "primec/embed/ScriptEngine.h"
#include "primec/ir/Ir.h"
#include "primec/ir/IrSerializer.h"
#include "primec/ir/IrValidation.h"
#include "primec/runtime/Vm.h"

#include "third_party/doctest.h"

#include <bit>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

using namespace primec;
namespace embed = primec::embed;

TEST_SUITE_BEGIN("primestruct.embed.host_calls");

namespace {
// main() { return(host_add(40, 2)) } as hand-built IR.
IrModule addModule() {
  IrModule module;
  module.hostImports.push_back({"host_add", {IrHostValueKind::I32, IrHostValueKind::I32}, IrHostValueKind::I32});
  IrFunction main;
  main.name = "/main";
  main.instructions = {{IrOpcode::PushI32, 40}, {IrOpcode::PushI32, 2}, {IrOpcode::CallHost, 0}, {IrOpcode::ReturnI32, 0}};
  module.functions.push_back(main);
  module.entryIndex = 0;
  return module;
}

VmHostBinding addBinding() {
  return {{IrHostValueKind::I32, IrHostValueKind::I32}, IrHostValueKind::I32,
          [](const uint64_t *args, uint64_t &result, std::string &) {
            result = static_cast<uint64_t>(static_cast<int32_t>(args[0]) + static_cast<int32_t>(args[1]));
            return true;
          }};
}

bool runVm(const IrModule &module, const VmHostFunctions &hosts, uint64_t &result, std::string &error) {
  return Vm{}.execute(module, result, error, std::vector<std::string_view>{"prog"}, hosts);
}
} // namespace

TEST_CASE("vm calls a bound host function") {
  VmHostFunctions hosts;
  hosts.bind("host_add", addBinding());
  uint64_t result = 0;
  std::string error;
  REQUIRE_MESSAGE(runVm(addModule(), hosts, result, error), error);
  CHECK(static_cast<int32_t>(result) == 42);
}

TEST_CASE("vm host call with no bindings fails before running") {
  uint64_t result = 0;
  std::string error;
  CHECK_FALSE(Vm{}.execute(addModule(), result, error, std::vector<std::string_view>{"prog"}));
  CHECK(error.find("unbound host function: host_add") != std::string::npos);
}

TEST_CASE("vm rejects a host signature mismatch before running") {
  VmHostFunctions hosts;
  hosts.bind("host_add", {{IrHostValueKind::I64, IrHostValueKind::I64}, IrHostValueKind::I64, addBinding().invoke});
  uint64_t result = 0;
  std::string error;
  CHECK_FALSE(runVm(addModule(), hosts, result, error));
  CHECK(error.find("signature mismatch for host_add") != std::string::npos);
  CHECK(error.find("(i32, i32) -> i32") != std::string::npos);
  CHECK(error.find("(i64, i64) -> i64") != std::string::npos);
}

TEST_CASE("vm host failure and exceptions become errors") {
  IrModule module = addModule();
  VmHostFunctions failing;
  failing.bind("host_add", {{IrHostValueKind::I32, IrHostValueKind::I32}, IrHostValueKind::I32,
                            [](const uint64_t *, uint64_t &, std::string &error) {
                              error = "no can do";
                              return false;
                            }});
  uint64_t result = 0;
  std::string error;
  CHECK_FALSE(runVm(module, failing, result, error));
  CHECK(error == "host function host_add failed: no can do");
  VmHostFunctions throwing;
  throwing.bind("host_add", {{IrHostValueKind::I32, IrHostValueKind::I32}, IrHostValueKind::I32,
                             [](const uint64_t *, uint64_t &, std::string &) -> bool { throw std::runtime_error("boom"); }});
  CHECK_FALSE(runVm(module, throwing, result, error));
  CHECK(error.find("exception: boom") != std::string::npos);
}

TEST_CASE("vm host calls cover every primitive kind and void callbacks") {
  IrModule module;
  module.hostImports.push_back({"observe", {IrHostValueKind::I64, IrHostValueKind::F64, IrHostValueKind::Bool}, IrHostValueKind::Void});
  module.hostImports.push_back({"half", {IrHostValueKind::F32}, IrHostValueKind::F32});
  IrFunction main;
  main.name = "/main";
  const float inputF32 = 5.0f;
  const double inputF64 = 2.5;
  main.instructions = {
      {IrOpcode::PushI64, 123456789012ull},
      {IrOpcode::PushF64, std::bit_cast<uint64_t>(inputF64)},
      {IrOpcode::PushI32, 1},
      {IrOpcode::CallHost, 0},
      {IrOpcode::PushF32, std::bit_cast<uint32_t>(inputF32)},
      {IrOpcode::CallHost, 1},
      {IrOpcode::ConvertF32ToI32, 0},
      {IrOpcode::ReturnI32, 0},
  };
  module.functions.push_back(main);
  module.entryIndex = 0;

  int64_t seenI64 = 0;
  double seenF64 = 0;
  bool seenBool = false;
  VmHostFunctions hosts;
  hosts.bind("observe", {{IrHostValueKind::I64, IrHostValueKind::F64, IrHostValueKind::Bool}, IrHostValueKind::Void,
                         [&](const uint64_t *args, uint64_t &, std::string &) {
                           seenI64 = static_cast<int64_t>(args[0]);
                           seenF64 = std::bit_cast<double>(args[1]);
                           seenBool = args[2] != 0;
                           return true;
                         }});
  hosts.bind("half", {{IrHostValueKind::F32}, IrHostValueKind::F32,
                      [](const uint64_t *args, uint64_t &result, std::string &) {
                        result = std::bit_cast<uint32_t>(std::bit_cast<float>(static_cast<uint32_t>(args[0])) / 2.0f);
                        return true;
                      }});
  uint64_t result = 0;
  std::string error;
  REQUIRE_MESSAGE(runVm(module, hosts, result, error), error);
  CHECK(seenI64 == 123456789012);
  CHECK(seenF64 == 2.5);
  CHECK(seenBool);
  CHECK(static_cast<int32_t>(result) == 2);  // 5.0f / 2 = 2.5 -> 2
}

TEST_CASE("host call results can be used by later instructions repeatedly") {
  IrModule module = addModule();
  // Call twice: host_add(40,2) then add its result via another call's inputs.
  module.functions[0].instructions = {{IrOpcode::PushI32, 40}, {IrOpcode::PushI32, 2}, {IrOpcode::CallHost, 0},
                                      {IrOpcode::PushI32, 8},  {IrOpcode::CallHost, 0}, {IrOpcode::ReturnI32, 0}};
  int calls = 0;
  VmHostFunctions hosts;
  auto binding = addBinding();
  auto inner = binding.invoke;
  binding.invoke = [&, inner](const uint64_t *args, uint64_t &result, std::string &error) {
    ++calls;
    return inner(args, result, error);
  };
  hosts.bind("host_add", binding);
  uint64_t result = 0;
  std::string error;
  REQUIRE_MESSAGE(runVm(module, hosts, result, error), error);
  CHECK(static_cast<int32_t>(result) == 50);
  CHECK(calls == 2);
}

TEST_CASE("validation only allows host calls for the vm target") {
  const IrModule module = addModule();
  std::string error;
  CHECK(validateIrModule(module, IrValidationTarget::Vm, error));
  for (const auto target : {IrValidationTarget::Any, IrValidationTarget::Native, IrValidationTarget::Glsl,
                            IrValidationTarget::Wasm, IrValidationTarget::WasmBrowser}) {
    CHECK_FALSE(validateIrModule(module, target, error));
    CHECK(error.find("host calls are only supported by the vm target") != std::string::npos);
  }
}

TEST_CASE("validation rejects bad host import references") {
  std::string error;
  IrModule badIndex = addModule();
  badIndex.functions[0].instructions[2].imm = 5;
  CHECK_FALSE(validateIrModule(badIndex, IrValidationTarget::Vm, error));
  CHECK(error.find("invalid host import index") != std::string::npos);
  IrModule duplicate = addModule();
  duplicate.hostImports.push_back(duplicate.hostImports[0]);
  CHECK_FALSE(validateIrModule(duplicate, IrValidationTarget::Vm, error));
  CHECK(error.find("duplicate IR host import name") != std::string::npos);
  IrModule unnamed = addModule();
  unnamed.hostImports[0].name.clear();
  CHECK_FALSE(validateIrModule(unnamed, IrValidationTarget::Vm, error));
}

TEST_CASE("host imports survive a serialization round trip") {
  const IrModule module = addModule();
  std::vector<uint8_t> bytes;
  std::string error;
  REQUIRE(serializeIr(module, bytes, error));
  IrModule decoded;
  REQUIRE_MESSAGE(deserializeIr(bytes, decoded, error), error);
  REQUIRE(decoded.hostImports.size() == 1);
  CHECK(decoded.hostImports[0].name == "host_add");
  CHECK(decoded.hostImports[0].parameters == module.hostImports[0].parameters);
  CHECK(decoded.hostImports[0].returnKind == IrHostValueKind::I32);
  std::vector<uint8_t> again;
  REQUIRE(serializeIr(decoded, again, error));
  CHECK(again == bytes);
}

TEST_CASE("deserialization rejects invalid host kinds and old versions") {
  const IrModule module = addModule();
  std::vector<uint8_t> bytes;
  std::string error;
  REQUIRE(serializeIr(module, bytes, error));
  auto badKind = bytes;
  badKind.back() = 0x7f;  // return kind is the final byte
  IrModule decoded;
  CHECK_FALSE(deserializeIr(badKind, decoded, error));
  CHECK(error.find("host import return kind") != std::string::npos);
  auto oldVersion = bytes;
  oldVersion[4] = 23;
  CHECK_FALSE(deserializeIr(oldVersion, decoded, error));
}

TEST_CASE("deserialization accepts every defined opcode value") {
  // Regression: the upper bound used to stop at HeapRealloc, so FileWriteStringDynamic
  // (declared after it) and CallHost could not be loaded back.
  for (uint32_t value = static_cast<uint32_t>(IrOpcode::PushI32); value <= static_cast<uint32_t>(IrOpcode::CallHost);
       ++value) {
    IrModule module = addModule();
    module.functions[0].instructions.push_back({static_cast<IrOpcode>(value), 0});
    std::vector<uint8_t> bytes;
    std::string error;
    REQUIRE(serializeIr(module, bytes, error));
    IrModule decoded;
    CAPTURE(value);
    CHECK_MESSAGE(deserializeIr(bytes, decoded, error), error);
  }
}

TEST_CASE("debug sessions refuse host calls with a diagnostic") {
  VmDebugSession session;
  std::string error;
  REQUIRE(session.start(addModule(), error));
  VmDebugStopReason reason = VmDebugStopReason::Step;
  bool ok = true;
  for (int i = 0; i < 8 && ok; ++i) {
    ok = session.step(reason, error);
  }
  CHECK_FALSE(ok);
  CHECK(error.find("host calls are not supported in VM debug sessions") != std::string::npos);
}

namespace {
embed::Script loadAddScript() {
  std::vector<uint8_t> bytes;
  std::string error;
  REQUIRE(serializeIr(addModule(), bytes, error));
  return embed::Script::loadBytecode(bytes, "host_add_script");
}
} // namespace

TEST_CASE("script binds a lambda with deduced signature and runs it") {
  auto script = loadAddScript();
  REQUIRE_MESSAGE(script.valid(), script.diagnostics());
  CHECK(script.requiredHostFunctions() == std::vector<std::string>{"host_add(i32, i32) -> i32"});
  script.bind("host_add", [](int32_t a, int32_t b) { return a + b; });
  std::string error;
  CHECK_MESSAGE(script.checkHostBindings(error), error);
  const auto result = script.run();
  CHECK(result.ok);
  CHECK(result.exitCode == 42);
}

TEST_CASE("script run without bindings fails with a diagnostic and runs nothing") {
  const auto script = loadAddScript();
  REQUIRE(script.valid());
  const auto result = script.run();
  CHECK_FALSE(result.ok);
  CHECK(result.diagnostics.find("unbound host function: host_add") != std::string::npos);
  std::string error;
  CHECK_FALSE(script.checkHostBindings(error));
}

TEST_CASE("script bind with the wrong C++ signature is reported") {
  auto script = loadAddScript();
  script.bind("host_add", [](double a, double b) { return a + b; });
  const auto result = script.run();
  CHECK_FALSE(result.ok);
  CHECK(result.diagnostics.find("signature mismatch for host_add") != std::string::npos);
  CHECK(result.diagnostics.find("(f64, f64) -> f64") != std::string::npos);
}

TEST_CASE("script rebinding replaces the earlier binding") {
  auto script = loadAddScript();
  script.bind("host_add", [](int32_t, int32_t) { return 1; });
  CHECK(script.run().exitCode == 1);
  script.bind("host_add", [](int32_t a, int32_t b) { return a * b; });
  CHECK(script.run().exitCode == 80);
}

TEST_CASE("script bindings capture host state and copies keep their own table") {
  auto script = loadAddScript();
  int calls = 0;
  script.bind("host_add", [&calls](int32_t a, int32_t b) {
    ++calls;
    return a + b;
  });
  auto copy = script;
  CHECK(script.run().exitCode == 42);
  CHECK(copy.run().exitCode == 42);
  CHECK(calls == 2);
  copy.bind("host_add", [](int32_t, int32_t) { return 7; });
  CHECK(copy.run().exitCode == 7);
  CHECK(script.run().exitCode == 42);
}

TEST_CASE("script host function that throws returns a diagnostic") {
  auto script = loadAddScript();
  script.bind("host_add", [](int32_t, int32_t) -> int32_t { throw std::runtime_error("host exploded"); });
  const auto result = script.run();
  CHECK_FALSE(result.ok);
  CHECK(result.diagnostics.find("host exploded") != std::string::npos);
}

TEST_CASE("script supports function pointers and every primitive type") {
  struct Probe {
    static int64_t widen(int32_t v) { return v; }
    static float scale(float v) { return v * 2.0f; }
    static bool flag(bool v) { return !v; }
    static uint64_t big(uint64_t v) { return v + 1; }
    static double precise(double v) { return v / 4.0; }
    static void sink(int64_t) {}
  };
  embed::HostBindings bindings;
  bindings.bind("widen", &Probe::widen);
  bindings.bind("scale", &Probe::scale);
  bindings.bind("flag", &Probe::flag);
  bindings.bind("big", &Probe::big);
  bindings.bind("precise", &Probe::precise);
  bindings.bind("sink", &Probe::sink);
  REQUIRE(bindings.entries().size() == 6);
  CHECK(bindings.entries()[0].parameters == std::vector<embed::HostType>{embed::HostType::I32});
  CHECK(bindings.entries()[0].returnType == embed::HostType::I64);
  CHECK(bindings.entries()[5].returnType == embed::HostType::Void);
  uint64_t result = 0;
  std::string error;
  const uint64_t minusFive = static_cast<uint64_t>(static_cast<int64_t>(-5));
  REQUIRE(bindings.entries()[0].invoke(&minusFive, result, error));
  CHECK(static_cast<int64_t>(result) == -5);
  const uint64_t f32In = std::bit_cast<uint32_t>(1.5f);
  REQUIRE(bindings.entries()[1].invoke(&f32In, result, error));
  CHECK(std::bit_cast<float>(static_cast<uint32_t>(result)) == 3.0f);
  const uint64_t one = 1;
  REQUIRE(bindings.entries()[2].invoke(&one, result, error));
  CHECK(result == 0);
  const uint64_t maxU64 = 0xFFFFFFFFFFFFFFFEull;
  REQUIRE(bindings.entries()[3].invoke(&maxU64, result, error));
  CHECK(result == 0xFFFFFFFFFFFFFFFFull);
  const uint64_t f64In = std::bit_cast<uint64_t>(10.0);
  REQUIRE(bindings.entries()[4].invoke(&f64In, result, error));
  CHECK(std::bit_cast<double>(result) == 2.5);
}

TEST_CASE("engine bindings flow into compiled scripts") {
  embed::ScriptEngine engine;
  int observed = 0;
  engine.bind("note", [&observed](int32_t v) { observed = v; });
  const auto script = engine.compileSource("/engine_bind.prime", "[return<int>]\nmain() {\n  return(1i32)\n}\n");
  REQUIRE(script.valid());
  CHECK(script.hostBindings().entries().size() == 1);
  CHECK(script.run().exitCode == 1);
  (void)observed;
}

TEST_SUITE_END();
