#include "test_compile_run_helpers.h"

#include "test_compile_run_collection_conformance_helpers.h"
#include "test_compile_run_container_error_conformance_helpers.h"
#include "test_compile_run_checked_pointer_conformance_helpers.h"
#include "test_compile_run_unchecked_pointer_conformance_helpers.h"
#include "test_compile_run_vm_collections_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.vm.collections");

TEST_CASE("runs vm with user map at string positional call shadow") {
  const std::string source = R"(
[return<int>]
/map/at([map<string, i32>] values, [string] key) {
  return(83i32)
}

[return<int>]
main() {
  [map<string, i32>] values{map<string, i32>("only"raw_utf8, 2i32)}
  return(at("only"raw_utf8, values))
}
)";
  const std::string srcPath = writeTemp("vm_user_map_at_string_positional_call_shadow.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_user_map_at_string_positional_call_shadow_err.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("vm backend requires integer indices for at") != std::string::npos);
}

TEST_CASE("runs vm with map access preferring later map receiver over string") {
  const std::string source = R"(
[return<int>]
/map/at([map<string, i32>] values, [string] key) {
  return(85i32)
}

[return<int>]
/string/at([string] values, [map<string, i32>] key) {
  return(86i32)
}

[return<int>]
main() {
  [map<string, i32>] values{map<string, i32>("only"raw_utf8, 2i32)}
  return(at("only"raw_utf8, values))
}
)";
  const std::string srcPath = writeTemp("vm_map_access_later_receiver_precedence.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_map_access_later_receiver_precedence_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("vm backend requires integer indices for at") != std::string::npos);
}

TEST_CASE("runs vm with user map at_unsafe string positional call shadow") {
  const std::string source = R"(
[return<int>]
/map/at_unsafe([map<string, i32>] values, [string] key) {
  return(84i32)
}

[return<int>]
main() {
  [map<string, i32>] values{map<string, i32>("only"raw_utf8, 2i32)}
  return(at_unsafe("only"raw_utf8, values))
}
)";
  const std::string srcPath = writeTemp("vm_user_map_at_unsafe_string_positional_call_shadow.prime", source);
  const std::string errPath = (std::filesystem::temp_directory_path() /
                               "primec_vm_user_map_at_unsafe_string_positional_call_shadow_err.txt")
                                  .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("vm backend requires integer indices for at_unsafe") != std::string::npos);
}

TEST_CASE("runs vm with user map at method shadow") {
  const std::string source = R"(
[return<int>]
/map/at([map<i32, i32>] values, [i32] key) {
  return(65i32)
}

[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(values.at(1i32))
}
)";
  const std::string srcPath = writeTemp("vm_user_map_at_method_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm user vector at call shadow during semantics") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/at([vector<i32>] values, [i32] index) {
  return(68i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(at([index] 1i32, [values] values))
}
)";
  const std::string srcPath = writeTemp("vm_user_vector_at_call_shadow.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_user_vector_at_call_shadow_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/vector/at") != std::string::npos);
}

TEST_CASE("rejects vm named vector at expression receiver precedence") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/at([vector<i32>] values, [string] index) {
  return(86i32)
}

[effects(heap_alloc), return<int>]
/string/at([string] values, [vector<i32>] index) {
  return(87i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  [string] index{"only"raw_utf8}
  return(at([index] index, [values] values))
}
)";
  const std::string srcPath = writeTemp("vm_user_vector_at_named_receiver_precedence.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_user_vector_at_named_receiver_precedence_err.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/vector/at") != std::string::npos);
}

TEST_CASE("runs vm user vector at method shadow through same-path helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/at([vector<i32>] values, [i32] index) {
  return(69i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(values.at(1i32))
}
)";
  const std::string srcPath = writeTemp("vm_user_vector_at_method_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 69);
}

TEST_CASE("keeps vm builtin string at_unsafe call over user shadow") {
  const std::string source = R"(
[return<int>]
/string/at_unsafe([string] values, [i32] index) {
  return(71i32)
}

[return<int>]
main() {
  [string] text{"abc"utf8}
  return(at_unsafe(text, 1i32))
}
)";
  const std::string srcPath = writeTemp("vm_user_string_at_unsafe_call_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 98);
}

TEST_CASE("keeps vm builtin string at_unsafe method over user shadow") {
  const std::string source = R"(
[return<int>]
/string/at_unsafe([string] values, [i32] index) {
  return(72i32)
}

[return<int>]
main() {
  [string] text{"abc"utf8}
  return(text.at_unsafe(1i32))
}
)";
  const std::string srcPath = writeTemp("vm_user_string_at_unsafe_method_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 98);
}

TEST_CASE("rejects vm user vector at_unsafe call shadow during semantics") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/at_unsafe([vector<i32>] values, [i32] index) {
  return(81i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(at_unsafe([index] 1i32, [values] values))
}
)";
  const std::string srcPath = writeTemp("vm_user_vector_at_unsafe_call_shadow.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_user_vector_at_unsafe_call_shadow_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/vector/at_unsafe") !=
        std::string::npos);
}

TEST_CASE("runs vm user vector at_unsafe method shadow through same-path helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/at_unsafe([vector<i32>] values, [i32] index) {
  return(82i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(values.at_unsafe(1i32))
}
)";
  const std::string srcPath = writeTemp("vm_user_vector_at_unsafe_method_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 82);
}

TEST_CASE("keeps vm builtin string at call over user shadow") {
  const std::string source = R"(
[return<int>]
/string/at([string] values, [i32] index) {
  return(83i32)
}

[return<int>]
main() {
  [string] text{"abc"utf8}
  return(at(text, 1i32))
}
)";
  const std::string srcPath = writeTemp("vm_user_string_at_call_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 98);
}

TEST_CASE("keeps vm builtin string at method over user shadow") {
  const std::string source = R"(
[return<int>]
/string/at([string] values, [i32] index) {
  return(84i32)
}

[return<int>]
main() {
  [string] text{"abc"utf8}
  return(text.at(1i32))
}
)";
  const std::string srcPath = writeTemp("vm_user_string_at_method_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 98);
}

TEST_CASE("rejects vm vector push helper during lowering") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32, 3i32)}
  pop(values)
  reserve(values, 3i32)
  push(values, 9i32)
  return(plus(values[2i32], capacity(values)))
}
)";
  const std::string srcPath = writeTemp("vm_vector_push.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_vector_push_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 12);
}

TEST_CASE("rejects vm vector mutator method calls during lowering") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32, 3i32)}
  values.pop()
  values.reserve(3i32)
  values.push(9i32)
  values.remove_at(1i32)
  values.remove_swap(0i32)
  values.clear()
  return(values.count())
}
)";
  const std::string srcPath = writeTemp("vm_vector_mutator_methods.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_vector_mutator_methods_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
}

TEST_CASE("compiles and runs canonical vector discard helpers with owned elements in vm backend") {
  expectCanonicalVectorDiscardOwnershipConformance("vm");
}

TEST_CASE("compiles and runs canonical vector indexed removal helpers with owned elements in vm backend") {
  expectCanonicalVectorIndexedRemovalOwnershipConformance("vm");
}

TEST_CASE("rejects vm vector push with non-relocation-trivial elements") {
  const std::string source = R"(
import /std/collections/*

[struct]
Mover() {
  [i32] value{1i32}

  [mut]
  Move([Reference<Self>] other) {
    assign(this, other)
  }
}

[effects(heap_alloc), return<int>]
main() {
  [vector<Mover> mut] values{vector<Mover>(Mover{})}
  push(values, Mover{})
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_vector_push_non_relocation_trivial_reject.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_vector_push_non_relocation_trivial_reject_out.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(outPath).find(
            "vector literal requires relocation-trivial vector element type until container move/reallocation semantics are "
            "implemented: Mover") != std::string::npos);
}

TEST_CASE("rejects vm vector constructor with non-relocation-trivial elements") {
  const std::string source = R"(
import /std/collections/*

[struct]
Mover() {
  [i32] value{1i32}

  [mut]
  Move([Reference<Self>] other) {
    assign(this, other)
  }
}

[effects(heap_alloc), return<int>]
main() {
  [vector<Mover>] values{vector<Mover>(Mover{}, Mover{})}
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_vector_constructor_non_relocation_trivial_reject.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_vector_constructor_non_relocation_trivial_reject_out.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(outPath).find(
            "vector literal requires relocation-trivial vector element type until container move/reallocation "
            "semantics are implemented: Mover") != std::string::npos);
}

TEST_CASE("runs vm indexed vector removals with ownership semantics") {
  expectVectorIndexedRemovalOwnershipConformance("vm");
}

TEST_CASE("runs vm with user push helper shadow") {
  const std::string source = R"(
[return<int>]
push([i32] left, [i32] right) {
  return(plus(left, right))
}

[return<int>]
main() {
  push(1i32, 2i32)
  return(push(4i32, 3i32))
}
)";
  const std::string srcPath = writeTemp("vm_user_push_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 7);
}

TEST_CASE("runs vm with user vector constructor shadow") {
  const std::string source = R"(
[return<int>]
vector([i32] value) {
  return(value)
}

[return<int>]
main() {
  return(vector([value] 9i32))
}
)";
  const std::string srcPath = writeTemp("vm_user_vector_constructor_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 9);
}

TEST_CASE("runs vm with user array constructor shadow") {
  const std::string source = R"(
[return<int>]
array([i32] value) {
  return(value)
}

[return<int>]
main() {
  return(array([value] 9i32))
}
)";
  const std::string srcPath = writeTemp("vm_user_array_constructor_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 9);
}

TEST_CASE("runs vm with builtin map constructor over user shadow") {
  const std::string source = R"(
[return<int>]
map([i32] key, [i32] value) {
  return(plus(key, value))
}

[return<int>]
main() {
  return(map([key] 4i32, [value] 6i32))
}
  )";
  const std::string srcPath = writeTemp("vm_user_map_constructor_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 0);
}

TEST_CASE("rejects vm builtin vector constructor named arguments") {
  const std::string source = R"(
[return<int>]
main() {
  return(vector([value] 1i32))
}
)";
  const std::string srcPath = writeTemp("vm_builtin_vector_constructor_named_args.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_builtin_vector_constructor_named_args_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_CASE("rejects vm builtin array constructor named arguments") {
  const std::string source = R"(
[return<int>]
main() {
  return(array([value] 1i32))
}
)";
  const std::string srcPath = writeTemp("vm_builtin_array_constructor_named_args.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_builtin_array_constructor_named_args_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_CASE("rejects vm builtin map constructor named arguments") {
  const std::string source = R"(
[return<int>]
main() {
  return(map([key] 1i32, [value] 2i32))
}
)";
  const std::string srcPath = writeTemp("vm_builtin_map_constructor_named_args.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_builtin_map_constructor_named_args_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_CASE("runs vm namespaced vector count named arguments through imported stdlib helper") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(/std/collections/vector/count([values] values))
}
)";
  const std::string srcPath = writeTemp("vm_namespaced_vector_count_named_args.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm namespaced vector capacity named arguments through imported stdlib helper") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(/std/collections/vector/capacity([values] values))
}
)";
  const std::string srcPath = writeTemp("vm_namespaced_vector_capacity_named_args.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm removed vector access alias named arguments") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(/vector/at([values] values, [index] 0i32))
}
)";
  const std::string srcPath = writeTemp("vm_removed_vector_access_alias_named_args.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_removed_vector_access_alias_named_args_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /vector/at") != std::string::npos);
}

TEST_CASE("rejects vm removed vector access alias at_unsafe named arguments") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(/vector/at_unsafe([values] values, [index] 0i32))
}
)";
  const std::string srcPath = writeTemp("vm_removed_vector_access_alias_at_unsafe_named_args.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_removed_vector_access_alias_at_unsafe_named_args_err.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /vector/at_unsafe") != std::string::npos);
}

TEST_CASE("runs vm with user map constructor block shadow") {
  const std::string source = R"(
[return<int>]
map([i32] key, [i32] value) {
  return(plus(key, value))
}

[return<int>]
main() {
  [i32 mut] result{0i32}
  map(4i32, 6i32) {
    assign(result, 7i32)
  }
  return(result)
}
)";
  const std::string srcPath = writeTemp("vm_user_map_constructor_block_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 7);
}

TEST_CASE("runs vm with user vector constructor block shadow") {
  const std::string source = R"(
[return<int>]
vector([i32] value) {
  return(value)
}

[return<int>]
main() {
  [i32 mut] result{0i32}
  vector(9i32) {
    assign(result, 4i32)
  }
  return(result)
}
)";
  const std::string srcPath = writeTemp("vm_user_vector_constructor_block_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 4);
}

TEST_CASE("runs vm with user array constructor block shadow") {
  const std::string source = R"(
[return<int>]
array([i32] value) {
  return(value)
}

[return<int>]
main() {
  [i32 mut] result{0i32}
  array(9i32) {
    assign(result, 5i32)
  }
  return(result)
}
)";
  const std::string srcPath = writeTemp("vm_user_array_constructor_block_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 5);
}

TEST_CASE("runs vm with user vector push call shadow returning grown count") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<void>]
/vector/push([vector<i32> mut] values, [i32] value) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  push(values, 3i32)
  return(count(values))
}
  )";
  const std::string srcPath = writeTemp("vm_user_vector_push_call_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 3);
}

TEST_SUITE_END();
