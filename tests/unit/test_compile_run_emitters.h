TEST_SUITE_BEGIN("primestruct.compile.run.emitters.cpp");

static void expectCppVectorCountCompatibilityTypeMismatchReject(const std::string &compileCmd) {
  const std::string errPath = (std::filesystem::temp_directory_path() /
                               "primec_cpp_vector_count_compatibility_type_mismatch_reject_err.txt")
                                  .string();
  const std::string captureCmd = compileCmd + " > /dev/null 2> " + errPath;
  CHECK(runCommand(captureCmd) != 0);
  CHECK_FALSE(readFile(errPath).empty());
}

TEST_CASE("compiles and runs chained method calls in C++ emitter") {
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
  const std::string srcPath = writeTemp("compile_method_chain.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_method_chain_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 4);
}

TEST_CASE("executions are ignored by C++ emitter") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

[return<void> effects(io_out)]
log([i32] value) {
  print_line("log"utf8)
  return()
}

[effects(io_out)]
log(1i32)
)";
  const std::string srcPath = writeTemp("compile_exec_ignored.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_exec_ignored_exe").string();
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_exec_ignored_out.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " > " + outPath;
  CHECK(runCommand(runCmd) == 1);
  CHECK(readFile(outPath).empty());
}

TEST_CASE("struct Create/Destroy helpers run in C++ emitter") {
  const std::string source = R"(
[struct]
Thing() {
  [i32] value{1i32}

  [effects(io_out)]
  Create() {
    print_line(1i32)
  }

  [effects(io_out)]
  Destroy() {
    print_line(2i32)
  }
}

[return<int>]
main() {
  Thing()
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_struct_lifecycle.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_struct_lifecycle_exe").string();
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_struct_lifecycle_out.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath).empty());
}

TEST_CASE("C++ emitter returns structs from functions") {
  const std::string source = R"(
[struct]
Point() {
  [i32] x{1i32}
  [i32] y{2i32}
}

[return<Point>]
make_point([i32] x, [i32] y) {
  return(Point([x] x, [y] y))
}

[return<int>]
sum([Point] value) {
  return(plus(value.x, value.y))
}

[return<int>]
main() {
  return(sum(make_point(3i32, 4i32)))
}
)";
  const std::string srcPath = writeTemp("compile_struct_return.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_struct_return_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("C++ emitter uses struct field defaults") {
  const std::string source = R"(
[struct]
Point() {
  [i32] x{1i32}
  [i32] y{2i32}
}

[return<int>]
main() {
  [Point] value{Point([x] 9i32)}
  return(plus(value.x, value.y))
}
)";
  const std::string srcPath = writeTemp("compile_struct_defaults.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_struct_defaults_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 11);
}

TEST_CASE("C++ emitter supports file io") {
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_file_io_exe.txt").string();
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
  const std::string srcPath = writeTemp("compile_file_io_exe.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_file_io_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
  CHECK(readFile(outPath) == "Hello 123 world\n\nABC");
}

TEST_CASE("C++ emitter supports file read_byte with deterministic eof") {
  const std::string inPath = (std::filesystem::temp_directory_path() / "primec_file_read_byte_exe.txt").string();
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
      "[return<Result<FileError>> effects(file_write, io_out) on_error<FileError, /log_file_error>]\n"
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
  const std::string srcPath = writeTemp("compile_file_read_byte_exe.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_file_read_byte_exe").string();
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_file_read_byte_out.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 0);
  CHECK(readFile(outPath) == "65\n66\nEOF\n99\n");
}

TEST_CASE("C++ emitter supports custom Result.why hooks") {
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
  const std::string srcPath = writeTemp("compile_result_why_custom.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_result_why_custom_exe").string();
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_result_why_custom_out.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "custom error\n");
}

TEST_CASE("C++ emitter supports image api unsupported contract deterministically") {
  const std::string source = R"(
import /std/image/*

[effects(heap_alloc, io_out, file_write), return<int>]
main() {
  [i32 mut] width{0i32}
  [i32 mut] height{0i32}
  [vector<i32> mut] pixels{vector<i32>()}
  print_line(Result.why(ppm.read(width, height, pixels, "input.ppm"utf8)))
  print_line(Result.why(ppm.write("output.ppm"utf8, width, height, pixels)))
  print_line(Result.why(png.read(width, height, pixels, "input.png"utf8)))
  print_line(Result.why(png.write("output.png"utf8, width, height, pixels)))
  return(plus(width, height))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_image_api_unsupported.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_cpp_image_api_unsupported").string();
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_cpp_image_api_unsupported.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 0);
  CHECK(readFile(outPath) ==
        "image_read_unsupported\n"
        "image_write_unsupported\n"
        "image_read_unsupported\n"
        "image_write_unsupported\n");
}

TEST_CASE("C++ emitter runs software renderer command serialization deterministically") {
  const std::string source = R"(
import /std/ui/*

[effects(io_out), return<void>]
dump_words([vector<i32>] words) {
  [i32] len{count(words)}
  for([i32 mut] index{0i32}, less_than(index, len), assign(index, plus(index, 1i32))) {
    if(greater_than(index, 0i32)) {
      print(","utf8)
    }
    print(words[index])
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
  const std::string srcPath = writeTemp("compile_cpp_ui_command_serialization.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_cpp_ui_command_serialization_exe").string();
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_cpp_ui_command_serialization.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 2);
  CHECK(readFile(outPath) == "1,2,2,9,2,4,30,40,6,12,34,56,255,1,11,7,9,14,255,240,0,255,3,72,105,33\n");
}

TEST_CASE("C++ emitter runs software renderer clip stack serialization deterministically") {
  const std::string source = R"(
import /std/ui/*

[effects(io_out), return<void>]
dump_words([vector<i32>] words) {
  [i32] len{count(words)}
  for([i32 mut] index{0i32}, less_than(index, len), assign(index, plus(index, 1i32))) {
    if(greater_than(index, 0i32)) {
      print(","utf8)
    }
    print(words[index])
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
  const std::string srcPath = writeTemp("compile_cpp_ui_clip_command_serialization.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_cpp_ui_clip_command_serialization_exe").string();
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_cpp_ui_clip_command_serialization.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 6);
  CHECK(readFile(outPath) ==
        "1,6,3,4,1,2,20,10,1,11,7,9,14,255,240,0,255,3,72,105,33,3,4,3,4,5,6,2,9,8,9,10,11,2,1,2,3,4,4,0,4,0\n");
}

TEST_CASE("C++ emitter runs two-pass layout tree serialization deterministically") {
  const std::string source = R"(
import /std/ui/*

[effects(io_out), return<void>]
dump_words([vector<i32>] words) {
  [i32] len{count(words)}
  for([i32 mut] index{0i32}, less_than(index, len), assign(index, plus(index, 1i32))) {
    if(greater_than(index, 0i32)) {
      print(","utf8)
    }
    print(words[index])
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
  const std::string srcPath = writeTemp("compile_cpp_ui_layout_tree_serialization.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_cpp_ui_layout_tree_serialization_exe").string();
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_cpp_ui_layout_tree_serialization.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 6);
  CHECK(readFile(outPath) ==
        "1,6,2,-1,24,36,10,20,50,40,1,0,20,5,12,22,46,5,2,0,12,14,12,30,46,14,1,2,8,4,13,31,44,4,1,2,10,6,13,37,44,6,1,0,6,7,12,47,46,7\n");
}

TEST_CASE("C++ emitter runs two-pass layout empty root deterministically") {
  const std::string source = R"(
import /std/ui/*

[effects(io_out), return<void>]
dump_words([vector<i32>] words) {
  [i32] len{count(words)}
  for([i32 mut] index{0i32}, less_than(index, len), assign(index, plus(index, 1i32))) {
    if(greater_than(index, 0i32)) {
      print(","utf8)
    }
    print(words[index])
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
  const std::string srcPath = writeTemp("compile_cpp_ui_layout_tree_empty_root.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_cpp_ui_layout_tree_empty_root_exe").string();
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_cpp_ui_layout_tree_empty_root.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 1);
  CHECK(readFile(outPath) == "1,1,2,-1,11,13,3,5,11,13\n");
}

TEST_CASE("C++ emitter runs basic widget controls through layout deterministically") {
  const std::string source = R"(
import /std/ui/*

[effects(io_out), return<void>]
dump_words([vector<i32>] words) {
  [i32] len{count(words)}
  for([i32 mut] index{0i32}, less_than(index, len), assign(index, plus(index, 1i32))) {
    if(greater_than(index, 0i32)) {
      print(","utf8)
    }
    print(words[index])
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
  const std::string srcPath = writeTemp("compile_cpp_ui_basic_widget_controls.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_cpp_ui_basic_widget_controls_exe").string();
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_cpp_ui_basic_widget_controls.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 9);
  CHECK(readFile(outPath) ==
        "1,5,1,10,6,7,10,1,2,3,255,2,72,105,2,9,6,19,28,16,4,10,20,30,255,1,10,9,22,10,250,251,252,255,2,71,111,2,9,6,37,28,14,3,40,50,60,255,1,11,8,39,10,200,210,220,255,3,97,98,99\n");
}

TEST_CASE("C++ emitter runs panel container widget deterministically") {
  const std::string source = R"(
import /std/ui/*

[effects(io_out), return<void>]
dump_words([vector<i32>] words) {
  [i32] len{count(words)}
  for([i32 mut] index{0i32}, less_than(index, len), assign(index, plus(index, 1i32))) {
    if(greater_than(index, 0i32)) {
      print(","utf8)
    }
    print(words[index])
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
  const std::string srcPath = writeTemp("compile_cpp_ui_panel_widget.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_cpp_ui_panel_widget_exe").string();
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_cpp_ui_panel_widget.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 14);
  CHECK(readFile(outPath) ==
        "1,8,1,11,5,6,10,1,2,3,255,3,84,111,112,2,9,5,18,26,31,4,8,9,10,255,3,4,7,20,22,27,2,9,7,20,22,14,3,20,30,40,255,1,10,9,22,10,200,201,202,255,2,71,111,2,9,7,35,22,12,2,50,60,70,255,1,11,8,36,10,210,211,212,255,3,97,98,99,4,0,1,9,5,51,10,1,2,3,255,1,33\n");
}

TEST_CASE("C++ emitter runs empty panel container stays balanced deterministically") {
  const std::string source = R"(
import /std/ui/*

[effects(io_out), return<void>]
dump_words([vector<i32>] words) {
  [i32] len{count(words)}
  for([i32 mut] index{0i32}, less_than(index, len), assign(index, plus(index, 1i32))) {
    if(greater_than(index, 0i32)) {
      print(","utf8)
    }
    print(words[index])
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
  const std::string srcPath = writeTemp("compile_cpp_ui_empty_panel_widget.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_cpp_ui_empty_panel_widget_exe").string();
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_cpp_ui_empty_panel_widget.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 5);
  CHECK(readFile(outPath) == "1,3,2,9,2,3,20,10,5,9,8,7,255,3,4,5,6,14,4,4,0\n");
}

TEST_CASE("C++ emitter runs composite login form deterministically") {
  const std::string source = R"(
import /std/ui/*

[effects(io_out), return<void>]
dump_words([vector<i32>] words) {
  [i32] len{count(words)}
  for([i32 mut] index{0i32}, less_than(index, len), assign(index, plus(index, 1i32))) {
    if(greater_than(index, 0i32)) {
      print(","utf8)
    }
    print(words[index])
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
  const std::string srcPath = writeTemp("compile_cpp_ui_composite_login_form.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_cpp_ui_composite_login_form_exe").string();
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_cpp_ui_composite_login_form.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 16);
  CHECK(readFile(outPath) ==
        "1,10,2,9,7,8,38,55,4,9,8,7,255,3,4,9,10,34,51,1,13,9,10,10,1,2,3,255,5,76,111,103,105,110,2,9,9,21,34,12,3,20,30,40,255,1,13,10,22,10,200,201,202,255,5,97,108,105,99,101,2,9,9,34,34,12,3,20,30,40,255,1,14,10,35,10,200,201,202,255,6,115,101,99,114,101,116,2,9,9,47,34,14,3,50,60,70,255,1,10,11,49,10,250,251,252,255,2,71,111,4,0\n");
}

TEST_CASE("C++ emitter runs html adapter login form deterministically") {
  const std::string source = R"(
import /std/ui/*

[effects(io_out), return<void>]
dump_words([vector<i32>] words) {
  [i32] len{count(words)}
  for([i32 mut] index{0i32}, less_than(index, len), assign(index, plus(index, 1i32))) {
    if(greater_than(index, 0i32)) {
      print(","utf8)
    }
    print(words[index])
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
  const std::string srcPath = writeTemp("compile_cpp_ui_html_login_form.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_cpp_ui_html_login_form_exe").string();
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_cpp_ui_html_login_form.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 14);
  CHECK(readFile(outPath) ==
        "1,8,1,12,1,0,7,8,38,55,2,4,9,8,7,255,2,17,2,1,9,10,34,10,10,1,2,3,255,5,76,111,103,105,110,4,23,3,1,9,21,34,12,10,1,3,20,30,40,255,200,201,202,255,5,97,108,105,99,101,5,13,3,2,10,117,115,101,114,95,105,110,112,117,116,4,24,4,1,9,34,34,12,10,1,3,20,30,40,255,200,201,202,255,6,115,101,99,114,101,116,5,13,4,2,10,112,97,115,115,95,105,110,112,117,116,3,20,5,1,9,47,34,14,10,2,3,50,60,70,255,250,251,252,255,2,71,111,5,15,5,1,12,115,117,98,109,105,116,95,99,108,105,99,107\n");
}

TEST_CASE("C++ emitter runs ui event stream deterministically") {
  const std::string source = R"(
import /std/ui/*

[effects(io_out), return<void>]
dump_words([vector<i32>] words) {
  [i32] len{count(words)}
  for([i32 mut] index{0i32}, less_than(index, len), assign(index, plus(index, 1i32))) {
    if(greater_than(index, 0i32)) {
      print(","utf8)
    }
    print(words[index])
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
  const std::string srcPath = writeTemp("compile_cpp_ui_event_stream.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_cpp_ui_event_stream_exe").string();
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_cpp_ui_event_stream.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 10);
  CHECK(readFile(outPath) ==
        "1,5,1,5,3,7,-1,20,30,2,5,5,7,1,20,30,3,5,5,7,1,21,31,4,4,3,13,3,1,5,4,3,13,1,0\n");
}

TEST_CASE("C++ emitter runs ui ime event stream deterministically") {
  const std::string source = R"(
import /std/ui/*

[effects(io_out), return<void>]
dump_words([vector<i32>] words) {
  [i32] len{count(words)}
  for([i32 mut] index{0i32}, less_than(index, len), assign(index, plus(index, 1i32))) {
    if(greater_than(index, 0i32)) {
      print(","utf8)
    }
    print(words[index])
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
  const std::string srcPath = writeTemp("compile_cpp_ui_ime_event_stream.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_cpp_ui_ime_event_stream_exe").string();
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_cpp_ui_ime_event_stream.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 5);
  CHECK(readFile(outPath) ==
        "1,2,6,7,3,1,4,3,97,108,124,7,9,3,-1,-1,5,97,108,105,99,101\n");
}

TEST_CASE("C++ emitter runs ui resize and focus event stream deterministically") {
  const std::string source = R"(
import /std/ui/*

[effects(io_out), return<void>]
dump_words([vector<i32>] words) {
  [i32] len{count(words)}
  for([i32 mut] index{0i32}, less_than(index, len), assign(index, plus(index, 1i32))) {
    if(greater_than(index, 0i32)) {
      print(","utf8)
    }
    print(words[index])
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
  const std::string srcPath = writeTemp("compile_cpp_ui_resize_focus_event_stream.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_cpp_ui_resize_focus_event_stream_exe").string();
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_cpp_ui_resize_focus_event_stream.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 6);
  CHECK(readFile(outPath) == "1,3,8,3,1,40,57,9,3,3,0,0,10,3,3,0,0\n");
}

TEST_CASE("C++ emitter supports graphics-style int return propagation with on_error") {
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

[return<Result<Frame, GfxError>>]
acquire_frame_ok() {
  return(Result.ok(Frame([token] 9i32)))
}

[return<Result<Frame, GfxError>>]
acquire_frame_fail() {
  return(7i32)
}

namespace Frame {
  [return<Result<GfxError>>]
  submit([Frame] self) {
    return(Result.ok())
  }
}

[effects(io_err)]
log_gfx_error([GfxError] err) {
  print_line_error(err.why())
}

[return<int> on_error<GfxError, /log_gfx_error>]
main_ok() {
  frame{acquire_frame_ok()?}
  frame.submit()?
  return(frame.token)
}

[return<int> effects(io_err) on_error<GfxError, /log_gfx_error>]
main_fail() {
  frame{acquire_frame_fail()?}
  return(frame.token)
}
)";
  const std::string srcPath = writeTemp("compile_graphics_int_on_error_exe.prime", source);
  const std::string okExePath =
      (std::filesystem::temp_directory_path() / "primec_graphics_int_on_error_exe_ok").string();
  const std::string failExePath =
      (std::filesystem::temp_directory_path() / "primec_graphics_int_on_error_exe_fail").string();
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_graphics_int_on_error_exe_err.txt").string();

  const std::string compileOkCmd =
      "./primec --emit=exe " + srcPath + " -o " + okExePath + " --entry /main_ok";
  const std::string compileFailCmd =
      "./primec --emit=exe " + srcPath + " -o " + failExePath + " --entry /main_fail";
  CHECK(runCommand(compileOkCmd) == 0);
  CHECK(runCommand(compileFailCmd) == 0);
  CHECK(runCommand(okExePath) == 9);
  CHECK(runCommand(failExePath + " 2> " + errPath) == 7);
  CHECK(readFile(errPath) == "frame_acquire_failed\n");
}

TEST_CASE("C++ emitter renders static fields and visibility") {
  const std::string source = R"(
[struct]
Widget() {
  [public i32] size{1i32}
  [private static i32] counter{2i32}
}

[return<int>]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_cpp_struct_static.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_struct_static.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("std::vector<uint64_t> heapSlots") != std::string::npos);
  CHECK(output.find("return ps_fn_0(stack, sp, heapSlots, argc, argv);") != std::string::npos);
}

TEST_CASE("C++ emitter uses copy to force by-value params") {
  const std::string source = R"(
[return<int>]
sum([vector<i32>] values, [copy vector<i32>] copy_values) {
  return(0i32)
}

[return<int> effects(heap_alloc)]
main() {
  return(sum(vector<i32>(1i32), vector<i32>(2i32)))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_copy_params.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_copy_params.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("heapAllocSlotCount") != std::string::npos);
  CHECK(output.find("static int64_t ps_fn_1(") != std::string::npos);
}

TEST_CASE("C++ emitter renders lambda captures") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] base{3i32}
  [i32 mut] bump{2i32}
  holder{[value base, ref bump]([i32] x) { [i32] sum{plus(x, base)} return(sum) }}
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_cpp_lambda.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_lambda.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("C++ emitter preserves explicit lambda captures") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] base{3i32}
  [i32 mut] bump{2i32}
  holder{[=, ref bump]([i32] x) { return(plus(x, bump)) }}
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_cpp_lambda_explicit.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_lambda_explicit.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("C++ emitter lambda mutators honor user vector helpers") {
  const std::string source = R"(
/vector/push([vector<i32> mut] values, [i32] value) { }
/vector/pop([vector<i32> mut] values) { }
/vector/reserve([vector<i32> mut] values, [i32] target) { }
/vector/clear([vector<i32> mut] values) { }
/vector/remove_at([vector<i32> mut] values, [i32] index) { }
/vector/remove_swap([vector<i32> mut] values, [i32] index) { }

[effects(heap_alloc), return<int>]
main() {
  holder{[]([i32] seed) {
    [vector<i32> mut] values{vector<i32>(1i32, 2i32, 3i32, seed)}
    push(values, 5i32)
    values.push(6i32)
    reserve(values, 10i32)
    values.reserve(11i32)
    remove_at(values, 0i32)
    values.remove_at(0i32)
    remove_swap(values, 0i32)
    values.remove_swap(0i32)
    pop(values)
    values.clear()
    clear(values)
    return(values.count())
  }}
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_cpp_lambda_vector_mutator_shadow_precedence.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_lambda_vector_mutator_shadow_precedence.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("C++ emitter lambda mutator positional call resolves user helper") {
  const std::string source = R"(
/vector/push([vector<i32> mut] values, [i32] value) { }

[effects(heap_alloc), return<int>]
main() {
  []() {
    [vector<i32> mut] values{vector<i32>(1i32)}
    push(5i32, values)
    return(values.count())
  }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_cpp_lambda_vector_mutator_positional_shadow.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_lambda_vector_mutator_positional_shadow.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects lambda std namespaced reordered mutator compatibility helper in C++ emitter") {
  const std::string source = R"(
/vector/push([vector<i32> mut] values, [i32] value) { }

[effects(heap_alloc), return<int>]
main() {
  holder{[]() {
    [vector<i32> mut] values{vector<i32>(1i32)}
    /std/collections/vector/push(5i32, values)
    return(values.count())
  }}
  return(holder())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_lambda_std_namespaced_reordered_mutator_compat_helper.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_cpp_lambda_std_namespaced_reordered_mutator_compat_helper_err.txt")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /i32/tag") != std::string::npos);
}

TEST_CASE("C++ emitter lambda mutator bool positional call resolves user helper") {
  const std::string source = R"(
/vector/push([vector<i32> mut] values, [bool] value) { }

[effects(heap_alloc), return<int>]
main() {
  []() {
    [vector<i32> mut] values{vector<i32>(1i32)}
    push(true, values)
    return(values.count())
  }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_cpp_lambda_vector_mutator_bool_positional_shadow.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_lambda_vector_mutator_bool_positional_shadow.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("C++ emitter lambda mutator named call prefers values receiver") {
  const std::string source = R"(
[effects(heap_alloc)]
/vector/push([vector<i32> mut] values, [vector<i32> mut] value) {
  pop(values)
}

[effects(heap_alloc), return<int>]
main() {
  holder{[]() {
    [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
    [vector<i32> mut] payload{vector<i32>(7i32, 8i32)}
    push([value] payload, [values] values)
    return(values.count())
  }}
  return(holder())
}
)";
  const std::string srcPath = writeTemp("compile_cpp_lambda_vector_mutator_named_values_receiver.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_lambda_vector_mutator_named_values_receiver_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("C++ emitter lambda mutator rewrite keeps known vector receiver leading names") {
  const std::string source = R"(
/i32/push([i32] value, [vector<i32> mut] values) { }

[effects(heap_alloc), return<int>]
main() {
  []() {
    [vector<i32> mut] values{vector<i32>(1i32)}
    [i32] index{8i32}
    push(values, index)
    return(values.count())
  }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_cpp_lambda_vector_mutator_known_receiver_no_reorder.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_lambda_vector_mutator_known_receiver_no_reorder.cpp")
          .string();

  const std::string compileCmd = "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("C++ emitter lambda mutator mismatch rejects user helper signatures") {
  const std::string source = R"(
/vector/push([vector<i32> mut] values, [bool] value) { }

[effects(heap_alloc), return<int>]
main() {
  []([i32] seed) {
    [vector<i32> mut] values{vector<i32>(seed)}
    push(values, 1i32)
    values.push(2i32)
    return(0i32)
  }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_cpp_lambda_vector_mutator_shadow_mismatch.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_lambda_vector_mutator_shadow_mismatch_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("C++ emitter lambda mutator mismatch rejects call-form helper") {
  const std::string source = R"(
/vector/push([vector<i32> mut] values, [bool] value) { }

[effects(heap_alloc), return<int>]
main() {
  []() {
    [vector<i32> mut] values{vector<i32>()}
    push(values, 1i32)
    return(0i32)
  }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_cpp_lambda_vector_mutator_call_mismatch.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_lambda_vector_mutator_call_mismatch_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("compiles and runs import alias in C++ emitter") {
  const std::string source = R"(
import /util
namespace util {
  [public return<int>]
  helper() {
    return(7i32)
  }
}
[return<int>]
main() {
  return(helper())
}
)";
  const std::string srcPath = writeTemp("compile_import_alias_exe.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_import_alias_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs array method calls in C++ emitter") {
  const std::string source = R"(
[return<int>]
/array/first([array<i32>] items) {
  return(items[0i32])
}

[return<int>]
main() {
  [array<i32>] items{array<i32>(7i32, 9i32)}
  return(items.first())
}
)";
  const std::string srcPath = writeTemp("compile_array_method.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_array_method_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs array index sugar") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(4i32, 7i32, 9i32)}
  return(values[1i32])
}
)";
  const std::string srcPath = writeTemp("compile_array_index.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_array_index_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs argv helpers in C++ emitter") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  print_line(args[1i32])
  return(args.count())
}
)";
  const std::string srcPath = writeTemp("compile_emit_argv.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_emit_argv_exe").string();
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_emit_argv_out.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);

  const std::string runCmd = exePath + " alpha beta > " + outPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(outPath) == "alpha\n");
}

TEST_CASE("compiles and runs array index sugar with u64") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(4i32, 7i32, 9i32)}
  return(values[1u64])
}
)";
  const std::string srcPath = writeTemp("compile_array_index_u64.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_array_index_u64_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs vector helpers in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32, 3i32)}
  push(values, 4i32)
  remove_at(values, 1i32)
  remove_swap(values, 1i32)
  pop(values)
  reserve(values, 8i32)
  capacity(values)
  clear(values)
  return(values.count())
}
)";
  const std::string srcPath = writeTemp("compile_vector_helpers_exe.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_vector_helpers_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("compiles and runs user vector mutator shadow precedence in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc)]
/vector/push([vector<i32> mut] values, [i32] value) { }

[effects(heap_alloc)]
/vector/pop([vector<i32> mut] values) { }

[effects(heap_alloc)]
/vector/reserve([vector<i32> mut] values, [i32] target) { }

[effects(heap_alloc)]
/vector/clear([vector<i32> mut] values) { }

[effects(heap_alloc)]
/vector/remove_at([vector<i32> mut] values, [i32] index) { }

[effects(heap_alloc)]
/vector/remove_swap([vector<i32> mut] values, [i32] index) { }

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32, 3i32, 4i32)}
  push(values, 5i32)
  values.push(6i32)
  reserve(values, 10i32)
  values.reserve(11i32)
  remove_at(values, 0i32)
  values.remove_at(0i32)
  remove_swap(values, 0i32)
  values.remove_swap(0i32)
  pop(values)
  values.clear()
  clear(values)
  return(values.count())
}
)";
  const std::string srcPath = writeTemp("compile_cpp_user_vector_mutator_shadow_precedence.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_cpp_user_vector_mutator_shadow_precedence_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 4);
}

TEST_CASE("C++ emitter statement mutator call-form resolves user helper") {
  const std::string source = R"(
/vector/push([vector<i32> mut] values, [i32] value) { }

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  push(values, 2i32)
  return(values.count())
}
)";
  const std::string srcPath = writeTemp("compile_cpp_vector_mutator_call_shadow_precedence.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vector_mutator_call_shadow_precedence.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("return ps_fn_0(stack, sp, heapSlots, argc, argv);") != std::string::npos);
}

TEST_CASE("rejects std namespaced reordered mutator compatibility helper in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc)]
/vector/push([vector<i32> mut] values, [i32] value) { }

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  /std/collections/vector/push(3i32, values)
  return(values.count())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_std_namespaced_reordered_mutator_compat_helper.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_cpp_std_namespaced_reordered_mutator_compat_helper_err.txt")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK_FALSE(readFile(errPath).empty());
}

TEST_CASE("compiles and runs user vector mutator named call shadow in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc)]
/vector/push([vector<i32> mut] values, [i32] value) { }

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  push([value] 3i32, [values] values)
  return(values.count())
}
)";
  const std::string srcPath = writeTemp("compile_cpp_vector_mutator_named_call_shadow.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_cpp_vector_mutator_named_call_shadow_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("C++ emitter statement mutator named call prefers values receiver") {
  const std::string source = R"(
[effects(heap_alloc)]
/vector/push([vector<i32> mut] values, [vector<i32> mut] value) {
  pop(values)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  [vector<i32> mut] payload{vector<i32>(9i32, 10i32)}
  push([value] payload, [values] values)
  return(values.count())
}
)";
  const std::string srcPath = writeTemp("compile_cpp_vector_mutator_named_values_receiver.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_cpp_vector_mutator_named_values_receiver_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs user vector mutator positional call shadow in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc)]
/vector/push([vector<i32> mut] values, [i32] value) { }

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  push(3i32, values)
  return(values.count())
}
)";
  const std::string srcPath = writeTemp("compile_cpp_vector_mutator_positional_call_shadow.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_cpp_vector_mutator_positional_call_shadow_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("rejects reordered namespaced vector push call compatibility alias in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc)]
/std/collections/vector/push([vector<i32> mut] values, [i32] value) { }

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  /vector/push(3i32, values)
  return(values.count())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_reordered_namespaced_vector_push_call_shadow.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_cpp_reordered_namespaced_vector_push_call_shadow_err.txt")
          .string();
  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("push requires vector binding") != std::string::npos);
}

TEST_CASE("rejects reordered namespaced vector push call expression compatibility alias in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/vector/push([vector<i32> mut] values, [i32] value) {
  return(value)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  return(/vector/push(3i32, values))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_reordered_namespaced_vector_push_call_expr_shadow.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_cpp_reordered_namespaced_vector_push_call_expr_shadow_err.txt")
          .string();
  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("push is only supported as a statement") != std::string::npos);
}

TEST_CASE("rejects auto-inferred std namespaced vector push compatibility receiver precedence in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/push([vector<i32> mut] values, [string] value) {
  return(11i32)
}

[effects(heap_alloc), return<bool>]
/std/collections/vector/push([vector<i32> mut] values, [string] value) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  [string] payload{"tag"raw_utf8}
  [auto] inferred{/std/collections/vector/push([value] payload, [values] values)}
  return(inferred)
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_std_namespaced_vector_push_expr_named_receiver_precedence_auto.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_cpp_std_namespaced_vector_push_expr_named_receiver_precedence_auto_err.txt")
          .string();
  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  const std::string error = readFile(errPath);
  CHECK(error.find("return type mismatch") != std::string::npos);
  CHECK(error.find("expected bool") != std::string::npos);
}

TEST_CASE("rejects auto-inferred std namespaced count helper compatibility receiver precedence in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/count([vector<i32>] values) {
  return(12i32)
}

[effects(heap_alloc), return<bool>]
/std/collections/vector/count([vector<i32>] values) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  [auto] inferred{/std/collections/vector/count(values)}
  return(inferred)
}
)";
  const std::string srcPath = writeTemp("compile_cpp_std_namespaced_vector_count_receiver_precedence_auto.prime",
                                        source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() /
       "primec_cpp_std_namespaced_vector_count_receiver_precedence_auto_exe")
          .string();
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_cpp_std_namespaced_vector_count_receiver_precedence_auto_err.txt")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main > /dev/null 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("return type mismatch: expected bool") != std::string::npos);
}

TEST_CASE("rejects auto-inferred std namespaced count helper canonical fallback in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<bool>]
/std/collections/vector/count([vector<i32>] values) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  [auto] inferred{/std/collections/vector/count(values)}
  return(inferred)
}
)";
  const std::string srcPath = writeTemp("compile_cpp_std_namespaced_vector_count_canonical_fallback_auto.prime",
                                        source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() /
       "primec_cpp_std_namespaced_vector_count_canonical_fallback_auto_exe")
          .string();
  const std::string errPath = (std::filesystem::temp_directory_path() /
                               "primec_cpp_std_namespaced_vector_count_canonical_fallback_auto_err.txt")
                                  .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main > /dev/null 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("return type mismatch: expected bool") != std::string::npos);
}

TEST_CASE("rejects std namespaced count expression compatibility receiver precedence in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/count([vector<i32>] values) {
  return(12i32)
}

[effects(heap_alloc), return<bool>]
/std/collections/vector/count([vector<i32>] values) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(/std/collections/vector/count(values))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_std_namespaced_vector_count_expr_receiver_precedence.prime",
                                        source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() /
       "primec_cpp_std_namespaced_vector_count_expr_receiver_precedence_exe")
          .string();
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_cpp_std_namespaced_vector_count_expr_receiver_precedence_err.txt")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main > /dev/null 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("return type mismatch: expected bool") != std::string::npos);
}

TEST_CASE("rejects std namespaced count expression canonical fallback in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<bool>]
/std/collections/vector/count([vector<i32>] values) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(/std/collections/vector/count(values))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_std_namespaced_vector_count_expr_canonical_fallback.prime",
                                        source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() /
       "primec_cpp_std_namespaced_vector_count_expr_canonical_fallback_exe")
          .string();
  const std::string errPath = (std::filesystem::temp_directory_path() /
                               "primec_cpp_std_namespaced_vector_count_expr_canonical_fallback_err.txt")
                                  .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main > /dev/null 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("return type mismatch: expected bool") != std::string::npos);
}

TEST_CASE("rejects std namespaced count non-builtin compatibility fallback in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/count([vector<i32>] values, [bool] marker) {
  return(91i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(/std/collections/vector/count(values, true))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_std_namespaced_count_non_builtin_compat_fallback.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_cpp_std_namespaced_count_non_builtin_compat_fallback_exe")
          .string();
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_cpp_std_namespaced_count_non_builtin_compat_fallback_err.txt")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main > /dev/null 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("argument count mismatch for builtin count") != std::string::npos);
}

TEST_CASE("rejects std namespaced count non-builtin compatibility fallback type mismatch in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<bool>]
/vector/count([vector<i32>] values, [bool] marker) {
  return(false)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(/std/collections/vector/count(values, true))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_std_namespaced_count_non_builtin_compat_fallback_mismatch.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() /
                               "primec_cpp_std_namespaced_count_non_builtin_compat_fallback_mismatch_exe")
                                  .string();
  const std::string errPath = (std::filesystem::temp_directory_path() /
                               "primec_cpp_std_namespaced_count_non_builtin_compat_fallback_mismatch_err.txt")
                                  .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main > /dev/null 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("argument count mismatch for builtin count") != std::string::npos);
}

TEST_CASE("rejects vector namespaced count non-builtin array fallback in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/array/count([vector<i32>] values, [bool] marker) {
  return(31i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(/vector/count(values, true))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_vector_namespaced_count_non_builtin_array_fallback_rejected.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_cpp_vector_namespaced_count_non_builtin_array_fallback_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects std namespaced capacity expression compatibility receiver precedence in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/capacity([vector<i32>] values) {
  return(12i32)
}

[effects(heap_alloc), return<bool>]
/std/collections/vector/capacity([vector<i32>] values) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(/std/collections/vector/capacity(values))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_std_namespaced_vector_capacity_expr_receiver_precedence.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() /
       "primec_cpp_std_namespaced_vector_capacity_expr_receiver_precedence_exe")
          .string();
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_cpp_std_namespaced_vector_capacity_expr_receiver_precedence_err.txt")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main > /dev/null 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("return type mismatch: expected bool") != std::string::npos);
}

TEST_CASE("rejects std namespaced capacity expression canonical fallback in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<bool>]
/std/collections/vector/capacity([vector<i32>] values) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(/std/collections/vector/capacity(values))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_std_namespaced_vector_capacity_expr_canonical_fallback.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() /
       "primec_cpp_std_namespaced_vector_capacity_expr_canonical_fallback_exe")
          .string();
  const std::string errPath = (std::filesystem::temp_directory_path() /
                               "primec_cpp_std_namespaced_vector_capacity_expr_canonical_fallback_err.txt")
                                  .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main > /dev/null 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("return type mismatch: expected bool") != std::string::npos);
}

TEST_CASE("compiles and runs user vector mutator bool positional call shadow in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc)]
/vector/push([vector<i32> mut] values, [bool] value) { }

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  push(true, values)
  return(values.count())
}
)";
  const std::string srcPath = writeTemp("compile_cpp_vector_mutator_bool_positional_call_shadow.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_cpp_vector_mutator_bool_positional_call_shadow_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("C++ emitter mutator rewrite keeps known vector receiver leading names") {
  const std::string source = R"(
[effects(heap_alloc)]
/i32/push([i32] value, [vector<i32> mut] values) { }

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  [i32] index{7i32}
  push(values, index)
  return(values.count())
}
)";
  const std::string srcPath = writeTemp("compile_cpp_mutator_known_receiver_no_reorder.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_cpp_mutator_known_receiver_no_reorder_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("compiles and runs user vector access named call shadow in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/at([vector<i32>] values, [i32] index) {
  return(91i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(at([index] 1i32, [values] values))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_vector_access_named_call_shadow.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_cpp_vector_access_named_call_shadow_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 91);
}

TEST_CASE("rejects auto-inferred std namespaced access helper compatibility receiver precedence in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/at([vector<i32>] values, [i32] index) {
  return(12i32)
}

[effects(heap_alloc), return<bool>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  [auto] inferred{/std/collections/vector/at([index] 0i32, [values] values)}
  return(inferred)
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_std_namespaced_vector_access_expr_named_receiver_precedence_auto.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() /
       "primec_cpp_std_namespaced_vector_access_expr_named_receiver_precedence_auto_exe")
          .string();
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_cpp_std_namespaced_vector_access_expr_named_receiver_precedence_auto_err.txt")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main > /dev/null 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("return type mismatch: expected bool") != std::string::npos);
}

TEST_CASE("rejects auto-inferred std namespaced access helper canonical fallback in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<bool>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  [auto] inferred{/std/collections/vector/at([index] 0i32, [values] values)}
  return(inferred)
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_std_namespaced_vector_access_expr_named_receiver_canonical_fallback_auto.prime",
                source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() /
       "primec_cpp_std_namespaced_vector_access_expr_named_receiver_canonical_fallback_auto_exe")
          .string();
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_cpp_std_namespaced_vector_access_expr_named_receiver_canonical_fallback_auto_err.txt")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main > /dev/null 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_CASE("rejects removed vector access alias named arguments in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(/vector/at([values] values, [index] 0i32))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_removed_vector_access_alias_named_args.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_cpp_removed_vector_access_alias_named_args_exe").string();
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_cpp_removed_vector_access_alias_named_args_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main > /dev/null 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_CASE("rejects removed vector access alias at_unsafe named arguments in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(/vector/at_unsafe([values] values, [index] 0i32))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_removed_vector_access_alias_at_unsafe_named_args.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() /
       "primec_cpp_removed_vector_access_alias_at_unsafe_named_args_exe")
          .string();
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_cpp_removed_vector_access_alias_at_unsafe_named_args_err.txt")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main > /dev/null 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_CASE("compiles and runs user vector access positional call shadow in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/at([vector<i32>] values, [i32] index) {
  return(73i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(at(1i32, values))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_vector_access_positional_call_shadow.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_cpp_vector_access_positional_call_shadow_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 73);
}

TEST_CASE("compiles and runs user map access string positional call shadow in C++ emitter") {
  const std::string source = R"(
[return<int>]
/map/at([map<string, i32>] values, [string] key) {
  return(84i32)
}

[return<int>]
main() {
  [map<string, i32>] values{map<string, i32>("only"raw_utf8, 2i32)}
  return(at("only"raw_utf8, values))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_map_access_string_positional_call_shadow.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_cpp_map_access_string_positional_call_shadow_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 84);
}

TEST_CASE("C++ emitter map access prefers later map receiver over string") {
  const std::string source = R"(
[return<int>]
/map/at([map<string, i32>] values, [string] key) {
  return(86i32)
}

[return<int>]
/string/at([string] values, [map<string, i32>] key) {
  return(87i32)
}

[return<int>]
main() {
  [map<string, i32>] values{map<string, i32>("only"raw_utf8, 2i32)}
  return(at("only"raw_utf8, values))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_map_access_later_receiver_precedence.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_cpp_map_access_later_receiver_precedence_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 86);
}

TEST_CASE("compiles and runs user map access unsafe string positional call shadow in C++ emitter") {
  const std::string source = R"(
[return<int>]
/map/at_unsafe([map<string, i32>] values, [string] key) {
  return(85i32)
}

[return<int>]
main() {
  [map<string, i32>] values{map<string, i32>("only"raw_utf8, 2i32)}
  return(at_unsafe("only"raw_utf8, values))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_map_access_unsafe_string_positional_call_shadow.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_cpp_map_access_unsafe_string_positional_call_shadow_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 85);
}

TEST_CASE("C++ emitter reorders canonical map access positional args") {
  const std::string source = R"(
[return<int>]
/std/collections/map/at([/std/collections/map<string, i32>] values, [string] key) {
  return(86i32)
}

[return<int>]
main() {
  [/std/collections/map<string, i32>] values{map<string, i32>("only"raw_utf8, 2i32)}
  return(/std/collections/map/at("only"raw_utf8, values))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_canonical_map_access_positional_reorder.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_cpp_canonical_map_access_positional_reorder_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 86);
}

TEST_CASE("C++ emitter keeps canonical map access key diagnostics on positional reorder") {
  const std::string source = R"(
[return<int>]
/std/collections/map/at([/std/collections/map<string, i32>] values, [string] key) {
  return(86i32)
}

[return<int>]
main() {
  [/std/collections/map<string, i32>] values{map<string, i32>("only"raw_utf8, 2i32)}
  return(/std/collections/map/at(1i32, values))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_canonical_map_access_positional_reorder_diag.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_cpp_canonical_map_access_positional_reorder_diag_exe")
          .string();
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_cpp_canonical_map_access_positional_reorder_diag_err.txt")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main > /dev/null 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("argument type mismatch for /std/collections/map/at parameter key") !=
        std::string::npos);
}

TEST_CASE("C++ emitter access rewrite keeps known collection receiver leading names") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/i32/at([i32] index, [vector<i32>] values) {
  return(66i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  [i32] index{1i32}
  return(at(values, index))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_access_known_receiver_no_reorder.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_cpp_access_known_receiver_no_reorder_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("rejects user vector mutator shadow arg mismatch in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc)]
/vector/push([vector<i32> mut] values, [bool] value) { }

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  push(values, 1i32)
  values.push(2i32)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_cpp_user_vector_mutator_shadow_arg_mismatch.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_cpp_user_vector_mutator_shadow_arg_mismatch_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects user vector mutator call-form arg mismatch in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc)]
/vector/push([vector<i32> mut] values, [bool] value) { }

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  push(values, 1i32)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_cpp_vector_mutator_call_shadow_mismatch.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_cpp_vector_mutator_call_shadow_mismatch_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("compiles and runs stdlib namespaced vector builtin aliases in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{/std/collections/vector/vector<i32>(4i32, 5i32)}
  /std/collections/vector/push(values, 6i32)
  [i32] countValue{/std/collections/vector/count(values)}
  [i32] capacityValue{/std/collections/vector/capacity(values)}
  [i32] tailValue{/std/collections/vector/at_unsafe(values, 2i32)}
  return(plus(plus(countValue, tailValue), minus(capacityValue, capacityValue)))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_stdlib_namespaced_vector_aliases.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_cpp_stdlib_namespaced_vector_aliases_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("rejects array namespaced vector constructor alias in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{/array/vector<i32>(4i32, 5i32)}
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_cpp_array_namespaced_vector_constructor_alias.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_cpp_array_namespaced_vector_constructor_alias_out.txt")
          .string();
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_cpp_array_namespaced_vector_constructor_alias_exe").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("unknown call target: /array/vector") != std::string::npos);
}

TEST_CASE("rejects array namespaced vector at alias in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{/std/collections/vector/vector<i32>(4i32, 5i32)}
  [i32] headValue{/array/at(values, 1i32)}
  return(headValue)
}
)";
  const std::string srcPath = writeTemp("compile_cpp_array_namespaced_vector_at_alias.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_cpp_array_namespaced_vector_at_alias_out.txt").string();
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_cpp_array_namespaced_vector_at_alias_exe").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("unknown call target: /array/at") != std::string::npos);
}

TEST_CASE("rejects array namespaced vector at_unsafe alias in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{/std/collections/vector/vector<i32>(4i32, 5i32)}
  [i32] tailValue{/array/at_unsafe(values, 1i32)}
  return(tailValue)
}
)";
  const std::string srcPath = writeTemp("compile_cpp_array_namespaced_vector_at_unsafe_alias.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_cpp_array_namespaced_vector_at_unsafe_alias_out.txt")
          .string();
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_cpp_array_namespaced_vector_at_unsafe_alias_exe").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("unknown call target: /array/at_unsafe") != std::string::npos);
}

TEST_CASE("rejects array namespaced vector count builtin alias in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{/std/collections/vector/vector<i32>(4i32, 5i32)}
  return(/array/count(values))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_array_namespaced_vector_count_builtin_alias.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_cpp_array_namespaced_vector_count_builtin_alias_out.txt")
          .string();
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_cpp_array_namespaced_vector_count_builtin_alias_exe").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("unknown call target: /array/count") != std::string::npos);
}

TEST_CASE("rejects array namespaced vector capacity alias in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{/std/collections/vector/vector<i32>(4i32, 5i32)}
  return(/array/capacity(values))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_array_namespaced_vector_capacity_alias.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_cpp_array_namespaced_vector_capacity_alias_out.txt")
          .string();
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_cpp_array_namespaced_vector_capacity_alias_exe").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("unknown call target: /array/capacity") != std::string::npos);
}

TEST_CASE("rejects array namespaced vector mutator alias in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{/std/collections/vector/vector<i32>(4i32, 5i32)}
  /array/push(values, 6i32)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_cpp_array_namespaced_vector_mutator_alias.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_cpp_array_namespaced_vector_mutator_alias_out.txt").string();
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_cpp_array_namespaced_vector_mutator_alias_exe").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("unknown call target: /array/push") != std::string::npos);
}

TEST_CASE("C++ emitter helper rejects full-path array mutator aliases") {
  primec::Expr call;
  call.kind = primec::Expr::Kind::Call;
  call.name = "/array/push";

  std::unordered_map<std::string, std::string> nameMap;
  std::string helper;
  CHECK_FALSE(primec::emitter::getVectorMutatorName(call, nameMap, helper));
}

TEST_CASE("C++ emitter helper rejects array namespaced vector constructor alias builtin") {
  primec::Expr call;
  call.kind = primec::Expr::Kind::Call;
  call.name = "/array/vector";

  std::string builtin;
  CHECK_FALSE(primec::emitter::getBuiltinCollectionName(call, builtin));
}

TEST_CASE("C++ emitter helper resolves bare vector count methods") {
  primec::Expr call;
  call.kind = primec::Expr::Kind::Call;
  call.isMethodCall = true;
  call.name = "count";

  primec::Expr receiver;
  receiver.kind = primec::Expr::Kind::Name;
  receiver.name = "values";
  call.args.push_back(receiver);
  call.argNames.push_back(std::nullopt);

  std::unordered_map<std::string, primec::emitter::BindingInfo> localTypes;
  primec::emitter::BindingInfo receiverInfo;
  receiverInfo.typeName = "vector";
  localTypes.emplace("values", receiverInfo);
  std::unordered_map<std::string, const primec::Definition *> defMap;
  std::unordered_map<std::string, std::string> importAliases;
  std::unordered_map<std::string, std::string> structTypeMap;
  std::unordered_map<std::string, primec::emitter::ReturnKind> returnKinds;
  std::unordered_map<std::string, std::string> returnStructs;
  std::string resolved;

  CHECK(primec::emitter::resolveMethodCallPath(
      call, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
  CHECK(resolved == "/vector/count");

  localTypes.clear();
  CHECK_FALSE(primec::emitter::resolveMethodCallPath(
      call, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
}

TEST_CASE("C++ emitter helper rejects removed full-path vector method aliases") {
  primec::Expr call;
  call.kind = primec::Expr::Kind::Call;
  call.isMethodCall = true;

  primec::Expr receiver;
  receiver.kind = primec::Expr::Kind::Name;
  receiver.name = "values";
  call.args.push_back(receiver);
  call.argNames.push_back(std::nullopt);

  std::unordered_map<std::string, primec::emitter::BindingInfo> localTypes;
  primec::emitter::BindingInfo receiverInfo;
  receiverInfo.typeName = "vector";
  localTypes.emplace("values", receiverInfo);

  std::unordered_map<std::string, const primec::Definition *> defMap;
  std::unordered_map<std::string, std::string> importAliases;
  std::unordered_map<std::string, std::string> structTypeMap;
  std::unordered_map<std::string, primec::emitter::ReturnKind> returnKinds;
  std::unordered_map<std::string, std::string> returnStructs;
  std::string resolved = "/stale/path";

  call.name = "/array/count";
  CHECK_FALSE(primec::emitter::resolveMethodCallPath(
      call, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
  CHECK(resolved.empty());

  resolved = "/stale/path";
  call.name = "/vector/count";
  CHECK_FALSE(primec::emitter::resolveMethodCallPath(
      call, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
  CHECK(resolved.empty());

  resolved = "/stale/path";
  call.name = "/std/collections/vector/count";
  CHECK_FALSE(primec::emitter::resolveMethodCallPath(
      call, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
  CHECK(resolved.empty());
}

TEST_CASE("C++ emitter helper keeps canonical receiver metadata for namespaced map access") {
  primec::Expr receiverCall;
  receiverCall.kind = primec::Expr::Kind::Call;
  receiverCall.name = "/std/collections/map/at";

  primec::Expr receiverName;
  receiverName.kind = primec::Expr::Kind::Name;
  receiverName.name = "values";

  primec::Expr keyLiteral;
  keyLiteral.kind = primec::Expr::Kind::Literal;
  keyLiteral.intWidth = 32;
  keyLiteral.literalValue = 1;

  receiverCall.args.push_back(receiverName);
  receiverCall.args.push_back(keyLiteral);
  receiverCall.argNames.push_back(std::nullopt);
  receiverCall.argNames.push_back(std::nullopt);

  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.isMethodCall = true;
  methodCall.name = "tag";
  methodCall.args.push_back(receiverCall);
  methodCall.argNames.push_back(std::nullopt);

  std::unordered_map<std::string, primec::emitter::BindingInfo> localTypes;
  primec::emitter::BindingInfo receiverInfo;
  receiverInfo.typeName = "map";
  localTypes.emplace("values", receiverInfo);

  std::unordered_map<std::string, const primec::Definition *> defMap;
  std::unordered_map<std::string, std::string> importAliases;
  std::unordered_map<std::string, std::string> structTypeMap;
  std::unordered_map<std::string, primec::emitter::ReturnKind> returnKinds;
  std::unordered_map<std::string, std::string> returnStructs;
  returnStructs.emplace("/map/at", "/AliasMarker");
  returnStructs.emplace("/std/collections/map/at", "/CanonicalMarker");

  std::string resolved;
  CHECK(primec::emitter::resolveMethodCallPath(
      methodCall, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
  CHECK(resolved == "/CanonicalMarker/tag");
}

TEST_CASE("C++ emitter helper falls back to canonical map receiver metadata when alias missing") {
  primec::Expr receiverCall;
  receiverCall.kind = primec::Expr::Kind::Call;
  receiverCall.name = "/std/collections/map/at";

  primec::Expr receiverName;
  receiverName.kind = primec::Expr::Kind::Name;
  receiverName.name = "values";

  primec::Expr keyLiteral;
  keyLiteral.kind = primec::Expr::Kind::Literal;
  keyLiteral.intWidth = 32;
  keyLiteral.literalValue = 1;

  receiverCall.args.push_back(receiverName);
  receiverCall.args.push_back(keyLiteral);
  receiverCall.argNames.push_back(std::nullopt);
  receiverCall.argNames.push_back(std::nullopt);

  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.isMethodCall = true;
  methodCall.name = "tag";
  methodCall.args.push_back(receiverCall);
  methodCall.argNames.push_back(std::nullopt);

  std::unordered_map<std::string, primec::emitter::BindingInfo> localTypes;
  primec::emitter::BindingInfo receiverInfo;
  receiverInfo.typeName = "map";
  localTypes.emplace("values", receiverInfo);

  std::unordered_map<std::string, const primec::Definition *> defMap;
  std::unordered_map<std::string, std::string> importAliases;
  std::unordered_map<std::string, std::string> structTypeMap;
  std::unordered_map<std::string, primec::emitter::ReturnKind> returnKinds;
  std::unordered_map<std::string, std::string> returnStructs;
  returnStructs.emplace("/std/collections/map/at", "/CanonicalMarker");

  std::string resolved;
  CHECK(primec::emitter::resolveMethodCallPath(
      methodCall, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
  CHECK(resolved == "/CanonicalMarker/tag");
}

TEST_CASE("C++ emitter helper keeps bare map access canonical struct forwarding") {
  primec::Expr receiverCall;
  receiverCall.kind = primec::Expr::Kind::Call;
  receiverCall.isMethodCall = true;
  receiverCall.name = "at";

  primec::Expr receiverName;
  receiverName.kind = primec::Expr::Kind::Name;
  receiverName.name = "values";

  primec::Expr keyLiteral;
  keyLiteral.kind = primec::Expr::Kind::Literal;
  keyLiteral.intWidth = 32;
  keyLiteral.literalValue = 1;

  receiverCall.args.push_back(receiverName);
  receiverCall.args.push_back(keyLiteral);
  receiverCall.argNames.push_back(std::nullopt);
  receiverCall.argNames.push_back(std::nullopt);

  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.isMethodCall = true;
  methodCall.name = "tag";
  methodCall.args.push_back(receiverCall);
  methodCall.argNames.push_back(std::nullopt);

  std::unordered_map<std::string, primec::emitter::BindingInfo> localTypes;
  primec::emitter::BindingInfo receiverInfo;
  receiverInfo.typeName = "map";
  receiverInfo.typeTemplateArg = "i32, i32";
  localTypes.emplace("values", receiverInfo);

  std::unordered_map<std::string, const primec::Definition *> defMap;
  std::unordered_map<std::string, std::string> importAliases;
  std::unordered_map<std::string, std::string> structTypeMap;
  std::unordered_map<std::string, primec::emitter::ReturnKind> returnKinds;
  std::unordered_map<std::string, std::string> returnStructs;
  returnStructs.emplace("/std/collections/map/at", "/CanonicalMarker");

  std::string resolved;
  CHECK(primec::emitter::resolveMethodCallPath(
      methodCall, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
  CHECK(resolved == "/i32/tag");
}

TEST_CASE("C++ emitter helper keeps slash-path map access struct forwarding") {
  primec::Expr receiverCall;
  receiverCall.kind = primec::Expr::Kind::Call;
  receiverCall.isMethodCall = true;
  receiverCall.name = "/std/collections/map/at";

  primec::Expr receiverName;
  receiverName.kind = primec::Expr::Kind::Name;
  receiverName.name = "values";

  primec::Expr keyLiteral;
  keyLiteral.kind = primec::Expr::Kind::Literal;
  keyLiteral.intWidth = 32;
  keyLiteral.literalValue = 1;

  receiverCall.args.push_back(receiverName);
  receiverCall.args.push_back(keyLiteral);
  receiverCall.argNames.push_back(std::nullopt);
  receiverCall.argNames.push_back(std::nullopt);

  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.isMethodCall = true;
  methodCall.name = "tag";
  methodCall.args.push_back(receiverCall);
  methodCall.argNames.push_back(std::nullopt);

  std::unordered_map<std::string, primec::emitter::BindingInfo> localTypes;
  primec::emitter::BindingInfo receiverInfo;
  receiverInfo.typeName = "map";
  receiverInfo.typeTemplateArg = "i32, i32";
  localTypes.emplace("values", receiverInfo);

  std::unordered_map<std::string, const primec::Definition *> defMap;
  std::unordered_map<std::string, std::string> importAliases;
  std::unordered_map<std::string, std::string> structTypeMap;
  std::unordered_map<std::string, primec::emitter::ReturnKind> returnKinds;
  std::unordered_map<std::string, std::string> returnStructs;
  returnStructs.emplace("/std/collections/map/at", "/CanonicalMarker");

  std::string resolved;
  CHECK(primec::emitter::resolveMethodCallPath(
      methodCall, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
  CHECK_FALSE(resolved.empty());
}

TEST_CASE("C++ emitter helper normalizes slashless map import receiver metadata paths") {
  primec::Expr receiverCall;
  receiverCall.kind = primec::Expr::Kind::Call;
  receiverCall.name = "map_at_alias";

  primec::Expr receiverName;
  receiverName.kind = primec::Expr::Kind::Name;
  receiverName.name = "values";

  primec::Expr keyLiteral;
  keyLiteral.kind = primec::Expr::Kind::Literal;
  keyLiteral.intWidth = 32;
  keyLiteral.literalValue = 1;

  receiverCall.args.push_back(receiverName);
  receiverCall.args.push_back(keyLiteral);
  receiverCall.argNames.push_back(std::nullopt);
  receiverCall.argNames.push_back(std::nullopt);

  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.isMethodCall = true;
  methodCall.name = "tag";
  methodCall.args.push_back(receiverCall);
  methodCall.argNames.push_back(std::nullopt);

  std::unordered_map<std::string, primec::emitter::BindingInfo> localTypes;
  primec::emitter::BindingInfo receiverInfo;
  receiverInfo.typeName = "map";
  localTypes.emplace("values", receiverInfo);

  std::unordered_map<std::string, const primec::Definition *> defMap;
  std::unordered_map<std::string, std::string> importAliases;
  importAliases.emplace("map_at_alias", "std/collections/map/at");
  std::unordered_map<std::string, std::string> structTypeMap;
  structTypeMap.emplace("/std/collections/map/at", "Marker");
  std::unordered_map<std::string, primec::emitter::ReturnKind> returnKinds;
  std::unordered_map<std::string, std::string> returnStructs;

  std::string resolved;
  CHECK(primec::emitter::resolveMethodCallPath(
      methodCall, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
  CHECK(resolved == "/std/collections/map/at/tag");

  structTypeMap.clear();
  resolved.clear();
  CHECK_FALSE(primec::emitter::resolveMethodCallPath(
      methodCall, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
}

TEST_CASE("C++ emitter helper resolves map method via import-alias return metadata") {
  primec::Expr receiverCall;
  receiverCall.kind = primec::Expr::Kind::Call;
  receiverCall.name = "map_at_alias";

  primec::Expr receiverName;
  receiverName.kind = primec::Expr::Kind::Name;
  receiverName.name = "values";

  primec::Expr keyLiteral;
  keyLiteral.kind = primec::Expr::Kind::Literal;
  keyLiteral.intWidth = 32;
  keyLiteral.literalValue = 1;

  receiverCall.args.push_back(receiverName);
  receiverCall.args.push_back(keyLiteral);
  receiverCall.argNames.push_back(std::nullopt);
  receiverCall.argNames.push_back(std::nullopt);

  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.isMethodCall = true;
  methodCall.name = "tag";
  methodCall.args.push_back(receiverCall);
  methodCall.argNames.push_back(std::nullopt);

  std::unordered_map<std::string, primec::emitter::BindingInfo> localTypes;
  primec::emitter::BindingInfo receiverInfo;
  receiverInfo.typeName = "map";
  localTypes.emplace("values", receiverInfo);

  std::unordered_map<std::string, const primec::Definition *> defMap;
  std::unordered_map<std::string, std::string> importAliases;
  importAliases.emplace("map_at_alias", "std/collections/map/at");
  std::unordered_map<std::string, std::string> structTypeMap;
  std::unordered_map<std::string, primec::emitter::ReturnKind> returnKinds;
  std::unordered_map<std::string, std::string> returnStructs;
  returnStructs.emplace("/std/collections/map/at", "/CanonicalMarker");

  std::string resolved;
  CHECK(primec::emitter::resolveMethodCallPath(
      methodCall, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
  CHECK(resolved == "/CanonicalMarker/tag");
}

TEST_CASE("C++ emitter helper normalizes slashless map metadata lookup targets") {
  primec::Expr receiverCall;
  receiverCall.kind = primec::Expr::Kind::Call;
  receiverCall.name = "map_at_alias";

  primec::Expr receiverName;
  receiverName.kind = primec::Expr::Kind::Name;
  receiverName.name = "values";

  primec::Expr keyLiteral;
  keyLiteral.kind = primec::Expr::Kind::Literal;
  keyLiteral.intWidth = 32;
  keyLiteral.literalValue = 1;

  receiverCall.args.push_back(receiverName);
  receiverCall.args.push_back(keyLiteral);
  receiverCall.argNames.push_back(std::nullopt);
  receiverCall.argNames.push_back(std::nullopt);

  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.isMethodCall = true;
  methodCall.name = "tag";
  methodCall.args.push_back(receiverCall);
  methodCall.argNames.push_back(std::nullopt);

  std::unordered_map<std::string, primec::emitter::BindingInfo> localTypes;
  primec::emitter::BindingInfo receiverInfo;
  receiverInfo.typeName = "map";
  localTypes.emplace("values", receiverInfo);

  std::unordered_map<std::string, const primec::Definition *> defMap;
  std::unordered_map<std::string, std::string> importAliases;
  importAliases.emplace("map_at_alias", "std/collections/map/at");
  std::unordered_map<std::string, std::string> structTypeMap;
  structTypeMap.emplace("std/collections/map/at", "Marker");
  std::unordered_map<std::string, primec::emitter::ReturnKind> returnKinds;
  std::unordered_map<std::string, std::string> returnStructs;

  std::string resolved;
  CHECK(primec::emitter::resolveMethodCallPath(
      methodCall, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
  CHECK(resolved == "/std/collections/map/at/tag");
}

TEST_CASE("C++ emitter helper resolves direct slashless canonical map receiver metadata") {
  primec::Expr receiverCall;
  receiverCall.kind = primec::Expr::Kind::Call;
  receiverCall.name = "std/collections/map/at";

  primec::Expr receiverName;
  receiverName.kind = primec::Expr::Kind::Name;
  receiverName.name = "values";

  primec::Expr keyLiteral;
  keyLiteral.kind = primec::Expr::Kind::Literal;
  keyLiteral.intWidth = 32;
  keyLiteral.literalValue = 1;

  receiverCall.args.push_back(receiverName);
  receiverCall.args.push_back(keyLiteral);
  receiverCall.argNames.push_back(std::nullopt);
  receiverCall.argNames.push_back(std::nullopt);

  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.isMethodCall = true;
  methodCall.name = "tag";
  methodCall.args.push_back(receiverCall);
  methodCall.argNames.push_back(std::nullopt);

  std::unordered_map<std::string, primec::emitter::BindingInfo> localTypes;
  primec::emitter::BindingInfo receiverInfo;
  receiverInfo.typeName = "map";
  localTypes.emplace("values", receiverInfo);

  std::unordered_map<std::string, const primec::Definition *> defMap;
  std::unordered_map<std::string, std::string> importAliases;
  std::unordered_map<std::string, std::string> structTypeMap;
  structTypeMap.emplace("std/collections/map/at", "Marker");
  std::unordered_map<std::string, primec::emitter::ReturnKind> returnKinds;
  std::unordered_map<std::string, std::string> returnStructs;

  std::string resolved;
  CHECK(primec::emitter::resolveMethodCallPath(
      methodCall, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
  CHECK(resolved == "/std/collections/map/at/tag");
}

TEST_CASE("C++ emitter helper resolves direct slashless canonical map return metadata") {
  primec::Expr receiverCall;
  receiverCall.kind = primec::Expr::Kind::Call;
  receiverCall.name = "std/collections/map/at";

  primec::Expr receiverName;
  receiverName.kind = primec::Expr::Kind::Name;
  receiverName.name = "values";

  primec::Expr keyLiteral;
  keyLiteral.kind = primec::Expr::Kind::Literal;
  keyLiteral.intWidth = 32;
  keyLiteral.literalValue = 1;

  receiverCall.args.push_back(receiverName);
  receiverCall.args.push_back(keyLiteral);
  receiverCall.argNames.push_back(std::nullopt);
  receiverCall.argNames.push_back(std::nullopt);

  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.isMethodCall = true;
  methodCall.name = "tag";
  methodCall.args.push_back(receiverCall);
  methodCall.argNames.push_back(std::nullopt);

  std::unordered_map<std::string, primec::emitter::BindingInfo> localTypes;
  primec::emitter::BindingInfo receiverInfo;
  receiverInfo.typeName = "map";
  localTypes.emplace("values", receiverInfo);

  std::unordered_map<std::string, const primec::Definition *> defMap;
  std::unordered_map<std::string, std::string> importAliases;
  std::unordered_map<std::string, std::string> structTypeMap;
  std::unordered_map<std::string, primec::emitter::ReturnKind> returnKinds;
  std::unordered_map<std::string, std::string> returnStructs;
  returnStructs.emplace("std/collections/map/at", "/CanonicalMarker");

  std::string resolved;
  CHECK(primec::emitter::resolveMethodCallPath(
      methodCall, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
  CHECK(resolved == "/CanonicalMarker/tag");
}

TEST_CASE("C++ emitter helper keeps canonical map access method unresolved without metadata") {
  primec::Expr receiverCall;
  receiverCall.kind = primec::Expr::Kind::Call;
  receiverCall.name = "/std/collections/map/at";

  primec::Expr receiverName;
  receiverName.kind = primec::Expr::Kind::Name;
  receiverName.name = "values";

  primec::Expr keyLiteral;
  keyLiteral.kind = primec::Expr::Kind::Literal;
  keyLiteral.intWidth = 32;
  keyLiteral.literalValue = 1;

  receiverCall.args.push_back(receiverName);
  receiverCall.args.push_back(keyLiteral);
  receiverCall.argNames.push_back(std::nullopt);
  receiverCall.argNames.push_back(std::nullopt);

  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.isMethodCall = true;
  methodCall.name = "tag";
  methodCall.args.push_back(receiverCall);
  methodCall.argNames.push_back(std::nullopt);

  std::unordered_map<std::string, primec::emitter::BindingInfo> localTypes;
  primec::emitter::BindingInfo receiverInfo;
  receiverInfo.typeName = "map";
  localTypes.emplace("values", receiverInfo);

  std::unordered_map<std::string, const primec::Definition *> defMap;
  std::unordered_map<std::string, std::string> importAliases;
  std::unordered_map<std::string, std::string> structTypeMap;
  std::unordered_map<std::string, primec::emitter::ReturnKind> returnKinds;
  std::unordered_map<std::string, std::string> returnStructs;

  std::string resolved;
  CHECK_FALSE(primec::emitter::resolveMethodCallPath(
      methodCall, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
  CHECK(resolved.empty());
}

TEST_CASE("C++ emitter helper rejects removed full-path map method aliases") {
  primec::Expr call;
  call.kind = primec::Expr::Kind::Call;
  call.isMethodCall = true;

  primec::Expr receiver;
  receiver.kind = primec::Expr::Kind::Name;
  receiver.name = "values";
  call.args.push_back(receiver);
  call.argNames.push_back(std::nullopt);

  std::unordered_map<std::string, primec::emitter::BindingInfo> localTypes;
  primec::emitter::BindingInfo receiverInfo;
  receiverInfo.typeName = "map";
  localTypes.emplace("values", receiverInfo);

  std::unordered_map<std::string, const primec::Definition *> defMap;
  std::unordered_map<std::string, std::string> importAliases;
  std::unordered_map<std::string, std::string> structTypeMap;
  std::unordered_map<std::string, primec::emitter::ReturnKind> returnKinds;
  std::unordered_map<std::string, std::string> returnStructs;
  std::string resolved = "/stale/path";

  call.name = "/map/count";
  CHECK_FALSE(primec::emitter::resolveMethodCallPath(
      call, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
  CHECK(resolved.empty());

  resolved = "/stale/path";
  call.name = "/std/collections/map/at";
  CHECK_FALSE(primec::emitter::resolveMethodCallPath(
      call, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
  CHECK(resolved.empty());
}

TEST_CASE("C++ emitter helper rejects full-path map aliases without receiver type") {
  primec::Expr call;
  call.kind = primec::Expr::Kind::Call;
  call.isMethodCall = true;
  call.name = "/std/collections/map/count";

  primec::Expr receiver;
  receiver.kind = primec::Expr::Kind::Name;
  receiver.name = "values";
  call.args.push_back(receiver);
  call.argNames.push_back(std::nullopt);

  std::unordered_map<std::string, primec::emitter::BindingInfo> localTypes;
  std::unordered_map<std::string, const primec::Definition *> defMap;
  std::unordered_map<std::string, std::string> importAliases;
  std::unordered_map<std::string, std::string> structTypeMap;
  std::unordered_map<std::string, primec::emitter::ReturnKind> returnKinds;
  std::unordered_map<std::string, std::string> returnStructs;
  std::string resolved;

  CHECK_FALSE(primec::emitter::resolveMethodCallPath(
      call, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
}

TEST_CASE("C++ emitter helper normalizes slashless map type import alias method targets") {
  primec::Expr call;
  call.kind = primec::Expr::Kind::Call;
  call.isMethodCall = true;
  call.name = "tag";

  primec::Expr receiver;
  receiver.kind = primec::Expr::Kind::Name;
  receiver.name = "value";
  call.args.push_back(receiver);
  call.argNames.push_back(std::nullopt);

  std::unordered_map<std::string, primec::emitter::BindingInfo> localTypes;
  primec::emitter::BindingInfo receiverInfo;
  receiverInfo.typeName = "MapAtAlias";
  localTypes.emplace("value", receiverInfo);

  std::unordered_map<std::string, const primec::Definition *> defMap;
  std::unordered_map<std::string, std::string> importAliases = {
      {"MapAtAlias", "std/collections/map/at"},
      {"ThingAlias", "pkg/Thing"},
  };
  std::unordered_map<std::string, std::string> structTypeMap;
  std::unordered_map<std::string, primec::emitter::ReturnKind> returnKinds = {
      {"/std/collections/map/at", primec::emitter::ReturnKind::Int},
  };
  std::unordered_map<std::string, std::string> returnStructs;
  std::string resolved;

  CHECK(primec::emitter::resolveMethodCallPath(
      call, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
  CHECK(resolved == "/std/collections/map/at/tag");

  receiverInfo.typeName = "std/collections/map/at";
  localTypes["value"] = receiverInfo;
  CHECK(primec::emitter::resolveMethodCallPath(
      call, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
  CHECK(resolved == "/std/collections/map/at/tag");

  returnKinds.emplace("/map/at", primec::emitter::ReturnKind::Int);
  receiverInfo.typeName = "map/at";
  localTypes["value"] = receiverInfo;
  CHECK(primec::emitter::resolveMethodCallPath(
      call, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
  CHECK(resolved == "/map/at/tag");

  receiverInfo.typeName = "ThingAlias";
  localTypes["value"] = receiverInfo;
  CHECK(primec::emitter::resolveMethodCallPath(
      call, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
  CHECK(resolved == "pkg/Thing/tag");
}

TEST_CASE("C++ emitter helper rejects canonical vector alias access struct-return forwarding resolution") {
  primec::Expr receiverCall;
  receiverCall.kind = primec::Expr::Kind::Call;
  receiverCall.name = "/vector/at";

  primec::Expr receiverName;
  receiverName.kind = primec::Expr::Kind::Name;
  receiverName.name = "values";

  primec::Expr indexLiteral;
  indexLiteral.kind = primec::Expr::Kind::Literal;
  indexLiteral.intWidth = 32;
  indexLiteral.literalValue = 0;

  receiverCall.args.push_back(receiverName);
  receiverCall.args.push_back(indexLiteral);
  receiverCall.argNames.push_back(std::nullopt);
  receiverCall.argNames.push_back(std::nullopt);

  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.isMethodCall = true;
  methodCall.name = "tag";
  methodCall.args.push_back(receiverCall);
  methodCall.argNames.push_back(std::nullopt);

  std::unordered_map<std::string, primec::emitter::BindingInfo> localTypes;
  primec::emitter::BindingInfo receiverInfo;
  receiverInfo.typeName = "vector";
  receiverInfo.typeTemplateArg = "i32";
  localTypes.emplace("values", receiverInfo);

  std::unordered_map<std::string, const primec::Definition *> defMap;
  std::unordered_map<std::string, std::string> importAliases;
  std::unordered_map<std::string, std::string> structTypeMap;
  std::unordered_map<std::string, primec::emitter::ReturnKind> returnKinds;
  std::unordered_map<std::string, std::string> returnStructs;
  returnStructs.emplace("/std/collections/vector/at", "/Marker");

  std::string resolved;
  CHECK_FALSE(primec::emitter::resolveMethodCallPath(
      methodCall, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
  CHECK(resolved.empty());
}

TEST_CASE("C++ emitter helper keeps vector element method unresolved without canonical metadata") {
  primec::Expr receiverCall;
  receiverCall.kind = primec::Expr::Kind::Call;
  receiverCall.name = "/vector/at";

  primec::Expr receiverName;
  receiverName.kind = primec::Expr::Kind::Name;
  receiverName.name = "values";

  primec::Expr indexLiteral;
  indexLiteral.kind = primec::Expr::Kind::Literal;
  indexLiteral.intWidth = 32;
  indexLiteral.literalValue = 0;

  receiverCall.args.push_back(receiverName);
  receiverCall.args.push_back(indexLiteral);
  receiverCall.argNames.push_back(std::nullopt);
  receiverCall.argNames.push_back(std::nullopt);

  primec::Expr methodCall;
  methodCall.kind = primec::Expr::Kind::Call;
  methodCall.isMethodCall = true;
  methodCall.name = "tag";
  methodCall.args.push_back(receiverCall);
  methodCall.argNames.push_back(std::nullopt);

  std::unordered_map<std::string, primec::emitter::BindingInfo> localTypes;
  primec::emitter::BindingInfo receiverInfo;
  receiverInfo.typeName = "vector";
  receiverInfo.typeTemplateArg = "i32";
  localTypes.emplace("values", receiverInfo);

  std::unordered_map<std::string, const primec::Definition *> defMap;
  std::unordered_map<std::string, std::string> importAliases;
  std::unordered_map<std::string, std::string> structTypeMap;
  std::unordered_map<std::string, primec::emitter::ReturnKind> returnKinds;
  std::unordered_map<std::string, std::string> returnStructs;

  std::string resolved;
  CHECK_FALSE(primec::emitter::resolveMethodCallPath(
      methodCall, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, resolved));
  CHECK(resolved.empty());
}

TEST_CASE("rejects stdlib canonical vector helper method-precedence forwarding in C++ emitter") {
  const std::string source = R"(
[return<int>]
/std/collections/vector/count([vector<i32>] values, [bool] marker) {
  return(90i32)
}

[return<int>]
/std/collections/vector/at([vector<i32>] values, [bool] index) {
  return(40i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(values.count(true), values.at(true)))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_stdlib_vector_method_helper_precedence_reject.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_cpp_stdlib_vector_method_helper_precedence_reject_err.txt")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main > /dev/null 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("argument count mismatch for builtin count") != std::string::npos);
}

TEST_CASE("rejects templated stdlib canonical vector helper method template args in C++ emitter") {
  const std::string source = R"(
[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [bool] marker) {
  return(90i32)
}

[return<int>]
/std/collections/vector/at<T>([vector<T>] values, [i32] index) {
  return(plus(index, 40i32))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(values.count<i32>(true), values.at<i32>(2i32)))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_stdlib_vector_template_method_helper_precedence.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_cpp_stdlib_vector_template_method_helper_precedence_exe")
          .string();
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_cpp_stdlib_vector_template_method_helper_precedence_err.txt")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main > /dev/null 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("count does not accept template arguments") != std::string::npos);
}

TEST_CASE("rejects vector namespaced call aliases in C++ emitter") {
  const std::string source = R"(
[return<int>]
/std/collections/vector/count([vector<i32>] values) {
  return(90i32)
}

[return<int>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return(plus(index, 40i32))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(/vector/count(values), /vector/at(values, 2i32)))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_vector_namespaced_call_alias_canonical_precedence.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_cpp_vector_namespaced_call_alias_canonical_precedence_exe")
          .string();

  const std::string errPath = (std::filesystem::temp_directory_path() /
                               "primec_cpp_vector_namespaced_call_alias_canonical_precedence_err.txt")
                                  .string();
  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main > /dev/null 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("/vector/at") != std::string::npos);
}

TEST_CASE("rejects vector namespaced templated canonical helper alias call without alias definition in C++ emitter") {
  const std::string source = R"(
[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(/vector/count<i32>(values, true))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_vector_namespaced_templated_canonical_alias_call.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_cpp_vector_namespaced_templated_canonical_alias_call_exe")
          .string();
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_cpp_vector_namespaced_templated_canonical_alias_call_err.txt")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main > /dev/null 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("count does not accept template arguments") != std::string::npos);
}

TEST_CASE("rejects vector alias arity-mismatch compatibility template forwarding in C++ emitter") {
  const std::string source = R"(
[return<int>]
/vector/count([vector<i32>] values) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(/vector/count(values, true), values.count(true)))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_vector_alias_arity_mismatch_compatibility_template_forwarding_reject.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_cpp_vector_alias_arity_mismatch_compatibility_template_forwarding_reject_err.txt")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main > /dev/null 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("argument count mismatch") != std::string::npos);
}

TEST_CASE("rejects vector alias compatibility template forwarding on bool type mismatch in C++ emitter") {
  const std::string source = R"(
[return<int>]
/vector/count([vector<i32>] values, [i32] marker) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(/vector/count(values, true), values.count(true)))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_vector_alias_implicit_canonical_forwarding_bool_type_mismatch.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() /
       "primec_cpp_vector_alias_implicit_canonical_forwarding_bool_type_mismatch_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  expectCppVectorCountCompatibilityTypeMismatchReject(compileCmd);
}

TEST_CASE("rejects vector alias compatibility template forwarding on non-bool type mismatch in C++ emitter") {
  const std::string source = R"(
[return<int>]
/vector/count([vector<i32>] values, [string] marker) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [i32] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(/vector/count(values, 1i32), values.count(1i32)))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_vector_alias_implicit_canonical_forwarding_non_bool_type_mismatch.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() /
       "primec_cpp_vector_alias_implicit_canonical_forwarding_non_bool_type_mismatch_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  expectCppVectorCountCompatibilityTypeMismatchReject(compileCmd);
}

TEST_CASE("rejects vector alias compatibility template forwarding on struct type mismatch in C++ emitter") {
  const std::string source = R"(
MarkerA() {}
MarkerB() {}

[return<int>]
/vector/count([vector<i32>] values, [MarkerA] marker) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [MarkerB] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  [MarkerB] marker{MarkerB()}
  return(plus(/vector/count(values, marker), values.count(marker)))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_vector_alias_implicit_canonical_forwarding_struct_type_mismatch.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() /
       "primec_cpp_vector_alias_implicit_canonical_forwarding_struct_type_mismatch_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  expectCppVectorCountCompatibilityTypeMismatchReject(compileCmd);
}

TEST_CASE("rejects vector alias compatibility template forwarding on constructor temporary struct mismatch in C++ emitter") {
  const std::string source = R"(
MarkerA() {}
MarkerB() {}

[return<int>]
/vector/count([vector<i32>] values, [MarkerA] marker) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [MarkerB] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(/vector/count(values, MarkerB()), values.count(MarkerB())))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_vector_alias_canonical_forwarding_constructor_struct_mismatch.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() /
       "primec_cpp_vector_alias_canonical_forwarding_constructor_struct_mismatch_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  expectCppVectorCountCompatibilityTypeMismatchReject(compileCmd);
}

TEST_CASE("rejects vector alias compatibility template forwarding on method-call temporary struct mismatch in C++ emitter") {
  const std::string source = R"(
MarkerA() {}
MarkerB() {}
Holder() {}

[return<MarkerB>]
/Holder/fetch([Holder] self) {
  return(MarkerB())
}

[return<int>]
/vector/count([vector<i32>] values, [MarkerA] marker) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [MarkerB] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  [Holder] holder{Holder()}
  return(plus(/vector/count(values, holder.fetch()), values.count(holder.fetch())))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_vector_alias_canonical_forwarding_method_struct_mismatch.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() /
       "primec_cpp_vector_alias_canonical_forwarding_method_struct_mismatch_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  expectCppVectorCountCompatibilityTypeMismatchReject(compileCmd);
}

TEST_CASE("rejects vector alias compatibility template forwarding on chained method-call temporary struct mismatch in C++ emitter") {
  const std::string source = R"(
MarkerA() {}
MarkerB() {}
Middle() {}
Holder() {}

[return<Middle>]
/Holder/first([Holder] self) {
  return(Middle())
}

[return<MarkerB>]
/Middle/next([Middle] self) {
  return(MarkerB())
}

[return<int>]
/vector/count([vector<i32>] values, [MarkerA] marker) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [MarkerB] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  [Holder] holder{Holder()}
  return(plus(/vector/count(values, holder.first().next()),
              values.count(holder.first().next())))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_vector_alias_canonical_forwarding_chained_method_struct_mismatch.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() /
       "primec_cpp_vector_alias_canonical_forwarding_chained_method_struct_mismatch_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  expectCppVectorCountCompatibilityTypeMismatchReject(compileCmd);
}

TEST_CASE("rejects vector alias compatibility template forwarding on array envelope element mismatch in C++ emitter") {
  const std::string source = R"(
[return<int>]
/vector/count([vector<i32>] values, [array<i32>] marker) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [array<i64>] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  [array<i64>] marker{array<i64>(1i64)}
  return(plus(/vector/count(values, marker), values.count(marker)))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_vector_alias_canonical_forwarding_array_envelope_mismatch.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() /
       "primec_cpp_vector_alias_canonical_forwarding_array_envelope_mismatch_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  expectCppVectorCountCompatibilityTypeMismatchReject(compileCmd);
}

TEST_CASE("rejects vector alias compatibility template forwarding on map envelope mismatch in C++ emitter") {
  const std::string source = R"(
[return<int>]
/vector/count([vector<i32>] values, [map<i32, string>] marker) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [map<i32, i64>] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  [map<i32, i64>] marker{map<i32, i64>(1i32, 2i64)}
  return(plus(/vector/count(values, marker), values.count(marker)))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_vector_alias_canonical_forwarding_map_envelope_mismatch.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() /
       "primec_cpp_vector_alias_canonical_forwarding_map_envelope_mismatch_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  expectCppVectorCountCompatibilityTypeMismatchReject(compileCmd);
}

TEST_CASE("rejects vector alias compatibility template forwarding on map envelope mismatch from call return in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<map<i32, i64>>]
makeMarker() {
  return(map<i32, i64>(1i32, 2i64))
}

[return<int>]
/vector/count([vector<i32>] values, [map<i32, string>] marker) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [map<i32, i64>] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(/vector/count(values, makeMarker()), values.count(makeMarker())))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_vector_alias_canonical_forwarding_map_call_return_mismatch.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() /
       "primec_cpp_vector_alias_canonical_forwarding_map_call_return_mismatch_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  expectCppVectorCountCompatibilityTypeMismatchReject(compileCmd);
}

TEST_CASE("rejects vector alias compatibility template forwarding on primitive mismatch from call return in C++ emitter") {
  const std::string source = R"(
[return<i32>]
makeMarker() {
  return(1i32)
}

[return<int>]
/vector/count([vector<i32>] values, [string] marker) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [i32] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(/vector/count(values, makeMarker()), values.count(makeMarker())))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_vector_alias_canonical_forwarding_primitive_call_return_mismatch.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() /
       "primec_cpp_vector_alias_canonical_forwarding_primitive_call_return_mismatch_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  expectCppVectorCountCompatibilityTypeMismatchReject(compileCmd);
}

TEST_CASE("rejects vector alias compatibility template forwarding when unknown expected meets primitive call return in C++ emitter") {
  const std::string source = R"(
Marker() {}

[return<i32>]
makeMarker() {
  return(1i32)
}

[return<int>]
/vector/count([vector<i32>] values, [Marker] marker) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [i32] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(/vector/count(values, makeMarker()), values.count(makeMarker())))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_vector_alias_canonical_forwarding_unknown_expected_primitive_call_return.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() /
       "primec_cpp_vector_alias_canonical_forwarding_unknown_expected_primitive_call_return_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  expectCppVectorCountCompatibilityTypeMismatchReject(compileCmd);
}

TEST_CASE("rejects vector alias compatibility template forwarding when unknown expected meets primitive binding in C++ emitter") {
  const std::string source = R"(
Marker() {}

[return<int>]
/vector/count([vector<i32>] values, [Marker] marker) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [i32] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  [i32] marker{1i32}
  return(plus(/vector/count(values, marker), values.count(marker)))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_vector_alias_canonical_forwarding_unknown_expected_primitive_binding.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() /
       "primec_cpp_vector_alias_canonical_forwarding_unknown_expected_primitive_binding_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  expectCppVectorCountCompatibilityTypeMismatchReject(compileCmd);
}

TEST_CASE("rejects vector alias compatibility template forwarding when unknown expected meets vector envelope binding in C++ emitter") {
  const std::string source = R"(
Marker() {}

[return<int>]
/vector/count([vector<i32>] values, [Marker] marker) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [vector<i32>] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  [vector<i32>] marker{vector<i32>(1i32)}
  return(plus(/vector/count(values, marker), values.count(marker)))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_vector_alias_canonical_forwarding_unknown_expected_vector_envelope_binding.prime",
                source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() /
       "primec_cpp_vector_alias_canonical_forwarding_unknown_expected_vector_envelope_binding_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  expectCppVectorCountCompatibilityTypeMismatchReject(compileCmd);
}

TEST_CASE("rejects vector helper method expression legacy alias forwarding in C++ emitter") {
  const std::string source = R"(
[return<int>]
/std/collections/vector/push([vector<i32> mut] values, [i32] value) {
  return(plus(count(values), value))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  return(plus(/vector/push(values, 2i32), values.push(2i32)))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_vector_helper_method_expression_canonical_stdlib_forwarding.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_cpp_vector_helper_method_expression_canonical_stdlib_forwarding_err.txt")
          .string();
  const std::string exePath =
      (std::filesystem::temp_directory_path() /
       "primec_cpp_vector_helper_method_expression_canonical_stdlib_forwarding_exe")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main > /dev/null 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("push is only supported as a statement") != std::string::npos);
}

TEST_CASE("rejects vector alias named-argument compatibility template forwarding in C++ emitter") {
  const std::string source = R"(
[return<int>]
/vector/count([vector<i32>] values) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(/vector/count([values] values, [marker] true),
              values.count([marker] true)))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_vector_alias_named_argument_compatibility_template_forwarding_reject.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_cpp_vector_alias_named_argument_compatibility_template_forwarding_reject_err.txt")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main > /dev/null 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("unknown named argument: marker") != std::string::npos);
}

TEST_CASE("rejects wrapper temporary templated vector method canonical forwarding in C++ emitter") {
  const std::string source = R"(
[return<int>]
/vector/count([vector<i32>] values) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<vector<i32>>]
wrapValues() {
  return(vector<i32>(5i32, 6i32, 7i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(wrapValues().count<i32>(true))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_wrapper_temp_templated_vector_method_canonical_forwarding.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_cpp_wrapper_temp_templated_vector_method_canonical_forwarding_err.txt")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main > /dev/null 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("template arguments are only supported on templated definitions: /vector/count") !=
        std::string::npos);
}

TEST_CASE("rejects array alias templated forwarding to canonical vector helper in C++ emitter") {
  const std::string source = R"(
[return<int>]
/array/count([vector<i32>] values) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(/array/count<i32>(values, true))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_array_alias_templated_vector_forwarding_rejected.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_cpp_array_alias_templated_vector_forwarding_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects stdlib templated vector count fallback to array alias in C++ emitter") {
  const std::string source = R"(
[return<int>]
/vector/count([vector<i32>] values) {
  return(7i32)
}

[return<int>]
/array/count<T>([vector<T>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(/std/collections/vector/count<i32>(values, true))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_stdlib_templated_vector_call_array_fallback_rejected.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_cpp_stdlib_templated_vector_call_array_fallback_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects vector alias templated forwarding past non-templated compatibility helper in C++ emitter") {
  const std::string source = R"(
[return<int>]
/vector/count([vector<i32>] values) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(/vector/count<i32>(values, true), values.count<i32>(true)))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_vector_templated_alias_forwarding_non_template_compat.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_cpp_vector_templated_alias_forwarding_non_template_compat_err.txt")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main > /dev/null 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("template arguments are only supported on templated definitions: /vector/count") !=
        std::string::npos);
}

TEST_CASE("rejects vector namespaced count capacity access aliases in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(4i32, 5i32)}
  [i32] c{/vector/count(values)}
  [i32] k{/vector/capacity(values)}
  [i32] first{/vector/at(values, 0i32)}
  [i32] second{/vector/at_unsafe(values, 1i32)}
  return(plus(plus(c, k), plus(first, second)))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_vector_namespaced_count_access_aliases.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_cpp_vector_namespaced_count_access_aliases_err.txt")
          .string();
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_cpp_vector_namespaced_count_access_aliases_exe").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main > /dev/null 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("/vector/at") != std::string::npos);
}

TEST_CASE("C++ emitter emits builtin statement for stdlib namespaced vector mutator") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  /std/collections/vector/push(values, 2i32)
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_stdlib_namespaced_vector_mutator_stmt.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() /
                               "primec_cpp_stdlib_namespaced_vector_mutator_stmt.cpp")
                                  .string();

  const std::string compileCmd = "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("static int64_t ps_fn_0(") != std::string::npos);
  CHECK(output.find("return ps_fn_0(stack, sp, heapSlots, argc, argv);") != std::string::npos);
}

TEST_CASE("C++ emitter accepts vector namespaced mutator alias statement") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  /vector/push(values, 2i32)
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_vector_namespaced_mutator_stmt.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_cpp_vector_namespaced_mutator_stmt.cpp").string();
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_cpp_vector_namespaced_mutator_stmt_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main > /dev/null 2> " + errPath;
  CHECK(runCommand(compileCmd) == 0);
  CHECK_FALSE(readFile(outPath).empty());
}

TEST_CASE("C++ emitter keeps stdlib namespaced vector access builtin fallback") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(4i32, 5i32)}
  return(plus(/std/collections/vector/at(values, 0i32),
              /std/collections/vector/at_unsafe(values, 1i32)))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_stdlib_namespaced_vector_access_fallback.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() /
                               "primec_cpp_stdlib_namespaced_vector_access_fallback.cpp")
                                  .string();

  const std::string compileCmd = "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("static int64_t ps_fn_0(") != std::string::npos);
  CHECK(output.find("return ps_fn_0(stack, sp, heapSlots, argc, argv);") != std::string::npos);
}

TEST_CASE("C++ emitter routes stdlib namespaced vector at map target to map helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(/std/collections/vector/at(values, 1i32))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_stdlib_namespaced_vector_at_map_helper.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() /
                               "primec_cpp_stdlib_namespaced_vector_at_map_helper.cpp")
                                  .string();

  const std::string compileCmd = "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("static int64_t ps_fn_0(") != std::string::npos);
  CHECK(output.find("return ps_fn_0(stack, sp, heapSlots, argc, argv);") != std::string::npos);
}

TEST_CASE("C++ emitter compiles stdlib namespaced map access and count helpers") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32, 2i32, 5i32)}
  [i32] c{/std/collections/map/count(values)}
  [i32] first{/std/collections/map/at(values, 1i32)}
  [i32] second{/std/collections/map/at_unsafe(values, 2i32)}
  return(plus(c, plus(first, second)))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_stdlib_namespaced_map_access_count.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_cpp_stdlib_namespaced_map_access_count_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 11);
}

TEST_CASE("C++ emitter compiles stdlib namespaced map helpers on canonical map references") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [/std/collections/map<i32, i32>] values{map<i32, i32>(1i32, 4i32, 2i32, 5i32)}
  [Reference</std/collections/map<i32, i32>>] ref{location(values)}
  [i32] c{/std/collections/map/count(ref)}
  [i32] first{/std/collections/map/at(ref, 1i32)}
  [i32] second{/std/collections/map/at_unsafe(ref, 2i32)}
  return(plus(c, plus(first, second)))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_stdlib_map_reference_helpers.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_cpp_stdlib_map_reference_helpers_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 11);
}

TEST_CASE("C++ emitter compiles map method sugar on wrapper-returned canonical map references") {
  const std::string source = R"(
[return<Reference</std/collections/map<i32, i32>>>]
borrowMap([Reference</std/collections/map<i32, i32>>] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [/std/collections/map<i32, i32>] values{map<i32, i32>(1i32, 4i32, 2i32, 5i32)}
  [Reference</std/collections/map<i32, i32>>] ref{borrowMap(location(values))}
  [i32] c{ref.count()}
  [i32] first{ref.at(1i32)}
  [i32] second{ref.at_unsafe(2i32)}
  return(plus(c, plus(first, second)))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_wrapper_map_reference_method_sugar.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() /
       "primec_cpp_wrapper_map_reference_method_sugar_exe")
          .string();
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_cpp_wrapper_map_reference_method_sugar_err.txt")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("native backend only supports returning array values") != std::string::npos);
}

TEST_CASE("C++ emitter keeps canonical diagnostics on wrapper-returned canonical map reference method sugar") {
  const std::string source = R"(
[return<Reference</std/collections/map<i32, i32>>>]
borrowMap([Reference</std/collections/map<i32, i32>>] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [/std/collections/map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  [Reference</std/collections/map<i32, i32>>] ref{borrowMap(location(values))}
  return(ref.at(true))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_wrapper_map_reference_method_sugar_diag.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_cpp_wrapper_map_reference_method_sugar_diag.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("at requires map key type i32") != std::string::npos);
}

TEST_CASE("C++ emitter treats canonical map reference string access as string receiver") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [/std/collections/map<i32, string>] values{map<i32, string>(1i32, "hello"utf8)}
  [Reference</std/collections/map<i32, string>>] ref{location(values)}
  return(ref[1i32].count())
}
)";
  const std::string srcPath = writeTemp("compile_cpp_canonical_map_reference_string_receiver.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() /
       "primec_cpp_canonical_map_reference_string_receiver_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 5);
}

TEST_CASE("C++ emitter keeps non-string diagnostics on canonical map reference access receivers") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [/std/collections/map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  [Reference</std/collections/map<i32, i32>>] ref{location(values)}
  return(ref[1i32].count())
}
)";
  const std::string srcPath = writeTemp("compile_cpp_canonical_map_reference_string_receiver_diag.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_cpp_canonical_map_reference_string_receiver_diag.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /i32/count") != std::string::npos);
}

TEST_CASE("compiles and runs map namespaced count compatibility alias in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/map/count([map<i32, i32>] values) {
  return(17i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(/map/count(values))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_map_namespaced_count_compatibility_alias_reject.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_cpp_map_namespaced_count_compatibility_alias_exe").string();
  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 17);
}

TEST_CASE("compiles and runs stdlib namespaced map count fallback to map alias in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 4i32))
}

[return<int>]
/map/count([map<i32, i32>] values) {
  return(77i32)
}

[effects(heap_alloc), return<int>]
main() {
  return(/std/collections/map/count(wrapMap()))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_stdlib_namespaced_map_count_alias_fallback.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_cpp_stdlib_namespaced_map_count_alias_fallback_exe")
          .string();
  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 77);
}

TEST_CASE("rejects stdlib namespaced map count alias fallback arity mismatch in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 4i32))
}

[return<int>]
/map/count([map<i32, i32>] values, [bool] marker) {
  return(77i32)
}

[effects(heap_alloc), return<int>]
main() {
  return(/std/collections/map/count(wrapMap()))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_stdlib_namespaced_map_count_alias_fallback_arity_reject.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_cpp_stdlib_namespaced_map_count_alias_fallback_arity_reject.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("argument count mismatch for /map/count") != std::string::npos);
}

TEST_CASE("compiles and runs stdlib namespaced map count templated alias fallback in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 4i32))
}

[return<int>]
/map/count<K, V>([map<K, V>] values, [bool] marker) {
  return(79i32)
}

[effects(heap_alloc), return<int>]
main() {
  return(/std/collections/map/count(wrapMap(), true))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_stdlib_namespaced_map_count_templated_alias_fallback.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() /
       "primec_cpp_stdlib_namespaced_map_count_templated_alias_fallback_exe")
          .string();
  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 79);
}

TEST_CASE("rejects stdlib namespaced map count templated alias fallback mismatch in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 4i32))
}

[return<int>]
/map/count<K, V>([map<K, V>] values, [bool] marker) {
  return(79i32)
}

[effects(heap_alloc), return<int>]
main() {
  return(/std/collections/map/count(wrapMap(), 1i32))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_stdlib_namespaced_map_count_templated_alias_fallback_mismatch.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_cpp_stdlib_namespaced_map_count_templated_alias_fallback_mismatch.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("argument type mismatch for /map/count") != std::string::npos);
  CHECK(diagnostics.find("parameter marker") != std::string::npos);
}

TEST_CASE("compiles and runs map namespaced at compatibility alias in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/map/at([map<i32, i32>] values, [i32] index) {
  return(17i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(/map/at(values, 1i32))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_map_namespaced_at_compatibility_alias_reject.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_cpp_map_namespaced_at_compatibility_alias_exe").string();
  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 17);
}

TEST_CASE("compiles and runs map unnamespaced count builtin fallback in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/map/count([map<i32, i32>] values) {
  return(17i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(count(values))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_map_unnamespaced_count_builtin_fallback_reject.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_cpp_map_unnamespaced_count_builtin_fallback_exe").string();
  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 17);
}

TEST_CASE("compiles and runs map unnamespaced count builtin fallback without canonical helper in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(count(values))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_map_unnamespaced_count_builtin_fallback_no_canonical_reject.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_cpp_map_unnamespaced_count_builtin_fallback_no_canonical_exe")
          .string();
  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs map unnamespaced at builtin fallback in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/map/at([map<i32, i32>] values, [i32] index) {
  return(17i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(at(values, 1i32))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_map_unnamespaced_at_builtin_fallback_reject.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_cpp_map_unnamespaced_at_builtin_fallback_exe").string();
  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 17);
}

TEST_CASE("compiles and runs map unnamespaced at_unsafe builtin fallback in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/map/at_unsafe([map<i32, i32>] values, [i32] index) {
  return(17i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(at_unsafe(values, 1i32))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_map_unnamespaced_at_unsafe_builtin_fallback_reject.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_cpp_map_unnamespaced_at_unsafe_builtin_fallback_exe").string();
  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 17);
}

TEST_CASE("compiles and runs map unnamespaced at builtin fallback without canonical helper in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(at(values, 1i32))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_map_unnamespaced_at_builtin_fallback_no_canonical_reject.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_cpp_map_unnamespaced_at_builtin_fallback_no_canonical_exe")
          .string();
  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 4);
}

TEST_CASE("compiles and runs map unnamespaced at_unsafe builtin fallback without canonical helper in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(at_unsafe(values, 1i32))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_map_unnamespaced_at_unsafe_builtin_fallback_no_canonical_reject.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_cpp_map_unnamespaced_at_unsafe_builtin_fallback_no_canonical_exe")
          .string();
  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 4);
}

TEST_CASE("rejects map namespaced count method compatibility alias in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/map/count([map<i32, i32>] values) {
  return(17i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(values./map/count())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_map_namespaced_count_method_compatibility_alias_reject.prime", source);
  const std::string errPath = (std::filesystem::temp_directory_path() /
                               "primec_cpp_map_namespaced_count_method_compatibility_alias_reject.err")
                                  .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("Semantic error") != std::string::npos);
}

TEST_CASE("rejects map namespaced at method compatibility alias in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/map/at([map<i32, i32>] values, [i32] index) {
  return(17i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(values./map/at(1i32))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_map_namespaced_at_method_compatibility_alias_reject.prime",
                                        source);
  const std::string errPath = (std::filesystem::temp_directory_path() /
                               "primec_cpp_map_namespaced_at_method_compatibility_alias_reject.err")
                                  .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("Semantic error") != std::string::npos);
}

TEST_CASE("C++ emitter resolves stdlib canonical map count helper in method-call sugar") {
  const std::string source = R"(
[return<int>]
/std/collections/map/count([map<i32, i32>] values, [bool] marker) {
  return(91i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(values.count(true))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_stdlib_canonical_map_count_method_sugar.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_cpp_stdlib_canonical_map_count_method_sugar_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 91);
}

TEST_CASE("C++ emitter resolves canonical map count helper on wrapper slash return method sugar") {
  const std::string source = R"(
[return<int>]
/std/collections/map/count([map<i32, i32>] values, [bool] marker) {
  return(92i32)
}

[effects(heap_alloc), return</std/collections/map<i32, i32>>]
wrapValues() {
  return(map<i32, i32>(1i32, 4i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(wrapValues().count(true))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_stdlib_canonical_map_count_wrapper_slash_return_method_sugar.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() /
       "primec_cpp_stdlib_canonical_map_count_wrapper_slash_return_method_sugar_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 92);
}

TEST_CASE("C++ emitter keeps canonical map count diagnostics on wrapper slash return method sugar") {
  const std::string source = R"(
[return<int>]
/std/collections/map/count([map<i32, i32>] values, [bool] marker) {
  return(92i32)
}

[effects(heap_alloc), return</std/collections/map<i32, i32>>]
wrapValues() {
  return(map<i32, i32>(1i32, 4i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(wrapValues().count(1i32))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_stdlib_canonical_map_count_wrapper_slash_return_method_sugar_diag.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_cpp_stdlib_canonical_map_count_wrapper_slash_return_method_sugar_diag.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("argument type mismatch for /std/collections/map/count parameter marker") !=
        std::string::npos);
}

TEST_CASE("C++ emitter keeps canonical map return arity diagnostics for stdlib envelopes") {
  const std::string source = R"(
[return</std/collections/map<string>>]
wrapMap([string] key, [i32] value) {
  return(map<string, i32>(key, value))
}

[effects(heap_alloc), return<int>]
main() {
  [map<string, i32>] values{wrapMap("only"raw_utf8, 3i32)}
  return(count(values))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_stdlib_canonical_map_return_arity_diag.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_cpp_stdlib_canonical_map_return_arity_diag.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("template arguments are only supported on templated definitions: /std/collections/map") !=
        std::string::npos);
}

TEST_CASE("C++ emitter supports explicit canonical map typed bindings for builtin helpers") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [/std/collections/map<i32, i32>] values{map<i32, i32>(1i32, 4i32, 2i32, 5i32)}
  return(plus(plus(count(values), values.at(1i32)), values.at_unsafe(2i32)))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_explicit_canonical_map_typed_binding_builtin_helpers.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() /
       "primec_cpp_explicit_canonical_map_typed_binding_builtin_helpers_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 11);
}

TEST_CASE("C++ emitter keeps builtin map diagnostics on explicit canonical typed bindings") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [/std/collections/map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(values.at(true))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_explicit_canonical_map_typed_binding_builtin_helpers_diag.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_cpp_explicit_canonical_map_typed_binding_builtin_helpers_diag.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("at requires map key type i32") != std::string::npos);
}

TEST_CASE("C++ emitter keeps canonical stdlib namespaced map access helpers") {
  const std::string source = R"(
[effects(heap_alloc), return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 4i32))
}

[return<int>]
/map/at([map<i32, i32>] values, [i32] key) {
  return(41i32)
}

[return<int>]
/std/collections/map/at([map<i32, i32>] values, [i32] key) {
  return(7i32)
}

[return<int>]
/map/at_unsafe([map<i32, i32>] values, [i32] key) {
  return(42i32)
}

[return<int>]
/std/collections/map/at_unsafe([map<i32, i32>] values, [i32] key) {
  return(8i32)
}

[effects(heap_alloc), return<int>]
main() {
  return(plus(/std/collections/map/at(wrapMap(), 1i32),
              /std/collections/map/at_unsafe(wrapMap(), 1i32)))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_stdlib_namespaced_map_access_canonical_precedence.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() /
       "primec_cpp_stdlib_namespaced_map_access_canonical_precedence_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 15);
}

TEST_CASE("C++ emitter keeps canonical diagnostics for stdlib namespaced map at") {
  const std::string source = R"(
[effects(heap_alloc), return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 4i32))
}

[return<int>]
/map/at([map<i32, i32>] values, [i32] key) {
  return(41i32)
}

[return<int>]
/std/collections/map/at([map<i32, i32>] values, [bool] key) {
  return(7i32)
}

[effects(heap_alloc), return<int>]
main() {
  return(/std/collections/map/at(wrapMap(), 1i32))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_stdlib_namespaced_map_at_canonical_precedence_diag.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_cpp_stdlib_namespaced_map_at_canonical_precedence_diag.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("argument type mismatch for /std/collections/map/at parameter key") !=
        std::string::npos);
}

TEST_CASE("C++ emitter keeps canonical stdlib namespaced map count helpers") {
  const std::string source = R"(
[effects(heap_alloc), return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 4i32))
}

[return<int>]
/map/count([map<i32, i32>] values) {
  return(41i32)
}

[return<int>]
/std/collections/map/count([map<i32, i32>] values) {
  return(7i32)
}

[effects(heap_alloc), return<int>]
main() {
  return(/std/collections/map/count(wrapMap()))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_stdlib_namespaced_map_count_canonical_precedence.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() /
       "primec_cpp_stdlib_namespaced_map_count_canonical_precedence_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("C++ emitter keeps canonical diagnostics for stdlib namespaced map count") {
  const std::string source = R"(
[effects(heap_alloc), return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 4i32))
}

[return<int>]
/map/count([map<i32, i32>] values) {
  return(41i32)
}

[return<int>]
/std/collections/map/count([map<i32, i32>] values, [bool] marker) {
  return(7i32)
}

[effects(heap_alloc), return<int>]
main() {
  return(/std/collections/map/count(wrapMap()))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_stdlib_namespaced_map_count_canonical_precedence_diag.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_cpp_stdlib_namespaced_map_count_canonical_precedence_diag.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("argument count mismatch for /std/collections/map/count") !=
        std::string::npos);
}

TEST_CASE("C++ emitter keeps canonical direct-call map access string receivers") {
  const std::string source = R"(
[return<int>]
/map/at([map<i32, string>] values, [i32] key) {
  return(41i32)
}

[return<string>]
/std/collections/map/at([map<i32, string>] values, [i32] key) {
  return("hello"utf8)
}

[return<int>]
/map/at_unsafe([map<i32, string>] values, [i32] key) {
  return(42i32)
}

[return<string>]
/std/collections/map/at_unsafe([map<i32, string>] values, [i32] key) {
  return("hello"utf8)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, string>] values{map<i32, string>(1i32, "value"utf8)}
  return(plus(/std/collections/map/at(values, 1i32).count(),
              /std/collections/map/at_unsafe(values, 1i32).count()))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_stdlib_namespaced_map_access_direct_call_string_receiver.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() /
       "primec_cpp_stdlib_namespaced_map_access_direct_call_string_receiver_exe")
          .string();
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_cpp_stdlib_namespaced_map_access_direct_call_string_receiver.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("native backend only supports numeric/bool, string, or struct parameters") !=
        std::string::npos);
}

TEST_CASE("C++ emitter keeps canonical diagnostics on direct-call map access receivers") {
  const std::string source = R"(
[return<string>]
/map/at([map<i32, string>] values, [i32] key) {
  return("hello"utf8)
}

[return<int>]
/std/collections/map/at([map<i32, string>] values, [i32] key) {
  return(41i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, string>] values{map<i32, string>(1i32, "value"utf8)}
  return(/std/collections/map/at(values, 1i32).count())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_stdlib_namespaced_map_access_direct_call_string_receiver_diag.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_cpp_stdlib_namespaced_map_access_direct_call_string_receiver_diag.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /string/count") != std::string::npos);
}

TEST_CASE("C++ emitter keeps canonical direct-call map count string receivers") {
  const std::string source = R"(
[return<int>]
/map/count([map<i32, string>] values) {
  return(41i32)
}

[return<string>]
/std/collections/map/count([map<i32, string>] values) {
  return("hello"utf8)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, string>] values{map<i32, string>(1i32, "value"utf8)}
  return(/std/collections/map/count(values).count())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_stdlib_namespaced_map_count_direct_call_string_receiver.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() /
       "primec_cpp_stdlib_namespaced_map_count_direct_call_string_receiver_exe")
          .string();
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_cpp_stdlib_namespaced_map_count_direct_call_string_receiver.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("native backend only supports numeric/bool, string, or struct parameters") !=
        std::string::npos);
}

TEST_CASE("C++ emitter keeps canonical diagnostics on direct-call map count receivers") {
  const std::string source = R"(
[return<string>]
/map/count([map<i32, string>] values) {
  return("hello"utf8)
}

[return<int>]
/std/collections/map/count([map<i32, string>] values) {
  return(41i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, string>] values{map<i32, string>(1i32, "value"utf8)}
  return(/std/collections/map/count(values).count())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_stdlib_namespaced_map_count_direct_call_string_receiver_diag.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_cpp_stdlib_namespaced_map_count_direct_call_string_receiver_diag.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /string/count") != std::string::npos);
}

TEST_CASE("rejects stdlib namespaced vector capacity on map target in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(/std/collections/vector/capacity(values))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_stdlib_namespaced_vector_capacity_map_reject.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_cpp_stdlib_namespaced_vector_capacity_map_reject.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("capacity requires vector target") != std::string::npos);
}

TEST_CASE("compiles and runs user wrapper temporary count capacity shadow precedence in C++ emitter") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32))
}

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(3i32, 4i32))
}

[return<int>]
/map/count([map<i32, i32>] values) {
  return(77i32)
}

[effects(heap_alloc), return<int>]
/vector/count([vector<i32>] values) {
  return(78i32)
}

[effects(heap_alloc), return<int>]
/vector/capacity([vector<i32>] values) {
  return(79i32)
}

[effects(heap_alloc), return<int>]
main() {
  return(plus(plus(count(wrapMap()), wrapMap().count()),
              plus(plus(count(wrapVector()), wrapVector().count()),
                   plus(capacity(wrapVector()), wrapVector().capacity()))))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_user_wrapper_temp_count_capacity_shadow_precedence.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_cpp_user_wrapper_temp_count_capacity_shadow_precedence_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == expectedProcessExitCode(468));
}

TEST_CASE("rejects user wrapper temporary count capacity shadow value mismatch in C++ emitter") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32))
}

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(3i32, 4i32))
}

[return<bool>]
/map/count([map<i32, i32>] values) {
  return(true)
}

[effects(heap_alloc), return<bool>]
/vector/count([vector<i32>] values) {
  return(false)
}

[effects(heap_alloc), return<bool>]
/vector/capacity([vector<i32>] values) {
  return(true)
}

[effects(heap_alloc), return<int>]
main() {
  [i32] mapCall{count(wrapMap())}
  [i32] mapMethod{wrapMap().count()}
  [i32] vectorCountCall{count(wrapVector())}
  [i32] vectorCountMethod{wrapVector().count()}
  [i32] vectorCapacityCall{capacity(wrapVector())}
  [i32] vectorCapacityMethod{wrapVector().capacity()}
  return(0i32)
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_user_wrapper_temp_count_capacity_shadow_value_mismatch.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_cpp_user_wrapper_temp_count_capacity_shadow_value_mismatch_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("C++ emitter keeps wrapper count capacity builtin fallback") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32, 3i32, 4i32))
}

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(5i32, 6i32, 7i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(plus(plus(count(wrapMap()), wrapMap().count()),
              plus(capacity(wrapVector()), wrapVector().capacity())))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_wrapper_count_capacity_builtin_fallback.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_cpp_wrapper_count_capacity_builtin_fallback.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects namespaced wrapper count capacity compatibility fallback in C++ emitter") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32, 3i32, 4i32))
}

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(5i32, 6i32, 7i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(plus(/std/collections/vector/count(wrapMap()),
              /vector/capacity(wrapVector())))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_namespaced_wrapper_count_capacity_compatibility_fallback_reject.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() /
                               "primec_cpp_namespaced_wrapper_count_capacity_compatibility_fallback_reject_out.txt")
                                  .string();

  const std::string compileCmd =
      "./primec --emit=cpp " + srcPath + " -o /dev/null --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("unsupported operand types for plus") != std::string::npos);
}

TEST_CASE("C++ emitter keeps array namespaced wrapper count builtin fallback") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32, 3i32, 4i32))
}

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(5i32, 6i32, 7i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(plus(/array/count(wrapMap()),
              /std/collections/vector/capacity(wrapVector())))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_array_namespaced_wrapper_count_capacity_builtin_fallback.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() /
                               "primec_cpp_array_namespaced_wrapper_count_capacity_builtin_fallback.cpp")
                                  .string();

  const std::string compileCmd = "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("C++ emitter rejects array namespaced wrapper capacity builtin fallback") {
  const std::string source = R"(
[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(5i32, 6i32, 7i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(/array/capacity(wrapVector()))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_array_namespaced_wrapper_capacity_reject.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_cpp_array_namespaced_wrapper_capacity_reject_out.txt")
          .string();
  const std::string cppOutPath =
      (std::filesystem::temp_directory_path() / "primec_cpp_array_namespaced_wrapper_capacity_reject.cpp").string();
  const std::string compileCmd =
      "./primec --emit=cpp " + srcPath + " -o " + cppOutPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("unknown call target: /array/capacity") != std::string::npos);
}

TEST_CASE("rejects namespaced count capacity method chain compatibility fallback in C++ emitter") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  tag([i32] value) {
    return(plus(value, 1i32))
  }
}

[effects(heap_alloc), return<int>]
main() {
  [string] text{"abc"raw_utf8}
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(plus(/std/collections/vector/count(text).tag(),
              /vector/capacity(values).tag()))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_namespaced_count_capacity_method_chain_compatibility_fallback_reject.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() /
                               "primec_cpp_namespaced_count_capacity_method_chain_compatibility_fallback_reject_out.txt")
                                  .string();

  const std::string compileCmd =
      "./primec --emit=cpp " + srcPath + " -o /dev/null --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) == 0);
}

TEST_CASE("rejects namespaced access method chain compatibility fallback in C++ emitter") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  tag([i32] value) {
    return(plus(value, 1i32))
  }
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(7i32, 8i32)}
  return(plus(/std/collections/vector/at(values, 0i32).tag(),
              /vector/at_unsafe(values, 1i32).tag()))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_namespaced_access_method_chain_compatibility_fallback_reject.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_cpp_namespaced_access_method_chain_compatibility_fallback_reject.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /vector/at_unsafe") != std::string::npos);
}

TEST_CASE("rejects namespaced wrapper string access method chain compatibility fallback in C++ emitter") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  tag([i32] value) {
    return(plus(value, 1i32))
  }
}

[return<string>]
wrapText() {
  return("abc"raw_utf8)
}

[return<int>]
main() {
  return(plus(/std/collections/vector/at(wrapText(), 1i32).tag(),
              /vector/at_unsafe(wrapText(), 0i32).tag()))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_namespaced_wrapper_string_access_method_chain_compatibility_fallback_reject.prime",
                source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() /
       "primec_cpp_namespaced_wrapper_string_access_method_chain_compatibility_fallback_reject_out.txt")
          .string();

  const std::string compileCmd =
      "./primec --emit=cpp " + srcPath + " -o /dev/null --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(outPath).find("unknown call target: /vector/at_unsafe") != std::string::npos);
}

TEST_CASE("rejects vector alias access struct method chain canonical forwarding in C++ emitter") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return(Marker(index))
}

[return<int>]
/Marker/tag([Marker] self) {
  return(self.value)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(/vector/at(values, 2i32).tag(),
              /vector/at(values, 1i32).tag()))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_vector_alias_access_struct_method_chain_canonical_forwarding_reject.prime", source);
  const std::string errPath = (std::filesystem::temp_directory_path() /
                               "primec_cpp_vector_alias_access_struct_method_chain_canonical_forwarding_reject.err")
                                  .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK_FALSE(readFile(errPath).empty());
}

TEST_CASE("rejects vector alias access struct method chain canonical diagnostics forwarding in C++ emitter") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return(Marker(index))
}

[return<int>]
/i32/tag([i32] self, [bool] marker) {
  return(self)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(/vector/at(values, 2i32).tag(1i32))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_vector_alias_access_struct_method_chain_canonical_diagnostic.prime", source);
  const std::string errPath = (std::filesystem::temp_directory_path() /
                               "primec_cpp_vector_alias_access_struct_method_chain_canonical_diagnostic.err")
                                  .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("argument type mismatch for /i32/tag parameter marker") != std::string::npos);
}

TEST_CASE("keeps canonical map access struct method chain forwarding in C++ emitter") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/map/at([map<i32, i32>] values, [i32] key) {
  return(Marker(key))
}

[return<int>]
/Marker/tag([Marker] self) {
  return(self.value)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(2i32, 7i32)}
  return(/std/collections/map/at(values, 2i32).tag())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_canonical_map_access_struct_method_chain_forwarding.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() /
                               "primec_cpp_canonical_map_access_struct_method_chain_forwarding.out")
                                  .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("prefers canonical bare map method struct chain forwarding in C++ emitter") {
  const std::string source = R"(
CanonicalMarker {
  [i32] value
}

AliasMarker {
  [i32] value
}

[return<AliasMarker>]
/map/at([map<i32, i32>] values, [i32] key) {
  return(AliasMarker(plus(key, 40i32)))
}

[return<CanonicalMarker>]
/std/collections/map/at([map<i32, i32>] values, [i32] key) {
  return(CanonicalMarker(key))
}

[return<int>]
/CanonicalMarker/tag([CanonicalMarker] self) {
  return(self.value)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(2i32, 7i32)}
  return(values.at(2i32).tag())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_bare_map_method_struct_chain_canonical_precedence.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_cpp_bare_map_method_struct_chain_canonical_precedence.out")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("keeps canonical bare map method non-struct diagnostics in C++ emitter") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/map/at([map<i32, i32>] values, [i32] key) {
  return(Marker(plus(key, 40i32)))
}

[return<i32>]
/std/collections/map/at([map<i32, i32>] values, [i32] key) {
  return(key)
}

[return<int>]
/Marker/tag([Marker] self) {
  return(self.value)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(2i32, 7i32)}
  return(values.at(2i32).tag())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_bare_map_method_struct_chain_canonical_diagnostic.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_cpp_bare_map_method_struct_chain_canonical_diagnostic.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /i32/tag") != std::string::npos);
}

TEST_CASE("rejects map access compatibility call struct method chain canonical forwarding in C++ emitter") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/map/at([map<i32, i32>] values, [i32] key) {
  return(Marker(key))
}

[return<int>]
/Marker/tag([Marker] self) {
  return(self.value)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(2i32, 7i32)}
  return(/map/at(values, 2i32).tag())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_map_access_alias_struct_method_chain_canonical_forwarding_reject.prime", source);
  const std::string errPath = (std::filesystem::temp_directory_path() /
                               "primec_cpp_map_access_alias_struct_method_chain_canonical_forwarding_reject.err")
                                  .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /i32/tag") != std::string::npos);
}

TEST_CASE("rejects map access compatibility call struct method chain canonical diagnostics forwarding in C++ emitter") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/map/at([map<i32, i32>] values, [i32] key) {
  return(Marker(key))
}

[return<int>]
/Marker/tag([Marker] self, [bool] marker) {
  return(self.value)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(2i32, 7i32)}
  return(/map/at(values, 2i32).tag(1i32))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_map_access_alias_struct_method_chain_canonical_diagnostic.prime", source);
  const std::string errPath = (std::filesystem::temp_directory_path() /
                               "primec_cpp_map_access_alias_struct_method_chain_canonical_diagnostic.err")
                                  .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("argument type mismatch for /Marker/tag parameter marker") != std::string::npos);
}

TEST_CASE("rejects vector alias access auto wrapper canonical struct-return forwarding in C++ emitter") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<int>]
/vector/at([vector<i32>] values, [i32] index) {
  return(index)
}

[return<Marker>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return(Marker(plus(index, 40i32)))
}

[return<int>]
/Marker/tag([Marker] self) {
  return(self.value)
}

[return<auto>]
project([vector<i32>] values) {
  return(/vector/at(values, 2i32))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(project(values).tag())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_vector_alias_access_auto_wrapper_canonical_struct_return_reject.prime", source);
  const std::string errPath = (std::filesystem::temp_directory_path() /
                               "primec_cpp_vector_alias_access_auto_wrapper_canonical_struct_return_reject.err")
                                  .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /i32/tag") != std::string::npos);
}

TEST_CASE("rejects vector alias access auto wrapper canonical diagnostics forwarding in C++ emitter") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<int>]
/vector/at([vector<i32>] values, [i32] index) {
  return(index)
}

[return<Marker>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return(Marker(plus(index, 40i32)))
}

[return<int>]
/i32/tag([i32] self, [bool] marker) {
  return(self)
}

[return<auto>]
project([vector<i32>] values) {
  return(/vector/at(values, 2i32))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(project(values).tag(1i32))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_vector_alias_access_auto_wrapper_canonical_diagnostic.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_cpp_vector_alias_access_auto_wrapper_canonical_diagnostic.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("argument type mismatch for /i32/tag parameter marker") != std::string::npos);
}

TEST_CASE("rejects vector method alias access struct method chain canonical forwarding in C++ emitter") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return(Marker(index))
}

[return<int>]
/Marker/tag([Marker] self) {
  return(self.value)
}

[return<auto>]
project([vector<i32>] values) {
  return(values./vector/at(2i32).tag())
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(project(values))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_vector_method_alias_access_struct_method_chain_canonical_forwarding_reject.prime",
                source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_cpp_vector_method_alias_access_struct_method_chain_canonical_forwarding_reject.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /i32/tag") != std::string::npos);
}

TEST_CASE("rejects vector method alias access struct method chain canonical diagnostics forwarding in C++ emitter") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return(Marker(index))
}

[return<int>]
/i32/tag([i32] self, [bool] marker) {
  return(self)
}

[return<auto>]
project([vector<i32>] values) {
  return(values./vector/at(2i32).tag(1i32))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(project(values))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_vector_method_alias_access_struct_method_chain_canonical_diagnostic.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_cpp_vector_method_alias_access_struct_method_chain_canonical_diagnostic.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("argument type mismatch for /i32/tag parameter marker") != std::string::npos);
}

TEST_CASE("C++ emitter forwards explicit-template vector count wrappers through canonical return kinds") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  tag([i32] value) {
    return(plus(value, 1i32))
  }
}

/vector/count([vector<i32>] values, [bool] marker) {
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [bool] marker) {
  return(41i32)
}

[return<auto>]
project([vector<i32>] values) {
  return(/vector/count<i32>(values, true))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(project(values).tag())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_vector_alias_count_explicit_template_wrapper_canonical_return.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() /
                               "primec_cpp_vector_alias_count_explicit_template_wrapper_canonical_return_exe")
                                  .string();
  const std::string errPath = (std::filesystem::temp_directory_path() /
                               "primec_cpp_vector_alias_count_explicit_template_wrapper_canonical_return.err")
                                  .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("template arguments are only supported on templated definitions: /vector/count") !=
        std::string::npos);
}

TEST_CASE("C++ emitter keeps canonical diagnostics for explicit-template vector count wrappers") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  tag([i32] value, [bool] marker) {
    return(value)
  }
}

/vector/count([vector<i32>] values, [bool] marker) {
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [bool] marker) {
  return(41i32)
}

[return<auto>]
project([vector<i32>] values) {
  return(/vector/count<i32>(values, true))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(project(values).tag(1i32))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_vector_alias_count_explicit_template_wrapper_canonical_diagnostic.prime", source);
  const std::string errPath = (std::filesystem::temp_directory_path() /
                               "primec_cpp_vector_alias_count_explicit_template_wrapper_canonical_diagnostic.err")
                                  .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("template arguments are only supported on templated definitions: /vector/count") !=
        std::string::npos);
}

TEST_CASE("rejects namespaced access method chain non-collection target in C++ emitter") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  tag([i32] value) {
    return(plus(value, 1i32))
  }
}

[return<int>]
main() {
  return(/std/collections/vector/at(7i32, 0i32).tag())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_namespaced_access_method_chain_non_collection_reject.prime", source);
  const std::string errPath = (std::filesystem::temp_directory_path() /
                               "primec_cpp_namespaced_access_method_chain_non_collection_reject.err")
                                  .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("at requires array, vector, map, or string target") != std::string::npos);
}

TEST_CASE("rejects namespaced wrapper access method chain non-collection target in C++ emitter") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  tag([i32] value) {
    return(plus(value, 1i32))
  }
}

[return<int>]
wrapNumber() {
  return(7i32)
}

[return<int>]
main() {
  return(/std/collections/vector/at(wrapNumber(), 0i32).tag())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_namespaced_wrapper_access_method_chain_non_collection_reject.prime", source);
  const std::string errPath = (std::filesystem::temp_directory_path() /
                               "primec_cpp_namespaced_wrapper_access_method_chain_non_collection_reject.err")
                                  .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("at requires array, vector, map, or string target") != std::string::npos);
}

TEST_CASE("rejects namespaced map capacity method chain target in C++ emitter") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  tag([i32] value) {
    return(plus(value, 1i32))
  }
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(/std/collections/vector/capacity(values).tag())
}
)";
  const std::string srcPath = writeTemp("compile_cpp_namespaced_map_capacity_method_chain_reject.prime", source);
  const std::string errPath = (std::filesystem::temp_directory_path() /
                               "primec_cpp_namespaced_map_capacity_method_chain_reject.err")
                                  .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("capacity requires vector target") != std::string::npos);
}

TEST_CASE("rejects wrapper map capacity target in C++ emitter") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32))
}

[return<int>]
main() {
  [i32] callValue{capacity(wrapMap())}
  [i32] methodValue{wrapMap().capacity()}
  return(plus(callValue, methodValue))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_wrapper_map_capacity_reject.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_cpp_wrapper_map_capacity_reject.err").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("capacity requires vector target") != std::string::npos);
}

TEST_CASE("rejects namespaced wrapper map capacity target in C++ emitter") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32))
}

[return<int>]
main() {
  return(/std/collections/vector/capacity(wrapMap()))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_namespaced_wrapper_map_capacity_reject.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_cpp_namespaced_wrapper_map_capacity_reject.err").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("capacity requires vector target") != std::string::npos);
}

TEST_CASE("C++ emitter infers wrapper collection builtin fallback") {
  const std::string source = R"(
wrapMap() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32, 3i32, 4i32)}
  return(values)
}

[effects(heap_alloc)]
wrapVector() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  return(plus(plus(count(wrapMap()), wrapMap().count()),
              plus(plus(count(wrapVector()), wrapVector().count()),
                   plus(capacity(wrapVector()), wrapVector().capacity()))))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_inferred_wrapper_count_capacity_builtin_fallback.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() /
                               "primec_cpp_inferred_wrapper_count_capacity_builtin_fallback.cpp")
                                  .string();

  const std::string compileCmd = "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects inferred wrapper map capacity target in C++ emitter") {
  const std::string source = R"(
wrapMap() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(values)
}

[return<int>]
main() {
  [i32] callValue{capacity(wrapMap())}
  [i32] methodValue{wrapMap().capacity()}
  return(plus(callValue, methodValue))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_inferred_wrapper_map_capacity_reject.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_cpp_inferred_wrapper_map_capacity_reject.err").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("capacity requires vector target") != std::string::npos);
}

TEST_CASE("C++ emitter infers wrapper access builtin fallback") {
  const std::string source = R"(
wrapMap() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(values)
}

[effects(heap_alloc)]
wrapVector() {
  [vector<i32>] values{vector<i32>(3i32, 4i32)}
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  return(plus(plus(plus(at(wrapMap(), 1i32), wrapMap().at(1i32)), wrapMap()[1i32]),
              plus(plus(at_unsafe(wrapMap(), 1i32), wrapMap().at_unsafe(1i32)),
                   plus(plus(plus(at(wrapVector(), 0i32), wrapVector().at(0i32)), wrapVector()[0i32]),
                        plus(at_unsafe(wrapVector(), 0i32), wrapVector().at_unsafe(0i32))))))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_inferred_wrapper_access_builtin_fallback.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_cpp_inferred_wrapper_access_builtin_fallback.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects inferred wrapper access key mismatch in C++ emitter") {
  const std::string source = R"(
wrapMap() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(values)
}

[return<int>]
main() {
  [i32] callValue{at(wrapMap(), true)}
  [i32] methodValue{wrapMap().at(true)}
  [i32] indexValue{wrapMap()[true]}
  [i32] unsafeCall{at_unsafe(wrapMap(), true)}
  [i32] unsafeMethod{wrapMap().at_unsafe(true)}
  return(plus(callValue, plus(methodValue, plus(indexValue, plus(unsafeCall, unsafeMethod)))))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_inferred_wrapper_access_key_mismatch.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_cpp_inferred_wrapper_access_key_mismatch_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("C++ emitter infers wrapper string access builtin fallback") {
  const std::string source = R"(
wrapText() {
  [string] text{"abc"raw_utf8}
  return(text)
}

[return<int>]
main() {
  return(plus(plus(at(wrapText(), 1i32), wrapText().at(2i32)),
              plus(at_unsafe(wrapText(), 0i32), wrapText().at_unsafe(1i32))))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_inferred_wrapper_string_access_builtin_fallback.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() /
                               "primec_cpp_inferred_wrapper_string_access_builtin_fallback.cpp")
                                  .string();

  const std::string compileCmd = "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("C++ emitter infers wrapper string count builtin fallback") {
  const std::string source = R"(
wrapText() {
  [string] text{"abc"raw_utf8}
  return(text)
}

[return<int>]
main() {
  return(plus(count(wrapText()), wrapText().count()))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_inferred_wrapper_string_count_builtin_fallback.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() /
       "primec_cpp_inferred_wrapper_string_count_builtin_fallback_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 6);
}

TEST_CASE("C++ emitter runs builtin count on canonical map reference string access") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [/std/collections/map<i32, string>] values{map<i32, string>(1i32, "hello"utf8)}
  [Reference</std/collections/map<i32, string>>] ref{location(values)}
  return(count(/std/collections/map/at(ref, 1i32)))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_builtin_count_canonical_map_reference_string_access.prime",
                                        source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() /
       "primec_cpp_builtin_count_canonical_map_reference_string_access_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 5);
}

TEST_CASE("C++ emitter runs builtin count on wrapper-returned canonical map string access") {
  const std::string source = R"(
[return</std/collections/map<i32, string>>]
wrapMap() {
  return(map<i32, string>(1i32, "hello"utf8))
}

[effects(heap_alloc), return<int>]
main() {
  return(count(/std/collections/map/at(wrapMap(), 1i32)))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_builtin_count_wrapper_canonical_map_string_access.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() /
       "primec_cpp_builtin_count_wrapper_canonical_map_string_access_exe")
          .string();
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_cpp_builtin_count_wrapper_canonical_map_string_access.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("native backend does not support string array return types on /wrapMap") !=
        std::string::npos);
}

TEST_CASE("C++ emitter treats wrapper-returned canonical map string access as string receiver") {
  const std::string source = R"(
[return</std/collections/map<i32, string>>]
wrapMap() {
  return(map<i32, string>(1i32, "hello"utf8))
}

[effects(heap_alloc), return<int>]
main() {
  return(wrapMap()[1i32].count())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_wrapper_canonical_map_string_receiver.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() /
       "primec_cpp_wrapper_canonical_map_string_receiver_exe")
          .string();
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_cpp_wrapper_canonical_map_string_receiver.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("native backend does not support string array return types on /wrapMap") !=
        std::string::npos);
}

TEST_CASE("C++ emitter treats direct-call wrapper-returned canonical map reference access as string receiver") {
  const std::string source = R"(
[return<Reference</std/collections/map<i32, string>>>]
borrowMap([Reference</std/collections/map<i32, string>>] values) {
  return(values)
}

[return<int>]
/string/count([string] values) {
  return(91i32)
}

[effects(heap_alloc), return<int>]
main() {
  [/std/collections/map<i32, string>] values{map<i32, string>(1i32, "hello"utf8)}
  return(/std/collections/map/at(borrowMap(location(values)), 1i32).count())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_direct_wrapper_map_reference_string_receiver.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() /
       "primec_cpp_direct_wrapper_map_reference_string_receiver_exe")
          .string();
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_cpp_direct_wrapper_map_reference_string_receiver.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("native backend only supports entry argument indexing") != std::string::npos);
}

TEST_CASE("C++ emitter keeps non-string diagnostics on wrapper-returned canonical map access receivers") {
  const std::string source = R"(
[return</std/collections/map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 4i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(wrapMap()[1i32].count())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_wrapper_canonical_map_string_receiver_diag.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_cpp_wrapper_canonical_map_string_receiver_diag.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /i32/count") != std::string::npos);
}

TEST_CASE("C++ emitter keeps non-string diagnostics on direct-call wrapper-returned canonical map reference access") {
  const std::string source = R"(
[return<Reference</std/collections/map<i32, i32>>>]
borrowMap([Reference</std/collections/map<i32, i32>>] values) {
  return(values)
}

[return<int>]
/string/count([string] values) {
  return(91i32)
}

[effects(heap_alloc), return<int>]
main() {
  [/std/collections/map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(/std/collections/map/at(borrowMap(location(values)), 1i32).count())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_direct_wrapper_map_reference_string_receiver_diag.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_cpp_direct_wrapper_map_reference_string_receiver_diag.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /i32/count") != std::string::npos);
}

TEST_CASE("C++ emitter keeps stdlib namespaced vector string access count fallback") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<string>] values{vector<string>("abc"raw_utf8)}
  return(count(/std/collections/vector/at(values, 0i32)))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_stdlib_namespaced_vector_string_access_count_fallback.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() /
                               "primec_cpp_stdlib_namespaced_vector_string_access_count_fallback.cpp")
                                  .string();

  const std::string compileCmd = "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects stdlib namespaced vector access count for non-string element in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32)}
  return(count(/std/collections/vector/at(values, 0i32)))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_stdlib_namespaced_vector_access_count_non_string_reject.prime", source);
  const std::string errPath = (std::filesystem::temp_directory_path() /
                               "primec_cpp_stdlib_namespaced_vector_access_count_non_string_reject.err")
                                  .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK_FALSE(readFile(errPath).empty());
}

TEST_CASE("rejects vector alias access count canonical wrapper return forwarding in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<vector<i32>>]
wrapValues() {
  return(vector<i32>(1i32))
}

[return<string>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return("abc"raw_utf8)
}

[effects(heap_alloc), return<int>]
main() {
  return(count(/vector/at(wrapValues(), 0i32)))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_vector_alias_access_count_canonical_wrapper_return_forwarding_reject.prime", source);
  const std::string errPath = (std::filesystem::temp_directory_path() /
                               "primec_cpp_vector_alias_access_count_canonical_wrapper_return_forwarding_reject.err")
                                  .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK_FALSE(readFile(errPath).empty());
}

TEST_CASE("rejects vector alias access count with canonical non-string wrapper return in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<vector<i32>>]
wrapValues() {
  return(vector<i32>(1i32))
}

[return<int>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return(7i32)
}

[effects(heap_alloc), return<int>]
main() {
  return(count(/vector/at(wrapValues(), 0i32)))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_vector_alias_access_count_canonical_wrapper_return_non_string_reject.prime", source);
  const std::string errPath = (std::filesystem::temp_directory_path() /
                               "primec_cpp_vector_alias_access_count_canonical_wrapper_return_non_string_reject.err")
                                  .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK_FALSE(readFile(errPath).empty());
}

TEST_CASE("rejects inferred wrapper string count arg mismatch in C++ emitter") {
  const std::string source = R"(
wrapText() {
  [string] text{"abc"raw_utf8}
  return(text)
}

[return<int>]
main() {
  [i32] callValue{count(wrapText(), 0i32)}
  [i32] methodValue{wrapText().count(1i32)}
  return(plus(callValue, methodValue))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_inferred_wrapper_string_count_arg_mismatch.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_cpp_inferred_wrapper_string_count_arg_mismatch_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects inferred wrapper string access index mismatch in C++ emitter") {
  const std::string source = R"(
wrapText() {
  [string] text{"abc"raw_utf8}
  return(text)
}

[return<int>]
main() {
  [i32] callValue{at(wrapText(), true)}
  [i32] methodValue{wrapText().at(true)}
  [i32] unsafeCall{at_unsafe(wrapText(), true)}
  [i32] unsafeMethod{wrapText().at_unsafe(true)}
  return(plus(callValue, plus(methodValue, plus(unsafeCall, unsafeMethod))))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_inferred_wrapper_string_access_index_mismatch.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_cpp_inferred_wrapper_string_access_index_mismatch_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("compiles and runs user wrapper temporary access shadow precedence in C++ emitter") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32))
}

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(3i32, 4i32))
}

[return<int>]
/map/at([map<i32, i32>] values, [i32] key) {
  return(80i32)
}

[return<int>]
/map/at_unsafe([map<i32, i32>] values, [i32] key) {
  return(81i32)
}

[effects(heap_alloc), return<int>]
/vector/at([vector<i32>] values, [i32] index) {
  return(82i32)
}

[effects(heap_alloc), return<int>]
/vector/at_unsafe([vector<i32>] values, [i32] index) {
  return(83i32)
}

[effects(heap_alloc), return<int>]
main() {
  return(plus(plus(plus(at(wrapMap(), 1i32), wrapMap().at(1i32)), wrapMap()[1i32]),
              plus(plus(at_unsafe(wrapMap(), 1i32), wrapMap().at_unsafe(1i32)),
                   plus(plus(plus(at(wrapVector(), 0i32), wrapVector().at(0i32)), wrapVector()[0i32]),
                        plus(at_unsafe(wrapVector(), 0i32), wrapVector().at_unsafe(0i32))))))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_user_wrapper_temp_access_shadow_precedence.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_cpp_user_wrapper_temp_access_shadow_precedence_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == expectedProcessExitCode(814));
}

TEST_CASE("rejects user wrapper temporary access shadow value mismatch in C++ emitter") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32))
}

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(3i32, 4i32))
}

[return<bool>]
/map/at([map<i32, i32>] values, [i32] key) {
  return(true)
}

[return<bool>]
/map/at_unsafe([map<i32, i32>] values, [i32] key) {
  return(false)
}

[effects(heap_alloc), return<bool>]
/vector/at([vector<i32>] values, [i32] index) {
  return(true)
}

[effects(heap_alloc), return<bool>]
/vector/at_unsafe([vector<i32>] values, [i32] index) {
  return(false)
}

[effects(heap_alloc), return<int>]
main() {
  [i32] mapCall{at(wrapMap(), 1i32)}
  [i32] mapMethod{wrapMap().at(1i32)}
  [i32] mapIndex{wrapMap()[1i32]}
  [i32] mapUnsafeCall{at_unsafe(wrapMap(), 1i32)}
  [i32] mapUnsafeMethod{wrapMap().at_unsafe(1i32)}
  [i32] vectorCall{at(wrapVector(), 0i32)}
  [i32] vectorMethod{wrapVector().at(0i32)}
  [i32] vectorIndex{wrapVector()[0i32]}
  [i32] vectorUnsafeCall{at_unsafe(wrapVector(), 0i32)}
  [i32] vectorUnsafeMethod{wrapVector().at_unsafe(0i32)}
  return(plus(mapCall, plus(mapMethod, plus(mapIndex, plus(mapUnsafeCall, plus(mapUnsafeMethod,
              plus(vectorCall, plus(vectorMethod, plus(vectorIndex, plus(vectorUnsafeCall, vectorUnsafeMethod)))))))))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_user_wrapper_temp_access_shadow_value_mismatch.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_cpp_user_wrapper_temp_access_shadow_value_mismatch_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects non-vector capacity call target in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(capacity(values))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_capacity_call_non_vector_reject.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_cpp_capacity_call_non_vector_reject.err").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("capacity requires vector target") != std::string::npos);
}

TEST_CASE("rejects non-vector capacity method target in C++ emitter") {
  const std::string source = R"(
[return<int>]
main() {
  [string] text{"abc"raw_utf8}
  return(text.capacity())
}
)";
  const std::string srcPath = writeTemp("compile_cpp_capacity_method_non_vector_reject.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_cpp_capacity_method_non_vector_reject.err").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("capacity requires vector target") != std::string::npos);
}

TEST_CASE("compiles and runs user array capacity helper shadow in C++ emitter") {
  const std::string source = R"(
[return<int>]
/array/capacity([array<i32>] values) {
  return(plus(count(values), 5i32))
}

[return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32)}
  return(plus(capacity(values), values.capacity()))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_user_array_capacity_shadow.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_cpp_user_array_capacity_shadow_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 14);
}

TEST_CASE("compiles and runs std math vector and color types") {
  const std::string source = R"(
import /std/math/*

[return<int>]
main() {
  [Vec2] v2{Vec2(1.0f32, 2.0f32)}
  [Vec3] v3{Vec3(3.0f32, 4.0f32, 0.0f32)}
  [Vec4] v4{Vec4(1.0f32, 2.0f32, 3.0f32, 4.0f32)}
  [f32] len2{v3.lengthSquared()}

  [ColorRGB] base{ColorRGB(0.5f32, 0.5f32, 0.5f32)}
  [ColorRGB] mixed{base.add(ColorRGB(0.5f32, 0.5f32, 0.5f32))}
  [ColorRGBA] rgba{ColorRGBA(0.5f32, 0.5f32, 0.5f32, 1.0f32)}
  [ColorRGBA] mixedA{rgba.mulScalar(2.0f32)}
  [ColorSRGB] srgb{ColorSRGB(0.0f32, 0.0f32, 0.0f32)}
  [ColorSRGBA] srgba{ColorSRGBA(0.0f32, 0.0f32, 0.0f32, 1.0f32)}

  [f32] total{len2 + mixed.r + mixed.g + mixed.b + mixedA.a + v2.x + v4.w + srgb.r + srgba.a}
  return(convert<int>(total))
}
)";
  const std::string srcPath = writeTemp("compile_std_math_vec_color.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_std_math_vec_color_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 36);
}

TEST_CASE("compiles and runs string-keyed map literal in C++ emitter") {
  const std::string source = R"(
[return<int>]
main() {
  [map<string, i32>] values{map<string, i32>("a"utf8, 1i32, "b"utf8, 2i32)}
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("compile_map_string_keys_exe.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_map_string_keys_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("compiles and runs lerp in C++ emitter") {
  const std::string source = R"(
import /std/math/*
[return<int>]
main() {
  return(lerp(2i32, 6i32, 1i32))
}
)";
  const std::string srcPath = writeTemp("compile_lerp_exe.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_lerp_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 6);
}

TEST_CASE("compiles and runs math-qualified clamp in C++ emitter") {
  const std::string source = R"(
[return<int>]
main() {
  return(/std/math/clamp(9i32, 2i32, 6i32))
}
)";
  const std::string srcPath = writeTemp("compile_math_clamp_exe.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_math_clamp_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 6);
}

TEST_CASE("compiles and runs math-qualified trig in C++ emitter") {
  const std::string source = R"(
[return<int>]
main() {
  return(convert<int>(/std/math/sin(0.0f)))
}
)";
  const std::string srcPath = writeTemp("compile_math_trig_exe.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_math_trig_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("compiles and runs math-qualified min/max in C++ emitter") {
  const std::string source = R"(
[return<int>]
main() {
  return(/std/math/min(9i32, /std/math/max(2i32, 6i32)))
}
)";
  const std::string srcPath = writeTemp("compile_math_minmax_exe.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_math_minmax_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 6);
}

TEST_CASE("compiles and runs math-qualified constants in C++ emitter") {
  const std::string source = R"(
[return<int>]
main() {
  return(plus(convert<int>(/std/math/pi), plus(convert<int>(/std/math/tau), convert<int>(/std/math/e))))
}
)";
  const std::string srcPath = writeTemp("compile_math_constants_exe.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_math_constants_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 11);
}

TEST_CASE("compiles and runs imported math constants in C++ emitter") {
  const std::string source = R"(
import /std/math/*
[return<int>]
main() {
  return(plus(convert<int>(pi), plus(convert<int>(tau), convert<int>(e))))
}
)";
  const std::string srcPath = writeTemp("compile_imported_math_constants_exe.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_imported_math_constants_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 11);
}

TEST_CASE("compiles and runs rounding builtins in C++ emitter") {
  const std::string source = R"(
import /std/math/*
[return<int>]
main() {
  [f32] value{1.5f}
  return(plus(plus(plus(convert<int>(floor(value)), convert<int>(ceil(value))),
                   plus(convert<int>(round(value)), convert<int>(trunc(value)))),
              convert<int>(multiply(fract(value), 10.0f))))
}
)";
  const std::string srcPath = writeTemp("compile_rounding_exe.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_rounding_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 11);
}

TEST_CASE("compiles and runs convert<bool> from float in C++ emitter") {
  const std::string source = R"(
[return<int>]
main() {
  return(plus(convert<int>(convert<bool>(0.0f)), convert<int>(convert<bool>(2.5f))))
}
)";
  const std::string srcPath = writeTemp("compile_convert_bool_float.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_convert_bool_float_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs string comparisons in C++ emitter") {
  const std::string source = R"(
[return<int>]
main() {
  return(plus(
    convert<int>(equal("alpha"raw_utf8, "alpha"raw_utf8)),
    convert<int>(less_than("alpha"raw_utf8, "beta"raw_utf8))
  ))
}
)";
  const std::string srcPath = writeTemp("compile_string_compare.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_string_compare_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("compiles and runs string map values in C++ emitter") {
  const std::string source = R"(
[return<int>]
main() {
  [map<string, string>] values{
    map<string, string>("a"raw_utf8, "alpha"raw_utf8, "b"raw_utf8, "beta"raw_utf8)
  }
  [string] value{at(values, "b"raw_utf8)}
  return(convert<int>(equal(value, "beta"raw_utf8)))
}
)";
  const std::string srcPath = writeTemp("compile_string_map_values.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_string_map_values_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("compiles and runs power/log builtins in C++ emitter") {
  const std::string source = R"(
import /std/math/*
[return<int>]
main() {
  return(plus(
    plus(
      plus(convert<int>(sqrt(9.0f)), convert<int>(cbrt(27.0f))),
      plus(convert<int>(pow(2.0f, 3.0f)), convert<int>(exp(0.0f)))
    ),
    plus(
      plus(convert<int>(exp2(3.0f)), convert<int>(log(1.0f))),
      plus(convert<int>(log2(8.0f)), convert<int>(log10(1000.0f)))
    )
  ))
}
)";
  const std::string srcPath = writeTemp("compile_power_log_exe.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_power_log_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 25);
}

TEST_CASE("compiles and runs integer pow negative exponent in C++ emitter") {
  const std::string source = R"(
import /std/math/*
[return<int>]
main() {
  return(pow(2i32, -1i32))
}
)";
  const std::string srcPath = writeTemp("compile_int_pow_negative_exe.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_int_pow_negative_exe").string();
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_int_pow_negative_err.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " 2> " + errPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(errPath) == "pow exponent must be non-negative\n");
}

TEST_CASE("compiles and runs trig builtins in C++ emitter") {
  const std::string source = R"(
import /std/math/*
[return<int>]
main() {
  [f32] zero{0.0f}
  [f32] one{1.0f}
  return(plus(
    convert<int>(cos(zero)),
    plus(
      plus(convert<int>(sin(zero)), convert<int>(tan(zero))),
      plus(
        plus(convert<int>(asin(zero)), convert<int>(acos(one))),
        plus(
          plus(convert<int>(atan(zero)), convert<int>(atan2(zero, one))),
          convert<int>(round(degrees(radians(180.0f))))
        )
      )
    )
  ))
}
)";
  const std::string srcPath = writeTemp("compile_trig_exe.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_trig_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 181);
}

TEST_CASE("compiles and runs hyperbolic builtins in C++ emitter") {
  const std::string source = R"(
import /std/math/*
[return<int>]
main() {
  [f32] zero{0.0f}
  return(plus(
    plus(convert<int>(sinh(zero)), convert<int>(cosh(zero))),
    plus(
      plus(convert<int>(tanh(zero)), convert<int>(asinh(zero))),
      plus(convert<int>(acosh(1.0f)), convert<int>(atanh(zero)))
    )
  ))
}
)";
  const std::string srcPath = writeTemp("compile_hyperbolic_exe.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_hyperbolic_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs float utils in C++ emitter") {
  const std::string source = R"(
import /std/math/*
[return<int>]
main() {
  return(plus(
    plus(convert<int>(fma(2.0f, 3.0f, 1.0f)), convert<int>(hypot(3.0f, 4.0f))),
    convert<int>(copysign(2.0f, -1.0f))
  ))
}
)";
  const std::string srcPath = writeTemp("compile_float_utils_exe.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_float_utils_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 10);
}

TEST_CASE("compiles and runs float predicates in C++ emitter") {
  const std::string source = R"(
import /std/math/*
[return<int>]
main() {
  [f32] zero{0.0f}
  [f32] one{1.0f}
  [f32] nanValue{divide(zero, zero)}
  [f32] infValue{divide(one, zero)}
  return(plus(
    plus(convert<int>(is_nan(nanValue)), convert<int>(is_inf(infValue))),
    convert<int>(is_finite(one))
  ))
}
)";
  const std::string srcPath = writeTemp("compile_float_predicates_exe.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_float_predicates_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("compiles and runs import aliases in C++ emitter") {
  const std::string source = R"(
import /util
namespace util {
  [public return<int>]
  inc([i32] value) {
    return(plus(value, 1i32))
  }
}
[return<int>]
main() {
  return(inc(4i32))
}
)";
  const std::string srcPath = writeTemp("compile_import_alias_exe.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_import_alias_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 5);
}

TEST_CASE("compiles and runs math constants in C++ emitter") {
  const std::string source = R"(
import /std/math/*
[return<int>]
main() {
  return(plus(
    plus(convert<int>(round(pi)), convert<int>(round(tau))),
    convert<int>(round(e))
  ))
}
)";
  const std::string srcPath = writeTemp("compile_constants_exe.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_constants_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 12);
}

TEST_CASE("compiles and runs array unsafe access in C++ emitter") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(4i32, 7i32, 9i32)}
  return(at_unsafe(values, 1i32))
}
)";
  const std::string srcPath = writeTemp("compile_array_unsafe_exe.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_array_unsafe_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs array unsafe access with u64 index in C++ emitter") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(4i32, 7i32, 9i32)}
  return(at_unsafe(values, 1u64))
}
)";
  const std::string srcPath = writeTemp("compile_array_unsafe_u64_exe.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_array_unsafe_u64_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs repeat loop") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{0i32}
  repeat(3i32) {
    assign(value, plus(value, 2i32))
  }
  return(value)
}
)";
  const std::string srcPath = writeTemp("compile_repeat_loop.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_repeat_loop_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 6);
}

TEST_CASE("compiles and runs loop while for sugar") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] total{0i32}
  [i32 mut] i{0i32}
  loop(2i32) {
    assign(total, plus(total, 1i32))
  }
  while(less_than(i 3i32)) {
    assign(total, plus(total, i))
    assign(i, plus(i, 1i32))
  }
  for([i32 mut] j{0i32} less_than(j 2i32) assign(j, plus(j, 1i32))) {
    assign(total, plus(total, j))
  }
  return(total)
}
)";
  const std::string srcPath = writeTemp("compile_loop_while_for.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_loop_while_for_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 6);
}

TEST_CASE("compiles and runs for binding condition") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] total{0i32}
  for([i32 mut] i{0i32} [bool] keep{less_than(i, 3i32)} assign(i, plus(i, 1i32))) {
    if(keep, then(){ assign(total, plus(total, 2i32)) }, else(){})
  }
  return(total)
}
)";
  const std::string srcPath = writeTemp("compile_for_condition_binding.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_for_condition_binding_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 6);
}

TEST_CASE("compiles and runs shared_scope loops") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] result{0i32}
  [shared_scope]
  loop(4i32) {
    [i32 mut] total{0i32}
    assign(total, plus(total, 1i32))
    assign(result, total)
  }
  [shared_scope]
  for([i32 mut] i{1i32} less_than(i 10i32) assign(i, multiply(i, 2i32))) {
    [i32 mut] total{i}
    assign(total, plus(total, 1i32))
    assign(result, total)
  }
  return(result)
}
)";
  const std::string srcPath = writeTemp("compile_shared_scope.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_shared_scope_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 5);
}

TEST_CASE("compiles and runs shared_scope for binding condition") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] total{0i32}
  [shared_scope]
  for([i32 mut] i{0i32} [bool] keep{less_than(i, 3i32)} assign(i, plus(i, 1i32))) {
    [i32 mut] acc{0i32}
    if(keep, then(){ assign(acc, plus(acc, 1i32)) }, else(){})
    assign(total, plus(total, acc))
  }
  return(total)
}
)";
  const std::string srcPath = writeTemp("compile_shared_scope_for_cond.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_shared_scope_for_cond_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 6);
}

TEST_CASE("compiles and runs shared_scope while loop") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] total{0i32}
  [i32 mut] i{0i32}
  [shared_scope]
  while(less_than(i, 3i32)) {
    [i32 mut] acc{0i32}
    assign(acc, plus(acc, 1i32))
    assign(total, plus(total, acc))
    assign(i, plus(i, 1i32))
  }
  return(total)
}
)";
  const std::string srcPath = writeTemp("compile_shared_scope_while.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_shared_scope_while_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 6);
}

TEST_CASE("compiles and runs increment decrement sugar") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{1i32}
  value++
  --value
  return(value)
}
)";
  const std::string srcPath = writeTemp("compile_increment_decrement.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_increment_decrement_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs brace constructor value") {
  const std::string source = R"(
[return<int>]
pick([i32] value) {
  return(value)
}

[return<int>]
main() {
  return(pick{ 3i32 })
}
)";
  const std::string srcPath = writeTemp("compile_brace_constructor_value.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_brace_constructor_value_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("compiles and runs nested definition call") {
  const std::string source = R"(
[return<int>]
main() {
  [return<int>]
  helper() {
    return(5i32)
  }
  return(/main/helper())
}
)";
  const std::string srcPath = writeTemp("compile_nested_definition_call.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_nested_definition_call_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 5);
}

TEST_CASE("compiles and runs map literal") {
  const std::string source = R"(
[return<int>]
main() {
  map<i32, i32>{1i32=2i32, 3i32=4i32}
  return(9i32)
}
)";
  const std::string srcPath = writeTemp("compile_map_literal.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_map_literal_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_SUITE_END();
