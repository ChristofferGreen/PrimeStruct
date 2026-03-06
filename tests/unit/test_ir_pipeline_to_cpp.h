#include <string>

TEST_SUITE_BEGIN("primestruct.ir.pipeline.to_cpp");

namespace {

primec::IrModule makeSimpleI32AddModuleForCpp() {
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::PushI32, 2});
  fn.instructions.push_back({primec::IrOpcode::PushI32, 3});
  fn.instructions.push_back({primec::IrOpcode::AddI32, 0});
  fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(fn);
  return module;
}

} // namespace

TEST_CASE("ir to cpp emitter writes numeric stack program") {
  primec::IrToCppEmitter emitter;
  const primec::IrModule module = makeSimpleI32AddModuleForCpp();
  std::string cpp;
  std::string error;
  REQUIRE(emitter.emitSource(module, cpp, error));
  CHECK(error.empty());
  CHECK(cpp.find("static int64_t ps_entry_0(int argc, char **argv)") != std::string::npos);
  CHECK(cpp.find("switch (pc)") != std::string::npos);
  CHECK(cpp.find("case 0") != std::string::npos);
  CHECK(cpp.find("stack[sp++] = 2;") != std::string::npos);
  CHECK(cpp.find("stack[sp++] = 3;") != std::string::npos);
  CHECK(cpp.find("stack[sp++] = left + right;") != std::string::npos);
  CHECK(cpp.find("return static_cast<int64_t>(static_cast<int32_t>(stack[--sp]));") != std::string::npos);
  CHECK(cpp.find("int main(int argc, char **argv)") != std::string::npos);
}

TEST_CASE("ir to cpp emitter writes i64 arithmetic and comparisons") {
  primec::IrToCppEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::PushI64, static_cast<uint64_t>(11)});
  fn.instructions.push_back({primec::IrOpcode::PushI64, static_cast<uint64_t>(5)});
  fn.instructions.push_back({primec::IrOpcode::SubI64, 0});
  fn.instructions.push_back({primec::IrOpcode::PushI64, static_cast<uint64_t>(6)});
  fn.instructions.push_back({primec::IrOpcode::CmpEqI64, 0});
  fn.instructions.push_back({primec::IrOpcode::ReturnI64, 0});
  module.functions.push_back(fn);

  std::string cpp;
  std::string error;
  REQUIRE(emitter.emitSource(module, cpp, error));
  CHECK(error.empty());
  CHECK(cpp.find("int64_t right = static_cast<int64_t>(stack[--sp]);") != std::string::npos);
  CHECK(cpp.find("stack[sp++] = static_cast<uint64_t>(left - right);") != std::string::npos);
  CHECK(cpp.find("stack[sp++] = (left == right) ? 1u : 0u;") != std::string::npos);
  CHECK(cpp.find("return static_cast<int64_t>(stack[--sp]);") != std::string::npos);
}

TEST_CASE("ir to cpp emitter writes f64 to i32 conversion") {
  primec::IrToCppEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::PushF64, 0});
  fn.instructions.push_back({primec::IrOpcode::ConvertF64ToI32, 0});
  fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(fn);

  std::string cpp;
  std::string error;
  REQUIRE(emitter.emitSource(module, cpp, error));
  CHECK(error.empty());
  CHECK(cpp.find("double value = psBitsToF64(stack[--sp]);") != std::string::npos);
  CHECK(cpp.find("stack[sp++] = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(value)));") !=
        std::string::npos);
}

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

TEST_CASE("ir to cpp emitter writes jump and conditional jump control flow") {
  primec::IrToCppEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::PushI32, 0});
  fn.instructions.push_back({primec::IrOpcode::JumpIfZero, 4});
  fn.instructions.push_back({primec::IrOpcode::PushI32, 99});
  fn.instructions.push_back({primec::IrOpcode::Jump, 5});
  fn.instructions.push_back({primec::IrOpcode::PushI32, 7});
  fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(fn);

  std::string cpp;
  std::string error;
  REQUIRE(emitter.emitSource(module, cpp, error));
  CHECK(error.empty());
  CHECK(cpp.find("pc = (cond == 0) ? 4 : 2;") != std::string::npos);
  CHECK(cpp.find("pc = 5;") != std::string::npos);
}

TEST_CASE("ir to cpp emitter writes call and callvoid dispatch") {
  primec::IrToCppEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;

  primec::IrFunction mainFn;
  mainFn.name = "/main";
  mainFn.instructions.push_back({primec::IrOpcode::CallVoid, 1});
  mainFn.instructions.push_back({primec::IrOpcode::Call, 2});
  mainFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(mainFn);

  primec::IrFunction voidFn;
  voidFn.name = "/log";
  voidFn.instructions.push_back({primec::IrOpcode::PushI32, 7});
  voidFn.instructions.push_back({primec::IrOpcode::PrintI32, primec::PrintFlagNewline});
  voidFn.instructions.push_back({primec::IrOpcode::ReturnVoid, 0});
  module.functions.push_back(voidFn);

  primec::IrFunction valueFn;
  valueFn.name = "/value";
  valueFn.instructions.push_back({primec::IrOpcode::PushI32, 42});
  valueFn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(valueFn);

  std::string cpp;
  std::string error;
  REQUIRE(emitter.emitSource(module, cpp, error));
  CHECK(error.empty());
  CHECK(cpp.find("static int64_t ps_fn_0(uint64_t *stack, std::size_t &sp, int argc, char **argv);") !=
        std::string::npos);
  CHECK(cpp.find("ps_fn_1(stack, sp, argc, argv);") != std::string::npos);
  CHECK(cpp.find("int64_t callValue = ps_fn_2(stack, sp, argc, argv);") != std::string::npos);
  CHECK(cpp.find("stack[sp++] = static_cast<uint64_t>(callValue);") != std::string::npos);
  CHECK(cpp.find("return ps_fn_0(stack, sp, argc, argv);") != std::string::npos);
}

TEST_CASE("ir to cpp emitter rejects unsupported opcodes") {
  primec::IrToCppEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::FileClose, 0});
  fn.instructions.push_back({primec::IrOpcode::ReturnVoid, 0});
  module.functions.push_back(fn);

  std::string cpp;
  std::string error;
  CHECK_FALSE(emitter.emitSource(module, cpp, error));
  CHECK(error.find("unsupported opcode") != std::string::npos);
}

TEST_CASE("ir to cpp emitter rejects invalid entry index") {
  primec::IrToCppEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 1;
  module.functions.push_back({});

  std::string cpp;
  std::string error;
  CHECK_FALSE(emitter.emitSource(module, cpp, error));
  CHECK(error == "IrToCppEmitter invalid IR entry index");
}

TEST_CASE("ir to cpp emitter rejects out-of-range local indices") {
  primec::IrToCppEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::PushI32, 1});
  fn.instructions.push_back({primec::IrOpcode::StoreLocal, 2048});
  fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(fn);

  std::string cpp;
  std::string error;
  CHECK_FALSE(emitter.emitSource(module, cpp, error));
  CHECK(error.find("local index out of range") != std::string::npos);
}

TEST_CASE("ir to cpp emitter rejects out-of-range address-of-local index") {
  primec::IrToCppEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::AddressOfLocal, 4096});
  fn.instructions.push_back({primec::IrOpcode::ReturnI64, 0});
  module.functions.push_back(fn);

  std::string cpp;
  std::string error;
  CHECK_FALSE(emitter.emitSource(module, cpp, error));
  CHECK(error.find("local index out of range") != std::string::npos);
}

TEST_CASE("ir to cpp emitter rejects out-of-range call targets") {
  primec::IrToCppEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::Call, 2});
  fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(fn);

  std::string cpp;
  std::string error;
  CHECK_FALSE(emitter.emitSource(module, cpp, error));
  CHECK(error.find("call target out of range") != std::string::npos);
}

TEST_CASE("ir to cpp emitter rejects out-of-range jump targets") {
  primec::IrToCppEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::Jump, 99});
  fn.instructions.push_back({primec::IrOpcode::ReturnVoid, 0});
  module.functions.push_back(fn);

  std::string cpp;
  std::string error;
  CHECK_FALSE(emitter.emitSource(module, cpp, error));
  CHECK(error.find("jump target out of range") != std::string::npos);
}

TEST_CASE("ir to cpp emitter rejects out-of-range string indices") {
  primec::IrToCppEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::PrintString, primec::encodePrintStringImm(3, 0)});
  fn.instructions.push_back({primec::IrOpcode::ReturnVoid, 0});
  module.functions.push_back(fn);

  std::string cpp;
  std::string error;
  CHECK_FALSE(emitter.emitSource(module, cpp, error));
  CHECK(error.find("string index out of range") != std::string::npos);
}

TEST_CASE("ir to cpp emitter rejects out-of-range string byte load indices") {
  primec::IrToCppEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction fn;
  fn.name = "/main";
  fn.instructions.push_back({primec::IrOpcode::PushI32, 0});
  fn.instructions.push_back({primec::IrOpcode::LoadStringByte, 2});
  fn.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(fn);

  std::string cpp;
  std::string error;
  CHECK_FALSE(emitter.emitSource(module, cpp, error));
  CHECK(error.find("string index out of range") != std::string::npos);
}

TEST_SUITE_END();
