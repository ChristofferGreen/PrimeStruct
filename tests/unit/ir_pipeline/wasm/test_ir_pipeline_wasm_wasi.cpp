#include "test_ir_pipeline_wasm_helpers.h"

using namespace ir_pipeline_wasm_test;

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("wasm emitter adds wasi imports memory and argv output lowering") {
  primec::WasmEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  module.stringTable.push_back("hello");

  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::PushArgc, 0});
  mainFn.instructions.push_back({primec::IrOpcode::Pop, 0});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::PrintArgv, primec::PrintFlagNewline});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::PrintArgvUnsafe, primec::PrintFlagStderr});
  mainFn.instructions.push_back(
      {primec::IrOpcode::PrintString, primec::encodePrintStringImm(0, primec::PrintFlagStderr | primec::PrintFlagNewline)});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnVoid, 0});
  module.functions.push_back(std::move(mainFn));

  std::vector<uint8_t> bytes;
  std::string error;
  REQUIRE(emitter.emitModule(module, bytes, error));
  CHECK(error.empty());

  std::vector<uint8_t> sectionIds;
  std::vector<uint32_t> sectionLengths;
  REQUIRE(parseSections(bytes, sectionIds, sectionLengths, error));
  CHECK(error.empty());
  const std::vector<uint8_t> expectedSectionIds = {1, 2, 3, 5, 7, 10, 11};
  CHECK(sectionIds == expectedSectionIds);
  CHECK_FALSE(sectionPayloadIsEmptyVector(bytes, 5));

  const auto textBytes = [](const std::string &text) {
    return std::vector<uint8_t>(text.begin(), text.end());
  };
  CHECK(containsByteSequence(bytes, textBytes("wasi_snapshot_preview1")));
  CHECK(containsByteSequence(bytes, textBytes("fd_write")));
  CHECK(containsByteSequence(bytes, textBytes("args_sizes_get")));
  CHECK(containsByteSequence(bytes, textBytes("args_get")));

  CHECK(containsByteSequence(bytes, {0x10, 0x00}));
  CHECK(containsByteSequence(bytes, {0x10, 0x01}));
  CHECK(containsByteSequence(bytes, {0x10, 0x02}));
  CHECK(containsByteSequence(bytes, {0x36, 0x02, 0x00}));
  CHECK(containsByteSequence(bytes, {0x28, 0x02, 0x00}));
}

TEST_CASE("wasm emitter maps wasi file open write flush close and error paths") {
  primec::WasmEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;

  const std::string outName = "primec_wasm_file_ops_output.txt";
  const std::string missingName = "primec_wasm_file_ops_missing.txt";
  module.stringTable.push_back(outName);
  module.stringTable.push_back(missingName);
  module.stringTable.push_back("payload");
  module.stringTable.push_back("tail");

  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::FileOpenWrite, 0});
  mainFn.instructions.push_back({primec::IrOpcode::StoreLocal, 0});
  mainFn.instructions.push_back({primec::IrOpcode::LoadLocal, 0});
  mainFn.instructions.push_back({primec::IrOpcode::FileWriteString, 2});
  mainFn.instructions.push_back({primec::IrOpcode::Pop, 0});
  mainFn.instructions.push_back({primec::IrOpcode::LoadLocal, 0});
  mainFn.instructions.push_back({primec::IrOpcode::PushI64, 3});
  mainFn.instructions.push_back({primec::IrOpcode::FileWriteStringDynamic, 0});
  mainFn.instructions.push_back({primec::IrOpcode::Pop, 0});
  mainFn.instructions.push_back({primec::IrOpcode::LoadLocal, 0});
  mainFn.instructions.push_back({primec::IrOpcode::FileFlush, 0});
  mainFn.instructions.push_back({primec::IrOpcode::Pop, 0});
  mainFn.instructions.push_back({primec::IrOpcode::LoadLocal, 0});
  mainFn.instructions.push_back({primec::IrOpcode::FileClose, 0});
  mainFn.instructions.push_back({primec::IrOpcode::Pop, 0});

  mainFn.instructions.push_back({primec::IrOpcode::FileOpenRead, 1});
  mainFn.instructions.push_back({primec::IrOpcode::StoreLocal, 1});
  mainFn.instructions.push_back({primec::IrOpcode::LoadLocal, 1});
  mainFn.instructions.push_back({primec::IrOpcode::FileClose, 0});
  mainFn.instructions.push_back({primec::IrOpcode::StoreLocal, 2});
  mainFn.instructions.push_back({primec::IrOpcode::LoadLocal, 2});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::CmpGtI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(std::move(mainFn));

  std::vector<uint8_t> bytes;
  std::string error;
  REQUIRE(emitter.emitModule(module, bytes, error));
  CHECK(error.empty());

  std::vector<uint8_t> sectionIds;
  std::vector<uint32_t> sectionLengths;
  REQUIRE(parseSections(bytes, sectionIds, sectionLengths, error));
  CHECK(error.empty());
  const std::vector<uint8_t> expectedSectionIds = {1, 2, 3, 5, 7, 10, 11};
  CHECK(sectionIds == expectedSectionIds);

  const auto textBytes = [](const std::string &text) {
    return std::vector<uint8_t>(text.begin(), text.end());
  };
  CHECK(containsByteSequence(bytes, textBytes("path_open")));
  CHECK(containsByteSequence(bytes, textBytes("fd_sync")));
  CHECK(containsByteSequence(bytes, textBytes("fd_close")));
  CHECK(containsByteSequence(bytes, textBytes("fd_write")));

  CHECK(containsByteSequence(bytes, {0x10, 0x00}));
  CHECK(containsByteSequence(bytes, {0x10, 0x01}));
  CHECK(containsByteSequence(bytes, {0x10, 0x02}));
  CHECK(containsByteSequence(bytes, {0x10, 0x03}));

  if (hasWasmtime()) {
    const std::filesystem::path tempRoot = irPipelineWasmPath("primec_wasm_file_ops_runtime");
    std::error_code ec;
    std::filesystem::remove_all(tempRoot, ec);
    std::filesystem::create_directories(tempRoot, ec);
    REQUIRE(!ec);

    const std::filesystem::path wasmPath = tempRoot / "file_ops.wasm";
    const std::filesystem::path runOutPath = tempRoot / "wasmtime_stdout.txt";
    const std::filesystem::path outFilePath = tempRoot / outName;
    {
      std::ofstream wasmFile(wasmPath, std::ios::binary);
      REQUIRE(wasmFile.good());
      wasmFile.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
      REQUIRE(wasmFile.good());
    }

    const std::string runCmd = "wasmtime --invoke main --dir=" + quoteShellArg(tempRoot.string()) + " " +
                               quoteShellArg(wasmPath.string()) + " > " + quoteShellArg(runOutPath.string());
    CHECK(runCommand(runCmd) == 0);
    CHECK(readFileText(runOutPath.string()).find("1") != std::string::npos);
    CHECK(readFileText(outFilePath.string()) == "payloadtail");
  }
}

TEST_CASE("wasm emitter maps wasi file read byte and eof paths") {
  primec::WasmEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;

  const std::string inName = "primec_wasm_file_read_input.txt";
  module.stringTable.push_back(inName);

  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::FileOpenRead, 0});
  mainFn.instructions.push_back({primec::IrOpcode::StoreLocal, 0});
  mainFn.instructions.push_back({primec::IrOpcode::LoadLocal, 0});
  mainFn.instructions.push_back({primec::IrOpcode::FileReadByte, 1});
  mainFn.instructions.push_back({primec::IrOpcode::Pop, 0});
  mainFn.instructions.push_back({primec::IrOpcode::LoadLocal, 0});
  mainFn.instructions.push_back({primec::IrOpcode::FileReadByte, 2});
  mainFn.instructions.push_back({primec::IrOpcode::StoreLocal, 3});
  mainFn.instructions.push_back({primec::IrOpcode::LoadLocal, 3});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, primec::FileReadEofCode});
  mainFn.instructions.push_back({primec::IrOpcode::CmpEqI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::LoadLocal, 1});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, 65});
  mainFn.instructions.push_back({primec::IrOpcode::CmpEqI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::MulI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(std::move(mainFn));

  std::vector<uint8_t> bytes;
  std::string error;
  REQUIRE(emitter.emitModule(module, bytes, error));
  CHECK(error.empty());

  const auto textBytes = [](const std::string &text) {
    return std::vector<uint8_t>(text.begin(), text.end());
  };
  CHECK(containsByteSequence(bytes, textBytes("fd_read")));

  if (hasWasmtime()) {
    const std::filesystem::path tempRoot = irPipelineWasmPath("primec_wasm_file_read_runtime");
    std::error_code ec;
    std::filesystem::remove_all(tempRoot, ec);
    std::filesystem::create_directories(tempRoot, ec);
    REQUIRE(!ec);

    {
      std::ofstream input(tempRoot / inName, std::ios::binary);
      REQUIRE(input.good());
      input.write("A", 1);
      REQUIRE(input.good());
    }

    const std::filesystem::path wasmPath = tempRoot / "file_read.wasm";
    {
      std::ofstream wasmFile(wasmPath, std::ios::binary);
      REQUIRE(wasmFile.good());
      wasmFile.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
      REQUIRE(wasmFile.good());
    }

    const std::filesystem::path runOutPath = tempRoot / "wasmtime_stdout.txt";
    const std::string runCmd = "wasmtime --invoke main --dir=" + quoteShellArg(tempRoot.string()) + " " +
                               quoteShellArg(wasmPath.string()) + " > " + quoteShellArg(runOutPath.string());
    CHECK(runCommand(runCmd) == 0);
    CHECK(readFileText(runOutPath.string()).find("1") != std::string::npos);
  }
}

TEST_CASE("wasm emitter formats decimal file writes for i32 i64 and u64") {
  primec::WasmEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;

  const std::string outName = "primec_wasm_file_numeric_output.txt";
  module.stringTable.push_back(outName);

  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::FileOpenWrite, 0});
  mainFn.instructions.push_back({primec::IrOpcode::StoreLocal, 0});

  mainFn.instructions.push_back({primec::IrOpcode::LoadLocal, 0});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, static_cast<uint64_t>(static_cast<int64_t>(-42))});
  mainFn.instructions.push_back({primec::IrOpcode::FileWriteI32, 0});
  mainFn.instructions.push_back({primec::IrOpcode::Pop, 0});

  mainFn.instructions.push_back({primec::IrOpcode::LoadLocal, 0});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, static_cast<uint64_t>(',')});
  mainFn.instructions.push_back({primec::IrOpcode::FileWriteByte, 0});
  mainFn.instructions.push_back({primec::IrOpcode::Pop, 0});

  mainFn.instructions.push_back({primec::IrOpcode::LoadLocal, 0});
  mainFn.instructions.push_back({primec::IrOpcode::PushF64, 0xc271f71fb04cb000ull});
  mainFn.instructions.push_back({primec::IrOpcode::ConvertF64ToI64, 0});
  mainFn.instructions.push_back({primec::IrOpcode::FileWriteI64, 0});
  mainFn.instructions.push_back({primec::IrOpcode::Pop, 0});

  mainFn.instructions.push_back({primec::IrOpcode::LoadLocal, 0});
  mainFn.instructions.push_back({primec::IrOpcode::PushI32, static_cast<uint64_t>(',')});
  mainFn.instructions.push_back({primec::IrOpcode::FileWriteByte, 0});
  mainFn.instructions.push_back({primec::IrOpcode::Pop, 0});

  mainFn.instructions.push_back({primec::IrOpcode::LoadLocal, 0});
  mainFn.instructions.push_back({primec::IrOpcode::PushF64, 0x41efffffffe00000ull});
  mainFn.instructions.push_back({primec::IrOpcode::ConvertF64ToU64, 0});
  mainFn.instructions.push_back({primec::IrOpcode::FileWriteU64, 0});
  mainFn.instructions.push_back({primec::IrOpcode::Pop, 0});

  mainFn.instructions.push_back({primec::IrOpcode::LoadLocal, 0});
  mainFn.instructions.push_back({primec::IrOpcode::FileClose, 0});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(std::move(mainFn));

  std::vector<uint8_t> bytes;
  std::string error;
  REQUIRE(emitter.emitModule(module, bytes, error));
  CHECK(error.empty());

  const auto textBytes = [](const std::string &text) {
    return std::vector<uint8_t>(text.begin(), text.end());
  };
  CHECK(containsByteSequence(bytes, textBytes("fd_write")));
  CHECK(containsByteSequence(bytes, {0x80}));
  CHECK(containsByteSequence(bytes, {0x82}));
  CHECK(containsByteSequence(bytes, {0xa7}));

  if (hasWasmtime()) {
    const std::filesystem::path tempRoot = irPipelineWasmPath("primec_wasm_file_numeric_runtime");
    std::error_code ec;
    std::filesystem::remove_all(tempRoot, ec);
    std::filesystem::create_directories(tempRoot, ec);
    REQUIRE(!ec);

    const std::filesystem::path wasmPath = tempRoot / "file_numeric.wasm";
    const std::filesystem::path outFilePath = tempRoot / outName;
    {
      std::ofstream wasmFile(wasmPath, std::ios::binary);
      REQUIRE(wasmFile.good());
      wasmFile.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
      REQUIRE(wasmFile.good());
    }

    const std::string runCmd = "wasmtime --invoke main --dir=" + quoteShellArg(tempRoot.string()) + " " +
                               quoteShellArg(wasmPath.string()) + " > /dev/null";
    CHECK(runCommand(runCmd) == 0);
    CHECK(readFileText(outFilePath.string()) == "-42,-1234567890123,4294967295");
  }
}

TEST_CASE("wasm emitter rejects unsupported opcodes for this slice") {
  primec::WasmEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;

  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::HeapAlloc, 1});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnVoid, 0});
  module.functions.push_back(mainFn);

  std::vector<uint8_t> bytes;
  std::string error;
  CHECK_FALSE(emitter.emitModule(module, bytes, error));
  CHECK(error.find("unsupported opcode") != std::string::npos);
}

TEST_SUITE_END();
