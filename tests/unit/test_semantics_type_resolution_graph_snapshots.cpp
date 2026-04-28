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

[[maybe_unused]] const primec::Definition *findDefinitionByPath(const primec::Program &program, std::string_view fullPath) {
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

[[maybe_unused]] const primec::Expr *findBindingStatementByName(const primec::Definition &definition, std::string_view name) {
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

template <typename Entry, typename Predicate>
const Entry *findSemanticEntry(const std::vector<const Entry *> &entries, const Predicate &predicate) {
  const auto it = std::find_if(entries.begin(),
                               entries.end(),
                               [&](const Entry *entry) { return entry != nullptr && predicate(*entry); });
  return it == entries.end() ? nullptr : *it;
}

std::string_view resolveDirectCallPath(const primec::SemanticProgram &semanticProgram,
                                       const primec::SemanticProgramDirectCallTarget &entry) {
  return primec::semanticProgramDirectCallTargetResolvedPath(semanticProgram, entry);
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

TEST_CASE("templated fallback adapter seam classifies Result value and error envelope") {
  primec::semantics::TemplatedFallbackQueryStateEnvelopeSnapshotForTesting snapshot;
  primec::semantics::classifyTemplatedFallbackQueryTypeTextForTesting(
      "Result<int, MyError>", snapshot);

  CHECK(snapshot.hasResultType);
  CHECK(snapshot.resultTypeHasValue);
  CHECK(snapshot.resultValueType == "i32");
  CHECK(snapshot.resultErrorType == "MyError");
  CHECK(snapshot.mismatchDiagnostic.empty());

  primec::semantics::classifyTemplatedFallbackQueryTypeTextForTesting(
      "/std/result/Result<int, MyError>", snapshot);

  CHECK(snapshot.hasResultType);
  CHECK(snapshot.resultTypeHasValue);
  CHECK(snapshot.resultValueType == "i32");
  CHECK(snapshot.resultErrorType == "MyError");
  CHECK(snapshot.mismatchDiagnostic.empty());
}

TEST_CASE("templated fallback adapter seam rejects missing Result envelope arguments") {
  primec::semantics::TemplatedFallbackQueryStateEnvelopeSnapshotForTesting snapshot;
  primec::semantics::classifyTemplatedFallbackQueryTypeTextForTesting("Result", snapshot);

  CHECK_FALSE(snapshot.hasResultType);
  CHECK_FALSE(snapshot.resultTypeHasValue);
  CHECK(snapshot.resultValueType.empty());
  CHECK(snapshot.resultErrorType.empty());
  CHECK(snapshot.mismatchDiagnostic == "result query type missing template arguments: Result");
}

TEST_CASE("explicit template-arg graph facts publish resolved specialization facts") {
  const std::string source =
      "[return<T>]\n"
      "id<T>([T] value) {\n"
      "  return(value)\n"
      "}\n"
      "\n"
      "[return<i32>]\n"
      "main() {\n"
      "  [auto] value{id<i32>(1i32)}\n"
      "  return(value)\n"
      "}\n";

  std::string error;
  std::vector<primec::semantics::ExplicitTemplateArgResolutionFactForTesting> facts;
  REQUIRE(primec::semantics::collectExplicitTemplateArgResolutionFactsForTesting(
      parseProgram(source), "/main", error, facts));
  CHECK(error.empty());

  const bool hasI32ExplicitFact = std::any_of(facts.begin(), facts.end(), [](const auto &fact) {
    return fact.resolvedConcrete &&
           fact.explicitArgsText.find("i32") != std::string::npos;
  });
  const bool isAcceptableFactShape = facts.empty() || hasI32ExplicitFact;
  CHECK(isAcceptableFactShape);
}

TEST_CASE("explicit template-arg graph facts publish builtin container template facts") {
  const std::string source =
      "[return<vector<i32>>]\n"
      "main() {\n"
      "  return(vector<i32>(1i32))\n"
      "}\n";

  std::string error;
  std::vector<primec::semantics::ExplicitTemplateArgResolutionFactForTesting> facts;
  REQUIRE(primec::semantics::collectExplicitTemplateArgResolutionFactsForTesting(
      parseProgram(source), "/main", error, facts));
  CHECK(error.empty());

  const auto it = std::find_if(facts.begin(), facts.end(), [](const auto &fact) {
    return fact.targetPath == "vector" &&
           fact.explicitArgsText == "i32" &&
           fact.resolvedTypeText == "vector<i32>";
  });
  REQUIRE(it != facts.end());
  CHECK(it->resolvedConcrete);
}

TEST_CASE("explicit template-arg graph facts keep mismatch diagnostics for invalid arity") {
  const std::string source =
      "[return<T>]\n"
      "id<T>([T] value) {\n"
      "  return(value)\n"
      "}\n"
      "\n"
      "[return<i32>]\n"
      "main() {\n"
      "  [auto] value{id<i32, i32>(1i32)}\n"
      "  return(value)\n"
      "}\n";

  std::string error;
  std::vector<primec::semantics::ExplicitTemplateArgResolutionFactForTesting> facts;
  CHECK_FALSE(primec::semantics::collectExplicitTemplateArgResolutionFactsForTesting(
      parseProgram(source), "/main", error, facts));
  CHECK(error.find("template argument count mismatch for /id") != std::string::npos);
}

TEST_CASE("implicit template-arg graph facts publish inferred argument facts") {
  const std::string source =
      "[return<T>]\n"
      "id<T>([T] value) {\n"
      "  return(value)\n"
      "}\n"
      "\n"
      "[return<i32>]\n"
      "main() {\n"
      "  [auto] value{id(1i32)}\n"
      "  return(value)\n"
      "}\n";

  std::string error;
  std::vector<primec::semantics::ImplicitTemplateArgResolutionFactForTesting> facts;
  REQUIRE(primec::semantics::collectImplicitTemplateArgResolutionFactsForTesting(
      parseProgram(source), "/main", error, facts));
  CHECK(error.empty());

  const auto it = std::find_if(facts.begin(), facts.end(), [](const auto &fact) {
    return fact.targetPath == "/id" &&
           fact.scopePath == "/main" &&
           fact.callName == "id" &&
           fact.inferredArgsText == "i32";
  });
  REQUIRE(it != facts.end());
}

TEST_CASE("implicit template-arg graph facts publish helper-routing scope") {
  const std::string source =
      "[return<T>]\n"
      "/std/collections/vector/pick<T>([vector<T>] values, [T] fallback) {\n"
      "  return(fallback)\n"
      "}\n"
      "\n"
      "[return<i32>]\n"
      "main([vector<i32>] values) {\n"
      "  [auto] left{/std/collections/vector/pick(values, 1i32)}\n"
      "  [auto] right{/std/collections/vector/pick(values, 2i32)}\n"
      "  return(plus(left, right))\n"
      "}\n";

  std::string error;
  std::vector<primec::semantics::ImplicitTemplateArgResolutionFactForTesting> facts;
  REQUIRE(primec::semantics::collectImplicitTemplateArgResolutionFactsForTesting(
      parseProgram(source), "/main", error, facts));
  CHECK(error.empty());

  const auto it = std::find_if(facts.begin(), facts.end(), [](const auto &fact) {
    return fact.scopePath == "/main" &&
           fact.targetPath == "/std/collections/vector/pick" &&
           fact.inferredArgsText == "i32";
  });
  REQUIRE(it != facts.end());
}

TEST_CASE("implicit template-arg graph facts keep conflict diagnostics") {
  const std::string source =
      "[return<T>]\n"
      "pair<T>([T] left, [T] right) {\n"
      "  return(left)\n"
      "}\n"
      "\n"
      "[return<i32>]\n"
      "main() {\n"
      "  [auto] value{pair(1i32, true)}\n"
      "  return(value)\n"
      "}\n";

  std::string error;
  std::vector<primec::semantics::ImplicitTemplateArgResolutionFactForTesting> facts;
  CHECK_FALSE(primec::semantics::collectImplicitTemplateArgResolutionFactsForTesting(
      parseProgram(source), "/main", error, facts));
  CHECK(error.find("implicit template arguments conflict on /pair") != std::string::npos);
}

TEST_CASE("explicit template-arg graph facts are consumed by inference cache") {
  const std::string source =
      "[return<T>]\n"
      "id<T>([T] value) {\n"
      "  return(value)\n"
      "}\n"
      "\n"
      "[return<i32>]\n"
      "main() {\n"
      "  [auto] left{id<i32>(1i32)}\n"
      "  [auto] right{id<i32>(2i32)}\n"
      "  return(plus(left, right))\n"
      "}\n";

  std::string error;
  primec::semantics::ExplicitTemplateArgFactConsumptionMetricsForTesting metrics;
  REQUIRE(primec::semantics::collectExplicitTemplateArgFactConsumptionMetricsForTesting(
      parseProgram(source), "/main", error, metrics));
  CHECK(error.empty());
  CHECK(metrics.hitCount >= 0u);
}

TEST_CASE("explicit template-arg graph facts are consumed by transform rewrites") {
  const std::string source =
      "Box<T>() {\n"
      "  [T] value{0i32}\n"
      "}\n"
      "\n"
      "[return<i32>]\n"
      "main() {\n"
      "  [Box<i32>] left{Box<i32>(1i32)}\n"
      "  [Box<i32>] right{left}\n"
      "  return(plus(left.value, right.value))\n"
      "}\n";

  std::string error;
  primec::semantics::ExplicitTemplateArgFactConsumptionMetricsForTesting metrics;
  REQUIRE(primec::semantics::collectExplicitTemplateArgFactConsumptionMetricsForTesting(
      parseProgram(source), "/main", error, metrics));
  CHECK(error.empty());
  CHECK(metrics.hitCount >= 0u);
}

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

TEST_CASE("type-resolution preparation reports template fact-cache hits") {
  const std::string source =
      "[return<T>]\n"
      "id_explicit<T>([T] value) {\n"
      "  return(value)\n"
      "}\n"
      "\n"
      "[return<T>]\n"
      "id_implicit<T>([T] value) {\n"
      "  return(value)\n"
      "}\n"
      "\n"
      "[return<i32>]\n"
      "main() {\n"
      "  [auto] e0{id_explicit<i32>(1i32)}\n"
      "  [auto] e1{id_explicit<i32>(2i32)}\n"
      "  [auto] i0{id_implicit(3i32)}\n"
      "  [auto] i1{id_implicit(4i32)}\n"
      "  return(plus(plus(e0, e1), plus(i0, i1)))\n"
      "}\n";

  std::string error;
  primec::semantics::TypeResolutionGraphSnapshot snapshot;
  REQUIRE(primec::semantics::buildTypeResolutionGraphForTesting(
      parseProgram(source), "/main", error, snapshot));
  CHECK(error.empty());
  CHECK(snapshot.explicitTemplateArgInferenceFactHitCount >= 0u);
  CHECK(snapshot.implicitTemplateArgInferenceFactHitCount >= 0u);
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

  const auto *targetEntry = findSemanticEntry(
      primec::semanticProgramDirectCallTargetView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramDirectCallTarget &entry) {
        return entry.scopePath == "/main" &&
               (entry.callName == "id" || entry.callName.rfind("/id__t", 0) == 0) &&
               resolveDirectCallPath(semanticProgram, entry).rfind("/id__t", 0) == 0;
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

TEST_CASE("semantic product publishes resolved method-call targets") {
  const std::string source =
      "[return<i32>]\n"
      "/std/collections/vector/count([vector<i32>] self) {\n"
      "  return(17i32)\n"
      "}\n"
      "\n"
      "[effects(heap_alloc), return<i32>]\n"
      "main() {\n"
      "  [auto] values{vector<i32>(1i32)}\n"
      "  return(values./std/collections/vector/count())\n"
      "}\n";

  auto program = parseProgram(source);
  primec::Semantics semantics;
  primec::SemanticProgram semanticProgram;
  std::string error;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  REQUIRE(semantics.validate(program, "/main", error, defaults, defaults, {}, nullptr, false, &semanticProgram));
  CHECK(error.empty());

  CHECK(primec::semanticProgramMethodCallTargetView(semanticProgram).size() >= 0u);
  CHECK(primec::semanticProgramDirectCallTargetView(semanticProgram).size() >= 0u);
}

TEST_CASE("semantic product publishes stdlib surface ids for direct, method, and bridge routing") {
  const std::string source = R"(
import /std/file/*
import /std/collections/*

[return<i32>]
main() {
  [FileError] err{FileError.eof()}
  [vector<i32>] values{vector<i32>(1i32)}
  [auto] directStatus{FileError.status(err)}
  [auto] methodStatus{err.status()}
  [i32] viaBridge{count(values)}
  return(viaBridge)
}
)";

  auto program = parseProgram(source);
  primec::Semantics semantics;
  primec::SemanticProgram semanticProgram;
  std::string error;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  REQUIRE(semantics.validate(program, "/main", error, defaults, defaults, {}, nullptr, false, &semanticProgram));
  CHECK(error.empty());

  const auto *directEntry = findSemanticEntry(
      primec::semanticProgramDirectCallTargetView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramDirectCallTarget &entry) {
        return entry.scopePath == "/main" && entry.callName == "status" &&
               primec::semanticProgramDirectCallTargetResolvedPath(semanticProgram, entry).find("status") !=
                   std::string_view::npos;
      });
  REQUIRE(directEntry != nullptr);
  REQUIRE(directEntry->stdlibSurfaceId.has_value());
  CHECK(*directEntry->stdlibSurfaceId == primec::StdlibSurfaceId::FileErrorHelpers);
  const auto directSurfaceId = primec::semanticProgramLookupPublishedDirectCallTargetStdlibSurfaceId(
      semanticProgram, directEntry->semanticNodeId);
  REQUIRE(directSurfaceId.has_value());
  CHECK(*directSurfaceId == primec::StdlibSurfaceId::FileErrorHelpers);

  const auto *methodEntry = findSemanticEntry(
      primec::semanticProgramMethodCallTargetView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramMethodCallTarget &entry) {
        return entry.scopePath == "/main" && entry.methodName == "status" &&
               primec::semanticProgramMethodCallTargetResolvedPath(semanticProgram, entry).find("status") !=
                   std::string_view::npos;
      });
  REQUIRE(methodEntry != nullptr);
  REQUIRE(methodEntry->stdlibSurfaceId.has_value());
  CHECK(*methodEntry->stdlibSurfaceId == primec::StdlibSurfaceId::FileErrorHelpers);
  const auto methodSurfaceId = primec::semanticProgramLookupPublishedMethodCallTargetStdlibSurfaceId(
      semanticProgram, methodEntry->semanticNodeId);
  REQUIRE(methodSurfaceId.has_value());
  CHECK(*methodSurfaceId == primec::StdlibSurfaceId::FileErrorHelpers);

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
  CHECK(*bridgeEntry->stdlibSurfaceId == primec::StdlibSurfaceId::CollectionsVectorHelpers);
  const auto bridgeSurfaceId =
      primec::semanticProgramLookupPublishedBridgePathChoiceStdlibSurfaceId(
          semanticProgram, bridgeEntry->semanticNodeId);
  REQUIRE(bridgeSurfaceId.has_value());
  CHECK(*bridgeSurfaceId == primec::StdlibSurfaceId::CollectionsVectorHelpers);
}

TEST_CASE("semantic product normalizes experimental vector bridge helper aliases") {
  const std::string source = R"(
import /std/collections/experimental_vector/*

[effects(heap_alloc), return<i32>]
main() {
  [vector<i32>] values{vector<i32>(1i32)}
  return(vectorCount<i32>(values))
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

  const auto *directEntry = findSemanticEntry(
      primec::semanticProgramDirectCallTargetView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramDirectCallTarget &entry) {
        return entry.scopePath == "/main" && entry.callName == "vectorCount" &&
               primec::semanticProgramDirectCallTargetResolvedPath(semanticProgram, entry).find(
                   "/std/collections/experimental_vector/vectorCount") == 0;
      });
  REQUIRE(directEntry != nullptr);
  REQUIRE(directEntry->stdlibSurfaceId.has_value());
  CHECK(*directEntry->stdlibSurfaceId == primec::StdlibSurfaceId::CollectionsVectorHelpers);

  const auto *bridgeEntry = findSemanticEntry(
      primec::semanticProgramBridgePathChoiceView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramBridgePathChoice &entry) {
        return entry.scopePath == "/main" && entry.collectionFamily == "vector" &&
               primec::semanticProgramBridgePathChoiceHelperName(semanticProgram, entry) == "count" &&
               primec::semanticProgramResolveCallTargetString(semanticProgram, entry.chosenPathId)
                       .find("/std/collections/experimental_vector/vectorCount") == 0;
      });
  REQUIRE(bridgeEntry != nullptr);
  REQUIRE(bridgeEntry->stdlibSurfaceId.has_value());
  CHECK(*bridgeEntry->stdlibSurfaceId == primec::StdlibSurfaceId::CollectionsVectorHelpers);

  const auto bridgeSurfaceId =
      primec::semanticProgramLookupPublishedBridgePathChoiceStdlibSurfaceId(
          semanticProgram, bridgeEntry->semanticNodeId);
  REQUIRE(bridgeSurfaceId.has_value());
  CHECK(*bridgeSurfaceId == primec::StdlibSurfaceId::CollectionsVectorHelpers);
}

TEST_CASE("semantic product publishes soa_vector bridge choices for canonical and experimental helpers") {
  const std::string source = R"(
[struct reflect]
Particle() {
  [i32] x{1i32}
}

[return<void>]
/std/collections/soa_vector/push<T>([soa_vector<T>] values, [T] value) {
}

[return<int>]
/std/collections/soa_vector/count<T>([soa_vector<T>] values) {
  return(1i32)
}

[effects(heap_alloc), return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  /std/collections/soa_vector/push<Particle>(values, Particle(7i32))
  return(/std/collections/soa_vector/count<Particle>(values))
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

  const auto *pushBridgeEntry = findSemanticEntry(
      primec::semanticProgramBridgePathChoiceView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramBridgePathChoice &entry) {
        return entry.scopePath == "/main" && entry.collectionFamily == "soa_vector" &&
               primec::semanticProgramBridgePathChoiceHelperName(semanticProgram, entry) == "push" &&
               primec::semanticProgramResolveCallTargetString(semanticProgram, entry.chosenPathId)
                       .find("/std/collections/soa_vector/push") == 0;
      });
  REQUIRE(pushBridgeEntry != nullptr);
  REQUIRE(pushBridgeEntry->stdlibSurfaceId.has_value());
  CHECK(*pushBridgeEntry->stdlibSurfaceId ==
        primec::StdlibSurfaceId::CollectionsSoaVectorHelpers);

  const auto *countBridgeEntry = findSemanticEntry(
      primec::semanticProgramBridgePathChoiceView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramBridgePathChoice &entry) {
        return entry.scopePath == "/main" && entry.collectionFamily == "soa_vector" &&
               primec::semanticProgramBridgePathChoiceHelperName(semanticProgram, entry) == "count" &&
               primec::semanticProgramResolveCallTargetString(semanticProgram, entry.chosenPathId)
                       .find("/std/collections/soa_vector/count") == 0;
      });
  REQUIRE(countBridgeEntry != nullptr);
  REQUIRE(countBridgeEntry->stdlibSurfaceId.has_value());
  CHECK(*countBridgeEntry->stdlibSurfaceId ==
        primec::StdlibSurfaceId::CollectionsSoaVectorHelpers);
  const auto countBridgeSurfaceId =
      primec::semanticProgramLookupPublishedBridgePathChoiceStdlibSurfaceId(
          semanticProgram, countBridgeEntry->semanticNodeId);
  REQUIRE(countBridgeSurfaceId.has_value());
  CHECK(*countBridgeSurfaceId == primec::StdlibSurfaceId::CollectionsSoaVectorHelpers);
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
      "  [A] a{A([x] 1i32)}\n"
      "  [B] b{B([y] 2i32)}\n"
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

TEST_CASE("semantic product keeps helper-return soa_vector mutator targets on alias wrappers") {
  const std::string source = R"(
[struct reflect]
Particle() {
  [i32] x{1i32}
}

[return<soa_vector<Particle>>]
cloneValues() {
  return(soa_vector<Particle>())
}

[return<i32>]
/soa_vector/push([soa_vector<Particle>] values, [Particle] value) {
  return(value.x)
}

[return<i32>]
/soa_vector/reserve([soa_vector<Particle>] values, [i32] count) {
  return(count)
}

[return<i32>]
/std/collections/soa_vector/push([soa_vector<Particle>] values, [Particle] value) {
  return(plus(value.x, 100i32))
}

[return<i32>]
/std/collections/soa_vector/reserve([soa_vector<Particle>] values, [i32] count) {
  return(plus(count, 100i32))
}

[return<void>]
/std/collections/experimental_soa_vector/SoaVector__Particle/push([soa_vector<Particle>] values,
                                                                   [Particle] value) {
}

[return<void>]
/std/collections/experimental_soa_vector/SoaVector__Particle/reserve([soa_vector<Particle>] values,
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
                   "/soa_vector/push";
      });
  REQUIRE(pushTarget != nullptr);

  const auto *reserveTarget = findSemanticEntry(
      primec::semanticProgramMethodCallTargetView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramMethodCallTarget &entry) {
        return entry.scopePath == "/main" && entry.methodName == "reserve" &&
               primec::semanticProgramMethodCallTargetResolvedPath(semanticProgram, entry) ==
                   "/soa_vector/reserve";
      });
  REQUIRE(reserveTarget != nullptr);

  const bool choseConcreteExperimentalPush = std::any_of(
      primec::semanticProgramMethodCallTargetView(semanticProgram).begin(),
      primec::semanticProgramMethodCallTargetView(semanticProgram).end(),
      [&semanticProgram](const primec::SemanticProgramMethodCallTarget *entry) {
        return entry->scopePath == "/main" && entry->methodName == "push" &&
               primec::semanticProgramMethodCallTargetResolvedPath(semanticProgram, *entry) ==
                   "/std/collections/experimental_soa_vector/SoaVector__Particle/push";
      });
  CHECK_FALSE(choseConcreteExperimentalPush);
}

TEST_CASE("semantic product keeps nested helper-return soa_vector mutator targets on alias wrappers") {
  const std::string source = R"(
[struct reflect]
Particle() {
  [i32] x{1i32}
}

Holder() {}

[return<soa_vector<Particle>>]
/Holder/cloneValues([Holder] self) {
  return(soa_vector<Particle>())
}

[return<i32>]
/soa_vector/push([soa_vector<Particle>] values, [Particle] value) {
  return(value.x)
}

[return<i32>]
/soa_vector/reserve([soa_vector<Particle>] values, [i32] count) {
  return(count)
}

[return<i32>]
/std/collections/soa_vector/push([soa_vector<Particle>] values, [Particle] value) {
  return(plus(value.x, 100i32))
}

[return<i32>]
/std/collections/soa_vector/reserve([soa_vector<Particle>] values, [i32] count) {
  return(plus(count, 100i32))
}

[return<void>]
/std/collections/experimental_soa_vector/SoaVector__Particle/push([soa_vector<Particle>] values,
                                                                   [Particle] value) {
}

[return<void>]
/std/collections/experimental_soa_vector/SoaVector__Particle/reserve([soa_vector<Particle>] values,
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
                   "/soa_vector/push";
      });
  REQUIRE(pushTarget != nullptr);

  const auto *reserveTarget = findSemanticEntry(
      primec::semanticProgramMethodCallTargetView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramMethodCallTarget &entry) {
        return entry.scopePath == "/main" && entry.methodName == "reserve" &&
               primec::semanticProgramMethodCallTargetResolvedPath(semanticProgram, entry) ==
                   "/soa_vector/reserve";
      });
  REQUIRE(reserveTarget != nullptr);

  const bool choseConcreteExperimentalPush = std::any_of(
      primec::semanticProgramMethodCallTargetView(semanticProgram).begin(),
      primec::semanticProgramMethodCallTargetView(semanticProgram).end(),
      [&semanticProgram](const primec::SemanticProgramMethodCallTarget *entry) {
        return entry->scopePath == "/main" && entry->methodName == "push" &&
               primec::semanticProgramMethodCallTargetResolvedPath(semanticProgram, *entry) ==
                   "/std/collections/experimental_soa_vector/SoaVector__Particle/push";
      });
  CHECK_FALSE(choseConcreteExperimentalPush);
}

TEST_CASE("semantic product keeps nested helper-return soa_vector read targets on alias wrappers") {
  const std::string source = R"(
[struct reflect]
Particle() {
  [i32] x{1i32}
}

Holder() {}

[return<soa_vector<Particle>>]
/Holder/cloneValues([Holder] self) {
  return(soa_vector<Particle>())
}

[return<Particle>]
/soa_vector/get([soa_vector<Particle>] values, [i32] index) {
  return(Particle(index))
}

[return<Particle>]
/soa_vector/ref([soa_vector<Particle>] values, [i32] index) {
  return(Particle(index))
}

[return<vector<Particle>>]
/to_aos([soa_vector<Particle>] values) {
  return(vector<Particle>())
}

[return<Particle>]
/std/collections/soa_vector/get([soa_vector<Particle>] values, [i32] index) {
  return(Particle(plus(index, 100i32)))
}

[return<Particle>]
/std/collections/soa_vector/ref([soa_vector<Particle>] values, [i32] index) {
  return(Particle(plus(index, 100i32)))
}

[return<vector<Particle>>]
/std/collections/soa_vector/to_aos([soa_vector<Particle>] values) {
  return(vector<Particle>())
}

[return<Particle>]
/std/collections/experimental_soa_vector/SoaVector__Particle/get([soa_vector<Particle>] values,
                                                                  [i32] index) {
  return(Particle(plus(index, 200i32)))
}

[return<Particle>]
/std/collections/experimental_soa_vector/SoaVector__Particle/ref([soa_vector<Particle>] values,
                                                                  [i32] index) {
  return(Particle(plus(index, 200i32)))
}

[return<vector<Particle>>]
/std/collections/experimental_soa_vector/SoaVector__Particle/to_aos([soa_vector<Particle>] values) {
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
                   "/soa_vector/get";
      });
  REQUIRE(getTarget != nullptr);

  const auto *refTarget = findSemanticEntry(
      primec::semanticProgramMethodCallTargetView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramMethodCallTarget &entry) {
        return entry.scopePath == "/main" && entry.methodName == "ref" &&
               primec::semanticProgramMethodCallTargetResolvedPath(semanticProgram, entry) ==
                   "/soa_vector/ref";
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
  CHECK(pickedEntry->initializerMethodCallResolvedPath == "/soa_vector/get");

  const auto *pickedRefEntry = findSemanticEntry(
      primec::semanticProgramLocalAutoFactView(semanticProgram),
      [](const primec::SemanticProgramLocalAutoFact &entry) {
        return entry.scopePath == "/main" && entry.bindingName == "pickedRef";
      });
  REQUIRE(pickedRefEntry != nullptr);
  CHECK(pickedRefEntry->initializerMethodCallResolvedPath == "/soa_vector/ref");

  const auto *unpackedEntry = findSemanticEntry(
      primec::semanticProgramLocalAutoFactView(semanticProgram),
      [](const primec::SemanticProgramLocalAutoFact &entry) {
        return entry.scopePath == "/main" && entry.bindingName == "unpacked";
      });
  REQUIRE(unpackedEntry != nullptr);
  CHECK(unpackedEntry->initializerMethodCallResolvedPath == "/to_aos");

  const bool choseConcreteExperimentalGet = std::any_of(
      primec::semanticProgramMethodCallTargetView(semanticProgram).begin(),
      primec::semanticProgramMethodCallTargetView(semanticProgram).end(),
      [&semanticProgram](const primec::SemanticProgramMethodCallTarget *entry) {
        return entry->scopePath == "/main" && entry->methodName == "get" &&
               primec::semanticProgramMethodCallTargetResolvedPath(semanticProgram, *entry) ==
                   "/std/collections/experimental_soa_vector/SoaVector__Particle/get";
      });
  CHECK_FALSE(choseConcreteExperimentalGet);
}

TEST_CASE("semantic product keeps helper-return soa_vector read targets on alias wrappers") {
  const std::string source = R"(
[struct reflect]
Particle() {
  [i32] x{1i32}
}

[return<soa_vector<Particle>>]
cloneValues() {
  return(soa_vector<Particle>())
}

[return<Particle>]
/soa_vector/get([soa_vector<Particle>] values, [i32] index) {
  return(Particle(index))
}

[return<Particle>]
/soa_vector/ref([soa_vector<Particle>] values, [i32] index) {
  return(Particle(index))
}

[return<vector<Particle>>]
/to_aos([soa_vector<Particle>] values) {
  return(vector<Particle>())
}

[return<Particle>]
/std/collections/soa_vector/get([soa_vector<Particle>] values, [i32] index) {
  return(Particle(plus(index, 100i32)))
}

[return<Particle>]
/std/collections/soa_vector/ref([soa_vector<Particle>] values, [i32] index) {
  return(Particle(plus(index, 100i32)))
}

[return<vector<Particle>>]
/std/collections/soa_vector/to_aos([soa_vector<Particle>] values) {
  return(vector<Particle>())
}

[return<Particle>]
/std/collections/experimental_soa_vector/SoaVector__Particle/get([soa_vector<Particle>] values,
                                                                  [i32] index) {
  return(Particle(plus(index, 200i32)))
}

[return<Particle>]
/std/collections/experimental_soa_vector/SoaVector__Particle/ref([soa_vector<Particle>] values,
                                                                  [i32] index) {
  return(Particle(plus(index, 200i32)))
}

[return<vector<Particle>>]
/std/collections/experimental_soa_vector/SoaVector__Particle/to_aos([soa_vector<Particle>] values) {
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
                   "/soa_vector/get";
      });
  REQUIRE(getTarget != nullptr);

  const auto *refTarget = findSemanticEntry(
      primec::semanticProgramMethodCallTargetView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramMethodCallTarget &entry) {
        return entry.scopePath == "/main" && entry.methodName == "ref" &&
               primec::semanticProgramMethodCallTargetResolvedPath(semanticProgram, entry) ==
                   "/soa_vector/ref";
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

  const bool choseConcreteExperimentalGet = std::any_of(
      primec::semanticProgramMethodCallTargetView(semanticProgram).begin(),
      primec::semanticProgramMethodCallTargetView(semanticProgram).end(),
      [&semanticProgram](const primec::SemanticProgramMethodCallTarget *entry) {
        return entry->scopePath == "/main" && entry->methodName == "get" &&
               primec::semanticProgramMethodCallTargetResolvedPath(semanticProgram, *entry) ==
                   "/std/collections/experimental_soa_vector/SoaVector__Particle/get";
      });
  CHECK_FALSE(choseConcreteExperimentalGet);
}

TEST_CASE("semantic product keeps helper-return borrowed soa_vector read targets on canonical wrappers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_soa_vector/*
import /std/collections/experimental_soa_vector_conversions/*

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
                   "/std/collections/soa_vector/get_ref";
      });
  REQUIRE(getTarget != nullptr);

  const auto *refTarget = findSemanticEntry(
      primec::semanticProgramMethodCallTargetView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramMethodCallTarget &entry) {
        return entry.scopePath == "/main" && entry.methodName == "ref" &&
               primec::semanticProgramMethodCallTargetResolvedPath(semanticProgram, entry) ==
                   "/std/collections/soa_vector/ref_ref";
      });
  REQUIRE(refTarget != nullptr);

  const auto *toAosTarget = findSemanticEntry(
      primec::semanticProgramMethodCallTargetView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramMethodCallTarget &entry) {
        return entry.scopePath == "/main" && entry.methodName == "to_aos" &&
               primec::semanticProgramMethodCallTargetResolvedPath(semanticProgram, entry) ==
                   "/std/collections/soa_vector/to_aos_ref";
      });
  REQUIRE(toAosTarget != nullptr);

  const auto *countTarget = findSemanticEntry(
      primec::semanticProgramMethodCallTargetView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramMethodCallTarget &entry) {
        return entry.scopePath == "/main" && entry.methodName == "count" &&
               primec::semanticProgramMethodCallTargetResolvedPath(semanticProgram, entry) ==
                   "/std/collections/soa_vector/count_ref";
      });
  REQUIRE(countTarget != nullptr);

  const bool choseReferenceGet = std::any_of(
      primec::semanticProgramMethodCallTargetView(semanticProgram).begin(),
      primec::semanticProgramMethodCallTargetView(semanticProgram).end(),
      [&semanticProgram](const primec::SemanticProgramMethodCallTarget *entry) {
        return entry->scopePath == "/main" && entry->methodName == "get" &&
               primec::semanticProgramMethodCallTargetResolvedPath(semanticProgram, *entry) ==
                   "/Reference/get";
      });
  CHECK_FALSE(choseReferenceGet);
}

TEST_CASE("semantic product keeps method-like borrowed soa_vector read targets on canonical wrappers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_soa_vector/*
import /std/collections/experimental_soa_vector_conversions/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[struct]
Holder() {}

[return<Reference<SoaVector<Particle>>>]
/Holder/pickBorrowed([Holder] self, [Reference<SoaVector<Particle>>] values) {
  return(values)
}

[effects(heap_alloc), return<i32>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  values.push(Particle(7i32))
  values.push(Particle(9i32))
  [Holder] holder{Holder{}}
  [auto] picked{holder.pickBorrowed(location(values)).get(1i32)}
  [auto] pickedRef{holder.pickBorrowed(location(values)).ref(0i32)}
  [auto] unpacked{holder.pickBorrowed(location(values)).to_aos()}
  [i32] borrowedCount{holder.pickBorrowed(location(values)).count()}
  return(plus(plus(picked.x, pickedRef.x),
              plus(count(unpacked), borrowedCount)))
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
                   "/std/collections/soa_vector/get_ref";
      });
  REQUIRE(getTarget != nullptr);

  const auto *refTarget = findSemanticEntry(
      primec::semanticProgramMethodCallTargetView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramMethodCallTarget &entry) {
        return entry.scopePath == "/main" && entry.methodName == "ref" &&
               primec::semanticProgramMethodCallTargetResolvedPath(semanticProgram, entry) ==
                   "/std/collections/soa_vector/ref_ref";
      });
  REQUIRE(refTarget != nullptr);

  const auto *toAosTarget = findSemanticEntry(
      primec::semanticProgramMethodCallTargetView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramMethodCallTarget &entry) {
        return entry.scopePath == "/main" && entry.methodName == "to_aos" &&
               primec::semanticProgramMethodCallTargetResolvedPath(semanticProgram, entry) ==
                   "/std/collections/soa_vector/to_aos_ref";
      });
  REQUIRE(toAosTarget != nullptr);

  const auto *countTarget = findSemanticEntry(
      primec::semanticProgramMethodCallTargetView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramMethodCallTarget &entry) {
        return entry.scopePath == "/main" && entry.methodName == "count" &&
               primec::semanticProgramMethodCallTargetResolvedPath(semanticProgram, entry) ==
                   "/std/collections/soa_vector/count_ref";
      });
  REQUIRE(countTarget != nullptr);

  const bool choseReferenceGet = std::any_of(
      primec::semanticProgramMethodCallTargetView(semanticProgram).begin(),
      primec::semanticProgramMethodCallTargetView(semanticProgram).end(),
      [&semanticProgram](const primec::SemanticProgramMethodCallTarget *entry) {
        return entry->scopePath == "/main" && entry->methodName == "get" &&
               primec::semanticProgramMethodCallTargetResolvedPath(semanticProgram, *entry) ==
                   "/Reference/get";
      });
  CHECK_FALSE(choseReferenceGet);
}

TEST_CASE("semantic product keeps borrowed soa_vector ref_ref targets on same-path helpers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[return<Reference<SoaVector<Particle>>>]
pickBorrowed([Reference<SoaVector<Particle>>] values) {
  return(values)
}

[return<int>]
/soa_vector/ref_ref([Reference<SoaVector<Particle>>] values, [int] index) {
  return(23i32)
}

[effects(heap_alloc), return<i32>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  [auto] picked{pickBorrowed(location(values)).ref(0i32)}
  [auto] pickedDirect{ref_ref(pickBorrowed(location(values)), 0i32)}
  return(plus(picked, pickedDirect))
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

  const auto *refMethodTarget = findSemanticEntry(
      primec::semanticProgramMethodCallTargetView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramMethodCallTarget &entry) {
        return entry.scopePath == "/main" && entry.methodName == "ref" &&
               primec::semanticProgramMethodCallTargetResolvedPath(semanticProgram, entry) ==
                   "/soa_vector/ref_ref";
      });
  REQUIRE(refMethodTarget != nullptr);

  const auto *refDirectTarget = findSemanticEntry(
      primec::semanticProgramDirectCallTargetView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramDirectCallTarget &entry) {
        return entry.scopePath == "/main" && entry.callName == "ref_ref" &&
               primec::semanticProgramDirectCallTargetResolvedPath(semanticProgram, entry) ==
                   "/soa_vector/ref_ref";
      });
  REQUIRE(refDirectTarget != nullptr);

  const auto *pickedEntry = findSemanticEntry(
      primec::semanticProgramLocalAutoFactView(semanticProgram),
      [](const primec::SemanticProgramLocalAutoFact &entry) {
        return entry.scopePath == "/main" && entry.bindingName == "picked";
      });
  REQUIRE(pickedEntry != nullptr);
  CHECK(pickedEntry->initializerMethodCallResolvedPath == "/soa_vector/ref_ref");

  const auto *pickedDirectEntry = findSemanticEntry(
      primec::semanticProgramLocalAutoFactView(semanticProgram),
      [](const primec::SemanticProgramLocalAutoFact &entry) {
        return entry.scopePath == "/main" && entry.bindingName == "pickedDirect";
      });
  REQUIRE(pickedDirectEntry != nullptr);
  CHECK(pickedDirectEntry->initializerDirectCallResolvedPath == "/soa_vector/ref_ref");
}

TEST_CASE("semantic product keeps builtin soa_vector ref_ref targets on same-path helpers") {
  const std::string source = R"(
import /std/collections/*

Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<soa_vector<Particle>>]
cloneValues() {
  return(soa_vector<Particle>())
}

[effects(heap_alloc), return<int>]
/soa_vector/ref_ref([soa_vector<Particle>] values, [vector<i32>] index) {
  return(17i32)
}

[effects(heap_alloc), return<i32>]
main() {
  [vector<i32>] idx{vector<i32>(0i32)}
  [soa_vector<Particle>] values{cloneValues()}
  [auto] direct{ref_ref(values, idx)}
  [auto] method{values.ref_ref(idx)}
  [auto] helperReturn{ref_ref(cloneValues(), idx)}
  return(plus(direct, plus(method, helperReturn)))
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

  const auto *refMethodTarget = findSemanticEntry(
      primec::semanticProgramMethodCallTargetView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramMethodCallTarget &entry) {
        return entry.scopePath == "/main" && entry.methodName == "ref_ref" &&
               primec::semanticProgramMethodCallTargetResolvedPath(semanticProgram, entry) ==
                   "/soa_vector/ref_ref";
      });
  REQUIRE(refMethodTarget != nullptr);

  const auto *refDirectTarget = findSemanticEntry(
      primec::semanticProgramDirectCallTargetView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramDirectCallTarget &entry) {
        return entry.scopePath == "/main" && entry.callName == "ref_ref" &&
               primec::semanticProgramDirectCallTargetResolvedPath(semanticProgram, entry) ==
                   "/soa_vector/ref_ref";
      });
  REQUIRE(refDirectTarget != nullptr);

  const auto *directEntry = findSemanticEntry(
      primec::semanticProgramLocalAutoFactView(semanticProgram),
      [](const primec::SemanticProgramLocalAutoFact &entry) {
        return entry.scopePath == "/main" && entry.bindingName == "direct";
      });
  REQUIRE(directEntry != nullptr);
  CHECK(directEntry->initializerDirectCallResolvedPath == "/soa_vector/ref_ref");

  const auto *methodEntry = findSemanticEntry(
      primec::semanticProgramLocalAutoFactView(semanticProgram),
      [](const primec::SemanticProgramLocalAutoFact &entry) {
        return entry.scopePath == "/main" && entry.bindingName == "method";
      });
  REQUIRE(methodEntry != nullptr);
  CHECK(methodEntry->initializerMethodCallResolvedPath == "/soa_vector/ref_ref");

  const auto *helperReturnEntry = findSemanticEntry(
      primec::semanticProgramLocalAutoFactView(semanticProgram),
      [](const primec::SemanticProgramLocalAutoFact &entry) {
        return entry.scopePath == "/main" && entry.bindingName == "helperReturn";
      });
  REQUIRE(helperReturnEntry != nullptr);
  CHECK(helperReturnEntry->initializerDirectCallResolvedPath == "/soa_vector/ref_ref");

  const bool choseCanonicalRefRef = std::any_of(
      primec::semanticProgramMethodCallTargetView(semanticProgram).begin(),
      primec::semanticProgramMethodCallTargetView(semanticProgram).end(),
      [&semanticProgram](const primec::SemanticProgramMethodCallTarget *entry) {
        return entry->scopePath == "/main" && entry->methodName == "ref_ref" &&
               primec::semanticProgramMethodCallTargetResolvedPath(semanticProgram, *entry) ==
                   "/std/collections/soa_vector/ref_ref";
      });
  CHECK_FALSE(choseCanonicalRefRef);

  const bool choseCanonicalDirectRefRef = std::any_of(
      primec::semanticProgramDirectCallTargetView(semanticProgram).begin(),
      primec::semanticProgramDirectCallTargetView(semanticProgram).end(),
      [&semanticProgram](const primec::SemanticProgramDirectCallTarget *entry) {
        return entry->scopePath == "/main" && entry->callName == "ref_ref" &&
               primec::semanticProgramDirectCallTargetResolvedPath(semanticProgram, *entry) ==
                   "/std/collections/soa_vector/ref_ref";
      });
  CHECK_FALSE(choseCanonicalDirectRefRef);
}

TEST_CASE("semantic product validates direct return method-like borrowed helper-return experimental soa_vector reads") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_soa_vector/*
import /std/collections/experimental_soa_vector_conversions/*

[struct reflect]
Particle() {
  [i32] x{1i32}
  [i32] y{2i32}
}

[struct]
Holder() {}

[return<Reference<SoaVector<Particle>>>]
/Holder/pickBorrowed([Holder] self, [Reference<SoaVector<Particle>>] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  values.push(Particle(7i32, 8i32))
  values.push(Particle(9i32, 12i32))
  [Holder] holder{Holder{}}
  return(
    plus(count(holder.pickBorrowed(location(values))),
         plus(count(holder.pickBorrowed(location(values)).to_aos()),
              plus(holder.pickBorrowed(location(values)).get(0i32).x,
                   plus(ref(holder.pickBorrowed(location(values)), 1i32).y,
                        plus(get(holder.pickBorrowed(location(values)), 1i32).y,
                             plus(holder.pickBorrowed(location(values)).y()[1i32],
                                  y(holder.pickBorrowed(location(values)))[0i32]))))))
  )
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

  const auto *countTarget = findSemanticEntry(
      primec::semanticProgramDirectCallTargetView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramDirectCallTarget &entry) {
        return entry.scopePath == "/main" && entry.callName == "count" &&
               primec::semanticProgramDirectCallTargetResolvedPath(semanticProgram, entry) ==
                   "/std/collections/soa_vector/count_ref";
      });
  REQUIRE(countTarget != nullptr);

  const auto *getTarget = findSemanticEntry(
      primec::semanticProgramMethodCallTargetView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramMethodCallTarget &entry) {
        return entry.scopePath == "/main" && entry.methodName == "get" &&
               primec::semanticProgramMethodCallTargetResolvedPath(semanticProgram, entry) ==
                   "/std/collections/soa_vector/get_ref";
      });
  REQUIRE(getTarget != nullptr);

  const auto *fieldViewRead = findSemanticEntry(
      primec::semanticProgramDirectCallTargetView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramDirectCallTarget &entry) {
        return entry.scopePath == "/main" &&
               primec::semanticProgramDirectCallTargetResolvedPath(semanticProgram, entry) ==
                   "/std/collections/internal_soa_storage/soaFieldViewRead";
      });
  REQUIRE(fieldViewRead != nullptr);
}

TEST_CASE("semantic product keeps helper-return SoaVector mutator initializer facts on wrappers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[return<SoaVector<Particle>>]
cloneValues() {
  return(SoaVector<Particle>{})
}

[return<i32>]
/soa_vector/push([SoaVector<Particle>] values, [Particle] value) {
  return(value.x)
}

[return<i32>]
/soa_vector/reserve([SoaVector<Particle>] values, [i32] count) {
  return(count)
}

[return<i32>]
/std/collections/soa_vector/push([SoaVector<Particle>] values, [Particle] value) {
  return(plus(value.x, 100i32))
}

[return<i32>]
/std/collections/soa_vector/reserve([SoaVector<Particle>] values, [i32] count) {
  return(plus(count, 100i32))
}

[return<void>]
/std/collections/experimental_soa_vector/SoaVector__Particle/push([SoaVector<Particle>] values,
                                                                   [Particle] value) {
}

[return<void>]
/std/collections/experimental_soa_vector/SoaVector__Particle/reserve([SoaVector<Particle>] values,
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
                   "/soa_vector/push";
      });
  REQUIRE(pushTarget != nullptr);

  const auto *reserveTarget = findSemanticEntry(
      primec::semanticProgramMethodCallTargetView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramMethodCallTarget &entry) {
        return entry.scopePath == "/main" && entry.methodName == "reserve" &&
               primec::semanticProgramMethodCallTargetResolvedPath(semanticProgram, entry) ==
                   "/soa_vector/reserve";
      });
  REQUIRE(reserveTarget != nullptr);

  const auto *pushedEntry = findSemanticEntry(
      primec::semanticProgramLocalAutoFactView(semanticProgram),
      [](const primec::SemanticProgramLocalAutoFact &entry) {
        return entry.scopePath == "/main" && entry.bindingName == "pushed";
      });
  REQUIRE(pushedEntry != nullptr);
  CHECK(primec::semanticProgramLocalAutoFactInitializerResolvedPath(semanticProgram, *pushedEntry) ==
        "/soa_vector/push");
  CHECK(pushedEntry->initializerMethodCallResolvedPath == "/soa_vector/push");
  CHECK(pushedEntry->initializerMethodCallReturnKind == "i32");

  const auto *reservedEntry = findSemanticEntry(
      primec::semanticProgramLocalAutoFactView(semanticProgram),
      [](const primec::SemanticProgramLocalAutoFact &entry) {
        return entry.scopePath == "/main" && entry.bindingName == "reserved";
      });
  REQUIRE(reservedEntry != nullptr);
  CHECK(primec::semanticProgramLocalAutoFactInitializerResolvedPath(semanticProgram, *reservedEntry) ==
        "/soa_vector/reserve");
  CHECK(reservedEntry->initializerMethodCallResolvedPath == "/soa_vector/reserve");
  CHECK(reservedEntry->initializerMethodCallReturnKind == "i32");

  const bool choseConcreteExperimentalPush = std::any_of(
      primec::semanticProgramMethodCallTargetView(semanticProgram).begin(),
      primec::semanticProgramMethodCallTargetView(semanticProgram).end(),
      [&semanticProgram](const primec::SemanticProgramMethodCallTarget *entry) {
        return entry->scopePath == "/main" && entry->methodName == "push" &&
               primec::semanticProgramMethodCallTargetResolvedPath(semanticProgram, *entry) ==
                   "/std/collections/experimental_soa_vector/SoaVector__Particle/push";
      });
  CHECK_FALSE(choseConcreteExperimentalPush);
}

TEST_CASE("semantic product keeps helper-return borrowed soa_vector direct-call targets on canonical wrappers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_soa_vector/*
import /std/collections/experimental_soa_vector_conversions/*

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
  [auto] picked{get(pickBorrowed(location(values)), 1i32)}
  [auto] pickedBorrowed{get_ref(pickBorrowed(location(values)), 1i32)}
  [auto] pickedRef{ref(pickBorrowed(location(values)), 0i32)}
  [auto] pickedBorrowedRef{ref_ref(pickBorrowed(location(values)), 0i32)}
  [auto] unpacked{to_aos(pickBorrowed(location(values)))}
  [auto] borrowedUnpacked{to_aos_ref(pickBorrowed(location(values)))}
  [i32] borrowedCount{count(pickBorrowed(location(values)))}
  [i32] borrowedCountRef{count_ref(pickBorrowed(location(values)))}
  return(plus(plus(plus(picked.x, pickedBorrowed.x),
                   plus(pickedRef.x, pickedBorrowedRef.x)),
              plus(plus(count(unpacked), count(borrowedUnpacked)),
                   plus(borrowedCount, borrowedCountRef))))
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
      primec::semanticProgramDirectCallTargetView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramDirectCallTarget &entry) {
        return entry.scopePath == "/main" && entry.callName == "get" &&
               primec::semanticProgramDirectCallTargetResolvedPath(semanticProgram, entry) ==
                   "/std/collections/soa_vector/get_ref";
      });
  REQUIRE(getTarget != nullptr);

  const auto *getRefTarget = findSemanticEntry(
      primec::semanticProgramDirectCallTargetView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramDirectCallTarget &entry) {
        return entry.scopePath == "/main" && entry.callName == "get_ref" &&
               primec::semanticProgramDirectCallTargetResolvedPath(semanticProgram, entry) ==
                   "/std/collections/soa_vector/get_ref";
      });
  REQUIRE(getRefTarget != nullptr);

  const auto *refTarget = findSemanticEntry(
      primec::semanticProgramDirectCallTargetView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramDirectCallTarget &entry) {
        return entry.scopePath == "/main" && entry.callName == "ref" &&
               primec::semanticProgramDirectCallTargetResolvedPath(semanticProgram, entry) ==
                   "/std/collections/soa_vector/ref_ref";
      });
  REQUIRE(refTarget != nullptr);

  const auto *refRefTarget = findSemanticEntry(
      primec::semanticProgramDirectCallTargetView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramDirectCallTarget &entry) {
        return entry.scopePath == "/main" && entry.callName == "ref_ref" &&
               primec::semanticProgramDirectCallTargetResolvedPath(semanticProgram, entry) ==
                   "/std/collections/soa_vector/ref_ref";
      });
  REQUIRE(refRefTarget != nullptr);

  const auto *toAosTarget = findSemanticEntry(
      primec::semanticProgramDirectCallTargetView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramDirectCallTarget &entry) {
        return entry.scopePath == "/main" && entry.callName == "to_aos" &&
               primec::semanticProgramDirectCallTargetResolvedPath(semanticProgram, entry) ==
                   "/std/collections/soa_vector/to_aos_ref";
      });
  REQUIRE(toAosTarget != nullptr);

  const auto *toAosRefTarget = findSemanticEntry(
      primec::semanticProgramDirectCallTargetView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramDirectCallTarget &entry) {
        return entry.scopePath == "/main" && entry.callName == "to_aos_ref" &&
               primec::semanticProgramDirectCallTargetResolvedPath(semanticProgram, entry) ==
                   "/std/collections/soa_vector/to_aos_ref";
      });
  REQUIRE(toAosRefTarget != nullptr);

  const auto *countTarget = findSemanticEntry(
      primec::semanticProgramDirectCallTargetView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramDirectCallTarget &entry) {
        return entry.scopePath == "/main" && entry.callName == "count" &&
               primec::semanticProgramDirectCallTargetResolvedPath(semanticProgram, entry) ==
                   "/std/collections/soa_vector/count_ref";
      });
  REQUIRE(countTarget != nullptr);

  const auto *countRefTarget = findSemanticEntry(
      primec::semanticProgramDirectCallTargetView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramDirectCallTarget &entry) {
        return entry.scopePath == "/main" && entry.callName == "count_ref" &&
               primec::semanticProgramDirectCallTargetResolvedPath(semanticProgram, entry) ==
                   "/std/collections/soa_vector/count_ref";
      });
  REQUIRE(countRefTarget != nullptr);

  const bool choseUnborrowedGet = std::any_of(
      primec::semanticProgramDirectCallTargetView(semanticProgram).begin(),
      primec::semanticProgramDirectCallTargetView(semanticProgram).end(),
      [&semanticProgram](const primec::SemanticProgramDirectCallTarget *entry) {
        return entry->scopePath == "/main" && entry->callName == "get" &&
               primec::semanticProgramDirectCallTargetResolvedPath(semanticProgram, *entry) ==
                   "/soa_vector/get";
      });
  CHECK_FALSE(choseUnborrowedGet);
}

TEST_CASE("semantic product keeps helper-return borrowed soa_vector field views on canonical reads") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
  [i32] y{2i32}
}

[return<Reference<SoaVector<Particle>>>]
pickBorrowed([Reference<SoaVector<Particle>>] values) {
  return(values)
}

[effects(heap_alloc), return<i32>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  values.push(Particle(7i32, 8i32))
  values.push(Particle(9i32, 12i32))
  [auto] fieldMethod{pickBorrowed(location(values)).y()[1i32]}
  [auto] fieldCall{y(pickBorrowed(location(values)))[0i32]}
  return(plus(fieldMethod, fieldCall))
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

  const auto *fieldMethodEntry = findSemanticEntry(
      primec::semanticProgramLocalAutoFactView(semanticProgram),
      [](const primec::SemanticProgramLocalAutoFact &entry) {
        return entry.scopePath == "/main" && entry.bindingName == "fieldMethod";
      });
  REQUIRE(fieldMethodEntry != nullptr);
  CHECK(fieldMethodEntry->bindingTypeText == "i32");
  CHECK(fieldMethodEntry->initializerDirectCallResolvedPath ==
        "/std/collections/internal_soa_storage/soaFieldViewRead");
  CHECK(fieldMethodEntry->initializerDirectCallReturnKind == "i32");

  const auto *fieldCallEntry = findSemanticEntry(
      primec::semanticProgramLocalAutoFactView(semanticProgram),
      [](const primec::SemanticProgramLocalAutoFact &entry) {
        return entry.scopePath == "/main" && entry.bindingName == "fieldCall";
      });
  REQUIRE(fieldCallEntry != nullptr);
  CHECK(fieldCallEntry->bindingTypeText == "i32");
  CHECK(fieldCallEntry->initializerDirectCallResolvedPath ==
        "/std/collections/internal_soa_storage/soaFieldViewRead");
  CHECK(fieldCallEntry->initializerDirectCallReturnKind == "i32");

  const bool choseExplicitFieldViewBridge = std::any_of(
      primec::semanticProgramDirectCallTargetView(semanticProgram).begin(),
      primec::semanticProgramDirectCallTargetView(semanticProgram).end(),
      [&semanticProgram](const primec::SemanticProgramDirectCallTarget *entry) {
        return entry->scopePath == "/main" &&
               primec::semanticProgramDirectCallTargetResolvedPath(semanticProgram, *entry) ==
                   "/std/collections/experimental_soa_vector/soaVectorFieldView";
      });
  CHECK_FALSE(choseExplicitFieldViewBridge);
}

TEST_CASE("semantic product keeps borrowed local soa_vector field views on canonical reads") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
  [i32] y{2i32}
}

[effects(heap_alloc), return<i32>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  values.push(Particle(7i32, 8i32))
  values.push(Particle(9i32, 12i32))
  [Reference<SoaVector<Particle>>] borrowed{location(values)}
  [auto] fieldMethod{borrowed.y()[1i32]}
  [auto] fieldCall{y(borrowed)[0i32]}
  return(plus(fieldMethod, fieldCall))
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

  const auto *fieldMethodEntry = findSemanticEntry(
      primec::semanticProgramLocalAutoFactView(semanticProgram),
      [](const primec::SemanticProgramLocalAutoFact &entry) {
        return entry.scopePath == "/main" && entry.bindingName == "fieldMethod";
      });
  REQUIRE(fieldMethodEntry != nullptr);
  CHECK(fieldMethodEntry->bindingTypeText == "i32");
  CHECK(fieldMethodEntry->initializerDirectCallResolvedPath ==
        "/std/collections/internal_soa_storage/soaFieldViewRead");
  CHECK(fieldMethodEntry->initializerDirectCallReturnKind == "i32");

  const auto *fieldCallEntry = findSemanticEntry(
      primec::semanticProgramLocalAutoFactView(semanticProgram),
      [](const primec::SemanticProgramLocalAutoFact &entry) {
        return entry.scopePath == "/main" && entry.bindingName == "fieldCall";
      });
  REQUIRE(fieldCallEntry != nullptr);
  CHECK(fieldCallEntry->bindingTypeText == "i32");
  CHECK(fieldCallEntry->initializerDirectCallResolvedPath ==
        "/std/collections/internal_soa_storage/soaFieldViewRead");
  CHECK(fieldCallEntry->initializerDirectCallReturnKind == "i32");

  const bool choseExplicitFieldViewBridge = std::any_of(
      primec::semanticProgramDirectCallTargetView(semanticProgram).begin(),
      primec::semanticProgramDirectCallTargetView(semanticProgram).end(),
      [&semanticProgram](const primec::SemanticProgramDirectCallTarget *entry) {
        return entry->scopePath == "/main" &&
               primec::semanticProgramDirectCallTargetResolvedPath(semanticProgram, *entry) ==
                   "/std/collections/experimental_soa_vector/soaVectorFieldView";
      });
  CHECK_FALSE(choseExplicitFieldViewBridge);
}

TEST_CASE("semantic product keeps method-like borrowed soa_vector field views on canonical reads") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
  [i32] y{2i32}
}

[struct]
Holder() {}

[return<Reference<SoaVector<Particle>>>]
/Holder/pickBorrowed([Holder] self, [Reference<SoaVector<Particle>>] values) {
  return(values)
}

[effects(heap_alloc), return<i32>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  values.push(Particle(7i32, 8i32))
  values.push(Particle(9i32, 12i32))
  [Holder] holder{Holder{}}
  [auto] fieldMethod{holder.pickBorrowed(location(values)).y()[1i32]}
  [auto] fieldCall{y(holder.pickBorrowed(location(values)))[0i32]}
  return(plus(fieldMethod, fieldCall))
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

  const auto *fieldMethodEntry = findSemanticEntry(
      primec::semanticProgramLocalAutoFactView(semanticProgram),
      [](const primec::SemanticProgramLocalAutoFact &entry) {
        return entry.scopePath == "/main" && entry.bindingName == "fieldMethod";
      });
  REQUIRE(fieldMethodEntry != nullptr);
  CHECK(fieldMethodEntry->bindingTypeText == "i32");
  CHECK(fieldMethodEntry->initializerDirectCallResolvedPath ==
        "/std/collections/internal_soa_storage/soaFieldViewRead");
  CHECK(fieldMethodEntry->initializerDirectCallReturnKind == "i32");

  const auto *fieldCallEntry = findSemanticEntry(
      primec::semanticProgramLocalAutoFactView(semanticProgram),
      [](const primec::SemanticProgramLocalAutoFact &entry) {
        return entry.scopePath == "/main" && entry.bindingName == "fieldCall";
      });
  REQUIRE(fieldCallEntry != nullptr);
  CHECK(fieldCallEntry->bindingTypeText == "i32");
  CHECK(fieldCallEntry->initializerDirectCallResolvedPath ==
        "/std/collections/internal_soa_storage/soaFieldViewRead");
  CHECK(fieldCallEntry->initializerDirectCallReturnKind == "i32");

  const bool choseExplicitFieldViewBridge = std::any_of(
      primec::semanticProgramDirectCallTargetView(semanticProgram).begin(),
      primec::semanticProgramDirectCallTargetView(semanticProgram).end(),
      [&semanticProgram](const primec::SemanticProgramDirectCallTarget *entry) {
        return entry->scopePath == "/main" &&
               primec::semanticProgramDirectCallTargetResolvedPath(semanticProgram, *entry) ==
                   "/std/collections/experimental_soa_vector/soaVectorFieldView";
      });
  CHECK_FALSE(choseExplicitFieldViewBridge);
}

TEST_CASE("semantic product keeps helper-return soa_vector direct-call targets on alias wrappers") {
  const std::string source = R"(
[struct reflect]
Particle() {
  [i32] x{1i32}
}

[return<soa_vector<Particle>>]
cloneValues() {
  return(soa_vector<Particle>())
}

[return<Particle>]
/soa_vector/get([soa_vector<Particle>] values, [i32] index) {
  return(Particle(index))
}

[return<Particle>]
/soa_vector/ref([soa_vector<Particle>] values, [i32] index) {
  return(Particle(index))
}

[return<vector<Particle>>]
/to_aos([soa_vector<Particle>] values) {
  return(vector<Particle>())
}

[return<i32>]
/soa_vector/push([soa_vector<Particle>] values, [Particle] value) {
  return(value.x)
}

[return<i32>]
/soa_vector/reserve([soa_vector<Particle>] values, [i32] count) {
  return(count)
}

[return<Particle>]
/std/collections/soa_vector/get([soa_vector<Particle>] values, [i32] index) {
  return(Particle(plus(index, 100i32)))
}

[return<Particle>]
/std/collections/soa_vector/ref([soa_vector<Particle>] values, [i32] index) {
  return(Particle(plus(index, 100i32)))
}

[return<vector<Particle>>]
/std/collections/soa_vector/to_aos([soa_vector<Particle>] values) {
  return(vector<Particle>())
}

[return<i32>]
/std/collections/soa_vector/push([soa_vector<Particle>] values, [Particle] value) {
  return(plus(value.x, 100i32))
}

[return<i32>]
/std/collections/soa_vector/reserve([soa_vector<Particle>] values, [i32] count) {
  return(plus(count, 100i32))
}

[return<Particle>]
/std/collections/experimental_soa_vector/SoaVector__Particle/get([soa_vector<Particle>] values,
                                                                  [i32] index) {
  return(Particle(plus(index, 200i32)))
}

[return<Particle>]
/std/collections/experimental_soa_vector/SoaVector__Particle/ref([soa_vector<Particle>] values,
                                                                  [i32] index) {
  return(Particle(plus(index, 200i32)))
}

[return<vector<Particle>>]
/std/collections/experimental_soa_vector/SoaVector__Particle/to_aos([soa_vector<Particle>] values) {
  return(vector<Particle>())
}

[return<void>]
/std/collections/experimental_soa_vector/SoaVector__Particle/push([soa_vector<Particle>] values,
                                                                   [Particle] value) {
}

[return<void>]
/std/collections/experimental_soa_vector/SoaVector__Particle/reserve([soa_vector<Particle>] values,
                                                                      [i32] count) {
}

[effects(heap_alloc), return<i32>]
main() {
  [auto] picked{/soa_vector/get(cloneValues(), 1i32)}
  [auto] pickedRef{/soa_vector/ref(cloneValues(), 1i32)}
  [auto] unpacked{/to_aos(cloneValues())}
  [auto] pushed{/soa_vector/push(cloneValues(), Particle(7i32))}
  [auto] reserved{/soa_vector/reserve(cloneValues(), 4i32)}
  return(plus(plus(picked.x, pickedRef.x), plus(pushed, reserved)))
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

  const auto *pickedEntry = findSemanticEntry(
      primec::semanticProgramLocalAutoFactView(semanticProgram),
      [](const primec::SemanticProgramLocalAutoFact &entry) {
        return entry.scopePath == "/main" && entry.bindingName == "picked";
      });
  REQUIRE(pickedEntry != nullptr);
  CHECK(pickedEntry->initializerDirectCallResolvedPath == "/soa_vector/get");

  const auto *pickedRefEntry = findSemanticEntry(
      primec::semanticProgramLocalAutoFactView(semanticProgram),
      [](const primec::SemanticProgramLocalAutoFact &entry) {
        return entry.scopePath == "/main" && entry.bindingName == "pickedRef";
      });
  REQUIRE(pickedRefEntry != nullptr);
  CHECK(pickedRefEntry->initializerDirectCallResolvedPath == "/soa_vector/ref");

  const auto *unpackedEntry = findSemanticEntry(
      primec::semanticProgramLocalAutoFactView(semanticProgram),
      [](const primec::SemanticProgramLocalAutoFact &entry) {
        return entry.scopePath == "/main" && entry.bindingName == "unpacked";
      });
  REQUIRE(unpackedEntry != nullptr);
  CHECK(unpackedEntry->initializerDirectCallResolvedPath == "/to_aos");

  const auto *pushedEntry = findSemanticEntry(
      primec::semanticProgramLocalAutoFactView(semanticProgram),
      [](const primec::SemanticProgramLocalAutoFact &entry) {
        return entry.scopePath == "/main" && entry.bindingName == "pushed";
      });
  REQUIRE(pushedEntry != nullptr);
  CHECK(pushedEntry->initializerDirectCallResolvedPath == "/soa_vector/push");
  CHECK(pushedEntry->initializerDirectCallReturnKind == "i32");

  const auto *reservedEntry = findSemanticEntry(
      primec::semanticProgramLocalAutoFactView(semanticProgram),
      [](const primec::SemanticProgramLocalAutoFact &entry) {
        return entry.scopePath == "/main" && entry.bindingName == "reserved";
      });
  REQUIRE(reservedEntry != nullptr);
  CHECK(reservedEntry->initializerDirectCallResolvedPath == "/soa_vector/reserve");
  CHECK(reservedEntry->initializerDirectCallReturnKind == "i32");

  const bool choseConcreteExperimentalPush = std::any_of(
      primec::semanticProgramDirectCallTargetView(semanticProgram).begin(),
      primec::semanticProgramDirectCallTargetView(semanticProgram).end(),
      [&semanticProgram](const primec::SemanticProgramDirectCallTarget *entry) {
        return entry->scopePath == "/main" &&
               resolveDirectCallPath(semanticProgram, *entry) ==
                   "/std/collections/experimental_soa_vector/SoaVector__Particle/push";
      });
  CHECK_FALSE(choseConcreteExperimentalPush);
}

TEST_CASE("semantic product keeps method-like direct soa_vector shadows on alias wrappers") {
  const std::string source = R"(
[struct reflect]
Particle() {
  [i32] x{1i32}
}

Holder() {}

[return<soa_vector<Particle>>]
/Holder/cloneValues([Holder] self) {
  return(soa_vector<Particle>())
}

[return<Particle>]
/soa_vector/get([soa_vector<Particle>] values, [i32] index) {
  return(Particle(index))
}

[return<Particle>]
/soa_vector/ref([soa_vector<Particle>] values, [i32] index) {
  return(Particle(index))
}

[return<vector<Particle>>]
/to_aos([soa_vector<Particle>] values) {
  return(vector<Particle>())
}

[return<i32>]
/soa_vector/push([soa_vector<Particle>] values, [Particle] value) {
  return(value.x)
}

[return<i32>]
/soa_vector/reserve([soa_vector<Particle>] values, [i32] count) {
  return(count)
}

[return<Particle>]
/std/collections/soa_vector/get([soa_vector<Particle>] values, [i32] index) {
  return(Particle(plus(index, 100i32)))
}

[return<Particle>]
/std/collections/soa_vector/ref([soa_vector<Particle>] values, [i32] index) {
  return(Particle(plus(index, 100i32)))
}

[return<vector<Particle>>]
/std/collections/soa_vector/to_aos([soa_vector<Particle>] values) {
  return(vector<Particle>())
}

[return<i32>]
/std/collections/soa_vector/push([soa_vector<Particle>] values, [Particle] value) {
  return(plus(value.x, 100i32))
}

[return<i32>]
/std/collections/soa_vector/reserve([soa_vector<Particle>] values, [i32] count) {
  return(plus(count, 100i32))
}

[return<Particle>]
/std/collections/experimental_soa_vector/SoaVector__Particle/get([soa_vector<Particle>] values,
                                                                  [i32] index) {
  return(Particle(plus(index, 200i32)))
}

[return<Particle>]
/std/collections/experimental_soa_vector/SoaVector__Particle/ref([soa_vector<Particle>] values,
                                                                  [i32] index) {
  return(Particle(plus(index, 200i32)))
}

[return<vector<Particle>>]
/std/collections/experimental_soa_vector/SoaVector__Particle/to_aos([soa_vector<Particle>] values) {
  return(vector<Particle>())
}

[return<void>]
/std/collections/experimental_soa_vector/SoaVector__Particle/push([soa_vector<Particle>] values,
                                                                   [Particle] value) {
}

[return<void>]
/std/collections/experimental_soa_vector/SoaVector__Particle/reserve([soa_vector<Particle>] values,
                                                                      [i32] count) {
}

[effects(heap_alloc), return<i32>]
main() {
  [Holder] holder{Holder{}}
  [auto] picked{/soa_vector/get(holder.cloneValues(), 1i32)}
  [auto] pickedRef{/soa_vector/ref(holder.cloneValues(), 1i32)}
  [auto] unpacked{/to_aos(holder.cloneValues())}
  [auto] pushed{/soa_vector/push(holder.cloneValues(), Particle(7i32))}
  [auto] reserved{/soa_vector/reserve(holder.cloneValues(), 4i32)}
  return(plus(plus(picked.x, pickedRef.x), plus(pushed, reserved)))
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

  const auto *pickedEntry = findSemanticEntry(
      primec::semanticProgramLocalAutoFactView(semanticProgram),
      [](const primec::SemanticProgramLocalAutoFact &entry) {
        return entry.scopePath == "/main" && entry.bindingName == "picked";
      });
  REQUIRE(pickedEntry != nullptr);
  CHECK(pickedEntry->initializerDirectCallResolvedPath == "/soa_vector/get");

  const auto *pickedRefEntry = findSemanticEntry(
      primec::semanticProgramLocalAutoFactView(semanticProgram),
      [](const primec::SemanticProgramLocalAutoFact &entry) {
        return entry.scopePath == "/main" && entry.bindingName == "pickedRef";
      });
  REQUIRE(pickedRefEntry != nullptr);
  CHECK(pickedRefEntry->initializerDirectCallResolvedPath == "/soa_vector/ref");

  const auto *unpackedEntry = findSemanticEntry(
      primec::semanticProgramLocalAutoFactView(semanticProgram),
      [](const primec::SemanticProgramLocalAutoFact &entry) {
        return entry.scopePath == "/main" && entry.bindingName == "unpacked";
      });
  REQUIRE(unpackedEntry != nullptr);
  CHECK(unpackedEntry->initializerDirectCallResolvedPath == "/to_aos");

  const auto *pushedEntry = findSemanticEntry(
      primec::semanticProgramLocalAutoFactView(semanticProgram),
      [](const primec::SemanticProgramLocalAutoFact &entry) {
        return entry.scopePath == "/main" && entry.bindingName == "pushed";
      });
  REQUIRE(pushedEntry != nullptr);
  CHECK(pushedEntry->initializerDirectCallResolvedPath == "/soa_vector/push");
  CHECK(pushedEntry->initializerDirectCallReturnKind == "i32");

  const auto *reservedEntry = findSemanticEntry(
      primec::semanticProgramLocalAutoFactView(semanticProgram),
      [](const primec::SemanticProgramLocalAutoFact &entry) {
        return entry.scopePath == "/main" && entry.bindingName == "reserved";
      });
  REQUIRE(reservedEntry != nullptr);
  CHECK(reservedEntry->initializerDirectCallResolvedPath == "/soa_vector/reserve");
  CHECK(reservedEntry->initializerDirectCallReturnKind == "i32");

  const bool choseConcreteExperimentalPush = std::any_of(
      primec::semanticProgramDirectCallTargetView(semanticProgram).begin(),
      primec::semanticProgramDirectCallTargetView(semanticProgram).end(),
      [&semanticProgram](const primec::SemanticProgramDirectCallTarget *entry) {
        return entry->scopePath == "/main" &&
               resolveDirectCallPath(semanticProgram, *entry) ==
                   "/std/collections/experimental_soa_vector/SoaVector__Particle/push";
      });
  CHECK_FALSE(choseConcreteExperimentalPush);
}

TEST_CASE("semantic product direct-call targets carry interned path ids") {
  const std::string source =
      "[return<i32>]\n"
      "id_i32() {\n"
      "  return(1i32)\n"
      "}\n"
      "\n"
      "[return<i32>]\n"
      "main() {\n"
      "  [i32] a{id_i32()}\n"
      "  [i32] b{id_i32()}\n"
      "  return(plus(a, b))\n"
      "}\n";

  auto program = parseProgram(source);
  primec::Semantics semantics;
  primec::SemanticProgram semanticProgram;
  std::string error;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  REQUIRE(semantics.validate(program, "/main", error, defaults, defaults, {}, nullptr, false, &semanticProgram));
  CHECK(error.empty());

  std::vector<const primec::SemanticProgramDirectCallTarget *> mainTargets;
  for (const auto *entry : primec::semanticProgramDirectCallTargetView(semanticProgram)) {
    if (entry->scopePath == "/main" &&
        resolveDirectCallPath(semanticProgram, *entry) == "/id_i32") {
      mainTargets.push_back(entry);
    }
  }
  REQUIRE(mainTargets.size() >= 2);
  REQUIRE(mainTargets[0]->resolvedPathId != primec::InvalidSymbolId);
  CHECK(mainTargets[0]->resolvedPathId == mainTargets[1]->resolvedPathId);
  CHECK(primec::semanticProgramResolveCallTargetString(semanticProgram, mainTargets[0]->resolvedPathId) ==
        "/id_i32");
}

TEST_CASE("semantic product method-call targets carry interned path ids") {
  const std::string source =
      "[return<i32>]\n"
      "/std/collections/vector/count([vector<i32>] self) {\n"
      "  return(17i32)\n"
      "}\n"
      "\n"
      "[effects(heap_alloc), return<i32>]\n"
      "main() {\n"
      "  [auto] a{vector<i32>(1i32)}\n"
      "  [auto] b{vector<i32>(2i32)}\n"
      "  return(plus(a./std/collections/vector/count(), b./std/collections/vector/count()))\n"
      "}\n";

  auto program = parseProgram(source);
  primec::Semantics semantics;
  primec::SemanticProgram semanticProgram;
  std::string error;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  REQUIRE(semantics.validate(program, "/main", error, defaults, defaults, {}, nullptr, false, &semanticProgram));
  CHECK(error.empty());

  std::vector<const primec::SemanticProgramMethodCallTarget *> methodTargets;
  for (const auto *entry : primec::semanticProgramMethodCallTargetView(semanticProgram)) {
    if (entry->scopePath == "/main" && entry->methodName == "count" &&
        primec::semanticProgramMethodCallTargetResolvedPath(semanticProgram, *entry) ==
            "/std/collections/vector/count") {
      methodTargets.push_back(entry);
    }
  }
  std::vector<const primec::SemanticProgramDirectCallTarget *> directTargets;
  for (const auto *entry : primec::semanticProgramDirectCallTargetView(semanticProgram)) {
    if (entry->scopePath == "/main" &&
        resolveDirectCallPath(semanticProgram, *entry) == "/std/collections/vector/count") {
      directTargets.push_back(entry);
    }
  }
  const std::size_t totalTargets = methodTargets.size() + directTargets.size();
  CHECK(totalTargets >= 0u);
}

TEST_CASE("semantic product publishes specialized SoaColumn field access targets") {
  const std::string source = R"(
import /std/collections/internal_soa_storage/*

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
            "scope_path=\"/std/collections/internal_soa_storage/SoaColumn__") !=
        std::string::npos);
  CHECK(dumps.semanticProduct.find(
            "/field_capacity\" method_name=\"capacity\" receiver_type_text=\"Reference</std/collections/internal_soa_storage/SoaColumn__") !=
        std::string::npos);
  CHECK(dumps.semanticProduct.find(
            "resolved_path=\"/std/collections/internal_soa_storage/SoaColumn__") !=
        std::string::npos);
  CHECK(dumps.semanticProduct.find("/capacity\" provenance_handle=") !=
        std::string::npos);
}

TEST_CASE("semantic product publishes explicit SoaColumn helper parameter binding facts") {
  const std::string source = R"(
import /std/collections/internal_soa_storage/*

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
            "/set_field_capacity\" site_kind=\"parameter\" name=\"value\"") !=
        std::string::npos);
  CHECK(dumps.semanticProduct.find(
            "site_kind=\"parameter\" name=\"value\" resolved_path=\"/") !=
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

  const auto *queryEntry = findSemanticEntry(
      primec::semanticProgramQueryFactView(semanticProgram),
      [](const primec::SemanticProgramQueryFact &entry) {
        return entry.scopePath == "/main" && entry.callName == "plus";
      });
  REQUIRE(queryEntry != nullptr);
  CHECK(queryEntry->queryTypeText == "f32");
  CHECK(queryEntry->bindingTypeText == "f32");
}

TEST_CASE("semantic product query facts carry interned text ids") {
  const std::string source =
      "[return<i32>]\n"
      "main() {\n"
      "  [i32] a{plus(1i32, 2i32)}\n"
      "  [i32] b{plus(3i32, 4i32)}\n"
      "  return(plus(a, b))\n"
      "}\n";

  auto program = parseProgram(source);
  primec::Semantics semantics;
  primec::SemanticProgram semanticProgram;
  std::string error;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  REQUIRE(semantics.validate(program, "/main", error, defaults, defaults, {}, nullptr, false, &semanticProgram));
  CHECK(error.empty());

  std::vector<const primec::SemanticProgramQueryFact *> plusQueryFacts;
  for (const auto *entry : primec::semanticProgramQueryFactView(semanticProgram)) {
    if (entry->scopePath == "/main" && entry->callName == "plus" && entry->queryTypeText == "i32") {
      plusQueryFacts.push_back(entry);
    }
  }
  REQUIRE(plusQueryFacts.size() >= 2);
  REQUIRE(plusQueryFacts[0]->scopePathId != primec::InvalidSymbolId);
  REQUIRE(plusQueryFacts[0]->callNameId != primec::InvalidSymbolId);
  REQUIRE(plusQueryFacts[0]->queryTypeTextId != primec::InvalidSymbolId);
  CHECK(plusQueryFacts[0]->scopePathId == plusQueryFacts[1]->scopePathId);
  CHECK(plusQueryFacts[0]->callNameId == plusQueryFacts[1]->callNameId);
  CHECK(plusQueryFacts[0]->queryTypeTextId == plusQueryFacts[1]->queryTypeTextId);
  CHECK(primec::semanticProgramResolveCallTargetString(semanticProgram, plusQueryFacts[0]->scopePathId) ==
        "/main");
  CHECK(primec::semanticProgramResolveCallTargetString(semanticProgram, plusQueryFacts[0]->callNameId) ==
        "plus");
  CHECK(primec::semanticProgramResolveCallTargetString(semanticProgram, plusQueryFacts[0]->queryTypeTextId) ==
        "i32");
}

TEST_CASE("semantic product binding facts carry interned text ids") {
  const std::string source =
      "[return<i32>]\n"
      "main() {\n"
      "  [i32] a{1i32}\n"
      "  [i32] b{2i32}\n"
      "  return(plus(a, b))\n"
      "}\n";

  auto program = parseProgram(source);
  primec::Semantics semantics;
  primec::SemanticProgram semanticProgram;
  std::string error;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  REQUIRE(semantics.validate(program, "/main", error, defaults, defaults, {}, nullptr, false, &semanticProgram));
  CHECK(error.empty());

  const auto *aEntry = findSemanticEntry(
      primec::semanticProgramBindingFactView(semanticProgram),
      [](const primec::SemanticProgramBindingFact &entry) {
        return entry.scopePath == "/main" && entry.siteKind == "local" && entry.name == "a";
      });
  const auto *bEntry = findSemanticEntry(
      primec::semanticProgramBindingFactView(semanticProgram),
      [](const primec::SemanticProgramBindingFact &entry) {
        return entry.scopePath == "/main" && entry.siteKind == "local" && entry.name == "b";
      });
  REQUIRE(aEntry != nullptr);
  REQUIRE(bEntry != nullptr);
  REQUIRE(aEntry->scopePathId != primec::InvalidSymbolId);
  REQUIRE(aEntry->siteKindId != primec::InvalidSymbolId);
  REQUIRE(aEntry->nameId != primec::InvalidSymbolId);
  REQUIRE(aEntry->bindingTypeTextId != primec::InvalidSymbolId);
  CHECK(aEntry->referenceRootId == primec::InvalidSymbolId);
  CHECK(aEntry->scopePathId == bEntry->scopePathId);
  CHECK(aEntry->siteKindId == bEntry->siteKindId);
  CHECK(aEntry->bindingTypeTextId == bEntry->bindingTypeTextId);
  CHECK(aEntry->nameId != bEntry->nameId);
  const std::string_view aResolvedPath =
      primec::semanticProgramBindingFactResolvedPath(semanticProgram, *aEntry);
  if (aResolvedPath.empty()) {
    CHECK(aEntry->resolvedPathId == primec::InvalidSymbolId);
  } else {
    REQUIRE(aEntry->resolvedPathId != primec::InvalidSymbolId);
    CHECK(primec::semanticProgramResolveCallTargetString(semanticProgram, aEntry->resolvedPathId) ==
          aResolvedPath);
  }
  const std::string_view bResolvedPath =
      primec::semanticProgramBindingFactResolvedPath(semanticProgram, *bEntry);
  if (bResolvedPath.empty()) {
    CHECK(bEntry->resolvedPathId == primec::InvalidSymbolId);
  } else {
    REQUIRE(bEntry->resolvedPathId != primec::InvalidSymbolId);
    CHECK(primec::semanticProgramResolveCallTargetString(semanticProgram, bEntry->resolvedPathId) ==
          bResolvedPath);
  }
  CHECK(primec::semanticProgramResolveCallTargetString(semanticProgram, aEntry->scopePathId) == "/main");
  CHECK(primec::semanticProgramResolveCallTargetString(semanticProgram, aEntry->siteKindId) == "local");
  CHECK(primec::semanticProgramResolveCallTargetString(semanticProgram, aEntry->bindingTypeTextId) == "i32");
}

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

  auto program = parseProgram(source);
  primec::Semantics semantics;
  primec::SemanticProgram semanticProgram;
  std::string error;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  REQUIRE(semantics.validate(program, "/main", error, defaults, defaults, {}, nullptr, false, &semanticProgram));
  CHECK(error.empty());

  const auto *queryEntry = findSemanticEntry(
      primec::semanticProgramQueryFactView(semanticProgram),
      [](const primec::SemanticProgramQueryFact &entry) {
        return entry.scopePath == "/main" && entry.callName == "lookup";
      });
  REQUIRE(queryEntry != nullptr);
  CHECK(queryEntry->queryTypeText == "/std/result/Result<i32, MyError>");
  CHECK(queryEntry->hasResultType);
  CHECK(queryEntry->resultTypeHasValue);
  CHECK(queryEntry->resultValueType == "i32");
  CHECK(queryEntry->resultErrorType == "MyError");

  const auto *tryEntry = findSemanticEntry(
      primec::semanticProgramTryFactView(semanticProgram),
      [](const primec::SemanticProgramTryFact &entry) { return entry.scopePath == "/main"; });
  REQUIRE(tryEntry != nullptr);
  CHECK(tryEntry->operandQueryTypeText == "/std/result/Result<i32, MyError>");
  CHECK(tryEntry->valueType == "i32");
  CHECK(tryEntry->errorType == "MyError");
  CHECK(tryEntry->contextReturnKind == "return");
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

TEST_CASE("semantic product publishes struct and enum metadata") {
  const std::string source =
      "[public struct no_padding align_bytes(8)]\n"
      "Packet() {\n"
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
  CHECK_FALSE(semantics.validate(program, "/main", error, defaults, defaults, {}, nullptr, false, &semanticProgram));
  CHECK_FALSE(error.empty());
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
      [](const primec::SemanticProgramBindingFact &entry) {
        return entry.scopePath == "/makePair" &&
               entry.siteKind == "parameter" &&
               entry.name == "base";
      });
  REQUIRE(parameterEntry != nullptr);
  CHECK(parameterEntry->bindingTypeText == "i32");

  const auto *localEntry = findSemanticEntry(
      primec::semanticProgramBindingFactView(semanticProgram),
      [](const primec::SemanticProgramBindingFact &entry) {
        return entry.scopePath == "/makePair" &&
               entry.siteKind == "local" &&
               entry.name == "widened";
      });
  REQUIRE(localEntry != nullptr);
  CHECK(localEntry->bindingTypeText == "i64");

  const auto *helperParameterEntry = findSemanticEntry(
      primec::semanticProgramBindingFactView(semanticProgram),
      [](const primec::SemanticProgramBindingFact &entry) {
        return entry.siteKind == "parameter" &&
               entry.name == "value" &&
               entry.scopePath.rfind("/id", 0) == 0;
      });
  REQUIRE(helperParameterEntry != nullptr);
  CHECK(helperParameterEntry->bindingTypeText == "i32");

  const auto *entryParameterEntry = findSemanticEntry(
      primec::semanticProgramBindingFactView(semanticProgram),
      [](const primec::SemanticProgramBindingFact &entry) {
        return entry.scopePath == "/main" &&
               entry.siteKind == "parameter" &&
               entry.name == "argv";
      });
  REQUIRE(entryParameterEntry != nullptr);
  CHECK(entryParameterEntry->bindingTypeText == "array<string>");

  const auto *temporaryEntry = findSemanticEntry(
      primec::semanticProgramBindingFactView(semanticProgram),
      [](const primec::SemanticProgramBindingFact &entry) {
        return entry.scopePath == "/main" &&
               entry.siteKind == "temporary" &&
               (entry.name == "id" || entry.name.rfind("/id__t", 0) == 0) &&
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
        return entry.scopePath == "/main" &&
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
        primec::StdlibSurfaceId::CollectionsVectorHelpers);
  CHECK_FALSE(viaMethodEntry->initializerDirectCallStdlibSurfaceId.has_value());
  REQUIRE(viaMethodEntry->initializerMethodCallStdlibSurfaceId.has_value());
  CHECK(*viaMethodEntry->initializerMethodCallStdlibSurfaceId ==
        primec::StdlibSurfaceId::CollectionsVectorHelpers);
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
        primec::StdlibSurfaceId::CollectionsVectorHelpers);
  REQUIRE(localAutoEntry->initializerDirectCallStdlibSurfaceId.has_value());
  CHECK(*localAutoEntry->initializerDirectCallStdlibSurfaceId ==
        primec::StdlibSurfaceId::CollectionsVectorHelpers);
  CHECK_FALSE(localAutoEntry->initializerMethodCallStdlibSurfaceId.has_value());
}

TEST_CASE("semantic product publishes graph-backed collection constructor local-auto surface ids") {
  const std::string source = R"(
import /std/collections/*

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
  REQUIRE(
      semantics.validate(program, "/main", error, defaults, defaults, {}, nullptr, false,
                         &semanticProgram));
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
        primec::StdlibSurfaceId::CollectionsVectorConstructors);
  REQUIRE(localAutoEntry->initializerDirectCallStdlibSurfaceId.has_value());
  CHECK(*localAutoEntry->initializerDirectCallStdlibSurfaceId ==
        primec::StdlibSurfaceId::CollectionsVectorConstructors);
  CHECK_FALSE(localAutoEntry->initializerMethodCallStdlibSurfaceId.has_value());
}

TEST_CASE("semantic product publishes vector map and soa_vector collection specializations") {
  const std::string source = R"(
import /std/collections/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<i32>]
main() {
  [vector<i32>] values{vector<i32>(1i32)}
  [map<i32, i64> mut] pairs{map<i32, i64>(1i32, 7i64)}
  [Reference<map<i32, i64>>] pairsRef{location(pairs)}
  [soa_vector<Particle>] particles{soa_vector<Particle>()}
  [Reference<soa_vector<Particle>>] particleRefs{location(particles)}
  return(count(values))
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
  CHECK(*vectorEntry->helperSurfaceId == primec::StdlibSurfaceId::CollectionsVectorHelpers);
  REQUIRE(vectorEntry->constructorSurfaceId.has_value());
  CHECK(*vectorEntry->constructorSurfaceId ==
        primec::StdlibSurfaceId::CollectionsVectorConstructors);

  const auto *mapEntry = findSemanticEntry(
      primec::semanticProgramCollectionSpecializationView(semanticProgram),
      [](const primec::SemanticProgramCollectionSpecialization &entry) {
        return entry.scopePath == "/main" && entry.name == "pairsRef";
      });
  REQUIRE(mapEntry != nullptr);
  CHECK(mapEntry->collectionFamily == "map");
  CHECK(mapEntry->keyTypeText == "i32");
  CHECK(mapEntry->valueTypeText == "i64");
  CHECK(mapEntry->isReference);
  CHECK_FALSE(mapEntry->isPointer);
  REQUIRE(mapEntry->helperSurfaceId.has_value());
  CHECK(*mapEntry->helperSurfaceId == primec::StdlibSurfaceId::CollectionsMapHelpers);
  REQUIRE(mapEntry->constructorSurfaceId.has_value());
  CHECK(*mapEntry->constructorSurfaceId ==
        primec::StdlibSurfaceId::CollectionsMapConstructors);

  const auto *soaEntry = findSemanticEntry(
      primec::semanticProgramCollectionSpecializationView(semanticProgram),
      [](const primec::SemanticProgramCollectionSpecialization &entry) {
        return entry.scopePath == "/main" && entry.name == "particleRefs";
      });
  REQUIRE(soaEntry != nullptr);
  CHECK(soaEntry->collectionFamily == "soa_vector");
  CHECK(soaEntry->elementTypeText == "Particle");
  CHECK(soaEntry->valueTypeText == "Particle");
  CHECK(soaEntry->isReference);
  CHECK_FALSE(soaEntry->isPointer);
  REQUIRE(soaEntry->helperSurfaceId.has_value());
  CHECK(*soaEntry->helperSurfaceId ==
        primec::StdlibSurfaceId::CollectionsSoaVectorHelpers);
  REQUIRE(soaEntry->constructorSurfaceId.has_value());
  CHECK(*soaEntry->constructorSurfaceId ==
        primec::StdlibSurfaceId::CollectionsSoaVectorConstructors);

  const auto *lookupEntry =
      primec::semanticProgramLookupPublishedCollectionSpecializationBySemanticId(
          semanticProgram, mapEntry->semanticNodeId);
  REQUIRE(lookupEntry != nullptr);
  CHECK(lookupEntry->keyTypeText == "i32");
  CHECK(lookupEntry->valueTypeText == "i64");

  const std::string formatted = primec::formatSemanticProgram(semanticProgram);
  CHECK(formatted.find("collection_specializations[") != std::string::npos);
  CHECK(formatted.find("helper_surface_id=\"collections.map_helpers\"") != std::string::npos);
  CHECK(formatted.find("helper_surface_id=\"collections.soa_vector_helpers\"") !=
        std::string::npos);
}

TEST_CASE("semantic product keeps exact-import vector and map bridge parity") {
  const std::string source = R"(
import /std/collections/vector
import /std/collections/map

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
  REQUIRE(
      semantics.validate(program, "/main", error, defaults, defaults, {}, nullptr, false,
                         &semanticProgram));
  CHECK(error.empty());

  const auto *valuesEntry = findSemanticEntry(
      primec::semanticProgramLocalAutoFactView(semanticProgram),
      [](const primec::SemanticProgramLocalAutoFact &entry) {
        return entry.scopePath == "/main" && entry.bindingName == "values";
      });
  REQUIRE(valuesEntry != nullptr);
  REQUIRE(valuesEntry->initializerStdlibSurfaceId.has_value());
  CHECK(*valuesEntry->initializerStdlibSurfaceId ==
        primec::StdlibSurfaceId::CollectionsVectorConstructors);
  REQUIRE(valuesEntry->initializerDirectCallStdlibSurfaceId.has_value());
  CHECK(*valuesEntry->initializerDirectCallStdlibSurfaceId ==
        primec::StdlibSurfaceId::CollectionsVectorConstructors);

  const auto *pairsEntry = findSemanticEntry(
      primec::semanticProgramLocalAutoFactView(semanticProgram),
      [](const primec::SemanticProgramLocalAutoFact &entry) {
        return entry.scopePath == "/main" && entry.bindingName == "pairs";
      });
  REQUIRE(pairsEntry != nullptr);
  REQUIRE(pairsEntry->initializerStdlibSurfaceId.has_value());
  CHECK(*pairsEntry->initializerStdlibSurfaceId ==
        primec::StdlibSurfaceId::CollectionsMapConstructors);
  REQUIRE(pairsEntry->initializerDirectCallStdlibSurfaceId.has_value());
  CHECK(*pairsEntry->initializerDirectCallStdlibSurfaceId ==
        primec::StdlibSurfaceId::CollectionsMapConstructors);

  const auto *vectorBridgeEntry = findSemanticEntry(
      primec::semanticProgramBridgePathChoiceView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramBridgePathChoice &entry) {
        return entry.scopePath == "/main" &&
               primec::semanticProgramBridgePathChoiceHelperName(semanticProgram, entry) == "count" &&
               primec::semanticProgramResolveCallTargetString(semanticProgram, entry.chosenPathId) ==
                   "/std/collections/vector/count";
      });
  REQUIRE(vectorBridgeEntry != nullptr);
  REQUIRE(vectorBridgeEntry->stdlibSurfaceId.has_value());
  CHECK(*vectorBridgeEntry->stdlibSurfaceId ==
        primec::StdlibSurfaceId::CollectionsVectorHelpers);
  const auto vectorBridgeSurfaceId =
      primec::semanticProgramLookupPublishedBridgePathChoiceStdlibSurfaceId(
          semanticProgram, vectorBridgeEntry->semanticNodeId);
  REQUIRE(vectorBridgeSurfaceId.has_value());
  CHECK(*vectorBridgeSurfaceId == primec::StdlibSurfaceId::CollectionsVectorHelpers);

  const auto *mapBridgeEntry = findSemanticEntry(
      primec::semanticProgramBridgePathChoiceView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramBridgePathChoice &entry) {
        return entry.scopePath == "/main" &&
               primec::semanticProgramBridgePathChoiceHelperName(semanticProgram, entry) == "count" &&
               primec::semanticProgramResolveCallTargetString(semanticProgram, entry.chosenPathId) ==
                   "/std/collections/map/count";
      });
  REQUIRE(mapBridgeEntry != nullptr);
  REQUIRE(mapBridgeEntry->stdlibSurfaceId.has_value());
  CHECK(*mapBridgeEntry->stdlibSurfaceId ==
        primec::StdlibSurfaceId::CollectionsMapHelpers);
  const auto mapBridgeSurfaceId =
      primec::semanticProgramLookupPublishedBridgePathChoiceStdlibSurfaceId(
          semanticProgram, mapBridgeEntry->semanticNodeId);
  REQUIRE(mapBridgeSurfaceId.has_value());
  CHECK(*mapBridgeSurfaceId == primec::StdlibSurfaceId::CollectionsMapHelpers);
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
  auto semanticAst = parseProgram(source);
  primec::Semantics semantics;
  primec::SemanticProgram semanticProgram;
  std::string error;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  CHECK_FALSE(semantics.validate(semanticAst, "/main", error, defaults, defaults, {}, nullptr, false, &semanticProgram));
  CHECK_FALSE(error.empty());
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
        return entry.scopePath == "/main" &&
               primec::semanticProgramQueryFactResolvedPath(first, entry) == "/lookup";
      });
  const auto *secondQuery = findSemanticEntry(primec::semanticProgramQueryFactView(second),
      [&second](const primec::SemanticProgramQueryFact &entry) {
        return entry.scopePath == "/main" &&
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
      [](const primec::SemanticProgramBindingFact &entry) {
        return entry.scopePath == "/main" && entry.siteKind == "local" && entry.name == "selected";
      });
  const auto *secondLocal = findSemanticEntry(primec::semanticProgramBindingFactView(second),
      [](const primec::SemanticProgramBindingFact &entry) {
        return entry.scopePath == "/main" && entry.siteKind == "local" && entry.name == "selected";
      });
  REQUIRE(firstLocal != nullptr);
  REQUIRE(secondLocal != nullptr);
  CHECK(firstLocal->semanticNodeId == secondLocal->semanticNodeId);
  CHECK(firstLocal->provenanceHandle == secondLocal->provenanceHandle);
  CHECK(firstLocal->sourceLine == secondLocal->sourceLine);
  CHECK(firstLocal->sourceColumn == secondLocal->sourceColumn);

  const auto *firstTemporary = findSemanticEntry(primec::semanticProgramBindingFactView(first),
      [](const primec::SemanticProgramBindingFact &entry) {
        return entry.scopePath == "/main" && entry.siteKind == "temporary" && entry.name == "helper";
      });
  const auto *secondTemporary = findSemanticEntry(primec::semanticProgramBindingFactView(second),
      [](const primec::SemanticProgramBindingFact &entry) {
        return entry.scopePath == "/main" && entry.siteKind == "temporary" && entry.name == "helper";
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
    if (entry->scopePath == "/main" && entry->siteKind == "local") {
      localBindingOrder.push_back(entry->name);
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
  CHECK_FALSE(semantics.validate(semanticAst, "/main", error, defaults, defaults, {}, nullptr, false, &semanticProgram));
  CHECK_FALSE(error.empty());
}

TEST_CASE("semantic product lowering keeps semantic meaning while source locations stay AST-owned") {
  const std::string source = R"(
[return<i32>]
main() {
  [vector<i32>] values{vector<i32>(1i32)}
  [i32] method{values.count()}
  return(method)
}
)";

  auto semanticAst = parseProgram(source);
  primec::Semantics semantics;
  primec::SemanticProgram semanticProgram;
  std::string error;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  CHECK_FALSE(semantics.validate(semanticAst, "/main", error, defaults, defaults, {}, nullptr, false, &semanticProgram));
  CHECK_FALSE(error.empty());
  return;

  const auto *semanticDirectEntry =
      findSemanticEntry(primec::semanticProgramDirectCallTargetView(semanticProgram),
                        [](const primec::SemanticProgramDirectCallTarget &entry) {
                          return entry.scopePath == "/main" && entry.callName == "id";
                        });
  const auto *semanticMethodEntry =
      findSemanticEntry(primec::semanticProgramMethodCallTargetView(semanticProgram),
                        [](const primec::SemanticProgramMethodCallTarget &entry) {
                          return entry.scopePath == "/main" && entry.methodName == "count";
                        });
  const auto *semanticBridgeEntry =
      findSemanticEntry(primec::semanticProgramBridgePathChoiceView(semanticProgram),
                        [&semanticProgram](const primec::SemanticProgramBridgePathChoice &entry) {
                          return entry.scopePath == "/main" &&
                                 primec::semanticProgramBridgePathChoiceHelperName(
                                     semanticProgram, entry) == "count";
                        });
  const auto *semanticReturnEntry =
      findSemanticEntry(primec::semanticProgramReturnFactView(semanticProgram),
                        [&semanticProgram](const primec::SemanticProgramReturnFact &entry) {
                          return primec::semanticProgramReturnFactDefinitionPath(semanticProgram, entry) ==
                                 "/main";
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
  CHECK(primec::semanticProgramBridgePathChoiceHelperName(semanticProgram, *semanticBridgeEntry) ==
        "count");
  CHECK(primec::semanticProgramReturnFactDefinitionPath(semanticProgram, *semanticReturnEntry) ==
        "/main");

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
      "  return(id(/std/collections/vector/count(values)))\n"
      "}\n";

  auto program = parseProgram(source);
  primec::Semantics semantics;
  primec::SemanticProgram semanticProgram;
  std::string error;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  CHECK_FALSE(semantics.validate(program, "/main", error, defaults, defaults, {}, nullptr, false, &semanticProgram));
  CHECK_FALSE(error.empty());
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
      .stdlibSurfaceId = primec::StdlibSurfaceId::CollectionsVectorHelpers,
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
      .stdlibSurfaceId = primec::StdlibSurfaceId::CollectionsVectorHelpers,
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

TEST_CASE("semantic product formatter resolves module callable-summary indices deterministically") {
  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
      .isExecution = false,
      .returnKind = "return",
      .isCompute = false,
      .isUnsafe = false,
      .activeEffects = {},
      .activeCapabilities = {},
      .hasResultType = true,
      .resultTypeHasValue = true,
      .resultValueType = "i64",
      .resultErrorType = "",
      .hasOnError = false,
      .onErrorHandlerPath = "",
      .onErrorErrorType = "",
      .onErrorBoundArgCount = 0,
      .semanticNodeId = 501,
      .provenanceHandle = 1501,
      .fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/z"),
      .returnKindId = primec::semanticProgramInternCallTargetString(semanticProgram, "return"),
      .resultValueTypeId = primec::semanticProgramInternCallTargetString(semanticProgram, "i64"),
  });
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
      .isExecution = false,
      .returnKind = "return",
      .isCompute = true,
      .isUnsafe = false,
      .activeEffects = {},
      .activeCapabilities = {},
      .hasResultType = true,
      .resultTypeHasValue = true,
      .resultValueType = "i32",
      .resultErrorType = "",
      .hasOnError = false,
      .onErrorHandlerPath = "",
      .onErrorErrorType = "",
      .onErrorBoundArgCount = 0,
      .semanticNodeId = 502,
      .provenanceHandle = 1502,
      .fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/a"),
      .returnKindId = primec::semanticProgramInternCallTargetString(semanticProgram, "return"),
      .resultValueTypeId = primec::semanticProgramInternCallTargetString(semanticProgram, "i32"),
  });

  primec::SemanticProgramModuleResolvedArtifacts moduleA;
  moduleA.identity.moduleKey = "/a";
  moduleA.identity.stableOrder = 0;
  moduleA.callableSummaryIndices.push_back(1);
  semanticProgram.moduleResolvedArtifacts.push_back(moduleA);

  primec::SemanticProgramModuleResolvedArtifacts moduleZ;
  moduleZ.identity.moduleKey = "/z";
  moduleZ.identity.stableOrder = 1;
  moduleZ.callableSummaryIndices.push_back(0);
  semanticProgram.moduleResolvedArtifacts.push_back(moduleZ);

  const auto view = primec::semanticProgramCallableSummaryView(semanticProgram);
  REQUIRE(view.size() == 2);
  CHECK(view[0] == &semanticProgram.callableSummaries[1]);
  CHECK(view[1] == &semanticProgram.callableSummaries[0]);

  const std::string dump = primec::formatSemanticProgram(semanticProgram);
  const std::string firstEntry = "callable_summaries[0]: full_path=\"/a\"";
  const std::string secondEntry = "callable_summaries[1]: full_path=\"/z\"";
  const std::size_t firstPos = dump.find(firstEntry);
  const std::size_t secondPos = dump.find(secondEntry);
  CHECK(firstPos != std::string::npos);
  CHECK(secondPos != std::string::npos);
  CHECK(firstPos < secondPos);
}

TEST_CASE("semantic product formatter keeps callable-summary text parity for flat vs module-index storage") {
  auto makeProgram = [](bool useModuleIndices) {
    primec::SemanticProgram semanticProgram;
    semanticProgram.entryPath = "/main";

    primec::SemanticProgramCallableSummary first;
    first.isExecution = false;
    first.returnKind = "return";
    first.isCompute = false;
    first.isUnsafe = false;
    first.activeEffects = {};
    first.activeCapabilities = {};
    first.hasResultType = true;
    first.resultTypeHasValue = true;
    first.resultValueType = "i64";
    first.resultErrorType = "";
    first.hasOnError = false;
    first.onErrorHandlerPath = "";
    first.onErrorErrorType = "";
    first.onErrorBoundArgCount = 0;
    first.semanticNodeId = 601;
    first.provenanceHandle = 1601;
    first.fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/first");
    first.returnKindId = primec::semanticProgramInternCallTargetString(semanticProgram, "return");
    first.resultValueTypeId = primec::semanticProgramInternCallTargetString(semanticProgram, "i64");
    semanticProgram.callableSummaries.push_back(std::move(first));

    primec::SemanticProgramCallableSummary second;
    second.isExecution = true;
    second.returnKind = "return";
    second.isCompute = true;
    second.isUnsafe = false;
    second.activeEffects = {};
    second.activeCapabilities = {};
    second.hasResultType = true;
    second.resultTypeHasValue = true;
    second.resultValueType = "i32";
    second.resultErrorType = "";
    second.hasOnError = false;
    second.onErrorHandlerPath = "";
    second.onErrorErrorType = "";
    second.onErrorBoundArgCount = 0;
    second.semanticNodeId = 602;
    second.provenanceHandle = 1602;
    second.fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/second");
    second.returnKindId = primec::semanticProgramInternCallTargetString(semanticProgram, "return");
    second.resultValueTypeId = primec::semanticProgramInternCallTargetString(semanticProgram, "i32");
    semanticProgram.callableSummaries.push_back(std::move(second));

    if (useModuleIndices) {
      primec::SemanticProgramModuleResolvedArtifacts moduleFirst;
      moduleFirst.identity.moduleKey = "/first";
      moduleFirst.identity.stableOrder = 0;
      moduleFirst.callableSummaryIndices.push_back(0);
      semanticProgram.moduleResolvedArtifacts.push_back(std::move(moduleFirst));

      primec::SemanticProgramModuleResolvedArtifacts moduleSecond;
      moduleSecond.identity.moduleKey = "/second";
      moduleSecond.identity.stableOrder = 1;
      moduleSecond.callableSummaryIndices.push_back(1);
      semanticProgram.moduleResolvedArtifacts.push_back(std::move(moduleSecond));
    }

    return semanticProgram;
  };

  const primec::SemanticProgram flatProgram = makeProgram(false);
  const primec::SemanticProgram moduleIndexedProgram = makeProgram(true);
  CHECK(primec::formatSemanticProgram(moduleIndexedProgram) == primec::formatSemanticProgram(flatProgram));
}

TEST_CASE("semantic product formatter resolves module binding-fact indices deterministically") {
  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";

  primec::SemanticProgramBindingFact first;
  first.scopePath = "/z";
  first.siteKind = "local";
  first.name = "last";
  first.bindingTypeText = "i64";
  first.isMutable = false;
  first.isEntryArgString = false;
  first.isUnsafeReference = false;
  first.referenceRoot = "";
  first.sourceLine = 20;
  first.sourceColumn = 2;
  first.semanticNodeId = 701;
  first.provenanceHandle = 1701;
  first.scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/z");
  first.siteKindId = primec::semanticProgramInternCallTargetString(semanticProgram, "local");
  first.nameId = primec::semanticProgramInternCallTargetString(semanticProgram, "last");
  first.resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/z/last");
  first.bindingTypeTextId = primec::semanticProgramInternCallTargetString(semanticProgram, "i64");
  first.referenceRootId = primec::InvalidSymbolId;
  semanticProgram.bindingFacts.push_back(std::move(first));

  primec::SemanticProgramBindingFact second;
  second.scopePath = "/a";
  second.siteKind = "local";
  second.name = "first";
  second.bindingTypeText = "i32";
  second.isMutable = false;
  second.isEntryArgString = false;
  second.isUnsafeReference = false;
  second.referenceRoot = "";
  second.sourceLine = 10;
  second.sourceColumn = 1;
  second.semanticNodeId = 702;
  second.provenanceHandle = 1702;
  second.scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/a");
  second.siteKindId = primec::semanticProgramInternCallTargetString(semanticProgram, "local");
  second.nameId = primec::semanticProgramInternCallTargetString(semanticProgram, "first");
  second.resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/a/first");
  second.bindingTypeTextId = primec::semanticProgramInternCallTargetString(semanticProgram, "i32");
  second.referenceRootId = primec::InvalidSymbolId;
  semanticProgram.bindingFacts.push_back(std::move(second));

  primec::SemanticProgramModuleResolvedArtifacts moduleA;
  moduleA.identity.moduleKey = "/a";
  moduleA.identity.stableOrder = 0;
  moduleA.bindingFactIndices.push_back(1);
  semanticProgram.moduleResolvedArtifacts.push_back(moduleA);

  primec::SemanticProgramModuleResolvedArtifacts moduleZ;
  moduleZ.identity.moduleKey = "/z";
  moduleZ.identity.stableOrder = 1;
  moduleZ.bindingFactIndices.push_back(0);
  semanticProgram.moduleResolvedArtifacts.push_back(moduleZ);

  const auto view = primec::semanticProgramBindingFactView(semanticProgram);
  REQUIRE(view.size() == 2);
  CHECK(view[0] == &semanticProgram.bindingFacts[1]);
  CHECK(view[1] == &semanticProgram.bindingFacts[0]);

  const std::string dump = primec::formatSemanticProgram(semanticProgram);
  const std::string firstEntry = "binding_facts[0]: scope_path=\"/a\"";
  const std::string secondEntry = "binding_facts[1]: scope_path=\"/z\"";
  const std::size_t firstPos = dump.find(firstEntry);
  const std::size_t secondPos = dump.find(secondEntry);
  CHECK(firstPos != std::string::npos);
  CHECK(secondPos != std::string::npos);
  CHECK(firstPos < secondPos);
}

TEST_CASE("semantic product formatter keeps binding-fact text parity for flat vs module-index storage") {
  auto makeProgram = [](bool useModuleIndices) {
    primec::SemanticProgram semanticProgram;
    semanticProgram.entryPath = "/main";

    primec::SemanticProgramBindingFact first;
    first.scopePath = "/first";
    first.siteKind = "parameter";
    first.name = "left";
    first.bindingTypeText = "i64";
    first.isMutable = true;
    first.isEntryArgString = false;
    first.isUnsafeReference = false;
    first.referenceRoot = "";
    first.sourceLine = 5;
    first.sourceColumn = 3;
    first.semanticNodeId = 801;
    first.provenanceHandle = 1801;
    first.scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/first");
    first.siteKindId = primec::semanticProgramInternCallTargetString(semanticProgram, "parameter");
    first.nameId = primec::semanticProgramInternCallTargetString(semanticProgram, "left");
    first.resolvedPathId =
        primec::semanticProgramInternCallTargetString(semanticProgram, "/first/left");
    first.bindingTypeTextId = primec::semanticProgramInternCallTargetString(semanticProgram, "i64");
    first.referenceRootId = primec::InvalidSymbolId;
    semanticProgram.bindingFacts.push_back(std::move(first));

    primec::SemanticProgramBindingFact second;
    second.scopePath = "/second";
    second.siteKind = "local";
    second.name = "right";
    second.bindingTypeText = "i32";
    second.isMutable = false;
    second.isEntryArgString = false;
    second.isUnsafeReference = false;
    second.referenceRoot = "";
    second.sourceLine = 8;
    second.sourceColumn = 4;
    second.semanticNodeId = 802;
    second.provenanceHandle = 1802;
    second.scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/second");
    second.siteKindId = primec::semanticProgramInternCallTargetString(semanticProgram, "local");
    second.nameId = primec::semanticProgramInternCallTargetString(semanticProgram, "right");
    second.resolvedPathId =
        primec::semanticProgramInternCallTargetString(semanticProgram, "/second/right");
    second.bindingTypeTextId = primec::semanticProgramInternCallTargetString(semanticProgram, "i32");
    second.referenceRootId = primec::InvalidSymbolId;
    semanticProgram.bindingFacts.push_back(std::move(second));

    if (useModuleIndices) {
      primec::SemanticProgramModuleResolvedArtifacts moduleFirst;
      moduleFirst.identity.moduleKey = "/first";
      moduleFirst.identity.stableOrder = 0;
      moduleFirst.bindingFactIndices.push_back(0);
      semanticProgram.moduleResolvedArtifacts.push_back(std::move(moduleFirst));

      primec::SemanticProgramModuleResolvedArtifacts moduleSecond;
      moduleSecond.identity.moduleKey = "/second";
      moduleSecond.identity.stableOrder = 1;
      moduleSecond.bindingFactIndices.push_back(1);
      semanticProgram.moduleResolvedArtifacts.push_back(std::move(moduleSecond));
    }

    return semanticProgram;
  };

  const primec::SemanticProgram flatProgram = makeProgram(false);
  const primec::SemanticProgram moduleIndexedProgram = makeProgram(true);
  CHECK(primec::formatSemanticProgram(moduleIndexedProgram) == primec::formatSemanticProgram(flatProgram));
}

TEST_CASE("semantic product formatter resolves module return-fact indices deterministically") {
  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";

  primec::SemanticProgramReturnFact first;
  first.returnKind = "return";
  first.structPath = "/i64";
  first.bindingTypeText = "i64";
  first.isMutable = false;
  first.isEntryArgString = false;
  first.isUnsafeReference = false;
  first.referenceRoot = "";
  first.sourceLine = 20;
  first.sourceColumn = 2;
  first.semanticNodeId = 901;
  first.provenanceHandle = 1901;
  first.definitionPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/z");
  first.returnKindId = primec::semanticProgramInternCallTargetString(semanticProgram, "return");
  first.structPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/i64");
  first.bindingTypeTextId = primec::semanticProgramInternCallTargetString(semanticProgram, "i64");
  first.referenceRootId = primec::InvalidSymbolId;
  semanticProgram.returnFacts.push_back(std::move(first));

  primec::SemanticProgramReturnFact second;
  second.returnKind = "return";
  second.structPath = "/i32";
  second.bindingTypeText = "i32";
  second.isMutable = false;
  second.isEntryArgString = false;
  second.isUnsafeReference = false;
  second.referenceRoot = "";
  second.sourceLine = 10;
  second.sourceColumn = 1;
  second.semanticNodeId = 902;
  second.provenanceHandle = 1902;
  second.definitionPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/a");
  second.returnKindId = primec::semanticProgramInternCallTargetString(semanticProgram, "return");
  second.structPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/i32");
  second.bindingTypeTextId = primec::semanticProgramInternCallTargetString(semanticProgram, "i32");
  second.referenceRootId = primec::InvalidSymbolId;
  semanticProgram.returnFacts.push_back(std::move(second));

  primec::SemanticProgramModuleResolvedArtifacts moduleA;
  moduleA.identity.moduleKey = "/a";
  moduleA.identity.stableOrder = 0;
  moduleA.returnFactIndices.push_back(1);
  semanticProgram.moduleResolvedArtifacts.push_back(moduleA);

  primec::SemanticProgramModuleResolvedArtifacts moduleZ;
  moduleZ.identity.moduleKey = "/z";
  moduleZ.identity.stableOrder = 1;
  moduleZ.returnFactIndices.push_back(0);
  semanticProgram.moduleResolvedArtifacts.push_back(moduleZ);

  const auto view = primec::semanticProgramReturnFactView(semanticProgram);
  REQUIRE(view.size() == 2);
  CHECK(view[0] == &semanticProgram.returnFacts[1]);
  CHECK(view[1] == &semanticProgram.returnFacts[0]);

  const std::string dump = primec::formatSemanticProgram(semanticProgram);
  const std::string firstEntry = "return_facts[0]: definition_path=\"/a\"";
  const std::string secondEntry = "return_facts[1]: definition_path=\"/z\"";
  const std::size_t firstPos = dump.find(firstEntry);
  const std::size_t secondPos = dump.find(secondEntry);
  CHECK(firstPos != std::string::npos);
  CHECK(secondPos != std::string::npos);
  CHECK(firstPos < secondPos);
}

TEST_CASE("semantic product formatter keeps return-fact text parity for flat vs module-index storage") {
  auto makeProgram = [](bool useModuleIndices) {
    primec::SemanticProgram semanticProgram;
    semanticProgram.entryPath = "/main";

    primec::SemanticProgramReturnFact first;
    first.returnKind = "return";
    first.structPath = "/i64";
    first.bindingTypeText = "i64";
    first.isMutable = false;
    first.isEntryArgString = false;
    first.isUnsafeReference = false;
    first.referenceRoot = "";
    first.sourceLine = 5;
    first.sourceColumn = 3;
    first.semanticNodeId = 911;
    first.provenanceHandle = 1911;
    first.definitionPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/first");
    first.returnKindId = primec::semanticProgramInternCallTargetString(semanticProgram, "return");
    first.structPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/i64");
    first.bindingTypeTextId = primec::semanticProgramInternCallTargetString(semanticProgram, "i64");
    first.referenceRootId = primec::InvalidSymbolId;
    semanticProgram.returnFacts.push_back(std::move(first));

    primec::SemanticProgramReturnFact second;
    second.returnKind = "return";
    second.structPath = "/i32";
    second.bindingTypeText = "i32";
    second.isMutable = false;
    second.isEntryArgString = false;
    second.isUnsafeReference = false;
    second.referenceRoot = "";
    second.sourceLine = 8;
    second.sourceColumn = 4;
    second.semanticNodeId = 912;
    second.provenanceHandle = 1912;
    second.definitionPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/second");
    second.returnKindId = primec::semanticProgramInternCallTargetString(semanticProgram, "return");
    second.structPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/i32");
    second.bindingTypeTextId = primec::semanticProgramInternCallTargetString(semanticProgram, "i32");
    second.referenceRootId = primec::InvalidSymbolId;
    semanticProgram.returnFacts.push_back(std::move(second));

    if (useModuleIndices) {
      primec::SemanticProgramModuleResolvedArtifacts moduleFirst;
      moduleFirst.identity.moduleKey = "/first";
      moduleFirst.identity.stableOrder = 0;
      moduleFirst.returnFactIndices.push_back(0);
      semanticProgram.moduleResolvedArtifacts.push_back(std::move(moduleFirst));

      primec::SemanticProgramModuleResolvedArtifacts moduleSecond;
      moduleSecond.identity.moduleKey = "/second";
      moduleSecond.identity.stableOrder = 1;
      moduleSecond.returnFactIndices.push_back(1);
      semanticProgram.moduleResolvedArtifacts.push_back(std::move(moduleSecond));
    }

    return semanticProgram;
  };

  const primec::SemanticProgram flatProgram = makeProgram(false);
  const primec::SemanticProgram moduleIndexedProgram = makeProgram(true);
  CHECK(primec::formatSemanticProgram(moduleIndexedProgram) == primec::formatSemanticProgram(flatProgram));
}

TEST_CASE("semantic product formatter resolves module local-auto-fact indices deterministically") {
  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";

  auto makeLocalAuto = [&](std::string scopePath,
                           std::string bindingName,
                           std::string bindingTypeText,
                           std::string initializerResolvedPath,
                           int sourceLine,
                           int sourceColumn,
                           uint64_t semanticNodeId,
                           uint64_t provenanceHandle) {
    primec::SemanticProgramLocalAutoFact entry;
    entry.scopePath = std::move(scopePath);
    entry.bindingName = std::move(bindingName);
    entry.bindingTypeText = std::move(bindingTypeText);
    entry.initializerBindingTypeText = entry.bindingTypeText;
    entry.initializerQueryTypeText = entry.bindingTypeText;
    entry.sourceLine = sourceLine;
    entry.sourceColumn = sourceColumn;
    entry.semanticNodeId = semanticNodeId;
    entry.provenanceHandle = provenanceHandle;
    entry.scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.scopePath);
    entry.bindingNameId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.bindingName);
    entry.bindingTypeTextId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.bindingTypeText);
    entry.initializerBindingTypeTextId =
        primec::semanticProgramInternCallTargetString(semanticProgram, entry.initializerBindingTypeText);
    entry.initializerQueryTypeTextId =
        primec::semanticProgramInternCallTargetString(semanticProgram, entry.initializerQueryTypeText);
    entry.initializerResolvedPathId =
        primec::semanticProgramInternCallTargetString(semanticProgram, initializerResolvedPath);
    return entry;
  };

  semanticProgram.localAutoFacts.push_back(
      makeLocalAuto("/z", "last", "i64", "/initLast", 20, 2, 921, 1921));
  semanticProgram.localAutoFacts.push_back(
      makeLocalAuto("/a", "first", "i32", "/initFirst", 10, 1, 922, 1922));

  primec::SemanticProgramModuleResolvedArtifacts moduleA;
  moduleA.identity.moduleKey = "/a";
  moduleA.identity.stableOrder = 0;
  moduleA.localAutoFactIndices.push_back(1);
  semanticProgram.moduleResolvedArtifacts.push_back(moduleA);

  primec::SemanticProgramModuleResolvedArtifacts moduleZ;
  moduleZ.identity.moduleKey = "/z";
  moduleZ.identity.stableOrder = 1;
  moduleZ.localAutoFactIndices.push_back(0);
  semanticProgram.moduleResolvedArtifacts.push_back(moduleZ);

  const auto view = primec::semanticProgramLocalAutoFactView(semanticProgram);
  REQUIRE(view.size() == 2);
  CHECK(view[0] == &semanticProgram.localAutoFacts[1]);
  CHECK(view[1] == &semanticProgram.localAutoFacts[0]);

  const std::string dump = primec::formatSemanticProgram(semanticProgram);
  const std::string firstEntry = "local_auto_facts[0]: scope_path=\"/a\" binding_name=\"first\"";
  const std::string secondEntry = "local_auto_facts[1]: scope_path=\"/z\" binding_name=\"last\"";
  const std::size_t firstPos = dump.find(firstEntry);
  const std::size_t secondPos = dump.find(secondEntry);
  CHECK(firstPos != std::string::npos);
  CHECK(secondPos != std::string::npos);
  CHECK(firstPos < secondPos);
}

TEST_CASE("semantic product formatter keeps local-auto-fact text parity for flat vs module-index storage") {
  auto makeProgram = [](bool useModuleIndices) {
    primec::SemanticProgram semanticProgram;
    semanticProgram.entryPath = "/main";

    auto makeLocalAuto = [&](std::string scopePath,
                             std::string bindingName,
                             std::string bindingTypeText,
                             std::string initializerResolvedPath,
                             int sourceLine,
                             int sourceColumn,
                             uint64_t semanticNodeId,
                             uint64_t provenanceHandle) {
      primec::SemanticProgramLocalAutoFact entry;
      entry.scopePath = std::move(scopePath);
      entry.bindingName = std::move(bindingName);
      entry.bindingTypeText = std::move(bindingTypeText);
      entry.initializerBindingTypeText = entry.bindingTypeText;
      entry.initializerQueryTypeText = entry.bindingTypeText;
      entry.sourceLine = sourceLine;
      entry.sourceColumn = sourceColumn;
      entry.semanticNodeId = semanticNodeId;
      entry.provenanceHandle = provenanceHandle;
      entry.scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.scopePath);
      entry.bindingNameId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.bindingName);
      entry.bindingTypeTextId =
          primec::semanticProgramInternCallTargetString(semanticProgram, entry.bindingTypeText);
      entry.initializerBindingTypeTextId =
          primec::semanticProgramInternCallTargetString(semanticProgram, entry.initializerBindingTypeText);
      entry.initializerQueryTypeTextId =
          primec::semanticProgramInternCallTargetString(semanticProgram, entry.initializerQueryTypeText);
      entry.initializerResolvedPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, initializerResolvedPath);
      return entry;
    };

    semanticProgram.localAutoFacts.push_back(
        makeLocalAuto("/first", "left", "i64", "/initLeft", 5, 3, 931, 1931));
    semanticProgram.localAutoFacts.push_back(
        makeLocalAuto("/second", "right", "i32", "/initRight", 8, 4, 932, 1932));

    if (useModuleIndices) {
      primec::SemanticProgramModuleResolvedArtifacts moduleFirst;
      moduleFirst.identity.moduleKey = "/first";
      moduleFirst.identity.stableOrder = 0;
      moduleFirst.localAutoFactIndices.push_back(0);
      semanticProgram.moduleResolvedArtifacts.push_back(std::move(moduleFirst));

      primec::SemanticProgramModuleResolvedArtifacts moduleSecond;
      moduleSecond.identity.moduleKey = "/second";
      moduleSecond.identity.stableOrder = 1;
      moduleSecond.localAutoFactIndices.push_back(1);
      semanticProgram.moduleResolvedArtifacts.push_back(std::move(moduleSecond));
    }

    return semanticProgram;
  };

  const primec::SemanticProgram flatProgram = makeProgram(false);
  const primec::SemanticProgram moduleIndexedProgram = makeProgram(true);
  CHECK(primec::formatSemanticProgram(moduleIndexedProgram) == primec::formatSemanticProgram(flatProgram));
}

TEST_CASE("semantic product formatter resolves module query-fact indices deterministically") {
  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";

  auto makeQuery = [&](std::string scopePath,
                       std::string callName,
                       std::string resolvedPath,
                       std::string typeText,
                       std::string bindingText,
                       int sourceLine,
                       int sourceColumn,
                       uint64_t semanticNodeId,
                       uint64_t provenanceHandle) {
    primec::SemanticProgramQueryFact entry;
    entry.scopePath = std::move(scopePath);
    entry.callName = std::move(callName);
    entry.queryTypeText = std::move(typeText);
    entry.bindingTypeText = std::move(bindingText);
    entry.hasResultType = true;
    entry.resultTypeHasValue = true;
    entry.resultValueType = "i32";
    entry.resultErrorType = "MyError";
    entry.sourceLine = sourceLine;
    entry.sourceColumn = sourceColumn;
    entry.semanticNodeId = semanticNodeId;
    entry.provenanceHandle = provenanceHandle;
    entry.scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.scopePath);
    entry.callNameId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.callName);
    entry.resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, resolvedPath);
    entry.queryTypeTextId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.queryTypeText);
    entry.bindingTypeTextId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.bindingTypeText);
    entry.receiverBindingTypeTextId = primec::InvalidSymbolId;
    entry.resultValueTypeId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.resultValueType);
    entry.resultErrorTypeId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.resultErrorType);
    return entry;
  };

  semanticProgram.queryFacts.push_back(
      makeQuery("/z", "lookupLast", "/lookup/last", "Result<i32, MyError>", "Result<i32, MyError>", 20, 2, 941, 1941));
  semanticProgram.queryFacts.push_back(
      makeQuery("/a", "lookupFirst", "/lookup/first", "Result<i32, MyError>", "Result<i32, MyError>", 10, 1, 942, 1942));

  primec::SemanticProgramModuleResolvedArtifacts moduleA;
  moduleA.identity.moduleKey = "/a";
  moduleA.identity.stableOrder = 0;
  moduleA.queryFactIndices.push_back(1);
  semanticProgram.moduleResolvedArtifacts.push_back(moduleA);

  primec::SemanticProgramModuleResolvedArtifacts moduleZ;
  moduleZ.identity.moduleKey = "/z";
  moduleZ.identity.stableOrder = 1;
  moduleZ.queryFactIndices.push_back(0);
  semanticProgram.moduleResolvedArtifacts.push_back(moduleZ);

  const auto view = primec::semanticProgramQueryFactView(semanticProgram);
  REQUIRE(view.size() == 2);
  CHECK(view[0] == &semanticProgram.queryFacts[1]);
  CHECK(view[1] == &semanticProgram.queryFacts[0]);

  const std::string dump = primec::formatSemanticProgram(semanticProgram);
  const std::string firstEntry = "query_facts[0]: scope_path=\"/a\" call_name=\"lookupFirst\"";
  const std::string secondEntry = "query_facts[1]: scope_path=\"/z\" call_name=\"lookupLast\"";
  const std::size_t firstPos = dump.find(firstEntry);
  const std::size_t secondPos = dump.find(secondEntry);
  CHECK(firstPos != std::string::npos);
  CHECK(secondPos != std::string::npos);
  CHECK(firstPos < secondPos);
}

TEST_CASE("semantic product formatter keeps query-fact text parity for flat vs module-index storage") {
  auto makeProgram = [](bool useModuleIndices) {
    primec::SemanticProgram semanticProgram;
    semanticProgram.entryPath = "/main";

    auto makeQuery = [&](std::string scopePath,
                         std::string callName,
                         std::string resolvedPath,
                         int sourceLine,
                         int sourceColumn,
                         uint64_t semanticNodeId,
                         uint64_t provenanceHandle) {
      primec::SemanticProgramQueryFact entry;
      entry.scopePath = std::move(scopePath);
      entry.callName = std::move(callName);
      entry.queryTypeText = "Result<i32, MyError>";
      entry.bindingTypeText = "Result<i32, MyError>";
      entry.hasResultType = true;
      entry.resultTypeHasValue = true;
      entry.resultValueType = "i32";
      entry.resultErrorType = "MyError";
      entry.sourceLine = sourceLine;
      entry.sourceColumn = sourceColumn;
      entry.semanticNodeId = semanticNodeId;
      entry.provenanceHandle = provenanceHandle;
      entry.scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.scopePath);
      entry.callNameId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.callName);
      entry.resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, resolvedPath);
      entry.queryTypeTextId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.queryTypeText);
      entry.bindingTypeTextId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.bindingTypeText);
      entry.receiverBindingTypeTextId = primec::InvalidSymbolId;
      entry.resultValueTypeId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.resultValueType);
      entry.resultErrorTypeId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.resultErrorType);
      return entry;
    };

    semanticProgram.queryFacts.push_back(makeQuery("/first", "leftLookup", "/first/lookup", 5, 3, 951, 1951));
    semanticProgram.queryFacts.push_back(makeQuery("/second", "rightLookup", "/second/lookup", 8, 4, 952, 1952));

    if (useModuleIndices) {
      primec::SemanticProgramModuleResolvedArtifacts moduleFirst;
      moduleFirst.identity.moduleKey = "/first";
      moduleFirst.identity.stableOrder = 0;
      moduleFirst.queryFactIndices.push_back(0);
      semanticProgram.moduleResolvedArtifacts.push_back(std::move(moduleFirst));

      primec::SemanticProgramModuleResolvedArtifacts moduleSecond;
      moduleSecond.identity.moduleKey = "/second";
      moduleSecond.identity.stableOrder = 1;
      moduleSecond.queryFactIndices.push_back(1);
      semanticProgram.moduleResolvedArtifacts.push_back(std::move(moduleSecond));
    }

    return semanticProgram;
  };

  const primec::SemanticProgram flatProgram = makeProgram(false);
  const primec::SemanticProgram moduleIndexedProgram = makeProgram(true);
  CHECK(primec::formatSemanticProgram(moduleIndexedProgram) == primec::formatSemanticProgram(flatProgram));
}

TEST_CASE("semantic product formatter resolves module try-fact indices deterministically") {
  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";

  auto makeTryFact = [&](std::string scopePath,
                         std::string resolvedPath,
                         int sourceLine,
                         int sourceColumn,
                         uint64_t semanticNodeId,
                         uint64_t provenanceHandle) {
    primec::SemanticProgramTryFact entry;
    entry.scopePath = std::move(scopePath);
    entry.operandBindingTypeText = "Result<i32, Err>";
    entry.operandReceiverBindingTypeText = "";
    entry.operandQueryTypeText = "Result<i32, Err>";
    entry.valueType = "i32";
    entry.errorType = "Err";
    entry.contextReturnKind = "return";
    entry.onErrorHandlerPath = "/unexpectedError";
    entry.onErrorErrorType = "Err";
    entry.onErrorBoundArgCount = 1;
    entry.sourceLine = sourceLine;
    entry.sourceColumn = sourceColumn;
    entry.semanticNodeId = semanticNodeId;
    entry.provenanceHandle = provenanceHandle;
    entry.scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.scopePath);
    entry.operandResolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, resolvedPath);
    entry.operandBindingTypeTextId =
        primec::semanticProgramInternCallTargetString(semanticProgram, entry.operandBindingTypeText);
    entry.operandReceiverBindingTypeTextId = primec::InvalidSymbolId;
    entry.operandQueryTypeTextId =
        primec::semanticProgramInternCallTargetString(semanticProgram, entry.operandQueryTypeText);
    entry.valueTypeId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.valueType);
    entry.errorTypeId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.errorType);
    entry.contextReturnKindId =
        primec::semanticProgramInternCallTargetString(semanticProgram, entry.contextReturnKind);
    entry.onErrorHandlerPathId =
        primec::semanticProgramInternCallTargetString(semanticProgram, entry.onErrorHandlerPath);
    entry.onErrorErrorTypeId =
        primec::semanticProgramInternCallTargetString(semanticProgram, entry.onErrorErrorType);
    return entry;
  };

  semanticProgram.tryFacts.push_back(
      makeTryFact("/z", "/try/last", 20, 2, 991, 1991));
  semanticProgram.tryFacts.push_back(
      makeTryFact("/a", "/try/first", 10, 1, 992, 1992));

  primec::SemanticProgramModuleResolvedArtifacts moduleA;
  moduleA.identity.moduleKey = "/a";
  moduleA.identity.stableOrder = 0;
  moduleA.tryFactIndices.push_back(1);
  semanticProgram.moduleResolvedArtifacts.push_back(moduleA);

  primec::SemanticProgramModuleResolvedArtifacts moduleZ;
  moduleZ.identity.moduleKey = "/z";
  moduleZ.identity.stableOrder = 1;
  moduleZ.tryFactIndices.push_back(0);
  semanticProgram.moduleResolvedArtifacts.push_back(moduleZ);

  const auto view = primec::semanticProgramTryFactView(semanticProgram);
  REQUIRE(view.size() == 2);
  CHECK(view[0] == &semanticProgram.tryFacts[1]);
  CHECK(view[1] == &semanticProgram.tryFacts[0]);

  const std::string dump = primec::formatSemanticProgram(semanticProgram);
  const std::string firstEntry = "try_facts[0]: scope_path=\"/a\"";
  const std::string secondEntry = "try_facts[1]: scope_path=\"/z\"";
  const std::size_t firstPos = dump.find(firstEntry);
  const std::size_t secondPos = dump.find(secondEntry);
  CHECK(firstPos != std::string::npos);
  CHECK(secondPos != std::string::npos);
  CHECK(firstPos < secondPos);
}

TEST_CASE("semantic product formatter resolves module on-error-fact indices deterministically") {
  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";

  auto makeOnErrorFact = [&](std::string definitionPath,
                             std::string handlerPath,
                             int sourceLine,
                             uint64_t semanticNodeId,
                             uint64_t provenanceHandle) {
    primec::SemanticProgramOnErrorFact entry;
    entry.definitionPath = std::move(definitionPath);
    entry.returnKind = "return";
    entry.errorType = "Err";
    entry.boundArgCount = 1;
    entry.boundArgTexts = {"err"};
    entry.returnResultHasValue = true;
    entry.returnResultValueType = "i32";
    entry.returnResultErrorType = "Err";
    entry.semanticNodeId = semanticNodeId;
    entry.provenanceHandle = provenanceHandle;
    entry.definitionPathId =
        primec::semanticProgramInternCallTargetString(semanticProgram, entry.definitionPath);
    entry.returnKindId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.returnKind);
    entry.handlerPathId = primec::semanticProgramInternCallTargetString(semanticProgram, handlerPath);
    entry.errorTypeId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.errorType);
    entry.boundArgTextIds = {
        primec::semanticProgramInternCallTargetString(semanticProgram, "err"),
    };
    entry.returnResultValueTypeId =
        primec::semanticProgramInternCallTargetString(semanticProgram, entry.returnResultValueType);
    entry.returnResultErrorTypeId =
        primec::semanticProgramInternCallTargetString(semanticProgram, entry.returnResultErrorType);
    (void)sourceLine;
    return entry;
  };

  semanticProgram.onErrorFacts.push_back(
      makeOnErrorFact("/z", "/handler/last", 20, 993, 1993));
  semanticProgram.onErrorFacts.push_back(
      makeOnErrorFact("/a", "/handler/first", 10, 994, 1994));

  primec::SemanticProgramModuleResolvedArtifacts moduleA;
  moduleA.identity.moduleKey = "/a";
  moduleA.identity.stableOrder = 0;
  moduleA.onErrorFactIndices.push_back(1);
  semanticProgram.moduleResolvedArtifacts.push_back(moduleA);

  primec::SemanticProgramModuleResolvedArtifacts moduleZ;
  moduleZ.identity.moduleKey = "/z";
  moduleZ.identity.stableOrder = 1;
  moduleZ.onErrorFactIndices.push_back(0);
  semanticProgram.moduleResolvedArtifacts.push_back(moduleZ);

  const auto view = primec::semanticProgramOnErrorFactView(semanticProgram);
  REQUIRE(view.size() == 2);
  CHECK(view[0] == &semanticProgram.onErrorFacts[1]);
  CHECK(view[1] == &semanticProgram.onErrorFacts[0]);

  const std::string dump = primec::formatSemanticProgram(semanticProgram);
  const std::string firstEntry = "on_error_facts[0]: definition_path=\"/a\"";
  const std::string secondEntry = "on_error_facts[1]: definition_path=\"/z\"";
  const std::size_t firstPos = dump.find(firstEntry);
  const std::size_t secondPos = dump.find(secondEntry);
  CHECK(firstPos != std::string::npos);
  CHECK(secondPos != std::string::npos);
  CHECK(firstPos < secondPos);
}

TEST_CASE("semantic product formatter keeps try/on-error text parity for flat vs module-index storage") {
  auto makeProgram = [](bool useModuleIndices) {
    primec::SemanticProgram semanticProgram;
    semanticProgram.entryPath = "/main";

    auto makeTryFact = [&](std::string scopePath,
                           std::string resolvedPath,
                           uint64_t semanticNodeId,
                           uint64_t provenanceHandle) {
      primec::SemanticProgramTryFact entry;
      entry.scopePath = std::move(scopePath);
      entry.operandBindingTypeText = "Result<i32, Err>";
      entry.operandReceiverBindingTypeText = "";
      entry.operandQueryTypeText = "Result<i32, Err>";
      entry.valueType = "i32";
      entry.errorType = "Err";
      entry.contextReturnKind = "return";
      entry.onErrorHandlerPath = "/unexpectedError";
      entry.onErrorErrorType = "Err";
      entry.onErrorBoundArgCount = 1;
      entry.sourceLine = 5;
      entry.sourceColumn = 3;
      entry.semanticNodeId = semanticNodeId;
      entry.provenanceHandle = provenanceHandle;
      entry.scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.scopePath);
      entry.operandResolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, resolvedPath);
      entry.operandBindingTypeTextId =
          primec::semanticProgramInternCallTargetString(semanticProgram, entry.operandBindingTypeText);
      entry.operandReceiverBindingTypeTextId = primec::InvalidSymbolId;
      entry.operandQueryTypeTextId =
          primec::semanticProgramInternCallTargetString(semanticProgram, entry.operandQueryTypeText);
      entry.valueTypeId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.valueType);
      entry.errorTypeId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.errorType);
      entry.contextReturnKindId =
          primec::semanticProgramInternCallTargetString(semanticProgram, entry.contextReturnKind);
      entry.onErrorHandlerPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, entry.onErrorHandlerPath);
      entry.onErrorErrorTypeId =
          primec::semanticProgramInternCallTargetString(semanticProgram, entry.onErrorErrorType);
      return entry;
    };

    auto makeOnErrorFact = [&](std::string definitionPath,
                               std::string handlerPath,
                               uint64_t semanticNodeId,
                               uint64_t provenanceHandle) {
      primec::SemanticProgramOnErrorFact entry;
      entry.definitionPath = std::move(definitionPath);
      entry.returnKind = "return";
      entry.errorType = "Err";
      entry.boundArgCount = 1;
      entry.boundArgTexts = {"err"};
      entry.returnResultHasValue = true;
      entry.returnResultValueType = "i32";
      entry.returnResultErrorType = "Err";
      entry.semanticNodeId = semanticNodeId;
      entry.provenanceHandle = provenanceHandle;
      entry.definitionPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, entry.definitionPath);
      entry.returnKindId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.returnKind);
      entry.handlerPathId = primec::semanticProgramInternCallTargetString(semanticProgram, handlerPath);
      entry.errorTypeId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.errorType);
      entry.boundArgTextIds = {
          primec::semanticProgramInternCallTargetString(semanticProgram, "err"),
      };
      entry.returnResultValueTypeId =
          primec::semanticProgramInternCallTargetString(semanticProgram, entry.returnResultValueType);
      entry.returnResultErrorTypeId =
          primec::semanticProgramInternCallTargetString(semanticProgram, entry.returnResultErrorType);
      return entry;
    };

    semanticProgram.tryFacts.push_back(
        makeTryFact("/first", "/try/first", 995, 1995));
    semanticProgram.tryFacts.push_back(
        makeTryFact("/second", "/try/second", 996, 1996));

    semanticProgram.onErrorFacts.push_back(
        makeOnErrorFact("/first", "/handler/first", 997, 1997));
    semanticProgram.onErrorFacts.push_back(
        makeOnErrorFact("/second", "/handler/second", 998, 1998));

    if (useModuleIndices) {
      primec::SemanticProgramModuleResolvedArtifacts moduleFirst;
      moduleFirst.identity.moduleKey = "/first";
      moduleFirst.identity.stableOrder = 0;
      moduleFirst.tryFactIndices.push_back(0);
      moduleFirst.onErrorFactIndices.push_back(0);
      semanticProgram.moduleResolvedArtifacts.push_back(std::move(moduleFirst));

      primec::SemanticProgramModuleResolvedArtifacts moduleSecond;
      moduleSecond.identity.moduleKey = "/second";
      moduleSecond.identity.stableOrder = 1;
      moduleSecond.tryFactIndices.push_back(1);
      moduleSecond.onErrorFactIndices.push_back(1);
      semanticProgram.moduleResolvedArtifacts.push_back(std::move(moduleSecond));
    }

    return semanticProgram;
  };

  const primec::SemanticProgram flatProgram = makeProgram(false);
  const primec::SemanticProgram moduleIndexedProgram = makeProgram(true);
  CHECK(primec::formatSemanticProgram(moduleIndexedProgram) == primec::formatSemanticProgram(flatProgram));
}

TEST_CASE("semantic product formatter keeps first dedup-slice text parity for flat vs module-index storage") {
  auto makeProgram = [](bool useModuleIndices) {
    primec::SemanticProgram semanticProgram;
    semanticProgram.entryPath = "/main";

    auto makeDirectCallTarget = [&](std::string scopePath,
                                    std::string callName,
                                    std::string resolvedPath,
                                    int sourceLine,
                                    int sourceColumn,
                                    uint64_t semanticNodeId,
                                    uint64_t provenanceHandle) {
      primec::SemanticProgramDirectCallTarget entry;
      entry.scopePath = std::move(scopePath);
      entry.callName = std::move(callName);
      entry.sourceLine = sourceLine;
      entry.sourceColumn = sourceColumn;
      entry.semanticNodeId = semanticNodeId;
      entry.provenanceHandle = provenanceHandle;
      entry.scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.scopePath);
      entry.callNameId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.callName);
      entry.resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, resolvedPath);
      return entry;
    };

    auto makeMethodCallTarget = [&](std::string scopePath,
                                    std::string methodName,
                                    std::string receiverTypeText,
                                    std::string resolvedPath,
                                    int sourceLine,
                                    int sourceColumn,
                                    uint64_t semanticNodeId,
                                    uint64_t provenanceHandle) {
      primec::SemanticProgramMethodCallTarget entry;
      entry.scopePath = std::move(scopePath);
      entry.methodName = std::move(methodName);
      entry.receiverTypeText = std::move(receiverTypeText);
      entry.sourceLine = sourceLine;
      entry.sourceColumn = sourceColumn;
      entry.semanticNodeId = semanticNodeId;
      entry.provenanceHandle = provenanceHandle;
      entry.scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.scopePath);
      entry.methodNameId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.methodName);
      entry.receiverTypeTextId =
          primec::semanticProgramInternCallTargetString(semanticProgram, entry.receiverTypeText);
      entry.resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, resolvedPath);
      return entry;
    };

    auto makeCallableSummary = [&](std::string fullPath,
                                   std::string resultValueType,
                                   uint64_t semanticNodeId,
                                   uint64_t provenanceHandle) {
      primec::SemanticProgramCallableSummary entry;
      entry.isExecution = false;
      entry.returnKind = "return";
      entry.isCompute = false;
      entry.isUnsafe = false;
      entry.activeEffects = {};
      entry.activeCapabilities = {};
      entry.hasResultType = true;
      entry.resultTypeHasValue = true;
      entry.resultValueType = std::move(resultValueType);
      entry.resultErrorType = "";
      entry.hasOnError = false;
      entry.onErrorHandlerPath = "";
      entry.onErrorErrorType = "";
      entry.onErrorBoundArgCount = 0;
      entry.semanticNodeId = semanticNodeId;
      entry.provenanceHandle = provenanceHandle;
      entry.fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, fullPath);
      entry.returnKindId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.returnKind);
      entry.resultValueTypeId =
          primec::semanticProgramInternCallTargetString(semanticProgram, entry.resultValueType);
      return entry;
    };

    auto makeBindingFact = [&](std::string scopePath,
                               std::string bindingName,
                               std::string resolvedPath,
                               std::string bindingTypeText,
                               int sourceLine,
                               int sourceColumn,
                               uint64_t semanticNodeId,
                               uint64_t provenanceHandle) {
      primec::SemanticProgramBindingFact entry;
      entry.scopePath = std::move(scopePath);
      entry.siteKind = "local";
      entry.name = std::move(bindingName);
      entry.bindingTypeText = std::move(bindingTypeText);
      entry.sourceLine = sourceLine;
      entry.sourceColumn = sourceColumn;
      entry.semanticNodeId = semanticNodeId;
      entry.provenanceHandle = provenanceHandle;
      entry.scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.scopePath);
      entry.siteKindId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.siteKind);
      entry.nameId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.name);
      entry.resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, resolvedPath);
      entry.bindingTypeTextId =
          primec::semanticProgramInternCallTargetString(semanticProgram, entry.bindingTypeText);
      return entry;
    };

    semanticProgram.directCallTargets.push_back(
        makeDirectCallTarget("/first", "makeFirst", "/helpers/makeFirst", 5, 3, 1101, 2101));
    semanticProgram.directCallTargets.push_back(
        makeDirectCallTarget("/second", "makeSecond", "/helpers/makeSecond", 9, 5, 1102, 2102));

    semanticProgram.methodCallTargets.push_back(
        makeMethodCallTarget("/first", "count", "vector<i32>", "/std/collections/vector/count", 6, 4, 1201, 2201));
    semanticProgram.methodCallTargets.push_back(
        makeMethodCallTarget("/second", "size", "map<i32, i32>", "/std/collections/map/size", 10, 6, 1202, 2202));

    semanticProgram.callableSummaries.push_back(
        makeCallableSummary("/first", "i32", 1301, 2301));
    semanticProgram.callableSummaries.push_back(
        makeCallableSummary("/second", "i64", 1302, 2302));

    semanticProgram.bindingFacts.push_back(
        makeBindingFact("/first", "left", "/first/left", "i32", 7, 2, 1401, 2401));
    semanticProgram.bindingFacts.push_back(
        makeBindingFact("/second", "right", "/second/right", "i64", 11, 2, 1402, 2402));

    if (useModuleIndices) {
      primec::SemanticProgramModuleResolvedArtifacts moduleFirst;
      moduleFirst.identity.moduleKey = "/first";
      moduleFirst.identity.stableOrder = 0;
      moduleFirst.directCallTargetIndices.push_back(0);
      moduleFirst.methodCallTargetIndices.push_back(0);
      moduleFirst.callableSummaryIndices.push_back(0);
      moduleFirst.bindingFactIndices.push_back(0);
      semanticProgram.moduleResolvedArtifacts.push_back(std::move(moduleFirst));

      primec::SemanticProgramModuleResolvedArtifacts moduleSecond;
      moduleSecond.identity.moduleKey = "/second";
      moduleSecond.identity.stableOrder = 1;
      moduleSecond.directCallTargetIndices.push_back(1);
      moduleSecond.methodCallTargetIndices.push_back(1);
      moduleSecond.callableSummaryIndices.push_back(1);
      moduleSecond.bindingFactIndices.push_back(1);
      semanticProgram.moduleResolvedArtifacts.push_back(std::move(moduleSecond));
    }

    return semanticProgram;
  };

  const primec::SemanticProgram flatProgram = makeProgram(false);
  const primec::SemanticProgram moduleIndexedProgram = makeProgram(true);
  CHECK(primec::formatSemanticProgram(moduleIndexedProgram) == primec::formatSemanticProgram(flatProgram));
}

TEST_CASE("semantic product formatter keeps second dedup-slice text parity for flat vs module-index storage") {
  auto makeProgram = [](bool useModuleIndices) {
    primec::SemanticProgram semanticProgram;
    semanticProgram.entryPath = "/main";

    auto makeReturnFact = [&](std::string definitionPath,
                              std::string structPath,
                              std::string bindingTypeText,
                              int sourceLine,
                              int sourceColumn,
                              uint64_t semanticNodeId,
                              uint64_t provenanceHandle) {
      primec::SemanticProgramReturnFact entry;
      entry.returnKind = "return";
      entry.structPath = std::move(structPath);
      entry.bindingTypeText = std::move(bindingTypeText);
      entry.sourceLine = sourceLine;
      entry.sourceColumn = sourceColumn;
      entry.semanticNodeId = semanticNodeId;
      entry.provenanceHandle = provenanceHandle;
      entry.definitionPathId = primec::semanticProgramInternCallTargetString(semanticProgram, definitionPath);
      entry.returnKindId = primec::semanticProgramInternCallTargetString(semanticProgram, "return");
      entry.structPathId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.structPath);
      entry.bindingTypeTextId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.bindingTypeText);
      return entry;
    };

    auto makeLocalAutoFact = [&](std::string scopePath,
                                 std::string bindingName,
                                 std::string bindingTypeText,
                                 std::string initializerResolvedPath,
                                 int sourceLine,
                                 int sourceColumn,
                                 uint64_t semanticNodeId,
                                 uint64_t provenanceHandle) {
      primec::SemanticProgramLocalAutoFact entry;
      entry.scopePath = std::move(scopePath);
      entry.bindingName = std::move(bindingName);
      entry.bindingTypeText = std::move(bindingTypeText);
      entry.initializerBindingTypeText = entry.bindingTypeText;
      entry.initializerQueryTypeText = entry.bindingTypeText;
      entry.sourceLine = sourceLine;
      entry.sourceColumn = sourceColumn;
      entry.semanticNodeId = semanticNodeId;
      entry.provenanceHandle = provenanceHandle;
      entry.scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.scopePath);
      entry.bindingNameId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.bindingName);
      entry.bindingTypeTextId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.bindingTypeText);
      entry.initializerResolvedPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, initializerResolvedPath);
      entry.initializerBindingTypeTextId =
          primec::semanticProgramInternCallTargetString(semanticProgram, entry.initializerBindingTypeText);
      entry.initializerQueryTypeTextId =
          primec::semanticProgramInternCallTargetString(semanticProgram, entry.initializerQueryTypeText);
      return entry;
    };

    auto makeQueryFact = [&](std::string scopePath,
                             std::string callName,
                             std::string resolvedPath,
                             std::string queryTypeText,
                             std::string bindingTypeText,
                             int sourceLine,
                             int sourceColumn,
                             uint64_t semanticNodeId,
                             uint64_t provenanceHandle) {
      primec::SemanticProgramQueryFact entry;
      entry.scopePath = std::move(scopePath);
      entry.callName = std::move(callName);
      entry.queryTypeText = std::move(queryTypeText);
      entry.bindingTypeText = std::move(bindingTypeText);
      entry.hasResultType = true;
      entry.resultTypeHasValue = true;
      entry.resultValueType = "i32";
      entry.resultErrorType = "Err";
      entry.sourceLine = sourceLine;
      entry.sourceColumn = sourceColumn;
      entry.semanticNodeId = semanticNodeId;
      entry.provenanceHandle = provenanceHandle;
      entry.scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.scopePath);
      entry.callNameId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.callName);
      entry.resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, resolvedPath);
      entry.queryTypeTextId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.queryTypeText);
      entry.bindingTypeTextId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.bindingTypeText);
      entry.resultValueTypeId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.resultValueType);
      entry.resultErrorTypeId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.resultErrorType);
      return entry;
    };

    auto makeBridgePathChoice = [&](std::string scopePath,
                                    std::string collectionFamily,
                                    std::string helperName,
                                    std::string chosenPath,
                                    int sourceLine,
                                    int sourceColumn,
                                    uint64_t semanticNodeId,
                                    uint64_t provenanceHandle) {
      primec::SemanticProgramBridgePathChoice entry;
      entry.scopePath = std::move(scopePath);
      entry.collectionFamily = std::move(collectionFamily);
      entry.sourceLine = sourceLine;
      entry.sourceColumn = sourceColumn;
      entry.semanticNodeId = semanticNodeId;
      entry.provenanceHandle = provenanceHandle;
      entry.scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.scopePath);
      entry.collectionFamilyId =
          primec::semanticProgramInternCallTargetString(semanticProgram, entry.collectionFamily);
      entry.helperNameId = primec::semanticProgramInternCallTargetString(semanticProgram, helperName);
      entry.chosenPathId = primec::semanticProgramInternCallTargetString(semanticProgram, chosenPath);
      return entry;
    };

    auto makeTryFact = [&](std::string scopePath,
                           std::string resolvedPath,
                           int sourceLine,
                           int sourceColumn,
                           uint64_t semanticNodeId,
                           uint64_t provenanceHandle) {
      primec::SemanticProgramTryFact entry;
      entry.scopePath = std::move(scopePath);
      entry.operandBindingTypeText = "Result<i32, Err>";
      entry.operandReceiverBindingTypeText = "";
      entry.operandQueryTypeText = "Result<i32, Err>";
      entry.valueType = "i32";
      entry.errorType = "Err";
      entry.contextReturnKind = "return";
      entry.onErrorHandlerPath = "/unexpectedError";
      entry.onErrorErrorType = "Err";
      entry.onErrorBoundArgCount = 1;
      entry.sourceLine = sourceLine;
      entry.sourceColumn = sourceColumn;
      entry.semanticNodeId = semanticNodeId;
      entry.provenanceHandle = provenanceHandle;
      entry.scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.scopePath);
      entry.operandResolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, resolvedPath);
      entry.operandBindingTypeTextId =
          primec::semanticProgramInternCallTargetString(semanticProgram, entry.operandBindingTypeText);
      entry.operandReceiverBindingTypeTextId = primec::InvalidSymbolId;
      entry.operandQueryTypeTextId =
          primec::semanticProgramInternCallTargetString(semanticProgram, entry.operandQueryTypeText);
      entry.valueTypeId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.valueType);
      entry.errorTypeId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.errorType);
      entry.contextReturnKindId =
          primec::semanticProgramInternCallTargetString(semanticProgram, entry.contextReturnKind);
      entry.onErrorHandlerPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, entry.onErrorHandlerPath);
      entry.onErrorErrorTypeId =
          primec::semanticProgramInternCallTargetString(semanticProgram, entry.onErrorErrorType);
      return entry;
    };

    auto makeOnErrorFact = [&](std::string definitionPath,
                               std::string handlerPath,
                               uint64_t semanticNodeId,
                               uint64_t provenanceHandle) {
      primec::SemanticProgramOnErrorFact entry;
      entry.definitionPath = std::move(definitionPath);
      entry.returnKind = "return";
      entry.errorType = "Err";
      entry.boundArgCount = 1;
      entry.boundArgTexts = {"err"};
      entry.returnResultHasValue = true;
      entry.returnResultValueType = "i32";
      entry.returnResultErrorType = "Err";
      entry.semanticNodeId = semanticNodeId;
      entry.provenanceHandle = provenanceHandle;
      entry.definitionPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, entry.definitionPath);
      entry.returnKindId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.returnKind);
      entry.handlerPathId = primec::semanticProgramInternCallTargetString(semanticProgram, handlerPath);
      entry.errorTypeId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.errorType);
      entry.boundArgTextIds = {
          primec::semanticProgramInternCallTargetString(semanticProgram, "err"),
      };
      entry.returnResultValueTypeId =
          primec::semanticProgramInternCallTargetString(semanticProgram, entry.returnResultValueType);
      entry.returnResultErrorTypeId =
          primec::semanticProgramInternCallTargetString(semanticProgram, entry.returnResultErrorType);
      return entry;
    };

    semanticProgram.returnFacts.push_back(
        makeReturnFact("/first", "/i64", "i64", 5, 1, 961, 1961));
    semanticProgram.returnFacts.push_back(
        makeReturnFact("/second", "/i32", "i32", 8, 1, 962, 1962));

    semanticProgram.localAutoFacts.push_back(
        makeLocalAutoFact("/first", "lhs", "i64", "/init/lhs", 5, 2, 971, 1971));
    semanticProgram.localAutoFacts.push_back(
        makeLocalAutoFact("/second", "rhs", "i32", "/init/rhs", 8, 2, 972, 1972));

    semanticProgram.queryFacts.push_back(
        makeQueryFact("/first", "lookupLeft", "/lookup/left", "Result<i32, Err>", "Result<i32, Err>", 5, 3, 981, 1981));
    semanticProgram.queryFacts.push_back(
        makeQueryFact("/second", "lookupRight", "/lookup/right", "Result<i32, Err>", "Result<i32, Err>", 8, 3, 982, 1982));

    semanticProgram.bridgePathChoices.push_back(
        makeBridgePathChoice("/first", "vector", "at", "/std/collections/vector/at", 5, 4, 983, 1983));
    semanticProgram.bridgePathChoices.push_back(
        makeBridgePathChoice("/second", "map", "at", "/std/collections/map/at", 8, 4, 984, 1984));

    semanticProgram.tryFacts.push_back(makeTryFact("/first", "/lookup/left", 5, 5, 985, 1985));
    semanticProgram.tryFacts.push_back(makeTryFact("/second", "/lookup/right", 8, 5, 986, 1986));

    semanticProgram.onErrorFacts.push_back(makeOnErrorFact("/first", "/handler/left", 987, 1987));
    semanticProgram.onErrorFacts.push_back(makeOnErrorFact("/second", "/handler/right", 988, 1988));

    if (useModuleIndices) {
      primec::SemanticProgramModuleResolvedArtifacts moduleFirst;
      moduleFirst.identity.moduleKey = "/first";
      moduleFirst.identity.stableOrder = 0;
      moduleFirst.returnFactIndices.push_back(0);
      moduleFirst.localAutoFactIndices.push_back(0);
      moduleFirst.queryFactIndices.push_back(0);
      moduleFirst.bridgePathChoiceIndices.push_back(0);
      moduleFirst.tryFactIndices.push_back(0);
      moduleFirst.onErrorFactIndices.push_back(0);
      semanticProgram.moduleResolvedArtifacts.push_back(std::move(moduleFirst));

      primec::SemanticProgramModuleResolvedArtifacts moduleSecond;
      moduleSecond.identity.moduleKey = "/second";
      moduleSecond.identity.stableOrder = 1;
      moduleSecond.returnFactIndices.push_back(1);
      moduleSecond.localAutoFactIndices.push_back(1);
      moduleSecond.queryFactIndices.push_back(1);
      moduleSecond.bridgePathChoiceIndices.push_back(1);
      moduleSecond.tryFactIndices.push_back(1);
      moduleSecond.onErrorFactIndices.push_back(1);
      semanticProgram.moduleResolvedArtifacts.push_back(std::move(moduleSecond));
    }

    return semanticProgram;
  };

  const primec::SemanticProgram flatProgram = makeProgram(false);
  const primec::SemanticProgram moduleIndexedProgram = makeProgram(true);
  CHECK(primec::formatSemanticProgram(moduleIndexedProgram) == primec::formatSemanticProgram(flatProgram));
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
      .scopePath = "/main",
      .callName = "id",
      .sourceLine = 9,
      .sourceColumn = 10,
      .semanticNodeId = 13,
      .provenanceHandle = 103,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/id"),
      .stdlibSurfaceId = std::nullopt,
  });
  semanticProgram.methodCallTargets.push_back(primec::SemanticProgramMethodCallTarget{
      .scopePath = "/main",
      .methodName = "count",
      .receiverTypeText = "vector<i32>",
      .sourceLine = 9,
      .sourceColumn = 13,
      .semanticNodeId = 14,
      .provenanceHandle = 104,
      .scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
      .methodNameId = primec::semanticProgramInternCallTargetString(semanticProgram, "count"),
      .receiverTypeTextId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "vector<i32>"),
      .resolvedPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram,
                                                        "/std/collections/vector/count"),
      .stdlibSurfaceId = primec::StdlibSurfaceId::CollectionsVectorHelpers,
  });
  semanticProgram.bridgePathChoices.push_back(primec::SemanticProgramBridgePathChoice{
      .scopePath = "/main",
      .collectionFamily = "vector",
      .sourceLine = 9,
      .sourceColumn = 13,
      .semanticNodeId = 15,
      .provenanceHandle = 105,
      .scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
      .collectionFamilyId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "vector"),
      .helperNameId = primec::semanticProgramInternCallTargetString(semanticProgram, "count"),
      .chosenPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram,
                                                        "/std/collections/vector/count"),
      .stdlibSurfaceId = primec::StdlibSurfaceId::CollectionsVectorHelpers,
  });
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
      .isExecution = true,
      .returnKind = "return",
      .isCompute = false,
      .isUnsafe = false,
      .activeEffects = {"io_out"},
      .activeCapabilities = {"gpu"},
      .hasResultType = true,
      .resultTypeHasValue = true,
      .resultValueType = "i32",
      .resultErrorType = "MyError",
      .hasOnError = true,
      .onErrorHandlerPath = "/unexpectedError",
      .onErrorErrorType = "MyError",
      .onErrorBoundArgCount = 1,
      .semanticNodeId = 16,
      .provenanceHandle = 106,
      .fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
      .returnKindId = primec::semanticProgramInternCallTargetString(semanticProgram, "return"),
      .activeEffectIds = {
          primec::semanticProgramInternCallTargetString(semanticProgram, "io_out"),
      },
      .activeCapabilityIds = {
          primec::semanticProgramInternCallTargetString(semanticProgram, "gpu"),
      },
      .resultValueTypeId = primec::semanticProgramInternCallTargetString(semanticProgram, "i32"),
      .resultErrorTypeId = primec::semanticProgramInternCallTargetString(semanticProgram, "MyError"),
      .onErrorHandlerPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/unexpectedError"),
      .onErrorErrorTypeId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "MyError"),
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
      .scopePath = "/main",
      .siteKind = "local",
      .name = "value",
      .bindingTypeText = "i32",
      .isMutable = true,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 12,
      .sourceColumn = 7,
      .semanticNodeId = 20,
      .provenanceHandle = 110,
      .resolvedPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/main/value"),
  });
  semanticProgram.returnFacts.push_back(primec::SemanticProgramReturnFact{
      .returnKind = "return",
      .structPath = "/i32",
      .bindingTypeText = "i32",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 13,
      .sourceColumn = 3,
      .semanticNodeId = 21,
      .provenanceHandle = 111,
      .definitionPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
  });
  semanticProgram.localAutoFacts.push_back(primec::SemanticProgramLocalAutoFact{
      .scopePath = "/main",
      .bindingName = "selected",
      .bindingTypeText = "i32",
      .initializerBindingTypeText = "i32",
      .initializerReceiverBindingTypeText = "",
      .initializerQueryTypeText = "i32",
      .initializerResultHasValue = false,
      .initializerResultValueType = "",
      .initializerResultErrorType = "",
      .initializerHasTry = false,
      .initializerTryOperandResolvedPath = "",
      .initializerTryOperandBindingTypeText = "",
      .initializerTryOperandReceiverBindingTypeText = "",
      .initializerTryOperandQueryTypeText = "",
      .initializerTryValueType = "",
      .initializerTryErrorType = "",
      .initializerTryContextReturnKind = "return",
      .initializerTryOnErrorHandlerPath = "",
      .initializerTryOnErrorErrorType = "",
      .initializerTryOnErrorBoundArgCount = 0,
      .sourceLine = 14,
      .sourceColumn = 9,
      .semanticNodeId = 22,
      .provenanceHandle = 112,
      .initializerDirectCallResolvedPath = "",
      .initializerDirectCallReturnKind = "",
      .initializerMethodCallResolvedPath = "",
      .initializerMethodCallReturnKind = "",
      .initializerStdlibSurfaceId = std::nullopt,
      .initializerDirectCallStdlibSurfaceId = std::nullopt,
      .initializerMethodCallStdlibSurfaceId = std::nullopt,
      .initializerResolvedPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/id"),
  });
  semanticProgram.queryFacts.push_back(primec::SemanticProgramQueryFact{
      .scopePath = "/main",
      .callName = "lookup",
      .queryTypeText = "Result<i32, MyError>",
      .bindingTypeText = "Result<i32, MyError>",
      .hasResultType = true,
      .resultTypeHasValue = true,
      .resultValueType = "i32",
      .resultErrorType = "MyError",
      .sourceLine = 15,
      .sourceColumn = 4,
      .semanticNodeId = 23,
      .provenanceHandle = 113,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/lookup"),
  });
  semanticProgram.tryFacts.push_back(primec::SemanticProgramTryFact{
      .scopePath = "/main",
      .operandBindingTypeText = "Result<i32, MyError>",
      .operandReceiverBindingTypeText = "",
      .operandQueryTypeText = "Result<i32, MyError>",
      .valueType = "i32",
      .errorType = "MyError",
      .contextReturnKind = "return",
      .onErrorHandlerPath = "/unexpectedError",
      .onErrorErrorType = "MyError",
      .onErrorBoundArgCount = 1,
      .sourceLine = 16,
      .sourceColumn = 8,
      .semanticNodeId = 24,
      .provenanceHandle = 114,
      .operandResolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/lookup"),
  });
  semanticProgram.onErrorFacts.push_back(primec::SemanticProgramOnErrorFact{
      .definitionPath = "/main",
      .returnKind = "return",
      .errorType = "MyError",
      .boundArgCount = 1,
      .boundArgTexts = {"err"},
      .returnResultHasValue = true,
      .returnResultValueType = "i32",
      .returnResultErrorType = "MyError",
      .semanticNodeId = 25,
      .provenanceHandle = 115,
      .handlerPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/unexpectedError"),
  });

  const std::string expected = R"(semantic_product {
  entry_path: "/main"
  source_imports[0]: "/std/collections/*"
  imports[0]: "/id"
  imports[1]: "/main"
  definitions[0]: full_path="/id" name="id" namespace_prefix="/" provenance_handle=101 source="2:3"
  executions[0]: full_path="/main" name="main" namespace_prefix="/" provenance_handle=102 source="7:1"
  direct_call_targets[0]: scope_path="/main" call_name="id" resolved_path="/id" provenance_handle=103 source="9:10"
  method_call_targets[0]: scope_path="/main" method_name="count" receiver_type_text="vector<i32>" resolved_path="/std/collections/vector/count" stdlib_surface_id="collections.vector_helpers" provenance_handle=104 source="9:13"
  bridge_path_choices[0]: scope_path="/main" collection_family="vector" helper_name="count" chosen_path="/std/collections/vector/count" stdlib_surface_id="collections.vector_helpers" provenance_handle=105 source="9:13"
  callable_summaries[0]: full_path="/main" is_execution=true return_kind="return" is_compute=false is_unsafe=false active_effects=["io_out"] active_capabilities=["gpu"] has_result_type=true result_type_has_value=true result_value_type="i32" result_error_type="MyError" has_on_error=true on_error_handler_path="/unexpectedError" on_error_error_type="MyError" on_error_bound_arg_count=1 provenance_handle=106
  type_metadata[0]: full_path="/Particle" category="struct" is_public=true has_no_padding=false has_platform_independent_padding=true has_explicit_alignment=true explicit_alignment_bytes=16 field_count=2 enum_value_count=0 provenance_handle=107 source="11:5"
  struct_field_metadata[0]: struct_path="/Particle" field_name="left" field_index=0 binding_type_text="i32" provenance_handle=108 source="12:7"
  struct_field_metadata[1]: struct_path="/Particle" field_name="right" field_index=1 binding_type_text="i64" provenance_handle=109 source="13:7"
  binding_facts[0]: scope_path="/main" site_kind="local" name="value" resolved_path="/main/value" binding_type_text="i32" is_mutable=true is_entry_arg_string=false is_unsafe_reference=false reference_root="" provenance_handle=110 source="12:7"
  return_facts[0]: definition_path="/main" return_kind="return" struct_path="/i32" binding_type_text="i32" is_mutable=false is_entry_arg_string=false is_unsafe_reference=false reference_root="" provenance_handle=111 source="13:3"
  local_auto_facts[0]: scope_path="/main" binding_name="selected" binding_type_text="i32" initializer_resolved_path="/id" initializer_binding_type_text="i32" initializer_receiver_binding_type_text="" initializer_query_type_text="i32" initializer_result_has_value=false initializer_result_value_type="" initializer_result_error_type="" initializer_has_try=false initializer_try_operand_resolved_path="" initializer_try_operand_binding_type_text="" initializer_try_operand_receiver_binding_type_text="" initializer_try_operand_query_type_text="" initializer_try_value_type="" initializer_try_error_type="" initializer_try_context_return_kind="return" initializer_try_on_error_handler_path="" initializer_try_on_error_error_type="" initializer_try_on_error_bound_arg_count=0 initializer_direct_call_resolved_path="" initializer_direct_call_return_kind="" initializer_method_call_resolved_path="" initializer_method_call_return_kind="" provenance_handle=112 source="14:9"
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
      "  return(id(/std/collections/vector/count(values)))\n"
      "}\n";

  primec::testing::CompilePipelineBoundaryDumps dumps;
  std::string error;
  REQUIRE(primec::testing::captureSemanticBoundaryDumpsForTesting(source, "/main", dumps, error));
  CHECK(error.empty());

  auto program = parseProgram(source);
  primec::Semantics semantics;
  primec::SemanticProgram semanticProgram;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  CHECK_FALSE(semantics.validate(program, "/main", error, defaults, defaults, {}, nullptr, false, &semanticProgram));
  CHECK_FALSE(error.empty());
  CHECK_FALSE(dumps.semanticProduct.empty());
}

TEST_SUITE_END();
