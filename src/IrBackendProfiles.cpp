#include "primec/IrBackendProfiles.h"

#include <string_view>

namespace primec {

namespace {

void replaceAll(std::string &text, std::string_view from, std::string_view to) {
  if (from.empty()) {
    return;
  }
  size_t pos = 0;
  while ((pos = text.find(from, pos)) != std::string::npos) {
    text.replace(pos, from.size(), to);
    pos += to.size();
  }
}

} // namespace

const IrBackendDiagnostics &vmIrBackendDiagnostics() {
  static constexpr IrBackendDiagnostics Diagnostics = {
      .loweringDiagnosticCode = DiagnosticCode::LoweringError,
      .validationDiagnosticCode = DiagnosticCode::LoweringError,
      .inliningDiagnosticCode = DiagnosticCode::LoweringError,
      .emitDiagnosticCode = DiagnosticCode::RuntimeError,
      .loweringErrorPrefix = "VM lowering error: ",
      .validationErrorPrefix = "VM IR validation error: ",
      .inliningErrorPrefix = "VM IR inlining error: ",
      .emitErrorPrefix = "VM error: ",
      .backendTag = "vm",
  };
  return Diagnostics;
}

void normalizeVmLoweringError(std::string &error) {
  replaceAll(error, "native backend", "vm backend");
}

} // namespace primec
