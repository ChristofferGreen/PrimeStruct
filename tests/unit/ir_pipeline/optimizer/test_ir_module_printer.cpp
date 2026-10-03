#include "primec/ir/IrModulePrinter.h"

#include "test_ir_optimizer_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.module_printer");

using optimizer_test::assemble;
using optimizer_test::moduleOf;

TEST_CASE("module listing is stable and decodes immediates") {
  primec::IrModule module = moduleOf(assemble({
      "PushI32 -5",
      "PushI64 9000000000",
      "PushF32 0x40200000",
      "PushF64 0x3ff0000000000000",
      "StoreLocal 2",
      "LoadLocal 2",
      "JumpIfZero 9",
      "AddI32",
      "Jump 10",
      "PrintI32",
      "ReturnVoid",
  }));
  module.stringTable = {"hi\n", "quote\"d"};
  module.functions[0].metadata.effectMask = primec::EffectIoOut;
  module.functions[0].metadata.capabilityMask = primec::EffectIoOut;
  const std::string text = primec::formatIrModule(module);
  const std::string expected =
      "ir_module_v1 schema=27\n"
      "entry=/main (function 0)\n"
      "string_table: 2\n"
      "  0: \"hi\\n\"\n"
      "  1: \"quote\\\"d\"\n"
      "host_imports: 0\n"
      "struct_layouts: 0\n"
      "functions: 1\n"
      "function 0 /main parameters=0 effects=0x1 capabilities=0x1 locals=3 instructions=11\n"
      "  0000  PushI32 -5\n"
      "  0001  PushI64 9000000000\n"
      "  0002  PushF32 0x40200000 (2.5)\n"
      "  0003  PushF64 0x3ff0000000000000 (1)\n"
      "  0004  StoreLocal local 2\n"
      "  0005  LoadLocal local 2\n"
      "  0006  JumpIfZero -> 9\n"
      "  0007  AddI32\n"
      "  0008  Jump -> 10\n"
      "  0009  PrintI32 flags=none\n"
      "  0010  ReturnVoid\n";
  CHECK(text == expected);
  CHECK(primec::formatIrModule(module) == text);
}

TEST_CASE("calls, host imports, strings and print flags are named") {
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction callee;
  callee.name = "/helper";
  callee.parameterCount = 1;
  callee.instructions = assemble({"StoreLocal 0", "ReturnVoid"});
  primec::IrFunction entry;
  entry.name = "/main";
  entry.instructions = assemble({"PushI32 1", "CallVoid 1", "CallHost 0", "ReturnVoid"});
  entry.instructions.push_back(
      {primec::IrOpcode::PrintString,
       primec::encodePrintStringImm(1, primec::PrintFlagNewline | primec::PrintFlagStderr)});
  entry.instructions.push_back({primec::IrOpcode::FileOpenRead, 0});
  module.functions.push_back(std::move(entry));
  module.functions.push_back(std::move(callee));
  module.stringTable = {"/tmp/x", "msg"};
  primec::IrHostImport import;
  import.name = "host.tick";
  import.parameters = {primec::IrHostValueKind::I32, primec::IrHostValueKind::String};
  import.returnKind = primec::IrHostValueKind::Bool;
  module.hostImports.push_back(import);

  const std::string text = primec::formatIrModule(module);
  CHECK(text.find("host_imports: 1\n  0: host.tick(i32, string) -> bool\n") != std::string::npos);
  CHECK(text.find("CallVoid 1 /helper") != std::string::npos);
  CHECK(text.find("CallHost 0 host.tick") != std::string::npos);
  CHECK(text.find("PrintString #1 \"msg\" flags=newline|stderr") != std::string::npos);
  CHECK(text.find("FileOpenRead #0 \"/tmp/x\"") != std::string::npos);
}

TEST_CASE("every opcode formats to its own name without crashing") {
  const primec::IrModule module;
  for (const primec::IrOpcodeInfo &info : primec::IrOpcodeTable) {
    CAPTURE(info.name);
    const std::string text = primec::formatIrInstruction(module, {info.op, 0});
    CHECK(text.rfind(info.name, 0) == 0);
  }
}

TEST_CASE("non-finite floats print by name and invalid references are marked") {
  const primec::IrModule module;
  CHECK(primec::formatIrInstruction(module, {primec::IrOpcode::PushF32, 0x7fc00000u}) ==
        "PushF32 0x7fc00000 (nan)");
  CHECK(primec::formatIrInstruction(module, {primec::IrOpcode::PushF64, 0xfff0000000000000ull}) ==
        "PushF64 0xfff0000000000000 (-inf)");
  CHECK(primec::formatIrInstruction(module, {primec::IrOpcode::FileOpenRead, 3}) ==
        "FileOpenRead #3 <invalid>");
  CHECK(primec::formatIrInstruction(module, {primec::IrOpcode::Call, 9}) == "Call 9");
}
