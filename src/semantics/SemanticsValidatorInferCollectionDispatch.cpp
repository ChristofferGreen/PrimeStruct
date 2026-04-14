#include "SemanticsValidator.h"

namespace primec::semantics {

bool SemanticsValidator::resolveBuiltinCollectionMethodReturnKind(
    const std::string &resolvedPath,
    const Expr &receiverExpr,
    const BuiltinCollectionDispatchResolvers &resolvers,
    ReturnKind &kindOut) const {
  std::string resolvedSoaCanonical =
      canonicalizeLegacySoaGetHelperPath(resolvedPath);
  if (resolvedPath == "/array/count" ||
      resolvedPath == "/std/collections/vector/count" || resolvedPath == "/string/count" ||
      resolvedPath == "/map/count" || resolvedPath == "/std/collections/map/count" ||
      resolvedPath == "/std/collections/map/count_ref" ||
      resolvedPath == "/std/collections/vector/capacity") {
    kindOut = ReturnKind::Int;
    return true;
  }
  if (resolvedPath == "/map/contains" || resolvedPath == "/std/collections/map/contains" ||
      resolvedPath == "/std/collections/map/contains_ref") {
    kindOut = ReturnKind::Bool;
    return true;
  }
  if (resolvedPath == "/string/at" || resolvedPath == "/string/at_unsafe") {
    kindOut = ReturnKind::Int;
    return true;
  }
  if (resolvedPath == "/array/at" || resolvedPath == "/array/at_unsafe") {
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
  if (resolvedPath == "/std/collections/vector/at" ||
      resolvedPath == "/std/collections/vector/at_unsafe") {
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
      isLegacyOrCanonicalSoaHelperPath(resolvedSoaCanonical, "get_ref");
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
  if (resolvedPath == "/map/at" || resolvedPath == "/map/at_ref" ||
      resolvedPath == "/map/at_unsafe" || resolvedPath == "/map/at_unsafe_ref" ||
      resolvedPath == "/std/collections/map/at" || resolvedPath == "/std/collections/map/at_unsafe" ||
      resolvedPath == "/std/collections/map/at_ref" ||
      resolvedPath == "/std/collections/map/at_unsafe_ref") {
    std::string keyType;
    std::string valueType;
    if (resolvers.resolveMapTarget(receiverExpr, keyType, valueType)) {
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
    if (resolvedPath == "/map/at_ref" ||
        resolvedPath == "/std/collections/map/at_ref") {
      builtinName = "at_ref";
    } else if (resolvedPath == "/map/at_unsafe_ref" ||
               resolvedPath == "/std/collections/map/at_unsafe_ref") {
      builtinName = "at_unsafe_ref";
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
        resolvers.resolveExperimentalMapTarget(receiverExpr, keyType, valueType)) {
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
