#include "IrLowererLowerEffects.h"

#include "IrLowererCallHelpers.h"
#include "IrLowererHelpers.h"
#include "IrLowererSemanticProductTargetAdapters.h"
#include "primec/Ir.h"
#include "primec/SemanticProduct.h"

namespace primec::ir_lowerer {
namespace {

void expandEffectImplications(std::unordered_set<std::string> &effects) {
  if (effects.count("file_write") != 0) {
    effects.insert("file_read");
  }
}

bool isDefinitionCallableForEffectMetadata(const Definition &def) {
  return !definitionHasTransform(def, "sum");
}

bool isDefinitionCallableForEffectMetadata(const Definition &def,
                                           const SemanticProgram *semanticProgram) {
  if (!isDefinitionCallableForEffectMetadata(def)) {
    return false;
  }
  if (semanticProgram == nullptr) {
    return true;
  }
  if (const auto *typeMetadata =
          semanticProgramLookupTypeMetadata(*semanticProgram, def.fullPath);
      typeMetadata != nullptr && typeMetadata->category == "sum") {
    return false;
  }
  for (const auto &sumMetadata : semanticProgram->sumTypeMetadata) {
    if (sumMetadata.fullPath == def.fullPath) {
      return false;
    }
  }
  return true;
}

} // namespace

bool findEntryDefinition(const Program &program,
                         const std::string &entryPath,
                         const Definition *&entryDefOut,
                         std::string &error) {
  entryDefOut = nullptr;
  for (const auto &def : program.definitions) {
    if (def.fullPath == entryPath) {
      entryDefOut = &def;
      break;
    }
  }
  if (!entryDefOut) {
    error = "native backend requires entry definition " + entryPath;
    return false;
  }
  return true;
}

bool validateNoSoftwareNumericTypesForBackendSurface(const SemanticProgram *semanticProgram,
                                                     std::string_view backendSurfaceName,
                                                     std::string &error) {
  if (semanticProgram == nullptr) {
    return true;
  }
  const std::string_view found =
      semanticProgramLookupPublishedLowererSoftwareNumericType(*semanticProgram);
  if (found.empty()) {
    return true;
  }
  error = std::string(backendSurfaceName) + " does not support software numeric types: " + std::string(found);
  return false;
}

bool validateNoRuntimeReflectionQueriesForBackendSurface(const SemanticProgram *semanticProgram,
                                                         std::string_view backendSurfaceName,
                                                         std::string &error) {
  if (semanticProgram == nullptr) {
    return true;
  }
  const std::string_view found =
      semanticProgramLookupPublishedLowererRuntimeReflectionPath(*semanticProgram);
  if (found.empty()) {
    return true;
  }
  if (semanticProgramLookupPublishedLowererRuntimeReflectionUsesObjectTable(*semanticProgram)) {
    error = "runtime reflection objects/tables are unsupported: " + std::string(found);
  } else {
    error = std::string(backendSurfaceName) + " requires compile-time reflection query elimination before IR emission: " +
            std::string(found);
  }
  return false;
}

bool effectBitForName(const std::string &name, uint64_t &outBit) {
  if (name == "io_out") {
    outBit = EffectIoOut;
    return true;
  }
  if (name == "io_err") {
    outBit = EffectIoErr;
    return true;
  }
  if (name == "heap_alloc") {
    outBit = EffectHeapAlloc;
    return true;
  }
  if (name == "pathspace_notify") {
    outBit = EffectPathSpaceNotify;
    return true;
  }
  if (name == "pathspace_insert") {
    outBit = EffectPathSpaceInsert;
    return true;
  }
  if (name == "pathspace_take") {
    outBit = EffectPathSpaceTake;
    return true;
  }
  if (name == "pathspace_bind") {
    outBit = EffectPathSpaceBind;
    return true;
  }
  if (name == "pathspace_schedule") {
    outBit = EffectPathSpaceSchedule;
    return true;
  }
  if (name == "file_write") {
    outBit = EffectFileWrite;
    return true;
  }
  if (name == "file_read") {
    outBit = EffectFileRead;
    return true;
  }
  if (name == "gpu_dispatch") {
    outBit = EffectGpuDispatch;
    return true;
  }
  return false;
}

bool isSupportedEffect(const std::string &name) {
  return name == "io_out" || name == "io_err" || name == "heap_alloc" || name == "pathspace_notify" ||
         name == "pathspace_insert" || name == "pathspace_take" || name == "pathspace_bind" ||
         name == "pathspace_schedule" || name == "file_write" || name == "file_read" ||
         name == "gpu_dispatch";
}

std::unordered_set<std::string> resolveActiveEffects(const std::vector<Transform> &transforms,
                                                     bool isEntry,
                                                     const std::vector<std::string> &defaultEffects,
                                                     const std::vector<std::string> &entryDefaultEffects) {
  std::unordered_set<std::string> effects;
  bool sawEffects = false;
  for (const auto &transform : transforms) {
    if (transform.name != "effects") {
      continue;
    }
    sawEffects = true;
    effects.clear();
    for (const auto &effect : transform.arguments) {
      effects.insert(effect);
    }
  }
  if (!sawEffects) {
    const auto &defaults = isEntry ? entryDefaultEffects : defaultEffects;
    for (const auto &effect : defaults) {
      effects.insert(effect);
    }
  }
  expandEffectImplications(effects);
  return effects;
}

bool validateEffectsTransforms(const std::vector<Transform> &transforms,
                               const std::string &context,
                               std::string &error) {
  return validateEffectsTransformsForBackendSurface(transforms, context, "native backend", error);
}

bool validateEffectsTransformsForBackendSurface(const std::vector<Transform> &transforms,
                                                const std::string &context,
                                                std::string_view backendSurfaceName,
                                                std::string &error) {
  for (const auto &transform : transforms) {
    if (transform.name != "effects") {
      continue;
    }
    for (const auto &effect : transform.arguments) {
      if (!isSupportedEffect(effect)) {
        error = std::string(backendSurfaceName) + " does not support effect: " + effect + " on " + context;
        return false;
      }
    }
  }
  return true;
}

bool validateActiveEffects(const std::vector<Transform> &transforms,
                           const std::string &context,
                           bool isEntry,
                           const std::vector<std::string> &defaultEffects,
                           const std::vector<std::string> &entryDefaultEffects,
                           std::string &error) {
  return validateActiveEffectsForBackendSurface(
      transforms, context, isEntry, defaultEffects, entryDefaultEffects, "native backend", error);
}

bool validateActiveEffectsForBackendSurface(const std::vector<Transform> &transforms,
                                            const std::string &context,
                                            bool isEntry,
                                            const std::vector<std::string> &defaultEffects,
                                            const std::vector<std::string> &entryDefaultEffects,
                                            std::string_view backendSurfaceName,
                                            std::string &error) {
  const auto effects = resolveActiveEffects(transforms, isEntry, defaultEffects, entryDefaultEffects);
  for (const auto &effect : effects) {
    if (!isSupportedEffect(effect)) {
      error = std::string(backendSurfaceName) + " does not support effect: " + effect + " on " + context;
      return false;
    }
  }
  return true;
}

bool validateProgramEffectsForBackendSurface(const Program &program,
                                             const SemanticProgram *semanticProgram,
                                             const std::string &entryPath,
                                             const std::vector<std::string> &defaultEffects,
                                             const std::vector<std::string> &entryDefaultEffects,
                                             std::string_view backendSurfaceName,
                                             std::string &error) {
  const auto validateCallableEffects =
      [&](const std::string &fullPath,
          const std::vector<Transform> &transforms,
          bool isEntry,
          const std::string &context) -> bool {
    if (semanticProgram != nullptr) {
      if (const auto *callableSummary =
              findSemanticProductCallableSummary(semanticProgram, fullPath);
          callableSummary != nullptr) {
        for (const auto &effect : callableSummary->activeEffects) {
          if (!isSupportedEffect(effect)) {
            error = std::string(backendSurfaceName) + " does not support effect: " + effect + " on " + context;
            return false;
          }
        }
        return true;
      }
      error = "missing semantic-product callable summary: " + fullPath;
      return false;
    }
    return validateActiveEffectsForBackendSurface(
        transforms, context, isEntry, defaultEffects, entryDefaultEffects, backendSurfaceName, error);
  };

  const auto validateExprEffects = [&](const auto &self, const Expr &expr, const std::string &context) -> bool {
    if (!validateEffectsTransformsForBackendSurface(expr.transforms, context, backendSurfaceName, error)) {
      return false;
    }
    for (const auto &arg : expr.args) {
      if (!self(self, arg, context)) {
        return false;
      }
    }
    for (const auto &bodyArg : expr.bodyArguments) {
      if (!self(self, bodyArg, context)) {
        return false;
      }
    }
    return true;
  };

  for (const auto &def : program.definitions) {
    if (isDefinitionCallableForEffectMetadata(def, semanticProgram) &&
        !validateCallableEffects(def.fullPath, def.transforms, def.fullPath == entryPath, def.fullPath)) {
      return false;
    }
    for (const auto &param : def.parameters) {
      if (!validateExprEffects(validateExprEffects, param, def.fullPath)) {
        return false;
      }
    }
    for (const auto &stmt : def.statements) {
      if (!validateExprEffects(validateExprEffects, stmt, def.fullPath)) {
        return false;
      }
    }
    if (def.returnExpr.has_value() && !validateExprEffects(validateExprEffects, *def.returnExpr, def.fullPath)) {
      return false;
    }
  }

  for (const auto &exec : program.executions) {
    if (!validateCallableEffects(exec.fullPath, exec.transforms, false, exec.fullPath)) {
      return false;
    }
    for (const auto &arg : exec.arguments) {
      if (!validateExprEffects(validateExprEffects, arg, exec.fullPath)) {
        return false;
      }
    }
    for (const auto &bodyArg : exec.bodyArguments) {
      if (!validateExprEffects(validateExprEffects, bodyArg, exec.fullPath)) {
        return false;
      }
    }
  }

  return true;
}

bool resolveEntryMetadataMasks(const Definition &entryDef,
                               const std::string &entryPath,
                               const std::vector<std::string> &defaultEffects,
                               const std::vector<std::string> &entryDefaultEffects,
                               uint64_t &entryEffectMaskOut,
                               uint64_t &entryCapabilityMaskOut,
                               std::string &error) {
  return resolveEntryMetadataMasks(entryDef,
                                   nullptr,
                                   entryPath,
                                   defaultEffects,
                                   entryDefaultEffects,
                                   entryEffectMaskOut,
                                   entryCapabilityMaskOut,
                                   error);
}

bool resolveEntryMetadataMasks(const Definition &entryDef,
                               const SemanticProgram *semanticProgram,
                               const std::string &entryPath,
                               const std::vector<std::string> &defaultEffects,
                               const std::vector<std::string> &entryDefaultEffects,
                               uint64_t &entryEffectMaskOut,
                               uint64_t &entryCapabilityMaskOut,
                               std::string &error) {
  if (semanticProgram != nullptr) {
    if (const auto *callableSummary = findSemanticProductCallableSummary(semanticProgram, entryPath);
        callableSummary != nullptr) {
      entryEffectMaskOut = 0;
      for (const auto &effect : callableSummary->activeEffects) {
        uint64_t bit = 0;
        if (!effectBitForName(effect, bit)) {
          error = "unsupported effect in metadata: " + effect;
          return false;
        }
        entryEffectMaskOut |= bit;
      }

      entryCapabilityMaskOut = 0;
      for (const auto &capability : callableSummary->activeCapabilities) {
        uint64_t bit = 0;
        if (!effectBitForName(capability, bit)) {
          error = "unsupported capability in metadata: " + capability;
          return false;
        }
        entryCapabilityMaskOut |= bit;
      }
      return true;
    }
    error = "missing semantic-product callable summary: " + entryPath;
    return false;
  }

  const auto entryEffects = resolveActiveEffects(entryDef.transforms, true, defaultEffects, entryDefaultEffects);
  if (!resolveEffectMask(entryDef.transforms, true, defaultEffects, entryDefaultEffects, entryEffectMaskOut, error)) {
    return false;
  }
  if (!resolveCapabilityMask(entryDef.transforms, entryEffects, entryPath, entryCapabilityMaskOut, error)) {
    return false;
  }
  return true;
}

bool resolveEffectMask(const std::vector<Transform> &transforms,
                       bool isEntry,
                       const std::vector<std::string> &defaultEffects,
                       const std::vector<std::string> &entryDefaultEffects,
                       uint64_t &maskOut,
                       std::string &error) {
  const auto effects = resolveActiveEffects(transforms, isEntry, defaultEffects, entryDefaultEffects);
  maskOut = 0;
  for (const auto &effect : effects) {
    uint64_t bit = 0;
    if (!effectBitForName(effect, bit)) {
      error = "unsupported effect in metadata: " + effect;
      return false;
    }
    maskOut |= bit;
  }
  return true;
}

bool resolveCapabilityMask(const std::vector<Transform> &transforms,
                           const std::unordered_set<std::string> &effects,
                           const std::string &duplicateContext,
                           uint64_t &maskOut,
                           std::string &error) {
  bool sawCapabilities = false;
  std::unordered_set<std::string> capabilities;
  for (const auto &transform : transforms) {
    if (transform.name != "capabilities") {
      continue;
    }
    if (sawCapabilities) {
      error = "duplicate capabilities transform on " + duplicateContext;
      return false;
    }
    sawCapabilities = true;
    capabilities.clear();
    for (const auto &arg : transform.arguments) {
      capabilities.insert(arg);
    }
  }

  if (!sawCapabilities) {
    capabilities = effects;
  }

  maskOut = 0;
  for (const auto &capability : capabilities) {
    uint64_t bit = 0;
    if (!effectBitForName(capability, bit)) {
      error = "unsupported capability in metadata: " + capability;
      return false;
    }
    maskOut |= bit;
  }
  return true;
}

} // namespace primec::ir_lowerer
