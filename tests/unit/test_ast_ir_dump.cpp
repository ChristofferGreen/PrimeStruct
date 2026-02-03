#include "primec/AstPrinter.h"
#include "primec/IrPrinter.h"
#include "primec/Lexer.h"
#include "primec/Parser.h"

#include "third_party/doctest.h"

namespace {
primec::Program parseProgram(const std::string &source) {
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK(parser.parse(program.definitions, program.executions, error));
  CHECK(error.empty());
  return program;
}
} // namespace

TEST_SUITE_BEGIN("primestruct.dumps");

TEST_CASE("ast dump matches expected format") {
  const std::string source = R"(
[return<int>]
main() {
  return(7i32)
}
)";
  const auto program = parseProgram(source);
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

TEST_SUITE_END();
