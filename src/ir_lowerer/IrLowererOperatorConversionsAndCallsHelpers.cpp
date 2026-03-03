#include "IrLowererOperatorConversionsAndCallsHelpers.h"

#include "IrLowererHelpers.h"

#include <cstring>
#include <limits>

namespace primec::ir_lowerer {

bool emitConversionsAndCallsOperatorExpr(
    const Expr &expr,
    const LocalMap &localsIn,
    int32_t &nextLocal,
    const EmitConversionsAndCallsExprWithLocalsFn &emitExpr,
    const InferConversionsAndCallsExprKindWithLocalsFn &inferExprKind,
    const EmitConversionsAndCallsCompareToZeroFn &emitCompareToZero,
    const AllocConversionsAndCallsTempLocalFn &allocTempLocal,
    const EmitConversionsAndCallsFloatToIntNonFiniteFn &emitFloatToIntNonFinite,
    const ResolveConversionsAndCallsStringTableTargetFn &resolveStringTableTarget,
    const ConversionsAndCallsValueKindFromTypeNameFn &valueKindFromTypeName,
    const ConversionsAndCallsGetMathConstantNameFn &getMathConstantName,
    const InferConversionsAndCallsStructExprPathFn &inferStructExprPath,
    const ResolveConversionsAndCallsStructSlotCountFn &resolveStructSlotCount,
    const EmitConversionsAndCallsStructCopyFromPtrsFn &emitStructCopyFromPtrs,
    std::vector<IrInstruction> &instructions,
    bool &handled,
    std::string &error) {
  handled = true;
  std::string builtin;
        if (getBuiltinConvertName(expr)) {
          if (expr.templateArgs.size() != 1) {
            error = "convert requires exactly one template argument";
            return false;
          }
          if (expr.args.size() != 1) {
            error = "convert requires exactly one argument";
            return false;
          }
          const std::string &typeName = expr.templateArgs.front();
          LocalInfo::ValueKind targetKind = valueKindFromTypeName(typeName);
          if (targetKind == LocalInfo::ValueKind::Unknown || targetKind == LocalInfo::ValueKind::String) {
            error = "native backend only supports convert<int>, convert<i32>, convert<i64>, convert<u64>, "
                    "convert<bool>, convert<f32>, or convert<f64>";
            return false;
          }
          auto tryEmitMathConstantConvert = [&](const Expr &arg) -> bool {
            if (arg.kind != Expr::Kind::Name) {
              return false;
            }
            std::string mathConst;
            if (!getMathConstantName(arg.name, mathConst)) {
              return false;
            }
            double value = 0.0;
            if (mathConst == "pi") {
              value = 3.14159265358979323846;
            } else if (mathConst == "tau") {
              value = 6.28318530717958647692;
            } else if (mathConst == "e") {
              value = 2.71828182845904523536;
            }
            if (targetKind == LocalInfo::ValueKind::Bool) {
              instructions.push_back({IrOpcode::PushI32, value != 0.0 ? 1u : 0u});
              return true;
            }
            if (targetKind == LocalInfo::ValueKind::Int32) {
              instructions.push_back(
                  {IrOpcode::PushI32, static_cast<uint64_t>(static_cast<int32_t>(value))});
              return true;
            }
            if (targetKind == LocalInfo::ValueKind::Int64) {
              instructions.push_back(
                  {IrOpcode::PushI64, static_cast<uint64_t>(static_cast<int64_t>(value))});
              return true;
            }
            if (targetKind == LocalInfo::ValueKind::UInt64) {
              instructions.push_back({IrOpcode::PushI64, static_cast<uint64_t>(value)});
              return true;
            }
            if (targetKind == LocalInfo::ValueKind::Float64) {
              uint64_t bits = 0;
              std::memcpy(&bits, &value, sizeof(bits));
              instructions.push_back({IrOpcode::PushF64, bits});
              return true;
            }
            float f32 = static_cast<float>(value);
            uint32_t bits = 0;
            std::memcpy(&bits, &f32, sizeof(bits));
            instructions.push_back({IrOpcode::PushF32, static_cast<uint64_t>(bits)});
            return true;
          };
          if (tryEmitMathConstantConvert(expr.args.front())) {
            return true;
          }
          if (!emitExpr(expr.args.front(), localsIn)) {
            return false;
          }
          if (targetKind == LocalInfo::ValueKind::Bool) {
            LocalInfo::ValueKind kind = inferExprKind(expr.args.front(), localsIn);
            if (!emitCompareToZero(kind, false)) {
              return false;
            }
            return true;
          }
          LocalInfo::ValueKind argKind = inferExprKind(expr.args.front(), localsIn);
          if (argKind == LocalInfo::ValueKind::Bool) {
            argKind = LocalInfo::ValueKind::Int32;
          }
          if (argKind == LocalInfo::ValueKind::Unknown || argKind == LocalInfo::ValueKind::String) {
            error = "convert requires numeric argument";
            return false;
          }
          bool needsFloatToIntGuard = false;
          LocalInfo::ValueKind floatKind = LocalInfo::ValueKind::Unknown;
          if (argKind == LocalInfo::ValueKind::Float32 || argKind == LocalInfo::ValueKind::Float64) {
            if (targetKind == LocalInfo::ValueKind::Int32 || targetKind == LocalInfo::ValueKind::Int64 ||
                targetKind == LocalInfo::ValueKind::UInt64) {
              needsFloatToIntGuard = true;
              floatKind = argKind;
            }
          }
          if (argKind == targetKind) {
            return true;
          }
          if (needsFloatToIntGuard) {
            int32_t tempValue = allocTempLocal();
            instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempValue)});
            IrOpcode cmpNeOp = (floatKind == LocalInfo::ValueKind::Float64) ? IrOpcode::CmpNeF64 : IrOpcode::CmpNeF32;
            IrOpcode cmpGtOp = (floatKind == LocalInfo::ValueKind::Float64) ? IrOpcode::CmpGtF64 : IrOpcode::CmpGtF32;
            IrOpcode cmpLtOp = (floatKind == LocalInfo::ValueKind::Float64) ? IrOpcode::CmpLtF64 : IrOpcode::CmpLtF32;

            auto pushFloatConst = [&](double value) {
              if (floatKind == LocalInfo::ValueKind::Float64) {
                uint64_t bits = 0;
                std::memcpy(&bits, &value, sizeof(bits));
                instructions.push_back({IrOpcode::PushF64, bits});
                return;
              }
              float f32 = static_cast<float>(value);
              uint32_t bits = 0;
              std::memcpy(&bits, &f32, sizeof(bits));
              instructions.push_back({IrOpcode::PushF32, static_cast<uint64_t>(bits)});
            };

            const double maxFinite = (floatKind == LocalInfo::ValueKind::Float64) ? 1.7976931348623157e308
                                                                                  : 3.4028234663852886e38;

            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
            instructions.push_back({cmpNeOp, 0});
            size_t jumpIfNotNaN = instructions.size();
            instructions.push_back({IrOpcode::JumpIfZero, 0});
            emitFloatToIntNonFinite();
            size_t nanOkIndex = instructions.size();
            instructions[jumpIfNotNaN].imm = static_cast<int32_t>(nanOkIndex);

            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
            pushFloatConst(maxFinite);
            instructions.push_back({cmpGtOp, 0});
            size_t jumpIfNotPosInf = instructions.size();
            instructions.push_back({IrOpcode::JumpIfZero, 0});
            emitFloatToIntNonFinite();
            size_t posOkIndex = instructions.size();
            instructions[jumpIfNotPosInf].imm = static_cast<int32_t>(posOkIndex);

            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
            pushFloatConst(-maxFinite);
            instructions.push_back({cmpLtOp, 0});
            size_t jumpIfNotNegInf = instructions.size();
            instructions.push_back({IrOpcode::JumpIfZero, 0});
            emitFloatToIntNonFinite();
            size_t negOkIndex = instructions.size();
            instructions[jumpIfNotNegInf].imm = static_cast<int32_t>(negOkIndex);

            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
          }
          IrOpcode convertOp = IrOpcode::ConvertI32ToF32;
          bool needsConvert = true;
          if (targetKind == LocalInfo::ValueKind::Float32) {
            if (argKind == LocalInfo::ValueKind::Int32) {
              convertOp = IrOpcode::ConvertI32ToF32;
            } else if (argKind == LocalInfo::ValueKind::Int64) {
              convertOp = IrOpcode::ConvertI64ToF32;
            } else if (argKind == LocalInfo::ValueKind::UInt64) {
              convertOp = IrOpcode::ConvertU64ToF32;
            } else if (argKind == LocalInfo::ValueKind::Float64) {
              convertOp = IrOpcode::ConvertF64ToF32;
            } else {
              error = "convert requires numeric argument";
              return false;
            }
          } else if (targetKind == LocalInfo::ValueKind::Float64) {
            if (argKind == LocalInfo::ValueKind::Int32) {
              convertOp = IrOpcode::ConvertI32ToF64;
            } else if (argKind == LocalInfo::ValueKind::Int64) {
              convertOp = IrOpcode::ConvertI64ToF64;
            } else if (argKind == LocalInfo::ValueKind::UInt64) {
              convertOp = IrOpcode::ConvertU64ToF64;
            } else if (argKind == LocalInfo::ValueKind::Float32) {
              convertOp = IrOpcode::ConvertF32ToF64;
            } else {
              error = "convert requires numeric argument";
              return false;
            }
          } else if (targetKind == LocalInfo::ValueKind::Int32) {
            if (argKind == LocalInfo::ValueKind::Float32) {
              convertOp = IrOpcode::ConvertF32ToI32;
            } else if (argKind == LocalInfo::ValueKind::Float64) {
              convertOp = IrOpcode::ConvertF64ToI32;
            } else {
              needsConvert = false;
            }
          } else if (targetKind == LocalInfo::ValueKind::Int64) {
            if (argKind == LocalInfo::ValueKind::Float32) {
              convertOp = IrOpcode::ConvertF32ToI64;
            } else if (argKind == LocalInfo::ValueKind::Float64) {
              convertOp = IrOpcode::ConvertF64ToI64;
            } else {
              needsConvert = false;
            }
          } else if (targetKind == LocalInfo::ValueKind::UInt64) {
            if (argKind == LocalInfo::ValueKind::Float32) {
              convertOp = IrOpcode::ConvertF32ToU64;
            } else if (argKind == LocalInfo::ValueKind::Float64) {
              convertOp = IrOpcode::ConvertF64ToU64;
            } else {
              needsConvert = false;
            }
          }
          if (needsConvert) {
            instructions.push_back({convertOp, 0});
          }
          return true;
        }
        if (getBuiltinPointerName(expr, builtin)) {
          if (expr.args.size() != 1) {
            error = builtin + " requires exactly one argument";
            return false;
          }
          if (builtin == "location") {
            const Expr &target = expr.args.front();
            if (target.kind != Expr::Kind::Name) {
              error = "location requires a local binding";
              return false;
            }
            auto it = localsIn.find(target.name);
            if (it == localsIn.end()) {
              error = "location requires a local binding";
              return false;
            }
            if (it->second.kind == LocalInfo::Kind::Reference) {
              instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
            } else {
              instructions.push_back({IrOpcode::AddressOfLocal, static_cast<uint64_t>(it->second.index)});
            }
            return true;
          }
          const Expr &pointerExpr = expr.args.front();
          if (pointerExpr.kind == Expr::Kind::Name) {
            auto it = localsIn.find(pointerExpr.name);
            if (it == localsIn.end()) {
              error = "native backend does not know identifier: " + pointerExpr.name;
              return false;
            }
            if (it->second.kind != LocalInfo::Kind::Pointer && it->second.kind != LocalInfo::Kind::Reference) {
              error = "dereference requires a pointer or reference";
              return false;
            }
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
          } else {
            if (!emitExpr(pointerExpr, localsIn)) {
              return false;
            }
          }
          instructions.push_back({IrOpcode::LoadIndirect, 0});
          return true;
        }
        if (getBuiltinCollectionName(expr, builtin)) {
          if (builtin == "array" || builtin == "vector") {
            if (expr.templateArgs.size() != 1) {
              error = builtin + " literal requires exactly one template argument";
              return false;
            }
            LocalInfo::ValueKind elemKind = valueKindFromTypeName(expr.templateArgs.front());
            if (elemKind == LocalInfo::ValueKind::Unknown || elemKind == LocalInfo::ValueKind::String) {
              error = "native backend only supports numeric/bool " + builtin + " literals";
              return false;
            }
            if (expr.args.size() > static_cast<size_t>(std::numeric_limits<int32_t>::max())) {
              error = builtin + " literal too large for native backend";
              return false;
            }

            const bool isVector = (builtin == "vector");
            const int32_t baseLocal = nextLocal;
            const int32_t headerSlots = isVector ? 2 : 1;
            nextLocal += static_cast<int32_t>(headerSlots + expr.args.size());

            instructions.push_back({IrOpcode::PushI32, static_cast<uint64_t>(static_cast<int32_t>(expr.args.size()))});
            instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal)});
            if (isVector) {
              instructions.push_back({IrOpcode::PushI32, static_cast<uint64_t>(static_cast<int32_t>(expr.args.size()))});
              instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal + 1)});
            }

            for (size_t i = 0; i < expr.args.size(); ++i) {
              const Expr &arg = expr.args[i];
              LocalInfo::ValueKind argKind = inferExprKind(arg, localsIn);
              if (argKind == LocalInfo::ValueKind::Unknown || argKind == LocalInfo::ValueKind::String) {
                error = "native backend requires " + builtin + " literal elements to be numeric/bool values";
                return false;
              }
              if (argKind != elemKind) {
                error = builtin + " literal element type mismatch";
                return false;
              }
              if (!emitExpr(arg, localsIn)) {
                return false;
              }
              instructions.push_back(
                  {IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal + headerSlots + static_cast<int32_t>(i))});
            }

            instructions.push_back({IrOpcode::AddressOfLocal, static_cast<uint64_t>(baseLocal)});
            return true;
          }
          if (builtin == "map") {
            if (expr.templateArgs.size() != 2) {
              error = "map literal requires exactly two template arguments";
              return false;
            }
            if (expr.args.size() % 2 != 0) {
              error = "map literal requires an even number of arguments";
              return false;
            }

            LocalInfo::ValueKind keyKind = valueKindFromTypeName(expr.templateArgs[0]);
            LocalInfo::ValueKind valueKind = valueKindFromTypeName(expr.templateArgs[1]);
            if (keyKind == LocalInfo::ValueKind::Unknown || valueKind == LocalInfo::ValueKind::Unknown ||
                valueKind == LocalInfo::ValueKind::String) {
              error = "native backend only supports numeric/bool map values";
              return false;
            }

            if (expr.args.size() > static_cast<size_t>(std::numeric_limits<int32_t>::max())) {
              error = "map literal too large for native backend";
              return false;
            }

            const int32_t baseLocal = nextLocal;
            nextLocal += static_cast<int32_t>(1 + expr.args.size());

            const int32_t pairCount = static_cast<int32_t>(expr.args.size() / 2);
            instructions.push_back({IrOpcode::PushI32, static_cast<uint64_t>(pairCount)});
            instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal)});

            for (size_t i = 0; i < expr.args.size(); ++i) {
              const Expr &arg = expr.args[i];
              const int32_t slot = baseLocal + 1 + static_cast<int32_t>(i);
              if (i % 2 == 0 && keyKind == LocalInfo::ValueKind::String) {
                int32_t stringIndex = -1;
                size_t length = 0;
                if (!resolveStringTableTarget(arg, localsIn, stringIndex, length)) {
                  error =
                      "native backend requires map literal string keys to be string literals or bindings backed by literals";
                  return false;
                }
                instructions.push_back({IrOpcode::PushI32, static_cast<uint64_t>(stringIndex)});
                instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(slot)});
                continue;
              }

              LocalInfo::ValueKind argKind = inferExprKind(arg, localsIn);
              if (argKind == LocalInfo::ValueKind::Unknown || argKind == LocalInfo::ValueKind::String) {
                error = "native backend requires map literal arguments to be numeric/bool values";
                return false;
              }
              LocalInfo::ValueKind expectedKind = (i % 2 == 0) ? keyKind : valueKind;
              if (argKind != expectedKind) {
                error = (i % 2 == 0) ? "map literal key type mismatch" : "map literal value type mismatch";
                return false;
              }
              if (!emitExpr(arg, localsIn)) {
                return false;
              }
              instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(slot)});
            }

            instructions.push_back({IrOpcode::AddressOfLocal, static_cast<uint64_t>(baseLocal)});
            return true;
          }
          error = "native backend does not support " + builtin + " literals";
          return false;
        }
        if (isSimpleCallName(expr, "increment") || isSimpleCallName(expr, "decrement")) {
          const bool isIncrement = isSimpleCallName(expr, "increment");
          const char *mutateName = isIncrement ? "increment" : "decrement";
          if (expr.args.size() != 1) {
            error = std::string(mutateName) + " requires exactly one argument";
            return false;
          }
          const Expr &target = expr.args.front();
          auto emitDelta = [&](LocalInfo::ValueKind kind) -> bool {
            if (kind == LocalInfo::ValueKind::Int32) {
              instructions.push_back({IrOpcode::PushI32, 1});
              instructions.push_back({isIncrement ? IrOpcode::AddI32 : IrOpcode::SubI32, 0});
              return true;
            }
            if (kind == LocalInfo::ValueKind::Int64 || kind == LocalInfo::ValueKind::UInt64) {
              instructions.push_back({IrOpcode::PushI64, 1});
              instructions.push_back({isIncrement ? IrOpcode::AddI64 : IrOpcode::SubI64, 0});
              return true;
            }
            if (kind == LocalInfo::ValueKind::Float64) {
              uint64_t bits = 0;
              double one = 1.0;
              std::memcpy(&bits, &one, sizeof(bits));
              instructions.push_back({IrOpcode::PushF64, bits});
              instructions.push_back({isIncrement ? IrOpcode::AddF64 : IrOpcode::SubF64, 0});
              return true;
            }
            if (kind == LocalInfo::ValueKind::Float32) {
              float one = 1.0f;
              uint32_t bits = 0;
              std::memcpy(&bits, &one, sizeof(bits));
              instructions.push_back({IrOpcode::PushF32, static_cast<uint64_t>(bits)});
              instructions.push_back({isIncrement ? IrOpcode::AddF32 : IrOpcode::SubF32, 0});
              return true;
            }
            error = std::string(mutateName) + " requires numeric operand";
            return false;
          };
          if (target.kind == Expr::Kind::Name) {
            auto it = localsIn.find(target.name);
            if (it == localsIn.end()) {
              error = std::string(mutateName) + " target must be a known binding: " + target.name;
              return false;
            }
            if (!it->second.isMutable) {
              error = std::string(mutateName) + " target must be mutable: " + target.name;
              return false;
            }
            if (it->second.kind == LocalInfo::Kind::Reference) {
              const int32_t ptrLocal = allocTempLocal();
              instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
              instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(ptrLocal)});
              instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
              instructions.push_back({IrOpcode::LoadIndirect, 0});
              if (!emitDelta(it->second.valueKind)) {
                return false;
              }
              const int32_t valueLocal = allocTempLocal();
              instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(valueLocal)});
              instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
              instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal)});
              instructions.push_back({IrOpcode::StoreIndirect, 0});
              instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal)});
              return true;
            }
            if (it->second.kind != LocalInfo::Kind::Value) {
              error = std::string(mutateName) + " target must be a mutable binding";
              return false;
            }
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
            if (!emitDelta(it->second.valueKind)) {
              return false;
            }
            instructions.push_back({IrOpcode::Dup, 0});
            instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(it->second.index)});
            return true;
          }
          if (target.kind == Expr::Kind::Call && isSimpleCallName(target, "dereference")) {
            if (target.args.size() != 1) {
              error = "dereference requires exactly one argument";
              return false;
            }
            LocalInfo::ValueKind valueKind = inferExprKind(target, localsIn);
            if (valueKind == LocalInfo::ValueKind::Unknown || valueKind == LocalInfo::ValueKind::Bool ||
                valueKind == LocalInfo::ValueKind::String) {
              error = std::string(mutateName) + " requires numeric operand";
              return false;
            }
            const Expr &pointerExpr = target.args.front();
            const int32_t ptrLocal = allocTempLocal();
            if (pointerExpr.kind == Expr::Kind::Name) {
              auto it = localsIn.find(pointerExpr.name);
              if (it == localsIn.end() || !it->second.isMutable) {
                error = std::string(mutateName) + " target must be a mutable binding";
                return false;
              }
              if (it->second.kind != LocalInfo::Kind::Pointer && it->second.kind != LocalInfo::Kind::Reference) {
                error = std::string(mutateName) + " target must be a mutable pointer binding";
                return false;
              }
              instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
            } else {
              if (!emitExpr(pointerExpr, localsIn)) {
                return false;
              }
            }
            instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(ptrLocal)});
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
            instructions.push_back({IrOpcode::LoadIndirect, 0});
            if (!emitDelta(valueKind)) {
              return false;
            }
            const int32_t valueLocal = allocTempLocal();
            instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(valueLocal)});
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal)});
            instructions.push_back({IrOpcode::StoreIndirect, 0});
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal)});
            return true;
          }
          error = std::string(mutateName) + " target must be a mutable binding";
          return false;
        }
        if (isSimpleCallName(expr, "assign")) {
          if (expr.args.size() != 2) {
            error = "assign requires exactly two arguments";
            return false;
          }
          const Expr &target = expr.args.front();
          if (target.kind == Expr::Kind::Name) {
            auto it = localsIn.find(target.name);
            if (it == localsIn.end()) {
              error = "assign target must be a known binding: " + target.name;
              return false;
            }
            if (!it->second.isMutable) {
              error = "assign target must be mutable: " + target.name;
              return false;
            }
            if (it->second.kind == LocalInfo::Kind::Reference) {
              const int32_t ptrLocal = allocTempLocal();
              instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
              instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(ptrLocal)});
              const int32_t valueLocal = allocTempLocal();
              if (!emitExpr(expr.args[1], localsIn)) {
                return false;
              }
              instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(valueLocal)});
              instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
              instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal)});
              instructions.push_back({IrOpcode::StoreIndirect, 0});
              instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal)});
              return true;
            }
            if (it->second.kind == LocalInfo::Kind::Value && !it->second.structTypeName.empty()) {
              std::string rhsStruct = inferStructExprPath(expr.args[1], localsIn);
              if (rhsStruct.empty() || rhsStruct != it->second.structTypeName) {
                error = "assign requires matching struct value";
                return false;
              }
              int32_t structSlotCount = 0;
              if (!resolveStructSlotCount(it->second.structTypeName, structSlotCount)) {
                return false;
              }
              const int32_t destPtrLocal = allocTempLocal();
              instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
              instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(destPtrLocal)});
              if (!emitExpr(expr.args[1], localsIn)) {
                return false;
              }
              const int32_t srcPtrLocal = allocTempLocal();
              instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(srcPtrLocal)});
              if (!emitStructCopyFromPtrs(destPtrLocal, srcPtrLocal, structSlotCount)) {
                return false;
              }
              instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
              return true;
            }
            if (!emitExpr(expr.args[1], localsIn)) {
              return false;
            }
            instructions.push_back({IrOpcode::Dup, 0});
            instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(it->second.index)});
            return true;
          }
          if (target.kind == Expr::Kind::Call && isSimpleCallName(target, "dereference")) {
            if (target.args.size() != 1) {
              error = "dereference requires exactly one argument";
              return false;
            }
            const Expr &pointerExpr = target.args.front();
            const int32_t ptrLocal = allocTempLocal();
            if (pointerExpr.kind == Expr::Kind::Name) {
              auto it = localsIn.find(pointerExpr.name);
              if (it == localsIn.end() || !it->second.isMutable) {
                error = "assign target must be a mutable binding";
                return false;
              }
              if (it->second.kind != LocalInfo::Kind::Pointer && it->second.kind != LocalInfo::Kind::Reference) {
                error = "assign target must be a mutable pointer binding";
                return false;
              }
              instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
            } else {
              if (!emitExpr(pointerExpr, localsIn)) {
                return false;
              }
            }
            instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(ptrLocal)});
            const int32_t valueLocal = allocTempLocal();
            if (!emitExpr(expr.args[1], localsIn)) {
              return false;
            }
            instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(valueLocal)});
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal)});
            instructions.push_back({IrOpcode::StoreIndirect, 0});
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal)});
            return true;
          }
          error = "native backend only supports assign to local names or dereference";
          return false;
        }
  handled = false;
  return true;
}

} // namespace primec::ir_lowerer
