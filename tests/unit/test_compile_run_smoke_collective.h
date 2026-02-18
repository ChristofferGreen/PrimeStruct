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

TEST_CASE("compiles and runs repeat with non-positive count") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{5i32}
  repeat(0i32) {
    assign(value, 9i32)
  }
  repeat(-2i32) {
    assign(value, 11i32)
  }
  return(value)
}
)";
  const std::string srcPath = writeTemp("compile_repeat_non_positive.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_repeat_non_positive_exe").string();
  const std::string nativePath =
      (std::filesystem::temp_directory_path() / "primec_repeat_non_positive_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 5);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 5);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 5);
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

