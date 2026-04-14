#include "test_compile_run_helpers.h"

#include "primec/IrSerializer.h"

TEST_SUITE_BEGIN("primestruct.compile.run.vm.outputs");


TEST_CASE("writes serialized ir output") {
  const std::string source = R"(
[return<int>]
main() {
  return(3i32)
}
)";
  const std::string srcPath = writeTemp("compile_ir_simple.prime", source);
  const std::string irPath = (testScratchPath("") / "primec_ir_simple.psir").string();
  const std::string compileCmd = "./primec --emit=ir " + srcPath + " -o " + irPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  std::ifstream file(irPath, std::ios::binary);
  REQUIRE(file.good());
  std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  primec::IrModule module;
  std::string error;
  REQUIRE(primec::deserializeIr(data, module, error));
  CHECK(error.empty());
  CHECK(module.entryIndex == 0);
  REQUIRE(module.functions.size() == 1);
  CHECK(module.functions[0].name == "/main");
}

TEST_CASE("no transforms rejects sugar") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32+2i32)
}
)";
  const std::string srcPath = writeTemp("compile_no_transforms.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_no_transforms_exe").string();

  const std::string compileCmd = "./primec --emit=cpp --no-transforms " + srcPath + " -o " + exePath;
  CHECK(runCommand(compileCmd) != 0);
}

TEST_CASE("no transforms rejects implicit utf8") {
  const std::string source = R"(
[return<int> effects(io_out)]
main() {
  print_line("implicit")
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_no_transforms_implicit_utf8.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_no_transforms_implicit_utf8_err.txt").string();
  const std::string compileCmd =
      "./primec --emit=cpp --no-transforms " + srcPath + " -o /dev/null 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("canonical string literal requires utf8/ascii suffix") != std::string::npos);
}

TEST_CASE("writes outputs under out dir") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("compile_out_dir.prime", source);
  const std::filesystem::path outDir = testScratchPath("") / "primec_out_dir_test";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  const std::string outFile = "primec_out_dir.cpp";
  const std::string expectedPath = (outDir / outFile).string();
  const std::string compileCmd =
      "./primec --emit=cpp " + srcPath + " -o " + outFile + " --out-dir " + outDir.string();
  CHECK(runCommand(compileCmd) == 0);
  CHECK(std::filesystem::exists(expectedPath));
}

TEST_CASE("runs vm file io") {
  const std::string outPath = (testScratchPath("") / "primec_vm_file_io.txt").string();
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
  const std::string srcPath = writeTemp("vm_file_io.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "Hello 123 world\n\nABC");
}

TEST_CASE("runs vm file read_byte with deterministic eof") {
  const std::string inPath = (testScratchPath("") / "primec_vm_file_read_byte.txt").string();
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
  const std::string srcPath = writeTemp("vm_file_read_byte.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_vm_file_read_byte_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "65\n66\nEOF\n99\n");
}

TEST_CASE("runs vm image api contract deterministically") {
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
  const std::string srcPath = writeTemp("vm_image_api_unsupported.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_vm_image_api_unsupported.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) ==
        "image_invalid_operation\n"
        "image_invalid_operation\n"
        "image_read_unsupported\n"
        "image_invalid_operation\n");
}

TEST_CASE("runs vm ppm read for ascii p3 inputs") {
  const std::string inPath = (testScratchPath("") / "primec_vm_image_read.ppm").string();
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
  const std::string srcPath = writeTemp("vm_image_read_ppm.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_vm_image_read_ppm.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 3);
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

TEST_CASE("runs vm ppm read for binary p6 inputs") {
  const std::string inPath = (testScratchPath("") / "primec_vm_image_read_p6.ppm").string();
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
  const std::string srcPath = writeTemp("vm_image_read_p6.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_vm_image_read_p6.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 3);
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

TEST_CASE("rejects truncated vm binary ppm reads deterministically") {
  const std::string inPath =
      (testScratchPath("") / "primec_vm_image_read_p6_truncated.ppm").string();
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
  const std::string srcPath = writeTemp("vm_image_read_p6_truncated.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_vm_image_read_p6_truncated.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) ==
        "image_invalid_operation\n"
        "0\n"
        "0\n"
        "0\n");
}

TEST_CASE("rejects oversized vm image read dimensions before overflow") {
  const std::string ppmInPath =
      (testScratchPath("") / "primec_vm_image_read_overflow.ppm").string();
  {
    std::ofstream file(ppmInPath, std::ios::binary);
    REQUIRE(file.good());
    file << "P3\n1431655766 1\n255\n7 9\n";
    REQUIRE(file.good());
  }

  const std::string pngInPath =
      (testScratchPath("") / "primec_vm_image_read_overflow.png").string();
  {
    const std::vector<unsigned char> pngBytes = withValidPngCrcs({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x55, 0x55, 0x55, 0x56, 0x00, 0x00, 0x00, 0x01,
        0x08, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x0e, 0x49, 0x44, 0x41, 0x54,
        0x78, 0x01, 0x01, 0x03, 0x00, 0xfc, 0xff, 0x00,
        0x07, 0x09, 0x00, 0x1a, 0x00, 0x11, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44,
        0x00, 0x00, 0x00, 0x00,
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

  const std::string srcPath = writeTemp("vm_image_read_overflow.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_vm_image_read_overflow.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 0);
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

TEST_CASE("runs vm ppm write for ascii p3 outputs" * doctest::skip(true)) {
  const std::filesystem::path outPath = testScratchPath("") / "primec_vm_image_write.ppm";
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
  const std::string srcPath = writeTemp("vm_image_write_ppm.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 0);
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

TEST_CASE("rejects invalid vm ppm write inputs deterministically" * doctest::skip(true)) {
  const std::filesystem::path outPath = testScratchPath("") / "primec_vm_image_write_invalid.ppm";
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
  const std::string srcPath = writeTemp("vm_image_write_invalid.prime", source);
  const std::string stdoutPath =
      (testScratchPath("") / "primec_vm_image_write_invalid.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + stdoutPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(stdoutPath) == "image_invalid_operation\n");
  CHECK(!std::filesystem::exists(outPath));
}

TEST_CASE("runs vm png write for deterministic rgb outputs" * doctest::skip(true)) {
  const std::filesystem::path outPath = testScratchPath("") / "primec_vm_image_write.png";
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
  const std::string srcPath = writeTemp("vm_image_write_png.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 0);
  const std::vector<unsigned char> expected = withValidPngCrcs({
      0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
      0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
      0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01,
      0x08, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x12, 0x49, 0x44, 0x41, 0x54,
      0x78, 0x01, 0x01, 0x07, 0x00, 0xf8, 0xff, 0x00,
      0xff, 0x00, 0x00, 0x00, 0xff, 0x80, 0x08, 0x7f,
      0x02, 0x7f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0x00, 0x00,
      0x00, 0x00,
  });
  CHECK(readBinaryFile(outPath.string()) == expected);
}

TEST_CASE("rejects invalid vm png write inputs deterministically" * doctest::skip(true)) {
  const std::filesystem::path outPath = testScratchPath("") / "primec_vm_image_write_invalid.png";
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
  const std::string srcPath = writeTemp("vm_image_write_invalid_png.prime", source);
  const std::string stdoutPath =
      (testScratchPath("") / "primec_vm_image_write_invalid_png.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + stdoutPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(stdoutPath) == "image_invalid_operation\n");
  CHECK(!std::filesystem::exists(outPath));
}

TEST_CASE("rejects oversized vm image write dimensions before overflow" * doctest::skip(true)) {
  const std::filesystem::path ppmOutPath =
      testScratchPath("") / "primec_vm_image_write_overflow.ppm";
  const std::filesystem::path pngOutPath =
      testScratchPath("") / "primec_vm_image_write_overflow.png";
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

  const std::string srcPath = writeTemp("vm_image_write_overflow.prime", source);
  const std::string stdoutPath =
      (testScratchPath("") / "primec_vm_image_write_overflow.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + stdoutPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(stdoutPath) ==
        "image_invalid_operation\n"
        "image_invalid_operation\n");
  CHECK(!std::filesystem::exists(ppmOutPath));
  CHECK(!std::filesystem::exists(pngOutPath));
}


TEST_CASE("runs vm png read for stored rgba inputs deterministically" * doctest::skip(true)) {
  const std::string inPath = (testScratchPath("") / "primec_vm_image_read.png").string();
  {
    const std::vector<unsigned char> pngBytes = withValidPngCrcs({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
        0x08, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x10, 0x49, 0x44, 0x41, 0x54,
        0x78, 0x01, 0x01, 0x05, 0x00, 0xfa, 0xff, 0x00,
        0x00, 0xff, 0x80, 0x40, 0x04, 0x42, 0x01, 0xc0,
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
  return(plus(width, height))
}
)", escapedPath);
  const std::string srcPath = writeTemp("vm_image_read_png.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_vm_image_read_png.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(outPath) ==
        "1\n"
        "1\n"
        "3\n"
        "0\n"
        "255\n"
        "128\n");
}

TEST_CASE("runs vm png read for stored sub-filter rgba inputs deterministically" * doctest::skip(true)) {
  const std::string inPath = (testScratchPath("") / "primec_vm_image_read_sub.png").string();
  {
    const std::vector<unsigned char> pngBytes = withValidPngCrcs({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01,
        0x08, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x14, 0x49, 0x44, 0x41, 0x54,
        0x78, 0x01, 0x01, 0x09, 0x00, 0xf6, 0xff, 0x01,
        0x0a, 0x14, 0x1e, 0x28, 0x05, 0x07, 0x09, 0x0b,
        0x02, 0xb0, 0x00, 0x86, 0x00, 0x00, 0x00, 0x00,
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
  const std::string srcPath = writeTemp("vm_image_read_sub_png.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_vm_image_read_sub_png.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(outPath) ==
        "2\n"
        "1\n"
        "6\n"
        "10\n"
        "20\n"
        "30\n"
        "15\n"
        "27\n"
        "39\n");
}

TEST_CASE("runs vm png read for stored up-filter rgba inputs deterministically" * doctest::skip(true)) {
  const std::string inPath = (testScratchPath("") / "primec_vm_image_read_up.png").string();
  {
    const std::vector<unsigned char> pngBytes = withValidPngCrcs({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02,
        0x08, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x15, 0x49, 0x44, 0x41, 0x54,
        0x78, 0x01, 0x01, 0x0a, 0x00, 0xf5, 0xff, 0x00,
        0x0a, 0x14, 0x1e, 0xff, 0x02, 0x05, 0x07, 0x09,
        0x00, 0x08, 0x15, 0x01, 0x53, 0x00, 0x00, 0x00,
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
  return(plus(width, height))
}
)", escapedPath);
  const std::string srcPath = writeTemp("vm_image_read_up_png.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_vm_image_read_up_png.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 3);
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

TEST_CASE("runs vm png read for stored average-filter rgba inputs deterministically" * doctest::skip(true)) {
  const std::string inPath =
      (testScratchPath("") / "primec_vm_image_read_average.png").string();
  {
    const std::vector<unsigned char> pngBytes = withValidPngCrcs({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02,
        0x08, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x15, 0x49, 0x44, 0x41, 0x54,
        0x78, 0x01, 0x01, 0x0a, 0x00, 0xf5, 0xff, 0x00,
        0x0a, 0x14, 0x1e, 0xff, 0x03, 0x0a, 0x11, 0x18,
        0x80, 0x08, 0xea, 0x01, 0xf2, 0x00, 0x00, 0x00,
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
  return(plus(width, height))
}
)", escapedPath);
  const std::string srcPath = writeTemp("vm_image_read_average_png.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_vm_image_read_average_png.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 3);
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

TEST_CASE("runs vm png read for fixed-huffman backreference rgba inputs deterministically" * doctest::skip(true)) {
  const std::string inPath = (testScratchPath("") / "primec_vm_image_read_fixed_sub.png").string();
  {
    const std::vector<unsigned char> pngBytes = withValidPngCrcs({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x01,
        0x08, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x0e, 0x49, 0x44, 0x41, 0x54,
        0x78, 0x01, 0x63, 0xe0, 0x12, 0x91, 0xd3, 0x80,
        0x61, 0x00, 0x07, 0x15, 0x01, 0x2d, 0x00, 0x00,
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
  print_line(pixels[6i32])
  print_line(pixels[7i32])
  print_line(pixels[8i32])
  return(plus(width, height))
}
)", escapedPath);
  const std::string srcPath = writeTemp("vm_image_read_fixed_sub_png.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_vm_image_read_fixed_sub_png.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 4);
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

TEST_CASE("rejects vm png read for dynamic-huffman literal rgb inputs deterministically" * doctest::skip(true)) {
  const std::string inPath =
      (testScratchPath("") / "primec_vm_image_read_dynamic_literal.png").string();
  {
    const std::vector<unsigned char> pngBytes = withValidPngCrcs({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01,
        0x08, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x19, 0x49, 0x44, 0x41, 0x54,
        0x78, 0x01, 0x05, 0xc0, 0xa1, 0x00, 0x00, 0x00,
        0x00, 0x03, 0xb0, 0xe7, 0xe7, 0xe7, 0xe7, 0xe7,
        0xe7, 0xfb, 0x73, 0x7d, 0xa0, 0x9c, 0xee, 0x02,
        0x37, 0x00, 0xd3, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x49, 0x45, 0x4e, 0x44, 0x00, 0x00, 0x00, 0x00,
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
  const std::string srcPath = writeTemp("vm_image_read_dynamic_literal_png.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_vm_image_read_dynamic_literal_png.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 1);
  CHECK(readFile(outPath) == "image_invalid_operation\n");
}

TEST_CASE("runs vm png read for dynamic-huffman backreference rgba inputs deterministically" * doctest::skip(true)) {
  const std::string inPath =
      (testScratchPath("") / "primec_vm_image_read_dynamic_backref.png").string();
  {
    const std::vector<unsigned char> pngBytes = withValidPngCrcs({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x01,
        0x08, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x1e, 0x49, 0x44, 0x41, 0x54,
        0x78, 0x01, 0x35, 0xc3, 0xa1, 0x00, 0x00, 0x00,
        0x0c, 0xc0, 0x80, 0xff, 0xff, 0xff, 0xff, 0xf9,
        0xf9, 0xfe, 0x96, 0x7f, 0xfa, 0xb6, 0x41, 0xf5,
        0xec, 0x7f, 0x13, 0xae, 0x03, 0xb2, 0x00, 0x00,
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
  print_line(pixels[6i32])
  print_line(pixels[7i32])
  print_line(pixels[8i32])
  return(plus(width, height))
}
)", escapedPath);
  const std::string srcPath = writeTemp("vm_image_read_dynamic_backref_png.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_vm_image_read_dynamic_backref_png.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 4);
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

TEST_CASE("rejects malformed vm png inputs deterministically" * doctest::skip(true)) {
  const std::string inPath = (testScratchPath("") / "primec_vm_image_read_invalid.png").string();
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
  const std::string srcPath = writeTemp("vm_image_read_invalid_png.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_vm_image_read_invalid_png.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) ==
        "image_invalid_operation\n"
        "0\n"
        "0\n"
        "0\n");
}

TEST_CASE("rejects vm png inputs with critical chunk crc mismatches deterministically" * doctest::skip(true)) {
  const std::string inPath =
      (testScratchPath("") / "primec_vm_image_read_invalid_crc.png").string();
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
  const std::string srcPath = writeTemp("vm_image_read_invalid_crc_png.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_vm_image_read_invalid_crc_png.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) ==
        "image_invalid_operation\n"
        "0\n"
        "0\n"
        "0\n");
}

TEST_CASE("rejects vm png inputs with non-consecutive idat chunks deterministically" * doctest::skip(true)) {
  const std::string inPath =
      (testScratchPath("") / "primec_vm_image_read_invalid_idat_order.png").string();
  {
    const std::vector<unsigned char> pngBytes = withValidPngCrcs({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
        0x08, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x08, 0x49, 0x44, 0x41, 0x54,
        0x78, 0x01, 0x01, 0x04, 0x00, 0xfb, 0xff, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x72, 0x75, 0x53, 0x74,
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
  const std::string srcPath = writeTemp("vm_image_read_invalid_idat_order_png.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_vm_image_read_invalid_idat_order_png.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) ==
        "image_invalid_operation\n"
        "0\n"
        "0\n"
        "0\n");
}


TEST_CASE("defaults to cpp extension for emit=cpp") {
  const std::string source = R"(
[return<int>]
main() {
  return(2i32)
}
)";
  const std::string srcPath = writeTemp("compile_default_cpp.prime", source);
  const std::filesystem::path outDir = testScratchPath("") / "primec_cpp_default_out";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::string compileCmd =
      "./primec --emit=cpp " + srcPath + " --out-dir " + outDir.string() + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  std::filesystem::path outputPath = outDir / std::filesystem::path(srcPath).stem();
  outputPath.replace_extension(".cpp");
  CHECK(std::filesystem::exists(outputPath));
}

TEST_CASE("defaults to cpp extension for emit=cpp-ir") {
  const std::string source = R"(
[return<int>]
main() {
  return(4i32)
}
)";
  const std::string srcPath = writeTemp("compile_default_cpp_ir.prime", source);
  const std::filesystem::path outDir = testScratchPath("") / "primec_cpp_ir_default_out";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::string compileCmd =
      "./primec --emit=cpp-ir " + srcPath + " --out-dir " + outDir.string() + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  std::filesystem::path outputPath = outDir / std::filesystem::path(srcPath).stem();
  outputPath.replace_extension(".cpp");
  CHECK(std::filesystem::exists(outputPath));
}

static bool vmIrBackendParitySupported() {
  static int cached = -1;
  if (cached != -1) {
    return cached == 1;
  }

  const std::string source = R"(
[return<int>]
main() {
  [f64] left{1.0f64}
  [f64] right{2.0f64}
  if(less_than(left, right), then() { return(7i32) }, else() { return(0i32) })
}
)";
  const std::string srcPath = writeTemp("compile_cpp_ir_backend_probe.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_cpp_ir_backend_probe.cpp").string();
  const std::string compileCmd = "./primec --emit=cpp-ir " + quoteShellArg(srcPath) + " -o " + quoteShellArg(outPath) +
                                 " --entry /main";
  const int code = runCommand(compileCmd);
  cached = (code == 0) ? 1 : 0;
  return cached == 1;
}

#define SKIP_IF_VM_IR_BACKEND_LIMITED()                                                                       \
  if (!vmIrBackendParitySupported()) {                                                                        \
    INFO("Skipping vm ir backend parity checks until ir-to-cpp backend supports this corpus");               \
    CHECK(true);                                                                                              \
    return;                                                                                                   \
  }

TEST_CASE("cpp-ir emitter writes IR-lowered cpp for integer subset") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] counter{1i32}
  assign(counter, plus(counter, 2i32))
  return(counter)
}
)";
  const std::string srcPath = writeTemp("compile_cpp_ir_i32_subset.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_cpp_ir_i32_subset.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp-ir " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("int64_t ps_entry_0(int argc, char **argv)") != std::string::npos);
  CHECK(output.find("switch (pc)") != std::string::npos);
}

TEST_CASE("cpp-ir emitter writes string and argv print paths") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  print_line("hello"utf8)
  print_line(args[1i32])
  return(args.count())
}
)";
  const std::string srcPath = writeTemp("compile_cpp_ir_print_argv.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_cpp_ir_print_argv.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp-ir " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("static const char *ps_string_table[]") != std::string::npos);
  CHECK(output.find("stack[sp++] = static_cast<uint64_t>(argc);") != std::string::npos);
  CHECK(output.find("argv[indexValue]") != std::string::npos);
}

TEST_CASE("cpp-ir emitter writes dynamic string print path") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int> effects(io_out)]
main() {
  [string mut] msg{"left"utf8}
  assign(msg, "right"utf8)
  print_line(msg)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_cpp_ir_dynamic_string_print.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_cpp_ir_dynamic_string_print.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp-ir " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("std::cout << ps_string_table[0];") != std::string::npos);
  CHECK(output.find("static constexpr std::size_t ps_string_table_count = 2u;") != std::string::npos);
}

TEST_CASE("cpp-ir emitter writes string indexing paths") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
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
  const std::string srcPath = writeTemp("compile_cpp_ir_string_indexing.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_cpp_ir_string_indexing.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp-ir " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("if (stringByteIndex >= 3ull)") != std::string::npos);
  CHECK(output.find("ps_string_table[0][stringByteIndex]") != std::string::npos);
}

TEST_CASE("cpp emitter uses ir backend for string indexing") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
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
  const std::string srcPath = writeTemp("compile_cpp_string_indexing_ir_first.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_cpp_string_indexing_ir_first.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("ps_string_table[0][stringByteIndex]") != std::string::npos);
  CHECK(output.find("ps_entry_0") != std::string::npos);
}

TEST_CASE("cpp-ir emitter writes pointer indirect paths") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{3i32}
  [Reference<i32> mut] ref{location(value)}
  assign(ref, 8i32)
  return(ref)
}
)";
  const std::string srcPath = writeTemp("compile_cpp_ir_pointer_indirect.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_cpp_ir_pointer_indirect.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp-ir " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("loadIndirectAddress % 16ull") != std::string::npos);
  CHECK(output.find("storeIndirectAddress % 16ull") != std::string::npos);
  CHECK(output.find("locals[storeIndirectIndex] = storeIndirectValue;") != std::string::npos);
}

TEST_CASE("cpp emitter uses ir backend for pointer indirect paths") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{3i32}
  [Reference<i32> mut] ref{location(value)}
  assign(ref, 8i32)
  return(ref)
}
)";
  const std::string srcPath = writeTemp("compile_cpp_pointer_indirect_ir_first.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_cpp_pointer_indirect_ir_first.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("loadIndirectAddress % 16ull") != std::string::npos);
  CHECK(output.find("ps_entry_0") != std::string::npos);
}

TEST_CASE("cpp-ir emitter writes callable dispatch paths") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<void> effects(io_out)]
logCall() {
  print_line("log"utf8)
}

[return<int>]
value() {
  return(41i32)
}

[return<int> effects(io_out)]
main() {
  logCall()
  return(plus(value(), 1i32))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_ir_calls.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_cpp_ir_calls.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp-ir " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find(
            "static int64_t ps_fn_0(PsStack &stack, std::size_t &sp, std::vector<uint64_t> &heapSlots, "
            "std::vector<PsHeapAllocation> &heapAllocations, int argc, char **argv);") != std::string::npos);
  CHECK(output.find(
            "static int64_t ps_fn_1(PsStack &stack, std::size_t &sp, std::vector<uint64_t> &heapSlots, "
            "std::vector<PsHeapAllocation> &heapAllocations, int argc, char **argv);") != std::string::npos);
  CHECK(output.find("return ps_fn_0(stack, sp, heapSlots, heapAllocations, argc, argv);") != std::string::npos);
}

TEST_CASE("cpp-ir emitter writes file read paths") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<Result<FileError>> effects(file_write) on_error<FileError, /log_file_error>]
main() {
  [File<Read>] file{File<Read>("/dev/null"utf8)?}
  file.close()?
  return(Result.ok())
}
[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error("file error"utf8)
}
)";
  const std::string srcPath = writeTemp("compile_cpp_ir_file_read_subset.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_cpp_ir_file_read_subset.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp-ir " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("int fileOpenFlags = O_RDONLY;") != std::string::npos);
  CHECK(output.find("int closeRc = ::close(closeFd);") != std::string::npos);
}

TEST_CASE("cpp-ir emitter writes file io paths") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<Result<FileError>> effects(file_write) on_error<FileError, /log_file_error>]
main() {
  [File<Write>] file{File<Write>("/tmp/primec_cpp_ir_file_io.txt"utf8)?}
  file.write("x"utf8)?
  file.flush()?
  file.close()?
  return(Result.ok())
}
[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error("file error"utf8)
}
)";
  const std::string srcPath = writeTemp("compile_cpp_ir_file_io_subset.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_cpp_ir_file_io_subset.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp-ir " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("static uint32_t psWriteAll(int fd, const void *data, std::size_t size)") != std::string::npos);
  CHECK(output.find("int fileFd = ::open(ps_string_table[0], fileOpenFlags, 0644);") != std::string::npos);
  CHECK(output.find("int closeRc = ::close(closeFd);") != std::string::npos);
}

TEST_CASE("cpp emitter uses ir backend for file io subset") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<Result<FileError>> effects(file_write) on_error<FileError, /log_file_error>]
main() {
  [File<Write>] file{File<Write>("/tmp/primec_cpp_file_io.txt"utf8)?}
  file.write("x"utf8)?
  file.flush()?
  file.close()?
  return(Result.ok())
}
[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error("file error"utf8)
}
)";
  const std::string srcPath = writeTemp("compile_cpp_file_io_ir_first.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_cpp_file_io_ir_first.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("static uint32_t psWriteAll(int fd, const void *data, std::size_t size)") != std::string::npos);
  CHECK(output.find("ps_entry_0") != std::string::npos);
}

TEST_CASE("cpp-ir emitter writes f64 arithmetic paths") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int>]
main() {
  [f64 mut] value{2.0f64}
  assign(value, plus(value, 0.5f64))
  if(greater_than(value, 2.4f64), then() { return(7i32) }, else() { return(3i32) })
}
)";
  const std::string srcPath = writeTemp("compile_cpp_ir_f64_math_subset.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_cpp_ir_f64_math_subset.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp-ir " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("static uint64_t psF64ToBits(double value)") != std::string::npos);
  CHECK(output.find("stack[sp++] = psF64ToBits(left + right);") != std::string::npos);
}

TEST_CASE("cpp emitter uses ir backend for f64 arithmetic subset") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int>]
main() {
  [f64 mut] value{2.0f64}
  assign(value, plus(value, 0.5f64))
  if(greater_than(value, 2.4f64), then() { return(7i32) }, else() { return(3i32) })
}
)";
  const std::string srcPath = writeTemp("compile_cpp_f64_math_ir_first.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_cpp_f64_math_ir_first.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("stack[sp++] = psF64ToBits(left + right);") != std::string::npos);
  CHECK(output.find("ps_entry_0") != std::string::npos);
}


TEST_CASE("cpp-ir emitter writes f64 conversion paths") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<i64>]
main() {
  [i32] base{7i32}
  [f64] widened{convert<f64>(base)}
  [f32] narrowed{convert<f32>(widened)}
  [f64] roundTrip{convert<f64>(narrowed)}
  return(convert<i64>(roundTrip))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_ir_f64_convert_subset.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_cpp_ir_f64_convert_subset.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp-ir " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("stack[sp++] = psF64ToBits(static_cast<double>(value));") != std::string::npos);
  CHECK(output.find("stack[sp++] = psF32ToBits(static_cast<float>(value));") != std::string::npos);
  CHECK(output.find("static int64_t psConvertF64ToI64(double value)") != std::string::npos);
  CHECK(output.find("int64_t converted = psConvertF64ToI64(value);") != std::string::npos);
}

TEST_CASE("cpp emitter uses ir backend for f64 conversion subset") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<i64>]
main() {
  [i32] base{7i32}
  [f64] widened{convert<f64>(base)}
  [f32] narrowed{convert<f32>(widened)}
  [f64] roundTrip{convert<f64>(narrowed)}
  return(convert<i64>(roundTrip))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_f64_convert_ir_first.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_cpp_f64_convert_ir_first.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("stack[sp++] = psF64ToBits(static_cast<double>(value));") != std::string::npos);
  CHECK(output.find("static int64_t psConvertF64ToI64(double value)") != std::string::npos);
  CHECK(output.find("int64_t converted = psConvertF64ToI64(value);") != std::string::npos);
  CHECK(output.find("ps_entry_0") != std::string::npos);
}

TEST_CASE("cpp-ir emitter writes f64 to i32 conversion paths") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int>]
main() {
  return(convert<int>(2.5f64))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_ir_f64_to_i32_convert.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_cpp_ir_f64_to_i32_convert.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp-ir " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("static int32_t psConvertF64ToI32(double value)") != std::string::npos);
  CHECK(output.find("int32_t converted = psConvertF64ToI32(value);") != std::string::npos);
}

TEST_CASE("cpp emitter uses ir backend for f64 to i32 conversion") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int>]
main() {
  return(convert<int>(2.5f64))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_f64_to_i32_ir_first.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_cpp_f64_to_i32_ir_first.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("static int32_t psConvertF64ToI32(double value)") != std::string::npos);
  CHECK(output.find("int32_t converted = psConvertF64ToI32(value);") != std::string::npos);
  CHECK(output.find("ps_entry_0") != std::string::npos);
}

TEST_CASE("cpp-ir emitter writes f32 to i64 conversion paths") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<i64>]
main() {
  return(convert<i64>(2.5f32))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_ir_f32_to_i64_convert.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_cpp_ir_f32_to_i64_convert.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp-ir " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("static int64_t psConvertF32ToI64(float value)") != std::string::npos);
  CHECK(output.find("int64_t converted = psConvertF32ToI64(value);") != std::string::npos);
}

TEST_CASE("cpp emitter uses ir backend for f32 to i64 conversion") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<i64>]
main() {
  return(convert<i64>(2.5f32))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_f32_to_i64_ir_first.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_cpp_f32_to_i64_ir_first.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("static int64_t psConvertF32ToI64(float value)") != std::string::npos);
  CHECK(output.find("int64_t converted = psConvertF32ToI64(value);") != std::string::npos);
  CHECK(output.find("ps_entry_0") != std::string::npos);
}

TEST_CASE("cpp-ir emitter writes f64 to i64 conversion paths") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<i64>]
main() {
  return(convert<i64>(2.5f64))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_ir_f64_to_i64_convert.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_cpp_ir_f64_to_i64_convert.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp-ir " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("static int64_t psConvertF64ToI64(double value)") != std::string::npos);
  CHECK(output.find("int64_t converted = psConvertF64ToI64(value);") != std::string::npos);
}

TEST_CASE("cpp emitter uses ir backend for f64 to i64 conversion") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<i64>]
main() {
  return(convert<i64>(2.5f64))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_f64_to_i64_ir_first.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_cpp_f64_to_i64_ir_first.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("static int64_t psConvertF64ToI64(double value)") != std::string::npos);
  CHECK(output.find("int64_t converted = psConvertF64ToI64(value);") != std::string::npos);
  CHECK(output.find("ps_entry_0") != std::string::npos);
}

TEST_CASE("cpp-ir emitter writes f64 to u64 conversion paths") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<u64>]
main() {
  return(convert<u64>(2.5f64))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_ir_f64_to_u64_convert.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_cpp_ir_f64_to_u64_convert.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp-ir " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("static uint64_t psConvertF64ToU64(double value)") != std::string::npos);
  CHECK(output.find("uint64_t converted = psConvertF64ToU64(value);") != std::string::npos);
}

TEST_CASE("cpp emitter uses ir backend for f64 to u64 conversion") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<u64>]
main() {
  return(convert<u64>(2.5f64))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_f64_to_u64_ir_first.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_cpp_f64_to_u64_ir_first.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("static uint64_t psConvertF64ToU64(double value)") != std::string::npos);
  CHECK(output.find("uint64_t converted = psConvertF64ToU64(value);") != std::string::npos);
  CHECK(output.find("ps_entry_0") != std::string::npos);
}

TEST_CASE("cpp-ir emitter writes f64 comparison paths") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int>]
main() {
  if(greater_than(2.5f64, 1.0f64), then() { return(7i32) }, else() { return(3i32) })
}
)";
  const std::string srcPath = writeTemp("compile_cpp_ir_f64_cmp_subset.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_cpp_ir_f64_cmp_subset.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp-ir " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("static double psBitsToF64(uint64_t raw)") != std::string::npos);
  CHECK(output.find("double right = psBitsToF64(stack[--sp]);") != std::string::npos);
}

TEST_CASE("cpp emitter uses ir backend for f64 comparison subset") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int>]
main() {
  if(greater_than(2.5f64, 1.0f64), then() { return(7i32) }, else() { return(3i32) })
}
)";
  const std::string srcPath = writeTemp("compile_cpp_f64_cmp_ir_first.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_cpp_f64_cmp_ir_first.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("static double psBitsToF64(uint64_t raw)") != std::string::npos);
  CHECK(output.find("ps_entry_0") != std::string::npos);
}

TEST_CASE("cpp-ir emitter writes f32 arithmetic and comparison paths") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int>]
main() {
  [f32 mut] value{2.0f32}
  assign(value, plus(value, 0.5f32))
  if(greater_than(value, 2.4f32), then() { return(7i32) }, else() { return(3i32) })
}
)";
  const std::string srcPath = writeTemp("compile_cpp_ir_f32_subset.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_cpp_ir_f32_subset.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp-ir " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("static float psBitsToF32(uint64_t raw)") != std::string::npos);
  CHECK(output.find("static uint64_t psF32ToBits(float value)") != std::string::npos);
  CHECK(output.find("float right = psBitsToF32(stack[--sp]);") != std::string::npos);
}

TEST_CASE("cpp emitter uses ir backend for f32 arithmetic subset") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int>]
main() {
  [f32 mut] value{2.0f32}
  assign(value, plus(value, 0.5f32))
  if(greater_than(value, 2.4f32), then() { return(7i32) }, else() { return(3i32) })
}
)";
  const std::string srcPath = writeTemp("compile_cpp_f32_ir_first.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_cpp_f32_ir_first.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("static float psBitsToF32(uint64_t raw)") != std::string::npos);
  CHECK(output.find("ps_entry_0") != std::string::npos);
}

TEST_CASE("defaults to psir extension for emit=ir") {
  const std::string source = R"(
[return<int>]
main() {
  return(3i32)
}
)";
  const std::string srcPath = writeTemp("compile_default_ir.prime", source);
  const std::filesystem::path outDir = testScratchPath("") / "primec_ir_default_out";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::string compileCmd =
      "./primec --emit=ir " + srcPath + " --out-dir " + outDir.string() + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  std::filesystem::path outputPath = outDir / std::filesystem::path(srcPath).stem();
  outputPath.replace_extension(".psir");
  CHECK(std::filesystem::exists(outputPath));
}

TEST_CASE("cpp emitter uses ir backend for file read subset") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<Result<FileError>> effects(file_write) on_error<FileError, /log_file_error>]
main() {
  [File<Read>] file{File<Read>("/dev/null"utf8)?}
  file.close()?
  return(Result.ok())
}
[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error("file error"utf8)
}
)";
  const std::string srcPath = writeTemp("compile_cpp_file_read_ir_first.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_cpp_file_read_ir_first.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("ps_entry_0") != std::string::npos);
  CHECK(output.find("int fileOpenFlags = O_RDONLY;") != std::string::npos);
}


TEST_CASE("compiles and runs void main with local binding") {
  const std::string source = R"(
[return<void>]
main() {
  [i32] value{1i32}
}
)";
  const std::string srcPath = writeTemp("compile_void_main.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_void_main_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("exe-ir emitter compiles and runs i32 subset") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] counter{1i32}
  assign(counter, plus(counter, 2i32))
  return(counter)
}
)";
  const std::string srcPath = writeTemp("compile_exe_ir_i32_subset.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_ir_i32_subset").string();

  const std::string compileCmd = "./primec --emit=exe-ir " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("exe-ir emitter compiles and runs i64 subset") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<i64>]
main() {
  [i64 mut] counter{10i64}
  assign(counter, plus(counter, 5i64))
  assign(counter, minus(counter, 2i64))
  return(counter)
}
)";
  const std::string srcPath = writeTemp("compile_exe_ir_i64_subset.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_ir_i64_subset").string();

  const std::string compileCmd = "./primec --emit=exe-ir " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 13);
}

TEST_CASE("exe-ir emitter compiles and runs argv prints") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  print_line(args[1i32])
  return(args.count())
}
)";
  const std::string srcPath = writeTemp("compile_exe_ir_print_argv.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_ir_print_argv").string();
  const std::string outPath = (testScratchPath("") / "primec_exe_ir_print_argv.out").string();

  const std::string compileCmd = "./primec --emit=exe-ir " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " alpha beta > " + outPath) == 3);
  CHECK(readFile(outPath) == "alpha\n");
}

TEST_CASE("exe-ir emitter compiles and runs dynamic string print") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int> effects(io_out)]
main() {
  [string mut] msg{"left"utf8}
  assign(msg, "right"utf8)
  print_line(msg)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_exe_ir_dynamic_string_print.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_ir_dynamic_string_print").string();
  const std::string outPath = (testScratchPath("") / "primec_exe_ir_dynamic_string_print.out").string();

  const std::string compileCmd = "./primec --emit=exe-ir " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 0);
  CHECK(readFile(outPath) == "left\n");
}

TEST_CASE("exe-ir emitter compiles and runs string indexing") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
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
  const std::string srcPath = writeTemp("compile_exe_ir_string_indexing.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_ir_string_indexing").string();

  const std::string compileCmd = "./primec --emit=exe-ir " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == (97 + 98 + 3));
}

TEST_CASE("exe-ir emitter compiles and runs pointer indirect paths") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{3i32}
  [Reference<i32> mut] ref{location(value)}
  assign(ref, 8i32)
  return(ref)
}
)";
  const std::string srcPath = writeTemp("compile_exe_ir_pointer_indirect.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_ir_pointer_indirect").string();

  const std::string compileCmd = "./primec --emit=exe-ir " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 8);
}

TEST_CASE("exe-ir emitter compiles and runs heap alloc intrinsic") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int> effects(heap_alloc)]
main() {
  [mut] ptr{/std/intrinsics/memory/alloc<i32>(1i32)}
  assign(dereference(ptr), 9i32)
  return(dereference(ptr))
}
)";
  const std::string srcPath = writeTemp("compile_exe_ir_heap_alloc_intrinsic.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_ir_heap_alloc_intrinsic").string();

  const std::string compileCmd = "./primec --emit=exe-ir " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("exe-ir emitter compiles and runs heap free intrinsic") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int> effects(heap_alloc)]
main() {
  [mut] ptr{/std/intrinsics/memory/alloc<i32>(1i32)}
  assign(dereference(ptr), 9i32)
  [i32] value{dereference(ptr)}
  /std/intrinsics/memory/free(ptr)
  return(value)
}
)";
  const std::string srcPath = writeTemp("compile_exe_ir_heap_free_intrinsic.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_ir_heap_free_intrinsic").string();

  const std::string compileCmd = "./primec --emit=exe-ir " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("exe-ir emitter compiles and runs heap realloc intrinsic") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int> effects(heap_alloc)]
main() {
  [mut] ptr{/std/intrinsics/memory/alloc<i32>(1i32)}
  assign(dereference(ptr), 9i32)
  [Pointer<i32> mut] grown{/std/intrinsics/memory/realloc(ptr, 2i32)}
  assign(dereference(plus(grown, 16i32)), 4i32)
  [i32] sum{plus(dereference(grown), dereference(plus(grown, 16i32)))}
  /std/intrinsics/memory/free(grown)
  return(sum)
}
)";
  const std::string srcPath = writeTemp("compile_exe_ir_heap_realloc_intrinsic.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_ir_heap_realloc_intrinsic").string();

  const std::string compileCmd = "./primec --emit=exe-ir " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 13);
}

TEST_CASE("exe-ir emitter compiles and runs checked memory at intrinsic") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int> effects(heap_alloc)]
main() {
  [mut] ptr{/std/intrinsics/memory/alloc<i32>(2i32)}
  assign(dereference(ptr), 9i32)
  [mut] second{/std/intrinsics/memory/at(ptr, 1i32, 2i32)}
  assign(dereference(second), 4i32)
  [i32] sum{plus(dereference(ptr), dereference(second))}
  /std/intrinsics/memory/free(ptr)
  return(sum)
}
)";
  const std::string srcPath = writeTemp("compile_exe_ir_heap_at_intrinsic.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_ir_heap_at_intrinsic").string();

  const std::string compileCmd = "./primec --emit=exe-ir " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 13);
}

TEST_CASE("exe-ir emitter compiles and runs unchecked memory at intrinsic") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int> effects(heap_alloc)]
main() {
  [mut] ptr{/std/intrinsics/memory/alloc<i32>(2i32)}
  assign(dereference(ptr), 9i32)
  [mut] second{/std/intrinsics/memory/at_unsafe(ptr, 1i32)}
  assign(dereference(second), 4i32)
  [i32] sum{plus(dereference(ptr), dereference(second))}
  /std/intrinsics/memory/free(ptr)
  return(sum)
}
)";
  const std::string srcPath = writeTemp("compile_exe_ir_heap_at_unsafe_intrinsic.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_ir_heap_at_unsafe_intrinsic").string();

  const std::string compileCmd = "./primec --emit=exe-ir " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 13);
}

TEST_CASE("exe-ir emitter faults on checked memory at out of bounds") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int> effects(heap_alloc)]
main() {
  [mut] ptr{/std/intrinsics/memory/alloc<i32>(1i32)}
  return(dereference(/std/intrinsics/memory/at(ptr, 1i32, 1i32)))
}
)";
  const std::string srcPath = writeTemp("compile_exe_ir_heap_at_out_of_bounds.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_ir_heap_at_out_of_bounds").string();
  const std::string errPath =
      (testScratchPath("") / "primec_exe_ir_heap_at_out_of_bounds.txt").string();

  const std::string compileCmd = "./primec --emit=exe-ir " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " 2> " + errPath) != 0);
  CHECK(readFile(errPath) == "pointer index out of bounds\n");
}

TEST_CASE("exe-ir emitter faults on dereference after heap free intrinsic") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int> effects(heap_alloc)]
main() {
  [mut] ptr{/std/intrinsics/memory/alloc<i32>(1i32)}
  /std/intrinsics/memory/free(ptr)
  return(dereference(ptr))
}
)";
  const std::string srcPath = writeTemp("compile_exe_ir_heap_free_invalid_deref.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_exe_ir_heap_free_invalid_deref").string();
  const std::string errPath =
      (testScratchPath("") / "primec_exe_ir_heap_free_invalid_deref.txt").string();

  const std::string compileCmd = "./primec --emit=exe-ir " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " 2> " + errPath) != 0);
  CHECK(readFile(errPath).find("invalid indirect address in IR") != std::string::npos);
}

TEST_CASE("exe-ir emitter compiles and runs file io subset") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string outPath = (testScratchPath("") / "primec_exe_ir_file_io_subset.txt").string();
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
  const std::string srcPath = writeTemp("compile_exe_ir_file_io_subset.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_ir_file_io_subset").string();

  const std::string compileCmd = "./primec --emit=exe-ir " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
  CHECK(readFile(outPath) == "Hello 123 world\n\nABC");
}

TEST_CASE("exe-ir emitter reports misaligned pointer dereference") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{5i32}
  return(dereference(plus(location(value), 8i32)))
}
)";
  const std::string srcPath = writeTemp("compile_exe_ir_pointer_misaligned.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_ir_pointer_misaligned").string();
  const std::string errPath =
      (testScratchPath("") / "primec_exe_ir_pointer_misaligned.err").string();

  const std::string compileCmd = "./primec --emit=exe-ir " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " 2> " + errPath) == 1);
  CHECK(readFile(errPath).find("unaligned indirect address in IR") != std::string::npos);
}

TEST_CASE("exe-ir emitter compiles and runs call and callvoid paths") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<void> effects(io_out)]
logCall() {
  print_line("log"utf8)
}

[return<int>]
value() {
  return(41i32)
}

[return<int> effects(io_out)]
main() {
  logCall()
  return(plus(value(), 1i32))
}
)";
  const std::string srcPath = writeTemp("compile_exe_ir_calls.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_ir_calls").string();
  const std::string outPath = (testScratchPath("") / "primec_exe_ir_calls.out").string();

  const std::string compileCmd = "./primec --emit=exe-ir " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 42);
  CHECK(readFile(outPath) == "log\n");
}

TEST_CASE("exe-ir emitter compiles and runs f32 arithmetic subset") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int>]
main() {
  [f32 mut] value{2.0f32}
  assign(value, plus(value, 0.5f32))
  if(greater_than(value, 2.4f32), then() { return(7i32) }, else() { return(3i32) })
}
)";
  const std::string srcPath = writeTemp("compile_exe_ir_f32_subset.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_ir_f32_subset").string();

  const std::string compileCmd = "./primec --emit=exe-ir " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}


TEST_CASE("exe-ir emitter compiles and runs f64 comparison subset") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int>]
main() {
  if(greater_than(2.5f64, 1.0f64), then() { return(7i32) }, else() { return(3i32) })
}
)";
  const std::string srcPath = writeTemp("compile_exe_ir_f64_cmp_subset.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_ir_f64_cmp_subset").string();

  const std::string compileCmd = "./primec --emit=exe-ir " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("exe emitter uses ir backend for f32 arithmetic subset") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int>]
main() {
  [f32 mut] value{2.0f32}
  assign(value, plus(value, 0.5f32))
  if(greater_than(value, 2.4f32), then() { return(7i32) }, else() { return(3i32) })
}
)";
  const std::string srcPath = writeTemp("compile_exe_f32_ir_first.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_f32_ir_first").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("exe emitter uses ir backend for string indexing") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
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
  const std::string srcPath = writeTemp("compile_exe_string_indexing_ir_first.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_string_indexing_ir_first").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == (97 + 98 + 3));
}

TEST_CASE("exe emitter uses ir backend for pointer indirect paths") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{3i32}
  [Reference<i32> mut] ref{location(value)}
  assign(ref, 8i32)
  return(ref)
}
)";
  const std::string srcPath = writeTemp("compile_exe_pointer_indirect_ir_first.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_pointer_indirect_ir_first").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 8);
}

TEST_CASE("exe emitter uses ir backend for heap alloc intrinsic") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int> effects(heap_alloc)]
main() {
  [mut] ptr{/std/intrinsics/memory/alloc<i32>(1i32)}
  assign(dereference(ptr), 9i32)
  return(dereference(ptr))
}
)";
  const std::string srcPath = writeTemp("compile_exe_heap_alloc_intrinsic_ir_first.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_heap_alloc_intrinsic_ir_first").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("exe emitter uses ir backend for heap free intrinsic") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int> effects(heap_alloc)]
main() {
  [mut] ptr{/std/intrinsics/memory/alloc<i32>(1i32)}
  assign(dereference(ptr), 9i32)
  [i32] value{dereference(ptr)}
  /std/intrinsics/memory/free(ptr)
  return(value)
}
)";
  const std::string srcPath = writeTemp("compile_exe_heap_free_intrinsic_ir_first.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_heap_free_intrinsic_ir_first").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("exe emitter uses ir backend for heap realloc intrinsic") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int> effects(heap_alloc)]
main() {
  [mut] ptr{/std/intrinsics/memory/alloc<i32>(1i32)}
  assign(dereference(ptr), 9i32)
  [Pointer<i32> mut] grown{/std/intrinsics/memory/realloc(ptr, 2i32)}
  assign(dereference(plus(grown, 16i32)), 4i32)
  [i32] sum{plus(dereference(grown), dereference(plus(grown, 16i32)))}
  /std/intrinsics/memory/free(grown)
  return(sum)
}
)";
  const std::string srcPath = writeTemp("compile_exe_heap_realloc_intrinsic_ir_first.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_heap_realloc_intrinsic_ir_first").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 13);
}

TEST_CASE("exe emitter uses ir backend for checked memory at intrinsic") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int> effects(heap_alloc)]
main() {
  [mut] ptr{/std/intrinsics/memory/alloc<i32>(2i32)}
  assign(dereference(ptr), 9i32)
  [mut] second{/std/intrinsics/memory/at(ptr, 1i32, 2i32)}
  assign(dereference(second), 4i32)
  [i32] sum{plus(dereference(ptr), dereference(second))}
  /std/intrinsics/memory/free(ptr)
  return(sum)
}
)";
  const std::string srcPath = writeTemp("compile_exe_heap_at_intrinsic_ir_first.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_heap_at_intrinsic_ir_first").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 13);
}

TEST_CASE("exe emitter uses ir backend for unchecked memory at intrinsic") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int> effects(heap_alloc)]
main() {
  [mut] ptr{/std/intrinsics/memory/alloc<i32>(2i32)}
  assign(dereference(ptr), 9i32)
  [mut] second{/std/intrinsics/memory/at_unsafe(ptr, 1i32)}
  assign(dereference(second), 4i32)
  [i32] sum{plus(dereference(ptr), dereference(second))}
  /std/intrinsics/memory/free(ptr)
  return(sum)
}
)";
  const std::string srcPath = writeTemp("compile_exe_heap_at_unsafe_intrinsic_ir_first.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_heap_at_unsafe_intrinsic_ir_first").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 13);
}

TEST_CASE("exe emitter uses ir backend for file io subset") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string outPath = (testScratchPath("") / "primec_exe_file_io_ir_first.txt").string();
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
  const std::string srcPath = writeTemp("compile_exe_file_io_ir_first.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_file_io_ir_first").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
  CHECK(readFile(outPath) == "Hello 123 world\n\nABC");
}

TEST_CASE("exe-ir emitter compiles and runs f64 arithmetic subset") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int>]
main() {
  [f64 mut] value{2.0f64}
  assign(value, plus(value, 0.5f64))
  if(greater_than(value, 2.4f64), then() { return(7i32) }, else() { return(3i32) })
}
)";
  const std::string srcPath = writeTemp("compile_exe_ir_f64_math_subset.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_ir_f64_math_subset").string();

  const std::string compileCmd = "./primec --emit=exe-ir " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("exe emitter uses ir backend for f64 comparison subset") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int>]
main() {
  if(greater_than(2.5f64, 1.0f64), then() { return(7i32) }, else() { return(3i32) })
}
)";
  const std::string srcPath = writeTemp("compile_exe_f64_cmp_ir_first.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_f64_cmp_ir_first").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("exe emitter uses ir backend for f64 arithmetic subset") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int>]
main() {
  [f64 mut] value{2.0f64}
  assign(value, plus(value, 0.5f64))
  if(greater_than(value, 2.4f64), then() { return(7i32) }, else() { return(3i32) })
}
)";
  const std::string srcPath = writeTemp("compile_exe_f64_math_ir_first.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_f64_math_ir_first").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("exe-ir emitter compiles and runs f64 conversion subset") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<i64>]
main() {
  [i32] base{7i32}
  [f64] widened{convert<f64>(base)}
  [f32] narrowed{convert<f32>(widened)}
  [f64] roundTrip{convert<f64>(narrowed)}
  return(convert<i64>(roundTrip))
}
)";
  const std::string srcPath = writeTemp("compile_exe_ir_f64_convert_subset.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_ir_f64_convert_subset").string();

  const std::string compileCmd = "./primec --emit=exe-ir " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("exe emitter uses ir backend for f64 conversion subset") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<i64>]
main() {
  [i32] base{7i32}
  [f64] widened{convert<f64>(base)}
  [f32] narrowed{convert<f32>(widened)}
  [f64] roundTrip{convert<f64>(narrowed)}
  return(convert<i64>(roundTrip))
}
)";
  const std::string srcPath = writeTemp("compile_exe_f64_convert_ir_first.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_f64_convert_ir_first").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("exe-ir emitter compiles and runs f64 to i32 conversion") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int>]
main() {
  return(convert<int>(2.5f64))
}
)";
  const std::string srcPath = writeTemp("compile_exe_ir_f64_to_i32_convert.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_ir_f64_to_i32_convert").string();

  const std::string compileCmd = "./primec --emit=exe-ir " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("exe emitter uses ir backend for f64 to i32 conversion") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int>]
main() {
  return(convert<int>(2.5f64))
}
)";
  const std::string srcPath = writeTemp("compile_exe_f64_to_i32_ir_first.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_f64_to_i32_ir_first").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("exe-ir emitter clamps f32/f64 to i64 conversion edges") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int>]
main() {
  [i64] minValue{plus(-9223372036854775807i64, -1i64)}
  if(not(equal(convert<i64>(divide(0.0f32, 0.0f32)), 0i64)), then() { return(1i32) }, else() { })
  if(not(equal(convert<i64>(divide(1.0f32, 0.0f32)), 9223372036854775807i64)), then() { return(2i32) }, else() { })
  if(not(equal(convert<i64>(divide(-1.0f32, 0.0f32)), minValue)), then() { return(3i32) }, else() { })
  if(not(equal(convert<i64>(divide(0.0f64, 0.0f64)), 0i64)), then() { return(4i32) }, else() { })
  if(not(equal(convert<i64>(divide(1.0f64, 0.0f64)), 9223372036854775807i64)), then() { return(5i32) }, else() { })
  if(not(equal(convert<i64>(divide(-1.0f64, 0.0f64)), minValue)), then() { return(6i32) }, else() { })
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_exe_ir_f64_to_i64_convert_edges.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_exe_ir_f64_to_i64_convert_edges").string();

  const std::string compileCmd = "./primec --emit=exe-ir " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("exe emitter uses ir backend for f32/f64 to i64 conversion edges") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int>]
main() {
  [i64] minValue{plus(-9223372036854775807i64, -1i64)}
  if(not(equal(convert<i64>(divide(0.0f32, 0.0f32)), 0i64)), then() { return(1i32) }, else() { })
  if(not(equal(convert<i64>(divide(1.0f32, 0.0f32)), 9223372036854775807i64)), then() { return(2i32) }, else() { })
  if(not(equal(convert<i64>(divide(-1.0f32, 0.0f32)), minValue)), then() { return(3i32) }, else() { })
  if(not(equal(convert<i64>(divide(0.0f64, 0.0f64)), 0i64)), then() { return(4i32) }, else() { })
  if(not(equal(convert<i64>(divide(1.0f64, 0.0f64)), 9223372036854775807i64)), then() { return(5i32) }, else() { })
  if(not(equal(convert<i64>(divide(-1.0f64, 0.0f64)), minValue)), then() { return(6i32) }, else() { })
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_exe_f64_to_i64_ir_first_edges.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_exe_f64_to_i64_ir_first_edges").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("exe-ir emitter truncates in-range f32/f64 to i64") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int>]
main() {
  if(not(equal(convert<i64>(2.9f32), 2i64)), then() { return(1i32) }, else() { })
  if(not(equal(convert<i64>(-2.9f32), -2i64)), then() { return(2i32) }, else() { })
  if(not(equal(convert<i64>(2.9f64), 2i64)), then() { return(3i32) }, else() { })
  if(not(equal(convert<i64>(-2.9f64), -2i64)), then() { return(4i32) }, else() { })
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_exe_ir_f64_to_i64_convert_truncation.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_exe_ir_f64_to_i64_convert_truncation").string();

  const std::string compileCmd = "./primec --emit=exe-ir " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("exe emitter uses ir backend for in-range f32/f64 to i64 truncation") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int>]
main() {
  if(not(equal(convert<i64>(2.9f32), 2i64)), then() { return(1i32) }, else() { })
  if(not(equal(convert<i64>(-2.9f32), -2i64)), then() { return(2i32) }, else() { })
  if(not(equal(convert<i64>(2.9f64), 2i64)), then() { return(3i32) }, else() { })
  if(not(equal(convert<i64>(-2.9f64), -2i64)), then() { return(4i32) }, else() { })
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_exe_f64_to_i64_ir_first_truncation.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_exe_f64_to_i64_ir_first_truncation").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("exe-ir emitter truncates in-range f32/f64 to u64") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int>]
main() {
  if(not(equal(convert<u64>(2.9f32), 2u64)), then() { return(1i32) }, else() { })
  if(not(equal(convert<u64>(42.9f32), 42u64)), then() { return(2i32) }, else() { })
  if(not(equal(convert<u64>(2.9f64), 2u64)), then() { return(3i32) }, else() { })
  if(not(equal(convert<u64>(42.9f64), 42u64)), then() { return(4i32) }, else() { })
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_exe_ir_f64_to_u64_convert_truncation.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_exe_ir_f64_to_u64_convert_truncation").string();

  const std::string compileCmd = "./primec --emit=exe-ir " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("exe emitter uses ir backend for in-range f32/f64 to u64 truncation") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int>]
main() {
  if(not(equal(convert<u64>(2.9f32), 2u64)), then() { return(1i32) }, else() { })
  if(not(equal(convert<u64>(42.9f32), 42u64)), then() { return(2i32) }, else() { })
  if(not(equal(convert<u64>(2.9f64), 2u64)), then() { return(3i32) }, else() { })
  if(not(equal(convert<u64>(42.9f64), 42u64)), then() { return(4i32) }, else() { })
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_exe_f64_to_u64_ir_first_truncation.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_exe_f64_to_u64_ir_first_truncation").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}


TEST_CASE("cpp and exe emitters match cpp-ir and exe-ir on shared corpus") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  struct DifferentialCase {
    const char *name;
    const char *source;
    const char *runtimeArgs;
    int expectedExitCode;
  };

  const std::vector<DifferentialCase> cases = {
      {
          "i32_arithmetic",
          R"(
[return<int>]
main() {
  [i32 mut] counter{1i32}
  assign(counter, plus(counter, 2i32))
  return(counter)
}
)",
          "",
          3,
      },
      {
          "argv_and_io",
          R"(
[return<int> effects(io_out, io_err)]
main([array<string>] args) {
  print_line(args[1i32])
  print_error("!"utf8)
  return(args.count())
}
)",
          " alpha beta",
          3,
      },
      {
          "dynamic_string",
          R"(
[return<int> effects(io_out)]
main() {
  [string mut] msg{"left"utf8}
  assign(msg, "right"utf8)
  print_line(msg)
  return(0i32)
}
)",
          "",
          0,
      },
  };

  for (const auto &testCase : cases) {
    CAPTURE(testCase.name);
    const std::string srcPath = writeTemp(std::string("compile_cpp_ir_differential_") + testCase.name + ".prime",
                                          testCase.source);
    const std::string astCppPath =
        (testScratchPath("") / (std::string("primec_cpp_differential_") + testCase.name + ".cpp"))
            .string();
    const std::string irCppPath =
        (testScratchPath("") / (std::string("primec_cpp_ir_differential_") + testCase.name + ".cpp"))
            .string();
    const std::string astExePath =
        (testScratchPath("") / (std::string("primec_exe_differential_") + testCase.name)).string();
    const std::string irExePath =
        (testScratchPath("") / (std::string("primec_exe_ir_differential_") + testCase.name)).string();

    const std::string compileAstCppCmd = "./primec --emit=cpp " + quoteShellArg(srcPath) + " -o " +
                                         quoteShellArg(astCppPath) + " --entry /main";
    const std::string compileIrCppCmd = "./primec --emit=cpp-ir " + quoteShellArg(srcPath) + " -o " +
                                        quoteShellArg(irCppPath) + " --entry /main";
    CHECK(runCommand(compileAstCppCmd) == 0);
    CHECK(runCommand(compileIrCppCmd) == 0);
    const std::string astCppSource = readFile(astCppPath);
    const std::string irCppSource = readFile(irCppPath);
    CHECK(!astCppSource.empty());
    CHECK(!irCppSource.empty());
    CHECK(astCppSource == irCppSource);

    const std::string compileAstExeCmd = "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " +
                                         quoteShellArg(astExePath) + " --entry /main";
    const std::string compileIrExeCmd = "./primec --emit=exe-ir " + quoteShellArg(srcPath) + " -o " +
                                        quoteShellArg(irExePath) + " --entry /main";
    CHECK(runCommand(compileAstExeCmd) == 0);
    CHECK(runCommand(compileIrExeCmd) == 0);

    const std::string astOutPath = (testScratchPath("") /
                                    (std::string("primec_exe_differential_") + testCase.name + ".out"))
                                       .string();
    const std::string astErrPath = (testScratchPath("") /
                                    (std::string("primec_exe_differential_") + testCase.name + ".err"))
                                       .string();
    const std::string irOutPath = (testScratchPath("") /
                                   (std::string("primec_exe_ir_differential_") + testCase.name + ".out"))
                                      .string();
    const std::string irErrPath = (testScratchPath("") /
                                   (std::string("primec_exe_ir_differential_") + testCase.name + ".err"))
                                      .string();

    const std::string runAstCmd = quoteShellArg(astExePath) + testCase.runtimeArgs + " > " + quoteShellArg(astOutPath) +
                                  " 2> " + quoteShellArg(astErrPath);
    const std::string runIrCmd = quoteShellArg(irExePath) + testCase.runtimeArgs + " > " + quoteShellArg(irOutPath) +
                                 " 2> " + quoteShellArg(irErrPath);
    CHECK(runCommand(runAstCmd) == testCase.expectedExitCode);
    CHECK(runCommand(runIrCmd) == testCase.expectedExitCode);
    CHECK(readFile(astOutPath) == readFile(irOutPath));
    CHECK(readFile(astErrPath) == readFile(irErrPath));
  }
}

TEST_CASE("cpp and exe diagnostics match cpp-ir and exe-ir (text and json)") {
  struct DiagnosticCase {
    const char *name;
    const char *source;
  };

  const std::vector<DiagnosticCase> cases = {
      {
          "semantic_argument_mismatch",
          R"(
/consume([i32] value) {
  value
}
[return<int>]
main() {
  consume(true)
  return(0i32)
}
)",
      },
      {
          "lowering_unsupported_lambda",
          R"(
[return<int>]
main() {
  holder{[]([i32] x) { return(x) }}
  return(0i32)
}
)",
      },
      {
          "lowering_software_numeric",
          R"(
[return<decimal>]
main() {
  [decimal] value{convert<decimal>(1.5f32)}
  return(value)
}
)",
      },
      {
          "lowering_non_empty_soa_vector_literal",
          R"(
Particle() {
  [i32] x{1i32}
}
[return<int> effects(heap_alloc)]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>(Particle(1i32))}
  return(0i32)
}
)",
      },
  };

  struct EmitPair {
    const char *left;
    const char *right;
  };

  const std::vector<EmitPair> emitPairs = {
      {"cpp", "cpp-ir"},
      {"exe", "exe-ir"},
  };

  for (const auto &testCase : cases) {
    CAPTURE(testCase.name);
    const std::string srcPath = writeTemp(std::string("compile_diagnostics_parity_") + testCase.name + ".prime",
                                          testCase.source);

    for (const auto &pair : emitPairs) {
      CAPTURE(pair.left);
      CAPTURE(pair.right);

      const std::filesystem::path tempDir = testScratchPath("");
      const std::string leftErrPath =
          (tempDir / (std::string("primec_diag_") + pair.left + "_" + testCase.name + ".txt")).string();
      const std::string rightErrPath =
          (tempDir / (std::string("primec_diag_") + pair.right + "_" + testCase.name + ".txt")).string();
      const std::string leftJsonErrPath =
          (tempDir / (std::string("primec_diag_json_") + pair.left + "_" + testCase.name + ".txt")).string();
      const std::string rightJsonErrPath =
          (tempDir / (std::string("primec_diag_json_") + pair.right + "_" + testCase.name + ".txt")).string();

      const std::string leftTextCmd = "./primec --emit=" + std::string(pair.left) + " " + quoteShellArg(srcPath) +
                                      " -o /dev/null --entry /main 2> " + quoteShellArg(leftErrPath);
      const std::string rightTextCmd = "./primec --emit=" + std::string(pair.right) + " " + quoteShellArg(srcPath) +
                                       " -o /dev/null --entry /main 2> " + quoteShellArg(rightErrPath);
      CHECK(runCommand(leftTextCmd) == 2);
      CHECK(runCommand(rightTextCmd) == 2);
      CHECK(readFile(leftErrPath) == readFile(rightErrPath));

      const std::string leftJsonCmd = "./primec --emit=" + std::string(pair.left) + " " + quoteShellArg(srcPath) +
                                      " -o /dev/null --entry /main --emit-diagnostics 2> " +
                                      quoteShellArg(leftJsonErrPath);
      const std::string rightJsonCmd = "./primec --emit=" + std::string(pair.right) + " " + quoteShellArg(srcPath) +
                                       " -o /dev/null --entry /main --emit-diagnostics 2> " +
                                       quoteShellArg(rightJsonErrPath);
      CHECK(runCommand(leftJsonCmd) == 2);
      CHECK(runCommand(rightJsonCmd) == 2);
      CHECK(readFile(leftJsonErrPath) == readFile(rightJsonErrPath));
    }
  }
}

TEST_CASE("compiles and runs explicit void return") {
  const std::string source = R"(
[return<void>]
main() {
  return()
}
)";
  const std::string srcPath = writeTemp("compile_void_return.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_void_return_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("exe emitter uses ir backend for file read subset") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<Result<FileError>> effects(file_write) on_error<FileError, /log_file_error>]
main() {
  [File<Read>] file{File<Read>("/dev/null"utf8)?}
  file.close()?
  return(Result.ok())
}
[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error("file error"utf8)
}
)";
  const std::string srcPath = writeTemp("compile_exe_file_read_ir_first.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_file_read_ir_first").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("compiles and runs implicit void main") {
  const std::string source = R"(
main() {
  [i32] value{1i32}
  value
}
)";
  const std::string srcPath = writeTemp("compile_void_implicit.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_void_implicit_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("compiles and runs argv count") {
  const std::string source = R"(
[return<int>]
main([array<string>] args) {
  return(args.count())
}
)";
  const std::string srcPath = writeTemp("compile_args.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_args_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
  CHECK(runCommand(exePath + " alpha beta") == 3);
}

TEST_CASE("compiles and runs argv count helper") {
  const std::string source = R"(
[return<int>]
main([array<string>] args) {
  return(count(args))
}
)";
  const std::string srcPath = writeTemp("compile_args_count_helper.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_args_count_helper_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
  CHECK(runCommand(exePath + " alpha beta") == 3);
}

TEST_CASE("compiles and runs argv error output in C++ emitter") {
  const std::string source = R"(
[return<int> effects(io_err)]
main([array<string>] args) {
  print_line_error(args[1i32])
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_args_error.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_args_error_exe").string();
  const std::string errPath = (testScratchPath("") / "primec_args_error_err.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " alpha beta 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath) == "alpha\n");
}

TEST_CASE("compiles and runs argv error output without newline in C++ emitter") {
  const std::string source = R"(
[return<int> effects(io_err)]
main([array<string>] args) {
  print_error(args[1i32])
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_args_error_no_newline.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_args_error_no_newline_exe").string();
  const std::string errPath =
      (testScratchPath("") / "primec_args_error_no_newline_err.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " alpha beta 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath) == "alpha");
}

TEST_CASE("compiles and runs argv error output u64 index in C++ emitter") {
  const std::string source = R"(
[return<int> effects(io_err)]
main([array<string>] args) {
  if(greater_than(args.count(), 1i32)) {
    print_error(args[1u64])
  } else {
  }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_args_error_u64.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_args_error_u64_exe").string();
  const std::string errPath =
      (testScratchPath("") / "primec_args_error_u64_err.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " alpha 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath) == "alpha");
}

TEST_CASE("compiles and runs argv unsafe error output in C++ emitter") {
  const std::string source = R"(
[return<int> effects(io_err)]
main([array<string>] args) {
  if(greater_than(args.count(), 1i32)) {
    print_error(at_unsafe(args, 1i32))
  } else {
  }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_args_error_unsafe.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_args_error_unsafe_exe").string();
  const std::string errPath =
      (testScratchPath("") / "primec_args_error_unsafe_err.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " alpha 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath) == "alpha");
}

TEST_CASE("compiles and runs argv unsafe line error output in C++ emitter") {
  const std::string source = R"(
[return<int> effects(io_err)]
main([array<string>] args) {
  if(greater_than(args.count(), 1i32)) {
    print_line_error(at_unsafe(args, 1i32))
  } else {
  }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_args_line_error_unsafe.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_args_line_error_unsafe_exe").string();
  const std::string errPath =
      (testScratchPath("") / "primec_args_line_error_unsafe_err.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " alpha 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath) == "alpha\n");
}

TEST_CASE("compiles and runs argv print in C++ emitter") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  if(greater_than(args.count(), 2i32)) {
    print_line(args[1i32])
    print_line(args[2i32])
  } else {
  }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_args_print.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_args_print_exe").string();
  const std::string outPath = (testScratchPath("") / "primec_args_print_out.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " alpha beta > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "alpha\nbeta\n");
}

TEST_CASE("compiles and runs argv print without newline in C++ emitter") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  if(greater_than(args.count(), 1i32)) {
    print(args[1i32])
  } else {
  }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_args_print_no_newline.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_args_print_no_newline_exe").string();
  const std::string outPath =
      (testScratchPath("") / "primec_args_print_no_newline_out.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " alpha > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "alpha");
}

TEST_CASE("compiles and runs argv print with u64 index in C++ emitter") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  if(greater_than(args.count(), 1i32)) {
    print_line(args[1u64])
  } else {
  }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_args_print_u64.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_args_print_u64_exe").string();
  const std::string outPath = (testScratchPath("") / "primec_args_print_u64_out.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " alpha > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "alpha\n");
}

TEST_CASE("compiles and runs argv unsafe access in C++ emitter") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  if(greater_than(args.count(), 2i32)) {
    print_line(at_unsafe(args, 1i32))
    print_line(at_unsafe(args, 2i32))
  } else {
  }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_args_unsafe.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_args_unsafe_exe").string();
  const std::string outPath = (testScratchPath("") / "primec_args_unsafe_out.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " alpha beta > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "alpha\nbeta\n");
}

TEST_CASE("compiles and runs argv unsafe access with u64 index in C++ emitter") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  if(greater_than(args.count(), 1i32)) {
    print_line(at_unsafe(args, 1u64))
  } else {
  }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_args_unsafe_u64.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_args_unsafe_u64_exe").string();
  const std::string outPath = (testScratchPath("") / "primec_args_unsafe_u64_out.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " alpha > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "alpha\n");
}

TEST_CASE("compiles and runs three-element array literal") {
  const std::string source = R"(
[return<int>]
main() {
  return(at_unsafe(array<i32>{1i32, 2i32, 3i32}, 2i32))
}
)";
  const std::string srcPath = writeTemp("compile_array_literal.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_array_literal_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("compiles and runs array literal count method") {
  const std::string source = R"(
[return<int>]
main() {
  return(array<i32>(1i32, 2i32, 3i32).count())
}
)";
  const std::string srcPath = writeTemp("compile_array_literal_count.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_array_literal_count_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("compiles and runs array literal unsafe access") {
  const std::string source = R"(
[return<int>]
main() {
  return(at_unsafe(array<i32>(4i32, 7i32, 9i32), 1i32))
}
)";
  const std::string srcPath = writeTemp("compile_array_literal_unsafe.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_array_literal_unsafe_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs array count helper") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32, 3i32)}
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("compile_array_count_helper.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_array_count_helper_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("compiles and runs array literal count helper") {
  const std::string source = R"(
[return<int>]
main() {
  return(count(array<i32>(1i32, 2i32, 3i32)))
}
)";
  const std::string srcPath = writeTemp("compile_array_literal_count_helper.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_array_literal_count_helper_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("compiles and runs literal method call in C++ emitter") {
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
  const std::string srcPath = writeTemp("compile_method_literal.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_method_literal_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}


TEST_SUITE_END();
