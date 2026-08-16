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
  entry.instructions.push_back({primec::IrOpcode::CallVoid, 1});
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

TEST_CASE("cpp-ir backend reports invalid entry index diagnostics") {
  const primec::IrBackend *backend = primec::findIrBackend("cpp-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 3;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::ReturnVoid, 0});
  module.functions.push_back(function);

  const std::filesystem::path outputPath =
      std::filesystem::current_path() / "primec_tests" / "unused_invalid_entry_index.cpp";
  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "cpp_ir_backend_invalid_entry_index_error.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  CHECK_FALSE(backend->emit(module, options, result, error));
  CHECK(error.find("ir-to-cpp failed:") != std::string::npos);
  CHECK(error.find("invalid IR entry index") != std::string::npos);
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
