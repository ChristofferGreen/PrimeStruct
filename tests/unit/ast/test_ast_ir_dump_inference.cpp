#include "test_ast_ir_dump_helpers.h"

TEST_SUITE_BEGIN("primestruct.dumps.ast_ir");

TEST_CASE("ir dump infers return type without transform") {
  const std::string source = R"(
main() {
  return(1.5f)
}
)";
  const auto program = parseProgram(source);
  primec::IrPrinter printer;
  const std::string dump = printer.print(program);
  const std::string expected =
      "module {\n"
      "  def /main(): f32 {\n"
      "    return 1.5f32\n"
      "  }\n"
      "}\n";
  CHECK(dump == expected);
}

TEST_CASE("ir dump infers return type from builtin plus") {
  const std::string source = R"(
main() {
  return(plus(1i32, 2i32))
}
)";
  const auto program = parseProgram(source);
  primec::IrPrinter printer;
  const std::string dump = printer.print(program);
  const std::string expected =
      "module {\n"
      "  def /main(): i32 {\n"
      "    return plus(1, 2)\n"
      "  }\n"
      "}\n";
  CHECK(dump == expected);
}

TEST_CASE("ir dump infers return type from builtin clamp") {
  const std::string source = R"(
import /std/math/*
main() {
  return(clamp(1.5f, 0.0f, 2.0f))
}
)";
  const auto program = parseProgram(source);
  primec::IrPrinter printer;
  const std::string dump = printer.print(program);
  const std::string expected =
      "module {\n"
      "  def /main(): f32 {\n"
      "    return clamp(1.5f32, 0.0f32, 2.0f32)\n"
      "  }\n"
      "}\n";
  CHECK(dump == expected);
}

TEST_CASE("ir dump prints transformed execution return inference") {
  const std::string source = R"(
[return<int>]
main() {
  [effects(io_out)] print_line("hi"utf8)
  return(1i32)
}
)";
  const auto program = parseProgramWithFilters(source);
  primec::IrPrinter printer;
  const std::string dump = printer.print(program);
  CHECK(dump.find("def /main(): i32") != std::string::npos);
  CHECK(dump.find("call print_line(\"hi\"utf8)") != std::string::npos);
}

TEST_CASE("ir dump prints local bindings before control flow") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{1i32}
  if(true) {
    assign(value, 2i32)
  }
  return(value)
}
)";
  const auto program = parseProgram(source);
  primec::IrPrinter printer;
  const std::string dump = printer.print(program);
  CHECK(dump.find("let value: i32 = 1") != std::string::npos);
  CHECK(dump.find("call if") != std::string::npos);
  CHECK(dump.find("assign(value, 2)") != std::string::npos);
}

TEST_CASE("ir dump prints nested block return inference") {
  const std::string source = R"(
main() {
  return(block() {
    [i32] value{4i32}
    value
  })
}
)";
  const auto program = parseProgram(source);
  primec::IrPrinter printer;
  const std::string dump = printer.print(program);
  CHECK(dump.find("def /main(): i32") != std::string::npos);
  CHECK(dump.find("return block() {") != std::string::npos);
  CHECK(dump.find("4") != std::string::npos);
}

TEST_CASE("ir dump infers return type from named helper call") {
  const std::string source = R"(
[return<int>]
helper() {
  return(7i32)
}

main() {
  return(helper())
}
)";
  const auto program = parseProgram(source);
  primec::IrPrinter printer;
  const std::string dump = printer.print(program);
  CHECK(dump.find("def /main(): i32") != std::string::npos);
  CHECK(dump.find("return helper()") != std::string::npos);
}

TEST_CASE("ir dump keeps inferred type on local return variable") {
  const std::string source = R"(
main() {
  [mut] value{1i32}
  assign(value, plus(value, 2i32))
  return(value)
}
)";
  const auto program = parseProgram(source);
  primec::IrPrinter printer;
  const std::string dump = printer.print(program);
  CHECK(dump.find("let value = 1") != std::string::npos);
  CHECK(dump.find("assign value, plus(value, 2)") != std::string::npos);
}

TEST_SUITE_END();
