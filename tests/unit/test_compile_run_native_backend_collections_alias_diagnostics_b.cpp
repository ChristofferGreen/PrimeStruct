#include "test_compile_run_helpers.h"

#include "test_compile_run_collection_conformance_helpers.h"
#include "test_compile_run_container_error_conformance_helpers.h"
#include "test_compile_run_checked_pointer_conformance_helpers.h"
#include "test_compile_run_unchecked_pointer_conformance_helpers.h"
#include "test_compile_run_native_backend_collections_helpers.h"

#if PRIMESTRUCT_NATIVE_COLLECTIONS_ENABLED
TEST_SUITE_BEGIN("primestruct.compile.run.native_backend.collections");

TEST_CASE("rejects native vector method alias access struct method chain with primitive argument diagnostics" * doctest::skip(true)) {
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

[return<auto>]
project([vector<i32>] values) {
  return(values./vector/at(2i32).tag(1i32))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(project(values))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_vector_method_alias_access_struct_method_chain_canonical_diagnostic.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_native_vector_method_alias_access_struct_method_chain_canonical_diagnostic.err")
          .string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("argument type mismatch for /i32/tag parameter marker") != std::string::npos);
}

TEST_CASE("rejects native vector method alias access field expression with struct receiver diagnostics" * doctest::skip(true)) {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return(Marker(index))
}

[return<auto>]
project([vector<i32>] values) {
  return(values./vector/at(2i32).value)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(project(values))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_vector_method_alias_access_field_expression_struct_receiver_diag.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_native_vector_method_alias_access_field_expression_struct_receiver_diag.err")
          .string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("field access requires struct receiver") != std::string::npos);
}

TEST_CASE("rejects native vector unsafe method alias access struct method chain with primitive receiver diagnostics" * doctest::skip(true)) {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/vector/at_unsafe([vector<i32>] values, [i32] index) {
  return(Marker(index))
}

[return<int>]
/Marker/tag([Marker] self) {
  return(self.value)
}

[return<auto>]
project([vector<i32>] values) {
  return(values./vector/at_unsafe(2i32).tag())
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(project(values))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_vector_method_alias_access_unsafe_struct_method_chain_canonical_forwarding_reject.prime",
                source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_native_vector_method_alias_access_unsafe_struct_method_chain_canonical_forwarding_reject.err")
          .string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /i32/tag") != std::string::npos);
}

TEST_CASE("rejects native vector unsafe method alias access field expression with struct receiver diagnostics" * doctest::skip(true)) {
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
  return(values./vector/at_unsafe(2i32).value)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(project(values))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_vector_method_alias_access_unsafe_field_expression_struct_receiver_diag.prime",
                source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_native_vector_method_alias_access_unsafe_field_expression_struct_receiver_diag.err")
          .string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("field access requires struct receiver") != std::string::npos);
}

TEST_CASE("rejects native map method alias access struct method chain with primitive receiver diagnostics") {
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

[return<auto>]
project([map<i32, i32>] values) {
  return(values./std/collections/map/at(2i32).tag())
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(2i32, 7i32)}
  return(project(values))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_map_method_alias_access_struct_method_chain_canonical_forwarding_reject.prime",
                source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_native_map_method_alias_access_struct_method_chain_canonical_forwarding_reject.err")
          .string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unable to infer return type on /project") != std::string::npos);
}

TEST_CASE("keeps canonical native map method access field expression forwarding") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/map/at([map<i32, i32>] values, [i32] key) {
  return(Marker(key))
}

[return<auto>]
project([map<i32, i32>] values) {
  return(values.at(2i32).value)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(2i32, 7i32)}
  return(project(values))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_canonical_map_method_field_access_forwarding.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_canonical_map_method_field_access_forwarding_exe")
          .string();
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("native vector method alias struct-return precedence currently crashes at runtime") {
  const std::string source = R"(
AliasMarker {
  [i32] value
}

CanonicalMarker {
  [i32] value
}

[return<AliasMarker>]
/vector/at([vector<i32>] values, [i32] index) {
  return(AliasMarker(plus(index, 40i32)))
}

[return<CanonicalMarker>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return(CanonicalMarker(index))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(values.at(2i32).value)
}
)";
  const std::string srcPath =
      writeTemp("compile_native_vector_method_struct_field_alias_precedence.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_vector_method_struct_field_alias_precedence_exe")
          .string();
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) != 0);
}

TEST_CASE("native keeps primitive diagnostics for canonical vector method access") {
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
  return(values.at(2i32).tag())
}
)";
  const std::string srcPath =
      writeTemp("compile_native_canonical_vector_method_struct_chain_reject.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_native_canonical_vector_method_struct_chain_reject.err")
          .string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /i32/tag") != std::string::npos);
}

TEST_CASE("native keeps struct receiver diagnostics for canonical vector unsafe method access") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/vector/at_unsafe([vector<i32>] values, [i32] index) {
  return(Marker(index))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(values.at_unsafe(2i32).value)
}
)";
  const std::string srcPath =
      writeTemp("compile_native_canonical_vector_unsafe_method_field_reject.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_native_canonical_vector_unsafe_method_field_reject.err")
          .string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("field access requires struct receiver") != std::string::npos);
}

TEST_CASE("rejects native map method alias access struct method chain with primitive argument diagnostics") {
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

[return<auto>]
project([map<i32, i32>] values) {
  return(values./std/collections/map/at(2i32).tag(1i32))
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(2i32, 7i32)}
  return(project(values))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_map_method_alias_access_struct_method_chain_canonical_diagnostic.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_native_map_method_alias_access_struct_method_chain_canonical_diagnostic.err")
          .string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unable to infer return type on /project") != std::string::npos);
}

TEST_CASE("rejects native wrapper-returned map method alias primitive receiver fallback") {
  const std::string source = R"(
[effects(heap_alloc), return</std/collections/map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(2i32, 7i32))
}

[return<int>]
/i32/tag([i32] self) {
  return(plus(self, 40i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(wrapMap()./std/collections/map/at(2i32).tag())
}
)";
  const std::string srcPath =
      writeTemp("compile_native_wrapper_map_method_alias_primitive_receiver_fallback.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_native_wrapper_map_method_alias_primitive_receiver_fallback.err")
          .string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /map/at") != std::string::npos);
}

TEST_CASE("rejects native wrapper-returned canonical map slash-method struct receiver fallback") {
  const std::string source = R"(
Marker {
  [i32] value
}

[effects(heap_alloc), return</std/collections/map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(2i32, 7i32))
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
  return(wrapMap()./std/collections/map/at(2i32).tag())
}
)";
  const std::string srcPath =
      writeTemp("compile_native_wrapper_map_method_alias_struct_receiver_forwarding.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_native_wrapper_map_method_alias_struct_receiver_forwarding.err")
          .string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /map/at") != std::string::npos);
}

TEST_CASE("rejects native wrapper-returned canonical direct-call map receiver fallback") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  tag([i32] value) {
    return(plus(value, 40i32))
  }
}

[effects(heap_alloc), return</std/collections/map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(2i32, 7i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(/std/collections/map/at(wrapMap(), 2i32).tag())
}
)";
  const std::string srcPath =
      writeTemp("compile_native_wrapper_canonical_direct_map_receiver_fallback.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_native_wrapper_canonical_direct_map_receiver_fallback.err")
          .string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/map/at") !=
        std::string::npos);
}

TEST_CASE("rejects native wrapper-returned compatibility direct-call map receiver fallback") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  tag([i32] value) {
    return(plus(value, 40i32))
  }
}

[effects(heap_alloc), return</std/collections/map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(2i32, 7i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(/map/at(wrapMap(), 2i32).tag())
}
)";
  const std::string srcPath =
      writeTemp("compile_native_wrapper_compatibility_direct_map_receiver_fallback.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_native_wrapper_compatibility_direct_map_receiver_fallback.err")
          .string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /map/at") != std::string::npos);
}

TEST_CASE("native keeps wrapper-returned map method alias primitive argument diagnostics") {
  const std::string source = R"(
Marker {
  [i32] value
}

[effects(heap_alloc), return</std/collections/map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(2i32, 7i32))
}

[return<Marker>]
/std/collections/map/at([map<i32, i32>] values, [i32] key) {
  return(Marker(key))
}

[return<int>]
/i32/tag([i32] self, [bool] marker) {
  return(self)
}

[return<auto>]
project() {
  return(wrapMap()./std/collections/map/at(2i32).tag(1i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(project())
}
)";
  const std::string srcPath =
      writeTemp("compile_native_wrapper_map_method_alias_primitive_argument_diag.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_native_wrapper_map_method_alias_primitive_argument_diag.err")
          .string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unable to infer return type on /project") != std::string::npos);
}

TEST_SUITE_END();
#endif
