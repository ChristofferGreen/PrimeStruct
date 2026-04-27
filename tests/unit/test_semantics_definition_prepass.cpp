#include "third_party/doctest.h"

#include "primec/SemanticValidationPlan.h"
#include "primec/SemanticsDefinitionPrepass.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.definition_prepass");

namespace {

std::vector<std::string> declarationPaths(
    const primec::semantics::DefinitionPrepassSnapshot &snapshot) {
  std::vector<std::string> paths;
  paths.reserve(snapshot.declarationsInStableOrder.size());
  for (const auto &entry : snapshot.declarationsInStableOrder) {
    paths.emplace_back(entry.fullPath);
  }
  return paths;
}

std::vector<primec::SymbolId> declarationSymbols(
    const primec::semantics::DefinitionPrepassSnapshot &snapshot) {
  std::vector<primec::SymbolId> ids;
  ids.reserve(snapshot.declarationsInStableOrder.size());
  for (const auto &entry : snapshot.declarationsInStableOrder) {
    ids.push_back(entry.pathSymbolId);
  }
  return ids;
}

std::vector<std::string> diagnosticMessages(const primec::SemanticDiagnosticInfo &diagnostics) {
  std::vector<std::string> messages;
  messages.reserve(diagnostics.records.size());
  for (const auto &record : diagnostics.records) {
    messages.push_back(record.message);
  }
  return messages;
}

std::vector<std::string> manifestPassNames() {
  std::vector<std::string> names;
  const auto &manifest = primec::semantics::semanticValidationPassManifest();
  names.reserve(manifest.size());
  for (const auto &entry : manifest) {
    names.emplace_back(entry.name);
  }
  return names;
}

const primec::semantics::SemanticValidationPassManifestEntry *findManifestPass(
    std::string_view name) {
  const auto &manifest = primec::semantics::semanticValidationPassManifest();
  const auto it = std::find_if(
      manifest.begin(),
      manifest.end(),
      [name](const primec::semantics::SemanticValidationPassManifestEntry &entry) {
        return entry.name == name;
      });
  return it == manifest.end() ? nullptr : &*it;
}

} // namespace

TEST_CASE("semantic validation pass manifest pins ordered pipeline phases") {
  const std::vector<std::string> expectedNames = {
      "semantic-transform-rules",
      "experimental-gfx-constructors",
      "reflection-generated-helpers",
      "builtin-soa-conversion-methods",
      "builtin-soa-to-aos-calls",
      "builtin-soa-helper-return-metadata",
      "builtin-soa-access-calls",
      "builtin-soa-count-calls",
      "builtin-soa-mutator-calls",
      "experimental-soa-inline-borrow-methods",
      "experimental-soa-same-path-helper-methods",
      "experimental-soa-to-aos-methods",
      "experimental-soa-field-view-indexes",
      "experimental-soa-field-view-helpers",
      "experimental-soa-field-view-carrier-indexes",
      "experimental-soa-field-view-assign-targets",
      "borrowed-experimental-map-methods",
      "experimental-map-value-methods",
      "builtin-map-insert-methods",
      "template-monomorphization",
      "reflection-metadata-queries",
      "convert-constructors",
      "validator-passes",
      "omitted-struct-initializers",
      "semantic-node-id-assignment",
      "semantic-product-publication",
  };
  CHECK(manifestPassNames() == expectedNames);

  const auto *first = findManifestPass("semantic-transform-rules");
  REQUIRE(first != nullptr);
  CHECK(first->inputOwnership ==
        primec::semantics::SemanticValidationPassOwnership::AstSyntax);
  CHECK(first->outputOwnership ==
        primec::semantics::SemanticValidationPassOwnership::SemanticCanonicalAst);
  CHECK(first->action ==
        primec::semantics::SemanticValidationPassAction::MutatesAst);
}

TEST_CASE("semantic validation pass manifest classifies compatibility and facts") {
  const auto *compat = findManifestPass("experimental-map-value-methods");
  REQUIRE(compat != nullptr);
  CHECK(compat->kind ==
        primec::semantics::SemanticValidationPassKind::CompatibilityRewrite);
  CHECK(compat->compatibilityRewrite);
  CHECK(compat->action ==
        primec::semantics::SemanticValidationPassAction::MutatesAst);

  const auto *metadata = findManifestPass("builtin-soa-helper-return-metadata");
  REQUIRE(metadata != nullptr);
  CHECK_FALSE(metadata->compatibilityRewrite);
  CHECK(metadata->action ==
        primec::semantics::SemanticValidationPassAction::ValidatesOnly);

  const auto *validator = findManifestPass("validator-passes");
  REQUIRE(validator != nullptr);
  CHECK(validator->kind ==
        primec::semantics::SemanticValidationPassKind::Validation);
  CHECK(validator->outputOwnership ==
        primec::semantics::SemanticValidationPassOwnership::SemanticProductFacts);
  CHECK(validator->action ==
        primec::semantics::SemanticValidationPassAction::PublishesFacts);

  const auto *publication = findManifestPass("semantic-product-publication");
  REQUIRE(publication != nullptr);
  CHECK(publication->kind ==
        primec::semantics::SemanticValidationPassKind::Publication);
  CHECK(publication->outputOwnership ==
        primec::semantics::SemanticValidationPassOwnership::SemanticProductFacts);
}

TEST_CASE("definition prepass indexes declarations and records duplicates deterministically") {
  const std::string source = R"(
[return<void>]
alpha() {
}

[return<void>]
beta() {
}

[return<void>]
alpha() {
}
)";

  const primec::Program program = parseProgram(source);
  const primec::semantics::DefinitionPrepassSnapshot snapshot =
      primec::semantics::buildDefinitionPrepassSnapshot(program);

  REQUIRE(snapshot.declarationsInStableOrder.size() == 3);
  CHECK(declarationPaths(snapshot) ==
        std::vector<std::string>{"/alpha", "/beta", "/alpha"});
  CHECK(declarationSymbols(snapshot) ==
        std::vector<primec::SymbolId>{1, 2, 1});
  CHECK(snapshot.pathSymbols.resolve(1) == "/alpha");
  CHECK(snapshot.pathSymbols.resolve(2) == "/beta");

  REQUIRE(snapshot.firstDeclarationIndexByPath.size() == 2);
  const auto alphaIt =
      snapshot.firstDeclarationIndexByPath.find(std::string_view("/alpha"));
  REQUIRE(alphaIt != snapshot.firstDeclarationIndexByPath.end());
  CHECK(alphaIt->second == 0);
  const auto betaIt =
      snapshot.firstDeclarationIndexByPath.find(std::string_view("/beta"));
  REQUIRE(betaIt != snapshot.firstDeclarationIndexByPath.end());
  CHECK(betaIt->second == 1);

  REQUIRE(snapshot.duplicateDeclarationPaths.size() == 1);
  CHECK(snapshot.duplicateDeclarationPaths[0] == "/alpha");
}

TEST_CASE("definition prepass is read-only over the program and repeat-run stable") {
  const std::string source = R"(
[return<i32>]
sum([i32] left, [i32] right) {
  return(add_i32(left, right))
}

[return<i32>]
main() {
  return(sum(1i32, 2i32))
}
)";

  primec::Program program = parseProgram(source);
  const std::vector<std::string> baselinePaths = {
      program.definitions[0].fullPath,
      program.definitions[1].fullPath,
  };
  const std::size_t baselineDefinitionCount = program.definitions.size();

  const primec::semantics::DefinitionPrepassSnapshot firstSnapshot =
      primec::semantics::buildDefinitionPrepassSnapshot(program);
  const primec::semantics::DefinitionPrepassSnapshot secondSnapshot =
      primec::semantics::buildDefinitionPrepassSnapshot(program);

  REQUIRE(program.definitions.size() == baselineDefinitionCount);
  REQUIRE(program.definitions.size() == baselinePaths.size());
  CHECK(program.definitions[0].fullPath == baselinePaths[0]);
  CHECK(program.definitions[1].fullPath == baselinePaths[1]);

  CHECK(declarationPaths(firstSnapshot) == declarationPaths(secondSnapshot));
  CHECK(declarationSymbols(firstSnapshot) == declarationSymbols(secondSnapshot));
  CHECK(firstSnapshot.duplicateDeclarationPaths ==
        secondSnapshot.duplicateDeclarationPaths);
  CHECK(firstSnapshot.firstDeclarationIndexByPath.size() ==
        secondSnapshot.firstDeclarationIndexByPath.size());
}

TEST_CASE("definition validation diagnostics keep source-order parity via prepass iteration") {
  const std::string source = R"(
[return<void>]
alpha() {
  unknown_alpha()
}

[return<void>]
beta() {
  unknown_beta()
}
)";

  std::string firstError;
  primec::SemanticDiagnosticInfo firstDiagnostics;
  const bool firstOk =
      validateProgramCollectingDiagnostics(source, "/alpha", firstError, firstDiagnostics);
  CHECK_FALSE(firstOk);
  CHECK_FALSE(firstError.empty());

  std::string secondError;
  primec::SemanticDiagnosticInfo secondDiagnostics;
  const bool secondOk =
      validateProgramCollectingDiagnostics(source, "/alpha", secondError, secondDiagnostics);
  CHECK_FALSE(secondOk);
  CHECK_FALSE(secondError.empty());

  const std::vector<std::string> firstMessages = diagnosticMessages(firstDiagnostics);
  const std::vector<std::string> secondMessages = diagnosticMessages(secondDiagnostics);
  REQUIRE(firstMessages.size() >= 2);
  REQUIRE(secondMessages.size() >= 2);
  CHECK(firstMessages == secondMessages);
  CHECK(firstMessages[0].find("unknown call target: unknown_alpha") != std::string::npos);
  CHECK(firstMessages[1].find("unknown call target: unknown_beta") != std::string::npos);
}

TEST_CASE("semantic validation plan captures prepass inputs deterministically") {
  primec::Program program;
  program.sourceImports = {"/std/collections/*"};
  program.imports = {"/std/math/*", "/std/collections/*"};

  primec::Definition helper;
  helper.fullPath = "/helper";
  program.definitions.push_back(helper);

  primec::Definition main;
  main.fullPath = "/main";
  program.definitions.push_back(main);

  primec::Execution exec;
  exec.fullPath = "/run";
  program.executions.push_back(exec);

  const primec::semantics::SemanticValidationPlan plan =
      primec::semantics::buildSemanticValidationPlan(program, "/main");

  CHECK(declarationPaths(plan.definitionPrepass) ==
        std::vector<std::string>{"/helper", "/main"});
  CHECK(plan.entry.fullPath == "/main");
  CHECK(plan.entry.declared);
  CHECK(plan.entry.stableOrderOffset == 1);
  CHECK(plan.entry.stableIndex == 1);

  CHECK(plan.imports.hasSourceImports);
  CHECK(plan.imports.sourceImportPaths ==
        std::vector<std::string>{"/std/collections/*"});
  CHECK(plan.imports.programImportPaths ==
        std::vector<std::string>{"/std/math/*", "/std/collections/*"});
  CHECK(plan.imports.directImportPaths ==
        std::vector<std::string>{"/std/collections/*"});
  CHECK(plan.imports.transitiveImportPaths ==
        std::vector<std::string>{"/std/math/*"});

  CHECK(plan.builtinCapabilityTables.effectsTransformName == "effects");
  CHECK(plan.builtinCapabilityTables.capabilitiesTransformName == "capabilities");
  CHECK(plan.builtinCapabilityTables.allowIdentifierNamedEffects);
  CHECK(plan.graphLocalAutoInputs.entryPath == "/main");
  CHECK(plan.graphLocalAutoInputs.definitionCount == 2);
  CHECK(plan.graphLocalAutoInputs.executionCount == 1);
  REQUIRE(plan.executionSlice.executionsInStableOrder.size() == 1);
  CHECK(plan.executionSlice.executionsInStableOrder[0].fullPath == "/run");
  CHECK(plan.executionSlice.executionsInStableOrder[0].stableIndex == 0);
}

TEST_SUITE_END();
