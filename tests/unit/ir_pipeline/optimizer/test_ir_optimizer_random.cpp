#include "primec/ir/IrModulePrinter.h"
#include "primec/ir/IrOptimizer.h"
#include "primec/runtime/Vm.h"
#include "primec/testing/TestScratch.h"

#include "test_ir_optimizer_helpers.h"

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#if defined(__unix__) || defined(__APPLE__)
#include <fcntl.h>
#include <unistd.h>
#endif

TEST_SUITE_BEGIN("primestruct.ir.optimizer_random");

// Seeded programs of valid, balanced stack IR (assignments, the dup/store/pop
// assignment idiom, discarded expressions, if/else, counted loops, integer and
// float expressions) are run through the VM before and after optimization; any
// difference in output, exit code or fault is a bug in a pass. The generator
// avoids operations that fault (division uses a nonzero constant) so every
// program terminates normally.

namespace {

using primec::IrInstruction;
using primec::IrOpcode;

class Generator {
public:
  explicit Generator(uint64_t seed) : rng_(seed) {}

  std::vector<IrInstruction> program() {
    // Locals 0..5 are scratch values, 6.. are loop counters (one per depth).
    for (uint64_t slot = 0; slot < ScratchLocals; ++slot) {
      emit(IrOpcode::PushI64, pick(5) == 0 ? rng_() : static_cast<uint64_t>(pick(7)) - 3);
      emit(IrOpcode::StoreLocal, slot);
    }
    block(0, 3 + pick(4));
    for (uint64_t slot = 0; slot < ScratchLocals; ++slot) {
      emit(IrOpcode::LoadLocal, slot);
      emit(IrOpcode::PrintI64, primec::PrintFlagNewline);
    }
    emit(IrOpcode::PushI32, 0);
    emit(IrOpcode::ReturnI32);
    return std::move(code_);
  }

  size_t loops = 0;
  size_t branches = 0;

private:
  static constexpr uint64_t ScratchLocals = 6;
  static constexpr uint64_t MaxDepth = 3;

  uint64_t pick(uint64_t bound) { return rng_() % bound; }

  size_t emit(IrOpcode op, uint64_t imm = 0) {
    code_.push_back({op, imm, 0});
    return code_.size() - 1;
  }

  void constant() {
    switch (pick(6)) {
      case 0:
        emit(IrOpcode::PushI32, static_cast<uint64_t>(static_cast<int64_t>(static_cast<int>(pick(9)) - 4)));
        break;
      case 1:
        emit(IrOpcode::PushI64, 0);
        break;
      case 2:
        emit(IrOpcode::PushI64, 1);
        break;
      case 3:
        emit(IrOpcode::PushI64, rng_());
        break;
      default:
        emit(IrOpcode::PushI32, pick(100));
        break;
    }
  }

  // Pushes exactly one 64-bit integer value.
  void intExpr(int depth) {
    const uint64_t choice = depth >= 3 ? pick(2) : pick(12);
    switch (choice) {
      case 0:
        constant();
        return;
      case 1:
        emit(IrOpcode::LoadLocal, pick(ScratchLocals));
        return;
      case 2:
      case 3:
      case 4: {
        intExpr(depth + 1);
        intExpr(depth + 1);
        static const IrOpcode ops[] = {IrOpcode::AddI64, IrOpcode::SubI64, IrOpcode::MulI64};
        emit(ops[choice - 2]);
        return;
      }
      case 5: {
        intExpr(depth + 1);
        intExpr(depth + 1);
        static const IrOpcode cmps[] = {IrOpcode::CmpLtI64, IrOpcode::CmpGtI64, IrOpcode::CmpEqI64, IrOpcode::CmpNeI64,
                                        IrOpcode::CmpLeI64, IrOpcode::CmpGeU64, IrOpcode::CmpLtU64};
        emit(cmps[pick(7)]);
        return;
      }
      case 6:
        intExpr(depth + 1);
        emit(pick(2) == 0 ? IrOpcode::NegI64 : IrOpcode::NegI32);
        return;
      case 7: {
        intExpr(depth + 1);
        emit(IrOpcode::PushI64, 1 + pick(7));  // nonzero divisor
        emit(pick(2) == 0 ? IrOpcode::DivI64 : IrOpcode::DivU64);
        return;
      }
      case 8:
        // x + 0, x * 1, x / 1 and x - 0 shapes the peephole pass rewrites.
        intExpr(depth + 1);
        emit(IrOpcode::PushI64, pick(2) == 0 ? 0 : 1);
        emit(pick(2) == 0 ? IrOpcode::AddI64 : IrOpcode::MulI64);
        return;
      case 9: {
        // A float round trip: int -> f64 -> arithmetic -> int.
        intExpr(depth + 1);
        emit(IrOpcode::ConvertI64ToF64);
        emit(IrOpcode::PushF64, std::bit_cast<uint64_t>(1.5 + static_cast<double>(pick(8))));
        emit(pick(2) == 0 ? IrOpcode::AddF64 : IrOpcode::MulF64);
        emit(IrOpcode::PushF64, std::bit_cast<uint64_t>(1000000.0));
        emit(IrOpcode::CmpLtF64);
        return;
      }
      case 10:
        // A constant sub-expression for the folder.
        emit(IrOpcode::PushI64, pick(50));
        emit(IrOpcode::PushI64, pick(50));
        emit(pick(2) == 0 ? IrOpcode::AddI64 : IrOpcode::MulI64);
        return;
      default:
        intExpr(depth + 1);
        emit(IrOpcode::Dup);
        emit(IrOpcode::AddI64);
        return;
    }
  }

  void statement(uint64_t depth) {
    const uint64_t kind = depth >= MaxDepth ? pick(5) : pick(8);
    switch (kind) {
      case 0:
        intExpr(0);
        emit(IrOpcode::StoreLocal, pick(ScratchLocals));
        return;
      case 1:  // assignment statement idiom
        intExpr(0);
        emit(IrOpcode::Dup);
        emit(IrOpcode::StoreLocal, pick(ScratchLocals));
        emit(IrOpcode::Pop);
        return;
      case 2:  // evaluated and discarded
        intExpr(0);
        emit(IrOpcode::Pop);
        return;
      case 3:
        intExpr(0);
        emit(IrOpcode::PrintI64, primec::PrintFlagNewline);
        return;
      case 4:  // store then use
        intExpr(0);
        emit(IrOpcode::StoreLocal, pick(ScratchLocals));
        emit(IrOpcode::LoadLocal, pick(ScratchLocals));
        emit(IrOpcode::Pop);
        return;
      case 5:
      case 6: {  // if / else
        ++branches;
        intExpr(0);
        const size_t toElse = emit(IrOpcode::JumpIfZero);
        block(depth + 1, 1 + pick(3));
        const size_t toEnd = emit(IrOpcode::Jump);
        code_[toElse].imm = code_.size();
        if (pick(3) != 0) {
          block(depth + 1, 1 + pick(3));
        }
        code_[toEnd].imm = code_.size();
        return;
      }
      default: {  // counted loop with its own counter
        ++loops;
        const uint64_t counter = ScratchLocals + depth;
        emit(IrOpcode::PushI32, 1 + pick(4));
        emit(IrOpcode::StoreLocal, counter);
        const size_t head = code_.size();
        emit(IrOpcode::LoadLocal, counter);
        const size_t exit = emit(IrOpcode::JumpIfZero);
        block(depth + 1, 1 + pick(3));
        emit(IrOpcode::LoadLocal, counter);
        emit(IrOpcode::PushI32, 1);
        emit(IrOpcode::SubI32);
        emit(IrOpcode::StoreLocal, counter);
        emit(IrOpcode::Jump, head);
        code_[exit].imm = code_.size();
        return;
      }
    }
  }

  void block(uint64_t depth, uint64_t count) {
    for (uint64_t i = 0; i < count; ++i) {
      statement(depth);
    }
  }

  std::mt19937_64 rng_;
  std::vector<IrInstruction> code_;
};

primec::IrModule makeModule(uint64_t seed, size_t *loops = nullptr, size_t *branches = nullptr) {
  Generator generator(seed);
  primec::IrModule module = optimizer_test::moduleOf(generator.program());
  module.functions[0].metadata.effectMask = primec::EffectIoOut;
  module.functions[0].metadata.capabilityMask = primec::EffectIoOut;
  if (loops != nullptr) {
    *loops = generator.loops;
  }
  if (branches != nullptr) {
    *branches = generator.branches;
  }
  return module;
}

struct Outcome {
  bool ok = false;
  uint64_t result = 0;
  std::string error;
  std::string output;

  bool operator==(const Outcome &other) const {
    return ok == other.ok && result == other.result && error == other.error && output == other.output;
  }
};

#if defined(__unix__) || defined(__APPLE__)
// Captures what the VM writes to stdout while alive.
class StdoutCapture {
public:
  explicit StdoutCapture(const std::string &path) : path_(path) {
    std::fflush(stdout);
    saved_ = ::dup(1);
    const int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) {
      ::dup2(fd, 1);
      ::close(fd);
    }
  }
  std::string finish() {
    std::fflush(stdout);
    if (saved_ >= 0) {
      ::dup2(saved_, 1);
      ::close(saved_);
      saved_ = -1;
    }
    std::ifstream in(path_);
    std::stringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
  }
  ~StdoutCapture() {
    if (saved_ >= 0) {
      finish();
    }
  }

private:
  std::string path_;
  int saved_ = -1;
};
#endif

Outcome run(const primec::IrModule &module) {
  static const std::string capturePath =
      primec::testing::testScratchPath("optimizer_random/stdout.txt").string();
  std::filesystem::create_directories(std::filesystem::path(capturePath).parent_path());
  Outcome outcome;
  StdoutCapture capture(capturePath);
  primec::Vm vm;
  outcome.ok = vm.execute(module, outcome.result, outcome.error);
  outcome.output = capture.finish();
  return outcome;
}

// Optimizes a copy with `options` and returns whether it matched the original.
bool optimizedMatches(const primec::IrModule &original,
                      const primec::OptimizationOptions &options,
                      const Outcome &baseline,
                      std::string &detail) {
  primec::IrModule optimized = original;
  primec::IrOptimizationReport report;
  std::string error;
  if (!primec::optimizeIrModule(optimized, options, primec::IrValidationTarget::Vm, report, error)) {
    detail = "optimizer failed: " + error;
    return false;
  }
  const Outcome after = run(optimized);
  if (!(after == baseline)) {
    detail = "behavior differs; original:\n" + primec::formatIrFunction(original, 0) + "optimized:\n" +
             primec::formatIrFunction(optimized, 0);
    return false;
  }
  return true;
}

} // namespace

TEST_CASE("generated programs are valid and exercise loops and branches") {
  size_t withLoop = 0;
  size_t withBranch = 0;
  for (uint64_t seed = 0; seed < 200; ++seed) {
    size_t loops = 0;
    size_t branches = 0;
    const primec::IrModule module = makeModule(seed, &loops, &branches);
    std::string error;
    REQUIRE_MESSAGE(primec::verifyIrModuleForOptimization(module, primec::IrValidationTarget::Vm, error),
                    "seed ", seed, ": ", error);
    withLoop += loops > 0 ? 1 : 0;
    withBranch += branches > 0 ? 1 : 0;
    const Outcome outcome = run(module);
    CHECK_MESSAGE(outcome.ok, "seed ", seed, " faulted: ", outcome.error);
  }
  CHECK(withLoop > 40);
  CHECK(withBranch > 40);
}

TEST_CASE("optimizing random programs at O1 preserves their behavior") {
  primec::OptimizationOptions options;
  options.level = 1;
  options.verifyEachPass = true;
  size_t shrunk = 0;
  for (uint64_t seed = 1000; seed < 2200; ++seed) {
    const primec::IrModule original = makeModule(seed);
    const Outcome baseline = run(original);
    REQUIRE_MESSAGE(baseline.ok, "seed ", seed, ": ", baseline.error);
    std::string detail;
    CHECK_MESSAGE(optimizedMatches(original, options, baseline, detail), "seed ", seed, ": ", detail);

    primec::IrModule optimized = original;
    primec::IrOptimizationReport report;
    std::string error;
    REQUIRE(primec::optimizeIrModule(optimized, options, primec::IrValidationTarget::Vm, report, error));
    shrunk += report.instructionsAfter < report.instructionsBefore ? 1 : 0;
  }
  // The generator emits patterns the passes target; if almost nothing shrinks
  // the test no longer exercises them.
  CHECK(shrunk > 600);
}

TEST_CASE("each pass alone preserves the behavior of random programs") {
  for (const primec::IrOptimizationPass &pass : primec::irOptimizationPasses()) {
    CAPTURE(pass.info.name);
    primec::OptimizationOptions options;
    options.enabledPasses = {std::string(pass.info.name)};
    options.verifyEachPass = true;
    for (uint64_t seed = 5000; seed < 5400; ++seed) {
      const primec::IrModule original = makeModule(seed);
      const Outcome baseline = run(original);
      REQUIRE_MESSAGE(baseline.ok, "seed ", seed, ": ", baseline.error);
      std::string detail;
      CHECK_MESSAGE(optimizedMatches(original, options, baseline, detail), "seed ", seed, ": ", detail);
    }
  }
}

TEST_CASE("optimization is deterministic and idempotent") {
  primec::OptimizationOptions options;
  options.level = 1;
  for (uint64_t seed = 9000; seed < 9150; ++seed) {
    const primec::IrModule original = makeModule(seed);
    primec::IrModule first = original;
    primec::IrModule second = original;
    primec::IrOptimizationReport report;
    std::string error;
    REQUIRE(primec::optimizeIrModule(first, options, primec::IrValidationTarget::Vm, report, error));
    REQUIRE(primec::optimizeIrModule(second, options, primec::IrValidationTarget::Vm, report, error));
    CHECK_MESSAGE(primec::formatIrModule(first) == primec::formatIrModule(second), "seed ", seed);

    // Running the pipeline again on its own output changes nothing.
    primec::IrModule again = first;
    REQUIRE(primec::optimizeIrModule(again, options, primec::IrValidationTarget::Vm, report, error));
    CHECK_MESSAGE(primec::formatIrModule(again) == primec::formatIrModule(first), "seed ", seed, " is not idempotent");
  }
}
