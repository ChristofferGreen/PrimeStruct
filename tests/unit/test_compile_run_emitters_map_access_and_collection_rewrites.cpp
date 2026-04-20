#include "test_compile_run_helpers.h"
#include "test_compile_run_collection_conformance_helpers.h"
#include "test_compile_run_emitters_helpers.h"

#include "primec/testing/EmitterHelpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.emitters.cpp");

TEST_CASE("rejects user map access unsafe string positional call shadow in C++ emitter") {
  const std::string source = R"(
[return<int>]
/map/at_unsafe([map<string, i32>] values, [string] key) {
  return(85i32)
}

[return<int>]
main() {
  [map<string, i32>] values{map<string, i32>("only"raw_utf8, 2i32)}
  return(at_unsafe("only"raw_utf8, values))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_map_access_unsafe_string_positional_call_shadow.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_map_access_unsafe_string_positional_call_shadow_err.txt")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK_FALSE(readFile(errPath).empty());
}

TEST_CASE("C++ emitter rejects canonical map access positional reorder") {
  const std::string source = R"(
[return<int>]
/std/collections/map/at([/std/collections/map<string, i32>] values, [string] key) {
  return(86i32)
}

[return<int>]
main() {
  [/std/collections/map<string, i32>] values{map<string, i32>("only"raw_utf8, 2i32)}
  return(/std/collections/map/at("only"raw_utf8, values))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_canonical_map_access_positional_reorder.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_canonical_map_access_positional_reorder_err.txt")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("argument type mismatch for /std/collections/map/at parameter values") !=
        std::string::npos);
}

TEST_CASE("C++ emitter keeps canonical map access key diagnostics on positional reorder") {
  const std::string source = R"(
[return<int>]
/std/collections/map/at([/std/collections/map<string, i32>] values, [string] key) {
  return(86i32)
}

[return<int>]
main() {
  [/std/collections/map<string, i32>] values{map<string, i32>("only"raw_utf8, 2i32)}
  return(/std/collections/map/at(1i32, values))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_canonical_map_access_positional_reorder_diag.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_canonical_map_access_positional_reorder_diag_exe")
          .string();
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_canonical_map_access_positional_reorder_diag_err.txt")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main > /dev/null 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("argument type mismatch for /std/collections/map/at parameter key") !=
        std::string::npos);
}

TEST_CASE("C++ emitter access rewrite keeps known collection receiver leading names") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
/i32/at([i32] index, [vector<i32>] values) {
  return(66i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  [i32] index{1i32}
  return(at(values, index))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_access_known_receiver_no_reorder.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_access_known_receiver_no_reorder_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("rejects user vector mutator shadow arg mismatch in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc)]
/vector/push([vector<i32> mut] values, [bool] value) { }

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  push(values, 1i32)
  values.push(2i32)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_cpp_user_vector_mutator_shadow_arg_mismatch.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_user_vector_mutator_shadow_arg_mismatch_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects user vector mutator call-form arg mismatch in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc)]
/vector/push([vector<i32> mut] values, [bool] value) { }

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  push(values, 1i32)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_cpp_vector_mutator_call_shadow_mismatch.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_vector_mutator_call_shadow_mismatch_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("compiles and runs stdlib namespaced vector builtin aliases in C++ emitter") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [auto mut] values{/std/collections/vector/vector<i32>(4i32, 5i32)}
  /std/collections/vector/push(values, 6i32)
  [i32] countValue{/std/collections/vector/count(values)}
  [i32] capacityValue{/std/collections/vector/capacity(values)}
  [i32] tailValue{/std/collections/vector/at_unsafe(values, 2i32)}
  return(plus(plus(countValue, tailValue), minus(capacityValue, capacityValue)))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_stdlib_namespaced_vector_aliases.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_stdlib_namespaced_vector_aliases_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("rejects array namespaced vector constructor alias in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{/array/vector<i32>(4i32, 5i32)}
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_cpp_array_namespaced_vector_constructor_alias.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_cpp_array_namespaced_vector_constructor_alias_out.txt")
          .string();
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_array_namespaced_vector_constructor_alias_exe").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("unknown call target: /array/vector") != std::string::npos);
}

TEST_CASE("rejects array namespaced vector at alias in C++ emitter") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [auto mut] values{/std/collections/vector/vector<i32>(4i32, 5i32)}
  [i32] headValue{/array/at(values, 1i32)}
  return(headValue)
}
)";
  const std::string srcPath = writeTemp("compile_cpp_array_namespaced_vector_at_alias.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_cpp_array_namespaced_vector_at_alias_out.txt").string();
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_array_namespaced_vector_at_alias_exe").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("unknown call target: /array/at") != std::string::npos);
}

TEST_CASE("rejects array namespaced vector at_unsafe alias in C++ emitter") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [auto mut] values{/std/collections/vector/vector<i32>(4i32, 5i32)}
  [i32] tailValue{/array/at_unsafe(values, 1i32)}
  return(tailValue)
}
)";
  const std::string srcPath = writeTemp("compile_cpp_array_namespaced_vector_at_unsafe_alias.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_cpp_array_namespaced_vector_at_unsafe_alias_out.txt")
          .string();
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_array_namespaced_vector_at_unsafe_alias_exe").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("unknown call target: /array/at_unsafe") != std::string::npos);
}

TEST_CASE("rejects wrapper array namespaced vector at alias in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(4i32, 5i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(/array/at(wrapVector(), 1i32))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_wrapper_array_namespaced_vector_at_alias.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_cpp_wrapper_array_namespaced_vector_at_alias_out.txt").string();
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_wrapper_array_namespaced_vector_at_alias_exe").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("unknown call target: /array/at") != std::string::npos);
}

TEST_CASE("rejects wrapper array namespaced vector at_unsafe alias in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(4i32, 5i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(/array/at_unsafe(wrapVector(), 1i32))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_wrapper_array_namespaced_vector_at_unsafe_alias.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_cpp_wrapper_array_namespaced_vector_at_unsafe_alias_out.txt")
          .string();
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_wrapper_array_namespaced_vector_at_unsafe_alias_exe").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("unknown call target: /array/at_unsafe") != std::string::npos);
}

TEST_CASE("rejects array namespaced vector count builtin alias in C++ emitter") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [auto mut] values{/std/collections/vector/vector<i32>(4i32, 5i32)}
  return(/array/count(values))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_array_namespaced_vector_count_builtin_alias.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_cpp_array_namespaced_vector_count_builtin_alias_out.txt")
          .string();
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_array_namespaced_vector_count_builtin_alias_exe").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("unknown call target: /array/count") != std::string::npos);
}

TEST_CASE("rejects array namespaced vector count method alias in C++ emitter") {
  const std::string source = R"(
[return<bool>]
/std/collections/vector/count([vector<i32>] values, [bool] marker) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32>] values{vector<i32>(4i32, 5i32)}
  return(values./array/count(true))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_array_namespaced_vector_count_method_alias.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_cpp_array_namespaced_vector_count_method_alias_out.txt")
          .string();
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_array_namespaced_vector_count_method_alias_exe").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("unknown method: /array/count") != std::string::npos);
}

TEST_CASE("rejects array namespaced vector capacity alias in C++ emitter") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [auto mut] values{/std/collections/vector/vector<i32>(4i32, 5i32)}
  return(/array/capacity(values))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_array_namespaced_vector_capacity_alias.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_cpp_array_namespaced_vector_capacity_alias_out.txt")
          .string();
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_array_namespaced_vector_capacity_alias_exe").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("unknown call target: /array/capacity") != std::string::npos);
}

TEST_CASE("rejects array namespaced vector capacity method alias in C++ emitter") {
  const std::string source = R"(
[return<bool>]
/std/collections/vector/capacity([vector<i32>] values, [bool] marker) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32>] values{vector<i32>(4i32, 5i32)}
  return(values./array/capacity(true))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_array_namespaced_vector_capacity_method_alias.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_cpp_array_namespaced_vector_capacity_method_alias_out.txt")
          .string();
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_array_namespaced_vector_capacity_method_alias_exe").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("unknown method: /array/capacity") != std::string::npos);
}

TEST_CASE("rejects array namespaced vector mutator alias in C++ emitter") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [auto mut] values{/std/collections/vector/vector<i32>(4i32, 5i32)}
  /array/push(values, 6i32)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_cpp_array_namespaced_vector_mutator_alias.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_cpp_array_namespaced_vector_mutator_alias_out.txt").string();
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_array_namespaced_vector_mutator_alias_exe").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("unknown call target: /array/push") != std::string::npos);
}

TEST_CASE("C++ emitter helper rejects full-path array mutator aliases") {
  primec::Expr call;
  call.kind = primec::Expr::Kind::Call;
  call.name = "/array/push";

  std::unordered_map<std::string, std::string> nameMap;
  std::string helper;
  CHECK_FALSE(primec::emitter::getVectorMutatorName(call, nameMap, helper));
}

TEST_CASE("C++ emitter helper rejects rooted vector mutator aliases") {
  primec::Expr call;
  call.kind = primec::Expr::Kind::Call;
  call.name = "/vector/push";

  std::unordered_map<std::string, std::string> nameMap;
  std::string helper;
  CHECK_FALSE(primec::emitter::getVectorMutatorName(call, nameMap, helper));
}

TEST_CASE("C++ emitter helper accepts canonical vector mutators without alias bridge") {
  auto expectAccepted = [&](const char *name, const char *expectedHelper) {
    primec::Expr call;
    call.kind = primec::Expr::Kind::Call;
    call.name = name;

    std::unordered_map<std::string, std::string> nameMap;
    std::string helper;
    CHECK(primec::emitter::getVectorMutatorName(call, nameMap, helper));
    CHECK(helper == expectedHelper);
  };

  expectAccepted("/std/collections/vector/push", "push");
  expectAccepted("/std/collections/vector/clear", "clear");
  expectAccepted("/std/collections/vector/remove_at", "remove_at");
  expectAccepted("/std/collections/vector/remove_swap", "remove_swap");

  primec::Expr namespacedPushCall;
  namespacedPushCall.kind = primec::Expr::Kind::Call;
  namespacedPushCall.name = "push";
  namespacedPushCall.namespacePrefix = "/std/collections/vector";

  std::unordered_map<std::string, std::string> nameMap;
  std::string helper;
  CHECK(primec::emitter::getVectorMutatorName(
      namespacedPushCall, nameMap, helper));
  CHECK(helper == "push");
}

TEST_CASE("C++ emitter helper rejects array namespaced vector constructor alias builtin") {
  primec::Expr call;
  call.kind = primec::Expr::Kind::Call;
  call.name = "/array/vector";

  std::string builtin;
  CHECK_FALSE(primec::emitter::getBuiltinCollectionName(call, builtin));
}

TEST_CASE("C++ emitter helper rejects bare vector count methods without helper metadata") {
  primec::Expr call;
  call.kind = primec::Expr::Kind::Call;
  call.isMethodCall = true;
  call.name = "count";

  primec::Expr receiver;
  receiver.kind = primec::Expr::Kind::Name;
  receiver.name = "values";
  call.args.push_back(receiver);
  call.argNames.push_back(std::nullopt);

  std::unordered_map<std::string, primec::emitter::BindingInfo> localTypes;
  primec::emitter::BindingInfo receiverInfo;
  receiverInfo.typeName = "vector";
  localTypes.emplace("values", receiverInfo);
  std::unordered_map<std::string, const primec::Definition *> defMap;
  std::unordered_map<std::string, std::string> importAliases;
  std::unordered_map<std::string, std::string> structTypeMap;
  std::unordered_map<std::string, primec::emitter::ReturnKind> returnKinds;
  std::unordered_map<std::string, std::string> returnStructs;
  std::string resolved = "/stale/path";

  CHECK_FALSE(primec::emitter::resolveMethodCallPath(
      call, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
  CHECK(resolved.empty());

  localTypes.clear();
  resolved = "/stale/path";
  CHECK_FALSE(primec::emitter::resolveMethodCallPath(
      call, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
  CHECK(resolved.empty());
}

TEST_CASE("C++ emitter helper resolves bare vector count methods when helper metadata exists") {
  primec::Expr call;
  call.kind = primec::Expr::Kind::Call;
  call.isMethodCall = true;
  call.name = "count";

  primec::Expr receiver;
  receiver.kind = primec::Expr::Kind::Name;
  receiver.name = "values";
  call.args.push_back(receiver);
  call.argNames.push_back(std::nullopt);

  std::unordered_map<std::string, primec::emitter::BindingInfo> localTypes;
  primec::emitter::BindingInfo receiverInfo;
  receiverInfo.typeName = "vector";
  localTypes.emplace("values", receiverInfo);

  primec::Definition canonicalCountDef;
  canonicalCountDef.fullPath = "/std/collections/vector/count";

  std::unordered_map<std::string, const primec::Definition *> defMap = {
      {canonicalCountDef.fullPath, &canonicalCountDef},
  };
  std::unordered_map<std::string, std::string> importAliases;
  std::unordered_map<std::string, std::string> structTypeMap;
  std::unordered_map<std::string, primec::emitter::ReturnKind> returnKinds;
  std::unordered_map<std::string, std::string> returnStructs;
  std::string resolved;

  CHECK(primec::emitter::resolveMethodCallPath(
      call, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
  CHECK(resolved == "/std/collections/vector/count");
}

TEST_CASE("C++ emitter helper rejects bare vector capacity methods without helper metadata") {
  primec::Expr call;
  call.kind = primec::Expr::Kind::Call;
  call.isMethodCall = true;
  call.name = "capacity";

  primec::Expr receiver;
  receiver.kind = primec::Expr::Kind::Name;
  receiver.name = "values";
  call.args.push_back(receiver);
  call.argNames.push_back(std::nullopt);

  std::unordered_map<std::string, primec::emitter::BindingInfo> localTypes;
  primec::emitter::BindingInfo receiverInfo;
  receiverInfo.typeName = "vector";
  localTypes.emplace("values", receiverInfo);

  std::unordered_map<std::string, const primec::Definition *> defMap;
  std::unordered_map<std::string, std::string> importAliases;
  std::unordered_map<std::string, std::string> structTypeMap;
  std::unordered_map<std::string, primec::emitter::ReturnKind> returnKinds;
  std::unordered_map<std::string, std::string> returnStructs;
  std::string resolved = "/stale/path";

  CHECK_FALSE(primec::emitter::resolveMethodCallPath(
      call, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
  CHECK(resolved.empty());
}

TEST_CASE("C++ emitter helper resolves bare vector capacity methods when helper metadata exists") {
  primec::Expr call;
  call.kind = primec::Expr::Kind::Call;
  call.isMethodCall = true;
  call.name = "capacity";

  primec::Expr receiver;
  receiver.kind = primec::Expr::Kind::Name;
  receiver.name = "values";
  call.args.push_back(receiver);
  call.argNames.push_back(std::nullopt);

  std::unordered_map<std::string, primec::emitter::BindingInfo> localTypes;
  primec::emitter::BindingInfo receiverInfo;
  receiverInfo.typeName = "vector";
  localTypes.emplace("values", receiverInfo);

  primec::Definition canonicalCapacityDef;
  canonicalCapacityDef.fullPath = "/std/collections/vector/capacity";

  std::unordered_map<std::string, const primec::Definition *> defMap = {
      {canonicalCapacityDef.fullPath, &canonicalCapacityDef},
  };
  std::unordered_map<std::string, std::string> importAliases;
  std::unordered_map<std::string, std::string> structTypeMap;
  std::unordered_map<std::string, primec::emitter::ReturnKind> returnKinds;
  std::unordered_map<std::string, std::string> returnStructs;
  std::string resolved;

  CHECK(primec::emitter::resolveMethodCallPath(
      call, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
  CHECK(resolved == "/std/collections/vector/capacity");
}

TEST_CASE("C++ emitter helper keeps explicit stdlib vector count and capacity methods canonical") {
  auto expectCanonicalMethodPath = [](const char *methodPath) {
    primec::Expr call;
    call.kind = primec::Expr::Kind::Call;
    call.isMethodCall = true;
    call.name = methodPath;

    primec::Expr receiver;
    receiver.kind = primec::Expr::Kind::Name;
    receiver.name = "values";
    call.args.push_back(receiver);
    call.argNames.push_back(std::nullopt);

    std::unordered_map<std::string, primec::emitter::BindingInfo> localTypes;
    primec::emitter::BindingInfo receiverInfo;
    receiverInfo.typeName = "vector";
    localTypes.emplace("values", receiverInfo);

    primec::Definition canonicalDef;
    canonicalDef.fullPath = methodPath;

    std::unordered_map<std::string, const primec::Definition *> defMap = {
        {canonicalDef.fullPath, &canonicalDef},
    };
    std::unordered_map<std::string, std::string> importAliases;
    std::unordered_map<std::string, std::string> structTypeMap;
    std::unordered_map<std::string, primec::emitter::ReturnKind> returnKinds;
    std::unordered_map<std::string, std::string> returnStructs;
    std::string resolved;

    CHECK(primec::emitter::resolveMethodCallPath(
        call, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
    CHECK(resolved == methodPath);
  };

  expectCanonicalMethodPath("/std/collections/vector/count");
  expectCanonicalMethodPath("/std/collections/vector/capacity");
}

TEST_CASE("C++ emitter helper prefers stdlib File flush helper when present") {
  primec::Expr call;
  call.kind = primec::Expr::Kind::Call;
  call.isMethodCall = true;
  call.name = "flush";

  primec::Expr receiver;
  receiver.kind = primec::Expr::Kind::Name;
  receiver.name = "file";
  call.args.push_back(receiver);
  call.argNames.push_back(std::nullopt);

  std::unordered_map<std::string, primec::emitter::BindingInfo> localTypes;
  primec::emitter::BindingInfo receiverInfo;
  receiverInfo.typeName = "File";
  receiverInfo.typeTemplateArg = "Write";
  localTypes.emplace("file", receiverInfo);

  std::unordered_map<std::string, std::string> importAliases;
  std::unordered_map<std::string, std::string> structTypeMap;
  std::unordered_map<std::string, primec::emitter::ReturnKind> returnKinds;
  std::unordered_map<std::string, std::string> returnStructs;
  std::string resolved;

  std::unordered_map<std::string, const primec::Definition *> defMap;
  CHECK(primec::emitter::resolveMethodCallPath(
      call, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
  CHECK(resolved == "/file/flush");

  primec::Definition fileFlushDef;
  fileFlushDef.fullPath = "/File/flush";
  defMap.emplace(fileFlushDef.fullPath, &fileFlushDef);

  CHECK(primec::emitter::resolveMethodCallPath(
      call, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
  CHECK(resolved == "/File/flush");
}

TEST_CASE("C++ emitter helper prefers stdlib File close helper when present") {
  primec::Expr call;
  call.kind = primec::Expr::Kind::Call;
  call.isMethodCall = true;
  call.name = "close";

  primec::Expr receiver;
  receiver.kind = primec::Expr::Kind::Name;
  receiver.name = "file";
  call.args.push_back(receiver);
  call.argNames.push_back(std::nullopt);

  std::unordered_map<std::string, primec::emitter::BindingInfo> localTypes;
  primec::emitter::BindingInfo receiverInfo;
  receiverInfo.typeName = "File";
  receiverInfo.typeTemplateArg = "Write";
  localTypes.emplace("file", receiverInfo);

  std::unordered_map<std::string, std::string> importAliases;
  std::unordered_map<std::string, std::string> structTypeMap;
  std::unordered_map<std::string, primec::emitter::ReturnKind> returnKinds;
  std::unordered_map<std::string, std::string> returnStructs;
  std::string resolved;

  std::unordered_map<std::string, const primec::Definition *> defMap;
  CHECK(primec::emitter::resolveMethodCallPath(
      call, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
  CHECK(resolved == "/file/close");

  primec::Definition fileCloseDef;
  fileCloseDef.fullPath = "/File/close";
  defMap.emplace(fileCloseDef.fullPath, &fileCloseDef);

  CHECK(primec::emitter::resolveMethodCallPath(
      call, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
  CHECK(resolved == "/File/close");
}

TEST_CASE("C++ emitter helper prefers stdlib File multi-value helpers when present") {
  auto makeFileMethodCall = [](const std::string &methodName, size_t valueCount) {
    primec::Expr call;
    call.kind = primec::Expr::Kind::Call;
    call.isMethodCall = true;
    call.name = methodName;

    primec::Expr receiver;
    receiver.kind = primec::Expr::Kind::Name;
    receiver.name = "file";
    call.args.push_back(receiver);
    call.argNames.push_back(std::nullopt);

    for (size_t i = 0; i < valueCount; ++i) {
      primec::Expr value;
      value.kind = primec::Expr::Kind::Literal;
      value.literalValue = static_cast<uint64_t>(i);
      call.args.push_back(value);
      call.argNames.push_back(std::nullopt);
    }
    return call;
  };

  std::unordered_map<std::string, primec::emitter::BindingInfo> localTypes;
  primec::emitter::BindingInfo receiverInfo;
  receiverInfo.typeName = "File";
  receiverInfo.typeTemplateArg = "Write";
  localTypes.emplace("file", receiverInfo);

  std::unordered_map<std::string, std::string> importAliases;
  std::unordered_map<std::string, std::string> structTypeMap;
  std::unordered_map<std::string, primec::emitter::ReturnKind> returnKinds;
  std::unordered_map<std::string, std::string> returnStructs;
  std::string resolved;

  std::unordered_map<std::string, const primec::Definition *> defMap;
  primec::Expr writeCall = makeFileMethodCall("write", 3);
  CHECK(primec::emitter::resolveMethodCallPath(
      writeCall, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
  CHECK(resolved == "/file/write");

  primec::Expr writeLineCall = makeFileMethodCall("write_line", 0);
  CHECK(primec::emitter::resolveMethodCallPath(
      writeLineCall, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
  CHECK(resolved == "/file/write_line");

  primec::Definition fileWriteDef;
  fileWriteDef.fullPath = "/File/write";
  defMap.emplace(fileWriteDef.fullPath, &fileWriteDef);
  primec::Definition fileWriteLineDef;
  fileWriteLineDef.fullPath = "/File/write_line";
  defMap.emplace(fileWriteLineDef.fullPath, &fileWriteLineDef);

  CHECK(primec::emitter::resolveMethodCallPath(
      writeCall, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
  CHECK(resolved == "/File/write");
  CHECK(primec::emitter::resolveMethodCallPath(
      writeLineCall, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
  CHECK(resolved == "/File/write_line");
}

TEST_SUITE_END();
