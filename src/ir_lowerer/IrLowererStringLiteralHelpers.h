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
using ResolveStringTableTargetFn =
    std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)>;

struct StringLiteralHelperContext {
  InternStringLiteralFn internString{};
  ResolveStringTableTargetFn resolveStringTableTarget{};
};

int32_t internLowererString(const std::string &text, std::vector<std::string> &stringTable);
bool parseLowererStringLiteral(const std::string &text, std::string &decoded, std::string &error);
StringLiteralHelperContext makeStringLiteralHelperContext(std::vector<std::string> &stringTable, std::string &error);
InternStringLiteralFn makeInternLowererString(std::vector<std::string> &stringTable);
ResolveStringTableTargetFn makeResolveStringTableTarget(const std::vector<std::string> &stringTable,
                                                        const InternStringLiteralFn &internString,
                                                        std::string &error);
bool resolveStringTableTarget(const Expr &expr,
                              const LocalMap &localsIn,
                              const std::vector<std::string> &stringTable,
                              const InternStringLiteralFn &internString,
                              int32_t &stringIndexOut,
                              size_t &lengthOut,
                              std::string &error);

} // namespace primec::ir_lowerer
