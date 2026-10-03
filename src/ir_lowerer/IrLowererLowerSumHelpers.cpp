#include "IrLowererLowerSumHelpers.h"

#include <algorithm>

#include "IrLowererCountAccessClassifiers.h"
#include "primec/ir_lowerer/IrLowererFlowHelpers.h"
#include "primec/ir_lowerer/IrLowererHelpers.h"
#include "primec/ir_lowerer/IrLowererSetupTypeCollectionHelpers.h"
#include "primec/ir_lowerer/IrLowererSetupTypeHelpers.h"
#include "primec/ir_lowerer/IrLowererStructLayoutHelpers.h"
#include "primec/ir_lowerer/IrLowererTemplateTypeParseHelpers.h"
#include "primec/ir/StdlibCollectionPaths.h"

namespace primec::ir_lowerer {


SumHelpersContext::SumHelpersContext(LowerSetupStageState &setupStageIn,
                                     LowerReturnEmitStageState &stateOutIn,
                                     const CallResolutionAdapters &callResolutionAdaptersIn,
                                     std::string &errorIn)
    : callResolutionAdapters(callResolutionAdaptersIn),
      error(errorIn),
      function(setupStageIn.function),
      nextLocal(setupStageIn.nextLocal),
      defMap(setupStageIn.defMap),
      emitExpr(stateOutIn.emitExpr),
      emitStatement(stateOutIn.emitStatement),
      allocTempLocal(stateOutIn.allocTempLocal),
      resolveDefinitionCall(stateOutIn.resolveDefinitionCall),
      resolveStructTypeName(
          setupStageIn.setupLocalsOrchestration.setupTypeAndStructTypeAdapters
              .structTypeResolutionAdapters.resolveStructTypeName),
      resolveStructSlotLayout(
          setupStageIn.setupLocalsOrchestration.structSlotResolutionAdapters
              .resolveStructSlotLayout),
      resolveExprPath(callResolutionAdaptersIn.resolveExprPath),
      inferExprKind(setupStageIn.inferenceSetupBootstrap.inferExprKind),
      inferStructExprPath(
          setupStageIn.setupLocalsOrchestration.uninitializedResolutionAdapters
              .inferStructExprPath),
      emitStructCopyFromPtrs(stateOutIn.emitStructCopyFromPtrs),
      emitInlineDefinitionCall(stateOutIn.emitInlineDefinitionCall) {}

    bool SumHelpersContext::isLowerableSumDefinition(const Definition &def) {
      return ir_lowerer::definitionHasTransform(def, "sum");
    }

    const SumVariant * SumHelpersContext::findSumVariantByName(const Definition &sumDef,
                                   const std::string &variantName) {
      for (const auto &variant : sumDef.sumVariants) {
        if (variant.name == variantName) {
          return &variant;
        }
      }
      return nullptr;
    }

    bool SumHelpersContext::isStdlibResultSumDefinition(const Definition &sumDef) {
      return sumDef.fullPath == "/std/result/Result" ||
             sumDef.fullPath.rfind("/std/result/Result__", 0) == 0;
    }

    bool SumHelpersContext::isLegacyResultOkCall(const Expr &expr) {
      return expr.kind == Expr::Kind::Call && expr.isMethodCall &&
             !expr.isFieldAccess && expr.name == "ok" &&
             !expr.args.empty() && expr.args.front().kind == Expr::Kind::Name &&
             expr.args.front().name == "Result" &&
             expr.templateArgs.empty() && !expr.hasBodyArguments &&
             expr.bodyArguments.empty() && !hasNamedArguments(expr.argNames);
    }

    std::string SumHelpersContext::stripGeneratedResultHelperSuffix(std::string helperPath) {
      const size_t specializationMarker = helperPath.rfind("__t");
      if (specializationMarker != std::string::npos) {
        helperPath.erase(specializationMarker);
      }
      const size_t overloadMarker = helperPath.rfind("__ov");
      if (overloadMarker != std::string::npos) {
        helperPath.erase(overloadMarker);
      }
      return helperPath;
    }

    bool SumHelpersContext::isStdlibResultVariantHelperCall(const Expr &expr, const std::string &variantName) {
      if (expr.kind != Expr::Kind::Call || expr.hasBodyArguments ||
          !expr.bodyArguments.empty() || hasNamedArguments(expr.argNames)) {
        return false;
      }
      const std::string normalizedExprName =
          stripGeneratedResultHelperSuffix(expr.name);
      const std::string normalizedSourceName =
          stripGeneratedResultHelperSuffix(expr.sourceName);
      const std::string resultVariantCallName = "Result." + variantName;
      if ((normalizedExprName == resultVariantCallName ||
           normalizedSourceName == resultVariantCallName) &&
          !expr.isMethodCall) {
        return true;
      }
      if (normalizedExprName == "/std/result/" + variantName ||
          normalizedExprName == "/result/" + variantName ||
          normalizedSourceName == "/std/result/" + variantName ||
          normalizedSourceName == "/result/" + variantName) {
        return true;
      }
      if (!expr.isMethodCall &&
          (normalizedExprName == variantName ||
           normalizedSourceName == variantName) &&
          (expr.namespacePrefix == "Result" ||
           expr.namespacePrefix == "/result/Result" ||
           expr.namespacePrefix == "/std/result/Result")) {
        return true;
      }
      if (!expr.isMethodCall &&
          (normalizedExprName == variantName ||
           normalizedSourceName == variantName) &&
          expr.templateArgs.size() == 2) {
        return true;
      }
      if (expr.isMethodCall &&
          (normalizedExprName == variantName ||
           normalizedSourceName == variantName) &&
          !expr.args.empty() && expr.args.front().kind == Expr::Kind::Name &&
          (expr.args.front().name == "Result" ||
           expr.args.front().name == "/result/Result" ||
           expr.args.front().name == "/std/result/Result")) {
        return true;
      }
      const auto &semanticTargets = callResolutionAdapters.semanticProductTargets;
      auto semanticResolvedPathMatches = [&](SymbolId resolvedPathId) {
        if (!semanticTargets.hasSemanticProduct ||
            semanticTargets.semanticProgram == nullptr ||
            resolvedPathId == InvalidSymbolId) {
          return false;
        }
        const std::string resolvedPath =
            std::string(semanticProgramResolveCallTargetString(
                *semanticTargets.semanticProgram, resolvedPathId));
        return resolvedPath == "/std/result/" + variantName ||
               resolvedPath == "/result/" + variantName;
      };
      if (const SemanticProgramBindingFact *bindingFact =
              findSemanticProductBindingFact(semanticTargets, expr);
          bindingFact != nullptr &&
          semanticResolvedPathMatches(bindingFact->resolvedPathId)) {
        return true;
      }
      if (const SemanticProgramQueryFact *queryFact =
              findSemanticProductQueryFact(semanticTargets, expr);
          queryFact != nullptr &&
          semanticResolvedPathMatches(queryFact->resolvedPathId)) {
        return true;
      }
      const Definition *callee = resolveDefinitionCall(expr);
      if (callee == nullptr) {
        return false;
      }
      const std::string calleePath =
          stripGeneratedResultHelperSuffix(callee->fullPath);
      return calleePath == "/std/result/" + variantName ||
             calleePath == "/result/" + variantName;
    }

    unsigned SumHelpersContext::stdlibResultVariantHelperPayloadIndex(const Expr &expr) {
      return !expr.args.empty() && expr.args.front().kind == Expr::Kind::Name &&
                     expr.args.front().name == "Result"
                 ? 1u
                 : 0u;
    }

    bool SumHelpersContext::isLegacyResultMapCall(const Expr &expr) {
      return expr.kind == Expr::Kind::Call && !expr.isFieldAccess && expr.name == "map" &&
             expr.args.size() == 3 && expr.args.front().kind == Expr::Kind::Name &&
             expr.args.front().name == "Result" &&
             expr.templateArgs.empty() && !expr.hasBodyArguments &&
             expr.bodyArguments.empty() && !hasNamedArguments(expr.argNames);
    }

    bool SumHelpersContext::isLegacyResultAndThenCall(const Expr &expr) {
      return expr.kind == Expr::Kind::Call && !expr.isFieldAccess && expr.name == "and_then" &&
             expr.args.size() == 3 && expr.args.front().kind == Expr::Kind::Name &&
             expr.args.front().name == "Result" &&
             expr.templateArgs.empty() && !expr.hasBodyArguments &&
             expr.bodyArguments.empty() && !hasNamedArguments(expr.argNames);
    }

    bool SumHelpersContext::isLegacyResultMap2Call(const Expr &expr) {
      return expr.kind == Expr::Kind::Call && !expr.isFieldAccess && expr.name == "map2" &&
             expr.args.size() == 4 && expr.args.front().kind == Expr::Kind::Name &&
             expr.args.front().name == "Result" &&
             expr.templateArgs.empty() && !expr.hasBodyArguments &&
             expr.bodyArguments.empty() && !hasNamedArguments(expr.argNames);
    }

    std::string SumHelpersContext::sumPayloadTypeText(const SumVariant &variant) {
      if (!variant.hasPayload) {
        return std::string{};
      }
      if (!variant.payloadTypeText.empty()) {
        return trimTemplateTypeText(variant.payloadTypeText);
      }
      if (variant.payloadTemplateArgs.empty()) {
        return trimTemplateTypeText(variant.payloadType);
      }
      return trimTemplateTypeText(variant.payloadType) + "<" +
             joinTemplateArgsText(variant.payloadTemplateArgs) + ">";
    }

    bool SumHelpersContext::splitPointerLikePayloadTypeText(const std::string &typeText,
                                              std::string &elementTypeTextOut) {
      const std::string normalized = trimTemplateTypeText(typeText);
      std::string base;
      std::string arg;
      if (!splitTemplateTypeName(normalized, base, arg)) {
        return false;
      }
      const std::string trimmedBase = trimTemplateTypeText(base);
      if (trimmedBase != "Pointer" && trimmedBase != "/Pointer" &&
          trimmedBase != "Reference" && trimmedBase != "/Reference") {
        return false;
      }
      elementTypeTextOut = trimTemplateTypeText(arg);
      return true;
    }

    LocalInfo::ValueKind SumHelpersContext::pointerLikePayloadValueKind(const std::string &typeText) {
      std::string elementTypeText;
      if (!splitPointerLikePayloadTypeText(typeText, elementTypeText)) {
        return LocalInfo::ValueKind::Unknown;
      }
      return LocalInfo::ValueKind::Int64;
    }

    LocalInfo::ValueKind SumHelpersContext::valueKindOrPointerLikeFromTypeName(const std::string &typeText) {
      const LocalInfo::ValueKind kind = valueKindFromTypeName(typeText);
      if (kind != LocalInfo::ValueKind::Unknown) {
        return kind;
      }
      return pointerLikePayloadValueKind(typeText);
    }

    void SumHelpersContext::applyStdlibResultSumInfoToLocal(const Definition &sumDef, LocalInfo &info) {
      if (!isStdlibResultSumDefinition(sumDef)) {
        return;
      }
      const SumVariant *okVariant = findSumVariantByName(sumDef, "ok");
      const SumVariant *errorVariant = findSumVariantByName(sumDef, "error");
      info.isResult = true;
      info.resultHasValue = okVariant != nullptr && okVariant->hasPayload;
      info.resultValueKind = LocalInfo::ValueKind::Unknown;
      info.resultValueCollectionKind = LocalInfo::Kind::Value;
      info.resultValueMapKeyKind = LocalInfo::ValueKind::Unknown;
      info.resultValueIsFileHandle = false;
      info.resultValueStructType.clear();
      info.resultErrorType.clear();
      if (okVariant != nullptr && okVariant->hasPayload) {
        const std::string valueTypeText = sumPayloadTypeText(*okVariant);
        info.resultValueKind = valueKindFromTypeName(valueTypeText);
        if (info.resultValueKind == LocalInfo::ValueKind::Unknown) {
          std::string valueStructPath;
          if (resolveStructTypeName(valueTypeText,
                                    sumDef.namespacePrefix,
                                    valueStructPath)) {
            info.resultValueStructType = std::move(valueStructPath);
          }
        }
      }
      if (errorVariant != nullptr && errorVariant->hasPayload) {
        info.resultErrorType = sumPayloadTypeText(*errorVariant);
      }
    }

    bool SumHelpersContext::resolveSumPayloadStorageInfo(const Definition &sumDef,
            const SumVariant &variant,
            LoweredSumPayloadStorageInfo &infoOut) {
      infoOut = {};
      if (!variant.hasPayload) {
        infoOut.slotCount = 0;
        return true;
      }
      const std::string payloadTypeText = sumPayloadTypeText(variant);
      std::string pointerElementTypeText;
      if (splitPointerLikePayloadTypeText(payloadTypeText, pointerElementTypeText)) {
        infoOut.valueKind = LocalInfo::ValueKind::Int64;
        infoOut.isPointerLike = true;
        infoOut.pointerElementTypeText = std::move(pointerElementTypeText);
        infoOut.slotCount = 1;
        return true;
      }
      infoOut.valueKind = valueKindFromTypeName(payloadTypeText);
      if (infoOut.valueKind != LocalInfo::ValueKind::Unknown) {
        infoOut.slotCount = 1;
        return true;
      }
      std::string payloadStructPath;
      if (!resolveStructTypeName(payloadTypeText,
                                 sumDef.namespacePrefix,
                                 payloadStructPath)) {
        return false;
      }
      StructSlotLayout payloadLayout;
      if (!resolveStructSlotLayout(payloadStructPath, payloadLayout)) {
        return false;
      }
      infoOut.structPath = std::move(payloadStructPath);
      infoOut.slotCount = payloadLayout.totalSlots;
      infoOut.isAggregate = true;
      return true;
    }

    bool SumHelpersContext::resolvePublishedSumPayloadStorageInfo(const Definition &sumDef,
            const SemanticProgramSumVariantMetadata &publishedVariant,
            LoweredSumPayloadStorageInfo &infoOut) {
      infoOut = {};
      if (!publishedVariant.hasPayload) {
        infoOut.slotCount = 0;
        return true;
      }
      const std::string payloadTypeText =
          trimTemplateTypeText(publishedVariant.payloadTypeText);
      std::string pointerElementTypeText;
      if (splitPointerLikePayloadTypeText(payloadTypeText, pointerElementTypeText)) {
        infoOut.valueKind = LocalInfo::ValueKind::Int64;
        infoOut.isPointerLike = true;
        infoOut.pointerElementTypeText = std::move(pointerElementTypeText);
        infoOut.slotCount = 1;
        return true;
      }
      infoOut.valueKind = valueKindFromTypeName(payloadTypeText);
      if (infoOut.valueKind != LocalInfo::ValueKind::Unknown) {
        infoOut.slotCount = 1;
        return true;
      }
      std::string payloadStructPath;
      if (!resolveStructTypeName(payloadTypeText,
                                 sumDef.namespacePrefix,
                                 payloadStructPath)) {
        return false;
      }
      StructSlotLayout payloadLayout;
      if (!resolveStructSlotLayout(payloadStructPath, payloadLayout)) {
        return false;
      }
      infoOut.structPath = std::move(payloadStructPath);
      infoOut.slotCount = payloadLayout.totalSlots;
      infoOut.isAggregate = true;
      return true;
    }

    const Definition * SumHelpersContext::resolveSumDefinitionByPath(const std::string &path) {
      if (path.empty()) {
        return nullptr;
      }
      auto defIt = defMap.find(path);
      if (defIt == defMap.end() || defIt->second == nullptr ||
          !isLowerableSumDefinition(*defIt->second)) {
        return nullptr;
      }
      return defIt->second;
    }

    const Definition * SumHelpersContext::resolveSumDefinitionForTypeText(const std::string &typeText, const std::string &namespacePrefix) {
      std::string normalized = trimTemplateTypeText(typeText);
      std::string base;
      std::string argList;
      if (splitTemplateTypeName(normalized, base, argList)) {
        normalized = trimTemplateTypeText(base);
      }
      if (normalized.empty()) {
        return nullptr;
      }

      std::vector<std::string> candidatePaths;
      auto addCandidate = [&](std::string candidate) {
        candidate = trimTemplateTypeText(candidate);
        if (!candidate.empty()) {
          candidatePaths.push_back(std::move(candidate));
        }
      };
      addCandidate(normalized);
      if (normalized.front() != '/') {
        addCandidate("/" + normalized);
        if (!namespacePrefix.empty()) {
          addCandidate(namespacePrefix + "/" + normalized);
        }
      }
      Expr syntheticTypeExpr;
      syntheticTypeExpr.kind = Expr::Kind::Call;
      syntheticTypeExpr.name = normalized;
      syntheticTypeExpr.namespacePrefix = namespacePrefix;
      addCandidate(resolveExprPath(syntheticTypeExpr));

      std::sort(candidatePaths.begin(), candidatePaths.end());
      candidatePaths.erase(std::unique(candidatePaths.begin(), candidatePaths.end()), candidatePaths.end());
      for (const std::string &candidate : candidatePaths) {
        if (const Definition *sumDef = resolveSumDefinitionByPath(candidate)) {
          return sumDef;
        }
      }

      const std::string suffix = normalized.front() == '/' ? normalized : "/" + normalized;
      std::vector<std::string> suffixMatches;
      for (const auto &[path, def] : defMap) {
        if (def == nullptr || !isLowerableSumDefinition(*def)) {
          continue;
        }
        if (path.size() >= suffix.size() &&
            path.compare(path.size() - suffix.size(), suffix.size(), suffix) == 0) {
          suffixMatches.push_back(path);
        }
      }
      std::sort(suffixMatches.begin(), suffixMatches.end());
      suffixMatches.erase(std::unique(suffixMatches.begin(), suffixMatches.end()), suffixMatches.end());
      return suffixMatches.size() == 1 ? resolveSumDefinitionByPath(suffixMatches.front()) : nullptr;
    }

    const Definition * SumHelpersContext::resolveSumDefinitionForLocalInfo(const LocalInfo &info) {
      return resolveSumDefinitionForTypeText(info.structTypeName, function.name);
    }

    std::string SumHelpersContext::unsupportedSumPayloadError(const Definition &sumDef, const SumVariant &variant) {
      return "native backend does not support sum payload type: " +
             sumDef.fullPath + "/" + variant.name + " (" + sumPayloadTypeText(variant) + ")";
    }

    bool SumHelpersContext::resolveSemanticProductSumVariantMetadata(const Definition &sumDef,
            const SumVariant &variant,
            std::string_view operationLabel,
            const SemanticProgramSumVariantMetadata *&publishedVariantOut) {
      publishedVariantOut = nullptr;
      const auto &semanticTargets = callResolutionAdapters.semanticProductTargets;
      if (!semanticTargets.hasSemanticProduct || semanticTargets.semanticProgram == nullptr) {
        return true;
      }
      const SemanticProgramSumVariantMetadata *publishedVariant =
          findSemanticProductSumVariantMetadata(semanticTargets, sumDef.fullPath, variant.name);
      const std::string diagnosticSuffix = sumDef.fullPath + " -> " + variant.name;
      if (publishedVariant == nullptr) {
        error = "missing semantic-product sum variant metadata for " +
                std::string(operationLabel) + ": " + diagnosticSuffix;
        return false;
      }
      const std::string expectedPayloadType =
          variant.hasPayload ? sumPayloadTypeText(variant) : std::string{};
      const uint32_t expectedTagValue = static_cast<uint32_t>(variant.variantIndex);
      if (publishedVariant->variantIndex != variant.variantIndex ||
          publishedVariant->tagValue != expectedTagValue ||
          publishedVariant->hasPayload != variant.hasPayload ||
          trimTemplateTypeText(publishedVariant->payloadTypeText) != expectedPayloadType) {
        error = "stale semantic-product sum variant metadata for " +
                std::string(operationLabel) + ": " + diagnosticSuffix;
        return false;
      }
      publishedVariantOut = publishedVariant;
      return true;
    }

    bool SumHelpersContext::resolveSemanticProductSumVariantTag(const Definition &sumDef,
            const SumVariant &variant,
            std::string_view operationLabel,
            int32_t &tagValueOut) {
      tagValueOut = static_cast<int32_t>(variant.variantIndex);
      const SemanticProgramSumVariantMetadata *publishedVariant = nullptr;
      if (!resolveSemanticProductSumVariantMetadata(
              sumDef, variant, operationLabel, publishedVariant)) {
        return false;
      }
      if (publishedVariant != nullptr) {
        tagValueOut = static_cast<int32_t>(publishedVariant->tagValue);
      }
      return true;
    }

    bool SumHelpersContext::resolveSemanticProductSumPayloadStorageInfo(const Definition &sumDef,
            const SumVariant &variant,
            std::string_view operationLabel,
            LoweredSumPayloadStorageInfo &infoOut) {
      const SemanticProgramSumVariantMetadata *publishedVariant = nullptr;
      if (!resolveSemanticProductSumVariantMetadata(
              sumDef, variant, operationLabel, publishedVariant)) {
        return false;
      }
      if (publishedVariant == nullptr) {
        return resolveSumPayloadStorageInfo(sumDef, variant, infoOut);
      }
      return resolvePublishedSumPayloadStorageInfo(sumDef, *publishedVariant, infoOut);
    }

    bool SumHelpersContext::loweredSumSlotCount(const Definition &sumDef, int32_t &totalSlotsOut) {
      int32_t maxPayloadSlots = 0;
      for (const auto &variant : sumDef.sumVariants) {
        LoweredSumPayloadStorageInfo payloadInfo;
        if (!resolveSemanticProductSumPayloadStorageInfo(
                sumDef, variant, "sum slot layout", payloadInfo)) {
          totalSlotsOut = 0;
          return false;
        }
        maxPayloadSlots = std::max(maxPayloadSlots, payloadInfo.slotCount);
      }
      totalSlotsOut = 2 + maxPayloadSlots;
      return true;
    }

    const SumVariant * SumHelpersContext::defaultUnitVariant(const Definition &sumDef) {
      if (sumDef.sumVariants.empty() || sumDef.sumVariants.front().hasPayload) {
        return nullptr;
      }
      return &sumDef.sumVariants.front();
    }

    const SumVariant * SumHelpersContext::unitVariantForNameExpr(const Definition &sumDef, const Expr &expr) {
      if (expr.kind != Expr::Kind::Name) {
        return nullptr;
      }
      const SumVariant *variant = findSumVariantByName(sumDef, expr.name);
      return variant != nullptr && !variant->hasPayload ? variant : nullptr;
    }

    const SumVariant * SumHelpersContext::unitVariantForConstructorArg(const Definition &sumDef, const Expr &arg) {
      if (const SumVariant *variant = unitVariantForNameExpr(sumDef, arg)) {
        return variant;
      }
      if (arg.kind != Expr::Kind::Call || arg.name != "block" ||
          !arg.hasBodyArguments || !arg.args.empty() ||
          arg.bodyArguments.size() != 1) {
        return nullptr;
      }
      return unitVariantForNameExpr(sumDef, arg.bodyArguments.front());
    }

    const SumVariant * SumHelpersContext::payloadVariantForConstructorArg(const Definition &sumDef, const Expr &arg) {
      const SumVariant *matchedVariant = nullptr;
      for (const auto &transform : arg.transforms) {
        if (!transform.arguments.empty()) {
          continue;
        }
        const SumVariant *variant = findSumVariantByName(sumDef, transform.name);
        if (variant == nullptr || !variant->hasPayload) {
          continue;
        }
        if (matchedVariant != nullptr) {
          return nullptr;
        }
        matchedVariant = variant;
      }
      return matchedVariant;
    }

    const SumVariant * SumHelpersContext::firstUnsupportedSumPayloadVariant(const Definition &sumDef) {
      for (const auto &variant : sumDef.sumVariants) {
        LoweredSumPayloadStorageInfo payloadInfo;
        if (!resolveSumPayloadStorageInfo(sumDef, variant, payloadInfo)) {
          return &variant;
        }
      }
      return nullptr;
    }

    bool SumHelpersContext::selectExplicitSumVariantForConstructor(const Expr &initializer,
            const Definition &targetSum,
            LoweredSumVariantSelection &selectionOut) {
      selectionOut = {};
      if (initializer.kind != Expr::Kind::Call || initializer.isMethodCall ||
          initializer.isFieldAccess) {
        return false;
      }
      auto constructorNameMatchesTargetSum = [&]() {
        std::string targetPath = targetSum.fullPath;
        if (const size_t arityMarker = targetPath.rfind("__arity");
            arityMarker != std::string::npos) {
          targetPath.erase(arityMarker);
        }
        if (const size_t specializationMarker = targetPath.rfind("__t");
            specializationMarker != std::string::npos) {
          targetPath.erase(specializationMarker);
        }
        const size_t slash = targetPath.find_last_of('/');
        const std::string targetName =
            slash == std::string::npos ? targetPath : targetPath.substr(slash + 1);
        return initializer.name == targetName ||
               initializer.name == targetPath ||
               (!targetPath.empty() && targetPath.front() == '/' &&
                initializer.name == targetPath.substr(1));
      };
      const Definition *constructorSum =
          resolveSumDefinitionForTypeText(initializer.name, initializer.namespacePrefix);
      if ((constructorSum == nullptr || constructorSum->fullPath != targetSum.fullPath) &&
          !constructorNameMatchesTargetSum()) {
        return false;
      }
      const std::vector<Expr> *constructorArgs = &initializer.args;
      const std::vector<std::optional<std::string>> *constructorArgNames =
          &initializer.argNames;
      std::vector<Expr> normalizedArgs;
      std::vector<std::optional<std::string>> normalizedArgNames;
      if (initializer.args.size() == 1 && initializer.argNames.size() == 1 &&
          !initializer.argNames.front().has_value() &&
          initializer.args.front().kind == Expr::Kind::Call &&
          initializer.args.front().name == "block" &&
          initializer.args.front().hasBodyArguments &&
          initializer.args.front().bodyArguments.empty() &&
          initializer.args.front().args.empty()) {
        constructorArgs = &normalizedArgs;
        constructorArgNames = &normalizedArgNames;
      }
      const SumVariant *variant = nullptr;
      if (constructorArgs->empty() && constructorArgNames->empty()) {
        variant = defaultUnitVariant(targetSum);
      } else if (constructorArgs->size() == 1 && constructorArgNames->size() == 1 &&
                 !constructorArgNames->front().has_value()) {
        variant = unitVariantForConstructorArg(targetSum, constructorArgs->front());
        if (variant == nullptr) {
          variant = payloadVariantForConstructorArg(targetSum, constructorArgs->front());
        }
      } else if (constructorArgs->size() == 1 && constructorArgNames->size() == 1 &&
                 constructorArgNames->front().has_value()) {
        variant = findSumVariantByName(targetSum, *constructorArgNames->front());
      }
      if (variant == nullptr) {
        return false;
      }
      LoweredSumPayloadStorageInfo payloadInfo;
      if (!resolveSemanticProductSumPayloadStorageInfo(
              targetSum, *variant, "sum constructor selection", payloadInfo)) {
        return false;
      }
      selectionOut.sumDef = &targetSum;
      selectionOut.variant = variant;
      selectionOut.payloadExpr = variant->hasPayload ? &constructorArgs->front() : nullptr;
      selectionOut.payloadKind = payloadInfo.valueKind;
      selectionOut.payloadStructPath = std::move(payloadInfo.structPath);
      selectionOut.payloadSlotCount = payloadInfo.slotCount;
      selectionOut.payloadIsAggregate = payloadInfo.isAggregate;
      return true;
    }

    bool SumHelpersContext::selectSumVariantForInitializer(const Expr &initializer,
            const Definition &targetSum,
            const LocalMap &valueLocals,
            LoweredSumVariantSelection &selectionOut) {
      if (isStdlibResultSumDefinition(targetSum)) {
        for (const char *variantName : {"ok", "error"}) {
          if (!isStdlibResultVariantHelperCall(initializer, variantName)) {
            continue;
          }
          const SumVariant *variant = findSumVariantByName(targetSum, variantName);
          if (variant == nullptr) {
            return false;
          }
          const size_t payloadIndex =
              stdlibResultVariantHelperPayloadIndex(initializer);
          if (variant->hasPayload !=
              (initializer.args.size() == payloadIndex + 1)) {
            return false;
          }
          LoweredSumPayloadStorageInfo payloadInfo;
          if (!resolveSemanticProductSumPayloadStorageInfo(
                  targetSum, *variant, "Result helper selection", payloadInfo)) {
            return false;
          }
          selectionOut.sumDef = &targetSum;
          selectionOut.variant = variant;
          selectionOut.payloadExpr =
              variant->hasPayload ? &initializer.args[payloadIndex] : nullptr;
          selectionOut.payloadKind = payloadInfo.valueKind;
          selectionOut.payloadStructPath = std::move(payloadInfo.structPath);
          selectionOut.payloadSlotCount = payloadInfo.slotCount;
          selectionOut.payloadIsAggregate = payloadInfo.isAggregate;
          return true;
        }
      }
      if (isStdlibResultSumDefinition(targetSum) && isLegacyResultOkCall(initializer)) {
        if (initializer.args.size() != 2 && initializer.args.size() != 1) {
          return false;
        }
        const SumVariant *variant = findSumVariantByName(targetSum, "ok");
        if (variant == nullptr) {
          return false;
        }
        if (variant->hasPayload != (initializer.args.size() == 2)) {
          return false;
        }
        LoweredSumPayloadStorageInfo payloadInfo;
        if (!resolveSemanticProductSumPayloadStorageInfo(
                targetSum, *variant, "Result.ok selection", payloadInfo)) {
          return false;
        }
        selectionOut.sumDef = &targetSum;
        selectionOut.variant = variant;
        selectionOut.payloadExpr = variant->hasPayload ? &initializer.args[1] : nullptr;
        selectionOut.payloadKind = payloadInfo.valueKind;
        selectionOut.payloadStructPath = std::move(payloadInfo.structPath);
        selectionOut.payloadSlotCount = payloadInfo.slotCount;
        selectionOut.payloadIsAggregate = payloadInfo.isAggregate;
        return true;
      }
      if (selectExplicitSumVariantForConstructor(initializer, targetSum, selectionOut)) {
        return true;
      }
      if (!error.empty()) {
        return false;
      }
      if (const SumVariant *variant = unitVariantForNameExpr(targetSum, initializer)) {
        selectionOut.sumDef = &targetSum;
        selectionOut.variant = variant;
        return true;
      }
      if (initializer.kind == Expr::Kind::Call && initializer.name == "block" &&
          initializer.hasBodyArguments && initializer.bodyArguments.empty() &&
          initializer.args.empty()) {
        if (const SumVariant *variant = defaultUnitVariant(targetSum)) {
          selectionOut.sumDef = &targetSum;
          selectionOut.variant = variant;
          return true;
        }
      }
      const SumVariant *matchedVariant = nullptr;
      LoweredSumPayloadStorageInfo matchedPayloadInfo;
      auto resolveSemanticProductInitializerPayloadShape =
          [&](LocalInfo::ValueKind &initializerKindOut,
              std::string &initializerStructPathOut) -> std::optional<bool> {
        initializerKindOut = LocalInfo::ValueKind::Unknown;
        initializerStructPathOut.clear();
        const auto &semanticTargets = callResolutionAdapters.semanticProductTargets;
        if (!semanticTargets.hasSemanticProduct || initializer.semanticNodeId == 0) {
          return std::nullopt;
        }
        std::vector<std::string> candidateTypeTexts;
        auto addSemanticProductCandidateTypeText =
            [&](const std::string &typeText, auto typeTextId) {
          std::string resolvedTypeText;
          if (semanticTargets.semanticProgram != nullptr &&
              typeTextId != InvalidSymbolId) {
            resolvedTypeText = std::string(semanticProgramResolveCallTargetString(
                *semanticTargets.semanticProgram,
                typeTextId));
          }
          if (resolvedTypeText.empty()) {
            resolvedTypeText = typeText;
          }
          resolvedTypeText = trimTemplateTypeText(resolvedTypeText);
          if (!resolvedTypeText.empty()) {
            candidateTypeTexts.push_back(std::move(resolvedTypeText));
          }
        };
        if (const SemanticProgramBindingFact *bindingFact =
                findSemanticProductBindingFact(semanticTargets, initializer);
            bindingFact != nullptr) {
          addSemanticProductCandidateTypeText(bindingFact->bindingTypeText,
                                              bindingFact->bindingTypeTextId);
        }
        if (const SemanticProgramQueryFact *queryFact =
                findSemanticProductQueryFact(semanticTargets, initializer);
            queryFact != nullptr) {
          addSemanticProductCandidateTypeText(queryFact->bindingTypeText,
                                              queryFact->bindingTypeTextId);
          addSemanticProductCandidateTypeText(queryFact->queryTypeText,
                                              queryFact->queryTypeTextId);
        }
        if (candidateTypeTexts.empty()) {
          return std::nullopt;
        }
        auto resolveTypeText = [&](const std::string &typeText) {
          const std::string normalizedTypeText = trimTemplateTypeText(typeText);
          if (normalizedTypeText.empty()) {
            return false;
          }
          const LocalInfo::ValueKind valueKind = valueKindOrPointerLikeFromTypeName(normalizedTypeText);
          if (valueKind != LocalInfo::ValueKind::Unknown) {
            initializerKindOut = valueKind;
            return true;
          }
          Expr syntheticTypeExpr;
          syntheticTypeExpr.kind = Expr::Kind::Call;
          syntheticTypeExpr.name = normalizedTypeText;
          syntheticTypeExpr.namespacePrefix = initializer.namespacePrefix;
          if (const Definition *typeDef = resolveDefinitionCall(syntheticTypeExpr);
              typeDef != nullptr && ir_lowerer::isStructDefinition(*typeDef)) {
            initializerStructPathOut = typeDef->fullPath;
            return true;
          }
          if (!targetSum.namespacePrefix.empty() &&
              !normalizedTypeText.empty() && normalizedTypeText.front() != '/') {
            syntheticTypeExpr.name = normalizedTypeText;
            syntheticTypeExpr.namespacePrefix = targetSum.namespacePrefix;
            if (const Definition *typeDef = resolveDefinitionCall(syntheticTypeExpr);
                typeDef != nullptr && ir_lowerer::isStructDefinition(*typeDef)) {
              initializerStructPathOut = typeDef->fullPath;
              return true;
            }
          }
          if (!normalizedTypeText.empty() && normalizedTypeText.front() != '/') {
            syntheticTypeExpr.name = "/" + normalizedTypeText;
            syntheticTypeExpr.namespacePrefix.clear();
            if (const Definition *typeDef = resolveDefinitionCall(syntheticTypeExpr);
                typeDef != nullptr && ir_lowerer::isStructDefinition(*typeDef)) {
              initializerStructPathOut = typeDef->fullPath;
              return true;
            }
          }
          return false;
        };
        for (const std::string &typeText : candidateTypeTexts) {
          if (resolveTypeText(typeText)) {
            return true;
          }
        }
        error = "stale semantic-product sum initializer type metadata: " +
                targetSum.fullPath;
        return false;
      };
      LocalInfo::ValueKind initializerKind = LocalInfo::ValueKind::Unknown;
      std::string initializerStructPath;
      const std::optional<bool> resolvedInitializerShape =
          resolveSemanticProductInitializerPayloadShape(initializerKind, initializerStructPath);
      if (resolvedInitializerShape.has_value() && !*resolvedInitializerShape) {
        return false;
      }
      if (!resolvedInitializerShape.has_value()) {
        initializerKind = inferExprKind(initializer, valueLocals);
        initializerStructPath = inferStructExprPath(initializer, valueLocals);
        if (initializerStructPath.empty() && initializer.kind == Expr::Kind::Call) {
          if (const Definition *initCallee = resolveDefinitionCall(initializer);
              initCallee != nullptr && ir_lowerer::isStructDefinition(*initCallee)) {
            initializerStructPath = initCallee->fullPath;
          }
        }
      }
      for (const auto &variant : targetSum.sumVariants) {
        if (!variant.hasPayload) {
          continue;
        }
        LoweredSumPayloadStorageInfo payloadInfo;
        if (!resolveSemanticProductSumPayloadStorageInfo(
                targetSum, variant, "sum initializer selection", payloadInfo)) {
          if (!error.empty()) {
            return false;
          }
          continue;
        }
        const bool matchesPayload =
            payloadInfo.isAggregate
                ? (!initializerStructPath.empty() &&
                   initializerStructPath == payloadInfo.structPath)
                : payloadInfo.valueKind == initializerKind;
        if (!matchesPayload) {
          continue;
        }
        if (matchedVariant != nullptr) {
          return false;
        }
        matchedVariant = &variant;
        matchedPayloadInfo = std::move(payloadInfo);
      }
      if (matchedVariant == nullptr) {
        return false;
      }
      selectionOut.sumDef = &targetSum;
      selectionOut.variant = matchedVariant;
      selectionOut.payloadExpr = &initializer;
      selectionOut.payloadKind = matchedPayloadInfo.valueKind;
      selectionOut.payloadStructPath = std::move(matchedPayloadInfo.structPath);
      selectionOut.payloadSlotCount = matchedPayloadInfo.slotCount;
      selectionOut.payloadIsAggregate = matchedPayloadInfo.isAggregate;
      return true;
    }

    void SumHelpersContext::emitLoweredSumHeader(int32_t baseLocal, int32_t totalSlots) {
      function.instructions.push_back(
          {IrOpcode::PushI32, static_cast<uint64_t>(static_cast<int32_t>(totalSlots - 1))});
      function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal)});
    }


} // namespace primec::ir_lowerer
