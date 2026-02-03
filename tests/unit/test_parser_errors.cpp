#include "primec/Lexer.h"
#include "primec/Parser.h"

#include "third_party/doctest.h"

TEST_SUITE_BEGIN("primestruct.parser.errors");

TEST_CASE("reports line and column on parse error") {
  const std::string source = R"(
[return<int>]
main( {
  return(1i32)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program.definitions, program.executions, error));
  CHECK(error.find("at 3:7") != std::string::npos);
}

TEST_SUITE_END();
