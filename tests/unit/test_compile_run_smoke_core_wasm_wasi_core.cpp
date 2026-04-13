#include "test_compile_run_helpers.h"

#include "test_compile_run_smoke_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.smoke");

TEST_CASE("primec wasm wasi stdout and stderr match vm output") {
  const std::string source = R"(
[return<int> effects(io_out, io_err)]
main() {
  print_line("hello"utf8)
  print_error("warn"utf8)
  print_line_error("err"utf8)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_emit_wasm_wasi_output_parity.prime", source);
  const std::string wasmPath =
      (testScratchPath("") / "primec_emit_wasm_wasi_output_parity.wasm").string();
  const std::string compileErrPath =
      (testScratchPath("") / "primec_emit_wasm_wasi_output_parity_compile_err.txt").string();
  const std::string vmOutPath = (testScratchPath("") / "primec_vm_wasi_output_parity_out.txt").string();
  const std::string vmErrPath = (testScratchPath("") / "primec_vm_wasi_output_parity_err.txt").string();
  const std::string wasmOutPath =
      (testScratchPath("") / "primec_wasm_wasi_output_parity_out.txt").string();
  const std::string wasmErrPath =
      (testScratchPath("") / "primec_wasm_wasi_output_parity_err.txt").string();

  const std::string wasmCmd = "./primec --emit=wasm " + quoteShellArg(srcPath) + " -o " + quoteShellArg(wasmPath) +
                              " --entry /main 2> " + quoteShellArg(compileErrPath);
  CHECK(runCommand(wasmCmd) == 0);
  CHECK(std::filesystem::exists(wasmPath));

  const std::string vmCmd = "./primevm " + quoteShellArg(srcPath) + " --entry /main > " + quoteShellArg(vmOutPath) +
                            " 2> " + quoteShellArg(vmErrPath);
  CHECK(runCommand(vmCmd) == 0);

  if (hasWasmtime()) {
    const std::string wasmRunCmd = "wasmtime --invoke main " + quoteShellArg(wasmPath) + " > " +
                                   quoteShellArg(wasmOutPath) + " 2> " + quoteShellArg(wasmErrPath);
    CHECK(runCommand(wasmRunCmd) == 0);
    CHECK(readFile(wasmOutPath) == readFile(vmOutPath));
    CHECK(readFile(wasmErrPath) == readFile(vmErrPath));
  }
}

TEST_CASE("primec wasm wasi argc path matches vm exit code") {
  const std::string source = R"(
[return<int>]
main([array<string>] args) {
  return(args.count())
}
)";
  const std::string srcPath = writeTemp("compile_emit_wasm_wasi_argc_parity.prime", source);
  const std::string wasmPath =
      (testScratchPath("") / "primec_emit_wasm_wasi_argc_parity.wasm").string();
  const std::string compileErrPath =
      (testScratchPath("") / "primec_emit_wasm_wasi_argc_parity_compile_err.txt").string();

  const std::string wasmCmd = "./primec --emit=wasm " + quoteShellArg(srcPath) + " -o " + quoteShellArg(wasmPath) +
                              " --entry /main 2> " + quoteShellArg(compileErrPath);
  CHECK(runCommand(wasmCmd) == 0);
  CHECK(std::filesystem::exists(wasmPath));

  const std::string vmCmd = "./primevm " + quoteShellArg(srcPath) + " --entry /main";
  const int vmExitCode = runCommand(vmCmd);

  if (hasWasmtime()) {
    const std::string wasmRunCmd = "wasmtime --invoke main " + quoteShellArg(wasmPath);
    const int wasmExitCode = runCommand(wasmRunCmd);
    CHECK(wasmExitCode == vmExitCode);
  }
}

TEST_CASE("primec wasm wasi supports File<Read>.read_byte with deterministic eof") {
  const std::filesystem::path tempRoot = testScratchPath("") / "primec_wasm_file_read_byte_runtime";
  std::error_code ec;
  std::filesystem::remove_all(tempRoot, ec);
  std::filesystem::create_directories(tempRoot, ec);
  REQUIRE(!ec);

  {
    std::ofstream input(tempRoot / "input.bin", std::ios::binary);
    REQUIRE(input.good());
    input.write("AB", 2);
    REQUIRE(input.good());
  }

  const std::string source = R"(
[return<Result<FileError>> effects(file_read, io_out) on_error<FileError, /log_file_error>]
main() {
  [File<Read>] file{ File<Read>("input.bin"utf8)? }
  [i32 mut] first{0i32}
  [i32 mut] second{0i32}
  [i32 mut] third{99i32}
  file.read_byte(first)?
  file.read_byte(second)?
  print_line(first)
  print_line(second)
  print_line(Result.why(file.read_byte(third)))
  print_line(third)
  file.close()?
  return(Result.ok())
}

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error("file error"utf8)
}
)";
  const std::string srcPath = writeTemp("compile_emit_wasm_file_read_byte.prime", source);
  const std::string wasmPath = (tempRoot / "file_read_byte.wasm").string();
  const std::string compileErrPath = (tempRoot / "file_read_byte_compile_err.txt").string();
  const std::string wasmCmd = "./primec --emit=wasm --wasm-profile wasi " + quoteShellArg(srcPath) + " -o " +
                              quoteShellArg(wasmPath) + " --entry /main 2> " + quoteShellArg(compileErrPath);
  CHECK(runCommand(wasmCmd) == 0);
  CHECK(std::filesystem::exists(wasmPath));

  if (hasWasmtime()) {
    const std::string outPath = (tempRoot / "file_read_byte_stdout.txt").string();
    const std::string wasmRunCmd = "wasmtime --invoke main --dir=" + quoteShellArg(tempRoot.string()) + " " +
                                   quoteShellArg(wasmPath) + " > " + quoteShellArg(outPath);
    CHECK(runCommand(wasmRunCmd) == 0);
    CHECK(readFile(outPath) == "65\n66\nEOF\n99\n");
  }
}

TEST_CASE("primec wasm wasi runs ppm read for ascii p3 inputs") {
  const std::filesystem::path tempRoot = testScratchPath("") / "primec_wasm_ppm_read_runtime";
  std::error_code ec;
  std::filesystem::remove_all(tempRoot, ec);
  std::filesystem::create_directories(tempRoot, ec);
  REQUIRE(!ec);

  {
    std::ofstream input(tempRoot / "input.ppm", std::ios::binary);
    REQUIRE(input.good());
    input << "P3\n2 1\n255\n255 0 0 0 255 128\n";
    REQUIRE(input.good());
  }

  const std::string source = R"(
import /std/image/*
import /std/collections/*

[effects(heap_alloc, io_out, file_write), return<int>]
main() {
  [i32 mut] width{0i32}
  [i32 mut] height{0i32}
  [vector<i32> mut] pixels{vector<i32>()}
  [Result<ImageError>] status{/std/image/ppm/read(width, height, pixels, "input.ppm"utf8)}
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
)";
  const std::string srcPath = writeTemp("compile_emit_wasm_ppm_read.prime", source);
  const std::string wasmPath = (tempRoot / "ppm_read.wasm").string();
  const std::string compileErrPath = (tempRoot / "ppm_read_compile_err.txt").string();
  if (!compileWasmWasiOrExpectUnsupported(srcPath, wasmPath, compileErrPath)) {
    return;
  }
  CHECK(std::filesystem::exists(wasmPath));

  const std::string outPath = (tempRoot / "ppm_read_stdout.txt").string();
  checkWasmWasiRuntimeInDir(tempRoot,
                            wasmPath,
                            outPath,
                            3,
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

TEST_CASE("primec wasm wasi runs binary p6 ppm inputs") {
  const std::filesystem::path tempRoot = testScratchPath("") / "primec_wasm_ppm_p6_runtime";
  std::error_code ec;
  std::filesystem::remove_all(tempRoot, ec);
  std::filesystem::create_directories(tempRoot, ec);
  REQUIRE(!ec);

  {
    std::ofstream input(tempRoot / "input.ppm", std::ios::binary);
    REQUIRE(input.good());
    input << "P6\n2 1\n255\n";
    input.put(static_cast<char>(255));
    input.put(static_cast<char>(0));
    input.put(static_cast<char>(0));
    input.put(static_cast<char>(0));
    input.put(static_cast<char>(255));
    input.put(static_cast<char>(128));
    REQUIRE(input.good());
  }

  const std::string source = R"(
import /std/image/*
import /std/collections/*

[effects(heap_alloc, io_out, file_read), return<int>]
main() {
  [i32 mut] width{0i32}
  [i32 mut] height{0i32}
  [vector<i32> mut] pixels{vector<i32>()}
  [Result<ImageError>] status{/std/image/ppm/read(width, height, pixels, "input.ppm"utf8)}
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
)";
  const std::string srcPath = writeTemp("compile_emit_wasm_ppm_p6.prime", source);
  const std::string wasmPath = (tempRoot / "ppm_p6.wasm").string();
  const std::string compileErrPath = (tempRoot / "ppm_p6_compile_err.txt").string();
  if (!compileWasmWasiOrExpectUnsupported(srcPath, wasmPath, compileErrPath)) {
    return;
  }
  CHECK(std::filesystem::exists(wasmPath));

  const std::string outPath = (tempRoot / "ppm_p6_stdout.txt").string();
  checkWasmWasiRuntimeInDir(tempRoot,
                            wasmPath,
                            outPath,
                            3,
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

TEST_CASE("primec wasm wasi rejects truncated binary ppm reads deterministically") {
  const std::filesystem::path tempRoot =
      testScratchPath("") / "primec_wasm_ppm_p6_truncated_runtime";
  std::error_code ec;
  std::filesystem::remove_all(tempRoot, ec);
  std::filesystem::create_directories(tempRoot, ec);
  REQUIRE(!ec);

  {
    std::ofstream input(tempRoot / "input.ppm", std::ios::binary);
    REQUIRE(input.good());
    input << "P6\n2 1\n255\n";
    input.put(static_cast<char>(255));
    input.put(static_cast<char>(0));
    input.put(static_cast<char>(0));
    input.put(static_cast<char>(0));
    input.put(static_cast<char>(255));
    REQUIRE(input.good());
  }

  const std::string source = R"(
import /std/image/*
import /std/collections/*

[effects(heap_alloc, io_out, file_write), return<int>]
main() {
  [i32 mut] width{7i32}
  [i32 mut] height{9i32}
  [vector<i32> mut] pixels{vector<i32>(1i32, 2i32, 3i32)}
  [Result<ImageError>] status{/std/image/ppm/read(width, height, pixels, "input.ppm"utf8)}
  print_line(Result.why(status))
  print_line(width)
  print_line(height)
  print_line(vectorCount<i32>(pixels))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_emit_wasm_ppm_p6_truncated.prime", source);
  const std::string wasmPath = (tempRoot / "ppm_p6_truncated.wasm").string();
  const std::string compileErrPath = (tempRoot / "ppm_p6_truncated_compile_err.txt").string();
  if (!compileWasmWasiOrExpectUnsupported(srcPath, wasmPath, compileErrPath)) {
    return;
  }
  CHECK(std::filesystem::exists(wasmPath));

  const std::string outPath = (tempRoot / "ppm_p6_truncated_stdout.txt").string();
  checkWasmWasiRuntimeInDir(tempRoot,
                            wasmPath,
                            outPath,
                            0,
                            "image_invalid_operation\n"
                            "0\n"
                            "0\n"
                            "0\n");
}

TEST_CASE("primec wasm wasi ppm write requires heap_alloc for vector literal") {
  const std::filesystem::path tempRoot = testScratchPath("") / "primec_wasm_ppm_write_runtime";
  std::error_code ec;
  std::filesystem::remove_all(tempRoot, ec);
  std::filesystem::create_directories(tempRoot, ec);
  REQUIRE(!ec);

  const std::string source = R"(
import /std/image/*
import /std/collections/*

[effects(file_write), return<int>]
main() {
  [vector<i32>] pixels{vector<i32>(255i32, 0i32, 0i32, 0i32, 255i32, 128i32)}
  [Result<ImageError>] status{/std/image/ppm/write("output.ppm"utf8, 2i32, 1i32, pixels)}
  if(Result.error(status),
     then() { return(1i32) },
     else() { })
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_emit_wasm_ppm_write.prime", source);
  const std::string wasmPath = (tempRoot / "ppm_write.wasm").string();
  const std::string compileErrPath = (tempRoot / "ppm_write_compile_err.txt").string();
  const std::string wasmCmd = "./primec --emit=wasm --wasm-profile wasi " + quoteShellArg(srcPath) + " -o " +
                              quoteShellArg(wasmPath) + " --entry /main 2> " + quoteShellArg(compileErrPath);
  CHECK(runCommand(wasmCmd) == 2);
  CHECK(!std::filesystem::exists(wasmPath));
  CHECK(readFile(compileErrPath).find("vector literal requires heap_alloc effect") != std::string::npos);
}

TEST_CASE("primec wasm wasi invalid ppm write requires heap_alloc for vector literal") {
  const std::filesystem::path tempRoot =
      testScratchPath("") / "primec_wasm_ppm_write_invalid_runtime";
  std::error_code ec;
  std::filesystem::remove_all(tempRoot, ec);
  std::filesystem::create_directories(tempRoot, ec);
  REQUIRE(!ec);

  const std::string source = R"(
import /std/image/*
import /std/collections/*

[effects(io_out, file_write), return<int>]
main() {
  [vector<i32>] pixels{vector<i32>(255i32, 0i32, 0i32)}
  [Result<ImageError>] status{/std/image/ppm/write("output.ppm"utf8, 2i32, 1i32, pixels)}
  print_line(Result.why(status))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_emit_wasm_ppm_write_invalid.prime", source);
  const std::string wasmPath = (tempRoot / "ppm_write_invalid.wasm").string();
  const std::string compileErrPath = (tempRoot / "ppm_write_invalid_compile_err.txt").string();
  const std::string wasmCmd = "./primec --emit=wasm --wasm-profile wasi " + quoteShellArg(srcPath) + " -o " +
                              quoteShellArg(wasmPath) + " --entry /main 2> " + quoteShellArg(compileErrPath);
  CHECK(runCommand(wasmCmd) == 2);
  CHECK(!std::filesystem::exists(wasmPath));
  CHECK(readFile(compileErrPath).find("vector literal requires heap_alloc effect") != std::string::npos);
}


TEST_SUITE_END();
