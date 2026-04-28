#include "SemanticsValidator.h"

namespace primec::semantics {
namespace {

bool isSumTransformName(const std::string &name) {
  return name == "sum";
}

bool isSumLayoutTransformName(const std::string &name) {
  return name == "struct" || name == "enum" || name == "pod" || name == "handle" ||
         name == "gpu_lane" || name == "no_padding" || name == "platform_independent_padding";
}

bool isUnsupportedSumPayloadEnvelope(const Transform &transform) {
  return transform.name == "auto" || transform.name == "void" || transform.name == "return" ||
         transform.name == "effects" || transform.name == "capabilities" ||
         transform.name == "on_error" || transform.name == "compute" ||
         transform.name == "workgroup_size" || transform.name == "unsafe" ||
         transform.name == "ast" || transform.name == "reflect" ||
         transform.name == "generate" || transform.name == "static" ||
         transform.name == "public" || transform.name == "private" ||
         transform.name == "mut" || transform.name == "copy" ||
         transform.name == "restrict" || transform.name == "align_bytes" ||
         transform.name == "align_kbytes" || isStructTransformName(transform.name);
}

bool isLowerCamelIdentifier(const std::string &name) {
  return !name.empty() && name.front() >= 'a' && name.front() <= 'z';
}

std::string sumPayloadTypeTextForBuildTransformValidation(const Transform &payload) {
  if (payload.templateArgs.empty()) {
    return payload.name;
  }
  return payload.name + "<" + joinTemplateArgs(payload.templateArgs) + ">";
}

bool isDirectRecursiveSumPayload(const Definition &def, const Transform &payload) {
  if (!payload.templateArgs.empty()) {
    return false;
  }
  if (payload.name == def.fullPath || payload.name == def.name) {
    return true;
  }
  if (!def.namespacePrefix.empty() &&
      payload.name == def.namespacePrefix + "/" + def.name) {
    return true;
  }
  return false;
}

} // namespace

bool SemanticsValidator::validateDefinitionBuildTransforms(
    const Definition &def,
    bool isStructHelper,
    bool isLifecycleHelper,
    std::unordered_set<std::string> &explicitStructs,
    std::vector<SemanticDiagnosticRecord> *transformDiagnosticRecords,
    bool &definitionTransformError) {
  definitionTransformError = false;
  std::unordered_set<std::string> seenEffects;
  bool sawCapabilities = false;
  bool sawOnError = false;
  bool sawCompute = false;
  bool sawUnsafe = false;
  bool sawWorkgroupSize = false;
  bool sawNoPadding = false;
  bool sawPlatformPadding = false;
  bool sawVisibility = false;
  bool isPublic = false;
  bool sawStatic = false;
  bool sawAst = false;
  bool sawReflect = false;
  bool sawGenerate = false;
  bool sawSum = false;
  bool hasReturnTransform = false;
  const bool collectTransformDiagnostics =
      transformDiagnosticRecords != nullptr && shouldCollectStructuredDiagnostics();

  auto addTransformDiagnostic = [&](const std::string &message) -> bool {
    if (!collectTransformDiagnostics) {
      (void)failDefinitionDiagnostic(def, message);
      return true;
    }
    SemanticDiagnosticRecord record;
    record.message = message;
    if (def.sourceLine > 0 && def.sourceColumn > 0) {
      record.primarySpan.line = def.sourceLine;
      record.primarySpan.column = def.sourceColumn;
      record.primarySpan.endLine = def.sourceLine;
      record.primarySpan.endColumn = def.sourceColumn;
      record.hasPrimarySpan = true;
    }
    SemanticDiagnosticRelatedSpan related;
    related.span.line = def.sourceLine;
    related.span.column = def.sourceColumn;
    related.span.endLine = def.sourceLine;
    related.span.endColumn = def.sourceColumn;
    related.label = "definition: " + def.fullPath;
    record.relatedSpans.push_back(std::move(related));
    rememberFirstCollectedDiagnosticMessage(message);
    transformDiagnosticRecords->push_back(std::move(record));
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
    if (transform.name == "return") {
      hasReturnTransform = true;
    }
    if (isSumTransformName(transform.name)) {
      if (sawSum) {
        if (addTransformDiagnostic("duplicate sum transform on " + def.fullPath)) {
          return false;
        }
        break;
      }
      sawSum = true;
      if (!transform.templateArgs.empty()) {
        if (addTransformDiagnostic("sum transform does not accept template arguments on " + def.fullPath)) {
          return false;
        }
        break;
      }
      if (!transform.arguments.empty()) {
        if (addTransformDiagnostic("sum transform does not accept arguments on " + def.fullPath)) {
          return false;
        }
        break;
      }
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
    } else if (transform.name == "ast") {
      if (sawAst) {
        if (addTransformDiagnostic("duplicate ast transform on " + def.fullPath)) {
          return false;
        }
        break;
      }
      sawAst = true;
      if (!transform.templateArgs.empty()) {
        if (addTransformDiagnostic("ast transform does not accept template arguments on " + def.fullPath)) {
          return false;
        }
        break;
      }
      if (!transform.arguments.empty()) {
        if (addTransformDiagnostic("ast transform does not accept arguments on " + def.fullPath)) {
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
    return true;
  }
  if (sawSum) {
    if (hasReturnTransform) {
      if (addTransformDiagnostic("sum definitions cannot declare return transforms: " + def.fullPath)) {
        return false;
      }
      return true;
    }
    if (!def.templateArgs.empty()) {
      if (addTransformDiagnostic("sum definitions do not support template parameters yet: " + def.fullPath)) {
        return false;
      }
      return true;
    }
    if (!def.parameters.empty()) {
      if (addTransformDiagnostic("sum definitions cannot declare parameters: " + def.fullPath)) {
        return false;
      }
      return true;
    }
    if (def.hasReturnStatement || def.returnExpr.has_value()) {
      if (addTransformDiagnostic("sum definitions cannot declare return statements: " + def.fullPath)) {
        return false;
      }
      return true;
    }
    for (const auto &transform : def.transforms) {
      if (isSumLayoutTransformName(transform.name)) {
        if (addTransformDiagnostic("sum definitions cannot combine with struct layout transforms: " + def.fullPath)) {
          return false;
        }
        return true;
      }
      if (transform.name == "effects" || transform.name == "capabilities" ||
          transform.name == "compute" || transform.name == "unsafe" ||
          transform.name == "on_error" || transform.name == "workgroup_size" ||
          transform.name == "ast" || transform.name == "reflect" ||
          transform.name == "generate") {
        if (addTransformDiagnostic("sum definitions cannot combine with callable transforms: " + def.fullPath)) {
          return false;
        }
        return true;
      }
    }

    std::unordered_set<std::string> seenVariants;
    for (const auto &stmt : def.statements) {
      const bool isUnitVariant =
          stmt.kind == Expr::Kind::Name && !stmt.name.empty() &&
          !stmt.isBinding && stmt.args.empty() && stmt.argNames.empty() &&
          stmt.bodyArguments.empty() && !stmt.hasBodyArguments &&
          stmt.transforms.empty() && stmt.templateArgs.empty();
      if (!stmt.isBinding && !isUnitVariant) {
        if (addTransformDiagnostic("sum variants require one payload envelope or bare unit variant on " +
                                   def.fullPath)) {
          return false;
        }
        return true;
      }
      if (!isLowerCamelIdentifier(stmt.name)) {
        if (addTransformDiagnostic("sum variant name must be lowerCamelCase: " + def.fullPath + "/" + stmt.name)) {
          return false;
        }
        return true;
      }
      if (!seenVariants.insert(stmt.name).second) {
        if (addTransformDiagnostic("duplicate sum variant: " + stmt.name + " on " + def.fullPath)) {
          return false;
        }
        return true;
      }
      if (stmt.args.size() > 0 || stmt.hasBodyArguments || !stmt.argNames.empty()) {
        if (addTransformDiagnostic("sum variants cannot declare initializers yet: " + def.fullPath + "/" + stmt.name)) {
          return false;
        }
        return true;
      }
      if (isUnitVariant) {
        continue;
      }
      if (stmt.transforms.size() != 1) {
        if (addTransformDiagnostic("sum variants require exactly one payload envelope on " + def.fullPath + "/" +
                                   stmt.name)) {
          return false;
        }
        return true;
      }
      const Transform &payload = stmt.transforms.front();
      if (!payload.arguments.empty() || isUnsupportedSumPayloadEnvelope(payload)) {
        if (addTransformDiagnostic("unsupported sum payload envelope on " + def.fullPath + "/" + stmt.name + ": " +
                                   payload.name)) {
          return false;
        }
        return true;
      }
      const std::string payloadTypeText =
          sumPayloadTypeTextForBuildTransformValidation(payload);
      const Definition *payloadSum =
          isDirectRecursiveSumPayload(def, payload)
              ? &def
              : resolveSumDefinitionForTypeText(payloadTypeText, def.namespacePrefix);
      if (payloadSum != nullptr && payloadSum->fullPath == def.fullPath) {
        if (addTransformDiagnostic("recursive sum payload is unsupported on " +
                                   def.fullPath + "/" + stmt.name)) {
          return false;
        }
        return true;
      }
    }
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
      return true;
    }
  }
  if (sawNoPadding && sawPlatformPadding) {
    if (addTransformDiagnostic("no_padding and platform_independent_padding are mutually exclusive on " + def.fullPath)) {
      return false;
    }
    return true;
  }

  bool isStruct = false;
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
    if (addTransformDiagnostic("pod definitions cannot be tagged as handle or gpu_lane: " + def.fullPath)) {
      return false;
    }
    return true;
  }
  if (hasHandle && hasGpuLane) {
    if (addTransformDiagnostic("handle definitions cannot be tagged as gpu_lane: " + def.fullPath)) {
      return false;
    }
    return true;
  }
  bool isFieldOnlyStruct = false;
  if (!sawSum && !isStruct && !hasReturnTransform && def.parameters.empty() && !def.hasReturnStatement &&
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
    return true;
  }
  if (isStruct) {
    explicitStructs.insert(def.fullPath);
    if (hasReturnTransform) {
      if (addTransformDiagnostic("struct definitions cannot declare return types: " + def.fullPath)) {
        return false;
      }
      return true;
    }
    if (def.hasReturnStatement) {
      if (addTransformDiagnostic("struct definitions cannot contain return statements: " + def.fullPath)) {
        return false;
      }
      return true;
    }
    if (def.returnExpr.has_value()) {
      if (addTransformDiagnostic("struct definitions cannot return a value: " + def.fullPath)) {
        return false;
      }
      return true;
    }
    if (!def.parameters.empty()) {
      if (addTransformDiagnostic("struct definitions cannot declare parameters: " + def.fullPath)) {
        return false;
      }
      return true;
    }
    for (const auto &stmt : def.statements) {
      if (!stmt.isBinding) {
        if (addTransformDiagnostic("struct definitions may only contain field bindings: " + def.fullPath)) {
          return false;
        }
        return true;
      }
    }
  }
  if (isStruct || isFieldOnlyStruct) {
    for (const auto &stmt : def.statements) {
      if (!stmt.isBinding) {
        continue;
      }
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
        if (addTransformDiagnostic("pod definitions cannot contain handle or gpu_lane fields: " + def.fullPath)) {
          return false;
        }
        return true;
      }
      if (fieldHasPod && fieldHasHandle) {
        if (addTransformDiagnostic("fields cannot be tagged as pod and handle: " + def.fullPath)) {
          return false;
        }
        return true;
      }
      if (fieldHasPod && fieldHasGpuLane) {
        if (addTransformDiagnostic("fields cannot be tagged as pod and gpu_lane: " + def.fullPath)) {
          return false;
        }
        return true;
      }
      if (fieldHasHandle && fieldHasGpuLane) {
        if (addTransformDiagnostic("fields cannot be tagged as handle and gpu_lane: " + def.fullPath)) {
          return false;
        }
        return true;
      }
    }
  }
  if (isPublic) {
    publicDefinitions_.insert(def.fullPath);
  }
  return true;
}

} // namespace primec::semantics
