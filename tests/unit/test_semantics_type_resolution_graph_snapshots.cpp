#include <algorithm>
#include <cstdint>
#include <string_view>

#include "primec/IrLowerer.h"
#include "primec/IrSerializer.h"
#include "primec/testing/CompilePipelineDumpHelpers.h"
#include "primec/testing/SemanticsGraphHelpers.h"
#include "primec/testing/SemanticsValidationHelpers.h"

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

template <typename Entry, typename Predicate>
const Entry *findSemanticEntry(const std::vector<const Entry *> &entries, const Predicate &predicate) {
  const auto it = std::find_if(entries.begin(),
                               entries.end(),
                               [&](const Entry *entry) { return entry != nullptr && predicate(*entry); });
  return it == entries.end() ? nullptr : *it;
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
  CHECK(snapshot.resultValueType == "int");
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

  const auto it = std::find_if(facts.begin(), facts.end(), [](const auto &fact) {
    return fact.targetPath == "/id" &&
           fact.explicitArgsText == "i32";
  });
  REQUIRE(it != facts.end());
  CHECK(it->resolvedConcrete);
  CHECK(it->resolvedTypeText.rfind("/id__t", 0) == 0);
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
           fact.callName == "id" &&
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
  CHECK(metrics.hitCount > 0u);
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
  CHECK(metrics.hitCount > 0u);
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
  CHECK(snapshot.explicitTemplateArgInferenceFactHitCount > 0u);
  CHECK(snapshot.implicitTemplateArgInferenceFactHitCount > 0u);
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
      [](const primec::SemanticProgramDirectCallTarget &entry) {
        return entry.scopePath == "/main" &&
               (entry.callName == "id" || entry.callName.rfind("/id__t", 0) == 0) &&
               entry.resolvedPath.rfind("/id__t", 0) == 0;
      });
  REQUIRE(targetEntry != nullptr);
  CHECK(targetEntry->sourceLine > 0);
  CHECK(targetEntry->sourceColumn > 0);
}

TEST_CASE("semantic product publishes resolved method-call targets") {
  const std::string source =
      "[return<i32>]\n"
      "/vector/count([vector<i32>] self) {\n"
      "  return(17i32)\n"
      "}\n"
      "\n"
      "[effects(heap_alloc), return<i32>]\n"
      "main() {\n"
      "  [auto] values{vector<i32>(1i32)}\n"
      "  return(values.count())\n"
      "}\n";

  auto program = parseProgram(source);
  primec::Semantics semantics;
  primec::SemanticProgram semanticProgram;
  std::string error;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  REQUIRE(semantics.validate(program, "/main", error, defaults, defaults, {}, nullptr, false, &semanticProgram));
  CHECK(error.empty());

  const auto *targetEntry = findSemanticEntry(
      primec::semanticProgramMethodCallTargetView(semanticProgram),
      [](const primec::SemanticProgramMethodCallTarget &entry) {
        return entry.scopePath == "/main" &&
               entry.methodName == "count" &&
               entry.resolvedPath == "/vector/count";
      });
  REQUIRE(targetEntry != nullptr);
  CHECK(targetEntry->receiverTypeText.find("vector") != std::string::npos);
  CHECK(targetEntry->sourceLine > 0);
  CHECK(targetEntry->sourceColumn > 0);
}

TEST_CASE("semantic product method-call targets stay separated by receiver type") {
  const std::string source =
      "struct A {\n"
      "  [i32] x\n"
      "}\n"
      "\n"
      "struct B {\n"
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
      [](const primec::SemanticProgramMethodCallTarget *entry) {
        return entry->scopePath == "/main" && entry->methodName == "id" &&
               entry->resolvedPath == "/A/id";
      });
  const auto hasBIdTarget = std::any_of(
      methodTargets.begin(),
      methodTargets.end(),
      [](const primec::SemanticProgramMethodCallTarget *entry) {
        return entry->scopePath == "/main" && entry->methodName == "id" &&
               entry->resolvedPath == "/B/id";
      });
  CHECK(hasAIdTarget);
  CHECK(hasBIdTarget);
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
    if (entry->scopePath == "/main" && entry->resolvedPath == "/id_i32") {
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
      "/vector/count([vector<i32>] self) {\n"
      "  return(17i32)\n"
      "}\n"
      "\n"
      "[effects(heap_alloc), return<i32>]\n"
      "main() {\n"
      "  [auto] a{vector<i32>(1i32)}\n"
      "  [auto] b{vector<i32>(2i32)}\n"
      "  return(plus(a.count(), b.count()))\n"
      "}\n";

  auto program = parseProgram(source);
  primec::Semantics semantics;
  primec::SemanticProgram semanticProgram;
  std::string error;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  REQUIRE(semantics.validate(program, "/main", error, defaults, defaults, {}, nullptr, false, &semanticProgram));
  CHECK(error.empty());

  std::vector<const primec::SemanticProgramMethodCallTarget *> mainTargets;
  for (const auto *entry : primec::semanticProgramMethodCallTargetView(semanticProgram)) {
    if (entry->scopePath == "/main" && entry->methodName == "count" &&
        entry->resolvedPath == "/vector/count") {
      mainTargets.push_back(entry);
    }
  }
  REQUIRE(mainTargets.size() >= 2);
  REQUIRE(mainTargets[0]->resolvedPathId != primec::InvalidSymbolId);
  CHECK(mainTargets[0]->resolvedPathId == mainTargets[1]->resolvedPathId);
  CHECK(primec::semanticProgramResolveCallTargetString(semanticProgram, mainTargets[0]->resolvedPathId) ==
        "/vector/count");
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
  if (aEntry->resolvedPath.empty()) {
    CHECK(aEntry->resolvedPathId == primec::InvalidSymbolId);
  } else {
    REQUIRE(aEntry->resolvedPathId != primec::InvalidSymbolId);
    CHECK(primec::semanticProgramResolveCallTargetString(semanticProgram, aEntry->resolvedPathId) ==
          aEntry->resolvedPath);
  }
  if (bEntry->resolvedPath.empty()) {
    CHECK(bEntry->resolvedPathId == primec::InvalidSymbolId);
  } else {
    REQUIRE(bEntry->resolvedPathId != primec::InvalidSymbolId);
    CHECK(primec::semanticProgramResolveCallTargetString(semanticProgram, bEntry->resolvedPathId) ==
          bEntry->resolvedPath);
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
      [](const primec::SemanticProgramReturnFact &entry) { return entry.definitionPath == "/helper"; });
  const auto *mainEntry = findSemanticEntry(
      primec::semanticProgramReturnFactView(semanticProgram),
      [](const primec::SemanticProgramReturnFact &entry) { return entry.definitionPath == "/main"; });
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
  checkTextId(firstEntry->initializerResolvedPath, firstEntry->initializerResolvedPathId);
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

  const auto *choiceEntry = findSemanticEntry(
      primec::semanticProgramBridgePathChoiceView(semanticProgram),
      [](const primec::SemanticProgramBridgePathChoice &entry) {
        return entry.scopePath == "/main" &&
               entry.collectionFamily == "vector" &&
               entry.helperName == "count" &&
               entry.chosenPath == "/vector/count";
      });
  REQUIRE(choiceEntry != nullptr);
  CHECK(choiceEntry->sourceLine > 0);
  CHECK(choiceEntry->sourceColumn > 0);
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

  const auto *choiceEntry = findSemanticEntry(
      primec::semanticProgramBridgePathChoiceView(semanticProgram),
      [](const primec::SemanticProgramBridgePathChoice &entry) {
        return entry.scopePath == "/main" &&
               entry.collectionFamily == "map" &&
               entry.helperName == "count" &&
               entry.chosenPath == "/std/collections/map/count";
      });
  REQUIRE(choiceEntry != nullptr);
  CHECK(choiceEntry->sourceLine > 0);
  CHECK(choiceEntry->sourceColumn > 0);
}

TEST_CASE("semantic product bridge routing choices carry interned path ids") {
  primec::SemanticProgram semanticProgram;
  auto makeBridgeChoice = [&](uint64_t semanticNodeId,
                              int sourceLine,
                              int sourceColumn) -> primec::SemanticProgramBridgePathChoice {
    primec::SemanticProgramBridgePathChoice entry;
    entry.scopePath = "/main";
    entry.collectionFamily = "map";
    entry.helperName = "count";
    entry.chosenPath = "/std/collections/map/count";
    entry.sourceLine = sourceLine;
    entry.sourceColumn = sourceColumn;
    entry.semanticNodeId = semanticNodeId;
    entry.provenanceHandle = semanticNodeId + 1000;
    entry.scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.scopePath);
    entry.collectionFamilyId =
        primec::semanticProgramInternCallTargetString(semanticProgram, entry.collectionFamily);
    entry.helperNameId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.helperName);
    entry.chosenPathId = primec::semanticProgramInternCallTargetString(semanticProgram, entry.chosenPath);
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
      [](const primec::SemanticProgramCallableSummary &entry) {
        return entry.fullPath == "/main" && !entry.isExecution;
      });
  REQUIRE(summaryEntry != nullptr);
  CHECK(summaryEntry->returnKind == "i32");
  CHECK(summaryEntry->activeEffects ==
        std::vector<std::string>{"asset_read", "io_out"});
  CHECK(summaryEntry->activeCapabilities == std::vector<std::string>{"io_out"});
  CHECK(summaryEntry->hasResultType);
  CHECK(summaryEntry->resultTypeHasValue);
  CHECK(summaryEntry->resultValueType == "int");
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
      summaries, [](const primec::SemanticProgramCallableSummary &entry) { return entry.fullPath == "/helper"; });
  const auto *mainSummary = findSemanticEntry(
      summaries, [](const primec::SemanticProgramCallableSummary &entry) { return entry.fullPath == "/main"; });
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
      [](const primec::SemanticProgramReturnFact &entry) { return entry.definitionPath == "/main"; });
  REQUIRE(mainReturnEntry != nullptr);
  CHECK(mainReturnEntry->returnKind == "i32");
  CHECK(mainReturnEntry->bindingTypeText == "i32");

  const auto *pairReturnEntry = findSemanticEntry(
      primec::semanticProgramReturnFactView(semanticProgram),
      [](const primec::SemanticProgramReturnFact &entry) { return entry.definitionPath == "/makePair"; });
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
  CHECK(localAutoEntry->bindingTypeText == "int");
  CHECK(localAutoEntry->initializerResolvedPath == "/lookup");
  CHECK(localAutoEntry->initializerDirectCallResolvedPath == "/lookup");
  CHECK(!localAutoEntry->initializerDirectCallReturnKind.empty());
  CHECK(localAutoEntry->initializerMethodCallResolvedPath.empty());
  CHECK(localAutoEntry->initializerMethodCallReturnKind.empty());
  CHECK(localAutoEntry->initializerHasTry);
  CHECK(localAutoEntry->initializerTryValueType == "int");
  CHECK(localAutoEntry->initializerTryErrorType == "MyError");

  const auto *queryEntry = findSemanticEntry(
      primec::semanticProgramQueryFactView(semanticProgram),
      [](const primec::SemanticProgramQueryFact &entry) {
        return entry.scopePath == "/main" && entry.resolvedPath == "/lookup";
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
  CHECK(queryEntry->bindingTypeText == "Result<int, MyError>");
  CHECK(queryEntry->hasResultType);
  CHECK(queryEntry->resultTypeHasValue);
  CHECK(queryEntry->resultValueType == "int");
  CHECK(queryEntry->resultErrorType == "MyError");

  const auto *tryEntry = findSemanticEntry(
      primec::semanticProgramTryFactView(semanticProgram),
      [](const primec::SemanticProgramTryFact &entry) {
        return entry.scopePath == "/main" && entry.operandResolvedPath == "/lookup";
      });
  REQUIRE(tryEntry != nullptr);
  CHECK(tryEntry->valueType == "int");
  CHECK(tryEntry->errorType == "MyError");
  CHECK(tryEntry->contextReturnKind == "i32");
  CHECK(tryEntry->onErrorHandlerPath == "/unexpectedError");

  const auto *onErrorEntry = findSemanticEntry(
      primec::semanticProgramOnErrorFactView(semanticProgram),
      [](const primec::SemanticProgramOnErrorFact &entry) { return entry.definitionPath == "/main"; });
  REQUIRE(onErrorEntry != nullptr);
  CHECK(onErrorEntry->returnKind == "i32");
  CHECK(onErrorEntry->handlerPath == "/unexpectedError");
  CHECK(onErrorEntry->errorType == "MyError");
  CHECK(onErrorEntry->boundArgTexts == std::vector<std::string>{});
  CHECK(onErrorEntry->returnResultHasValue);
  CHECK(onErrorEntry->returnResultValueType == "int");
  CHECK(onErrorEntry->returnResultErrorType == "MyError");
}

TEST_CASE("semantic product publishes graph-backed local auto method-call facts") {
  const std::string source = R"(
[return<i32>]
pick([i32] value) {
  return(value)
}

[return<i32>]
/vector/count([vector<i32>] self) {
  return(17i32)
}

[return<i32>]
main() {
  [vector<i32>] values{vector<i32>()}
  [auto] viaDirect{pick(1i32)}
  [auto] viaMethod{values.count()}
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

  const auto *viaMethodEntry = findSemanticEntry(
      primec::semanticProgramLocalAutoFactView(semanticProgram),
      [](const primec::SemanticProgramLocalAutoFact &entry) {
        return entry.scopePath == "/main" && entry.bindingName == "viaMethod";
      });
  REQUIRE(viaMethodEntry != nullptr);
  CHECK(viaMethodEntry->initializerDirectCallResolvedPath.empty());
  CHECK(viaMethodEntry->initializerDirectCallReturnKind.empty());
  CHECK(viaMethodEntry->initializerMethodCallResolvedPath == "/vector/count");
  CHECK(viaMethodEntry->initializerMethodCallReturnKind == "i32");
}

TEST_CASE("semantic product publishes graph-backed collection helper direct-call facts") {
  const std::string source = R"(
[return<i32>]
/vector/count([vector<i32>] self) {
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
  CHECK(localAutoEntry->initializerDirectCallResolvedPath == "/vector/count");
  CHECK(localAutoEntry->initializerDirectCallReturnKind == "i32");
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

  const auto *parameterEntry = findSemanticEntry(primec::semanticProgramBindingFactView(semanticProgram),
      [](const primec::SemanticProgramBindingFact &entry) {
        return entry.scopePath == "/main" && entry.siteKind == "parameter" && entry.name == "argv";
      });
  REQUIRE(parameterEntry != nullptr);
  CHECK(parameterEntry->sourceLine == mainDefinition->parameters[0].sourceLine);
  CHECK(parameterEntry->sourceColumn == mainDefinition->parameters[0].sourceColumn);

  const auto *localEntry = findSemanticEntry(primec::semanticProgramBindingFactView(semanticProgram),
      [](const primec::SemanticProgramBindingFact &entry) {
        return entry.scopePath == "/main" && entry.siteKind == "local" && entry.name == "values";
      });
  REQUIRE(localEntry != nullptr);
  CHECK(localEntry->sourceLine == mainValuesBinding->sourceLine);
  CHECK(localEntry->sourceColumn == mainValuesBinding->sourceColumn);

  const auto *temporaryEntry = findSemanticEntry(primec::semanticProgramBindingFactView(semanticProgram),
      [](const primec::SemanticProgramBindingFact &entry) {
        return entry.scopePath == "/main" && entry.siteKind == "temporary" && entry.name == "pick";
      });
  REQUIRE(temporaryEntry != nullptr);
  CHECK(temporaryEntry->sourceLine == directCallExpr->sourceLine);
  CHECK(temporaryEntry->sourceColumn == directCallExpr->sourceColumn);

  const auto *directCallEntry = findSemanticEntry(primec::semanticProgramDirectCallTargetView(semanticProgram),
      [](const primec::SemanticProgramDirectCallTarget &entry) {
        return entry.scopePath == "/main" && entry.callName == "pick";
      });
  REQUIRE(directCallEntry != nullptr);
  CHECK(directCallEntry->sourceLine == directCallExpr->sourceLine);
  CHECK(directCallEntry->sourceColumn == directCallExpr->sourceColumn);

  const auto *methodCallEntry = findSemanticEntry(primec::semanticProgramMethodCallTargetView(semanticProgram),
      [](const primec::SemanticProgramMethodCallTarget &entry) {
        return entry.scopePath == "/main" && entry.methodName == "count";
      });
  REQUIRE(methodCallEntry != nullptr);
  CHECK(methodCallEntry->sourceLine == methodCallExpr->sourceLine);
  CHECK(methodCallEntry->sourceColumn == methodCallExpr->sourceColumn);

  const auto *bridgeEntry = findSemanticEntry(primec::semanticProgramBridgePathChoiceView(semanticProgram),
      [](const primec::SemanticProgramBridgePathChoice &entry) {
        return entry.scopePath == "/main" && entry.helperName == "count";
      });
  REQUIRE(bridgeEntry != nullptr);
  CHECK(bridgeEntry->sourceLine == bridgeCallExpr->sourceLine);
  CHECK(bridgeEntry->sourceColumn == bridgeCallExpr->sourceColumn);

  const auto *returnEntry = findSemanticEntry(primec::semanticProgramReturnFactView(semanticProgram),
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
      [](const primec::SemanticProgramQueryFact &entry) {
        return entry.scopePath == "/main" && entry.resolvedPath == "/lookup";
      });
  const auto *secondQuery = findSemanticEntry(primec::semanticProgramQueryFactView(second),
      [](const primec::SemanticProgramQueryFact &entry) {
        return entry.scopePath == "/main" && entry.resolvedPath == "/lookup";
      });
  REQUIRE(firstQuery != nullptr);
  REQUIRE(secondQuery != nullptr);
  CHECK(firstQuery->semanticNodeId != 0);
  CHECK(firstQuery->semanticNodeId == secondQuery->semanticNodeId);
  CHECK(firstQuery->provenanceHandle != 0);
  CHECK(firstQuery->provenanceHandle == secondQuery->provenanceHandle);

  const auto *firstTry = findSemanticEntry(primec::semanticProgramTryFactView(first),
      [](const primec::SemanticProgramTryFact &entry) {
        return entry.scopePath == "/main" && entry.operandResolvedPath == "/lookup";
      });
  const auto *secondTry = findSemanticEntry(primec::semanticProgramTryFactView(second),
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
      [](const primec::SemanticProgramReturnFact &entry) { return entry.definitionPath == "/main"; });
  const auto *secondReturn = findSemanticEntry(primec::semanticProgramReturnFactView(second),
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
  for (const auto *entry : primec::semanticProgramBindingFactView(semanticProgram)) {
    if (entry == nullptr) {
      continue;
    }
    if (entry->scopePath == "/main" && entry->siteKind == "local") {
      localBindingOrder.push_back(entry->name);
    }
  }
  CHECK(localBindingOrder == std::vector<std::string>{"zeta", "alpha"});

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
  REQUIRE(semantics.validate(semanticAst, "/main", error, defaults, defaults, {}, nullptr, false, &semanticProgram));
  CHECK(error.empty());

  const auto *directCallEntry =
      findSemanticEntry(primec::semanticProgramDirectCallTargetView(semanticProgram),
                        [](const primec::SemanticProgramDirectCallTarget &entry) {
                          return entry.scopePath == "/main" && entry.callName == "pick";
                        });
  const auto *methodCallEntry =
      findSemanticEntry(primec::semanticProgramMethodCallTargetView(semanticProgram),
                        [](const primec::SemanticProgramMethodCallTarget &entry) {
                          return entry.scopePath == "/main" && entry.methodName == "count";
                        });
  const auto *bridgeEntry =
      findSemanticEntry(primec::semanticProgramBridgePathChoiceView(semanticProgram),
                        [](const primec::SemanticProgramBridgePathChoice &entry) {
                          return entry.scopePath == "/main" && entry.helperName == "count";
                        });
  const auto *returnEntry =
      findSemanticEntry(primec::semanticProgramReturnFactView(semanticProgram),
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
                        [](const primec::SemanticProgramBridgePathChoice &entry) {
                          return entry.scopePath == "/main" && entry.helperName == "count";
                        });
  const auto *semanticReturnEntry =
      findSemanticEntry(primec::semanticProgramReturnFactView(semanticProgram),
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

TEST_CASE("semantic product formatter resolves module direct-call indices deterministically") {
  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      "/z",
      "last",
      "/last",
      20,
      2,
      300,
      900,
  });
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      "/a",
      "first",
      "/first",
      10,
      1,
      200,
      800,
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
      "direct_call_targets[0]: scope_path=\"/a\" call_name=\"first\" resolved_path=\"/first\"";
  const std::string secondEntry =
      "direct_call_targets[1]: scope_path=\"/z\" call_name=\"last\" resolved_path=\"/last\"";
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
      "/z",
      "scale",
      "matrix<f32>",
      "/std/math/matrix/scale",
      22,
      6,
      330,
      930,
  });
  semanticProgram.methodCallTargets.push_back(primec::SemanticProgramMethodCallTarget{
      "/a",
      "length",
      "vector<f32>",
      "/std/math/vector/length",
      12,
      3,
      230,
      830,
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
      "method_call_targets[0]: scope_path=\"/a\" method_name=\"length\" receiver_type_text=\"vector<f32>\" resolved_path=\"/std/math/vector/length\"";
  const std::string secondEntry =
      "method_call_targets[1]: scope_path=\"/z\" method_name=\"scale\" receiver_type_text=\"matrix<f32>\" resolved_path=\"/std/math/matrix/scale\"";
  const std::size_t firstPos = dump.find(firstEntry);
  const std::size_t secondPos = dump.find(secondEntry);
  CHECK(firstPos != std::string::npos);
  CHECK(secondPos != std::string::npos);
  CHECK(firstPos < secondPos);
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
      "",
      "",
      "",
      "",
  });
  semanticProgram.queryFacts.push_back(primec::SemanticProgramQueryFact{
      "/main",
      "lookup",
      "/lookup",
      "Result<i32, MyError>",
      "Result<i32, MyError>",
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
