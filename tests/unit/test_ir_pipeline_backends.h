#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
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

uint64_t f32ToBits(float value) {
  uint32_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  return static_cast<uint64_t>(bits);
}

uint64_t f64ToBits(double value) {
  uint64_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  return bits;
}

std::vector<uint8_t> readBinaryFile(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  return std::vector<uint8_t>(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

std::string readTextFile(const std::filesystem::path &path) {
  std::ifstream input(path);
  return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

} // namespace

TEST_CASE("ir backend registry reports deterministic order and lookup") {
  const std::vector<std::string_view> expectedKinds = {
      "vm", "native", "ir", "wasm", "glsl-ir", "spirv-ir", "cpp-ir", "exe-ir"};
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

TEST_CASE("main routes cpp/exe through ir backends without legacy fallback branch") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path mainPath = cwd / "src" / "main.cpp";
  if (!std::filesystem::exists(mainPath)) {
    mainPath = cwd.parent_path() / "src" / "main.cpp";
  }
  REQUIRE(std::filesystem::exists(mainPath));

  const std::string source = readTextFile(mainPath);
  CHECK(source.find("findIrBackend(\"cpp-ir\")") != std::string::npos);
  CHECK(source.find("findIrBackend(\"exe-ir\")") != std::string::npos);
  CHECK(source.find("const char *irFallbackKind") == std::string::npos);
  CHECK(source.find("emitter.emitCpp(program, options.entryPath)") == std::string::npos);
}

TEST_CASE("main routes glsl and spirv through ir backends without legacy fallback branches") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path mainPath = cwd / "src" / "main.cpp";
  if (!std::filesystem::exists(mainPath)) {
    mainPath = cwd.parent_path() / "src" / "main.cpp";
  }
  REQUIRE(std::filesystem::exists(mainPath));

  const std::string source = readTextFile(mainPath);
  CHECK(source.find("findIrBackend(\"glsl-ir\")") != std::string::npos);
  CHECK(source.find("findIrBackend(\"spirv-ir\")") != std::string::npos);
  CHECK(source.find("if (options.emitKind == \"glsl\")") == std::string::npos);
  CHECK(source.find("if (options.emitKind == \"spirv\")") == std::string::npos);
  CHECK(source.find("if (irFailure.stage != IrBackendRunFailureStage::Emit)") == std::string::npos);
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

TEST_CASE("cpp-ir backend writes C++ source") {
  const primec::IrBackend *backend = primec::findIrBackend("cpp-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  const primec::IrModule module = makeReturnI32Module(13);
  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_test.cpp";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "cpp_ir_backend_test.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("static int64_t ps_fn_0") != std::string::npos);
  CHECK(source.find("return static_cast<int>(ps_entry_0(argc, argv));") != std::string::npos);
}

TEST_CASE("cpp-ir backend writes u64 compare source with canonical bool literals") {
  const primec::IrBackend *backend = primec::findIrBackend("cpp-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushI64, static_cast<uint64_t>(9)});
  function.instructions.push_back({primec::IrOpcode::PushI64, static_cast<uint64_t>(7)});
  function.instructions.push_back({primec::IrOpcode::CmpGeU64, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_cmp_u64.cpp";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "cpp_ir_backend_cmp_u64.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("uint64_t right = stack[--sp];") != std::string::npos);
  CHECK(source.find("uint64_t left = stack[--sp];") != std::string::npos);
  CHECK(source.find("stack[sp++] = (left >= right) ? 1u : 0u;") != std::string::npos);
}

TEST_CASE("cpp-ir backend rejects empty non-entry function bodies") {
  const primec::IrBackend *backend = primec::findIrBackend("cpp-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction entry;
  entry.name = "/main";
  entry.instructions.push_back({primec::IrOpcode::ReturnVoid, 0});
  module.functions.push_back(entry);

  primec::IrFunction helper;
  helper.name = "/helper";
  module.functions.push_back(helper);

  primec::IrBackendEmitOptions options;
  options.outputPath = (std::filesystem::current_path() / "primec_tests" / "unused_empty_helper.cpp").string();
  options.inputPath = "cpp_ir_backend_empty_helper.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  CHECK_FALSE(backend->emit(module, options, result, error));
  CHECK(error == "ir-to-cpp failed: IrToCppEmitter function has no instructions at index 1");
}

TEST_CASE("cpp-ir backend reports file write string index diagnostics") {
  const primec::IrBackend *backend = primec::findIrBackend("cpp-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  module.stringTable.push_back("/dev/null");
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::FileOpenWrite, 0});
  function.instructions.push_back({primec::IrOpcode::FileWriteString, 5});
  function.instructions.push_back({primec::IrOpcode::ReturnVoid, 0});
  module.functions.push_back(function);

  const std::filesystem::path outputPath =
      std::filesystem::current_path() / "primec_tests" / "unused_file_write_string_index.cpp";
  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "cpp_ir_backend_file_write_string_index_error.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  CHECK_FALSE(backend->emit(module, options, result, error));
  CHECK(error.find("ir-to-cpp failed:") != std::string::npos);
  CHECK(error.find("string index out of range") != std::string::npos);
}

TEST_CASE("cpp-ir backend reports file open string index diagnostics") {
  const primec::IrBackend *backend = primec::findIrBackend("cpp-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::FileOpenWrite, 5});
  function.instructions.push_back({primec::IrOpcode::ReturnVoid, 0});
  module.functions.push_back(function);

  const std::filesystem::path outputPath =
      std::filesystem::current_path() / "primec_tests" / "unused_file_open_string_index.cpp";
  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "cpp_ir_backend_file_open_string_index_error.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  CHECK_FALSE(backend->emit(module, options, result, error));
  CHECK(error.find("ir-to-cpp failed:") != std::string::npos);
  CHECK(error.find("string index out of range") != std::string::npos);
}

TEST_CASE("cpp-ir backend reports call target range diagnostics") {
  const primec::IrBackend *backend = primec::findIrBackend("cpp-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::Call, 3});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path outputPath =
      std::filesystem::current_path() / "primec_tests" / "unused_call_target_range.cpp";
  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "cpp_ir_backend_call_target_range_error.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  CHECK_FALSE(backend->emit(module, options, result, error));
  CHECK(error.find("ir-to-cpp failed:") != std::string::npos);
  CHECK(error.find("call target out of range") != std::string::npos);
}

TEST_CASE("cpp-ir backend reports jump target range diagnostics") {
  const primec::IrBackend *backend = primec::findIrBackend("cpp-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushI32, 0});
  function.instructions.push_back({primec::IrOpcode::JumpIfZero, 7});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path outputPath =
      std::filesystem::current_path() / "primec_tests" / "unused_jump_target_range.cpp";
  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "cpp_ir_backend_jump_target_range_error.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  CHECK_FALSE(backend->emit(module, options, result, error));
  CHECK(error.find("ir-to-cpp failed:") != std::string::npos);
  CHECK(error.find("jump target out of range") != std::string::npos);
}

TEST_CASE("cpp-ir backend reports unconditional jump target range diagnostics") {
  const primec::IrBackend *backend = primec::findIrBackend("cpp-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::Jump, 7});
  function.instructions.push_back({primec::IrOpcode::ReturnVoid, 0});
  module.functions.push_back(function);

  const std::filesystem::path outputPath =
      std::filesystem::current_path() / "primec_tests" / "unused_unconditional_jump_target_range.cpp";
  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "cpp_ir_backend_unconditional_jump_target_range_error.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  CHECK_FALSE(backend->emit(module, options, result, error));
  CHECK(error.find("ir-to-cpp failed:") != std::string::npos);
  CHECK(error.find("jump target out of range") != std::string::npos);
}

TEST_CASE("cpp-ir backend reports unsupported opcode diagnostics") {
  const primec::IrBackend *backend = primec::findIrBackend("cpp-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({static_cast<primec::IrOpcode>(0), 0});
  function.instructions.push_back({primec::IrOpcode::ReturnVoid, 0});
  module.functions.push_back(function);

  const std::filesystem::path outputPath =
      std::filesystem::current_path() / "primec_tests" / "unused_unsupported_opcode.cpp";
  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "cpp_ir_backend_unsupported_opcode_error.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  CHECK_FALSE(backend->emit(module, options, result, error));
  CHECK(error.find("ir-to-cpp failed:") != std::string::npos);
  CHECK(error.find("unsupported opcode") != std::string::npos);
}

TEST_CASE("cpp-ir backend reports print string index diagnostics") {
  const primec::IrBackend *backend = primec::findIrBackend("cpp-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back(
      {primec::IrOpcode::PrintString, primec::encodePrintStringImm(3, primec::encodePrintFlags(true, false))});
  function.instructions.push_back({primec::IrOpcode::ReturnVoid, 0});
  module.functions.push_back(function);

  const std::filesystem::path outputPath =
      std::filesystem::current_path() / "primec_tests" / "unused_print_string_index.cpp";
  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "cpp_ir_backend_print_string_index_error.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  CHECK_FALSE(backend->emit(module, options, result, error));
  CHECK(error.find("ir-to-cpp failed:") != std::string::npos);
  CHECK(error.find("string index out of range") != std::string::npos);
}

TEST_CASE("cpp-ir backend reports string byte load index diagnostics") {
  const primec::IrBackend *backend = primec::findIrBackend("cpp-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushI32, 0});
  function.instructions.push_back({primec::IrOpcode::LoadStringByte, 3});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path outputPath =
      std::filesystem::current_path() / "primec_tests" / "unused_string_byte_index.cpp";
  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "cpp_ir_backend_string_byte_index_error.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  CHECK_FALSE(backend->emit(module, options, result, error));
  CHECK(error.find("ir-to-cpp failed:") != std::string::npos);
  CHECK(error.find("string index out of range") != std::string::npos);
}

TEST_CASE("cpp-ir backend writes f32 opcode helpers") {
  const primec::IrBackend *backend = primec::findIrBackend("cpp-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushF32, f32ToBits(2.0f)});
  function.instructions.push_back({primec::IrOpcode::PushF32, f32ToBits(0.5f)});
  function.instructions.push_back({primec::IrOpcode::AddF32, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnF32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_f32.cpp";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "cpp_ir_backend_f32.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("#include <cstring>") != std::string::npos);
  CHECK(source.find("static float psBitsToF32(uint64_t raw)") != std::string::npos);
  CHECK(source.find("static uint64_t psF32ToBits(float value)") != std::string::npos);
  CHECK(source.find("float right = psBitsToF32(stack[--sp]);") != std::string::npos);
}

TEST_CASE("cpp-ir backend writes string byte load paths") {
  const primec::IrBackend *backend = primec::findIrBackend("cpp-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  module.stringTable.push_back("abc");
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushI32, static_cast<uint64_t>(1)});
  function.instructions.push_back({primec::IrOpcode::LoadStringByte, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_string_byte.cpp";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "cpp_ir_backend_string_byte.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("if (stringByteIndex >= 3ull)") != std::string::npos);
  CHECK(source.find("ps_string_table[0][stringByteIndex]") != std::string::npos);
}

TEST_CASE("cpp-ir backend writes indirect local pointer paths") {
  const primec::IrBackend *backend = primec::findIrBackend("cpp-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushI32, static_cast<uint64_t>(3)});
  function.instructions.push_back({primec::IrOpcode::StoreLocal, 0});
  function.instructions.push_back({primec::IrOpcode::AddressOfLocal, 0});
  function.instructions.push_back({primec::IrOpcode::PushI32, static_cast<uint64_t>(8)});
  function.instructions.push_back({primec::IrOpcode::StoreIndirect, 0});
  function.instructions.push_back({primec::IrOpcode::AddressOfLocal, 0});
  function.instructions.push_back({primec::IrOpcode::LoadIndirect, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_pointer.cpp";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "cpp_ir_backend_pointer.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("stack[sp++] = static_cast<uint64_t>(0ull * 16ull);") != std::string::npos);
  CHECK(source.find("loadIndirectAddress % 16ull") != std::string::npos);
  CHECK(source.find("storeIndirectAddress % 16ull") != std::string::npos);
}

TEST_CASE("cpp-ir backend writes file io paths") {
  const primec::IrBackend *backend = primec::findIrBackend("cpp-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  module.stringTable.push_back("/tmp/ir_backend_registry_file_io.txt");
  module.stringTable.push_back("hello");
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::FileOpenWrite, 0});
  function.instructions.push_back({primec::IrOpcode::Dup, 0});
  function.instructions.push_back({primec::IrOpcode::FileWriteString, 1});
  function.instructions.push_back({primec::IrOpcode::Pop, 0});
  function.instructions.push_back({primec::IrOpcode::Dup, 0});
  function.instructions.push_back({primec::IrOpcode::FileFlush, 0});
  function.instructions.push_back({primec::IrOpcode::Pop, 0});
  function.instructions.push_back({primec::IrOpcode::FileClose, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_file_io.cpp";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "cpp_ir_backend_file_io.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("static uint32_t psWriteAll(int fd, const void *data, std::size_t size)") != std::string::npos);
  CHECK(source.find("int fileFd = ::open(ps_string_table[0], fileOpenFlags, 0644);") != std::string::npos);
  CHECK(source.find("int flushRc = ::fsync(flushFd);") != std::string::npos);
  CHECK(source.find("int closeRc = ::close(closeFd);") != std::string::npos);
}

TEST_CASE("cpp-ir backend writes file write u64 path") {
  const primec::IrBackend *backend = primec::findIrBackend("cpp-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  module.stringTable.push_back("/dev/null");
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::FileOpenWrite, 0});
  function.instructions.push_back({primec::IrOpcode::PushI64, static_cast<uint64_t>(123)});
  function.instructions.push_back({primec::IrOpcode::FileWriteU64, 0});
  function.instructions.push_back({primec::IrOpcode::Pop, 0});
  function.instructions.push_back({primec::IrOpcode::FileClose, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_file_write_u64.cpp";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "cpp_ir_backend_file_write_u64.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("uint64_t fileU64Value = stack[--sp];") != std::string::npos);
  CHECK(source.find("int fileU64Fd = static_cast<int>(fileU64Handle & 0xffffffffu);") != std::string::npos);
  CHECK(source.find("std::string fileU64Text = std::to_string(static_cast<unsigned long long>(fileU64Value));") !=
        std::string::npos);
  CHECK(source.find("psWriteAll(fileU64Fd, fileU64Text.data(), fileU64Text.size())") != std::string::npos);
}

TEST_CASE("cpp-ir backend writes file write i32 path") {
  const primec::IrBackend *backend = primec::findIrBackend("cpp-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  module.stringTable.push_back("/dev/null");
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::FileOpenWrite, 0});
  function.instructions.push_back({primec::IrOpcode::PushI32, static_cast<uint64_t>(-17)});
  function.instructions.push_back({primec::IrOpcode::FileWriteI32, 0});
  function.instructions.push_back({primec::IrOpcode::Pop, 0});
  function.instructions.push_back({primec::IrOpcode::FileClose, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_file_write_i32.cpp";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "cpp_ir_backend_file_write_i32.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("int64_t fileI32Value = static_cast<int64_t>(static_cast<int32_t>(stack[--sp]));") !=
        std::string::npos);
  CHECK(source.find("int fileI32Fd = static_cast<int>(fileI32Handle & 0xffffffffu);") != std::string::npos);
  CHECK(source.find("std::string fileI32Text = std::to_string(fileI32Value);") != std::string::npos);
  CHECK(source.find("psWriteAll(fileI32Fd, fileI32Text.data(), fileI32Text.size())") != std::string::npos);
}

TEST_CASE("cpp-ir backend writes file write i64 path") {
  const primec::IrBackend *backend = primec::findIrBackend("cpp-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  module.stringTable.push_back("/dev/null");
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::FileOpenWrite, 0});
  function.instructions.push_back({primec::IrOpcode::PushI64, static_cast<uint64_t>(-19)});
  function.instructions.push_back({primec::IrOpcode::FileWriteI64, 0});
  function.instructions.push_back({primec::IrOpcode::Pop, 0});
  function.instructions.push_back({primec::IrOpcode::FileClose, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_file_write_i64.cpp";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "cpp_ir_backend_file_write_i64.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("int64_t fileI64Value = static_cast<int64_t>(stack[--sp]);") != std::string::npos);
  CHECK(source.find("int fileI64Fd = static_cast<int>(fileI64Handle & 0xffffffffu);") != std::string::npos);
  CHECK(source.find("std::string fileI64Text = std::to_string(fileI64Value);") != std::string::npos);
  CHECK(source.find("psWriteAll(fileI64Fd, fileI64Text.data(), fileI64Text.size())") != std::string::npos);
}

TEST_CASE("cpp-ir backend writes file write byte path") {
  const primec::IrBackend *backend = primec::findIrBackend("cpp-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  module.stringTable.push_back("/dev/null");
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::FileOpenWrite, 0});
  function.instructions.push_back({primec::IrOpcode::PushI32, static_cast<uint64_t>(0x41)});
  function.instructions.push_back({primec::IrOpcode::FileWriteByte, 0});
  function.instructions.push_back({primec::IrOpcode::Pop, 0});
  function.instructions.push_back({primec::IrOpcode::FileClose, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_file_write_byte.cpp";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "cpp_ir_backend_file_write_byte.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("uint8_t fileByteValue = static_cast<uint8_t>(stack[--sp] & 0xffu);") != std::string::npos);
  CHECK(source.find("int fileByteFd = static_cast<int>(fileByteHandle & 0xffffffffu);") != std::string::npos);
  CHECK(source.find("psWriteAll(fileByteFd, &fileByteValue, 1)") != std::string::npos);
}

TEST_CASE("cpp-ir backend writes file write newline path") {
  const primec::IrBackend *backend = primec::findIrBackend("cpp-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  module.stringTable.push_back("/dev/null");
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::FileOpenWrite, 0});
  function.instructions.push_back({primec::IrOpcode::FileWriteNewline, 0});
  function.instructions.push_back({primec::IrOpcode::Pop, 0});
  function.instructions.push_back({primec::IrOpcode::FileClose, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_file_write_newline.cpp";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "cpp_ir_backend_file_write_newline.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("int fileLineFd = static_cast<int>(fileLineHandle & 0xffffffffu);") != std::string::npos);
  CHECK(source.find("char fileLineValue = '\\n';") != std::string::npos);
  CHECK(source.find("psWriteAll(fileLineFd, &fileLineValue, 1)") != std::string::npos);
}

TEST_CASE("cpp-ir backend writes file write string path") {
  const primec::IrBackend *backend = primec::findIrBackend("cpp-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  module.stringTable.push_back("/dev/null");
  module.stringTable.push_back("hello");
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::FileOpenWrite, 0});
  function.instructions.push_back({primec::IrOpcode::FileWriteString, 1});
  function.instructions.push_back({primec::IrOpcode::Pop, 0});
  function.instructions.push_back({primec::IrOpcode::FileClose, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_file_write_string.cpp";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "cpp_ir_backend_file_write_string.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("int fileStringFd = static_cast<int>(fileStringHandle & 0xffffffffu);") != std::string::npos);
  CHECK(source.find("psWriteAll(fileStringFd, ps_string_table[1], 5ull)") != std::string::npos);
}

TEST_CASE("cpp-ir backend writes file read path") {
  const primec::IrBackend *backend = primec::findIrBackend("cpp-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  module.stringTable.push_back("/dev/null");
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::FileOpenRead, 0});
  function.instructions.push_back({primec::IrOpcode::FileClose, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_file_read.cpp";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "cpp_ir_backend_file_read.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("int fileOpenFlags = O_RDONLY;") != std::string::npos);
  CHECK(source.find("int fileFd = ::open(ps_string_table[0], fileOpenFlags, 0644);") != std::string::npos);
  CHECK(source.find("int closeRc = ::close(closeFd);") != std::string::npos);
}

TEST_CASE("cpp-ir backend writes file write path") {
  const primec::IrBackend *backend = primec::findIrBackend("cpp-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  module.stringTable.push_back("/dev/null");
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::FileOpenWrite, 0});
  function.instructions.push_back({primec::IrOpcode::FileClose, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_file_write.cpp";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "cpp_ir_backend_file_write.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("int fileOpenFlags = O_WRONLY | O_CREAT | O_TRUNC;") != std::string::npos);
  CHECK(source.find("int fileFd = ::open(ps_string_table[0], fileOpenFlags, 0644);") != std::string::npos);
  CHECK(source.find("int closeRc = ::close(closeFd);") != std::string::npos);
}

TEST_CASE("cpp-ir backend writes file append path") {
  const primec::IrBackend *backend = primec::findIrBackend("cpp-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  module.stringTable.push_back("/dev/null");
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::FileOpenAppend, 0});
  function.instructions.push_back({primec::IrOpcode::FileClose, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_file_append.cpp";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "cpp_ir_backend_file_append.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("int fileOpenFlags = O_WRONLY | O_CREAT | O_APPEND;") != std::string::npos);
  CHECK(source.find("int fileFd = ::open(ps_string_table[0], fileOpenFlags, 0644);") != std::string::npos);
  CHECK(source.find("int closeRc = ::close(closeFd);") != std::string::npos);
}

TEST_CASE("cpp-ir backend omits file io helpers when unused") {
  const primec::IrBackend *backend = primec::findIrBackend("cpp-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushI32, static_cast<uint64_t>(5)});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_file_io_unused.cpp";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "cpp_ir_backend_file_io_unused.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("#include <cerrno>") == std::string::npos);
  CHECK(source.find("#include <fcntl.h>") == std::string::npos);
  CHECK(source.find("#include <unistd.h>") == std::string::npos);
  CHECK(source.find("static uint32_t psWriteAll(int fd, const void *data, std::size_t size)") ==
        std::string::npos);
}

TEST_CASE("cpp-ir backend writes f64 compare helpers") {
  const primec::IrBackend *backend = primec::findIrBackend("cpp-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushF64, f64ToBits(2.5)});
  function.instructions.push_back({primec::IrOpcode::PushF64, f64ToBits(1.0)});
  function.instructions.push_back({primec::IrOpcode::CmpGtF64, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_f64_cmp.cpp";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "cpp_ir_backend_f64_cmp.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("static double psBitsToF64(uint64_t raw)") != std::string::npos);
  CHECK(source.find("double right = psBitsToF64(stack[--sp]);") != std::string::npos);
}

TEST_CASE("cpp-ir backend writes f64 arithmetic helpers") {
  const primec::IrBackend *backend = primec::findIrBackend("cpp-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushF64, f64ToBits(3.0)});
  function.instructions.push_back({primec::IrOpcode::PushF64, f64ToBits(0.5)});
  function.instructions.push_back({primec::IrOpcode::AddF64, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnF64, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_f64_math.cpp";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "cpp_ir_backend_f64_math.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("static uint64_t psF64ToBits(double value)") != std::string::npos);
  CHECK(source.find("stack[sp++] = psF64ToBits(left + right);") != std::string::npos);
}

TEST_CASE("cpp-ir backend writes f64 conversion helpers") {
  const primec::IrBackend *backend = primec::findIrBackend("cpp-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushI32, static_cast<uint64_t>(7)});
  function.instructions.push_back({primec::IrOpcode::ConvertI32ToF64, 0});
  function.instructions.push_back({primec::IrOpcode::ConvertF64ToF32, 0});
  function.instructions.push_back({primec::IrOpcode::ConvertF32ToF64, 0});
  function.instructions.push_back({primec::IrOpcode::ConvertF64ToI32, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_f64_convert.cpp";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "cpp_ir_backend_f64_convert.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("#include <cmath>") != std::string::npos);
  CHECK(source.find("#include <limits>") != std::string::npos);
  CHECK(source.find("static int32_t psConvertF32ToI32(float value)") != std::string::npos);
  CHECK(source.find("static int32_t psConvertF64ToI32(double value)") != std::string::npos);
  CHECK(source.find("stack[sp++] = psF64ToBits(static_cast<double>(value));") != std::string::npos);
  CHECK(source.find("stack[sp++] = psF32ToBits(static_cast<float>(value));") != std::string::npos);
  CHECK(source.find("int32_t converted = psConvertF64ToI32(value);") != std::string::npos);
}

TEST_CASE("cpp-ir backend omits i32 float conversion clamp helpers when unused") {
  const primec::IrBackend *backend = primec::findIrBackend("cpp-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushI32, 5});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_convert_i32_unused.cpp";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "cpp_ir_backend_convert_i32_unused.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("#include <cmath>") == std::string::npos);
  CHECK(source.find("#include <limits>") == std::string::npos);
  CHECK(source.find("static int32_t psConvertF32ToI32(float value)") == std::string::npos);
  CHECK(source.find("static int32_t psConvertF64ToI32(double value)") == std::string::npos);
}

TEST_CASE("cpp-ir backend writes u64 float conversion clamp helpers") {
  const primec::IrBackend *backend = primec::findIrBackend("cpp-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushF32, f32ToBits(1.5f)});
  function.instructions.push_back({primec::IrOpcode::ConvertF32ToU64, 0});
  function.instructions.push_back({primec::IrOpcode::Pop, 0});
  function.instructions.push_back({primec::IrOpcode::PushF64, f64ToBits(2.5)});
  function.instructions.push_back({primec::IrOpcode::ConvertF64ToU64, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI64, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_f64_convert_u64.cpp";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "cpp_ir_backend_f64_convert_u64.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("#include <cmath>") != std::string::npos);
  CHECK(source.find("#include <limits>") != std::string::npos);
  CHECK(source.find("static uint64_t psConvertF32ToU64(float value)") != std::string::npos);
  CHECK(source.find("static uint64_t psConvertF64ToU64(double value)") != std::string::npos);
  CHECK(source.find("uint64_t converted = psConvertF32ToU64(value);") != std::string::npos);
  CHECK(source.find("uint64_t converted = psConvertF64ToU64(value);") != std::string::npos);
}

TEST_CASE("cpp-ir backend omits u64 float conversion clamp helpers when unused") {
  const primec::IrBackend *backend = primec::findIrBackend("cpp-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushI32, 5});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_convert_u64_unused.cpp";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "cpp_ir_backend_convert_u64_unused.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("#include <cmath>") == std::string::npos);
  CHECK(source.find("#include <limits>") == std::string::npos);
  CHECK(source.find("static uint64_t psConvertF32ToU64(float value)") == std::string::npos);
  CHECK(source.find("static uint64_t psConvertF64ToU64(double value)") == std::string::npos);
}

TEST_CASE("cpp-ir backend writes i64 float conversion clamp helpers") {
  const primec::IrBackend *backend = primec::findIrBackend("cpp-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushF32, f32ToBits(1.5f)});
  function.instructions.push_back({primec::IrOpcode::ConvertF32ToI64, 0});
  function.instructions.push_back({primec::IrOpcode::Pop, 0});
  function.instructions.push_back({primec::IrOpcode::PushF64, f64ToBits(2.5)});
  function.instructions.push_back({primec::IrOpcode::ConvertF64ToI64, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI64, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_f64_convert_i64.cpp";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "cpp_ir_backend_f64_convert_i64.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("#include <cmath>") != std::string::npos);
  CHECK(source.find("#include <limits>") != std::string::npos);
  CHECK(source.find("static int64_t psConvertF32ToI64(float value)") != std::string::npos);
  CHECK(source.find("static int64_t psConvertF64ToI64(double value)") != std::string::npos);
  CHECK(source.find("int64_t converted = psConvertF32ToI64(value);") != std::string::npos);
  CHECK(source.find("int64_t converted = psConvertF64ToI64(value);") != std::string::npos);
}

TEST_CASE("cpp-ir backend omits i64 float conversion clamp helpers when unused") {
  const primec::IrBackend *backend = primec::findIrBackend("cpp-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushI32, 9});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_convert_i64_unused.cpp";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "cpp_ir_backend_convert_i64_unused.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("#include <cmath>") == std::string::npos);
  CHECK(source.find("#include <limits>") == std::string::npos);
  CHECK(source.find("static int64_t psConvertF32ToI64(float value)") == std::string::npos);
  CHECK(source.find("static int64_t psConvertF64ToI64(double value)") == std::string::npos);
}

TEST_CASE("glsl-ir backend writes GLSL source") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  const primec::IrModule module = makeReturnI32Module(17);
  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_test.glsl";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "glsl_ir_backend_test.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("#version 450") != std::string::npos);
  CHECK(source.find("ps_output.value = ps_entry_0(stack, sp);") != std::string::npos);
}

TEST_CASE("glsl-ir backend writes loadstringbyte source") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  module.stringTable.push_back("AB");
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushI32, 1});
  function.instructions.push_back({primec::IrOpcode::LoadStringByte, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_string_byte.glsl";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "glsl_ir_backend_string_byte.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("switch (stringByteIndex)") != std::string::npos);
  CHECK(source.find("case 0u: stringByte = 65; break;") != std::string::npos);
  CHECK(source.find("case 1u: stringByte = 66; break;") != std::string::npos);
}

TEST_CASE("glsl-ir backend writes pushi64 source for i32-range literals") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushI64, static_cast<uint64_t>(static_cast<int64_t>(31))});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_push_i64.glsl";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "glsl_ir_backend_push_i64.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("stack[sp++] = 31;") != std::string::npos);
}

TEST_CASE("glsl-ir backend writes i64 compare source for narrowed values") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushI64, static_cast<uint64_t>(static_cast<int64_t>(4))});
  function.instructions.push_back({primec::IrOpcode::PushI64, static_cast<uint64_t>(static_cast<int64_t>(4))});
  function.instructions.push_back({primec::IrOpcode::CmpEqI64, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI64, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_cmp_i64.glsl";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "glsl_ir_backend_cmp_i64.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("stack[sp++] = (left == right) ? 1 : 0;") != std::string::npos);
  CHECK(source.find("return stack[--sp];") != std::string::npos);
}

TEST_CASE("glsl-ir backend writes i64 arithmetic source for narrowed values") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushI64, static_cast<uint64_t>(static_cast<int64_t>(8))});
  function.instructions.push_back({primec::IrOpcode::PushI64, static_cast<uint64_t>(static_cast<int64_t>(3))});
  function.instructions.push_back({primec::IrOpcode::AddI64, 0});
  function.instructions.push_back({primec::IrOpcode::NegI64, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI64, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_arith_i64.glsl";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "glsl_ir_backend_arith_i64.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("stack[sp++] = left + right;") != std::string::npos);
  CHECK(source.find("stack[sp - 1] = -stack[sp - 1];") != std::string::npos);
}

TEST_CASE("glsl-ir backend writes u64 compare source for narrowed values") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushI64, static_cast<uint64_t>(static_cast<int64_t>(2))});
  function.instructions.push_back({primec::IrOpcode::PushI64, static_cast<uint64_t>(static_cast<int64_t>(9))});
  function.instructions.push_back({primec::IrOpcode::CmpLtU64, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_cmp_u64.glsl";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "glsl_ir_backend_cmp_u64.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("uint right = uint(stack[--sp]);") != std::string::npos);
  CHECK(source.find("uint left = uint(stack[--sp]);") != std::string::npos);
  CHECK(source.find("stack[sp++] = (left < right) ? 1 : 0;") != std::string::npos);
}

TEST_CASE("glsl-ir backend writes u64 division source for narrowed values") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushI64, static_cast<uint64_t>(static_cast<int64_t>(10))});
  function.instructions.push_back({primec::IrOpcode::PushI64, static_cast<uint64_t>(static_cast<int64_t>(3))});
  function.instructions.push_back({primec::IrOpcode::DivU64, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_div_u64.glsl";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "glsl_ir_backend_div_u64.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("uint right = uint(stack[--sp]);") != std::string::npos);
  CHECK(source.find("uint left = uint(stack[--sp]);") != std::string::npos);
  CHECK(source.find("stack[sp++] = int(left / right);") != std::string::npos);
}

TEST_CASE("glsl-ir backend writes i64/u64 to f32 conversion source") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushI64, static_cast<uint64_t>(static_cast<int64_t>(4))});
  function.instructions.push_back({primec::IrOpcode::ConvertI64ToF32, 0});
  function.instructions.push_back({primec::IrOpcode::Pop, 0});
  function.instructions.push_back({primec::IrOpcode::PushI64, static_cast<uint64_t>(static_cast<int64_t>(5))});
  function.instructions.push_back({primec::IrOpcode::ConvertU64ToF32, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_convert_i64_u64_f32.glsl";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "glsl_ir_backend_convert_i64_u64_f32.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("stack[sp++] = floatBitsToInt(float(stack[--sp]));") != std::string::npos);
  CHECK(source.find("stack[sp++] = floatBitsToInt(float(uint(stack[--sp])));") != std::string::npos);
}

TEST_CASE("glsl-ir backend writes f32 to i64 conversion source") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushF32, 0x41200000u});
  function.instructions.push_back({primec::IrOpcode::ConvertF32ToI64, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI64, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_convert_f32_i64.glsl";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "glsl_ir_backend_convert_f32_i64.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("float value = intBitsToFloat(stack[--sp]);") != std::string::npos);
  CHECK(source.find("converted = int(value);") != std::string::npos);
}

TEST_CASE("glsl-ir backend writes f32 to u64 conversion source") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushF32, 0x41200000u});
  function.instructions.push_back({primec::IrOpcode::ConvertF32ToU64, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI64, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_convert_f32_u64.glsl";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "glsl_ir_backend_convert_f32_u64.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("uint converted = 0u;") != std::string::npos);
  CHECK(source.find("converted = uint(int(value));") != std::string::npos);
  CHECK(source.find("stack[sp++] = int(converted);") != std::string::npos);
}

TEST_CASE("glsl-ir backend writes f32/f64 passthrough conversion source") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushF32, 0x3f800000u});
  function.instructions.push_back({primec::IrOpcode::ConvertF32ToF64, 0});
  function.instructions.push_back({primec::IrOpcode::ConvertF64ToF32, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_convert_f32_f64.glsl";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "glsl_ir_backend_convert_f32_f64.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("// Narrowed GLSL path keeps f32/f64 conversion as bit-preserving passthrough.") !=
        std::string::npos);
  CHECK(source.find("// Narrowed GLSL path keeps f64/f32 conversion as bit-preserving passthrough.") !=
        std::string::npos);
}

TEST_CASE("glsl-ir backend writes i32/f64 narrowed conversion source") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushI32, 11});
  function.instructions.push_back({primec::IrOpcode::ConvertI32ToF64, 0});
  function.instructions.push_back({primec::IrOpcode::ConvertF64ToI32, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_convert_i32_f64.glsl";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "glsl_ir_backend_convert_i32_f64.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("// Narrowed GLSL path lowers i32/f64 conversion through f32 payloads.") != std::string::npos);
  CHECK(source.find("// Narrowed GLSL path lowers f64/i32 conversion through f32 payloads.") != std::string::npos);
}

TEST_CASE("glsl-ir backend writes i64/u64 to f64 narrowed conversion source") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushI64, static_cast<uint64_t>(static_cast<int64_t>(8))});
  function.instructions.push_back({primec::IrOpcode::ConvertI64ToF64, 0});
  function.instructions.push_back({primec::IrOpcode::Pop, 0});
  function.instructions.push_back({primec::IrOpcode::PushI64, static_cast<uint64_t>(static_cast<int64_t>(9))});
  function.instructions.push_back({primec::IrOpcode::ConvertU64ToF64, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_convert_i64_u64_f64.glsl";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "glsl_ir_backend_convert_i64_u64_f64.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("// Narrowed GLSL path lowers i64/f64 conversion through f32 payloads.") != std::string::npos);
  CHECK(source.find("// Narrowed GLSL path lowers u64/f64 conversion through f32 payloads.") != std::string::npos);
}

TEST_CASE("glsl-ir backend writes f64 to i64/u64 narrowed conversion source") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushF32, 0x40c00000u});
  function.instructions.push_back({primec::IrOpcode::ConvertF32ToF64, 0});
  function.instructions.push_back({primec::IrOpcode::ConvertF64ToI64, 0});
  function.instructions.push_back({primec::IrOpcode::Pop, 0});
  function.instructions.push_back({primec::IrOpcode::PushF32, 0x41100000u});
  function.instructions.push_back({primec::IrOpcode::ConvertF32ToF64, 0});
  function.instructions.push_back({primec::IrOpcode::ConvertF64ToU64, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI64, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_convert_f64_i64_u64.glsl";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "glsl_ir_backend_convert_f64_i64_u64.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("// Narrowed GLSL path lowers f64/i64 conversion through f32 payloads.") != std::string::npos);
  CHECK(source.find("// Narrowed GLSL path lowers f64/u64 conversion through f32 payloads.") != std::string::npos);
  CHECK(source.find("uint converted = 0u;") != std::string::npos);
}

TEST_CASE("glsl-ir backend writes narrowed f64 literal and return source") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushF64, 0x3ff8000000000000ull});
  function.instructions.push_back({primec::IrOpcode::ReturnF64, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_push_return_f64.glsl";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "glsl_ir_backend_push_return_f64.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("// Narrowed GLSL path lowers f64 literals through f32 payloads.") != std::string::npos);
  CHECK(source.find("stack[sp++] = int(uint(1069547520u));") != std::string::npos);
  CHECK(source.find("// Narrowed GLSL path returns f64 values through f32 payloads.") != std::string::npos);
}

TEST_CASE("glsl-ir backend writes narrowed f64 add source") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushF64, 0x3ff8000000000000ull});
  function.instructions.push_back({primec::IrOpcode::PushF64, 0x4000000000000000ull});
  function.instructions.push_back({primec::IrOpcode::AddF64, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnF64, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_add_f64.glsl";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "glsl_ir_backend_add_f64.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("// Narrowed GLSL path lowers f64 add through f32 payloads.") != std::string::npos);
  CHECK(source.find("stack[sp++] = floatBitsToInt(left + right);") != std::string::npos);
}

TEST_CASE("glsl-ir backend writes narrowed f64 sub source") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushF64, 0x4004000000000000ull});
  function.instructions.push_back({primec::IrOpcode::PushF64, 0x3ff8000000000000ull});
  function.instructions.push_back({primec::IrOpcode::SubF64, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnF64, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_sub_f64.glsl";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "glsl_ir_backend_sub_f64.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("// Narrowed GLSL path lowers f64 sub through f32 payloads.") != std::string::npos);
  CHECK(source.find("stack[sp++] = floatBitsToInt(left - right);") != std::string::npos);
}

TEST_CASE("glsl-ir backend writes narrowed f64 mul source") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushF64, 0x3ff8000000000000ull});
  function.instructions.push_back({primec::IrOpcode::PushF64, 0x4010000000000000ull});
  function.instructions.push_back({primec::IrOpcode::MulF64, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnF64, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_mul_f64.glsl";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "glsl_ir_backend_mul_f64.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("// Narrowed GLSL path lowers f64 mul through f32 payloads.") != std::string::npos);
  CHECK(source.find("stack[sp++] = floatBitsToInt(left * right);") != std::string::npos);
}

TEST_CASE("glsl-ir backend writes narrowed f64 div source") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushF64, 0x4014000000000000ull});
  function.instructions.push_back({primec::IrOpcode::PushF64, 0x4000000000000000ull});
  function.instructions.push_back({primec::IrOpcode::DivF64, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnF64, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_div_f64.glsl";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "glsl_ir_backend_div_f64.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("// Narrowed GLSL path lowers f64 div through f32 payloads.") != std::string::npos);
  CHECK(source.find("stack[sp++] = floatBitsToInt(left / right);") != std::string::npos);
}

TEST_CASE("glsl-ir backend writes narrowed f64 neg source") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushF64, 0x3ff8000000000000ull});
  function.instructions.push_back({primec::IrOpcode::NegF64, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnF64, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_neg_f64.glsl";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "glsl_ir_backend_neg_f64.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("// Narrowed GLSL path lowers f64 neg through f32 payloads.") != std::string::npos);
  CHECK(source.find("stack[sp - 1] = floatBitsToInt(-value);") != std::string::npos);
}

TEST_CASE("glsl-ir backend writes narrowed f64 equality compare source") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushF64, 0x3ff8000000000000ull});
  function.instructions.push_back({primec::IrOpcode::PushF64, 0x3ff8000000000000ull});
  function.instructions.push_back({primec::IrOpcode::CmpEqF64, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_cmp_eq_f64.glsl";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "glsl_ir_backend_cmp_eq_f64.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("// Narrowed GLSL path lowers f64 equality compare through f32 payloads.") != std::string::npos);
  CHECK(source.find("stack[sp++] = (left == right) ? 1 : 0;") != std::string::npos);
}

TEST_CASE("glsl-ir backend writes narrowed f64 inequality compare source") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushF64, 0x3ff8000000000000ull});
  function.instructions.push_back({primec::IrOpcode::PushF64, 0x4000000000000000ull});
  function.instructions.push_back({primec::IrOpcode::CmpNeF64, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_cmp_ne_f64.glsl";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "glsl_ir_backend_cmp_ne_f64.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("// Narrowed GLSL path lowers f64 inequality compare through f32 payloads.") != std::string::npos);
  CHECK(source.find("stack[sp++] = (left != right) ? 1 : 0;") != std::string::npos);
}

TEST_CASE("glsl-ir backend writes narrowed f64 less-than compare source") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushF64, 0x3ff8000000000000ull});
  function.instructions.push_back({primec::IrOpcode::PushF64, 0x4000000000000000ull});
  function.instructions.push_back({primec::IrOpcode::CmpLtF64, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_cmp_lt_f64.glsl";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "glsl_ir_backend_cmp_lt_f64.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("// Narrowed GLSL path lowers f64 less-than compare through f32 payloads.") != std::string::npos);
  CHECK(source.find("stack[sp++] = (left < right) ? 1 : 0;") != std::string::npos);
}

TEST_CASE("glsl-ir backend writes narrowed f64 less-or-equal compare source") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushF64, 0x3ff8000000000000ull});
  function.instructions.push_back({primec::IrOpcode::PushF64, 0x3ff8000000000000ull});
  function.instructions.push_back({primec::IrOpcode::CmpLeF64, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_cmp_le_f64.glsl";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "glsl_ir_backend_cmp_le_f64.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("// Narrowed GLSL path lowers f64 less-or-equal compare through f32 payloads.") != std::string::npos);
  CHECK(source.find("stack[sp++] = (left <= right) ? 1 : 0;") != std::string::npos);
}

TEST_CASE("glsl-ir backend writes narrowed f64 greater-than compare source") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushF64, 0x4000000000000000ull});
  function.instructions.push_back({primec::IrOpcode::PushF64, 0x3ff8000000000000ull});
  function.instructions.push_back({primec::IrOpcode::CmpGtF64, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_cmp_gt_f64.glsl";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "glsl_ir_backend_cmp_gt_f64.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("// Narrowed GLSL path lowers f64 greater-than compare through f32 payloads.") != std::string::npos);
  CHECK(source.find("stack[sp++] = (left > right) ? 1 : 0;") != std::string::npos);
}

TEST_CASE("glsl-ir backend writes narrowed f64 greater-or-equal compare source") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushF64, 0x4000000000000000ull});
  function.instructions.push_back({primec::IrOpcode::PushF64, 0x4000000000000000ull});
  function.instructions.push_back({primec::IrOpcode::CmpGeF64, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_cmp_ge_f64.glsl";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "glsl_ir_backend_cmp_ge_f64.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("// Narrowed GLSL path lowers f64 greater-or-equal compare through f32 payloads.") !=
        std::string::npos);
  CHECK(source.find("stack[sp++] = (left >= right) ? 1 : 0;") != std::string::npos);
}

TEST_CASE("glsl-ir backend writes file-open-read stub source") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  module.stringTable.push_back("/dev/null");
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::FileOpenRead, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_file_open_read.glsl";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "glsl_ir_backend_file_open_read.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("// GLSL backend cannot open files; push deterministic invalid file handle.") != std::string::npos);
  CHECK(source.find("stack[sp++] = -1;") != std::string::npos);
}

TEST_CASE("glsl-ir backend writes file-open-write stub source") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  module.stringTable.push_back("/tmp/out.txt");
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::FileOpenWrite, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_file_open_write.glsl";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "glsl_ir_backend_file_open_write.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("// GLSL backend cannot open files; push deterministic invalid file handle.") != std::string::npos);
  CHECK(source.find("stack[sp++] = -1;") != std::string::npos);
}

TEST_CASE("glsl-ir backend writes file-open-append stub source") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  module.stringTable.push_back("/tmp/out.txt");
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::FileOpenAppend, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_file_open_append.glsl";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "glsl_ir_backend_file_open_append.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("// GLSL backend cannot open files; push deterministic invalid file handle.") != std::string::npos);
  CHECK(source.find("stack[sp++] = -1;") != std::string::npos);
}

TEST_CASE("glsl-ir backend writes file-close stub source") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushI32, static_cast<uint64_t>(-1)});
  function.instructions.push_back({primec::IrOpcode::FileClose, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_file_close.glsl";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "glsl_ir_backend_file_close.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("// GLSL backend cannot close files; replace handle with deterministic success code.") !=
        std::string::npos);
  CHECK(source.find("stack[sp - 1] = 0;") != std::string::npos);
}

TEST_CASE("glsl-ir backend writes file-flush stub source") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushI32, static_cast<uint64_t>(-1)});
  function.instructions.push_back({primec::IrOpcode::FileFlush, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_file_flush.glsl";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "glsl_ir_backend_file_flush.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("// GLSL backend cannot flush files; replace handle with deterministic success code.") !=
        std::string::npos);
  CHECK(source.find("stack[sp - 1] = 0;") != std::string::npos);
}

TEST_CASE("glsl-ir backend writes file-write-i32 stub source") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushI32, static_cast<uint64_t>(-1)});
  function.instructions.push_back({primec::IrOpcode::PushI32, 7});
  function.instructions.push_back({primec::IrOpcode::FileWriteI32, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_file_write_i32.glsl";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "glsl_ir_backend_file_write_i32.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("// GLSL backend cannot write files; consume value/handle and push deterministic success code.") !=
        std::string::npos);
  CHECK(source.find("sp -= 2;") != std::string::npos);
  CHECK(source.find("stack[sp++] = 0;") != std::string::npos);
}

TEST_CASE("glsl-ir backend writes file-write-i64 stub source") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushI32, static_cast<uint64_t>(-1)});
  function.instructions.push_back({primec::IrOpcode::PushI64, 7});
  function.instructions.push_back({primec::IrOpcode::FileWriteI64, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_file_write_i64.glsl";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "glsl_ir_backend_file_write_i64.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("// GLSL backend cannot write files; consume value/handle and push deterministic success code.") !=
        std::string::npos);
  CHECK(source.find("sp -= 2;") != std::string::npos);
  CHECK(source.find("stack[sp++] = 0;") != std::string::npos);
}

TEST_CASE("glsl-ir backend writes file-write-u64 stub source") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushI32, static_cast<uint64_t>(-1)});
  function.instructions.push_back({primec::IrOpcode::PushI64, 7});
  function.instructions.push_back({primec::IrOpcode::FileWriteU64, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_file_write_u64.glsl";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "glsl_ir_backend_file_write_u64.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("// GLSL backend cannot write files; consume value/handle and push deterministic success code.") !=
        std::string::npos);
  CHECK(source.find("sp -= 2;") != std::string::npos);
  CHECK(source.find("stack[sp++] = 0;") != std::string::npos);
}

TEST_CASE("glsl-ir backend writes file-write-string stub source") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  module.stringTable.push_back("hello");
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushI32, static_cast<uint64_t>(-1)});
  function.instructions.push_back({primec::IrOpcode::FileWriteString, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_file_write_string.glsl";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "glsl_ir_backend_file_write_string.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("// GLSL backend cannot write files; replace handle with deterministic success code.") !=
        std::string::npos);
  CHECK(source.find("stack[sp - 1] = 0;") != std::string::npos);
}

TEST_CASE("glsl-ir backend writes file-write-byte stub source") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushI32, static_cast<uint64_t>(-1)});
  function.instructions.push_back({primec::IrOpcode::PushI32, 65});
  function.instructions.push_back({primec::IrOpcode::FileWriteByte, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_file_write_byte.glsl";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "glsl_ir_backend_file_write_byte.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("// GLSL backend cannot write files; consume byte/handle and push deterministic success code.") !=
        std::string::npos);
  CHECK(source.find("sp -= 2;") != std::string::npos);
  CHECK(source.find("stack[sp++] = 0;") != std::string::npos);
}

TEST_CASE("glsl-ir backend writes file-write-newline stub source") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushI32, static_cast<uint64_t>(-1)});
  function.instructions.push_back({primec::IrOpcode::FileWriteNewline, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_file_write_newline.glsl";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "glsl_ir_backend_file_write_newline.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("// GLSL backend cannot write files; replace handle with deterministic success code.") !=
        std::string::npos);
  CHECK(source.find("stack[sp - 1] = 0;") != std::string::npos);
}

TEST_CASE("glsl-ir backend writes address-of-local source") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::AddressOfLocal, 7});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_address_of_local.glsl";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "glsl_ir_backend_address_of_local.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("// GLSL backend lowers local addresses to deterministic slot-byte offsets.") !=
        std::string::npos);
  CHECK(source.find("stack[sp++] = 56;") != std::string::npos);
}

TEST_CASE("glsl-ir backend writes load-indirect source") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushI32, 41});
  function.instructions.push_back({primec::IrOpcode::StoreLocal, 3});
  function.instructions.push_back({primec::IrOpcode::AddressOfLocal, 3});
  function.instructions.push_back({primec::IrOpcode::LoadIndirect, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_load_indirect.glsl";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "glsl_ir_backend_load_indirect.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("// GLSL backend loads locals through deterministic aligned byte-slot addressing.") !=
        std::string::npos);
  CHECK(source.find("uint loadIndirectAddress = uint(stack[--sp]);") != std::string::npos);
  CHECK(source.find("if ((loadIndirectAddress & 7u) == 0u) {") != std::string::npos);
  CHECK(source.find("loadIndirectValue = locals[loadIndirectIndex];") != std::string::npos);
  CHECK(source.find("stack[sp++] = loadIndirectValue;") != std::string::npos);
}

TEST_CASE("glsl-ir backend writes store-indirect source") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushI32, 24});
  function.instructions.push_back({primec::IrOpcode::PushI32, 41});
  function.instructions.push_back({primec::IrOpcode::StoreIndirect, 0});
  function.instructions.push_back({primec::IrOpcode::LoadLocal, 3});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_store_indirect.glsl";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "glsl_ir_backend_store_indirect.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("// GLSL backend stores locals through deterministic aligned byte-slot addressing.") !=
        std::string::npos);
  CHECK(source.find("int storeIndirectValue = stack[--sp];") != std::string::npos);
  CHECK(source.find("uint storeIndirectAddress = uint(stack[--sp]);") != std::string::npos);
  CHECK(source.find("if ((storeIndirectAddress & 7u) == 0u) {") != std::string::npos);
  CHECK(source.find("locals[storeIndirectIndex] = storeIndirectValue;") != std::string::npos);
  CHECK(source.find("stack[sp++] = storeIndirectValue;") != std::string::npos);
}

TEST_CASE("glsl-ir backend reports emitter diagnostics") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({static_cast<primec::IrOpcode>(255), 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  primec::IrBackendEmitOptions options;
  options.outputPath = (std::filesystem::current_path() / "primec_tests" / "unused.glsl").string();
  options.inputPath = "glsl_ir_backend_error.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  CHECK_FALSE(backend->emit(module, options, result, error));
  CHECK(error.find("ir-to-glsl failed:") != std::string::npos);
}

TEST_CASE("glsl-ir backend reports print string index diagnostics") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back(
      {primec::IrOpcode::PrintString, primec::encodePrintStringImm(3, primec::encodePrintFlags(true, false))});
  function.instructions.push_back({primec::IrOpcode::ReturnVoid, 0});
  module.functions.push_back(function);

  primec::IrBackendEmitOptions options;
  options.outputPath = (std::filesystem::current_path() / "primec_tests" / "unused_print_index.glsl").string();
  options.inputPath = "glsl_ir_backend_print_string_index_error.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  CHECK_FALSE(backend->emit(module, options, result, error));
  CHECK(error.find("ir-to-glsl failed:") != std::string::npos);
  CHECK(error.find("string index out of range") != std::string::npos);
}

TEST_CASE("spirv-ir backend reports emitter diagnostics") {
  const primec::IrBackend *backend = primec::findIrBackend("spirv-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({static_cast<primec::IrOpcode>(255), 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  primec::IrBackendEmitOptions options;
  options.outputPath = (std::filesystem::current_path() / "primec_tests" / "unused.spv").string();
  options.inputPath = "spirv_ir_backend_error.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  CHECK_FALSE(backend->emit(module, options, result, error));
  CHECK(error.find("ir-to-glsl failed:") != std::string::npos);
}

TEST_SUITE_END();
