#pragma once

#include <string>

namespace primec {

// The compiler build id a library archive records: `src-<64 hex>`, the SHA-256 of the compiler's source files (the
// front end, IR, lowering, runtime and support code under `src/` and `include/primec/`, not the executables, the UI
// or the embed API), computed when the project is built (tools/compiler_source_id.cpp) and compiled into this
// library. Every executable and library built from one source tree carries the same id, so `primec`, an embed host
// and a test binary agree on which archives they can load, and the id is a constant: nothing reads an executable.
const std::string &compilerSourceBuildId();

} // namespace primec
