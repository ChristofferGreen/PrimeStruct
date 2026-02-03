#include "primec/Lexer.h"
#include "primec/Parser.h"
#include "primec/Semantics.h"

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

bool validateProgram(const std::string &source, const std::string &entry, std::string &error) {
  auto program = parseProgram(source);
  primec::Semantics semantics;
  return semantics.validate(program, entry, error);
}
} // namespace

TEST_SUITE_BEGIN("primestruct.semantics");

TEST_CASE("missing entry definition fails") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/missing", error));
  CHECK(error.find("missing entry definition") != std::string::npos);
}

TEST_CASE("unknown identifier fails") {
  const std::string source = R"(
[return<int>]
main(x) {
  return(y)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown identifier") != std::string::npos);
}

TEST_CASE("argument count mismatch fails") {
  const std::string source = R"(
[return<int>]
callee(x) {
  return(x)
}

[return<int>]
main() {
  return(callee(1i32, 2i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch") != std::string::npos);
}

TEST_CASE("unsupported return type fails") {
  const std::string source = R"(
[return<float>]
main() {
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unsupported return type") != std::string::npos);
}

TEST_SUITE_END();
