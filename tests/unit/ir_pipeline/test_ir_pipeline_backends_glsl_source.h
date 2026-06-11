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
