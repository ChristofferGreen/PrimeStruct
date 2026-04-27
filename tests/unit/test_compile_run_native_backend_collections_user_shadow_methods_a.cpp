#include "test_compile_run_helpers.h"

#include "test_compile_run_collection_conformance_helpers.h"
#include "test_compile_run_container_error_conformance_helpers.h"
#include "test_compile_run_checked_pointer_conformance_helpers.h"
#include "test_compile_run_unchecked_pointer_conformance_helpers.h"
#include "test_compile_run_native_backend_collections_helpers.h"

#if PRIMESTRUCT_NATIVE_COLLECTIONS_ENABLED
TEST_SUITE_BEGIN("primestruct.compile.run.native_backend.collections");

TEST_CASE("compiles and runs native user array at method shadow") {
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

TEST_CASE("compiles and runs native user array at_unsafe call shadow") {
  const std::string source = R"(
[return<int>]
/array/at_unsafe([array<i32>] values, [i32] index) {
  return(85i32)
}

[return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32)}
  return(at_unsafe(values, 1i32))
}
)";
  const std::string srcPath = writeTemp("compile_native_user_array_at_unsafe_call_shadow.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_user_array_at_unsafe_call_shadow_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 85);
}

TEST_CASE("compiles and runs native user array at_unsafe method shadow") {
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

TEST_CASE("compiles and runs native user map at_unsafe call shadow") {
  const std::string source = R"(
[return<int>]
/map/at_unsafe([map<i32, i32>] values, [i32] key) {
  return(62i32)
}

[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(at_unsafe(values, 1i32))
}
)";
  const std::string srcPath = writeTemp("compile_native_user_map_at_unsafe_call_shadow.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_user_map_at_unsafe_call_shadow_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("compiles and runs native user map at_unsafe method shadow") {
  const std::string source = R"(
[return<int>]
/map/at_unsafe([map<i32, i32>] values, [i32] key) {
  return(64i32)
}

[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(values.at_unsafe(1i32))
}
)";
  const std::string srcPath = writeTemp("compile_native_user_map_at_unsafe_method_shadow.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_user_map_at_unsafe_method_shadow_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("compiles and runs native user map at call shadow") {
  const std::string source = R"(
[return<int>]
/map/at([map<i32, i32>] values, [i32] key) {
  return(67i32)
}

[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(at(values, 1i32))
}
)";
  const std::string srcPath = writeTemp("compile_native_user_map_at_call_shadow.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_user_map_at_call_shadow_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("compiles and runs native user map at string positional call shadow") {
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

TEST_CASE("compiles and runs native map access preferring later map receiver over string") {
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

TEST_CASE("compiles and runs native user map at_unsafe string positional call shadow") {
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

TEST_CASE("compiles and runs native user map at method shadow") {
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
  const std::string srcPath = writeTemp("compile_native_user_map_at_method_shadow.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_user_map_at_method_shadow_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("rejects native user vector at call shadow") {
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
  const std::string srcPath = writeTemp("compile_native_user_vector_at_call_shadow.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_native_user_vector_at_call_shadow.err").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/vector/at") !=
        std::string::npos);
}

TEST_CASE("compiles and runs canonical vector discard helpers with owned elements in native backend") {
  expectCanonicalVectorDiscardOwnershipConformance("native");
}

TEST_CASE("compiles and runs canonical vector indexed removal helpers with owned elements in native backend") {
  expectCanonicalVectorIndexedRemovalOwnershipConformance("native");
}

TEST_CASE("rejects native vector reserve with non-relocation-trivial elements") {
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

[struct]
Wrapper() {
  [Mover] value{Mover()}
}

[effects(heap_alloc), return<int>]
main() {
  [vector<Wrapper> mut] values{vector<Wrapper>(Wrapper())}
  reserve(values, 4i32)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_vector_reserve_non_relocation_trivial_reject.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_native_vector_reserve_non_relocation_trivial_reject_out.txt")
          .string();
  const std::string exePath =
      (testScratchPath("") / "primec_native_vector_reserve_non_relocation_trivial_reject_exe")
          .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find(
            "reserve requires relocation-trivial vector element type until container move/reallocation semantics are "
            "implemented: Wrapper") != std::string::npos);
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
  [vector<Mover>] values{vector<Mover>(Mover(), Mover())}
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
            "vector literal requires relocation-trivial vector element type until container move/reallocation "
            "semantics are implemented: Mover") != std::string::npos);
}

TEST_CASE("runs native indexed vector removals with ownership semantics") {
  expectVectorIndexedRemovalOwnershipConformance("native");
}

TEST_CASE("rejects native named vector at expression receiver precedence") {
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
  const std::string srcPath =
      writeTemp("compile_native_user_vector_at_named_receiver_precedence.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_native_user_vector_at_named_receiver_precedence.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/vector/at") !=
        std::string::npos);
}

TEST_CASE("compiles and runs native user vector at method shadow") {
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
  const std::string srcPath = writeTemp("compile_native_user_vector_at_method_shadow.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_user_vector_at_method_shadow_exe").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 69);
}

TEST_CASE("compiles and runs native user string at_unsafe call shadow") {
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
  const std::string srcPath = writeTemp("compile_native_user_string_at_unsafe_call_shadow.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_user_string_at_unsafe_call_shadow_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 71);
}

TEST_CASE("compiles and runs native user string at_unsafe method shadow") {
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
  const std::string srcPath = writeTemp("compile_native_user_string_at_unsafe_method_shadow.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_user_string_at_unsafe_method_shadow_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 72);
}

TEST_CASE("rejects native user vector at_unsafe call shadow") {
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
  const std::string srcPath = writeTemp("compile_native_user_vector_at_unsafe_call_shadow.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_native_user_vector_at_unsafe_call_shadow.err").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/vector/at_unsafe") !=
        std::string::npos);
}

TEST_CASE("compiles and runs native user vector at_unsafe method shadow") {
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
  const std::string srcPath = writeTemp("compile_native_user_vector_at_unsafe_method_shadow.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_user_vector_at_unsafe_method_shadow_exe").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 82);
}

TEST_CASE("compiles and runs native user string at call shadow") {
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
  const std::string srcPath = writeTemp("compile_native_user_string_at_call_shadow.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_user_string_at_call_shadow_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 83);
}

TEST_CASE("compiles and runs native user string at method shadow") {
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
  const std::string srcPath = writeTemp("compile_native_user_string_at_method_shadow.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_user_string_at_method_shadow_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 84);
}

TEST_SUITE_END();
#endif
