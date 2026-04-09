#include "third_party/doctest.h"

#include "primec/SymbolInterner.h"

#include <string>
#include <vector>

TEST_SUITE_BEGIN("primestruct.semantics.symbol_interner");

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

  REQUIRE(mergedAB.size() == mergedBA.size());
  for (std::size_t i = 0; i < mergedAB.size(); ++i) {
    const primec::SymbolId id = static_cast<primec::SymbolId>(i + 1);
    CHECK(mergedAB.resolve(id) == mergedBA.resolve(id));
  }
  CHECK(mergedAB.resolve(1) == "/std/math/mat2");
  CHECK(mergedAB.resolve(2) == "/std/math/quat");
  CHECK(mergedAB.resolve(3) == "/std/math/vec2");

  const primec::SymbolInterner mergedViaGeneric =
      primec::SymbolInterner::mergeWorkerSnapshotsDeterministic({snapshotB, snapshotA});
  REQUIRE(mergedViaGeneric.size() == mergedAB.size());
  for (std::size_t i = 0; i < mergedAB.size(); ++i) {
    const primec::SymbolId id = static_cast<primec::SymbolId>(i + 1);
    CHECK(mergedViaGeneric.resolve(id) == mergedAB.resolve(id));
  }

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

TEST_SUITE_END();
