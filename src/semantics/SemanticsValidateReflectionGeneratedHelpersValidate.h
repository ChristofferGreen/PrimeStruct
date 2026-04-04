#pragma once

#include "SemanticsValidateReflectionGeneratedHelpersSerialization.h"

namespace primec::semantics {

bool emitReflectionValidateHelper(ReflectionGeneratedHelperContext &context);
bool emitReflectionSoaSchemaHelpers(ReflectionGeneratedHelperContext &context);
bool emitReflectionSoaSchemaStorageHelpers(ReflectionGeneratedHelperContext &context);

} // namespace primec::semantics
