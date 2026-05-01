#include "test_compile_run_helpers.h"

#include "test_compile_run_native_backend_core_helpers.h"

#if PRIMESTRUCT_NATIVE_CORE_ENABLED
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
      "  print_line_error(FileError.why(err))\n"
      "}\n";
  const std::string srcPath = writeTemp("native_stdlib_file_helpers.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_stdlib_file_helpers_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
  CHECK(readFile(outPath) == "6566\nCDE");
}

TEST_CASE("native uses stdlib File open helper wrappers") {
  const std::string filePath =
      (std::filesystem::temp_directory_path() / "primec_native_stdlib_file_open_helpers.txt").string();
  const std::string stdoutPath =
      (std::filesystem::temp_directory_path() / "primec_native_stdlib_file_open_helpers_out.txt").string();
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
      "[return<Result<FileError>> effects(file_read, file_write, io_out) on_error<FileError, /log_file_error>]\n"
      "main() {\n"
      "  [File<Write>] writer{/File/openWrite(\"" + escapedPath + "\"utf8)?}\n"
      "  writer.write(\"alpha\"utf8)?\n"
      "  writer.close()?\n"
      "  [File<Append>] appender{/File/open_append(\"" + escapedPath + "\"utf8)?}\n"
      "  appender.write_line(\"omega\"utf8)?\n"
      "  appender.close()?\n"
      "  [File<Read>] reader{/File/openRead(\"" + escapedPath + "\"utf8)?}\n"
      "  [i32 mut] first{0i32}\n"
      "  reader.read_byte(first)?\n"
      "  print_line(first)\n"
      "  reader.close()?\n"
      "  return(Result.ok())\n"
      "}\n"
      "[effects(io_err)]\n"
      "log_file_error([FileError] err) {\n"
      "  print_line_error(FileError.why(err))\n"
      "}\n";
  const std::string srcPath = writeTemp("native_stdlib_file_open_helpers.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_stdlib_file_open_helpers_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + stdoutPath) == 0);
  CHECK(readFile(filePath) == "alphaomega\n");
  CHECK(readFile(stdoutPath) == "97\n");
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
      "  print_line_error(FileError.why(err))\n"
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
      "  print_line_error(FileError.why(err))\n"
      "}\n";
  const std::string srcPath = writeTemp("native_stdlib_file_string_helpers.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_stdlib_file_string_helpers_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
  CHECK(readFile(outPath) == "alphaomega\n");
}

TEST_CASE("native uses stdlib File helper wrappers and broader fallback arities") {
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
      "  /File/write<Write, string, i32, string, i32, string, i32, string, i32, string, i32>(file, \"prefix\"utf8, 1i32, \"-\"utf8, 2i32, \"-\"utf8, 3i32, \"=\"utf8, 4i32, \".\"utf8, 5i32)?\n"
      "  /File/write_line<Write, string, i32, string, i32, string, i32, string, i32, string, i32>(file, \"alpha\"utf8, 7i32, \"omega\"utf8, 8i32, \"delta\"utf8, 9i32, \"!\"utf8, 10i32, \"?\"utf8, 11i32)?\n"
      "  file.write(\"left\"utf8, 1i32, \"mid\"utf8, 2i32, \"right\"utf8, 3i32, \".\"utf8, 4i32, \"!\"utf8, 5i32)?\n"
      "  file.write_line(4i32, \" \"utf8, 5i32, \" \"utf8, 6i32, \"?\"utf8, 7i32, \"!\"utf8, 8i32, \"#\"utf8)?\n"
      "  file.close()?\n"
      "  return(Result.ok())\n"
      "}\n"
      "[effects(io_err)]\n"
      "log_file_error([FileError] err) {\n"
      "  print_line_error(FileError.why(err))\n"
      "}\n";
  const std::string srcPath = writeTemp("native_stdlib_file_broad_helpers.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_stdlib_file_multi_helpers_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
  CHECK(readFile(outPath) == "prefix1-2-3=4.5alpha7omega8delta9!10?11\nleft1mid2right3.4!54 5 6?7!8#\n");
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
  [Marker] marker{Marker{}}
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
  return(count_values(Pair{}, Pair{}))
}
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_struct_count.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_struct_count").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("native materializes pure spread struct variadic packs for count") {
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
  return(forward(Pair{}, Pair{}))
}
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_struct_spread.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_struct_spread").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
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
  return(count_values(Pair{}, [spread] values))
}

[return<int>]
main() {
  return(forward(Pair{}, Pair{}))
}
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_struct_mixed.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_struct_mixed").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
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
  return(score_pairs(Pair{7i32}, Pair{9i32}))
}
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_struct_index.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_struct_index").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 17);
}


TEST_SUITE_END();
#endif
