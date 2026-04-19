#include "IrLowererLowerEffects.h"

#include <cctype>
#include <functional>
#include <string_view>

#include "IrLowererHelpers.h"
#include "IrLowererSemanticProductTargetAdapters.h"
#include "primec/Ir.h"

namespace primec::ir_lowerer {
namespace {

bool isSoftwareNumericName(const std::string &name) {
  return name == "integer" || name == "decimal" || name == "complex";
}

bool isReflectionMetadataQueryName(const std::string &name) {
  return name == "type_name" || name == "type_kind" || name == "is_struct" || name == "field_count" ||
         name == "field_name" || name == "field_type" || name == "field_visibility" ||
         name == "has_transform" || name == "has_trait";
}

bool isReflectionMetadataQueryPath(const std::string &path) {
  constexpr std::string_view prefix = "/meta/";
  if (path.rfind(prefix, 0) != 0) {
    return false;
  }
  const std::string queryName = path.substr(prefix.size());
  if (queryName.empty() || queryName.find('/') != std::string::npos) {
    return false;
  }
  return isReflectionMetadataQueryName(queryName);
}

bool isRuntimeReflectionPath(const std::string &path) {
  return path == "/meta/object" || path == "/meta/table" || path.rfind("/meta/object/", 0) == 0 ||
         path.rfind("/meta/table/", 0) == 0;
}

void expandEffectImplications(std::unordered_set<std::string> &effects) {
  if (effects.count("file_write") != 0) {
    effects.insert("file_read");
  }
}

bool splitTopLevelTemplateArgs(const std::string &text, std::vector<std::string> &out) {
  out.clear();
  int depth = 0;
  size_t start = 0;
  auto pushSegment = [&](size_t end) {
    size_t segStart = start;
    while (segStart < end && std::isspace(static_cast<unsigned char>(text[segStart]))) {
      ++segStart;
    }
    size_t segEnd = end;
    while (segEnd > segStart && std::isspace(static_cast<unsigned char>(text[segEnd - 1]))) {
      --segEnd;
    }
    out.push_back(text.substr(segStart, segEnd - segStart));
  };
  for (size_t i = 0; i < text.size(); ++i) {
    char c = text[i];
    if (c == '<') {
      ++depth;
      continue;
    }
    if (c == '>') {
      if (depth > 0) {
        --depth;
      }
      continue;
    }
    if (c == ',' && depth == 0) {
      pushSegment(i);
      start = i + 1;
    }
  }
  pushSegment(text.size());
  for (const auto &seg : out) {
    if (seg.empty()) {
      return false;
    }
  }
  return !out.empty();
}

std::string findSoftwareNumericType(const std::string &typeName) {
  if (typeName.empty()) {
    return {};
  }
  std::string base;
  std::string arg;
  if (!splitTemplateTypeName(typeName, base, arg)) {
    return isSoftwareNumericName(typeName) ? typeName : std::string{};
  }
  if (isSoftwareNumericName(base)) {
    return base;
  }
  std::vector<std::string> args;
  if (!splitTopLevelTemplateArgs(arg, args)) {
    return {};
  }
  for (const auto &nested : args) {
    std::string found = findSoftwareNumericType(nested);
    if (!found.empty()) {
      return found;
    }
  }
  return {};
}

std::string scanTransformsForSoftwareNumeric(const std::vector<Transform> &transforms) {
  for (const auto &transform : transforms) {
    std::string found = findSoftwareNumericType(transform.name);
    if (!found.empty()) {
      return found;
    }
    for (const auto &arg : transform.templateArgs) {
      found = findSoftwareNumericType(arg);
      if (!found.empty()) {
        return found;
      }
    }
  }
  return {};
}

std::string scanExprForSoftwareNumeric(const Expr &expr) {
  std::string found = scanTransformsForSoftwareNumeric(expr.transforms);
  if (!found.empty()) {
    return found;
  }
  for (const auto &arg : expr.templateArgs) {
    found = findSoftwareNumericType(arg);
    if (!found.empty()) {
      return found;
    }
  }
  for (const auto &arg : expr.args) {
    found = scanExprForSoftwareNumeric(arg);
    if (!found.empty()) {
      return found;
    }
  }
  for (const auto &arg : expr.bodyArguments) {
    found = scanExprForSoftwareNumeric(arg);
    if (!found.empty()) {
      return found;
    }
  }
  return {};
}

std::string scanExprForRuntimeReflectionQuery(const Expr &expr) {
  if (expr.kind == Expr::Kind::Call &&
      (isReflectionMetadataQueryPath(expr.name) || isRuntimeReflectionPath(expr.name))) {
    return expr.name;
  }
  for (const auto &arg : expr.args) {
    std::string found = scanExprForRuntimeReflectionQuery(arg);
    if (!found.empty()) {
      return found;
    }
  }
  for (const auto &arg : expr.bodyArguments) {
    std::string found = scanExprForRuntimeReflectionQuery(arg);
    if (!found.empty()) {
      return found;
    }
  }
  return {};
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

bool validateNoSoftwareNumericTypes(const Program &program, std::string &error) {
  for (const auto &def : program.definitions) {
    std::string found = scanTransformsForSoftwareNumeric(def.transforms);
    if (!found.empty()) {
      error = "native backend does not support software numeric types: " + found;
      return false;
    }
    for (const auto &param : def.parameters) {
      found = scanExprForSoftwareNumeric(param);
      if (!found.empty()) {
        error = "native backend does not support software numeric types: " + found;
        return false;
      }
    }
    for (const auto &stmt : def.statements) {
      found = scanExprForSoftwareNumeric(stmt);
      if (!found.empty()) {
        error = "native backend does not support software numeric types: " + found;
        return false;
      }
    }
    if (def.returnExpr.has_value()) {
      found = scanExprForSoftwareNumeric(*def.returnExpr);
      if (!found.empty()) {
        error = "native backend does not support software numeric types: " + found;
        return false;
      }
    }
  }

  for (const auto &exec : program.executions) {
    std::string found = scanTransformsForSoftwareNumeric(exec.transforms);
    if (!found.empty()) {
      error = "native backend does not support software numeric types: " + found;
      return false;
    }
    for (const auto &arg : exec.arguments) {
      found = scanExprForSoftwareNumeric(arg);
      if (!found.empty()) {
        error = "native backend does not support software numeric types: " + found;
        return false;
      }
    }
    for (const auto &arg : exec.bodyArguments) {
      found = scanExprForSoftwareNumeric(arg);
      if (!found.empty()) {
        error = "native backend does not support software numeric types: " + found;
        return false;
      }
    }
  }

  return true;
}

bool validateNoRuntimeReflectionQueries(const Program &program, std::string &error) {
  for (const auto &def : program.definitions) {
    for (const auto &param : def.parameters) {
      const std::string found = scanExprForRuntimeReflectionQuery(param);
      if (!found.empty()) {
        if (isRuntimeReflectionPath(found)) {
          error = "runtime reflection objects/tables are unsupported: " + found;
        } else {
          error = "native backend requires compile-time reflection query elimination before IR emission: " + found;
        }
        return false;
      }
    }
    for (const auto &stmt : def.statements) {
      const std::string found = scanExprForRuntimeReflectionQuery(stmt);
      if (!found.empty()) {
        if (isRuntimeReflectionPath(found)) {
          error = "runtime reflection objects/tables are unsupported: " + found;
        } else {
          error = "native backend requires compile-time reflection query elimination before IR emission: " + found;
        }
        return false;
      }
    }
    if (def.returnExpr.has_value()) {
      const std::string found = scanExprForRuntimeReflectionQuery(*def.returnExpr);
      if (!found.empty()) {
        if (isRuntimeReflectionPath(found)) {
          error = "runtime reflection objects/tables are unsupported: " + found;
        } else {
          error = "native backend requires compile-time reflection query elimination before IR emission: " + found;
        }
        return false;
      }
    }
  }

  for (const auto &exec : program.executions) {
    for (const auto &arg : exec.arguments) {
      const std::string found = scanExprForRuntimeReflectionQuery(arg);
      if (!found.empty()) {
        if (isRuntimeReflectionPath(found)) {
          error = "runtime reflection objects/tables are unsupported: " + found;
        } else {
          error = "native backend requires compile-time reflection query elimination before IR emission: " + found;
        }
        return false;
      }
    }
    for (const auto &arg : exec.bodyArguments) {
      const std::string found = scanExprForRuntimeReflectionQuery(arg);
      if (!found.empty()) {
        if (isRuntimeReflectionPath(found)) {
          error = "runtime reflection objects/tables are unsupported: " + found;
        } else {
          error = "native backend requires compile-time reflection query elimination before IR emission: " + found;
        }
        return false;
      }
    }
  }

  return true;
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
  for (const auto &transform : transforms) {
    if (transform.name != "effects") {
      continue;
    }
    for (const auto &effect : transform.arguments) {
      if (!isSupportedEffect(effect)) {
        error = "native backend does not support effect: " + effect + " on " + context;
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
  const auto effects = resolveActiveEffects(transforms, isEntry, defaultEffects, entryDefaultEffects);
  for (const auto &effect : effects) {
    if (!isSupportedEffect(effect)) {
      error = "native backend does not support effect: " + effect + " on " + context;
      return false;
    }
  }
  return true;
}

bool validateProgramEffects(const Program &program,
                            const std::string &entryPath,
                            const std::vector<std::string> &defaultEffects,
                            const std::vector<std::string> &entryDefaultEffects,
                            std::string &error) {
  return validateProgramEffects(program, nullptr, entryPath, defaultEffects, entryDefaultEffects, error);
}

bool validateProgramEffects(const Program &program,
                            const SemanticProgram *semanticProgram,
                            const std::string &entryPath,
                            const std::vector<std::string> &defaultEffects,
                            const std::vector<std::string> &entryDefaultEffects,
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
            error = "native backend does not support effect: " + effect + " on " + context;
            return false;
          }
        }
        return true;
      }
      error = "missing semantic-product callable summary: " + fullPath;
      return false;
    }
    return validateActiveEffects(transforms, context, isEntry, defaultEffects, entryDefaultEffects, error);
  };

  const auto validateExprEffects = [&](const auto &self, const Expr &expr, const std::string &context) -> bool {
    if (!validateEffectsTransforms(expr.transforms, context, error)) {
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
    if (!validateCallableEffects(def.fullPath, def.transforms, def.fullPath == entryPath, def.fullPath)) {
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
