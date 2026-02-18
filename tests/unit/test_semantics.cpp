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

bool parseProgramWithError(const std::string &source, std::string &error) {
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  return parser.parse(program, error);
}

bool validateProgram(const std::string &source, const std::string &entry, std::string &error) {
  auto program = parseProgram(source);
  primec::Semantics semantics;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  return semantics.validate(program, entry, error, defaults, defaults);
}

bool validateProgramWithDefaults(const std::string &source,
                                 const std::string &entry,
                                 const std::vector<std::string> &defaultEffects,
                                 const std::vector<std::string> &entryDefaultEffects,
                                 std::string &error) {
  auto program = parseProgram(source);
  primec::Semantics semantics;
  return semantics.validate(program, entry, error, defaultEffects, entryDefaultEffects);
}

bool validateProgramWithDefaults(const std::string &source,
                                 const std::string &entry,
                                 const std::vector<std::string> &defaultEffects,
                                 std::string &error) {
  return validateProgramWithDefaults(source, entry, defaultEffects, defaultEffects, error);
}
} // namespace

#include "test_semantics_entry_entry.h"
#include "test_semantics_entry_resolution.h"
#include "test_semantics_entry_parameters.h"
#include "test_semantics_entry_return_inference.h"
#include "test_semantics_entry_operators.h"
#include "test_semantics_entry_math_imports.h"
#include "test_semantics_entry_tags.h"
#include "test_semantics_entry_builtins_numeric.h"
#include "test_semantics_entry_executions.h"
#include "test_semantics_entry_transforms.h"
#include "test_semantics_capabilities.h"
#include "test_semantics_bindings.h"
#include "test_semantics_imports.h"
#include "test_semantics_calls_and_flow_control_core.h"
#include "test_semantics_calls_and_flow_control_misc.h"
#include "test_semantics_calls_and_flow_collections.h"
#include "test_semantics_calls_and_flow_access.h"
#include "test_semantics_calls_and_flow_named_args.h"
#include "test_semantics_calls_and_flow_numeric_builtins.h"
#include "test_semantics_calls_and_flow_comparisons_literals.h"
#include "test_semantics_calls_and_flow_effects.h"
#include "test_semantics_lambdas.h"
