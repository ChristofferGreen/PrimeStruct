#pragma once

#include "primec/IrBackends.h"

#include <string>

namespace primec {

const IrBackendDiagnostics &vmIrBackendDiagnostics();
void normalizeVmLoweringError(std::string &error);

} // namespace primec
