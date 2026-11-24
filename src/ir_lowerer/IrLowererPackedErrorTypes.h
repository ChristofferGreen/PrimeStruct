#pragma once

#include <string>

namespace primec::ir_lowerer {

// Whether `typeName` is one of the stdlib error structs (`FileError`, `ImageError`, `ContainerError`, `GfxError`), as
// written or by full path. Each is a one-`i32`-field struct that the lowering treats as a scalar for `Result<T, E>`
// packing, and that a real call must not return: see the definition for why.
bool isPackedErrorStructTypeName(const std::string &typeName);

} // namespace primec::ir_lowerer
