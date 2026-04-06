#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

#include "primec/CliDriver.h"
#include "primec/EmitKind.h"
#include "primec/IrBackends.h"
#include "primec/IrBackendProfiles.h"
#include "primec/IrLowerer.h"
#include "primec/IrPreparation.h"
#include "primec/testing/CompilePipelineDumpHelpers.h"

#include "test_ir_pipeline_backends_helpers.h"
#include "test_ir_pipeline_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.backends");

namespace {
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

TEST_CASE("ir preparation helper requires semantic product before lowering") {
  primec::Program program;
  primec::Options options;
  options.entryPath = "/main";
  options.inlineIrCalls = true;

  primec::IrModule ir;
  primec::IrPreparationFailure failure;
  CHECK_FALSE(primec::prepareIrModule(program, nullptr, options, primec::IrValidationTarget::Vm, ir, failure));
  CHECK(failure.stage == primec::IrPreparationFailureStage::Lowering);
  CHECK(failure.message == "semantic product is required for IR preparation");
  CHECK(failure.diagnosticInfo.message == failure.message);
}

TEST_CASE("ir lowerer requires semantic product before lowering") {
  primec::Program program;
  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, nullptr, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "semantic product is required for IR lowering");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir preparation helper reports lowering-stage failure for unresolved entry") {
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  primec::Options options;
  options.entryPath = "/missing";
  options.inlineIrCalls = true;

  primec::IrModule ir;
  primec::IrPreparationFailure failure;
  CHECK_FALSE(primec::prepareIrModule(program, &semanticProgram, options, primec::IrValidationTarget::Vm, ir, failure));
  CHECK(failure.stage == primec::IrPreparationFailureStage::Lowering);
  CHECK(failure.message == "native backend requires entry definition /missing");
  CHECK(failure.diagnosticInfo.message == failure.message);
}

TEST_CASE("ir lowerer rejects missing semantic-product direct-call targets") {
  primec::Program program;

  primec::Definition callee;
  callee.fullPath = "/callee";
  program.definitions.push_back(callee);

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "callee";
  callExpr.semanticNodeId = 41;
  mainDef.statements.push_back(callExpr);
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "missing semantic-product direct-call target: /main -> callee");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir lowerer rejects missing semantic-product method-call targets") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "values";
  primec::Expr methodCallExpr;
  methodCallExpr.kind = primec::Expr::Kind::Call;
  methodCallExpr.name = "count";
  methodCallExpr.isMethodCall = true;
  methodCallExpr.semanticNodeId = 42;
  methodCallExpr.args.push_back(receiverExpr);
  mainDef.statements.push_back(methodCallExpr);
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "missing semantic-product method-call target: /main -> count");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir lowerer rejects missing semantic-product binding facts") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  primec::Expr param;
  param.name = "value";
  param.semanticNodeId = 7;
  mainDef.parameters.push_back(param);
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "missing semantic-product binding fact: /main -> parameter value");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir lowerer rejects missing semantic-product return facts") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 81;
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
      .fullPath = "/main",
      .isExecution = false,
      .returnKind = "return",
      .isCompute = false,
      .isUnsafe = false,
      .activeEffects = {},
      .activeCapabilities = {},
      .hasResultType = false,
      .resultTypeHasValue = false,
      .resultValueType = "",
      .resultErrorType = "",
      .hasOnError = false,
      .onErrorHandlerPath = "",
      .onErrorErrorType = "",
      .onErrorBoundArgCount = 0,
      .semanticNodeId = 81,
  });

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "missing semantic-product return fact: /main");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir lowerer rejects missing semantic-product on_error facts") {
  primec::Program program;

  primec::Definition handlerDef;
  handlerDef.fullPath = "/handler";
  handlerDef.semanticNodeId = 90;
  program.definitions.push_back(handlerDef);

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 91;
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
      .fullPath = "/handler",
      .isExecution = false,
      .returnKind = "void",
      .isCompute = false,
      .isUnsafe = false,
      .activeEffects = {},
      .activeCapabilities = {},
      .hasResultType = false,
      .resultTypeHasValue = false,
      .resultValueType = "",
      .resultErrorType = "",
      .hasOnError = false,
      .onErrorHandlerPath = "",
      .onErrorErrorType = "",
      .onErrorBoundArgCount = 0,
      .semanticNodeId = 90,
  });
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
      .fullPath = "/main",
      .isExecution = false,
      .returnKind = "void",
      .isCompute = false,
      .isUnsafe = false,
      .activeEffects = {},
      .activeCapabilities = {},
      .hasResultType = false,
      .resultTypeHasValue = false,
      .resultValueType = "",
      .resultErrorType = "",
      .hasOnError = true,
      .onErrorHandlerPath = "/handler",
      .onErrorErrorType = "FileError",
      .onErrorBoundArgCount = 0,
      .semanticNodeId = 91,
  });
  semanticProgram.returnFacts.push_back(primec::SemanticProgramReturnFact{
      .definitionPath = "/semantic/main",
      .returnKind = "void",
      .structPath = "",
      .bindingTypeText = "void",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 91,
  });

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "missing semantic-product on_error fact: /main");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("cli driver preserves parse-stage diagnostic context") {
  primec::CompilePipelineOutput output;
  output.filteredSource = "main() { return(1i32) }";
  output.failure.stage = primec::CompilePipelineErrorStage::Parse;
  output.failure.message = "raw parse failure";
  output.failure.diagnosticInfo.message = "expected token";
  output.hasFailure = true;

  const primec::CliFailure failure = primec::describeCompilePipelineFailure(output);

  CHECK(failure.code == primec::DiagnosticCode::ParseError);
  CHECK(failure.plainPrefix == "Parse error: ");
  CHECK(failure.message == "raw parse failure");
  CHECK(failure.exitCode == 2);
  CHECK(failure.notes == std::vector<std::string>{"stage: parse"});
  REQUIRE(failure.diagnosticInfo.has_value());
  CHECK(failure.diagnosticInfo->message == output.failure.diagnosticInfo.message);
  REQUIRE(failure.sourceText.has_value());
  CHECK(*failure.sourceText == output.filteredSource);
}

TEST_CASE("cli driver reports semantic-product availability on post-semantics failures") {
  primec::CompilePipelineOutput output;
  output.filteredSource = "import /std/gfx/*\n[return<int>]\nmain(){ return(0i32) }\n";
  output.semanticProgram.entryPath = "/main";
  output.hasSemanticProgram = true;
  output.failure.stage = primec::CompilePipelineErrorStage::Semantic;
  output.failure.message = "graphics stdlib runtime substrate unavailable for glsl target: /std/gfx/*";
  output.failure.diagnosticInfo.message = output.failure.message;
  output.hasFailure = true;

  const primec::CliFailure failure = primec::describeCompilePipelineFailure(output);

  CHECK(failure.code == primec::DiagnosticCode::SemanticError);
  CHECK(failure.plainPrefix == "Semantic error: ");
  CHECK(failure.message == output.failure.message);
  CHECK(failure.notes == std::vector<std::string>({"stage: semantic", "semantic-product: available"}));
  REQUIRE(failure.diagnosticInfo.has_value());
  CHECK(failure.diagnosticInfo->message == output.failure.message);
  REQUIRE(failure.sourceText.has_value());
  CHECK(*failure.sourceText == output.filteredSource);
}

TEST_CASE("cpp-ir backend accepts semantic-product prepared IR from compile pipeline helper") {
  const std::string source =
      "[return<T>]\n"
      "id<T>([T] value) {\n"
      "  return(value)\n"
      "}\n"
      "\n"
      "[return<i32>]\n"
      "/vector/count([vector<i32>] self) {\n"
      "  return(17i32)\n"
      "}\n"
      "\n"
      "[return<i32>]\n"
      "main() {\n"
      "  [auto] selected{id(1i32)}\n"
      "  [auto] values{vector<i32>(1i32)}\n"
      "  return(selected + values.count())\n"
      "}\n";

  primec::testing::CompilePipelineBackendConformance conformance;
  std::string error;
  REQUIRE(primec::testing::runCompilePipelineBackendConformanceForTesting(
      source, "/main", "cpp-ir", conformance, error));
  CHECK(error.empty());
  CHECK(conformance.output.hasSemanticProgram);
  CHECK(conformance.backendKind == "cpp-ir");
  const auto *directCall = conformance.findDirectCallTarget("/main", "id");
  REQUIRE(directCall != nullptr);
  CHECK(directCall->resolvedPath.rfind("/id__t", 0) == 0);
  const auto *methodCall = conformance.findMethodCallTarget("/main", "count");
  REQUIRE(methodCall != nullptr);
  CHECK(methodCall->resolvedPath == "/vector/count");
  CHECK(conformance.emitResult.exitCode == 0);

  const std::string cpp = readTextFile(conformance.outputPath);
  CHECK(cpp.find("static int64_t ps_fn_0") != std::string::npos);
  CHECK(cpp.find("return static_cast<int>(ps_entry_0(argc, argv));") != std::string::npos);
}

TEST_CASE("vm backend executes semantic-product prepared IR from compile pipeline helper") {
  const std::string source =
      "[return<T>]\n"
      "id<T>([T] value) {\n"
      "  return(value)\n"
      "}\n"
      "\n"
      "[return<i32>]\n"
      "/vector/count([vector<i32>] self) {\n"
      "  return(17i32)\n"
      "}\n"
      "\n"
      "[return<i32>]\n"
      "main() {\n"
      "  [auto] selected{id(1i32)}\n"
      "  [auto] values{vector<i32>(1i32)}\n"
      "  return(selected + values.count())\n"
      "}\n";

  primec::testing::CompilePipelineBackendConformance conformance;
  std::string error;
  REQUIRE(primec::testing::runCompilePipelineBackendConformanceForTesting(
      source, "/main", "vm", conformance, error));
  CHECK(error.empty());
  CHECK(conformance.output.hasSemanticProgram);
  CHECK(conformance.backendKind == "vm");
  const auto *directCall = conformance.findDirectCallTarget("/main", "id");
  REQUIRE(directCall != nullptr);
  CHECK(directCall->resolvedPath.rfind("/id__t", 0) == 0);
  const auto *methodCall = conformance.findMethodCallTarget("/main", "count");
  REQUIRE(methodCall != nullptr);
  CHECK(methodCall->resolvedPath == "/vector/count");
  CHECK(conformance.emitResult.exitCode == 18);
}

TEST_CASE("native backend emits semantic-product prepared IR from compile pipeline helper") {
  const std::string source =
      "[return<T>]\n"
      "id<T>([T] value) {\n"
      "  return(value)\n"
      "}\n"
      "\n"
      "[return<i32>]\n"
      "/vector/count([vector<i32>] self) {\n"
      "  return(17i32)\n"
      "}\n"
      "\n"
      "[return<i32>]\n"
      "main() {\n"
      "  [auto] selected{id(1i32)}\n"
      "  [auto] values{vector<i32>(1i32)}\n"
      "  return(selected + values.count())\n"
      "}\n";

  primec::testing::CompilePipelineBackendConformance conformance;
  std::string error;
  REQUIRE(primec::testing::runCompilePipelineBackendConformanceForTesting(
      source, "/main", "native", conformance, error));
  CHECK(error.empty());
  CHECK(conformance.output.hasSemanticProgram);
  CHECK(conformance.backendKind == "native");
  const auto *directCall = conformance.findDirectCallTarget("/main", "id");
  REQUIRE(directCall != nullptr);
  CHECK(directCall->resolvedPath.rfind("/id__t", 0) == 0);
  const auto *methodCall = conformance.findMethodCallTarget("/main", "count");
  REQUIRE(methodCall != nullptr);
  CHECK(methodCall->resolvedPath == "/vector/count");
  CHECK(conformance.emitResult.exitCode == 0);
  CHECK(std::filesystem::exists(conformance.outputPath));
  CHECK(std::filesystem::is_regular_file(conformance.outputPath));
  CHECK(std::filesystem::file_size(conformance.outputPath) > 0);
}

TEST_CASE("compile pipeline preserves semantic product on post-semantics failure") {
  const std::filesystem::path tempPath = makeTempIrPipelineSourcePath();
  {
    std::ofstream file(tempPath);
    REQUIRE(file.good());
    file << R"(import /std/gfx/experimental/*

[return<i32>]
main() {
  return(0i32)
}
)";
  }

  primec::Options options;
  options.inputPath = tempPath.string();
  options.entryPath = "/main";
  options.emitKind = "glsl";
  options.collectDiagnostics = true;
  primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);

  primec::CompilePipelineOutput output;
  primec::CompilePipelineDiagnosticInfo diagnosticInfo;
  primec::CompilePipelineErrorStage errorStage = primec::CompilePipelineErrorStage::None;
  std::string error;
  const bool ok = primec::runCompilePipeline(options, output, errorStage, error, &diagnosticInfo);

  std::error_code ec;
  std::filesystem::remove(tempPath, ec);

  CHECK_FALSE(ok);
  CHECK(errorStage == primec::CompilePipelineErrorStage::Semantic);
  REQUIRE(output.hasFailure);
  CHECK(output.failure.stage == primec::CompilePipelineErrorStage::Semantic);
  CHECK(output.failure.message.find("graphics stdlib runtime substrate unavailable for glsl target") !=
        std::string::npos);
  CHECK(output.failure.message == error);
  REQUIRE(!output.failure.diagnosticInfo.records.empty());
  CHECK(output.failure.diagnosticInfo.records.front().message == output.failure.message);
  REQUIRE(output.hasSemanticProgram);
  CHECK(output.semanticProgram.entryPath == "/main");
  CHECK(std::find(output.semanticProgram.imports.begin(),
                  output.semanticProgram.imports.end(),
                  "/std/gfx/experimental/*") != output.semanticProgram.imports.end());
  CHECK(diagnosticInfo.message == output.failure.diagnosticInfo.message);
  REQUIRE(!diagnosticInfo.records.empty());
  CHECK(diagnosticInfo.records.front().message == output.failure.message);
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
  CHECK(source.find("describeCompilePipelineFailure(pipelineOutput)") != std::string::npos);
  CHECK(source.find("describeIrPreparationFailure(") != std::string::npos);
  CHECK(source.find("pipelineOutput.hasSemanticProgram ? &pipelineOutput.semanticProgram : nullptr") !=
        std::string::npos);
  CHECK(source.find("prepareIrModule(program, semanticProgram, options, validationTarget, ir, prepFailure)") !=
        std::string::npos);
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
  CHECK(source.find("describeCompilePipelineFailure(pipelineOutput)") != std::string::npos);
  CHECK(source.find("describeIrPreparationFailure(") != std::string::npos);
  CHECK(source.find("pipelineOutput.hasSemanticProgram ? &pipelineOutput.semanticProgram : nullptr") !=
        std::string::npos);
  CHECK(source.find("prepareIrModule(program, semanticProgram, options, primec::IrValidationTarget::Vm, ir, irFailure)") !=
        std::string::npos);
  CHECK(source.find("findIrBackend(\"vm\")") == std::string::npos);
  CHECK(source.find("CompilePipelineErrorStage::Import") == std::string::npos);
  CHECK(source.find("IrPreparationFailureStage::Validation") == std::string::npos);
  CHECK(source.find("IrLowerer lowerer") == std::string::npos);
  CHECK(source.find("inlineIrModuleCalls(ir, error)") == std::string::npos);
  CHECK(source.find("validateIrModule(ir, primec::IrValidationTarget::Vm, error)") == std::string::npos);
}
