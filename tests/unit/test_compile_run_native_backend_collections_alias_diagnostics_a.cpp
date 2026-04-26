#include "test_compile_run_helpers.h"

#include "test_compile_run_collection_conformance_helpers.h"
#include "test_compile_run_container_error_conformance_helpers.h"
#include "test_compile_run_checked_pointer_conformance_helpers.h"
#include "test_compile_run_unchecked_pointer_conformance_helpers.h"
#include "test_compile_run_native_backend_collections_helpers.h"

#if PRIMESTRUCT_NATIVE_COLLECTIONS_ENABLED
TEST_SUITE_BEGIN("primestruct.compile.run.native_backend.collections");

TEST_CASE("rejects native templated stdlib vector wrapper temporary unsafe method index mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<vector<T>>]
wrapVector<T>([T] value) {
  return(vectorSingle<T>(value))
}

[return<int>]
main() {
  return(wrapVector<i32>(4i32).at_unsafe(true))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_stdlib_collection_shim_templated_return_vector_temp_unsafe_method_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects native vector alias access auto wrapper primitive receiver diagnostics") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<int>]
/vector/at([vector<i32>] values, [i32] index) {
  return(index)
}

[return<Marker>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return(Marker(plus(index, 40i32)))
}

[return<int>]
/Marker/tag([Marker] self) {
  return(self.value)
}

[return<auto>]
project([vector<i32>] values) {
  return(/vector/at(values, 2i32))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(project(values).tag())
}
)";
  const std::string srcPath =
      writeTemp("compile_native_vector_alias_access_auto_wrapper_canonical_struct_return_reject.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_native_vector_alias_access_auto_wrapper_canonical_struct_return_reject.err")
          .string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /i32/tag") != std::string::npos);
}

TEST_CASE("rejects native vector alias access auto wrapper canonical diagnostics forwarding") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<int>]
/vector/at([vector<i32>] values, [i32] index) {
  return(index)
}

[return<Marker>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return(Marker(plus(index, 40i32)))
}

[return<int>]
/i32/tag([i32] self, [bool] marker) {
  return(self)
}

[return<auto>]
project([vector<i32>] values) {
  return(/vector/at(values, 2i32))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(project(values).tag(1i32))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_vector_alias_access_auto_wrapper_canonical_diagnostic.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_native_vector_alias_access_auto_wrapper_canonical_diagnostic.err")
          .string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("argument type mismatch for /i32/tag parameter marker") !=
        std::string::npos);
}

TEST_CASE("rejects native vector alias access struct method chain with rooted helper diagnostics") {
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
      writeTemp("compile_native_vector_alias_access_struct_method_chain_canonical_forwarding_reject.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_native_vector_alias_access_struct_method_chain_canonical_forwarding_reject.err")
          .string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /vector/at") != std::string::npos);
}

TEST_CASE("rejects native vector alias access struct method chain with primitive receiver diagnostics") {
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
      writeTemp("compile_native_vector_alias_access_struct_method_chain_canonical_diagnostic.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_native_vector_alias_access_struct_method_chain_canonical_diagnostic.err")
          .string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /vector/at") !=
        std::string::npos);
}

TEST_CASE("rejects native vector alias access field expression with struct receiver diagnostics") {
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
      writeTemp("compile_native_vector_alias_access_field_expression_struct_receiver_diag.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_native_vector_alias_access_field_expression_struct_receiver_diag.err")
          .string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /vector/at") != std::string::npos);
}

TEST_CASE("keeps native canonical vector access call struct method chain forwarding") {
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
      writeTemp("compile_native_canonical_vector_access_struct_method_chain_forwarding.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_native_canonical_vector_access_struct_method_chain_forwarding_exe")
          .string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(quoteShellArg(exePath)) == 2);
}

TEST_CASE("rejects native canonical vector unsafe access field expression forwarding") {
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
      writeTemp("compile_native_canonical_vector_access_unsafe_field_expression_forwarding.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_native_canonical_vector_access_unsafe_field_expression_forwarding_exe")
          .string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("rejects native map access compatibility call struct method chain with primitive receiver diagnostics") {
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
      writeTemp("compile_native_map_access_alias_struct_method_chain_canonical_forwarding_reject.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_native_map_access_alias_struct_method_chain_canonical_forwarding_reject.err")
          .string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /map/at") != std::string::npos);
}

TEST_CASE("rejects native map access compatibility call struct method chain with primitive argument diagnostics") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/map/at([map<i32, i32>] values, [i32] key) {
  return(Marker(key))
}

[return<int>]
/i32/tag([i32] self, [bool] marker) {
  return(self)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(2i32, 7i32)}
  return(/map/at(values, 2i32).tag(1i32))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_map_access_alias_struct_method_chain_canonical_diagnostic.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_native_map_access_alias_struct_method_chain_canonical_diagnostic.err")
          .string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /map/at") != std::string::npos);
}

TEST_CASE("rejects native map unsafe compatibility call struct method chain with primitive receiver diagnostics") {
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
  return(/map/at_unsafe(values, 2i32).tag())
}
)";
  const std::string srcPath =
      writeTemp("compile_native_map_access_alias_unsafe_struct_method_chain_canonical_forwarding_reject.prime",
                source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_native_map_access_alias_unsafe_struct_method_chain_canonical_forwarding_reject.err")
          .string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /map/at_unsafe") != std::string::npos);
}

TEST_CASE("rejects native map unsafe compatibility call struct method chain with primitive argument diagnostics") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/map/at_unsafe([map<i32, i32>] values, [i32] key) {
  return(Marker(key))
}

[return<int>]
/i32/tag([i32] self, [bool] marker) {
  return(self)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(2i32, 7i32)}
  return(/map/at_unsafe(values, 2i32).tag(1i32))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_map_access_alias_unsafe_struct_method_chain_canonical_diagnostic.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_native_map_access_alias_unsafe_struct_method_chain_canonical_diagnostic.err")
          .string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /map/at_unsafe") != std::string::npos);
}

TEST_CASE("rejects native vector method alias access struct method chain with array receiver diagnostics") {
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

[return<auto>]
project([vector<i32>] values) {
  return(values./vector/at(2i32).tag())
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(project(values))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_vector_method_alias_access_struct_method_chain_canonical_forwarding_reject.prime",
                source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_native_vector_method_alias_access_struct_method_chain_canonical_forwarding_reject.err")
          .string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /array/tag") != std::string::npos);
}

TEST_CASE("rejects native vector namespaced access slash methods without alias helper on vector receiver") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(values./vector/at(1i32),
              values./vector/at_unsafe(2i32)))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_vector_access_slash_methods_alias_no_helper.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_native_vector_access_slash_methods_alias_no_helper.err")
          .string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  const std::string err = readFile(errPath);
  CHECK(err.find("unknown method: /vector/at") != std::string::npos);
}

TEST_CASE("rejects native array compatibility access slash methods on vector receiver") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(values./array/at(1i32),
              values./array/at_unsafe(2i32)))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_array_access_slash_methods_vector_no_helper.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_native_array_access_slash_methods_vector_no_helper.err")
          .string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /array/at") != std::string::npos);
}

TEST_CASE("rejects native array compatibility access slash method chain before receiver typing") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  tag([i32] value) {
    return(plus(value, 1i32))
  }
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(values./array/at_unsafe(2i32).tag())
}
)";
  const std::string srcPath =
      writeTemp("compile_native_array_access_slash_method_chain_vector_no_helper.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_native_array_access_slash_method_chain_vector_no_helper.err")
          .string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /array/at_unsafe") != std::string::npos);
}

TEST_CASE("rejects native wrapper-returned array compatibility access slash method chains before receiver typing") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  tag([i32] value) {
    return(plus(value, 1i32))
  }
}

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(5i32, 6i32, 7i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(plus(wrapVector()./array/at(1i32).tag(),
              wrapVector()./array/at_unsafe(2i32).tag()))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_wrapper_array_access_slash_method_chain_vector_no_helper.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_native_wrapper_array_access_slash_method_chain_vector_no_helper.err")
          .string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /array/at") != std::string::npos);
}

TEST_SUITE_END();
#endif
