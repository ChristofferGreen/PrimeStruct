#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string_view>
#include <vector>

#include "primec/IrBackends.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.backends");

namespace {

primec::IrModule makeReturnI32Module(int32_t value) {
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushI32, static_cast<uint64_t>(static_cast<uint32_t>(value))});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);
  return module;
}

std::vector<uint8_t> readBinaryFile(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  return std::vector<uint8_t>(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

} // namespace

TEST_CASE("ir backend registry reports deterministic order and lookup") {
  const std::vector<std::string_view> expectedKinds = {"vm", "native", "ir", "wasm"};
  CHECK(primec::listIrBackendKinds() == expectedKinds);

  for (std::string_view kind : expectedKinds) {
    const primec::IrBackend *backend = primec::findIrBackend(kind);
    REQUIRE(backend != nullptr);
    CHECK(backend->emitKind() == kind);
    CHECK(std::string_view(backend->diagnostics().backendTag) == kind);
  }

  CHECK(primec::findIrBackend("cpp") == nullptr);
  CHECK(primec::findIrBackend("glsl") == nullptr);
}

TEST_CASE("vm ir backend executes module and returns exit code") {
  const primec::IrBackend *backend = primec::findIrBackend("vm");
  REQUIRE(backend != nullptr);
  CHECK_FALSE(backend->requiresOutputPath());

  const primec::IrModule module = makeReturnI32Module(7);
  primec::IrBackendEmitOptions options;
  options.inputPath = "vm_backend_test.prime";
  options.programArgs = {"one", "two"};
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 7);
}

TEST_CASE("vm ir backend reports invalid ir entry index") {
  const primec::IrBackend *backend = primec::findIrBackend("vm");
  REQUIRE(backend != nullptr);

  primec::IrModule invalidModule;
  invalidModule.entryIndex = -1;
  primec::IrBackendEmitOptions options;
  options.inputPath = "vm_backend_invalid.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  CHECK_FALSE(backend->emit(invalidModule, options, result, error));
  CHECK(error.find("invalid IR entry index") != std::string::npos);
}

TEST_CASE("ir serializer backend writes psir magic header") {
  const primec::IrBackend *backend = primec::findIrBackend("ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  const primec::IrModule module = makeReturnI32Module(11);
  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_test.psir";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "ir_backend_test.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::vector<uint8_t> bytes = readBinaryFile(outputPath);
  REQUIRE(bytes.size() >= 4);
  const uint32_t magic = static_cast<uint32_t>(bytes[0]) | (static_cast<uint32_t>(bytes[1]) << 8) |
                         (static_cast<uint32_t>(bytes[2]) << 16) | (static_cast<uint32_t>(bytes[3]) << 24);
  CHECK(magic == 0x50534952u);
}

TEST_SUITE_END();
