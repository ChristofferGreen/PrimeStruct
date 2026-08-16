TEST_CASE("ir to cpp emitter writes print and argv opcodes") {
  primec::IrToCppEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  module.stringTable.push_back("hello");
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::PushI32, 5});
  fn.instructions.push_back({primec::IrOpcode::PrintI32, primec::PrintFlagNewline});
  fn.instructions.push_back({primec::IrOpcode::PrintString, primec::encodePrintStringImm(0, 0)});
  fn.instructions.push_back({primec::IrOpcode::PushI32, 0});
  fn.instructions.push_back({primec::IrOpcode::PrintStringDynamic, primec::PrintFlagNewline});
  fn.instructions.push_back({primec::IrOpcode::PushArgc, 0});
  fn.instructions.push_back({primec::IrOpcode::PushI32, 0});
  fn.instructions.push_back({primec::IrOpcode::PrintArgvUnsafe, primec::PrintFlagStderr});
  fn.instructions.push_back({primec::IrOpcode::ReturnVoid, 0});
  module.functions.push_back(fn);

  std::string cpp;
  std::string error;
  REQUIRE(emitter.emitSource(module, cpp, error));
  CHECK(error.empty());
  CHECK(cpp.find("std::cout << static_cast<int32_t>(stack[--sp]);") != std::string::npos);
  CHECK(cpp.find("std::cout << ps_string_table[0];") != std::string::npos);
  CHECK(cpp.find("if (stringIndex >= ps_string_table_count)") != std::string::npos);
  CHECK(cpp.find("std::cout << ps_string_table[stringIndex];") != std::string::npos);
  CHECK(cpp.find("stack[sp++] = static_cast<uint64_t>(argc);") != std::string::npos);
  CHECK(cpp.find("argv[indexValue]") != std::string::npos);
  CHECK(cpp.find("std::cerr << argv[indexValue];") != std::string::npos);
}

TEST_CASE("ir to cpp emitter writes string byte load opcode") {
  primec::IrToCppEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  module.stringTable.push_back("abc");
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::PushI32, 1});
  fn.instructions.push_back({primec::IrOpcode::LoadStringByte, 0});
  fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(fn);

  std::string cpp;
  std::string error;
  REQUIRE(emitter.emitSource(module, cpp, error));
  CHECK(error.empty());
  CHECK(cpp.find("if (stringByteIndex >= 3ull)") != std::string::npos);
  CHECK(cpp.find("ps_string_table[0][stringByteIndex]") != std::string::npos);
}

TEST_CASE("ir to cpp emitter writes indirect local pointer opcodes") {
  primec::IrToCppEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::PushI32, 3});
  fn.instructions.push_back({primec::IrOpcode::StoreLocal, 0});
  fn.instructions.push_back({primec::IrOpcode::AddressOfLocal, 0});
  fn.instructions.push_back({primec::IrOpcode::PushI32, 8});
  fn.instructions.push_back({primec::IrOpcode::StoreIndirect, 0});
  fn.instructions.push_back({primec::IrOpcode::AddressOfLocal, 0});
  fn.instructions.push_back({primec::IrOpcode::LoadIndirect, 0});
  fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(fn);

  std::string cpp;
  std::string error;
  REQUIRE(emitter.emitSource(module, cpp, error));
  CHECK(error.empty());
  CHECK(cpp.find("stack[sp++] = static_cast<uint64_t>(0ull * 16ull);") != std::string::npos);
  CHECK(cpp.find("loadIndirectAddress % 16ull") != std::string::npos);
  CHECK(cpp.find("locals[loadIndirectIndex]") != std::string::npos);
  CHECK(cpp.find("locals[storeIndirectIndex] = storeIndirectValue;") != std::string::npos);
}

TEST_CASE("ir to cpp emitter writes file io opcodes") {
  primec::IrToCppEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  module.stringTable.push_back("/tmp/ir_to_cpp_file_io.txt");
  module.stringTable.push_back("hello");
  module.stringTable.push_back("world");
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::FileOpenWrite, 0});
  fn.instructions.push_back({primec::IrOpcode::Dup, 0});
  fn.instructions.push_back({primec::IrOpcode::FileWriteString, 1});
  fn.instructions.push_back({primec::IrOpcode::Pop, 0});
  fn.instructions.push_back({primec::IrOpcode::Dup, 0});
  fn.instructions.push_back({primec::IrOpcode::PushI64, 2});
  fn.instructions.push_back({primec::IrOpcode::FileWriteStringDynamic, 0});
  fn.instructions.push_back({primec::IrOpcode::Pop, 0});
  fn.instructions.push_back({primec::IrOpcode::Dup, 0});
  fn.instructions.push_back({primec::IrOpcode::FileFlush, 0});
  fn.instructions.push_back({primec::IrOpcode::Pop, 0});
  fn.instructions.push_back({primec::IrOpcode::FileClose, 0});
  fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(fn);

  std::string cpp;
  std::string error;
  REQUIRE(emitter.emitSource(module, cpp, error));
  CHECK(error.empty());
  CHECK(cpp.find("static uint32_t psWriteAll(int fd, const void *data, std::size_t size)") != std::string::npos);
  CHECK(cpp.find("int fileFd = ::open(ps_string_table[0], fileOpenFlags, 0644);") != std::string::npos);
  CHECK(cpp.find("if (fileStringDynamicIndex >= ps_string_table_count)") != std::string::npos);
  CHECK(cpp.find("psWriteAll(fileStringDynamicFd, ps_string_table[fileStringDynamicIndex], "
                 "std::char_traits<char>::length(ps_string_table[fileStringDynamicIndex]))") !=
        std::string::npos);
  CHECK(cpp.find("int flushRc = ::fsync(flushFd);") != std::string::npos);
  CHECK(cpp.find("int closeRc = ::close(closeFd);") != std::string::npos);
}

TEST_CASE("ir to cpp emitter writes file write u64 opcode") {
  primec::IrToCppEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  module.stringTable.push_back("/dev/null");
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::FileOpenWrite, 0});
  fn.instructions.push_back({primec::IrOpcode::PushI64, static_cast<uint64_t>(42)});
  fn.instructions.push_back({primec::IrOpcode::FileWriteU64, 0});
  fn.instructions.push_back({primec::IrOpcode::Pop, 0});
  fn.instructions.push_back({primec::IrOpcode::FileClose, 0});
  fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(fn);

  std::string cpp;
  std::string error;
  REQUIRE(emitter.emitSource(module, cpp, error));
  CHECK(error.empty());
  CHECK(cpp.find("uint64_t fileU64Value = stack[--sp];") != std::string::npos);
  CHECK(cpp.find("int fileU64Fd = static_cast<int>(fileU64Handle & 0xffffffffu);") != std::string::npos);
  CHECK(cpp.find("std::string fileU64Text = std::to_string(static_cast<unsigned long long>(fileU64Value));") !=
        std::string::npos);
  CHECK(cpp.find("psWriteAll(fileU64Fd, fileU64Text.data(), fileU64Text.size())") != std::string::npos);
}

TEST_CASE("ir to cpp emitter writes file write i32 opcode") {
  primec::IrToCppEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  module.stringTable.push_back("/dev/null");
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::FileOpenWrite, 0});
  fn.instructions.push_back({primec::IrOpcode::PushI32, static_cast<uint64_t>(-17)});
  fn.instructions.push_back({primec::IrOpcode::FileWriteI32, 0});
  fn.instructions.push_back({primec::IrOpcode::Pop, 0});
  fn.instructions.push_back({primec::IrOpcode::FileClose, 0});
  fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(fn);

  std::string cpp;
  std::string error;
  REQUIRE(emitter.emitSource(module, cpp, error));
  CHECK(error.empty());
  CHECK(cpp.find("int64_t fileI32Value = static_cast<int64_t>(static_cast<int32_t>(stack[--sp]));") !=
        std::string::npos);
  CHECK(cpp.find("int fileI32Fd = static_cast<int>(fileI32Handle & 0xffffffffu);") != std::string::npos);
  CHECK(cpp.find("std::string fileI32Text = std::to_string(fileI32Value);") != std::string::npos);
  CHECK(cpp.find("psWriteAll(fileI32Fd, fileI32Text.data(), fileI32Text.size())") != std::string::npos);
}

TEST_CASE("ir to cpp emitter writes file write i64 opcode") {
  primec::IrToCppEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  module.stringTable.push_back("/dev/null");
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::FileOpenWrite, 0});
  fn.instructions.push_back({primec::IrOpcode::PushI64, static_cast<uint64_t>(-19)});
  fn.instructions.push_back({primec::IrOpcode::FileWriteI64, 0});
  fn.instructions.push_back({primec::IrOpcode::Pop, 0});
  fn.instructions.push_back({primec::IrOpcode::FileClose, 0});
  fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(fn);

  std::string cpp;
  std::string error;
  REQUIRE(emitter.emitSource(module, cpp, error));
  CHECK(error.empty());
  CHECK(cpp.find("int64_t fileI64Value = static_cast<int64_t>(stack[--sp]);") != std::string::npos);
  CHECK(cpp.find("int fileI64Fd = static_cast<int>(fileI64Handle & 0xffffffffu);") != std::string::npos);
  CHECK(cpp.find("std::string fileI64Text = std::to_string(fileI64Value);") != std::string::npos);
  CHECK(cpp.find("psWriteAll(fileI64Fd, fileI64Text.data(), fileI64Text.size())") != std::string::npos);
}

TEST_CASE("ir to cpp emitter writes file write byte opcode") {
  primec::IrToCppEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  module.stringTable.push_back("/dev/null");
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::FileOpenWrite, 0});
  fn.instructions.push_back({primec::IrOpcode::PushI32, static_cast<uint64_t>(0x41)});
  fn.instructions.push_back({primec::IrOpcode::FileWriteByte, 0});
  fn.instructions.push_back({primec::IrOpcode::Pop, 0});
  fn.instructions.push_back({primec::IrOpcode::FileClose, 0});
  fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(fn);

  std::string cpp;
  std::string error;
  REQUIRE(emitter.emitSource(module, cpp, error));
  CHECK(error.empty());
  CHECK(cpp.find("uint8_t fileByteValue = static_cast<uint8_t>(stack[--sp] & 0xffu);") != std::string::npos);
  CHECK(cpp.find("int fileByteFd = static_cast<int>(fileByteHandle & 0xffffffffu);") != std::string::npos);
  CHECK(cpp.find("psWriteAll(fileByteFd, &fileByteValue, 1)") != std::string::npos);
}

TEST_CASE("ir to cpp emitter writes file write newline opcode") {
  primec::IrToCppEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  module.stringTable.push_back("/dev/null");
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::FileOpenWrite, 0});
  fn.instructions.push_back({primec::IrOpcode::FileWriteNewline, 0});
  fn.instructions.push_back({primec::IrOpcode::Pop, 0});
  fn.instructions.push_back({primec::IrOpcode::FileClose, 0});
  fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(fn);

  std::string cpp;
  std::string error;
  REQUIRE(emitter.emitSource(module, cpp, error));
  CHECK(error.empty());
  CHECK(cpp.find("int fileLineFd = static_cast<int>(fileLineHandle & 0xffffffffu);") != std::string::npos);
  CHECK(cpp.find("char fileLineValue = '\\n';") != std::string::npos);
  CHECK(cpp.find("psWriteAll(fileLineFd, &fileLineValue, 1)") != std::string::npos);
}

TEST_CASE("ir to cpp emitter writes file write string opcode") {
  primec::IrToCppEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  module.stringTable.push_back("/dev/null");
  module.stringTable.push_back("hello");
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::FileOpenWrite, 0});
  fn.instructions.push_back({primec::IrOpcode::FileWriteString, 1});
  fn.instructions.push_back({primec::IrOpcode::Pop, 0});
  fn.instructions.push_back({primec::IrOpcode::FileClose, 0});
  fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(fn);

  std::string cpp;
  std::string error;
  REQUIRE(emitter.emitSource(module, cpp, error));
  CHECK(error.empty());
  CHECK(cpp.find("int fileStringFd = static_cast<int>(fileStringHandle & 0xffffffffu);") != std::string::npos);
  CHECK(cpp.find("psWriteAll(fileStringFd, ps_string_table[1], 5ull)") != std::string::npos);
}

TEST_CASE("ir to cpp emitter writes file open read opcode") {
  primec::IrToCppEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  module.stringTable.push_back("/dev/null");
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::FileOpenRead, 0});
  fn.instructions.push_back({primec::IrOpcode::FileClose, 0});
  fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(fn);

  std::string cpp;
  std::string error;
  REQUIRE(emitter.emitSource(module, cpp, error));
  CHECK(error.empty());
  CHECK(cpp.find("int fileOpenFlags = O_RDONLY;") != std::string::npos);
  CHECK(cpp.find("int fileFd = ::open(ps_string_table[0], fileOpenFlags, 0644);") != std::string::npos);
}

TEST_CASE("ir to cpp emitter writes file open write opcode") {
  primec::IrToCppEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  module.stringTable.push_back("/dev/null");
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::FileOpenWrite, 0});
  fn.instructions.push_back({primec::IrOpcode::FileClose, 0});
  fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(fn);

  std::string cpp;
  std::string error;
  REQUIRE(emitter.emitSource(module, cpp, error));
  CHECK(error.empty());
  CHECK(cpp.find("int fileOpenFlags = O_WRONLY | O_CREAT | O_TRUNC;") != std::string::npos);
  CHECK(cpp.find("int fileFd = ::open(ps_string_table[0], fileOpenFlags, 0644);") != std::string::npos);
}

TEST_CASE("ir to cpp emitter writes file open append opcode") {
  primec::IrToCppEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  module.stringTable.push_back("/dev/null");
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::FileOpenAppend, 0});
  fn.instructions.push_back({primec::IrOpcode::FileClose, 0});
  fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(fn);

  std::string cpp;
  std::string error;
  REQUIRE(emitter.emitSource(module, cpp, error));
  CHECK(error.empty());
  CHECK(cpp.find("int fileOpenFlags = O_WRONLY | O_CREAT | O_APPEND;") != std::string::npos);
  CHECK(cpp.find("int fileFd = ::open(ps_string_table[0], fileOpenFlags, 0644);") != std::string::npos);
}

TEST_CASE("ir to cpp emitter omits file io helpers when unused") {
  primec::IrToCppEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::PushI32, 3});
  fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(fn);

  std::string cpp;
  std::string error;
  REQUIRE(emitter.emitSource(module, cpp, error));
  CHECK(error.empty());
  CHECK(cpp.find("#include <cerrno>") == std::string::npos);
  CHECK(cpp.find("#include <fcntl.h>") == std::string::npos);
  CHECK(cpp.find("#include <unistd.h>") == std::string::npos);
  CHECK(cpp.find("static uint32_t psWriteAll(int fd, const void *data, std::size_t size)") == std::string::npos);
}
