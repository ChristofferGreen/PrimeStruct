#pragma once

#include <string_view>

namespace primec {

bool isTextTransformName(std::string_view name);
bool isSemanticTransformName(std::string_view name);

} // namespace primec
