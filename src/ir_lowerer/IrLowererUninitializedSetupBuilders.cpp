#include "IrLowererUninitializedTypeHelpers.h"

#include <memory>
#include <vector>

#include "IrLowererBindingTypeHelpers.h"
#include "IrLowererCallHelpers.h"
#include "IrLowererHelpers.h"
#include "IrLowererSetupTypeHelpers.h"
#include "IrLowererStructTypeHelpers.h"
#include "IrLowererTemplateTypeParseHelpers.h"

namespace primec::ir_lowerer {

bool buildProgramEntryReturnRuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup(
    std::vector<std::string> &stringTable,
    IrFunction &function,
    const Program &program,
    const Definition &entryDef,
    const std::string &entryPath,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const std::unordered_map<std::string, std::string> &importAliases,
    const std::unordered_set<std::string> &structNames,
    std::size_t structReserveHint,
    const EnumerateStructLayoutFieldsFn &enumerateStructLayoutFields,
    EntryReturnRuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup &out,
    std::string &error) {
  return buildProgramEntryReturnRuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup(
      stringTable,
      function,
      program,
      nullptr,
      entryDef,
      entryPath,
      defMap,
      importAliases,
      structNames,
      structReserveHint,
      enumerateStructLayoutFields,
      out,
      error);
}

bool buildProgramEntryReturnRuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup(
    std::vector<std::string> &stringTable,
    IrFunction &function,
    const Program &program,
    const SemanticProgram *semanticProgram,
    const Definition &entryDef,
    const std::string &entryPath,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const std::unordered_map<std::string, std::string> &importAliases,
    const std::unordered_set<std::string> &structNames,
    std::size_t structReserveHint,
    const EnumerateStructLayoutFieldsFn &enumerateStructLayoutFields,
    EntryReturnRuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup &out,
    std::string &error) {
  const bool hasMathImport = hasProgramMathImport(program.imports);
  return buildEntryReturnRuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup(
      stringTable,
      function,
      program,
      semanticProgram,
      entryDef,
      entryPath,
      defMap,
      importAliases,
      hasMathImport,
      structNames,
      structReserveHint,
      enumerateStructLayoutFields,
      out,
      error);
}

bool buildEntryReturnRuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup(
    std::vector<std::string> &stringTable,
    IrFunction &function,
    const Program &program,
    const Definition &entryDef,
    const std::string &entryPath,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const std::unordered_map<std::string, std::string> &importAliases,
    bool hasMathImport,
    const std::unordered_set<std::string> &structNames,
    std::size_t structReserveHint,
    const EnumerateStructLayoutFieldsFn &enumerateStructLayoutFields,
    EntryReturnRuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup &out,
    std::string &error) {
  return buildEntryReturnRuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup(
      stringTable,
      function,
      program,
      nullptr,
      entryDef,
      entryPath,
      defMap,
      importAliases,
      hasMathImport,
      structNames,
      structReserveHint,
      enumerateStructLayoutFields,
      out,
      error);
}

bool buildEntryReturnRuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup(
    std::vector<std::string> &stringTable,
    IrFunction &function,
    const Program &program,
    const SemanticProgram *semanticProgram,
    const Definition &entryDef,
    const std::string &entryPath,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const std::unordered_map<std::string, std::string> &importAliases,
    bool hasMathImport,
    const std::unordered_set<std::string> &structNames,
    std::size_t structReserveHint,
    const EnumerateStructLayoutFieldsFn &enumerateStructLayoutFields,
    EntryReturnRuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup &out,
    std::string &error) {
  std::destroy_at(&out);
  std::construct_at(&out);
  if (!analyzeEntryReturnTransforms(entryDef, entryPath, out.entryReturnConfig, error)) {
    return false;
  }
  if (!buildRuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup(
          stringTable,
          function,
          program,
          semanticProgram,
          entryDef,
          out.entryReturnConfig.returnsVoid,
          defMap,
          importAliases,
          hasMathImport,
          structNames,
          structReserveHint,
          enumerateStructLayoutFields,
          out.runtimeEntrySetupMathTypeStructAndUninitializedResolutionSetup,
          error)) {
    return false;
  }
  return true;
}

bool buildRuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup(
    std::vector<std::string> &stringTable,
    IrFunction &function,
    const Program &program,
    const Definition &entryDef,
    bool definitionReturnsVoid,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const std::unordered_map<std::string, std::string> &importAliases,
    bool hasMathImport,
    const std::unordered_set<std::string> &structNames,
    std::size_t structReserveHint,
    const EnumerateStructLayoutFieldsFn &enumerateStructLayoutFields,
    RuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup &out,
    std::string &error) {
  return buildRuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup(
      stringTable,
      function,
      program,
      nullptr,
      entryDef,
      definitionReturnsVoid,
      defMap,
      importAliases,
      hasMathImport,
      structNames,
      structReserveHint,
      enumerateStructLayoutFields,
      out,
      error);
}

bool buildRuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup(
    std::vector<std::string> &stringTable,
    IrFunction &function,
    const Program &program,
    const SemanticProgram *semanticProgram,
    const Definition &entryDef,
    bool definitionReturnsVoid,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const std::unordered_map<std::string, std::string> &importAliases,
    bool hasMathImport,
    const std::unordered_set<std::string> &structNames,
    std::size_t structReserveHint,
    const EnumerateStructLayoutFieldsFn &enumerateStructLayoutFields,
    RuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup &out,
    std::string &error) {
  std::destroy_at(&out);
  std::construct_at(&out);
  out.runtimeErrorAndStringLiteralSetup =
      makeRuntimeErrorAndStringLiteralSetup(stringTable, function, error);
  if (!buildEntrySetupMathTypeStructAndUninitializedResolutionSetup(
          program,
          semanticProgram,
          entryDef,
          definitionReturnsVoid,
          defMap,
          importAliases,
          hasMathImport,
          structNames,
          structReserveHint,
          enumerateStructLayoutFields,
          out.entrySetupMathTypeStructAndUninitializedResolutionSetup,
          error)) {
    return false;
  }
  return true;
}

bool buildEntrySetupMathTypeStructAndUninitializedResolutionSetup(
    const Program &program,
    const Definition &entryDef,
    bool definitionReturnsVoid,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const std::unordered_map<std::string, std::string> &importAliases,
    bool hasMathImport,
    const std::unordered_set<std::string> &structNames,
    std::size_t structReserveHint,
    const EnumerateStructLayoutFieldsFn &enumerateStructLayoutFields,
    EntrySetupMathTypeStructAndUninitializedResolutionSetup &out,
    std::string &error) {
  return buildEntrySetupMathTypeStructAndUninitializedResolutionSetup(program,
                                                                      nullptr,
                                                                      entryDef,
                                                                      definitionReturnsVoid,
                                                                      defMap,
                                                                      importAliases,
                                                                      hasMathImport,
                                                                      structNames,
                                                                      structReserveHint,
                                                                      enumerateStructLayoutFields,
                                                                      out,
                                                                      error);
}

bool buildEntrySetupMathTypeStructAndUninitializedResolutionSetup(
    const Program &program,
    const SemanticProgram *semanticProgram,
    const Definition &entryDef,
    bool definitionReturnsVoid,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const std::unordered_map<std::string, std::string> &importAliases,
    bool hasMathImport,
    const std::unordered_set<std::string> &structNames,
    std::size_t structReserveHint,
    const EnumerateStructLayoutFieldsFn &enumerateStructLayoutFields,
    EntrySetupMathTypeStructAndUninitializedResolutionSetup &out,
    std::string &error) {
  std::destroy_at(&out);
  std::construct_at(&out);
  if (!buildEntryCountCallOnErrorSetup(program,
                                        entryDef,
                                        definitionReturnsVoid,
                                        defMap,
                                        importAliases,
                                        semanticProgram,
                                        out.entryCountCallOnErrorSetup,
                                        error)) {
    return false;
  }
  const auto &entryCallOnErrorSetup = out.entryCountCallOnErrorSetup.callOnErrorSetup;
  if (!buildSetupMathTypeStructAndUninitializedResolutionSetup(
          hasMathImport,
          structNames,
          importAliases,
          structReserveHint,
          enumerateStructLayoutFields,
          defMap,
          entryCallOnErrorSetup.callResolutionAdapters.resolveExprPath,
          out.setupMathTypeStructAndUninitializedResolutionSetup,
          error)) {
    return false;
  }
  return true;
}

bool buildSetupMathTypeStructAndUninitializedResolutionSetup(
    bool hasMathImport,
    const std::unordered_set<std::string> &structNames,
    const std::unordered_map<std::string, std::string> &importAliases,
    std::size_t structReserveHint,
    const EnumerateStructLayoutFieldsFn &enumerateStructLayoutFields,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const InferStructExprPathFn &resolveExprPath,
    SetupMathTypeStructAndUninitializedResolutionSetup &out,
    std::string &error) {
  std::destroy_at(&out);
  std::construct_at(&out);
  out.setupMathAndBindingAdapters = makeSetupMathAndBindingAdapters(hasMathImport);
  if (!buildSetupTypeStructAndUninitializedResolutionSetup(structNames,
                                                           importAliases,
                                                           structReserveHint,
                                                           enumerateStructLayoutFields,
                                                           defMap,
                                                           resolveExprPath,
                                                           out.setupTypeStructAndUninitializedResolutionSetup,
                                                           error)) {
    return false;
  }
  return true;
}

bool buildSetupTypeStructAndUninitializedResolutionSetup(
    const std::unordered_set<std::string> &structNames,
    const std::unordered_map<std::string, std::string> &importAliases,
    std::size_t structReserveHint,
    const EnumerateStructLayoutFieldsFn &enumerateStructLayoutFields,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const InferStructExprPathFn &resolveExprPath,
    SetupTypeStructAndUninitializedResolutionSetup &out,
    std::string &error) {
  std::destroy_at(&out);
  std::construct_at(&out);
  out.setupTypeAndStructTypeAdapters = makeSetupTypeAndStructTypeAdapters(structNames, importAliases);
  const auto &structTypeResolutionAdapters = out.setupTypeAndStructTypeAdapters.structTypeResolutionAdapters;
  if (!buildStructAndUninitializedResolutionSetup(structReserveHint,
                                                  enumerateStructLayoutFields,
                                                  defMap,
                                                  structTypeResolutionAdapters.resolveStructTypeName,
                                                  out.setupTypeAndStructTypeAdapters.valueKindFromTypeName,
                                                  resolveExprPath,
                                                  out.structAndUninitializedResolutionSetup,
                                                  error)) {
    return false;
  }
  return true;
}

bool buildStructAndUninitializedResolutionSetup(
    std::size_t structReserveHint,
    const EnumerateStructLayoutFieldsFn &enumerateStructLayoutFields,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResolveStructTypeNameFn &resolveStructTypeName,
    const ValueKindFromTypeNameFn &valueKindFromTypeName,
    const InferStructExprPathFn &resolveExprPath,
    StructAndUninitializedResolutionSetup &out,
    std::string &error) {
  std::destroy_at(&out);
  std::construct_at(&out);
  out.fieldIndexes =
      buildStructAndUninitializedFieldIndexes(structReserveHint, enumerateStructLayoutFields);
  out.structLayoutResolutionAdapters = makeStructLayoutResolutionAdaptersWithOwnedSlotState(
      out.fieldIndexes.structLayoutFieldIndex,
      defMap,
      resolveStructTypeName,
      valueKindFromTypeName,
      error);
  out.uninitializedResolutionAdapters = makeUninitializedResolutionAdapters(
      resolveStructTypeName,
      resolveExprPath,
      out.fieldIndexes.uninitializedFieldBindingIndex,
      defMap,
      out.structLayoutResolutionAdapters.structSlotResolution.resolveStructFieldSlot,
      error);
  return true;
}

UninitializedResolutionAdapters makeUninitializedResolutionAdapters(
    const ResolveStructTypeNameFn &resolveStructTypePath,
    const InferStructExprPathFn &resolveExprPath,
    const UninitializedFieldBindingIndex &fieldIndex,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResolveStructFieldSlotFn &resolveStructFieldSlot,
    std::string &error) {
  UninitializedResolutionAdapters adapters{};
  adapters.resolveUninitializedTypeInfo = makeResolveUninitializedTypeInfo(resolveStructTypePath, error);
  adapters.resolveUninitializedStorage = makeResolveUninitializedStorageAccessFromDefinitionFieldIndex(
      fieldIndex, defMap, adapters.resolveUninitializedTypeInfo, resolveStructFieldSlot, error);
  adapters.inferStructExprPath = makeInferStructExprPathFromDefinitionMapByCallTargetWithFieldIndex(
      defMap, resolveStructTypePath, resolveExprPath, fieldIndex, resolveStructFieldSlot);
  return adapters;
}

ResolveUninitializedFieldTypeInfoFn makeResolveUninitializedTypeInfo(
    const ResolveStructTypeNameFn &resolveStructTypePath,
    std::string &error) {
  return [resolveStructTypePath, &error](
             const std::string &typeText, const std::string &namespacePrefix, UninitializedTypeInfo &out) {
    return resolveUninitializedTypeInfo(typeText, namespacePrefix, resolveStructTypePath, out, error);
  };
}

} // namespace primec::ir_lowerer
