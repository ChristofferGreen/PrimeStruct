#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
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

template <typename Entry, typename Predicate>
const Entry *findSemanticEntry(const std::vector<Entry> &entries, const Predicate &predicate) {
  const auto it = std::find_if(entries.begin(), entries.end(), predicate);
  if (it == entries.end()) {
    return nullptr;
  }
  return &*it;
}

template <typename Entry, typename Predicate>
const Entry *findSemanticEntry(const std::vector<const Entry *> &entries, const Predicate &predicate) {
  const auto it = std::find_if(entries.begin(),
                               entries.end(),
                               [&](const Entry *entry) { return entry != nullptr && predicate(*entry); });
  if (it == entries.end()) {
    return nullptr;
  }
  return *it;
}

std::string buildMathStressSemanticSource(std::size_t localDefinitionCount) {
  std::string source = "import /std/math/*\n\n";
  for (std::size_t i = 0; i < localDefinitionCount; ++i) {
    source += "[return<i32>]\n";
    source += "leaf_" + std::to_string(i) + "() {\n";
    source += "  return(/std/math/abs(" + std::to_string(i + 1) + "i32))\n";
    source += "}\n\n";
  }
  source += "[return<i32>]\n";
  source += "main() {\n";
  source += "  [i32 mut] total{0i32}\n";
  for (std::size_t i = 0; i < localDefinitionCount; ++i) {
    source += "  assign(total, plus(total, leaf_" + std::to_string(i) + "()))\n";
  }
  source += "  return(total)\n";
  source += "}\n";
  return source;
}

std::string buildMathStressDiagnosticSource(std::size_t localDefinitionCount) {
  std::string source = "import /std/math/*\n\n";
  for (std::size_t i = 0; i < localDefinitionCount; ++i) {
    source += "[return<void>]\n";
    source += "broken_" + std::to_string(i) + "() {\n";
    source += "  missing_symbol_" + std::to_string(i) + "()\n";
    source += "}\n\n";
  }
  source += "[return<i32>]\n";
  source += "main() {\n";
  source += "  return(0i32)\n";
  source += "}\n";
  return source;
}

std::vector<std::string> compileDiagnosticMessages(const primec::CompilePipelineDiagnosticInfo &diagnostics) {
  std::vector<std::string> messages;
  messages.reserve(diagnostics.records.size());
  for (const auto &record : diagnostics.records) {
    messages.push_back(record.message);
  }
  return messages;
}

void checkOptionalRssCheckpointSnapshot(const primec::SemanticPhaseCounterSnapshot &snapshot) {
  const bool hasCheckpoints = snapshot.rssBeforeBytes > 0 || snapshot.rssAfterBytes > 0;
  if (!hasCheckpoints) {
    CHECK(snapshot.rssBeforeBytes == 0);
    CHECK(snapshot.rssAfterBytes == 0);
    return;
  }
  CHECK(snapshot.rssBeforeBytes > 0);
  CHECK(snapshot.rssAfterBytes > 0);
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
  callee.semanticNodeId = 82;
  program.definitions.push_back(callee);

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 81;
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

TEST_CASE("ir lowerer rejects missing semantic-product direct-call semantic ids") {
  primec::Program program;

  primec::Definition callee;
  callee.fullPath = "/callee";
  callee.semanticNodeId = 82;
  program.definitions.push_back(callee);

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "callee";
  mainDef.statements.push_back(callExpr);
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "missing semantic-product direct-call semantic id: /main -> callee");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir lowerer rejects missing semantic-product method-call targets") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 81;
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

TEST_CASE("ir lowerer rejects missing semantic-product method-call semantic ids") {
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
  CHECK(error == "missing semantic-product method-call semantic id: /main -> count");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir lowerer rejects missing semantic-product method-call resolved path ids") {
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
  methodCallExpr.semanticNodeId = 4201;
  methodCallExpr.args.push_back(receiverExpr);
  mainDef.statements.push_back(methodCallExpr);
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  semanticProgram.methodCallTargets.push_back(primec::SemanticProgramMethodCallTarget{
      .scopePath = "/main",
      .methodName = "count",
      .receiverTypeText = "vector<i32>",
      .sourceLine = 1,
      .sourceColumn = 1,
      .semanticNodeId = 4201,
      .resolvedPathId = primec::InvalidSymbolId,
  });

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "missing semantic-product method-call resolved path id: /main -> count");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir lowerer rejects missing semantic-product bridge-path choices") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  primec::Expr valuesExpr;
  valuesExpr.kind = primec::Expr::Kind::Name;
  valuesExpr.name = "values";
  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "count";
  callExpr.semanticNodeId = 52;
  callExpr.args.push_back(valuesExpr);
  mainDef.statements.push_back(callExpr);
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "count",
      .sourceLine = 1,
      .sourceColumn = 1,
      .semanticNodeId = 52,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/vector/count"),
  });

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "missing semantic-product bridge-path choice: /main -> count");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir lowerer rejects missing semantic-product bridge helper name ids") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  primec::Expr valuesExpr;
  valuesExpr.kind = primec::Expr::Kind::Name;
  valuesExpr.name = "values";
  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "count";
  callExpr.semanticNodeId = 5201;
  callExpr.args.push_back(valuesExpr);
  mainDef.statements.push_back(callExpr);
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "count",
      .sourceLine = 1,
      .sourceColumn = 1,
      .semanticNodeId = 5201,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/vector/count"),
  });
  semanticProgram.bridgePathChoices.push_back(primec::SemanticProgramBridgePathChoice{
      .scopePath = "/main",
      .collectionFamily = "vector",
      .sourceLine = 1,
      .sourceColumn = 1,
      .semanticNodeId = 5201,
      .helperNameId = primec::InvalidSymbolId,
      .chosenPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/vector/count"),
  });

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "missing semantic-product bridge helper name id: /main -> count");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir lowerer reports bridge helper id errors before direct-call target gaps") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  primec::Expr valuesExpr;
  valuesExpr.kind = primec::Expr::Kind::Name;
  valuesExpr.name = "values";
  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "count";
  callExpr.semanticNodeId = 5202;
  callExpr.args.push_back(valuesExpr);
  mainDef.statements.push_back(callExpr);
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  semanticProgram.bridgePathChoices.push_back(primec::SemanticProgramBridgePathChoice{
      .scopePath = "/main",
      .collectionFamily = "vector",
      .sourceLine = 1,
      .sourceColumn = 1,
      .semanticNodeId = 5202,
      .helperNameId = primec::InvalidSymbolId,
      .chosenPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/vector/count"),
  });

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "missing semantic-product bridge helper name id: /main -> count");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir lowerer rejects semantic-product bridge paths with invalid helper name ids") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  primec::Expr valuesExpr;
  valuesExpr.kind = primec::Expr::Kind::Name;
  valuesExpr.name = "values";
  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "count";
  callExpr.semanticNodeId = 5203;
  callExpr.args.push_back(valuesExpr);
  mainDef.statements.push_back(callExpr);
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "count",
      .sourceLine = 1,
      .sourceColumn = 1,
      .semanticNodeId = 5203,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/vector/count"),
  });
  semanticProgram.bridgePathChoices.push_back(primec::SemanticProgramBridgePathChoice{
      .scopePath = "/main",
      .collectionFamily = "vector",
      .sourceLine = 1,
      .sourceColumn = 1,
      .semanticNodeId = 5203,
      .helperNameId =
          static_cast<primec::SymbolId>(semanticProgram.callTargetStringTable.size() + 1u),
      .chosenPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/vector/count"),
  });

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "missing semantic-product bridge helper name id: /main -> count");
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

TEST_CASE("ir lowerer rejects semantic-product binding facts missing resolved path ids") {
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
  semanticProgram.bindingFacts.push_back(primec::SemanticProgramBindingFact{
      .scopePath = "/main",
      .siteKind = "parameter",
      .name = "value",
      .bindingTypeText = "i32",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 7,
      .provenanceHandle = 0,
      .resolvedPathId = primec::InvalidSymbolId,
  });

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "missing semantic-product binding resolved path id: /main -> parameter value");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir lowerer rejects missing semantic-product local binding facts") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 81;

  primec::Expr initializerExpr;
  initializerExpr.kind = primec::Expr::Kind::Literal;
  initializerExpr.literalValue = 1;
  initializerExpr.intWidth = 32;

  primec::Expr bindingExpr;
  bindingExpr.isBinding = true;
  bindingExpr.name = "selected";
  bindingExpr.semanticNodeId = 43;
  bindingExpr.args.push_back(initializerExpr);
  mainDef.statements.push_back(bindingExpr);
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
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
      .semanticNodeId = 81,
      .provenanceHandle = 0,
      .fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
  });
  semanticProgram.returnFacts.push_back(primec::SemanticProgramReturnFact{
      .returnKind = "void",
      .structPath = "",
      .bindingTypeText = "void",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 81,
      .definitionPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
  });
  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "missing semantic-product binding fact: /main -> local selected");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir lowerer completeness checks keep deterministic first-failure order") {
  primec::Program program;

  primec::Definition callee;
  callee.fullPath = "/callee";
  callee.semanticNodeId = 82;
  program.definitions.push_back(callee);

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 81;

  primec::Expr param;
  param.name = "value";
  param.semanticNodeId = 45;
  primec::Transform paramArrayTransform;
  paramArrayTransform.name = "array";
  paramArrayTransform.templateArgs = {"string"};
  param.transforms.push_back(paramArrayTransform);
  mainDef.parameters.push_back(param);

  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "callee";
  callExpr.semanticNodeId = 46;
  mainDef.statements.push_back(callExpr);

  primec::Expr initializerExpr;
  initializerExpr.kind = primec::Expr::Kind::Literal;
  initializerExpr.literalValue = 1;
  initializerExpr.intWidth = 32;

  primec::Expr bindingExpr;
  bindingExpr.isBinding = true;
  bindingExpr.name = "selected";
  bindingExpr.semanticNodeId = 47;
  bindingExpr.args.push_back(initializerExpr);
  mainDef.statements.push_back(bindingExpr);
  program.definitions.push_back(mainDef);

  primec::IrLowerer lowerer;
  auto lowerWithSemanticProduct = [&](primec::SemanticProgram &semanticProgram,
                                      std::string &errorOut,
                                      primec::DiagnosticSinkReport &diagnosticOut) {
    primec::IrModule module;
    return lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, errorOut, &diagnosticOut);
  };

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
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
      .semanticNodeId = 81,
      .provenanceHandle = 0,
      .fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
  });
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
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
      .semanticNodeId = 82,
      .provenanceHandle = 0,
      .fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/callee"),
  });
  semanticProgram.returnFacts.push_back(primec::SemanticProgramReturnFact{
      .returnKind = "void",
      .structPath = "",
      .bindingTypeText = "void",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 81,
      .definitionPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
  });
  semanticProgram.returnFacts.push_back(primec::SemanticProgramReturnFact{
      .returnKind = "void",
      .structPath = "",
      .bindingTypeText = "void",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 82,
      .definitionPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/callee"),
  });

  std::string error;
  primec::DiagnosticSinkReport diagnosticInfo;
  CHECK_FALSE(lowerWithSemanticProduct(semanticProgram, error, diagnosticInfo));
  CHECK(error == "missing semantic-product direct-call target: /main -> callee");
  CHECK(diagnosticInfo.message == error);

  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "callee",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 46,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/callee"),
  });

  error.clear();
  diagnosticInfo = {};
  CHECK_FALSE(lowerWithSemanticProduct(semanticProgram, error, diagnosticInfo));
  CHECK(error == "missing semantic-product binding fact: /main -> parameter value");
  CHECK(diagnosticInfo.message == error);

  semanticProgram.bindingFacts.push_back(primec::SemanticProgramBindingFact{
      .scopePath = "/main",
      .siteKind = "parameter",
      .name = "value",
      .bindingTypeText = "array<string>",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 45,
      .provenanceHandle = 0,
      .resolvedPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/main/value"),
  });
  semanticProgram.bindingFacts.push_back(primec::SemanticProgramBindingFact{
      .scopePath = "/main",
      .siteKind = "local",
      .name = "selected",
      .bindingTypeText = "i32",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 47,
      .provenanceHandle = 0,
      .resolvedPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/main/selected"),
  });

  semanticProgram.bindingFacts.back().resolvedPathId =
      static_cast<primec::SymbolId>(semanticProgram.callTargetStringTable.size() + 1u);
  error.clear();
  diagnosticInfo = {};
  CHECK_FALSE(lowerWithSemanticProduct(semanticProgram, error, diagnosticInfo));
  CHECK(error == "missing semantic-product binding resolved path id: /main -> local selected");
  CHECK(diagnosticInfo.message == error);

  semanticProgram.bindingFacts.back().resolvedPathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/main/selected");
  primec::SemanticProgramLocalAutoFact localAutoFact;
  localAutoFact.scopePath = "/main";
  localAutoFact.bindingName = "selected";
  localAutoFact.bindingTypeText = "i32";
  localAutoFact.semanticNodeId = 47;
  localAutoFact.initializerResolvedPathId =
      static_cast<primec::SymbolId>(semanticProgram.callTargetStringTable.size() + 1u);
  semanticProgram.localAutoFacts.push_back(std::move(localAutoFact));
  error.clear();
  diagnosticInfo = {};
  CHECK_FALSE(lowerWithSemanticProduct(semanticProgram, error, diagnosticInfo));
  CHECK(error == "missing semantic-product local-auto initializer path id: /main -> local selected");
  CHECK(diagnosticInfo.message == error);

  semanticProgram.localAutoFacts.back().initializerResolvedPathId = primec::InvalidSymbolId;
  error.clear();
  diagnosticInfo = {};
  CHECK(lowerWithSemanticProduct(semanticProgram, error, diagnosticInfo));
  CHECK(error.empty());
  CHECK(diagnosticInfo.message.empty());
}

TEST_CASE("ir lowerer rejects missing semantic-product callable summaries") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "missing semantic-product callable summary path id");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir lowerer rejects missing semantic-product entry parameter facts") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 81;
  primec::Expr entryParam;
  entryParam.name = "argv";
  primec::Transform arrayTransform;
  arrayTransform.name = "array";
  arrayTransform.templateArgs = {"string"};
  entryParam.transforms.push_back(arrayTransform);
  mainDef.parameters.push_back(entryParam);
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
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
      .semanticNodeId = 81,
      .provenanceHandle = 0,
      .fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
  });
  semanticProgram.returnFacts.push_back(primec::SemanticProgramReturnFact{
      .returnKind = "void",
      .structPath = "",
      .bindingTypeText = "void",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 81,
      .definitionPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
  });

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "missing semantic-product entry parameter fact: /main");
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
      .provenanceHandle = 0,
      .fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
  });

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "missing semantic-product return fact: /main");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir lowerer rejects missing semantic-product callable result metadata") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 82;
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
      .isExecution = false,
      .returnKind = "return",
      .isCompute = false,
      .isUnsafe = false,
      .activeEffects = {},
      .activeCapabilities = {},
      .hasResultType = true,
      .resultTypeHasValue = true,
      .resultValueType = "",
      .resultErrorType = "FileError",
      .hasOnError = false,
      .onErrorHandlerPath = "",
      .onErrorErrorType = "",
      .onErrorBoundArgCount = 0,
      .semanticNodeId = 82,
      .provenanceHandle = 0,
      .fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
  });
  semanticProgram.returnFacts.push_back(primec::SemanticProgramReturnFact{
      .returnKind = "return",
      .structPath = "",
      .bindingTypeText = "Result<i32, FileError>",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 82,
      .definitionPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
  });

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "missing semantic-product callable result metadata: /main");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir lowerer rejects missing semantic-product callable summary path id") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 8201;
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
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
      .semanticNodeId = 8201,
      .provenanceHandle = 0,
      .fullPathId = primec::InvalidSymbolId,
  });
  semanticProgram.returnFacts.push_back(primec::SemanticProgramReturnFact{
      .returnKind = "return",
      .structPath = "",
      .bindingTypeText = "i32",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 8201,
      .definitionPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
  });

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "missing semantic-product callable summary path id");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir lowerer rejects invalid semantic-product callable summary path id") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 8202;
  primec::Expr returnExpr;
  returnExpr.kind = primec::Expr::Kind::Literal;
  returnExpr.literalValue = 1;
  returnExpr.intWidth = 32;
  mainDef.returnExpr = returnExpr;
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
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
      .semanticNodeId = 8202,
      .provenanceHandle = 0,
      .fullPathId =
          static_cast<primec::SymbolId>(semanticProgram.callTargetStringTable.size() + 1u),
  });

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "missing semantic-product callable summary: /main");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir lowerer rejects missing semantic-product return binding types") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
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
      .semanticNodeId = 0,
      .provenanceHandle = 0,
      .fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
  });
  semanticProgram.returnFacts.push_back(primec::SemanticProgramReturnFact{
      .returnKind = "return",
      .structPath = "",
      .bindingTypeText = "",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 0,
      .definitionPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
  });

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "missing semantic-product return binding type: /main");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir lowerer rejects missing semantic-product return definition path id") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 9101;
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
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
      .semanticNodeId = 9101,
      .provenanceHandle = 0,
      .fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
  });
  semanticProgram.returnFacts.push_back(primec::SemanticProgramReturnFact{
      .returnKind = "return",
      .structPath = "",
      .bindingTypeText = "i32",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 9101,
      .definitionPathId = primec::InvalidSymbolId,
  });

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "missing semantic-product return definition path id");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir lowerer rejects incomplete semantic-product query facts") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "lookup";
  callExpr.semanticNodeId = 83;
  mainDef.statements.push_back(callExpr);
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "lookup",
      .sourceLine = 1,
      .sourceColumn = 1,
      .semanticNodeId = 83,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/lookup"),
  });
  semanticProgram.queryFacts.push_back(primec::SemanticProgramQueryFact{
      .scopePath = "/main",
      .callName = "lookup",
      .queryTypeText = "Result<i32, FileError>",
      .bindingTypeText = "Result<i32, FileError>",
      .hasResultType = true,
      .resultTypeHasValue = true,
      .resultValueType = "",
      .resultErrorType = "FileError",
      .sourceLine = 1,
      .sourceColumn = 1,
      .semanticNodeId = 83,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/lookup"),
  });
  primec::SemanticProgramCallableSummary callableSummary;
  callableSummary.fullPathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/main");
  callableSummary.returnKind = "i32";
  semanticProgram.callableSummaries.push_back(std::move(callableSummary));

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "incomplete semantic-product query fact: lookup");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir lowerer rejects missing semantic-product query resolved path id") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "lookup";
  callExpr.semanticNodeId = 8301;
  mainDef.statements.push_back(callExpr);
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "lookup",
      .sourceLine = 1,
      .sourceColumn = 1,
      .semanticNodeId = 8301,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/lookup"),
  });
  semanticProgram.queryFacts.push_back(primec::SemanticProgramQueryFact{
      .scopePath = "/main",
      .callName = "lookup",
      .queryTypeText = "Result<i32, FileError>",
      .bindingTypeText = "Result<i32, FileError>",
      .hasResultType = true,
      .resultTypeHasValue = true,
      .resultValueType = "i32",
      .resultErrorType = "FileError",
      .sourceLine = 1,
      .sourceColumn = 1,
      .semanticNodeId = 8301,
      .resolvedPathId = primec::InvalidSymbolId,
  });
  primec::SemanticProgramCallableSummary callableSummary;
  callableSummary.fullPathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/main");
  callableSummary.returnKind = "i32";
  semanticProgram.callableSummaries.push_back(std::move(callableSummary));

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "missing semantic-product query resolved path id: lookup");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir lowerer rejects incomplete semantic-product try facts") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  primec::Expr sourceExpr;
  sourceExpr.kind = primec::Expr::Kind::Name;
  sourceExpr.name = "value";
  primec::Expr tryExpr;
  tryExpr.kind = primec::Expr::Kind::Call;
  tryExpr.name = "try";
  tryExpr.args.push_back(sourceExpr);
  tryExpr.semanticNodeId = 84;
  mainDef.statements.push_back(tryExpr);
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "try",
      .sourceLine = 1,
      .sourceColumn = 1,
      .semanticNodeId = 84,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/try"),
  });
  semanticProgram.tryFacts.push_back(primec::SemanticProgramTryFact{
      .scopePath = "/main",
      .operandBindingTypeText = "Result<i32, FileError>",
      .operandReceiverBindingTypeText = "",
      .operandQueryTypeText = "Result<i32, FileError>",
      .valueType = "",
      .errorType = "FileError",
      .contextReturnKind = "return",
      .onErrorHandlerPath = "/handler",
      .onErrorErrorType = "FileError",
      .onErrorBoundArgCount = 1,
      .sourceLine = 1,
      .sourceColumn = 1,
      .semanticNodeId = 84,
      .operandResolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/lookup"),
  });
  primec::SemanticProgramCallableSummary callableSummary;
  callableSummary.fullPathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/main");
  callableSummary.returnKind = "i32";
  semanticProgram.callableSummaries.push_back(std::move(callableSummary));

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "incomplete semantic-product try fact: try");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir lowerer rejects missing semantic-product try operand resolved path id") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  primec::Expr sourceExpr;
  sourceExpr.kind = primec::Expr::Kind::Name;
  sourceExpr.name = "value";
  primec::Expr tryExpr;
  tryExpr.kind = primec::Expr::Kind::Call;
  tryExpr.name = "try";
  tryExpr.args.push_back(sourceExpr);
  tryExpr.semanticNodeId = 8401;
  mainDef.statements.push_back(tryExpr);
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "try",
      .sourceLine = 1,
      .sourceColumn = 1,
      .semanticNodeId = 8401,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/try"),
  });
  semanticProgram.tryFacts.push_back(primec::SemanticProgramTryFact{
      .scopePath = "/main",
      .operandBindingTypeText = "Result<i32, FileError>",
      .operandReceiverBindingTypeText = "",
      .operandQueryTypeText = "Result<i32, FileError>",
      .valueType = "i32",
      .errorType = "FileError",
      .contextReturnKind = "return",
      .onErrorHandlerPath = "/handler",
      .onErrorErrorType = "FileError",
      .onErrorBoundArgCount = 1,
      .sourceLine = 1,
      .sourceColumn = 1,
      .semanticNodeId = 8401,
      .operandResolvedPathId = primec::InvalidSymbolId,
  });
  primec::SemanticProgramCallableSummary callableSummary;
  callableSummary.fullPathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/main");
  callableSummary.returnKind = "i32";
  semanticProgram.callableSummaries.push_back(std::move(callableSummary));

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "missing semantic-product try operand resolved path id: try");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir lowerer rejects missing semantic-product on_error handler path id") {
  primec::Program program;

  primec::Definition handlerDef;
  handlerDef.fullPath = "/handler";
  handlerDef.semanticNodeId = 9200;
  program.definitions.push_back(handlerDef);

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 9201;
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
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
      .semanticNodeId = 9200,
      .provenanceHandle = 0,
      .fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/handler"),
  });
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
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
      .semanticNodeId = 9201,
      .provenanceHandle = 0,
      .fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
  });
  semanticProgram.returnFacts.push_back(primec::SemanticProgramReturnFact{
      .returnKind = "void",
      .structPath = "",
      .bindingTypeText = "void",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 9201,
      .definitionPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
  });
  semanticProgram.onErrorFacts.push_back(primec::SemanticProgramOnErrorFact{
      .definitionPath = "/main",
      .returnKind = "void",
      .errorType = "FileError",
      .boundArgCount = 0,
      .boundArgTexts = {},
      .returnResultHasValue = false,
      .returnResultValueType = "",
      .returnResultErrorType = "",
      .semanticNodeId = 9201,
      .provenanceHandle = 0,
      .definitionPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
      .returnKindId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "void"),
      .handlerPathId = primec::InvalidSymbolId,
      .errorTypeId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "FileError"),
  });

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "missing semantic-product on_error handler path id: /main");
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
      .provenanceHandle = 0,
      .fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/handler"),
  });
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
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
      .provenanceHandle = 0,
      .fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
  });
  semanticProgram.returnFacts.push_back(primec::SemanticProgramReturnFact{
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
      .definitionPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/semantic/main"),
  });

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "missing semantic-product on_error fact: /main");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir lowerer rejects mismatched semantic-product on_error bound args") {
  primec::Program program;

  primec::Definition handlerDef;
  handlerDef.fullPath = "/handler";
  handlerDef.semanticNodeId = 92;
  program.definitions.push_back(handlerDef);

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 93;
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
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
      .semanticNodeId = 92,
      .provenanceHandle = 0,
      .fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/handler"),
  });
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
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
      .onErrorBoundArgCount = 2,
      .semanticNodeId = 93,
      .provenanceHandle = 0,
      .fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
  });
  semanticProgram.returnFacts.push_back(primec::SemanticProgramReturnFact{
      .returnKind = "void",
      .structPath = "",
      .bindingTypeText = "void",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 93,
      .definitionPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
  });
  semanticProgram.onErrorFacts.push_back(primec::SemanticProgramOnErrorFact{
      .definitionPath = "/main",
      .returnKind = "void",
      .errorType = "FileError",
      .boundArgCount = 2,
      .boundArgTexts = {"1i32"},
      .returnResultHasValue = false,
      .returnResultValueType = "",
      .returnResultErrorType = "",
      .semanticNodeId = 93,
      .handlerPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/handler"),
  });

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "semantic-product on_error bound arg mismatch on /main");
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
  CHECK(conformance.resolvedDirectCallPath(*directCall).rfind("/id__t", 0) == 0);
  const auto *methodCall = conformance.findMethodCallTarget("/main", "count");
  REQUIRE(methodCall != nullptr);
  CHECK(conformance.resolvedMethodCallPath(*methodCall) == "/vector/count");
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
  CHECK(conformance.resolvedDirectCallPath(*directCall).rfind("/id__t", 0) == 0);
  const auto *methodCall = conformance.findMethodCallTarget("/main", "count");
  REQUIRE(methodCall != nullptr);
  CHECK(conformance.resolvedMethodCallPath(*methodCall) == "/vector/count");
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
  CHECK(conformance.resolvedDirectCallPath(*directCall).rfind("/id__t", 0) == 0);
  const auto *methodCall = conformance.findMethodCallTarget("/main", "count");
  REQUIRE(methodCall != nullptr);
  CHECK(conformance.resolvedMethodCallPath(*methodCall) == "/vector/count");
  CHECK(conformance.emitResult.exitCode == 0);
  CHECK(std::filesystem::exists(conformance.outputPath));
  CHECK(std::filesystem::is_regular_file(conformance.outputPath));
  CHECK(std::filesystem::file_size(conformance.outputPath) > 0);
}

TEST_CASE("backend conformance keeps semantic-product-owned facts aligned across backends") {
  const std::string source = R"(
[return<void>]
unexpectedError([i32] err) {
}

[return<T>]
id<T>([T] value) {
  return(value)
}

[return<Result<int, i32>>]
lookup() {
  return(Result.ok(4i32))
}

[return<i32>]
/vector/count([vector<i32>] self) {
  return(17i32)
}

[return<i32> effects(heap_alloc) on_error<i32, /unexpectedError>]
main() {
  [auto] direct{id(1i32)}
  [auto] values{vector<i32>(1i32)}
  [i32] method{values.count()}
  [i32] bridge{count(values)}
  [auto] selected{try(lookup())}
  return(direct + method + bridge + selected)
}
)";

  auto runConformance = [&](std::string_view emitKind) {
    primec::testing::CompilePipelineBackendConformance conformance;
    std::string error;
    REQUIRE(primec::testing::runCompilePipelineBackendConformanceForTesting(
        source, "/main", emitKind, conformance, error));
    CHECK(error.empty());
    CHECK(conformance.output.hasSemanticProgram);
    return conformance;
  };

  const auto cppConformance = runConformance("cpp-ir");
  const auto vmConformance = runConformance("vm");
  const auto nativeConformance = runConformance("native");

  const auto *cppDirect = cppConformance.findDirectCallTarget("/main", "id");
  const auto *vmDirect = vmConformance.findDirectCallTarget("/main", "id");
  const auto *nativeDirect = nativeConformance.findDirectCallTarget("/main", "id");
  REQUIRE(cppDirect != nullptr);
  REQUIRE(vmDirect != nullptr);
  REQUIRE(nativeDirect != nullptr);
  const std::string_view cppDirectPath = cppConformance.resolvedDirectCallPath(*cppDirect);
  const std::string_view vmDirectPath = vmConformance.resolvedDirectCallPath(*vmDirect);
  const std::string_view nativeDirectPath = nativeConformance.resolvedDirectCallPath(*nativeDirect);
  CHECK(cppDirectPath.rfind("/id__t", 0) == 0);
  CHECK(vmDirectPath == cppDirectPath);
  CHECK(nativeDirectPath == cppDirectPath);

  const auto *cppMethod = cppConformance.findMethodCallTarget("/main", "count");
  const auto *vmMethod = vmConformance.findMethodCallTarget("/main", "count");
  const auto *nativeMethod = nativeConformance.findMethodCallTarget("/main", "count");
  REQUIRE(cppMethod != nullptr);
  REQUIRE(vmMethod != nullptr);
  REQUIRE(nativeMethod != nullptr);
  const std::string_view cppMethodPath = cppConformance.resolvedMethodCallPath(*cppMethod);
  const std::string_view vmMethodPath = vmConformance.resolvedMethodCallPath(*vmMethod);
  const std::string_view nativeMethodPath = nativeConformance.resolvedMethodCallPath(*nativeMethod);
  CHECK(cppMethodPath == "/vector/count");
  CHECK(vmMethodPath == cppMethodPath);
  CHECK(nativeMethodPath == cppMethodPath);

  const auto *cppBridge = findSemanticEntry(primec::semanticProgramBridgePathChoiceView(cppConformance.output.semanticProgram),
      [&cppConformance](const primec::SemanticProgramBridgePathChoice &entry) {
        return entry.scopePath == "/main" &&
               primec::semanticProgramBridgePathChoiceHelperName(
                   cppConformance.output.semanticProgram, entry) == "count";
      });
  const auto *vmBridge = findSemanticEntry(primec::semanticProgramBridgePathChoiceView(vmConformance.output.semanticProgram),
      [&vmConformance](const primec::SemanticProgramBridgePathChoice &entry) {
        return entry.scopePath == "/main" &&
               primec::semanticProgramBridgePathChoiceHelperName(
                   vmConformance.output.semanticProgram, entry) == "count";
      });
  const auto *nativeBridge = findSemanticEntry(primec::semanticProgramBridgePathChoiceView(nativeConformance.output.semanticProgram),
      [&nativeConformance](const primec::SemanticProgramBridgePathChoice &entry) {
        return entry.scopePath == "/main" &&
               primec::semanticProgramBridgePathChoiceHelperName(
                   nativeConformance.output.semanticProgram, entry) == "count";
      });
  CHECK((cppBridge != nullptr) == (vmBridge != nullptr));
  CHECK((cppBridge != nullptr) == (nativeBridge != nullptr));
  if (cppBridge != nullptr) {
    const std::string_view cppBridgePath = primec::semanticProgramResolveCallTargetString(
        cppConformance.output.semanticProgram, cppBridge->chosenPathId);
    const std::string_view vmBridgePath = primec::semanticProgramResolveCallTargetString(
        vmConformance.output.semanticProgram, vmBridge->chosenPathId);
    const std::string_view nativeBridgePath = primec::semanticProgramResolveCallTargetString(
        nativeConformance.output.semanticProgram, nativeBridge->chosenPathId);
    CHECK(cppBridgePath == "/vector/count");
    CHECK(vmBridgePath == cppBridgePath);
    CHECK(nativeBridgePath == cppBridgePath);
  }

  const auto *cppLocalAuto = findSemanticEntry(primec::semanticProgramLocalAutoFactView(cppConformance.output.semanticProgram),
      [](const primec::SemanticProgramLocalAutoFact &entry) {
        return entry.scopePath == "/main" && entry.bindingName == "selected";
      });
  const auto *vmLocalAuto = findSemanticEntry(primec::semanticProgramLocalAutoFactView(vmConformance.output.semanticProgram),
      [](const primec::SemanticProgramLocalAutoFact &entry) {
        return entry.scopePath == "/main" && entry.bindingName == "selected";
      });
  const auto *nativeLocalAuto = findSemanticEntry(primec::semanticProgramLocalAutoFactView(nativeConformance.output.semanticProgram),
      [](const primec::SemanticProgramLocalAutoFact &entry) {
        return entry.scopePath == "/main" && entry.bindingName == "selected";
      });
  REQUIRE(cppLocalAuto != nullptr);
  REQUIRE(vmLocalAuto != nullptr);
  REQUIRE(nativeLocalAuto != nullptr);
  CHECK(primec::semanticProgramLocalAutoFactInitializerResolvedPath(
            cppConformance.output.semanticProgram, *cppLocalAuto) == "/lookup");
  CHECK(cppLocalAuto->initializerHasTry);
  CHECK(cppLocalAuto->initializerTryValueType == "int");
  CHECK(vmLocalAuto->bindingTypeText == cppLocalAuto->bindingTypeText);
  CHECK(nativeLocalAuto->bindingTypeText == cppLocalAuto->bindingTypeText);

  const auto *cppQuery = findSemanticEntry(primec::semanticProgramQueryFactView(cppConformance.output.semanticProgram),
      [&cppConformance](const primec::SemanticProgramQueryFact &entry) {
        return entry.scopePath == "/main" &&
               primec::semanticProgramQueryFactResolvedPath(cppConformance.output.semanticProgram, entry) ==
                   "/lookup";
      });
  const auto *vmQuery = findSemanticEntry(primec::semanticProgramQueryFactView(vmConformance.output.semanticProgram),
      [&vmConformance](const primec::SemanticProgramQueryFact &entry) {
        return entry.scopePath == "/main" &&
               primec::semanticProgramQueryFactResolvedPath(vmConformance.output.semanticProgram, entry) ==
                   "/lookup";
      });
  const auto *nativeQuery = findSemanticEntry(primec::semanticProgramQueryFactView(nativeConformance.output.semanticProgram),
      [&nativeConformance](const primec::SemanticProgramQueryFact &entry) {
        return entry.scopePath == "/main" &&
               primec::semanticProgramQueryFactResolvedPath(nativeConformance.output.semanticProgram, entry) ==
                   "/lookup";
      });
  REQUIRE(cppQuery != nullptr);
  REQUIRE(vmQuery != nullptr);
  REQUIRE(nativeQuery != nullptr);
  CHECK(cppQuery->bindingTypeText == "i64");
  CHECK(cppQuery->resultValueType == "int");
  CHECK(cppQuery->resultErrorType == "i32");
  CHECK(vmQuery->bindingTypeText == cppQuery->bindingTypeText);
  CHECK(nativeQuery->bindingTypeText == cppQuery->bindingTypeText);

  const auto *cppTry = findSemanticEntry(primec::semanticProgramTryFactView(cppConformance.output.semanticProgram),
      [&cppConformance](const primec::SemanticProgramTryFact &entry) {
        return entry.scopePath == "/main" &&
               primec::semanticProgramTryFactOperandResolvedPath(cppConformance.output.semanticProgram, entry) ==
                   "/lookup";
      });
  const auto *vmTry = findSemanticEntry(primec::semanticProgramTryFactView(vmConformance.output.semanticProgram),
      [&vmConformance](const primec::SemanticProgramTryFact &entry) {
        return entry.scopePath == "/main" &&
               primec::semanticProgramTryFactOperandResolvedPath(vmConformance.output.semanticProgram, entry) ==
                   "/lookup";
      });
  const auto *nativeTry = findSemanticEntry(primec::semanticProgramTryFactView(nativeConformance.output.semanticProgram),
      [&nativeConformance](const primec::SemanticProgramTryFact &entry) {
        return entry.scopePath == "/main" &&
               primec::semanticProgramTryFactOperandResolvedPath(nativeConformance.output.semanticProgram, entry) ==
                   "/lookup";
      });
  REQUIRE(cppTry != nullptr);
  REQUIRE(vmTry != nullptr);
  REQUIRE(nativeTry != nullptr);
  CHECK(cppTry->valueType == "int");
  CHECK(cppTry->errorType == "i32");
  CHECK(cppTry->onErrorHandlerPath == "/unexpectedError");
  CHECK(vmTry->valueType == cppTry->valueType);
  CHECK(nativeTry->valueType == cppTry->valueType);

  const auto *cppOnError = findSemanticEntry(primec::semanticProgramOnErrorFactView(cppConformance.output.semanticProgram),
      [](const primec::SemanticProgramOnErrorFact &entry) {
        return entry.definitionPath == "/main";
      });
  const auto *vmOnError = findSemanticEntry(primec::semanticProgramOnErrorFactView(vmConformance.output.semanticProgram),
      [](const primec::SemanticProgramOnErrorFact &entry) {
        return entry.definitionPath == "/main";
      });
  const auto *nativeOnError = findSemanticEntry(primec::semanticProgramOnErrorFactView(nativeConformance.output.semanticProgram),
      [](const primec::SemanticProgramOnErrorFact &entry) {
        return entry.definitionPath == "/main";
      });
  REQUIRE(cppOnError != nullptr);
  REQUIRE(vmOnError != nullptr);
  REQUIRE(nativeOnError != nullptr);
  CHECK(cppOnError->returnKind == "i32");
  CHECK(primec::semanticProgramOnErrorFactHandlerPath(cppConformance.output.semanticProgram, *cppOnError) ==
        "/unexpectedError");
  CHECK(primec::semanticProgramOnErrorFactHandlerPath(vmConformance.output.semanticProgram, *vmOnError) ==
        primec::semanticProgramOnErrorFactHandlerPath(cppConformance.output.semanticProgram, *cppOnError));
  CHECK(primec::semanticProgramOnErrorFactHandlerPath(nativeConformance.output.semanticProgram, *nativeOnError) ==
        primec::semanticProgramOnErrorFactHandlerPath(cppConformance.output.semanticProgram, *cppOnError));

  const auto *cppReturn = findSemanticEntry(primec::semanticProgramReturnFactView(cppConformance.output.semanticProgram),
      [&cppConformance](const primec::SemanticProgramReturnFact &entry) {
        return primec::semanticProgramReturnFactDefinitionPath(
                   cppConformance.output.semanticProgram, entry) == "/main";
      });
  const auto *vmReturn = findSemanticEntry(primec::semanticProgramReturnFactView(vmConformance.output.semanticProgram),
      [&vmConformance](const primec::SemanticProgramReturnFact &entry) {
        return primec::semanticProgramReturnFactDefinitionPath(
                   vmConformance.output.semanticProgram, entry) == "/main";
      });
  const auto *nativeReturn = findSemanticEntry(primec::semanticProgramReturnFactView(nativeConformance.output.semanticProgram),
      [&nativeConformance](const primec::SemanticProgramReturnFact &entry) {
        return primec::semanticProgramReturnFactDefinitionPath(
                   nativeConformance.output.semanticProgram, entry) == "/main";
      });
  REQUIRE(cppReturn != nullptr);
  REQUIRE(vmReturn != nullptr);
  REQUIRE(nativeReturn != nullptr);
  CHECK(cppReturn->bindingTypeText == "i32");
  CHECK(vmReturn->bindingTypeText == cppReturn->bindingTypeText);
  CHECK(nativeReturn->bindingTypeText == cppReturn->bindingTypeText);

  CHECK(cppConformance.backendKind == "cpp-ir");
  CHECK(vmConformance.backendKind == "vm");
  CHECK(nativeConformance.backendKind == "native");
  CHECK(cppConformance.emitResult.exitCode == 0);
  CHECK(vmConformance.emitResult.exitCode == 39);
  CHECK(nativeConformance.emitResult.exitCode == 0);

  const std::string cpp = readTextFile(cppConformance.outputPath);
  CHECK(cpp.find("static int64_t ps_fn_0") != std::string::npos);
  CHECK(std::filesystem::exists(nativeConformance.outputPath));
  CHECK(std::filesystem::is_regular_file(nativeConformance.outputPath));
  CHECK(std::filesystem::file_size(nativeConformance.outputPath) > 0);
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

TEST_CASE("compile pipeline skips semantic product for ast-semantic dumps") {
  const std::filesystem::path tempPath = makeTempIrPipelineSourcePath();
  {
    std::ofstream file(tempPath);
    REQUIRE(file.good());
    file << R"(namespace bench {
[i32]
main() {
  return(0)
}
}
)";
  }

  primec::Options options;
  options.inputPath = tempPath.string();
  options.entryPath = "/bench/main";
  options.emitKind = "native";
  options.dumpStage = "ast-semantic";
  options.collectDiagnostics = true;
  primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);

  primec::CompilePipelineOutput output;
  primec::CompilePipelineDiagnosticInfo diagnosticInfo;
  primec::CompilePipelineErrorStage errorStage = primec::CompilePipelineErrorStage::None;
  std::string error;
  const bool ok = primec::runCompilePipeline(options, output, errorStage, error, &diagnosticInfo);

  std::error_code ec;
  std::filesystem::remove(tempPath, ec);

  REQUIRE(ok);
  CHECK(output.hasDumpOutput);
  CHECK(errorStage == primec::CompilePipelineErrorStage::None);
  CHECK_FALSE(output.hasFailure);
  CHECK(output.semanticProductRequested == false);
  CHECK(output.semanticProductBuilt == false);
  CHECK(output.semanticProductDecision ==
        primec::CompilePipelineSemanticProductDecision::SkipForAstSemanticDump);
  CHECK_FALSE(output.hasSemanticProgram);
  CHECK(output.dumpOutput.find("ast {") != std::string::npos);
  CHECK(output.dumpOutput.find("/bench/main()") != std::string::npos);
}

TEST_CASE("compile pipeline builds semantic product for semantic-product dumps") {
  const std::filesystem::path tempPath = makeTempIrPipelineSourcePath();
  {
    std::ofstream file(tempPath);
    REQUIRE(file.good());
    file << R"(namespace bench {
[void]
callee() {}
[i32]
main() {
  callee()
  return(0)
}
}
)";
  }

  primec::Options options;
  options.inputPath = tempPath.string();
  options.entryPath = "/bench/main";
  options.emitKind = "native";
  options.dumpStage = "semantic-product";
  options.collectDiagnostics = true;
  primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);

  primec::CompilePipelineOutput output;
  primec::CompilePipelineDiagnosticInfo diagnosticInfo;
  primec::CompilePipelineErrorStage errorStage = primec::CompilePipelineErrorStage::None;
  std::string error;
  const bool ok = primec::runCompilePipeline(options, output, errorStage, error, &diagnosticInfo);

  std::error_code ec;
  std::filesystem::remove(tempPath, ec);

  REQUIRE(ok);
  CHECK(output.hasDumpOutput);
  CHECK(errorStage == primec::CompilePipelineErrorStage::None);
  CHECK_FALSE(output.hasFailure);
  CHECK(output.semanticProductRequested);
  CHECK(output.semanticProductBuilt);
  CHECK(output.semanticProductDecision ==
        primec::CompilePipelineSemanticProductDecision::RequireForConsumingPath);
  REQUIRE(output.hasSemanticProgram);
  CHECK(output.semanticProgram.entryPath == "/bench/main");
  CHECK(output.dumpOutput.find("semantic_product {") != std::string::npos);
  CHECK(output.dumpOutput.find("direct_call_targets[0]:") != std::string::npos);
}

TEST_CASE("compile pipeline keeps semantic product for emit paths") {
  const std::filesystem::path tempPath = makeTempIrPipelineSourcePath();
  {
    std::ofstream file(tempPath);
    REQUIRE(file.good());
    file << R"(namespace bench {
[i32]
main() {
  return(0)
}
}
)";
  }

  primec::Options options;
  options.inputPath = tempPath.string();
  options.entryPath = "/bench/main";
  options.emitKind = "vm";
  options.collectDiagnostics = true;
  primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);

  primec::CompilePipelineOutput output;
  primec::CompilePipelineDiagnosticInfo diagnosticInfo;
  primec::CompilePipelineErrorStage errorStage = primec::CompilePipelineErrorStage::None;
  std::string error;
  const bool ok = primec::runCompilePipeline(options, output, errorStage, error, &diagnosticInfo);

  std::error_code ec;
  std::filesystem::remove(tempPath, ec);

  REQUIRE(ok);
  CHECK_FALSE(output.hasDumpOutput);
  CHECK(errorStage == primec::CompilePipelineErrorStage::None);
  CHECK_FALSE(output.hasFailure);
  CHECK(output.semanticProductRequested);
  CHECK(output.semanticProductBuilt);
  CHECK(output.semanticProductDecision ==
        primec::CompilePipelineSemanticProductDecision::RequireForConsumingPath);
  REQUIRE(output.hasSemanticProgram);
  CHECK(output.semanticProgram.entryPath == "/bench/main");
}

TEST_CASE("compile pipeline explicit skip mode omits semantic product for non-consuming paths") {
  const std::filesystem::path tempPath = makeTempIrPipelineSourcePath();
  {
    std::ofstream file(tempPath);
    REQUIRE(file.good());
    file << R"(namespace bench {
[i32]
main() {
  return(0)
}
}
)";
  }

  primec::Options options;
  options.inputPath = tempPath.string();
  options.entryPath = "/bench/main";
  options.emitKind = "vm";
  options.skipSemanticProductForNonConsumingPath = true;
  options.collectDiagnostics = true;
  primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);

  primec::CompilePipelineOutput output;
  primec::CompilePipelineDiagnosticInfo diagnosticInfo;
  primec::CompilePipelineErrorStage errorStage = primec::CompilePipelineErrorStage::None;
  std::string error;
  const bool ok = primec::runCompilePipeline(options, output, errorStage, error, &diagnosticInfo);

  std::error_code ec;
  std::filesystem::remove(tempPath, ec);

  REQUIRE(ok);
  CHECK_FALSE(output.hasDumpOutput);
  CHECK(errorStage == primec::CompilePipelineErrorStage::None);
  CHECK_FALSE(output.hasFailure);
  CHECK_FALSE(output.semanticProductRequested);
  CHECK_FALSE(output.semanticProductBuilt);
  CHECK(output.semanticProductDecision ==
        primec::CompilePipelineSemanticProductDecision::SkipForNonConsumingPath);
  CHECK_FALSE(output.hasSemanticProgram);
}

TEST_CASE("ir pipeline helper parseAndValidate skips semantic product when not requested") {
  const std::string source = R"(import /std/math/Vec2

[i32]
main() {
  return(0i32)
}
)";

  primec::Program program;
  std::string error;
  const bool ok = parseAndValidate(source, program, error, {"io_out", "io_err"});
  INFO(error);
  REQUIRE(ok);
  CHECK(error.empty());
  CHECK_FALSE(program.definitions.empty());
}

TEST_CASE("compile pipeline explicit skip mode rejects semantic-product dump requests") {
  const std::filesystem::path tempPath = makeTempIrPipelineSourcePath();
  {
    std::ofstream file(tempPath);
    REQUIRE(file.good());
    file << R"(namespace bench {
[return<i32>]
main() {
  return(0i32)
}
}
)";
  }

  primec::Options options;
  options.inputPath = tempPath.string();
  options.entryPath = "/bench/main";
  options.emitKind = "vm";
  options.dumpStage = "semantic-product";
  options.skipSemanticProductForNonConsumingPath = true;
  primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);

  primec::CompilePipelineOutput output;
  primec::CompilePipelineErrorStage errorStage = primec::CompilePipelineErrorStage::None;
  std::string error;
  const bool ok = primec::runCompilePipeline(options, output, errorStage, error);

  std::error_code ec;
  std::filesystem::remove(tempPath, ec);

  CHECK_FALSE(ok);
  CHECK(errorStage == primec::CompilePipelineErrorStage::Semantic);
  CHECK(error == "semantic-product dump requested without semantic product");
  CHECK(output.semanticProductDecision ==
        primec::CompilePipelineSemanticProductDecision::SkipForNonConsumingPath);
  CHECK_FALSE(output.hasSemanticProgram);
}

TEST_CASE("compile pipeline benchmark force-on keeps semantic product for ast-semantic dumps") {
  const std::filesystem::path tempPath = makeTempIrPipelineSourcePath();
  {
    std::ofstream file(tempPath);
    REQUIRE(file.good());
    file << R"(namespace bench {
[return<i32>]
main() {
  return(0i32)
}
}
)";
  }

  primec::Options options;
  options.inputPath = tempPath.string();
  options.entryPath = "/bench/main";
  options.emitKind = "native";
  options.dumpStage = "ast-semantic";
  options.benchmarkForceSemanticProduct = true;
  primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);

  primec::CompilePipelineOutput output;
  primec::CompilePipelineErrorStage errorStage = primec::CompilePipelineErrorStage::None;
  std::string error;
  const bool ok = primec::runCompilePipeline(options, output, errorStage, error);

  std::error_code ec;
  std::filesystem::remove(tempPath, ec);

  REQUIRE(ok);
  CHECK(output.hasDumpOutput);
  CHECK(output.semanticProductRequested);
  CHECK(output.semanticProductBuilt);
  CHECK(output.semanticProductDecision ==
        primec::CompilePipelineSemanticProductDecision::ForcedOnForBenchmark);
  CHECK(output.hasSemanticProgram);
}

TEST_CASE("compile pipeline benchmark force-off rejects semantic-product dump") {
  const std::filesystem::path tempPath = makeTempIrPipelineSourcePath();
  {
    std::ofstream file(tempPath);
    REQUIRE(file.good());
    file << R"(namespace bench {
[return<i32>]
main() {
  return(0i32)
}
}
)";
  }

  primec::Options options;
  options.inputPath = tempPath.string();
  options.entryPath = "/bench/main";
  options.emitKind = "native";
  options.dumpStage = "semantic-product";
  options.benchmarkForceSemanticProduct = false;
  primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);

  primec::CompilePipelineOutput output;
  primec::CompilePipelineErrorStage errorStage = primec::CompilePipelineErrorStage::None;
  std::string error;
  const bool ok = primec::runCompilePipeline(options, output, errorStage, error);

  std::error_code ec;
  std::filesystem::remove(tempPath, ec);

  CHECK_FALSE(ok);
  CHECK(errorStage == primec::CompilePipelineErrorStage::Semantic);
  CHECK(error == "semantic-product dump requested without semantic product");
  CHECK(output.semanticProductDecision ==
        primec::CompilePipelineSemanticProductDecision::ForcedOffForBenchmark);
  CHECK_FALSE(output.hasSemanticProgram);
}

TEST_CASE("compile pipeline benchmark no-fact-emission keeps semantic product shells empty") {
  const std::filesystem::path tempPath = makeTempIrPipelineSourcePath();
  {
    std::ofstream file(tempPath);
    REQUIRE(file.good());
    file << R"(namespace bench {
[void]
callee() {}
[return<i32>]
main() {
  callee()
  return(0i32)
}
}
)";
  }

  primec::Options options;
  options.inputPath = tempPath.string();
  options.entryPath = "/bench/main";
  options.emitKind = "native";
  options.dumpStage = "semantic-product";
  options.benchmarkSemanticNoFactEmission = true;
  primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);

  primec::CompilePipelineOutput output;
  primec::CompilePipelineErrorStage errorStage = primec::CompilePipelineErrorStage::None;
  std::string error;
  const bool ok = primec::runCompilePipeline(options, output, errorStage, error);

  std::error_code ec;
  std::filesystem::remove(tempPath, ec);

  REQUIRE(ok);
  REQUIRE(output.hasSemanticProgram);
  CHECK_FALSE(output.hasSemanticPhaseCounters);
  CHECK(output.semanticProgram.directCallTargets.empty());
  CHECK(output.semanticProgram.callableSummaries.empty());
  CHECK(output.semanticProgram.bindingFacts.empty());
  CHECK(output.semanticProgram.queryFacts.empty());
}

TEST_CASE("compile pipeline ast-semantic phase counters prove semantic-product build skip") {
  const std::filesystem::path tempPath = makeTempIrPipelineSourcePath();
  {
    std::ofstream file(tempPath);
    REQUIRE(file.good());
    file << R"(namespace bench {
[return<i32>]
callee([i32] value) {
  return(value)
}
[return<i32>]
main() {
  [i32 mut] value{4i32}
  return(callee(value))
}
}
)";
  }

  primec::Options options;
  options.inputPath = tempPath.string();
  options.entryPath = "/bench/main";
  options.emitKind = "native";
  options.dumpStage = "ast-semantic";
  options.benchmarkSemanticPhaseCounters = true;
  primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);

  primec::CompilePipelineOutput output;
  primec::CompilePipelineErrorStage errorStage = primec::CompilePipelineErrorStage::None;
  std::string error;
  const bool ok = primec::runCompilePipeline(options, output, errorStage, error);

  std::error_code ec;
  std::filesystem::remove(tempPath, ec);

  REQUIRE(ok);
  CHECK(error.empty());
  CHECK(output.semanticProductDecision ==
        primec::CompilePipelineSemanticProductDecision::SkipForAstSemanticDump);
  CHECK_FALSE(output.semanticProductRequested);
  CHECK_FALSE(output.semanticProductBuilt);
  CHECK_FALSE(output.hasSemanticProgram);
  REQUIRE(output.hasSemanticPhaseCounters);
  CHECK(output.semanticPhaseCounters.validation.callsVisited > 0);
  CHECK(output.semanticPhaseCounters.semanticProductBuild.callsVisited == 0);
  CHECK(output.semanticPhaseCounters.semanticProductBuild.factsProduced == 0);
}

TEST_CASE("compile pipeline benchmark semantic phase counters are opt-in and populated") {
  const std::filesystem::path tempPath = makeTempIrPipelineSourcePath();
  {
    std::ofstream file(tempPath);
    REQUIRE(file.good());
    file << R"(namespace bench {
[return<i32>]
callee([i32] value) {
  return(value)
}
[return<i32>]
main() {
  [i32 mut] value{4i32}
  return(callee(value))
}
}
)";
  }

  primec::Options baseOptions;
  baseOptions.inputPath = tempPath.string();
  baseOptions.entryPath = "/bench/main";
  baseOptions.emitKind = "native";
  baseOptions.dumpStage = "semantic-product";
  primec::addDefaultStdlibInclude(baseOptions.inputPath, baseOptions.importPaths);

  primec::CompilePipelineOutput defaultOutput;
  primec::CompilePipelineErrorStage defaultErrorStage = primec::CompilePipelineErrorStage::None;
  std::string defaultError;
  const bool defaultOk =
      primec::runCompilePipeline(baseOptions, defaultOutput, defaultErrorStage, defaultError);
  REQUIRE(defaultOk);
  CHECK(defaultError.empty());
  CHECK_FALSE(defaultOutput.hasSemanticPhaseCounters);

  primec::Options countersOptions = baseOptions;
  countersOptions.benchmarkSemanticPhaseCounters = true;

  primec::CompilePipelineOutput countersOutput;
  primec::CompilePipelineErrorStage countersErrorStage = primec::CompilePipelineErrorStage::None;
  std::string countersError;
  const bool countersOk =
      primec::runCompilePipeline(countersOptions, countersOutput, countersErrorStage, countersError);

  std::error_code ec;
  std::filesystem::remove(tempPath, ec);

  REQUIRE(countersOk);
  CHECK(countersError.empty());
  REQUIRE(countersOutput.hasSemanticPhaseCounters);
  CHECK(countersOutput.semanticPhaseCounters.validation.callsVisited > 0);
  CHECK(countersOutput.semanticPhaseCounters.validation.peakLocalMapSize > 0);
  CHECK(countersOutput.semanticPhaseCounters.validation.factsProduced == 0);
  CHECK(countersOutput.semanticPhaseCounters.semanticProductBuild.callsVisited > 0);
  CHECK(countersOutput.semanticPhaseCounters.semanticProductBuild.peakLocalMapSize == 0);
  CHECK(countersOutput.semanticPhaseCounters.semanticProductBuild.factsProduced > 0);
}

TEST_CASE("compile pipeline benchmark semantic allocation counters are opt-in and populated") {
  const std::filesystem::path tempPath = makeTempIrPipelineSourcePath();
  {
    std::ofstream file(tempPath);
    REQUIRE(file.good());
    file << R"(namespace bench {
[return<i32>]
callee([i32] value) {
  return(value)
}
[return<i32>]
main() {
  [i32 mut] value{4i32}
  return(callee(value))
}
}
)";
  }

  primec::Options options;
  options.inputPath = tempPath.string();
  options.entryPath = "/bench/main";
  options.emitKind = "native";
  options.dumpStage = "semantic-product";
  options.benchmarkSemanticAllocationCounters = true;
  primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);

  primec::CompilePipelineOutput output;
  primec::CompilePipelineErrorStage errorStage = primec::CompilePipelineErrorStage::None;
  std::string error;
  const bool ok = primec::runCompilePipeline(options, output, errorStage, error);

  std::error_code ec;
  std::filesystem::remove(tempPath, ec);

  REQUIRE(ok);
  CHECK(error.empty());
  REQUIRE(output.hasSemanticPhaseCounters);
  CHECK(output.semanticPhaseCounters.validation.allocationCount > 0);
  CHECK(output.semanticPhaseCounters.validation.allocatedBytes > 0);
  CHECK(output.semanticPhaseCounters.semanticProductBuild.allocationCount > 0);
  CHECK(output.semanticPhaseCounters.semanticProductBuild.allocatedBytes > 0);
}

TEST_CASE("compile pipeline benchmark semantic rss checkpoints are opt-in and populated") {
  const std::filesystem::path tempPath = makeTempIrPipelineSourcePath();
  {
    std::ofstream file(tempPath);
    REQUIRE(file.good());
    file << R"(namespace bench {
[return<i32>]
callee([i32] value) {
  return(value)
}
[return<i32>]
main() {
  [i32 mut] value{4i32}
  return(callee(value))
}
}
)";
  }

  primec::Options options;
  options.inputPath = tempPath.string();
  options.entryPath = "/bench/main";
  options.emitKind = "native";
  options.dumpStage = "semantic-product";
  options.benchmarkSemanticRssCheckpoints = true;
  primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);

  primec::CompilePipelineOutput output;
  primec::CompilePipelineErrorStage errorStage = primec::CompilePipelineErrorStage::None;
  std::string error;
  const bool ok = primec::runCompilePipeline(options, output, errorStage, error);

  std::error_code ec;
  std::filesystem::remove(tempPath, ec);

  REQUIRE(ok);
  CHECK(error.empty());
  REQUIRE(output.hasSemanticPhaseCounters);
  checkOptionalRssCheckpointSnapshot(output.semanticPhaseCounters.validation);
  checkOptionalRssCheckpointSnapshot(output.semanticPhaseCounters.semanticProductBuild);
}

TEST_CASE("compile pipeline benchmark fact-family allowlist keeps only selected collectors") {
  const std::filesystem::path tempPath = makeTempIrPipelineSourcePath();
  {
    std::ofstream file(tempPath);
    REQUIRE(file.good());
    file << R"(namespace bench {
[void]
callee() {}
[return<i32>]
main() {
  callee()
  return(0i32)
}
}
)";
  }

  primec::Options options;
  options.inputPath = tempPath.string();
  options.entryPath = "/bench/main";
  options.emitKind = "native";
  options.dumpStage = "semantic-product";
  options.benchmarkSemanticFactFamiliesSpecified = true;
  options.benchmarkSemanticFactFamilies = {"callable_summaries"};
  primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);

  primec::CompilePipelineOutput output;
  primec::CompilePipelineErrorStage errorStage = primec::CompilePipelineErrorStage::None;
  std::string error;
  const bool ok = primec::runCompilePipeline(options, output, errorStage, error);

  std::error_code ec;
  std::filesystem::remove(tempPath, ec);

  REQUIRE(ok);
  REQUIRE(output.hasSemanticProgram);
  CHECK_FALSE(output.semanticProgram.callableSummaries.empty());
  CHECK(output.semanticProgram.directCallTargets.empty());
  CHECK(output.semanticProgram.bindingFacts.empty());
  CHECK(output.semanticProgram.returnFacts.empty());
  CHECK(output.semanticProgram.queryFacts.empty());
}

TEST_CASE("compile pipeline direct and bridge collector merge keeps output-order parity") {
  const std::filesystem::path tempPath = makeTempIrPipelineSourcePath();
  {
    std::ofstream file(tempPath);
    REQUIRE(file.good());
    file << R"(namespace bench {
[void]
callee() {}
[void]
main() {
  [vector<i32>] values{vector<i32>()}
  callee()
  [i32] countA{count(values)}
  [i32] countB{count(values)}
  [i32] capA{capacity(values)}
}
}
)";
  }

  const auto runWithFamilies = [&](const std::vector<std::string> &families) {
    primec::Options options;
    options.inputPath = tempPath.string();
    options.entryPath = "/bench/main";
    options.emitKind = "native";
    options.dumpStage = "semantic-product";
    options.benchmarkSemanticFactFamiliesSpecified = true;
    options.benchmarkSemanticFactFamilies = families;
    primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);

    primec::CompilePipelineOutput output;
    primec::CompilePipelineErrorStage errorStage = primec::CompilePipelineErrorStage::None;
    std::string error;
    const bool ok = primec::runCompilePipeline(options, output, errorStage, error);
    REQUIRE(ok);
    CHECK(error.empty());
    REQUIRE(output.hasSemanticProgram);
    return output;
  };

  const primec::CompilePipelineOutput combinedOutput =
      runWithFamilies({"direct_call_targets", "bridge_path_choices"});
  const primec::CompilePipelineOutput directOnlyOutput =
      runWithFamilies({"direct_call_targets"});
  const primec::CompilePipelineOutput bridgeOnlyOutput =
      runWithFamilies({"bridge_path_choices"});

  std::error_code ec;
  std::filesystem::remove(tempPath, ec);

  const auto combinedDirectTargets =
      primec::semanticProgramDirectCallTargetView(combinedOutput.semanticProgram);
  const auto directOnlyTargets =
      primec::semanticProgramDirectCallTargetView(directOnlyOutput.semanticProgram);
  REQUIRE(combinedDirectTargets.size() == directOnlyTargets.size());
  for (std::size_t i = 0; i < combinedDirectTargets.size(); ++i) {
    const auto *combinedEntry = combinedDirectTargets[i];
    const auto *directOnlyEntry = directOnlyTargets[i];
    REQUIRE(combinedEntry != nullptr);
    REQUIRE(directOnlyEntry != nullptr);
    CHECK(combinedEntry->scopePath == directOnlyEntry->scopePath);
    CHECK(combinedEntry->callName == directOnlyEntry->callName);
    CHECK(primec::semanticProgramDirectCallTargetResolvedPath(combinedOutput.semanticProgram, *combinedEntry) ==
          primec::semanticProgramDirectCallTargetResolvedPath(directOnlyOutput.semanticProgram, *directOnlyEntry));
  }

  const auto combinedBridgeChoices =
      primec::semanticProgramBridgePathChoiceView(combinedOutput.semanticProgram);
  const auto bridgeOnlyChoices =
      primec::semanticProgramBridgePathChoiceView(bridgeOnlyOutput.semanticProgram);
  REQUIRE(combinedBridgeChoices.size() == bridgeOnlyChoices.size());
  for (std::size_t i = 0; i < combinedBridgeChoices.size(); ++i) {
    const auto *combinedEntry = combinedBridgeChoices[i];
    const auto *bridgeOnlyEntry = bridgeOnlyChoices[i];
    REQUIRE(combinedEntry != nullptr);
    REQUIRE(bridgeOnlyEntry != nullptr);
    CHECK(combinedEntry->scopePath == bridgeOnlyEntry->scopePath);
    CHECK(combinedEntry->collectionFamily == bridgeOnlyEntry->collectionFamily);
    CHECK(primec::semanticProgramBridgePathChoiceHelperName(combinedOutput.semanticProgram, *combinedEntry) ==
          primec::semanticProgramBridgePathChoiceHelperName(bridgeOnlyOutput.semanticProgram, *bridgeOnlyEntry));
    CHECK(primec::semanticProgramResolveCallTargetString(combinedOutput.semanticProgram, combinedEntry->chosenPathId) ==
          primec::semanticProgramResolveCallTargetString(bridgeOnlyOutput.semanticProgram,
                                                         bridgeOnlyEntry->chosenPathId));
  }
}

TEST_CASE("compile pipeline benchmark worker-count stress keeps /std/math/* semantic-product dumps deterministic") {
  constexpr std::size_t localDefinitionCount = 64;
  const std::filesystem::path tempPath = makeTempIrPipelineSourcePath();
  {
    std::ofstream file(tempPath);
    REQUIRE(file.good());
    file << buildMathStressSemanticSource(localDefinitionCount);
  }

  struct StressSnapshot {
    std::string dumpOutput;
    std::size_t definitionCount = 0;
    std::size_t callableSummaryCount = 0;
    std::size_t directCallTargetCount = 0;
  };

  const auto runStress = [&]() {
    primec::Options options;
    options.inputPath = tempPath.string();
    options.entryPath = "/main";
    options.emitKind = "native";
    options.dumpStage = "semantic-product";
    options.collectDiagnostics = true;
    options.benchmarkSemanticDefinitionValidationWorkerCount = 4;
    primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);

    primec::CompilePipelineOutput output;
    primec::CompilePipelineDiagnosticInfo diagnosticInfo;
    primec::CompilePipelineErrorStage errorStage = primec::CompilePipelineErrorStage::None;
    std::string error;
    const bool ok = primec::runCompilePipeline(options, output, errorStage, error, &diagnosticInfo);

    REQUIRE(ok);
    CHECK(error.empty());
    CHECK(errorStage == primec::CompilePipelineErrorStage::None);
    CHECK_FALSE(output.hasFailure);
    CHECK(output.semanticProductRequested);
    CHECK(output.semanticProductBuilt);
    REQUIRE(output.hasSemanticProgram);
    REQUIRE(output.hasDumpOutput);
    CHECK(std::find(output.program.imports.begin(),
                    output.program.imports.end(),
                    "/std/math/*") != output.program.imports.end());
    CHECK(output.program.definitions.size() >= localDefinitionCount + 1);
    CHECK(output.semanticProgram.callableSummaries.size() >= localDefinitionCount + 1);
    CHECK(output.semanticProgram.directCallTargets.size() >= localDefinitionCount);

    StressSnapshot snapshot;
    snapshot.dumpOutput = output.dumpOutput;
    snapshot.definitionCount = output.program.definitions.size();
    snapshot.callableSummaryCount = output.semanticProgram.callableSummaries.size();
    snapshot.directCallTargetCount = output.semanticProgram.directCallTargets.size();
    return snapshot;
  };

  const StressSnapshot firstRun = runStress();
  const StressSnapshot secondRun = runStress();

  std::error_code ec;
  std::filesystem::remove(tempPath, ec);

  CHECK(firstRun.dumpOutput == secondRun.dumpOutput);
  CHECK(firstRun.definitionCount == secondRun.definitionCount);
  CHECK(firstRun.callableSummaryCount == secondRun.callableSummaryCount);
  CHECK(firstRun.directCallTargetCount == secondRun.directCallTargetCount);
}

TEST_CASE("compile pipeline benchmark worker-count equivalence keeps /std/math/* semantic-product output stable across 1,2,4 workers") {
  constexpr std::size_t localDefinitionCount = 64;
  const std::filesystem::path tempPath = makeTempIrPipelineSourcePath();
  {
    std::ofstream file(tempPath);
    REQUIRE(file.good());
    file << buildMathStressSemanticSource(localDefinitionCount);
  }

  struct StressSnapshot {
    std::string dumpOutput;
    std::size_t definitionCount = 0;
    std::size_t callableSummaryCount = 0;
    std::size_t directCallTargetCount = 0;
  };

  const auto runWithWorkerCount = [&](int workerCount) {
    primec::Options options;
    options.inputPath = tempPath.string();
    options.entryPath = "/main";
    options.emitKind = "native";
    options.dumpStage = "semantic-product";
    options.collectDiagnostics = true;
    options.benchmarkSemanticDefinitionValidationWorkerCount = workerCount;
    primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);

    primec::CompilePipelineOutput output;
    primec::CompilePipelineDiagnosticInfo diagnosticInfo;
    primec::CompilePipelineErrorStage errorStage = primec::CompilePipelineErrorStage::None;
    std::string error;
    const bool ok = primec::runCompilePipeline(options, output, errorStage, error, &diagnosticInfo);

    REQUIRE(ok);
    CHECK(error.empty());
    CHECK(errorStage == primec::CompilePipelineErrorStage::None);
    CHECK_FALSE(output.hasFailure);
    CHECK(output.semanticProductRequested);
    CHECK(output.semanticProductBuilt);
    REQUIRE(output.hasSemanticProgram);
    REQUIRE(output.hasDumpOutput);
    CHECK(std::find(output.program.imports.begin(),
                    output.program.imports.end(),
                    "/std/math/*") != output.program.imports.end());
    CHECK(output.program.definitions.size() >= localDefinitionCount + 1);
    CHECK(output.semanticProgram.callableSummaries.size() >= localDefinitionCount + 1);
    CHECK(output.semanticProgram.directCallTargets.size() >= localDefinitionCount);

    StressSnapshot snapshot;
    snapshot.dumpOutput = output.dumpOutput;
    snapshot.definitionCount = output.program.definitions.size();
    snapshot.callableSummaryCount = output.semanticProgram.callableSummaries.size();
    snapshot.directCallTargetCount = output.semanticProgram.directCallTargets.size();
    return snapshot;
  };

  const StressSnapshot singleWorker = runWithWorkerCount(1);
  const StressSnapshot twoWorkers = runWithWorkerCount(2);
  const StressSnapshot fourWorkers = runWithWorkerCount(4);

  std::error_code ec;
  std::filesystem::remove(tempPath, ec);

  CHECK(singleWorker.dumpOutput == twoWorkers.dumpOutput);
  CHECK(singleWorker.dumpOutput == fourWorkers.dumpOutput);
  CHECK(twoWorkers.dumpOutput == fourWorkers.dumpOutput);
  CHECK(singleWorker.definitionCount == twoWorkers.definitionCount);
  CHECK(singleWorker.definitionCount == fourWorkers.definitionCount);
  CHECK(twoWorkers.definitionCount == fourWorkers.definitionCount);
  CHECK(singleWorker.callableSummaryCount == twoWorkers.callableSummaryCount);
  CHECK(singleWorker.callableSummaryCount == fourWorkers.callableSummaryCount);
  CHECK(twoWorkers.callableSummaryCount == fourWorkers.callableSummaryCount);
  CHECK(singleWorker.directCallTargetCount == twoWorkers.directCallTargetCount);
  CHECK(singleWorker.directCallTargetCount == fourWorkers.directCallTargetCount);
  CHECK(twoWorkers.directCallTargetCount == fourWorkers.directCallTargetCount);
}

TEST_CASE("compile pipeline benchmark worker-count stress keeps /std/math/* diagnostics deterministic") {
  constexpr std::size_t localDefinitionCount = 64;
  const std::filesystem::path tempPath = makeTempIrPipelineSourcePath();
  {
    std::ofstream file(tempPath);
    REQUIRE(file.good());
    file << buildMathStressDiagnosticSource(localDefinitionCount);
  }

  const auto runStress = [&]() {
    primec::Options options;
    options.inputPath = tempPath.string();
    options.entryPath = "/main";
    options.emitKind = "native";
    options.collectDiagnostics = true;
    options.benchmarkSemanticDefinitionValidationWorkerCount = 4;
    primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);

    primec::CompilePipelineOutput output;
    primec::CompilePipelineDiagnosticInfo diagnosticInfo;
    primec::CompilePipelineErrorStage errorStage = primec::CompilePipelineErrorStage::None;
    std::string error;
    const bool ok = primec::runCompilePipeline(options, output, errorStage, error, &diagnosticInfo);

    CHECK_FALSE(ok);
    CHECK(errorStage == primec::CompilePipelineErrorStage::Semantic);
    REQUIRE(output.hasFailure);
    CHECK(output.failure.stage == primec::CompilePipelineErrorStage::Semantic);
    CHECK(output.failure.message == error);
    CHECK_FALSE(output.failure.diagnosticInfo.records.empty());
    CHECK_FALSE(diagnosticInfo.records.empty());
    return compileDiagnosticMessages(output.failure.diagnosticInfo);
  };

  const std::vector<std::string> firstRunMessages = runStress();
  const std::vector<std::string> secondRunMessages = runStress();

  std::error_code ec;
  std::filesystem::remove(tempPath, ec);

  CHECK(firstRunMessages == secondRunMessages);
  CHECK(firstRunMessages.size() >= 2);
}

TEST_CASE("compile pipeline benchmark worker-count equivalence keeps /std/math/* diagnostics stable across 1,2,4 workers") {
  constexpr std::size_t localDefinitionCount = 64;
  const std::filesystem::path tempPath = makeTempIrPipelineSourcePath();
  {
    std::ofstream file(tempPath);
    REQUIRE(file.good());
    file << buildMathStressDiagnosticSource(localDefinitionCount);
  }

  const auto runWithWorkerCount = [&](int workerCount) {
    primec::Options options;
    options.inputPath = tempPath.string();
    options.entryPath = "/main";
    options.emitKind = "native";
    options.collectDiagnostics = true;
    options.benchmarkSemanticDefinitionValidationWorkerCount = workerCount;
    primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);

    primec::CompilePipelineOutput output;
    primec::CompilePipelineDiagnosticInfo diagnosticInfo;
    primec::CompilePipelineErrorStage errorStage = primec::CompilePipelineErrorStage::None;
    std::string error;
    const bool ok = primec::runCompilePipeline(options, output, errorStage, error, &diagnosticInfo);

    CHECK_FALSE(ok);
    CHECK(errorStage == primec::CompilePipelineErrorStage::Semantic);
    REQUIRE(output.hasFailure);
    CHECK(output.failure.stage == primec::CompilePipelineErrorStage::Semantic);
    CHECK(output.failure.message == error);
    CHECK_FALSE(output.failure.diagnosticInfo.records.empty());
    CHECK_FALSE(diagnosticInfo.records.empty());
    return compileDiagnosticMessages(output.failure.diagnosticInfo);
  };

  const std::vector<std::string> singleWorkerMessages = runWithWorkerCount(1);
  const std::vector<std::string> twoWorkerMessages = runWithWorkerCount(2);
  const std::vector<std::string> fourWorkerMessages = runWithWorkerCount(4);

  std::error_code ec;
  std::filesystem::remove(tempPath, ec);

  CHECK(singleWorkerMessages == twoWorkerMessages);
  CHECK(singleWorkerMessages == fourWorkerMessages);
  CHECK(twoWorkerMessages == fourWorkerMessages);
  CHECK(singleWorkerMessages.size() >= 2);
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
