#include <algorithm>
#include <cstdint>
#include <string_view>

#include "primec/IrLowerer.h"
#include "primec/IrSerializer.h"
#include "primec/testing/CompilePipelineDumpHelpers.h"
#include "primec/testing/SemanticsGraphHelpers.h"

#include "third_party/doctest.h"
#include "test_semantics_helpers.h"
#include "test_semantics_type_resolution_graph_helpers.h"

namespace {

const primec::Definition *findDefinitionByPath(const primec::Program &program, std::string_view fullPath) {
  const auto it =
      std::find_if(program.definitions.begin(),
                   program.definitions.end(),
                   [fullPath](const primec::Definition &definition) { return definition.fullPath == fullPath; });
  return it == program.definitions.end() ? nullptr : &*it;
}

primec::Definition *findDefinitionByPathMutable(primec::Program &program, std::string_view fullPath) {
  const auto it =
      std::find_if(program.definitions.begin(),
                   program.definitions.end(),
                   [fullPath](const primec::Definition &definition) { return definition.fullPath == fullPath; });
  return it == program.definitions.end() ? nullptr : &*it;
}

const primec::Expr *findBindingStatementByName(const primec::Definition &definition, std::string_view name) {
  const auto it =
      std::find_if(definition.statements.begin(),
                   definition.statements.end(),
                   [name](const primec::Expr &statement) { return statement.isBinding && statement.name == name; });
  return it == definition.statements.end() ? nullptr : &*it;
}

template <typename Predicate>
const primec::Expr *findExprRecursive(const primec::Expr &expr, const Predicate &predicate) {
  if (predicate(expr)) {
    return &expr;
  }
  for (const auto &arg : expr.args) {
    if (const primec::Expr *found = findExprRecursive(arg, predicate)) {
      return found;
    }
  }
  for (const auto &bodyExpr : expr.bodyArguments) {
    if (const primec::Expr *found = findExprRecursive(bodyExpr, predicate)) {
      return found;
    }
  }
  return nullptr;
}

template <typename Predicate>
primec::Expr *findExprRecursiveMutable(primec::Expr &expr, const Predicate &predicate) {
  if (predicate(expr)) {
    return &expr;
  }
  for (auto &arg : expr.args) {
    if (primec::Expr *found = findExprRecursiveMutable(arg, predicate)) {
      return found;
    }
  }
  for (auto &bodyExpr : expr.bodyArguments) {
    if (primec::Expr *found = findExprRecursiveMutable(bodyExpr, predicate)) {
      return found;
    }
  }
  return nullptr;
}

template <typename Predicate>
const primec::Expr *findExprInDefinition(const primec::Definition &definition, const Predicate &predicate) {
  for (const auto &parameter : definition.parameters) {
    if (const primec::Expr *found = findExprRecursive(parameter, predicate)) {
      return found;
    }
  }
  for (const auto &statement : definition.statements) {
    if (const primec::Expr *found = findExprRecursive(statement, predicate)) {
      return found;
    }
  }
  if (definition.returnExpr.has_value()) {
    return findExprRecursive(*definition.returnExpr, predicate);
  }
  return nullptr;
}

template <typename Predicate>
primec::Expr *findExprInDefinitionMutable(primec::Definition &definition, const Predicate &predicate) {
  for (auto &parameter : definition.parameters) {
    if (primec::Expr *found = findExprRecursiveMutable(parameter, predicate)) {
      return found;
    }
  }
  for (auto &statement : definition.statements) {
    if (primec::Expr *found = findExprRecursiveMutable(statement, predicate)) {
      return found;
    }
  }
  if (definition.returnExpr.has_value()) {
    return findExprRecursiveMutable(*definition.returnExpr, predicate);
  }
  return nullptr;
}

template <typename Entry, typename Predicate>
const Entry *findSemanticEntry(const std::vector<Entry> &entries, const Predicate &predicate) {
  const auto it = std::find_if(entries.begin(), entries.end(), predicate);
  return it == entries.end() ? nullptr : &*it;
}

bool hasCanonicalSourceMapEntry(const primec::IrModule &module, int sourceLine, int sourceColumn) {
  return std::any_of(module.instructionSourceMap.begin(),
                     module.instructionSourceMap.end(),
                     [sourceLine, sourceColumn](const primec::IrInstructionSourceMapEntry &entry) {
                       return entry.provenance == primec::IrSourceMapProvenance::CanonicalAst &&
                              entry.line == static_cast<uint32_t>(sourceLine) &&
                              entry.column == static_cast<uint32_t>(sourceColumn);
                     });
}

std::vector<uint8_t> serializeIrIgnoringSourceMapsAndDebug(const primec::IrModule &module) {
  primec::IrModule sanitized = module;
  sanitized.instructionSourceMap.clear();
  for (auto &function : sanitized.functions) {
    function.localDebugSlots.clear();
    for (auto &instruction : function.instructions) {
      instruction.debugId = 0;
    }
  }

  std::vector<uint8_t> encoded;
  std::string error;
  REQUIRE(primec::serializeIr(sanitized, encoded, error));
  CHECK(error.empty());
  return encoded;
}

} // namespace

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

TEST_CASE("semantic product publishes specialized SoaColumn field access targets") {
  const std::string source = R"(
import /std/collections/experimental_soa_storage/*

[effects(heap_alloc), return<int>]
main() {
  [SoaColumns5<i32, i32, i32, i32, i32> mut] values{soaColumns5New<i32, i32, i32, i32, i32>()}
  return(soaColumns5Capacity<i32, i32, i32, i32, i32>(values))
}
)";

  primec::testing::CompilePipelineBoundaryDumps dumps;
  std::string error;
  REQUIRE(primec::testing::captureSemanticBoundaryDumpsForTesting(
      source, "/main", dumps, error));
  CHECK(error.empty());
  CHECK(dumps.semanticProduct.find(
            "scope_path=\"/std/collections/experimental_soa_storage/SoaColumn__") !=
        std::string::npos);
  CHECK(dumps.semanticProduct.find(
            "/field_capacity\" method_name=\"capacity\" receiver_type_text=\"Reference</std/collections/experimental_soa_storage/SoaColumn__") !=
        std::string::npos);
  CHECK(dumps.semanticProduct.find(
            "resolved_path=\"/std/collections/experimental_soa_storage/SoaColumn__") !=
        std::string::npos);
  CHECK(dumps.semanticProduct.find("/capacity\" provenance_handle=") !=
        std::string::npos);
}

TEST_CASE("semantic product publishes explicit SoaColumn helper parameter binding facts") {
  const std::string source = R"(
import /std/collections/experimental_soa_storage/*

[effects(heap_alloc), return<int>]
main() {
  [SoaColumns5<i32, i32, i32, i32, i32> mut] values{soaColumns5New<i32, i32, i32, i32, i32>()}
  soaColumns5Reserve<i32, i32, i32, i32, i32>(values, 4i32)
  return(soaColumns5Capacity<i32, i32, i32, i32, i32>(values))
}
)";

  primec::testing::CompilePipelineBoundaryDumps dumps;
  std::string error;
  REQUIRE(primec::testing::captureSemanticBoundaryDumpsForTesting(
      source, "/main", dumps, error));
  CHECK(error.empty());
  CHECK(dumps.semanticProduct.find(
            "/set_field_capacity\" site_kind=\"parameter\" name=\"value\" resolved_path=\"\" binding_type_text=\"i32\"") !=
        std::string::npos);
}

TEST_CASE("semantic product query facts prefer local bindings over math constants") {
  const std::string source =
      "import /std/math/*\n"
      "\n"
      "[return<int>]\n"
      "main() {\n"
      "  [f32] e{acosh(1.0f32)}\n"
      "  [f32] f{atanh(0.0f32)}\n"
      "  return(convert<int>(plus(e, f)))\n"
      "}\n";

  auto program = parseProgram(source);
  primec::Semantics semantics;
  primec::SemanticProgram semanticProgram;
  std::string error;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  REQUIRE(semantics.validate(program, "/main", error, defaults, defaults, {}, nullptr, false, &semanticProgram));
  CHECK(error.empty());

  const auto queryIt =
      std::find_if(semanticProgram.queryFacts.begin(),
                   semanticProgram.queryFacts.end(),
                   [](const primec::SemanticProgramQueryFact &entry) {
                     return entry.scopePath == "/main" && entry.callName == "plus";
                   });
  REQUIRE(queryIt != semanticProgram.queryFacts.end());
  CHECK(queryIt->queryTypeText == "f32");
  CHECK(queryIt->bindingTypeText == "f32");
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
  CHECK(modeIt->sourceLine > 0);
  CHECK(modeIt->sourceColumn > 0);

  const auto leftFieldIt =
      std::find_if(semanticProgram.structFieldMetadata.begin(),
                   semanticProgram.structFieldMetadata.end(),
                   [](const primec::SemanticProgramStructFieldMetadata &entry) {
                     return entry.structPath == "/Packet" && entry.fieldName == "left";
                   });
  REQUIRE(leftFieldIt != semanticProgram.structFieldMetadata.end());
  CHECK(leftFieldIt->fieldIndex == 0u);
  CHECK(leftFieldIt->bindingTypeText == "i32");
  CHECK(leftFieldIt->sourceLine > 0);
  CHECK(leftFieldIt->sourceColumn > 0);

  const auto rightFieldIt =
      std::find_if(semanticProgram.structFieldMetadata.begin(),
                   semanticProgram.structFieldMetadata.end(),
                   [](const primec::SemanticProgramStructFieldMetadata &entry) {
                     return entry.structPath == "/Packet" && entry.fieldName == "right";
                   });
  REQUIRE(rightFieldIt != semanticProgram.structFieldMetadata.end());
  CHECK(rightFieldIt->fieldIndex == 1u);
  CHECK(rightFieldIt->bindingTypeText == "i64");
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

  const auto entryParameterIt =
      std::find_if(semanticProgram.bindingFacts.begin(),
                   semanticProgram.bindingFacts.end(),
                   [](const primec::SemanticProgramBindingFact &entry) {
                     return entry.scopePath == "/main" &&
                            entry.siteKind == "parameter" &&
                            entry.name == "argv";
                   });
  REQUIRE(entryParameterIt != semanticProgram.bindingFacts.end());
  CHECK(entryParameterIt->bindingTypeText == "array<string>");

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
  CHECK(onErrorIt->boundArgTexts == std::vector<std::string>{});
  CHECK(onErrorIt->returnResultHasValue);
  CHECK(onErrorIt->returnResultValueType == "int");
  CHECK(onErrorIt->returnResultErrorType == "MyError");
}

TEST_CASE("semantic product source locations stay aligned with AST-owned lowering facts") {
  const std::string source =
      "Packet {\n"
      "  [i32] left{1i32}\n"
      "  [i64] right{2i64}\n"
      "}\n"
      "\n"
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
      "main([array<string>] argv) {\n"
      "  [vector<i32>] values{vector<i32>()}\n"
      "  [i32] direct{pick(1i32)}\n"
      "  [i32] method{values.count()}\n"
      "  [i32] bridge{count(values)}\n"
      "  return(bridge)\n"
      "}\n";

  const primec::Program astProgram = parseProgram(source);
  const primec::Definition *packetDefinition = findDefinitionByPath(astProgram, "/Packet");
  const primec::Definition *mainDefinition = findDefinitionByPath(astProgram, "/main");
  REQUIRE(packetDefinition != nullptr);
  REQUIRE(mainDefinition != nullptr);
  REQUIRE(mainDefinition->returnExpr.has_value());

  const primec::Expr *packetLeftField = findBindingStatementByName(*packetDefinition, "left");
  const primec::Expr *mainValuesBinding = findBindingStatementByName(*mainDefinition, "values");
  const primec::Expr *directCallExpr =
      findExprInDefinition(*mainDefinition,
                           [](const primec::Expr &expr) {
                             return expr.kind == primec::Expr::Kind::Call && !expr.isMethodCall &&
                                    expr.name == "pick";
                           });
  const primec::Expr *methodCallExpr =
      findExprInDefinition(*mainDefinition,
                           [](const primec::Expr &expr) {
                             return expr.kind == primec::Expr::Kind::Call && expr.isMethodCall &&
                                    expr.name == "count";
                           });
  const primec::Expr *bridgeCallExpr =
      findExprInDefinition(*mainDefinition,
                           [](const primec::Expr &expr) {
                             return expr.kind == primec::Expr::Kind::Call && !expr.isMethodCall &&
                                    expr.name == "count";
                           });
  REQUIRE(packetLeftField != nullptr);
  REQUIRE(mainValuesBinding != nullptr);
  REQUIRE(directCallExpr != nullptr);
  REQUIRE(methodCallExpr != nullptr);
  REQUIRE(bridgeCallExpr != nullptr);

  auto semanticAst = parseProgram(source);
  primec::Semantics semantics;
  primec::SemanticProgram semanticProgram;
  std::string error;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  REQUIRE(semantics.validate(semanticAst, "/main", error, defaults, defaults, {}, nullptr, false, &semanticProgram));
  CHECK(error.empty());

  const auto *typeEntry =
      findSemanticEntry(semanticProgram.typeMetadata,
                        [](const primec::SemanticProgramTypeMetadata &entry) { return entry.fullPath == "/Packet"; });
  REQUIRE(typeEntry != nullptr);
  CHECK(typeEntry->sourceLine == packetDefinition->sourceLine);
  CHECK(typeEntry->sourceColumn == packetDefinition->sourceColumn);

  const auto *fieldEntry = findSemanticEntry(
      semanticProgram.structFieldMetadata,
      [](const primec::SemanticProgramStructFieldMetadata &entry) {
        return entry.structPath == "/Packet" && entry.fieldName == "left";
      });
  REQUIRE(fieldEntry != nullptr);
  CHECK(fieldEntry->sourceLine == packetLeftField->sourceLine);
  CHECK(fieldEntry->sourceColumn == packetLeftField->sourceColumn);

  const auto *parameterEntry = findSemanticEntry(
      semanticProgram.bindingFacts,
      [](const primec::SemanticProgramBindingFact &entry) {
        return entry.scopePath == "/main" && entry.siteKind == "parameter" && entry.name == "argv";
      });
  REQUIRE(parameterEntry != nullptr);
  CHECK(parameterEntry->sourceLine == mainDefinition->parameters[0].sourceLine);
  CHECK(parameterEntry->sourceColumn == mainDefinition->parameters[0].sourceColumn);

  const auto *localEntry = findSemanticEntry(
      semanticProgram.bindingFacts,
      [](const primec::SemanticProgramBindingFact &entry) {
        return entry.scopePath == "/main" && entry.siteKind == "local" && entry.name == "values";
      });
  REQUIRE(localEntry != nullptr);
  CHECK(localEntry->sourceLine == mainValuesBinding->sourceLine);
  CHECK(localEntry->sourceColumn == mainValuesBinding->sourceColumn);

  const auto *temporaryEntry = findSemanticEntry(
      semanticProgram.bindingFacts,
      [](const primec::SemanticProgramBindingFact &entry) {
        return entry.scopePath == "/main" && entry.siteKind == "temporary" && entry.name == "pick";
      });
  REQUIRE(temporaryEntry != nullptr);
  CHECK(temporaryEntry->sourceLine == directCallExpr->sourceLine);
  CHECK(temporaryEntry->sourceColumn == directCallExpr->sourceColumn);

  const auto *directCallEntry = findSemanticEntry(
      semanticProgram.directCallTargets,
      [](const primec::SemanticProgramDirectCallTarget &entry) {
        return entry.scopePath == "/main" && entry.callName == "pick";
      });
  REQUIRE(directCallEntry != nullptr);
  CHECK(directCallEntry->sourceLine == directCallExpr->sourceLine);
  CHECK(directCallEntry->sourceColumn == directCallExpr->sourceColumn);

  const auto *methodCallEntry = findSemanticEntry(
      semanticProgram.methodCallTargets,
      [](const primec::SemanticProgramMethodCallTarget &entry) {
        return entry.scopePath == "/main" && entry.methodName == "count";
      });
  REQUIRE(methodCallEntry != nullptr);
  CHECK(methodCallEntry->sourceLine == methodCallExpr->sourceLine);
  CHECK(methodCallEntry->sourceColumn == methodCallExpr->sourceColumn);

  const auto *bridgeEntry = findSemanticEntry(
      semanticProgram.bridgePathChoices,
      [](const primec::SemanticProgramBridgePathChoice &entry) {
        return entry.scopePath == "/main" && entry.helperName == "count";
      });
  REQUIRE(bridgeEntry != nullptr);
  CHECK(bridgeEntry->sourceLine == bridgeCallExpr->sourceLine);
  CHECK(bridgeEntry->sourceColumn == bridgeCallExpr->sourceColumn);

  const auto *returnEntry = findSemanticEntry(
      semanticProgram.returnFacts,
      [](const primec::SemanticProgramReturnFact &entry) { return entry.definitionPath == "/main"; });
  REQUIRE(returnEntry != nullptr);
  CHECK(returnEntry->sourceLine == mainDefinition->returnExpr->sourceLine);
  CHECK(returnEntry->sourceColumn == mainDefinition->returnExpr->sourceColumn);
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
      "  return(Result.ok(direct + selected))\n"
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

  const auto *firstDirectCall = findSemanticEntry(
      first.directCallTargets,
      [](const primec::SemanticProgramDirectCallTarget &entry) {
        return entry.scopePath == "/main" && entry.callName == "helper";
      });
  const auto *secondDirectCall = findSemanticEntry(
      second.directCallTargets,
      [](const primec::SemanticProgramDirectCallTarget &entry) {
        return entry.scopePath == "/main" && entry.callName == "helper";
      });
  REQUIRE(firstDirectCall != nullptr);
  REQUIRE(secondDirectCall != nullptr);
  CHECK(firstDirectCall->semanticNodeId != 0);
  CHECK(firstDirectCall->semanticNodeId == secondDirectCall->semanticNodeId);
  CHECK(firstDirectCall->provenanceHandle != 0);
  CHECK(firstDirectCall->provenanceHandle == secondDirectCall->provenanceHandle);

  const auto *firstQuery = findSemanticEntry(
      first.queryFacts,
      [](const primec::SemanticProgramQueryFact &entry) {
        return entry.scopePath == "/main" && entry.resolvedPath == "/lookup";
      });
  const auto *secondQuery = findSemanticEntry(
      second.queryFacts,
      [](const primec::SemanticProgramQueryFact &entry) {
        return entry.scopePath == "/main" && entry.resolvedPath == "/lookup";
      });
  REQUIRE(firstQuery != nullptr);
  REQUIRE(secondQuery != nullptr);
  CHECK(firstQuery->semanticNodeId != 0);
  CHECK(firstQuery->semanticNodeId == secondQuery->semanticNodeId);
  CHECK(firstQuery->provenanceHandle != 0);
  CHECK(firstQuery->provenanceHandle == secondQuery->provenanceHandle);

  const auto *firstTry = findSemanticEntry(
      first.tryFacts,
      [](const primec::SemanticProgramTryFact &entry) {
        return entry.scopePath == "/main" && entry.operandResolvedPath == "/lookup";
      });
  const auto *secondTry = findSemanticEntry(
      second.tryFacts,
      [](const primec::SemanticProgramTryFact &entry) {
        return entry.scopePath == "/main" && entry.operandResolvedPath == "/lookup";
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

  const auto *firstLocal = findSemanticEntry(
      first.bindingFacts,
      [](const primec::SemanticProgramBindingFact &entry) {
        return entry.scopePath == "/main" && entry.siteKind == "local" && entry.name == "selected";
      });
  const auto *secondLocal = findSemanticEntry(
      second.bindingFacts,
      [](const primec::SemanticProgramBindingFact &entry) {
        return entry.scopePath == "/main" && entry.siteKind == "local" && entry.name == "selected";
      });
  REQUIRE(firstLocal != nullptr);
  REQUIRE(secondLocal != nullptr);
  CHECK(firstLocal->semanticNodeId == secondLocal->semanticNodeId);
  CHECK(firstLocal->provenanceHandle == secondLocal->provenanceHandle);
  CHECK(firstLocal->sourceLine == secondLocal->sourceLine);
  CHECK(firstLocal->sourceColumn == secondLocal->sourceColumn);

  const auto *firstTemporary = findSemanticEntry(
      first.bindingFacts,
      [](const primec::SemanticProgramBindingFact &entry) {
        return entry.scopePath == "/main" && entry.siteKind == "temporary" && entry.name == "helper";
      });
  const auto *secondTemporary = findSemanticEntry(
      second.bindingFacts,
      [](const primec::SemanticProgramBindingFact &entry) {
        return entry.scopePath == "/main" && entry.siteKind == "temporary" && entry.name == "helper";
      });
  REQUIRE(firstTemporary != nullptr);
  REQUIRE(secondTemporary != nullptr);
  CHECK(firstTemporary->semanticNodeId == secondTemporary->semanticNodeId);
  CHECK(firstTemporary->provenanceHandle == secondTemporary->provenanceHandle);
  CHECK(firstTemporary->sourceLine == secondTemporary->sourceLine);
  CHECK(firstTemporary->sourceColumn == secondTemporary->sourceColumn);

  const auto *firstDirectCall = findSemanticEntry(
      first.directCallTargets,
      [](const primec::SemanticProgramDirectCallTarget &entry) {
        return entry.scopePath == "/main" && entry.callName == "helper";
      });
  const auto *secondDirectCall = findSemanticEntry(
      second.directCallTargets,
      [](const primec::SemanticProgramDirectCallTarget &entry) {
        return entry.scopePath == "/main" && entry.callName == "helper";
      });
  REQUIRE(firstDirectCall != nullptr);
  REQUIRE(secondDirectCall != nullptr);
  CHECK(firstDirectCall->semanticNodeId == secondDirectCall->semanticNodeId);
  CHECK(firstDirectCall->provenanceHandle == secondDirectCall->provenanceHandle);
  CHECK(firstDirectCall->sourceLine == secondDirectCall->sourceLine);
  CHECK(firstDirectCall->sourceColumn == secondDirectCall->sourceColumn);

  const auto *firstReturn = findSemanticEntry(
      first.returnFacts,
      [](const primec::SemanticProgramReturnFact &entry) { return entry.definitionPath == "/main"; });
  const auto *secondReturn = findSemanticEntry(
      second.returnFacts,
      [](const primec::SemanticProgramReturnFact &entry) { return entry.definitionPath == "/main"; });
  REQUIRE(firstReturn != nullptr);
  REQUIRE(secondReturn != nullptr);
  CHECK(firstReturn->semanticNodeId == secondReturn->semanticNodeId);
  CHECK(firstReturn->provenanceHandle == secondReturn->provenanceHandle);
  CHECK(firstReturn->sourceLine == secondReturn->sourceLine);
  CHECK(firstReturn->sourceColumn == secondReturn->sourceColumn);
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
  for (const auto &entry : semanticProgram.bindingFacts) {
    if (entry.scopePath == "/main" && entry.siteKind == "local") {
      localBindingOrder.push_back(entry.name);
    }
  }
  CHECK(localBindingOrder == std::vector<std::string>{"zeta", "alpha"});

  std::vector<std::string> directCallOrder;
  for (const auto &entry : semanticProgram.directCallTargets) {
    if (entry.scopePath == "/main" && (entry.callName == "second" || entry.callName == "first")) {
      directCallOrder.push_back(entry.callName);
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
  REQUIRE(semantics.validate(semanticAst, "/main", error, defaults, defaults, {}, nullptr, false, &semanticProgram));
  CHECK(error.empty());

  const auto *directCallEntry =
      findSemanticEntry(semanticProgram.directCallTargets,
                        [](const primec::SemanticProgramDirectCallTarget &entry) {
                          return entry.scopePath == "/main" && entry.callName == "pick";
                        });
  const auto *methodCallEntry =
      findSemanticEntry(semanticProgram.methodCallTargets,
                        [](const primec::SemanticProgramMethodCallTarget &entry) {
                          return entry.scopePath == "/main" && entry.methodName == "count";
                        });
  const auto *bridgeEntry =
      findSemanticEntry(semanticProgram.bridgePathChoices,
                        [](const primec::SemanticProgramBridgePathChoice &entry) {
                          return entry.scopePath == "/main" && entry.helperName == "count";
                        });
  const auto *returnEntry =
      findSemanticEntry(semanticProgram.returnFacts,
                        [](const primec::SemanticProgramReturnFact &entry) {
                          return entry.definitionPath == "/main";
                        });
  REQUIRE(directCallEntry != nullptr);
  REQUIRE(methodCallEntry != nullptr);
  REQUIRE(bridgeEntry != nullptr);
  REQUIRE(returnEntry != nullptr);
  CHECK(directCallEntry->provenanceHandle != 0);
  CHECK(methodCallEntry->provenanceHandle != 0);
  CHECK(bridgeEntry->provenanceHandle != 0);
  CHECK(returnEntry->provenanceHandle != 0);

  primec::IrLowerer lowerer;
  primec::IrModule semanticModule;
  REQUIRE(lowerer.lower(semanticAst, &semanticProgram, "/main", defaults, defaults, semanticModule, error));
  CHECK(error.empty());

  CHECK_FALSE(semanticModule.instructionSourceMap.empty());
  CHECK(hasCanonicalSourceMapEntry(semanticModule, directCallEntry->sourceLine, directCallEntry->sourceColumn));
  CHECK(hasCanonicalSourceMapEntry(semanticModule, methodCallEntry->sourceLine, methodCallEntry->sourceColumn));
  CHECK(hasCanonicalSourceMapEntry(semanticModule, bridgeEntry->sourceLine, bridgeEntry->sourceColumn));
  CHECK(hasCanonicalSourceMapEntry(semanticModule, returnEntry->sourceLine, returnEntry->sourceColumn));
}

TEST_CASE("semantic product lowering keeps semantic meaning while source locations stay AST-owned") {
  const std::string source = R"(
MyError {
}

[return<void>]
unexpectedError([MyError] err) {
}

[return<T>]
id<T>([T] value) {
  return(value)
}

[return<Result<int, MyError>>]
lookup() {
  return(Result.ok(4i32))
}

[return<i32>]
/vector/count([vector<i32>] self) {
  return(17i32)
}

[return<i32> on_error<MyError, /unexpectedError>]
main() {
  [auto] direct{id(1i32)}
  [auto] values{vector<i32>(1i32)}
  [i32] method{values.count()}
  [i32] bridge{count(values)}
  [auto] selected{try(lookup())}
  return(direct + method + bridge + selected)
}
)";

  auto semanticAst = parseProgram(source);
  primec::Semantics semantics;
  primec::SemanticProgram semanticProgram;
  std::string error;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  REQUIRE(semantics.validate(semanticAst, "/main", error, defaults, defaults, {}, nullptr, false, &semanticProgram));
  CHECK(error.empty());

  const auto *semanticDirectEntry =
      findSemanticEntry(semanticProgram.directCallTargets,
                        [](const primec::SemanticProgramDirectCallTarget &entry) {
                          return entry.scopePath == "/main" && entry.callName == "id";
                        });
  const auto *semanticMethodEntry =
      findSemanticEntry(semanticProgram.methodCallTargets,
                        [](const primec::SemanticProgramMethodCallTarget &entry) {
                          return entry.scopePath == "/main" && entry.methodName == "count";
                        });
  const auto *semanticBridgeEntry =
      findSemanticEntry(semanticProgram.bridgePathChoices,
                        [](const primec::SemanticProgramBridgePathChoice &entry) {
                          return entry.scopePath == "/main" && entry.helperName == "count";
                        });
  const auto *semanticReturnEntry =
      findSemanticEntry(semanticProgram.returnFacts,
                        [](const primec::SemanticProgramReturnFact &entry) {
                          return entry.definitionPath == "/main";
                        });
  REQUIRE(semanticDirectEntry != nullptr);
  REQUIRE(semanticMethodEntry != nullptr);
  REQUIRE(semanticBridgeEntry != nullptr);
  REQUIRE(semanticReturnEntry != nullptr);

  primec::Program baselineAst = semanticAst;
  primec::Program driftedAst = semanticAst;

  primec::Definition *driftedMain = findDefinitionByPathMutable(driftedAst, "/main");
  REQUIRE(driftedMain != nullptr);
  REQUIRE(driftedMain->transforms.size() >= 2);

  primec::Expr *directCallExpr =
      findExprInDefinitionMutable(*driftedMain,
                                  [](const primec::Expr &expr) {
                                    return expr.kind == primec::Expr::Kind::Call && !expr.isMethodCall &&
                                           expr.name == "id";
                                  });
  primec::Expr *methodCallExpr =
      findExprInDefinitionMutable(*driftedMain,
                                  [](const primec::Expr &expr) {
                                    return expr.kind == primec::Expr::Kind::Call && expr.isMethodCall &&
                                           expr.name == "count";
                                  });
  primec::Expr *bridgeCallExpr =
      findExprInDefinitionMutable(*driftedMain,
                                  [](const primec::Expr &expr) {
                                    return expr.kind == primec::Expr::Kind::Call && !expr.isMethodCall &&
                                           expr.name == "count";
                                  });
  primec::Expr *lookupCallExpr =
      findExprInDefinitionMutable(*driftedMain,
                                  [](const primec::Expr &expr) {
                                    return expr.kind == primec::Expr::Kind::Call && !expr.isMethodCall &&
                                           expr.name == "lookup";
                                  });
  REQUIRE(directCallExpr != nullptr);
  REQUIRE(methodCallExpr != nullptr);
  REQUIRE(bridgeCallExpr != nullptr);
  REQUIRE(lookupCallExpr != nullptr);
  auto onErrorTransformIt =
      std::find_if(driftedMain->transforms.begin(),
                   driftedMain->transforms.end(),
                   [](const primec::Transform &transform) { return transform.name == "on_error"; });
  REQUIRE(onErrorTransformIt != driftedMain->transforms.end());

  const int originalDirectLine = semanticDirectEntry->sourceLine;
  const int originalDirectColumn = semanticDirectEntry->sourceColumn;
  const int originalMethodLine = semanticMethodEntry->sourceLine;
  const int originalMethodColumn = semanticMethodEntry->sourceColumn;
  const int originalBridgeLine = semanticBridgeEntry->sourceLine;
  const int originalBridgeColumn = semanticBridgeEntry->sourceColumn;

  directCallExpr->name = "drifted_direct_name";
  directCallExpr->sourceLine = 901;
  directCallExpr->sourceColumn = 11;
  methodCallExpr->name = "drifted_method_name";
  methodCallExpr->sourceLine = 902;
  methodCallExpr->sourceColumn = 13;
  bridgeCallExpr->name = "drifted_bridge_name";
  bridgeCallExpr->sourceLine = 903;
  bridgeCallExpr->sourceColumn = 15;
  lookupCallExpr->name = "drifted_lookup_name";
  onErrorTransformIt->templateArgs[0] = "WrongError";
  onErrorTransformIt->templateArgs[1] = "/wrongHandler";

  CHECK(semanticDirectEntry->callName == "id");
  CHECK(semanticMethodEntry->methodName == "count");
  CHECK(semanticBridgeEntry->helperName == "count");
  CHECK(semanticReturnEntry->definitionPath == "/main");

  primec::IrLowerer lowerer;
  primec::IrModule baselineModule;
  REQUIRE(lowerer.lower(baselineAst, &semanticProgram, "/main", defaults, defaults, baselineModule, error));
  CHECK(error.empty());

  primec::IrModule driftedModule;
  REQUIRE(lowerer.lower(driftedAst, &semanticProgram, "/main", defaults, defaults, driftedModule, error));
  CHECK(error.empty());

  CHECK(serializeIrIgnoringSourceMapsAndDebug(baselineModule) ==
        serializeIrIgnoringSourceMapsAndDebug(driftedModule));

  CHECK(hasCanonicalSourceMapEntry(driftedModule, 901, 11));
  CHECK(hasCanonicalSourceMapEntry(driftedModule, 902, 13));
  CHECK(hasCanonicalSourceMapEntry(driftedModule, 903, 15));

  CHECK_FALSE(hasCanonicalSourceMapEntry(driftedModule, originalDirectLine, originalDirectColumn));
  CHECK_FALSE(hasCanonicalSourceMapEntry(driftedModule, originalMethodLine, originalMethodColumn));
  CHECK_FALSE(hasCanonicalSourceMapEntry(driftedModule, originalBridgeLine, originalBridgeColumn));
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
  CHECK(dump.find("struct_field_metadata[") != std::string::npos);
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
      11,
      101,
  });
  semanticProgram.executions.push_back(primec::SemanticProgramExecution{
      "main",
      "/main",
      "/",
      7,
      1,
      12,
      102,
  });
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      "/main",
      "id",
      "/id",
      9,
      10,
      13,
      103,
  });
  semanticProgram.methodCallTargets.push_back(primec::SemanticProgramMethodCallTarget{
      "/main",
      "count",
      "vector<i32>",
      "/std/collections/vector/count",
      9,
      13,
      14,
      104,
  });
  semanticProgram.bridgePathChoices.push_back(primec::SemanticProgramBridgePathChoice{
      "/main",
      "vector",
      "count",
      "/std/collections/vector/count",
      9,
      13,
      15,
      105,
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
      16,
      106,
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
      17,
      107,
  });
  semanticProgram.structFieldMetadata.push_back(primec::SemanticProgramStructFieldMetadata{
      "/Particle",
      "left",
      0,
      "i32",
      12,
      7,
      18,
      108,
  });
  semanticProgram.structFieldMetadata.push_back(primec::SemanticProgramStructFieldMetadata{
      "/Particle",
      "right",
      1,
      "i64",
      13,
      7,
      19,
      109,
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
      20,
      110,
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
      21,
      111,
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
      22,
      112,
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
      23,
      113,
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
      24,
      114,
  });
  semanticProgram.onErrorFacts.push_back(primec::SemanticProgramOnErrorFact{
      "/main",
      "return",
      "/unexpectedError",
      "MyError",
      1,
      {"err"},
      true,
      "i32",
      "MyError",
      25,
      115,
  });

  const std::string expected = R"(semantic_product {
  entry_path: "/main"
  source_imports[0]: "/std/collections/*"
  imports[0]: "/id"
  imports[1]: "/main"
  definitions[0]: full_path="/id" name="id" namespace_prefix="/" provenance_handle=101 source="2:3"
  executions[0]: full_path="/main" name="main" namespace_prefix="/" provenance_handle=102 source="7:1"
  direct_call_targets[0]: scope_path="/main" call_name="id" resolved_path="/id" provenance_handle=103 source="9:10"
  method_call_targets[0]: scope_path="/main" method_name="count" receiver_type_text="vector<i32>" resolved_path="/std/collections/vector/count" provenance_handle=104 source="9:13"
  bridge_path_choices[0]: scope_path="/main" collection_family="vector" helper_name="count" chosen_path="/std/collections/vector/count" provenance_handle=105 source="9:13"
  callable_summaries[0]: full_path="/main" is_execution=true return_kind="return" is_compute=false is_unsafe=false active_effects=["io_out"] active_capabilities=["gpu"] has_result_type=true result_type_has_value=true result_value_type="i32" result_error_type="MyError" has_on_error=true on_error_handler_path="/unexpectedError" on_error_error_type="MyError" on_error_bound_arg_count=1 provenance_handle=106
  type_metadata[0]: full_path="/Particle" category="struct" is_public=true has_no_padding=false has_platform_independent_padding=true has_explicit_alignment=true explicit_alignment_bytes=16 field_count=2 enum_value_count=0 provenance_handle=107 source="11:5"
  struct_field_metadata[0]: struct_path="/Particle" field_name="left" field_index=0 binding_type_text="i32" provenance_handle=108 source="12:7"
  struct_field_metadata[1]: struct_path="/Particle" field_name="right" field_index=1 binding_type_text="i64" provenance_handle=109 source="13:7"
  binding_facts[0]: scope_path="/main" site_kind="local" name="value" resolved_path="/main/value" binding_type_text="i32" is_mutable=true is_entry_arg_string=false is_unsafe_reference=false reference_root="" provenance_handle=110 source="12:7"
  return_facts[0]: definition_path="/main" return_kind="return" struct_path="/i32" binding_type_text="i32" is_mutable=false is_entry_arg_string=false is_unsafe_reference=false reference_root="" provenance_handle=111 source="13:3"
  local_auto_facts[0]: scope_path="/main" binding_name="selected" binding_type_text="i32" initializer_resolved_path="/id" initializer_binding_type_text="i32" initializer_receiver_binding_type_text="" initializer_query_type_text="i32" initializer_result_has_value=false initializer_result_value_type="" initializer_result_error_type="" initializer_has_try=false initializer_try_operand_resolved_path="" initializer_try_operand_binding_type_text="" initializer_try_operand_receiver_binding_type_text="" initializer_try_operand_query_type_text="" initializer_try_value_type="" initializer_try_error_type="" initializer_try_context_return_kind="return" initializer_try_on_error_handler_path="" initializer_try_on_error_error_type="" initializer_try_on_error_bound_arg_count=0 provenance_handle=112 source="14:9"
  query_facts[0]: scope_path="/main" call_name="lookup" resolved_path="/lookup" query_type_text="Result<i32, MyError>" binding_type_text="Result<i32, MyError>" receiver_binding_type_text="" has_result_type=true result_type_has_value=true result_value_type="i32" result_error_type="MyError" provenance_handle=113 source="15:4"
  try_facts[0]: scope_path="/main" operand_resolved_path="/lookup" operand_binding_type_text="Result<i32, MyError>" operand_receiver_binding_type_text="" operand_query_type_text="Result<i32, MyError>" value_type="i32" error_type="MyError" context_return_kind="return" on_error_handler_path="/unexpectedError" on_error_error_type="MyError" on_error_bound_arg_count=1 provenance_handle=114 source="16:8"
  on_error_facts[0]: definition_path="/main" return_kind="return" handler_path="/unexpectedError" error_type="MyError" bound_arg_count=1 bound_arg_texts=["err"] return_result_has_value=true return_result_value_type="i32" return_result_error_type="MyError" provenance_handle=115
}
)";

  CHECK(primec::formatSemanticProgram(semanticProgram) == expected);
}

TEST_CASE("semantic product dump helper matches formatter output") {
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

  primec::testing::CompilePipelineBoundaryDumps dumps;
  std::string error;
  REQUIRE(primec::testing::captureSemanticBoundaryDumpsForTesting(source, "/main", dumps, error));
  CHECK(error.empty());

  auto program = parseProgram(source);
  primec::Semantics semantics;
  primec::SemanticProgram semanticProgram;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  REQUIRE(semantics.validate(program, "/main", error, defaults, defaults, {}, nullptr, false, &semanticProgram));
  CHECK(error.empty());

  CHECK(dumps.semanticProduct == primec::formatSemanticProgram(semanticProgram));
}

TEST_SUITE_END();
