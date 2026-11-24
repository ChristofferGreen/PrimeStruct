#include "third_party/doctest.h"

#include "primec/backend/WasmEmitter.h"
#include "primec/testing/TestScratch.h"

#include "test_ir_optimizer_helpers.h"
#include "test_ir_runtime_programs.h"
#include "test_ir_vm_run.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#if defined(__unix__) || defined(__APPLE__)
#include <sys/wait.h>
#endif

TEST_SUITE_BEGIN("primestruct.ir.wasm_heap");

// The slot heap of a Wasm module (src/wasm_emitter/WasmEmitterHeap.cpp) is a port of the VM's `VmHeapCore`: the same
// addresses, the same reuse of freed memory and the same fault texts. Every test builds an IR module, runs it on the VM
// and as a WASI module under node, and compares what they print, the exit code and the fault text.

namespace {

using optimizer_test::Outcome;

constexpr const char *NodeRunner = R"(
"use strict";
const fs = require("node:fs");
process.removeAllListeners("warning");
process.on("warning", () => {});
const { WASI } = require("node:wasi");
const wasi = new WASI({ version: "preview1", args: ["program"], returnOnExit: true });
class ProgramExit { constructor(code) { this.code = code; } }
const imports = wasi.getImportObject();
imports.wasi_snapshot_preview1.proc_exit = (code) => { throw new ProgramExit(code); };
const instance = new WebAssembly.Instance(new WebAssembly.Module(fs.readFileSync(process.argv[2])), imports);
if (instance.exports.memory) {
  wasi.initialize(instance);
}
try {
  const result = instance.exports.main();
  process.exitCode = result === undefined ? 0 : Number(BigInt.asUintN(8, BigInt(result)));
} catch (e) {
  if (!(e instanceof ProgramExit)) {
    console.error("unexpected " + e);
    process.exit(99);
  }
  process.exitCode = e.code;
}
)";

// The browser profile has no imports: the result of `main` is printed, a trap is reported as `trap`.
constexpr const char *BrowserRunner = R"(
"use strict";
const fs = require("node:fs");
const instance = new WebAssembly.Instance(new WebAssembly.Module(fs.readFileSync(process.argv[2])), {});
try {
  console.log(String(instance.exports.main()));
} catch (e) {
  console.log(e instanceof WebAssembly.RuntimeError ? "trap" : "unexpected " + e);
}
)";

bool nodeAvailable() {
  static const bool available = std::system("node --version > /dev/null 2>&1") == 0;
  return available;
}

std::filesystem::path scratch(const std::string &name) {
  const std::filesystem::path path = primec::testing::testScratchPath("wasm_heap/" + name);
  std::filesystem::create_directories(path.parent_path());
  return path;
}

std::string readText(const std::filesystem::path &path) {
  std::ifstream file(path, std::ios::binary);
  std::stringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}

struct NodeRun {
  bool built = false;
  int exitCode = -1;
  std::string out;
  std::string err;
  std::string buildError;
};

// Emits the module for the profile and runs it under node with `runner`.
// `nodeFlags` is put after `node` (a deep recursion needs a bigger stack than the default).
NodeRun runOnNode(const primec::IrModule &module,
                  const std::string &name,
                  bool browser,
                  const std::string &nodeFlags = "") {
  NodeRun run;
  primec::WasmEmitter emitter;
  emitter.browserProfile = browser;
  std::vector<uint8_t> bytes;
  if (!emitter.emitModule(module, bytes, run.buildError)) {
    return run;
  }
  run.built = true;
  const std::filesystem::path wasmPath = scratch(name + ".wasm");
  std::ofstream(wasmPath, std::ios::binary)
      .write(reinterpret_cast<const char *>(bytes.data()),
             static_cast<std::streamsize>(bytes.size()));
  const std::filesystem::path runnerPath =
      scratch(browser ? "browser_runner.cjs" : "wasi_runner.cjs");
  std::ofstream(runnerPath) << (browser ? BrowserRunner : NodeRunner);
  const std::filesystem::path outPath = scratch(name + ".stdout");
  const std::filesystem::path errPath = scratch(name + ".stderr");
  const std::string command = "node " + nodeFlags + " '" + runnerPath.string() + "' '" +
                              wasmPath.string() + "' > '" + outPath.string() + "' 2> '" +
                              errPath.string() + "'";
  const int status = std::system(command.c_str());
#if defined(__unix__) || defined(__APPLE__)
  run.exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#else
  run.exitCode = status;
#endif
  run.out = readText(outPath);
  run.err = readText(errPath);
  return run;
}

// The Wasm module behaves like the VM: stdout, exit code (the result of `main` modulo 256, or 3 with the fault text).
void expectMatchesVm(const primec::IrModule &module,
                     const std::string &name,
                     const std::string &nodeFlags = "",
                     bool mustSucceed = false) {
  const Outcome expected = optimizer_test::run(module, {"program"});
  if (mustSucceed) {
    CHECK_MESSAGE(expected.ok, name << ": " << expected.error);
  }
  const NodeRun actual = runOnNode(module, name, false, nodeFlags);
  INFO(name);
  CAPTURE(actual.err);
  REQUIRE_MESSAGE(actual.built, actual.buildError);
  CHECK(actual.out == expected.output);
  if (expected.ok) {
    CHECK(actual.exitCode ==
          static_cast<int>(static_cast<uint8_t>(static_cast<int32_t>(expected.result))));
    CHECK(actual.err.empty());
  } else {
    CHECK(actual.exitCode == 3);
    CHECK(actual.err == "VM error: " + expected.error + "\n");
  }
}

primec::IrModule mainOf(const std::vector<std::string> &lines) {
  primec::IrModule module = optimizer_test::moduleOf({});
  std::vector<primec::IrInstruction> code;
  for (const std::string &line : lines) {
    code.push_back(optimizer_test::assembleOne(line));
  }
  module.functions[0] = optimizer_test::functionOf("/main", std::move(code));
  module.functions[0].metadata.effectMask = primec::EffectIoOut;
  return module;
}

// The module with the string heap statistic function imported as host function 0 (`__pstext_stat(u64) -> u64`).
primec::IrModule withStat(primec::IrModule module) {
  primec::IrHostImport stat;
  stat.name = "std/text/__pstext_stat";
  stat.parameters = {primec::IrHostValueKind::U64};
  stat.returnKind = primec::IrHostValueKind::U64;
  module.hostImports.push_back(stat);
  return module;
}

// A random sequence of allocations, frees and reallocations over eight address variables, printing every address
// and the first value of every block (which must be zero when it is new), so best fit, tail extension, merging of
// free runs, generations and in-place growth all show in the output.
primec::IrModule randomHeapProgram(uint64_t seed, int steps) {
  uint64_t state = seed * 2685821657736338717ull + 1442695040888963407ull;
  const auto next = [&](uint64_t bound) {
    state ^= state << 13;
    state ^= state >> 7;
    state ^= state << 17;
    return state % bound;
  };
  constexpr int Variables = 8;
  std::vector<bool> live(Variables, false);
  std::vector<std::string> code;
  const auto printFirstValue = [&](int variable) {
    code.push_back("LoadLocal " + std::to_string(variable));
    code.push_back("LoadIndirect");
    code.push_back("PrintI64 1");
  };
  const auto storeMarker = [&](int variable, uint64_t value) {
    code.push_back("LoadLocal " + std::to_string(variable));
    code.push_back("PushI64 " + std::to_string(value));
    code.push_back("StoreIndirect");
    code.push_back("Pop");
  };
  const auto freeVariable = [&](int variable) {
    code.push_back("LoadLocal " + std::to_string(variable));
    code.push_back("HeapFree");
    code.push_back("PushI64 0");
    code.push_back("StoreLocal " + std::to_string(variable));
    live[static_cast<size_t>(variable)] = false;
  };
  for (int step = 0; step < steps; ++step) {
    const int variable = static_cast<int>(next(Variables));
    const uint64_t choice = next(10);
    const uint64_t count = next(10);
    const bool isLive = live[static_cast<size_t>(variable)];
    if (choice < 5 || (!isLive && choice < 8)) {
      if (isLive) {
        freeVariable(variable);
      }
      code.push_back("PushI64 " + std::to_string(count));
      code.push_back("HeapAlloc");
      code.push_back("StoreLocal " + std::to_string(variable));
      code.push_back("LoadLocal " + std::to_string(variable));
      code.push_back("PrintU64 1");
      if (count != 0) {
        printFirstValue(variable);
        storeMarker(variable, 100 + static_cast<uint64_t>(step));
        live[static_cast<size_t>(variable)] = true;
      }
    } else if (isLive && choice < 8) {
      freeVariable(variable);
    } else if (isLive) {
      code.push_back("LoadLocal " + std::to_string(variable));
      code.push_back("PushI64 " + std::to_string(count));
      code.push_back("HeapRealloc");
      code.push_back("StoreLocal " + std::to_string(variable));
      code.push_back("LoadLocal " + std::to_string(variable));
      code.push_back("PrintU64 1");
      if (count == 0) {
        live[static_cast<size_t>(variable)] = false;
      } else {
        printFirstValue(variable);
      }
    }
  }
  for (int variable = 0; variable < Variables; ++variable) {
    if (live[static_cast<size_t>(variable)]) {
      freeVariable(variable);
    }
  }
  code.push_back("PushI32 0");
  code.push_back("ReturnI32");
  return mainOf(code);
}

} // namespace

TEST_CASE("wasm heap alloc, free, realloc and indirect access match the VM") {
  if (!nodeAvailable()) {
    MESSAGE("SKIP: node is unavailable");
    return;
  }
  expectMatchesVm(optimizer_test::heapProgram(), "heap");
  expectMatchesVm(optimizer_test::heapReuseProgram(), "heap_reuse");
}

TEST_CASE("wasm heap faults carry the VM's message and exit code 3") {
  if (!nodeAvailable()) {
    MESSAGE("SKIP: node is unavailable");
    return;
  }
  int checked = 0;
  for (const optimizer_test::FaultProgram &fault : optimizer_test::faultPrograms()) {
    const bool heapFault = fault.name.find("free") != std::string::npos ||
                           fault.name.find("realloc") != std::string::npos ||
                           fault.name.find("stale") != std::string::npos ||
                           fault.name.find("use_after") != std::string::npos;
    if (!heapFault) {
      continue;
    }
    CAPTURE(fault.name);
    expectMatchesVm(fault.module, "fault_" + fault.name);
    ++checked;
  }
  CHECK(checked >= 5);
}

TEST_CASE("a wasm allocation the heap cannot serve is the VM's overflow fault") {
  if (!nodeAvailable()) {
    MESSAGE("SKIP: node is unavailable");
    return;
  }
  // More slots than the VM's 2^37 limit: both fault the same way. (The Wasm heap holds at most WasmHeapMaxSlots.)
  expectMatchesVm(mainOf({"PushI64 1099511627776", "HeapAlloc", "Pop", "ReturnVoid"}), "overflow");
  const NodeRun beyond =
      runOnNode(mainOf({"PushI64 134217729", "HeapAlloc", "Pop", "ReturnVoid"}), "capacity", false);
  REQUIRE(beyond.built);
  CHECK(beyond.exitCode == 3);
  CHECK(beyond.err == "VM error: VM heap allocation overflow\n");
}

TEST_CASE(
    "a wasm heap holds more slots than it starts with and reads and writes the last of them") {
  if (!nodeAvailable()) {
    MESSAGE("SKIP: node is unavailable");
    return;
  }
  // 2,097,153 slots (the old fixed capacity plus one): the first and the last slot are written and read back, the block
  // grows past 4 million slots (a move), the old last slot is still there, and the freed block is reused.
  expectMatchesVm(mainOf({"PushI64 2097153",  "HeapAlloc",        "StoreLocal 0",
                          "LoadLocal 0",      "PrintU64 1",       "LoadLocal 0",
                          "PushI64 7",        "StoreIndirect",    "Pop",
                          "LoadLocal 0",      "PushI64 33554432", "AddI64",
                          "PushI64 9",        "StoreIndirect",    "Pop",
                          "LoadLocal 0",      "LoadIndirect",     "PrintI64 1",
                          "LoadLocal 0",      "PushI64 33554432", "AddI64",
                          "LoadIndirect",     "PrintI64 1",       "LoadLocal 0",
                          "PushI64 4200000",  "HeapRealloc",      "StoreLocal 0",
                          "LoadLocal 0",      "PrintU64 1",       "LoadLocal 0",
                          "LoadIndirect",     "PrintI64 1",       "LoadLocal 0",
                          "PushI64 33554432", "AddI64",           "LoadIndirect",
                          "PrintI64 1",       "LoadLocal 0",      "HeapFree",
                          "PushI64 3",        "HeapAlloc",        "PrintU64 1",
                          "PushI32 0",        "ReturnI32"}),
                  "beyond_old_capacity");
}

TEST_CASE("wasm heap regions that move keep every value and address") {
  if (!nodeAvailable()) {
    MESSAGE("SKIP: node is unavailable");
    return;
  }
  // Sixty blocks of 1000 slots (60,000 slots, past the initial regions: several moves), each holding its number at its
  // first slot; every other block is freed, new blocks reuse the holes, and every first slot is printed at the end.
  std::vector<std::string> code;
  constexpr int Blocks = 60;
  for (int i = 0; i < Blocks; ++i) {
    code.push_back("PushI64 1000");
    code.push_back("HeapAlloc");
    code.push_back("StoreLocal " + std::to_string(i));
    code.push_back("LoadLocal " + std::to_string(i));
    code.push_back("PrintU64 1");
    code.push_back("LoadLocal " + std::to_string(i));
    code.push_back("PushI64 " + std::to_string(1000 + i));
    code.push_back("StoreIndirect");
    code.push_back("Pop");
  }
  for (int i = 0; i < Blocks; i += 2) {
    code.push_back("LoadLocal " + std::to_string(i));
    code.push_back("HeapFree");
  }
  for (int i = 0; i < Blocks; i += 4) {
    code.push_back("PushI64 1500");
    code.push_back("HeapAlloc");
    code.push_back("StoreLocal " + std::to_string(i));
    code.push_back("LoadLocal " + std::to_string(i));
    code.push_back("PrintU64 1");
  }
  for (int i = 1; i < Blocks; i += 2) {
    code.push_back("LoadLocal " + std::to_string(i));
    code.push_back("LoadIndirect");
    code.push_back("PrintI64 1");
  }
  code.push_back("PushI32 0");
  code.push_back("ReturnI32");
  expectMatchesVm(mainOf(code), "regions_move");
  // The count of live blocks (a global the move leaves alone) is still what the VM says.
  const std::vector<std::string> counted = {"PushI64 20000",
                                            "HeapAlloc",
                                            "Pop",
                                            "PushI64 40000",
                                            "HeapAlloc",
                                            "Pop",
                                            "PushI64 8",
                                            "CallHost 0",
                                            "PrintU64 1",
                                            "PushI32 0",
                                            "ReturnI32"};
  expectMatchesVm(withStat(mainOf(counted)), "regions_move_stat");
}

namespace {

// A recursion whose frames take a local address (so they live on the shadow stack) and each allocate heap: the stack
// outgrows its pages and the heap outgrows its regions, in turn, so the memory is relocated both ways and every frame
// still reads, through its address, what it wrote into its own local after the callee returned.
primec::IrModule recursionWithHeapProgram(int depth, int frameLocals) {
  primec::IrModule module = optimizer_test::moduleOf({});
  const std::string marker = std::to_string(frameLocals - 1);
  module.functions[0] =
      optimizer_test::functionOf("/main",
                                 {optimizer_test::assembleOne("PushI64 " + std::to_string(depth)),
                                  optimizer_test::assembleOne("Call 1"),
                                  optimizer_test::assembleOne("PrintI64 1"),
                                  optimizer_test::assembleOne("PushI32 0"),
                                  optimizer_test::assembleOne("ReturnI32")});
  std::vector<primec::IrInstruction> body;
  for (const std::string &line : std::vector<std::string>{"StoreLocal 0",
                                                          "PushI64 3",
                                                          "StoreLocal " + marker,
                                                          "PushI64 100",
                                                          "HeapAlloc",
                                                          "Pop",
                                                          "LoadLocal 0",
                                                          "JumpIfZero 18",
                                                          "LoadLocal 0",
                                                          "PushI64 1",
                                                          "SubI64",
                                                          "Call 1",
                                                          "PushI64 5",
                                                          "StoreLocal " + marker,
                                                          "AddressOfLocal " + marker,
                                                          "LoadIndirect",
                                                          "AddI64",
                                                          "ReturnI64",
                                                          "AddressOfLocal " + marker,
                                                          "LoadIndirect",
                                                          "ReturnI64"}) {
    body.push_back(optimizer_test::assembleOne(line));
  }
  module.functions.push_back(optimizer_test::functionOf("/f", std::move(body), 1));
  return module;
}

} // namespace

TEST_CASE("wasm frames and heap regions relocate each other and keep their values") {
  if (!nodeAvailable()) {
    MESSAGE("SKIP: node is unavailable");
    return;
  }
  // 1500 frames of 121 slots (181,500 slots, past the stack's first 131,072) and 150,000 heap slots (past the initial
  // regions): the heap moves behind the stack, then the stack has to move behind the heap.
  // Each wasm frame of the recursion holds the 121 locals of the function: more than the default stack of node.
  expectMatchesVm(recursionWithHeapProgram(1500, 121), "recursion_with_heap", "--stack-size=3900");
}

TEST_CASE("wasm u8 opcodes over slots match the VM") {
  if (!nodeAvailable()) {
    MESSAGE("SKIP: node is unavailable");
    return;
  }
  // ZextU8 cuts a slot to its low byte (4660 is 0x1234, -1 is all ones). LoadU8 reads the low byte of the slot an address
  // names (a heap slot and a local); StoreU8 writes the byte as the whole slot and leaves the byte on the stack.
  expectMatchesVm(
      mainOf({"PushI64 4660", "ZextU8",           "PrintI64 1",   "PushI64 -1",       "ZextU8",
              "PrintI64 1",   "PushI32 7",        "ZextU8",       "PrintI64 1",       "PushI64 3",
              "HeapAlloc",    "StoreLocal 0",     "LoadLocal 0",  "PushI64 4660",     "StoreU8",
              "PrintI64 1",   "LoadLocal 0",      "LoadIndirect", "PrintI64 1",       "LoadLocal 0",
              "PushI64 4660", "StoreIndirect",    "Pop",          "LoadLocal 0",      "LoadU8",
              "PrintI64 1",   "PushI64 300",      "StoreLocal 1", "AddressOfLocal 1", "LoadU8",
              "PrintI64 1",   "AddressOfLocal 1", "PushI64 513",  "StoreU8",          "Pop",
              "LoadLocal 1",  "PrintI64 1",       "PushI32 0",    "ReturnI32"}),
      "u8_over_slots");
}

TEST_CASE("wasm u8 loads and stores of a bad address are the VM's faults") {
  if (!nodeAvailable()) {
    MESSAGE("SKIP: node is unavailable");
    return;
  }
  expectMatchesVm(mainOf({"PushI64 17", "LoadU8", "Pop", "ReturnVoid"}), "u8_load_unaligned");
  expectMatchesVm(mainOf({"PushI64 0", "LoadU8", "Pop", "ReturnVoid"}), "u8_load_null");
  expectMatchesVm(mainOf({"PushI64 4096", "PushI64 1", "StoreU8", "Pop", "ReturnVoid"}),
                  "u8_store_invalid");
}

TEST_CASE("wasm heap addresses of random alloc, free and realloc sequences match the VM") {
  if (!nodeAvailable()) {
    MESSAGE("SKIP: node is unavailable");
    return;
  }
  for (const uint64_t seed : {1ull, 2ull, 3ull, 4ull, 5ull}) {
    const primec::IrModule module = randomHeapProgram(seed, 220);
    expectMatchesVm(module, "random_" + std::to_string(seed));
    // The sequences are long enough to reuse freed slots: some address carries a generation above 0 (bits 48-62).
    const Outcome expected = optimizer_test::run(module, {"program"});
    bool reused = false;
    std::istringstream lines(expected.output);
    for (std::string line; std::getline(lines, line);) {
      reused = reused || (line.size() >= 19 &&
                          std::stoull(line) >= (uint64_t{1} << 63) + (uint64_t{1} << 48));
    }
    CHECK(reused);
  }
}

TEST_CASE("wasm heap in place growth and merged free runs match the VM") {
  if (!nodeAvailable()) {
    MESSAGE("SKIP: node is unavailable");
    return;
  }
  // a (2), b (2), c (2): free a and b so their runs merge, an allocation of 4 takes the merged run (best fit), the
  // last block grows in place at the end of the heap, and a block with a free run after it grows where it is.
  expectMatchesVm(
      mainOf({"PushI64 2",    "HeapAlloc",    "StoreLocal 0", "PushI64 2",    "HeapAlloc",
              "StoreLocal 1", "PushI64 2",    "HeapAlloc",    "StoreLocal 2", "LoadLocal 0",
              "HeapFree",     "LoadLocal 1",  "HeapFree",     "PushI64 4",    "HeapAlloc",
              "StoreLocal 3", "LoadLocal 3",  "PrintU64 1",   "LoadLocal 2",  "PushI64 5",
              "HeapRealloc",  "StoreLocal 2", "LoadLocal 2",  "PrintU64 1",   "LoadLocal 3",
              "HeapFree",     "PushI64 1",    "HeapAlloc",    "StoreLocal 4", "LoadLocal 4",
              "PrintU64 1",   "PushI64 3",    "HeapAlloc",    "StoreLocal 5", "LoadLocal 5",
              "PrintU64 1",   "LoadLocal 4",  "PushI64 3",    "HeapRealloc",  "StoreLocal 4",
              "LoadLocal 4",  "PrintU64 1",   "PushI32 0",    "ReturnI32"}),
      "growth");
}

TEST_CASE("the browser profile has the heap and traps on a bad address") {
  if (!nodeAvailable()) {
    MESSAGE("SKIP: node is unavailable");
    return;
  }
  const NodeRun value = runOnNode(mainOf({"PushI64 3",
                                          "HeapAlloc",
                                          "StoreLocal 0",
                                          "LoadLocal 0",
                                          "PushI64 42",
                                          "StoreIndirect",
                                          "Pop",
                                          "LoadLocal 0",
                                          "LoadIndirect",
                                          "ReturnI64"}),
                                  "browser_value",
                                  true);
  REQUIRE_MESSAGE(value.built, value.buildError);
  CHECK(value.out == "42\n");
  const NodeRun trap = runOnNode(
      mainOf({"PushI64 9223372036854775824", "HeapFree", "ReturnVoid"}), "browser_trap", true);
  REQUIRE_MESSAGE(trap.built, trap.buildError);
  CHECK(trap.out == "trap\n");
}

TEST_CASE("the browser profile grows its heap past the old capacity") {
  if (!nodeAvailable()) {
    MESSAGE("SKIP: node is unavailable");
    return;
  }
  const NodeRun grown = runOnNode(mainOf({"PushI64 2200000",
                                          "HeapAlloc",
                                          "StoreLocal 0",
                                          "LoadLocal 0",
                                          "PushI64 35199984",
                                          "AddI64",
                                          "PushI64 42",
                                          "StoreIndirect",
                                          "Pop",
                                          "LoadLocal 0",
                                          "PushI64 35199984",
                                          "AddI64",
                                          "LoadIndirect",
                                          "ReturnI64"}),
                                  "browser_grown",
                                  true);
  REQUIRE_MESSAGE(grown.built, grown.buildError);
  CHECK(grown.out == "42\n");
}

TEST_CASE("wasm answers the live allocation count like the VM") {
  if (!nodeAvailable()) {
    MESSAGE("SKIP: node is unavailable");
    return;
  }
  // Statistic 8 after each step: allocations count one each (a zero-slot allocation is none), a free gives one back, a
  // realloc that grows in place or moves keeps the count, a realloc to zero frees, and 0 to 7 read 0 (no text blocks).
  expectMatchesVm(
      withStat(
          mainOf({"PushI64 8",    "CallHost 0",   "PrintU64 1",  "PushI64 3",    "HeapAlloc",
                  "StoreLocal 0", "PushI64 0",    "HeapAlloc",   "Pop",          "PushI64 2",
                  "HeapAlloc",    "StoreLocal 1", "PushI64 8",   "CallHost 0",   "PrintU64 1",
                  "LoadLocal 0",  "PushI64 9",    "HeapRealloc", "StoreLocal 0", "PushI64 8",
                  "CallHost 0",   "PrintU64 1",   "LoadLocal 1", "PushI64 40",   "HeapRealloc",
                  "StoreLocal 1", "PushI64 8",    "CallHost 0",  "PrintU64 1",   "LoadLocal 0",
                  "HeapFree",     "PushI64 8",    "CallHost 0",  "PrintU64 1",   "LoadLocal 1",
                  "PushI64 0",    "HeapRealloc",  "Pop",         "PushI64 8",    "CallHost 0",
                  "PrintU64 1",   "PushI64 0",    "CallHost 0",  "PrintU64 1",   "PushI64 1",
                  "CallHost 0",   "PrintU64 1",   "PushI64 5",   "CallHost 0",   "PrintU64 1",
                  "PushI64 7",    "CallHost 0",   "PrintU64 1",  "PushI32 0",    "ReturnI32"})),
      "stat_live_blocks");
  for (const uint64_t seed : {11ull, 12ull}) {
    // The random programs end with every block freed; the count before and after must be what the VM says.
    primec::IrModule module = withStat(randomHeapProgram(seed, 120));
    std::vector<primec::IrInstruction> &code = module.functions[0].instructions;
    std::vector<primec::IrInstruction> traced;
    for (const primec::IrInstruction &inst : code) {
      traced.push_back(inst);
      if (inst.op == primec::IrOpcode::StoreLocal) {
        traced.push_back(optimizer_test::assembleOne("PushI64 8"));
        traced.push_back(optimizer_test::assembleOne("CallHost 0"));
        traced.push_back(optimizer_test::assembleOne("PrintU64 1"));
      }
    }
    code = std::move(traced);
    expectMatchesVm(module, "stat_random_" + std::to_string(seed));
  }
}

TEST_CASE("the browser profile answers the live allocation count") {
  if (!nodeAvailable()) {
    MESSAGE("SKIP: node is unavailable");
    return;
  }
  const NodeRun counted = runOnNode(withStat(mainOf({"PushI64 2",
                                                     "HeapAlloc",
                                                     "Pop",
                                                     "PushI64 1",
                                                     "HeapAlloc",
                                                     "HeapFree",
                                                     "PushI64 8",
                                                     "CallHost 0",
                                                     "ReturnI64"})),
                                    "browser_stat",
                                    true);
  REQUIRE_MESSAGE(counted.built, counted.buildError);
  CHECK(counted.out == "1\n");
  // A module that never allocates has no heap: the count is 0.
  const NodeRun none = runOnNode(
      withStat(mainOf({"PushI64 8", "CallHost 0", "ReturnI64"})), "browser_stat_none", true);
  REQUIRE_MESSAGE(none.built, none.buildError);
  CHECK(none.out == "0\n");
}

namespace {

// The address of the byte region that starts at granule `granule` and was the `generation`th one made (the VM's
// `Tag | ByteRegionBit | generation << 48 | offset`).
int64_t byteAddress(uint64_t granule, uint64_t generation) {
  return static_cast<int64_t>(0x8000800000000000ull | (generation << 48) | (granule * 8));
}

// `byte[address + offset] = value`, leaving nothing on the stack.
void storeByte(std::vector<std::string> &code,
               const std::string &address,
               uint64_t offset,
               uint64_t value) {
  code.push_back(address);
  code.push_back("PushI64 " + std::to_string(offset));
  code.push_back("AddI64");
  code.push_back("PushI64 " + std::to_string(value));
  code.push_back("StoreU8");
  code.push_back("Pop");
}

// Prints `byte[address + offset]`.
void printByte(std::vector<std::string> &code, const std::string &address, uint64_t offset) {
  code.push_back(address);
  code.push_back("PushI64 " + std::to_string(offset));
  code.push_back("AddI64");
  code.push_back("LoadU8");
  code.push_back("PrintI64 1");
}

// A random sequence of byte region allocations, frees and reallocations over eight address variables, printing every
// address and the first and last byte of every region (zero when it is new), so best fit over footprints, tail
// extension, merging of free runs, generations and the copy of a realloc all show in the output.
primec::IrModule randomByteProgram(uint64_t seed, int steps) {
  uint64_t state = seed * 2685821657736338717ull + 1442695040888963407ull;
  const auto next = [&](uint64_t bound) {
    state ^= state << 13;
    state ^= state >> 7;
    state ^= state << 17;
    return state % bound;
  };
  constexpr int Variables = 8;
  std::vector<uint64_t> counts(Variables, 0);
  std::vector<std::string> code;
  const auto variableAddress = [](int variable) { return "LoadLocal " + std::to_string(variable); };
  const auto freeVariable = [&](int variable) {
    code.push_back(variableAddress(variable));
    code.push_back("HeapFree");
    code.push_back("PushI64 0");
    code.push_back("StoreLocal " + std::to_string(variable));
    counts[static_cast<size_t>(variable)] = 0;
  };
  const auto describe = [&](int variable, uint64_t step) {
    const uint64_t count = counts[static_cast<size_t>(variable)];
    code.push_back(variableAddress(variable));
    code.push_back("PrintU64 1");
    printByte(code, variableAddress(variable), 0);
    printByte(code, variableAddress(variable), count - 1);
    storeByte(code, variableAddress(variable), 0, 1 + step % 250);
    storeByte(code, variableAddress(variable), count - 1, 2 + step % 250);
  };
  for (int step = 0; step < steps; ++step) {
    const int variable = static_cast<int>(next(Variables));
    const uint64_t choice = next(10);
    const uint64_t count = next(5) == 0 ? next(300) : next(40);
    const bool isLive = counts[static_cast<size_t>(variable)] != 0;
    if (choice < 5 || (!isLive && choice < 8)) {
      if (isLive) {
        freeVariable(variable);
      }
      code.push_back("PushI64 " + std::to_string(count));
      code.push_back("HeapAllocBytes");
      code.push_back("StoreLocal " + std::to_string(variable));
      if (count != 0) {
        counts[static_cast<size_t>(variable)] = count;
        describe(variable, static_cast<uint64_t>(step));
      } else {
        code.push_back(variableAddress(variable));
        code.push_back("PrintU64 1");
      }
    } else if (isLive && choice < 8) {
      freeVariable(variable);
    } else if (isLive) {
      code.push_back(variableAddress(variable));
      code.push_back("PushI64 " + std::to_string(count));
      code.push_back("HeapRealloc");
      code.push_back("StoreLocal " + std::to_string(variable));
      if (count == 0) {
        code.push_back(variableAddress(variable));
        code.push_back("PrintU64 1");
        counts[static_cast<size_t>(variable)] = 0;
      } else {
        counts[static_cast<size_t>(variable)] = count;
        describe(variable, static_cast<uint64_t>(step));
      }
    }
  }
  for (int variable = 0; variable < Variables; ++variable) {
    if (counts[static_cast<size_t>(variable)] != 0) {
      freeVariable(variable);
    }
  }
  code.push_back("PushI32 0");
  code.push_back("ReturnI32");
  return mainOf(code);
}

// The address of the `i`th region of a run of equal regions of `granules` granules made one after the other.
std::string regionAddress(int i, uint64_t granules) {
  return "PushI64 " + std::to_string(byteAddress(granules * static_cast<uint64_t>(i),
                                                 static_cast<uint64_t>(i) + 1));
}

} // namespace

TEST_CASE("wasm byte regions allocate, read, write, free and realloc like the VM") {
  if (!nodeAvailable()) {
    MESSAGE("SKIP: node is unavailable");
    return;
  }
  // Five bytes written and read back, the region freed and a new one of the same size made (the same base, a new
  // generation, zeroed), a realloc that keeps the leading bytes, and slot and byte allocations interleaved with the u8
  // access of a slot address still reading the slot.
  std::vector<std::string> code = {
      "PushI64 5", "HeapAllocBytes", "StoreLocal 0", "LoadLocal 0", "PrintU64 1"};
  for (uint64_t i = 0; i < 5; ++i) {
    storeByte(code, "LoadLocal 0", i, 10 + i);
  }
  for (uint64_t i = 0; i < 5; ++i) {
    printByte(code, "LoadLocal 0", i);
  }
  for (const std::string &line : std::vector<std::string>{"LoadLocal 0",
                                                          "HeapFree",
                                                          "PushI64 5",
                                                          "HeapAllocBytes",
                                                          "StoreLocal 1",
                                                          "LoadLocal 1",
                                                          "PrintU64 1"}) {
    code.push_back(line);
  }
  printByte(code, "LoadLocal 1", 0);
  storeByte(code, "LoadLocal 1", 4, 99);
  for (const std::string &line : std::vector<std::string>{"LoadLocal 1",
                                                          "PushI64 20",
                                                          "HeapRealloc",
                                                          "StoreLocal 2",
                                                          "LoadLocal 2",
                                                          "PrintU64 1"}) {
    code.push_back(line);
  }
  printByte(code, "LoadLocal 2", 4);
  printByte(code, "LoadLocal 2", 19);
  for (const std::string &line : std::vector<std::string>{
           "PushI64 3",   "HeapAlloc",    "StoreLocal 3", "LoadLocal 3",  "PrintU64 1",
           "LoadLocal 3", "PushI64 4660", "StoreU8",      "PrintI64 1",   "LoadLocal 3",
           "LoadU8",      "PrintI64 1",   "LoadLocal 3",  "LoadIndirect", "PrintI64 1",
           "LoadLocal 2", "PushI64 3",    "HeapRealloc",  "StoreLocal 2", "LoadLocal 2",
           "PrintU64 1"}) {
    code.push_back(line);
  }
  printByte(code, "LoadLocal 2", 2);
  for (const std::string &line : std::vector<std::string>{"LoadLocal 2",
                                                          "PushI64 0",
                                                          "HeapRealloc",
                                                          "PrintU64 1",
                                                          "LoadLocal 3",
                                                          "HeapFree",
                                                          "PushI32 0",
                                                          "ReturnI32"}) {
    code.push_back(line);
  }
  expectMatchesVm(mainOf(code), "byte_basic");
}

TEST_CASE("wasm byte region faults carry the VM's message and exit code 3") {
  if (!nodeAvailable()) {
    MESSAGE("SKIP: node is unavailable");
    return;
  }
  const std::vector<std::string> alloc16 = {"PushI64 16", "HeapAllocBytes", "StoreLocal 0"};
  const auto withTail = [&](std::vector<std::string> tail) {
    std::vector<std::string> code = alloc16;
    code.insert(code.end(), tail.begin(), tail.end());
    code.push_back("ReturnVoid");
    return mainOf(code);
  };
  const std::vector<std::pair<std::string, primec::IrModule>> faults = {
      {"use_after_free", withTail({"LoadLocal 0", "HeapFree", "LoadLocal 0", "LoadU8", "Pop"})},
      {"store_after_free",
       withTail({"LoadLocal 0", "HeapFree", "LoadLocal 0", "PushI64 1", "StoreU8", "Pop"})},
      {"past_the_end", withTail({"LoadLocal 0", "PushI64 16", "AddI64", "LoadU8", "Pop"})},
      {"store_past_the_end",
       withTail({"LoadLocal 0", "PushI64 16", "AddI64", "PushI64 1", "StoreU8", "Pop"})},
      {"before_the_start", withTail({"LoadLocal 0", "PushI64 1", "SubI64", "LoadU8", "Pop"})},
      {"padding_of_the_footprint",
       mainOf(
           {"PushI64 5", "HeapAllocBytes", "PushI64 5", "AddI64", "LoadU8", "Pop", "ReturnVoid"})},
      {"free_inside_a_region", withTail({"LoadLocal 0", "PushI64 8", "AddI64", "HeapFree"})},
      {"free_unaligned", withTail({"LoadLocal 0", "PushI64 1", "AddI64", "HeapFree"})},
      {"double_free", withTail({"LoadLocal 0", "HeapFree", "LoadLocal 0", "HeapFree"})},
      {"realloc_freed",
       withTail({"LoadLocal 0", "HeapFree", "LoadLocal 0", "PushI64 4", "HeapRealloc", "Pop"})},
      {"realloc_freed_to_zero",
       withTail({"LoadLocal 0", "HeapFree", "LoadLocal 0", "PushI64 0", "HeapRealloc", "Pop"})},
      {"realloc_inside",
       withTail({"LoadLocal 0", "PushI64 8", "AddI64", "PushI64 4", "HeapRealloc", "Pop"})},
      {"stale_after_realloc",
       withTail(
           {"LoadLocal 0", "PushI64 64", "HeapRealloc", "Pop", "LoadLocal 0", "LoadU8", "Pop"})},
      {"slot_access_of_a_byte_address", withTail({"LoadLocal 0", "LoadIndirect", "Pop"})},
      {"slot_store_to_a_byte_address",
       withTail({"LoadLocal 0", "PushI64 1", "StoreIndirect", "Pop"})},
      {"huge_region", mainOf({"PushI64 35184372088832", "HeapAllocBytes", "Pop", "ReturnVoid"})},
      {"slot_u8_fault_with_regions",
       mainOf({"PushI64 4", "HeapAllocBytes", "Pop", "PushI64 17", "LoadU8", "Pop", "ReturnVoid"})},
  };
  for (const auto &[name, module] : faults) {
    CAPTURE(name);
    expectMatchesVm(module, "byte_fault_" + name);
  }
  // A region the byte storage cannot hold (more than 512 MiB) is the VM's overflow fault on Wasm.
  const NodeRun beyond = runOnNode(
      mainOf({"PushI64 600000000", "HeapAllocBytes", "Pop", "ReturnVoid"}), "byte_capacity", false);
  REQUIRE(beyond.built);
  CHECK(beyond.exitCode == 3);
  CHECK(beyond.err == "VM error: VM heap allocation overflow\n");
}

TEST_CASE("wasm byte addresses of random alloc, free and realloc sequences match the VM") {
  if (!nodeAvailable()) {
    MESSAGE("SKIP: node is unavailable");
    return;
  }
  for (const uint64_t seed : {1ull, 2ull, 3ull, 4ull, 5ull, 6ull}) {
    const primec::IrModule module = randomByteProgram(seed, 260);
    expectMatchesVm(module, "random_bytes_" + std::to_string(seed));
    // The sequences are long enough to make many regions: some address carries a generation above 20.
    const Outcome expected = optimizer_test::run(module, {"program"});
    bool many = false;
    std::istringstream lines(expected.output);
    for (std::string line; std::getline(lines, line);) {
      many = many || (line.size() >= 19 &&
                      std::stoull(line) >= 0x8000800000000000ull + (uint64_t{1} << 48) * 20);
    }
    CHECK(many);
  }
}

TEST_CASE("wasm byte regions that move keep every byte and address") {
  if (!nodeAvailable()) {
    MESSAGE("SKIP: node is unavailable");
    return;
  }
  // 100 regions of 1000 bytes (100,000 bytes, past the initial 16 KiB: several moves), each marked at its first and last
  // byte; every other one is freed, new regions reuse the holes, and every marker is read at the end. Region i starts at
  // granule 125 * i and has generation i + 1.
  std::vector<std::string> code;
  constexpr int Regions = 100;
  for (int i = 0; i < Regions; ++i) {
    code.push_back("PushI64 1000");
    code.push_back("HeapAllocBytes");
    code.push_back("PrintU64 1");
    storeByte(code, regionAddress(i, 125), 0, 1 + static_cast<uint64_t>(i));
    storeByte(code, regionAddress(i, 125), 999, 101 + static_cast<uint64_t>(i));
  }
  for (int i = 0; i < Regions; i += 2) {
    code.push_back(regionAddress(i, 125));
    code.push_back("HeapFree");
  }
  for (int i = 0; i < Regions; i += 4) {
    code.push_back("PushI64 1500");
    code.push_back("HeapAllocBytes");
    code.push_back("PrintU64 1");
  }
  for (int i = 1; i < Regions; i += 2) {
    printByte(code, regionAddress(i, 125), 0);
    printByte(code, regionAddress(i, 125), 999);
  }
  // One region of 3 MB, written and read at its last byte, then grown to 5 MB with the byte kept.
  for (const std::string &line :
       std::vector<std::string>{"PushI64 3000000", "HeapAllocBytes", "StoreLocal 7"}) {
    code.push_back(line);
  }
  storeByte(code, "LoadLocal 7", 2999999, 77);
  for (const std::string &line : std::vector<std::string>{"LoadLocal 7",
                                                          "PushI64 5000000",
                                                          "HeapRealloc",
                                                          "StoreLocal 7",
                                                          "LoadLocal 7",
                                                          "PrintU64 1"}) {
    code.push_back(line);
  }
  printByte(code, "LoadLocal 7", 2999999);
  printByte(code, "LoadLocal 7", 4999999);
  code.push_back("PushI32 0");
  code.push_back("ReturnI32");
  expectMatchesVm(mainOf(code), "byte_regions_move");
}

TEST_CASE("wasm byte regions keep many free runs") {
  if (!nodeAvailable()) {
    MESSAGE("SKIP: node is unavailable");
    return;
  }
  // 2,500 regions of 8 bytes (past the initial 2,048 granules), every odd one freed (1,250 free runs, past the initial
  // run table), new regions reuse the holes lowest first, and the even ones still hold their bytes.
  std::vector<std::string> code;
  constexpr int Regions = 2500;
  for (int i = 0; i < Regions; ++i) {
    code.push_back("PushI64 8");
    code.push_back("HeapAllocBytes");
    if (i % 250 == 0) {
      code.push_back("PrintU64 1");
    } else {
      code.push_back("Pop");
    }
  }
  for (int i = 0; i < Regions; i += 2) {
    storeByte(code, regionAddress(i, 1), 7, static_cast<uint64_t>(i % 251));
  }
  for (int i = 1; i < Regions; i += 2) {
    code.push_back(regionAddress(i, 1));
    code.push_back("HeapFree");
  }
  for (int i = 0; i < 40; ++i) {
    code.push_back("PushI64 " + std::to_string(i % 3 == 0 ? 16 : 8));
    code.push_back("HeapAllocBytes");
    code.push_back("PrintU64 1");
  }
  for (int i = 0; i < Regions; i += 100) {
    printByte(code, regionAddress(i, 1), 7);
  }
  code.push_back("PushI32 0");
  code.push_back("ReturnI32");
  expectMatchesVm(mainOf(code), "byte_free_runs");
}

TEST_CASE("the browser profile has byte regions and traps on a bad byte address") {
  if (!nodeAvailable()) {
    MESSAGE("SKIP: node is unavailable");
    return;
  }
  const NodeRun value = runOnNode(mainOf({"PushI64 40000",
                                          "HeapAllocBytes",
                                          "StoreLocal 0",
                                          "LoadLocal 0",
                                          "PushI64 39999",
                                          "AddI64",
                                          "PushI64 42",
                                          "StoreU8",
                                          "Pop",
                                          "LoadLocal 0",
                                          "PushI64 39999",
                                          "AddI64",
                                          "LoadU8",
                                          "ReturnI64"}),
                                  "browser_bytes",
                                  true);
  REQUIRE_MESSAGE(value.built, value.buildError);
  CHECK(value.out == "42\n");
  const NodeRun trap = runOnNode(
      mainOf({"PushI64 4", "HeapAllocBytes", "PushI64 4", "AddI64", "LoadU8", "ReturnI64"}),
      "browser_byte_trap",
      true);
  REQUIRE_MESSAGE(trap.built, trap.buildError);
  CHECK(trap.out == "trap\n");
}

// ---- the string heap (`__pstext_alloc` and its relatives) ----
//
// The string heap of a Wasm module (src/wasm_emitter/WasmEmitterTextHeap.cpp) is a port of the VM's `VmTextHeap`: the same
// size classes, chunks, block addresses (which carry the generation of the chunk's byte region), reuse, and fault texts.

namespace {

constexpr int TextAlloc = 0;
constexpr int TextFree = 1;
constexpr int TextCapacity = 2;
constexpr int TextLength = 3;
constexpr int TextUsable = 4;
constexpr int TextTryAlloc = 5;
constexpr int TextStat = 6;
constexpr int TextSetLimit = 7;
constexpr int TextArenaBegin = 8;
constexpr int TextArenaEnd = 9;
constexpr int TextMove = 10;
constexpr int TextFill = 11;
constexpr int TextCompare = 12;
constexpr int TextFind = 13;
constexpr int TextLiteral = 14;
constexpr int TextArgument = 15;

// The module with the string heap functions imported as host functions 0 to 15 (`TextAlloc` and the others above).
primec::IrModule withText(primec::IrModule module) {
  const auto add =
      [&](const char *name, primec::IrHostValueKind returnKind, size_t parameters = 1) {
        primec::IrHostImport import;
        import.name = std::string("std/text/") + name;
        import.parameters.assign(parameters, primec::IrHostValueKind::U64);
        import.returnKind = returnKind;
        module.hostImports.push_back(import);
      };
  add("__pstext_alloc", primec::IrHostValueKind::U64);
  add("__pstext_free", primec::IrHostValueKind::Void);
  add("__pstext_capacity", primec::IrHostValueKind::U64);
  add("__pstext_length", primec::IrHostValueKind::I32);
  add("__pstext_usable", primec::IrHostValueKind::U64);
  add("__pstext_try_alloc", primec::IrHostValueKind::U64);
  add("__pstext_stat", primec::IrHostValueKind::U64);
  add("__pstext_set_limit", primec::IrHostValueKind::Void);
  add("__pstext_arena_begin", primec::IrHostValueKind::Void, 0);
  add("__pstext_arena_end", primec::IrHostValueKind::Void, 0);
  add("__pstext_move", primec::IrHostValueKind::Void, 3);
  add("__pstext_fill", primec::IrHostValueKind::Void, 3);
  add("__pstext_compare", primec::IrHostValueKind::I32, 3);
  add("__pstext_find", primec::IrHostValueKind::I32, 3);
  add("__pstext_literal", primec::IrHostValueKind::U64);
  add("__pstext_argument", primec::IrHostValueKind::U64);
  return module;
}

std::string callText(int function) {
  return "CallHost " + std::to_string(function);
}

// The block size `VmTextHeap::blockSizeFor` gives a text of `size` bytes (a size of at most 64 KiB).
uint64_t textBlockSize(uint64_t size) {
  if (size <= 32) {
    return 32;
  }
  uint64_t base = 32;
  while (base * 2 < size) {
    base *= 2;
  }
  const uint64_t step = base / 4;
  return base + (size - base + step - 1) / step * step;
}

// Prints the statistics the string heap answers: chunks, bytes in use, bytes free, large blocks, their bytes,
// allocations, open arenas and the bytes they handed out.
void printTextStats(std::vector<std::string> &code) {
  for (const int which : {0, 1, 2, 3, 4, 5, 6, 7}) {
    code.push_back("PushI64 " + std::to_string(which));
    code.push_back(callText(TextStat));
    code.push_back("PrintU64 1");
  }
}

// Prints the address, the capacity, the usable bytes and the length of the block in variable `variable`.
void printTextBlock(std::vector<std::string> &code, int variable) {
  const std::string load = "LoadLocal " + std::to_string(variable);
  code.push_back(load);
  code.push_back("PrintU64 1");
  code.push_back(load);
  code.push_back(callText(TextCapacity));
  code.push_back("PrintU64 1");
  code.push_back(load);
  code.push_back(callText(TextUsable));
  code.push_back("PrintU64 1");
  code.push_back(load);
  code.push_back(callText(TextLength));
  code.push_back("PrintI32 1");
}

void allocText(std::vector<std::string> &code,
               int variable,
               uint64_t size,
               int function = TextAlloc) {
  code.push_back("PushI64 " + std::to_string(size));
  code.push_back(callText(function));
  code.push_back("StoreLocal " + std::to_string(variable));
}

void freeText(std::vector<std::string> &code, int variable) {
  code.push_back("LoadLocal " + std::to_string(variable));
  code.push_back(callText(TextFree));
}

// A random sequence of allocations (sizes from one byte to 64 KiB), text writes, frees and queries over ten address
// variables, printing the address, capacity, usable bytes and length of every new block and the statistics, so size
// classes, chunk growth, the lowest chunk with a free block, the retained free chunk and its return, and the zeroing
// of freed blocks all show in the output.
primec::IrModule
randomTextProgram(uint64_t seed, int steps, bool withLarge = false, bool withArenas = false) {
  uint64_t state = seed * 2685821657736338717ull + 1442695040888963407ull;
  const auto next = [&](uint64_t bound) {
    state ^= state << 13;
    state ^= state >> 7;
    state ^= state << 17;
    return state % bound;
  };
  constexpr int Variables = 10;
  std::vector<uint64_t> size(Variables, 0);
  // the arena depth a block was made at (0 for none): a block of an arena is gone when its arena ends
  std::vector<int> madeAt(Variables, 0);
  int depth = 0;
  const auto endArena = [&](std::vector<std::string> &lines) {
    lines.push_back(callText(TextArenaEnd));
    for (int i = 0; i < Variables; ++i) {
      if (madeAt[static_cast<size_t>(i)] == depth) {
        size[static_cast<size_t>(i)] = 0;
        madeAt[static_cast<size_t>(i)] = 0;
      }
    }
    --depth;
  };
  std::vector<std::string> code;
  for (int step = 0; step < steps; ++step) {
    if (withArenas && next(7) == 0) {
      if (depth == 0 || (depth < 3 && next(2) == 0)) {
        code.push_back(callText(TextArenaBegin));
        ++depth;
      } else {
        endArena(code);
      }
    }
    const int variable = static_cast<int>(next(Variables));
    const uint64_t choice = next(12);
    const bool live = size[static_cast<size_t>(variable)] != 0;
    if (choice < 6 || !live) {
      if (live) {
        freeText(code, variable);
        size[static_cast<size_t>(variable)] = 0;
        madeAt[static_cast<size_t>(variable)] = 0;
      }
      const uint64_t kind = next(100);
      uint64_t request = 1 + next(120);
      if (kind >= 55 && kind < 85) {
        request = 121 + next(2880);
      } else if (kind >= 85 && kind < 97) {
        request = 3001 + next(17000);
      } else if (kind >= 97) {
        request = 20001 + next(45535);
      }
      if (withLarge && next(8) == 0) {
        request = 65537 + next(150000);
      }
      allocText(code, variable, request, next(4) == 0 ? TextTryAlloc : TextAlloc);
      size[static_cast<size_t>(variable)] = request;
      madeAt[static_cast<size_t>(variable)] = depth;
      printTextBlock(code, variable);
      // a prefix of text: the length is what was written (a block holds `blockSize - 1` bytes of text)
      const uint64_t written =
          std::min<uint64_t>(next(6), (request > 65536 ? request : textBlockSize(request)) - 1);
      for (uint64_t i = 0; i < written; ++i) {
        storeByte(code, "LoadLocal " + std::to_string(variable), i, 65 + i);
      }
      code.push_back("LoadLocal " + std::to_string(variable));
      code.push_back(callText(TextLength));
      code.push_back("PrintI32 1");
    } else if (choice < 10) {
      freeText(code, variable);
      size[static_cast<size_t>(variable)] = 0;
      madeAt[static_cast<size_t>(variable)] = 0;
      // a freed block still has its size, and no text
      code.push_back("PushI64 0");
      code.push_back("StoreLocal " + std::to_string(variable));
    } else {
      printTextBlock(code, variable);
    }
    if (step % 5 == 4) {
      printTextStats(code);
    }
  }
  while (depth > 0) {
    endArena(code);
  }
  for (int variable = 0; variable < Variables; ++variable) {
    if (size[static_cast<size_t>(variable)] != 0) {
      freeText(code, variable);
    }
  }
  printTextStats(code);
  code.push_back("PushI32 0");
  code.push_back("ReturnI32");
  return withText(mainOf(code));
}

} // namespace

TEST_CASE("wasm string heap blocks have the VM's addresses, capacities and lengths") {
  if (!nodeAvailable()) {
    MESSAGE("SKIP: node is unavailable");
    return;
  }
  // Blocks of every size class edge, written, queried and freed, then the same sizes again (reuse of the freed blocks
  // in the VM's order), with the statistics after each phase.
  std::vector<std::string> code;
  const std::vector<uint64_t> sizes = {1,    31,   32,   33,    40,    41,    64,    65,    80,
                                       81,   100,  128,  129,   200,   256,   1000,  4095,  4096,
                                       4097, 5000, 9999, 16384, 20000, 32768, 40000, 65535, 65536};
  int variable = 0;
  for (const uint64_t size : sizes) {
    allocText(code, variable, size);
    printTextBlock(code, variable);
    storeByte(code, "LoadLocal " + std::to_string(variable), 0, 72);
    storeByte(code, "LoadLocal " + std::to_string(variable), 1, 105);
    code.push_back("LoadLocal " + std::to_string(variable));
    code.push_back(callText(TextLength));
    code.push_back("PrintI32 1");
    ++variable;
  }
  printTextStats(code);
  for (int i = 0; i < variable; i += 2) {
    freeText(code, i);
  }
  printTextStats(code);
  for (int i = 0; i < variable; i += 2) {
    allocText(code, i, sizes[static_cast<size_t>(i)]);
    printTextBlock(code, i);
  }
  for (int i = 0; i < variable; ++i) {
    freeText(code, i);
  }
  printTextStats(code);
  allocText(code, 0, 0);
  code.push_back("LoadLocal 0");
  code.push_back("PrintU64 1");
  code.push_back("PushI64 0");
  code.push_back(callText(TextCapacity));
  code.push_back("PrintU64 1");
  code.push_back("PushI64 0");
  code.push_back(callText(TextLength));
  code.push_back("PrintI32 1");
  code.push_back("PushI64 0");
  code.push_back(callText(TextUsable));
  code.push_back("PrintU64 1");
  code.push_back("PushI64 0");
  code.push_back(callText(TextFree));
  code.push_back("PushI32 0");
  code.push_back("ReturnI32");
  expectMatchesVm(withText(mainOf(code)), "text_classes");
}

// The block counts of the chunks a size class takes one after the other (the VM's rule: the first chunk is 4096 bytes
// or one block, each further one as large as the class holds, up to 256 KiB), up to and including the second chunk of
// 256 KiB.
std::vector<uint64_t> textChunkPlan(uint64_t blockBytes) {
  std::vector<uint64_t> plan;
  uint64_t held = 0;
  int full = 0;
  while (full < 2) {
    const uint64_t bytes =
        std::min<uint64_t>(std::max<uint64_t>(std::max<uint64_t>(4096, blockBytes), held), 262144);
    plan.push_back(bytes / blockBytes);
    held += bytes;
    full += bytes == 262144 ? 1 : 0;
  }
  return plan;
}

TEST_CASE("wasm string heap chunks grow, fill, empty and return like the VM's") {
  if (!nodeAvailable()) {
    MESSAGE("SKIP: node is unavailable");
    return;
  }
  // 600 blocks of 100 bytes (class 112: chunks of 4096, 4096, 8192, ... bytes), every fifth address printed; every
  // second block freed (the lowest chunk with a free block is reused first), 150 blocks made again, the rest freed in
  // reverse order (chunks that are all free: one kept per class, the others returned), then blocks of three other
  // classes, and the heap emptied again; the statistics after each phase show the chunk counts and bytes.
  std::vector<std::string> code;
  constexpr int Blocks = 600;
  for (int i = 0; i < Blocks; ++i) {
    allocText(code, i, 100);
    if (i % 5 == 0) {
      code.push_back("LoadLocal " + std::to_string(i));
      code.push_back("PrintU64 1");
    }
  }
  printTextStats(code);
  for (int i = 1; i < Blocks; i += 2) {
    freeText(code, i);
  }
  printTextStats(code);
  for (int i = 1; i < Blocks; i += 4) {
    allocText(code, i, 100);
    code.push_back("LoadLocal " + std::to_string(i));
    code.push_back("PrintU64 1");
  }
  printTextStats(code);
  for (int i = Blocks - 1; i >= 0; --i) {
    if (i % 2 == 0 || i % 4 == 1) {
      freeText(code, i);
    }
    if (i % 100 == 0) {
      printTextStats(code);
    }
  }
  for (int round = 0; round < 3; ++round) {
    for (int i = 0; i < 40; ++i) {
      allocText(code, i, 100 + 700 * static_cast<uint64_t>(round));
      code.push_back("LoadLocal " + std::to_string(i));
      code.push_back("PrintU64 1");
    }
    printTextStats(code);
    for (int i = 0; i < 40; ++i) {
      freeText(code, i);
    }
    printTextStats(code);
  }
  code.push_back("PushI32 0");
  code.push_back("ReturnI32");
  expectMatchesVm(withText(mainOf(code)), "text_chunks");
}

TEST_CASE("wasm string heap returns the largest free chunks past the limit of free chunks") {
  if (!nodeAvailable()) {
    MESSAGE("SKIP: node is unavailable");
    return;
  }
  // Six size classes each take chunks up to a second chunk of 256 KiB and free the blocks of that last chunk: a class
  // keeps one chunk that is all free, so the chunks kept together pass 1 MiB and the largest go back first; blocks made
  // afterwards show which chunks the heap kept (an address that reuses a kept chunk has its generation), and the
  // statistics show the bytes.
  std::vector<std::string> code;
  int variable = 0;
  std::vector<std::pair<int, int>>
      lastChunk; // first variable and count of the blocks of each class's last chunk
  const std::vector<uint64_t> sizes = {4000, 8000, 16000, 32000, 60000, 2000};
  for (const uint64_t size : sizes) {
    const std::vector<uint64_t> plan = textChunkPlan(textBlockSize(size));
    uint64_t total = 0;
    for (const uint64_t blocks : plan) {
      total += blocks;
    }
    const int first = variable;
    for (uint64_t i = 0; i < total; ++i) {
      allocText(code, variable++, size);
    }
    code.push_back("LoadLocal " + std::to_string(variable - 1));
    code.push_back("PrintU64 1");
    lastChunk.push_back(
        {static_cast<int>(first + total - plan.back()), static_cast<int>(plan.back())});
    printTextStats(code);
  }
  for (const auto &[first, count] : lastChunk) {
    for (int i = first; i < first + count; ++i) {
      freeText(code, i);
    }
    printTextStats(code);
  }
  for (const uint64_t size : sizes) {
    allocText(code, 0, size);
    code.push_back("LoadLocal 0");
    code.push_back("PrintU64 1");
    freeText(code, 0);
  }
  printTextStats(code);
  code.push_back("PushI32 0");
  code.push_back("ReturnI32");
  expectMatchesVm(withText(mainOf(code)), "text_free_chunks");
}

TEST_CASE("wasm string heap addresses of random alloc, free and query sequences match the VM") {
  if (!nodeAvailable()) {
    MESSAGE("SKIP: node is unavailable");
    return;
  }
  for (const uint64_t seed : {1ull, 2ull, 3ull, 4ull, 5ull, 6ull, 7ull, 8ull}) {
    expectMatchesVm(randomTextProgram(seed, 160), "text_random_" + std::to_string(seed));
  }
}

TEST_CASE("wasm string heap faults carry the VM's message and exit code 3") {
  if (!nodeAvailable()) {
    MESSAGE("SKIP: node is unavailable");
    return;
  }
  const std::vector<std::string> alloc100 = {"PushI64 100", callText(TextAlloc), "StoreLocal 0"};
  const auto withTail = [&](std::vector<std::string> tail) {
    std::vector<std::string> code = alloc100;
    code.insert(code.end(), tail.begin(), tail.end());
    code.push_back("ReturnVoid");
    return withText(mainOf(code));
  };
  const std::string load = "LoadLocal 0";
  const std::vector<std::pair<std::string, primec::IrModule>> faults = {
      {"free_twice", withTail({load, callText(TextFree), load, callText(TextFree)})},
      {"free_inside_a_block", withTail({load, "PushI64 8", "AddI64", callText(TextFree)})},
      {"free_a_slot_address", withTail({"PushI64 4096", callText(TextFree)})},
      {"free_a_byte_address_of_no_chunk",
       withTail({"PushI64 -9223231299358420480", callText(TextFree)})},
      {"free_a_byte_region", withTail({"PushI64 8", "HeapAllocBytes", callText(TextFree)})},
      {"free_a_stale_generation",
       withTail({load,
                 callText(TextFree),
                 "PushI64 8",
                 "HeapAllocBytes",
                 "Pop",
                 load,
                 callText(TextFree)})},
      {"capacity_inside_a_block",
       withTail({load, "PushI64 3", "AddI64", callText(TextCapacity), "Pop"})},
      {"capacity_of_a_slot_address", withTail({"PushI64 4096", callText(TextCapacity), "Pop"})},
      {"capacity_of_a_returned_chunk",
       withTail({load,
                 callText(TextFree),
                 "PushI64 20000",
                 callText(TextAlloc),
                 "Pop",
                 load,
                 callText(TextCapacity),
                 "Pop"})},
      {"length_inside_a_block",
       withTail({load, "PushI64 1", "AddI64", callText(TextLength), "Pop"})},
      {"usable_of_a_byte_region",
       withTail({"PushI64 8", "HeapAllocBytes", callText(TextUsable), "Pop"})},
  };
  for (const auto &[name, module] : faults) {
    CAPTURE(name);
    expectMatchesVm(module, "text_fault_" + name);
  }
}

TEST_CASE("wasm string heap large blocks have the VM's addresses, capacities and reuse") {
  if (!nodeAvailable()) {
    MESSAGE("SKIP: node is unavailable");
    return;
  }
  // Blocks above 64 KiB are byte regions of exactly their size: written at both ends, queried, freed in an order that
  // leaves free runs, then made again (the byte regions' best fit decides the addresses), next to small blocks.
  std::vector<std::string> code;
  const std::vector<uint64_t> sizes = {65537, 70000, 131072, 300000, 65538, 1000000, 100, 65537};
  int variable = 0;
  for (const uint64_t size : sizes) {
    allocText(code, variable, size);
    printTextBlock(code, variable);
    const std::string address = "LoadLocal " + std::to_string(variable);
    storeByte(code, address, 0, 72);
    storeByte(code, address, size - 2, 105);
    code.push_back(address);
    code.push_back(callText(TextLength));
    code.push_back("PrintI32 1");
    ++variable;
  }
  printTextStats(code);
  for (const int i : {1, 3, 5, 0}) {
    freeText(code, i);
    printTextStats(code);
  }
  for (const int i : {1, 3, 5, 0}) {
    allocText(code, i, sizes[static_cast<size_t>(i)] + 8 * static_cast<uint64_t>(i));
    printTextBlock(code, i);
  }
  printTextStats(code);
  for (int i = 0; i < variable; ++i) {
    freeText(code, i);
  }
  printTextStats(code);
  code.push_back("PushI32 0");
  code.push_back("ReturnI32");
  expectMatchesVm(withText(mainOf(code)), "text_large", "", true);
}

TEST_CASE("wasm string heap addresses of random sequences with large blocks match the VM") {
  if (!nodeAvailable()) {
    MESSAGE("SKIP: node is unavailable");
    return;
  }
  for (const uint64_t seed : {11ull, 12ull, 13ull, 14ull}) {
    expectMatchesVm(
        randomTextProgram(seed, 120, true), "text_random_large_" + std::to_string(seed), "", true);
  }
}

// `__pstext_set_limit(bytes)`.
void setTextLimit(std::vector<std::string> &code, uint64_t bytes) {
  code.push_back("PushI64 " + std::to_string(bytes));
  code.push_back(callText(TextSetLimit));
}

TEST_CASE("wasm string heap limit returns free chunks to make room and refuses the rest") {
  if (!nodeAvailable()) {
    MESSAGE("SKIP: node is unavailable");
    return;
  }
  // 64 blocks of the 4096 class take chunks of 1, 1, 2, 4, 8, 16 and 32 blocks (256 KiB); the last chunk is freed and
  // kept as the class's free chunk. With a limit of 300000 bytes a large block of 100000 bytes first returns that free
  // chunk, a larger one does not fit (`try_alloc` answers 0, the counters stay), the limit lifted lets it through, and a
  // limit below what is held refuses small blocks of a new class too.
  std::vector<std::string> code;
  for (int i = 0; i < 64; ++i) {
    allocText(code, i, 4000);
  }
  printTextStats(code);
  for (int i = 32; i < 64; ++i) {
    freeText(code, i);
  }
  printTextStats(code);
  setTextLimit(code, 300000);
  allocText(code, 70, 100000, TextTryAlloc);
  code.push_back("LoadLocal 70");
  code.push_back("PrintU64 1");
  printTextStats(code);
  allocText(code, 71, 100000, TextTryAlloc);
  code.push_back("LoadLocal 71");
  code.push_back("PrintU64 1");
  allocText(code, 72, 70000, TextTryAlloc);
  code.push_back("LoadLocal 72");
  code.push_back("PrintU64 1");
  allocText(code, 73, 50, TextTryAlloc);
  code.push_back("LoadLocal 73");
  code.push_back("PrintU64 1");
  printTextStats(code);
  freeText(code, 70);
  allocText(code, 73, 50, TextTryAlloc);
  code.push_back("LoadLocal 73");
  code.push_back("PrintU64 1");
  printTextStats(code);
  setTextLimit(code, 0);
  allocText(code, 74, 100000, TextTryAlloc);
  code.push_back("LoadLocal 74");
  code.push_back("PrintU64 1");
  printTextStats(code);
  setTextLimit(code, 1);
  allocText(code, 75, 20, TextTryAlloc);
  code.push_back("LoadLocal 75");
  code.push_back("PrintU64 1");
  printTextStats(code);
  code.push_back("PushI32 0");
  code.push_back("ReturnI32");
  expectMatchesVm(withText(mainOf(code)), "text_limit", "", true);
}

TEST_CASE("wasm string heap large block and limit faults carry the VM's message and exit code 3") {
  if (!nodeAvailable()) {
    MESSAGE("SKIP: node is unavailable");
    return;
  }
  const auto program = [&](std::vector<std::string> code) {
    code.push_back("ReturnVoid");
    return withText(mainOf(code));
  };
  const std::string load = "LoadLocal 0";
  const std::vector<std::pair<std::string, primec::IrModule>> faults = {
      {"limit_small_block",
       program({"PushI64 5000",
                callText(TextSetLimit),
                "PushI64 100",
                callText(TextAlloc),
                "Pop",
                "PushI64 5000",
                callText(TextAlloc),
                "Pop"})},
      {"limit_large_block",
       program({"PushI64 70000",
                callText(TextSetLimit),
                "PushI64 100000",
                callText(TextAlloc),
                "Pop"})},
      {"large_free_twice",
       program({"PushI64 100000",
                callText(TextAlloc),
                "StoreLocal 0",
                load,
                callText(TextFree),
                load,
                callText(TextFree)})},
      {"large_free_inside",
       program({"PushI64 100000",
                callText(TextAlloc),
                "StoreLocal 0",
                load,
                "PushI64 8",
                "AddI64",
                callText(TextFree)})},
      {"large_capacity_inside",
       program({"PushI64 100000",
                callText(TextAlloc),
                "StoreLocal 0",
                load,
                "PushI64 8",
                "AddI64",
                callText(TextCapacity),
                "Pop"})},
      {"large_free_stale_generation",
       program({"PushI64 100000",
                callText(TextAlloc),
                "StoreLocal 0",
                load,
                callText(TextFree),
                "PushI64 100000",
                callText(TextAlloc),
                "Pop",
                load,
                callText(TextFree)})},
      {"large_length_after_free",
       program({"PushI64 100000",
                callText(TextAlloc),
                "StoreLocal 0",
                load,
                callText(TextFree),
                load,
                callText(TextLength),
                "Pop"})},
  };
  for (const auto &[name, module] : faults) {
    CAPTURE(name);
    expectMatchesVm(module, "text_large_fault_" + name);
  }
}

TEST_CASE("wasm string arenas bump blocks, nest, free at once and match the VM's statistics") {
  if (!nodeAvailable()) {
    MESSAGE("SKIP: node is unavailable");
    return;
  }
  // Blocks of several size classes in an arena (chunks of 4096 bytes or four blocks, doubling), `free` of an arena
  // block (nothing happens), a nested arena with large blocks, the inner one ended (the outer one keeps filling its
  // chunk), the outer one ended, then ordinary blocks (the byte regions the arenas returned are reused); the
  // statistics after each step show the chunks, bytes in use, large blocks, depth and arena bytes.
  std::vector<std::string> code;
  const std::vector<uint64_t> sizes = {
      10, 100, 100, 3000, 3000, 3000, 70000, 40, 40, 20000, 20000, 65536, 100};
  code.push_back(callText(TextArenaBegin));
  int variable = 0;
  for (int round = 0; round < 3; ++round) {
    for (const uint64_t size : sizes) {
      allocText(code, variable % 40, size);
      printTextBlock(code, variable % 40);
      ++variable;
    }
    printTextStats(code);
    freeText(code, 3);
    freeText(code, 6);
    printTextStats(code);
    code.push_back(callText(TextArenaBegin));
  }
  for (int i = 0; i < 100; ++i) {
    allocText(code, 40 + i % 5, 100 + static_cast<uint64_t>(i) * 7);
    code.push_back("LoadLocal " + std::to_string(40 + i % 5));
    code.push_back("PrintU64 1");
  }
  printTextStats(code);
  for (int round = 0; round < 4; ++round) {
    code.push_back(callText(TextArenaEnd));
    printTextStats(code);
    allocText(code, 50, 200);
    code.push_back("LoadLocal 50");
    code.push_back("PrintU64 1");
  }
  for (const uint64_t size : {100ull, 5000ull, 70000ull, 100ull}) {
    allocText(code, 51, size);
    code.push_back("LoadLocal 51");
    code.push_back("PrintU64 1");
    freeText(code, 51);
  }
  printTextStats(code);
  code.push_back("PushI32 0");
  code.push_back("ReturnI32");
  expectMatchesVm(withText(mainOf(code)), "text_arenas", "", true);
}

TEST_CASE("wasm string heap addresses of random sequences with arenas match the VM") {
  if (!nodeAvailable()) {
    MESSAGE("SKIP: node is unavailable");
    return;
  }
  for (const uint64_t seed : {21ull, 22ull, 23ull, 24ull, 25ull, 26ull}) {
    expectMatchesVm(randomTextProgram(seed, 140, seed % 2 == 0, true),
                    "text_random_arena_" + std::to_string(seed),
                    "",
                    true);
  }
}

TEST_CASE("wasm string arenas count their chunks in the limit") {
  if (!nodeAvailable()) {
    MESSAGE("SKIP: node is unavailable");
    return;
  }
  // A limit of 20000 bytes: an arena takes chunks of 4096, 8192 (a class's second chunk doubles) and then no more room;
  // `try_alloc` answers 0 for the block that needs the third chunk, ending the arena makes room again.
  std::vector<std::string> code;
  setTextLimit(code, 20000);
  code.push_back(callText(TextArenaBegin));
  for (int i = 0; i < 20; ++i) {
    allocText(code, i, 1000, TextTryAlloc);
    code.push_back("LoadLocal " + std::to_string(i));
    code.push_back("PrintU64 1");
  }
  printTextStats(code);
  code.push_back(callText(TextArenaEnd));
  printTextStats(code);
  for (int i = 0; i < 20; ++i) {
    allocText(code, i, 1000, TextTryAlloc);
    code.push_back("LoadLocal " + std::to_string(i));
    code.push_back("PrintU64 1");
  }
  printTextStats(code);
  code.push_back("PushI32 0");
  code.push_back("ReturnI32");
  expectMatchesVm(withText(mainOf(code)), "text_arena_limit", "", true);
}

TEST_CASE("wasm string arena faults carry the VM's message and exit code 3") {
  if (!nodeAvailable()) {
    MESSAGE("SKIP: node is unavailable");
    return;
  }
  const auto program = [&](std::vector<std::string> code) {
    code.push_back("ReturnVoid");
    return withText(mainOf(code));
  };
  const std::string load = "LoadLocal 0";
  const std::vector<std::pair<std::string, primec::IrModule>> faults = {
      {"end_without_begin", program({callText(TextArenaEnd)})},
      {"end_twice",
       program({callText(TextArenaBegin), callText(TextArenaEnd), callText(TextArenaEnd)})},
      {"capacity_after_end",
       program({callText(TextArenaBegin),
                "PushI64 100",
                callText(TextAlloc),
                "StoreLocal 0",
                callText(TextArenaEnd),
                load,
                callText(TextCapacity),
                "Pop"})},
      {"capacity_of_a_large_block_after_end",
       program({callText(TextArenaBegin),
                "PushI64 100000",
                callText(TextAlloc),
                "StoreLocal 0",
                callText(TextArenaEnd),
                load,
                callText(TextCapacity),
                "Pop"})},
      {"limit_in_an_arena",
       program({"PushI64 3000",
                callText(TextSetLimit),
                callText(TextArenaBegin),
                "PushI64 100",
                callText(TextAlloc),
                "Pop"})},
  };
  for (const auto &[name, module] : faults) {
    CAPTURE(name);
    expectMatchesVm(module, "text_arena_fault_" + name);
  }
}

// The address `LoadLocal variable` plus `offset`.
void pushTextAddress(std::vector<std::string> &code, int variable, uint64_t offset) {
  code.push_back("LoadLocal " + std::to_string(variable));
  if (offset != 0) {
    code.push_back("PushI64 " + std::to_string(offset));
    code.push_back("AddI64");
  }
}

// `__pstext_move(to + toOffset, from + fromOffset, count)`.
void moveText(std::vector<std::string> &code,
              int to,
              uint64_t toOffset,
              int from,
              uint64_t fromOffset,
              uint64_t count) {
  pushTextAddress(code, to, toOffset);
  pushTextAddress(code, from, fromOffset);
  code.push_back("PushI64 " + std::to_string(count));
  code.push_back(callText(TextMove));
}

// `__pstext_fill(to + offset, value, count)`.
void fillText(
    std::vector<std::string> &code, int to, uint64_t offset, uint64_t value, uint64_t count) {
  pushTextAddress(code, to, offset);
  code.push_back("PushI64 " + std::to_string(value));
  code.push_back("PushI64 " + std::to_string(count));
  code.push_back(callText(TextFill));
}

// Prints the result of `__pstext_compare(a + aOffset, b + bOffset, count)`.
void compareText(std::vector<std::string> &code,
                 int a,
                 uint64_t aOffset,
                 int b,
                 uint64_t bOffset,
                 uint64_t count) {
  pushTextAddress(code, a, aOffset);
  pushTextAddress(code, b, bOffset);
  code.push_back("PushI64 " + std::to_string(count));
  code.push_back(callText(TextCompare));
  code.push_back("PrintI32 1");
}

// Prints the result of `__pstext_find(variable + offset, count, value)`.
void findText(
    std::vector<std::string> &code, int variable, uint64_t offset, uint64_t count, uint64_t value) {
  pushTextAddress(code, variable, offset);
  code.push_back("PushI64 " + std::to_string(count));
  code.push_back("PushI64 " + std::to_string(value));
  code.push_back(callText(TextFind));
  code.push_back("PrintI32 1");
}

TEST_CASE("wasm string byte operations move, fill, compare and find like the VM") {
  if (!nodeAvailable()) {
    MESSAGE("SKIP: node is unavailable");
    return;
  }
  // Two text blocks and a large one: a prefix written byte by byte, overlapping moves in both directions, fills,
  // compares of equal and differing ranges (the first difference decides, bytes compare unsigned) and finds of a
  // present, an absent and a 256 + value byte; every operation with a count of 0 on addresses that name nothing.
  std::vector<std::string> code;
  allocText(code, 0, 100);
  allocText(code, 1, 100);
  allocText(code, 2, 100000);
  for (uint64_t i = 0; i < 10; ++i) {
    storeByte(code, "LoadLocal 0", i, 65 + i);
    storeByte(code, "LoadLocal 1", i, 65 + i);
  }
  storeByte(code, "LoadLocal 1", 6, 200);
  storeByte(code, "LoadLocal 0", 20, 129);
  printTextBlock(code, 0);
  moveText(code, 0, 2, 0, 0, 8);
  printTextBlock(code, 0);
  moveText(code, 0, 0, 0, 3, 7);
  printTextBlock(code, 0);
  moveText(code, 2, 0, 0, 0, 40);
  moveText(code, 2, 99990, 0, 0, 10);
  moveText(code, 0, 50, 2, 99990, 10);
  printTextBlock(code, 2);
  printTextBlock(code, 0);
  fillText(code, 1, 10, 88, 5);
  fillText(code, 1, 0, 0x141, 3);
  printTextBlock(code, 1);
  compareText(code, 0, 0, 1, 0, 10);
  compareText(code, 0, 0, 0, 0, 10);
  compareText(code, 1, 3, 0, 3, 3);
  compareText(code, 0, 6, 1, 6, 1);
  compareText(code, 1, 6, 0, 6, 1);
  compareText(code, 0, 20, 1, 20, 4);
  compareText(code, 1, 20, 0, 20, 4);
  findText(code, 0, 0, 100, 67);
  findText(code, 0, 0, 100, 67 + 256);
  findText(code, 0, 0, 100, 250);
  findText(code, 0, 20, 80, 129);
  findText(code, 0, 5, 2, 70);
  findText(code, 2, 0, 100000, 65);
  findText(code, 2, 99990, 10, 74);
  // a count of 0 touches nothing, whatever the addresses are
  for (const char *address : {"PushI64 0", "PushI64 4096", "PushI64 -9223231299358420480"}) {
    code.push_back(address);
    code.push_back(address);
    code.push_back("PushI64 0");
    code.push_back(callText(TextMove));
    code.push_back(address);
    code.push_back("PushI64 7");
    code.push_back("PushI64 0");
    code.push_back(callText(TextFill));
    code.push_back(address);
    code.push_back(address);
    code.push_back("PushI64 0");
    code.push_back(callText(TextCompare));
    code.push_back("PrintI32 1");
    code.push_back(address);
    code.push_back("PushI64 0");
    code.push_back("PushI64 7");
    code.push_back(callText(TextFind));
    code.push_back("PrintI32 1");
  }
  printTextStats(code);
  code.push_back("PushI32 0");
  code.push_back("ReturnI32");
  expectMatchesVm(withText(mainOf(code)), "text_bytes", "", true);
}

TEST_CASE("wasm string byte operation faults carry the VM's message and exit code 3") {
  if (!nodeAvailable()) {
    MESSAGE("SKIP: node is unavailable");
    return;
  }
  // Two blocks of 100 bytes (variables 0 and 1, capacity 112) are made first.
  const auto program = [&](const std::vector<std::string> &code) {
    std::vector<std::string> all = {"PushI64 100",
                                    callText(TextAlloc),
                                    "StoreLocal 0",
                                    "PushI64 100",
                                    callText(TextAlloc),
                                    "StoreLocal 1"};
    all.insert(all.end(), code.begin(), code.end());
    all.push_back("ReturnVoid");
    return withText(mainOf(all));
  };
  std::vector<std::string> pastTheEnd;
  moveText(pastTheEnd, 0, 0, 1, 0, 113);
  std::vector<std::string> startsInside;
  moveText(startsInside, 0, 100, 1, 0, 20);
  std::vector<std::string> sourceBad;
  sourceBad.push_back("LoadLocal 0");
  sourceBad.push_back("PushI64 4096");
  sourceBad.push_back("PushI64 8");
  sourceBad.push_back(callText(TextMove));
  const std::vector<std::string> fillSlot = {
      "PushI64 4096", "PushI64 1", "PushI64 4", callText(TextFill)};
  std::vector<std::string> compareSecond;
  pushTextAddress(compareSecond, 0, 0);
  compareSecond.push_back("PushI64 8");
  compareSecond.push_back("PushI64 4");
  compareSecond.push_back(callText(TextCompare));
  compareSecond.push_back("Pop");
  std::vector<std::string> findFreed;
  findFreed.push_back("LoadLocal 1");
  findFreed.push_back(callText(TextFree));
  findText(findFreed, 1, 0, 5, 7);
  std::vector<std::string> fillGeneration;
  fillGeneration.push_back("LoadLocal 1");
  fillGeneration.push_back(callText(TextFree));
  fillGeneration.push_back("PushI64 100");
  fillGeneration.push_back(callText(TextAlloc));
  fillGeneration.push_back("Pop");
  fillText(fillGeneration, 1, 0, 3, 10);
  std::vector<std::string> hugeCount;
  fillText(hugeCount, 0, 0, 3, 5000000000ull);
  const std::vector<std::pair<std::string, primec::IrModule>> faults = {
      {"move_past_the_end", program(pastTheEnd)},
      {"move_starts_at_the_end", program(startsInside)},
      {"move_from_a_slot_address", program(sourceBad)},
      {"fill_a_slot_address", program(fillSlot)},
      {"compare_with_an_invalid_second", program(compareSecond)},
      {"find_a_freed_block", program(findFreed)},
      {"fill_a_stale_generation", program(fillGeneration)},
      {"fill_a_huge_count", program(hugeCount)},
  };
  for (const auto &[name, module] : faults) {
    CAPTURE(name);
    expectMatchesVm(module, "text_bytes_fault_" + name);
  }
}

TEST_CASE("wasm string literals and entry arguments match the VM") {
  if (!nodeAvailable()) {
    MESSAGE("SKIP: node is unavailable");
    return;
  }
  // Literals are copied into byte regions the first time they are asked for (the address is cached, an empty one is 0);
  // text blocks made after them show the generation they took; the bytes are read back and compared with a block.
  std::vector<std::string> code;
  for (const int index : {0, 2, 0, 1, 2}) {
    code.push_back("PushI64 " + std::to_string(index));
    code.push_back(callText(TextLiteral));
    code.push_back("PrintU64 1");
  }
  code.push_back("PushI64 0");
  code.push_back(callText(TextLiteral));
  code.push_back("StoreLocal 0");
  for (uint64_t i = 0; i < 6; ++i) {
    printByte(code, "LoadLocal 0", i);
  }
  allocText(code, 1, 40);
  code.push_back("LoadLocal 1");
  code.push_back("PrintU64 1");
  moveText(code, 1, 0, 0, 0, 6);
  printTextBlock(code, 1);
  compareText(code, 0, 0, 1, 0, 5);
  compareText(code, 0, 0, 1, 0, 6);
  findText(code, 0, 0, 5, 'l');
  code.push_back("PushI64 2");
  code.push_back(callText(TextLiteral));
  code.push_back("StoreLocal 2");
  compareText(code, 2, 0, 0, 0, 4);
  findText(code, 2, 0, 10, 'w');
  // arguments: index 0 is the program name, the others none
  for (const int index : {0, 0, 1, -1, 5}) {
    code.push_back("PushI32 " + std::to_string(index));
    code.push_back(callText(TextArgument));
    code.push_back("StoreLocal 3");
    code.push_back("LoadLocal 3");
    code.push_back("PrintU64 1");
    code.push_back("LoadLocal 3");
    code.push_back(callText(TextLength));
    code.push_back("PrintI32 1");
  }
  printTextStats(code);
  code.push_back("PushI32 0");
  code.push_back("ReturnI32");
  primec::IrModule module = withText(mainOf(code));
  module.stringTable = {"hello world", "", "world wide web"};
  module.hostImports[static_cast<size_t>(TextArgument)].parameters = {primec::IrHostValueKind::I32};
  expectMatchesVm(module, "text_literals", "", true);
}

TEST_CASE("the browser profile has a string heap and traps on a bad block address") {
  if (!nodeAvailable()) {
    MESSAGE("SKIP: node is unavailable");
    return;
  }
  const NodeRun value = runOnNode(withText(mainOf({"PushI64 100",
                                                   callText(TextAlloc),
                                                   "StoreLocal 0",
                                                   "LoadLocal 0",
                                                   "PushI64 65",
                                                   "StoreU8",
                                                   "Pop",
                                                   "LoadLocal 0",
                                                   callText(TextLength),
                                                   "LoadLocal 0",
                                                   callText(TextCapacity),
                                                   "AddI64",
                                                   "ReturnI64"})),
                                  "browser_text",
                                  true);
  REQUIRE_MESSAGE(value.built, value.buildError);
  // length 1 (one byte written), capacity 112
  CHECK(value.out == "113\n");
  const NodeRun trap = runOnNode(withText(mainOf({"PushI64 100",
                                                  callText(TextAlloc),
                                                  "PushI64 8",
                                                  "AddI64",
                                                  callText(TextFree),
                                                  "ReturnVoid"})),
                                 "browser_text_trap",
                                 true);
  REQUIRE_MESSAGE(trap.built, trap.buildError);
  CHECK(trap.out == "trap\n");
}

TEST_CASE("wasm reads the length of a table string by a computed index like the VM") {
  if (!nodeAvailable()) {
    MESSAGE("SKIP: node is unavailable");
    return;
  }
  const auto withStrings = [&](std::vector<std::string> code) {
    primec::IrModule module = mainOf(code);
    module.stringTable = {"abc", "", "hello world"};
    return module;
  };
  expectMatchesVm(withStrings({"PushI64 2", "LoadStringLength", "ReturnI64"}),
                  "string_length_last");
  expectMatchesVm(withStrings({"PushI64 1", "LoadStringLength", "ReturnI64"}),
                  "string_length_empty");
  expectMatchesVm(withStrings({"PushI64 0",
                               "LoadStringLength",
                               "PushI64 2",
                               "LoadStringLength",
                               "AddI64",
                               "ReturnI64"}),
                  "string_length_sum");
  // the VM's fault for an index past the table and for a negative one, which it reads as an unsigned value
  expectMatchesVm(withStrings({"PushI64 3", "LoadStringLength", "ReturnI64"}),
                  "string_length_past");
  expectMatchesVm(withStrings({"PushI64 -1", "LoadStringLength", "ReturnI64"}),
                  "string_length_negative");
  expectMatchesVm(mainOf({"PushI64 0", "LoadStringLength", "ReturnI64"}),
                  "string_length_empty_table");
}

TEST_SUITE_END();
