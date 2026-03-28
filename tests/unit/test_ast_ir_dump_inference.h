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
  return(clamp(2i32, 1i32, 5i32))
}
)";
  const auto program = parseProgram(source);
  primec::IrPrinter printer;
  const std::string dump = printer.print(program);
  const std::string expected =
      "module {\n"
      "  def /main(): i32 {\n"
      "    return clamp(2, 1, 5)\n"
      "  }\n"
      "}\n";
  CHECK(dump == expected);
}

TEST_CASE("ir dump infers return type from builtin min") {
  const std::string source = R"(
import /std/math/*
main() {
  return(min(2i32, 5i32))
}
)";
  const auto program = parseProgram(source);
  primec::IrPrinter printer;
  const std::string dump = printer.print(program);
  const std::string expected =
      "module {\n"
      "  def /main(): i32 {\n"
      "    return min(2, 5)\n"
      "  }\n"
      "}\n";
  CHECK(dump == expected);
}

TEST_CASE("ir dump infers return type from builtin abs") {
  const std::string source = R"(
import /std/math/*
main() {
  return(abs(negate(2i32)))
}
)";
  const auto program = parseProgram(source);
  primec::IrPrinter printer;
  const std::string dump = printer.print(program);
  const std::string expected =
      "module {\n"
      "  def /main(): i32 {\n"
      "    return abs(negate(2))\n"
      "  }\n"
      "}\n";
  CHECK(dump == expected);
}

TEST_CASE("ir dump infers return type from builtin saturate") {
  const std::string source = R"(
import /std/math/*
main() {
  return(saturate(2i32))
}
)";
  const auto program = parseProgram(source);
  primec::IrPrinter printer;
  const std::string dump = printer.print(program);
  const std::string expected =
      "module {\n"
      "  def /main(): i32 {\n"
      "    return saturate(2)\n"
      "  }\n"
      "}\n";
  CHECK(dump == expected);
}

TEST_CASE("ir dump infers void return without transform") {
  const std::string source = R"(
main() {
  return()
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

TEST_CASE("ir dump prints collection literal calls") {
  const std::string source = R"(
[return<int>]
main() {
  array<i32>{1i32, 2i32}
  map<i32, i32>{1i32, 2i32}
  return(1i32)
}
)";
  const auto program = parseProgramWithFilters(source);
  primec::IrPrinter printer;
  const std::string dump = printer.print(program);
  const std::string expected =
      "module {\n"
      "  def /main(): i32 {\n"
      "    call array(1, 2)\n"
      "    call map(1, 2)\n"
      "    return 1\n"
      "  }\n"
      "}\n";
  CHECK(dump == expected);
}

TEST_CASE("ir dump prints convert call") {
  const std::string source = R"(
[return<int>]
main() {
  return(convert<int>(1.5f))
}
)";
  const auto program = parseProgram(source);
  primec::IrPrinter printer;
  const std::string dump = printer.print(program);
  const std::string expected =
      "module {\n"
      "  def /main(): i32 {\n"
      "    return convert(1.5f32)\n"
      "  }\n"
      "}\n";
  CHECK(dump == expected);
}

TEST_CASE("ir dump prints mixed named arguments") {
  const std::string source = R"(
[return<int>]
sum3([i32] a, [i32] b, [i32] c) {
  return(plus(plus(a, b), c))
}

[return<int>]
main() {
  return(sum3(1i32, [c] 3i32, [b] 2i32))
}
)";
  const auto program = parseProgram(source);
  primec::IrPrinter printer;
  const std::string dump = printer.print(program);
  const std::string expected =
      "module {\n"
      "  def /sum3([i32] a, [i32] b, [i32] c): i32 {\n"
      "    return plus(plus(a, b), c)\n"
      "  }\n"
      "  def /main(): i32 {\n"
      "    return sum3(1, [c] 3, [b] 2)\n"
      "  }\n"
      "}\n";
  CHECK(dump == expected);
}

TEST_CASE("ir dump prints map literal with named-arg value") {
  const std::string source = R"(
[return<int>]
make_color([i32] hue, [i32] value) {
  return(plus(hue, value))
}

[return<int>]
main() {
  map<i32, i32>{1i32=make_color([hue] 2i32, [value] 3i32)}
  return(1i32)
}
)";
  const auto program = parseProgramWithFilters(source);
  primec::IrPrinter printer;
  const std::string dump = printer.print(program);
  const std::string expected =
      "module {\n"
      "  def /make_color([i32] hue, [i32] value): i32 {\n"
      "    return plus(hue, value)\n"
      "  }\n"
      "  def /main(): i32 {\n"
      "    call map(1, make_color([hue] 2, [value] 3))\n"
      "    return 1\n"
      "  }\n"
      "}\n";
  CHECK(dump == expected);
}

TEST_CASE("ast dump prints execution arguments") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

execute_repeat(2i32)
)";
  const auto program = parseProgram(source);
  primec::AstPrinter printer;
  const std::string dump = printer.print(program);
  const std::string expected =
      "ast {\n"
      "  [return<int>] /main() {\n"
      "    return 1\n"
      "  }\n"
      "  /execute_repeat(2)\n"
      "}\n";
  CHECK(dump == expected);
}

TEST_CASE("ir dump prints execution arguments") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

execute_repeat(2i32)
)";
  const auto program = parseProgram(source);
  primec::IrPrinter printer;
  const std::string dump = printer.print(program);
  const std::string expected =
      "module {\n"
      "  def /main(): i32 {\n"
      "    return 1\n"
      "  }\n"
      "  exec /execute_repeat(2)\n"
      "}\n";
  CHECK(dump == expected);
}

TEST_CASE("ast dump prints execution named arguments with collections") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

execute_task([items] array<i32>{1i32, 2i32}, [pairs] map<i32, i32>{1i32=2i32})
)";
  const auto program = parseProgramWithFilters(source);
  primec::AstPrinter printer;
  const std::string dump = printer.print(program);
  const std::string expected =
      "ast {\n"
      "  [return<int>] /main() {\n"
      "    return 1\n"
      "  }\n"
      "  /execute_task([items] array<i32>(1, 2), [pairs] map<i32, i32>(1, 2))\n"
      "}\n";
  CHECK(dump == expected);
}

TEST_CASE("ir dump prints execution named arguments with collections") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

execute_task([items] array<i32>{1i32, 2i32}, [pairs] map<i32, i32>{1i32=2i32})
)";
  const auto program = parseProgramWithFilters(source);
  primec::IrPrinter printer;
  const std::string dump = printer.print(program);
  const std::string expected =
      "module {\n"
      "  def /main(): i32 {\n"
      "    return 1\n"
      "  }\n"
      "  exec /execute_task([items] array(1, 2), [pairs] map(1, 2))\n"
      "}\n";
  CHECK(dump == expected);
}

TEST_CASE("ir dump prints execution named arguments") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

execute_repeat([count] 2i32)
)";
  const auto program = parseProgram(source);
  primec::IrPrinter printer;
  const std::string dump = printer.print(program);
  const std::string expected =
      "module {\n"
      "  def /main(): i32 {\n"
      "    return 1\n"
      "  }\n"
      "  exec /execute_repeat([count] 2)\n"
      "}\n";
  CHECK(dump == expected);
}

TEST_CASE("ast dump prints execution named arguments") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

execute_repeat([count] 2i32)
)";
  const auto program = parseProgram(source);
  primec::AstPrinter printer;
  const std::string dump = printer.print(program);
  const std::string expected =
      "ast {\n"
      "  [return<int>] /main() {\n"
      "    return 1\n"
      "  }\n"
      "  /execute_repeat([count] 2)\n"
      "}\n";
  CHECK(dump == expected);
}

TEST_CASE("ir dump prints local bindings") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{4i32}
  assign(value, 9i32)
  return(value)
}
)";
  const auto program = parseProgram(source);
  primec::IrPrinter printer;
  const std::string dump = printer.print(program);
  const std::string expected =
      "module {\n"
      "  def /main(): i32 {\n"
      "    let value: i32 = 4\n"
      "    assign value, 9\n"
      "    return value\n"
      "  }\n"
      "}\n";
  CHECK(dump == expected);
}

TEST_CASE("ir dump prints unary minus rewrite") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{3i32}
  return(-value)
}
)";
  const auto program = parseProgramWithFilters(source);
  primec::IrPrinter printer;
  const std::string dump = printer.print(program);
  const std::string expected =
      "module {\n"
      "  def /main(): i32 {\n"
      "    let value: i32 = 3\n"
      "    return negate(value)\n"
      "  }\n"
      "}\n";
  CHECK(dump == expected);
}

TEST_CASE("ast dump prints early return") {
  const std::string source = R"(
[return<int>]
main() {
  if(true) {
    return(5i32)
  } else {
    return(2i32)
  }
}
)";
  const auto program = parseProgram(source);
  primec::AstPrinter printer;
  const std::string dump = printer.print(program);
  const std::string expected =
      "ast {\n"
      "  [return<int>] /main() {\n"
      "    if(true, then() { return(5) }, else() { return(2) })\n"
      "  }\n"
      "}\n";
  CHECK(dump == expected);
}

TEST_CASE("ir dump prints early return") {
  const std::string source = R"(
[return<int>]
main() {
  if(true) {
    return(5i32)
  } else {
    return(2i32)
  }
}
)";
  const auto program = parseProgram(source);
  primec::IrPrinter printer;
  const std::string dump = printer.print(program);
  const std::string expected =
      "module {\n"
      "  def /main(): i32 {\n"
      "    call if(true, then() { return(5) }, else() { return(2) })\n"
      "  }\n"
      "}\n";
  CHECK(dump == expected);
}

TEST_CASE("ir dump prints execution transforms") {
  const std::string source = R"(
[return<int>]
main() {
  return(3i32)
}

[effects]
run(1i32)
)";
  const auto program = parseProgram(source);
  primec::IrPrinter printer;
  const std::string dump = printer.print(program);
  CHECK(dump.find("exec [effects] /run(1)") != std::string::npos);
}

TEST_CASE("ir dump prints execution transform arguments") {
  const std::string source = R"(
[return<int>]
main() {
  return(3i32)
}

[effects(global_write, io_out)]
run(1i32)
)";
  const auto program = parseProgram(source);
  primec::IrPrinter printer;
  const std::string dump = printer.print(program);
  CHECK(dump.find("exec [effects(global_write, io_out)] /run(1)") != std::string::npos);
}
