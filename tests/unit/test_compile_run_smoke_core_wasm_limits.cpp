#include "test_compile_run_helpers.h"

#include "test_compile_run_smoke_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.smoke");

TEST_CASE("primec wasm documented limit IDs have conformance coverage") {
  struct WasmSectionInfo {
    uint8_t id = 0;
    size_t payloadOffset = 0;
    uint32_t payloadSize = 0;
  };
  struct WasmImportInfo {
    std::string module;
    std::string name;
  };

  const auto readBinary = [](const std::string &path, std::vector<uint8_t> &out) {
    out.clear();
    std::ifstream file(path, std::ios::binary);
    if (!file.good()) {
      return false;
    }
    file.seekg(0, std::ios::end);
    const std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    if (size <= 0) {
      return false;
    }
    out.resize(static_cast<size_t>(size));
    file.read(reinterpret_cast<char *>(out.data()), size);
    return file.good() || file.eof();
  };

  const auto readU32Leb = [](const std::vector<uint8_t> &bytes, size_t &cursor, uint32_t &value) {
    value = 0;
    uint32_t shift = 0;
    for (int i = 0; i < 5; ++i) {
      if (cursor >= bytes.size()) {
        return false;
      }
      const uint8_t byte = bytes[cursor++];
      value |= static_cast<uint32_t>(byte & 0x7fu) << shift;
      if ((byte & 0x80u) == 0) {
        return true;
      }
      shift += 7;
    }
    return false;
  };

  const auto parseSections = [&](const std::vector<uint8_t> &bytes, std::vector<WasmSectionInfo> &sections) {
    sections.clear();
    if (bytes.size() < 8) {
      return false;
    }
    const std::vector<uint8_t> wasmHeader = {0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00};
    for (size_t i = 0; i < wasmHeader.size(); ++i) {
      if (bytes[i] != wasmHeader[i]) {
        return false;
      }
    }
    size_t cursor = 8;
    while (cursor < bytes.size()) {
      WasmSectionInfo section;
      section.id = bytes[cursor++];
      if (!readU32Leb(bytes, cursor, section.payloadSize)) {
        return false;
      }
      section.payloadOffset = cursor;
      if (cursor + section.payloadSize > bytes.size()) {
        return false;
      }
      sections.push_back(section);
      cursor += section.payloadSize;
    }
    return true;
  };

  const auto parseImportSection = [&](const std::vector<uint8_t> &bytes,
                                      const std::vector<WasmSectionInfo> &sections,
                                      std::vector<WasmImportInfo> &imports) {
    imports.clear();
    const WasmSectionInfo *importSection = nullptr;
    for (const WasmSectionInfo &section : sections) {
      if (section.id == 2) {
        importSection = &section;
        break;
      }
    }
    if (importSection == nullptr) {
      return false;
    }

    auto readName = [&](size_t &cursor, const size_t limit, std::string &out) {
      uint32_t length = 0;
      if (!readU32Leb(bytes, cursor, length)) {
        return false;
      }
      if (cursor + length > limit) {
        return false;
      }
      out.assign(reinterpret_cast<const char *>(bytes.data() + cursor), length);
      cursor += length;
      return true;
    };

    size_t cursor = importSection->payloadOffset;
    const size_t limit = importSection->payloadOffset + importSection->payloadSize;
    uint32_t importCount = 0;
    if (!readU32Leb(bytes, cursor, importCount)) {
      return false;
    }
    for (uint32_t i = 0; i < importCount; ++i) {
      WasmImportInfo entry;
      if (!readName(cursor, limit, entry.module) || !readName(cursor, limit, entry.name)) {
        return false;
      }
      if (cursor >= limit) {
        return false;
      }
      const uint8_t kind = bytes[cursor++];
      if (kind != 0x00) {
        return false;
      }
      uint32_t typeIndex = 0;
      if (!readU32Leb(bytes, cursor, typeIndex)) {
        return false;
      }
      imports.push_back(entry);
    }
    return cursor == limit;
  };

  {
    CAPTURE("WASM-LIMIT-MEM-ON-DEMAND");
    const std::string source = R"(
[return<int>]
main() {
  return(plus(2i32, 3i32))
}
)";
    const std::string srcPath = writeTemp("compile_emit_wasm_limit_mem_on_demand.prime", source);
    const std::string wasmPath =
        (testScratchPath("") / "primec_emit_wasm_limit_mem_on_demand.wasm").string();
    const std::string errPath =
        (testScratchPath("") / "primec_emit_wasm_limit_mem_on_demand.err").string();
    const std::string wasmCmd = "./primec --emit=wasm --wasm-profile browser --default-effects none " +
                                quoteShellArg(srcPath) + " -o " + quoteShellArg(wasmPath) + " --entry /main 2> " +
                                quoteShellArg(errPath);
    CHECK(runCommand(wasmCmd) == 0);

    std::vector<uint8_t> bytes;
    REQUIRE(readBinary(wasmPath, bytes));
    std::vector<WasmSectionInfo> sections;
    REQUIRE(parseSections(bytes, sections));
    size_t memorySectionCount = 0;
    for (const WasmSectionInfo &section : sections) {
      if (section.id == 5) {
        ++memorySectionCount;
      }
    }
    CHECK(memorySectionCount == 0u);

    std::vector<WasmImportInfo> imports;
    REQUIRE(parseImportSection(bytes, sections, imports));
    CHECK(imports.empty());
  }

  {
    CAPTURE("WASM-LIMIT-MEM-SINGLE");
    CAPTURE("WASM-LIMIT-IMPORTS-WASI");
    const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  print_line("ok"utf8)
  return(args.count())
}
)";
    const std::string srcPath = writeTemp("compile_emit_wasm_limit_runtime_surface.prime", source);
    const std::string wasmPath =
        (testScratchPath("") / "primec_emit_wasm_limit_runtime_surface.wasm").string();
    const std::string errPath =
        (testScratchPath("") / "primec_emit_wasm_limit_runtime_surface.err").string();
    const std::string wasmCmd = "./primec --emit=wasm --wasm-profile wasi " + quoteShellArg(srcPath) + " -o " +
                                quoteShellArg(wasmPath) + " --entry /main 2> " + quoteShellArg(errPath);
    CHECK(runCommand(wasmCmd) == 0);

    std::vector<uint8_t> bytes;
    REQUIRE(readBinary(wasmPath, bytes));
    std::vector<WasmSectionInfo> sections;
    REQUIRE(parseSections(bytes, sections));

    const WasmSectionInfo *memorySection = nullptr;
    for (const WasmSectionInfo &section : sections) {
      if (section.id == 5) {
        CHECK(memorySection == nullptr);
        memorySection = &section;
      }
    }
    REQUIRE(memorySection != nullptr);
    size_t memoryCursor = memorySection->payloadOffset;
    const size_t memoryLimit = memorySection->payloadOffset + memorySection->payloadSize;
    uint32_t memoryCount = 0;
    REQUIRE(readU32Leb(bytes, memoryCursor, memoryCount));
    CHECK(memoryCount == 1u);
    CHECK(memoryCursor <= memoryLimit);

    std::vector<WasmImportInfo> imports;
    REQUIRE(parseImportSection(bytes, sections, imports));
    std::vector<std::string> importModules;
    std::vector<std::string> importNames;
    importModules.reserve(imports.size());
    importNames.reserve(imports.size());
    for (const WasmImportInfo &entry : imports) {
      importModules.push_back(entry.module);
      importNames.push_back(entry.name);
    }
    for (const std::string &moduleName : importModules) {
      CHECK(moduleName == "wasi_snapshot_preview1");
    }
    std::sort(importNames.begin(), importNames.end());
    const std::vector<std::string> expectedNames = {"args_get", "args_sizes_get", "fd_write"};
    CHECK(importNames == expectedNames);
  }

  struct DiagnosticLimitCase {
    std::string limitId;
    std::string name;
    std::string profile;
    std::string source;
    std::string expectedMessage;
  };
  const std::vector<DiagnosticLimitCase> diagnosticsCases = {
      {
          "WASM-LIMIT-PROFILE-BROWSER",
          "browser_rejects_effects",
          "browser",
          R"(
[return<int> effects(io_out)]
main() {
  return(0i32)
}
)",
          "unsupported effect mask bits for wasm-browser target",
      },
      {
          "WASM-LIMIT-PROFILE-BROWSER",
          "browser_rejects_wasi_opcodes",
          "browser",
          R"(
[return<int>]
main([array<string>] args) {
  return(args.count())
}
)",
          "unsupported opcode for wasm-browser target",
      },
      {
          "WASM-LIMIT-PROFILE-WASI-ALLOWLIST",
          "wasi_rejects_unsupported_opcode",
          "wasi",
          R"(
[return<int>]
main() {
  [i32 mut] value{1i32}
  [Reference<i32>] ref{location(value)}
  return(dereference(ref))
}
)",
          "unsupported opcode for wasm target",
      },
  };
  for (const auto &diagCase : diagnosticsCases) {
    CAPTURE(diagCase.limitId);
    CAPTURE(diagCase.name);
    const std::string srcPath = writeTemp("compile_emit_wasm_limit_diag_" + diagCase.name + ".prime", diagCase.source);
    const std::string wasmPath =
        (testScratchPath("") / ("primec_emit_wasm_limit_diag_" + diagCase.name + ".wasm")).string();
    const std::string errPath =
        (testScratchPath("") / ("primec_emit_wasm_limit_diag_" + diagCase.name + ".json")).string();
    const std::string wasmCmd = "./primec --emit=wasm --wasm-profile " + diagCase.profile +
                                " --default-effects none " + quoteShellArg(srcPath) + " -o " +
                                quoteShellArg(wasmPath) + " --entry /main --emit-diagnostics 2> " +
                                quoteShellArg(errPath);
    CHECK(runCommand(wasmCmd) == 2);
    const std::string diagnostics = readFile(errPath);
    CHECK(diagnostics.find(diagCase.expectedMessage) != std::string::npos);
    CHECK(diagnostics.find("\"notes\":[\"backend: wasm\",\"stage: ir-validate\"]") != std::string::npos);
  }
}


TEST_SUITE_END();
