#include "test_compile_run_helpers.h"
#include "test_compile_run_collection_conformance_helpers.h"
#include "test_compile_run_emitters_helpers.h"

#include "primec/testing/EmitterHelpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.emitters.cpp");

TEST_CASE("C++ emitter rejects direct-call string vector access helpers before lowering") {
  const std::string source = R"(
[return<string>]
wrapText() {
  return("abc"raw_utf8)
}

[return<int>]
main() {
  return(plus(/std/collections/vector/at(wrapText(), 1i32),
              /vector/at_unsafe(wrapText(), 0i32)))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_direct_string_vector_access_no_helper.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_direct_string_vector_access_no_helper_cpp.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=cpp " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/vector/at") != std::string::npos);
}

TEST_CASE("rejects direct-call string vector access builtin fallback in C++ emitter") {
  const std::string source = R"(
[return<string>]
wrapText() {
  return("abc"raw_utf8)
}

[return<int>]
main() {
  return(plus(/std/collections/vector/at(wrapText(), 1i32),
              /vector/at_unsafe(wrapText(), 0i32)))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_direct_string_vector_access_no_helper_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_direct_string_vector_access_no_helper_exe.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/vector/at") != std::string::npos);
}

TEST_CASE("rejects slash-method wrapper string access method chain compatibility fallback in C++ emitter") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  tag([i32] value) {
    return(plus(value, 1i32))
  }
}

[return<string>]
wrapText() {
  return("abc"raw_utf8)
}

[return<int>]
main() {
  return(plus(wrapText()./std/collections/vector/at(1i32).tag(),
              wrapText()./vector/at_unsafe(0i32).tag()))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_slash_method_wrapper_string_access_method_chain_compatibility_fallback.prime",
                source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_slash_method_wrapper_string_access_method_chain_compatibility_fallback_err.txt")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /std/collections/vector/at") != std::string::npos);
}

TEST_CASE("rejects slash-method wrapper string access method chain i32 diagnostics in C++ emitter") {
  const std::string source = R"(
[return<string>]
wrapText() {
  return("abc"raw_utf8)
}

[return<int>]
main() {
  return(wrapText()./std/collections/vector/at(1i32).missing_tag())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_slash_method_wrapper_string_access_method_chain_diag.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_slash_method_wrapper_string_access_method_chain_diag.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /i32/missing_tag") != std::string::npos);
}

TEST_CASE("C++ emitter rejects slash-method string vector access helpers before lowering") {
  const std::string source = R"(
[return<string>]
wrapText() {
  return("abc"raw_utf8)
}

[return<int>]
main() {
  return(plus(wrapText()./std/collections/vector/at(1i32),
              wrapText()./vector/at_unsafe(0i32)))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_slash_method_string_vector_access_no_helper.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_slash_method_string_vector_access_no_helper_cpp.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=cpp " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /std/collections/vector/at") != std::string::npos);
}

TEST_CASE("rejects slash-method string vector access builtin fallback in C++ emitter") {
  const std::string source = R"(
[return<string>]
wrapText() {
  return("abc"raw_utf8)
}

[return<int>]
main() {
  return(plus(wrapText()./std/collections/vector/at(1i32),
              wrapText()./vector/at_unsafe(0i32)))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_slash_method_string_vector_access_no_helper_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_slash_method_string_vector_access_no_helper_exe.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /std/collections/vector/at") != std::string::npos);
}

TEST_CASE("rejects alias slash-method vector access on array receiver in C++ emitter") {
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
      writeTemp("compile_cpp_alias_slash_vector_access_array_no_helper_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_alias_slash_vector_access_array_no_helper_exe.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /vector/at") != std::string::npos);
}

TEST_CASE("rejects vector alias access struct method chain without same-path helper in C++ emitter") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return(Marker(index))
}

[return<int>]
/Marker/tag([Marker] self) {
  return(self.value)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(/vector/at(values, 2i32).tag(),
              /vector/at(values, 1i32).tag()))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_vector_alias_access_struct_method_chain_canonical_forwarding.prime", source);
  const std::string errPath = (testScratchPath("") /
                               "primec_cpp_vector_alias_access_struct_method_chain_canonical_forwarding.err")
                                  .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /i32/tag") != std::string::npos);
}

TEST_CASE("rejects vector alias access struct method chain canonical receiver diagnostics in C++ emitter") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return(Marker(index))
}

[return<int>]
/i32/tag([i32] self, [bool] marker) {
  return(self)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(/vector/at(values, 2i32).tag(1i32))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_vector_alias_access_struct_method_chain_canonical_diagnostic.prime", source);
  const std::string errPath = (testScratchPath("") /
                               "primec_cpp_vector_alias_access_struct_method_chain_canonical_diagnostic.err")
                                  .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /Marker/tag") != std::string::npos);
}

TEST_CASE("rejects vector alias access field expression without same-path helper in C++ emitter") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return(Marker(index))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(/vector/at(values, 2i32).value)
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_vector_alias_access_field_expression_struct_receiver_forwarding.prime", source);
  const std::string errPath = (testScratchPath("") /
                               "primec_cpp_vector_alias_access_field_expression_struct_receiver_forwarding.err")
                                  .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /vector/at") != std::string::npos);
}

TEST_CASE("keeps canonical vector access call struct method chain forwarding in C++ emitter") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return(Marker(index))
}

[return<int>]
/Marker/tag([Marker] self) {
  return(self.value)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(/std/collections/vector/at(values, 2i32).tag())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_canonical_vector_access_struct_method_chain_forwarding.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_cpp_canonical_vector_access_struct_method_chain_forwarding_exe")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(quoteShellArg(exePath)) == 2);
}

TEST_CASE("C++ emitter keeps canonical vector unsafe access field expression forwarding") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/vector/at_unsafe([vector<i32>] values, [i32] index) {
  return(Marker(index))
}

[return<auto>]
project([vector<i32>] values) {
  return(/std/collections/vector/at_unsafe(values, 2i32).value)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(project(values))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_canonical_vector_access_unsafe_field_expression_forwarding.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_cpp_canonical_vector_access_unsafe_field_expression_forwarding_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("keeps canonical direct-call map access struct method chain forwarding in C++ emitter") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/map/at([map<i32, i32>] values, [i32] key) {
  return(Marker(key))
}

[return<int>]
/Marker/tag([Marker] self) {
  return(self.value)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(2i32, 7i32)}
  return(/std/collections/map/at(values, 2i32).tag())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_canonical_direct_map_access_struct_method_chain_forwarding.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_cpp_canonical_direct_map_access_struct_method_chain_forwarding_exe")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(quoteShellArg(exePath)) == 2);
}

TEST_CASE("keeps canonical direct-call map unsafe struct method chain forwarding in C++ emitter") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/map/at_unsafe([map<i32, i32>] values, [i32] key) {
  return(Marker(key))
}

[return<int>]
/Marker/tag([Marker] self) {
  return(self.value)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(2i32, 7i32)}
  return(/std/collections/map/at_unsafe(values, 2i32).tag())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_canonical_direct_map_unsafe_struct_method_chain_forwarding.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_cpp_canonical_direct_map_unsafe_struct_method_chain_forwarding_exe")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(quoteShellArg(exePath)) == 2);
}

TEST_CASE("keeps canonical direct-call map access primitive diagnostics in C++ emitter") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/map/at([map<i32, i32>] values, [i32] key) {
  return(Marker(plus(key, 40i32)))
}

[return<i32>]
/std/collections/map/at([map<i32, i32>] values, [i32] key) {
  return(key)
}

[return<int>]
/Marker/tag([Marker] self) {
  return(self.value)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(2i32, 7i32)}
  return(/std/collections/map/at(values, 2i32).tag())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_canonical_direct_map_access_struct_method_chain_diag.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_canonical_direct_map_access_struct_method_chain_diag.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /i32/tag") != std::string::npos);
}

TEST_CASE("prefers canonical bare map method struct chain forwarding in C++ emitter") {
  const std::string source = R"(
CanonicalMarker {
  [i32] value
}

AliasMarker {
  [i32] value
}

[return<AliasMarker>]
/map/at([map<i32, i32>] values, [i32] key) {
  return(AliasMarker(plus(key, 40i32)))
}

[return<CanonicalMarker>]
/std/collections/map/at([map<i32, i32>] values, [i32] key) {
  return(CanonicalMarker(key))
}

[return<int>]
/CanonicalMarker/tag([CanonicalMarker] self) {
  return(self.value)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(2i32, 7i32)}
  return(values.at(2i32).tag())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_bare_map_method_struct_chain_canonical_precedence.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_cpp_bare_map_method_struct_chain_canonical_precedence.out")
          .string();
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_bare_map_method_struct_chain_canonical_precedence_exe")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(quoteShellArg(exePath)) == 2);
  CHECK(readFile(outPath).empty());
}

TEST_CASE("prefers canonical bare map unsafe method struct chain forwarding in C++ emitter") {
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

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(2i32, 7i32)}
  return(values.at_unsafe(2i32).tag())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_bare_map_unsafe_method_struct_chain_canonical_precedence.prime", source);
  const std::string outPath = (testScratchPath("") /
                               "primec_cpp_bare_map_unsafe_method_struct_chain_canonical_precedence.out")
                                  .string();
  const std::string exePath =
      (testScratchPath("") /
       "primec_cpp_bare_map_unsafe_method_struct_chain_canonical_precedence_exe")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(quoteShellArg(exePath)) == 2);
  CHECK(readFile(outPath).empty());
}

TEST_CASE("keeps canonical bare map method non-struct diagnostics in C++ emitter") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/map/at([map<i32, i32>] values, [i32] key) {
  return(Marker(plus(key, 40i32)))
}

[return<i32>]
/std/collections/map/at([map<i32, i32>] values, [i32] key) {
  return(key)
}

[return<int>]
/Marker/tag([Marker] self) {
  return(self.value)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(2i32, 7i32)}
  return(values.at(2i32).tag())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_bare_map_method_struct_chain_canonical_diagnostic.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_bare_map_method_struct_chain_canonical_diagnostic.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /i32/tag") != std::string::npos);
}

TEST_CASE("rejects map access compatibility call struct method chain canonical forwarding in C++ emitter") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/map/at([map<i32, i32>] values, [i32] key) {
  return(Marker(key))
}

[return<int>]
/Marker/tag([Marker] self) {
  return(self.value)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(2i32, 7i32)}
  return(/map/at(values, 2i32).tag())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_map_access_alias_struct_method_chain_canonical_forwarding_reject.prime", source);
  const std::string errPath = (testScratchPath("") /
                               "primec_cpp_map_access_alias_struct_method_chain_canonical_forwarding_reject.err")
                                  .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /map/at") != std::string::npos);
}

TEST_SUITE_END();
