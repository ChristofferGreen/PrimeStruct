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

    bool SumHelpersContext::tryEmitLoweredSumConstructorExpr(const Expr &expr, const LocalMap &valueLocals) {
      const Definition *sumDef = resolveSumDefinitionForTypeText(expr.name, expr.namespacePrefix);
      if (sumDef == nullptr && callResolutionAdapters.semanticProductTargets.hasSemanticProduct &&
          expr.semanticNodeId != 0) {
        auto resolveSemanticTypeText = [&](const std::string &typeText,
                                           SymbolId typeTextId) -> const Definition * {
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
          return resolveSumDefinitionForTypeText(trimTemplateTypeText(resolvedTypeText),
                                                 expr.namespacePrefix);
        };
        const auto &semanticTargets = callResolutionAdapters.semanticProductTargets;
        if (const SemanticProgramBindingFact *bindingFact =
                findSemanticProductBindingFact(semanticTargets, expr);
            bindingFact != nullptr) {
          sumDef = resolveSemanticTypeText(bindingFact->bindingTypeText,
                                           bindingFact->bindingTypeTextId);
        }
        if (sumDef == nullptr) {
          if (const SemanticProgramQueryFact *queryFact =
                  findSemanticProductQueryFact(semanticTargets, expr);
              queryFact != nullptr) {
            sumDef = resolveSemanticTypeText(queryFact->bindingTypeText,
                                             queryFact->bindingTypeTextId);
            if (sumDef == nullptr) {
              sumDef = resolveSemanticTypeText(queryFact->queryTypeText,
                                               queryFact->queryTypeTextId);
            }
          }
        }
      }
      if (sumDef == nullptr) {
        return false;
      }
      int32_t totalSlots = 0;
      if (!loweredSumSlotCount(*sumDef, totalSlots)) {
        if (!error.empty()) {
          return false;
        }
        LoweredSumVariantSelection selection;
        const bool selectedForDiagnostic =
            selectExplicitSumVariantForConstructor(expr, *sumDef, selection);
        if (!selectedForDiagnostic && !error.empty()) {
          return false;
        }
        if (selectedForDiagnostic && selection.variant != nullptr) {
          error = unsupportedSumPayloadError(*sumDef, *selection.variant);
        } else if (const SumVariant *unsupportedVariant = firstUnsupportedSumPayloadVariant(*sumDef);
                   unsupportedVariant != nullptr) {
          error = unsupportedSumPayloadError(*sumDef, *unsupportedVariant);
        } else {
          error = "native backend does not support sum payload type on " + sumDef->fullPath;
        }
        return false;
      }
      const int32_t baseLocal = nextLocal;
      nextLocal += totalSlots;
      emitLoweredSumHeader(baseLocal, totalSlots);
      if (!emitLoweredSumConstructionIntoLocal(baseLocal, *sumDef, expr, valueLocals)) {
        return false;
      }
      function.instructions.push_back({IrOpcode::AddressOfLocal, static_cast<uint64_t>(baseLocal)});
      return true;
    }

    std::string SumHelpersContext::describePickTargetName(const Expr &targetExpr) {
      if (targetExpr.kind == Expr::Kind::Name && !targetExpr.name.empty()) {
        return function.name + " -> " + targetExpr.name;
      }
      if (!targetExpr.name.empty()) {
        return function.name + " -> " + targetExpr.name;
      }
      return function.name + " -> <expr>";
    }

    const Definition * SumHelpersContext::resolveSemanticProductPickTargetSumDefinition(const Expr &targetExpr,
            const LocalMap &valueLocals) {
      const auto &semanticTargets = callResolutionAdapters.semanticProductTargets;
      if (!semanticTargets.hasSemanticProduct || semanticTargets.semanticProgram == nullptr) {
        return nullptr;
      }

      auto requirePublishedSumMetadata =
          [&](const Definition &sumDef, const Expr &sourceExpr) -> const Definition * {
        if (findSemanticProductSumTypeMetadata(semanticTargets, sumDef.fullPath) == nullptr) {
          error = "missing semantic-product sum metadata for pick target: " +
                  describePickTargetName(sourceExpr);
          return nullptr;
        }
        return &sumDef;
      };

      auto resolveSemanticProductTypeText =
          [&](const std::string &typeText, auto typeTextId) {
        std::string resolvedTypeText;
        if (typeTextId != InvalidSymbolId) {
          resolvedTypeText = std::string(semanticProgramResolveCallTargetString(
              *semanticTargets.semanticProgram,
              typeTextId));
        }
        if (resolvedTypeText.empty()) {
          resolvedTypeText = typeText;
        }
        return trimTemplateTypeText(resolvedTypeText);
      };

      if (targetExpr.kind == Expr::Kind::Name) {
        const SemanticProgramBindingFact *bindingFact =
            findSemanticProductBindingFact(semanticTargets, targetExpr);
        const std::string bindingTypeText =
            bindingFact != nullptr
                ? resolveSemanticProductTypeText(bindingFact->bindingTypeText,
                                                 bindingFact->bindingTypeTextId)
                : std::string{};
        if (bindingTypeText.empty()) {
          if (valueLocals.find(targetExpr.name) != valueLocals.end()) {
            error = "missing semantic-product pick target binding fact: " +
                    describePickTargetName(targetExpr);
          }
          return nullptr;
        }
        const Definition *semanticSumDef =
            resolveSumDefinitionForTypeText(bindingTypeText, function.name);
        if (semanticSumDef == nullptr) {
          error = "semantic-product pick target binding type is not a sum: " +
                  describePickTargetName(targetExpr);
          return nullptr;
        }
        auto localIt = valueLocals.find(targetExpr.name);
        if (localIt != valueLocals.end()) {
          const Definition *localSumDef = resolveSumDefinitionForLocalInfo(localIt->second);
          if (localSumDef != nullptr && localSumDef->fullPath != semanticSumDef->fullPath) {
            error = "stale semantic-product pick target binding type: " +
                    describePickTargetName(targetExpr);
            return nullptr;
          }
        }
        return requirePublishedSumMetadata(*semanticSumDef, targetExpr);
      }

      if (targetExpr.kind == Expr::Kind::Call) {
        if (!targetExpr.isMethodCall) {
          if (const Definition *constructorSum =
                  resolveSumDefinitionForTypeText(targetExpr.name, targetExpr.namespacePrefix);
              constructorSum != nullptr) {
            return requirePublishedSumMetadata(*constructorSum, targetExpr);
          }
        }
        if (targetExpr.semanticNodeId != 0) {
          const SemanticProgramQueryFact *queryFact =
              findSemanticProductQueryFact(semanticTargets, targetExpr);
          if (queryFact == nullptr) {
            error = "missing semantic-product pick target query fact: " +
                    describePickTargetName(targetExpr);
            return nullptr;
          }
          std::string queryTypeText =
              resolveSemanticProductTypeText(queryFact->bindingTypeText,
                                             queryFact->bindingTypeTextId);
          if (queryTypeText.empty()) {
            queryTypeText =
                resolveSemanticProductTypeText(queryFact->queryTypeText,
                                               queryFact->queryTypeTextId);
          }
          if (queryTypeText.empty()) {
            error = "incomplete semantic-product pick target query fact: " +
                    describePickTargetName(targetExpr);
            return nullptr;
          }
          const Definition *querySumDef =
              resolveSumDefinitionForTypeText(queryTypeText, function.name);
          if (querySumDef == nullptr) {
            error = "semantic-product pick target query type is not a sum: " +
                    describePickTargetName(targetExpr);
            return nullptr;
          }
          const std::string queryTargetPath =
              std::string(semanticProgramQueryFactResolvedPath(
                  *semanticTargets.semanticProgram, *queryFact));
          if (!queryTargetPath.empty()) {
            const SemanticProgramReturnFact *returnFact =
                findSemanticProductReturnFactByPath(semanticTargets, queryTargetPath);
            const std::string returnTypeText =
                returnFact != nullptr
                    ? resolveSemanticProductTypeText(returnFact->bindingTypeText,
                                                     returnFact->bindingTypeTextId)
                    : std::string{};
            if (!returnTypeText.empty()) {
              const Definition *returnSumDef =
                  resolveSumDefinitionForTypeText(returnTypeText, function.name);
              if (returnSumDef != nullptr &&
                  returnSumDef->fullPath != querySumDef->fullPath) {
                error = "stale semantic-product pick target query type: " +
                        describePickTargetName(targetExpr);
                return nullptr;
              }
            }
          }
          return requirePublishedSumMetadata(*querySumDef, targetExpr);
        }
      }

      return nullptr;
    }

    const Definition * SumHelpersContext::resolvePickTargetSumDefinition(const Expr &targetExpr, const LocalMap &valueLocals) {
      if (const Definition *semanticSumDef =
              resolveSemanticProductPickTargetSumDefinition(targetExpr, valueLocals);
          semanticSumDef != nullptr || !error.empty()) {
        return semanticSumDef;
      }
      if (targetExpr.kind == Expr::Kind::Name) {
        auto localIt = valueLocals.find(targetExpr.name);
        if (localIt != valueLocals.end()) {
          return resolveSumDefinitionForLocalInfo(localIt->second);
        }
      }
      if (targetExpr.kind == Expr::Kind::Call && !targetExpr.isMethodCall) {
        return resolveSumDefinitionForTypeText(targetExpr.name, targetExpr.namespacePrefix);
      }
      return nullptr;
    }

    void SumHelpersContext::emitLoadSumSlotIndirect(int32_t sumPtrLocal, int32_t slotOffset) {
      function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(sumPtrLocal)});
      function.instructions.push_back(
          {IrOpcode::PushI64, static_cast<uint64_t>(slotOffset) * IrSlotBytes});
      function.instructions.push_back({IrOpcode::AddI64, 0});
      function.instructions.push_back({IrOpcode::LoadIndirect, 0});
    }

    void SumHelpersContext::emitSumTagComparison(int32_t sumPtrLocal, int32_t tagValue) {
      emitLoadSumSlotIndirect(sumPtrLocal, 1);
      function.instructions.push_back(
          {IrOpcode::PushI32, static_cast<uint64_t>(static_cast<int32_t>(tagValue))});
      function.instructions.push_back({IrOpcode::CmpEqI32, 0});
    }

    const Definition * SumHelpersContext::findSumPayloadMoveHelper(const std::string &structPath) {
      auto moveIt = defMap.find(structPath + "/Move");
      if (moveIt != defMap.end()) {
        return moveIt->second;
      }
      moveIt = defMap.find(structPath + "/Copy");
      if (moveIt != defMap.end()) {
        return moveIt->second;
      }
      return nullptr;
    }

    const Definition * SumHelpersContext::findSumPayloadDestroyHelper(const std::string &structPath) {
      auto destroyIt = defMap.find(structPath + "/DestroyStack");
      if (destroyIt != defMap.end()) {
        return destroyIt->second;
      }
      destroyIt = defMap.find(structPath + "/Destroy");
      if (destroyIt != defMap.end()) {
        return destroyIt->second;
      }
      return nullptr;
    }

    // Whether some variant's payload has destroy helpers to run when the sum is destroyed.
    bool SumHelpersContext::sumPayloadsNeedDestroy(const Definition &sumDef) {
      for (const auto &variant : sumDef.sumVariants) {
        LoweredSumPayloadStorageInfo payloadInfo;
        if (resolveSumPayloadStorageInfo(sumDef, variant, payloadInfo) && payloadInfo.isAggregate &&
            ir_lowerer::structNeedsDestroyHelpers(
                payloadInfo.structPath,
                [&](const std::string &path) {
                  return ir_lowerer::findStackDestroyHelper(defMap, path);
                },
                resolveStructSlotLayout)) {
          return true;
        }
      }
      error.clear();
      return false;
    }

    bool SumHelpersContext::emitActiveSumPayloadDestroyFromSumPtr(const Definition &sumDef, int32_t sourceSumPtrLocal, const LocalMap &valueLocals) {
      std::vector<size_t> endJumps;
      for (const auto &variant : sumDef.sumVariants) {
        LoweredSumPayloadStorageInfo payloadInfo;
        if (!resolveSemanticProductSumPayloadStorageInfo(
                sumDef, variant, "sum payload destroy", payloadInfo)) {
          if (error.empty()) {
            error = unsupportedSumPayloadError(sumDef, variant);
          }
          return false;
        }
        if (!payloadInfo.isAggregate) {
          continue;
        }
        // The active payload runs its destroy helpers, its fields' included.
        const auto findDestroyHelper = [&](const std::string &path) {
          return ir_lowerer::findStackDestroyHelper(defMap, path);
        };
        if (!ir_lowerer::structNeedsDestroyHelpers(
                payloadInfo.structPath, findDestroyHelper, resolveStructSlotLayout)) {
          continue;
        }
        int32_t tagValue = 0;
        if (!resolveSemanticProductSumVariantTag(
                sumDef, variant, "sum payload destroy", tagValue)) {
          return false;
        }
        emitSumTagComparison(sourceSumPtrLocal, tagValue);
        const size_t nextVariantJump = function.instructions.size();
        function.instructions.push_back({IrOpcode::JumpIfZero, 0});
        const int32_t payloadPtrLocal = allocTempLocal();
        function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(sourceSumPtrLocal)});
        function.instructions.push_back({IrOpcode::PushI64, static_cast<uint64_t>(2) * IrSlotBytes});
        function.instructions.push_back({IrOpcode::AddI64, 0});
        function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(payloadPtrLocal)});
        if (!ir_lowerer::emitStructDestroyHelpersFromPtr(
                payloadPtrLocal,
                payloadInfo.structPath,
                findDestroyHelper,
                resolveStructSlotLayout,
                allocTempLocal,
                [&](IrOpcode op, uint64_t imm) { function.instructions.push_back({op, imm}); },
                valueLocals,
                emitInlineDefinitionCall,
                error)) {
          return false;
        }
        endJumps.push_back(function.instructions.size());
        function.instructions.push_back({IrOpcode::Jump, 0});
        function.instructions[nextVariantJump].imm = static_cast<uint64_t>(function.instructions.size());
      }
      for (size_t jumpIndex : endJumps) {
        function.instructions[jumpIndex].imm = static_cast<uint64_t>(function.instructions.size());
      }
      return true;
    }

    bool SumHelpersContext::emitActiveSumPayloadMoveFromSumPtr(int32_t destBaseLocal,
            const Definition &sumDef,
            int32_t sourceSumPtrLocal,
            const LocalMap &valueLocals) {
      std::vector<size_t> endJumps;
      for (const auto &variant : sumDef.sumVariants) {
        LoweredSumPayloadStorageInfo payloadInfo;
        if (!resolveSemanticProductSumPayloadStorageInfo(
                sumDef, variant, "sum payload move", payloadInfo)) {
          if (error.empty()) {
            error = unsupportedSumPayloadError(sumDef, variant);
          }
          return false;
        }
        int32_t tagValue = 0;
        if (!resolveSemanticProductSumVariantTag(
                sumDef, variant, "sum payload move", tagValue)) {
          return false;
        }
        emitSumTagComparison(sourceSumPtrLocal, tagValue);
        const size_t nextVariantJump = function.instructions.size();
        function.instructions.push_back({IrOpcode::JumpIfZero, 0});
        if (!variant.hasPayload) {
          endJumps.push_back(function.instructions.size());
          function.instructions.push_back({IrOpcode::Jump, 0});
          function.instructions[nextVariantJump].imm =
              static_cast<uint64_t>(function.instructions.size());
          continue;
        }
        if (payloadInfo.isAggregate) {
          const int32_t destPtrLocal = allocTempLocal();
          function.instructions.push_back({IrOpcode::AddressOfLocal, static_cast<uint64_t>(destBaseLocal + 2)});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(destPtrLocal)});
          const int32_t srcPtrLocal = allocTempLocal();
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(sourceSumPtrLocal)});
          function.instructions.push_back({IrOpcode::PushI64, static_cast<uint64_t>(2) * IrSlotBytes});
          function.instructions.push_back({IrOpcode::AddI64, 0});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(srcPtrLocal)});
          if (const Definition *moveHelper = findSumPayloadMoveHelper(payloadInfo.structPath)) {
            if (!ir_lowerer::emitMoveHelperFromPtrs(destPtrLocal,
                                                    srcPtrLocal,
                                                    payloadInfo.structPath,
                                                    moveHelper,
                                                    valueLocals,
                                                    [&](const Expr &callExpr,
                                                        const Definition &callee,
                                                        const LocalMap &callLocals,
                                                        bool requireValue) {
                                                      return emitInlineDefinitionCall(
                                                          callExpr, callee, callLocals, requireValue);
                                                    },
                                                    error)) {
              return false;
            }
          } else if (!emitStructCopyFromPtrs(destPtrLocal, srcPtrLocal, payloadInfo.slotCount)) {
            return false;
          }
        } else {
          emitLoadSumSlotIndirect(sourceSumPtrLocal, 2);
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(destBaseLocal + 2)});
        }
        endJumps.push_back(function.instructions.size());
        function.instructions.push_back({IrOpcode::Jump, 0});
        function.instructions[nextVariantJump].imm = static_cast<uint64_t>(function.instructions.size());
      }
      for (size_t jumpIndex : endJumps) {
        function.instructions[jumpIndex].imm = static_cast<uint64_t>(function.instructions.size());
      }
      return true;
    }

    bool SumHelpersContext::tryEmitLoweredSumMoveIntoLocal(int32_t baseLocal,
            const Definition &sumDef,
            const Expr &initializer,
            const LocalMap &valueLocals,
            bool &emittedOut) {
      emittedOut = false;
      if (initializer.kind != Expr::Kind::Call || initializer.isMethodCall ||
          initializer.isFieldAccess || !isSimpleCallName(initializer, "move") ||
          initializer.args.size() != 1 || initializer.args.front().kind != Expr::Kind::Name ||
          hasNamedArguments(initializer.argNames) || !initializer.templateArgs.empty() ||
          initializer.hasBodyArguments || !initializer.bodyArguments.empty()) {
        return true;
      }
      auto sourceIt = valueLocals.find(initializer.args.front().name);
      if (sourceIt == valueLocals.end()) {
        error = "native backend sum move requires a local source: " + initializer.args.front().name;
        return false;
      }
      const Definition *sourceSumDef = resolveSumDefinitionForLocalInfo(sourceIt->second);
      if (sourceSumDef == nullptr || sourceSumDef->fullPath != sumDef.fullPath) {
        error = "native backend sum move source type mismatch on " + sumDef.fullPath;
        return false;
      }
      const int32_t sourceSumPtrLocal = allocTempLocal();
      function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(sourceIt->second.index)});
      function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(sourceSumPtrLocal)});
      emitLoadSumSlotIndirect(sourceSumPtrLocal, 1);
      function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal + 1)});
      if (!emitActiveSumPayloadMoveFromSumPtr(baseLocal, sumDef, sourceSumPtrLocal, valueLocals)) {
        return false;
      }
      emittedOut = true;
      return true;
    }

    bool SumHelpersContext::isMutableLocalExpr(const Expr &candidate, const LocalMap &localsIn) {
      if (candidate.kind != Expr::Kind::Name) {
        return false;
      }
      auto it = localsIn.find(candidate.name);
      return it != localsIn.end() && it->second.isMutable;
    }

    bool SumHelpersContext::makePickPayloadLocalInfo(const Definition &sumDef,
            const SumVariant &variant,
            LocalInfo &payloadInfoOut) {
      LoweredSumPayloadStorageInfo payloadStorage;
      if (!resolveSemanticProductSumPayloadStorageInfo(
              sumDef, variant, "pick payload local", payloadStorage)) {
        if (error.empty()) {
          error = unsupportedSumPayloadError(sumDef, variant);
        }
        return false;
      }
      payloadInfoOut = {};
      if (payloadStorage.isPointerLike) {
        payloadInfoOut.kind = LocalInfo::Kind::Pointer;
        payloadInfoOut.valueKind = valueKindFromTypeName(payloadStorage.pointerElementTypeText);
        std::string pointerElementStructPath;
        if (payloadInfoOut.valueKind == LocalInfo::ValueKind::Unknown &&
            resolveStructTypeName(payloadStorage.pointerElementTypeText,
                                  sumDef.namespacePrefix,
                                  pointerElementStructPath)) {
          payloadInfoOut.structTypeName = std::move(pointerElementStructPath);
        }
        return true;
      }
      payloadInfoOut.kind = LocalInfo::Kind::Value;
      payloadInfoOut.valueKind =
          payloadStorage.isAggregate ? LocalInfo::ValueKind::Int64 : payloadStorage.valueKind;
      payloadInfoOut.structTypeName = payloadStorage.structPath;
      payloadInfoOut.structSlotCount = payloadStorage.slotCount;
      return true;
    }

    bool SumHelpersContext::bindPickPayload(const Definition &sumDef,
            const SumVariant &variant,
            const Expr &binderExpr,
            int32_t sumPtrLocal,
            LocalMap &branchLocals,
            bool isMutable) {
      LocalInfo payloadInfo;
      if (!makePickPayloadLocalInfo(sumDef, variant, payloadInfo)) {
        return false;
      }
      payloadInfo.isMutable = isMutable;
      payloadInfo.index = nextLocal++;
      if (!payloadInfo.structTypeName.empty()) {
        function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(sumPtrLocal)});
        function.instructions.push_back({IrOpcode::PushI64, static_cast<uint64_t>(2) * IrSlotBytes});
        function.instructions.push_back({IrOpcode::AddI64, 0});
      } else {
        emitLoadSumSlotIndirect(sumPtrLocal, 2);
      }
      function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(payloadInfo.index)});
      branchLocals.emplace(binderExpr.name, payloadInfo);
      return true;
    }

    bool SumHelpersContext::isPickCall(const Expr &candidate) {
      return candidate.kind == Expr::Kind::Call && !candidate.isBinding &&
             !candidate.isMethodCall && !candidate.isFieldAccess &&
             (isSimpleCallName(candidate, "pick") ||
              (candidate.name == "pick" &&
               (candidate.namespacePrefix.empty() ||
                candidate.namespacePrefix == "/Maybe"))) &&
             candidate.templateArgs.empty() && !hasNamedArguments(candidate.argNames);
    }

    bool SumHelpersContext::isPickArmEnvelopeBase(const Expr &candidate) {
      return candidate.kind == Expr::Kind::Call && !candidate.isBinding &&
             !candidate.isMethodCall && !candidate.isFieldAccess &&
             candidate.hasBodyArguments && !candidate.name.empty() &&
             candidate.templateArgs.empty() && !hasNamedArguments(candidate.argNames);
    }

    bool SumHelpersContext::isPayloadPickArmEnvelope(const Expr &candidate) {
      return isPickArmEnvelopeBase(candidate) && candidate.args.size() == 1 &&
             candidate.args.front().kind == Expr::Kind::Name;
    }

    bool SumHelpersContext::isUnitCallPickArmEnvelope(const Expr &candidate) {
      return isPickArmEnvelopeBase(candidate) && candidate.args.empty();
    }

    bool SumHelpersContext::isUnitBindingPickArmEnvelope(const Expr &candidate) {
      return candidate.kind == Expr::Kind::Call && candidate.isBinding &&
             !candidate.isMethodCall && !candidate.isFieldAccess &&
             !candidate.name.empty() && candidate.transforms.empty() &&
             candidate.templateArgs.empty() && !candidate.hasBodyArguments &&
             candidate.bodyArguments.empty() && candidate.args.size() == 1 &&
             candidate.argNames.size() == 1 && !candidate.argNames.front().has_value();
    }

    bool SumHelpersContext::isSupportedPickArmEnvelope(const Expr &candidate) {
      return isPayloadPickArmEnvelope(candidate) ||
             isUnitCallPickArmEnvelope(candidate) ||
             isUnitBindingPickArmEnvelope(candidate);
    }

    std::vector<const Expr *> SumHelpersContext::pickArmBodyExprs(const Expr &arm) {
      std::vector<const Expr *> out;
      if (isPayloadPickArmEnvelope(arm) || isUnitCallPickArmEnvelope(arm)) {
        out.reserve(arm.bodyArguments.size());
        for (const Expr &bodyExpr : arm.bodyArguments) {
          out.push_back(&bodyExpr);
        }
        return out;
      }
      if (!isUnitBindingPickArmEnvelope(arm)) {
        return out;
      }
      const Expr &initializer = arm.args.front();
      if (initializer.kind == Expr::Kind::Call && initializer.name == "block" &&
          initializer.hasBodyArguments && initializer.args.empty()) {
        out.reserve(initializer.bodyArguments.size());
        for (const Expr &bodyExpr : initializer.bodyArguments) {
          out.push_back(&bodyExpr);
        }
        return out;
      }
      out.push_back(&initializer);
      return out;
    }

    const Expr * SumHelpersContext::findPickArmValueExpr(const Expr &arm) {
      const Expr *valueExpr = nullptr;
      bool sawReturn = false;
      for (const Expr *bodyExprPtr : pickArmBodyExprs(arm)) {
        const Expr &bodyExpr = *bodyExprPtr;
        if (bodyExpr.isBinding) {
          continue;
        }
        if (isReturnCall(bodyExpr)) {
          if (bodyExpr.args.size() != 1) {
            return nullptr;
          }
          valueExpr = &bodyExpr.args.front();
          sawReturn = true;
          continue;
        }
        if (!sawReturn) {
          valueExpr = &bodyExpr;
        }
      }
      return valueExpr;
    }

    const SumVariant * SumHelpersContext::resolvePickArmVariant(const Definition &sumDef, const Expr &arm) {
      const SumVariant *variant = findSumVariantByName(sumDef, arm.name);
      if (variant == nullptr) {
        error = "native backend unknown pick variant on " + sumDef.fullPath + ": " + arm.name;
        return nullptr;
      }
      return variant;
    }

    bool SumHelpersContext::emitPickArmPrefixStatements(const Expr &arm, const Expr *valueExpr, LocalMap &branchLocals) {
      for (const Expr *bodyExprPtr : pickArmBodyExprs(arm)) {
        const Expr &bodyExpr = *bodyExprPtr;
        if (isReturnCall(bodyExpr) && bodyExpr.args.size() == 1 && valueExpr == &bodyExpr.args.front()) {
          return true;
        }
        if (&bodyExpr == valueExpr) {
          return true;
        }
        if (!emitStatement(bodyExpr, branchLocals)) {
          return false;
        }
      }
      return true;
    }

    std::optional<bool> SumHelpersContext::resolveSemanticProductPickAggregateResultStructPath(const Expr &valueExpr,
            const Definition &sumDef,
            const SumVariant &variant,
            std::string &structPathOut) {
      structPathOut.clear();
      const auto &semanticTargets = callResolutionAdapters.semanticProductTargets;
      if (!semanticTargets.hasSemanticProduct || valueExpr.semanticNodeId == 0) {
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
              findSemanticProductBindingFact(semanticTargets, valueExpr);
          bindingFact != nullptr) {
        addSemanticProductCandidateTypeText(bindingFact->bindingTypeText,
                                            bindingFact->bindingTypeTextId);
      }
      if (const SemanticProgramQueryFact *queryFact =
              findSemanticProductQueryFact(semanticTargets, valueExpr);
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
        if (valueKindOrPointerLikeFromTypeName(normalizedTypeText) != LocalInfo::ValueKind::Unknown) {
          return true;
        }

        auto tryResolveStruct = [&](const std::string &namespacePrefix) {
          std::string candidateStructPath;
          if (resolveStructTypeName(normalizedTypeText,
                                    namespacePrefix,
                                    candidateStructPath)) {
            structPathOut = std::move(candidateStructPath);
            return true;
          }
          return false;
        };
        if (tryResolveStruct(valueExpr.namespacePrefix) ||
            tryResolveStruct(function.name) ||
            tryResolveStruct(sumDef.namespacePrefix)) {
          return true;
        }
        return false;
      };

      for (const std::string &typeText : candidateTypeTexts) {
        if (resolveTypeText(typeText)) {
          return true;
        }
      }
      error = "stale semantic-product pick aggregate result metadata: " +
              sumDef.fullPath + "/" + variant.name;
      return false;
    }

    bool SumHelpersContext::inferPickAggregateResult(const Expr &expr,
            const Definition &sumDef,
            const LocalMap &valueLocals,
            std::string &structPathOut,
            StructSlotLayout &layoutOut) {
      structPathOut.clear();
      bool sawAggregateResult = false;
      for (const Expr &arm : expr.bodyArguments) {
        const bool payloadArm = isPayloadPickArmEnvelope(arm);
        if (!isSupportedPickArmEnvelope(arm)) {
          error = "native backend requires pick arms as variant blocks";
          return false;
        }
        const SumVariant *variant = resolvePickArmVariant(sumDef, arm);
        if (variant == nullptr) {
          return false;
        }
        if (variant->hasPayload != payloadArm) {
          error = variant->hasPayload
                      ? "native backend requires payload pick arm for " +
                            sumDef.fullPath + "/" + variant->name
                      : "native backend unit pick arm cannot bind payload: " +
                            sumDef.fullPath + "/" + variant->name;
          return false;
        }
        int32_t tagValue = 0;
        if (!resolveSemanticProductSumVariantTag(sumDef, *variant, "pick arm", tagValue)) {
          return false;
        }
        LocalMap branchLocals = valueLocals;
        if (payloadArm) {
          LocalInfo payloadInfo;
          if (!makePickPayloadLocalInfo(sumDef, *variant, payloadInfo)) {
            return false;
          }
          branchLocals.emplace(arm.args.front().name, payloadInfo);
        }
        const Expr *valueExpr = findPickArmValueExpr(arm);
        if (valueExpr == nullptr) {
          error = "native backend requires pick arms to produce a value";
          return false;
        }
        std::string branchStructPath;
        const std::optional<bool> resolvedBySemanticProduct =
            resolveSemanticProductPickAggregateResultStructPath(
                *valueExpr, sumDef, *variant, branchStructPath);
        if (resolvedBySemanticProduct.has_value() && !*resolvedBySemanticProduct) {
          return false;
        }
        if (!resolvedBySemanticProduct.has_value()) {
          branchStructPath = inferStructExprPath(*valueExpr, branchLocals);
        }
        if (branchStructPath.empty()) {
          structPathOut.clear();
          return true;
        }
        if (!sawAggregateResult) {
          structPathOut = branchStructPath;
          sawAggregateResult = true;
          continue;
        }
        if (branchStructPath != structPathOut) {
          error = "native backend requires pick aggregate arms to produce the same struct type";
          return false;
        }
      }
      if (!sawAggregateResult) {
        structPathOut.clear();
        return true;
      }
      if (!resolveStructSlotLayout(structPathOut, layoutOut)) {
        error = "native backend could not resolve pick aggregate result layout: " + structPathOut;
        return false;
      }
      return true;
    }

    LoweredSumPickEmitResult SumHelpersContext::tryEmitPickExpr(const Expr &expr, const LocalMap &valueLocals) {
      if (!isPickCall(expr)) {
        return LoweredSumPickEmitResult::NotMatched;
      }
      if (!expr.hasBodyArguments && expr.bodyArguments.empty()) {
        return LoweredSumPickEmitResult::NotMatched;
      }
      if (expr.args.size() != 1 || expr.bodyArguments.empty()) {
        error = "native backend requires pick(value) with variant arms";
        return LoweredSumPickEmitResult::Error;
      }
      const Definition *sumDef = resolvePickTargetSumDefinition(expr.args.front(), valueLocals);
      if (sumDef == nullptr) {
        if (error.empty()) {
          error = "native backend pick target requires sum value";
        }
        return LoweredSumPickEmitResult::Error;
      }
      const int32_t sumPtrLocal = allocTempLocal();
      if (!emitExpr(expr.args.front(), valueLocals)) {
        return LoweredSumPickEmitResult::Error;
      }
      function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(sumPtrLocal)});
      std::string aggregateResultStructPath;
      StructSlotLayout aggregateResultLayout;
      if (!inferPickAggregateResult(expr,
                                    *sumDef,
                                    valueLocals,
                                    aggregateResultStructPath,
                                    aggregateResultLayout)) {
        return LoweredSumPickEmitResult::Error;
      }
      const bool emitsAggregateResult = !aggregateResultStructPath.empty();
      int32_t resultBaseLocal = -1;
      int32_t resultLocal = allocTempLocal();
      if (emitsAggregateResult) {
        resultBaseLocal = nextLocal;
        nextLocal += aggregateResultLayout.totalSlots;
        function.instructions.push_back(
            {IrOpcode::PushI32, static_cast<uint64_t>(static_cast<int32_t>(aggregateResultLayout.totalSlots - 1))});
        function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(resultBaseLocal)});
        function.instructions.push_back({IrOpcode::AddressOfLocal, static_cast<uint64_t>(resultBaseLocal)});
        function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(resultLocal)});
      }
      std::vector<size_t> endJumps;
      for (const Expr &arm : expr.bodyArguments) {
        const bool payloadArm = isPayloadPickArmEnvelope(arm);
        if (!isSupportedPickArmEnvelope(arm)) {
          error = "native backend requires pick arms as variant blocks";
          return LoweredSumPickEmitResult::Error;
        }
        const SumVariant *variant = resolvePickArmVariant(*sumDef, arm);
        if (variant == nullptr) {
          return LoweredSumPickEmitResult::Error;
        }
        if (variant->hasPayload != payloadArm) {
          error = variant->hasPayload
                      ? "native backend requires payload pick arm for " +
                            sumDef->fullPath + "/" + variant->name
                      : "native backend unit pick arm cannot bind payload: " +
                            sumDef->fullPath + "/" + variant->name;
          return LoweredSumPickEmitResult::Error;
        }
        int32_t tagValue = 0;
        if (!resolveSemanticProductSumVariantTag(*sumDef, *variant, "pick arm", tagValue)) {
          return LoweredSumPickEmitResult::Error;
        }
        emitSumTagComparison(sumPtrLocal, tagValue);
        const size_t nextArmJump = function.instructions.size();
        function.instructions.push_back({IrOpcode::JumpIfZero, 0});
        LocalMap branchLocals = valueLocals;
        if (payloadArm &&
            !bindPickPayload(*sumDef,
                             *variant,
                             arm.args.front(),
                             sumPtrLocal,
                             branchLocals,
                             isMutableLocalExpr(expr.args.front(), valueLocals))) {
          return LoweredSumPickEmitResult::Error;
        }
        const Expr *valueExpr = findPickArmValueExpr(arm);
        if (valueExpr == nullptr) {
          error = "native backend requires pick arms to produce a value";
          return LoweredSumPickEmitResult::Error;
        }
        if (!emitPickArmPrefixStatements(arm, valueExpr, branchLocals) ||
            !emitExpr(*valueExpr, branchLocals)) {
          return LoweredSumPickEmitResult::Error;
        }
        if (emitsAggregateResult) {
          const int32_t srcPtrLocal = allocTempLocal();
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(srcPtrLocal)});
          if (!emitStructCopyFromPtrs(resultLocal, srcPtrLocal, aggregateResultLayout.totalSlots)) {
            return LoweredSumPickEmitResult::Error;
          }
          if (shouldDisarmStructCopySourceExpr(*valueExpr)) {
            ir_lowerer::emitDisarmTemporaryStructAfterCopy(
                [&](IrOpcode op, uint64_t imm) { function.instructions.push_back({op, imm}); },
                srcPtrLocal,
                aggregateResultStructPath);
          }
        } else {
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(resultLocal)});
        }
        endJumps.push_back(function.instructions.size());
        function.instructions.push_back({IrOpcode::Jump, 0});
        function.instructions[nextArmJump].imm =
            static_cast<uint64_t>(function.instructions.size());
      }
      for (size_t jumpIndex : endJumps) {
        function.instructions[jumpIndex].imm =
            static_cast<uint64_t>(function.instructions.size());
      }
      function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(resultLocal)});
      return LoweredSumPickEmitResult::Emitted;
    }

    LoweredSumPickEmitResult SumHelpersContext::tryEmitPickStatement(const Expr &stmt, LocalMap &localsIn) {
      if (!isPickCall(stmt)) {
        return LoweredSumPickEmitResult::NotMatched;
      }
      if (!stmt.hasBodyArguments && stmt.bodyArguments.empty()) {
        return LoweredSumPickEmitResult::NotMatched;
      }
      if (stmt.args.size() != 1 || stmt.bodyArguments.empty()) {
        error = "native backend requires pick(value) with variant arms";
        return LoweredSumPickEmitResult::Error;
      }
      const Definition *sumDef = resolvePickTargetSumDefinition(stmt.args.front(), localsIn);
      if (sumDef == nullptr) {
        if (error.empty()) {
          error = "native backend pick target requires sum value";
        }
        return LoweredSumPickEmitResult::Error;
      }
      const int32_t sumPtrLocal = allocTempLocal();
      if (!emitExpr(stmt.args.front(), localsIn)) {
        return LoweredSumPickEmitResult::Error;
      }
      function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(sumPtrLocal)});
      std::vector<size_t> endJumps;
      for (const Expr &arm : stmt.bodyArguments) {
        const bool payloadArm = isPayloadPickArmEnvelope(arm);
        if (!isSupportedPickArmEnvelope(arm)) {
          error = "native backend requires pick arms as variant blocks";
          return LoweredSumPickEmitResult::Error;
        }
        const SumVariant *variant = resolvePickArmVariant(*sumDef, arm);
        if (variant == nullptr) {
          return LoweredSumPickEmitResult::Error;
        }
        if (variant->hasPayload != payloadArm) {
          error = variant->hasPayload
                      ? "native backend requires payload pick arm for " +
                            sumDef->fullPath + "/" + variant->name
                      : "native backend unit pick arm cannot bind payload: " +
                            sumDef->fullPath + "/" + variant->name;
          return LoweredSumPickEmitResult::Error;
        }
        int32_t tagValue = 0;
        if (!resolveSemanticProductSumVariantTag(*sumDef, *variant, "pick arm", tagValue)) {
          return LoweredSumPickEmitResult::Error;
        }
        emitSumTagComparison(sumPtrLocal, tagValue);
        const size_t nextArmJump = function.instructions.size();
        function.instructions.push_back({IrOpcode::JumpIfZero, 0});
        LocalMap branchLocals = localsIn;
        if (payloadArm &&
            !bindPickPayload(*sumDef,
                             *variant,
                             arm.args.front(),
                             sumPtrLocal,
                             branchLocals,
                             isMutableLocalExpr(stmt.args.front(), localsIn))) {
          return LoweredSumPickEmitResult::Error;
        }
        for (const Expr *bodyExprPtr : pickArmBodyExprs(arm)) {
          if (!emitStatement(*bodyExprPtr, branchLocals)) {
            return LoweredSumPickEmitResult::Error;
          }
        }
        endJumps.push_back(function.instructions.size());
        function.instructions.push_back({IrOpcode::Jump, 0});
        function.instructions[nextArmJump].imm =
            static_cast<uint64_t>(function.instructions.size());
      }
      for (size_t jumpIndex : endJumps) {
        function.instructions[jumpIndex].imm =
            static_cast<uint64_t>(function.instructions.size());
      }
      return LoweredSumPickEmitResult::Emitted;
    }


} // namespace primec::ir_lowerer
