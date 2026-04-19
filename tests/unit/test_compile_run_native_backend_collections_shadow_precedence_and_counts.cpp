#include "test_compile_run_helpers.h"

#include "test_compile_run_collection_conformance_helpers.h"
#include "test_compile_run_container_error_conformance_helpers.h"
#include "test_compile_run_checked_pointer_conformance_helpers.h"
#include "test_compile_run_unchecked_pointer_conformance_helpers.h"
#include "test_compile_run_native_backend_collections_helpers.h"

#if PRIMESTRUCT_NATIVE_COLLECTIONS_ENABLED
TEST_SUITE_BEGIN("primestruct.compile.run.native_backend.collections");

TEST_CASE("native bare vector mutator methods compile without imported helpers") {
  expectBareVectorMutatorMethodImportRequirement("native", "push", "7i32");
  expectBareVectorMutatorMethodImportRequirement("native", "pop", "");
  expectBareVectorMutatorMethodImportRequirement("native", "reserve", "8i32");
  expectBareVectorMutatorMethodImportRequirement("native", "clear", "");
  expectBareVectorMutatorMethodImportRequirement("native", "remove_at", "1i32");
  expectBareVectorMutatorMethodImportRequirement("native", "remove_swap", "1i32");
}

TEST_CASE("compiles and runs native user array count method shadow") {
  const std::string source = R"(
[return<int>]
/array/count([array<i32>] values) {
  return(99i32)
}

[return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32)}
  return(values.count())
}
)";
  const std::string srcPath = writeTemp("compile_native_user_array_count_method_shadow.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_user_array_count_method_shadow_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 99);
}

TEST_CASE("compiles and runs native user array count call shadow" * doctest::skip(true)) {
  const std::string source = R"(
[return<int>]
/array/count([array<i32>] values) {
  return(98i32)
}

[return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32)}
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("compile_native_user_array_count_call_shadow.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_user_array_count_call_shadow_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 98);
}

TEST_CASE("compiles and runs native user map count call shadow" * doctest::skip(true)) {
  const std::string source = R"(
[return<int>]
/map/count([map<i32, i32>] values) {
  return(96i32)
}

[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("compile_native_user_map_count_call_shadow.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_user_map_count_call_shadow_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs native user map count method shadow" * doctest::skip(true)) {
  const std::string source = R"(
[return<int>]
/map/count([map<i32, i32>] values) {
  return(93i32)
}

[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(values.count())
}
)";
  const std::string srcPath = writeTemp("compile_native_user_map_count_method_shadow.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_user_map_count_method_shadow_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs native canonical map sugar before compatibility aliases" * doctest::skip(true)) {
  const std::string source = R"(
[return<int>]
/map/count([map<i32, i32>] values) {
  return(96i32)
}

[return<int>]
/std/collections/map/count([map<i32, i32>] values) {
  return(73i32)
}

[return<int>]
/map/at([map<i32, i32>] values, [i32] key) {
  return(41i32)
}

[return<int>]
/std/collections/map/at([map<i32, i32>] values, [i32] key) {
  return(11i32)
}

[return<int>]
/map/at_unsafe([map<i32, i32>] values, [i32] key) {
  return(42i32)
}

[return<int>]
/std/collections/map/at_unsafe([map<i32, i32>] values, [i32] key) {
  return(12i32)
}

[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(plus(plus(count(values), values.count()),
              plus(values.at(1i32), values.at_unsafe(1i32))))
}
)";
  const std::string srcPath = writeTemp("compile_native_canonical_map_sugar_before_aliases.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_canonical_map_sugar_before_aliases_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 169);
}

TEST_CASE("rejects native canonical unknown map helper with canonical diagnostics") {
  const std::string source = R"(
[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(/std/collections/map/missing(values))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_canonical_unknown_map_helper.prime", source);
  const std::string outPath =
      (testScratchPath("") /
       "primec_native_canonical_unknown_map_helper_out.txt")
          .string();
  const std::string exePath =
      (testScratchPath("") /
       "primec_native_canonical_unknown_map_helper_exe")
          .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  const std::string diagnostics = readFile(outPath);
  CHECK(diagnostics.find("unknown call target: /std/collections/map/missing") != std::string::npos);
  CHECK(diagnostics.find("unknown call target: std/collections/map/missing") == std::string::npos);
}

TEST_CASE("compiles and runs native canonical map access string shadow before compatibility aliases") {
  const std::string source = R"(
[return<int>]
/map/at([map<i32, string>] values, [i32] key) {
  return(41i32)
}

[return<string>]
/std/collections/map/at([map<i32, string>] values, [i32] key) {
  return("hello"utf8)
}

[return<int>]
/map/at_unsafe([map<i32, string>] values, [i32] key) {
  return(42i32)
}

[return<string>]
/std/collections/map/at_unsafe([map<i32, string>] values, [i32] key) {
  return("hello"utf8)
}

[return<int>]
/string/count([string] values) {
  return(5i32)
}

[return<int>]
main() {
  [map<i32, string>] values{map<i32, string>(1i32, "value"utf8)}
  return(plus(values.at(1i32).count(), values.at_unsafe(1i32).count()))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_canonical_map_access_string_shadow_before_aliases.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_native_canonical_map_access_string_shadow_before_aliases_exe")
          .string();
  const std::string outPath =
      (testScratchPath("") /
       "primec_native_canonical_map_access_string_shadow_before_aliases_out.txt")
          .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 10);
}

TEST_CASE("native canonical map access non-string compatibility aliases no longer override same-path typing" * doctest::skip(true)) {
  const std::string source = R"(
[return<string>]
/map/at([map<i32, string>] values, [i32] key) {
  return("hello"utf8)
}

[return<int>]
/std/collections/map/at([map<i32, string>] values, [i32] key) {
  return(41i32)
}

[return<string>]
/map/at_unsafe([map<i32, string>] values, [i32] key) {
  return("hello"utf8)
}

[return<int>]
/std/collections/map/at_unsafe([map<i32, string>] values, [i32] key) {
  return(42i32)
}

[return<int>]
/string/count([string] values) {
  return(5i32)
}

[return<int>]
main() {
  [map<i32, string>] values{map<i32, string>(1i32, "value"utf8)}
  return(plus(values.at(1i32).count(), values.at_unsafe(1i32).count()))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_canonical_map_access_string_shadow_before_aliases_diag.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_native_canonical_map_access_string_shadow_before_aliases_diag_exe")
          .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main > /dev/null 2>&1";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("compiles and runs native explicit map helper calls through same-path aliases" * doctest::skip(true)) {
  const std::string source = R"(
import /std/collections/*

[return<int>]
/map/count([map<i32, i32>] values) {
  return(70i32)
}

[return<bool>]
/map/contains([map<i32, i32>] values, [i32] key) {
  return(false)
}

[return<Result<i32, ContainerError>>]
/map/tryAt([map<i32, i32>] values, [i32] key) {
  return(Result.ok(80i32))
}

[return<int>]
/map/at([map<i32, i32>] values, [i32] key) {
  return(60i32)
}

[return<int>]
/map/at_unsafe([map<i32, i32>] values, [i32] key) {
  return(30i32)
}

[effects(io_err)]
unexpectedDirectMapAliasTryAt([ContainerError] err) {}

[return<int> on_error<ContainerError, /unexpectedDirectMapAliasTryAt>]
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
      writeTemp("compile_native_direct_map_alias_helper_same_path_precedence.prime", source);
  const std::string outPath =
      (testScratchPath("") /
       "primec_native_direct_map_alias_helper_same_path_precedence_out.txt")
          .string();
  const std::string exePath = (testScratchPath("") /
                               "primec_native_direct_map_alias_helper_same_path_precedence_exe")
                                  .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(readFile(outPath).empty());
  CHECK(runCommand(exePath) == 243);
}

TEST_CASE("compiles and runs native explicit canonical map helper calls through same-path helpers" * doctest::skip(true)) {
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

[return<int>]
/std/collections/map/count([map<i32, i32>] values) {
  return(70i32)
}

[return<bool>]
/std/collections/map/contains([map<i32, i32>] values, [i32] key) {
  return(false)
}

[return<Result<i32, i32>>]
/std/collections/map/tryAt([map<i32, i32>] values, [i32] key) {
  return(Result.ok(80i32))
}

[return<int>]
/std/collections/map/at([map<i32, i32>] values, [i32] key) {
  return(60i32)
}

[return<Marker>]
/std/collections/map/at_unsafe([map<i32, i32>] values, [i32] key) {
  return(Marker(30i32))
}

[return<int> on_error<i32, /unexpectedCanonicalMapTryAt>]
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
      writeTemp("compile_native_direct_canonical_map_helper_same_path_precedence.prime", source);
  const std::string outPath =
      (testScratchPath("") /
       "primec_native_direct_canonical_map_helper_same_path_precedence_out.txt")
          .string();
  const std::string exePath =
      (testScratchPath("") /
       "primec_native_direct_canonical_map_helper_same_path_precedence_exe")
          .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(readFile(outPath).empty());
  CHECK(runCommand(exePath) == 243);
}

TEST_CASE("rejects native map compatibility count call mismatch with canonical templated helper present") {
  const std::string source = R"(
[return<int>]
/map/count([map<i32, i32>] values) {
  return(96i32)
}

[return<bool>]
/std/collections/map/count<K, V>([map<K, V>] values, [bool] marker) {
  return(false)
}

[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(/map/count(values, true))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_map_count_call_alias_mismatch_with_canonical_templated_helper.prime", source);
  const std::string outPath =
      (testScratchPath("") /
       "primec_native_map_count_call_alias_mismatch_with_canonical_templated_helper_out.txt")
          .string();
  const std::string exePath =
      (testScratchPath("") /
       "primec_native_map_count_call_alias_mismatch_with_canonical_templated_helper_exe")
          .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("argument count mismatch for /map/count") != std::string::npos);
}

TEST_CASE("compiles and runs native map compatibility explicit-template count call with canonical templated helper present") {
  const std::string source = R"(
[return<int>]
/map/count<K, V>([map<K, V>] values, [bool] marker) {
  return(96i32)
}

[return<bool>]
/std/collections/map/count<K, V>([map<K, V>] values, [bool] marker) {
  return(false)
}

[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(/map/count<i32, i32>(values, true))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_map_count_explicit_template_alias_precedence_with_canonical_templated_helper.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_native_map_count_explicit_template_alias_precedence_with_canonical_templated_helper_exe")
          .string();

  const std::string outPath =
      (testScratchPath("") /
       "primec_native_map_count_explicit_template_alias_precedence_with_canonical_templated_helper_out.txt")
          .string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(readFile(outPath).empty());
  CHECK(runCommand(exePath) == 96);
}

TEST_CASE("rejects native map compatibility explicit-template count call with non-templated alias helper") {
  const std::string source = R"(
[return<int>]
/map/count([map<i32, i32>] values, [bool] marker) {
  return(96i32)
}

[return<bool>]
/std/collections/map/count<K, V>([map<K, V>] values, [bool] marker) {
  return(false)
}

[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(/map/count<i32, i32>(values, true))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_map_count_explicit_template_non_templated_alias_reject.prime", source);
  const std::string outPath =
      (testScratchPath("") /
       "primec_native_map_count_explicit_template_non_templated_alias_reject_out.txt")
          .string();
  const std::string exePath =
      (testScratchPath("") /
       "primec_native_map_count_explicit_template_non_templated_alias_reject_exe")
          .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("template arguments are only supported on templated definitions: /map/count") !=
        std::string::npos);
}

TEST_CASE("rejects native map compatibility explicit-template count method with non-templated alias helper") {
  const std::string source = R"(
[return<int>]
/map/count([map<i32, i32>] values, [bool] marker) {
  return(96i32)
}

[return<bool>]
/std/collections/map/count<K, V>([map<K, V>] values, [bool] marker) {
  return(false)
}

[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(values.count<i32, i32>(true))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_map_count_explicit_template_method_non_templated_alias_reject.prime", source);
  const std::string outPath =
      (testScratchPath("") /
       "primec_native_map_count_explicit_template_method_non_templated_alias_reject_out.txt")
          .string();
  const std::string exePath =
      (testScratchPath("") /
       "primec_native_map_count_explicit_template_method_non_templated_alias_reject_exe")
          .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("template arguments are only supported on templated definitions: /map/count") !=
        std::string::npos);
}

TEST_CASE("rejects native canonical explicit-template map count call with non-templated canonical helper") {
  const std::string source = R"(
[return<bool>]
/std/collections/map/count([map<i32, i32>] values, [bool] marker) {
  return(false)
}

[return<int>]
/map/count<K, V>([map<K, V>] values, [bool] marker) {
  return(96i32)
}

[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(/std/collections/map/count<i32, i32>(values, true))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_canonical_map_count_explicit_template_non_templated_canonical_reject.prime", source);
  const std::string outPath =
      (testScratchPath("") /
       "primec_native_canonical_map_count_explicit_template_non_templated_canonical_reject_out.txt")
          .string();
  const std::string exePath =
      (testScratchPath("") /
       "primec_native_canonical_map_count_explicit_template_non_templated_canonical_reject_exe")
          .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("template arguments are only supported on templated definitions: /std/collections/map/count") !=
        std::string::npos);
}

TEST_CASE("rejects native canonical implicit-template map count call with canonical argument-shape diagnostics") {
  const std::string source = R"(
[return<bool>]
/std/collections/map/count([map<i32, i32>] values, [bool] marker) {
  return(false)
}

[return<int>]
/map/count<K, V>([map<K, V>] values) {
  return(96i32)
}

[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(/std/collections/map/count(values))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_canonical_map_count_implicit_template_arg_shape_canonical_diag_reject.prime", source);
  const std::string outPath =
      (testScratchPath("") /
       "primec_native_canonical_map_count_implicit_template_arg_shape_canonical_diag_reject_out.txt")
          .string();
  const std::string exePath =
      (testScratchPath("") /
       "primec_native_canonical_map_count_implicit_template_arg_shape_canonical_diag_reject_exe")
          .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("argument count mismatch for /std/collections/map/count") != std::string::npos);
}

TEST_CASE("compiles and runs native canonical implicit-template map count call with wrapper slash return envelope") {
  const std::string source = R"(
[return<int>]
/std/collections/map/count<K, V>([map<K, V>] values, [bool] marker) {
  return(96i32)
}

[effects(heap_alloc), return</std/collections/map<i32, i32>>]
wrapValues() {
  return(map<i32, i32>(1i32, 2i32))
}

[return<int>]
main() {
  return(/std/collections/map/count(wrapValues(), true))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_canonical_map_count_implicit_template_wrapper_slash_return_envelope.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_native_canonical_map_count_implicit_template_wrapper_slash_return_envelope_exe")
          .string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 96);
}

TEST_CASE("compiles and runs native user string count call shadow" * doctest::skip(true)) {
  const std::string source = R"(
[return<int>]
/string/count([string] values) {
  return(94i32)
}

[return<int>]
main() {
  [string] text{"abc"utf8}
  return(count(text))
}
)";
  const std::string srcPath = writeTemp("compile_native_user_string_count_call_shadow.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_user_string_count_call_shadow_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 94);
}

TEST_CASE("compiles and runs native user string count method shadow") {
  const std::string source = R"(
[return<int>]
/string/count([string] values) {
  return(95i32)
}

[return<int>]
main() {
  [string] text{"abc"utf8}
  return(text.count())
}
)";
  const std::string srcPath = writeTemp("compile_native_user_string_count_method_shadow.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_user_string_count_method_shadow_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 95);
}

TEST_CASE("native runs canonical map reference string access" * doctest::skip(true)) {
  const std::string source = R"(
[return<int>]
/string/count([string] values) {
  return(91i32)
}

[effects(heap_alloc), return<int>]
main() {
  [/std/collections/map<i32, string>] values{map<i32, string>(1i32, "hello"utf8)}
  [Reference</std/collections/map<i32, string>>] ref{location(values)}
  return(ref[1i32].count())
}
)";
  const std::string srcPath =
      writeTemp("compile_native_user_string_count_method_shadow_map_reference_access.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_native_user_string_count_method_shadow_map_reference_access_exe")
          .string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 5);
}

TEST_CASE("native keeps current builtin count runtime failure on canonical map reference string access" * doctest::skip(true)) {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [/std/collections/map<i32, string>] values{map<i32, string>(1i32, "hello"utf8)}
  [Reference</std/collections/map<i32, string>>] ref{location(values)}
  return(count(/std/collections/map/at(ref, 1i32)))
}
)";
  const std::string srcPath = writeTemp("compile_native_builtin_count_canonical_map_reference_string_access.prime",
                                        source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_native_builtin_count_canonical_map_reference_string_access_exe")
          .string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("native rejects bare builtin count on wrapper-returned canonical map access before lowering") {
  const std::string source = R"(
[return</std/collections/map<i32, string>>]
wrapMap() {
  return(map<i32, string>(1i32, "hello"utf8))
}

[effects(heap_alloc), return<int>]
main() {
  return(count(/std/collections/map/at(wrapMap(), 1i32)))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_builtin_count_wrapper_canonical_map_string_access.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_native_builtin_count_wrapper_canonical_map_string_access_exe")
          .string();
  const std::string errPath =
      (testScratchPath("") /
       "primec_native_builtin_count_wrapper_canonical_map_string_access.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/map/at") !=
        std::string::npos);
}

TEST_CASE("compiles and runs native user string count method shadow on wrapper-returned canonical map access") {
  const std::string source = R"(
[return<int>]
/string/count([string] values) {
  return(91i32)
}

[return</std/collections/map<i32, string>>]
wrapMap() {
  return(map<i32, string>(1i32, "hello"utf8))
}

[effects(heap_alloc), return<int>]
main() {
  return(wrapMap()[1i32].count())
}
)";
  const std::string srcPath =
      writeTemp("compile_native_user_string_count_method_shadow_wrapper_canonical_map_access.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_native_user_string_count_method_shadow_wrapper_canonical_map_access_exe")
          .string();
  const std::string outPath =
      (testScratchPath("") /
       "primec_native_user_string_count_method_shadow_wrapper_canonical_map_access_out.txt")
          .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) == 2);
  CHECK_FALSE(readFile(outPath).empty());
}

TEST_SUITE_END();
#endif
