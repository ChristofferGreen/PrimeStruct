#include <algorithm>
#include <cstdint>
#include <cstring>

#include "third_party/doctest.h"

#include "primec/IrLowerer.h"
#include "primec/IrSerializer.h"
#include "primec/Lexer.h"
#include "primec/Parser.h"
#include "primec/Semantics.h"
#include "primec/Vm.h"

namespace {
bool parseAndValidate(const std::string &source,
                      primec::Program &program,
                      std::string &error,
                      const std::vector<std::string> &defaultEffects,
                      const std::vector<std::string> &entryDefaultEffects) {
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  if (!parser.parse(program, error)) {
    return false;
  }
  primec::Semantics semantics;
  return semantics.validate(program, "/main", error, defaultEffects, entryDefaultEffects);
}

bool parseAndValidate(const std::string &source,
                      primec::Program &program,
                      std::string &error,
                      std::vector<std::string> defaultEffects = {}) {
  return parseAndValidate(source, program, error, defaultEffects, defaultEffects);
}
} // namespace

#include "test_ir_pipeline_serialization.h"
#include "test_ir_pipeline_pointers.h"
#include "test_ir_pipeline_conversions.h"
#include "test_ir_pipeline_entry_args.h"
