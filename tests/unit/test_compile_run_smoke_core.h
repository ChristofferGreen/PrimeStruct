TEST_SUITE_BEGIN("primestruct.compile.run.smoke");

static constexpr std::string_view IrResultOkUnsupportedMessage =
    "IR backends only support Result.ok with supported payload values";

static bool compileAcrossBackendsOrExpectUnsupported(const std::string &nameStem,
                                                     const std::string &compileExeCmd,
                                                     const std::string &exePath,
                                                     const std::string &runVmCmd,
                                                     const std::string &compileNativeCmd,
                                                     const std::string &nativePath,
                                                     const std::string &nativeMessage,
                                                     const std::string &vmMessage) {
  const std::filesystem::path tempRoot = testScratchPath("");
  const std::string exeErrPath = (tempRoot / (nameStem + "_exe_err.txt")).string();
  const std::string vmErrPath = (tempRoot / (nameStem + "_vm_err.txt")).string();
  const std::string nativeErrPath = (tempRoot / (nameStem + "_native_err.txt")).string();

  const int exeCode = runCommand(compileExeCmd + " 2> " + quoteShellArg(exeErrPath));
  if (exeCode == 0) {
    return true;
  }

  CHECK(exeCode == 2);
  CHECK_FALSE(std::filesystem::exists(exePath));
  CHECK(readFile(exeErrPath).find(nativeMessage) != std::string::npos);

  const int vmCode = runCommand(runVmCmd + " 2> " + quoteShellArg(vmErrPath));
  CHECK(vmCode == 2);
  CHECK(readFile(vmErrPath).find(vmMessage) != std::string::npos);

  const int nativeCode = runCommand(compileNativeCmd + " 2> " + quoteShellArg(nativeErrPath));
  CHECK(nativeCode == 2);
  CHECK_FALSE(std::filesystem::exists(nativePath));
  CHECK(readFile(nativeErrPath).find(nativeMessage) != std::string::npos);
  return false;
}

static bool compileWasmWasiOrExpectUnsupported(const std::string &srcPath,
                                               const std::string &wasmPath,
                                               const std::string &errPath) {
  const int code = compileWasmWasiProgram(srcPath, wasmPath, errPath);
  if (code == 0) {
    return true;
  }
  CHECK(code == 2);
  CHECK_FALSE(std::filesystem::exists(wasmPath));
  const std::string error = readFile(errPath);
  CHECK((error.find("Wasm IR validation error") != std::string::npos ||
         error.find("unsupported opcode for wasm target") != std::string::npos ||
         error.find("only supports arithmetic/comparison/clamp/min/max/abs/sign/saturate/convert/pointer/assign/increment/decrement calls in expressions") !=
             std::string::npos));
  return false;
}

static bool runWasmCompileCommandOrExpectUnsupported(const std::string &wasmCmd,
                                                     const std::string &wasmPath,
                                                     const std::string &errPath) {
  const int code = runCommand(wasmCmd);
  if (code == 0) {
    return true;
  }
  CHECK(code == 2);
  CHECK_FALSE(std::filesystem::exists(wasmPath));
  const std::string error = readFile(errPath);
  CHECK((error.find("Wasm IR validation error") != std::string::npos ||
         error.find("unsupported opcode for wasm target") != std::string::npos ||
         error.find("only supports arithmetic/comparison/clamp/min/max/abs/sign/saturate/convert/pointer/assign/increment/decrement calls in expressions") !=
             std::string::npos));
  return false;
}


#include "test_compile_run_smoke_basic.h"
#include "test_compile_run_smoke_gfx_imports.h"
#include "test_compile_run_smoke_gfx_entrypoints.h"
#include "test_compile_run_smoke_gfx_end_to_end.h"
#include "test_compile_run_smoke_wasm_core.h"
#include "test_compile_run_smoke_wasm_wasi_core.h"
#include "test_compile_run_smoke_wasm_wasi_png_write.h"
#include "test_compile_run_smoke_wasm_wasi_png_decode.h"
#include "test_compile_run_smoke_wasm_wasi_png_interlaced.h"
#include "test_compile_run_smoke_wasm_profiles.h"
#include "test_compile_run_smoke_wasm_limits.h"
#include "test_compile_run_smoke_contracts_and_cli.h"
#include "test_compile_run_smoke_debug_and_docs.h"
#include "test_compile_run_smoke_demo_scripts.h"

TEST_SUITE_END();
