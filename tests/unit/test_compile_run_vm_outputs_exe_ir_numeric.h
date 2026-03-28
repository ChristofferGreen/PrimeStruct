#pragma once

TEST_CASE("exe-ir emitter compiles and runs f64 comparison subset") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int>]
main() {
  if(greater_than(2.5f64, 1.0f64), then() { return(7i32) }, else() { return(3i32) })
}
)";
  const std::string srcPath = writeTemp("compile_exe_ir_f64_cmp_subset.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_ir_f64_cmp_subset").string();

  const std::string compileCmd = "./primec --emit=exe-ir " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("exe emitter uses ir backend for f32 arithmetic subset") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int>]
main() {
  [f32 mut] value{2.0f32}
  assign(value, plus(value, 0.5f32))
  if(greater_than(value, 2.4f32), then() { return(7i32) }, else() { return(3i32) })
}
)";
  const std::string srcPath = writeTemp("compile_exe_f32_ir_first.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_f32_ir_first").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("exe emitter uses ir backend for string indexing") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
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
  const std::string srcPath = writeTemp("compile_exe_string_indexing_ir_first.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_string_indexing_ir_first").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == (97 + 98 + 3));
}

TEST_CASE("exe emitter uses ir backend for pointer indirect paths") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{3i32}
  [Reference<i32> mut] ref{location(value)}
  assign(ref, 8i32)
  return(ref)
}
)";
  const std::string srcPath = writeTemp("compile_exe_pointer_indirect_ir_first.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_pointer_indirect_ir_first").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 8);
}

TEST_CASE("exe emitter uses ir backend for heap alloc intrinsic") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int> effects(heap_alloc)]
main() {
  [mut] ptr{/std/intrinsics/memory/alloc<i32>(1i32)}
  assign(dereference(ptr), 9i32)
  return(dereference(ptr))
}
)";
  const std::string srcPath = writeTemp("compile_exe_heap_alloc_intrinsic_ir_first.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_heap_alloc_intrinsic_ir_first").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("exe emitter uses ir backend for heap free intrinsic") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int> effects(heap_alloc)]
main() {
  [mut] ptr{/std/intrinsics/memory/alloc<i32>(1i32)}
  assign(dereference(ptr), 9i32)
  [i32] value{dereference(ptr)}
  /std/intrinsics/memory/free(ptr)
  return(value)
}
)";
  const std::string srcPath = writeTemp("compile_exe_heap_free_intrinsic_ir_first.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_heap_free_intrinsic_ir_first").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("exe emitter uses ir backend for heap realloc intrinsic") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int> effects(heap_alloc)]
main() {
  [mut] ptr{/std/intrinsics/memory/alloc<i32>(1i32)}
  assign(dereference(ptr), 9i32)
  [Pointer<i32> mut] grown{/std/intrinsics/memory/realloc(ptr, 2i32)}
  assign(dereference(plus(grown, 16i32)), 4i32)
  [i32] sum{plus(dereference(grown), dereference(plus(grown, 16i32)))}
  /std/intrinsics/memory/free(grown)
  return(sum)
}
)";
  const std::string srcPath = writeTemp("compile_exe_heap_realloc_intrinsic_ir_first.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_heap_realloc_intrinsic_ir_first").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 13);
}

TEST_CASE("exe emitter uses ir backend for checked memory at intrinsic") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int> effects(heap_alloc)]
main() {
  [mut] ptr{/std/intrinsics/memory/alloc<i32>(2i32)}
  assign(dereference(ptr), 9i32)
  [mut] second{/std/intrinsics/memory/at(ptr, 1i32, 2i32)}
  assign(dereference(second), 4i32)
  [i32] sum{plus(dereference(ptr), dereference(second))}
  /std/intrinsics/memory/free(ptr)
  return(sum)
}
)";
  const std::string srcPath = writeTemp("compile_exe_heap_at_intrinsic_ir_first.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_heap_at_intrinsic_ir_first").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 13);
}

TEST_CASE("exe emitter uses ir backend for unchecked memory at intrinsic") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int> effects(heap_alloc)]
main() {
  [mut] ptr{/std/intrinsics/memory/alloc<i32>(2i32)}
  assign(dereference(ptr), 9i32)
  [mut] second{/std/intrinsics/memory/at_unsafe(ptr, 1i32)}
  assign(dereference(second), 4i32)
  [i32] sum{plus(dereference(ptr), dereference(second))}
  /std/intrinsics/memory/free(ptr)
  return(sum)
}
)";
  const std::string srcPath = writeTemp("compile_exe_heap_at_unsafe_intrinsic_ir_first.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_heap_at_unsafe_intrinsic_ir_first").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 13);
}

TEST_CASE("exe emitter uses ir backend for file io subset") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string outPath = (testScratchPath("") / "primec_exe_file_io_ir_first.txt").string();
  auto escape = [](const std::string &text) {
    std::string out;
    out.reserve(text.size());
    for (char c : text) {
      if (c == '\\' || c == '"') {
        out.push_back('\\');
      }
      out.push_back(c);
    }
    return out;
  };
  const std::string escapedPath = escape(outPath);
  const std::string source =
      "[return<Result<FileError>> effects(file_write) on_error<FileError, /log_file_error>]\n"
      "main() {\n"
      "  [File<Write>] file{ File<Write>(\"" + escapedPath + "\"utf8)? }\n"
      "  [array<i32>] bytes{ array<i32>(65i32, 66i32, 67i32) }\n"
      "  file.write(\"Hello \"utf8, 123i32, \" world\"utf8)?\n"
      "  file.write_line(\"\"utf8)?\n"
      "  file.write_byte(10i32)?\n"
      "  file.write_bytes(bytes)?\n"
      "  file.flush()?\n"
      "  file.close()?\n"
      "  return(Result.ok())\n"
      "}\n"
      "[effects(io_err)]\n"
      "log_file_error([FileError] err) {\n"
      "  print_line_error(\"file error\"utf8)\n"
      "}\n";
  const std::string srcPath = writeTemp("compile_exe_file_io_ir_first.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_file_io_ir_first").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
  CHECK(readFile(outPath) == "Hello 123 world\n\nABC");
}

TEST_CASE("exe-ir emitter compiles and runs f64 arithmetic subset") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int>]
main() {
  [f64 mut] value{2.0f64}
  assign(value, plus(value, 0.5f64))
  if(greater_than(value, 2.4f64), then() { return(7i32) }, else() { return(3i32) })
}
)";
  const std::string srcPath = writeTemp("compile_exe_ir_f64_math_subset.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_ir_f64_math_subset").string();

  const std::string compileCmd = "./primec --emit=exe-ir " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("exe emitter uses ir backend for f64 comparison subset") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int>]
main() {
  if(greater_than(2.5f64, 1.0f64), then() { return(7i32) }, else() { return(3i32) })
}
)";
  const std::string srcPath = writeTemp("compile_exe_f64_cmp_ir_first.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_f64_cmp_ir_first").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("exe emitter uses ir backend for f64 arithmetic subset") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int>]
main() {
  [f64 mut] value{2.0f64}
  assign(value, plus(value, 0.5f64))
  if(greater_than(value, 2.4f64), then() { return(7i32) }, else() { return(3i32) })
}
)";
  const std::string srcPath = writeTemp("compile_exe_f64_math_ir_first.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_f64_math_ir_first").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("exe-ir emitter compiles and runs f64 conversion subset") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<i64>]
main() {
  [i32] base{7i32}
  [f64] widened{convert<f64>(base)}
  [f32] narrowed{convert<f32>(widened)}
  [f64] roundTrip{convert<f64>(narrowed)}
  return(convert<i64>(roundTrip))
}
)";
  const std::string srcPath = writeTemp("compile_exe_ir_f64_convert_subset.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_ir_f64_convert_subset").string();

  const std::string compileCmd = "./primec --emit=exe-ir " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("exe emitter uses ir backend for f64 conversion subset") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<i64>]
main() {
  [i32] base{7i32}
  [f64] widened{convert<f64>(base)}
  [f32] narrowed{convert<f32>(widened)}
  [f64] roundTrip{convert<f64>(narrowed)}
  return(convert<i64>(roundTrip))
}
)";
  const std::string srcPath = writeTemp("compile_exe_f64_convert_ir_first.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_f64_convert_ir_first").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("exe-ir emitter compiles and runs f64 to i32 conversion") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int>]
main() {
  return(convert<int>(2.5f64))
}
)";
  const std::string srcPath = writeTemp("compile_exe_ir_f64_to_i32_convert.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_ir_f64_to_i32_convert").string();

  const std::string compileCmd = "./primec --emit=exe-ir " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("exe emitter uses ir backend for f64 to i32 conversion") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int>]
main() {
  return(convert<int>(2.5f64))
}
)";
  const std::string srcPath = writeTemp("compile_exe_f64_to_i32_ir_first.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_f64_to_i32_ir_first").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("exe-ir emitter clamps f32/f64 to i64 conversion edges") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int>]
main() {
  [i64] minValue{plus(-9223372036854775807i64, -1i64)}
  if(not(equal(convert<i64>(divide(0.0f32, 0.0f32)), 0i64)), then() { return(1i32) }, else() { })
  if(not(equal(convert<i64>(divide(1.0f32, 0.0f32)), 9223372036854775807i64)), then() { return(2i32) }, else() { })
  if(not(equal(convert<i64>(divide(-1.0f32, 0.0f32)), minValue)), then() { return(3i32) }, else() { })
  if(not(equal(convert<i64>(divide(0.0f64, 0.0f64)), 0i64)), then() { return(4i32) }, else() { })
  if(not(equal(convert<i64>(divide(1.0f64, 0.0f64)), 9223372036854775807i64)), then() { return(5i32) }, else() { })
  if(not(equal(convert<i64>(divide(-1.0f64, 0.0f64)), minValue)), then() { return(6i32) }, else() { })
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_exe_ir_f64_to_i64_convert_edges.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_exe_ir_f64_to_i64_convert_edges").string();

  const std::string compileCmd = "./primec --emit=exe-ir " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("exe emitter uses ir backend for f32/f64 to i64 conversion edges") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int>]
main() {
  [i64] minValue{plus(-9223372036854775807i64, -1i64)}
  if(not(equal(convert<i64>(divide(0.0f32, 0.0f32)), 0i64)), then() { return(1i32) }, else() { })
  if(not(equal(convert<i64>(divide(1.0f32, 0.0f32)), 9223372036854775807i64)), then() { return(2i32) }, else() { })
  if(not(equal(convert<i64>(divide(-1.0f32, 0.0f32)), minValue)), then() { return(3i32) }, else() { })
  if(not(equal(convert<i64>(divide(0.0f64, 0.0f64)), 0i64)), then() { return(4i32) }, else() { })
  if(not(equal(convert<i64>(divide(1.0f64, 0.0f64)), 9223372036854775807i64)), then() { return(5i32) }, else() { })
  if(not(equal(convert<i64>(divide(-1.0f64, 0.0f64)), minValue)), then() { return(6i32) }, else() { })
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_exe_f64_to_i64_ir_first_edges.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_exe_f64_to_i64_ir_first_edges").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("exe-ir emitter truncates in-range f32/f64 to i64") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int>]
main() {
  if(not(equal(convert<i64>(2.9f32), 2i64)), then() { return(1i32) }, else() { })
  if(not(equal(convert<i64>(-2.9f32), -2i64)), then() { return(2i32) }, else() { })
  if(not(equal(convert<i64>(2.9f64), 2i64)), then() { return(3i32) }, else() { })
  if(not(equal(convert<i64>(-2.9f64), -2i64)), then() { return(4i32) }, else() { })
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_exe_ir_f64_to_i64_convert_truncation.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_exe_ir_f64_to_i64_convert_truncation").string();

  const std::string compileCmd = "./primec --emit=exe-ir " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("exe emitter uses ir backend for in-range f32/f64 to i64 truncation") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int>]
main() {
  if(not(equal(convert<i64>(2.9f32), 2i64)), then() { return(1i32) }, else() { })
  if(not(equal(convert<i64>(-2.9f32), -2i64)), then() { return(2i32) }, else() { })
  if(not(equal(convert<i64>(2.9f64), 2i64)), then() { return(3i32) }, else() { })
  if(not(equal(convert<i64>(-2.9f64), -2i64)), then() { return(4i32) }, else() { })
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_exe_f64_to_i64_ir_first_truncation.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_exe_f64_to_i64_ir_first_truncation").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("exe-ir emitter truncates in-range f32/f64 to u64") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int>]
main() {
  if(not(equal(convert<u64>(2.9f32), 2u64)), then() { return(1i32) }, else() { })
  if(not(equal(convert<u64>(42.9f32), 42u64)), then() { return(2i32) }, else() { })
  if(not(equal(convert<u64>(2.9f64), 2u64)), then() { return(3i32) }, else() { })
  if(not(equal(convert<u64>(42.9f64), 42u64)), then() { return(4i32) }, else() { })
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_exe_ir_f64_to_u64_convert_truncation.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_exe_ir_f64_to_u64_convert_truncation").string();

  const std::string compileCmd = "./primec --emit=exe-ir " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("exe emitter uses ir backend for in-range f32/f64 to u64 truncation") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int>]
main() {
  if(not(equal(convert<u64>(2.9f32), 2u64)), then() { return(1i32) }, else() { })
  if(not(equal(convert<u64>(42.9f32), 42u64)), then() { return(2i32) }, else() { })
  if(not(equal(convert<u64>(2.9f64), 2u64)), then() { return(3i32) }, else() { })
  if(not(equal(convert<u64>(42.9f64), 42u64)), then() { return(4i32) }, else() { })
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_exe_f64_to_u64_ir_first_truncation.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_exe_f64_to_u64_ir_first_truncation").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

