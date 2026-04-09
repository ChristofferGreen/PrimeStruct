#include "SemanticsValidator.h"

namespace primec::semantics {

bool SemanticsValidator::resolveBuiltinCollectionMethodReturnKind(
    const std::string &resolvedPath,
    const Expr &receiverExpr,
    const BuiltinCollectionDispatchResolvers &resolvers,
    ReturnKind &kindOut) const {
  if (resolvedPath == "/array/count" || resolvedPath == "/vector/count" ||
      resolvedPath == "/std/collections/vector/count" || resolvedPath == "/string/count" ||
      resolvedPath == "/map/count" || resolvedPath == "/std/collections/map/count" ||
      resolvedPath == "/std/collections/map/count_ref" ||
      resolvedPath == "/vector/capacity" || resolvedPath == "/std/collections/vector/capacity") {
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
  if (resolvedPath == "/vector/at" || resolvedPath == "/vector/at_unsafe" ||
      resolvedPath == "/std/collections/vector/at" ||
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
  if (resolvedPath == "/soa_vector/get" || resolvedPath == "/soa_vector/get_ref" ||
      resolvedPath == "/soa_vector/ref" || resolvedPath == "/soa_vector/ref_ref" ||
      resolvedPath == "/std/collections/soa_vector/get" ||
      resolvedPath == "/std/collections/soa_vector/get_ref" ||
      resolvedPath == "/std/collections/soa_vector/ref" ||
      resolvedPath == "/std/collections/soa_vector/ref_ref" ||
      resolvedPath == "/std/collections/experimental_soa_vector/soaVectorGetRef" ||
      resolvedPath == "/std/collections/experimental_soa_vector/soaVectorRefRef") {
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

  std::vector<size_t> receiverIndices;
  auto appendReceiverIndex = [&](size_t index) {
    if (index >= callExpr.args.size()) {
      return;
    }
    for (size_t existing : receiverIndices) {
      if (existing == index) {
        return;
      }
    }
    receiverIndices.push_back(index);
  };

  const bool hasNamedArgs = hasNamedArguments(callExpr.argNames);
  if (hasNamedArgs) {
    bool foundValuesReceiver = false;
    for (size_t i = 0; i < callExpr.args.size(); ++i) {
      if (i < callExpr.argNames.size() && callExpr.argNames[i].has_value() &&
          *callExpr.argNames[i] == "values") {
        appendReceiverIndex(i);
        foundValuesReceiver = true;
      }
    }
    if (!foundValuesReceiver) {
      appendReceiverIndex(0);
    }
  } else {
    appendReceiverIndex(0);
    const bool probePositionalReorderedReceiver =
        callExpr.args.size() > 1 &&
        (callExpr.args.front().kind == Expr::Kind::Literal ||
         callExpr.args.front().kind == Expr::Kind::BoolLiteral ||
         callExpr.args.front().kind == Expr::Kind::FloatLiteral ||
         callExpr.args.front().kind == Expr::Kind::StringLiteral);
    if (probePositionalReorderedReceiver) {
      for (size_t i = 1; i < callExpr.args.size(); ++i) {
        appendReceiverIndex(i);
      }
    }
  }

  for (size_t receiverIndex : receiverIndices) {
    if (resolveReceiverReturnKind(callExpr.args[receiverIndex])) {
      return true;
    }
  }
  return false;
}

} // namespace primec::semantics
