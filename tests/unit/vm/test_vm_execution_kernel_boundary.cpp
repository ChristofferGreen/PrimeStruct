#include "primec/VmExecutionKernel.h"
#include "primec/VmKernelBoundary.h"

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

  primec::IrInstruction numericInst = inst(primec::IrOpcode::AddI32);
  std::vector<std::uint64_t> stack = {38, 4};
  CHECK(primec::vm_kernel::executePureNumericOpcode(
            numericInst, stack, error) ==
        primec::vm_kernel::PureOpcodeResult::Continue);
  REQUIRE(stack.size() == 1);
  CHECK(stack.back() == 42);
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
      repoRoot / "src" / "runtime" / "VmExecutionKernel.cpp";
  const std::filesystem::path numericSharedPath =
      repoRoot / "src" / "runtime" / "VmNumericOpcodeShared.cpp";
  const std::filesystem::path kernelBoundaryHeaderPath =
      repoRoot / "include" / "primec" / "VmKernelBoundary.h";
  const std::filesystem::path kernelBoundarySourcePath =
      repoRoot / "src" / "runtime" / "VmKernelBoundary.cpp";
  REQUIRE(std::filesystem::exists(headerPath));
  REQUIRE(std::filesystem::exists(sourcePath));
  REQUIRE(std::filesystem::exists(numericSharedPath));
  REQUIRE(std::filesystem::exists(kernelBoundaryHeaderPath));
  REQUIRE(std::filesystem::exists(kernelBoundarySourcePath));

  const std::string header = readText(headerPath);
  const std::string source = readText(sourcePath);
  const std::string numericShared = readText(numericSharedPath);
  const std::string kernelBoundaryHeader = readText(kernelBoundaryHeaderPath);
  const std::string kernelBoundarySource = readText(kernelBoundarySourcePath);
  CHECK(header.find("primec/Vm.h") == std::string::npos);
  CHECK(header.find("VmDebug") == std::string::npos);
  CHECK(source.find("VmHeapHelpers.h") == std::string::npos);
  CHECK(source.find("VmIoHelpers.h") == std::string::npos);
  CHECK(source.find("primevm_main") == std::string::npos);
  CHECK(source.find("#include \"primec/VmKernelBoundary.h\"") !=
        std::string::npos);
  CHECK(source.find("vm_kernel::isPureNumericOpcode(op)") !=
        std::string::npos);
  CHECK(source.find("handleSharedVmControlFlowOpcode(") != std::string::npos);
  CHECK(source.find("handleVmNumericOpcode(") != std::string::npos);
  CHECK(numericShared.find("#include \"primec/VmKernelBoundary.h\"") !=
        std::string::npos);
  CHECK(numericShared.find("executePureNumericOpcode(inst, stack, error)") !=
        std::string::npos);
  CHECK(numericShared.find("case IrOpcode::AddI32:") == std::string::npos);
  CHECK(kernelBoundaryHeader.find("enum class PureOpcodeResult") !=
        std::string::npos);
  CHECK(kernelBoundarySource.find("case IrOpcode::AddI32:") !=
        std::string::npos);
  CHECK(kernelBoundarySource.find("case IrOpcode::CmpGtU64:") !=
        std::string::npos);
}

TEST_SUITE_END();
