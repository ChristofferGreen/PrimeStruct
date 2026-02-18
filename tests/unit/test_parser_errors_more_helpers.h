#pragma once

#include "primec/Lexer.h"
#include "primec/Parser.h"
#include "primec/Semantics.h"

#include "third_party/doctest.h"

namespace {
bool validateProgram(const std::string &source, const std::string &entry, std::string &error) {
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  if (!parser.parse(program, error)) {
    return false;
  }
  primec::Semantics semantics;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  return semantics.validate(program, entry, error, defaults, defaults);
}
} // namespace

