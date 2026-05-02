#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

namespace {

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

template <typename Entry, typename Predicate>
bool anySemanticEntry(const std::vector<Entry> &entries, const Predicate &predicate) {
  return std::any_of(entries.begin(), entries.end(), predicate);
}

const primec::SemanticProgramModuleResolvedArtifacts *findModuleArtifacts(const primec::SemanticProgram &semanticProgram,
                                                                          std::string_view moduleKey) {
  return findSemanticEntry(
      semanticProgram.moduleResolvedArtifacts,
      [moduleKey](const primec::SemanticProgramModuleResolvedArtifacts &entry) {
        return entry.identity.moduleKey == moduleKey;
      });
}

void populateOneSemanticModuleViewEntry(primec::SemanticProgram &semanticProgram) {
  semanticProgram.directCallTargets.emplace_back();
  semanticProgram.methodCallTargets.emplace_back();
  semanticProgram.bridgePathChoices.emplace_back();
  semanticProgram.callableSummaries.emplace_back();
  semanticProgram.bindingFacts.emplace_back();
  semanticProgram.returnFacts.emplace_back();
  semanticProgram.collectionSpecializations.emplace_back();
  semanticProgram.localAutoFacts.emplace_back();
  semanticProgram.queryFacts.emplace_back();
  semanticProgram.tryFacts.emplace_back();
  semanticProgram.onErrorFacts.emplace_back();
}

void indexOneSemanticModuleViewEntry(primec::SemanticProgramModuleResolvedArtifacts &module) {
  module.directCallTargetIndices.push_back(0);
  module.methodCallTargetIndices.push_back(0);
  module.bridgePathChoiceIndices.push_back(0);
  module.callableSummaryIndices.push_back(0);
  module.bindingFactIndices.push_back(0);
  module.returnFactIndices.push_back(0);
  module.collectionSpecializationIndices.push_back(0);
  module.localAutoFactIndices.push_back(0);
  module.queryFactIndices.push_back(0);
  module.tryFactIndices.push_back(0);
  module.onErrorFactIndices.push_back(0);
}

void checkSemanticModuleViewsEmpty(const primec::SemanticProgram &semanticProgram) {
  CHECK(primec::semanticProgramDirectCallTargetView(semanticProgram).empty());
  CHECK(primec::semanticProgramMethodCallTargetView(semanticProgram).empty());
  CHECK(primec::semanticProgramBridgePathChoiceView(semanticProgram).empty());
  CHECK(primec::semanticProgramCallableSummaryView(semanticProgram).empty());
  CHECK(primec::semanticProgramBindingFactView(semanticProgram).empty());
  CHECK(primec::semanticProgramReturnFactView(semanticProgram).empty());
  CHECK(primec::semanticProgramCollectionSpecializationView(semanticProgram).empty());
  CHECK(primec::semanticProgramLocalAutoFactView(semanticProgram).empty());
  CHECK(primec::semanticProgramQueryFactView(semanticProgram).empty());
  CHECK(primec::semanticProgramTryFactView(semanticProgram).empty());
  CHECK(primec::semanticProgramOnErrorFactView(semanticProgram).empty());
}

void checkSemanticModuleViewsReturnStoredEntry(const primec::SemanticProgram &semanticProgram) {
  const auto directCallTargets = primec::semanticProgramDirectCallTargetView(semanticProgram);
  const auto methodCallTargets = primec::semanticProgramMethodCallTargetView(semanticProgram);
  const auto bridgePathChoices = primec::semanticProgramBridgePathChoiceView(semanticProgram);
  const auto callableSummaries = primec::semanticProgramCallableSummaryView(semanticProgram);
  const auto bindingFacts = primec::semanticProgramBindingFactView(semanticProgram);
  const auto returnFacts = primec::semanticProgramReturnFactView(semanticProgram);
  const auto collectionSpecializations =
      primec::semanticProgramCollectionSpecializationView(semanticProgram);
  const auto localAutoFacts = primec::semanticProgramLocalAutoFactView(semanticProgram);
  const auto queryFacts = primec::semanticProgramQueryFactView(semanticProgram);
  const auto tryFacts = primec::semanticProgramTryFactView(semanticProgram);
  const auto onErrorFacts = primec::semanticProgramOnErrorFactView(semanticProgram);

  REQUIRE(directCallTargets.size() == 1);
  REQUIRE(methodCallTargets.size() == 1);
  REQUIRE(bridgePathChoices.size() == 1);
  REQUIRE(callableSummaries.size() == 1);
  REQUIRE(bindingFacts.size() == 1);
  REQUIRE(returnFacts.size() == 1);
  REQUIRE(collectionSpecializations.size() == 1);
  REQUIRE(localAutoFacts.size() == 1);
  REQUIRE(queryFacts.size() == 1);
  REQUIRE(tryFacts.size() == 1);
  REQUIRE(onErrorFacts.size() == 1);

  CHECK(directCallTargets.front() == &semanticProgram.directCallTargets.front());
  CHECK(methodCallTargets.front() == &semanticProgram.methodCallTargets.front());
  CHECK(bridgePathChoices.front() == &semanticProgram.bridgePathChoices.front());
  CHECK(callableSummaries.front() == &semanticProgram.callableSummaries.front());
  CHECK(bindingFacts.front() == &semanticProgram.bindingFacts.front());
  CHECK(returnFacts.front() == &semanticProgram.returnFacts.front());
  CHECK(collectionSpecializations.front() == &semanticProgram.collectionSpecializations.front());
  CHECK(localAutoFacts.front() == &semanticProgram.localAutoFacts.front());
  CHECK(queryFacts.front() == &semanticProgram.queryFacts.front());
  CHECK(tryFacts.front() == &semanticProgram.tryFacts.front());
  CHECK(onErrorFacts.front() == &semanticProgram.onErrorFacts.front());
}

void populateGraphFactFormatterStrings(primec::SemanticProgram &semanticProgram) {
  primec::SemanticProgramLocalAutoFact localAutoFact;
  localAutoFact.scopePath = "/raw/local";
  localAutoFact.bindingName = "rawLocal";
  localAutoFact.bindingTypeText = "RawLocalType";
  localAutoFact.initializerBindingTypeText = "RawInitializerType";
  localAutoFact.initializerTryValueType = "RawTryValue";
  localAutoFact.initializerTryErrorType = "RawTryError";
  semanticProgram.localAutoFacts.push_back(std::move(localAutoFact));

  primec::SemanticProgramQueryFact queryFact;
  queryFact.scopePath = "/raw/query";
  queryFact.callName = "rawQuery";
  queryFact.queryTypeText = "RawQueryType";
  queryFact.bindingTypeText = "RawQueryBinding";
  queryFact.resultValueType = "RawQueryValue";
  queryFact.resultErrorType = "RawQueryError";
  semanticProgram.queryFacts.push_back(std::move(queryFact));

  primec::SemanticProgramTryFact tryFact;
  tryFact.scopePath = "/raw/try";
  tryFact.operandBindingTypeText = "RawTryOperand";
  tryFact.valueType = "RawTryValue";
  tryFact.errorType = "RawTryError";
  tryFact.contextReturnKind = "RawTryReturn";
  tryFact.onErrorErrorType = "RawTryOnError";
  semanticProgram.tryFacts.push_back(std::move(tryFact));

  primec::SemanticProgramOnErrorFact onErrorFact;
  onErrorFact.definitionPath = "/raw/on_error";
  onErrorFact.returnKind = "RawOnErrorReturn";
  onErrorFact.errorType = "RawOnErrorError";
  onErrorFact.boundArgTexts = {"RawBoundArg"};
  onErrorFact.returnResultValueType = "RawOnErrorValue";
  onErrorFact.returnResultErrorType = "RawOnErrorResultError";
  semanticProgram.onErrorFacts.push_back(std::move(onErrorFact));
}

void indexGraphFactFormatterStrings(primec::SemanticProgram &semanticProgram) {
  auto &localAutoFact = semanticProgram.localAutoFacts.front();
  localAutoFact.scopePathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/mapped/local");
  localAutoFact.bindingNameId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "mappedLocal");
  localAutoFact.bindingTypeTextId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "MappedLocalType");
  localAutoFact.initializerBindingTypeTextId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "MappedInitializerType");
  localAutoFact.initializerTryValueTypeId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "MappedTryValue");
  localAutoFact.initializerTryErrorTypeId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "MappedTryError");

  auto &queryFact = semanticProgram.queryFacts.front();
  queryFact.scopePathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/mapped/query");
  queryFact.callNameId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "mappedQuery");
  queryFact.queryTypeTextId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "MappedQueryType");
  queryFact.bindingTypeTextId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "MappedQueryBinding");
  queryFact.resultValueTypeId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "MappedQueryValue");
  queryFact.resultErrorTypeId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "MappedQueryError");

  auto &tryFact = semanticProgram.tryFacts.front();
  tryFact.scopePathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/mapped/try");
  tryFact.operandBindingTypeTextId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "MappedTryOperand");
  tryFact.valueTypeId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "MappedTryValue");
  tryFact.errorTypeId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "MappedTryError");
  tryFact.contextReturnKindId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "MappedTryReturn");
  tryFact.onErrorErrorTypeId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "MappedTryOnError");

  auto &onErrorFact = semanticProgram.onErrorFacts.front();
  onErrorFact.definitionPathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/mapped/on_error");
  onErrorFact.returnKindId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "MappedOnErrorReturn");
  onErrorFact.errorTypeId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "MappedOnErrorError");
  onErrorFact.boundArgTextIds = {
      primec::semanticProgramInternCallTargetString(semanticProgram, "MappedBoundArg")};
  onErrorFact.returnResultValueTypeId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "MappedOnErrorValue");
  onErrorFact.returnResultErrorTypeId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "MappedOnErrorResultError");
}

void indexGraphFactModuleEntries(primec::SemanticProgram &semanticProgram) {
  primec::SemanticProgramModuleResolvedArtifacts module;
  module.identity.moduleKey = "/main";
  module.localAutoFactIndices.push_back(0);
  module.queryFactIndices.push_back(0);
  module.tryFactIndices.push_back(0);
  module.onErrorFactIndices.push_back(0);
  semanticProgram.moduleResolvedArtifacts.push_back(std::move(module));
}

void populateDefinitionMetadata(primec::SemanticProgram &semanticProgram) {
  primec::SemanticProgramDefinition laterDefinition;
  laterDefinition.fullPath = "/later";
  semanticProgram.definitions.push_back(std::move(laterDefinition));

  primec::SemanticProgramDefinition earlierDefinition;
  earlierDefinition.fullPath = "/earlier";
  semanticProgram.definitions.push_back(std::move(earlierDefinition));
}

void checkDefinitionMetadataVisible(const primec::SemanticProgram &semanticProgram) {
  const auto definitions = primec::semanticProgramDefinitionView(semanticProgram);
  REQUIRE(definitions.size() == 2);
  CHECK(definitions[0] == &semanticProgram.definitions[0]);
  CHECK(definitions[1] == &semanticProgram.definitions[1]);

  const auto *laterDefinition =
      primec::semanticProgramLookupPublishedDefinition(semanticProgram, "/later");
  const auto *earlierDefinition =
      primec::semanticProgramLookupPublishedDefinition(semanticProgram, "/earlier");
  REQUIRE(laterDefinition != nullptr);
  REQUIRE(earlierDefinition != nullptr);
  CHECK(laterDefinition == &semanticProgram.definitions[0]);
  CHECK(earlierDefinition == &semanticProgram.definitions[1]);
}

void populateParticleTypeMetadata(primec::SemanticProgram &semanticProgram) {
  primec::SemanticProgramTypeMetadata typeMetadata;
  typeMetadata.fullPath = "/Particle";
  typeMetadata.category = "struct";
  typeMetadata.fieldCount = 2;
  semanticProgram.typeMetadata.push_back(std::move(typeMetadata));

  primec::SemanticProgramStructFieldMetadata rightField;
  rightField.structPath = "/Particle";
  rightField.fieldName = "right";
  rightField.fieldIndex = 1;
  rightField.bindingTypeText = "i64";
  semanticProgram.structFieldMetadata.push_back(std::move(rightField));

  primec::SemanticProgramStructFieldMetadata leftField;
  leftField.structPath = "/Particle";
  leftField.fieldName = "left";
  leftField.fieldIndex = 0;
  leftField.bindingTypeText = "i32";
  semanticProgram.structFieldMetadata.push_back(std::move(leftField));
}

void checkParticleTypeMetadataVisible(const primec::SemanticProgram &semanticProgram) {
  const auto *typeMetadata =
      primec::semanticProgramLookupTypeMetadata(semanticProgram, "/Particle");
  REQUIRE(typeMetadata != nullptr);
  CHECK(typeMetadata == &semanticProgram.typeMetadata.front());

  const auto allTypes = primec::semanticProgramTypeMetadataView(semanticProgram);
  REQUIRE(allTypes.size() == 1);
  CHECK(allTypes.front() == &semanticProgram.typeMetadata.front());

  const auto structTypes = primec::semanticProgramStructTypeMetadataView(semanticProgram);
  REQUIRE(structTypes.size() == 1);
  CHECK(structTypes.front() == &semanticProgram.typeMetadata.front());

  const auto allFields = primec::semanticProgramStructFieldMetadataView(semanticProgram);
  REQUIRE(allFields.size() == 2);
  CHECK(allFields[0] == &semanticProgram.structFieldMetadata[0]);
  CHECK(allFields[1] == &semanticProgram.structFieldMetadata[1]);

  const auto fields =
      primec::semanticProgramStructFieldMetadataView(semanticProgram, "/Particle");
  REQUIRE(fields.size() == 2);
  CHECK(fields[0] == &semanticProgram.structFieldMetadata[1]);
  CHECK(fields[1] == &semanticProgram.structFieldMetadata[0]);
}

void populateResultSumMetadata(primec::SemanticProgram &semanticProgram) {
  primec::SemanticProgramSumTypeMetadata sumMetadata;
  sumMetadata.fullPath = "/Result";
  sumMetadata.isPublic = true;
  sumMetadata.activeTagTypeText = "u32";
  sumMetadata.payloadStorageText = "ResultPayload";
  sumMetadata.variantCount = 2;
  semanticProgram.sumTypeMetadata.push_back(std::move(sumMetadata));

  primec::SemanticProgramSumVariantMetadata errorVariant;
  errorVariant.sumPath = "/Result";
  errorVariant.variantName = "error";
  errorVariant.variantIndex = 1;
  errorVariant.tagValue = 1;
  errorVariant.hasPayload = true;
  errorVariant.payloadTypeText = "Err";
  semanticProgram.sumVariantMetadata.push_back(std::move(errorVariant));

  primec::SemanticProgramSumVariantMetadata okVariant;
  okVariant.sumPath = "/Result";
  okVariant.variantName = "ok";
  okVariant.variantIndex = 0;
  okVariant.tagValue = 0;
  okVariant.hasPayload = true;
  okVariant.payloadTypeText = "i32";
  semanticProgram.sumVariantMetadata.push_back(std::move(okVariant));
}

uint64_t makeTestSumVariantMetadataKey(primec::SymbolId sumPathId,
                                       primec::SymbolId variantNameId) {
  return (static_cast<uint64_t>(sumPathId) << 32) |
         static_cast<uint64_t>(variantNameId);
}

void checkResultSumMetadataVisible(const primec::SemanticProgram &semanticProgram) {
  const auto *sumMetadata =
      primec::semanticProgramLookupPublishedSumTypeMetadata(semanticProgram, "/Result");
  REQUIRE(sumMetadata != nullptr);
  CHECK(sumMetadata == &semanticProgram.sumTypeMetadata.front());

  const auto sumTypes = primec::semanticProgramSumTypeMetadataView(semanticProgram);
  REQUIRE(sumTypes.size() == 1);
  CHECK(sumTypes.front() == &semanticProgram.sumTypeMetadata.front());

  const auto sumVariants = primec::semanticProgramSumVariantMetadataView(semanticProgram);
  REQUIRE(sumVariants.size() == 2);
  CHECK(sumVariants[0] == &semanticProgram.sumVariantMetadata[0]);
  CHECK(sumVariants[1] == &semanticProgram.sumVariantMetadata[1]);

  const auto *okVariant = primec::semanticProgramLookupPublishedSumVariantMetadata(
      semanticProgram, "/Result", "ok");
  REQUIRE(okVariant != nullptr);
  CHECK(okVariant == &semanticProgram.sumVariantMetadata[1]);

  const auto *errorVariant = primec::semanticProgramLookupPublishedSumVariantMetadata(
      semanticProgram, "/Result", "error");
  REQUIRE(errorVariant != nullptr);
  CHECK(errorVariant == &semanticProgram.sumVariantMetadata[0]);
}

} // namespace

TEST_CASE("semantic product module views require module indexes after freeze") {
  primec::SemanticProgram mutableSemanticProgram;
  populateOneSemanticModuleViewEntry(mutableSemanticProgram);

  checkSemanticModuleViewsReturnStoredEntry(mutableSemanticProgram);

  primec::SemanticProgram rawFrozenSemanticProgram;
  populateOneSemanticModuleViewEntry(rawFrozenSemanticProgram);
  primec::freezeSemanticProgramPublishedStorage(rawFrozenSemanticProgram);

  checkSemanticModuleViewsEmpty(rawFrozenSemanticProgram);

  primec::SemanticProgram missingIndexSemanticProgram;
  populateOneSemanticModuleViewEntry(missingIndexSemanticProgram);
  primec::SemanticProgramModuleResolvedArtifacts emptyModule;
  emptyModule.identity.moduleKey = "/main";
  missingIndexSemanticProgram.moduleResolvedArtifacts.push_back(std::move(emptyModule));
  primec::freezeSemanticProgramPublishedStorage(missingIndexSemanticProgram);

  checkSemanticModuleViewsEmpty(missingIndexSemanticProgram);

  primec::SemanticProgram mappedSemanticProgram;
  populateOneSemanticModuleViewEntry(mappedSemanticProgram);
  primec::SemanticProgramModuleResolvedArtifacts mappedModule;
  mappedModule.identity.moduleKey = "/main";
  indexOneSemanticModuleViewEntry(mappedModule);
  mappedSemanticProgram.moduleResolvedArtifacts.push_back(std::move(mappedModule));
  primec::freezeSemanticProgramPublishedStorage(mappedSemanticProgram);

  checkSemanticModuleViewsReturnStoredEntry(mappedSemanticProgram);
}

TEST_CASE("semantic product graph fact dumps require interned text after freeze") {
  primec::SemanticProgram mutableSemanticProgram;
  populateGraphFactFormatterStrings(mutableSemanticProgram);
  indexGraphFactModuleEntries(mutableSemanticProgram);

  const std::string mutableDump = primec::formatSemanticProgram(mutableSemanticProgram);
  CHECK(mutableDump.find("/raw/local") != std::string::npos);
  CHECK(mutableDump.find("RawQueryType") != std::string::npos);
  CHECK(mutableDump.find("RawTryReturn") != std::string::npos);
  CHECK(mutableDump.find("RawBoundArg") != std::string::npos);

  primec::SemanticProgram rawFrozenSemanticProgram;
  populateGraphFactFormatterStrings(rawFrozenSemanticProgram);
  indexGraphFactModuleEntries(rawFrozenSemanticProgram);
  primec::freezeSemanticProgramPublishedStorage(rawFrozenSemanticProgram);

  const std::string rawFrozenDump = primec::formatSemanticProgram(rawFrozenSemanticProgram);
  CHECK(rawFrozenDump.find("local_auto_facts[0]") != std::string::npos);
  CHECK(rawFrozenDump.find("query_facts[0]") != std::string::npos);
  CHECK(rawFrozenDump.find("try_facts[0]") != std::string::npos);
  CHECK(rawFrozenDump.find("on_error_facts[0]") != std::string::npos);
  CHECK(rawFrozenDump.find("/raw/local") == std::string::npos);
  CHECK(rawFrozenDump.find("RawQueryType") == std::string::npos);
  CHECK(rawFrozenDump.find("RawTryReturn") == std::string::npos);
  CHECK(rawFrozenDump.find("RawBoundArg") == std::string::npos);

  primec::SemanticProgram mappedSemanticProgram;
  populateGraphFactFormatterStrings(mappedSemanticProgram);
  indexGraphFactFormatterStrings(mappedSemanticProgram);
  indexGraphFactModuleEntries(mappedSemanticProgram);
  primec::freezeSemanticProgramPublishedStorage(mappedSemanticProgram);

  const std::string mappedDump = primec::formatSemanticProgram(mappedSemanticProgram);
  CHECK(mappedDump.find("/mapped/local") != std::string::npos);
  CHECK(mappedDump.find("MappedQueryType") != std::string::npos);
  CHECK(mappedDump.find("MappedTryReturn") != std::string::npos);
  CHECK(mappedDump.find("MappedBoundArg") != std::string::npos);
  CHECK(mappedDump.find("/raw/local") == std::string::npos);
  CHECK(mappedDump.find("RawQueryType") == std::string::npos);
  CHECK(mappedDump.find("RawTryReturn") == std::string::npos);
  CHECK(mappedDump.find("RawBoundArg") == std::string::npos);
}

TEST_CASE("semantic product definition view requires published maps after freeze") {
  primec::SemanticProgram mutableSemanticProgram;
  populateDefinitionMetadata(mutableSemanticProgram);

  CHECK(primec::semanticProgramDefinitionView(mutableSemanticProgram).size() == 2);

  primec::SemanticProgram rawFrozenSemanticProgram;
  populateDefinitionMetadata(rawFrozenSemanticProgram);
  primec::freezeSemanticProgramPublishedStorage(rawFrozenSemanticProgram);

  CHECK(primec::semanticProgramDefinitionView(rawFrozenSemanticProgram).empty());
  CHECK(primec::semanticProgramLookupPublishedDefinition(rawFrozenSemanticProgram, "/later") ==
        nullptr);
  CHECK(primec::formatSemanticProgram(rawFrozenSemanticProgram)
            .find("definitions[0]") == std::string::npos);

  primec::SemanticProgram mappedSemanticProgram;
  populateDefinitionMetadata(mappedSemanticProgram);
  const auto laterPathId =
      primec::semanticProgramInternCallTargetString(mappedSemanticProgram, "/later");
  const auto earlierPathId =
      primec::semanticProgramInternCallTargetString(mappedSemanticProgram, "/earlier");
  mappedSemanticProgram.publishedRoutingLookups.definitionIndicesByPathId.insert_or_assign(
      earlierPathId, 1);
  mappedSemanticProgram.publishedRoutingLookups.definitionIndicesByPathId.insert_or_assign(
      laterPathId, 0);
  primec::freezeSemanticProgramPublishedStorage(mappedSemanticProgram);

  checkDefinitionMetadataVisible(mappedSemanticProgram);
  const std::string mappedDump = primec::formatSemanticProgram(mappedSemanticProgram);
  const std::size_t laterPos = mappedDump.find("definitions[0]: full_path=\"/later\"");
  const std::size_t earlierPos = mappedDump.find("definitions[1]: full_path=\"/earlier\"");
  CHECK(laterPos != std::string::npos);
  CHECK(earlierPos != std::string::npos);
  CHECK(laterPos < earlierPos);
}

TEST_CASE("semantic product type metadata requires published maps after freeze") {
  primec::SemanticProgram mutableSemanticProgram;
  populateParticleTypeMetadata(mutableSemanticProgram);

  checkParticleTypeMetadataVisible(mutableSemanticProgram);

  primec::SemanticProgram rawFrozenSemanticProgram;
  populateParticleTypeMetadata(rawFrozenSemanticProgram);
  primec::freezeSemanticProgramPublishedStorage(rawFrozenSemanticProgram);

  CHECK(primec::semanticProgramLookupTypeMetadata(rawFrozenSemanticProgram, "/Particle") ==
        nullptr);
  CHECK(primec::semanticProgramTypeMetadataView(rawFrozenSemanticProgram).empty());
  CHECK(primec::semanticProgramStructTypeMetadataView(rawFrozenSemanticProgram).empty());
  CHECK(primec::semanticProgramStructFieldMetadataView(rawFrozenSemanticProgram).empty());
  CHECK(primec::semanticProgramStructFieldMetadataView(rawFrozenSemanticProgram, "/Particle").empty());
  const std::string rawFrozenDump = primec::formatSemanticProgram(rawFrozenSemanticProgram);
  CHECK(rawFrozenDump.find("type_metadata[0]") == std::string::npos);
  CHECK(rawFrozenDump.find("struct_field_metadata[0]") == std::string::npos);

  primec::SemanticProgram mappedSemanticProgram;
  populateParticleTypeMetadata(mappedSemanticProgram);
  const auto particlePathId =
      primec::semanticProgramInternCallTargetString(mappedSemanticProgram, "/Particle");
  mappedSemanticProgram.publishedRoutingLookups.typeMetadataIndicesByPathId.insert_or_assign(
      particlePathId, 0);
  mappedSemanticProgram.publishedRoutingLookups.structFieldMetadataIndicesByStructPathId
      [particlePathId] = {0, 1};
  primec::freezeSemanticProgramPublishedStorage(mappedSemanticProgram);

  checkParticleTypeMetadataVisible(mappedSemanticProgram);
  const std::string mappedDump = primec::formatSemanticProgram(mappedSemanticProgram);
  CHECK(mappedDump.find("type_metadata[0]: full_path=\"/Particle\"") != std::string::npos);
  const std::size_t rightPos =
      mappedDump.find("struct_field_metadata[0]: struct_path=\"/Particle\" field_name=\"right\"");
  const std::size_t leftPos =
      mappedDump.find("struct_field_metadata[1]: struct_path=\"/Particle\" field_name=\"left\"");
  CHECK(rightPos != std::string::npos);
  CHECK(leftPos != std::string::npos);
  CHECK(rightPos < leftPos);
}

TEST_CASE("semantic product sum metadata requires published maps after freeze") {
  primec::SemanticProgram mutableSemanticProgram;
  populateResultSumMetadata(mutableSemanticProgram);

  checkResultSumMetadataVisible(mutableSemanticProgram);

  primec::SemanticProgram rawFrozenSemanticProgram;
  populateResultSumMetadata(rawFrozenSemanticProgram);
  primec::freezeSemanticProgramPublishedStorage(rawFrozenSemanticProgram);

  CHECK(primec::semanticProgramLookupPublishedSumTypeMetadata(
            rawFrozenSemanticProgram, "/Result") == nullptr);
  CHECK(primec::semanticProgramLookupPublishedSumVariantMetadata(
            rawFrozenSemanticProgram, "/Result", "ok") == nullptr);
  CHECK(primec::semanticProgramSumTypeMetadataView(rawFrozenSemanticProgram).empty());
  CHECK(primec::semanticProgramSumVariantMetadataView(rawFrozenSemanticProgram).empty());
  const std::string rawFrozenDump = primec::formatSemanticProgram(rawFrozenSemanticProgram);
  CHECK(rawFrozenDump.find("sum_type_metadata[0]") == std::string::npos);
  CHECK(rawFrozenDump.find("sum_variant_metadata[0]") == std::string::npos);

  primec::SemanticProgram mappedSemanticProgram;
  populateResultSumMetadata(mappedSemanticProgram);
  const auto resultPathId =
      primec::semanticProgramInternCallTargetString(mappedSemanticProgram, "/Result");
  const auto okNameId =
      primec::semanticProgramInternCallTargetString(mappedSemanticProgram, "ok");
  const auto errorNameId =
      primec::semanticProgramInternCallTargetString(mappedSemanticProgram, "error");
  mappedSemanticProgram.publishedRoutingLookups.sumTypeMetadataIndicesByPathId
      .insert_or_assign(resultPathId, 0);
  mappedSemanticProgram.publishedRoutingLookups
      .sumVariantMetadataIndicesBySumPathAndVariantNameId
      .insert_or_assign(makeTestSumVariantMetadataKey(resultPathId, okNameId), 1);
  mappedSemanticProgram.publishedRoutingLookups
      .sumVariantMetadataIndicesBySumPathAndVariantNameId
      .insert_or_assign(makeTestSumVariantMetadataKey(resultPathId, errorNameId), 0);
  primec::freezeSemanticProgramPublishedStorage(mappedSemanticProgram);

  checkResultSumMetadataVisible(mappedSemanticProgram);
  const std::string mappedDump = primec::formatSemanticProgram(mappedSemanticProgram);
  CHECK(mappedDump.find("sum_type_metadata[0]: full_path=\"/Result\"") !=
        std::string::npos);
  const std::size_t errorPos =
      mappedDump.find("sum_variant_metadata[0]: sum_path=\"/Result\" variant_name=\"error\"");
  const std::size_t okPos =
      mappedDump.find("sum_variant_metadata[1]: sum_path=\"/Result\" variant_name=\"ok\"");
  CHECK(errorPos != std::string::npos);
  CHECK(okPos != std::string::npos);
  CHECK(errorPos < okPos);
}

TEST_CASE("semantics validate publishes allowlisted pilot routing artifacts") {
  const std::string source = R"(
import /std/collections/*

[return<i32>]
id_i32([i32] value) {
  return(value)
}

[effects(heap_alloc), return<i32>]
main() {
  [auto] selected{id_i32(1i32)}
  [auto] values{vector<i32>(1i32)}
  [i32] viaMethod{values.count()}
  [i32] viaBridge{count(values)}
  return(plus(selected, plus(viaMethod, viaBridge)))
}
)";

  primec::Program program;
  std::string error;
  const std::vector<std::string> collectorAllowlist = {
      "direct_call_targets",
      "method_call_targets",
      "bridge_path_choices",
      "callable_summaries",
  };
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  primec::SemanticProgram semanticProgram;
  const std::filesystem::path tempPath = makeTempIrPipelineSourcePath();
  {
    std::ofstream file(tempPath);
    REQUIRE(file.good());
    file << source;
  }

  primec::Options options;
  options.inputPath = tempPath.string();
  options.entryPath = "/main";
  options.defaultEffects = defaults;
  options.entryDefaultEffects = defaults;
  options.benchmarkSemanticFactFamiliesSpecified = true;
  options.benchmarkSemanticFactFamilies = collectorAllowlist;
  applyCompilePipelineSemanticProductIntentForTesting(
      options, CompilePipelineSemanticProductIntentForTesting::Require);
  primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);

  primec::CompilePipelineOutput output;
  primec::CompilePipelineErrorStage errorStage = primec::CompilePipelineErrorStage::None;
  const bool ok = primec::runCompilePipeline(options, output, errorStage, error);
  std::error_code ec;
  std::filesystem::remove(tempPath, ec);
  REQUIRE(ok);
  CHECK(error.empty());
  REQUIRE(output.hasSemanticProgram);
  program = std::move(output.program);
  semanticProgram = std::move(output.semanticProgram);

  const auto *directEntry = findSemanticEntry(
      primec::semanticProgramDirectCallTargetView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramDirectCallTarget &entry) {
        return entry.scopePath == "/main" && entry.callName == "id_i32" &&
               primec::semanticProgramDirectCallTargetResolvedPath(semanticProgram, entry) == "/id_i32";
      });
  const auto *methodEntry = findSemanticEntry(
      primec::semanticProgramMethodCallTargetView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramMethodCallTarget &entry) {
        return entry.scopePath == "/main" && entry.methodName == "count" &&
               primec::semanticProgramMethodCallTargetResolvedPath(semanticProgram, entry) ==
                   "/std/collections/vector/count";
      });
  const auto *bridgeEntry = findSemanticEntry(
      primec::semanticProgramBridgePathChoiceView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramBridgePathChoice &entry) {
        return primec::semanticProgramBridgePathChoiceHelperName(semanticProgram, entry) == "count" &&
               primec::semanticProgramResolveCallTargetString(semanticProgram, entry.chosenPathId) ==
                   "/std/collections/vector/count";
      });
  const auto *summaryEntry = findSemanticEntry(
      primec::semanticProgramCallableSummaryView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramCallableSummary &entry) {
        return primec::semanticProgramCallableSummaryFullPath(semanticProgram, entry) == "/main";
      });

  REQUIRE(directEntry != nullptr);
  REQUIRE(methodEntry != nullptr);
  REQUIRE(bridgeEntry != nullptr);
  REQUIRE(summaryEntry != nullptr);
  CHECK_FALSE(summaryEntry->isExecution);
  CHECK(summaryEntry->returnKind == "i32");

  CHECK(semanticProgram.bindingFacts.empty());
  CHECK(semanticProgram.returnFacts.empty());
  CHECK(semanticProgram.localAutoFacts.empty());
  CHECK(semanticProgram.queryFacts.empty());
  CHECK(semanticProgram.tryFacts.empty());
  CHECK(semanticProgram.onErrorFacts.empty());

  const auto *mainArtifacts = findModuleArtifacts(semanticProgram, "/");
  REQUIRE(mainArtifacts != nullptr);
  CHECK(anySemanticEntry(
      mainArtifacts->directCallTargetIndices,
      [&](std::size_t index) {
        return index < semanticProgram.directCallTargets.size() &&
               primec::semanticProgramDirectCallTargetResolvedPath(semanticProgram,
                                                                   semanticProgram.directCallTargets[index]) ==
                   "/id_i32";
      }));
  CHECK(anySemanticEntry(
      mainArtifacts->methodCallTargetIndices,
      [&](std::size_t index) {
        return index < semanticProgram.methodCallTargets.size() &&
               primec::semanticProgramMethodCallTargetResolvedPath(semanticProgram,
                                                                   semanticProgram.methodCallTargets[index]) ==
                   "/std/collections/vector/count";
      }));
  CHECK(anySemanticEntry(
      mainArtifacts->callableSummaryIndices,
      [&](std::size_t index) {
        return index < semanticProgram.callableSummaries.size() &&
               primec::semanticProgramCallableSummaryFullPath(semanticProgram,
                                                              semanticProgram.callableSummaries[index]) == "/main";
      }));
  CHECK_FALSE(anySemanticEntry(
      mainArtifacts->callableSummaryIndices,
      [&](std::size_t index) {
        return index < semanticProgram.callableSummaries.size() &&
               primec::semanticProgramCallableSummaryFullPath(semanticProgram,
                                                              semanticProgram.callableSummaries[index])
                   .starts_with("/std/collections/");
      }));

  CHECK(anySemanticEntry(
      semanticProgram.moduleResolvedArtifacts,
      [&](const primec::SemanticProgramModuleResolvedArtifacts &module) {
        return module.identity.moduleKey != "/" &&
               anySemanticEntry(
                   module.callableSummaryIndices,
                   [&](std::size_t index) {
                     return index < semanticProgram.callableSummaries.size() &&
                            primec::semanticProgramCallableSummaryFullPath(
                                semanticProgram, semanticProgram.callableSummaries[index])
                                .starts_with("/std/collections/");
                   });
      }));
  CHECK(anySemanticEntry(
      semanticProgram.moduleResolvedArtifacts,
      [&](const primec::SemanticProgramModuleResolvedArtifacts &module) {
        return anySemanticEntry(
            module.bridgePathChoiceIndices,
            [&](std::size_t index) {
              return index < semanticProgram.bridgePathChoices.size() &&
                     primec::semanticProgramResolveCallTargetString(
                         semanticProgram, semanticProgram.bridgePathChoices[index].chosenPathId) ==
                         "/std/collections/vector/count";
            });
      }));
}

TEST_CASE("semantics validate publishes binding return and import artifacts through public semantic product views") {
  const std::string source = R"(
import /std/math/abs

[return<i32>]
helper([i32] input) {
  [i32] normalized{abs(input)}
  return(normalized)
}

[return<i32>]
main() {
  [i32] value{helper(-3i32)}
  return(value)
}
)";

  const std::vector<std::string> collectorAllowlist = {
      "binding_facts",
      "return_facts",
      "callable_summaries",
  };
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  const std::filesystem::path tempPath = makeTempIrPipelineSourcePath();
  {
    std::ofstream file(tempPath);
    REQUIRE(file.good());
    file << source;
  }

  primec::Options options;
  options.inputPath = tempPath.string();
  options.entryPath = "/main";
  options.defaultEffects = defaults;
  options.entryDefaultEffects = defaults;
  options.benchmarkSemanticFactFamiliesSpecified = true;
  options.benchmarkSemanticFactFamilies = collectorAllowlist;
  applyCompilePipelineSemanticProductIntentForTesting(
      options, CompilePipelineSemanticProductIntentForTesting::Require);
  primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);

  primec::CompilePipelineOutput output;
  primec::CompilePipelineErrorStage errorStage =
      primec::CompilePipelineErrorStage::None;
  std::string error;
  const bool ok = primec::runCompilePipeline(options, output, errorStage, error);
  std::error_code ec;
  std::filesystem::remove(tempPath, ec);

  REQUIRE(ok);
  CHECK(error.empty());
  REQUIRE(output.hasSemanticProgram);
  const primec::SemanticProgram &semanticProgram = output.semanticProgram;

  const auto *helperSummary = findSemanticEntry(
      primec::semanticProgramCallableSummaryView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramCallableSummary &entry) {
        return primec::semanticProgramCallableSummaryFullPath(semanticProgram, entry) ==
               "/helper";
      });
  const auto *mainSummary = findSemanticEntry(
      primec::semanticProgramCallableSummaryView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramCallableSummary &entry) {
        return primec::semanticProgramCallableSummaryFullPath(semanticProgram, entry) ==
               "/main";
      });
  const auto *helperBinding = findSemanticEntry(
      primec::semanticProgramBindingFactView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramBindingFact &entry) {
        return entry.scopePath == "/helper" &&
               entry.siteKind == "local" &&
               entry.name == "normalized" &&
               primec::semanticProgramBindingFactResolvedPath(semanticProgram, entry) ==
                   "/helper/normalized";
      });
  const auto *mainBinding = findSemanticEntry(
      primec::semanticProgramBindingFactView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramBindingFact &entry) {
        return entry.scopePath == "/main" &&
               entry.siteKind == "local" &&
               entry.name == "value" &&
               primec::semanticProgramBindingFactResolvedPath(semanticProgram, entry) ==
                   "/main/value";
      });
  const auto *helperReturn = findSemanticEntry(
      primec::semanticProgramReturnFactView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramReturnFact &entry) {
        return primec::semanticProgramReturnFactDefinitionPath(semanticProgram, entry) ==
               "/helper";
      });
  const auto *mainReturn = findSemanticEntry(
      primec::semanticProgramReturnFactView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramReturnFact &entry) {
        return primec::semanticProgramReturnFactDefinitionPath(semanticProgram, entry) ==
               "/main";
      });
  REQUIRE(helperSummary != nullptr);
  REQUIRE(mainSummary != nullptr);
  REQUIRE(helperBinding != nullptr);
  REQUIRE(mainBinding != nullptr);
  REQUIRE(helperReturn != nullptr);
  REQUIRE(mainReturn != nullptr);
  CHECK(helperSummary->returnKind == "i32");
  CHECK(mainSummary->returnKind == "i32");
  CHECK(helperBinding->bindingTypeText == "i32");
  CHECK(mainBinding->bindingTypeText == "i32");
  CHECK(helperReturn->returnKind == "i32");
  CHECK(mainReturn->returnKind == "i32");
  CHECK(helperReturn->bindingTypeText == "i32");
  CHECK(mainReturn->bindingTypeText == "i32");

  const auto *rootArtifacts = findModuleArtifacts(semanticProgram, "/");
  const auto *absArtifacts =
      findModuleArtifacts(semanticProgram, "/std/math/abs");
  REQUIRE(rootArtifacts != nullptr);
  REQUIRE(absArtifacts != nullptr);

  CHECK(anySemanticEntry(
      rootArtifacts->bindingFactIndices,
      [&](std::size_t index) {
        return index < semanticProgram.bindingFacts.size() &&
               semanticProgram.bindingFacts[index].scopePath == "/helper" &&
               semanticProgram.bindingFacts[index].name == "normalized";
      }));
  CHECK(anySemanticEntry(
      rootArtifacts->bindingFactIndices,
      [&](std::size_t index) {
        return index < semanticProgram.bindingFacts.size() &&
               semanticProgram.bindingFacts[index].scopePath == "/main" &&
               semanticProgram.bindingFacts[index].name == "value";
      }));
  CHECK(anySemanticEntry(
      rootArtifacts->returnFactIndices,
      [&](std::size_t index) {
        return index < semanticProgram.returnFacts.size() &&
               primec::semanticProgramReturnFactDefinitionPath(
                   semanticProgram, semanticProgram.returnFacts[index]) == "/helper";
      }));
  CHECK(anySemanticEntry(
      rootArtifacts->returnFactIndices,
      [&](std::size_t index) {
        return index < semanticProgram.returnFacts.size() &&
               primec::semanticProgramReturnFactDefinitionPath(
                   semanticProgram, semanticProgram.returnFacts[index]) == "/main";
      }));
  CHECK(anySemanticEntry(
      rootArtifacts->callableSummaryIndices,
      [&](std::size_t index) {
        return index < semanticProgram.callableSummaries.size() &&
               primec::semanticProgramCallableSummaryFullPath(
                   semanticProgram, semanticProgram.callableSummaries[index]) == "/helper";
      }));
  CHECK(anySemanticEntry(
      rootArtifacts->callableSummaryIndices,
      [&](std::size_t index) {
        return index < semanticProgram.callableSummaries.size() &&
               primec::semanticProgramCallableSummaryFullPath(
                   semanticProgram, semanticProgram.callableSummaries[index]) == "/main";
      }));
  CHECK(anySemanticEntry(
      absArtifacts->callableSummaryIndices,
      [&](std::size_t index) {
        return index < semanticProgram.callableSummaries.size() &&
               primec::semanticProgramCallableSummaryFullPath(
                   semanticProgram, semanticProgram.callableSummaries[index]) ==
                   "/std/math/abs";
      }));
}

TEST_CASE("semantics validate publishes struct field metadata and parameter binding facts") {
  const std::string source = R"(
Particle {
  [i32] left
  [i64] right
}

[return<i32>]
sum([Particle] particle, [i32] extra) {
  return(plus(particle.left, plus(convert<i32>(particle.right), extra)))
}

[return<i32>]
main() {
  [Particle] particle{Particle(left: 1i32, right: 2i64)}
  return(sum(particle, 3i32))
}
)";

  const std::vector<std::string> collectorAllowlist = {
      "binding_facts",
  };
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  const std::filesystem::path tempPath = makeTempIrPipelineSourcePath();
  {
    std::ofstream file(tempPath);
    REQUIRE(file.good());
    file << source;
  }

  primec::Options options;
  options.inputPath = tempPath.string();
  options.entryPath = "/main";
  options.defaultEffects = defaults;
  options.entryDefaultEffects = defaults;
  options.benchmarkSemanticFactFamiliesSpecified = true;
  options.benchmarkSemanticFactFamilies = collectorAllowlist;
  applyCompilePipelineSemanticProductIntentForTesting(
      options, CompilePipelineSemanticProductIntentForTesting::Require);

  primec::CompilePipelineOutput output;
  primec::CompilePipelineErrorStage errorStage =
      primec::CompilePipelineErrorStage::None;
  std::string error;
  const bool ok = primec::runCompilePipeline(options, output, errorStage, error);
  std::error_code ec;
  std::filesystem::remove(tempPath, ec);

  REQUIRE(ok);
  CHECK(error.empty());
  REQUIRE(output.hasSemanticProgram);
  const primec::SemanticProgram &semanticProgram = output.semanticProgram;

  const auto *particleType =
      primec::semanticProgramLookupTypeMetadata(semanticProgram, "/Particle");
  REQUIRE(particleType != nullptr);
  CHECK(particleType->category == "struct");
  CHECK(particleType->fieldCount == 2);

  const auto particleFields =
      primec::semanticProgramStructFieldMetadataView(semanticProgram, "/Particle");
  REQUIRE(particleFields.size() == 2);
  const auto *leftField = findSemanticEntry(
      particleFields,
      [](const primec::SemanticProgramStructFieldMetadata &entry) {
        return entry.structPath == "/Particle" && entry.fieldName == "left";
      });
  const auto *rightField = findSemanticEntry(
      particleFields,
      [](const primec::SemanticProgramStructFieldMetadata &entry) {
        return entry.structPath == "/Particle" && entry.fieldName == "right";
      });
  REQUIRE(leftField != nullptr);
  REQUIRE(rightField != nullptr);
  CHECK(leftField->fieldIndex == 0);
  CHECK(leftField->bindingTypeText == "i32");
  CHECK(rightField->fieldIndex == 1);
  CHECK(rightField->bindingTypeText == "i64");

  const auto *particleParameter = findSemanticEntry(
      primec::semanticProgramBindingFactView(semanticProgram),
      [](const primec::SemanticProgramBindingFact &entry) {
        return entry.scopePath == "/sum" &&
               entry.siteKind == "parameter" &&
               entry.name == "particle";
      });
  const auto *extraParameter = findSemanticEntry(
      primec::semanticProgramBindingFactView(semanticProgram),
      [](const primec::SemanticProgramBindingFact &entry) {
        return entry.scopePath == "/sum" &&
               entry.siteKind == "parameter" &&
               entry.name == "extra";
      });
  REQUIRE(particleParameter != nullptr);
  REQUIRE(extraParameter != nullptr);
  CHECK((particleParameter->bindingTypeText == "Particle" ||
         particleParameter->bindingTypeText == "/Particle"));
  CHECK(extraParameter->bindingTypeText == "i32");

  const auto *rootArtifacts = findModuleArtifacts(semanticProgram, "/");
  REQUIRE(rootArtifacts != nullptr);
  CHECK(anySemanticEntry(
      rootArtifacts->bindingFactIndices,
      [&](std::size_t index) {
        return index < semanticProgram.bindingFacts.size() &&
               semanticProgram.bindingFacts[index].scopePath == "/sum" &&
               semanticProgram.bindingFacts[index].siteKind == "parameter" &&
               semanticProgram.bindingFacts[index].name == "particle";
      }));
  CHECK(anySemanticEntry(
      rootArtifacts->bindingFactIndices,
      [&](std::size_t index) {
        return index < semanticProgram.bindingFacts.size() &&
               semanticProgram.bindingFacts[index].scopePath == "/sum" &&
               semanticProgram.bindingFacts[index].siteKind == "parameter" &&
               semanticProgram.bindingFacts[index].name == "extra";
      }));
}

TEST_CASE("semantics validate publishes module artifacts in import order") {
  const std::string source = R"(
import /std/math/max
import /std/math/abs

[return<i32>]
main() {
  return(plus(max(3i32, 1i32), abs(-4i32)))
}
)";

  const std::vector<std::string> collectorAllowlist = {
      "callable_summaries",
  };
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  const std::filesystem::path tempPath = makeTempIrPipelineSourcePath();
  {
    std::ofstream file(tempPath);
    REQUIRE(file.good());
    file << source;
  }

  primec::Options options;
  options.inputPath = tempPath.string();
  options.entryPath = "/main";
  options.defaultEffects = defaults;
  options.entryDefaultEffects = defaults;
  options.benchmarkSemanticFactFamiliesSpecified = true;
  options.benchmarkSemanticFactFamilies = collectorAllowlist;
  applyCompilePipelineSemanticProductIntentForTesting(
      options, CompilePipelineSemanticProductIntentForTesting::Require);
  primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);

  primec::CompilePipelineOutput output;
  primec::CompilePipelineErrorStage errorStage =
      primec::CompilePipelineErrorStage::None;
  std::string error;
  const bool ok = primec::runCompilePipeline(options, output, errorStage, error);
  std::error_code ec;
  std::filesystem::remove(tempPath, ec);

  REQUIRE(ok);
  CHECK(error.empty());
  REQUIRE(output.hasSemanticProgram);
  const primec::SemanticProgram &semanticProgram = output.semanticProgram;

  const auto *rootArtifacts = findModuleArtifacts(semanticProgram, "/");
  const auto *maxArtifacts =
      findModuleArtifacts(semanticProgram, "/std/math/max");
  const auto *absArtifacts =
      findModuleArtifacts(semanticProgram, "/std/math/abs");
  REQUIRE(rootArtifacts != nullptr);
  REQUIRE(maxArtifacts != nullptr);
  REQUIRE(absArtifacts != nullptr);

  CHECK(rootArtifacts->identity.stableOrder < maxArtifacts->identity.stableOrder);
  CHECK(maxArtifacts->identity.stableOrder < absArtifacts->identity.stableOrder);

  const auto rootIt = std::find_if(
      semanticProgram.moduleResolvedArtifacts.begin(),
      semanticProgram.moduleResolvedArtifacts.end(),
      [](const primec::SemanticProgramModuleResolvedArtifacts &module) {
        return module.identity.moduleKey == "/";
      });
  const auto maxIt = std::find_if(
      semanticProgram.moduleResolvedArtifacts.begin(),
      semanticProgram.moduleResolvedArtifacts.end(),
      [](const primec::SemanticProgramModuleResolvedArtifacts &module) {
        return module.identity.moduleKey == "/std/math/max";
      });
  const auto absIt = std::find_if(
      semanticProgram.moduleResolvedArtifacts.begin(),
      semanticProgram.moduleResolvedArtifacts.end(),
      [](const primec::SemanticProgramModuleResolvedArtifacts &module) {
        return module.identity.moduleKey == "/std/math/abs";
      });
  REQUIRE(rootIt != semanticProgram.moduleResolvedArtifacts.end());
  REQUIRE(maxIt != semanticProgram.moduleResolvedArtifacts.end());
  REQUIRE(absIt != semanticProgram.moduleResolvedArtifacts.end());
  CHECK(rootIt < maxIt);
  CHECK(maxIt < absIt);

  CHECK(anySemanticEntry(
      rootArtifacts->callableSummaryIndices,
      [&](std::size_t index) {
        return index < semanticProgram.callableSummaries.size() &&
               primec::semanticProgramCallableSummaryFullPath(
                   semanticProgram, semanticProgram.callableSummaries[index]) ==
                   "/main";
      }));
  CHECK(anySemanticEntry(
      maxArtifacts->callableSummaryIndices,
      [&](std::size_t index) {
        return index < semanticProgram.callableSummaries.size() &&
               primec::semanticProgramCallableSummaryFullPath(
                   semanticProgram, semanticProgram.callableSummaries[index]) ==
                   "/std/math/max";
      }));
  CHECK(anySemanticEntry(
      absArtifacts->callableSummaryIndices,
      [&](std::size_t index) {
        return index < semanticProgram.callableSummaries.size() &&
               primec::semanticProgramCallableSummaryFullPath(
                   semanticProgram, semanticProgram.callableSummaries[index]) ==
                   "/std/math/abs";
      }));
}

TEST_SUITE_END();
