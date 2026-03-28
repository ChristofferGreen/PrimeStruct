TEST_CASE("rejects wrapper explicit vector capacity calls without helper in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(5i32, 6i32, 7i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(/vector/capacity(wrapVector()))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_wrapper_explicit_vector_capacity_call_deleted_stub_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_wrapper_explicit_vector_capacity_call_deleted_stub.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("unknown call target: /vector/capacity") != std::string::npos);
}

TEST_CASE("compiles and runs wrapper explicit vector count capacity aliases through same-path helpers in C++ emitter") {
  const std::string source = R"(
[return<int>]
/vector/count([vector<i32>] values) {
  return(18i32)
}

[return<int>]
/vector/capacity([vector<i32>] values) {
  return(19i32)
}

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(5i32, 6i32, 7i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(plus(/vector/count(wrapVector()),
              /vector/capacity(wrapVector())))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_wrapper_explicit_vector_count_capacity_alias_same_path.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_cpp_wrapper_explicit_vector_count_capacity_alias_same_path_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 37);
}

TEST_CASE("rejects namespaced wrapper vector capacity vector target without helper in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(5i32, 6i32, 7i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(/std/collections/vector/capacity(wrapVector()))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_namespaced_wrapper_vector_capacity_vector_target_reject.prime", source);
  const std::string outPath = (testScratchPath("") /
                               "primec_cpp_namespaced_wrapper_vector_capacity_vector_target_reject_out.txt")
                                  .string();

  const std::string compileCmd =
      "./primec --emit=cpp " + srcPath + " -o /dev/null --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("unknown call target: /std/collections/vector/capacity") !=
        std::string::npos);
}

TEST_CASE("compiles and runs wrapper bare vector capacity through imported stdlib helper in C++ emitter") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(5i32, 6i32, 7i32))
}

[effects(heap_alloc), return<int>]
/std/collections/vector/capacity([vector<i32>] values) {
  return(17i32)
}

[effects(heap_alloc), return<int>]
main() {
  return(capacity(wrapVector()))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_wrapper_bare_vector_capacity_imported.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_wrapper_bare_vector_capacity_imported_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 17);
}

TEST_CASE("C++ emitter rejects wrapper bare vector capacity calls without helper before emission") {
  const std::string source = R"(
[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(5i32, 6i32, 7i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(capacity(wrapVector()))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_wrapper_bare_vector_capacity_call_deleted_stub.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_cpp_wrapper_bare_vector_capacity_call_deleted_stub.txt")
          .string();

  const std::string compileCmd =
      "./primec --emit=cpp " + srcPath + " -o /dev/null --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("unknown call target: /std/collections/vector/capacity") !=
        std::string::npos);
}

TEST_CASE("rejects wrapper bare vector capacity calls without helper in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(5i32, 6i32, 7i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(capacity(wrapVector()))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_wrapper_bare_vector_capacity_call_deleted_stub_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_wrapper_bare_vector_capacity_call_deleted_stub.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/vector/capacity") !=
        std::string::npos);
}

TEST_CASE("C++ emitter lowers bare vector capacity methods without helper to deleted stub") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(values.capacity())
}
)";
  const std::string srcPath = writeTemp("compile_cpp_bare_vector_capacity_method_deleted_stub.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_cpp_bare_vector_capacity_method_deleted_stub.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("ps_missing_vector_capacity_method_helper") != std::string::npos);
  CHECK(output.find("ps_missing_vector_capacity_method_helper(values)") != std::string::npos);
}

TEST_CASE("rejects bare vector capacity methods without helper in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(values.capacity())
}
)";
  const std::string srcPath = writeTemp("compile_cpp_bare_vector_capacity_method_deleted_stub_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_bare_vector_capacity_method_deleted_stub.err").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("ps_missing_vector_capacity_method_helper") != std::string::npos);
}

TEST_CASE("rejects wrapper vector capacity methods without helper in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(5i32, 6i32, 7i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(wrapVector().capacity())
}
)";
  const std::string srcPath = writeTemp("compile_cpp_wrapper_vector_capacity_method_deleted_stub_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_wrapper_vector_capacity_method_deleted_stub.err").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("unknown method: /vector/capacity") != std::string::npos);
}

TEST_CASE("rejects wrapper vector capacity methods without helper before emission in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(5i32, 6i32, 7i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(wrapVector().capacity())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_wrapper_vector_capacity_method_deleted_stub_cpp.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_cpp_wrapper_vector_capacity_method_deleted_stub.txt")
          .string();

  const std::string compileCmd =
      "./primec --emit=cpp " + srcPath + " -o /dev/null --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("unknown method: /vector/capacity") != std::string::npos);
}

TEST_CASE("C++ emitter rejects duplicate local canonical slash-method vector capacity overloads") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/vector/capacity([map<i32, i32>] values) {
  return(41i32)
}

[effects(heap_alloc), return<int>]
/std/collections/vector/capacity([array<i32>] values) {
  return(42i32)
}

[effects(heap_alloc), return<int>]
/std/collections/vector/capacity([string] values) {
  return(43i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  [array<i32>] items{array<i32>(1i32, 2i32, 3i32)}
  [string] text{"abc"raw_utf8}
  return(plus(plus(values./std/collections/vector/capacity(),
                    items./std/collections/vector/capacity()),
              text./std/collections/vector/capacity()))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_local_canonical_slash_vector_capacity_same_path.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_local_canonical_slash_vector_capacity_same_path_err.txt")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main > /dev/null 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("duplicate definition: /std/collections/vector/capacity") != std::string::npos);
}

TEST_CASE("C++ emitter rejects local canonical slash-method vector capacity on map receiver before emission") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(values./std/collections/vector/capacity())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_local_canonical_slash_vector_capacity_map_deleted_stub.prime", source);
  const std::string outPath =
      (testScratchPath("") /
       "primec_cpp_local_canonical_slash_vector_capacity_map_deleted_stub.txt")
          .string();

  const std::string compileCmd =
      "./primec --emit=cpp " + srcPath + " -o /dev/null --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("unknown method: /std/collections/vector/capacity") !=
        std::string::npos);
}

TEST_CASE("rejects local canonical slash-method vector capacity on string receiver in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [string] value{"abc"raw_utf8}
  return(value./std/collections/vector/capacity())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_local_canonical_slash_vector_capacity_string_deleted_stub_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_local_canonical_slash_vector_capacity_string_deleted_stub.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("unknown method: /std/collections/vector/capacity") !=
        std::string::npos);
}

TEST_CASE("compiles and runs vector namespaced count capacity slash methods through explicit alias helpers in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/count([vector<i32>] values) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
/vector/capacity([vector<i32>] values) {
  return(20i32)
}

[effects(heap_alloc), return<int>]
/std/collections/vector/count([vector<i32>] values) {
  return(7i32)
}

[effects(heap_alloc), return<int>]
/std/collections/vector/capacity([vector<i32>] values) {
  return(8i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(values./vector/count(),
              values./vector/capacity()))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_vector_count_capacity_slash_methods_alias_same_path.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_cpp_vector_count_capacity_slash_methods_alias_same_path_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 110);
}

TEST_CASE("compiles and runs stdlib namespaced vector count capacity slash methods in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/vector/count([vector<i32>] values) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
/std/collections/vector/capacity([vector<i32>] values) {
  return(20i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(values./std/collections/vector/count(),
              values./std/collections/vector/capacity()))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_stdlib_vector_count_capacity_slash_methods.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_stdlib_vector_count_capacity_slash_methods_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 110);
}

TEST_CASE("C++ emitter lowers vector namespaced count capacity slash methods without helper to deleted stubs") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(values./vector/count(),
              values./vector/capacity()))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_vector_count_capacity_slash_methods_deleted_stub.prime", source);
  const std::string outPath =
      (testScratchPath("") /
       "primec_cpp_vector_count_capacity_slash_methods_deleted_stub.cpp")
          .string();

  const std::string compileCmd = "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("ps_missing_vector_count_method_helper") != std::string::npos);
  CHECK(output.find("ps_missing_vector_count_method_helper(values)") != std::string::npos);
  CHECK(output.find("ps_missing_vector_capacity_method_helper") != std::string::npos);
  CHECK(output.find("ps_missing_vector_capacity_method_helper(values)") != std::string::npos);
}

TEST_CASE("C++ emitter lowers stdlib namespaced vector count capacity slash methods without helper to deleted stubs") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(values./std/collections/vector/count(),
              values./std/collections/vector/capacity()))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_stdlib_vector_count_capacity_slash_methods_deleted_stub.prime", source);
  const std::string outPath =
      (testScratchPath("") /
       "primec_cpp_stdlib_vector_count_capacity_slash_methods_deleted_stub.cpp")
          .string();

  const std::string compileCmd = "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("ps_missing_vector_count_method_helper") != std::string::npos);
  CHECK(output.find("ps_missing_vector_count_method_helper(values)") != std::string::npos);
  CHECK(output.find("ps_missing_vector_capacity_method_helper") != std::string::npos);
  CHECK(output.find("ps_missing_vector_capacity_method_helper(values)") != std::string::npos);
}

TEST_CASE("rejects vector namespaced count capacity slash methods without helper in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(values./vector/count(),
              values./vector/capacity()))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_vector_count_capacity_slash_methods_deleted_stub_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_vector_count_capacity_slash_methods_deleted_stub.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  const std::string errors = readFile(errPath);
  CHECK(errors.find("ps_missing_vector_count_method_helper") != std::string::npos);
  CHECK(errors.find("ps_missing_vector_capacity_method_helper") != std::string::npos);
}

TEST_CASE("rejects stdlib namespaced vector count capacity slash methods without helper in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(values./std/collections/vector/count(),
              values./std/collections/vector/capacity()))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_stdlib_vector_count_capacity_slash_methods_deleted_stub_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_stdlib_vector_count_capacity_slash_methods_deleted_stub.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  const std::string errors = readFile(errPath);
  CHECK(errors.find("ps_missing_vector_count_method_helper") != std::string::npos);
  CHECK(errors.find("ps_missing_vector_capacity_method_helper") != std::string::npos);
}

TEST_CASE("C++ emitter lowers cross-path vector count capacity slash methods to deleted stubs") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/vector/count([vector<i32>] values) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
/vector/capacity([vector<i32>] values) {
  return(20i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(values./vector/count(),
              values./std/collections/vector/capacity()))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_vector_cross_path_count_capacity_slash_methods_deleted_stub.prime", source);
  const std::string outPath =
      (testScratchPath("") /
       "primec_cpp_vector_cross_path_count_capacity_slash_methods_deleted_stub.cpp")
          .string();

  const std::string compileCmd = "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("ps_missing_vector_count_method_helper") != std::string::npos);
  CHECK(output.find("ps_missing_vector_count_method_helper(values)") != std::string::npos);
  CHECK(output.find("ps_missing_vector_capacity_method_helper") != std::string::npos);
  CHECK(output.find("ps_missing_vector_capacity_method_helper(values)") != std::string::npos);
}

TEST_CASE("rejects cross-path vector count capacity slash helper routing in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/vector/count([vector<i32>] values) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
/vector/capacity([vector<i32>] values) {
  return(20i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(values./vector/count(),
              values./std/collections/vector/capacity()))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_vector_cross_path_count_capacity_slash_methods_deleted_stub_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_vector_cross_path_count_capacity_slash_methods_deleted_stub.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  const std::string errors = readFile(errPath);
  CHECK(errors.find("ps_missing_vector_count_method_helper") != std::string::npos);
  CHECK(errors.find("ps_missing_vector_capacity_method_helper") != std::string::npos);
}

TEST_CASE("compiles and runs stdlib namespaced vector access slash methods in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return(plus(index, 40i32))
}

[effects(heap_alloc), return<int>]
/std/collections/vector/at_unsafe([vector<i32>] values, [i32] index) {
  return(plus(index, 50i32))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(values./std/collections/vector/at(2i32),
              values./std/collections/vector/at_unsafe(1i32)))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_stdlib_vector_access_slash_methods.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_stdlib_vector_access_slash_methods_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 93);
}

TEST_CASE("C++ emitter lowers stdlib namespaced vector access slash methods without helper to deleted stubs") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(values./std/collections/vector/at(1i32),
              values./std/collections/vector/at_unsafe(2i32)))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_stdlib_vector_access_slash_methods_deleted_stub.prime", source);
  const std::string outPath =
      (testScratchPath("") /
       "primec_cpp_stdlib_vector_access_slash_methods_deleted_stub.cpp")
          .string();

  const std::string compileCmd = "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("ps_missing_vector_at_method_helper") != std::string::npos);
  CHECK(output.find("ps_missing_vector_at_method_helper(values, 1)") != std::string::npos);
  CHECK(output.find("ps_missing_vector_at_unsafe_method_helper") != std::string::npos);
  CHECK(output.find("ps_missing_vector_at_unsafe_method_helper(values, 2)") != std::string::npos);
}

TEST_CASE("rejects stdlib namespaced vector access slash methods without helper in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(values./std/collections/vector/at(1i32),
              values./std/collections/vector/at_unsafe(2i32)))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_stdlib_vector_access_slash_methods_deleted_stub_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_stdlib_vector_access_slash_methods_deleted_stub.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  const std::string errors = readFile(errPath);
  CHECK(errors.find("ps_missing_vector_at_method_helper") != std::string::npos);
  CHECK(errors.find("ps_missing_vector_at_unsafe_method_helper") != std::string::npos);
}

