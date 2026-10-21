#pragma once

// Shared by the two C++ emitters: how a `[host]` import of an IR module becomes a
// direct call of an `extern "C"` function that the program's link step supplies
// (docs/spec/host-and-core-library.md). The function carries the import's name; its
// C signature follows the VM's host slot encoding: i32, i64, u64, f32, f64 and bool
// pass as their C types, a static `string` as a NUL-terminated `const char *`, and the
// result is one of those scalars or void.

#include "primec/ir/Ir.h"

#include <string>
#include <vector>

namespace primec {

// The C type of a host value (parameters or result); nullptr for kinds that cannot be
// called directly yet (a text result).
const char *hostCType(IrHostValueKind kind, bool isResult);

// Checks every host import of `module` can be lowered: its name is a C identifier, no
// import returns text, and two imports of one name agree on their signature.
bool validateHostImportsForCpp(const IrModule &module, std::string &error);

// `extern "C" { ... }` declaring each distinct import once, in import order; empty when
// the module has none.
std::string hostImportDeclarations(const std::vector<IrHostImport> &imports);

// True when some import passes or returns kind `kind` (so the emitters know which
// float helpers the call sites need).
bool hostImportsUseKind(const std::vector<IrHostImport> &imports, IrHostValueKind kind);

} // namespace primec
