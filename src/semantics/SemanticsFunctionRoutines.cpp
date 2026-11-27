#include "SemanticsFunctionRoutines.h"

#include <cctype>

namespace primec::semantics {

namespace {

std::string leafOfPath(std::string_view path) {
  const size_t slash = path.find_last_of('/');
  return std::string(slash == std::string_view::npos ? path : path.substr(slash + 1));
}

constexpr std::string_view Reinterpret = "/std/intrinsics/memory/reinterpret";

} // namespace

std::string functionRoutineId(std::string_view text) {
  std::string out;
  for (const char ch : text) {
    out.push_back(std::isalnum(static_cast<unsigned char>(ch)) != 0 || ch == '_' ? ch : '_');
  }
  return out;
}

std::string functionInvokeAddressPath(std::string_view instancePath, std::string_view closureType) {
  return "/__fn_address_invoke_" + functionRoutineId(leafOfPath(instancePath)) + "_" +
         functionRoutineId(closureType);
}

std::string functionCloneAddressPath(std::string_view closureType) {
  return "/__fn_address_clone_" + functionRoutineId(closureType);
}

std::string functionReleaseAddressPath(std::string_view closureType) {
  return "/__fn_address_release_" + functionRoutineId(closureType);
}

std::string functionInvokeRoutinePath(std::string_view instancePath, std::string_view closureType) {
  return "/__fn_invoke_" + functionRoutineId(leafOfPath(instancePath)) + "_" + functionRoutineId(closureType);
}

std::string functionCallDeclarationPath(std::string_view instancePath) {
  return "/__fn_call_" + functionRoutineId(leafOfPath(instancePath));
}

std::string functionSharedCallDeclarationsText() {
  return "[indirect_call unsafe effects(heap_alloc) return<Pointer<i32>>]\n"
         "__fn_clone_call([u64] address, [Pointer<i32>] env) {}\n"
         "[indirect_call unsafe effects(heap_alloc) return<void>]\n"
         "__fn_release_call([u64] address, [Pointer<i32>] env) {}\n";
}

std::string functionCloneRoutinePath(std::string_view closureType) {
  return "/__fn_clone_" + functionRoutineId(closureType);
}

std::string functionReleaseRoutinePath(std::string_view closureType) {
  return "/__fn_release_" + functionRoutineId(closureType);
}

std::string functionCloneRoutineText(std::string_view closureType) {
  const std::string closure(closureType);
  const std::string reinterpret(Reinterpret);
  return "[unsafe effects(heap_alloc) return<Pointer<i32>>]\n" + functionCloneRoutinePath(closureType).substr(1) +
         "([Pointer<i32>] env) {\n"
         "  [Pointer<" + closure + "> mut] source{" + reinterpret + "<" + closure + ">(env)}\n"
         "  [Pointer<uninitialized<" + closure + ">> mut] slot{/std/intrinsics/memory/alloc<uninitialized<" + closure +
         ">>(1i32)}\n"
         "  [" + closure + "] copied{dereference(source)}\n"
         "  init(dereference(slot), copied)\n"
         "  return(" + reinterpret + "<i32>(slot))\n"
         "}\n";
}

std::string functionReleaseRoutineText(std::string_view closureType) {
  const std::string closure(closureType);
  const std::string reinterpret(Reinterpret);
  return "[unsafe effects(heap_alloc) return<void>]\n" + functionReleaseRoutinePath(closureType).substr(1) +
         "([Pointer<i32>] env) {\n"
         "  [Pointer<uninitialized<" + closure + ">> mut] slot{" + reinterpret + "<uninitialized<" + closure +
         ">>(env)}\n"
         "  drop(dereference(slot))\n"
         "  /std/intrinsics/memory/free(slot)\n"
         "}\n";
}

std::string functionAddressGetterText(std::string_view getterPath, std::string_view routinePath) {
  return "[function_address(\"" + std::string(routinePath) + "\"utf8) return<u64> effects(none)]\n" +
         std::string(getterPath).substr(1) + "() {}\n";
}

std::string functionInvokeBodyText(std::string_view closureType, std::string_view arguments, bool returnsVoid) {
  const std::string closure(closureType);
  const std::string invoke = "dereference(target).call(" + std::string(arguments) + ")";
  return "  [Pointer<" + closure + "> mut] target{" + std::string(Reinterpret) + "<" + closure + ">(env)}\n" +
         (returnsVoid ? "  " + invoke + "\n  return()\n" : "  return(" + invoke + ")\n");
}

} // namespace primec::semantics
