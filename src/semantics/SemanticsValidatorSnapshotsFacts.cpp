// Per-family semantic-product fact snapshots (types, bindings, array extents, returns, queries).
#include "SemanticsValidatorSnapshotHelpers.h"

namespace primec::semantics {
using namespace snapshot_detail;

void SemanticsValidator::ensureOnErrorSnapshotFactCache() const {
  if (onErrorSnapshotFactCacheValid_) {
    return;
  }

  onErrorSnapshotCache_.clear();
  if (mergedWorkerPublicationFactsValid_) {
    onErrorSnapshotCache_ = mergedWorkerPublicationFacts_.onErrorFacts;
  } else {
    onErrorSnapshotCache_.reserve(program_.definitions.size());
    collectOnErrorSnapshotEntriesForStableRange(
        0,
        validationPlan_->definitionPrepass.declarationsInStableOrder.size(),
        onErrorSnapshotCache_);
  }
  sortCollectedOnErrorSnapshots(onErrorSnapshotCache_);
  onErrorSnapshotFactCacheValid_ = true;
}

std::vector<TypeMetadataSnapshotEntry>
SemanticsValidator::typeMetadataSnapshotForSemanticProduct() const {
  std::vector<TypeMetadataSnapshotEntry> entries;
  entries.reserve(program_.definitions.size());

  for (const auto &def : program_.definitions) {
    const std::string category = typeMetadataCategoryForSnapshot(def);
    if (category.empty()) {
      continue;
    }

    bool hasNoPadding = false;
    bool hasPlatformIndependentPadding = false;
    for (const auto &transform : def.transforms) {
      if (transform.name == "no_padding") {
        hasNoPadding = true;
      } else if (transform.name == "platform_independent_padding") {
        hasPlatformIndependentPadding = true;
      }
    }

    uint32_t explicitAlignmentBytes = 0;
    const bool hasExplicitAlignment = explicitAlignmentBytesForSnapshot(def.transforms, explicitAlignmentBytes);

    size_t fieldCount = 0;
    size_t enumValueCount = 0;
    if (category == "enum") {
      enumValueCount = def.statements.size();
    } else if (category == "sum") {
      fieldCount = def.sumVariants.size();
    } else {
      for (const auto &stmt : def.statements) {
        if (stmt.isBinding && !isCompileTimeTypeBinding(stmt) &&
            !isStaticFieldStatement(stmt)) {
          ++fieldCount;
        }
      }
    }

    entries.push_back(TypeMetadataSnapshotEntry{
        def.fullPath,
        category,
        publicDefinitions_.count(def.fullPath) > 0,
        hasNoPadding,
        hasPlatformIndependentPadding,
        hasExplicitAlignment,
        explicitAlignmentBytes,
        fieldCount,
        enumValueCount,
        def.sourceLine,
        def.sourceColumn,
        def.semanticNodeId,
    });
  }

  std::stable_sort(entries.begin(), entries.end(), [](const auto &left, const auto &right) {
    if (left.fullPath != right.fullPath) {
      return left.fullPath < right.fullPath;
    }
    return left.category < right.category;
  });
  return entries;
}

std::vector<StructFieldMetadataSnapshotEntry>
SemanticsValidator::structFieldMetadataSnapshotForSemanticProduct() {
  std::vector<StructFieldMetadataSnapshotEntry> entries;

  auto withPreservedError = [&](const std::function<bool()> &fn) {
    return runWithPreservedDiagnostic(fn);
  };

  for (const auto &def : program_.definitions) {
    const std::string category = typeMetadataCategoryForSnapshot(def);
    if (category.empty() || category == "enum" || category == "sum") {
      continue;
    }

    size_t fieldIndex = 0;
    for (const auto &stmt : def.statements) {
      if (!stmt.isBinding || isStaticFieldStatement(stmt) ||
          isCompileTimeTypeBinding(stmt)) {
        continue;
      }

      BindingInfo binding;
      if (!withPreservedError([&]() { return resolveStructFieldBinding(def, stmt, binding); })) {
        continue;
      }

      entries.push_back(StructFieldMetadataSnapshotEntry{
          def.fullPath,
          stmt.name,
          fieldIndex,
          stmt.sourceLine,
          stmt.sourceColumn,
          std::move(binding),
          stmt.semanticNodeId,
      });
      ++fieldIndex;
    }
  }

  std::stable_sort(entries.begin(), entries.end(), [](const auto &left, const auto &right) {
    if (left.structPath != right.structPath) {
      return left.structPath < right.structPath;
    }
    if (left.fieldIndex != right.fieldIndex) {
      return left.fieldIndex < right.fieldIndex;
    }
    return left.fieldName < right.fieldName;
  });
  return entries;
}

std::vector<SumTypeMetadataSnapshotEntry>
SemanticsValidator::sumTypeMetadataSnapshotForSemanticProduct() const {
  std::vector<SumTypeMetadataSnapshotEntry> entries;
  entries.reserve(program_.definitions.size());

  for (const auto &def : program_.definitions) {
    if (!hasSumTransformSnapshot(def)) {
      continue;
    }
    entries.push_back(SumTypeMetadataSnapshotEntry{
        def.fullPath,
        publicDefinitions_.count(def.fullPath) > 0,
        "u32",
        "inline_max_payload",
        def.sumVariants.size(),
        def.sourceLine,
        def.sourceColumn,
        def.semanticNodeId,
    });
  }

  std::stable_sort(entries.begin(), entries.end(), [](const auto &left, const auto &right) {
    return left.fullPath < right.fullPath;
  });
  return entries;
}

std::vector<SumVariantMetadataSnapshotEntry>
SemanticsValidator::sumVariantMetadataSnapshotForSemanticProduct() const {
  std::vector<SumVariantMetadataSnapshotEntry> entries;
  for (const auto &def : program_.definitions) {
    if (!hasSumTransformSnapshot(def)) {
      continue;
    }
    entries.reserve(entries.size() + def.sumVariants.size());
    for (const auto &variant : def.sumVariants) {
      uint64_t semanticNodeId = variant.semanticNodeId;
      int sourceLine = variant.sourceLine;
      int sourceColumn = variant.sourceColumn;
      for (const auto &stmt : def.statements) {
        if (!stmt.isBinding || stmt.name != variant.name) {
          continue;
        }
        semanticNodeId = stmt.semanticNodeId;
        sourceLine = stmt.sourceLine;
        sourceColumn = stmt.sourceColumn;
        break;
      }
      entries.push_back(SumVariantMetadataSnapshotEntry{
          def.fullPath,
          variant.name,
          variant.variantIndex,
          static_cast<uint32_t>(variant.variantIndex),
          variant.hasPayload,
          variant.payloadTypeText,
          sourceLine,
          sourceColumn,
          semanticNodeId,
      });
    }
  }

  std::stable_sort(entries.begin(), entries.end(), [](const auto &left, const auto &right) {
    if (left.sumPath != right.sumPath) {
      return left.sumPath < right.sumPath;
    }
    if (left.variantIndex != right.variantIndex) {
      return left.variantIndex < right.variantIndex;
    }
    return left.variantName < right.variantName;
  });
  return entries;
}

std::vector<BindingFactSnapshotEntry>
SemanticsValidator::bindingFactSnapshotForSemanticProduct() {
  using ActiveLocalBindings = std::unordered_map<std::string, BindingInfo>;

  std::vector<BindingFactSnapshotEntry> entries;

  for (const auto &def : program_.definitions) {
    const auto paramsIt = paramsByDef_.find(def.fullPath);
    if (paramsIt == paramsByDef_.end()) {
      continue;
    }
    const auto &defParams = paramsIt->second;
    const size_t syntheticLeadingParamCount =
        (defParams.size() > def.parameters.size() && !defParams.empty() && defParams.front().name == "this")
            ? (defParams.size() - def.parameters.size())
            : 0;
    const size_t paramCount =
        std::min(def.parameters.size(), defParams.size() - std::min(defParams.size(), syntheticLeadingParamCount));
    for (size_t i = 0; i < paramCount; ++i) {
      entries.push_back(BindingFactSnapshotEntry{
          def.fullPath,
          "parameter",
          defParams[syntheticLeadingParamCount + i].name,
          {},
          def.parameters[i].sourceLine,
          def.parameters[i].sourceColumn,
          defParams[syntheticLeadingParamCount + i].binding,
          def.parameters[i].semanticNodeId,
      });
    }
  }

  auto withPreservedError = [&](const std::function<bool()> &fn) {
    return runWithPreservedDiagnostic(fn);
  };

  auto inferBindingForLocals = [&](const Definition &def,
                                   const std::vector<ParameterInfo> &defParams,
                                   const ActiveLocalBindings &activeLocals,
                                   const Expr &bindingExpr,
                                   BindingInfo &bindingOut) {
    const std::string namespacePrefix =
        bindingExpr.namespacePrefix.empty() ? def.fullPath : bindingExpr.namespacePrefix;
    std::optional<std::string> restrictType;
    if (!withPreservedError([&]() {
          return parseBindingInfo(
              bindingExpr, namespacePrefix, structNames_, importAliases_, bindingOut, restrictType, error_,
              &sumNames_, nullptr, /*allowCapabilityArg=*/true);
        })) {
      return false;
    }
    const bool hasExplicitType = hasExplicitBindingTypeTransform(bindingExpr);
    const bool explicitAutoType = hasExplicitType && normalizeBindingTypeName(bindingOut.typeName) == "auto";
    if (bindingExpr.args.size() == 1 && (!hasExplicitType || explicitAutoType)) {
      BindingInfo inferred = bindingOut;
      if (withPreservedError([&]() {
            return inferBindingTypeFromInitializer(
                bindingExpr.args.front(), defParams, activeLocals, inferred, &bindingExpr);
          })) {
        bindingOut = std::move(inferred);
      }
    }
    if (bindingOut.typeTemplateArg.empty()) {
      const std::string resolvedStructType =
          resolveStructTypePath(bindingOut.typeName, namespacePrefix, structNames_);
      if (!resolvedStructType.empty()) {
        bindingOut.typeName = resolvedStructType;
      }
    }
    if (!restrictType.has_value()) {
      return true;
    }
    const bool hasTemplate = !bindingOut.typeTemplateArg.empty();
    return restrictMatchesBinding(
        *restrictType, bindingOut.typeName, bindingOut.typeTemplateArg, hasTemplate, namespacePrefix);
  };

  std::function<void(const Definition &,
                     const std::vector<ParameterInfo> &,
                     const Expr &,
                     ActiveLocalBindings &,
                     LocalBindingScope &)>
      visitExpr;
  std::function<void(const Definition &,
                     const std::vector<ParameterInfo> &,
                     const std::vector<Expr> &,
                     ActiveLocalBindings &,
                     LocalBindingScope &)>
      visitExprSequence;

  visitExprSequence = [&](const Definition &def,
                          const std::vector<ParameterInfo> &defParams,
                          const std::vector<Expr> &exprs,
                          ActiveLocalBindings &activeLocals,
                          LocalBindingScope &scope) {
    for (const auto &expr : exprs) {
      visitExpr(def, defParams, expr, activeLocals, scope);
    }
  };

  visitExpr = [&](const Definition &def,
                  const std::vector<ParameterInfo> &defParams,
                  const Expr &expr,
                  ActiveLocalBindings &activeLocals,
                  LocalBindingScope &scope) {
    (void)scope;
    if (expr.isBinding) {
      for (const auto &arg : expr.args) {
        LocalBindingScope argScope(*this, activeLocals);
        visitExpr(def, defParams, arg, activeLocals, argScope);
      }
      if (!expr.bodyArguments.empty()) {
        LocalBindingScope bodyScope(*this, activeLocals);
        visitExprSequence(def, defParams, expr.bodyArguments, activeLocals, bodyScope);
      }

      BindingInfo binding;
      if (inferBindingForLocals(def, defParams, activeLocals, expr, binding)) {
        entries.push_back(BindingFactSnapshotEntry{
            def.fullPath,
            "local",
            expr.name,
            {},
            expr.sourceLine,
            expr.sourceColumn,
            binding,
            expr.semanticNodeId,
        });
        insertLocalBinding(activeLocals, expr.name, std::move(binding));
      }
      return;
    }

    if (expr.kind == Expr::Kind::Name && expr.semanticNodeId != 0) {
      if (const BindingInfo *binding = findBinding(defParams, activeLocals, expr.name);
          binding != nullptr) {
        const bool isLocal = activeLocals.find(expr.name) != activeLocals.end();
        entries.push_back(BindingFactSnapshotEntry{
            def.fullPath,
            isLocal ? "local-reference" : "parameter-reference",
            expr.name,
            fallbackSnapshotBindingResolvedPath(def.fullPath, expr.name),
            expr.sourceLine,
            expr.sourceColumn,
            *binding,
            expr.semanticNodeId,
        });
      }
      return;
    }

    if (expr.kind == Expr::Kind::Call) {
      CallSnapshotData callData;
      if (inferCallSnapshotData(defParams, activeLocals, expr, callData) &&
          !callData.binding.typeName.empty()) {
        entries.push_back(BindingFactSnapshotEntry{
            def.fullPath,
            "temporary",
            expr.name,
            std::move(callData.resolvedPath),
            expr.sourceLine,
            expr.sourceColumn,
            std::move(callData.binding),
            expr.semanticNodeId,
        });
      }
    }

    for (const auto &arg : expr.args) {
      LocalBindingScope argScope(*this, activeLocals);
      visitExpr(def, defParams, arg, activeLocals, argScope);
    }
    if (!expr.bodyArguments.empty()) {
      LocalBindingScope bodyScope(*this, activeLocals);
      visitExprSequence(def, defParams, expr.bodyArguments, activeLocals, bodyScope);
    }
  };

  auto resetSnapshotInferenceCaches = [&]() {
    callTargetResolutionScratch_.resetArena();
    inferExprReturnKindMemo_.clear();
    inferExprReturnKindMemo_.rehash(0);
    inferStructReturnMemo_.clear();
    inferStructReturnMemo_.rehash(0);
    structFieldReturnKindMemo_.clear();
    structFieldReturnKindMemo_.rehash(0);
    localBindingMemoRevisionByIdentity_.clear();
    localBindingMemoRevisionByIdentity_.rehash(0);
    queryTypeInferenceDefinitionStack_.clear();
    queryTypeInferenceDefinitionStack_.rehash(0);
    queryTypeInferenceExprStack_.clear();
    queryTypeInferenceExprStack_.rehash(0);
  };

  for (const auto &def : program_.definitions) {
    DefinitionContextScope definitionScope(*this, def);
    ValidationStateScope validationStateScope(*this, buildDefinitionValidationState(def));
    const auto paramsIt = paramsByDef_.find(def.fullPath);
    if (paramsIt == paramsByDef_.end()) {
      resetSnapshotInferenceCaches();
      continue;
    }
    const auto &defParams = paramsIt->second;

    ActiveLocalBindings definitionLocals;
    LocalBindingScope definitionScopeLocals(*this, definitionLocals);
    visitExprSequence(def, defParams, def.statements, definitionLocals, definitionScopeLocals);
    if (def.returnExpr.has_value()) {
      LocalBindingScope returnScope(*this, definitionLocals);
      visitExpr(def, defParams, *def.returnExpr, definitionLocals, returnScope);
    }
    resetSnapshotInferenceCaches();
  }

  std::stable_sort(entries.begin(), entries.end(), [](const auto &left, const auto &right) {
    if (left.scopePath != right.scopePath) {
      return left.scopePath < right.scopePath;
    }
    if (left.sourceLine != right.sourceLine) {
      return left.sourceLine < right.sourceLine;
    }
    if (left.sourceColumn != right.sourceColumn) {
      return left.sourceColumn < right.sourceColumn;
    }
    if (left.siteKind != right.siteKind) {
      return left.siteKind < right.siteKind;
    }
    if (left.name != right.name) {
      return left.name < right.name;
    }
    return left.resolvedPath < right.resolvedPath;
  });
  return entries;
}

std::vector<ArrayExtentFactSnapshotEntry>
SemanticsValidator::arrayExtentFactSnapshotForSemanticProduct() {
  std::vector<ArrayExtentFactSnapshotEntry> entries;
  std::unordered_map<std::string, std::size_t> staticExtentsByResolvedPath;
  std::unordered_map<std::string, std::string> extentExpressionsByResolvedPath;

  for (const auto &def : program_.definitions) {
    collectStaticArrayExtentsForBindings(def.fullPath,
                                         def.statements,
                                         staticExtentsByResolvedPath,
                                         extentExpressionsByResolvedPath);
  }

  auto resolvedPathForBindingEntry = [](const BindingFactSnapshotEntry &entry) {
    return entry.resolvedPath.empty()
               ? fallbackSnapshotBindingResolvedPath(entry.scopePath, entry.name)
               : entry.resolvedPath;
  };

  for (const auto &bindingEntry : bindingFactSnapshotForSemanticProduct()) {
    SnapshotArrayExtentShape shape;
    if (!classifySnapshotArrayExtentBinding(bindingEntry.binding, shape)) {
      continue;
    }

    std::string siteKind;
    if (bindingEntry.siteKind == "local" && !shape.isReference) {
      siteKind = "local-value";
    } else if (bindingEntry.siteKind == "parameter" && shape.isReference) {
      siteKind = "parameter-reference";
    } else if (bindingEntry.siteKind == "parameter" && !shape.isReference) {
      siteKind = "parameter-value";
    } else {
      continue;
    }

    const std::string resolvedPath = resolvedPathForBindingEntry(bindingEntry);
    ArrayExtentFactSnapshotEntry entry;
    entry.scopePath = bindingEntry.scopePath;
    entry.siteKind = std::move(siteKind);
    entry.targetName = bindingEntry.name;
    entry.targetResolvedPath = resolvedPath;
    entry.binding = bindingEntry.binding;
    entry.elementTypeText = shape.elementTypeText;
    if (const auto extentExpression =
            extentExpressionsByResolvedPath.find(resolvedPath);
        extentExpression != extentExpressionsByResolvedPath.end()) {
      entry.extentExpression = extentExpression->second;
    } else {
      entry.extentExpression = "count(" + bindingEntry.name + ")";
    }
    entry.isReference = shape.isReference;
    if (const auto staticExtent = staticExtentsByResolvedPath.find(resolvedPath);
        staticExtent != staticExtentsByResolvedPath.end()) {
      entry.hasStaticExtent = true;
      entry.staticExtent = staticExtent->second;
    }
    entry.sourceLine = bindingEntry.sourceLine;
    entry.sourceColumn = bindingEntry.sourceColumn;
    entry.semanticNodeId = bindingEntry.semanticNodeId;
    entry.targetSemanticNodeId = bindingEntry.semanticNodeId;
    entries.push_back(std::move(entry));
  }

  forEachLocalAwareSnapshotCall(
      [&](const Definition &def,
          const std::vector<ParameterInfo> &defParams,
          const Expr &expr,
          const std::unordered_map<std::string, BindingInfo> &activeLocals) {
        const Expr *target = arrayExtentCountTarget(expr);
        if (target == nullptr || target->kind != Expr::Kind::Name) {
          return;
        }
        const BindingInfo *targetBinding =
            findBinding(defParams, activeLocals, target->name);
        if (targetBinding == nullptr) {
          return;
        }
        SnapshotArrayExtentShape shape;
        if (!classifySnapshotArrayExtentBinding(*targetBinding, shape)) {
          return;
        }

        const std::string resolvedPath =
            fallbackSnapshotBindingResolvedPath(def.fullPath, target->name);
        ArrayExtentFactSnapshotEntry entry;
        entry.scopePath = def.fullPath;
        entry.siteKind = "count-expression";
        entry.targetName = target->name;
        entry.targetResolvedPath = resolvedPath;
        entry.binding = *targetBinding;
        entry.elementTypeText = shape.elementTypeText;
        entry.extentExpression = "count(" + target->name + ")";
        entry.isReference = shape.isReference;
        if (const auto staticExtent = staticExtentsByResolvedPath.find(resolvedPath);
            staticExtent != staticExtentsByResolvedPath.end()) {
          entry.hasStaticExtent = true;
          entry.staticExtent = staticExtent->second;
        }
        entry.sourceLine = expr.sourceLine;
        entry.sourceColumn = expr.sourceColumn;
        entry.semanticNodeId = expr.semanticNodeId;
        entry.targetSemanticNodeId = target->semanticNodeId;
        entries.push_back(std::move(entry));
      });

  std::stable_sort(entries.begin(), entries.end(), [](const auto &left, const auto &right) {
    if (left.scopePath != right.scopePath) {
      return left.scopePath < right.scopePath;
    }
    if (left.sourceLine != right.sourceLine) {
      return left.sourceLine < right.sourceLine;
    }
    if (left.sourceColumn != right.sourceColumn) {
      return left.sourceColumn < right.sourceColumn;
    }
    if (left.siteKind != right.siteKind) {
      return left.siteKind < right.siteKind;
    }
    if (left.targetName != right.targetName) {
      return left.targetName < right.targetName;
    }
    if (left.extentExpression != right.extentExpression) {
      return left.extentExpression < right.extentExpression;
    }
    return left.semanticNodeId < right.semanticNodeId;
  });
  return entries;
}

std::vector<ReturnFactSnapshotEntry>
SemanticsValidator::returnFactSnapshotForSemanticProduct() {
  std::vector<ReturnFactSnapshotEntry> entries;
  entries.reserve(program_.definitions.size());

  auto withPreservedError = [&](const std::function<bool()> &fn) {
    return runWithPreservedDiagnostic(fn);
  };

  for (const auto &definition : program_.definitions) {
    const auto kindIt = returnKinds_.find(definition.fullPath);
    if (kindIt == returnKinds_.end()) {
      continue;
    }
    ReturnFactSnapshotEntry entry;
    entry.definitionPath = definition.fullPath;
    entry.kind = kindIt->second;
    entry.sourceLine = definition.returnExpr.has_value() ? definition.returnExpr->sourceLine : definition.sourceLine;
    entry.sourceColumn =
        definition.returnExpr.has_value() ? definition.returnExpr->sourceColumn : definition.sourceColumn;
    entry.semanticNodeId = definition.semanticNodeId;
    if (const auto structIt = returnStructs_.find(definition.fullPath);
        structIt != returnStructs_.end()) {
      entry.structPath = structIt->second;
    }
    if (const auto bindingIt = returnBindings_.find(definition.fullPath);
        bindingIt != returnBindings_.end()) {
      entry.binding = bindingIt->second;
    }
    if (entry.binding.typeName.empty()) {
      BindingInfo inferredBinding;
      if (withPreservedError([&]() {
            return inferDefinitionReturnBinding(definition, inferredBinding);
          })) {
        entry.binding = std::move(inferredBinding);
      }
    }
    if (entry.binding.typeName.empty()) {
      if (entry.kind == ReturnKind::Array && !entry.structPath.empty()) {
        entry.binding.typeName = entry.structPath;
      } else if (entry.kind == ReturnKind::Void) {
        entry.binding.typeName = "void";
      } else if (entry.kind != ReturnKind::Unknown) {
        entry.binding.typeName = typeNameForReturnKind(entry.kind);
      }
    }
    entries.push_back(std::move(entry));
  }
  std::stable_sort(entries.begin(), entries.end(), [](const auto &left, const auto &right) {
    return left.definitionPath < right.definitionPath;
  });
  return entries;
}

std::vector<LocalAutoBindingSnapshotEntry>
SemanticsValidator::localAutoFactSnapshotForSemanticProduct() const {
  return localAutoBindingSnapshotForTesting();
}

std::vector<QueryFactSnapshotEntry>
SemanticsValidator::queryFactSnapshotForSemanticProduct() {
  ensureQuerySnapshotFactCaches();
  queryFactSnapshotCacheValid_ = false;
  return std::exchange(queryFactSnapshotCache_, {});
}

std::vector<TryValueSnapshotEntry>
SemanticsValidator::tryFactSnapshotForSemanticProduct() {
  ensureCallAndTrySnapshotFactCaches(
      true, false);
  tryValueSnapshotCacheValid_ = false;
  return std::exchange(tryValueSnapshotCache_, {});
}

std::vector<OnErrorSnapshotEntry>
SemanticsValidator::onErrorFactSnapshotForSemanticProduct() {
  ensureOnErrorSnapshotFactCache();
  onErrorSnapshotFactCacheValid_ = false;
  return std::exchange(onErrorSnapshotCache_, {});
}

} // namespace primec::semantics
