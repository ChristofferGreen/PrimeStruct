#include "third_party/doctest.h"

#include "test_ir_pipeline_backends_registry_shared.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.backends.registry");

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

TEST_CASE("backend capability registry describes runtime capability availability") {
  auto signatureFor = [](const primec::IrBackendCapabilityProfile &profile) {
    std::string signature = profile.emitKind;
    signature += ":";
    if (std::string_view(profile.wasmProfile).empty()) {
      signature += "-";
    } else {
      signature += profile.wasmProfile;
    }
    signature += ":";
    signature += profile.targetName;
    signature += ":";
    std::vector<std::string_view> capabilities;
    const bool supportsGraphicsRuntime = primec::irBackendCapabilityProfileSupports(
        profile, primec::IrBackendCapability::GraphicsRuntimeSubstrate);
    if (supportsGraphicsRuntime) {
      capabilities.push_back("graphics-runtime");
    }
    const bool supportsRuntimeReflection = primec::irBackendCapabilityProfileSupports(
        profile, primec::IrBackendCapability::RuntimeReflection);
    if (supportsRuntimeReflection) {
      capabilities.push_back("runtime-reflection");
    }
    if (capabilities.empty()) {
      signature += "-";
    } else {
      for (std::size_t i = 0; i < capabilities.size(); ++i) {
        if (i != 0) {
          signature += ",";
        }
        signature += capabilities[i];
      }
    }
    return signature;
  };

  std::vector<std::string> actual;
  for (const primec::IrBackendCapabilityProfile &profile : primec::listIrBackendCapabilityProfiles()) {
    actual.push_back(signatureFor(profile));
  }

  const std::vector<std::string> expected = {
      "vm:-:vm:graphics-runtime,runtime-reflection",
      "native:-:native:graphics-runtime,runtime-reflection",
      "ir:-:ir:graphics-runtime,runtime-reflection",
      "wasm:wasi:wasm-wasi:-",
      "wasm:browser:wasm-browser:-",
      "glsl:-:glsl:-",
      "glsl-ir:-:glsl:-",
      "spirv:-:spirv:-",
      "spirv-ir:-:spirv:-",
      "cpp:-:cpp:graphics-runtime,runtime-reflection",
      "cpp-ir:-:cpp-ir:graphics-runtime,runtime-reflection",
      "exe:-:exe:graphics-runtime,runtime-reflection",
      "exe-ir:-:exe-ir:graphics-runtime,runtime-reflection",
  };

  CHECK(actual == expected);
}

TEST_CASE("backend capability query routes graphics runtime substrate by target profile") {
  auto graphicsSupportFor = [](std::string_view emitKind, std::string_view wasmProfile = "wasi") {
    primec::Options options;
    options.emitKind = std::string(emitKind);
    options.wasmProfile = std::string(wasmProfile);
    return primec::queryIrBackendCapabilitySupport(
        options, primec::IrBackendCapability::GraphicsRuntimeSubstrate);
  };

  const primec::IrBackendCapabilitySupport vmSupport = graphicsSupportFor("vm");
  CHECK(vmSupport.supported);
  CHECK(vmSupport.targetName == "vm");

  const primec::IrBackendCapabilitySupport nativeSupport = graphicsSupportFor("native");
  CHECK(nativeSupport.supported);
  CHECK(nativeSupport.targetName == "native");

  const primec::IrBackendCapabilitySupport cppAliasSupport = graphicsSupportFor("cpp");
  CHECK(cppAliasSupport.supported);
  CHECK(cppAliasSupport.targetName == "cpp");

  const primec::IrBackendCapabilitySupport irSupport = graphicsSupportFor("ir");
  CHECK(irSupport.supported);
  CHECK(irSupport.targetName == "ir");

  const primec::IrBackendCapabilitySupport wasmBrowserSupport =
      graphicsSupportFor("wasm", "browser");
  CHECK_FALSE(wasmBrowserSupport.supported);
  CHECK(wasmBrowserSupport.targetName == "wasm-browser");

  const primec::IrBackendCapabilitySupport wasmWasiSupport = graphicsSupportFor("wasm");
  CHECK_FALSE(wasmWasiSupport.supported);
  CHECK(wasmWasiSupport.targetName == "wasm-wasi");

  const primec::IrBackendCapabilitySupport glslAliasSupport = graphicsSupportFor("glsl-ir");
  CHECK_FALSE(glslAliasSupport.supported);
  CHECK(glslAliasSupport.targetName == "glsl");

  const primec::IrBackendCapabilitySupport spirvSupport = graphicsSupportFor("spirv");
  CHECK_FALSE(spirvSupport.supported);
  CHECK(spirvSupport.targetName == "spirv");
}

TEST_CASE("backend capability query routes runtime reflection by target profile") {
  auto reflectionSupportFor = [](std::string_view emitKind, std::string_view wasmProfile = "wasi") {
    primec::Options options;
    options.emitKind = std::string(emitKind);
    options.wasmProfile = std::string(wasmProfile);
    return primec::queryIrBackendCapabilitySupport(
        options, primec::IrBackendCapability::RuntimeReflection);
  };

  const primec::IrBackendCapabilitySupport vmSupport = reflectionSupportFor("vm");
  CHECK(vmSupport.supported);
  CHECK(vmSupport.targetName == "vm");

  const primec::IrBackendCapabilitySupport nativeSupport = reflectionSupportFor("native");
  CHECK(nativeSupport.supported);
  CHECK(nativeSupport.targetName == "native");

  const primec::IrBackendCapabilitySupport cppAliasSupport = reflectionSupportFor("cpp");
  CHECK(cppAliasSupport.supported);
  CHECK(cppAliasSupport.targetName == "cpp");

  const primec::IrBackendCapabilitySupport exeIrSupport = reflectionSupportFor("exe-ir");
  CHECK(exeIrSupport.supported);
  CHECK(exeIrSupport.targetName == "exe-ir");

  const primec::IrBackendCapabilitySupport wasmBrowserSupport =
      reflectionSupportFor("wasm", "browser");
  CHECK_FALSE(wasmBrowserSupport.supported);
  CHECK(wasmBrowserSupport.targetName == "wasm-browser");

  const primec::IrBackendCapabilitySupport wasmWasiSupport = reflectionSupportFor("wasm");
  CHECK_FALSE(wasmWasiSupport.supported);
  CHECK(wasmWasiSupport.targetName == "wasm-wasi");

  const primec::IrBackendCapabilitySupport glslAliasSupport = reflectionSupportFor("glsl-ir");
  CHECK_FALSE(glslAliasSupport.supported);
  CHECK(glslAliasSupport.targetName == "glsl");

  const primec::IrBackendCapabilitySupport spirvSupport = reflectionSupportFor("spirv");
  CHECK_FALSE(spirvSupport.supported);
  CHECK(spirvSupport.targetName == "spirv");
}

TEST_CASE("ir preparation rejects runtime reflection before unsupported backend lowering") {
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  semanticProgram.publishedLowererPreflightFacts.firstRuntimeReflectionPathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/meta/type_name");

  primec::Options options;
  options.emitKind = "wasm";
  options.wasmProfile = "browser";
  options.entryPath = "/main";

  primec::IrModule ir;
  primec::IrPreparationFailure failure;
  CHECK_FALSE(primec::prepareIrModule(
      program, &semanticProgram, options, primec::IrValidationTarget::WasmBrowser, ir, failure));
  CHECK(failure.stage == primec::IrPreparationFailureStage::Lowering);
  CHECK(failure.message ==
        "runtime reflection support unavailable for wasm-browser target: /meta/type_name");
  CHECK(failure.diagnosticInfo.message == failure.message);
}

TEST_CASE("ir preparation rejects missing semantic-product lowerer preflight runtime reflection ids") {
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  semanticProgram.publishedLowererPreflightFacts.hasRuntimeReflectionPath = true;

  primec::Options options;
  options.emitKind = "wasm";
  options.wasmProfile = "browser";
  options.entryPath = "/main";

  primec::IrModule ir;
  primec::IrPreparationFailure failure;
  CHECK_FALSE(primec::prepareIrModule(
      program, &semanticProgram, options, primec::IrValidationTarget::WasmBrowser, ir, failure));
  CHECK(failure.stage == primec::IrPreparationFailureStage::Lowering);
  CHECK(failure.message ==
        "missing semantic-product lowerer preflight runtime reflection path id");
  CHECK(failure.diagnosticInfo.message == failure.message);
}

TEST_CASE("ir preparation rejects stale semantic-product lowerer preflight runtime reflection ids") {
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  semanticProgram.publishedLowererPreflightFacts.hasRuntimeReflectionPath = true;
  semanticProgram.publishedLowererPreflightFacts.firstRuntimeReflectionPathId =
      static_cast<primec::SymbolId>(semanticProgram.callTargetStringTable.size() + 1u);

  primec::Options options;
  options.emitKind = "wasm";
  options.wasmProfile = "browser";
  options.entryPath = "/main";

  primec::IrModule ir;
  primec::IrPreparationFailure failure;
  CHECK_FALSE(primec::prepareIrModule(
      program, &semanticProgram, options, primec::IrValidationTarget::WasmBrowser, ir, failure));
  CHECK(failure.stage == primec::IrPreparationFailureStage::Lowering);
  CHECK(failure.message ==
        "stale semantic-product lowerer preflight runtime reflection path id");
  CHECK(failure.diagnosticInfo.message == failure.message);
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

TEST_CASE("ir preparation phase manifest pins ordered handoffs") {
  const auto &manifest = primec::irPreparationPhaseManifest();
  REQUIRE(manifest.size() == 6);

  std::vector<std::string_view> names;
  names.reserve(manifest.size());
  for (const primec::IrPreparationPhaseManifestEntry &entry : manifest) {
    names.push_back(entry.name);
    CHECK_FALSE(entry.requiredInputs.empty());
    CHECK_FALSE(entry.invalidationNotes.empty());
    CHECK_FALSE(entry.consumerNotes.empty());
  }

  const std::vector<std::string_view> expectedNames = {
      "semantic-product-preflight",
      "lower-semantic-product-to-ir",
      "validate-lowered-ir",
      "inline-ir-calls",
      "validate-inlined-ir",
      "release-lowered-ast-bodies",
  };
  CHECK(names == expectedNames);

  const auto *lowering = findIrPreparationPhase("lower-semantic-product-to-ir");
  REQUIRE(lowering != nullptr);
  CHECK(lowering->inputOwnership ==
        primec::IrPreparationPhaseOwnership::CompilePipelineAstAndSemanticProduct);
  CHECK(lowering->outputOwnership ==
        primec::IrPreparationPhaseOwnership::IrPreparationLoweredIr);
  CHECK(lowering->action == primec::IrPreparationPhaseAction::CreatesOutput);
  CHECK_FALSE(lowering->optional);
  CHECK(std::string_view(lowering->requiredInputs).find("semantic-product facts") !=
        std::string_view::npos);
  CHECK(std::string_view(lowering->invalidationNotes).find("stale semantic-product facts") !=
        std::string_view::npos);
}

TEST_CASE("ir preparation phase manifest documents inline invalidation") {
  const auto *validateLowered = findIrPreparationPhase("validate-lowered-ir");
  const auto *inlineCalls = findIrPreparationPhase("inline-ir-calls");
  const auto *validateInlined = findIrPreparationPhase("validate-inlined-ir");
  const auto *releaseAst = findIrPreparationPhase("release-lowered-ast-bodies");
  REQUIRE(validateLowered != nullptr);
  REQUIRE(inlineCalls != nullptr);
  REQUIRE(validateInlined != nullptr);
  REQUIRE(releaseAst != nullptr);

  CHECK(validateLowered < inlineCalls);
  CHECK(inlineCalls < validateInlined);
  CHECK(validateInlined < releaseAst);

  CHECK(inlineCalls->optional);
  CHECK(inlineCalls->inputOwnership ==
        primec::IrPreparationPhaseOwnership::IrPreparationValidatedIr);
  CHECK(inlineCalls->outputOwnership ==
        primec::IrPreparationPhaseOwnership::IrPreparationInlinedIr);
  CHECK(inlineCalls->action == primec::IrPreparationPhaseAction::MutatesOutput);
  CHECK(std::string_view(inlineCalls->invalidationNotes)
            .find("invalidates the prior validation result") != std::string_view::npos);

  CHECK(validateInlined->optional);
  CHECK(validateInlined->inputOwnership ==
        primec::IrPreparationPhaseOwnership::IrPreparationInlinedIr);
  CHECK(validateInlined->outputOwnership ==
        primec::IrPreparationPhaseOwnership::IrPreparationValidatedIr);
  CHECK(validateInlined->action == primec::IrPreparationPhaseAction::ValidatesOnly);

  CHECK(releaseAst->inputOwnership ==
        primec::IrPreparationPhaseOwnership::CompilerAstStorage);
  CHECK(releaseAst->action == primec::IrPreparationPhaseAction::ReleasesInputStorage);
  CHECK(std::string_view(releaseAst->invalidationNotes).find("releases lowered AST bodies") !=
        std::string_view::npos);
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

TEST_CASE("ir preparation releases lowered AST bodies while preserving source-map provenance") {
  const std::filesystem::path tempPath = makeTempIrPipelineSourcePath();
  {
    std::ofstream file(tempPath);
    REQUIRE(file.good());
    file << R"(
[return<i32>]
helper([i32] input) {
  [i32] local{input}
  return(local)
}

[return<i32>]
main() {
  [i32] value{helper(21i32)}
  return(value)
}
)";
  }

  primec::Options options;
  options.inputPath = tempPath.string();
  options.entryPath = "/main";
  options.emitKind = "vm";
  primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);

  primec::CompilePipelineOutput output;
  primec::CompilePipelineErrorStage errorStage = primec::CompilePipelineErrorStage::None;
  std::string error;
  REQUIRE(primec::runCompilePipeline(options, output, errorStage, error));
  REQUIRE(output.hasSemanticProgram);

  std::error_code ec;
  std::filesystem::remove(tempPath, ec);

  const primec::ProgramHeapEstimateStats before = primec::estimateProgramHeap(output.program);
  CHECK(before.dynamicBytes > 0);
  CHECK(before.exprs > 0);
  CHECK(std::any_of(output.program.definitions.begin(),
                    output.program.definitions.end(),
                    [](const primec::Definition &definition) {
                      return !definition.parameters.empty() || !definition.statements.empty() ||
                             definition.returnExpr.has_value();
                    }));

  primec::IrModule ir;
  primec::IrPreparationFailure failure;
  REQUIRE(primec::prepareIrModule(
      output.program, &output.semanticProgram, options, primec::IrValidationTarget::Vm, ir, failure));

  const primec::ProgramHeapEstimateStats after = primec::estimateProgramHeap(output.program);
  CHECK(after.dynamicBytes < before.dynamicBytes);
  CHECK(after.exprs < before.exprs);

  for (const auto &definition : output.program.definitions) {
    CHECK(definition.parameters.empty());
    CHECK(definition.statements.empty());
    CHECK_FALSE(definition.returnExpr.has_value());
  }
  for (const auto &execution : output.program.executions) {
    CHECK(execution.arguments.empty());
    CHECK(execution.argumentNames.empty());
    CHECK(execution.bodyArguments.empty());
    CHECK_FALSE(execution.hasBodyArguments);
  }

  REQUIRE_FALSE(ir.instructionSourceMap.empty());
  CHECK(std::any_of(ir.instructionSourceMap.begin(),
                    ir.instructionSourceMap.end(),
                    [](const primec::IrInstructionSourceMapEntry &entry) {
                      return entry.provenance == primec::IrSourceMapProvenance::CanonicalAst && entry.line > 0 &&
                             entry.column > 0;
                    }));
}

TEST_CASE("semantic-product contract rejects missing local-auto facts across entry targets") {
  const std::string source =
      "[return<T>]\n"
      "id<T>([T] value) {\n"
      "  return(value)\n"
      "}\n"
      "\n"
      "[return<i32>]\n"
      "main() {\n"
      "  [auto] selected{id(21i32)}\n"
      "  return(selected)\n"
      "}\n";

  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidateThroughCompilePipeline(
      source, program, &semanticProgram, error, {}, {}));
  CHECK(error.empty());
  REQUIRE(!semanticProgram.localAutoFacts.empty());

  auto clearPublishedLocalAutoFacts = [](primec::SemanticProgram &semanticProduct) {
    semanticProduct.localAutoFacts.clear();
    semanticProduct.publishedRoutingLookups.localAutoFactIndicesByExpr.clear();
    semanticProduct.publishedRoutingLookups.localAutoFactIndicesByInitPathAndBindingNameId.clear();
    for (auto &moduleArtifacts : semanticProduct.moduleResolvedArtifacts) {
      moduleArtifacts.localAutoFactIndices.clear();
    }
  };

  const std::vector<std::pair<std::string_view, primec::IrValidationTarget>> entryTargets = {
      {"primevm", primec::IrValidationTarget::Vm},
      {"primec-native", primec::IrValidationTarget::Native},
      {"primec-generic", primec::IrValidationTarget::Any},
  };

  for (const auto &[entryLabel, validationTarget] : entryTargets) {
    CAPTURE(entryLabel);
    primec::Program mutatedProgram = program;
    primec::SemanticProgram mutatedSemanticProduct = semanticProgram;
    clearPublishedLocalAutoFacts(mutatedSemanticProduct);

    primec::Options options;
    options.entryPath = "/main";

    primec::IrModule ir;
    primec::IrPreparationFailure failure;
    CHECK_FALSE(primec::prepareIrModule(
        mutatedProgram, &mutatedSemanticProduct, options, validationTarget, ir, failure));
    CHECK(failure.stage == primec::IrPreparationFailureStage::Lowering);
    CHECK(failure.message ==
          "missing semantic-product local-auto fact: /main -> local selected");
    CHECK(failure.diagnosticInfo.message == failure.message);
  }
}

TEST_CASE("semantic-product contract rejects stale local-auto binding types") {
  const std::string source =
      "[return<T>]\n"
      "id<T>([T] value) {\n"
      "  return(value)\n"
      "}\n"
      "\n"
      "[return<i32>]\n"
      "main() {\n"
      "  [auto] selected{id(21i32)}\n"
      "  return(selected)\n"
      "}\n";

  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidateThroughCompilePipeline(
      source, program, &semanticProgram, error, {}, {}));
  CHECK(error.empty());
  REQUIRE(!semanticProgram.localAutoFacts.empty());

  primec::SemanticProgramLocalAutoFact *selectedFact = nullptr;
  for (auto &entry : semanticProgram.localAutoFacts) {
    if (entry.scopePath == "/main" && entry.bindingName == "selected") {
      selectedFact = &entry;
      break;
    }
  }
  REQUIRE(selectedFact != nullptr);
  selectedFact->bindingTypeText = "f64";

  primec::Options options;
  options.entryPath = "/main";

  primec::IrModule ir;
  primec::IrPreparationFailure failure;
  CHECK_FALSE(primec::prepareIrModule(
      program, &semanticProgram, options, primec::IrValidationTarget::Vm, ir, failure));
  CHECK(failure.stage == primec::IrPreparationFailureStage::Lowering);
  CHECK(failure.message ==
        "stale semantic-product local-auto binding type metadata: /main -> local selected");
  CHECK(failure.diagnosticInfo.message == failure.message);
}

TEST_CASE("compile pipeline semantic handoff gate reaches lowering and rejects stale facts") {
  const std::string source =
      "import /std/math/*\n"
      "\n"
      "[i32]\n"
      "helper([i32] value) {\n"
      "  return(value)\n"
      "}\n"
      "\n"
      "[i32]\n"
      "main() {\n"
      "  [auto] selected{helper(41i32)}\n"
      "  return(selected)\n"
      "}\n";

  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidateThroughCompilePipeline(
      source, program, &semanticProgram, error, {}, {}));
  CHECK(error.empty());

  const auto *validatorPass = findSemanticValidationPass("validator-passes");
  const auto *publicationPass =
      findSemanticValidationPass("semantic-product-publication");
  REQUIRE(validatorPass != nullptr);
  REQUIRE(publicationPass != nullptr);
  CHECK(validatorPass->action ==
        primec::semantics::SemanticValidationPassAction::PublishesFacts);
  CHECK(publicationPass->kind ==
        primec::semantics::SemanticValidationPassKind::Publication);

  CHECK(std::find(program.imports.begin(), program.imports.end(), "/std/math/*") !=
        program.imports.end());
  const auto *mainDefinition = findSemanticEntry(
      program.definitions,
      [](const primec::Definition &definition) {
        return definition.fullPath == "/main";
      });
  REQUIRE(mainDefinition != nullptr);
  CHECK(std::any_of(mainDefinition->transforms.begin(),
                    mainDefinition->transforms.end(),
                    [](const primec::Transform &transform) {
                      return transform.name == "return" &&
                             transform.templateArgs ==
                                 std::vector<std::string>{"i32"};
                    }));

  const auto *mainModule =
      findSemanticModuleArtifacts(semanticProgram, "/");
  REQUIRE(mainModule != nullptr);
  CHECK(!mainModule->directCallTargetIndices.empty());
  CHECK(!mainModule->callableSummaryIndices.empty());
  CHECK(!mainModule->returnFactIndices.empty());
  CHECK(!mainModule->localAutoFactIndices.empty());
  CHECK(!semanticProgram.publishedRoutingLookups.directCallTargetIdsByExpr.empty());
  CHECK(!semanticProgram.publishedRoutingLookups.localAutoFactIndicesByExpr.empty());
  CHECK(!semanticProgram.publishedRoutingLookups.callableSummaryIndicesByPathId.empty());

  const auto directCallTargets =
      primec::semanticProgramDirectCallTargetView(semanticProgram);
  const auto *helperCall = findSemanticEntry(
      directCallTargets,
      [&semanticProgram](const primec::SemanticProgramDirectCallTarget &entry) {
        return entry.scopePath == "/main" &&
               entry.callName == "helper" &&
               primec::semanticProgramDirectCallTargetResolvedPath(
                   semanticProgram, entry) == "/helper";
      });
  REQUIRE(helperCall != nullptr);

  primec::Options options;
  options.entryPath = "/main";
  primec::IrModule ir;
  primec::IrPreparationFailure failure;
  primec::Program loweringProgram = program;
  primec::SemanticProgram loweringSemanticProgram = semanticProgram;
  REQUIRE(primec::prepareIrModule(loweringProgram,
                                  &loweringSemanticProgram,
                                  options,
                                  primec::IrValidationTarget::Vm,
                                  ir,
                                  failure));
  CHECK(!ir.functions.empty());
  CHECK(ir.entryIndex >= 0);

  primec::Program staleProgram = program;
  primec::SemanticProgram staleSemanticProgram = semanticProgram;
  staleSemanticProgram.directCallTargets.clear();
  staleSemanticProgram.publishedRoutingLookups.directCallTargetIdsByExpr.clear();
  staleSemanticProgram.publishedRoutingLookups.directCallStdlibSurfaceIdsByExpr.clear();
  for (auto &moduleArtifacts : staleSemanticProgram.moduleResolvedArtifacts) {
    moduleArtifacts.directCallTargetIndices.clear();
  }

  primec::IrModule staleIr;
  primec::IrPreparationFailure staleFailure;
  CHECK_FALSE(primec::prepareIrModule(staleProgram,
                                      &staleSemanticProgram,
                                      options,
                                      primec::IrValidationTarget::Vm,
                                      staleIr,
                                      staleFailure));
  CHECK(staleFailure.stage == primec::IrPreparationFailureStage::Lowering);
  CHECK(staleFailure.message ==
        "missing semantic-product direct-call target: /main -> helper");
  CHECK(staleFailure.diagnosticInfo.message == staleFailure.message);
}

TEST_CASE("published target lookups ignore raw routing facts without maps") {
  primec::SemanticProgram semanticProgram;
  const auto directPathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/helper");
  const auto methodPathId =
      primec::semanticProgramInternCallTargetString(semanticProgram,
                                                    "/vector/count");
  const auto bridgePathId =
      primec::semanticProgramInternCallTargetString(semanticProgram,
                                                    "/vector/count");

  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "helper",
      .sourceLine = 1,
      .sourceColumn = 1,
      .semanticNodeId = 11,
      .provenanceHandle = 0,
      .scopePathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
      .callNameId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "helper"),
      .resolvedPathId = directPathId,
      .stdlibSurfaceId = primec::findStdlibSurfaceMetadataByBridgeKey("collections.map_helpers")->id,
  });
  semanticProgram.methodCallTargets.push_back(primec::SemanticProgramMethodCallTarget{
      .scopePath = "/main",
      .methodName = "count",
      .receiverTypeText = "vector<i32>",
      .sourceLine = 1,
      .sourceColumn = 1,
      .semanticNodeId = 12,
      .provenanceHandle = 0,
      .scopePathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
      .methodNameId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "count"),
      .receiverTypeTextId =
          primec::semanticProgramInternCallTargetString(semanticProgram,
                                                        "vector<i32>"),
      .resolvedPathId = methodPathId,
      .stdlibSurfaceId = primec::StdlibSurfaceId::CollectionsManifestSurface0,
  });
  semanticProgram.bridgePathChoices.push_back(primec::SemanticProgramBridgePathChoice{
      .scopePath = "/main",
      .collectionFamily = "vector",
      .sourceLine = 1,
      .sourceColumn = 1,
      .semanticNodeId = 13,
      .provenanceHandle = 0,
      .scopePathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
      .collectionFamilyId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "vector"),
      .helperNameId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "count"),
      .chosenPathId = bridgePathId,
      .stdlibSurfaceId = primec::StdlibSurfaceId::CollectionsManifestSurface0,
  });

  CHECK_FALSE(
      primec::semanticProgramLookupPublishedDirectCallTargetId(semanticProgram, 11)
          .has_value());
  CHECK_FALSE(
      primec::semanticProgramLookupPublishedMethodCallTargetId(semanticProgram, 12)
          .has_value());
  CHECK_FALSE(
      primec::semanticProgramLookupPublishedBridgePathChoiceId(semanticProgram, 13)
          .has_value());
  CHECK_FALSE(primec::semanticProgramLookupPublishedDirectCallTargetStdlibSurfaceId(
                  semanticProgram, 11)
                  .has_value());
  CHECK_FALSE(primec::semanticProgramLookupPublishedMethodCallTargetStdlibSurfaceId(
                  semanticProgram, 12)
                  .has_value());
  CHECK_FALSE(primec::semanticProgramLookupPublishedBridgePathChoiceStdlibSurfaceId(
                  semanticProgram, 13)
                  .has_value());

  primec::Expr directExpr;
  directExpr.kind = primec::Expr::Kind::Call;
  directExpr.name = "helper";
  directExpr.semanticNodeId = 11;
  primec::Expr methodExpr;
  methodExpr.kind = primec::Expr::Kind::Call;
  methodExpr.isMethodCall = true;
  methodExpr.name = "count";
  methodExpr.semanticNodeId = 12;
  primec::Expr bridgeExpr;
  bridgeExpr.kind = primec::Expr::Kind::Call;
  bridgeExpr.name = "count";
  bridgeExpr.semanticNodeId = 13;

  const auto rawFactOnlyAdapter =
      primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);
  CHECK(primec::ir_lowerer::findSemanticProductDirectCallTarget(
            rawFactOnlyAdapter, directExpr)
            .empty());
  CHECK_FALSE(primec::ir_lowerer::findSemanticProductDirectCallStdlibSurfaceId(
                  rawFactOnlyAdapter, directExpr)
                  .has_value());
  CHECK(primec::ir_lowerer::findSemanticProductMethodCallTarget(
            rawFactOnlyAdapter, methodExpr)
            .empty());
  CHECK_FALSE(primec::ir_lowerer::findSemanticProductMethodCallStdlibSurfaceId(
                  rawFactOnlyAdapter, methodExpr)
                  .has_value());
  CHECK(primec::ir_lowerer::findSemanticProductBridgePathChoice(
            rawFactOnlyAdapter, bridgeExpr)
            .empty());
  CHECK_FALSE(
      primec::ir_lowerer::findSemanticProductBridgePathChoiceStdlibSurfaceId(
          rawFactOnlyAdapter, bridgeExpr)
          .has_value());

  semanticProgram.publishedRoutingLookups.directCallTargetIdsByExpr
      .insert_or_assign(11, directPathId);
  semanticProgram.publishedRoutingLookups.methodCallTargetIdsByExpr
      .insert_or_assign(12, methodPathId);
  semanticProgram.publishedRoutingLookups.bridgePathChoiceIdsByExpr
      .insert_or_assign(13, bridgePathId);
  semanticProgram.publishedRoutingLookups.directCallStdlibSurfaceIdsByExpr
      .insert_or_assign(11, primec::findStdlibSurfaceMetadataByBridgeKey("collections.map_helpers")->id);
  semanticProgram.publishedRoutingLookups.methodCallStdlibSurfaceIdsByExpr
      .insert_or_assign(12, primec::StdlibSurfaceId::CollectionsManifestSurface0);
  semanticProgram.publishedRoutingLookups.bridgePathChoiceStdlibSurfaceIdsByExpr
      .insert_or_assign(13, primec::StdlibSurfaceId::CollectionsManifestSurface0);

  CHECK(primec::semanticProgramLookupPublishedDirectCallTargetId(
            semanticProgram, 11)
            .value_or(primec::InvalidSymbolId) == directPathId);
  CHECK(primec::semanticProgramLookupPublishedMethodCallTargetId(
            semanticProgram, 12)
            .value_or(primec::InvalidSymbolId) == methodPathId);
  CHECK(primec::semanticProgramLookupPublishedBridgePathChoiceId(
            semanticProgram, 13)
            .value_or(primec::InvalidSymbolId) == bridgePathId);
  CHECK(primec::semanticProgramLookupPublishedDirectCallTargetStdlibSurfaceId(
            semanticProgram, 11) ==
        primec::findStdlibSurfaceMetadataByBridgeKey("collections.map_helpers")->id);
  CHECK(primec::semanticProgramLookupPublishedMethodCallTargetStdlibSurfaceId(
            semanticProgram, 12) ==
        primec::StdlibSurfaceId::CollectionsManifestSurface0);
  CHECK(primec::semanticProgramLookupPublishedBridgePathChoiceStdlibSurfaceId(
            semanticProgram, 13) ==
        primec::StdlibSurfaceId::CollectionsManifestSurface0);

  const auto publishedLookupAdapter =
      primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);
  CHECK(primec::ir_lowerer::findSemanticProductDirectCallTarget(
            publishedLookupAdapter, directExpr) == "/helper");
  CHECK(primec::ir_lowerer::findSemanticProductDirectCallStdlibSurfaceId(
            publishedLookupAdapter, directExpr) ==
        primec::findStdlibSurfaceMetadataByBridgeKey("collections.map_helpers")->id);
  CHECK(primec::ir_lowerer::findSemanticProductMethodCallTarget(
            publishedLookupAdapter, methodExpr) == "/vector/count");
  CHECK(primec::ir_lowerer::findSemanticProductMethodCallStdlibSurfaceId(
            publishedLookupAdapter, methodExpr) ==
        primec::StdlibSurfaceId::CollectionsManifestSurface0);
  CHECK(primec::ir_lowerer::findSemanticProductBridgePathChoice(
            publishedLookupAdapter, bridgeExpr) == "/vector/count");
  CHECK(primec::ir_lowerer::findSemanticProductBridgePathChoiceStdlibSurfaceId(
            publishedLookupAdapter, bridgeExpr) ==
        primec::StdlibSurfaceId::CollectionsManifestSurface0);
}

TEST_CASE("semantic-product coverage validators ignore raw routing facts without maps") {
  {
    primec::Program program;
    primec::Definition callee;
    callee.fullPath = "/callee";
    callee.semanticNodeId = 6100;
    program.definitions.push_back(callee);
    primec::Definition mainDef;
    mainDef.fullPath = "/main";
    mainDef.semanticNodeId = 6101;
    primec::Expr callExpr;
    callExpr.kind = primec::Expr::Kind::Call;
    callExpr.name = "callee";
    callExpr.semanticNodeId = 6102;
    mainDef.statements.push_back(callExpr);
    program.definitions.push_back(mainDef);

    primec::SemanticProgram semanticProgram;
    addVoidCallableSummary(semanticProgram, 6101);
    addVoidCallableSummaryForPath(semanticProgram, "/callee", 6100);
    const auto calleePathId =
        primec::semanticProgramInternCallTargetString(semanticProgram, "/callee");
    semanticProgram.definitions.push_back(primec::SemanticProgramDefinition{
        .name = "callee",
        .fullPath = "/callee",
        .namespacePrefix = "",
        .sourceLine = 1,
        .sourceColumn = 1,
        .semanticNodeId = 6100,
    });
    semanticProgram.publishedRoutingLookups.definitionIndicesByPathId
        .insert_or_assign(calleePathId, 0);
    semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
        .scopePath = "/main",
        .callName = "callee",
        .sourceLine = 1,
        .sourceColumn = 1,
        .semanticNodeId = 6102,
        .resolvedPathId = calleePathId,
        .stdlibSurfaceId = std::nullopt,
    });

    std::string error;
    CHECK_FALSE(primec::ir_lowerer::validateSemanticProductDirectCallCoverage(
        program, &semanticProgram, error));
    CHECK(error == "missing semantic-product direct-call target: /main -> callee");
  }

  {
    primec::Program program;
    primec::Definition mainDef;
    mainDef.fullPath = "/main";
    mainDef.semanticNodeId = 6200;
    primec::Expr receiverExpr;
    receiverExpr.kind = primec::Expr::Kind::Name;
    receiverExpr.name = "values";
    primec::Expr methodCallExpr;
    methodCallExpr.kind = primec::Expr::Kind::Call;
    methodCallExpr.name = "count";
    methodCallExpr.isMethodCall = true;
    methodCallExpr.semanticNodeId = 6201;
    methodCallExpr.args.push_back(receiverExpr);
    mainDef.statements.push_back(methodCallExpr);
    program.definitions.push_back(mainDef);

    primec::SemanticProgram semanticProgram;
    addVoidCallableSummary(semanticProgram, 6200);
    semanticProgram.methodCallTargets.push_back(primec::SemanticProgramMethodCallTarget{
        .scopePath = "/main",
        .methodName = "count",
        .receiverTypeText = "vector<i32>",
        .sourceLine = 1,
        .sourceColumn = 1,
        .semanticNodeId = 6201,
        .resolvedPathId = primec::semanticProgramInternCallTargetString(
            semanticProgram, "/std/collections/vector/count"),
        .stdlibSurfaceId = std::nullopt,
    });

    std::string error;
    CHECK_FALSE(primec::ir_lowerer::validateSemanticProductMethodCallCoverage(
        program, &semanticProgram, error));
    CHECK(error == "missing semantic-product method-call target: /main -> count");
  }

  {
    primec::Program program;
    primec::Definition mainDef;
    mainDef.fullPath = "/main";
    mainDef.semanticNodeId = 6300;
    primec::Expr valuesExpr;
    valuesExpr.kind = primec::Expr::Kind::Name;
    valuesExpr.name = "values";
    primec::Expr callExpr;
    callExpr.kind = primec::Expr::Kind::Call;
    callExpr.name = "count";
    callExpr.semanticNodeId = 6301;
    callExpr.args.push_back(valuesExpr);
    mainDef.statements.push_back(callExpr);
    program.definitions.push_back(mainDef);

    primec::SemanticProgram semanticProgram;
    addVoidCallableSummary(semanticProgram, 6300);
    semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
        .scopePath = "/main",
        .callName = "count",
        .sourceLine = 1,
        .sourceColumn = 1,
        .semanticNodeId = 6301,
        .resolvedPathId =
            primec::semanticProgramInternCallTargetString(semanticProgram, "/vector/count"),
        .stdlibSurfaceId = primec::StdlibSurfaceId::CollectionsManifestSurface0,
    });
    semanticProgram.publishedRoutingLookups.directCallTargetIdsByExpr
        .insert_or_assign(6301, semanticProgram.directCallTargets.back().resolvedPathId);
    semanticProgram.publishedRoutingLookups.directCallStdlibSurfaceIdsByExpr
        .insert_or_assign(6301, primec::StdlibSurfaceId::CollectionsManifestSurface0);
    semanticProgram.bridgePathChoices.push_back(primec::SemanticProgramBridgePathChoice{
        .scopePath = "/main",
        .collectionFamily = "vector",
        .sourceLine = 1,
        .sourceColumn = 1,
        .semanticNodeId = 6301,
        .helperNameId = primec::InvalidSymbolId,
        .chosenPathId = primec::semanticProgramInternCallTargetString(
            semanticProgram, "/vector/count"),
        .stdlibSurfaceId = primec::StdlibSurfaceId::CollectionsManifestSurface0,
    });

    std::string error;
    CHECK_FALSE(primec::ir_lowerer::validateSemanticProductBridgePathCoverage(
        program, &semanticProgram, error));
    CHECK(error == "missing semantic-product bridge-path choice: /main -> count");
  }
}

TEST_CASE("native pick target sum resolution uses semantic-product facts") {
  const std::string source = R"(
[sum]
Choice {
  [i32] left
  [i32] right
}

[sum]
OtherChoice {
  [i32] left
  [i32] right
}

[return<i32>]
main() {
  [Choice] choice{[right] 41i32}
  return(pick(choice) {
    left(value) {
      plus(value, 1i32)
    }
    right(value) {
      plus(value, 2i32)
    }
  })
}
)";

  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidateThroughCompilePipeline(
      source, program, &semanticProgram, error, {}, {}));
  CHECK(error.empty());

  const auto semanticProductTargets =
      primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);
  const auto *mainDefinition = findSemanticEntry(
      program.definitions,
      [](const primec::Definition &definition) {
        return definition.fullPath == "/main";
      });
  REQUIRE(mainDefinition != nullptr);
  REQUIRE(mainDefinition->returnExpr.has_value());
  REQUIRE(mainDefinition->returnExpr->kind == primec::Expr::Kind::Call);
  REQUIRE(mainDefinition->returnExpr->name == "pick");
  REQUIRE(!mainDefinition->returnExpr->args.empty());
  const primec::Expr &choiceTarget = mainDefinition->returnExpr->args.front();
  REQUIRE(choiceTarget.kind == primec::Expr::Kind::Name);
  REQUIRE(choiceTarget.name == "choice");
  REQUIRE(choiceTarget.semanticNodeId != 0);
  const auto *choiceFact =
      primec::ir_lowerer::findSemanticProductBindingFact(
          semanticProductTargets, choiceTarget);
  REQUIRE(choiceFact != nullptr);
  CHECK(choiceFact->semanticNodeId == choiceTarget.semanticNodeId);
  CHECK(choiceFact->bindingTypeText == "Choice");
  REQUIRE(primec::ir_lowerer::findSemanticProductSumTypeMetadata(
              semanticProductTargets, "/Choice") != nullptr);
  REQUIRE(primec::ir_lowerer::findSemanticProductSumTypeMetadata(
              semanticProductTargets, "/OtherChoice") != nullptr);
  const auto *rightVariantMetadata =
      primec::ir_lowerer::findSemanticProductSumVariantMetadata(
          semanticProductTargets, "/Choice", "right");
  REQUIRE(rightVariantMetadata != nullptr);
  CHECK(rightVariantMetadata->variantIndex == 1);
  CHECK(rightVariantMetadata->tagValue == 1);
  CHECK(rightVariantMetadata->hasPayload);
  CHECK(rightVariantMetadata->payloadTypeText == "i32");
  const auto choiceLeftVariantOrder = std::find_if(
      semanticProgram.sumVariantMetadata.begin(),
      semanticProgram.sumVariantMetadata.end(),
      [](const primec::SemanticProgramSumVariantMetadata &entry) {
        return entry.sumPath == "/Choice" && entry.variantName == "left";
      });
  const auto choiceRightVariantOrder = std::find_if(
      semanticProgram.sumVariantMetadata.begin(),
      semanticProgram.sumVariantMetadata.end(),
      [](const primec::SemanticProgramSumVariantMetadata &entry) {
        return entry.sumPath == "/Choice" && entry.variantName == "right";
      });
  REQUIRE(choiceLeftVariantOrder != semanticProgram.sumVariantMetadata.end());
  REQUIRE(choiceRightVariantOrder != semanticProgram.sumVariantMetadata.end());
  CHECK(choiceLeftVariantOrder < choiceRightVariantOrder);
  const auto choiceOrder = std::find_if(
      semanticProgram.sumTypeMetadata.begin(),
      semanticProgram.sumTypeMetadata.end(),
      [](const primec::SemanticProgramSumTypeMetadata &entry) {
        return entry.fullPath == "/Choice";
      });
  const auto otherOrder = std::find_if(
      semanticProgram.sumTypeMetadata.begin(),
      semanticProgram.sumTypeMetadata.end(),
      [](const primec::SemanticProgramSumTypeMetadata &entry) {
        return entry.fullPath == "/OtherChoice";
      });
  REQUIRE(choiceOrder != semanticProgram.sumTypeMetadata.end());
  REQUIRE(otherOrder != semanticProgram.sumTypeMetadata.end());
  CHECK(choiceOrder < otherOrder);

  primec::Options options;
  options.entryPath = "/main";
  primec::IrModule ir;
  primec::IrPreparationFailure failure;
  primec::Program loweringProgram = program;
  primec::SemanticProgram loweringSemanticProgram = semanticProgram;
  REQUIRE(primec::prepareIrModule(loweringProgram,
                                  &loweringSemanticProgram,
                                  options,
                                  primec::IrValidationTarget::Native,
                                  ir,
                                  failure));
  CHECK(!ir.functions.empty());

  auto rewriteChoiceBindingType =
      [](primec::SemanticProgram &semanticProduct, const std::string &typeText) {
        bool updated = false;
        for (auto &fact : semanticProduct.bindingFacts) {
          if (semanticTextOrFallback(semanticProduct, fact.scopePathId, fact.scopePath) == "/main" &&
              semanticTextOrFallback(semanticProduct, fact.nameId, fact.name) == "choice") {
            fact.bindingTypeText = typeText;
            fact.bindingTypeTextId =
                primec::semanticProgramInternCallTargetString(semanticProduct, typeText);
            updated = true;
          }
        }
        return updated;
      };

  primec::Program staleProgram = program;
  primec::SemanticProgram staleSemanticProgram = semanticProgram;
  REQUIRE(rewriteChoiceBindingType(staleSemanticProgram, "OtherChoice"));

  primec::IrModule staleIr;
  primec::IrPreparationFailure staleFailure;
  CHECK_FALSE(primec::prepareIrModule(staleProgram,
                                      &staleSemanticProgram,
                                      options,
                                      primec::IrValidationTarget::Native,
                                      staleIr,
                                      staleFailure));
  CHECK(staleFailure.stage == primec::IrPreparationFailureStage::Lowering);
  CHECK(staleFailure.message ==
        "stale semantic-product pick target binding type: /main -> choice");
  CHECK(staleFailure.diagnosticInfo.message == staleFailure.message);

  primec::Program missingMetadataProgram = program;
  primec::SemanticProgram missingMetadataSemanticProgram = semanticProgram;
  missingMetadataSemanticProgram.sumTypeMetadata.erase(
      std::remove_if(missingMetadataSemanticProgram.sumTypeMetadata.begin(),
                     missingMetadataSemanticProgram.sumTypeMetadata.end(),
                     [](const primec::SemanticProgramSumTypeMetadata &entry) {
                       return entry.fullPath == "/Choice";
                     }),
      missingMetadataSemanticProgram.sumTypeMetadata.end());
  missingMetadataSemanticProgram.publishedRoutingLookups
      .sumTypeMetadataIndicesByPathId.clear();

  primec::IrModule missingMetadataIr;
  primec::IrPreparationFailure missingMetadataFailure;
  CHECK_FALSE(primec::prepareIrModule(missingMetadataProgram,
                                      &missingMetadataSemanticProgram,
                                      options,
                                      primec::IrValidationTarget::Native,
                                      missingMetadataIr,
                                      missingMetadataFailure));
  CHECK(missingMetadataFailure.stage == primec::IrPreparationFailureStage::Lowering);
  CHECK(missingMetadataFailure.message ==
        "missing semantic-product sum metadata for pick target: /main -> choice");
  CHECK(missingMetadataFailure.diagnosticInfo.message == missingMetadataFailure.message);

  auto rewriteChoiceRightVariantTag =
      [](primec::SemanticProgram &semanticProduct, uint32_t tagValue) {
        for (auto &entry : semanticProduct.sumVariantMetadata) {
          if (entry.sumPath == "/Choice" && entry.variantName == "right") {
            entry.tagValue = tagValue;
            return true;
          }
        }
        return false;
      };

  primec::Program staleVariantProgram = program;
  primec::SemanticProgram staleVariantSemanticProgram = semanticProgram;
  REQUIRE(rewriteChoiceRightVariantTag(staleVariantSemanticProgram, 42));

  primec::IrModule staleVariantIr;
  primec::IrPreparationFailure staleVariantFailure;
  CHECK_FALSE(primec::prepareIrModule(staleVariantProgram,
                                      &staleVariantSemanticProgram,
                                      options,
                                      primec::IrValidationTarget::Native,
                                      staleVariantIr,
                                      staleVariantFailure));
  CHECK(staleVariantFailure.stage == primec::IrPreparationFailureStage::Lowering);
  CHECK(staleVariantFailure.message ==
        "stale semantic-product sum variant metadata for sum slot layout: /Choice -> right");
  CHECK(staleVariantFailure.diagnosticInfo.message == staleVariantFailure.message);

  primec::Program missingVariantProgram = program;
  primec::SemanticProgram missingVariantSemanticProgram = semanticProgram;
  missingVariantSemanticProgram.sumVariantMetadata.erase(
      std::remove_if(missingVariantSemanticProgram.sumVariantMetadata.begin(),
                     missingVariantSemanticProgram.sumVariantMetadata.end(),
                     [](const primec::SemanticProgramSumVariantMetadata &entry) {
                       return entry.sumPath == "/Choice" &&
                              entry.variantName == "right";
                     }),
      missingVariantSemanticProgram.sumVariantMetadata.end());
  missingVariantSemanticProgram.publishedRoutingLookups
      .sumVariantMetadataIndicesBySumPathAndVariantNameId.clear();
  for (std::size_t index = 0;
       index < missingVariantSemanticProgram.sumVariantMetadata.size();
       ++index) {
    const auto &entry = missingVariantSemanticProgram.sumVariantMetadata[index];
    const auto sumPathId = primec::semanticProgramLookupCallTargetStringId(
        missingVariantSemanticProgram, entry.sumPath);
    const auto variantNameId = primec::semanticProgramLookupCallTargetStringId(
        missingVariantSemanticProgram, entry.variantName);
    if (sumPathId.has_value() && variantNameId.has_value()) {
      missingVariantSemanticProgram.publishedRoutingLookups
          .sumVariantMetadataIndicesBySumPathAndVariantNameId.insert_or_assign(
              (static_cast<uint64_t>(*sumPathId) << 32) |
                  static_cast<uint64_t>(*variantNameId),
              index);
    }
  }

  primec::IrModule missingVariantIr;
  primec::IrPreparationFailure missingVariantFailure;
  CHECK_FALSE(primec::prepareIrModule(missingVariantProgram,
                                      &missingVariantSemanticProgram,
                                      options,
                                      primec::IrValidationTarget::Native,
                                      missingVariantIr,
                                      missingVariantFailure));
  CHECK(missingVariantFailure.stage == primec::IrPreparationFailureStage::Lowering);
  CHECK(missingVariantFailure.message ==
        "missing semantic-product sum variant metadata for sum slot layout: /Choice -> right");
  CHECK(missingVariantFailure.diagnosticInfo.message == missingVariantFailure.message);
}

TEST_CASE("native pick payload locals use semantic-product variant metadata") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path lowerSumHelpersPath =
      cwd / "src" / "ir_lowerer" / "IrLowererLowerSumHelpers.cpp";
  if (!std::filesystem::exists(lowerSumHelpersPath)) {
    lowerSumHelpersPath =
        cwd.parent_path() / "src" / "ir_lowerer" / "IrLowererLowerSumHelpers.cpp";
  }
  REQUIRE(std::filesystem::exists(lowerSumHelpersPath));

  const std::string source = readTextFile(lowerSumHelpersPath);
  CHECK(source.find("resolvePublishedSumPayloadStorageInfo") != std::string::npos);
  CHECK(source.find("\"pick payload local\"") != std::string::npos);
  CHECK(source.find("\"pick arm\"") != std::string::npos);
  CHECK(source.find("makePickPayloadLocalInfo(sumDef, *variant, payloadInfo)") !=
        std::string::npos);
  CHECK(source.find("bindPickPayload(*sumDef,") != std::string::npos);
  CHECK(source.find("isMutableLocalExpr(") != std::string::npos);
  const size_t aggregateResolverPos =
      source.find("resolveSemanticProductPickAggregateResultStructPath");
  REQUIRE(aggregateResolverPos != std::string::npos);
  const size_t aggregateBindingFactPos =
      source.find("findSemanticProductBindingFact(semanticTargets, valueExpr)",
                  aggregateResolverPos);
  const size_t aggregateQueryFactPos =
      source.find("findSemanticProductQueryFact(semanticTargets, valueExpr)",
                  aggregateResolverPos);
  const size_t aggregateTypeResolverPos =
      source.find("addSemanticProductCandidateTypeText", aggregateResolverPos);
  const size_t aggregateSemanticResolvePos =
      source.find("semanticProgramResolveCallTargetString(",
                  aggregateTypeResolverPos);
  const size_t aggregateBindingIdPos =
      source.find("bindingFact->bindingTypeTextId", aggregateTypeResolverPos);
  const size_t aggregateQueryBindingIdPos =
      source.find("queryFact->bindingTypeTextId", aggregateBindingIdPos);
  const size_t aggregateQueryTypeIdPos =
      source.find("queryFact->queryTypeTextId", aggregateQueryBindingIdPos);
  const size_t staleAggregatePos =
      source.find("stale semantic-product pick aggregate result metadata",
                  aggregateResolverPos);
  const size_t aggregateFallbackPos =
      source.find("branchStructPath = inferStructExprPath(*valueExpr, branchLocals)",
                  aggregateResolverPos);
  REQUIRE(aggregateBindingFactPos != std::string::npos);
  REQUIRE(aggregateQueryFactPos != std::string::npos);
  REQUIRE(aggregateTypeResolverPos != std::string::npos);
  REQUIRE(aggregateSemanticResolvePos != std::string::npos);
  REQUIRE(aggregateBindingIdPos != std::string::npos);
  REQUIRE(aggregateQueryBindingIdPos != std::string::npos);
  REQUIRE(aggregateQueryTypeIdPos != std::string::npos);
  REQUIRE(staleAggregatePos != std::string::npos);
  REQUIRE(aggregateFallbackPos != std::string::npos);
  CHECK(aggregateTypeResolverPos < aggregateBindingFactPos);
  CHECK(aggregateSemanticResolvePos < aggregateBindingIdPos);
  CHECK(aggregateBindingIdPos < aggregateQueryBindingIdPos);
  CHECK(aggregateQueryBindingIdPos < aggregateQueryTypeIdPos);
  CHECK(aggregateBindingFactPos < aggregateFallbackPos);
  CHECK(aggregateQueryFactPos < aggregateFallbackPos);
  CHECK(aggregateQueryTypeIdPos < aggregateFallbackPos);
  CHECK(source.find("if (!resolvedBySemanticProduct.has_value())", aggregateResolverPos) !=
        std::string::npos);
  CHECK(source.find("validateSemanticProductPickArmVariant") == std::string::npos);
  CHECK(source.find("makePickPayloadLocalInfo(sumDef, *variant, publishedVariant, payloadInfo)") ==
        std::string::npos);
  CHECK(source.find("publishedVariant, arm.args.front(), sumPtrLocal, branchLocals") ==
        std::string::npos);
}

TEST_CASE("native pick target sum resolution resolves interned type ids") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path lowerSumHelpersPath =
      cwd / "src" / "ir_lowerer" / "IrLowererLowerSumHelpers.cpp";
  if (!std::filesystem::exists(lowerSumHelpersPath)) {
    lowerSumHelpersPath =
        cwd.parent_path() / "src" / "ir_lowerer" / "IrLowererLowerSumHelpers.cpp";
  }
  REQUIRE(std::filesystem::exists(lowerSumHelpersPath));

  const std::string source = readTextFile(lowerSumHelpersPath);
  const size_t resolverPos =
      source.find("resolveSemanticProductPickTargetSumDefinition");
  REQUIRE(resolverPos != std::string::npos);
  const size_t typeResolverPos =
      source.find("resolveSemanticProductTypeText", resolverPos);
  const size_t semanticResolvePos =
      source.find("semanticProgramResolveCallTargetString(", typeResolverPos);
  const size_t bindingFactPos =
      source.find("findSemanticProductBindingFact(semanticTargets, targetExpr)",
                  typeResolverPos);
  const size_t bindingScopePos =
      source.find("findSemanticProductBindingFactByScopeAndName", resolverPos);
  const size_t bindingIdPos =
      source.find("bindingFact->bindingTypeTextId", typeResolverPos);
  const size_t queryBindingIdPos =
      source.find("queryFact->bindingTypeTextId", bindingIdPos);
  const size_t queryTypeIdPos =
      source.find("queryFact->queryTypeTextId", queryBindingIdPos);
  const size_t returnIdPos =
      source.find("returnFact->bindingTypeTextId", queryTypeIdPos);
  const size_t metadataPos =
      source.find("return requirePublishedSumMetadata(*querySumDef, targetExpr)",
                  returnIdPos);
  REQUIRE(typeResolverPos != std::string::npos);
  REQUIRE(semanticResolvePos != std::string::npos);
  REQUIRE(bindingFactPos != std::string::npos);
  REQUIRE(bindingScopePos == std::string::npos);
  REQUIRE(bindingIdPos != std::string::npos);
  REQUIRE(queryBindingIdPos != std::string::npos);
  REQUIRE(queryTypeIdPos != std::string::npos);
  REQUIRE(returnIdPos != std::string::npos);
  REQUIRE(metadataPos != std::string::npos);
  CHECK(typeResolverPos < semanticResolvePos);
  CHECK(semanticResolvePos < bindingFactPos);
  CHECK(bindingFactPos < bindingIdPos);
  CHECK(bindingIdPos < queryBindingIdPos);
  CHECK(queryBindingIdPos < queryTypeIdPos);
  CHECK(queryTypeIdPos < returnIdPos);
  CHECK(returnIdPos < metadataPos);
}

TEST_CASE("native sum construction uses semantic-product variant tags") {
  const std::string source = R"(
[sum]
Choice {
  [i32] left
  [i32] right
}

[return<i32>]
main() {
  [Choice] choice{[right] 41i32}
  return(0i32)
}
)";

  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidateThroughCompilePipeline(
      source, program, &semanticProgram, error, {}, {}));
  CHECK(error.empty());

  primec::Options options;
  options.entryPath = "/main";
  primec::IrModule ir;
  primec::IrPreparationFailure failure;
  primec::Program loweringProgram = program;
  primec::SemanticProgram loweringSemanticProgram = semanticProgram;
  REQUIRE(primec::prepareIrModule(loweringProgram,
                                  &loweringSemanticProgram,
                                  options,
                                  primec::IrValidationTarget::Native,
                                  ir,
                                  failure));
  CHECK(!ir.functions.empty());

  auto rewriteChoiceRightVariantTag =
      [](primec::SemanticProgram &semanticProduct, uint32_t tagValue) {
        for (auto &entry : semanticProduct.sumVariantMetadata) {
          if (entry.sumPath == "/Choice" && entry.variantName == "right") {
            entry.tagValue = tagValue;
            return true;
          }
        }
        return false;
      };

  primec::Program staleProgram = program;
  primec::SemanticProgram staleSemanticProgram = semanticProgram;
  REQUIRE(rewriteChoiceRightVariantTag(staleSemanticProgram, 42));

  primec::IrModule staleIr;
  primec::IrPreparationFailure staleFailure;
  CHECK_FALSE(primec::prepareIrModule(staleProgram,
                                      &staleSemanticProgram,
                                      options,
                                      primec::IrValidationTarget::Native,
                                      staleIr,
                                      staleFailure));
  CHECK(staleFailure.stage == primec::IrPreparationFailureStage::Lowering);
  CHECK(staleFailure.message ==
        "stale semantic-product sum variant metadata for sum slot layout: /Choice -> right");
  CHECK(staleFailure.diagnosticInfo.message == staleFailure.message);
}

TEST_CASE("native sum slot layout uses semantic-product variant metadata") {
  const std::string source = R"(
[struct]
LeftPayload() {
  [i32] value{0i32}
}

[struct]
RightPayload() {
  [i32] value{0i32}
}

[sum]
Choice {
  [LeftPayload] left
  [RightPayload] right
}

[return<i32>]
main() {
  [Choice] choice{[left] LeftPayload{7i32}}
  return(0i32)
}
)";

  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidateThroughCompilePipeline(
      source, program, &semanticProgram, error, {}, {}));
  CHECK(error.empty());

  primec::Options options;
  options.entryPath = "/main";
  primec::IrModule ir;
  primec::IrPreparationFailure failure;
  primec::Program loweringProgram = program;
  primec::SemanticProgram loweringSemanticProgram = semanticProgram;
  REQUIRE(primec::prepareIrModule(loweringProgram,
                                  &loweringSemanticProgram,
                                  options,
                                  primec::IrValidationTarget::Native,
                                  ir,
                                  failure));
  CHECK(!ir.functions.empty());

  auto rewriteChoiceRightVariantPayload =
      [](primec::SemanticProgram &semanticProduct, const std::string &payloadTypeText) {
        for (auto &entry : semanticProduct.sumVariantMetadata) {
          if (entry.sumPath == "/Choice" && entry.variantName == "right") {
            entry.payloadTypeText = payloadTypeText;
            return true;
          }
        }
        return false;
      };

  primec::Program staleProgram = program;
  primec::SemanticProgram staleSemanticProgram = semanticProgram;
  REQUIRE(rewriteChoiceRightVariantPayload(staleSemanticProgram, "i64"));

  primec::IrModule staleIr;
  primec::IrPreparationFailure staleFailure;
  CHECK_FALSE(primec::prepareIrModule(staleProgram,
                                      &staleSemanticProgram,
                                      options,
                                      primec::IrValidationTarget::Native,
                                      staleIr,
                                      staleFailure));
  CHECK(staleFailure.stage == primec::IrPreparationFailureStage::Lowering);
  CHECK(staleFailure.message ==
        "stale semantic-product sum variant metadata for sum slot layout: /Choice -> right");
  CHECK(staleFailure.diagnosticInfo.message == staleFailure.message);
}

TEST_CASE("native sum variant selection uses semantic-product payload metadata") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path lowerSumHelpersPath =
      cwd / "src" / "ir_lowerer" / "IrLowererLowerSumHelpers.cpp";
  if (!std::filesystem::exists(lowerSumHelpersPath)) {
    lowerSumHelpersPath =
        cwd.parent_path() / "src" / "ir_lowerer" / "IrLowererLowerSumHelpers.cpp";
  }
  REQUIRE(std::filesystem::exists(lowerSumHelpersPath));

  const std::string source = readTextFile(lowerSumHelpersPath);
  const size_t initializerShapePos =
      source.find("resolveSemanticProductInitializerPayloadShape");
  const size_t initializerBindingFactPos =
      source.find("findSemanticProductBindingFact(semanticTargets, initializer)",
                  initializerShapePos);
  const size_t initializerQueryFactPos =
      source.find("findSemanticProductQueryFact(semanticTargets, initializer)",
                  initializerShapePos);
  const size_t initializerTypeResolverPos =
      source.find("addSemanticProductCandidateTypeText", initializerShapePos);
  const size_t initializerSemanticResolvePos =
      source.find("semanticProgramResolveCallTargetString(",
                  initializerTypeResolverPos);
  const size_t initializerBindingIdPos =
      source.find("bindingFact->bindingTypeTextId", initializerTypeResolverPos);
  const size_t initializerQueryBindingIdPos =
      source.find("queryFact->bindingTypeTextId", initializerBindingIdPos);
  const size_t initializerQueryTypeIdPos =
      source.find("queryFact->queryTypeTextId", initializerQueryBindingIdPos);
  const size_t staleInitializerTypePos =
      source.find("stale semantic-product sum initializer type metadata",
                  initializerShapePos);
  const size_t initializerFallbackKindPos =
      source.find("initializerKind = inferExprKind(initializer, valueLocals)",
                  initializerShapePos);
  REQUIRE(initializerShapePos != std::string::npos);
  REQUIRE(initializerBindingFactPos != std::string::npos);
  REQUIRE(initializerQueryFactPos != std::string::npos);
  REQUIRE(initializerTypeResolverPos != std::string::npos);
  REQUIRE(initializerSemanticResolvePos != std::string::npos);
  REQUIRE(initializerBindingIdPos != std::string::npos);
  REQUIRE(initializerQueryBindingIdPos != std::string::npos);
  REQUIRE(initializerQueryTypeIdPos != std::string::npos);
  REQUIRE(staleInitializerTypePos != std::string::npos);
  REQUIRE(initializerFallbackKindPos != std::string::npos);
  CHECK(initializerTypeResolverPos < initializerBindingFactPos);
  CHECK(initializerSemanticResolvePos < initializerBindingIdPos);
  CHECK(initializerBindingIdPos < initializerQueryBindingIdPos);
  CHECK(initializerQueryBindingIdPos < initializerQueryTypeIdPos);
  CHECK(initializerBindingFactPos < initializerFallbackKindPos);
  CHECK(initializerQueryFactPos < initializerFallbackKindPos);
  CHECK(initializerQueryTypeIdPos < initializerFallbackKindPos);
  CHECK(source.find("if (!resolvedInitializerShape.has_value())") != std::string::npos);
  CHECK(source.find("\"sum constructor selection\"") != std::string::npos);
  CHECK(source.find("\"Result.ok selection\"") != std::string::npos);
  CHECK(source.find("\"sum initializer selection\"") != std::string::npos);
  CHECK(source.find("targetSum, *variant, \"sum constructor selection\", payloadInfo") !=
        std::string::npos);
  CHECK(source.find("targetSum, *variant, \"Result.ok selection\", payloadInfo") !=
        std::string::npos);
  CHECK(source.find("targetSum, variant, \"sum initializer selection\", payloadInfo") !=
        std::string::npos);
  CHECK(source.find("resolveSumPayloadStorageInfo(targetSum, *variant, payloadInfo)") ==
        std::string::npos);
  CHECK(source.find("resolveSumPayloadStorageInfo(targetSum, variant, payloadInfo)") ==
        std::string::npos);
}

TEST_CASE("native Result combinators use semantic-product variant tags") {
  const std::string source = R"(
import /std/file/*

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(err.why())
}

[return<i32> effects(io_err) on_error<FileError, /log_file_error>]
main() {
  [Result<i32, FileError>] source{Result.ok(2i32)}
  [Result<i32, FileError>] mapped{
    Result.map(source, []([i32] value) { return(plus(value, 1i32)) })
  }
  [Result<i32, FileError>] chained{
    Result.and_then(mapped, []([i32] value) { return(Result.ok(plus(value, 3i32))) })
  }
  [Result<i32, FileError>] combined{
    Result.map2(chained, Result.ok(5i32), []([i32] left, [i32] right) {
      return(plus(left, right))
    })
  }
  return(try(combined))
}
)";

  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidateThroughCompilePipeline(
      source, program, &semanticProgram, error, {}, {}));
  CHECK(error.empty());

  primec::Options options;
  options.entryPath = "/main";
  primec::IrModule ir;
  primec::IrPreparationFailure failure;
  primec::Program loweringProgram = program;
  primec::SemanticProgram loweringSemanticProgram = semanticProgram;
  REQUIRE(primec::prepareIrModule(loweringProgram,
                                  &loweringSemanticProgram,
                                  options,
                                  primec::IrValidationTarget::Native,
                                  ir,
                                  failure));
  CHECK(!ir.functions.empty());

  auto rewriteStdlibResultErrorVariantTag =
      [](primec::SemanticProgram &semanticProduct, uint32_t tagValue) {
        std::vector<std::string> valueResultPaths;
        for (const auto &entry : semanticProduct.sumVariantMetadata) {
          const bool isResultSum = entry.sumPath.find("Result") != std::string::npos;
          if (isResultSum && entry.variantName == "ok" &&
              entry.payloadTypeText.find("i32") != std::string::npos) {
            valueResultPaths.push_back(entry.sumPath);
          }
        }
        bool rewritten = false;
        for (auto &entry : semanticProduct.sumVariantMetadata) {
          if (std::find(valueResultPaths.begin(), valueResultPaths.end(), entry.sumPath) !=
                  valueResultPaths.end() &&
              entry.variantName == "error") {
            entry.tagValue = tagValue;
            rewritten = true;
          }
        }
        return rewritten;
      };

  primec::Program staleProgram = program;
  primec::SemanticProgram staleSemanticProgram = semanticProgram;
  if (!rewriteStdlibResultErrorVariantTag(staleSemanticProgram, 42)) {
    CHECK(std::none_of(semanticProgram.sumVariantMetadata.begin(),
                       semanticProgram.sumVariantMetadata.end(),
                       [](const primec::SemanticProgramSumVariantMetadata &entry) {
                         return entry.sumPath.find("Result") != std::string::npos;
                       }));
    return;
  }

  primec::IrModule staleIr;
  primec::IrPreparationFailure staleFailure;
  CHECK_FALSE(primec::prepareIrModule(staleProgram,
                                      &staleSemanticProgram,
                                      options,
                                      primec::IrValidationTarget::Native,
                                      staleIr,
                                      staleFailure));
  CHECK(staleFailure.stage == primec::IrPreparationFailureStage::Lowering);
  CHECK(staleFailure.message.find(
            "stale semantic-product sum variant metadata for Result.map target error: ") !=
        std::string::npos);
  CHECK(staleFailure.diagnosticInfo.message == staleFailure.message);
}

TEST_CASE("native Result combinator payload storage uses semantic-product variant metadata") {
  const std::string source = R"(
import /std/file/*

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(err.why())
}

[return<i32> effects(io_err) on_error<FileError, /log_file_error>]
main() {
  [Result<i32, FileError>] source{Result.ok(2i32)}
  [Result<bool, FileError>] mapped{
    Result.map(source, []([i32] value) { return(true) })
  }
  [Result<i32, FileError>] chained{
    Result.and_then(mapped, []([bool] flag) { return(Result.ok(3i32)) })
  }
  [Result<i32, FileError>] combined{
    Result.map2(chained, Result.ok(5i32), []([i32] left, [i32] right) {
      return(plus(left, right))
    })
  }
  return(try(combined))
}
)";

  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidateThroughCompilePipeline(
      source, program, &semanticProgram, error, {}, {}));
  CHECK(error.empty());

  primec::Options options;
  options.entryPath = "/main";
  primec::IrModule ir;
  primec::IrPreparationFailure failure;
  primec::Program loweringProgram = program;
  primec::SemanticProgram loweringSemanticProgram = semanticProgram;
  REQUIRE(primec::prepareIrModule(loweringProgram,
                                  &loweringSemanticProgram,
                                  options,
                                  primec::IrValidationTarget::Native,
                                  ir,
                                  failure));
  CHECK(!ir.functions.empty());

  auto rewriteSourceResultErrorPayload =
      [](primec::SemanticProgram &semanticProduct, std::string payloadTypeText) {
        std::vector<std::string> sourceResultPaths;
        for (const auto &entry : semanticProduct.sumVariantMetadata) {
          const bool isResultSum = entry.sumPath.find("Result") != std::string::npos;
          if (isResultSum && entry.variantName == "ok" &&
              entry.payloadTypeText.find("i32") != std::string::npos) {
            sourceResultPaths.push_back(entry.sumPath);
          }
        }
        bool rewritten = false;
        for (auto &entry : semanticProduct.sumVariantMetadata) {
          if (std::find(sourceResultPaths.begin(), sourceResultPaths.end(), entry.sumPath) !=
                  sourceResultPaths.end() &&
              entry.variantName == "error") {
            entry.payloadTypeText = payloadTypeText;
            rewritten = true;
          }
        }
        return rewritten;
      };

  primec::Program staleProgram = program;
  primec::SemanticProgram staleSemanticProgram = semanticProgram;
  if (!rewriteSourceResultErrorPayload(staleSemanticProgram, "i64")) {
    CHECK(std::none_of(semanticProgram.sumVariantMetadata.begin(),
                       semanticProgram.sumVariantMetadata.end(),
                       [](const primec::SemanticProgramSumVariantMetadata &entry) {
                         return entry.sumPath.find("Result") != std::string::npos;
                       }));
    return;
  }

  primec::IrModule staleIr;
  primec::IrPreparationFailure staleFailure;
  CHECK_FALSE(primec::prepareIrModule(staleProgram,
                                      &staleSemanticProgram,
                                      options,
                                      primec::IrValidationTarget::Native,
                                      staleIr,
                                      staleFailure));
  CHECK(staleFailure.stage == primec::IrPreparationFailureStage::Lowering);
  CHECK(staleFailure.message.find(
            "stale semantic-product sum variant metadata for Result.map source error payload: ") !=
        std::string::npos);
  CHECK(staleFailure.diagnosticInfo.message == staleFailure.message);
}

TEST_CASE("native Result combinator sources use semantic-product query facts") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path lowerSumHelpersPath =
      cwd / "src" / "ir_lowerer" / "IrLowererLowerSumHelpers.cpp";
  if (!std::filesystem::exists(lowerSumHelpersPath)) {
    lowerSumHelpersPath =
        cwd.parent_path() / "src" / "ir_lowerer" / "IrLowererLowerSumHelpers.cpp";
  }
  REQUIRE(std::filesystem::exists(lowerSumHelpersPath));

  const std::string source = readTextFile(lowerSumHelpersPath);
  const size_t semanticResolverPos =
      source.find("resolveSemanticProductResultSumSourceDefinition");
  REQUIRE(semanticResolverPos != std::string::npos);
  const size_t queryFactPos =
      source.find("findSemanticProductQueryFact(", semanticResolverPos);
  const size_t typeResolverPos =
      source.find("auto resolveQueryTypeText", queryFactPos);
  const size_t internedTypeTextPos =
      source.find("semanticProgramResolveCallTargetString(", typeResolverPos);
  const size_t bindingTextIdPos =
      source.find("queryFact->bindingTypeTextId", typeResolverPos);
  const size_t queryTextIdPos =
      source.find("queryFact->queryTypeTextId", bindingTextIdPos);
  const size_t sumDefinitionPos =
      source.find("resolveSumDefinitionForTypeText", typeResolverPos);
  const size_t staleDiagnosticPos =
      source.find("stale semantic-product Result-combinator source query metadata",
                  semanticResolverPos);
  const size_t fallbackStructPathPos =
      source.find("const std::string sourceStructPath = inferStructExprPath(sourceExpr, sourceLocals)",
                  semanticResolverPos);
  REQUIRE(queryFactPos != std::string::npos);
  REQUIRE(typeResolverPos != std::string::npos);
  REQUIRE(internedTypeTextPos != std::string::npos);
  REQUIRE(bindingTextIdPos != std::string::npos);
  REQUIRE(queryTextIdPos != std::string::npos);
  REQUIRE(sumDefinitionPos != std::string::npos);
  REQUIRE(staleDiagnosticPos != std::string::npos);
  REQUIRE(fallbackStructPathPos != std::string::npos);
  CHECK(queryFactPos < typeResolverPos);
  CHECK(typeResolverPos < internedTypeTextPos);
  CHECK(internedTypeTextPos < sumDefinitionPos);
  CHECK(typeResolverPos < bindingTextIdPos);
  CHECK(bindingTextIdPos < queryTextIdPos);
  CHECK(queryFactPos < fallbackStructPathPos);
  CHECK(source.find("if (!resolvedBySemanticProductQuery.has_value())") != std::string::npos);
  CHECK(source.find("resolveQueryTypeText(queryFact->bindingTypeText);",
                    semanticResolverPos) == std::string::npos);
}

TEST_CASE("native Result why direct-call sources use semantic-product query facts") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path resultWhyHelpersPath =
      cwd / "src" / "ir_lowerer" / "IrLowererResultWhyHelpers.cpp";
  if (!std::filesystem::exists(resultWhyHelpersPath)) {
    resultWhyHelpersPath =
        cwd.parent_path() / "src" / "ir_lowerer" / "IrLowererResultWhyHelpers.cpp";
  }
  REQUIRE(std::filesystem::exists(resultWhyHelpersPath));

  const std::string source = readTextFile(resultWhyHelpersPath);
  const size_t resolverPos =
      source.find("resolveSemanticQueryResultErrorTypeText");
  const size_t semanticResolvePos =
      source.find("semanticProgramResolveCallTargetString(", resolverPos);
  const size_t errorTypeIdPos =
      source.find("queryFact.resultErrorTypeId", resolverPos);
  const size_t directCallResolverPos =
      source.find("directCallReturnsImportedStdlibResultSum");
  REQUIRE(directCallResolverPos != std::string::npos);
  const size_t queryFactPos =
      source.find("findSemanticProductQueryFact(", directCallResolverPos);
  const size_t missingDiagnosticPos =
      source.find("missing semantic-product Result.why source query fact",
                  directCallResolverPos);
  const size_t staleDiagnosticPos =
      source.find("stale semantic-product Result.why source query metadata",
                  directCallResolverPos);
  const size_t fallbackTransformScanPos =
      source.find("for (const auto &transform : calleeDef->transforms)",
                  directCallResolverPos);
  REQUIRE(queryFactPos != std::string::npos);
  REQUIRE(missingDiagnosticPos != std::string::npos);
  REQUIRE(staleDiagnosticPos != std::string::npos);
  REQUIRE(fallbackTransformScanPos != std::string::npos);
  REQUIRE(resolverPos != std::string::npos);
  REQUIRE(semanticResolvePos != std::string::npos);
  REQUIRE(errorTypeIdPos != std::string::npos);
  CHECK(resolverPos < directCallResolverPos);
  CHECK(resolverPos < errorTypeIdPos);
  CHECK(errorTypeIdPos < semanticResolvePos);
  CHECK(queryFactPos < fallbackTransformScanPos);
  CHECK(staleDiagnosticPos < fallbackTransformScanPos);
  CHECK(source.find("trimTemplateTypeText(queryFact->resultErrorType)") ==
        std::string::npos);
}

TEST_CASE("native Result error direct-call sources use semantic-product query facts") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path packedResultHelpersPath =
      cwd / "src" / "ir_lowerer" / "IrLowererPackedResultHelpers.cpp";
  if (!std::filesystem::exists(packedResultHelpersPath)) {
    packedResultHelpersPath =
        cwd.parent_path() / "src" / "ir_lowerer" / "IrLowererPackedResultHelpers.cpp";
  }
  REQUIRE(std::filesystem::exists(packedResultHelpersPath));

  const std::string source = readTextFile(packedResultHelpersPath);
  const size_t resolverPos =
      source.find("resolveSemanticProductQueryErrorTypeText");
  const size_t semanticResolvePos =
      source.find("semanticProgramResolveCallTargetString(", resolverPos);
  const size_t errorTypeIdPos =
      source.find("queryFact.resultErrorTypeId", resolverPos);
  const size_t directCallResolverPos =
      source.find("directCallReturnsImportedStdlibResultSum");
  REQUIRE(directCallResolverPos != std::string::npos);
  const size_t queryFactPos =
      source.find("findSemanticProductQueryFact(", directCallResolverPos);
  const size_t missingDiagnosticPos =
      source.find("missing semantic-product Result.error source query fact",
                  directCallResolverPos);
  const size_t staleDiagnosticPos =
      source.find("stale semantic-product Result.error source query metadata",
                  directCallResolverPos);
  const size_t fallbackTransformScanPos =
      source.find("for (const auto &transform : calleeDef->transforms)",
                  directCallResolverPos);
  REQUIRE(queryFactPos != std::string::npos);
  REQUIRE(missingDiagnosticPos != std::string::npos);
  REQUIRE(staleDiagnosticPos != std::string::npos);
  REQUIRE(fallbackTransformScanPos != std::string::npos);
  REQUIRE(resolverPos != std::string::npos);
  REQUIRE(semanticResolvePos != std::string::npos);
  REQUIRE(errorTypeIdPos != std::string::npos);
  CHECK(resolverPos < directCallResolverPos);
  CHECK(resolverPos < errorTypeIdPos);
  CHECK(errorTypeIdPos < semanticResolvePos);
  CHECK(queryFactPos < fallbackTransformScanPos);
  CHECK(staleDiagnosticPos < fallbackTransformScanPos);
  CHECK(source.find("trimTemplateTypeText(queryFact->resultErrorType)") ==
        std::string::npos);
}

TEST_CASE("native aggregate pointer dereference calls use semantic-product return facts") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path operatorMemoryPointerHelpersPath =
      cwd / "src" / "ir_lowerer" / "IrLowererOperatorMemoryPointerHelpers.cpp";
  if (!std::filesystem::exists(operatorMemoryPointerHelpersPath)) {
    operatorMemoryPointerHelpersPath =
        cwd.parent_path() / "src" / "ir_lowerer" / "IrLowererOperatorMemoryPointerHelpers.cpp";
  }
  REQUIRE(std::filesystem::exists(operatorMemoryPointerHelpersPath));

  const std::string source = readTextFile(operatorMemoryPointerHelpersPath);
  const size_t helperPos = source.find("resolveSemanticReturnFactBindingType");
  REQUIRE(helperPos != std::string::npos);
  const size_t idPos = source.find("returnFact.bindingTypeTextId", helperPos);
  const size_t semanticResolvePos =
      source.find("semanticProgramResolveCallTargetString(", idPos);
  const size_t fallbackTextPos =
      source.find("return trimTemplateTypeText(returnFact.bindingTypeText)", semanticResolvePos);
  REQUIRE(idPos != std::string::npos);
  REQUIRE(semanticResolvePos != std::string::npos);
  REQUIRE(fallbackTextPos != std::string::npos);
  CHECK(helperPos < idPos);
  CHECK(idPos < semanticResolvePos);
  CHECK(semanticResolvePos < fallbackTextPos);

  const size_t resolverPos = source.find("resolveAggregatePointerLikeCallExpr");
  REQUIRE(resolverPos != std::string::npos);
  const size_t returnFactPos =
      source.find("findSemanticProductReturnFactByPath", resolverPos);
  const size_t helperCallPos =
      source.find("resolveSemanticReturnFactBindingType", returnFactPos);
  const size_t missingDiagnosticPos =
      source.find("missing semantic-product aggregate pointer return metadata",
                  resolverPos);
  const size_t fallbackTransformScanPos =
      source.find("for (const auto &transform : callee->transforms)",
                  resolverPos);
  REQUIRE(returnFactPos != std::string::npos);
  REQUIRE(helperCallPos != std::string::npos);
  REQUIRE(missingDiagnosticPos != std::string::npos);
  REQUIRE(fallbackTransformScanPos != std::string::npos);
  CHECK(returnFactPos < fallbackTransformScanPos);
  CHECK(returnFactPos < helperCallPos);
  CHECK(helperCallPos < fallbackTransformScanPos);
  CHECK(missingDiagnosticPos < fallbackTransformScanPos);
}

TEST_CASE("native location direct reference calls use semantic-product return facts") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path operatorMemoryPointerHelpersPath =
      cwd / "src" / "ir_lowerer" / "IrLowererOperatorMemoryPointerHelpers.cpp";
  if (!std::filesystem::exists(operatorMemoryPointerHelpersPath)) {
    operatorMemoryPointerHelpersPath =
        cwd.parent_path() / "src" / "ir_lowerer" / "IrLowererOperatorMemoryPointerHelpers.cpp";
  }
  REQUIRE(std::filesystem::exists(operatorMemoryPointerHelpersPath));

  const std::string source = readTextFile(operatorMemoryPointerHelpersPath);
  const size_t helperPos = source.find("resolveSemanticReturnFactBindingType");
  REQUIRE(helperPos != std::string::npos);
  const size_t idPos = source.find("returnFact.bindingTypeTextId", helperPos);
  const size_t semanticResolvePos =
      source.find("semanticProgramResolveCallTargetString(", idPos);
  const size_t fallbackTextPos =
      source.find("return trimTemplateTypeText(returnFact.bindingTypeText)", semanticResolvePos);
  REQUIRE(idPos != std::string::npos);
  REQUIRE(semanticResolvePos != std::string::npos);
  REQUIRE(fallbackTextPos != std::string::npos);
  CHECK(helperPos < idPos);
  CHECK(idPos < semanticResolvePos);
  CHECK(semanticResolvePos < fallbackTextPos);

  const size_t resolverPos = source.find("resolveReferenceReturnCallExpr");
  REQUIRE(resolverPos != std::string::npos);
  const size_t returnFactPos =
      source.find("findSemanticProductReturnFactByPath", resolverPos);
  const size_t helperCallPos =
      source.find("resolveSemanticReturnFactBindingType", returnFactPos);
  const size_t missingDiagnosticPos =
      source.find("missing semantic-product location reference return metadata",
                  resolverPos);
  const size_t fallbackTransformScanPos =
      source.find("for (const auto &transform : callee->transforms)",
                  resolverPos);
  REQUIRE(returnFactPos != std::string::npos);
  REQUIRE(helperCallPos != std::string::npos);
  REQUIRE(missingDiagnosticPos != std::string::npos);
  REQUIRE(fallbackTransformScanPos != std::string::npos);
  CHECK(returnFactPos < fallbackTransformScanPos);
  CHECK(returnFactPos < helperCallPos);
  CHECK(helperCallPos < fallbackTransformScanPos);
  CHECK(missingDiagnosticPos < fallbackTransformScanPos);
}

TEST_CASE("native field receivers use semantic-product type facts") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path lowerEmitExprPath =
      cwd / "src" / "ir_lowerer" / "IrLowererLowerEmitExpr.h";
  if (!std::filesystem::exists(lowerEmitExprPath)) {
    lowerEmitExprPath =
        cwd.parent_path() / "src" / "ir_lowerer" / "IrLowererLowerEmitExpr.h";
  }
  REQUIRE(std::filesystem::exists(lowerEmitExprPath));

  const std::string source = readTextFile(lowerEmitExprPath);
  const size_t sharedTypeResolverPos =
      source.find("auto appendSemanticProductTypeTextCandidate");
  const size_t semanticResolverPos =
      source.find("resolveSemanticProductFieldReceiverStructPath");
  REQUIRE(sharedTypeResolverPos != std::string::npos);
  REQUIRE(semanticResolverPos != std::string::npos);
  const size_t bindingFactPos =
      source.find("findSemanticProductBindingFact(semanticTargets, receiverExpr)",
                  semanticResolverPos);
  const size_t queryFactPos =
      source.find("findSemanticProductQueryFact(semanticTargets, receiverExpr)",
                  semanticResolverPos);
  const size_t fieldQueryFactPos =
      source.find("findSemanticProductQueryFact(semanticTargets, expr)",
                  semanticResolverPos);
  const size_t sourceFallbackPos =
      source.find("semanticTargets.semanticProgram->queryFacts",
                  semanticResolverPos);
  const size_t staleDiagnosticPos =
      source.find("stale semantic-product field receiver metadata",
                  semanticResolverPos);
  const size_t fallbackStructPathPos =
      source.find("structPath = inferStructExprPath(receiver, localsIn)",
                  semanticResolverPos);
  const size_t resolveStructTypePos =
      source.find("resolveStructTypeName(normalizedTypeText",
                  semanticResolverPos);
  const size_t primitiveTypePos =
      source.find("valueKindFromTypeName(normalizedTypeText)",
                  semanticResolverPos);
  const size_t internedTypeResolverPos =
      source.find("semanticProgramResolveCallTargetString(",
                  sharedTypeResolverPos);
  REQUIRE(bindingFactPos != std::string::npos);
  REQUIRE(queryFactPos != std::string::npos);
  REQUIRE(fieldQueryFactPos != std::string::npos);
  REQUIRE(sourceFallbackPos != std::string::npos);
  REQUIRE(internedTypeResolverPos != std::string::npos);
  REQUIRE(staleDiagnosticPos != std::string::npos);
  REQUIRE(fallbackStructPathPos != std::string::npos);
  REQUIRE(resolveStructTypePos != std::string::npos);
  REQUIRE(primitiveTypePos != std::string::npos);
  CHECK(source.find("appendSemanticProductTypeTextCandidate(\n"
                    "                  candidateTypeTexts,\n"
                    "                  bindingFact->bindingTypeText,",
                    semanticResolverPos) != std::string::npos);
  CHECK(source.find("bindingFact->bindingTypeTextId",
                    semanticResolverPos) != std::string::npos);
  CHECK(source.find("appendSemanticProductTypeTextCandidate(\n"
                    "                  candidateTypeTexts,\n"
                    "                  queryFact->bindingTypeText,",
                    semanticResolverPos) != std::string::npos);
  CHECK(source.find("queryFact->bindingTypeTextId",
                    semanticResolverPos) != std::string::npos);
  CHECK(source.find("appendSemanticProductTypeTextCandidate(\n"
                    "                  candidateTypeTexts,\n"
                    "                  queryFact->queryTypeText,",
                    semanticResolverPos) != std::string::npos);
  CHECK(source.find("queryFact->queryTypeTextId",
                    semanticResolverPos) != std::string::npos);
  CHECK(source.find("candidate.sourceLine != expr.sourceLine",
                    semanticResolverPos) != std::string::npos);
  CHECK(source.find("candidate.sourceColumn != expr.sourceColumn",
                    semanticResolverPos) != std::string::npos);
  CHECK(source.find("candidate.scopePath == function.name",
                    semanticResolverPos) != std::string::npos);
  CHECK(source.find("fieldQueryFact->receiverBindingTypeText",
                    semanticResolverPos) != std::string::npos);
  CHECK(source.find("fieldQueryFact->receiverBindingTypeTextId",
                    semanticResolverPos) != std::string::npos);
  CHECK(source.find("splitTemplateTypeName(normalizedTypeText, wrapperBase, wrapperArgs)",
                    semanticResolverPos) != std::string::npos);
  CHECK(internedTypeResolverPos < fallbackStructPathPos);
  CHECK(sharedTypeResolverPos < semanticResolverPos);
  CHECK(bindingFactPos < fallbackStructPathPos);
  CHECK(queryFactPos < fallbackStructPathPos);
  CHECK(fieldQueryFactPos < fallbackStructPathPos);
  CHECK(sourceFallbackPos < fallbackStructPathPos);
  CHECK(resolveStructTypePos < primitiveTypePos);
  CHECK(source.find("auto addCandidateTypeText", semanticResolverPos) ==
        std::string::npos);
  CHECK(source.find("if (!resolvedFieldReceiverBySemanticProduct.has_value())",
                    semanticResolverPos) != std::string::npos);
}

TEST_CASE("native packed Result payloads use semantic-product type facts") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path lowerEmitExprPath =
      cwd / "src" / "ir_lowerer" / "IrLowererLowerEmitExpr.h";
  if (!std::filesystem::exists(lowerEmitExprPath)) {
    lowerEmitExprPath =
        cwd.parent_path() / "src" / "ir_lowerer" / "IrLowererLowerEmitExpr.h";
  }
  REQUIRE(std::filesystem::exists(lowerEmitExprPath));

  const std::string source = readTextFile(lowerEmitExprPath);
  const size_t sharedTypeResolverPos =
      source.find("auto appendSemanticProductTypeTextCandidate");
  const size_t semanticResolverPos =
      source.find("resolveSemanticProductPackedResultPayload");
  REQUIRE(sharedTypeResolverPos != std::string::npos);
  REQUIRE(semanticResolverPos != std::string::npos);
  const size_t bindingFactPos =
      source.find("findSemanticProductBindingFact(semanticTargets, valueExpr)",
                  semanticResolverPos);
  const size_t queryFactPos =
      source.find("findSemanticProductQueryFact(semanticTargets, valueExpr)",
                  semanticResolverPos);
  const size_t collectionTypePos =
      source.find("resolveSupportedResultCollectionType(normalizedTypeText",
                  semanticResolverPos);
  const size_t staleDiagnosticPos =
      source.find("stale semantic-product packed Result payload metadata",
                  semanticResolverPos);
  const size_t fallbackKindPos =
      source.find("inferredValueKind = inferExprKind(valueExpr, valueLocals)",
                  semanticResolverPos);
  const size_t fallbackStructPos =
      source.find("inferPackedResultStructType(",
                  semanticResolverPos);
  const size_t collectionFallbackPos =
      source.find("resolveCollectionPayload(collectionKind, collectionValueKind)",
                  semanticResolverPos);
  const size_t internedTypeResolverPos =
      source.find("semanticProgramResolveCallTargetString(",
                  sharedTypeResolverPos);
  REQUIRE(bindingFactPos != std::string::npos);
  REQUIRE(queryFactPos != std::string::npos);
  REQUIRE(internedTypeResolverPos != std::string::npos);
  REQUIRE(collectionTypePos != std::string::npos);
  REQUIRE(staleDiagnosticPos != std::string::npos);
  REQUIRE(fallbackKindPos != std::string::npos);
  REQUIRE(fallbackStructPos != std::string::npos);
  REQUIRE(collectionFallbackPos != std::string::npos);
  CHECK(source.find("appendSemanticProductTypeTextCandidate(\n"
                    "                  candidateTypeTexts,\n"
                    "                  bindingFact->bindingTypeText,",
                    semanticResolverPos) != std::string::npos);
  CHECK(source.find("bindingFact->bindingTypeTextId",
                    semanticResolverPos) != std::string::npos);
  CHECK(source.find("appendSemanticProductTypeTextCandidate(\n"
                    "                  candidateTypeTexts,\n"
                    "                  queryFact->bindingTypeText,",
                    semanticResolverPos) != std::string::npos);
  CHECK(source.find("queryFact->bindingTypeTextId",
                    semanticResolverPos) != std::string::npos);
  CHECK(source.find("appendSemanticProductTypeTextCandidate(\n"
                    "                  candidateTypeTexts,\n"
                    "                  queryFact->queryTypeText,",
                    semanticResolverPos) != std::string::npos);
  CHECK(source.find("queryFact->queryTypeTextId",
                    semanticResolverPos) != std::string::npos);
  CHECK(internedTypeResolverPos < collectionFallbackPos);
  CHECK(internedTypeResolverPos < fallbackKindPos);
  CHECK(sharedTypeResolverPos < semanticResolverPos);
  CHECK(bindingFactPos < collectionFallbackPos);
  CHECK(queryFactPos < collectionFallbackPos);
  CHECK(bindingFactPos < fallbackKindPos);
  CHECK(queryFactPos < fallbackKindPos);
  CHECK(source.find("auto addCandidateTypeText", semanticResolverPos) ==
        std::string::npos);
  CHECK(source.find("if (!resolvedPackedResultPayloadBySemanticProduct.has_value())",
                    semanticResolverPos) != std::string::npos);
}

TEST_CASE("direct Result ok payload metadata uses semantic-product type facts first") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path resultMetadataHelpersPath =
      cwd / "src" / "ir_lowerer" / "IrLowererResultMetadataHelpers.cpp";
  if (!std::filesystem::exists(resultMetadataHelpersPath)) {
    resultMetadataHelpersPath =
        cwd.parent_path() / "src" / "ir_lowerer" / "IrLowererResultMetadataHelpers.cpp";
  }
  REQUIRE(std::filesystem::exists(resultMetadataHelpersPath));

  const std::string source = readTextFile(resultMetadataHelpersPath);
  const size_t metadataPos = source.find("void applyDirectResultValueMetadata");
  REQUIRE(metadataPos != std::string::npos);
  const std::string semanticFactNeedle =
      "applySemanticDirectValueMetadataFact("
      "valueExpr, semanticProgram, semanticIndex, out)";
  const size_t semanticFactPos =
      source.find(semanticFactNeedle, metadataPos);
  const size_t resolverPos =
      source.find("std::string resolveSemanticDirectValueTypeText");
  const size_t idCheckPos =
      source.find("typeTextId != InvalidSymbolId", resolverPos);
  const size_t internedTypeTextPos =
      source.find("semanticProgramResolveCallTargetString", idCheckPos);
  const size_t copiedTextFallbackPos =
      source.find("return trimTemplateTypeText(typeText);", internedTypeTextPos);
  const size_t suppressFallbackPos =
      source.find("suppressSemanticCallDefinitionFallback",
                  metadataPos);
  const size_t collectionFallbackPos =
      source.find("inferDirectResultValueCollectionInfo(",
                  suppressFallbackPos);
  const size_t structFallbackPos =
      source.find("inferDirectResultValueStructType(",
                  suppressFallbackPos);
  REQUIRE(semanticFactPos != std::string::npos);
  REQUIRE(resolverPos != std::string::npos);
  REQUIRE(idCheckPos != std::string::npos);
  REQUIRE(internedTypeTextPos != std::string::npos);
  REQUIRE(copiedTextFallbackPos != std::string::npos);
  REQUIRE(collectionFallbackPos != std::string::npos);
  REQUIRE(structFallbackPos != std::string::npos);
  REQUIRE(suppressFallbackPos != std::string::npos);
  CHECK(idCheckPos < internedTypeTextPos);
  CHECK(internedTypeTextPos < copiedTextFallbackPos);
  CHECK(semanticFactPos < collectionFallbackPos);
  CHECK(semanticFactPos < structFallbackPos);
  CHECK(internedTypeTextPos < collectionFallbackPos);
  CHECK(internedTypeTextPos < structFallbackPos);
  CHECK(suppressFallbackPos < collectionFallbackPos);
  CHECK(suppressFallbackPos < structFallbackPos);
  CHECK(source.find("std::string resolvedTypeText = typeText;", resolverPos) ==
        std::string::npos);
}

TEST_CASE("native Result ok emission uses semantic-product payload facts") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path packedResultHelpersPath =
      cwd / "src" / "ir_lowerer" / "IrLowererPackedResultHelpers.cpp";
  if (!std::filesystem::exists(packedResultHelpersPath)) {
    packedResultHelpersPath =
        cwd.parent_path() / "src" / "ir_lowerer" / "IrLowererPackedResultHelpers.cpp";
  }
  REQUIRE(std::filesystem::exists(packedResultHelpersPath));

  const std::string source = readTextFile(packedResultHelpersPath);
  const size_t emitPos = source.find("ResultOkMethodCallEmitResult tryEmitResultOkCall");
  REQUIRE(emitPos != std::string::npos);
  const size_t resolverPos =
      source.find("std::string resolveSemanticProductPayloadTypeText");
  const size_t idCheckPos =
      source.find("textId != InvalidSymbolId", resolverPos);
  const size_t internedTypeTextPos =
      source.find("semanticProgramResolveCallTargetString", idCheckPos);
  const size_t copiedTextFallbackPos =
      source.find("return trimTemplateTypeText(text);", internedTypeTextPos);
  const size_t semanticPayloadPos =
      source.find("resolveSemanticProductResultOkPayloadInfo", emitPos);
  const size_t rewriteFallbackPos =
      source.find("!hasSemanticPayloadInfo", emitPos);
  const size_t missingDiagnosticPos =
      source.find("missing semantic-product Result.ok payload metadata",
                  emitPos);
  const size_t inferKindPos =
      source.find("inferExprKind(payloadExpr, localsIn)", emitPos);
  const size_t collectionFallbackPos =
      source.find("inferDirectResultValueCollectionInfo", emitPos);
  const size_t structFallbackPos =
      source.find("inferPackedResultStructType", emitPos);
  REQUIRE(resolverPos != std::string::npos);
  REQUIRE(idCheckPos != std::string::npos);
  REQUIRE(internedTypeTextPos != std::string::npos);
  REQUIRE(copiedTextFallbackPos != std::string::npos);
  REQUIRE(semanticPayloadPos != std::string::npos);
  REQUIRE(rewriteFallbackPos != std::string::npos);
  REQUIRE(missingDiagnosticPos != std::string::npos);
  REQUIRE(inferKindPos != std::string::npos);
  REQUIRE(collectionFallbackPos != std::string::npos);
  REQUIRE(structFallbackPos != std::string::npos);
  CHECK(idCheckPos < internedTypeTextPos);
  CHECK(internedTypeTextPos < copiedTextFallbackPos);
  CHECK(semanticPayloadPos < rewriteFallbackPos);
  CHECK(rewriteFallbackPos < missingDiagnosticPos);
  CHECK(semanticPayloadPos < inferKindPos);
  CHECK(missingDiagnosticPos < inferKindPos);
  CHECK(semanticPayloadPos < collectionFallbackPos);
  CHECK(missingDiagnosticPos < collectionFallbackPos);
  CHECK(semanticPayloadPos < structFallbackPos);
  CHECK(missingDiagnosticPos < structFallbackPos);
  CHECK(source.find("std::string resolvedText = text;", resolverPos) ==
        std::string::npos);
}

TEST_CASE("native try Result lowering uses semantic-product variant metadata") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path tryHelpersPath =
      cwd / "src" / "ir_lowerer" / "IrLowererLowerEmitExprTryHelpers.cpp";
  if (!std::filesystem::exists(tryHelpersPath)) {
    tryHelpersPath =
        cwd.parent_path() / "src" / "ir_lowerer" / "IrLowererLowerEmitExprTryHelpers.cpp";
  }
  REQUIRE(std::filesystem::exists(tryHelpersPath));

  const std::string source = readTextFile(tryHelpersPath);
  CHECK(source.find("\"try candidate ok payload\"") != std::string::npos);
  CHECK(source.find("\"try candidate error payload\"") != std::string::npos);
  CHECK(source.find("\"try operand candidate ok payload\"") != std::string::npos);
  CHECK(source.find("\"try operand candidate error payload\"") != std::string::npos);
  CHECK(source.find("\"try source ok payload\"") != std::string::npos);
  CHECK(source.find("\"try source error payload\"") != std::string::npos);
  CHECK(source.find("\"try source error payload copy\"") != std::string::npos);
  CHECK(source.find("\"try target error payload copy\"") != std::string::npos);
  CHECK(source.find("\"try source ok\"") != std::string::npos);
  CHECK(source.find("\"try target error\"") != std::string::npos);
  CHECK(source.find("resolveSumPayloadStorageInfo(") == std::string::npos);
  CHECK(source.find("okVariant->variantIndex") == std::string::npos);
  CHECK(source.find("targetErrorVariant->variantIndex") == std::string::npos);
}

TEST_CASE("native try operand result metadata resolves interned query ids") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path tryHelpersPath =
      cwd / "src" / "ir_lowerer" / "IrLowererLowerEmitExprTryHelpers.cpp";
  if (!std::filesystem::exists(tryHelpersPath)) {
    tryHelpersPath =
        cwd.parent_path() / "src" / "ir_lowerer" / "IrLowererLowerEmitExprTryHelpers.cpp";
  }
  REQUIRE(std::filesystem::exists(tryHelpersPath));

  const std::string source = readTextFile(tryHelpersPath);
  const size_t resolverPos =
      source.find("auto resolveSemanticTryProductTypeText");
  const size_t semanticResolvePos =
      source.find("semanticProgramResolveCallTargetString(", resolverPos);
  const size_t applyPos = source.find("auto applySemanticOperandResultType",
                                      semanticResolvePos);
  const size_t errorTypePos =
      source.find("resolveSemanticTryProductTypeText(queryFact->resultErrorType,",
                  applyPos);
  const size_t errorIdPos =
      source.find("queryFact->resultErrorTypeId", errorTypePos);
  const size_t valueTypePos =
      source.find("resolveSemanticTryProductTypeText(queryFact->resultValueType,",
                  errorIdPos);
  const size_t valueIdPos =
      source.find("queryFact->resultValueTypeId", valueTypePos);
  const size_t fallbackPos =
      source.find("auto resolveLambdaReturnedValueExpr", valueIdPos);
  REQUIRE(resolverPos != std::string::npos);
  REQUIRE(semanticResolvePos != std::string::npos);
  REQUIRE(applyPos != std::string::npos);
  REQUIRE(errorTypePos != std::string::npos);
  REQUIRE(errorIdPos != std::string::npos);
  REQUIRE(valueTypePos != std::string::npos);
  REQUIRE(valueIdPos != std::string::npos);
  REQUIRE(fallbackPos != std::string::npos);
  CHECK(resolverPos < semanticResolvePos);
  CHECK(semanticResolvePos < applyPos);
  CHECK(applyPos < errorTypePos);
  CHECK(errorTypePos < errorIdPos);
  CHECK(errorIdPos < valueTypePos);
  CHECK(valueTypePos < valueIdPos);
  CHECK(valueIdPos < fallbackPos);
  CHECK(source.find("resultInfoOut.errorType = queryFact->resultErrorType;") ==
        std::string::npos);
  CHECK(source.find("return applySemanticTryValueType(queryFact->resultValueType,") ==
        std::string::npos);
  CHECK(source.find("auto resolveQueryResultTypeText") == std::string::npos);
}

TEST_CASE("native try fact result metadata resolves interned ids") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path tryHelpersPath =
      cwd / "src" / "ir_lowerer" / "IrLowererLowerEmitExprTryHelpers.cpp";
  if (!std::filesystem::exists(tryHelpersPath)) {
    tryHelpersPath =
        cwd.parent_path() / "src" / "ir_lowerer" / "IrLowererLowerEmitExprTryHelpers.cpp";
  }
  REQUIRE(std::filesystem::exists(tryHelpersPath));

  const std::string trySource = readTextFile(tryHelpersPath);
  const size_t resolverPos =
      trySource.find("auto resolveSemanticTryProductTypeText");
  const size_t semanticResolvePos =
      trySource.find("semanticProgramResolveCallTargetString(", resolverPos);
  const size_t errorTypePos =
      trySource.find("resolveSemanticTryProductTypeText(tryFact->errorType,",
                     semanticResolvePos);
  const size_t errorIdPos = trySource.find("tryFact->errorTypeId", errorTypePos);
  const size_t valueTypePos =
      trySource.find("resolveSemanticTryProductTypeText(tryFact->valueType,",
                     errorIdPos);
  const size_t valueIdPos = trySource.find("tryFact->valueTypeId", valueTypePos);
  REQUIRE(resolverPos != std::string::npos);
  REQUIRE(semanticResolvePos != std::string::npos);
  REQUIRE(errorTypePos != std::string::npos);
  REQUIRE(errorIdPos != std::string::npos);
  REQUIRE(valueTypePos != std::string::npos);
  REQUIRE(valueIdPos != std::string::npos);
  CHECK(resolverPos < semanticResolvePos);
  CHECK(semanticResolvePos < errorTypePos);
  CHECK(errorTypePos < errorIdPos);
  CHECK(errorIdPos < valueTypePos);
  CHECK(valueTypePos < valueIdPos);
  CHECK(trySource.find("resultInfo.errorType = tryFact->errorType;") ==
        std::string::npos);
  CHECK(trySource.find("applySemanticTryValueType(tryFact->valueType") ==
        std::string::npos);
  CHECK(trySource.find("auto resolveTryFactTypeText") == std::string::npos);

  std::filesystem::path inferencePath =
      cwd / "src" / "ir_lowerer" / "IrLowererLowerInferenceDispatchSetup.cpp";
  if (!std::filesystem::exists(inferencePath)) {
    inferencePath =
        cwd.parent_path() / "src" / "ir_lowerer" / "IrLowererLowerInferenceDispatchSetup.cpp";
  }
  REQUIRE(std::filesystem::exists(inferencePath));

  const std::string inferenceSource = readTextFile(inferencePath);
  const size_t valueResolverPos = inferenceSource.find("auto resolveTryFactValueTypeText");
  const size_t valueSemanticResolvePos =
      inferenceSource.find("semanticProgramResolveCallTargetString(", valueResolverPos);
  const size_t valueKindPos =
      inferenceSource.find("valueKindFromTypeName(valueTypeText)", valueSemanticResolvePos);
  REQUIRE(valueResolverPos != std::string::npos);
  REQUIRE(valueSemanticResolvePos != std::string::npos);
  REQUIRE(valueKindPos != std::string::npos);
  CHECK(valueResolverPos < valueSemanticResolvePos);
  CHECK(valueSemanticResolvePos < valueKindPos);
  CHECK(inferenceSource.find("valueKindFromTypeName(tryFact->valueType)") ==
        std::string::npos);
}

TEST_CASE("for-condition auto bindings use semantic-product binding facts") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path loopsPath =
      cwd / "src" / "ir_lowerer" / "IrLowererLowerStatementsLoops.h";
  if (!std::filesystem::exists(loopsPath)) {
    loopsPath =
        cwd.parent_path() / "src" / "ir_lowerer" / "IrLowererLowerStatementsLoops.h";
  }
  REQUIRE(std::filesystem::exists(loopsPath));

  const std::string source = readTextFile(loopsPath);
  CHECK(source.find("isSemanticForConditionBindingCandidate") != std::string::npos);
  CHECK(source.find("findSemanticProductBindingFact(") != std::string::npos);
  CHECK(source.find("bindingFact->bindingTypeTextId") != std::string::npos);
  CHECK(source.find("semanticProgramResolveCallTargetString(") != std::string::npos);
  CHECK(source.find("missing semantic-product for-condition binding fact") != std::string::npos);
  CHECK(source.find("semanticForConditionBindingExpr.semanticNodeId = 0;") != std::string::npos);
  CHECK(source.find("*conditionBindingForDeclaration") != std::string::npos);
  CHECK(source.find(
            "bindingFact != nullptr ? trimTemplateTypeText(bindingFact->bindingTypeText)") ==
        std::string::npos);
  CHECK(source.find("declareForConditionBinding(\n                cond,") == std::string::npos);
}

TEST_CASE("local-auto statement bindings resolve interned binding ids") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path bindingsPath =
      cwd / "src" / "ir_lowerer" / "IrLowererLowerStatementsBindings.h";
  if (!std::filesystem::exists(bindingsPath)) {
    bindingsPath =
        cwd.parent_path() / "src" / "ir_lowerer" / "IrLowererLowerStatementsBindings.h";
  }
  REQUIRE(std::filesystem::exists(bindingsPath));

  const std::string source = readTextFile(bindingsPath);
  const size_t candidatePos = source.find("isLocalAutoBindingCandidate(stmt)");
  REQUIRE(candidatePos != std::string::npos);
  const size_t factPos =
      source.find("findSemanticProductLocalAutoFactBySemanticId", candidatePos);
  const size_t idPos = source.find("localAutoFact->bindingTypeTextId", factPos);
  const size_t resolverPos =
      source.find("semanticProgramResolveCallTargetString(", idPos);
  const size_t fallbackTextPos =
      source.find("bindingTypeText = localAutoFact->bindingTypeText", resolverPos);
  const size_t transformPos =
      source.find("semanticLocalAutoBindingExpr.transforms.push_back", fallbackTextPos);
  REQUIRE(factPos != std::string::npos);
  REQUIRE(idPos != std::string::npos);
  REQUIRE(resolverPos != std::string::npos);
  REQUIRE(fallbackTextPos != std::string::npos);
  REQUIRE(transformPos != std::string::npos);
  CHECK(factPos < idPos);
  CHECK(idPos < resolverPos);
  CHECK(resolverPos < fallbackTextPos);
  CHECK(fallbackTextPos < transformPos);
  CHECK(source.find(
            "localAutoFact != nullptr ? trimTemplateTypeText(localAutoFact->bindingTypeText)") ==
        std::string::npos);
}

TEST_CASE("statement binding local info resolves interned binding ids") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path localInfoPath =
      cwd / "src" / "ir_lowerer" / "IrLowererLowerStatementsBindingLocalInfo.h";
  if (!std::filesystem::exists(localInfoPath)) {
    localInfoPath =
        cwd.parent_path() / "src" / "ir_lowerer" /
        "IrLowererLowerStatementsBindingLocalInfo.h";
  }
  REQUIRE(std::filesystem::exists(localInfoPath));

  const std::string source = readTextFile(localInfoPath);
  const size_t fallbackPos =
      source.find("info.kind == LocalInfo::Kind::Value && info.structTypeName.empty()");
  REQUIRE(fallbackPos != std::string::npos);
  const size_t factPos = source.find("findSemanticProductBindingFact(", fallbackPos);
  const size_t idPos = source.find("bindingFact->bindingTypeTextId", factPos);
  const size_t resolverPos =
      source.find("semanticProgramResolveCallTargetString(", idPos);
  const size_t fallbackTextPos =
      source.find("semanticTypeText = bindingFact->bindingTypeText", resolverPos);
  const size_t valueKindPos = source.find("valueKindFromTypeName(semanticTypeText)", fallbackTextPos);
  REQUIRE(factPos != std::string::npos);
  REQUIRE(idPos != std::string::npos);
  REQUIRE(resolverPos != std::string::npos);
  REQUIRE(fallbackTextPos != std::string::npos);
  REQUIRE(valueKindPos != std::string::npos);
  CHECK(factPos < idPos);
  CHECK(idPos < resolverPos);
  CHECK(resolverPos < fallbackTextPos);
  CHECK(fallbackTextPos < valueKindPos);
  CHECK(source.find("bindingFact != nullptr && !bindingFact->bindingTypeText.empty()") ==
        std::string::npos);
}


TEST_SUITE_END();
