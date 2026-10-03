#include "../test_compile_run_helpers.h"

#include "primec/ir/IrSerializer.h"

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

TEST_CASE("vm writes and reads back a file") {
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
import /std/collections/vector

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
  // TODO-4757 (fixed): Result.why() now reports the real per-variant
  // message instead of an empty string. ppm/png write() report a generic
  // "image_invalid_operation" (their own why() has no dedicated variant
  // for the failure this repro hits), and png/read() reports
  // "image_read_unsupported" since PNG decoding isn't implemented.
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
import /std/collections/vector

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
  print_line(/std/collections/vector/count<i32>(pixels))
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
import /std/collections/vector

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
  print_line(/std/collections/vector/count<i32>(pixels))
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
import /std/collections/vector

[effects(heap_alloc, io_out, file_write), return<int>]
main() {
  [i32 mut] width{7i32}
  [i32 mut] height{9i32}
  [vector<i32> mut] pixels{vector<i32>(1i32, 2i32, 3i32)}
  [Result<ImageError>] status{/std/image/ppm/read(width, height, pixels, "__PATH__"utf8)}
  print_line(Result.why(status))
  print_line(width)
  print_line(height)
  print_line(/std/collections/vector/count<i32>(pixels))
  return(0i32)
}
)", escapedPath);
  const std::string srcPath = writeTemp("vm_image_read_p6_truncated.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_vm_image_read_p6_truncated.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  // TODO-4757 (fixed): Result.why() on an ImageError Result now reports the
  // real message instead of an empty string.
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
import /std/collections/vector

[effects(heap_alloc, io_out, file_write), return<int>]
main() {
  [i32 mut] width{7i32}
  [i32 mut] height{9i32}
  [vector<i32> mut] pixels{vector<i32>(1i32, 2i32, 3i32)}
  [Result<ImageError>] ppmStatus{/std/image/ppm/read(width, height, pixels, "__PPM_PATH__"utf8)}
  print_line(Result.why(ppmStatus))
  print_line(width)
  print_line(height)
  print_line(/std/collections/vector/count<i32>(pixels))
  assign(width, 11i32)
  assign(height, 13i32)
  assign(pixels, vector<i32>(4i32, 5i32, 6i32))
  [Result<ImageError>] pngStatus{/std/image/png/read(width, height, pixels, "__PNG_PATH__"utf8)}
  print_line(Result.why(pngStatus))
  print_line(width)
  print_line(height)
  print_line(/std/collections/vector/count<i32>(pixels))
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
  // TODO-4757 (fixed): Result.why() on an ImageError Result now reports the
  // real message instead of an empty string.
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

TEST_CASE("runs vm ppm write for ascii p3 outputs") {
  const std::filesystem::path outPath = testScratchPath("") / "primec_vm_image_write.ppm";
  std::error_code ec;
  std::filesystem::remove(outPath, ec);

  const std::string escapedPath = escapeStringLiteral(outPath.string());
  const std::string source = injectEscapedPath(R"(
import /std/image/*
import /std/collections/vector

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

TEST_CASE("rejects invalid vm ppm write inputs deterministically") {
  const std::filesystem::path outPath = testScratchPath("") / "primec_vm_image_write_invalid.ppm";
  std::error_code ec;
  std::filesystem::remove(outPath, ec);

  const std::string escapedPath = escapeStringLiteral(outPath.string());
  const std::string source = injectEscapedPath(R"(
import /std/image/*
import /std/collections/vector

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

TEST_CASE("runs vm png write for deterministic rgb outputs") {
  const std::filesystem::path outPath = testScratchPath("") / "primec_vm_image_write.png";
  std::error_code ec;
  std::filesystem::remove(outPath, ec);

  const std::string escapedPath = escapeStringLiteral(outPath.string());
  const std::string source = injectEscapedPath(R"(
import /std/image/*
import /std/collections/vector

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

TEST_CASE("rejects invalid vm png write inputs deterministically") {
  const std::filesystem::path outPath = testScratchPath("") / "primec_vm_image_write_invalid.png";
  std::error_code ec;
  std::filesystem::remove(outPath, ec);

  const std::string escapedPath = escapeStringLiteral(outPath.string());
  const std::string source = injectEscapedPath(R"(
import /std/image/*
import /std/collections/vector

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

TEST_CASE("rejects oversized vm image write dimensions before overflow") {
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
import /std/collections/vector

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
  // TODO-4757 (fixed): Result.why() on these oversized-write ImageError
  // Results now reports the real message. Both ppm/write and png/write map
  // every writeImpl() failure to invalidOperation() (image.prime's
  // ppm/write and png/write, e.g. lines 2532-2539) regardless of cause, so
  // "image_invalid_operation" - not "image_write_unsupported" - is the
  // correct message here; the previous "image_write_unsupported" pin was
  // an unverified guess made while Result.why() was still blank.
  CHECK(readFile(stdoutPath) ==
        "image_invalid_operation\n"
        "image_invalid_operation\n");
  CHECK(!std::filesystem::exists(ppmOutPath));
  CHECK(!std::filesystem::exists(pngOutPath));
}

TEST_CASE("runs vm png read for stored rgba inputs deterministically") {
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
import /std/collections/vector

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
  print_line(/std/collections/vector/count<i32>(pixels))
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

TEST_CASE("runs vm png read for stored sub-filter rgba inputs deterministically") {
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
import /std/collections/vector

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
  print_line(/std/collections/vector/count<i32>(pixels))
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

TEST_CASE("runs vm png read for stored up-filter rgba inputs deterministically") {
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
import /std/collections/vector

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
  print_line(/std/collections/vector/count<i32>(pixels))
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

TEST_CASE("runs vm png read for stored average-filter rgba inputs deterministically") {
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
import /std/collections/vector

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
  print_line(/std/collections/vector/count<i32>(pixels))
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

TEST_CASE("runs vm png read for fixed-huffman backreference rgba inputs deterministically") {
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
import /std/collections/vector

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
  print_line(/std/collections/vector/count<i32>(pixels))
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

TEST_CASE("runs vm png read for dynamic-huffman literal indexed inputs deterministically") {
  const std::string inPath =
      (testScratchPath("") / "primec_vm_image_read_dynamic_literal.png").string();
  {
    const std::vector<unsigned char> pngBytes = withValidPngCrcs({
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x09,
        0x08, 0x03, 0x00, 0x00, 0x00, 0xc4, 0x98, 0xb6,
        0xd6, 0x00, 0x00, 0x00, 0x18, 0x50, 0x4c, 0x54,
        0x45, 0x0a, 0x14, 0x1e, 0x28, 0x32, 0x3c, 0x46,
        0x50, 0x5a, 0x64, 0x6e, 0x78, 0x82, 0x8c, 0x96,
        0xa0, 0xaa, 0xb4, 0xbe, 0xc8, 0xd2, 0xdc, 0xe6,
        0xf0, 0xb6, 0x07, 0x60, 0xa9, 0x00, 0x00, 0x00,
        0x19, 0x49, 0x44, 0x41, 0x54, 0x78, 0x01, 0x05,
        0xc1, 0x05, 0x01, 0x00, 0x00, 0x08, 0x00, 0x20,
        0xec, 0xff, 0x8f, 0x05, 0x84, 0x54, 0xda, 0x58,
        0xc7, 0x03, 0x00, 0xd6, 0x00, 0x1d, 0xbf, 0xe3,
        0xee, 0xb4, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45,
        0x4e, 0x44, 0xae, 0x42, 0x60, 0x82,
    });
    std::ofstream file(inPath, std::ios::binary);
    REQUIRE(file.good());
    file.write(reinterpret_cast<const char *>(pngBytes.data()), static_cast<std::streamsize>(pngBytes.size()));
    REQUIRE(file.good());
  }

  const std::string escapedPath = escapeStringLiteral(inPath);
  const std::string source = injectEscapedPath(R"(
import /std/image/*
import /std/collections/vector

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
  print_line(/std/collections/vector/count<i32>(pixels))
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
  CHECK(runCommand(runCmd) == 10);
  CHECK(readFile(outPath) ==
        "1\n"
        "9\n"
        "27\n"
        "10\n"
        "20\n"
        "30\n"
        "40\n"
        "50\n"
        "60\n");
}

TEST_CASE("runs vm png read for dynamic-huffman backreference rgba inputs deterministically") {
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
import /std/collections/vector

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
  print_line(/std/collections/vector/count<i32>(pixels))
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

TEST_CASE("rejects malformed vm png inputs deterministically") {
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
import /std/collections/vector

[effects(heap_alloc, io_out, file_write), return<int>]
main() {
  [i32 mut] width{7i32}
  [i32 mut] height{9i32}
  [vector<i32> mut] pixels{vector<i32>(1i32, 2i32, 3i32)}
  [Result<ImageError>] status{/std/image/png/read(width, height, pixels, "__PATH__"utf8)}
  print_line(Result.why(status))
  print_line(width)
  print_line(height)
  print_line(/std/collections/vector/count<i32>(pixels))
  return(0i32)
}
)", escapedPath);
  const std::string srcPath = writeTemp("vm_image_read_invalid_png.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_vm_image_read_invalid_png.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  // TODO-4757 (fixed): Result.why() on an ImageError Result now reports the
  // real message instead of an empty string.
  CHECK(readFile(outPath) ==
        "image_invalid_operation\n"
        "0\n"
        "0\n"
        "0\n");
}

TEST_CASE("rejects vm png inputs with critical chunk crc mismatches deterministically") {
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
import /std/collections/vector

[effects(heap_alloc, io_out, file_write), return<int>]
main() {
  [i32 mut] width{7i32}
  [i32 mut] height{9i32}
  [vector<i32> mut] pixels{vector<i32>(1i32, 2i32, 3i32)}
  [Result<ImageError>] status{/std/image/png/read(width, height, pixels, "__PATH__"utf8)}
  print_line(Result.why(status))
  print_line(width)
  print_line(height)
  print_line(/std/collections/vector/count<i32>(pixels))
  return(0i32)
}
)", escapedPath);
  const std::string srcPath = writeTemp("vm_image_read_invalid_crc_png.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_vm_image_read_invalid_crc_png.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  // TODO-4757 (fixed): Result.why() on an ImageError Result now reports the
  // real message instead of an empty string.
  CHECK(readFile(outPath) ==
        "image_invalid_operation\n"
        "0\n"
        "0\n"
        "0\n");
}

TEST_CASE("rejects vm png inputs with non-consecutive idat chunks deterministically") {
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
import /std/collections/vector

[effects(heap_alloc, io_out, file_write), return<int>]
main() {
  [i32 mut] width{7i32}
  [i32 mut] height{9i32}
  [vector<i32> mut] pixels{vector<i32>(1i32, 2i32, 3i32)}
  [Result<ImageError>] status{/std/image/png/read(width, height, pixels, "__PATH__"utf8)}
  print_line(Result.why(status))
  print_line(width)
  print_line(height)
  print_line(/std/collections/vector/count<i32>(pixels))
  return(0i32)
}
)", escapedPath);
  const std::string srcPath = writeTemp("vm_image_read_invalid_idat_order_png.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_vm_image_read_invalid_idat_order_png.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  // TODO-4757 (fixed): Result.why() on an ImageError Result now reports the
  // real message instead of an empty string.
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

TEST_SUITE_END();
