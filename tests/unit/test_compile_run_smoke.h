TEST_SUITE_BEGIN("primestruct.compile.run.smoke");

TEST_CASE("compiles and runs simple main") {
  const std::string source = R"(
[return<int>]
main() {
  return(7i32)
}
)";
  const std::string srcPath = writeTemp("compile_simple.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_simple_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  const std::string runVmCmd = "./primevm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
  CHECK(runCommand(runVmCmd) == 7);
}

TEST_CASE("default entry path is main") {
  const std::string source = R"(
[return<int>]
main() {
  return(4i32)
}
)";
  const std::string srcPath = writeTemp("compile_default_entry.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_default_entry_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath;
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 4);

  const std::string runVmCmd = "./primevm " + srcPath;
  CHECK(runCommand(runVmCmd) == 4);
}

TEST_CASE("defaults to native output with stem name") {
  const std::string source = R"(
[return<int>]
main() {
  return(5i32)
}
)";
  const std::string srcPath = writeTemp("compile_default_output.prime", source);
  const std::filesystem::path outDir = std::filesystem::temp_directory_path() / "primec_default_out";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::string compileCmd =
      "./primec " + quoteShellArg(srcPath) + " --out-dir " + quoteShellArg(outDir.string()) + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);

  const std::filesystem::path outputPath = outDir / std::filesystem::path(srcPath).stem();
  CHECK(std::filesystem::exists(outputPath));
  CHECK(runCommand(quoteShellArg(outputPath.string())) == 5);
}

TEST_CASE("primevm forwards entry args") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  print_line(args[1i32])
  return(args.count())
}
)";
  const std::string srcPath = writeTemp("vm_args.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primevm_args_out.txt").string();

  const std::string runVmCmd = "./primevm " + srcPath + " --entry /main -- alpha beta > " + outPath;
  CHECK(runCommand(runVmCmd) == 3);
  CHECK(readFile(outPath) == "alpha\n");
}

TEST_CASE("compiles and runs with line comments after expressions") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{7i32}// comment with a+b and a/b should be ignored
  return(value)
}
)";
  const std::string srcPath = writeTemp("compile_line_comment.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_line_comment_exe").string();
  const std::string nativePath = (std::filesystem::temp_directory_path() / "primec_line_comment_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 7);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 7);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 7);
}

TEST_CASE("compiles and runs string count and indexing in C++ emitter") {
  const std::string source = R"(
[return<int>]
main() {
  [string] text{"abc"utf8}
  [i32] a{at(text, 0i32)}
  [i32] b{at_unsafe(text, 1i32)}
  [i32] len{count(text)}
  return(plus(plus(a, b), len))
}
)";
  const std::string srcPath = writeTemp("compile_string_index.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_string_index_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == (97 + 98 + 3));
}

TEST_CASE("compiles and runs single-quoted strings in C++ emitter") {
  const std::string source = R"(
[return<int>]
main() {
  [string] text{'{"k":"v"}'utf8}
  [i32] k{at(text, 2i32)}
  [i32] v{at_unsafe(text, 6i32)}
  [i32] len{count(text)}
  return(plus(plus(k, v), len))
}
)";
  const std::string srcPath = writeTemp("compile_single_quoted_string.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_single_quoted_string_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == (107 + 118 + 9));
}

TEST_CASE("compiles and runs method calls via type namespaces") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  inc([i32] self) {
    return(plus(self, 1i32))
  }
}

[return<int>]
main() {
  return(1i32.inc())
}
)";
  const std::string srcPath = writeTemp("compile_method_call_i32.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_method_call_i32_exe").string();
  const std::string nativePath = (std::filesystem::temp_directory_path() / "primec_method_call_i32_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 2);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 2);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 2);
}

TEST_CASE("compiles and runs count forwarding to method") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  count([i32] self) {
    return(plus(self, 4i32))
  }
}

[return<int>]
main() {
  return(count(3i32))
}
)";
  const std::string srcPath = writeTemp("compile_count_forward.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_count_forward_exe").string();
  const std::string nativePath = (std::filesystem::temp_directory_path() / "primec_count_forward_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 7);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 7);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 7);
}

TEST_CASE("compiles and runs boolean ops with integer inputs") {
  const std::string source = R"(
[return<bool>]
main() {
  return(and(1i32, or(0i32, not(0i32))))
}
)";
  const std::string srcPath = writeTemp("compile_bool_ops_int.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_bool_ops_int_exe").string();
  const std::string nativePath = (std::filesystem::temp_directory_path() / "primec_bool_ops_int_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 1);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 1);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 1);
}

TEST_CASE("compiles and runs boolean ops short-circuit") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{0i32}
  or(1i32, assign(value, 2i32))
  and(0i32, assign(value, 3i32))
  return(value)
}
)";
  const std::string srcPath = writeTemp("compile_bool_ops_short_circuit.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_bool_ops_short_circuit_exe").string();
  const std::string nativePath =
      (std::filesystem::temp_directory_path() / "primec_bool_ops_short_circuit_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 0);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 0);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 0);
}

TEST_CASE("compiles and runs import after definitions") {
  const std::string source = R"(
namespace util {
  [return<int>]
  inc([i32] value) {
    return(plus(value, 1i32))
  }
}
[return<int>]
main() {
  return(inc(4i32))
}
import /util
)";
  const std::string srcPath = writeTemp("compile_import_after_defs.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_import_after_defs_exe").string();
  const std::string nativePath =
      (std::filesystem::temp_directory_path() / "primec_import_after_defs_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 5);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 5);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 5);
}

TEST_CASE("rejects import alias for nested definitions") {
  const std::string source = R"(
import /pkg
namespace pkg {
  namespace util {
    [return<int>]
    inc([i32] value) {
      return(plus(value, 1i32))
    }
  }
}
namespace pkg {
  [return<int>]
  add_one([i32] value) {
    return(plus(value, 1i32))
  }
}
[return<int>]
main() {
  return(add_one(inc(2i32)))
}
)";
  const std::string srcPath = writeTemp("compile_import_immediate_children.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_import_immediate_children_err.txt").string();

  const std::string compileCppCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o /dev/null --entry /main 2> " + quoteShellArg(errPath);
  CHECK(runCommand(compileCppCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: inc") != std::string::npos);

  const std::string runVmCmd =
      "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main 2> " + quoteShellArg(errPath);
  CHECK(runCommand(runVmCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: inc") != std::string::npos);

  const std::string compileNativeCmd =
      "./primec --emit=native " + quoteShellArg(srcPath) + " -o /dev/null --entry /main 2> " + quoteShellArg(errPath);
  CHECK(runCommand(compileNativeCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: inc") != std::string::npos);
}

TEST_CASE("compiles and runs fully-qualified nested call") {
  const std::string source = R"(
namespace pkg {
  namespace util {
    [return<int>]
    inc([i32] value) {
      return(plus(value, 1i32))
    }
  }
}
[return<int>]
main() {
  return(/pkg/util/inc(3i32))
}
)";
  const std::string srcPath = writeTemp("compile_fully_qualified_nested.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_fully_qualified_nested_exe").string();
  const std::string nativePath =
      (std::filesystem::temp_directory_path() / "primec_fully_qualified_nested_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 4);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 4);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 4);
}

TEST_CASE("compiles and runs method call with fully-qualified definition") {
  const std::string source = R"(
[return<int>]
/i32/inc([i32] self, [i32] extra) {
  return(plus(plus(self, extra), 1i32))
}
[return<int>]
main() {
  return(1i32.inc(2i32))
}
)";
  const std::string srcPath = writeTemp("compile_fully_qualified_method_def.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_fully_qualified_method_def_exe").string();
  const std::string nativePath =
      (std::filesystem::temp_directory_path() / "primec_fully_qualified_method_def_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 4);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 4);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 4);
}

TEST_CASE("compiles and runs repeat with bool count") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{0i32}
  repeat(false) {
    assign(value, 9i32)
  }
  repeat(true) {
    assign(value, plus(value, 2i32))
  }
  return(value)
}
)";
  const std::string srcPath = writeTemp("compile_repeat_bool.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_repeat_bool_exe").string();
  const std::string nativePath = (std::filesystem::temp_directory_path() / "primec_repeat_bool_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 2);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 2);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 2);
}

TEST_CASE("compiles and runs pathspace builtins as no-ops") {
  const std::string source = R"(
[return<int> effects(pathspace_notify, pathspace_insert, pathspace_take)]
main() {
  [string] path{"/events/test"utf8}
  [string] storePath{"/store/value"utf8}
  notify(path, 1i32)
  insert(storePath, 2i32)
  take(storePath)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_pathspace_builtins.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_pathspace_builtins_exe").string();
  const std::string nativePath = (std::filesystem::temp_directory_path() / "primec_pathspace_builtins_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 0);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 0);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 0);
}

TEST_CASE("compiles and runs binding without explicit type") {
  const std::string source = R"(
[return<int>]
main() {
  [mut] value{1i32}
  assign(value, plus(value, 2i32))
  return(value)
}
)";
  const std::string srcPath = writeTemp("compile_default_type_binding.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_default_type_binding_exe").string();
  const std::string nativePath = (std::filesystem::temp_directory_path() / "primec_default_type_binding_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 3);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 3);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 3);
}

TEST_CASE("compiles and runs binding inferring i64") {
  const std::string source = R"(
[return<int>]
main() {
  [mut] value{1i64}
  assign(value, plus(value, 2i64))
  return(convert<i32>(value))
}
)";
  const std::string srcPath = writeTemp("compile_infer_i64_binding.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_infer_i64_binding_exe").string();
  const std::string nativePath = (std::filesystem::temp_directory_path() / "primec_infer_i64_binding_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 3);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 3);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 3);
}

TEST_CASE("compiles and runs binding inferring u64") {
  const std::string source = R"(
[return<int>]
main() {
  [mut] value{1u64}
  assign(value, plus(value, 2u64))
  return(convert<i32>(value))
}
)";
  const std::string srcPath = writeTemp("compile_infer_u64_binding.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_infer_u64_binding_exe").string();
  const std::string nativePath = (std::filesystem::temp_directory_path() / "primec_infer_u64_binding_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 3);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 3);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 3);
}

TEST_CASE("compiles and runs binding inferring array type") {
  const std::string source = R"(
[return<int>]
main() {
  [mut] values{array<i32>(1i32, 2i32, 3i32)}
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("compile_infer_array_binding.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_infer_array_binding_exe").string();
  const std::string nativePath =
      (std::filesystem::temp_directory_path() / "primec_infer_array_binding_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 3);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 3);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 3);
}

TEST_CASE("compiles and runs array and map bracket sugar") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] len{count(array<i32>[1i32, 2i32, 3i32])}
  map<i32, i32>[1i32=2i32, 3i32=4i32]
  return(len)
}
)";
  const std::string srcPath = writeTemp("compile_collection_brackets.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_collection_brackets_exe").string();
  const std::string nativePath =
      (std::filesystem::temp_directory_path() / "primec_collection_brackets_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 3);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 3);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 3);
}

TEST_CASE("compiles and runs indexing into array bracket literal") {
  const std::string source = R"(
[return<int>]
main() {
  return(array<i32>[1i32, 5i32, 9i32][2i32])
}
)";
  const std::string srcPath = writeTemp("compile_array_bracket_index.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_array_bracket_index_exe").string();
  const std::string nativePath =
      (std::filesystem::temp_directory_path() / "primec_array_bracket_index_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 9);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 9);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 9);
}

TEST_CASE("compiles and runs binding inferring map type") {
  const std::string source = R"(
[return<int>]
main() {
  [mut] values{map<i32, i32>{1i32=2i32, 3i32=4i32}}
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_infer_map_binding.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_infer_map_binding_exe").string();
  const std::string nativePath = (std::filesystem::temp_directory_path() / "primec_infer_map_binding_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 0);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 0);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 0);
}

TEST_CASE("compiles and runs map count") {
  const std::string source = R"(
[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>{1i32=2i32, 3i32=4i32}}
  return(plus(count(values), values.count()))
}
)";
  const std::string srcPath = writeTemp("compile_map_count.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_map_count_exe").string();
  const std::string nativePath = (std::filesystem::temp_directory_path() / "primec_map_count_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 4);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 4);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 4);
}

TEST_CASE("compiles and runs map indexing") {
  const std::string source = R"(
[return<int>]
main() {
  [mut] values{map<i32, i32>{1i32=10i32, 2i32=20i32}}
  return(values[2i32])
}
)";
  const std::string srcPath = writeTemp("compile_map_indexing.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_map_indexing_exe").string();
  const std::string nativePath = (std::filesystem::temp_directory_path() / "primec_map_indexing_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 20);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 20);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 20);
}

TEST_CASE("compiles and runs map indexing with u64 keys") {
  const std::string source = R"(
[return<int>]
main() {
  return(map<u64, i32>{1u64=7i32, 9u64=1i32}[1u64])
}
)";
  const std::string srcPath = writeTemp("compile_map_u64_indexing.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_map_u64_indexing_exe").string();
  const std::string nativePath = (std::filesystem::temp_directory_path() / "primec_map_u64_indexing_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 7);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 7);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 7);
}

TEST_CASE("compiles and runs string-keyed map indexing in C++ emitter") {
  const std::string source = R"(
[return<int>]
main() {
  return(map<string, i32>{"a"utf8=1i32, "b"utf8=4i32}["b"utf8])
}
)";
  const std::string srcPath = writeTemp("compile_map_string_indexing.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_map_string_indexing_exe").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 4);
}

TEST_CASE("string-keyed map indexing checks missing key in C++ emitter") {
  const std::string source = R"(
[return<int>]
main() {
  return(map<string, i32>{"a"utf8=1i32}["missing"utf8])
}
)";
  const std::string srcPath = writeTemp("compile_map_string_bounds.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_map_string_bounds_exe").string();
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_map_string_bounds_err.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " 2> " + errPath) == 3);
  CHECK(readFile(errPath) == "map key not found\n");
}

TEST_CASE("map indexing checks missing key") {
  const std::string source = R"(
[return<int>]
main() {
  [mut] values{map<i32, i32>{1i32=10i32, 2i32=20i32}}
  return(values[9i32])
}
)";
  const std::string srcPath = writeTemp("compile_map_bounds.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_map_bounds_exe").string();
  const std::string nativePath = (std::filesystem::temp_directory_path() / "primec_map_bounds_native").string();
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_map_bounds_err.txt").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath + " 2> " + errPath) == 3);
  CHECK(readFile(errPath) == "map key not found\n");

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runVmCmd) == 3);
  CHECK(readFile(errPath) == "map key not found\n");

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath + " 2> " + errPath) == 3);
  CHECK(readFile(errPath) == "map key not found\n");
}

TEST_CASE("map indexing rejects mismatched key type in vm/native") {
  const std::string source = R"(
[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>{1i32=10i32}}
  return(values[1u64])
}
)";
  const std::string srcPath = writeTemp("compile_map_key_mismatch.prime", source);
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_map_key_mismatch_err.txt").string();
  const std::string nativePath = (std::filesystem::temp_directory_path() / "primec_map_key_mismatch_native").string();

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runVmCmd) == 2);
  CHECK(readFile(errPath) == "Semantic error: at requires map key type i32\n");

  const std::string compileNativeCmd =
      "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main 2> " + errPath;
  CHECK(runCommand(compileNativeCmd) == 2);
  CHECK(readFile(errPath) == "Semantic error: at requires map key type i32\n");
}

TEST_CASE("compiles and runs binding inference feeding method call") {
  const std::string source = R"(
namespace i64 {
  [return<i64>]
  inc([i64] self) {
    return(plus(self, 1i64))
  }
}

[return<int>]
main() {
  [mut] value{plus(1i64, 2i64)}
  return(convert<i32>(value.inc()))
}
)";
  const std::string srcPath = writeTemp("compile_infer_binding_method.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_infer_binding_method_exe").string();
  const std::string nativePath =
      (std::filesystem::temp_directory_path() / "primec_infer_binding_method_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 4);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 4);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 4);
}

TEST_CASE("compiles and runs binding inference from if expression feeding method call") {
  const std::string source = R"(
namespace i64 {
  [return<i64>]
  inc([i64] self) {
    return(plus(self, 1i64))
  }
}

[return<int>]
main() {
  [mut] value{if(true) { 3i64 } else { 7i64 }}
  return(convert<i32>(value.inc()))
}
)";
  const std::string srcPath = writeTemp("compile_infer_binding_if_method.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_infer_binding_if_method_exe").string();
  const std::string nativePath =
      (std::filesystem::temp_directory_path() / "primec_infer_binding_if_method_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 4);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 4);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 4);
}

TEST_CASE("compiles and runs binding inference from mixed if branches") {
  const std::string source = R"(
namespace i64 {
  [return<i64>]
  inc([i64] self) {
    return(plus(self, 1i64))
  }
}

[return<int>]
main() {
  [mut] value{if(false, 2i32, 5i64)}
  return(convert<i32>(value.inc()))
}
)";
  const std::string srcPath = writeTemp("compile_infer_if_mixed_numeric.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_infer_if_mixed_exe").string();
  const std::string nativePath =
      (std::filesystem::temp_directory_path() / "primec_infer_if_mixed_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 6);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 6);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 6);
}

TEST_CASE("compiles and runs parameter inferring i64 from default initializer") {
  const std::string source = R"(
[return<i64>]
id([mut] value{3i64}) {
  return(value)
}

[return<int>]
main() {
  return(convert<i32>(id()))
}
)";
  const std::string srcPath = writeTemp("compile_infer_i64_param_default.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_infer_i64_param_default_exe").string();
  const std::string nativePath =
      (std::filesystem::temp_directory_path() / "primec_infer_i64_param_default_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 3);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 3);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 3);
}

TEST_CASE("compiles and runs map literal preserving assignment value") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{0i32}
  map<i32, i32>{1i32=value=2i32}
  return(value)
}
)";
  const std::string srcPath = writeTemp("compile_map_value_assign.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_map_value_assign_exe").string();
  const std::string nativePath = (std::filesystem::temp_directory_path() / "primec_map_value_assign_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 2);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 2);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 2);
}

TEST_CASE("C++ emitter array access checks bounds") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(4i32)}
  return(values[9i32])
}
)";
  const std::string srcPath = writeTemp("compile_cpp_array_bounds.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_cpp_array_bounds_exe").string();
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_cpp_array_bounds_err.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " 2> " + errPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(errPath) == "array index out of bounds\n");
}

TEST_CASE("C++ emitter string access checks bounds") {
  const std::string source = R"(
[return<int>]
main() {
  [string] text{"abc"utf8}
  return(at(text, 9i32))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_string_bounds.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_cpp_string_bounds_exe").string();
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_cpp_string_bounds_err.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " 2> " + errPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(errPath) == "string index out of bounds\n");
}

TEST_CASE("runs program in vm") {
  const std::string source = R"(
[return<int>]
main() {
  return(7i32)
}
)";
  const std::string srcPath = writeTemp("vm_simple.prime", source);
  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 7);
}

TEST_CASE("runs vm with string count and indexing") {
  const std::string source = R"(
[return<int>]
main() {
  [string] text{"abc"utf8}
  [i32] a{text[0i32]}
  [i32] b{at_unsafe(text, 1i32)}
  [i32] len{text.count()}
  return(plus(plus(a, b), len))
}
)";
  const std::string srcPath = writeTemp("vm_string_index.prime", source);
  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == (97 + 98 + 3));
}

TEST_CASE("runs vm with string literal count") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] a{count("abc"utf8)}
  [i32] b{count("hi"utf8)}
  return(plus(a, b))
}
)";
  const std::string srcPath = writeTemp("vm_string_literal_count.prime", source);
  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == (3 + 2));
}

TEST_CASE("runs vm with string literal method count") {
  const std::string source = R"(
[return<int>]
main() {
  return("hi"utf8.count())
}
)";
  const std::string srcPath = writeTemp("vm_string_literal_method_count.prime", source);
  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 2);
}

TEST_CASE("runs primevm with argv count") {
  const std::string source = R"(
[return<int>]
main([array<string>] args) {
  return(args.count())
}
)";
  const std::string srcPath = writeTemp("primevm_args_count.prime", source);
  const std::string runVmCmd = "./primevm " + srcPath + " --entry /main -- alpha beta";
  CHECK(runCommand(runVmCmd) == 3);
}

TEST_CASE("vm string access checks bounds") {
  const std::string source = R"(
[return<int>]
main() {
  [string] text{"abc"utf8}
  return(plus(100i32, text[9i32]))
}
)";
  const std::string srcPath = writeTemp("vm_string_bounds.prime", source);
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_vm_string_bounds_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(errPath) == "string index out of bounds\n");
}

TEST_CASE("vm string access rejects negative index") {
  const std::string source = R"(
[return<int>]
main() {
  [string] text{"abc"utf8}
  return(plus(100i32, text[-1i32]))
}
)";
  const std::string srcPath = writeTemp("vm_string_negative.prime", source);
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_vm_string_negative_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(errPath) == "string index out of bounds\n");
}

TEST_CASE("runs vm with argv printing") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  print_line(args[0i32])
  return(args.count())
}
)";
  const std::string srcPath = writeTemp("vm_argv_print.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_vm_argv_print_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 1);
  CHECK(readFile(outPath) == srcPath + "\n");
}

TEST_CASE("runs vm with argv printing without newline") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  print(args[0i32])
  return(args.count())
}
)";
  const std::string srcPath = writeTemp("vm_argv_print_no_newline.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_argv_print_no_newline_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 1);
  CHECK(readFile(outPath) == srcPath);
}

TEST_CASE("runs vm with forwarded argv") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  if(greater_than(args.count(), 1i32)) {
    print_line(args[1i32])
  } else {
    print_line("missing"utf8)
  }
  return(args.count())
}
)";
  const std::string srcPath = writeTemp("vm_forward_argv.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_vm_forward_argv_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main -- alpha beta > " + outPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(outPath) == "alpha\n");
}

TEST_CASE("runs vm with argv count helper") {
  const std::string source = R"(
[return<int>]
main([array<string>] args) {
  return(count(args))
}
)";
  const std::string srcPath = writeTemp("vm_argv_count_helper.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main -- alpha beta";
  CHECK(runCommand(runCmd) == 3);
}

TEST_CASE("runs vm with argv i64 index") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  print_line(args[1i64])
  return(args.count())
}
)";
  const std::string srcPath = writeTemp("vm_argv_i64.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_vm_argv_i64_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main -- alpha beta > " + outPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(outPath) == "alpha\n");
}

TEST_CASE("runs vm with argv u64 index") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  print_line(args[1u64])
  return(args.count())
}
)";
  const std::string srcPath = writeTemp("vm_argv_u64.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_vm_argv_u64_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main -- alpha beta > " + outPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(outPath) == "alpha\n");
}

TEST_CASE("runs vm with argv error output") {
  const std::string source = R"(
[return<int> effects(io_err)]
main([array<string>] args) {
  print_line_error(args[1i32])
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_argv_error.prime", source);
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_vm_argv_error_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main -- alpha beta 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath) == "alpha\n");
}


TEST_CASE("runs vm with argv error output without newline") {
  const std::string source = R"(
[return<int> effects(io_err)]
main([array<string>] args) {
  print_error(args[1i32])
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_argv_error_no_newline.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_argv_error_no_newline_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main -- alpha beta 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath) == "alpha");
}

TEST_CASE("runs vm with argv error output u64 index") {
  const std::string source = R"(
[return<int> effects(io_err)]
main([array<string>] args) {
  print_error(args[1u64])
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_argv_error_u64.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_argv_error_u64_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main -- alpha beta 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath) == "alpha");
}

TEST_CASE("runs vm with argv line error output u64 index") {
  const std::string source = R"(
[return<int> effects(io_err)]
main([array<string>] args) {
  print_line_error(args[1u64])
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_argv_line_error_u64.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_argv_line_error_u64_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main -- alpha beta 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath) == "alpha\n");
}

TEST_CASE("runs vm with argv unsafe error output") {
  const std::string source = R"(
[return<int> effects(io_err)]
main([array<string>] args) {
  print_error(at_unsafe(args, 1i32))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_argv_error_unsafe.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_argv_error_unsafe_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main -- alpha beta 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath) == "alpha");
}

TEST_CASE("runs vm with argv unsafe line error output") {
  const std::string source = R"(
[return<int> effects(io_err)]
main([array<string>] args) {
  print_line_error(at_unsafe(args, 1i32))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_argv_line_error_unsafe.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_argv_line_error_unsafe_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main -- alpha beta 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath) == "alpha\n");
}

TEST_SUITE_END();
