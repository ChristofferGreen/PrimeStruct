#include "primec/IrBackends.h"

#include "primec/ExternalTooling.h"
#include "primec/IrSerializer.h"
#include "primec/IrToCppEmitter.h"
#include "primec/IrToGlslEmitter.h"
#include "primec/NativeEmitter.h"
#include "primec/ProcessRunner.h"
#include "primec/Vm.h"
#include "primec/WasmEmitter.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

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

bool writeBinaryFile(const std::string &path, const std::vector<uint8_t> &data) {
  std::ofstream out(path, std::ios::binary);
  if (!out.is_open()) {
    return false;
  }
  out.write(reinterpret_cast<const char *>(data.data()), static_cast<std::streamsize>(data.size()));
  return static_cast<bool>(out);
}

bool writeTextFile(const std::string &path, const std::string &data) {
  std::ofstream out(path);
  if (!out.is_open()) {
    return false;
  }
  out << data;
  return static_cast<bool>(out);
}

class VmIrBackend final : public IrBackend {
public:
  std::string_view emitKind() const override {
    return "vm";
  }

  const IrBackendDiagnostics &diagnostics() const override {
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

  IrValidationTarget validationTarget(const Options & /*options*/) const override {
    return IrValidationTarget::Vm;
  }

  bool requiresOutputPath() const override {
    return false;
  }

  void normalizeLoweringError(std::string &error) const override {
    replaceAll(error, "native backend", "vm backend");
  }

  bool emit(const IrModule &module,
            const IrBackendEmitOptions &options,
            IrBackendEmitResult &result,
            std::string &error) const override {
    Vm vm;
    std::vector<std::string_view> args;
    args.reserve(1 + options.programArgs.size());
    args.push_back(options.inputPath);
    for (const auto &arg : options.programArgs) {
      args.push_back(arg);
    }
    uint64_t vmResult = 0;
    if (!vm.execute(module, vmResult, error, args)) {
      return false;
    }
    result.exitCode = static_cast<int>(static_cast<int32_t>(vmResult));
    return true;
  }
};

class NativeIrBackend final : public IrBackend {
public:
  std::string_view emitKind() const override {
    return "native";
  }

  const IrBackendDiagnostics &diagnostics() const override {
    static constexpr IrBackendDiagnostics Diagnostics = {
        .loweringDiagnosticCode = DiagnosticCode::LoweringError,
        .validationDiagnosticCode = DiagnosticCode::LoweringError,
        .inliningDiagnosticCode = DiagnosticCode::LoweringError,
        .emitDiagnosticCode = DiagnosticCode::EmitError,
        .loweringErrorPrefix = "Native lowering error: ",
        .validationErrorPrefix = "Native IR validation error: ",
        .inliningErrorPrefix = "Native IR inlining error: ",
        .emitErrorPrefix = "Native emit error: ",
        .backendTag = "native",
    };
    return Diagnostics;
  }

  IrValidationTarget validationTarget(const Options & /*options*/) const override {
    return IrValidationTarget::Native;
  }

  bool requiresOutputPath() const override {
    return true;
  }

  bool emit(const IrModule &module,
            const IrBackendEmitOptions &options,
            IrBackendEmitResult & /*result*/,
            std::string &error) const override {
    NativeEmitter nativeEmitter;
    return nativeEmitter.emitExecutable(module, options.outputPath, error);
  }
};

class SerializeIrBackend final : public IrBackend {
public:
  std::string_view emitKind() const override {
    return "ir";
  }

  const IrBackendDiagnostics &diagnostics() const override {
    static constexpr IrBackendDiagnostics Diagnostics = {
        .loweringDiagnosticCode = DiagnosticCode::LoweringError,
        .validationDiagnosticCode = DiagnosticCode::IrSerializeError,
        .inliningDiagnosticCode = DiagnosticCode::IrSerializeError,
        .emitDiagnosticCode = DiagnosticCode::IrSerializeError,
        .loweringErrorPrefix = "IR lowering error: ",
        .validationErrorPrefix = "IR validation error: ",
        .inliningErrorPrefix = "IR inlining error: ",
        .emitErrorPrefix = "IR serialize error: ",
        .backendTag = "ir",
    };
    return Diagnostics;
  }

  IrValidationTarget validationTarget(const Options & /*options*/) const override {
    return IrValidationTarget::Any;
  }

  bool requiresOutputPath() const override {
    return true;
  }

  bool emit(const IrModule &module,
            const IrBackendEmitOptions &options,
            IrBackendEmitResult & /*result*/,
            std::string &error) const override {
    std::vector<uint8_t> data;
    if (!serializeIr(module, data, error)) {
      return false;
    }
    if (!writeBinaryFile(options.outputPath, data)) {
      error = options.outputPath;
      return false;
    }
    return true;
  }
};

class WasmIrBackend final : public IrBackend {
public:
  std::string_view emitKind() const override {
    return "wasm";
  }

  const IrBackendDiagnostics &diagnostics() const override {
    static constexpr IrBackendDiagnostics Diagnostics = {
        .loweringDiagnosticCode = DiagnosticCode::LoweringError,
        .validationDiagnosticCode = DiagnosticCode::LoweringError,
        .inliningDiagnosticCode = DiagnosticCode::LoweringError,
        .emitDiagnosticCode = DiagnosticCode::EmitError,
        .loweringErrorPrefix = "Wasm lowering error: ",
        .validationErrorPrefix = "Wasm IR validation error: ",
        .inliningErrorPrefix = "Wasm IR inlining error: ",
        .emitErrorPrefix = "Wasm emit error: ",
        .backendTag = "wasm",
    };
    return Diagnostics;
  }

  IrValidationTarget validationTarget(const Options &options) const override {
    return options.wasmProfile == "browser" ? IrValidationTarget::WasmBrowser : IrValidationTarget::Wasm;
  }

  bool requiresOutputPath() const override {
    return true;
  }

  bool emit(const IrModule &module,
            const IrBackendEmitOptions &options,
            IrBackendEmitResult & /*result*/,
            std::string &error) const override {
    WasmEmitter wasmEmitter;
    std::vector<uint8_t> data;
    if (!wasmEmitter.emitModule(module, data, error)) {
      return false;
    }
    if (!writeBinaryFile(options.outputPath, data)) {
      error = options.outputPath;
      return false;
    }
    return true;
  }
};

class GlslIrBackend final : public IrBackend {
public:
  std::string_view emitKind() const override {
    return "glsl-ir";
  }

  const IrBackendDiagnostics &diagnostics() const override {
    static constexpr IrBackendDiagnostics Diagnostics = {
        .loweringDiagnosticCode = DiagnosticCode::LoweringError,
        .validationDiagnosticCode = DiagnosticCode::LoweringError,
        .inliningDiagnosticCode = DiagnosticCode::LoweringError,
        .emitDiagnosticCode = DiagnosticCode::EmitError,
        .loweringErrorPrefix = "GLSL-IR lowering error: ",
        .validationErrorPrefix = "GLSL-IR validation error: ",
        .inliningErrorPrefix = "GLSL-IR inlining error: ",
        .emitErrorPrefix = "GLSL-IR emit error: ",
        .backendTag = "glsl-ir",
    };
    return Diagnostics;
  }

  IrValidationTarget validationTarget(const Options & /*options*/) const override {
    return IrValidationTarget::Any;
  }

  bool requiresOutputPath() const override {
    return true;
  }

  bool emit(const IrModule &module,
            const IrBackendEmitOptions &options,
            IrBackendEmitResult & /*result*/,
            std::string &error) const override {
    IrToGlslEmitter emitter;
    std::string glslSource;
    if (!emitter.emitSource(module, glslSource, error)) {
      error = "ir-to-glsl failed: " + error;
      return false;
    }
    if (!writeTextFile(options.outputPath, glslSource)) {
      error = options.outputPath;
      return false;
    }
    return true;
  }
};

class CppIrBackend final : public IrBackend {
public:
  std::string_view emitKind() const override {
    return "cpp-ir";
  }

  const IrBackendDiagnostics &diagnostics() const override {
    static constexpr IrBackendDiagnostics Diagnostics = {
        .loweringDiagnosticCode = DiagnosticCode::LoweringError,
        .validationDiagnosticCode = DiagnosticCode::LoweringError,
        .inliningDiagnosticCode = DiagnosticCode::LoweringError,
        .emitDiagnosticCode = DiagnosticCode::EmitError,
        .loweringErrorPrefix = "C++ IR lowering error: ",
        .validationErrorPrefix = "C++ IR validation error: ",
        .inliningErrorPrefix = "C++ IR inlining error: ",
        .emitErrorPrefix = "C++ IR emit error: ",
        .backendTag = "cpp-ir",
    };
    return Diagnostics;
  }

  IrValidationTarget validationTarget(const Options & /*options*/) const override {
    return IrValidationTarget::Any;
  }

  bool requiresOutputPath() const override {
    return true;
  }

  bool emit(const IrModule &module,
            const IrBackendEmitOptions &options,
            IrBackendEmitResult & /*result*/,
            std::string &error) const override {
    IrToCppEmitter emitter;
    std::string cppSource;
    if (!emitter.emitSource(module, cppSource, error)) {
      error = "ir-to-cpp failed: " + error;
      return false;
    }
    if (!writeTextFile(options.outputPath, cppSource)) {
      error = options.outputPath;
      return false;
    }
    return true;
  }
};

class ExeIrBackend final : public IrBackend {
public:
  std::string_view emitKind() const override {
    return "exe-ir";
  }

  const IrBackendDiagnostics &diagnostics() const override {
    static constexpr IrBackendDiagnostics Diagnostics = {
        .loweringDiagnosticCode = DiagnosticCode::LoweringError,
        .validationDiagnosticCode = DiagnosticCode::LoweringError,
        .inliningDiagnosticCode = DiagnosticCode::LoweringError,
        .emitDiagnosticCode = DiagnosticCode::EmitError,
        .loweringErrorPrefix = "EXE IR lowering error: ",
        .validationErrorPrefix = "EXE IR validation error: ",
        .inliningErrorPrefix = "EXE IR inlining error: ",
        .emitErrorPrefix = "EXE IR emit error: ",
        .backendTag = "exe-ir",
    };
    return Diagnostics;
  }

  IrValidationTarget validationTarget(const Options & /*options*/) const override {
    return IrValidationTarget::Any;
  }

  bool requiresOutputPath() const override {
    return true;
  }

  bool emit(const IrModule &module,
            const IrBackendEmitOptions &options,
            IrBackendEmitResult & /*result*/,
            std::string &error) const override {
    IrToCppEmitter emitter;
    std::string cppSource;
    if (!emitter.emitSource(module, cppSource, error)) {
      error = "ir-to-cpp failed: " + error;
      return false;
    }

    const std::filesystem::path outputPath(options.outputPath);
    std::filesystem::path cppPath = outputPath;
    cppPath.replace_extension(".cpp");
    if (!writeTextFile(cppPath.string(), cppSource)) {
      error = cppPath.string();
      return false;
    }

    const ProcessRunner &processRunner = systemProcessRunner();
    if (!compileCppExecutable(processRunner, cppPath, outputPath)) {
      error = "Failed to compile output executable";
      return false;
    }
    return true;
  }
};

const std::array<const IrBackend *, 7> &registeredBackends() {
  static const VmIrBackend VmBackend;
  static const NativeIrBackend NativeBackend;
  static const SerializeIrBackend IrBackendImpl;
  static const WasmIrBackend WasmBackend;
  static const GlslIrBackend GlslBackend;
  static const CppIrBackend CppBackend;
  static const ExeIrBackend ExeBackend;
  static const std::array<const IrBackend *, 7> Backends = {
      &VmBackend,
      &NativeBackend,
      &IrBackendImpl,
      &WasmBackend,
      &GlslBackend,
      &CppBackend,
      &ExeBackend,
  };
  return Backends;
}

} // namespace

void IrBackend::normalizeLoweringError(std::string & /*error*/) const {}

const IrBackend *findIrBackend(std::string_view emitKind) {
  const auto &backends = registeredBackends();
  for (const IrBackend *backend : backends) {
    if (backend->emitKind() == emitKind) {
      return backend;
    }
  }
  return nullptr;
}

std::vector<std::string_view> listIrBackendKinds() {
  std::vector<std::string_view> kinds;
  const auto &backends = registeredBackends();
  kinds.reserve(backends.size());
  for (const IrBackend *backend : backends) {
    kinds.push_back(backend->emitKind());
  }
  return kinds;
}

} // namespace primec
