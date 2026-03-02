#pragma once

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

using InternStringFn = std::function<int32_t(const std::string &)>;
using EmitInstructionFn = std::function<void(IrOpcode, uint64_t)>;
using LookupStringBindingFn = std::function<StringBindingInfo(const std::string &)>;

StringCallEmitResult emitLiteralOrBindingStringCallValue(const Expr &arg,
                                                         const InternStringFn &internString,
                                                         const EmitInstructionFn &emitInstruction,
                                                         const LookupStringBindingFn &lookupBinding,
                                                         StringCallSource &sourceOut,
                                                         int32_t &stringIndexOut,
                                                         bool &argvCheckedOut,
                                                         std::string &error);

} // namespace primec::ir_lowerer
