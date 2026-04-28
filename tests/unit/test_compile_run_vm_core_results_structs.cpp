#include "test_compile_run_helpers.h"

#include <cerrno>

TEST_SUITE_BEGIN("primestruct.compile.run.vm.core");


TEST_CASE("vm supports single-slot struct Result combinator payloads on IR-backed paths") {
  const std::string source = R"(
import /std/file/*

[struct]
Label() {
  [i32] code{0i32}
}

[return<Result<Label, FileError>>]
make_label([i32] code) {
  return(Result.ok(Label{[code] code}))
}

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(err.why())
}

[return<int> effects(io_out, io_err) on_error<FileError, /log_file_error>]
main() {
  [Label] mapped{try(Result.map(make_label(2i32), []([Label] value) {
    return(Label{[code] plus(value.code, 5i32)})
  }))}
  print_line(mapped.code)
  [Label] chained{try(Result.and_then(make_label(2i32), []([Label] value) {
    return(Result.ok(Label{[code] plus(value.code, 3i32)}))
  }))}
  print_line(chained.code)
  [Label] summed{try(Result.map2(make_label(2i32), make_label(5i32), []([Label] left, [Label] right) {
    return(Label{[code] plus(left.code, right.code)})
  }))}
  print_line(summed.code)
  return(19i32)
}
)";
  const std::string srcPath = writeTemp("vm_result_single_slot_struct_combinators_ir_backed.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_result_single_slot_struct_combinators_ir_backed_out.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 19);
  CHECK(readFile(outPath) == "7\n5\n7\n");
}

TEST_CASE("vm supports direct File Result payloads on IR-backed paths") {
  const std::string filePath =
      (std::filesystem::temp_directory_path() / "primec_vm_result_file_payload_ir_backed.txt").string();
  {
    std::ofstream file(filePath, std::ios::binary);
    REQUIRE(file.good());
    file.write("ABC", 3);
    REQUIRE(file.good());
  }
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
  const std::string escapedPath = escape(filePath);
  const std::string source =
      "import /std/file/*\n"
      "[effects(io_err)]\n"
      "log_file_error([FileError] err) {\n"
      "  print_line_error(err.why())\n"
      "}\n"
      "[return<int> effects(file_read, io_out, io_err) on_error<FileError, /log_file_error>]\n"
      "main() {\n"
      "  [File<Read>] file{File<Read>(\"" + escapedPath + "\"utf8)?}\n"
      "  [Result<File<Read>, FileError>] wrapped{Result.ok(file)}\n"
      "  [File<Read>] reopened{try(wrapped)}\n"
      "  [i32 mut] first{0i32}\n"
      "  reopened.read_byte(first)?\n"
      "  print_line(first)\n"
      "  reopened.close()?\n"
      "  return(first)\n"
      "}\n";
  const std::string srcPath = writeTemp("vm_result_file_payload_ir_backed.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_result_file_payload_ir_backed_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 65);
  CHECK(readFile(outPath) == "65\n");
}

TEST_CASE("vm supports packed File Result combinator payloads on IR-backed paths") {
  const std::string filePath =
      (std::filesystem::temp_directory_path() / "primec_vm_result_file_combinators_ir_backed.txt").string();
  {
    std::ofstream file(filePath, std::ios::binary);
    REQUIRE(file.good());
    file.write("ABC", 3);
    REQUIRE(file.good());
  }
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
  const std::string escapedPath = escape(filePath);
  const std::string source =
      "import /std/file/*\n"
      "[effects(io_err)]\n"
      "log_file_error([FileError] err) {\n"
      "  print_line_error(err.why())\n"
      "}\n"
      "[return<int> effects(file_read, io_out, io_err) on_error<FileError, /log_file_error>]\n"
      "main() {\n"
      "  [File<Read>] openedA{File<Read>(\"" + escapedPath + "\"utf8)?}\n"
      "  [File<Read>] mapped{try(Result.map(Result.ok(openedA), []([File<Read>] file) { return(file) }))}\n"
      "  [i32 mut] first{0i32}\n"
      "  mapped.read_byte(first)?\n"
      "  mapped.close()?\n"
      "  print_line(first)\n"
      "  [File<Read>] openedB{File<Read>(\"" + escapedPath + "\"utf8)?}\n"
      "  [File<Read>] chained{try(Result.and_then(Result.ok(openedB), []([File<Read>] file) { return(Result.ok(file)) }))}\n"
      "  [i32 mut] second{0i32}\n"
      "  chained.read_byte(second)?\n"
      "  chained.close()?\n"
      "  print_line(second)\n"
      "  [File<Read>] openedC{File<Read>(\"" + escapedPath + "\"utf8)?}\n"
      "  [File<Read>] openedD{File<Read>(\"" + escapedPath + "\"utf8)?}\n"
      "  [File<Read>] combined{\n"
      "    try(Result.map2(Result.ok(openedC), Result.ok(openedD), []([File<Read>] left, [File<Read>] right) {\n"
      "      return(left)\n"
      "    }))\n"
      "  }\n"
      "  [i32 mut] third{0i32}\n"
      "  combined.read_byte(third)?\n"
      "  combined.close()?\n"
      "  print_line(third)\n"
      "  return(plus(first, plus(second, third)))\n"
      "}\n";
  const std::string srcPath = writeTemp("vm_result_file_combinators_ir_backed.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_result_file_combinators_ir_backed_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 195);
  CHECK(readFile(outPath) == "65\n65\n65\n");
}

TEST_CASE("vm supports multi-slot struct Result payloads on IR-backed paths") {
  const std::string source = R"(
import /std/file/*

[struct]
Pair() {
  [i32] left{0i32}
  [i32] right{0i32}
}

[return<Result<Pair, FileError>>]
make_pair([i32] left, [i32] right) {
  return(Result.ok(Pair{[left] left, [right] right}))
}

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(err.why())
}

[return<int> effects(io_out, io_err) on_error<FileError, /log_file_error>]
main() {
  [Pair] direct{try(make_pair(1i32, 2i32))}
  [Pair] mapped{try(Result.map(make_pair(2i32, 3i32), []([Pair] value) {
    return(Pair{[left] plus(value.left, 5i32), [right] plus(value.right, 7i32)})
  }))}
  [Pair] chained{try(Result.and_then(make_pair(2i32, 3i32), []([Pair] value) {
    return(Result.ok(Pair{[left] plus(value.left, value.right), [right] 9i32}))
  }))}
  [Pair] summed{try(Result.map2(make_pair(1i32, 4i32), make_pair(2i32, 5i32), []([Pair] left, [Pair] right) {
    return(Pair{[left] plus(left.left, right.left), [right] plus(left.right, right.right)})
  }))}
  print_line(direct.left)
  print_line(direct.right)
  print_line(mapped.left)
  print_line(mapped.right)
  print_line(chained.left)
  print_line(chained.right)
  print_line(summed.left)
  print_line(summed.right)
  return(plus(direct.left,
    plus(direct.right,
      plus(mapped.left,
        plus(mapped.right,
          plus(chained.left, plus(chained.right, plus(summed.left, summed.right))))))))
}
)";
  const std::string srcPath = writeTemp("vm_result_multi_slot_struct_payload_ir_backed.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_result_multi_slot_struct_payload_ir_backed_out.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 46);
  CHECK(readFile(outPath) == "1\n2\n7\n10\n5\n9\n3\n9\n");
}

TEST_CASE("vm supports direct array and vector Result payloads on IR-backed paths") {
  const std::string source = R"(
import /std/file/*

[return<Result<array<i32>, FileError>>]
make_numbers() {
  [array<i32>] values{array<i32>(1i32, 2i32, 3i32)}
  return(Result.ok(values))
}

[return<Result<vector<i32>, FileError>> effects(heap_alloc)]
make_vector() {
  return(Result.ok(vector<i32>(4i32, 5i32)))
}

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(err.why())
}

[return<int> effects(io_out, io_err, heap_alloc) on_error<FileError, /log_file_error>]
main() {
  [array<i32>] direct{try(make_numbers())}
  [vector<i32>] vector_values{try(make_vector())}
  print_line(count(direct))
  print_line(direct[0i32])
  print_line(direct[2i32])
  return(plus(direct[0i32], direct[2i32]))
}
)";
  const std::string srcPath = writeTemp("vm_result_array_vector_payload_ir_backed.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_result_array_vector_payload_ir_backed_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 4);
  CHECK(readFile(outPath) == "3\n1\n3\n");
}

TEST_CASE("vm supports block-bodied Result.and_then lambdas on IR-backed paths") {
  const std::string source = R"(
import /std/file/*
import /std/collections/*

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(err.why())
}

[return<int> effects(io_err) on_error<FileError, /log_file_error>]
main() {
  [Result<i32, FileError>] ok{Result.ok(2i32)}
  [Result<i32, FileError>] chained{
    Result.and_then(ok, []([i32] value) {
      [i32] adjusted{plus(value, 1i32)}
      return(Result.ok(multiply(adjusted, 3i32)))
    })
  }
  return(try(chained))
}
)";
  const std::string srcPath = writeTemp("vm_result_and_then_block_body_ir_backed.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_result_and_then_block_body_ir_backed_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 9);
  CHECK(readFile(outPath).empty());
}

TEST_CASE("vm supports final-if Result.and_then lambdas on IR-backed paths") {
  const std::string source = R"(
import /std/file/*

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(err.why())
}

[return<int> effects(io_err) on_error<FileError, /log_file_error>]
main() {
  [Result<i32, FileError>] ok{Result.ok(2i32)}
  [Result<i32, FileError>] chained{
    Result.and_then(ok, []([i32] value) {
      if(equal(value, 2i32),
        then(){ return(Result.ok(multiply(value, 5i32))) },
        else(){ return(Result.ok(0i32)) })
    })
  }
  return(try(chained))
}
)";
  const std::string srcPath = writeTemp("vm_result_and_then_final_if_ir_backed.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_result_and_then_final_if_ir_backed_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 10);
  CHECK(readFile(outPath).empty());
}

TEST_CASE("vm supports direct map Result payloads on IR-backed paths") {
  const std::string source = R"(
import /std/file/*
import /std/collections/*

[return<Result<map<i32, i32>, FileError>>]
make_values() {
  [map<i32, i32>] values{1i32=7i32, 3i32=9i32}
  return(Result.ok(values))
}

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(err.why())
}

[return<int> effects(io_out, io_err) on_error<FileError, /log_file_error>]
main() {
  [map<i32, i32>] direct{try(make_values())}
  print_line(mapCount<i32, i32>(direct))
  print_line(mapAtUnsafe<i32, i32>(direct, 1i32))
  print_line(mapAtUnsafe<i32, i32>(direct, 3i32))
  return(plus(mapAtUnsafe<i32, i32>(direct, 1i32), mapAtUnsafe<i32, i32>(direct, 3i32)))
}
)";
  const std::string srcPath = writeTemp("vm_result_map_payload_ir_backed.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_result_map_payload_ir_backed_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 16);
  CHECK(readFile(outPath) == "2\n7\n9\n");
}

TEST_CASE("vm supports Buffer Result payloads on IR-backed paths") {
  const std::string source = R"(
import /std/gfx/*

[return<Result<Buffer<i32>, GfxError>> effects(gpu_dispatch)]
make_buffer() {
  [array<i32>] values{array<i32>(1i32, 2i32, 3i32)}
  return(Result.ok(/std/gfx/Buffer/upload(values)))
}

[effects(io_err)]
log_gfx_error([GfxError] err) {
  print_line_error(err.why())
}

[return<int> effects(gpu_dispatch, io_out, io_err) on_error<GfxError, /log_gfx_error>]
main() {
  [Result<Buffer<i32>, GfxError>] directStatus{make_buffer()}
  [GfxError] err{queueSubmitFailed()}
  [Result<Buffer<i32>, GfxError>] failedStatus{err.result<Buffer<i32>>()}
  if(Result.error(directStatus)) {
    return(1i32)
  }
  if(not(Result.error(failedStatus))) {
    return(2i32)
  }
  if(not(equal(count(Result.why(directStatus)), 0i32))) {
    return(3i32)
  }
  if(not(equal(count(Result.why(failedStatus)), 19i32))) {
    return(4i32)
  }
  [Buffer<i32>] direct{try(directStatus)}
  [Buffer<i32>] mappedValue{
    try(Result.map(make_buffer(), []([Buffer<i32>] value) { return(value) }))
  }
  [Buffer<i32>] chainedValue{
    try(Result.and_then(make_buffer(), []([Buffer<i32>] value) { return(Result.ok(value)) }))
  }
  [Buffer<i32>] combinedValue{
    try(Result.map2(make_buffer(), make_buffer(), []([Buffer<i32>] left, [Buffer<i32>] right) { return(right) }))
  }
  [array<i32>] directOut{direct.readback()}
  [array<i32>] mappedOut{mappedValue.readback()}
  [array<i32>] chainedOut{chainedValue.readback()}
  [array<i32>] combinedOut{combinedValue.readback()}
  [i32] directCount{direct.count()}
  [i32] mappedHead{mappedOut[0i32]}
  [i32] chainedCount{chainedValue.count()}
  [i32] combinedTail{combinedOut[2i32]}
  print_line(directCount)
  print_line(mappedHead)
  print_line(chainedCount)
  print_line(combinedTail)
  return(plus(plus(directCount, mappedHead), plus(chainedCount, combinedTail)))
}
)";
  const std::string srcPath = writeTemp("vm_result_buffer_payload_ir_backed.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_result_buffer_payload_ir_backed_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 10);
  CHECK(readFile(outPath) == "3\n1\n3\n3\n");
}

TEST_CASE("vm supports auto-bound direct Result combinator try consumers") {
  const std::string source = R"(
import /std/file/*

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(err.why())
}

[return<int> effects(io_out, io_err) on_error<FileError, /log_file_error>]
main() {
  [auto] mapped{ Result.map(Result.ok(2i32), []([i32] value) { return(multiply(value, 4i32)) }) }
  [auto] chained{ Result.and_then(Result.ok(2i32), []([i32] value) { return(Result.ok(plus(value, 3i32))) }) }
  [auto] summed{
    Result.map2(Result.ok(2i32), Result.ok(3i32), []([i32] left, [i32] right) { return(plus(left, right)) })
  }
  print_line(try(mapped))
  print_line(try(chained))
  return(try(summed))
}
)";
  const std::string srcPath = writeTemp("vm_result_auto_bound_combinators.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_result_auto_bound_combinators_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 5);
  CHECK(readFile(outPath) == "8\n5\n");
}


TEST_SUITE_END();
