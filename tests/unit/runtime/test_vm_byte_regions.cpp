#include "primec/ir/Ir.h"
#include "primec/ir/IrSerializer.h"
#include "primec/ir/IrValidation.h"
#include "primec/runtime/Vm.h"
#include "primec/runtime/VmHeapCore.h"

#include "third_party/doctest.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

using namespace primec;

TEST_SUITE_BEGIN("primestruct.runtime.byte_regions");

namespace {
IrModule moduleOf(std::vector<IrInstruction> instructions) {
  IrModule module;
  IrFunction main;
  main.name = "/main";
  main.instructions = std::move(instructions);
  module.functions.push_back(std::move(main));
  module.entryIndex = 0;
  return module;
}

struct Run {
  bool ok = false;
  int32_t value = 0;
  std::string error;
};

Run run(const IrModule &module) {
  Run result;
  uint64_t raw = 0;
  result.ok = Vm{}.execute(module, raw, result.error, std::vector<std::string_view>{"prog"});
  result.value = static_cast<int32_t>(raw);
  return result;
}

constexpr IrOpcode Alloc = IrOpcode::HeapAllocBytes;
} // namespace

TEST_CASE("bytes written to a byte region read back, zero extended, at any offset") {
  const Run result = run(moduleOf({
      {IrOpcode::PushI32, 16}, {Alloc, 0}, {IrOpcode::StoreLocal, 0},
      {IrOpcode::LoadLocal, 0}, {IrOpcode::PushI32, 3}, {IrOpcode::AddI64, 0},
      {IrOpcode::PushI32, 300}, {IrOpcode::StoreU8, 0}, {IrOpcode::Pop, 0},
      {IrOpcode::LoadLocal, 0}, {IrOpcode::PushI32, 3}, {IrOpcode::AddI64, 0}, {IrOpcode::LoadU8, 0},
      {IrOpcode::ReturnI32, 0}}));
  REQUIRE_MESSAGE(result.ok, result.error);
  CHECK(result.value == 44); // 300 stores its low 8 bits
}

TEST_CASE("StoreU8 pushes the stored byte like StoreIndirect pushes its value") {
  const Run result = run(moduleOf({
      {IrOpcode::PushI32, 4}, {Alloc, 0}, {IrOpcode::PushI32, 0xFFFFFFFFull}, {IrOpcode::StoreU8, 0},
      {IrOpcode::ReturnI32, 0}}));
  REQUIRE_MESSAGE(result.ok, result.error);
  CHECK(result.value == 255);
}

TEST_CASE("a new byte region is zeroed and bytes are independent") {
  const Run result = run(moduleOf({
      {IrOpcode::PushI32, 8}, {Alloc, 0}, {IrOpcode::StoreLocal, 0},
      {IrOpcode::LoadLocal, 0}, {IrOpcode::PushI32, 5}, {IrOpcode::AddI64, 0},
      {IrOpcode::PushI32, 9}, {IrOpcode::StoreU8, 0}, {IrOpcode::Pop, 0},
      // byte 4 + byte 6 are still zero, byte 5 is 9: 0 + 9 + 0
      {IrOpcode::LoadLocal, 0}, {IrOpcode::PushI32, 4}, {IrOpcode::AddI64, 0}, {IrOpcode::LoadU8, 0},
      {IrOpcode::LoadLocal, 0}, {IrOpcode::PushI32, 5}, {IrOpcode::AddI64, 0}, {IrOpcode::LoadU8, 0},
      {IrOpcode::AddI32, 0},
      {IrOpcode::LoadLocal, 0}, {IrOpcode::PushI32, 6}, {IrOpcode::AddI64, 0}, {IrOpcode::LoadU8, 0},
      {IrOpcode::AddI32, 0}, {IrOpcode::ReturnI32, 0}}));
  REQUIRE_MESSAGE(result.ok, result.error);
  CHECK(result.value == 9);
}

TEST_CASE("realloc of a byte region keeps its bytes and zeroes the new ones") {
  const Run result = run(moduleOf({
      {IrOpcode::PushI32, 4}, {Alloc, 0}, {IrOpcode::StoreLocal, 0},
      {IrOpcode::LoadLocal, 0}, {IrOpcode::PushI32, 2}, {IrOpcode::AddI64, 0},
      {IrOpcode::PushI32, 7}, {IrOpcode::StoreU8, 0}, {IrOpcode::Pop, 0},
      {IrOpcode::LoadLocal, 0}, {IrOpcode::PushI32, 64}, {IrOpcode::HeapRealloc, 0}, {IrOpcode::StoreLocal, 1},
      {IrOpcode::LoadLocal, 1}, {IrOpcode::PushI32, 2}, {IrOpcode::AddI64, 0}, {IrOpcode::LoadU8, 0},
      {IrOpcode::LoadLocal, 1}, {IrOpcode::PushI32, 63}, {IrOpcode::AddI64, 0}, {IrOpcode::LoadU8, 0},
      {IrOpcode::AddI32, 0}, {IrOpcode::ReturnI32, 0}}));
  REQUIRE_MESSAGE(result.ok, result.error);
  CHECK(result.value == 7);
}

TEST_CASE("realloc to zero frees the region and a stale address faults") {
  const Run result = run(moduleOf({
      {IrOpcode::PushI32, 4}, {Alloc, 0}, {IrOpcode::StoreLocal, 0},
      {IrOpcode::LoadLocal, 0}, {IrOpcode::PushI32, 0}, {IrOpcode::HeapRealloc, 0}, {IrOpcode::Pop, 0},
      {IrOpcode::LoadLocal, 0}, {IrOpcode::LoadU8, 0}, {IrOpcode::ReturnI32, 0}}));
  CHECK_FALSE(result.ok);
  CHECK(result.error.find("invalid byte address in IR") != std::string::npos);
}

TEST_CASE("reading past a region, before it, or after it was freed faults") {
  const std::vector<IrInstruction> prologue = {{IrOpcode::PushI32, 8}, {Alloc, 0}, {IrOpcode::StoreLocal, 0}};
  const auto withTail = [&](std::vector<IrInstruction> tail) {
    std::vector<IrInstruction> all = prologue;
    all.insert(all.end(), tail.begin(), tail.end());
    return run(moduleOf(std::move(all)));
  };
  // The last byte is fine, the one after it is not.
  CHECK(withTail({{IrOpcode::LoadLocal, 0}, {IrOpcode::PushI32, 7}, {IrOpcode::AddI64, 0}, {IrOpcode::LoadU8, 0},
                  {IrOpcode::ReturnI32, 0}})
            .ok);
  const Run past = withTail({{IrOpcode::LoadLocal, 0}, {IrOpcode::PushI32, 8}, {IrOpcode::AddI64, 0},
                             {IrOpcode::LoadU8, 0}, {IrOpcode::ReturnI32, 0}});
  CHECK_FALSE(past.ok);
  CHECK(past.error.find("invalid byte address in IR") != std::string::npos);
  const Run before = withTail({{IrOpcode::LoadLocal, 0}, {IrOpcode::PushI32, 1}, {IrOpcode::SubI64, 0},
                               {IrOpcode::LoadU8, 0}, {IrOpcode::ReturnI32, 0}});
  CHECK_FALSE(before.ok);
  const Run freed = withTail({{IrOpcode::LoadLocal, 0}, {IrOpcode::HeapFree, 0}, {IrOpcode::LoadLocal, 0},
                              {IrOpcode::LoadU8, 0}, {IrOpcode::ReturnI32, 0}});
  CHECK_FALSE(freed.ok);
  CHECK(freed.error.find("invalid byte address in IR") != std::string::npos);
  const Run store = withTail({{IrOpcode::LoadLocal, 0}, {IrOpcode::PushI32, 8}, {IrOpcode::AddI64, 0},
                              {IrOpcode::PushI32, 1}, {IrOpcode::StoreU8, 0}, {IrOpcode::ReturnI32, 0}});
  CHECK_FALSE(store.ok);
  CHECK(store.error.find("invalid byte address in IR") != std::string::npos);
}

TEST_CASE("a freed byte region is reused, and the old address stays invalid") {
  // The second allocation reuses the first one's bytes under a new generation.
  const Run result = run(moduleOf({
      {IrOpcode::PushI32, 8}, {Alloc, 0}, {IrOpcode::StoreLocal, 0},
      {IrOpcode::LoadLocal, 0}, {IrOpcode::PushI32, 99}, {IrOpcode::StoreU8, 0}, {IrOpcode::Pop, 0},
      {IrOpcode::LoadLocal, 0}, {IrOpcode::HeapFree, 0},
      {IrOpcode::PushI32, 8}, {Alloc, 0}, {IrOpcode::StoreLocal, 1},
      {IrOpcode::LoadLocal, 1}, {IrOpcode::LoadU8, 0}, // zeroed again
      {IrOpcode::LoadLocal, 0}, {IrOpcode::LoadU8, 0}, // stale address: faults
      {IrOpcode::AddI32, 0}, {IrOpcode::ReturnI32, 0}}));
  CHECK_FALSE(result.ok);
  CHECK(result.error.find("invalid byte address in IR") != std::string::npos);
}

TEST_CASE("a byte access to an aligned slot reads and writes the slot's low byte") {
  // A pointer to a u8 local or slot field dereferences with LoadU8/StoreU8 like a byte-region pointer.
  const Run loadHeapSlot = run(moduleOf({{IrOpcode::PushI32, 2}, {IrOpcode::HeapAlloc, 0}, {IrOpcode::StoreLocal, 0},
                                         {IrOpcode::LoadLocal, 0}, {IrOpcode::PushI64, 0x1FF}, {IrOpcode::StoreIndirect, 0},
                                         {IrOpcode::Pop, 0}, {IrOpcode::LoadLocal, 0}, {IrOpcode::LoadU8, 0},
                                         {IrOpcode::ReturnI32, 0}}));
  REQUIRE_MESSAGE(loadHeapSlot.ok, loadHeapSlot.error);
  CHECK(loadHeapSlot.value == 0xFF);
  const Run storeLocal = run(moduleOf({{IrOpcode::AddressOfLocal, 0}, {IrOpcode::PushI32, 300}, {IrOpcode::StoreU8, 0},
                                       {IrOpcode::Pop, 0}, {IrOpcode::LoadLocal, 0}, {IrOpcode::ReturnI32, 0}}));
  REQUIRE_MESSAGE(storeLocal.ok, storeLocal.error);
  CHECK(storeLocal.value == 44);
  // An unaligned slot address is still a fault.
  const Run unaligned = run(moduleOf({{IrOpcode::AddressOfLocal, 0}, {IrOpcode::PushI64, 1}, {IrOpcode::AddI64, 0},
                                      {IrOpcode::LoadU8, 0}, {IrOpcode::ReturnI32, 0}}));
  CHECK_FALSE(unaligned.ok);
}

TEST_CASE("slot and byte memory cannot be mixed") {
  const Run slotOnByte = run(moduleOf({{IrOpcode::PushI32, 16}, {Alloc, 0}, {IrOpcode::LoadIndirect, 0},
                                       {IrOpcode::ReturnI32, 0}}));
  CHECK_FALSE(slotOnByte.ok);
  CHECK(slotOnByte.error.find("slot access to a byte region in IR") != std::string::npos);
  const Run storeSlotOnByte = run(moduleOf({{IrOpcode::PushI32, 16}, {Alloc, 0}, {IrOpcode::PushI32, 1},
                                            {IrOpcode::StoreIndirect, 0}, {IrOpcode::ReturnI32, 0}}));
  CHECK_FALSE(storeSlotOnByte.ok);
  // Freeing a slot region with a byte address of the same offset is not a free of that region.
  const Run freeWrongKind = run(moduleOf({{IrOpcode::PushI32, 1}, {IrOpcode::HeapAlloc, 0}, {IrOpcode::StoreLocal, 0},
                                          {IrOpcode::PushI32, 1}, {Alloc, 0}, {IrOpcode::HeapFree, 0},
                                          {IrOpcode::LoadLocal, 0}, {IrOpcode::LoadIndirect, 0}, {IrOpcode::ReturnI32, 0}}));
  CHECK(freeWrongKind.ok);
}

TEST_CASE("a zero byte allocation is the null address and freeing it is a no-op") {
  const Run result = run(moduleOf({{IrOpcode::PushI32, 0}, {Alloc, 0}, {IrOpcode::StoreLocal, 0},
                                   {IrOpcode::LoadLocal, 0}, {IrOpcode::HeapFree, 0},
                                   {IrOpcode::LoadLocal, 0}, {IrOpcode::ReturnI32, 0}}));
  REQUIRE_MESSAGE(result.ok, result.error);
  CHECK(result.value == 0);
}

TEST_CASE("ZextU8 keeps the low eight bits") {
  const auto zext = [](int32_t value) {
    return run(moduleOf({{IrOpcode::PushI32, static_cast<uint64_t>(static_cast<uint32_t>(value))}, {IrOpcode::ZextU8, 0},
                         {IrOpcode::ReturnI32, 0}}));
  };
  CHECK(zext(0).value == 0);
  CHECK(zext(200).value == 200);
  CHECK(zext(255).value == 255);
  CHECK(zext(256).value == 0);
  CHECK(zext(257).value == 1);
  CHECK(zext(-1).value == 255);
  const Run wide = run(moduleOf({{IrOpcode::PushI64, 0x123456789Aull}, {IrOpcode::ZextU8, 0}, {IrOpcode::ReturnI64, 0}}));
  REQUIRE_MESSAGE(wide.ok, wide.error);
  CHECK(wide.value == 0x9A);
  const Run underflow = run(moduleOf({{IrOpcode::ZextU8, 0}, {IrOpcode::ReturnI32, 0}}));
  CHECK_FALSE(underflow.ok);
}

TEST_CASE("byte opcodes underflow with a clear message") {
  CHECK(run(moduleOf({{Alloc, 0}, {IrOpcode::ReturnVoid, 0}})).error.find("IR stack underflow on heap alloc bytes") !=
        std::string::npos);
  CHECK(run(moduleOf({{IrOpcode::LoadU8, 0}, {IrOpcode::ReturnVoid, 0}})).error.find("IR stack underflow on load u8") !=
        std::string::npos);
  CHECK(run(moduleOf({{IrOpcode::PushI32, 1}, {IrOpcode::StoreU8, 0}, {IrOpcode::ReturnVoid, 0}}))
            .error.find("IR stack underflow on store u8") != std::string::npos);
}

TEST_CASE("the byte opcodes survive serialization") {
  IrModule module = moduleOf({{IrOpcode::PushI32, 4}, {Alloc, 0}, {IrOpcode::PushI32, 7}, {IrOpcode::StoreU8, 0},
                              {IrOpcode::ZextU8, 0}, {IrOpcode::ReturnI32, 0}});
  std::string error;
  REQUIRE_MESSAGE(validateIrModule(module, IrValidationTarget::Vm, error), error);
  std::vector<uint8_t> bytes;
  REQUIRE_MESSAGE(serializeIr(module, bytes, error), error);
  IrModule back;
  REQUIRE_MESSAGE(deserializeIr(bytes, back, error), error);
  REQUIRE(back.functions.size() == 1);
  REQUIRE(back.functions[0].instructions.size() == module.functions[0].instructions.size());
  for (size_t i = 0; i < module.functions[0].instructions.size(); ++i) {
    CHECK(back.functions[0].instructions[i].op == module.functions[0].instructions[i].op);
    CHECK(back.functions[0].instructions[i].imm == module.functions[0].instructions[i].imm);
  }
  CHECK(run(back).value == 7);
  CHECK(IrSchemaVersion >= 28u);
}

TEST_CASE("only the VM accepts the byte opcodes for now") {
  const IrModule module = moduleOf({{IrOpcode::PushI32, 4}, {Alloc, 0}, {IrOpcode::LoadU8, 0}, {IrOpcode::ReturnI32, 0}});
  std::string error;
  CHECK(validateIrModule(module, IrValidationTarget::Vm, error));
  CHECK(validateIrModule(module, IrValidationTarget::Serialized, error));
  for (const IrValidationTarget target : {IrValidationTarget::Cpp, IrValidationTarget::Native,
                                          IrValidationTarget::Wasm, IrValidationTarget::Glsl}) {
    error.clear();
    CHECK_FALSE(validateIrModule(module, target, error));
    CHECK(error.find("only supported by the vm yet") != std::string::npos);
    CHECK(error.find("TODO-5542") != std::string::npos);
  }
  const IrModule zext = moduleOf({{IrOpcode::PushI32, 4}, {IrOpcode::ZextU8, 0}, {IrOpcode::ReturnI32, 0}});
  CHECK_FALSE(validateIrModule(zext, IrValidationTarget::Native, error));
  CHECK_FALSE(validateIrModule(zext, IrValidationTarget::Wasm, error));
}

TEST_CASE("heap core reuses the smallest free run that fits and coalesces neighbours") {
  VmHeapCore heap;
  std::string error;
  uint64_t a = 0, b = 0, c = 0, d = 0;
  REQUIRE(heap.allocateBytes(10, a, error));
  REQUIRE(heap.allocateBytes(10, b, error));
  REQUIRE(heap.allocateBytes(10, c, error));
  CHECK(heap.liveByteCount() == 30);
  CHECK(VmHeapCore::isByteAddress(a));
  CHECK((b & VmHeapCore::ByteOffsetMask) == 10);
  REQUIRE(heap.release(a, error));
  REQUIRE(heap.release(b, error));
  CHECK(heap.liveByteCount() == 10);
  // The freed neighbours form one run of 20 bytes, so 15 bytes reuse it from the start.
  REQUIRE(heap.allocateBytes(15, d, error));
  CHECK((d & VmHeapCore::ByteOffsetMask) == 0);
  CHECK(heap.byteAt(a) == nullptr); // an earlier generation of those bytes
  CHECK(heap.byteAt(d + 14) != nullptr);
  CHECK(heap.byteAt(d + 15) == nullptr);
  CHECK(heap.byteAt(c) != nullptr);
  // The remaining five bytes are still free: five bytes fit exactly, six extend the heap.
  uint64_t e = 0, f = 0;
  REQUIRE(heap.allocateBytes(5, e, error));
  CHECK((e & VmHeapCore::ByteOffsetMask) == 15);
  REQUIRE(heap.allocateBytes(6, f, error));
  CHECK((f & VmHeapCore::ByteOffsetMask) == 30);
  CHECK(heap.bytes.size() == 36);
}

TEST_CASE("heap core extends a free run at the end of the heap") {
  VmHeapCore heap;
  std::string error;
  uint64_t a = 0, b = 0, c = 0;
  REQUIRE(heap.allocateBytes(8, a, error));
  REQUIRE(heap.allocateBytes(8, b, error));
  REQUIRE(heap.release(b, error));
  REQUIRE(heap.allocateBytes(24, c, error));
  CHECK((c & VmHeapCore::ByteOffsetMask) == 8); // the free tail run was extended, not skipped
  CHECK(heap.bytes.size() == 32);
  CHECK(heap.liveByteCount() == 32);
}

TEST_CASE("heap core realloc copies the leading bytes and frees the old region") {
  VmHeapCore heap;
  std::string error;
  uint64_t region = 0, grown = 0, shrunk = 0;
  REQUIRE(heap.allocateBytes(4, region, error));
  for (uint64_t i = 0; i < 4; ++i) {
    *heap.byteAt(region + i) = static_cast<uint8_t>(10 + i);
  }
  REQUIRE(heap.reallocate(region, 8, grown, error));
  CHECK(heap.byteAt(region) == nullptr);
  CHECK(*heap.byteAt(grown + 3) == 13);
  CHECK(*heap.byteAt(grown + 7) == 0);
  REQUIRE(heap.reallocate(grown, 2, shrunk, error));
  CHECK(*heap.byteAt(shrunk + 1) == 11);
  CHECK(heap.byteAt(shrunk + 2) == nullptr);
  CHECK(heap.liveByteCount() == 2);
  uint64_t none = 1;
  REQUIRE(heap.reallocate(shrunk, 0, none, error));
  CHECK(none == 0);
  CHECK(heap.liveByteCount() == 0);
  // Realloc of the null address allocates slots, as before; byte regions start with allocateBytes.
  uint64_t slots = 0;
  REQUIRE(heap.reallocate(0, 2, slots, error));
  CHECK_FALSE(VmHeapCore::isByteAddress(slots));
}

TEST_CASE("heap core rejects freeing an address it does not hold") {
  VmHeapCore heap;
  std::string error;
  uint64_t region = 0;
  REQUIRE(heap.allocateBytes(4, region, error));
  CHECK_FALSE(heap.release(region + 1, error)); // inside, but not the start
  CHECK(error.find("invalid heap free address in IR") != std::string::npos);
  REQUIRE(heap.release(region, error));
  CHECK_FALSE(heap.release(region, error)); // already freed
  uint64_t out = 0;
  CHECK_FALSE(heap.reallocate(region, 8, out, error));
}

TEST_CASE("heap core refuses a byte allocation that cannot be addressed") {
  VmHeapCore heap;
  std::string error;
  uint64_t address = 0;
  CHECK_FALSE(heap.allocateBytes(VmHeapCore::ByteOffsetMask + 1, address, error));
  CHECK(error == "VM heap allocation overflow");
}
