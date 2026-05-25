#pragma once

#include "primec/IrBackends.h"

#include <span>
#include <string>

namespace primec {

const IrBackendDiagnostics &vmIrBackendDiagnostics();
void normalizeVmLoweringError(std::string &error);

// Future backend/profile feature gates should add a capability bit here and
// query this registry instead of branching directly on emit/profile strings.
std::span<const IrBackendCapabilityProfile> listIrBackendCapabilityProfiles();
bool irBackendCapabilityProfileSupports(const IrBackendCapabilityProfile &profile,
                                        IrBackendCapability capability);
IrBackendCapabilitySupport queryIrBackendCapabilitySupport(const Options &options,
                                                           IrBackendCapability capability);

} // namespace primec
