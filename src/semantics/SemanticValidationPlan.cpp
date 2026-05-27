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
       SemanticValidationPassId::SemanticTransformRules,
       SemanticValidationPassKind::CoreCanonicalization,
       SemanticValidationPassOwnership::AstSyntax,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassAction::MutatesAst,
       false,
       "strip text-only transforms, apply semantic transform hooks, and expand semantic transform rules"},
      {"experimental-gfx-constructors",
       SemanticValidationPassId::ExperimentalGfxConstructors,
       SemanticValidationPassKind::CompatibilityRewrite,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassAction::MutatesAst,
       true,
       "rewrite legacy gfx constructor spellings before template monomorphization"},
      {"reflection-generated-helpers",
       SemanticValidationPassId::ReflectionGeneratedHelpers,
       SemanticValidationPassKind::CoreCanonicalization,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassAction::MutatesAst,
       false,
       "materialize generated reflection helper definitions"},
      {"builtin-soa-conversion-methods",
       SemanticValidationPassId::BuiltinSoaConversionMethods,
       SemanticValidationPassKind::CompatibilityRewrite,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassAction::MutatesAst,
       true,
       "rewrite builtin soa" "_vector conversion method compatibility forms"},
      {"builtin-soa-to-aos-calls",
       SemanticValidationPassId::BuiltinSoaToAosCalls,
       SemanticValidationPassKind::CompatibilityRewrite,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassAction::MutatesAst,
       true,
       "rewrite builtin soa" "_vector to" "_aos compatibility calls"},
      {"builtin-soa-helper-return-metadata",
       SemanticValidationPassId::BuiltinSoaHelperReturnMetadata,
       SemanticValidationPassKind::CoreCanonicalization,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassAction::ValidatesOnly,
       false,
       "validate builtin soa" "_vector helper return metadata before helper rewrites"},
      {"builtin-soa-access-calls",
       SemanticValidationPassId::BuiltinSoaAccessCalls,
       SemanticValidationPassKind::CompatibilityRewrite,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassAction::MutatesAst,
       true,
       "rewrite builtin soa" "_vector access helper compatibility calls"},
      {"builtin-soa-count-calls",
       SemanticValidationPassId::BuiltinSoaCountCalls,
       SemanticValidationPassKind::CompatibilityRewrite,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassAction::MutatesAst,
       true,
       "rewrite builtin soa" "_vector count helper compatibility calls"},
      {"builtin-soa-mutator-calls",
       SemanticValidationPassId::BuiltinSoaMutatorCalls,
       SemanticValidationPassKind::CompatibilityRewrite,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassAction::MutatesAst,
       true,
       "rewrite builtin soa" "_vector mutator helper compatibility calls"},
      {"experimental-soa-inline-borrow-methods",
       SemanticValidationPassId::ExperimentalSoaInlineBorrowMethods,
       SemanticValidationPassKind::CompatibilityRewrite,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassAction::MutatesAst,
       true,
       "rewrite experimental soa" "_vector inline borrow method compatibility forms"},
      {"experimental-soa-same-path-helper-methods",
       SemanticValidationPassId::ExperimentalSoaSamePathHelperMethods,
       SemanticValidationPassKind::CompatibilityRewrite,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassAction::MutatesAst,
       true,
       "rewrite experimental soa" "_vector same-path helper method compatibility forms"},
      {"experimental-soa-to-aos-methods",
       SemanticValidationPassId::ExperimentalSoaToAosMethods,
       SemanticValidationPassKind::CompatibilityRewrite,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassAction::MutatesAst,
       true,
       "rewrite experimental soa" "_vector to" "_aos method compatibility forms"},
      {"experimental-soa-field-view-indexes",
       SemanticValidationPassId::ExperimentalSoaFieldViewIndexes,
       SemanticValidationPassKind::CompatibilityRewrite,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassAction::MutatesAst,
       true,
       "rewrite experimental soa" "_vector field-view index compatibility forms"},
      {"experimental-soa-field-view-helpers",
       SemanticValidationPassId::ExperimentalSoaFieldViewHelpers,
       SemanticValidationPassKind::CompatibilityRewrite,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassAction::MutatesAst,
       true,
       "rewrite experimental soa" "_vector field-view helper compatibility forms"},
      {"experimental-soa-field-view-carrier-indexes",
       SemanticValidationPassId::ExperimentalSoaFieldViewCarrierIndexes,
       SemanticValidationPassKind::CompatibilityRewrite,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassAction::MutatesAst,
       true,
       "rewrite experimental soa" "_vector field-view carrier index compatibility forms"},
      {"experimental-soa-field-view-assign-targets",
       SemanticValidationPassId::ExperimentalSoaFieldViewAssignTargets,
       SemanticValidationPassKind::CompatibilityRewrite,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassAction::MutatesAst,
       true,
       "rewrite experimental soa" "_vector field-view assignment target compatibility forms"},
      {"borrowed-experimental-map-methods",
       SemanticValidationPassId::BorrowedExperimentalMapMethods,
       SemanticValidationPassKind::CompatibilityRewrite,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassAction::MutatesAst,
       true,
       "rewrite borrowed experimental map method compatibility forms"},
      {"experimental-map-value-methods",
       SemanticValidationPassId::ExperimentalMapValueMethods,
       SemanticValidationPassKind::CompatibilityRewrite,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassAction::MutatesAst,
       true,
       "rewrite experimental map value method compatibility forms"},
      {"builtin-map-insert-methods",
       SemanticValidationPassId::BuiltinMapInsertMethods,
       SemanticValidationPassKind::CompatibilityRewrite,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassAction::MutatesAst,
       true,
       "rewrite builtin map insert method compatibility forms"},
      {"compile-time-branch-pruning",
       SemanticValidationPassId::CompileTimeBranchPruning,
       SemanticValidationPassKind::CoreCanonicalization,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassAction::MutatesAst,
       false,
       "select statement-level compile-time branches before template monomorphization"},
      {"template-monomorphization",
       SemanticValidationPassId::TemplateMonomorphization,
       SemanticValidationPassKind::TemplateMonomorphization,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassAction::MutatesAst,
       false,
       "monomorphize templates after pre-template AST rewrites"},
      {"compile-time-specialized-branch-pruning",
       SemanticValidationPassId::CompileTimeSpecializedBranchPruning,
       SemanticValidationPassKind::CoreCanonicalization,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassAction::MutatesAst,
       false,
       "select specialized compile-time branches after template monomorphization"},
      {"reflection-metadata-queries",
       SemanticValidationPassId::ReflectionMetadataQueries,
       SemanticValidationPassKind::CoreCanonicalization,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassAction::MutatesAst,
       false,
       "rewrite reflection metadata queries after template monomorphization"},
      {"convert-constructors",
       SemanticValidationPassId::ConvertConstructors,
       SemanticValidationPassKind::CoreCanonicalization,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassAction::MutatesAst,
       false,
       "rewrite primitive conversion constructors before validator passes"},
      {"validator-passes",
       SemanticValidationPassId::ValidatorPasses,
       SemanticValidationPassKind::Validation,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassOwnership::SemanticProductFacts,
       SemanticValidationPassAction::PublishesFacts,
       false,
       "run semantic validation and collect publication facts"},
      {"omitted-struct-initializers",
       SemanticValidationPassId::OmittedStructInitializers,
       SemanticValidationPassKind::CoreCanonicalization,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassAction::MutatesAst,
       false,
       "rewrite omitted struct initializer envelopes after validator inference"},
      {"semantic-node-id-assignment",
       SemanticValidationPassId::SemanticNodeIdAssignment,
       SemanticValidationPassKind::CoreCanonicalization,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassOwnership::SemanticCanonicalAst,
       SemanticValidationPassAction::MutatesAst,
       false,
       "assign stable semantic node ids before semantic-product publication"},
      {"semantic-product-publication",
       SemanticValidationPassId::SemanticProductPublication,
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
