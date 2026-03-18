#pragma once

#include <string>

#include "SemanticsValidateReflectionGeneratedHelpersSerialization.h"

namespace primec::semantics {

bool emitReflectionComparisonHelper(ReflectionGeneratedHelperContext &context,
                                    const std::string &helperName,
                                    const std::string &comparisonName,
                                    const std::string &foldOperatorName,
                                    bool emptyValue);
bool emitReflectionCompareHelper(ReflectionGeneratedHelperContext &context);
bool emitReflectionHash64Helper(ReflectionGeneratedHelperContext &context);

} // namespace primec::semantics
