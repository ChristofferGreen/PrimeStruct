#include "test_compile_run_helpers.h"

#include "test_compile_run_collection_conformance_helpers.h"
#include "test_compile_run_container_error_conformance_helpers.h"
#include "test_compile_run_checked_pointer_conformance_helpers.h"
#include "test_compile_run_unchecked_pointer_conformance_helpers.h"
#include "test_compile_run_native_backend_collections_helpers.h"

#if PRIMESTRUCT_NATIVE_COLLECTIONS_ENABLED
TEST_SUITE_BEGIN("primestruct.compile.run.native_backend.collections");

TEST_CASE("compiles native stdlib namespaced map helpers on canonical map references") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [/std/collections/map<i32, i32>] values{map<i32, i32>(1i32, 4i32, 2i32, 5i32)}
  [Reference</std/collections/map<i32, i32>>] ref{location(values)}
  [i32] c{/std/collections/map/count(ref)}
  [i32] first{/std/collections/map/at(ref, 1i32)}
  [i32] second{/std/collections/map/at_unsafe(ref, 2i32)}
  return(plus(c, plus(first, second)))
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_map_reference_helpers.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_native_stdlib_map_reference_helpers_out.txt").string();
  const std::string exePath =
      (testScratchPath("") / "primec_native_stdlib_map_reference_helpers_exe").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) == 0);
}

TEST_CASE("compiles and runs native canonical map method with slash return type receiver") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/map/count([map<i32, i32>] values) {
  return(73i32)
}

[effects(heap_alloc), return</std/collections/map<i32, i32>>]
makeValues() {
  return(map<i32, i32>(1i32, 4i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(makeValues().count())
}
)";
  const std::string srcPath =
      writeTemp("compile_native_canonical_map_method_slash_return_type_receiver.prime", source);
  const std::string outPath =
      (testScratchPath("") /
       "primec_native_canonical_map_method_slash_return_type_receiver_out.txt")
          .string();
  const std::string exePath =
      (testScratchPath("") /
       "primec_native_canonical_map_method_slash_return_type_receiver_exe")
          .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 73);
}

TEST_CASE("compiles and runs native canonical map access helpers on wrapper slash return receiver") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/map/at([map<i32, i32>] values, [i32] key) {
  return(41i32)
}

[effects(heap_alloc), return<int>]
/std/collections/map/at_unsafe([map<i32, i32>] values, [i32] key) {
  return(42i32)
}

[effects(heap_alloc), return</std/collections/map<i32, i32>>]
wrapValues() {
  return(map<i32, i32>(1i32, 4i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(plus(plus(at(wrapValues(), 1i32), wrapValues().at(1i32)),
              plus(at_unsafe(wrapValues(), 1i32), wrapValues().at_unsafe(1i32))))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_canonical_map_access_helpers_wrapper_slash_return_receiver.prime", source);
  const std::string outPath = (testScratchPath("") /
                               "primec_native_canonical_map_access_helpers_wrapper_slash_return_receiver_out.txt")
                                  .string();
  const std::string exePath = (testScratchPath("") /
                               "primec_native_canonical_map_access_helpers_wrapper_slash_return_receiver_exe")
                                  .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(readFile(outPath).empty());
  CHECK(runCommand(exePath) == 166);
}

TEST_CASE("rejects native canonical map access helper key mismatch on wrapper slash return receiver") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return</std/collections/map<i32, i32>>]
wrapValues() {
  return(map<i32, i32>(1i32, 4i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(wrapValues().at(true))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_canonical_map_access_helper_key_mismatch_wrapper_slash_return_receiver.prime", source);
  const std::string outPath =
      (testScratchPath("") /
       "primec_native_canonical_map_access_helper_key_mismatch_wrapper_slash_return_receiver_out.txt")
          .string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("Semantic error: argument type mismatch for /std/collections/map/at parameter key") !=
        std::string::npos);
}

TEST_CASE("compiles and runs native explicit canonical map typed bindings with builtin helpers") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [/std/collections/map<i32, i32>] values{map<i32, i32>(1i32, 4i32, 2i32, 5i32)}
  return(plus(plus(count(values), at(values, 1i32)), at_unsafe(values, 2i32)))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_explicit_canonical_map_typed_binding_builtin_helpers.prime", source);
  const std::string outPath = (testScratchPath("") /
                               "primec_native_explicit_canonical_map_typed_binding_builtin_helpers_out.txt")
                                  .string();
  const std::string exePath = (testScratchPath("") /
                               "primec_native_explicit_canonical_map_typed_binding_builtin_helpers_exe")
                                  .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(readFile(outPath).empty());
  CHECK(runCommand(exePath) == 11);
}

TEST_CASE("rejects native explicit canonical map typed binding key mismatch") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [/std/collections/map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(at(values, true))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_explicit_canonical_map_typed_binding_builtin_helpers_diag.prime", source);
  const std::string outPath =
      (testScratchPath("") /
       "primec_native_explicit_canonical_map_typed_binding_builtin_helpers_diag_out.txt")
          .string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("Semantic error: at requires map key type i32") != std::string::npos);
}

TEST_CASE("rejects native stdlib map constructor alias fallback without import") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/map/map<T, U>([T] key, [U] value) {
  return(77i32)
}

[effects(heap_alloc), return<int>]
main() {
  return(/std/collections/map/map<i32, i32>(1i32, 2i32))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_stdlib_namespaced_map_constructor_alias_fallback.prime", source);
  const std::string outPath = (testScratchPath("") /
                               "primec_native_stdlib_namespaced_map_constructor_alias_fallback_out.txt")
                                  .string();
  const std::string exePath = (testScratchPath("") /
                               "primec_native_stdlib_namespaced_map_constructor_alias_fallback_exe")
                                  .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("unknown call target: /std/collections/map/map") != std::string::npos);
}

TEST_CASE("compiles and runs native stdlib map at alias fallback without import") {
  const std::string source = R"(
[effects(heap_alloc), return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 4i32))
}

[effects(heap_alloc), return<int>]
/map/at([map<i32, i32>] values, [i32] index) {
  return(66i32)
}

[effects(heap_alloc), return<int>]
main() {
  return(/std/collections/map/at(wrapMap(), 1i32))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_stdlib_namespaced_map_at_alias_fallback.prime", source);
  const std::string exePath = (testScratchPath("") /
                               "primec_native_stdlib_namespaced_map_at_alias_fallback_exe")
                                  .string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 4);
}

TEST_CASE("compiles and runs native stdlib map at unsafe alias fallback without import") {
  const std::string source = R"(
[effects(heap_alloc), return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 4i32))
}

[effects(heap_alloc), return<int>]
/map/at_unsafe([map<i32, i32>] values, [i32] index) {
  return(67i32)
}

[effects(heap_alloc), return<int>]
main() {
  return(/std/collections/map/at_unsafe(wrapMap(), 1i32))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_stdlib_namespaced_map_at_unsafe_alias_fallback.prime", source);
  const std::string exePath = (testScratchPath("") /
                               "primec_native_stdlib_namespaced_map_at_unsafe_alias_fallback_exe")
                                  .string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 4);
}

TEST_CASE("compiles native bare map count through canonical helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/map/count([map<i32, i32>] values) {
  return(17i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(count(values))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_map_unnamespaced_count_builtin_fallback_reject.prime", source);
  const std::string outPath = (testScratchPath("") /
                               "primec_native_map_unnamespaced_count_builtin_fallback_reject_out.txt")
                                  .string();
  const std::string exePath = (testScratchPath("") /
                               "primec_native_map_unnamespaced_count_builtin_fallback_reject_exe")
                                  .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) == 0);
}

TEST_CASE("rejects native bare map count without imported canonical helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(count(values))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_map_unnamespaced_count_builtin_fallback_no_canonical_reject.prime", source);
  const std::string outPath = (testScratchPath("") /
                               "primec_native_map_unnamespaced_count_builtin_fallback_no_canonical_reject_out.txt")
                                  .string();
  const std::string exePath = (testScratchPath("") /
                               "primec_native_map_unnamespaced_count_builtin_fallback_no_canonical_reject_exe")
                                  .string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(outPath).find("unknown call target: /std/collections/map/count") != std::string::npos);
}

TEST_CASE("compiles native bare map at through canonical helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/map/at([map<i32, i32>] values, [i32] index) {
  return(17i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(at(values, 1i32))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_bare_map_at_with_canonical_helper.prime", source);
  const std::string outPath = (testScratchPath("") /
                               "primec_native_bare_map_at_with_canonical_helper_out.txt")
                                  .string();
  const std::string exePath = (testScratchPath("") /
                               "primec_native_bare_map_at_with_canonical_helper_exe")
                                  .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) == 0);
}

TEST_CASE("compiles native bare map at_unsafe through canonical helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/map/at_unsafe([map<i32, i32>] values, [i32] index) {
  return(17i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(at_unsafe(values, 1i32))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_bare_map_at_unsafe_with_canonical_helper.prime", source);
  const std::string outPath = (testScratchPath("") /
                               "primec_native_bare_map_at_unsafe_with_canonical_helper_out.txt")
                                  .string();
  const std::string exePath = (testScratchPath("") /
                               "primec_native_bare_map_at_unsafe_with_canonical_helper_exe")
                                  .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) == 0);
}

TEST_CASE("rejects native bare map at call without helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(at(values, 1i32))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_bare_map_at_without_helper_reject.prime", source);
  const std::string outPath = (testScratchPath("") /
                               "primec_native_bare_map_at_without_helper_reject_out.txt")
                                  .string();
  const std::string exePath = (testScratchPath("") /
                               "primec_native_bare_map_at_without_helper_reject_exe")
                                  .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("unknown call target: /std/collections/map/at") != std::string::npos);
}

TEST_CASE("rejects native bare map at_unsafe call without helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(at_unsafe(values, 1i32))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_bare_map_at_unsafe_without_helper_reject.prime", source);
  const std::string outPath =
      (testScratchPath("") /
       "primec_native_bare_map_at_unsafe_without_helper_reject_out.txt")
          .string();
  const std::string exePath =
      (testScratchPath("") /
       "primec_native_bare_map_at_unsafe_without_helper_reject_exe")
          .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("unknown call target: /std/collections/map/at_unsafe") != std::string::npos);
}

TEST_CASE("rejects native map namespaced count method compatibility alias") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/map/count([map<i32, i32>] values) {
  return(17i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(values./map/count())
}
)";
  const std::string srcPath =
      writeTemp("compile_native_map_namespaced_count_method_compatibility_alias_reject.prime", source);
  const std::string outPath = (testScratchPath("") /
                               "primec_native_map_namespaced_count_method_compatibility_alias_reject_out.txt")
                                  .string();
  const std::string exePath = (testScratchPath("") /
                               "primec_native_map_namespaced_count_method_compatibility_alias_reject_exe")
                                  .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("Semantic error") != std::string::npos);
}

TEST_CASE("rejects native bare map count method without imported canonical helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(values.count())
}
)";
  const std::string srcPath = writeTemp("compile_native_bare_map_count_method_without_import.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_native_bare_map_count_method_without_import_out.txt")
          .string();
  const std::string exePath =
      (testScratchPath("") / "primec_native_bare_map_count_method_without_import_exe").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(outPath).find("unknown call target: /std/collections/map/count") != std::string::npos);
}

TEST_CASE("compiles and runs native bare map contains through canonical helper") {
  const std::string source = R"(
[effects(heap_alloc), return<bool>]
/std/collections/map/contains([map<i32, i32>] values, [i32] key) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(contains(values, 1i32))
}
)";
  const std::string srcPath = writeTemp("compile_native_bare_map_contains_call_with_canonical_helper.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_native_bare_map_contains_call_with_canonical_helper_out.txt")
          .string();
  const std::string exePath =
      (testScratchPath("") / "primec_native_bare_map_contains_call_with_canonical_helper_exe")
          .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("rejects native bare map contains call without imported canonical helper") {
  const std::string source = R"(
[effects(heap_alloc), return<bool>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(contains(values, 1i32))
}
)";
  const std::string srcPath = writeTemp("compile_native_bare_map_contains_call_without_import.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_native_bare_map_contains_call_without_import_out.txt")
          .string();
  const std::string exePath =
      (testScratchPath("") / "primec_native_bare_map_contains_call_without_import_exe")
          .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("unknown call target: /std/collections/map/contains") != std::string::npos);
}

TEST_CASE("rejects native bare map contains method without imported canonical helper") {
  const std::string source = R"(
[effects(heap_alloc), return<bool>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(values.contains(1i32))
}
)";
  const std::string srcPath = writeTemp("compile_native_bare_map_contains_method_without_import.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_native_bare_map_contains_method_without_import_out.txt")
          .string();
  const std::string exePath =
      (testScratchPath("") / "primec_native_bare_map_contains_method_without_import_exe")
          .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("unknown call target: /std/collections/map/contains") != std::string::npos);
}

TEST_CASE("rejects native bare map tryAt method without imported canonical helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(try(values.tryAt(1i32)))
}
)";
  const std::string srcPath = writeTemp("compile_native_bare_map_tryat_method_without_import.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_native_bare_map_tryat_method_without_import_out.txt")
          .string();
  const std::string exePath =
      (testScratchPath("") / "primec_native_bare_map_tryat_method_without_import_exe")
          .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("unknown call target: /std/collections/map/tryAt") != std::string::npos);
}

TEST_CASE("rejects native bare map access methods without imported canonical helpers") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32, 2i32, 5i32)}
  return(plus(values.at(1i32), values.at_unsafe(2i32)))
}
)";
  const std::string srcPath = writeTemp("compile_native_bare_map_access_methods_without_import.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_native_bare_map_access_methods_without_import_out.txt")
          .string();
  const std::string exePath =
      (testScratchPath("") / "primec_native_bare_map_access_methods_without_import_exe")
          .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("unknown call target: /std/collections/map/at") != std::string::npos);
}

TEST_CASE("rejects native map namespaced at method compatibility alias") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/map/at([map<i32, i32>] values, [i32] index) {
  return(17i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(values./map/at(1i32))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_map_namespaced_at_method_compatibility_alias_reject.prime", source);
  const std::string outPath = (testScratchPath("") /
                               "primec_native_map_namespaced_at_method_compatibility_alias_reject_out.txt")
                                  .string();
  const std::string exePath = (testScratchPath("") /
                               "primec_native_map_namespaced_at_method_compatibility_alias_reject_exe")
                                  .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("Semantic error") != std::string::npos);
}

TEST_SUITE_END();
#endif
