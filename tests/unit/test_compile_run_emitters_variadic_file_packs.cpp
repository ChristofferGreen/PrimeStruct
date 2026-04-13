#include "test_compile_run_helpers.h"
#include "test_compile_run_collection_conformance_helpers.h"
#include "test_compile_run_emitters_helpers.h"

#include "primec/testing/EmitterHelpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.emitters.cpp");

TEST_CASE("C++ emitter materializes variadic pointer FileError packs with indexed dereference why methods") {
  const std::string source = R"(
[return<int>]
score_ptrs([args<Pointer<FileError>>] values) {
  return(plus(count(dereference(values[0i32]).why()), count(dereference(values[2i32]).why())))
}

[return<int>]
forward([args<Pointer<FileError>>] values) {
  return(score_ptrs([spread] values))
}

[return<int>]
forward_mixed([args<Pointer<FileError>>] values) {
  [FileError] extra{13i32}
  [Pointer<FileError>] extra_ptr{location(extra)}
  return(score_ptrs(extra_ptr, [spread] values))
}

[return<int>]
main() {
  [FileError] a0{13i32}
  [FileError] a1{2i32}
  [FileError] a2{17i32}
  [Pointer<FileError>] r0{location(a0)}
  [Pointer<FileError>] r1{location(a1)}
  [Pointer<FileError>] r2{location(a2)}
  [FileError] b0{2i32}
  [FileError] b1{17i32}
  [FileError] b2{13i32}
  [Pointer<FileError>] s0{location(b0)}
  [Pointer<FileError>] s1{location(b1)}
  [Pointer<FileError>] s2{location(b2)}
  [FileError] c0{2i32}
  [FileError] c1{17i32}
  [Pointer<FileError>] t0{location(c0)}
  [Pointer<FileError>] t1{location(c1)}
  return(plus(score_ptrs(r0, r1, r2), plus(forward(s0, s1, s2), forward_mixed(t0, t1))))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_variadic_args_pointer_file_error.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_variadic_args_pointer_file_error_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 36);
}

TEST_CASE("C++ emitter materializes variadic borrowed File handle packs with indexed dereference file methods") {
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
  const std::string pathA0 = (testScratchPath("") / "primec_cpp_variadic_borrowed_file_handle_a0.txt").string();
  const std::string pathA1 = (testScratchPath("") / "primec_cpp_variadic_borrowed_file_handle_a1.txt").string();
  const std::string pathA2 = (testScratchPath("") / "primec_cpp_variadic_borrowed_file_handle_a2.txt").string();
  const std::string pathB0 = (testScratchPath("") / "primec_cpp_variadic_borrowed_file_handle_b0.txt").string();
  const std::string pathB1 = (testScratchPath("") / "primec_cpp_variadic_borrowed_file_handle_b1.txt").string();
  const std::string pathB2 = (testScratchPath("") / "primec_cpp_variadic_borrowed_file_handle_b2.txt").string();
  const std::string pathC0 = (testScratchPath("") / "primec_cpp_variadic_borrowed_file_handle_c0.txt").string();
  const std::string pathC1 = (testScratchPath("") / "primec_cpp_variadic_borrowed_file_handle_c1.txt").string();
  const std::string pathExtra =
      (testScratchPath("") / "primec_cpp_variadic_borrowed_file_handle_extra.txt").string();

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
  const std::string srcPath = writeTemp("compile_cpp_variadic_args_borrowed_file_handle.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_variadic_args_borrowed_file_handle_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 39);
  CHECK(readFile(pathA0) == "alpha\n");
  CHECK(readFile(pathA2) == "omega\n");
  CHECK(readFile(pathExtra) == "alpha\n");
  CHECK(readFile(pathC1) == "omega\n");
}

TEST_CASE("C++ emitter materializes variadic pointer File handle packs with indexed dereference file methods") {
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
  const std::string pathA0 = (testScratchPath("") / "primec_cpp_variadic_pointer_file_handle_a0.txt").string();
  const std::string pathA1 = (testScratchPath("") / "primec_cpp_variadic_pointer_file_handle_a1.txt").string();
  const std::string pathA2 = (testScratchPath("") / "primec_cpp_variadic_pointer_file_handle_a2.txt").string();
  const std::string pathB0 = (testScratchPath("") / "primec_cpp_variadic_pointer_file_handle_b0.txt").string();
  const std::string pathB1 = (testScratchPath("") / "primec_cpp_variadic_pointer_file_handle_b1.txt").string();
  const std::string pathB2 = (testScratchPath("") / "primec_cpp_variadic_pointer_file_handle_b2.txt").string();
  const std::string pathC0 = (testScratchPath("") / "primec_cpp_variadic_pointer_file_handle_c0.txt").string();
  const std::string pathC1 = (testScratchPath("") / "primec_cpp_variadic_pointer_file_handle_c1.txt").string();
  const std::string pathExtra =
      (testScratchPath("") / "primec_cpp_variadic_pointer_file_handle_extra.txt").string();

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
  const std::string srcPath = writeTemp("compile_cpp_variadic_args_pointer_file_handle.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_variadic_args_pointer_file_handle_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 39);
  CHECK(readFile(pathA0) == "alpha\n");
  CHECK(readFile(pathA2) == "omega\n");
  CHECK(readFile(pathExtra) == "alpha\n");
  CHECK(readFile(pathC1) == "omega\n");
}

TEST_CASE("C++ emitter rejects variadic pointer string packs") {
  const std::string source = R"(
[return<int>]
score([args<Pointer<string>>] values) {
  return(count(values))
}

[return<int>]
main() {
  [string] first{"first"utf8}
  [Pointer<string>] p0{location(first)}
  score(p0)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_cpp_variadic_args_pointer_string_reject.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_cpp_variadic_args_pointer_string_reject_err.txt").string();
  const std::string compileCmd =
      "./primec --emit=cpp " + quoteShellArg(srcPath) + " -o /dev/null --entry /main 2> " + quoteShellArg(errPath);
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("variadic args<T> does not support string pointers or references") != std::string::npos);
}

TEST_CASE("C++ emitter rejects variadic reference string packs") {
  const std::string source = R"(
[return<int>]
score([args<Reference<string>>] values) {
  return(count(values))
}

[return<int>]
main() {
  [string] first{"first"utf8}
  [Reference<string>] r0{location(first)}
  score(r0)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_cpp_variadic_args_reference_string_reject.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_cpp_variadic_args_reference_string_reject_err.txt").string();
  const std::string compileCmd =
      "./primec --emit=cpp " + quoteShellArg(srcPath) + " -o /dev/null --entry /main 2> " + quoteShellArg(errPath);
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("variadic args<T> does not support string pointers or references") != std::string::npos);
}

TEST_CASE("C++ emitter rejects variadic reference packs without location forwarding") {
  const std::string source = R"(
[return<int>]
score([args<Reference<i32>>] values) {
  return(count(values))
}

[return<int>]
main() {
  return(score(1i32, 2i32))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_variadic_args_reference_forwarding_reject.prime", source);
  const std::string errPath = (std::filesystem::temp_directory_path() /
                               "primec_cpp_variadic_args_reference_forwarding_reject_err.txt")
                                  .string();
  const std::string compileCmd =
      "./primec --emit=cpp " + quoteShellArg(srcPath) + " -o /dev/null --entry /main 2> " + quoteShellArg(errPath);
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find(
            "variadic args<Reference<T>> requires reference values or location(...) forwarding") !=
        std::string::npos);
}

TEST_CASE("C++ emitter rejects variadic pointer packs without location forwarding") {
  const std::string source = R"(
[return<int>]
score([args<Pointer<i32>>] values) {
  return(count(values))
}

[return<int>]
main() {
  return(score(1i32, 2i32))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_variadic_args_pointer_forwarding_reject.prime", source);
  const std::string errPath = (std::filesystem::temp_directory_path() /
                               "primec_cpp_variadic_args_pointer_forwarding_reject_err.txt")
                                  .string();
  const std::string compileCmd =
      "./primec --emit=cpp " + quoteShellArg(srcPath) + " -o /dev/null --entry /main 2> " + quoteShellArg(errPath);
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find(
            "variadic args<Pointer<T>> requires pointer values or location(...) forwarding") !=
        std::string::npos);
}

TEST_CASE("C++ emitter materializes variadic borrowed experimental map packs with indexed dereference count calls") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<int> effects(heap_alloc)]
score_refs([args<Reference<Map<i32, i32>>>] values) {
  return(plus(/std/collections/experimental_map/mapCount<i32, i32>(dereference(values[0i32])),
              /std/collections/experimental_map/mapCount<i32, i32>(dereference(values[2i32]))))
}

[return<int> effects(heap_alloc)]
forward([args<Reference<Map<i32, i32>>>] values) {
  return(score_refs([spread] values))
}

[return<int> effects(heap_alloc)]
forward_mixed([args<Reference<Map<i32, i32>>>] values) {
  [Map<i32, i32>] extra{mapSingle(1i32, 2i32)}
  return(score_refs(location(extra), [spread] values))
}

[return<int> effects(heap_alloc)]
main() {
  [Map<i32, i32>] a0{mapPair(1i32, 2i32, 3i32, 4i32)}
  [Map<i32, i32>] a1{mapSingle(5i32, 6i32)}
  [Map<i32, i32>] a2{mapTriple(7i32, 8i32, 9i32, 10i32, 11i32, 12i32)}
  [Map<i32, i32>] b0{mapSingle(13i32, 14i32)}
  [Map<i32, i32>] b1{mapPair(15i32, 16i32, 17i32, 18i32)}
  [Map<i32, i32>] b2{mapSingle(19i32, 20i32)}
  [Map<i32, i32>] c0{mapPair(21i32, 22i32, 23i32, 24i32)}
  [Map<i32, i32>] c1{mapTriple(25i32, 26i32, 27i32, 28i32, 29i32, 30i32)}
  return(plus(score_refs(location(a0), location(a1), location(a2)),
              plus(forward(location(b0), location(b1), location(b2)),
                   forward_mixed(location(c0), location(c1)))))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_variadic_args_borrowed_experimental_map_count.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_cpp_variadic_args_borrowed_experimental_map_count_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 11);
}

TEST_CASE("C++ emitter materializes variadic scalar pointer packs from borrowed locations") {
  const std::string source = R"(
[return<Reference<i32>>]
borrow_ref([Reference<i32>] value) {
  return(value)
}

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
  return(score_ptrs(location(borrow_ref(extra_ref)), [spread] values))
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

  return(plus(score_ptrs(location(borrow_ref(r0)), location(borrow_ref(r1)), location(borrow_ref(r2))),
              plus(forward(location(borrow_ref(s0)), location(borrow_ref(s1)), location(borrow_ref(s2))),
                   forward_mixed(location(borrow_ref(t0)), location(borrow_ref(t1))))))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_variadic_args_scalar_pointer_borrowed_location.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_cpp_variadic_args_scalar_pointer_borrowed_location_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 23);
}

TEST_CASE("C++ emitter materializes variadic struct pointer packs from borrowed locations") {
  const std::string source = R"(
[struct]
Pair() {
  [i32] value{0i32}
}

[return<int>]
/Pair/score([Pair] self) {
  return(plus(self.value, 1i32))
}

[return<Reference<Pair>>]
borrow_ref([Reference<Pair>] value) {
  return(value)
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
  return(score_ptrs(location(borrow_ref(extra_ref)), [spread] values))
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

  return(plus(score_ptrs(location(borrow_ref(r0)), location(borrow_ref(r1)), location(borrow_ref(r2))),
              plus(forward(location(borrow_ref(s0)), location(borrow_ref(s1)), location(borrow_ref(s2))),
                   forward_mixed(location(borrow_ref(t0)), location(borrow_ref(t1))))))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_variadic_args_struct_pointer_borrowed_location.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_cpp_variadic_args_struct_pointer_borrowed_location_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 65);
}

TEST_CASE("C++ emitter materializes variadic scalar pointer packs from imported helper references") {
  const std::string source = R"(
import /util/*

namespace util {
  [public return<Reference<i32>>]
  borrow_ref([Reference<i32>] value) {
    return(value)
  }
}

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
  return(score_ptrs(location(borrow_ref(extra_ref)), [spread] values))
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

  return(plus(score_ptrs(location(borrow_ref(r0)), location(borrow_ref(r1)), location(borrow_ref(r2))),
              plus(forward(location(borrow_ref(s0)), location(borrow_ref(s1)), location(borrow_ref(s2))),
                   forward_mixed(location(borrow_ref(t0)), location(borrow_ref(t1))))))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_variadic_args_scalar_pointer_imported_helper_ref.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_cpp_variadic_args_scalar_pointer_imported_helper_ref_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 23);
}

TEST_CASE("C++ emitter materializes variadic struct pointer packs from imported helper references") {
  const std::string source = R"(
import /util/*

[struct]
Pair() {
  [i32] value{0i32}
}

[return<int>]
/Pair/score([Pair] self) {
  return(plus(self.value, 1i32))
}

namespace util {
  [public return<Reference<Pair>>]
  borrow_ref([Reference<Pair>] value) {
    return(value)
  }
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
  return(score_ptrs(location(borrow_ref(extra_ref)), [spread] values))
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

  return(plus(score_ptrs(location(borrow_ref(r0)), location(borrow_ref(r1)), location(borrow_ref(r2))),
              plus(forward(location(borrow_ref(s0)), location(borrow_ref(s1)), location(borrow_ref(s2))),
                   forward_mixed(location(borrow_ref(t0)), location(borrow_ref(t1))))))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_variadic_args_struct_pointer_imported_helper_ref.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_cpp_variadic_args_struct_pointer_imported_helper_ref_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 65);
}


TEST_SUITE_END();
