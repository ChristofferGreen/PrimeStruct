#include "primec/IrBackends.h"

#include "primec/ExternalTooling.h"
#include "primec/IrBackendProfiles.h"
#include "primec/IrSerializer.h"
#include "primec/IrToCppEmitter.h"
#include "primec/IrToGlslEmitter.h"
#include "primec/NativeEmitter.h"
#include "primec/ProcessRunner.h"
#include "primec/TempPaths.h"
#include "primec/Vm.h"
#include "primec/WasmEmitter.h"

#include <array>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

namespace primec {
namespace {

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

std::string injectComputeLayout(const std::string &glslSource) {
  const std::string layoutLine = "layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n";
  size_t versionPos = glslSource.find("#version");
  if (versionPos == std::string::npos) {
    return "#version 450\n" + layoutLine + glslSource;
  }
  size_t lineEnd = glslSource.find('\n', versionPos);
  if (lineEnd == std::string::npos) {
    return glslSource + "\n" + layoutLine;
  }
  return glslSource.substr(0, lineEnd + 1) + layoutLine + glslSource.substr(lineEnd + 1);
}

size_t totalInstructionCount(const IrModule &module) {
  size_t count = 0;
  for (const IrFunction &function : module.functions) {
    count += function.instructions.size();
  }
  return count;
}

IrModule pruneIrModuleToReachableFunctions(const IrModule &module) {
  const std::size_t functionCount = module.functions.size();
  if (functionCount == 0 || module.entryIndex < 0 ||
      static_cast<std::size_t>(module.entryIndex) >= functionCount) {
    return module;
  }

  std::vector<bool> reachable(functionCount, false);
  std::deque<std::size_t> pending;
  const std::size_t entryIndex = static_cast<std::size_t>(module.entryIndex);
  reachable[entryIndex] = true;
  pending.push_back(entryIndex);

  while (!pending.empty()) {
    const std::size_t functionIndex = pending.front();
    pending.pop_front();
    const auto &function = module.functions[functionIndex];
    for (const auto &instruction : function.instructions) {
      if (instruction.op != IrOpcode::Call && instruction.op != IrOpcode::CallVoid) {
        continue;
      }
      const std::size_t targetIndex = static_cast<std::size_t>(instruction.imm);
      if (targetIndex >= functionCount || reachable[targetIndex]) {
        continue;
      }
      reachable[targetIndex] = true;
      pending.push_back(targetIndex);
    }
  }

  std::vector<int32_t> oldToNew(functionCount, -1);
  IrModule pruned;
  pruned.stringTable = module.stringTable;
  pruned.structLayouts = module.structLayouts;
  pruned.instructionSourceMap = module.instructionSourceMap;
  pruned.functions.reserve(functionCount);
  for (std::size_t oldIndex = 0; oldIndex < functionCount; ++oldIndex) {
    if (!reachable[oldIndex]) {
      continue;
    }
    oldToNew[oldIndex] = static_cast<int32_t>(pruned.functions.size());
    pruned.functions.push_back(module.functions[oldIndex]);
  }

  pruned.entryIndex = oldToNew[entryIndex];
  for (auto &function : pruned.functions) {
    for (auto &instruction : function.instructions) {
      if (instruction.op != IrOpcode::Call && instruction.op != IrOpcode::CallVoid) {
        continue;
      }
      const std::size_t oldTarget = static_cast<std::size_t>(instruction.imm);
      if (oldTarget >= oldToNew.size() || oldToNew[oldTarget] < 0) {
        continue;
      }
      instruction.imm = static_cast<uint64_t>(oldToNew[oldTarget]);
    }
  }

  return pruned;
}

class VmIrBackend final : public IrBackend {
public:
  std::string_view emitKind() const override {
    return "vm";
  }

  const IrBackendDiagnostics &diagnostics() const override {
    return vmIrBackendDiagnostics();
  }

  IrValidationTarget validationTarget(const Options & /*options*/) const override {
    return IrValidationTarget::Vm;
  }

  bool requiresOutputPath() const override {
    return false;
  }

  void normalizeLoweringError(std::string &error) const override {
    normalizeVmLoweringError(error);
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
    return IrValidationTarget::Glsl;
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

class SpirvIrBackend final : public IrBackend {
public:
  std::string_view emitKind() const override {
    return "spirv-ir";
  }

  const IrBackendDiagnostics &diagnostics() const override {
    static constexpr IrBackendDiagnostics Diagnostics = {
        .loweringDiagnosticCode = DiagnosticCode::LoweringError,
        .validationDiagnosticCode = DiagnosticCode::LoweringError,
        .inliningDiagnosticCode = DiagnosticCode::LoweringError,
        .emitDiagnosticCode = DiagnosticCode::EmitError,
        .loweringErrorPrefix = "SPIR-V IR lowering error: ",
        .validationErrorPrefix = "SPIR-V IR validation error: ",
        .inliningErrorPrefix = "SPIR-V IR inlining error: ",
        .emitErrorPrefix = "SPIR-V IR emit error: ",
        .backendTag = "spirv-ir",
    };
    return Diagnostics;
  }

  IrValidationTarget validationTarget(const Options & /*options*/) const override {
    return IrValidationTarget::Glsl;
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

    const std::string spirvSource = injectComputeLayout(glslSource);
    const ProcessRunner &processRunner = systemProcessRunner();
    std::string toolName;
    if (!findSpirvCompiler(processRunner, toolName)) {
      error = "glslangValidator or glslc not found";
      return false;
    }

    const std::filesystem::path tempPath = primecUniqueTempFile("spirv_ir", ".comp");
    if (!writeTextFile(tempPath.string(), spirvSource)) {
      error = tempPath.string();
      return false;
    }

    const bool ok = compileSpirv(processRunner, toolName, tempPath, options.outputPath);
    std::error_code ec;
    std::filesystem::remove(tempPath, ec);
    if (!ok) {
      error = "tool invocation failed";
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
    const IrModule prunedModule = pruneIrModuleToReachableFunctions(module);
    IrToCppEmitter emitter;
    std::string cppSource;
    if (!emitter.emitSource(prunedModule, cppSource, error)) {
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
    const IrModule prunedModule = pruneIrModuleToReachableFunctions(module);
    const std::filesystem::path outputPath(options.outputPath);
    std::filesystem::path cppPath = outputPath;
    cppPath.replace_extension(".cpp");

    auto emitCppSidecar = [&]() -> bool {
      IrToCppEmitter emitter;
      std::string cppSource;
      if (!emitter.emitSource(prunedModule, cppSource, error)) {
        error = "ir-to-cpp failed: " + error;
        return false;
      }
      if (!writeTextFile(cppPath.string(), cppSource)) {
        error = cppPath.string();
        return false;
      }
      return true;
    };

    // Prefer native executable emission for `--emit=exe` to avoid host-compiler
    // memory spikes on large IR modules. Keep writing the `.cpp` sidecar for
    // normal-sized programs and diagnostics, but skip sidecar generation for
    // very large modules where the sidecar itself causes significant RSS growth.
    constexpr size_t ExeCppSidecarInstructionLimit = 50000u;
    NativeEmitter nativeEmitter;
    std::string nativeError;
    if (nativeEmitter.emitExecutable(prunedModule, options.outputPath, nativeError)) {
      if (totalInstructionCount(prunedModule) <= ExeCppSidecarInstructionLimit) {
        if (!emitCppSidecar()) {
          return false;
        }
      }
      return true;
    }

    if (!emitCppSidecar()) {
      return false;
    }

    const ProcessRunner &processRunner = systemProcessRunner();
    if (!compileCppExecutable(processRunner, cppPath, outputPath)) {
      error = "Failed to compile output executable";
      if (!nativeError.empty()) {
        error += " (native backend fallback failed: " + nativeError + ")";
      }
      return false;
    }
    return true;
  }
};

const std::array<const IrBackend *, 8> &registeredBackends() {
  static const VmIrBackend VmBackend;
  static const NativeIrBackend NativeBackend;
  static const SerializeIrBackend IrBackendImpl;
  static const WasmIrBackend WasmBackend;
  static const GlslIrBackend GlslBackend;
  static const SpirvIrBackend SpirvBackend;
  static const CppIrBackend CppBackend;
  static const ExeIrBackend ExeBackend;
  static const std::array<const IrBackend *, 8> Backends = {
      &VmBackend,
      &NativeBackend,
      &IrBackendImpl,
      &WasmBackend,
      &GlslBackend,
      &SpirvBackend,
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
