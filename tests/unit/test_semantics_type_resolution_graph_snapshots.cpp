#include <algorithm>
#include <cstdint>

#include "primec/testing/SemanticsValidationHelpers.h"

#include "third_party/doctest.h"
#include "test_semantics_helpers.h"
#include "test_semantics_type_resolution_graph_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.type_resolution_graph");

TEST_CASE("type resolution try operand metadata stays aligned with query snapshots") {
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
  primec::semantics::TypeResolutionTryValueSnapshot trySnapshot;
  REQUIRE(primec::semantics::computeTypeResolutionTryValueSnapshotForTesting(
      parseProgram(source), "/main", error, trySnapshot));
  CHECK(error.empty());

  primec::semantics::TypeResolutionQueryBindingSnapshot queryBindingSnapshot;
  REQUIRE(primec::semantics::computeTypeResolutionQueryBindingSnapshotForTesting(
      parseProgram(source), "/main", error, queryBindingSnapshot));
  CHECK(error.empty());

  primec::semantics::TypeResolutionQueryCallSnapshot queryCallSnapshot;
  REQUIRE(primec::semantics::computeTypeResolutionQueryCallSnapshotForTesting(
      parseProgram(source), "/main", error, queryCallSnapshot));
  CHECK(error.empty());

  primec::semantics::TypeResolutionQueryResultTypeSnapshot queryResultSnapshot;
  REQUIRE(primec::semantics::computeTypeResolutionQueryResultTypeSnapshotForTesting(
      parseProgram(source), "/main", error, queryResultSnapshot));
  CHECK(error.empty());

  const auto &tryEntry = requireTryValueSnapshotEntry(trySnapshot, "/main", "/lookup");
  const auto &callEntry = requireQueryCallSnapshotEntry(queryCallSnapshot, "/main", "/lookup");
  const auto &bindingEntry = requireQueryBindingSnapshotEntry(queryBindingSnapshot, "/main", "/lookup");
  const auto &resultEntry = requireQueryResultTypeSnapshotEntry(queryResultSnapshot, "/main", "/lookup");
  CHECK(tryEntry.operandResolvedPath == callEntry.resolvedPath);
  CHECK(tryEntry.operandResolvedPath == bindingEntry.resolvedPath);
  CHECK(tryEntry.operandBindingTypeText == bindingEntry.bindingTypeText);
  CHECK(tryEntry.operandReceiverBindingTypeText.empty());
  CHECK(tryEntry.operandQueryTypeText == callEntry.typeText);
  CHECK(tryEntry.valueTypeText == resultEntry.valueTypeText);
  CHECK(tryEntry.errorTypeText == resultEntry.errorTypeText);
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
  const auto &callEntry = requireCallBindingSnapshotEntry(callSnapshot, "/main", "/id__t25a78a513414c3bf");
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
      requireQueryBindingSnapshotEntry(queryBindingSnapshot, "/main", "/id__t25a78a513414c3bf");
  const auto &callEntry = requireCallBindingSnapshotEntry(callSnapshot, "/main", "/id__t25a78a513414c3bf");
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
      requireQueryCallSnapshotEntry(queryCallSnapshot, "/main", "/id__t25a78a513414c3bf");
  const auto &callEntry = requireCallBindingSnapshotEntry(callSnapshot, "/main", "/id__t25a78a513414c3bf");
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

  const auto targetIt =
      std::find_if(semanticProgram.directCallTargets.begin(),
                   semanticProgram.directCallTargets.end(),
                   [](const primec::SemanticProgramDirectCallTarget &entry) {
                     return entry.scopePath == "/main" &&
                            (entry.callName == "id" || entry.callName.rfind("/id__t", 0) == 0) &&
                            entry.resolvedPath.rfind("/id__t", 0) == 0;
                   });
  REQUIRE(targetIt != semanticProgram.directCallTargets.end());
  CHECK(targetIt->sourceLine > 0);
  CHECK(targetIt->sourceColumn > 0);
}

TEST_CASE("semantic product publishes resolved method-call targets") {
  const std::string source =
      "[return<i32>]\n"
      "/vector/count([vector<i32>] self) {\n"
      "  return(17i32)\n"
      "}\n"
      "\n"
      "[return<i32>]\n"
      "main() {\n"
      "  [auto] values{vector(1i32)}\n"
      "  return(values.count())\n"
      "}\n";

  auto program = parseProgram(source);
  primec::Semantics semantics;
  primec::SemanticProgram semanticProgram;
  std::string error;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  REQUIRE(semantics.validate(program, "/main", error, defaults, defaults, {}, nullptr, false, &semanticProgram));
  CHECK(error.empty());

  const auto targetIt =
      std::find_if(semanticProgram.methodCallTargets.begin(),
                   semanticProgram.methodCallTargets.end(),
                   [](const primec::SemanticProgramMethodCallTarget &entry) {
                     return entry.scopePath == "/main" &&
                            entry.methodName == "count" &&
                            entry.resolvedPath == "/vector/count";
                   });
  REQUIRE(targetIt != semanticProgram.methodCallTargets.end());
  CHECK(targetIt->receiverTypeText.find("vector") != std::string::npos);
  CHECK(targetIt->sourceLine > 0);
  CHECK(targetIt->sourceColumn > 0);
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
  REQUIRE(semantics.validate(program, "/main", error, defaults, defaults, {}, nullptr, false, &semanticProgram));
  CHECK(error.empty());

  const auto choiceIt =
      std::find_if(semanticProgram.bridgePathChoices.begin(),
                   semanticProgram.bridgePathChoices.end(),
                   [](const primec::SemanticProgramBridgePathChoice &entry) {
                     return entry.scopePath == "/main" &&
                            entry.collectionFamily == "vector" &&
                            entry.helperName == "count" &&
                            entry.chosenPath == "/vector/count";
                   });
  REQUIRE(choiceIt != semanticProgram.bridgePathChoices.end());
  CHECK(choiceIt->sourceLine > 0);
  CHECK(choiceIt->sourceColumn > 0);
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
  REQUIRE(semantics.validate(program, "/main", error, defaults, defaults, {}, nullptr, false, &semanticProgram));
  CHECK(error.empty());

  const auto choiceIt =
      std::find_if(semanticProgram.bridgePathChoices.begin(),
                   semanticProgram.bridgePathChoices.end(),
                   [](const primec::SemanticProgramBridgePathChoice &entry) {
                     return entry.scopePath == "/main" &&
                            entry.collectionFamily == "map" &&
                            entry.helperName == "count" &&
                            entry.chosenPath == "/std/collections/map/count";
                   });
  REQUIRE(choiceIt != semanticProgram.bridgePathChoices.end());
  CHECK(choiceIt->sourceLine > 0);
  CHECK(choiceIt->sourceColumn > 0);
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

  const auto summaryIt =
      std::find_if(semanticProgram.callableSummaries.begin(),
                   semanticProgram.callableSummaries.end(),
                   [](const primec::SemanticProgramCallableSummary &entry) {
                     return entry.fullPath == "/main" && !entry.isExecution;
                   });
  REQUIRE(summaryIt != semanticProgram.callableSummaries.end());
  CHECK(summaryIt->returnKind == "i32");
  CHECK(summaryIt->activeEffects ==
        std::vector<std::string>{"asset_read", "io_out"});
  CHECK(summaryIt->activeCapabilities == std::vector<std::string>{"io_out"});
  CHECK(summaryIt->hasResultType);
  CHECK(summaryIt->resultTypeHasValue);
  CHECK(summaryIt->resultValueType == "int");
  CHECK(summaryIt->resultErrorType == "MyError");
  CHECK(summaryIt->hasOnError);
  CHECK(summaryIt->onErrorHandlerPath == "/unexpectedError");
  CHECK(summaryIt->onErrorErrorType == "MyError");
}

TEST_CASE("semantic product publishes struct and enum metadata") {
  const std::string source =
      "[public struct no_padding align_bytes(8)]\n"
      "Packet {\n"
      "  [i32] left{1i32}\n"
      "  [i64] right{2i64}\n"
      "}\n"
      "\n"
      "[enum]\n"
      "Mode {\n"
      "  Idle\n"
      "  Busy\n"
      "}\n"
      "\n"
      "[return<int>]\n"
      "main() {\n"
      "  return(0i32)\n"
      "}\n";

  auto program = parseProgram(source);
  primec::Semantics semantics;
  primec::SemanticProgram semanticProgram;
  std::string error;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  REQUIRE(semantics.validate(program, "/main", error, defaults, defaults, {}, nullptr, false, &semanticProgram));
  CHECK(error.empty());

  const auto packetIt =
      std::find_if(semanticProgram.typeMetadata.begin(),
                   semanticProgram.typeMetadata.end(),
                   [](const primec::SemanticProgramTypeMetadata &entry) {
                     return entry.fullPath == "/Packet";
                   });
  REQUIRE(packetIt != semanticProgram.typeMetadata.end());
  CHECK(packetIt->category == "struct");
  CHECK(packetIt->isPublic);
  CHECK(packetIt->hasNoPadding);
  CHECK(packetIt->hasExplicitAlignment);
  CHECK(packetIt->explicitAlignmentBytes == 8u);
  CHECK(packetIt->fieldCount == 2u);
  CHECK(packetIt->enumValueCount == 0u);
  CHECK(packetIt->sourceLine > 0);
  CHECK(packetIt->sourceColumn > 0);

  const auto modeIt =
      std::find_if(semanticProgram.typeMetadata.begin(),
                   semanticProgram.typeMetadata.end(),
                   [](const primec::SemanticProgramTypeMetadata &entry) {
                     return entry.fullPath == "/Mode";
                   });
  REQUIRE(modeIt != semanticProgram.typeMetadata.end());
  CHECK(modeIt->category == "enum");
  CHECK(modeIt->enumValueCount == 2u);
  CHECK(modeIt->fieldCount == 0u);
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
      "main() {\n"
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

  const auto parameterIt =
      std::find_if(semanticProgram.bindingFacts.begin(),
                   semanticProgram.bindingFacts.end(),
                   [](const primec::SemanticProgramBindingFact &entry) {
                     return entry.scopePath == "/makePair" &&
                            entry.siteKind == "parameter" &&
                            entry.name == "base";
                   });
  REQUIRE(parameterIt != semanticProgram.bindingFacts.end());
  CHECK(parameterIt->bindingTypeText == "i32");

  const auto localIt =
      std::find_if(semanticProgram.bindingFacts.begin(),
                   semanticProgram.bindingFacts.end(),
                   [](const primec::SemanticProgramBindingFact &entry) {
                     return entry.scopePath == "/makePair" &&
                            entry.siteKind == "local" &&
                            entry.name == "widened";
                   });
  REQUIRE(localIt != semanticProgram.bindingFacts.end());
  CHECK(localIt->bindingTypeText == "i64");

  const auto helperParameterIt =
      std::find_if(semanticProgram.bindingFacts.begin(),
                   semanticProgram.bindingFacts.end(),
                   [](const primec::SemanticProgramBindingFact &entry) {
                     return entry.siteKind == "parameter" &&
                            entry.name == "value" &&
                            entry.scopePath.rfind("/id", 0) == 0;
                   });
  REQUIRE(helperParameterIt != semanticProgram.bindingFacts.end());
  CHECK(helperParameterIt->bindingTypeText == "i32");

  const auto tempIt =
      std::find_if(semanticProgram.bindingFacts.begin(),
                   semanticProgram.bindingFacts.end(),
                   [](const primec::SemanticProgramBindingFact &entry) {
                     return entry.scopePath == "/main" &&
                            entry.siteKind == "temporary" &&
                            (entry.name == "id" || entry.name.rfind("/id__t", 0) == 0) &&
                            entry.bindingTypeText == "i32";
                   });
  REQUIRE(tempIt != semanticProgram.bindingFacts.end());
  CHECK(tempIt->sourceLine > 0);
  CHECK(tempIt->sourceColumn > 0);

  const auto mainReturnIt =
      std::find_if(semanticProgram.returnFacts.begin(),
                   semanticProgram.returnFacts.end(),
                   [](const primec::SemanticProgramReturnFact &entry) {
                     return entry.definitionPath == "/main";
                   });
  REQUIRE(mainReturnIt != semanticProgram.returnFacts.end());
  CHECK(mainReturnIt->returnKind == "i32");
  CHECK(mainReturnIt->bindingTypeText == "i32");

  const auto pairReturnIt =
      std::find_if(semanticProgram.returnFacts.begin(),
                   semanticProgram.returnFacts.end(),
                   [](const primec::SemanticProgramReturnFact &entry) {
                     return entry.definitionPath == "/makePair";
                   });
  REQUIRE(pairReturnIt != semanticProgram.returnFacts.end());
  CHECK(pairReturnIt->structPath == "/Pair");
  CHECK(pairReturnIt->bindingTypeText == "Pair");
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

  const auto localAutoIt =
      std::find_if(semanticProgram.localAutoFacts.begin(),
                   semanticProgram.localAutoFacts.end(),
                   [](const primec::SemanticProgramLocalAutoFact &entry) {
                     return entry.scopePath == "/main" && entry.bindingName == "selected";
                   });
  REQUIRE(localAutoIt != semanticProgram.localAutoFacts.end());
  CHECK(localAutoIt->bindingTypeText == "int");
  CHECK(localAutoIt->initializerResolvedPath == "/lookup");
  CHECK(localAutoIt->initializerHasTry);
  CHECK(localAutoIt->initializerTryValueType == "int");
  CHECK(localAutoIt->initializerTryErrorType == "MyError");

  const auto queryIt =
      std::find_if(semanticProgram.queryFacts.begin(),
                   semanticProgram.queryFacts.end(),
                   [](const primec::SemanticProgramQueryFact &entry) {
                     return entry.scopePath == "/main" && entry.resolvedPath == "/lookup";
                   });
  REQUIRE(queryIt != semanticProgram.queryFacts.end());
  CHECK(queryIt->bindingTypeText == "Result<int, MyError>");
  CHECK(queryIt->hasResultType);
  CHECK(queryIt->resultTypeHasValue);
  CHECK(queryIt->resultValueType == "int");
  CHECK(queryIt->resultErrorType == "MyError");

  const auto tryIt =
      std::find_if(semanticProgram.tryFacts.begin(),
                   semanticProgram.tryFacts.end(),
                   [](const primec::SemanticProgramTryFact &entry) {
                     return entry.scopePath == "/main" && entry.operandResolvedPath == "/lookup";
                   });
  REQUIRE(tryIt != semanticProgram.tryFacts.end());
  CHECK(tryIt->valueType == "int");
  CHECK(tryIt->errorType == "MyError");
  CHECK(tryIt->contextReturnKind == "i32");
  CHECK(tryIt->onErrorHandlerPath == "/unexpectedError");

  const auto onErrorIt =
      std::find_if(semanticProgram.onErrorFacts.begin(),
                   semanticProgram.onErrorFacts.end(),
                   [](const primec::SemanticProgramOnErrorFact &entry) {
                     return entry.definitionPath == "/main";
                   });
  REQUIRE(onErrorIt != semanticProgram.onErrorFacts.end());
  CHECK(onErrorIt->returnKind == "i32");
  CHECK(onErrorIt->handlerPath == "/unexpectedError");
  CHECK(onErrorIt->errorType == "MyError");
  CHECK(onErrorIt->returnResultHasValue);
  CHECK(onErrorIt->returnResultValueType == "int");
  CHECK(onErrorIt->returnResultErrorType == "MyError");
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
      "  return(id(values.count()))\n"
      "}\n";

  auto program = parseProgram(source);
  primec::Semantics semantics;
  primec::SemanticProgram semanticProgram;
  std::string error;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  REQUIRE(semantics.validate(program, "/main", error, defaults, defaults, {}, nullptr, false, &semanticProgram));
  CHECK(error.empty());

  const std::string dump = primec::formatSemanticProgram(semanticProgram);
  CHECK(dump.find("semantic_product {") != std::string::npos);
  CHECK(dump.find("entry_path: \"/main\"") != std::string::npos);
  CHECK(dump.find("direct_call_targets[") != std::string::npos);
  CHECK(dump.find("method_call_targets[") != std::string::npos);
  CHECK(dump.find("bridge_path_choices[") != std::string::npos);
  CHECK(dump.find("callable_summaries[") != std::string::npos);
  CHECK(dump.find("binding_facts[") != std::string::npos);
  CHECK(dump.find("return_facts[") != std::string::npos);
}

TEST_CASE("semantic product formatter exact golden is stable") {
  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  semanticProgram.sourceImports = {"/std/collections/*"};
  semanticProgram.imports = {"/id", "/main"};
  semanticProgram.definitions.push_back(primec::SemanticProgramDefinition{
      "id",
      "/id",
      "/",
      2,
      3,
  });
  semanticProgram.executions.push_back(primec::SemanticProgramExecution{
      "main",
      "/main",
      "/",
      7,
      1,
  });
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      "/main",
      "id",
      "/id",
      9,
      10,
  });
  semanticProgram.methodCallTargets.push_back(primec::SemanticProgramMethodCallTarget{
      "/main",
      "count",
      "vector<i32>",
      "/std/collections/vector/count",
      9,
      13,
  });
  semanticProgram.bridgePathChoices.push_back(primec::SemanticProgramBridgePathChoice{
      "/main",
      "vector",
      "count",
      "/std/collections/vector/count",
      9,
      13,
  });
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
      "/main",
      true,
      "return",
      false,
      false,
      {"io_out"},
      {"gpu"},
      true,
      true,
      "i32",
      "MyError",
      true,
      "/unexpectedError",
      "MyError",
      1,
  });
  semanticProgram.typeMetadata.push_back(primec::SemanticProgramTypeMetadata{
      "/Particle",
      "struct",
      true,
      false,
      true,
      true,
      16,
      2,
      0,
      11,
      5,
  });
  semanticProgram.bindingFacts.push_back(primec::SemanticProgramBindingFact{
      "/main",
      "local",
      "value",
      "/main/value",
      "i32",
      true,
      false,
      false,
      "",
      12,
      7,
  });
  semanticProgram.returnFacts.push_back(primec::SemanticProgramReturnFact{
      "/main",
      "return",
      "/i32",
      "i32",
      false,
      false,
      false,
      "",
      13,
      3,
  });
  semanticProgram.localAutoFacts.push_back(primec::SemanticProgramLocalAutoFact{
      "/main",
      "selected",
      "i32",
      "/id",
      "i32",
      "",
      "i32",
      false,
      "",
      "",
      false,
      "",
      "",
      "",
      "",
      "",
      "",
      "return",
      "",
      "",
      0,
      14,
      9,
  });
  semanticProgram.queryFacts.push_back(primec::SemanticProgramQueryFact{
      "/main",
      "lookup",
      "/lookup",
      "Result<i32, MyError>",
      "Result<i32, MyError>",
      "",
      true,
      true,
      "i32",
      "MyError",
      15,
      4,
  });
  semanticProgram.tryFacts.push_back(primec::SemanticProgramTryFact{
      "/main",
      "/lookup",
      "Result<i32, MyError>",
      "",
      "Result<i32, MyError>",
      "i32",
      "MyError",
      "return",
      "/unexpectedError",
      "MyError",
      1,
      16,
      8,
  });
  semanticProgram.onErrorFacts.push_back(primec::SemanticProgramOnErrorFact{
      "/main",
      "return",
      "/unexpectedError",
      "MyError",
      1,
      true,
      "i32",
      "MyError",
  });

  const std::string expected = R"(semantic_product {
  entry_path: "/main"
  source_imports[0]: "/std/collections/*"
  imports[0]: "/id"
  imports[1]: "/main"
  definitions[0]: full_path="/id" name="id" namespace_prefix="/" source="2:3"
  executions[0]: full_path="/main" name="main" namespace_prefix="/" source="7:1"
  direct_call_targets[0]: scope_path="/main" call_name="id" resolved_path="/id" source="9:10"
  method_call_targets[0]: scope_path="/main" method_name="count" receiver_type_text="vector<i32>" resolved_path="/std/collections/vector/count" source="9:13"
  bridge_path_choices[0]: scope_path="/main" collection_family="vector" helper_name="count" chosen_path="/std/collections/vector/count" source="9:13"
  callable_summaries[0]: full_path="/main" is_execution=true return_kind="return" is_compute=false is_unsafe=false active_effects=["io_out"] active_capabilities=["gpu"] has_result_type=true result_type_has_value=true result_value_type="i32" result_error_type="MyError" has_on_error=true on_error_handler_path="/unexpectedError" on_error_error_type="MyError" on_error_bound_arg_count=1
  type_metadata[0]: full_path="/Particle" category="struct" is_public=true has_no_padding=false has_platform_independent_padding=true has_explicit_alignment=true explicit_alignment_bytes=16 field_count=2 enum_value_count=0 source="11:5"
  binding_facts[0]: scope_path="/main" site_kind="local" name="value" resolved_path="/main/value" binding_type_text="i32" is_mutable=true is_entry_arg_string=false is_unsafe_reference=false reference_root="" source="12:7"
  return_facts[0]: definition_path="/main" return_kind="return" struct_path="/i32" binding_type_text="i32" is_mutable=false is_entry_arg_string=false is_unsafe_reference=false reference_root="" source="13:3"
  local_auto_facts[0]: scope_path="/main" binding_name="selected" binding_type_text="i32" initializer_resolved_path="/id" initializer_binding_type_text="i32" initializer_receiver_binding_type_text="" initializer_query_type_text="i32" initializer_result_has_value=false initializer_result_value_type="" initializer_result_error_type="" initializer_has_try=false initializer_try_operand_resolved_path="" initializer_try_operand_binding_type_text="" initializer_try_operand_receiver_binding_type_text="" initializer_try_operand_query_type_text="" initializer_try_value_type="" initializer_try_error_type="" initializer_try_context_return_kind="return" initializer_try_on_error_handler_path="" initializer_try_on_error_error_type="" initializer_try_on_error_bound_arg_count=0 source="14:9"
  query_facts[0]: scope_path="/main" call_name="lookup" resolved_path="/lookup" query_type_text="Result<i32, MyError>" binding_type_text="Result<i32, MyError>" receiver_binding_type_text="" has_result_type=true result_type_has_value=true result_value_type="i32" result_error_type="MyError" source="15:4"
  try_facts[0]: scope_path="/main" operand_resolved_path="/lookup" operand_binding_type_text="Result<i32, MyError>" operand_receiver_binding_type_text="" operand_query_type_text="Result<i32, MyError>" value_type="i32" error_type="MyError" context_return_kind="return" on_error_handler_path="/unexpectedError" on_error_error_type="MyError" on_error_bound_arg_count=1 source="16:8"
  on_error_facts[0]: definition_path="/main" return_kind="return" handler_path="/unexpectedError" error_type="MyError" bound_arg_count=1 return_result_has_value=true return_result_value_type="i32" return_result_error_type="MyError"
}
)";

  CHECK(primec::formatSemanticProgram(semanticProgram) == expected);
}

TEST_SUITE_END();
