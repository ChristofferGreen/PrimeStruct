#include "IrLowererStatementBindingHelpers.h"

#include "IrLowererHelpers.h"
#include "IrLowererSetupTypeHelpers.h"

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
  if (!hasExplicitType && info.kind == LocalInfo::Kind::Value) {
    if (init.kind == Expr::Kind::Name) {
      auto it = localsIn.find(init.name);
      if (it != localsIn.end()) {
        info.kind = it->second.kind;
      }
    } else if (init.kind == Expr::Kind::Call) {
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

  if (info.kind == LocalInfo::Kind::Map) {
    if (hasExplicitType) {
      for (const auto &transform : stmt.transforms) {
        if (transform.name == "map" && transform.templateArgs.size() == 2) {
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
    info.valueKind = inferExprKind(init, localsIn);
    if (info.valueKind == LocalInfo::ValueKind::Unknown) {
      info.valueKind = LocalInfo::ValueKind::Int32;
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
  infoOut.kind = bindingKind(param);
  if (hasExplicitBindingTypeTransform(param)) {
    infoOut.valueKind = bindingValueKind(param, infoOut.kind);
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
      if (transform.name == "map" && transform.templateArgs.size() == 2) {
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
      infoOut.valueKind = infoOut.resultHasValue ? LocalInfo::ValueKind::Int64 : LocalInfo::ValueKind::Int32;
      if (!transform.templateArgs.empty()) {
        infoOut.resultErrorType = transform.templateArgs.back();
      }
    }
  }

  infoOut.isFileError = isFileErrorBinding(param);
  setReferenceArrayInfo(param, infoOut);
  applyStructArrayInfo(param, infoOut);
  applyStructValueInfo(param, infoOut);
  if (!isStringBinding(param)) {
    return true;
  }
  if (infoOut.kind != LocalInfo::Kind::Value) {
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
