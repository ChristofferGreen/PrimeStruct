#include "IrLowererStatementBindingHelpers.h"

#include "IrLowererStatementBindingInternal.h"

#include "IrLowererBindingTransformHelpers.h"
#include "IrLowererBindingTypeHelpers.h"
#include "IrLowererHelpers.h"
#include "IrLowererSetupTypeHelpers.h"
#include "IrLowererTemplateTypeParseHelpers.h"

namespace primec::ir_lowerer {

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
      if (it != localsIn.end() &&
          (it->second.kind == LocalInfo::Kind::Pointer || it->second.kind == LocalInfo::Kind::Reference)) {
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
      if (it != localsIn.end() &&
          (it->second.kind == LocalInfo::Kind::Array || it->second.kind == LocalInfo::Kind::Vector)) {
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
                                 std::string &error,
                                 const std::function<const Definition *(const Expr &, const LocalMap &)>
                                     &resolveMethodCallDefinition,
                                 const std::function<const Definition *(const Expr &)> &resolveDefinitionCall,
                                 const std::function<bool(const std::string &, ReturnInfo &)> &getReturnInfo) {
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
    ResultExprInfo inferredResultInfo;
    if (inferCallParameterDefaultResultInfo(
            param.args.front(),
            localsForKindInference,
            inferExprKind,
            resolveMethodCallDefinition,
            resolveDefinitionCall,
            getReturnInfo,
            inferredResultInfo) &&
        inferredResultInfo.isResult) {
      infoOut.isResult = true;
      infoOut.resultHasValue = inferredResultInfo.hasValue;
      infoOut.resultValueKind = inferredResultInfo.valueKind;
      infoOut.resultValueCollectionKind = inferredResultInfo.valueCollectionKind;
      infoOut.resultValueMapKeyKind = inferredResultInfo.valueMapKeyKind;
      infoOut.resultValueIsFileHandle = inferredResultInfo.valueIsFileHandle;
      infoOut.resultValueStructType = inferredResultInfo.valueStructType;
      infoOut.resultErrorType = inferredResultInfo.errorType;
      infoOut.valueKind = infoOut.resultHasValue ? LocalInfo::ValueKind::Int64 : LocalInfo::ValueKind::Int32;
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
    } else if (applyErrorTypeMetadata(transform.name, infoOut)) {
      continue;
    } else if (transform.name == "Result") {
      infoOut.isResult = true;
      infoOut.resultHasValue = (transform.templateArgs.size() == 2);
      infoOut.resultValueKind = LocalInfo::ValueKind::Unknown;
      infoOut.resultValueCollectionKind = LocalInfo::Kind::Value;
      infoOut.resultValueMapKeyKind = LocalInfo::ValueKind::Unknown;
      if (infoOut.resultHasValue && !transform.templateArgs.empty()) {
        resolveSupportedResultCollectionType(
            transform.templateArgs.front(),
            infoOut.resultValueCollectionKind,
            infoOut.resultValueKind,
            &infoOut.resultValueMapKeyKind);
        if (infoOut.resultValueCollectionKind == LocalInfo::Kind::Value) {
          infoOut.resultValueKind = valueKindFromTypeName(transform.templateArgs.front());
        }
      }
      infoOut.resultValueIsFileHandle =
          infoOut.resultHasValue && !transform.templateArgs.empty() &&
          isFileHandleTypeText(transform.templateArgs.front());
      if (infoOut.resultValueIsFileHandle) {
        infoOut.resultValueKind = LocalInfo::ValueKind::Int64;
      }
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
      applyErrorTypeMetadata(targetType, infoOut);
      bool resultHasValue = false;
      LocalInfo::ValueKind resultValueKind = LocalInfo::ValueKind::Unknown;
      std::string resultErrorType;
      if (parseResultTypeName(targetType, resultHasValue, resultValueKind, resultErrorType)) {
        infoOut.isResult = true;
        infoOut.resultHasValue = resultHasValue;
        std::string resultValueType;
        infoOut.resultValueCollectionKind = LocalInfo::Kind::Value;
        infoOut.resultValueMapKeyKind = LocalInfo::ValueKind::Unknown;
        infoOut.resultValueIsFileHandle =
            resultHasValue && extractResultValueTypeText(targetType, resultValueType) &&
            isFileHandleTypeText(resultValueType);
        if (resultHasValue && !resultValueType.empty()) {
          resolveSupportedResultCollectionType(
              resultValueType,
              infoOut.resultValueCollectionKind,
              infoOut.resultValueKind,
              &infoOut.resultValueMapKeyKind);
        }
        if (infoOut.resultValueIsFileHandle) {
          infoOut.resultValueKind = LocalInfo::ValueKind::Int64;
        } else if (infoOut.resultValueCollectionKind == LocalInfo::Kind::Value) {
          infoOut.resultValueKind = resultValueKind;
        }
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

  if (infoOut.errorTypeName == "GfxError" && infoOut.errorHelperNamespacePath.empty() && param.args.size() == 1) {
    const Expr &initExpr = param.args.front();
    const Definition *initDef =
        initExpr.isMethodCall ? (resolveMethodCallDefinition ? resolveMethodCallDefinition(initExpr, localsForKindInference)
                                                             : nullptr)
                              : (resolveDefinitionCall ? resolveDefinitionCall(initExpr) : nullptr);
    if (initDef != nullptr) {
      if (initDef->fullPath.rfind("/std/gfx/experimental/", 0) == 0) {
        infoOut.errorHelperNamespacePath = "/std/gfx/experimental/GfxError";
      } else if (initDef->fullPath.rfind("/std/gfx/", 0) == 0 ||
                 initDef->fullPath.rfind("/GfxError/", 0) == 0) {
        infoOut.errorHelperNamespacePath = "/std/gfx/GfxError";
      }
    }
  }

  infoOut.isFileError = infoOut.isFileError || isFileErrorBinding(param);
  setReferenceArrayInfo(param, infoOut);
  applyStructArrayInfo(param, infoOut);
  applyStructValueInfo(param, infoOut);
  if (infoOut.kind == LocalInfo::Kind::Value && !infoOut.structTypeName.empty()) {
    infoOut.valueKind = LocalInfo::ValueKind::Int64;
  }
  const bool isUnsupportedStringPointerReferenceArgsPack = [&param]() {
    for (const auto &transform : param.transforms) {
      if (transform.name == "effects" || transform.name == "capabilities" ||
          isBindingQualifierName(transform.name)) {
        continue;
      }
      if (transform.name != "args" || transform.templateArgs.size() != 1) {
        return false;
      }
      std::string base;
      std::string arg;
      if (!splitTemplateTypeName(trimTemplateTypeText(transform.templateArgs.front()), base, arg)) {
        return false;
      }
      const std::string normalizedBase = normalizeCollectionBindingTypeName(base);
      return (normalizedBase == "Pointer" || normalizedBase == "Reference") &&
             trimTemplateTypeText(arg) == "string";
    }
    return false;
  }();
  if (isUnsupportedStringPointerReferenceArgsPack) {
    error = "variadic args<T> does not support string pointers or references";
    return false;
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

} // namespace primec::ir_lowerer
