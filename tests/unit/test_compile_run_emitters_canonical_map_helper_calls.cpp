#include "test_compile_run_helpers.h"
#include "test_compile_run_collection_conformance_helpers.h"
#include "test_compile_run_emitters_helpers.h"

#include "primec/testing/EmitterHelpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.emitters.cpp");

TEST_CASE("rejects canonical slash-method map access without helper in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32, 2i32, 5i32)}
  return(plus(values./std/collections/map/at(1i32),
              values./std/collections/map/at_unsafe(2i32)))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_canonical_slash_method_map_access_deleted_stub_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_canonical_slash_method_map_access_deleted_stub.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  const std::string errors = readFile(errPath);
  CHECK(errors.find("unknown method: /map/at") != std::string::npos);
}

TEST_CASE("compiles and runs bare map count through canonical helper in C++ emitter" * doctest::skip(true)) {
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
      writeTemp("compile_cpp_map_unnamespaced_count_builtin_fallback_reject.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_map_unnamespaced_count_builtin_fallback_exe").string();
  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 17);
}

TEST_CASE("rejects bare map count without imported canonical helper in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(count(values))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_map_unnamespaced_count_builtin_fallback_no_canonical_reject.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_map_unnamespaced_count_builtin_fallback_no_canonical.err")
          .string();
  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/map/count") != std::string::npos);
}

TEST_CASE("rejects bare map count through compatibility alias when canonical helper is absent in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/map/count([map<i32, i32>] values) {
  return(19i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(count(values))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_map_unnamespaced_count_compatibility_alias_reject.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_map_unnamespaced_count_compatibility_alias_reject.err")
          .string();
  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/map/count") != std::string::npos);
}

TEST_CASE("compiles and runs bare map at through canonical helper in C++ emitter" * doctest::skip(true)) {
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
      writeTemp("compile_cpp_bare_map_at_with_canonical_helper.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_bare_map_at_with_canonical_helper_exe").string();
  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 17);
}

TEST_CASE("compiles and runs bare map at_unsafe through canonical helper in C++ emitter" * doctest::skip(true)) {
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
      writeTemp("compile_cpp_bare_map_at_unsafe_with_canonical_helper.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_bare_map_at_unsafe_with_canonical_helper_exe").string();
  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 17);
}

TEST_CASE("rejects bare map at call without helper in C++ emitter with unknown-target diagnostics") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(at(values, 1i32))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_bare_map_at_without_helper_reject.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_bare_map_at_without_helper_reject.err").string();
  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/map/at") != std::string::npos);
}

TEST_CASE("rejects bare map at through compatibility alias in C++ emitter with unknown-target diagnostics") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/map/at([map<i32, i32>] values, [i32] index) {
  return(17i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(at(values, 1i32))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_bare_map_at_through_compatibility_alias_reject.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_bare_map_at_through_compatibility_alias_reject.err").string();
  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/map/at") != std::string::npos);
}

TEST_CASE("rejects bare map at_unsafe call without helper in C++ emitter with unknown-target diagnostics") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(at_unsafe(values, 1i32))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_bare_map_at_unsafe_without_helper_reject.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_bare_map_at_unsafe_without_helper_reject.err")
          .string();
  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/map/at_unsafe") != std::string::npos);
}

TEST_CASE("rejects bare map at_unsafe through compatibility alias in C++ emitter with unknown-target diagnostics") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/map/at_unsafe([map<i32, i32>] values, [i32] index) {
  return(17i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(at_unsafe(values, 1i32))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_bare_map_at_unsafe_through_compatibility_alias_reject.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_bare_map_at_unsafe_through_compatibility_alias_reject.err")
          .string();
  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/map/at_unsafe") != std::string::npos);
}

TEST_CASE("compiles and runs bare vector at through imported stdlib helper in C++ emitter") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 4i32)}
  return(at(values, 1i32))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_bare_vector_at_imported.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_bare_vector_at_imported_exe").string();
  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 4);
}

TEST_CASE("rejects bare vector at without imported helper in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 4i32)}
  return(at(values, 1i32))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_bare_vector_at_import_requirement.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_bare_vector_at_import_requirement_err.txt").string();
  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/vector/at") != std::string::npos);
}

TEST_CASE("compiles and runs bare vector at_unsafe through imported stdlib helper in C++ emitter") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 4i32)}
  return(at_unsafe(values, 1i32))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_bare_vector_at_unsafe_imported.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_bare_vector_at_unsafe_imported_exe").string();
  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 4);
}

TEST_CASE("rejects bare vector at_unsafe without imported helper in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 4i32)}
  return(at_unsafe(values, 1i32))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_bare_vector_at_unsafe_import_requirement.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_bare_vector_at_unsafe_import_requirement_err.txt")
          .string();
  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/vector/at_unsafe") != std::string::npos);
}

TEST_CASE("compiles and runs map unnamespaced contains through canonical helper in C++ emitter") {
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
  const std::string srcPath =
      writeTemp("compile_cpp_map_unnamespaced_contains_prefers_canonical_helper.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_map_unnamespaced_contains_prefers_canonical_helper_exe")
          .string();
  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("rejects map unnamespaced contains through compatibility helper when canonical helper is absent in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<bool>]
/map/contains([map<i32, i32>] values, [i32] key) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(contains(values, 1i32))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_map_unnamespaced_contains_prefers_compatibility_helper.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_map_unnamespaced_contains_prefers_compatibility_helper.err")
          .string();
  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/map/contains") !=
        std::string::npos);
}

TEST_CASE("compiles and runs map unnamespaced contains preferring canonical helper over compatibility alias in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<bool>]
/std/collections/map/contains([map<i32, i32>] values, [i32] key) {
  return(false)
}

[effects(heap_alloc), return<bool>]
/map/contains([map<i32, i32>] values, [i32] key) {
  return(true)
}

[effects(heap_alloc), return<bool>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(contains(values, 1i32))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_map_unnamespaced_contains_prefers_canonical_over_alias.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_cpp_map_unnamespaced_contains_prefers_canonical_over_alias_exe")
          .string();
  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("rejects map unnamespaced contains without helper in C++ emitter with unknown-target diagnostics") {
  const std::string source = R"(
[effects(heap_alloc), return<bool>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(contains(values, 1i32))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_map_unnamespaced_contains_without_helper_reject.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_cpp_map_unnamespaced_contains_without_helper_reject_exe")
          .string();
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_map_unnamespaced_contains_without_helper_reject.err")
          .string();
  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/map/contains") != std::string::npos);
}

TEST_CASE("rejects map unnamespaced tryAt through canonical helper in C++ emitter without on_error") {
  const std::string source = R"(
[effects(heap_alloc), return<Result<i32, ContainerError>>]
/std/collections/map/tryAt([map<i32, i32>] values, [i32] key) {
  return(Result.ok(17i32))
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(try(tryAt(values, 1i32)))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_map_unnamespaced_try_at_prefers_canonical_helper.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_map_unnamespaced_try_at_prefers_canonical_helper.err")
          .string();
  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("missing on_error for ? usage") != std::string::npos);
}

TEST_CASE("rejects map unnamespaced tryAt through compatibility helper in C++ emitter with unknown-target diagnostics") {
  const std::string source = R"(
[effects(heap_alloc), return<Result<i32, ContainerError>>]
/map/tryAt([map<i32, i32>] values, [i32] key) {
  return(Result.ok(19i32))
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(try(tryAt(values, 1i32)))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_map_unnamespaced_try_at_prefers_compatibility_helper.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_map_unnamespaced_try_at_prefers_compatibility_helper.err")
          .string();
  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/map/tryAt") != std::string::npos);
}

TEST_CASE("rejects map unnamespaced tryAt preferring canonical helper over compatibility alias in C++ emitter without on_error") {
  const std::string source = R"(
[effects(heap_alloc), return<Result<i32, ContainerError>>]
/std/collections/map/tryAt([map<i32, i32>] values, [i32] key) {
  return(Result.ok(17i32))
}

[effects(heap_alloc), return<Result<i32, ContainerError>>]
/map/tryAt([map<i32, i32>] values, [i32] key) {
  return(Result.ok(19i32))
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(try(tryAt(values, 1i32)))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_map_unnamespaced_try_at_prefers_canonical_over_alias.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_map_unnamespaced_try_at_prefers_canonical_over_alias.err")
          .string();
  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("missing on_error for ? usage") != std::string::npos);
}

TEST_CASE("compiles and runs explicit map helper calls through same-path aliases in C++ emitter" * doctest::skip(true)) {
  const std::string source = R"(
import /std/collections/*

[effects(io_err)]
unexpectedDirectMapAliasTryAt([ContainerError] err) {}

[effects(heap_alloc), return<int>]
/map/count([map<i32, i32>] values) {
  return(70i32)
}

[effects(heap_alloc), return<bool>]
/map/contains([map<i32, i32>] values, [i32] key) {
  return(false)
}

[effects(heap_alloc), return<Result<i32, ContainerError>>]
/map/tryAt([map<i32, i32>] values, [i32] key) {
  return(Result.ok(80i32))
}

[effects(heap_alloc), return<int>]
/map/at([map<i32, i32>] values, [i32] key) {
  return(60i32)
}

[effects(heap_alloc), return<int>]
/map/at_unsafe([map<i32, i32>] values, [i32] key) {
  return(30i32)
}

[effects(heap_alloc), return<int> on_error<ContainerError, /unexpectedDirectMapAliasTryAt>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32, 2i32, 5i32)}
  [i32] countValue{/map/count(values)}
  [bool] containsValue{/map/contains(values, 2i32)}
  [i32] tryValue{try(/map/tryAt(values, 2i32))}
  [i32] atValue{/map/at(values, 1i32)}
  [i32] unsafeValue{/map/at_unsafe(values, 2i32)}
  [i32] containsScore{if(containsValue, then(){ 1i32 }, else(){ 3i32 })}
  return(plus(plus(countValue, tryValue),
              plus(atValue, plus(unsafeValue, containsScore))))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_direct_map_alias_helper_same_path_precedence.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_cpp_direct_map_alias_helper_same_path_precedence_exe")
          .string();
  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 243);
}

TEST_CASE("compiles and runs explicit canonical map helper calls through same-path helpers in C++ emitter" * doctest::skip(true)) {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<int>]
/Marker/tag([Marker] self) {
  return(self.value)
}

[effects(io_err)]
unexpectedCanonicalMapTryAt([i32] err) {}

[effects(heap_alloc), return<int>]
/std/collections/map/count([map<i32, i32>] values) {
  return(70i32)
}

[effects(heap_alloc), return<bool>]
/std/collections/map/contains([map<i32, i32>] values, [i32] key) {
  return(false)
}

[effects(heap_alloc), return<Result<i32, i32>>]
/std/collections/map/tryAt([map<i32, i32>] values, [i32] key) {
  return(Result.ok(80i32))
}

[effects(heap_alloc), return<int>]
/std/collections/map/at([map<i32, i32>] values, [i32] key) {
  return(60i32)
}

[effects(heap_alloc), return<Marker>]
/std/collections/map/at_unsafe([map<i32, i32>] values, [i32] key) {
  return(Marker(30i32))
}

[effects(heap_alloc), return<int> on_error<i32, /unexpectedCanonicalMapTryAt>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32, 2i32, 5i32)}
  [i32] countValue{/std/collections/map/count(values)}
  [bool] containsValue{/std/collections/map/contains(values, 2i32)}
  [i32] tryValue{try(/std/collections/map/tryAt(values, 2i32))}
  [i32] atValue{/std/collections/map/at(values, 1i32)}
  [i32] unsafeValue{/std/collections/map/at_unsafe(values, 2i32).tag()}
  [i32] containsScore{if(containsValue, then(){ 1i32 }, else(){ 3i32 })}
  return(plus(plus(countValue, tryValue),
              plus(atValue, plus(unsafeValue, containsScore))))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_direct_canonical_map_helper_same_path_precedence.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_cpp_direct_canonical_map_helper_same_path_precedence_exe")
          .string();
  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 243);
}

TEST_CASE("rejects bare map tryAt call without imported canonical helper in C++ emitter with unknown-target diagnostics") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(try(tryAt(values, 1i32)))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_bare_map_try_at_call_without_import.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_bare_map_try_at_call_without_import.err").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/map/tryAt") != std::string::npos);
}

TEST_CASE("rejects map tryAt compatibility call struct method chain canonical forwarding in C++ emitter") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/map/tryAt([map<i32, i32>] values, [i32] key) {
  return(Marker(key))
}

[return<int>]
/Marker/tag([Marker] self) {
  return(self.value)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(2i32, 7i32)}
  return(/map/tryAt(values, 2i32).tag())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_map_try_at_alias_struct_method_chain_canonical_forwarding_reject.prime", source);
  const std::string errPath = (testScratchPath("") /
                               "primec_cpp_map_try_at_alias_struct_method_chain_canonical_forwarding_reject.err")
                                  .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /map/tryAt") != std::string::npos);
}

TEST_CASE("rejects same-path direct map tryAt struct method chain through alias helper in C++ emitter") {
  const std::string source = R"(
CanonicalMarker {
  [i32] value
}

AliasMarker {
  [i32] value
}

[return<AliasMarker>]
/map/tryAt([map<i32, i32>] values, [i32] key) {
  return(AliasMarker(plus(key, 40i32)))
}

[return<CanonicalMarker>]
/std/collections/map/tryAt([map<i32, i32>] values, [i32] key) {
  return(CanonicalMarker(key))
}

[return<int>]
/CanonicalMarker/tag([CanonicalMarker] self) {
  return(self.value)
}

[return<int>]
/AliasMarker/tag([AliasMarker] self) {
  return(self.value)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(2i32, 7i32)}
  return(/map/tryAt(values, 2i32).tag())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_direct_map_try_at_struct_method_chain_alias_helper_reject.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_direct_map_try_at_struct_method_chain_alias_helper_reject.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /map/tryAt") != std::string::npos);
}

TEST_CASE("compiles and runs canonical direct map tryAt struct method chain in C++ emitter") {
  const std::string source = R"(
CanonicalMarker {
  [i32] value
}

AliasMarker {
  [i32] value
}

[return<AliasMarker>]
/map/tryAt([map<i32, i32>] values, [i32] key) {
  return(AliasMarker(plus(key, 40i32)))
}

[return<CanonicalMarker>]
/std/collections/map/tryAt([map<i32, i32>] values, [i32] key) {
  return(CanonicalMarker(key))
}

[return<int>]
/CanonicalMarker/tag([CanonicalMarker] self) {
  return(self.value)
}

[return<int>]
/AliasMarker/tag([AliasMarker] self) {
  return(self.value)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(2i32, 7i32)}
  return(/std/collections/map/tryAt(values, 2i32).tag())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_direct_canonical_map_try_at_struct_method_chain.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_cpp_direct_canonical_map_try_at_struct_method_chain_exe")
          .string();
  const std::string outPath =
      (testScratchPath("") /
       "primec_cpp_direct_canonical_map_try_at_struct_method_chain.out")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath + " 2>&1") == 2);
  CHECK(readFile(outPath).empty());
}

TEST_CASE("rejects same-path direct map at_unsafe struct method chain through alias helper in C++ emitter") {
  const std::string source = R"(
CanonicalMarker {
  [i32] value
}

AliasMarker {
  [i32] value
}

[return<AliasMarker>]
/map/at_unsafe([map<i32, i32>] values, [i32] key) {
  return(AliasMarker(plus(key, 40i32)))
}

[return<CanonicalMarker>]
/std/collections/map/at_unsafe([map<i32, i32>] values, [i32] key) {
  return(CanonicalMarker(key))
}

[return<int>]
/CanonicalMarker/tag([CanonicalMarker] self) {
  return(self.value)
}

[return<int>]
/AliasMarker/tag([AliasMarker] self) {
  return(self.value)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(2i32, 7i32)}
  return(/map/at_unsafe(values, 2i32).tag())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_direct_map_at_unsafe_struct_method_chain_alias_helper_reject.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_direct_map_at_unsafe_struct_method_chain_alias_helper_reject.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /map/at_unsafe") != std::string::npos);
}

TEST_CASE("compiles and runs canonical direct map at_unsafe struct method chain in C++ emitter") {
  const std::string source = R"(
CanonicalMarker {
  [i32] value
}

AliasMarker {
  [i32] value
}

[return<AliasMarker>]
/map/at_unsafe([map<i32, i32>] values, [i32] key) {
  return(AliasMarker(plus(key, 40i32)))
}

[return<CanonicalMarker>]
/std/collections/map/at_unsafe([map<i32, i32>] values, [i32] key) {
  return(CanonicalMarker(key))
}

[return<int>]
/CanonicalMarker/tag([CanonicalMarker] self) {
  return(self.value)
}

[return<int>]
/AliasMarker/tag([AliasMarker] self) {
  return(self.value)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(2i32, 7i32)}
  return(/std/collections/map/at_unsafe(values, 2i32).tag())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_direct_canonical_map_at_unsafe_struct_method_chain.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_cpp_direct_canonical_map_at_unsafe_struct_method_chain_exe")
          .string();
  const std::string outPath =
      (testScratchPath("") /
       "primec_cpp_direct_canonical_map_at_unsafe_struct_method_chain.out")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath + " 2>&1") == 2);
  CHECK(readFile(outPath).empty());
}

TEST_CASE("rejects map namespaced count method compatibility alias in C++ emitter") {
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
      writeTemp("compile_cpp_map_namespaced_count_method_compatibility_alias_reject.prime", source);
  const std::string errPath = (testScratchPath("") /
                               "primec_cpp_map_namespaced_count_method_compatibility_alias_reject.err")
                                  .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("Semantic error") != std::string::npos);
}

TEST_CASE("C++ emitter rejects bare map count method without imported canonical helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(values.count())
}
)";
  const std::string srcPath = writeTemp("compile_cpp_bare_map_count_method_without_import.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_bare_map_count_method_without_import.err").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/map/count") != std::string::npos);
}

TEST_CASE("C++ emitter rejects bare map contains method without imported canonical helper") {
  const std::string source = R"(
[effects(heap_alloc), return<bool>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(values.contains(1i32))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_bare_map_contains_method_without_import.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_bare_map_contains_method_without_import.err").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/map/contains") != std::string::npos);
}

TEST_CASE("compiles and runs bare map contains struct method chain through canonical helper in C++ emitter") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/map/contains([map<i32, i32>] values, [i32] key) {
  return(Marker(11i32))
}

[return<int>]
/Marker/tag([Marker] self) {
  return(self.value)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(values.contains(1i32).tag())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_bare_map_contains_struct_method_chain_canonical_helper.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_cpp_bare_map_contains_struct_method_chain_canonical_helper_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 11);
}

TEST_CASE("rejects bare map contains struct method chain through alias helper in C++ emitter") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/map/contains([map<i32, i32>] values, [i32] key) {
  return(Marker(19i32))
}

[return<int>]
/Marker/tag([Marker] self) {
  return(self.value)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(values.contains(1i32).tag())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_bare_map_contains_struct_method_chain_alias_helper_reject.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_bare_map_contains_struct_method_chain_alias_helper_reject.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/map/contains") !=
        std::string::npos);
}

TEST_CASE("C++ emitter rejects bare map tryAt method without imported canonical helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(try(values.tryAt(1i32)))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_bare_map_tryat_method_without_import.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_bare_map_tryat_method_without_import.err").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/map/tryAt") !=
        std::string::npos);
}

TEST_CASE("compiles and runs bare map tryAt struct method chain through canonical helper in C++ emitter") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/map/tryAt([map<i32, i32>] values, [i32] key) {
  return(Marker(13i32))
}

[return<int>]
/Marker/tag([Marker] self) {
  return(self.value)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(values.tryAt(1i32).tag())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_bare_map_tryat_struct_method_chain_canonical_helper.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_cpp_bare_map_tryat_struct_method_chain_canonical_helper_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 13);
}

TEST_CASE("rejects bare map tryAt struct method chain through alias helper in C++ emitter") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/map/tryAt([map<i32, i32>] values, [i32] key) {
  return(Marker(21i32))
}

[return<int>]
/Marker/tag([Marker] self) {
  return(self.value)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(values.tryAt(1i32).tag())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_bare_map_tryat_struct_method_chain_alias_helper_reject.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_bare_map_tryat_struct_method_chain_alias_helper_reject.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/map/tryAt") !=
        std::string::npos);
}

TEST_CASE("C++ emitter rejects bare map access methods without imported canonical helpers") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32, 2i32, 5i32)}
  return(plus(values.at(1i32), values.at_unsafe(2i32)))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_bare_map_access_methods_without_import.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_bare_map_access_methods_without_import.err").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/map/at") != std::string::npos);
}

TEST_CASE("rejects map namespaced at method compatibility alias in C++ emitter") {
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
  const std::string srcPath = writeTemp("compile_cpp_map_namespaced_at_method_compatibility_alias_reject.prime",
                                        source);
  const std::string errPath = (testScratchPath("") /
                               "primec_cpp_map_namespaced_at_method_compatibility_alias_reject.err")
                                  .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("Semantic error") != std::string::npos);
}

TEST_CASE("C++ emitter resolves stdlib canonical map count helper in method-call sugar") {
  const std::string source = R"(
[return<int>]
/std/collections/map/count([map<i32, i32>] values, [bool] marker) {
  return(91i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(values.count(true))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_stdlib_canonical_map_count_method_sugar.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_stdlib_canonical_map_count_method_sugar_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 91);
}

TEST_SUITE_END();
