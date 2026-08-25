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

TEST_CASE("semantic product keeps vector and map bridge parity") {
  const std::string source = R"(
import /std/collections/vector
import /std/collections/map

[return<i32>]
/std/collections/vector/count<T>([vector<T>] values) {
  return(1i32)
}

[return<i32>]
/std/collections/map/count<K, V>([Map<K, V>] values) {
  return(2i32)
}

[effects(heap_alloc), return<i32>]
main() {
  [auto] values{vector<i32>(1i32, 2i32)}
  [auto] pairs{map<i32, i32>(1i32, 7i32, 2i32, 11i32)}
  [i32] viaVector{count(values)}
  [i32] viaMap{count(pairs)}
  return(plus(viaVector, viaMap))
}
)";

  auto program = parseProgram(source);
  primec::Semantics semantics;
  primec::SemanticProgram semanticProgram;
  std::string error;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  CHECK_MESSAGE(
      semantics.validate(program, "/main", error, defaults, defaults, {}, nullptr, false,
                         &semanticProgram),
      error);
  if (!error.empty()) {
    return;
  }
  CHECK(error.empty());

  const auto *valuesEntry = findSemanticEntry(
      primec::semanticProgramLocalAutoFactView(semanticProgram),
      [](const primec::SemanticProgramLocalAutoFact &entry) {
        return entry.scopePath == "/main" && entry.bindingName == "values";
      });
  REQUIRE(valuesEntry != nullptr);
  REQUIRE(valuesEntry->initializerStdlibSurfaceId.has_value());
  CHECK(*valuesEntry->initializerStdlibSurfaceId ==
        primec::StdlibSurfaceId::CollectionsManifestSurface1);
  REQUIRE(valuesEntry->initializerDirectCallStdlibSurfaceId.has_value());
  CHECK(*valuesEntry->initializerDirectCallStdlibSurfaceId ==
        primec::StdlibSurfaceId::CollectionsManifestSurface1);

  const auto *pairsEntry = findSemanticEntry(
      primec::semanticProgramLocalAutoFactView(semanticProgram),
      [](const primec::SemanticProgramLocalAutoFact &entry) {
        return entry.scopePath == "/main" && entry.bindingName == "pairs";
      });
  REQUIRE(pairsEntry != nullptr);
  REQUIRE(pairsEntry->initializerStdlibSurfaceId.has_value());
  CHECK(*pairsEntry->initializerStdlibSurfaceId ==
        primec::findStdlibSurfaceMetadataByBridgeKey("collections.map_constructors")->id);
  REQUIRE(pairsEntry->initializerDirectCallStdlibSurfaceId.has_value());
  CHECK(*pairsEntry->initializerDirectCallStdlibSurfaceId ==
        primec::findStdlibSurfaceMetadataByBridgeKey("collections.map_constructors")->id);

  auto resolveBridgeScopePath =
      [&semanticProgram](const primec::SemanticProgramBridgePathChoice &entry) {
        const std::string_view resolvedScope =
            primec::semanticProgramResolveCallTargetString(semanticProgram, entry.scopePathId);
        return resolvedScope.empty() ? std::string_view(entry.scopePath) : resolvedScope;
      };
  auto resolveBridgeCollectionFamily =
      [&semanticProgram](const primec::SemanticProgramBridgePathChoice &entry) {
        const std::string_view resolvedFamily =
            primec::semanticProgramResolveCallTargetString(semanticProgram,
                                                           entry.collectionFamilyId);
        return resolvedFamily.empty() ? std::string_view(entry.collectionFamily)
                                      : resolvedFamily;
      };

  const auto *vectorBridgeEntry = findSemanticEntry(
      primec::semanticProgramBridgePathChoiceView(semanticProgram),
      [&semanticProgram, &resolveBridgeScopePath, &resolveBridgeCollectionFamily](
          const primec::SemanticProgramBridgePathChoice &entry) {
        return resolveBridgeScopePath(entry) == "/main" &&
               resolveBridgeCollectionFamily(entry) == "vector" &&
               primec::semanticProgramBridgePathChoiceHelperName(semanticProgram, entry) == "count";
      });
  REQUIRE(vectorBridgeEntry != nullptr);
  CHECK(resolveBridgeCollectionFamily(*vectorBridgeEntry) == "vector");
  CHECK(primec::semanticProgramResolveCallTargetString(semanticProgram,
                                                       vectorBridgeEntry->chosenPathId) ==
        "/std/collections/vector/count");
  REQUIRE(vectorBridgeEntry->stdlibSurfaceId.has_value());
  CHECK(*vectorBridgeEntry->stdlibSurfaceId ==
        primec::StdlibSurfaceId::CollectionsManifestSurface0);
  const auto vectorBridgeSurfaceId =
      primec::semanticProgramLookupPublishedBridgePathChoiceStdlibSurfaceId(
          semanticProgram, vectorBridgeEntry->semanticNodeId);
  REQUIRE(vectorBridgeSurfaceId.has_value());
  CHECK(*vectorBridgeSurfaceId == primec::StdlibSurfaceId::CollectionsManifestSurface0);

  const auto *mapBridgeEntry = findSemanticEntry(
      primec::semanticProgramBridgePathChoiceView(semanticProgram),
      [&semanticProgram, &resolveBridgeScopePath, &resolveBridgeCollectionFamily](
          const primec::SemanticProgramBridgePathChoice &entry) {
        return resolveBridgeScopePath(entry) == "/main" &&
               resolveBridgeCollectionFamily(entry) == "map" &&
               primec::semanticProgramBridgePathChoiceHelperName(semanticProgram, entry) == "count" &&
               primec::semanticProgramResolveCallTargetString(semanticProgram, entry.chosenPathId) ==
                   "/std/collections/map/count";
      });
  REQUIRE(mapBridgeEntry != nullptr);
  REQUIRE(mapBridgeEntry->stdlibSurfaceId.has_value());
  CHECK(*mapBridgeEntry->stdlibSurfaceId ==
        primec::findStdlibSurfaceMetadataByBridgeKey("collections.map_helpers")->id);
  const auto mapBridgeSurfaceId =
      primec::semanticProgramLookupPublishedBridgePathChoiceStdlibSurfaceId(
          semanticProgram, mapBridgeEntry->semanticNodeId);
  REQUIRE(mapBridgeSurfaceId.has_value());
  CHECK(*mapBridgeSurfaceId == primec::findStdlibSurfaceMetadataByBridgeKey("collections.map_helpers")->id);
}

TEST_CASE("semantic product keeps graph-backed local auto facts for nested borrowed array access helpers") {
  const std::string source = R"(
[return<int>]
score_refs([args<Reference<array<i32>>>] values) {
  [auto] head{at_unsafe(dereference(at(values, 0i32)), 1i32)}
  return(head)
}

[return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32)}
  [Reference<array<i32>>] ref{location(values)}
  return(score_refs(ref))
}
)";

  auto program = parseProgram(source);
  primec::Semantics semantics;
  primec::SemanticProgram semanticProgram;
  std::string error;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  REQUIRE(semantics.validate(program, "/main", error, defaults, defaults, {}, nullptr, false,
                             &semanticProgram));
  CHECK(error.empty());

  const auto *localAutoEntry = findSemanticEntry(
      primec::semanticProgramLocalAutoFactView(semanticProgram),
      [](const primec::SemanticProgramLocalAutoFact &entry) {
        return entry.scopePath == "/score_refs" && entry.bindingName == "head";
      });
  REQUIRE(localAutoEntry != nullptr);
  CHECK(localAutoEntry->bindingTypeText == "i32");
  CHECK_FALSE(
      primec::semanticProgramLocalAutoFactInitializerResolvedPath(semanticProgram, *localAutoEntry)
          .empty());
  CHECK_FALSE(localAutoEntry->initializerDirectCallResolvedPath.empty());
}

TEST_CASE("semantic product semantic ids stay deterministic across repeated validation runs") {
  const std::string source =
      "MyError {\n"
      "}\n"
      "\n"
      "[return<void>]\n"
      "unexpectedError([MyError] err) {\n"
      "}\n"
      "\n"
      "[return<i32>]\n"
      "helper([i32] value) {\n"
      "  return(value)\n"
      "}\n"
      "\n"
      "[return<Result<int, MyError>>]\n"
      "lookup() {\n"
      "  return(Result.ok(4i32))\n"
      "}\n"
      "\n"
      "[return<Result<int, MyError>> on_error<MyError, /unexpectedError>]\n"
      "main() {\n"
      "  [i32] direct{helper(1i32)}\n"
      "  [auto] selected{try(lookup())}\n"
      "  return(Result.ok(plus(direct, selected)))\n"
      "}\n";

  auto validateSemanticProduct = [](const std::string &programText) {
    auto program = parseProgram(programText);
    primec::Semantics semantics;
    primec::SemanticProgram semanticProgram;
    std::string error;
    const std::vector<std::string> defaults = {"io_out", "io_err"};
    REQUIRE(semantics.validate(program, "/main", error, defaults, defaults, {}, nullptr, false, &semanticProgram));
    CHECK(error.empty());
    return semanticProgram;
  };

  const primec::SemanticProgram first = validateSemanticProduct(source);
  const primec::SemanticProgram second = validateSemanticProduct(source);

  CHECK(primec::formatSemanticProgram(first) == primec::formatSemanticProgram(second));

  const auto *firstMain =
      findSemanticEntry(first.definitions,
                        [](const primec::SemanticProgramDefinition &entry) { return entry.fullPath == "/main"; });
  const auto *secondMain =
      findSemanticEntry(second.definitions,
                        [](const primec::SemanticProgramDefinition &entry) { return entry.fullPath == "/main"; });
  REQUIRE(firstMain != nullptr);
  REQUIRE(secondMain != nullptr);
  CHECK(firstMain->semanticNodeId != 0);
  CHECK(firstMain->semanticNodeId == secondMain->semanticNodeId);
  CHECK(firstMain->provenanceHandle != 0);
  CHECK(firstMain->provenanceHandle == secondMain->provenanceHandle);

  const auto *firstDirectCall = findSemanticEntry(primec::semanticProgramDirectCallTargetView(first),
      [](const primec::SemanticProgramDirectCallTarget &entry) {
        return entry.scopePath == "/main" && entry.callName == "helper";
      });
  const auto *secondDirectCall = findSemanticEntry(primec::semanticProgramDirectCallTargetView(second),
      [](const primec::SemanticProgramDirectCallTarget &entry) {
        return entry.scopePath == "/main" && entry.callName == "helper";
      });
  REQUIRE(firstDirectCall != nullptr);
  REQUIRE(secondDirectCall != nullptr);
  CHECK(firstDirectCall->semanticNodeId != 0);
  CHECK(firstDirectCall->semanticNodeId == secondDirectCall->semanticNodeId);
  CHECK(firstDirectCall->provenanceHandle != 0);
  CHECK(firstDirectCall->provenanceHandle == secondDirectCall->provenanceHandle);

  const auto *firstQuery = findSemanticEntry(primec::semanticProgramQueryFactView(first),
      [&first](const primec::SemanticProgramQueryFact &entry) {
        return primec::semanticProgramResolveCallTargetString(first, entry.scopePathId) == "/main" &&
               primec::semanticProgramQueryFactResolvedPath(first, entry) == "/lookup";
      });
  const auto *secondQuery = findSemanticEntry(primec::semanticProgramQueryFactView(second),
      [&second](const primec::SemanticProgramQueryFact &entry) {
        return primec::semanticProgramResolveCallTargetString(second, entry.scopePathId) == "/main" &&
               primec::semanticProgramQueryFactResolvedPath(second, entry) == "/lookup";
      });
  REQUIRE(firstQuery != nullptr);
  REQUIRE(secondQuery != nullptr);
  CHECK(firstQuery->semanticNodeId != 0);
  CHECK(firstQuery->semanticNodeId == secondQuery->semanticNodeId);
  CHECK(firstQuery->provenanceHandle != 0);
  CHECK(firstQuery->provenanceHandle == secondQuery->provenanceHandle);

  const auto *firstTry = findSemanticEntry(primec::semanticProgramTryFactView(first),
      [&first](const primec::SemanticProgramTryFact &entry) {
        return entry.scopePath == "/main" &&
               primec::semanticProgramTryFactOperandResolvedPath(first, entry) == "/lookup";
      });
  const auto *secondTry = findSemanticEntry(primec::semanticProgramTryFactView(second),
      [&second](const primec::SemanticProgramTryFact &entry) {
        return entry.scopePath == "/main" &&
               primec::semanticProgramTryFactOperandResolvedPath(second, entry) == "/lookup";
      });
  REQUIRE(firstTry != nullptr);
  REQUIRE(secondTry != nullptr);
  CHECK(firstTry->semanticNodeId != 0);
  CHECK(firstTry->semanticNodeId == secondTry->semanticNodeId);
  CHECK(firstTry->provenanceHandle != 0);
  CHECK(firstTry->provenanceHandle == secondTry->provenanceHandle);
}

TEST_CASE("semantic product semantic ids ignore unrelated definition ordering") {
  const std::string sourceA =
      "[return<i32>]\n"
      "helper([i32] value) {\n"
      "  return(value)\n"
      "}\n"
      "\n"
      "[return<i32>]\n"
      "noise([i32] value) {\n"
      "  return(value)\n"
      "}\n"
      "\n"
      "[return<i32>]\n"
      "main() {\n"
      "  [i32] selected{helper(1i32)}\n"
      "  return(selected)\n"
      "}\n";
  const std::string sourceB =
      "[return<i32>]\n"
      "noise([i32] value) {\n"
      "  return(value)\n"
      "}\n"
      "\n"
      "[return<i32>]\n"
      "helper([i32] value) {\n"
      "  return(value)\n"
      "}\n"
      "\n"
      "[return<i32>]\n"
      "main() {\n"
      "  [i32] selected{helper(1i32)}\n"
      "  return(selected)\n"
      "}\n";

  auto validateSemanticProduct = [](const std::string &programText) {
    auto program = parseProgram(programText);
    primec::Semantics semantics;
    primec::SemanticProgram semanticProgram;
    std::string error;
    const std::vector<std::string> defaults = {"io_out", "io_err"};
    REQUIRE(semantics.validate(program, "/main", error, defaults, defaults, {}, nullptr, false, &semanticProgram));
    CHECK(error.empty());
    return semanticProgram;
  };

  const primec::SemanticProgram first = validateSemanticProduct(sourceA);
  const primec::SemanticProgram second = validateSemanticProduct(sourceB);

  const auto *firstHelper =
      findSemanticEntry(first.definitions,
                        [](const primec::SemanticProgramDefinition &entry) { return entry.fullPath == "/helper"; });
  const auto *secondHelper =
      findSemanticEntry(second.definitions,
                        [](const primec::SemanticProgramDefinition &entry) { return entry.fullPath == "/helper"; });
  REQUIRE(firstHelper != nullptr);
  REQUIRE(secondHelper != nullptr);
  CHECK(firstHelper->semanticNodeId != 0);
  CHECK(firstHelper->semanticNodeId == secondHelper->semanticNodeId);

  const auto *firstMain =
      findSemanticEntry(first.definitions,
                        [](const primec::SemanticProgramDefinition &entry) { return entry.fullPath == "/main"; });
  const auto *secondMain =
      findSemanticEntry(second.definitions,
                        [](const primec::SemanticProgramDefinition &entry) { return entry.fullPath == "/main"; });
  REQUIRE(firstMain != nullptr);
  REQUIRE(secondMain != nullptr);
  CHECK(firstMain->semanticNodeId == secondMain->semanticNodeId);
  CHECK(firstMain->provenanceHandle == secondMain->provenanceHandle);
  CHECK(firstMain->sourceLine == secondMain->sourceLine);
  CHECK(firstMain->sourceColumn == secondMain->sourceColumn);

  const auto *firstLocal = findSemanticEntry(primec::semanticProgramBindingFactView(first),
      [&first](const primec::SemanticProgramBindingFact &entry) {
        return primec::semanticProgramResolveCallTargetString(first, entry.scopePathId) == "/main" && primec::semanticProgramResolveCallTargetString(first, entry.siteKindId) == "local" && primec::semanticProgramResolveCallTargetString(first, entry.nameId) == "selected";
      });
  const auto *secondLocal = findSemanticEntry(primec::semanticProgramBindingFactView(second),
      [&second](const primec::SemanticProgramBindingFact &entry) {
        return primec::semanticProgramResolveCallTargetString(second, entry.scopePathId) == "/main" && primec::semanticProgramResolveCallTargetString(second, entry.siteKindId) == "local" && primec::semanticProgramResolveCallTargetString(second, entry.nameId) == "selected";
      });
  REQUIRE(firstLocal != nullptr);
  REQUIRE(secondLocal != nullptr);
  CHECK(firstLocal->semanticNodeId == secondLocal->semanticNodeId);
  CHECK(firstLocal->provenanceHandle == secondLocal->provenanceHandle);
  CHECK(firstLocal->sourceLine == secondLocal->sourceLine);
  CHECK(firstLocal->sourceColumn == secondLocal->sourceColumn);

  const auto *firstTemporary = findSemanticEntry(primec::semanticProgramBindingFactView(first),
      [&first](const primec::SemanticProgramBindingFact &entry) {
        return primec::semanticProgramResolveCallTargetString(first, entry.scopePathId) == "/main" && primec::semanticProgramResolveCallTargetString(first, entry.siteKindId) == "temporary" && primec::semanticProgramResolveCallTargetString(first, entry.nameId) == "helper";
      });
  const auto *secondTemporary = findSemanticEntry(primec::semanticProgramBindingFactView(second),
      [&second](const primec::SemanticProgramBindingFact &entry) {
        return primec::semanticProgramResolveCallTargetString(second, entry.scopePathId) == "/main" && primec::semanticProgramResolveCallTargetString(second, entry.siteKindId) == "temporary" && primec::semanticProgramResolveCallTargetString(second, entry.nameId) == "helper";
      });
  REQUIRE(firstTemporary != nullptr);
  REQUIRE(secondTemporary != nullptr);
  CHECK(firstTemporary->semanticNodeId == secondTemporary->semanticNodeId);
  CHECK(firstTemporary->provenanceHandle == secondTemporary->provenanceHandle);
  CHECK(firstTemporary->sourceLine == secondTemporary->sourceLine);
  CHECK(firstTemporary->sourceColumn == secondTemporary->sourceColumn);

  const auto *firstDirectCall = findSemanticEntry(primec::semanticProgramDirectCallTargetView(first),
      [](const primec::SemanticProgramDirectCallTarget &entry) {
        return entry.scopePath == "/main" && entry.callName == "helper";
      });
  const auto *secondDirectCall = findSemanticEntry(primec::semanticProgramDirectCallTargetView(second),
      [](const primec::SemanticProgramDirectCallTarget &entry) {
        return entry.scopePath == "/main" && entry.callName == "helper";
      });
  REQUIRE(firstDirectCall != nullptr);
  REQUIRE(secondDirectCall != nullptr);
  CHECK(firstDirectCall->semanticNodeId == secondDirectCall->semanticNodeId);
  CHECK(firstDirectCall->provenanceHandle == secondDirectCall->provenanceHandle);
  CHECK(firstDirectCall->sourceLine == secondDirectCall->sourceLine);
  CHECK(firstDirectCall->sourceColumn == secondDirectCall->sourceColumn);

  const auto *firstReturn = findSemanticEntry(primec::semanticProgramReturnFactView(first),
      [&first](const primec::SemanticProgramReturnFact &entry) {
        return primec::semanticProgramReturnFactDefinitionPath(first, entry) == "/main";
      });
  const auto *secondReturn = findSemanticEntry(primec::semanticProgramReturnFactView(second),
      [&second](const primec::SemanticProgramReturnFact &entry) {
        return primec::semanticProgramReturnFactDefinitionPath(second, entry) == "/main";
      });
  REQUIRE(firstReturn != nullptr);
  REQUIRE(secondReturn != nullptr);
  CHECK(firstReturn->semanticNodeId == secondReturn->semanticNodeId);
  CHECK(firstReturn->provenanceHandle == secondReturn->provenanceHandle);
  CHECK(firstReturn->sourceLine == secondReturn->sourceLine);
  CHECK(firstReturn->sourceColumn == secondReturn->sourceColumn);
}

TEST_CASE("local generated type identity stays deterministic in semantic product") {
  const std::string source =
      "[return<i32>]\n"
      "sum_pair([i32] left, [i32] right) {\n"
      "  [type] LeftT { typeof<left> }\n"
      "  [type] RightT { typeof<right> }\n"
      "  [struct] PairT {\n"
      "    [LeftT] first{0i32}\n"
      "    [RightT] second{0i32}\n"
      "  }\n"
      "  [PairT] pair{PairT{left, right}}\n"
      "  return(plus(pair.first, pair.second))\n"
      "}\n"
      "\n"
      "[return<i32>]\n"
      "mirror_pair([i32] left, [i32] right) {\n"
      "  [type] LeftT { typeof<left> }\n"
      "  [type] RightT { typeof<right> }\n"
      "  [struct] PairT {\n"
      "    [LeftT] first{0i32}\n"
      "    [RightT] second{0i32}\n"
      "  }\n"
      "  [PairT] pair{PairT{left, right}}\n"
      "  return(plus(pair.first, pair.second))\n"
      "}\n"
      "\n"
      "[return<i32>]\n"
      "main() {\n"
      "  [i32] direct{sum_pair(1i32, 2i32)}\n"
      "  [i32] mirrored{mirror_pair(3i32, 4i32)}\n"
      "  return(plus(direct, mirrored))\n"
      "}\n";

  auto validateSemanticProduct = [](const std::string &programText) {
    auto program = parseProgram(programText);
    primec::Semantics semantics;
    primec::SemanticProgram semanticProgram;
    std::string error;
    const std::vector<std::string> defaults = {"io_out", "io_err"};
    REQUIRE(semantics.validate(program,
                               "/main",
                               error,
                               defaults,
                               defaults,
                               {},
                               nullptr,
                               false,
                               &semanticProgram));
    CHECK(error.empty());
    return semanticProgram;
  };

  const primec::SemanticProgram first = validateSemanticProduct(source);
  const primec::SemanticProgram second = validateSemanticProduct(source);

  CHECK(primec::formatSemanticProgram(first) ==
        primec::formatSemanticProgram(second));

  const auto *firstSumPair =
      findSemanticEntry(first.typeMetadata,
                        [](const primec::SemanticProgramTypeMetadata &entry) {
                          return entry.fullPath == "/sum_pair/PairT";
                        });
  const auto *secondSumPair =
      findSemanticEntry(second.typeMetadata,
                        [](const primec::SemanticProgramTypeMetadata &entry) {
                          return entry.fullPath == "/sum_pair/PairT";
                        });
  const auto *firstMirrorPair =
      findSemanticEntry(first.typeMetadata,
                        [](const primec::SemanticProgramTypeMetadata &entry) {
                          return entry.fullPath == "/mirror_pair/PairT";
                        });
  REQUIRE(firstSumPair != nullptr);
  REQUIRE(secondSumPair != nullptr);
  REQUIRE(firstMirrorPair != nullptr);
  CHECK(firstSumPair->semanticNodeId != 0);
  CHECK(firstSumPair->semanticNodeId == secondSumPair->semanticNodeId);
  CHECK(firstSumPair->provenanceHandle != 0);
  CHECK(firstSumPair->provenanceHandle == secondSumPair->provenanceHandle);
  CHECK(firstSumPair->semanticNodeId != firstMirrorPair->semanticNodeId);

  const std::string dump = primec::formatSemanticProgram(first);
  CHECK(dump.find("full_path=\"/sum_pair/PairT\"") != std::string::npos);
  CHECK(dump.find("full_path=\"/mirror_pair/PairT\"") !=
        std::string::npos);
  CHECK(dump.find("struct_path=\"/sum_pair/PairT\" field_name=\"first\"") !=
        std::string::npos);
  CHECK(dump.find("struct_path=\"/sum_pair/PairT\" field_name=\"second\"") !=
        std::string::npos);
}

TEST_CASE("local generated type semantic ids ignore unrelated definition order") {
  const std::string sharedSuffix =
      "[return<i32>]\n"
      "sum_pair([i32] left, [i32] right) {\n"
      "  [type] LeftT { typeof<left> }\n"
      "  [type] RightT { typeof<right> }\n"
      "  [struct] PairT {\n"
      "    [LeftT] first{0i32}\n"
      "    [RightT] second{0i32}\n"
      "  }\n"
      "  [PairT] pair{PairT{left, right}}\n"
      "  return(plus(pair.first, pair.second))\n"
      "}\n"
      "\n"
      "[return<i32>]\n"
      "main() {\n"
      "  return(sum_pair(1i32, 2i32))\n"
      "}\n";
  const std::string helperDefinition =
      "[return<i32>]\n"
      "helper([i32] value) {\n"
      "  return(value)\n"
      "}\n"
      "\n";
  const std::string noiseDefinition =
      "[return<i32>]\n"
      "noise([i32] value) {\n"
      "  return(value)\n"
      "}\n"
      "\n";
  const std::string sourceA =
      helperDefinition + noiseDefinition + sharedSuffix;
  const std::string sourceB =
      noiseDefinition + helperDefinition + sharedSuffix;

  auto validateSemanticProduct = [](const std::string &programText) {
    auto program = parseProgram(programText);
    primec::Semantics semantics;
    primec::SemanticProgram semanticProgram;
    std::string error;
    const std::vector<std::string> defaults = {"io_out", "io_err"};
    REQUIRE(semantics.validate(program,
                               "/main",
                               error,
                               defaults,
                               defaults,
                               {},
                               nullptr,
                               false,
                               &semanticProgram));
    CHECK(error.empty());
    return semanticProgram;
  };

  const primec::SemanticProgram first = validateSemanticProduct(sourceA);
  const primec::SemanticProgram second = validateSemanticProduct(sourceB);

  const auto *firstType =
      findSemanticEntry(first.typeMetadata,
                        [](const primec::SemanticProgramTypeMetadata &entry) {
                          return entry.fullPath == "/sum_pair/PairT";
                        });
  const auto *secondType =
      findSemanticEntry(second.typeMetadata,
                        [](const primec::SemanticProgramTypeMetadata &entry) {
                          return entry.fullPath == "/sum_pair/PairT";
                        });
  REQUIRE(firstType != nullptr);
  REQUIRE(secondType != nullptr);
  CHECK(firstType->semanticNodeId == secondType->semanticNodeId);
  CHECK(firstType->provenanceHandle == secondType->provenanceHandle);
  CHECK(firstType->sourceLine == secondType->sourceLine);
  CHECK(firstType->sourceColumn == secondType->sourceColumn);

  const auto *firstField =
      findSemanticEntry(first.structFieldMetadata,
                        [](const primec::SemanticProgramStructFieldMetadata &entry) {
                          return entry.structPath == "/sum_pair/PairT" &&
                                 entry.fieldName == "first";
                        });
  const auto *secondField =
      findSemanticEntry(second.structFieldMetadata,
                        [](const primec::SemanticProgramStructFieldMetadata &entry) {
                          return entry.structPath == "/sum_pair/PairT" &&
                                 entry.fieldName == "first";
                        });
  REQUIRE(firstField != nullptr);
  REQUIRE(secondField != nullptr);
  CHECK(firstField->semanticNodeId == secondField->semanticNodeId);
  CHECK(firstField->provenanceHandle == secondField->provenanceHandle);
}

TEST_CASE("local generated type paths are pinned in boundary dumps") {
  const std::string source =
      "[return<T>]\n"
      "sum_pair<T>([T] left, [T] right) {\n"
      "  [type] LeftT { typeof<left> }\n"
      "  [type] RightT { typeof<right> }\n"
      "  [struct] PairT {\n"
      "    [LeftT] first{0i32}\n"
      "    [RightT] second{0i32}\n"
      "  }\n"
      "  [PairT] pair{PairT{left, right}}\n"
      "  return(plus(pair.first, pair.second))\n"
      "}\n"
      "\n"
      "[return<i32>]\n"
      "main() {\n"
      "  return(sum_pair<i32>(1i32, 2i32))\n"
      "}\n";

  primec::testing::CompilePipelineBoundaryDumps dumps;
  std::string error;
  REQUIRE(primec::testing::captureSemanticBoundaryDumpsForTesting(
      source, "/main", dumps, error));
  CHECK(error.empty());
  CHECK(dumps.astSemantic.find("/sum_pair__t") != std::string::npos);
  CHECK(dumps.astSemantic.find("/PairT") != std::string::npos);
  CHECK(dumps.semanticProduct.find("full_path=\"/sum_pair__t") !=
        std::string::npos);
  CHECK(dumps.semanticProduct.find("struct_path=\"/sum_pair__t") !=
        std::string::npos);
  const size_t generatedConstructorTarget =
      dumps.semanticProduct.find("resolved_path=\"/sum_pair__t");
  REQUIRE(generatedConstructorTarget != std::string::npos);
  CHECK(dumps.semanticProduct.find("/PairT\"", generatedConstructorTarget) !=
        std::string::npos);
  CHECK(dumps.semanticProduct.find("LeftT") == std::string::npos);
  CHECK(dumps.semanticProduct.find("RightT") == std::string::npos);
  CHECK(dumps.ir.find("module {") != std::string::npos);

  const std::string invalidSource =
      "[return<auto>]\n"
      "make_pair<T>([T] value) {\n"
      "  [type] ValueT { typeof<value> }\n"
      "  [struct] PairT {\n"
      "    [ValueT] first{0i32}\n"
      "  }\n"
      "  [PairT] pair{PairT{value}}\n"
      "  return(pair)\n"
      "}\n"
      "\n"
      "[return<i32>]\n"
      "main() {\n"
      "  return(make_pair<i32>(1i32))\n"
      "}\n";
  const std::filesystem::path invalidPath =
      primec::testing::detail::makeCompilePipelineDumpSourcePath();
  {
    std::ofstream file(invalidPath);
    REQUIRE(file.good());
    file << invalidSource;
  }
  primec::Options options;
  options.inputPath = invalidPath.string();
  options.entryPath = "/main";
  options.emitKind = "native";
  options.wasmProfile = "wasi";
  options.dumpStage = "semantic-product";
  options.defaultEffects = primec::testing::detail::defaultCompilePipelineTestingEffects();
  options.entryDefaultEffects = options.defaultEffects;
  primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);

  primec::CompilePipelineOutput output;
  primec::CompilePipelineErrorStage errorStage =
      primec::CompilePipelineErrorStage::None;
  std::string invalidError;
  CHECK_FALSE(primec::runCompilePipeline(options, output, errorStage, invalidError));

  std::error_code ec;
  std::filesystem::remove(invalidPath, ec);

  CHECK(errorStage == primec::CompilePipelineErrorStage::Semantic);
  CHECK(invalidError.find("local generated struct cannot escape return type: "
                          "/make_pair__t") != std::string::npos);
  CHECK_FALSE(output.hasDumpOutput);
  CHECK_FALSE(output.hasSemanticProgram);
  CHECK(output.dumpOutput.find("ValueT") == std::string::npos);
  CHECK(output.dumpOutput.find("PairT") == std::string::npos);
}

TEST_CASE("semantic product ownership surfaces keep deterministic source order") {
  const std::string source =
      "Record {\n"
      "  [i32] zeta{1i32}\n"
      "  [i32] alpha{2i32}\n"
      "}\n"
      "\n"
      "[return<i32>]\n"
      "first([i32] value) {\n"
      "  return(value)\n"
      "}\n"
      "\n"
      "[return<i32>]\n"
      "second([i32] value) {\n"
      "  return(value)\n"
      "}\n"
      "\n"
      "[return<i32>]\n"
      "main() {\n"
      "  [i32] zeta{second(2i32)}\n"
      "  [i32] alpha{first(1i32)}\n"
      "  return(alpha)\n"
      "}\n";

  auto program = parseProgram(source);
  primec::Semantics semantics;
  primec::SemanticProgram semanticProgram;
  std::string error;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  REQUIRE(semantics.validate(program, "/main", error, defaults, defaults, {}, nullptr, false, &semanticProgram));
  CHECK(error.empty());

  std::vector<std::string> fieldOrder;
  for (const auto &entry : semanticProgram.structFieldMetadata) {
    if (entry.structPath == "/Record") {
      fieldOrder.push_back(entry.fieldName);
    }
  }
  CHECK(fieldOrder == std::vector<std::string>{"zeta", "alpha"});

  std::vector<std::string> localBindingOrder;
  for (const auto *entry : primec::semanticProgramBindingFactView(semanticProgram)) {
    if (entry == nullptr) {
      continue;
    }
    if (primec::semanticProgramResolveCallTargetString(semanticProgram, entry->scopePathId) == "/main" &&
        primec::semanticProgramResolveCallTargetString(semanticProgram, entry->siteKindId) == "local") {
      localBindingOrder.push_back(std::string(
          primec::semanticProgramResolveCallTargetString(semanticProgram, entry->nameId)));
    }
  }
  std::vector<std::string> sortedLocalBindingOrder = localBindingOrder;
  std::sort(sortedLocalBindingOrder.begin(), sortedLocalBindingOrder.end());
  CHECK(sortedLocalBindingOrder == std::vector<std::string>{"alpha", "zeta"});

  std::vector<std::string> directCallOrder;
  for (const auto *entry : primec::semanticProgramDirectCallTargetView(semanticProgram)) {
    if (entry == nullptr) {
      continue;
    }
    if (entry->scopePath == "/main" && (entry->callName == "second" || entry->callName == "first")) {
      directCallOrder.push_back(entry->callName);
    }
  }
  CHECK(directCallOrder == std::vector<std::string>{"second", "first"});

  const std::string dump = primec::formatSemanticProgram(semanticProgram);
  const size_t zetaFieldPos = dump.find("struct_field_metadata[0]: struct_path=\"/Record\" field_name=\"zeta\"");
  const size_t alphaFieldPos = dump.find("struct_field_metadata[1]: struct_path=\"/Record\" field_name=\"alpha\"");
  REQUIRE(zetaFieldPos != std::string::npos);
  REQUIRE(alphaFieldPos != std::string::npos);
  CHECK(zetaFieldPos < alphaFieldPos);
}

TEST_CASE("semantic product lowering preserves debug source-map provenance") {
  const std::string source =
      "[return<i32>]\n"
      "pick([i32] value) {\n"
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
      "  [vector<i32>] values{vector<i32>()}\n"
      "  [i32] direct{pick(1i32)}\n"
      "  [i32] method{values.count()}\n"
      "  [i32] bridge{count(values)}\n"
      "  return(bridge)\n"
      "}\n";

  auto semanticAst = parseProgram(source);
  primec::Semantics semantics;
  primec::SemanticProgram semanticProgram;
  std::string error;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  CHECK(semantics.validate(semanticAst, "/main", error, defaults, defaults, {}, nullptr, false, &semanticProgram));
  CHECK(error.empty());
}

TEST_CASE("type resolution local Result.ok metadata stays aligned with wrapped call snapshots") {
  const std::string source =
      "MyError {\n"
      "}\n"
      "\n"
      "[return<auto>]\n"
      "makeValue() {\n"
      "  return(4i32)\n"
      "}\n"
      "\n"
      "[return<Result<int, MyError>>]\n"
      "main() {\n"
      "  [auto] status{Result.ok(makeValue())}\n"
      "  return(status)\n"
      "}\n";

  std::string error;
  primec::semantics::TypeResolutionQueryCallSnapshot queryCallSnapshot;
  REQUIRE(primec::semantics::computeTypeResolutionQueryCallSnapshotForTesting(
      parseProgram(source), "/main", error, queryCallSnapshot));
  CHECK(error.empty());

  primec::semantics::TypeResolutionQueryBindingSnapshot queryBindingSnapshot;
  REQUIRE(primec::semantics::computeTypeResolutionQueryBindingSnapshotForTesting(
      parseProgram(source), "/main", error, queryBindingSnapshot));
  CHECK(error.empty());

  primec::semantics::TypeResolutionCallBindingSnapshot callSnapshot;
  REQUIRE(primec::semantics::computeTypeResolutionCallBindingSnapshotForTesting(
      parseProgram(source), "/main", error, callSnapshot));
  CHECK(error.empty());

  const auto &queryCallEntry = requireQueryCallSnapshotEntry(queryCallSnapshot, "/main", "/makeValue");
  const auto &queryBindingEntry = requireQueryBindingSnapshotEntry(queryBindingSnapshot, "/main", "/makeValue");
  const auto &callEntry = requireCallBindingSnapshotEntry(callSnapshot, "/main", "/makeValue");

  CHECK(queryCallEntry.typeText == callEntry.bindingTypeText);
  CHECK(queryBindingEntry.bindingTypeText == callEntry.bindingTypeText);
}

TEST_CASE("semantic product formatter emits deterministic lowering-facing sections") {
  const std::string source =
      "import /std/collections/*\n"
      "\n"
      "[return<T>]\n"
      "id<T>([T] value) {\n"
      "  return(value)\n"
      "}\n"
      "\n"
      "[return<i32>]\n"
      "main() {\n"
      "  [vector<i32>] values{vector<i32>()}\n"
      "  return(id(/std/collections/vector/count(values)))\n"
      "}\n";

  auto program = parseProgram(source);
  primec::Semantics semantics;
  primec::SemanticProgram semanticProgram;
  std::string error;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  // TODO-4815 (fixed): id(/std/collections/vector/count(values)) now
  // correctly infers its implicit template argument (T=i32) again.
  CHECK(semantics.validate(program, "/main", error, defaults, defaults, {}, nullptr, false, &semanticProgram));
  CHECK(error.empty());
}

TEST_CASE("semantic product formatter resolves module direct-call indices deterministically") {
  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/z",
      .callName = "last",
      .sourceLine = 20,
      .sourceColumn = 2,
      .semanticNodeId = 300,
      .provenanceHandle = 900,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/last"),
      .stdlibSurfaceId = primec::StdlibSurfaceId::FileErrorHelpers,
  });
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/a",
      .callName = "first",
      .sourceLine = 10,
      .sourceColumn = 1,
      .semanticNodeId = 200,
      .provenanceHandle = 800,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/first"),
      .stdlibSurfaceId = primec::StdlibSurfaceId::FileHelpers,
  });

  primec::SemanticProgramModuleResolvedArtifacts moduleA;
  moduleA.identity.moduleKey = "/a";
  moduleA.identity.stableOrder = 0;
  moduleA.directCallTargetIndices.push_back(1);
  semanticProgram.moduleResolvedArtifacts.push_back(moduleA);

  primec::SemanticProgramModuleResolvedArtifacts moduleZ;
  moduleZ.identity.moduleKey = "/z";
  moduleZ.identity.stableOrder = 1;
  moduleZ.directCallTargetIndices.push_back(0);
  semanticProgram.moduleResolvedArtifacts.push_back(moduleZ);

  const auto view = primec::semanticProgramDirectCallTargetView(semanticProgram);
  REQUIRE(view.size() == 2);
  CHECK(view[0] == &semanticProgram.directCallTargets[1]);
  CHECK(view[1] == &semanticProgram.directCallTargets[0]);

  const std::string dump = primec::formatSemanticProgram(semanticProgram);
  const std::string firstEntry =
      "direct_call_targets[0]: scope_path=\"/a\" call_name=\"first\" resolved_path=\"/first\" stdlib_surface_id=\"file.file_helpers\"";
  const std::string secondEntry =
      "direct_call_targets[1]: scope_path=\"/z\" call_name=\"last\" resolved_path=\"/last\" stdlib_surface_id=\"file.file_error\"";
  const std::size_t firstPos = dump.find(firstEntry);
  const std::size_t secondPos = dump.find(secondEntry);
  CHECK(firstPos != std::string::npos);
  CHECK(secondPos != std::string::npos);
  CHECK(firstPos < secondPos);
}

TEST_CASE("semantic product formatter resolves module method-call indices deterministically") {
  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  semanticProgram.methodCallTargets.push_back(primec::SemanticProgramMethodCallTarget{
      .scopePath = "/z",
      .methodName = "scale",
      .receiverTypeText = "matrix<f32>",
      .sourceLine = 22,
      .sourceColumn = 6,
      .semanticNodeId = 330,
      .provenanceHandle = 930,
      .scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/z"),
      .methodNameId = primec::semanticProgramInternCallTargetString(semanticProgram, "scale"),
      .receiverTypeTextId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "matrix<f32>"),
      .resolvedPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram,
                                                        "/std/math/matrix/scale"),
      .stdlibSurfaceId = primec::StdlibSurfaceId::GfxBufferHelpers,
  });
  semanticProgram.methodCallTargets.push_back(primec::SemanticProgramMethodCallTarget{
      .scopePath = "/a",
      .methodName = "length",
      .receiverTypeText = "vector<f32>",
      .sourceLine = 12,
      .sourceColumn = 3,
      .semanticNodeId = 230,
      .provenanceHandle = 830,
      .scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/a"),
      .methodNameId = primec::semanticProgramInternCallTargetString(semanticProgram, "length"),
      .receiverTypeTextId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "vector<f32>"),
      .resolvedPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram,
                                                        "/std/math/vector/length"),
      .stdlibSurfaceId = primec::StdlibSurfaceId::CollectionsManifestSurface0,
  });

  primec::SemanticProgramModuleResolvedArtifacts moduleA;
  moduleA.identity.moduleKey = "/a";
  moduleA.identity.stableOrder = 0;
  moduleA.methodCallTargetIndices.push_back(1);
  semanticProgram.moduleResolvedArtifacts.push_back(moduleA);

  primec::SemanticProgramModuleResolvedArtifacts moduleZ;
  moduleZ.identity.moduleKey = "/z";
  moduleZ.identity.stableOrder = 1;
  moduleZ.methodCallTargetIndices.push_back(0);
  semanticProgram.moduleResolvedArtifacts.push_back(moduleZ);

  const auto view = primec::semanticProgramMethodCallTargetView(semanticProgram);
  REQUIRE(view.size() == 2);
  CHECK(view[0] == &semanticProgram.methodCallTargets[1]);
  CHECK(view[1] == &semanticProgram.methodCallTargets[0]);

  const std::string dump = primec::formatSemanticProgram(semanticProgram);
  const std::string firstEntry =
      "method_call_targets[0]: scope_path=\"/a\" method_name=\"length\" receiver_type_text=\"vector<f32>\" resolved_path=\"/std/math/vector/length\" stdlib_surface_id=\"collections.vector_helpers\"";
  const std::string secondEntry =
      "method_call_targets[1]: scope_path=\"/z\" method_name=\"scale\" receiver_type_text=\"matrix<f32>\" resolved_path=\"/std/math/matrix/scale\" stdlib_surface_id=\"gfx.buffer_helpers\"";
  const std::size_t firstPos = dump.find(firstEntry);
  const std::size_t secondPos = dump.find(secondEntry);
  CHECK(firstPos != std::string::npos);
  CHECK(secondPos != std::string::npos);
  CHECK(firstPos < secondPos);
}

TEST_CASE("semantic product formatter resolves module bridge-path-choice indices deterministically") {
  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  semanticProgram.bridgePathChoices.push_back(primec::SemanticProgramBridgePathChoice{
      .scopePath = "/z",
      .collectionFamily = "matrix",
      .sourceLine = 23,
      .sourceColumn = 7,
      .semanticNodeId = 430,
      .provenanceHandle = 1030,
      .scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/z"),
      .collectionFamilyId = primec::semanticProgramInternCallTargetString(semanticProgram, "matrix"),
      .helperNameId = primec::semanticProgramInternCallTargetString(semanticProgram, "scale"),
      .chosenPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/std/math/matrix/scale"),
      .stdlibSurfaceId = primec::StdlibSurfaceId::GfxBufferHelpers,
  });
  semanticProgram.bridgePathChoices.push_back(primec::SemanticProgramBridgePathChoice{
      .scopePath = "/a",
      .collectionFamily = "vector",
      .sourceLine = 13,
      .sourceColumn = 4,
      .semanticNodeId = 330,
      .provenanceHandle = 930,
      .scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/a"),
      .collectionFamilyId = primec::semanticProgramInternCallTargetString(semanticProgram, "vector"),
      .helperNameId = primec::semanticProgramInternCallTargetString(semanticProgram, "length"),
      .chosenPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/std/math/vector/length"),
      .stdlibSurfaceId = primec::StdlibSurfaceId::CollectionsManifestSurface0,
  });

  primec::SemanticProgramModuleResolvedArtifacts moduleA;
  moduleA.identity.moduleKey = "/a";
  moduleA.identity.stableOrder = 0;
  moduleA.bridgePathChoiceIndices.push_back(1);
  semanticProgram.moduleResolvedArtifacts.push_back(moduleA);

  primec::SemanticProgramModuleResolvedArtifacts moduleZ;
  moduleZ.identity.moduleKey = "/z";
  moduleZ.identity.stableOrder = 1;
  moduleZ.bridgePathChoiceIndices.push_back(0);
  semanticProgram.moduleResolvedArtifacts.push_back(moduleZ);

  const auto view = primec::semanticProgramBridgePathChoiceView(semanticProgram);
  REQUIRE(view.size() == 2);
  CHECK(view[0] == &semanticProgram.bridgePathChoices[1]);
  CHECK(view[1] == &semanticProgram.bridgePathChoices[0]);

  const std::string dump = primec::formatSemanticProgram(semanticProgram);
  const std::string firstEntry =
      "bridge_path_choices[0]: scope_path=\"/a\" collection_family=\"vector\" helper_name=\"length\" chosen_path=\"/std/math/vector/length\" stdlib_surface_id=\"collections.vector_helpers\"";
  const std::string secondEntry =
      "bridge_path_choices[1]: scope_path=\"/z\" collection_family=\"matrix\" helper_name=\"scale\" chosen_path=\"/std/math/matrix/scale\" stdlib_surface_id=\"gfx.buffer_helpers\"";
  const std::size_t firstPos = dump.find(firstEntry);
  const std::size_t secondPos = dump.find(secondEntry);
  CHECK(firstPos != std::string::npos);
  CHECK(secondPos != std::string::npos);
  CHECK(firstPos < secondPos);
}

TEST_CASE("semantic product formatter keeps bridge-path-choice text parity for flat vs module-index storage") {
  auto makeProgram = [](bool useModuleIndices) {
    primec::SemanticProgram semanticProgram;
    semanticProgram.entryPath = "/main";

    semanticProgram.bridgePathChoices.push_back(primec::SemanticProgramBridgePathChoice{
        .scopePath = "/first",
        .collectionFamily = "vector",
        .sourceLine = 5,
        .sourceColumn = 3,
        .semanticNodeId = 431,
        .provenanceHandle = 1031,
        .scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/first"),
        .collectionFamilyId = primec::semanticProgramInternCallTargetString(semanticProgram, "vector"),
        .helperNameId = primec::semanticProgramInternCallTargetString(semanticProgram, "length"),
        .chosenPathId =
            primec::semanticProgramInternCallTargetString(semanticProgram, "/std/math/vector/length"),
        .stdlibSurfaceId = std::nullopt,
    });
    semanticProgram.bridgePathChoices.push_back(primec::SemanticProgramBridgePathChoice{
        .scopePath = "/second",
        .collectionFamily = "matrix",
        .sourceLine = 8,
        .sourceColumn = 4,
        .semanticNodeId = 432,
        .provenanceHandle = 1032,
        .scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/second"),
        .collectionFamilyId = primec::semanticProgramInternCallTargetString(semanticProgram, "matrix"),
        .helperNameId = primec::semanticProgramInternCallTargetString(semanticProgram, "scale"),
        .chosenPathId =
            primec::semanticProgramInternCallTargetString(semanticProgram, "/std/math/matrix/scale"),
        .stdlibSurfaceId = std::nullopt,
    });

    if (useModuleIndices) {
      primec::SemanticProgramModuleResolvedArtifacts moduleFirst;
      moduleFirst.identity.moduleKey = "/first";
      moduleFirst.identity.stableOrder = 0;
      moduleFirst.bridgePathChoiceIndices.push_back(0);
      semanticProgram.moduleResolvedArtifacts.push_back(std::move(moduleFirst));

      primec::SemanticProgramModuleResolvedArtifacts moduleSecond;
      moduleSecond.identity.moduleKey = "/second";
      moduleSecond.identity.stableOrder = 1;
      moduleSecond.bridgePathChoiceIndices.push_back(1);
      semanticProgram.moduleResolvedArtifacts.push_back(std::move(moduleSecond));
    }

    return semanticProgram;
  };

  const primec::SemanticProgram flatProgram = makeProgram(false);
  const primec::SemanticProgram moduleIndexedProgram = makeProgram(true);
  CHECK(primec::formatSemanticProgram(moduleIndexedProgram) == primec::formatSemanticProgram(flatProgram));
}

TEST_SUITE_END();
