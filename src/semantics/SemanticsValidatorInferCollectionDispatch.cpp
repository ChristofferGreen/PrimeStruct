#include "SemanticsValidator.h"
#include "SemanticsValidatorInferCollectionCompatibilityInternal.h"
#include "primec/support/CollectionHelperNames.h"

namespace primec::semantics {
namespace {

const StdlibSurfaceMetadata *
keyValueHelperSurfaceMetadataForInferCollectionDispatch() {
  return keyValueHelperSurfaceMetadataLocal();
}

bool resolveKeyValueHelperResolvedPathForInferCollectionDispatch(
    const std::string &resolvedPath,
    std::string &resolvedKeyValueHelperNameOut) {
  resolvedKeyValueHelperNameOut.clear();
  const StdlibSurfaceMetadata *metadata =
      keyValueHelperSurfaceMetadataForInferCollectionDispatch();
  return metadata != nullptr &&
         resolvePublishedCollectionHelperResolvedPath(
             resolvedPath, metadata->id, resolvedKeyValueHelperNameOut);
}

} // namespace

bool SemanticsValidator::resolveBuiltinCollectionMethodReturnKind(
    const std::string &resolvedPath,
    const Expr &receiverExpr,
    const BuiltinCollectionDispatchResolvers &resolvers,
    ReturnKind &kindOut) const {
  std::string resolvedKeyValueHelperName;
  if (resolveKeyValueHelperResolvedPathForInferCollectionDispatch(
          resolvedPath, resolvedKeyValueHelperName)) {
    auto declaredReturnKind = [](const Definition &definition,
                                 ReturnKind &declaredKindOut) {
      for (const Transform &transform : definition.transforms) {
        if (transform.name != "return" || transform.templateArgs.size() != 1) {
          continue;
        }
        declaredKindOut =
            returnKindForTypeName(normalizeBindingTypeName(transform.templateArgs.front()));
        return true;
      }
      return false;
    };
    auto defIt = defMap_.find(resolvedPath);
    if (defIt != defMap_.end() && defIt->second != nullptr) {
      ReturnKind declaredKind = ReturnKind::Unknown;
      if (declaredReturnKind(*defIt->second, declaredKind)) {
        if (declaredKind != ReturnKind::Unknown) {
          kindOut = declaredKind;
          return true;
        }
        return false;
      }
    }
    for (const Definition &definition : program_.definitions) {
      if (!matchesResolvedPath(definition.fullPath, resolvedPath)) {
        continue;
      }
      ReturnKind declaredKind = ReturnKind::Unknown;
      if (declaredReturnKind(definition, declaredKind)) {
        if (declaredKind != ReturnKind::Unknown) {
          kindOut = declaredKind;
          return true;
        }
        return false;
      }
    }
  }
  std::string resolvedSoaCanonical =
      canonicalizeLegacySoaGetHelperPath(resolvedPath);
  const bool resolvedSoaCanonicalIsCount =
      isLegacyOrCanonicalSoaHelperPath(resolvedSoaCanonical, "count");
  const bool resolvedSoaCanonicalIsCountRef =
      isLegacyOrCanonicalSoaHelperPath(resolvedSoaCanonical, collection_helpers::kCountRef);
  const bool resolvedVectorCount =
      isStdNamespacedVectorCompatibilityHelperPath(resolvedPath, "count");
  const bool resolvedVectorCapacity =
      isStdNamespacedVectorCompatibilityHelperPath(resolvedPath, "capacity");
  if (resolvedPath == collection_helpers::kRootedArrayCount ||
      resolvedVectorCount || resolvedPath == collection_helpers::kRootedStringCount ||
      resolvedSoaCanonicalIsCount || resolvedSoaCanonicalIsCountRef ||
      isExperimentalSoaCountLikeHelperPath(resolvedSoaCanonical) ||
      resolvedVectorCapacity) {
    kindOut = ReturnKind::Int;
    return true;
  }
  if (resolvedKeyValueHelperName == "contains" ||
      resolvedKeyValueHelperName == collection_helpers::kContainsRef) {
    kindOut = ReturnKind::Bool;
    return true;
  }
  if (resolvedPath == collection_helpers::kRootedStringAt || resolvedPath == collection_helpers::kRootedStringAtUnsafe) {
    kindOut = ReturnKind::Int;
    return true;
  }
  if (resolvedPath == collection_helpers::kRootedArrayAt || resolvedPath == collection_helpers::kRootedArrayAtUnsafe) {
    std::string elemType;
    if (resolvers.resolveArgsPackAccessTarget(receiverExpr, elemType) ||
        resolvers.resolveArrayTarget(receiverExpr, elemType)) {
      ReturnKind kind = returnKindForTypeName(normalizeBindingTypeName(elemType));
      if (kind != ReturnKind::Unknown) {
        kindOut = kind;
        return true;
      }
    }
    return false;
  }
  if (isStdNamespacedVectorCompatibilityHelperPath(resolvedPath, "at") ||
      isStdNamespacedVectorCompatibilityHelperPath(resolvedPath, "at_unsafe")) {
    std::string elemType;
    if (resolvers.resolveVectorTarget(receiverExpr, elemType)) {
      ReturnKind kind = returnKindForTypeName(normalizeBindingTypeName(elemType));
      if (kind != ReturnKind::Unknown) {
        kindOut = kind;
        return true;
      }
    }
    if (resolvers.resolveStringTarget(receiverExpr)) {
      kindOut = ReturnKind::Int;
      return true;
    }
    return false;
  }
  const bool resolvedSoaCanonicalIsGet =
      isLegacyOrCanonicalSoaHelperPath(resolvedSoaCanonical, "get");
  const bool resolvedSoaCanonicalIsGetRef =
      isLegacyOrCanonicalSoaHelperPath(resolvedSoaCanonical, collection_helpers::kGetRef);
  if (resolvedSoaCanonicalIsGet ||
      resolvedSoaCanonicalIsGetRef ||
      isCanonicalSoaRefLikeHelperPath(resolvedSoaCanonical) ||
      isExperimentalSoaBorrowedHelperPath(resolvedSoaCanonical)) {
    std::string elemType;
    if (resolvers.resolveSoaVectorTarget(receiverExpr, elemType)) {
      ReturnKind kind = returnKindForTypeName(normalizeBindingTypeName(elemType));
      if (kind != ReturnKind::Unknown) {
        kindOut = kind;
        return true;
      }
    }
    return false;
  }
  if (resolvedKeyValueHelperName == "at" ||
      resolvedKeyValueHelperName == "at_unsafe" ||
      resolvedKeyValueHelperName == collection_helpers::kAtRef ||
      resolvedKeyValueHelperName == collection_helpers::kAtUnsafeRef) {
    std::string keyType;
    std::string valueType;
    if ((resolvers.resolveMapTarget != nullptr &&
         resolvers.resolveMapTarget(receiverExpr, keyType, valueType)) ||
        (resolvers.resolveKeyValueTarget != nullptr &&
         resolvers.resolveKeyValueTarget(receiverExpr, keyType, valueType))) {
      ReturnKind kind = returnKindForTypeName(normalizeBindingTypeName(valueType));
      if (kind != ReturnKind::Unknown) {
        kindOut = kind;
        return true;
      }
    }
    return false;
  }
  return false;
}

bool SemanticsValidator::resolveBuiltinCollectionAccessCallReturnKind(
    const Expr &callExpr,
    const BuiltinCollectionDispatchResolvers &resolvers,
    ReturnKind &kindOut) const {
  kindOut = ReturnKind::Unknown;
  if (callExpr.isMethodCall || callExpr.args.size() != 2) {
    return false;
  }
  std::string builtinName;
  if (!getBuiltinArrayAccessName(callExpr, builtinName)) {
    const std::string resolvedPath = resolveCalleePath(callExpr);
    std::string resolvedKeyValueHelperName;
    if (!resolveKeyValueHelperResolvedPathForInferCollectionDispatch(
            resolvedPath, resolvedKeyValueHelperName)) {
      return false;
    }
    if (resolvedKeyValueHelperName == collection_helpers::kAtRef) {
      builtinName = collection_helpers::kAtRef;
    } else if (resolvedKeyValueHelperName == collection_helpers::kAtUnsafeRef) {
      builtinName = collection_helpers::kAtUnsafeRef;
    } else {
      return false;
    }
  }

  auto resolveReceiverReturnKind = [&](const Expr &receiverExpr) {
    std::string elemType;
    if (resolvers.resolveStringTarget(receiverExpr)) {
      kindOut = ReturnKind::Int;
      return true;
    }
    std::string keyType;
    std::string valueType;
    if (resolvers.resolveMapTarget(receiverExpr, keyType, valueType) ||
        resolvers.resolveKeyValueTarget(receiverExpr, keyType, valueType)) {
      ReturnKind kind = returnKindForTypeName(normalizeBindingTypeName(valueType));
      if (kind != ReturnKind::Unknown) {
        kindOut = kind;
        return true;
      }
    }
    if (resolvers.resolveVectorTarget(receiverExpr, elemType)) {
      ReturnKind kind = returnKindForTypeName(normalizeBindingTypeName(elemType));
      if (kind != ReturnKind::Unknown) {
        kindOut = kind;
        return true;
      }
    }
    if (resolvers.resolveSoaVectorTarget(receiverExpr, elemType)) {
      ReturnKind kind = returnKindForTypeName(normalizeBindingTypeName(elemType));
      if (kind != ReturnKind::Unknown) {
        kindOut = kind;
        return true;
      }
    }
    if (resolvers.resolveArgsPackAccessTarget(receiverExpr, elemType) ||
        resolvers.resolveArrayTarget(receiverExpr, elemType)) {
      ReturnKind kind = returnKindForTypeName(normalizeBindingTypeName(elemType));
      if (kind != ReturnKind::Unknown) {
        kindOut = kind;
        return true;
      }
    }
    return false;
  };

  auto tryResolveReceiverIndex = [&](size_t index) -> bool {
    if (index >= callExpr.args.size()) {
      return false;
    }
    return resolveReceiverReturnKind(callExpr.args[index]);
  };

  const bool hasNamedArgs = hasNamedArguments(callExpr.argNames);
  if (hasNamedArgs) {
    bool foundValuesReceiver = false;
    for (size_t i = 0; i < callExpr.args.size(); ++i) {
      if (i < callExpr.argNames.size() && callExpr.argNames[i].has_value() &&
          *callExpr.argNames[i] == "values") {
        foundValuesReceiver = true;
        if (tryResolveReceiverIndex(i)) {
          return true;
        }
      }
    }
    if (!foundValuesReceiver && tryResolveReceiverIndex(0)) {
      return true;
    }
    return false;
  }

  if (tryResolveReceiverIndex(0)) {
    return true;
  }
  const bool probePositionalReorderedReceiver =
      callExpr.args.size() > 1 &&
      (callExpr.args.front().kind == Expr::Kind::Literal ||
       callExpr.args.front().kind == Expr::Kind::BoolLiteral ||
       callExpr.args.front().kind == Expr::Kind::FloatLiteral ||
       callExpr.args.front().kind == Expr::Kind::StringLiteral);
  if (probePositionalReorderedReceiver) {
    for (size_t i = 1; i < callExpr.args.size(); ++i) {
      if (tryResolveReceiverIndex(i)) {
        return true;
      }
    }
  }
  return false;
}

} // namespace primec::semantics
