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

    bool SumHelpersContext::emitLoweredSumConstructionIntoLocal(int32_t baseLocal,
            const Definition &sumDef,
            const Expr &initializer,
            const LocalMap &valueLocals) {
      auto emitLoadSumSlotIndirectForConstruction = [&](int32_t sumPtrLocal, int32_t slotOffset) {
        function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(sumPtrLocal)});
        if (slotOffset != 0) {
          function.instructions.push_back({IrOpcode::PushI64, static_cast<uint64_t>(slotOffset) * IrSlotBytes});
          function.instructions.push_back({IrOpcode::AddI64, 0});
        }
        function.instructions.push_back({IrOpcode::LoadIndirect, 0});
      };
      auto emitSumTagComparisonForConstruction = [&](int32_t sumPtrLocal, int32_t tagValue) {
        emitLoadSumSlotIndirectForConstruction(sumPtrLocal, 1);
        function.instructions.push_back(
            {IrOpcode::PushI32, static_cast<uint64_t>(static_cast<int32_t>(tagValue))});
        function.instructions.push_back({IrOpcode::CmpEqI32, 0});
      };
      auto findResultLambdaValueExpr = [](const Expr &lambdaExpr) -> const Expr * {
        for (size_t i = 0; i < lambdaExpr.bodyArguments.size(); ++i) {
          const Expr &bodyExpr = lambdaExpr.bodyArguments[i];
          const bool isLast = (i + 1 == lambdaExpr.bodyArguments.size());
          if (bodyExpr.isBinding) {
            continue;
          }
          if (isReturnCall(bodyExpr)) {
            if (bodyExpr.args.size() != 1 || !isLast) {
              return nullptr;
            }
            return &bodyExpr.args.front();
          }
          if (isLast) {
            return &bodyExpr;
          }
        }
        return nullptr;
      };
      auto resultLambdaParameterName = [](const Expr &parameterExpr) {
        if ((parameterExpr.kind == Expr::Kind::Name || parameterExpr.isBinding) &&
            !parameterExpr.name.empty()) {
          return parameterExpr.name;
        }
        return std::string{};
      };
      auto emitResultLambdaPrefixStatements =
          [&](const Expr &lambdaExpr, const Expr *valueExpr, LocalMap &lambdaLocals) -> bool {
        for (const Expr &bodyExpr : lambdaExpr.bodyArguments) {
          if (isReturnCall(bodyExpr) && bodyExpr.args.size() == 1 && valueExpr == &bodyExpr.args.front()) {
            return true;
          }
          if (&bodyExpr == valueExpr) {
            return true;
          }
          if (!emitStatement(bodyExpr, lambdaLocals)) {
            return false;
          }
        }
        return true;
      };
      auto bindSumPayloadLocal =
          [&](const Definition &payloadSumDef,
              const SumVariant &payloadVariant,
              int32_t sumPtrLocal,
              const std::string &payloadName,
              std::string_view operationLabel,
              LocalMap &payloadLocals) -> bool {
        LoweredSumPayloadStorageInfo payloadStorage;
        if (!resolveSemanticProductSumPayloadStorageInfo(
                payloadSumDef, payloadVariant, operationLabel, payloadStorage)) {
          if (error.empty()) {
            error = unsupportedSumPayloadError(payloadSumDef, payloadVariant);
          }
          return false;
        }
        LocalInfo payloadInfo;
        payloadInfo.kind = LocalInfo::Kind::Value;
        payloadInfo.valueKind = payloadStorage.isAggregate ? LocalInfo::ValueKind::Int64 : payloadStorage.valueKind;
        payloadInfo.structTypeName = payloadStorage.structPath;
        payloadInfo.structSlotCount = payloadStorage.slotCount;
        payloadInfo.index = nextLocal++;
        if (payloadStorage.isAggregate) {
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(sumPtrLocal)});
          function.instructions.push_back({IrOpcode::PushI64, static_cast<uint64_t>(2) * IrSlotBytes});
          function.instructions.push_back({IrOpcode::AddI64, 0});
        } else {
          emitLoadSumSlotIndirectForConstruction(sumPtrLocal, 2);
        }
        function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(payloadInfo.index)});
        if (payloadInfo.valueKind == LocalInfo::ValueKind::String && payloadInfo.structTypeName.empty()) {
          payloadInfo.stringSource = LocalInfo::StringSource::RuntimeIndex;
        }
        payloadLocals.emplace(payloadName, payloadInfo);
        return true;
      };
      auto emitCopySumPayload =
          [&](const Definition &sourceSumDef,
              const SumVariant &sourceVariant,
              const SumVariant &targetVariant,
              int32_t sourceSumPtrLocal,
              int32_t targetBaseLocal,
              const std::string &builtinName,
              const std::string &payloadDescription) -> bool {
        LoweredSumPayloadStorageInfo sourcePayload;
        LoweredSumPayloadStorageInfo targetPayload;
        const std::string sourceOperationLabel =
            builtinName + " source " + payloadDescription + " payload";
        const std::string targetOperationLabel =
            builtinName + " target " + payloadDescription + " payload";
        if (!resolveSemanticProductSumPayloadStorageInfo(
                sourceSumDef, sourceVariant, sourceOperationLabel, sourcePayload)) {
          if (error.empty()) {
            error = unsupportedSumPayloadError(sourceSumDef, sourceVariant);
          }
          return false;
        }
        if (!resolveSemanticProductSumPayloadStorageInfo(
                sumDef, targetVariant, targetOperationLabel, targetPayload)) {
          if (error.empty()) {
            error = unsupportedSumPayloadError(sumDef, targetVariant);
          }
          return false;
        }
        if (sourcePayload.isAggregate != targetPayload.isAggregate ||
            sourcePayload.valueKind != targetPayload.valueKind ||
            sourcePayload.structPath != targetPayload.structPath ||
            sourcePayload.slotCount != targetPayload.slotCount) {
          error = "native backend " + builtinName + " requires matching " +
                  payloadDescription + " payload storage";
          return false;
        }
        if (sourcePayload.isAggregate) {
          const int32_t srcPtrLocal = allocTempLocal();
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(sourceSumPtrLocal)});
          function.instructions.push_back({IrOpcode::PushI64, static_cast<uint64_t>(2) * IrSlotBytes});
          function.instructions.push_back({IrOpcode::AddI64, 0});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(srcPtrLocal)});
          const int32_t destPtrLocal = allocTempLocal();
          function.instructions.push_back({IrOpcode::AddressOfLocal, static_cast<uint64_t>(targetBaseLocal + 2)});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(destPtrLocal)});
          return emitStructCopyFromPtrs(destPtrLocal, srcPtrLocal, sourcePayload.slotCount);
        }
        emitLoadSumSlotIndirectForConstruction(sourceSumPtrLocal, 2);
        function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(targetBaseLocal + 2)});
        return true;
      };
      auto initializerResolvesToTargetSum = [&](const Expr &valueExpr) {
        if (isStdlibResultSumDefinition(sumDef) &&
            (isLegacyResultOkCall(valueExpr) ||
             isStdlibResultVariantHelperCall(valueExpr, "ok") ||
             isStdlibResultVariantHelperCall(valueExpr, "error"))) {
          return false;
        }
        auto constructorNameMatchesTargetSum = [&]() {
          if (valueExpr.kind != Expr::Kind::Call || valueExpr.isMethodCall ||
              valueExpr.isFieldAccess) {
            return false;
          }
          std::string targetPath = sumDef.fullPath;
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
          return valueExpr.name == targetName ||
                 valueExpr.name == targetPath ||
                 (!targetPath.empty() && targetPath.front() == '/' &&
                  valueExpr.name == targetPath.substr(1));
        };
        if (constructorNameMatchesTargetSum()) {
          return false;
        }
        if (valueExpr.kind == Expr::Kind::Call && !valueExpr.isMethodCall &&
            !valueExpr.isFieldAccess) {
          if (const Definition *constructorSum =
                  resolveSumDefinitionForTypeText(valueExpr.name,
                                                  valueExpr.namespacePrefix);
              constructorSum != nullptr &&
              constructorSum->fullPath == sumDef.fullPath) {
            return false;
          }
        }
        auto typeTextResolvesToTargetSum =
            [&](const std::string &typeText, auto typeTextId) {
          std::string resolvedTypeText;
          const auto &semanticTargets = callResolutionAdapters.semanticProductTargets;
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
          if (resolvedTypeText.empty()) {
            return false;
          }
          const Definition *candidateSum =
              resolveSumDefinitionForTypeText(resolvedTypeText,
                                             valueExpr.namespacePrefix);
          if (candidateSum == nullptr) {
            candidateSum = resolveSumDefinitionForTypeText(resolvedTypeText,
                                                          function.name);
          }
          return candidateSum != nullptr &&
                 candidateSum->fullPath == sumDef.fullPath;
        };
        if (valueExpr.kind == Expr::Kind::Name) {
          auto localIt = valueLocals.find(valueExpr.name);
          if (localIt != valueLocals.end() &&
              localIt->second.structTypeName == sumDef.fullPath) {
            return true;
          }
        }
        if (valueExpr.kind == Expr::Kind::Call && !valueExpr.isMethodCall &&
            !valueExpr.isFieldAccess) {
          if (const Definition *callee = resolveDefinitionCall(valueExpr);
              callee != nullptr) {
            for (const Transform &transform : callee->transforms) {
              if (transform.name == "return" &&
                  transform.templateArgs.size() == 1 &&
                  typeTextResolvesToTargetSum(transform.templateArgs.front(),
                                              InvalidSymbolId)) {
                return true;
              }
            }
          }
        }
        const auto &semanticTargets = callResolutionAdapters.semanticProductTargets;
        if (semanticTargets.hasSemanticProduct && valueExpr.semanticNodeId != 0) {
          if (const SemanticProgramBindingFact *bindingFact =
                  findSemanticProductBindingFact(semanticTargets, valueExpr);
              bindingFact != nullptr &&
              typeTextResolvesToTargetSum(bindingFact->bindingTypeText,
                                          bindingFact->bindingTypeTextId)) {
            return true;
          }
          if (const SemanticProgramQueryFact *queryFact =
                  findSemanticProductQueryFact(semanticTargets, valueExpr);
              queryFact != nullptr &&
              (typeTextResolvesToTargetSum(queryFact->bindingTypeText,
                                           queryFact->bindingTypeTextId) ||
               typeTextResolvesToTargetSum(queryFact->queryTypeText,
                                           queryFact->queryTypeTextId))) {
            return true;
          }
        }
        return false;
      };
      auto emitExistingSumValueIntoLocal = [&]() -> std::optional<bool> {
        if (!initializerResolvesToTargetSum(initializer)) {
          return std::nullopt;
        }
        auto initializerIsExistingSumLocal = [&]() {
          if (initializer.kind != Expr::Kind::Name) {
            return false;
          }
          auto localIt = valueLocals.find(initializer.name);
          return localIt != valueLocals.end() &&
                 localIt->second.structTypeName == sumDef.fullPath;
        };
        auto emitPackedResultValueIntoLocal = [&]() -> std::optional<bool> {
          if (!isStdlibResultSumDefinition(sumDef)) {
            return std::nullopt;
          }
          // A definition returning the stdlib Result sum hands back its sum storage.
          if (initializer.kind == Expr::Kind::Call && !initializer.isMethodCall) {
            if (const Definition *callee = resolveDefinitionCall(initializer);
                callee != nullptr && declaredStdlibResultSumReturn(*callee) != nullptr) {
              return std::nullopt;
            }
          }
          return emitPackedResultIntoSum(
              sumDef, baseLocal, [&]() { return emitExpr(initializer, valueLocals); });
        };
        if (!initializerIsExistingSumLocal()) {
          const std::optional<bool> packedResultEmitResult =
              emitPackedResultValueIntoLocal();
          if (packedResultEmitResult.has_value()) {
            return packedResultEmitResult;
          }
        }
        if (!emitExpr(initializer, valueLocals)) {
          return false;
        }
        const int32_t sourceSumPtrLocal = allocTempLocal();
        function.instructions.push_back(
            {IrOpcode::StoreLocal, static_cast<uint64_t>(sourceSumPtrLocal)});
        emitLoadSumSlotIndirectForConstruction(sourceSumPtrLocal, 1);
        function.instructions.push_back(
            {IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal + 1)});
        std::vector<size_t> endJumps;
        for (const auto &variant : sumDef.sumVariants) {
          int32_t tagValue = 0;
          if (!resolveSemanticProductSumVariantTag(
                  sumDef, variant, "sum value copy", tagValue)) {
            return false;
          }
          emitSumTagComparisonForConstruction(sourceSumPtrLocal, tagValue);
          const size_t nextVariantJump = function.instructions.size();
          function.instructions.push_back({IrOpcode::JumpIfZero, 0});
          if (variant.hasPayload &&
              !emitCopySumPayload(sumDef,
                                  variant,
                                  variant,
                                  sourceSumPtrLocal,
                                  baseLocal,
                                  "sum value copy",
                                  variant.name)) {
            return false;
          }
          endJumps.push_back(function.instructions.size());
          function.instructions.push_back({IrOpcode::Jump, 0});
          function.instructions[nextVariantJump].imm =
              static_cast<uint64_t>(function.instructions.size());
        }
        for (size_t jumpIndex : endJumps) {
          function.instructions[jumpIndex].imm =
              static_cast<uint64_t>(function.instructions.size());
        }
        return true;
      };
      auto emitSelectedSumPayloadIntoLocal =
          [&](const LoweredSumVariantSelection &selection,
              const LocalMap &selectedLocals,
              int32_t targetBaseLocal) -> bool {
        if (selection.variant == nullptr) {
          error = "native backend sum variant was not selected for " + sumDef.fullPath;
          return false;
        }
        int32_t activeTag = 0;
        if (!resolveSemanticProductSumVariantTag(
                sumDef, *selection.variant, "sum construction", activeTag)) {
          return false;
        }
        function.instructions.push_back(
            {IrOpcode::PushI32, static_cast<uint64_t>(activeTag)});
        function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(targetBaseLocal + 1)});
        if (!selection.variant->hasPayload) {
          return true;
        }
        if (selection.payloadExpr == nullptr) {
          error = "native backend sum payload was not selected for " +
                  sumDef.fullPath + "/" + selection.variant->name;
          return false;
        }
        if (selection.payloadIsAggregate) {
          if (!emitExpr(*selection.payloadExpr, selectedLocals)) {
            return false;
          }
          const int32_t srcPtrLocal = allocTempLocal();
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(srcPtrLocal)});
          const int32_t destPtrLocal = allocTempLocal();
          function.instructions.push_back({IrOpcode::AddressOfLocal, static_cast<uint64_t>(targetBaseLocal + 2)});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(destPtrLocal)});
          if (!emitStructCopyFromPtrs(destPtrLocal, srcPtrLocal, selection.payloadSlotCount)) {
            return false;
          }
          if (shouldDisarmStructCopySourceExpr(*selection.payloadExpr)) {
            ir_lowerer::emitDisarmTemporaryStructAfterCopy(
                [&](IrOpcode op, uint64_t imm) { function.instructions.push_back({op, imm}); },
                srcPtrLocal,
                selection.payloadStructPath);
          } else if (!isStdlibResultSumDefinition(sumDef)) {
            // A payload taken from a place is copied like a struct field initialized from one, so
            // the sum owns its payload (docs/spec/value-lifecycle.md).
            bool ranCopyHelper = false;
            if (!ir_lowerer::emitStructCopyHelpersFromPtrs(
                    destPtrLocal,
                    srcPtrLocal,
                    selection.payloadStructPath,
                    [&](const std::string &path) -> const Definition * {
                      auto copyIt = defMap.find(path + "/Copy");
                      return copyIt == defMap.end() ? nullptr : copyIt->second;
                    },
                    resolveStructSlotLayout,
                    allocTempLocal,
                    [&](IrOpcode op, uint64_t imm) { function.instructions.push_back({op, imm}); },
                    selectedLocals,
                    emitInlineDefinitionCall,
                    ranCopyHelper,
                    error)) {
              return false;
            }
          }
          return true;
        }
        if (!emitExpr(*selection.payloadExpr, selectedLocals)) {
          return false;
        }
        function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(targetBaseLocal + 2)});
        return true;
      };
      auto emitMappedValueIntoSum =
          [&](const Expr &mappedExpr,
              const LocalMap &mappedLocals,
              const SumVariant &targetVariant,
              std::string_view operationLabel,
              int32_t targetBaseLocal) -> bool {
        LoweredSumPayloadStorageInfo targetPayload;
        if (!resolveSemanticProductSumPayloadStorageInfo(
                sumDef, targetVariant, operationLabel, targetPayload)) {
          if (error.empty()) {
            error = unsupportedSumPayloadError(sumDef, targetVariant);
          }
          return false;
        }
        if (!targetPayload.isAggregate) {
          if (!emitExpr(mappedExpr, mappedLocals)) {
            return false;
          }
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(targetBaseLocal + 2)});
          return true;
        }
        if (!emitExpr(mappedExpr, mappedLocals)) {
          return false;
        }
        const int32_t srcPtrLocal = allocTempLocal();
        function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(srcPtrLocal)});
        const int32_t destPtrLocal = allocTempLocal();
        function.instructions.push_back({IrOpcode::AddressOfLocal, static_cast<uint64_t>(targetBaseLocal + 2)});
        function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(destPtrLocal)});
        if (!emitStructCopyFromPtrs(destPtrLocal, srcPtrLocal, targetPayload.slotCount)) {
          return false;
        }
        if (shouldDisarmStructCopySourceExpr(mappedExpr)) {
          ir_lowerer::emitDisarmTemporaryStructAfterCopy(
              [&](IrOpcode op, uint64_t imm) { function.instructions.push_back({op, imm}); },
              srcPtrLocal,
              targetPayload.structPath);
        }
        return true;
      };
      auto emitAndThenResultIntoSum =
          [&](const Expr &resultExpr,
              const LocalMap &resultLocals,
              int32_t targetBaseLocal) -> bool {
        if (resultExpr.kind == Expr::Kind::Name) {
          auto localIt = resultLocals.find(resultExpr.name);
          if (localIt != resultLocals.end() &&
              localIt->second.structTypeName == sumDef.fullPath) {
            if (!emitExpr(resultExpr, resultLocals)) {
              return false;
            }
            const int32_t sourceSumPtrLocal = allocTempLocal();
            function.instructions.push_back(
                {IrOpcode::StoreLocal, static_cast<uint64_t>(sourceSumPtrLocal)});
            emitLoadSumSlotIndirectForConstruction(sourceSumPtrLocal, 1);
            function.instructions.push_back(
                {IrOpcode::StoreLocal, static_cast<uint64_t>(targetBaseLocal + 1)});
            std::vector<size_t> endJumps;
            for (const auto &variant : sumDef.sumVariants) {
              int32_t tagValue = 0;
              if (!resolveSemanticProductSumVariantTag(
                      sumDef, variant, "Result.and_then local result copy", tagValue)) {
                return false;
              }
              emitSumTagComparisonForConstruction(sourceSumPtrLocal, tagValue);
              const size_t nextVariantJump = function.instructions.size();
              function.instructions.push_back({IrOpcode::JumpIfZero, 0});
              if (variant.hasPayload &&
                  !emitCopySumPayload(sumDef,
                                      variant,
                                      variant,
                                      sourceSumPtrLocal,
                                      targetBaseLocal,
                                      "Result.and_then",
                                      variant.name)) {
                return false;
              }
              endJumps.push_back(function.instructions.size());
              function.instructions.push_back({IrOpcode::Jump, 0});
              function.instructions[nextVariantJump].imm =
                  static_cast<uint64_t>(function.instructions.size());
            }
            for (size_t jumpIndex : endJumps) {
              function.instructions[jumpIndex].imm =
                  static_cast<uint64_t>(function.instructions.size());
            }
            return true;
          }
        }
        LoweredSumVariantSelection selection;
        if (!selectSumVariantForInitializer(resultExpr, sumDef, resultLocals, selection)) {
          if (error.empty()) {
            error = "IR backends require Result.and_then lambdas to produce stdlib Result sums";
          }
          return false;
        }
        if (selection.variant == nullptr) {
          error = "IR backends require Result.and_then lambdas to produce stdlib Result sums";
          return false;
        }
        return emitSelectedSumPayloadIntoLocal(selection, resultLocals, targetBaseLocal);
      };
      struct StdlibResultSumSource {
        const Definition *sumDef = nullptr;
        int32_t sumPtrLocal = -1;
      };
      auto resolveSemanticProductResultSumSourceDefinition =
          [&](const Expr &sourceExpr,
              const std::string &builtinName,
              const std::string &sourceLabel,
              const Definition *&sourceSumDefOut) -> std::optional<bool> {
        sourceSumDefOut = nullptr;
        const auto &semanticTargets = callResolutionAdapters.semanticProductTargets;
        if (!semanticTargets.hasSemanticProduct || sourceExpr.semanticNodeId == 0) {
          return std::nullopt;
        }
        const SemanticProgramQueryFact *queryFact =
            findSemanticProductQueryFact(semanticTargets, sourceExpr);
        if (queryFact == nullptr) {
          return std::nullopt;
        }
        auto resolveQueryTypeText = [&](const std::string &typeText,
                                        SymbolId typeTextId) -> const Definition * {
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
          const std::string normalizedTypeText = trimTemplateTypeText(resolvedTypeText);
          if (normalizedTypeText.empty()) {
            return nullptr;
          }
          const Definition *candidate =
              resolveSumDefinitionForTypeText(normalizedTypeText, sourceExpr.namespacePrefix);
          return candidate != nullptr && isStdlibResultSumDefinition(*candidate)
                     ? candidate
                     : nullptr;
        };
        if (const Definition *candidate =
                resolveQueryTypeText(queryFact->bindingTypeText,
                                     queryFact->bindingTypeTextId);
            candidate != nullptr) {
          sourceSumDefOut = candidate;
          return true;
        }
        if (const Definition *candidate =
                resolveQueryTypeText(queryFact->queryTypeText,
                                     queryFact->queryTypeTextId);
            candidate != nullptr) {
          sourceSumDefOut = candidate;
          return true;
        }
        error = "stale semantic-product Result-combinator source query metadata for " +
                builtinName + " " + sourceLabel;
        return false;
      };
      auto materializeStdlibResultSumSource =
          [&](const Expr &sourceExpr,
              const LocalMap &sourceLocals,
              const std::string &builtinName,
              const std::string &sourceDescription,
              StdlibResultSumSource &sourceOut) -> bool {
        sourceOut = {};
        const std::string sourceLabel =
            sourceDescription.empty() ? "source" : sourceDescription + " source";
        const Definition *sourceSumDef = nullptr;
        if (sourceExpr.kind == Expr::Kind::Name) {
          auto sourceIt = sourceLocals.find(sourceExpr.name);
          if (sourceIt == sourceLocals.end()) {
            error = "native backend " + builtinName + " " +
                    sourceLabel + " is unknown: " + sourceExpr.name;
            return false;
          }
          sourceSumDef = resolveSumDefinitionForLocalInfo(sourceIt->second);
          if (sourceSumDef == nullptr || !isStdlibResultSumDefinition(*sourceSumDef)) {
            error = "native backend " + builtinName + " " +
                    sourceLabel + " requires stdlib Result sum";
            return false;
          }
          sourceOut.sumDef = sourceSumDef;
          sourceOut.sumPtrLocal = allocTempLocal();
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(sourceIt->second.index)});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(sourceOut.sumPtrLocal)});
          return true;
        }
        if (sourceExpr.kind != Expr::Kind::Call) {
          error = "native backend " + builtinName + " " +
                  sourceLabel + " requires local or direct stdlib Result sum";
          return false;
        }
        const std::optional<bool> resolvedBySemanticProductQuery =
            resolveSemanticProductResultSumSourceDefinition(
                sourceExpr, builtinName, sourceLabel, sourceSumDef);
        if (resolvedBySemanticProductQuery.has_value() && !*resolvedBySemanticProductQuery) {
          return false;
        }
        if (!resolvedBySemanticProductQuery.has_value()) {
          const std::string sourceStructPath = inferStructExprPath(sourceExpr, sourceLocals);
          sourceSumDef = resolveSumDefinitionForTypeText(sourceStructPath, sourceExpr.namespacePrefix);
        }
        if (sourceSumDef == nullptr || !isStdlibResultSumDefinition(*sourceSumDef)) {
          error = "native backend " + builtinName + " " +
                  sourceLabel + " requires local or direct stdlib Result sum";
          return false;
        }
        if (isLegacyResultOkCall(sourceExpr) ||
            isStdlibResultVariantHelperCall(sourceExpr, "ok") ||
            isStdlibResultVariantHelperCall(sourceExpr, "error")) {
          int32_t totalSlots = 0;
          if (!loweredSumSlotCount(*sourceSumDef, totalSlots)) {
            return false;
          }
          const int32_t baseLocal = nextLocal;
          nextLocal += totalSlots;
          emitLoweredSumHeader(baseLocal, totalSlots);
          LoweredSumVariantSelection selection;
          if (!selectSumVariantForInitializer(
                  sourceExpr, *sourceSumDef, sourceLocals, selection) ||
              selection.variant == nullptr) {
            if (error.empty()) {
              error = "native backend could not select Result source variant";
            }
            return false;
          }
          if (!emitSelectedSumPayloadIntoLocal(selection, sourceLocals, baseLocal)) {
            return false;
          }
          sourceOut.sumDef = sourceSumDef;
          sourceOut.sumPtrLocal = allocTempLocal();
          function.instructions.push_back(
              {IrOpcode::AddressOfLocal, static_cast<uint64_t>(baseLocal)});
          function.instructions.push_back(
              {IrOpcode::StoreLocal, static_cast<uint64_t>(sourceOut.sumPtrLocal)});
          return true;
        }
        auto tryMaterializePackedResultCallSource = [&]() -> std::optional<bool> {
          if (!sourceExpr.isMethodCall) {
            if (const Definition *callee = resolveDefinitionCall(sourceExpr);
                callee != nullptr && declaredStdlibResultSumReturn(*callee) != nullptr) {
              return std::nullopt;
            }
          }
          int32_t totalSlots = 0;
          if (!loweredSumSlotCount(*sourceSumDef, totalSlots)) {
            return false;
          }
          const int32_t baseLocal = nextLocal;
          const std::optional<bool> decoded =
              emitPackedResultIntoSum(*sourceSumDef, baseLocal, [&]() {
                nextLocal += totalSlots;
                emitLoweredSumHeader(baseLocal, totalSlots);
                return emitExpr(sourceExpr, sourceLocals);
              });
          if (!decoded.has_value() || !*decoded) {
            return decoded;
          }
          sourceOut.sumPtrLocal = allocTempLocal();
          function.instructions.push_back(
              {IrOpcode::AddressOfLocal, static_cast<uint64_t>(baseLocal)});
          function.instructions.push_back(
              {IrOpcode::StoreLocal, static_cast<uint64_t>(sourceOut.sumPtrLocal)});
          sourceOut.sumDef = sourceSumDef;
          return true;
        };
        if (const auto packedSourceResult = tryMaterializePackedResultCallSource();
            packedSourceResult.has_value()) {
          return *packedSourceResult;
        }
        if (!emitExpr(sourceExpr, sourceLocals)) {
          return false;
        }
        sourceOut.sumDef = sourceSumDef;
        sourceOut.sumPtrLocal = allocTempLocal();
        function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(sourceOut.sumPtrLocal)});
        return true;
      };
      auto tryEmitLoweredResultMapIntoLocal = [&]() -> std::optional<bool> {
        if (!isStdlibResultSumDefinition(sumDef) || !isLegacyResultMapCall(initializer)) {
          return std::nullopt;
        }
        const Expr &sourceExpr = initializer.args[1];
        StdlibResultSumSource source;
        if (!materializeStdlibResultSumSource(sourceExpr, valueLocals, "Result.map", "", source)) {
          return false;
        }
        const SumVariant *sourceOkVariant = findSumVariantByName(*source.sumDef, "ok");
        const SumVariant *sourceErrorVariant = findSumVariantByName(*source.sumDef, "error");
        const SumVariant *targetOkVariant = findSumVariantByName(sumDef, "ok");
        const SumVariant *targetErrorVariant = findSumVariantByName(sumDef, "error");
        if (sourceOkVariant == nullptr || sourceErrorVariant == nullptr ||
            targetOkVariant == nullptr || targetErrorVariant == nullptr ||
            !sourceOkVariant->hasPayload || !sourceErrorVariant->hasPayload ||
            !targetOkVariant->hasPayload || !targetErrorVariant->hasPayload) {
          error = "native backend Result.map requires value-carrying stdlib Result sums";
          return false;
        }
        const Expr &lambdaExpr = initializer.args[2];
        if (!lambdaExpr.isLambda) {
          error = "Result.map requires a lambda argument";
          return false;
        }
        const std::string lambdaParameterName =
            lambdaExpr.args.size() == 1 ? resultLambdaParameterName(lambdaExpr.args.front()) : std::string{};
        if (lambdaParameterName.empty()) {
          error = "Result.map requires a single-parameter lambda";
          return false;
        }
        const Expr *mappedValueExpr = findResultLambdaValueExpr(lambdaExpr);
        if (mappedValueExpr == nullptr) {
          error = "IR backends require Result.map lambda bodies";
          return false;
        }

        int32_t sourceOkTag = 0;
        int32_t targetOkTag = 0;
        int32_t targetErrorTag = 0;
        if (!resolveSemanticProductSumVariantTag(
                *source.sumDef, *sourceOkVariant, "Result.map source ok", sourceOkTag) ||
            !resolveSemanticProductSumVariantTag(
                sumDef, *targetOkVariant, "Result.map target ok", targetOkTag) ||
            !resolveSemanticProductSumVariantTag(
                sumDef, *targetErrorVariant, "Result.map target error", targetErrorTag)) {
          return false;
        }

        emitSumTagComparisonForConstruction(source.sumPtrLocal, sourceOkTag);
        const size_t jumpErrorIndex = function.instructions.size();
        function.instructions.push_back({IrOpcode::JumpIfZero, 0});

        function.instructions.push_back({IrOpcode::PushI32, static_cast<uint64_t>(targetOkTag)});
        function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal + 1)});
        LocalMap lambdaLocals = valueLocals;
        if (!bindSumPayloadLocal(*source.sumDef,
                                 *sourceOkVariant,
                                 source.sumPtrLocal,
                                 lambdaParameterName,
                                 "Result.map source ok payload",
                                 lambdaLocals)) {
          return false;
        }
        if (!emitResultLambdaPrefixStatements(lambdaExpr, mappedValueExpr, lambdaLocals)) {
          return false;
        }
        if (!emitMappedValueIntoSum(
                *mappedValueExpr, lambdaLocals, *targetOkVariant, "Result.map target ok payload", baseLocal)) {
          return false;
        }
        const size_t jumpEndIndex = function.instructions.size();
        function.instructions.push_back({IrOpcode::Jump, 0});

        const size_t errorIndex = function.instructions.size();
        function.instructions[jumpErrorIndex].imm = static_cast<uint64_t>(errorIndex);
        function.instructions.push_back({IrOpcode::PushI32, static_cast<uint64_t>(targetErrorTag)});
        function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal + 1)});
        if (!emitCopySumPayload(*source.sumDef,
                                *sourceErrorVariant,
                                *targetErrorVariant,
                                source.sumPtrLocal,
                                baseLocal,
                                "Result.map",
                                "error")) {
          return false;
        }

        const size_t endIndex = function.instructions.size();
        function.instructions[jumpEndIndex].imm = static_cast<uint64_t>(endIndex);
        return true;
      };
      if (const auto resultMapEmitResult = tryEmitLoweredResultMapIntoLocal();
          resultMapEmitResult.has_value()) {
        return *resultMapEmitResult;
      }
      auto tryEmitLoweredResultAndThenIntoLocal = [&]() -> std::optional<bool> {
        if (!isStdlibResultSumDefinition(sumDef) || !isLegacyResultAndThenCall(initializer)) {
          return std::nullopt;
        }
        const Expr &sourceExpr = initializer.args[1];
        StdlibResultSumSource source;
        if (!materializeStdlibResultSumSource(sourceExpr, valueLocals, "Result.and_then", "", source)) {
          return false;
        }
        const SumVariant *sourceOkVariant = findSumVariantByName(*source.sumDef, "ok");
        const SumVariant *sourceErrorVariant = findSumVariantByName(*source.sumDef, "error");
        const SumVariant *targetErrorVariant = findSumVariantByName(sumDef, "error");
        if (sourceOkVariant == nullptr || sourceErrorVariant == nullptr ||
            targetErrorVariant == nullptr || !sourceOkVariant->hasPayload ||
            !sourceErrorVariant->hasPayload || !targetErrorVariant->hasPayload) {
          error = "native backend Result.and_then requires value-carrying stdlib Result sums";
          return false;
        }
        const Expr &lambdaExpr = initializer.args[2];
        if (!lambdaExpr.isLambda) {
          error = "Result.and_then requires a lambda argument";
          return false;
        }
        const std::string lambdaParameterName =
            lambdaExpr.args.size() == 1 ? resultLambdaParameterName(lambdaExpr.args.front()) : std::string{};
        if (lambdaParameterName.empty()) {
          error = "Result.and_then requires a single-parameter lambda";
          return false;
        }
        const Expr *chainedResultExpr = findResultLambdaValueExpr(lambdaExpr);
        if (chainedResultExpr == nullptr) {
          error = "IR backends require Result.and_then lambda bodies";
          return false;
        }

        int32_t sourceOkTag = 0;
        int32_t targetErrorTag = 0;
        if (!resolveSemanticProductSumVariantTag(
                *source.sumDef, *sourceOkVariant, "Result.and_then source ok", sourceOkTag) ||
            !resolveSemanticProductSumVariantTag(
                sumDef, *targetErrorVariant, "Result.and_then target error", targetErrorTag)) {
          return false;
        }

        emitSumTagComparisonForConstruction(source.sumPtrLocal, sourceOkTag);
        const size_t jumpErrorIndex = function.instructions.size();
        function.instructions.push_back({IrOpcode::JumpIfZero, 0});

        LocalMap lambdaLocals = valueLocals;
        if (!bindSumPayloadLocal(*source.sumDef,
                                 *sourceOkVariant,
                                 source.sumPtrLocal,
                                 lambdaParameterName,
                                 "Result.and_then source ok payload",
                                 lambdaLocals)) {
          return false;
        }
        if (!emitResultLambdaPrefixStatements(lambdaExpr, chainedResultExpr, lambdaLocals)) {
          return false;
        }
        if (!emitAndThenResultIntoSum(*chainedResultExpr, lambdaLocals, baseLocal)) {
          return false;
        }
        const size_t jumpEndIndex = function.instructions.size();
        function.instructions.push_back({IrOpcode::Jump, 0});

        const size_t errorIndex = function.instructions.size();
        function.instructions[jumpErrorIndex].imm = static_cast<uint64_t>(errorIndex);
        function.instructions.push_back({IrOpcode::PushI32, static_cast<uint64_t>(targetErrorTag)});
        function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal + 1)});
        if (!emitCopySumPayload(*source.sumDef,
                                *sourceErrorVariant,
                                *targetErrorVariant,
                                source.sumPtrLocal,
                                baseLocal,
                                "Result.and_then",
                                "error")) {
          return false;
        }

        const size_t endIndex = function.instructions.size();
        function.instructions[jumpEndIndex].imm = static_cast<uint64_t>(endIndex);
        return true;
      };
      if (const auto resultAndThenEmitResult = tryEmitLoweredResultAndThenIntoLocal();
          resultAndThenEmitResult.has_value()) {
        return *resultAndThenEmitResult;
      }
      auto tryEmitLoweredResultMap2IntoLocal = [&]() -> std::optional<bool> {
        if (!isStdlibResultSumDefinition(sumDef) || !isLegacyResultMap2Call(initializer)) {
          return std::nullopt;
        }
        const Expr &leftExpr = initializer.args[1];
        const Expr &rightExpr = initializer.args[2];
        StdlibResultSumSource left;
        StdlibResultSumSource right;
        if (!materializeStdlibResultSumSource(leftExpr, valueLocals, "Result.map2", "left", left) ||
            !materializeStdlibResultSumSource(rightExpr, valueLocals, "Result.map2", "right", right)) {
          return false;
        }
        const SumVariant *leftOkVariant = findSumVariantByName(*left.sumDef, "ok");
        const SumVariant *leftErrorVariant = findSumVariantByName(*left.sumDef, "error");
        const SumVariant *rightOkVariant = findSumVariantByName(*right.sumDef, "ok");
        const SumVariant *rightErrorVariant = findSumVariantByName(*right.sumDef, "error");
        const SumVariant *targetOkVariant = findSumVariantByName(sumDef, "ok");
        const SumVariant *targetErrorVariant = findSumVariantByName(sumDef, "error");
        if (leftOkVariant == nullptr || leftErrorVariant == nullptr ||
            rightOkVariant == nullptr || rightErrorVariant == nullptr ||
            targetOkVariant == nullptr || targetErrorVariant == nullptr ||
            !leftOkVariant->hasPayload || !leftErrorVariant->hasPayload ||
            !rightOkVariant->hasPayload || !rightErrorVariant->hasPayload ||
            !targetOkVariant->hasPayload || !targetErrorVariant->hasPayload) {
          error = "native backend Result.map2 requires value-carrying stdlib Result sums";
          return false;
        }
        const Expr &lambdaExpr = initializer.args[3];
        if (!lambdaExpr.isLambda) {
          error = "Result.map2 requires a lambda argument";
          return false;
        }
        const std::string leftParameterName =
            lambdaExpr.args.size() == 2 ? resultLambdaParameterName(lambdaExpr.args[0]) : std::string{};
        const std::string rightParameterName =
            lambdaExpr.args.size() == 2 ? resultLambdaParameterName(lambdaExpr.args[1]) : std::string{};
        if (leftParameterName.empty() || rightParameterName.empty()) {
          error = "Result.map2 requires a two-parameter lambda";
          return false;
        }
        const Expr *mappedValueExpr = findResultLambdaValueExpr(lambdaExpr);
        if (mappedValueExpr == nullptr) {
          error = "IR backends require Result.map2 lambda bodies";
          return false;
        }

        int32_t leftOkTag = 0;
        int32_t rightOkTag = 0;
        int32_t targetOkTag = 0;
        int32_t targetErrorTag = 0;
        if (!resolveSemanticProductSumVariantTag(
                *left.sumDef, *leftOkVariant, "Result.map2 left ok", leftOkTag) ||
            !resolveSemanticProductSumVariantTag(
                *right.sumDef, *rightOkVariant, "Result.map2 right ok", rightOkTag) ||
            !resolveSemanticProductSumVariantTag(
                sumDef, *targetOkVariant, "Result.map2 target ok", targetOkTag) ||
            !resolveSemanticProductSumVariantTag(
                sumDef, *targetErrorVariant, "Result.map2 target error", targetErrorTag)) {
          return false;
        }

        emitSumTagComparisonForConstruction(left.sumPtrLocal, leftOkTag);
        const size_t jumpLeftErrorIndex = function.instructions.size();
        function.instructions.push_back({IrOpcode::JumpIfZero, 0});

        emitSumTagComparisonForConstruction(right.sumPtrLocal, rightOkTag);
        const size_t jumpRightErrorIndex = function.instructions.size();
        function.instructions.push_back({IrOpcode::JumpIfZero, 0});

        function.instructions.push_back({IrOpcode::PushI32, static_cast<uint64_t>(targetOkTag)});
        function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal + 1)});
        LocalMap lambdaLocals = valueLocals;
        if (!bindSumPayloadLocal(*left.sumDef,
                                 *leftOkVariant,
                                 left.sumPtrLocal,
                                 leftParameterName,
                                 "Result.map2 left ok payload",
                                 lambdaLocals)) {
          return false;
        }
        if (!bindSumPayloadLocal(*right.sumDef,
                                 *rightOkVariant,
                                 right.sumPtrLocal,
                                 rightParameterName,
                                 "Result.map2 right ok payload",
                                 lambdaLocals)) {
          return false;
        }
        if (!emitResultLambdaPrefixStatements(lambdaExpr, mappedValueExpr, lambdaLocals)) {
          return false;
        }
        if (!emitMappedValueIntoSum(
                *mappedValueExpr, lambdaLocals, *targetOkVariant, "Result.map2 target ok payload", baseLocal)) {
          return false;
        }
        const size_t jumpEndIndex = function.instructions.size();
        function.instructions.push_back({IrOpcode::Jump, 0});

        const size_t leftErrorIndex = function.instructions.size();
        function.instructions[jumpLeftErrorIndex].imm = static_cast<uint64_t>(leftErrorIndex);
        function.instructions.push_back({IrOpcode::PushI32, static_cast<uint64_t>(targetErrorTag)});
        function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal + 1)});
        if (!emitCopySumPayload(*left.sumDef,
                                *leftErrorVariant,
                                *targetErrorVariant,
                                left.sumPtrLocal,
                                baseLocal,
                                "Result.map2",
                                "left error")) {
          return false;
        }
        const size_t jumpAfterLeftErrorIndex = function.instructions.size();
        function.instructions.push_back({IrOpcode::Jump, 0});

        const size_t rightErrorIndex = function.instructions.size();
        function.instructions[jumpRightErrorIndex].imm = static_cast<uint64_t>(rightErrorIndex);
        function.instructions.push_back({IrOpcode::PushI32, static_cast<uint64_t>(targetErrorTag)});
        function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal + 1)});
        if (!emitCopySumPayload(*right.sumDef,
                                *rightErrorVariant,
                                *targetErrorVariant,
                                right.sumPtrLocal,
                                baseLocal,
                                "Result.map2",
                                "right error")) {
          return false;
        }

        const size_t endIndex = function.instructions.size();
        function.instructions[jumpEndIndex].imm = static_cast<uint64_t>(endIndex);
        function.instructions[jumpAfterLeftErrorIndex].imm = static_cast<uint64_t>(endIndex);
        return true;
      };
      if (const auto resultMap2EmitResult = tryEmitLoweredResultMap2IntoLocal();
          resultMap2EmitResult.has_value()) {
        return *resultMap2EmitResult;
      }
      if (const auto existingSumEmitResult = emitExistingSumValueIntoLocal();
          existingSumEmitResult.has_value()) {
        return *existingSumEmitResult;
      }
      LoweredSumVariantSelection selection;
      if (!selectSumVariantForInitializer(initializer, sumDef, valueLocals, selection)) {
        if (error.empty()) {
          error = "native backend could not select sum variant for " + sumDef.fullPath;
        }
        return false;
      }
      if (selection.variant == nullptr) {
        error = "native backend could not select sum variant for " + sumDef.fullPath;
        return false;
      }
      return emitSelectedSumPayloadIntoLocal(selection, valueLocals, baseLocal);
    }


} // namespace primec::ir_lowerer
