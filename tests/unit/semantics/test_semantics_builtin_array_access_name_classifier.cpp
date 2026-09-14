#include "third_party/doctest.h"

#include <optional>
#include <string>
#include <string_view>

#include "primec/support/BuiltinArrayAccessNameClassifier.h"

TEST_SUITE_BEGIN("primestruct.semantics.builtin_array_access_name_classifier");

namespace {

using primec::AccessAliasSpellingMode;
using primec::BuiltinArrayAccessAliasOutcome;
using primec::BuiltinArrayAccessAliasResult;
using primec::BuiltinArrayAccessKeyValueLookup;
using primec::classifyAccessAliasToken;
using primec::classifyBuiltinArrayAccessNameForIrLowerer;
using primec::classifyBuiltinArrayAccessNameForSemantics;
using primec::matchBuiltinArrayAccessAliasUnderPrefix;

// Real prefixes production code computes via collectionMemberRootLocal/
// experimentalCollectionMemberRootLocal (semantics) and
// collectionMemberRoot/experimentalCollectionMemberRoot/
// collection_paths::modulePrefixBare (ir_lowerer) - hardcoded here from
// StdlibCollectionPaths.h's real constants (kVectorFolder="vector",
// kInternalSoaStorageFolder="soa_storage", experimentalFolder("vector")=
// "experimental_vector") so these tests exercise the same strings real
// call sites would pass.
const std::string kStdCollectionsPrefix = "std/collections/";
const std::string kStdVectorRootPrefix = "std/collections/vector/";
const std::string kExperimentalVectorPrefix = "std/collections/experimental_vector/";
const std::string kExperimentalMapPrefix = "std/collections/experimental_map/";
const std::string kInternalSoaStorageFolderPrefix = "std/collections/soa_storage/";

BuiltinArrayAccessKeyValueLookup noKeyValueMatch() {
  return [](std::string_view) {
    return BuiltinArrayAccessAliasResult{};  // kNoMatch, empty token
  };
}

BuiltinArrayAccessKeyValueLookup semanticsStyleKeyValueLookup(std::string_view resolvedMember) {
  // Mirrors semantics' real shape: resolve a member name, then classify it
  // like every other branch (so it CAN accept "at"/"at_unsafe").
  return [resolvedMember](std::string_view name) -> BuiltinArrayAccessAliasResult {
    if (name.find('/') == std::string_view::npos) {
      return {};
    }
    std::optional<std::string> token =
        classifyAccessAliasToken(std::string(resolvedMember), AccessAliasSpellingMode::kFull);
    if (token) {
      return BuiltinArrayAccessAliasResult{BuiltinArrayAccessAliasOutcome::kAccept, *token};
    }
    return BuiltinArrayAccessAliasResult{BuiltinArrayAccessAliasOutcome::kReject, {}};
  };
}

BuiltinArrayAccessKeyValueLookup irLowererStyleRejectingLookup(bool resolvesAsKeyValuePath) {
  // Mirrors ir_lowerer's real shape: bool-only, and ANY resolved key-value
  // path is an unconditional reject, never an accept.
  return [resolvesAsKeyValuePath](std::string_view) -> BuiltinArrayAccessAliasResult {
    if (resolvesAsKeyValuePath) {
      return BuiltinArrayAccessAliasResult{BuiltinArrayAccessAliasOutcome::kReject, {}};
    }
    return {};
  };
}

}  // namespace

// --- classifyAccessAliasToken: the shared, pure alias-token primitive -----

TEST_CASE("classifyAccessAliasToken bare mode recognizes at/at_ref/at_unsafe/at_unsafe_ref only") {
  CHECK(classifyAccessAliasToken("at", AccessAliasSpellingMode::kBareOnly) == "at");
  CHECK(classifyAccessAliasToken("at_ref", AccessAliasSpellingMode::kBareOnly) == "at");
  CHECK(classifyAccessAliasToken("at_unsafe", AccessAliasSpellingMode::kBareOnly) == "at_unsafe");
  CHECK(classifyAccessAliasToken("at_unsafe_ref", AccessAliasSpellingMode::kBareOnly) == "at_unsafe");
  CHECK_FALSE(classifyAccessAliasToken("vectorAt", AccessAliasSpellingMode::kBareOnly).has_value());
  CHECK_FALSE(classifyAccessAliasToken("vectorAtUnsafe", AccessAliasSpellingMode::kBareOnly).has_value());
}

TEST_CASE("classifyAccessAliasToken concatenated mode recognizes vectorAt/vectorAtUnsafe only") {
  CHECK(classifyAccessAliasToken("vectorAt", AccessAliasSpellingMode::kConcatenatedOnly) == "at");
  CHECK(classifyAccessAliasToken("vectorAtUnsafe", AccessAliasSpellingMode::kConcatenatedOnly) == "at_unsafe");
  CHECK_FALSE(classifyAccessAliasToken("at", AccessAliasSpellingMode::kConcatenatedOnly).has_value());
  CHECK_FALSE(classifyAccessAliasToken("at_ref", AccessAliasSpellingMode::kConcatenatedOnly).has_value());
}

TEST_CASE("classifyAccessAliasToken full mode is the union of bare and concatenated") {
  CHECK(classifyAccessAliasToken("at", AccessAliasSpellingMode::kFull) == "at");
  CHECK(classifyAccessAliasToken("at_ref", AccessAliasSpellingMode::kFull) == "at");
  CHECK(classifyAccessAliasToken("at_unsafe", AccessAliasSpellingMode::kFull) == "at_unsafe");
  CHECK(classifyAccessAliasToken("at_unsafe_ref", AccessAliasSpellingMode::kFull) == "at_unsafe");
  CHECK(classifyAccessAliasToken("vectorAt", AccessAliasSpellingMode::kFull) == "at");
  CHECK(classifyAccessAliasToken("vectorAtUnsafe", AccessAliasSpellingMode::kFull) == "at_unsafe");
}

TEST_CASE("classifyAccessAliasToken strips __t<hash> template suffix then __<n> generated suffix") {
  CHECK(classifyAccessAliasToken("at__t12345678", AccessAliasSpellingMode::kFull) == "at");
  CHECK(classifyAccessAliasToken("at__3", AccessAliasSpellingMode::kFull) == "at");
  CHECK(classifyAccessAliasToken("vectorAt__t9", AccessAliasSpellingMode::kConcatenatedOnly) == "at");
}

TEST_CASE("classifyAccessAliasToken never recognizes the deleted bare capitalized spelling (branch 2)") {
  // TODO-5293 Step (3): the bare "At"/"AtUnsafe" spelling was confirmed
  // dead and deleted from semantics' production
  // accessAliasFromMemberName. This classifier must not resurrect it in
  // any mode.
  CHECK_FALSE(classifyAccessAliasToken("At", AccessAliasSpellingMode::kFull).has_value());
  CHECK_FALSE(classifyAccessAliasToken("AtUnsafe", AccessAliasSpellingMode::kFull).has_value());
  CHECK_FALSE(classifyAccessAliasToken("At", AccessAliasSpellingMode::kBareOnly).has_value());
  CHECK_FALSE(classifyAccessAliasToken("AtUnsafe", AccessAliasSpellingMode::kConcatenatedOnly).has_value());
}

TEST_CASE("classifyAccessAliasToken rejects unrelated member names") {
  CHECK_FALSE(classifyAccessAliasToken("insert", AccessAliasSpellingMode::kFull).has_value());
  CHECK_FALSE(classifyAccessAliasToken("", AccessAliasSpellingMode::kFull).has_value());
}

// --- matchBuiltinArrayAccessAliasUnderPrefix: the shared prefix primitive -

TEST_CASE("matchBuiltinArrayAccessAliasUnderPrefix reports kNoMatch when the prefix is absent") {
  auto result = matchBuiltinArrayAccessAliasUnderPrefix("std/other/at", "std/collections/vector/", {},
                                                         AccessAliasSpellingMode::kFull);
  CHECK(result.outcome == BuiltinArrayAccessAliasOutcome::kNoMatch);
}

TEST_CASE("matchBuiltinArrayAccessAliasUnderPrefix legacy shape rejects a residual slash in the raw alias") {
  auto result = matchBuiltinArrayAccessAliasUnderPrefix("std/collections/vector/nested/at",
                                                         "std/collections/vector/", {},
                                                         AccessAliasSpellingMode::kFull);
  CHECK(result.outcome == BuiltinArrayAccessAliasOutcome::kReject);
}

TEST_CASE("matchBuiltinArrayAccessAliasUnderPrefix legacy shape accepts a clean alias") {
  auto result = matchBuiltinArrayAccessAliasUnderPrefix("std/collections/vector/at", "std/collections/vector/",
                                                         {}, AccessAliasSpellingMode::kFull);
  CHECK(result.outcome == BuiltinArrayAccessAliasOutcome::kAccept);
  CHECK(result.token == "at");
}

TEST_CASE("matchBuiltinArrayAccessAliasUnderPrefix with rejectOnRawResidualSlash=false strips before classifying") {
  // The one real call site (semantics' stdVectorRoot) that behaves this
  // way: a '/' occurring after a __t<hash> marker is stripped away before
  // classification is attempted, so it can still accept.
  auto strippedAccepts = matchBuiltinArrayAccessAliasUnderPrefix(
      "std/collections/vector/at__t9/foo", "std/collections/vector/", {}, AccessAliasSpellingMode::kFull,
      /*rejectOnRawResidualSlash=*/false);
  CHECK(strippedAccepts.outcome == BuiltinArrayAccessAliasOutcome::kAccept);
  CHECK(strippedAccepts.token == "at");

  // The default (true) legacy shape genuinely disagrees on this exact
  // input - it rejects outright on the raw residual slash, never
  // attempting the strip.
  auto legacyRejects = matchBuiltinArrayAccessAliasUnderPrefix(
      "std/collections/vector/at__t9/foo", "std/collections/vector/", {}, AccessAliasSpellingMode::kFull);
  CHECK(legacyRejects.outcome == BuiltinArrayAccessAliasOutcome::kReject);
}

TEST_CASE("matchBuiltinArrayAccessAliasUnderPrefix receiver-base shape (branch 3) disambiguates by receiver type") {
  // specializedVectorMethodAccessCall shape:
  // "std/collections/vector/Vector__t12345678/at".
  auto matchesSpecializedVector = matchBuiltinArrayAccessAliasUnderPrefix(
      "std/collections/vector/Vector__t12345678/at", "std/collections/vector/", "Vector",
      AccessAliasSpellingMode::kBareOnly);
  CHECK(matchesSpecializedVector.outcome == BuiltinArrayAccessAliasOutcome::kAccept);
  CHECK(matchesSpecializedVector.token == "at");

  // A different receiver type's "at" member under the same folder must not
  // collide by spelling alone.
  auto rejectsOtherReceiver = matchBuiltinArrayAccessAliasUnderPrefix(
      "std/collections/vector/OtherType/at", "std/collections/vector/", "Vector",
      AccessAliasSpellingMode::kBareOnly);
  CHECK(rejectsOtherReceiver.outcome == BuiltinArrayAccessAliasOutcome::kReject);

  // Bare receiverBase (no "__" suffix) also matches directly.
  auto matchesBareReceiver = matchBuiltinArrayAccessAliasUnderPrefix(
      "std/collections/vector/Vector/at", "std/collections/vector/", "Vector", AccessAliasSpellingMode::kBareOnly);
  CHECK(matchesBareReceiver.outcome == BuiltinArrayAccessAliasOutcome::kAccept);
}

TEST_CASE("matchBuiltinArrayAccessAliasUnderPrefix receiver-base shape (branch 3) still classifies bare-only") {
  // matchAccessAlias never recognizes the concatenated "vectorAt" spelling
  // - only ir_lowerer's separate matchLegacyAccessAlias does.
  auto result = matchBuiltinArrayAccessAliasUnderPrefix("std/collections/vector/vectorAt",
                                                         "std/collections/vector/", "Vector",
                                                         AccessAliasSpellingMode::kBareOnly);
  CHECK(result.outcome == BuiltinArrayAccessAliasOutcome::kReject);
}

// --- classifyBuiltinArrayAccessNameForSemantics: real semantics-stage call
// patterns (mirroring semantics::getBuiltinArrayAccessName post branch-2
// deletion) --------------------------------------------------------------

TEST_CASE("semantics composition: plain std vector at/at_unsafe") {
  std::string out;
  CHECK(classifyBuiltinArrayAccessNameForSemantics("std/collections/vector/at", "vector/at", kStdCollectionsPrefix,
                                                    kExperimentalVectorPrefix, kExperimentalMapPrefix,
                                                    kStdVectorRootPrefix, noKeyValueMatch(), out));
  CHECK(out == "at");

  CHECK(classifyBuiltinArrayAccessNameForSemantics("std/collections/vector/at_unsafe", "vector/at_unsafe",
                                                    kStdCollectionsPrefix, kExperimentalVectorPrefix,
                                                    kExperimentalMapPrefix, kStdVectorRootPrefix, noKeyValueMatch(),
                                                    out));
  CHECK(out == "at_unsafe");
}

TEST_CASE("semantics composition: concatenated vectorAt spelling still recognized (branch 2's non-divergent half)") {
  std::string out;
  CHECK(classifyBuiltinArrayAccessNameForSemantics("std/collections/vector/vectorAt", "vector/vectorAt",
                                                    kStdCollectionsPrefix, kExperimentalVectorPrefix,
                                                    kExperimentalMapPrefix, kStdVectorRootPrefix, noKeyValueMatch(),
                                                    out));
  CHECK(out == "at");
}

TEST_CASE("semantics composition: bare capitalized At/AtUnsafe is rejected (branch 2, confirmed dead, deleted)") {
  std::string out;
  CHECK_FALSE(classifyBuiltinArrayAccessNameForSemantics("std/collections/vector/At", "vector/At",
                                                          kStdCollectionsPrefix, kExperimentalVectorPrefix,
                                                          kExperimentalMapPrefix, kStdVectorRootPrefix,
                                                          noKeyValueMatch(), out));
  CHECK_FALSE(classifyBuiltinArrayAccessNameForSemantics("std/collections/vector/AtUnsafe", "vector/AtUnsafe",
                                                          kStdCollectionsPrefix, kExperimentalVectorPrefix,
                                                          kExperimentalMapPrefix, kStdVectorRootPrefix,
                                                          noKeyValueMatch(), out));
}

TEST_CASE("semantics composition: experimental vector/map roots") {
  std::string out;
  CHECK(classifyBuiltinArrayAccessNameForSemantics("std/collections/experimental_vector/at",
                                                    "experimental_vector/at", kStdCollectionsPrefix,
                                                    kExperimentalVectorPrefix, kExperimentalMapPrefix,
                                                    kStdVectorRootPrefix, noKeyValueMatch(), out));
  CHECK(out == "at");
  CHECK(classifyBuiltinArrayAccessNameForSemantics("std/collections/experimental_map/at_unsafe",
                                                    "experimental_map/at_unsafe", kStdCollectionsPrefix,
                                                    kExperimentalVectorPrefix, kExperimentalMapPrefix,
                                                    kStdVectorRootPrefix, noKeyValueMatch(), out));
  CHECK(out == "at_unsafe");
}

TEST_CASE("semantics composition: branch 3's receiver-typed shape is NOT part of the shared core - returns false") {
  // TODO-5293 Step (2): semantics' real getBuiltinArrayAccessName provably
  // returns false on this exact Expr shape (the stdVectorRoot branch's
  // stripTemplateSpecializationSuffix erases from the first "__t" onward,
  // consuming the trailing "/at" too, leaving "Vector" - which classifies
  // to nothing). This is the confirmed real divergence from ir_lowerer's
  // receiver-base-disambiguated "true" on the identical shape - the
  // semantics composition here must reproduce semantics' "false", not
  // ir_lowerer's "true".
  std::string out;
  CHECK_FALSE(classifyBuiltinArrayAccessNameForSemantics(
      "std/collections/vector/Vector__t12345678/at", "vector/Vector__t12345678/at", kStdCollectionsPrefix,
      kExperimentalVectorPrefix, kExperimentalMapPrefix, kStdVectorRootPrefix, noKeyValueMatch(), out));
}

TEST_CASE("semantics composition: array/ root is always rejected") {
  std::string out;
  CHECK_FALSE(classifyBuiltinArrayAccessNameForSemantics("array/at", "array/at", kStdCollectionsPrefix,
                                                          kExperimentalVectorPrefix, kExperimentalMapPrefix,
                                                          kStdVectorRootPrefix, noKeyValueMatch(), out));
}

TEST_CASE("semantics composition: key-value helper delegate CAN accept when the resolved member is at/at_unsafe") {
  std::string out;
  CHECK(classifyBuiltinArrayAccessNameForSemantics(
      "std/collections/experimental_map/Map__t1/at", "at", kStdCollectionsPrefix, kExperimentalVectorPrefix,
      kExperimentalMapPrefix, kStdVectorRootPrefix, semanticsStyleKeyValueLookup("at"), out));
  CHECK(out == "at");
}

TEST_CASE("semantics composition: key-value helper delegate rejects when the resolved member is not at/at_unsafe") {
  std::string out;
  CHECK_FALSE(classifyBuiltinArrayAccessNameForSemantics(
      "std/collections/experimental_map/Map__t1/insert", "insert", kStdCollectionsPrefix,
      kExperimentalVectorPrefix, kExperimentalMapPrefix, kStdVectorRootPrefix,
      semanticsStyleKeyValueLookup("insert"), out));
}

TEST_CASE("semantics composition: bare unrooted name falls through to the raw-name classify") {
  std::string out;
  CHECK(classifyBuiltinArrayAccessNameForSemantics("at", "at", kStdCollectionsPrefix, kExperimentalVectorPrefix,
                                                    kExperimentalMapPrefix, kStdVectorRootPrefix, noKeyValueMatch(),
                                                    out));
  CHECK(out == "at");
}

TEST_CASE("semantics composition: empty raw name is rejected") {
  std::string out;
  CHECK_FALSE(classifyBuiltinArrayAccessNameForSemantics("", "", kStdCollectionsPrefix, kExperimentalVectorPrefix,
                                                          kExperimentalMapPrefix, kStdVectorRootPrefix,
                                                          noKeyValueMatch(), out));
}

// --- classifyBuiltinArrayAccessNameForIrLowerer: real ir_lowerer-stage
// call patterns (mirroring ir_lowerer::getBuiltinArrayAccessName) ---------

namespace {
// A vectorHelperPathAtName/AtUnsafeName value that never naturally
// collides with the scopedName spellings these tests exercise, so the
// early exclusion check (tested separately below) stays inert here.
const std::string kUnusedVectorHelperPathAt = "std/collections/vector/__unused_canonical_at_marker__";
const std::string kUnusedVectorHelperPathAtUnsafe = "std/collections/vector/__unused_canonical_at_unsafe_marker__";

bool irLowererClassify(const std::string &scopedName, const std::string &rawName,
                       const BuiltinArrayAccessKeyValueLookup &kvLookup, std::string &out) {
  return classifyBuiltinArrayAccessNameForIrLowerer(
      scopedName, rawName, kStdCollectionsPrefix, kStdVectorRootPrefix, kExperimentalVectorPrefix,
      kStdVectorRootPrefix /* legacyVectorFolderPrefix - real production passes the same value here */,
      kInternalSoaStorageFolderPrefix, kUnusedVectorHelperPathAt, kUnusedVectorHelperPathAtUnsafe, kvLookup, {},
      out);
}
}  // namespace

TEST_CASE("ir_lowerer composition: plain std vector at/at_unsafe") {
  std::string out;
  CHECK(irLowererClassify("std/collections/vector/at", "at", noKeyValueMatch(), out));
  CHECK(out == "at");
  CHECK(irLowererClassify("std/collections/vector/at_unsafe", "at_unsafe", noKeyValueMatch(), out));
  CHECK(out == "at_unsafe");
}

TEST_CASE("ir_lowerer composition: bare capitalized At/AtUnsafe never matched (never existed here)") {
  std::string out;
  CHECK_FALSE(irLowererClassify("std/collections/vector/At", "At", noKeyValueMatch(), out));
}

TEST_CASE("ir_lowerer composition: branch 3 receiver-typed shape IS accepted, unlike semantics") {
  // The specializedVectorMethodAccessCall shape - the confirmed real
  // divergence point from semantics (which returns false on the identical
  // shape, see the semantics test above).
  std::string out;
  CHECK(irLowererClassify("std/collections/vector/Vector__t12345678/at", "at", noKeyValueMatch(), out));
  CHECK(out == "at");
}

TEST_CASE("ir_lowerer composition: legacy concatenated vectorAt under std/collections/") {
  std::string out;
  CHECK(irLowererClassify("std/collections/vectorAt", "vectorAt", noKeyValueMatch(), out));
  CHECK(out == "at");
}

TEST_CASE("ir_lowerer composition: branch 4 SoaColumn receiver-typed shape is accepted") {
  std::string out;
  CHECK(irLowererClassify("std/collections/soa_storage/SoaColumn__t42/at_unsafe", "at_unsafe", noKeyValueMatch(),
                          out));
  CHECK(out == "at_unsafe");
}

TEST_CASE("ir_lowerer composition: soa_storage fallback callback is consulted, and only accepted when at/at_unsafe") {
  std::string out;
  bool fallbackCalled = false;
  auto fallback = [&](const std::string &name) -> std::optional<std::string> {
    fallbackCalled = true;
    CHECK(name.rfind("std/collections/soa_storage/", 0) == 0);
    return std::string("at");
  };
  CHECK(classifyBuiltinArrayAccessNameForIrLowerer(
      "std/collections/soa_storage/std/image/whatever", "whatever", kStdCollectionsPrefix, kStdVectorRootPrefix,
      kExperimentalVectorPrefix, kStdVectorRootPrefix, kInternalSoaStorageFolderPrefix,
      "std/collections/vector/at", "std/collections/vector/at_unsafe", noKeyValueMatch(), fallback, out));
  CHECK(fallbackCalled);
  CHECK(out == "at");
}

TEST_CASE("ir_lowerer composition: key-value surface match is an unconditional reject, never an accept") {
  // TODO-5293 fresh finding this round: ir_lowerer's real
  // resolvesKeyValueHelperSurfacePath-based check is used as a hard
  // rejection of ANY key-value-surface path, unlike semantics which can
  // still accept when the resolved member is at/at_unsafe.
  std::string out;
  CHECK_FALSE(irLowererClassify("std/collections/experimental_map/Map__t1/at", "at",
                                irLowererStyleRejectingLookup(true), out));
}

TEST_CASE("ir_lowerer composition: array/ and bare vector/ roots are always rejected") {
  std::string out;
  CHECK_FALSE(irLowererClassify("array/at", "at", noKeyValueMatch(), out));
  CHECK_FALSE(irLowererClassify("vector/at", "at", noKeyValueMatch(), out));
}

TEST_CASE("ir_lowerer composition: bare unrooted at/at_unsafe accepted via raw-name fallback") {
  std::string out;
  CHECK(irLowererClassify("at", "at", noKeyValueMatch(), out));
  CHECK(out == "at");
  CHECK(irLowererClassify("at_unsafe", "at_unsafe", noKeyValueMatch(), out));
  CHECK(out == "at_unsafe");
}

TEST_CASE("ir_lowerer composition: raw-name fallback rejects at_ref (bare-mode-only spelling not accepted here)") {
  // ir_lowerer's real rawName fallback compares only against the literal
  // "at"/"at_unsafe" strings, never "at_ref"/"at_unsafe_ref" - unlike the
  // bare-mode primitive used earlier in the same function for the
  // receiver-typed roots.
  std::string out;
  CHECK_FALSE(irLowererClassify("at_ref", "at_ref", noKeyValueMatch(), out));
}

TEST_CASE("ir_lowerer composition: the injected canonical vector-helper surface path is excluded outright") {
  // The real body's very first check: scopedName (after only a generated-
  // suffix strip - no template-suffix strip here) equal to the canonical
  // stdlib vector "at"/"at_unsafe" helper surface path is rejected before
  // any of the root-walking logic runs at all, even though it would
  // otherwise classify as "at" under the std vector root.
  std::string out;
  const std::string canonicalAtPath = "std/collections/vector/Vector/at";
  CHECK_FALSE(classifyBuiltinArrayAccessNameForIrLowerer(
      canonicalAtPath, "at", kStdCollectionsPrefix, kStdVectorRootPrefix, kExperimentalVectorPrefix,
      kStdVectorRootPrefix, kInternalSoaStorageFolderPrefix, canonicalAtPath /* vectorHelperPathAtName */,
      kUnusedVectorHelperPathAtUnsafe, noKeyValueMatch(), {}, out));
  // Sanity check: without the exclusion, this exact scopedName WOULD
  // classify (branch 3's receiver-base-disambiguated "Vector/at" shape).
  CHECK(irLowererClassify(canonicalAtPath, "at", noKeyValueMatch(), out));
  CHECK(out == "at");
}

TEST_SUITE_END();
