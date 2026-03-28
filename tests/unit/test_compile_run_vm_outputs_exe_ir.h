#pragma once

TEST_CASE("compiles and runs void main with local binding") {
  const std::string source = R"(
[return<void>]
main() {
  [i32] value{1i32}
}
)";
  const std::string srcPath = writeTemp("compile_void_main.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_void_main_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("exe-ir emitter compiles and runs i32 subset") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] counter{1i32}
  assign(counter, plus(counter, 2i32))
  return(counter)
}
)";
  const std::string srcPath = writeTemp("compile_exe_ir_i32_subset.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_ir_i32_subset").string();

  const std::string compileCmd = "./primec --emit=exe-ir " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("exe-ir emitter compiles and runs i64 subset") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<i64>]
main() {
  [i64 mut] counter{10i64}
  assign(counter, plus(counter, 5i64))
  assign(counter, minus(counter, 2i64))
  return(counter)
}
)";
  const std::string srcPath = writeTemp("compile_exe_ir_i64_subset.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_ir_i64_subset").string();

  const std::string compileCmd = "./primec --emit=exe-ir " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 13);
}

TEST_CASE("exe-ir emitter compiles and runs argv prints") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  print_line(args[1i32])
  return(args.count())
}
)";
  const std::string srcPath = writeTemp("compile_exe_ir_print_argv.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_ir_print_argv").string();
  const std::string outPath = (testScratchPath("") / "primec_exe_ir_print_argv.out").string();

  const std::string compileCmd = "./primec --emit=exe-ir " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " alpha beta > " + outPath) == 3);
  CHECK(readFile(outPath) == "alpha\n");
}

TEST_CASE("exe-ir emitter compiles and runs dynamic string print") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int> effects(io_out)]
main() {
  [string mut] msg{"left"utf8}
  assign(msg, "right"utf8)
  print_line(msg)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_exe_ir_dynamic_string_print.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_ir_dynamic_string_print").string();
  const std::string outPath = (testScratchPath("") / "primec_exe_ir_dynamic_string_print.out").string();

  const std::string compileCmd = "./primec --emit=exe-ir " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 0);
  CHECK(readFile(outPath) == "left\n");
}

TEST_CASE("exe-ir emitter compiles and runs string indexing") {
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
  const std::string srcPath = writeTemp("compile_exe_ir_string_indexing.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_ir_string_indexing").string();

  const std::string compileCmd = "./primec --emit=exe-ir " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == (97 + 98 + 3));
}

TEST_CASE("exe-ir emitter compiles and runs pointer indirect paths") {
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
  const std::string srcPath = writeTemp("compile_exe_ir_pointer_indirect.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_ir_pointer_indirect").string();

  const std::string compileCmd = "./primec --emit=exe-ir " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 8);
}

TEST_CASE("exe-ir emitter compiles and runs heap alloc intrinsic") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int> effects(heap_alloc)]
main() {
  [mut] ptr{/std/intrinsics/memory/alloc<i32>(1i32)}
  assign(dereference(ptr), 9i32)
  return(dereference(ptr))
}
)";
  const std::string srcPath = writeTemp("compile_exe_ir_heap_alloc_intrinsic.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_ir_heap_alloc_intrinsic").string();

  const std::string compileCmd = "./primec --emit=exe-ir " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("exe-ir emitter compiles and runs heap free intrinsic") {
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
  const std::string srcPath = writeTemp("compile_exe_ir_heap_free_intrinsic.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_ir_heap_free_intrinsic").string();

  const std::string compileCmd = "./primec --emit=exe-ir " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("exe-ir emitter compiles and runs heap realloc intrinsic") {
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
  const std::string srcPath = writeTemp("compile_exe_ir_heap_realloc_intrinsic.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_ir_heap_realloc_intrinsic").string();

  const std::string compileCmd = "./primec --emit=exe-ir " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 13);
}

TEST_CASE("exe-ir emitter compiles and runs checked memory at intrinsic") {
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
  const std::string srcPath = writeTemp("compile_exe_ir_heap_at_intrinsic.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_ir_heap_at_intrinsic").string();

  const std::string compileCmd = "./primec --emit=exe-ir " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 13);
}

TEST_CASE("exe-ir emitter compiles and runs unchecked memory at intrinsic") {
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
  const std::string srcPath = writeTemp("compile_exe_ir_heap_at_unsafe_intrinsic.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_ir_heap_at_unsafe_intrinsic").string();

  const std::string compileCmd = "./primec --emit=exe-ir " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 13);
}

TEST_CASE("exe-ir emitter faults on checked memory at out of bounds") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int> effects(heap_alloc)]
main() {
  [mut] ptr{/std/intrinsics/memory/alloc<i32>(1i32)}
  return(dereference(/std/intrinsics/memory/at(ptr, 1i32, 1i32)))
}
)";
  const std::string srcPath = writeTemp("compile_exe_ir_heap_at_out_of_bounds.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_ir_heap_at_out_of_bounds").string();
  const std::string errPath =
      (testScratchPath("") / "primec_exe_ir_heap_at_out_of_bounds.txt").string();

  const std::string compileCmd = "./primec --emit=exe-ir " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " 2> " + errPath) != 0);
  CHECK(readFile(errPath) == "pointer index out of bounds\n");
}

TEST_CASE("exe-ir emitter faults on dereference after heap free intrinsic") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int> effects(heap_alloc)]
main() {
  [mut] ptr{/std/intrinsics/memory/alloc<i32>(1i32)}
  /std/intrinsics/memory/free(ptr)
  return(dereference(ptr))
}
)";
  const std::string srcPath = writeTemp("compile_exe_ir_heap_free_invalid_deref.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_exe_ir_heap_free_invalid_deref").string();
  const std::string errPath =
      (testScratchPath("") / "primec_exe_ir_heap_free_invalid_deref.txt").string();

  const std::string compileCmd = "./primec --emit=exe-ir " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " 2> " + errPath) != 0);
  CHECK(readFile(errPath).find("invalid indirect address in IR") != std::string::npos);
}

TEST_CASE("exe-ir emitter compiles and runs file io subset") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string outPath = (testScratchPath("") / "primec_exe_ir_file_io_subset.txt").string();
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
  const std::string srcPath = writeTemp("compile_exe_ir_file_io_subset.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_ir_file_io_subset").string();

  const std::string compileCmd = "./primec --emit=exe-ir " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
  CHECK(readFile(outPath) == "Hello 123 world\n\nABC");
}

TEST_CASE("exe-ir emitter reports misaligned pointer dereference") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{5i32}
  return(dereference(plus(location(value), 8i32)))
}
)";
  const std::string srcPath = writeTemp("compile_exe_ir_pointer_misaligned.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_ir_pointer_misaligned").string();
  const std::string errPath =
      (testScratchPath("") / "primec_exe_ir_pointer_misaligned.err").string();

  const std::string compileCmd = "./primec --emit=exe-ir " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " 2> " + errPath) == 1);
  CHECK(readFile(errPath).find("unaligned indirect address in IR") != std::string::npos);
}

TEST_CASE("exe-ir emitter compiles and runs call and callvoid paths") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<void> effects(io_out)]
logCall() {
  print_line("log"utf8)
}

[return<int>]
value() {
  return(41i32)
}

[return<int> effects(io_out)]
main() {
  logCall()
  return(plus(value(), 1i32))
}
)";
  const std::string srcPath = writeTemp("compile_exe_ir_calls.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_ir_calls").string();
  const std::string outPath = (testScratchPath("") / "primec_exe_ir_calls.out").string();

  const std::string compileCmd = "./primec --emit=exe-ir " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 42);
  CHECK(readFile(outPath) == "log\n");
}

TEST_CASE("exe-ir emitter compiles and runs f32 arithmetic subset") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int>]
main() {
  [f32 mut] value{2.0f32}
  assign(value, plus(value, 0.5f32))
  if(greater_than(value, 2.4f32), then() { return(7i32) }, else() { return(3i32) })
}
)";
  const std::string srcPath = writeTemp("compile_exe_ir_f32_subset.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_ir_f32_subset").string();

  const std::string compileCmd = "./primec --emit=exe-ir " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

