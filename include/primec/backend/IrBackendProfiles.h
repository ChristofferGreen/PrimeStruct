#pragma once

#include "primec/backend/IrBackends.h"

#include <span>
#include <string>

namespace primec {

const IrBackendDiagnostics &vmIrBackendDiagnostics();
void normalizeVmLoweringError(std::string &error);
// Rewrites the `native backend` the lowerer writes into the name of the target that lowers (`wasm backend`).
void normalizeLoweringBackendName(std::string &error, const IrBackendDiagnostics &diagnostics);

// Future backend/profile feature gates should add a capability bit here and
// query this registry instead of branching directly on emit/profile strings.
std::span<const IrBackendCapabilityProfile> listIrBackendCapabilityProfiles();
bool irBackendCapabilityProfileSupports(const IrBackendCapabilityProfile &profile,
                                        IrBackendCapability capability);
IrBackendCapabilitySupport queryIrBackendCapabilitySupport(const Options &options,
                                                           IrBackendCapability capability);

} // namespace primec
