#include "primec/support/CompilerSourceId.h"

namespace primec {

const std::string &compilerSourceBuildId() {
  // The literal is written by tools/compiler_source_id.cpp into the build directory.
  static const std::string id =
#include "CompilerSourceId.inc"
      ;
  return id;
}

} // namespace primec
