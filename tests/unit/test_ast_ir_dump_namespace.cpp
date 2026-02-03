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

TEST_SUITE_BEGIN("primestruct.dumps.namespace");

TEST_CASE("ast dump includes namespace paths") {
  const std::string source = R"(
namespace demo {
  [return<int>]
  main() {
    return(2i32)
  }
}
)";
  const auto program = parseProgram(source);
  primec::AstPrinter printer;
  const std::string dump = printer.print(program);
  CHECK(dump.find("/demo/main") != std::string::npos);
}

TEST_CASE("ir dump includes namespace paths") {
  const std::string source = R"(
namespace demo {
  [return<int>]
  main() {
    return(4i32)
  }
}
)";
  const auto program = parseProgram(source);
  primec::IrPrinter printer;
  const std::string dump = printer.print(program);
  CHECK(dump.find("/demo/main") != std::string::npos);
}

TEST_SUITE_END();
