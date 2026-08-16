#pragma once

#include <span>
#include <string_view>

namespace primec {

std::span<const std::string_view> listPrimecEmitKinds();
bool isPrimecEmitKind(std::string_view emitKind);
std::string_view primecEmitKindsUsage();
std::string_view resolveIrBackendEmitKind(std::string_view emitKind);

} // namespace primec
