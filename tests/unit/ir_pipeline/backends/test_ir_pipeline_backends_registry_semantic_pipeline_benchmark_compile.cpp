#include "third_party/doctest.h"

#include "test_ir_pipeline_backends_registry_shared.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.backends.registry");

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
  CHECK(conformance.resolvedDirectCallPath(*directCall).rfind("/id", 0) == 0);
  const auto *methodCall = conformance.findMethodCallTarget("/main", "count");
  REQUIRE(methodCall != nullptr);
  CHECK(conformance.resolvedMethodCallPath(*methodCall) == "/std/collections/vector/count");
  CHECK(conformance.emitResult.exitCode == 2);
}

TEST_CASE("semantic-product fact family ownership is explicit") {
  CHECK(primec::SemanticProductContractVersionCurrent ==
        primec::SemanticProductContractVersionV3);
  CHECK(primec::semanticProgramFactFamilyIsSemanticProductOwned("directCallTargets"));
  CHECK(primec::semanticProgramFactFamilyIsSemanticProductOwned("bindingFacts"));
  CHECK(primec::semanticProgramFactFamilyIsSemanticProductOwned("arrayExtentFacts"));
  CHECK(primec::semanticProgramFactFamilyIsSemanticProductOwned("onErrorFacts"));
  CHECK(primec::semanticProgramFactFamilyIsAstProvenanceOwned("definitions"));
  CHECK(primec::semanticProgramFactFamilyIsAstProvenanceOwned("executions"));
  CHECK(primec::semanticProgramFactFamilyOwnership("publishedRoutingLookups") ==
        primec::SemanticProgramFactOwnership::DerivedIndex);
  CHECK_FALSE(primec::semanticProgramFactFamilyOwnership("unknownFacts").has_value());
}

TEST_CASE("vm backend conformance keeps semantic-product contract v3") {
  const std::string source =
      "[return<i32>]\n"
      "main() {\n"
      "  return(0i32)\n"
      "}\n";

  primec::testing::CompilePipelineBackendConformance conformance;
  std::string error;
  REQUIRE(primec::testing::runCompilePipelineBackendConformanceForTesting(
      source, "/main", "vm", conformance, error));
  CHECK(error.empty());
  REQUIRE(conformance.output.hasSemanticProgram);
  CHECK(conformance.output.semanticProgram.contractVersion ==
        primec::SemanticProductContractVersionCurrent);
  CHECK(conformance.emitResult.exitCode == 0);
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

#if !defined(__APPLE__) || (!defined(__arm64__) && !defined(__aarch64__))
  INFO("SKIP: native backend emit is only supported on macOS arm64");
  return;
#endif
  primec::testing::CompilePipelineBackendConformance conformance;
  std::string error;
  REQUIRE(primec::testing::runCompilePipelineBackendConformanceForTesting(
      source, "/main", "native", conformance, error));
  CHECK(error.empty());
  CHECK(conformance.output.hasSemanticProgram);
  CHECK(conformance.backendKind == "native");
  const auto *directCall = conformance.findDirectCallTarget("/main", "id");
  REQUIRE(directCall != nullptr);
  CHECK(conformance.resolvedDirectCallPath(*directCall).rfind("/id", 0) == 0);
  const auto *methodCall = conformance.findMethodCallTarget("/main", "count");
  REQUIRE(methodCall != nullptr);
  CHECK(conformance.resolvedMethodCallPath(*methodCall) == "/std/collections/vector/count");
  CHECK(conformance.emitResult.exitCode == 0);
  CHECK(std::filesystem::exists(conformance.outputPath));
  CHECK(std::filesystem::is_regular_file(conformance.outputPath));
  CHECK(std::filesystem::file_size(conformance.outputPath) > 0);
}

TEST_CASE("backend conformance keeps semantic-product-owned facts aligned across backends") {
  const std::string source = R"(
import /std/collections/*

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
#if defined(__APPLE__) && (defined(__arm64__) || defined(__aarch64__))
  const auto nativeConformance = runConformance("native");
#else
  const auto &nativeConformance = cppConformance;
#endif

  const auto *cppDirect = cppConformance.findDirectCallTarget("/main", "id");
  const auto *vmDirect = vmConformance.findDirectCallTarget("/main", "id");
  const auto *nativeDirect = nativeConformance.findDirectCallTarget("/main", "id");
  REQUIRE(cppDirect != nullptr);
  REQUIRE(vmDirect != nullptr);
  REQUIRE(nativeDirect != nullptr);
  const std::string_view cppDirectPath = cppConformance.resolvedDirectCallPath(*cppDirect);
  const std::string_view vmDirectPath = vmConformance.resolvedDirectCallPath(*vmDirect);
  const std::string_view nativeDirectPath = nativeConformance.resolvedDirectCallPath(*nativeDirect);
  CHECK(cppDirectPath.rfind("/id", 0) == 0);
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
  CHECK(cppMethodPath == "/std/collections/vector/count");
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
    CHECK(cppBridgePath.rfind("/std/collections/vector/count", 0) == 0);
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

  const auto *cppVector =
      cppConformance.findCollectionSpecialization("/main", "values");
  const auto *vmVector =
      vmConformance.findCollectionSpecialization("/main", "values");
  const auto *nativeVector =
      nativeConformance.findCollectionSpecialization("/main", "values");
  REQUIRE(cppVector != nullptr);
  REQUIRE(vmVector != nullptr);
  REQUIRE(nativeVector != nullptr);
  CHECK(cppVector->collectionFamily == "vector");
  CHECK(cppVector->elementTypeText == "i32");
  CHECK(vmVector->collectionFamily == cppVector->collectionFamily);
  CHECK(nativeVector->collectionFamily == cppVector->collectionFamily);
  CHECK(vmVector->elementTypeText == cppVector->elementTypeText);
  CHECK(nativeVector->elementTypeText == cppVector->elementTypeText);

  const auto *cppQuery = findSemanticEntry(primec::semanticProgramQueryFactView(cppConformance.output.semanticProgram),
      [&cppConformance](const primec::SemanticProgramQueryFact &entry) {
        const std::string_view scopePath =
            entry.scopePathId != primec::InvalidSymbolId
                ? primec::semanticProgramResolveCallTargetString(
                      cppConformance.output.semanticProgram, entry.scopePathId)
                : std::string_view(entry.scopePath);
        return scopePath == "/main" &&
               primec::semanticProgramQueryFactResolvedPath(cppConformance.output.semanticProgram, entry) ==
                   "/lookup";
      });
  const auto *vmQuery = findSemanticEntry(primec::semanticProgramQueryFactView(vmConformance.output.semanticProgram),
      [&vmConformance](const primec::SemanticProgramQueryFact &entry) {
        const std::string_view scopePath =
            entry.scopePathId != primec::InvalidSymbolId
                ? primec::semanticProgramResolveCallTargetString(
                      vmConformance.output.semanticProgram, entry.scopePathId)
                : std::string_view(entry.scopePath);
        return scopePath == "/main" &&
               primec::semanticProgramQueryFactResolvedPath(vmConformance.output.semanticProgram, entry) ==
                   "/lookup";
      });
  const auto *nativeQuery = findSemanticEntry(primec::semanticProgramQueryFactView(nativeConformance.output.semanticProgram),
      [&nativeConformance](const primec::SemanticProgramQueryFact &entry) {
        const std::string_view scopePath =
            entry.scopePathId != primec::InvalidSymbolId
                ? primec::semanticProgramResolveCallTargetString(
                      nativeConformance.output.semanticProgram, entry.scopePathId)
                : std::string_view(entry.scopePath);
        return scopePath == "/main" &&
               primec::semanticProgramQueryFactResolvedPath(nativeConformance.output.semanticProgram, entry) ==
                   "/lookup";
      });
  REQUIRE(cppQuery != nullptr);
  REQUIRE(vmQuery != nullptr);
  REQUIRE(nativeQuery != nullptr);
  CHECK(cppQuery->bindingTypeText == "Result<int, i32>");
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
  CHECK(cppConformance.emitResult.exitCode == 0);
  CHECK(vmConformance.emitResult.exitCode == 7);

  const std::string cpp = readTextFile(cppConformance.outputPath);
  CHECK(cpp.find("static int64_t ps_fn_0") != std::string::npos);
#if defined(__APPLE__) && (defined(__arm64__) || defined(__aarch64__))
  CHECK(nativeConformance.backendKind == "native");
  CHECK(nativeConformance.emitResult.exitCode == 0);
  CHECK(std::filesystem::exists(nativeConformance.outputPath));
  CHECK(std::filesystem::is_regular_file(nativeConformance.outputPath));
  CHECK(std::filesystem::file_size(nativeConformance.outputPath) > 0);
#endif
}

TEST_CASE("backend conformance keeps auto-bound Result combinator facts aligned across backends") {
  const std::string source = R"(
import /std/file/*

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(err.why())
}

[return<int> effects(io_out, io_err) on_error<FileError, /log_file_error>]
main() {
  [auto] mapped{ Result.map(Result.ok(2i32), []([i32] value) { return(multiply(value, 4i32)) }) }
  [auto] chained{ Result.and_then(Result.ok(2i32), []([i32] value) { return(Result.ok(plus(value, 3i32))) }) }
  [auto] summed{
    Result.map2(Result.ok(2i32), Result.ok(3i32), []([i32] left, [i32] right) { return(plus(left, right)) })
  }
  [i32] first{try(mapped)}
  [i32] second{try(chained)}
  [i32] third{try(summed)}
  return(first + second + third)
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
#if defined(__APPLE__) && (defined(__arm64__) || defined(__aarch64__))
  const auto nativeConformance = runConformance("native");
#else
  const auto &nativeConformance = cppConformance;
#endif

  auto requireLocalAutoFact = [](const primec::SemanticProgram &semanticProgram,
                                 std::string_view bindingName) {
    const auto *localAuto = findSemanticEntry(
        primec::semanticProgramLocalAutoFactView(semanticProgram),
        [bindingName](const primec::SemanticProgramLocalAutoFact &entry) {
          return entry.scopePath == "/main" && entry.bindingName == bindingName;
        });
    REQUIRE(localAuto != nullptr);
    return localAuto;
  };

  auto checkLocalAutoFact = [&](std::string_view bindingName) {
    const auto *cppLocalAuto = requireLocalAutoFact(cppConformance.output.semanticProgram, bindingName);
    const auto *vmLocalAuto = requireLocalAutoFact(vmConformance.output.semanticProgram, bindingName);
    const auto *nativeLocalAuto = requireLocalAutoFact(nativeConformance.output.semanticProgram, bindingName);

    CHECK(cppLocalAuto->bindingTypeText == "Result<i32, FileError>");
    CHECK(vmLocalAuto->bindingTypeText == cppLocalAuto->bindingTypeText);
    CHECK(nativeLocalAuto->bindingTypeText == cppLocalAuto->bindingTypeText);

    const std::string_view cppResolved =
        primec::semanticProgramLocalAutoFactInitializerResolvedPath(
            cppConformance.output.semanticProgram, *cppLocalAuto);
    const std::string_view vmResolved =
        primec::semanticProgramLocalAutoFactInitializerResolvedPath(
            vmConformance.output.semanticProgram, *vmLocalAuto);
    const std::string_view nativeResolved =
        primec::semanticProgramLocalAutoFactInitializerResolvedPath(
            nativeConformance.output.semanticProgram, *nativeLocalAuto);
    CHECK(!cppResolved.empty());
    CHECK(vmResolved == cppResolved);
    CHECK(nativeResolved == cppResolved);
  };

  checkLocalAutoFact("mapped");
  checkLocalAutoFact("chained");
  checkLocalAutoFact("summed");

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

  primec::CompilePipelineDiagnosticInfo diagnosticInfo;
  primec::CompilePipelineErrorStage errorStage = primec::CompilePipelineErrorStage::None;
  std::string error;
  primec::CompilePipelineResult result =
      primec::runCompilePipelineResult(options, errorStage, error, &diagnosticInfo);

  std::error_code ec;
  std::filesystem::remove(tempPath, ec);

  const auto *failure = std::get_if<primec::CompilePipelineFailureResult>(&result);
  REQUIRE(failure != nullptr);
  CHECK(errorStage == primec::CompilePipelineErrorStage::Semantic);
  CHECK(failure->failure.stage == primec::CompilePipelineErrorStage::Semantic);
  CHECK(error == "graphics stdlib runtime substrate unavailable for glsl target: /std/gfx/experimental/*");
  CHECK(failure->failure.message == error);
  REQUIRE_FALSE(failure->failure.diagnosticInfo.records.empty());
  CHECK(failure->failure.diagnosticInfo.records.front().message == error);
  // TODO-5229 (docs/todo.md): this source imports /std/gfx/experimental/*
  // but never uses any symbol from it. Under default whole-file stdlib
  // splicing, the entire module gets included regardless of usage, so
  // semantic validation always succeeds first and the semantic product
  // below is already built by the time the graphics-backend-mismatch
  // check (this test's actual subject) fails. Under
  // PRIMESTRUCT_FORCE_LAZY_STDLIB_IMPORTS=1 (the differential harness that
  // doubles the whole test corpus as a lazy-vs-default comparison, per
  // TODO-5229), lazy stdlib import expansion correctly splices nothing for
  // an unused import, so semantic validation genuinely fails on its own
  // before ever reaching the graphics check - there is no real semantic
  // product to preserve in that case, only the graphics diagnostic itself
  // (still asserted above, and still correctly prioritized ahead of the
  // "unknown import path" failure it would otherwise be masked by - see
  // TODO-5229's progress notes). This is a legitimate, understood
  // divergence between the two paths for this specific never-used-import
  // shape, not a product-preservation bug, so it's pinned here rather than
  // chased further.
  const bool lazyStdlibImportsForced =
      std::getenv("PRIMESTRUCT_FORCE_LAZY_STDLIB_IMPORTS") != nullptr;
  if (!lazyStdlibImportsForced) {
    REQUIRE(failure->hasSemanticProgram);
    CHECK(failure->semanticProgram.entryPath == "/main");
    CHECK(std::find(failure->semanticProgram.imports.begin(),
                    failure->semanticProgram.imports.end(),
                    "/std/gfx/experimental/*") != failure->semanticProgram.imports.end());
  }
  CHECK(diagnosticInfo.message == failure->failure.diagnosticInfo.message);
  REQUIRE_FALSE(diagnosticInfo.records.empty());
  CHECK(diagnosticInfo.records.front().message == error);
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

TEST_CASE("compile pipeline semantic-product generation stays limited to semantic-product dumps and consuming emit paths") {
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

  struct Scenario {
    std::string_view name;
    std::string_view emitKind;
    std::string_view dumpStage;
    bool expectDumpOutput;
    bool expectSemanticProductRequested;
    bool expectSemanticProductBuilt;
    bool expectSemanticProgram;
  };

  const std::vector<Scenario> scenarios = {
      {"pre-ast dump", "native", "pre_ast", true, false, false, false},
      {"ast dump", "native", "ast", true, false, false, false},
      {"ir dump", "native", "ir", true, false, false, false},
      {"type-graph dump", "native", "type-graph", true, false, false, false},
      {"ast-semantic dump", "native", "ast-semantic", true, false, false, false},
      {"semantic-product dump", "native", "semantic-product", true, true, true, true},
      {"native emit", "native", "", false, true, true, true},
      {"vm emit", "vm", "", false, true, true, true},
  };

  for (const Scenario &scenario : scenarios) {
    primec::Options options;
    options.inputPath = tempPath.string();
    options.entryPath = "/bench/main";
    options.emitKind = std::string(scenario.emitKind);
    options.dumpStage = std::string(scenario.dumpStage);
    options.collectDiagnostics = true;
    primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);

    primec::CompilePipelineOutput output;
    primec::CompilePipelineDiagnosticInfo diagnosticInfo;
    primec::CompilePipelineErrorStage errorStage = primec::CompilePipelineErrorStage::None;
    std::string error;
    const bool ok = primec::runCompilePipeline(options, output, errorStage, error, &diagnosticInfo);

    INFO(scenario.name);
    INFO(error);
    REQUIRE(ok);
    CHECK(error.empty());
    CHECK(errorStage == primec::CompilePipelineErrorStage::None);
    CHECK_FALSE(output.hasFailure);
    CHECK(output.hasDumpOutput == scenario.expectDumpOutput);
    CHECK(output.semanticProductRequested == scenario.expectSemanticProductRequested);
    CHECK(output.semanticProductBuilt == scenario.expectSemanticProductBuilt);
    CHECK(output.hasSemanticProgram == scenario.expectSemanticProgram);
    if (scenario.expectSemanticProgram) {
      CHECK(output.semanticProgram.entryPath == "/bench/main");
    }
  }

  std::error_code ec;
  std::filesystem::remove(tempPath, ec);
}

TEST_CASE("compile pipeline pre-ast dump returns before parser failures") {
  const std::filesystem::path tempPath = makeTempIrPipelineSourcePath();
  {
    std::ofstream file(tempPath);
    REQUIRE(file.good());
    file << R"(namespace bench {
[return<i32>]
main( {
  return(0i32)
}
}
)";
  }

  primec::Options options;
  options.inputPath = tempPath.string();
  options.entryPath = "/bench/main";
  options.emitKind = "native";
  options.dumpStage = "pre_ast";
  options.collectDiagnostics = true;
  primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);

  primec::CompilePipelineOutput output;
  primec::CompilePipelineDiagnosticInfo diagnosticInfo;
  primec::CompilePipelineErrorStage errorStage = primec::CompilePipelineErrorStage::None;
  std::string error;
  const bool ok = primec::runCompilePipeline(options, output, errorStage, error, &diagnosticInfo);

  std::error_code ec;
  std::filesystem::remove(tempPath, ec);

  INFO(error);
  REQUIRE(ok);
  CHECK(error.empty());
  CHECK(errorStage == primec::CompilePipelineErrorStage::None);
  CHECK_FALSE(output.hasFailure);
  REQUIRE(output.hasDumpOutput);
  CHECK(output.dumpOutput == output.filteredSource);
  CHECK(output.dumpOutput.find("main( {") != std::string::npos);
  CHECK(output.program.definitions.empty());
  CHECK(output.program.executions.empty());
  CHECK_FALSE(output.hasSemanticProgram);
  CHECK(diagnosticInfo.records.empty());
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

TEST_SUITE_END();
