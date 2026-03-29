TEST_CASE("compiles and runs vector namespaced access slash methods through explicit alias helpers in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/at([vector<i32>] values, [i32] index) {
  return(plus(index, 40i32))
}

[effects(heap_alloc), return<int>]
/vector/at_unsafe([vector<i32>] values, [i32] index) {
  return(plus(index, 50i32))
}

[effects(heap_alloc), return<int>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return(7i32)
}

[effects(heap_alloc), return<int>]
/std/collections/vector/at_unsafe([vector<i32>] values, [i32] index) {
  return(8i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(values./vector/at(2i32),
              values./vector/at_unsafe(1i32)))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_vector_access_slash_methods_alias_same_path.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_cpp_vector_access_slash_methods_alias_same_path_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 93);
}

TEST_CASE("C++ emitter rejects vector namespaced access slash methods without alias helper before lowering") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(values./vector/at(1i32),
              values./vector/at_unsafe(2i32)))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_vector_access_slash_methods_alias_no_helper.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_vector_access_slash_methods_alias_no_helper.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=cpp " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("unknown method: /vector/at") != std::string::npos);
}

TEST_CASE("rejects vector namespaced access slash methods without alias helper in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(values./vector/at(1i32),
              values./vector/at_unsafe(2i32)))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_vector_access_slash_methods_alias_no_helper_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_vector_access_slash_methods_alias_no_helper_exe.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("unknown method: /vector/at") != std::string::npos);
}

TEST_CASE("rejects stdlib vector namespaced access slash methods without canonical helper in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(values./std/collections/vector/at(1i32),
              values./std/collections/vector/at_unsafe(2i32)))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_stdlib_vector_access_slash_methods_no_helper_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_stdlib_vector_access_slash_methods_no_helper_exe.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("unknown method: /std/collections/vector/at") != std::string::npos);
}

TEST_CASE("C++ emitter lowers bare vector at methods without helper to deleted stub") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(values.at(1i32))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_bare_vector_at_method_deleted_stub.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_cpp_bare_vector_at_method_deleted_stub.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("ps_missing_vector_at_method_helper") != std::string::npos);
  CHECK(output.find("ps_missing_vector_at_method_helper(values, 1)") != std::string::npos);
}

TEST_CASE("rejects bare vector at methods without helper in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(values.at(1i32))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_bare_vector_at_method_deleted_stub_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_bare_vector_at_method_deleted_stub.err").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("ps_missing_vector_at_method_helper") != std::string::npos);
}

TEST_CASE("C++ emitter lowers bare vector at_unsafe methods without helper to deleted stub") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(values.at_unsafe(1i32))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_bare_vector_at_unsafe_method_deleted_stub.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_cpp_bare_vector_at_unsafe_method_deleted_stub.cpp")
          .string();

  const std::string compileCmd = "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("ps_missing_vector_at_unsafe_method_helper") != std::string::npos);
  CHECK(output.find("ps_missing_vector_at_unsafe_method_helper(values, 1)") != std::string::npos);
}

TEST_CASE("rejects wrapper vector at_unsafe methods without helper in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(5i32, 6i32, 7i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(wrapVector().at_unsafe(1i32))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_wrapper_vector_at_unsafe_method_deleted_stub_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_wrapper_vector_at_unsafe_method_deleted_stub.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("ps_missing_vector_at_unsafe_method_helper") != std::string::npos);
}

TEST_CASE("C++ emitter lowers wrapper bare vector at calls without helper to deleted stub") {
  const std::string source = R"(
[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(5i32, 6i32, 7i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(at(wrapVector(), 1i32))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_wrapper_bare_vector_at_call_deleted_stub.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_cpp_wrapper_bare_vector_at_call_deleted_stub.cpp")
          .string();

  const std::string compileCmd = "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("ps_missing_vector_at_call_helper") != std::string::npos);
  CHECK(output.find("ps_missing_vector_at_call_helper(wrapVector(), 1)") != std::string::npos);
}

TEST_CASE("rejects wrapper bare vector at calls without helper in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(5i32, 6i32, 7i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(at(wrapVector(), 1i32))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_wrapper_bare_vector_at_call_deleted_stub_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_wrapper_bare_vector_at_call_deleted_stub.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("ps_missing_vector_at_call_helper") != std::string::npos);
}

TEST_CASE("C++ emitter lowers wrapper bare vector at_unsafe calls without helper to deleted stub") {
  const std::string source = R"(
[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(5i32, 6i32, 7i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(at_unsafe(wrapVector(), 1i32))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_wrapper_bare_vector_at_unsafe_call_deleted_stub.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_cpp_wrapper_bare_vector_at_unsafe_call_deleted_stub.cpp")
          .string();

  const std::string compileCmd = "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("ps_missing_vector_at_unsafe_call_helper") != std::string::npos);
  CHECK(output.find("ps_missing_vector_at_unsafe_call_helper(wrapVector(), 1)") != std::string::npos);
}

TEST_CASE("C++ emitter lowers wrapper explicit vector access calls without helper to deleted stubs") {
  const std::string source = R"(
[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(5i32, 6i32, 7i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(plus(/vector/at(wrapVector(), 1i32),
              /std/collections/vector/at_unsafe(wrapVector(), 2i32)))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_wrapper_explicit_vector_access_call_deleted_stub.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_cpp_wrapper_explicit_vector_access_call_deleted_stub.cpp")
          .string();

  const std::string compileCmd = "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("ps_missing_vector_at_call_helper") != std::string::npos);
  CHECK(output.find("ps_missing_vector_at_call_helper(wrapVector(), 1)") != std::string::npos);
  CHECK(output.find("ps_missing_vector_at_unsafe_call_helper") != std::string::npos);
  CHECK(output.find("ps_missing_vector_at_unsafe_call_helper(wrapVector(), 2)") != std::string::npos);
}

TEST_CASE("rejects wrapper bare vector at_unsafe calls without helper in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(5i32, 6i32, 7i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(at_unsafe(wrapVector(), 1i32))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_wrapper_bare_vector_at_unsafe_call_deleted_stub_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_wrapper_bare_vector_at_unsafe_call_deleted_stub.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("ps_missing_vector_at_unsafe_call_helper") != std::string::npos);
}

TEST_CASE("rejects wrapper explicit vector access calls without helper in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(5i32, 6i32, 7i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(/vector/at(wrapVector(), 1i32))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_wrapper_explicit_vector_access_call_deleted_stub_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_wrapper_explicit_vector_access_call_deleted_stub.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("ps_missing_vector_at_call_helper") != std::string::npos);
}

TEST_CASE("C++ emitter lowers bare vector mutator statements without helper to deleted stubs") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32, 3i32)}
  push(values, 4i32)
  values.push(5i32)
  pop(values)
  values.pop()
  reserve(values, 8i32)
  values.reserve(9i32)
  clear(values)
  values.clear()
  remove_at(values, 0i32)
  values.remove_at(0i32)
  remove_swap(values, 0i32)
  values.remove_swap(0i32)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_cpp_bare_vector_mutator_deleted_stubs.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_cpp_bare_vector_mutator_deleted_stubs.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("ps_missing_vector_push_call_helper(values, 4)") != std::string::npos);
  CHECK(output.find("ps_missing_vector_push_method_helper(values, 5)") != std::string::npos);
  CHECK(output.find("ps_missing_vector_pop_call_helper(values)") != std::string::npos);
  CHECK(output.find("ps_missing_vector_pop_method_helper(values)") != std::string::npos);
  CHECK(output.find("ps_missing_vector_reserve_call_helper(values, 8)") != std::string::npos);
  CHECK(output.find("ps_missing_vector_reserve_method_helper(values, 9)") != std::string::npos);
  CHECK(output.find("ps_missing_vector_clear_call_helper(values)") != std::string::npos);
  CHECK(output.find("ps_missing_vector_clear_method_helper(values)") != std::string::npos);
  CHECK(output.find("ps_missing_vector_remove_at_call_helper(values, 0)") != std::string::npos);
  CHECK(output.find("ps_missing_vector_remove_at_method_helper(values, 0)") != std::string::npos);
  CHECK(output.find("ps_missing_vector_remove_swap_call_helper(values, 0)") != std::string::npos);
  CHECK(output.find("ps_missing_vector_remove_swap_method_helper(values, 0)") != std::string::npos);
}

TEST_CASE("C++ emitter lowers explicit vector mutator method statements without helper to deleted stubs") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32, 3i32)}
  values./vector/push(4i32)
  values./std/collections/vector/clear()
  return(0i32)
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_explicit_vector_mutator_method_deleted_stubs.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_cpp_explicit_vector_mutator_method_deleted_stubs.cpp")
          .string();

  const std::string compileCmd = "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("ps_missing_vector_push_method_helper(values, 4)") != std::string::npos);
  CHECK(output.find("ps_missing_vector_clear_method_helper(values)") != std::string::npos);
}

TEST_CASE("rejects bare vector mutator call statements without helper in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32, 3i32)}
  push(values, 4i32)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_cpp_bare_vector_mutator_call_deleted_stub_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_bare_vector_mutator_call_deleted_stub.err").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("ps_missing_vector_push_call_helper") != std::string::npos);
}

TEST_CASE("rejects bare vector mutator method statements without helper in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32, 3i32)}
  values.push(4i32)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_cpp_bare_vector_mutator_method_deleted_stub_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_bare_vector_mutator_method_deleted_stub.err").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("ps_missing_vector_push_method_helper") != std::string::npos);
}

TEST_CASE("rejects explicit vector mutator alias method statements without helper in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32, 3i32)}
  values./vector/push(4i32)
  return(0i32)
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_explicit_vector_mutator_alias_method_deleted_stub_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_explicit_vector_mutator_alias_method_deleted_stub.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("ps_missing_vector_push_method_helper") != std::string::npos);
}

TEST_CASE("rejects explicit canonical vector clear method without imported helper in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32, 3i32)}
  values./std/collections/vector/clear()
  return(0i32)
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_explicit_vector_mutator_canonical_method_deleted_stub_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_explicit_vector_mutator_canonical_method_deleted_stub.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("unknown method: /std/collections/vector/clear") != std::string::npos);
}

TEST_CASE("rejects explicit canonical vector push method without imported helper in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32, 3i32)}
  values./std/collections/vector/push(4i32)
  return(0i32)
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_explicit_vector_push_canonical_method_deleted_stub_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_explicit_vector_push_canonical_method_deleted_stub.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("unknown method: /std/collections/vector/push") != std::string::npos);
}

TEST_CASE("rejects explicit canonical vector pop method without imported helper in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32, 3i32)}
  values./std/collections/vector/pop()
  return(0i32)
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_explicit_vector_pop_canonical_method_deleted_stub_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_explicit_vector_pop_canonical_method_deleted_stub.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("unknown method: /std/collections/vector/pop") != std::string::npos);
}

TEST_CASE("rejects explicit canonical vector reserve method without imported helper in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32, 3i32)}
  values./std/collections/vector/reserve(9i32)
  return(0i32)
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_explicit_vector_reserve_canonical_method_deleted_stub_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_explicit_vector_reserve_canonical_method_deleted_stub.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("unknown method: /std/collections/vector/reserve") != std::string::npos);
}

TEST_CASE("compiles and runs explicit canonical vector mutator method helper in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc)]
/std/collections/vector/push([vector<i32> mut] values, [i32] value) {
  assign(values[1i32], plus(value, 70i32))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  values./std/collections/vector/push(3i32)
  return(values[1i32])
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_explicit_canonical_vector_mutator_method_helper.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_cpp_explicit_canonical_vector_mutator_method_helper_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 73);
}

TEST_CASE("C++ emitter lowers alias vector mutator methods with canonical-only helper to deleted stub") {
  const std::string source = R"(
[effects(heap_alloc)]
/std/collections/vector/push([vector<i32> mut] values, [i32] value) {
  assign(values[1i32], plus(value, 70i32))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  values./vector/push(3i32)
  return(values[1i32])
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_alias_vector_mutator_method_canonical_only_deleted_stub.prime", source);
  const std::string outPath =
      (testScratchPath("") /
       "primec_cpp_alias_vector_mutator_method_canonical_only_deleted_stub.cpp")
          .string();

  const std::string compileCmd = "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("ps_missing_vector_push_method_helper(values, 3)") != std::string::npos);
}

TEST_CASE("rejects alias vector mutator methods with canonical-only helper in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc)]
/std/collections/vector/push([vector<i32> mut] values, [i32] value) {
  assign(values[1i32], plus(value, 70i32))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  values./vector/push(3i32)
  return(values[1i32])
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_alias_vector_mutator_method_canonical_only_deleted_stub_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_alias_vector_mutator_method_canonical_only_deleted_stub.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("ps_missing_vector_push_method_helper") != std::string::npos);
}

TEST_CASE("C++ emitter lowers canonical vector mutator methods with alias-only helper to deleted stub") {
  const std::string source = R"(
[effects(heap_alloc)]
/vector/push([vector<i32> mut] values, [i32] value) {
  assign(values[1i32], plus(value, 70i32))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  values./std/collections/vector/push(3i32)
  return(values[1i32])
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_canonical_vector_mutator_method_alias_only_deleted_stub.prime", source);
  const std::string outPath =
      (testScratchPath("") /
       "primec_cpp_canonical_vector_mutator_method_alias_only_deleted_stub.cpp")
          .string();

  const std::string compileCmd = "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("ps_missing_vector_push_method_helper(values, 3)") != std::string::npos);
}
