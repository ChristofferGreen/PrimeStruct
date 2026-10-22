#include "primec/embed/Script.h"
#include "primec/embed/ScriptEngine.h"
#include "primec/ir/Ir.h"
#include "primec/ir/IrSerializer.h"

#include "third_party/doctest.h"

#include <cstdint>
#include <string>
#include <vector>

using primec::embed::ScriptEngine;

TEST_SUITE_BEGIN("primestruct.language.u8_containers");

namespace {

const std::string Prelude = "import /std/collections/*\n";

// Runs `body` as `main` (effects heap_alloc) and returns the exit code, or -1 when it does not compile.
int runMain(const std::string &body, const std::string &extraDefinitions = "") {
  ScriptEngine engine;
  const std::string source =
      Prelude + extraDefinitions + "\n[return<int> effects(heap_alloc)]\nmain() {\n" + body + "\n}\n";
  const auto script = engine.compileSource("/u8_containers.prime", source);
  if (!script.valid()) {
    MESSAGE(script.diagnostics());
    return -1;
  }
  const auto result = script.run();
  if (!result.ok) {
    MESSAGE(result.diagnostics);
    return -2;
  }
  return result.exitCode;
}

primec::IrModule lowered(const std::string &body) {
  ScriptEngine engine;
  const std::string source = Prelude + "\n[return<int> effects(heap_alloc)]\nmain() {\n" + body + "\n}\n";
  const auto script = engine.compileSource("/u8_containers_ir.prime", source);
  REQUIRE_MESSAGE(script.valid(), script.diagnostics());
  std::vector<uint8_t> bytes;
  std::string error;
  REQUIRE_MESSAGE(script.saveBytecode(bytes, error), error);
  primec::IrModule module;
  REQUIRE_MESSAGE(primec::deserializeIr(bytes, module, error), error);
  return module;
}

bool usesOpcode(const primec::IrModule &module, primec::IrOpcode op) {
  for (const auto &function : module.functions) {
    for (const auto &instruction : function.instructions) {
      if (instruction.op == op) {
        return true;
      }
    }
  }
  return false;
}

} // namespace

TEST_CASE("vector<u8> stores its elements in a byte region") {
  const primec::IrModule packed =
      lowered("  [vector<u8> mut] v{vector<u8>(1u8, 2u8)}\n  v.push(3u8)\n  return(convert<i32>(v.at(2i32)))");
  CHECK(usesOpcode(packed, primec::IrOpcode::HeapAllocBytes));
  CHECK(usesOpcode(packed, primec::IrOpcode::StoreU8));
  CHECK(usesOpcode(packed, primec::IrOpcode::LoadU8));
  const primec::IrModule slots =
      lowered("  [vector<i32> mut] v{vector<i32>(1i32, 2i32)}\n  v.push(3i32)\n  return(v.at(2i32))");
  CHECK_FALSE(usesOpcode(slots, primec::IrOpcode::HeapAllocBytes));
  CHECK_FALSE(usesOpcode(slots, primec::IrOpcode::StoreU8));
  CHECK_FALSE(usesOpcode(slots, primec::IrOpcode::LoadU8));
}

TEST_CASE("vector<u8> literal, push, at and count") {
  CHECK(runMain("  [vector<u8> mut] v{vector<u8>(10u8, 250u8, 7u8)}\n"
                "  v.push(9u8)\n"
                "  return(convert<i32>(v.at(1i32)) + v.count() * 1000i32)") == 4250);
  CHECK(runMain("  [vector<u8>] v{vector<u8>(10u8, 250u8, 7u8)}\n"
                "  return(convert<i32>(v.at(0i32) + v.at(1i32) + v.at(2i32)))") == 11);
}

TEST_CASE("vector<u8> element assignment stores one byte") {
  CHECK(runMain("  [vector<u8> mut] v{vector<u8>(1u8, 2u8, 3u8)}\n"
                "  v[1i32] = 200u8\n"
                "  return(convert<i32>(v.at(0i32)) * 10000i32 + convert<i32>(v.at(1i32)) * 100i32 / 100i32 + "
                "convert<i32>(v.at(2i32)))") == 10000 + 200 + 3);
}

TEST_CASE("vector<u8> pop, clear and reserve") {
  CHECK(runMain("  [vector<u8> mut] v{vector<u8>(1u8, 2u8, 3u8)}\n  v.pop()\n  return(v.count())") == 2);
  CHECK(runMain("  [vector<u8> mut] v{vector<u8>(1u8, 2u8, 3u8)}\n  v.clear()\n  return(v.count())") == 0);
  CHECK(runMain("  [vector<u8> mut] v{vector<u8>(1u8)}\n  v.reserve(100i32)\n  return(v.capacity())") == 100);
}

TEST_CASE("vector<u8> grows past its initial capacity and keeps every byte") {
  const std::string make = R"(
[return<vector<u8>> effects(heap_alloc)]
make_bytes([i32] n) {
  [vector<u8> mut] out{vector<u8>()}
  for([i32 mut] i{0i32}, i < n, i++) {
    out.push(convert<u8>(i))
  }
  return(out)
}

[return<int> effects(heap_alloc)]
checksum([Reference<vector<u8>>] values) {
  [i32 mut] total{0i32}
  for([i32 mut] i{0i32}, i < values.count(), i++) {
    total = total + convert<i32>(values.at(i))
  }
  return(total)
}
)";
  CHECK(runMain("  [vector<u8> mut] v{make_bytes(5000i32)}\n  return(v.count() + convert<i32>(v.at(4999i32)))", make) ==
        5000 + 135);
  CHECK(runMain("  [vector<u8> mut] v{make_bytes(5000i32)}\n  return(checksum(v) - 629340i32)", make) == 0);
}

TEST_CASE("vector<u8> out of range access faults") {
  ScriptEngine engine;
  const auto script = engine.compileSource(
      "/u8_oob.prime",
      Prelude + "\n[return<int> effects(heap_alloc)]\nmain() {\n  [vector<u8>] v{vector<u8>(1u8)}\n  "
                "return(convert<i32>(v.at(5i32)))\n}\n");
  REQUIRE(script.valid());
  const auto result = script.run();
  // A bounds fault ends the program with the runtime-fault exit code (3), as for slot-backed vectors.
  CHECK(result.exitCode == 3);
}

TEST_CASE("Pointer<u8> buffers are byte addressed") {
  const std::string source = R"(
import /std/collections/buffer_unchecked/*
import /std/collections/buffer_checked/*

[return<int> effects(heap_alloc) unsafe]
main() {
  [Pointer<u8> mut] raw{bufferAlloc<u8>(16i32)}
  for([i32 mut] i{0i32}, i < 16i32, i++) {
    bufferWriteChecked<u8>(raw, i, 16i32, convert<u8>(i * 20i32))
  }
  [i32] third{convert<i32>(bufferReadChecked<u8>(raw, 3i32, 16i32))}
  raw = bufferGrow<u8>(raw, 4096i32)
  bufferWriteUnsafe<u8>(raw, 4000i32, convert<u8>(77i32))
  [i32] far{convert<i32>(bufferReadUnsafe<u8>(raw, 4000i32))}
  [i32] kept{convert<i32>(bufferReadUnsafe<u8>(raw, 15i32))}
  bufferFree<u8>(raw)
  return(third * 10000i32 + far * 100i32 + kept - 6044i32 * 100i32 / 100i32)
}
)";
  ScriptEngine engine;
  const auto script = engine.compileSource("/u8_pointer.prime", source);
  REQUIRE_MESSAGE(script.valid(), script.diagnostics());
  const auto result = script.run();
  REQUIRE(result.ok);
  CHECK(result.exitCode == 60 * 10000 + 77 * 100 + 44 - 6044);
}
