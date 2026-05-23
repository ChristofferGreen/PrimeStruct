#include "primec/VmExecutionKernel.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "third_party/doctest.h"

TEST_SUITE_BEGIN("primestruct.vm.execution.kernel");

namespace {

class TestVmKernelHost final : public primec::vm_detail::VmKernelHost {
public:
  std::uint64_t argumentCount() const override { return argCount; }
  std::uint64_t slotBytes() const override { return primec::IrSlotBytes; }
  std::size_t maxCallDepth() const override { return 32; }

  bool resolveIndirectAddress(std::uint64_t address,
                              std::vector<std::uint64_t> &locals,
                              std::uint64_t *&slot,
                              std::string &error) override {
    const std::uint64_t slotIndex = address / slotBytes();
    if (address % slotBytes() != 0 || slotIndex >= locals.size()) {
      error = "test host invalid indirect address";
      return false;
    }
    slot = &locals[static_cast<std::size_t>(slotIndex)];
    return true;
  }

  bool allocateHeapSlots(std::uint64_t slotCount,
                         std::uint64_t &address,
                         std::string &error) override {
    (void)slotCount;
    (void)address;
    error = "test host denies heap allocation";
    return false;
  }

  bool freeHeapSlots(std::uint64_t address, std::string &error) override {
    (void)address;
    error = "test host denies heap free";
    return false;
  }

  bool reallocHeapSlots(std::uint64_t address,
                        std::uint64_t slotCount,
                        std::uint64_t &newAddress,
                        std::string &error) override {
    (void)address;
    (void)slotCount;
    (void)newAddress;
    error = "test host denies heap realloc";
    return false;
  }

  bool handlePrintInstruction(const primec::IrModule &module,
                              const primec::IrInstruction &inst,
                              std::vector<std::uint64_t> &stack,
                              std::string &error) override {
    (void)module;
    (void)inst;
    (void)stack;
    error = "test host denies print";
    return false;
  }

  bool handleFileInstruction(const primec::IrModule &module,
                             const primec::IrInstruction &inst,
                             std::vector<std::uint64_t> &stack,
                             std::vector<std::uint64_t> &locals,
                             std::string &error) override {
    (void)module;
    (void)inst;
    (void)stack;
    (void)locals;
    error = "test host denies file";
    return false;
  }

  std::uint64_t argCount = 0;
};

primec::IrInstruction inst(primec::IrOpcode op, std::uint64_t imm = 0) {
  primec::IrInstruction instruction;
  instruction.op = op;
  instruction.imm = imm;
  return instruction;
}

primec::IrModule makeKernelCallModule() {
  primec::IrFunction entry;
  entry.name = "/main";
  entry.instructions = {
      inst(primec::IrOpcode::Call, 1),
      inst(primec::IrOpcode::PushI32, 4),
      inst(primec::IrOpcode::AddI32),
      inst(primec::IrOpcode::ReturnI32),
  };

  primec::IrFunction callee;
  callee.name = "/callee";
  callee.instructions = {
      inst(primec::IrOpcode::PushI32, 38),
      inst(primec::IrOpcode::ReturnI32),
  };

  primec::IrModule module;
  module.entryIndex = 0;
  module.functions = {entry, callee};
  return module;
}

} // namespace

TEST_CASE("vm execution kernel owns frame calls and numeric opcodes") {
  TestVmKernelHost host;
  std::uint64_t result = 0;
  std::string error;

  CHECK(primec::vm_detail::executeVmKernel(
      makeKernelCallModule(), host, result, error));
  CHECK(result == 42);
  CHECK(error.empty());
}

TEST_CASE("vm execution kernel delegates runtime-only print opcodes to host") {
  primec::IrFunction entry;
  entry.name = "/main";
  entry.instructions = {
      inst(primec::IrOpcode::PushI32, 1),
      inst(primec::IrOpcode::PrintI32),
      inst(primec::IrOpcode::ReturnI32),
  };

  primec::IrModule module;
  module.entryIndex = 0;
  module.functions = {entry};

  TestVmKernelHost host;
  std::uint64_t result = 0;
  std::string error;
  CHECK_FALSE(primec::vm_detail::executeVmKernel(module, host, result, error));
  CHECK(error == "test host denies print");
  CHECK(primec::vm_detail::isVmKernelPrintOpcode(primec::IrOpcode::PrintI32));
  CHECK_FALSE(primec::vm_detail::isVmKernelPrintOpcode(primec::IrOpcode::AddI32));
}

TEST_CASE("vm execution kernel keeps argv and local memory behind host boundary") {
  primec::IrFunction entry;
  entry.name = "/main";
  entry.instructions = {
      inst(primec::IrOpcode::PushArgc),
      inst(primec::IrOpcode::StoreLocal, 0),
      inst(primec::IrOpcode::AddressOfLocal, 0),
      inst(primec::IrOpcode::LoadIndirect),
      inst(primec::IrOpcode::ReturnI32),
  };

  primec::IrModule module;
  module.entryIndex = 0;
  module.functions = {entry};

  TestVmKernelHost host;
  host.argCount = 7;
  std::uint64_t result = 0;
  std::string error;
  CHECK(primec::vm_detail::executeVmKernel(module, host, result, error));
  CHECK(result == 7);
}

TEST_CASE("vm execution kernel exposes file opcode host boundary") {
  CHECK(primec::vm_detail::isVmKernelFileOpcode(primec::IrOpcode::FileFlush));
  CHECK_FALSE(primec::vm_detail::isVmKernelFileOpcode(primec::IrOpcode::PushI32));
}

TEST_CASE("vm execution kernel avoids runtime-only dependencies") {
  auto readText = [](const std::filesystem::path &path) {
    std::ifstream file(path);
    CHECK(file.is_open());
    if (!file.is_open()) {
      return std::string{};
    }
    return std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
  };
  const std::filesystem::path repoRoot =
      std::filesystem::exists(std::filesystem::path("src"))
          ? std::filesystem::path(".")
          : std::filesystem::path("..");
  const std::filesystem::path headerPath =
      repoRoot / "include" / "primec" / "VmExecutionKernel.h";
  const std::filesystem::path sourcePath =
      repoRoot / "src" / "VmExecutionKernel.cpp";
  REQUIRE(std::filesystem::exists(headerPath));
  REQUIRE(std::filesystem::exists(sourcePath));

  const std::string header = readText(headerPath);
  const std::string source = readText(sourcePath);
  CHECK(header.find("primec/Vm.h") == std::string::npos);
  CHECK(header.find("VmDebug") == std::string::npos);
  CHECK(source.find("VmHeapHelpers.h") == std::string::npos);
  CHECK(source.find("VmIoHelpers.h") == std::string::npos);
  CHECK(source.find("primevm_main") == std::string::npos);
  CHECK(source.find("handleSharedVmControlFlowOpcode(") != std::string::npos);
  CHECK(source.find("handleVmNumericOpcode(") != std::string::npos);
}

TEST_SUITE_END();
