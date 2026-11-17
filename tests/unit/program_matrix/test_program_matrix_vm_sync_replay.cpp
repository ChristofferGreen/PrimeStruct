#include "program_matrix.h"

TEST_SUITE_BEGIN("primestruct.program_matrix.vm_sync_replay");

// `primevm --vm-record-sync` and `--vm-replay-sync` (docs/spec/vm-design.md, Synchronization record and replay;
// TODO-5901) on a program with a `Mutex` and a `Channel`: a `--vm-threads` run writes the record, a run on the
// deterministic scheduler follows it and prints what the threaded run printed. The program prints the values in the
// order they arrive through the channel, so its output depends on the interleaving and each record is its own
// run's. Each case is one front-end run per primevm call (0.5 to 1 s).

namespace {

const char *const Source = R"(
import /std/sync/*

[struct]
Tally() {
  [i64 mut] total{0i64}
}

[effects(channel, lock, heap_alloc), return<void>]
produce([Channel<i64>] jobs, [Mutex<Tally>] tally, [i64] first) {
  [i64 mut] i{0i64}
  while(i < 3i64) {
    [i64] value{first + i}
    tally.locked([value]([Reference<Tally>] state) {
      state.total = state.total + value
    })
    jobs.send(value)
    i = i + 1i64
  }
}

[effects(task, channel, lock, heap_alloc, io_out), return<int>]
main() {
  jobs{Channel<i64>{[capacity] 2i32}}
  tally{Mutex<Tally>{Tally{}}}
  [Task<void>] a{[spawn] produce(jobs, tally, 10i64)}
  [Task<void>] b{[spawn] produce(jobs, tally, 20i64)}
  [i32 mut] received{0i32}
  while(received < 6i32) {
    pick(jobs.receive()) {
      none {
        received = 6i32
      }
      some(value) {
        print_line(value)
        received = received + 1i32
      }
    }
  }
  wait(a)
  wait(b)
  print_line(tally.locked([]([Reference<Tally>] state) {
    return(state.total)
  }))
  return(0i32)
}
)";

std::filesystem::path writeSource(const std::string &name, const std::string &source) {
  const std::filesystem::path dir =
      primec::testing::testScratchPath("program_matrix/" + name + "/x").parent_path();
  std::filesystem::create_directories(dir);
  const std::filesystem::path sourcePath = dir / "program.prime";
  std::ofstream file(sourcePath, std::ios::binary);
  file << source;
  return sourcePath;
}

program_matrix::Outcome
primevm(const std::filesystem::path &source, const std::string &flags, const std::string &stem) {
  return program_matrix::runCaptured("./primevm " + program_matrix::quote(source.string()) +
                                         " --entry /main -O2 " + flags,
                                     source.parent_path() / stem);
}

} // namespace

TEST_CASE("primevm replays the record of a threaded run to the same stdout") {
  const std::filesystem::path source = writeSource("vm_sync_replay_pairs", Source);
  const std::string record = (source.parent_path() / "run.sync").string();
  for (int repeat = 0; repeat < 2; ++repeat) {
    INFO("pair " << repeat);
    const program_matrix::Outcome threaded = primevm(
        source, "--vm-threads --vm-record-sync " + program_matrix::quote(record), "threaded");
    REQUIRE_MESSAGE(threaded.ran, threaded.note);
    CHECK(threaded.exitCode == 0);
    CHECK(threaded.out.size() > 6);
    const program_matrix::Outcome replayed =
        primevm(source, "--vm-replay-sync " + program_matrix::quote(record), "replayed");
    REQUIRE_MESSAGE(replayed.ran, replayed.note);
    CHECK(replayed.exitCode == threaded.exitCode);
    CHECK(replayed.out == threaded.out);
    CHECK(replayed.err == threaded.err);
  }
}

TEST_CASE("primevm refuses the record of a changed program and a record of a run without threads") {
  const std::filesystem::path source = writeSource("vm_sync_replay_refused", Source);
  const std::string record = (source.parent_path() / "run.sync").string();
  const program_matrix::Outcome threaded =
      primevm(source, "--vm-threads --vm-record-sync " + program_matrix::quote(record), "threaded");
  REQUIRE_MESSAGE(threaded.ran, threaded.note);
  REQUIRE(threaded.exitCode == 0);

  std::string changedSource = Source;
  changedSource.replace(changedSource.find("20i64"), 5, "30i64");
  const std::filesystem::path changed = writeSource("vm_sync_replay_changed", changedSource);
  const program_matrix::Outcome refused =
      primevm(changed, "--vm-replay-sync " + program_matrix::quote(record), "refused");
  REQUIRE_MESSAGE(refused.ran, refused.note);
  CHECK(refused.exitCode != 0);
  CHECK(refused.out.empty());
  CHECK(refused.err.find("recorded from a different program") != std::string::npos);

  // --vm-record-sync without --vm-threads is a usage error and runs nothing.
  const program_matrix::Outcome usage =
      primevm(source, "--vm-record-sync " + program_matrix::quote(record), "usage");
  REQUIRE_MESSAGE(usage.ran, usage.note);
  CHECK(usage.exitCode != 0);
  CHECK(usage.out.empty());
  CHECK(usage.err.find("needs --vm-threads") != std::string::npos);
}
