#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

#include "primec/Ast.h"
#include "primec/Ir.h"

namespace primec::ir_lowerer {

enum class StringCallSource { None, TableIndex, ArgvIndex, RuntimeIndex };

struct StringBindingInfo {
  bool found = false;
  bool isString = false;
  int32_t localIndex = 0;
  StringCallSource source = StringCallSource::None;
  int32_t stringIndex = -1;
  bool argvChecked = true;
};

enum class StringCallEmitResult { Handled, NotHandled, Error };

struct StringIndexOps {
  IrOpcode pushZero = IrOpcode::PushI64;
  IrOpcode cmpLt = IrOpcode::CmpLtI64;
  IrOpcode cmpGe = IrOpcode::CmpGeI64;
  bool skipNegativeCheck = false;
};

using InternStringFn = std::function<int32_t(const std::string &)>;
using EmitInstructionFn = std::function<void(IrOpcode, uint64_t)>;
using LookupStringBindingFn = std::function<StringBindingInfo(const std::string &)>;
using ResolveArrayAccessNameFn = std::function<bool(const Expr &, std::string &)>;
using IsEntryArgsNameFn = std::function<bool(const Expr &)>;
using ResolveStringIndexOpsFn = std::function<bool(const Expr &, const std::string &, StringIndexOps &, std::string &)>;
using EmitExprFn = std::function<bool(const Expr &)>;
using InferCallReturnsStringFn = std::function<bool(const Expr &)>;
using AllocTempLocalFn = std::function<int32_t()>;
using EmitArrayIndexOutOfBoundsFn = std::function<void()>;
using GetInstructionCountFn = std::function<size_t()>;
using PatchInstructionImmFn = std::function<void(size_t, int32_t)>;

StringCallEmitResult emitLiteralOrBindingStringCallValue(const Expr &arg,
                                                         const InternStringFn &internString,
                                                         const EmitInstructionFn &emitInstruction,
                                                         const LookupStringBindingFn &lookupBinding,
                                                         StringCallSource &sourceOut,
                                                         int32_t &stringIndexOut,
                                                         bool &argvCheckedOut,
                                                         std::string &error);

StringCallEmitResult emitCallStringCallValue(const Expr &arg,
                                             const ResolveArrayAccessNameFn &resolveArrayAccessName,
                                             const IsEntryArgsNameFn &isEntryArgsName,
                                             const ResolveStringIndexOpsFn &resolveStringIndexOps,
                                             const EmitExprFn &emitExpr,
                                             const InferCallReturnsStringFn &inferCallReturnsString,
                                             const AllocTempLocalFn &allocTempLocal,
                                             const EmitInstructionFn &emitInstruction,
                                             const GetInstructionCountFn &getInstructionCount,
                                             const PatchInstructionImmFn &patchInstructionImm,
                                             const EmitArrayIndexOutOfBoundsFn &emitArrayIndexOutOfBounds,
                                             StringCallSource &sourceOut,
                                             bool &argvCheckedOut,
                                             std::string &error);

} // namespace primec::ir_lowerer
