#include "SemanticsValidator.h"

namespace primec::semantics {

bool SemanticsValidator::resolveBuiltinCollectionMethodReturnKind(
    const std::string &resolvedPath,
    const Expr &receiverExpr,
    const BuiltinCollectionDispatchResolvers &resolvers,
    ReturnKind &kindOut) const {
  if (resolvedPath == "/array/count" || resolvedPath == "/vector/count" || resolvedPath == "/string/count" ||
      resolvedPath == "/map/count" || resolvedPath == "/std/collections/map/count" ||
      resolvedPath == "/vector/capacity") {
    kindOut = ReturnKind::Int;
    return true;
  }
  if (resolvedPath == "/map/contains" || resolvedPath == "/std/collections/map/contains") {
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
  if (resolvedPath == "/vector/at" || resolvedPath == "/vector/at_unsafe") {
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
  if (resolvedPath == "/map/at" || resolvedPath == "/map/at_unsafe" ||
      resolvedPath == "/std/collections/map/at" || resolvedPath == "/std/collections/map/at_unsafe") {
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
    return false;
  }

  std::string elemType;
  if (resolvers.resolveStringTarget(callExpr.args.front())) {
    kindOut = ReturnKind::Int;
    return true;
  }
  std::string keyType;
  std::string valueType;
  if (resolvers.resolveMapTarget(callExpr.args.front(), keyType, valueType)) {
    ReturnKind kind = returnKindForTypeName(normalizeBindingTypeName(valueType));
    if (kind != ReturnKind::Unknown) {
      kindOut = kind;
      return true;
    }
  }
  if (resolvers.resolveVectorTarget(callExpr.args.front(), elemType)) {
    ReturnKind kind = returnKindForTypeName(normalizeBindingTypeName(elemType));
    if (kind != ReturnKind::Unknown) {
      kindOut = kind;
      return true;
    }
  }
  if (resolvers.resolveArgsPackAccessTarget(callExpr.args.front(), elemType) ||
      resolvers.resolveArrayTarget(callExpr.args.front(), elemType)) {
    ReturnKind kind = returnKindForTypeName(normalizeBindingTypeName(elemType));
    if (kind != ReturnKind::Unknown) {
      kindOut = kind;
      return true;
    }
  }
  return false;
}

} // namespace primec::semantics
