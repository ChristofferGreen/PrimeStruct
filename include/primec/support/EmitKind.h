#pragma once

#include <span>
#include <string_view>

namespace primec {

std::span<const std::string_view> listPrimecEmitKinds();
bool isPrimecEmitKind(std::string_view emitKind);
std::string_view primecEmitKindsUsage();
// `interface` (the interface text) and `symbols` (its symbol table): they print a module's interface from the validated
// program and have no IR backend, so they are not in listPrimecEmitKinds.
bool isPrimecInterfaceEmitKind(std::string_view emitKind);
std::string_view resolveIrBackendEmitKind(std::string_view emitKind);

} // namespace primec
