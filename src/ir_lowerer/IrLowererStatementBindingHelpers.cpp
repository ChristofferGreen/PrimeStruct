#include "IrLowererStatementBindingHelpers.h"

#include "IrLowererHelpers.h"
#include "IrLowererIndexKindHelpers.h"
#include "IrLowererSetupTypeHelpers.h"
#include "IrLowererStringLiteralHelpers.h"

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
  bool emittedValue = false;

  if (init.kind == Expr::Kind::StringLiteral) {
    std::string decoded;
    if (!parseLowererStringLiteral(init.stringValue, decoded, error)) {
      return false;
    }
    index = internString(decoded);
    source = LocalInfo::StringSource::TableIndex;
    emittedValue = true;
    instructions.push_back({IrOpcode::PushI64, static_cast<uint64_t>(index)});
  } else if (init.kind == Expr::Kind::Name) {
    auto it = localsIn.find(init.name);
    if (it == localsIn.end()) {
      error = "native backend does not know identifier: " + init.name;
      return false;
    }
    if (it->second.valueKind != LocalInfo::ValueKind::String || it->second.stringSource == LocalInfo::StringSource::None) {
      error = "native backend requires string bindings to use string literals, bindings, or entry args";
      return false;
    }
    source = it->second.stringSource;
    index = it->second.stringIndex;
    if (source == LocalInfo::StringSource::ArgvIndex) {
      argvChecked = it->second.argvChecked;
    }
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
    emittedValue = true;
  } else if (init.kind == Expr::Kind::Call) {
    std::string accessName;
    if (getBuiltinArrayAccessName(init, accessName)) {
      if (init.args.size() != 2) {
        error = accessName + " requires exactly two arguments";
        return false;
      }
      if (!isEntryArgsName(init.args.front(), localsIn)) {
        error = "native backend only supports entry argument indexing";
        return false;
      }
      LocalInfo::ValueKind indexKind = normalizeIndexKind(inferExprKind(init.args[1], localsIn));
      if (!isSupportedIndexKind(indexKind)) {
        error = "native backend requires integer indices for " + accessName;
        return false;
      }

      const int32_t argvIndexLocal = allocTempLocal();
      if (!emitExpr(init.args[1], localsIn)) {
        return false;
      }
      instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(argvIndexLocal)});

      if (accessName == "at") {
        if (indexKind != LocalInfo::ValueKind::UInt64) {
          instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(argvIndexLocal)});
          instructions.push_back({pushZeroForIndex(indexKind), 0});
          instructions.push_back({cmpLtForIndex(indexKind), 0});
          size_t jumpNonNegative = instructions.size();
          instructions.push_back({IrOpcode::JumpIfZero, 0});
          emitArrayIndexOutOfBounds();
          size_t nonNegativeIndex = instructions.size();
          instructions[jumpNonNegative].imm = static_cast<int32_t>(nonNegativeIndex);
        }

        instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(argvIndexLocal)});
        instructions.push_back({IrOpcode::PushArgc, 0});
        instructions.push_back({cmpGeForIndex(indexKind), 0});
        size_t jumpInRange = instructions.size();
        instructions.push_back({IrOpcode::JumpIfZero, 0});
        emitArrayIndexOutOfBounds();
        size_t inRangeIndex = instructions.size();
        instructions[jumpInRange].imm = static_cast<int32_t>(inRangeIndex);
      }

      instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(argvIndexLocal)});
      source = LocalInfo::StringSource::ArgvIndex;
      argvChecked = (accessName == "at");
      emittedValue = true;
    } else {
      LocalInfo::ValueKind initKind = inferExprKind(init, localsIn);
      if (initKind != LocalInfo::ValueKind::String) {
        error = "native backend requires string bindings to use string literals, bindings, or entry args";
        return false;
      }
      if (!emitExpr(init, localsIn)) {
        return false;
      }
      source = LocalInfo::StringSource::RuntimeIndex;
      emittedValue = true;
    }
  } else {
    error = "native backend requires string bindings to use string literals, bindings, or entry args";
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
  if (!emittedValue) {
    error = "native backend requires string bindings to use string literals, bindings, or entry args";
    return false;
  }

  localsIn.emplace(stmt.name, info);
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(info.index)});
  return true;
}

} // namespace primec::ir_lowerer
