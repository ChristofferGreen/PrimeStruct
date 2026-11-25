#pragma once

#include "primec/ast/Ast.h"

#include <string>

namespace primec::ir_lowerer {

// The first rule of the address ABI's shape (a real call on the VM and the optimizing C++ emitters) that a
// definition breaks, other than a parameter type. `None` means the shape is taken.
enum class AddressAbiBlocker {
  None,
  HostDefinition,
  StructDefinition,
  SumDefinition,
  ComputeDefinition,
  RequireContract,
  CollectionsHelper,
  LifecycleHelper,
  ReceiverStruct,
  ClosureReferenceCapture,
  Parameter,
  SumParameter,
  CollectionResult,
  VectorResultNotOwned,
  UnsupportedResult,
  UsesTry,
  GeneratedTailValue,
};

// A stable, hyphenated code for each blocker (`on-error-handler`, `sum-parameter`, ...), for reports and tests; empty
// for `None`. The codes never change spelling; a new blocker gets a new code.
const char *addressAbiBlockerCode(AddressAbiBlocker blocker);

// Text that names what `def` does that a real call cannot take, for a diagnostic (`public function /f cannot be
// compiled into a library: <text>`). Empty for `None`.
std::string describeAddressAbiBlocker(AddressAbiBlocker blocker, const Definition &def);

// The mutable `vector<T>` bindings of a body and whether every `return` hands back one of them: a task returns the
// record of a vector it built, so the waiter takes that vector over. A parameter, a field or another expression would
// need a copy the return does not make.
bool vectorResultReturnsOwnedLocals(const Definition &def);

} // namespace primec::ir_lowerer
