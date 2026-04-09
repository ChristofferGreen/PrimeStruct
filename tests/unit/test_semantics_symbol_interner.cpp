#include "third_party/doctest.h"

#include "primec/SymbolInterner.h"

#include <algorithm>
#include <array>
#include <string>
#include <vector>

TEST_SUITE_BEGIN("primestruct.semantics.symbol_interner");

static std::vector<std::string> resolveSymbolTable(const primec::SymbolInterner &interner) {
  std::vector<std::string> symbols;
  symbols.reserve(interner.size());
  for (std::size_t i = 0; i < interner.size(); ++i) {
    const primec::SymbolId id = static_cast<primec::SymbolId>(i + 1);
    symbols.emplace_back(interner.resolve(id));
  }
  return symbols;
}

TEST_CASE("symbol interner assigns deterministic first-seen ids") {
  primec::SymbolInterner interner;
  const primec::SymbolId alpha1 = interner.intern("/std/math/Vec2");
  const primec::SymbolId beta = interner.intern("/std/math/Mat2");
  const primec::SymbolId alpha2 = interner.intern("/std/math/Vec2");

  CHECK(alpha1 == 1);
  CHECK(beta == 2);
  CHECK(alpha2 == alpha1);
  CHECK(interner.size() == 2);
  CHECK(interner.resolve(alpha1) == "/std/math/Vec2");
  CHECK(interner.resolve(beta) == "/std/math/Mat2");
}

TEST_CASE("symbol interner lookup and resolve handle missing values") {
  primec::SymbolInterner interner;
  CHECK(interner.empty());
  CHECK(interner.lookup("/missing").has_value() == false);
  CHECK(interner.resolve(primec::InvalidSymbolId).empty());
  CHECK(interner.resolve(123).empty());

  const primec::SymbolId id = interner.intern("/present");
  REQUIRE(id != primec::InvalidSymbolId);
  CHECK(interner.contains("/present"));
  CHECK(interner.lookup("/present").has_value());
  CHECK(interner.lookup("/present").value() == id);
  CHECK(interner.resolve(id) == "/present");
}

TEST_CASE("symbol interner clears ids and restarts numbering") {
  primec::SymbolInterner interner;
  const primec::SymbolId first = interner.intern("alpha");
  const primec::SymbolId second = interner.intern("beta");
  REQUIRE(first == 1);
  REQUIRE(second == 2);

  interner.clear();
  CHECK(interner.empty());
  CHECK(interner.lookup("alpha").has_value() == false);
  CHECK(interner.resolve(first).empty());

  const primec::SymbolId reset = interner.intern("alpha");
  CHECK(reset == 1);
  CHECK(interner.resolve(reset) == "alpha");
}

TEST_CASE("symbol interner repeat runs keep identical single-thread ids") {
  const std::vector<std::string> sequence = {
      "/std/math/vector", "/std/math/matrix", "/std/math/vector",
      "/std/math/quaternion", "/std/math/vector", "/std/math/matrix",
      "/std/math/complex"};

  std::vector<primec::SymbolId> baselineIds;
  std::vector<std::string> baselineResolved;
  for (int run = 0; run < 6; ++run) {
    primec::SymbolInterner interner;
    std::vector<primec::SymbolId> runIds;
    runIds.reserve(sequence.size());
    for (const std::string &symbol : sequence) {
      runIds.push_back(interner.intern(symbol));
    }

    if (run == 0) {
      baselineIds = runIds;
      baselineResolved.reserve(interner.size());
      for (std::size_t i = 0; i < interner.size(); ++i) {
        const primec::SymbolId id = static_cast<primec::SymbolId>(i + 1);
        baselineResolved.emplace_back(interner.resolve(id));
      }
      continue;
    }

    CHECK(runIds == baselineIds);
    REQUIRE(interner.size() == baselineResolved.size());
    for (std::size_t i = 0; i < baselineResolved.size(); ++i) {
      const primec::SymbolId id = static_cast<primec::SymbolId>(i + 1);
      CHECK(interner.resolve(id) == baselineResolved[i]);
    }
  }
}

TEST_CASE("symbol interner worker snapshot keeps local id order") {
  primec::SymbolInterner interner;
  CHECK(interner.intern("/worker/a") == 1);
  CHECK(interner.intern("/worker/b") == 2);
  CHECK(interner.intern("/worker/a") == 1);
  CHECK(interner.intern("/worker/c") == 3);

  const primec::WorkerSymbolInternerSnapshot snapshot = interner.snapshotForWorker(7);
  CHECK(snapshot.workerId == 7);
  REQUIRE(snapshot.symbolsByLocalId.size() == 3);
  CHECK(snapshot.symbolsByLocalId[0] == "/worker/a");
  CHECK(snapshot.symbolsByLocalId[1] == "/worker/b");
  CHECK(snapshot.symbolsByLocalId[2] == "/worker/c");
}

TEST_CASE("symbol interner two-worker merge helper is input-order independent") {
  primec::SymbolInterner workerA;
  CHECK(workerA.intern("/std/math/vec2") == 1);
  CHECK(workerA.intern("/std/math/mat2") == 2);

  primec::SymbolInterner workerB;
  CHECK(workerB.intern("/std/math/mat2") == 1);
  CHECK(workerB.intern("/std/math/quat") == 2);

  const primec::WorkerSymbolInternerSnapshot snapshotA = workerA.snapshotForWorker(2);
  const primec::WorkerSymbolInternerSnapshot snapshotB = workerB.snapshotForWorker(1);

  const primec::SymbolInterner mergedAB = primec::SymbolInterner::mergeTwoWorkerSnapshotsDeterministic(
      snapshotA, snapshotB);
  const primec::SymbolInterner mergedBA = primec::SymbolInterner::mergeTwoWorkerSnapshotsDeterministic(
      snapshotB, snapshotA);

  const std::vector<std::string> mergedSymbolsAB = resolveSymbolTable(mergedAB);
  const std::vector<std::string> mergedSymbolsBA = resolveSymbolTable(mergedBA);
  CHECK(mergedSymbolsAB == mergedSymbolsBA);
  CHECK(mergedSymbolsAB == std::vector<std::string>{
                              "/std/math/mat2", "/std/math/quat", "/std/math/vec2"});

  const primec::SymbolInterner mergedViaGeneric =
      primec::SymbolInterner::mergeWorkerSnapshotsDeterministic({snapshotB, snapshotA});
  CHECK(resolveSymbolTable(mergedViaGeneric) == mergedSymbolsAB);

  const std::vector<primec::SymbolId> workerBToMerged =
      primec::SymbolInterner::remapLocalIdsToMerged(snapshotB, mergedAB);
  REQUIRE(workerBToMerged.size() == 2);
  CHECK(workerBToMerged[0] == 1);
  CHECK(workerBToMerged[1] == 2);

  const std::vector<primec::SymbolId> workerAToMerged =
      primec::SymbolInterner::remapLocalIdsToMerged(snapshotA, mergedAB);
  REQUIRE(workerAToMerged.size() == 2);
  CHECK(workerAToMerged[0] == 3);
  CHECK(workerAToMerged[1] == 1);
}

TEST_CASE("symbol interner two-worker merge helper is repeat-run deterministic") {
  primec::SymbolInterner workerA;
  CHECK(workerA.intern("/std/math/vec2") == 1);
  CHECK(workerA.intern("/std/math/vec4") == 2);
  CHECK(workerA.intern("/std/math/mat4") == 3);

  primec::SymbolInterner workerB;
  CHECK(workerB.intern("/std/math/mat4") == 1);
  CHECK(workerB.intern("/std/math/quat") == 2);
  CHECK(workerB.intern("/std/math/complex") == 3);

  const primec::WorkerSymbolInternerSnapshot snapshotA = workerA.snapshotForWorker(11);
  const primec::WorkerSymbolInternerSnapshot snapshotB = workerB.snapshotForWorker(5);

  std::vector<std::string> baselineSymbols;
  std::vector<primec::SymbolId> baselineA;
  std::vector<primec::SymbolId> baselineB;

  for (int run = 0; run < 16; ++run) {
    const bool flipInputOrder = (run % 2) != 0;
    const primec::SymbolInterner merged = flipInputOrder
                                              ? primec::SymbolInterner::mergeTwoWorkerSnapshotsDeterministic(
                                                    snapshotB, snapshotA)
                                              : primec::SymbolInterner::mergeTwoWorkerSnapshotsDeterministic(
                                                    snapshotA, snapshotB);
    const std::vector<std::string> symbols = resolveSymbolTable(merged);
    const std::vector<primec::SymbolId> remapA =
        primec::SymbolInterner::remapLocalIdsToMerged(snapshotA, merged);
    const std::vector<primec::SymbolId> remapB =
        primec::SymbolInterner::remapLocalIdsToMerged(snapshotB, merged);

    if (run == 0) {
      baselineSymbols = symbols;
      baselineA = remapA;
      baselineB = remapB;
      continue;
    }

    CHECK(symbols == baselineSymbols);
    CHECK(remapA == baselineA);
    CHECK(remapB == baselineB);
  }
}

TEST_CASE("symbol interner two-worker merge tie-breaks equal worker ids lexicographically") {
  const primec::WorkerSymbolInternerSnapshot snapshotLexLow{
      .workerId = 7,
      .symbolsByLocalId = {"/std/math/alpha", "/std/math/shared"},
  };
  const primec::WorkerSymbolInternerSnapshot snapshotLexHigh{
      .workerId = 7,
      .symbolsByLocalId = {"/std/math/zeta", "/std/math/shared"},
  };

  const primec::SymbolInterner mergedLowHigh =
      primec::SymbolInterner::mergeTwoWorkerSnapshotsDeterministic(snapshotLexLow,
                                                                    snapshotLexHigh);
  const primec::SymbolInterner mergedHighLow =
      primec::SymbolInterner::mergeTwoWorkerSnapshotsDeterministic(snapshotLexHigh,
                                                                    snapshotLexLow);
  const std::vector<std::string> expectedSymbols = {"/std/math/alpha",
                                                    "/std/math/shared",
                                                    "/std/math/zeta"};
  CHECK(resolveSymbolTable(mergedLowHigh) == expectedSymbols);
  CHECK(resolveSymbolTable(mergedHighLow) == expectedSymbols);

  const std::vector<primec::SymbolId> remapLexLow =
      primec::SymbolInterner::remapLocalIdsToMerged(snapshotLexLow, mergedLowHigh);
  REQUIRE(remapLexLow.size() == 2);
  CHECK(remapLexLow[0] == 1);
  CHECK(remapLexLow[1] == 2);

  const std::vector<primec::SymbolId> remapLexHigh =
      primec::SymbolInterner::remapLocalIdsToMerged(snapshotLexHigh, mergedLowHigh);
  REQUIRE(remapLexHigh.size() == 2);
  CHECK(remapLexHigh[0] == 3);
  CHECK(remapLexHigh[1] == 2);
}

TEST_CASE("symbol interner N-worker merge preserves canonical first-seen ordering") {
  primec::SymbolInterner workerA;
  CHECK(workerA.intern("/std/math/c") == 1);
  CHECK(workerA.intern("/std/math/a") == 2);

  primec::SymbolInterner workerB;
  CHECK(workerB.intern("/std/math/b") == 1);
  CHECK(workerB.intern("/std/math/c") == 2);

  primec::SymbolInterner workerC;
  CHECK(workerC.intern("/std/math/d") == 1);
  CHECK(workerC.intern("/std/math/a") == 2);

  const primec::WorkerSymbolInternerSnapshot snapshotA = workerA.snapshotForWorker(3);
  const primec::WorkerSymbolInternerSnapshot snapshotB = workerB.snapshotForWorker(1);
  const primec::WorkerSymbolInternerSnapshot snapshotC = workerC.snapshotForWorker(2);

  const primec::SymbolInterner merged = primec::SymbolInterner::mergeWorkerSnapshotsDeterministic(
      {snapshotA, snapshotC, snapshotB});

  const std::vector<std::string> expectedSymbols = {
      "/std/math/b", "/std/math/c", "/std/math/d", "/std/math/a"};
  CHECK(resolveSymbolTable(merged) == expectedSymbols);

  const std::vector<primec::SymbolId> remapA =
      primec::SymbolInterner::remapLocalIdsToMerged(snapshotA, merged);
  REQUIRE(remapA.size() == 2);
  CHECK(remapA[0] == 2);
  CHECK(remapA[1] == 4);

  const std::vector<primec::SymbolId> remapB =
      primec::SymbolInterner::remapLocalIdsToMerged(snapshotB, merged);
  REQUIRE(remapB.size() == 2);
  CHECK(remapB[0] == 1);
  CHECK(remapB[1] == 2);

  const std::vector<primec::SymbolId> remapC =
      primec::SymbolInterner::remapLocalIdsToMerged(snapshotC, merged);
  REQUIRE(remapC.size() == 2);
  CHECK(remapC[0] == 3);
  CHECK(remapC[1] == 4);
}

TEST_CASE("symbol interner N-worker merge is permutation and repeat-run deterministic") {
  primec::SymbolInterner workerA;
  CHECK(workerA.intern("/std/math/x") == 1);
  CHECK(workerA.intern("/std/math/y") == 2);
  CHECK(workerA.intern("/std/math/p") == 3);

  primec::SymbolInterner workerB;
  CHECK(workerB.intern("/std/math/a") == 1);
  CHECK(workerB.intern("/std/math/x") == 2);
  CHECK(workerB.intern("/std/math/z") == 3);

  primec::SymbolInterner workerC;
  CHECK(workerC.intern("/std/math/b") == 1);
  CHECK(workerC.intern("/std/math/y") == 2);
  CHECK(workerC.intern("/std/math/a") == 3);

  primec::SymbolInterner workerD;
  CHECK(workerD.intern("/std/math/c") == 1);
  CHECK(workerD.intern("/std/math/z") == 2);
  CHECK(workerD.intern("/std/math/w") == 3);

  const std::array<primec::WorkerSymbolInternerSnapshot, 4> snapshots = {
      workerA.snapshotForWorker(30),
      workerB.snapshotForWorker(10),
      workerC.snapshotForWorker(20),
      workerD.snapshotForWorker(40),
  };

  std::array<std::size_t, 4> permutation = {0, 1, 2, 3};
  std::vector<std::string> baselineSymbols;
  std::array<std::vector<primec::SymbolId>, 4> baselineRemaps;
  bool hasBaseline = false;
  int permutationsChecked = 0;

  do {
    std::vector<primec::WorkerSymbolInternerSnapshot> input;
    input.reserve(permutation.size());
    for (std::size_t index : permutation) {
      input.push_back(snapshots[index]);
    }

    const primec::SymbolInterner merged =
        primec::SymbolInterner::mergeWorkerSnapshotsDeterministic(std::move(input));
    const std::vector<std::string> mergedSymbols = resolveSymbolTable(merged);
    const std::array<std::vector<primec::SymbolId>, 4> remaps = {
        primec::SymbolInterner::remapLocalIdsToMerged(snapshots[0], merged),
        primec::SymbolInterner::remapLocalIdsToMerged(snapshots[1], merged),
        primec::SymbolInterner::remapLocalIdsToMerged(snapshots[2], merged),
        primec::SymbolInterner::remapLocalIdsToMerged(snapshots[3], merged),
    };

    if (!hasBaseline) {
      baselineSymbols = mergedSymbols;
      baselineRemaps = remaps;
      hasBaseline = true;
    } else {
      CHECK(mergedSymbols == baselineSymbols);
      CHECK(remaps == baselineRemaps);
    }

    ++permutationsChecked;
  } while (std::next_permutation(permutation.begin(), permutation.end()));

  CHECK(permutationsChecked == 24);
}

TEST_CASE("symbol interner N-worker merge tie-breaks equal worker ids lexicographically") {
  const std::array<primec::WorkerSymbolInternerSnapshot, 3> snapshots = {
      primec::WorkerSymbolInternerSnapshot{
          .workerId = 9,
          .symbolsByLocalId = {"/std/math/gamma", "/std/math/shared-a"},
      },
      primec::WorkerSymbolInternerSnapshot{
          .workerId = 9,
          .symbolsByLocalId = {"/std/math/alpha", "/std/math/shared-b"},
      },
      primec::WorkerSymbolInternerSnapshot{
          .workerId = 9,
          .symbolsByLocalId = {"/std/math/beta", "/std/math/shared-a"},
      },
  };

  std::array<std::size_t, 3> permutation = {0, 1, 2};
  std::vector<std::string> baselineSymbols;
  bool hasBaseline = false;
  int permutationsChecked = 0;

  do {
    std::vector<primec::WorkerSymbolInternerSnapshot> input;
    input.reserve(permutation.size());
    for (std::size_t index : permutation) {
      input.push_back(snapshots[index]);
    }

    const primec::SymbolInterner merged =
        primec::SymbolInterner::mergeWorkerSnapshotsDeterministic(std::move(input));
    const std::vector<std::string> mergedSymbols = resolveSymbolTable(merged);

    if (!hasBaseline) {
      baselineSymbols = mergedSymbols;
      hasBaseline = true;
    } else {
      CHECK(mergedSymbols == baselineSymbols);
    }

    ++permutationsChecked;
  } while (std::next_permutation(permutation.begin(), permutation.end()));

  CHECK(permutationsChecked == 6);
  CHECK(baselineSymbols == std::vector<std::string>{
                              "/std/math/alpha", "/std/math/shared-b",
                              "/std/math/beta",  "/std/math/shared-a",
                              "/std/math/gamma"});
}

TEST_SUITE_END();
