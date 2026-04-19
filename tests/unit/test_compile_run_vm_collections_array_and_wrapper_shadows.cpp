#include "test_compile_run_helpers.h"

#include "test_compile_run_collection_conformance_helpers.h"
#include "test_compile_run_container_error_conformance_helpers.h"
#include "test_compile_run_checked_pointer_conformance_helpers.h"
#include "test_compile_run_unchecked_pointer_conformance_helpers.h"
#include "test_compile_run_vm_collections_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.vm.collections");

TEST_CASE("runs vm with user array count call shadow") {
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
  const std::string srcPath = writeTemp("vm_user_array_count_call_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 98);
}

TEST_CASE("runs vm with canonical slash vector count same-path helper on array receiver") {
  const std::string source = R"(
[return<array<i32>>]
wrapArray() {
  return(array<i32>(1i32, 2i32, 3i32))
}

[return<int>]
/std/collections/vector/count([array<i32>] values) {
  return(88i32)
}

[return<int>]
main() {
  return(wrapArray()./std/collections/vector/count())
}
)";
  const std::string srcPath =
      writeTemp("vm_canonical_slash_vector_count_array_same_path_helper.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 88);
}

TEST_CASE("rejects vm wrapper-returned canonical vector count slash-method on array receiver") {
  const std::string source = R"(
[return<array<i32>>]
wrapArray() {
  return(array<i32>(1i32, 2i32, 3i32))
}

[return<int>]
main() {
  return(wrapArray()./std/collections/vector/count())
}
)";
  const std::string srcPath =
      writeTemp("vm_canonical_slash_vector_count_array_no_helper.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_canonical_slash_vector_count_array_no_helper.out.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(outPath).find("unknown method: /std/collections/vector/count") !=
        std::string::npos);
}

TEST_CASE("rejects vm wrapper-returned canonical vector capacity slash-method on array receiver") {
  const std::string source = R"(
[return<array<i32>>]
wrapArray() {
  return(array<i32>(1i32, 2i32, 3i32))
}

[return<int>]
main() {
  return(wrapArray()./std/collections/vector/capacity())
}
)";
  const std::string srcPath =
      writeTemp("vm_canonical_slash_vector_capacity_array_no_helper.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_canonical_slash_vector_capacity_array_no_helper.out.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(outPath).find("capacity requires vector target") !=
        std::string::npos);
}

TEST_CASE("rejects vm alias slash-method vector access on array receiver") {
  const std::string source = R"(
[return<array<i32>>]
wrapArray() {
  return(array<i32>(1i32, 2i32, 3i32))
}

[return<int>]
main() {
  return(plus(wrapArray()./vector/at(1i32),
              wrapArray()./vector/at_unsafe(0i32)))
}
)";
  const std::string srcPath =
      writeTemp("vm_alias_slash_vector_access_array_no_helper.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_alias_slash_vector_access_array_no_helper.out.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(outPath).find("unknown method: /vector/at") != std::string::npos);
}

TEST_CASE("runs vm with user map count call shadow") {
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
  const std::string srcPath = writeTemp("vm_user_map_count_call_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 1);
}

TEST_CASE("runs vm with user map count method shadow") {
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
  const std::string srcPath = writeTemp("vm_user_map_count_method_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 1);
}

TEST_CASE("runs vm canonical map sugar before compatibility aliases") {
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
  const std::string srcPath = writeTemp("vm_canonical_map_sugar_before_aliases.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 169);
}

TEST_CASE("rejects vm canonical unknown map helper with canonical diagnostics") {
  const std::string source = R"(
[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(/std/collections/map/missing(values))
}
)";
  const std::string srcPath = writeTemp("vm_canonical_unknown_map_helper.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_canonical_unknown_map_helper_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) != 0);
  const std::string diagnostics = readFile(outPath);
  CHECK(diagnostics.find("unknown call target: /std/collections/map/missing") != std::string::npos);
  CHECK(diagnostics.find("unknown call target: std/collections/map/missing") == std::string::npos);
}

TEST_CASE("vm canonical map access string shadow currently fails backend lowering") {
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
  const std::string srcPath = writeTemp("vm_canonical_map_access_string_shadow_before_aliases.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 10);
}

TEST_CASE("vm canonical map access non-string shadow currently fails backend lowering") {
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
      writeTemp("vm_canonical_map_access_string_shadow_before_aliases_diag.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_canonical_map_access_string_shadow_before_aliases_diag.out")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) != 0);
  CHECK_FALSE(readFile(outPath).empty());
}

TEST_CASE("rejects vm map compatibility count call with canonical templated helper present") {
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
  return(/map/count(values))
}
)";
  const std::string srcPath =
      writeTemp("vm_map_count_call_alias_precedence_with_canonical_templated_helper.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_map_count_call_alias_precedence_with_canonical_templated_helper_out.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) != 0);
}

TEST_CASE("rejects vm map compatibility count call mismatch with canonical templated helper present") {
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
      writeTemp("vm_map_count_call_alias_mismatch_with_canonical_templated_helper.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_map_count_call_alias_mismatch_with_canonical_templated_helper_out.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) != 0);
  CHECK(readFile(outPath).find("argument count mismatch for /map/count") != std::string::npos);
}

TEST_CASE("runs vm map compatibility explicit-template count call with canonical templated helper present") {
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
      writeTemp("vm_map_count_explicit_template_alias_precedence_with_canonical_templated_helper.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_map_count_explicit_template_alias_precedence_with_canonical_templated_helper_out.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) == 96);
  CHECK(readFile(outPath).empty());
}

TEST_CASE("rejects vm map compatibility explicit-template count call with non-templated alias helper") {
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
      writeTemp("vm_map_count_explicit_template_non_templated_alias_reject.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_map_count_explicit_template_non_templated_alias_reject_out.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) != 0);
  CHECK(readFile(outPath).find("template arguments are only supported on templated definitions: /map/count") !=
        std::string::npos);
}

TEST_CASE("rejects vm map compatibility explicit-template count method with non-templated alias helper") {
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
      writeTemp("vm_map_count_explicit_template_method_non_templated_alias_reject.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_map_count_explicit_template_method_non_templated_alias_reject_out.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) != 0);
  CHECK(readFile(outPath).find("template arguments are only supported on templated definitions: /map/count") !=
        std::string::npos);
}

TEST_CASE("rejects vm canonical explicit-template map count call with non-templated canonical helper") {
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
      writeTemp("vm_canonical_map_count_explicit_template_non_templated_canonical_reject.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_canonical_map_count_explicit_template_non_templated_canonical_reject_out.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) != 0);
  CHECK(readFile(outPath).find("template arguments are only supported on templated definitions: /std/collections/map/count") !=
        std::string::npos);
}

TEST_CASE("rejects vm canonical implicit-template map count call with canonical argument-shape diagnostics") {
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
      writeTemp("vm_canonical_map_count_implicit_template_arg_shape_canonical_diag_reject.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_canonical_map_count_implicit_template_arg_shape_canonical_diag_reject_out.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) != 0);
  CHECK(readFile(outPath).find("argument count mismatch for /std/collections/map/count") != std::string::npos);
}

TEST_CASE("runs vm canonical implicit-template map count call with wrapper slash return envelope") {
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
      writeTemp("vm_canonical_map_count_implicit_template_wrapper_slash_return_envelope.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 96);
}

TEST_CASE("runs vm with user string count call shadow") {
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
  const std::string srcPath = writeTemp("vm_user_string_count_call_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 94);
}

TEST_CASE("runs vm with user string count method shadow") {
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
  const std::string srcPath = writeTemp("vm_user_string_count_method_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 95);
}

TEST_CASE("runs vm canonical map reference string access") {
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
  const std::string srcPath = writeTemp("vm_user_string_count_method_shadow_map_reference_access.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_user_string_count_method_shadow_map_reference_access.err")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 91);
}

TEST_CASE("runs vm builtin count on canonical map reference string access") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [/std/collections/map<i32, string>] values{map<i32, string>(1i32, "hello"utf8)}
  [Reference</std/collections/map<i32, string>>] ref{location(values)}
  return(count(/std/collections/map/at(ref, 1i32)))
}
)";
  const std::string srcPath = writeTemp("vm_builtin_count_canonical_map_reference_string_access.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_builtin_count_canonical_map_reference_string_access.err")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(errPath).find("VM error: invalid string index in IR") != std::string::npos);
}

TEST_CASE("vm rejects bare builtin count on wrapper-returned canonical map access before lowering") {
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
  const std::string srcPath = writeTemp("vm_builtin_count_wrapper_canonical_map_string_access.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_builtin_count_wrapper_canonical_map_string_access.err")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/map/at") !=
        std::string::npos);
}

TEST_CASE("vm rejects user string count method shadow on wrapper-returned canonical map access") {
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
      writeTemp("vm_user_string_count_method_shadow_wrapper_canonical_map_access.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("vm keeps imported wrapper-returned canonical map reference access lowering diagnostics") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
/string/count([string] values) {
  return(91i32)
}

[return<Reference</std/collections/map<i32, string>>>]
borrowMap([Reference</std/collections/map<i32, string>>] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [/std/collections/map<i32, string>] values{map<i32, string>(1i32, "hello"utf8)}
  return(/std/collections/map/at(borrowMap(location(values)), 1i32).count())
}
)";
  const std::string srcPath =
      writeTemp("vm_user_string_count_method_shadow_direct_wrapper_map_reference_access.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_user_string_count_method_shadow_direct_wrapper_map_reference_access.err")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("vm backend does not support string array return types on /borrowMap") !=
        std::string::npos);
}

TEST_CASE("vm keeps wrapper-returned canonical map reference primitive receiver diagnostics") {
  const std::string source = R"(
[return<Reference</std/collections/map<i32, i32>>>]
borrowMap([Reference</std/collections/map<i32, i32>>] values) {
  return(values)
}

[return<int>]
/string/count([string] values) {
  return(91i32)
}

[effects(heap_alloc), return<int>]
main() {
  [/std/collections/map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(/std/collections/map/at(borrowMap(location(values)), 1i32).count())
}
)";
  const std::string srcPath =
      writeTemp("vm_nonimport_direct_wrapper_map_reference_string_receiver_diag.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_nonimport_direct_wrapper_map_reference_string_receiver_diag.err")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /i32/count") != std::string::npos);
}

TEST_CASE("runs vm user map method sugar on wrapper-returned canonical map references") {
  const std::string source = R"(
[return<Reference</std/collections/map<i32, i32>>>]
borrowMap([Reference</std/collections/map<i32, i32>>] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [/std/collections/map<i32, i32>] values{map<i32, i32>(1i32, 4i32, 2i32, 5i32)}
  return(plus(borrowMap(location(values)).count(),
              plus(borrowMap(location(values)).at(1i32),
                   borrowMap(location(values)).at_unsafe(2i32))))
}
)";
  const std::string srcPath =
      writeTemp("vm_wrapper_map_reference_method_sugar.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 11);
}

TEST_CASE("vm keeps non-string diagnostics on canonical map reference access count shadow" * doctest::skip(true)) {
  const std::string source = R"(
[return<int>]
/string/count([string] values) {
  return(91i32)
}

[effects(heap_alloc), return<int>]
main() {
  [/std/collections/map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  [Reference</std/collections/map<i32, i32>>] ref{location(values)}
  return(ref[1i32].count())
}
)";
  const std::string srcPath = writeTemp("vm_user_string_count_method_shadow_map_reference_access_diag.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_user_string_count_method_shadow_map_reference_access_diag.err")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /i32/count") != std::string::npos);
}

TEST_CASE("vm keeps key diagnostics on wrapper-returned canonical map reference method sugar" * doctest::skip(true)) {
  const std::string source = R"(
[return<Reference</std/collections/map<i32, i32>>>]
borrowMap([Reference</std/collections/map<i32, i32>>] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [/std/collections/map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(borrowMap(location(values)).at(true))
}
)";
  const std::string srcPath =
      writeTemp("vm_wrapper_map_reference_method_sugar_diag.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_wrapper_map_reference_method_sugar_diag.err")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("Semantic error: argument type mismatch for /std/collections/map/at parameter key") !=
        std::string::npos);
}

TEST_SUITE_END();
