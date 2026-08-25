

TEST_CASE("semantic product routing fact v1 shape markers match public fields") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path directCallFactsPath =
      cwd / "include" / "primec" / "semantic_product" / "DirectCallFacts.h";
  std::filesystem::path methodCallFactsPath =
      cwd / "include" / "primec" / "semantic_product" / "MethodCallFacts.h";
  if (!std::filesystem::exists(directCallFactsPath)) {
    directCallFactsPath =
        cwd.parent_path() / "include" / "primec" / "semantic_product" / "DirectCallFacts.h";
    methodCallFactsPath =
        cwd.parent_path() / "include" / "primec" / "semantic_product" / "MethodCallFacts.h";
  }
  REQUIRE(std::filesystem::exists(directCallFactsPath));
  REQUIRE(std::filesystem::exists(methodCallFactsPath));

  const auto trim = [](std::string value) {
    const std::size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
      return std::string{};
    }
    const std::size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
  };
  const auto extractStructShape = [&](const std::string &source, std::string_view structName) {
    const std::string structNeedle = "struct " + std::string(structName) + " {";
    const std::size_t structStart = source.find(structNeedle);
    if (structStart == std::string::npos) {
      return "missing struct " + std::string(structName);
    }
    const std::size_t bodyStart = source.find('\n', structStart);
    const std::size_t bodyEnd = source.find("\n};", bodyStart);
    if (bodyStart == std::string::npos || bodyEnd == std::string::npos) {
      return "missing struct body " + std::string(structName);
    }

    std::string shape;
    std::size_t lineStart = bodyStart + 1;
    while (lineStart < bodyEnd) {
      const std::size_t lineEnd = source.find('\n', lineStart);
      const std::size_t clampedLineEnd =
          lineEnd == std::string::npos || lineEnd > bodyEnd ? bodyEnd : lineEnd;
      const std::string line =
          trim(source.substr(lineStart, clampedLineEnd - lineStart));
      if (!line.empty()) {
        if (!shape.empty()) {
          shape += "|";
        }
        shape += line;
      }
      if (lineEnd == std::string::npos || lineEnd >= bodyEnd) {
        break;
      }
      lineStart = lineEnd + 1;
    }
    return shape;
  };
  const auto extractStringViewMarker = [&](const std::string &source, std::string_view markerName) {
    const std::size_t markerStart = source.find(markerName);
    if (markerStart == std::string::npos) {
      return "missing marker " + std::string(markerName);
    }
    const std::size_t initStart = source.find('=', markerStart);
    if (initStart == std::string::npos) {
      return "missing marker initializer " + std::string(markerName);
    }
    std::size_t initEnd = std::string::npos;
    bool inString = false;
    for (std::size_t i = initStart + 1; i < source.size(); ++i) {
      if (source[i] == '"') {
        inString = !inString;
      } else if (source[i] == ';' && !inString) {
        initEnd = i;
        break;
      }
    }
    if (initEnd == std::string::npos) {
      return "missing marker terminator " + std::string(markerName);
    }

    std::string marker;
    std::size_t searchStart = initStart;
    while (searchStart < initEnd) {
      const std::size_t quoteStart = source.find('"', searchStart);
      if (quoteStart == std::string::npos || quoteStart >= initEnd) {
        break;
      }
      const std::size_t quoteEnd = source.find('"', quoteStart + 1);
      if (quoteEnd == std::string::npos || quoteEnd > initEnd) {
        return "unterminated marker string " + std::string(markerName);
      }
      marker += source.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
      searchStart = quoteEnd + 1;
    }
    return marker;
  };

  const std::string directCallFacts = readTextFile(directCallFactsPath);
  const std::string methodCallFacts = readTextFile(methodCallFactsPath);
  CHECK(extractStructShape(directCallFacts, "SemanticProgramDirectCallTarget") ==
        extractStringViewMarker(directCallFacts, "SemanticProgramDirectCallTargetContractV1Shape"));
  CHECK(extractStructShape(methodCallFacts, "SemanticProgramMethodCallTarget") ==
        extractStringViewMarker(methodCallFacts, "SemanticProgramMethodCallTargetContractV1Shape"));
}

TEST_CASE("semantic-product consumer coverage matrix stays source locked") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  const std::filesystem::path root =
      std::filesystem::exists(cwd / "include" / "primec" / "frontend" / "SemanticProduct.h")
          ? cwd
          : cwd.parent_path();
  auto readRepoFile = [&](const std::filesystem::path &relativePath) {
    const std::filesystem::path fullPath = root / relativePath;
    REQUIRE(std::filesystem::exists(fullPath));
    return readTextFile(fullPath);
  };

  const std::string semanticProductSource = readRepoFile("src/frontend/SemanticProduct.cpp");
  const std::string matrix = readRepoFile("docs/SemanticProductConsumerMatrix.md");
  const std::string registryTests =
      readRepoFile("tests/unit/ir_pipeline/backends/test_ir_pipeline_backends_registry_native_semantic_product_result.cpp") +
      readRepoFile("tests/unit/ir_pipeline/backends/test_ir_pipeline_backends_registry_semantic_lowerer_product_rejects.cpp") +
      readRepoFile("tests/unit/ir_pipeline/backends/test_ir_pipeline_backends_registry_lowerer_semantic_product_rejects.cpp") +
      readRepoFile("tests/unit/ir_pipeline/backends/test_ir_pipeline_backends_registry_semantic_pipeline_benchmark_compile.cpp");
  const std::string snapshotTests =
      readRepoFile("tests/unit/semantics/type_resolution/test_semantics_type_resolution_graph_snapshots_require_predicates_facts_ct_if.cpp") +
      readRepoFile("tests/unit/semantics/type_resolution/test_semantics_type_resolution_graph_snapshots_targets_semantic_product_soa.cpp") +
      readRepoFile("tests/unit/semantics/type_resolution/test_semantics_type_resolution_graph_snapshots_semantic_product_publishes_ids.cpp") +
      readRepoFile("tests/unit/semantics/type_resolution/test_semantics_type_resolution_graph_snapshots_formatter_module_fact_semantic.cpp");
  const std::string entrySetupTests = readRepoFile(
      "tests/unit/ir_pipeline/validation/test_ir_pipeline_validation_ir_lowerer_entry_setup_step_resolves_entry_metadata.cpp");
  const std::string callHelperTests = readRepoFile(
      "tests/unit/ir_pipeline/validation/test_ir_pipeline_validation_ir_lowerer_call_helpers_source_delegation_stays_stable.cpp");
  const std::string compileTimeFacadeTests =
      readRepoFile("tests/unit/compile_time/test_compile_time_evaluation_facade.cpp");

  const std::size_t familiesStart =
      semanticProductSource.find("static const std::vector<SemanticProgramFactFamilyInfo> Families = {");
  REQUIRE(familiesStart != std::string::npos);
  const std::size_t familiesEnd =
      semanticProductSource.find("  };\n  return Families;", familiesStart);
  REQUIRE(familiesEnd != std::string::npos);
  const std::string familiesBlock =
      semanticProductSource.substr(familiesStart, familiesEnd - familiesStart);

  std::vector<std::string> factFamilies;
  std::size_t searchStart = 0;
  while (true) {
    const std::size_t entryStart = familiesBlock.find("{\"", searchStart);
    if (entryStart == std::string::npos) {
      break;
    }
    const std::size_t nameStart = entryStart + 2;
    const std::size_t nameEnd = familiesBlock.find('"', nameStart);
    REQUIRE(nameEnd != std::string::npos);
    factFamilies.push_back(familiesBlock.substr(nameStart, nameEnd - nameStart));
    searchStart = nameEnd + 1;
  }
  REQUIRE(factFamilies.size() >= 20);
  for (const std::string &factFamily : factFamilies) {
    CHECK(matrix.find("| `" + factFamily + "` |") != std::string::npos);
  }

  CHECK(matrix.find("Every row below is source-locked against `semanticProgramFactFamilyInfos()`") !=
        std::string::npos);
  CHECK(matrix.find("SPCM-FOLLOWUP-struct-fields") != std::string::npos);
  CHECK(matrix.find("SPCM-FOLLOWUP-requirements") == std::string::npos);

  auto checkCoverage = [&](const std::string &factFamily,
                           const std::string &positiveTest,
                           const std::string &positiveSource,
                           const std::string &staleOrMissingTest,
                           const std::string &staleOrMissingSource) {
    const std::string rowPrefix = "| `" + factFamily + "` |";
    const std::size_t rowStart = matrix.find(rowPrefix);
    REQUIRE(rowStart != std::string::npos);
    const std::size_t rowEnd = matrix.find('\n', rowStart);
    REQUIRE(rowEnd != std::string::npos);
    const std::string row = matrix.substr(rowStart, rowEnd - rowStart);
    CHECK(row.find("positive: `" + positiveTest + "`") != std::string::npos);
    CHECK(row.find("stale/missing: `" + staleOrMissingTest + "`") !=
          std::string::npos);
    CHECK(positiveSource.find("TEST_CASE(\"" + positiveTest + "\")") !=
          std::string::npos);
    CHECK(staleOrMissingSource.find("TEST_CASE(\"" + staleOrMissingTest + "\")") !=
          std::string::npos);
  };

  checkCoverage(
      "directCallTargets",
      "ir lowerer keeps semantic-product direct-call targets authoritative over rooted rewritten expr names",
      registryTests,
      "ir lowerer rejects stale semantic-product direct-call metadata",
      registryTests);
  checkCoverage("bindingFacts",
                "for-condition auto bindings use semantic-product binding facts",
                registryTests,
                "ir lowerer rejects missing semantic-product binding facts",
                registryTests);
  checkCoverage("arrayExtentFacts",
                "semantic product publishes array extent facts",
                snapshotTests,
                "ir lowerer rejects missing semantic-product array extent facts",
                registryTests);
  checkCoverage("queryFacts",
                "native Result combinator sources use semantic-product query facts",
                registryTests,
                "ir lowerer rejects stale semantic-product query facts",
                registryTests);
  checkCoverage(
      "tryFacts",
      "ir lowerer semantic-product adapter uses try semantic-id matches without path fallback",
      callHelperTests,
      "ir lowerer rejects stale semantic-product try result metadata",
      registryTests);

  const std::string preflightRowPrefix = "| `publishedLowererPreflightFacts` |";
  const std::size_t preflightRowStart = matrix.find(preflightRowPrefix);
  REQUIRE(preflightRowStart != std::string::npos);
  const std::size_t preflightRowEnd = matrix.find('\n', preflightRowStart);
  REQUIRE(preflightRowEnd != std::string::npos);
  const std::string preflightRow =
      matrix.substr(preflightRowStart, preflightRowEnd - preflightRowStart);
  CHECK(preflightRow.find("stale/missing: `ir lowerer rejects missing semantic-product "
                          "lowerer preflight software numeric ids`") != std::string::npos);
  CHECK(preflightRow.find("stale/missing: `ir lowerer rejects stale semantic-product "
                          "lowerer preflight software numeric ids`") != std::string::npos);
  CHECK(preflightRow.find("stale/missing: `ir preparation rejects missing semantic-product "
                          "lowerer preflight runtime reflection ids`") != std::string::npos);
  CHECK(preflightRow.find("stale/missing: `ir preparation rejects stale semantic-product "
                          "lowerer preflight runtime reflection ids`") != std::string::npos);
  CHECK(entrySetupTests.find("TEST_CASE(\"ir lowerer rejects missing semantic-product "
                             "lowerer preflight software numeric ids\")") !=
        std::string::npos);
  CHECK(entrySetupTests.find("TEST_CASE(\"ir lowerer rejects stale semantic-product "
                             "lowerer preflight software numeric ids\")") !=
        std::string::npos);
  CHECK(registryTests.find("TEST_CASE(\"ir preparation rejects missing semantic-product "
                           "lowerer preflight runtime reflection ids\")") !=
        std::string::npos);
  CHECK(registryTests.find("TEST_CASE(\"ir preparation rejects stale semantic-product "
                           "lowerer preflight runtime reflection ids\")") !=
        std::string::npos);

  checkCoverage(
      "requirementPredicateFacts",
      "compile-time evaluation facade wraps published requirement facts",
      compileTimeFacadeTests,
      "compile-time evaluation rejects stale or missing requirementPredicateFacts",
      compileTimeFacadeTests);
}

TEST_CASE("semantic product query and try projections expose stable public lookup keys") {
  const std::string source = R"(
[return<void>]
unexpected_error([i32] err) {
}

[return<Result<int, i32>>]
lookup_alpha() {
  return(Result.ok(4i32))
}

[return<Result<int, i32>>]
lookup_beta() {
  return(Result.ok(5i32))
}

[return<i32> effects(heap_alloc) on_error<i32, /unexpected_error>]
main() {
  [auto] beta_value{try(lookup_beta())}
  [auto] alpha_value{try(lookup_alpha())}
  return(beta_value)
}
)";

  const std::filesystem::path sourcePath =
      primec::testing::detail::makeCompilePipelineDumpSourcePath();
  {
    std::ofstream file(sourcePath);
    REQUIRE(file.good());
    file << source;
  }

  primec::Options options;
  options.inputPath = sourcePath.string();
  options.entryPath = "/main";
  options.emitKind = "native";
  options.defaultEffects = primec::testing::detail::defaultCompilePipelineTestingEffects();
  options.entryDefaultEffects = options.defaultEffects;
  options.benchmarkSemanticFactFamiliesSpecified = true;
  options.benchmarkSemanticFactFamilies = {"query_facts", "try_facts"};
  primec::testing::detail::applySemanticProductIntent(
      options, primec::testing::detail::CompilePipelineSemanticProductIntent::Require);
  primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);

  primec::CompilePipelineOutput output;
  primec::CompilePipelineErrorStage errorStage =
      primec::CompilePipelineErrorStage::None;
  std::string error;
  const bool ok = primec::runCompilePipeline(options, output, errorStage, error);
  std::error_code ec;
  std::filesystem::remove(sourcePath, ec);

  REQUIRE(ok);
  CHECK(error.empty());
  REQUIRE(output.hasSemanticProgram);
  const primec::SemanticProgram &semanticProgram = output.semanticProgram;
  const auto semanticTextOrFallback =
      [&semanticProgram](primec::SymbolId textId,
                         std::string_view fallback) {
        if (textId != primec::InvalidSymbolId) {
          const std::string_view resolved =
              primec::semanticProgramResolveCallTargetString(semanticProgram, textId);
          if (!resolved.empty()) {
            return std::string(resolved);
          }
        }
        return std::string(fallback);
      };

  std::vector<const primec::SemanticProgramQueryFact *> lookupQueryFacts;
  for (const auto *entry : primec::semanticProgramQueryFactView(semanticProgram)) {
    if (entry == nullptr ||
        semanticTextOrFallback(entry->scopePathId, entry->scopePath) != "/main") {
      continue;
    }
    const std::string callName =
        semanticTextOrFallback(entry->callNameId, entry->callName);
    const std::string resolvedPath =
        std::string(primec::semanticProgramQueryFactResolvedPath(semanticProgram, *entry));
    if ((callName == "lookup_beta" && resolvedPath == "/lookup_beta") ||
        (callName == "lookup_alpha" && resolvedPath == "/lookup_alpha")) {
      lookupQueryFacts.push_back(entry);
    }
  }
  REQUIRE(lookupQueryFacts.size() == 2);
  CHECK(primec::semanticProgramQueryFactResolvedPath(semanticProgram, *lookupQueryFacts[0]) ==
        "/lookup_beta");
  CHECK(primec::semanticProgramQueryFactResolvedPath(semanticProgram, *lookupQueryFacts[1]) ==
        "/lookup_alpha");
  CHECK(lookupQueryFacts[0]->sourceLine < lookupQueryFacts[1]->sourceLine);

  const primec::SemanticProgramQueryFact *queryFact = lookupQueryFacts.front();
  REQUIRE(queryFact != nullptr);
  CHECK(semanticTextOrFallback(queryFact->callNameId, queryFact->callName) ==
        "lookup_beta");
  CHECK(queryFact->bindingTypeText == "Result<int, i32>");
  CHECK(queryFact->hasResultType);
  CHECK(queryFact->resultTypeHasValue);
  CHECK(queryFact->resultValueType == "int");
  CHECK(queryFact->resultErrorType == "i32");
  REQUIRE(queryFact->semanticNodeId != 0);
  REQUIRE(queryFact->resolvedPathId != primec::InvalidSymbolId);
  REQUIRE(queryFact->callNameId != primec::InvalidSymbolId);
  CHECK(primec::semanticProgramLookupPublishedQueryFactBySemanticId(
            semanticProgram, queryFact->semanticNodeId) == queryFact);
  CHECK(primec::semanticProgramLookupPublishedQueryFactByResolvedPathAndCallNameId(
            semanticProgram, queryFact->resolvedPathId, queryFact->callNameId) ==
        queryFact);

  std::vector<const primec::SemanticProgramTryFact *> mainTryFacts;
  for (const auto *entry : primec::semanticProgramTryFactView(semanticProgram)) {
    if (entry != nullptr &&
        semanticTextOrFallback(entry->scopePathId, entry->scopePath) == "/main") {
      mainTryFacts.push_back(entry);
    }
  }
  REQUIRE(mainTryFacts.size() == 2);
  CHECK(primec::semanticProgramTryFactOperandResolvedPath(semanticProgram, *mainTryFacts[0]) ==
        "/lookup_beta");
  CHECK(primec::semanticProgramTryFactOperandResolvedPath(semanticProgram, *mainTryFacts[1]) ==
        "/lookup_alpha");
  CHECK(mainTryFacts[0]->sourceLine < mainTryFacts[1]->sourceLine);

  const primec::SemanticProgramTryFact *tryFact = nullptr;
  for (const auto *entry : primec::semanticProgramTryFactView(semanticProgram)) {
    if (entry != nullptr &&
        semanticTextOrFallback(entry->scopePathId, entry->scopePath) == "/main" &&
        primec::semanticProgramTryFactOperandResolvedPath(semanticProgram, *entry) ==
            "/lookup_beta") {
      tryFact = entry;
      break;
    }
  }
  REQUIRE(tryFact != nullptr);
  CHECK(tryFact->valueType == "int");
  CHECK(tryFact->errorType == "i32");
  CHECK(tryFact->onErrorHandlerPath == "/unexpected_error");
  REQUIRE(tryFact->semanticNodeId != 0);
  REQUIRE(tryFact->operandResolvedPathId != primec::InvalidSymbolId);
  CHECK(primec::semanticProgramLookupPublishedTryFactBySemanticId(
            semanticProgram, tryFact->semanticNodeId) == tryFact);
  CHECK(primec::semanticProgramLookupPublishedTryFactByOperandPathAndSource(
            semanticProgram,
            tryFact->operandResolvedPathId,
            tryFact->sourceLine,
            tryFact->sourceColumn) == tryFact);
}

