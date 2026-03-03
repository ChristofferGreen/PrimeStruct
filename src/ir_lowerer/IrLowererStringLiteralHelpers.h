#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "IrLowererSharedTypes.h"
#include "primec/Ast.h"

namespace primec::ir_lowerer {

using InternStringLiteralFn = std::function<int32_t(const std::string &)>;

bool parseLowererStringLiteral(const std::string &text, std::string &decoded, std::string &error);
bool resolveStringTableTarget(const Expr &expr,
                              const LocalMap &localsIn,
                              const std::vector<std::string> &stringTable,
                              const InternStringLiteralFn &internString,
                              int32_t &stringIndexOut,
                              size_t &lengthOut,
                              std::string &error);

} // namespace primec::ir_lowerer
