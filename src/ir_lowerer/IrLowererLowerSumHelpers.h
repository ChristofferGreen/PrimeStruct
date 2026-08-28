#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "IrLowererCallHelpers.h"
#include "IrLowererLowerReturnEmitStage.h"
#include "IrLowererStructTypeHelpers.h"
#include "primec/ast/Ast.h"
#include "primec/frontend/SemanticProduct.h"

namespace primec::ir_lowerer {

using StructSlotLayout = StructSlotLayoutInfo;

struct LoweredSumVariantSelection {
  const Definition *sumDef = nullptr;
  const SumVariant *variant = nullptr;
  const Expr *payloadExpr = nullptr;
  LocalInfo::ValueKind payloadKind = LocalInfo::ValueKind::Unknown;
  std::string payloadStructPath;
  int32_t payloadSlotCount = 1;
  bool payloadIsAggregate = false;
};

struct LoweredSumPayloadStorageInfo {
  LocalInfo::ValueKind valueKind = LocalInfo::ValueKind::Unknown;
  std::string structPath;
  int32_t slotCount = 1;
  bool isAggregate = false;
  // TODO-5248: Pointer<T>/Reference<T> payloads (e.g. Maybe<Pointer<T>>)
  // store as a single Int64 address slot (valueKind above), but reading
  // the payload back out (pick binding) needs the pointee element type
  // to reconstruct a proper Kind::Pointer local, not just a raw Int64.
  bool isPointerLike = false;
  std::string pointerElementTypeText;
};

enum class LoweredSumPickEmitResult {
  NotMatched,
  Emitted,
  Error,
};

class SumHelpersContext {
public:
  SumHelpersContext(LowerSetupStageState &setupStage,
                    LowerReturnEmitStageState &stateOut,
                    const CallResolutionAdapters &callResolutionAdapters,
                    std::string &error);

  bool isLowerableSumDefinition(const Definition &def);
  const SumVariant * findSumVariantByName(const Definition &sumDef, const std::string &variantName);
  bool isStdlibResultSumDefinition(const Definition &sumDef);
  bool isLegacyResultOkCall(const Expr &expr);
  std::string stripGeneratedResultHelperSuffix(std::string helperPath);
  bool isStdlibResultVariantHelperCall(const Expr &expr, const std::string &variantName);
  unsigned stdlibResultVariantHelperPayloadIndex(const Expr &expr);
  bool isLegacyResultMapCall(const Expr &expr);
  bool isLegacyResultAndThenCall(const Expr &expr);
  bool isLegacyResultMap2Call(const Expr &expr);
  std::string sumPayloadTypeText(const SumVariant &variant);
  bool splitPointerLikePayloadTypeText(const std::string &typeText, std::string &elementTypeTextOut);
  LocalInfo::ValueKind pointerLikePayloadValueKind(const std::string &typeText);
  LocalInfo::ValueKind valueKindOrPointerLikeFromTypeName(const std::string &typeText);
  void applyStdlibResultSumInfoToLocal(const Definition &sumDef, LocalInfo &info);
  bool resolveSumPayloadStorageInfo(const Definition &sumDef, const SumVariant &variant, LoweredSumPayloadStorageInfo &infoOut);
  bool resolvePublishedSumPayloadStorageInfo(const Definition &sumDef, const SemanticProgramSumVariantMetadata &publishedVariant, LoweredSumPayloadStorageInfo &infoOut);
  const Definition * resolveSumDefinitionByPath(const std::string &path);
  const Definition * resolveSumDefinitionForTypeText(const std::string &typeText, const std::string &namespacePrefix);
  const Definition * resolveSumDefinitionForLocalInfo(const LocalInfo &info);
  std::string unsupportedSumPayloadError(const Definition &sumDef, const SumVariant &variant);
  bool resolveSemanticProductSumVariantMetadata(const Definition &sumDef, const SumVariant &variant, std::string_view operationLabel, const SemanticProgramSumVariantMetadata *&publishedVariantOut);
  bool resolveSemanticProductSumVariantTag(const Definition &sumDef, const SumVariant &variant, std::string_view operationLabel, int32_t &tagValueOut);
  bool resolveSemanticProductSumPayloadStorageInfo(const Definition &sumDef, const SumVariant &variant, std::string_view operationLabel, LoweredSumPayloadStorageInfo &infoOut);
  bool loweredSumSlotCount(const Definition &sumDef, int32_t &totalSlotsOut);
  const SumVariant * defaultUnitVariant(const Definition &sumDef);
  const SumVariant * unitVariantForNameExpr(const Definition &sumDef, const Expr &expr);
  const SumVariant * unitVariantForConstructorArg(const Definition &sumDef, const Expr &arg);
  const SumVariant * payloadVariantForConstructorArg(const Definition &sumDef, const Expr &arg);
  const SumVariant * firstUnsupportedSumPayloadVariant(const Definition &sumDef);
  bool selectExplicitSumVariantForConstructor(const Expr &initializer, const Definition &targetSum, LoweredSumVariantSelection &selectionOut);
  bool selectSumVariantForInitializer(const Expr &initializer, const Definition &targetSum, const LocalMap &valueLocals, LoweredSumVariantSelection &selectionOut);
  void emitLoweredSumHeader(int32_t baseLocal, int32_t totalSlots);
  bool emitLoweredSumConstructionIntoLocal(int32_t baseLocal, const Definition &sumDef, const Expr &initializer, const LocalMap &valueLocals);
  bool tryEmitLoweredSumConstructorExpr(const Expr &expr, const LocalMap &valueLocals);
  std::string describePickTargetName(const Expr &targetExpr);
  const Definition * resolveSemanticProductPickTargetSumDefinition(const Expr &targetExpr, const LocalMap &valueLocals);
  const Definition * resolvePickTargetSumDefinition(const Expr &targetExpr, const LocalMap &valueLocals);
  void emitLoadSumSlotIndirect(int32_t sumPtrLocal, int32_t slotOffset);
  void emitSumTagComparison(int32_t sumPtrLocal, int32_t tagValue);
  const Definition * findSumPayloadMoveHelper(const std::string &structPath);
  const Definition * findSumPayloadDestroyHelper(const std::string &structPath);
  bool emitActiveSumPayloadDestroyFromSumPtr(const Definition &sumDef, int32_t sourceSumPtrLocal, const LocalMap &valueLocals);
  bool emitActiveSumPayloadMoveFromSumPtr(int32_t destBaseLocal, const Definition &sumDef, int32_t sourceSumPtrLocal, const LocalMap &valueLocals);
  bool tryEmitLoweredSumMoveIntoLocal(int32_t baseLocal, const Definition &sumDef, const Expr &initializer, const LocalMap &valueLocals, bool &emittedOut);
  bool isMutableLocalExpr(const Expr &candidate, const LocalMap &localsIn);
  bool makePickPayloadLocalInfo(const Definition &sumDef, const SumVariant &variant, LocalInfo &payloadInfoOut);
  bool bindPickPayload(const Definition &sumDef, const SumVariant &variant, const Expr &binderExpr, int32_t sumPtrLocal, LocalMap &branchLocals, bool isMutable);
  bool isPickCall(const Expr &candidate);
  bool isPickArmEnvelopeBase(const Expr &candidate);
  bool isPayloadPickArmEnvelope(const Expr &candidate);
  bool isUnitCallPickArmEnvelope(const Expr &candidate);
  bool isUnitBindingPickArmEnvelope(const Expr &candidate);
  bool isSupportedPickArmEnvelope(const Expr &candidate);
  std::vector<const Expr *> pickArmBodyExprs(const Expr &arm);
  const Expr * findPickArmValueExpr(const Expr &arm);
  const SumVariant * resolvePickArmVariant(const Definition &sumDef, const Expr &arm);
  bool emitPickArmPrefixStatements(const Expr &arm, const Expr *valueExpr, LocalMap &branchLocals);
  std::optional<bool> resolveSemanticProductPickAggregateResultStructPath(const Expr &valueExpr, const Definition &sumDef, const SumVariant &variant, std::string &structPathOut);
  bool inferPickAggregateResult(const Expr &expr, const Definition &sumDef, const LocalMap &valueLocals, std::string &structPathOut, StructSlotLayout &layoutOut);
  LoweredSumPickEmitResult tryEmitPickExpr(const Expr &expr, const LocalMap &valueLocals);
  LoweredSumPickEmitResult tryEmitPickStatement(const Expr &stmt, LocalMap &localsIn);

private:
  const CallResolutionAdapters &callResolutionAdapters;
  std::string &error;
  IrFunction &function;
  int32_t &nextLocal;
  std::unordered_map<std::string, const Definition *> &defMap;
  const std::function<bool(const Expr &, const LocalMap &)> &emitExpr;
  const std::function<bool(const Expr &, LocalMap &)> &emitStatement;
  const std::function<int32_t()> &allocTempLocal;
  const ResolveDefinitionCallFn &resolveDefinitionCall;
  const std::function<bool(const std::string &, const std::string &, std::string &)> &resolveStructTypeName;
  const std::function<bool(const std::string &, StructSlotLayout &)> &resolveStructSlotLayout;
  const ResolveExprPathFn &resolveExprPath;
  const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind;
  const std::function<std::string(const Expr &, const LocalMap &)> &inferStructExprPath;
  const std::function<bool(int32_t, int32_t, int32_t)> &emitStructCopyFromPtrs;
  const std::function<bool(const Expr &, const Definition &, const LocalMap &, bool)> &emitInlineDefinitionCall;
};

} // namespace primec::ir_lowerer
