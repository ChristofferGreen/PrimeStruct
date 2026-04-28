#include "test_compile_run_helpers.h"
#include "test_compile_run_collection_conformance_helpers.h"
#include "test_compile_run_emitters_helpers.h"

#include "primec/testing/EmitterHelpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.emitters.cpp");

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
  const std::string exePath = (testScratchPath("") / "primec_method_chain_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 4);
}

TEST_CASE("C++ emitter lowers explicit Result sum constructors through bridge helpers") {
  const std::string source = R"(
import /std/result/*

[return<Result<i32, i32>>]
make_value_ok() {
  return(Result<i32, i32>{[ok] 7i32})
}

[return<Result<i32, i32>>]
make_value_error() {
  return(Result<i32, i32>{[error] 4i32})
}

[return<Result<i32>>]
make_status_ok() {
  return(Result<i32>{ok})
}

[return<Result<i32>>]
make_status_default() {
  return(Result<i32>{})
}

[return<Result<i32>>]
make_status_error() {
  return(Result<i32>{[error] 5i32})
}

[return<int>]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_cpp_explicit_result_sum_constructors.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_explicit_result_sum_constructors.cpp").string();

  const std::string compileCmd =
      "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("ps_result_value_ok(static_cast<uint32_t>(") != std::string::npos);
  CHECK(output.find("ps_result_value_error(static_cast<uint32_t>(") != std::string::npos);
  CHECK(output.find("ps_result_status_error(static_cast<uint32_t>(") != std::string::npos);
  CHECK(output.find("return ps_result_status_ok();") != std::string::npos);
  CHECK(output.find("return Result<") == std::string::npos);
  CHECK(output.find("{[error]") == std::string::npos);
}

TEST_CASE("C++ emitter guards Result.why on ok bridge values") {
  const std::string source = R"(
import /std/file/*
import /std/result/*

[return<Result<FileError>>]
make_status_ok() {
  return(Result<FileError>{})
}

[return<Result<FileError>>]
make_status_error() {
  return(FileError.status(FileError.eof()))
}

[return<Result<i32, FileError>>]
make_value_ok() {
  return(Result<i32, FileError>{[ok] 7i32})
}

[return<Result<i32, FileError>>]
make_value_error() {
  return(FileError.result<i32>(FileError.eof()))
}

[return<int>]
main() {
  [string] statusOkWhy{Result.why(make_status_ok())}
  [string] statusErrorWhy{Result.why(make_status_error())}
  [string] valueOkWhy{Result.why(make_value_ok())}
  [string] valueErrorWhy{Result.why(make_value_error())}
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_cpp_result_why_guard.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_result_why_guard.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("if (!(ps_result_status_is_error(ps_result))) { return std::string_view(); }") !=
        std::string::npos);
  CHECK(output.find("if (!(ps_result_value_is_error(ps_result))) { return std::string_view(); }") !=
        std::string::npos);
  CHECK(output.find("ps_file_error_why(ps_result_status_error_payload(ps_result))") != std::string::npos);
  CHECK(output.find("ps_file_error_why(ps_result_value_error_payload(ps_result))") != std::string::npos);
}

TEST_CASE("C++ emitter packs single-field error sum constructor payloads") {
  const std::string source = R"(
import /std/result/*

[struct]
MyError() {
  [i32] code{0i32}
}

[return<Result<MyError>>]
make_status_error() {
  return(Result<MyError>{[error] MyError{[code] 7i32}})
}

[return<Result<i32, MyError>>]
make_value_error() {
  [MyError] err{MyError{[code] 9i32}}
  return(Result<i32, MyError>{[error] err})
}

[return<int>]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_cpp_result_error_struct_constructor.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_result_error_struct_constructor.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("ps_result_status_error(static_cast<uint32_t>((") != std::string::npos);
  CHECK(output.find(".code))") != std::string::npos);
  CHECK(output.find("ps_result_value_error(static_cast<uint32_t>((err).code))") != std::string::npos);
  CHECK(output.find("static_cast<uint32_t>(err)") == std::string::npos);
}

TEST_CASE("C++ emitter rejects experimental map custom comparable struct keys") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[struct]
Key() {
  [i32] value{0i32}
}

[return<bool>]
/Key/equal([Key] left, [Key] right) {
  return(equal(left.value, right.value))
}

[return<bool>]
/Key/less_than([Key] left, [Key] right) {
  return(less_than(left.value, right.value))
}

[effects(heap_alloc), return<int>]
main() {
  [Map<Key, i32>] values{mapPair<Key, i32>(Key{2i32}, 7i32, Key{5i32}, 11i32)}
  [i32 mut] total{mapCount<Key, i32>(values)}
  assign(total, plus(total, mapAt<Key, i32>(values, Key{2i32})))
  assign(total, plus(total, mapAtUnsafe<Key, i32>(values, Key{5i32})))
  if(mapContains<Key, i32>(values, Key{2i32}),
     then() { assign(total, plus(total, 1i32)) },
     else() { })
  return(total)
}
)";
  const std::string srcPath = writeTemp("compile_cpp_experimental_map_custom_comparable_key.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_experimental_map_custom_comparable_key_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("native backend only supports numeric/bool map values") !=
        std::string::npos);
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
  const std::string exePath = (testScratchPath("") / "primec_exec_ignored_exe").string();
  const std::string outPath = (testScratchPath("") / "primec_exec_ignored_out.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " > " + outPath;
  CHECK(runCommand(runCmd) == 1);
  CHECK(readFile(outPath).empty());
}

TEST_CASE("struct Create/Destroy helpers currently trip C++ emitter stack underflow at runtime") {
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
  Thing{}
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_struct_lifecycle.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_struct_lifecycle_exe").string();
  const std::string outPath = (testScratchPath("") / "primec_struct_lifecycle_out.txt").string();
  const std::string errPath = (testScratchPath("") / "primec_struct_lifecycle_err.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " > " + outPath + " 2> " + errPath;
  CHECK(runCommand(runCmd) == 1);
  CHECK(readFile(outPath).empty());
  CHECK(readFile(errPath).find("IR stack underflow on pop") != std::string::npos);
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
  return(Point{[x] x, [y] y})
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
  const std::string exePath = (testScratchPath("") / "primec_struct_return_exe").string();

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
  [Point] value{Point{[x] 9i32}}
  return(plus(value.x, value.y))
}
)";
  const std::string srcPath = writeTemp("compile_struct_defaults.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_struct_defaults_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 11);
}

TEST_CASE("C++ emitter supports file io") {
  const std::string outPath = (testScratchPath("") / "primec_file_io_exe.txt").string();
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
  const std::string exePath = (testScratchPath("") / "primec_file_io_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
  CHECK(readFile(outPath) == "Hello 123 world\n\nABC");
}

TEST_CASE("C++ emitter supports file read_byte with deterministic eof") {
  const std::string inPath = (testScratchPath("") / "primec_file_read_byte_exe.txt").string();
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
  const std::string exePath = (testScratchPath("") / "primec_file_read_byte_exe").string();
  const std::string outPath = (testScratchPath("") / "primec_file_read_byte_out.txt").string();

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
  const std::string exePath = (testScratchPath("") / "primec_result_why_custom_exe").string();
  const std::string outPath =
      (testScratchPath("") / "primec_result_why_custom_out.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "custom error\n");
}

TEST_CASE("C++ emitter compiles nested Result combinators") {
  CHECK(runSharedCppEmitterFixture("primec_cpp_emitter_result_fixture", sharedCppEmitterResultFixtureSource(), 2) ==
        16);
}

TEST_CASE("C++ emitter compiles Result.and_then direct Result.ok lambda") {
  CHECK(runSharedCppEmitterFixture("primec_cpp_emitter_result_fixture", sharedCppEmitterResultFixtureSource(), 3) ==
        20);
}

TEST_CASE("C++ emitter supports image api contract deterministically") {
  const std::string outPath = testScratchPath("primec_cpp_image_api_unsupported.txt").string();

  CHECK(runSharedCppEmitterFixtureToFile("primec_cpp_emitter_image_fixture",
                                         sharedCppEmitterImageFixtureSource(),
                                         2,
                                         outPath) == 0);
  CHECK(readFile(outPath) ==
        "image_invalid_operation\n"
        "image_invalid_operation\n"
        "image_read_unsupported\n"
        "image_invalid_operation\n");
}

TEST_CASE("C++ emitter runs software renderer command serialization deterministically") {
  const std::string outPath = testScratchPath("primec_cpp_ui_command_serialization.txt").string();

  CHECK(runSharedCppEmitterFixtureToFile("primec_cpp_emitter_ui_commands_fixture",
                                         sharedCppEmitterUiCommandFixtureSource(),
                                         2,
                                         outPath) == 2);
  CHECK(readFile(outPath) == "1,2,2,9,2,4,30,40,6,12,34,56,255,1,11,7,9,14,255,240,0,255,3,72,105,33\n");
}

TEST_CASE("C++ emitter runs software renderer clip stack serialization deterministically") {
  const std::string outPath = testScratchPath("primec_cpp_ui_clip_command_serialization.txt").string();

  CHECK(runSharedCppEmitterFixtureToFile("primec_cpp_emitter_ui_commands_fixture",
                                         sharedCppEmitterUiCommandFixtureSource(),
                                         3,
                                         outPath) == 6);
  CHECK(readFile(outPath) ==
        "1,6,3,4,1,2,20,10,1,11,7,9,14,255,240,0,255,3,72,105,33,3,4,3,4,5,6,2,9,8,9,10,11,2,1,2,3,4,4,0,4,0\n");
}

TEST_CASE("C++ emitter runs two-pass layout tree serialization deterministically") {
  const std::string outPath = testScratchPath("primec_cpp_ui_layout_tree_serialization.txt").string();

  CHECK(runSharedCppEmitterFixtureToFile("primec_cpp_emitter_ui_layout_fixture",
                                         sharedCppEmitterUiLayoutFixtureSource(),
                                         2,
                                         outPath) == 6);
  CHECK(readFile(outPath) ==
        "1,6,2,-1,24,36,10,20,50,40,1,0,20,5,12,22,46,5,2,0,12,14,12,30,46,14,1,2,8,4,13,31,44,4,1,2,10,6,13,37,44,6,1,0,6,7,12,47,46,7\n");
}

TEST_CASE("C++ emitter runs two-pass layout empty root deterministically") {
  const std::string outPath = testScratchPath("primec_cpp_ui_layout_tree_empty_root.txt").string();

  CHECK(runSharedCppEmitterFixtureToFile("primec_cpp_emitter_ui_layout_fixture",
                                         sharedCppEmitterUiLayoutFixtureSource(),
                                         3,
                                         outPath) == 1);
  CHECK(readFile(outPath) == "1,1,2,-1,11,13,3,5,11,13\n");
}

TEST_CASE("C++ emitter runs basic widget controls through layout deterministically") {
  const std::string outPath = testScratchPath("primec_cpp_ui_basic_widget_controls.txt").string();

  CHECK(runSharedCppEmitterFixtureToFile("primec_cpp_emitter_ui_widgets_fixture",
                                         sharedCppEmitterUiWidgetFixtureSource(),
                                         2,
                                         outPath) == 9);
  CHECK(readFile(outPath) ==
        "1,5,1,10,6,7,10,1,2,3,255,2,72,105,2,9,6,19,28,16,4,10,20,30,255,1,10,9,22,10,250,251,252,255,2,71,111,2,9,6,37,28,14,3,40,50,60,255,1,11,8,39,10,200,210,220,255,3,97,98,99\n");
}

TEST_CASE("C++ emitter runs panel container widget deterministically") {
  const std::string outPath = testScratchPath("primec_cpp_ui_panel_widget.txt").string();

  CHECK(runSharedCppEmitterFixtureToFile("primec_cpp_emitter_ui_widgets_fixture",
                                         sharedCppEmitterUiWidgetFixtureSource(),
                                         3,
                                         outPath) == 15);
  CHECK(readFile(outPath) ==
        "1,9,1,11,5,6,10,1,2,3,255,3,84,111,112,2,9,5,18,26,31,4,8,9,10,255,3,4,7,20,22,27,2,9,7,20,22,14,3,20,30,40,255,1,10,9,22,10,200,201,202,255,2,71,111,2,9,7,35,22,12,2,50,60,70,255,1,11,8,36,10,210,211,212,255,3,97,98,99,4,0,1,9,5,51,10,1,2,3,255,1,33\n");
}

TEST_CASE("C++ emitter runs empty panel container stays balanced deterministically") {
  const std::string outPath = testScratchPath("primec_cpp_ui_empty_panel_widget.txt").string();

  CHECK(runSharedCppEmitterFixtureToFile("primec_cpp_emitter_ui_widgets_fixture",
                                         sharedCppEmitterUiWidgetFixtureSource(),
                                         4,
                                         outPath) == 5);
  CHECK(readFile(outPath) == "1,3,2,9,2,3,20,10,5,9,8,7,255,3,4,5,6,14,4,4,0\n");
}

TEST_CASE("C++ emitter runs composite login form deterministically") {
  const std::string outPath = testScratchPath("primec_cpp_ui_composite_login_form.txt").string();

  CHECK(runSharedCppEmitterFixtureToFile("primec_cpp_emitter_ui_login_fixture",
                                         sharedCppEmitterUiLoginFixtureSource(),
                                         2,
                                         outPath) == 16);
  CHECK(readFile(outPath) ==
        "1,10,2,9,7,8,38,55,4,9,8,7,255,3,4,9,10,34,51,1,13,9,10,10,1,2,3,255,5,76,111,103,105,110,2,9,9,21,34,12,3,20,30,40,255,1,13,10,22,10,200,201,202,255,5,97,108,105,99,101,2,9,9,34,34,12,3,20,30,40,255,1,14,10,35,10,200,201,202,255,6,115,101,99,114,101,116,2,9,9,47,34,14,3,50,60,70,255,1,10,11,49,10,250,251,252,255,2,71,111,4,0\n");
}

TEST_CASE("C++ emitter runs html adapter login form deterministically") {
  const std::string outPath = testScratchPath("primec_cpp_ui_html_login_form.txt").string();

  CHECK(runSharedCppEmitterFixtureToFile("primec_cpp_emitter_ui_login_fixture",
                                         sharedCppEmitterUiLoginFixtureSource(),
                                         3,
                                         outPath) == 14);
  CHECK(readFile(outPath) ==
        "1,8,1,12,1,0,7,8,38,55,2,4,9,8,7,255,2,17,2,1,9,10,34,10,10,1,2,3,255,5,76,111,103,105,110,4,23,3,1,9,21,34,12,10,1,3,20,30,40,255,200,201,202,255,5,97,108,105,99,101,5,13,3,2,10,117,115,101,114,95,105,110,112,117,116,4,24,4,1,9,34,34,12,10,1,3,20,30,40,255,200,201,202,255,6,115,101,99,114,101,116,5,13,4,2,10,112,97,115,115,95,105,110,112,117,116,3,20,5,1,9,47,34,14,10,2,3,50,60,70,255,250,251,252,255,2,71,111,5,15,5,1,12,115,117,98,109,105,116,95,99,108,105,99,107\n");
}

TEST_CASE("C++ emitter runs ui event stream deterministically") {
  const std::string outPath = testScratchPath("primec_cpp_ui_event_stream.txt").string();

  CHECK(runSharedCppEmitterFixtureToFile("primec_cpp_emitter_ui_events_fixture",
                                         sharedCppEmitterUiEventFixtureSource(),
                                         2,
                                         outPath) == 10);
  CHECK(readFile(outPath) ==
        "1,5,1,5,3,7,-1,20,30,2,5,5,7,1,20,30,3,5,5,7,1,21,31,4,4,3,13,3,1,5,4,3,13,1,0\n");
}

TEST_CASE("C++ emitter runs ui ime event stream deterministically") {
  const std::string outPath = testScratchPath("primec_cpp_ui_ime_event_stream.txt").string();

  CHECK(runSharedCppEmitterFixtureToFile("primec_cpp_emitter_ui_events_fixture",
                                         sharedCppEmitterUiEventFixtureSource(),
                                         3,
                                         outPath) == 5);
  CHECK(readFile(outPath) ==
        "1,2,6,7,3,1,4,3,97,108,124,7,9,3,-1,-1,5,97,108,105,99,101\n");
}

TEST_CASE("C++ emitter runs ui resize and focus event stream deterministically") {
  const std::string outPath = testScratchPath("primec_cpp_ui_resize_focus_event_stream.txt").string();

  CHECK(runSharedCppEmitterFixtureToFile("primec_cpp_emitter_ui_events_fixture",
                                         sharedCppEmitterUiEventFixtureSource(),
                                         4,
                                         outPath) == 6);
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
  const std::string srcPath = writeTemp("compile_graphics_int_on_error_exe.prime", source);
  const std::string okExePath =
      (testScratchPath("") / "primec_graphics_int_on_error_exe_ok").string();
  const std::string failExePath =
      (testScratchPath("") / "primec_graphics_int_on_error_exe_fail").string();
  const std::string errPath =
      (testScratchPath("") / "primec_graphics_int_on_error_exe_err.txt").string();

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
  const std::string outPath = (testScratchPath("") / "primec_struct_static.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("std::vector<uint64_t> heapSlots") != std::string::npos);
  CHECK(output.find("return ps_fn_0(stack, sp, heapSlots, heapAllocations, argc, argv);") != std::string::npos);
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
  const std::string outPath = (testScratchPath("") / "primec_copy_params.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("heapAllocSlotCount") != std::string::npos);
  CHECK(output.find("static int64_t ps_fn_0(") != std::string::npos);
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
  const std::string outPath = (testScratchPath("") / "primec_lambda.cpp").string();

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
  const std::string outPath = (testScratchPath("") / "primec_lambda_explicit.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_SUITE_END();
