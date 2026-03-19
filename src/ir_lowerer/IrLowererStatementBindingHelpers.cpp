#include "IrLowererStatementBindingHelpers.h"

#include "IrLowererBindingTransformHelpers.h"
#include "IrLowererBindingTypeHelpers.h"
#include "IrLowererHelpers.h"
#include "IrLowererIndexKindHelpers.h"
#include "IrLowererSetupTypeHelpers.h"
#include "IrLowererStringCallHelpers.h"
#include "IrLowererStringLiteralHelpers.h"
#include "IrLowererTemplateTypeParseHelpers.h"
#include "IrLowererUninitializedTypeHelpers.h"

namespace primec::ir_lowerer {

namespace {

bool hasSoaVectorTypeTransform(const Expr &expr) {
  for (const auto &transform : expr.transforms) {
    if (transform.name == "soa_vector") {
      return true;
    }
  }
  return false;
}

bool extractArgsPackElementTypeText(const Expr &expr, std::string &typeTextOut) {
  typeTextOut.clear();
  for (const auto &transform : expr.transforms) {
    if (transform.name == "effects" || transform.name == "capabilities") {
      continue;
    }
    if (isBindingQualifierName(transform.name)) {
      continue;
    }
    if (!transform.arguments.empty()) {
      continue;
    }
    if (transform.name != "args" || transform.templateArgs.size() != 1) {
      return false;
    }
    typeTextOut = trimTemplateTypeText(transform.templateArgs.front());
    return !typeTextOut.empty();
  }
  return false;
}

void applyArgsPackElementMetadata(const std::string &typeText, LocalInfo &infoOut) {
  if (trimTemplateTypeText(typeText) == "FileError") {
    infoOut.isFileError = true;
    infoOut.valueKind = LocalInfo::ValueKind::Int32;
    return;
  }

  bool resultHasValue = false;
  LocalInfo::ValueKind resultValueKind = LocalInfo::ValueKind::Unknown;
  std::string resultErrorType;
  if (parseResultTypeName(typeText, resultHasValue, resultValueKind, resultErrorType)) {
    infoOut.isResult = true;
    infoOut.resultHasValue = resultHasValue;
    infoOut.resultValueKind = resultValueKind;
    infoOut.resultErrorType = resultErrorType;
    infoOut.valueKind = resultHasValue ? LocalInfo::ValueKind::Int64 : LocalInfo::ValueKind::Int32;
    return;
  }

  std::string base;
  std::string arg;
  if (!splitTemplateTypeName(typeText, base, arg)) {
    return;
  }
  base = normalizeCollectionBindingTypeName(base);
  if (base == "File") {
    infoOut.isFileHandle = true;
    infoOut.valueKind = LocalInfo::ValueKind::Int64;
    return;
  }
  if (base == "Buffer") {
    infoOut.argsPackElementKind = LocalInfo::Kind::Buffer;
    infoOut.valueKind = valueKindFromTypeName(trimTemplateTypeText(arg));
    return;
  }
  if (base == "array") {
    infoOut.argsPackElementKind = LocalInfo::Kind::Array;
    infoOut.valueKind = valueKindFromTypeName(trimTemplateTypeText(arg));
    return;
  }
  if (base == "Pointer") {
    infoOut.argsPackElementKind = LocalInfo::Kind::Pointer;
    std::string pointerTargetType = trimTemplateTypeText(arg);
    if (extractTopLevelUninitializedTypeText(pointerTargetType, pointerTargetType)) {
      infoOut.targetsUninitializedStorage = true;
    }
    std::string pointerBase;
    std::string pointerArg;
    if (splitTemplateTypeName(pointerTargetType, pointerBase, pointerArg) &&
        normalizeCollectionBindingTypeName(pointerBase) == "File") {
      infoOut.isFileHandle = true;
      infoOut.valueKind = LocalInfo::ValueKind::Int64;
      return;
    }
    if (splitTemplateTypeName(pointerTargetType, pointerBase, pointerArg) &&
        normalizeCollectionBindingTypeName(pointerBase) == "array") {
      infoOut.pointerToArray = true;
      infoOut.valueKind = valueKindFromTypeName(trimTemplateTypeText(pointerArg));
      return;
    }
    if (splitTemplateTypeName(pointerTargetType, pointerBase, pointerArg) &&
        normalizeCollectionBindingTypeName(pointerBase) == "vector") {
      infoOut.pointerToVector = true;
      infoOut.valueKind = valueKindFromTypeName(trimTemplateTypeText(pointerArg));
      return;
    }
    if (splitTemplateTypeName(pointerTargetType, pointerBase, pointerArg) &&
        normalizeCollectionBindingTypeName(pointerBase) == "soa_vector") {
      infoOut.pointerToVector = true;
      infoOut.isSoaVector = true;
      infoOut.valueKind = valueKindFromTypeName(trimTemplateTypeText(pointerArg));
      return;
    }
    if (splitTemplateTypeName(pointerTargetType, pointerBase, pointerArg) &&
        normalizeCollectionBindingTypeName(pointerBase) == "map") {
      std::vector<std::string> args;
      if (!splitTemplateArgs(pointerArg, args) || args.size() != 2) {
        return;
      }
      infoOut.pointerToMap = true;
      infoOut.mapKeyKind = valueKindFromTypeName(trimTemplateTypeText(args[0]));
      infoOut.mapValueKind = valueKindFromTypeName(trimTemplateTypeText(args[1]));
      infoOut.valueKind = infoOut.mapValueKind;
      return;
    }
    if (splitTemplateTypeName(pointerTargetType, pointerBase, pointerArg) &&
        normalizeCollectionBindingTypeName(pointerBase) == "Buffer") {
      infoOut.pointerToBuffer = true;
      infoOut.valueKind = valueKindFromTypeName(trimTemplateTypeText(pointerArg));
      return;
    }
    if (pointerTargetType == "FileError") {
      infoOut.isFileError = true;
      infoOut.valueKind = LocalInfo::ValueKind::Int32;
      return;
    }
    bool pointerResultHasValue = false;
    LocalInfo::ValueKind pointerResultValueKind = LocalInfo::ValueKind::Unknown;
    std::string pointerResultErrorType;
    if (parseResultTypeName(pointerTargetType,
                            pointerResultHasValue,
                            pointerResultValueKind,
                            pointerResultErrorType)) {
      infoOut.isResult = true;
      infoOut.resultHasValue = pointerResultHasValue;
      infoOut.resultValueKind = pointerResultValueKind;
      infoOut.resultErrorType = pointerResultErrorType;
      infoOut.valueKind = pointerResultHasValue ? LocalInfo::ValueKind::Int64 : LocalInfo::ValueKind::Int32;
      return;
    }
    const LocalInfo::ValueKind pointerValueKind = valueKindFromTypeName(pointerTargetType);
    if (pointerValueKind != LocalInfo::ValueKind::Unknown) {
      infoOut.valueKind = pointerValueKind;
    }
    return;
  }
  if (base == "Reference") {
    std::string refBase;
    std::string refArg;
    infoOut.argsPackElementKind = LocalInfo::Kind::Reference;
    std::string refTargetType = trimTemplateTypeText(arg);
    if (extractTopLevelUninitializedTypeText(refTargetType, refTargetType)) {
      infoOut.targetsUninitializedStorage = true;
    }
    bool refResultHasValue = false;
    LocalInfo::ValueKind refResultValueKind = LocalInfo::ValueKind::Unknown;
    std::string refResultErrorType;
    if (parseResultTypeName(refTargetType, refResultHasValue, refResultValueKind, refResultErrorType)) {
      infoOut.isResult = true;
      infoOut.resultHasValue = refResultHasValue;
      infoOut.resultValueKind = refResultValueKind;
      infoOut.resultErrorType = refResultErrorType;
      infoOut.valueKind = refResultHasValue ? LocalInfo::ValueKind::Int64 : LocalInfo::ValueKind::Int32;
      return;
    }
    if (!splitTemplateTypeName(refTargetType, refBase, refArg)) {
      if (refTargetType == "FileError") {
        infoOut.isFileError = true;
      }
      const LocalInfo::ValueKind refValueKind = valueKindFromTypeName(refTargetType);
      if (refValueKind != LocalInfo::ValueKind::Unknown) {
        infoOut.valueKind = refValueKind;
      }
      return;
    }
    refBase = normalizeCollectionBindingTypeName(refBase);
    if (refBase == "array") {
      infoOut.argsPackElementKind = LocalInfo::Kind::Reference;
      infoOut.referenceToArray = true;
      infoOut.valueKind = valueKindFromTypeName(trimTemplateTypeText(refArg));
      return;
    }
    if (refBase == "vector") {
      infoOut.argsPackElementKind = LocalInfo::Kind::Reference;
      infoOut.referenceToVector = true;
      infoOut.valueKind = valueKindFromTypeName(trimTemplateTypeText(refArg));
      return;
    }
    if (refBase == "soa_vector") {
      infoOut.argsPackElementKind = LocalInfo::Kind::Reference;
      infoOut.referenceToVector = true;
      infoOut.isSoaVector = true;
      infoOut.valueKind = valueKindFromTypeName(trimTemplateTypeText(refArg));
      return;
    }
    if (refBase == "map") {
      std::vector<std::string> args;
      if (!splitTemplateArgs(refArg, args) || args.size() != 2) {
        return;
      }
      infoOut.argsPackElementKind = LocalInfo::Kind::Reference;
      infoOut.referenceToMap = true;
      infoOut.mapKeyKind = valueKindFromTypeName(trimTemplateTypeText(args[0]));
      infoOut.mapValueKind = valueKindFromTypeName(trimTemplateTypeText(args[1]));
      infoOut.valueKind = infoOut.mapValueKind;
      return;
    }
    if (refBase == "File") {
      infoOut.argsPackElementKind = LocalInfo::Kind::Reference;
      infoOut.isFileHandle = true;
      infoOut.valueKind = LocalInfo::ValueKind::Int64;
      return;
    }
    if (refBase == "Buffer") {
      infoOut.argsPackElementKind = LocalInfo::Kind::Reference;
      infoOut.referenceToBuffer = true;
      infoOut.valueKind = valueKindFromTypeName(trimTemplateTypeText(refArg));
      return;
    }
    const LocalInfo::ValueKind refValueKind = valueKindFromTypeName(trimTemplateTypeText(arg));
    if (refValueKind != LocalInfo::ValueKind::Unknown) {
      infoOut.valueKind = refValueKind;
    }
    return;
  }
  if (base == "vector") {
    infoOut.argsPackElementKind = LocalInfo::Kind::Vector;
    infoOut.valueKind = valueKindFromTypeName(trimTemplateTypeText(arg));
    return;
  }
  if (base == "soa_vector") {
    infoOut.argsPackElementKind = LocalInfo::Kind::Vector;
    infoOut.isSoaVector = true;
    infoOut.valueKind = valueKindFromTypeName(trimTemplateTypeText(arg));
    return;
  }
  if (base == "map") {
    std::vector<std::string> args;
    if (!splitTemplateArgs(arg, args) || args.size() != 2) {
      return;
    }
    infoOut.argsPackElementKind = LocalInfo::Kind::Map;
    infoOut.mapKeyKind = valueKindFromTypeName(trimTemplateTypeText(args[0]));
    infoOut.mapValueKind = valueKindFromTypeName(trimTemplateTypeText(args[1]));
    infoOut.valueKind = infoOut.mapValueKind;
  }
}

void applyArgsPackElementStructMetadata(const Expr &param,
                                        const std::string &elementTypeText,
                                        const ApplyStructBindingInfoFn &applyStructArrayInfo,
                                        const ApplyStructBindingInfoFn &applyStructValueInfo,
                                        LocalInfo &infoOut) {
  if (elementTypeText.empty() || !infoOut.structTypeName.empty()) {
    return;
  }

  Expr syntheticBinding;
  syntheticBinding.namespacePrefix = param.namespacePrefix;

  Transform syntheticTransform;
  std::string base;
  std::string arg;
  if (splitTemplateTypeName(elementTypeText, base, arg)) {
    syntheticTransform.name = trimTemplateTypeText(base);
    if (!arg.empty()) {
      std::vector<std::string> templateArgs;
      if (splitTemplateArgs(arg, templateArgs)) {
        syntheticTransform.templateArgs = std::move(templateArgs);
      } else {
        syntheticTransform.templateArgs.push_back(trimTemplateTypeText(arg));
      }
    }
  } else {
    syntheticTransform.name = trimTemplateTypeText(elementTypeText);
  }
  syntheticBinding.transforms.push_back(std::move(syntheticTransform));

  LocalInfo elementInfo = infoOut;
  elementInfo.kind = (infoOut.argsPackElementKind == LocalInfo::Kind::Value) ? LocalInfo::Kind::Value
                                                                              : infoOut.argsPackElementKind;
  elementInfo.isArgsPack = false;
  applyStructArrayInfo(syntheticBinding, elementInfo);
  applyStructValueInfo(syntheticBinding, elementInfo);
  if (!elementInfo.structTypeName.empty()) {
    infoOut.structTypeName = elementInfo.structTypeName;
  }
}

bool isPointerMemoryIntrinsicCallImpl(const Expr &expr) {
  std::string builtinName;
  if (expr.kind != Expr::Kind::Call || !getBuiltinMemoryName(expr, builtinName)) {
    return false;
  }
  return builtinName == "alloc" || builtinName == "realloc" || builtinName == "at" ||
         builtinName == "at_unsafe";
}

LocalInfo::ValueKind inferPointerMemoryIntrinsicValueKind(const Expr &expr,
                                                          const LocalMap &localsIn,
                                                          const InferBindingExprKindFn &inferExprKind) {
  std::string builtinName;
  if (expr.kind != Expr::Kind::Call || !getBuiltinMemoryName(expr, builtinName)) {
    return LocalInfo::ValueKind::Unknown;
  }
  if (builtinName == "alloc" && expr.templateArgs.size() == 1) {
    return valueKindFromTypeName(unwrapTopLevelUninitializedTypeText(expr.templateArgs.front()));
  }
  if ((builtinName == "realloc" && expr.args.size() == 2) || (builtinName == "at" && expr.args.size() == 3) ||
      (builtinName == "at_unsafe" && expr.args.size() == 2)) {
    if (expr.args.front().kind == Expr::Kind::Name) {
      auto it = localsIn.find(expr.args.front().name);
      if (it != localsIn.end() &&
          (it->second.kind == LocalInfo::Kind::Pointer || it->second.kind == LocalInfo::Kind::Reference)) {
        return it->second.valueKind;
      }
    }
    return inferExprKind(expr.args.front(), localsIn);
  }
  return LocalInfo::ValueKind::Unknown;
}

std::string inferPointerMemoryIntrinsicStructType(const Expr &expr, const LocalMap &localsIn) {
  std::string builtinName;
  if (expr.kind != Expr::Kind::Call || !getBuiltinMemoryName(expr, builtinName)) {
    return "";
  }
  if (builtinName == "realloc" && expr.args.size() == 2 && expr.args.front().kind == Expr::Kind::Name) {
    auto it = localsIn.find(expr.args.front().name);
    if (it != localsIn.end() &&
        (it->second.kind == LocalInfo::Kind::Pointer || it->second.kind == LocalInfo::Kind::Reference)) {
      return it->second.structTypeName;
    }
  }
  if ((builtinName == "at" && expr.args.size() == 3 && expr.args.front().kind == Expr::Kind::Name) ||
      (builtinName == "at_unsafe" && expr.args.size() == 2 && expr.args.front().kind == Expr::Kind::Name)) {
    auto it = localsIn.find(expr.args.front().name);
    if (it != localsIn.end() &&
        (it->second.kind == LocalInfo::Kind::Pointer || it->second.kind == LocalInfo::Kind::Reference)) {
      return it->second.structTypeName;
    }
  }
  return "";
}

bool inferPointerMemoryIntrinsicTargetsUninitializedStorageImpl(const Expr &expr, const LocalMap &localsIn) {
  std::string builtinName;
  if (expr.kind != Expr::Kind::Call || !getBuiltinMemoryName(expr, builtinName)) {
    return false;
  }
  if (builtinName == "alloc" && expr.templateArgs.size() == 1) {
    std::string targetType;
    return extractTopLevelUninitializedTypeText(expr.templateArgs.front(), targetType);
  }
  if ((builtinName == "realloc" && expr.args.size() == 2) || (builtinName == "at" && expr.args.size() == 3) ||
      (builtinName == "at_unsafe" && expr.args.size() == 2)) {
    if (expr.args.front().kind != Expr::Kind::Name) {
      return false;
    }
    auto it = localsIn.find(expr.args.front().name);
    return it != localsIn.end() && it->second.targetsUninitializedStorage;
  }
  return false;
}

LocalInfo::ValueKind inferSpecialCallValueKind(const Expr &expr) {
  if (!(expr.kind == Expr::Kind::Call && expr.isMethodCall && !expr.args.empty() &&
        expr.args.front().kind == Expr::Kind::Name && expr.args.front().name == "Result")) {
    return LocalInfo::ValueKind::Unknown;
  }
  if (expr.name == "ok") {
    return expr.args.size() > 1 ? LocalInfo::ValueKind::Int64 : LocalInfo::ValueKind::Int32;
  }
  if (expr.name == "error") {
    return LocalInfo::ValueKind::Bool;
  }
  if (expr.name == "why") {
    return LocalInfo::ValueKind::String;
  }
  return LocalInfo::ValueKind::Unknown;
}

} // namespace

bool inferPointerMemoryIntrinsicTargetsUninitializedStorage(const Expr &expr, const LocalMap &localsIn) {
  return inferPointerMemoryIntrinsicTargetsUninitializedStorageImpl(expr, localsIn);
}

bool isPointerMemoryIntrinsicCall(const Expr &expr) {
  return isPointerMemoryIntrinsicCallImpl(expr);
}

StatementBindingTypeInfo inferStatementBindingTypeInfo(const Expr &stmt,
                                                       const Expr &init,
                                                       const LocalMap &localsIn,
                                                       const HasExplicitBindingTypeTransformFn &hasExplicitBindingTypeTransform,
                                                       const BindingKindFn &bindingKind,
                                                       const BindingValueKindFn &bindingValueKind,
                                                       const InferBindingExprKindFn &inferExprKind) {
  StatementBindingTypeInfo info;
  info.kind = bindingKind(stmt);
  const bool hasExplicitType = hasExplicitBindingTypeTransform(stmt);
  auto isRawStructBufferInit = [&](const Expr &candidate) {
    if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall || candidate.isBinding) {
      return false;
    }
    return candidate.name == "Buffer" || candidate.name == "/std/gfx/Buffer" ||
           candidate.name == "/std/gfx/experimental/Buffer" ||
           candidate.name.rfind("/std/gfx/Buffer__t", 0) == 0 ||
           candidate.name.rfind("/std/gfx/experimental/Buffer__t", 0) == 0;
  };
  if (hasExplicitType && info.kind == LocalInfo::Kind::Buffer && isRawStructBufferInit(init)) {
    info.kind = LocalInfo::Kind::Value;
  }
  LocalInfo::ValueKind inferredInitValueKind = LocalInfo::ValueKind::Unknown;
  if (!hasExplicitType && info.kind == LocalInfo::Kind::Value) {
      if (init.kind == Expr::Kind::Name) {
        auto it = localsIn.find(init.name);
        if (it != localsIn.end()) {
          info.kind = it->second.kind;
        }
      } else if (init.kind == Expr::Kind::Call) {
        inferredInitValueKind = inferExprKind(init, localsIn);
        if (isPointerMemoryIntrinsicCall(init)) {
          info.kind = LocalInfo::Kind::Pointer;
        } else if (inferredInitValueKind == LocalInfo::ValueKind::Unknown) {
        std::string collection;
        if (getBuiltinCollectionName(init, collection)) {
          if (collection == "array") {
            info.kind = LocalInfo::Kind::Array;
          } else if (collection == "vector") {
            info.kind = LocalInfo::Kind::Vector;
          } else if (collection == "map") {
            info.kind = LocalInfo::Kind::Map;
          }
        }
      }
    }
  }

  if (info.kind == LocalInfo::Kind::Map) {
    if (hasExplicitType) {
      for (const auto &transform : stmt.transforms) {
        const std::string normalizedName = normalizeDeclaredCollectionTypeBase(transform.name);
        if (normalizedName == "map" && transform.templateArgs.size() == 2) {
          info.mapKeyKind = valueKindFromTypeName(transform.templateArgs[0]);
          info.mapValueKind = valueKindFromTypeName(transform.templateArgs[1]);
          break;
        }
      }
    } else if (init.kind == Expr::Kind::Name) {
      auto it = localsIn.find(init.name);
      if (it != localsIn.end() && it->second.kind == LocalInfo::Kind::Map) {
        info.mapKeyKind = it->second.mapKeyKind;
        info.mapValueKind = it->second.mapValueKind;
      }
    } else if (init.kind == Expr::Kind::Call) {
      std::string collection;
      if (getBuiltinCollectionName(init, collection) && collection == "map" && init.templateArgs.size() == 2) {
        info.mapKeyKind = valueKindFromTypeName(init.templateArgs[0]);
        info.mapValueKind = valueKindFromTypeName(init.templateArgs[1]);
      }
    }
    info.valueKind = info.mapValueKind;
    return info;
  }

  if (hasExplicitType) {
    info.valueKind = bindingValueKind(stmt, info.kind);
    return info;
  }

  if (info.kind == LocalInfo::Kind::Value) {
    const LocalInfo::ValueKind specialInitValueKind = inferSpecialCallValueKind(init);
    info.valueKind = (specialInitValueKind != LocalInfo::ValueKind::Unknown)
                         ? specialInitValueKind
                         : ((inferredInitValueKind != LocalInfo::ValueKind::Unknown)
                                ? inferredInitValueKind
                                : inferExprKind(init, localsIn));
    return info;
  }

  if (info.kind == LocalInfo::Kind::Pointer || info.kind == LocalInfo::Kind::Reference) {
    if (init.kind == Expr::Kind::Name) {
      auto it = localsIn.find(init.name);
      if (it != localsIn.end() && (it->second.kind == LocalInfo::Kind::Pointer || it->second.kind == LocalInfo::Kind::Reference)) {
        info.valueKind = it->second.valueKind;
      }
      info.structTypeName = (it != localsIn.end()) ? it->second.structTypeName : "";
    } else if (info.kind == LocalInfo::Kind::Pointer && init.kind == Expr::Kind::Call &&
               isPointerMemoryIntrinsicCall(init)) {
      info.valueKind = inferPointerMemoryIntrinsicValueKind(init, localsIn, inferExprKind);
      info.structTypeName = inferPointerMemoryIntrinsicStructType(init, localsIn);
    }
    return info;
  }

  if (info.kind == LocalInfo::Kind::Array || info.kind == LocalInfo::Kind::Vector) {
    if (init.kind == Expr::Kind::Name) {
      auto it = localsIn.find(init.name);
      if (it != localsIn.end() && (it->second.kind == LocalInfo::Kind::Array || it->second.kind == LocalInfo::Kind::Vector)) {
        info.valueKind = it->second.valueKind;
      }
    } else if (init.kind == Expr::Kind::Call) {
      std::string collection;
      if (getBuiltinCollectionName(init, collection) && (collection == "array" || collection == "vector") &&
          init.templateArgs.size() == 1) {
        info.valueKind = valueKindFromTypeName(init.templateArgs.front());
      }
    }
  }

  return info;
}

bool inferCallParameterLocalInfo(const Expr &param,
                                 const LocalMap &localsForKindInference,
                                 const IsBindingMutableFn &isBindingMutable,
                                 const HasExplicitBindingTypeTransformFn &hasExplicitBindingTypeTransform,
                                 const BindingKindFn &bindingKind,
                                 const BindingValueKindFn &bindingValueKind,
                                 const InferBindingExprKindFn &inferExprKind,
                                 const IsFileErrorBindingFn &isFileErrorBinding,
                                 const SetReferenceArrayInfoForBindingFn &setReferenceArrayInfo,
                                 const ApplyStructBindingInfoFn &applyStructArrayInfo,
                                 const ApplyStructBindingInfoFn &applyStructValueInfo,
                                 const IsStringBindingFn &isStringBinding,
                                 LocalInfo &infoOut,
                                 std::string &error) {
  infoOut.isMutable = isBindingMutable(param);
  infoOut.isSoaVector = hasSoaVectorTypeTransform(param);
  infoOut.isArgsPack = isArgsPackBinding(param);
  infoOut.kind = bindingKind(param);
  if (hasExplicitBindingTypeTransform(param)) {
    infoOut.valueKind = bindingValueKind(param, infoOut.kind);
  } else if (param.args.size() == 1 && infoOut.kind == LocalInfo::Kind::Value &&
             isPointerMemoryIntrinsicCall(param.args.front())) {
    infoOut.kind = LocalInfo::Kind::Pointer;
    infoOut.valueKind = inferPointerMemoryIntrinsicValueKind(param.args.front(), localsForKindInference, inferExprKind);
    infoOut.structTypeName = inferPointerMemoryIntrinsicStructType(param.args.front(), localsForKindInference);
    infoOut.targetsUninitializedStorage =
        inferPointerMemoryIntrinsicTargetsUninitializedStorage(param.args.front(), localsForKindInference);
  } else if (param.args.size() == 1 && infoOut.kind == LocalInfo::Kind::Value) {
    infoOut.valueKind = inferExprKind(param.args.front(), localsForKindInference);
    if (infoOut.valueKind == LocalInfo::ValueKind::Unknown) {
      infoOut.valueKind = LocalInfo::ValueKind::Int32;
    }
  } else {
    infoOut.valueKind = bindingValueKind(param, infoOut.kind);
  }

  if (infoOut.kind == LocalInfo::Kind::Map) {
    for (const auto &transform : param.transforms) {
      const std::string normalizedName = normalizeDeclaredCollectionTypeBase(transform.name);
      if (normalizedName == "map" && transform.templateArgs.size() == 2) {
        infoOut.mapKeyKind = valueKindFromTypeName(transform.templateArgs[0]);
        infoOut.mapValueKind = valueKindFromTypeName(transform.templateArgs[1]);
        break;
      }
    }
  }
  for (const auto &transform : param.transforms) {
    if (transform.name == "File") {
      infoOut.isFileHandle = true;
      infoOut.valueKind = LocalInfo::ValueKind::Int64;
    } else if (transform.name == "Result") {
      infoOut.isResult = true;
      infoOut.resultHasValue = (transform.templateArgs.size() == 2);
      infoOut.resultValueKind =
          infoOut.resultHasValue ? valueKindFromTypeName(transform.templateArgs.front())
                                 : LocalInfo::ValueKind::Unknown;
      infoOut.valueKind = infoOut.resultHasValue ? LocalInfo::ValueKind::Int64 : LocalInfo::ValueKind::Int32;
      if (!transform.templateArgs.empty()) {
        infoOut.resultErrorType = transform.templateArgs.back();
      }
    } else if ((transform.name == "Reference" || transform.name == "Pointer") && transform.templateArgs.size() == 1) {
      const std::string originalTargetType = trimTemplateTypeText(transform.templateArgs.front());
      std::string targetType = originalTargetType;
      if (extractTopLevelUninitializedTypeText(originalTargetType, targetType)) {
        infoOut.targetsUninitializedStorage = true;
      }
      std::string wrappedBase;
      std::string wrappedArg;
      if (splitTemplateTypeName(targetType, wrappedBase, wrappedArg) &&
          normalizeCollectionBindingTypeName(wrappedBase) == "File") {
        infoOut.isFileHandle = true;
        infoOut.valueKind = LocalInfo::ValueKind::Int64;
      }
      if (transform.name == "Pointer" &&
          splitTemplateTypeName(targetType, wrappedBase, wrappedArg) &&
          normalizeCollectionBindingTypeName(wrappedBase) == "array") {
        infoOut.pointerToArray = true;
        infoOut.valueKind = valueKindFromTypeName(trimTemplateTypeText(wrappedArg));
      }
      if (transform.name == "Pointer" &&
          splitTemplateTypeName(targetType, wrappedBase, wrappedArg) &&
          normalizeCollectionBindingTypeName(wrappedBase) == "vector") {
        infoOut.pointerToVector = true;
        infoOut.valueKind = valueKindFromTypeName(trimTemplateTypeText(wrappedArg));
      }
      if (transform.name == "Pointer" &&
          splitTemplateTypeName(targetType, wrappedBase, wrappedArg) &&
          normalizeCollectionBindingTypeName(wrappedBase) == "soa_vector") {
        infoOut.pointerToVector = true;
        infoOut.isSoaVector = true;
        infoOut.valueKind = valueKindFromTypeName(trimTemplateTypeText(wrappedArg));
      }
      if (transform.name == "Pointer" &&
          splitTemplateTypeName(targetType, wrappedBase, wrappedArg) &&
          normalizeCollectionBindingTypeName(wrappedBase) == "map") {
        std::vector<std::string> args;
        if (splitTemplateArgs(wrappedArg, args) && args.size() == 2) {
          infoOut.pointerToMap = true;
          infoOut.mapKeyKind = valueKindFromTypeName(trimTemplateTypeText(args[0]));
          infoOut.mapValueKind = valueKindFromTypeName(trimTemplateTypeText(args[1]));
          infoOut.valueKind = infoOut.mapValueKind;
        }
      }
      if (transform.name == "Pointer" &&
          splitTemplateTypeName(targetType, wrappedBase, wrappedArg) &&
          normalizeCollectionBindingTypeName(wrappedBase) == "Buffer") {
        infoOut.pointerToBuffer = true;
        infoOut.valueKind = valueKindFromTypeName(trimTemplateTypeText(wrappedArg));
      }
      bool resultHasValue = false;
      LocalInfo::ValueKind resultValueKind = LocalInfo::ValueKind::Unknown;
      std::string resultErrorType;
      if (parseResultTypeName(targetType, resultHasValue, resultValueKind, resultErrorType)) {
        infoOut.isResult = true;
        infoOut.resultHasValue = resultHasValue;
        infoOut.resultValueKind = resultValueKind;
        infoOut.valueKind = resultHasValue ? LocalInfo::ValueKind::Int64 : LocalInfo::ValueKind::Int32;
        infoOut.resultErrorType = resultErrorType;
      }
    }
  }

  if (infoOut.isArgsPack) {
    std::string elementTypeText;
    if (extractArgsPackElementTypeText(param, elementTypeText)) {
      applyArgsPackElementMetadata(elementTypeText, infoOut);
      applyArgsPackElementStructMetadata(param, elementTypeText, applyStructArrayInfo, applyStructValueInfo, infoOut);
    }
  }

  infoOut.isFileError = infoOut.isFileError || isFileErrorBinding(param);
  setReferenceArrayInfo(param, infoOut);
  applyStructArrayInfo(param, infoOut);
  applyStructValueInfo(param, infoOut);
  if (infoOut.kind == LocalInfo::Kind::Value && !infoOut.structTypeName.empty()) {
    infoOut.valueKind = LocalInfo::ValueKind::Int64;
  }
  if (!isStringBinding(param)) {
    return true;
  }
  if (infoOut.kind != LocalInfo::Kind::Value && infoOut.kind != LocalInfo::Kind::Map) {
    error = "native backend does not support string pointers or references";
    return false;
  }
  infoOut.valueKind = LocalInfo::ValueKind::String;
  infoOut.stringSource = LocalInfo::StringSource::RuntimeIndex;
  infoOut.stringIndex = -1;
  infoOut.argvChecked = true;
  return true;
}

bool selectUninitializedStorageZeroInstruction(LocalInfo::Kind kind,
                                               LocalInfo::ValueKind valueKind,
                                               const std::string &bindingName,
                                               IrOpcode &zeroOp,
                                               uint64_t &zeroImm,
                                               std::string &error) {
  zeroOp = IrOpcode::PushI32;
  zeroImm = 0;
  if (kind == LocalInfo::Kind::Array || kind == LocalInfo::Kind::Vector || kind == LocalInfo::Kind::Map ||
      kind == LocalInfo::Kind::Buffer) {
    zeroOp = IrOpcode::PushI64;
    return true;
  }

  switch (valueKind) {
    case LocalInfo::ValueKind::Int64:
    case LocalInfo::ValueKind::UInt64:
      zeroOp = IrOpcode::PushI64;
      return true;
    case LocalInfo::ValueKind::Float32:
      zeroOp = IrOpcode::PushF32;
      return true;
    case LocalInfo::ValueKind::Float64:
      zeroOp = IrOpcode::PushF64;
      return true;
    case LocalInfo::ValueKind::Int32:
    case LocalInfo::ValueKind::Bool:
      zeroOp = IrOpcode::PushI32;
      return true;
    case LocalInfo::ValueKind::String:
      zeroOp = IrOpcode::PushI64;
      return true;
    default:
      error = "native backend requires a concrete uninitialized storage type on " + bindingName;
      return false;
  }
}

bool emitStringStatementBindingInitializer(const Expr &stmt,
                                           const Expr &init,
                                           LocalMap &localsIn,
                                           int32_t &nextLocal,
                                           std::vector<IrInstruction> &instructions,
                                           const IsBindingMutableFn &isBindingMutable,
                                           const std::function<int32_t(const std::string &)> &internString,
                                           const EmitExprForBindingFn &emitExpr,
                                           const InferBindingExprKindFn &inferExprKind,
                                           const std::function<int32_t()> &allocTempLocal,
                                           const IsEntryArgsNameFn &isEntryArgsName,
                                           const std::function<void()> &emitArrayIndexOutOfBounds,
                                           std::string &error) {
  int32_t index = -1;
  LocalInfo::StringSource source = LocalInfo::StringSource::None;
  bool argvChecked = true;
  if (!emitStringValueForCallFromLocals(
          init,
          localsIn,
          internString,
          [&](IrOpcode op, uint64_t imm) { instructions.push_back({op, imm}); },
          [&](const Expr &expr, std::string &accessName) { return getBuiltinArrayAccessName(expr, accessName); },
          [&](const Expr &expr) { return isEntryArgsName(expr, localsIn); },
          [&](const Expr &indexExpr, const std::string &accessName, StringIndexOps &ops, std::string &errorOut) {
            LocalInfo::ValueKind indexKind = normalizeIndexKind(inferExprKind(indexExpr, localsIn));
            if (!isSupportedIndexKind(indexKind)) {
              errorOut = "native backend requires integer indices for " + accessName;
              return false;
            }
            ops.pushZero = pushZeroForIndex(indexKind);
            ops.cmpLt = cmpLtForIndex(indexKind);
            ops.cmpGe = cmpGeForIndex(indexKind);
            ops.skipNegativeCheck = (indexKind == LocalInfo::ValueKind::UInt64);
            return true;
          },
          [&](const Expr &expr) { return emitExpr(expr, localsIn); },
          [&](const Expr &expr) { return inferExprKind(expr, localsIn) == LocalInfo::ValueKind::String; },
          allocTempLocal,
          [&]() { return instructions.size(); },
          [&](size_t indexToPatch, int32_t target) { instructions[indexToPatch].imm = target; },
          emitArrayIndexOutOfBounds,
          source,
          index,
          argvChecked,
          error)) {
    return false;
  }

  LocalInfo info;
  info.index = nextLocal++;
  info.isMutable = isBindingMutable(stmt);
  info.kind = LocalInfo::Kind::Value;
  info.valueKind = LocalInfo::ValueKind::String;
  info.stringSource = source;
  info.stringIndex = index;
  info.argvChecked = argvChecked;

  localsIn.emplace(stmt.name, info);
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(info.index)});
  return true;
}

UninitializedStorageInitDropEmitResult tryEmitUninitializedStorageInitDropStatement(
    const Expr &stmt,
    LocalMap &localsIn,
    std::vector<IrInstruction> &instructions,
    const ResolveUninitializedStorageForStatementFn &resolveUninitializedStorage,
    const EmitExprForBindingFn &emitExpr,
    const ResolveStructSlotLayoutForStatementFn &resolveStructSlotLayout,
    const std::function<int32_t()> &allocTempLocal,
    const EmitStructCopyFromPtrsForStatementFn &emitStructCopyFromPtrs,
    std::string &error) {
  if (stmt.kind != Expr::Kind::Call || stmt.isMethodCall ||
      (!isSimpleCallName(stmt, "init") && !isSimpleCallName(stmt, "drop"))) {
    return UninitializedStorageInitDropEmitResult::NotMatched;
  }

  const bool isInit = isSimpleCallName(stmt, "init");
  const size_t expectedArgs = isInit ? 2 : 1;
  if (stmt.args.size() != expectedArgs) {
    error = std::string(isInit ? "init" : "drop") + " requires exactly " + std::to_string(expectedArgs) + " argument" +
            (expectedArgs == 1 ? "" : "s");
    return UninitializedStorageInitDropEmitResult::Error;
  }

  UninitializedStorageAccessInfo access;
  bool resolved = false;
  if (!resolveUninitializedStorage(stmt.args.front(), localsIn, access, resolved)) {
    return UninitializedStorageInitDropEmitResult::Error;
  }
  if (!resolved) {
    error = std::string(isInit ? "init" : "drop") + " requires uninitialized storage";
    return UninitializedStorageInitDropEmitResult::Error;
  }

  if (!isInit) {
    return UninitializedStorageInitDropEmitResult::Emitted;
  }

  auto emitFieldPointer = [&](const Expr &receiver, const StructSlotFieldInfo &field, int32_t ptrLocal) -> bool {
    if (!emitExpr(receiver, localsIn)) {
      return false;
    }
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(ptrLocal)});
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
    const uint64_t offsetBytes = static_cast<uint64_t>(field.slotOffset) * IrSlotBytes;
    if (offsetBytes != 0) {
      instructions.push_back({IrOpcode::PushI64, offsetBytes});
      instructions.push_back({IrOpcode::AddI64, 0});
    }
    return true;
  };
  auto emitIndirectPointer = [&](const Expr &pointerExpr, int32_t ptrLocal) -> bool {
    if (pointerExpr.kind == Expr::Kind::Name) {
      auto it = localsIn.find(pointerExpr.name);
      if (it != localsIn.end() &&
          (it->second.kind == LocalInfo::Kind::Pointer || it->second.kind == LocalInfo::Kind::Reference)) {
        instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
        instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(ptrLocal)});
        return true;
      }
    }
    if (!emitExpr(pointerExpr, localsIn)) {
      return false;
    }
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(ptrLocal)});
    return true;
  };

  const Expr &valueExpr = stmt.args.back();
  if (access.location == UninitializedStorageAccessInfo::Location::Local) {
    const LocalInfo &storageInfo = *access.local;
    if (!storageInfo.structTypeName.empty()) {
      StructSlotLayoutInfo layout;
      if (!resolveStructSlotLayout(storageInfo.structTypeName, layout)) {
        return UninitializedStorageInitDropEmitResult::Error;
      }
      if (!emitExpr(valueExpr, localsIn)) {
        return UninitializedStorageInitDropEmitResult::Error;
      }
      const int32_t srcPtrLocal = allocTempLocal();
      instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(srcPtrLocal)});
      const int32_t destPtrLocal = allocTempLocal();
      instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(storageInfo.index)});
      instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(destPtrLocal)});
      if (!emitStructCopyFromPtrs(destPtrLocal, srcPtrLocal, layout.totalSlots)) {
        return UninitializedStorageInitDropEmitResult::Error;
      }
      return UninitializedStorageInitDropEmitResult::Emitted;
    }
    if (!emitExpr(valueExpr, localsIn)) {
      return UninitializedStorageInitDropEmitResult::Error;
    }
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(storageInfo.index)});
    return UninitializedStorageInitDropEmitResult::Emitted;
  }

  if (access.location == UninitializedStorageAccessInfo::Location::Field) {
    const Expr &receiver = stmt.args.front().args.front();
    const StructSlotFieldInfo &field = access.fieldSlot;
    const int32_t ptrLocal = allocTempLocal();
    if (!emitFieldPointer(receiver, field, ptrLocal)) {
      return UninitializedStorageInitDropEmitResult::Error;
    }
    if (!field.structPath.empty()) {
      if (!emitExpr(valueExpr, localsIn)) {
        return UninitializedStorageInitDropEmitResult::Error;
      }
      const int32_t srcPtrLocal = allocTempLocal();
      instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(srcPtrLocal)});
      if (!emitStructCopyFromPtrs(ptrLocal, srcPtrLocal, field.slotCount)) {
        return UninitializedStorageInitDropEmitResult::Error;
      }
      return UninitializedStorageInitDropEmitResult::Emitted;
    }
    if (!emitExpr(valueExpr, localsIn)) {
      return UninitializedStorageInitDropEmitResult::Error;
    }
    const int32_t valueLocal = allocTempLocal();
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(valueLocal)});
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal)});
    instructions.push_back({IrOpcode::StoreIndirect, 0});
    instructions.push_back({IrOpcode::Pop, 0});
    return UninitializedStorageInitDropEmitResult::Emitted;
  }

  if (access.location == UninitializedStorageAccessInfo::Location::Indirect) {
    if (access.pointerExpr == nullptr) {
      error = "native backend could not resolve uninitialized pointer target";
      return UninitializedStorageInitDropEmitResult::Error;
    }
    const int32_t ptrLocal = allocTempLocal();
    if (!emitIndirectPointer(*access.pointerExpr, ptrLocal)) {
      return UninitializedStorageInitDropEmitResult::Error;
    }
    if (!access.typeInfo.structPath.empty()) {
      StructSlotLayoutInfo layout;
      if (!resolveStructSlotLayout(access.typeInfo.structPath, layout)) {
        return UninitializedStorageInitDropEmitResult::Error;
      }
      if (!emitExpr(valueExpr, localsIn)) {
        return UninitializedStorageInitDropEmitResult::Error;
      }
      const int32_t srcPtrLocal = allocTempLocal();
      instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(srcPtrLocal)});
      if (!emitStructCopyFromPtrs(ptrLocal, srcPtrLocal, layout.totalSlots)) {
        return UninitializedStorageInitDropEmitResult::Error;
      }
      return UninitializedStorageInitDropEmitResult::Emitted;
    }
    if (!emitExpr(valueExpr, localsIn)) {
      return UninitializedStorageInitDropEmitResult::Error;
    }
    const int32_t valueLocal = allocTempLocal();
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(valueLocal)});
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal)});
    instructions.push_back({IrOpcode::StoreIndirect, 0});
    instructions.push_back({IrOpcode::Pop, 0});
    return UninitializedStorageInitDropEmitResult::Emitted;
  }

  return UninitializedStorageInitDropEmitResult::Emitted;
}

UninitializedStorageTakeEmitResult tryEmitUninitializedStorageTakeStatement(
    const Expr &stmt,
    const LocalMap &localsIn,
    std::vector<IrInstruction> &instructions,
    const ResolveUninitializedStorageForStatementFn &resolveUninitializedStorage,
    const EmitExprForBindingFn &emitExpr) {
  if (stmt.kind != Expr::Kind::Call || stmt.isMethodCall || !isSimpleCallName(stmt, "take") || stmt.args.size() != 1) {
    return UninitializedStorageTakeEmitResult::NotMatched;
  }

  UninitializedStorageAccessInfo access;
  bool resolved = false;
  if (!resolveUninitializedStorage(stmt.args.front(), localsIn, access, resolved)) {
    return UninitializedStorageTakeEmitResult::Error;
  }
  if (!resolved) {
    return UninitializedStorageTakeEmitResult::NotMatched;
  }
  if (!emitExpr(stmt, localsIn)) {
    return UninitializedStorageTakeEmitResult::Error;
  }
  instructions.push_back({IrOpcode::Pop, 0});
  return UninitializedStorageTakeEmitResult::Emitted;
}

StatementPrintPathSpaceEmitResult tryEmitPrintPathSpaceStatementBuiltin(
    const Expr &stmt,
    const LocalMap &localsIn,
    const EmitPrintArgForStatementFn &emitPrintArg,
    const ResolveDefinitionCallForStatementFn &resolveDefinitionCall,
    const EmitExprForBindingFn &emitExpr,
    std::vector<IrInstruction> &instructions,
    std::string &error) {
  PrintBuiltin printBuiltin;
  if (stmt.kind == Expr::Kind::Call && getPrintBuiltin(stmt, printBuiltin)) {
    if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
      error = printBuiltin.name + " does not support body arguments";
      return StatementPrintPathSpaceEmitResult::Error;
    }
    if (stmt.args.size() != 1) {
      error = printBuiltin.name + " requires exactly one argument";
      return StatementPrintPathSpaceEmitResult::Error;
    }
    if (!emitPrintArg(stmt.args.front(), localsIn, printBuiltin)) {
      return StatementPrintPathSpaceEmitResult::Error;
    }
    return StatementPrintPathSpaceEmitResult::Emitted;
  }

  PathSpaceBuiltin pathSpaceBuiltin;
  if (stmt.kind == Expr::Kind::Call && getPathSpaceBuiltin(stmt, pathSpaceBuiltin) && !resolveDefinitionCall(stmt)) {
    if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
      error = pathSpaceBuiltin.name + " does not support body arguments";
      return StatementPrintPathSpaceEmitResult::Error;
    }
    if (!stmt.templateArgs.empty()) {
      error = pathSpaceBuiltin.name + " does not support template arguments";
      return StatementPrintPathSpaceEmitResult::Error;
    }
    if (stmt.args.size() != pathSpaceBuiltin.argumentCount) {
      error = pathSpaceBuiltin.name + " requires exactly " + std::to_string(pathSpaceBuiltin.argumentCount) +
              " argument" + (pathSpaceBuiltin.argumentCount == 1 ? "" : "s");
      return StatementPrintPathSpaceEmitResult::Error;
    }
    for (const auto &arg : stmt.args) {
      if (arg.kind == Expr::Kind::StringLiteral) {
        continue;
      }
      if (!emitExpr(arg, localsIn)) {
        return StatementPrintPathSpaceEmitResult::Error;
      }
      instructions.push_back({IrOpcode::Pop, 0});
    }
    return StatementPrintPathSpaceEmitResult::Emitted;
  }

  return StatementPrintPathSpaceEmitResult::NotMatched;
}

ReturnStatementEmitResult tryEmitReturnStatement(
    const Expr &stmt,
    const LocalMap &localsIn,
    std::vector<IrInstruction> &instructions,
    const std::optional<ReturnStatementInlineContext> &inlineContext,
    bool definitionReturnsVoid,
    bool &sawReturn,
    const EmitExprForBindingFn &emitExpr,
    const InferBindingExprKindFn &inferExprKind,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferArrayElementKind,
    const std::function<void()> &emitFileScopeCleanupAll,
    std::string &error) {
  if (!isReturnCall(stmt)) {
    return ReturnStatementEmitResult::NotMatched;
  }

  auto isSupportedScalarKind = [](LocalInfo::ValueKind kind) {
    return kind == LocalInfo::ValueKind::Int64 || kind == LocalInfo::ValueKind::UInt64 ||
           kind == LocalInfo::ValueKind::Int32 || kind == LocalInfo::ValueKind::Bool ||
           kind == LocalInfo::ValueKind::Float32 || kind == LocalInfo::ValueKind::Float64 ||
           kind == LocalInfo::ValueKind::String;
  };

  if (inlineContext.has_value()) {
    const ReturnStatementInlineContext &context = *inlineContext;
    if (stmt.args.empty()) {
      if (!context.returnsVoid) {
        error = "return requires exactly one argument";
        return ReturnStatementEmitResult::Error;
      }
      const size_t jumpIndex = instructions.size();
      instructions.push_back({IrOpcode::Jump, 0});
      if (context.returnJumps) {
        context.returnJumps->push_back(jumpIndex);
      }
      return ReturnStatementEmitResult::Emitted;
    }
    if (stmt.args.size() != 1) {
      error = "return requires exactly one argument";
      return ReturnStatementEmitResult::Error;
    }
    if (context.returnsVoid) {
      error = "return value not allowed for void definition";
      return ReturnStatementEmitResult::Error;
    }
    if (context.returnLocal < 0) {
      error = "native backend missing inline return local";
      return ReturnStatementEmitResult::Error;
    }

    const Expr &valueExpr = stmt.args.front();
    if (!emitExpr(valueExpr, localsIn)) {
      return ReturnStatementEmitResult::Error;
    }

    if (context.returnsArray) {
      LocalInfo::ValueKind arrayKind = inferArrayElementKind(valueExpr, localsIn);
      if (arrayKind == LocalInfo::ValueKind::Unknown) {
        arrayKind = context.returnKind;
      }
      if (arrayKind == LocalInfo::ValueKind::Unknown) {
        const LocalInfo::ValueKind scalarKind = inferExprKind(valueExpr, localsIn);
        const bool isOpaqueAggregateHandle =
            scalarKind == LocalInfo::ValueKind::Int64 || scalarKind == LocalInfo::ValueKind::UInt64;
        if (isSupportedScalarKind(scalarKind) && !isOpaqueAggregateHandle) {
          error = "native backend only supports returning array values"
                  " [inline scalar=" + typeNameForValueKind(scalarKind) +
                  ", declared=" + typeNameForValueKind(context.returnKind) + "]";
          return ReturnStatementEmitResult::Error;
        }
      } else if (arrayKind == LocalInfo::ValueKind::String) {
        error = "native backend does not support string array return types";
        return ReturnStatementEmitResult::Error;
      }
      instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(context.returnLocal)});
      const size_t jumpIndex = instructions.size();
      instructions.push_back({IrOpcode::Jump, 0});
      if (context.returnJumps) {
        context.returnJumps->push_back(jumpIndex);
      }
      return ReturnStatementEmitResult::Emitted;
    }

    const LocalInfo::ValueKind returnKind = inferExprKind(valueExpr, localsIn);
    if (!isSupportedScalarKind(returnKind)) {
      instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(context.returnLocal)});
      const size_t jumpIndex = instructions.size();
      instructions.push_back({IrOpcode::Jump, 0});
      if (context.returnJumps) {
        context.returnJumps->push_back(jumpIndex);
      }
      return ReturnStatementEmitResult::Emitted;
    }
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(context.returnLocal)});
    const size_t jumpIndex = instructions.size();
    instructions.push_back({IrOpcode::Jump, 0});
    if (context.returnJumps) {
      context.returnJumps->push_back(jumpIndex);
    }
    return ReturnStatementEmitResult::Emitted;
  }

  if (stmt.args.empty()) {
    if (!definitionReturnsVoid) {
      error = "return requires exactly one argument";
      return ReturnStatementEmitResult::Error;
    }
    if (emitFileScopeCleanupAll) {
      emitFileScopeCleanupAll();
    }
    instructions.push_back({IrOpcode::ReturnVoid, 0});
    sawReturn = true;
    return ReturnStatementEmitResult::Emitted;
  }
  if (stmt.args.size() != 1) {
    error = "return requires exactly one argument";
    return ReturnStatementEmitResult::Error;
  }
  if (definitionReturnsVoid) {
    error = "return value not allowed for void definition";
    return ReturnStatementEmitResult::Error;
  }

  const Expr &valueExpr = stmt.args.front();
  if (!emitExpr(valueExpr, localsIn)) {
    return ReturnStatementEmitResult::Error;
  }
  if (emitFileScopeCleanupAll) {
    emitFileScopeCleanupAll();
  }

  const LocalInfo::ValueKind arrayKind = inferArrayElementKind(valueExpr, localsIn);
  if (arrayKind != LocalInfo::ValueKind::Unknown) {
    if (arrayKind == LocalInfo::ValueKind::String) {
      error = "native backend does not support string array return types";
      return ReturnStatementEmitResult::Error;
    }
    instructions.push_back({IrOpcode::ReturnI64, 0});
    sawReturn = true;
    return ReturnStatementEmitResult::Emitted;
  }

  const LocalInfo::ValueKind returnKind = inferExprKind(valueExpr, localsIn);
  if (returnKind == LocalInfo::ValueKind::Int64 || returnKind == LocalInfo::ValueKind::UInt64 ||
      returnKind == LocalInfo::ValueKind::String) {
    instructions.push_back({IrOpcode::ReturnI64, 0});
  } else if (returnKind == LocalInfo::ValueKind::Int32 || returnKind == LocalInfo::ValueKind::Bool) {
    instructions.push_back({IrOpcode::ReturnI32, 0});
  } else if (returnKind == LocalInfo::ValueKind::Float32) {
    instructions.push_back({IrOpcode::ReturnF32, 0});
  } else if (returnKind == LocalInfo::ValueKind::Float64) {
    instructions.push_back({IrOpcode::ReturnF64, 0});
  } else {
    instructions.push_back({IrOpcode::ReturnI64, 0});
  }

  sawReturn = true;
  return ReturnStatementEmitResult::Emitted;
}

StatementMatchIfEmitResult tryEmitMatchIfStatement(const Expr &stmt,
                                                   LocalMap &localsIn,
                                                   const EmitExprForBindingFn &emitExpr,
                                                   const InferBindingExprKindFn &inferExprKind,
                                                   const EmitBlockForBindingFn &emitBlock,
                                                   const EmitStatementForBindingFn &emitStatement,
                                                   std::vector<IrInstruction> &instructions,
                                                   std::string &error) {
  if (isMatchCall(stmt)) {
    Expr expanded;
    if (!lowerMatchToIf(stmt, expanded, error)) {
      return StatementMatchIfEmitResult::Error;
    }
    if (!emitStatement(expanded, localsIn)) {
      return StatementMatchIfEmitResult::Error;
    }
    return StatementMatchIfEmitResult::Emitted;
  }

  if (!isIfCall(stmt)) {
    return StatementMatchIfEmitResult::NotMatched;
  }

  if (stmt.args.size() != 3) {
    error = "if requires condition, then, else";
    return StatementMatchIfEmitResult::Error;
  }
  if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
    error = "if does not accept trailing block arguments";
    return StatementMatchIfEmitResult::Error;
  }
  if (!emitExpr(stmt.args[0], localsIn)) {
    return StatementMatchIfEmitResult::Error;
  }
  const LocalInfo::ValueKind condKind = inferExprKind(stmt.args[0], localsIn);
  const bool isIntegralCondition =
      condKind == LocalInfo::ValueKind::Int32 || condKind == LocalInfo::ValueKind::Int64 ||
      condKind == LocalInfo::ValueKind::UInt64;
  if (condKind != LocalInfo::ValueKind::Bool && !isIntegralCondition) {
    error = "if condition requires bool";
    return StatementMatchIfEmitResult::Error;
  }

  const Expr &thenArg = stmt.args[1];
  const Expr &elseArg = stmt.args[2];
  auto isIfBlockEnvelope = [&](const Expr &candidate) -> bool {
    if (candidate.kind != Expr::Kind::Call || candidate.isBinding || candidate.isMethodCall) {
      return false;
    }
    if (!candidate.args.empty() || !candidate.templateArgs.empty() || hasNamedArguments(candidate.argNames)) {
      return false;
    }
    if (!candidate.hasBodyArguments && candidate.bodyArguments.empty()) {
      return false;
    }
    return true;
  };
  if (!isIfBlockEnvelope(thenArg) || !isIfBlockEnvelope(elseArg)) {
    error = "if branches require block envelopes";
    return StatementMatchIfEmitResult::Error;
  }

  auto emitBranch = [&](const Expr &branch, LocalMap &branchLocals) -> bool { return emitBlock(branch, branchLocals); };
  const size_t jumpIfZeroIndex = instructions.size();
  instructions.push_back({IrOpcode::JumpIfZero, 0});
  LocalMap thenLocals = localsIn;
  if (!emitBranch(thenArg, thenLocals)) {
    return StatementMatchIfEmitResult::Error;
  }
  const size_t jumpIndex = instructions.size();
  instructions.push_back({IrOpcode::Jump, 0});
  const size_t elseIndex = instructions.size();
  instructions[jumpIfZeroIndex].imm = static_cast<int32_t>(elseIndex);
  LocalMap elseLocals = localsIn;
  if (!emitBranch(elseArg, elseLocals)) {
    return StatementMatchIfEmitResult::Error;
  }
  const size_t endIndex = instructions.size();
  instructions[jumpIndex].imm = static_cast<int32_t>(endIndex);
  return StatementMatchIfEmitResult::Emitted;
}

} // namespace primec::ir_lowerer
