#pragma once

#include "SemanticsValidateReflectionGeneratedHelpersSerialization.h"

namespace primec::semantics {

bool emitReflectionClearHelper(ReflectionGeneratedHelperContext &context);
bool emitReflectionCopyFromHelper(ReflectionGeneratedHelperContext &context);
bool emitReflectionDefaultHelper(ReflectionGeneratedHelperContext &context);
bool emitReflectionIsDefaultHelper(ReflectionGeneratedHelperContext &context);

} // namespace primec::semantics
