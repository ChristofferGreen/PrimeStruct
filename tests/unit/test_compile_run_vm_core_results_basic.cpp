#include "test_compile_run_helpers.h"

#include <cerrno>

TEST_SUITE_BEGIN("primestruct.compile.run.vm.core");


TEST_CASE("vm supports string return types") {
  const std::string source = R"(
[return<string>]
message() {
  return("hi"utf8)
}

[return<int> effects(io_out)]
main() {
  print_line(message())
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_string_return.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_vm_string_return_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "hi\n");
}

TEST_CASE("vm supports Result.why hooks") {
  const std::string source = R"(
[struct]
MyError() {
  [i32] code{0i32}
}

namespace MyError {
  [return<string>]
  why([MyError] err) {
    return("custom error"utf8)
  }
}

[return<Result<MyError>>]
make_error() {
  return(1i32)
}

[return<int> effects(io_out)]
main() {
  print_line(Result.why(make_error()))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_result_why_custom.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_vm_result_why_custom_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "custom error\n");
}

TEST_CASE("vm supports stdlib FileError result helpers") {
  const std::string source = R"(
import /std/file/*

[return<Result<FileError>>]
make_status() {
  return(FileError.status(fileReadEof()))
}

[return<Result<i32, FileError>>]
make_value() {
  return(FileError.result<i32>(fileReadEof()))
}

[return<int> effects(io_out)]
main() {
  [Result<FileError>] status{make_status()}
  [Result<i32, FileError>] valueStatus{make_value()}
  [FileError] err{fileReadEof()}
  [Result<FileError>] methodStatus{FileError.status(err)}
  [Result<i32, FileError>] methodValueStatus{FileError.result<i32>(err)}
  [bool] eof{fileErrorIsEof(fileReadEof())}
  [bool] otherEof{fileErrorIsEof(1i32)}
  [bool] directStatusError{Result.error(status)}
  [bool] methodStatusError{Result.error(methodStatus)}
  if(not(Result.error(status))) {
    return(1i32)
  }
  if(not(Result.error(valueStatus))) {
    return(2i32)
  }
  if(not(eof)) {
    return(3i32)
  }
  if(otherEof) {
    return(4i32)
  }
  if(not(directStatusError)) {
    return(5i32)
  }
  if(not(methodStatusError)) {
    return(6i32)
  }
  print_line(Result.why(status))
  print_line(Result.why(valueStatus))
  print_line(Result.why(methodStatus))
  print_line(Result.why(methodValueStatus))
  print_line(Result.why(FileError.status(fileReadEof())))
  print_line(Result.why(FileError.result<i32>(fileReadEof())))
  print_line(Result.why(methodStatus))
  print_line(Result.why(methodValueStatus))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_file_error_result_helpers.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_stdlib_file_error_result_helpers_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "EOF\nEOF\nEOF\nEOF\nEOF\nEOF\nEOF\nEOF\n");
}

TEST_CASE("vm supports Result.map on IR-backed path") {
  const std::string source = R"(
import /std/file/*

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(err.why())
}

[return<int> effects(io_out, io_err) on_error<FileError, /log_file_error>]
main() {
  [Result<i32, FileError>] ok{Result.ok(2i32)}
  [Result<i32, FileError>] failed{FileError.result<i32>(fileReadEof())}
  [Result<i32, FileError>] mappedOk{
    Result.map(ok, []([i32] value) { return(multiply(value, 4i32)) })
  }
  [Result<i32, FileError>] mappedFailed{
    Result.map(failed, []([i32] value) { return(multiply(value, 4i32)) })
  }
  if(Result.error(mappedOk)) {
    return(1i32)
  }
  if(not(Result.error(mappedFailed))) {
    return(2i32)
  }
  print_line(Result.why(mappedFailed))
  return(try(mappedOk))
}
)";
  const std::string srcPath = writeTemp("vm_result_map_ir_backed.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_result_map_ir_backed_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 8);
  CHECK(readFile(outPath) == "EOF\n");
}

TEST_CASE("vm supports Result.and_then on IR-backed path") {
  const std::string source = R"(
import /std/file/*

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(err.why())
}

[return<int> effects(io_out, io_err) on_error<FileError, /log_file_error>]
main() {
  [Result<i32, FileError>] ok{Result.ok(2i32)}
  [Result<i32, FileError>] failed{FileError.result<i32>(fileReadEof())}
  [Result<i32, FileError>] chainedOk{
    Result.and_then(ok, []([i32] value) { return(Result.ok(multiply(value, 4i32))) })
  }
  [Result<FileError>] chainedStatus{
    Result.and_then(ok, []([i32] value) { return(FileError.status(FileError.eof())) })
  }
  [Result<i32, FileError>] chainedFailed{
    Result.and_then(failed, []([i32] value) { return(Result.ok(multiply(value, 4i32))) })
  }
  if(Result.error(chainedOk)) {
    return(1i32)
  }
  if(not(Result.error(chainedStatus))) {
    return(2i32)
  }
  if(not(Result.error(chainedFailed))) {
    return(3i32)
  }
  print_line(Result.why(chainedStatus))
  print_line(Result.why(chainedFailed))
  return(try(chainedOk))
}
)";
  const std::string srcPath = writeTemp("vm_result_and_then_ir_backed.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_result_and_then_ir_backed_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 8);
  CHECK(readFile(outPath) == "EOF\nEOF\n");
}

TEST_CASE("vm supports Result.map2 on IR-backed path") {
  const std::string source = R"(
import /std/collections/*

swallow_container_error([ContainerError] err) {}

[return<int> effects(io_out) on_error<ContainerError, /swallow_container_error>]
main() {
  [Result<i32, ContainerError>] first{Result.ok(2i32)}
  [Result<i32, ContainerError>] second{Result.ok(3i32)}
  [Result<i32, ContainerError>] leftFailed{/ContainerError/result<i32>(/ContainerError/missing_key())}
  [Result<i32, ContainerError>] rightFailed{/ContainerError/result<i32>(/ContainerError/empty())}
  [Result<i32, ContainerError>] summed{
    Result.map2(first, second, []([i32] left, [i32] right) { return(plus(left, right)) })
  }
  [Result<i32, ContainerError>] firstError{
    Result.map2(leftFailed, rightFailed, []([i32] left, [i32] right) { return(plus(left, right)) })
  }
  [Result<i32, ContainerError>] secondError{
    Result.map2(first, rightFailed, []([i32] left, [i32] right) { return(plus(left, right)) })
  }
  if(Result.error(summed)) {
    return(1i32)
  }
  if(not(Result.error(firstError))) {
    return(2i32)
  }
  if(not(Result.error(secondError))) {
    return(3i32)
  }
  print_line(Result.why(firstError))
  print_line(Result.why(secondError))
  return(try(summed))
}
)";
  const std::string srcPath = writeTemp("vm_result_map2_ir_backed.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_result_map2_ir_backed_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 5);
  CHECK(readFile(outPath) == "container missing key\ncontainer empty\n");
}

TEST_CASE("vm supports f32 Result payloads on IR-backed paths") {
  const std::string source = R"(
import /std/file/*

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(err.why())
}

[return<int> effects(io_out, io_err) on_error<FileError, /log_file_error>]
main() {
  [Result<f32, FileError>] ok{Result.ok(1.5f32)}
  [Result<f32, FileError>] mapped{
    Result.map(ok, []([f32] value) { return(plus(value, 1.0f32)) })
  }
  [Result<f32, FileError>] chained{
    Result.and_then(mapped, []([f32] value) { return(Result.ok(multiply(value, 2.0f32))) })
  }
  [Result<f32, FileError>] summed{
    Result.map2(chained, Result.ok(0.5f32), []([f32] left, [f32] right) { return(plus(left, right)) })
  }
  [f32] value{summed?}
  print_line(convert<int>(multiply(value, 10.0f32)))
  return(convert<int>(multiply(value, 10.0f32)))
}
)";
  const std::string srcPath = writeTemp("vm_result_f32_ir_backed.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_result_f32_ir_backed_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 55);
  CHECK(readFile(outPath) == "55\n");
}

TEST_CASE("vm supports direct Result.ok combinator sources on IR-backed paths") {
  const std::string source = R"(
import /std/file/*

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(err.why())
}

[return<int> effects(io_out, io_err) on_error<FileError, /log_file_error>]
main() {
  [Result<i32, FileError>] mapped{
    Result.map(Result.ok(2i32), []([i32] value) { return(multiply(value, 4i32)) })
  }
  [Result<i32, FileError>] chained{
    Result.and_then(Result.ok(2i32), []([i32] value) { return(Result.ok(plus(value, 3i32))) })
  }
  [Result<i32, FileError>] summed{
    Result.map2(Result.ok(2i32), Result.ok(3i32), []([i32] left, [i32] right) { return(plus(left, right)) })
  }
  print_line(try(mapped))
  print_line(try(chained))
  return(try(summed))
}
)";
  const std::string srcPath = writeTemp("vm_result_direct_ok_ir_backed.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_result_direct_ok_ir_backed_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 5);
  CHECK(readFile(outPath) == "8\n5\n");
}

TEST_CASE("vm supports direct packed ContainerError and ImageError Result payloads on IR-backed paths" * doctest::skip(true)) {
  const std::string source = R"(
import /std/file/*
import /std/collections/*
import /std/image/*

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(err.why())
}

[return<int> effects(io_out, io_err) on_error<FileError, /log_file_error>]
main() {
  [ContainerError] container{try(Result.ok(ContainerError(4i32)))}
  [ImageError] image{try(Result.ok(ImageError(3i32)))}
  return(plus(container.code, image.code))
}
)";
  const std::string srcPath = writeTemp("vm_result_packed_error_payloads_ir_backed.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_result_packed_error_payloads_ir_backed_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 7);
  CHECK(readFile(outPath).empty());
}

TEST_CASE("vm supports packed error struct Result combinator payloads on IR-backed paths" * doctest::skip(true)) {
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
    try(Result.map(Result.ok(2i32), []([i32] value) { return(ContainerError(value)) }))
  }
  [ImageError] chained{
    try(Result.and_then(Result.ok(3i32), []([i32] value) { return(Result.ok(ImageError(value))) }))
  }
  [GfxError] summed{
    try(Result.map2(Result.ok(4i32), Result.ok(5i32), []([i32] left, [i32] right) {
      return(GfxError(plus(left, right)))
    }))
  }
  return(plus(mapped.code, plus(chained.code, summed.code)))
}
)";
  const std::string srcPath = writeTemp("vm_result_packed_error_combinators_ir_backed.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_result_packed_error_combinators_ir_backed_out.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 14);
  CHECK(readFile(outPath).empty());
}

TEST_CASE("vm supports direct single-slot struct Result.ok payloads on IR-backed paths") {
  const std::string source = R"(
import /std/file/*

[struct]
Label() {
  [i32] code{0i32}
}

[return<Result<Label, FileError>>]
make_label() {
  return(Result.ok(Label([code] 7i32)))
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
  const std::string srcPath = writeTemp("vm_result_single_slot_struct_payload_ir_backed.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_result_single_slot_struct_payload_ir_backed_out.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 7);
  CHECK(readFile(outPath) == "7\n");
}


TEST_SUITE_END();
