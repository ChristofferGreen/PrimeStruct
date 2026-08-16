#include "test_ast_ir_dump_helpers.h"

TEST_SUITE_BEGIN("primestruct.dumps.ast_ir");

TEST_CASE("ast dump matches expected format") {
  const std::string source = R"(
[return<int>]
main() {
  return(7i32)
}
)";
  const auto program = parseProgramWithFilters(source);
  primec::AstPrinter printer;
  const std::string dump = printer.print(program);
  const std::string expected =
      "ast {\n"
      "  [return<int>] /main() {\n"
      "    return 7\n"
      "  }\n"
      "}\n";
  CHECK(dump == expected);
}

TEST_CASE("ir dump matches expected format") {
  const std::string source = R"(
[return<int>]
main() {
  return(9i32)
}
)";
  const auto program = parseProgram(source);
  primec::IrPrinter printer;
  const std::string dump = printer.print(program);
  const std::string expected =
      "module {\n"
      "  def /main(): i32 {\n"
      "    return 9\n"
      "  }\n"
      "}\n";
  CHECK(dump == expected);
}

TEST_CASE("ast dump prints void return") {
  const std::string source = R"(
[return<void>]
main() {
  return()
}
)";
  const auto program = parseProgram(source);
  primec::AstPrinter printer;
  const std::string dump = printer.print(program);
  const std::string expected =
      "ast {\n"
      "  [return<void>] /main() {\n"
      "    return\n"
      "  }\n"
      "}\n";
  CHECK(dump == expected);
}

TEST_CASE("ir dump prints void return") {
  const std::string source = R"(
[return<void>]
main() {
}
)";
  const auto program = parseProgram(source);
  primec::IrPrinter printer;
  const std::string dump = printer.print(program);
  const std::string expected =
      "module {\n"
      "  def /main(): void {\n"
      "    return\n"
      "  }\n"
      "}\n";
  CHECK(dump == expected);
}

TEST_CASE("ast dump prints string literals") {
  const std::string source = R"(
[return<void>]
main() {
  log("hello"utf8)
}
)";
  const auto program = parseProgram(source);
  primec::AstPrinter printer;
  const std::string dump = printer.print(program);
  const std::string expected =
      "ast {\n"
      "  [return<void>] /main() {\n"
      "    log(\"hello\"utf8)\n"
      "  }\n"
      "}\n";
  CHECK(dump == expected);
}

TEST_CASE("ir dump prints string literals") {
  const std::string source = R"(
[return<void>]
main() {
  log("hello"utf8)
}
)";
  const auto program = parseProgram(source);
  primec::IrPrinter printer;
  const std::string dump = printer.print(program);
  const std::string expected =
      "module {\n"
      "  def /main(): void {\n"
      "    call log(\"hello\"utf8)\n"
      "    return\n"
      "  }\n"
      "}\n";
  CHECK(dump == expected);
}

TEST_CASE("ast dump prints method call template args") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32.wrap<i32>())
}
)";
  const auto program = parseProgram(source);
  primec::AstPrinter printer;
  const std::string dump = printer.print(program);
  const std::string expected =
      "ast {\n"
      "  [return<int>] /main() {\n"
      "    return 1.wrap<i32>()\n"
      "  }\n"
      "}\n";
  CHECK(dump == expected);
}

TEST_CASE("ir dump prints method call template args") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32.wrap<i32>())
}
)";
  const auto program = parseProgram(source);
  primec::IrPrinter printer;
  const std::string dump = printer.print(program);
  const std::string expected =
      "module {\n"
      "  def /main(): i32 {\n"
      "    return 1.wrap<i32>()\n"
      "  }\n"
      "}\n";
  CHECK(dump == expected);
}

TEST_CASE("ast dump prints float literals") {
  const std::string source = R"(
[return<float>]
main() {
  return(1.25f)
}
)";
  const auto program = parseProgram(source);
  primec::AstPrinter printer;
  const std::string dump = printer.print(program);
  const std::string expected =
      "ast {\n"
      "  [return<float>] /main() {\n"
      "    return 1.25f32\n"
      "  }\n"
      "}\n";
  CHECK(dump == expected);
}

TEST_CASE("ir dump prints float literals") {
  const std::string source = R"(
[return<float>]
main() {
  return(1.25f)
}
)";
  const auto program = parseProgram(source);
  primec::IrPrinter printer;
  const std::string dump = printer.print(program);
  const std::string expected =
      "module {\n"
      "  def /main(): f32 {\n"
      "    return 1.25f32\n"
      "  }\n"
      "}\n";
  CHECK(dump == expected);
}

TEST_CASE("ast dump prints leading dot float literals") {
  const std::string source = R"(
[return<float>]
main() {
  return(.5f32)
}
)";
  const auto program = parseProgram(source);
  primec::AstPrinter printer;
  const std::string dump = printer.print(program);
  const std::string expected =
      "ast {\n"
      "  [return<float>] /main() {\n"
      "    return .5f32\n"
      "  }\n"
      "}\n";
  CHECK(dump == expected);
}

TEST_CASE("ir dump prints leading dot float literals") {
  const std::string source = R"(
[return<float>]
main() {
  return(.5f32)
}
)";
  const auto program = parseProgram(source);
  primec::IrPrinter printer;
  const std::string dump = printer.print(program);
  const std::string expected =
      "module {\n"
      "  def /main(): f32 {\n"
      "    return .5f32\n"
      "  }\n"
      "}\n";
  CHECK(dump == expected);
}

TEST_CASE("ir dump prints bool return type") {
  const std::string source = R"(
[return<bool>]
main() {
  [bool] flag{true}
  return(flag)
}
)";
  const auto program = parseProgram(source);
  primec::IrPrinter printer;
  const std::string dump = printer.print(program);
  const std::string expected =
      "module {\n"
      "  def /main(): bool {\n"
      "    let flag: bool = true\n"
      "    return flag\n"
      "  }\n"
      "}\n";
  CHECK(dump == expected);
}

TEST_CASE("ir dump prints array return type") {
  const std::string source = R"(
[return<array<i32>>]
main() {
  return(array<i32>{1i32, 2i32})
}
)";
  const auto program = parseProgram(source);
  primec::IrPrinter printer;
  const std::string dump = printer.print(program);
  const std::string expected =
      "module {\n"
      "  def /main(): array {\n"
      "    return array(1, 2)\n"
      "  }\n"
      "}\n";
  CHECK(dump == expected);
}

TEST_CASE("ast dump prints transform arguments") {
  const std::string source = R"(
[effects(global_write, io_out), return<int>]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  primec::AstPrinter printer;
  const std::string dump = printer.print(program);
  const std::string expected =
      "ast {\n"
      "  [effects(global_write, io_out), return<int>] /main() {\n"
      "    return 1\n"
      "  }\n"
      "}\n";
  CHECK(dump == expected);
}

TEST_CASE("ast dump prints struct definition") {
  const std::string source = R"(
[struct]
data() {
  [i32] value{1i32}
}
)";
  const auto program = parseProgram(source);
  primec::AstPrinter printer;
  const std::string dump = printer.print(program);
  const std::string expected =
      "ast {\n"
      "  [struct] /data() {\n"
      "    [i32] value{1}\n"
      "  }\n"
      "}\n";
  CHECK(dump == expected);
}

TEST_CASE("ir dump prints struct definition") {
  const std::string source = R"(
[struct]
data() {
  [i32] value{1i32}
}
)";
  const auto program = parseProgram(source);
  primec::IrPrinter printer;
  const std::string dump = printer.print(program);
  const std::string expected =
      "module {\n"
      "  def /data(): void {\n"
      "    let value: i32 = 1\n"
      "    return\n"
      "  }\n"
      "}\n";
  CHECK(dump == expected);
}

TEST_CASE("ir dump prints pod definition") {
  const std::string source = R"(
[pod]
data() {
  [i32] value{1i32}
}
)";
  const auto program = parseProgram(source);
  primec::IrPrinter printer;
  const std::string dump = printer.print(program);
  const std::string expected =
      "module {\n"
      "  def /data(): void {\n"
      "    let value: i32 = 1\n"
      "    return\n"
      "  }\n"
      "}\n";
  CHECK(dump == expected);
}

TEST_CASE("ast dump prints named arguments") {
  const std::string source = R"(
[return<int>]
main() {
  return(make_color([hue] 1i32, [value] 2i32))
}
)";
  const auto program = parseProgram(source);
  primec::AstPrinter printer;
  const std::string dump = printer.print(program);
  const std::string expected =
      "ast {\n"
      "  [return<int>] /main() {\n"
      "    return make_color([hue] 1, [value] 2)\n"
      "  }\n"
      "}\n";
  CHECK(dump == expected);
}

TEST_CASE("ir dump prints named arguments") {
  const std::string source = R"(
[return<int>]
main() {
  return(make_color([hue] 1i32, [value] 2i32))
}
)";
  const auto program = parseProgram(source);
  primec::IrPrinter printer;
  const std::string dump = printer.print(program);
  const std::string expected =
      "module {\n"
      "  def /main(): i32 {\n"
      "    return make_color([hue] 1, [value] 2)\n"
      "  }\n"
      "}\n";
  CHECK(dump == expected);
}
TEST_SUITE_END();
