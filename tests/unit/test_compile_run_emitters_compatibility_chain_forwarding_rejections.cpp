#include "test_compile_run_helpers.h"
#include "test_compile_run_collection_conformance_helpers.h"
#include "test_compile_run_emitters_helpers.h"

#include "primec/testing/EmitterHelpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.emitters.cpp");

TEST_CASE("rejects map access compatibility call struct method chain canonical diagnostics forwarding in C++ emitter") {
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
      writeTemp("compile_cpp_map_access_alias_struct_method_chain_canonical_diagnostic.prime", source);
  const std::string errPath = (testScratchPath("") /
                               "primec_cpp_map_access_alias_struct_method_chain_canonical_diagnostic.err")
                                  .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /map/at") != std::string::npos);
}

TEST_CASE("rejects map unsafe compatibility call struct method chain canonical forwarding in C++ emitter") {
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
      writeTemp("compile_cpp_map_access_alias_unsafe_struct_method_chain_canonical_forwarding_reject.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_map_access_alias_unsafe_struct_method_chain_canonical_forwarding_reject.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /map/at_unsafe") != std::string::npos);
}

TEST_CASE("rejects map unsafe compatibility call struct method chain primitive argument diagnostics in C++ emitter") {
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
      writeTemp("compile_cpp_map_access_alias_unsafe_struct_method_chain_canonical_diagnostic.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_map_access_alias_unsafe_struct_method_chain_canonical_diagnostic.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /map/at_unsafe") != std::string::npos);
}

TEST_CASE("rejects vector alias access auto wrapper canonical struct-return forwarding in C++ emitter") {
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
      writeTemp("compile_cpp_vector_alias_access_auto_wrapper_canonical_struct_return_reject.prime", source);
  const std::string errPath = (testScratchPath("") /
                               "primec_cpp_vector_alias_access_auto_wrapper_canonical_struct_return_reject.err")
                                  .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /i32/tag") != std::string::npos);
}

TEST_CASE("rejects vector alias access auto wrapper canonical diagnostics forwarding in C++ emitter") {
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
      writeTemp("compile_cpp_vector_alias_access_auto_wrapper_canonical_diagnostic.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_vector_alias_access_auto_wrapper_canonical_diagnostic.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("argument type mismatch for /i32/tag parameter marker") != std::string::npos);
}

TEST_CASE("C++ emitter keeps vector method alias access struct method forwarding") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/vector/at([vector<i32>] values, [i32] index) {
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
      writeTemp("compile_cpp_vector_method_alias_access_struct_method_chain_same_path_forwarding.prime",
                source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_cpp_vector_method_alias_access_struct_method_chain_same_path_forwarding_exe")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("rejects vector method alias access canonical-only helper routing in C++ emitter" * doctest::skip(true)) {
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
      writeTemp("compile_cpp_vector_method_alias_access_canonical_only_helper_routing_reject.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_vector_method_alias_access_canonical_only_helper_routing_reject.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /vector/at") != std::string::npos);
}

TEST_CASE("rejects vector method alias access struct method chain with helper receiver diagnostics in C++ emitter") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/vector/at([vector<i32>] values, [i32] index) {
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
      writeTemp("compile_cpp_vector_method_alias_access_struct_method_chain_same_path_diagnostic.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_vector_method_alias_access_struct_method_chain_canonical_diagnostic.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unable to infer return type on /project") != std::string::npos);
}

TEST_CASE("rejects vector method alias access field expression with struct receiver diagnostics in C++ emitter") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/vector/at([vector<i32>] values, [i32] index) {
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
      writeTemp("compile_cpp_vector_method_alias_access_field_expression_same_path_struct_receiver_diag.prime",
                source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_vector_method_alias_access_field_expression_struct_receiver_diag.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("field access requires struct receiver") != std::string::npos);
}

TEST_CASE("C++ emitter keeps vector unsafe method alias access struct method forwarding") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/vector/at_unsafe([vector<i32>] values, [i32] index) {
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
      writeTemp("compile_cpp_vector_method_alias_access_unsafe_struct_method_chain_same_path_forwarding.prime",
                source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_vector_method_alias_access_unsafe_struct_method_chain_same_path_forwarding.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  const std::string err = readFile(errPath);
  CHECK(err.find("unknown method: /vector/at_unsafe") != std::string::npos);
}

TEST_CASE("rejects vector method alias access receiver fallback without helper in C++ emitter") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  tag([i32] value) {
    return(plus(value, 40i32))
  }
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(values./vector/at(1i32).tag())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_vector_method_alias_access_receiver_fallback_reject.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_vector_method_alias_access_receiver_fallback_reject.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  const std::string err = readFile(errPath);
  CHECK(err.find("unknown method: /vector/at") != std::string::npos);
}

TEST_CASE("rejects vector unsafe method alias access field expression with struct receiver diagnostics in C++ emitter") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/vector/at_unsafe([vector<i32>] values, [i32] index) {
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
      writeTemp("compile_cpp_vector_method_alias_access_unsafe_field_expression_same_path_struct_receiver_diag.prime",
                source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_vector_method_alias_access_unsafe_field_expression_struct_receiver_diag.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK_FALSE(readFile(errPath).empty());
}

TEST_CASE("rejects vector unsafe method alias access receiver fallback without helper in C++ emitter") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  tag([i32] value) {
    return(plus(value, 40i32))
  }
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(values./vector/at_unsafe(1i32).tag())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_vector_method_alias_access_unsafe_receiver_fallback_reject.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_vector_method_alias_access_unsafe_receiver_fallback_reject.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  const std::string err = readFile(errPath);
  CHECK(err.find("unknown method: /vector/at_unsafe") != std::string::npos);
}

TEST_CASE("C++ emitter keeps vector method alias struct-return precedence over canonical helper" * doctest::skip(true)) {
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
      writeTemp("compile_cpp_vector_method_struct_field_alias_precedence.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_vector_method_struct_field_alias_precedence_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 42);
}

TEST_CASE("C++ emitter keeps canonical vector method access struct forwarding" * doctest::skip(true)) {
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
      writeTemp("compile_cpp_canonical_vector_method_struct_chain_forwarding.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_canonical_vector_method_struct_chain_forwarding_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("C++ emitter keeps canonical vector unsafe method field forwarding" * doctest::skip(true)) {
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
      writeTemp("compile_cpp_canonical_vector_unsafe_method_field_forwarding.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_canonical_vector_unsafe_method_field_forwarding_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("keeps canonical map slash-method struct method chain forwarding in C++ emitter" * doctest::skip(true)) {
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
      writeTemp("compile_cpp_map_method_alias_access_struct_method_chain_canonical_forwarding_reject.prime",
                source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_map_method_alias_access_struct_method_chain_canonical_forwarding_reject.err")
          .string();
  const std::string exePath =
      (testScratchPath("") /
       "primec_cpp_map_method_alias_access_struct_method_chain_canonical_forwarding_exe")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 0);
  CHECK(readFile(errPath).empty());
  CHECK(runCommand(quoteShellArg(exePath)) == 2);
}


TEST_SUITE_END();
