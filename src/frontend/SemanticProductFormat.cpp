#include "primec/frontend/SemanticProduct.h"

#include "primec/support/CompileArena.h"

#include <algorithm>
#include <limits>
#include <sstream>
#include <string_view>
#include "SemanticProductInternal.h"

namespace primec {
using namespace semantic_product_detail;

std::string formatSemanticProgram(const SemanticProgram &semanticProgram) {
  std::ostringstream out;
  out << "semantic_product {\n";
  appendSemanticHeaderLine(out, "entry_path", quoteSemanticString(semanticProgram.entryPath));
  for (size_t i = 0; i < semanticProgram.sourceImports.size(); ++i) {
    appendSemanticIndexedLine(out, "source_imports", i, quoteSemanticString(semanticProgram.sourceImports[i]));
  }
  for (size_t i = 0; i < semanticProgram.imports.size(); ++i) {
    appendSemanticIndexedLine(out, "imports", i, quoteSemanticString(semanticProgram.imports[i]));
  }
  for (size_t i = 0; i < semanticProgram.definitions.size(); ++i) {
    const auto &entry = semanticProgram.definitions[i];
    std::string definitionText =
        "full_path=" + quoteSemanticString(entry.fullPath) + " name=" +
        quoteSemanticString(entry.name) + " namespace_prefix=" +
        quoteSemanticString(entry.namespacePrefix);
    if (!entry.templateParameters.empty()) {
      definitionText += " template_params=" +
                        formatSemanticTemplateParameterList(entry.templateParameters,
                                                            entry.templateParameterIsPack);
    }
    if (!entry.templatePackBindings.empty()) {
      definitionText += " template_pack_bindings=" +
                        formatSemanticTemplatePackBindingList(entry.templatePackBindings);
    }
    definitionText += " provenance_handle=" + std::to_string(entry.provenanceHandle) +
                      " source=" +
                      quoteSemanticString(formatSemanticSourceLocation(entry.sourceLine,
                                                                       entry.sourceColumn));
    appendSemanticIndexedLine(out,
                              "definitions",
                              i,
                              definitionText);
  }
  for (size_t i = 0; i < semanticProgram.executions.size(); ++i) {
    const auto &entry = semanticProgram.executions[i];
    appendSemanticIndexedLine(out,
                              "executions",
                              i,
                              "full_path=" + quoteSemanticString(entry.fullPath) + " name=" +
                                  quoteSemanticString(entry.name) + " namespace_prefix=" +
                                  quoteSemanticString(entry.namespacePrefix) + " provenance_handle=" +
                                  std::to_string(entry.provenanceHandle) + " source=" +
                                  quoteSemanticString(formatSemanticSourceLocation(entry.sourceLine, entry.sourceColumn)));
  }
  const auto directCallTargets = semanticProgramDirectCallTargetView(semanticProgram);
  for (size_t i = 0; i < directCallTargets.size(); ++i) {
    const auto &entry = *directCallTargets[i];
    const std::string_view scopePath =
        semanticProgramResolveCallTargetString(semanticProgram, entry.scopePathId);
    const std::string_view callName =
        semanticProgramResolveCallTargetString(semanticProgram, entry.callNameId);
    const std::string_view resolvedPath =
        semanticProgramResolveCallTargetString(semanticProgram, entry.resolvedPathId);
    appendSemanticIndexedLine(out,
                              "direct_call_targets",
                              i,
                              "scope_path=" +
                                  quoteSemanticString(scopePath.empty() ? entry.scopePath : scopePath) +
                                  " call_name=" +
                                  quoteSemanticString(callName.empty() ? entry.callName : callName) +
                                  " resolved_path=" +
                                  quoteSemanticString(resolvedPath) +
                                  (entry.stdlibSurfaceId.has_value()
                                       ? " stdlib_surface_id=" +
                                             quoteSemanticString(
                                                 formatSemanticStdlibSurfaceId(*entry.stdlibSurfaceId))
                                       : "") +
                                  " provenance_handle=" +
                                  std::to_string(entry.provenanceHandle) + " source=" +
                                  quoteSemanticString(formatSemanticSourceLocation(entry.sourceLine, entry.sourceColumn)));
  }
  const auto methodCallTargets = semanticProgramMethodCallTargetView(semanticProgram);
  for (size_t i = 0; i < methodCallTargets.size(); ++i) {
    const auto &entry = *methodCallTargets[i];
    const std::string_view scopePath =
        semanticProgramResolveCallTargetString(semanticProgram, entry.scopePathId);
    const std::string_view methodName =
        semanticProgramResolveCallTargetString(semanticProgram, entry.methodNameId);
    const std::string_view receiverTypeText =
        semanticProgramResolveCallTargetString(semanticProgram, entry.receiverTypeTextId);
    const std::string_view resolvedPath =
        semanticProgramMethodCallTargetResolvedPath(semanticProgram, entry);
    appendSemanticIndexedLine(out,
                              "method_call_targets",
                              i,
                              "scope_path=" +
                                  quoteSemanticString(scopePath.empty() ? entry.scopePath : scopePath) +
                                  " method_name=" +
                                  quoteSemanticString(methodName.empty() ? entry.methodName : methodName) +
                                  " receiver_type_text=" +
                                  quoteSemanticString(receiverTypeText.empty()
                                                          ? entry.receiverTypeText
                                                          : receiverTypeText) +
                                  " resolved_path=" +
                                  quoteSemanticString(resolvedPath) +
                                  (entry.stdlibSurfaceId.has_value()
                                       ? " stdlib_surface_id=" +
                                             quoteSemanticString(
                                                 formatSemanticStdlibSurfaceId(*entry.stdlibSurfaceId))
                                       : "") +
                                  " provenance_handle=" +
                                  std::to_string(entry.provenanceHandle) + " source=" +
                                  quoteSemanticString(formatSemanticSourceLocation(entry.sourceLine, entry.sourceColumn)));
  }
  const auto bridgePathChoices = semanticProgramBridgePathChoiceView(semanticProgram);
  for (size_t i = 0; i < bridgePathChoices.size(); ++i) {
    const auto &entry = *bridgePathChoices[i];
    const std::string_view scopePath =
        semanticProgramResolveCallTargetString(semanticProgram, entry.scopePathId);
    const std::string_view collectionFamily =
        semanticProgramResolveCallTargetString(semanticProgram, entry.collectionFamilyId);
    const std::string_view helperName =
        semanticProgramBridgePathChoiceHelperName(semanticProgram, entry);
    const std::string_view chosenPath =
        semanticProgramResolveCallTargetString(semanticProgram, entry.chosenPathId);
    appendSemanticIndexedLine(out,
                              "bridge_path_choices",
                              i,
                              "scope_path=" +
                                  quoteSemanticString(scopePath.empty() ? entry.scopePath : scopePath) +
                                  " collection_family=" +
                                  quoteSemanticString(collectionFamily.empty()
                                                          ? entry.collectionFamily
                                                          : collectionFamily) +
                                  " helper_name=" +
                                  quoteSemanticString(helperName) +
                                  " chosen_path=" +
                                  quoteSemanticString(chosenPath) +
                                  (entry.stdlibSurfaceId.has_value()
                                       ? " stdlib_surface_id=" +
                                             quoteSemanticString(
                                                 formatSemanticStdlibSurfaceId(*entry.stdlibSurfaceId))
                                       : "") +
                                  " provenance_handle=" +
                                  std::to_string(entry.provenanceHandle) + " source=" +
                                  quoteSemanticString(formatSemanticSourceLocation(entry.sourceLine, entry.sourceColumn)));
  }
  const auto callableSummaries = semanticProgramCallableSummaryView(semanticProgram);
  for (size_t i = 0; i < callableSummaries.size(); ++i) {
    const auto &entry = *callableSummaries[i];
    const std::string_view fullPath =
        semanticProgramCallableSummaryFullPath(semanticProgram, entry);
    const std::string_view returnKind =
        semanticProgramResolveCallTargetString(semanticProgram, entry.returnKindId);
    const std::string formattedActiveEffects =
        formatSemanticStringListFromIds(semanticProgram, entry.activeEffectIds, entry.activeEffects);
    const std::string formattedActiveCapabilities =
        formatSemanticStringListFromIds(semanticProgram, entry.activeCapabilityIds, entry.activeCapabilities);
    const std::string_view resultValueType =
        semanticProgramResolveCallTargetString(semanticProgram, entry.resultValueTypeId);
    const std::string_view resultErrorType =
        semanticProgramResolveCallTargetString(semanticProgram, entry.resultErrorTypeId);
    const std::string_view onErrorHandlerPath =
        semanticProgramResolveCallTargetString(semanticProgram, entry.onErrorHandlerPathId);
    const std::string_view onErrorErrorType =
        semanticProgramResolveCallTargetString(semanticProgram, entry.onErrorErrorTypeId);
    appendSemanticIndexedLine(out,
                              "callable_summaries",
                              i,
                              "full_path=" +
                                  quoteSemanticString(fullPath) +
                                  " is_execution=" +
                                  formatSemanticBool(entry.isExecution) + " return_kind=" +
                                  quoteSemanticString(returnKind.empty() ? entry.returnKind : returnKind) +
                                  " is_compute=" +
                                  formatSemanticBool(entry.isCompute) + " is_unsafe=" +
                                  formatSemanticBool(entry.isUnsafe) + " active_effects=" +
                                  formattedActiveEffects + " active_capabilities=" +
                                  formattedActiveCapabilities + " has_result_type=" +
                                  formatSemanticBool(entry.hasResultType) + " result_type_has_value=" +
                                  formatSemanticBool(entry.resultTypeHasValue) + " result_value_type=" +
                                  quoteSemanticString(resultValueType.empty()
                                                          ? entry.resultValueType
                                                          : resultValueType) +
                                  " result_error_type=" +
                                  quoteSemanticString(resultErrorType.empty()
                                                          ? entry.resultErrorType
                                                          : resultErrorType) +
                                  " has_on_error=" +
                                  formatSemanticBool(entry.hasOnError) + " on_error_handler_path=" +
                                  quoteSemanticString(onErrorHandlerPath.empty()
                                                          ? entry.onErrorHandlerPath
                                                          : onErrorHandlerPath) +
                                  " on_error_error_type=" +
                                  quoteSemanticString(onErrorErrorType.empty()
                                                          ? entry.onErrorErrorType
                                                          : onErrorErrorType) +
                                  " on_error_bound_arg_count=" +
                                  std::to_string(entry.onErrorBoundArgCount) + " provenance_handle=" +
                                  std::to_string(entry.provenanceHandle));
  }
  for (size_t i = 0; i < semanticProgram.typeMetadata.size(); ++i) {
    const auto &entry = semanticProgram.typeMetadata[i];
    appendSemanticIndexedLine(out,
                              "type_metadata",
                              i,
                              "full_path=" + quoteSemanticString(entry.fullPath) + " category=" +
                                  quoteSemanticString(entry.category) + " is_public=" +
                                  formatSemanticBool(entry.isPublic) + " has_no_padding=" +
                                  formatSemanticBool(entry.hasNoPadding) + " has_platform_independent_padding=" +
                                  formatSemanticBool(entry.hasPlatformIndependentPadding) +
                                  " has_explicit_alignment=" + formatSemanticBool(entry.hasExplicitAlignment) +
                                  " explicit_alignment_bytes=" + std::to_string(entry.explicitAlignmentBytes) +
                                  " field_count=" + std::to_string(entry.fieldCount) + " enum_value_count=" +
                                  std::to_string(entry.enumValueCount) + " provenance_handle=" +
                                  std::to_string(entry.provenanceHandle) + " source=" +
                                  quoteSemanticString(formatSemanticSourceLocation(entry.sourceLine, entry.sourceColumn)));
  }
  for (size_t i = 0; i < semanticProgram.structFieldMetadata.size(); ++i) {
    const auto &entry = semanticProgram.structFieldMetadata[i];
    appendSemanticIndexedLine(out,
                              "struct_field_metadata",
                              i,
                              "struct_path=" + quoteSemanticString(entry.structPath) + " field_name=" +
                                  quoteSemanticString(entry.fieldName) + " field_index=" +
                                  std::to_string(entry.fieldIndex) + " binding_type_text=" +
                                  quoteSemanticString(entry.bindingTypeText) + " provenance_handle=" +
                                  std::to_string(entry.provenanceHandle) + " source=" +
                                  quoteSemanticString(formatSemanticSourceLocation(entry.sourceLine, entry.sourceColumn)));
  }
  for (size_t i = 0; i < semanticProgram.sumTypeMetadata.size(); ++i) {
    const auto &entry = semanticProgram.sumTypeMetadata[i];
    appendSemanticIndexedLine(out,
                              "sum_type_metadata",
                              i,
                              "full_path=" + quoteSemanticString(entry.fullPath) + " is_public=" +
                                  formatSemanticBool(entry.isPublic) + " active_tag_type_text=" +
                                  quoteSemanticString(entry.activeTagTypeText) + " payload_storage_text=" +
                                  quoteSemanticString(entry.payloadStorageText) + " variant_count=" +
                                  std::to_string(entry.variantCount) + " provenance_handle=" +
                                  std::to_string(entry.provenanceHandle) + " source=" +
                                  quoteSemanticString(formatSemanticSourceLocation(entry.sourceLine, entry.sourceColumn)));
  }
  for (size_t i = 0; i < semanticProgram.sumVariantMetadata.size(); ++i) {
    const auto &entry = semanticProgram.sumVariantMetadata[i];
    appendSemanticIndexedLine(out,
                              "sum_variant_metadata",
                              i,
                              "sum_path=" + quoteSemanticString(entry.sumPath) + " variant_name=" +
                                  quoteSemanticString(entry.variantName) + " variant_index=" +
                                  std::to_string(entry.variantIndex) + " tag_value=" +
                                  std::to_string(entry.tagValue) + " payload_type_text=" +
                                  quoteSemanticString(entry.payloadTypeText) + " has_payload=" +
                                  formatSemanticBool(entry.hasPayload) + " provenance_handle=" +
                                  std::to_string(entry.provenanceHandle) + " source=" +
                                  quoteSemanticString(formatSemanticSourceLocation(entry.sourceLine, entry.sourceColumn)));
  }
  const auto collectionSpecializations = semanticProgramCollectionSpecializationView(semanticProgram);
  for (size_t i = 0; i < collectionSpecializations.size(); ++i) {
    const auto &entry = *collectionSpecializations[i];
    const auto specializationText = [&](SymbolId id, const std::string &fallback) -> std::string_view {
      const std::string_view resolved = semanticProgramResolveCallTargetString(semanticProgram, id);
      return resolved.empty() ? std::string_view(fallback) : resolved;
    };
    appendSemanticIndexedLine(out,
                              "collection_specializations",
                              i,
                              "scope_path=" +
                                  quoteSemanticString(specializationText(entry.scopePathId, entry.scopePath)) +
                                  " site_kind=" +
                                  quoteSemanticString(specializationText(entry.siteKindId, entry.siteKind)) +
                                  " name=" +
                                  quoteSemanticString(specializationText(entry.nameId, entry.name)) +
                                  " collection_family=" +
                                  quoteSemanticString(specializationText(entry.collectionFamilyId,
                                                                         entry.collectionFamily)) +
                                  " binding_type_text=" +
                                  quoteSemanticString(specializationText(entry.bindingTypeTextId,
                                                                         entry.bindingTypeText)) +
                                  " element_type_text=" +
                                  quoteSemanticString(specializationText(entry.elementTypeTextId,
                                                                         entry.elementTypeText)) +
                                  " key_type_text=" +
                                  quoteSemanticString(specializationText(entry.keyTypeTextId,
                                                                         entry.keyTypeText)) +
                                  " value_type_text=" +
                                  quoteSemanticString(specializationText(entry.valueTypeTextId,
                                                                         entry.valueTypeText)) +
                                  (specializationText(entry.structPathId, entry.structPath).empty()
                                       ? ""
                                       : " struct_path=" +
                                             quoteSemanticString(specializationText(entry.structPathId,
                                                                                    entry.structPath))) +
                                  " is_reference=" +
                                  formatSemanticBool(entry.isReference) + " is_pointer=" +
                                  formatSemanticBool(entry.isPointer) +
                                  (entry.helperSurfaceId.has_value()
                                       ? " helper_surface_id=" +
                                             quoteSemanticString(
                                                 formatSemanticStdlibSurfaceId(*entry.helperSurfaceId))
                                       : "") +
                                  (entry.constructorSurfaceId.has_value()
                                       ? " constructor_surface_id=" +
                                             quoteSemanticString(formatSemanticStdlibSurfaceId(
                                                 *entry.constructorSurfaceId))
                                       : "") +
                                  " provenance_handle=" +
                                  std::to_string(entry.provenanceHandle) + " source=" +
                                  quoteSemanticString(formatSemanticSourceLocation(entry.sourceLine, entry.sourceColumn)));
  }
  const auto arrayExtentFacts = semanticProgramArrayExtentFactView(semanticProgram);
  for (size_t i = 0; i < arrayExtentFacts.size(); ++i) {
    const auto &entry = *arrayExtentFacts[i];
    const auto extentText = [&](SymbolId id, const std::string &fallback) -> std::string_view {
      const std::string_view resolved = semanticProgramResolveCallTargetString(semanticProgram, id);
      return resolved.empty() ? std::string_view(fallback) : resolved;
    };
    appendSemanticIndexedLine(out,
                              "array_extent_facts",
                              i,
                              "scope_path=" +
                                  quoteSemanticString(extentText(entry.scopePathId, entry.scopePath)) +
                                  " site_kind=" +
                                  quoteSemanticString(extentText(entry.siteKindId, entry.siteKind)) +
                                  " target_name=" +
                                  quoteSemanticString(extentText(entry.targetNameId, entry.targetName)) +
                                  " target_resolved_path=" +
                                  quoteSemanticString(extentText(entry.targetResolvedPathId,
                                                                 entry.targetResolvedPath)) +
                                  " binding_type_text=" +
                                  quoteSemanticString(extentText(entry.bindingTypeTextId,
                                                                 entry.bindingTypeText)) +
                                  " element_type_text=" +
                                  quoteSemanticString(extentText(entry.elementTypeTextId,
                                                                 entry.elementTypeText)) +
                                  " extent_expression=" +
                                  quoteSemanticString(extentText(entry.extentExpressionId,
                                                                 entry.extentExpression)) +
                                  " is_reference=" +
                                  formatSemanticBool(entry.isReference) + " has_static_extent=" +
                                  formatSemanticBool(entry.hasStaticExtent) + " static_extent=" +
                                  std::to_string(entry.staticExtent) +
                                  " target_semantic_node_id=" +
                                  std::to_string(entry.targetSemanticNodeId) +
                                  " provenance_handle=" +
                                  std::to_string(entry.provenanceHandle) + " source=" +
                                  quoteSemanticString(formatSemanticSourceLocation(entry.sourceLine,
                                                                                   entry.sourceColumn)));
  }
  const auto bindingFacts = semanticProgramBindingFactView(semanticProgram);
  for (size_t i = 0; i < bindingFacts.size(); ++i) {
    const auto &entry = *bindingFacts[i];
    const std::string_view scopePath =
        semanticProgramResolveCallTargetString(semanticProgram, entry.scopePathId);
    const std::string_view siteKind =
        semanticProgramResolveCallTargetString(semanticProgram, entry.siteKindId);
    const std::string_view name =
        semanticProgramResolveCallTargetString(semanticProgram, entry.nameId);
    const std::string_view resolvedPath =
        semanticProgramResolveCallTargetString(semanticProgram, entry.resolvedPathId);
    const std::string_view bindingTypeText =
        semanticProgramResolveCallTargetString(semanticProgram, entry.bindingTypeTextId);
    const std::string_view referenceRoot =
        semanticProgramResolveCallTargetString(semanticProgram, entry.referenceRootId);
    appendSemanticIndexedLine(out,
                              "binding_facts",
                              i,
                              "scope_path=" +
                                  quoteSemanticString(scopePath.empty() ? entry.scopePath : scopePath) +
                                  " site_kind=" +
                                  quoteSemanticString(siteKind.empty() ? entry.siteKind : siteKind) +
                                  " name=" + quoteSemanticString(name.empty() ? entry.name : name) +
                                  " resolved_path=" +
                                  quoteSemanticString(resolvedPath) +
                                  " binding_type_text=" +
                                  quoteSemanticString(bindingTypeText.empty()
                                                          ? entry.bindingTypeText
                                                          : bindingTypeText) +
                                  " is_mutable=" +
                                  formatSemanticBool(entry.isMutable) + " is_entry_arg_string=" +
                                  formatSemanticBool(entry.isEntryArgString) + " is_unsafe_reference=" +
                                  formatSemanticBool(entry.isUnsafeReference) + " reference_root=" +
                                  quoteSemanticString(referenceRoot.empty() ? entry.referenceRoot : referenceRoot) +
                                  " provenance_handle=" +
                                  std::to_string(entry.provenanceHandle) + " source=" +
                                  quoteSemanticString(formatSemanticSourceLocation(entry.sourceLine, entry.sourceColumn)));
  }
  const auto returnFacts = semanticProgramReturnFactView(semanticProgram);
  for (size_t i = 0; i < returnFacts.size(); ++i) {
    const auto &entry = *returnFacts[i];
    const std::string_view definitionPath =
        semanticProgramReturnFactDefinitionPath(semanticProgram, entry);
    const std::string_view returnKind =
        semanticProgramResolveCallTargetString(semanticProgram, entry.returnKindId);
    const std::string_view structPath =
        semanticProgramResolveCallTargetString(semanticProgram, entry.structPathId);
    const std::string_view bindingTypeText =
        semanticProgramResolveCallTargetString(semanticProgram, entry.bindingTypeTextId);
    const std::string_view referenceRoot =
        semanticProgramResolveCallTargetString(semanticProgram, entry.referenceRootId);
    appendSemanticIndexedLine(out,
                              "return_facts",
                              i,
                              "definition_path=" +
                                  quoteSemanticString(definitionPath) +
                                  " return_kind=" +
                                  quoteSemanticString(returnKind.empty() ? entry.returnKind : returnKind) +
                                  " struct_path=" +
                                  quoteSemanticString(structPath.empty() ? entry.structPath : structPath) +
                                  " binding_type_text=" +
                                  quoteSemanticString(bindingTypeText.empty() ? entry.bindingTypeText
                                                                              : bindingTypeText) +
                                  " is_mutable=" +
                                  formatSemanticBool(entry.isMutable) + " is_entry_arg_string=" +
                                  formatSemanticBool(entry.isEntryArgString) + " is_unsafe_reference=" +
                                  formatSemanticBool(entry.isUnsafeReference) + " reference_root=" +
                                  quoteSemanticString(referenceRoot.empty() ? entry.referenceRoot
                                                                            : referenceRoot) +
                                  " provenance_handle=" +
                                  std::to_string(entry.provenanceHandle) + " source=" +
                                  quoteSemanticString(formatSemanticSourceLocation(entry.sourceLine, entry.sourceColumn)));
  }
  const auto localAutoFacts = semanticProgramLocalAutoFactView(semanticProgram);
  for (size_t i = 0; i < localAutoFacts.size(); ++i) {
    const auto &entry = *localAutoFacts[i];
    const auto localAutoText = [&](SymbolId id, const std::string &fallback) -> std::string_view {
      const std::string_view resolved = semanticProgramResolveCallTargetString(semanticProgram, id);
      return resolved.empty() ? std::string_view(fallback) : resolved;
    };
    const std::string initializerStdlibSurfaceText =
        entry.initializerStdlibSurfaceId.has_value()
            ? " initializer_stdlib_surface_id=" +
                  quoteSemanticString(
                      formatSemanticStdlibSurfaceId(*entry.initializerStdlibSurfaceId))
            : std::string{};
    const std::string initializerDirectCallStdlibSurfaceText =
        entry.initializerDirectCallStdlibSurfaceId.has_value()
            ? " initializer_direct_call_stdlib_surface_id=" +
                  quoteSemanticString(formatSemanticStdlibSurfaceId(
                      *entry.initializerDirectCallStdlibSurfaceId))
            : std::string{};
    const std::string initializerMethodCallStdlibSurfaceText =
        entry.initializerMethodCallStdlibSurfaceId.has_value()
            ? " initializer_method_call_stdlib_surface_id=" +
                  quoteSemanticString(formatSemanticStdlibSurfaceId(
                      *entry.initializerMethodCallStdlibSurfaceId))
            : std::string{};
    appendSemanticIndexedLine(out,
                              "local_auto_facts",
                              i,
                              "scope_path=" +
                                  quoteSemanticString(localAutoText(entry.scopePathId, entry.scopePath)) +
                                  " binding_name=" +
                                  quoteSemanticString(localAutoText(entry.bindingNameId, entry.bindingName)) +
                                  " binding_type_text=" +
                                  quoteSemanticString(localAutoText(entry.bindingTypeTextId,
                                                                    entry.bindingTypeText)) +
                                  " initializer_resolved_path=" +
                                  quoteSemanticString(
                                      semanticProgramLocalAutoFactInitializerResolvedPath(
                                          semanticProgram, entry)) +
                                  initializerStdlibSurfaceText +
                                  " initializer_binding_type_text=" +
                                  quoteSemanticString(localAutoText(entry.initializerBindingTypeTextId,
                                                                    entry.initializerBindingTypeText)) +
                                  " initializer_receiver_binding_type_text=" +
                                  quoteSemanticString(
                                      localAutoText(entry.initializerReceiverBindingTypeTextId,
                                                    entry.initializerReceiverBindingTypeText)) +
                                  " initializer_query_type_text=" +
                                  quoteSemanticString(localAutoText(entry.initializerQueryTypeTextId,
                                                                    entry.initializerQueryTypeText)) +
                                  " initializer_result_has_value=" +
                                  formatSemanticBool(entry.initializerResultHasValue) +
                                  " initializer_result_value_type=" +
                                  quoteSemanticString(localAutoText(entry.initializerResultValueTypeId,
                                                                    entry.initializerResultValueType)) +
                                  " initializer_result_error_type=" +
                                  quoteSemanticString(localAutoText(entry.initializerResultErrorTypeId,
                                                                    entry.initializerResultErrorType)) +
                                  " initializer_has_try=" + formatSemanticBool(entry.initializerHasTry) +
                                  " initializer_try_operand_resolved_path=" +
                                  quoteSemanticString(localAutoText(
                                      entry.initializerTryOperandResolvedPathId,
                                      entry.initializerTryOperandResolvedPath)) +
                                  " initializer_try_operand_binding_type_text=" +
                                  quoteSemanticString(
                                      localAutoText(entry.initializerTryOperandBindingTypeTextId,
                                                    entry.initializerTryOperandBindingTypeText)) +
                                  " initializer_try_operand_receiver_binding_type_text=" +
                                  quoteSemanticString(localAutoText(
                                      entry.initializerTryOperandReceiverBindingTypeTextId,
                                      entry.initializerTryOperandReceiverBindingTypeText)) +
                                  " initializer_try_operand_query_type_text=" +
                                  quoteSemanticString(localAutoText(
                                      entry.initializerTryOperandQueryTypeTextId,
                                      entry.initializerTryOperandQueryTypeText)) +
                                  " initializer_try_value_type=" +
                                  quoteSemanticString(localAutoText(entry.initializerTryValueTypeId,
                                                                    entry.initializerTryValueType)) +
                                  " initializer_try_error_type=" +
                                  quoteSemanticString(localAutoText(entry.initializerTryErrorTypeId,
                                                                    entry.initializerTryErrorType)) +
                                  " initializer_try_context_return_kind=" +
                                  quoteSemanticString(
                                      localAutoText(entry.initializerTryContextReturnKindId,
                                                    entry.initializerTryContextReturnKind)) +
                                  " initializer_try_on_error_handler_path=" +
                                  quoteSemanticString(localAutoText(
                                      entry.initializerTryOnErrorHandlerPathId,
                                      entry.initializerTryOnErrorHandlerPath)) +
                                  " initializer_try_on_error_error_type=" +
                                  quoteSemanticString(localAutoText(entry.initializerTryOnErrorErrorTypeId,
                                                                    entry.initializerTryOnErrorErrorType)) +
                                  " initializer_try_on_error_bound_arg_count=" +
                                  std::to_string(entry.initializerTryOnErrorBoundArgCount) +
                                  " initializer_direct_call_resolved_path=" +
                                  quoteSemanticString(localAutoText(
                                      entry.initializerDirectCallResolvedPathId,
                                      entry.initializerDirectCallResolvedPath)) +
                                  initializerDirectCallStdlibSurfaceText +
                                  " initializer_direct_call_return_kind=" +
                                  quoteSemanticString(localAutoText(entry.initializerDirectCallReturnKindId,
                                                                    entry.initializerDirectCallReturnKind)) +
                                  " initializer_method_call_resolved_path=" +
                                  quoteSemanticString(localAutoText(
                                      entry.initializerMethodCallResolvedPathId,
                                      entry.initializerMethodCallResolvedPath)) +
                                  initializerMethodCallStdlibSurfaceText +
                                  " initializer_method_call_return_kind=" +
                                  quoteSemanticString(localAutoText(entry.initializerMethodCallReturnKindId,
                                                                    entry.initializerMethodCallReturnKind)) +
                                  " provenance_handle=" + std::to_string(entry.provenanceHandle) + " source=" +
                                  quoteSemanticString(formatSemanticSourceLocation(entry.sourceLine, entry.sourceColumn)));
  }
  const auto queryFacts = semanticProgramQueryFactView(semanticProgram);
  for (size_t i = 0; i < queryFacts.size(); ++i) {
    const auto &entry = *queryFacts[i];
    const std::string_view scopePath =
        semanticProgramResolveCallTargetString(semanticProgram, entry.scopePathId);
    const std::string_view callName =
        semanticProgramResolveCallTargetString(semanticProgram, entry.callNameId);
    const std::string_view resolvedPath = semanticProgramQueryFactResolvedPath(semanticProgram, entry);
    const std::string_view queryTypeText =
        semanticProgramResolveCallTargetString(semanticProgram, entry.queryTypeTextId);
    const std::string_view bindingTypeText =
        semanticProgramResolveCallTargetString(semanticProgram, entry.bindingTypeTextId);
    const std::string_view receiverBindingTypeText =
        semanticProgramResolveCallTargetString(semanticProgram, entry.receiverBindingTypeTextId);
    const std::string_view resultValueType =
        semanticProgramResolveCallTargetString(semanticProgram, entry.resultValueTypeId);
    const std::string_view resultErrorType =
        semanticProgramResolveCallTargetString(semanticProgram, entry.resultErrorTypeId);
    appendSemanticIndexedLine(out,
                              "query_facts",
                              i,
                              "scope_path=" +
                                  quoteSemanticString(scopePath.empty() ? entry.scopePath : scopePath) +
                                  " call_name=" +
                                  quoteSemanticString(callName.empty() ? entry.callName : callName) +
                                  " resolved_path=" +
                                  quoteSemanticString(resolvedPath) +
                                  " query_type_text=" +
                                  quoteSemanticString(queryTypeText.empty() ? entry.queryTypeText : queryTypeText) +
                                  " binding_type_text=" +
                                  quoteSemanticString(bindingTypeText.empty() ? entry.bindingTypeText
                                                                            : bindingTypeText) +
                                  " receiver_binding_type_text=" +
                                  quoteSemanticString(receiverBindingTypeText.empty()
                                                        ? entry.receiverBindingTypeText
                                                        : receiverBindingTypeText) +
                                  " has_result_type=" +
                                  formatSemanticBool(entry.hasResultType) + " result_type_has_value=" +
                                  formatSemanticBool(entry.resultTypeHasValue) + " result_value_type=" +
                                  quoteSemanticString(resultValueType.empty() ? entry.resultValueType
                                                                              : resultValueType) +
                                  " result_error_type=" +
                                  quoteSemanticString(resultErrorType.empty() ? entry.resultErrorType
                                                                              : resultErrorType) +
                                  " provenance_handle=" +
                                  std::to_string(entry.provenanceHandle) + " source=" +
                                  quoteSemanticString(formatSemanticSourceLocation(entry.sourceLine, entry.sourceColumn)));
  }
  const auto tryFacts = semanticProgramTryFactView(semanticProgram);
  for (size_t i = 0; i < tryFacts.size(); ++i) {
    const auto &entry = *tryFacts[i];
    const std::string_view scopePath =
        semanticProgramResolveCallTargetString(semanticProgram, entry.scopePathId);
    const std::string_view operandResolvedPath =
        semanticProgramTryFactOperandResolvedPath(semanticProgram, entry);
    const std::string_view operandBindingTypeText =
        semanticProgramResolveCallTargetString(semanticProgram, entry.operandBindingTypeTextId);
    const std::string_view operandReceiverBindingTypeText = semanticProgramResolveCallTargetString(
        semanticProgram, entry.operandReceiverBindingTypeTextId);
    const std::string_view operandQueryTypeText =
        semanticProgramResolveCallTargetString(semanticProgram, entry.operandQueryTypeTextId);
    const std::string_view valueType =
        semanticProgramResolveCallTargetString(semanticProgram, entry.valueTypeId);
    const std::string_view errorType =
        semanticProgramResolveCallTargetString(semanticProgram, entry.errorTypeId);
    const std::string_view contextReturnKind =
        semanticProgramResolveCallTargetString(semanticProgram, entry.contextReturnKindId);
    const std::string_view onErrorHandlerPath =
        semanticProgramResolveCallTargetString(semanticProgram, entry.onErrorHandlerPathId);
    const std::string_view onErrorErrorType =
        semanticProgramResolveCallTargetString(semanticProgram, entry.onErrorErrorTypeId);
    appendSemanticIndexedLine(out,
                              "try_facts",
                              i,
                                  "scope_path=" +
                                  quoteSemanticString(scopePath.empty() ? entry.scopePath : scopePath) +
                                  " operand_resolved_path=" +
                                  quoteSemanticString(operandResolvedPath) +
                                  " operand_binding_type_text=" +
                                  quoteSemanticString(operandBindingTypeText.empty()
                                                          ? entry.operandBindingTypeText
                                                          : operandBindingTypeText) +
                                  " operand_receiver_binding_type_text=" +
                                  quoteSemanticString(operandReceiverBindingTypeText.empty()
                                                          ? entry.operandReceiverBindingTypeText
                                                          : operandReceiverBindingTypeText) +
                                  " operand_query_type_text=" +
                                  quoteSemanticString(operandQueryTypeText.empty()
                                                          ? entry.operandQueryTypeText
                                                          : operandQueryTypeText) +
                                  " value_type=" +
                                  quoteSemanticString(valueType.empty() ? entry.valueType : valueType) +
                                  " error_type=" +
                                  quoteSemanticString(errorType.empty() ? entry.errorType : errorType) +
                                  " context_return_kind=" +
                                  quoteSemanticString(contextReturnKind.empty() ? entry.contextReturnKind
                                                                                : contextReturnKind) +
                                  " on_error_handler_path=" +
                                  quoteSemanticString(onErrorHandlerPath.empty() ? entry.onErrorHandlerPath
                                                                                 : onErrorHandlerPath) +
                                  " on_error_error_type=" +
                                  quoteSemanticString(onErrorErrorType.empty() ? entry.onErrorErrorType
                                                                               : onErrorErrorType) +
                                  " on_error_bound_arg_count=" +
                                  std::to_string(entry.onErrorBoundArgCount) + " provenance_handle=" +
                                  std::to_string(entry.provenanceHandle) + " source=" +
                                  quoteSemanticString(formatSemanticSourceLocation(entry.sourceLine, entry.sourceColumn)));
  }
  const auto onErrorFacts = semanticProgramOnErrorFactView(semanticProgram);
  const auto requirementPredicateFacts =
      semanticProgramRequirementPredicateFactView(semanticProgram);
  for (size_t i = 0; i < requirementPredicateFacts.size(); ++i) {
    const auto &entry = *requirementPredicateFacts[i];
    const std::string_view definitionPath =
        semanticProgramResolveCallTargetString(semanticProgram, entry.definitionPathId);
    const std::string_view predicateKind =
        semanticProgramResolveCallTargetString(semanticProgram, entry.predicateKindId);
    const std::string_view predicateName =
        semanticProgramResolveCallTargetString(semanticProgram, entry.predicateNameId);
    const std::string_view relationOperator =
        semanticProgramResolveCallTargetString(semanticProgram, entry.relationOperatorId);
    const std::string_view sourceText =
        semanticProgramResolveCallTargetString(semanticProgram, entry.sourceTextId);
    const std::string compileTimeEffects =
        formatSemanticStringListFromIds(semanticProgram,
                                        entry.compileTimeEffectIds,
                                        entry.compileTimeEffects);
    const std::string_view evaluationOutcome =
        semanticProgramResolveCallTargetString(semanticProgram, entry.evaluationOutcomeId);
    const std::string_view evaluationDiagnostic =
        semanticProgramResolveCallTargetString(semanticProgram, entry.evaluationDiagnosticId);
    appendSemanticIndexedLine(out,
                              "requirement_predicate_facts",
                              i,
                              "definition_path=" +
                                  quoteSemanticString(definitionPath.empty()
                                                          ? entry.definitionPath
                                                          : definitionPath) +
                                  " predicate_kind=" +
                                  quoteSemanticString(predicateKind.empty()
                                                          ? entry.predicateKind
                                                          : predicateKind) +
                                  " predicate_name=" +
                                  quoteSemanticString(predicateName.empty()
                                                          ? entry.predicateName
                                                          : predicateName) +
                                  " relation_operator=" +
                                  quoteSemanticString(relationOperator.empty()
                                                          ? entry.relationOperator
                                                          : relationOperator) +
                                  " source_text=" +
                                  quoteSemanticString(sourceText.empty()
                                                          ? entry.sourceText
                                                          : sourceText) +
                                  " operands=" +
                                  formatSemanticRequirementOperandList(semanticProgram,
                                                                       entry.operands) +
                                  " compile_time_effects=" +
                                  compileTimeEffects +
                                  " evaluation_outcome=" +
                                  quoteSemanticString(evaluationOutcome.empty()
                                                          ? entry.evaluationOutcome
                                                          : evaluationOutcome) +
                                  " evaluation_diagnostic=" +
                                  quoteSemanticString(evaluationDiagnostic.empty()
                                                          ? entry.evaluationDiagnostic
                                                          : evaluationDiagnostic) +
                                  " provenance_handle=" +
                                  std::to_string(entry.provenanceHandle) + " source=" +
                                  quoteSemanticString(formatSemanticSourceLocation(entry.sourceLine,
                                                                                   entry.sourceColumn)));
  }
  for (size_t i = 0; i < onErrorFacts.size(); ++i) {
    const auto &entry = *onErrorFacts[i];
    const std::string_view definitionPath =
        semanticProgramResolveCallTargetString(semanticProgram, entry.definitionPathId);
    const std::string_view returnKind =
        semanticProgramResolveCallTargetString(semanticProgram, entry.returnKindId);
    const std::string_view handlerPath = semanticProgramOnErrorFactHandlerPath(semanticProgram, entry);
    const std::string_view errorType =
        semanticProgramResolveCallTargetString(semanticProgram, entry.errorTypeId);
    const std::string boundArgTexts = formatSemanticStringListFromIds(
        semanticProgram, entry.boundArgTextIds, entry.boundArgTexts);
    const std::string_view returnResultValueType =
        semanticProgramResolveCallTargetString(semanticProgram, entry.returnResultValueTypeId);
    const std::string_view returnResultErrorType =
        semanticProgramResolveCallTargetString(semanticProgram, entry.returnResultErrorTypeId);
    appendSemanticIndexedLine(out,
                              "on_error_facts",
                              i,
                              "definition_path=" +
                                  quoteSemanticString(definitionPath.empty() ? entry.definitionPath
                                                                             : definitionPath) +
                                  " return_kind=" +
                                  quoteSemanticString(returnKind.empty() ? entry.returnKind : returnKind) +
                                  " handler_path=" +
                                  quoteSemanticString(handlerPath) +
                                  " error_type=" +
                                  quoteSemanticString(errorType.empty() ? entry.errorType : errorType) +
                                  " bound_arg_count=" +
                                  std::to_string(entry.boundArgCount) + " bound_arg_texts=" +
                                  boundArgTexts + " return_result_has_value=" +
                                  formatSemanticBool(entry.returnResultHasValue) + " return_result_value_type=" +
                                  quoteSemanticString(returnResultValueType.empty()
                                                          ? entry.returnResultValueType
                                                          : returnResultValueType) +
                                  " return_result_error_type=" +
                                  quoteSemanticString(returnResultErrorType.empty()
                                                          ? entry.returnResultErrorType
                                                          : returnResultErrorType) +
                                  " provenance_handle=" +
                                  std::to_string(entry.provenanceHandle));
  }
  out << "}\n";
  return out.str();
}

} // namespace primec
