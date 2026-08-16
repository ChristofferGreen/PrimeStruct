#pragma once

#include "primec/ast/AstPrinter.h"
#include "primec/ir/IrPrinter.h"
#include "primec/frontend/Lexer.h"
#include "primec/frontend/Parser.h"
#include "primec/support/TextFilterPipeline.h"

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

primec::Program parseProgramWithFilters(const std::string &source) {
  primec::TextFilterPipeline pipeline;
  std::string filtered;
  std::string error;
  CHECK(pipeline.apply(source, filtered, error));
  CHECK(error.empty());
  primec::Lexer lexer(filtered);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  error.clear();
  CHECK(parser.parse(program, error));
  CHECK(error.empty());
  return program;
}
} // namespace
