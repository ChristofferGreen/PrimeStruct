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

TEST_SUITE_END();
