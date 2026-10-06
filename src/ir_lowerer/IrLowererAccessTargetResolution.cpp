#include "primec/ir_lowerer/IrLowererCallHelpers.h"

#include <string_view>
#include <utility>
#include <vector>

#include "primec/ir_lowerer/IrLowererBindingTypeHelpers.h"
#include "primec/ir_lowerer/IrLowererHelpers.h"
#include "primec/ir_lowerer/IrLowererIndexKindHelpers.h"
#include "primec/ir_lowerer/IrLowererSemanticProductTargetAdapters.h"
#include "primec/ir_lowerer/IrLowererSetupTypeCollectionHelpers.h"
#include "primec/ir_lowerer/IrLowererSetupTypeHelpers.h"
#include "primec/ir_lowerer/IrLowererTemplateTypeParseHelpers.h"
#include "primec/support/CollectionHelperNames.h"
#include "IrLowererAccessTargetInternal.h"

namespace primec::ir_lowerer {
using namespace access_target_internal;

SemanticStringAccessTargetKind classifyAccessTargetSemanticStringKind(
    const Expr &targetExpr,
    const SemanticProgram *semanticProgram,
    const SemanticProductIndex *semanticIndex) {
  if (semanticProgram == nullptr || semanticIndex == nullptr || targetExpr.semanticNodeId == 0) {
    return SemanticStringAccessTargetKind::Unknown;
  }

  auto classifyTypeText = [&](const std::string &typeText,
                              SymbolId typeTextId) {
    const std::string resolvedTypeText =
        resolveAccessSemanticTypeText(semanticProgram, typeText, typeTextId);
    if (resolvedTypeText.empty()) {
      return SemanticStringAccessTargetKind::Unknown;
    }
    return valueKindFromTypeName(resolvedTypeText) == LocalInfo::ValueKind::String
               ? SemanticStringAccessTargetKind::String
               : SemanticStringAccessTargetKind::NonString;
  };

  if (const auto *collectionFact =
          findSemanticProductCollectionSpecialization(*semanticIndex, targetExpr);
      collectionFact != nullptr) {
    const std::string collectionFamily = resolveAccessSemanticTypeText(
        semanticProgram, collectionFact->collectionFamily, collectionFact->collectionFamilyId);
    if (collectionFamily == "string") {
      return SemanticStringAccessTargetKind::String;
    }
    return SemanticStringAccessTargetKind::NonString;
  }
  if (const auto *queryFact =
          findSemanticProductQueryFact(semanticProgram, *semanticIndex, targetExpr);
      queryFact != nullptr) {
    SemanticStringAccessTargetKind kind =
        classifyTypeText(queryFact->queryTypeText, queryFact->queryTypeTextId);
    if (kind != SemanticStringAccessTargetKind::Unknown) {
      return kind;
    }
    return classifyTypeText(queryFact->bindingTypeText, queryFact->bindingTypeTextId);
  }
  if (const auto *bindingFact =
          findSemanticProductBindingFact(*semanticIndex, targetExpr);
      bindingFact != nullptr) {
    return classifyTypeText(bindingFact->bindingTypeText, bindingFact->bindingTypeTextId);
  }
  if (const auto *localAutoFact =
          findSemanticProductLocalAutoFactBySemanticId(*semanticIndex, targetExpr);
      localAutoFact != nullptr) {
    return classifyTypeText(localAutoFact->bindingTypeText, localAutoFact->bindingTypeTextId);
  }
  return SemanticStringAccessTargetKind::Unknown;
}

CollectionPairTypeInfo resolveCollectionPairTypeInfo(
    const Expr &target,
    const LocalMap &localsIn,
    const ResolveCallCollectionPairTypeInfoFn &resolveCallCollectionPairTypeInfo,
    const SemanticProgram *semanticProgram,
    const SemanticProductIndex *semanticIndex) {
  CollectionPairTypeInfo info;
  const auto peelLocationWrappers = [&](const Expr &expr) {
    const Expr *current = &expr;
    while (current->kind == Expr::Kind::Call &&
           isSimpleCallName(*current, "location") &&
           current->args.size() == 1) {
      current = &current->args.front();
    }
    return current;
  };
  auto populateFromDirectLocal = [&](const LocalInfo &localInfo, bool dereferenced) {
    const bool inferredKeyValue = hasInferredTypedKeyValue(localInfo);
    const bool inferredWrappedKeyValue =
        hasInferredTypedWrappedKeyValue(localInfo, localInfo.kind);
    if (!inferredKeyValue) {
      return false;
    }
    info.isKeyValueTarget = true;
    info.keyValueKeyKind = localInfo.keyValueKeyKind;
    info.keyValueValueKind = localInfo.keyValueValueKind;
    const bool isWrappedKeyValue = inferredWrappedKeyValue;
    info.isWrappedKeyValueTarget = isWrappedKeyValue && !dereferenced;
    const bool isDirectKeyValueStorage = localInfo.kind == LocalInfo::Kind::Value;
    const std::string &resolvedStructTypeName = localInfo.structTypeName;
    const bool preserveDirectExperimentalKeyValueStruct =
        isDirectKeyValueStorage && isKeyValueStorageStructPath(resolvedStructTypeName);
    if (((!info.isWrappedKeyValueTarget || dereferenced) &&
         (!isDirectKeyValueStorage || preserveDirectExperimentalKeyValueStruct)) ||
        (info.isWrappedKeyValueTarget && isKeyValueStorageStructPath(resolvedStructTypeName))) {
      info.structTypeName = resolvedStructTypeName;
    }
    return true;
  };
  auto populateFromArgsPackElement = [&](const LocalInfo &localInfo, bool dereferenced) {
    if (!localInfo.isArgsPack) {
      return false;
    }
    // TODO-5287 (see docs/todo_finished.md): isDirectKeyValue is true for both
    // a genuine `args<map<K,V>>` pack element and the stdlib map
    // constructor's own internal `args<Entry<K,V>>` pack element - this does
    // not consult localInfo.structTypeName the way
    // IrLowererLowerStatementsExpr.h's `isKeyValueAccessReceiverArgsPackOfMap`
    // does (empty structTypeName => genuine map element, populated
    // monomorphized entry-struct path => constructor's internal pack). See that call site's
    // comment and TODO-5292 for the concrete unification/fix this gap
    // motivates.
    const bool isDirectKeyValue =
        localInfo.argsPackElementKind == LocalInfo::Kind::Value &&
        hasInferredTypedKeyValue(localInfo);
    const bool inferredWrappedKeyValue =
        hasInferredTypedWrappedKeyValue(localInfo, localInfo.argsPackElementKind);
    const bool isWrappedKeyValue = inferredWrappedKeyValue;
    if (!isDirectKeyValue && !isWrappedKeyValue) {
      return false;
    }
    info.isKeyValueTarget = true;
    info.keyValueKeyKind = localInfo.keyValueKeyKind;
    info.keyValueValueKind = localInfo.keyValueValueKind;
    info.isWrappedKeyValueTarget = isWrappedKeyValue && !dereferenced;
    const bool isDirectKeyValueStorage =
        localInfo.argsPackElementKind == LocalInfo::Kind::Value;
    const std::string &resolvedStructTypeName = localInfo.structTypeName;
    const bool preserveDirectExperimentalKeyValueStruct =
        isDirectKeyValueStorage && isKeyValueStorageStructPath(resolvedStructTypeName);
    if (((!info.isWrappedKeyValueTarget || dereferenced) &&
         (!isDirectKeyValueStorage || preserveDirectExperimentalKeyValueStruct)) ||
        (info.isWrappedKeyValueTarget && isKeyValueStorageStructPath(resolvedStructTypeName))) {
      info.structTypeName = resolvedStructTypeName;
    }
    return true;
  };
  auto resolveArgsPackAccessTarget = [&](const Expr &candidate,
                                         bool dereferenced) {
    const Expr *accessReceiver = peelLocationWrappers(candidate);
    bool receiverDereferenced = dereferenced;
    while (accessReceiver->kind == Expr::Kind::Call &&
           isSimpleCallName(*accessReceiver, "dereference") &&
           accessReceiver->args.size() == 1) {
      receiverDereferenced = true;
      accessReceiver = peelLocationWrappers(accessReceiver->args.front());
    }
    if (accessReceiver->kind == Expr::Kind::Name) {
      auto it = localsIn.find(accessReceiver->name);
      if (it != localsIn.end() && populateFromArgsPackElement(it->second, false)) {
        info.isWrappedKeyValueTarget = info.isWrappedKeyValueTarget && !receiverDereferenced;
        return true;
      }
    }
    return false;
  };

  if (target.kind == Expr::Kind::Name) {
    auto it = localsIn.find(target.name);
    if (it != localsIn.end() && populateFromArgsPackElement(it->second, false)) {
      return info;
    }
  }
  if (target.kind == Expr::Kind::Call) {
    std::string preSemanticAccessName;
    std::string preSemanticKeyValueHelperName;
    const std::string preSemanticScopedPath = resolveScopedCallPath(target);
    const bool preSemanticAliasKeyValueAccess =
        resolveKeyValueHelperAliasName(target, preSemanticKeyValueHelperName) &&
        isKeyValueAccessHelperName(preSemanticKeyValueHelperName);
    const bool preSemanticExplicitKeyValueAccess =
        !target.isMethodCall &&
        (isExplicitKeyValueAccessHelperPath(preSemanticScopedPath) ||
         preSemanticAliasKeyValueAccess) &&
        target.args.size() == 2;
    if (((getBuiltinArrayAccessName(target, preSemanticAccessName) &&
          target.args.size() == 2) ||
         preSemanticExplicitKeyValueAccess) &&
        resolveArgsPackAccessTarget(target.args.front(), false)) {
      return info;
    }
  }

  bool hasSemanticTargetFact = false;
  if (resolveSemanticCollectionPairTypeInfo(
          target, semanticProgram, semanticIndex, info, hasSemanticTargetFact)) {
    return info;
  }
  if (hasSemanticTargetFact) {
    return {};
  }
  if (target.kind == Expr::Kind::Name) {
    if (target.semanticNodeId != 0 && resolveCallCollectionPairTypeInfo) {
      CollectionPairTypeInfo inferred;
      if (resolveCallCollectionPairTypeInfo(target, inferred)) {
        return inferred;
      }
    }
    auto it = localsIn.find(target.name);
    if (it != localsIn.end()) {
      populateFromDirectLocal(it->second, false);
    }
    if (!info.isKeyValueTarget && resolveCallCollectionPairTypeInfo) {
      CollectionPairTypeInfo inferred;
      if (resolveCallCollectionPairTypeInfo(target, inferred)) {
        return inferred;
      }
    }
    return info;
  }
  if (target.kind == Expr::Kind::Call) {
    if (isSimpleCallName(target, "dereference") && target.args.size() == 1) {
      const Expr &derefTarget = target.args.front();
      if (derefTarget.kind == Expr::Kind::Name) {
        auto it = localsIn.find(derefTarget.name);
        if (it != localsIn.end() && populateFromDirectLocal(it->second, true)) {
          return info;
        }
      }
      std::string derefAccessName;
      const std::string derefScopedPath = resolveScopedCallPath(derefTarget);
      const bool isDerefKeyValueAccess =
          derefTarget.kind == Expr::Kind::Call &&
          ((getBuiltinArrayAccessName(derefTarget, derefAccessName) &&
            derefTarget.args.size() == 2) ||
           (isExplicitKeyValueAccessHelperPath(derefScopedPath) &&
            derefTarget.args.size() == 2));
      if (isDerefKeyValueAccess &&
          derefTarget.args.front().kind == Expr::Kind::Name) {
        auto it = localsIn.find(derefTarget.args.front().name);
        if (it != localsIn.end() && populateFromArgsPackElement(it->second, true)) {
          return info;
        }
      }
    }
    std::string accessName;
    std::string helperName;
    const std::string scopedTargetPath = resolveScopedCallPath(target);
    const bool isAliasKeyValueArgsPackAccess =
        resolveKeyValueHelperAliasName(target, helperName) &&
        isKeyValueAccessHelperName(helperName);
    const bool isExplicitKeyValueArgsPackAccess =
        !target.isMethodCall &&
        (isExplicitKeyValueAccessHelperPath(scopedTargetPath) ||
         isAliasKeyValueArgsPackAccess) &&
        target.args.size() == 2;
    if ((getBuiltinArrayAccessName(target, accessName) && target.args.size() == 2) ||
        isExplicitKeyValueArgsPackAccess) {
      if (resolveArgsPackAccessTarget(target.args.front(), false)) {
        return info;
      }
    }
    std::string collection;
    if (resolveCallCollectionPairTypeInfo) {
      CollectionPairTypeInfo inferred;
      if (resolveCallCollectionPairTypeInfo(target, inferred)) {
        return inferred;
      }
    }
    CollectionPairTypeInfo directConstructorInfo;
    const bool hasDirectConstructorInfo =
        inferDirectKeyValueConstructorTargetInfo(target, directConstructorInfo);
    if (hasDirectConstructorInfo) {
      return directConstructorInfo;
    }
    if (getBuiltinCollectionName(target, collection) && collection == "map" &&
        target.templateArgs.size() == 2) {
      info.isKeyValueTarget = true;
      info.keyValueKeyKind = valueKindFromTypeName(target.templateArgs[0]);
      info.keyValueValueKind = valueKindFromTypeName(target.templateArgs[1]);
      return info;
    }
  }
  return info;
}

CollectionPairTypeInfo resolveCollectionPairTypeInfo(
    const Expr &target,
    const LocalMap &localsIn,
    const ResolveCallCollectionPairTypeInfoFn &resolveCallCollectionPairTypeInfo) {
  return resolveCollectionPairTypeInfo(
      target, localsIn, resolveCallCollectionPairTypeInfo, nullptr, nullptr);
}

CollectionPairTypeInfo resolveCollectionPairTypeInfo(const Expr &target, const LocalMap &localsIn) {
  return resolveCollectionPairTypeInfo(target, localsIn, {}, nullptr, nullptr);
}

bool inferForwardedCollectionPairTypeInfo(
    const Expr &target,
    const Definition &callee,
    const LocalMap &localsIn,
    const ResolveCallCollectionPairTypeInfoFn &resolveCallCollectionPairTypeInfo,
    CollectionPairTypeInfo &targetInfoOut) {
  targetInfoOut = {};
  if (target.kind != Expr::Kind::Call || target.isMethodCall || target.isBinding) {
    return false;
  }

  std::string parameterName;
  if (!resolveForwardedReturnParameterName(callee, parameterName)) {
    return false;
  }
  const Expr *forwardedArg =
      resolveCallArgumentForParameter(target, callee, parameterName);
  if (forwardedArg == nullptr) {
    return false;
  }
  if (isForwardedKeyValueNewConstructor(*forwardedArg)) {
    return false;
  }

  CollectionPairTypeInfo forwardedInfo =
      resolveCollectionPairTypeInfo(
          *forwardedArg, localsIn, resolveCallCollectionPairTypeInfo, nullptr, nullptr);
  if (!forwardedInfo.isKeyValueTarget) {
    return false;
  }
  targetInfoOut = std::move(forwardedInfo);
  return true;
}

bool validateCollectionPairTypeInfo(const CollectionPairTypeInfo &targetInfo,
                                 const std::string &accessName,
                                 std::string &error) {
  if (!targetInfo.isKeyValueTarget) {
    return true;
  }
  if (targetInfo.keyValueKeyKind == LocalInfo::ValueKind::Unknown ||
      targetInfo.keyValueValueKind == LocalInfo::ValueKind::Unknown) {
    error = "native backend requires typed map bindings for " + accessName;
    return false;
  }
  return true;
}

NonLiteralStringAccessTargetResult validateNonLiteralStringAccessTarget(
    const Expr &targetExpr,
    const LocalMap &localsIn,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<bool(const Expr &, const LocalMap &)> &isEntryArgsName,
    std::string &error,
    const SemanticProgram *semanticProgram,
    const SemanticProductIndex *semanticIndex) {
  if (targetExpr.kind == Expr::Kind::StringLiteral) {
    return NonLiteralStringAccessTargetResult::Stop;
  }
  const SemanticStringAccessTargetKind semanticTargetKind =
      classifyAccessTargetSemanticStringKind(targetExpr, semanticProgram, semanticIndex);
  if (semanticTargetKind == SemanticStringAccessTargetKind::NonString) {
    return NonLiteralStringAccessTargetResult::Continue;
  }
  if (targetExpr.kind == Expr::Kind::Name) {
    const LocalInfo::ValueKind targetKind =
        semanticTargetKind == SemanticStringAccessTargetKind::String
            ? LocalInfo::ValueKind::String
            : inferExprKind(targetExpr, localsIn);
    if (targetKind != LocalInfo::ValueKind::Unknown &&
        targetKind != LocalInfo::ValueKind::String) {
      return NonLiteralStringAccessTargetResult::Continue;
    }
    if (targetKind == LocalInfo::ValueKind::String) {
      error = "native backend only supports indexing into string literals or string bindings";
      return NonLiteralStringAccessTargetResult::Error;
    }
    auto it = localsIn.find(targetExpr.name);
    if (it != localsIn.end() && it->second.kind == LocalInfo::Kind::Value &&
        it->second.valueKind == LocalInfo::ValueKind::String) {
      error = "native backend only supports indexing into string literals or string bindings";
      return NonLiteralStringAccessTargetResult::Error;
    }
  }
  if (isEntryArgsName(targetExpr, localsIn)) {
    error = "native backend only supports entry argument indexing in print calls or string bindings";
    return NonLiteralStringAccessTargetResult::Error;
  }
  return NonLiteralStringAccessTargetResult::Continue;
}

NonLiteralStringAccessTargetResult validateNonLiteralStringAccessTarget(
    const Expr &targetExpr,
    const LocalMap &localsIn,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<bool(const Expr &, const LocalMap &)> &isEntryArgsName,
    std::string &error) {
  return validateNonLiteralStringAccessTarget(
      targetExpr, localsIn, inferExprKind, isEntryArgsName, error, nullptr, nullptr);
}

bool resolveValidatedAccessIndexKind(
    const Expr &indexExpr,
    const LocalMap &localsIn,
    const std::string &accessName,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    LocalInfo::ValueKind &indexKindOut,
    std::string &error,
    const SemanticProgram *semanticProgram,
    const SemanticProductIndex *semanticIndex) {
  indexKindOut = normalizeIndexKind(resolveAccessIndexSemanticKind(indexExpr, semanticProgram, semanticIndex));
  if (indexKindOut == LocalInfo::ValueKind::Unknown) {
    indexKindOut = normalizeIndexKind(inferExprKind(indexExpr, localsIn));
  }
  if (!isSupportedIndexKind(indexKindOut)) {
    error = "native backend requires integer indices for " + accessName;
    return false;
  }
  return true;
}

bool resolveValidatedAccessIndexKind(
    const Expr &indexExpr,
    const LocalMap &localsIn,
    const std::string &accessName,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    LocalInfo::ValueKind &indexKindOut,
    std::string &error) {
  return resolveValidatedAccessIndexKind(
      indexExpr, localsIn, accessName, inferExprKind, indexKindOut, error, nullptr, nullptr);
}

ArrayVectorAccessTargetInfo resolveArrayVectorAccessTargetInfo(
    const Expr &target,
    const LocalMap &localsIn,
    const ResolveCallArrayVectorAccessTargetInfoFn &resolveCallArrayVectorAccessTargetInfo,
    const SemanticProgram *semanticProgram,
    const SemanticProductIndex *semanticIndex) {
  ArrayVectorAccessTargetInfo info;
  const auto elementSlotCountForLocal = [](const LocalInfo &localInfo) {
    if (localInfo.isArgsPack) {
      const bool isInlineStructPack =
          localInfo.argsPackElementKind == LocalInfo::Kind::Value &&
          !localInfo.structTypeName.empty() &&
          localInfo.structSlotCount > 0;
      return isInlineStructPack ? localInfo.structSlotCount : 1;
    }
    if (localInfo.vectorStructElementSlotCount > 0) {
      return localInfo.vectorStructElementSlotCount;
    }
    return localInfo.structSlotCount;
  };
  const auto populateFromArgsPackLocal = [&](const LocalInfo &localInfo, bool dereferenced) {
    if (!localInfo.isArgsPack) {
      return false;
    }

    info.elemKind = localInfo.valueKind;
    info.isSoaVector = localInfo.isSoaVector;
    info.isArgsPackTarget = !dereferenced;
    info.argsPackElementKind = localInfo.argsPackElementKind;
    info.elemSlotCount = elementSlotCountForLocal(localInfo);
    info.structTypeName = localInfo.structTypeName;
    info.isKeyValueTarget = false;
    info.isWrappedKeyValueTarget = false;

    if (localInfo.argsPackElementKind == LocalInfo::Kind::Array ||
        localInfo.argsPackElementKind == LocalInfo::Kind::Buffer ||
        localInfo.argsPackElementKind == LocalInfo::Kind::Vector) {
      info.isArrayOrVectorTarget = true;
      info.isVectorTarget =
          dereferenced && localInfo.argsPackElementKind == LocalInfo::Kind::Vector;
      return true;
    }
    // TODO-5287 (see docs/todo_finished.md): this branch fires identically
    // for a genuine `args<map<K,V>>` pack element and the stdlib map
    // constructor's own internal `args<Entry<K,V>>` pack element - like
    // resolveCollectionPairTypeInfo's populateFromArgsPackElement above (same
    // gap, see its comment), it does not consult localInfo.structTypeName
    // the way IrLowererLowerStatementsExpr.h's
    // `isKeyValueAccessReceiverArgsPackOfMap` does for a Name-kind receiver.
    // See TODO-5292 for the concrete unification/fix this gap motivates.
    if (localInfo.argsPackElementKind == LocalInfo::Kind::Value &&
        hasInferredTypedKeyValue(localInfo)) {
      info.isArrayOrVectorTarget = true;
      info.isVectorTarget = false;
      info.isKeyValueTarget = true;
      return true;
    }
    if (localInfo.argsPackElementKind == LocalInfo::Kind::Value &&
        localInfo.valueKind != LocalInfo::ValueKind::Unknown &&
        localInfo.valueKind != LocalInfo::ValueKind::String &&
        localInfo.structTypeName.empty()) {
      info.isArrayOrVectorTarget = true;
      info.isVectorTarget = false;
      return true;
    }
    if (localInfo.argsPackElementKind == LocalInfo::Kind::Value &&
        !localInfo.isSoaVector &&
        isCollectionVectorRecordPath(localInfo.structTypeName)) {
      info.isArrayOrVectorTarget = true;
      info.isVectorTarget = true;
      info.isSoaVector = false;
      info.argsPackElementKind = LocalInfo::Kind::Vector;
      info.elemSlotCount = 1;
      return true;
    }
    if (!localInfo.structTypeName.empty() &&
        localInfo.argsPackElementKind == LocalInfo::Kind::Value) {
      info.isArrayOrVectorTarget = true;
      info.isVectorTarget = false;
      return true;
    }
    if (localInfo.argsPackElementKind == LocalInfo::Kind::Reference &&
        (localInfo.referenceToArray || localInfo.referenceToVector || localInfo.referenceToBuffer ||
         !localInfo.structTypeName.empty())) {
      info.isArrayOrVectorTarget = true;
      info.isVectorTarget = localInfo.referenceToVector;
      info.isKeyValueTarget = hasInferredTypedKeyValue(localInfo) && dereferenced;
      info.isWrappedKeyValueTarget = hasInferredTypedKeyValue(localInfo) && !dereferenced;
      if (hasInferredTypedKeyValue(localInfo) && dereferenced) {
        info.structTypeName = localInfo.structTypeName;
      }
      return true;
    }
    if (localInfo.argsPackElementKind == LocalInfo::Kind::Reference &&
        localInfo.valueKind != LocalInfo::ValueKind::Unknown) {
      info.isArrayOrVectorTarget = true;
      info.isVectorTarget = false;
      return true;
    }
    if (localInfo.argsPackElementKind == LocalInfo::Kind::Pointer &&
        (localInfo.pointerToArray || localInfo.pointerToVector || localInfo.pointerToBuffer ||
         !localInfo.structTypeName.empty())) {
      info.isArrayOrVectorTarget = true;
      info.isVectorTarget = localInfo.pointerToVector;
      info.isKeyValueTarget = hasInferredTypedKeyValue(localInfo) && dereferenced;
      info.isWrappedKeyValueTarget = hasInferredTypedKeyValue(localInfo) && !dereferenced;
      if (hasInferredTypedKeyValue(localInfo) && dereferenced) {
        info.structTypeName = localInfo.structTypeName;
      }
      return true;
    }
    if (localInfo.argsPackElementKind == LocalInfo::Kind::Pointer &&
        localInfo.valueKind != LocalInfo::ValueKind::Unknown) {
      info.isArrayOrVectorTarget = true;
      info.isVectorTarget = false;
      return true;
    }
    return false;
  };

  if (target.kind == Expr::Kind::Name) {
    auto it = localsIn.find(target.name);
    if (it != localsIn.end() && populateFromArgsPackLocal(it->second, false)) {
      return info;
    }
  }

  bool hasSemanticTargetFact = false;
  if (resolveSemanticArrayVectorAccessTargetInfo(
          target, semanticProgram, semanticIndex, info, hasSemanticTargetFact) &&
      (info.elemKind != LocalInfo::ValueKind::Unknown ||
       !info.structTypeName.empty())) {
    if (target.kind == Expr::Kind::Name && info.isVectorTarget &&
        info.elemKind == LocalInfo::ValueKind::Unknown) {
      if (auto it = localsIn.find(target.name);
          it != localsIn.end() && it->second.vectorStructElementSlotCount > 0) {
        info.elemSlotCount = it->second.vectorStructElementSlotCount;
      }
    }
    return info;
  }
  if (hasSemanticTargetFact && resolveCallArrayVectorAccessTargetInfo) {
    ArrayVectorAccessTargetInfo inferred;
    if (resolveCallArrayVectorAccessTargetInfo(target, inferred)) {
      return inferred;
    }
  }
  if (hasSemanticTargetFact) {
    return {};
  }

  if (target.kind == Expr::Kind::Name) {
    if (target.semanticNodeId != 0 && resolveCallArrayVectorAccessTargetInfo) {
      ArrayVectorAccessTargetInfo inferred;
      if (resolveCallArrayVectorAccessTargetInfo(target, inferred)) {
        return inferred;
      }
    }
    auto it = localsIn.find(target.name);
    if (it != localsIn.end() && populateFromArgsPackLocal(it->second, false)) {
      return info;
    }
    if (it != localsIn.end() &&
        (it->second.kind == LocalInfo::Kind::Array || it->second.kind == LocalInfo::Kind::Vector ||
         it->second.kind == LocalInfo::Kind::Buffer)) {
      info.isArrayOrVectorTarget = true;
      info.elemKind = it->second.valueKind;
      info.isVectorTarget = (it->second.kind == LocalInfo::Kind::Vector);
      info.isSoaVector = it->second.isSoaVector;
      info.isArgsPackTarget = it->second.isArgsPack;
      info.argsPackElementKind = it->second.argsPackElementKind;
      info.elemSlotCount = elementSlotCountForLocal(it->second);
      info.structTypeName = it->second.structTypeName;
      return info;
    }
    if (it != localsIn.end() && it->second.kind == LocalInfo::Kind::Value &&
        it->second.isSoaVector && !it->second.structTypeName.empty()) {
      info.isArrayOrVectorTarget = true;
      info.elemKind = it->second.valueKind;
      info.isVectorTarget = false;
      info.isSoaVector = true;
      info.isArgsPackTarget = it->second.isArgsPack;
      info.argsPackElementKind = it->second.argsPackElementKind;
      info.elemSlotCount = elementSlotCountForLocal(it->second);
      info.structTypeName = it->second.structTypeName;
      return info;
    }
    if (it != localsIn.end() && it->second.kind == LocalInfo::Kind::Value &&
        !it->second.isSoaVector && isCollectionVectorRecordPath(it->second.structTypeName)) {
      info.isArrayOrVectorTarget = true;
      info.elemKind = it->second.valueKind;
      info.isVectorTarget = true;
      info.isSoaVector = false;
      info.isStructBoxedRecordTarget = true;
      info.isArgsPackTarget = it->second.isArgsPack;
      info.argsPackElementKind = it->second.argsPackElementKind;
      info.elemSlotCount = elementSlotCountForLocal(it->second);
      info.structTypeName = it->second.structTypeName;
      return info;
    }
    if (it != localsIn.end() && it->second.kind == LocalInfo::Kind::Reference &&
        (it->second.referenceToArray || it->second.referenceToVector || it->second.referenceToBuffer)) {
      info.isArrayOrVectorTarget = true;
      info.elemKind = it->second.valueKind;
      info.isVectorTarget = it->second.referenceToVector;
      info.isSoaVector = it->second.isSoaVector;
      info.isArgsPackTarget = it->second.isArgsPack;
      info.argsPackElementKind = it->second.argsPackElementKind;
      info.elemSlotCount = elementSlotCountForLocal(it->second);
      info.structTypeName = it->second.structTypeName;
      return info;
    }
    if (it != localsIn.end() && it->second.kind == LocalInfo::Kind::Pointer &&
        (it->second.pointerToArray || it->second.pointerToBuffer)) {
      info.isArrayOrVectorTarget = true;
      info.elemKind = it->second.valueKind;
      info.isVectorTarget = false;
      info.isArgsPackTarget = it->second.isArgsPack;
      info.argsPackElementKind = it->second.argsPackElementKind;
      info.elemSlotCount = elementSlotCountForLocal(it->second);
      info.structTypeName = it->second.structTypeName;
      return info;
    }
    if (it != localsIn.end() && it->second.kind == LocalInfo::Kind::Pointer && it->second.pointerToVector) {
      info.isArrayOrVectorTarget = true;
      info.elemKind = it->second.valueKind;
      info.isVectorTarget = true;
      info.isSoaVector = it->second.isSoaVector;
      info.isArgsPackTarget = it->second.isArgsPack;
      info.argsPackElementKind = it->second.argsPackElementKind;
      info.elemSlotCount = elementSlotCountForLocal(it->second);
      info.structTypeName = it->second.structTypeName;
      return info;
    }
    if (!info.isArrayOrVectorTarget && resolveCallArrayVectorAccessTargetInfo) {
      ArrayVectorAccessTargetInfo inferred;
      if (resolveCallArrayVectorAccessTargetInfo(target, inferred)) {
        return inferred;
      }
    }
    return info;
  }
  if (target.kind == Expr::Kind::Call) {
    if (!target.isMethodCall && isSimpleCallName(target, "slice") &&
        target.args.size() == 3) {
      ArrayVectorAccessTargetInfo sourceInfo = resolveArrayVectorAccessTargetInfo(
          target.args.front(),
          localsIn,
          resolveCallArrayVectorAccessTargetInfo,
          semanticProgram,
          semanticIndex);
      if (sourceInfo.isArrayOrVectorTarget && !sourceInfo.isVectorTarget) {
        sourceInfo.isArgsPackTarget = false;
        sourceInfo.argsPackElementKind = LocalInfo::Kind::Value;
        info = sourceInfo;
        return info;
      }
    }
    if (isSimpleCallName(target, "dereference") &&
        target.args.size() == 1 &&
        target.args.front().kind == Expr::Kind::Name) {
      auto localIt = localsIn.find(target.args.front().name);
      if (localIt != localsIn.end() &&
          localIt->second.isArgsPack &&
          populateFromArgsPackLocal(localIt->second, false)) {
        return info;
      }
    }
    auto resolveArrayOrVectorAccessName = [](const Expr &accessExpr,
                                             std::string &accessNameOut) {
      if (getBuiltinArrayAccessName(accessExpr, accessNameOut)) {
        return true;
      }
      std::string vectorHelperName;
      if (resolveVectorHelperAliasName(accessExpr, vectorHelperName) &&
          (vectorHelperName == "at" || vectorHelperName == "at_unsafe")) {
        accessNameOut = vectorHelperName;
        return true;
      }
      return false;
    };

    auto resolveDereferencedArgsPackTarget = [&](const Expr &derefTarget) {
      std::string derefAccessName;
      if (!(resolveArrayOrVectorAccessName(derefTarget, derefAccessName) && derefTarget.args.size() == 2)) {
        return false;
      }

      const Expr &accessReceiver = derefTarget.args.front();
      if (accessReceiver.kind != Expr::Kind::Name) {
        return false;
      }

      auto localIt = localsIn.find(accessReceiver.name);
      if (localIt == localsIn.end() || !localIt->second.isArgsPack) {
        return false;
      }

      const LocalInfo &localInfo = localIt->second;
      if (populateFromArgsPackLocal(localInfo, true)) {
        return true;
      }
      if (localInfo.argsPackElementKind == LocalInfo::Kind::Array) {
        info.isArrayOrVectorTarget = true;
        info.elemKind = localInfo.valueKind;
        info.isVectorTarget = false;
        info.isArgsPackTarget = false;
        info.argsPackElementKind = localInfo.argsPackElementKind;
        info.elemSlotCount = elementSlotCountForLocal(localInfo);
        info.structTypeName = localInfo.structTypeName;
        return true;
      }
      if (localInfo.argsPackElementKind == LocalInfo::Kind::Buffer) {
        info.isArrayOrVectorTarget = true;
        info.elemKind = localInfo.valueKind;
        info.isVectorTarget = false;
        info.isArgsPackTarget = false;
        info.argsPackElementKind = localInfo.argsPackElementKind;
        info.elemSlotCount = elementSlotCountForLocal(localInfo);
        info.structTypeName = localInfo.structTypeName;
        return true;
      }
      if (localInfo.argsPackElementKind == LocalInfo::Kind::Vector) {
        info.isArrayOrVectorTarget = true;
        info.elemKind = localInfo.valueKind;
        info.isVectorTarget = true;
        info.isSoaVector = localInfo.isSoaVector;
        info.isArgsPackTarget = false;
        info.argsPackElementKind = localInfo.argsPackElementKind;
        info.elemSlotCount = elementSlotCountForLocal(localInfo);
        info.structTypeName = localInfo.structTypeName;
        return true;
      }
      if (localInfo.argsPackElementKind == LocalInfo::Kind::Reference &&
          (localInfo.referenceToArray || localInfo.referenceToVector || localInfo.referenceToBuffer)) {
        info.isArrayOrVectorTarget = true;
        info.elemKind = localInfo.valueKind;
        info.isVectorTarget = localInfo.referenceToVector;
        info.isSoaVector = localInfo.isSoaVector;
        info.isArgsPackTarget = false;
        info.argsPackElementKind = localInfo.argsPackElementKind;
        info.elemSlotCount = elementSlotCountForLocal(localInfo);
        info.structTypeName = localInfo.structTypeName;
        return true;
      }
      if (localInfo.argsPackElementKind == LocalInfo::Kind::Pointer &&
          (localInfo.pointerToArray || localInfo.pointerToVector || localInfo.pointerToBuffer)) {
        info.isArrayOrVectorTarget = true;
        info.elemKind = localInfo.valueKind;
        info.isVectorTarget = localInfo.pointerToVector;
        info.isSoaVector = localInfo.isSoaVector;
        info.isArgsPackTarget = false;
        info.argsPackElementKind = localInfo.argsPackElementKind;
        info.elemSlotCount = elementSlotCountForLocal(localInfo);
        info.structTypeName = localInfo.structTypeName;
        return true;
      }
      return false;
    };

    std::string accessName;
    std::string helperName;
    const std::string scopedTargetPath = resolveScopedCallPath(target);
    const bool isAliasKeyValueArgsPackAccess =
        resolveKeyValueHelperAliasName(target, helperName) &&
        isKeyValueAccessHelperName(helperName);
    const bool isExplicitKeyValueArgsPackAccess =
        !target.isMethodCall &&
        (isExplicitKeyValueAccessHelperPath(scopedTargetPath) ||
         isAliasKeyValueArgsPackAccess) &&
        target.args.size() == 2;
    if ((resolveArrayOrVectorAccessName(target, accessName) && target.args.size() == 2) ||
        isExplicitKeyValueArgsPackAccess) {
      const Expr &accessReceiver = target.args.front();
      if (accessReceiver.kind == Expr::Kind::Name) {
        auto localIt = localsIn.find(accessReceiver.name);
        if (localIt != localsIn.end() && populateFromArgsPackLocal(localIt->second, false)) {
          return info;
        }
        if (localIt != localsIn.end() && localIt->second.isArgsPack) {
          if (localIt->second.argsPackElementKind == LocalInfo::Kind::Vector) {
            info.isArrayOrVectorTarget = true;
            info.elemKind = localIt->second.valueKind;
            info.isVectorTarget = true;
            info.isSoaVector = localIt->second.isSoaVector;
            info.isArgsPackTarget = true;
            info.argsPackElementKind = localIt->second.argsPackElementKind;
            info.elemSlotCount = elementSlotCountForLocal(localIt->second);
            info.structTypeName = localIt->second.structTypeName;
            return info;
          }
          if (localIt->second.argsPackElementKind == LocalInfo::Kind::Array ||
              localIt->second.argsPackElementKind == LocalInfo::Kind::Buffer ||
              (localIt->second.argsPackElementKind == LocalInfo::Kind::Reference &&
               (localIt->second.referenceToArray || localIt->second.referenceToVector ||
                localIt->second.referenceToBuffer))) {
            info.isArrayOrVectorTarget = true;
            info.elemKind = localIt->second.valueKind;
            info.isVectorTarget =
                localIt->second.argsPackElementKind == LocalInfo::Kind::Reference &&
                localIt->second.referenceToVector;
            info.isSoaVector = localIt->second.isSoaVector;
            info.isArgsPackTarget = true;
            info.argsPackElementKind = localIt->second.argsPackElementKind;
            info.elemSlotCount = elementSlotCountForLocal(localIt->second);
            info.structTypeName = localIt->second.structTypeName;
            return info;
          }
          if (localIt->second.argsPackElementKind == LocalInfo::Kind::Pointer &&
              (localIt->second.pointerToArray || localIt->second.pointerToBuffer)) {
            info.isArrayOrVectorTarget = true;
            info.elemKind = localIt->second.valueKind;
            info.isVectorTarget = false;
            info.isArgsPackTarget = true;
            info.argsPackElementKind = localIt->second.argsPackElementKind;
            info.elemSlotCount = elementSlotCountForLocal(localIt->second);
            info.structTypeName = localIt->second.structTypeName;
            return info;
          }
          if (localIt->second.argsPackElementKind == LocalInfo::Kind::Pointer && localIt->second.pointerToVector) {
            info.isArrayOrVectorTarget = true;
            info.elemKind = localIt->second.valueKind;
            info.isVectorTarget = true;
            info.isSoaVector = localIt->second.isSoaVector;
            info.isArgsPackTarget = true;
            info.argsPackElementKind = localIt->second.argsPackElementKind;
            info.elemSlotCount = elementSlotCountForLocal(localIt->second);
            info.structTypeName = localIt->second.structTypeName;
            return info;
          }
        }
      }
    }
    if (isSimpleCallName(target, "dereference") && target.args.size() == 1 &&
        resolveDereferencedArgsPackTarget(target.args.front())) {
      return info;
    }
    std::string collection;
    if (getBuiltinCollectionName(target, collection) &&
        (collection == "array" || collection == "vector" || collection == "Buffer" ||
         collection == "soa") &&
        target.templateArgs.size() == 1) {
      info.isArrayOrVectorTarget = true;
      info.elemKind = valueKindFromTypeName(target.templateArgs.front());
      info.isVectorTarget = (collection == "vector");
      info.isSoaVector = (collection == "soa");
      if (info.isSoaVector) {
        info.structTypeName =
            inferExperimentalSoaVectorStructPathFromTypeName(target.templateArgs.front());
      }
      return info;
    }
    if (resolveCallArrayVectorAccessTargetInfo) {
      ArrayVectorAccessTargetInfo inferred;
      if (resolveCallArrayVectorAccessTargetInfo(target, inferred)) {
        return inferred;
      }
    }
  }
  return info;
}

ArrayVectorAccessTargetInfo resolveArrayVectorAccessTargetInfo(
    const Expr &target,
    const LocalMap &localsIn,
    const ResolveCallArrayVectorAccessTargetInfoFn &resolveCallArrayVectorAccessTargetInfo) {
  return resolveArrayVectorAccessTargetInfo(
      target, localsIn, resolveCallArrayVectorAccessTargetInfo, nullptr, nullptr);
}

ArrayVectorAccessTargetInfo resolveArrayVectorAccessTargetInfo(
    const Expr &target, const LocalMap &localsIn) {
  return resolveArrayVectorAccessTargetInfo(target, localsIn, {}, nullptr, nullptr);
}

} // namespace primec::ir_lowerer
