#include "test_compile_run_helpers.h"

#include "test_compile_run_native_backend_core_helpers.h"

#if PRIMESTRUCT_NATIVE_CORE_ENABLED
TEST_SUITE_BEGIN("primestruct.compile.run.native_backend.core");

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


TEST_SUITE_END();
#endif
