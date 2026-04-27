#include "test_compile_run_helpers.h"

#include <cerrno>

TEST_SUITE_BEGIN("primestruct.compile.run.vm.core");


TEST_CASE("vm uses stdlib File helper wrappers") {
  const std::string filePath =
      (std::filesystem::temp_directory_path() / "primec_vm_stdlib_file_helpers.txt").string();
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
  const std::string source = R"(
import /std/file/*

[return<Result<FileError>> effects(file_write) on_error<FileError, /log_file_error>]
main() {
  [File<Write>] file{ File<Write>("__PATH__"utf8)? }
  [array<i32>] bytes{ array<i32>(68i32, 69i32) }
  /File/write<Write, i32>(file, 65i32)?
  /File/write_line<Write, i32>(file, 66i32)?
  file.write_byte(67i32)?
  /File/write_bytes(file, bytes)?
  /File/flush(file)?
  file.close()?
  return(Result.ok())
}

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(FileError.why(err))
}
)";
  std::string program = source;
  const std::string placeholder = "__PATH__";
  const size_t pathPos = program.find(placeholder);
  REQUIRE(pathPos != std::string::npos);
  program.replace(pathPos, placeholder.size(), escape(filePath));
  const std::string srcPath = writeTemp("vm_stdlib_file_helpers.prime", program);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(filePath) == "6566\nCDE");
}

TEST_CASE("vm uses stdlib File open helper wrappers") {
  const std::string filePath =
      (std::filesystem::temp_directory_path() / "primec_vm_stdlib_file_open_helpers.txt").string();
  const std::string stdoutPath =
      (std::filesystem::temp_directory_path() / "primec_vm_stdlib_file_open_helpers_out.txt").string();
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
  const std::string source = R"(
import /std/file/*

[return<Result<FileError>> effects(file_read, file_write, io_out) on_error<FileError, /log_file_error>]
main() {
  [File<Write>] writer{/File/open_write("__PATH__"utf8)?}
  writer.write("alpha"utf8)?
  writer.close()?
  [File<Append>] appender{/File/open_append("__PATH__"utf8)?}
  appender.write_line("omega"utf8)?
  appender.close()?
  [File<Read>] reader{/File/open_read("__PATH__"utf8)?}
  [i32 mut] first{0i32}
  reader.read_byte(first)?
  print_line(first)
  reader.close()?
  return(Result.ok())
}

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(FileError.why(err))
}
)";
  std::string program = source;
  const std::string placeholder = "__PATH__";
  size_t pathPos = program.find(placeholder);
  REQUIRE(pathPos != std::string::npos);
  program.replace(pathPos, placeholder.size(), escape(filePath));
  pathPos = program.find(placeholder);
  REQUIRE(pathPos != std::string::npos);
  program.replace(pathPos, placeholder.size(), escape(filePath));
  pathPos = program.find(placeholder);
  REQUIRE(pathPos != std::string::npos);
  program.replace(pathPos, placeholder.size(), escape(filePath));
  const std::string srcPath = writeTemp("vm_stdlib_file_open_helpers.prime", program);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + stdoutPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(filePath) == "alphaomega\n");
  CHECK(readFile(stdoutPath) == "97\n");
}

TEST_CASE("vm stdlib File close helper disarms the original handle") {
  const std::string filePath =
      (std::filesystem::temp_directory_path() / "primec_vm_stdlib_file_close_helper.txt").string();
  const auto escape = [](const std::string &text) {
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
  const std::string source = R"(
import /std/file/*

[effects(file_write), return<Result<FileError>> on_error<FileError, /log_file_error>]
main() {
  [File<Write>] file{ File<Write>("__PATH__"utf8)? }
  /File/write<Write, string>(file, "alpha"utf8)?
  file.close()?
  /File/write<Write, string>(file, "beta"utf8)
  return(Result.ok())
}

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(FileError.why(err))
}
)";
  std::string program = source;
  const std::string placeholder = "__PATH__";
  const size_t pathPos = program.find(placeholder);
  REQUIRE(pathPos != std::string::npos);
  program.replace(pathPos, placeholder.size(), escape(filePath));
  const std::string srcPath = writeTemp("vm_stdlib_file_close_helper.prime", program);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(filePath) == "alpha");
}

TEST_CASE("vm uses stdlib File string helper wrappers") {
  const std::string filePath =
      (std::filesystem::temp_directory_path() / "primec_vm_stdlib_file_string_helpers.txt").string();
  const auto escape = [](const std::string &text) {
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
  const std::string source = R"(
import /std/file/*

[return<Result<FileError>> effects(file_write) on_error<FileError, /log_file_error>]
main() {
  [File<Write>] file{ File<Write>("__PATH__"utf8)? }
  [string] text{"alpha"utf8}
  /File/write<Write, string>(file, text)?
  /File/write_line<Write, string>(file, "omega"utf8)?
  file.close()?
  return(Result.ok())
}

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(FileError.why(err))
}
)";
  std::string program = source;
  const std::string placeholder = "__PATH__";
  const size_t pathPos = program.find(placeholder);
  REQUIRE(pathPos != std::string::npos);
  program.replace(pathPos, placeholder.size(), escape(filePath));
  const std::string srcPath = writeTemp("vm_stdlib_file_string_helpers.prime", program);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(filePath) == "alphaomega\n");
}

TEST_CASE("vm uses stdlib File helper wrappers and broader fallback arities") {
  const std::string filePath =
      (std::filesystem::temp_directory_path() / "primec_vm_stdlib_file_broad_helpers.txt").string();
  const auto escape = [](const std::string &text) {
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
  const std::string source = R"(
import /std/file/*

[return<Result<FileError>> effects(file_write) on_error<FileError, /log_file_error>]
main() {
  [File<Write>] file{ File<Write>("__PATH__"utf8)? }
  /File/write<Write, string, i32, string, i32, string, i32, string, i32, string, i32>(
      file,
      "prefix"utf8,
      1i32,
      "-"utf8,
      2i32,
      "-"utf8,
      3i32,
      "="utf8,
      4i32,
      "."utf8,
      5i32)?
  /File/write_line<Write, string, i32, string, i32, string, i32, string, i32, string, i32>(
      file,
      "alpha"utf8,
      7i32,
      "omega"utf8,
      8i32,
      "delta"utf8,
      9i32,
      "!"utf8,
      10i32,
      "?"utf8,
      11i32)?
  file.write("left"utf8, 1i32, "mid"utf8, 2i32, "right"utf8, 3i32, "."utf8, 4i32, "!"utf8, 5i32)?
  file.write_line(4i32, " "utf8, 5i32, " "utf8, 6i32, "?"utf8, 7i32, "!"utf8, 8i32, "#"utf8)?
  file.close()?
  return(Result.ok())
}

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(FileError.why(err))
}
)";
  std::string program = source;
  const std::string placeholder = "__PATH__";
  const size_t pathPos = program.find(placeholder);
  REQUIRE(pathPos != std::string::npos);
  program.replace(pathPos, placeholder.size(), escape(filePath));
  const std::string srcPath = writeTemp("vm_stdlib_file_broad_helpers.prime", program);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(filePath) == "prefix1-2-3=4.5alpha7omega8delta9!10?11\nleft1mid2right3.4!54 5 6?7!8#\n");
}

TEST_CASE("vm resolves templated helper overload families by exact arity") {
  const std::string source = R"(
[struct]
Marker() {}

[return<i32>]
/helper/value<T>([T] value) {
  return(1i32)
}

[return<i32>]
/helper/value<A, B>([A] first, [B] second) {
  return(2i32)
}

[return<i32>]
/Marker/mark<T>([Marker] self, [T] value) {
  return(/helper/value(value))
}

[return<i32>]
/Marker/mark<A, B>([Marker] self, [A] first, [B] second) {
  return(/helper/value(first, second))
}

[effects(io_out), return<int>]
main() {
  [Marker] marker{Marker{}}
  print_line(/helper/value(9i32))
  print_line(/helper/value(9i32, true))
  print_line(marker.mark(7i32))
  print_line(marker.mark(7i32, false))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_helper_overload_families.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_helper_overload_families_out.txt").string();

  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "1\n2\n1\n2\n");
}

TEST_CASE("vm supports graphics-style int return propagation with on_error") {
  const std::string source = R"(
[struct]
GfxError() {
  [i32] code{0i32}
}

namespace GfxError {
  [return<string>]
  why([GfxError] err) {
    return(if(equal(err.code, 7i32), then() { "frame_acquire_failed"utf8 }, else() { "queue_submit_failed"utf8 }))
  }
}

[struct]
Frame() {
  [i32] token{0i32}
}

[return<Result<i32, GfxError>>]
acquire_frame_ok() {
  return(Result.ok(9i32))
}

[return<Result<i32, GfxError>>]
acquire_frame_fail() {
  return(7i64)
}

[return<Result<GfxError>>]
submit_frame([i32] token) {
  return(Result.ok())
}

[effects(io_err)]
log_gfx_error([GfxError] err) {
  print_line_error(err.why())
}

[return<int> on_error<GfxError, /log_gfx_error>]
main_ok() {
  [i32] frameToken{acquire_frame_ok()?}
  submit_frame(frameToken)?
  return(frameToken)
}

[return<int> effects(io_err) on_error<GfxError, /log_gfx_error>]
main_fail() {
  [i32] frameToken{acquire_frame_fail()?}
  return(frameToken)
}
)";
  const std::string srcPath = writeTemp("vm_graphics_int_on_error.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_graphics_int_on_error_err.txt").string();
  const std::string okCmd = "./primec --emit=vm " + srcPath + " --entry /main_ok";
  const std::string failCmd = "./primec --emit=vm " + srcPath + " --entry /main_fail 2> " + errPath;
  CHECK(runCommand(okCmd) == 9);
  CHECK(runCommand(failCmd) == 7);
  CHECK(readFile(errPath) == "frame_acquire_failed\n");
}

TEST_CASE("vm supports string Result.ok payloads through try") {
  const std::string source = R"(
[struct]
ParseError() {
  [i32] code{0i32}
}

namespace ParseError {
  [return<string>]
  why([ParseError] err) {
    return("parse failed"utf8)
  }
}

[return<Result<string, ParseError>>]
greeting() {
  return(Result.ok("alpha"utf8))
}

[effects(io_err)]
log_parse_error([ParseError] err) {
  print_line_error(err.why())
}

[return<int> effects(io_out, io_err) on_error<ParseError, /log_parse_error>]
main() {
  [string] text{greeting()?}
  print_line(text)
  return(count(text))
}
)";
  const std::string srcPath = writeTemp("vm_result_ok_string.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_vm_result_ok_string_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 5);
  CHECK(readFile(outPath) == "alpha\n");
}

TEST_CASE("vm supports direct string Result combinator consumers") {
  const std::string source = R"(
[struct]
ParseError() {
  [i32] code{0i32}
}

namespace ParseError {
  [return<string>]
  why([ParseError] err) {
    return("parse failed"utf8)
  }
}

swallow_parse_error([ParseError] err) {}

[return<int> effects(io_out) on_error<ParseError, /swallow_parse_error>]
main() {
  print_line(try(Result.map(Result.ok("alpha"utf8), []([string] value) { return(value) })))
  print_line(try(Result.and_then(Result.ok("beta"utf8), []([string] value) { return(Result.ok(value)) })))
  print_line(try(Result.map2(Result.ok("gamma"utf8), Result.ok("delta"utf8), []([string] left, [string] right) {
    return(left)
  })))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_result_combinators_string.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_result_combinators_string_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "alpha\nbeta\ngamma\n");
}

TEST_CASE("vm supports definition-backed string Result combinator sources") {
  const std::string source = R"(
[struct]
ParseError() {
  [i32] code{0i32}
}

namespace ParseError {
  [return<string>]
  why([ParseError] err) {
    return("parse failed"utf8)
  }
}

[struct]
Reader() {
  [i32] marker{0i32}
}

[return<Result<string, ParseError>>]
greeting() {
  return(Result.ok("alpha"utf8))
}

[return<Result<string, ParseError>>]
/Reader/read([Reader] self) {
  return(Result.ok("beta"utf8))
}

swallow_parse_error([ParseError] err) {}

[return<int> effects(io_out) on_error<ParseError, /swallow_parse_error>]
main() {
  [Reader] reader{Reader{}}
  print_line(try(Result.map(greeting(), []([string] value) { return(value) })))
  print_line(try(Result.and_then(reader.read(), []([string] value) { return(Result.ok(value)) })))
  print_line(try(Result.map2(greeting(), reader.read(), []([string] left, [string] right) { return(left) })))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_result_combinators_string_definition_sources.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_result_combinators_string_definition_sources_out.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "alpha\nbeta\nalpha\n");
}



TEST_SUITE_END();
