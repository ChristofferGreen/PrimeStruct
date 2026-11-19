#include "test_ir_pipeline_wasm_helpers.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <random>

using namespace ir_pipeline_wasm_test;

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

// `FmaF32` and `FmaF64` compute a * b + c with one rounding on Wasm, through helper functions of the module
// (docs/spec/vm-design.md, Fused multiply-add). These cases run them under Node's built-in WebAssembly on a grid of
// special values and on seeded random operands, and compare every result bit for bit with std::fma (any NaN equals
// any NaN). A machine without Node skips them with a message.

namespace {

// Reads `module data` lines of hex bit patterns `a b c expected`, calls the module's exported `fma` and reports the
// first mismatches. The exit code is 0 only when every line matches.
constexpr const char *FmaNodeRunner = R"(
"use strict";
const fs = require("node:fs");
const isF64 = process.argv[4] === "f64";
const instance = new WebAssembly.Instance(new WebAssembly.Module(fs.readFileSync(process.argv[2])), {});
const fma = instance.exports.fma;
const view = new DataView(new ArrayBuffer(8));
const toNumber = (hex) => {
  if (isF64) { view.setBigUint64(0, BigInt("0x" + hex)); return view.getFloat64(0); }
  view.setUint32(0, parseInt(hex, 16)); return view.getFloat32(0);
};
const toBits = (value) => {
  if (isF64) { view.setFloat64(0, value); return view.getBigUint64(0); }
  view.setFloat32(0, value); return BigInt(view.getUint32(0));
};
const isNan = (bits) => isF64 ? ((bits >> 52n) & 0x7ffn) === 0x7ffn && (bits & 0xfffffffffffffn) !== 0n
                              : ((bits >> 23n) & 0xffn) === 0xffn && (bits & 0x7fffffn) !== 0n;
let bad = 0;
let count = 0;
for (const line of fs.readFileSync(process.argv[3], "utf8").split("\n")) {
  if (line === "") continue;
  const [a, b, c, expected] = line.split(" ");
  const want = BigInt("0x" + expected);
  const got = toBits(fma(toNumber(a), toNumber(b), toNumber(c)));
  ++count;
  if (got !== want && !(isNan(got) && isNan(want))) {
    if (bad++ < 5) console.error("fma(" + a + ", " + b + ", " + c + ") = " + got.toString(16) + ", want " + expected);
  }
}
console.error(count + " cases, " + bad + " wrong");
process.exitCode = bad === 0 && count > 0 ? 0 : 1;
)";

// A module that exports `fma(a, b, c)`: function 0 calls it with float arguments so its parameters are typed as floats
// (the export is the entry).
std::vector<uint8_t> buildFmaModule(bool isF64) {
  const primec::IrOpcode push = isF64 ? primec::IrOpcode::PushF64 : primec::IrOpcode::PushF32;
  const primec::IrOpcode fused = isF64 ? primec::IrOpcode::FmaF64 : primec::IrOpcode::FmaF32;
  const primec::IrOpcode ret = isF64 ? primec::IrOpcode::ReturnF64 : primec::IrOpcode::ReturnF32;
  primec::IrModule module;
  module.entryIndex = 1;

  primec::IrFunction caller;
  caller.name = "/caller";
  for (int i = 0; i < 3; ++i) {
    caller.instructions.push_back({push, 0});
  }
  caller.instructions.push_back({primec::IrOpcode::Call, 1});
  caller.instructions.push_back({ret, 0});
  module.functions.push_back(std::move(caller));

  primec::IrFunction fma;
  fma.name = "/fma";
  fma.parameterCount = 3;
  fma.instructions.push_back({primec::IrOpcode::StoreLocal, 2});
  fma.instructions.push_back({primec::IrOpcode::StoreLocal, 1});
  fma.instructions.push_back({primec::IrOpcode::StoreLocal, 0});
  fma.instructions.push_back({primec::IrOpcode::LoadLocal, 0});
  fma.instructions.push_back({primec::IrOpcode::LoadLocal, 1});
  fma.instructions.push_back({primec::IrOpcode::LoadLocal, 2});
  fma.instructions.push_back({fused, 0});
  fma.instructions.push_back({ret, 0});
  module.functions.push_back(std::move(fma));

  primec::WasmEmitter emitter;
  std::vector<uint8_t> bytes;
  std::string error;
  REQUIRE(emitter.emitModule(module, bytes, error));
  CHECK(error.empty());
  return bytes;
}

template <typename Float, typename Bits> std::string hexLine(Float a, Float b, Float c) {
  const Float result = std::fma(a, b, c);
  const auto bitsOf = [](Float value) {
    Bits bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
  };
  char buffer[128];
  const char *format =
      sizeof(Bits) == 8 ? "%016llx %016llx %016llx %016llx\n" : "%08llx %08llx %08llx %08llx\n";
  std::snprintf(buffer,
                sizeof(buffer),
                format,
                static_cast<unsigned long long>(bitsOf(a)),
                static_cast<unsigned long long>(bitsOf(b)),
                static_cast<unsigned long long>(bitsOf(c)),
                static_cast<unsigned long long>(bitsOf(result)));
  return buffer;
}

template <typename Float, typename Bits> Float fromBits(Bits bits) {
  Float value = 0;
  std::memcpy(&value, &bits, sizeof(value));
  return value;
}

// The operands of the grid: signed zeros, ones, fractions, powers of two near the significand width, the extremes of
// the range, subnormals, infinities and a NaN.
template <typename Float> std::vector<Float> gridValues() {
  using Limits = std::numeric_limits<Float>;
  const Float eps = Limits::epsilon();
  std::vector<Float> values = {Float(0),
                               -Float(0),
                               Float(1),
                               Float(-1),
                               Float(2),
                               Float(-2),
                               Float(0.5),
                               Float(3),
                               Float(0.1),
                               Float(-0.1),
                               Float(1) / Float(3),
                               Float(1e10),
                               Float(1e-10),
                               Limits::max(),
                               -Limits::max(),
                               Limits::min(),
                               Limits::min() / Float(2),
                               Limits::denorm_min(),
                               -Limits::denorm_min(),
                               Float(1) + eps,
                               Float(1) - eps / Float(2),
                               Float(1) + eps / Float(2) * Float(3),
                               std::ldexp(Float(1), Limits::digits),
                               std::ldexp(Float(1), Limits::digits) + Float(2),
                               std::ldexp(Float(1), Limits::max_exponent - 2),
                               std::ldexp(Float(1), Limits::min_exponent + 1),
                               Limits::infinity(),
                               -Limits::infinity(),
                               Limits::quiet_NaN()};
  return values;
}

template <typename Float, typename Bits> std::string fmaData(uint64_t seed, int randomCases) {
  std::string data;
  const std::vector<Float> grid = gridValues<Float>();
  for (const Float a : grid) {
    for (const Float b : grid) {
      for (const Float c : grid) {
        data += hexLine<Float, Bits>(a, b, c);
      }
    }
  }
  std::mt19937_64 random(seed);
  const Bits signMask = Bits(1) << (sizeof(Bits) * 8 - 1);
  const int fractionBits = std::numeric_limits<Float>::digits - 1;
  const int exponentBits = static_cast<int>(sizeof(Bits) * 8) - 1 - fractionBits;
  const Bits exponentMax = (Bits(1) << exponentBits) - 1;
  const Bits fractionMask = (Bits(1) << fractionBits) - 1;
  const auto randomFloat = [&](int exponentLow, int exponentHigh) {
    const Bits exponent =
        static_cast<Bits>(exponentLow + random() % (exponentHigh - exponentLow + 1));
    const Bits bits = (static_cast<Bits>(random()) & signMask) | (exponent << fractionBits) |
                      (static_cast<Bits>(random()) & fractionMask);
    return fromBits<Float>(bits);
  };
  const int bias = std::numeric_limits<Float>::max_exponent - 1;
  for (int i = 0; i < randomCases; ++i) {
    Float a;
    Float b;
    Float c;
    switch (i % 4) {
    case 0: // anywhere in the range, finite or not
      a = randomFloat(0, static_cast<int>(exponentMax));
      b = randomFloat(0, static_cast<int>(exponentMax));
      c = randomFloat(0, static_cast<int>(exponentMax));
      break;
    case 1: // an addend that nearly cancels the product
      a = randomFloat(bias - 20, bias + 20);
      b = randomFloat(bias - 20, bias + 20);
      c = -(a * b);
      for (uint64_t steps = random() % 3; steps > 0; --steps) {
        c = std::nextafter(c, (random() & 1) != 0 ? Float(1e30) : Float(-1e30));
      }
      break;
    case 2: // a product and an addend near the subnormal range
      a = randomFloat(0, bias / 2);
      b = randomFloat(0, bias / 2);
      c = randomFloat(0, 3 * bias / 4);
      break;
    default: // a product near overflow
      a = randomFloat(bias + bias / 2 - 4, bias + bias / 2 + 4);
      b = randomFloat(bias / 2 - 4, bias / 2 + 8);
      c = randomFloat(bias + bias / 2 - 4, bias + bias / 2 + 4);
      break;
    }
    data += hexLine<Float, Bits>(a, b, c);
  }
  return data;
}

void checkFmaAgainstStd(bool isF64) {
  if (hasNode()) {
    const std::string name = isF64 ? "f64" : "f32";
    const std::vector<uint8_t> bytes = buildFmaModule(isF64);
    const std::filesystem::path root = irPipelineWasmPath("fma_" + name);
    std::error_code ec;
    std::filesystem::create_directories(root, ec);
    REQUIRE(!ec);
    const std::string wasmPath = (root / "fma.wasm").string();
    {
      std::ofstream wasmFile(wasmPath, std::ios::binary);
      wasmFile.write(reinterpret_cast<const char *>(bytes.data()),
                     static_cast<std::streamsize>(bytes.size()));
      REQUIRE(wasmFile.good());
    }
    const std::string dataPath = (root / "cases.txt").string();
    {
      std::ofstream dataFile(dataPath, std::ios::binary);
      dataFile << (isF64 ? fmaData<double, uint64_t>(12345, 24000)
                         : fmaData<float, uint32_t>(54321, 24000));
      REQUIRE(dataFile.good());
    }
    const std::string runnerPath = (root / "runner.cjs").string();
    {
      std::ofstream runnerFile(runnerPath, std::ios::binary);
      runnerFile << FmaNodeRunner;
      REQUIRE(runnerFile.good());
    }
    const std::string errPath = (root / "stderr.txt").string();
    const int exitCode =
        runCommand("node " + quoteShellArg(runnerPath) + " " + quoteShellArg(wasmPath) + " " +
                   quoteShellArg(dataPath) + " " + name + " 2> " + quoteShellArg(errPath));
    CHECK_MESSAGE(exitCode == 0, readFileText(errPath));
  } else {
    MESSAGE("SKIP: node is unavailable; the Wasm fma helpers were not run");
  }
}

} // namespace

TEST_CASE("wasm fma of f64 equals std::fma bit for bit") {
  checkFmaAgainstStd(true);
}

TEST_CASE("wasm fma of f32 equals std::fma bit for bit") {
  checkFmaAgainstStd(false);
}

TEST_SUITE_END();
