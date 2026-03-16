#include "SemanticsValidator.h"

#include <array>
#include <cctype>
#include <string_view>
#include <utility>

namespace primec::semantics {

bool SemanticsValidator::buildDefinitionMaps() {
  defaultEffectSet_.clear();
  entryDefaultEffectSet_.clear();
  defMap_.clear();
  returnKinds_.clear();
  returnStructs_.clear();
  structNames_.clear();
  publicDefinitions_.clear();
  paramsByDef_.clear();

  auto isMathBuiltinName = [&](const std::string &name) -> bool {
    Expr probe;
    probe.name = name;
    std::string builtinName;
    return getBuiltinMathName(probe, builtinName, true) || getBuiltinClampName(probe, builtinName, true) ||
           getBuiltinMinMaxName(probe, builtinName, true) || getBuiltinAbsSignName(probe, builtinName, true) ||
           getBuiltinSaturateName(probe, builtinName, true) || isBuiltinMathConstant(name, true);
  };
  const auto isGeneratedTemplateSpecializationName = [](const std::string &name) {
    const size_t marker = name.rfind("__t");
    if (marker == std::string::npos || marker + 3 >= name.size()) {
      return false;
    }
    for (size_t i = marker + 3; i < name.size(); ++i) {
      if (!std::isxdigit(static_cast<unsigned char>(name[i]))) {
        return false;
      }
    }
    return true;
  };

  for (const auto &effect : defaultEffects_) {
    if (!isEffectName(effect)) {
      error_ = "invalid default effect: " + effect;
      return false;
    }
    defaultEffectSet_.insert(effect);
  }
  for (const auto &effect : entryDefaultEffects_) {
    if (!isEffectName(effect)) {
      error_ = "invalid entry default effect: " + effect;
      return false;
    }
    entryDefaultEffectSet_.insert(effect);
  }

  std::unordered_set<std::string> explicitStructs;
  struct HelperSuffixInfo {
    std::string_view suffix;
    std::string_view placement;
  };
  auto isStructDefinition = [&](const Definition &def, bool &isExplicitOut) {
    bool hasStruct = false;
    bool hasReturn = false;
    for (const auto &transform : def.transforms) {
      if (transform.name == "return") {
        hasReturn = true;
      }
      if (isStructTransformName(transform.name)) {
        hasStruct = true;
      }
    }
    if (hasStruct) {
      isExplicitOut = true;
      return true;
    }
    if (hasReturn || !def.parameters.empty() || def.hasReturnStatement || def.returnExpr.has_value()) {
      isExplicitOut = false;
      return false;
    }
    for (const auto &stmt : def.statements) {
      if (!stmt.isBinding) {
        isExplicitOut = false;
        return false;
      }
    }
    isExplicitOut = false;
    return true;
  };
  for (const auto &def : program_.definitions) {
    DefinitionContextScope definitionScope(*this, def);
    bool isExplicit = false;
    if (isStructDefinition(def, isExplicit)) {
      structNames_.insert(def.fullPath);
      if (isExplicit) {
        explicitStructs.insert(def.fullPath);
      }
    }
  }

  auto isLifecycleHelperName = [](const std::string &fullPath) -> bool {
    static const std::array<HelperSuffixInfo, 10> suffixes = {{
        {"Create", ""},
        {"Destroy", ""},
        {"Copy", ""},
        {"Move", ""},
        {"CreateStack", "stack"},
        {"DestroyStack", "stack"},
        {"CreateHeap", "heap"},
        {"DestroyHeap", "heap"},
        {"CreateBuffer", "buffer"},
        {"DestroyBuffer", "buffer"},
    }};
    for (const auto &info : suffixes) {
      const std::string_view suffix = info.suffix;
      if (fullPath.size() < suffix.size() + 1) {
        continue;
      }
      const size_t suffixStart = fullPath.size() - suffix.size();
      if (fullPath[suffixStart - 1] != '/') {
        continue;
      }
      if (fullPath.compare(suffixStart, suffix.size(), suffix.data(), suffix.size()) != 0) {
        continue;
      }
      return true;
    }
    return false;
  };
  auto isStructHelperDefinition = [&](const Definition &def) -> bool {
    if (!def.isNested) {
      return false;
    }
    if (structNames_.count(def.fullPath) > 0) {
      return false;
    }
    const size_t slash = def.fullPath.find_last_of('/');
    if (slash == std::string::npos || slash == 0) {
      return false;
    }
    const std::string parent = def.fullPath.substr(0, slash);
    return structNames_.count(parent) > 0;
  };
  std::vector<SemanticDiagnosticRecord> transformDiagnosticRecords;
  const bool collectTransformDiagnostics = collectDiagnostics_ && diagnosticInfo_ != nullptr;
  for (const auto &def : program_.definitions) {
    DefinitionContextScope definitionScope(*this, def);
    if (defMap_.count(def.fullPath) > 0) {
      error_ = "duplicate definition: " + def.fullPath;
      return false;
    }
    if (def.fullPath.find('/', 1) == std::string::npos) {
      const std::string rootName = def.fullPath.substr(1);
      if ((mathImportAll_ || mathImports_.count(rootName) > 0) && isMathBuiltinName(rootName)) {
        error_ = "import creates name conflict: " + rootName;
        return false;
      }
    }
    std::unordered_set<std::string> seenEffects;
    bool sawCapabilities = false;
    bool sawOnError = false;
    bool sawCompute = false;
    bool sawUnsafe = false;
    bool sawWorkgroupSize = false;
    bool sawNoPadding = false;
    bool sawPlatformPadding = false;
    const bool isStructHelper = isStructHelperDefinition(def);
    const bool isLifecycleHelper = isLifecycleHelperName(def.fullPath);
    bool sawVisibility = false;
    bool isPublic = false;
    bool sawStatic = false;
    bool sawReflect = false;
    bool sawGenerate = false;
    bool definitionTransformError = false;
    auto addTransformDiagnostic = [&](const std::string &message) -> bool {
      if (!collectTransformDiagnostics) {
        error_ = message;
        return true;
      }
      SemanticDiagnosticRecord record;
      record.message = message;
      if (def.sourceLine > 0 && def.sourceColumn > 0) {
        record.line = def.sourceLine;
        record.column = def.sourceColumn;
      }
      SemanticDiagnosticRelatedSpan related;
      related.line = def.sourceLine;
      related.column = def.sourceColumn;
      related.label = "definition: " + def.fullPath;
      record.relatedSpans.push_back(std::move(related));
      if (error_.empty()) {
        error_ = message;
      }
      transformDiagnosticRecords.push_back(std::move(record));
      definitionTransformError = true;
      return false;
    };
    for (const auto &transform : def.transforms) {
      if (transform.name == "public" || transform.name == "private") {
        if (!transform.templateArgs.empty()) {
          if (addTransformDiagnostic(transform.name + " transform does not accept template arguments on " + def.fullPath)) {
            return false;
          }
          break;
        }
        if (!transform.arguments.empty()) {
          if (addTransformDiagnostic(transform.name + " transform does not accept arguments on " + def.fullPath)) {
            return false;
          }
          break;
        }
        if (sawVisibility) {
          if (addTransformDiagnostic("definition visibility transforms are mutually exclusive: " + def.fullPath)) {
            return false;
          }
          break;
        }
        sawVisibility = true;
        isPublic = (transform.name == "public");
        continue;
      }
      if (transform.name == "static") {
        if (!transform.templateArgs.empty()) {
          if (addTransformDiagnostic("static transform does not accept template arguments on " + def.fullPath)) {
            return false;
          }
          break;
        }
        if (!transform.arguments.empty()) {
          if (addTransformDiagnostic("static transform does not accept arguments on " + def.fullPath)) {
            return false;
          }
          break;
        }
        if (sawStatic) {
          if (addTransformDiagnostic("duplicate static transform on " + def.fullPath)) {
            return false;
          }
          break;
        }
        sawStatic = true;
        if (!isStructHelper || isLifecycleHelper) {
          if (addTransformDiagnostic("binding visibility/static transforms are only valid on bindings: " + def.fullPath)) {
            return false;
          }
          break;
        }
        continue;
      }
      if (transform.name == "no_padding" || transform.name == "platform_independent_padding") {
        if (!transform.templateArgs.empty()) {
          if (addTransformDiagnostic(transform.name + " transform does not accept template arguments on " + def.fullPath)) {
            return false;
          }
          break;
        }
        if (!transform.arguments.empty()) {
          if (addTransformDiagnostic(transform.name + " transform does not accept arguments on " + def.fullPath)) {
            return false;
          }
          break;
        }
        if (transform.name == "no_padding") {
          if (sawNoPadding) {
            if (addTransformDiagnostic("duplicate no_padding transform on " + def.fullPath)) {
              return false;
            }
            break;
          }
          sawNoPadding = true;
        } else {
          if (sawPlatformPadding) {
            if (addTransformDiagnostic("duplicate platform_independent_padding transform on " + def.fullPath)) {
              return false;
            }
            break;
          }
          sawPlatformPadding = true;
        }
        continue;
      }
      if (isBindingQualifierName(transform.name)) {
        if (addTransformDiagnostic("binding visibility/static transforms are only valid on bindings: " + def.fullPath)) {
          return false;
        }
        break;
      }
      if (transform.name == "copy") {
        if (addTransformDiagnostic("copy transform is only supported on bindings and parameters: " + def.fullPath)) {
          return false;
        }
        break;
      }
      if (transform.name == "restrict") {
        if (addTransformDiagnostic("restrict transform is only supported on bindings and parameters: " + def.fullPath)) {
          return false;
        }
        break;
      }
      if (transform.name == "return") {
        if (!transform.arguments.empty()) {
          if (addTransformDiagnostic("return transform does not accept arguments on " + def.fullPath)) {
            return false;
          }
          break;
        }
      } else if (transform.name == "on_error") {
        if (sawOnError) {
          if (addTransformDiagnostic("duplicate on_error transform on " + def.fullPath)) {
            return false;
          }
          break;
        }
        sawOnError = true;
        if (transform.templateArgs.size() != 2) {
          if (addTransformDiagnostic("on_error requires exactly two template arguments on " + def.fullPath)) {
            return false;
          }
          break;
        }
      } else if (transform.name == "compute") {
        if (sawCompute) {
          if (addTransformDiagnostic("duplicate compute transform on " + def.fullPath)) {
            return false;
          }
          break;
        }
        sawCompute = true;
        if (!transform.templateArgs.empty()) {
          if (addTransformDiagnostic("compute does not accept template arguments on " + def.fullPath)) {
            return false;
          }
          break;
        }
        if (!transform.arguments.empty()) {
          if (addTransformDiagnostic("compute does not accept arguments on " + def.fullPath)) {
            return false;
          }
          break;
        }
      } else if (transform.name == "unsafe") {
        if (sawUnsafe) {
          if (addTransformDiagnostic("duplicate unsafe transform on " + def.fullPath)) {
            return false;
          }
          break;
        }
        sawUnsafe = true;
        if (!transform.templateArgs.empty()) {
          if (addTransformDiagnostic("unsafe does not accept template arguments on " + def.fullPath)) {
            return false;
          }
          break;
        }
        if (!transform.arguments.empty()) {
          if (addTransformDiagnostic("unsafe does not accept arguments on " + def.fullPath)) {
            return false;
          }
          break;
        }
      } else if (transform.name == "workgroup_size") {
        if (sawWorkgroupSize) {
          if (addTransformDiagnostic("duplicate workgroup_size transform on " + def.fullPath)) {
            return false;
          }
          break;
        }
        sawWorkgroupSize = true;
        if (!transform.templateArgs.empty()) {
          if (addTransformDiagnostic("workgroup_size does not accept template arguments on " + def.fullPath)) {
            return false;
          }
          break;
        }
        if (transform.arguments.size() != 3) {
          if (addTransformDiagnostic("workgroup_size requires three arguments on " + def.fullPath)) {
            return false;
          }
          break;
        }
        int value = 0;
        for (const auto &arg : transform.arguments) {
          if (!parsePositiveIntArg(arg, value)) {
            if (addTransformDiagnostic("workgroup_size requires positive integer arguments on " + def.fullPath)) {
              return false;
            }
            break;
          }
        }
        if (definitionTransformError) {
          break;
        }
      } else if (transform.name == "stack" || transform.name == "heap" || transform.name == "buffer") {
        if (addTransformDiagnostic("placement transforms are not supported: " + def.fullPath)) {
          return false;
        }
        break;
      } else if (transform.name == "effects") {
        if (!validateEffectsTransform(transform, def.fullPath, error_)) {
          if (addTransformDiagnostic(error_)) {
            return false;
          }
          break;
        }
        for (const auto &effect : transform.arguments) {
          if (!seenEffects.insert(effect).second) {
            if (addTransformDiagnostic("duplicate effects transform on " + def.fullPath)) {
              return false;
            }
            break;
          }
        }
        if (definitionTransformError) {
          break;
        }
      } else if (transform.name == "capabilities") {
        if (sawCapabilities) {
          if (addTransformDiagnostic("duplicate capabilities transform on " + def.fullPath)) {
            return false;
          }
          break;
        }
        sawCapabilities = true;
        if (!validateCapabilitiesTransform(transform, def.fullPath, error_)) {
          if (addTransformDiagnostic(error_)) {
            return false;
          }
          break;
        }
      } else if (transform.name == "align_bytes" || transform.name == "align_kbytes") {
        if (!validateAlignTransform(transform, def.fullPath, error_)) {
          if (addTransformDiagnostic(error_)) {
            return false;
          }
          break;
        }
      } else if (transform.name == "reflect") {
        if (sawReflect) {
          if (addTransformDiagnostic("duplicate reflect transform on " + def.fullPath)) {
            return false;
          }
          break;
        }
        sawReflect = true;
        if (!transform.templateArgs.empty()) {
          if (addTransformDiagnostic("reflect transform does not accept template arguments on " + def.fullPath)) {
            return false;
          }
          break;
        }
        if (!transform.arguments.empty()) {
          if (addTransformDiagnostic("reflect transform does not accept arguments on " + def.fullPath)) {
            return false;
          }
          break;
        }
      } else if (transform.name == "generate") {
        if (sawGenerate) {
          if (addTransformDiagnostic("duplicate generate transform on " + def.fullPath)) {
            return false;
          }
          break;
        }
        sawGenerate = true;
        if (!transform.templateArgs.empty()) {
          if (addTransformDiagnostic("generate transform does not accept template arguments on " + def.fullPath)) {
            return false;
          }
          break;
        }
        if (transform.arguments.empty()) {
          if (addTransformDiagnostic("generate transform requires at least one generator on " + def.fullPath)) {
            return false;
          }
          break;
        }
        std::unordered_set<std::string> seenGenerators;
        for (const auto &generator : transform.arguments) {
          if (!isSupportedReflectionGeneratorName(generator)) {
            if (generator == "ToString") {
              if (addTransformDiagnostic("reflection generator ToString is deferred on " + def.fullPath +
                                         ": dynamic string construction/runtime support is not available; use DebugPrint")) {
                return false;
              }
              break;
            }
            if (addTransformDiagnostic("unsupported reflection generator on " + def.fullPath + ": " + generator)) {
              return false;
            }
            break;
          }
          if (!seenGenerators.insert(generator).second) {
            if (addTransformDiagnostic("duplicate reflection generator on " + def.fullPath + ": " + generator)) {
              return false;
            }
            break;
          }
        }
        if (definitionTransformError) {
          break;
        }
      } else if (isStructTransformName(transform.name)) {
        if (!transform.templateArgs.empty()) {
          if (addTransformDiagnostic("struct transform does not accept template arguments on " + def.fullPath)) {
            return false;
          }
          break;
        }
        if (!transform.arguments.empty()) {
          if (addTransformDiagnostic("struct transform does not accept arguments on " + def.fullPath)) {
            return false;
          }
          break;
        }
      }
    }
    if (definitionTransformError) {
      continue;
    }
    if (sawWorkgroupSize && !sawCompute) {
      bool hasCompute = false;
      for (const auto &transform : def.transforms) {
        if (transform.name == "compute") {
          hasCompute = true;
          break;
        }
      }
      if (!hasCompute) {
        if (addTransformDiagnostic("workgroup_size is only valid on compute definitions: " + def.fullPath)) {
          return false;
        }
        continue;
      }
    }
    if (sawNoPadding && sawPlatformPadding) {
      if (addTransformDiagnostic("no_padding and platform_independent_padding are mutually exclusive on " + def.fullPath)) {
        return false;
      }
      continue;
    }
    bool isStruct = false;
    bool hasReturnTransform = false;
    bool hasPod = false;
    bool hasHandle = false;
    bool hasGpuLane = false;
    for (const auto &transform : def.transforms) {
      if (transform.name == "return") {
        hasReturnTransform = true;
      }
      if (transform.name == "pod") {
        hasPod = true;
      } else if (transform.name == "handle") {
        hasHandle = true;
      } else if (transform.name == "gpu_lane") {
        hasGpuLane = true;
      }
      if (isStructTransformName(transform.name)) {
        isStruct = true;
      }
    }
    if (hasPod && (hasHandle || hasGpuLane)) {
      error_ = "pod definitions cannot be tagged as handle or gpu_lane: " + def.fullPath;
      return false;
    }
    if (hasHandle && hasGpuLane) {
      error_ = "handle definitions cannot be tagged as gpu_lane: " + def.fullPath;
      return false;
    }
    bool isFieldOnlyStruct = false;
    if (!isStruct && !hasReturnTransform && def.parameters.empty() && !def.hasReturnStatement &&
        !def.returnExpr.has_value()) {
      isFieldOnlyStruct = true;
      for (const auto &stmt : def.statements) {
        if (!stmt.isBinding) {
          isFieldOnlyStruct = false;
          break;
        }
      }
    }
    if (isStruct || isFieldOnlyStruct) {
      structNames_.insert(def.fullPath);
    }
    if ((sawReflect || sawGenerate) && !isStruct && !isFieldOnlyStruct) {
      if (addTransformDiagnostic("reflection transforms are only valid on struct definitions: " + def.fullPath)) {
        return false;
      }
      continue;
    }
    if (sawGenerate && !sawReflect) {
      if (addTransformDiagnostic("generate transform requires reflect on " + def.fullPath)) {
        return false;
      }
      continue;
    }
    if (isStruct) {
      explicitStructs.insert(def.fullPath);
      if (hasReturnTransform) {
        error_ = "struct definitions cannot declare return types: " + def.fullPath;
        return false;
      }
      if (def.hasReturnStatement) {
        error_ = "struct definitions cannot contain return statements: " + def.fullPath;
        return false;
      }
      if (def.returnExpr.has_value()) {
        error_ = "struct definitions cannot return a value: " + def.fullPath;
        return false;
      }
      if (!def.parameters.empty()) {
        error_ = "struct definitions cannot declare parameters: " + def.fullPath;
        return false;
      }
      for (const auto &stmt : def.statements) {
        if (!stmt.isBinding) {
          error_ = "struct definitions may only contain field bindings: " + def.fullPath;
          return false;
        }
      }
    }
    if (isStruct || isFieldOnlyStruct) {
      for (const auto &stmt : def.statements) {
        if (stmt.isBinding) {
          bool fieldHasHandle = false;
          bool fieldHasGpuLane = false;
          bool fieldHasPod = false;
          for (const auto &transform : stmt.transforms) {
            if (transform.name == "handle") {
              fieldHasHandle = true;
            } else if (transform.name == "pod") {
              fieldHasPod = true;
            } else if (transform.name == "gpu_lane") {
              fieldHasGpuLane = true;
            }
          }
          if (hasPod && (fieldHasHandle || fieldHasGpuLane)) {
            error_ = "pod definitions cannot contain handle or gpu_lane fields: " + def.fullPath;
            return false;
          }
          if (fieldHasPod && fieldHasHandle) {
            error_ = "fields cannot be tagged as pod and handle: " + def.fullPath;
            return false;
          }
          if (fieldHasPod && fieldHasGpuLane) {
            error_ = "fields cannot be tagged as pod and gpu_lane: " + def.fullPath;
            return false;
          }
          if (fieldHasHandle && fieldHasGpuLane) {
            error_ = "fields cannot be tagged as handle and gpu_lane: " + def.fullPath;
            return false;
          }
        }
      }
    }
    if (isPublic) {
      publicDefinitions_.insert(def.fullPath);
    }
    defMap_[def.fullPath] = &def;
  }
  if (collectTransformDiagnostics && !transformDiagnosticRecords.empty()) {
    diagnosticInfo_->records = std::move(transformDiagnosticRecords);
    if (!diagnosticInfo_->records.empty()) {
      diagnosticInfo_->line = diagnosticInfo_->records.front().line;
      diagnosticInfo_->column = diagnosticInfo_->records.front().column;
      diagnosticInfo_->relatedSpans = diagnosticInfo_->records.front().relatedSpans;
    }
    return false;
  }

  importAliases_.clear();
  std::vector<SemanticDiagnosticRecord> importDiagnosticRecords;
  auto collectImportDiagnostics = collectDiagnostics_ && diagnosticInfo_ != nullptr;
  auto addImportDiagnostic = [&](const std::string &message, const Definition *relatedDef = nullptr) {
    if (!collectImportDiagnostics) {
      error_ = message;
      return true;
    }
    SemanticDiagnosticRecord record;
    record.message = message;
    if (relatedDef != nullptr && relatedDef->sourceLine > 0 && relatedDef->sourceColumn > 0) {
      SemanticDiagnosticRelatedSpan related;
      related.line = relatedDef->sourceLine;
      related.column = relatedDef->sourceColumn;
      related.label = "definition: " + relatedDef->fullPath;
      record.relatedSpans.push_back(std::move(related));
    }
    if (error_.empty()) {
      error_ = message;
    }
    importDiagnosticRecords.push_back(std::move(record));
    return false;
  };

  for (const auto &importPath : program_.imports) {
    if (importPath == "/std/math") {
      if (addImportDiagnostic("import /std/math is not supported; use import /std/math/* or /std/math/<name>")) {
        return false;
      }
      continue;
    }
    if (importPath.empty() || importPath[0] != '/') {
      if (addImportDiagnostic("import path must be a slash path")) {
        return false;
      }
      continue;
    }
    bool isWildcard = false;
    std::string prefix;
    if (importPath.size() >= 2 && importPath.compare(importPath.size() - 2, 2, "/*") == 0) {
      isWildcard = true;
      prefix = importPath.substr(0, importPath.size() - 2);
    } else if (importPath.find('/', 1) == std::string::npos) {
      isWildcard = true;
      prefix = importPath;
    }
    if (isWildcard) {
      const std::string scopedPrefix = prefix + "/";
      bool sawImmediateDefinition = false;
      bool importError = false;
      for (const auto &def : program_.definitions) {
        DefinitionContextScope definitionScope(*this, def);
        if (def.fullPath.rfind(scopedPrefix, 0) != 0) {
          continue;
        }
        const std::string remainder = def.fullPath.substr(scopedPrefix.size());
        if (remainder.empty() || remainder.find('/') != std::string::npos) {
          continue;
        }
        if (publicDefinitions_.count(def.fullPath) == 0) {
          continue;
        }
        sawImmediateDefinition = true;
        if (isGeneratedTemplateSpecializationName(remainder)) {
          continue;
        }
        if (isRootBuiltinName(remainder)) {
          if (addImportDiagnostic("import creates name conflict: " + remainder, &def)) {
            return false;
          }
          importError = true;
          break;
        }
        if (allowMathBareName(remainder) && isMathBuiltinName(remainder)) {
          if (addImportDiagnostic("import creates name conflict: " + remainder, &def)) {
            return false;
          }
          importError = true;
          break;
        }
        const std::string rootPath = "/" + remainder;
        auto rootIt = defMap_.find(rootPath);
        if (rootIt != defMap_.end()) {
          if (addImportDiagnostic("import creates name conflict: " + remainder, rootIt->second)) {
            return false;
          }
          importError = true;
          break;
        }
        auto [it, inserted] = importAliases_.emplace(remainder, def.fullPath);
        if (!inserted && it->second != def.fullPath) {
          if (addImportDiagnostic("import creates name conflict: " + remainder, &def)) {
            return false;
          }
          importError = true;
          break;
        }
      }
      if (importError) {
        continue;
      }
      if (!sawImmediateDefinition && prefix != "/std/math") {
        if (addImportDiagnostic("unknown import path: " + importPath)) {
          return false;
        }
        continue;
      }
      continue;
    }
    bool isMathBuiltinImport = false;
    if (importPath.rfind("/std/math/", 0) == 0 && importPath.size() > 10) {
      std::string name = importPath.substr(10);
      if (name.find('/') == std::string::npos && name != "*" && isMathBuiltinName(name)) {
        isMathBuiltinImport = true;
      }
    }
    auto defIt = defMap_.find(importPath);
    if (defIt == defMap_.end()) {
      if (!isMathBuiltinImport) {
        if (addImportDiagnostic("unknown import path: " + importPath)) {
          return false;
        }
      }
      continue;
    }
    if (publicDefinitions_.count(importPath) == 0) {
      if (addImportDiagnostic("import path refers to private definition: " + importPath, defIt->second)) {
        return false;
      }
      continue;
    }
    const std::string remainder = importPath.substr(importPath.find_last_of('/') + 1);
    if (remainder.empty()) {
      continue;
    }
    if (isRootBuiltinName(remainder)) {
      if (addImportDiagnostic("import creates name conflict: " + remainder, defIt->second)) {
        return false;
      }
      continue;
    }
    if (allowMathBareName(remainder) && isMathBuiltinName(remainder)) {
      if (addImportDiagnostic("import creates name conflict: " + remainder, defIt->second)) {
        return false;
      }
      continue;
    }
    const std::string rootPath = "/" + remainder;
    auto rootIt = defMap_.find(rootPath);
    if (rootIt != defMap_.end()) {
      if (addImportDiagnostic("import creates name conflict: " + remainder, rootIt->second)) {
        return false;
      }
      continue;
    }
    auto [it, inserted] = importAliases_.emplace(remainder, importPath);
    if (!inserted && it->second != importPath) {
      if (addImportDiagnostic("import creates name conflict: " + remainder, defIt->second)) {
        return false;
      }
      continue;
    }
  }

  if (collectImportDiagnostics && !importDiagnosticRecords.empty()) {
    diagnosticInfo_->records = std::move(importDiagnosticRecords);
    if (!diagnosticInfo_->records.empty()) {
      diagnosticInfo_->line = diagnosticInfo_->records.front().line;
      diagnosticInfo_->column = diagnosticInfo_->records.front().column;
      diagnosticInfo_->relatedSpans = diagnosticInfo_->records.front().relatedSpans;
    }
    return false;
  }

  auto resolveStructReturnPath = [&](const std::string &typeName,
                                     const std::string &namespacePrefix) -> std::string {
    if (typeName.empty()) {
      return "";
    }
    auto normalizeCollectionPath = [](const std::string &baseName) -> std::string {
      if (baseName == "array" || baseName == "vector" || baseName == "string") {
        return "/" + baseName;
      }
      if (isMapCollectionTypeName(baseName)) {
        return "/map";
      }
      return "";
    };
    std::string normalizedType = normalizeBindingTypeName(typeName);
    if (std::string collectionPath = normalizeCollectionPath(normalizedType); !collectionPath.empty()) {
      return collectionPath;
    }
    std::string collectionBase;
    std::string collectionArgs;
    if (splitTemplateTypeName(normalizedType, collectionBase, collectionArgs)) {
      std::vector<std::string> args;
      if (splitTopLevelTemplateArgs(collectionArgs, args)) {
        if (collectionBase == "array" && args.size() == 1) {
          return "/array";
        }
        if (collectionBase == "vector" && args.size() == 1) {
          return "/vector";
        }
        if (isMapCollectionTypeName(collectionBase) && args.size() == 2) {
          return "/map";
        }
      }
    }
    if (!typeName.empty() && typeName[0] == '/') {
      return structNames_.count(typeName) > 0 ? typeName : "";
    }
    std::string current = namespacePrefix;
    while (true) {
      if (!current.empty()) {
        std::string direct = current + "/" + typeName;
        if (structNames_.count(direct) > 0) {
          return direct;
        }
        if (current.size() > typeName.size()) {
          const size_t start = current.size() - typeName.size();
          if (start > 0 && current[start - 1] == '/' &&
              current.compare(start, typeName.size(), typeName) == 0 &&
              structNames_.count(current) > 0) {
            return current;
          }
        }
      } else {
        std::string root = "/" + typeName;
        if (structNames_.count(root) > 0) {
          return root;
        }
      }
      if (current.empty()) {
        break;
      }
      const size_t slash = current.find_last_of('/');
      if (slash == std::string::npos || slash == 0) {
        current.clear();
      } else {
        current.erase(slash);
      }
    }
    auto importIt = importAliases_.find(typeName);
    if (importIt != importAliases_.end() && structNames_.count(importIt->second) > 0) {
      return importIt->second;
    }
    if (!typeName.empty() && typeName.front() != '/') {
      const std::string suffix = "/" + typeName;
      std::string uniqueMatch;
      for (const auto &path : structNames_) {
        if (path.size() < suffix.size() ||
            path.compare(path.size() - suffix.size(), suffix.size(), suffix) != 0) {
          continue;
        }
        if (!uniqueMatch.empty() && uniqueMatch != path) {
          return "";
        }
        uniqueMatch = path;
      }
      if (!uniqueMatch.empty()) {
        return uniqueMatch;
      }
    }
    return "";
  };

  for (const auto &def : program_.definitions) {
    DefinitionContextScope definitionScope(*this, def);
    if (structNames_.count(def.fullPath) > 0) {
      returnStructs_[def.fullPath] = def.fullPath;
      continue;
    }
    for (const auto &transform : def.transforms) {
      if (transform.name != "return" || transform.templateArgs.size() != 1) {
        continue;
      }
      const std::string &typeName = transform.templateArgs.front();
      std::string structPath = resolveStructReturnPath(typeName, def.namespacePrefix);
      if (!structPath.empty()) {
        returnStructs_[def.fullPath] = structPath;
      }
      break;
    }
  }

  std::vector<SemanticDiagnosticRecord> returnKindDiagnosticRecords;
  const bool collectReturnKindDiagnostics = collectDiagnostics_ && diagnosticInfo_ != nullptr;
  for (const auto &def : program_.definitions) {
    DefinitionContextScope definitionScope(*this, def);
    ReturnKind kind = ReturnKind::Void;
    if (explicitStructs.count(def.fullPath) > 0) {
      kind = ReturnKind::Array;
    } else {
      std::string returnKindError;
      for (const auto &transform : def.transforms) {
        if (transform.name != "return" || transform.templateArgs.size() != 1) {
          continue;
        }
        std::string base;
        std::string arg;
        if (!splitTemplateTypeName(transform.templateArgs.front(), base, arg) || base != "soa_vector") {
          break;
        }
        std::vector<std::string> args;
        if (!splitTopLevelTemplateArgs(arg, args) || args.size() != 1) {
          break;
        }
        if (!isSoaVectorStructElementType(args.front(), def.namespacePrefix, structNames_, importAliases_)) {
          break;
        }
        if (!validateSoaVectorElementFieldEnvelopes(args.front(), def.namespacePrefix)) {
          returnKindError = error_;
        }
        break;
      }
      if (returnKindError.empty()) {
        kind = getReturnKind(def, structNames_, importAliases_, returnKindError);
      }
      if (!returnKindError.empty()) {
        if (!collectReturnKindDiagnostics) {
          error_ = returnKindError;
          return false;
        }
        SemanticDiagnosticRecord record;
        record.message = returnKindError;
        if (def.sourceLine > 0 && def.sourceColumn > 0) {
          record.line = def.sourceLine;
          record.column = def.sourceColumn;
        }
        SemanticDiagnosticRelatedSpan related;
        related.line = def.sourceLine;
        related.column = def.sourceColumn;
        related.label = "definition: " + def.fullPath;
        record.relatedSpans.push_back(std::move(related));
        if (error_.empty()) {
          error_ = returnKindError;
        }
        returnKindDiagnosticRecords.push_back(std::move(record));
        continue;
      }
    }
    returnKinds_[def.fullPath] = kind;
  }
  if (collectReturnKindDiagnostics && !returnKindDiagnosticRecords.empty()) {
    diagnosticInfo_->records = std::move(returnKindDiagnosticRecords);
    if (!diagnosticInfo_->records.empty()) {
      diagnosticInfo_->line = diagnosticInfo_->records.front().line;
      diagnosticInfo_->column = diagnosticInfo_->records.front().column;
      diagnosticInfo_->relatedSpans = diagnosticInfo_->records.front().relatedSpans;
    }
    return false;
  }

  auto isLifecycleHelper =
      [](const std::string &fullPath, std::string &parentOut, std::string &placementOut) -> bool {
    static const std::array<HelperSuffixInfo, 10> suffixes = {{
        {"Create", ""},
        {"Destroy", ""},
        {"Copy", ""},
        {"Move", ""},
        {"CreateStack", "stack"},
        {"DestroyStack", "stack"},
        {"CreateHeap", "heap"},
        {"DestroyHeap", "heap"},
        {"CreateBuffer", "buffer"},
        {"DestroyBuffer", "buffer"},
    }};
    for (const auto &info : suffixes) {
      const std::string_view suffix = info.suffix;
      if (fullPath.size() < suffix.size() + 1) {
        continue;
      }
      const size_t suffixStart = fullPath.size() - suffix.size();
      if (fullPath[suffixStart - 1] != '/') {
        continue;
      }
      if (fullPath.compare(suffixStart, suffix.size(), suffix.data(), suffix.size()) != 0) {
        continue;
      }
      parentOut = fullPath.substr(0, suffixStart - 1);
      placementOut = std::string(info.placement);
      return true;
    }
    return false;
  };
  auto isCopyHelperName = [](const std::string &fullPath) -> bool {
    static constexpr std::string_view copySuffix = "/Copy";
    static constexpr std::string_view moveSuffix = "/Move";
    if (fullPath.size() <= copySuffix.size()) {
      return false;
    }
    if (fullPath.compare(fullPath.size() - copySuffix.size(),
                         copySuffix.size(),
                         copySuffix.data(),
                         copySuffix.size()) == 0) {
      return true;
    }
    if (fullPath.size() <= moveSuffix.size()) {
      return false;
    }
    return fullPath.compare(fullPath.size() - moveSuffix.size(),
                            moveSuffix.size(),
                            moveSuffix.data(),
                            moveSuffix.size()) == 0;
  };
  for (const auto &def : program_.definitions) {
    DefinitionContextScope definitionScope(*this, def);
    std::string parentPath;
    std::string placement;
    if (!isLifecycleHelper(def.fullPath, parentPath, placement)) {
      continue;
    }
    if (parentPath.empty() || structNames_.count(parentPath) == 0) {
      error_ = "lifecycle helper must be nested inside a struct: " + def.fullPath;
      return false;
    }
    if (isCopyHelperName(def.fullPath)) {
      if (def.parameters.size() != 1) {
        error_ = "Copy/Move helpers require exactly one parameter: " + def.fullPath;
        return false;
      }
    } else if (!def.parameters.empty()) {
      error_ = "lifecycle helpers do not accept parameters: " + def.fullPath;
      return false;
    }
  }

  return buildParameters();
}

bool SemanticsValidator::buildParameters() {
  entryArgsName_.clear();
  struct HelperSuffixInfo {
    std::string_view suffix;
    std::string_view placement;
  };
  auto isLifecycleHelper =
      [](const std::string &fullPath, std::string &parentOut, std::string &placementOut) -> bool {
    static const std::array<HelperSuffixInfo, 10> suffixes = {{
        {"Create", ""},
        {"Destroy", ""},
        {"Copy", ""},
        {"Move", ""},
        {"CreateStack", "stack"},
        {"DestroyStack", "stack"},
        {"CreateHeap", "heap"},
        {"DestroyHeap", "heap"},
        {"CreateBuffer", "buffer"},
        {"DestroyBuffer", "buffer"},
    }};
    for (const auto &info : suffixes) {
      const std::string_view suffix = info.suffix;
      if (fullPath.size() < suffix.size() + 1) {
        continue;
      }
      const size_t suffixStart = fullPath.size() - suffix.size();
      if (fullPath[suffixStart - 1] != '/') {
        continue;
      }
      if (fullPath.compare(suffixStart, suffix.size(), suffix.data(), suffix.size()) != 0) {
        continue;
      }
      parentOut = fullPath.substr(0, suffixStart - 1);
      placementOut = std::string(info.placement);
      return true;
    }
    return false;
  };
  auto isCopyHelperName = [](const std::string &fullPath) -> bool {
    static constexpr std::string_view copySuffix = "/Copy";
    static constexpr std::string_view moveSuffix = "/Move";
    if (fullPath.size() <= copySuffix.size()) {
      return false;
    }
    if (fullPath.compare(fullPath.size() - copySuffix.size(),
                         copySuffix.size(),
                         copySuffix.data(),
                         copySuffix.size()) == 0) {
      return true;
    }
    if (fullPath.size() <= moveSuffix.size()) {
      return false;
    }
    return fullPath.compare(fullPath.size() - moveSuffix.size(),
                            moveSuffix.size(),
                            moveSuffix.data(),
                            moveSuffix.size()) == 0;
  };
  auto isStructHelperDefinition = [&](const Definition &def, std::string &parentOut) -> bool {
    parentOut.clear();
    if (!def.isNested) {
      return false;
    }
    if (structNames_.count(def.fullPath) > 0) {
      return false;
    }
    const size_t slash = def.fullPath.find_last_of('/');
    if (slash == std::string::npos || slash == 0) {
      return false;
    }
    const std::string parent = def.fullPath.substr(0, slash);
    if (structNames_.count(parent) == 0) {
      return false;
    }
    parentOut = parent;
    return true;
  };
  auto hasStaticTransform = [](const Definition &def) -> bool {
    for (const auto &transform : def.transforms) {
      if (transform.name == "static") {
        return true;
      }
    }
    return false;
  };

  for (const auto &def : program_.definitions) {
    DefinitionContextScope definitionScope(*this, def);
    std::unordered_set<std::string> seen;
    std::vector<ParameterInfo> params;
    params.reserve(def.parameters.size());
    auto defaultResolvesToDefinition = [&](const Expr &candidate) -> bool {
      return candidate.kind == Expr::Kind::Call && defMap_.find(resolveCalleePath(candidate)) != defMap_.end();
    };
    for (const auto &param : def.parameters) {
      if (!param.isBinding) {
        error_ = "parameters must use binding syntax: " + def.fullPath;
        return false;
      }
      if (param.hasBodyArguments || !param.bodyArguments.empty()) {
        error_ = "parameter does not accept block arguments: " + param.name;
        return false;
      }
      if (!seen.insert(param.name).second) {
        error_ = "duplicate parameter: " + param.name;
        return false;
      }
      BindingInfo binding;
      std::optional<std::string> restrictType;
      if (!parseBindingInfo(param, def.namespacePrefix, structNames_, importAliases_, binding, restrictType, error_)) {
        return false;
      }
      if (binding.typeName == "uninitialized") {
        error_ = "uninitialized storage is not allowed on parameters: " + param.name;
        return false;
      }
      if (param.args.size() > 1) {
        error_ = "parameter defaults accept at most one argument: " + param.name;
        return false;
      }
      if (param.args.size() == 1 && !isDefaultExprAllowed(param.args.front(), defaultResolvesToDefinition)) {
        if (param.args.front().kind == Expr::Kind::Call && hasNamedArguments(param.args.front().argNames)) {
          error_ = "parameter default does not accept named arguments: " + param.name;
        } else {
          error_ = "parameter default must be a literal or pure expression: " + param.name;
        }
        return false;
      }
      if (!hasExplicitBindingTypeTransform(param) && param.args.size() == 1) {
        (void)tryInferBindingTypeFromInitializer(param.args.front(), {}, {}, binding, hasAnyMathImport());
      }
      if (!validateBuiltinMapKeyType(binding, &def.templateArgs, error_)) {
        return false;
      }
      ParameterInfo info;
      info.name = param.name;
      info.binding = std::move(binding);
      if (param.args.size() == 1) {
        info.defaultExpr = &param.args.front();
      }
      if (restrictType.has_value()) {
        const bool hasTemplate = !info.binding.typeTemplateArg.empty();
        if (!restrictMatchesBinding(*restrictType,
                                    info.binding.typeName,
                                    info.binding.typeTemplateArg,
                                    hasTemplate,
                                    def.namespacePrefix)) {
          error_ = "restrict type does not match binding type";
          return false;
        }
      }
      params.push_back(std::move(info));
    }
    std::string parentPath;
    std::string placement;
    std::string helperParent;
    const bool isLifecycle = isLifecycleHelper(def.fullPath, parentPath, placement);
    if (!isLifecycle) {
      (void)isStructHelperDefinition(def, helperParent);
      if (!helperParent.empty()) {
        parentPath = helperParent;
      }
    }
    const bool isStructHelper = isLifecycle || !helperParent.empty();
    const bool isStaticHelper = isStructHelper && !isLifecycle && hasStaticTransform(def);
    bool sawMut = false;
    for (const auto &transform : def.transforms) {
      if (transform.name != "mut") {
        continue;
      }
      if (sawMut) {
        error_ = "duplicate mut transform on " + def.fullPath;
        return false;
      }
      sawMut = true;
      if (!transform.templateArgs.empty()) {
        error_ = "mut transform does not accept template arguments on " + def.fullPath;
        return false;
      }
      if (!transform.arguments.empty()) {
        error_ = "mut transform does not accept arguments on " + def.fullPath;
        return false;
      }
    }
    if (sawMut && !isStructHelper) {
      error_ = "mut transform is only supported on struct helpers: " + def.fullPath;
      return false;
    }
    if (sawMut && isStaticHelper) {
      error_ = "mut transform is not allowed on static helpers: " + def.fullPath;
      return false;
    }

    if (isLifecycle) {
      if (isCopyHelperName(def.fullPath)) {
        if (params.size() != 1) {
          error_ = "Copy/Move helpers require exactly one parameter: " + def.fullPath;
          return false;
        }
        const auto &copyParam = params.front();
        if (copyParam.binding.typeName != "Reference" || copyParam.binding.typeTemplateArg != parentPath) {
          error_ = "Copy/Move helpers require [Reference<Self>] parameter: " + def.fullPath;
          return false;
        }
      }
    }
    if (isStructHelper && !isStaticHelper) {
      if (!seen.insert("this").second) {
        error_ = "duplicate parameter: this";
        return false;
      }
      ParameterInfo info;
      info.name = "this";
      info.binding.typeName = "Reference";
      info.binding.typeTemplateArg = parentPath;
      info.binding.isMutable = sawMut;
      params.insert(params.begin(), std::move(info));
    }
    if (def.fullPath == entryPath_) {
      if (params.size() == 1 && params.front().binding.typeName == "array" &&
          params.front().binding.typeTemplateArg == "string") {
        entryArgsName_ = params.front().name;
      }
    }
    paramsByDef_[def.fullPath] = std::move(params);
  }
  return true;
}

std::string SemanticsValidator::resolveCalleePath(const Expr &expr) const {
  auto rewriteCanonicalCollectionHelperPath = [&](const std::string &resolvedPath) -> std::string {
    auto canonicalVectorHelperAliasPath = [&](std::string_view helperName) -> std::string {
      if (helperName == "count" || helperName == "capacity" || helperName == "at" ||
          helperName == "at_unsafe") {
        return "/vector/" + std::string(helperName);
      }
      return {};
    };
    auto canonicalMapHelperAliasPath = [&](std::string_view helperName) -> std::string {
      if (helperName == "count" || helperName == "contains" || helperName == "tryAt" ||
          helperName == "at" || helperName == "at_unsafe") {
        return "/map/" + std::string(helperName);
      }
      return {};
    };
    auto rewriteCanonicalHelper = [&](std::string_view prefix,
                                      const auto &aliasPathBuilder,
                                      bool requirePositionalBuiltinAlias) -> std::string {
      if (resolvedPath.rfind(prefix, 0) != 0) {
        return resolvedPath;
      }
      const std::string helperName = resolvedPath.substr(prefix.size());
      const std::string aliasPath = aliasPathBuilder(helperName);
      if (aliasPath.empty()) {
        return resolvedPath;
      }
      if (defMap_.count(aliasPath) > 0) {
        return aliasPath;
      }
      if (requirePositionalBuiltinAlias && !hasNamedArguments(expr.argNames) &&
          defMap_.count(resolvedPath) > 0) {
        return aliasPath;
      }
      return resolvedPath;
    };
    std::string rewritten =
        rewriteCanonicalHelper("/std/collections/vector/", canonicalVectorHelperAliasPath, true);
    if (rewritten != resolvedPath) {
      return rewritten;
    }
    return rewriteCanonicalHelper("/std/collections/map/", canonicalMapHelperAliasPath, false);
  };
  auto rewriteCanonicalCollectionConstructorPath = [&](const std::string &resolvedPath) -> std::string {
    if (expr.isMethodCall) {
      return rewriteCanonicalCollectionHelperPath(resolvedPath);
    }
    auto vectorConstructorHelperPath = [&]() -> std::string {
      switch (expr.args.size()) {
      case 0:
        return "/std/collections/vectorNew";
      case 1:
        return "/std/collections/vectorSingle";
      case 2:
        return "/std/collections/vectorPair";
      case 3:
        return "/std/collections/vectorTriple";
      case 4:
        return "/std/collections/vectorQuad";
      case 5:
        return "/std/collections/vectorQuint";
      case 6:
        return "/std/collections/vectorSext";
      case 7:
        return "/std/collections/vectorSept";
      case 8:
        return "/std/collections/vectorOct";
      default:
        return {};
      }
    };
    auto mapConstructorHelperPath = [&]() -> std::string {
      switch (expr.args.size()) {
      case 0:
        return "/std/collections/mapNew";
      case 2:
        return "/std/collections/mapSingle";
      case 4:
        return "/std/collections/mapPair";
      case 6:
        return "/std/collections/mapTriple";
      case 8:
        return "/std/collections/mapQuad";
      case 10:
        return "/std/collections/mapQuint";
      case 12:
        return "/std/collections/mapSext";
      case 14:
        return "/std/collections/mapSept";
      case 16:
        return "/std/collections/mapOct";
      default:
        return {};
      }
    };
    std::string helperPath;
    if (resolvedPath == "/std/collections/vector/vector") {
      helperPath = vectorConstructorHelperPath();
    } else if (resolvedPath == "/std/collections/map/map") {
      helperPath = mapConstructorHelperPath();
    }
    if (!helperPath.empty() && defMap_.count(helperPath) > 0) {
      return helperPath;
    }
    return rewriteCanonicalCollectionHelperPath(resolvedPath);
  };
  if (expr.name.empty()) {
    return "";
  }
  if (!expr.name.empty() && expr.name[0] == '/') {
    return rewriteCanonicalCollectionConstructorPath(expr.name);
  }
  if (expr.name.find('/') != std::string::npos) {
    return rewriteCanonicalCollectionConstructorPath("/" + expr.name);
  }
  std::string pointerBuiltinName;
  if (getBuiltinPointerName(expr, pointerBuiltinName)) {
    return "/" + pointerBuiltinName;
  }
  if (!expr.namespacePrefix.empty()) {
    std::string normalizedPrefix = expr.namespacePrefix;
    if (!normalizedPrefix.empty() && normalizedPrefix.front() != '/') {
      normalizedPrefix.insert(normalizedPrefix.begin(), '/');
    }
    auto isRemovedVectorCompatibilityHelper = [](std::string_view helperName) {
      return helperName == "count" || helperName == "capacity" || helperName == "at" ||
             helperName == "at_unsafe" || helperName == "push" || helperName == "pop" ||
             helperName == "reserve" || helperName == "clear" || helperName == "remove_at" ||
             helperName == "remove_swap";
    };
    auto isRemovedMapCompatibilityHelper = [](std::string_view helperName) {
      return helperName == "count" || helperName == "at" || helperName == "at_unsafe";
    };
    const size_t lastSlash = normalizedPrefix.find_last_of('/');
    const std::string_view suffix = lastSlash == std::string::npos
                                        ? std::string_view(normalizedPrefix)
                                        : std::string_view(normalizedPrefix).substr(lastSlash + 1);
    if (suffix == expr.name && defMap_.count(normalizedPrefix) > 0) {
      return normalizedPrefix;
    }
    std::string prefix = normalizedPrefix;
    while (!prefix.empty()) {
      std::string candidate = prefix + "/" + expr.name;
      if (defMap_.count(candidate) > 0) {
        return candidate;
      }
      const size_t slash = prefix.find_last_of('/');
      if (slash == std::string::npos) {
        break;
      }
      prefix = prefix.substr(0, slash);
    }
    if (normalizedPrefix.rfind("/std/collections/vector", 0) == 0 &&
        isRemovedVectorCompatibilityHelper(expr.name)) {
      return normalizedPrefix + "/" + expr.name;
    }
    if (normalizedPrefix.rfind("/std/collections/map", 0) == 0 &&
        isRemovedMapCompatibilityHelper(expr.name)) {
      return normalizedPrefix + "/" + expr.name;
    }
    auto it = importAliases_.find(expr.name);
    if (it != importAliases_.end()) {
      return rewriteCanonicalCollectionConstructorPath(it->second);
    }
    return rewriteCanonicalCollectionConstructorPath(normalizedPrefix + "/" + expr.name);
  }
  std::string root = "/" + expr.name;
  if (defMap_.count(root) > 0) {
    return rewriteCanonicalCollectionConstructorPath(root);
  }
  auto it = importAliases_.find(expr.name);
  if (it != importAliases_.end()) {
    return rewriteCanonicalCollectionConstructorPath(it->second);
  }
  return rewriteCanonicalCollectionConstructorPath(root);
}

bool SemanticsValidator::resolveStructFieldBinding(const Definition &structDef,
                                                   const Expr &fieldStmt,
                                                   BindingInfo &bindingOut) {
  std::optional<std::string> restrictType;
  std::string parseError;
  if (!parseBindingInfo(fieldStmt, structDef.namespacePrefix, structNames_, importAliases_, bindingOut, restrictType,
                        parseError)) {
    error_ = parseError;
    return false;
  }
  if (hasExplicitBindingTypeTransform(fieldStmt)) {
    if (!validateBuiltinMapKeyType(bindingOut, &structDef.templateArgs, error_)) {
      return false;
    }
    return true;
  }
  const std::string fieldPath = structDef.fullPath + "/" + fieldStmt.name;
  if (fieldStmt.args.size() != 1) {
    error_ = "omitted struct field envelope requires exactly one initializer: " + fieldPath;
    return false;
  }
  const std::vector<ParameterInfo> noParams;
  const std::unordered_map<std::string, BindingInfo> noLocals;
  BindingInfo inferred = bindingOut;
  if (inferBindingTypeFromInitializer(fieldStmt.args.front(), noParams, noLocals, inferred)) {
    if (!(inferred.typeName == "array" && inferred.typeTemplateArg.empty())) {
      bindingOut = std::move(inferred);
      if (!validateBuiltinMapKeyType(bindingOut, &structDef.templateArgs, error_)) {
        return false;
      }
      return true;
    }
  }
  if (inferExprReturnKind(fieldStmt.args.front(), noParams, noLocals) == ReturnKind::Array) {
    std::string structPath = inferStructReturnPath(fieldStmt.args.front(), noParams, noLocals);
    if (!structPath.empty()) {
      bindingOut.typeName = std::move(structPath);
      bindingOut.typeTemplateArg.clear();
      if (!validateBuiltinMapKeyType(bindingOut, &structDef.templateArgs, error_)) {
        return false;
      }
      return true;
    }
  }
  error_ = "unresolved or ambiguous omitted struct field envelope: " + fieldPath;
  return false;
}

bool SemanticsValidator::validateSoaVectorElementFieldEnvelopes(const std::string &typeArg,
                                                                const std::string &namespacePrefix) {
  auto resolveStructPath = [&](const std::string &candidate, const std::string &scope) -> std::string {
    const std::string normalized = normalizeBindingTypeName(candidate);
    std::string resolved = resolveStructTypePath(normalized, scope, structNames_);
    if (!resolved.empty()) {
      return resolved;
    }
    auto importIt = importAliases_.find(normalized);
    if (importIt != importAliases_.end() && structNames_.count(importIt->second) > 0) {
      return importIt->second;
    }
    return {};
  };
  std::string elementStructPath = resolveStructPath(typeArg, namespacePrefix);
  if (elementStructPath.empty()) {
    return true;
  }
  auto structIt = defMap_.find(elementStructPath);
  if (structIt == defMap_.end()) {
    return true;
  }
  auto hasStaticTransform = [](const Expr &stmt) -> bool {
    for (const auto &transform : stmt.transforms) {
      if (transform.name == "static") {
        return true;
      }
    }
    return false;
  };
  std::unordered_set<std::string> activeStructs;
  std::function<bool(const Definition &, const std::string &)> validateStructFields;
  validateStructFields = [&](const Definition &structDef, const std::string &pathPrefix) -> bool {
    if (!activeStructs.insert(structDef.fullPath).second) {
      return true;
    }
    for (const auto &fieldStmt : structDef.statements) {
      if (!fieldStmt.isBinding || hasStaticTransform(fieldStmt)) {
        continue;
      }
      BindingInfo fieldBinding;
      if (!resolveStructFieldBinding(structDef, fieldStmt, fieldBinding)) {
        activeStructs.erase(structDef.fullPath);
        return false;
      }
      const std::string fieldPath = pathPrefix + "/" + fieldStmt.name;
      const std::string normalizedFieldType = normalizeBindingTypeName(fieldBinding.typeName);
      if (normalizedFieldType == "string" || fieldBinding.typeName == "Pointer" ||
          fieldBinding.typeName == "Reference" || !fieldBinding.typeTemplateArg.empty()) {
        std::string fieldType = fieldBinding.typeName;
        if (!fieldBinding.typeTemplateArg.empty()) {
          fieldType += "<" + fieldBinding.typeTemplateArg + ">";
        }
        error_ = "soa_vector field envelope is unsupported on " + fieldPath + ": " + fieldType;
        activeStructs.erase(structDef.fullPath);
        return false;
      }
      if (fieldBinding.typeTemplateArg.empty() && !isPrimitiveBindingTypeName(normalizedFieldType)) {
        const std::string nestedStructPath = resolveStructPath(fieldBinding.typeName, structDef.namespacePrefix);
        if (!nestedStructPath.empty()) {
          auto nestedIt = defMap_.find(nestedStructPath);
          if (nestedIt != defMap_.end()) {
            if (!validateStructFields(*nestedIt->second, fieldPath)) {
              activeStructs.erase(structDef.fullPath);
              return false;
            }
          }
        }
      }
    }
    activeStructs.erase(structDef.fullPath);
    return true;
  };
  return validateStructFields(*structIt->second, structIt->second->fullPath);
}

bool SemanticsValidator::resolveUninitializedStorageBinding(const std::vector<ParameterInfo> &params,
                                                            const std::unordered_map<std::string, BindingInfo> &locals,
                                                            const Expr &storage,
                                                            BindingInfo &bindingOut,
                                                            bool &resolvedOut) {
  resolvedOut = false;
  auto bindingTypeText = [](const BindingInfo &binding) {
    if (binding.typeTemplateArg.empty()) {
      return binding.typeName;
    }
    return binding.typeName + "<" + binding.typeTemplateArg + ">";
  };
  auto assignBindingTypeFromText = [&](const std::string &typeText) {
    const std::string normalizedType = normalizeBindingTypeName(typeText);
    std::string base;
    std::string argText;
    if (splitTemplateTypeName(normalizedType, base, argText) && !base.empty()) {
      bindingOut.typeName = normalizeBindingTypeName(base);
      bindingOut.typeTemplateArg = argText;
      return;
    }
    bindingOut.typeName = normalizedType;
    bindingOut.typeTemplateArg.clear();
  };
  std::function<bool(const Expr &, BindingInfo &)> resolvePointerBinding;
  resolvePointerBinding = [&](const Expr &pointerExpr, BindingInfo &pointerBinding) -> bool {
    if (pointerExpr.kind == Expr::Kind::Name) {
      if (const BindingInfo *binding = findBinding(params, locals, pointerExpr.name)) {
        pointerBinding = *binding;
        return true;
      }
      return false;
    }
    if (pointerExpr.kind != Expr::Kind::Call) {
      return false;
    }
    std::string pointerBuiltin;
    if (getBuiltinPointerName(pointerExpr, pointerBuiltin) && pointerBuiltin == "location" && pointerExpr.args.size() == 1) {
      BindingInfo pointeeBinding;
      bool pointeeResolved = false;
      if (!resolveUninitializedStorageBinding(params, locals, pointerExpr.args.front(), pointeeBinding, pointeeResolved)) {
        return false;
      }
      if (!pointeeResolved) {
        return false;
      }
      pointerBinding.typeName = "Reference";
      pointerBinding.typeTemplateArg = bindingTypeText(pointeeBinding);
      return true;
    }
    if (inferBindingTypeFromInitializer(pointerExpr, params, locals, pointerBinding)) {
      return true;
    }
    auto defIt = defMap_.find(resolveCalleePath(pointerExpr));
    if (defIt == defMap_.end() || defIt->second == nullptr) {
      return false;
    }
    for (const auto &transform : defIt->second->transforms) {
      if (transform.name != "return" || transform.templateArgs.size() != 1) {
        continue;
      }
      const std::string normalizedReturnType = normalizeBindingTypeName(transform.templateArgs.front());
      std::string base;
      std::string argText;
      if (!splitTemplateTypeName(normalizedReturnType, base, argText) || argText.empty()) {
        return false;
      }
      base = normalizeBindingTypeName(base);
      if (base != "Reference" && base != "Pointer") {
        return false;
      }
      pointerBinding.typeName = base;
      pointerBinding.typeTemplateArg = argText;
      return true;
    }
    return false;
  };
  if (storage.kind == Expr::Kind::Name) {
    if (const BindingInfo *binding = findBinding(params, locals, storage.name)) {
      bindingOut = *binding;
      resolvedOut = true;
    }
    return true;
  }
  if (storage.kind == Expr::Kind::Call) {
    std::string pointerBuiltin;
    if (getBuiltinPointerName(storage, pointerBuiltin) && pointerBuiltin == "dereference" && storage.args.size() == 1) {
      const Expr &pointerExpr = storage.args.front();
      BindingInfo pointerBinding;
      if (resolvePointerBinding(pointerExpr, pointerBinding)) {
        const std::string normalizedPointerType = normalizeBindingTypeName(pointerBinding.typeName);
        if ((normalizedPointerType == "Reference" || normalizedPointerType == "Pointer") &&
            !pointerBinding.typeTemplateArg.empty()) {
          assignBindingTypeFromText(pointerBinding.typeTemplateArg);
          resolvedOut = true;
          return true;
        }
      }
      return true;
    }
  }
  if (!storage.isFieldAccess || storage.args.size() != 1) {
    return true;
  }
  auto resolveStructReceiverPath = [&](const Expr &receiver, std::string &structPathOut) -> bool {
    structPathOut.clear();
    auto assignStructPathFromType = [&](std::string receiverType, const std::string &namespacePrefix) {
      receiverType = normalizeBindingTypeName(receiverType);
      if (receiverType.empty()) {
        return false;
      }
      structPathOut = resolveStructTypePath(receiverType, namespacePrefix, structNames_);
      if (structPathOut.empty()) {
        auto importIt = importAliases_.find(receiverType);
        if (importIt != importAliases_.end() && structNames_.count(importIt->second) > 0) {
          structPathOut = importIt->second;
        }
      }
      return !structPathOut.empty();
    };
    if (receiver.kind == Expr::Kind::Name) {
      const BindingInfo *receiverBinding = findBinding(params, locals, receiver.name);
      if (!receiverBinding) {
        return false;
      }
      std::string receiverType = receiverBinding->typeName;
      if ((receiverType == "Reference" || receiverType == "Pointer") && !receiverBinding->typeTemplateArg.empty()) {
        receiverType = receiverBinding->typeTemplateArg;
      }
      return assignStructPathFromType(receiverType, receiver.namespacePrefix);
    }
    if (receiver.kind != Expr::Kind::Call || receiver.isBinding) {
      return false;
    }
    std::string inferredStruct = inferStructReturnPath(receiver, params, locals);
    if (!inferredStruct.empty() && structNames_.count(inferredStruct) > 0) {
      structPathOut = inferredStruct;
      return true;
    }
    const std::string resolvedType = resolveCalleePath(receiver);
    if (structNames_.count(resolvedType) > 0) {
      structPathOut = resolvedType;
      return true;
    }
    return false;
  };
  const Expr &receiver = storage.args.front();
  std::string structPath;
  if (!resolveStructReceiverPath(receiver, structPath)) {
    return true;
  }
  auto defIt = defMap_.find(structPath);
  if (defIt == defMap_.end() || !defIt->second) {
    return true;
  }
  auto isStaticField = [](const Expr &stmt) -> bool {
    for (const auto &transform : stmt.transforms) {
      if (transform.name == "static") {
        return true;
      }
    }
    return false;
  };
  for (const auto &stmt : defIt->second->statements) {
    if (!stmt.isBinding || isStaticField(stmt)) {
      continue;
    }
    if (stmt.name != storage.name) {
      continue;
    }
    BindingInfo fieldBinding;
    if (!resolveStructFieldBinding(*defIt->second, stmt, fieldBinding)) {
      return false;
    }
    bindingOut = std::move(fieldBinding);
    resolvedOut = true;
    return true;
  }
  return true;
}

bool SemanticsValidator::isBuiltinBlockCall(const Expr &expr) const {
  if (!isBlockCall(expr)) {
    return false;
  }
  return defMap_.count(resolveCalleePath(expr)) == 0;
}

bool SemanticsValidator::isParam(const std::vector<ParameterInfo> &params, const std::string &name) const {
  for (const auto &param : params) {
    if (param.name == name) {
      return true;
    }
  }
  return false;
}

const BindingInfo *SemanticsValidator::findParamBinding(const std::vector<ParameterInfo> &params,
                                                        const std::string &name) const {
  for (const auto &param : params) {
    if (param.name == name) {
      return &param.binding;
    }
  }
  return nullptr;
}

std::string SemanticsValidator::typeNameForReturnKind(ReturnKind kind) const {
  switch (kind) {
    case ReturnKind::Int:
      return "i32";
    case ReturnKind::Int64:
      return "i64";
    case ReturnKind::UInt64:
      return "u64";
    case ReturnKind::Bool:
      return "bool";
    case ReturnKind::String:
      return "string";
    case ReturnKind::Float32:
      return "f32";
    case ReturnKind::Float64:
      return "f64";
    case ReturnKind::Integer:
      return "integer";
    case ReturnKind::Decimal:
      return "decimal";
    case ReturnKind::Complex:
      return "complex";
    case ReturnKind::Array:
      return "array";
    default:
      return "";
  }
}

bool SemanticsValidator::inferBindingTypeFromInitializer(
    const Expr &initializer,
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    BindingInfo &bindingOut) {
  auto bindingTypeText = [](const BindingInfo &binding) {
    if (binding.typeTemplateArg.empty()) {
      return binding.typeName;
    }
    return binding.typeName + "<" + binding.typeTemplateArg + ">";
  };
  auto assignBindingTypeFromText = [&](const std::string &typeText) -> bool {
    const std::string normalizedType = normalizeBindingTypeName(typeText);
    if (normalizedType.empty()) {
      return false;
    }
    std::string base;
    std::string argText;
    if (splitTemplateTypeName(normalizedType, base, argText)) {
      bindingOut.typeName = normalizeBindingTypeName(base);
      bindingOut.typeTemplateArg = argText;
      return true;
    }
    bindingOut.typeName = normalizedType;
    bindingOut.typeTemplateArg.clear();
    return true;
  };
  auto assignBindingTypeFromResultInfo = [&](const ResultTypeInfo &resultInfo) -> bool {
    if (!resultInfo.isResult || resultInfo.errorType.empty()) {
      return false;
    }
    bindingOut.typeName = "Result";
    if (!resultInfo.hasValue) {
      bindingOut.typeTemplateArg = resultInfo.errorType;
      return true;
    }
    if (resultInfo.valueType.empty()) {
      return false;
    }
    bindingOut.typeTemplateArg = resultInfo.valueType + ", " + resultInfo.errorType;
    return true;
  };
  auto inferTryInitializerBinding = [&]() -> bool {
    if (initializer.kind != Expr::Kind::Call || initializer.isMethodCall || !isSimpleCallName(initializer, "try") ||
        initializer.args.size() != 1 || !initializer.templateArgs.empty() || initializer.hasBodyArguments ||
        !initializer.bodyArguments.empty()) {
      return false;
    }
    ResultTypeInfo resultInfo;
    if (!resolveResultTypeForExpr(initializer.args.front(), params, locals, resultInfo) || !resultInfo.isResult ||
        !resultInfo.hasValue || resultInfo.valueType.empty()) {
      return false;
    }
    return assignBindingTypeFromText(resultInfo.valueType);
  };
  auto inferDirectResultOkBinding = [&]() -> bool {
    if (initializer.kind != Expr::Kind::Call || !initializer.isMethodCall || initializer.name != "ok" ||
        initializer.templateArgs.size() != 0 || initializer.hasBodyArguments || !initializer.bodyArguments.empty()) {
      return false;
    }
    if (initializer.args.empty()) {
      return false;
    }
    const Expr &receiver = initializer.args.front();
    if (receiver.kind != Expr::Kind::Name || normalizeBindingTypeName(receiver.name) != "Result") {
      return false;
    }
    auto inferCurrentErrorType = [&]() -> std::string {
      if (currentResultType_.has_value() && currentResultType_->isResult && !currentResultType_->errorType.empty()) {
        return currentResultType_->errorType;
      }
      if (currentOnError_.has_value() && !currentOnError_->errorType.empty()) {
        return currentOnError_->errorType;
      }
      return "_";
    };
    if (initializer.args.size() == 1) {
      bindingOut.typeName = "Result";
      bindingOut.typeTemplateArg = inferCurrentErrorType();
      return true;
    }
    if (initializer.args.size() != 2) {
      return false;
    }
    BindingInfo payloadBinding;
    if (!inferBindingTypeFromInitializer(initializer.args.back(), params, locals, payloadBinding)) {
      return false;
    }
    const std::string payloadTypeText = bindingTypeText(payloadBinding);
    if (payloadTypeText.empty()) {
      return false;
    }
    bindingOut.typeName = "Result";
    bindingOut.typeTemplateArg = payloadTypeText + ", " + inferCurrentErrorType();
    return true;
  };
  auto inferCollectionBindingFromExpr = [&](const Expr &expr) -> bool {
    auto copyNamedBinding = [&](const std::string &name) -> bool {
      if (const BindingInfo *paramBinding = findParamBinding(params, name)) {
        bindingOut = *paramBinding;
        return true;
      }
      auto it = locals.find(name);
      if (it == locals.end()) {
        return false;
      }
      bindingOut = it->second;
      return true;
    };
    if (expr.kind == Expr::Kind::Name) {
      return copyNamedBinding(expr.name);
    }
    if (expr.kind != Expr::Kind::Call) {
      return false;
    }
    std::string collection;
    if (getBuiltinCollectionName(expr, collection)) {
      if ((collection == "array" || collection == "vector" || collection == "soa_vector") && expr.templateArgs.size() == 1) {
        bindingOut.typeName = collection;
        bindingOut.typeTemplateArg = expr.templateArgs.front();
        return true;
      }
      if (collection == "map" && expr.templateArgs.size() == 2) {
        bindingOut.typeName = "map";
        bindingOut.typeTemplateArg = joinTemplateArgs(expr.templateArgs);
        return true;
      }
    }
    auto defIt = defMap_.find(resolveCalleePath(expr));
    if (defIt == defMap_.end() || defIt->second == nullptr) {
      return false;
    }
    for (const auto &transform : defIt->second->transforms) {
      if (transform.name != "return" || transform.templateArgs.size() != 1) {
        continue;
      }
      const std::string normalizedReturnType = normalizeBindingTypeName(transform.templateArgs.front());
      std::string base;
      std::string argText;
      if (!splitTemplateTypeName(normalizedReturnType, base, argText)) {
        return false;
      }
      base = normalizeBindingTypeName(base);
      std::vector<std::string> args;
      if (!splitTopLevelTemplateArgs(argText, args)) {
        return false;
      }
      if ((base == "array" || base == "vector" || base == "soa_vector") && args.size() == 1) {
        bindingOut.typeName = base;
        bindingOut.typeTemplateArg = argText;
        return true;
      }
      if (isMapCollectionTypeName(base) && args.size() == 2) {
        bindingOut.typeName = base;
        bindingOut.typeTemplateArg = argText;
        return true;
      }
      if ((base == "Reference" || base == "Pointer") && args.size() == 1) {
        bindingOut.typeName = base;
        bindingOut.typeTemplateArg = args.front();
        return true;
      }
      return false;
    }
    return false;
  };
  auto inferBuiltinCollectionValueBinding = [&](const Expr &expr) -> bool {
    auto inferArrayElementType = [&](const BindingInfo &binding, std::string &elemTypeOut) {
      elemTypeOut.clear();
      if ((binding.typeName == "array" || binding.typeName == "vector" || binding.typeName == "soa_vector") &&
          !binding.typeTemplateArg.empty()) {
        elemTypeOut = binding.typeTemplateArg;
        return true;
      }
      if ((binding.typeName == "Reference" || binding.typeName == "Pointer") && !binding.typeTemplateArg.empty()) {
        std::string base;
        std::string argText;
        if (!splitTemplateTypeName(binding.typeTemplateArg, base, argText)) {
          return false;
        }
        base = normalizeBindingTypeName(base);
        if ((base == "array" || base == "vector" || base == "soa_vector") && !argText.empty()) {
          elemTypeOut = argText;
          return true;
        }
      }
      return false;
    };
    if (expr.kind != Expr::Kind::Call) {
      return false;
    }
    std::string builtinAccessName;
    if (getBuiltinArrayAccessName(expr, builtinAccessName) && expr.args.size() == 2) {
      BindingInfo collectionBinding;
      if (!inferCollectionBindingFromExpr(expr.args.front())) {
        return false;
      }
      collectionBinding = bindingOut;
      std::string keyType;
      std::string valueType;
      if (extractMapKeyValueTypes(collectionBinding, keyType, valueType)) {
        bindingOut.typeName = normalizeBindingTypeName(valueType);
        bindingOut.typeTemplateArg.clear();
        return true;
      }
      std::string elemType;
      if (inferArrayElementType(collectionBinding, elemType)) {
        bindingOut.typeName = normalizeBindingTypeName(elemType);
        bindingOut.typeTemplateArg.clear();
        return true;
      }
      return false;
    }
    const bool isCountLike =
        (isSimpleCallName(expr, "count") ||
         (resolveCalleePath(expr) == "/std/collections/map/count" &&
          defMap_.find("/std/collections/map/count") != defMap_.end())) &&
        expr.args.size() == 1;
    const bool isMapContainsLike =
        !expr.isMethodCall && isSimpleCallName(expr, "contains") && expr.args.size() == 2 &&
        (currentDefinitionPath_ == "/std/collections/mapContains" ||
         currentDefinitionPath_ == "/std/collections/mapTryAt");
    if (isMapContainsLike) {
      BindingInfo collectionBinding;
      if (!inferCollectionBindingFromExpr(expr.args.front())) {
        return false;
      }
      collectionBinding = bindingOut;
      std::string keyType;
      std::string valueType;
      if (!extractMapKeyValueTypes(collectionBinding, keyType, valueType)) {
        return false;
      }
      bindingOut.typeName = "bool";
      bindingOut.typeTemplateArg.clear();
      return true;
    }
    if (!isCountLike) {
      return false;
    }
    BindingInfo collectionBinding;
    if (!inferCollectionBindingFromExpr(expr.args.front())) {
      return false;
    }
    collectionBinding = bindingOut;
    std::string keyType;
    std::string valueType;
    std::string elemType;
    if (extractMapKeyValueTypes(collectionBinding, keyType, valueType) ||
        inferArrayElementType(collectionBinding, elemType) ||
        collectionBinding.typeName == "string") {
      bindingOut.typeName = "i32";
      bindingOut.typeTemplateArg.clear();
      return true;
    }
    return false;
  };
  auto inferBuiltinPointerBinding = [&](const Expr &expr) -> bool {
    std::function<bool(const Expr &, std::string &)> resolvePointerTargetType;
    resolvePointerTargetType = [&](const Expr &candidate, std::string &targetOut) -> bool {
      if (candidate.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, candidate.name)) {
          if ((paramBinding->typeName == "Pointer" || paramBinding->typeName == "Reference") &&
              !paramBinding->typeTemplateArg.empty()) {
            targetOut = paramBinding->typeTemplateArg;
            return true;
          }
          return false;
        }
        auto it = locals.find(candidate.name);
        if (it == locals.end()) {
          return false;
        }
        if ((it->second.typeName == "Pointer" || it->second.typeName == "Reference") &&
            !it->second.typeTemplateArg.empty()) {
          targetOut = it->second.typeTemplateArg;
          return true;
        }
        return false;
      }
      if (candidate.kind != Expr::Kind::Call) {
        return false;
      }
      std::string builtinName;
      if (getBuiltinPointerName(candidate, builtinName) && builtinName == "location" && candidate.args.size() == 1) {
        const Expr &target = candidate.args.front();
        if (target.kind != Expr::Kind::Name) {
          return false;
        }
        if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
          if (paramBinding->typeName == "Reference" && !paramBinding->typeTemplateArg.empty()) {
            targetOut = paramBinding->typeTemplateArg;
          } else {
            targetOut = paramBinding->typeName;
          }
          return true;
        }
        auto it = locals.find(target.name);
        if (it == locals.end()) {
          return false;
        }
        if (it->second.typeName == "Reference" && !it->second.typeTemplateArg.empty()) {
          targetOut = it->second.typeTemplateArg;
        } else {
          targetOut = it->second.typeName;
        }
        return true;
      }
      if (getBuiltinMemoryName(candidate, builtinName)) {
        if (builtinName == "alloc" && candidate.templateArgs.size() == 1 && candidate.args.size() == 1) {
          targetOut = candidate.templateArgs.front();
          return true;
        }
        if (builtinName == "realloc" && candidate.args.size() == 2) {
          return resolvePointerTargetType(candidate.args.front(), targetOut);
        }
        if (builtinName == "at" && candidate.templateArgs.empty() && candidate.args.size() == 3) {
          return resolvePointerTargetType(candidate.args.front(), targetOut);
        }
        if (builtinName == "at_unsafe" && candidate.templateArgs.empty() && candidate.args.size() == 2) {
          return resolvePointerTargetType(candidate.args.front(), targetOut);
        }
      }
      std::string opName;
      if (getBuiltinOperatorName(candidate, opName) && (opName == "plus" || opName == "minus") &&
          candidate.args.size() == 2) {
        if (isPointerLikeExpr(candidate.args[1], params, locals)) {
          return false;
        }
        return resolvePointerTargetType(candidate.args.front(), targetOut);
      }
      return false;
    };
    if (expr.kind != Expr::Kind::Call) {
      return false;
    }
    std::string builtinName;
    if (!getBuiltinMemoryName(expr, builtinName)) {
      return false;
    }
    std::string targetType;
    if (builtinName == "alloc") {
      if (expr.templateArgs.size() != 1 || expr.args.size() != 1) {
        return false;
      }
      targetType = expr.templateArgs.front();
    } else if (builtinName == "realloc") {
      if (expr.args.size() != 2 || !resolvePointerTargetType(expr.args.front(), targetType)) {
        return false;
      }
    } else if (builtinName == "at") {
      if (!expr.templateArgs.empty() || expr.args.size() != 3 || !resolvePointerTargetType(expr.args.front(), targetType)) {
        return false;
      }
    } else if (builtinName == "at_unsafe") {
      if (!expr.templateArgs.empty() || expr.args.size() != 2 || !resolvePointerTargetType(expr.args.front(), targetType)) {
        return false;
      }
    } else {
      return false;
    }
    bindingOut.typeName = "Pointer";
    bindingOut.typeTemplateArg = targetType;
    return true;
  };
  auto inferDeclaredCollectionBinding = [&](const Definition &definition) -> bool {
    auto isSupportedCollectionType = [&](const std::string &typeName) -> bool {
      std::string base;
      std::string argText;
      if (!splitTemplateTypeName(normalizeBindingTypeName(typeName), base, argText)) {
        return false;
      }
      base = normalizeBindingTypeName(base);
      std::vector<std::string> args;
      if (!splitTopLevelTemplateArgs(argText, args)) {
        return false;
      }
      return ((base == "array" || base == "vector" || base == "soa_vector") && args.size() == 1) ||
             (isMapCollectionTypeName(base) && args.size() == 2);
    };
    for (const auto &transform : definition.transforms) {
      if (transform.name != "return" || transform.templateArgs.size() != 1) {
        continue;
      }
      const std::string normalizedReturnType = normalizeBindingTypeName(transform.templateArgs.front());
      std::string base;
      std::string argText;
      if (!splitTemplateTypeName(normalizedReturnType, base, argText)) {
        return false;
      }
      base = normalizeBindingTypeName(base);
      std::vector<std::string> args;
      if (!splitTopLevelTemplateArgs(argText, args)) {
        return false;
      }
      if ((base == "array" || base == "vector" || base == "soa_vector") && args.size() == 1) {
        bindingOut.typeName = base;
        bindingOut.typeTemplateArg = argText;
        return true;
      }
      if (isMapCollectionTypeName(base) && args.size() == 2) {
        bindingOut.typeName = base;
        bindingOut.typeTemplateArg = argText;
        return true;
      }
      if ((base == "Reference" || base == "Pointer") && args.size() == 1 && isSupportedCollectionType(args.front())) {
        bindingOut.typeName = base;
        bindingOut.typeTemplateArg = args.front();
        return true;
      }
      return false;
    }
    return false;
  };
  auto inferCallInitializerBinding = [&]() -> bool {
    if (initializer.kind != Expr::Kind::Call) {
      return false;
    }
    ReturnKind resolvedKind = inferExprReturnKind(initializer, params, locals);
    if (resolvedKind == ReturnKind::Unknown || resolvedKind == ReturnKind::Void) {
      return false;
    }
    if (resolvedKind == ReturnKind::Array) {
      auto defIt = defMap_.find(resolveCalleePath(initializer));
      if (defIt == defMap_.end()) {
        return false;
      }
      if (inferDeclaredCollectionBinding(*defIt->second)) {
        return true;
      }
      std::string inferredStruct = inferStructReturnPath(initializer, params, locals);
      if (!inferredStruct.empty()) {
        bindingOut.typeName = inferredStruct;
        bindingOut.typeTemplateArg.clear();
        return true;
      }
    }
    std::string inferredType = typeNameForReturnKind(resolvedKind);
    if (inferredType.empty()) {
      return false;
    }
    bindingOut.typeName = inferredType;
    bindingOut.typeTemplateArg.clear();
    return true;
  };
  if (tryInferBindingTypeFromInitializer(initializer, params, locals, bindingOut, hasAnyMathImport())) {
    if (!(bindingOut.typeName == "array" && bindingOut.typeTemplateArg.empty())) {
      return true;
    }
    if (inferCallInitializerBinding()) {
      return true;
    }
    if (inferBuiltinCollectionValueBinding(initializer)) {
      return true;
    }
    if (inferBuiltinPointerBinding(initializer)) {
      return true;
    }
    return true;
  }
  if (inferTryInitializerBinding()) {
    return true;
  }
  if (inferDirectResultOkBinding()) {
    return true;
  }
  ResultTypeInfo resultInfo;
  if (resolveResultTypeForExpr(initializer, params, locals, resultInfo) &&
      assignBindingTypeFromResultInfo(resultInfo)) {
    return true;
  }
  if (inferCallInitializerBinding()) {
    return true;
  }
  if (inferBuiltinCollectionValueBinding(initializer)) {
    return true;
  }
  if (inferBuiltinPointerBinding(initializer)) {
    return true;
  }
  ReturnKind kind = inferExprReturnKind(initializer, params, locals);
  if (kind == ReturnKind::Unknown || kind == ReturnKind::Void) {
    return false;
  }
  std::string inferred = typeNameForReturnKind(kind);
  if (inferred.empty()) {
    return false;
  }
  bindingOut.typeName = inferred;
  bindingOut.typeTemplateArg.clear();
  return true;
}

} // namespace primec::semantics
