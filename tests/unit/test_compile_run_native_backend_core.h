#include <cerrno>

#if defined(__APPLE__) && (defined(__arm64__) || defined(__aarch64__))
TEST_SUITE_BEGIN("primestruct.compile.run.native_backend.core");
TEST_CASE("compiles and runs native file io") {
  const std::string outPath = (testScratchPath("") / "primec_native_file_io.txt").string();
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
  const std::string srcPath = writeTemp("native_file_io.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_file_io_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
  CHECK(readFile(outPath) == "Hello 123 world\n\nABC");
}

TEST_CASE("compiles and runs native file read_byte with deterministic eof") {
  const std::string inPath = (testScratchPath("") / "primec_native_file_read_byte.txt").string();
  {
    std::ofstream file(inPath, std::ios::binary);
    REQUIRE(file.good());
    file.write("AB", 2);
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
  const std::string escapedPath = escape(inPath);
  const std::string source =
      "[return<Result<FileError>> effects(file_read, io_out) on_error<FileError, /log_file_error>]\n"
      "main() {\n"
      "  [File<Read>] file{ File<Read>(\"" + escapedPath + "\"utf8)? }\n"
      "  [i32 mut] first{0i32}\n"
      "  [i32 mut] second{0i32}\n"
      "  [i32 mut] third{99i32}\n"
      "  file.read_byte(first)?\n"
      "  file.read_byte(second)?\n"
      "  print_line(first)\n"
      "  print_line(second)\n"
      "  print_line(Result.why(file.read_byte(third)))\n"
      "  print_line(third)\n"
      "  file.close()?\n"
      "  return(Result.ok())\n"
      "}\n"
      "[effects(io_err)]\n"
      "log_file_error([FileError] err) {\n"
      "  print_line_error(\"file error\"utf8)\n"
      "}\n";
  const std::string srcPath = writeTemp("native_file_read_byte.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_file_read_byte_exe").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_file_read_byte_out.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 0);
  CHECK(readFile(outPath) == "65\n66\nEOF\n99\n");
}

TEST_CASE("native uses stdlib File helper wrappers") {
  const std::string outPath =
      (testScratchPath("") / "primec_native_stdlib_file_helpers.txt").string();
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
      "import /std/file/*\n"
      "[return<Result<FileError>> effects(file_write) on_error<FileError, /log_file_error>]\n"
      "main() {\n"
      "  [File<Write>] file{ File<Write>(\"" + escapedPath + "\"utf8)? }\n"
      "  [array<i32>] bytes{ array<i32>(68i32, 69i32) }\n"
      "  /File/write<Write, i32>(file, 65i32)?\n"
      "  /File/write_line<Write, i32>(file, 66i32)?\n"
      "  file.write_byte(67i32)?\n"
      "  /File/write_bytes(file, bytes)?\n"
      "  /File/flush(file)?\n"
      "  file.close()?\n"
      "  return(Result.ok())\n"
      "}\n"
      "[effects(io_err)]\n"
      "log_file_error([FileError] err) {\n"
      "  print_line_error(Result.why(fileErrorStatus(err)))\n"
      "}\n";
  const std::string srcPath = writeTemp("native_stdlib_file_helpers.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_stdlib_file_helpers_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
  CHECK(readFile(outPath) == "6566\nCDE");
}

TEST_CASE("native stdlib File close helper disarms the original handle") {
  const std::string filePath =
      (testScratchPath("") / "primec_native_stdlib_file_close_helper.txt").string();
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
      "[effects(file_write), return<Result<FileError>> on_error<FileError, /log_file_error>]\n"
      "main() {\n"
      "  [File<Write>] file{ File<Write>(\"" + escapedPath + "\"utf8)? }\n"
      "  /File/write<Write, string>(file, \"alpha\"utf8)?\n"
      "  file.close()?\n"
      "  /File/write<Write, string>(file, \"beta\"utf8)\n"
      "  return(Result.ok())\n"
      "}\n"
      "[effects(io_err)]\n"
      "log_file_error([FileError] err) {\n"
      "  print_line_error(Result.why(fileErrorStatus(err)))\n"
      "}\n";
  const std::string srcPath = writeTemp("native_stdlib_file_close_helper.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_stdlib_file_close_helper_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
  CHECK(readFile(filePath) == "alpha");
}

TEST_CASE("native uses stdlib File string helper wrappers") {
  const std::string outPath =
      (testScratchPath("") / "primec_native_stdlib_file_string_helpers.txt").string();
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
      "import /std/file/*\n"
      "[return<Result<FileError>> effects(file_write) on_error<FileError, /log_file_error>]\n"
      "main() {\n"
      "  [File<Write>] file{ File<Write>(\"" + escapedPath + "\"utf8)? }\n"
      "  [string] text{\"alpha\"utf8}\n"
      "  /File/write<Write, string>(file, text)?\n"
      "  /File/write_line<Write, string>(file, \"omega\"utf8)?\n"
      "  file.close()?\n"
      "  return(Result.ok())\n"
      "}\n"
      "[effects(io_err)]\n"
      "log_file_error([FileError] err) {\n"
      "  print_line_error(Result.why(fileErrorStatus(err)))\n"
      "}\n";
  const std::string srcPath = writeTemp("native_stdlib_file_string_helpers.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_stdlib_file_string_helpers_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
  CHECK(readFile(outPath) == "alphaomega\n");
}

TEST_CASE("native uses stdlib File multi-value helper wrappers") {
  const std::string outPath =
      (testScratchPath("") / "primec_native_stdlib_file_multi_helpers.txt").string();
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
      "import /std/file/*\n"
      "[return<Result<FileError>> effects(file_write) on_error<FileError, /log_file_error>]\n"
      "main() {\n"
      "  [File<Write>] file{ File<Write>(\"" + escapedPath + "\"utf8)? }\n"
      "  /File/write<Write, string, i32, string>(file, \"alpha\"utf8, 7i32, \"omega\"utf8)?\n"
      "  /File/write_line<Write, i32, string, i32, string, i32>(file, 255i32, \" \"utf8, 0i32, \" \"utf8, 0i32)?\n"
      "  file.write(\"left\"utf8, 1i32, \"right\"utf8)?\n"
      "  file.write_line()?\n"
      "  file.close()?\n"
      "  return(Result.ok())\n"
      "}\n"
      "[effects(io_err)]\n"
      "log_file_error([FileError] err) {\n"
      "  print_line_error(Result.why(fileErrorStatus(err)))\n"
      "}\n";
  const std::string srcPath = writeTemp("native_stdlib_file_multi_helpers.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_stdlib_file_multi_helpers_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
  CHECK(readFile(outPath) == "alpha7omega255 0 0\nleft1right\n");
}

TEST_CASE("native resolves templated helper overload families by exact arity") {
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
  [Marker] marker{Marker()}
  print_line(/helper/value(9i32))
  print_line(/helper/value(9i32, true))
  print_line(marker.mark(7i32))
  print_line(marker.mark(7i32, false))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("native_helper_overload_families.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_helper_overload_families_exe").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_helper_overload_families_out.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 0);
  CHECK(readFile(outPath) == "1\n2\n1\n2\n");
}

TEST_CASE("compiles and runs native mutable scalar helper copy-back") {
  const std::string source = R"(
[return<void>]
set42([i32 mut] value) {
  assign(value, 42i32)
}

[effects(io_out), return<int>]
main() {
  [i32 mut] value{7i32}
  set42(value)
  print_line(value)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_mut_scalar_param.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_mut_scalar_param").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_mut_scalar_param.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 0);
  CHECK(readFile(outPath) == "42\n");
}

TEST_CASE("compiles and runs native executable") {
  const std::string source = R"(
[return<int>]
main() {
  return(7i32)
}
)";
  const std::string srcPath = writeTemp("compile_native.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("native supports support-matrix binding types") {
  const std::string source = R"(
import /std/collections/*

[return<int> effects(heap_alloc)]
main() {
  [i32] i{1i32}
  [i64] j{2i64}
  [u64] k{3u64}
  [bool] b{true}
  [f32] x{1.5f32}
  [f64] y{2.5f64}
  [array<i32>] arr{array<i32>(1i32, 2i32, 3i32)}
  [vector<i32>] vec{vector<i32>(4i32, 5i32)}
  [map<i32, i32>] table{map<i32, i32>(1i32, 7i32)}
  [string] text{"ok"utf8}
  return(plus(
    plus(
      plus(convert<int>(i), convert<int>(j)),
      plus(convert<int>(k), if(b, then() { 1i32 }, else() { 0i32 }))
    ),
    plus(
      plus(convert<int>(x), convert<int>(y)),
      plus(count(arr), plus(vectorCount<i32>(vec), plus(mapCount<i32, i32>(table), count(text))))
    )
  ))
}
)";
  const std::string srcPath = writeTemp("compile_native_support_matrix_types.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_support_matrix_types").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 18);
}

TEST_CASE("native materializes direct variadic args packs") {
  const std::string source = R"(
[return<int>]
score([i32] head, [args<i32>] values) {
  return(plus(head, plus(count(values), values[1i32])))
}

[return<int>]
main() {
  return(score(10i32, 20i32, 30i32, 40i32))
}
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_direct.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_direct").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 43);
}

TEST_CASE("native materializes string variadic args packs and pure spread forwarding") {
  const std::string source = R"(
[return<int>]
pack_score([args<string>] values) {
  return(plus(count(values), values[1i32].count()))
}

[return<int>]
forward([args<string>] values) {
  return(pack_score([spread] values))
}

[return<int>]
main() {
  return(forward("a"raw_utf8, "bbb"raw_utf8))
}
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_string.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_string").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 5);
}

TEST_CASE("native materializes mixed explicit and spread variadic forwarding") {
  const std::string source = R"(
[return<int>]
count_values([args<i32>] values) {
  return(plus(values[0i32], values[2i32]))
}

[return<int>]
forward([args<i32>] values) {
  return(count_values(99i32, [spread] values))
}

[return<int>]
main() {
  return(forward(1i32, 2i32))
}
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_mixed_spread.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_mixed_spread").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 101);
}

TEST_CASE("native materializes direct struct variadic args packs for count") {
  const std::string source = R"(
[struct]
Pair() {
  [i32] value{0i32}
}

[return<int>]
count_values([args<Pair>] values) {
  return(count(values))
}

[return<int>]
main() {
  return(count_values(Pair(), Pair()))
}
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_struct_count.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_struct_count").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("native forwards pure spread struct variadic packs for count") {
  const std::string source = R"(
[struct]
Pair() {
  [i32] value{0i32}
}

[return<int>]
count_values([args<Pair>] values) {
  return(count(values))
}

[return<int>]
forward([args<Pair>] values) {
  return(count_values([spread] values))
}

[return<int>]
main() {
  return(forward(Pair(), Pair()))
}
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_struct_spread.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_struct_spread").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("native materializes mixed struct spread variadic forwarding for count") {
  const std::string source = R"(
[struct]
Pair() {
  [i32] value{0i32}
}

[return<int>]
count_values([args<Pair>] values) {
  return(count(values))
}

[return<int>]
forward([args<Pair>] values) {
  return(count_values(Pair(), [spread] values))
}

[return<int>]
main() {
  return(forward(Pair(), Pair()))
}
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_struct_mixed.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_struct_mixed").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("native materializes direct struct variadic pack indexing and method access") {
  const std::string source = R"(
[struct]
Pair() {
  [i32] value{7i32}
}

[return<int>]
/Pair/score([Pair] self) {
  return(plus(self.value, 1i32))
}

[return<int>]
score_pairs([args<Pair>] values) {
  [Pair] head{at(values, 0i32)}
  [Pair] tail{at(values, 1i32)}
  return(plus(head.value, tail.score()))
}

[return<int>]
main() {
  return(score_pairs(Pair(7i32), Pair(9i32)))
}
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_struct_index.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_struct_index").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 17);
}

TEST_CASE("native forwards pure spread struct pack indexing and method access") {
  const std::string source = R"(
[struct]
Pair() {
  [i32] value{7i32}
}

[return<int>]
/Pair/score([Pair] self) {
  return(plus(self.value, 1i32))
}

[return<int>]
score_pairs([args<Pair>] values) {
  return(plus(values[0i32].value, values[1i32].score()))
}

[return<int>]
forward([args<Pair>] values) {
  return(score_pairs([spread] values))
}

[return<int>]
main() {
  return(forward(Pair(7i32), Pair(9i32)))
}
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_struct_access_spread.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_struct_access_spread").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 17);
}

TEST_CASE("native materializes mixed struct pack indexing and method access") {
  const std::string source = R"(
[struct]
Pair() {
  [i32] value{7i32}
}

[return<int>]
/Pair/score([Pair] self) {
  return(plus(self.value, 1i32))
}

[return<int>]
score_pairs([args<Pair>] values) {
  return(plus(values[0i32].value, values[2i32].score()))
}

[return<int>]
forward([args<Pair>] values) {
  return(score_pairs(Pair(5i32), [spread] values))
}

[return<int>]
main() {
  return(forward(Pair(7i32), Pair(9i32)))
}
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_struct_access_mixed.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_struct_access_mixed").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 15);
}

TEST_CASE("native materializes variadic Result packs with indexed why and try access") {
  const std::string source = R"(
[struct]
ParseError() {
  [i32] code{0i32}
}

namespace ParseError {
  [return<string>]
  why([ParseError] err) {
    return(if(equal(err.code, 7i32), then() { "bad"utf8 }, else() { "other"utf8 }))
  }
}

swallow_parse_error([ParseError] err) {}

[return<Result<i32, ParseError>>]
ok_value([i32] value) {
  return(Result.ok(value))
}

[return<Result<i32, ParseError>>]
fail_bad() {
  return(7i64)
}

[return<int> on_error<ParseError, /swallow_parse_error>]
score_results([args<Result<i32, ParseError>>] values) {
  [i32] head{try(values[0i32])}
  [i32] tailWhyCount{count(Result.why(values[minus(count(values), 1i32)]))}
  return(plus(head, tailWhyCount))
}

[return<int>]
forward([args<Result<i32, ParseError>>] values) {
  return(score_results([spread] values))
}

[return<int>]
forward_mixed([args<Result<i32, ParseError>>] values) {
  return(score_results(ok_value(10i32), [spread] values))
}

[return<int>]
main() {
  return(plus(score_results(ok_value(2i32), fail_bad()),
              plus(forward(ok_value(3i32), fail_bad()),
                   forward_mixed(ok_value(4i32), fail_bad()))))
}
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_result.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_result").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 30);
}

TEST_CASE("native materializes variadic borrowed Result packs with indexed dereference why and try access") {
  const std::string source = R"(
[struct]
ParseError() {
  [i32] code{0i32}
}

namespace ParseError {
  [return<string>]
  why([ParseError] err) {
    return(if(equal(err.code, 7i32), then() { "bad"utf8 }, else() { "other"utf8 }))
  }
}

swallow_parse_error([ParseError] err) {}

[return<Result<i32, ParseError>>]
ok_value([i32] value) {
  return(Result.ok(value))
}

[return<Result<i32, ParseError>>]
fail_bad() {
  return(7i64)
}

[return<int> on_error<ParseError, /swallow_parse_error>]
score_results([args<Reference<Result<i32, ParseError>>>] values) {
  [i32] head{try(dereference(values[0i32]))}
  [i32] tailWhyCount{count(Result.why(dereference(values[minus(count(values), 1i32)])))}
  return(plus(head, tailWhyCount))
}

[return<int>]
forward([args<Reference<Result<i32, ParseError>>>] values) {
  return(score_results([spread] values))
}

[return<int>]
forward_mixed([args<Reference<Result<i32, ParseError>>>] values) {
  [Result<i32, ParseError>] extra{ok_value(10i32)}
  [Reference<Result<i32, ParseError>>] extra_ref{location(extra)}
  return(score_results(extra_ref, [spread] values))
}

[return<int>]
main() {
  [Result<i32, ParseError>] a0{ok_value(2i32)}
  [Result<i32, ParseError>] a1{fail_bad()}
  [Reference<Result<i32, ParseError>>] r0{location(a0)}
  [Reference<Result<i32, ParseError>>] r1{location(a1)}

  [Result<i32, ParseError>] b0{ok_value(3i32)}
  [Result<i32, ParseError>] b1{fail_bad()}
  [Reference<Result<i32, ParseError>>] s0{location(b0)}
  [Reference<Result<i32, ParseError>>] s1{location(b1)}

  [Result<i32, ParseError>] c0{ok_value(4i32)}
  [Result<i32, ParseError>] c1{fail_bad()}
  [Reference<Result<i32, ParseError>>] t0{location(c0)}
  [Reference<Result<i32, ParseError>>] t1{location(c1)}

  return(plus(score_results(r0, r1),
              plus(forward(s0, s1),
                   forward_mixed(t0, t1))))
}
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_borrowed_result.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_borrowed_result").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 30);
}

TEST_CASE("native materializes variadic pointer Result packs with indexed dereference why and inferred try access") {
  const std::string source = R"(
[struct]
ParseError() {
  [i32] code{0i32}
}

namespace ParseError {
  [return<string>]
  why([ParseError] err) {
    return(if(equal(err.code, 7i32), then() { "bad"utf8 }, else() { "other"utf8 }))
  }
}

swallow_parse_error([ParseError] err) {}

[return<Result<i32, ParseError>>]
ok_value([i32] value) {
  return(Result.ok(value))
}

[return<Result<i32, ParseError>>]
fail_bad() {
  return(7i64)
}

[return<int> on_error<ParseError, /swallow_parse_error>]
score_results([args<Pointer<Result<i32, ParseError>>>] values) {
  [auto] head{try(dereference(values[0i32]))}
  [i32] tailWhyCount{count(Result.why(dereference(values[minus(count(values), 1i32)])))}
  return(plus(head, tailWhyCount))
}

[return<int>]
forward([args<Pointer<Result<i32, ParseError>>>] values) {
  return(score_results([spread] values))
}

[return<int>]
forward_mixed([args<Pointer<Result<i32, ParseError>>>] values) {
  [Result<i32, ParseError>] extra{ok_value(10i32)}
  [Pointer<Result<i32, ParseError>>] extra_ptr{location(extra)}
  return(score_results(extra_ptr, [spread] values))
}

[return<int>]
main() {
  [Result<i32, ParseError>] a0{ok_value(2i32)}
  [Result<i32, ParseError>] a1{fail_bad()}
  [Pointer<Result<i32, ParseError>>] r0{location(a0)}
  [Pointer<Result<i32, ParseError>>] r1{location(a1)}

  [Result<i32, ParseError>] b0{ok_value(3i32)}
  [Result<i32, ParseError>] b1{fail_bad()}
  [Pointer<Result<i32, ParseError>>] s0{location(b0)}
  [Pointer<Result<i32, ParseError>>] s1{location(b1)}

  [Result<i32, ParseError>] c0{ok_value(4i32)}
  [Result<i32, ParseError>] c1{fail_bad()}
  [Pointer<Result<i32, ParseError>>] t0{location(c0)}
  [Pointer<Result<i32, ParseError>>] t1{location(c1)}

  return(plus(score_results(r0, r1),
              plus(forward(s0, s1),
                   forward_mixed(t0, t1))))
}
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_pointer_result.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_pointer_result").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 30);
}

TEST_CASE("native materializes variadic status-only Result packs with indexed error and why access") {
  const std::string source = R"(
[struct]
ParseError() {
  [i32] code{0i32}
}

namespace ParseError {
  [return<string>]
  why([ParseError] err) {
    return(if(equal(err.code, 7i32), then() { "bad"utf8 }, else() { "other"utf8 }))
  }
}

[return<Result<ParseError>>]
ok_status() {
  return(Result.ok())
}

[return<Result<ParseError>>]
fail_bad() {
  return(7i32)
}

[return<int>]
score_results([args<Result<ParseError>>] values) {
  [auto] tailHasError{Result.error(values[minus(count(values), 1i32)])}
  [i32] tailWhyCount{count(Result.why(values[minus(count(values), 1i32)]))}
  return(if(tailHasError, then() { plus(10i32, tailWhyCount) }, else() { 0i32 }))
}

[return<int>]
forward([args<Result<ParseError>>] values) {
  return(score_results([spread] values))
}

[return<int>]
forward_mixed([args<Result<ParseError>>] values) {
  return(score_results(ok_status(), [spread] values))
}

[return<int>]
main() {
  return(plus(score_results(ok_status(), fail_bad()),
              plus(forward(ok_status(), fail_bad()),
                   forward_mixed(ok_status(), fail_bad()))))
}
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_status_result.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_status_result").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 39);
}

TEST_CASE("native materializes variadic borrowed status-only Result packs with indexed dereference error and why access") {
  const std::string source = R"(
[struct]
ParseError() {
  [i32] code{0i32}
}

namespace ParseError {
  [return<string>]
  why([ParseError] err) {
    return(if(equal(err.code, 7i32), then() { "bad"utf8 }, else() { "other"utf8 }))
  }
}

[return<Result<ParseError>>]
ok_status() {
  return(Result.ok())
}

[return<Result<ParseError>>]
fail_bad() {
  return(7i32)
}

[return<int>]
score_results([args<Reference<Result<ParseError>>>] values) {
  [auto] tailHasError{Result.error(dereference(values[minus(count(values), 1i32)]))}
  [i32] tailWhyCount{count(Result.why(dereference(values[minus(count(values), 1i32)])))}
  return(if(tailHasError, then() { plus(10i32, tailWhyCount) }, else() { 0i32 }))
}

[return<int>]
forward([args<Reference<Result<ParseError>>>] values) {
  return(score_results([spread] values))
}

[return<int>]
forward_mixed([args<Reference<Result<ParseError>>>] values) {
  [Result<ParseError>] extra{ok_status()}
  [Reference<Result<ParseError>>] extra_ref{location(extra)}
  return(score_results(extra_ref, [spread] values))
}

[return<int>]
main() {
  [Result<ParseError>] a0{ok_status()}
  [Result<ParseError>] a1{fail_bad()}
  [Reference<Result<ParseError>>] r0{location(a0)}
  [Reference<Result<ParseError>>] r1{location(a1)}

  [Result<ParseError>] b0{ok_status()}
  [Result<ParseError>] b1{fail_bad()}
  [Reference<Result<ParseError>>] s0{location(b0)}
  [Reference<Result<ParseError>>] s1{location(b1)}

  [Result<ParseError>] c0{ok_status()}
  [Result<ParseError>] c1{fail_bad()}
  [Reference<Result<ParseError>>] t0{location(c0)}
  [Reference<Result<ParseError>>] t1{location(c1)}

  return(plus(score_results(r0, r1),
              plus(forward(s0, s1),
                   forward_mixed(t0, t1))))
}
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_borrowed_status_result.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_borrowed_status_result").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 39);
}

TEST_CASE("native materializes variadic pointer status-only Result packs with indexed dereference error and why access") {
  const std::string source = R"(
[struct]
ParseError() {
  [i32] code{0i32}
}

namespace ParseError {
  [return<string>]
  why([ParseError] err) {
    return(if(equal(err.code, 7i32), then() { "bad"utf8 }, else() { "other"utf8 }))
  }
}

[return<Result<ParseError>>]
ok_status() {
  return(Result.ok())
}

[return<Result<ParseError>>]
fail_bad() {
  return(7i32)
}

[return<int>]
score_results([args<Pointer<Result<ParseError>>>] values) {
  [auto] tailHasError{Result.error(dereference(values[minus(count(values), 1i32)]))}
  [i32] tailWhyCount{count(Result.why(dereference(values[minus(count(values), 1i32)])))}
  return(if(tailHasError, then() { plus(10i32, tailWhyCount) }, else() { 0i32 }))
}

[return<int>]
forward([args<Pointer<Result<ParseError>>>] values) {
  return(score_results([spread] values))
}

[return<int>]
forward_mixed([args<Pointer<Result<ParseError>>>] values) {
  [Result<ParseError>] extra{ok_status()}
  [Pointer<Result<ParseError>>] extra_ptr{location(extra)}
  return(score_results(extra_ptr, [spread] values))
}

[return<int>]
main() {
  [Result<ParseError>] a0{ok_status()}
  [Result<ParseError>] a1{fail_bad()}
  [Pointer<Result<ParseError>>] r0{location(a0)}
  [Pointer<Result<ParseError>>] r1{location(a1)}

  [Result<ParseError>] b0{ok_status()}
  [Result<ParseError>] b1{fail_bad()}
  [Pointer<Result<ParseError>>] s0{location(b0)}
  [Pointer<Result<ParseError>>] s1{location(b1)}

  [Result<ParseError>] c0{ok_status()}
  [Result<ParseError>] c1{fail_bad()}
  [Pointer<Result<ParseError>>] t0{location(c0)}
  [Pointer<Result<ParseError>>] t1{location(c1)}

  return(plus(score_results(r0, r1),
              plus(forward(s0, s1),
                   forward_mixed(t0, t1))))
}
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_pointer_status_result.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_pointer_status_result").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 39);
}

TEST_CASE("native materializes variadic vector packs with indexed count methods") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
score_vectors([args<vector<i32>>] values) {
  [vector<i32>] head{values[0i32]}
  [vector<i32>] tail{values[2i32]}
  return(plus(/std/collections/vector/count(head),
              /std/collections/vector/count(tail)))
}

[return<int>]
forward([args<vector<i32>>] values) {
  return(score_vectors([spread] values))
}

[effects(heap_alloc), return<int>]
forward_mixed([args<vector<i32>>] values) {
  [vector<i32>] extra{vector<i32>(1i32, 99i32)}
  return(score_vectors(extra, [spread] values))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] a0{vector<i32>(1i32, 2i32)}
  [vector<i32>] a1{vector<i32>(3i32, 4i32)}
  [vector<i32>] a2{vector<i32>(4i32, 5i32, 6i32)}
  [vector<i32>] b0{vector<i32>(7i32, 8i32)}
  [vector<i32>] b1{vector<i32>(8i32, 9i32)}
  [vector<i32>] b2{vector<i32>(10i32, 11i32)}
  [vector<i32>] c0{vector<i32>(11i32, 12i32)}
  [vector<i32>] c1{vector<i32>(13i32, 14i32, 15i32)}
  return(plus(score_vectors(a0, a1, a2),
              plus(forward(b0, b1, b2),
                   forward_mixed(c0, c1))))
}
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_vector.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_vector").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 14);
}

TEST_CASE("native materializes variadic vector packs with indexed capacity methods") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
score_vectors([args<vector<i32>>] values) {
  [vector<i32>] head{values[0i32]}
  [vector<i32>] tail{values[2i32]}
  return(plus(/std/collections/vector/capacity(head),
              /std/collections/vector/capacity(tail)))
}

[return<int>]
forward([args<vector<i32>>] values) {
  return(score_vectors([spread] values))
}

[effects(heap_alloc), return<int>]
forward_mixed([args<vector<i32>>] values) {
  [vector<i32>] extra{vector<i32>(1i32, 99i32)}
  return(score_vectors(extra, [spread] values))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] a0{vector<i32>(1i32, 2i32)}
  [vector<i32>] a1{vector<i32>(3i32, 4i32)}
  [vector<i32>] a2{vector<i32>(4i32, 5i32, 6i32)}
  [vector<i32>] b0{vector<i32>(7i32, 8i32)}
  [vector<i32>] b1{vector<i32>(8i32, 9i32)}
  [vector<i32>] b2{vector<i32>(10i32, 11i32)}
  [vector<i32>] c0{vector<i32>(11i32, 12i32)}
  [vector<i32>] c1{vector<i32>(13i32, 14i32, 15i32)}
  return(plus(score_vectors(a0, a1, a2),
              plus(forward(b0, b1, b2),
                   forward_mixed(c0, c1))))
}
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_vector_capacity.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_vector_capacity").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 14);
}

TEST_CASE("native materializes variadic vector packs with indexed statement mutators") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
mutate_vectors([args<vector<i32>>] values) {
  [vector<i32> mut] head{values[0i32]}
  [vector<i32> mut] middle{values[1i32]}
  [vector<i32> mut] tail{values[2i32]}
  head.push(9i32)
  head.pop()
  middle.reserve(6i32)
  middle.clear()
  tail.remove_at(1i32)
  tail.remove_swap(0i32)
  return(plus(/std/collections/vector/count(head),
              plus(/std/collections/vector/capacity(middle),
                   /std/collections/vector/count(tail))))
}

[effects(heap_alloc), return<int>]
forward([args<vector<i32>>] values) {
  return(mutate_vectors([spread] values))
}

[effects(heap_alloc), return<int>]
forward_mixed([args<vector<i32>>] values) {
  [vector<i32>] extra{vectorSingle<i32>(20i32)}
  return(mutate_vectors(extra, [spread] values))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] a0{vector<i32>(1i32, 2i32)}
  [vector<i32>] a1{vectorSingle<i32>(3i32)}
  [vector<i32>] a2{vector<i32>(4i32, 5i32, 6i32)}

  [vector<i32>] b0{vectorSingle<i32>(7i32)}
  [vector<i32>] b1{vector<i32>(8i32, 9i32)}
  [vector<i32>] b2{vector<i32>(10i32, 11i32, 12i32)}

  [vector<i32>] c0{vector<i32>(13i32, 14i32)}
  [vector<i32>] c1{vector<i32>(15i32, 16i32, 17i32)}

  return(plus(mutate_vectors(a0, a1, a2),
              plus(forward(b0, b1, b2),
                   forward_mixed(c0, c1))))
}
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_vector_mutators.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_vector_mutators").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 25);
}

TEST_CASE("native materializes variadic array packs with indexed count methods") {
  const std::string source = R"(
[return<int>]
score_arrays([args<array<i32>>] values) {
  return(plus(values[0i32].count(), values[2i32].count()))
}

[return<int>]
forward([args<array<i32>>] values) {
  return(score_arrays([spread] values))
}

[return<int>]
forward_mixed([args<array<i32>>] values) {
  return(score_arrays(array<i32>(1i32), [spread] values))
}

[return<int>]
main() {
  return(plus(score_arrays(array<i32>(1i32, 2i32), array<i32>(3i32), array<i32>(4i32, 5i32, 6i32)),
              plus(forward(array<i32>(7i32), array<i32>(8i32, 9i32), array<i32>(10i32)),
                   forward_mixed(array<i32>(11i32, 12i32), array<i32>(13i32, 14i32, 15i32)))))
}
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_array.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_array").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 11);
}

TEST_CASE("native materializes variadic borrowed array packs with indexed count methods") {
  const std::string source = R"(
[return<int>]
score_refs([args<Reference<array<i32>>>] values) {
  return(plus(values[0i32].count(), values[2i32].count()))
}

[return<int>]
forward([args<Reference<array<i32>>>] values) {
  return(score_refs([spread] values))
}

[return<int>]
forward_mixed([args<Reference<array<i32>>>] values) {
  [array<i32>] extra{array<i32>(1i32)}
  [Reference<array<i32>>] extra_ref{location(extra)}
  return(score_refs(extra_ref, [spread] values))
}

[return<int>]
main() {
  [array<i32>] a0{array<i32>(1i32, 2i32)}
  [array<i32>] a1{array<i32>(3i32)}
  [array<i32>] a2{array<i32>(4i32, 5i32, 6i32)}
  [Reference<array<i32>>] r0{location(a0)}
  [Reference<array<i32>>] r1{location(a1)}
  [Reference<array<i32>>] r2{location(a2)}

  [array<i32>] b0{array<i32>(7i32)}
  [array<i32>] b1{array<i32>(8i32, 9i32)}
  [array<i32>] b2{array<i32>(10i32)}
  [Reference<array<i32>>] s0{location(b0)}
  [Reference<array<i32>>] s1{location(b1)}
  [Reference<array<i32>>] s2{location(b2)}

  [array<i32>] c0{array<i32>(11i32, 12i32)}
  [array<i32>] c1{array<i32>(13i32, 14i32, 15i32)}
  [Reference<array<i32>>] t0{location(c0)}
  [Reference<array<i32>>] t1{location(c1)}

  return(plus(score_refs(r0, r1, r2),
              plus(forward(s0, s1, s2),
                   forward_mixed(t0, t1))))
}
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_borrowed_array.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_borrowed_array").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 11);
}

TEST_CASE("native materializes variadic borrowed array packs with indexed dereference access helpers") {
  const std::string source = R"(
[return<int>]
score_refs([args<Reference<array<i32>>>] values) {
  [auto] head{at_unsafe(dereference(at(values, 0i32)), 1i32)}
  return(plus(head, dereference(at(values, 2i32)).at(0i32)))
}

[return<int>]
forward([args<Reference<array<i32>>>] values) {
  return(score_refs([spread] values))
}

[return<int>]
forward_mixed([args<Reference<array<i32>>>] values) {
  [array<i32>] extra{array<i32>(1i32, 2i32)}
  [Reference<array<i32>>] extra_ref{location(extra)}
  return(score_refs(extra_ref, [spread] values))
}

[return<int>]
main() {
  [array<i32>] a0{array<i32>(1i32, 2i32)}
  [array<i32>] a1{array<i32>(3i32)}
  [array<i32>] a2{array<i32>(4i32, 5i32, 6i32)}
  [Reference<array<i32>>] r0{location(a0)}
  [Reference<array<i32>>] r1{location(a1)}
  [Reference<array<i32>>] r2{location(a2)}

  [array<i32>] b0{array<i32>(7i32, 8i32)}
  [array<i32>] b1{array<i32>(9i32)}
  [array<i32>] b2{array<i32>(10i32, 11i32)}
  [Reference<array<i32>>] s0{location(b0)}
  [Reference<array<i32>>] s1{location(b1)}
  [Reference<array<i32>>] s2{location(b2)}

  [array<i32>] c0{array<i32>(12i32)}
  [array<i32>] c1{array<i32>(13i32, 14i32, 15i32)}
  [Reference<array<i32>>] t0{location(c0)}
  [Reference<array<i32>>] t1{location(c1)}

  return(plus(score_refs(r0, r1, r2),
              plus(forward(s0, s1, s2),
                   forward_mixed(t0, t1))))
}
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_borrowed_array_deref_access.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_borrowed_array_deref_access").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 39);
}

TEST_CASE("native materializes variadic pointer array packs with indexed count methods") {
  const std::string source = R"(
[return<int>]
score_ptrs([args<Pointer<array<i32>>>] values) {
  return(plus(values[0i32].count(), values[2i32].count()))
}

[return<int>]
forward([args<Pointer<array<i32>>>] values) {
  return(score_ptrs([spread] values))
}

[return<int>]
forward_mixed([args<Pointer<array<i32>>>] values) {
  [array<i32>] extra{array<i32>(1i32)}
  [Pointer<array<i32>>] extra_ptr{location(extra)}
  return(score_ptrs(extra_ptr, [spread] values))
}

[return<int>]
main() {
  [array<i32>] a0{array<i32>(1i32, 2i32)}
  [array<i32>] a1{array<i32>(3i32)}
  [array<i32>] a2{array<i32>(4i32, 5i32, 6i32)}
  [Pointer<array<i32>>] p0{location(a0)}
  [Pointer<array<i32>>] p1{location(a1)}
  [Pointer<array<i32>>] p2{location(a2)}

  [array<i32>] b0{array<i32>(7i32)}
  [array<i32>] b1{array<i32>(8i32, 9i32)}
  [array<i32>] b2{array<i32>(10i32)}
  [Pointer<array<i32>>] q0{location(b0)}
  [Pointer<array<i32>>] q1{location(b1)}
  [Pointer<array<i32>>] q2{location(b2)}

  [array<i32>] c0{array<i32>(11i32, 12i32)}
  [array<i32>] c1{array<i32>(13i32, 14i32, 15i32)}
  [Pointer<array<i32>>] r0{location(c0)}
  [Pointer<array<i32>>] r1{location(c1)}

  return(plus(score_ptrs(p0, p1, p2),
              plus(forward(q0, q1, q2),
                   forward_mixed(r0, r1))))
}
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_pointer_array.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_pointer_array").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 11);
}

TEST_CASE("native materializes variadic pointer array packs with indexed dereference access helpers") {
  const std::string source = R"(
[return<int>]
score_ptrs([args<Pointer<array<i32>>>] values) {
  [auto] head{at_unsafe(dereference(at(values, 0i32)), 1i32)}
  return(plus(head, dereference(at(values, 2i32)).at(0i32)))
}

[return<int>]
forward([args<Pointer<array<i32>>>] values) {
  return(score_ptrs([spread] values))
}

[return<int>]
forward_mixed([args<Pointer<array<i32>>>] values) {
  [array<i32>] extra{array<i32>(1i32, 2i32)}
  [Pointer<array<i32>>] extra_ptr{location(extra)}
  return(score_ptrs(extra_ptr, [spread] values))
}

[return<int>]
main() {
  [array<i32>] a0{array<i32>(1i32, 2i32)}
  [array<i32>] a1{array<i32>(3i32)}
  [array<i32>] a2{array<i32>(4i32, 5i32, 6i32)}
  [Pointer<array<i32>>] p0{location(a0)}
  [Pointer<array<i32>>] p1{location(a1)}
  [Pointer<array<i32>>] p2{location(a2)}

  [array<i32>] b0{array<i32>(7i32, 8i32)}
  [array<i32>] b1{array<i32>(9i32)}
  [array<i32>] b2{array<i32>(10i32, 11i32)}
  [Pointer<array<i32>>] q0{location(b0)}
  [Pointer<array<i32>>] q1{location(b1)}
  [Pointer<array<i32>>] q2{location(b2)}

  [array<i32>] c0{array<i32>(12i32)}
  [array<i32>] c1{array<i32>(13i32, 14i32, 15i32)}
  [Pointer<array<i32>>] r0{location(c0)}
  [Pointer<array<i32>>] r1{location(c1)}

  return(plus(score_ptrs(p0, p1, p2),
              plus(forward(q0, q1, q2),
                   forward_mixed(r0, r1))))
}
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_pointer_array_deref_access.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_pointer_array_deref_access").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 39);
}

TEST_CASE("native materializes variadic scalar reference packs with indexed dereference") {
  const std::string source = R"(
[return<int>]
score_refs([args<Reference<i32>>] values) {
  return(plus(dereference(values[0i32]), dereference(values[2i32])))
}

[return<int>]
forward([args<Reference<i32>>] values) {
  return(score_refs([spread] values))
}

[return<int>]
forward_mixed([args<Reference<i32>>] values) {
  [i32] extra{1i32}
  [Reference<i32>] extra_ref{location(extra)}
  return(score_refs(extra_ref, [spread] values))
}

[return<int>]
main() {
  [i32] a0{1i32}
  [i32] a1{2i32}
  [i32] a2{3i32}

  [i32] b0{4i32}
  [i32] b1{5i32}
  [i32] b2{6i32}

  [i32] c0{7i32}
  [i32] c1{8i32}

  return(plus(score_refs(location(a0), location(a1), location(a2)),
              plus(forward(location(b0), location(b1), location(b2)),
                   forward_mixed(location(c0), location(c1)))))
}
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_scalar_reference.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_scalar_reference").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 23);
}

TEST_CASE("native materializes variadic struct reference packs with indexed field and helper access") {
  const std::string source = R"(
[struct]
Pair() {
  [i32] value{7i32}
}

[return<int>]
/Pair/score([Pair] self) {
  return(plus(self.value, 1i32))
}

[return<int>]
score_refs([args<Reference<Pair>>] values) {
  return(plus(values[0i32].value, values[2i32].score()))
}

[return<int>]
forward([args<Reference<Pair>>] values) {
  return(score_refs([spread] values))
}

[return<int>]
forward_mixed([args<Reference<Pair>>] values) {
  [Pair] extra{Pair(5i32)}
  [Reference<Pair>] extra_ref{location(extra)}
  return(score_refs(extra_ref, [spread] values))
}

[return<int>]
main() {
  [Pair] a0{Pair(7i32)}
  [Pair] a1{Pair(8i32)}
  [Pair] a2{Pair(9i32)}

  [Pair] b0{Pair(11i32)}
  [Pair] b1{Pair(12i32)}
  [Pair] b2{Pair(13i32)}

  [Pair] c0{Pair(15i32)}
  [Pair] c1{Pair(17i32)}

  return(plus(score_refs(location(a0), location(a1), location(a2)),
              plus(forward(location(b0), location(b1), location(b2)),
                   forward_mixed(location(c0), location(c1)))))
}
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_struct_reference.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_struct_reference").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 65);
}

TEST_CASE("native materializes variadic struct pointer packs with indexed field and helper access") {
  const std::string source = R"(
[struct]
Pair() {
  [i32] value{7i32}
}

[return<int>]
/Pair/score([Pair] self) {
  return(plus(self.value, 1i32))
}

[return<int>]
score_ptrs([args<Pointer<Pair>>] values) {
  return(plus(values[0i32].value, values[2i32].score()))
}

[return<int>]
forward([args<Pointer<Pair>>] values) {
  return(score_ptrs([spread] values))
}

[return<int>]
forward_mixed([args<Pointer<Pair>>] values) {
  [Pair] extra{Pair(5i32)}
  [Reference<Pair>] extra_ref{location(extra)}
  return(score_ptrs(location(extra_ref), [spread] values))
}

[return<int>]
main() {
  [Pair] a0{Pair(7i32)}
  [Pair] a1{Pair(8i32)}
  [Pair] a2{Pair(9i32)}
  [Reference<Pair>] r0{location(a0)}
  [Reference<Pair>] r1{location(a1)}
  [Reference<Pair>] r2{location(a2)}

  [Pair] b0{Pair(11i32)}
  [Pair] b1{Pair(12i32)}
  [Pair] b2{Pair(13i32)}
  [Reference<Pair>] s0{location(b0)}
  [Reference<Pair>] s1{location(b1)}
  [Reference<Pair>] s2{location(b2)}

  [Pair] c0{Pair(15i32)}
  [Pair] c1{Pair(17i32)}
  [Reference<Pair>] t0{location(c0)}
  [Reference<Pair>] t1{location(c1)}

  return(plus(score_ptrs(location(r0), location(r1), location(r2)),
              plus(forward(location(s0), location(s1), location(s2)),
                   forward_mixed(location(t0), location(t1)))))
}
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_struct_pointer.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_struct_pointer").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 65);
}

TEST_CASE("native materializes variadic scalar pointer packs with indexed dereference") {
  const std::string source = R"(
[return<int>]
score_ptrs([args<Pointer<i32>>] values) {
  return(plus(dereference(values[0i32]), dereference(values[2i32])))
}

[return<int>]
forward([args<Pointer<i32>>] values) {
  return(score_ptrs([spread] values))
}

[return<int>]
forward_mixed([args<Pointer<i32>>] values) {
  [i32] extra{1i32}
  [Reference<i32>] extra_ref{location(extra)}
  return(score_ptrs(location(extra_ref), [spread] values))
}

[return<int>]
main() {
  [i32] a0{1i32}
  [i32] a1{2i32}
  [i32] a2{3i32}
  [Reference<i32>] r0{location(a0)}
  [Reference<i32>] r1{location(a1)}
  [Reference<i32>] r2{location(a2)}

  [i32] b0{4i32}
  [i32] b1{5i32}
  [i32] b2{6i32}
  [Reference<i32>] s0{location(b0)}
  [Reference<i32>] s1{location(b1)}
  [Reference<i32>] s2{location(b2)}

  [i32] c0{7i32}
  [i32] c1{8i32}
  [Reference<i32>] t0{location(c0)}
  [Reference<i32>] t1{location(c1)}

  return(plus(score_ptrs(location(r0), location(r1), location(r2)),
              plus(forward(location(s0), location(s1), location(s2)),
                   forward_mixed(location(t0), location(t1)))))
}
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_scalar_pointer.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_scalar_pointer").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 23);
}

TEST_CASE("native materializes variadic pointer uninitialized scalar packs with indexed init and take") {
  const std::string source = R"(
[return<int>]
score_ptrs([args<Pointer<uninitialized<i32>>>] values) {
  init(dereference(values[0i32]), 2i32)
  init(dereference(values.at(1i32)), 3i32)
  init(dereference(values.at_unsafe(2i32)), 4i32)
  return(plus(take(dereference(values[0i32])),
              plus(take(dereference(values.at(1i32))),
                   take(dereference(values.at_unsafe(2i32))))))
}

[return<int>]
forward([args<Pointer<uninitialized<i32>>>] values) {
  return(score_ptrs([spread] values))
}

[return<int>]
forward_mixed([args<Pointer<uninitialized<i32>>>] values) {
  [uninitialized<i32>] extra{uninitialized<i32>()}
  return(score_ptrs(location(extra), [spread] values))
}

[return<int>]
main() {
  [uninitialized<i32>] a0{uninitialized<i32>()}
  [uninitialized<i32>] a1{uninitialized<i32>()}
  [uninitialized<i32>] a2{uninitialized<i32>()}

  [uninitialized<i32>] b0{uninitialized<i32>()}
  [uninitialized<i32>] b1{uninitialized<i32>()}
  [uninitialized<i32>] b2{uninitialized<i32>()}

  [uninitialized<i32>] c0{uninitialized<i32>()}
  [uninitialized<i32>] c1{uninitialized<i32>()}

  return(plus(score_ptrs(location(a0), location(a1), location(a2)),
              plus(forward(location(b0), location(b1), location(b2)),
                   forward_mixed(location(c0), location(c1)))))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_variadic_args_pointer_uninitialized_scalar.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_pointer_uninitialized_scalar").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 27);
}

TEST_CASE("native materializes variadic borrowed uninitialized scalar packs with indexed init and take") {
  const std::string source = R"(
[return<int>]
score_refs([args<Reference<uninitialized<i32>>>] values) {
  init(dereference(values[0i32]), 2i32)
  init(dereference(values.at(1i32)), 3i32)
  init(dereference(values.at_unsafe(2i32)), 4i32)
  return(plus(take(dereference(values[0i32])),
              plus(take(dereference(values.at(1i32))),
                   take(dereference(values.at_unsafe(2i32))))))
}

[return<int>]
forward([args<Reference<uninitialized<i32>>>] values) {
  return(score_refs([spread] values))
}

[return<int>]
forward_mixed([args<Reference<uninitialized<i32>>>] values) {
  [uninitialized<i32>] extra{uninitialized<i32>()}
  return(score_refs(location(extra), [spread] values))
}

[return<int>]
main() {
  [uninitialized<i32>] a0{uninitialized<i32>()}
  [uninitialized<i32>] a1{uninitialized<i32>()}
  [uninitialized<i32>] a2{uninitialized<i32>()}

  [uninitialized<i32>] b0{uninitialized<i32>()}
  [uninitialized<i32>] b1{uninitialized<i32>()}
  [uninitialized<i32>] b2{uninitialized<i32>()}

  [uninitialized<i32>] c0{uninitialized<i32>()}
  [uninitialized<i32>] c1{uninitialized<i32>()}

  return(plus(score_refs(location(a0), location(a1), location(a2)),
              plus(forward(location(b0), location(b1), location(b2)),
                   forward_mixed(location(c0), location(c1)))))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_variadic_args_reference_uninitialized_scalar.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_variadic_args_reference_uninitialized_scalar")
          .string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 27);
}

TEST_CASE("native materializes variadic borrowed map packs with indexed count methods") {
  const std::string source = R"(
[return<int>]
score_refs([args<Reference</std/collections/map<i32, i32>>>] values) {
  return(plus(values[0i32].count(), values[2i32].count()))
}

[return<int>]
forward([args<Reference</std/collections/map<i32, i32>>>] values) {
  return(score_refs([spread] values))
}

[return<int>]
forward_mixed([args<Reference</std/collections/map<i32, i32>>>] values) {
  [/std/collections/map<i32, i32>] extra{map<i32, i32>(1i32, 2i32)}
  [Reference</std/collections/map<i32, i32>>] extra_ref{location(extra)}
  return(score_refs(extra_ref, [spread] values))
}

[return<int>]
main() {
  [/std/collections/map<i32, i32>] a0{map<i32, i32>(1i32, 2i32, 3i32, 4i32)}
  [/std/collections/map<i32, i32>] a1{map<i32, i32>(5i32, 6i32)}
  [/std/collections/map<i32, i32>] a2{map<i32, i32>(7i32, 8i32, 9i32, 10i32, 11i32, 12i32)}
  [Reference</std/collections/map<i32, i32>>] r0{location(a0)}
  [Reference</std/collections/map<i32, i32>>] r1{location(a1)}
  [Reference</std/collections/map<i32, i32>>] r2{location(a2)}

  [/std/collections/map<i32, i32>] b0{map<i32, i32>(13i32, 14i32)}
  [/std/collections/map<i32, i32>] b1{map<i32, i32>(15i32, 16i32, 17i32, 18i32)}
  [/std/collections/map<i32, i32>] b2{map<i32, i32>(19i32, 20i32)}
  [Reference</std/collections/map<i32, i32>>] s0{location(b0)}
  [Reference</std/collections/map<i32, i32>>] s1{location(b1)}
  [Reference</std/collections/map<i32, i32>>] s2{location(b2)}

  [/std/collections/map<i32, i32>] c0{map<i32, i32>(21i32, 22i32, 23i32, 24i32)}
  [/std/collections/map<i32, i32>] c1{map<i32, i32>(25i32, 26i32, 27i32, 28i32, 29i32, 30i32)}
  [Reference</std/collections/map<i32, i32>>] t0{location(c0)}
  [Reference</std/collections/map<i32, i32>>] t1{location(c1)}

  return(plus(score_refs(r0, r1, r2),
              plus(forward(s0, s1, s2),
                   forward_mixed(t0, t1))))
}
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_borrowed_map.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_borrowed_map").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 11);
}

TEST_CASE("native materializes variadic borrowed map packs with indexed dereference count methods") {
  const std::string source = R"(
[return<int>]
score_refs([args<Reference</std/collections/map<i32, i32>>>] values) {
  return(plus(dereference(at(values, 0i32)).count(),
              dereference(at(values, 2i32)).count()))
}

[return<int>]
forward([args<Reference</std/collections/map<i32, i32>>>] values) {
  return(score_refs([spread] values))
}

[return<int>]
forward_mixed([args<Reference</std/collections/map<i32, i32>>>] values) {
  [/std/collections/map<i32, i32>] extra{map<i32, i32>(1i32, 2i32)}
  [Reference</std/collections/map<i32, i32>>] extra_ref{location(extra)}
  return(score_refs(extra_ref, [spread] values))
}

[return<int>]
main() {
  [/std/collections/map<i32, i32>] a0{map<i32, i32>(1i32, 2i32, 3i32, 4i32)}
  [/std/collections/map<i32, i32>] a1{map<i32, i32>(5i32, 6i32)}
  [/std/collections/map<i32, i32>] a2{map<i32, i32>(7i32, 8i32, 9i32, 10i32, 11i32, 12i32)}
  [Reference</std/collections/map<i32, i32>>] r0{location(a0)}
  [Reference</std/collections/map<i32, i32>>] r1{location(a1)}
  [Reference</std/collections/map<i32, i32>>] r2{location(a2)}

  [/std/collections/map<i32, i32>] b0{map<i32, i32>(13i32, 14i32)}
  [/std/collections/map<i32, i32>] b1{map<i32, i32>(15i32, 16i32, 17i32, 18i32)}
  [/std/collections/map<i32, i32>] b2{map<i32, i32>(19i32, 20i32)}
  [Reference</std/collections/map<i32, i32>>] s0{location(b0)}
  [Reference</std/collections/map<i32, i32>>] s1{location(b1)}
  [Reference</std/collections/map<i32, i32>>] s2{location(b2)}

  [/std/collections/map<i32, i32>] c0{map<i32, i32>(21i32, 22i32, 23i32, 24i32)}
  [/std/collections/map<i32, i32>] c1{map<i32, i32>(25i32, 26i32, 27i32, 28i32, 29i32, 30i32)}
  [Reference</std/collections/map<i32, i32>>] t0{location(c0)}
  [Reference</std/collections/map<i32, i32>>] t1{location(c1)}

  return(plus(score_refs(r0, r1, r2),
              plus(forward(s0, s1, s2),
                   forward_mixed(t0, t1))))
}
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_borrowed_map_deref_count.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_borrowed_map_deref_count").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 11);
}

TEST_CASE("native materializes variadic borrowed map packs with indexed dereference lookup helpers") {
  const std::string source = R"(
[return<int>]
score_refs([args<Reference</std/collections/map<i32, i32>>>] values) {
  [auto] head{dereference(at(values, 0i32)).at_unsafe(3i32)}
  if(dereference(at(values, 2i32)).contains(11i32),
     then(){ return(plus(head, dereference(at(values, 2i32)).at(11i32))) },
     else(){ return(0i32) })
}

[return<int>]
forward([args<Reference</std/collections/map<i32, i32>>>] values) {
  return(score_refs([spread] values))
}

[return<int>]
forward_mixed([args<Reference</std/collections/map<i32, i32>>>] values) {
  [/std/collections/map<i32, i32>] extra{map<i32, i32>(19i32, 20i32, 3i32, 4i32)}
  [Reference</std/collections/map<i32, i32>>] extra_ref{location(extra)}
  return(score_refs(extra_ref, [spread] values))
}

[return<int>]
main() {
  [/std/collections/map<i32, i32>] a0{map<i32, i32>(1i32, 2i32, 3i32, 4i32)}
  [/std/collections/map<i32, i32>] a1{map<i32, i32>(5i32, 6i32)}
  [/std/collections/map<i32, i32>] a2{map<i32, i32>(7i32, 8i32, 9i32, 10i32, 11i32, 12i32)}
  [Reference</std/collections/map<i32, i32>>] r0{location(a0)}
  [Reference</std/collections/map<i32, i32>>] r1{location(a1)}
  [Reference</std/collections/map<i32, i32>>] r2{location(a2)}

  [/std/collections/map<i32, i32>] b0{map<i32, i32>(13i32, 14i32, 3i32, 4i32)}
  [/std/collections/map<i32, i32>] b1{map<i32, i32>(15i32, 16i32)}
  [/std/collections/map<i32, i32>] b2{map<i32, i32>(17i32, 18i32, 11i32, 12i32)}
  [Reference</std/collections/map<i32, i32>>] s0{location(b0)}
  [Reference</std/collections/map<i32, i32>>] s1{location(b1)}
  [Reference</std/collections/map<i32, i32>>] s2{location(b2)}

  [/std/collections/map<i32, i32>] c0{map<i32, i32>(21i32, 22i32)}
  [/std/collections/map<i32, i32>] c1{map<i32, i32>(23i32, 24i32, 11i32, 12i32)}
  [Reference</std/collections/map<i32, i32>>] t0{location(c0)}
  [Reference</std/collections/map<i32, i32>>] t1{location(c1)}

  return(plus(score_refs(r0, r1, r2),
              plus(forward(s0, s1, s2),
                   forward_mixed(t0, t1))))
}
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_borrowed_map_deref_lookup.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_borrowed_map_deref_lookup").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 48);
}

TEST_CASE("native materializes variadic borrowed map packs with indexed tryAt inference") {
  const std::string source = R"(
[return<int>]
score_refs([args<Reference</std/collections/map<i32, i32>>>] values) {
  [auto] head{/std/collections/map/at_unsafe<i32, i32>(at(values, 0i32), 3i32)}
  [auto] tailMissing{
      Result.error(/std/collections/map/tryAt<i32, i32>(at(values, minus(count(values), 1i32)), 99i32))}
  return(if(tailMissing,
            then(){ return(plus(head, /std/collections/map/at_unsafe<i32, i32>(at(values, minus(count(values), 1i32)), 11i32))) },
            else(){ return(0i32) }))
}

[return<int>]
forward([args<Reference</std/collections/map<i32, i32>>>] values) {
  return(score_refs([spread] values))
}

[return<int>]
forward_mixed([args<Reference</std/collections/map<i32, i32>>>] values) {
  [/std/collections/map<i32, i32>] extra{map<i32, i32>(1i32, 2i32, 3i32, 8i32)}
  [Reference</std/collections/map<i32, i32>>] extra_ref{location(extra)}
  return(score_refs(extra_ref, [spread] values))
}

[return<int>]
main() {
  [/std/collections/map<i32, i32>] a0{map<i32, i32>(1i32, 2i32, 3i32, 4i32)}
  [/std/collections/map<i32, i32>] a1{map<i32, i32>(5i32, 6i32)}
  [/std/collections/map<i32, i32>] a2{map<i32, i32>(7i32, 8i32, 9i32, 10i32, 11i32, 12i32)}
  [Reference</std/collections/map<i32, i32>>] r0{location(a0)}
  [Reference</std/collections/map<i32, i32>>] r1{location(a1)}
  [Reference</std/collections/map<i32, i32>>] r2{location(a2)}

  [/std/collections/map<i32, i32>] b0{map<i32, i32>(1i32, 2i32, 3i32, 6i32)}
  [/std/collections/map<i32, i32>] b1{map<i32, i32>(5i32, 6i32)}
  [/std/collections/map<i32, i32>] b2{map<i32, i32>(7i32, 8i32, 9i32, 10i32, 11i32, 14i32)}
  [Reference</std/collections/map<i32, i32>>] s0{location(b0)}
  [Reference</std/collections/map<i32, i32>>] s1{location(b1)}
  [Reference</std/collections/map<i32, i32>>] s2{location(b2)}

  [/std/collections/map<i32, i32>] c0{map<i32, i32>(7i32, 8i32, 9i32, 10i32, 11i32, 16i32)}
  [/std/collections/map<i32, i32>] c1{map<i32, i32>(11i32, 16i32, 13i32, 14i32)}
  [Reference</std/collections/map<i32, i32>>] t0{location(c0)}
  [Reference</std/collections/map<i32, i32>>] t1{location(c1)}

  return(plus(score_refs(r0, r1, r2),
              plus(forward(s0, s1, s2),
                   forward_mixed(t0, t1))))
}
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_borrowed_map_tryat.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_borrowed_map_tryat").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 60);
}

TEST_CASE("native materializes variadic pointer map packs with indexed count methods") {
  const std::string source = R"(
[return<int>]
score_ptrs([args<Pointer</std/collections/map<i32, i32>>>] values) {
  return(plus(values[0i32].count(), values[2i32].count()))
}

[return<int>]
forward([args<Pointer</std/collections/map<i32, i32>>>] values) {
  return(score_ptrs([spread] values))
}

[return<int>]
forward_mixed([args<Pointer</std/collections/map<i32, i32>>>] values) {
  [/std/collections/map<i32, i32>] extra{map<i32, i32>(1i32, 2i32)}
  [Pointer</std/collections/map<i32, i32>>] extra_ptr{location(extra)}
  return(score_ptrs(extra_ptr, [spread] values))
}

[return<int>]
main() {
  [/std/collections/map<i32, i32>] a0{map<i32, i32>(1i32, 2i32, 3i32, 4i32)}
  [/std/collections/map<i32, i32>] a1{map<i32, i32>(5i32, 6i32)}
  [/std/collections/map<i32, i32>] a2{map<i32, i32>(7i32, 8i32, 9i32, 10i32, 11i32, 12i32)}
  [Pointer</std/collections/map<i32, i32>>] r0{location(a0)}
  [Pointer</std/collections/map<i32, i32>>] r1{location(a1)}
  [Pointer</std/collections/map<i32, i32>>] r2{location(a2)}

  [/std/collections/map<i32, i32>] b0{map<i32, i32>(13i32, 14i32)}
  [/std/collections/map<i32, i32>] b1{map<i32, i32>(15i32, 16i32, 17i32, 18i32)}
  [/std/collections/map<i32, i32>] b2{map<i32, i32>(19i32, 20i32)}
  [Pointer</std/collections/map<i32, i32>>] s0{location(b0)}
  [Pointer</std/collections/map<i32, i32>>] s1{location(b1)}
  [Pointer</std/collections/map<i32, i32>>] s2{location(b2)}

  [/std/collections/map<i32, i32>] c0{map<i32, i32>(21i32, 22i32, 23i32, 24i32)}
  [/std/collections/map<i32, i32>] c1{map<i32, i32>(25i32, 26i32, 27i32, 28i32, 29i32, 30i32)}
  [Pointer</std/collections/map<i32, i32>>] t0{location(c0)}
  [Pointer</std/collections/map<i32, i32>>] t1{location(c1)}

  return(plus(score_ptrs(r0, r1, r2),
              plus(forward(s0, s1, s2),
                   forward_mixed(t0, t1))))
}
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_pointer_map.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_pointer_map").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 11);
}

TEST_CASE("native materializes variadic pointer map packs with indexed dereference count methods") {
  const std::string source = R"(
[return<int>]
score_ptrs([args<Pointer</std/collections/map<i32, i32>>>] values) {
  return(plus(dereference(at(values, 0i32)).count(),
              dereference(at(values, 2i32)).count()))
}

[return<int>]
forward([args<Pointer</std/collections/map<i32, i32>>>] values) {
  return(score_ptrs([spread] values))
}

[return<int>]
forward_mixed([args<Pointer</std/collections/map<i32, i32>>>] values) {
  [/std/collections/map<i32, i32>] extra{map<i32, i32>(1i32, 2i32)}
  [Pointer</std/collections/map<i32, i32>>] extra_ptr{location(extra)}
  return(score_ptrs(extra_ptr, [spread] values))
}

[return<int>]
main() {
  [/std/collections/map<i32, i32>] a0{map<i32, i32>(1i32, 2i32, 3i32, 4i32)}
  [/std/collections/map<i32, i32>] a1{map<i32, i32>(5i32, 6i32)}
  [/std/collections/map<i32, i32>] a2{map<i32, i32>(7i32, 8i32, 9i32, 10i32, 11i32, 12i32)}
  [Pointer</std/collections/map<i32, i32>>] r0{location(a0)}
  [Pointer</std/collections/map<i32, i32>>] r1{location(a1)}
  [Pointer</std/collections/map<i32, i32>>] r2{location(a2)}

  [/std/collections/map<i32, i32>] b0{map<i32, i32>(13i32, 14i32)}
  [/std/collections/map<i32, i32>] b1{map<i32, i32>(15i32, 16i32, 17i32, 18i32)}
  [/std/collections/map<i32, i32>] b2{map<i32, i32>(19i32, 20i32)}
  [Pointer</std/collections/map<i32, i32>>] s0{location(b0)}
  [Pointer</std/collections/map<i32, i32>>] s1{location(b1)}
  [Pointer</std/collections/map<i32, i32>>] s2{location(b2)}

  [/std/collections/map<i32, i32>] c0{map<i32, i32>(21i32, 22i32, 23i32, 24i32)}
  [/std/collections/map<i32, i32>] c1{map<i32, i32>(25i32, 26i32, 27i32, 28i32, 29i32, 30i32)}
  [Pointer</std/collections/map<i32, i32>>] t0{location(c0)}
  [Pointer</std/collections/map<i32, i32>>] t1{location(c1)}

  return(plus(score_ptrs(r0, r1, r2),
              plus(forward(s0, s1, s2),
                   forward_mixed(t0, t1))))
}
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_pointer_map_deref_count.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_pointer_map_deref_count").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 11);
}

TEST_CASE("native materializes variadic pointer map packs with indexed lookup helpers") {
  const std::string source = R"(
[return<int>]
score_ptrs([args<Pointer</std/collections/map<i32, i32>>>] values) {
  [auto] head{/std/collections/map/at_unsafe(at(values, 0i32), 3i32)}
  if(at(values, 2i32).contains(11i32),
     then(){ return(plus(head, at(values, 2i32).at(11i32))) },
     else(){ return(0i32) })
}

[return<int>]
forward([args<Pointer</std/collections/map<i32, i32>>>] values) {
  return(score_ptrs([spread] values))
}

[return<int>]
forward_mixed([args<Pointer</std/collections/map<i32, i32>>>] values) {
  [/std/collections/map<i32, i32>] extra{map<i32, i32>(19i32, 20i32, 3i32, 4i32)}
  [Pointer</std/collections/map<i32, i32>>] extra_ptr{location(extra)}
  return(score_ptrs(extra_ptr, [spread] values))
}

[return<int>]
main() {
  [/std/collections/map<i32, i32>] a0{map<i32, i32>(1i32, 2i32, 3i32, 4i32)}
  [/std/collections/map<i32, i32>] a1{map<i32, i32>(5i32, 6i32)}
  [/std/collections/map<i32, i32>] a2{map<i32, i32>(7i32, 8i32, 9i32, 10i32, 11i32, 12i32)}
  [Pointer</std/collections/map<i32, i32>>] r0{location(a0)}
  [Pointer</std/collections/map<i32, i32>>] r1{location(a1)}
  [Pointer</std/collections/map<i32, i32>>] r2{location(a2)}

  [/std/collections/map<i32, i32>] b0{map<i32, i32>(13i32, 14i32, 3i32, 4i32)}
  [/std/collections/map<i32, i32>] b1{map<i32, i32>(15i32, 16i32)}
  [/std/collections/map<i32, i32>] b2{map<i32, i32>(17i32, 18i32, 11i32, 12i32)}
  [Pointer</std/collections/map<i32, i32>>] s0{location(b0)}
  [Pointer</std/collections/map<i32, i32>>] s1{location(b1)}
  [Pointer</std/collections/map<i32, i32>>] s2{location(b2)}

  [/std/collections/map<i32, i32>] c0{map<i32, i32>(21i32, 22i32)}
  [/std/collections/map<i32, i32>] c1{map<i32, i32>(23i32, 24i32, 11i32, 12i32)}
  [Pointer</std/collections/map<i32, i32>>] t0{location(c0)}
  [Pointer</std/collections/map<i32, i32>>] t1{location(c1)}

  return(plus(score_ptrs(r0, r1, r2),
              plus(forward(s0, s1, s2),
                   forward_mixed(t0, t1))))
}
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_pointer_map_lookup.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_pointer_map_lookup").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 48);
}

TEST_CASE("native materializes variadic pointer map packs with indexed dereference lookup helpers") {
  const std::string source = R"(
[return<int>]
score_ptrs([args<Pointer</std/collections/map<i32, i32>>>] values) {
  [auto] head{dereference(at(values, 0i32)).at_unsafe(3i32)}
  if(dereference(at(values, 2i32)).contains(11i32),
     then(){ return(plus(head, dereference(at(values, 2i32)).at(11i32))) },
     else(){ return(0i32) })
}

[return<int>]
forward([args<Pointer</std/collections/map<i32, i32>>>] values) {
  return(score_ptrs([spread] values))
}

[return<int>]
forward_mixed([args<Pointer</std/collections/map<i32, i32>>>] values) {
  [/std/collections/map<i32, i32>] extra{map<i32, i32>(19i32, 20i32, 3i32, 4i32)}
  [Pointer</std/collections/map<i32, i32>>] extra_ptr{location(extra)}
  return(score_ptrs(extra_ptr, [spread] values))
}

[return<int>]
main() {
  [/std/collections/map<i32, i32>] a0{map<i32, i32>(1i32, 2i32, 3i32, 4i32)}
  [/std/collections/map<i32, i32>] a1{map<i32, i32>(5i32, 6i32)}
  [/std/collections/map<i32, i32>] a2{map<i32, i32>(7i32, 8i32, 9i32, 10i32, 11i32, 12i32)}
  [Pointer</std/collections/map<i32, i32>>] r0{location(a0)}
  [Pointer</std/collections/map<i32, i32>>] r1{location(a1)}
  [Pointer</std/collections/map<i32, i32>>] r2{location(a2)}

  [/std/collections/map<i32, i32>] b0{map<i32, i32>(13i32, 14i32, 3i32, 4i32)}
  [/std/collections/map<i32, i32>] b1{map<i32, i32>(15i32, 16i32)}
  [/std/collections/map<i32, i32>] b2{map<i32, i32>(17i32, 18i32, 11i32, 12i32)}
  [Pointer</std/collections/map<i32, i32>>] s0{location(b0)}
  [Pointer</std/collections/map<i32, i32>>] s1{location(b1)}
  [Pointer</std/collections/map<i32, i32>>] s2{location(b2)}

  [/std/collections/map<i32, i32>] c0{map<i32, i32>(21i32, 22i32)}
  [/std/collections/map<i32, i32>] c1{map<i32, i32>(23i32, 24i32, 11i32, 12i32)}
  [Pointer</std/collections/map<i32, i32>>] t0{location(c0)}
  [Pointer</std/collections/map<i32, i32>>] t1{location(c1)}

  return(plus(score_ptrs(r0, r1, r2),
              plus(forward(s0, s1, s2),
                   forward_mixed(t0, t1))))
}
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_pointer_map_deref_lookup.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_pointer_map_deref_lookup").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 48);
}

TEST_CASE("native materializes variadic pointer map packs with indexed tryAt inference") {
  const std::string source = R"(
[return<int>]
score_ptrs([args<Pointer</std/collections/map<i32, i32>>>] values) {
  [auto] head{/std/collections/map/at_unsafe<i32, i32>(at(values, 0i32), 3i32)}
  [auto] tailMissing{
      Result.error(/std/collections/map/tryAt<i32, i32>(at(values, minus(count(values), 1i32)), 99i32))}
  return(if(tailMissing,
            then(){ return(plus(head, /std/collections/map/at_unsafe<i32, i32>(at(values, minus(count(values), 1i32)), 11i32))) },
            else(){ return(0i32) }))
}

[return<int>]
forward([args<Pointer</std/collections/map<i32, i32>>>] values) {
  return(score_ptrs([spread] values))
}

[return<int>]
forward_mixed([args<Pointer</std/collections/map<i32, i32>>>] values) {
  [/std/collections/map<i32, i32>] extra{map<i32, i32>(1i32, 2i32, 3i32, 8i32)}
  [Pointer</std/collections/map<i32, i32>>] extra_ptr{location(extra)}
  return(score_ptrs(extra_ptr, [spread] values))
}

[return<int>]
main() {
  [/std/collections/map<i32, i32>] a0{map<i32, i32>(1i32, 2i32, 3i32, 4i32)}
  [/std/collections/map<i32, i32>] a1{map<i32, i32>(5i32, 6i32)}
  [/std/collections/map<i32, i32>] a2{map<i32, i32>(7i32, 8i32, 9i32, 10i32, 11i32, 12i32)}
  [Pointer</std/collections/map<i32, i32>>] r0{location(a0)}
  [Pointer</std/collections/map<i32, i32>>] r1{location(a1)}
  [Pointer</std/collections/map<i32, i32>>] r2{location(a2)}

  [/std/collections/map<i32, i32>] b0{map<i32, i32>(1i32, 2i32, 3i32, 6i32)}
  [/std/collections/map<i32, i32>] b1{map<i32, i32>(5i32, 6i32)}
  [/std/collections/map<i32, i32>] b2{map<i32, i32>(7i32, 8i32, 9i32, 10i32, 11i32, 14i32)}
  [Pointer</std/collections/map<i32, i32>>] s0{location(b0)}
  [Pointer</std/collections/map<i32, i32>>] s1{location(b1)}
  [Pointer</std/collections/map<i32, i32>>] s2{location(b2)}

  [/std/collections/map<i32, i32>] c0{map<i32, i32>(7i32, 8i32, 9i32, 10i32, 11i32, 16i32)}
  [/std/collections/map<i32, i32>] c1{map<i32, i32>(11i32, 16i32, 13i32, 14i32)}
  [Pointer</std/collections/map<i32, i32>>] t0{location(c0)}
  [Pointer</std/collections/map<i32, i32>>] t1{location(c1)}

  return(plus(score_ptrs(r0, r1, r2),
              plus(forward(s0, s1, s2),
                   forward_mixed(t0, t1))))
}
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_pointer_map_tryat.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_pointer_map_tryat").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 60);
}

TEST_CASE("native materializes variadic pointer vector packs with indexed count methods") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
score_ptrs([args<Pointer<vector<i32>>>] values) {
  [vector<i32>] head{dereference(at(values, 0i32))}
  [vector<i32>] tail{dereference(at(values, 2i32))}
  return(plus(/std/collections/vector/count(head),
              /std/collections/vector/count(tail)))
}

[return<int>]
forward([args<Pointer<vector<i32>>>] values) {
  return(score_ptrs([spread] values))
}

[effects(heap_alloc), return<int>]
forward_mixed([args<Pointer<vector<i32>>>] values) {
  [vector<i32>] extra{vectorSingle<i32>(1i32)}
  [Pointer<vector<i32>>] extra_ptr{location(extra)}
  return(score_ptrs(extra_ptr, [spread] values))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] a0{vector<i32>(1i32, 2i32)}
  [vector<i32>] a1{vectorSingle<i32>(3i32)}
  [vector<i32>] a2{vector<i32>(4i32, 5i32, 6i32, 7i32)}
  [Pointer<vector<i32>>] r0{location(a0)}
  [Pointer<vector<i32>>] r1{location(a1)}
  [Pointer<vector<i32>>] r2{location(a2)}

  [vector<i32>] b0{vectorSingle<i32>(8i32)}
  [vector<i32>] b1{vector<i32>(9i32, 10i32)}
  [vector<i32>] b2{vector<i32>(11i32, 12i32, 13i32)}
  [Pointer<vector<i32>>] s0{location(b0)}
  [Pointer<vector<i32>>] s1{location(b1)}
  [Pointer<vector<i32>>] s2{location(b2)}

  [vector<i32>] c0{vector<i32>(14i32, 15i32)}
  [vector<i32>] c1{vector<i32>(16i32, 17i32, 18i32, 19i32, 20i32)}
  [Pointer<vector<i32>>] t0{location(c0)}
  [Pointer<vector<i32>>] t1{location(c1)}

  return(plus(score_ptrs(r0, r1, r2),
              plus(forward(s0, s1, s2),
                   forward_mixed(t0, t1))))
}
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_pointer_vector.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_pointer_vector").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 16);
}

TEST_CASE("native materializes variadic pointer vector packs with indexed dereference capacity methods") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
score_ptrs([args<Pointer<vector<i32>>>] values) {
  [vector<i32>] head{dereference(at(values, 0i32))}
  [vector<i32>] tail{dereference(at(values, 2i32))}
  return(plus(/std/collections/vector/capacity(head),
              /std/collections/vector/capacity(tail)))
}

[return<int>]
forward([args<Pointer<vector<i32>>>] values) {
  return(score_ptrs([spread] values))
}

[effects(heap_alloc), return<int>]
forward_mixed([args<Pointer<vector<i32>>>] values) {
  [vector<i32>] extra{vectorSingle<i32>(1i32)}
  [Pointer<vector<i32>>] extra_ptr{location(extra)}
  return(score_ptrs(extra_ptr, [spread] values))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] a0{vector<i32>(1i32, 2i32)}
  [vector<i32>] a1{vectorSingle<i32>(3i32)}
  [vector<i32>] a2{vector<i32>(4i32, 5i32, 6i32, 7i32)}
  [Pointer<vector<i32>>] r0{location(a0)}
  [Pointer<vector<i32>>] r1{location(a1)}
  [Pointer<vector<i32>>] r2{location(a2)}

  [vector<i32>] b0{vectorSingle<i32>(8i32)}
  [vector<i32>] b1{vector<i32>(9i32, 10i32)}
  [vector<i32>] b2{vector<i32>(11i32, 12i32, 13i32)}
  [Pointer<vector<i32>>] s0{location(b0)}
  [Pointer<vector<i32>>] s1{location(b1)}
  [Pointer<vector<i32>>] s2{location(b2)}

  [vector<i32>] c0{vector<i32>(14i32, 15i32)}
  [vector<i32>] c1{vector<i32>(16i32, 17i32, 18i32, 19i32, 20i32)}
  [Pointer<vector<i32>>] t0{location(c0)}
  [Pointer<vector<i32>>] t1{location(c1)}

  return(plus(score_ptrs(r0, r1, r2),
              plus(forward(s0, s1, s2),
                   forward_mixed(t0, t1))))
}
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_pointer_vector_deref_capacity.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_pointer_vector_deref_capacity").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 16);
}

TEST_CASE("native materializes variadic pointer vector packs with indexed dereference access helpers") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
score_ptrs([args<Pointer<vector<i32>>>] values) {
  [auto] head{at_unsafe(dereference(at(values, 0i32)), 1i32)}
  return(plus(head, dereference(at(values, 2i32)).at(0i32)))
}

[return<int>]
forward([args<Pointer<vector<i32>>>] values) {
  return(score_ptrs([spread] values))
}

[effects(heap_alloc), return<int>]
forward_mixed([args<Pointer<vector<i32>>>] values) {
  [vector<i32>] extra{vector<i32>(1i32, 2i32)}
  [Pointer<vector<i32>>] extra_ptr{location(extra)}
  return(score_ptrs(extra_ptr, [spread] values))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] a0{vector<i32>(1i32, 2i32)}
  [vector<i32>] a1{vectorSingle<i32>(3i32)}
  [vector<i32>] a2{vector<i32>(4i32, 5i32, 6i32)}
  [Pointer<vector<i32>>] r0{location(a0)}
  [Pointer<vector<i32>>] r1{location(a1)}
  [Pointer<vector<i32>>] r2{location(a2)}

  [vector<i32>] b0{vector<i32>(7i32, 8i32)}
  [vector<i32>] b1{vectorSingle<i32>(9i32)}
  [vector<i32>] b2{vector<i32>(10i32, 11i32)}
  [Pointer<vector<i32>>] s0{location(b0)}
  [Pointer<vector<i32>>] s1{location(b1)}
  [Pointer<vector<i32>>] s2{location(b2)}

  [vector<i32>] c0{vectorSingle<i32>(12i32)}
  [vector<i32>] c1{vector<i32>(13i32, 14i32, 15i32)}
  [Pointer<vector<i32>>] t0{location(c0)}
  [Pointer<vector<i32>>] t1{location(c1)}

  return(plus(score_ptrs(r0, r1, r2),
              plus(forward(s0, s1, s2),
                   forward_mixed(t0, t1))))
}
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_pointer_vector_deref_access.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_pointer_vector_deref_access").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 39);
}

TEST_CASE("native materializes variadic pointer vector packs with indexed dereference statement mutators") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
mutate_ptrs([args<Pointer<vector<i32>>>] values) {
  [vector<i32> mut] head{dereference(at(values, 0i32))}
  [vector<i32> mut] middle{dereference(at(values, 1i32))}
  [vector<i32> mut] tail{dereference(at(values, 2i32))}
  head.push(9i32)
  head.pop()
  middle.reserve(6i32)
  middle.clear()
  tail.remove_at(1i32)
  tail.remove_swap(0i32)
  return(plus(/std/collections/vector/count(head),
              plus(/std/collections/vector/capacity(middle),
                   /std/collections/vector/count(tail))))
}

[effects(heap_alloc), return<int>]
forward([args<Pointer<vector<i32>>>] values) {
  return(mutate_ptrs([spread] values))
}

[effects(heap_alloc), return<int>]
forward_mixed([args<Pointer<vector<i32>>>] values) {
  [vector<i32>] extra{vectorSingle<i32>(20i32)}
  [Pointer<vector<i32>>] extra_ptr{location(extra)}
  return(mutate_ptrs(extra_ptr, [spread] values))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] a0{vector<i32>(1i32, 2i32)}
  [vector<i32>] a1{vectorSingle<i32>(3i32)}
  [vector<i32>] a2{vector<i32>(4i32, 5i32, 6i32)}
  [Pointer<vector<i32>>] r0{location(a0)}
  [Pointer<vector<i32>>] r1{location(a1)}
  [Pointer<vector<i32>>] r2{location(a2)}

  [vector<i32>] b0{vectorSingle<i32>(7i32)}
  [vector<i32>] b1{vector<i32>(8i32, 9i32)}
  [vector<i32>] b2{vector<i32>(10i32, 11i32, 12i32)}
  [Pointer<vector<i32>>] s0{location(b0)}
  [Pointer<vector<i32>>] s1{location(b1)}
  [Pointer<vector<i32>>] s2{location(b2)}

  [vector<i32>] c0{vector<i32>(13i32, 14i32)}
  [vector<i32>] c1{vector<i32>(15i32, 16i32, 17i32)}
  [Pointer<vector<i32>>] t0{location(c0)}
  [Pointer<vector<i32>>] t1{location(c1)}

  return(plus(mutate_ptrs(r0, r1, r2),
              plus(forward(s0, s1, s2),
                   forward_mixed(t0, t1))))
}
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_pointer_vector_mutators.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_pointer_vector_mutators").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 25);
}

TEST_CASE("native materializes variadic borrowed vector packs with indexed count methods") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
score_refs([args<Reference<vector<i32>>>] values) {
  [vector<i32>] head{dereference(at(values, 0i32))}
  [vector<i32>] tail{dereference(at(values, 2i32))}
  return(plus(/std/collections/vector/count(head),
              /std/collections/vector/count(tail)))
}

[return<int>]
forward([args<Reference<vector<i32>>>] values) {
  return(score_refs([spread] values))
}

[effects(heap_alloc), return<int>]
forward_mixed([args<Reference<vector<i32>>>] values) {
  [vector<i32>] extra{vectorSingle<i32>(1i32)}
  [Reference<vector<i32>>] extra_ref{location(extra)}
  return(score_refs(extra_ref, [spread] values))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] a0{vector<i32>(1i32, 2i32)}
  [vector<i32>] a1{vectorSingle<i32>(3i32)}
  [vector<i32>] a2{vector<i32>(4i32, 5i32, 6i32, 7i32)}
  [Reference<vector<i32>>] r0{location(a0)}
  [Reference<vector<i32>>] r1{location(a1)}
  [Reference<vector<i32>>] r2{location(a2)}

  [vector<i32>] b0{vectorSingle<i32>(8i32)}
  [vector<i32>] b1{vector<i32>(9i32, 10i32)}
  [vector<i32>] b2{vector<i32>(11i32, 12i32, 13i32)}
  [Reference<vector<i32>>] s0{location(b0)}
  [Reference<vector<i32>>] s1{location(b1)}
  [Reference<vector<i32>>] s2{location(b2)}

  [vector<i32>] c0{vector<i32>(14i32, 15i32)}
  [vector<i32>] c1{vector<i32>(16i32, 17i32, 18i32, 19i32, 20i32)}
  [Reference<vector<i32>>] t0{location(c0)}
  [Reference<vector<i32>>] t1{location(c1)}

  return(plus(score_refs(r0, r1, r2),
              plus(forward(s0, s1, s2),
                   forward_mixed(t0, t1))))
}
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_borrowed_vector.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_borrowed_vector").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 16);
}

TEST_CASE("native materializes variadic borrowed vector packs with indexed dereference capacity methods") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
score_refs([args<Reference<vector<i32>>>] values) {
  [vector<i32>] head{dereference(at(values, 0i32))}
  [vector<i32>] tail{dereference(at(values, 2i32))}
  return(plus(/std/collections/vector/capacity(head),
              /std/collections/vector/capacity(tail)))
}

[return<int>]
forward([args<Reference<vector<i32>>>] values) {
  return(score_refs([spread] values))
}

[effects(heap_alloc), return<int>]
forward_mixed([args<Reference<vector<i32>>>] values) {
  [vector<i32>] extra{vectorSingle<i32>(1i32)}
  [Reference<vector<i32>>] extra_ref{location(extra)}
  return(score_refs(extra_ref, [spread] values))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] a0{vector<i32>(1i32, 2i32)}
  [vector<i32>] a1{vectorSingle<i32>(3i32)}
  [vector<i32>] a2{vector<i32>(4i32, 5i32, 6i32, 7i32)}
  [Reference<vector<i32>>] r0{location(a0)}
  [Reference<vector<i32>>] r1{location(a1)}
  [Reference<vector<i32>>] r2{location(a2)}

  [vector<i32>] b0{vectorSingle<i32>(8i32)}
  [vector<i32>] b1{vector<i32>(9i32, 10i32)}
  [vector<i32>] b2{vector<i32>(11i32, 12i32, 13i32)}
  [Reference<vector<i32>>] s0{location(b0)}
  [Reference<vector<i32>>] s1{location(b1)}
  [Reference<vector<i32>>] s2{location(b2)}

  [vector<i32>] c0{vector<i32>(14i32, 15i32)}
  [vector<i32>] c1{vector<i32>(16i32, 17i32, 18i32, 19i32, 20i32)}
  [Reference<vector<i32>>] t0{location(c0)}
  [Reference<vector<i32>>] t1{location(c1)}

  return(plus(score_refs(r0, r1, r2),
              plus(forward(s0, s1, s2),
                   forward_mixed(t0, t1))))
}
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_borrowed_vector_deref_capacity.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_borrowed_vector_deref_capacity").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 16);
}

TEST_CASE("native materializes variadic borrowed vector packs with indexed dereference access helpers") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
score_refs([args<Reference<vector<i32>>>] values) {
  [auto] head{at_unsafe(dereference(at(values, 0i32)), 1i32)}
  return(plus(head, dereference(at(values, 2i32)).at(0i32)))
}

[return<int>]
forward([args<Reference<vector<i32>>>] values) {
  return(score_refs([spread] values))
}

[effects(heap_alloc), return<int>]
forward_mixed([args<Reference<vector<i32>>>] values) {
  [vector<i32>] extra{vector<i32>(1i32, 2i32)}
  [Reference<vector<i32>>] extra_ref{location(extra)}
  return(score_refs(extra_ref, [spread] values))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] a0{vector<i32>(1i32, 2i32)}
  [vector<i32>] a1{vectorSingle<i32>(3i32)}
  [vector<i32>] a2{vector<i32>(4i32, 5i32, 6i32)}
  [Reference<vector<i32>>] r0{location(a0)}
  [Reference<vector<i32>>] r1{location(a1)}
  [Reference<vector<i32>>] r2{location(a2)}

  [vector<i32>] b0{vector<i32>(7i32, 8i32)}
  [vector<i32>] b1{vectorSingle<i32>(9i32)}
  [vector<i32>] b2{vector<i32>(10i32, 11i32)}
  [Reference<vector<i32>>] s0{location(b0)}
  [Reference<vector<i32>>] s1{location(b1)}
  [Reference<vector<i32>>] s2{location(b2)}

  [vector<i32>] c0{vectorSingle<i32>(12i32)}
  [vector<i32>] c1{vector<i32>(13i32, 14i32, 15i32)}
  [Reference<vector<i32>>] t0{location(c0)}
  [Reference<vector<i32>>] t1{location(c1)}

  return(plus(score_refs(r0, r1, r2),
              plus(forward(s0, s1, s2),
                   forward_mixed(t0, t1))))
}
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_borrowed_vector_deref_access.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_borrowed_vector_deref_access").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 39);
}

TEST_CASE("native materializes variadic borrowed vector packs with indexed dereference statement mutators") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
mutate_refs([args<Reference<vector<i32>>>] values) {
  [vector<i32> mut] head{dereference(at(values, 0i32))}
  [vector<i32> mut] middle{dereference(at(values, 1i32))}
  [vector<i32> mut] tail{dereference(at(values, 2i32))}
  head.push(9i32)
  head.pop()
  middle.reserve(6i32)
  middle.clear()
  tail.remove_at(1i32)
  tail.remove_swap(0i32)
  return(plus(/std/collections/vector/count(head),
              plus(/std/collections/vector/capacity(middle),
                   /std/collections/vector/count(tail))))
}

[effects(heap_alloc), return<int>]
forward([args<Reference<vector<i32>>>] values) {
  return(mutate_refs([spread] values))
}

[effects(heap_alloc), return<int>]
forward_mixed([args<Reference<vector<i32>>>] values) {
  [vector<i32>] extra{vectorSingle<i32>(20i32)}
  [Reference<vector<i32>>] extra_ref{location(extra)}
  return(mutate_refs(extra_ref, [spread] values))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] a0{vector<i32>(1i32, 2i32)}
  [vector<i32>] a1{vectorSingle<i32>(3i32)}
  [vector<i32>] a2{vector<i32>(4i32, 5i32, 6i32)}
  [Reference<vector<i32>>] r0{location(a0)}
  [Reference<vector<i32>>] r1{location(a1)}
  [Reference<vector<i32>>] r2{location(a2)}

  [vector<i32>] b0{vectorSingle<i32>(7i32)}
  [vector<i32>] b1{vector<i32>(8i32, 9i32)}
  [vector<i32>] b2{vector<i32>(10i32, 11i32, 12i32)}
  [Reference<vector<i32>>] s0{location(b0)}
  [Reference<vector<i32>>] s1{location(b1)}
  [Reference<vector<i32>>] s2{location(b2)}

  [vector<i32>] c0{vector<i32>(13i32, 14i32)}
  [vector<i32>] c1{vector<i32>(15i32, 16i32, 17i32)}
  [Reference<vector<i32>>] t0{location(c0)}
  [Reference<vector<i32>>] t1{location(c1)}

  return(plus(mutate_refs(r0, r1, r2),
              plus(forward(s0, s1, s2),
                   forward_mixed(t0, t1))))
}
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_borrowed_vector_mutators.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_borrowed_vector_mutators").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 25);
}

TEST_CASE("native materializes variadic borrowed soa_vector packs with indexed count methods") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
score_refs([args<Reference<soa_vector<Particle>>>] values) {
  [soa_vector<Particle>] head{dereference(at(values, 0i32))}
  [soa_vector<Particle>] tail{dereference(at(values, 2i32))}
  return(plus(count(values), plus(count(head), count(tail))))
}

[return<int>]
forward([args<Reference<soa_vector<Particle>>>] values) {
  return(score_refs([spread] values))
}

[effects(heap_alloc), return<int>]
forward_mixed([args<Reference<soa_vector<Particle>>>] values) {
  [soa_vector<Particle>] extra{soa_vector<Particle>()}
  [Reference<soa_vector<Particle>>] extra_ref{location(extra)}
  return(score_refs(extra_ref, [spread] values))
}

[effects(heap_alloc), return<int>]
main() {
  [soa_vector<Particle>] a0{soa_vector<Particle>()}
  [soa_vector<Particle>] a1{soa_vector<Particle>()}
  [soa_vector<Particle>] a2{soa_vector<Particle>()}
  [Reference<soa_vector<Particle>>] r0{location(a0)}
  [Reference<soa_vector<Particle>>] r1{location(a1)}
  [Reference<soa_vector<Particle>>] r2{location(a2)}

  [soa_vector<Particle>] b0{soa_vector<Particle>()}
  [soa_vector<Particle>] b1{soa_vector<Particle>()}
  [soa_vector<Particle>] b2{soa_vector<Particle>()}
  [Reference<soa_vector<Particle>>] s0{location(b0)}
  [Reference<soa_vector<Particle>>] s1{location(b1)}
  [Reference<soa_vector<Particle>>] s2{location(b2)}

  [soa_vector<Particle>] c0{soa_vector<Particle>()}
  [soa_vector<Particle>] c1{soa_vector<Particle>()}
  [Reference<soa_vector<Particle>>] t0{location(c0)}
  [Reference<soa_vector<Particle>>] t1{location(c1)}

  return(plus(score_refs(r0, r1, r2),
              plus(forward(s0, s1, s2),
                   forward_mixed(t0, t1))))
}
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_borrowed_soa_vector.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_borrowed_soa_vector").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("native materializes variadic pointer soa_vector packs with indexed count methods") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
score_ptrs([args<Pointer<soa_vector<Particle>>>] values) {
  [soa_vector<Particle>] head{dereference(at(values, 0i32))}
  [soa_vector<Particle>] tail{dereference(at(values, 2i32))}
  return(plus(count(values), plus(count(head), count(tail))))
}

[return<int>]
forward([args<Pointer<soa_vector<Particle>>>] values) {
  return(score_ptrs([spread] values))
}

[effects(heap_alloc), return<int>]
forward_mixed([args<Pointer<soa_vector<Particle>>>] values) {
  [soa_vector<Particle>] extra{soa_vector<Particle>()}
  [Pointer<soa_vector<Particle>>] extra_ptr{location(extra)}
  return(score_ptrs(extra_ptr, [spread] values))
}

[effects(heap_alloc), return<int>]
main() {
  [soa_vector<Particle>] a0{soa_vector<Particle>()}
  [soa_vector<Particle>] a1{soa_vector<Particle>()}
  [soa_vector<Particle>] a2{soa_vector<Particle>()}
  [Pointer<soa_vector<Particle>>] r0{location(a0)}
  [Pointer<soa_vector<Particle>>] r1{location(a1)}
  [Pointer<soa_vector<Particle>>] r2{location(a2)}

  [soa_vector<Particle>] b0{soa_vector<Particle>()}
  [soa_vector<Particle>] b1{soa_vector<Particle>()}
  [soa_vector<Particle>] b2{soa_vector<Particle>()}
  [Pointer<soa_vector<Particle>>] s0{location(b0)}
  [Pointer<soa_vector<Particle>>] s1{location(b1)}
  [Pointer<soa_vector<Particle>>] s2{location(b2)}

  [soa_vector<Particle>] c0{soa_vector<Particle>()}
  [soa_vector<Particle>] c1{soa_vector<Particle>()}
  [Pointer<soa_vector<Particle>>] t0{location(c0)}
  [Pointer<soa_vector<Particle>>] t1{location(c1)}

  return(plus(score_ptrs(r0, r1, r2),
              plus(forward(s0, s1, s2),
                   forward_mixed(t0, t1))))
}
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_pointer_soa_vector.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_pointer_soa_vector").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("native materializes variadic soa_vector packs with indexed count methods") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
score_soas([args<soa_vector<Particle>>] values) {
  [soa_vector<Particle>] head{values[0i32]}
  [soa_vector<Particle>] tail{values[2i32]}
  return(plus(count(values), plus(count(head), count(tail))))
}

[return<int>]
forward([args<soa_vector<Particle>>] values) {
  return(score_soas([spread] values))
}

[effects(heap_alloc), return<int>]
forward_mixed([args<soa_vector<Particle>>] values) {
  return(score_soas(soa_vector<Particle>(), [spread] values))
}

[effects(heap_alloc), return<int>]
main() {
  return(plus(score_soas(soa_vector<Particle>(), soa_vector<Particle>(), soa_vector<Particle>()),
              plus(forward(soa_vector<Particle>(), soa_vector<Particle>(), soa_vector<Particle>()),
                   forward_mixed(soa_vector<Particle>(), soa_vector<Particle>()))))
}
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_soa_vector.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_soa_vector").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("native materializes variadic map packs with indexed count methods") {
  const std::string source = R"(
[return<int>]
score_maps([args<map<i32, i32>>] values) {
  return(plus(values[0i32].count(), values[2i32].count()))
}

[return<int>]
forward([args<map<i32, i32>>] values) {
  return(score_maps([spread] values))
}

[return<int>]
forward_mixed([args<map<i32, i32>>] values) {
  return(score_maps(map<i32, i32>(1i32, 2i32), [spread] values))
}

[return<int>]
main() {
  return(plus(score_maps(map<i32, i32>(1i32, 2i32, 3i32, 4i32),
                         map<i32, i32>(5i32, 6i32),
                         map<i32, i32>(7i32, 8i32, 9i32, 10i32, 11i32, 12i32)),
              plus(forward(map<i32, i32>(13i32, 14i32),
                           map<i32, i32>(15i32, 16i32, 17i32, 18i32),
                           map<i32, i32>(19i32, 20i32)),
                   forward_mixed(map<i32, i32>(21i32, 22i32, 23i32, 24i32),
                                 map<i32, i32>(25i32, 26i32, 27i32, 28i32, 29i32, 30i32)))))
}
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_map.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_map").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 11);
}

TEST_CASE("native materializes variadic map packs with indexed tryAt inference") {
  const std::string source = R"(
[return<int>]
score_maps([args<map<i32, i32>>] values) {
  [auto] head{/std/collections/map/at_unsafe<i32, i32>(at(values, 0i32), 3i32)}
  [auto] tailMissing{
      Result.error(/std/collections/map/tryAt<i32, i32>(at(values, minus(count(values), 1i32)), 99i32))}
  return(if(tailMissing,
            then(){ return(plus(head, /std/collections/map/at_unsafe<i32, i32>(at(values, minus(count(values), 1i32)), 11i32))) },
            else(){ return(0i32) }))
}

[return<int>]
forward([args<map<i32, i32>>] values) {
  return(score_maps([spread] values))
}

[return<int>]
forward_mixed([args<map<i32, i32>>] values) {
  return(score_maps(map<i32, i32>(1i32, 2i32, 3i32, 8i32), [spread] values))
}

[return<int>]
main() {
  return(plus(score_maps(map<i32, i32>(1i32, 2i32, 3i32, 4i32),
                         map<i32, i32>(5i32, 6i32),
                         map<i32, i32>(7i32, 8i32, 9i32, 10i32, 11i32, 12i32)),
              plus(forward(map<i32, i32>(1i32, 2i32, 3i32, 6i32),
                           map<i32, i32>(5i32, 6i32),
                           map<i32, i32>(7i32, 8i32, 9i32, 10i32, 11i32, 14i32)),
                   forward_mixed(map<i32, i32>(7i32, 8i32, 9i32, 10i32, 11i32, 16i32)))))
}
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_map_tryat.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_map_tryat").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 60);
}

TEST_CASE("native materializes variadic experimental map packs with indexed canonical count calls") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<int> effects(heap_alloc)]
score_maps([args<Map<i32, i32>>] values) {
  [Map<i32, i32>] head{at(values, 0i32)}
  [Map<i32, i32>] tail{at(values, 2i32)}
  return(plus(/std/collections/experimental_map/mapCount<i32, i32>(head),
              /std/collections/experimental_map/mapCount<i32, i32>(tail)))
}

[return<int> effects(heap_alloc)]
forward([args<Map<i32, i32>>] values) {
  return(score_maps([spread] values))
}

[return<int> effects(heap_alloc)]
forward_mixed([args<Map<i32, i32>>] values) {
  return(score_maps(mapSingle(1i32, 2i32), [spread] values))
}

[return<int> effects(heap_alloc)]
main() {
  return(plus(score_maps(mapPair(1i32, 2i32, 3i32, 4i32),
                         mapSingle(5i32, 6i32),
                         mapTriple(7i32, 8i32, 9i32, 10i32, 11i32, 12i32)),
              plus(forward(mapSingle(13i32, 14i32),
                           mapPair(15i32, 16i32, 17i32, 18i32),
                           mapSingle(19i32, 20i32)),
                   forward_mixed(mapPair(21i32, 22i32, 23i32, 24i32),
                                 mapTriple(25i32, 26i32, 27i32, 28i32, 29i32, 30i32)))))
}
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_experimental_map_count.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_experimental_map_count").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 11);
}

TEST_CASE("native preserves if expression values in arithmetic context") {
  const std::string source = R"(
[return<int>]
main() {
  [bool] on{true}
  [bool] off{false}
  [int] whenOn{plus(0i32, if(on, then() { 1i32 }, else() { 0i32 }))}
  [int] whenOff{plus(0i32, if(off, then() { 1i32 }, else() { 0i32 }))}
  return(plus(whenOn, whenOff))
}
)";
  const std::string srcPath = writeTemp("compile_native_if_expr_stack_merge.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_if_expr_stack_merge").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs native float arithmetic") {
  const std::string source = R"(
[return<int>]
main() {
  [f32] a{1.5f32}
  [f32] b{2.0f32}
  [f32] c{multiply(plus(a, b), 2.0f32)}
  return(convert<int>(c))
}
)";
  const std::string srcPath = writeTemp("compile_native_float.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_float_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs native image api contract deterministically") {
  const std::string source = R"(
import /std/image/*
import /std/collections/*

[effects(heap_alloc, io_out, file_write), return<int>]
main() {
  [i32 mut] width{0i32}
  [i32 mut] height{0i32}
  [vector<i32> mut] pixels{vector<i32>()}
  print_line(Result.why(/std/image/ppm/read(width, height, pixels, "input.ppm"utf8)))
  print_line(Result.why(/std/image/ppm/write("output.ppm"utf8, width, height, pixels)))
  print_line(Result.why(/std/image/png/read(width, height, pixels, "input.png"utf8)))
  print_line(Result.why(/std/image/png/write("output.png"utf8, width, height, pixels)))
  return(plus(width, height))
}
)";
  const std::string srcPath = writeTemp("compile_native_image_api_unsupported.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_image_api_unsupported").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_image_api_unsupported.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 0);
  CHECK(readFile(outPath) ==
        "image_invalid_operation\n"
        "image_invalid_operation\n"
        "image_invalid_operation\n"
        "image_invalid_operation\n");
}

TEST_CASE("native uses stdlib ImageError result helpers") {
  const std::string source = R"(
import /std/image/*

[return<int> effects(io_out)]
main() {
  print_line(Result.why(/ImageError/status(imageReadUnsupported())))
  print_line(Result.why(/ImageError/result<i32>(imageWriteUnsupported())))
  print_line(Result.why(/ImageError/result<i32>(imageInvalidOperation())))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_image_error_result_helpers.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_image_error_result_helpers").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_image_error_result_helpers.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 0);
  CHECK(readFile(outPath) ==
        "image_read_unsupported\n"
        "image_write_unsupported\n"
        "image_invalid_operation\n");
}

TEST_CASE("native uses stdlib ImageError why wrapper") {
  const std::string source = R"(
import /std/image/*

[return<int> effects(io_out)]
main() {
  [ImageError] err{imageReadUnsupported()}
  [Result<ImageError>] methodStatus{/ImageError/status(err)}
  [Result<i32, ImageError>] methodValueStatus{/ImageError/result<i32>(err)}
  print_line(/ImageError/why(err))
  print_line(/ImageError/why(err))
  print_line(/ImageError/why(err))
  print_line(Result.why(imageErrorStatus(err)))
  print_line(Result.why(methodStatus))
  print_line(Result.why(methodValueStatus))
  print_line(Result.why(methodStatus))
  print_line(Result.why(methodValueStatus))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_image_error_why_wrapper.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_image_error_why_wrapper").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_image_error_why_wrapper.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 0);
  CHECK(readFile(outPath) ==
        "image_read_unsupported\n"
        "image_read_unsupported\n"
        "image_read_unsupported\n"
        "image_read_unsupported\n"
        "image_read_unsupported\n"
        "image_read_unsupported\n"
        "image_read_unsupported\n"
        "image_read_unsupported\n");
}

TEST_CASE("native uses stdlib ImageError constructor wrappers") {
  const std::string source = R"(
import /std/image/*

[return<int> effects(io_out)]
main() {
  print_line(Result.why(imageErrorStatus(/ImageError/read_unsupported())))
  print_line(Result.why(imageErrorStatus(/ImageError/write_unsupported())))
  print_line(Result.why(imageErrorStatus(/ImageError/invalid_operation())))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_image_error_constructor_wrappers.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_image_error_constructor_wrappers").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_image_error_constructor_wrappers.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 0);
  CHECK(readFile(outPath) ==
        "image_read_unsupported\n"
        "image_write_unsupported\n"
        "image_invalid_operation\n");
}

TEST_CASE("native uses stdlib GfxError result helpers") {
  const std::string source = R"(
import /std/gfx/experimental/*

[return<int> effects(io_out)]
main() {
  [GfxError] err{queueSubmitFailed()}
  print_line(Result.why(gfxErrorStatus(queueSubmitFailed())))
  print_line(Result.why(gfxErrorResult<i32>(framePresentFailed())))
  print_line(Result.why(gfxErrorStatus(err)))
  print_line(Result.why(gfxErrorStatus(err)))
  print_line(Result.why(gfxErrorResult<i32>(err)))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_gfx_error_result_helpers.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_gfx_error_result_helpers").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_gfx_error_result_helpers.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 0);
  CHECK(readFile(outPath) ==
        "queue_submit_failed\n"
        "frame_present_failed\n"
        "queue_submit_failed\n"
        "queue_submit_failed\n"
        "queue_submit_failed\n");
}

TEST_CASE("native uses canonical stdlib GfxError result helpers") {
  const std::string source = R"(
import /std/gfx/*

[return<int> effects(io_out)]
main() {
  [GfxError] err{queueSubmitFailed()}
  print_line(Result.why(/GfxError/status(queueSubmitFailed())))
  print_line(Result.why(/GfxError/result<i32>(framePresentFailed())))
  print_line(Result.why(gfxErrorStatus(err)))
  print_line(Result.why(gfxErrorResult<i32>(err)))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_canonical_gfx_error_result_helpers.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_canonical_gfx_error_result_helpers").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_canonical_gfx_error_result_helpers.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 0);
  CHECK(readFile(outPath) ==
        "queue_submit_failed\n"
        "frame_present_failed\n"
        "queue_submit_failed\n"
        "queue_submit_failed\n");
}

TEST_CASE("native uses canonical stdlib GfxError why wrapper") {
  const std::string source = R"(
import /std/gfx/*

[return<int> effects(io_out)]
main() {
  [GfxError] err{queueSubmitFailed()}
  [Result<GfxError>] methodStatus{gfxErrorStatus(err)}
  [Result<i32, GfxError>] methodValueStatus{gfxErrorResult<i32>(err)}
  print_line(/GfxError/why(err))
  print_line(/GfxError/why(err))
  print_line(/GfxError/why(err))
  print_line(Result.why(gfxErrorStatus(err)))
  print_line(Result.why(methodStatus))
  print_line(Result.why(methodValueStatus))
  print_line(Result.why(methodStatus))
  print_line(Result.why(methodValueStatus))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_canonical_gfx_error_why_wrapper.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_canonical_gfx_error_why_wrapper").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_canonical_gfx_error_why_wrapper.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 0);
  CHECK(readFile(outPath) ==
        "queue_submit_failed\n"
        "queue_submit_failed\n"
        "queue_submit_failed\n"
        "queue_submit_failed\n"
        "queue_submit_failed\n"
        "queue_submit_failed\n"
        "queue_submit_failed\n"
        "queue_submit_failed\n");
}

TEST_CASE("native uses canonical stdlib GfxError constructor wrappers") {
  const std::string source = R"(
import /std/gfx/*

[return<int> effects(io_out)]
main() {
  print_line(Result.why(gfxErrorStatus(/GfxError/window_create_failed())))
  print_line(Result.why(gfxErrorStatus(/GfxError/device_create_failed())))
  print_line(Result.why(gfxErrorStatus(/GfxError/frame_present_failed())))
  return(0i32)
}
)";
  const std::string srcPath =
      writeTemp("compile_native_canonical_gfx_error_constructor_wrappers.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_canonical_gfx_error_constructor_wrappers").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_canonical_gfx_error_constructor_wrappers.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 0);
  CHECK(readFile(outPath) ==
        "window_create_failed\n"
        "device_create_failed\n"
        "frame_present_failed\n");
}

TEST_CASE("native uses stdlib experimental Buffer helper methods") {
  const std::string source = R"(
import /std/gfx/experimental/*

[return<int>]
main() {
  [Buffer<i32>] emptyBuffer{Buffer<i32>([token] 0i32, [elementCount] 0i32)}
  [Buffer<i32>] fullBuffer{Buffer<i32>([token] 7i32, [elementCount] 4i32)}
  if(not(emptyBuffer.empty())) {
    return(90i32)
  }
  if(emptyBuffer.is_valid()) {
    return(91i32)
  }
  if(fullBuffer.empty()) {
    return(92i32)
  }
  if(not(fullBuffer.is_valid())) {
    return(93i32)
  }
  return(plus(fullBuffer.count(), /std/gfx/experimental/Buffer/count(emptyBuffer)))
}
)";
  const std::string srcPath = writeTemp("compile_native_experimental_gfx_buffer_helpers.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_experimental_gfx_buffer_helpers").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 4);
}

TEST_CASE("native uses canonical stdlib Buffer helper methods") {
  const std::string source = R"(
import /std/gfx/*

[return<int>]
main() {
  [Buffer<i32>] emptyBuffer{Buffer<i32>([token] 0i32, [elementCount] 0i32)}
  [Buffer<i32>] fullBuffer{Buffer<i32>([token] 7i32, [elementCount] 5i32)}
  if(not(emptyBuffer.empty())) {
    return(90i32)
  }
  if(emptyBuffer.is_valid()) {
    return(91i32)
  }
  if(fullBuffer.empty()) {
    return(92i32)
  }
  if(not(fullBuffer.is_valid())) {
    return(93i32)
  }
  return(plus(fullBuffer.count(), /std/gfx/Buffer/count(emptyBuffer)))
}
)";
  const std::string srcPath = writeTemp("compile_native_canonical_gfx_buffer_helpers.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_canonical_gfx_buffer_helpers").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 5);
}

TEST_CASE("native uses stdlib experimental Buffer readback helpers") {
  const std::string source = R"(
import /std/gfx/experimental/*

[effects(gpu_dispatch), return<int>]
main() {
  [Buffer<i32>] data{/std/gpu/buffer<i32>(3i32)}
  [array<i32>] viaMethod{data.readback()}
  [array<i32>] viaDirect{/std/gfx/experimental/Buffer/readback(data)}
  return(plus(plus(viaMethod.count(), viaDirect.count()), plus(viaMethod[0i32], viaDirect[2i32])))
}
)";
  const std::string srcPath = writeTemp("compile_native_experimental_gfx_buffer_readback_helpers.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_experimental_gfx_buffer_readback_helpers").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 6);
}

TEST_CASE("native uses canonical stdlib Buffer readback helpers") {
  const std::string source = R"(
import /std/gfx/*

[effects(gpu_dispatch), return<int>]
main() {
  [Buffer<i32>] data{/std/gpu/buffer<i32>(3i32)}
  [array<i32>] viaMethod{data.readback()}
  [array<i32>] viaDirect{/std/gfx/Buffer/readback(data)}
  return(plus(plus(viaMethod.count(), viaDirect.count()), plus(viaMethod[0i32], viaDirect[2i32])))
}
)";
  const std::string srcPath = writeTemp("compile_native_canonical_gfx_buffer_readback_helpers.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_canonical_gfx_buffer_readback_helpers").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 6);
}

TEST_CASE("native uses stdlib experimental Buffer allocation helpers") {
  const std::string source = R"(
import /std/gfx/experimental/*

[effects(gpu_dispatch), return<int>]
main() {
  [Buffer<i32>] data{/std/gfx/experimental/Buffer/allocate<i32>(3i32)}
  [array<i32>] out{data.readback()}
  return(plus(/std/gfx/experimental/Buffer/count(data), out.count()))
}
)";
  const std::string srcPath = writeTemp("compile_native_experimental_gfx_buffer_allocate_helpers.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_experimental_gfx_buffer_allocate_helpers").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 6);
}

TEST_CASE("native uses canonical stdlib Buffer allocation helpers") {
  const std::string source = R"(
import /std/gfx/*

[effects(gpu_dispatch), return<int>]
main() {
  [Buffer<i32>] data{/std/gfx/Buffer/allocate<i32>(3i32)}
  [array<i32>] out{data.readback()}
  return(plus(/std/gfx/Buffer/count(data), out.count()))
}
)";
  const std::string srcPath = writeTemp("compile_native_canonical_gfx_buffer_allocate_helpers.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_canonical_gfx_buffer_allocate_helpers").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 6);
}

TEST_CASE("native uses stdlib experimental Buffer constructor entry point") {
  const std::string source = R"(
import /std/gfx/experimental/*

[effects(gpu_dispatch), return<int>]
main() {
  [Buffer<i32>] data{Buffer<i32>(3i32)}
  [array<i32>] out{data.readback()}
  return(plus(/std/gfx/experimental/Buffer/count(data), out.count()))
}
)";
  const std::string srcPath = writeTemp("compile_native_experimental_gfx_buffer_constructor.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_experimental_gfx_buffer_constructor").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 6);
}

TEST_CASE("native uses canonical stdlib Buffer constructor entry point") {
  const std::string source = R"(
import /std/gfx/*

[effects(gpu_dispatch), return<int>]
main() {
  [Buffer<i32>] data{Buffer<i32>(3i32)}
  [array<i32>] out{data.readback()}
  return(plus(/std/gfx/Buffer/count(data), out.count()))
}
)";
  const std::string srcPath = writeTemp("compile_native_canonical_gfx_buffer_constructor.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_canonical_gfx_buffer_constructor").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 6);
}

TEST_CASE("native uses stdlib experimental Buffer upload helpers") {
  const std::string source = R"(
import /std/gfx/experimental/*

[effects(gpu_dispatch), return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32, 3i32)}
  [Buffer<i32>] data{/std/gfx/experimental/Buffer/upload(values)}
  [array<i32>] out{data.readback()}
  return(plus(plus(out.count(), out[0i32]), out[2i32]))
}
)";
  const std::string srcPath = writeTemp("compile_native_experimental_gfx_buffer_upload_helpers.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_experimental_gfx_buffer_upload_helpers").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("native uses canonical stdlib Buffer upload helpers") {
  const std::string source = R"(
import /std/gfx/*

[effects(gpu_dispatch), return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32, 3i32)}
  [Buffer<i32>] data{/std/gfx/Buffer/upload(values)}
  [array<i32>] out{data.readback()}
  return(plus(plus(out.count(), out[0i32]), out[2i32]))
}
)";
  const std::string srcPath = writeTemp("compile_native_canonical_gfx_buffer_upload_helpers.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_canonical_gfx_buffer_upload_helpers").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs native ppm read for ascii p3 inputs") {
  const std::string inPath = (testScratchPath("") / "primec_native_image_read.ppm").string();
  {
    std::ofstream file(inPath, std::ios::binary);
    REQUIRE(file.good());
    file << "P3\n2 1\n255\n255 0 0 0 255 128\n";
    REQUIRE(file.good());
  }

  const std::string escapedPath = escapeStringLiteral(inPath);
  const std::string source = injectEscapedPath(R"(
import /std/image/*
import /std/collections/*

[effects(heap_alloc, io_out, file_read), return<int>]
main() {
  [i32 mut] width{0i32}
  [i32 mut] height{0i32}
  [vector<i32> mut] pixels{vector<i32>()}
  [Result<ImageError>] status{/std/image/ppm/read(width, height, pixels, "__PATH__"utf8)}
  if(Result.error(status),
     then() {
       print_line(Result.why(status))
       return(1i32)
     },
     else() { })
  print_line(width)
  print_line(height)
  print_line(vectorCount<i32>(pixels))
  print_line(pixels[0i32])
  print_line(pixels[1i32])
  print_line(pixels[2i32])
  print_line(pixels[3i32])
  print_line(pixels[4i32])
  print_line(pixels[5i32])
  return(plus(width, height))
}
)", escapedPath);
  const std::string srcPath = writeTemp("compile_native_image_read_ppm.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_image_read_ppm").string();
  const std::string outPath = (testScratchPath("") / "primec_native_image_read_ppm.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 3);
  CHECK(readFile(outPath) ==
        "2\n"
        "1\n"
        "6\n"
        "255\n"
        "0\n"
        "0\n"
        "0\n"
        "255\n"
        "128\n");
}

TEST_CASE("compiles and runs native ppm read for binary p6 inputs") {
  const std::string inPath = (testScratchPath("") / "primec_native_image_read_p6.ppm").string();
  {
    std::ofstream file(inPath, std::ios::binary);
    REQUIRE(file.good());
    file << "P6\n2 1\n255\n";
    file.put(static_cast<char>(255));
    file.put(static_cast<char>(0));
    file.put(static_cast<char>(0));
    file.put(static_cast<char>(0));
    file.put(static_cast<char>(255));
    file.put(static_cast<char>(128));
    REQUIRE(file.good());
  }

  const std::string escapedPath = escapeStringLiteral(inPath);
  const std::string source = injectEscapedPath(R"(
import /std/image/*
import /std/collections/*

[effects(heap_alloc, io_out, file_write), return<int>]
main() {
  [i32 mut] width{0i32}
  [i32 mut] height{0i32}
  [vector<i32> mut] pixels{vector<i32>()}
  [Result<ImageError>] status{/std/image/ppm/read(width, height, pixels, "__PATH__"utf8)}
  if(Result.error(status),
     then() {
       print_line(Result.why(status))
       return(1i32)
     },
     else() { })
  print_line(width)
  print_line(height)
  print_line(vectorCount<i32>(pixels))
  print_line(pixels[0i32])
  print_line(pixels[1i32])
  print_line(pixels[2i32])
  print_line(pixels[3i32])
  print_line(pixels[4i32])
  print_line(pixels[5i32])
  return(plus(width, height))
}
)", escapedPath);
  const std::string srcPath = writeTemp("compile_native_image_read_p6.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_image_read_p6").string();
  const std::string outPath = (testScratchPath("") / "primec_native_image_read_p6.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 3);
  CHECK(readFile(outPath) ==
        "2\n"
        "1\n"
        "6\n"
        "255\n"
        "0\n"
        "0\n"
        "0\n"
        "255\n"
        "128\n");
}

TEST_CASE("compiles and rejects truncated native binary ppm reads deterministically") {
  const std::string inPath =
      (testScratchPath("") / "primec_native_image_read_p6_truncated.ppm").string();
  {
    std::ofstream file(inPath, std::ios::binary);
    REQUIRE(file.good());
    file << "P6\n2 1\n255\n";
    file.put(static_cast<char>(255));
    file.put(static_cast<char>(0));
    file.put(static_cast<char>(0));
    file.put(static_cast<char>(0));
    file.put(static_cast<char>(255));
    REQUIRE(file.good());
  }

  const std::string escapedPath = escapeStringLiteral(inPath);
  const std::string source = injectEscapedPath(R"(
import /std/image/*
import /std/collections/*

[effects(heap_alloc, io_out, file_write), return<int>]
main() {
  [i32 mut] width{7i32}
  [i32 mut] height{9i32}
  [vector<i32> mut] pixels{vector<i32>(1i32, 2i32, 3i32)}
  [Result<ImageError>] status{/std/image/ppm/read(width, height, pixels, "__PATH__"utf8)}
  print_line(Result.why(status))
  print_line(width)
  print_line(height)
  print_line(vectorCount<i32>(pixels))
  return(0i32)
}
)", escapedPath);
  const std::string srcPath = writeTemp("compile_native_image_read_p6_truncated.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_image_read_p6_truncated").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_image_read_p6_truncated.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 0);
  CHECK(readFile(outPath) ==
        "image_invalid_operation\n"
        "0\n"
        "0\n"
        "0\n");
}

TEST_CASE("compiles and rejects oversized native image read dimensions before overflow") {
  const std::string ppmInPath =
      (testScratchPath("") / "primec_native_image_read_overflow.ppm").string();
  {
    std::ofstream file(ppmInPath, std::ios::binary);
    REQUIRE(file.good());
    file << "P3\n1431655766 1\n255\n7 9\n";
    REQUIRE(file.good());
  }

  const std::string pngInPath =
      (testScratchPath("") / "primec_native_image_read_overflow.png").string();
  {
    const std::vector<unsigned char> pngBytes = withValidPngCrcs({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x55, 0x55, 0x55, 0x56, 0x00, 0x00, 0x00, 0x01,
        0x08, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x0e, 0x49, 0x44, 0x41, 0x54,
        0x78, 0x01, 0x01, 0x03, 0x00, 0xfc, 0xff, 0x00,
        0x07, 0x09, 0x00, 0x1a, 0x00, 0x11, 0x00, 0x00,
        0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0x00, 0x00,
        0x00, 0x00,
    });
    std::ofstream file(pngInPath, std::ios::binary);
    REQUIRE(file.good());
    file.write(reinterpret_cast<const char *>(pngBytes.data()), static_cast<std::streamsize>(pngBytes.size()));
    REQUIRE(file.good());
  }

  const std::string escapedPpmPath = escapeStringLiteral(ppmInPath);
  const std::string escapedPngPath = escapeStringLiteral(pngInPath);
  std::string source = R"(
import /std/image/*
import /std/collections/*

[effects(heap_alloc, io_out, file_write), return<int>]
main() {
  [i32 mut] width{7i32}
  [i32 mut] height{9i32}
  [vector<i32> mut] pixels{vector<i32>(1i32, 2i32, 3i32)}
  [Result<ImageError>] ppmStatus{/std/image/ppm/read(width, height, pixels, "__PPM_PATH__"utf8)}
  print_line(Result.why(ppmStatus))
  print_line(width)
  print_line(height)
  print_line(vectorCount<i32>(pixels))
  assign(width, 11i32)
  assign(height, 13i32)
  assign(pixels, vector<i32>(4i32, 5i32, 6i32))
  [Result<ImageError>] pngStatus{/std/image/png/read(width, height, pixels, "__PNG_PATH__"utf8)}
  print_line(Result.why(pngStatus))
  print_line(width)
  print_line(height)
  print_line(vectorCount<i32>(pixels))
  return(0i32)
}
)";
  const size_t ppmPathOffset = source.find("__PPM_PATH__");
  REQUIRE(ppmPathOffset != std::string::npos);
  source.replace(ppmPathOffset, std::string("__PPM_PATH__").size(), escapedPpmPath);
  const size_t pngPathOffset = source.find("__PNG_PATH__");
  REQUIRE(pngPathOffset != std::string::npos);
  source.replace(pngPathOffset, std::string("__PNG_PATH__").size(), escapedPngPath);

  const std::string srcPath = writeTemp("compile_native_image_read_overflow.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_image_read_overflow").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_image_read_overflow.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 0);
  CHECK(readFile(outPath) ==
        "image_invalid_operation\n"
        "0\n"
        "0\n"
        "0\n"
        "image_invalid_operation\n"
        "0\n"
        "0\n"
        "0\n");
}

TEST_CASE("compiles and runs native ppm write for ascii p3 outputs") {
  const std::filesystem::path outPath = testScratchPath("") / "primec_native_image_write.ppm";
  std::error_code ec;
  std::filesystem::remove(outPath, ec);

  const std::string escapedPath = escapeStringLiteral(outPath.string());
  const std::string source = injectEscapedPath(R"(
import /std/image/*
import /std/collections/*

[effects(heap_alloc, file_write), return<int>]
main() {
  [vector<i32>] pixels{vector<i32>(255i32, 0i32, 0i32, 0i32, 255i32, 128i32)}
  [Result<ImageError>] status{/std/image/ppm/write("__PATH__"utf8, 2i32, 1i32, pixels)}
  if(Result.error(status),
     then() { return(1i32) },
     else() { })
  return(0i32)
}
)", escapedPath);
  const std::string srcPath = writeTemp("compile_native_image_write_ppm.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_image_write_ppm").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
  CHECK(readFile(outPath.string()) ==
        "P3\n"
        "2 1\n"
        "255\n"
        "255\n"
        "0\n"
        "0\n"
        "0\n"
        "255\n"
        "128\n");
}

TEST_CASE("compiles and rejects invalid native ppm write inputs deterministically") {
  const std::filesystem::path outPath =
      testScratchPath("") / "primec_native_image_write_invalid.ppm";
  std::error_code ec;
  std::filesystem::remove(outPath, ec);

  const std::string escapedPath = escapeStringLiteral(outPath.string());
  const std::string source = injectEscapedPath(R"(
import /std/image/*
import /std/collections/*

[effects(heap_alloc, io_out, file_write), return<int>]
main() {
  [vector<i32>] pixels{vector<i32>(255i32, 0i32, 0i32)}
  [Result<ImageError>] status{/std/image/ppm/write("__PATH__"utf8, 2i32, 1i32, pixels)}
  print_line(Result.why(status))
  return(0i32)
}
)", escapedPath);
  const std::string srcPath = writeTemp("compile_native_image_write_invalid.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_image_write_invalid").string();
  const std::string stdoutPath =
      (testScratchPath("") / "primec_native_image_write_invalid.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + stdoutPath) == 0);
  CHECK(readFile(stdoutPath) == "image_invalid_operation\n");
  CHECK(!std::filesystem::exists(outPath));
}

TEST_CASE("compiles and runs native png write for deterministic rgb outputs") {
  const std::filesystem::path outPath = testScratchPath("") / "primec_native_image_write.png";
  std::error_code ec;
  std::filesystem::remove(outPath, ec);

  const std::string escapedPath = escapeStringLiteral(outPath.string());
  const std::string source = injectEscapedPath(R"(
import /std/image/*
import /std/collections/*

[effects(heap_alloc, file_write), return<int>]
main() {
  [vector<i32>] pixels{vector<i32>(255i32, 0i32, 0i32, 0i32, 255i32, 128i32)}
  [Result<ImageError>] status{/std/image/png/write("__PATH__"utf8, 2i32, 1i32, pixels)}
  if(Result.error(status),
     then() { return(1i32) },
     else() { })
  return(0i32)
}
)", escapedPath);
  const std::string srcPath = writeTemp("compile_native_image_write_png.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_image_write_png").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
  const std::vector<unsigned char> expected = withValidPngCrcs({
      0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
      0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
      0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01,
      0x08, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x12, 0x49, 0x44, 0x41, 0x54,
      0x78, 0x01, 0x01, 0x07, 0x00, 0xf8, 0xff, 0x00,
      0xff, 0x00, 0x00, 0x00, 0xff, 0x80, 0x08, 0x7f,
      0x02, 0x7f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0x00, 0x00,
      0x00, 0x00,
  });
  CHECK(readBinaryFile(outPath.string()) == expected);
}

TEST_CASE("compiles and rejects invalid native png write inputs deterministically") {
  const std::filesystem::path outPath =
      testScratchPath("") / "primec_native_image_write_invalid.png";
  std::error_code ec;
  std::filesystem::remove(outPath, ec);

  const std::string escapedPath = escapeStringLiteral(outPath.string());
  const std::string source = injectEscapedPath(R"(
import /std/image/*
import /std/collections/*

[effects(heap_alloc, io_out, file_write), return<int>]
main() {
  [vector<i32>] pixels{vector<i32>(255i32, 0i32, 0i32)}
  [Result<ImageError>] status{/std/image/png/write("__PATH__"utf8, 2i32, 1i32, pixels)}
  print_line(Result.why(status))
  return(0i32)
}
)", escapedPath);
  const std::string srcPath = writeTemp("compile_native_image_write_invalid_png.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_image_write_invalid_png").string();
  const std::string stdoutPath =
      (testScratchPath("") / "primec_native_image_write_invalid_png.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + stdoutPath) == 0);
  CHECK(readFile(stdoutPath) == "image_invalid_operation\n");
  CHECK(!std::filesystem::exists(outPath));
}

TEST_CASE("rejects oversized native image write dimensions before overflow") {
  const std::filesystem::path ppmOutPath =
      testScratchPath("") / "primec_native_image_write_overflow.ppm";
  const std::filesystem::path pngOutPath =
      testScratchPath("") / "primec_native_image_write_overflow.png";
  std::error_code ec;
  std::filesystem::remove(ppmOutPath, ec);
  std::filesystem::remove(pngOutPath, ec);

  const std::string escapedPpmPath = escapeStringLiteral(ppmOutPath.string());
  const std::string escapedPngPath = escapeStringLiteral(pngOutPath.string());
  std::string source = R"(
import /std/image/*
import /std/collections/*

[effects(heap_alloc, io_out, file_write), return<int>]
main() {
  [vector<i32>] pixels{vector<i32>(7i32, 9i32)}
  print_line(Result.why(/std/image/ppm/write("__PPM_PATH__"utf8, 1431655766i32, 1i32, pixels)))
  print_line(Result.why(/std/image/png/write("__PNG_PATH__"utf8, 1431655766i32, 1i32, pixels)))
  return(0i32)
}
)";
  const size_t ppmPathOffset = source.find("__PPM_PATH__");
  REQUIRE(ppmPathOffset != std::string::npos);
  source.replace(ppmPathOffset, std::string("__PPM_PATH__").size(), escapedPpmPath);
  const size_t pngPathOffset = source.find("__PNG_PATH__");
  REQUIRE(pngPathOffset != std::string::npos);
  source.replace(pngPathOffset, std::string("__PNG_PATH__").size(), escapedPngPath);

  const std::string srcPath = writeTemp("compile_native_image_write_overflow.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_image_write_overflow").string();
  const std::string stdoutPath =
      (testScratchPath("") / "primec_native_image_write_overflow.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + stdoutPath) == 0);
  CHECK(readFile(stdoutPath) ==
        "image_invalid_operation\n"
        "image_invalid_operation\n");
  CHECK(!std::filesystem::exists(ppmOutPath));
  CHECK(!std::filesystem::exists(pngOutPath));
}

TEST_CASE("compiles and runs native png read for stored rgb inputs") {
  const std::string inPath = (testScratchPath("") / "primec_native_image_read.png").string();
  {
    const std::vector<unsigned char> pngBytes = withValidPngCrcs({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
        0x08, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x0f, 0x49, 0x44, 0x41,
        0x54,
        0x78, 0x01, 0x01, 0x04, 0x00, 0xfb, 0xff, 0x00,
        0xff, 0x00, 0x00, 0x03, 0x01, 0x01, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x49, 0x45, 0x4e, 0x44,
        0x00, 0x00, 0x00, 0x00,
    });
    std::ofstream file(inPath, std::ios::binary);
    REQUIRE(file.good());
    file.write(reinterpret_cast<const char *>(pngBytes.data()), static_cast<std::streamsize>(pngBytes.size()));
    REQUIRE(file.good());
  }

  const std::string escapedPath = escapeStringLiteral(inPath);
  const std::string source = injectEscapedPath(R"(
import /std/image/*
import /std/collections/*

[effects(heap_alloc, io_out, file_write), return<int>]
main() {
  [i32 mut] width{0i32}
  [i32 mut] height{0i32}
  [vector<i32> mut] pixels{vector<i32>()}
  [Result<ImageError>] status{/std/image/png/read(width, height, pixels, "__PATH__"utf8)}
  if(Result.error(status),
     then() {
       print_line(Result.why(status))
       return(1i32)
     },
     else() { })
  print_line(width)
  print_line(height)
  print_line(vectorCount<i32>(pixels))
  print_line(pixels[0i32])
  print_line(pixels[1i32])
  print_line(pixels[2i32])
  return(plus(width, height))
}
)", escapedPath);
  const std::string srcPath = writeTemp("compile_native_image_read_png.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_image_read_png").string();
  const std::string outPath = (testScratchPath("") / "primec_native_image_read_png.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 2);
  CHECK(readFile(outPath) ==
        "1\n"
        "1\n"
        "3\n"
        "255\n"
        "0\n"
        "0\n");
}

TEST_CASE("compiles and runs native png read for sub-filter grayscale inputs") {
  const std::string inPath = (testScratchPath("") / "primec_native_image_read_gray_sub.png").string();
  {
    const std::vector<unsigned char> pngBytes = withValidPngCrcs({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01,
        0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x0e, 0x49, 0x44, 0x41,
        0x54, 0x78, 0x01, 0x01, 0x03, 0x00, 0xfc, 0xff,
        0x01, 0x0a, 0x14, 0x00, 0x2e, 0x00, 0x20, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x49, 0x45, 0x4e, 0x44, 0x00, 0x00, 0x00, 0x00,
    });
    std::ofstream file(inPath, std::ios::binary);
    REQUIRE(file.good());
    file.write(reinterpret_cast<const char *>(pngBytes.data()), static_cast<std::streamsize>(pngBytes.size()));
    REQUIRE(file.good());
  }

  const std::string escapedPath = escapeStringLiteral(inPath);
  const std::string source = injectEscapedPath(R"(
import /std/image/*
import /std/collections/*

[effects(heap_alloc, io_out, file_write), return<int>]
main() {
  [i32 mut] width{0i32}
  [i32 mut] height{0i32}
  [vector<i32> mut] pixels{vector<i32>()}
  [Result<ImageError>] status{/std/image/png/read(width, height, pixels, "__PATH__"utf8)}
  if(Result.error(status),
     then() {
       print_line(Result.why(status))
       return(1i32)
     },
     else() { })
  print_line(width)
  print_line(height)
  print_line(vectorCount<i32>(pixels))
  print_line(pixels[0i32])
  print_line(pixels[1i32])
  print_line(pixels[2i32])
  print_line(pixels[3i32])
  print_line(pixels[4i32])
  print_line(pixels[5i32])
  return(plus(width, height))
}
)", escapedPath);
  const std::string srcPath = writeTemp("compile_native_image_read_gray_sub_png.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_image_read_gray_sub_png").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_image_read_gray_sub_png.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 3);
  CHECK(readFile(outPath) ==
        "2\n"
        "1\n"
        "6\n"
        "10\n"
        "10\n"
        "10\n"
        "30\n"
        "30\n"
        "30\n");
}

TEST_CASE("compiles and runs native png read for sub-filter grayscale-alpha inputs") {
  const std::string inPath =
      (testScratchPath("") / "primec_native_image_read_gray_alpha_sub.png").string();
  {
    const std::vector<unsigned char> pngBytes = withValidPngCrcs({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01,
        0x08, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x10, 0x49, 0x44, 0x41,
        0x54, 0x78, 0x01, 0x01, 0x05, 0x00, 0xfa, 0xff,
        0x01, 0x0a, 0x5a, 0x14, 0x05, 0x01, 0x6d, 0x00,
        0x7f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x49, 0x45, 0x4e, 0x44, 0x00, 0x00, 0x00,
        0x00,
    });
    std::ofstream file(inPath, std::ios::binary);
    REQUIRE(file.good());
    file.write(reinterpret_cast<const char *>(pngBytes.data()), static_cast<std::streamsize>(pngBytes.size()));
    REQUIRE(file.good());
  }

  const std::string escapedPath = escapeStringLiteral(inPath);
  const std::string source = injectEscapedPath(R"(
import /std/image/*
import /std/collections/*

[effects(heap_alloc, io_out, file_write), return<int>]
main() {
  [i32 mut] width{0i32}
  [i32 mut] height{0i32}
  [vector<i32> mut] pixels{vector<i32>()}
  [Result<ImageError>] status{/std/image/png/read(width, height, pixels, "__PATH__"utf8)}
  if(Result.error(status),
     then() {
       print_line(Result.why(status))
       return(1i32)
     },
     else() { })
  print_line(width)
  print_line(height)
  print_line(vectorCount<i32>(pixels))
  print_line(pixels[0i32])
  print_line(pixels[1i32])
  print_line(pixels[2i32])
  print_line(pixels[3i32])
  print_line(pixels[4i32])
  print_line(pixels[5i32])
  return(plus(width, height))
}
)", escapedPath);
  const std::string srcPath = writeTemp("compile_native_image_read_gray_alpha_sub_png.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_image_read_gray_alpha_sub_png").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_image_read_gray_alpha_sub_png.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 3);
  CHECK(readFile(outPath) ==
        "2\n"
        "1\n"
        "6\n"
        "10\n"
        "10\n"
        "10\n"
        "30\n"
        "30\n"
        "30\n");
}

TEST_CASE("compiles and runs native png read for 1-bit grayscale inputs") {
  const std::string inPath = (testScratchPath("") / "primec_native_image_read_gray1.png").string();
  {
    const std::vector<unsigned char> pngBytes = withValidPngCrcs({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01,
        0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x44, 0x41,
        0x54, 0x78, 0x01, 0x01, 0x02, 0x00, 0xfd, 0xff,
        0x00, 0x40, 0x00, 0x42, 0x00, 0x41, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45,
        0x4e, 0x44, 0x00, 0x00, 0x00, 0x00,
    });
    std::ofstream file(inPath, std::ios::binary);
    REQUIRE(file.good());
    file.write(reinterpret_cast<const char *>(pngBytes.data()), static_cast<std::streamsize>(pngBytes.size()));
    REQUIRE(file.good());
  }

  const std::string escapedPath = escapeStringLiteral(inPath);
  const std::string source = injectEscapedPath(R"(
import /std/image/*
import /std/collections/*

[effects(heap_alloc, io_out, file_write), return<int>]
main() {
  [i32 mut] width{0i32}
  [i32 mut] height{0i32}
  [vector<i32> mut] pixels{vector<i32>()}
  [Result<ImageError>] status{/std/image/png/read(width, height, pixels, "__PATH__"utf8)}
  if(Result.error(status),
     then() {
       print_line(Result.why(status))
       return(1i32)
     },
     else() { })
  print_line(width)
  print_line(height)
  print_line(vectorCount<i32>(pixels))
  print_line(pixels[0i32])
  print_line(pixels[1i32])
  print_line(pixels[2i32])
  print_line(pixels[3i32])
  print_line(pixels[4i32])
  print_line(pixels[5i32])
  return(plus(width, height))
}
)", escapedPath);
  const std::string srcPath = writeTemp("compile_native_image_read_gray1_png.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_image_read_gray1_png").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_image_read_gray1_png.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 3);
  CHECK(readFile(outPath) ==
        "2\n"
        "1\n"
        "6\n"
        "0\n"
        "0\n"
        "0\n"
        "255\n"
        "255\n"
        "255\n");
}

TEST_CASE("compiles and runs native png read for 4-bit grayscale inputs") {
  const std::string inPath = (testScratchPath("") / "primec_native_image_read_gray4.png").string();
  {
    const std::vector<unsigned char> pngBytes = withValidPngCrcs({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01,
        0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x44, 0x41,
        0x54, 0x78, 0x01, 0x01, 0x02, 0x00, 0xfd, 0xff,
        0x00, 0x1e, 0x00, 0x20, 0x00, 0x1f, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45,
        0x4e, 0x44, 0x00, 0x00, 0x00, 0x00,
    });
    std::ofstream file(inPath, std::ios::binary);
    REQUIRE(file.good());
    file.write(reinterpret_cast<const char *>(pngBytes.data()), static_cast<std::streamsize>(pngBytes.size()));
    REQUIRE(file.good());
  }

  const std::string escapedPath = escapeStringLiteral(inPath);
  const std::string source = injectEscapedPath(R"(
import /std/image/*
import /std/collections/*

[effects(heap_alloc, io_out, file_write), return<int>]
main() {
  [i32 mut] width{0i32}
  [i32 mut] height{0i32}
  [vector<i32> mut] pixels{vector<i32>()}
  [Result<ImageError>] status{/std/image/png/read(width, height, pixels, "__PATH__"utf8)}
  if(Result.error(status),
     then() {
       print_line(Result.why(status))
       return(1i32)
     },
     else() { })
  print_line(width)
  print_line(height)
  print_line(vectorCount<i32>(pixels))
  print_line(pixels[0i32])
  print_line(pixels[1i32])
  print_line(pixels[2i32])
  print_line(pixels[3i32])
  print_line(pixels[4i32])
  print_line(pixels[5i32])
  return(plus(width, height))
}
)", escapedPath);
  const std::string srcPath = writeTemp("compile_native_image_read_gray4_png.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_image_read_gray4_png").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_image_read_gray4_png.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 3);
  CHECK(readFile(outPath) ==
        "2\n"
        "1\n"
        "6\n"
        "17\n"
        "17\n"
        "17\n"
        "238\n"
        "238\n"
        "238\n");
}

TEST_CASE("compiles and runs native png read for 16-bit grayscale inputs") {
  const std::string inPath =
      (testScratchPath("") / "primec_native_image_read_gray16_sub.png").string();
  {
    const std::vector<unsigned char> pngBytes = withValidPngCrcs({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01,
        0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x10, 0x49, 0x44, 0x41,
        0x54, 0x78, 0x01, 0x01, 0x05, 0x00, 0xfa, 0xff,
        0x01, 0x12, 0x34, 0x6e, 0x4b, 0x02, 0x15, 0x01,
        0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x49, 0x45, 0x4e, 0x44, 0x00, 0x00, 0x00,
        0x00,
    });
    std::ofstream file(inPath, std::ios::binary);
    REQUIRE(file.good());
    file.write(reinterpret_cast<const char *>(pngBytes.data()), static_cast<std::streamsize>(pngBytes.size()));
    REQUIRE(file.good());
  }

  const std::string escapedPath = escapeStringLiteral(inPath);
  const std::string source = injectEscapedPath(R"(
import /std/image/*
import /std/collections/*

[effects(heap_alloc, io_out, file_write), return<int>]
main() {
  [i32 mut] width{0i32}
  [i32 mut] height{0i32}
  [vector<i32> mut] pixels{vector<i32>()}
  [Result<ImageError>] status{/std/image/png/read(width, height, pixels, "__PATH__"utf8)}
  if(Result.error(status),
     then() {
       print_line(Result.why(status))
       return(1i32)
     },
     else() { })
  print_line(width)
  print_line(height)
  print_line(vectorCount<i32>(pixels))
  print_line(pixels[0i32])
  print_line(pixels[1i32])
  print_line(pixels[2i32])
  print_line(pixels[3i32])
  print_line(pixels[4i32])
  print_line(pixels[5i32])
  return(plus(width, height))
}
)", escapedPath);
  const std::string srcPath = writeTemp("compile_native_image_read_gray16_sub_png.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_image_read_gray16_sub_png").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_image_read_gray16_sub_png.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 3);
  CHECK(readFile(outPath) ==
        "2\n"
        "1\n"
        "6\n"
        "18\n"
        "18\n"
        "18\n"
        "127\n"
        "127\n"
        "127\n");
}

TEST_CASE("compiles and runs native png read for stored sub-filter rgb inputs") {
  const std::string inPath = (testScratchPath("") / "primec_native_image_read_sub.png").string();
  {
    const std::vector<unsigned char> pngBytes = withValidPngCrcs({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01,
        0x08, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x12, 0x49, 0x44, 0x41,
        0x54,
        0x78, 0x01, 0x01, 0x07, 0x00, 0xf8, 0xff, 0x01,
        0x0a, 0x14, 0x1e, 0x28, 0x32, 0x3c, 0x02, 0x3e,
        0x00, 0xd4, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0x00, 0x00,
        0x00, 0x00,
    });
    std::ofstream file(inPath, std::ios::binary);
    REQUIRE(file.good());
    file.write(reinterpret_cast<const char *>(pngBytes.data()), static_cast<std::streamsize>(pngBytes.size()));
    REQUIRE(file.good());
  }

  const std::string escapedPath = escapeStringLiteral(inPath);
  const std::string source = injectEscapedPath(R"(
import /std/image/*
import /std/collections/*

[effects(heap_alloc, io_out, file_write), return<int>]
main() {
  [i32 mut] width{0i32}
  [i32 mut] height{0i32}
  [vector<i32> mut] pixels{vector<i32>()}
  [Result<ImageError>] status{/std/image/png/read(width, height, pixels, "__PATH__"utf8)}
  if(Result.error(status),
     then() {
       print_line(Result.why(status))
       return(1i32)
     },
     else() { })
  print_line(width)
  print_line(height)
  print_line(vectorCount<i32>(pixels))
  print_line(pixels[0i32])
  print_line(pixels[1i32])
  print_line(pixels[2i32])
  print_line(pixels[3i32])
  print_line(pixels[4i32])
  print_line(pixels[5i32])
  return(plus(width, height))
}
)", escapedPath);
  const std::string srcPath = writeTemp("compile_native_image_read_sub_png.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_image_read_sub_png").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_image_read_sub_png.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 3);
  CHECK(readFile(outPath) ==
        "2\n"
        "1\n"
        "6\n"
        "10\n"
        "20\n"
        "30\n"
        "50\n"
        "70\n"
        "90\n");
}

TEST_CASE("compiles and runs native png read for stored up-filter rgb inputs") {
  const std::string inPath = (testScratchPath("") / "primec_native_image_read_up.png").string();
  {
    const std::vector<unsigned char> pngBytes = withValidPngCrcs({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02,
        0x08, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x13, 0x49, 0x44, 0x41,
        0x54,
        0x78, 0x01, 0x01, 0x08, 0x00, 0xf7, 0xff, 0x00,
        0x0a, 0x14, 0x1e, 0x02, 0x05, 0x07, 0x09, 0x01,
        0x8a, 0x00, 0x54, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0x00,
        0x00, 0x00, 0x00,
    });
    std::ofstream file(inPath, std::ios::binary);
    REQUIRE(file.good());
    file.write(reinterpret_cast<const char *>(pngBytes.data()), static_cast<std::streamsize>(pngBytes.size()));
    REQUIRE(file.good());
  }

  const std::string escapedPath = escapeStringLiteral(inPath);
  const std::string source = injectEscapedPath(R"(
import /std/image/*
import /std/collections/*

[effects(heap_alloc, io_out, file_write), return<int>]
main() {
  [i32 mut] width{0i32}
  [i32 mut] height{0i32}
  [vector<i32> mut] pixels{vector<i32>()}
  [Result<ImageError>] status{/std/image/png/read(width, height, pixels, "__PATH__"utf8)}
  if(Result.error(status),
     then() {
       print_line(Result.why(status))
       return(1i32)
     },
     else() { })
  print_line(width)
  print_line(height)
  print_line(vectorCount<i32>(pixels))
  print_line(pixels[0i32])
  print_line(pixels[1i32])
  print_line(pixels[2i32])
  print_line(pixels[3i32])
  print_line(pixels[4i32])
  print_line(pixels[5i32])
  return(plus(width, height))
}
)", escapedPath);
  const std::string srcPath = writeTemp("compile_native_image_read_up_png.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_image_read_up_png").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_image_read_up_png.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 3);
  CHECK(readFile(outPath) ==
        "1\n"
        "2\n"
        "6\n"
        "10\n"
        "20\n"
        "30\n"
        "15\n"
        "27\n"
        "39\n");
}

TEST_CASE("compiles and runs native png read for stored average-filter rgb inputs") {
  const std::string inPath =
      (testScratchPath("") / "primec_native_image_read_average.png").string();
  {
    const std::vector<unsigned char> pngBytes = withValidPngCrcs({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02,
        0x08, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x13, 0x49, 0x44, 0x41,
        0x54,
        0x78, 0x01, 0x01, 0x08, 0x00, 0xf7, 0xff, 0x00,
        0x0a, 0x14, 0x1e, 0x03, 0x0a, 0x11, 0x18, 0x01,
        0xc0, 0x00, 0x73, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0x00,
        0x00, 0x00, 0x00,
    });
    std::ofstream file(inPath, std::ios::binary);
    REQUIRE(file.good());
    file.write(reinterpret_cast<const char *>(pngBytes.data()), static_cast<std::streamsize>(pngBytes.size()));
    REQUIRE(file.good());
  }

  const std::string escapedPath = escapeStringLiteral(inPath);
  const std::string source = injectEscapedPath(R"(
import /std/image/*
import /std/collections/*

[effects(heap_alloc, io_out, file_write), return<int>]
main() {
  [i32 mut] width{0i32}
  [i32 mut] height{0i32}
  [vector<i32> mut] pixels{vector<i32>()}
  [Result<ImageError>] status{/std/image/png/read(width, height, pixels, "__PATH__"utf8)}
  if(Result.error(status),
     then() {
       print_line(Result.why(status))
       return(1i32)
     },
     else() { })
  print_line(width)
  print_line(height)
  print_line(vectorCount<i32>(pixels))
  print_line(pixels[0i32])
  print_line(pixels[1i32])
  print_line(pixels[2i32])
  print_line(pixels[3i32])
  print_line(pixels[4i32])
  print_line(pixels[5i32])
  return(plus(width, height))
}
)", escapedPath);
  const std::string srcPath = writeTemp("compile_native_image_read_average_png.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_image_read_average_png").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_image_read_average_png.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 3);
  CHECK(readFile(outPath) ==
        "1\n"
        "2\n"
        "6\n"
        "10\n"
        "20\n"
        "30\n"
        "15\n"
        "27\n"
        "39\n");
}

TEST_CASE("compiles and runs native png read for stored paeth-filter rgb inputs") {
  const std::string inPath = (testScratchPath("") / "primec_native_image_read_paeth.png").string();
  {
    const std::vector<unsigned char> pngBytes = withValidPngCrcs({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02,
        0x08, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x19, 0x49, 0x44, 0x41,
        0x54,
        0x78, 0x01, 0x01, 0x0e, 0x00, 0xf1, 0xff, 0x04,
        0x0a, 0x14, 0x1e, 0x28, 0x32, 0x3c, 0x04, 0x05,
        0x07, 0x09, 0x0a, 0x14, 0x1e, 0x09, 0x19, 0x01,
        0x2c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x49, 0x45, 0x4e, 0x44, 0x00, 0x00, 0x00,
        0x00,
    });
    std::ofstream file(inPath, std::ios::binary);
    REQUIRE(file.good());
    file.write(reinterpret_cast<const char *>(pngBytes.data()), static_cast<std::streamsize>(pngBytes.size()));
    REQUIRE(file.good());
  }

  const std::string escapedPath = escapeStringLiteral(inPath);
  const std::string source = injectEscapedPath(R"(
import /std/image/*
import /std/collections/*

[effects(heap_alloc, io_out, file_write), return<int>]
main() {
  [i32 mut] width{0i32}
  [i32 mut] height{0i32}
  [vector<i32> mut] pixels{vector<i32>()}
  [Result<ImageError>] status{/std/image/png/read(width, height, pixels, "__PATH__"utf8)}
  if(Result.error(status),
     then() {
       print_line(Result.why(status))
       return(1i32)
     },
     else() { })
  print_line(width)
  print_line(height)
  print_line(vectorCount<i32>(pixels))
  print_line(pixels[0i32])
  print_line(pixels[1i32])
  print_line(pixels[2i32])
  print_line(pixels[3i32])
  print_line(pixels[4i32])
  print_line(pixels[5i32])
  print_line(pixels[6i32])
  print_line(pixels[7i32])
  print_line(pixels[8i32])
  print_line(pixels[9i32])
  print_line(pixels[10i32])
  print_line(pixels[11i32])
  return(plus(width, height))
}
)", escapedPath);
  const std::string srcPath = writeTemp("compile_native_image_read_paeth_png.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_image_read_paeth_png").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_image_read_paeth_png.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 4);
  CHECK(readFile(outPath) ==
        "2\n"
        "2\n"
        "12\n"
        "10\n"
        "20\n"
        "30\n"
        "50\n"
        "70\n"
        "90\n"
        "15\n"
        "27\n"
        "39\n"
        "60\n"
        "90\n"
        "120\n");
}

TEST_CASE("compiles and runs native png read for stored paeth-filter rgba inputs") {
  const std::string inPath =
      (testScratchPath("") / "primec_native_image_read_paeth_rgba.png").string();
  {
    const std::vector<unsigned char> pngBytes = withValidPngCrcs({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02,
        0x08, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x1d, 0x49, 0x44, 0x41,
        0x54,
        0x78, 0x01, 0x01, 0x12, 0x00, 0xed, 0xff, 0x04,
        0x0a, 0x14, 0x1e, 0x28, 0x1e, 0x1e, 0x1e, 0x28,
        0x04, 0x05, 0x07, 0x09, 0x14, 0x05, 0x0f, 0x19,
        0x14, 0x0d, 0x9c, 0x01, 0x59, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e,
        0x44, 0x00, 0x00, 0x00, 0x00,
    });
    std::ofstream file(inPath, std::ios::binary);
    REQUIRE(file.good());
    file.write(reinterpret_cast<const char *>(pngBytes.data()), static_cast<std::streamsize>(pngBytes.size()));
    REQUIRE(file.good());
  }

  const std::string escapedPath = escapeStringLiteral(inPath);
  const std::string source = injectEscapedPath(R"(
import /std/image/*
import /std/collections/*

[effects(heap_alloc, io_out, file_write), return<int>]
main() {
  [i32 mut] width{0i32}
  [i32 mut] height{0i32}
  [vector<i32> mut] pixels{vector<i32>()}
  [Result<ImageError>] status{/std/image/png/read(width, height, pixels, "__PATH__"utf8)}
  if(Result.error(status),
     then() {
       print_line(Result.why(status))
       return(1i32)
     },
     else() { })
  print_line(width)
  print_line(height)
  print_line(vectorCount<i32>(pixels))
  print_line(pixels[0i32])
  print_line(pixels[1i32])
  print_line(pixels[2i32])
  print_line(pixels[3i32])
  print_line(pixels[4i32])
  print_line(pixels[5i32])
  print_line(pixels[6i32])
  print_line(pixels[7i32])
  print_line(pixels[8i32])
  print_line(pixels[9i32])
  print_line(pixels[10i32])
  print_line(pixels[11i32])
  return(plus(width, height))
}
)", escapedPath);
  const std::string srcPath = writeTemp("compile_native_image_read_paeth_rgba_png.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_image_read_paeth_rgba_png").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_image_read_paeth_rgba_png.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 4);
  CHECK(readFile(outPath) ==
        "2\n"
        "2\n"
        "12\n"
        "10\n"
        "20\n"
        "30\n"
        "40\n"
        "50\n"
        "60\n"
        "15\n"
        "27\n"
        "39\n"
        "45\n"
        "65\n"
        "85\n");
}

TEST_CASE("compiles and runs native png read for fixed-huffman backreference rgb inputs") {
  const std::string inPath = (testScratchPath("") / "primec_native_image_read_fixed.png").string();
  {
    const std::vector<unsigned char> pngBytes = withValidPngCrcs({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x01,
        0x08, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x44, 0x41,
        0x54,
        0x78, 0x01, 0x63, 0xe0, 0x12, 0x91, 0x83, 0x20,
        0x00, 0x03, 0x52, 0x00, 0xb5, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e,
        0x44,
        0x00, 0x00, 0x00, 0x00,
    });
    std::ofstream file(inPath, std::ios::binary);
    REQUIRE(file.good());
    file.write(reinterpret_cast<const char *>(pngBytes.data()), static_cast<std::streamsize>(pngBytes.size()));
    REQUIRE(file.good());
  }

  const std::string escapedPath = escapeStringLiteral(inPath);
  const std::string source = injectEscapedPath(R"(
import /std/image/*
import /std/collections/*

[effects(heap_alloc, io_out, file_write), return<int>]
main() {
  [i32 mut] width{0i32}
  [i32 mut] height{0i32}
  [vector<i32> mut] pixels{vector<i32>()}
  [Result<ImageError>] status{/std/image/png/read(width, height, pixels, "__PATH__"utf8)}
  if(Result.error(status),
     then() {
       print_line(Result.why(status))
       return(1i32)
     },
     else() { })
  print_line(width)
  print_line(height)
  print_line(vectorCount<i32>(pixels))
  print_line(pixels[0i32])
  print_line(pixels[1i32])
  print_line(pixels[2i32])
  print_line(pixels[3i32])
  print_line(pixels[4i32])
  print_line(pixels[5i32])
  print_line(pixels[6i32])
  print_line(pixels[7i32])
  print_line(pixels[8i32])
  return(plus(width, height))
}
)", escapedPath);
  const std::string srcPath = writeTemp("compile_native_image_read_fixed_png.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_image_read_fixed_png").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_image_read_fixed_png.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 4);
  CHECK(readFile(outPath) ==
        "3\n"
        "1\n"
        "9\n"
        "10\n"
        "20\n"
        "30\n"
        "10\n"
        "20\n"
        "30\n"
        "10\n"
        "20\n"
        "30\n");
}

TEST_CASE("compiles and runs native png read for dynamic-huffman literal rgb inputs") {
  const std::string inPath =
      (testScratchPath("") / "primec_native_image_read_dynamic_literal.png").string();
  {
    const std::vector<unsigned char> pngBytes = withValidPngCrcs({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01,
        0x08, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x1b, 0x49, 0x44, 0x41,
        0x54,
        0x78, 0x01, 0x05, 0xc0, 0xa1, 0x00, 0x00, 0x00,
        0x00, 0x03, 0xb0, 0xe7, 0xe7, 0xe7, 0xe7, 0xe7,
        0xe7, 0xfb, 0x73, 0x7d, 0xa0, 0x9c, 0xee, 0x02,
        0x37, 0x00, 0xd3, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0x00,
        0x00, 0x00, 0x00,
    });
    std::ofstream file(inPath, std::ios::binary);
    REQUIRE(file.good());
    file.write(reinterpret_cast<const char *>(pngBytes.data()), static_cast<std::streamsize>(pngBytes.size()));
    REQUIRE(file.good());
  }

  const std::string escapedPath = escapeStringLiteral(inPath);
  const std::string source = injectEscapedPath(R"(
import /std/image/*
import /std/collections/*

[effects(heap_alloc, io_out, file_write), return<int>]
main() {
  [i32 mut] width{0i32}
  [i32 mut] height{0i32}
  [vector<i32> mut] pixels{vector<i32>()}
  [Result<ImageError>] status{/std/image/png/read(width, height, pixels, "__PATH__"utf8)}
  if(Result.error(status),
     then() {
       print_line(Result.why(status))
       return(1i32)
     },
     else() { })
  print_line(width)
  print_line(height)
  print_line(vectorCount<i32>(pixels))
  print_line(pixels[0i32])
  print_line(pixels[1i32])
  print_line(pixels[2i32])
  print_line(pixels[3i32])
  print_line(pixels[4i32])
  print_line(pixels[5i32])
  return(plus(width, height))
}
)", escapedPath);
  const std::string srcPath = writeTemp("compile_native_image_read_dynamic_literal_png.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_image_read_dynamic_literal_png").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_image_read_dynamic_literal_png.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 3);
  CHECK(readFile(outPath) ==
        "2\n"
        "1\n"
        "6\n"
        "10\n"
        "20\n"
        "30\n"
        "40\n"
        "50\n"
        "60\n");
}

TEST_CASE("compiles and runs native png read for dynamic-huffman backreference rgb inputs") {
  const std::string inPath =
      (testScratchPath("") / "primec_native_image_read_dynamic_backref.png").string();
  {
    const std::vector<unsigned char> pngBytes = withValidPngCrcs({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x01,
        0x08, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x1b, 0x49, 0x44, 0x41,
        0x54,
        0x78, 0x01, 0x25, 0xc3, 0xa1, 0x00, 0x00, 0x00,
        0x00, 0xc3, 0x80, 0xff, 0xf1, 0xf9, 0xf9, 0xfe,
        0x98, 0x0f, 0xdf, 0x36, 0x38, 0x7d, 0x03, 0x03,
        0x52, 0x00, 0xb5, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0x00,
        0x00, 0x00, 0x00,
    });
    std::ofstream file(inPath, std::ios::binary);
    REQUIRE(file.good());
    file.write(reinterpret_cast<const char *>(pngBytes.data()), static_cast<std::streamsize>(pngBytes.size()));
    REQUIRE(file.good());
  }

  const std::string escapedPath = escapeStringLiteral(inPath);
  const std::string source = injectEscapedPath(R"(
import /std/image/*
import /std/collections/*

[effects(heap_alloc, io_out, file_write), return<int>]
main() {
  [i32 mut] width{0i32}
  [i32 mut] height{0i32}
  [vector<i32> mut] pixels{vector<i32>()}
  [Result<ImageError>] status{/std/image/png/read(width, height, pixels, "__PATH__"utf8)}
  if(Result.error(status),
     then() {
       print_line(Result.why(status))
       return(1i32)
     },
     else() { })
  print_line(width)
  print_line(height)
  print_line(vectorCount<i32>(pixels))
  print_line(pixels[0i32])
  print_line(pixels[1i32])
  print_line(pixels[2i32])
  print_line(pixels[3i32])
  print_line(pixels[4i32])
  print_line(pixels[5i32])
  print_line(pixels[6i32])
  print_line(pixels[7i32])
  print_line(pixels[8i32])
  return(plus(width, height))
}
)", escapedPath);
  const std::string srcPath = writeTemp("compile_native_image_read_dynamic_backref_png.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_image_read_dynamic_backref_png").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_image_read_dynamic_backref_png.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 4);
  CHECK(readFile(outPath) ==
        "3\n"
        "1\n"
        "9\n"
        "10\n"
        "20\n"
        "30\n"
        "10\n"
        "20\n"
        "30\n"
        "10\n"
        "20\n"
        "30\n");
}

TEST_CASE("compiles and rejects malformed native png inputs deterministically") {
  const std::string inPath =
      (testScratchPath("") / "primec_native_image_read_invalid.png").string();
  {
    const std::vector<unsigned char> pngBytes = {0x00, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a};
    std::ofstream file(inPath, std::ios::binary);
    REQUIRE(file.good());
    file.write(reinterpret_cast<const char *>(pngBytes.data()), static_cast<std::streamsize>(pngBytes.size()));
    REQUIRE(file.good());
  }

  const std::string escapedPath = escapeStringLiteral(inPath);
  const std::string source = injectEscapedPath(R"(
import /std/image/*
import /std/collections/*

[effects(heap_alloc, io_out, file_write), return<int>]
main() {
  [i32 mut] width{7i32}
  [i32 mut] height{9i32}
  [vector<i32> mut] pixels{vector<i32>(1i32, 2i32, 3i32)}
  [Result<ImageError>] status{/std/image/png/read(width, height, pixels, "__PATH__"utf8)}
  print_line(Result.why(status))
  print_line(width)
  print_line(height)
  print_line(vectorCount<i32>(pixels))
  return(0i32)
}
)", escapedPath);
  const std::string srcPath = writeTemp("compile_native_image_read_invalid_png.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_image_read_invalid_png").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_image_read_invalid_png.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 0);
  CHECK(readFile(outPath) ==
        "image_invalid_operation\n"
        "0\n"
        "0\n"
        "0\n");
}

TEST_CASE("compiles and runs native png read for sub-filter indexed-color inputs") {
  const std::string inPath =
      (testScratchPath("") / "primec_native_image_read_indexed_sub.png").string();
  {
    const std::vector<unsigned char> pngBytes = withValidPngCrcs({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01,
        0x08, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x09, 0x50, 0x4c, 0x54, 0x45,
        0x00, 0x00, 0x00, 0x0a, 0x14, 0x1e, 0x32, 0x46,
        0x5a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x0e, 0x49, 0x44, 0x41, 0x54, 0x78, 0x01, 0x01,
        0x03, 0x00, 0xfc, 0xff, 0x01, 0x01, 0x01, 0x00,
        0x09, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0x00,
        0x00, 0x00, 0x00,
    });
    std::ofstream file(inPath, std::ios::binary);
    REQUIRE(file.good());
    file.write(reinterpret_cast<const char *>(pngBytes.data()), static_cast<std::streamsize>(pngBytes.size()));
    REQUIRE(file.good());
  }

  const std::string escapedPath = escapeStringLiteral(inPath);
  const std::string source = injectEscapedPath(R"(
import /std/image/*
import /std/collections/*

[effects(heap_alloc, io_out, file_write), return<int>]
main() {
  [i32 mut] width{0i32}
  [i32 mut] height{0i32}
  [vector<i32> mut] pixels{vector<i32>()}
  [Result<ImageError>] status{/std/image/png/read(width, height, pixels, "__PATH__"utf8)}
  if(Result.error(status),
     then() {
       print_line(Result.why(status))
       return(1i32)
     },
     else() { })
  print_line(width)
  print_line(height)
  print_line(vectorCount<i32>(pixels))
  print_line(pixels[0i32])
  print_line(pixels[1i32])
  print_line(pixels[2i32])
  print_line(pixels[3i32])
  print_line(pixels[4i32])
  print_line(pixels[5i32])
  return(plus(width, height))
}
)", escapedPath);
  const std::string srcPath = writeTemp("compile_native_image_read_indexed_sub_png.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_image_read_indexed_sub_png").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_image_read_indexed_sub_png.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 3);
  CHECK(readFile(outPath) ==
        "2\n"
        "1\n"
        "6\n"
        "10\n"
        "20\n"
        "30\n"
        "50\n"
        "70\n"
        "90\n");
}

TEST_CASE("compiles and rejects indexed native png inputs with out-of-range palette indexes") {
  const std::string inPath =
      (testScratchPath("") / "primec_native_image_read_indexed_invalid_palette.png").string();
  {
    const std::vector<unsigned char> pngBytes = withValidPngCrcs({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
        0x08, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x03, 0x50, 0x4c, 0x54, 0x45,
        0x0a, 0x14, 0x1e, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x0d, 0x49, 0x44, 0x41, 0x54, 0x78,
        0x01, 0x01, 0x02, 0x00, 0xfd, 0xff, 0x00, 0x01,
        0x00, 0x03, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44,
        0x00, 0x00, 0x00, 0x00,
    });
    std::ofstream file(inPath, std::ios::binary);
    REQUIRE(file.good());
    file.write(reinterpret_cast<const char *>(pngBytes.data()), static_cast<std::streamsize>(pngBytes.size()));
    REQUIRE(file.good());
  }

  const std::string escapedPath = escapeStringLiteral(inPath);
  const std::string source = injectEscapedPath(R"(
import /std/image/*
import /std/collections/*

[effects(heap_alloc, io_out, file_write), return<int>]
main() {
  [i32 mut] width{7i32}
  [i32 mut] height{9i32}
  [vector<i32> mut] pixels{vector<i32>(1i32, 2i32, 3i32)}
  [Result<ImageError>] status{/std/image/png/read(width, height, pixels, "__PATH__"utf8)}
  print_line(Result.why(status))
  print_line(width)
  print_line(height)
  print_line(vectorCount<i32>(pixels))
  return(0i32)
}
)", escapedPath);
  const std::string srcPath = writeTemp("compile_native_image_read_indexed_invalid_palette_png.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_image_read_indexed_invalid_palette_png").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_image_read_indexed_invalid_palette_png.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 0);
  CHECK(readFile(outPath) ==
        "image_invalid_operation\n"
        "0\n"
        "0\n"
        "0\n");
}

TEST_CASE("compiles and runs native png read for 2-bit indexed-color inputs") {
  const std::string inPath =
      (testScratchPath("") / "primec_native_image_read_indexed2.png").string();
  {
    const std::vector<unsigned char> pngBytes = withValidPngCrcs({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x01,
        0x02, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x09, 0x50, 0x4c, 0x54, 0x45,
        0x00, 0x00, 0x00, 0x0a, 0x14, 0x1e, 0x28, 0x32,
        0x3c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x0d, 0x49, 0x44, 0x41, 0x54, 0x78, 0x01, 0x01,
        0x02, 0x00, 0xfd, 0xff, 0x00, 0x18, 0x00, 0x1a,
        0x00, 0x19, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0x00, 0x00,
        0x00, 0x00,
    });
    std::ofstream file(inPath, std::ios::binary);
    REQUIRE(file.good());
    file.write(reinterpret_cast<const char *>(pngBytes.data()), static_cast<std::streamsize>(pngBytes.size()));
    REQUIRE(file.good());
  }

  const std::string escapedPath = escapeStringLiteral(inPath);
  const std::string source = injectEscapedPath(R"(
import /std/image/*
import /std/collections/*

[effects(heap_alloc, io_out, file_write), return<int>]
main() {
  [i32 mut] width{0i32}
  [i32 mut] height{0i32}
  [vector<i32> mut] pixels{vector<i32>()}
  [Result<ImageError>] status{/std/image/png/read(width, height, pixels, "__PATH__"utf8)}
  if(Result.error(status),
     then() {
       print_line(Result.why(status))
       return(1i32)
     },
     else() { })
  print_line(width)
  print_line(height)
  print_line(vectorCount<i32>(pixels))
  print_line(pixels[0i32])
  print_line(pixels[1i32])
  print_line(pixels[2i32])
  print_line(pixels[3i32])
  print_line(pixels[4i32])
  print_line(pixels[5i32])
  print_line(pixels[6i32])
  print_line(pixels[7i32])
  print_line(pixels[8i32])
  return(plus(width, height))
}
)", escapedPath);
  const std::string srcPath = writeTemp("compile_native_image_read_indexed2_png.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_image_read_indexed2_png").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_image_read_indexed2_png.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 4);
  CHECK(readFile(outPath) ==
        "3\n"
        "1\n"
        "9\n"
        "0\n"
        "0\n"
        "0\n"
        "10\n"
        "20\n"
        "30\n"
        "40\n"
        "50\n"
        "60\n");
}

TEST_CASE("compiles and rejects 1-bit indexed native png inputs with out-of-range palette indexes") {
  const std::string inPath =
      (testScratchPath("") / "primec_native_image_read_indexed1_invalid_palette.png").string();
  {
    const std::vector<unsigned char> pngBytes = withValidPngCrcs({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
        0x01, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x03, 0x50, 0x4c, 0x54, 0x45,
        0x0a, 0x14, 0x1e, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x0d, 0x49, 0x44, 0x41, 0x54, 0x78,
        0x01, 0x01, 0x02, 0x00, 0xfd, 0xff, 0x00, 0x80,
        0x00, 0x82, 0x00, 0x81, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44,
        0x00, 0x00, 0x00, 0x00,
    });
    std::ofstream file(inPath, std::ios::binary);
    REQUIRE(file.good());
    file.write(reinterpret_cast<const char *>(pngBytes.data()), static_cast<std::streamsize>(pngBytes.size()));
    REQUIRE(file.good());
  }

  const std::string escapedPath = escapeStringLiteral(inPath);
  const std::string source = injectEscapedPath(R"(
import /std/image/*
import /std/collections/*

[effects(heap_alloc, io_out, file_write), return<int>]
main() {
  [i32 mut] width{7i32}
  [i32 mut] height{9i32}
  [vector<i32> mut] pixels{vector<i32>(1i32, 2i32, 3i32)}
  [Result<ImageError>] status{/std/image/png/read(width, height, pixels, "__PATH__"utf8)}
  print_line(Result.why(status))
  print_line(width)
  print_line(height)
  print_line(vectorCount<i32>(pixels))
  return(0i32)
}
)", escapedPath);
  const std::string srcPath = writeTemp("compile_native_image_read_indexed1_invalid_palette_png.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_image_read_indexed1_invalid_palette_png").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_image_read_indexed1_invalid_palette_png.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 0);
  CHECK(readFile(outPath) ==
        "image_invalid_operation\n"
        "0\n"
        "0\n"
        "0\n");
}

TEST_CASE("compiles and runs native png read for 16-bit rgb inputs") {
  const std::string inPath =
      (testScratchPath("") / "primec_native_image_read_rgb16_sub.png").string();
  {
    const std::vector<unsigned char> pngBytes = withValidPngCrcs({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01,
        0x10, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x18, 0x49, 0x44, 0x41, 0x54,
        0x78, 0x01, 0x01, 0x0d, 0x00, 0xf2, 0xff, 0x01,
        0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0x5c, 0x17,
        0x11, 0x11, 0x11, 0x11, 0x17, 0xfb, 0x03, 0x23,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x49, 0x45, 0x4e, 0x44, 0x00, 0x00, 0x00, 0x00,
    });
    std::ofstream file(inPath, std::ios::binary);
    REQUIRE(file.good());
    file.write(reinterpret_cast<const char *>(pngBytes.data()), static_cast<std::streamsize>(pngBytes.size()));
    REQUIRE(file.good());
  }

  const std::string escapedPath = escapeStringLiteral(inPath);
  const std::string source = injectEscapedPath(R"(
import /std/image/*
import /std/collections/*

[effects(heap_alloc, io_out, file_write), return<int>]
main() {
  [i32 mut] width{0i32}
  [i32 mut] height{0i32}
  [vector<i32> mut] pixels{vector<i32>()}
  [Result<ImageError>] status{/std/image/png/read(width, height, pixels, "__PATH__"utf8)}
  if(Result.error(status),
     then() {
       print_line(Result.why(status))
       return(1i32)
     },
     else() { })
  print_line(width)
  print_line(height)
  print_line(vectorCount<i32>(pixels))
  print_line(pixels[0i32])
  print_line(pixels[1i32])
  print_line(pixels[2i32])
  print_line(pixels[3i32])
  print_line(pixels[4i32])
  print_line(pixels[5i32])
  return(plus(width, height))
}
)", escapedPath);
  const std::string srcPath = writeTemp("compile_native_image_read_rgb16_sub_png.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_image_read_rgb16_sub_png").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_image_read_rgb16_sub_png.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 3);
  CHECK(readFile(outPath) ==
        "2\n"
        "1\n"
        "6\n"
        "18\n"
        "86\n"
        "154\n"
        "109\n"
        "103\n"
        "171\n");
}

TEST_CASE("compiles and runs native png read for 16-bit rgba inputs") {
  const std::string inPath =
      (testScratchPath("") / "primec_native_image_read_rgba16_sub.png").string();
  {
    const std::vector<unsigned char> pngBytes = withValidPngCrcs({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01,
        0x10, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x1c, 0x49, 0x44, 0x41, 0x54,
        0x78, 0x01, 0x01, 0x11, 0x00, 0xee, 0xff, 0x01,
        0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0,
        0x4a, 0xa7, 0xef, 0xef, 0xef, 0xef, 0x10, 0x00,
        0x47, 0x51, 0x08, 0xf7, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44,
        0x00, 0x00, 0x00, 0x00,
    });
    std::ofstream file(inPath, std::ios::binary);
    REQUIRE(file.good());
    file.write(reinterpret_cast<const char *>(pngBytes.data()), static_cast<std::streamsize>(pngBytes.size()));
    REQUIRE(file.good());
  }

  const std::string escapedPath = escapeStringLiteral(inPath);
  const std::string source = injectEscapedPath(R"(
import /std/image/*
import /std/collections/*

[effects(heap_alloc, io_out, file_write), return<int>]
main() {
  [i32 mut] width{0i32}
  [i32 mut] height{0i32}
  [vector<i32> mut] pixels{vector<i32>()}
  [Result<ImageError>] status{/std/image/png/read(width, height, pixels, "__PATH__"utf8)}
  if(Result.error(status),
     then() {
       print_line(Result.why(status))
       return(1i32)
     },
     else() { })
  print_line(width)
  print_line(height)
  print_line(vectorCount<i32>(pixels))
  print_line(pixels[0i32])
  print_line(pixels[1i32])
  print_line(pixels[2i32])
  print_line(pixels[3i32])
  print_line(pixels[4i32])
  print_line(pixels[5i32])
  return(plus(width, height))
}
)", escapedPath);
  const std::string srcPath = writeTemp("compile_native_image_read_rgba16_sub_png.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_image_read_rgba16_sub_png").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_image_read_rgba16_sub_png.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 3);
  CHECK(readFile(outPath) ==
        "2\n"
        "1\n"
        "6\n"
        "18\n"
        "86\n"
        "154\n"
        "92\n"
        "69\n"
        "137\n");
}

TEST_CASE("compiles and runs native png read for 16-bit grayscale-alpha inputs") {
  const std::string inPath =
      (testScratchPath("") / "primec_native_image_read_gray_alpha_16_sub.png").string();
  {
    const std::vector<unsigned char> pngBytes = withValidPngCrcs({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01,
        0x10, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x14, 0x49, 0x44, 0x41, 0x54,
        0x78, 0x01, 0x01, 0x09, 0x00, 0xf6, 0xff, 0x01,
        0x12, 0x34, 0x56, 0x78, 0x5c, 0x17, 0x44, 0x44,
        0x08, 0xeb, 0x02, 0x11, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44,
        0x00, 0x00, 0x00, 0x00,
    });
    std::ofstream file(inPath, std::ios::binary);
    REQUIRE(file.good());
    file.write(reinterpret_cast<const char *>(pngBytes.data()), static_cast<std::streamsize>(pngBytes.size()));
    REQUIRE(file.good());
  }

  const std::string escapedPath = escapeStringLiteral(inPath);
  const std::string source = injectEscapedPath(R"(
import /std/image/*
import /std/collections/*

[effects(heap_alloc, io_out, file_write), return<int>]
main() {
  [i32 mut] width{0i32}
  [i32 mut] height{0i32}
  [vector<i32> mut] pixels{vector<i32>()}
  [Result<ImageError>] status{/std/image/png/read(width, height, pixels, "__PATH__"utf8)}
  if(Result.error(status),
     then() {
       print_line(Result.why(status))
       return(1i32)
     },
     else() { })
  print_line(width)
  print_line(height)
  print_line(vectorCount<i32>(pixels))
  print_line(pixels[0i32])
  print_line(pixels[1i32])
  print_line(pixels[2i32])
  print_line(pixels[3i32])
  print_line(pixels[4i32])
  print_line(pixels[5i32])
  return(plus(width, height))
}
)", escapedPath);
  const std::string srcPath = writeTemp("compile_native_image_read_gray_alpha_16_sub_png.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_image_read_gray_alpha_16_sub_png").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_image_read_gray_alpha_16_sub_png.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 3);
  CHECK(readFile(outPath) ==
        "2\n"
        "1\n"
        "6\n"
        "18\n"
        "18\n"
        "18\n"
        "109\n"
        "109\n"
        "109\n");
}

TEST_CASE("compiles and runs native png read for Adam7 interlaced rgb inputs") {
  const std::string inPath =
      (testScratchPath("") / "primec_native_image_read_interlaced_rgb.png").string();
  {
    const std::vector<unsigned char> pngBytes = withValidPngCrcs({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x05,
        0x08, 0x02, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x61, 0x49, 0x44, 0x41,
        0x54, 0x78, 0x01, 0x01, 0x56, 0x00, 0xa9, 0xff,
        0x00, 0x00, 0x00, 0x00, 0x00, 0xa0, 0x34, 0x68,
        0x00, 0x1c, 0xc8, 0x2c, 0xbc, 0xfc, 0x94, 0x00,
        0x50, 0x1a, 0xb4, 0x00, 0x6c, 0xe2, 0xe0, 0x00,
        0x0e, 0x64, 0x16, 0x5e, 0x7e, 0xca, 0xae, 0x98,
        0x7e, 0x00, 0x28, 0x0d, 0x5a, 0x78, 0x27, 0x0e,
        0x00, 0x36, 0x71, 0x70, 0x86, 0x8b, 0x24, 0x00,
        0x44, 0xd5, 0x86, 0x94, 0xef, 0x3a, 0x00, 0x07,
        0x32, 0x0b, 0x2f, 0x3f, 0x65, 0x57, 0x4c, 0xbf,
        0x7f, 0x59, 0x19, 0xa7, 0x66, 0x73, 0x00, 0x15,
        0x96, 0x21, 0x3d, 0xa3, 0x7b, 0x65, 0xb0, 0xd5,
        0x8d, 0xbd, 0x2f, 0xb5, 0xca, 0x89, 0xcf, 0x85,
        0x1f, 0x37,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x49, 0x45, 0x4e, 0x44, 0x00, 0x00, 0x00, 0x00,
    });
    std::ofstream file(inPath, std::ios::binary);
    REQUIRE(file.good());
    file.write(reinterpret_cast<const char *>(pngBytes.data()), static_cast<std::streamsize>(pngBytes.size()));
    REQUIRE(file.good());
  }

  const std::string escapedPath = escapeStringLiteral(inPath);
  const std::string source = injectEscapedPath(R"(
import /std/image/*
import /std/collections/*

[effects(heap_alloc, io_out, file_write), return<int>]
main() {
  [i32 mut] width{0i32}
  [i32 mut] height{0i32}
  [vector<i32> mut] pixels{vector<i32>()}
  [Result<ImageError>] status{/std/image/png/read(width, height, pixels, "__PATH__"utf8)}
  if(Result.error(status),
     then() {
       print_line(Result.why(status))
       return(1i32)
     },
     else() { })
  print_line(width)
  print_line(height)
  print_line(vectorCount<i32>(pixels))
  print_line(pixels[0i32])
  print_line(pixels[1i32])
  print_line(pixels[2i32])
  print_line(pixels[12i32])
  print_line(pixels[13i32])
  print_line(pixels[14i32])
  print_line(pixels[60i32])
  print_line(pixels[61i32])
  print_line(pixels[62i32])
  print_line(pixels[6i32])
  print_line(pixels[7i32])
  print_line(pixels[8i32])
  print_line(pixels[9i32])
  print_line(pixels[10i32])
  print_line(pixels[11i32])
  print_line(pixels[30i32])
  print_line(pixels[31i32])
  print_line(pixels[32i32])
  print_line(pixels[3i32])
  print_line(pixels[4i32])
  print_line(pixels[5i32])
  print_line(pixels[15i32])
  print_line(pixels[16i32])
  print_line(pixels[17i32])
  print_line(pixels[54i32])
  print_line(pixels[55i32])
  print_line(pixels[56i32])
  print_line(pixels[72i32])
  print_line(pixels[73i32])
  print_line(pixels[74i32])
  return(plus(width, height))
}
)", escapedPath);
  const std::string srcPath = writeTemp("compile_native_image_read_interlaced_rgb_png.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_image_read_interlaced_rgb_png").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_image_read_interlaced_rgb_png.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 10);
  CHECK(readFile(outPath) ==
        "5\n"
        "5\n"
        "75\n"
        "0\n"
        "0\n"
        "0\n"
        "160\n"
        "52\n"
        "104\n"
        "28\n"
        "200\n"
        "44\n"
        "80\n"
        "26\n"
        "180\n"
        "120\n"
        "39\n"
        "14\n"
        "14\n"
        "100\n"
        "22\n"
        "40\n"
        "13\n"
        "90\n"
        "7\n"
        "50\n"
        "11\n"
        "141\n"
        "189\n"
        "47\n"
        "188\n"
        "252\n"
        "148\n");
}

TEST_CASE("compiles and runs native png read for Adam7 interlaced indexed-color inputs") {
  const std::string inPath =
      (testScratchPath("") / "primec_native_image_read_interlaced_indexed.png").string();
  {
    const std::vector<unsigned char> pngBytes = withValidPngCrcs({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x05,
        0x02, 0x03, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x0c, 0x50, 0x4c, 0x54,
        0x45, 0x00, 0x00, 0x00, 0x40, 0x20, 0x10, 0x80,
        0x60, 0x30, 0xff, 0xc8, 0x64, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x23, 0x49, 0x44, 0x41,
        0x54, 0x78, 0x01, 0x01, 0x18, 0x00, 0xe7, 0xff,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80,
        0x00, 0x80, 0x00, 0x88, 0x00, 0x70, 0x00, 0xd0,
        0x00, 0x70, 0x00, 0x6c, 0x40, 0x00, 0xc6, 0xc0,
        0x2b, 0x98, 0x05, 0x6b, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44,
        0x00, 0x00, 0x00, 0x00,
    });
    std::ofstream file(inPath, std::ios::binary);
    REQUIRE(file.good());
    file.write(reinterpret_cast<const char *>(pngBytes.data()), static_cast<std::streamsize>(pngBytes.size()));
    REQUIRE(file.good());
  }

  const std::string escapedPath = escapeStringLiteral(inPath);
  const std::string source = injectEscapedPath(R"(
import /std/image/*
import /std/collections/*

[effects(heap_alloc, io_out, file_write), return<int>]
main() {
  [i32 mut] width{0i32}
  [i32 mut] height{0i32}
  [vector<i32> mut] pixels{vector<i32>()}
  [Result<ImageError>] status{/std/image/png/read(width, height, pixels, "__PATH__"utf8)}
  if(Result.error(status),
     then() {
       print_line(Result.why(status))
       return(1i32)
     },
     else() { })
  print_line(width)
  print_line(height)
  print_line(vectorCount<i32>(pixels))
  print_line(pixels[0i32])
  print_line(pixels[1i32])
  print_line(pixels[2i32])
  print_line(pixels[12i32])
  print_line(pixels[13i32])
  print_line(pixels[14i32])
  print_line(pixels[60i32])
  print_line(pixels[61i32])
  print_line(pixels[62i32])
  print_line(pixels[6i32])
  print_line(pixels[7i32])
  print_line(pixels[8i32])
  print_line(pixels[9i32])
  print_line(pixels[10i32])
  print_line(pixels[11i32])
  print_line(pixels[30i32])
  print_line(pixels[31i32])
  print_line(pixels[32i32])
  print_line(pixels[3i32])
  print_line(pixels[4i32])
  print_line(pixels[5i32])
  print_line(pixels[15i32])
  print_line(pixels[16i32])
  print_line(pixels[17i32])
  print_line(pixels[54i32])
  print_line(pixels[55i32])
  print_line(pixels[56i32])
  print_line(pixels[72i32])
  print_line(pixels[73i32])
  print_line(pixels[74i32])
  return(plus(width, height))
}
)", escapedPath);
  const std::string srcPath = writeTemp("compile_native_image_read_interlaced_indexed_png.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_image_read_interlaced_indexed_png").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_image_read_interlaced_indexed_png.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 10);
  CHECK(readFile(outPath) ==
        "5\n"
        "5\n"
        "75\n"
        "0\n"
        "0\n"
        "0\n"
        "0\n"
        "0\n"
        "0\n"
        "0\n"
        "0\n"
        "0\n"
        "128\n"
        "96\n"
        "48\n"
        "255\n"
        "200\n"
        "100\n"
        "128\n"
        "96\n"
        "48\n"
        "64\n"
        "32\n"
        "16\n"
        "64\n"
        "32\n"
        "16\n"
        "128\n"
        "96\n"
        "48\n"
        "0\n"
        "0\n"
        "0\n");
}

TEST_CASE("compiles and rejects malformed Adam7 interlaced native png inputs") {
  const std::string inPath =
      (testScratchPath("") / "primec_native_image_read_interlaced_invalid.png").string();
  {
    const std::vector<unsigned char> pngBytes = withValidPngCrcs({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x05,
        0x08, 0x02, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x60, 0x49, 0x44, 0x41,
        0x54, 0x78, 0x01, 0x01, 0x55, 0x00, 0xaa, 0xff,
        0x00, 0x00, 0x00, 0x00, 0x00, 0xa0, 0x34, 0x68,
        0x00, 0x1c, 0xc8, 0x2c, 0xbc, 0xfc, 0x94, 0x00,
        0x50, 0x1a, 0xb4, 0x00, 0x6c, 0xe2, 0xe0, 0x00,
        0x0e, 0x64, 0x16, 0x5e, 0x7e, 0xca, 0xae, 0x98,
        0x7e, 0x00, 0x28, 0x0d, 0x5a, 0x78, 0x27, 0x0e,
        0x00, 0x36, 0x71, 0x70, 0x86, 0x8b, 0x24, 0x00,
        0x44, 0xd5, 0x86, 0x94, 0xef, 0x3a, 0x00, 0x07,
        0x32, 0x0b, 0x2f, 0x3f, 0x65, 0x57, 0x4c, 0xbf,
        0x7f, 0x59, 0x19, 0xa7, 0x66, 0x73, 0x00, 0x15,
        0x96, 0x21, 0x3d, 0xa3, 0x7b, 0x65, 0xb0, 0xd5,
        0x8d, 0xbd, 0x2f, 0xb5, 0xca, 0xb0, 0x4e, 0x1e,
        0xae, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x49, 0x45, 0x4e, 0x44, 0x00, 0x00, 0x00,
        0x00,
    });
    std::ofstream file(inPath, std::ios::binary);
    REQUIRE(file.good());
    file.write(reinterpret_cast<const char *>(pngBytes.data()), static_cast<std::streamsize>(pngBytes.size()));
    REQUIRE(file.good());
  }

  const std::string escapedPath = escapeStringLiteral(inPath);
  const std::string source = injectEscapedPath(R"(
import /std/image/*
import /std/collections/*

[effects(heap_alloc, io_out, file_write), return<int>]
main() {
  [i32 mut] width{7i32}
  [i32 mut] height{9i32}
  [vector<i32> mut] pixels{vector<i32>(1i32, 2i32, 3i32)}
  [Result<ImageError>] status{/std/image/png/read(width, height, pixels, "__PATH__"utf8)}
  print_line(Result.why(status))
  print_line(width)
  print_line(height)
  print_line(vectorCount<i32>(pixels))
  return(0i32)
}
)", escapedPath);
  const std::string srcPath = writeTemp("compile_native_image_read_interlaced_invalid_png.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_image_read_interlaced_invalid_png").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_image_read_interlaced_invalid_png.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 0);
  CHECK(readFile(outPath) ==
        "image_invalid_operation\n"
        "0\n"
        "0\n"
        "0\n");
}

TEST_CASE("compiles and runs native png read for optional plte and split idat inputs") {
  const std::string inPath =
      (testScratchPath("") / "primec_native_image_read_plte_split_idat.png").string();
  {
    const std::vector<unsigned char> pngBytes = withValidPngCrcs({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
        0x08, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x03, 0x50, 0x4c, 0x54, 0x45,
        0xaa, 0xbb, 0xcc, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x08, 0x49, 0x44, 0x41, 0x54,
        0x78, 0x01, 0x01, 0x04, 0x00, 0xfb, 0xff, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x07, 0x49, 0x44, 0x41, 0x54,
        0xff, 0x00, 0x00, 0x03, 0x01, 0x01, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44,
        0x00, 0x00, 0x00, 0x00,
    });
    std::ofstream file(inPath, std::ios::binary);
    REQUIRE(file.good());
    file.write(reinterpret_cast<const char *>(pngBytes.data()), static_cast<std::streamsize>(pngBytes.size()));
    REQUIRE(file.good());
  }

  const std::string escapedPath = escapeStringLiteral(inPath);
  const std::string source = injectEscapedPath(R"(
import /std/image/*
import /std/collections/*

[effects(heap_alloc, io_out, file_write), return<int>]
main() {
  [i32 mut] width{0i32}
  [i32 mut] height{0i32}
  [vector<i32> mut] pixels{vector<i32>()}
  [Result<ImageError>] status{/std/image/png/read(width, height, pixels, "__PATH__"utf8)}
  if(Result.error(status),
     then() {
       print_line(Result.why(status))
       return(1i32)
     },
     else() { })
  print_line(width)
  print_line(height)
  print_line(vectorCount<i32>(pixels))
  print_line(pixels[0i32])
  print_line(pixels[1i32])
  print_line(pixels[2i32])
  return(plus(width, height))
}
)", escapedPath);
  const std::string srcPath = writeTemp("compile_native_image_read_plte_split_idat_png.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_image_read_plte_split_idat_png").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_image_read_plte_split_idat_png.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 2);
  CHECK(readFile(outPath) ==
        "1\n"
        "1\n"
        "3\n"
        "255\n"
        "0\n"
        "0\n");
}

TEST_CASE("compiles and rejects native png inputs with critical chunk crc mismatches") {
  const std::string inPath =
      (testScratchPath("") / "primec_native_image_read_invalid_crc.png").string();
  {
    const std::vector<unsigned char> pngBytes = withCorruptedFirstPngChunkCrc({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
        0x08, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x0c, 0x49, 0x44, 0x41,
        0x54, 0x78, 0x01, 0x01, 0x04, 0x00, 0xfb, 0xff,
        0x00, 0x11, 0x22, 0x33, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44,
        0x00, 0x00, 0x00, 0x00,
    });
    std::ofstream file(inPath, std::ios::binary);
    REQUIRE(file.good());
    file.write(reinterpret_cast<const char *>(pngBytes.data()), static_cast<std::streamsize>(pngBytes.size()));
    REQUIRE(file.good());
  }

  const std::string escapedPath = escapeStringLiteral(inPath);
  const std::string source = injectEscapedPath(R"(
import /std/image/*
import /std/collections/*

[effects(heap_alloc, io_out, file_write), return<int>]
main() {
  [i32 mut] width{7i32}
  [i32 mut] height{9i32}
  [vector<i32> mut] pixels{vector<i32>(1i32, 2i32, 3i32)}
  [Result<ImageError>] status{/std/image/png/read(width, height, pixels, "__PATH__"utf8)}
  print_line(Result.why(status))
  print_line(width)
  print_line(height)
  print_line(vectorCount<i32>(pixels))
  return(0i32)
}
)", escapedPath);
  const std::string srcPath = writeTemp("compile_native_image_read_invalid_crc_png.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_image_read_invalid_crc_png").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_image_read_invalid_crc_png.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 0);
  CHECK(readFile(outPath) ==
        "image_invalid_operation\n"
        "0\n"
        "0\n"
        "0\n");
}

TEST_CASE("compiles and rejects native png inputs with plte after idat") {
  const std::string inPath =
      (testScratchPath("") / "primec_native_image_read_invalid_plte_order.png").string();
  {
    const std::vector<unsigned char> pngBytes = withValidPngCrcs({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
        0x08, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x08, 0x49, 0x44, 0x41, 0x54,
        0x78, 0x01, 0x01, 0x04, 0x00, 0xfb, 0xff, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x03, 0x50, 0x4c, 0x54, 0x45,
        0xaa, 0xbb, 0xcc, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x07, 0x49, 0x44, 0x41, 0x54,
        0xff, 0x00, 0x00, 0x03, 0x01, 0x01, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44,
        0x00, 0x00, 0x00, 0x00,
    });
    std::ofstream file(inPath, std::ios::binary);
    REQUIRE(file.good());
    file.write(reinterpret_cast<const char *>(pngBytes.data()), static_cast<std::streamsize>(pngBytes.size()));
    REQUIRE(file.good());
  }

  const std::string escapedPath = escapeStringLiteral(inPath);
  const std::string source = injectEscapedPath(R"(
import /std/image/*
import /std/collections/*

[effects(heap_alloc, io_out, file_write), return<int>]
main() {
  [i32 mut] width{7i32}
  [i32 mut] height{9i32}
  [vector<i32> mut] pixels{vector<i32>(1i32, 2i32, 3i32)}
  [Result<ImageError>] status{/std/image/png/read(width, height, pixels, "__PATH__"utf8)}
  print_line(Result.why(status))
  print_line(width)
  print_line(height)
  print_line(vectorCount<i32>(pixels))
  return(0i32)
}
)", escapedPath);
  const std::string srcPath = writeTemp("compile_native_image_read_invalid_plte_order_png.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_image_read_invalid_plte_order_png").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_image_read_invalid_plte_order_png.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 0);
  CHECK(readFile(outPath) ==
        "image_invalid_operation\n"
        "0\n"
        "0\n"
        "0\n");
}

TEST_CASE("compiles and runs if expression in native backend") {
  const std::string source = R"(
[return<int>]
main() {
  return(if(false, then(){ 4i32 }, else(){ 9i32 }))
}
)";
  const std::string srcPath = writeTemp("compile_native_if_expr.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_if_expr_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("compiles and runs match cases in native backend") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{2i32}
  return(match(value, case(1i32) { 10i32 }, case(2i32) { 20i32 }, else() { 30i32 }))
}
)";
  const std::string srcPath = writeTemp("compile_native_match_cases.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_match_cases_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 20);
}

TEST_CASE("compiles and runs native definition call") {
  const std::string source = R"(
[return<int>]
inc([i32] x) {
  return(plus(x, 1i32))
}

[return<int>]
main() {
  return(inc(6i32))
}
)";
  const std::string srcPath = writeTemp("compile_native_def_call.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_def_call_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs native method call") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  inc([i32] value) {
    return(plus(value, 1i32))
  }
}

[return<int>]
main() {
  [i32] value{5i32}
  return(value.inc())
}
)";
  const std::string srcPath = writeTemp("compile_native_method_call.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_method_call_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 6);
}

TEST_CASE("compiles and runs native method count call") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  count([i32] value) {
    return(plus(value, 2i32))
  }
}

[return<int>]
main() {
  [i32] value{3i32}
  return(value.count())
}
)";
  const std::string srcPath = writeTemp("compile_native_method_count.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_method_count_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 5);
}

TEST_CASE("compiles and runs native literal method call") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  inc([i32] value) {
    return(plus(value, 1i32))
  }
}

[return<int>]
main() {
  return(1i32.inc())
}
)";
  const std::string srcPath = writeTemp("compile_native_method_literal.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_method_literal_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("compiles and runs native chained method calls") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  inc([i32] value) {
    return(plus(value, 1i32))
  }

  [return<int>]
  dec([i32] value) {
    return(minus(value, 1i32))
  }
}

[return<int>]
make() {
  return(4i32)
}

[return<int>]
main() {
  return(make().inc().dec())
}
)";
  const std::string srcPath = writeTemp("compile_native_method_chain.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_method_chain_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 4);
}

TEST_CASE("compiles and runs native Result.why hooks") {
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
  const std::string srcPath = writeTemp("compile_native_result_why_custom.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_result_why_custom_exe").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_result_why_custom_out.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "custom error\n");
}

TEST_CASE("native supports stdlib FileError result helpers") {
  const std::string source = R"(
import /std/file/*

[return<Result<FileError>>]
make_status() {
  return(/FileError/status(fileReadEof()))
}

[return<Result<i32, FileError>>]
make_value() {
  return(/FileError/result<i32>(fileReadEof()))
}

[return<int> effects(io_out)]
main() {
  [Result<FileError>] status{make_status()}
  [Result<i32, FileError>] valueStatus{make_value()}
  [FileError] err{fileReadEof()}
  [Result<FileError>] methodStatus{/FileError/status(err)}
  [Result<i32, FileError>] methodValueStatus{/FileError/result<i32>(err)}
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
  print_line(Result.why(/FileError/status(fileReadEof())))
  print_line(Result.why(/FileError/result<i32>(fileReadEof())))
  print_line(Result.why(methodStatus))
  print_line(Result.why(methodValueStatus))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_file_error_result_helpers.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_stdlib_file_error_result_helpers").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_stdlib_file_error_result_helpers_out.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "EOF\nEOF\nEOF\nEOF\nEOF\nEOF\nEOF\nEOF\n");
}

TEST_CASE("native backend supports Result.map on IR-backed path") {
  const std::string source = R"(
import /std/file/*

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(err.why())
}

[return<int> effects(io_out, io_err) on_error<FileError, /log_file_error>]
main() {
  [Result<i32, FileError>] ok{Result.ok(2i32)}
  [Result<i32, FileError>] failed{/FileError/result<i32>(fileReadEof())}
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
  const std::string srcPath = writeTemp("compile_native_result_map_ir_backed.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_result_map_ir_backed").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_result_map_ir_backed_out.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " > " + outPath;
  CHECK(runCommand(runCmd) == 8);
  CHECK(readFile(outPath) == "EOF\n");
}

TEST_CASE("native backend supports Result.and_then on IR-backed path") {
  const std::string source = R"(
import /std/file/*

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(err.why())
}

[return<int> effects(io_out, io_err) on_error<FileError, /log_file_error>]
main() {
  [Result<i32, FileError>] ok{Result.ok(2i32)}
  [Result<i32, FileError>] failed{/FileError/result<i32>(fileReadEof())}
  [Result<i32, FileError>] chainedOk{
    Result.and_then(ok, []([i32] value) { return(Result.ok(multiply(value, 4i32))) })
  }
  [Result<FileError>] chainedStatus{
    Result.and_then(ok, []([i32] value) { return(/FileError/status(/FileError/eof())) })
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
  const std::string srcPath = writeTemp("compile_native_result_and_then_ir_backed.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_result_and_then_ir_backed").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_result_and_then_ir_backed_out.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " > " + outPath;
  CHECK(runCommand(runCmd) == 8);
  CHECK(readFile(outPath) == "EOF\nEOF\n");
}

TEST_CASE("native backend supports Result.map2 on IR-backed path") {
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
  const std::string srcPath = writeTemp("compile_native_result_map2_ir_backed.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_result_map2_ir_backed").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_result_map2_ir_backed_out.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " > " + outPath;
  CHECK(runCommand(runCmd) == 5);
  CHECK(readFile(outPath) == "container missing key\ncontainer empty\n");
}

TEST_CASE("native backend supports f32 Result payloads on IR-backed paths") {
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
  const std::string srcPath = writeTemp("compile_native_result_f32_ir_backed.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_result_f32_ir_backed").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_result_f32_ir_backed_out.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " > " + outPath;
  CHECK(runCommand(runCmd) == 55);
  CHECK(readFile(outPath) == "55\n");
}

TEST_CASE("native backend supports direct Result.ok combinator sources on IR-backed paths") {
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
  const std::string srcPath = writeTemp("compile_native_result_direct_ok_ir_backed.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_result_direct_ok_ir_backed").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_result_direct_ok_ir_backed_out.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " > " + outPath;
  CHECK(runCommand(runCmd) == 5);
  CHECK(readFile(outPath) == "8\n5\n");
}

TEST_CASE("native backend rejects auto-bound direct Result combinator try consumers") {
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
  const std::string errPath =
      (testScratchPath("") / "primec_native_result_auto_bound_combinators_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("try requires Result argument") != std::string::npos);
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
  [GfxError] err{GfxError(8i32)}
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

TEST_CASE("native backend supports graphics-style int return propagation with on_error") {
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
  const std::string srcPath = writeTemp("native_graphics_int_on_error.prime", source);
  const std::string okExePath =
      (testScratchPath("") / "primec_native_graphics_int_on_error_ok").string();
  const std::string failExePath =
      (testScratchPath("") / "primec_native_graphics_int_on_error_fail").string();
  const std::string errPath =
      (testScratchPath("") / "primec_native_graphics_int_on_error_err.txt").string();

  const std::string compileOkCmd =
      "./primec --emit=native " + srcPath + " -o " + okExePath + " --entry /main_ok";
  const std::string compileFailCmd =
      "./primec --emit=native " + srcPath + " -o " + failExePath + " --entry /main_fail";
  CHECK(runCommand(compileOkCmd) == 0);
  CHECK(runCommand(compileFailCmd) == 0);
  CHECK(runCommand(okExePath) == 9);
  CHECK(runCommand(failExePath + " 2> " + errPath) == 7);
  CHECK(readFile(errPath).empty());
}

TEST_CASE("native backend supports string Result.ok payloads through try") {
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
  const std::string srcPath = writeTemp("native_result_ok_string.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_result_ok_string_exe").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_result_ok_string_out.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " > " + outPath;
  CHECK(runCommand(runCmd) == 5);
  CHECK(readFile(outPath) == "alpha\n");
}

TEST_CASE("native backend supports direct string Result combinator consumers") {
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
  const std::string srcPath = writeTemp("native_result_combinators_string.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_result_combinators_string_exe").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_result_combinators_string_out.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "alpha\nbeta\ngamma\n");
}

TEST_CASE("native backend supports definition-backed string Result combinator sources") {
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
  [Reader] reader{Reader()}
  print_line(try(Result.map(greeting(), []([string] value) { return(value) })))
  print_line(try(Result.and_then(reader.read(), []([string] value) { return(Result.ok(value)) })))
  print_line(try(Result.map2(greeting(), reader.read(), []([string] left, [string] right) { return(left) })))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("native_result_combinators_string_definition_sources.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_result_combinators_string_definition_sources_exe")
          .string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_result_combinators_string_definition_sources_out.txt")
          .string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "alpha\nbeta\nalpha\n");
}

#if defined(EACCES)
TEST_CASE("compiles and runs native FileError.why mapping") {
  const std::string source =
      "[return<Result<FileError>>]\n"
      "make_error() {\n"
      "  return(" + std::to_string(EACCES) + "i32)\n"
      "}\n"
      "\n"
      "[return<int> effects(io_out)]\n"
      "main() {\n"
      "  print_line(Result.why(make_error()))\n"
      "  return(0i32)\n"
      "}\n";
  const std::string srcPath = writeTemp("compile_native_file_error_why.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_file_error_why_exe").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_file_error_why_out.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "EACCES\n");
}
#endif

TEST_CASE("native uses stdlib FileError why wrapper") {
  const std::string source = R"(
import /std/file/*

[return<int> effects(io_out)]
main() {
  [FileError] err{fileReadEof()}
  if(not(fileErrorIsEof(err))) {
    return(1i32)
  }
  print_line(/FileError/why(err))
  print_line(FileError.why(err))
  print_line(err.why())
  print_line(Result.why(fileErrorStatus(err)))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_file_error_why.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_stdlib_file_error_why_exe").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_stdlib_file_error_why_out.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "EOF\nEOF\nEOF\nEOF\n");
}

TEST_CASE("native uses stdlib FileError eof wrapper") {
  const std::string source = R"(
import /std/file/*

[return<int>]
main() {
  [FileError] eofErr{fileReadEof()}
  [FileError] otherErr{1i32}
  if(not(/FileError/is_eof(eofErr))) {
    return(1i32)
  }
  if(not(FileError.is_eof(eofErr))) {
    return(2i32)
  }
  if(not(/FileError/is_eof(eofErr))) {
    return(3i32)
  }
  if(/FileError/is_eof(otherErr)) {
    return(4i32)
  }
  if(FileError.is_eof(otherErr)) {
    return(5i32)
  }
  if(/FileError/is_eof(otherErr)) {
    return(6i32)
  }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_file_error_is_eof.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_stdlib_file_error_is_eof_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("native uses stdlib FileError eof constructor wrapper") {
  const std::string source = R"(
import /std/file/*

[return<int> effects(io_out)]
main() {
  [FileError] err{FileError.eof()}
  print_line(/FileError/why(err))
  print_line(FileError.why(err))
  if(not(FileError.is_eof(err))) {
    return(1i32)
  }
  if(not(/FileError/is_eof(err))) {
    return(2i32)
  }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_file_error_eof_wrapper.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_stdlib_file_error_eof_wrapper_exe").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_stdlib_file_error_eof_wrapper_out.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "EOF\nEOF\n");
}

#if defined(EACCES) && defined(ENOENT) && defined(EEXIST)
TEST_CASE("native materializes variadic FileError packs with indexed why methods") {
  const std::string source =
      "[return<int>]\n"
      "score_errors([args<FileError>] values) {\n"
      "  return(plus(count(values[0i32].why()), count(values[2i32].why())))\n"
      "}\n"
      "\n"
      "[return<int>]\n"
      "forward([args<FileError>] values) {\n"
      "  return(score_errors([spread] values))\n"
      "}\n"
      "\n"
      "[return<int>]\n"
      "forward_mixed([args<FileError>] values) {\n"
      "  [FileError] extra{" + std::to_string(EACCES) + "i32}\n"
      "  return(score_errors(extra, [spread] values))\n"
      "}\n"
      "\n"
      "[return<int>]\n"
      "main() {\n"
      "  [FileError] a0{" + std::to_string(EACCES) + "i32}\n"
      "  [FileError] a1{" + std::to_string(ENOENT) + "i32}\n"
      "  [FileError] a2{" + std::to_string(EEXIST) + "i32}\n"
      "  [FileError] b0{" + std::to_string(ENOENT) + "i32}\n"
      "  [FileError] b1{" + std::to_string(EEXIST) + "i32}\n"
      "  [FileError] b2{" + std::to_string(EACCES) + "i32}\n"
      "  [FileError] c0{" + std::to_string(ENOENT) + "i32}\n"
      "  [FileError] c1{" + std::to_string(EEXIST) + "i32}\n"
      "  return(plus(score_errors(a0, a1, a2),\n"
      "              plus(forward(b0, b1, b2),\n"
      "                   forward_mixed(c0, c1))))\n"
      "}\n";
  const std::string srcPath = writeTemp("compile_native_variadic_args_file_error.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_file_error").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 36);
}

TEST_CASE("native materializes variadic borrowed FileError packs with indexed dereference why methods") {
  const std::string source =
      "[return<int>]\n"
      "score_refs([args<Reference<FileError>>] values) {\n"
      "  return(plus(count(dereference(values[0i32]).why()), count(dereference(values[2i32]).why())))\n"
      "}\n"
      "\n"
      "[return<int>]\n"
      "forward([args<Reference<FileError>>] values) {\n"
      "  return(score_refs([spread] values))\n"
      "}\n"
      "\n"
      "[return<int>]\n"
      "forward_mixed([args<Reference<FileError>>] values) {\n"
      "  [FileError] extra{" + std::to_string(EACCES) + "i32}\n"
      "  [Reference<FileError>] extra_ref{location(extra)}\n"
      "  return(score_refs(extra_ref, [spread] values))\n"
      "}\n"
      "\n"
      "[return<int>]\n"
      "main() {\n"
      "  [FileError] a0{" + std::to_string(EACCES) + "i32}\n"
      "  [FileError] a1{" + std::to_string(ENOENT) + "i32}\n"
      "  [FileError] a2{" + std::to_string(EEXIST) + "i32}\n"
      "  [Reference<FileError>] r0{location(a0)}\n"
      "  [Reference<FileError>] r1{location(a1)}\n"
      "  [Reference<FileError>] r2{location(a2)}\n"
      "  [FileError] b0{" + std::to_string(ENOENT) + "i32}\n"
      "  [FileError] b1{" + std::to_string(EEXIST) + "i32}\n"
      "  [FileError] b2{" + std::to_string(EACCES) + "i32}\n"
      "  [Reference<FileError>] s0{location(b0)}\n"
      "  [Reference<FileError>] s1{location(b1)}\n"
      "  [Reference<FileError>] s2{location(b2)}\n"
      "  [FileError] c0{" + std::to_string(ENOENT) + "i32}\n"
      "  [FileError] c1{" + std::to_string(EEXIST) + "i32}\n"
      "  [Reference<FileError>] t0{location(c0)}\n"
      "  [Reference<FileError>] t1{location(c1)}\n"
      "  return(plus(score_refs(r0, r1, r2),\n"
      "              plus(forward(s0, s1, s2),\n"
      "                   forward_mixed(t0, t1))))\n"
      "}\n";
  const std::string srcPath = writeTemp("compile_native_variadic_args_borrowed_file_error.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_borrowed_file_error").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 36);
}

TEST_CASE("native materializes variadic pointer FileError packs with indexed dereference why methods") {
  const std::string source =
      "[return<int>]\n"
      "score_ptrs([args<Pointer<FileError>>] values) {\n"
      "  return(plus(count(dereference(values[0i32]).why()), count(dereference(values[2i32]).why())))\n"
      "}\n"
      "\n"
      "[return<int>]\n"
      "forward([args<Pointer<FileError>>] values) {\n"
      "  return(score_ptrs([spread] values))\n"
      "}\n"
      "\n"
      "[return<int>]\n"
      "forward_mixed([args<Pointer<FileError>>] values) {\n"
      "  [FileError] extra{" + std::to_string(EACCES) + "i32}\n"
      "  [Pointer<FileError>] extra_ptr{location(extra)}\n"
      "  return(score_ptrs(extra_ptr, [spread] values))\n"
      "}\n"
      "\n"
      "[return<int>]\n"
      "main() {\n"
      "  [FileError] a0{" + std::to_string(EACCES) + "i32}\n"
      "  [FileError] a1{" + std::to_string(ENOENT) + "i32}\n"
      "  [FileError] a2{" + std::to_string(EEXIST) + "i32}\n"
      "  [Pointer<FileError>] r0{location(a0)}\n"
      "  [Pointer<FileError>] r1{location(a1)}\n"
      "  [Pointer<FileError>] r2{location(a2)}\n"
      "  [FileError] b0{" + std::to_string(ENOENT) + "i32}\n"
      "  [FileError] b1{" + std::to_string(EEXIST) + "i32}\n"
      "  [FileError] b2{" + std::to_string(EACCES) + "i32}\n"
      "  [Pointer<FileError>] s0{location(b0)}\n"
      "  [Pointer<FileError>] s1{location(b1)}\n"
      "  [Pointer<FileError>] s2{location(b2)}\n"
      "  [FileError] c0{" + std::to_string(ENOENT) + "i32}\n"
      "  [FileError] c1{" + std::to_string(EEXIST) + "i32}\n"
      "  [Pointer<FileError>] t0{location(c0)}\n"
      "  [Pointer<FileError>] t1{location(c1)}\n"
      "  return(plus(score_ptrs(r0, r1, r2),\n"
      "              plus(forward(s0, s1, s2),\n"
      "                   forward_mixed(t0, t1))))\n"
      "}\n";
  const std::string srcPath = writeTemp("compile_native_variadic_args_pointer_file_error.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_pointer_file_error").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 36);
}

TEST_CASE("native materializes variadic wrapped FileError packs with named free builtin at receivers") {
  const std::string source =
      "[return<int>]\n"
      "score_refs([args<Reference<FileError>>] values) {\n"
      "  return(plus(count(at([values] values, [index] 0i32).why()),\n"
      "              count(at([values] values, [index] minus(count(values), 1i32)).why())))\n"
      "}\n"
      "\n"
      "[return<int>]\n"
      "forward_refs([args<Reference<FileError>>] values) {\n"
      "  return(score_refs([spread] values))\n"
      "}\n"
      "\n"
      "[return<int>]\n"
      "forward_refs_mixed([args<Reference<FileError>>] values) {\n"
      "  [FileError] extra{" + std::to_string(EACCES) + "i32}\n"
      "  [Reference<FileError>] extra_ref{location(extra)}\n"
      "  return(score_refs(extra_ref, [spread] values))\n"
      "}\n"
      "\n"
      "[return<int>]\n"
      "score_ptrs([args<Pointer<FileError>>] values) {\n"
      "  return(plus(count(at([values] values, [index] 0i32).why()),\n"
      "              count(at([values] values, [index] minus(count(values), 1i32)).why())))\n"
      "}\n"
      "\n"
      "[return<int>]\n"
      "forward_ptrs([args<Pointer<FileError>>] values) {\n"
      "  return(score_ptrs([spread] values))\n"
      "}\n"
      "\n"
      "[return<int>]\n"
      "forward_ptrs_mixed([args<Pointer<FileError>>] values) {\n"
      "  [FileError] extra{" + std::to_string(EACCES) + "i32}\n"
      "  [Pointer<FileError>] extra_ptr{location(extra)}\n"
      "  return(score_ptrs(extra_ptr, [spread] values))\n"
      "}\n"
      "\n"
      "[return<int>]\n"
      "main() {\n"
      "  [FileError] a0{" + std::to_string(EACCES) + "i32}\n"
      "  [FileError] a1{" + std::to_string(ENOENT) + "i32}\n"
      "  [FileError] a2{" + std::to_string(EEXIST) + "i32}\n"
      "  [Reference<FileError>] r0{location(a0)}\n"
      "  [Reference<FileError>] r1{location(a1)}\n"
      "  [Reference<FileError>] r2{location(a2)}\n"
      "  [FileError] b0{" + std::to_string(ENOENT) + "i32}\n"
      "  [FileError] b1{" + std::to_string(EEXIST) + "i32}\n"
      "  [FileError] b2{" + std::to_string(EACCES) + "i32}\n"
      "  [Reference<FileError>] s0{location(b0)}\n"
      "  [Reference<FileError>] s1{location(b1)}\n"
      "  [Reference<FileError>] s2{location(b2)}\n"
      "  [FileError] c0{" + std::to_string(ENOENT) + "i32}\n"
      "  [FileError] c1{" + std::to_string(EEXIST) + "i32}\n"
      "  [Reference<FileError>] t0{location(c0)}\n"
      "  [Reference<FileError>] t1{location(c1)}\n"
      "  [FileError] d0{" + std::to_string(EACCES) + "i32}\n"
      "  [FileError] d1{" + std::to_string(ENOENT) + "i32}\n"
      "  [FileError] d2{" + std::to_string(EEXIST) + "i32}\n"
      "  [Pointer<FileError>] p0{location(d0)}\n"
      "  [Pointer<FileError>] p1{location(d1)}\n"
      "  [Pointer<FileError>] p2{location(d2)}\n"
      "  [FileError] e0{" + std::to_string(ENOENT) + "i32}\n"
      "  [FileError] e1{" + std::to_string(EEXIST) + "i32}\n"
      "  [FileError] e2{" + std::to_string(EACCES) + "i32}\n"
      "  [Pointer<FileError>] q0{location(e0)}\n"
      "  [Pointer<FileError>] q1{location(e1)}\n"
      "  [Pointer<FileError>] q2{location(e2)}\n"
      "  [FileError] f0{" + std::to_string(ENOENT) + "i32}\n"
      "  [FileError] f1{" + std::to_string(EEXIST) + "i32}\n"
      "  [Pointer<FileError>] u0{location(f0)}\n"
      "  [Pointer<FileError>] u1{location(f1)}\n"
      "  return(plus(score_refs(r0, r1, r2),\n"
      "              plus(forward_refs(s0, s1, s2),\n"
      "                   plus(forward_refs_mixed(t0, t1),\n"
      "                        plus(score_ptrs(p0, p1, p2),\n"
      "                             plus(forward_ptrs(q0, q1, q2),\n"
      "                                  forward_ptrs_mixed(u0, u1)))))))\n"
      "}\n";
  const std::string srcPath = writeTemp("compile_native_variadic_named_args_wrapped_file_error.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_variadic_named_args_wrapped_file_error").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 72);
}

TEST_CASE("native materializes variadic File handle packs with indexed file methods") {
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
  const std::string pathA0 = (testScratchPath("") / "primec_native_variadic_file_handle_a0.txt").string();
  const std::string pathA1 = (testScratchPath("") / "primec_native_variadic_file_handle_a1.txt").string();
  const std::string pathA2 = (testScratchPath("") / "primec_native_variadic_file_handle_a2.txt").string();
  const std::string pathB0 = (testScratchPath("") / "primec_native_variadic_file_handle_b0.txt").string();
  const std::string pathB1 = (testScratchPath("") / "primec_native_variadic_file_handle_b1.txt").string();
  const std::string pathB2 = (testScratchPath("") / "primec_native_variadic_file_handle_b2.txt").string();
  const std::string pathC0 = (testScratchPath("") / "primec_native_variadic_file_handle_c0.txt").string();
  const std::string pathC1 = (testScratchPath("") / "primec_native_variadic_file_handle_c1.txt").string();
  const std::string pathExtra =
      (testScratchPath("") / "primec_native_variadic_file_handle_extra.txt").string();

  const std::string source =
      "[effects(file_write)]\n"
      "swallow_file_error([FileError] err) {}\n"
      "\n"
      "[return<int> effects(file_write) on_error<FileError, /swallow_file_error>]\n"
      "score_files([args<File<Write>>] values) {\n"
      "  values[0i32].write_line(\"alpha\"utf8)?\n"
      "  values[minus(count(values), 1i32)].write_line(\"omega\"utf8)?\n"
      "  values[0i32].flush()?\n"
      "  values[minus(count(values), 1i32)].flush()?\n"
      "  values[0i32].close()?\n"
      "  values[minus(count(values), 1i32)].close()?\n"
      "  return(plus(count(values), 10i32))\n"
      "}\n"
      "\n"
      "[return<int> effects(file_write) on_error<FileError, /swallow_file_error>]\n"
      "forward([args<File<Write>>] values) {\n"
      "  return(score_files([spread] values))\n"
      "}\n"
      "\n"
      "[return<int> effects(file_write) on_error<FileError, /swallow_file_error>]\n"
      "forward_mixed([args<File<Write>>] values) {\n"
      "  [File<Write>] extra{File<Write>(\"" + escape(pathExtra) + "\"utf8)?}\n"
      "  return(score_files(extra, [spread] values))\n"
      "}\n"
      "\n"
      "[return<int> effects(file_write) on_error<FileError, /swallow_file_error>]\n"
      "main() {\n"
      "  [File<Write>] a0{File<Write>(\"" + escape(pathA0) + "\"utf8)?}\n"
      "  [File<Write>] a1{File<Write>(\"" + escape(pathA1) + "\"utf8)?}\n"
      "  [File<Write>] a2{File<Write>(\"" + escape(pathA2) + "\"utf8)?}\n"
      "  [File<Write>] b0{File<Write>(\"" + escape(pathB0) + "\"utf8)?}\n"
      "  [File<Write>] b1{File<Write>(\"" + escape(pathB1) + "\"utf8)?}\n"
      "  [File<Write>] b2{File<Write>(\"" + escape(pathB2) + "\"utf8)?}\n"
      "  [File<Write>] c0{File<Write>(\"" + escape(pathC0) + "\"utf8)?}\n"
      "  [File<Write>] c1{File<Write>(\"" + escape(pathC1) + "\"utf8)?}\n"
      "  return(plus(score_files(a0, a1, a2), plus(forward(b0, b1, b2), forward_mixed(c0, c1))))\n"
      "}\n";
  const std::string srcPath = writeTemp("compile_native_variadic_args_file_handle.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_file_handle").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 39);
  CHECK(readFile(pathA0) == "alpha\n");
  CHECK(readFile(pathA2) == "omega\n");
  CHECK(readFile(pathB0) == "alpha\n");
  CHECK(readFile(pathB2) == "omega\n");
  CHECK(readFile(pathExtra) == "alpha\n");
  CHECK(readFile(pathC1) == "omega\n");
}

TEST_CASE("native materializes variadic borrowed File handle packs with indexed dereference file methods") {
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
  const std::string pathA0 =
      (testScratchPath("") / "primec_native_variadic_borrowed_file_handle_a0.txt").string();
  const std::string pathA1 =
      (testScratchPath("") / "primec_native_variadic_borrowed_file_handle_a1.txt").string();
  const std::string pathA2 =
      (testScratchPath("") / "primec_native_variadic_borrowed_file_handle_a2.txt").string();
  const std::string pathB0 =
      (testScratchPath("") / "primec_native_variadic_borrowed_file_handle_b0.txt").string();
  const std::string pathB1 =
      (testScratchPath("") / "primec_native_variadic_borrowed_file_handle_b1.txt").string();
  const std::string pathB2 =
      (testScratchPath("") / "primec_native_variadic_borrowed_file_handle_b2.txt").string();
  const std::string pathC0 =
      (testScratchPath("") / "primec_native_variadic_borrowed_file_handle_c0.txt").string();
  const std::string pathC1 =
      (testScratchPath("") / "primec_native_variadic_borrowed_file_handle_c1.txt").string();
  const std::string pathExtra =
      (testScratchPath("") / "primec_native_variadic_borrowed_file_handle_extra.txt").string();

  const std::string source =
      "[effects(file_write)]\n"
      "swallow_file_error([FileError] err) {}\n"
      "\n"
      "[return<int> effects(file_write) on_error<FileError, /swallow_file_error>]\n"
      "score_refs([args<Reference<File<Write>>>] values) {\n"
      "  dereference(values[0i32]).write_line(\"alpha\"utf8)?\n"
      "  dereference(values[minus(count(values), 1i32)]).write_line(\"omega\"utf8)?\n"
      "  dereference(values[0i32]).flush()?\n"
      "  dereference(values[minus(count(values), 1i32)]).flush()?\n"
      "  return(plus(count(values), 10i32))\n"
      "}\n"
      "\n"
      "[return<int> effects(file_write) on_error<FileError, /swallow_file_error>]\n"
      "forward([args<Reference<File<Write>>>] values) {\n"
      "  return(score_refs([spread] values))\n"
      "}\n"
      "\n"
      "[return<int> effects(file_write) on_error<FileError, /swallow_file_error>]\n"
      "forward_mixed([args<Reference<File<Write>>>] values) {\n"
      "  [File<Write>] extra{File<Write>(\"" + escape(pathExtra) + "\"utf8)?}\n"
      "  [Reference<File<Write>>] extra_ref{location(extra)}\n"
      "  return(score_refs(extra_ref, [spread] values))\n"
      "}\n"
      "\n"
      "[return<int> effects(file_write) on_error<FileError, /swallow_file_error>]\n"
      "main() {\n"
      "  [File<Write>] a0{File<Write>(\"" + escape(pathA0) + "\"utf8)?}\n"
      "  [File<Write>] a1{File<Write>(\"" + escape(pathA1) + "\"utf8)?}\n"
      "  [File<Write>] a2{File<Write>(\"" + escape(pathA2) + "\"utf8)?}\n"
      "  [Reference<File<Write>>] r0{location(a0)}\n"
      "  [Reference<File<Write>>] r1{location(a1)}\n"
      "  [Reference<File<Write>>] r2{location(a2)}\n"
      "  [File<Write>] b0{File<Write>(\"" + escape(pathB0) + "\"utf8)?}\n"
      "  [File<Write>] b1{File<Write>(\"" + escape(pathB1) + "\"utf8)?}\n"
      "  [File<Write>] b2{File<Write>(\"" + escape(pathB2) + "\"utf8)?}\n"
      "  [Reference<File<Write>>] s0{location(b0)}\n"
      "  [Reference<File<Write>>] s1{location(b1)}\n"
      "  [Reference<File<Write>>] s2{location(b2)}\n"
      "  [File<Write>] c0{File<Write>(\"" + escape(pathC0) + "\"utf8)?}\n"
      "  [File<Write>] c1{File<Write>(\"" + escape(pathC1) + "\"utf8)?}\n"
      "  [Reference<File<Write>>] t0{location(c0)}\n"
      "  [Reference<File<Write>>] t1{location(c1)}\n"
      "  return(plus(score_refs(r0, r1, r2), plus(forward(s0, s1, s2), forward_mixed(t0, t1))))\n"
      "}\n";
  const std::string srcPath = writeTemp("compile_native_variadic_args_borrowed_file_handle.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_borrowed_file_handle").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 39);
  CHECK(readFile(pathA0) == "alpha\n");
  CHECK(readFile(pathA2) == "omega\n");
  CHECK(readFile(pathB0) == "alpha\n");
  CHECK(readFile(pathB2) == "omega\n");
  CHECK(readFile(pathExtra) == "alpha\n");
  CHECK(readFile(pathC1) == "omega\n");
}

TEST_CASE("native materializes variadic pointer File handle packs with indexed dereference file methods") {
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
  const std::string pathA0 =
      (testScratchPath("") / "primec_native_variadic_pointer_file_handle_a0.txt").string();
  const std::string pathA1 =
      (testScratchPath("") / "primec_native_variadic_pointer_file_handle_a1.txt").string();
  const std::string pathA2 =
      (testScratchPath("") / "primec_native_variadic_pointer_file_handle_a2.txt").string();
  const std::string pathB0 =
      (testScratchPath("") / "primec_native_variadic_pointer_file_handle_b0.txt").string();
  const std::string pathB1 =
      (testScratchPath("") / "primec_native_variadic_pointer_file_handle_b1.txt").string();
  const std::string pathB2 =
      (testScratchPath("") / "primec_native_variadic_pointer_file_handle_b2.txt").string();
  const std::string pathC0 =
      (testScratchPath("") / "primec_native_variadic_pointer_file_handle_c0.txt").string();
  const std::string pathC1 =
      (testScratchPath("") / "primec_native_variadic_pointer_file_handle_c1.txt").string();
  const std::string pathExtra =
      (testScratchPath("") / "primec_native_variadic_pointer_file_handle_extra.txt").string();

  const std::string source =
      "[effects(file_write)]\n"
      "swallow_file_error([FileError] err) {}\n"
      "\n"
      "[return<int> effects(file_write) on_error<FileError, /swallow_file_error>]\n"
      "score_ptrs([args<Pointer<File<Write>>>] values) {\n"
      "  dereference(values[0i32]).write_line(\"alpha\"utf8)?\n"
      "  dereference(values[minus(count(values), 1i32)]).write_line(\"omega\"utf8)?\n"
      "  dereference(values[0i32]).flush()?\n"
      "  dereference(values[minus(count(values), 1i32)]).flush()?\n"
      "  return(plus(count(values), 10i32))\n"
      "}\n"
      "\n"
      "[return<int> effects(file_write) on_error<FileError, /swallow_file_error>]\n"
      "forward([args<Pointer<File<Write>>>] values) {\n"
      "  return(score_ptrs([spread] values))\n"
      "}\n"
      "\n"
      "[return<int> effects(file_write) on_error<FileError, /swallow_file_error>]\n"
      "forward_mixed([args<Pointer<File<Write>>>] values) {\n"
      "  [File<Write>] extra{File<Write>(\"" + escape(pathExtra) + "\"utf8)?}\n"
      "  [Pointer<File<Write>>] extra_ptr{location(extra)}\n"
      "  return(score_ptrs(extra_ptr, [spread] values))\n"
      "}\n"
      "\n"
      "[return<int> effects(file_write) on_error<FileError, /swallow_file_error>]\n"
      "main() {\n"
      "  [File<Write>] a0{File<Write>(\"" + escape(pathA0) + "\"utf8)?}\n"
      "  [File<Write>] a1{File<Write>(\"" + escape(pathA1) + "\"utf8)?}\n"
      "  [File<Write>] a2{File<Write>(\"" + escape(pathA2) + "\"utf8)?}\n"
      "  [Pointer<File<Write>>] r0{location(a0)}\n"
      "  [Pointer<File<Write>>] r1{location(a1)}\n"
      "  [Pointer<File<Write>>] r2{location(a2)}\n"
      "  [File<Write>] b0{File<Write>(\"" + escape(pathB0) + "\"utf8)?}\n"
      "  [File<Write>] b1{File<Write>(\"" + escape(pathB1) + "\"utf8)?}\n"
      "  [File<Write>] b2{File<Write>(\"" + escape(pathB2) + "\"utf8)?}\n"
      "  [Pointer<File<Write>>] s0{location(b0)}\n"
      "  [Pointer<File<Write>>] s1{location(b1)}\n"
      "  [Pointer<File<Write>>] s2{location(b2)}\n"
      "  [File<Write>] c0{File<Write>(\"" + escape(pathC0) + "\"utf8)?}\n"
      "  [File<Write>] c1{File<Write>(\"" + escape(pathC1) + "\"utf8)?}\n"
      "  [Pointer<File<Write>>] t0{location(c0)}\n"
      "  [Pointer<File<Write>>] t1{location(c1)}\n"
      "  return(plus(score_ptrs(r0, r1, r2), plus(forward(s0, s1, s2), forward_mixed(t0, t1))))\n"
      "}\n";
  const std::string srcPath = writeTemp("compile_native_variadic_args_pointer_file_handle.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_pointer_file_handle").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 39);
  CHECK(readFile(pathA0) == "alpha\n");
  CHECK(readFile(pathA2) == "omega\n");
  CHECK(readFile(pathB0) == "alpha\n");
  CHECK(readFile(pathB2) == "omega\n");
  CHECK(readFile(pathExtra) == "alpha\n");
  CHECK(readFile(pathC1) == "omega\n");
}
#endif

TEST_CASE("compiles and runs native void call with string param") {
  const std::string source = R"(
[return<void> effects(io_out)]
echo([string] msg) {
  print_line(msg)
}

[return<int> effects(io_out)]
main() {
  echo("alpha"utf8)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_string_call.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_string_call_exe").string();
  const std::string outPath = (testScratchPath("") / "primec_native_string_call_out.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "alpha\n");
}

TEST_CASE("compiles and runs native string indexing") {
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
  const std::string srcPath = writeTemp("compile_native_string_index_builtin.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_string_index_builtin_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == (97 + 98 + 3));
}

TEST_CASE("compiles and runs native string parameter indexing") {
  const std::string source = R"(
[return<i32>]
pick([string] text, [i32] index) {
  return(at(text, index))
}

[return<int>]
main() {
  return(pick("abc"utf8, 1i32))
}
)";
  const std::string srcPath = writeTemp("compile_native_string_param_index.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_string_param_index_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 98);
}

TEST_CASE("compiles and runs native software renderer command serialization deterministically") {
  const std::string source = R"(
import /std/ui/*
import /std/math/*
import /std/collections/*

[effects(io_out), return<void>]
dump_words([vector<i32>] words) {
  [i32] len{vectorCount<i32>(words)}
  for([i32 mut] index{0i32}, less_than(index, len), assign(index, plus(index, 1i32))) {
    if(greater_than(index, 0i32)) {
      print(","utf8)
    }
    print(vectorAt<i32>(words, index))
  }
  print_line(""utf8)
}

[effects(heap_alloc, io_out), return<int>]
main() {
  [CommandList mut] commands{CommandList()}
  commands.draw_rounded_rect(
    2i32,
    4i32,
    30i32,
    40i32,
    6i32,
    Rgba8([r] 12i32, [g] 34i32, [b] 56i32, [a] 255i32)
  )
  commands.draw_text(
    7i32,
    9i32,
    14i32,
    Rgba8([r] 255i32, [g] 240i32, [b] 0i32, [a] 255i32),
    "Hi!"utf8
  )
  dump_words(commands.serialize())
  return(commands.command_count())
}
)";
  const std::string srcPath = writeTemp("compile_native_ui_command_serialization.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_ui_command_serialization").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_ui_command_serialization.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 2);
  CHECK(readFile(outPath) == "1,2,2,9,2,4,30,40,6,12,34,56,255,1,11,7,9,14,255,240,0,255,3,72,105,33\n");
}

TEST_CASE("compiles and runs native software renderer clip stack serialization deterministically") {
  const std::string source = R"(
import /std/ui/*
import /std/math/*
import /std/collections/*

[effects(io_out), return<void>]
dump_words([vector<i32>] words) {
  [i32] len{vectorCount<i32>(words)}
  for([i32 mut] index{0i32}, less_than(index, len), assign(index, plus(index, 1i32))) {
    if(greater_than(index, 0i32)) {
      print(","utf8)
    }
    print(vectorAt<i32>(words, index))
  }
  print_line(""utf8)
}

[effects(heap_alloc, io_out), return<int>]
main() {
  [CommandList mut] commands{CommandList()}
  commands.push_clip(1i32, 2i32, 20i32, 10i32)
  commands.draw_text(
    7i32,
    9i32,
    14i32,
    Rgba8([r] 255i32, [g] 240i32, [b] 0i32, [a] 255i32),
    "Hi!"utf8
  )
  commands.push_clip(3i32, 4i32, 5i32, 6i32)
  commands.draw_rounded_rect(
    8i32,
    9i32,
    10i32,
    11i32,
    2i32,
    Rgba8([r] 1i32, [g] 2i32, [b] 3i32, [a] 4i32)
  )
  commands.pop_clip()
  commands.pop_clip()
  commands.pop_clip()
  dump_words(commands.serialize())
  return(plus(commands.command_count(), commands.clip_depth()))
}
)";
  const std::string srcPath = writeTemp("compile_native_ui_clip_command_serialization.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_ui_clip_command_serialization").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_ui_clip_command_serialization.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 6);
  CHECK(readFile(outPath) ==
        "1,6,3,4,1,2,20,10,1,11,7,9,14,255,240,0,255,3,72,105,33,3,4,3,4,5,6,2,9,8,9,10,11,2,1,2,3,4,4,0,4,0\n");
}

TEST_CASE("compiles and runs native two-pass layout tree serialization deterministically") {
  const std::string source = R"(
import /std/ui/*
import /std/math/*
import /std/collections/*

[effects(io_out), return<void>]
dump_words([vector<i32>] words) {
  [i32] len{vectorCount<i32>(words)}
  for([i32 mut] index{0i32}, less_than(index, len), assign(index, plus(index, 1i32))) {
    if(greater_than(index, 0i32)) {
      print(","utf8)
    }
    print(vectorAt<i32>(words, index))
  }
  print_line(""utf8)
}

[effects(heap_alloc, io_out), return<int>]
main() {
  [LayoutTree mut] tree{LayoutTree()}
  [i32] root{tree.append_root_column(2i32, 3i32, 10i32, 4i32)}
  [i32] header{tree.append_leaf(root, 20i32, 5i32)}
  [i32] body{tree.append_column(root, 1i32, 2i32, 12i32, 8i32)}
  [i32] badge{tree.append_leaf(body, 8i32, 4i32)}
  [i32] details{tree.append_leaf(body, 10i32, 6i32)}
  [i32] footer{tree.append_leaf(root, 6i32, 7i32)}
  tree.measure()
  tree.arrange(10i32, 20i32, 50i32, 40i32)
  dump_words(tree.serialize())
  return(tree.node_count())
}
)";
  const std::string srcPath = writeTemp("compile_native_ui_layout_tree_serialization.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_ui_layout_tree_serialization").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_ui_layout_tree_serialization.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 6);
  CHECK(readFile(outPath) ==
        "1,6,2,-1,24,36,10,20,50,40,1,0,20,5,12,22,46,5,2,0,12,14,12,30,46,14,1,2,8,4,13,31,44,4,1,2,10,6,13,37,44,6,1,0,6,7,12,47,46,7\n");
}

TEST_CASE("compiles and runs native two-pass layout empty root deterministically") {
  const std::string source = R"(
import /std/ui/*
import /std/math/*
import /std/collections/*

[effects(io_out), return<void>]
dump_words([vector<i32>] words) {
  [i32] len{vectorCount<i32>(words)}
  for([i32 mut] index{0i32}, less_than(index, len), assign(index, plus(index, 1i32))) {
    if(greater_than(index, 0i32)) {
      print(","utf8)
    }
    print(vectorAt<i32>(words, index))
  }
  print_line(""utf8)
}

[effects(heap_alloc, io_out), return<int>]
main() {
  [LayoutTree mut] tree{LayoutTree()}
  [i32] root{tree.append_root_column(4i32, 9i32, 11i32, 13i32)}
  tree.measure()
  tree.arrange(3i32, 5i32, 11i32, 13i32)
  dump_words(tree.serialize())
  return(tree.node_count())
}
)";
  const std::string srcPath = writeTemp("compile_native_ui_layout_tree_empty_root.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_ui_layout_tree_empty_root").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_ui_layout_tree_empty_root.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 1);
  CHECK(readFile(outPath) == "1,1,2,-1,11,13,3,5,11,13\n");
}

TEST_CASE("compiles and runs native basic widget controls through layout deterministically") {
  const std::string source = R"(
import /std/ui/*
import /std/math/*
import /std/collections/*

[effects(io_out), return<void>]
dump_words([vector<i32>] words) {
  [i32] len{vectorCount<i32>(words)}
  for([i32 mut] index{0i32}, less_than(index, len), assign(index, plus(index, 1i32))) {
    if(greater_than(index, 0i32)) {
      print(","utf8)
    }
    print(vectorAt<i32>(words, index))
  }
  print_line(""utf8)
}

[effects(heap_alloc, io_out), return<int>]
main() {
  [LayoutTree mut] layout{LayoutTree()}
  [i32] root{layout.append_root_column(1i32, 2i32, 0i32, 0i32)}
  [i32] title{layout.append_label(root, 10i32, "Hi"utf8)}
  [i32] action{layout.append_button(root, 10i32, 3i32, "Go"utf8)}
  [i32] field{layout.append_input(root, 10i32, 2i32, 18i32, "abc"utf8)}
  layout.measure()
  layout.arrange(5i32, 6i32, 30i32, 46i32)

  [CommandList mut] commands{CommandList()}
  commands.draw_label(layout, title, 10i32, Rgba8([r] 1i32, [g] 2i32, [b] 3i32, [a] 255i32), "Hi"utf8)
  commands.draw_button(
    layout,
    action,
    10i32,
    3i32,
    4i32,
    Rgba8([r] 10i32, [g] 20i32, [b] 30i32, [a] 255i32),
    Rgba8([r] 250i32, [g] 251i32, [b] 252i32, [a] 255i32),
    "Go"utf8
  )
  commands.draw_input(
    layout,
    field,
    10i32,
    2i32,
    3i32,
    Rgba8([r] 40i32, [g] 50i32, [b] 60i32, [a] 255i32),
    Rgba8([r] 200i32, [g] 210i32, [b] 220i32, [a] 255i32),
    "abc"utf8
  )
  dump_words(commands.serialize())
  return(plus(layout.node_count(), commands.command_count()))
}
)";
  const std::string srcPath = writeTemp("compile_native_ui_basic_widget_controls.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_ui_basic_widget_controls").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_ui_basic_widget_controls.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 9);
  CHECK(readFile(outPath) ==
        "1,5,1,10,6,7,10,1,2,3,255,2,72,105,2,9,6,19,28,16,4,10,20,30,255,1,10,9,22,10,250,251,252,255,2,71,111,2,9,6,37,28,14,3,40,50,60,255,1,11,8,39,10,200,210,220,255,3,97,98,99\n");
}

TEST_CASE("compiles and runs native panel container widget deterministically") {
  const std::string source = R"(
import /std/ui/*
import /std/math/*
import /std/collections/*

[effects(io_out), return<void>]
dump_words([vector<i32>] words) {
  [i32] len{vectorCount<i32>(words)}
  for([i32 mut] index{0i32}, less_than(index, len), assign(index, plus(index, 1i32))) {
    if(greater_than(index, 0i32)) {
      print(","utf8)
    }
    print(vectorAt<i32>(words, index))
  }
  print_line(""utf8)
}

[effects(heap_alloc, io_out), return<int>]
main() {
  [LayoutTree mut] layout{LayoutTree()}
  [i32] root{layout.append_root_column(1i32, 2i32, 0i32, 0i32)}
  [i32] title{layout.append_label(root, 10i32, "Top"utf8)}
  [i32] panel{layout.append_panel(root, 2i32, 1i32, 20i32, 12i32)}
  [i32] action{layout.append_button(panel, 10i32, 2i32, "Go"utf8)}
  [i32] field{layout.append_input(panel, 10i32, 1i32, 16i32, "abc"utf8)}
  [i32] footer{layout.append_label(root, 10i32, "!"utf8)}
  layout.measure()
  layout.arrange(4i32, 5i32, 28i32, 60i32)

  [CommandList mut] commands{CommandList()}
  commands.draw_label(layout, title, 10i32, Rgba8([r] 1i32, [g] 2i32, [b] 3i32, [a] 255i32), "Top"utf8)
  commands.begin_panel(layout, panel, 4i32, Rgba8([r] 8i32, [g] 9i32, [b] 10i32, [a] 255i32))
  commands.draw_button(
    layout,
    action,
    10i32,
    2i32,
    3i32,
    Rgba8([r] 20i32, [g] 30i32, [b] 40i32, [a] 255i32),
    Rgba8([r] 200i32, [g] 201i32, [b] 202i32, [a] 255i32),
    "Go"utf8
  )
  commands.draw_input(
    layout,
    field,
    10i32,
    1i32,
    2i32,
    Rgba8([r] 50i32, [g] 60i32, [b] 70i32, [a] 255i32),
    Rgba8([r] 210i32, [g] 211i32, [b] 212i32, [a] 255i32),
    "abc"utf8
  )
  commands.end_panel()
  commands.draw_label(layout, footer, 10i32, Rgba8([r] 1i32, [g] 2i32, [b] 3i32, [a] 255i32), "!"utf8)
  dump_words(commands.serialize())
  return(plus(layout.node_count(), plus(commands.command_count(), commands.clip_depth())))
}
)";
  const std::string srcPath = writeTemp("compile_native_ui_panel_widget.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_ui_panel_widget").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_ui_panel_widget.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 15);
  CHECK(readFile(outPath) ==
        "1,9,1,11,5,6,10,1,2,3,255,3,84,111,112,2,9,5,18,26,31,4,8,9,10,255,3,4,7,20,22,27,2,9,7,20,22,14,3,20,30,40,255,1,10,9,22,10,200,201,202,255,2,71,111,2,9,7,35,22,12,2,50,60,70,255,1,11,8,36,10,210,211,212,255,3,97,98,99,4,0,1,9,5,51,10,1,2,3,255,1,33\n");
}

TEST_CASE("compiles and runs native empty panel container stays balanced deterministically") {
  const std::string source = R"(
import /std/ui/*
import /std/math/*
import /std/collections/*

[effects(io_out), return<void>]
dump_words([vector<i32>] words) {
  [i32] len{vectorCount<i32>(words)}
  for([i32 mut] index{0i32}, less_than(index, len), assign(index, plus(index, 1i32))) {
    if(greater_than(index, 0i32)) {
      print(","utf8)
    }
    print(vectorAt<i32>(words, index))
  }
  print_line(""utf8)
}

[effects(heap_alloc, io_out), return<int>]
main() {
  [LayoutTree mut] layout{LayoutTree()}
  [i32] root{layout.append_root_column(0i32, 0i32, 0i32, 0i32)}
  [i32] panel{layout.append_panel(root, 3i32, 1i32, 12i32, 10i32)}
  layout.measure()
  layout.arrange(2i32, 3i32, 20i32, 18i32)

  [CommandList mut] commands{CommandList()}
  commands.begin_panel(layout, panel, 5i32, Rgba8([r] 9i32, [g] 8i32, [b] 7i32, [a] 255i32))
  commands.end_panel()
  dump_words(commands.serialize())
  return(plus(layout.node_count(), plus(commands.command_count(), commands.clip_depth())))
}
)";
  const std::string srcPath = writeTemp("compile_native_ui_empty_panel_widget.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_ui_empty_panel_widget").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_ui_empty_panel_widget.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 5);
  CHECK(readFile(outPath) == "1,3,2,9,2,3,20,10,5,9,8,7,255,3,4,5,6,14,4,4,0\n");
}

TEST_CASE("compiles and runs native composite login form deterministically") {
  const std::string source = R"(
import /std/ui/*
import /std/math/*
import /std/collections/*

[effects(io_out), return<void>]
dump_words([vector<i32>] words) {
  [i32] len{vectorCount<i32>(words)}
  for([i32 mut] index{0i32}, less_than(index, len), assign(index, plus(index, 1i32))) {
    if(greater_than(index, 0i32)) {
      print(","utf8)
    }
    print(vectorAt<i32>(words, index))
  }
  print_line(""utf8)
}

[effects(heap_alloc, io_out), return<int>]
main() {
  [LayoutTree mut] layout{LayoutTree()}
  [i32] root{layout.append_root_column(1i32, 0i32, 0i32, 0i32)}
  [LoginFormNodes] login{layout.append_login_form(
    root,
    2i32,
    1i32,
    10i32,
    1i32,
    2i32,
    16i32,
    "Login"utf8,
    "alice"utf8,
    "secret"utf8,
    "Go"utf8
  )}
  layout.measure()
  layout.arrange(6i32, 7i32, 40i32, 57i32)

  [CommandList mut] commands{CommandList()}
  commands.draw_login_form(
    layout,
    login,
    10i32,
    1i32,
    2i32,
    4i32,
    3i32,
    Rgba8([r] 9i32, [g] 8i32, [b] 7i32, [a] 255i32),
    Rgba8([r] 1i32, [g] 2i32, [b] 3i32, [a] 255i32),
    Rgba8([r] 20i32, [g] 30i32, [b] 40i32, [a] 255i32),
    Rgba8([r] 200i32, [g] 201i32, [b] 202i32, [a] 255i32),
    Rgba8([r] 50i32, [g] 60i32, [b] 70i32, [a] 255i32),
    Rgba8([r] 250i32, [g] 251i32, [b] 252i32, [a] 255i32),
    "Login"utf8,
    "alice"utf8,
    "secret"utf8,
    "Go"utf8
  )
  dump_words(commands.serialize())
  return(plus(layout.node_count(), plus(commands.command_count(), commands.clip_depth())))
}
)";
  const std::string srcPath = writeTemp("compile_native_ui_composite_login_form.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_ui_composite_login_form").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_ui_composite_login_form.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 16);
  CHECK(readFile(outPath) ==
        "1,10,2,9,7,8,38,55,4,9,8,7,255,3,4,9,10,34,51,1,13,9,10,10,1,2,3,255,5,76,111,103,105,110,2,9,9,21,34,12,3,20,30,40,255,1,13,10,22,10,200,201,202,255,5,97,108,105,99,101,2,9,9,34,34,12,3,20,30,40,255,1,14,10,35,10,200,201,202,255,6,115,101,99,114,101,116,2,9,9,47,34,14,3,50,60,70,255,1,10,11,49,10,250,251,252,255,2,71,111,4,0\n");
}

TEST_CASE("compiles and runs native html adapter login form deterministically") {
  const std::string source = R"(
import /std/ui/*
import /std/math/*
import /std/collections/*

[effects(io_out), return<void>]
dump_words([vector<i32>] words) {
  [i32] len{vectorCount<i32>(words)}
  for([i32 mut] index{0i32}, less_than(index, len), assign(index, plus(index, 1i32))) {
    if(greater_than(index, 0i32)) {
      print(","utf8)
    }
    print(vectorAt<i32>(words, index))
  }
  print_line(""utf8)
}

[effects(heap_alloc, io_out), return<int>]
main() {
  [LayoutTree mut] layout{LayoutTree()}
  [i32] root{layout.append_root_column(1i32, 0i32, 0i32, 0i32)}
  [LoginFormNodes] login{layout.append_login_form(
    root,
    2i32,
    1i32,
    10i32,
    1i32,
    2i32,
    16i32,
    "Login"utf8,
    "alice"utf8,
    "secret"utf8,
    "Go"utf8
  )}
  layout.measure()
  layout.arrange(6i32, 7i32, 40i32, 57i32)

  [HtmlCommandList mut] html{HtmlCommandList()}
  html.emit_login_form(
    layout,
    login,
    10i32,
    1i32,
    2i32,
    4i32,
    3i32,
    Rgba8([r] 9i32, [g] 8i32, [b] 7i32, [a] 255i32),
    Rgba8([r] 1i32, [g] 2i32, [b] 3i32, [a] 255i32),
    Rgba8([r] 20i32, [g] 30i32, [b] 40i32, [a] 255i32),
    Rgba8([r] 200i32, [g] 201i32, [b] 202i32, [a] 255i32),
    Rgba8([r] 50i32, [g] 60i32, [b] 70i32, [a] 255i32),
    Rgba8([r] 250i32, [g] 251i32, [b] 252i32, [a] 255i32),
    "Login"utf8,
    "alice"utf8,
    "secret"utf8,
    "Go"utf8,
    "user_input"utf8,
    "pass_input"utf8,
    "submit_click"utf8
  )
  dump_words(html.serialize())
  return(plus(layout.node_count(), html.commandCount))
}
)";
  const std::string srcPath = writeTemp("compile_native_ui_html_login_form.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_ui_html_login_form").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_ui_html_login_form.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 14);
  CHECK(readFile(outPath) ==
        "1,8,1,12,1,0,7,8,38,55,2,4,9,8,7,255,2,17,2,1,9,10,34,10,10,1,2,3,255,5,76,111,103,105,110,4,23,3,1,9,21,34,12,10,1,3,20,30,40,255,200,201,202,255,5,97,108,105,99,101,5,13,3,2,10,117,115,101,114,95,105,110,112,117,116,4,24,4,1,9,34,34,12,10,1,3,20,30,40,255,200,201,202,255,6,115,101,99,114,101,116,5,13,4,2,10,112,97,115,115,95,105,110,112,117,116,3,20,5,1,9,47,34,14,10,2,3,50,60,70,255,250,251,252,255,2,71,111,5,15,5,1,12,115,117,98,109,105,116,95,99,108,105,99,107\n");
}

TEST_CASE("compiles and runs native ui event stream deterministically") {
  const std::string source = R"(
import /std/ui/*
import /std/math/*
import /std/collections/*

[effects(io_out), return<void>]
dump_words([vector<i32>] words) {
  [i32] len{vectorCount<i32>(words)}
  for([i32 mut] index{0i32}, less_than(index, len), assign(index, plus(index, 1i32))) {
    if(greater_than(index, 0i32)) {
      print(","utf8)
    }
    print(vectorAt<i32>(words, index))
  }
  print_line(""utf8)
}

[effects(heap_alloc, io_out), return<int>]
main() {
  [LayoutTree mut] layout{LayoutTree()}
  [i32] root{layout.append_root_column(1i32, 0i32, 0i32, 0i32)}
  [LoginFormNodes] login{layout.append_login_form(
    root,
    2i32,
    1i32,
    10i32,
    1i32,
    2i32,
    16i32,
    "Login"utf8,
    "alice"utf8,
    "secret"utf8,
    "Go"utf8
  )}

  [UiEventStream mut] events{UiEventStream()}
  events.push_pointer_move(login.usernameInput, 7i32, 20i32, 30i32)
  events.push_pointer_down(login.submitButton, 7i32, 1i32, 20i32, 30i32)
  events.push_pointer_up(login.submitButton, 7i32, 1i32, 21i32, 31i32)
  events.push_key_down(login.usernameInput, 13i32, 3i32, 1i32)
  events.push_key_up(login.usernameInput, 13i32, 1i32)
  dump_words(events.serialize())
  return(plus(login.submitButton, events.event_count()))
}
)";
  const std::string srcPath = writeTemp("compile_native_ui_event_stream.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_ui_event_stream").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_ui_event_stream.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 10);
  CHECK(readFile(outPath) ==
        "1,5,1,5,3,7,-1,20,30,2,5,5,7,1,20,30,3,5,5,7,1,21,31,4,4,3,13,3,1,5,4,3,13,1,0\n");
}

TEST_CASE("compiles and runs native ui ime event stream deterministically") {
  const std::string source = R"(
import /std/ui/*
import /std/math/*
import /std/collections/*

[effects(io_out), return<void>]
dump_words([vector<i32>] words) {
  [i32] len{vectorCount<i32>(words)}
  for([i32 mut] index{0i32}, less_than(index, len), assign(index, plus(index, 1i32))) {
    if(greater_than(index, 0i32)) {
      print(","utf8)
    }
    print(vectorAt<i32>(words, index))
  }
  print_line(""utf8)
}

[effects(heap_alloc, io_out), return<int>]
main() {
  [LayoutTree mut] layout{LayoutTree()}
  [i32] root{layout.append_root_column(1i32, 0i32, 0i32, 0i32)}
  [LoginFormNodes] login{layout.append_login_form(
    root,
    2i32,
    1i32,
    10i32,
    1i32,
    2i32,
    16i32,
    "Login"utf8,
    "alice"utf8,
    "secret"utf8,
    "Go"utf8
  )}

  [UiEventStream mut] events{UiEventStream()}
  events.push_ime_preedit(login.usernameInput, 1i32, 4i32, "al|"utf8)
  events.push_ime_commit(login.usernameInput, "alice"utf8)
  dump_words(events.serialize())
  return(plus(login.usernameInput, events.event_count()))
}
)";
  const std::string srcPath = writeTemp("compile_native_ui_ime_event_stream.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_ui_ime_event_stream").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_ui_ime_event_stream.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 5);
  CHECK(readFile(outPath) ==
        "1,2,6,7,3,1,4,3,97,108,124,7,9,3,-1,-1,5,97,108,105,99,101\n");
}

TEST_CASE("compiles and runs native ui resize and focus event stream deterministically") {
  const std::string source = R"(
import /std/ui/*
import /std/math/*
import /std/collections/*

[effects(io_out), return<void>]
dump_words([vector<i32>] words) {
  [i32] len{vectorCount<i32>(words)}
  for([i32 mut] index{0i32}, less_than(index, len), assign(index, plus(index, 1i32))) {
    if(greater_than(index, 0i32)) {
      print(","utf8)
    }
    print(vectorAt<i32>(words, index))
  }
  print_line(""utf8)
}

[effects(heap_alloc, io_out), return<int>]
main() {
  [LayoutTree mut] layout{LayoutTree()}
  [i32] root{layout.append_root_column(1i32, 0i32, 0i32, 0i32)}
  [LoginFormNodes] login{layout.append_login_form(
    root,
    2i32,
    1i32,
    10i32,
    1i32,
    2i32,
    16i32,
    "Login"utf8,
    "alice"utf8,
    "secret"utf8,
    "Go"utf8
  )}

  [UiEventStream mut] events{UiEventStream()}
  events.push_resize(login.panel, 40i32, 57i32)
  events.push_focus_gained(login.usernameInput)
  events.push_focus_lost(login.usernameInput)
  dump_words(events.serialize())
  return(plus(login.usernameInput, events.event_count()))
}
)";
  const std::string srcPath = writeTemp("compile_native_ui_resize_focus_event_stream.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_ui_resize_focus_event_stream").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_ui_resize_focus_event_stream.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 6);
  CHECK(readFile(outPath) == "1,3,8,3,1,40,57,9,3,3,0,0,10,3,3,0,0\n");
}

TEST_CASE("compiles and runs native large frame") {
  std::ostringstream source;
  source << "[return<int>]\n";
  source << "main() {\n";
  for (int i = 0; i < 300; ++i) {
    source << "  [i32] v" << i << "{" << i << "i32}\n";
  }
  source << "  return(v0)\n";
  source << "}\n";
  const std::string srcPath = writeTemp("compile_native_large_frame.prime", source.str());
  const std::string exePath = (testScratchPath("") / "primec_native_large_frame_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("rejects native recursive calls") {
  const std::string source = R"(
[return<int>]
main() {
  return(main())
}
)";
  const std::string srcPath = writeTemp("compile_native_recursive.prime", source);
  const std::string errPath = (testScratchPath("") / "primec_native_recursive_err.txt").string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath) == "Native lowering error: native backend does not support recursive calls: /main\n");
}

TEST_CASE("native accepts string pointers") {
  const std::string source = R"(
[return<void>]
main() {
  [string] name{"hi"utf8}
  [Pointer<string>] ptr{location(name)}
}
)";
  const std::string srcPath = writeTemp("compile_native_string_ptr.prime", source);
  const std::string errPath = (testScratchPath("") / "primec_native_string_ptr_err.txt").string();
  const std::string exePath = (testScratchPath("") / "primec_native_string_ptr_exe").string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 0);
  CHECK(readFile(errPath).empty());
}

TEST_CASE("native ignores top-level executions") {
  const std::string source = R"(
[return<bool>]
unused() {
  return(equal("a"utf8, "b"utf8))
}

[return<int>]
main() {
  return(0i32)
}

unused()
)";
  const std::string srcPath = writeTemp("compile_native_exec_ignored.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_exec_ignored_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_SUITE_END();
#endif
