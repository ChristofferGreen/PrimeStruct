#pragma once

#include <string>

namespace primec::ir_lowerer {

bool parseLowererStringLiteral(const std::string &text, std::string &decoded, std::string &error);

} // namespace primec::ir_lowerer
