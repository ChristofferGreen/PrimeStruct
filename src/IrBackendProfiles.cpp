#include "primec/IrBackendProfiles.h"

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

namespace primec {

namespace {

constexpr uint32_t GraphicsRuntimeSubstrateCapabilityMask =
    static_cast<uint32_t>(IrBackendCapability::GraphicsRuntimeSubstrate);
constexpr uint32_t RuntimeReflectionCapabilityMask =
    static_cast<uint32_t>(IrBackendCapability::RuntimeReflection);
constexpr uint32_t HostRuntimeCapabilityMask =
    GraphicsRuntimeSubstrateCapabilityMask | RuntimeReflectionCapabilityMask;

constexpr std::array<IrBackendCapabilityProfile, 13> CapabilityProfiles = {{
    {.emitKind = "vm",
     .wasmProfile = "",
     .targetName = "vm",
     .capabilities = HostRuntimeCapabilityMask},
    {.emitKind = "native",
     .wasmProfile = "",
     .targetName = "native",
     .capabilities = HostRuntimeCapabilityMask},
    {.emitKind = "ir",
     .wasmProfile = "",
     .targetName = "ir",
     .capabilities = HostRuntimeCapabilityMask},
    {.emitKind = "wasm", .wasmProfile = "wasi", .targetName = "wasm-wasi", .capabilities = 0},
    {.emitKind = "wasm", .wasmProfile = "browser", .targetName = "wasm-browser", .capabilities = 0},
    {.emitKind = "glsl", .wasmProfile = "", .targetName = "glsl", .capabilities = 0},
    {.emitKind = "glsl-ir", .wasmProfile = "", .targetName = "glsl", .capabilities = 0},
    {.emitKind = "spirv", .wasmProfile = "", .targetName = "spirv", .capabilities = 0},
    {.emitKind = "spirv-ir", .wasmProfile = "", .targetName = "spirv", .capabilities = 0},
    {.emitKind = "cpp",
     .wasmProfile = "",
     .targetName = "cpp",
     .capabilities = HostRuntimeCapabilityMask},
    {.emitKind = "cpp-ir",
     .wasmProfile = "",
     .targetName = "cpp-ir",
     .capabilities = HostRuntimeCapabilityMask},
    {.emitKind = "exe",
     .wasmProfile = "",
     .targetName = "exe",
     .capabilities = HostRuntimeCapabilityMask},
    {.emitKind = "exe-ir",
     .wasmProfile = "",
     .targetName = "exe-ir",
     .capabilities = HostRuntimeCapabilityMask},
}};

std::string_view effectiveWasmProfile(std::string_view wasmProfile) {
  return wasmProfile == "browser" ? "browser" : "wasi";
}

const IrBackendCapabilityProfile *findCapabilityProfileForOptions(const Options &options) {
  const std::string_view emitKind = options.emitKind;
  const std::string_view wasmProfile =
      emitKind == "wasm" ? effectiveWasmProfile(options.wasmProfile) : std::string_view();

  for (const IrBackendCapabilityProfile &profile : CapabilityProfiles) {
    if (std::string_view(profile.emitKind) != emitKind) {
      continue;
    }
    if (std::string_view(profile.wasmProfile) != wasmProfile) {
      continue;
    }
    return &profile;
  }

  return nullptr;
}

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

std::span<const IrBackendCapabilityProfile> listIrBackendCapabilityProfiles() {
  return std::span<const IrBackendCapabilityProfile>(CapabilityProfiles.data(),
                                                     CapabilityProfiles.size());
}

bool irBackendCapabilityProfileSupports(const IrBackendCapabilityProfile &profile,
                                        IrBackendCapability capability) {
  return (profile.capabilities & static_cast<uint32_t>(capability)) != 0;
}

IrBackendCapabilitySupport queryIrBackendCapabilitySupport(const Options &options,
                                                           IrBackendCapability capability) {
  const IrBackendCapabilityProfile *profile = findCapabilityProfileForOptions(options);
  if (profile == nullptr) {
    return {.supported = true, .targetName = ""};
  }
  return {
      .supported = irBackendCapabilityProfileSupports(*profile, capability),
      .targetName = profile->targetName,
  };
}

} // namespace primec
