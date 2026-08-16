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
