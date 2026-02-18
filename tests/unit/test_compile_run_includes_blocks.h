TEST_CASE("compiles and runs include inside namespace") {
  const std::string libPath =
      writeTemp("compile_namespace_lib.prime", "[return<int>]\nhelper(){ return(9i32) }\n");
  const std::string source =
      "namespace outer {\n"
      "  include<\"" + libPath + "\">\n"
      "}\n"
      "[return<int>]\n"
      "main(){ return(/outer/helper()) }\n";
  const std::string srcPath = writeTemp("compile_namespace_include.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_namespace_inc_exe").string();
  const std::string nativePath =
      (std::filesystem::temp_directory_path() / "primec_namespace_inc_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 9);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 9);

  const std::string compileNativeCmd =
      "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 9);
}

TEST_CASE("compiles and runs include with import aliases") {
  const std::string libSource = R"(
namespace lib {
  [return<int>]
  add([i32] a, [i32] b) {
    return(plus(a, b))
  }
}
)";
  const std::string libPath = writeTemp("compile_lib_imports.prime", libSource);
  const std::string source =
      "include<\"" + libPath + "\">\n"
      "import /lib\n"
      "[return<int>]\n"
      "main(){ return(add(4i32, 3i32)) }\n";
  const std::string srcPath = writeTemp("compile_include_imports.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_inc_imports_exe").string();
  const std::string nativePath = (std::filesystem::temp_directory_path() / "primec_inc_imports_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 7);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 7);

  const std::string compileNativeCmd =
      "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 7);
}

TEST_CASE("compiles and runs block expression with multiline body") {
  const std::string source = R"(
[return<int>]
main() {
  return(block(){
    [i32] x{1i32}
    plus(x, 2i32)
  })
}
)";
  const std::string srcPath = writeTemp("compile_block_expr_multiline.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_block_expr_multiline_exe").string();
  const std::string nativePath =
      (std::filesystem::temp_directory_path() / "primec_block_expr_multiline_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 3);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 3);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 3);
}

TEST_CASE("compiles and runs block expression return value") {
  const std::string source = R"(
[return<int>]
main() {
  return(block(){
    [i32] x{3i32}
    return(plus(x, 4i32))
  })
}
)";
  const std::string srcPath = writeTemp("compile_block_expr_return.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_block_expr_return_exe").string();
  const std::string nativePath = (std::filesystem::temp_directory_path() / "primec_block_expr_return_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 7);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 7);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 7);
}

TEST_CASE("compiles and runs block expression early return") {
  const std::string source = R"(
[return<int>]
main() {
  return(block(){
    return(3i32)
    4i32
  })
}
)";
  const std::string srcPath = writeTemp("compile_block_expr_early_return.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_block_expr_early_return_exe").string();
  const std::string nativePath =
      (std::filesystem::temp_directory_path() / "primec_block_expr_early_return_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 3);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 3);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 3);
}

TEST_CASE("compiles and runs block binding inference for method calls") {
  const std::string source = R"(
namespace i64 {
  [return<i64>]
  inc([i64] self) {
    return(plus(self, 1i64))
  }
}

[return<int>]
main() {
  return(convert<i32>(block(){
    [mut] value{plus(1i64, 2i64)}
    value.inc()
  }))
}
)";
  const std::string srcPath = writeTemp("compile_block_binding_infer_method.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_block_binding_infer_method_exe").string();
  const std::string nativePath =
      (std::filesystem::temp_directory_path() / "primec_block_binding_infer_method_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 4);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 4);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 4);
}

TEST_CASE("compiles and runs block binding inference for mixed if numeric types") {
  const std::string source = R"(
[return<int> effects(io_out)]
main() {
  print_line(block(){
    [mut] value{if(false, then(){ 1i32 }, else(){ 5000000000i64 })}
    value
  })
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_block_infer_if_numeric.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_block_infer_if_numeric_exe").string();
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_block_infer_if_numeric_out.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 0);
  CHECK(readFile(outPath) == "5000000000\n");
}

TEST_CASE("compiles and runs operator rewrite") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32+2i32)
}
)";
  const std::string srcPath = writeTemp("compile_ops.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_ops_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("compiles and runs operator rewrite with calls") {
  const std::string source = R"(
[return<int>]
helper() {
  return(5i32)
}

[return<int>]
main() {
  return(helper()+2i32)
}
)";
  const std::string srcPath = writeTemp("compile_ops_call.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_ops_call_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs operator rewrite with parentheses") {
  const std::string source = R"(
[return<int>]
main() {
  return((1i32+2i32)*3i32)
}
)";
  const std::string srcPath = writeTemp("compile_ops_paren.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_ops_paren_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("compiles and runs operator rewrite with unary minus operand") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{2i32}
  return(3i32+-value)
}
)";
  const std::string srcPath = writeTemp("compile_ops_unary_minus.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_ops_unary_minus_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}


TEST_CASE("rejects negate on u64") {
  const std::string source = R"(
[return<u64>]
main() {
  return(negate(2u64))
}
)";
  const std::string srcPath = writeTemp("compile_negate_u64.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_negate_u64_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("compiles and runs short-circuit and") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{1i32}
  [bool mut] witness{false}
  and(equal(value, 0i32), assign(witness, true))
  return(convert<int>(witness))
}
)";
  const std::string srcPath = writeTemp("compile_and_short.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_and_short_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("compiles and runs short-circuit or") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{1i32}
  [bool mut] witness{false}
  or(equal(value, 1i32), assign(witness, true))
  return(convert<int>(witness))
}
)";
  const std::string srcPath = writeTemp("compile_or_short.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_or_short_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("compiles and runs numeric boolean ops") {
  const std::string source = R"(
[return<bool>]
main() {
  return(or(convert<bool>(0i32), convert<bool>(2i32)))
}
)";
  const std::string srcPath = writeTemp("compile_bool_numeric.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_bool_numeric_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs convert<bool>") {
  const std::string source = R"(
[return<bool>]
main() {
  return(convert<bool>(0i32))
}
)";
  const std::string srcPath = writeTemp("compile_convert_bool.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_convert_bool_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("compiles and runs convert<i64>") {
  const std::string source = R"(
[return<i64>]
main() {
  return(convert<i64>(9i64))
}
)";
  const std::string srcPath = writeTemp("compile_convert_i64.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_convert_i64_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("compiles and runs convert<u64>") {
  const std::string source = R"(
[return<u64>]
main() {
  return(convert<u64>(10u64))
}
)";
  const std::string srcPath = writeTemp("compile_convert_u64.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_convert_u64_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 10);
}

TEST_CASE("compiles and runs convert<bool> from u64") {
  const std::string source = R"(
[return<bool>]
main() {
  return(convert<bool>(1u64))
}
)";
  const std::string srcPath = writeTemp("compile_convert_bool_u64.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_convert_bool_u64_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs convert<bool> from negative i64") {
  const std::string source = R"(
[return<bool>]
main() {
  return(convert<bool>(-1i64))
}
)";
  const std::string srcPath = writeTemp("compile_convert_bool_i64_neg.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_convert_bool_i64_neg_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs pointer helpers") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{4i32}
  return(dereference(location(value)))
}
)";
  const std::string srcPath = writeTemp("compile_pointer_helpers.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_pointer_helpers_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 4);
}

TEST_CASE("compiles and runs pointer plus helper") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{5i32}
  return(dereference(plus(location(value), 0i32)))
}
)";
  const std::string srcPath = writeTemp("compile_pointer_plus.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_pointer_plus_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 5);
}

TEST_CASE("compiles and runs pointer plus on reference") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{8i32}
  [Reference<i32>] ref{location(value)}
  return(dereference(plus(location(ref), 0i32)))
}
)";
  const std::string srcPath = writeTemp("compile_pointer_plus_ref.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_pointer_plus_ref_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 8);
}

TEST_CASE("compiles and runs pointer minus helper") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] first{4i32}
  [i32] second{9i32}
  return(dereference(minus(location(second), 0i32)))
}
)";
  const std::string srcPath = writeTemp("compile_pointer_minus.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_pointer_minus_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("compiles and runs pointer minus u64 offset") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{5i32}
  return(dereference(minus(location(value), 0u64)))
}
)";
  const std::string srcPath = writeTemp("compile_pointer_minus_u64.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_pointer_minus_u64_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 5);
}
