TEST_CASE("vm array access checks bounds") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(4i32)}
  return(plus(100i32, values[9i32]))
}
)";
  const std::string srcPath = writeTemp("vm_array_bounds.prime", source);
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_vm_array_bounds_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(errPath) == "array index out of bounds\n");
}

TEST_CASE("vm array access with u64 index") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(4i32, 7i32, 9i32)}
  return(plus(100i32, values[1u64]))
}
)";
  const std::string srcPath = writeTemp("vm_array_u64.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 107);
}

TEST_CASE("vm array access rejects negative index") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(4i32)}
  return(plus(100i32, values[-1i32]))
}
)";
  const std::string srcPath = writeTemp("vm_array_negative.prime", source);
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_vm_array_negative_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(errPath) == "array index out of bounds\n");
}

TEST_CASE("vm vector access checks bounds") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(4i32)}
  return(plus(100i32, values[9i32]))
}
)";
  const std::string srcPath = writeTemp("vm_vector_bounds.prime", source);
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_vm_vector_bounds_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(errPath) == "array index out of bounds\n");
}

TEST_CASE("vm vector access rejects negative index") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(4i32)}
  return(plus(100i32, values[-1i32]))
}
)";
  const std::string srcPath = writeTemp("vm_vector_negative.prime", source);
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_vm_vector_negative_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(errPath) == "array index out of bounds\n");
}

TEST_CASE("vm array unsafe access reads element") {
  const std::string source = R"(
[return<int> effects(io_out)]
main() {
  [array<i32>] values{array<i32>(4i32, 7i32, 9i32)}
  print_line(at_unsafe(values, 1i32))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_array_unsafe.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_vm_array_unsafe_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "7\n");
}

TEST_CASE("vm array unsafe access with u64 index") {
  const std::string source = R"(
[return<int> effects(io_out)]
main() {
  [array<i32>] values{array<i32>(4i32, 7i32, 9i32)}
  print_line(at_unsafe(values, 1u64))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_array_unsafe_u64.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_vm_array_unsafe_u64_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "7\n");
}

TEST_CASE("vm argv access checks bounds") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  print_line(args[9i32])
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_argv_bounds.prime", source);
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_vm_argv_bounds_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(errPath) == "array index out of bounds\n");
}

TEST_CASE("vm argv access rejects negative index") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  print_line(args[-1i32])
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_argv_negative.prime", source);
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_vm_argv_negative_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(errPath) == "array index out of bounds\n");
}

TEST_CASE("vm argv unsafe access skips bounds") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  print_line(at_unsafe(args, 9i32))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_argv_unsafe_bounds.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_vm_argv_unsafe_out.txt").string();
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_vm_argv_unsafe_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath).empty());
  CHECK(readFile(outPath).empty());
}

TEST_CASE("vm argv unsafe access with u64 index") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  print_line(at_unsafe(args, 1u64))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_argv_unsafe_u64.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_vm_argv_unsafe_u64_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main -- alpha beta > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "alpha\n");
}

TEST_CASE("vm argv unsafe access skips negative index") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  print_line(at_unsafe(args, -1i32))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_argv_unsafe_negative.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_vm_argv_unsafe_negative_out.txt").string();
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_vm_argv_unsafe_negative_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath).empty());
  CHECK(readFile(outPath).empty());
}

TEST_CASE("vm argv binding checks bounds") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  [string] value{args[9i32]}
  print_line(value)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_argv_binding_bounds.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_argv_binding_bounds_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(errPath) == "array index out of bounds\n");
}

TEST_CASE("vm argv binding unsafe skips bounds") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  [string] value{at_unsafe(args, 9i32)}
  print_line(value)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_argv_binding_unsafe.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_vm_argv_binding_unsafe_out.txt").string();
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_vm_argv_binding_unsafe_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath).empty());
  CHECK(readFile(outPath).empty());
}

TEST_CASE("vm argv unsafe binding copy skips bounds") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  [string] first{at_unsafe(args, 9i32)}
  [string] second{first}
  print_line(second)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_argv_binding_unsafe_copy.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_argv_binding_unsafe_copy_out.txt").string();
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_argv_binding_unsafe_copy_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath).empty());
  CHECK(readFile(outPath).empty());
}

TEST_CASE("vm argv string binding count fails") {
  const std::string source = R"(
[return<int>]
main([array<string>] args) {
  [string] value{args[0i32]}
  return(count(value))
}
)";
  const std::string srcPath = writeTemp("vm_argv_binding_count.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_argv_binding_count_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath) ==
        "Semantic error: entry argument strings are only supported in print calls or string bindings\n");
}

TEST_CASE("vm argv string binding index fails") {
  const std::string source = R"(
[return<int>]
main([array<string>] args) {
  [string] value{args[0i32]}
  return(value[0i32])
}
)";
  const std::string srcPath = writeTemp("vm_argv_binding_index.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_argv_binding_index_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath) ==
        "Semantic error: entry argument strings are only supported in print calls or string bindings\n");
}

TEST_CASE("rejects vm argv call argument") {
  const std::string source = R"(
[return<void> effects(io_out)]
echo([string] msg) {
  print_line(msg)
}

[return<int> effects(io_out)]
main([array<string>] args) {
  echo(args[9i32])
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_argv_call_bounds.prime", source);
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_vm_argv_call_bounds_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath) ==
        "Semantic error: entry argument strings are only supported in print calls or string bindings\n");
}

TEST_CASE("rejects vm argv call argument unsafe") {
  const std::string source = R"(
[return<void> effects(io_out)]
echo([string] msg) {
  print_line(msg)
}

[return<int> effects(io_out)]
main([array<string>] args) {
  echo(at_unsafe(args, 9i32))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_argv_call_unsafe.prime", source);
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_vm_argv_call_unsafe_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath) ==
        "Semantic error: entry argument strings are only supported in print calls or string bindings\n");
}
