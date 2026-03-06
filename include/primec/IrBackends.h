#pragma once

#include "primec/Diagnostics.h"
#include "primec/Ir.h"
#include "primec/IrValidation.h"
#include "primec/Options.h"

#include <string>
#include <string_view>
#include <vector>

namespace primec {

struct IrBackendDiagnostics {
  DiagnosticCode loweringDiagnosticCode = DiagnosticCode::LoweringError;
  DiagnosticCode validationDiagnosticCode = DiagnosticCode::LoweringError;
  DiagnosticCode inliningDiagnosticCode = DiagnosticCode::LoweringError;
  DiagnosticCode emitDiagnosticCode = DiagnosticCode::EmitError;
  const char *loweringErrorPrefix = "";
  const char *validationErrorPrefix = "";
  const char *inliningErrorPrefix = "";
  const char *emitErrorPrefix = "";
  const char *backendTag = "";
};

struct IrBackendEmitOptions {
  std::string outputPath;
  std::string inputPath;
  std::vector<std::string> programArgs;
};

struct IrBackendEmitResult {
  int exitCode = 0;
};

class IrBackend {
public:
  virtual ~IrBackend() = default;
  virtual std::string_view emitKind() const = 0;
  virtual const IrBackendDiagnostics &diagnostics() const = 0;
  virtual IrValidationTarget validationTarget(const Options &options) const = 0;
  virtual bool requiresOutputPath() const = 0;
  virtual void normalizeLoweringError(std::string &error) const;
  virtual bool emit(const IrModule &module,
                    const IrBackendEmitOptions &options,
                    IrBackendEmitResult &result,
                    std::string &error) const = 0;
};

const IrBackend *findIrBackend(std::string_view emitKind);
std::vector<std::string_view> listIrBackendKinds();

} // namespace primec
