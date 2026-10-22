#pragma once

// Shared by the two C++ emitters: how a `[host]` import of an IR module becomes a
// direct call of an `extern "C"` function that the program's link step supplies
// (docs/spec/host-and-core-library.md). The function carries the import's name; its
// C signature follows the VM's host slot encoding: i32, i64, u64, f32, f64 and bool
// pass as their C types, u8 as uint8_t, a static `string` or a `String` as a NUL-terminated
// `const char *`, and the result is one of those, void, or text: a `const char *` the
// program copies into its string heap (the optimized emitters; docs/spec/vm-design.md,
// "Host text").

#include "primec/ir/Ir.h"

#include <string>
#include <vector>

namespace primec {

// The C type of a host value (parameters or result); nullptr for a void parameter and
// a string result.
const char *hostCType(IrHostValueKind kind, bool isResult);

// Checks every host import of `module` can be lowered: its name is a C identifier, no
// import returns a bare string, and two imports of one name agree on their signature.
bool validateHostImportsForCpp(const IrModule &module, std::string &error);

// `extern "C" { ... }` declaring each distinct import once, in import order; empty when
// the module has none.
std::string hostImportDeclarations(const std::vector<IrHostImport> &imports);

// True when some import passes or returns kind `kind` (so the emitters know which
// float helpers the call sites need).
bool hostImportsUseKind(const std::vector<IrHostImport> &imports, IrHostValueKind kind);

} // namespace primec
