#include "IrLowererOperatorConversionsAndCallsHelpers.h"

#include "IrLowererHelpers.h"

#include <algorithm>
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
    const ResolveConversionsAndCallsStructFieldInfoFn &resolveStructFieldInfo,
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
          if (builtin == "array" || builtin == "vector" || builtin == "soa_vector") {
            if (expr.templateArgs.size() != 1) {
              error = builtin + " literal requires exactly one template argument";
              return false;
            }
            if (expr.args.size() > static_cast<size_t>(std::numeric_limits<int32_t>::max())) {
              error = builtin + " literal too large for native backend";
              return false;
            }

            const bool isSoaVector = (builtin == "soa_vector");
            const bool isVectorLike = (builtin == "vector" || isSoaVector);
            LocalInfo::ValueKind elemKind = valueKindFromTypeName(expr.templateArgs.front());
            if (!isSoaVector &&
                (elemKind == LocalInfo::ValueKind::Unknown || elemKind == LocalInfo::ValueKind::String)) {
              error = "native backend only supports numeric/bool " + builtin + " literals";
              return false;
            }
            if (isSoaVector && !expr.args.empty()) {
              error = "native backend does not support non-empty soa_vector literals";
              return false;
            }

            const int32_t baseLocal = nextLocal;
            const int32_t headerSlots = isVectorLike ? 3 : 1;
            const int32_t literalCount = static_cast<int32_t>(expr.args.size());
            if (isVectorLike && literalCount > kVectorLocalDynamicCapacityLimit) {
              if (isSoaVector) {
                error = "soa_vector literal exceeds local capacity limit (256)";
              } else {
                error = vectorLiteralExceedsLocalCapacityLimitMessage();
              }
              return false;
            }
            const int32_t storageCapacity =
                isVectorLike ? std::max(literalCount, kVectorLocalDynamicCapacityLimit) : literalCount;
            const int32_t dataBaseLocal = baseLocal + headerSlots;
            nextLocal += isVectorLike ? headerSlots : (headerSlots + storageCapacity);

            instructions.push_back({IrOpcode::PushI32, static_cast<uint64_t>(literalCount)});
            instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal)});
            if (isVectorLike) {
              instructions.push_back({IrOpcode::PushI32, static_cast<uint64_t>(literalCount)});
              instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal + 1)});
              if (isSoaVector) {
                instructions.push_back({IrOpcode::PushI64, 0});
              } else {
                instructions.push_back({IrOpcode::PushI32, static_cast<uint64_t>(storageCapacity)});
                instructions.push_back({IrOpcode::HeapAlloc, 0});
              }
              instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal + 2)});
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
              if (isVectorLike) {
                instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(baseLocal + 2)});
                const uint64_t offsetBytes = static_cast<uint64_t>(i) * IrSlotBytes;
                if (offsetBytes != 0) {
                  instructions.push_back({IrOpcode::PushI64, offsetBytes});
                  instructions.push_back({IrOpcode::AddI64, 0});
                }
                if (!emitExpr(arg, localsIn)) {
                  return false;
                }
                instructions.push_back({IrOpcode::StoreIndirect, 0});
                instructions.push_back({IrOpcode::Pop, 0});
              } else {
                if (!emitExpr(arg, localsIn)) {
                  return false;
                }
                instructions.push_back(
                    {IrOpcode::StoreLocal, static_cast<uint64_t>(dataBaseLocal + static_cast<int32_t>(i))});
              }
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
                if (resolveStringTableTarget(arg, localsIn, stringIndex, length)) {
                  instructions.push_back({IrOpcode::PushI32, static_cast<uint64_t>(stringIndex)});
                  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(slot)});
                  continue;
                }
                const LocalInfo::ValueKind keyArgKind = inferExprKind(arg, localsIn);
                if (keyArgKind != LocalInfo::ValueKind::String) {
                  error = "map literal key type mismatch";
                  return false;
                }
                if (!emitExpr(arg, localsIn)) {
                  return false;
                }
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
          if (target.kind == Expr::Kind::Call && target.isFieldAccess) {
            if (target.args.size() != 1) {
              error = "assign target must be a mutable binding";
              return false;
            }
            const Expr &receiverExpr = target.args.front();
            const std::string receiverStruct = inferStructExprPath(receiverExpr, localsIn);
            if (receiverStruct.empty()) {
              error = "assign target must be a mutable binding";
              return false;
            }
            int32_t fieldSlotOffset = 0;
            int32_t fieldSlotCount = 0;
            std::string fieldStructPath;
            if (!resolveStructFieldInfo(receiverStruct,
                                        target.name,
                                        fieldSlotOffset,
                                        fieldSlotCount,
                                        fieldStructPath)) {
              error = "assign target must be a mutable binding";
              return false;
            }
            auto emitReceiverPointer = [&]() -> bool {
              if (receiverExpr.kind == Expr::Kind::Name) {
                auto it = localsIn.find(receiverExpr.name);
                if (it != localsIn.end() && it->second.kind == LocalInfo::Kind::Reference) {
                  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
                  return true;
                }
              }
              return emitExpr(receiverExpr, localsIn);
            };
            if (!emitReceiverPointer()) {
              return false;
            }
            const int32_t receiverPtrLocal = allocTempLocal();
            instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(receiverPtrLocal)});
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(receiverPtrLocal)});
            const uint64_t fieldOffsetBytes = static_cast<uint64_t>(fieldSlotOffset) * IrSlotBytes;
            if (fieldOffsetBytes != 0) {
              instructions.push_back({IrOpcode::PushI64, fieldOffsetBytes});
              instructions.push_back({IrOpcode::AddI64, 0});
            }
            const int32_t destPtrLocal = allocTempLocal();
            instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(destPtrLocal)});
            if (!fieldStructPath.empty()) {
              const std::string rhsStruct = inferStructExprPath(expr.args[1], localsIn);
              if (rhsStruct.empty() || rhsStruct != fieldStructPath) {
                error = "assign requires matching struct value";
                return false;
              }
              if (!emitExpr(expr.args[1], localsIn)) {
                return false;
              }
              const int32_t srcPtrLocal = allocTempLocal();
              instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(srcPtrLocal)});
              if (!emitStructCopyFromPtrs(destPtrLocal, srcPtrLocal, fieldSlotCount)) {
                return false;
              }
              instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(destPtrLocal)});
              return true;
            }
            const int32_t valueLocal = allocTempLocal();
            if (!emitExpr(expr.args[1], localsIn)) {
              return false;
            }
            instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(valueLocal)});
            instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(destPtrLocal)});
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

bool emitConversionsAndCallsControlExprTail(
    const Expr &expr,
    const LocalMap &localsIn,
    const EmitConversionsAndCallsExprWithLocalsFn &emitExpr,
    const EmitConversionsAndCallsStatementWithLocalsFn &emitStatement,
    const InferConversionsAndCallsExprKindWithLocalsFn &inferExprKind,
    const CombineConversionsAndCallsNumericKindsFn &combineNumericKinds,
    const HasConversionsAndCallsNamedArgumentsFn &hasNamedArguments,
    const ResolveConversionsAndCallsDefinitionCallFn &resolveDefinitionCall,
    const ResolveConversionsAndCallsExprPathFn &resolveExprPath,
    const LowerConversionsAndCallsMatchToIfFn &lowerMatchToIf,
    const IsConversionsAndCallsBindingMutableFn &isBindingMutable,
    const ConversionsAndCallsBindingKindFn &bindingKind,
    const HasConversionsAndCallsExplicitBindingTypeTransformFn &hasExplicitBindingTypeTransform,
    const ConversionsAndCallsBindingValueKindFn &bindingValueKind,
    const InferConversionsAndCallsStructExprPathFn &inferStructExprPath,
    const ApplyConversionsAndCallsStructArrayInfoFn &applyStructArrayInfo,
    const ApplyConversionsAndCallsStructValueInfoFn &applyStructValueInfo,
    const EnterConversionsAndCallsScopedBlockFn &enterScopedBlock,
    const ExitConversionsAndCallsScopedBlockFn &exitScopedBlock,
    const IsConversionsAndCallsReturnCallFn &isReturnCall,
    const IsConversionsAndCallsBlockCallFn &isBlockCall,
    const IsConversionsAndCallsMatchCallFn &isMatchCall,
    const IsConversionsAndCallsIfCallFn &isIfCall,
    std::vector<IrInstruction> &instructions,
    bool &handled,
    std::string &error) {
  handled = true;
  if (isBlockCall(expr) && expr.hasBodyArguments) {
    if (!expr.args.empty() || !expr.templateArgs.empty() || hasNamedArguments(expr.argNames)) {
      error = "block expression does not accept arguments";
      return false;
    }
    if (resolveDefinitionCall(expr) != nullptr) {
      error = "block arguments require a definition target: " + resolveExprPath(expr);
      return false;
    }
    if (expr.bodyArguments.empty()) {
      error = "block expression requires a value";
      return false;
    }
    LocalMap blockLocals = localsIn;
    for (size_t i = 0; i < expr.bodyArguments.size(); ++i) {
      const Expr &bodyStmt = expr.bodyArguments[i];
      const bool isLast = (i + 1 == expr.bodyArguments.size());
      if (bodyStmt.isBinding) {
        if (isLast) {
          error = "block expression must end with an expression";
          return false;
        }
        if (!emitStatement(bodyStmt, blockLocals)) {
          return false;
        }
        continue;
      }
      if (isReturnCall(bodyStmt)) {
        if (bodyStmt.args.size() != 1) {
          error = "return requires a value in block expression";
          return false;
        }
        return emitExpr(bodyStmt.args.front(), blockLocals);
      }
      if (isLast) {
        return emitExpr(bodyStmt, blockLocals);
      }
      if (!emitStatement(bodyStmt, blockLocals)) {
        return false;
      }
    }
    error = "block expression requires a value";
    return false;
  }

  if (isMatchCall(expr)) {
    Expr expanded;
    if (!lowerMatchToIf(expr, expanded, error)) {
      return false;
    }
    return emitExpr(expanded, localsIn);
  }

  if (isIfCall(expr)) {
    if (expr.args.size() != 3) {
      error = "if requires condition, then, else";
      return false;
    }
    if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
      error = "if does not accept trailing block arguments";
      return false;
    }

    const Expr &cond = expr.args[0];
    const Expr &thenArg = expr.args[1];
    const Expr &elseArg = expr.args[2];
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
      return false;
    }

    auto inferBranchValueKind = [&](const Expr &candidate, const LocalMap &localsBase) -> LocalInfo::ValueKind {
      LocalMap branchLocals = localsBase;
      bool sawValue = false;
      LocalInfo::ValueKind lastKind = LocalInfo::ValueKind::Unknown;
      for (const auto &bodyExpr : candidate.bodyArguments) {
        if (bodyExpr.isBinding) {
          if (bodyExpr.args.size() != 1) {
            return LocalInfo::ValueKind::Unknown;
          }
          LocalInfo info;
          info.index = 0;
          info.isMutable = isBindingMutable(bodyExpr);
          info.kind = bindingKind(bodyExpr);
          LocalInfo::ValueKind valueKind = LocalInfo::ValueKind::Unknown;
          if (hasExplicitBindingTypeTransform(bodyExpr)) {
            valueKind = bindingValueKind(bodyExpr, info.kind);
          } else if (bodyExpr.args.size() == 1 && info.kind == LocalInfo::Kind::Value) {
            valueKind = inferExprKind(bodyExpr.args.front(), branchLocals);
            if (valueKind == LocalInfo::ValueKind::Unknown) {
              std::string inferredStruct = inferStructExprPath(bodyExpr.args.front(), branchLocals);
              if (!inferredStruct.empty()) {
                info.structTypeName = inferredStruct;
              } else {
                valueKind = LocalInfo::ValueKind::Int32;
              }
            }
          }
          info.valueKind = valueKind;
          applyStructArrayInfo(bodyExpr, info);
          applyStructValueInfo(bodyExpr, info);
          if (info.structTypeName.empty() && info.kind == LocalInfo::Kind::Value &&
              info.valueKind == LocalInfo::ValueKind::Unknown && bodyExpr.args.size() == 1) {
            std::string inferredStruct = inferStructExprPath(bodyExpr.args.front(), branchLocals);
            if (!inferredStruct.empty()) {
              info.structTypeName = inferredStruct;
            } else {
              info.valueKind = LocalInfo::ValueKind::Int32;
            }
          }
          branchLocals.emplace(bodyExpr.name, info);
          continue;
        }
        if (isReturnCall(bodyExpr)) {
          if (bodyExpr.args.size() != 1) {
            return LocalInfo::ValueKind::Unknown;
          }
          return inferExprKind(bodyExpr.args.front(), branchLocals);
        }
        sawValue = true;
        lastKind = inferExprKind(bodyExpr, branchLocals);
      }
      return sawValue ? lastKind : LocalInfo::ValueKind::Unknown;
    };

    auto emitBranchValue = [&](const Expr &candidate, const LocalMap &localsBase) -> bool {
      if (!isIfBlockEnvelope(candidate)) {
        return emitExpr(candidate, localsBase);
      }
      if (candidate.bodyArguments.empty()) {
        error = "if branch blocks must produce a value";
        return false;
      }

      enterScopedBlock();
      auto leaveScope = [&]() { exitScopedBlock(); };

      LocalMap branchLocals = localsBase;
      for (size_t i = 0; i < candidate.bodyArguments.size(); ++i) {
        const Expr &bodyStmt = candidate.bodyArguments[i];
        const bool isLast = (i + 1 == candidate.bodyArguments.size());
        if (bodyStmt.isBinding) {
          if (isLast) {
            error = "if branch blocks must end with an expression";
            leaveScope();
            return false;
          }
          if (!emitStatement(bodyStmt, branchLocals)) {
            leaveScope();
            return false;
          }
          continue;
        }
        if (isReturnCall(bodyStmt)) {
          if (bodyStmt.args.size() != 1) {
            error = "return requires a value in if branch";
            leaveScope();
            return false;
          }
          bool ok = emitExpr(bodyStmt.args.front(), branchLocals);
          leaveScope();
          return ok;
        }
        if (isLast) {
          bool ok = emitExpr(bodyStmt, branchLocals);
          leaveScope();
          return ok;
        }
        if (!emitStatement(bodyStmt, branchLocals)) {
          leaveScope();
          return false;
        }
      }

      leaveScope();
      error = "if branch blocks must produce a value";
      return false;
    };

    if (!emitExpr(cond, localsIn)) {
      return false;
    }
    LocalInfo::ValueKind condKind = inferExprKind(cond, localsIn);
    if (condKind != LocalInfo::ValueKind::Bool) {
      error = "if condition requires bool";
      return false;
    }

    LocalInfo::ValueKind thenKind = inferBranchValueKind(thenArg, localsIn);
    LocalInfo::ValueKind elseKind = inferBranchValueKind(elseArg, localsIn);
    LocalInfo::ValueKind resultKind = LocalInfo::ValueKind::Unknown;
    if (thenKind == elseKind) {
      resultKind = thenKind;
    } else {
      resultKind = combineNumericKinds(thenKind, elseKind);
    }
    if (resultKind == LocalInfo::ValueKind::Unknown) {
      error = "if branches must return compatible types";
      return false;
    }

    size_t jumpIfZeroIndex = instructions.size();
    instructions.push_back({IrOpcode::JumpIfZero, 0});
    if (!emitBranchValue(thenArg, localsIn)) {
      return false;
    }
    size_t jumpEndIndex = instructions.size();
    instructions.push_back({IrOpcode::Jump, 0});
    size_t elseIndex = instructions.size();
    instructions[jumpIfZeroIndex].imm = static_cast<int32_t>(elseIndex);
    if (!emitBranchValue(elseArg, localsIn)) {
      return false;
    }
    size_t endIndex = instructions.size();
    instructions[jumpEndIndex].imm = static_cast<int32_t>(endIndex);
    return true;
  }

  handled = false;
  return true;
}

} // namespace primec::ir_lowerer
