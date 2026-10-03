#include "IrLowererCountAccessHelpers.h"
#include "IrLowererCountAccessClassifiers.h"

#include <algorithm>
#include <cctype>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "IrLowererBindingTypeHelpers.h"
#include "IrLowererBindingTransformHelpers.h"
#include "IrLowererHelpers.h"
#include "IrLowererSemanticProductTargetAdapters.h"
#include "IrLowererSetupTypeCollectionHelpers.h"
#include "IrLowererSetupTypeHelpers.h"
#include "IrLowererTemplateTypeParseHelpers.h"
#include "primec/frontend/SemanticProduct.h"
#include "primec/ir/SoaPathHelpers.h"
#include "primec/support/StdlibSurfaceRegistry.h"
#include "primec/ir/StdlibCollectionPaths.h"
#include "primec/support/CollectionHelperNames.h"
#include "IrLowererCountAccessInternal.h"

namespace primec::ir_lowerer {
using namespace count_access_internal;

CountAccessCallEmitResult tryEmitCountAccessCall(
    const Expr &expr,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const LocalMap &)> &isArrayCountCallFn,
    const std::function<bool(const Expr &, const LocalMap &)> &isVectorCapacityCallFn,
    const std::function<bool(const Expr &, const LocalMap &)> &isStringCountCallFn,
    const std::function<bool(const Expr &, const LocalMap &)> &isEntryArgsNameFn,
    const std::function<bool(const Expr &, const LocalMap &)> &isDynamicCollectionCountTargetFn,
    const std::function<bool(const Expr &, const LocalMap &)> &isDynamicVectorCountTargetFn,
    const std::function<bool(const Expr &, const LocalMap &)> &isDynamicVectorCapacityTargetFn,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    std::string &error,
    const SemanticProgram *semanticProgram,
    const SemanticProductIndex *semanticIndex) {
  const std::string scopedCallPath = resolveScopedCallPath(expr);
  const std::string semanticMethodTarget =
      expr.isMethodCall
          ? findSemanticProductMethodCallTarget(semanticProgram, expr)
          : std::string{};
  const std::string semanticDirectTarget =
      !expr.isMethodCall
          ? findSemanticProductDirectCallTarget(semanticProgram, expr)
          : std::string{};
  const bool canonicalSoaColumnCount =
      scopedCallPath == collection_helpers::kCanonicalSoaStorageSoaColumnCount ||
      scopedCallPath.rfind(
          collection_helpers::kCanonicalSoaStorageSoaColumnCountSpecialized, 0) == 0 ||
      semanticDirectTarget ==
          collection_helpers::kCanonicalSoaStorageSoaColumnCount;
  const bool canonicalSoaMethodCount =
      expr.isMethodCall && expr.args.size() == 1 &&
      resolveCallLeafName(expr) == "count" &&
      expr.args.front().kind == Expr::Kind::Name &&
      [&]() {
        auto localIt = localsIn.find(expr.args.front().name);
        return localIt != localsIn.end() && localIt->second.isSoaVector;
      }();
  if (scopedCallPath == collection_helpers::kCanonicalSoaCount ||
      scopedCallPath.rfind(collection_helpers::kCanonicalSoaCountSpecialized, 0) == 0 ||
      scopedCallPath == "std/collections/soa/count" ||
      scopedCallPath.rfind("std/collections/soa/count__", 0) == 0 ||
      semanticMethodTarget == collection_helpers::kCanonicalSoaCount ||
      semanticDirectTarget == collection_helpers::kCanonicalSoaCount ||
      canonicalSoaColumnCount || canonicalSoaMethodCount) {
    if (expr.args.size() != 1) {
      error = "count requires exactly one argument";
      return CountAccessCallEmitResult::Error;
    }
    const Expr &target = expr.args.front();
    if (target.kind == Expr::Kind::Name) {
      auto localIt = localsIn.find(target.name);
      if (localIt != localsIn.end()) {
        if (localIt->second.isArgsPack) {
          emitInstruction(IrOpcode::LoadLocal,
                          static_cast<uint64_t>(localIt->second.index));
          emitInstruction(IrOpcode::LoadIndirect, 0);
          return CountAccessCallEmitResult::Emitted;
        }
        emitInstruction(IrOpcode::LoadLocal,
                        static_cast<uint64_t>(localIt->second.index));
      } else if (!emitExpr(target, localsIn)) {
        return CountAccessCallEmitResult::Error;
      }
    } else if (!emitExpr(target, localsIn)) {
      return CountAccessCallEmitResult::Error;
    }
    emitInstruction(IrOpcode::PushI64,
                    (canonicalSoaColumnCount ? 1ull : 2ull) * IrSlotBytes);
    emitInstruction(IrOpcode::AddI64, 0);
    emitInstruction(IrOpcode::LoadIndirect, 0);
    return CountAccessCallEmitResult::Emitted;
  }
  // TODO-5256: reject count() over an "at"-shaped call whose resolved return
  // type is not a string. The resolved semantic product already records the
  // override's actual result type, so this check works no matter which
  // dispatch path later handles the call; without it the raw scalar was
  // dereferenced as a string handle at runtime ("unaligned indirect address").
  if ((count_access_detail::isUnqualifiedCollectionBuiltinName(expr, "count") ||
       isSimpleCallName(expr, "count")) &&
      expr.args.size() == 1) {
    const Expr &guardedCountTarget = expr.args.front();
    std::string guardedAccessName;
    if (guardedCountTarget.kind == Expr::Kind::Call &&
        guardedCountTarget.args.size() == 2 &&
        getBuiltinArrayAccessName(guardedCountTarget, guardedAccessName) &&
        (guardedAccessName == "at" || guardedAccessName == "at_unsafe") &&
        classifySemanticStringCountTarget(guardedCountTarget, semanticProgram,
                                          semanticIndex) ==
            SemanticStringCountTargetResolution::NonString) {
      error = "count() argument resolves to a non-string value";
      return CountAccessCallEmitResult::Error;
    }
  }
  const bool explicitPublishedVectorCountCall =
      isExplicitPublishedVectorMetadataCall(expr, "count");
  const bool explicitPublishedKeyValueCountCall =
      isExplicitPublishedKeyValueMetadataCall(expr, semanticProgram, "count");
  const bool semanticVectorCountBridge =
      isSemanticVectorCountBridgeCall(expr, semanticProgram);
  const bool semanticArrayCountMethod =
      isSemanticArrayCountMethodTarget(expr, semanticProgram);
  const bool explicitPublishedVectorCapacityCall =
      isExplicitPublishedVectorMetadataCall(expr, "capacity");
  const auto resolvedVectorCountMethodHasBuiltinTarget = [&]() {
    if (!isResolvedVectorCountMethodCall(expr, semanticProgram) ||
        expr.args.size() != 1) {
      return false;
    }
    const Expr &target = expr.args.front();
    if (isDynamicVectorCountTargetFn != nullptr &&
        isDynamicVectorCountTargetFn(target, localsIn)) {
      return true;
    }
    if (isVectorCountTarget(target, localsIn)) {
      return true;
    }
    SemanticCountTargetInfo semanticTargetInfo;
    if (semanticProgram != nullptr && semanticIndex != nullptr &&
        target.semanticNodeId != 0 &&
        classifySemanticCountTarget(target,
                                    semanticProgram,
                                    semanticIndex,
                                    semanticTargetInfo) &&
        semanticTargetInfo.isVector) {
      return true;
    }
    const std::string receiverTypeText =
        semanticMethodReceiverTypeText(semanticProgram, expr);
    return !receiverTypeText.empty() &&
           classifySemanticCountTargetTypeText(receiverTypeText,
                                               semanticTargetInfo) &&
           semanticTargetInfo.isVector;
  };
  const bool resolvedVectorCountMethodBuiltinTarget =
      resolvedVectorCountMethodHasBuiltinTarget();
  const auto isCountLikeCall = [&]() {
    return (count_access_detail::isUnqualifiedCollectionBuiltinName(expr, "count") ||
            explicitPublishedVectorCountCall ||
            explicitPublishedKeyValueCountCall ||
            semanticArrayCountMethod ||
            resolvedVectorCountMethodBuiltinTarget) &&
           expr.args.size() == 1;
  };
  const auto isBareSimpleCountLikeCall = [&](std::string_view helperName) {
    return expr.kind == Expr::Kind::Call &&
           !expr.isMethodCall &&
           expr.namespacePrefix.empty() &&
           expr.name == std::string(helperName) &&
           expr.args.size() == 1;
  };
  if (isCountLikeCall()) {
    const std::string receiverTypeText =
        semanticMethodReceiverTypeText(semanticProgram, expr);
    if (!receiverTypeText.empty()) {
      SemanticCountTargetInfo receiverInfo;
      if (classifySemanticCountTargetTypeText(receiverTypeText,
                                              receiverInfo) &&
          !receiverInfo.isString &&
          !receiverInfo.isCollection) {
        error = "native backend only supports entry argument indexing";
        return CountAccessCallEmitResult::Error;
      }
    }
  }
  const auto shouldDeferBareSimpleCountLikeCall = [&](std::string_view helperName) {
    if (!isBareSimpleCountLikeCall(helperName)) {
      return false;
    }
    const Expr &target = expr.args.front();
    if (helperName == "count") {
      const bool isRuntimeStringTarget =
          inferExprKind &&
          inferExprKind(target, localsIn) == LocalInfo::ValueKind::String;
      if (isArrayCountCallFn(expr, localsIn) ||
          isRuntimeStringTarget ||
          (isStringCountCallFn != nullptr &&
           isStringCountCallFn(expr, localsIn)) ||
          (isDynamicCollectionCountTargetFn != nullptr &&
           isDynamicCollectionCountTargetFn(target, localsIn)) ||
          (isDynamicVectorCountTargetFn != nullptr &&
           isDynamicVectorCountTargetFn(target, localsIn))) {
        return false;
      }
      std::string accessName;
      if (target.kind == Expr::Kind::Call &&
          target.args.size() == 2 &&
          getBuiltinArrayAccessName(target, accessName) &&
          (accessName == "at" || accessName == "at_unsafe")) {
        return false;
      }
      return true;
    }
    if ((isVectorCapacityCallFn != nullptr &&
         isVectorCapacityCallFn(expr, localsIn)) ||
        (isDynamicVectorCapacityTargetFn != nullptr &&
         isDynamicVectorCapacityTargetFn(target, localsIn))) {
      return false;
    }
    return true;
  };
  if (shouldDeferBareSimpleCountLikeCall("count") ||
      shouldDeferBareSimpleCountLikeCall("capacity")) {
    return CountAccessCallEmitResult::NotHandled;
  }
  if (isResolvedVectorCountMethodCall(expr, semanticProgram) &&
      !semanticArrayCountMethod &&
      !resolvedVectorCountMethodBuiltinTarget) {
    return CountAccessCallEmitResult::NotHandled;
  }
  const bool namedArgVectorTemporaryCountCall =
      explicitPublishedVectorCountCall &&
      expr.args.size() == 1 &&
      isNamedArgumentCollectionTemporary(expr.args.front(), "vector");
  if (explicitPublishedVectorCountCall &&
      expr.args.size() == 1 &&
      isEntryArgsNameFn &&
      isEntryArgsNameFn(expr.args.front(), localsIn)) {
    emitInstruction(IrOpcode::PushArgc, 0);
    return CountAccessCallEmitResult::Emitted;
  }
  const bool semanticBridgeCountAccessTarget =
      semanticVectorCountBridge &&
      expr.args.size() == 1 &&
      (isArrayCountCallFn(expr, localsIn) ||
       (isDynamicCollectionCountTargetFn != nullptr &&
        isDynamicCollectionCountTargetFn(expr.args.front(), localsIn)) ||
       (isDynamicVectorCountTargetFn != nullptr &&
        isDynamicVectorCountTargetFn(expr.args.front(), localsIn)));
  const bool explicitPublishedKeyValueCountAccessTarget =
      explicitPublishedKeyValueCountCall &&
      expr.args.size() == 1 &&
      isDynamicCollectionCountTargetFn != nullptr &&
      isDynamicCollectionCountTargetFn(expr.args.front(), localsIn);
  const bool explicitPublishedVectorCountAccessTarget =
      explicitPublishedVectorCountCall &&
      expr.args.size() == 1 &&
      ((isDynamicCollectionCountTargetFn != nullptr &&
        isDynamicCollectionCountTargetFn(expr.args.front(), localsIn)) ||
       (isDynamicVectorCountTargetFn != nullptr &&
        isDynamicVectorCountTargetFn(expr.args.front(), localsIn)));
  if (explicitPublishedVectorCountCall &&
      !namedArgVectorTemporaryCountCall &&
      !semanticBridgeCountAccessTarget &&
      !explicitPublishedVectorCountAccessTarget) {
    return CountAccessCallEmitResult::NotHandled;
  }
  const bool explicitPublishedVectorCapacityAccessTarget =
      explicitPublishedVectorCapacityCall &&
      expr.args.size() == 1 &&
      isDynamicVectorCapacityTargetFn != nullptr &&
      isDynamicVectorCapacityTargetFn(expr.args.front(), localsIn);
  if (explicitPublishedVectorCapacityCall &&
      !explicitPublishedVectorCapacityAccessTarget) {
    return CountAccessCallEmitResult::NotHandled;
  }
  if ((isExplicitRemovedCountLikeAliasCall(expr, "count") &&
       !explicitPublishedKeyValueCountAccessTarget) ||
      isExplicitRemovedCountLikeAliasCall(expr, "capacity")) {
    return CountAccessCallEmitResult::NotHandled;
  }
  const auto emitDynamicVectorHeaderBase = [&](const Expr &target) {
    if (target.kind == Expr::Kind::Name) {
      auto it = localsIn.find(target.name);
      if (it != localsIn.end() && isCollectionVectorStructValueLocal(it->second)) {
        emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index));
        return true;
      }
    }
    return emitExpr(target, localsIn);
  };
  const auto emitDynamicVectorCount = [&](const Expr &target) {
    if (target.kind == Expr::Kind::Name) {
      auto it = localsIn.find(target.name);
      if (it != localsIn.end() && !it->second.isArgsPack) {
        if (isExperimentalSoaVectorStructLocal(it->second) ||
            (it->second.isSoaVector && !it->second.usesBuiltinCollectionLayout)) {
          emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index));
          emitInstruction(IrOpcode::PushI64, 2ull * IrSlotBytes);
          emitInstruction(IrOpcode::AddI64, 0);
          emitInstruction(IrOpcode::LoadIndirect, 0);
          return true;
        }
        // Canonical vector: count (fieldCount) is at slot 1 after the implicit type tag at slot 0.
        if (it->second.kind == LocalInfo::Kind::Vector && !it->second.isSoaVector) {
          emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index));
          emitInstruction(IrOpcode::PushI64, IrSlotBytes);
          emitInstruction(IrOpcode::AddI64, 0);
          emitInstruction(IrOpcode::LoadIndirect, 0);
          return true;
        }
        const std::string canonicalVecPath = vectorBackingTypePath();
        if (it->second.kind == LocalInfo::Kind::Value &&
            (it->second.structTypeName == canonicalVecPath ||
             it->second.structTypeName.rfind(canonicalVecPath + "__", 0) == 0)) {
          emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index));
          emitInstruction(IrOpcode::PushI64, IrSlotBytes);
          emitInstruction(IrOpcode::AddI64, 0);
          emitInstruction(IrOpcode::LoadIndirect, 0);
          return true;
        }
      }
    }
    if (!emitDynamicVectorHeaderBase(target)) {
      return false;
    }
    emitInstruction(IrOpcode::LoadIndirect, 0);
    return true;
  };
  const auto emitDynamicVectorCapacity = [&](const Expr &target) {
    if (target.kind == Expr::Kind::Name) {
      auto it = localsIn.find(target.name);
      if (it != localsIn.end() && !it->second.isArgsPack) {
        // Canonical vector: capacity (fieldCapacity) is at slot 2 after type tag (0) and count (1).
        const std::string canonicalVecPath = vectorBackingTypePath();
        const bool isCanonicalVectorLocal =
            (it->second.kind == LocalInfo::Kind::Vector && !it->second.isSoaVector) ||
            (it->second.kind == LocalInfo::Kind::Value &&
             (it->second.structTypeName == canonicalVecPath ||
              it->second.structTypeName.rfind(canonicalVecPath + "__", 0) == 0));
        if (isCanonicalVectorLocal) {
          emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index));
          emitInstruction(IrOpcode::PushI64, 2ull * IrSlotBytes);
          emitInstruction(IrOpcode::AddI64, 0);
          emitInstruction(IrOpcode::LoadIndirect, 0);
          return true;
        }
      }
    }
    if (!emitDynamicVectorHeaderBase(target)) {
      return false;
    }
    emitInstruction(IrOpcode::PushI64, IrSlotBytes);
    emitInstruction(IrOpcode::AddI64, 0);
    emitInstruction(IrOpcode::LoadIndirect, 0);
    return true;
  };
  if (isInternalVectorMetadataCall(expr, "count") &&
      isDynamicVectorCountTargetFn != nullptr &&
      isDynamicVectorCountTargetFn(expr.args.front(), localsIn)) {
    if (!emitDynamicVectorCount(expr.args.front())) {
      return CountAccessCallEmitResult::Error;
    }
    return CountAccessCallEmitResult::Emitted;
  }
  if (isInternalVectorMetadataCall(expr, "capacity") &&
      isDynamicVectorCapacityTargetFn != nullptr &&
      isDynamicVectorCapacityTargetFn(expr.args.front(), localsIn)) {
    if (!emitDynamicVectorCapacity(expr.args.front())) {
      return CountAccessCallEmitResult::Error;
    }
    return CountAccessCallEmitResult::Emitted;
  }
  if (isInternalSoaStorageMetadataCall(expr, "field_count") &&
      isInternalSoaStorageMetadataTarget(expr.args.front(), localsIn)) {
    if (!emitInternalSoaStorageMetadataBase(
            expr.args.front(), localsIn, emitInstruction, emitExpr)) {
      return CountAccessCallEmitResult::Error;
    }
    emitInternalSoaStorageMetadataLoad("field_count", emitInstruction);
    return CountAccessCallEmitResult::Emitted;
  }
  if (isInternalSoaStorageMetadataCall(expr, "field_capacity") &&
      isInternalSoaStorageMetadataTarget(expr.args.front(), localsIn)) {
    if (!emitInternalSoaStorageMetadataBase(
            expr.args.front(), localsIn, emitInstruction, emitExpr)) {
      return CountAccessCallEmitResult::Error;
    }
    emitInternalSoaStorageMetadataLoad("field_capacity", emitInstruction);
    return CountAccessCallEmitResult::Emitted;
  }
  const bool namedArgVectorTemporaryCountTarget =
      (count_access_detail::isUnqualifiedCollectionBuiltinName(expr, "count") ||
       explicitPublishedVectorCountCall ||
       explicitPublishedKeyValueCountCall) &&
      expr.args.size() == 1 &&
      isNamedArgumentCollectionTemporary(expr.args.front(), "vector");
  if (namedArgVectorTemporaryCountTarget) {
    error = "count requires array, vector, map, or string target";
    return CountAccessCallEmitResult::Error;
  }
  const bool isFieldAccessTarget =
      expr.kind == Expr::Kind::Call && expr.args.size() == 1 &&
      expr.args.front().kind == Expr::Kind::Call && expr.args.front().isFieldAccess;
  const auto isStaticallyKnownStringCountTarget = [&]() {
    if (!isCountLikeCall()) {
      return false;
    }
    const Expr &target = expr.args.front();
    if (target.kind == Expr::Kind::StringLiteral) {
      return true;
    }
    if (target.kind != Expr::Kind::Name) {
      return false;
    }
    auto it = localsIn.find(target.name);
    return it != localsIn.end() &&
           it->second.valueKind == LocalInfo::ValueKind::String &&
           it->second.stringSource == LocalInfo::StringSource::TableIndex;
  };
  const auto isRuntimeStringCountTarget = [&]() {
    if (!isCountLikeCall() || isStaticallyKnownStringCountTarget()) {
      return false;
    }
    const std::string receiverTypeText =
        semanticMethodReceiverTypeText(semanticProgram, expr);
    if (!receiverTypeText.empty()) {
      SemanticCountTargetInfo receiverInfo;
      if (classifySemanticCountTargetTypeText(receiverTypeText,
                                              receiverInfo)) {
        if (receiverInfo.isString) {
          return true;
        }
        return false;
      }
    }
    const Expr &target = expr.args.front();
    const SemanticStringCountTargetResolution semanticStringTarget =
        classifySemanticStringCountTarget(target, semanticProgram, semanticIndex);
    if (semanticStringTarget == SemanticStringCountTargetResolution::String) {
      return true;
    }
    if (semanticStringTarget == SemanticStringCountTargetResolution::NonString) {
      return false;
    }
    const auto semanticStringKeyValueAccess =
        classifySemanticStringKeyValueAccess(target, semanticProgram, semanticIndex);
    if (semanticStringKeyValueAccess ==
        SemanticStringKeyValueAccessResolution::StringKeyValueAccess) {
      return true;
    }
    if (semanticStringKeyValueAccess ==
        SemanticStringKeyValueAccessResolution::NonStringKeyValueAccess) {
      return false;
    }
    if (isSourceMethodStringKeyValueAccessTarget(target, semanticProgram, semanticIndex)) {
      return true;
    }
    if (inferExprKind) {
      const LocalInfo::ValueKind targetKind = inferExprKind(target, localsIn);
      if (targetKind == LocalInfo::ValueKind::String) {
        return true;
      }
      if (targetKind != LocalInfo::ValueKind::Unknown) {
        return false;
      }
    }
    if (target.kind == Expr::Kind::Name) {
      auto it = localsIn.find(target.name);
      if (it != localsIn.end() && it->second.valueKind == LocalInfo::ValueKind::String) {
        return true;
      }
    }
    return false;
  };
  const auto emitRuntimeStringCountTarget = [&]() {
    Expr rewrittenStringTarget = expr.args.front();
    std::string explicitKeyValueAccessName;
    if (hasExplicitStdKeyValueSourceSpelling(rewrittenStringTarget) &&
        getBuiltinArrayAccessName(rewrittenStringTarget, explicitKeyValueAccessName) &&
        (explicitKeyValueAccessName == "at" ||
         explicitKeyValueAccessName == "at_unsafe")) {
      rewrittenStringTarget.name = explicitKeyValueAccessName;
      rewrittenStringTarget.namespacePrefix.clear();
      rewrittenStringTarget.isMethodCall = false;
      rewrittenStringTarget.isFieldAccess = false;
      rewrittenStringTarget.semanticNodeId = 0;
      rewrittenStringTarget.templateArgs.clear();
    } else {
      std::string sourceMethodAccessName;
      if (isSourceMethodStringKeyValueAccessTarget(
              rewrittenStringTarget, semanticProgram, semanticIndex,
              &sourceMethodAccessName)) {
        rewrittenStringTarget.name = sourceMethodAccessName;
        rewrittenStringTarget.namespacePrefix.clear();
        rewrittenStringTarget.isMethodCall = true;
        rewrittenStringTarget.isFieldAccess = false;
        rewrittenStringTarget.semanticNodeId = 0;
        rewrittenStringTarget.templateArgs.clear();
      }
    }
    if (!emitExpr(rewrittenStringTarget, localsIn)) {
      return CountAccessCallEmitResult::Error;
    }
    emitInstruction(IrOpcode::LoadStringLength, 0);
    return CountAccessCallEmitResult::Emitted;
  };
  if (isCountLikeCall() && expr.args.front().kind == Expr::Kind::Call) {
    const Expr &target = expr.args.front();
    std::string accessName;
    if (target.name.find('/') == std::string::npos &&
        !hasExplicitStdKeyValueSourceSpelling(target) &&
        getBuiltinArrayAccessName(target, accessName) &&
        ([&]() {
          const auto semanticStringKeyValueAccess =
              classifySemanticStringKeyValueAccess(target, semanticProgram, semanticIndex);
          if (semanticStringKeyValueAccess ==
              SemanticStringKeyValueAccessResolution::StringKeyValueAccess) {
            return true;
          }
          if (semanticStringKeyValueAccess ==
              SemanticStringKeyValueAccessResolution::NonStringKeyValueAccess) {
            return false;
          }
          return isSourceMethodStringKeyValueAccessTarget(target, semanticProgram, semanticIndex);
        }() ||
         (publishedKeyValueAccessHelperReturnsString(semanticProgram, accessName) &&
          !target.args.empty() &&
          !isVectorCountTarget(target.args.front(), localsIn)))) {
      if (expr.isMethodCall) {
        return CountAccessCallEmitResult::NotHandled;
      }
      return emitRuntimeStringCountTarget();
    }
  }
  if (isRuntimeStringCountTarget()) {
    if (expr.isMethodCall && expr.args.front().kind == Expr::Kind::Call) {
      const auto semanticStringKeyValueAccess =
          classifySemanticStringKeyValueAccess(
              expr.args.front(), semanticProgram, semanticIndex);
      if (semanticStringKeyValueAccess ==
          SemanticStringKeyValueAccessResolution::StringKeyValueAccess) {
        return CountAccessCallEmitResult::NotHandled;
      }
    }
    return emitRuntimeStringCountTarget();
  }
  if (isArrayCountCallFn(expr, localsIn)) {
    if (count_access_detail::isUnqualifiedCollectionBuiltinName(expr, "count") &&
        expr.args.size() == 1 &&
        expr.args.front().kind == Expr::Kind::Name) {
      auto it = localsIn.find(expr.args.front().name);
      if (it != localsIn.end() &&
          !it->second.isArgsPack &&
          (isExperimentalSoaVectorStructLocal(it->second) ||
           (it->second.isSoaVector && !it->second.usesBuiltinCollectionLayout))) {
        emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index));
        emitInstruction(IrOpcode::PushI64, 2ull * IrSlotBytes);
        emitInstruction(IrOpcode::AddI64, 0);
        emitInstruction(IrOpcode::LoadIndirect, 0);
        return CountAccessCallEmitResult::Emitted;
      }
      if (it != localsIn.end() && it->second.isArgsPack) {
        emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index));
        emitInstruction(IrOpcode::LoadIndirect, 0);
        return CountAccessCallEmitResult::Emitted;
      }
      // Canonical vector: count (fieldCount) is at slot 1, not slot 0.
      if (it != localsIn.end() && !it->second.isArgsPack &&
          it->second.kind == LocalInfo::Kind::Vector && !it->second.isSoaVector) {
        emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index));
        emitInstruction(IrOpcode::PushI64, IrSlotBytes);
        emitInstruction(IrOpcode::AddI64, 0);
        emitInstruction(IrOpcode::LoadIndirect, 0);
        return CountAccessCallEmitResult::Emitted;
      }
      const std::string canonicalVecPath = vectorBackingTypePath();
      if (it != localsIn.end() && !it->second.isArgsPack &&
          it->second.kind == LocalInfo::Kind::Value &&
          (it->second.structTypeName == canonicalVecPath ||
           it->second.structTypeName.rfind(canonicalVecPath + "__", 0) == 0)) {
        emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index));
        emitInstruction(IrOpcode::PushI64, IrSlotBytes);
        emitInstruction(IrOpcode::AddI64, 0);
        emitInstruction(IrOpcode::LoadIndirect, 0);
        return CountAccessCallEmitResult::Emitted;
      }
    }
    if (isEntryArgsNameFn(expr.args.front(), localsIn)) {
      emitInstruction(IrOpcode::PushArgc, 0);
      return CountAccessCallEmitResult::Emitted;
    }
    // A fresh (unbound) vector<T>(...) literal target shares this generic
    // fallback with array literals, but the canonical vector record has an
    // implicit type-tag slot before fieldCount (see the Name-kind branches
    // above), so it needs the same slot-1 offset those branches apply -
    // without it this reads the always-zero tag slot instead of the count.
    std::string targetCollectionName;
    const bool isVectorLiteralCountTarget =
        expr.args.front().kind == Expr::Kind::Call &&
        getBuiltinCollectionName(expr.args.front(), targetCollectionName) &&
        targetCollectionName == "vector";
    if (!emitExpr(expr.args.front(), localsIn)) {
      return CountAccessCallEmitResult::Error;
    }
    if (isVectorLiteralCountTarget) {
      emitInstruction(IrOpcode::PushI64, IrSlotBytes);
      emitInstruction(IrOpcode::AddI64, 0);
    }
    emitInstruction(IrOpcode::LoadIndirect, 0);
    return CountAccessCallEmitResult::Emitted;
  }

  const bool blocksBareVectorCountCall =
      isBareSimpleCountLikeCall("count") && expr.args.front().kind != Expr::Kind::Call &&
      (isDynamicVectorCountTargetFn && isDynamicVectorCountTargetFn(expr.args.front(), localsIn) &&
       !isVectorCountTarget(expr.args.front(), localsIn) &&
       !isFieldAccessTarget);
  const bool blocksLocalVectorCountCall =
      isBareSimpleCountLikeCall("count") &&
      isVectorCountTarget(expr.args.front(), localsIn) &&
      !(isDynamicCollectionCountTargetFn &&
        isDynamicCollectionCountTargetFn(expr.args.front(), localsIn));
  const bool blocksBareVectorCapacityCall =
      isBareSimpleCountLikeCall("capacity") &&
      expr.args.front().kind != Expr::Kind::Call &&
      (isDynamicVectorCapacityTargetFn && isDynamicVectorCapacityTargetFn(expr.args.front(), localsIn) &&
       !(isVectorCapacityCallFn && isVectorCapacityCallFn(expr, localsIn)) &&
       !isFieldAccessTarget);
  const bool blocksLocalVectorCapacityCall =
      isBareSimpleCountLikeCall("capacity") &&
      isVectorCountTarget(expr.args.front(), localsIn) &&
      !(isDynamicVectorCapacityTargetFn &&
        isDynamicVectorCapacityTargetFn(expr.args.front(), localsIn));
  if (blocksBareVectorCountCall || blocksLocalVectorCountCall ||
      blocksBareVectorCapacityCall || blocksLocalVectorCapacityCall) {
    return CountAccessCallEmitResult::NotHandled;
  }

  if (isVectorCapacityCallFn(expr, localsIn)) {
    if (!emitDynamicVectorCapacity(expr.args.front())) {
      return CountAccessCallEmitResult::Error;
    }
    return CountAccessCallEmitResult::Emitted;
  }

  if (isCountLikeCall()) {
    const Expr &target = expr.args.front();
    std::string vectorAccessName;
    if (target.kind == Expr::Kind::Call && target.args.size() == 2 &&
        getBuiltinArrayAccessName(target, vectorAccessName) &&
        (vectorAccessName == "at" || vectorAccessName == "at_unsafe")) {
      const SemanticStringCountTargetResolution semanticStringTarget =
          classifySemanticStringCountTarget(target, semanticProgram,
                                            semanticIndex);
      // TODO-5256: an "at"-shaped argument only lowers as a string-character
      // count when the resolved access really yields a string handle. When
      // semantics classified it as anything else (e.g. a user override of
      // /std/collections/vector/at returning a scalar), reject at compile
      // time instead of letting later lowering dereference the raw value.
      if (semanticStringTarget == SemanticStringCountTargetResolution::NonString) {
        error = "count() argument resolves to a non-string value";
        return CountAccessCallEmitResult::Error;
      }
      if (semanticStringTarget == SemanticStringCountTargetResolution::String &&
          vectorAccessName == "at_unsafe") {
        return CountAccessCallEmitResult::NotHandled;
      }
    }
  }

  const bool dynamicCollectionCountTarget =
      expr.args.size() == 1 &&
      isDynamicCollectionCountTargetFn &&
      isDynamicCollectionCountTargetFn(expr.args.front(), localsIn);
  const bool dynamicVectorCountTarget =
      expr.args.size() == 1 &&
      ((isDynamicVectorCountTargetFn &&
        isDynamicVectorCountTargetFn(expr.args.front(), localsIn)) ||
       isVectorCountTarget(expr.args.front(), localsIn));
  if ((count_access_detail::isUnqualifiedCollectionBuiltinName(expr, "count") ||
       explicitPublishedKeyValueCountCall ||
       resolvedVectorCountMethodBuiltinTarget ||
       (explicitPublishedVectorCountCall && dynamicVectorCountTarget)) &&
      expr.args.size() == 1 &&
      (dynamicCollectionCountTarget ||
       (explicitPublishedVectorCountCall && dynamicVectorCountTarget))) {
    const SemanticStringCountTargetResolution semanticStringTarget =
        classifySemanticStringCountTarget(expr.args.front(),
                                          semanticProgram,
                                          semanticIndex);
    if (semanticStringTarget == SemanticStringCountTargetResolution::String ||
        (inferExprKind &&
         inferExprKind(expr.args.front(), localsIn) ==
             LocalInfo::ValueKind::String)) {
      // String receivers can be dynamic call results; defer to string count emission.
    } else {
      if (!emitDynamicVectorCount(expr.args.front())) {
        return CountAccessCallEmitResult::Error;
      }
      return CountAccessCallEmitResult::Emitted;
    }
  }

  if ((expr.namespacePrefix.empty() || explicitPublishedVectorCapacityCall) &&
      (count_access_detail::isUnqualifiedCollectionBuiltinName(expr, "capacity") || explicitPublishedVectorCapacityCall) &&
      expr.args.size() == 1 &&
      isDynamicVectorCapacityTargetFn && isDynamicVectorCapacityTargetFn(expr.args.front(), localsIn)) {
    if (!emitDynamicVectorCapacity(expr.args.front())) {
      return CountAccessCallEmitResult::Error;
    }
    return CountAccessCallEmitResult::Emitted;
  }

  const auto stringCountResult = tryEmitStringCountCall(
      expr,
      localsIn,
      isStringCountCallFn,
      inferExprKind,
      resolveStringTableTarget,
      [&](int32_t length) { emitInstruction(IrOpcode::PushI32, static_cast<uint64_t>(length)); },
      error);
  if (stringCountResult == StringCountCallEmitResult::Error) {
    return CountAccessCallEmitResult::Error;
  }
  if (stringCountResult == StringCountCallEmitResult::Emitted) {
    return CountAccessCallEmitResult::Emitted;
  }

  if ((count_access_detail::isUnqualifiedCollectionBuiltinName(expr, "count") ||
       explicitPublishedKeyValueCountCall) && expr.args.size() == 1 &&
      expr.args.front().kind == Expr::Kind::Call) {
    std::string accessName;
    const Expr &target = expr.args.front();
    if (getBuiltinArrayAccessName(target, accessName) && target.args.size() == 2) {
      const auto semanticStringKeyValueAccess =
          classifySemanticStringKeyValueAccess(target, semanticProgram, semanticIndex);
      if (semanticStringKeyValueAccess ==
          SemanticStringKeyValueAccessResolution::StringKeyValueAccess) {
        if (!emitExpr(target, localsIn)) {
          return CountAccessCallEmitResult::Error;
        }
        emitInstruction(IrOpcode::LoadStringLength, 0);
        return CountAccessCallEmitResult::Emitted;
      }
      if (semanticStringKeyValueAccess ==
          SemanticStringKeyValueAccessResolution::NonStringKeyValueAccess) {
        return CountAccessCallEmitResult::NotHandled;
      }
      if (inferExprKind) {
        const LocalInfo::ValueKind targetKind = inferExprKind(target, localsIn);
        if (targetKind == LocalInfo::ValueKind::String) {
          if (!emitExpr(target, localsIn)) {
            return CountAccessCallEmitResult::Error;
          }
          emitInstruction(IrOpcode::LoadStringLength, 0);
          return CountAccessCallEmitResult::Emitted;
        }
        if (targetKind != LocalInfo::ValueKind::Unknown) {
          return CountAccessCallEmitResult::NotHandled;
        }
      }
      const Expr &accessTarget = target.args.front();
      bool stringKeyValueAccess = false;
      if (accessTarget.kind == Expr::Kind::Name) {
        auto it = localsIn.find(accessTarget.name);
        if (it != localsIn.end()) {
          const LocalInfo &info = it->second;
          stringKeyValueAccess =
              info.keyValueKeyKind != LocalInfo::ValueKind::Unknown &&
              info.keyValueValueKind == LocalInfo::ValueKind::String;
        }
      } else if (accessTarget.kind == Expr::Kind::Call) {
        std::string collection;
        if (getBuiltinCollectionName(accessTarget, collection) && collection == "map" &&
            accessTarget.templateArgs.size() == 2) {
          stringKeyValueAccess =
              accessTarget.templateArgs[1] == "string" || collection_helpers::isCollectionFamilyRoot(accessTarget.templateArgs[1], collection_helpers::CollectionFamily::String);
        }
      }
      if (stringKeyValueAccess) {
        if (!emitExpr(target, localsIn)) {
          return CountAccessCallEmitResult::Error;
        }
        emitInstruction(IrOpcode::LoadStringLength, 0);
        return CountAccessCallEmitResult::Emitted;
      }
    }
  }

  if ((count_access_detail::isUnqualifiedCollectionBuiltinName(expr, "count") ||
       explicitPublishedKeyValueCountCall) && expr.args.size() == 1 &&
      inferExprKind) {
    const SemanticStringCountTargetResolution semanticStringTarget =
        classifySemanticStringCountTarget(expr.args.front(), semanticProgram, semanticIndex);
    if (semanticStringTarget == SemanticStringCountTargetResolution::NonString) {
      return CountAccessCallEmitResult::NotHandled;
    }
    if (semanticStringTarget != SemanticStringCountTargetResolution::String &&
        inferExprKind(expr.args.front(), localsIn) != LocalInfo::ValueKind::String) {
      return CountAccessCallEmitResult::NotHandled;
    }
    Expr rewrittenStringTarget = expr.args.front();
    std::string accessName;
    if (isSourceMethodStringKeyValueAccessTarget(
            rewrittenStringTarget, semanticProgram, semanticIndex, &accessName)) {
      rewrittenStringTarget.isMethodCall = false;
      rewrittenStringTarget.isFieldAccess = false;
      rewrittenStringTarget.namespacePrefix.clear();
      rewrittenStringTarget.name =
          canonicalKeyValueHelperPath(accessName);
    } else if (semanticStringTarget == SemanticStringCountTargetResolution::String &&
               rewrittenStringTarget.kind == Expr::Kind::Call &&
               rewrittenStringTarget.args.size() == 2 &&
               getBuiltinArrayAccessName(rewrittenStringTarget, accessName) &&
               (accessName == "at" || accessName == "at_unsafe")) {
      rewrittenStringTarget.isMethodCall = false;
      rewrittenStringTarget.isFieldAccess = false;
      rewrittenStringTarget.namespacePrefix.clear();
      rewrittenStringTarget.name = canonicalKeyValueHelperPath(accessName);
    }
    if (!emitExpr(rewrittenStringTarget, localsIn)) {
      return CountAccessCallEmitResult::Error;
    }
    emitInstruction(IrOpcode::LoadStringLength, 0);
    return CountAccessCallEmitResult::Emitted;
  }

  return CountAccessCallEmitResult::NotHandled;
}

CountAccessCallEmitResult tryEmitCountAccessCall(
    const Expr &expr,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const LocalMap &)> &isArrayCountCallFn,
    const std::function<bool(const Expr &, const LocalMap &)> &isVectorCapacityCallFn,
    const std::function<bool(const Expr &, const LocalMap &)> &isStringCountCallFn,
    const std::function<bool(const Expr &, const LocalMap &)> &isEntryArgsNameFn,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    std::string &error) {
  return tryEmitCountAccessCall(
      expr,
      localsIn,
      isArrayCountCallFn,
      isVectorCapacityCallFn,
      isStringCountCallFn,
      isEntryArgsNameFn,
      [](const Expr &, const LocalMap &) { return false; },
      [](const Expr &, const LocalMap &) { return false; },
      [](const Expr &, const LocalMap &) { return false; },
      [](const Expr &, const LocalMap &) { return LocalInfo::ValueKind::Unknown; },
      resolveStringTableTarget,
      emitExpr,
      emitInstruction,
      error);
}

CountAccessCallEmitResult tryEmitCountAccessCall(
    const Expr &expr,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const LocalMap &)> &isArrayCountCallFn,
    const std::function<bool(const Expr &, const LocalMap &)> &isVectorCapacityCallFn,
    const std::function<bool(const Expr &, const LocalMap &)> &isStringCountCallFn,
    const std::function<bool(const Expr &, const LocalMap &)> &isEntryArgsNameFn,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    std::string &error) {
  return tryEmitCountAccessCall(
      expr,
      localsIn,
      isArrayCountCallFn,
      isVectorCapacityCallFn,
      isStringCountCallFn,
      isEntryArgsNameFn,
      {},
      {},
      {},
      inferExprKind,
      resolveStringTableTarget,
      emitExpr,
      emitInstruction,
      error);
}

} // namespace primec::ir_lowerer
