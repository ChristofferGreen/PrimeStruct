#include "test_compile_run_helpers.h"

#include "test_compile_run_native_backend_core_helpers.h"

#if PRIMESTRUCT_NATIVE_CORE_ENABLED
TEST_SUITE_BEGIN("primestruct.compile.run.native_backend.core");

TEST_CASE("native backend supports imported stdlib Result sum pick") {
  const std::string source = R"(
import /std/result/*

[struct]
MyError() {
  [i32] code{5i32}
}

[return<int>]
main() {
  [Result<i32, MyError>] success{ok<i32, MyError>(7i32)}
  [Result<i32, MyError>] failure{error<i32, MyError>(MyError{})}
  [i32] left{pick(success) {
    ok(value) {
      value
    }
    error(err) {
      err.code
    }
  }}
  [i32] right{pick(failure) {
    ok(value) {
      value
    }
    error(err) {
      err.code
    }
  }}
  return(plus(left, right))
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_result_sum_pick.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_stdlib_result_sum_pick").string();
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 12);
}

TEST_CASE("native backend supports Result.error on imported stdlib Result sum") {
  const std::string source = R"(
import /std/result/*

[struct]
MyError() {
  [i32] code{5i32}
}

[return<int>]
main() {
  [Result<i32, MyError>] success{ok<i32, MyError>(7i32)}
  [Result<i32, MyError>] failure{error<i32, MyError>(MyError{})}
  if(Result.error(success)) {
    return(1i32)
  }
  if(not(Result.error(failure))) {
    return(2i32)
  }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_result_sum_error_helper.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_stdlib_result_sum_error_helper").string();
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("native backend supports Result.why on imported stdlib Result sum") {
  const std::string source = R"(
import /std/result/*

[struct]
MyError() {
  [i32] code{5i32}
}

namespace MyError {
  [return<string>]
  why([MyError] err) {
    if(equal(err.code, 5i32)) {
      return("five"utf8)
    }
    return("other"utf8)
  }
}

[return<int> effects(io_out)]
main() {
  [Result<i32, MyError>] success{ok<i32, MyError>(7i32)}
  [Result<i32, MyError>] failure{error<i32, MyError>(MyError{})}
  print_line(Result.why(success))
  print_line(Result.why(failure))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_result_sum_why_helper.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_stdlib_result_sum_why_helper").string();
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_native_stdlib_result_sum_why_helper_out.txt").string();
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 0);
  CHECK(readFile(outPath) == "\nfive\n");
}

TEST_CASE("native backend supports legacy Result.ok on imported stdlib Result sum") {
  const std::string source = R"(
import /std/result/*

[return<Result<i32, i32>>]
make_success() {
  return(Result.ok(7i32))
}

[return<int>]
main() {
  [Result<i32, i32>] localSuccess{Result.ok(5i32)}
  [Result<i32, i32>] returnedSuccess{make_success()}
  [Result<i32, i32>] failure{error<i32, i32>(3i32)}
  [i32] localValue{pick(localSuccess) {
    ok(value) {
      value
    }
    error(err) {
      100i32
    }
  }}
  [i32] returnedValue{pick(returnedSuccess) {
    ok(value) {
      value
    }
    error(err) {
      101i32
    }
  }}
  [i32] failureValue{pick(failure) {
    ok(value) {
      102i32
    }
    error(err) {
      err
    }
  }}
  if(not(equal(localValue, 5i32))) {
    return(1i32)
  }
  if(not(equal(returnedValue, 7i32))) {
    return(2i32)
  }
  if(not(equal(failureValue, 3i32))) {
    return(3i32)
  }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_result_sum_legacy_ok_helper.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_stdlib_result_sum_legacy_ok_helper").string();
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("native backend supports legacy Result.map on imported stdlib Result sum") {
  const std::string source = R"(
import /std/result/*

[return<Result<i32, i32>>]
make_mapped_success() {
  [Result<i32, i32>] source{Result.ok(7i32)}
  return(Result.map(source, []([i32] value) { return(plus(value, 4i32)) }))
}

[return<int>]
main() {
  [Result<i32, i32>] okSource{Result.ok(5i32)}
  [Result<i32, i32>] errorSource{error<i32, i32>(3i32)}
  [Result<i32, i32>] mappedOk{
    Result.map(okSource, []([i32] value) { return(plus(value, 2i32)) })
  }
  [Result<i32, i32>] mappedError{
    Result.map(errorSource, []([i32] value) { return(plus(value, 2i32)) })
  }
  [Result<i32, i32>] returnedMapped{make_mapped_success()}
  [i32] okValue{pick(mappedOk) {
    ok(value) {
      value
    }
    error(err) {
      100i32
    }
  }}
  [i32] errorValue{pick(mappedError) {
    ok(value) {
      101i32
    }
    error(err) {
      err
    }
  }}
  [i32] returnedValue{pick(returnedMapped) {
    ok(value) {
      value
    }
    error(err) {
      102i32
    }
  }}
  if(not(equal(okValue, 7i32))) {
    return(1i32)
  }
  if(not(equal(errorValue, 3i32))) {
    return(2i32)
  }
  if(not(equal(returnedValue, 11i32))) {
    return(3i32)
  }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_result_sum_legacy_map_helper.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_stdlib_result_sum_legacy_map_helper").string();
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("native backend supports legacy Result.and_then on imported stdlib Result sum") {
  const std::string source = R"(
import /std/result/*

[return<Result<i32, i32>>]
make_chained_success() {
  [Result<i32, i32>] source{Result.ok(7i32)}
  return(Result.and_then(source, []([i32] value) { return(Result.ok(plus(value, 4i32))) }))
}

[return<int>]
main() {
  [Result<i32, i32>] okSource{Result.ok(5i32)}
  [Result<i32, i32>] errorSource{error<i32, i32>(3i32)}
  [Result<i32, i32>] chainedOk{
    Result.and_then(okSource, []([i32] value) { return(Result.ok(plus(value, 2i32))) })
  }
  [Result<i32, i32>] chainedToError{
    Result.and_then(okSource, []([i32] value) { return(Result<i32, i32>{[error] 4i32}) })
  }
  [Result<i32, i32>] chainedError{
    Result.and_then(errorSource, []([i32] value) { return(Result.ok(plus(value, 2i32))) })
  }
  [Result<i32, i32>] returnedChained{make_chained_success()}
  [i32] okValue{pick(chainedOk) {
    ok(value) {
      value
    }
    error(err) {
      100i32
    }
  }}
  [i32] toErrorValue{pick(chainedToError) {
    ok(value) {
      101i32
    }
    error(err) {
      err
    }
  }}
  [i32] errorValue{pick(chainedError) {
    ok(value) {
      102i32
    }
    error(err) {
      err
    }
  }}
  [i32] returnedValue{pick(returnedChained) {
    ok(value) {
      value
    }
    error(err) {
      103i32
    }
  }}
  if(not(equal(okValue, 7i32))) {
    return(1i32)
  }
  if(not(equal(toErrorValue, 4i32))) {
    return(2i32)
  }
  if(not(equal(errorValue, 3i32))) {
    return(3i32)
  }
  if(not(equal(returnedValue, 11i32))) {
    return(4i32)
  }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_result_sum_legacy_and_then_helper.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_stdlib_result_sum_legacy_and_then_helper").string();
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("native backend supports legacy Result.map2 on imported stdlib Result sum") {
  const std::string source = R"(
import /std/result/*

[return<Result<i32, i32>>]
make_summed_success() {
  [Result<i32, i32>] left{Result.ok(7i32)}
  [Result<i32, i32>] right{Result.ok(4i32)}
  return(Result.map2(left, right, []([i32] a, [i32] b) { return(plus(a, b)) }))
}

[return<int>]
main() {
  [Result<i32, i32>] leftOk{Result.ok(5i32)}
  [Result<i32, i32>] rightOk{Result.ok(2i32)}
  [Result<i32, i32>] leftError{error<i32, i32>(3i32)}
  [Result<i32, i32>] rightError{error<i32, i32>(4i32)}
  [Result<i32, i32>] summed{
    Result.map2(leftOk, rightOk, []([i32] a, [i32] b) { return(plus(a, b)) })
  }
  [Result<i32, i32>] firstError{
    Result.map2(leftError, rightError, []([i32] a, [i32] b) { return(plus(a, b)) })
  }
  [Result<i32, i32>] secondError{
    Result.map2(leftOk, rightError, []([i32] a, [i32] b) { return(plus(a, b)) })
  }
  [Result<i32, i32>] returnedSummed{make_summed_success()}
  [i32] summedValue{pick(summed) {
    ok(value) {
      value
    }
    error(err) {
      100i32
    }
  }}
  [i32] firstErrorValue{pick(firstError) {
    ok(value) {
      101i32
    }
    error(err) {
      err
    }
  }}
  [i32] secondErrorValue{pick(secondError) {
    ok(value) {
      102i32
    }
    error(err) {
      err
    }
  }}
  [i32] returnedValue{pick(returnedSummed) {
    ok(value) {
      value
    }
    error(err) {
      103i32
    }
  }}
  if(not(equal(summedValue, 7i32))) {
    return(1i32)
  }
  if(not(equal(firstErrorValue, 3i32))) {
    return(2i32)
  }
  if(not(equal(secondErrorValue, 4i32))) {
    return(3i32)
  }
  if(not(equal(returnedValue, 11i32))) {
    return(4i32)
  }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_result_sum_legacy_map2_helper.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_stdlib_result_sum_legacy_map2_helper").string();
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("native backend compiles packed error struct Result combinator payloads on IR-backed paths") {
  const std::string source = R"(
import /std/file/*
import /std/collections/*
import /std/image/*
import /std/gfx/*

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(err.why())
}

[return<int> effects(io_out, io_err) on_error<FileError, /log_file_error>]
main() {
  [ContainerError] mapped{
    try(Result.map(Result.ok(2i32), []([i32] value) { return(ContainerError{value}) }))
  }
  [ImageError] chained{
    try(Result.and_then(Result.ok(3i32), []([i32] value) { return(Result.ok(ImageError{value})) }))
  }
  [GfxError] summed{
    try(Result.map2(Result.ok(4i32), Result.ok(5i32), []([i32] left, [i32] right) {
      return(GfxError{plus(left, right)})
    }))
  }
  return(plus(mapped.code, plus(chained.code, summed.code)))
}
)";
  const std::string srcPath = writeTemp("compile_native_result_packed_error_combinators_ir_backed.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_result_packed_error_combinators_ir_backed").string();
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_native_result_packed_error_combinators_ir_backed_out.txt")
          .string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " > " + outPath;
  CHECK(runCommand(runCmd) == 14);
  CHECK(readFile(outPath).empty());
}

TEST_CASE("native backend supports direct single-slot struct Result.ok payloads on IR-backed paths") {
  const std::string source = R"(
import /std/file/*

[struct]
Label() {
  [i32] code{0i32}
}

[return<Result<Label, FileError>>]
make_label() {
  return(Result.ok(Label{[code] 7i32}))
}

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(err.why())
}

[return<int> effects(io_out, io_err) on_error<FileError, /log_file_error>]
main() {
  [Label] value{try(make_label())}
  print_line(value.code)
  return(value.code)
}
)";
  const std::string srcPath = writeTemp("compile_native_result_single_slot_struct_payload_ir_backed.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_result_single_slot_struct_payload_ir_backed").string();
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_native_result_single_slot_struct_payload_ir_backed_out.txt")
          .string();
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " > " + outPath;
  CHECK(runCommand(runCmd) == 7);
  CHECK(readFile(outPath) == "7\n");
}

TEST_CASE("native backend supports single-slot struct Result combinator payloads on IR-backed paths") {
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
  const std::string srcPath =
      writeTemp("compile_native_result_single_slot_struct_combinators_ir_backed.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() /
                               "primec_native_result_single_slot_struct_combinators_ir_backed")
                                  .string();
  const std::string outPath =
      (std::filesystem::temp_directory_path() /
       "primec_native_result_single_slot_struct_combinators_ir_backed_out.txt")
          .string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " > " + outPath;
  CHECK(runCommand(runCmd) == 19);
  CHECK(readFile(outPath) == "7\n5\n7\n");
}

TEST_CASE("native backend supports direct File Result payloads on IR-backed paths") {
  const std::string filePath =
      (std::filesystem::temp_directory_path() / "primec_native_result_file_payload_ir_backed.txt").string();
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
  const std::string srcPath = writeTemp("compile_native_result_file_payload_ir_backed.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_result_file_payload_ir_backed").string();
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_native_result_file_payload_ir_backed_out.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " > " + outPath;
  CHECK(runCommand(runCmd) == 65);
  CHECK(readFile(outPath) == "65\n");
}

TEST_CASE("native backend supports packed File Result combinator payloads on IR-backed paths") {
  const std::string filePath =
      (std::filesystem::temp_directory_path() / "primec_native_result_file_combinators_ir_backed.txt").string();
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
  const std::string srcPath = writeTemp("compile_native_result_file_combinators_ir_backed.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_result_file_combinators_ir_backed").string();
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_native_result_file_combinators_ir_backed_out.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " > " + outPath;
  CHECK(runCommand(runCmd) == 195);
  CHECK(readFile(outPath) == "65\n65\n65\n");
}

TEST_CASE("native backend compiles multi-slot struct Result payloads on IR-backed paths") {
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
  const std::string srcPath = writeTemp("compile_native_result_multi_slot_struct_payload_ir_backed.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_result_multi_slot_struct_payload_ir_backed").string();
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
}

TEST_CASE("native backend compiles direct array Result payloads on IR-backed paths") {
  const std::string source = R"(
import /std/file/*

[return<Result<array<i32>, FileError>>]
make_numbers() {
  [array<i32>] values{array<i32>(1i32, 2i32, 3i32)}
  return(Result.ok(values))
}

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(err.why())
}

[return<int> effects(io_out, io_err) on_error<FileError, /log_file_error>]
main() {
  [array<i32>] direct{try(make_numbers())}
  print_line(count(direct))
  print_line(direct[0i32])
  print_line(direct[2i32])
  return(plus(direct[0i32], direct[2i32]))
}
)";
  const std::string srcPath = writeTemp("compile_native_result_array_vector_payload_ir_backed.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_result_array_vector_payload_ir_backed").string();
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_native_result_array_vector_payload_ir_backed_out.txt")
          .string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
}

TEST_CASE("native backend supports block-bodied Result.and_then lambdas on IR-backed paths") {
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
  const std::string srcPath = writeTemp("compile_native_result_and_then_block_body_ir_backed.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_result_and_then_block_body_ir_backed").string();
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_native_result_and_then_block_body_ir_backed_out.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " > " + outPath;
  CHECK(runCommand(runCmd) == 9);
  CHECK(readFile(outPath).empty());
}

TEST_CASE("native backend supports final-if Result.and_then lambdas on IR-backed paths") {
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
  const std::string srcPath = writeTemp("compile_native_result_and_then_final_if_ir_backed.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_result_and_then_final_if_ir_backed").string();
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_native_result_and_then_final_if_ir_backed_out.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " > " + outPath;
  CHECK(runCommand(runCmd) == 10);
  CHECK(readFile(outPath).empty());
}

TEST_CASE("native backend compiles direct map Result payloads on IR-backed paths") {
  const std::string source = R"(
import /std/file/*
import /std/collections/*

[return<Result<map<i32, i32>, FileError>>]
make_values() {
  [map<i32, i32>] values{map<i32, i32>{1i32=7i32, 3i32=9i32}}
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
  const std::string srcPath = writeTemp("compile_native_result_map_payload_ir_backed.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_result_map_payload_ir_backed").string();
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_native_result_map_payload_ir_backed_out.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
}

TEST_CASE("native backend supports Buffer Result payloads on IR-backed paths") {
  const std::string source = R"(
import /std/gfx/*

[return<Result<Buffer<i32>, GfxError>> effects(gpu_dispatch)]
make_buffer() {
  [array<i32>] values{array<i32>(1i32, 2i32, 3i32)}
  return(Result.ok(/std/gfx/Buffer/upload(values)))
}

[return<int> on_error<GfxError, /swallow_gfx_error>]
consume_error() {
  [GfxError] err{queueSubmitFailed()}
  [Result<Buffer<i32>, GfxError>] failed{err.result<Buffer<i32>>()}
  [Buffer<i32>] unreachable{try(failed)}
  return(unreachable.count())
}

[effects(io_err)]
swallow_gfx_error([GfxError] err) {}

[return<int> effects(gpu_dispatch) on_error<GfxError, /swallow_gfx_error>]
main() {
  [Result<Buffer<i32>, GfxError>] directStatus{make_buffer()}
  [Result<Buffer<i32>, GfxError>] mappedStatus{
    Result.map(make_buffer(), []([Buffer<i32>] value) { return(value) })
  }
  [Result<Buffer<i32>, GfxError>] chainedStatus{
    Result.and_then(make_buffer(), []([Buffer<i32>] value) { return(Result.ok(value)) })
  }
  [Result<Buffer<i32>, GfxError>] combinedStatus{
    Result.map2(make_buffer(), make_buffer(), []([Buffer<i32>] left, [Buffer<i32>] right) { return(right) })
  }
  [GfxError] err{queueSubmitFailed()}
  [Result<Buffer<i32>, GfxError>] failedStatus{err.result<Buffer<i32>>()}

  if(Result.error(directStatus)) {
    return(1i32)
  }
  if(Result.error(mappedStatus)) {
    return(2i32)
  }
  if(Result.error(chainedStatus)) {
    return(3i32)
  }
  if(Result.error(combinedStatus)) {
    return(4i32)
  }
  if(not(Result.error(failedStatus))) {
    return(5i32)
  }
  if(not(equal(count(Result.why(directStatus)), 0i32))) {
    return(6i32)
  }
  if(not(equal(count(Result.why(failedStatus)), 19i32))) {
    return(7i32)
  }
  if(not(equal(consume_error(), 8i32))) {
    return(8i32)
  }

  [Buffer<i32>] direct{try(directStatus)}
  [Buffer<i32>] mappedValue{try(mappedStatus)}
  [Buffer<i32>] chainedValue{try(chainedStatus)}
  [Buffer<i32>] combinedValue{try(combinedStatus)}
  [array<i32>] directOut{direct.readback()}
  [array<i32>] mappedOut{mappedValue.readback()}
  [array<i32>] chainedOut{chainedValue.readback()}
  [array<i32>] combinedOut{combinedValue.readback()}
  return(plus(plus(direct.count(), mappedOut[0i32]), plus(chainedValue.count(), combinedOut[2i32])))
}
)";
  const std::string srcPath = writeTemp("compile_native_result_buffer_payload_ir_backed.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_result_buffer_payload_ir_backed").string();
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_native_result_buffer_payload_ir_backed_out.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 10);
  CHECK(readFile(outPath).empty());
}

TEST_CASE("native backend supports auto-bound direct Result combinator try consumers") {
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
  const std::string srcPath = writeTemp("compile_native_result_auto_bound_combinators.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_result_auto_bound_combinators").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_result_auto_bound_combinators_out.txt").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 5);
  CHECK(readFile(outPath) == "8\n5\n");
}

TEST_CASE("compiles and runs native direct type namespace string helpers") {
  const std::string source = R"(
[struct]
GfxError() {
  [i32] code{0i32}
}

namespace GfxError {
  [return<string>]
  why([GfxError] err) {
    return("queue_submit_failed"utf8)
  }
}

[return<int> effects(io_out)]
main() {
  [GfxError] err{GfxError{8i32}}
  [string] whyText{GfxError.why(err)}
  print_line(GfxError.why(err))
  return(count(whyText))
}
)";
  const std::string srcPath = writeTemp("compile_native_type_namespace_string_helper.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_type_namespace_string_helper_exe").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_type_namespace_string_helper_out.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " > " + outPath;
  CHECK(runCommand(runCmd) == 19);
  CHECK(readFile(outPath) == "queue_submit_failed\n");
}

TEST_CASE("native backend supports try on imported stdlib Result sum ok") {
  const std::string source = R"(
import /std/result/*

[struct]
MyError() {
  [i32] code{6i32}
}

[return<void>]
swallow([MyError] err) {
}

[return<int> on_error<MyError, /swallow>]
main() {
  [Result<i32, MyError>] status{ok<i32, MyError>(9i32)}
  [i32] value{try(status)}
  return(value)
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_result_sum_try_ok.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_stdlib_result_sum_try_ok_exe").string();
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("native backend supports try on imported stdlib Result sum error") {
  const std::string source = R"(
import /std/result/*

[struct]
MyError() {
  [i32] code{6i32}
}

[return<void> effects(io_out)]
swallow([MyError] err) {
  print_line(err.code)
}

[return<int> effects(io_out) on_error<MyError, /swallow>]
main() {
  [Result<i32, MyError>] status{error<i32, MyError>(MyError{})}
  [i32] value{try(status)}
  return(value)
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_result_sum_try_error.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_stdlib_result_sum_try_error_exe").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_stdlib_result_sum_try_error_out.txt").string();
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 6);
  CHECK(readFile(outPath) == "6\n");
}

TEST_CASE("native backend supports postfix question on direct imported stdlib Result sum ok") {
  const std::string source = R"(
import /std/result/*

[struct]
MyError() {
  [i32] code{6i32}
}

[return<Result<i32, MyError>>]
make_ok() {
  return(Result.ok(9i32))
}

[return<void>]
swallow([MyError] err) {
}

[return<int> on_error<MyError, /swallow>]
main() {
  [i32] value{make_ok()?}
  return(value)
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_result_sum_postfix_direct_ok.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_stdlib_result_sum_postfix_direct_ok_exe").string();
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("native backend supports postfix question on direct imported stdlib Result sum error") {
  const std::string source = R"(
import /std/result/*

[struct]
MyError() {
  [i32] code{6i32}
}

[return<Result<i32, MyError>>]
make_error() {
  return(error<i32, MyError>(MyError{}))
}

[return<void> effects(io_out)]
swallow([MyError] err) {
  print_line(err.code)
}

[return<Result<i32, MyError>> effects(io_out) on_error<MyError, /swallow>]
forward_error() {
  [i32] value{make_error()?}
  return(Result.ok(value))
}

[return<int> effects(io_out)]
main() {
  [Result<i32, MyError>] status{forward_error()}
  [i32] observed{pick(status) {
    ok(value) {
      value
    }
    error(err) {
      err.code
    }
  }}
  return(observed)
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_result_sum_postfix_direct_error.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_stdlib_result_sum_postfix_direct_error_exe").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_stdlib_result_sum_postfix_direct_error_out.txt").string();
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 6);
  CHECK(readFile(outPath) == "6\n");
}

TEST_CASE("native backend propagates imported stdlib Result sum try ok through Result return") {
  const std::string source = R"(
import /std/result/*

[struct]
MyError() {
  [i32] code{6i32}
}

[return<void>]
swallow([MyError] err) {
}

[return<Result<i32, MyError>> on_error<MyError, /swallow>]
forward_ok() {
  [Result<i32, MyError>] status{ok<i32, MyError>(9i32)}
  [i32] value{try(status)}
  return(Result.ok(plus(value, 1i32)))
}

[return<int>]
main() {
  [Result<i32, MyError>] status{forward_ok()}
  [i32] observed{pick(status) {
    ok(value) {
      value
    }
    error(err) {
      err.code
    }
  }}
  return(observed)
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_result_sum_try_result_return_ok.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_stdlib_result_sum_try_result_return_ok_exe").string();
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 10);
}

TEST_CASE("native backend propagates imported stdlib Result sum try error through Result return") {
  const std::string source = R"(
import /std/result/*

[struct]
MyError() {
  [i32] code{6i32}
}

[return<void> effects(io_out)]
swallow([MyError] err) {
  print_line(err.code)
}

[return<Result<i32, MyError>> effects(io_out) on_error<MyError, /swallow>]
forward_error() {
  [Result<i32, MyError>] status{error<i32, MyError>(MyError{})}
  [i32] value{try(status)}
  return(Result.ok(value))
}

[return<int> effects(io_out)]
main() {
  [Result<i32, MyError>] status{forward_error()}
  [i32] observed{pick(status) {
    ok(value) {
      value
    }
    error(err) {
      err.code
    }
  }}
  return(observed)
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_result_sum_try_result_return_error.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_stdlib_result_sum_try_result_return_error_exe").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_stdlib_result_sum_try_result_return_error_out.txt").string();
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 6);
  CHECK(readFile(outPath) == "6\n");
}

TEST_SUITE_END();
#endif
