#include "EmitterEmitSetupMathImport.h"

namespace primec::emitter {

namespace {

bool isMathImportPath(const std::string &path) {
  if (path == "/std/math/*") {
    return true;
  }
  return path.rfind("/std/math/", 0) == 0 && path.size() > 10;
}

} // namespace

bool runEmitterEmitSetupMathImport(const Program &program) {
  for (const auto &importPath : program.imports) {
    if (isMathImportPath(importPath)) {
      return true;
    }
  }
  return false;
}

} // namespace primec::emitter
