#include "primec/SemanticValidationPlan.h"

#include <string_view>
#include <unordered_set>

namespace primec::semantics {

namespace {

std::vector<std::string> copyImportPaths(const std::vector<std::string> &paths) {
  return paths;
}

} // namespace

const std::vector<SemanticValidationPassManifestEntry> &semanticValidationPassManifest() {
  static const std::vector<SemanticValidationPassManifestEntry> Manifest = {
      {"semantic-transform-rules",
       SemanticValidationPassKind::CoreCanonicalization,
       SemanticValidationPassOwnership::AstSyntax,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassAction::MutatesAst,
       false,
       "strip text-only transforms, apply semantic transform hooks, and expand semantic transform rules"},
      {"experimental-gfx-constructors",
       SemanticValidationPassKind::CompatibilityRewrite,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassAction::MutatesAst,
       true,
       "rewrite legacy gfx constructor spellings before template monomorphization"},
      {"reflection-generated-helpers",
       SemanticValidationPassKind::CoreCanonicalization,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassAction::MutatesAst,
       false,
       "materialize generated reflection helper definitions"},
      {"builtin-soa-conversion-methods",
       SemanticValidationPassKind::CompatibilityRewrite,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassAction::MutatesAst,
       true,
       "rewrite builtin soa_vector conversion method compatibility forms"},
      {"builtin-soa-to-aos-calls",
       SemanticValidationPassKind::CompatibilityRewrite,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassAction::MutatesAst,
       true,
       "rewrite builtin soa_vector to_aos compatibility calls"},
      {"builtin-soa-helper-return-metadata",
       SemanticValidationPassKind::CoreCanonicalization,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassAction::ValidatesOnly,
       false,
       "validate builtin soa_vector helper return metadata before helper rewrites"},
      {"builtin-soa-access-calls",
       SemanticValidationPassKind::CompatibilityRewrite,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassAction::MutatesAst,
       true,
       "rewrite builtin soa_vector access helper compatibility calls"},
      {"builtin-soa-count-calls",
       SemanticValidationPassKind::CompatibilityRewrite,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassAction::MutatesAst,
       true,
       "rewrite builtin soa_vector count helper compatibility calls"},
      {"builtin-soa-mutator-calls",
       SemanticValidationPassKind::CompatibilityRewrite,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassAction::MutatesAst,
       true,
       "rewrite builtin soa_vector mutator helper compatibility calls"},
      {"experimental-soa-inline-borrow-methods",
       SemanticValidationPassKind::CompatibilityRewrite,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassAction::MutatesAst,
       true,
       "rewrite experimental soa_vector inline borrow method compatibility forms"},
      {"experimental-soa-same-path-helper-methods",
       SemanticValidationPassKind::CompatibilityRewrite,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassAction::MutatesAst,
       true,
       "rewrite experimental soa_vector same-path helper method compatibility forms"},
      {"experimental-soa-to-aos-methods",
       SemanticValidationPassKind::CompatibilityRewrite,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassAction::MutatesAst,
       true,
       "rewrite experimental soa_vector to_aos method compatibility forms"},
      {"experimental-soa-field-view-indexes",
       SemanticValidationPassKind::CompatibilityRewrite,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassAction::MutatesAst,
       true,
       "rewrite experimental soa_vector field-view index compatibility forms"},
      {"experimental-soa-field-view-helpers",
       SemanticValidationPassKind::CompatibilityRewrite,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassAction::MutatesAst,
       true,
       "rewrite experimental soa_vector field-view helper compatibility forms"},
      {"experimental-soa-field-view-carrier-indexes",
       SemanticValidationPassKind::CompatibilityRewrite,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassAction::MutatesAst,
       true,
       "rewrite experimental soa_vector field-view carrier index compatibility forms"},
      {"experimental-soa-field-view-assign-targets",
       SemanticValidationPassKind::CompatibilityRewrite,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassAction::MutatesAst,
       true,
       "rewrite experimental soa_vector field-view assignment target compatibility forms"},
      {"borrowed-experimental-map-methods",
       SemanticValidationPassKind::CompatibilityRewrite,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassAction::MutatesAst,
       true,
       "rewrite borrowed experimental map method compatibility forms"},
      {"experimental-map-value-methods",
       SemanticValidationPassKind::CompatibilityRewrite,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassAction::MutatesAst,
       true,
       "rewrite experimental map value method compatibility forms"},
      {"builtin-map-insert-methods",
       SemanticValidationPassKind::CompatibilityRewrite,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassAction::MutatesAst,
       true,
       "rewrite builtin map insert method compatibility forms"},
      {"template-monomorphization",
       SemanticValidationPassKind::TemplateMonomorphization,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassAction::MutatesAst,
       false,
       "monomorphize templates after pre-template AST rewrites"},
      {"reflection-metadata-queries",
       SemanticValidationPassKind::CoreCanonicalization,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassAction::MutatesAst,
       false,
       "rewrite reflection metadata queries after template monomorphization"},
      {"convert-constructors",
       SemanticValidationPassKind::CoreCanonicalization,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassAction::MutatesAst,
       false,
       "rewrite primitive conversion constructors before validator passes"},
      {"validator-passes",
       SemanticValidationPassKind::Validation,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassOwnership::SemanticProductFacts,
       SemanticValidationPassAction::PublishesFacts,
       false,
       "run semantic validation and collect publication facts"},
      {"omitted-struct-initializers",
       SemanticValidationPassKind::CoreCanonicalization,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassAction::MutatesAst,
       false,
       "rewrite omitted struct initializer envelopes after validator inference"},
      {"semantic-node-id-assignment",
       SemanticValidationPassKind::CoreCanonicalization,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassAction::MutatesAst,
       false,
       "assign stable semantic node ids before semantic-product publication"},
      {"semantic-product-publication",
       SemanticValidationPassKind::Publication,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassOwnership::SemanticProductFacts,
       SemanticValidationPassAction::PublishesFacts,
       false,
       "publish immutable semantic-product facts after successful validation"},
  };
  return Manifest;
}

SemanticValidationPlan buildSemanticValidationPlan(const Program &program,
                                                   const std::string &entryPath) {
  SemanticValidationPlan plan;
  plan.definitionPrepass = buildDefinitionPrepassSnapshot(program);

  plan.entry.fullPath = entryPath;
  const auto entryIt =
      plan.definitionPrepass.firstDeclarationIndexByPath.find(std::string_view(entryPath));
  if (entryIt != plan.definitionPrepass.firstDeclarationIndexByPath.end()) {
    plan.entry.declared = true;
    plan.entry.stableOrderOffset = entryIt->second;
    plan.entry.stableIndex =
        plan.definitionPrepass.declarationsInStableOrder[entryIt->second].stableIndex;
  }

  plan.imports.hasSourceImports = !program.sourceImports.empty();
  plan.imports.sourceImportPaths = copyImportPaths(program.sourceImports);
  plan.imports.programImportPaths = copyImportPaths(program.imports);
  plan.imports.directImportPaths = plan.imports.hasSourceImports
                                       ? plan.imports.sourceImportPaths
                                       : plan.imports.programImportPaths;
  if (plan.imports.hasSourceImports) {
    std::unordered_set<std::string> directImportSet(
        plan.imports.directImportPaths.begin(),
        plan.imports.directImportPaths.end());
    for (const auto &importPath : plan.imports.programImportPaths) {
      if (directImportSet.count(importPath) == 0) {
        plan.imports.transitiveImportPaths.push_back(importPath);
      }
    }
  }

  plan.graphLocalAutoInputs.entryPath = entryPath;
  plan.graphLocalAutoInputs.definitionCount = program.definitions.size();
  plan.graphLocalAutoInputs.executionCount = program.executions.size();

  plan.executionSlice.executionsInStableOrder.reserve(program.executions.size());
  for (std::size_t stableIndex = 0; stableIndex < program.executions.size();
       ++stableIndex) {
    plan.executionSlice.executionsInStableOrder.push_back(
        SemanticValidationExecutionDeclaration{
            program.executions[stableIndex].fullPath,
            stableIndex,
        });
  }

  return plan;
}

} // namespace primec::semantics
