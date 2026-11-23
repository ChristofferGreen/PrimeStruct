#include "third_party/doctest.h"

#include "primec/backend/WasmEmitter.h"
#include "primec/testing/TestScratch.h"

#include "test_ir_optimizer_helpers.h"
#include "test_ir_runtime_programs.h"
#include "test_ir_vm_run.h"

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
const wasi = new WASI({ version: "preview1", args: [process.argv[2]], returnOnExit: true });
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
                     const std::string &nodeFlags = "") {
  const Outcome expected = optimizer_test::run(module, {"program"});
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

TEST_SUITE_END();
