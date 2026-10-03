#include "third_party/doctest.h"

#include "test_semantics_type_resolution_graph_snapshots_shared.h"

TEST_SUITE_BEGIN("primestruct.semantics.type_resolution_graph");

TEST_CASE("semantic product keeps vector and map bridge parity") {
  const std::string source = R"(
import /std/collections/vector
import /std/collections/map

[return<i32>]
/std/collections/vector/count<T>([vector<T>] values) {
  return(1i32)
}

[return<i32>]
/std/collections/map/count<K, V>([map<K, V>] values) {
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
