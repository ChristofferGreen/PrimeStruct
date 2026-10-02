// Collection helper target parity (TODO-5349): every row of the helper matrix is
// compiled and its three views are compared - the ast-semantic dump spelling,
// the call targets the semantic product publishes, and what lowering/the VM do
// with the call. The published targets are pinned in a generated header and
// rendered into docs/CollectionHelperTargets.md.
//
// Regenerate both after an intentional change:
//   PRIMESTRUCT_COLLECTION_PARITY_REGEN=<repo root>
//     build-release/PrimeStruct_collection_parity_tests --test-case="*regenerates*"
#include "collection_helper_published_targets.h"
#include "collection_helper_rows.h"

#include "primec/runtime/Vm.h"
#include "primec/semantic_product/DirectCallFacts.h"
#include "primec/semantic_product/MethodCallFacts.h"
#include "primec/testing/CompilePipelineDumpHelpers.h"

#include "third_party/doctest.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

TEST_SUITE_BEGIN("primestruct.collection_parity");

namespace {

struct Observed {
  CollectionRowOutcome outcome = CollectionRowOutcome::Ok;
  int exitCode = 0;
  std::string message;
  bool semanticOk = false;
  std::vector<std::string> targets;  // published collection helper targets for /main, normalized
  std::string mainAst;               // ast-semantic body of /main
};

std::string normalizeTarget(const std::string &path) {
  const size_t slash = path.rfind('/');
  const std::string head = slash == std::string::npos ? std::string() : path.substr(0, slash + 1);
  std::string last = slash == std::string::npos ? path : path.substr(slash + 1);
  const size_t specialization = last.find("__");
  if (specialization != std::string::npos) {
    last.erase(specialization);
  }
  return head + last;
}

bool isCollectionTarget(const std::string &path) {
  static const char *prefixes[] = {"/std/collections/", "/array/", "/string/", "/vector/", "/soa/", "/map/"};
  for (const char *prefix : prefixes) {
    if (path.rfind(prefix, 0) == 0) {
      return true;
    }
  }
  return path == "/array" || path == "/string";
}

std::string programFor(const CollectionRow &row) {
  return std::string(row.imports) + row.preamble + "[effects(heap_alloc), return<int>]\nmain() {\n  " + row.setup +
         "\n  return(" + row.expr + ")\n}\n";
}

std::vector<std::string> publishedTargets(const primec::SemanticProgram &program) {
  struct Entry {
    int line;
    int column;
    int kind;
    std::string path;
  };
  std::vector<Entry> entries;
  for (const auto *target : primec::semanticProgramDirectCallTargetView(program)) {
    if (target == nullptr || target->scopePath != "/main") {
      continue;
    }
    const std::string path = normalizeTarget(std::string(primec::semanticProgramDirectCallTargetResolvedPath(program, *target)));
    if (isCollectionTarget(path)) {
      entries.push_back({target->sourceLine, target->sourceColumn, 0, path});
    }
  }
  for (const auto *target : primec::semanticProgramMethodCallTargetView(program)) {
    if (target == nullptr || target->scopePath != "/main") {
      continue;
    }
    const std::string path = normalizeTarget(std::string(primec::semanticProgramMethodCallTargetResolvedPath(program, *target)));
    if (isCollectionTarget(path)) {
      entries.push_back({target->sourceLine, target->sourceColumn, 1, path});
    }
  }
  std::sort(entries.begin(), entries.end(), [](const Entry &a, const Entry &b) {
    return std::tie(a.line, a.column, a.kind, a.path) < std::tie(b.line, b.column, b.kind, b.path);
  });
  std::vector<std::string> paths;
  for (const auto &entry : entries) {
    paths.push_back(entry.path);
  }
  return paths;
}

std::string extractMainAst(const std::string &ast) {
  const size_t start = ast.find("/main()");
  if (start == std::string::npos) {
    return {};
  }
  const size_t end = ast.find("\n  }\n", start);
  return ast.substr(start, end == std::string::npos ? std::string::npos : end - start);
}

Observed observeUncached(const CollectionRow &row) {
  Observed observed;
  const std::string source = programFor(row);
  primec::testing::detail::PreparedCompilePipelineIrState prepared;
  std::string error;
  const bool prepareOk = primec::testing::prepareCompilePipelineIr(source, "/main", "vm", prepared, error);
  if (prepared.output.hasSemanticProgram) {
    observed.targets = publishedTargets(prepared.output.semanticProgram);
  }
  observed.semanticOk = prepared.errorStage != primec::CompilePipelineErrorStage::Semantic &&
                        prepared.errorStage != primec::CompilePipelineErrorStage::Import &&
                        prepared.errorStage != primec::CompilePipelineErrorStage::Parse;
  if (!prepareOk) {
    observed.outcome = observed.semanticOk ? CollectionRowOutcome::LoweringError : CollectionRowOutcome::SemanticError;
    observed.message = error;
    return observed;
  }
  uint64_t result = 0;
  std::string vmError;
  primec::Vm vm;
  if (!vm.execute(prepared.ir, result, vmError, std::vector<std::string_view>{"parity"})) {
    observed.outcome = CollectionRowOutcome::LoweringError;
    observed.message = "VM error: " + vmError;
    return observed;
  }
  observed.exitCode = static_cast<int>(static_cast<int32_t>(result));
  // One extra pipeline run for the ast-semantic spelling (the semantic product is
  // skipped for that dump stage, so it cannot come from the run above).
  const std::filesystem::path sourcePath = primec::testing::detail::makeCompilePipelineDumpSourcePath();
  {
    std::ofstream file(sourcePath);
    file << source;
  }
  std::string ast;
  if (primec::testing::detail::captureCompilePipelineDumpStageFromPath(
          sourcePath, "/main", "ast-semantic",
          primec::testing::detail::CompilePipelineSemanticProductIntent::SkipForNonConsumingPath, ast, error)) {
    observed.mainAst = extractMainAst(ast);
  }
  std::error_code removeError;
  std::filesystem::remove(sourcePath, removeError);
  return observed;
}

// Each row costs two pipeline runs (stdlib import dominates), so observations are
// computed once per process and shared by the checks below. The suite is sharded
// by family in CTest; a family shard takes roughly 20-40 seconds in release.
const Observed &observe(const CollectionRow &row) {
  static std::map<const CollectionRow *, Observed> cache;
  const auto it = cache.find(&row);
  if (it != cache.end()) {
    return it->second;
  }
  return cache.emplace(&row, observeUncached(row)).first->second;
}

const char *outcomeName(CollectionRowOutcome outcome) {
  switch (outcome) {
  case CollectionRowOutcome::Ok:
    return "ok";
  case CollectionRowOutcome::SemanticError:
    return "semantic error";
  case CollectionRowOutcome::LoweringError:
    return "lowering error";
  }
  return "?";
}

std::string renderDoc() {
  std::ostringstream out;
  out << "# Collection helper targets\n\n"
         "Generated by `tests/unit/collection_parity/test_collection_helper_parity.cpp` (do not edit by\n"
         "hand; see that file for the regeneration command). Each row is one helper call shape in a tiny\n"
         "program; the table records what the compiler does with it today: the outcome, and the\n"
         "collection call targets the semantic product publishes for `/main` (specialization suffixes\n"
         "stripped). Rows marked with a TODO id are known defects, pinned so a fix is a deliberate\n"
         "change. `Reference<...>` rows (`(Reference)` forms) cover borrowed receivers and the `_ref` helpers.\n\n"
         "| family | helper | form | outcome | published targets | note |\n"
         "| --- | --- | --- | --- | --- | --- |\n";
  const auto &rows = collectionRows();
  const auto &pinned = collectionRowPublishedTargets();
  for (size_t i = 0; i < rows.size(); ++i) {
    const CollectionRow &row = rows[i];
    out << "| " << row.family << " | `" << row.helper << "` | " << row.form << " | " << outcomeName(row.outcome);
    if (row.outcome == CollectionRowOutcome::Ok) {
      out << " (" << row.expectedExit << ")";
    }
    out << " | ";
    if (i < pinned.size()) {
      for (size_t t = 0; t < pinned[i].size(); ++t) {
        out << (t == 0 ? "" : "<br>") << "`" << pinned[i][t] << "`";
      }
    }
    out << " | ";
    if (row.outcome != CollectionRowOutcome::Ok) {
      out << row.messageContains;
    }
    if (row.knownIssue[0] != '\0') {
      out << (row.outcome != CollectionRowOutcome::Ok ? " " : "") << "**" << row.knownIssue << "**";
    }
    out << " |\n";
  }
  return out.str();
}

std::string readTextFile(const std::string &path) {
  std::ifstream in(path);
  return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}
} // namespace

TEST_CASE("collection parity regenerates pins and doc when asked") {
  const char *root = std::getenv("PRIMESTRUCT_COLLECTION_PARITY_REGEN");
  if (root == nullptr) {
    return;
  }
  const auto &rows = collectionRows();
  std::ostringstream header;
  header << "#pragma once\n\n#include <string>\n#include <vector>\n\n"
            "// Generated by test_collection_helper_parity.cpp (PRIMESTRUCT_COLLECTION_PARITY_REGEN).\n"
            "// Index i matches collectionRows()[i]: published collection call targets for /main.\n"
            "inline const std::vector<std::vector<std::string>> &collectionRowPublishedTargets() {\n"
            "  static const std::vector<std::vector<std::string>> targets = {\n";
  std::vector<std::vector<std::string>> observedTargets;
  for (const CollectionRow &row : rows) {
    const Observed observed = observe(row);
    observedTargets.push_back(observed.targets);
    header << "      {";
    for (size_t i = 0; i < observed.targets.size(); ++i) {
      header << (i == 0 ? "" : ", ") << '"' << observed.targets[i] << '"';
    }
    header << "},\n";
  }
  header << "  };\n  return targets;\n}\n";
  std::ofstream(std::string(root) + "/tests/unit/collection_parity/collection_helper_published_targets.h")
      << header.str();
  // The doc renders from the (just written) pins, so render with the fresh values.
  std::ostringstream doc;
  doc << "# Collection helper targets\n\n"
         "Generated by `tests/unit/collection_parity/test_collection_helper_parity.cpp` (do not edit by\n"
         "hand; see that file for the regeneration command). Each row is one helper call shape in a tiny\n"
         "program; the table records what the compiler does with it today: the outcome, and the\n"
         "collection call targets the semantic product publishes for `/main` (specialization suffixes\n"
         "stripped). Rows marked with a TODO id are known defects, pinned so a fix is a deliberate\n"
         "change. `Reference<...>` rows (`(Reference)` forms) cover borrowed receivers and the `_ref` helpers.\n\n"
         "| family | helper | form | outcome | published targets | note |\n"
         "| --- | --- | --- | --- | --- | --- |\n";
  for (size_t i = 0; i < rows.size(); ++i) {
    const CollectionRow &row = rows[i];
    doc << "| " << row.family << " | `" << row.helper << "` | " << row.form << " | " << outcomeName(row.outcome);
    if (row.outcome == CollectionRowOutcome::Ok) {
      doc << " (" << row.expectedExit << ")";
    }
    doc << " | ";
    for (size_t t = 0; t < observedTargets[i].size(); ++t) {
      doc << (t == 0 ? "" : "<br>") << "`" << observedTargets[i][t] << "`";
    }
    doc << " | ";
    if (row.outcome != CollectionRowOutcome::Ok) {
      doc << row.messageContains;
    }
    if (row.knownIssue[0] != '\0') {
      doc << (row.outcome != CollectionRowOutcome::Ok ? " " : "") << "**" << row.knownIssue << "**";
    }
    doc << " |\n";
  }
  std::ofstream(std::string(root) + "/docs/CollectionHelperTargets.md") << doc.str();
}

void checkFamily(const std::string &family) {
  const std::regex explicitPath(R"((/std/collections/[a-z_]+/[A-Za-z_]+)(__[A-Za-z0-9_]+)?\()");
  const auto &rows = collectionRows();
  const auto &pinned = collectionRowPublishedTargets();
  REQUIRE_MESSAGE(pinned.size() == rows.size(), "regenerate the pins (see the file header)");
  size_t checked = 0;
  for (size_t i = 0; i < rows.size(); ++i) {
    const CollectionRow &row = rows[i];
    if (family != row.family) {
      continue;
    }
    ++checked;
    CAPTURE(row.family);
    CAPTURE(row.helper);
    CAPTURE(row.form);
    const Observed &observed = observe(row);
    // 1. outcome / VM result is as pinned
    CHECK_MESSAGE(observed.outcome == row.outcome,
                  "expected " << outcomeName(row.outcome) << " but saw " << outcomeName(observed.outcome) << ": "
                              << observed.message);
    if (row.outcome == CollectionRowOutcome::Ok && observed.outcome == CollectionRowOutcome::Ok) {
      CHECK(observed.exitCode == row.expectedExit);
    } else if (row.outcome != CollectionRowOutcome::Ok) {
      CHECK_MESSAGE(observed.message.find(row.messageContains) != std::string::npos, observed.message);
    }
    // 2. the semantic product publishes the pinned targets
    CHECK(observed.targets == pinned[i]);
    // 3. explicit helper paths in the dump agree with the published targets
    const std::set<std::string> published(observed.targets.begin(), observed.targets.end());
    for (auto it = std::sregex_iterator(observed.mainAst.begin(), observed.mainAst.end(), explicitPath);
         it != std::sregex_iterator(); ++it) {
      const std::string spelled = (*it)[1].str();
      CHECK_MESSAGE(published.count(spelled) > 0,
                    "dump spells " << spelled << " but the semantic product does not publish it");
    }
  }
  CHECK_MESSAGE(checked > 0, "no rows for family " << family);
}

TEST_CASE("collection parity vector rows") { checkFamily("vector"); }
TEST_CASE("collection parity array rows") { checkFamily("array"); }
TEST_CASE("collection parity string rows") { checkFamily("string"); }
TEST_CASE("collection parity map rows") { checkFamily("map"); }
TEST_CASE("collection parity soa rows") { checkFamily("soa"); }

TEST_CASE("every helper family and call form is represented in the matrix") {
  std::set<std::string> families;
  std::set<std::string> helpers;
  for (const CollectionRow &row : collectionRows()) {
    families.insert(row.family);
    helpers.insert(row.helper);
  }
  for (const char *family : {"vector", "array", "string", "map", "soa"}) {
    CHECK_MESSAGE(families.count(family) > 0, family);
  }
  for (const char *helper : {"count", "capacity", "at", "at_unsafe", "push", "pop", "reserve", "clear", "remove_at",
                             "remove_swap", "contains", "insert", "get", "ref", "to_aos"}) {
    CHECK_MESSAGE(helpers.count(helper) > 0, helper);
  }
}

#ifdef PRIMESTRUCT_SOURCE_DIR
TEST_CASE("docs/CollectionHelperTargets.md is up to date with the matrix") {
  CHECK_MESSAGE(readTextFile(std::string(PRIMESTRUCT_SOURCE_DIR) + "/docs/CollectionHelperTargets.md") == renderDoc(),
                "regenerate docs/CollectionHelperTargets.md (see the file header)");
}
#endif

TEST_SUITE_END();
