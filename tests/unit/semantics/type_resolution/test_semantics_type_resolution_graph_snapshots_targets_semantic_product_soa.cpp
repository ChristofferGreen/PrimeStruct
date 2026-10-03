#include "third_party/doctest.h"

#include "test_semantics_type_resolution_graph_snapshots_shared.h"

TEST_SUITE_BEGIN("primestruct.semantics.type_resolution_graph");

TEST_CASE("implicit template-arg graph facts are consumed by inference cache") {
  const std::string source =
      "[return<T>]\n"
      "id<T>([T] value) {\n"
      "  return(value)\n"
      "}\n"
      "\n"
      "[return<i32>]\n"
      "main() {\n"
      "  [auto] left{id(1i32)}\n"
      "  [auto] right{id(2i32)}\n"
      "  return(plus(left, right))\n"
      "}\n";

  std::string error;
  primec::semantics::ImplicitTemplateArgFactConsumptionMetricsForTesting metrics;
  REQUIRE(primec::semantics::collectImplicitTemplateArgFactConsumptionMetricsForTesting(
      parseProgram(source), "/main", error, metrics));
  CHECK(error.empty());
  CHECK(metrics.hitCount > 0u);
}

TEST_CASE("type resolution graph snapshot records timing metrics") {
  const std::string source = R"(
Pair {
  left{i32}
  right{i64}
}

[return<i32>]
main() {
  [auto] data{Pair(1i32, 2i64)}
  return(data.left)
}
)";

  std::string error;
  primec::semantics::TypeResolutionGraphSnapshot snapshot;
  REQUIRE(primec::semantics::buildTypeResolutionGraphForTesting(
      parseProgram(source), "/main", error, snapshot));
  CHECK(error.empty());

  CHECK(snapshot.nodeCount == snapshot.nodes.size());
  CHECK(snapshot.edgeCount == snapshot.edges.size());
  CHECK(snapshot.nodeCount > 0u);
  CHECK(snapshot.sccCount > 0u);
  if (snapshot.prepareMaxMillis != 0u) {
    CHECK(snapshot.prepareMaxMillis >= snapshot.prepareMillis);
  }
  if (snapshot.buildMaxMillis != 0u) {
    CHECK(snapshot.buildMaxMillis >= snapshot.buildMillis);
  }
}

TEST_CASE("type resolution local query metadata stays aligned with query snapshots") {
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

  std::string error;
  primec::semantics::TypeResolutionLocalBindingSnapshot localSnapshot;
  REQUIRE(primec::semantics::computeTypeResolutionLocalBindingSnapshotForTesting(
      parseProgram(source), "/main", error, localSnapshot));
  CHECK(error.empty());

  primec::semantics::TypeResolutionQueryCallSnapshot queryCallSnapshot;
  REQUIRE(primec::semantics::computeTypeResolutionQueryCallSnapshotForTesting(
      parseProgram(source), "/main", error, queryCallSnapshot));
  CHECK(error.empty());

  primec::semantics::TypeResolutionQueryBindingSnapshot queryBindingSnapshot;
  REQUIRE(primec::semantics::computeTypeResolutionQueryBindingSnapshotForTesting(
      parseProgram(source), "/main", error, queryBindingSnapshot));
  CHECK(error.empty());

  primec::semantics::TypeResolutionQueryResultTypeSnapshot queryResultSnapshot;
  REQUIRE(primec::semantics::computeTypeResolutionQueryResultTypeSnapshotForTesting(
      parseProgram(source), "/main", error, queryResultSnapshot));
  CHECK(error.empty());

  const auto &localEntry = requireLocalBindingSnapshotEntry(localSnapshot, "/main", "selected");
  const auto &callEntry = requireQueryCallSnapshotEntry(queryCallSnapshot, "/main", "/lookup");
  const auto &bindingEntry = requireQueryBindingSnapshotEntry(queryBindingSnapshot, "/main", "/lookup");
  const auto &resultEntry = requireQueryResultTypeSnapshotEntry(queryResultSnapshot, "/main", "/lookup");

  CHECK(localEntry.initializerResolvedPath == callEntry.resolvedPath);
  CHECK(localEntry.initializerBindingTypeText == bindingEntry.bindingTypeText);
  CHECK(localEntry.initializerQueryTypeText == callEntry.typeText);
  CHECK(localEntry.initializerReceiverBindingTypeText.empty());
  CHECK(localEntry.initializerResultHasValue == resultEntry.hasValue);
  CHECK(localEntry.initializerResultValueTypeText == resultEntry.valueTypeText);
  CHECK(localEntry.initializerResultErrorTypeText == resultEntry.errorTypeText);
}

TEST_CASE("type resolution local call metadata stays aligned with call snapshot") {
  const std::string source =
      "[return<T>]\n"
      "id<T>([T] value) {\n"
      "  return(value)\n"
      "}\n"
      "\n"
      "[return<i32>]\n"
      "main() {\n"
      "  [auto] selected{id(1i32)}\n"
      "  return(selected)\n"
      "}\n";

  std::string error;
  primec::semantics::TypeResolutionLocalBindingSnapshot localSnapshot;
  REQUIRE(primec::semantics::computeTypeResolutionLocalBindingSnapshotForTesting(
      parseProgram(source), "/main", error, localSnapshot));
  CHECK(error.empty());

  primec::semantics::TypeResolutionCallBindingSnapshot callSnapshot;
  REQUIRE(primec::semantics::computeTypeResolutionCallBindingSnapshotForTesting(
      parseProgram(source), "/main", error, callSnapshot));
  CHECK(error.empty());

  const auto &localEntry = requireLocalBindingSnapshotEntry(localSnapshot, "/main", "selected");
  const auto &callEntry = requireCallBindingSnapshotEntry(callSnapshot, "/main", "/id");
  CHECK(localEntry.initializerResolvedPath == callEntry.resolvedPath);
  CHECK(localEntry.initializerBindingTypeText == callEntry.bindingTypeText);
  CHECK(localEntry.initializerReceiverBindingTypeText.empty());
  CHECK(localEntry.initializerQueryTypeText == callEntry.bindingTypeText);
  CHECK(!localEntry.initializerResultHasValue);
}

TEST_CASE("type resolution query binding metadata stays aligned with call snapshot") {
  const std::string source =
      "[return<T>]\n"
      "id<T>([T] value) {\n"
      "  return(value)\n"
      "}\n"
      "\n"
      "[return<i32>]\n"
      "main() {\n"
      "  [auto] selected{id(1i32)}\n"
      "  return(selected)\n"
      "}\n";

  std::string error;
  primec::semantics::TypeResolutionQueryBindingSnapshot queryBindingSnapshot;
  REQUIRE(primec::semantics::computeTypeResolutionQueryBindingSnapshotForTesting(
      parseProgram(source), "/main", error, queryBindingSnapshot));
  CHECK(error.empty());

  primec::semantics::TypeResolutionCallBindingSnapshot callSnapshot;
  REQUIRE(primec::semantics::computeTypeResolutionCallBindingSnapshotForTesting(
      parseProgram(source), "/main", error, callSnapshot));
  CHECK(error.empty());

  const auto &queryEntry =
      requireQueryBindingSnapshotEntry(queryBindingSnapshot, "/main", "/id");
  const auto &callEntry = requireCallBindingSnapshotEntry(callSnapshot, "/main", "/id");
  CHECK(queryEntry.resolvedPath == callEntry.resolvedPath);
  CHECK(queryEntry.bindingTypeText == callEntry.bindingTypeText);
}

TEST_CASE("type resolution query call metadata stays aligned with call snapshot") {
  const std::string source =
      "[return<T>]\n"
      "id<T>([T] value) {\n"
      "  return(value)\n"
      "}\n"
      "\n"
      "[return<i32>]\n"
      "main() {\n"
      "  [auto] selected{id(1i32)}\n"
      "  return(selected)\n"
      "}\n";

  std::string error;
  primec::semantics::TypeResolutionQueryCallSnapshot queryCallSnapshot;
  REQUIRE(primec::semantics::computeTypeResolutionQueryCallSnapshotForTesting(
      parseProgram(source), "/main", error, queryCallSnapshot));
  CHECK(error.empty());

  primec::semantics::TypeResolutionCallBindingSnapshot callSnapshot;
  REQUIRE(primec::semantics::computeTypeResolutionCallBindingSnapshotForTesting(
      parseProgram(source), "/main", error, callSnapshot));
  CHECK(error.empty());

  const auto &queryEntry =
      requireQueryCallSnapshotEntry(queryCallSnapshot, "/main", "/id");
  const auto &callEntry = requireCallBindingSnapshotEntry(callSnapshot, "/main", "/id");
  CHECK(queryEntry.resolvedPath == callEntry.resolvedPath);
  CHECK(queryEntry.typeText == callEntry.bindingTypeText);
}

TEST_CASE("semantic product publishes resolved direct-call targets") {
  const std::string source =
      "[return<T>]\n"
      "id<T>([T] value) {\n"
      "  return(value)\n"
      "}\n"
      "\n"
      "[return<i32>]\n"
      "main() {\n"
      "  [auto] selected{id(1i32)}\n"
      "  return(selected)\n"
      "}\n";

  auto program = parseProgram(source);
  primec::Semantics semantics;
  primec::SemanticProgram semanticProgram;
  std::string error;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  REQUIRE(semantics.validate(program, "/main", error, defaults, defaults, {}, nullptr, false, &semanticProgram));
  CHECK(error.empty());

  const auto *targetEntry = findSemanticEntry(
      primec::semanticProgramDirectCallTargetView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramDirectCallTarget &entry) {
        // resolvedPath is deliberately canonicalized (specialization suffix
        // stripped) even in published semantic-product facts; callName is
        // the one that retains the specialized "/id__t<hash>" spelling.
        return entry.scopePath == "/main" &&
               (entry.callName == "id" || entry.callName.rfind("/id__t", 0) == 0) &&
               resolveDirectCallPath(semanticProgram, entry) == "/id";
      });
  REQUIRE(targetEntry != nullptr);
  CHECK(targetEntry->provenanceHandle != 0);
  CHECK(targetEntry->sourceLine > 0);
  CHECK(targetEntry->sourceColumn > 0);
}

TEST_CASE("semantic product publishes resolved direct-call targets for local binding reads") {
  const std::string source =
      "[return<i32>]\n"
      "main() {\n"
      "  [i32 mut] value{5i32}\n"
      "  assign(value, 6i32)\n"
      "  return(value)\n"
      "}\n";

  auto program = parseProgram(source);
  primec::Semantics semantics;
  primec::SemanticProgram semanticProgram;
  std::string error;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  REQUIRE(semantics.validate(program, "/main", error, defaults, defaults, {}, nullptr, false, &semanticProgram));
  CHECK(error.empty());

  const auto *targetEntry = findSemanticEntry(
      primec::semanticProgramDirectCallTargetView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramDirectCallTarget &entry) {
        return entry.scopePath == "/main" &&
               entry.callName == "value" &&
               !resolveDirectCallPath(semanticProgram, entry).empty();
      });
  REQUIRE(targetEntry != nullptr);
  CHECK(targetEntry->provenanceHandle != 0);
  CHECK(targetEntry->sourceLine > 0);
  CHECK(targetEntry->sourceColumn > 0);
}

TEST_CASE("semantic product publishes stdlib surface ids for direct, method, and bridge routing") {
  const std::string source = R"(
import /std/collections/vector

[return<i32>]
/std/collections/vector/count([vector<i32>] self) {
  return(17i32)
}

[effects(heap_alloc), return<i32>]
main() {
  [vector<i32>] values{vector<i32>(1i32)}
  [auto] directCount{/std/collections/vector/count(values)}
  [auto] methodCount{values./std/collections/vector/count()}
  [i32] viaBridge{count(values)}
  return(viaBridge)
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

  const auto *directEntry = findSemanticEntry(
      primec::semanticProgramDirectCallTargetView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramDirectCallTarget &entry) {
        return entry.scopePath == "/main" &&
               entry.callName == "/std/collections/vector/count" &&
               primec::semanticProgramDirectCallTargetResolvedPath(semanticProgram, entry) ==
                   "/std/collections/vector/count";
      });
  REQUIRE(directEntry != nullptr);
  REQUIRE(directEntry->stdlibSurfaceId.has_value());
  CHECK(*directEntry->stdlibSurfaceId == primec::StdlibSurfaceId::CollectionsManifestSurface0);
  const auto directSurfaceId = primec::semanticProgramLookupPublishedDirectCallTargetStdlibSurfaceId(
      semanticProgram, directEntry->semanticNodeId);
  REQUIRE(directSurfaceId.has_value());
  CHECK(*directSurfaceId == primec::StdlibSurfaceId::CollectionsManifestSurface0);

  const auto *methodEntry = findSemanticEntry(
      primec::semanticProgramMethodCallTargetView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramMethodCallTarget &entry) {
        return entry.scopePath == "/main" &&
               entry.methodName == "/std/collections/vector/count" &&
               primec::semanticProgramMethodCallTargetResolvedPath(semanticProgram, entry) ==
                   "/std/collections/vector/count";
      });
  REQUIRE(methodEntry != nullptr);
  REQUIRE(methodEntry->stdlibSurfaceId.has_value());
  CHECK(*methodEntry->stdlibSurfaceId == primec::StdlibSurfaceId::CollectionsManifestSurface0);
  const auto methodSurfaceId = primec::semanticProgramLookupPublishedMethodCallTargetStdlibSurfaceId(
      semanticProgram, methodEntry->semanticNodeId);
  REQUIRE(methodSurfaceId.has_value());
  CHECK(*methodSurfaceId == primec::StdlibSurfaceId::CollectionsManifestSurface0);

  const auto *bridgeEntry = findSemanticEntry(
      primec::semanticProgramBridgePathChoiceView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramBridgePathChoice &entry) {
        return entry.scopePath == "/main" &&
               primec::semanticProgramBridgePathChoiceHelperName(semanticProgram, entry) == "count" &&
               primec::semanticProgramResolveCallTargetString(semanticProgram, entry.chosenPathId) ==
                   "/std/collections/vector/count";
      });
  REQUIRE(bridgeEntry != nullptr);
  REQUIRE(bridgeEntry->stdlibSurfaceId.has_value());
  CHECK(*bridgeEntry->stdlibSurfaceId == primec::StdlibSurfaceId::CollectionsManifestSurface0);
  const auto bridgeSurfaceId =
      primec::semanticProgramLookupPublishedBridgePathChoiceStdlibSurfaceId(
          semanticProgram, bridgeEntry->semanticNodeId);
  REQUIRE(bridgeSurfaceId.has_value());
  CHECK(*bridgeSurfaceId == primec::StdlibSurfaceId::CollectionsManifestSurface0);
}

TEST_CASE("semantic product normalizes experimental vector bridge helper aliases") {
  const std::string source = R"(
import /std/collections/vector/*

[effects(heap_alloc), return<i32>]
main() {
  [vector<i32>] values{vector<i32>(1i32)}
  return(/std/collections/vector/count<i32>(values))
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

  // TODO-4052/4053/4054 folded /std/collections/experimental_vector/* into
  // the canonical /std/collections/vector/* namespace (see
  // docs/todo_finished.md); the canonical helper is now self-contained and
  // no longer bridges to a separate experimental_vector spelling.
  const auto *directEntry = findSemanticEntry(
      primec::semanticProgramDirectCallTargetView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramDirectCallTarget &entry) {
        return entry.scopePath == "/main" && entry.callName == "/std/collections/vector/count" &&
               primec::semanticProgramDirectCallTargetResolvedPath(semanticProgram, entry) ==
                   "/std/collections/vector/count";
      });
  REQUIRE(directEntry != nullptr);
  REQUIRE(directEntry->stdlibSurfaceId.has_value());
  CHECK(*directEntry->stdlibSurfaceId == primec::StdlibSurfaceId::CollectionsManifestSurface0);

  const auto *bridgeEntry = findSemanticEntry(
      primec::semanticProgramBridgePathChoiceView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramBridgePathChoice &entry) {
        return entry.scopePath == "/main" && entry.collectionFamily == "vector" &&
               primec::semanticProgramBridgePathChoiceHelperName(semanticProgram, entry) == "count" &&
               primec::semanticProgramResolveCallTargetString(semanticProgram, entry.chosenPathId) ==
                   "/std/collections/vector/count";
      });
  REQUIRE(bridgeEntry != nullptr);
  REQUIRE(bridgeEntry->stdlibSurfaceId.has_value());
  CHECK(*bridgeEntry->stdlibSurfaceId == primec::StdlibSurfaceId::CollectionsManifestSurface0);

  const auto bridgeSurfaceId =
      primec::semanticProgramLookupPublishedBridgePathChoiceStdlibSurfaceId(
          semanticProgram, bridgeEntry->semanticNodeId);
  REQUIRE(bridgeSurfaceId.has_value());
  CHECK(*bridgeSurfaceId == primec::StdlibSurfaceId::CollectionsManifestSurface0);
}

TEST_CASE("semantic product publishes soa bridge choices for canonical and experimental helpers") {
  const std::string source = R"(
[struct reflect]
Particle() {
  [i32] x{1i32}
}

[return<void>]
/std/collections/soa/push<T>([soa<T> mut] values, [T] value) {
}

[return<int>]
/std/collections/soa/count<T>([soa<T>] values) {
  return(1i32)
}

[effects(heap_alloc), return<int>]
main() {
  [soa<Particle> mut] values{soa<Particle>()}
  /std/collections/soa/push<Particle>(values, Particle(7i32))
  return(/std/collections/soa/count<Particle>(values))
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

  // Bridge-path-choice facts label the SOA family with the compiler's
  // internal collection type name ("soa_vector", see
  // internalSoaCollectionTypeName() in SemanticsBuiltinPathHelpers.cpp),
  // distinct from the "soa" spelling used by CollectionSpecialization facts.
  const auto *pushBridgeEntry = findSemanticEntry(
      primec::semanticProgramBridgePathChoiceView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramBridgePathChoice &entry) {
        return entry.scopePath == "/main" && entry.collectionFamily == "soa_vector" &&
               primec::semanticProgramBridgePathChoiceHelperName(semanticProgram, entry) == "push" &&
               primec::semanticProgramResolveCallTargetString(semanticProgram, entry.chosenPathId)
                       .find("/std/collections/soa/push") == 0;
      });
  REQUIRE(pushBridgeEntry != nullptr);
  REQUIRE(pushBridgeEntry->stdlibSurfaceId.has_value());
  CHECK(*pushBridgeEntry->stdlibSurfaceId ==
        primec::StdlibSurfaceId::CollectionsColumnarHelpers);

  const auto *countBridgeEntry = findSemanticEntry(
      primec::semanticProgramBridgePathChoiceView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramBridgePathChoice &entry) {
        return entry.scopePath == "/main" && entry.collectionFamily == "soa_vector" &&
               primec::semanticProgramBridgePathChoiceHelperName(semanticProgram, entry) == "count" &&
               primec::semanticProgramResolveCallTargetString(semanticProgram, entry.chosenPathId)
                       .find("/std/collections/soa/count") == 0;
      });
  REQUIRE(countBridgeEntry != nullptr);
  REQUIRE(countBridgeEntry->stdlibSurfaceId.has_value());
  CHECK(*countBridgeEntry->stdlibSurfaceId ==
        primec::StdlibSurfaceId::CollectionsColumnarHelpers);
  const auto countBridgeSurfaceId =
      primec::semanticProgramLookupPublishedBridgePathChoiceStdlibSurfaceId(
          semanticProgram, countBridgeEntry->semanticNodeId);
  REQUIRE(countBridgeSurfaceId.has_value());
  CHECK(*countBridgeSurfaceId == primec::StdlibSurfaceId::CollectionsColumnarHelpers);
}

TEST_CASE("semantic product method-call targets stay separated by receiver type") {
  const std::string source =
      "[struct]\n"
      "A() {\n"
      "  [i32] x\n"
      "}\n"
      "\n"
      "[struct]\n"
      "B() {\n"
      "  [i32] y\n"
      "}\n"
      "\n"
      "[return<i32>]\n"
      "/A/id([A] self) {\n"
      "  return(self.x)\n"
      "}\n"
      "\n"
      "[return<i32>]\n"
      "/B/id([B] self) {\n"
      "  return(self.y)\n"
      "}\n"
      "\n"
      "[return<i32>]\n"
      "main() {\n"
      "  [A] a{A{[x] 1i32}}\n"
      "  [B] b{B{[y] 2i32}}\n"
      "  return(plus(a.id(), b.id()))\n"
      "}\n";

  auto program = parseProgram(source);
  primec::Semantics semantics;
  primec::SemanticProgram semanticProgram;
  std::string error;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  REQUIRE(semantics.validate(program, "/main", error, defaults, defaults, {}, nullptr, false, &semanticProgram));
  CHECK(error.empty());

  const auto methodTargets = primec::semanticProgramMethodCallTargetView(semanticProgram);
  const auto hasAIdTarget = std::any_of(
      methodTargets.begin(),
      methodTargets.end(),
      [&semanticProgram](const primec::SemanticProgramMethodCallTarget *entry) {
        return entry->scopePath == "/main" && entry->methodName == "id" &&
               primec::semanticProgramMethodCallTargetResolvedPath(semanticProgram, *entry) ==
                   "/A/id";
      });
  const auto hasBIdTarget = std::any_of(
      methodTargets.begin(),
      methodTargets.end(),
      [&semanticProgram](const primec::SemanticProgramMethodCallTarget *entry) {
        return entry->scopePath == "/main" && entry->methodName == "id" &&
               primec::semanticProgramMethodCallTargetResolvedPath(semanticProgram, *entry) ==
                   "/B/id";
      });
  CHECK(hasAIdTarget);
  CHECK(hasBIdTarget);
}

TEST_CASE("semantic product publishes user struct own at as index direct-call target") {
  // TODO-5315: `values[k]` and `at(values, k)` on a user struct that declares
  // its own `at` publish the struct method, not the builtin `/at`, so lowering
  // needs no receiver heuristics.
  const std::string source =
      "namespace demo {\n"
      "  [struct]\n"
      "  Bag() {\n"
      "    [i32 mut] total{7i32}\n"
      "\n"
      "    [return<i32>]\n"
      "    at([i32] key) {\n"
      "      return(plus(this.total, key))\n"
      "    }\n"
      "  }\n"
      "}\n"
      "\n"
      "[return<i32>]\n"
      "main() {\n"
      "  [/demo/Bag mut] values{/demo/Bag{}}\n"
      "  [i32] indexed{values[1i32]}\n"
      "  return(plus(indexed, at(values, 2i32)))\n"
      "}\n";

  auto program = parseProgram(source);
  primec::Semantics semantics;
  primec::SemanticProgram semanticProgram;
  std::string error;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  REQUIRE(semantics.validate(program, "/main", error, defaults, defaults, {}, nullptr, false, &semanticProgram));
  CHECK(error.empty());

  const auto directTargets = primec::semanticProgramDirectCallTargetView(semanticProgram);
  std::vector<std::string> mainAtTargets;
  for (const auto *entry : directTargets) {
    if (entry->scopePath == "/main" && entry->callName == "at") {
      mainAtTargets.emplace_back(
          primec::semanticProgramDirectCallTargetResolvedPath(semanticProgram, *entry));
    }
  }
  // Binding initializers can publish more than one entry for the same call,
  // so pin the resolved path of every `/main` `at` entry, not their count.
  REQUIRE(mainAtTargets.size() >= 2);
  for (const auto &target : mainAtTargets) {
    CHECK(target == "/demo/Bag/at");
  }
}

TEST_CASE("semantic product routes namespaced user Map methods to the user struct") {
  // TODO-4751: a bare `Map` spelling is an ordinary struct name, so a user
  // `Map<K, V>` in its own namespace publishes its own specialized method
  // targets instead of the canonical `/std/collections/map/*` helpers.
  const std::string source =
      "namespace mylib {\n"
      "  [public struct]\n"
      "  Map<K, V>() {\n"
      "    [i32 mut] total{0i32}\n"
      "\n"
      "    [public return<i32>]\n"
      "    count() {\n"
      "      return(this.total)\n"
      "    }\n"
      "\n"
      "    [public mut return<void>]\n"
      "    insert([K] key, [V] value) {\n"
      "      assign(this.total, plus(this.total, 1i32))\n"
      "    }\n"
      "  }\n"
      "}\n"
      "import /mylib/*\n"
      "\n"
      "[return<i32>]\n"
      "main() {\n"
      "  [Map<i32, i32> mut] values{Map<i32, i32>{}}\n"
      "  values.insert(1i32, 2i32)\n"
      "  return(values.count())\n"
      "}\n";

  auto program = parseProgram(source);
  primec::Semantics semantics;
  primec::SemanticProgram semanticProgram;
  std::string error;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  REQUIRE(semantics.validate(program, "/main", error, defaults, defaults, {}, nullptr, false, &semanticProgram));
  CHECK(error.empty());

  const std::string userMapPrefix = "/mylib/Map__t";
  auto isUserMapMethod = [&](const std::string &path, const std::string &method) {
    return path.rfind(userMapPrefix, 0) == 0 &&
           path.size() > method.size() + 1 &&
           path.compare(path.size() - method.size() - 1, method.size() + 1, "/" + method) == 0;
  };
  std::vector<std::string> mainTargets;
  for (const auto *entry : primec::semanticProgramMethodCallTargetView(semanticProgram)) {
    if (entry->scopePath == "/main") {
      mainTargets.emplace_back(
          primec::semanticProgramMethodCallTargetResolvedPath(semanticProgram, *entry));
    }
  }
  for (const auto *entry : primec::semanticProgramDirectCallTargetView(semanticProgram)) {
    if (entry->scopePath == "/main" &&
        (entry->callName == "count" || entry->callName == "insert")) {
      mainTargets.emplace_back(
          primec::semanticProgramDirectCallTargetResolvedPath(semanticProgram, *entry));
    }
  }
  const bool hasCount = std::any_of(mainTargets.begin(), mainTargets.end(), [&](const std::string &path) {
    return isUserMapMethod(path, "count");
  });
  const bool hasInsert = std::any_of(mainTargets.begin(), mainTargets.end(), [&](const std::string &path) {
    return isUserMapMethod(path, "insert");
  });
  CHECK(hasCount);
  CHECK(hasInsert);
  for (const auto &path : mainTargets) {
    CHECK(path.rfind("/std/collections/map/", 0) != 0);
  }
}

TEST_CASE("semantic product keeps helper-return soa mutator targets on alias wrappers") {
  const std::string source = R"(
[struct reflect]
Particle() {
  [i32] x{1i32}
}

[return<soa<Particle>>]
cloneValues() {
  [soa<Particle>] values{soa<Particle>()}
  return(values)
}

[return<i32>]
/soa/push([soa<Particle>] values, [Particle] value) {
  return(value.x)
}

[return<i32>]
/soa/reserve([soa<Particle>] values, [i32] count) {
  return(count)
}

[return<i32>]
/std/collections/soa/push([soa<Particle>] values, [Particle] value) {
  return(plus(value.x, 100i32))
}

[return<i32>]
/std/collections/soa/reserve([soa<Particle>] values, [i32] count) {
  return(plus(count, 100i32))
}

[return<void>]
/std/collections/soa/SoaVector__Particle/push([soa<Particle>] values,
                                                                   [Particle] value) {
}

[return<void>]
/std/collections/soa/SoaVector__Particle/reserve([soa<Particle>] values,
                                                                      [i32] count) {
}

[effects(heap_alloc), return<i32>]
main() {
  [auto] pushed{cloneValues().push(Particle(7i32))}
  [auto] reserved{cloneValues().reserve(4i32)}
  return(plus(pushed, reserved))
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

  const auto *pushTarget = findSemanticEntry(
      primec::semanticProgramMethodCallTargetView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramMethodCallTarget &entry) {
        return entry.scopePath == "/main" && entry.methodName == "push" &&
               primec::semanticProgramMethodCallTargetResolvedPath(semanticProgram, entry) ==
                   "/soa/push";
      });
  REQUIRE(pushTarget != nullptr);

  const auto *reserveTarget = findSemanticEntry(
      primec::semanticProgramMethodCallTargetView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramMethodCallTarget &entry) {
        return entry.scopePath == "/main" && entry.methodName == "reserve" &&
               primec::semanticProgramMethodCallTargetResolvedPath(semanticProgram, entry) ==
                   "/soa/reserve";
      });
  REQUIRE(reserveTarget != nullptr);

  const auto choseConcreteExperimentalPushTargets = primec::semanticProgramMethodCallTargetView(semanticProgram);
  const bool choseConcreteExperimentalPush = std::any_of(
      choseConcreteExperimentalPushTargets.begin(),
      choseConcreteExperimentalPushTargets.end(),
      [&semanticProgram](const primec::SemanticProgramMethodCallTarget *entry) {
        return entry->scopePath == "/main" && entry->methodName == "push" &&
               primec::semanticProgramMethodCallTargetResolvedPath(semanticProgram, *entry) ==
                   "/std/collections/soa/SoaVector__Particle/push";
      });
  CHECK_FALSE(choseConcreteExperimentalPush);
}

TEST_CASE("semantic product keeps nested helper-return soa mutator targets on alias wrappers") {
  const std::string source = R"(
[struct reflect]
Particle() {
  [i32] x{1i32}
}

Holder() {}

[return<soa<Particle>>]
/Holder/cloneValues([Holder] self) {
  [soa<Particle>] values{soa<Particle>()}
  return(values)
}

[return<i32>]
/soa/push([soa<Particle>] values, [Particle] value) {
  return(value.x)
}

[return<i32>]
/soa/reserve([soa<Particle>] values, [i32] count) {
  return(count)
}

[return<i32>]
/std/collections/soa/push([soa<Particle>] values, [Particle] value) {
  return(plus(value.x, 100i32))
}

[return<i32>]
/std/collections/soa/reserve([soa<Particle>] values, [i32] count) {
  return(plus(count, 100i32))
}

[return<void>]
/std/collections/soa/SoaVector__Particle/push([soa<Particle>] values,
                                                                   [Particle] value) {
}

[return<void>]
/std/collections/soa/SoaVector__Particle/reserve([soa<Particle>] values,
                                                                      [i32] count) {
}

[effects(heap_alloc), return<i32>]
main() {
  [Holder] holder{Holder{}}
  [auto] pushed{holder.cloneValues().push(Particle(7i32))}
  [auto] reserved{holder.cloneValues().reserve(4i32)}
  return(plus(pushed, reserved))
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

  const auto *pushTarget = findSemanticEntry(
      primec::semanticProgramMethodCallTargetView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramMethodCallTarget &entry) {
        return entry.scopePath == "/main" && entry.methodName == "push" &&
               primec::semanticProgramMethodCallTargetResolvedPath(semanticProgram, entry) ==
                   "/soa/push";
      });
  REQUIRE(pushTarget != nullptr);

  const auto *reserveTarget = findSemanticEntry(
      primec::semanticProgramMethodCallTargetView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramMethodCallTarget &entry) {
        return entry.scopePath == "/main" && entry.methodName == "reserve" &&
               primec::semanticProgramMethodCallTargetResolvedPath(semanticProgram, entry) ==
                   "/soa/reserve";
      });
  REQUIRE(reserveTarget != nullptr);

  const auto choseConcreteExperimentalPushTargets = primec::semanticProgramMethodCallTargetView(semanticProgram);
  const bool choseConcreteExperimentalPush = std::any_of(
      choseConcreteExperimentalPushTargets.begin(),
      choseConcreteExperimentalPushTargets.end(),
      [&semanticProgram](const primec::SemanticProgramMethodCallTarget *entry) {
        return entry->scopePath == "/main" && entry->methodName == "push" &&
               primec::semanticProgramMethodCallTargetResolvedPath(semanticProgram, *entry) ==
                   "/std/collections/soa/SoaVector__Particle/push";
      });
  CHECK_FALSE(choseConcreteExperimentalPush);
}

TEST_CASE("semantic product keeps nested helper-return soa read targets on alias wrappers") {
  const std::string source = R"(
import /std/collections/vector

[struct reflect]
Particle() {
  [i32] x{1i32}
}

Holder() {}

[return<soa<Particle>>]
/Holder/cloneValues([Holder] self) {
  [soa<Particle>] values{soa<Particle>()}
  return(values)
}

[return<Particle>]
/soa/get([soa<Particle>] values, [i32] index) {
  return(Particle(index))
}

[return<Particle>]
/soa/ref([soa<Particle>] values, [i32] index) {
  return(Particle(index))
}

[return<vector<Particle>>]
/to_aos([soa<Particle>] values) {
  return(vector<Particle>())
}

[return<Particle>]
/std/collections/soa/get([soa<Particle>] values, [i32] index) {
  return(Particle(plus(index, 100i32)))
}

[return<Particle>]
/std/collections/soa/ref([soa<Particle>] values, [i32] index) {
  return(Particle(plus(index, 100i32)))
}

[return<vector<Particle>>]
/std/collections/soa/to_aos([soa<Particle>] values) {
  return(vector<Particle>())
}

[return<Particle>]
/std/collections/soa/SoaVector__Particle/get([soa<Particle>] values,
                                                                  [i32] index) {
  return(Particle(plus(index, 200i32)))
}

[return<Particle>]
/std/collections/soa/SoaVector__Particle/ref([soa<Particle>] values,
                                                                  [i32] index) {
  return(Particle(plus(index, 200i32)))
}

[return<vector<Particle>>]
/std/collections/soa/SoaVector__Particle/to_aos([soa<Particle>] values) {
  return(vector<Particle>())
}

[effects(heap_alloc), return<i32>]
main() {
  [Holder] holder{Holder{}}
  [auto] picked{holder.cloneValues().get(1i32)}
  [auto] pickedRef{holder.cloneValues().ref(0i32)}
  [auto] unpacked{holder.cloneValues().to_aos()}
  return(plus(plus(picked.x, pickedRef.x), count(unpacked)))
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

  const auto *getTarget = findSemanticEntry(
      primec::semanticProgramMethodCallTargetView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramMethodCallTarget &entry) {
        return entry.scopePath == "/main" && entry.methodName == "get" &&
               primec::semanticProgramMethodCallTargetResolvedPath(semanticProgram, entry) ==
                   "/soa/get";
      });
  REQUIRE(getTarget != nullptr);

  const auto *refTarget = findSemanticEntry(
      primec::semanticProgramMethodCallTargetView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramMethodCallTarget &entry) {
        return entry.scopePath == "/main" && entry.methodName == "ref" &&
               primec::semanticProgramMethodCallTargetResolvedPath(semanticProgram, entry) ==
                   "/soa/ref";
      });
  REQUIRE(refTarget != nullptr);

  const auto *toAosTarget = findSemanticEntry(
      primec::semanticProgramMethodCallTargetView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramMethodCallTarget &entry) {
        return entry.scopePath == "/main" && entry.methodName == "to_aos" &&
               primec::semanticProgramMethodCallTargetResolvedPath(semanticProgram, entry) ==
                   "/to_aos";
      });
  REQUIRE(toAosTarget != nullptr);

  const auto *pickedEntry = findSemanticEntry(
      primec::semanticProgramLocalAutoFactView(semanticProgram),
      [](const primec::SemanticProgramLocalAutoFact &entry) {
        return entry.scopePath == "/main" && entry.bindingName == "picked";
      });
  REQUIRE(pickedEntry != nullptr);
  CHECK(pickedEntry->initializerMethodCallResolvedPath == "/soa/get");

  const auto *pickedRefEntry = findSemanticEntry(
      primec::semanticProgramLocalAutoFactView(semanticProgram),
      [](const primec::SemanticProgramLocalAutoFact &entry) {
        return entry.scopePath == "/main" && entry.bindingName == "pickedRef";
      });
  REQUIRE(pickedRefEntry != nullptr);
  CHECK(pickedRefEntry->initializerMethodCallResolvedPath == "/soa/ref");

  const auto *unpackedEntry = findSemanticEntry(
      primec::semanticProgramLocalAutoFactView(semanticProgram),
      [](const primec::SemanticProgramLocalAutoFact &entry) {
        return entry.scopePath == "/main" && entry.bindingName == "unpacked";
      });
  REQUIRE(unpackedEntry != nullptr);
  CHECK(unpackedEntry->initializerMethodCallResolvedPath == "/to_aos");

  const auto choseConcreteExperimentalGetTargets = primec::semanticProgramMethodCallTargetView(semanticProgram);
  const bool choseConcreteExperimentalGet = std::any_of(
      choseConcreteExperimentalGetTargets.begin(),
      choseConcreteExperimentalGetTargets.end(),
      [&semanticProgram](const primec::SemanticProgramMethodCallTarget *entry) {
        return entry->scopePath == "/main" && entry->methodName == "get" &&
               primec::semanticProgramMethodCallTargetResolvedPath(semanticProgram, *entry) ==
                   "/std/collections/soa/SoaVector__Particle/get";
      });
  CHECK_FALSE(choseConcreteExperimentalGet);
}

TEST_CASE("semantic product keeps helper-return soa read targets on alias wrappers") {
  const std::string source = R"(
import /std/collections/vector

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[return<soa<Particle>>]
cloneValues() {
  [soa<Particle>] values{soa<Particle>()}
  return(values)
}

[return<Particle>]
/soa/get([soa<Particle>] values, [i32] index) {
  return(Particle(index))
}

[return<Particle>]
/soa/ref([soa<Particle>] values, [i32] index) {
  return(Particle(index))
}

[return<vector<Particle>>]
/to_aos([soa<Particle>] values) {
  return(vector<Particle>())
}

[return<Particle>]
/std/collections/soa/get([soa<Particle>] values, [i32] index) {
  return(Particle(plus(index, 100i32)))
}

[return<Particle>]
/std/collections/soa/ref([soa<Particle>] values, [i32] index) {
  return(Particle(plus(index, 100i32)))
}

[return<vector<Particle>>]
/std/collections/soa/to_aos([soa<Particle>] values) {
  return(vector<Particle>())
}

[return<Particle>]
/std/collections/soa/SoaVector__Particle/get([soa<Particle>] values,
                                                                  [i32] index) {
  return(Particle(plus(index, 200i32)))
}

[return<Particle>]
/std/collections/soa/SoaVector__Particle/ref([soa<Particle>] values,
                                                                  [i32] index) {
  return(Particle(plus(index, 200i32)))
}

[return<vector<Particle>>]
/std/collections/soa/SoaVector__Particle/to_aos([soa<Particle>] values) {
  return(vector<Particle>())
}

[effects(heap_alloc), return<i32>]
main() {
  [auto] picked{cloneValues().get(1i32)}
  [auto] pickedRef{cloneValues().ref(1i32)}
  [auto] unpacked{cloneValues().to_aos()}
  return(plus(plus(picked.x, pickedRef.x), count(unpacked)))
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

  const auto *getTarget = findSemanticEntry(
      primec::semanticProgramMethodCallTargetView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramMethodCallTarget &entry) {
        return entry.scopePath == "/main" && entry.methodName == "get" &&
               primec::semanticProgramMethodCallTargetResolvedPath(semanticProgram, entry) ==
                   "/soa/get";
      });
  REQUIRE(getTarget != nullptr);

  const auto *refTarget = findSemanticEntry(
      primec::semanticProgramMethodCallTargetView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramMethodCallTarget &entry) {
        return entry.scopePath == "/main" && entry.methodName == "ref" &&
               primec::semanticProgramMethodCallTargetResolvedPath(semanticProgram, entry) ==
                   "/soa/ref";
      });
  REQUIRE(refTarget != nullptr);

  const auto *toAosTarget = findSemanticEntry(
      primec::semanticProgramMethodCallTargetView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramMethodCallTarget &entry) {
        return entry.scopePath == "/main" && entry.methodName == "to_aos" &&
               primec::semanticProgramMethodCallTargetResolvedPath(semanticProgram, entry) ==
                   "/to_aos";
      });
  REQUIRE(toAosTarget != nullptr);

  const auto choseConcreteExperimentalGetTargets = primec::semanticProgramMethodCallTargetView(semanticProgram);
  const bool choseConcreteExperimentalGet = std::any_of(
      choseConcreteExperimentalGetTargets.begin(),
      choseConcreteExperimentalGetTargets.end(),
      [&semanticProgram](const primec::SemanticProgramMethodCallTarget *entry) {
        return entry->scopePath == "/main" && entry->methodName == "get" &&
               primec::semanticProgramMethodCallTargetResolvedPath(semanticProgram, *entry) ==
                   "/std/collections/soa/SoaVector__Particle/get";
      });
  CHECK_FALSE(choseConcreteExperimentalGet);
}

TEST_CASE("semantic product keeps helper-return borrowed soa read targets on canonical wrappers compatibility") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*
import /std/collections/soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[return<Reference<SoaVector<Particle>>>]
pickBorrowed([Reference<SoaVector<Particle>>] values) {
  return(values)
}

[effects(heap_alloc), return<i32>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  values.push(Particle(7i32))
  values.push(Particle(9i32))
  [auto] picked{pickBorrowed(location(values)).get(1i32)}
  [auto] pickedRef{pickBorrowed(location(values)).ref(0i32)}
  [auto] unpacked{pickBorrowed(location(values)).to_aos()}
  [i32] borrowedCount{pickBorrowed(location(values)).count()}
  return(plus(plus(picked.x, pickedRef.x),
              plus(count(unpacked), borrowedCount)))
}
)";

  primec::CompilePipelineOutput output;
  std::string error;
  const bool ok = validateSoaCompatSourceForTesting(source, output, error);
  INFO(error);
  // TODO-5050 shape (a) + to_aos_ref gap (RESOLVED): canonical public soa
  // read-helper routing (get/ref/count/to_aos) now all work correctly on a
  // receiver expression that is itself a call to a user-defined helper
  // returning Reference<SoaVector<T>>. Verified via a standalone
  // `--emit=vm` probe that the fixture also runs to completion, returning
  // 20 (9 + 7 + 2 + 2).
  CHECK(ok);
}

TEST_SUITE_END();
