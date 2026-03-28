#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

#include "primec/CliDriver.h"
#include "primec/EmitKind.h"
#include "primec/IrBackends.h"
#include "primec/IrBackendProfiles.h"
#include "primec/IrPreparation.h"

#include "test_ir_pipeline_backends_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.backends");

namespace {

primec::IrModule makeReturnI32Module(int32_t value) {
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushI32, static_cast<uint64_t>(static_cast<uint32_t>(value))});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);
  return module;
}

uint64_t f32ToBits(float value) {
  uint32_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  return static_cast<uint64_t>(bits);
}

uint64_t f64ToBits(double value) {
  uint64_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  return bits;
}

} // namespace

TEST_CASE("ir backend registry reports deterministic order and lookup") {
  const std::vector<std::string_view> expectedKinds = {
      "vm", "native", "ir", "wasm", "glsl-ir", "spirv-ir", "cpp-ir", "exe-ir"};
  CHECK(primec::listIrBackendKinds() == expectedKinds);

  for (std::string_view kind : expectedKinds) {
    const primec::IrBackend *backend = primec::findIrBackend(kind);
    REQUIRE(backend != nullptr);
    CHECK(backend->emitKind() == kind);
    CHECK(std::string_view(backend->diagnostics().backendTag) == kind);
  }

  CHECK(primec::findIrBackend("cpp") == nullptr);
  CHECK(primec::findIrBackend("glsl") == nullptr);
}

TEST_CASE("emit kind aliases resolve to canonical ir backend kinds") {
  CHECK(primec::resolveIrBackendEmitKind("cpp") == "cpp-ir");
  CHECK(primec::resolveIrBackendEmitKind("exe") == "exe-ir");
  CHECK(primec::resolveIrBackendEmitKind("glsl") == "glsl-ir");
  CHECK(primec::resolveIrBackendEmitKind("spirv") == "spirv-ir");
  CHECK(primec::resolveIrBackendEmitKind("cpp-ir") == "cpp-ir");
  CHECK(primec::resolveIrBackendEmitKind("native") == "native");
  CHECK(primec::resolveIrBackendEmitKind("unknown") == "unknown");
}

TEST_CASE("all production primec emit kinds route through ir backend resolution") {
  const std::vector<std::string_view> expectedKinds = {
      "cpp", "cpp-ir", "exe", "exe-ir", "native", "ir", "vm", "glsl", "spirv", "wasm", "glsl-ir", "spirv-ir"};

  const std::span<const std::string_view> emitKinds = primec::listPrimecEmitKinds();
  CHECK(std::vector<std::string_view>(emitKinds.begin(), emitKinds.end()) == expectedKinds);
  CHECK(primec::primecEmitKindsUsage() == "cpp|cpp-ir|exe|exe-ir|native|ir|vm|glsl|spirv|wasm|glsl-ir|spirv-ir");

  for (const std::string_view emitKind : emitKinds) {
    CAPTURE(emitKind);
    CHECK(primec::isPrimecEmitKind(emitKind));
    const std::string_view resolvedKind = primec::resolveIrBackendEmitKind(emitKind);
    CHECK(primec::findIrBackend(resolvedKind) != nullptr);
  }

  CHECK_FALSE(primec::isPrimecEmitKind("metal"));
}

TEST_CASE("ir preparation helper reports lowering-stage failure for unresolved entry") {
  primec::Program program;
  primec::Options options;
  options.entryPath = "/missing";
  options.inlineIrCalls = true;

  primec::IrModule ir;
  primec::IrPreparationFailure failure;
  CHECK_FALSE(primec::prepareIrModule(program, options, primec::IrValidationTarget::Vm, ir, failure));
  CHECK(failure.stage == primec::IrPreparationFailureStage::Lowering);
  CHECK(!failure.message.empty());
}

TEST_CASE("cli driver preserves parse-stage diagnostic context") {
  primec::CompilePipelineOutput output;
  output.filteredSource = "main() { return(1i32) }";

  primec::CompilePipelineDiagnosticInfo diagnosticInfo;
  diagnosticInfo.message = "expected token";

  const primec::CliFailure failure = primec::describeCompilePipelineFailure(
      primec::CompilePipelineErrorStage::Parse, "raw parse failure", output, diagnosticInfo);

  CHECK(failure.code == primec::DiagnosticCode::ParseError);
  CHECK(failure.plainPrefix == "Parse error: ");
  CHECK(failure.message == "raw parse failure");
  CHECK(failure.exitCode == 2);
  CHECK(failure.notes == std::vector<std::string>{"stage: parse"});
  REQUIRE(failure.diagnosticInfo.has_value());
  CHECK(failure.diagnosticInfo->message == diagnosticInfo.message);
  REQUIRE(failure.sourceText.has_value());
  CHECK(*failure.sourceText == output.filteredSource);
}

TEST_CASE("cli driver maps ir preparation failures through backend diagnostics") {
  const primec::IrBackend *vmBackend = primec::findIrBackend("vm");
  REQUIRE(vmBackend != nullptr);

  primec::IrPreparationFailure loweringFailure;
  loweringFailure.stage = primec::IrPreparationFailureStage::Lowering;
  loweringFailure.message = "native backend rejected entry";

  const primec::CliFailure loweringCliFailure = primec::describeIrPreparationFailure(loweringFailure, *vmBackend, 7);
  CHECK(loweringCliFailure.code == vmBackend->diagnostics().loweringDiagnosticCode);
  CHECK(loweringCliFailure.plainPrefix == vmBackend->diagnostics().loweringErrorPrefix);
  CHECK(loweringCliFailure.exitCode == 7);
  CHECK(loweringCliFailure.notes == std::vector<std::string>{"backend: vm"});
  CHECK(loweringCliFailure.message.find("vm backend") != std::string::npos);
  CHECK(loweringCliFailure.message.find("native backend") == std::string::npos);

  primec::IrPreparationFailure validationFailure;
  validationFailure.stage = primec::IrPreparationFailureStage::Validation;
  validationFailure.message = "bad ir";

  const primec::CliFailure validationCliFailure = primec::describeIrPreparationFailure(validationFailure, *vmBackend);
  CHECK(validationCliFailure.code == vmBackend->diagnostics().validationDiagnosticCode);
  CHECK(validationCliFailure.plainPrefix == vmBackend->diagnostics().validationErrorPrefix);
  CHECK(validationCliFailure.notes == std::vector<std::string>({"backend: vm", "stage: ir-validate"}));
  CHECK(validationCliFailure.message == "bad ir");
}

TEST_CASE("shared vm backend profile exposes canonical diagnostics") {
  const primec::IrBackendDiagnostics &diagnostics = primec::vmIrBackendDiagnostics();
  CHECK(diagnostics.loweringDiagnosticCode == primec::DiagnosticCode::LoweringError);
  CHECK(std::string_view(diagnostics.emitErrorPrefix) == "VM error: ");
  CHECK(std::string_view(diagnostics.backendTag) == "vm");

  primec::IrPreparationFailure loweringFailure;
  loweringFailure.stage = primec::IrPreparationFailureStage::Lowering;
  loweringFailure.message = "native backend rejected entry";
  const primec::CliFailure cliFailure =
      primec::describeIrPreparationFailure(loweringFailure, diagnostics, &primec::normalizeVmLoweringError, 5);
  CHECK(cliFailure.code == primec::DiagnosticCode::LoweringError);
  CHECK(cliFailure.exitCode == 5);
  CHECK(cliFailure.message.find("vm backend") != std::string::npos);

  std::string loweringError = "native backend rejected entry";
  primec::normalizeVmLoweringError(loweringError);
  CHECK(loweringError.find("vm backend") != std::string::npos);
  CHECK(loweringError.find("native backend") == std::string::npos);
}

TEST_CASE("main routes cpp and exe through ir backend alias lookup") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path mainPath = cwd / "src" / "main.cpp";
  if (!std::filesystem::exists(mainPath)) {
    mainPath = cwd.parent_path() / "src" / "main.cpp";
  }
  REQUIRE(std::filesystem::exists(mainPath));

  const std::string source = readTextFile(mainPath);
  CHECK(source.find("resolveIrBackendEmitKind(options.emitKind)") != std::string::npos);
  CHECK(source.find("findIrBackend(irBackendKind)") != std::string::npos);
  CHECK(source.find("describeCompilePipelineFailure(") != std::string::npos);
  CHECK(source.find("describeIrPreparationFailure(") != std::string::npos);
  CHECK(source.find("prepareIrModule(program, options, validationTarget, ir, prepFailure)") != std::string::npos);
  CHECK(source.find("IrLowerer lowerer") == std::string::npos);
  CHECK(source.find("inlineIrModuleCalls(ir, error)") == std::string::npos);
  CHECK(source.find("validateIrModule(ir, validationTarget, error)") == std::string::npos);
  CHECK(source.find("if (options.emitKind == \"cpp\" || options.emitKind == \"exe\")") == std::string::npos);
  CHECK(source.find("isProductionCppOrExeEmitKind(") == std::string::npos);
  CHECK(source.find("CompilePipelineErrorStage::Import") == std::string::npos);
  CHECK(source.find("IrPreparationFailureStage::Validation") == std::string::npos);
  CHECK(source.find("scanSoftwareNumericTypes(") == std::string::npos);
  CHECK(source.find("software numeric types are not supported:") == std::string::npos);
  CHECK(source.find("emitter.emitCpp(program, options.entryPath)") == std::string::npos);
  CHECK(source.find("compileCppExecutable(") == std::string::npos);
  CHECK(source.find("#include \"primec/Emitter.h\"") == std::string::npos);
}

TEST_CASE("primevm uses shared ir preparation helper") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path mainPath = cwd / "src" / "primevm_main.cpp";
  if (!std::filesystem::exists(mainPath)) {
    mainPath = cwd.parent_path() / "src" / "primevm_main.cpp";
  }
  REQUIRE(std::filesystem::exists(mainPath));

  const std::string source = readTextFile(mainPath);
  CHECK(source.find("vmIrBackendDiagnostics()") != std::string::npos);
  CHECK(source.find("normalizeVmLoweringError") != std::string::npos);
  CHECK(source.find("describeCompilePipelineFailure(") != std::string::npos);
  CHECK(source.find("describeIrPreparationFailure(") != std::string::npos);
  CHECK(source.find("prepareIrModule(program, options, primec::IrValidationTarget::Vm, ir, irFailure)") !=
        std::string::npos);
  CHECK(source.find("findIrBackend(\"vm\")") == std::string::npos);
  CHECK(source.find("CompilePipelineErrorStage::Import") == std::string::npos);
  CHECK(source.find("IrPreparationFailureStage::Validation") == std::string::npos);
  CHECK(source.find("IrLowerer lowerer") == std::string::npos);
  CHECK(source.find("inlineIrModuleCalls(ir, error)") == std::string::npos);
  CHECK(source.find("validateIrModule(ir, primec::IrValidationTarget::Vm, error)") == std::string::npos);
}
