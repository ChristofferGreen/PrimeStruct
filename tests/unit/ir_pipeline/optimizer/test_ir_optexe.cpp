#include "primec/backend/IrToOptCppEmitter.h"
#include "primec/ir/IrOpcodeTable.h"
#include "primec/ir/IrOptimizer.h"
#include "primec/ir/IrPureSemantics.h"
#include "primec/support/ExternalTooling.h"
#include "primec/support/ProcessRunner.h"
#include "primec/testing/TestScratch.h"

#include "test_ir_optimizer_helpers.h"
#include "test_ir_random_programs.h"
#include "test_ir_runtime_programs.h"
#include "test_ir_vm_run.h"

#include <bit>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#if defined(__unix__) || defined(__APPLE__)
#include <sys/wait.h>
#endif

TEST_SUITE_BEGIN("primestruct.ir.optexe");

// The optexe emitter turns IR into structured C++ that the host compiler
// optimizes. Every test here checks the same contract: the compiled program
// behaves exactly like the VM running the same module (output, exit code, and
// fault message), at the host optimization levels the test names. Compiling
// costs about a second per program, so the tests batch many cases into one
// module (one function per case, called from a driver) wherever the cases
// cannot fault.

namespace {

using optimizer_test::Outcome;

struct ProgramRun {
  bool built = false;
  int exitCode = -1;
  std::string out;
  std::string err;
  std::string buildError;
};

std::string readText(const std::filesystem::path &path) {
  std::ifstream file(path, std::ios::binary);
  std::stringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}

std::filesystem::path scratch(const std::string &name) {
  const std::filesystem::path path = primec::testing::testScratchPath("optexe/" + name);
  std::filesystem::create_directories(path.parent_path());
  return path;
}

// Emits, compiles at host level `hostLevel` and runs a module.
ProgramRun compileAndRun(const primec::IrModule &module,
                         const std::string &name,
                         int hostLevel,
                         const std::vector<std::string> &args = {}) {
  ProgramRun run;
  std::string source;
  if (!primec::IrToOptCppEmitter().emitSource(module, source, run.buildError)) {
    return run;
  }
  const std::filesystem::path cppPath = scratch(name + ".cpp");
  const std::filesystem::path binary = scratch(name + ".bin");
  std::ofstream(cppPath) << source;
  if (!primec::compileCppExecutableOptimized(
          primec::systemProcessRunner(), cppPath, binary, hostLevel)) {
    run.buildError = "host compiler failed on " + cppPath.string();
    return run;
  }
  run.built = true;
  const std::filesystem::path outPath = scratch(name + ".stdout");
  const std::filesystem::path errPath = scratch(name + ".stderr");
  std::string command = "'" + binary.string() + "'";
  for (const std::string &arg : args) {
    command += " '" + arg + "'";
  }
  const int status = std::system(
      (command + " > '" + outPath.string() + "' 2> '" + errPath.string() + "'").c_str());
#if defined(__unix__) || defined(__APPLE__)
  run.exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#else
  run.exitCode = status;
#endif
  run.out = readText(outPath);
  run.err = readText(errPath);
  return run;
}

// Checks that the compiled program matches the VM, including the fault text.
void expectMatchesVm(const primec::IrModule &module,
                     const std::string &name,
                     int hostLevel,
                     const std::vector<std::string> &args = {}) {
  std::vector<std::string_view> vmArgs = {"program"};
  for (const std::string &arg : args) {
    vmArgs.push_back(arg);
  }
  const Outcome expected = optimizer_test::run(module, vmArgs);
  const ProgramRun actual = compileAndRun(module, name, hostLevel, args);
  INFO(name << " at host -O" << hostLevel);
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

std::string diagnosticFor(const primec::IrModule &module) {
  std::string source;
  std::string error;
  CHECK_FALSE(primec::IrToOptCppEmitter().emitSource(module, source, error));
  return error;
}

bool contains(const std::string &text, const std::string &needle) {
  return text.find(needle) != std::string::npos;
}

std::string emitted(const primec::IrModule &module) {
  std::string source;
  std::string error;
  REQUIRE_MESSAGE(primec::IrToOptCppEmitter().emitSource(module, source, error), error);
  return source;
}

// The sign and payload of a NaN produced by arithmetic is unspecified: x86 hardware
// yields the negative default NaN at run time while a host compiler folding the
// same expression yields the positive one. Results are compared as "a NaN".
std::string canonicalResult(std::string_view opName, const std::string &line) {
  const bool producesF32 =
      opName.ends_with("F32") &&
      (opName.starts_with("Add") || opName.starts_with("Sub") || opName.starts_with("Mul") ||
       opName.starts_with("Div") || opName.starts_with("Neg"));
  const bool producesF64 =
      opName.ends_with("F64") &&
      (opName.starts_with("Add") || opName.starts_with("Sub") || opName.starts_with("Mul") ||
       opName.starts_with("Div") || opName.starts_with("Neg"));
  const bool convertsToF32 = opName.starts_with("Convert") && opName.ends_with("ToF32");
  const bool convertsToF64 = opName.starts_with("Convert") && opName.ends_with("ToF64");
  const uint64_t value = std::stoull(line);
  if ((producesF32 || convertsToF32) && (value & 0x7FFFFFFFull) > 0x7F800000ull) {
    return "nan32";
  }
  if ((producesF64 || convertsToF64) && (value & 0x7FFFFFFFFFFFFFFFull) > 0x7FF0000000000000ull) {
    return "nan64";
  }
  return line;
}

uint64_t f32Bits(float value) {
  return static_cast<uint64_t>(std::bit_cast<uint32_t>(value));
}
uint64_t f64Bits(double value) {
  return std::bit_cast<uint64_t>(value);
}

} // namespace

TEST_CASE("optexe rejects modules it cannot compile with a precise diagnostic") {
  using primec::IrOpcode;
  SUBCASE("host calls") {
    const primec::IrModule module =
        optimizer_test::moduleOf(optimizer_test::assemble({"CallHost 0", "ReturnVoid"}));
    const std::string error = diagnosticFor(module);
    CHECK(contains(error, "CallHost"));
    CHECK(contains(error, "/main"));
  }
  SUBCASE("entry with parameters") {
    primec::IrModule module = optimizer_test::moduleOf(optimizer_test::assemble({"ReturnVoid"}), 1);
    CHECK(contains(diagnosticFor(module), "entry function"));
  }
  SUBCASE("invalid entry index") {
    primec::IrModule module = optimizer_test::moduleOf(optimizer_test::assemble({"ReturnVoid"}));
    module.entryIndex = 4;
    CHECK(contains(diagnosticFor(module), "entry"));
  }
  SUBCASE("unbalanced stack") {
    const primec::IrModule module =
        optimizer_test::moduleOf(optimizer_test::assemble({"AddI64", "ReturnVoid"}));
    CHECK(contains(diagnosticFor(module), "operand stack"));
  }
}

TEST_CASE("optexe maps stack slots and locals to variables, and only addressed frames to memory") {
  const primec::IrModule scalar = optimizer_test::moduleOf(optimizer_test::assemble({"PushI32 5",
                                                                                     "StoreLocal 0",
                                                                                     "LoadLocal 0",
                                                                                     "JumpIfZero 5",
                                                                                     "Jump 5",
                                                                                     "PushI32 0",
                                                                                     "ReturnI32"}));
  const std::string scalarSource = emitted(scalar);
  CHECK(contains(scalarSource, "uint64_t l0 = 0"));
  CHECK_FALSE(contains(scalarSource, "uint64_t frame["));
  CHECK(contains(scalarSource, "goto L"));

  const primec::IrModule addressed = optimizer_test::moduleOf(optimizer_test::assemble(
      {"PushI32 5", "StoreLocal 0", "AddressOfLocal 0", "LoadIndirect", "ReturnI32"}));
  const std::string addressedSource = emitted(addressed);
  CHECK(contains(addressedSource, "uint64_t frame[1]"));
  CHECK_FALSE(contains(addressedSource, "uint64_t l0"));

  // The VM resolves indirect accesses against the current frame, even when the
  // address came from the caller, so an indirect access alone needs a frame.
  const primec::IrModule indirectOnly = optimizer_test::moduleOf(
      optimizer_test::assemble({"PushI32 16", "LoadIndirect", "ReturnI32"}));
  CHECK(contains(emitted(indirectOnly), "uint64_t frame["));
}

// Runtime note: this case compiles ~6000 constant-operand cases and takes several
// seconds; the compile is the cost, and the constants are the point (the host
// compiler folds them, which is what the NaN canonicalization below is about).
TEST_CASE("optexe pure opcodes match the VM over edge-case operands") {
  using primec::IrOpcode;
  // Each opcode runs over the operand grid of its own kind: integers (including
  // the 32-bit boundaries and INT64_MIN), f32 or f64 bit patterns with signed
  // zeros, NaN, infinities and subnormals. Cases the VM faults on, or whose
  // result is host-defined (float to integer conversions out of range), are
  // excluded as in the optimizer's constant folder.
  const std::vector<uint64_t> intGrid = {
      0,
      1,
      2,
      static_cast<uint64_t>(-1),
      static_cast<uint64_t>(-2),
      7,
      0x7FFFFFFFull,
      0x80000000ull,
      0x100000000ull,
      0x8000000000000000ull,
      0x7FFFFFFFFFFFFFFFull,
      0xDEADBEEFCAFEF00Dull,
  };
  const std::vector<uint64_t> f64Grid = {
      f64Bits(0.0),
      f64Bits(-0.0),
      f64Bits(1.5),
      f64Bits(-2.25),
      f64Bits(1e300),
      f64Bits(std::numeric_limits<double>::quiet_NaN()),
      f64Bits(std::numeric_limits<double>::infinity()),
      f64Bits(std::numeric_limits<double>::denorm_min()),
  };
  const std::vector<uint64_t> f32Grid = {
      f32Bits(0.0f),
      f32Bits(-0.0f),
      f32Bits(1.5f),
      f32Bits(-2.25f),
      f32Bits(3e38f),
      f32Bits(std::numeric_limits<float>::quiet_NaN()),
      f32Bits(std::numeric_limits<float>::infinity()),
      f32Bits(std::numeric_limits<float>::denorm_min()),
  };
  const auto gridFor = [&](std::string_view name) -> const std::vector<uint64_t> & {
    const bool fromInteger = name.starts_with("ConvertI") || name.starts_with("ConvertU");
    if (fromInteger) {
      return intGrid;
    }
    if (name.find("F32") != std::string_view::npos) {
      return f32Grid;
    }
    if (name.find("F64") != std::string_view::npos) {
      return f64Grid;
    }
    return intGrid;
  };
  std::vector<primec::IrFunction> bodies;
  std::vector<std::string> descriptions; // one per printed line, for readable failures
  std::vector<std::string_view> opNames; // aligned with descriptions
  size_t cases = 0;
  for (const primec::IrOpcodeInfo &info : primec::IrOpcodeTable) {
    if (!primec::isIrPureOpcode(info.op)) {
      continue;
    }
    const bool binary = primec::irPureOpcodeArity(info.op) == 2;
    std::vector<primec::IrInstruction> code;
    const std::vector<uint64_t> &grid = gridFor(info.name);
    for (const uint64_t lhs : grid) {
      for (const uint64_t rhs : grid) {
        if (!binary && rhs != grid.front()) {
          continue;
        }
        if (!primec::irPureEvalIsPortable(info.op, lhs, rhs)) {
          continue;
        }
        code.push_back({IrOpcode::PushI64, lhs});
        if (binary) {
          code.push_back({IrOpcode::PushI64, rhs});
        }
        code.push_back({info.op, 0});
        code.push_back({IrOpcode::PrintU64, primec::PrintFlagNewline});
        std::ostringstream description;
        description << info.name << " 0x" << std::hex << lhs;
        if (binary) {
          description << " 0x" << rhs;
        }
        descriptions.push_back(description.str());
        opNames.push_back(info.name);
        ++cases;
      }
    }
    code.push_back({IrOpcode::ReturnVoid, 0});
    bodies.push_back(optimizer_test::functionOf(std::string("/") + info.name, std::move(code)));
  }
  CHECK(cases > 3000);
  const primec::IrModule module = optimizer_test::driverModule(std::move(bodies));
  const Outcome expected = optimizer_test::run(module);
  REQUIRE(expected.ok);
  const ProgramRun actual = compileAndRun(module, "pure_ops", 1);
  REQUIRE_MESSAGE(actual.built, actual.buildError);
  CHECK(actual.exitCode == 0);
  std::istringstream expectedLines(expected.output);
  std::istringstream actualLines(actual.out);
  std::string expectedLine;
  std::string actualLine;
  size_t mismatches = 0;
  for (size_t i = 0; i < descriptions.size(); ++i) {
    REQUIRE(std::getline(expectedLines, expectedLine));
    REQUIRE(std::getline(actualLines, actualLine));
    if (canonicalResult(opNames[i], expectedLine) != canonicalResult(opNames[i], actualLine) &&
        mismatches++ < 8) {
      FAIL_CHECK(descriptions[i] << ": VM " << expectedLine << ", optexe " << actualLine);
    }
  }
  CHECK(mismatches == 0);
}

TEST_CASE("optexe matches the VM on random programs, optimized or not") {
  constexpr uint64_t Programs = 60;
  std::vector<primec::IrFunction> bodies;
  for (uint64_t seed = 0; seed < Programs; ++seed) {
    bodies.push_back(optimizer_test::makeModule(seed + 31000).functions[0]);
    bodies.back().name = "/program" + std::to_string(seed);
  }
  const primec::IrModule module = optimizer_test::driverModule(std::move(bodies));
  expectMatchesVm(module, "random_plain", 1);

  primec::IrModule optimized = module;
  primec::OptimizationOptions options;
  options.level = 2;
  options.verifyEachPass = true;
  primec::IrOptimizationReport report;
  std::string error;
  REQUIRE_MESSAGE(
      primec::optimizeIrModule(optimized, options, primec::IrValidationTarget::Any, report, error),
      error);
  CHECK(report.instructionsAfter < report.instructionsBefore);
  expectMatchesVm(optimized, "random_optimized", 2);
}

TEST_CASE("optexe calls pass arguments as the callee's initial stack and return values") {
  expectMatchesVm(optimizer_test::callsProgram(), "calls", 2);
}

TEST_CASE("optexe heap, indirect addressing and frame addresses match the VM") {
  expectMatchesVm(optimizer_test::heapProgram(), "heap", 1);
}

TEST_CASE("optexe string, argv and file opcodes match the VM") {
  const std::filesystem::path file = scratch("io_roundtrip.txt");
  expectMatchesVm(optimizer_test::ioProgram(file.string()), "io", 1, {"first", "se cond"});
  std::error_code ignored;
  std::filesystem::remove(file, ignored);
}

TEST_CASE("optexe faults carry the VM's message and exit code 3") {
  for (const optimizer_test::FaultProgram &fault : optimizer_test::faultPrograms()) {
    CAPTURE(fault.name);
    expectMatchesVm(fault.module, "fault_" + fault.name, 0);
  }
}
