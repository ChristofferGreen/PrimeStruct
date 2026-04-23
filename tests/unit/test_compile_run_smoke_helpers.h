#pragma once

#include "primec/IrSerializer.h"
#include "primec/Options.h"
#include "primec/OptionsParser.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <vector>

inline std::string makeDapFrame(const std::string &payload) {
  return "Content-Length: " + std::to_string(payload.size()) + "\r\n\r\n" + payload;
}

inline std::vector<std::string> parseDapFrames(const std::string &raw, bool &ok) {
  ok = true;
  std::vector<std::string> payloads;
  size_t pos = 0;
  while (pos < raw.size()) {
    const size_t headerEnd = raw.find("\r\n\r\n", pos);
    if (headerEnd == std::string::npos) {
      ok = false;
      return {};
    }
    const std::string headers = raw.substr(pos, headerEnd - pos);
    pos = headerEnd + 4;

    size_t contentLength = 0;
    bool sawContentLength = false;
    size_t headerPos = 0;
    while (headerPos <= headers.size()) {
      const size_t lineEnd = headers.find("\r\n", headerPos);
      const size_t splitEnd = lineEnd == std::string::npos ? headers.size() : lineEnd;
      const std::string line = headers.substr(headerPos, splitEnd - headerPos);
      const size_t colon = line.find(':');
      if (colon != std::string::npos) {
        std::string key = line.substr(0, colon);
        std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) {
          return static_cast<char>(std::tolower(c));
        });
        std::string value = line.substr(colon + 1);
        value.erase(value.begin(),
                    std::find_if(value.begin(), value.end(), [](unsigned char c) { return !std::isspace(c); }));
        value.erase(
            std::find_if(value.rbegin(), value.rend(), [](unsigned char c) { return !std::isspace(c); }).base(),
            value.end());
        if (key == "content-length") {
          sawContentLength = true;
          try {
            contentLength = static_cast<size_t>(std::stoull(value));
          } catch (...) {
            ok = false;
            return {};
          }
        }
      }
      if (lineEnd == std::string::npos) {
        break;
      }
      headerPos = lineEnd + 2;
    }

    if (!sawContentLength || pos + contentLength > raw.size()) {
      ok = false;
      return {};
    }
    payloads.push_back(raw.substr(pos, contentLength));
    pos += contentLength;
  }
  return payloads;
}

inline int compileWasmWasiProgram(const std::string &srcPath,
                                  const std::string &wasmPath,
                                  const std::string &errPath) {
  const std::string wasmCmd = "./primec --emit=wasm --wasm-profile wasi " + quoteShellArg(srcPath) + " -o " +
                              quoteShellArg(wasmPath) + " --entry /main 2> " + quoteShellArg(errPath);
  return runCommand(wasmCmd);
}

inline void checkWasmWasiRuntimeInDir(const std::filesystem::path &tempRoot,
                                      const std::string &wasmPath,
                                      const std::string &outPath,
                                      int expectedExitCode,
                                      const std::string &expectedStdout) {
  if (!hasWasmtime()) {
    return;
  }

  const std::string wasmRunCmd = "wasmtime --invoke main --dir=" + quoteShellArg(tempRoot.string()) + " " +
                                 quoteShellArg(wasmPath) + " > " + quoteShellArg(outPath);
  CHECK(runCommand(wasmRunCmd) == expectedExitCode);
  CHECK(readFile(outPath) == expectedStdout);
}

inline constexpr std::string_view IrResultOkUnsupportedMessage =
    "IR backends only support Result.ok with supported payload values";
inline constexpr std::string_view NativeArrayLiteralUnsupportedMessage =
    "native backend only supports numeric/bool/string array literals";
inline constexpr std::string_view VmArrayLiteralUnsupportedMessage =
    "vm backend only supports numeric/bool/string array literals";

inline bool compileAcrossBackendsOrExpectUnsupported(const std::string &nameStem,
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

inline bool compileWasmWasiOrExpectUnsupported(const std::string &srcPath,
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

inline bool runWasmCompileCommandOrExpectUnsupported(const std::string &wasmCmd,
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
