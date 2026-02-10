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
  return semantics.validate(program, entry, error, {});
}

bool validateProgramWithDefaults(const std::string &source,
                                 const std::string &entry,
                                 const std::vector<std::string> &defaultEffects,
                                 std::string &error) {
  auto program = parseProgram(source);
  primec::Semantics semantics;
  return semantics.validate(program, entry, error, defaultEffects);
}
} // namespace

TEST_SUITE_BEGIN("primestruct.semantics");

#include "test_semantics_entry.h"
#include "test_semantics_capabilities.h"
#include "test_semantics_bindings.h"
#include "test_semantics_imports.h"
#include "test_semantics_calls_and_flow.h"
