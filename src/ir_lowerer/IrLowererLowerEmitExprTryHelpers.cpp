#include "IrLowererLowerEmitExprTryHelpers.h"

#include "primec/ir_lowerer/IrLowererBindingTransformHelpers.h"
#include "IrLowererCountAccessClassifiers.h"
#include "primec/ir_lowerer/IrLowererFlowHelpers.h"
#include "primec/ir_lowerer/IrLowererHelpers.h"
#include "primec/ir_lowerer/IrLowererInlineCallContextHelpers.h"
#include "primec/ir_lowerer/IrLowererInlineParamHelpers.h"
#include "primec/ir_lowerer/IrLowererInlineStructArgHelpers.h"
#include "primec/ir_lowerer/IrLowererOnErrorHelpers.h"
#include "IrLowererRequirementContractHelpers.h"
#include "primec/ir_lowerer/IrLowererSetupTypeCollectionHelpers.h"
#include "primec/ir_lowerer/IrLowererSetupTypeHelpers.h"
#include "primec/ir_lowerer/IrLowererStructLayoutHelpers.h"
#include "primec/ir_lowerer/IrLowererTemplateTypeParseHelpers.h"
#include "IrLowererVectorRecordLayoutHelpers.h"
#include "primec/ir/StdlibCollectionPaths.h"
#include "primec/support/Diagnostics.h"
#include "primec/support/SourceLocationMapper.h"
#include "primec/support/CollectionHelperNames.h"

#include <limits>

namespace primec::ir_lowerer {

namespace {

bool matchesGeneratedSpecializedType(std::string_view path, std::string_view collectionName,
                                     std::string_view typeName) {
  const std::string typePath = experimentalCollectionTypePath(collectionName, typeName);
  return path.rfind(typePath + "__", 0) == 0;
}

} // namespace

std::optional<bool> tryLowerEmitExprTryHelper(
    LowerSetupStageState &setupStage,
    LowerReturnEmitStageState &stateOut,
    const CallResolutionAdapters &callResolutionAdapters,
    SumHelpersContext &sumHelpers,
    const Expr &expr,
    const LocalMap &localsIn,
    std::string &error) {
    auto &function = setupStage.function;
    auto &nextLocal = setupStage.nextLocal;
    auto &structFieldInfoByName = setupStage.structFieldInfoByName;
    auto &defMap = setupStage.defMap;
    int32_t &onErrorTempCounter = setupStage.onErrorTempCounter;
    auto &currentOnError = setupStage.currentOnError;
    auto &currentReturnResult = setupStage.currentReturnResult;
    auto *&activeInlineContext = stateOut.activeInlineContext;
    using InlineContext = LowerReturnEmitInlineContext;
    auto &emitInlineDefinitionCall = stateOut.emitInlineDefinitionCall;
    auto &emitFileScopeCleanupAll = stateOut.emitFileScopeCleanupAll;
    auto &setupLocalsOrchestration = setupStage.setupLocalsOrchestration;
    const bool &returnsVoid = setupLocalsOrchestration.entryReturnConfig.returnsVoid;
    auto &getReturnInfo = setupStage.inferenceSetupBootstrap.getReturnInfo;
    auto &resolveMethodCallDefinition = setupStage.inferenceSetupBootstrap.resolveMethodCallDefinition;
    auto &inferExprKind = setupStage.inferenceSetupBootstrap.inferExprKind;
    const auto &structSlotResolutionAdapters = setupLocalsOrchestration.structSlotResolutionAdapters;
    auto &resolveStructSlotLayout = structSlotResolutionAdapters.resolveStructSlotLayout;
    const auto &uninitializedResolutionAdapters = setupLocalsOrchestration.uninitializedResolutionAdapters;
    auto &inferStructExprPath = uninitializedResolutionAdapters.inferStructExprPath;
    auto &emitExpr = stateOut.emitExpr;
    auto &allocTempLocal = stateOut.allocTempLocal;
    auto &resolveDefinitionCall = stateOut.resolveDefinitionCall;
    auto &emitStructCopyFromPtrs = stateOut.emitStructCopyFromPtrs;
    const auto &structTypeResolutionAdapters =
        setupLocalsOrchestration.setupTypeAndStructTypeAdapters.structTypeResolutionAdapters;
    auto &resolveStructTypeName = structTypeResolutionAdapters.resolveStructTypeName;

        if (!expr.isMethodCall && isSimpleCallName(expr, "try")) {
          if (expr.args.size() != 1) {
            error = "try requires exactly one argument";
            return false;
          }
          if (!currentOnError.has_value()) {
            error = "missing on_error for ? usage";
            return false;
          }
          if (!currentReturnResult.has_value() && returnsVoid) {
            error = "try requires Result or int return type";
            return false;
          }
          ResultExprInfo resultInfo;
          std::string semanticTryFactError;
          auto resolveSemanticTryProductTypeText = [&](const std::string &typeText,
                                                       SymbolId typeTextId) {
            const auto &semanticTargets =
                callResolutionAdapters.semanticProductTargets;
            if (semanticTargets.semanticProgram != nullptr &&
                typeTextId != InvalidSymbolId) {
              std::string resolvedTypeText = std::string(
                  semanticProgramResolveCallTargetString(
                      *semanticTargets.semanticProgram, typeTextId));
              if (!resolvedTypeText.empty()) {
                return trimTemplateTypeText(resolvedTypeText);
              }
            }
            return trimTemplateTypeText(typeText);
          };
          auto applySemanticTryValueType = [&](const std::string &valueTypeText,
                                               ResultExprInfo &resultInfoOut) {
            const std::string trimmedValueType = trimTemplateTypeText(valueTypeText);
            if (trimmedValueType.empty()) {
              return false;
            }
            resultInfoOut.valueCollectionKind = LocalInfo::Kind::Value;
            resultInfoOut.valueKind = LocalInfo::ValueKind::Unknown;
            resultInfoOut.valueMapKeyKind = LocalInfo::ValueKind::Unknown;
            resultInfoOut.valueIsFileHandle = false;
            resultInfoOut.valueStructType.clear();
            if (ir_lowerer::resolveSupportedResultCollectionType(
                    trimmedValueType,
                    resultInfoOut.valueCollectionKind,
                    resultInfoOut.valueKind,
                    &resultInfoOut.valueMapKeyKind)) {
              return true;
            }
            std::string baseType;
            std::string typeArgs;
            if (splitTemplateTypeName(trimmedValueType, baseType, typeArgs) &&
                normalizeCollectionBindingTypeName(baseType) == "File") {
              resultInfoOut.valueKind = LocalInfo::ValueKind::Int64;
              resultInfoOut.valueIsFileHandle = true;
              return true;
            }
            if (trimmedValueType == "ContainerError" ||
                trimmedValueType == collection_helpers::kCanonicalContainerErrorType) {
              resultInfoOut.valueStructType = collection_helpers::kCanonicalContainerErrorType;
              return true;
            }
            if (trimmedValueType == "ImageError" ||
                trimmedValueType == "/std/image/ImageError") {
              resultInfoOut.valueStructType = "/std/image/ImageError";
              return true;
            }
            if (trimmedValueType == "GfxError" ||
                trimmedValueType == "/std/gfx/GfxError" ||
                trimmedValueType == "/std/gfx/experimental/GfxError") {
              resultInfoOut.valueStructType =
                  trimmedValueType == "/std/gfx/experimental/GfxError"
                      ? "/std/gfx/experimental/GfxError"
                      : "/std/gfx/GfxError";
              return true;
            }
            resultInfoOut.valueKind = valueKindFromTypeName(trimmedValueType);
            if (resultInfoOut.valueKind != LocalInfo::ValueKind::Unknown) {
              return true;
            }
            std::string resolvedStructPath;
            if (resolveStructTypeName(trimmedValueType, expr.namespacePrefix, resolvedStructPath)) {
              resultInfoOut.valueStructType = std::move(resolvedStructPath);
              return true;
            }
            resultInfoOut.valueStructType = trimmedValueType;
            if (!resultInfoOut.valueStructType.empty() && resultInfoOut.valueStructType.front() != '/') {
              resultInfoOut.valueStructType.insert(resultInfoOut.valueStructType.begin(), '/');
            }
            return true;
          };
          auto applySemanticOperandResultType = [&](const Expr &operandExpr,
                                                    ResultExprInfo &resultInfoOut) {
            if (!callResolutionAdapters.semanticProductTargets.hasSemanticProduct ||
                operandExpr.semanticNodeId == 0) {
              return false;
            }
            const auto *queryFact =
                findSemanticProductQueryFactBySemanticId(
                    callResolutionAdapters.semanticProductTargets.semanticIndex,
                    operandExpr);
            if (queryFact == nullptr || !queryFact->hasResultType) {
              return false;
            }
            resultInfoOut.isResult = true;
            resultInfoOut.hasValue = queryFact->resultTypeHasValue;
            resultInfoOut.errorType =
                resolveSemanticTryProductTypeText(queryFact->resultErrorType,
                                                  queryFact->resultErrorTypeId);
            if (!resultInfoOut.hasValue) {
              return true;
            }
            return applySemanticTryValueType(
                resolveSemanticTryProductTypeText(queryFact->resultValueType,
                                                  queryFact->resultValueTypeId),
                resultInfoOut);
          };
          auto resolveLambdaReturnedValueExpr = [&](const Expr &lambdaExpr,
                                                    const Expr *&valueExprOut) {
            valueExprOut = nullptr;
            if (!lambdaExpr.isLambda) {
              return false;
            }
            for (size_t i = 0; i < lambdaExpr.bodyArguments.size(); ++i) {
              const Expr &bodyExpr = lambdaExpr.bodyArguments[i];
              const bool isLast = (i + 1 == lambdaExpr.bodyArguments.size());
              if (bodyExpr.isBinding || !isLast) {
                continue;
              }
              if (isSimpleCallName(bodyExpr, "return")) {
                if (bodyExpr.args.size() != 1) {
                  return false;
                }
                valueExprOut = &bodyExpr.args.front();
                return true;
              }
              valueExprOut = &bodyExpr;
              return true;
            }
            return false;
          };
          auto applyBuiltinCombinatorStructFallback = [&](const Expr &operandExpr,
                                                          ResultExprInfo &resultInfoOut) {
            if (operandExpr.kind != Expr::Kind::Call) {
              return false;
            }
            const Expr *candidateValueExpr = nullptr;
            if ((operandExpr.name == "map" && operandExpr.args.size() == 3) ||
                (operandExpr.name == "and_then" && operandExpr.args.size() == 3)) {
              if (!resolveLambdaReturnedValueExpr(operandExpr.args[2], candidateValueExpr)) {
                return false;
              }
            } else if (operandExpr.name == "map2" && operandExpr.args.size() == 4) {
              if (!resolveLambdaReturnedValueExpr(operandExpr.args[3], candidateValueExpr)) {
                return false;
              }
            } else {
              return false;
            }
            if (candidateValueExpr == nullptr) {
              return false;
            }
            const auto isResultOkCall = [&](const Expr &candidateExpr) {
              return candidateExpr.kind == Expr::Kind::Call &&
                     candidateExpr.name == "ok" &&
                     candidateExpr.args.size() == 2 &&
                     candidateExpr.args.front().kind == Expr::Kind::Name &&
                     candidateExpr.args.front().name == "Result";
            };
            if (operandExpr.name == "and_then" && isResultOkCall(*candidateValueExpr)) {
              candidateValueExpr = &candidateValueExpr->args[1];
            }
            if (candidateValueExpr->kind != Expr::Kind::Call) {
              return false;
            }
            auto resolveScopedCallPath = [](const Expr &candidateExpr) {
              if (!candidateExpr.name.empty() && candidateExpr.name.front() == '/') {
                return candidateExpr.name;
              }
              if (candidateExpr.namespacePrefix.empty()) {
                return candidateExpr.name;
              }
              std::string scopedPath = candidateExpr.namespacePrefix;
              if (!scopedPath.empty() && scopedPath.front() != '/') {
                scopedPath.insert(scopedPath.begin(), '/');
              }
              if (!candidateExpr.name.empty()) {
                if (!scopedPath.empty() && scopedPath.back() != '/') {
                  scopedPath.push_back('/');
                }
                scopedPath += candidateExpr.name;
              }
              return scopedPath;
            };
            std::string valueStructType;
            const Definition *valueDef = resolveDefinitionCall(*candidateValueExpr);
            const std::string candidateValuePath = resolveScopedCallPath(*candidateValueExpr);
            if (valueDef != nullptr && isStructDefinition(*valueDef)) {
              valueStructType = valueDef->fullPath;
            } else if (candidateValuePath == "ContainerError" ||
                       candidateValuePath == collection_helpers::kCanonicalContainerErrorType) {
              valueStructType = collection_helpers::kCanonicalContainerErrorType;
            } else if (candidateValuePath == "ImageError" ||
                       candidateValuePath == "/std/image/ImageError") {
              valueStructType = "/std/image/ImageError";
            } else if (candidateValuePath == "GfxError" ||
                       candidateValuePath == "/std/gfx/GfxError" ||
                       candidateValuePath == "/std/gfx/experimental/GfxError") {
              valueStructType = candidateValuePath == "/std/gfx/experimental/GfxError"
                                    ? "/std/gfx/experimental/GfxError"
                                    : "/std/gfx/GfxError";
            }
            if (valueStructType.empty()) {
              return false;
            }
            resultInfoOut.isResult = true;
            resultInfoOut.hasValue = true;
            resultInfoOut.valueCollectionKind = LocalInfo::Kind::Value;
            resultInfoOut.valueKind = LocalInfo::ValueKind::Unknown;
            resultInfoOut.valueMapKeyKind = LocalInfo::ValueKind::Unknown;
            resultInfoOut.valueIsFileHandle = false;
            resultInfoOut.valueStructType = std::move(valueStructType);
            return true;
          };
          auto applyBuiltinCombinatorValueKindFallback = [&](const Expr &operandExpr,
                                                             ResultExprInfo &resultInfoOut) {
            if (operandExpr.kind != Expr::Kind::Call ||
                resultInfoOut.valueCollectionKind != LocalInfo::Kind::Value ||
                resultInfoOut.valueKind != LocalInfo::ValueKind::Unknown ||
                resultInfoOut.valueMapKeyKind != LocalInfo::ValueKind::Unknown ||
                !resultInfoOut.valueStructType.empty() ||
                resultInfoOut.valueIsFileHandle) {
              return false;
            }
            const Expr *candidateValueExpr = nullptr;
            if ((operandExpr.name == "map" && operandExpr.args.size() == 3) ||
                (operandExpr.name == "and_then" && operandExpr.args.size() == 3)) {
              if (!resolveLambdaReturnedValueExpr(operandExpr.args[2], candidateValueExpr)) {
                return false;
              }
            } else if (operandExpr.name == "map2" && operandExpr.args.size() == 4) {
              if (!resolveLambdaReturnedValueExpr(operandExpr.args[3], candidateValueExpr)) {
                return false;
              }
            } else {
              return false;
            }
            const auto isResultOkCall = [&](const Expr &candidateExpr) {
              return candidateExpr.kind == Expr::Kind::Call &&
                     candidateExpr.name == "ok" &&
                     candidateExpr.args.size() == 2 &&
                     candidateExpr.args.front().kind == Expr::Kind::Name &&
                     candidateExpr.args.front().name == "Result";
            };
            if (operandExpr.name == "and_then" && candidateValueExpr != nullptr &&
                isResultOkCall(*candidateValueExpr)) {
              candidateValueExpr = &candidateValueExpr->args[1];
            }
            if (candidateValueExpr == nullptr) {
              return false;
            }
            std::string builtinComparisonName;
            if (!ir_lowerer::getBuiltinComparisonName(*candidateValueExpr, builtinComparisonName)) {
              return false;
            }
            resultInfoOut.isResult = true;
            resultInfoOut.hasValue = true;
            resultInfoOut.valueCollectionKind = LocalInfo::Kind::Value;
            resultInfoOut.valueKind = LocalInfo::ValueKind::Bool;
            resultInfoOut.valueMapKeyKind = LocalInfo::ValueKind::Unknown;
            resultInfoOut.valueIsFileHandle = false;
            resultInfoOut.valueStructType.clear();
            return true;
          };
          if (callResolutionAdapters.semanticProductTargets.hasSemanticProduct && expr.semanticNodeId != 0) {
            const auto *tryFact =
                findSemanticProductTryFactBySemanticId(
                    callResolutionAdapters.semanticProductTargets.semanticIndex,
                    expr);
            if (tryFact != nullptr) {
              resultInfo.isResult = true;
              resultInfo.hasValue = true;
              resultInfo.errorType =
                  resolveSemanticTryProductTypeText(tryFact->errorType,
                                                    tryFact->errorTypeId);
              if (!applySemanticTryValueType(
                      resolveSemanticTryProductTypeText(tryFact->valueType,
                                                        tryFact->valueTypeId),
                      resultInfo)) {
                semanticTryFactError = "incomplete semantic-product try fact: try";
                resultInfo = ResultExprInfo{};
              }
            } else {
              semanticTryFactError = "missing semantic-product try fact: try";
            }
          }
          auto resolveResultFieldInfo = [&](const Expr &valueExpr, ResultExprInfo &fieldResultOut) {
            fieldResultOut = ResultExprInfo{};
            if (!(valueExpr.kind == Expr::Kind::Call && valueExpr.isFieldAccess && valueExpr.args.size() == 1)) {
              return false;
            }

            const std::string receiverStructPath = inferStructExprPath(valueExpr.args.front(), localsIn);
            if (receiverStructPath.empty()) {
              return false;
            }

            LayoutFieldBinding fieldBinding;
            if (!ir_lowerer::resolveStructLayoutFieldBinding(
                    receiverStructPath, valueExpr.name, structFieldInfoByName, defMap, fieldBinding) ||
                normalizeCollectionBindingTypeName(fieldBinding.typeName) != "Result") {
              return false;
            }

            std::vector<std::string> resultArgs;
            if (fieldBinding.typeTemplateArg.empty() ||
                !splitTemplateArgs(fieldBinding.typeTemplateArg, resultArgs) ||
                (resultArgs.size() != 1 && resultArgs.size() != 2)) {
              return false;
            }

            fieldResultOut.isResult = true;
            fieldResultOut.hasValue = (resultArgs.size() == 2);
            fieldResultOut.errorType = trimTemplateTypeText(resultArgs.back());
            if (!fieldResultOut.hasValue) {
              return true;
            }

            const std::string valueTypeText = trimTemplateTypeText(resultArgs.front());
            fieldResultOut.valueCollectionKind = LocalInfo::Kind::Value;
            fieldResultOut.valueKind = LocalInfo::ValueKind::Unknown;
            fieldResultOut.valueMapKeyKind = LocalInfo::ValueKind::Unknown;
            if (!ir_lowerer::resolveSupportedResultCollectionType(
                    valueTypeText,
                    fieldResultOut.valueCollectionKind,
                    fieldResultOut.valueKind,
                    &fieldResultOut.valueMapKeyKind)) {
              std::string valueStructType;
              if (resolveStructTypeName(valueTypeText, valueExpr.namespacePrefix, valueStructType)) {
                fieldResultOut.valueStructType = std::move(valueStructType);
              } else {
                fieldResultOut.valueKind = valueKindFromTypeName(valueTypeText);
              }
            }
            return fieldResultOut.valueCollectionKind != LocalInfo::Kind::Value ||
                   fieldResultOut.valueKind != LocalInfo::ValueKind::Unknown ||
                   !fieldResultOut.valueStructType.empty();
          };
#include "IrLowererLowerEmitExprTryHelpersBody.h"
          bool emittedStdlibResultSumTry = false;
          if (!tryEmitStdlibResultSumTry(emittedStdlibResultSumTry)) {
            return false;
          }
          if (emittedStdlibResultSumTry) {
            return true;
          }

          if (resultInfo.hasValue) {
            if (!ir_lowerer::isSupportedPackedResultValueInfo(
                    resultInfo,
                    [&](const std::string &structPath, StructSlotLayoutInfo &layoutOut) {
                      return resolveStructSlotLayout(structPath, layoutOut);
                    })) {
              error = ir_lowerer::unsupportedPackedResultValueKindError("try");
              return false;
            }
            const int32_t errorLocal = allocTempLocal();
            ir_lowerer::emitResultWhyErrorLocalFromResult(
                resultLocal,
                resultInfo,
                errorLocal,
                allocTempLocal,
                [&](IrOpcode op, uint64_t imm) { function.instructions.push_back({op, imm}); });
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(errorLocal)});
            function.instructions.push_back({IrOpcode::PushI64, 0});
            function.instructions.push_back({IrOpcode::CmpEqI64, 0});
            size_t jumpError = function.instructions.size();
            function.instructions.push_back({IrOpcode::JumpIfZero, 0});
            if (ir_lowerer::usesInlineBufferResultErrorDiscriminator(resultInfo)) {
              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(resultLocal)});
            } else {
              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(resultLocal)});
              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(errorLocal)});
              function.instructions.push_back({IrOpcode::PushI64, 4294967296ull});
              function.instructions.push_back({IrOpcode::MulI64, 0});
              function.instructions.push_back({IrOpcode::SubI64, 0});
            }
            if (!resultInfo.valueStructType.empty()) {
              ir_lowerer::PackedResultStructPayloadInfo payloadInfo;
              if (!ir_lowerer::resolvePackedResultStructPayloadInfo(
                      resultInfo.valueStructType,
                      [&](const std::string &structPath, StructSlotLayoutInfo &layoutOut) {
                        return resolveStructSlotLayout(structPath, layoutOut);
                      },
                      payloadInfo)) {
                error = ir_lowerer::unsupportedPackedResultValueKindError("try");
                return false;
              }
              if (payloadInfo.isPackedSingleSlot) {
                const int32_t baseLocal = nextLocal;
                nextLocal += payloadInfo.slotCount;
                const int32_t ptrLocal = nextLocal++;
                function.instructions.push_back(
                    {IrOpcode::PushI32, static_cast<uint64_t>(static_cast<int32_t>(payloadInfo.slotCount - 1))});
                function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal)});
                function.instructions.push_back(
                    {IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal + payloadInfo.fieldOffset)});
                function.instructions.push_back({IrOpcode::AddressOfLocal, static_cast<uint64_t>(baseLocal)});
                function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(ptrLocal)});
                function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
              }
            }
            size_t jumpEnd = function.instructions.size();
            function.instructions.push_back({IrOpcode::Jump, 0});
            size_t errorIndex = function.instructions.size();
            function.instructions[jumpError].imm = static_cast<int32_t>(errorIndex);
            if (!emitOnErrorReturn(errorLocal)) {
              return false;
            }
            size_t endIndex = function.instructions.size();
            function.instructions[jumpEnd].imm = static_cast<int32_t>(endIndex);
            return true;
          }
          const int32_t errorLocal = allocTempLocal();
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(resultLocal)});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(errorLocal)});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(errorLocal)});
          function.instructions.push_back({IrOpcode::PushI64, 0});
          function.instructions.push_back({IrOpcode::CmpEqI64, 0});
          size_t jumpError = function.instructions.size();
          function.instructions.push_back({IrOpcode::JumpIfZero, 0});
          function.instructions.push_back({IrOpcode::PushI32, 0});
          size_t jumpEnd = function.instructions.size();
          function.instructions.push_back({IrOpcode::Jump, 0});
          size_t errorIndex = function.instructions.size();
          function.instructions[jumpError].imm = static_cast<int32_t>(errorIndex);
          if (!emitOnErrorReturn(errorLocal)) {
            return false;
          }
          size_t endIndex = function.instructions.size();
          function.instructions[jumpEnd].imm = static_cast<int32_t>(endIndex);
          return true;
        }
    return std::nullopt;
}

} // namespace primec::ir_lowerer
