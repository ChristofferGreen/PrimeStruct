#include "../test_compile_run_helpers.h"

#include "../test_compile_run_collection_conformance_helpers.h"
#include "../test_compile_run_container_error_conformance_helpers.h"
#include "../test_compile_run_checked_pointer_conformance_helpers.h"
#include "../test_compile_run_unchecked_pointer_conformance_helpers.h"
#include "test_compile_run_native_backend_collections_helpers.h"

#if PRIMESTRUCT_NATIVE_COLLECTIONS_ENABLED
TEST_SUITE_BEGIN("primestruct.compile.run.native_backend.collections");

TEST_CASE("native user array at method shadow") {
  const std::string source = R"(
[return<int>]
/array/at([array<i32>] values, [i32] index) {
  return(63i32)
}

[return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32)}
  return(values.at(1i32))
}
)";
  const std::string srcPath = writeTemp("compile_native_user_array_at_method_shadow.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_user_array_at_method_shadow_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 63);
}

TEST_CASE("native user array at_unsafe method shadow") {
  const std::string source = R"(
[return<int>]
/array/at_unsafe([array<i32>] values, [i32] index) {
  return(86i32)
}

[return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32)}
  return(values.at_unsafe(1i32))
}
)";
  const std::string srcPath = writeTemp("compile_native_user_array_at_unsafe_method_shadow.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_user_array_at_unsafe_method_shadow_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 86);
}

TEST_CASE("native user map at string positional call shadow") {
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
  const std::string srcPath = writeTemp("compile_native_user_map_at_string_positional_call_shadow.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_user_map_at_string_positional_call_shadow_exe")
          .string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("native map access preferring later map receiver over string") {
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
  const std::string srcPath = writeTemp("compile_native_map_access_later_receiver_precedence.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_map_access_later_receiver_precedence_exe")
          .string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("native user map at_unsafe string positional call shadow") {
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
  const std::string srcPath =
      writeTemp("compile_native_user_map_at_unsafe_string_positional_call_shadow.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_user_map_at_unsafe_string_positional_call_shadow_exe")
          .string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("canonical vector discard helpers with owned elements in native backend") {
  expectCanonicalVectorDiscardOwnershipConformance("native");
}

TEST_CASE("canonical vector indexed removal helpers with owned elements in native backend") {
  expectCanonicalVectorIndexedRemovalOwnershipConformance("native");
}

TEST_CASE("rejects native vector constructor with non-relocation-trivial elements") {
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
  const std::string srcPath =
      writeTemp("compile_native_vector_constructor_non_relocation_trivial_reject.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_native_vector_constructor_non_relocation_trivial_reject_out.txt")
          .string();
  const std::string exePath =
      (testScratchPath("") / "primec_native_vector_constructor_non_relocation_trivial_reject_exe")
          .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find(
            "collection literal requires relocation-trivial collection element type until container move/reallocation "
            "semantics are implemented: Mover") != std::string::npos);
}

TEST_CASE("runs native indexed vector removals with ownership semantics") {
  expectVectorIndexedRemovalOwnershipConformance("native");
}

TEST_SUITE_END();
#endif
