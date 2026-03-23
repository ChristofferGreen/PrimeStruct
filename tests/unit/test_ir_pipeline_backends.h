#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#include "primec/CliDriver.h"
#include "primec/EmitKind.h"
#include "primec/IrBackends.h"
#include "primec/IrBackendProfiles.h"
#include "primec/IrPreparation.h"

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

std::vector<uint8_t> readBinaryFile(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  return std::vector<uint8_t>(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

std::string readTextFile(const std::filesystem::path &path) {
  std::ifstream input(path);
  return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

std::string readTextFiles(const std::vector<std::filesystem::path> &paths) {
  std::string combined;
  for (const auto &path : paths) {
    combined += readTextFile(path);
    combined.push_back('\n');
  }
  return combined;
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

TEST_CASE("semantics validator caches base validation contexts behind a single current context") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path headerPath = cwd / "src" / "semantics" / "SemanticsValidator.h";
  std::filesystem::path sourcePath = cwd / "src" / "semantics" / "SemanticsValidator.cpp";
  std::filesystem::path buildPath = cwd / "src" / "semantics" / "SemanticsValidatorBuild.cpp";
  if (!std::filesystem::exists(headerPath)) {
    headerPath = cwd.parent_path() / "src" / "semantics" / "SemanticsValidator.h";
    sourcePath = cwd.parent_path() / "src" / "semantics" / "SemanticsValidator.cpp";
    buildPath = cwd.parent_path() / "src" / "semantics" / "SemanticsValidatorBuild.cpp";
  }
  REQUIRE(std::filesystem::exists(headerPath));
  REQUIRE(std::filesystem::exists(sourcePath));
  REQUIRE(std::filesystem::exists(buildPath));

  const std::string header = readTextFile(headerPath);
  const std::string source = readTextFile(sourcePath);
  const std::string build = readTextFile(buildPath);
  CHECK(header.find("std::unordered_map<std::string, ValidationContext> definitionValidationContexts_") !=
        std::string::npos);
  CHECK(header.find("std::unordered_map<std::string, ValidationContext> executionValidationContexts_") !=
        std::string::npos);
  CHECK(header.find("ValidationContext currentValidationContext_") != std::string::npos);
  CHECK(header.find("std::unordered_set<std::string> activeEffects_;") == std::string::npos);
  CHECK(header.find("std::unordered_set<std::string> movedBindings_;") == std::string::npos);
  CHECK(header.find("std::unordered_set<std::string> endedReferenceBorrows_;") == std::string::npos);
  CHECK(header.find("std::string currentDefinitionPath_;") == std::string::npos);
  CHECK(build.find("definitionValidationContexts_.try_emplace(def.fullPath, makeDefinitionValidationContext(def));") !=
        std::string::npos);
  CHECK(build.find("executionValidationContexts_.try_emplace(exec.fullPath, makeExecutionValidationContext(exec));") !=
        std::string::npos);
  CHECK(source.find("snapshotValidationContext()") == std::string::npos);
  CHECK(source.find("restoreValidationContext(") == std::string::npos);
}

TEST_CASE("type resolution graph builder is wired through semantics testing api") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path cmakePath = cwd / "CMakeLists.txt";
  std::filesystem::path testApiPath = cwd / "include" / "primec" / "testing" / "SemanticsValidationHelpers.h";
  std::filesystem::path graphHeaderPath = cwd / "src" / "semantics" / "TypeResolutionGraph.h";
  std::filesystem::path graphSourcePath = cwd / "src" / "semantics" / "TypeResolutionGraph.cpp";
  std::filesystem::path pipelinePath = cwd / "src" / "CompilePipeline.cpp";
  std::filesystem::path primecMainPath = cwd / "src" / "main.cpp";
  std::filesystem::path primevmMainPath = cwd / "src" / "primevm_main.cpp";
  if (!std::filesystem::exists(cmakePath)) {
    cmakePath = cwd.parent_path() / "CMakeLists.txt";
    testApiPath = cwd.parent_path() / "include" / "primec" / "testing" / "SemanticsValidationHelpers.h";
    graphHeaderPath = cwd.parent_path() / "src" / "semantics" / "TypeResolutionGraph.h";
    graphSourcePath = cwd.parent_path() / "src" / "semantics" / "TypeResolutionGraph.cpp";
    pipelinePath = cwd.parent_path() / "src" / "CompilePipeline.cpp";
    primecMainPath = cwd.parent_path() / "src" / "main.cpp";
    primevmMainPath = cwd.parent_path() / "src" / "primevm_main.cpp";
  }
  REQUIRE(std::filesystem::exists(cmakePath));
  REQUIRE(std::filesystem::exists(testApiPath));
  REQUIRE(std::filesystem::exists(graphHeaderPath));
  REQUIRE(std::filesystem::exists(graphSourcePath));
  REQUIRE(std::filesystem::exists(pipelinePath));
  REQUIRE(std::filesystem::exists(primecMainPath));
  REQUIRE(std::filesystem::exists(primevmMainPath));

  const std::string cmake = readTextFile(cmakePath);
  const std::string testApi = readTextFile(testApiPath);
  const std::string graphHeader = readTextFile(graphHeaderPath);
  const std::string graphSource = readTextFile(graphSourcePath);
  const std::string pipeline = readTextFile(pipelinePath);
  const std::string primecMain = readTextFile(primecMainPath);
  const std::string primevmMain = readTextFile(primevmMainPath);
  CHECK(cmake.find("src/semantics/TypeResolutionGraph.cpp") != std::string::npos);
  CHECK(cmake.find("primestruct.semantics.type_resolution_graph") != std::string::npos);
  CHECK(testApi.find("struct TypeResolutionGraphSnapshotNode") != std::string::npos);
  CHECK(testApi.find("struct TypeResolutionGraphSnapshotEdge") != std::string::npos);
  CHECK(testApi.find("struct TypeResolutionGraphSnapshot") != std::string::npos);
  CHECK(testApi.find("buildTypeResolutionGraphForTesting") != std::string::npos);
  CHECK(testApi.find("dumpTypeResolutionGraphForTesting") != std::string::npos);
  CHECK(graphHeader.find("enum class TypeResolutionNodeKind") != std::string::npos);
  CHECK(graphHeader.find("enum class TypeResolutionEdgeKind") != std::string::npos);
  CHECK(graphHeader.find("struct TypeResolutionGraphNode") != std::string::npos);
  CHECK(graphHeader.find("struct TypeResolutionGraphEdge") != std::string::npos);
  CHECK(graphHeader.find("struct TypeResolutionGraph") != std::string::npos);
  CHECK(graphHeader.find("buildTypeResolutionGraphForProgram") != std::string::npos);
  CHECK(graphHeader.find("formatTypeResolutionGraph") != std::string::npos);
  CHECK(graphSource.find("TypeResolutionGraphBuilder") != std::string::npos);
  CHECK(graphSource.find("monomorphizeTemplates(program, entryPath, error)") != std::string::npos);
  CHECK(graphSource.find("rewriteConvertConstructors(program, error)") != std::string::npos);
  CHECK(graphSource.find("buildTypeResolutionGraph(program)") != std::string::npos);
  CHECK(graphSource.find("buildTypeResolutionGraphForProgram(std::move(program), entryPath") != std::string::npos);
  CHECK(graphSource.find("out = formatTypeResolutionGraph(graph);") != std::string::npos);
  CHECK(pipeline.find("DumpStage::TypeGraph") != std::string::npos);
  CHECK(pipeline.find("dumpStage == \"type_graph\" || dumpStage == \"type-graph\"") != std::string::npos);
  CHECK(pipeline.find("semantics::buildTypeResolutionGraphForProgram(") != std::string::npos);
  CHECK(pipeline.find("output.dumpOutput = semantics::formatTypeResolutionGraph(graph);") != std::string::npos);
  CHECK(primecMain.find("[--dump-stage pre_ast|ast|ast-semantic|type-graph|ir]") != std::string::npos);
  CHECK(primevmMain.find("[--dump-stage pre_ast|ast|ast-semantic|type-graph|ir]") != std::string::npos);
}

TEST_CASE("graph type resolver pilot is wired through options and semantics inference") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path optionsHeaderPath = cwd / "include" / "primec" / "Options.h";
  std::filesystem::path optionsParserPath = cwd / "src" / "OptionsParser.cpp";
  std::filesystem::path semanticsHeaderPath = cwd / "include" / "primec" / "Semantics.h";
  std::filesystem::path semanticsValidatePath = cwd / "src" / "semantics" / "SemanticsValidate.cpp";
  std::filesystem::path validatorHeaderPath = cwd / "src" / "semantics" / "SemanticsValidator.h";
  std::filesystem::path validatorCorePath = cwd / "src" / "semantics" / "SemanticsValidator.cpp";
  std::filesystem::path validatorBuildPath = cwd / "src" / "semantics" / "SemanticsValidatorBuild.cpp";
  std::filesystem::path validatorBuildUtilityPath =
      cwd / "src" / "semantics" / "SemanticsValidatorBuildUtility.cpp";
  std::filesystem::path validatorBuildStructFieldsPath =
      cwd / "src" / "semantics" / "SemanticsValidatorBuildStructFields.cpp";
  std::filesystem::path validatorBuildDirectCallBindingPath =
      cwd / "src" / "semantics" / "SemanticsValidatorBuildDirectCallBinding.cpp";
  std::filesystem::path validatorCollectionsPath = cwd / "src" / "semantics" / "SemanticsValidatorInferCollections.cpp";
  std::filesystem::path validatorExprPath = cwd / "src" / "semantics" / "SemanticsValidatorExpr.cpp";
  std::filesystem::path validatorExprCollectionDispatchSetupPath =
      cwd / "src" / "semantics" / "SemanticsValidatorExprCollectionDispatchSetup.cpp";
  std::filesystem::path validatorExprCollectionAccessSetupPath =
      cwd / "src" / "semantics" / "SemanticsValidatorExprCollectionAccessSetup.cpp";
  std::filesystem::path validatorExprDirectCollectionFallbacksPath =
      cwd / "src" / "semantics" / "SemanticsValidatorExprDirectCollectionFallbacks.cpp";
  std::filesystem::path validatorExprMethodResolutionPath =
      cwd / "src" / "semantics" / "SemanticsValidatorExprMethodResolution.cpp";
  std::filesystem::path validatorExprBodyArgumentsPath =
      cwd / "src" / "semantics" / "SemanticsValidatorExprBodyArguments.cpp";
  std::filesystem::path validatorCollectionHelperRewritesPath =
      cwd / "src" / "semantics" / "SemanticsValidatorCollectionHelperRewrites.cpp";
  std::filesystem::path validatorInferPath = cwd / "src" / "semantics" / "SemanticsValidatorInfer.cpp";
  std::filesystem::path validatorInferDefinitionPath =
      cwd / "src" / "semantics" / "SemanticsValidatorInferDefinition.cpp";
  std::filesystem::path validatorInferGraphPath =
      cwd / "src" / "semantics" / "SemanticsValidatorInferGraph.cpp";
  std::filesystem::path validatorInferMethodResolutionPath =
      cwd / "src" / "semantics" / "SemanticsValidatorInferMethodResolution.cpp";
  std::filesystem::path validatorInferStructReturnPath =
      cwd / "src" / "semantics" / "SemanticsValidatorInferStructReturn.cpp";
  std::filesystem::path validatorInferTargetResolutionPath =
      cwd / "src" / "semantics" / "SemanticsValidatorInferTargetResolution.cpp";
  std::filesystem::path validatorInferUtilityPath =
      cwd / "src" / "semantics" / "SemanticsValidatorInferUtility.cpp";
  std::filesystem::path pipelinePath = cwd / "src" / "CompilePipeline.cpp";
  std::filesystem::path primecMainPath = cwd / "src" / "main.cpp";
  std::filesystem::path primevmMainPath = cwd / "src" / "primevm_main.cpp";
  if (!std::filesystem::exists(optionsHeaderPath)) {
    optionsHeaderPath = cwd.parent_path() / "include" / "primec" / "Options.h";
    optionsParserPath = cwd.parent_path() / "src" / "OptionsParser.cpp";
    semanticsHeaderPath = cwd.parent_path() / "include" / "primec" / "Semantics.h";
    semanticsValidatePath = cwd.parent_path() / "src" / "semantics" / "SemanticsValidate.cpp";
    validatorHeaderPath = cwd.parent_path() / "src" / "semantics" / "SemanticsValidator.h";
    validatorCorePath = cwd.parent_path() / "src" / "semantics" / "SemanticsValidator.cpp";
    validatorBuildPath = cwd.parent_path() / "src" / "semantics" / "SemanticsValidatorBuild.cpp";
    validatorBuildUtilityPath = cwd.parent_path() / "src" / "semantics" / "SemanticsValidatorBuildUtility.cpp";
    validatorBuildStructFieldsPath =
        cwd.parent_path() / "src" / "semantics" / "SemanticsValidatorBuildStructFields.cpp";
    validatorBuildDirectCallBindingPath =
        cwd.parent_path() / "src" / "semantics" / "SemanticsValidatorBuildDirectCallBinding.cpp";
    validatorCollectionsPath = cwd.parent_path() / "src" / "semantics" / "SemanticsValidatorInferCollections.cpp";
    validatorExprPath = cwd.parent_path() / "src" / "semantics" / "SemanticsValidatorExpr.cpp";
    validatorExprCollectionDispatchSetupPath =
        cwd.parent_path() / "src" / "semantics" / "SemanticsValidatorExprCollectionDispatchSetup.cpp";
    validatorExprCollectionAccessSetupPath =
        cwd.parent_path() / "src" / "semantics" / "SemanticsValidatorExprCollectionAccessSetup.cpp";
    validatorExprDirectCollectionFallbacksPath =
        cwd.parent_path() / "src" / "semantics" / "SemanticsValidatorExprDirectCollectionFallbacks.cpp";
    validatorExprMethodResolutionPath =
        cwd.parent_path() / "src" / "semantics" / "SemanticsValidatorExprMethodResolution.cpp";
    validatorExprBodyArgumentsPath =
        cwd.parent_path() / "src" / "semantics" / "SemanticsValidatorExprBodyArguments.cpp";
    validatorCollectionHelperRewritesPath =
        cwd.parent_path() / "src" / "semantics" / "SemanticsValidatorCollectionHelperRewrites.cpp";
    validatorInferPath = cwd.parent_path() / "src" / "semantics" / "SemanticsValidatorInfer.cpp";
    validatorInferDefinitionPath =
        cwd.parent_path() / "src" / "semantics" / "SemanticsValidatorInferDefinition.cpp";
    validatorInferGraphPath = cwd.parent_path() / "src" / "semantics" / "SemanticsValidatorInferGraph.cpp";
    validatorInferMethodResolutionPath =
        cwd.parent_path() / "src" / "semantics" / "SemanticsValidatorInferMethodResolution.cpp";
    validatorInferStructReturnPath =
        cwd.parent_path() / "src" / "semantics" / "SemanticsValidatorInferStructReturn.cpp";
    validatorInferTargetResolutionPath =
        cwd.parent_path() / "src" / "semantics" / "SemanticsValidatorInferTargetResolution.cpp";
    validatorInferUtilityPath =
        cwd.parent_path() / "src" / "semantics" / "SemanticsValidatorInferUtility.cpp";
    pipelinePath = cwd.parent_path() / "src" / "CompilePipeline.cpp";
    primecMainPath = cwd.parent_path() / "src" / "main.cpp";
    primevmMainPath = cwd.parent_path() / "src" / "primevm_main.cpp";
  }
  REQUIRE(std::filesystem::exists(optionsHeaderPath));
  REQUIRE(std::filesystem::exists(optionsParserPath));
  REQUIRE(std::filesystem::exists(semanticsHeaderPath));
  REQUIRE(std::filesystem::exists(semanticsValidatePath));
  REQUIRE(std::filesystem::exists(validatorHeaderPath));
  REQUIRE(std::filesystem::exists(validatorCorePath));
  REQUIRE(std::filesystem::exists(validatorBuildPath));
  REQUIRE(std::filesystem::exists(validatorBuildUtilityPath));
  REQUIRE(std::filesystem::exists(validatorBuildStructFieldsPath));
  REQUIRE(std::filesystem::exists(validatorBuildDirectCallBindingPath));
  REQUIRE(std::filesystem::exists(validatorCollectionsPath));
  REQUIRE(std::filesystem::exists(validatorExprPath));
  REQUIRE(std::filesystem::exists(validatorExprCollectionDispatchSetupPath));
  REQUIRE(std::filesystem::exists(validatorExprCollectionAccessSetupPath));
  REQUIRE(std::filesystem::exists(validatorExprDirectCollectionFallbacksPath));
  REQUIRE(std::filesystem::exists(validatorExprMethodResolutionPath));
  REQUIRE(std::filesystem::exists(validatorExprBodyArgumentsPath));
  REQUIRE(std::filesystem::exists(validatorCollectionHelperRewritesPath));
  REQUIRE(std::filesystem::exists(validatorInferPath));
  REQUIRE(std::filesystem::exists(validatorInferDefinitionPath));
  REQUIRE(std::filesystem::exists(validatorInferGraphPath));
  REQUIRE(std::filesystem::exists(validatorInferMethodResolutionPath));
  REQUIRE(std::filesystem::exists(validatorInferStructReturnPath));
  REQUIRE(std::filesystem::exists(validatorInferTargetResolutionPath));
  REQUIRE(std::filesystem::exists(validatorInferUtilityPath));
  REQUIRE(std::filesystem::exists(pipelinePath));
  REQUIRE(std::filesystem::exists(primecMainPath));
  REQUIRE(std::filesystem::exists(primevmMainPath));

  const std::string optionsHeader = readTextFile(optionsHeaderPath);
  const std::string optionsParser = readTextFile(optionsParserPath);
  const std::string semanticsHeader = readTextFile(semanticsHeaderPath);
  const std::string semanticsValidate = readTextFile(semanticsValidatePath);
  const std::string validatorHeader = readTextFile(validatorHeaderPath);
  const std::string validatorCore = readTextFile(validatorCorePath);
  const std::string validatorBuild = readTextFiles({
      validatorBuildPath,
      validatorBuildUtilityPath,
      validatorBuildStructFieldsPath,
      validatorBuildDirectCallBindingPath,
  });
  const std::string validatorCollections = readTextFile(validatorCollectionsPath);
  const std::string validatorExprMain = readTextFile(validatorExprPath);
  const std::string validatorExprBodyArguments =
      readTextFile(validatorExprBodyArgumentsPath);
  const std::string validatorExpr = readTextFiles({
      validatorExprPath,
      validatorExprCollectionDispatchSetupPath,
      validatorExprCollectionAccessSetupPath,
      validatorExprDirectCollectionFallbacksPath,
      validatorExprMethodResolutionPath,
      validatorCollectionHelperRewritesPath,
  });
  const std::string validatorInferMain = readTextFile(validatorInferPath);
  const std::string validatorInfer = readTextFiles({
      validatorInferPath,
      validatorCollectionsPath,
      validatorInferDefinitionPath,
      validatorInferGraphPath,
      validatorInferMethodResolutionPath,
      validatorInferStructReturnPath,
      validatorInferTargetResolutionPath,
      validatorInferUtilityPath,
  });
  const std::string pipeline = readTextFile(pipelinePath);
  const std::string primecMain = readTextFile(primecMainPath);
  const std::string primevmMain = readTextFile(primevmMainPath);

  CHECK(optionsHeader.find("std::string typeResolver = \"graph\";") != std::string::npos);
  CHECK(optionsParser.find("unsupported --type-resolver value: ") != std::string::npos);
  CHECK(optionsParser.find("--type-resolver requires a value") != std::string::npos);
  CHECK(semanticsHeader.find("const std::string &typeResolver = \"graph\"") != std::string::npos);
  CHECK(semanticsValidate.find("collectDiagnostics, typeResolver") != std::string::npos);
  CHECK(validatorHeader.find("bool inferUnknownReturnKindsGraph();") != std::string::npos);
  CHECK(validatorHeader.find("void collectGraphLocalAutoBindings(const TypeResolutionGraph &graph);") !=
        std::string::npos);
  CHECK(validatorHeader.find("bool lookupGraphLocalAutoBinding(const std::string &scopePath,") !=
        std::string::npos);
  CHECK(validatorHeader.find("bool lookupGraphLocalAutoBinding(const Expr &bindingExpr, BindingInfo &bindingOut) const;") !=
        std::string::npos);
  CHECK(validatorHeader.find("std::unordered_map<std::string, BindingInfo> graphLocalAutoBindings_;") !=
        std::string::npos);
  CHECK(validatorHeader.find("bool ensureDefinitionReturnKindReady(const Definition &def);") != std::string::npos);
  CHECK(validatorHeader.find("bool inferDefinitionReturnKindGraphStep") != std::string::npos);
  CHECK(validatorHeader.find("bool graphTypeResolverEnabled_ = false;") != std::string::npos);
  CHECK(validatorHeader.find("bool inferDefinitionReturnBinding(const Definition &def, BindingInfo &bindingOut);") !=
        std::string::npos);
  CHECK(validatorHeader.find("bool inferQueryExprTypeText(const Expr &expr,") != std::string::npos);
  CHECK(validatorHeader.find("std::string preferredExperimentalMapHelperTarget(std::string_view helperName) const;") !=
        std::string::npos);
  CHECK(validatorHeader.find("std::string preferredCanonicalExperimentalMapHelperTarget(std::string_view helperName) const;") !=
        std::string::npos);
  CHECK(validatorHeader.find("bool canonicalExperimentalMapHelperPath(const std::string &resolvedPath,") !=
        std::string::npos);
  CHECK(validatorHeader.find("bool canonicalizeExperimentalMapHelperResolvedPath(const std::string &resolvedPath,") !=
        std::string::npos);
  CHECK(validatorHeader.find("bool shouldBuiltinValidateCurrentMapWrapperHelper(std::string_view helperName) const;") !=
        std::string::npos);
  CHECK(validatorHeader.find("std::string mapNamespacedMethodCompatibilityPath(") !=
        std::string::npos);
  CHECK(validatorHeader.find("std::string directMapHelperCompatibilityPath(") !=
        std::string::npos);
  CHECK(validatorHeader.find("std::string explicitRemovedCollectionMethodPath(std::string_view rawMethodName,") !=
        std::string::npos);
  CHECK(validatorHeader.find("bool shouldPreserveRemovedCollectionHelperPath(const std::string &path) const;") !=
        std::string::npos);
  CHECK(validatorHeader.find("bool isUnnamespacedMapCountBuiltinFallbackCall(") !=
        std::string::npos);
  CHECK(validatorHeader.find("bool resolveRemovedMapBodyArgumentTarget(") !=
        std::string::npos);
  CHECK(validatorHeader.find("struct BuiltinCollectionDispatchResolverAdapters {") !=
        std::string::npos);
  CHECK(validatorHeader.find("std::function<bool(const Expr &, BindingInfo &)> resolveBindingTarget;") !=
        std::string::npos);
  CHECK(validatorHeader.find("std::function<bool(const Expr &, BindingInfo &)> inferCallBinding;") !=
        std::string::npos);
  CHECK(validatorHeader.find("BuiltinCollectionDispatchResolvers makeBuiltinCollectionDispatchResolvers(") !=
        std::string::npos);
  CHECK(validatorHeader.find("std::function<bool(const Expr &, std::string &)> resolveIndexedArgsPackElementType;") !=
        std::string::npos);
  CHECK(validatorHeader.find("std::function<bool(const Expr &, std::string &)> resolveDereferencedIndexedArgsPackElementType;") !=
        std::string::npos);
  CHECK(validatorHeader.find("std::function<bool(const Expr &, std::string &)> resolveWrappedIndexedArgsPackElementType;") !=
        std::string::npos);
  CHECK(validatorHeader.find("std::function<bool(const Expr &, std::string &)> resolveSoaVectorTarget;") !=
        std::string::npos);
  CHECK(validatorHeader.find("std::function<bool(const Expr &, std::string &)> resolveBufferTarget;") !=
        std::string::npos);
  CHECK(validatorHeader.find("std::function<bool(const Expr &, std::string &, std::string &)> resolveExperimentalMapTarget;") !=
        std::string::npos);
  CHECK(validatorHeader.find("std::function<bool(const Expr &, std::string &, std::string &)> resolveExperimentalMapValueTarget;") !=
        std::string::npos);
  CHECK(validatorBuild.find("lookupGraphLocalAutoBinding(currentValidationContext_.definitionPath, bindingExpr, bindingOut)") !=
        std::string::npos);
  CHECK(validatorBuild.find("lookupGraphLocalAutoBinding(structDef.fullPath, fieldStmt, bindingOut)") !=
        std::string::npos);
  CHECK(validatorCollections.find("ValidationContextScope validationContextScope(*this, buildDefinitionValidationContext(def));") !=
        std::string::npos);
  CHECK(validatorCollections.find("bool SemanticsValidator::inferQueryExprTypeText(const Expr &expr,") !=
        std::string::npos);
  CHECK(validatorCollections.find("SemanticsValidator::makeBuiltinCollectionDispatchResolvers(") !=
        std::string::npos);
  CHECK(validatorCollections.find("const BuiltinCollectionDispatchResolverAdapters &adapters") !=
        std::string::npos);
  CHECK(validatorCollections.find("auto resolveBindingTarget = [") !=
        std::string::npos);
  CHECK(validatorCollections.find("auto inferCallBinding = [") !=
        std::string::npos);
  CHECK(validatorCollections.find("auto isDirectMapConstructorCall = [") !=
        std::string::npos);
  CHECK(validatorCollections.find("state->resolveSoaVectorTarget = [") !=
        std::string::npos);
  CHECK(validatorCollections.find("state->resolveBufferTarget = [") !=
        std::string::npos);
  CHECK(validatorCollections.find("state->resolveExperimentalMapTarget =") != std::string::npos);
  CHECK(validatorCollections.find("state->resolveExperimentalMapValueTarget =") != std::string::npos);
  CHECK(validatorCollections.find("struct ExperimentalMapHelperDescriptor {") != std::string::npos);
  CHECK(validatorCollections.find("constexpr ExperimentalMapHelperDescriptor kExperimentalMapHelperDescriptors[] = {") !=
        std::string::npos);
  CHECK(validatorCollections.find("std::string SemanticsValidator::preferredExperimentalMapHelperTarget(std::string_view helperName) const {") !=
        std::string::npos);
  CHECK(validatorCollections.find("std::string SemanticsValidator::preferredCanonicalExperimentalMapHelperTarget(std::string_view helperName) const {") !=
        std::string::npos);
  CHECK(validatorCollections.find("bool SemanticsValidator::canonicalExperimentalMapHelperPath(const std::string &resolvedPath,") !=
        std::string::npos);
  CHECK(validatorCollections.find("bool SemanticsValidator::canonicalizeExperimentalMapHelperResolvedPath(const std::string &resolvedPath,") !=
        std::string::npos);
  CHECK(validatorCollections.find("bool SemanticsValidator::shouldBuiltinValidateCurrentMapWrapperHelper(std::string_view helperName) const {") !=
        std::string::npos);
  CHECK(validatorCollections.find("std::string SemanticsValidator::mapNamespacedMethodCompatibilityPath(") !=
        std::string::npos);
  CHECK(validatorCollections.find("std::string SemanticsValidator::directMapHelperCompatibilityPath(") !=
        std::string::npos);
  CHECK(validatorCollections.find("enum class RemovedCollectionHelperFamily {") != std::string::npos);
  CHECK(validatorCollections.find("struct RemovedCollectionHelperDescriptor {") != std::string::npos);
  CHECK(validatorCollections.find("constexpr RemovedCollectionHelperDescriptor kRemovedCollectionHelperDescriptors[] = {") !=
        std::string::npos);
  CHECK(validatorCollections.find("std::string SemanticsValidator::explicitRemovedCollectionMethodPath(std::string_view rawMethodName,") !=
        std::string::npos);
  CHECK(validatorCollections.find("bool SemanticsValidator::shouldPreserveRemovedCollectionHelperPath(const std::string &path) const {") !=
        std::string::npos);
  CHECK(validatorCollections.find("bool SemanticsValidator::isUnnamespacedMapCountBuiltinFallbackCall(") !=
        std::string::npos);
  CHECK(validatorCollections.find("findRemovedCollectionHelperReference(") != std::string::npos);
  CHECK(validatorCollections.find("bool SemanticsValidator::resolveRemovedMapBodyArgumentTarget(") !=
        std::string::npos);
  CHECK(validatorCollections.find("if (inferQueryExprTypeText(target, params, locals, targetTypeText))") !=
        std::string::npos);
  CHECK(validatorBuild.find("bool SemanticsValidator::inferResolvedDirectCallBindingType(") !=
        std::string::npos);
  CHECK(validatorCore.find("return inferQueryExprTypeText(receiverExpr, params, locals, typeTextOut);") !=
        std::string::npos);
  CHECK(validatorCore.find("auto isEnvelopeValueExpr = [&](const Expr &candidate, bool allowAnyName) -> bool {") ==
        std::string::npos);
  CHECK(validatorCore.find("inferExprTypeText(stmt.args.front(), defParams, defLocals, inferredLocalType)") ==
        std::string::npos);
  CHECK(validatorExprMain.find("auto resolveCallCollectionTypePath = [&](const Expr &target, std::string &typePathOut) -> bool {") ==
        std::string::npos);
  CHECK(validatorExprMain.find("auto resolveCallCollectionTemplateArgs =") == std::string::npos);
  CHECK(validatorExpr.find("const BuiltinCollectionDispatchResolverAdapters builtinCollectionDispatchResolverAdapters{") !=
        std::string::npos);
  CHECK(validatorExpr.find("const BuiltinCollectionDispatchResolvers builtinCollectionDispatchResolvers =") !=
        std::string::npos);
  CHECK(validatorExpr.find("makeBuiltinCollectionDispatchResolvers(params, locals, builtinCollectionDispatchResolverAdapters)") !=
        std::string::npos);
  CHECK(validatorExpr.find("auto resolveIndexedArgsPackElementType = [&](const Expr &target, std::string &elemTypeOut) -> bool {") !=
        std::string::npos);
  CHECK(validatorExpr.find("auto resolveDereferencedIndexedArgsPackElementType = [&](const Expr &target, std::string &elemTypeOut) -> bool {") !=
        std::string::npos);
  CHECK(validatorExpr.find("auto resolveWrappedIndexedArgsPackElementType = [&](const Expr &target, std::string &elemTypeOut) -> bool {") !=
        std::string::npos);
  CHECK(validatorExpr.find("const auto &resolveArgsPackAccessTarget = builtinCollectionDispatchResolvers.resolveArgsPackAccessTarget;") !=
        std::string::npos);
  CHECK(validatorExpr.find("const auto &resolveArrayTarget = builtinCollectionDispatchResolvers.resolveArrayTarget;") !=
        std::string::npos);
  CHECK(validatorExpr.find("const auto &resolveVectorTarget = builtinCollectionDispatchResolvers.resolveVectorTarget;") !=
        std::string::npos);
  CHECK(validatorExpr.find("const auto &resolveSoaVectorTarget = builtinCollectionDispatchResolvers.resolveSoaVectorTarget;") !=
        std::string::npos);
  CHECK(validatorExpr.find("const auto &resolveStringTarget = builtinCollectionDispatchResolvers.resolveStringTarget;") !=
        std::string::npos);
  CHECK(validatorExpr.find("const auto &resolveMapTargetWithTypes = builtinCollectionDispatchResolvers.resolveMapTarget;") !=
        std::string::npos);
  CHECK(validatorExpr.find("const auto &resolveExperimentalMapTarget =") != std::string::npos);
  CHECK(validatorExpr.find("builtinCollectionDispatchResolvers.resolveExperimentalMapTarget;") !=
        std::string::npos);
  CHECK(validatorExpr.find("dispatchResolvers.resolveExperimentalMapValueTarget == nullptr") != std::string::npos);
  CHECK(validatorExpr.find("dispatchResolvers.resolveExperimentalMapValueTarget(receiverExpr, keyType, valueType)") !=
        std::string::npos);
  CHECK(validatorExprMain.find("auto preferredExperimentalMapHelperTarget = [&](std::string_view helperName) {") ==
        std::string::npos);
  CHECK(validatorExprMain.find("auto preferredCanonicalExperimentalMapHelperTarget = [&](std::string_view helperName) {") ==
        std::string::npos);
  CHECK(validatorExprMain.find("auto canonicalExperimentalMapHelperPath = [&](const std::string &resolvedPath, std::string &canonicalPathOut, std::string &helperNameOut) {") ==
        std::string::npos);
  CHECK(validatorExprMain.find("auto canonicalizeExperimentalMapHelperResolvedPath = [&](const std::string &resolvedPath,") ==
        std::string::npos);
  CHECK(validatorExprMain.find("auto shouldBuiltinValidateCurrentMapWrapperHelper = [&](std::string_view helperName) {") ==
        std::string::npos);
  CHECK(validatorExprMain.find("auto isMapNamespacedCountCompatibilityCall = [&](const Expr &candidate) -> bool {") ==
        std::string::npos);
  CHECK(validatorExprMain.find("auto isMapNamespacedAccessCompatibilityCall = [&](const Expr &candidate) -> bool {") ==
        std::string::npos);
  CHECK(validatorExprMain.find("auto getMapNamespacedMethodCompatibilityPath = [&](const Expr &candidate) -> std::string {") ==
        std::string::npos);
  CHECK(validatorExprMain.find("auto getDirectMapHelperCompatibilityPath = [&](const Expr &candidate) -> std::string {") ==
        std::string::npos);
  CHECK(validatorExprMain.find("auto isUnnamespacedMapCountBuiltinFallbackCall = [&](const Expr &candidate) -> bool {") ==
        std::string::npos);
  CHECK(validatorExprMain.find("auto explicitRemovedCollectionMethodPath = [&](const std::string &rawMethodName) -> std::string {") ==
        std::string::npos);
  CHECK(validatorExprMain.find("auto shouldPreserveBodyArgumentTarget = [&](const std::string &path) -> bool {") ==
        std::string::npos);
  CHECK(validatorExprMain.find("auto isRemovedVectorCompatibilityHelper = [](std::string_view helperName) {") ==
        std::string::npos);
  CHECK(validatorExprMain.find("auto isRemovedMapCompatibilityHelper = [](std::string_view helperName) {") ==
        std::string::npos);
  CHECK(validatorExpr.find("preferredCanonicalExperimentalMapHelperTarget(helperName)") != std::string::npos);
  CHECK(validatorExpr.find("canonicalExperimentalMapHelperPath(") != std::string::npos);
  CHECK(validatorExpr.find("canonicalizeExperimentalMapHelperResolvedPath(") != std::string::npos);
  CHECK(validatorExpr.find("shouldBuiltinValidateCurrentMapWrapperHelper(") != std::string::npos);
  CHECK(validatorHeader.find("bool validateExprMethodCallTarget(") != std::string::npos);
  CHECK(validatorExprMain.find("ExprMethodResolutionContext methodResolutionContext;") !=
        std::string::npos);
  CHECK(validatorExprMain.find("validateExprMethodCallTarget(") != std::string::npos);
  CHECK(validatorExprMain.find("mapNamespacedMethodCompatibilityPath(expr, params, locals, builtinCollectionDispatchResolverAdapters)") ==
        std::string::npos);
  CHECK(validatorExpr.find("mapNamespacedMethodCompatibilityPath(expr, params, locals, dispatchResolverAdapters)") !=
        std::string::npos);
  CHECK(validatorExprMain.find("error_ = \"method call missing receiver\";") == std::string::npos);
  CHECK(validatorExpr.find("error_ = \"method call missing receiver\";") != std::string::npos);
  CHECK(validatorExprMain.find("auto resolveInferredMapMethodFallback = [&]() -> bool {") ==
        std::string::npos);
  CHECK(validatorExpr.find("auto resolveInferredMapMethodFallback = [&]() -> bool {") !=
        std::string::npos);
  CHECK(validatorExpr.find("directMapHelperCompatibilityPath(") != std::string::npos);
  CHECK(validatorExpr.find("explicitRemovedCollectionMethodPath(methodName)") !=
        std::string::npos);
  CHECK(validatorExpr.find("shouldPreserveRemovedCollectionHelperPath(resolved)") ==
        std::string::npos);
  CHECK(validatorExprBodyArguments.find("shouldPreserveRemovedCollectionHelperPath(resolved)") !=
        std::string::npos);
  CHECK(validatorExpr.find("isUnnamespacedMapCountBuiltinFallbackCall(expr, params, locals, builtinCollectionDispatchResolverAdapters)") !=
        std::string::npos);
  CHECK(validatorExpr.find("resolveRemovedMapBodyArgumentTarget(") ==
        std::string::npos);
  CHECK(validatorExprBodyArguments.find("resolveRemovedMapBodyArgumentTarget(") !=
        std::string::npos);
  CHECK(validatorExprMain.find("auto resolveBareMapCallBodyArgumentTarget = [&]() -> bool {") ==
        std::string::npos);
  CHECK(validatorExprMain.find("auto remapWrappedMapMethodBodyArgumentTarget = [&]() -> bool {") ==
        std::string::npos);
  CHECK(validatorHeader.find("struct ExprCollectionDispatchSetup") != std::string::npos);
  CHECK(validatorHeader.find("bool prepareExprCollectionDispatchSetup(") != std::string::npos);
  CHECK(validatorHeader.find("void prepareExprCollectionAccessDispatchContext(") !=
        std::string::npos);
  CHECK(validatorHeader.find("struct ExprDirectCollectionFallbackContext") != std::string::npos);
  CHECK(validatorHeader.find("bool validateExprDirectCollectionFallbacks(") != std::string::npos);
  CHECK(validatorExprMain.find("const std::string directRemovedMapCompatibilityPath =") ==
        std::string::npos);
  CHECK(validatorExpr.find("const std::string directRemovedMapCompatibilityPath =") !=
        std::string::npos);
  CHECK(validatorExprMain.find("prepareExprCollectionDispatchSetup(") != std::string::npos);
  CHECK(validatorExpr.find("bool SemanticsValidator::prepareExprCollectionDispatchSetup(") !=
        std::string::npos);
  CHECK(validatorExprMain.find("prepareExprCollectionAccessDispatchContext(") !=
        std::string::npos);
  CHECK(validatorExpr.find("void SemanticsValidator::prepareExprCollectionAccessDispatchContext(") !=
        std::string::npos);
  CHECK(validatorExprMain.find("validateExprDirectCollectionFallbacks(") != std::string::npos);
  CHECK(validatorExpr.find("bool SemanticsValidator::validateExprDirectCollectionFallbacks(") !=
        std::string::npos);
  CHECK(validatorExprMain.find("const bool allowStdNamespacedVectorUserReceiverProbe =") ==
        std::string::npos);
  CHECK(validatorExpr.find("const bool allowStdNamespacedVectorUserReceiverProbe =") !=
        std::string::npos);
  CHECK(validatorExprMain.find("auto resolvesExperimentalVectorValueReceiverForBareAccess =") ==
        std::string::npos);
  CHECK(validatorExpr.find("auto resolvesExperimentalVectorValueReceiverForBareAccess =") !=
        std::string::npos);
  CHECK(validatorExprMain.find("collectionAccessDispatchContext.resolveArrayTarget =") ==
        std::string::npos);
  CHECK(validatorExpr.find("contextOut.resolveArrayTarget = dispatchResolvers.resolveArrayTarget;") !=
        std::string::npos);
  CHECK(validatorExpr.find("if (resolveMapTargetWithTypes(target, keyType, valueType) ||") !=
        std::string::npos);
  CHECK(validatorExpr.find("resolveExperimentalMapTarget(target, keyType, valueType)) {") !=
        std::string::npos);
  CHECK(validatorExpr.find("auto resolveMapValueType = [&](const Expr &target, std::string &valueTypeOut) -> bool {") !=
        std::string::npos);
  CHECK(validatorExprMain.find("auto resolveIndexedArgsPackElementType = [&](const Expr &target, std::string &elemTypeOut) -> bool {") ==
        std::string::npos);
  CHECK(validatorExprMain.find("auto resolveDereferencedIndexedArgsPackElementType = [&](const Expr &target, std::string &elemTypeOut) -> bool {") ==
        std::string::npos);
  CHECK(validatorExprMain.find("auto resolveWrappedIndexedArgsPackElementType = [&](const Expr &target, std::string &elemTypeOut) -> bool {") ==
        std::string::npos);
  CHECK(validatorExprMain.find("auto extractWrappedPointeeType = [&](const std::string &typeText, std::string &pointeeTypeOut) -> bool {") ==
        std::string::npos);
  CHECK(validatorExprMain.find("auto extractCollectionElementType = [&](const std::string &typeText,") ==
        std::string::npos);
  CHECK(validatorExprMain.find("auto resolveArgsPackAccessTarget = [&](const Expr &target, std::string &elemType) -> bool {") ==
        std::string::npos);
  CHECK(validatorExprMain.find("auto resolveArrayTarget = [&](const Expr &target, std::string &elemType) -> bool {") ==
        std::string::npos);
  CHECK(validatorExprMain.find("auto resolveVectorTarget = [&](const Expr &target, std::string &elemType) -> bool {") ==
        std::string::npos);
  CHECK(validatorExprMain.find("auto resolveSoaVectorTarget = [&](const Expr &target, std::string &elemType) -> bool {") ==
        std::string::npos);
  CHECK(validatorExprMain.find("std::function<bool(const Expr &)> resolveStringTarget =") == std::string::npos);
  CHECK(validatorExprMain.find("auto resolveMapValueTypeForStringTarget = [&](const Expr &target, std::string &valueTypeOut) -> bool {") ==
        std::string::npos);
  CHECK(validatorExprMain.find("auto extractExperimentalMapFieldTypes = [&](const BindingInfo &binding,") ==
        std::string::npos);
  CHECK(validatorExprMain.find("auto extractAnyMapKeyValueTypes = [&](const BindingInfo &binding,") ==
        std::string::npos);
  CHECK(validatorExprMain.find("auto resolveExperimentalMapTarget = [&](const Expr &target,") ==
        std::string::npos);
  CHECK(validatorExprMain.find("auto resolveExperimentalMapValueTarget = [&](const Expr &target,") ==
        std::string::npos);
  CHECK(validatorInfer.find("buildTypeResolutionGraph(program_)") != std::string::npos);
  CHECK(validatorInfer.find("collectGraphLocalAutoBindings(graph);") != std::string::npos);
  CHECK(validatorInfer.find("dependencyCountByBindingKey") != std::string::npos);
  CHECK(validatorInfer.find("dependencyIt->second > 0 &&") != std::string::npos);
  CHECK(validatorInfer.find("isIfCall(initializer) || isMatchCall(initializer) || isBuiltinBlockCall(initializer)") !=
        std::string::npos);
  CHECK(validatorInfer.find("graphLocalAutoBindings_.try_emplace(bindingKey, binding);") !=
        std::string::npos);
  CHECK(validatorInfer.find("computeCondensationDag(") != std::string::npos);
  CHECK(validatorInfer.find("std::vector<const Definition *> unresolvedDefinitions = collectUnknownDefinitions(componentNode);") !=
        std::string::npos);
  CHECK(validatorInfer.find("makeBuiltinCollectionDispatchResolvers(params, locals, ") != std::string::npos);
  CHECK(validatorInfer.find("const auto &resolveIndexedArgsPackElementType =") != std::string::npos);
  CHECK(validatorInfer.find("builtinCollectionDispatchResolvers.resolveIndexedArgsPackElementType;") !=
        std::string::npos);
  CHECK(validatorInfer.find("const auto &resolveDereferencedIndexedArgsPackElementType =") !=
        std::string::npos);
  CHECK(validatorInfer.find("builtinCollectionDispatchResolvers.resolveDereferencedIndexedArgsPackElementType;") !=
        std::string::npos);
  CHECK(validatorInfer.find("const auto &resolveWrappedIndexedArgsPackElementType =") != std::string::npos);
  CHECK(validatorInfer.find("builtinCollectionDispatchResolvers.resolveWrappedIndexedArgsPackElementType;") !=
        std::string::npos);
  CHECK(validatorInfer.find("const auto &resolveArgsPackAccessTarget = builtinCollectionDispatchResolvers.resolveArgsPackAccessTarget;") !=
        std::string::npos);
  CHECK(validatorInfer.find("const auto &resolveArrayTarget = builtinCollectionDispatchResolvers.resolveArrayTarget;") !=
        std::string::npos);
  CHECK(validatorInfer.find("const auto &resolveVectorTarget = builtinCollectionDispatchResolvers.resolveVectorTarget;") !=
        std::string::npos);
  CHECK(validatorInfer.find("const auto &resolveSoaVectorTarget = builtinCollectionDispatchResolvers.resolveSoaVectorTarget;") !=
        std::string::npos);
  CHECK(validatorInfer.find("const auto &resolveBufferTarget = builtinCollectionDispatchResolvers.resolveBufferTarget;") !=
        std::string::npos);
  CHECK(validatorInfer.find("const auto &resolveStringTarget = builtinCollectionDispatchResolvers.resolveStringTarget;") !=
        std::string::npos);
  CHECK(validatorInfer.find("const auto &resolveMapTarget = builtinCollectionDispatchResolvers.resolveMapTarget;") !=
        std::string::npos);
  CHECK(validatorInferMain.find("auto resolveIndexedArgsPackElementType = [&](const Expr &target, std::string &elemTypeOut) -> bool {") ==
        std::string::npos);
  CHECK(validatorInferMain.find("auto resolveDereferencedIndexedArgsPackElementType = [&](const Expr &target, std::string &elemTypeOut) -> bool {") ==
        std::string::npos);
  CHECK(validatorInferMain.find("auto resolveWrappedIndexedArgsPackElementType = [&](const Expr &target, std::string &elemTypeOut) -> bool {") ==
        std::string::npos);
  CHECK(validatorInferMain.find("auto resolveArgsPackAccessTarget = [&](const Expr &target, std::string &elemType)") ==
        std::string::npos);
  CHECK(validatorInferMain.find("auto resolveArrayTarget = [&](const Expr &target, std::string &elemType)") ==
        std::string::npos);
  CHECK(validatorInferMain.find("auto resolveVectorTarget = [&](const Expr &target, std::string &elemType)") ==
        std::string::npos);
  CHECK(validatorInferMain.find("auto resolveSoaVectorTarget = [&](const Expr &target, std::string &elemType) -> bool {") ==
        std::string::npos);
  CHECK(validatorInferMain.find("auto resolveBufferTarget = [&](const Expr &target, std::string &elemType) -> bool {") ==
        std::string::npos);
  CHECK(validatorInferMain.find("std::function<bool(const Expr &)> resolveStringTarget =") == std::string::npos);
  CHECK(validatorInferMain.find("std::function<bool(const Expr &, std::string &, std::string &)> resolveMapTarget =") ==
        std::string::npos);
  CHECK(validatorInferMain.find("auto resolveBuiltinAccessReceiverExprInline = [&](const Expr &accessExpr) -> const Expr * {") ==
        std::string::npos);
  CHECK(validatorInferMain.find("auto extractWrappedPointeeType = [&](const std::string &typeText, std::string &pointeeTypeOut) -> bool {") ==
        std::string::npos);
  CHECK(validatorInferMain.find("auto extractCollectionElementType = [&](const std::string &typeText,") ==
        std::string::npos);
  CHECK(validatorInfer.find("return SemanticsValidator::resolveCallCollectionTypePath(target, params, locals, typePathOut);") !=
        std::string::npos);
  CHECK(validatorInfer.find("return SemanticsValidator::resolveCallCollectionTemplateArgs(target, expectedBase, params, locals, argsOut);") !=
        std::string::npos);
  CHECK(validatorInferMain.find("auto inferTargetTypeText = [&](const Expr &candidate, std::string &typeTextOut) -> bool {") ==
        std::string::npos);
  CHECK(validatorInferMain.find("auto extractAnyMapKeyValueTypes = [&](const BindingInfo &binding,") ==
        std::string::npos);
  CHECK(validatorInfer.find("formatReturnInferenceCycleDiagnostic(") != std::string::npos);
  CHECK(validatorInfer.find("cycle member: ") != std::string::npos);
  CHECK(validatorInfer.find("allowRecursiveReturnInference_ = false;") != std::string::npos);
  CHECK(validatorInfer.find("ensureDefinitionReturnKindReady(*defIt->second)") != std::string::npos);
  CHECK(pipeline.find("options.typeResolver") != std::string::npos);
  CHECK(primecMain.find("[--type-resolver legacy|graph]") != std::string::npos);
  CHECK(primevmMain.find("[--type-resolver legacy|graph]") != std::string::npos);
}

TEST_CASE("type resolver parity harness is wired through ir pipeline tests") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path irPipelinePath = cwd / "tests" / "unit" / "test_ir_pipeline.cpp";
  std::filesystem::path parityHeaderPath = cwd / "tests" / "unit" / "test_ir_pipeline_type_resolution_parity.h";
  if (!std::filesystem::exists(irPipelinePath)) {
    irPipelinePath = cwd.parent_path() / "tests" / "unit" / "test_ir_pipeline.cpp";
    parityHeaderPath = cwd.parent_path() / "tests" / "unit" / "test_ir_pipeline_type_resolution_parity.h";
  }
  REQUIRE(std::filesystem::exists(irPipelinePath));
  REQUIRE(std::filesystem::exists(parityHeaderPath));

  const std::string irPipeline = readTextFile(irPipelinePath);
  const std::string parityHeader = readTextFile(parityHeaderPath);
  CHECK(irPipeline.find("TypeResolverPipelineSnapshot") != std::string::npos);
  CHECK(irPipeline.find("runTypeResolverPipelineSnapshot") != std::string::npos);
  CHECK(irPipeline.find("#include \"test_ir_pipeline_type_resolution_parity.h\"") != std::string::npos);
  CHECK(parityHeader.find("legacy and graph type resolvers keep diagnostics and vm ir aligned on parity corpus") !=
        std::string::npos);
  CHECK(parityHeader.find("direct_call_local_auto_struct") != std::string::npos);
  CHECK(parityHeader.find("direct_call_local_auto_collection") != std::string::npos);
  CHECK(parityHeader.find("block_local_auto_struct") != std::string::npos);
  CHECK(parityHeader.find("if_local_auto_collection") != std::string::npos);
  CHECK(parityHeader.find("ambiguous_omitted_field_envelope") != std::string::npos);
  CHECK(parityHeader.find("query_collection_return_binding") != std::string::npos);
  CHECK(parityHeader.find("query_result_return_binding") != std::string::npos);
  CHECK(parityHeader.find("query_map_receiver_type_text") != std::string::npos);
  CHECK(parityHeader.find("infer_map_value_return_kind") != std::string::npos);
  CHECK(parityHeader.find("shared_collection_receiver_classifiers") != std::string::npos);
  CHECK(parityHeader.find("graph type resolver intentionally upgrades recursive cycle diagnostics") !=
        std::string::npos);
  CHECK(parityHeader.find("grounded mutual recursion currently diverges between legacy and graph vm pipelines") !=
        std::string::npos);
}

TEST_CASE("strongly connected component utility is wired through semantics testing api") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path cmakePath = cwd / "CMakeLists.txt";
  std::filesystem::path testApiPath = cwd / "include" / "primec" / "testing" / "SemanticsValidationHelpers.h";
  std::filesystem::path sccHeaderPath = cwd / "src" / "semantics" / "StronglyConnectedComponents.h";
  std::filesystem::path sccSourcePath = cwd / "src" / "semantics" / "StronglyConnectedComponents.cpp";
  if (!std::filesystem::exists(cmakePath)) {
    cmakePath = cwd.parent_path() / "CMakeLists.txt";
    testApiPath = cwd.parent_path() / "include" / "primec" / "testing" / "SemanticsValidationHelpers.h";
    sccHeaderPath = cwd.parent_path() / "src" / "semantics" / "StronglyConnectedComponents.h";
    sccSourcePath = cwd.parent_path() / "src" / "semantics" / "StronglyConnectedComponents.cpp";
  }
  REQUIRE(std::filesystem::exists(cmakePath));
  REQUIRE(std::filesystem::exists(testApiPath));
  REQUIRE(std::filesystem::exists(sccHeaderPath));
  REQUIRE(std::filesystem::exists(sccSourcePath));

  const std::string cmake = readTextFile(cmakePath);
  const std::string testApi = readTextFile(testApiPath);
  const std::string sccHeader = readTextFile(sccHeaderPath);
  const std::string sccSource = readTextFile(sccSourcePath);
  CHECK(cmake.find("src/semantics/StronglyConnectedComponents.cpp") != std::string::npos);
  CHECK(cmake.find("primestruct.semantics.scc") != std::string::npos);
  CHECK(testApi.find("struct StronglyConnectedComponentsTestEdge") != std::string::npos);
  CHECK(testApi.find("struct StronglyConnectedComponentSnapshot") != std::string::npos);
  CHECK(testApi.find("struct StronglyConnectedComponentsSnapshot") != std::string::npos);
  CHECK(testApi.find("computeStronglyConnectedComponentsForTesting") != std::string::npos);
  CHECK(sccHeader.find("struct DirectedGraphEdge") != std::string::npos);
  CHECK(sccHeader.find("struct StronglyConnectedComponent") != std::string::npos);
  CHECK(sccHeader.find("struct StronglyConnectedComponents") != std::string::npos);
  CHECK(sccHeader.find("computeStronglyConnectedComponents") != std::string::npos);
  CHECK(sccSource.find("class TarjanSccBuilder") != std::string::npos);
  CHECK(sccSource.find("normalizeComponentOrder") != std::string::npos);
  CHECK(sccSource.find("computeStronglyConnectedComponentsForTesting") != std::string::npos);
}

TEST_CASE("condensation dag utility is wired through semantics testing api") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path cmakePath = cwd / "CMakeLists.txt";
  std::filesystem::path testApiPath = cwd / "include" / "primec" / "testing" / "SemanticsValidationHelpers.h";
  std::filesystem::path dagHeaderPath = cwd / "src" / "semantics" / "CondensationDag.h";
  std::filesystem::path dagSourcePath = cwd / "src" / "semantics" / "CondensationDag.cpp";
  if (!std::filesystem::exists(cmakePath)) {
    cmakePath = cwd.parent_path() / "CMakeLists.txt";
    testApiPath = cwd.parent_path() / "include" / "primec" / "testing" / "SemanticsValidationHelpers.h";
    dagHeaderPath = cwd.parent_path() / "src" / "semantics" / "CondensationDag.h";
    dagSourcePath = cwd.parent_path() / "src" / "semantics" / "CondensationDag.cpp";
  }
  REQUIRE(std::filesystem::exists(cmakePath));
  REQUIRE(std::filesystem::exists(testApiPath));
  REQUIRE(std::filesystem::exists(dagHeaderPath));
  REQUIRE(std::filesystem::exists(dagSourcePath));

  const std::string cmake = readTextFile(cmakePath);
  const std::string testApi = readTextFile(testApiPath);
  const std::string dagHeader = readTextFile(dagHeaderPath);
  const std::string dagSource = readTextFile(dagSourcePath);
  CHECK(cmake.find("src/semantics/CondensationDag.cpp") != std::string::npos);
  CHECK(cmake.find("primestruct.semantics.condensation_dag") != std::string::npos);
  CHECK(testApi.find("struct CondensationDagNodeSnapshot") != std::string::npos);
  CHECK(testApi.find("struct CondensationDagEdgeSnapshot") != std::string::npos);
  CHECK(testApi.find("struct CondensationDagSnapshot") != std::string::npos);
  CHECK(testApi.find("computeCondensationDagForTesting") != std::string::npos);
  CHECK(dagHeader.find("struct CondensationDagNode") != std::string::npos);
  CHECK(dagHeader.find("struct CondensationDagEdge") != std::string::npos);
  CHECK(dagHeader.find("struct CondensationDag") != std::string::npos);
  CHECK(dagHeader.find("buildCondensationDag") != std::string::npos);
  CHECK(dagHeader.find("computeCondensationDag") != std::string::npos);
  CHECK(dagSource.find("computeStronglyConnectedComponents(nodeCount, edges)") != std::string::npos);
  CHECK(dagSource.find("collectCondensationEdgePairs") != std::string::npos);
  CHECK(dagSource.find("buildTopologicalComponentOrder") != std::string::npos);
  CHECK(dagSource.find("std::priority_queue<uint32_t, std::vector<uint32_t>, std::greater<uint32_t>>") !=
        std::string::npos);
}

TEST_CASE("main routes glsl and spirv through ir backends without legacy fallback branches") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path mainPath = cwd / "src" / "main.cpp";
  if (!std::filesystem::exists(mainPath)) {
    mainPath = cwd.parent_path() / "src" / "main.cpp";
  }
  REQUIRE(std::filesystem::exists(mainPath));

  const std::string source = readTextFile(mainPath);
  CHECK(source.find("resolveIrBackendEmitKind(options.emitKind)") != std::string::npos);
  CHECK(source.find("findIrBackend(options.emitKind)") == std::string::npos);
  CHECK(source.find("if (options.emitKind == \"glsl\")") == std::string::npos);
  CHECK(source.find("if (options.emitKind == \"spirv\")") == std::string::npos);
  CHECK(source.find("if (irBackend == nullptr && options.emitKind == \"glsl\")") == std::string::npos);
  CHECK(source.find("if (irBackend == nullptr && options.emitKind == \"spirv\")") == std::string::npos);
  CHECK(source.find("if (irFailure.stage != IrBackendRunFailureStage::Emit)") == std::string::npos);
}

TEST_CASE("backend boundary ADR is present and referenced from design doc") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path adrPath = cwd / "docs" / "adr" / "0001-backend-ir-boundary.md";
  std::filesystem::path designPath = cwd / "docs" / "PrimeStruct.md";
  if (!std::filesystem::exists(adrPath)) {
    adrPath = cwd.parent_path() / "docs" / "adr" / "0001-backend-ir-boundary.md";
    designPath = cwd.parent_path() / "docs" / "PrimeStruct.md";
  }

  REQUIRE(std::filesystem::exists(adrPath));
  REQUIRE(std::filesystem::exists(designPath));

  const std::string adr = readTextFile(adrPath);
  CHECK(adr.find("All code generation backends must consume canonical `IrModule` only.") != std::string::npos);
  CHECK(adr.find("AST-direct backend emission paths are not allowed") != std::string::npos);
  CHECK(adr.find("Follow-up status (2026-03-11)") != std::string::npos);
  CHECK(adr.find("production `primec --emit` aliases") != std::string::npos);

  const std::string design = readTextFile(designPath);
  CHECK(design.find("docs/adr/0001-backend-ir-boundary.md") != std::string::npos);
  CHECK(design.find("including production aliases (`cpp`, `exe`, `glsl`, `spirv`)") != std::string::npos);
}

TEST_CASE("cmake splits primec library into subsystem targets") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path cmakePath = cwd / "CMakeLists.txt";
  if (!std::filesystem::exists(cmakePath)) {
    cmakePath = cwd.parent_path() / "CMakeLists.txt";
  }
  REQUIRE(std::filesystem::exists(cmakePath));

  const std::string cmake = readTextFile(cmakePath);
  CHECK(cmake.find("set(PRIMESTRUCT_SUPPORT_SOURCES") != std::string::npos);
  CHECK(cmake.find("set(PRIMESTRUCT_FRONTEND_SOURCES") != std::string::npos);
  CHECK(cmake.find("set(PRIMESTRUCT_IR_SOURCES") != std::string::npos);
  CHECK(cmake.find("set(PRIMESTRUCT_CODEGEN_SOURCES") != std::string::npos);
  CHECK(cmake.find("set(PRIMESTRUCT_RUNTIME_SOURCES") != std::string::npos);
  CHECK(cmake.find("set(PRIMESTRUCT_BACKEND_REGISTRY_SOURCES") != std::string::npos);
  CHECK(cmake.find("src/IrBackendProfiles.cpp") != std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidateConvertConstructors.cpp") != std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidateExperimentalGfxConstructors.cpp") != std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidateReflectionGeneratedHelpersCloneDebug.cpp") !=
        std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidateReflectionGeneratedHelpersCompare.cpp") !=
        std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidateReflectionGeneratedHelpers.cpp") != std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidateReflectionGeneratedHelpersSerialization.cpp") !=
        std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidateReflectionGeneratedHelpersState.cpp") !=
        std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidateReflectionGeneratedHelpersValidate.cpp") !=
        std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidateReflectionMetadata.cpp") != std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidateTransforms.cpp") != std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorExprBodyArguments.cpp") != std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorExprBlock.cpp") != std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorExprCollectionAccess.cpp") != std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorExprCollectionAccessValidation.cpp") !=
        std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorExprCollectionDispatchSetup.cpp") !=
        std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorExprCollectionAccessSetup.cpp") !=
        std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorExprDirectCollectionFallbacks.cpp") !=
        std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorExprCollectionCountCapacity.cpp") !=
        std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorExprCountCapacityMapBuiltins.cpp") !=
        std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorExprLateCollectionAccessFallbacks.cpp") !=
        std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorExprLateFallbackBuiltins.cpp") !=
        std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorExprCollectionPredicates.cpp") !=
        std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorExprCollectionLiterals.cpp") !=
        std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorExprControlFlow.cpp") != std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorExprFieldResolution.cpp") != std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorExprGpuBuffer.cpp") != std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorExprLateBuiltins.cpp") != std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorExprLateCallCompatibility.cpp") !=
        std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorExprLateMapAccessBuiltins.cpp") !=
        std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorExprLateMapSoaBuiltins.cpp") !=
        std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorExprLambda.cpp") != std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorExprMapSoaBuiltins.cpp") != std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorExprMethodResolution.cpp") != std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorExprMutationBorrows.cpp") !=
        std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorExprNamedArgumentBuiltins.cpp") !=
        std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorExprNumeric.cpp") != std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorExprPointerLike.cpp") != std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorExprResolvedCallArguments.cpp") !=
        std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorExprReferenceEscapes.cpp") !=
        std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorExprResultFile.cpp") != std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorExprScalarPointerMemory.cpp") !=
        std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorExprStructConstructors.cpp") !=
        std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorExprTry.cpp") != std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorExprVectorHelpers.cpp") != std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorInferCollectionCountCapacity.cpp") != std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorInferCollectionDirectCountCapacity.cpp") != std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorInferCollectionDispatch.cpp") != std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorInferCollections.cpp") != std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorInferControlFlow.cpp") != std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorInferDefinition.cpp") != std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorPassesEffects.cpp") != std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorPassesDiagnostics.cpp") != std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorStatementBindings.cpp") != std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorStatementBuiltins.cpp") != std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorStatementControlFlow.cpp") != std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidatorStatementVectorHelpers.cpp") != std::string::npos);
  CHECK(cmake.find("src/ir_lowerer/IrLowererAccessTargetResolution.cpp") != std::string::npos);
  CHECK(cmake.find("src/ir_lowerer/IrLowererAccessLoadHelpers.cpp") != std::string::npos);
  CHECK(cmake.find("src/ir_lowerer/IrLowererIndexedAccessEmit.cpp") != std::string::npos);
  CHECK(cmake.find("src/ir_lowerer/IrLowererCallResolution.cpp") != std::string::npos);
  CHECK(cmake.find("src/ir_lowerer/IrLowererInlineNativeCallDispatch.cpp") != std::string::npos);
  CHECK(cmake.find("src/ir_lowerer/IrLowererNativeTailDispatch.cpp") != std::string::npos);
  CHECK(cmake.find("src/VmHeapHelpers.cpp") != std::string::npos);
  CHECK(cmake.find("add_library(primec_support_lib") != std::string::npos);
  CHECK(cmake.find("add_library(primec_frontend_lib") != std::string::npos);
  CHECK(cmake.find("add_library(primec_ir_lib") != std::string::npos);
  CHECK(cmake.find("add_library(primec_backend_emitters_lib") != std::string::npos);
  CHECK(cmake.find("add_library(primec_codegen_lib INTERFACE)") != std::string::npos);
  CHECK(cmake.find("add_library(primec_runtime_lib") != std::string::npos);
  CHECK(cmake.find("add_library(primec_backend_registry_lib") != std::string::npos);
  CHECK(cmake.find("add_library(primec_backend_lib INTERFACE)") != std::string::npos);
  CHECK(cmake.find("add_library(primec_lib INTERFACE)") != std::string::npos);
  CHECK(cmake.find("target_link_libraries(primec_frontend_lib PUBLIC primec_support_lib)") != std::string::npos);
  CHECK(cmake.find("target_link_libraries(primec_runtime_lib PUBLIC primec_frontend_lib primec_support_lib)") !=
        std::string::npos);
  CHECK(cmake.find("target_link_libraries(primec_backend_emitters_lib PUBLIC primec_frontend_lib primec_support_lib)") !=
        std::string::npos);
  CHECK(cmake.find("target_link_libraries(primec_codegen_lib INTERFACE primec_backend_emitters_lib)") !=
        std::string::npos);
  CHECK(cmake.find("target_link_libraries(primec_ir_lib PUBLIC primec_frontend_lib primec_support_lib)") !=
        std::string::npos);
  CHECK(cmake.find("target_link_libraries(primec_backend_registry_lib PRIVATE primec_backend_emitters_lib primec_runtime_lib)") !=
        std::string::npos);
  CHECK(cmake.find("target_link_libraries(primec_backend_registry_lib PUBLIC primec_backend_emitters_lib primec_runtime_lib)") ==
        std::string::npos);
  CHECK(cmake.find("target_link_libraries(primec_backend_registry_lib PRIVATE primec_codegen_lib primec_runtime_lib)") ==
        std::string::npos);
  CHECK(cmake.find("target_link_libraries(primec_backend_registry_lib PUBLIC primec_codegen_lib primec_runtime_lib)") ==
        std::string::npos);
  CHECK(cmake.find("target_link_libraries(primec_backend_lib INTERFACE primec_ir_lib primec_backend_registry_lib)") !=
        std::string::npos);
  CHECK(cmake.find("target_link_libraries(primec_lib INTERFACE primec_backend_lib primec_frontend_lib primec_support_lib)") !=
        std::string::npos);
  CHECK(cmake.find("target_link_libraries(primec PRIVATE primec_ir_lib primec_backend_registry_lib)") !=
        std::string::npos);
  CHECK(cmake.find("target_link_libraries(primec PRIVATE primec_backend_lib)") == std::string::npos);
  CHECK(cmake.find("target_link_libraries(primec PRIVATE primec_lib)") == std::string::npos);
  CHECK(cmake.find("target_link_libraries(primevm PRIVATE primec_ir_lib primec_runtime_lib)") != std::string::npos);
  CHECK(cmake.find("add_executable(PrimeStruct_misc_tests") != std::string::npos);
  CHECK(cmake.find("target_link_libraries(PrimeStruct_misc_tests PRIVATE primec_ir_lib)") != std::string::npos);
  CHECK(cmake.find("set(PrimeStructMiscTestSuites") != std::string::npos);
  CHECK(cmake.find("COMMAND $<TARGET_FILE:PrimeStruct_misc_tests> \"--test-suite=${suite}\"") !=
        std::string::npos);
  CHECK(cmake.find("add_executable(PrimeStruct_backend_tests") != std::string::npos);
  CHECK(cmake.find("target_link_libraries(PrimeStruct_backend_tests PRIVATE primec_ir_lib primec_backend_registry_lib)") !=
        std::string::npos);
  CHECK(cmake.find("target_link_libraries(PrimeStruct_backend_tests PRIVATE primec_backend_lib)") ==
        std::string::npos);
  CHECK(cmake.find("set(PrimeStructBackendTestSuites") != std::string::npos);
  CHECK(cmake.find("COMMAND $<TARGET_FILE:PrimeStruct_backend_tests> \"--test-suite=${suite}\"") !=
        std::string::npos);
  CHECK(cmake.find("add_executable(PrimeStruct_semantics_tests") != std::string::npos);
  CHECK(cmake.find("target_link_libraries(PrimeStruct_semantics_tests PRIVATE primec_ir_lib)") != std::string::npos);
  CHECK(cmake.find("set(PrimeStructSemanticsTestSuites") != std::string::npos);
  CHECK(cmake.find("COMMAND $<TARGET_FILE:PrimeStruct_semantics_tests> \"--test-suite=${suite}\"") !=
        std::string::npos);
  CHECK(cmake.find("add_executable(PrimeStruct_text_filter_tests") != std::string::npos);
  CHECK(cmake.find("target_link_libraries(PrimeStruct_text_filter_tests PRIVATE primec_frontend_lib)") !=
        std::string::npos);
  CHECK(cmake.find("set(PrimeStructTextFilterTestSuites") != std::string::npos);
  CHECK(cmake.find("COMMAND $<TARGET_FILE:PrimeStruct_text_filter_tests> \"--test-suite=${suite}\"") !=
        std::string::npos);
  CHECK(cmake.find("add_executable(PrimeStruct_parser_tests") != std::string::npos);
  CHECK(cmake.find("target_link_libraries(PrimeStruct_parser_tests PRIVATE primec_frontend_lib)") !=
        std::string::npos);
  CHECK(cmake.find("set(PrimeStructParserTestSuites") != std::string::npos);
  CHECK(cmake.find("COMMAND $<TARGET_FILE:PrimeStruct_parser_tests> \"--test-suite=${suite}\"") !=
        std::string::npos);
  CHECK(cmake.find("find_package(Python3 REQUIRED COMPONENTS Interpreter)") != std::string::npos);
  CHECK(cmake.find("NAME PrimeStruct_include_layers") != std::string::npos);
  CHECK(cmake.find("scripts/check_include_layers.py") != std::string::npos);
  CHECK(cmake.find("scripts/include_layer_allowlist.txt") != std::string::npos);
  CHECK(cmake.find("PrimeStruct_misc_suite_registration") != std::string::npos);
  CHECK(cmake.find("PrimeStruct_backend_suite_registration") != std::string::npos);
  CHECK(cmake.find("PrimeStruct_parser_suite_registration") != std::string::npos);
  CHECK(cmake.find("PrimeStruct_text_filter_suite_registration") != std::string::npos);
  CHECK(cmake.find("target_link_libraries(PrimeStruct_tests PRIVATE primec_lib)") == std::string::npos);
}

TEST_CASE("include layer guardrail baseline tracks existing private test headers") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path scriptPath = cwd / "scripts" / "check_include_layers.py";
  std::filesystem::path allowlistPath = cwd / "scripts" / "include_layer_allowlist.txt";
  std::filesystem::path emitterTestApiPath = cwd / "include" / "primec" / "testing" / "EmitterHelpers.h";
  std::filesystem::path irLowererTestApiPath = cwd / "include" / "primec" / "testing" / "IrLowererHelpers.h";
  std::filesystem::path parserTestApiPath = cwd / "include" / "primec" / "testing" / "ParserHelpers.h";
  std::filesystem::path semanticsTestApiPath = cwd / "include" / "primec" / "testing" / "SemanticsValidationHelpers.h";
  std::filesystem::path textFilterTestApiPath = cwd / "include" / "primec" / "testing" / "TextFilterHelpers.h";
  std::filesystem::path parserHelperTestPath = cwd / "tests" / "unit" / "test_parser_basic_parser_helpers.h";
  std::filesystem::path textFilterHelperTestPath = cwd / "tests" / "unit" / "test_text_filter_helpers.cpp";
  std::filesystem::path compileRunTestPath = cwd / "tests" / "unit" / "test_compile_run.cpp";
  std::filesystem::path irPipelineTestPath = cwd / "tests" / "unit" / "test_ir_pipeline.cpp";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = cwd.parent_path() / "scripts" / "check_include_layers.py";
    allowlistPath = cwd.parent_path() / "scripts" / "include_layer_allowlist.txt";
    emitterTestApiPath = cwd.parent_path() / "include" / "primec" / "testing" / "EmitterHelpers.h";
    irLowererTestApiPath = cwd.parent_path() / "include" / "primec" / "testing" / "IrLowererHelpers.h";
    parserTestApiPath = cwd.parent_path() / "include" / "primec" / "testing" / "ParserHelpers.h";
    semanticsTestApiPath = cwd.parent_path() / "include" / "primec" / "testing" / "SemanticsValidationHelpers.h";
    textFilterTestApiPath = cwd.parent_path() / "include" / "primec" / "testing" / "TextFilterHelpers.h";
    parserHelperTestPath = cwd.parent_path() / "tests" / "unit" / "test_parser_basic_parser_helpers.h";
    textFilterHelperTestPath = cwd.parent_path() / "tests" / "unit" / "test_text_filter_helpers.cpp";
    compileRunTestPath = cwd.parent_path() / "tests" / "unit" / "test_compile_run.cpp";
    irPipelineTestPath = cwd.parent_path() / "tests" / "unit" / "test_ir_pipeline.cpp";
  }
  REQUIRE(std::filesystem::exists(scriptPath));
  REQUIRE(std::filesystem::exists(allowlistPath));
  REQUIRE(std::filesystem::exists(emitterTestApiPath));
  REQUIRE(std::filesystem::exists(irLowererTestApiPath));
  REQUIRE(std::filesystem::exists(parserTestApiPath));
  REQUIRE(std::filesystem::exists(semanticsTestApiPath));
  REQUIRE(std::filesystem::exists(textFilterTestApiPath));
  REQUIRE(std::filesystem::exists(parserHelperTestPath));
  REQUIRE(std::filesystem::exists(textFilterHelperTestPath));
  REQUIRE(std::filesystem::exists(compileRunTestPath));
  REQUIRE(std::filesystem::exists(irPipelineTestPath));

  const std::string script = readTextFile(scriptPath);
  CHECK(script.find("public headers must not include private src headers") != std::string::npos);
  CHECK(script.find("production sources must not include test headers") != std::string::npos);
  CHECK(script.find("direct tests -> src include is not allowlisted") != std::string::npos);
  CHECK(script.find("stale allowlist entry should be removed") != std::string::npos);

  const std::string allowlist = readTextFile(allowlistPath);
  CHECK(allowlist.find("tests/unit/test_ir_pipeline.cpp -> src/emitter/") == std::string::npos);
  CHECK(allowlist.find("tests/unit/test_ir_pipeline.cpp -> src/ir_lowerer/") == std::string::npos);
  CHECK(allowlist.find("tests/unit/test_ir_pipeline.cpp -> src/semantics/SemanticsValidatorExprCaptureSplitStep.h") ==
        std::string::npos);
  CHECK(allowlist.find("tests/unit/test_ir_pipeline.cpp -> src/semantics/SemanticsValidatorStatementLoopCountStep.h") ==
        std::string::npos);
  CHECK(allowlist.find("tests/unit/test_compile_run.cpp -> src/emitter/EmitterHelpers.h") == std::string::npos);
  CHECK(allowlist.find("tests/unit/test_parser_basic_parser_helpers.h -> src/parser/ParserHelpers.h") ==
        std::string::npos);
  CHECK(allowlist.find("tests/unit/test_text_filter_helpers.cpp -> src/text_filter/TextFilterHelpers.h") ==
        std::string::npos);

  const std::string emitterTestApi = readTextFile(emitterTestApiPath);
  CHECK(emitterTestApi.find("namespace primec::emitter") != std::string::npos);
  CHECK(emitterTestApi.find("bool runEmitterEmitSetupMathImport(const Program &program);") != std::string::npos);
  CHECK(emitterTestApi.find("std::optional<EmitterLifecycleHelperMatch> runEmitterEmitSetupLifecycleHelperMatchStep") !=
        std::string::npos);
  CHECK(emitterTestApi.find("EmitterExprControlIfBranchBodyEmitResult") != std::string::npos);
  CHECK(emitterTestApi.find("EmitterExprControlIfTernaryFallbackStepResult") != std::string::npos);

  const std::string irLowererTestApi = readTextFile(irLowererTestApiPath);
  CHECK(irLowererTestApi.find("namespace primec::ir_lowerer") != std::string::npos);
  CHECK(irLowererTestApi.find("struct LocalInfo") != std::string::npos);
  CHECK(irLowererTestApi.find("struct LowerInferenceSetupBootstrapState") != std::string::npos);
  CHECK(irLowererTestApi.find("struct BufferInitInfo") != std::string::npos);
  CHECK(irLowererTestApi.find("enum class StringCallEmitResult") != std::string::npos);
  CHECK(irLowererTestApi.find("struct UninitializedStorageAccessInfo") != std::string::npos);

  const std::string parserTestApi = readTextFile(parserTestApiPath);
  CHECK(parserTestApi.find("namespace primec::parser") != std::string::npos);
  CHECK(parserTestApi.find("bool isBuiltinName(const std::string &name, bool allowMathBare);") !=
        std::string::npos);

  const std::string textFilterTestApi = readTextFile(textFilterTestApiPath);
  CHECK(textFilterTestApi.find("namespace primec::text_filter") != std::string::npos);
  CHECK(textFilterTestApi.find("bool isSeparator(char c);") != std::string::npos);
  CHECK(textFilterTestApi.find("std::string maybeAppendUtf8(const std::string &token);") != std::string::npos);

  const std::string semanticsTestApi = readTextFile(semanticsTestApiPath);
  CHECK(semanticsTestApi.find("namespace primec::semantics") != std::string::npos);
  CHECK(semanticsTestApi.find("std::vector<std::string> runSemanticsValidatorExprCaptureSplitStep") !=
        std::string::npos);
  CHECK(semanticsTestApi.find("runSemanticsValidatorStatementKnownIterationCountStep") != std::string::npos);
  CHECK(semanticsTestApi.find("runSemanticsValidatorStatementCanIterateMoreThanOnceStep") != std::string::npos);
  CHECK(semanticsTestApi.find("runSemanticsValidatorStatementIsNegativeIntegerLiteralStep") != std::string::npos);

  const std::string parserHelperTest = readTextFile(parserHelperTestPath);
  CHECK(parserHelperTest.find("#include \"primec/testing/ParserHelpers.h\"") != std::string::npos);
  CHECK(parserHelperTest.find("#include \"src/parser/ParserHelpers.h\"") == std::string::npos);

  const std::string textFilterHelperTest = readTextFile(textFilterHelperTestPath);
  CHECK(textFilterHelperTest.find("#include \"primec/testing/TextFilterHelpers.h\"") != std::string::npos);
  CHECK(textFilterHelperTest.find("#include \"src/text_filter/TextFilterHelpers.h\"") == std::string::npos);

  const std::string compileRunTest = readTextFile(compileRunTestPath);
  CHECK(compileRunTest.find("#include \"src/emitter/EmitterHelpers.h\"") == std::string::npos);

  const std::string irPipelineTest = readTextFile(irPipelineTestPath);
  CHECK(irPipelineTest.find("#include \"primec/testing/EmitterHelpers.h\"") != std::string::npos);
  CHECK(irPipelineTest.find("#include \"primec/testing/IrLowererHelpers.h\"") != std::string::npos);
  CHECK(irPipelineTest.find("#include \"primec/testing/SemanticsValidationHelpers.h\"") != std::string::npos);
  CHECK(irPipelineTest.find("#include \"src/emitter/") == std::string::npos);
  CHECK(irPipelineTest.find("#include \"src/ir_lowerer/") == std::string::npos);
  CHECK(irPipelineTest.find("#include \"src/semantics/SemanticsValidatorExprCaptureSplitStep.h\"") ==
        std::string::npos);
  CHECK(irPipelineTest.find("#include \"src/semantics/SemanticsValidatorStatementLoopCountStep.h\"") ==
        std::string::npos);
}

TEST_CASE("glsl and spirv ir backends use glsl ir validation target") {
  primec::Options options;

  const primec::IrBackend *glslBackend = primec::findIrBackend("glsl-ir");
  REQUIRE(glslBackend != nullptr);
  CHECK(glslBackend->validationTarget(options) == primec::IrValidationTarget::Glsl);

  const primec::IrBackend *spirvBackend = primec::findIrBackend("spirv-ir");
  REQUIRE(spirvBackend != nullptr);
  CHECK(spirvBackend->validationTarget(options) == primec::IrValidationTarget::Glsl);
}

TEST_CASE("vm ir backend executes module and returns exit code") {
  const primec::IrBackend *backend = primec::findIrBackend("vm");
  REQUIRE(backend != nullptr);
  CHECK_FALSE(backend->requiresOutputPath());

  const primec::IrModule module = makeReturnI32Module(7);
  primec::IrBackendEmitOptions options;
  options.inputPath = "vm_backend_test.prime";
  options.programArgs = {"one", "two"};
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 7);
}

TEST_CASE("vm ir backend reports invalid ir entry index") {
  const primec::IrBackend *backend = primec::findIrBackend("vm");
  REQUIRE(backend != nullptr);

  primec::IrModule invalidModule;
  invalidModule.entryIndex = -1;
  primec::IrBackendEmitOptions options;
  options.inputPath = "vm_backend_invalid.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  CHECK_FALSE(backend->emit(invalidModule, options, result, error));
  CHECK(error.find("invalid IR entry index") != std::string::npos);
}

TEST_CASE("ir serializer backend writes psir magic header") {
  const primec::IrBackend *backend = primec::findIrBackend("ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  const primec::IrModule module = makeReturnI32Module(11);
  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_test.psir";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "ir_backend_test.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::vector<uint8_t> bytes = readBinaryFile(outputPath);
  REQUIRE(bytes.size() >= 4);
  const uint32_t magic = static_cast<uint32_t>(bytes[0]) | (static_cast<uint32_t>(bytes[1]) << 8) |
                         (static_cast<uint32_t>(bytes[2]) << 16) | (static_cast<uint32_t>(bytes[3]) << 24);
  CHECK(magic == 0x50534952u);
}

TEST_CASE("cpp-ir backend writes C++ source") {
  const primec::IrBackend *backend = primec::findIrBackend("cpp-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  const primec::IrModule module = makeReturnI32Module(13);
  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_test.cpp";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "cpp_ir_backend_test.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("static int64_t ps_fn_0") != std::string::npos);
  CHECK(source.find("return static_cast<int>(ps_entry_0(argc, argv));") != std::string::npos);
}

TEST_CASE("cpp-ir backend writes u64 compare source with canonical bool literals") {
  const primec::IrBackend *backend = primec::findIrBackend("cpp-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushI64, static_cast<uint64_t>(9)});
  function.instructions.push_back({primec::IrOpcode::PushI64, static_cast<uint64_t>(7)});
  function.instructions.push_back({primec::IrOpcode::CmpGeU64, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_cmp_u64.cpp";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "cpp_ir_backend_cmp_u64.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("uint64_t right = stack[--sp];") != std::string::npos);
  CHECK(source.find("uint64_t left = stack[--sp];") != std::string::npos);
  CHECK(source.find("stack[sp++] = (left >= right) ? 1u : 0u;") != std::string::npos);
}

TEST_CASE("cpp-ir backend rejects empty non-entry function bodies") {
  const primec::IrBackend *backend = primec::findIrBackend("cpp-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction entry;
  entry.name = "/main";
  entry.instructions.push_back({primec::IrOpcode::ReturnVoid, 0});
  module.functions.push_back(entry);

  primec::IrFunction helper;
  helper.name = "/helper";
  module.functions.push_back(helper);

  primec::IrBackendEmitOptions options;
  options.outputPath = (std::filesystem::current_path() / "primec_tests" / "unused_empty_helper.cpp").string();
  options.inputPath = "cpp_ir_backend_empty_helper.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  CHECK_FALSE(backend->emit(module, options, result, error));
  CHECK(error == "ir-to-cpp failed: IrToCppEmitter function has no instructions at index 1");
}

TEST_CASE("cpp-ir backend reports invalid entry index diagnostics") {
  const primec::IrBackend *backend = primec::findIrBackend("cpp-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 3;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::ReturnVoid, 0});
  module.functions.push_back(function);

  const std::filesystem::path outputPath =
      std::filesystem::current_path() / "primec_tests" / "unused_invalid_entry_index.cpp";
  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "cpp_ir_backend_invalid_entry_index_error.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  CHECK_FALSE(backend->emit(module, options, result, error));
  CHECK(error.find("ir-to-cpp failed:") != std::string::npos);
  CHECK(error.find("invalid IR entry index") != std::string::npos);
}

TEST_CASE("cpp-ir backend reports file write string index diagnostics") {
  const primec::IrBackend *backend = primec::findIrBackend("cpp-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  module.stringTable.push_back("/dev/null");
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::FileOpenWrite, 0});
  function.instructions.push_back({primec::IrOpcode::FileWriteString, 5});
  function.instructions.push_back({primec::IrOpcode::ReturnVoid, 0});
  module.functions.push_back(function);

  const std::filesystem::path outputPath =
      std::filesystem::current_path() / "primec_tests" / "unused_file_write_string_index.cpp";
  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "cpp_ir_backend_file_write_string_index_error.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  CHECK_FALSE(backend->emit(module, options, result, error));
  CHECK(error.find("ir-to-cpp failed:") != std::string::npos);
  CHECK(error.find("string index out of range") != std::string::npos);
}

TEST_CASE("cpp-ir backend reports file open string index diagnostics") {
  const primec::IrBackend *backend = primec::findIrBackend("cpp-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::FileOpenWrite, 5});
  function.instructions.push_back({primec::IrOpcode::ReturnVoid, 0});
  module.functions.push_back(function);

  const std::filesystem::path outputPath =
      std::filesystem::current_path() / "primec_tests" / "unused_file_open_string_index.cpp";
  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "cpp_ir_backend_file_open_string_index_error.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  CHECK_FALSE(backend->emit(module, options, result, error));
  CHECK(error.find("ir-to-cpp failed:") != std::string::npos);
  CHECK(error.find("string index out of range") != std::string::npos);
}

TEST_CASE("cpp-ir backend reports call target range diagnostics") {
  const primec::IrBackend *backend = primec::findIrBackend("cpp-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::Call, 3});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path outputPath =
      std::filesystem::current_path() / "primec_tests" / "unused_call_target_range.cpp";
  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "cpp_ir_backend_call_target_range_error.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  CHECK_FALSE(backend->emit(module, options, result, error));
  CHECK(error.find("ir-to-cpp failed:") != std::string::npos);
  CHECK(error.find("call target out of range") != std::string::npos);
}

TEST_CASE("cpp-ir backend reports jump target range diagnostics") {
  const primec::IrBackend *backend = primec::findIrBackend("cpp-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushI32, 0});
  function.instructions.push_back({primec::IrOpcode::JumpIfZero, 7});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path outputPath =
      std::filesystem::current_path() / "primec_tests" / "unused_jump_target_range.cpp";
  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "cpp_ir_backend_jump_target_range_error.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  CHECK_FALSE(backend->emit(module, options, result, error));
  CHECK(error.find("ir-to-cpp failed:") != std::string::npos);
  CHECK(error.find("jump target out of range") != std::string::npos);
}

TEST_CASE("cpp-ir backend reports unconditional jump target range diagnostics") {
  const primec::IrBackend *backend = primec::findIrBackend("cpp-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::Jump, 7});
  function.instructions.push_back({primec::IrOpcode::ReturnVoid, 0});
  module.functions.push_back(function);

  const std::filesystem::path outputPath =
      std::filesystem::current_path() / "primec_tests" / "unused_unconditional_jump_target_range.cpp";
  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "cpp_ir_backend_unconditional_jump_target_range_error.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  CHECK_FALSE(backend->emit(module, options, result, error));
  CHECK(error.find("ir-to-cpp failed:") != std::string::npos);
  CHECK(error.find("jump target out of range") != std::string::npos);
}

TEST_CASE("cpp-ir backend reports unsupported opcode diagnostics") {
  const primec::IrBackend *backend = primec::findIrBackend("cpp-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({static_cast<primec::IrOpcode>(0), 0});
  function.instructions.push_back({primec::IrOpcode::ReturnVoid, 0});
  module.functions.push_back(function);

  const std::filesystem::path outputPath =
      std::filesystem::current_path() / "primec_tests" / "unused_unsupported_opcode.cpp";
  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "cpp_ir_backend_unsupported_opcode_error.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  CHECK_FALSE(backend->emit(module, options, result, error));
  CHECK(error.find("ir-to-cpp failed:") != std::string::npos);
  CHECK(error.find("unsupported opcode") != std::string::npos);
}

TEST_CASE("cpp-ir backend reports print string index diagnostics") {
  const primec::IrBackend *backend = primec::findIrBackend("cpp-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back(
      {primec::IrOpcode::PrintString, primec::encodePrintStringImm(3, primec::encodePrintFlags(true, false))});
  function.instructions.push_back({primec::IrOpcode::ReturnVoid, 0});
  module.functions.push_back(function);

  const std::filesystem::path outputPath =
      std::filesystem::current_path() / "primec_tests" / "unused_print_string_index.cpp";
  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "cpp_ir_backend_print_string_index_error.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  CHECK_FALSE(backend->emit(module, options, result, error));
  CHECK(error.find("ir-to-cpp failed:") != std::string::npos);
  CHECK(error.find("string index out of range") != std::string::npos);
}

TEST_CASE("cpp-ir backend reports string byte load index diagnostics") {
  const primec::IrBackend *backend = primec::findIrBackend("cpp-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushI32, 0});
  function.instructions.push_back({primec::IrOpcode::LoadStringByte, 3});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path outputPath =
      std::filesystem::current_path() / "primec_tests" / "unused_string_byte_index.cpp";
  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "cpp_ir_backend_string_byte_index_error.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  CHECK_FALSE(backend->emit(module, options, result, error));
  CHECK(error.find("ir-to-cpp failed:") != std::string::npos);
  CHECK(error.find("string index out of range") != std::string::npos);
}

TEST_CASE("cpp-ir backend writes f32 opcode helpers") {
  const primec::IrBackend *backend = primec::findIrBackend("cpp-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushF32, f32ToBits(2.0f)});
  function.instructions.push_back({primec::IrOpcode::PushF32, f32ToBits(0.5f)});
  function.instructions.push_back({primec::IrOpcode::AddF32, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnF32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_f32.cpp";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "cpp_ir_backend_f32.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("#include <cstring>") != std::string::npos);
  CHECK(source.find("static float psBitsToF32(uint64_t raw)") != std::string::npos);
  CHECK(source.find("static uint64_t psF32ToBits(float value)") != std::string::npos);
  CHECK(source.find("float right = psBitsToF32(stack[--sp]);") != std::string::npos);
}

TEST_CASE("cpp-ir backend writes string byte load paths") {
  const primec::IrBackend *backend = primec::findIrBackend("cpp-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  module.stringTable.push_back("abc");
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushI32, static_cast<uint64_t>(1)});
  function.instructions.push_back({primec::IrOpcode::LoadStringByte, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_string_byte.cpp";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "cpp_ir_backend_string_byte.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("if (stringByteIndex >= 3ull)") != std::string::npos);
  CHECK(source.find("ps_string_table[0][stringByteIndex]") != std::string::npos);
}

TEST_CASE("cpp-ir backend writes indirect local pointer paths") {
  const primec::IrBackend *backend = primec::findIrBackend("cpp-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushI32, static_cast<uint64_t>(3)});
  function.instructions.push_back({primec::IrOpcode::StoreLocal, 0});
  function.instructions.push_back({primec::IrOpcode::AddressOfLocal, 0});
  function.instructions.push_back({primec::IrOpcode::PushI32, static_cast<uint64_t>(8)});
  function.instructions.push_back({primec::IrOpcode::StoreIndirect, 0});
  function.instructions.push_back({primec::IrOpcode::AddressOfLocal, 0});
  function.instructions.push_back({primec::IrOpcode::LoadIndirect, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_pointer.cpp";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "cpp_ir_backend_pointer.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("stack[sp++] = static_cast<uint64_t>(0ull * 16ull);") != std::string::npos);
  CHECK(source.find("loadIndirectAddress % 16ull") != std::string::npos);
  CHECK(source.find("storeIndirectAddress % 16ull") != std::string::npos);
}

TEST_CASE("cpp-ir backend emits dup and pop underflow guards") {
  const primec::IrBackend *backend = primec::findIrBackend("cpp-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::Dup, 0});
  function.instructions.push_back({primec::IrOpcode::Pop, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_dup_pop_underflow.cpp";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "cpp_ir_backend_dup_pop_underflow.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("if (sp == 0) {") != std::string::npos);
  CHECK(source.find("IR stack underflow on dup") != std::string::npos);
  CHECK(source.find("IR stack underflow on pop") != std::string::npos);
}

TEST_CASE("cpp-ir backend emits arithmetic print file return underflow guards") {
  const primec::IrBackend *backend = primec::findIrBackend("cpp-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  module.stringTable.push_back("payload");
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::AddI32, 0});
  function.instructions.push_back({primec::IrOpcode::SubI32, 0});
  function.instructions.push_back({primec::IrOpcode::MulI32, 0});
  function.instructions.push_back({primec::IrOpcode::DivI32, 0});
  function.instructions.push_back({primec::IrOpcode::AddI64, 0});
  function.instructions.push_back({primec::IrOpcode::SubI64, 0});
  function.instructions.push_back({primec::IrOpcode::MulI64, 0});
  function.instructions.push_back({primec::IrOpcode::DivI64, 0});
  function.instructions.push_back({primec::IrOpcode::DivU64, 0});
  function.instructions.push_back({primec::IrOpcode::AddF32, 0});
  function.instructions.push_back({primec::IrOpcode::SubF32, 0});
  function.instructions.push_back({primec::IrOpcode::MulF32, 0});
  function.instructions.push_back({primec::IrOpcode::DivF32, 0});
  function.instructions.push_back({primec::IrOpcode::AddF64, 0});
  function.instructions.push_back({primec::IrOpcode::SubF64, 0});
  function.instructions.push_back({primec::IrOpcode::MulF64, 0});
  function.instructions.push_back({primec::IrOpcode::DivF64, 0});
  function.instructions.push_back({primec::IrOpcode::PrintI32, primec::encodePrintFlags(false, false)});
  function.instructions.push_back({primec::IrOpcode::PrintI64, primec::encodePrintFlags(false, false)});
  function.instructions.push_back({primec::IrOpcode::PrintU64, primec::encodePrintFlags(false, false)});
  function.instructions.push_back({primec::IrOpcode::PrintStringDynamic, primec::encodePrintFlags(false, false)});
  function.instructions.push_back({primec::IrOpcode::PrintArgv, primec::encodePrintFlags(false, false)});
  function.instructions.push_back({primec::IrOpcode::PrintArgvUnsafe, primec::encodePrintFlags(false, false)});
  function.instructions.push_back({primec::IrOpcode::FileClose, 0});
  function.instructions.push_back({primec::IrOpcode::FileFlush, 0});
  function.instructions.push_back({primec::IrOpcode::FileWriteI32, 0});
  function.instructions.push_back({primec::IrOpcode::FileWriteI64, 0});
  function.instructions.push_back({primec::IrOpcode::FileWriteU64, 0});
  function.instructions.push_back({primec::IrOpcode::FileWriteString, 0});
  function.instructions.push_back({primec::IrOpcode::FileWriteByte, 0});
  function.instructions.push_back({primec::IrOpcode::FileWriteNewline, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI64, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnF32, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnF64, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnVoid, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_stack_underflow_guards.cpp";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "cpp_ir_backend_stack_underflow_guards.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("IR stack underflow on add") != std::string::npos);
  CHECK(source.find("IR stack underflow on sub") != std::string::npos);
  CHECK(source.find("IR stack underflow on mul") != std::string::npos);
  CHECK(source.find("IR stack underflow on div") != std::string::npos);
  CHECK(source.find("IR stack underflow on float op") != std::string::npos);
  CHECK(source.find("IR stack underflow on print") != std::string::npos);
  CHECK(source.find("IR stack underflow on file close") != std::string::npos);
  CHECK(source.find("IR stack underflow on file flush") != std::string::npos);
  CHECK(source.find("IR stack underflow on file write") != std::string::npos);
  CHECK(source.find("IR stack underflow on return") != std::string::npos);
  CHECK(source.find("if (sp < 2)") != std::string::npos);
  CHECK(source.find("if (sp == 0)") != std::string::npos);
}

TEST_CASE("cpp-ir backend writes file io paths") {
  const primec::IrBackend *backend = primec::findIrBackend("cpp-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  module.stringTable.push_back("/tmp/ir_backend_registry_file_io.txt");
  module.stringTable.push_back("hello");
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::FileOpenWrite, 0});
  function.instructions.push_back({primec::IrOpcode::Dup, 0});
  function.instructions.push_back({primec::IrOpcode::FileWriteString, 1});
  function.instructions.push_back({primec::IrOpcode::Pop, 0});
  function.instructions.push_back({primec::IrOpcode::Dup, 0});
  function.instructions.push_back({primec::IrOpcode::FileFlush, 0});
  function.instructions.push_back({primec::IrOpcode::Pop, 0});
  function.instructions.push_back({primec::IrOpcode::FileClose, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_file_io.cpp";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "cpp_ir_backend_file_io.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("static uint32_t psWriteAll(int fd, const void *data, std::size_t size)") != std::string::npos);
  CHECK(source.find("int fileFd = ::open(ps_string_table[0], fileOpenFlags, 0644);") != std::string::npos);
  CHECK(source.find("int flushRc = ::fsync(flushFd);") != std::string::npos);
  CHECK(source.find("int closeRc = ::close(closeFd);") != std::string::npos);
}

TEST_CASE("cpp-ir backend writes file write u64 path") {
  const primec::IrBackend *backend = primec::findIrBackend("cpp-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  module.stringTable.push_back("/dev/null");
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::FileOpenWrite, 0});
  function.instructions.push_back({primec::IrOpcode::PushI64, static_cast<uint64_t>(123)});
  function.instructions.push_back({primec::IrOpcode::FileWriteU64, 0});
  function.instructions.push_back({primec::IrOpcode::Pop, 0});
  function.instructions.push_back({primec::IrOpcode::FileClose, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_file_write_u64.cpp";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "cpp_ir_backend_file_write_u64.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("uint64_t fileU64Value = stack[--sp];") != std::string::npos);
  CHECK(source.find("int fileU64Fd = static_cast<int>(fileU64Handle & 0xffffffffu);") != std::string::npos);
  CHECK(source.find("std::string fileU64Text = std::to_string(static_cast<unsigned long long>(fileU64Value));") !=
        std::string::npos);
  CHECK(source.find("psWriteAll(fileU64Fd, fileU64Text.data(), fileU64Text.size())") != std::string::npos);
}

TEST_CASE("cpp-ir backend writes file write i32 path") {
  const primec::IrBackend *backend = primec::findIrBackend("cpp-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  module.stringTable.push_back("/dev/null");
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::FileOpenWrite, 0});
  function.instructions.push_back({primec::IrOpcode::PushI32, static_cast<uint64_t>(-17)});
  function.instructions.push_back({primec::IrOpcode::FileWriteI32, 0});
  function.instructions.push_back({primec::IrOpcode::Pop, 0});
  function.instructions.push_back({primec::IrOpcode::FileClose, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_file_write_i32.cpp";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "cpp_ir_backend_file_write_i32.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("int64_t fileI32Value = static_cast<int64_t>(static_cast<int32_t>(stack[--sp]));") !=
        std::string::npos);
  CHECK(source.find("int fileI32Fd = static_cast<int>(fileI32Handle & 0xffffffffu);") != std::string::npos);
  CHECK(source.find("std::string fileI32Text = std::to_string(fileI32Value);") != std::string::npos);
  CHECK(source.find("psWriteAll(fileI32Fd, fileI32Text.data(), fileI32Text.size())") != std::string::npos);
}

TEST_CASE("cpp-ir backend writes file write i64 path") {
  const primec::IrBackend *backend = primec::findIrBackend("cpp-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  module.stringTable.push_back("/dev/null");
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::FileOpenWrite, 0});
  function.instructions.push_back({primec::IrOpcode::PushI64, static_cast<uint64_t>(-19)});
  function.instructions.push_back({primec::IrOpcode::FileWriteI64, 0});
  function.instructions.push_back({primec::IrOpcode::Pop, 0});
  function.instructions.push_back({primec::IrOpcode::FileClose, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_file_write_i64.cpp";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "cpp_ir_backend_file_write_i64.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("int64_t fileI64Value = static_cast<int64_t>(stack[--sp]);") != std::string::npos);
  CHECK(source.find("int fileI64Fd = static_cast<int>(fileI64Handle & 0xffffffffu);") != std::string::npos);
  CHECK(source.find("std::string fileI64Text = std::to_string(fileI64Value);") != std::string::npos);
  CHECK(source.find("psWriteAll(fileI64Fd, fileI64Text.data(), fileI64Text.size())") != std::string::npos);
}

TEST_CASE("cpp-ir backend writes file write byte path") {
  const primec::IrBackend *backend = primec::findIrBackend("cpp-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  module.stringTable.push_back("/dev/null");
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::FileOpenWrite, 0});
  function.instructions.push_back({primec::IrOpcode::PushI32, static_cast<uint64_t>(0x41)});
  function.instructions.push_back({primec::IrOpcode::FileWriteByte, 0});
  function.instructions.push_back({primec::IrOpcode::Pop, 0});
  function.instructions.push_back({primec::IrOpcode::FileClose, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_file_write_byte.cpp";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "cpp_ir_backend_file_write_byte.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("uint8_t fileByteValue = static_cast<uint8_t>(stack[--sp] & 0xffu);") != std::string::npos);
  CHECK(source.find("int fileByteFd = static_cast<int>(fileByteHandle & 0xffffffffu);") != std::string::npos);
  CHECK(source.find("psWriteAll(fileByteFd, &fileByteValue, 1)") != std::string::npos);
}

TEST_CASE("cpp-ir backend writes file write newline path") {
  const primec::IrBackend *backend = primec::findIrBackend("cpp-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  module.stringTable.push_back("/dev/null");
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::FileOpenWrite, 0});
  function.instructions.push_back({primec::IrOpcode::FileWriteNewline, 0});
  function.instructions.push_back({primec::IrOpcode::Pop, 0});
  function.instructions.push_back({primec::IrOpcode::FileClose, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_file_write_newline.cpp";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "cpp_ir_backend_file_write_newline.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("int fileLineFd = static_cast<int>(fileLineHandle & 0xffffffffu);") != std::string::npos);
  CHECK(source.find("char fileLineValue = '\\n';") != std::string::npos);
  CHECK(source.find("psWriteAll(fileLineFd, &fileLineValue, 1)") != std::string::npos);
}

TEST_CASE("cpp-ir backend writes file write string path") {
  const primec::IrBackend *backend = primec::findIrBackend("cpp-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  module.stringTable.push_back("/dev/null");
  module.stringTable.push_back("hello");
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::FileOpenWrite, 0});
  function.instructions.push_back({primec::IrOpcode::FileWriteString, 1});
  function.instructions.push_back({primec::IrOpcode::Pop, 0});
  function.instructions.push_back({primec::IrOpcode::FileClose, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_file_write_string.cpp";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "cpp_ir_backend_file_write_string.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("int fileStringFd = static_cast<int>(fileStringHandle & 0xffffffffu);") != std::string::npos);
  CHECK(source.find("psWriteAll(fileStringFd, ps_string_table[1], 5ull)") != std::string::npos);
}

TEST_CASE("cpp-ir backend writes file read path") {
  const primec::IrBackend *backend = primec::findIrBackend("cpp-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  module.stringTable.push_back("/dev/null");
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::FileOpenRead, 0});
  function.instructions.push_back({primec::IrOpcode::FileClose, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_file_read.cpp";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "cpp_ir_backend_file_read.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("int fileOpenFlags = O_RDONLY;") != std::string::npos);
  CHECK(source.find("int fileFd = ::open(ps_string_table[0], fileOpenFlags, 0644);") != std::string::npos);
  CHECK(source.find("int closeRc = ::close(closeFd);") != std::string::npos);
}

TEST_CASE("cpp-ir backend writes file write path") {
  const primec::IrBackend *backend = primec::findIrBackend("cpp-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  module.stringTable.push_back("/dev/null");
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::FileOpenWrite, 0});
  function.instructions.push_back({primec::IrOpcode::FileClose, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_file_write.cpp";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "cpp_ir_backend_file_write.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("int fileOpenFlags = O_WRONLY | O_CREAT | O_TRUNC;") != std::string::npos);
  CHECK(source.find("int fileFd = ::open(ps_string_table[0], fileOpenFlags, 0644);") != std::string::npos);
  CHECK(source.find("int closeRc = ::close(closeFd);") != std::string::npos);
}

TEST_CASE("cpp-ir backend writes file append path") {
  const primec::IrBackend *backend = primec::findIrBackend("cpp-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  module.stringTable.push_back("/dev/null");
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::FileOpenAppend, 0});
  function.instructions.push_back({primec::IrOpcode::FileClose, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_file_append.cpp";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "cpp_ir_backend_file_append.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("int fileOpenFlags = O_WRONLY | O_CREAT | O_APPEND;") != std::string::npos);
  CHECK(source.find("int fileFd = ::open(ps_string_table[0], fileOpenFlags, 0644);") != std::string::npos);
  CHECK(source.find("int closeRc = ::close(closeFd);") != std::string::npos);
}

TEST_CASE("cpp-ir backend omits file io helpers when unused") {
  const primec::IrBackend *backend = primec::findIrBackend("cpp-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushI32, static_cast<uint64_t>(5)});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_file_io_unused.cpp";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "cpp_ir_backend_file_io_unused.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("#include <cerrno>") == std::string::npos);
  CHECK(source.find("#include <fcntl.h>") == std::string::npos);
  CHECK(source.find("#include <unistd.h>") == std::string::npos);
  CHECK(source.find("static uint32_t psWriteAll(int fd, const void *data, std::size_t size)") ==
        std::string::npos);
}

TEST_CASE("cpp-ir backend writes f64 compare helpers") {
  const primec::IrBackend *backend = primec::findIrBackend("cpp-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushF64, f64ToBits(2.5)});
  function.instructions.push_back({primec::IrOpcode::PushF64, f64ToBits(1.0)});
  function.instructions.push_back({primec::IrOpcode::CmpGtF64, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_f64_cmp.cpp";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "cpp_ir_backend_f64_cmp.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("static double psBitsToF64(uint64_t raw)") != std::string::npos);
  CHECK(source.find("double right = psBitsToF64(stack[--sp]);") != std::string::npos);
}

TEST_CASE("cpp-ir backend writes f64 arithmetic helpers") {
  const primec::IrBackend *backend = primec::findIrBackend("cpp-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushF64, f64ToBits(3.0)});
  function.instructions.push_back({primec::IrOpcode::PushF64, f64ToBits(0.5)});
  function.instructions.push_back({primec::IrOpcode::AddF64, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnF64, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_f64_math.cpp";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "cpp_ir_backend_f64_math.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("static uint64_t psF64ToBits(double value)") != std::string::npos);
  CHECK(source.find("stack[sp++] = psF64ToBits(left + right);") != std::string::npos);
}

TEST_CASE("cpp-ir backend writes f64 conversion helpers") {
  const primec::IrBackend *backend = primec::findIrBackend("cpp-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushI32, static_cast<uint64_t>(7)});
  function.instructions.push_back({primec::IrOpcode::ConvertI32ToF64, 0});
  function.instructions.push_back({primec::IrOpcode::ConvertF64ToF32, 0});
  function.instructions.push_back({primec::IrOpcode::ConvertF32ToF64, 0});
  function.instructions.push_back({primec::IrOpcode::ConvertF64ToI32, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_f64_convert.cpp";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "cpp_ir_backend_f64_convert.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("#include <cmath>") != std::string::npos);
  CHECK(source.find("#include <limits>") != std::string::npos);
  CHECK(source.find("static int32_t psConvertF32ToI32(float value)") != std::string::npos);
  CHECK(source.find("static int32_t psConvertF64ToI32(double value)") != std::string::npos);
  CHECK(source.find("stack[sp++] = psF64ToBits(static_cast<double>(value));") != std::string::npos);
  CHECK(source.find("stack[sp++] = psF32ToBits(static_cast<float>(value));") != std::string::npos);
  CHECK(source.find("int32_t converted = psConvertF64ToI32(value);") != std::string::npos);
}

TEST_CASE("cpp-ir backend omits i32 float conversion clamp helpers when unused") {
  const primec::IrBackend *backend = primec::findIrBackend("cpp-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushI32, 5});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_convert_i32_unused.cpp";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "cpp_ir_backend_convert_i32_unused.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("#include <cmath>") == std::string::npos);
  CHECK(source.find("#include <limits>") != std::string::npos);
  CHECK(source.find("static int32_t psConvertF32ToI32(float value)") == std::string::npos);
  CHECK(source.find("static int32_t psConvertF64ToI32(double value)") == std::string::npos);
}

TEST_CASE("cpp-ir backend writes u64 float conversion clamp helpers") {
  const primec::IrBackend *backend = primec::findIrBackend("cpp-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushF32, f32ToBits(1.5f)});
  function.instructions.push_back({primec::IrOpcode::ConvertF32ToU64, 0});
  function.instructions.push_back({primec::IrOpcode::Pop, 0});
  function.instructions.push_back({primec::IrOpcode::PushF64, f64ToBits(2.5)});
  function.instructions.push_back({primec::IrOpcode::ConvertF64ToU64, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI64, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_f64_convert_u64.cpp";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "cpp_ir_backend_f64_convert_u64.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("#include <cmath>") != std::string::npos);
  CHECK(source.find("#include <limits>") != std::string::npos);
  CHECK(source.find("static uint64_t psConvertF32ToU64(float value)") != std::string::npos);
  CHECK(source.find("static uint64_t psConvertF64ToU64(double value)") != std::string::npos);
  CHECK(source.find("uint64_t converted = psConvertF32ToU64(value);") != std::string::npos);
  CHECK(source.find("uint64_t converted = psConvertF64ToU64(value);") != std::string::npos);
}

TEST_CASE("cpp-ir backend omits u64 float conversion clamp helpers when unused") {
  const primec::IrBackend *backend = primec::findIrBackend("cpp-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushI32, 5});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_convert_u64_unused.cpp";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "cpp_ir_backend_convert_u64_unused.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("#include <cmath>") == std::string::npos);
  CHECK(source.find("#include <limits>") != std::string::npos);
  CHECK(source.find("static uint64_t psConvertF32ToU64(float value)") == std::string::npos);
  CHECK(source.find("static uint64_t psConvertF64ToU64(double value)") == std::string::npos);
}

TEST_CASE("cpp-ir backend writes i64 float conversion clamp helpers") {
  const primec::IrBackend *backend = primec::findIrBackend("cpp-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushF32, f32ToBits(1.5f)});
  function.instructions.push_back({primec::IrOpcode::ConvertF32ToI64, 0});
  function.instructions.push_back({primec::IrOpcode::Pop, 0});
  function.instructions.push_back({primec::IrOpcode::PushF64, f64ToBits(2.5)});
  function.instructions.push_back({primec::IrOpcode::ConvertF64ToI64, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI64, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_f64_convert_i64.cpp";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "cpp_ir_backend_f64_convert_i64.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("#include <cmath>") != std::string::npos);
  CHECK(source.find("#include <limits>") != std::string::npos);
  CHECK(source.find("static int64_t psConvertF32ToI64(float value)") != std::string::npos);
  CHECK(source.find("static int64_t psConvertF64ToI64(double value)") != std::string::npos);
  CHECK(source.find("int64_t converted = psConvertF32ToI64(value);") != std::string::npos);
  CHECK(source.find("int64_t converted = psConvertF64ToI64(value);") != std::string::npos);
}

TEST_CASE("cpp-ir backend omits i64 float conversion clamp helpers when unused") {
  const primec::IrBackend *backend = primec::findIrBackend("cpp-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushI32, 9});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_convert_i64_unused.cpp";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "cpp_ir_backend_convert_i64_unused.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("#include <cmath>") == std::string::npos);
  CHECK(source.find("#include <limits>") != std::string::npos);
  CHECK(source.find("static int64_t psConvertF32ToI64(float value)") == std::string::npos);
  CHECK(source.find("static int64_t psConvertF64ToI64(double value)") == std::string::npos);
}

TEST_CASE("glsl-ir backend writes GLSL source") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  const primec::IrModule module = makeReturnI32Module(17);
  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_test.glsl";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "glsl_ir_backend_test.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("#version 450") != std::string::npos);
  CHECK(source.find("ps_output.value = ps_entry_0(stack, sp);") != std::string::npos);
}

TEST_CASE("glsl-ir backend writes loadstringbyte source") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  module.stringTable.push_back("AB");
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushI32, 1});
  function.instructions.push_back({primec::IrOpcode::LoadStringByte, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_string_byte.glsl";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "glsl_ir_backend_string_byte.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("switch (stringByteIndex)") != std::string::npos);
  CHECK(source.find("case 0u: stringByte = 65; break;") != std::string::npos);
  CHECK(source.find("case 1u: stringByte = 66; break;") != std::string::npos);
}

TEST_CASE("glsl-ir backend writes pushi64 source for i32-range literals") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushI64, static_cast<uint64_t>(static_cast<int64_t>(31))});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_push_i64.glsl";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "glsl_ir_backend_push_i64.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("stack[sp++] = 31;") != std::string::npos);
}

TEST_CASE("glsl-ir backend writes i64 compare source for narrowed values") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushI64, static_cast<uint64_t>(static_cast<int64_t>(4))});
  function.instructions.push_back({primec::IrOpcode::PushI64, static_cast<uint64_t>(static_cast<int64_t>(4))});
  function.instructions.push_back({primec::IrOpcode::CmpEqI64, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI64, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_cmp_i64.glsl";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "glsl_ir_backend_cmp_i64.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("stack[sp++] = (left == right) ? 1 : 0;") != std::string::npos);
  CHECK(source.find("return stack[--sp];") != std::string::npos);
}

TEST_CASE("glsl-ir backend writes i64 arithmetic source for narrowed values") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushI64, static_cast<uint64_t>(static_cast<int64_t>(8))});
  function.instructions.push_back({primec::IrOpcode::PushI64, static_cast<uint64_t>(static_cast<int64_t>(3))});
  function.instructions.push_back({primec::IrOpcode::AddI64, 0});
  function.instructions.push_back({primec::IrOpcode::NegI64, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI64, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_arith_i64.glsl";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "glsl_ir_backend_arith_i64.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("stack[sp++] = left + right;") != std::string::npos);
  CHECK(source.find("stack[sp - 1] = -stack[sp - 1];") != std::string::npos);
}

TEST_CASE("glsl-ir backend writes u64 compare source for narrowed values") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushI64, static_cast<uint64_t>(static_cast<int64_t>(2))});
  function.instructions.push_back({primec::IrOpcode::PushI64, static_cast<uint64_t>(static_cast<int64_t>(9))});
  function.instructions.push_back({primec::IrOpcode::CmpLtU64, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_cmp_u64.glsl";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "glsl_ir_backend_cmp_u64.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("uint right = uint(stack[--sp]);") != std::string::npos);
  CHECK(source.find("uint left = uint(stack[--sp]);") != std::string::npos);
  CHECK(source.find("stack[sp++] = (left < right) ? 1 : 0;") != std::string::npos);
}

TEST_CASE("glsl-ir backend writes u64 division source for narrowed values") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushI64, static_cast<uint64_t>(static_cast<int64_t>(10))});
  function.instructions.push_back({primec::IrOpcode::PushI64, static_cast<uint64_t>(static_cast<int64_t>(3))});
  function.instructions.push_back({primec::IrOpcode::DivU64, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_div_u64.glsl";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "glsl_ir_backend_div_u64.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("uint right = uint(stack[--sp]);") != std::string::npos);
  CHECK(source.find("uint left = uint(stack[--sp]);") != std::string::npos);
  CHECK(source.find("stack[sp++] = int(left / right);") != std::string::npos);
}

TEST_CASE("glsl-ir backend writes i64/u64 to f32 conversion source") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushI64, static_cast<uint64_t>(static_cast<int64_t>(4))});
  function.instructions.push_back({primec::IrOpcode::ConvertI64ToF32, 0});
  function.instructions.push_back({primec::IrOpcode::Pop, 0});
  function.instructions.push_back({primec::IrOpcode::PushI64, static_cast<uint64_t>(static_cast<int64_t>(5))});
  function.instructions.push_back({primec::IrOpcode::ConvertU64ToF32, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_convert_i64_u64_f32.glsl";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "glsl_ir_backend_convert_i64_u64_f32.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("stack[sp++] = floatBitsToInt(float(stack[--sp]));") != std::string::npos);
  CHECK(source.find("stack[sp++] = floatBitsToInt(float(uint(stack[--sp])));") != std::string::npos);
}

TEST_CASE("glsl-ir backend writes f32 to i64 conversion source") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushF32, 0x41200000u});
  function.instructions.push_back({primec::IrOpcode::ConvertF32ToI64, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI64, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_convert_f32_i64.glsl";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "glsl_ir_backend_convert_f32_i64.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("float value = intBitsToFloat(stack[--sp]);") != std::string::npos);
  CHECK(source.find("converted = int(value);") != std::string::npos);
}

TEST_CASE("glsl-ir backend writes f32 to u64 conversion source") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushF32, 0x41200000u});
  function.instructions.push_back({primec::IrOpcode::ConvertF32ToU64, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI64, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_convert_f32_u64.glsl";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "glsl_ir_backend_convert_f32_u64.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("uint converted = 0u;") != std::string::npos);
  CHECK(source.find("converted = uint(int(value));") != std::string::npos);
  CHECK(source.find("stack[sp++] = int(converted);") != std::string::npos);
}

TEST_CASE("glsl-ir backend writes f32/f64 passthrough conversion source") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushF32, 0x3f800000u});
  function.instructions.push_back({primec::IrOpcode::ConvertF32ToF64, 0});
  function.instructions.push_back({primec::IrOpcode::ConvertF64ToF32, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_convert_f32_f64.glsl";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "glsl_ir_backend_convert_f32_f64.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("// Narrowed GLSL path keeps f32/f64 conversion as bit-preserving passthrough.") !=
        std::string::npos);
  CHECK(source.find("// Narrowed GLSL path keeps f64/f32 conversion as bit-preserving passthrough.") !=
        std::string::npos);
}

TEST_CASE("glsl-ir backend writes i32/f64 narrowed conversion source") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushI32, 11});
  function.instructions.push_back({primec::IrOpcode::ConvertI32ToF64, 0});
  function.instructions.push_back({primec::IrOpcode::ConvertF64ToI32, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_convert_i32_f64.glsl";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "glsl_ir_backend_convert_i32_f64.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("// Narrowed GLSL path lowers i32/f64 conversion through f32 payloads.") != std::string::npos);
  CHECK(source.find("// Narrowed GLSL path lowers f64/i32 conversion through f32 payloads.") != std::string::npos);
}

TEST_CASE("glsl-ir backend writes i64/u64 to f64 narrowed conversion source") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushI64, static_cast<uint64_t>(static_cast<int64_t>(8))});
  function.instructions.push_back({primec::IrOpcode::ConvertI64ToF64, 0});
  function.instructions.push_back({primec::IrOpcode::Pop, 0});
  function.instructions.push_back({primec::IrOpcode::PushI64, static_cast<uint64_t>(static_cast<int64_t>(9))});
  function.instructions.push_back({primec::IrOpcode::ConvertU64ToF64, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_convert_i64_u64_f64.glsl";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "glsl_ir_backend_convert_i64_u64_f64.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("// Narrowed GLSL path lowers i64/f64 conversion through f32 payloads.") != std::string::npos);
  CHECK(source.find("// Narrowed GLSL path lowers u64/f64 conversion through f32 payloads.") != std::string::npos);
}

TEST_CASE("glsl-ir backend writes f64 to i64/u64 narrowed conversion source") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushF32, 0x40c00000u});
  function.instructions.push_back({primec::IrOpcode::ConvertF32ToF64, 0});
  function.instructions.push_back({primec::IrOpcode::ConvertF64ToI64, 0});
  function.instructions.push_back({primec::IrOpcode::Pop, 0});
  function.instructions.push_back({primec::IrOpcode::PushF32, 0x41100000u});
  function.instructions.push_back({primec::IrOpcode::ConvertF32ToF64, 0});
  function.instructions.push_back({primec::IrOpcode::ConvertF64ToU64, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI64, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_convert_f64_i64_u64.glsl";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "glsl_ir_backend_convert_f64_i64_u64.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("// Narrowed GLSL path lowers f64/i64 conversion through f32 payloads.") != std::string::npos);
  CHECK(source.find("// Narrowed GLSL path lowers f64/u64 conversion through f32 payloads.") != std::string::npos);
  CHECK(source.find("uint converted = 0u;") != std::string::npos);
}

TEST_CASE("glsl-ir backend writes narrowed f64 literal and return source") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushF64, 0x3ff8000000000000ull});
  function.instructions.push_back({primec::IrOpcode::ReturnF64, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_push_return_f64.glsl";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "glsl_ir_backend_push_return_f64.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("// Narrowed GLSL path lowers f64 literals through f32 payloads.") != std::string::npos);
  CHECK(source.find("stack[sp++] = int(uint(1069547520u));") != std::string::npos);
  CHECK(source.find("// Narrowed GLSL path returns f64 values through f32 payloads.") != std::string::npos);
}

TEST_CASE("glsl-ir backend writes narrowed f64 add source") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushF64, 0x3ff8000000000000ull});
  function.instructions.push_back({primec::IrOpcode::PushF64, 0x4000000000000000ull});
  function.instructions.push_back({primec::IrOpcode::AddF64, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnF64, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_add_f64.glsl";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "glsl_ir_backend_add_f64.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("// Narrowed GLSL path lowers f64 add through f32 payloads.") != std::string::npos);
  CHECK(source.find("stack[sp++] = floatBitsToInt(left + right);") != std::string::npos);
}

TEST_CASE("glsl-ir backend writes narrowed f64 sub source") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushF64, 0x4004000000000000ull});
  function.instructions.push_back({primec::IrOpcode::PushF64, 0x3ff8000000000000ull});
  function.instructions.push_back({primec::IrOpcode::SubF64, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnF64, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_sub_f64.glsl";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "glsl_ir_backend_sub_f64.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("// Narrowed GLSL path lowers f64 sub through f32 payloads.") != std::string::npos);
  CHECK(source.find("stack[sp++] = floatBitsToInt(left - right);") != std::string::npos);
}

TEST_CASE("glsl-ir backend writes narrowed f64 mul source") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushF64, 0x3ff8000000000000ull});
  function.instructions.push_back({primec::IrOpcode::PushF64, 0x4010000000000000ull});
  function.instructions.push_back({primec::IrOpcode::MulF64, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnF64, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_mul_f64.glsl";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "glsl_ir_backend_mul_f64.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("// Narrowed GLSL path lowers f64 mul through f32 payloads.") != std::string::npos);
  CHECK(source.find("stack[sp++] = floatBitsToInt(left * right);") != std::string::npos);
}

TEST_CASE("glsl-ir backend writes narrowed f64 div source") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushF64, 0x4014000000000000ull});
  function.instructions.push_back({primec::IrOpcode::PushF64, 0x4000000000000000ull});
  function.instructions.push_back({primec::IrOpcode::DivF64, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnF64, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_div_f64.glsl";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "glsl_ir_backend_div_f64.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("// Narrowed GLSL path lowers f64 div through f32 payloads.") != std::string::npos);
  CHECK(source.find("stack[sp++] = floatBitsToInt(left / right);") != std::string::npos);
}

TEST_CASE("glsl-ir backend writes narrowed f64 neg source") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushF64, 0x3ff8000000000000ull});
  function.instructions.push_back({primec::IrOpcode::NegF64, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnF64, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_neg_f64.glsl";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "glsl_ir_backend_neg_f64.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("// Narrowed GLSL path lowers f64 neg through f32 payloads.") != std::string::npos);
  CHECK(source.find("stack[sp - 1] = floatBitsToInt(-value);") != std::string::npos);
}

TEST_CASE("glsl-ir backend writes narrowed f64 equality compare source") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushF64, 0x3ff8000000000000ull});
  function.instructions.push_back({primec::IrOpcode::PushF64, 0x3ff8000000000000ull});
  function.instructions.push_back({primec::IrOpcode::CmpEqF64, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_cmp_eq_f64.glsl";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "glsl_ir_backend_cmp_eq_f64.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("// Narrowed GLSL path lowers f64 equality compare through f32 payloads.") != std::string::npos);
  CHECK(source.find("stack[sp++] = (left == right) ? 1 : 0;") != std::string::npos);
}

TEST_CASE("glsl-ir backend writes narrowed f64 inequality compare source") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushF64, 0x3ff8000000000000ull});
  function.instructions.push_back({primec::IrOpcode::PushF64, 0x4000000000000000ull});
  function.instructions.push_back({primec::IrOpcode::CmpNeF64, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_cmp_ne_f64.glsl";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "glsl_ir_backend_cmp_ne_f64.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("// Narrowed GLSL path lowers f64 inequality compare through f32 payloads.") != std::string::npos);
  CHECK(source.find("stack[sp++] = (left != right) ? 1 : 0;") != std::string::npos);
}

TEST_CASE("glsl-ir backend writes narrowed f64 less-than compare source") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushF64, 0x3ff8000000000000ull});
  function.instructions.push_back({primec::IrOpcode::PushF64, 0x4000000000000000ull});
  function.instructions.push_back({primec::IrOpcode::CmpLtF64, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_cmp_lt_f64.glsl";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "glsl_ir_backend_cmp_lt_f64.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("// Narrowed GLSL path lowers f64 less-than compare through f32 payloads.") != std::string::npos);
  CHECK(source.find("stack[sp++] = (left < right) ? 1 : 0;") != std::string::npos);
}

TEST_CASE("glsl-ir backend writes narrowed f64 less-or-equal compare source") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushF64, 0x3ff8000000000000ull});
  function.instructions.push_back({primec::IrOpcode::PushF64, 0x3ff8000000000000ull});
  function.instructions.push_back({primec::IrOpcode::CmpLeF64, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_cmp_le_f64.glsl";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "glsl_ir_backend_cmp_le_f64.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("// Narrowed GLSL path lowers f64 less-or-equal compare through f32 payloads.") != std::string::npos);
  CHECK(source.find("stack[sp++] = (left <= right) ? 1 : 0;") != std::string::npos);
}

TEST_CASE("glsl-ir backend writes narrowed f64 greater-than compare source") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushF64, 0x4000000000000000ull});
  function.instructions.push_back({primec::IrOpcode::PushF64, 0x3ff8000000000000ull});
  function.instructions.push_back({primec::IrOpcode::CmpGtF64, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_cmp_gt_f64.glsl";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "glsl_ir_backend_cmp_gt_f64.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("// Narrowed GLSL path lowers f64 greater-than compare through f32 payloads.") != std::string::npos);
  CHECK(source.find("stack[sp++] = (left > right) ? 1 : 0;") != std::string::npos);
}

TEST_CASE("glsl-ir backend writes narrowed f64 greater-or-equal compare source") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushF64, 0x4000000000000000ull});
  function.instructions.push_back({primec::IrOpcode::PushF64, 0x4000000000000000ull});
  function.instructions.push_back({primec::IrOpcode::CmpGeF64, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_cmp_ge_f64.glsl";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "glsl_ir_backend_cmp_ge_f64.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("// Narrowed GLSL path lowers f64 greater-or-equal compare through f32 payloads.") !=
        std::string::npos);
  CHECK(source.find("stack[sp++] = (left >= right) ? 1 : 0;") != std::string::npos);
}

TEST_CASE("glsl-ir backend writes file-open-read stub source") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  module.stringTable.push_back("/dev/null");
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::FileOpenRead, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_file_open_read.glsl";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "glsl_ir_backend_file_open_read.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("// GLSL backend cannot open files; push deterministic invalid file handle.") != std::string::npos);
  CHECK(source.find("stack[sp++] = -1;") != std::string::npos);
}

TEST_CASE("glsl-ir backend writes file-open-write stub source") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  module.stringTable.push_back("/tmp/out.txt");
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::FileOpenWrite, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_file_open_write.glsl";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "glsl_ir_backend_file_open_write.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("// GLSL backend cannot open files; push deterministic invalid file handle.") != std::string::npos);
  CHECK(source.find("stack[sp++] = -1;") != std::string::npos);
}

TEST_CASE("glsl-ir backend writes file-open-append stub source") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  module.stringTable.push_back("/tmp/out.txt");
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::FileOpenAppend, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_file_open_append.glsl";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "glsl_ir_backend_file_open_append.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("// GLSL backend cannot open files; push deterministic invalid file handle.") != std::string::npos);
  CHECK(source.find("stack[sp++] = -1;") != std::string::npos);
}

TEST_CASE("glsl-ir backend writes file-close stub source") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushI32, static_cast<uint64_t>(-1)});
  function.instructions.push_back({primec::IrOpcode::FileClose, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_file_close.glsl";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "glsl_ir_backend_file_close.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("// GLSL backend cannot close files; replace handle with deterministic success code.") !=
        std::string::npos);
  CHECK(source.find("stack[sp - 1] = 0;") != std::string::npos);
}

TEST_CASE("glsl-ir backend writes file-flush stub source") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushI32, static_cast<uint64_t>(-1)});
  function.instructions.push_back({primec::IrOpcode::FileFlush, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_file_flush.glsl";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "glsl_ir_backend_file_flush.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("// GLSL backend cannot flush files; replace handle with deterministic success code.") !=
        std::string::npos);
  CHECK(source.find("stack[sp - 1] = 0;") != std::string::npos);
}

TEST_CASE("glsl-ir backend writes file-write-i32 stub source") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushI32, static_cast<uint64_t>(-1)});
  function.instructions.push_back({primec::IrOpcode::PushI32, 7});
  function.instructions.push_back({primec::IrOpcode::FileWriteI32, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_file_write_i32.glsl";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "glsl_ir_backend_file_write_i32.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("// GLSL backend cannot write files; consume value/handle and push deterministic success code.") !=
        std::string::npos);
  CHECK(source.find("sp -= 2;") != std::string::npos);
  CHECK(source.find("stack[sp++] = 0;") != std::string::npos);
}

TEST_CASE("glsl-ir backend writes file-write-i64 stub source") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushI32, static_cast<uint64_t>(-1)});
  function.instructions.push_back({primec::IrOpcode::PushI64, 7});
  function.instructions.push_back({primec::IrOpcode::FileWriteI64, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_file_write_i64.glsl";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "glsl_ir_backend_file_write_i64.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("// GLSL backend cannot write files; consume value/handle and push deterministic success code.") !=
        std::string::npos);
  CHECK(source.find("sp -= 2;") != std::string::npos);
  CHECK(source.find("stack[sp++] = 0;") != std::string::npos);
}

TEST_CASE("glsl-ir backend writes file-write-u64 stub source") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushI32, static_cast<uint64_t>(-1)});
  function.instructions.push_back({primec::IrOpcode::PushI64, 7});
  function.instructions.push_back({primec::IrOpcode::FileWriteU64, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_file_write_u64.glsl";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "glsl_ir_backend_file_write_u64.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("// GLSL backend cannot write files; consume value/handle and push deterministic success code.") !=
        std::string::npos);
  CHECK(source.find("sp -= 2;") != std::string::npos);
  CHECK(source.find("stack[sp++] = 0;") != std::string::npos);
}

TEST_CASE("glsl-ir backend writes file-write-string stub source") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  module.stringTable.push_back("hello");
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushI32, static_cast<uint64_t>(-1)});
  function.instructions.push_back({primec::IrOpcode::FileWriteString, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_file_write_string.glsl";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "glsl_ir_backend_file_write_string.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("// GLSL backend cannot write files; replace handle with deterministic success code.") !=
        std::string::npos);
  CHECK(source.find("stack[sp - 1] = 0;") != std::string::npos);
}

TEST_CASE("glsl-ir backend writes file-write-byte stub source") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushI32, static_cast<uint64_t>(-1)});
  function.instructions.push_back({primec::IrOpcode::PushI32, 65});
  function.instructions.push_back({primec::IrOpcode::FileWriteByte, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_file_write_byte.glsl";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "glsl_ir_backend_file_write_byte.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("// GLSL backend cannot write files; consume byte/handle and push deterministic success code.") !=
        std::string::npos);
  CHECK(source.find("sp -= 2;") != std::string::npos);
  CHECK(source.find("stack[sp++] = 0;") != std::string::npos);
}

TEST_CASE("glsl-ir backend writes file-write-newline stub source") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushI32, static_cast<uint64_t>(-1)});
  function.instructions.push_back({primec::IrOpcode::FileWriteNewline, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_file_write_newline.glsl";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "glsl_ir_backend_file_write_newline.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("// GLSL backend cannot write files; replace handle with deterministic success code.") !=
        std::string::npos);
  CHECK(source.find("stack[sp - 1] = 0;") != std::string::npos);
}

TEST_CASE("glsl-ir backend writes address-of-local source") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::AddressOfLocal, 7});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_address_of_local.glsl";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "glsl_ir_backend_address_of_local.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("// GLSL backend lowers local addresses to deterministic slot-byte offsets.") !=
        std::string::npos);
  CHECK(source.find("stack[sp++] = 56;") != std::string::npos);
}

TEST_CASE("glsl-ir backend writes load-indirect source") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushI32, 41});
  function.instructions.push_back({primec::IrOpcode::StoreLocal, 3});
  function.instructions.push_back({primec::IrOpcode::AddressOfLocal, 3});
  function.instructions.push_back({primec::IrOpcode::LoadIndirect, 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_load_indirect.glsl";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "glsl_ir_backend_load_indirect.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("// GLSL backend loads locals through deterministic aligned byte-slot addressing.") !=
        std::string::npos);
  CHECK(source.find("uint loadIndirectAddress = uint(stack[--sp]);") != std::string::npos);
  CHECK(source.find("if ((loadIndirectAddress & 7u) == 0u) {") != std::string::npos);
  CHECK(source.find("loadIndirectValue = locals[loadIndirectIndex];") != std::string::npos);
  CHECK(source.find("stack[sp++] = loadIndirectValue;") != std::string::npos);
}

TEST_CASE("glsl-ir backend writes store-indirect source") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushI32, 24});
  function.instructions.push_back({primec::IrOpcode::PushI32, 41});
  function.instructions.push_back({primec::IrOpcode::StoreIndirect, 0});
  function.instructions.push_back({primec::IrOpcode::LoadLocal, 3});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  const std::filesystem::path dir = std::filesystem::current_path() / "primec_tests";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  CHECK_FALSE(static_cast<bool>(ec));
  const std::filesystem::path outputPath = dir / "ir_backend_registry_store_indirect.glsl";
  std::filesystem::remove(outputPath, ec);

  primec::IrBackendEmitOptions options;
  options.outputPath = outputPath.string();
  options.inputPath = "glsl_ir_backend_store_indirect.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  REQUIRE(backend->emit(module, options, result, error));
  CHECK(error.empty());
  CHECK(result.exitCode == 0);

  const std::string source = readTextFile(outputPath);
  CHECK(source.find("// GLSL backend stores locals through deterministic aligned byte-slot addressing.") !=
        std::string::npos);
  CHECK(source.find("int storeIndirectValue = stack[--sp];") != std::string::npos);
  CHECK(source.find("uint storeIndirectAddress = uint(stack[--sp]);") != std::string::npos);
  CHECK(source.find("if ((storeIndirectAddress & 7u) == 0u) {") != std::string::npos);
  CHECK(source.find("locals[storeIndirectIndex] = storeIndirectValue;") != std::string::npos);
  CHECK(source.find("stack[sp++] = storeIndirectValue;") != std::string::npos);
}

TEST_CASE("glsl-ir backend reports emitter diagnostics") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({static_cast<primec::IrOpcode>(255), 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  primec::IrBackendEmitOptions options;
  options.outputPath = (std::filesystem::current_path() / "primec_tests" / "unused.glsl").string();
  options.inputPath = "glsl_ir_backend_error.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  CHECK_FALSE(backend->emit(module, options, result, error));
  CHECK(error.find("ir-to-glsl failed:") != std::string::npos);
}

TEST_CASE("glsl-ir backend reports print string index diagnostics") {
  const primec::IrBackend *backend = primec::findIrBackend("glsl-ir");
  REQUIRE(backend != nullptr);

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back(
      {primec::IrOpcode::PrintString, primec::encodePrintStringImm(3, primec::encodePrintFlags(true, false))});
  function.instructions.push_back({primec::IrOpcode::ReturnVoid, 0});
  module.functions.push_back(function);

  primec::IrBackendEmitOptions options;
  options.outputPath = (std::filesystem::current_path() / "primec_tests" / "unused_print_index.glsl").string();
  options.inputPath = "glsl_ir_backend_print_string_index_error.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  CHECK_FALSE(backend->emit(module, options, result, error));
  CHECK(error.find("ir-to-glsl failed:") != std::string::npos);
  CHECK(error.find("string index out of range") != std::string::npos);
}

TEST_CASE("spirv-ir backend reports emitter diagnostics") {
  const primec::IrBackend *backend = primec::findIrBackend("spirv-ir");
  REQUIRE(backend != nullptr);
  CHECK(backend->requiresOutputPath());

  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({static_cast<primec::IrOpcode>(255), 0});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);

  primec::IrBackendEmitOptions options;
  options.outputPath = (std::filesystem::current_path() / "primec_tests" / "unused.spv").string();
  options.inputPath = "spirv_ir_backend_error.prime";
  primec::IrBackendEmitResult result;
  std::string error;
  CHECK_FALSE(backend->emit(module, options, result, error));
  CHECK(error.find("ir-to-glsl failed:") != std::string::npos);
}

TEST_SUITE_END();
