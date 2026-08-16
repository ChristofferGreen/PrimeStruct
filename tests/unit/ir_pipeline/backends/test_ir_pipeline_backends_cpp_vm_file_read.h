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
  CHECK(source.find("#include <limits>") != std::string::npos);
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
  CHECK(source.find("#include <limits>") != std::string::npos);
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
  CHECK(source.find("#include <limits>") != std::string::npos);
  CHECK(source.find("static int64_t psConvertF32ToI64(float value)") == std::string::npos);
  CHECK(source.find("static int64_t psConvertF64ToI64(double value)") == std::string::npos);
}

