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
