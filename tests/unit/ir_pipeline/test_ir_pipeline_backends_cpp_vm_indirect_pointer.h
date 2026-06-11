
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

TEST_CASE("cpp-ir backend emits dup and pop underflow guards") {
  const primec::IrBackend *backend = primec::findIrBackend("cpp-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::Dup, 0});
  function.instructions.push_back({primec::IrOpcode::Pop, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_dup_pop_underflow.cpp";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "cpp_ir_backend_dup_pop_underflow.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("IR stack underflow on ") != std::string::npos);
  CHECK(source.find("psEnsureStack(sp, 1ull, \"dup\")") != std::string::npos);
  CHECK(source.find("psEnsureStack(sp, 1ull, \"pop\")") != std::string::npos);
}

TEST_CASE("cpp-ir backend emits arithmetic print file return underflow guards") {
  const primec::IrBackend *backend = primec::findIrBackend("cpp-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  module.stringTable.push_back("payload");
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::AddI32, 0});
  function.instructions.push_back({primec::IrOpcode::SubI32, 0});
  function.instructions.push_back({primec::IrOpcode::MulI32, 0});
  function.instructions.push_back({primec::IrOpcode::DivI32, 0});
  function.instructions.push_back({primec::IrOpcode::AddI64, 0});
  function.instructions.push_back({primec::IrOpcode::SubI64, 0});
  function.instructions.push_back({primec::IrOpcode::MulI64, 0});
  function.instructions.push_back({primec::IrOpcode::DivI64, 0});
  function.instructions.push_back({primec::IrOpcode::DivU64, 0});
  function.instructions.push_back({primec::IrOpcode::AddF32, 0});
  function.instructions.push_back({primec::IrOpcode::SubF32, 0});
  function.instructions.push_back({primec::IrOpcode::MulF32, 0});
  function.instructions.push_back({primec::IrOpcode::DivF32, 0});
  function.instructions.push_back({primec::IrOpcode::AddF64, 0});
  function.instructions.push_back({primec::IrOpcode::SubF64, 0});
  function.instructions.push_back({primec::IrOpcode::MulF64, 0});
  function.instructions.push_back({primec::IrOpcode::DivF64, 0});
  function.instructions.push_back({primec::IrOpcode::CmpEqI32, 0});
  function.instructions.push_back({primec::IrOpcode::CmpEqI64, 0});
  function.instructions.push_back({primec::IrOpcode::CmpEqF32, 0});
  function.instructions.push_back({primec::IrOpcode::CmpEqF64, 0});
  function.instructions.push_back({primec::IrOpcode::CmpEqI32, 0});
  function.instructions.push_back({primec::IrOpcode::NegI32, 0});
  function.instructions.push_back({primec::IrOpcode::NegI64, 0});
  function.instructions.push_back({primec::IrOpcode::NegF32, 0});
  function.instructions.push_back({primec::IrOpcode::NegF64, 0});
  function.instructions.push_back({primec::IrOpcode::ConvertI32ToF32, 0});
  function.instructions.push_back({primec::IrOpcode::ConvertF32ToI32, 0});
  function.instructions.push_back({primec::IrOpcode::ConvertI64ToF64, 0});
  function.instructions.push_back({primec::IrOpcode::ConvertF64ToI64, 0});
  function.instructions.push_back({primec::IrOpcode::StoreLocal, 0});
  function.instructions.push_back({primec::IrOpcode::LoadIndirect, 0});
  function.instructions.push_back({primec::IrOpcode::StoreIndirect, 0});
  function.instructions.push_back({primec::IrOpcode::HeapAlloc, 0});
  function.instructions.push_back({primec::IrOpcode::HeapFree, 0});
  function.instructions.push_back({primec::IrOpcode::HeapRealloc, 0});
  function.instructions.push_back({primec::IrOpcode::Dup, 0});
  function.instructions.push_back({primec::IrOpcode::JumpIfZero, 0});
  function.instructions.push_back({primec::IrOpcode::PrintI32, primec::encodePrintFlags(false, false)});
  function.instructions.push_back({primec::IrOpcode::PrintI64, primec::encodePrintFlags(false, false)});
  function.instructions.push_back({primec::IrOpcode::PrintU64, primec::encodePrintFlags(false, false)});
  function.instructions.push_back({primec::IrOpcode::PrintStringDynamic, primec::encodePrintFlags(false, false)});
  function.instructions.push_back({primec::IrOpcode::PrintArgv, primec::encodePrintFlags(false, false)});
  function.instructions.push_back({primec::IrOpcode::PrintArgvUnsafe, primec::encodePrintFlags(false, false)});
  function.instructions.push_back({primec::IrOpcode::FileClose, 0});
  function.instructions.push_back({primec::IrOpcode::FileFlush, 0});
  function.instructions.push_back({primec::IrOpcode::FileWriteI32, 0});
  function.instructions.push_back({primec::IrOpcode::FileWriteI64, 0});
  function.instructions.push_back({primec::IrOpcode::FileWriteU64, 0});
  function.instructions.push_back({primec::IrOpcode::FileWriteString, 0});
  function.instructions.push_back({primec::IrOpcode::FileWriteByte, 0});
  function.instructions.push_back({primec::IrOpcode::FileWriteNewline, 0});
  function.instructions.push_back({primec::IrOpcode::LoadStringByte, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI64, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnF32, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnF64, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnVoid, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_stack_underflow_guards.cpp";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "cpp_ir_backend_stack_underflow_guards.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("IR stack underflow on ") != std::string::npos);
  CHECK(source.find("psEnsureStack(sp, 2ull, \"add\")") != std::string::npos);
  CHECK(source.find("psEnsureStack(sp, 2ull, \"sub\")") != std::string::npos);
  CHECK(source.find("psEnsureStack(sp, 2ull, \"mul\")") != std::string::npos);
  CHECK(source.find("psEnsureStack(sp, 2ull, \"div\")") != std::string::npos);
  CHECK(source.find("psEnsureStack(sp, 2ull, \"float op\")") != std::string::npos);
  CHECK(source.find("psEnsureStack(sp, 2ull, \"compare\")") != std::string::npos);
  CHECK(source.find("psEnsureStack(sp, 1ull, \"negate\")") != std::string::npos);
  CHECK(source.find("psEnsureStack(sp, 1ull, \"convert\")") != std::string::npos);
  CHECK(source.find("psEnsureStack(sp, 1ull, \"store\")") != std::string::npos);
  CHECK(source.find("psEnsureStack(sp, 1ull, \"load indirect\")") != std::string::npos);
  CHECK(source.find("psEnsureStack(sp, 2ull, \"store indirect\")") != std::string::npos);
  CHECK(source.find("psEnsureStack(sp, 1ull, \"heap alloc\")") != std::string::npos);
  CHECK(source.find("psEnsureStack(sp, 1ull, \"heap free\")") != std::string::npos);
  CHECK(source.find("psEnsureStack(sp, 2ull, \"heap realloc\")") != std::string::npos);
  CHECK(source.find("psEnsureStack(sp, 1ull, \"jump\")") != std::string::npos);
  CHECK(source.find("psEnsureStack(sp, 1ull, \"print\")") != std::string::npos);
  CHECK(source.find("psEnsureStack(sp, 1ull, \"file close\")") != std::string::npos);
  CHECK(source.find("psEnsureStack(sp, 1ull, \"file flush\")") != std::string::npos);
  CHECK(source.find("psEnsureStack(sp, 2ull, \"file write\")") != std::string::npos);
  CHECK(source.find("psEnsureStack(sp, 1ull, \"string byte\")") != std::string::npos);
  CHECK(source.find("psEnsureStack(sp, 1ull, \"return\")") != std::string::npos);
  CHECK(source.find("uint64_t dupValue = stack[sp - 1];") != std::string::npos);
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
