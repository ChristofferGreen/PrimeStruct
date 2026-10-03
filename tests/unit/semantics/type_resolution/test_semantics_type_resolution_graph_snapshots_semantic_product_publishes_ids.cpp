#include "third_party/doctest.h"

#include "test_semantics_type_resolution_graph_snapshots_shared.h"

TEST_SUITE_BEGIN("primestruct.semantics.type_resolution_graph");

TEST_CASE("semantic product return facts carry interned text ids") {
  const std::string source =
      "[return<i32>]\n"
      "helper() {\n"
      "  return(1i32)\n"
      "}\n"
      "\n"
      "[return<i32>]\n"
      "main() {\n"
      "  return(helper())\n"
      "}\n";

  auto program = parseProgram(source);
  primec::Semantics semantics;
  primec::SemanticProgram semanticProgram;
  std::string error;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  REQUIRE(semantics.validate(program, "/main", error, defaults, defaults, {}, nullptr, false, &semanticProgram));
  CHECK(error.empty());

  const auto *helperEntry = findSemanticEntry(
      primec::semanticProgramReturnFactView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramReturnFact &entry) {
        return primec::semanticProgramReturnFactDefinitionPath(semanticProgram, entry) == "/helper";
      });
  const auto *mainEntry = findSemanticEntry(
      primec::semanticProgramReturnFactView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramReturnFact &entry) {
        return primec::semanticProgramReturnFactDefinitionPath(semanticProgram, entry) == "/main";
      });
  REQUIRE(helperEntry != nullptr);
  REQUIRE(mainEntry != nullptr);

  REQUIRE(helperEntry->definitionPathId != primec::InvalidSymbolId);
  REQUIRE(helperEntry->returnKindId != primec::InvalidSymbolId);
  REQUIRE(helperEntry->bindingTypeTextId != primec::InvalidSymbolId);
  CHECK(helperEntry->referenceRootId == primec::InvalidSymbolId);
  CHECK(helperEntry->definitionPathId != mainEntry->definitionPathId);
  CHECK(helperEntry->returnKindId == mainEntry->returnKindId);
  CHECK(helperEntry->bindingTypeTextId == mainEntry->bindingTypeTextId);
  if (helperEntry->structPath.empty()) {
    CHECK(helperEntry->structPathId == primec::InvalidSymbolId);
  } else {
    REQUIRE(helperEntry->structPathId != primec::InvalidSymbolId);
    CHECK(primec::semanticProgramResolveCallTargetString(semanticProgram, helperEntry->structPathId) ==
          helperEntry->structPath);
  }
  if (mainEntry->structPath.empty()) {
    CHECK(mainEntry->structPathId == primec::InvalidSymbolId);
  } else {
    REQUIRE(mainEntry->structPathId != primec::InvalidSymbolId);
    CHECK(primec::semanticProgramResolveCallTargetString(semanticProgram, mainEntry->structPathId) ==
          mainEntry->structPath);
  }
  CHECK(primec::semanticProgramResolveCallTargetString(semanticProgram, helperEntry->definitionPathId) ==
        "/helper");
  CHECK(primec::semanticProgramResolveCallTargetString(semanticProgram, helperEntry->returnKindId) == "i32");
  CHECK(primec::semanticProgramResolveCallTargetString(semanticProgram, helperEntry->bindingTypeTextId) ==
        "i32");
}

TEST_CASE("semantic product local auto facts carry interned text ids") {
  const std::string source =
      "[return<i32>]\n"
      "id([i32] value) {\n"
      "  return(value)\n"
      "}\n"
      "\n"
      "[return<i32>]\n"
      "main() {\n"
      "  [auto] first{id(1i32)}\n"
      "  [auto] second{id(2i32)}\n"
      "  return(plus(first, second))\n"
      "}\n";

  auto program = parseProgram(source);
  primec::Semantics semantics;
  primec::SemanticProgram semanticProgram;
  std::string error;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  REQUIRE(semantics.validate(program, "/main", error, defaults, defaults, {}, nullptr, false, &semanticProgram));
  CHECK(error.empty());

  const auto *firstEntry = findSemanticEntry(
      primec::semanticProgramLocalAutoFactView(semanticProgram),
      [](const primec::SemanticProgramLocalAutoFact &entry) {
        return entry.scopePath == "/main" && entry.bindingName == "first";
      });
  const auto *secondEntry = findSemanticEntry(
      primec::semanticProgramLocalAutoFactView(semanticProgram),
      [](const primec::SemanticProgramLocalAutoFact &entry) {
        return entry.scopePath == "/main" && entry.bindingName == "second";
      });
  REQUIRE(firstEntry != nullptr);
  REQUIRE(secondEntry != nullptr);

  const auto checkTextId = [&](std::string_view text, primec::SymbolId id) {
    if (text.empty()) {
      CHECK(id == primec::InvalidSymbolId);
    } else {
      REQUIRE(id != primec::InvalidSymbolId);
      CHECK(primec::semanticProgramResolveCallTargetString(semanticProgram, id) == text);
    }
  };

  checkTextId(firstEntry->scopePath, firstEntry->scopePathId);
  checkTextId(firstEntry->bindingName, firstEntry->bindingNameId);
  checkTextId(firstEntry->bindingTypeText, firstEntry->bindingTypeTextId);
  checkTextId(
      primec::semanticProgramLocalAutoFactInitializerResolvedPath(semanticProgram, *firstEntry),
      firstEntry->initializerResolvedPathId);
  checkTextId(firstEntry->initializerBindingTypeText, firstEntry->initializerBindingTypeTextId);
  checkTextId(firstEntry->initializerReceiverBindingTypeText,
              firstEntry->initializerReceiverBindingTypeTextId);
  checkTextId(firstEntry->initializerQueryTypeText, firstEntry->initializerQueryTypeTextId);
  checkTextId(firstEntry->initializerResultValueType, firstEntry->initializerResultValueTypeId);
  checkTextId(firstEntry->initializerResultErrorType, firstEntry->initializerResultErrorTypeId);
  checkTextId(firstEntry->initializerTryOperandResolvedPath,
              firstEntry->initializerTryOperandResolvedPathId);
  checkTextId(firstEntry->initializerTryOperandBindingTypeText,
              firstEntry->initializerTryOperandBindingTypeTextId);
  checkTextId(firstEntry->initializerTryOperandReceiverBindingTypeText,
              firstEntry->initializerTryOperandReceiverBindingTypeTextId);
  checkTextId(firstEntry->initializerTryOperandQueryTypeText,
              firstEntry->initializerTryOperandQueryTypeTextId);
  checkTextId(firstEntry->initializerTryValueType, firstEntry->initializerTryValueTypeId);
  checkTextId(firstEntry->initializerTryErrorType, firstEntry->initializerTryErrorTypeId);
  checkTextId(firstEntry->initializerTryContextReturnKind,
              firstEntry->initializerTryContextReturnKindId);
  checkTextId(firstEntry->initializerTryOnErrorHandlerPath,
              firstEntry->initializerTryOnErrorHandlerPathId);
  checkTextId(firstEntry->initializerTryOnErrorErrorType,
              firstEntry->initializerTryOnErrorErrorTypeId);
  checkTextId(firstEntry->initializerDirectCallResolvedPath,
              firstEntry->initializerDirectCallResolvedPathId);
  checkTextId(firstEntry->initializerDirectCallReturnKind,
              firstEntry->initializerDirectCallReturnKindId);
  checkTextId(firstEntry->initializerMethodCallResolvedPath,
              firstEntry->initializerMethodCallResolvedPathId);
  checkTextId(firstEntry->initializerMethodCallReturnKind,
              firstEntry->initializerMethodCallReturnKindId);

  CHECK(firstEntry->scopePathId == secondEntry->scopePathId);
  CHECK(firstEntry->bindingTypeTextId == secondEntry->bindingTypeTextId);
  CHECK(firstEntry->initializerResolvedPathId == secondEntry->initializerResolvedPathId);
  CHECK(firstEntry->initializerBindingTypeTextId == secondEntry->initializerBindingTypeTextId);
  CHECK(firstEntry->initializerDirectCallResolvedPathId ==
        secondEntry->initializerDirectCallResolvedPathId);
  CHECK(firstEntry->initializerDirectCallReturnKindId ==
        secondEntry->initializerDirectCallReturnKindId);
  CHECK(firstEntry->bindingNameId != secondEntry->bindingNameId);
}

TEST_CASE("semantic product try facts carry interned text ids") {
  const std::string source = R"(
MyError {
}

[return<void>]
unexpectedError([MyError] err) {
}

[return<Result<i32, MyError>>]
lookup() {
  return(Result.ok(4i32))
}

[return<Result<i32, MyError>> on_error<MyError, /unexpectedError>]
main() {
  [auto] selected{try(lookup())}
  return(Result.ok(selected))
}
)";

  auto program = parseProgram(source);
  primec::Semantics semantics;
  primec::SemanticProgram semanticProgram;
  std::string error;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  REQUIRE(semantics.validate(program, "/main", error, defaults, defaults, {}, nullptr, false, &semanticProgram));
  CHECK(error.empty());

  const auto *tryEntry = findSemanticEntry(
      primec::semanticProgramTryFactView(semanticProgram),
      [](const primec::SemanticProgramTryFact &entry) { return entry.scopePath == "/main"; });
  REQUIRE(tryEntry != nullptr);

  const auto checkTextId = [&](std::string_view text, primec::SymbolId id) {
    if (text.empty()) {
      CHECK(id == primec::InvalidSymbolId);
    } else {
      REQUIRE(id != primec::InvalidSymbolId);
      CHECK(primec::semanticProgramResolveCallTargetString(semanticProgram, id) == text);
    }
  };

  checkTextId(tryEntry->scopePath, tryEntry->scopePathId);
  checkTextId(primec::semanticProgramTryFactOperandResolvedPath(semanticProgram, *tryEntry),
              tryEntry->operandResolvedPathId);
  checkTextId(tryEntry->operandBindingTypeText, tryEntry->operandBindingTypeTextId);
  checkTextId(tryEntry->operandReceiverBindingTypeText, tryEntry->operandReceiverBindingTypeTextId);
  checkTextId(tryEntry->operandQueryTypeText, tryEntry->operandQueryTypeTextId);
  checkTextId(tryEntry->valueType, tryEntry->valueTypeId);
  checkTextId(tryEntry->errorType, tryEntry->errorTypeId);
  checkTextId(tryEntry->contextReturnKind, tryEntry->contextReturnKindId);
  checkTextId(tryEntry->onErrorHandlerPath, tryEntry->onErrorHandlerPathId);
  checkTextId(tryEntry->onErrorErrorType, tryEntry->onErrorErrorTypeId);
}

TEST_CASE("semantic product try facts accept qualified stdlib Result spelling") {
  const std::string source = R"(
import /std/result/*

MyError {
}

[return<void>]
unexpectedError([MyError] err) {
}

[return</std/result/Result<i32, MyError>>]
lookup() {
  return(Result.ok(4i32))
}

[return</std/result/Result<i32, MyError>> on_error<MyError, /unexpectedError>]
main() {
  [auto] selected{try(lookup())}
  return(Result.ok(selected))
}
)";

  const std::filesystem::path sourcePath =
      primec::testing::detail::makeCompilePipelineDumpSourcePath();
  {
    std::ofstream file(sourcePath);
    REQUIRE(static_cast<bool>(file));
    file << source;
  }

  primec::Options options;
  options.inputPath = sourcePath.string();
  options.entryPath = "/main";
  options.emitKind = "native";
  options.wasmProfile = "wasi";
  options.defaultEffects = {"io_out", "io_err"};
  options.entryDefaultEffects = options.defaultEffects;
  primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);

  primec::CompilePipelineOutput output;
  primec::CompilePipelineErrorStage errorStage = primec::CompilePipelineErrorStage::None;
  std::string error;
  const bool ok = primec::runCompilePipeline(options, output, errorStage, error);

  std::error_code ec;
  std::filesystem::remove(sourcePath, ec);

  REQUIRE(ok);
  CHECK(error.empty());
  REQUIRE(output.hasSemanticProgram);
  primec::SemanticProgram &semanticProgram = output.semanticProgram;

  const auto *queryEntry = findSemanticEntry(
      primec::semanticProgramQueryFactView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramQueryFact &entry) {
        return primec::semanticProgramResolveCallTargetString(semanticProgram, entry.scopePathId) == "/main" &&
               primec::semanticProgramResolveCallTargetString(semanticProgram, entry.callNameId) == "lookup";
      });
  REQUIRE(queryEntry != nullptr);
  CHECK(queryEntry->queryTypeText == "/std/result/Result__arity2__t5ae7b1c726c44fc7");
  CHECK(queryEntry->hasResultType);
  CHECK(queryEntry->resultTypeHasValue);
  CHECK(queryEntry->resultValueType == "i32");
  CHECK(queryEntry->resultErrorType == "MyError");

  const auto *tryEntry = findSemanticEntry(
      primec::semanticProgramTryFactView(semanticProgram),
      [](const primec::SemanticProgramTryFact &entry) { return entry.scopePath == "/main"; });
  REQUIRE(tryEntry != nullptr);
  CHECK(tryEntry->operandQueryTypeText == "/std/result/Result__arity2__t5ae7b1c726c44fc7");
  CHECK(tryEntry->valueType == "i32");
  CHECK(tryEntry->errorType == "MyError");
  // main's return type is the /std/result/Result sum, which the return-kind
  // classifier buckets as "array" (its catch-all for non-scalar aggregate
  // return types), not a literal "return" spelling.
  CHECK(tryEntry->contextReturnKind == "array");
  CHECK(tryEntry->onErrorHandlerPath == "/unexpectedError");
  CHECK(tryEntry->onErrorErrorType == "MyError");
}

TEST_CASE("semantic product on_error facts carry interned text ids") {
  const std::string source = R"(
MyError {
}

[return<void>]
unexpectedError([MyError] err) {
}

[return<Result<i32, MyError>>]
lookup() {
  return(Result.ok(4i32))
}

[return<Result<i32, MyError>> on_error<MyError, /unexpectedError>]
main() {
  [auto] selected{try(lookup())}
  return(Result.ok(selected))
}

[return<Result<i32, MyError>> on_error<MyError, /unexpectedError>]
other() {
  [auto] selected{try(lookup())}
  return(Result.ok(selected))
}
)";

  auto program = parseProgram(source);
  primec::Semantics semantics;
  primec::SemanticProgram semanticProgram;
  std::string error;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  REQUIRE(semantics.validate(program, "/main", error, defaults, defaults, {}, nullptr, false, &semanticProgram));
  CHECK(error.empty());

  const auto *mainEntry = findSemanticEntry(
      primec::semanticProgramOnErrorFactView(semanticProgram),
      [](const primec::SemanticProgramOnErrorFact &entry) { return entry.definitionPath == "/main"; });
  const auto *otherEntry = findSemanticEntry(
      primec::semanticProgramOnErrorFactView(semanticProgram),
      [](const primec::SemanticProgramOnErrorFact &entry) { return entry.definitionPath == "/other"; });
  REQUIRE(mainEntry != nullptr);
  REQUIRE(otherEntry != nullptr);

  const auto checkTextId = [&](std::string_view text, primec::SymbolId id) {
    if (text.empty()) {
      CHECK(id == primec::InvalidSymbolId);
    } else {
      REQUIRE(id != primec::InvalidSymbolId);
      CHECK(primec::semanticProgramResolveCallTargetString(semanticProgram, id) == text);
    }
  };

  checkTextId(mainEntry->definitionPath, mainEntry->definitionPathId);
  checkTextId(mainEntry->returnKind, mainEntry->returnKindId);
  checkTextId(primec::semanticProgramOnErrorFactHandlerPath(semanticProgram, *mainEntry),
              mainEntry->handlerPathId);
  checkTextId(mainEntry->errorType, mainEntry->errorTypeId);
  checkTextId(mainEntry->returnResultValueType, mainEntry->returnResultValueTypeId);
  checkTextId(mainEntry->returnResultErrorType, mainEntry->returnResultErrorTypeId);

  CHECK(mainEntry->boundArgTexts.size() == mainEntry->boundArgTextIds.size());
  for (std::size_t i = 0; i < mainEntry->boundArgTexts.size(); ++i) {
    checkTextId(mainEntry->boundArgTexts[i], mainEntry->boundArgTextIds[i]);
  }

  CHECK(mainEntry->definitionPathId != otherEntry->definitionPathId);
  CHECK(mainEntry->returnKindId == otherEntry->returnKindId);
  CHECK(mainEntry->handlerPathId == otherEntry->handlerPathId);
  CHECK(mainEntry->errorTypeId == otherEntry->errorTypeId);
  CHECK(mainEntry->returnResultValueTypeId == otherEntry->returnResultValueTypeId);
  CHECK(mainEntry->returnResultErrorTypeId == otherEntry->returnResultErrorTypeId);
}

TEST_CASE("semantic product publishes same-path collection bridge routing choices") {
  const std::string source =
      "[return<i32>]\n"
      "/vector/count([vector<i32>] values) {\n"
      "  return(17i32)\n"
      "}\n"
      "\n"
      "[return<i32>]\n"
      "main() {\n"
      "  [auto] values{vector(1i32)}\n"
      "  return(count(values))\n"
      "}\n";

  auto program = parseProgram(source);
  primec::Semantics semantics;
  primec::SemanticProgram semanticProgram;
  std::string error;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  CHECK_FALSE(semantics.validate(program, "/main", error, defaults, defaults, {}, nullptr, false, &semanticProgram));
  CHECK_FALSE(error.empty());
}

TEST_CASE("semantic product publishes canonical collection bridge routing choices") {
  const std::string source =
      "[effects(heap_alloc), return<int>]\n"
      "main() {\n"
      "  [map<string, i32>] values{map<string, i32>(\"left\"raw_utf8, 4i32, \"right\"raw_utf8, 7i32)}\n"
      "  return(count(values))\n"
      "}\n";

  auto program = parseProgram(source);
  primec::Semantics semantics;
  primec::SemanticProgram semanticProgram;
  std::string error;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  CHECK_FALSE(semantics.validate(program, "/main", error, defaults, defaults, {}, nullptr, false, &semanticProgram));
  CHECK_FALSE(error.empty());
}

TEST_CASE("semantic product bridge routing choices carry interned path ids") {
  primec::SemanticProgram semanticProgram;
  auto makeBridgeChoice = [&](uint64_t semanticNodeId,
                              int sourceLine,
                              int sourceColumn) -> primec::SemanticProgramBridgePathChoice {
    primec::SemanticProgramBridgePathChoice entry;
    entry.scopePath = "/main";
    entry.collectionFamily = "map";
    entry.sourceLine = sourceLine;
    entry.sourceColumn = sourceColumn;
    entry.semanticNodeId = semanticNodeId;
    entry.provenanceHandle = semanticNodeId + 1000;
    entry.scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.scopePath);
    entry.collectionFamilyId =
        primec::semanticProgramInternCallTargetString(semanticProgram, entry.collectionFamily);
    entry.helperNameId = primec::semanticProgramInternCallTargetString(semanticProgram, "count");
    entry.chosenPathId =
        primec::semanticProgramInternCallTargetString(semanticProgram, "/std/collections/map/count");
    return entry;
  };

  semanticProgram.bridgePathChoices.push_back(makeBridgeChoice(101, 10, 4));
  semanticProgram.bridgePathChoices.push_back(makeBridgeChoice(102, 11, 6));

  REQUIRE(semanticProgram.bridgePathChoices.size() == 2);
  const auto &first = semanticProgram.bridgePathChoices[0];
  const auto &second = semanticProgram.bridgePathChoices[1];
  REQUIRE(first.scopePathId != primec::InvalidSymbolId);
  REQUIRE(first.collectionFamilyId != primec::InvalidSymbolId);
  REQUIRE(first.helperNameId != primec::InvalidSymbolId);
  REQUIRE(first.chosenPathId != primec::InvalidSymbolId);
  CHECK(first.scopePathId == second.scopePathId);
  CHECK(first.collectionFamilyId == second.collectionFamilyId);
  CHECK(first.helperNameId == second.helperNameId);
  CHECK(first.chosenPathId == second.chosenPathId);
  CHECK(primec::semanticProgramResolveCallTargetString(semanticProgram, first.chosenPathId) ==
        "/std/collections/map/count");
}

TEST_CASE("semantic product publishes callable effect and capability summaries") {
  const std::string source =
      "MyError {\n"
      "}\n"
      "\n"
      "[return<void>]\n"
      "unexpectedError([MyError] err) {\n"
      "}\n"
      "\n"
      "[effects(io_out, asset_read) capabilities(io_out) return<Result<int, MyError>> on_error<MyError, /unexpectedError>]\n"
      "main() {\n"
      "  return(Result.ok(4i32))\n"
      "}\n";

  auto program = parseProgram(source);
  primec::Semantics semantics;
  primec::SemanticProgram semanticProgram;
  std::string error;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  REQUIRE(semantics.validate(program, "/main", error, defaults, defaults, {}, nullptr, false, &semanticProgram));
  CHECK(error.empty());

  const auto *summaryEntry = findSemanticEntry(
      primec::semanticProgramCallableSummaryView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramCallableSummary &entry) {
        return primec::semanticProgramCallableSummaryFullPath(semanticProgram, entry) ==
                   "/main" &&
               !entry.isExecution;
      });
  REQUIRE(summaryEntry != nullptr);
  const bool hasExpectedReturnKind =
      summaryEntry->returnKind == "i32" || summaryEntry->returnKind == "i64";
  CHECK(hasExpectedReturnKind);
  CHECK(summaryEntry->activeEffects ==
        std::vector<std::string>{"asset_read", "io_out"});
  CHECK(summaryEntry->activeCapabilities == std::vector<std::string>{"io_out"});
  CHECK(summaryEntry->hasResultType);
  CHECK(summaryEntry->resultTypeHasValue);
  const bool hasExpectedResultValueType =
      summaryEntry->resultValueType == "i32" || summaryEntry->resultValueType == "int";
  CHECK(hasExpectedResultValueType);
  CHECK(summaryEntry->resultErrorType == "MyError");
  CHECK(summaryEntry->hasOnError);
  CHECK(summaryEntry->onErrorHandlerPath == "/unexpectedError");
  CHECK(summaryEntry->onErrorErrorType == "MyError");
  REQUIRE(summaryEntry->fullPathId != primec::InvalidSymbolId);
  REQUIRE(summaryEntry->returnKindId != primec::InvalidSymbolId);
  CHECK(summaryEntry->activeEffectIds.size() == summaryEntry->activeEffects.size());
  CHECK(summaryEntry->activeCapabilityIds.size() == summaryEntry->activeCapabilities.size());
  CHECK(primec::semanticProgramResolveCallTargetString(semanticProgram, summaryEntry->fullPathId) == "/main");
  CHECK(primec::semanticProgramResolveCallTargetString(semanticProgram, summaryEntry->returnKindId) ==
        summaryEntry->returnKind);
  CHECK(primec::semanticProgramResolveCallTargetString(
            semanticProgram, summaryEntry->onErrorHandlerPathId) == "/unexpectedError");
}

TEST_CASE("semantic product callable summaries reuse interned return kind ids") {
  const std::string source =
      "[return<i32>]\n"
      "helper() {\n"
      "  return(1i32)\n"
      "}\n"
      "\n"
      "[return<i32>]\n"
      "main() {\n"
      "  return(helper())\n"
      "}\n";

  auto program = parseProgram(source);
  primec::Semantics semantics;
  primec::SemanticProgram semanticProgram;
  std::string error;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  REQUIRE(semantics.validate(program, "/main", error, defaults, defaults, {}, nullptr, false, &semanticProgram));
  CHECK(error.empty());

  const auto summaries = primec::semanticProgramCallableSummaryView(semanticProgram);
  const auto *helperSummary = findSemanticEntry(
      summaries,
      [&semanticProgram](const primec::SemanticProgramCallableSummary &entry) {
        return primec::semanticProgramCallableSummaryFullPath(semanticProgram, entry) == "/helper";
      });
  const auto *mainSummary = findSemanticEntry(
      summaries,
      [&semanticProgram](const primec::SemanticProgramCallableSummary &entry) {
        return primec::semanticProgramCallableSummaryFullPath(semanticProgram, entry) == "/main";
      });
  REQUIRE(helperSummary != nullptr);
  REQUIRE(mainSummary != nullptr);
  REQUIRE(helperSummary->returnKindId != primec::InvalidSymbolId);
  REQUIRE(mainSummary->returnKindId != primec::InvalidSymbolId);
  CHECK(helperSummary->returnKindId == mainSummary->returnKindId);
  CHECK(primec::semanticProgramResolveCallTargetString(semanticProgram, helperSummary->returnKindId) == "i32");
}

TEST_CASE("semantic product publishes binding and return facts") {
  const std::string source =
      "Pair {\n"
      "  [i32] left{1i32}\n"
      "  [i64] right{2i64}\n"
      "}\n"
      "\n"
      "[return<T>]\n"
      "id<T>([T] value) {\n"
      "  return(value)\n"
      "}\n"
      "\n"
      "[return<Pair>]\n"
      "makePair([i32] base) {\n"
      "  [i64] widened{2i64}\n"
      "  [Pair] pair{Pair(base, widened)}\n"
      "  return(pair)\n"
      "}\n"
      "\n"
      "[return<i32>]\n"
      "main([array<string>] argv) {\n"
      "  [i32] seed{7i32}\n"
      "  [i32] chosen{id(seed)}\n"
      "  return(chosen)\n"
      "}\n";

  auto program = parseProgram(source);
  primec::Semantics semantics;
  primec::SemanticProgram semanticProgram;
  std::string error;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  REQUIRE(semantics.validate(program, "/main", error, defaults, defaults, {}, nullptr, false, &semanticProgram));
  CHECK(error.empty());

  const auto *parameterEntry = findSemanticEntry(
      primec::semanticProgramBindingFactView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramBindingFact &entry) {
        return primec::semanticProgramResolveCallTargetString(semanticProgram, entry.scopePathId) == "/makePair" &&
               primec::semanticProgramResolveCallTargetString(semanticProgram, entry.siteKindId) == "parameter" &&
               primec::semanticProgramResolveCallTargetString(semanticProgram, entry.nameId) == "base";
      });
  REQUIRE(parameterEntry != nullptr);
  CHECK(parameterEntry->bindingTypeText == "i32");

  const auto *localEntry = findSemanticEntry(
      primec::semanticProgramBindingFactView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramBindingFact &entry) {
        return primec::semanticProgramResolveCallTargetString(semanticProgram, entry.scopePathId) == "/makePair" &&
               primec::semanticProgramResolveCallTargetString(semanticProgram, entry.siteKindId) == "local" &&
               primec::semanticProgramResolveCallTargetString(semanticProgram, entry.nameId) == "widened";
      });
  REQUIRE(localEntry != nullptr);
  CHECK(localEntry->bindingTypeText == "i64");

  const auto *helperParameterEntry = findSemanticEntry(
      primec::semanticProgramBindingFactView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramBindingFact &entry) {
        return primec::semanticProgramResolveCallTargetString(semanticProgram, entry.siteKindId) == "parameter" &&
               primec::semanticProgramResolveCallTargetString(semanticProgram, entry.nameId) == "value" &&
               primec::semanticProgramResolveCallTargetString(semanticProgram, entry.scopePathId).rfind("/id", 0) == 0;
      });
  REQUIRE(helperParameterEntry != nullptr);
  CHECK(helperParameterEntry->bindingTypeText == "i32");

  const auto *entryParameterEntry = findSemanticEntry(
      primec::semanticProgramBindingFactView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramBindingFact &entry) {
        return primec::semanticProgramResolveCallTargetString(semanticProgram, entry.scopePathId) == "/main" &&
               primec::semanticProgramResolveCallTargetString(semanticProgram, entry.siteKindId) == "parameter" &&
               primec::semanticProgramResolveCallTargetString(semanticProgram, entry.nameId) == "argv";
      });
  REQUIRE(entryParameterEntry != nullptr);
  CHECK(entryParameterEntry->bindingTypeText == "array<string>");

  const auto *temporaryEntry = findSemanticEntry(
      primec::semanticProgramBindingFactView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramBindingFact &entry) {
        const std::string_view name =
            primec::semanticProgramResolveCallTargetString(semanticProgram, entry.nameId);
        return primec::semanticProgramResolveCallTargetString(semanticProgram, entry.scopePathId) == "/main" &&
               primec::semanticProgramResolveCallTargetString(semanticProgram, entry.siteKindId) == "temporary" &&
               (name == "id" || name.rfind("/id__t", 0) == 0) &&
               entry.bindingTypeText == "i32";
      });
  REQUIRE(temporaryEntry != nullptr);
  CHECK(temporaryEntry->sourceLine > 0);
  CHECK(temporaryEntry->sourceColumn > 0);

  const auto *mainReturnEntry = findSemanticEntry(
      primec::semanticProgramReturnFactView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramReturnFact &entry) {
        return primec::semanticProgramReturnFactDefinitionPath(semanticProgram, entry) == "/main";
      });
  REQUIRE(mainReturnEntry != nullptr);
  CHECK(mainReturnEntry->returnKind == "i32");
  CHECK(mainReturnEntry->bindingTypeText == "i32");

  const auto *pairReturnEntry = findSemanticEntry(
      primec::semanticProgramReturnFactView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramReturnFact &entry) {
        return primec::semanticProgramReturnFactDefinitionPath(semanticProgram, entry) == "/makePair";
      });
  REQUIRE(pairReturnEntry != nullptr);
  CHECK(pairReturnEntry->structPath == "/Pair");
  CHECK(pairReturnEntry->bindingTypeText == "Pair");
}

TEST_CASE("semantic product publishes array extent facts") {
  const std::string source = R"(
[return<i32>]
consume([Reference<array<i32>>] values) {
  [i32] viaParam{count(values)}
  return(viaParam)
}

[return<i32>]
score([args<Reference<i32>>] refs) {
  [i32] viaArgs{count(refs)}
  return(viaArgs)
}

[return<i32>]
main() {
  [array<i32>] values{array<i32>{1i32, 2i32, 3i32}}
  [array<i32>] window{slice(values, 1i32, 3i32)}
  [i32] localCount{count(values)}
  [i32] windowCount{count(window)}
  [Reference<array<i32>>] ref{location(values)}
  return(consume(ref))
}
)";

  auto program = parseProgram(source);
  primec::Semantics semantics;
  primec::SemanticProgram semanticProgram;
  std::string error;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  REQUIRE_MESSAGE(
      semantics.validate(program, "/main", error, defaults, defaults, {}, nullptr,
                         false, &semanticProgram),
      error);
  CHECK(error.empty());

  const auto arrayExtentFacts =
      primec::semanticProgramArrayExtentFactView(semanticProgram);
  const auto *localExtent = findSemanticEntry(
      arrayExtentFacts,
      [](const primec::SemanticProgramArrayExtentFact &entry) {
        return entry.scopePath == "/main" &&
               entry.siteKind == "local-value" &&
               entry.targetName == "values";
      });
  REQUIRE(localExtent != nullptr);
  CHECK(localExtent->bindingTypeText == "array<i32>");
  CHECK(localExtent->elementTypeText == "i32");
  CHECK(localExtent->extentExpression == "count(values)");
  CHECK_FALSE(localExtent->isReference);
  CHECK(localExtent->hasStaticExtent);
  CHECK(localExtent->staticExtent == 3);
  CHECK(localExtent->semanticNodeId == localExtent->targetSemanticNodeId);
  CHECK(primec::semanticProgramArrayExtentFactTargetResolvedPath(
            semanticProgram, *localExtent) == "/main/values");

  const auto *sliceExtent = findSemanticEntry(
      arrayExtentFacts,
      [](const primec::SemanticProgramArrayExtentFact &entry) {
        return entry.scopePath == "/main" &&
               entry.siteKind == "local-value" &&
               entry.targetName == "window";
      });
  REQUIRE(sliceExtent != nullptr);
  CHECK(sliceExtent->bindingTypeText == "array<i32>");
  CHECK(sliceExtent->elementTypeText == "i32");
  CHECK(sliceExtent->extentExpression == "3 - 1");
  CHECK_FALSE(sliceExtent->isReference);
  CHECK(sliceExtent->hasStaticExtent);
  CHECK(sliceExtent->staticExtent == 2);

  const auto *parameterExtent = findSemanticEntry(
      arrayExtentFacts,
      [](const primec::SemanticProgramArrayExtentFact &entry) {
        return entry.scopePath == "/consume" &&
               entry.siteKind == "parameter-reference" &&
               entry.targetName == "values";
      });
  REQUIRE(parameterExtent != nullptr);
  CHECK(parameterExtent->bindingTypeText == "Reference<array<i32>>");
  CHECK(parameterExtent->elementTypeText == "i32");
  CHECK(parameterExtent->isReference);
  CHECK_FALSE(parameterExtent->hasStaticExtent);

  const auto *argsParameterExtent = findSemanticEntry(
      arrayExtentFacts,
      [](const primec::SemanticProgramArrayExtentFact &entry) {
        return entry.scopePath == "/score" &&
               entry.siteKind == "parameter-value" &&
               entry.targetName == "refs";
      });
  REQUIRE(argsParameterExtent != nullptr);
  CHECK(argsParameterExtent->bindingTypeText == "args<Reference<i32>>");
  CHECK(argsParameterExtent->elementTypeText == "Reference<i32>");
  CHECK_FALSE(argsParameterExtent->isReference);
  CHECK_FALSE(argsParameterExtent->hasStaticExtent);

  const auto *localCountExtent = findSemanticEntry(
      arrayExtentFacts,
      [](const primec::SemanticProgramArrayExtentFact &entry) {
        return entry.scopePath == "/main" &&
               entry.siteKind == "count-expression" &&
               entry.targetName == "values";
      });
  REQUIRE(localCountExtent != nullptr);
  CHECK(localCountExtent->bindingTypeText == "array<i32>");
  CHECK(localCountExtent->hasStaticExtent);
  CHECK(localCountExtent->staticExtent == 3);
  CHECK(localCountExtent->targetSemanticNodeId != 0);
  CHECK(localCountExtent->semanticNodeId != localCountExtent->targetSemanticNodeId);

  const auto *sliceCountExtent = findSemanticEntry(
      arrayExtentFacts,
      [](const primec::SemanticProgramArrayExtentFact &entry) {
        return entry.scopePath == "/main" &&
               entry.siteKind == "count-expression" &&
               entry.targetName == "window";
      });
  REQUIRE(sliceCountExtent != nullptr);
  CHECK(sliceCountExtent->bindingTypeText == "array<i32>");
  CHECK(sliceCountExtent->hasStaticExtent);
  CHECK(sliceCountExtent->staticExtent == 2);

  const auto *parameterCountExtent = findSemanticEntry(
      arrayExtentFacts,
      [](const primec::SemanticProgramArrayExtentFact &entry) {
        return entry.scopePath == "/consume" &&
               entry.siteKind == "count-expression" &&
               entry.targetName == "values";
      });
  REQUIRE(parameterCountExtent != nullptr);
  CHECK(parameterCountExtent->bindingTypeText == "Reference<array<i32>>");
  CHECK(parameterCountExtent->isReference);
  CHECK_FALSE(parameterCountExtent->hasStaticExtent);
  CHECK(parameterCountExtent->targetSemanticNodeId != 0);

  const auto *argsCountExtent = findSemanticEntry(
      arrayExtentFacts,
      [](const primec::SemanticProgramArrayExtentFact &entry) {
        return entry.scopePath == "/score" &&
               entry.siteKind == "count-expression" &&
               entry.targetName == "refs";
      });
  REQUIRE(argsCountExtent != nullptr);
  CHECK(argsCountExtent->bindingTypeText == "args<Reference<i32>>");
  CHECK(argsCountExtent->elementTypeText == "Reference<i32>");
  CHECK_FALSE(argsCountExtent->isReference);
  CHECK_FALSE(argsCountExtent->hasStaticExtent);
  CHECK(argsCountExtent->targetSemanticNodeId != 0);

  const auto *lookupEntry =
      primec::semanticProgramLookupPublishedArrayExtentFactBySemanticId(
          semanticProgram, localCountExtent->semanticNodeId);
  REQUIRE(lookupEntry != nullptr);
  CHECK(lookupEntry->siteKind == "count-expression");
  CHECK(lookupEntry->targetName == "values");

  const std::string formatted = primec::formatSemanticProgram(semanticProgram);
  CHECK(formatted.find("array_extent_facts[") != std::string::npos);
  CHECK(formatted.find("site_kind=\"local-value\"") != std::string::npos);
  CHECK(formatted.find("site_kind=\"parameter-reference\"") !=
        std::string::npos);
  CHECK(formatted.find("has_static_extent=true static_extent=3") !=
        std::string::npos);
}

TEST_CASE("semantic product publishes graph-backed local auto query try and on_error facts") {
  const std::string source = R"(
MyError {
}

[return<void>]
unexpectedError([MyError] err) {
}

[return<Result<int, MyError>>]
lookup() {
  return(Result.ok(4i32))
}

[return<Result<int, MyError>> on_error<MyError, /unexpectedError>]
main() {
  [auto] selected{try(lookup())}
  return(Result.ok(selected))
}
)";

  auto program = parseProgram(source);
  primec::Semantics semantics;
  primec::SemanticProgram semanticProgram;
  std::string error;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  REQUIRE(semantics.validate(program, "/main", error, defaults, defaults, {}, nullptr, false, &semanticProgram));
  CHECK(error.empty());

  const auto *localAutoEntry = findSemanticEntry(
      primec::semanticProgramLocalAutoFactView(semanticProgram),
      [](const primec::SemanticProgramLocalAutoFact &entry) {
        return entry.scopePath == "/main" && entry.bindingName == "selected";
      });
  REQUIRE(localAutoEntry != nullptr);
  CHECK(localAutoEntry->bindingTypeText == "i32");
  CHECK(primec::semanticProgramLocalAutoFactInitializerResolvedPath(
            semanticProgram, *localAutoEntry) == "/lookup");
  CHECK(localAutoEntry->initializerDirectCallResolvedPath == "/lookup");
  CHECK(!localAutoEntry->initializerDirectCallReturnKind.empty());
  CHECK(localAutoEntry->initializerMethodCallResolvedPath.empty());
  CHECK(localAutoEntry->initializerMethodCallReturnKind.empty());
  CHECK(localAutoEntry->initializerHasTry);
  const bool hasExpectedInitializerTryValueType =
      localAutoEntry->initializerTryValueType == "i32" ||
      localAutoEntry->initializerTryValueType == "int";
  CHECK(hasExpectedInitializerTryValueType);
  CHECK(localAutoEntry->initializerTryErrorType == "MyError");

  const auto *queryEntry = findSemanticEntry(
      primec::semanticProgramQueryFactView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramQueryFact &entry) {
        return primec::semanticProgramResolveCallTargetString(semanticProgram, entry.scopePathId) == "/main" &&
               primec::semanticProgramQueryFactResolvedPath(semanticProgram, entry) == "/lookup";
      });
  REQUIRE(queryEntry != nullptr);
  const std::string_view queryReceiverBindingTypeText =
      primec::semanticProgramResolveCallTargetString(
          semanticProgram, queryEntry->receiverBindingTypeTextId);
  CHECK(localAutoEntry->initializerReceiverBindingTypeText ==
        queryReceiverBindingTypeText);
  CHECK(localAutoEntry->initializerQueryTypeText == queryEntry->queryTypeText);
  CHECK(localAutoEntry->initializerResultHasValue ==
        queryEntry->resultTypeHasValue);
  CHECK(localAutoEntry->initializerResultValueType ==
        queryEntry->resultValueType);
  CHECK(localAutoEntry->initializerResultErrorType ==
        queryEntry->resultErrorType);
  CHECK_FALSE(queryEntry->bindingTypeText.empty());
  CHECK(queryEntry->hasResultType);
  CHECK(queryEntry->resultTypeHasValue);
  const bool hasExpectedQueryResultValueType =
      queryEntry->resultValueType == "i32" || queryEntry->resultValueType == "int";
  CHECK(hasExpectedQueryResultValueType);
  CHECK(queryEntry->resultErrorType == "MyError");

  const auto *tryEntry = findSemanticEntry(
      primec::semanticProgramTryFactView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramTryFact &entry) {
        return entry.scopePath == "/main" &&
               primec::semanticProgramTryFactOperandResolvedPath(semanticProgram, entry) == "/lookup";
      });
  REQUIRE(tryEntry != nullptr);
  const bool hasExpectedTryValueType =
      tryEntry->valueType == "i32" || tryEntry->valueType == "int";
  CHECK(hasExpectedTryValueType);
  CHECK(tryEntry->errorType == "MyError");
  const bool hasExpectedTryContextKind =
      tryEntry->contextReturnKind == "i32" || tryEntry->contextReturnKind == "i64";
  CHECK(hasExpectedTryContextKind);
  CHECK(tryEntry->onErrorHandlerPath == "/unexpectedError");

  const auto *onErrorEntry = findSemanticEntry(
      primec::semanticProgramOnErrorFactView(semanticProgram),
      [](const primec::SemanticProgramOnErrorFact &entry) { return entry.definitionPath == "/main"; });
  REQUIRE(onErrorEntry != nullptr);
  const bool hasExpectedOnErrorReturnKind =
      onErrorEntry->returnKind == "i32" || onErrorEntry->returnKind == "i64";
  CHECK(hasExpectedOnErrorReturnKind);
  CHECK(primec::semanticProgramOnErrorFactHandlerPath(semanticProgram, *onErrorEntry) ==
        "/unexpectedError");
  CHECK(onErrorEntry->errorType == "MyError");
  CHECK(onErrorEntry->boundArgTexts == std::vector<std::string>{});
  CHECK(onErrorEntry->returnResultHasValue);
  const bool hasExpectedOnErrorResultValueType =
      onErrorEntry->returnResultValueType == "i32" ||
      onErrorEntry->returnResultValueType == "int";
  CHECK(hasExpectedOnErrorResultValueType);
  CHECK(onErrorEntry->returnResultErrorType == "MyError");
}

TEST_CASE("semantic product publishes graph-backed local auto method-call facts") {
  const std::string source = R"(
[return<i32>]
pick([i32] value) {
  return(value)
}

[return<i32>]
/std/collections/vector/count([vector<i32>] self) {
  return(17i32)
}

[return<i32>]
main() {
  [vector<i32>] values{vector<i32>()}
  [auto] viaDirect{pick(1i32)}
  [auto] viaMethod{values./std/collections/vector/count()}
  return(plus(viaDirect, viaMethod))
}
)";

  auto program = parseProgram(source);
  primec::Semantics semantics;
  primec::SemanticProgram semanticProgram;
  std::string error;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  REQUIRE(semantics.validate(program, "/main", error, defaults, defaults, {}, nullptr, false, &semanticProgram));
  CHECK(error.empty());

  const auto *viaDirectEntry = findSemanticEntry(
      primec::semanticProgramLocalAutoFactView(semanticProgram),
      [](const primec::SemanticProgramLocalAutoFact &entry) {
        return entry.scopePath == "/main" && entry.bindingName == "viaDirect";
      });
  REQUIRE(viaDirectEntry != nullptr);
  CHECK(viaDirectEntry->initializerDirectCallResolvedPath == "/pick");
  CHECK(viaDirectEntry->initializerDirectCallReturnKind == "i32");
  CHECK(viaDirectEntry->initializerMethodCallResolvedPath.empty());
  CHECK(viaDirectEntry->initializerMethodCallReturnKind.empty());
  CHECK_FALSE(viaDirectEntry->initializerStdlibSurfaceId.has_value());
  CHECK_FALSE(viaDirectEntry->initializerDirectCallStdlibSurfaceId.has_value());
  CHECK_FALSE(viaDirectEntry->initializerMethodCallStdlibSurfaceId.has_value());

  const auto *viaMethodEntry = findSemanticEntry(
      primec::semanticProgramLocalAutoFactView(semanticProgram),
      [](const primec::SemanticProgramLocalAutoFact &entry) {
        return entry.scopePath == "/main" && entry.bindingName == "viaMethod";
      });
  REQUIRE(viaMethodEntry != nullptr);
  CHECK(viaMethodEntry->initializerDirectCallResolvedPath.empty());
  CHECK(viaMethodEntry->initializerDirectCallReturnKind.empty());
  CHECK(viaMethodEntry->initializerMethodCallResolvedPath == "/std/collections/vector/count");
  CHECK(viaMethodEntry->initializerMethodCallReturnKind == "i32");
  REQUIRE(viaMethodEntry->initializerStdlibSurfaceId.has_value());
  CHECK(*viaMethodEntry->initializerStdlibSurfaceId ==
        primec::StdlibSurfaceId::CollectionsManifestSurface0);
  CHECK_FALSE(viaMethodEntry->initializerDirectCallStdlibSurfaceId.has_value());
  REQUIRE(viaMethodEntry->initializerMethodCallStdlibSurfaceId.has_value());
  CHECK(*viaMethodEntry->initializerMethodCallStdlibSurfaceId ==
        primec::StdlibSurfaceId::CollectionsManifestSurface0);
}

TEST_CASE("semantic product publishes graph-backed collection helper direct-call facts") {
  const std::string source = R"(
[return<i32>]
/std/collections/vector/count([vector<i32>] self) {
  return(17i32)
}

[return<i32>]
main() {
  [vector<i32>] values{vector<i32>()}
  [auto] viaStd{/std/collections/vector/count(values)}
  return(viaStd)
}
)";

  auto program = parseProgram(source);
  primec::Semantics semantics;
  primec::SemanticProgram semanticProgram;
  std::string error;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  REQUIRE(semantics.validate(program, "/main", error, defaults, defaults, {}, nullptr, false, &semanticProgram));
  CHECK(error.empty());

  const auto *localAutoEntry = findSemanticEntry(
      primec::semanticProgramLocalAutoFactView(semanticProgram),
      [](const primec::SemanticProgramLocalAutoFact &entry) {
        return entry.scopePath == "/main" && entry.bindingName == "viaStd";
      });
  REQUIRE(localAutoEntry != nullptr);
  CHECK(localAutoEntry->initializerDirectCallResolvedPath == "/std/collections/vector/count");
  CHECK(localAutoEntry->initializerDirectCallReturnKind == "i32");
  REQUIRE(localAutoEntry->initializerStdlibSurfaceId.has_value());
  CHECK(*localAutoEntry->initializerStdlibSurfaceId ==
        primec::StdlibSurfaceId::CollectionsManifestSurface0);
  REQUIRE(localAutoEntry->initializerDirectCallStdlibSurfaceId.has_value());
  CHECK(*localAutoEntry->initializerDirectCallStdlibSurfaceId ==
        primec::StdlibSurfaceId::CollectionsManifestSurface0);
  CHECK_FALSE(localAutoEntry->initializerMethodCallStdlibSurfaceId.has_value());
}

TEST_CASE("semantic product publishes graph-backed collection constructor local-auto surface ids") {
  const std::string source = R"(
import /std/collections/vector

[effects(heap_alloc), return<i32>]
main() {
  [auto] values{vector<i32>(1i32)}
  return(1i32)
}
)";

  auto program = parseProgram(source);
  primec::Semantics semantics;
  primec::SemanticProgram semanticProgram;
  std::string error;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  REQUIRE_MESSAGE(
      semantics.validate(program, "/main", error, defaults, defaults, {}, nullptr,
                         false, &semanticProgram),
      error);
  CHECK(error.empty());

  const auto *localAutoEntry = findSemanticEntry(
      primec::semanticProgramLocalAutoFactView(semanticProgram),
      [](const primec::SemanticProgramLocalAutoFact &entry) {
        return entry.scopePath == "/main" && entry.bindingName == "values";
      });
  REQUIRE(localAutoEntry != nullptr);
  CHECK(localAutoEntry->bindingTypeText == "vector<i32>");
  REQUIRE(localAutoEntry->initializerStdlibSurfaceId.has_value());
  CHECK(*localAutoEntry->initializerStdlibSurfaceId ==
        primec::StdlibSurfaceId::CollectionsManifestSurface1);
  REQUIRE(localAutoEntry->initializerDirectCallStdlibSurfaceId.has_value());
  CHECK(*localAutoEntry->initializerDirectCallStdlibSurfaceId ==
        primec::StdlibSurfaceId::CollectionsManifestSurface1);
  CHECK_FALSE(localAutoEntry->initializerMethodCallStdlibSurfaceId.has_value());
}

TEST_CASE("semantic product publishes vector map and soa collection specializations") {
  const std::string source = R"(
[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<i32>]
main() {
  [vector<i32>] values{vector<i32>(1i32)}
  [map<i32, i64> mut] pairs{map<i32, i64>(1i32, 7i64)}
  [Reference<map<i32, i64>>] pairsRef{location(pairs)}
  [soa<Particle>] particles{soa<Particle>()}
  [Reference<soa<Particle>>] particleRefs{location(particles)}
  return(0i32)
}
)";

  auto program = parseProgram(source);
  primec::Semantics semantics;
  primec::SemanticProgram semanticProgram;
  std::string error;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  REQUIRE(
      semantics.validate(program, "/main", error, defaults, defaults, {}, nullptr, false,
                         &semanticProgram));
  CHECK(error.empty());

  const auto *vectorEntry = findSemanticEntry(
      primec::semanticProgramCollectionSpecializationView(semanticProgram),
      [](const primec::SemanticProgramCollectionSpecialization &entry) {
        return entry.scopePath == "/main" && entry.name == "values";
      });
  REQUIRE(vectorEntry != nullptr);
  CHECK(vectorEntry->collectionFamily == "vector");
  CHECK(vectorEntry->elementTypeText == "i32");
  REQUIRE(vectorEntry->helperSurfaceId.has_value());
  CHECK(*vectorEntry->helperSurfaceId == primec::StdlibSurfaceId::CollectionsManifestSurface0);
  REQUIRE(vectorEntry->constructorSurfaceId.has_value());
  CHECK(*vectorEntry->constructorSurfaceId ==
        primec::StdlibSurfaceId::CollectionsManifestSurface1);

  const auto *mapEntry = findSemanticEntry(
      primec::semanticProgramCollectionSpecializationView(semanticProgram),
      [](const primec::SemanticProgramCollectionSpecialization &entry) {
        return entry.scopePath == "/main" && entry.name == "pairsRef";
      });
  REQUIRE(mapEntry != nullptr);
  CHECK(mapEntry->collectionFamily == "map");
  CHECK(mapEntry->keyTypeText == "i32");
  CHECK(mapEntry->valueTypeText == "i64");
  CHECK(mapEntry->structPath.rfind("/std/collections/map/MapValue__", 0) == 0);
  REQUIRE(mapEntry->structPathId != primec::InvalidSymbolId);
  CHECK(primec::semanticProgramResolveCallTargetString(semanticProgram,
                                                       mapEntry->structPathId) ==
        mapEntry->structPath);
  CHECK(mapEntry->isReference);
  CHECK_FALSE(mapEntry->isPointer);
  REQUIRE(mapEntry->helperSurfaceId.has_value());
  CHECK(*mapEntry->helperSurfaceId == primec::findStdlibSurfaceMetadataByBridgeKey("collections.map_helpers")->id);
  REQUIRE(mapEntry->constructorSurfaceId.has_value());
  CHECK(*mapEntry->constructorSurfaceId ==
        primec::findStdlibSurfaceMetadataByBridgeKey("collections.map_constructors")->id);

  const auto *soaEntry = findSemanticEntry(
      primec::semanticProgramCollectionSpecializationView(semanticProgram),
      [](const primec::SemanticProgramCollectionSpecialization &entry) {
        return entry.scopePath == "/main" && entry.name == "particleRefs";
      });
  REQUIRE(soaEntry != nullptr);
  CHECK(soaEntry->collectionFamily == "soa");
  CHECK(soaEntry->elementTypeText == "Particle");
  CHECK(soaEntry->valueTypeText == "Particle");
  CHECK(soaEntry->isReference);
  CHECK_FALSE(soaEntry->isPointer);
  REQUIRE(soaEntry->helperSurfaceId.has_value());
  CHECK(*soaEntry->helperSurfaceId ==
        primec::StdlibSurfaceId::CollectionsColumnarHelpers);
  REQUIRE(soaEntry->constructorSurfaceId.has_value());
  CHECK(*soaEntry->constructorSurfaceId ==
        primec::StdlibSurfaceId::CollectionsColumnarConstructors);

  const auto *lookupEntry =
      primec::semanticProgramLookupPublishedCollectionSpecializationBySemanticId(
          semanticProgram, mapEntry->semanticNodeId);
  REQUIRE(lookupEntry != nullptr);
  CHECK(lookupEntry->keyTypeText == "i32");
  CHECK(lookupEntry->valueTypeText == "i64");

  const std::string formatted = primec::formatSemanticProgram(semanticProgram);
  CHECK(formatted.find("collection_specializations[") != std::string::npos);
  CHECK(formatted.find("struct_path=\"/std/collections/map/MapValue__") !=
        std::string::npos);
  CHECK(formatted.find("helper_surface_id=\"collections.map_helpers\"") != std::string::npos);
  CHECK(formatted.find("helper_surface_id=\"collections.soa_helpers\"") !=
        std::string::npos);
}

TEST_SUITE_END();
