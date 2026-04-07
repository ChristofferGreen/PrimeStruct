#include "primec/SemanticProduct.h"

#include <sstream>
#include <string_view>

namespace primec {
namespace {

std::string quoteSemanticString(std::string_view value) {
  std::string out;
  out.reserve(value.size() + 2);
  out.push_back('"');
  for (char ch : value) {
    switch (ch) {
      case '\\':
        out += "\\\\";
        break;
      case '"':
        out += "\\\"";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        out.push_back(ch);
        break;
    }
  }
  out.push_back('"');
  return out;
}

std::string formatSemanticBool(bool value) {
  return value ? "true" : "false";
}

std::string formatSemanticStringList(const std::vector<std::string> &values) {
  std::ostringstream out;
  out << "[";
  for (size_t i = 0; i < values.size(); ++i) {
    if (i != 0) {
      out << ", ";
    }
    out << quoteSemanticString(values[i]);
  }
  out << "]";
  return out.str();
}

void appendSemanticHeaderLine(std::ostringstream &out, std::string_view label, const std::string &value) {
  out << "  " << label << ": " << value << "\n";
}

void appendSemanticIndexedLine(std::ostringstream &out,
                               std::string_view label,
                               size_t index,
                               const std::string &value) {
  out << "  " << label << "[" << index << "]: " << value << "\n";
}

std::string formatSemanticSourceLocation(int line, int column) {
  return std::to_string(line) + ":" + std::to_string(column);
}

template <typename EntryT, typename ModuleEntriesFn, typename FlatEntriesFn>
std::vector<const EntryT *> buildModuleOrFlatSemanticView(const SemanticProgram &semanticProgram,
                                                          ModuleEntriesFn moduleEntries,
                                                          FlatEntriesFn flatEntries) {
  std::vector<const EntryT *> entries;
  if (!semanticProgram.moduleResolvedArtifacts.empty()) {
    size_t moduleEntryCount = 0;
    for (const auto &module : semanticProgram.moduleResolvedArtifacts) {
      moduleEntryCount += moduleEntries(module).size();
    }
    entries.reserve(moduleEntryCount);
    for (const auto &module : semanticProgram.moduleResolvedArtifacts) {
      for (const auto &entry : moduleEntries(module)) {
        entries.push_back(&entry);
      }
    }
    if (!entries.empty() || flatEntries(semanticProgram).empty()) {
      return entries;
    }
  }

  const auto &flat = flatEntries(semanticProgram);
  entries.reserve(flat.size());
  for (const auto &entry : flat) {
    entries.push_back(&entry);
  }
  return entries;
}

} // namespace

std::vector<const SemanticProgramDirectCallTarget *>
semanticProgramDirectCallTargetView(const SemanticProgram &semanticProgram) {
  return buildModuleOrFlatSemanticView<SemanticProgramDirectCallTarget>(
      semanticProgram,
      [](const SemanticProgramModuleResolvedArtifacts &module)
          -> const std::vector<SemanticProgramDirectCallTarget> & { return module.directCallTargets; },
      [](const SemanticProgram &program) -> const std::vector<SemanticProgramDirectCallTarget> & {
        return program.directCallTargets;
      });
}

std::vector<const SemanticProgramMethodCallTarget *>
semanticProgramMethodCallTargetView(const SemanticProgram &semanticProgram) {
  return buildModuleOrFlatSemanticView<SemanticProgramMethodCallTarget>(
      semanticProgram,
      [](const SemanticProgramModuleResolvedArtifacts &module)
          -> const std::vector<SemanticProgramMethodCallTarget> & { return module.methodCallTargets; },
      [](const SemanticProgram &program) -> const std::vector<SemanticProgramMethodCallTarget> & {
        return program.methodCallTargets;
      });
}

std::vector<const SemanticProgramBridgePathChoice *>
semanticProgramBridgePathChoiceView(const SemanticProgram &semanticProgram) {
  return buildModuleOrFlatSemanticView<SemanticProgramBridgePathChoice>(
      semanticProgram,
      [](const SemanticProgramModuleResolvedArtifacts &module)
          -> const std::vector<SemanticProgramBridgePathChoice> & { return module.bridgePathChoices; },
      [](const SemanticProgram &program) -> const std::vector<SemanticProgramBridgePathChoice> & {
        return program.bridgePathChoices;
      });
}

std::vector<const SemanticProgramCallableSummary *>
semanticProgramCallableSummaryView(const SemanticProgram &semanticProgram) {
  return buildModuleOrFlatSemanticView<SemanticProgramCallableSummary>(
      semanticProgram,
      [](const SemanticProgramModuleResolvedArtifacts &module)
          -> const std::vector<SemanticProgramCallableSummary> & { return module.callableSummaries; },
      [](const SemanticProgram &program) -> const std::vector<SemanticProgramCallableSummary> & {
        return program.callableSummaries;
      });
}

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
    appendSemanticIndexedLine(out,
                              "definitions",
                              i,
                              "full_path=" + quoteSemanticString(entry.fullPath) + " name=" +
                                  quoteSemanticString(entry.name) + " namespace_prefix=" +
                                  quoteSemanticString(entry.namespacePrefix) + " provenance_handle=" +
                                  std::to_string(entry.provenanceHandle) + " source=" +
                                  quoteSemanticString(formatSemanticSourceLocation(entry.sourceLine, entry.sourceColumn)));
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
  for (size_t i = 0; i < semanticProgram.directCallTargets.size(); ++i) {
    const auto &entry = semanticProgram.directCallTargets[i];
    appendSemanticIndexedLine(out,
                              "direct_call_targets",
                              i,
                              "scope_path=" + quoteSemanticString(entry.scopePath) + " call_name=" +
                                  quoteSemanticString(entry.callName) + " resolved_path=" +
                                  quoteSemanticString(entry.resolvedPath) + " provenance_handle=" +
                                  std::to_string(entry.provenanceHandle) + " source=" +
                                  quoteSemanticString(formatSemanticSourceLocation(entry.sourceLine, entry.sourceColumn)));
  }
  for (size_t i = 0; i < semanticProgram.methodCallTargets.size(); ++i) {
    const auto &entry = semanticProgram.methodCallTargets[i];
    appendSemanticIndexedLine(out,
                              "method_call_targets",
                              i,
                              "scope_path=" + quoteSemanticString(entry.scopePath) + " method_name=" +
                                  quoteSemanticString(entry.methodName) + " receiver_type_text=" +
                                  quoteSemanticString(entry.receiverTypeText) + " resolved_path=" +
                                  quoteSemanticString(entry.resolvedPath) + " provenance_handle=" +
                                  std::to_string(entry.provenanceHandle) + " source=" +
                                  quoteSemanticString(formatSemanticSourceLocation(entry.sourceLine, entry.sourceColumn)));
  }
  for (size_t i = 0; i < semanticProgram.bridgePathChoices.size(); ++i) {
    const auto &entry = semanticProgram.bridgePathChoices[i];
    appendSemanticIndexedLine(out,
                              "bridge_path_choices",
                              i,
                              "scope_path=" + quoteSemanticString(entry.scopePath) + " collection_family=" +
                                  quoteSemanticString(entry.collectionFamily) + " helper_name=" +
                                  quoteSemanticString(entry.helperName) + " chosen_path=" +
                                  quoteSemanticString(entry.chosenPath) + " provenance_handle=" +
                                  std::to_string(entry.provenanceHandle) + " source=" +
                                  quoteSemanticString(formatSemanticSourceLocation(entry.sourceLine, entry.sourceColumn)));
  }
  for (size_t i = 0; i < semanticProgram.callableSummaries.size(); ++i) {
    const auto &entry = semanticProgram.callableSummaries[i];
    appendSemanticIndexedLine(out,
                              "callable_summaries",
                              i,
                              "full_path=" + quoteSemanticString(entry.fullPath) + " is_execution=" +
                                  formatSemanticBool(entry.isExecution) + " return_kind=" +
                                  quoteSemanticString(entry.returnKind) + " is_compute=" +
                                  formatSemanticBool(entry.isCompute) + " is_unsafe=" +
                                  formatSemanticBool(entry.isUnsafe) + " active_effects=" +
                                  formatSemanticStringList(entry.activeEffects) + " active_capabilities=" +
                                  formatSemanticStringList(entry.activeCapabilities) + " has_result_type=" +
                                  formatSemanticBool(entry.hasResultType) + " result_type_has_value=" +
                                  formatSemanticBool(entry.resultTypeHasValue) + " result_value_type=" +
                                  quoteSemanticString(entry.resultValueType) + " result_error_type=" +
                                  quoteSemanticString(entry.resultErrorType) + " has_on_error=" +
                                  formatSemanticBool(entry.hasOnError) + " on_error_handler_path=" +
                                  quoteSemanticString(entry.onErrorHandlerPath) + " on_error_error_type=" +
                                  quoteSemanticString(entry.onErrorErrorType) + " on_error_bound_arg_count=" +
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
  for (size_t i = 0; i < semanticProgram.bindingFacts.size(); ++i) {
    const auto &entry = semanticProgram.bindingFacts[i];
    appendSemanticIndexedLine(out,
                              "binding_facts",
                              i,
                              "scope_path=" + quoteSemanticString(entry.scopePath) + " site_kind=" +
                                  quoteSemanticString(entry.siteKind) + " name=" +
                                  quoteSemanticString(entry.name) + " resolved_path=" +
                                  quoteSemanticString(entry.resolvedPath) + " binding_type_text=" +
                                  quoteSemanticString(entry.bindingTypeText) + " is_mutable=" +
                                  formatSemanticBool(entry.isMutable) + " is_entry_arg_string=" +
                                  formatSemanticBool(entry.isEntryArgString) + " is_unsafe_reference=" +
                                  formatSemanticBool(entry.isUnsafeReference) + " reference_root=" +
                                  quoteSemanticString(entry.referenceRoot) + " provenance_handle=" +
                                  std::to_string(entry.provenanceHandle) + " source=" +
                                  quoteSemanticString(formatSemanticSourceLocation(entry.sourceLine, entry.sourceColumn)));
  }
  for (size_t i = 0; i < semanticProgram.returnFacts.size(); ++i) {
    const auto &entry = semanticProgram.returnFacts[i];
    appendSemanticIndexedLine(out,
                              "return_facts",
                              i,
                              "definition_path=" + quoteSemanticString(entry.definitionPath) + " return_kind=" +
                                  quoteSemanticString(entry.returnKind) + " struct_path=" +
                                  quoteSemanticString(entry.structPath) + " binding_type_text=" +
                                  quoteSemanticString(entry.bindingTypeText) + " is_mutable=" +
                                  formatSemanticBool(entry.isMutable) + " is_entry_arg_string=" +
                                  formatSemanticBool(entry.isEntryArgString) + " is_unsafe_reference=" +
                                  formatSemanticBool(entry.isUnsafeReference) + " reference_root=" +
                                  quoteSemanticString(entry.referenceRoot) + " provenance_handle=" +
                                  std::to_string(entry.provenanceHandle) + " source=" +
                                  quoteSemanticString(formatSemanticSourceLocation(entry.sourceLine, entry.sourceColumn)));
  }
  for (size_t i = 0; i < semanticProgram.localAutoFacts.size(); ++i) {
    const auto &entry = semanticProgram.localAutoFacts[i];
    appendSemanticIndexedLine(out,
                              "local_auto_facts",
                              i,
                              "scope_path=" + quoteSemanticString(entry.scopePath) + " binding_name=" +
                                  quoteSemanticString(entry.bindingName) + " binding_type_text=" +
                                  quoteSemanticString(entry.bindingTypeText) + " initializer_resolved_path=" +
                                  quoteSemanticString(entry.initializerResolvedPath) +
                                  " initializer_binding_type_text=" +
                                  quoteSemanticString(entry.initializerBindingTypeText) +
                                  " initializer_receiver_binding_type_text=" +
                                  quoteSemanticString(entry.initializerReceiverBindingTypeText) +
                                  " initializer_query_type_text=" +
                                  quoteSemanticString(entry.initializerQueryTypeText) +
                                  " initializer_result_has_value=" +
                                  formatSemanticBool(entry.initializerResultHasValue) +
                                  " initializer_result_value_type=" +
                                  quoteSemanticString(entry.initializerResultValueType) +
                                  " initializer_result_error_type=" +
                                  quoteSemanticString(entry.initializerResultErrorType) +
                                  " initializer_has_try=" + formatSemanticBool(entry.initializerHasTry) +
                                  " initializer_try_operand_resolved_path=" +
                                  quoteSemanticString(entry.initializerTryOperandResolvedPath) +
                                  " initializer_try_operand_binding_type_text=" +
                                  quoteSemanticString(entry.initializerTryOperandBindingTypeText) +
                                  " initializer_try_operand_receiver_binding_type_text=" +
                                  quoteSemanticString(entry.initializerTryOperandReceiverBindingTypeText) +
                                  " initializer_try_operand_query_type_text=" +
                                  quoteSemanticString(entry.initializerTryOperandQueryTypeText) +
                                  " initializer_try_value_type=" +
                                  quoteSemanticString(entry.initializerTryValueType) +
                                  " initializer_try_error_type=" +
                                  quoteSemanticString(entry.initializerTryErrorType) +
                                  " initializer_try_context_return_kind=" +
                                  quoteSemanticString(entry.initializerTryContextReturnKind) +
                                  " initializer_try_on_error_handler_path=" +
                                  quoteSemanticString(entry.initializerTryOnErrorHandlerPath) +
                                  " initializer_try_on_error_error_type=" +
                                  quoteSemanticString(entry.initializerTryOnErrorErrorType) +
                                  " initializer_try_on_error_bound_arg_count=" +
                                  std::to_string(entry.initializerTryOnErrorBoundArgCount) +
                                  " provenance_handle=" + std::to_string(entry.provenanceHandle) + " source=" +
                                  quoteSemanticString(formatSemanticSourceLocation(entry.sourceLine, entry.sourceColumn)));
  }
  for (size_t i = 0; i < semanticProgram.queryFacts.size(); ++i) {
    const auto &entry = semanticProgram.queryFacts[i];
    appendSemanticIndexedLine(out,
                              "query_facts",
                              i,
                              "scope_path=" + quoteSemanticString(entry.scopePath) + " call_name=" +
                                  quoteSemanticString(entry.callName) + " resolved_path=" +
                                  quoteSemanticString(entry.resolvedPath) + " query_type_text=" +
                                  quoteSemanticString(entry.queryTypeText) + " binding_type_text=" +
                                  quoteSemanticString(entry.bindingTypeText) + " receiver_binding_type_text=" +
                                  quoteSemanticString(entry.receiverBindingTypeText) + " has_result_type=" +
                                  formatSemanticBool(entry.hasResultType) + " result_type_has_value=" +
                                  formatSemanticBool(entry.resultTypeHasValue) + " result_value_type=" +
                                  quoteSemanticString(entry.resultValueType) + " result_error_type=" +
                                  quoteSemanticString(entry.resultErrorType) + " provenance_handle=" +
                                  std::to_string(entry.provenanceHandle) + " source=" +
                                  quoteSemanticString(formatSemanticSourceLocation(entry.sourceLine, entry.sourceColumn)));
  }
  for (size_t i = 0; i < semanticProgram.tryFacts.size(); ++i) {
    const auto &entry = semanticProgram.tryFacts[i];
    appendSemanticIndexedLine(out,
                              "try_facts",
                              i,
                              "scope_path=" + quoteSemanticString(entry.scopePath) + " operand_resolved_path=" +
                                  quoteSemanticString(entry.operandResolvedPath) +
                                  " operand_binding_type_text=" +
                                  quoteSemanticString(entry.operandBindingTypeText) +
                                  " operand_receiver_binding_type_text=" +
                                  quoteSemanticString(entry.operandReceiverBindingTypeText) +
                                  " operand_query_type_text=" +
                                  quoteSemanticString(entry.operandQueryTypeText) + " value_type=" +
                                  quoteSemanticString(entry.valueType) + " error_type=" +
                                  quoteSemanticString(entry.errorType) + " context_return_kind=" +
                                  quoteSemanticString(entry.contextReturnKind) + " on_error_handler_path=" +
                                  quoteSemanticString(entry.onErrorHandlerPath) + " on_error_error_type=" +
                                  quoteSemanticString(entry.onErrorErrorType) + " on_error_bound_arg_count=" +
                                  std::to_string(entry.onErrorBoundArgCount) + " provenance_handle=" +
                                  std::to_string(entry.provenanceHandle) + " source=" +
                                  quoteSemanticString(formatSemanticSourceLocation(entry.sourceLine, entry.sourceColumn)));
  }
  for (size_t i = 0; i < semanticProgram.onErrorFacts.size(); ++i) {
    const auto &entry = semanticProgram.onErrorFacts[i];
    appendSemanticIndexedLine(out,
                              "on_error_facts",
                              i,
                              "definition_path=" + quoteSemanticString(entry.definitionPath) + " return_kind=" +
                                  quoteSemanticString(entry.returnKind) + " handler_path=" +
                                  quoteSemanticString(entry.handlerPath) + " error_type=" +
                                  quoteSemanticString(entry.errorType) + " bound_arg_count=" +
                                  std::to_string(entry.boundArgCount) + " bound_arg_texts=" +
                                  formatSemanticStringList(entry.boundArgTexts) + " return_result_has_value=" +
                                  formatSemanticBool(entry.returnResultHasValue) + " return_result_value_type=" +
                                  quoteSemanticString(entry.returnResultValueType) +
                                  " return_result_error_type=" +
                                  quoteSemanticString(entry.returnResultErrorType) + " provenance_handle=" +
                                  std::to_string(entry.provenanceHandle));
  }
  out << "}\n";
  return out.str();
}

} // namespace primec
