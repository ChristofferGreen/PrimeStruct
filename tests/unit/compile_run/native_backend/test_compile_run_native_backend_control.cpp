#include "../test_compile_run_helpers.h"

#if (defined(__APPLE__) && (defined(__arm64__) || defined(__aarch64__))) || (defined(__linux__) && defined(__x86_64__))
TEST_SUITE_BEGIN("primestruct.compile.run.native_backend.control");

TEST_CASE("native print/print_line write to stdout and stderr") {
  const std::string source = R"(
[return<int>]
main() {
  print(42i32)
  print_line("hello"utf8)
  print_error("oops"utf8)
  print_line_error(7i32)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_print.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_print_exe").string();
  const std::string outPath = (testScratchPath("") / "primec_native_print_out.txt").string();
  const std::string errPath = (testScratchPath("") / "primec_native_print_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main --default-effects=io_out,io_err";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " > " + outPath + " 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "42hello\n");
  CHECK(readFile(errPath) == "oops7\n");
}

TEST_CASE("default entry effects enable io output") {
  const std::string source = R"(
[return<int>]
main() {
  print_line("entry default effects"utf8)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_entry_default_effects.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_entry_default_exe").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_entry_default_out.txt").string();
  const std::string vmOutPath =
      (testScratchPath("") / "primec_vm_entry_default_out.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "entry default effects\n");

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + vmOutPath;
  CHECK(runCommand(runVmCmd) == 0);
  CHECK(readFile(vmOutPath) == "entry default effects\n");
}

TEST_CASE("default effects token does not enable io_err output") {
  const std::string source = R"(
[return<int>]
main() {
  print_line_error("err"utf8)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_default_effects_no_err.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_default_effects_no_err_exe").string();
  const std::string errPath =
      (testScratchPath("") / "primec_native_default_effects_no_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main --default-effects=default 2> " +
      errPath;
  CHECK(runCommand(compileCmd) == 2);
  const std::string err = readFile(errPath);
  CHECK(err.find("Semantic error: print_line_error requires io_err effect") != std::string::npos);
  CHECK(err.find(": error: Semantic error: print_line_error requires io_err effect") != std::string::npos);
  CHECK(err.find("^") != std::string::npos);
}

TEST_CASE("default effects token enables vm output") {
  const std::string source = R"(
[return<int>]
main() {
  print_line("vm default effects"utf8)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_vm_print_default_effects.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_vm_print_default_out.txt").string();

  const std::string runCmd =
      "./primec --emit=vm " + srcPath + " --entry /main --default-effects=default > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "vm default effects\n");
}

TEST_CASE("default effects none requires explicit effects") {
  const std::string source = R"(
[return<int>]
main() {
  print_line("no effects"utf8)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_print_no_effects.prime", source);
  const std::string errPath = (testScratchPath("") / "primec_print_no_effects_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main --default-effects=none 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("print_line requires io_out effect") != std::string::npos);
}

TEST_CASE("native string access checks bounds") {
  const std::string source = R"(
[return<int>]
main() {
  [string] text{"abc"utf8}
  return(plus(100i32, text[9i32]))
}
)";
  const std::string srcPath = writeTemp("compile_native_string_bounds.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_string_bounds_exe").string();
  const std::string errPath = (testScratchPath("") / "primec_native_string_bounds_err.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " 2> " + errPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(errPath) == "string index out of bounds\n");
}

TEST_CASE("native string access rejects negative index") {
  const std::string source = R"(
[return<int>]
main() {
  [string] text{"abc"utf8}
  return(plus(100i32, text[-1i32]))
}
)";
  const std::string srcPath = writeTemp("compile_native_string_negative.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_string_negative_exe").string();
  const std::string errPath = (testScratchPath("") / "primec_native_string_negative_err.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " 2> " + errPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(errPath) == "string index out of bounds\n");
}

TEST_CASE("native prints full helper-returned string") {
  // TODO-4752: x86_64 PrintStringDynamic used to load the string length
  // into rdi, which the write syscall setup overwrote with the fd, so every
  // runtime-indexed string printed only its first byte.
  const std::string source = R"(
[return<string>]
make_message() {
  return("hello world"utf8)
}

[return<int> effects(io_out, io_err)]
main() {
  [string] message{make_message()}
  print_line(message)
  print(message)
  print_line_error(message)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_helper_string_print.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_helper_string_print_exe").string();
  const std::string outPath = (testScratchPath("") / "primec_native_helper_string_print_out.txt").string();
  const std::string errPath = (testScratchPath("") / "primec_native_helper_string_print_err.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath + " 2> " + errPath) == 0);
  CHECK(readFile(outPath) == "hello world\nhello world");
  CHECK(readFile(errPath) == "hello world\n");
}

TEST_CASE("native file write keeps full helper-returned string") {
  // TODO-4752: same x86_64 rdi length clobber as the print case, in
  // FileWriteStringDynamic.
  const std::string filePath =
      (testScratchPath("") / "primec_native_helper_string_file_out.txt").string();
  std::string source = R"(
import /std/file/*

[return<string>]
make_text() {
  return("hello file"utf8)
}

[return<Result<FileError>> effects(file_write) on_error<FileError, /log_file_error>]
main() {
  [string] text{make_text()}
  [File<Write>] file{ File<Write>("__PATH__"utf8)? }
  file.write(text)?
  file.close()?
  return(Result.ok())
}

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(FileError.why(err))
}
)";
  const std::string placeholder = "__PATH__";
  const size_t pathPos = source.find(placeholder);
  REQUIRE(pathPos != std::string::npos);
  source.replace(pathPos, placeholder.size(), filePath);
  const std::string srcPath = writeTemp("compile_native_helper_string_file.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_helper_string_file_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
  CHECK(readFile(filePath) == "hello file");
}

TEST_SUITE_END();
#endif
