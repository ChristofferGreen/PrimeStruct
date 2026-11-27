#pragma once

#include <string>
#include <string_view>

namespace primec::semantics {

// The routines a closure type gets when it is converted to a `Function` (docs/spec/value-lifecycle.md, Function
// values). A `Function` holds the addresses of three routines of its closure and the closure's heap block:
//   invoke   enters the closure's `call` with the block and the `Function`'s arguments,
//   clone    copies the closure into a new block of its own and returns it,
//   release  destroys the closure in its block and frees the block.
// Each routine is a definition of its own and each address comes from a `[function_address]` getter, so a library's
// `Function` calls the routines of the program that converted the closure.

// The name a text takes in a generated path: letters, digits and underscores, the rest an underscore.
std::string functionRoutineId(std::string_view text);

// The path of the getter of the address of a routine: `invoke`, `clone` or `release`, for the closure type of a
// conversion and, for `invoke`, the Function instance it is converted to.
std::string functionInvokeAddressPath(std::string_view instancePath, std::string_view closureType);
std::string functionCloneAddressPath(std::string_view closureType);
std::string functionReleaseAddressPath(std::string_view closureType);

// The declaration a `Function` instance's `call` makes through its address (`[indirect_call]`, with the instance's
// signature after the address and the closure's block), and the two every instance's `Copy` and `Destroy` make
// (`/__fn_clone_call` and `/__fn_release_call`).
std::string functionCallDeclarationPath(std::string_view instancePath);
std::string functionSharedCallDeclarationsText();

// The paths of the routines themselves.
std::string functionInvokeRoutinePath(std::string_view instancePath, std::string_view closureType);
std::string functionCloneRoutinePath(std::string_view closureType);
std::string functionReleaseRoutinePath(std::string_view closureType);

// The definitions of the clone and release routines of a closure type, and the getters of every routine's address,
// as source text for the parser.
std::string functionCloneRoutineText(std::string_view closureType);
std::string functionReleaseRoutineText(std::string_view closureType);
std::string functionAddressGetterText(std::string_view getterPath, std::string_view routinePath);

// The body of the invoke routine of a closure type: the closure's `call` on the block, with the argument names the
// routine takes. `returnsVoid` says whether the `Function` returns a value.
std::string functionInvokeBodyText(std::string_view closureType, std::string_view arguments, bool returnsVoid);

} // namespace primec::semantics
