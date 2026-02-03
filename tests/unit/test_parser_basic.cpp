#include "primec/Lexer.h"
#include "primec/Parser.h"

#include "third_party/doctest.h"

namespace {
primec::Program parseProgram(const std::string &source) {
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK(parser.parse(program, error));
  CHECK(error.empty());
  return program;
}
} // namespace

TEST_SUITE_BEGIN("primestruct.parser.basic");

TEST_CASE("parses minimal main definition") {
  const std::string source = R"(
[return<int>]
main() {
  return(7i32)
}
)";

  const auto program = parseProgram(source);
  CHECK(program.definitions.size() == 1);
  CHECK(program.definitions[0].fullPath == "/main");
}

TEST_CASE("rejects execution with arguments and body") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

execute_repeat(3i32) { main() }
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("executions are not supported in v0.1") != std::string::npos);
}

TEST_SUITE_END();
